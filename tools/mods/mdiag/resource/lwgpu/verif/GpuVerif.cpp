/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2016,2018-2019, 2021 by LWPU Corporation.  All rights 
 * reserved. All information contained herein is proprietary and confidential 
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the 
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "GpuVerif.h"

#include <stdio.h>
#include <stdarg.h>

#include "core/include/gpu.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "mdiag/utils/utils.h"
#include "mdiag/utils/XMLlogging.h"
#include "core/utility/errloggr.h"
#include "mdiag/tests/gpu/trace_3d/tracechan.h"
#include "mdiag/utils/lwgpu_classes.h"

#include "VerifUtils.h"
#include "CrcChecker.h"
#include "CrcSelfgildChecker.h"
#include "ReferenceChecker.h"
#include "InterruptChecker.h"
#include "LwlCounters.h"

#include "fermi/gf100/dev_graphics_nobundle.h"

GpuVerif::GpuVerif
(
    LwRm* pLwRm,
    SmcEngine* pSmcEngine,
    LWGpuResource *lwgpu,
    IGpuSurfaceMgr *surfMgr,
    const ArgReader *params,
    UINT32 traceArch,
    ICrcProfile* profile,
    ICrcProfile* goldProfile,
    TraceChannel* channel,
    TraceFileMgr* traceFileMgr,
    bool dmaCheck
)
{
    Initialize(pLwRm, pSmcEngine, lwgpu, surfMgr, params, traceArch,
            profile, goldProfile, channel, traceFileMgr, dmaCheck);
}

// Selfgild
GpuVerif::GpuVerif
(
    SelfgildStates::const_iterator begin,
    SelfgildStates::const_iterator end,
    LwRm* pLwRm, 
    SmcEngine* pSmcEngine,
    LWGpuResource *lwgpu,
    IGpuSurfaceMgr *surfMgr,
    const ArgReader *params,
    UINT32 traceArch,
    ICrcProfile* profile,
    ICrcProfile* goldProfile,
    TraceChannel* channel,
    TraceFileMgr* traceFileMgr,
    bool dmaCheck
)
{
    Initialize(pLwRm, pSmcEngine, lwgpu, surfMgr, params, traceArch, profile,
            goldProfile, channel, traceFileMgr, dmaCheck);
    transform(begin, end, back_inserter(m_Strategies),
              [&params](SelfgildState* state) -> SelfgildStrategy*
              { return state->GetSelfgildStrategy(params); });
}

void GpuVerif::Initialize
(
    LwRm* pLwRm, 
    SmcEngine* pSmcEngine,
    LWGpuResource *lwgpu,
    IGpuSurfaceMgr *surfMgr,
    const ArgReader *params,
    UINT32 traceArch,
    ICrcProfile* profile,
    ICrcProfile* goldProfile,
    TraceChannel* channel,
    TraceFileMgr* traceFileMgr,
    bool dmaCheck
)
{
    m_pLwRm = pLwRm;
    m_pSmcEngine = pSmcEngine;
    m_lwgpu = lwgpu;
    m_surfMgr = surfMgr;
    m_params = params;
    m_class = traceArch;
    m_reportFile = NULL;
    m_traceFileMgr = traceFileMgr;
    m_DmaCheck = dmaCheck;

    m_profile = profile;
    m_goldProfile = goldProfile;
    m_rawImageMode = RAWOFF;

    m_surfZ = NULL;

    m_Channel = channel;

    // This if block is temporary code in support of bug 580638,
    // to detect multiple different values for -append_crc_key.
    UINT32 appendCrcCount = params->ParamPresent("-append_crc_key");
    if (appendCrcCount > 1)
    {
        const vector<string>* appendCrcs = params->ParamStrList(
                "-append_crc_key", NULL);
        auto start = appendCrcs->begin();
        auto cursor = appendCrcs->begin();
        cursor++;
        while (cursor != appendCrcs->end())
        {
            if (*cursor != *start)
            {
                MASSERT(!"-append_crc_key: multiple differing values");
            }
            cursor++;
        }
    }

    m_CrcChain = 0;
    //imgNameZ = "test";

    m_ForceDimInCrcKey = (params->ParamPresent("-force_dim_in_crc_key") > 0);
    m_crcMode = (CrcEnums::CRC_MODE)params->ParamUnsigned("-crcMode", CrcEnums::CRC_CHECK);

    m_DmaUtils = new DmaUtils(this);
    m_DumpUtils = new DumpUtils(this);

    m_ChecksPerformed = 0;
}

GpuVerif::~GpuVerif()
{
    if (m_CrcChain)
    {
        delete m_CrcChain;
        m_CrcChain = 0;
    }

    SurfIt2_t surfIt = m_surfaces.begin();
    SurfIt2_t surfItEnd = m_surfaces.end();
    for(; surfIt != surfItEnd; ++surfIt)
    {
        delete(*surfIt);
    }

    delete m_surfZ;

    AllocSurfaceVec2_t::const_iterator surfIter = m_AllocSurfaces.begin();
    AllocSurfaceVec2_t::const_iterator surfIterEnd = m_AllocSurfaces.end();
    for (; surfIter != surfIterEnd; ++surfIter)
    {
        delete(*surfIter);
    }

    DmaBufVec2_t::const_iterator bufIter = m_DmaBufferInfos.begin();
    DmaBufVec2_t::const_iterator bufIterEnd = m_DmaBufferInfos.end();
    for (; bufIter != bufIterEnd; ++bufIter)
    {
        delete(*bufIter);
    }

    delete m_DmaUtils;
    m_DmaUtils = NULL;

    SelfgildStrategies::const_iterator end = m_Strategies.end();
    for (SelfgildStrategies::const_iterator iter = m_Strategies.begin(); iter != end; ++iter)
    {
        delete *iter;
    }
}

bool GpuVerif::Setup(BufferConfig* config)
{
    VerifUtils::Setup(this); // must go first!
    m_DmaUtils->Setup(config);

    //Check if this name is already taken,
    //if so, add _2, _3, etc at the end of name
    m_outFilename = VerifUtils::ModifySameNames(m_params->ParamStr("-o", "test"));
    m_outFilename = UtilStrTranslate(m_outFilename, DIR_SEPARATOR_CHAR2, DIR_SEPARATOR_CHAR);

    if (m_params->ParamPresent("-RawImageMode"))
    {
        m_rawImageMode = (_RAWMODE)m_params->ParamUnsigned("-RawImageMode");
    }
    else if (m_params->ParamPresent("-RawImagesHW") && 
        (Platform::GetSimulationMode() == Platform::Hardware))
    {
        m_rawImageMode = RAWON;
    }

    for (int i=0; i < NUM_CHECKERS; ++i)
        m_checkers[i].reset(0);

    // Create all the checkers
    if (m_Strategies.size() == 0)
        m_checkers[CT_CRC].reset(new CrcChecker);
    else
        m_checkers[CT_CRC].reset(new CrcSelfgildChecker);

    m_checkers[CT_Reference].reset(new ReferenceChecker);
    m_checkers[CT_Interrupts].reset(new InterruptChecker);
    m_checkers[CT_LwlCounters].reset(new LwlCounters);

    for(UINT32 i = 0; i < NUM_CHECKERS; ++i)
    {
        if (m_checkers[i].get() != 0)
            m_checkers[i]->Setup(this);
    }

    VerifUtils::WriteVerifXml(m_profile);

    return true;
}

void GpuVerif::SetCrcChainManager(const ArgReader* params,
        GpuSubdevice* pSubdev, UINT32 defaultClass)
{
    m_CrcChain = new Trace3DCrcChain(params, pSubdev, defaultClass);
    if(m_CrcChain)
    {
        m_GpuKey = "";
    }
}

bool GpuVerif::DumpActiveSurface(MdiagSurf* surface, const char* fname, UINT32 subdev)
{
    return m_DumpUtils->DumpActiveSurface(surface, fname, subdev);
}

// ---------------------------------------------------------------------------------
//  adds a color surface
// ---------------------------------------------------------------------------------
void GpuVerif::AddSurfC(MdiagSurf* surf, int index, const CrcRect& rect)
{
    MASSERT(surf);
    MASSERT(m_surfMgr->GetValid(surf));
    MASSERT(index >= 0 && index <= MAX_RENDER_TARGETS);

    SurfInfo2* info = new SurfInfo2(surf, this, index);
    info->m_GpuVerif = this;
    info->rect.x = rect.x;
    info->rect.y = rect.y;
    info->rect.width = rect.width;
    info->rect.height = rect.height;
    MASSERT(info->rect.x + info->rect.width <= surf->GetWidth());
    MASSERT(info->rect.y + info->rect.height<= surf->GetHeight());

    m_surfaces.push_back(info);
    m_allSurfaces.push_back(info);
}

// ---------------------------------------------------------------------------------
//  adds zeta target, only one allowed!
// ---------------------------------------------------------------------------------
void GpuVerif::AddSurfZ(MdiagSurf* surf, const CrcRect& rect)
{
    MASSERT(!m_surfZ);
    MASSERT(surf);
    MASSERT(m_surfMgr->GetValid(surf));

    SurfInfo2* info = new SurfInfo2(surf, this);
    info->m_GpuVerif = this;
    info->rect.x = rect.x;
    info->rect.y = rect.y;
    info->rect.width = rect.width;
    info->rect.height = rect.height;
    MASSERT(info->rect.x + info->rect.width <= surf->GetWidth());
    MASSERT(info->rect.y + info->rect.height<= surf->GetHeight());

    m_surfZ = info;
    m_allSurfaces.push_back(m_surfZ);
}

RC GpuVerif::AddAllocSurface(MdiagSurf*surface, const string &crcName,
    CHECK_METHOD check, UINT64 offset, UINT64 size, UINT32 granularity,
    bool crcMatch, const CrcRect& rect, bool rawCrc, UINT32 classNum)
{
    MASSERT(surface != 0);

    // Verify that the offset and size are aligned properly.
    if (granularity)
    {
        if (offset % granularity != 0)
            return RC::BAD_PARAMETER;
        if (size % granularity != 0)
            return RC::BAD_PARAMETER;
    }

    AllocSurfaceInfo2 *info = new AllocSurfaceInfo2(surface, this);

    info->m_GpuVerif = this;

    // AllocSurfaceInfo2::DumpImageToFile needs to call
    // Surface2D::GetGpuDev() so we should save it here.
    info->m_CheckInfo.pDmaBuf = surface;

    info->m_CheckInfo.Filename = crcName;
    info->m_CheckInfo.Offset = offset;
    info->m_CheckInfo.Granularity = granularity;
    info->m_CheckInfo.pTilingParams = 0;
    info->m_CheckInfo.pBlockLinear = 0;
    info->m_CheckInfo.CrcMatch = crcMatch;
    info->m_CheckInfo.Width = 0;
    info->m_CheckInfo.Height = 0;
    info->m_CheckInfo.Check = check;
    info->m_CheckInfo.CrcChainClass = classNum;
    info->m_CheckInfo.SetBufferSize(size);
    info->m_CheckInfo.SetUntiledSize(0);

    info->rect.x = rect.x;
    info->rect.y = rect.y;
    info->rect.width = rect.width;
    info->rect.height = rect.height;
    MASSERT(info->rect.x + info->rect.width <= surface->GetWidth());
    MASSERT(info->rect.y + info->rect.height<= surface->GetHeight());
    info->rawCrc = rawCrc;

    m_AllocSurfaces.push_back(info);
    m_allSurfaces.push_back(info);

    return OK;
}

RC GpuVerif::RemoveAllocSurface(const MdiagSurf* surface)
{
    AllocSurfaceVec2_t::iterator it = m_AllocSurfaces.begin();
    while (it != m_AllocSurfaces.end())
    {
        const MdiagSurf* pSurf = (*it)->GetSurface();

        if (pSurf == surface)
        {
            // remove the copy in allSurfaces
            vector<SurfaceInfo*>::iterator dupIt;
            dupIt = find(m_allSurfaces.begin(), m_allSurfaces.end(), *it);
            MASSERT (dupIt != m_allSurfaces.end());
            m_allSurfaces.erase(dupIt);

            it = m_AllocSurfaces.erase(it);
        }
        else
        {
            it ++;
        }
    }

    return OK;
}

RC GpuVerif::AddDmaBuffer(MdiagSurf *pDmaBuf, CHECK_METHOD Check, const char *Filename,
                          UINT64 Offset, UINT64 Size, UINT32 Granularity,
                          const VP2TilingParams *pTilingParams,
                          BlockLinear* pBl, UINT32 Width, UINT32 Height, bool CrcMatch)
{
    MASSERT(Check != NO_CHECK);

    for (auto buffInfo : m_DmaBufferInfos)
    {
        // Change was added to check for overlapping offsets in shared 
        // MdiagSurfs and then add the module if offsets do not match. 
        // But it led to failures in a frozen Volta FakeGL trace 
        // (stream_ssync_ts_tasks). This has been fixed from Hopper traces. 
        // Hence non-overlapping same-surface buffers are only for >= Hopper.
        if (EngineClasses::IsGpuFamilyClassOrLater(
                TraceSubChannel::GetClassNum(), LWGpuClasses::GPU_CLASS_HOPPER))
        {
            if ((pDmaBuf == buffInfo->DmaBuffers.pDmaBuf) &&
                (Offset == buffInfo->DmaBuffers.Offset))
            {
                return OK;
            }
        }
        else if (pDmaBuf == buffInfo->DmaBuffers.pDmaBuf)
        {
            return OK;
        }
    }

    // Verify that the offset and size are aligned properly
    if (Granularity)
    {
        if (Offset % Granularity != 0)
            return RC::BAD_PARAMETER;

        // Continue the removing of 4B aligned surface size restrict since a bug fixing for
        // bug#200148307, hence won't check if Size is a granularity aligned or rounded one.
        // NOTE: only affected CPU computing CRC, in GPU computing CRC cases,
        // GpuComputeCRC(), etc, functions they would still check granularity aligned
        // since GPU HW may require 4B aligned surface size.
    }

    // Add it to the list
    DmaBufferInfo2* dmaInfo = new DmaBufferInfo2(pDmaBuf, this);
    dmaInfo->DmaBuffers.pDmaBuf       = pDmaBuf;
    dmaInfo->DmaBuffers.Filename      = Filename;
    dmaInfo->DmaBuffers.Offset        = Offset;
    dmaInfo->DmaBuffers.Granularity   = Granularity;
    dmaInfo->DmaBuffers.pTilingParams = pTilingParams;
    dmaInfo->DmaBuffers.pBlockLinear  = pBl;
    dmaInfo->DmaBuffers.CrcMatch      = CrcMatch;
    dmaInfo->DmaBuffers.Width         = Width;
    dmaInfo->DmaBuffers.Height        = Height;
    dmaInfo->DmaBuffers.Check         = Check;

    dmaInfo->DmaBuffers.SetBufferSize(Size);

    if (pBl)
        dmaInfo->DmaBuffers.SetUntiledSize(Width*Height);
    else
        dmaInfo->DmaBuffers.SetUntiledSize(0);

    dmaInfo->dmabuf_dumped = false;
    dmaInfo->dmabuf_name = "";
    dmaInfo->imgNameDmaBuf = "";

    m_DmaBufferInfos.push_back(dmaInfo);
    m_allSurfaces.push_back(dmaInfo);

    return OK;
}

void GpuVerif::OpenReportFile()
{
    MASSERT(m_crcMode == CrcEnums::CRC_REPORT && !m_reportFile);
    m_reportFile = VerifUtils::CreateTestReport(m_outFilename);
}

void GpuVerif::CloseReportFile()
{
    MASSERT(m_reportFile);
    fclose(m_reportFile);
    m_reportFile = NULL;
}

INT32 GpuVerif::WriteToReport(const char * Format, ...)
{
    INT32 CharactersWritten = 0;

    if (m_reportFile)
    {
        va_list Arguments;
        va_start(Arguments, Format);

        CharactersWritten = vfprintf(m_reportFile, Format, Arguments);

        va_end(Arguments);
    }

    return CharactersWritten;
}

void GpuVerif::RegisterChecker(VerifCheckerType type, IVerifChecker* checker)
{
    MASSERT((type < NUM_CHECKERS) && checker);
    m_checkers[type].reset(checker);
}

UINT32 GpuVerif::CallwlateCrc(const string& checkName, UINT32 subdev,
        const SurfaceAssembler::Rectangle *rect)
{
    ICrcVerifier* pCrcVerif = dynamic_cast<ICrcVerifier*>(m_checkers[CT_CRC].get());
    MASSERT(pCrcVerif != 0);
    return pCrcVerif->CallwlateCrc(checkName, subdev, rect);
}

TestEnums::TEST_STATUS GpuVerif::DoCrcCheck
(
    ITestStateReport *report,
    UINT32 gpuSubdevice,
    const SurfaceSet *skipSurfaces
)
{
    PostTestCrcCheckInfo info(report, gpuSubdevice, skipSurfaces);
    return m_checkers[CT_CRC]->Check(&info);
}

// called by functions outside gpuverif to do crc check on specified dynamic surface
TestEnums::TEST_STATUS GpuVerif::DoSurfaceCrcCheck(
    ITestStateReport* report,
    const string& checkName, const string& surfaceSuffix,
    UINT32 gpuSubdevice)
{
    SurfaceCheckInfo info(report, checkName, surfaceSuffix, gpuSubdevice);
    return m_checkers[CT_CRC]->Check(&info);
}

TestEnums::TEST_STATUS GpuVerif::DoIntrCheck(ITestStateReport *report)
{
    ICheckInfo* info = new ICheckInfo(report);
    return m_checkers[CT_Interrupts]->Check(info);
}

TestEnums::TEST_STATUS GpuVerif::CheckIntrPassFail()
{
    InterruptChecker* pChecker = dynamic_cast<InterruptChecker*>(m_checkers[CT_Interrupts].get());
    MASSERT(pChecker != 0);
    return pChecker->CheckIntrPassFail();
}

// ---------------------------------------------------------------------------------
//  check if zeta buffer is valid
// ---------------------------------------------------------------------------------
bool GpuVerif::ZSurfOK() const
{
    return m_surfZ && m_surfZ->IsValid();
}

RC GpuVerif::DumpSurfOrBuf(MdiagSurf *surfOrBuf, string &fname, UINT32 gpuSubdevice)
{
    return m_DumpUtils->DumpSurfOrBuf(surfOrBuf, fname, gpuSubdevice);
}

RC GpuVerif::FlushGpuCache(UINT32 gpuSubdevice)
{
    RC rc;
    LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS param;
    memset(&param, 0, sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));
    param.flags = FLD_SET_DRF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FB_FLUSH, _YES, param.flags);
    if (!m_params->ParamPresent("-no_l2_flush"))
    {
        param.flags = FLD_SET_DRF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FLUSH_MODE, _FULL_CACHE, param.flags);
        param.flags = FLD_SET_DRF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _YES, param.flags);
        param.flags = FLD_SET_DRF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _YES, param.flags);

        DebugPrintf("Flushing L2...\n");
    }

    DebugPrintf("Sending UFLUSH...\n");

    rc = m_pLwRm->Control(
            m_pLwRm->GetSubdeviceHandle(m_lwgpu->GetGpuSubdevice(gpuSubdevice)),
            LW2080_CTRL_CMD_FB_FLUSH_GPU_CACHE,
            &param,
            sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));

    if (rc != OK)
    {
        ErrPrintf("Error flushing l2 cache, message: %s\n", rc.Message());
        return rc;
    }

    param.flags = FLD_SET_DRF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _SYSTEM_MEMORY, param.flags);
    rc = m_pLwRm->Control(
            m_pLwRm->GetSubdeviceHandle(m_lwgpu->GetGpuSubdevice(gpuSubdevice)),
            LW2080_CTRL_CMD_FB_FLUSH_GPU_CACHE,
            &param,
            sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));

    if (rc != OK)
    {
        ErrPrintf("Error flushing l2 cache, message: %s\n", rc.Message());
    }
    return rc;
}

// Helper for ErrorLogger so that it can support legacy HW engine error
// handling in the same way as before
//------------------------------------------------------------------------------
struct IntrEntry
{
    UINT32 gr_intr;
    UINT32 class_error;
    UINT32 addr;
    UINT32 data_lo;
};

bool IntrErrorFilter(UINT64 intr, const char *lwrErr, const char *expectedErr)
{
    IntrEntry lwr, expected;

    // If either the current or expected error doesnt match the default interrupt format
    // assume it is a real error
    if ((sscanf(lwrErr, "gr_intr=0x%08x class_error=0x%08x trapped addr=0x%08x data_lo=0x%08x\n",
                &lwr.gr_intr, &lwr.class_error, &lwr.addr, &lwr.data_lo) != 4) ||
        (sscanf(expectedErr, "gr_intr=0x%08x class_error=0x%08x trapped addr=0x%08x data_lo=0x%08x\n",
                &expected.gr_intr, &expected.class_error, &expected.addr, &expected.data_lo) != 4))
    {
        return true;
    }

    // would be nice to pull these in from a ref header!
    #define TRAPPED_ADDR_MTHD_MSK 0xffff

    // We always expect to match in the STATUS valid bit
    bool failStatus =
        DRF_VAL(_PGRAPH, _TRAPPED_ADDR, _STATUS, lwr.addr) !=
        DRF_VAL(_PGRAPH, _TRAPPED_ADDR, _STATUS, expected.addr);

    bool failMethod = false;
    bool failDataLo = false;
    if (DRF_VAL(_PGRAPH, _TRAPPED_ADDR, _STATUS, lwr.addr) ==
        LW_PGRAPH_TRAPPED_ADDR_STATUS_VALID)
    {
        // TRAPPED_ADDR is valid -- compare method and data_lo
        failMethod = (lwr.addr & TRAPPED_ADDR_MTHD_MSK) != (expected.addr & TRAPPED_ADDR_MTHD_MSK);
        failDataLo = lwr.data_lo != expected.data_lo;
    }

    // Note that we never check DATA_HI.  It's not deterministic, and it's never
    // relevant to the error anyway.  If the method represented by DATA_HI were
    // to cause an error, then it'd get moved down into DATA_LO first before the
    // error was reported.

    // if this is a SetContextDma method with a handle, data will not match
    if ((lwr.addr & TRAPPED_ADDR_MTHD_MSK) < 0x0200) // method where host does hash lookup
        failDataLo = false;

    if ((lwr.gr_intr != expected.gr_intr) ||
        (lwr.class_error != expected.class_error) ||
        failStatus || failMethod || failDataLo)
    {
        InfoPrintf("TraceFile::IntrErrorFilter:\n"
            "expected:\n%s"
            "actual:\n"
            "gr_intr=0x%08x class_error=0x%08x trapped addr=0x%08x data_lo=0x%08x\n"
            "Mismatch on interrupt %lld\n",
            expectedErr,
            lwr.gr_intr, lwr.class_error, lwr.addr, lwr.data_lo, intr);
        return true; // there was a real error
    }

    DebugPrintf("TraceFile::IntrErrorFilter: Previous miscompare deemed acceptable\n");
    return false;
}

