/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   tracechan.cpp
 * @brief  TraceChannel: per-channel resources used by Trace3DTest.
 */
#include "mdiag/tests/gpu/lwgpu_ch_helper.h"
#include "tracechan.h"
#include "tracerel.h"
#include "mdiag/tests/gpu/lwgpu_subchannel.h"
#include "class/cl007d.h" // LW04_SOFTWARE_TEST
#include "class/cl5080.h" // LW50_DEFERRED_API_CLASS
#include "class/cl9297.h" // FERMI_C
#include "class/cl86b6.h" // LW86B6_VIDEO_COMPOSITOR
#include "class/cl902d.h" // FERMI_TWOD_A
#include "class/cl90b3.h" // GF100_MSPPP
#include "class/cl90b8.h" // GF106_DMA_DECOMPRESS
#include "class/cl95a1.h" // LW95A1_TSEC
#include "class/cl95b1.h" // LW95B1_VIDEO_MSVLD
#include "class/cl95b2.h" // LW95B2_VIDEO_MSPDEC
#include "class/cla297.h" // KEPLER_C
#include "class/cla140.h" // KEPLER_INLINE_TO_MEMORY_B
#include "class/cla06c.h" // KEPLER_CHANNEL_GROUP_A
#include "class/clb097.h" // MAXWELL_A
#include "class/clb197.h" // MAXWELL_B
#include "class/clc397.h" // VOLTA_A
#include "mdiag/utils/lwgpu_classes.h"
#include "core/include/massert.h"
#include "core/include/gpu.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/channel.h"
#include "gpu/utility/chanwmgr.h"
#include "gpu/include/notifier.h"
#include "core/include/lwrm.h"
#include "lwos.h"
#include "mdiag/utils/buffinfo.h"
#include "tracetsg.h"
#include "mdiag/tests/gpu/lwgpu_tsg.h"
#include "trace_3d.h"
#include "mdiag/utils/perf_mon.h"
#include "massage.h"
#include "teegroups.h"
#include "mdiag/smc/smcengine.h"
#include "mdiag/utl/utl.h"

#define MSGID(Group) T3D_MSGID(Group)

// Round up the number to the upper minimum power of two
static UINT32 RoundUpToPowerOfTwo(UINT32 val)
{
    if ((val & (val-1)) != 0)
    {
        MASSERT((val & 0x8000000) == 0); // no overflow
        val <<= 1;
    }
    while ((val & (val-1)) != 0) val &= val-1;

    return val;
}

//------------------------------------------------------------------------------
TraceChannel::TraceChannel(string name, BuffInfo* inf, Trace3DTest *test)
: m_Name(name),
  m_pChHelper(0),
  m_pCh(0),
  m_TimeSlice(0),
  m_pGpuVerif(0),
  m_BuffInfo(inf),
  m_IgnoreVAB(false),
  m_bUsePascalPagePool(false),
  m_bSetupTrapHanderVa(false),
  m_pTraceTsg(0),
  m_IsGpuManagedChannel(false),
  m_Preemptive(false),
  m_VprMode(false),
  m_GpFifoEntries(0),
  m_MaxCountedGpFifoEntries(0),
  m_PushbufferSize(0),
  m_EngineId(0),
  m_Test(test),
  m_IsDynamic(false),
  m_IsValidInParsing(true),
  m_SCGType(LWGpuChannel::GRAPHICS_COMPUTE0),
  m_Pbdma(0),
  m_hVASpace(0)
{
    MASSERT(m_Test);
    params = m_Test->params;
    m_Trace = m_Test->GetTrace();
}

//------------------------------------------------------------------------------
TraceChannel::~TraceChannel()
{
    Free();
}

//------------------------------------------------------------------------------
void TraceChannel::AddModule(TraceModule * module)
{
    for (TraceModules::iterator imod = m_PbufModules.begin();
            imod != m_PbufModules.end();
                    ++imod)
    {
        if ((*imod) == module)
            return;
    }
    m_PbufModules.push_back(module);
}

//------------------------------------------------------------------------------
void TraceChannel::SetTimeSlice(UINT32 val)
{
    m_TimeSlice = val;
}

//------------------------------------------------------------------------------
RC TraceChannel::AllocChannel
(
    const ArgReader * params
)
{
    RC rc;
    Utility::CleanupOnReturn<TraceChannel> cleanup(this, &TraceChannel::Free);

    InfoPrintf("TraceChannel::AllocChannel \"%s\" reserve GPU resource, alloc channel.\n",
            m_Name.c_str());

    if (GetGpuResource() == 0)
    {
        ErrPrintf("Can't aquire GPU\n");
        return RC::LWRM_INSUFFICIENT_RESOURCES;
    }

    if (0 == GetChHelper())
    {
        ErrPrintf("create GpuChannelHelper failed\n");
        return RC::SOFTWARE_ERROR;
    }

    if (!m_pChHelper->ParseChannelArgs(params))
    {
        ErrPrintf("ParseChannelArgs failed\n");
            return RC::SOFTWARE_ERROR;
    }

    // Allocate all the subchannels
    for( size_t i=0; i<m_SubChList.size(); ++i )
    {
        CHECK_RC( m_SubChList[i]->AllocSubChannel(GetGpuResource()) );
    }

    CalcEngineId();

    m_pChHelper->acquire_channel();
    m_pCh = m_pChHelper->channel();
    if ( !m_pCh ) {
        ErrPrintf("Channel acquire failed\n");
        return RC::SOFTWARE_ERROR;
    }

    m_pCh->SetVASpace(m_pVaSpace);

    SanityCheckSharedChannel();

    if (m_pTraceTsg)
    {
        // get lwgpu_tsg
        LWGpuTsg* pGpuTsg = m_pTraceTsg->GetLWGpuTsg();
        if (0 == pGpuTsg)
        {
            CHECK_RC(m_pTraceTsg->AllocLWGpuTsg(KEPLER_CHANNEL_GROUP_A,
                                               &pGpuTsg,
                                               GetGpuResource(),
                                               m_EngineId));
            m_Test->AddTraceTsg(m_pTraceTsg);

            if (Utl::HasInstance())
            {
                Utl::Instance()->AddTestTsg(m_Test, m_pTraceTsg->GetLWGpuTsg());
            }
        }

        if (pGpuTsg) m_pCh->SetLWGpuTsg(pGpuTsg);

        m_pCh->SetSubCtx(m_pSubCtx);
    }

    // Must do this before calling alloc_channel.
    if (m_TimeSlice)
        m_pCh->SetTimeSlice(m_TimeSlice);

    if (m_PushbufferSize)
    {
        m_pCh->SetPushbufferSize(m_PushbufferSize);
    }

    m_pCh->ParseChannelArgs(params);
    if (params->ParamPresent("-preemptive_all_channels") > 0)
    {
        // Set all compute channels to be preemptive
        if (GetComputeSubChannel() != 0)
        {
            SetPreemptive(true);
            m_pCh->SetPreemptive(true);
        }
    }
    else
    {
        // Kepler allows some channel contains compute subch to be preemptive
        // See bug 642249
        UINT32 count = params->ParamPresent("-preemptive_channel");
        for (UINT32 i = 0; i < count; ++i)
        {
            if (params->ParamNStr("-preemptive_channel", i, 0) == GetName())
            {
                SetPreemptive(true);
                m_pCh->SetPreemptive(true);
                break;
            }
        }
    }

    UINT32 count = params->ParamPresent("-set_ce_prefetch_channel");
    for (UINT32 i = 0; i < count; ++i)
    {
        if (params->ParamNStr("-set_ce_prefetch_channel", i, 0) == GetName())
        {
            if (GetCopySubChannel() == 0)
            {
                ErrPrintf("-set_ce_prefetch_channel can be only used for GR or CE channel\n");
                return RC::BAD_PARAMETER;
            }

            m_pCh->SetCEPrefetch(true);
            break;
        }
    }

    if (GetPreemptive() && m_pCh)
    {
        m_pCh->SetPreemptive(true);
    }
    if (IsGpuManagedChannel() && m_pCh)
    {
        m_pCh->SetIsGpuManaged(true);
    }
    if (params->ParamPresent("-vpr_channel") > 0)
    {
        SetVprMode(true);
        m_pCh->SetVprMode(true);
    }

    CHECK_RC(m_pCh->SetSCGType(m_SCGType));
    m_pCh->SetPbdma(m_Pbdma);

    if (m_Test->GetNoAutoFlush() || params->ParamPresent("-conlwrrent") > 0)
    {
        m_pCh->SetUpdateType(LWGpuChannel::MANUAL);
    }

    if (m_GpFifoEntries)
    {
        m_pCh->SetGpFifoEntries(m_GpFifoEntries);
    }
    else if ( (LWGpuChannel::MANUAL == m_pCh->GetUpdateType() ||        // autoflush is disabled
               m_Test->GetTrace()->GetSyncPmTriggerPairNum() > 0)       // or subctx perf tests
            && m_MaxCountedGpFifoEntries > m_pCh->GetGpFifoEntries())   // larger than default value
    {
        m_pCh->SetGpFifoEntries(RoundUpToPowerOfTwo(m_MaxCountedGpFifoEntries));
    }

    m_pCh->SetName(m_Name);

    if (!m_pChHelper->alloc_channel(m_EngineId, TraceSubChannel::GetClassNum()))
    {
        ErrPrintf("Channel Alloc failed\n");
        return RC::SOFTWARE_ERROR;
    }

   // Default Wait-for-idle mode is interrupt except on A-model.
    UINT32 wfimethod = m_Test->GetWfiMethod();
    if (IsGpuManagedChannel())
    {
        // MODS can't insert method for GPU managed channel.
        InfoPrintf("Force WFI_POLL for GPU managed channel\n");
        ForceWfiMethod(WFI_POLL);
    }
    else
    {
        if (wfimethod == WFI_POLL &&
            ChannelWrapperMgr::Instance()->UsesRunlistWrapper())
        {
            ErrPrintf("Cannot use -wfi_poll with -runlist (RM restriction)\n");
            return RC::BAD_PARAMETER;
        }
        ForceWfiMethod( (WfiMethod)wfimethod );
    }

    if (!m_pChHelper)
    {
        // Trace3DTest was supposed to have called AllocChannel first!
        MASSERT(0);
        return RC::SOFTWARE_ERROR;
    }

    if (GetGrSubChannel() != 0)
    {
        m_VAB.SetLocation(Memory::Fb);
        CHECK_RC(ParseDmaBufferArgs(m_VAB, params, "vab", &m_VABPteKindName, 0));
    }

    //
    // Bug 907195
    // Amodel need the channel name to channel ID map
    if (Platform::GetSimulationMode() == Platform::Amodel)
    {
        MASSERT(m_pCh);

        // get the data
        const string channelName = GetName();
        const UINT32 channelId = m_pCh->ChannelNum();

        // prepare the buffer
        UINT32 size = sizeof(channelId) + channelName.size();
        vector<UINT08> escwriteBuffer(size + 1);
        UINT08* buf = &escwriteBuffer[0];

        *(UINT32*)buf = channelId;
        buf += sizeof(UINT32);
        memcpy(buf, channelName.c_str(), channelName.size());
        buf[channelName.size()] = '\0'; // end with NULL for safe

        // key = "CHANNEL_NAME_ID_MAP"
        const char* key = "CHANNEL_NAME_ID_MAP";
        GpuDevice* gpudev = m_Test->GetBoundGpuDevice();
        for (UINT32 subdev = 0; subdev < gpudev->GetNumSubdevices(); subdev++)
        {
            // it doesn't matter to ignore the return value of escapewrite
            GpuSubdevice* gpuSubdev = gpudev->GetSubdevice(subdev);
            gpuSubdev->EscapeWriteBuffer(key, 0, size, &escwriteBuffer[0]);
        }
    }

    cleanup.Cancel();
    return OK;
}

//------------------------------------------------------------------------------
RC TraceChannel::AllocObjects()
{
    RC rc = OK;

    for( size_t i=0; i<m_SubChList.size(); ++i )
    {
        if (m_SubChList[i]->IsDynamic())
        {
            continue; // skip static object creation
        }

        CHECK_RC( m_SubChList[i]->AllocObject(m_pChHelper) );
    }

    if (m_SubChList.size() == 1)
    {
        // the subch num specified in test.hdr has no meaning for single subch
        m_SubChList[0]->SetTraceSubChNum(TraceSubChannel::UNSPECIFIED_TRACE_SUBCHNUM);
    }

    return rc;
}

//------------------------------------------------------------------------------
void TraceChannel::ForceWfiMethod(WfiMethod m)
{
    for(size_t i=0; i<m_SubChList.size(); ++i)
    {
        m_SubChList[i]->ForceWfiMethod(m);
    }
}

//------------------------------------------------------------------------------
RC TraceChannel::AllocNotifiers(const ArgReader * params)
{
    RC rc = OK;
    if (!UseHostSemaphore())
    {
        for(size_t i=0; i<m_SubChList.size(); ++i)
        {
            CHECK_RC(m_SubChList[i]->AllocNotifier(params, m_Name));
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
void TraceChannel::Free()
{
    DebugPrintf(MSGID(ChannelLog), "TraceChannel::Free \"%s\": free channel, notifier, release gpu.\n",
            m_Name.c_str());

    // Bug 642344: now that GpuVerif's DmaReaders use LWGpuChannel,
    // allow them to clean up before calling release_channel()
    if (m_pGpuVerif)
        delete m_pGpuVerif;
    m_pGpuVerif = 0;

    for(UINT32 i=0; i<m_SubChList.size(); ++i)
    {
        delete m_SubChList[i];
    }
    m_SubChList.clear();

    if (m_pChHelper) {
        m_pChHelper->release_channel();
        delete m_pChHelper;
        m_pChHelper = NULL;
    }
    m_PbufModules.clear();

    m_pCh = 0;

    if (m_pTraceTsg)
    {
        m_pTraceTsg->RemoveTraceChannel(this);
        m_pTraceTsg = 0;
    }

    m_VAB.Free();
    m_AmodelCirlwlarBuffer.Free();

    m_pSubCtx.reset();
    m_pVaSpace.reset();
}

//------------------------------------------------------------------------------
void TraceChannel::ClearNotifiers()
{
    for( size_t i=0; i<m_SubChList.size(); ++i )
    {
        m_SubChList[i]->ClearNotifier();
    }
}

//------------------------------------------------------------------------------
RC TraceChannel::SendNotifiers()
{
    RC rc = OK;

    if (!UseHostSemaphore())
    {
        for( size_t i=0; i<m_SubChList.size(); ++i )
        {
            CHECK_RC( m_SubChList[i]->SendNotifier() );
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
bool TraceChannel::PollNotifiersDone()
{
    bool NotifiersDone = true;

    for( size_t i=0; i<m_SubChList.size(); ++i )
    {
        NotifiersDone = NotifiersDone && m_SubChList[i]->PollNotifierDone();
    }
    return NotifiersDone;
}

//------------------------------------------------------------------------------
UINT16 TraceChannel::PollNotifierValue()
{
    if ( HasMultipleSubChs() )
    {
        MASSERT(!"Notifier polling not supported for multiple subchannels");
    }
    if ( IsHostOnlyChannel() )
    {
        MASSERT(!"Notifier polling not supported for host only channels");
    }
    return m_SubChList[0]->PollNotifierValue();
}

//------------------------------------------------------------------------------
void TraceChannel::BeginChannelSwitch()
{
    if (m_pCh)
        m_pCh->BeginChannelSwitch();
}

//------------------------------------------------------------------------------
void TraceChannel::EndChannelSwitch()
{
    if (m_pCh)
        m_pCh->EndChannelSwitch();
}

//------------------------------------------------------------------------------
string TraceChannel::GetName() const
{
    return m_Name;
}

//------------------------------------------------------------------------------
WfiMethod TraceChannel::GetWfiMethod() const
{
    if ( IsHostOnlyChannel() )
    {
        return WFI_HOST;
    }
    return m_SubChList[0]->GetWfiMethod();
}

//------------------------------------------------------------------------------
LWGpuChannel * TraceChannel::GetCh() const
{
    return m_pCh;
}

//------------------------------------------------------------------------------
GpuChannelHelper * TraceChannel::GetChHelper()
{
    if (0 == m_pChHelper)
    {
        m_pChHelper = mk_GpuChannelHelper(m_Test->GetTestName(),
                                          GetGpuResource(),
                                          m_Test,
                                          GetLwRmPtr(),
                                          GetSmcEngine());
        if (!m_pChHelper)
            ErrPrintf("create GpuChannelHelper failed\n");
    }
    return m_pChHelper;
}

//------------------------------------------------------------------------------
LWGpuResource * TraceChannel::GetGpuResource()
{
    return m_Test->GetGpuResource();
}

//------------------------------------------------------------------------------
TraceModules::iterator TraceChannel::ModBegin()
{
    return m_PbufModules.begin();
}

//------------------------------------------------------------------------------
TraceModules::iterator TraceChannel::ModEnd()
{
    return m_PbufModules.end();
}

//------------------------------------------------------------------------------
TestEnums::TEST_STATUS TraceChannel::VerifySurfaces
(
    ITestStateReport *report,
    UINT32 subdev,
    const SurfaceSet *skipSurfaces)
{
    if (IsHostOnlyChannel())
    {
        ErrPrintf("Can not run CRC check on host only channel %s\n", m_Name.c_str());
        return TestEnums::TEST_NOT_STARTED;
    }

    return  m_pGpuVerif->DoCrcCheck(report, subdev, skipSurfaces);
}

//------------------------------------------------------------------------------
RC TraceChannel::AddDmaBuffer(MdiagSurf *pDmaBuf, CHECK_METHOD Check, const char *Filename,
                              UINT32 Offset, UINT32 Size, UINT32 Granularity,
                              const VP2TilingParams *pTilingParams,
                              BlockLinear* pBlockLinear, UINT32 Width, UINT32 Height, bool CrcMatch)
{
    if (IsHostOnlyChannel())
    {
        ErrPrintf("Can not add DMA buffer to host only channel %s\n", m_Name.c_str());
        return RC::SOFTWARE_ERROR;
    }

    return m_pGpuVerif->AddDmaBuffer(pDmaBuf, Check, Filename, Offset, Size, Granularity,
                                     pTilingParams, pBlockLinear, Width, Height, CrcMatch);
}

//------------------------------------------------------------------------------
RC TraceChannel::AddAllocSurface(MdiagSurf *surface, const string &name,
    CHECK_METHOD check, UINT64 offset, UINT64 size, UINT32 granularity,
    bool crcMatch, const CrcRect& rect, bool rawCrc, UINT32 classNum)
{
    if (IsHostOnlyChannel())
    {
        ErrPrintf("Can not add alloc surface to host only channel %s\n", m_Name.c_str());
        return RC::SOFTWARE_ERROR;
    }

    return m_pGpuVerif->AddAllocSurface(surface, name, check, offset,
        size, granularity, crcMatch, rect, rawCrc, classNum);
}

//------------------------------------------------------------------------------
RC TraceChannel::SetupCRCChecker
(
    IGpuSurfaceMgr* sm,
    const ArgReader * params,
    ICrcProfile* profile,
    ICrcProfile* goldProfile,
    GpuSubdevice *pSubdev
)
{
    if (IsHostOnlyChannel())
    {
        ErrPrintf("Can not setup checker for host only channel %s\n", m_Name.c_str());
        return RC::SOFTWARE_ERROR;
    }

    // for channel with more than one subchannels, graphics class number is used for
    // CRC chaining. If no such subchannel exists, just use the first class number
    // declared in hdr file
    TraceSubChannel* psub = GetGrSubChannel();
    if ( !psub )
    {
        psub = m_SubChList[0];
    }
    MASSERT( psub );
    m_pGpuVerif = new GpuVerif(GetLwRmPtr(),
        GetSmcEngine(), GetGpuResource(),
        sm, params, psub->GetClass(),
        profile, goldProfile, this, GetTraceFileMgr(), m_Test->NeedDmaCheck());
    if (!m_pGpuVerif)
    {
        ErrPrintf("Can not setup checker for channel %s\n", m_Name.c_str());
        return RC::SOFTWARE_ERROR;
    }

    if (!m_pGpuVerif->Setup(sm->GetBufferConfig()))
        return RC::SOFTWARE_ERROR;

    m_pGpuVerif->SetCrcChainManager(params, pSubdev, psub->GetClass());
    return  OK;
}

//------------------------------------------------------------------------------
RC TraceChannel::SetupSelfgildChecker
(
    SelfgildStates::const_iterator begin,
    SelfgildStates::const_iterator end,
    IGpuSurfaceMgr* sm,
    const ArgReader * params,
    ICrcProfile* profile,
    ICrcProfile* goldProfile,
    GpuSubdevice *pSubdev
)
{
    if (IsHostOnlyChannel())
    {
        ErrPrintf("Can not setup checker for host only channel %s\n", m_Name.c_str());
        return RC::SOFTWARE_ERROR;
    }

    TraceSubChannel* psub = GetGrSubChannel();
    if ( !psub )
    {
        psub = m_SubChList[0];
    }
    MASSERT( psub );
    m_pGpuVerif = new GpuVerif(
            begin, end, GetLwRmPtr(), GetSmcEngine(), GetGpuResource(), sm, params,
            psub->GetClass(), profile, goldProfile, this, GetTraceFileMgr(), m_Test->NeedDmaCheck());
    if (!m_pGpuVerif)
    {
        ErrPrintf("Can not setup checker for channel %s\n", m_Name.c_str());
        return RC::SOFTWARE_ERROR;
    }

    if (!m_pGpuVerif->Setup(sm->GetBufferConfig()))
        return RC::SOFTWARE_ERROR;

    m_pGpuVerif->SetCrcChainManager(params, pSubdev, psub->GetClass());
    return OK;
}

const CrcChain* TraceChannel::GetCrcChain() const
{
    if ( m_pGpuVerif )
    {
        return m_pGpuVerif->GetCrcChain();
    }
    return NULL;
}

bool TraceChannel::HasMultipleSubChs() const
{
    return( m_SubChList.size() > 1);
}

bool TraceChannel::SubChSanityCheck(const char* filename) const
{
    size_t subch_num = m_SubChList.size();
    for( size_t i=0; i<subch_num; ++i )
    {
        if ( !m_SubChList[i]->GetClass() )
        {
            ErrPrintf("%s: you must declare a CLASS for subchannel %s!\n",
                filename, m_SubChList[i]->GetName().c_str());
            return false;
        }
    }

    return true;
}

MethodMassager TraceChannel::GetMassager(UINT32 subch, UINT32 method_offset)
{
    switch( m_SubChList.size() )
    {
        case 0:
            MASSERT(!"Cannot massage method as no subchannel defined!");
            return 0;
        case 1:
            return m_SubChList[0]->GetMassager();
        default:
        {
            TraceSubChannel *psubch = GetSubChannel( subch );
            if (psubch == 0)
            {
                if (method_offset <= 4)
                {
                    // Some special methods do not have subchannel field
                    // We do not allow to massage them for now
                    return 0;
                }
                MASSERT( !"Cannot get massager for subchannel 0!\n");
            }
            return psubch->GetMassager();
        }
    }
}

TraceSubChannel* TraceChannel::GetGrSubChannel()
{
    for(size_t i=0; i<m_SubChList.size(); ++i)
    {
        if ( m_SubChList[i]->GrSubChannel() )
        {
            return m_SubChList[i];
        }
    }
    return 0;
}

TraceSubChannel* TraceChannel::GetComputeSubChannel()
{
    for (size_t i=0; i<m_SubChList.size(); ++i)
    {
        if (m_SubChList[i]->ComputeSubChannel())
        {
            return m_SubChList[i];
        }
    }
    return 0;
}

TraceSubChannel* TraceChannel::GetCopySubChannel()
{
    for (size_t i=0; i<m_SubChList.size(); ++i)
    {
        if (m_SubChList[i]->CopySubChannel())
        {
            return m_SubChList[i];
        }
    }
    return 0;
}

TraceSubChannel* TraceChannel::GetSubChannel(const string& name)
{
    if ( name.empty() && !m_SubChList.empty() )
    {
        return m_SubChList[0];
    }

    for(size_t i=0; i<m_SubChList.size(); ++i)
    {
        if ( m_SubChList[i]->GetName() == name )
        {
            return m_SubChList[i];
        }
    }
    return 0;
}

// errorOnNonExist is an optional parameter that defaults to true
//
TraceSubChannel* TraceChannel::GetSubChannel( UINT32 i, bool errorOnNonExist )
{
    size_t subChannelSize = m_SubChList.size();
    for(size_t idx = 0; idx < subChannelSize; ++ idx)
    {
       if (m_SubChList[idx]->GetTraceSubChNum() == i)
       {
           return m_SubChList[idx];
       }
    }

    if (errorOnNonExist)
    {
        ErrPrintf("Subchannel %d is not defined in test.hdr\n", i);
    }

    return 0;
}

// returns the hwclass for a channel's in-trace-data subchannel
// number (not replay time subchannel number)
//
UINT32 TraceChannel::GetTraceSubChannelHwClass( UINT32 traceSubch )
{
    size_t subChannelSize = m_SubChList.size();
    for(size_t idx = 0; idx < subChannelSize; ++ idx)
    {
        TraceSubChannel* traceSubChannel = m_SubChList[idx];
        if (traceSubChannel->GetTraceSubChNum() == traceSubch)
        {
            return traceSubChannel->GetClass();
        }
    }

    return 0;
}

void TraceChannel::GetRelocSurfaces(SurfaceSet *pSurfaces) const
{
    MASSERT(pSurfaces != NULL);
    set<TraceModule*> traceModulesToProcess;
    set<TraceModule*> traceModulesDone;

    traceModulesToProcess.insert(m_PbufModules.begin(), m_PbufModules.end());
    while (!traceModulesToProcess.empty())
    {
        TraceModule *pTraceModule = *traceModulesToProcess.begin();
        traceModulesToProcess.erase(pTraceModule);
        if (!pTraceModule || traceModulesDone.count(pTraceModule))
            continue;
        traceModulesDone.insert(pTraceModule);

        for (TraceModule::RelocIter ppReloc = pTraceModule->RelocBegin();
             ppReloc != pTraceModule->RelocEnd(); ++ppReloc)
        {
            (*ppReloc)->GetSurfaces(pTraceModule, pSurfaces);
            traceModulesToProcess.insert((*ppReloc)->GetTarget());
        }

        // Implicit relocs added during setup
        IGpuSurfaceMgr *pSurfMgr = pTraceModule->GetSurfaceMgr();
        for (UINT32 ii = 0; ii < MAX_RENDER_TARGETS; ++ii)
        {
            pSurfaces->insert(pSurfMgr->GetSurface(ColorSurfaceTypes[ii], 0));
        }
        pSurfaces->insert(pSurfMgr->GetSurface(SURFACE_TYPE_Z, 0));
    }
}

RC TraceChannel::SetTraceTsg(TraceTsg* pTraceTsg)
{
    m_pTraceTsg = pTraceTsg;
    return OK;
}

void TraceChannel::SetGpFifoEntries(UINT32 val)
{
    m_GpFifoEntries = val;
}

void TraceChannel::SetPushbufferSize(UINT64 val)
{
    m_PushbufferSize = val;
}

RC TraceChannel::SetGpuManagedChannel(bool isGpuManagedCh)
{
    m_IsGpuManagedChannel = isGpuManagedCh;
    return OK;
}

RC TraceChannel::SetMethodCount()
{
    RC rc;

    if (m_PbufModules.begin() == m_PbufModules.end())
    {
        InfoPrintf("%s need to do nothing for trace without pb.\n", __FUNCTION__);
        return OK;
    }

    if ( !m_pCh )
    {
        ErrPrintf("%s: SetMethodCount failed\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    rc = SetFermiMethodCount(m_pCh);
    if (rc != OK)
    {
        ErrPrintf("%s: SetFermiMethodCount() failed on channel(0x%x)!\n",
            __FUNCTION__,
            m_pCh->GetChannelClass());
        rc.Clear();
        rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}

RC TraceChannel::SetupSubChannelClass(TraceSubChannel* pTraceSubch)
{
    RC rc;

    LWGpuSubChannel* psubch = pTraceSubch->GetSubCh();
    MASSERT(psubch);

    // LW04_SOFTWARE_TEST does not support set object,
    // but it should be scheduled which is done in set_object()
    if (pTraceSubch->GetClass() == LW04_SOFTWARE_TEST)
    {
        CHECK_RC(psubch->channel()->ScheduleChannel(true));
    }
    else
    {
        psubch->set_object();
    }

    //No support for VAB after Maxwell
    if (pTraceSubch->GetClass() >= MAXWELL_A)
    {
        m_IgnoreVAB = true;
    }
    else
    {
        m_IgnoreVAB = false;
    }

    UINT32 subchClass = pTraceSubch->GetClass();

    if (pTraceSubch->CopySubChannel() ||
            EngineClasses::IsClassType("Lwenc", subchClass) ||
            EngineClasses::IsClassType("Lwjpg", subchClass) ||
            EngineClasses::IsClassType("Ofa", subchClass) ||
            subchClass == LWGpuClasses::GPU_LW_NULL_CLASS || 
            subchClass == LW50_DEFERRED_API_CLASS || 
            subchClass == FERMI_TWOD_A ||
            subchClass == KEPLER_INLINE_TO_MEMORY_B ||
            subchClass == LW04_SOFTWARE_TEST ||
            subchClass == GF106_DMA_DECOMPRESS)
    {
        rc = OK;
    }
    else if (EngineClasses::IsClassType("Lwdec", subchClass) || 
                subchClass == LW86B6_VIDEO_COMPOSITOR ||
                subchClass == GF100_MSPPP || 
                subchClass == LW95A1_TSEC ||
                subchClass == LW95B1_VIDEO_MSVLD ||
                subchClass == LW95B2_VIDEO_MSPDEC)
    {
        CHECK_RC(SetupMsdec());
    }
    else if (pTraceSubch->ComputeSubChannel())
    {
        if (EngineClasses::IsGpuFamilyClassOrLater(
            subchClass, LWGpuClasses::GPU_CLASS_VOLTA))
        {
            CHECK_RC(SetupVoltaCompute(psubch));
        }
        else if (EngineClasses::IsGpuFamilyClassOrLater(
            subchClass, LWGpuClasses::GPU_CLASS_KEPLER))
        {
            CHECK_RC(SetupKeplerCompute(psubch));
        }
        else
        {
            CHECK_RC(SetupFermiCompute(psubch));
        }
    }
    else if (pTraceSubch->GrSubChannel())
    {
        if (EngineClasses::IsGpuFamilyClassOrLater(
            subchClass, LWGpuClasses::GPU_CLASS_HOPPER))
        {
            CHECK_RC(SetupHopperA(psubch));
        }
        else if (EngineClasses::IsGpuFamilyClassOrLater(
            subchClass, LWGpuClasses::GPU_CLASS_AMPERE))
        {
            CHECK_RC(SetupAmpereA(psubch));
        }
        else if (EngineClasses::IsGpuFamilyClassOrLater(
            subchClass, LWGpuClasses::GPU_CLASS_VOLTA))
        {
            if (subchClass == VOLTA_A)
            {
                CHECK_RC(SetupVoltaA(psubch));
            }
            else
            {
                CHECK_RC(SetupTuringA(psubch));
            }
        }
        else if (EngineClasses::IsGpuFamilyClassOrLater(
            subchClass, LWGpuClasses::GPU_CLASS_PASCAL_A))
        {
            CHECK_RC(SetupPascalA(psubch));
        }
        else if (EngineClasses::IsGpuFamilyClassOrLater(
            subchClass, LWGpuClasses::GPU_CLASS_MAXWELL))
        {
            if (subchClass == MAXWELL_A)
            {
                CHECK_RC(SetupMaxwellA(psubch));
            }
            else
            {
                CHECK_RC(SetupMaxwellB(psubch));
            }
        }
        else if (EngineClasses::IsGpuFamilyClassOrLater(
            subchClass, LWGpuClasses::GPU_CLASS_KEPLER))
        {
            CHECK_RC(SetupKepler(psubch));
        }
        else
        {
            if (subchClass == FERMI_C)
            {
                CHECK_RC(SetupFermi_c(psubch));
            }
            else
            {
                CHECK_RC(SetupFermi(psubch));
            }
        }
    }
    else
    {
        ErrPrintf("Don't know how to initialize class 0x%X\n", subchClass);
        rc = RC::UNSUPPORTED_FUNCTION;
    }

    if (rc != OK)
    {
        ErrPrintf("Class 0x%X init failed: %s\n", subchClass, rc.Message());
        return rc;
    }

    return rc;
}

RC TraceChannel::SetupClass()
{
    RC rc;

    InfoPrintf("Setting up initial class on channel %u on gpu %d:0\n",
        m_pCh->ChannelNum(),
        m_Test->GetBoundGpuDevice()->GetDeviceInst());

    TraceSubChannelList::iterator subchend = SubChEnd();
    for( TraceSubChannelList::iterator iter = SubChBegin();
         iter != subchend; ++iter )
    {
        CHECK_RC(SetupSubChannelClass(*iter));
    }

    // If there are no subchannels, channel could still be used to send down Host only methods
    // so we need to enable ScheduleChannel then the following WFI can be exelwted
    // Notes: for any dynamic channel allocation including traceOp, pcy and UTL, we may need to add support as well
    // by introducing some extra flags specified by users
    if (IsHostOnlyChannel())
    {
        CHECK_RC(m_pCh->ScheduleChannel(true));
    }

    if ((params->ParamPresent("-insert_method") > 0) ||
        (params->ParamPresent("-insert_class_method") > 0) ||
        (params->ParamPresent("-insert_subch_method") > 0))
    {
        CHECK_RC(InjectMassageMethods());
    }

    if (params->ParamPresent("-tsg_name") == 0)
    {
        // When multiple trace3d tests running together in TSG mode,
        // this WFI can cause channel switching, which is undesirable
        // for TSG tests.
        rc = GetCh()->WaitForIdleRC();
        if (rc != OK)
        {
            ErrPrintf("trace_3d: WaitForIdle failed: %s\n", rc.Message());
            return rc;
        }
    }

    InfoPrintf("Done setting up initial class on channel %u on gpu %d:0\n",
        m_pCh->ChannelNum(),
        m_Test->GetBoundGpuDevice()->GetDeviceInst());

    return rc;
}

void TraceChannel::AddZLwllId(UINT32 id)
{
    if (id == UINT32(-1))
    {
        // Invalid id -- do nothing
        return;
    }

    m_Trace->AddZLwllId(id);

    if (m_Test->GetGpuResource()->PerfMon())
    {
        PerformanceMonitor::AddZLwllId(id);
    }
}

namespace {

    class InsertedMethodMassager: public Massager
    {
    public:
        // method-data pair
        typedef pair<UINT32, UINT32> MthdPair;
        // map between channel and method-data pair
        typedef map<TraceSubChannel*, vector<MthdPair> > MapChMthList;

        InsertedMethodMassager(const MapChMthList& m_list)
            : m_MethodList(m_list) {};
        virtual void Massage(PushBufferMessage& message)
        {
            const UINT32 subchNum = message.GetSubChannelNum();
            MapChMthList::const_iterator iter = m_MethodList.begin();
            for ( ; iter != m_MethodList.end(); iter ++)
            {
                if (TraceSubChannel::UNSPECIFIED_TRACE_SUBCHNUM !=
                    iter->first->GetTraceSubChNum())
                {
                    // need to check trace channel number
                    if (subchNum != iter->first->GetTraceSubChNum())
                    {
                        continue;
                    }
                }

                for (UINT32 index=0; index < message.GetPayloadSize(); index++)
                {
                    const UINT32 method = message.GetMethodOffset(index);

                    // massage all method-data list
                    vector<MthdPair>::const_iterator itMthList = iter->second.begin();
                    for (; itMthList != iter->second.end(); itMthList++)
                    {
                        if (itMthList->first == method)
                        {
                            message.SetPayload(index, itMthList->second);
                        }
                    }
                }
            }
        }

    private:

        const MapChMthList & m_MethodList;
    };
}

//
// Inject methods into the pushbuffer and massage the traces - replace all data
// in the trace with the one given in the command line.
//
// Command line format: -insert_method <method_op> <data>
//
// This function will NOT check if method_op and data are valid.
//
RC
TraceChannel::InjectMassageMethods()
{
    //
    // 1. Prepare data to be inserted
    //
    RC  rc;
    UINT32 i, opcode, data, argnum;
    InsertedMethodMassager::MapChMthList insDataList;

    // methods inserted by -insert_method
    // -insert_method alsways uses the first subchannel
    TraceSubChannel * pTraceSubCh = GetSubChannel("");
    argnum = params->ParamPresent("-insert_method");
    for (i = 0; i < argnum; i ++)
    {
        opcode = params->ParamNUnsigned("-insert_method", i, 0);
        data = params->ParamNUnsigned("-insert_method", i, 1);
        insDataList[pTraceSubCh].push_back(make_pair(opcode, data));
    }

    // methods inserted by -insert_class_method
    argnum = params->ParamPresent("-insert_class_method");
    for (i = 0; i < argnum; i ++)
    {
        char *p = NULL;
        const char* className = params->ParamNStr("-insert_class_method", i, 0);
        UINT32 subchClass = (UINT32)Utility::Strtoull(className, &p, 0);
        if ((p == NULL) || (*p != 0))
        {
            // not num format, get class num from name
            subchClass = m_Test->ClassStr2Class(className);
        }

        pTraceSubCh = NULL;
        TraceSubChannelList::iterator iter;
        for(iter = SubChBegin(); iter != SubChEnd(); iter++)
        {
            if (subchClass == (*iter)->GetClass())
            {
                pTraceSubCh = *iter;
                break;
            }
        }

        if (NULL == pTraceSubCh)
        {
            DebugPrintf(MSGID(Massage), "%s -insert_class_method: can't find matched subChannel with"
                "specified class(0x%X), skip method insertion.\n",
                __FUNCTION__, subchClass);
            continue;
        }

        opcode = params->ParamNUnsigned( "-insert_class_method", i, 1);
        data = params->ParamNUnsigned("-insert_class_method", i, 2);
        insDataList[pTraceSubCh].push_back(make_pair(opcode, data));
    }

    // methods inserted by -insert_subch_method
    argnum = params->ParamPresent("-insert_subch_method");
    for (i = 0; i < argnum; i ++)
    {
        const char* subchname = params->ParamNStr("-insert_subch_method", i, 0);
        pTraceSubCh = GetSubChannel(subchname);

        if (NULL == pTraceSubCh)
        {
            DebugPrintf(MSGID(Massage), "%s -insert_subch_method: can't find matched subChannel with"
                "specified name(%s), skip method insertion.\n",
                __FUNCTION__, subchname);
            continue;
        }

        opcode = params->ParamNUnsigned( "-insert_subch_method", i, 1);
        data = params->ParamNUnsigned("-insert_subch_method", i, 2);
        insDataList[pTraceSubCh].push_back(make_pair(opcode, data));
    }

    //
    // 2. Insert the methods to subchannel
    //
    InsertedMethodMassager::MapChMthList::const_iterator itSubch;
    for (itSubch = insDataList.begin();
         itSubch != insDataList.end();
         itSubch ++)
    {
        pTraceSubCh = itSubch->first;
        MASSERT(pTraceSubCh);
        LWGpuSubChannel *pSubch = pTraceSubCh->GetSubCh();
        MASSERT(pSubch);

        vector<InsertedMethodMassager::MthdPair>::const_iterator iter;
        for (iter = itSubch->second.begin();
             iter != itSubch->second.end();
             iter ++)
        {
            DebugPrintf(MSGID(Massage), "%s: insert method(0x%x) with data(0x%x) into subch %s.\n",
                __FUNCTION__, iter->first, iter->second,
                pTraceSubCh->GetName().c_str());

            CHECK_RC(pSubch->MethodWriteRC(iter->first, iter->second));
        }
    }

    //
    // 3. Massage non-empty Pushbuffer
    //
    if (insDataList.size() > 0 &&
        m_PbufModules.begin() != m_PbufModules.end())
    {
        DebugPrintf(MSGID(Massage), "Massaging injected method\n");
        InsertedMethodMassager messageMassager(insDataList);
        ModuleIter iMod = m_PbufModules.begin();
        for (; iMod != m_PbufModules.end(); ++iMod)
        {
            (*iMod)->MassagePushBuffer(messageMassager);
        }
    }

    return OK;
}

//
// SCG channel type comes from trace or cmdline
// Only compute channel support scg_compute1
RC TraceChannel::SetSCGType(LWGpuChannel::SCGType type)
{
    // Bug:1265863.  Comment this out because GetComputeSubChannel
    // is causing problems. Eventually we need to fix this to provide
    // some degree of error checking, but we can trust test writers for now..
    //
    // only compute channel support scg_compute1 now
    //if ((0 == GetComputeSubChannel()) &&
    //    (LWGpuChannel::COMPUTE1 == type))
    //{
    //    return RC::BAD_PARAMETER;
    //}

    m_SCGType = type;

    return OK;
}

void TraceChannel::SetPbdma(UINT32 pbdma)
{
    m_Pbdma = pbdma;
}

void TraceChannel::SetVASpace(const shared_ptr<VaSpace> &pVaSpace)
{
    m_pVaSpace = pVaSpace;
    m_hVASpace = m_pVaSpace->GetHandle();
}

// Bug 706087: avoid sending notifiers down subchannels that cannot
// support them.  This change moves the notifiers from the subchannel
// level to the channel level.  (Using host notifier instead of methods.)

// This switch statement is the switch statement from
// SendNotifier2Subdev(), minus the fermi and kepler cases.  It is meant
// to be future-proof.
bool TraceChannel::UseHostSemaphore()
{
    if (m_Test->GetWfiMethod() == WFI_HOST)
    {
        return true;
    }
    for( size_t i=0; i<m_SubChList.size(); ++i )
    {
        if (!(m_SubChList[i]->GrSubChannel() ||
            m_SubChList[i]->I2memSubChannel() ||
            m_SubChList[i]->ComputeSubChannel() ||
            m_SubChList[i]->VideoSubChannel() ||
            m_SubChList[i]->CopySubChannel() ||
            m_SubChList[i]->GetClass() == LW86B6_VIDEO_COMPOSITOR ||
            m_SubChList[i]->GetClass() == GF106_DMA_DECOMPRESS ||
            m_SubChList[i]->GetClass() == FERMI_TWOD_A))
        {
            return true;
        }
    }
    return false;
}

bool TraceChannel::MatchesChannelManagedMode(GpuTrace::ChannelManagedMode mode)
{
    switch(mode)
    {
    case GpuTrace::CPU_GPU_MANAGED:
        return true;
    case GpuTrace::GPU_MANAGED:
        return IsGpuManagedChannel();
    case GpuTrace::CPU_MANAGED:
        return !IsGpuManagedChannel();
    default:
        MASSERT(!"Invalid channel managed mode!\n");
    }

    return false;
}

RC TraceChannel::SendPostTraceMethods()
{
    RC rc;
    TraceSubChannelList::iterator subchend = SubChEnd();
    for( TraceSubChannelList::iterator iter = SubChBegin();
         iter != subchend; ++iter )
    {
        LWGpuSubChannel* psubch = (*iter)->GetSubCh();
        MASSERT( psubch );

        if (EngineClasses::IsClassType("GR", (*iter)->GetClass()))
        {
            if ((*iter)->GetClass() >= MAXWELL_B)
            {
                CHECK_RC(SendPostTraceMethodsMaxwellB( psubch ));
            }
            else if ((*iter)->GetClass() >= KEPLER_C)
            {
                CHECK_RC(SendPostTraceMethodsKepler( psubch ));
            }
        }
    }

    return rc;
}

TraceSubChannel* TraceChannel::AddSubChannel
(
    const string& name,
    const ArgReader* params
)
{
    TraceSubChannel* psubch = new TraceSubChannel(name, m_BuffInfo, params);
    m_SubChList.push_back(psubch);
    return psubch;
}

RC TraceChannel::RemoveSubChannel(TraceSubChannel* subch)
{
     TraceSubChannelList::iterator  it;
     it = find(m_SubChList.begin(), m_SubChList.end(), subch);
     if (it != m_SubChList.end())
     {
         delete *it;
         m_SubChList.erase(it);
     }

     return OK;
}


LwRm* TraceChannel::GetLwRmPtr()
{
    return m_Test->GetLwRmPtr();
}

SmcEngine* TraceChannel::GetSmcEngine()
{
    return m_Test->GetSmcEngine();
}

RC TraceChannel::FalconMethodWrite(LWGpuSubChannel* pSubch, UINT32 falconMethod, UINT32 data, UINT32 nopMethod)
{
    RC rc;

    CHECK_RC(pSubch->MethodWriteRC(falconMethod, data));

    // According to hardware, these NOP methods are needed to ensure that
    // the MME data is good when the firmware method is handled.
    for(int i = 0; i < FALCON_NOP_COUNT; i++)
        CHECK_RC(pSubch->MethodWriteRC(nopMethod, 0));

    return rc;
}

bool TraceChannel::IsGrCE(UINT32 ceType)
{
    if (ceType == CEObjCreateParam::GRAPHIC_CE)
        return true;

    vector<UINT32> grCeGroup;
    if (GetGpuResource()->GetGrCopyEngineType(grCeGroup, GetLwRmPtr()) != OK)
        MASSERT(!"TraceChannel::IsGrCE: Failed to get GrCE group\n");

    if (ceType < CEObjCreateParam::CE_INSTANCE_MAX)
    {
        const UINT32 ceEngineType = CEObjCreateParam::GetEngineTypeByCeType(ceType);
        for(size_t i = 0; i < grCeGroup.size(); i++)
        {
            if (ceEngineType == grCeGroup[i])
            {
                return true;
            }
        }
    }
    return false;
}

// For CE/LWDEC/LWENC/Gr get the engineId offset since they support multiple engines
UINT32 TraceChannel::GetEngineOffset(UINT32 engineBase)
{
    set<UINT32> engineIds;
    for (auto& subCh : m_SubChList)
    {
        UINT32 inst = 0;
        if (MDiagUtils::IsCopyEngine(engineBase))
        {
            inst = subCh->GetCeType();
            MASSERT(inst < LW2080_ENGINE_TYPE_COPY_SIZE);
        }
        else if (MDiagUtils::IsLwDecEngine(engineBase))
        {
            inst = subCh->GetLwDecOffset();
            MASSERT(inst < LW2080_ENGINE_TYPE_LWDEC_SIZE);
        }
        else if (MDiagUtils::IsLWJPGEngine(engineBase))
        {
            inst = subCh->GetLwJpgOffset();
            MASSERT(inst < LW2080_ENGINE_TYPE_LWJPEG_SIZE);
        }
        else if (MDiagUtils::IsLwEncEngine(engineBase))
        {
            inst = subCh->GetLwEncOffset();
            MASSERT(inst < LW2080_ENGINE_TYPE_LWENC_SIZE);
        }
        else if (MDiagUtils::IsGraphicsEngine(engineBase))
        {
            // Legacy mode: only GR0 available
            // SMC mode: With subscription model (each SmcEngine has its own 
            // LwRm*), the client only sees GR0
            inst = 0;
            MASSERT(inst < LW2080_ENGINE_TYPE_GR_SIZE);
        }
        else
        {
            // For non-multiple engine types, the engine offset will be 0
            inst = 0;
        }
        engineIds.insert(inst);
    }
    // Each channel/TSG can be running on only 1 engine (for multiple engine types,
    // it should be just one of the multiple engines)
    MASSERT(engineIds.size() == 1);
    return *(engineIds.begin());
}

void TraceChannel::SetUnspecifiedCE()
{
    for (auto& subCh : m_SubChList)
    {
        if (subCh->CopySubChannel() &&
            subCh->GetCeType() == CEObjCreateParam::UNSPECIFIED_CE)
        {
            // Get the first available CE
            GpuSubdevice *pSubDev = m_Test->GetBoundGpuDevice()->GetSubdevice(0);
            vector<UINT32> supportedCEs;
            if (GetGpuResource()->GetSupportedCeNum(pSubDev, &supportedCEs, GetLwRmPtr()) != OK)
                MASSERT(!"Error in GetSupportedCeNum\n");
            MASSERT(supportedCEs.size() != 0);
            subCh->SetCeType(supportedCEs[0]);
            subCh->SetWasUnspecifiedCE(true);
        }
    }
}

bool TraceChannel::CheckAndSetGrCE(TraceSubChannelList tsgSubChList)
{
    bool isGrCE = false;

    // Check is any channel in the TSG is GR
    if (m_pTraceTsg &&
        (any_of(m_pTraceTsg->GetTraceChannels().begin(),
                m_pTraceTsg->GetTraceChannels().end(),
                [] (const auto & traceChannel) { return MDiagUtils::IsGraphicsEngine(traceChannel->GetEngineId()); } )))
    {
        isGrCE = true;
    }
    // Check if any CE_Type is GrCE
    // or there is a GR subchannel
    else if (any_of(tsgSubChList.begin(), tsgSubChList.end(),
                [this] (const auto & subCh) {return (subCh->CopySubChannel() && this->IsGrCE(subCh->GetCeType())); }))
    {
        isGrCE = true;
    }
    else if (any_of(tsgSubChList.begin(), tsgSubChList.end(),
                    [] (const auto & subCh) {return (subCh->GrSubChannel() || subCh->ComputeSubChannel()); }))
    {
        isGrCE = true;
    }

    // Override the CE subchannels in current channel to GRAPHIC_CE
    if (isGrCE)
    {
        for (const auto & subCh : m_SubChList)
        {
            if (subCh->CopySubChannel() &&
                subCh->GetCeType() != CEObjCreateParam::GRAPHIC_CE &&
                subCh->GetWasUnspecifiedCE())
            {
                WarnPrintf("%s: Overriding UNSPECIFIED_CE to GRAPHIC_CE since TSG/Channel has a GR/GrCE subchannel\n",
                            __FUNCTION__, subCh->GetCeType());
                subCh->SetCeType(CEObjCreateParam::GRAPHIC_CE);
            }
        }
    }

    // Check if all the CE subchannels have the same instance
    // or all of them are GrCE
    for (auto& iSubCh : tsgSubChList)
    {
        for (auto& jSubCh : tsgSubChList)
        {
            if (iSubCh->CopySubChannel() && // Both the subchanneles
                jSubCh->CopySubChannel() && // are Copy subchannel
                (iSubCh->GetCeType() != CEObjCreateParam::UNSPECIFIED_CE) && // Both subchannels CEType
                (jSubCh->GetCeType() != CEObjCreateParam::UNSPECIFIED_CE) && // are specified
                (iSubCh->GetCeType() != jSubCh->GetCeType()) && // Their CE instance do not match
                !IsGrCE(iSubCh->GetCeType()) && // Both of them are non GrCE
                !IsGrCE(jSubCh->GetCeType()))   // GrCE can have different CE instance
            {
                ErrPrintf("%s: Channel %s CE%d subchannel instance does not match CE%d subchannel instance"
                            " and they both are non-GrCE\n",
                            __FUNCTION__, m_Name.c_str(), iSubCh->GetCeType(), jSubCh->GetCeType());
                MASSERT(0);
            }
        }
    }

    return isGrCE;
}

void TraceChannel::CalcEngineId()
{
    GpuDevice *gpuDev = m_Test->GetBoundGpuDevice();
    bool isGrCESubCh = false;

    // EngineId for channels is relevant only starting from Ampere
    if (!(gpuDev->GetSubdevice(0)->HasFeature(Device:: GPUSUB_HOST_REQUIRES_ENGINE_ID)))
    {
        m_EngineId = LW2080_ENGINE_TYPE_NULL;
        return;
    }

    SmcEngine* pSmcEngine = GetSmcEngine();
    TraceSubChannelList tsgSubChList;

    // Get classes for all subchannels within the tracechannels of the same TSG
    if (m_pTraceTsg)
    {
        for (auto const & tsgTraceChannel : m_pTraceTsg->GetTraceChannels())
        {
            tsgSubChList.insert(tsgSubChList.end(), tsgTraceChannel->SubChBegin(), tsgTraceChannel->SubChEnd());
        }
    }
    else
    {
        tsgSubChList.insert(tsgSubChList.end(), m_SubChList.begin(), m_SubChList.end());
    }

    // If the channel does not have any subchannel, there are a few cases:
    // 1. Channel is part of a TSG and TSG already allocated- Use TSG engineId
    // 2. Channel is part of a TSG but TSG has not been allocated:
    // 2.a. tsgSubChList is empty- use GR engine
    // 2.b. tsgSubChList is not empty: get engineId the old way with some tweaks
    //      - Get EngineIdBase via classIds from tsgSubChList
    //      - Use engineOffset = 0 or SmcEngineId (if SMC mode)
    // 3. Channel is a bare channel- use GR engine
    if (m_SubChList.empty())
    {
        // Case 1
        if (m_pTraceTsg && m_pTraceTsg->GetLWGpuTsg())
        {
            m_EngineId = m_pTraceTsg->GetLWGpuTsg()->GetEngineId();
            return;
        }

        // Case 2.a and 3
        if (!m_pTraceTsg || (m_pTraceTsg && (tsgSubChList.empty())))
        {
            m_EngineId = MDiagUtils::GetGrEngineId(0);
            return;
        }
    }

    if (any_of(tsgSubChList.begin(), tsgSubChList.end(),
                [] (const auto & subCh) { return subCh->CopySubChannel(); } ))
    {
        SetUnspecifiedCE();
        isGrCESubCh = CheckAndSetGrCE(tsgSubChList);
    }

    vector<UINT32> classIds;

    // Get classes for all subchannels in this tsg
    for (auto& subCh : tsgSubChList)
    {
        classIds.push_back(subCh->GetClass());
    }

    UINT32 engineIdBase = MDiagUtils::GetEngineIdBase(classIds, gpuDev, GetLwRmPtr(), isGrCESubCh);

    // Graphics engine ID for SmcEngine is always GR0
    if (MDiagUtils::IsGraphicsEngine(engineIdBase) && pSmcEngine)
    {
        m_EngineId = MDiagUtils::GetGrEngineId(0);
        return;
    }

    UINT32 engineOffset = 0;
    if (!m_SubChList.empty())
    {
        // For CE/LWDEC/LWENC/SMC get the engineId offset since they support multiple engines
        engineOffset = GetEngineOffset(engineIdBase);
    }

    // Use GR engineId for SW classes since m_EngineId is mainly for reserving 
    // HW resources and SW engine does not have any HW resources
    if (MDiagUtils::IsSWEngine(engineIdBase))
    {
        m_EngineId = MDiagUtils::GetGrEngineId(0);
        return;
    }

    if (MDiagUtils::IsGraphicsEngine(engineIdBase)) // GR's engineIds are non-continuous
        m_EngineId = MDiagUtils::GetGrEngineId(engineOffset);
    else // For CE/LWDEC/LWENC engineIds are continuous
        m_EngineId = engineIdBase + engineOffset;
}

// -use_channel is a way to create a shared channel between traces
// Trace1: -use_channel A
// Trace2: -use_channel A
// Each of the traces will create its own TraceChannel object (if trace_3d)
// But only one LWGpuChannel will be created, both TraceChannels will use the
// same LWGpuChannel. This sanity check tries to check if there is consistency
// (TSG/SubCtx) between the LWGpuChannel already allocated and this
// TraceChannel object (using the same LWGpuChannel)
// Note:
// Since a lot of tests use -use_channel, some conditions just print a warning
// message (instead of asserting) for now
void TraceChannel::SanityCheckSharedChannel()
{
    if (m_pChHelper->is_channel_setup()) // LWGpuChannel allocation already completed (shared channel)
    {
        // TSG
        if (m_pCh->GetLWGpuTsg()) // Allocated LWGpuChannel already has a TSG allocated
        {
            // This TraceChannel does not have a TSG
            if (!m_pTraceTsg)
            {
                ErrPrintf("-use_channel used and the allocated LWGpuChannel has a TSG (%s) but current TraceChannel (%s) does not have a TSG\n",
                            m_pCh->GetLWGpuTsg()->GetName().c_str(), m_Name.c_str());
                ErrPrintf("Please use -tsg_name in all traces using shared channel to share the tsg of the channel between traces\n");
                MASSERT(0);
            }
            // This TraceChannel has a TSG
            else
            {
                // This TraceChannel has a TSG of a different name
                if (m_pCh->GetLWGpuTsg()->GetName() != m_pTraceTsg->GetName())
                {
                    ErrPrintf("-use_channel used but 2 separate TSGs (%s, %s) are being created for 1 shared channel\n",
                                m_pCh->GetLWGpuTsg()->GetName().c_str(), m_pTraceTsg->GetName().c_str());
                    ErrPrintf("Please use -tsg_name in all traces using shared channel to share the tsg of the channel between traces\n");
                    MASSERT(0);
                }
                // This TraceChannel has a TSG of the same name
                else
                {
                    // Check if both of them are shared
                    if (!m_pCh->GetLWGpuTsg()->IsShared() ||
                        !m_pTraceTsg->IsSharedTsg())
                    {
                        ErrPrintf("-use_channel used but 2 separate TSGs (%s) with the same name are being created for 1 shared channel\n",
                                m_pCh->GetLWGpuTsg()->GetName().c_str(), m_pTraceTsg->GetName().c_str());
                        ErrPrintf("Please use -tsg_name in all traces using shared channel to share the tsg of the channel between traces\n");
                        MASSERT(0);
                    }
                }
            }
        }
        else // Allocated LWGpuChannel does not have a TSG
        {
            // This TraceChannel has a TSG
            if (m_pTraceTsg)
            {
                WarnPrintf("-use_channel used and the allocated LWGpuChannel does not have a TSG but current TraceChannel (%s) has a TSG (%s)\n",
                            m_Name.c_str(), m_pTraceTsg->GetName().c_str());
                WarnPrintf("Overriding TraceChannel's(%s) TSG (%s) to nullptr\n", m_Name.c_str(), m_pTraceTsg->GetName().c_str());
                WarnPrintf("If all the traces using the same -use_channel are trace_3d then please add -tsg_name to all of them\n");
                WarnPrintf("If any of the traces using the same -use_channel is non-trace_3d then please use -ignore_tsgs for this trace\n");
                m_pTraceTsg = nullptr;
            }
        }

        // SubCtx
        if (m_pCh->GetSubCtx()) // Allocated LWGpuChannel already has a subcontext
        {
            // This TraceChannel does not have a subctx
            if (!m_pSubCtx)
            {
                ErrPrintf("-use_channel used and the allocated LWGpuChannel has a SubCtx (%s) but current TraceChannel (%s) does not have a SubCtx\n",
                            m_pCh->GetSubCtx()->GetName().c_str(), m_Name.c_str());
                ErrPrintf("Please use -subcontext_name in all traces using shared channel to share the subcontext of the channel between traces\n");
                MASSERT(0);
            }

            // This TraceChannel has a subctx, but a different one
            else if (m_pCh->GetSubCtx() != m_pSubCtx)
            {
                ErrPrintf("-use_channel used but 2 separate SubCtxs (%s, %s) are being created for 1 shared channel\n",
                            m_pCh->GetSubCtx()->GetName().c_str(), m_pSubCtx->GetName().c_str());
                ErrPrintf("Please use -subcontext_name in all traces using shared channel to share the subcontext of the channel between trace\n");
                MASSERT(0);
            }
        }
        else // Allocated LWGpuChannel does not have a subctx
        {
            if (m_pSubCtx) // This TraceChannel has a subctx
            {
                WarnPrintf("-use_channel used and the allocated LWGpuChannel does not have a SubCtx but current TraceChannel (%s) has a SubCtx (%s)\n",
                            m_Name.c_str(), m_pSubCtx->GetName().c_str());
                WarnPrintf("Overriding TraceChannel's(%s) Subctx (%s) to nullptr\n", m_Name.c_str(), m_pSubCtx->GetName().c_str());
                WarnPrintf("If all the traces using the same -use_channel are trace_3d then please add -subcontext_name to all of them\n");
                WarnPrintf("If any of the traces using the same -use_channel is non-trace_3d then please use -ignore_subcontexts for this trace\n");
                m_pSubCtx.reset();
            }
        }
    }
}
