/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Dma Copy Test.  Used to test multiple copy engine instances conlwrrently.

#include "lwos.h"
#include "Lwcm.h"

#include "class/cl90b5.h" // GF100_DMA_COPY
#include "class/cla0b5.h" // KEPLER_DMA_COPY_A
#include "class/clb0b5.h" // MAXWELL_DMA_COPY_A
#include "class/clc0b5.h" // PASCAL_DMA_COPY_A
#include "class/clc1b5.h" // PASCAL_DMA_COPY_B
#include "class/clc3b5.h" // VOLTA_DMA_COPY_A
#include "class/clc5b5.h" // TURING_DMA_COPY_A
#include "class/clc6b5.h" // AMPERE_DMA_COPY_A
#include "class/clc7b5.h" // AMPERE_DMA_COPY_B
#include "class/clc8b5.h" // HOPPER_DMA_COPY_A
#include "class/clc9b5.h" // BLACKWELL_DMA_COPY_A
#include "class/cl85b5sw.h"
#include "class/cl90b5sw.h"
#include "class/cla06fsubch.h"

#include "core/include/tee.h"
#include "core/include/xp.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "core/include/console.h"
#include "core/include/cnslmgr.h"
#include "core/include/jsonlog.h"
#include "core/include/lwrm.h"
#include "core/include/channel.h"
#include "core/include/gpu.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/utility/blocklin.h"
#include "gpu/include/gpumgr.h"

#include "random.h"
#include "cpyengtest.h"

using Utility::AlignUp;
using Utility::AlignDown;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

const UINT32 Pitch_Align = 4;

const UINT32 Block_Align = 256;

const UINT32 CpyEngTest::sc_DmaCopy = LWA06F_SUBCHANNEL_COPY_ENGINE;
const UINT32 Payload = 0x87654321;

static const char * strLoc[] =
    {
        "Fb",
        "C ",
        "NC"
    };

static void InitVar(const char *varName, UINT32 *var, UINT32 maxVal)
{
    if (*var == 0 || *var > maxVal)
    {
        Printf(Tee::PriDebug, "Clamping %s to %d\n", varName, maxVal);
        *var = maxVal;
    }
}

static void FixFormat(UINT32 *format)
{
    switch(*format)
    {
        case 1:
        case 2:
        case 4:
            break;

        default:
            Printf(
                Tee::PriDebug,
                "Format %d out of range (must be 1, 2, or 4).  Setting to 1\n",
                *format);
            *format = 1;
    }
}

static void DumpOneChunk(Tee::Priority pri, UINT08 *addr, UINT32 chunkSize);

//------------------------------------------------------------------------------

CpyEngTest::CpyEngTest() :
    m_CoherentSupported(true),
    m_NonCoherentSupported(true),
    m_ForcePitchInEven(false),
    m_ForcePitchOutEven(false)
{
    SetName("CpyEngTest");
    m_pTstCfg = GetTestConfiguration();
    m_hCh.clear();
    m_pCh.clear();
    // Have at least 1 dma copy alloc so that it can be queried in IsSupported
    m_DmaCopyAlloc.resize(1);
    m_mapDstPicker.clear();
    m_pFpCtx = GetFpContext();

    CreateFancyPickerArray(FPK_COPYENG_NUM_PICKERS);
    m_pPickers = GetFancyPickerArray();

    m_pSurfaces.resize(NUM_SURFACES);

    m_TotalErrorCount = 0;
    m_SurfSize = 0;
    m_MinLineWidthInBytes = 0;
    m_MaxXBytes = 0;
    m_MaxYLines = 0;
    m_PrintPri = Tee::PriSecret; // Instead of abusing PriDebug, this test should use VerbosePrintf
    m_DetailedPri = Tee::PriSecret;
    m_TestStep = false;
    m_XOptThresh = 0;
    m_YOptThresh = 0;
    m_CheckInputForError = true;
    m_DebuggingOnLA = false;
    m_DumpChunks = 0;
    m_NumLoopsWithError = 0;
    m_LogErrors = false;
    m_EngineCount = 0;
    m_SubdevMask = 0;
    m_EngMask = ~0U;
    m_AllMappings = false;
}

//------------------------------------------------------------------------------

CpyEngTest::~CpyEngTest()
{
    FreeMem();
}

//------------------------------------------------------------------------------

RC CpyEngTest::Setup()
{
    TestDevicePtr pTestDevice = GetBoundTestDevice();
    GpuSubdevice *pGpuSubdevice = pTestDevice->GetInterface<GpuSubdevice>();
    RC rc;

    CHECK_RC(PexTest::Setup());
    CHECK_RC(GpuTest::AllocDisplay());

    // Allocate memory and GPU resources:
    m_CoherentSupported =
        pGpuSubdevice->HasFeature(Device::GPUSUB_HAS_COHERENT_SYSMEM);
    m_NonCoherentSupported = pGpuSubdevice->SupportsNonCoherent();
    CHECK_RC(InitModeSpecificMembers());
    CHECK_RC(AllocMem());

    CHECK_RC(GetDisplayMgrTestContext()->DisplayImage(&m_pSurfaces[FB_LINEAR],
        DisplayMgr::DONT_WAIT_FOR_UPDATE));

    CHECK_RC(FillSurfaces());

    vector<UINT32> classIds;
    classIds.push_back(GF100_DMA_COPY);
    classIds.push_back(KEPLER_DMA_COPY_A);
    classIds.push_back(MAXWELL_DMA_COPY_A);
    classIds.push_back(PASCAL_DMA_COPY_A);
    classIds.push_back(PASCAL_DMA_COPY_B);
    classIds.push_back(VOLTA_DMA_COPY_A);
    classIds.push_back(TURING_DMA_COPY_A);
    classIds.push_back(AMPERE_DMA_COPY_A);
    classIds.push_back(AMPERE_DMA_COPY_B);
    classIds.push_back(HOPPER_DMA_COPY_A);
    classIds.push_back(BLACKWELL_DMA_COPY_A);

    CHECK_RC(GetBoundGpuDevice()->GetPhysicalEnginesFilteredByClass(classIds, &m_EngineList));
    sort(m_EngineList.begin(), m_EngineList.end());

    m_EngineCount = static_cast<INT32>(m_EngineList.size());
    Printf(Tee::PriDebug, "Number of logical copy engines = %i\n", m_EngineCount);

    constexpr INT32 numBits = static_cast<INT32>(CHAR_BIT * (sizeof m_EngMask));
    if (m_EngineCount > numBits)
    {
        Printf(Tee::PriError,
               "Number of engines exceeds %d, update EngMask processing\n",
               numBits);
        return RC::SOFTWARE_ERROR;
    }

    if (!m_EngineCount)
    {
        Printf(Tee::PriError, "No copy engines found!\n");
        // Commented out due to RM bug 2717652, RM returns empty PCE mask in VF
        //return RC::UNSUPPORTED_DEVICE;
    }

    LW2080_CTRL_CE_SET_PCE_LCE_CONFIG_PARAMS ceRestore = {};
    if ((m_AllMappings || !GetBoundGpuSubdevice()->HasFeature(GpuSubdevice::GPUSUB_SUPPORTS_LCE_PCE_ONE_TO_ONE)) &&
        (!GetBoundGpuSubdevice()->HasFeature(GpuSubdevice::GPUSUB_SUPPORTS_LCE_PCE_REMAPPING) ||
         RC::OK != GetBoundGpuSubdevice()->GetPceLceMapping(&ceRestore)))
    {
        Printf(Tee::PriError, "GPU doesn't support runtime changes to PceLce mapping\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    return OK;
}

//------------------------------------------------------------------------------

RC CpyEngTest::SetupChannelsPerInstance(const vector<UINT32>& engineList)
{
    RC rc;
    m_pTstCfg->SetAllowMultipleChannels(true);

    for (UINT32 i = 0; i < engineList.size(); i++)
    {
        CHECK_RC(m_pTstCfg->AllocateChannelWithEngine(&m_pCh[i],
                                                  &m_hCh[i],
                                                  &m_DmaCopyAlloc[i],
                                                  engineList[i]));
        Printf(Tee::PriLow, "Allocated Channel and DMA Copy object.\n");

        //Instantiate Object
        CHECK_RC(m_pCh[i]->SetObject(sc_DmaCopy, m_DmaCopyAlloc[i].GetHandle()));
        Printf(Tee::PriLow, "Instantiated DMA Copy object.\n");

        CHECK_RC(m_Semaphore.Allocate(m_pCh[i], i));
    }
    return rc;
}

//------------------------------------------------------------------------------

RC CpyEngTest::Cleanup()
{
    StickyRC firstRc;

    if (GetDisplayMgrTestContext())
    {
        firstRc = GetDisplayMgrTestContext()->DisplayImage(
                static_cast<Surface*>(0), DisplayMgr::WAIT_FOR_UPDATE);
    }

    firstRc = CleanupEngines();

    for (vector<Surface2D>::iterator itr = m_pSurfaces.begin();
            itr != m_pSurfaces.end(); ++itr)
    {
        if (itr->IsAllocated())
            itr->Free();
    }

    m_SrcCheckSurf.Free();
    m_DstCheckSurf.Free();

    firstRc = PexTest::Cleanup();

    return firstRc;
}
//-----------------------------------------------------------------------------

bool CpyEngTest::IsSupported()
{
    GpuDevice * pGpuDev = GetBoundGpuDevice();

    if (m_DmaCopyAlloc[0].IsSupported(pGpuDev))
    {
        return true;
    }
    else
    {
        return false;
    }
}

//-----------------------------------------------------------------------------

bool CpyEngTest::ParseUserDataPattern(const JsArray &ptnArray)
{
    m_UserDataPattern.clear();
    JavaScriptPtr pJavaScript;
    UINT32 temp;

    for (UINT32 i = 0; i < ptnArray.size(); ++i)
    {
        if (OK != pJavaScript->FromJsval(ptnArray[i], &temp))
        {
            m_UserDataPattern.clear();

            Printf(Tee::PriNormal, "Can't parse element %d of the array.\n", i);
            return false;
        }
        m_UserDataPattern.push_back(temp);
    }

    return true;
}

//-----------------------------------------------------------------------------

void CpyEngTest::AddToUserDataPattern(UINT32 val)
{
    m_UserDataPattern.push_back(val);
}

//-----------------------------------------------------------------------------

void CpyEngTest::PrintUserDataPattern(Tee::Priority pri) const
{
    Printf(pri, "Using user data pattern of length %d:",
           (int)m_UserDataPattern.size());

    for (UINT32 i = 0; i < m_UserDataPattern.size(); ++i)
    {
        Printf(pri,"%08x  ", m_UserDataPattern[i]);
        if ((i % 4) == 0)
            Printf(pri, "\n");
    }
    Printf(pri, "\n");
}

//------------------------------------------------------------------------------

void CpyEngTest::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);
    // Print the options we arrived at, so the user can detect .js file typos...

    if (m_SurfSize)
        Printf(pri, "\tSurfSize:    %d\n", m_SurfSize);
    if (m_MinLineWidthInBytes)
        Printf(pri, "\tSurfPitch:   %d\n", m_MinLineWidthInBytes);
    if (m_MaxXBytes)
        Printf(pri, "\tMaxXBytes:   %d\n", m_MaxXBytes);
    if (m_MaxYLines)
        Printf(pri, "\tMaxYLines:   %d\n", m_MaxYLines);
    if (m_PrintPri != Tee::PriDebug)
        Printf(pri, "\tPrintPri:    %d\n", m_PrintPri);
    if (m_DetailedPri != Tee::PriDebug)
        Printf(pri, "\tDetailedPri: %d\n", m_DetailedPri);
    Printf(pri, "\tEngMask:     0x%x\n", m_EngMask);
    Printf(pri, "\tAllMappings: %s\n", (m_AllMappings)?"true":"false");
}

//------------------------------------------------------------------------------

RC CpyEngTest::InitModeSpecificMembers()
{
    UINT32 w = m_pTstCfg->SurfaceWidth();
    UINT32 h = m_pTstCfg->SurfaceHeight();

    // Learn the pitch, decide what size surfaces to allocate.

    // Update parameters to match the video mode.
    m_MinLineWidthInBytes = w*ColorUtils::PixelBytes(ColorUtils::A8R8G8B8);
    // >=G80 displayable surfaces must have a 256-byte aligned pitch
    m_MinLineWidthInBytes = AlignUp<256>(m_MinLineWidthInBytes);

    InitVar("SurfSize", &m_SurfSize, m_MinLineWidthInBytes * h);

    InitVar("MaxXBytes", &m_MaxXBytes, m_MinLineWidthInBytes);

    // 2047 is the maximum for Class039 LineCount() method
    UINT32 dispMaxLines = ((m_SurfSize/2) + m_MinLineWidthInBytes - 1)
        / m_MinLineWidthInBytes;

    InitVar("MaxYLines", &m_MaxYLines, min((UINT32)2047, dispMaxLines));

    return OK;
}

//------------------------------------------------------------------------------

void CpyEngTest::SeedRandom(UINT32 s)
{
    m_pFpCtx->Rand.SeedRandom(s);
}

//------------------------------------------------------------------------------

RC CpyEngTest::RunTest()
{
    RC rc;
    LwRmPtr pLwrm;

    // For SLI devices, send methods to only one of the gpus.
    m_SubdevMask = 1 << GetBoundGpuSubdevice()->GetSubdeviceInst();

    m_ErrorLog.clear(); // clean out any errors from previous runs
    m_NumLoopsWithError = 0;
    m_TotalErrorCount = 0;

    UINT32 pceMask = GetBoundGpuSubdevice()->GetFsImpl()->PceMask();
    const bool supportsRemapping = GetBoundGpuSubdevice()->HasFeature(GpuSubdevice::GPUSUB_SUPPORTS_LCE_PCE_REMAPPING);
    const bool requiresRemapping = m_AllMappings || !GetBoundGpuSubdevice()->HasFeature(GpuSubdevice::GPUSUB_SUPPORTS_LCE_PCE_ONE_TO_ONE);

    // Restore the original CE Mapping when we're finished
    LW2080_CTRL_CE_SET_PCE_LCE_CONFIG_PARAMS ceRestore = {};
    if (supportsRemapping)
    {
        CHECK_RC(GetBoundGpuSubdevice()->GetPceLceMapping(&ceRestore));
        ceRestore.ceEngineType = m_EngineList[0];
    }
    auto RestoreCe = [&]() -> RC
    {
        if (supportsRemapping && requiresRemapping)
        {
            return pLwrm->ControlBySubdevice(GetBoundGpuSubdevice(),
                                             LW2080_CTRL_CMD_CE_SET_PCE_LCE_CONFIG,
                                             &ceRestore, sizeof(ceRestore));
        }
        return RC::OK;
    };
    DEFERNAME(restore)
    {
        RestoreCe();
    };

    vector<vector<UINT32>> iterations;
    if (!supportsRemapping || !requiresRemapping)
    {
        // With 1:1 mapping we can test all the LCE/PCEs at once
        iterations.push_back(m_EngineList);
    }
    else
    {
        vector<UINT32> validEngines;
        for (UINT32 engine : m_EngineList)
        {
            const UINT32 engIdx = LW2080_ENGINE_TYPE_COPY_IDX(engine);
            if (!(m_EngMask & (1 << engIdx)))
                continue;
            validEngines.push_back(engine);
        }

        const UINT32 validEnginesSize = static_cast<UINT32>(validEngines.size());
        MASSERT(validEnginesSize >= 2);

        // Test two engines at a time, only changing one engine at a time
        for (UINT32 idx = 0; idx < validEnginesSize; idx++)
        {
            const UINT32 engine = m_EngineList[idx];
            const UINT32 pairIdx = (idx + validEnginesSize - 1) % validEnginesSize;
            const UINT32 enginePair = m_EngineList[pairIdx];
            iterations.push_back(vector<UINT32>{engine, enginePair});

            if (!m_AllMappings || validEnginesSize == 2)
                break; // Only need the first pair
        }
    }

    for (const vector<UINT32>& engineList : iterations)
    {
        if (requiresRemapping)
        {
            MASSERT(engineList.size() == 2);
            const UINT32 engIdx = LW2080_ENGINE_TYPE_COPY_IDX(engineList[0]);
            const UINT32 engPairIdx = LW2080_ENGINE_TYPE_COPY_IDX(engineList[1]);

            LW2080_CTRL_CE_SET_PCE_LCE_CONFIG_PARAMS ceMap = {};
            ceMap.ceEngineType = engineList[0];

            string remapStr = "Remapping CE:\n\tpceLceMap: [";

            const UINT32 pceCount = Utility::CountBits(pceMask);
            for (UINT32 i = 0, mapCount = 0; i < LW2080_CTRL_MAX_PCES; i++)
            {
                if (pceMask & (1 << i))
                {
                    if (mapCount++ < (pceCount / 2))
                        ceMap.pceLceMap[i] = engIdx;
                    else
                        ceMap.pceLceMap[i] = engPairIdx;
                }
                else
                {
                    ceMap.pceLceMap[i] = 0xF;
                }

                if (i != 0)
                    remapStr += ",";
                remapStr += Utility::StrPrintf(" 0x%x", ceMap.pceLceMap[i]);
            }
            remapStr += Utility::StrPrintf("]\n\tgrceSharedLceMap: [0x%x, 0x%x]\n", engIdx, engPairIdx);
            ceMap.grceSharedLceMap[0] = engIdx;
            ceMap.grceSharedLceMap[1] = engPairIdx;

            VerbosePrintf(remapStr.c_str());

            CHECK_RC(pLwrm->ControlBySubdevice(GetBoundGpuSubdevice(),
                                               LW2080_CTRL_CMD_CE_SET_PCE_LCE_CONFIG,
                                               &ceMap, sizeof(ceMap)));
        }

        CHECK_RC(InitializeEngines(engineList));

        Printf(Tee::PriLow, "Starting test.\n");
        rc = DoDmaTest(engineList);
        Printf(Tee::PriLow, "Completed test.\n");

        if (m_LogErrors && !m_ErrorLog.empty())
        {

            rc = RC::MEM_TO_MEM_RESULT_NOT_MATCH;
            Printf(Tee::PriHigh, "\nERRORS FOR THIS RUN\n");
            for (ErrIterator it = m_ErrorLog.begin(); it != m_ErrorLog.end();
                ++it)
            {
                Printf(
                    Tee::PriHigh,
                    "PhysAddr: 0x%08x, Expect 0x%02x, Actual 0x%02x, Count %5d\n",
                    Platform::VirtualToPhysical32(it->first.virtAddr),
                    it->first.expectedVal, it->first.actualVal, it->second);

                m_TotalErrorCount += it->second;
            }

            Printf(Tee::PriHigh, "Total errors: %d\n", m_TotalErrorCount);

            Printf(Tee::PriHigh, "\n%d out of %d DMAs had 1 or more errors\n",
                   m_NumLoopsWithError, m_pTstCfg->Loops());

        }

        CHECK_RC(rc);

        CHECK_RC(CleanupEngines());
    }

    restore.Cancel();
    CHECK_RC(RestoreCe());

    return rc;
}

//------------------------------------------------------------------------

RC CpyEngTest::InitializeEngines(const vector<UINT32>& engineList)
{
    RC rc;

    const UINT32 instanceCount = static_cast<UINT32>(engineList.size());

    m_srcDims.resize(instanceCount);
    m_dstDims.resize(instanceCount);
    m_testParams.resize(instanceCount);
    m_pCh.resize(instanceCount);
    m_hCh.resize(instanceCount);
    m_DmaCopyAlloc.resize(instanceCount);
    m_Semaphore.Setup(NotifySemaphore::ONE_WORD,
                      m_pTstCfg->NotifierLocation(),
                      instanceCount);
    CHECK_RC(SetupChannelsPerInstance(engineList));
    return rc;
}

//------------------------------------------------------------------------
RC CpyEngTest::CleanupEngines()
{
    StickyRC firstRc;

    for (UINT32 id = 0; id < m_DmaCopyAlloc.size(); id++)
    {
        m_DmaCopyAlloc[id].Free();
    }

    for (UINT32 id = 0; id < m_pCh.size(); id++)
    {
        if (GetBoundGpuDevice()->GetNumSubdevices() > 1)
            m_pCh[id]->WriteSetSubdevice(Channel::AllSubdevicesMask);
        firstRc = m_pTstCfg->FreeChannel(m_pCh[id]);
    }

    m_Semaphore.Free();

    return firstRc;
}

//------------------------------------------------------------------------

RC CpyEngTest::DoDmaTest(const vector<UINT32>& engineList)
{
    RC rc;
    UINT32 startLoop        = m_pTstCfg->StartLoop();
    UINT32 restartSkipCount = m_pTstCfg->RestartSkipCount();
    UINT32 seed             = m_pTstCfg->Seed();
    UINT32 endLoop          = startLoop + m_pTstCfg->Loops();

    if ((startLoop % restartSkipCount) != 0)
        Printf(Tee::PriNormal, "WARNING: StartLoop is not a restart point.\n");

    m_pPickers->RestartInitialized();

    Printf(m_PrintPri,
           "loop  src->dst (fmt pitch off lineBytes lineCount) INPUT\n"
           "               (fmt pitch off)                     OUTPUT\n"
           "=========================================================\n");

    m_pFpCtx->LoopNum = startLoop;
    // keep track of true loop count for reseeding random number generator
    UINT32 trueLoopCount = startLoop;

    while (m_pFpCtx->LoopNum < endLoop)
    {
        if ((trueLoopCount == startLoop) ||
            ((trueLoopCount % restartSkipCount) == 0))
        {
            m_pFpCtx->RestartNum = trueLoopCount / restartSkipCount;

            Printf(Tee::PriLow, "\n\tRestart: loop %d, seed %#010x\n",
                   m_pFpCtx->LoopNum, seed + trueLoopCount);

            SeedRandom(seed + trueLoopCount);
            m_pPickers->RestartInitialized();
        }
        // Increment actual loop coun to avoid reusing current seed value.
        ++trueLoopCount;
        for (UINT32 engNum = 0; engNum < engineList.size(); engNum++)
        {
            UINT32 srcIndex, dstIndex;
            bool notFound = true;
            while(notFound)
            { // repeatedly check surfaces until we find one with free space
                srcIndex = ValidatePick(
                    (*m_pPickers)[FPK_COPYENG_SRC_SURF].Pick());
                notFound = m_mapDstPicker[&m_pSurfaces[srcIndex]].IsFull();
            }
            notFound = true;
            while(notFound)
            { // repeatedly check surfaces until we find one with free space
                dstIndex = ValidatePick(
                    (*m_pPickers)[FPK_COPYENG_DST_SURF].Pick());
                notFound = m_mapDstPicker[&m_pSurfaces[dstIndex]].IsFull();
            }

            m_srcDims[engNum].surf = &m_pSurfaces[srcIndex];
            m_srcDims[engNum].MemLayout = m_pSurfaces[srcIndex].GetLayout();
            m_srcDims[engNum].skipRun = false;
            m_dstDims[engNum].surf = &m_pSurfaces[dstIndex];
            m_dstDims[engNum].MemLayout = m_pSurfaces[dstIndex].GetLayout();
            m_dstDims[engNum].skipRun = false;

            // Check that surface ranges have been allocated

            if (m_mapDstPicker[m_srcDims[engNum].surf].IsEmpty())
            {
                m_mapDstPicker[m_srcDims[engNum].surf].InitSeed(m_pFpCtx->Rand.GetRandom());
                if (m_srcDims[engNum].MemLayout == Surface2D::Pitch)
                    m_mapDstPicker[m_srcDims[engNum].surf].AllocRange(
                        0, m_SurfSize, Pitch_Align);
                else
                    m_mapDstPicker[m_srcDims[engNum].surf].AllocRange(
                        0, m_SurfSize, Block_Align);
            }

            if (m_mapDstPicker[m_dstDims[engNum].surf].IsEmpty())
            {
                m_mapDstPicker[m_dstDims[engNum].surf].InitSeed(m_pFpCtx->Rand.GetRandom());
                if (m_dstDims[engNum].MemLayout == Surface2D::Pitch)
                    m_mapDstPicker[m_dstDims[engNum].surf].AllocRange(
                        0, m_SurfSize, Pitch_Align);
                else
                    m_mapDstPicker[m_dstDims[engNum].surf].AllocRange(
                        0, m_SurfSize, Block_Align);
            }

            CHECK_RC(m_srcDims[engNum].surf->BindGpuChannel(m_hCh[engNum]));
            CHECK_RC(m_dstDims[engNum].surf->BindGpuChannel(m_hCh[engNum]));

            rc = CopyAndVerifyOneLoop(engNum,&m_srcDims[engNum],
                                          &m_dstDims[engNum],
                                          &m_testParams[engNum].lengthIn,
                                          &m_testParams[engNum].lineCount,
                                          m_PrintPri);
            if (OK != rc)
            {
                GetJsonExit()->AddFailLoop(m_pFpCtx->LoopNum);
                return rc;
            }
        }
        bool Run = true;
        // Check all engines for parameter failures;
        // If any failed, then abort this run
        for (UINT32 engNum = 0; engNum < engineList.size(); engNum++)
            if (m_srcDims[engNum].skipRun)
                Run = false;

        if (Run)
        {
            // copy surface parameters were acceptable; proceed
            ++m_pFpCtx->LoopNum;
            // Write methods
            for (UINT32 engNum = 0; engNum < engineList.size(); engNum++)
            {
                const UINT32 engIdx = LW2080_ENGINE_TYPE_COPY_IDX(engineList[engNum]);
                if (!(m_EngMask & (1 << engIdx)))
                    continue;
                CHECK_RC(WriteMethods(engNum, &m_srcDims[engNum],
                                      &m_dstDims[engNum],
                                      m_testParams[engNum].lengthIn,
                                      m_testParams[engNum].lineCount,
                                      m_PrintPri));
            }

            // Flush all channels
            for (UINT32 engNum = 0; engNum < engineList.size(); engNum++)
            {
                const UINT32 engIdx = LW2080_ENGINE_TYPE_COPY_IDX(engineList[engNum]);
                if (!(m_EngMask & (1 << engIdx)))
                    continue;
                m_pCh[engNum]->Flush();
            }

            // Wait for all tests to complete and perform checks

            rc = m_Semaphore.Wait(m_pTstCfg->TimeoutMs());

            if (rc != OK)
            {
                Printf(
                    m_PrintPri,
                    "Error WAITING ON NOTIFIER, skipping to data description\n");
                GetJsonExit()->AddFailLoop(m_pFpCtx->LoopNum);
                return rc;
            }
            // Check all memory copies
            rc = PerformAllChecks(engineList, m_PrintPri);
            if (OK != rc)
            {
                GetJsonExit()->AddFailLoop(m_pFpCtx->LoopNum);
                return rc;
            }

            CHECK_RC(GetPcieErrors(GetBoundGpuSubdevice()));

            if (m_TestStep)
            {
                ConsoleManager::ConsoleContext consoleCtx;
                Console* pConsole = consoleCtx.AcquireConsole(false);
                Console::VirtualKey k = pConsole->GetKey();
                if (k == Console::VKC_LOWER_Q)
                    return RC::USER_ABORTED_SCRIPT;
            }
        }
        m_mapDstPicker.clear();
    }

    CHECK_RC(CheckPcieErrors(GetBoundGpuSubdevice()));
    return rc;
}

//------------------------------------------------------------------------------

void CpyEngTest::ForcePitchEvenIfApplicable
(
    UINT32 which,
    UINT32 *pitch,
    UINT32  minPitch
)
{
    switch(which)
    {
        case FPK_COPYENG_PITCH_IN:
            if (!m_ForcePitchInEven)
                return;
            break;
        case FPK_COPYENG_PITCH_OUT:
            if (!m_ForcePitchOutEven)
                return;
            break;

        default:
            MASSERT(!"Invalid PITCH passed to ForcePitchEvenIfApplicable");
            return;
    }

    if ((*pitch % 2) != 0)
    {
        // Round down so we don't run into issues with the size of the surface,
        // unless rounding down would cause us to go below the min.
        if (*pitch - 1 >= minPitch)
        {
            *pitch -= 1;
        }
        else
        {
            *pitch += 1;
        }
    }
}

//-----------------------------------------------------------------------------

RC CpyEngTest::CopyAndVerifyOneLoop
(
    UINT32 id,
    SurfDimensions *srcDims,
    SurfDimensions *dstDims,
    UINT32 *lengthIn,
    UINT32 *lineCount,
    UINT32 pri
)
{
    RC rc;

    m_Semaphore.SetOneTriggerPayload(id, Payload);
    // Init and check surface and copy dimensions
    if (SetupSurfaceDims(srcDims, dstDims,
                         lineCount, lengthIn, pri))
    {
        if (!(m_EngMask & (1 << id)))
            m_Semaphore.SetOnePayload(id, Payload);
        else
            m_Semaphore.SetOnePayload(id, 0);
    }
    else
    {
        srcDims->skipRun = true;
        return rc;
    }

    return rc;
}

//-----------------------------------------------------------------------------

RC CpyEngTest::WriteMethods
(
    UINT32 id,
    SurfDimensions *srcDims,
    SurfDimensions *dstDims,
    UINT32 lengthIn,
    UINT32 lineCount,
    UINT32 pri
)
{
    UINT32 offsetInUpper  =
        UINT32((srcDims->surf->GetCtxDmaOffsetGpu() + srcDims->offsetOrig) >> 32);
    UINT32 offsetOutUpper =
        UINT32((dstDims->surf->GetCtxDmaOffsetGpu() + dstDims->offsetOrig) >> 32);

    UINT32 offsetIn  = UINT32(srcDims->surf->GetCtxDmaOffsetGpu()
                              + srcDims->offsetOrig);
    UINT32 offsetOut = UINT32(dstDims->surf->GetCtxDmaOffsetGpu()
                              + dstDims->offsetOrig);

    Printf(pri, "rdDma: %08x, wrDma: %08x, offsetIn: %d, offsetOut: %d\n",
           srcDims->surf->GetCtxDmaHandleGpu(),
           dstDims->surf->GetCtxDmaHandleGpu(), offsetIn,
           offsetOut);
    if (srcDims->MemLayout == Surface2D::Pitch)
        Printf(pri, "pitchIn: %d, ", srcDims->Pitch);
    else
        Printf(pri, "widthIn: %d, heightIn %d, ", srcDims->Width,
               srcDims->Height);

    if (dstDims->MemLayout == Surface2D::Pitch)
        Printf(pri, "pitchOut: %d, ", dstDims->Pitch);
    else
        Printf(pri, "widthOut: %d, heightOut %d, ", dstDims->Width,
               dstDims->Height);

    Printf(pri, "lengthIn: %d, lineCount: %d\n",
           lengthIn, lineCount);

    Printf(pri, "formatIn: %d, formatOut: %d\n", srcDims->format,
           dstDims->format);

    // Cause the (possible) other GPUs of the SLI device to ignore the
    // following methods.
    if (GetBoundGpuDevice()->GetNumSubdevices() > 1)
        m_pCh[id]->WriteSetSubdevice(m_SubdevMask);

    switch (m_DmaCopyAlloc[0].GetClass())
    {
        case GF100_DMA_COPY:
        case KEPLER_DMA_COPY_A:
        case MAXWELL_DMA_COPY_A:
        case PASCAL_DMA_COPY_A:
        case PASCAL_DMA_COPY_B:
        case VOLTA_DMA_COPY_A:
        case TURING_DMA_COPY_A:
        case AMPERE_DMA_COPY_A:
        case AMPERE_DMA_COPY_B:
        case HOPPER_DMA_COPY_A:
        case BLACKWELL_DMA_COPY_A:
            return Write90b5Methods(id, srcDims, dstDims,
                                    lengthIn, lineCount,
                                    offsetIn, offsetInUpper,
                                    offsetOut, offsetOutUpper,
                                    pri);
            break;

        default:
            // unsupported engine
            return RC::UNSUPPORTED_DEVICE;
            break;
    }
};

//-----------------------------------------------------------------------------

RC CpyEngTest::Write90b5Methods
(
    UINT32 id,
    SurfDimensions *srcDims,
    SurfDimensions *dstDims,
    UINT32 lengthIn,
    UINT32 lineCount,
    UINT32 offsetIn,
    UINT32 offsetInUpper,
    UINT32 offsetOut,
    UINT32 offsetOutUpper,
    UINT32 pri
)
{
    RC rc;
    UINT32 layoutMem = 0;
    UINT32 Class = m_DmaCopyAlloc[0].GetClass();

    if (Class == GF100_DMA_COPY)
    {
        // Set application for copy engine
        CHECK_RC(m_pCh[id]->Write
                 (sc_DmaCopy, LW90B5_SET_APPLICATION_ID,
                  DRF_DEF(90B5, _SET_APPLICATION_ID, _ID, _NORMAL)));
    }

    CHECK_RC(m_pCh[id]->Write(sc_DmaCopy, LW90B5_OFFSET_IN_UPPER,
        2,
        offsetInUpper, /* LW90B5_OFFSET_IN_UPPER */
        offsetIn)); /* LW90B5_OFFSET_IN_LOWER */

    CHECK_RC(m_pCh[id]->Write(sc_DmaCopy, LW90B5_OFFSET_OUT_UPPER,
        2,
        offsetOutUpper, /* LW90B5_OFFSET_OUT_UPPER */
        offsetOut)); /* LW90B5_OFFSET_OUT_LOWER */

    if (srcDims->MemLayout == Surface2D::Pitch)
        layoutMem |= DRF_DEF(90B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _PITCH);
    else
        layoutMem |= DRF_DEF(90B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT,
                             _BLOCKLINEAR);

    if (dstDims->MemLayout == Surface2D::Pitch)
        layoutMem |= DRF_DEF(90B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH);
    else
        layoutMem |= DRF_DEF(90B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT,
                             _BLOCKLINEAR);

    if (srcDims->MemLayout == Surface2D::Pitch)
        m_pCh[id]->Write(sc_DmaCopy, LW90B5_PITCH_IN,  srcDims->Pitch);
    else
    {
        Printf(pri, "Src GobHeight = %i\n", srcDims->GobHeight);
        if (srcDims->GobHeight == 8)
        {
            CHECK_RC(m_pCh[id]->Write(sc_DmaCopy, LW90B5_SET_SRC_BLOCK_SIZE,
                                      DRF_NUM(90B5, _SET_SRC_BLOCK_SIZE, _WIDTH,
                                              srcDims->BlockWidth) |
                                      DRF_NUM(90B5, _SET_SRC_BLOCK_SIZE, _HEIGHT,
                                              srcDims->BlockHeight) |
                                      DRF_NUM(90B5, _SET_SRC_BLOCK_SIZE, _DEPTH,
                                              srcDims->BlockDepth) |
                                      DRF_DEF(90B5, _SET_SRC_BLOCK_SIZE,
                                              _GOB_HEIGHT,
                                              _GOB_HEIGHT_FERMI_8)));
        }
        else
        {
            CHECK_RC(m_pCh[id]->Write(sc_DmaCopy, LW90B5_SET_SRC_BLOCK_SIZE,
                                      DRF_NUM(90B5, _SET_SRC_BLOCK_SIZE, _WIDTH,
                                              srcDims->BlockWidth) |
                                      DRF_NUM(90B5, _SET_SRC_BLOCK_SIZE, _HEIGHT,
                                              srcDims->BlockHeight) |
                                      DRF_NUM(90B5, _SET_SRC_BLOCK_SIZE, _DEPTH,
                                              srcDims->BlockDepth) |
                                      DRF_DEF(90B5, _SET_SRC_BLOCK_SIZE,
                                              _GOB_HEIGHT,
                                              _GOB_HEIGHT_TESLA_4)));

        }
        CHECK_RC(m_pCh[id]->Write(sc_DmaCopy, LW90B5_SET_SRC_DEPTH, 1));
        CHECK_RC(m_pCh[id]->Write(sc_DmaCopy, LW90B5_SET_SRC_LAYER, 0));
        CHECK_RC(m_pCh[id]->Write(sc_DmaCopy, LW90B5_SET_SRC_WIDTH,
                              srcDims->Width));
        CHECK_RC(m_pCh[id]->Write(sc_DmaCopy, LW90B5_SET_SRC_HEIGHT,
                              srcDims->Height));

        switch (Class)
        {
            case GF100_DMA_COPY:
            case KEPLER_DMA_COPY_A:
            case MAXWELL_DMA_COPY_A:
            case PASCAL_DMA_COPY_A:
                CHECK_RC(m_pCh[id]->Write(sc_DmaCopy, LW90B5_SET_SRC_ORIGIN,
                                      DRF_NUM(90B5, _SET_SRC_ORIGIN, _X,
                                              srcDims->OriginX) |
                                      DRF_NUM(90B5, _SET_SRC_ORIGIN, _Y,
                                              srcDims->OriginY)));
                break;
            default: // Volta+
            case PASCAL_DMA_COPY_B:
            case VOLTA_DMA_COPY_A:
            case TURING_DMA_COPY_A:
            case AMPERE_DMA_COPY_A:
            case AMPERE_DMA_COPY_B:
            case HOPPER_DMA_COPY_A:
            case BLACKWELL_DMA_COPY_A:
                CHECK_RC(m_pCh[id]->Write(sc_DmaCopy, LWC3B5_SRC_ORIGIN_X, 2,
                                      DRF_NUM(C3B5, _SRC_ORIGIN_X, _VALUE,
                                              srcDims->OriginX),
                                      DRF_NUM(C3B5, _SRC_ORIGIN_Y, _VALUE,
                                              srcDims->OriginY)));
                break;
        }
    }
    if (dstDims->MemLayout == Surface2D::Pitch)
        m_pCh[id]->Write(sc_DmaCopy, LW90B5_PITCH_OUT, dstDims->Pitch);
    else
    {
        Printf(pri, "Dst GobHeight = %i\n", dstDims->GobHeight);
        if (dstDims->GobHeight == 8)
        {
            CHECK_RC(m_pCh[id]->Write(sc_DmaCopy, LW90B5_SET_DST_BLOCK_SIZE,
                                      DRF_NUM(90B5, _SET_DST_BLOCK_SIZE, _WIDTH,
                                              dstDims->BlockWidth) |
                                      DRF_NUM(90B5, _SET_DST_BLOCK_SIZE, _HEIGHT,
                                              dstDims->BlockHeight) |
                                      DRF_NUM(90B5, _SET_DST_BLOCK_SIZE, _DEPTH,
                                              dstDims->BlockDepth) |
                                      DRF_DEF(90B5, _SET_DST_BLOCK_SIZE,
                                              _GOB_HEIGHT,
                                              _GOB_HEIGHT_FERMI_8)));
        }
        else
        {
            CHECK_RC(m_pCh[id]->Write(sc_DmaCopy, LW90B5_SET_DST_BLOCK_SIZE,
                                      DRF_NUM(90B5, _SET_DST_BLOCK_SIZE, _WIDTH,
                                              dstDims->BlockWidth) |
                                      DRF_NUM(90B5, _SET_DST_BLOCK_SIZE, _HEIGHT,
                                              dstDims->BlockHeight) |
                                      DRF_NUM(90B5, _SET_DST_BLOCK_SIZE, _DEPTH,
                                              dstDims->BlockDepth) |
                                      DRF_DEF(90B5, _SET_DST_BLOCK_SIZE,
                                              _GOB_HEIGHT,
                                              _GOB_HEIGHT_TESLA_4)));
        }
        CHECK_RC(m_pCh[id]->Write(sc_DmaCopy, LW90B5_SET_DST_DEPTH, 1));
        CHECK_RC(m_pCh[id]->Write(sc_DmaCopy, LW90B5_SET_DST_LAYER, 0));
        CHECK_RC(m_pCh[id]->Write(sc_DmaCopy, LW90B5_SET_DST_WIDTH,
                                  dstDims->Width));
        CHECK_RC(m_pCh[id]->Write(sc_DmaCopy, LW90B5_SET_DST_HEIGHT,
                                  dstDims->Height));

        switch (Class)
        {
            case GF100_DMA_COPY:
            case KEPLER_DMA_COPY_A:
            case MAXWELL_DMA_COPY_A:
            case PASCAL_DMA_COPY_A:
                CHECK_RC(m_pCh[id]->Write(sc_DmaCopy, LW90B5_SET_DST_ORIGIN,
                                      DRF_NUM(90B5, _SET_DST_ORIGIN, _X,
                                              dstDims->OriginX) |
                                      DRF_NUM(90B5, _SET_DST_ORIGIN, _Y,
                                              dstDims->OriginY)));
                break;
            default: // Volta+
            case PASCAL_DMA_COPY_B:
            case VOLTA_DMA_COPY_A:
            case TURING_DMA_COPY_A:
            case AMPERE_DMA_COPY_A:
            case AMPERE_DMA_COPY_B:
            case HOPPER_DMA_COPY_A:
            case BLACKWELL_DMA_COPY_A:
                CHECK_RC(m_pCh[id]->Write(sc_DmaCopy, LWC3B5_DST_ORIGIN_X, 2,
                                      DRF_NUM(C3B5, _DST_ORIGIN_X, _VALUE,
                                              dstDims->OriginX),
                                      DRF_NUM(C3B5, _DST_ORIGIN_Y, _VALUE,
                                              dstDims->OriginY)));
                break;
        }
    }

    if (Class == GF100_DMA_COPY)
    {
        UINT32 AddressMode = 0;
        AddressMode |= DRF_DEF(90B5, _ADDRESSING_MODE, _SRC_TYPE, _VIRTUAL);
        AddressMode |= DRF_DEF(90B5, _ADDRESSING_MODE, _DST_TYPE, _VIRTUAL);
        switch (srcDims->surf->GetLocation())
        {
            case Memory::Fb:
                AddressMode |= DRF_DEF(90B5, _ADDRESSING_MODE, _SRC_TARGET, _LOCAL_FB);
                break;

            case Memory::Coherent:
                AddressMode |= DRF_DEF(90B5, _ADDRESSING_MODE, _SRC_TARGET, _COHERENT_SYSMEM);
                break;

            case Memory::NonCoherent:
                AddressMode |= DRF_DEF(90B5, _ADDRESSING_MODE, _SRC_TARGET, _NONCOHERENT_SYSMEM);
                break;

            default:
                Printf(Tee::PriHigh,"Unsupported memory type for source\n");
                return RC::BAD_MEMLOC;
        }

        switch (dstDims->surf->GetLocation())
        {
            case Memory::Fb:
                AddressMode |= DRF_DEF(90B5, _ADDRESSING_MODE, _DST_TARGET, _LOCAL_FB);
                break;

            case Memory::Coherent:
                AddressMode |= DRF_DEF(90B5, _ADDRESSING_MODE, _DST_TARGET, _COHERENT_SYSMEM);
                break;

            case Memory::NonCoherent:
                AddressMode |= DRF_DEF(90B5, _ADDRESSING_MODE, _DST_TARGET, _NONCOHERENT_SYSMEM);
                break;

            default:
                Printf(Tee::PriHigh,"Unsupported memory type for destination\n");
                return RC::BAD_MEMLOC;
        }
        CHECK_RC(m_pCh[id]->Write(sc_DmaCopy, LW90B5_ADDRESSING_MODE, AddressMode));
    }
    else
    {
        layoutMem |= DRF_DEF(A0B5, _LAUNCH_DMA, _SRC_TYPE, _VIRTUAL);
        layoutMem |= DRF_DEF(A0B5, _LAUNCH_DMA, _DST_TYPE, _VIRTUAL);
    }

    CHECK_RC(m_pCh[id]->Write(sc_DmaCopy, LW90B5_LINE_LENGTH_IN, lengthIn));
    CHECK_RC(m_pCh[id]->Write(sc_DmaCopy, LW90B5_LINE_COUNT, lineCount));

    CHECK_RC(m_pCh[id]->Write(sc_DmaCopy,
        LW90B5_SET_SEMAPHORE_A,
        2,
        UINT32(m_Semaphore.GetCtxDmaOffsetGpu(id) >> 32), /* LW90B5_SET_SEMAPHORE_A */
        UINT32(m_Semaphore.GetCtxDmaOffsetGpu(id)))); /* LW90B5_SET_SEMAPHORE_B */

    // Only need to flush cache on GPU/CheetAh surfaces with GPU caching
    // enabled to ensure that CPU and CheetAh will both see the correctly
    // modified GPU data
    const bool bCacheFlushReqd = GetBoundGpuSubdevice()->IsSOC();

    // Memory flush operations do not wait until the engine is idle so it is
    // necessary to perform them after the SemaphoreRelease (which is done with
    // WFI enabled)
    //
    // Do a fake release here to cause the engine (need to use 0 so that there
    // is no chance of this unblocking a Wait operation)
    CHECK_RC(m_pCh[id]->Write(sc_DmaCopy, LW90B5_SET_SEMAPHORE_PAYLOAD,
                          bCacheFlushReqd ? 0 : Payload));

    // Use NON_PIPELINED to WAR hw bug 618956.
    CHECK_RC(m_pCh[id]->Write(sc_DmaCopy, LW90B5_LAUNCH_DMA,
                              layoutMem |
                              DRF_DEF(90B5, _LAUNCH_DMA, _SEMAPHORE_TYPE,
                                      _RELEASE_ONE_WORD_SEMAPHORE) |
                              DRF_DEF(90B5, _LAUNCH_DMA, _INTERRUPT_TYPE, _NONE) |
                              DRF_DEF(90B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE) |
                              DRF_DEF(90B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE,
                                      _NON_PIPELINED) |
                              DRF_DEF(90B5, _LAUNCH_DMA, _MULTI_LINE_ENABLE,
                                      _TRUE)));

    if (bCacheFlushReqd)
    {
        // At this point we would do a sysmembar flush, but it's not necessary
        // because the LAUNCH_DMA method above has FLUSH_ENABLE_TRUE.

        // Host semaphore release with WFI will wait for previous operations
        // to complete. We need that before doing an L2 flush.
        CHECK_RC(m_pCh[id]->SetSemaphoreOffset(m_Semaphore.GetCtxDmaOffsetGpu(id)));
        CHECK_RC(m_pCh[id]->SemaphoreRelease(0));
        CHECK_RC(m_pCh[id]->WriteL2FlushDirty());
        CHECK_RC(m_pCh[id]->SemaphoreRelease(Payload));
    }

    return OK;
}

//-----------------------------------------------------------------------------

bool CpyEngTest::ExcludeRanges
(
    SurfDimensions *srcDims,
    SurfDimensions *dstDims,
    UINT32 lineCount,
    UINT32 lengthIn
)
{
    UINT32 excludeSize;
    if (lineCount == 72)
        excludeSize = 0;
    if (dstDims->MemLayout ==  Surface2D::Pitch)
    {
        UINT32 excludeWidth;
        if (lineCount == 1)
            excludeWidth = lengthIn; // Only exclude what is copied
        else
            excludeWidth = dstDims->Pitch;
        excludeSize = excludeWidth * lineCount;
    }
    else
        excludeSize = dstDims->Width * dstDims->Height;

    if (!m_mapDstPicker[dstDims->surf].ExcludeRange(
            dstDims->offsetOrig,
            dstDims->offsetOrig + excludeSize))
        return false;

    if (srcDims->MemLayout ==  Surface2D::Pitch)
    {
        UINT32 excludeWidth;
        if (lineCount == 1)
            excludeWidth = lengthIn; // Only exclude what is copied
        else
            excludeWidth = srcDims->Pitch;
        excludeSize = excludeWidth * lineCount;
    }
    else
        excludeSize = srcDims->Width * srcDims->Height;

    if (!m_mapDstPicker[srcDims->surf].ExcludeRange(
        srcDims->offsetOrig,
        srcDims->offsetOrig + excludeSize))
        return false;
    return true;
}

//-----------------------------------------------------------------------------

void CpyEngTest::SetSrcAndDstRanges
(
    SurfDimensions *srcDims,
    SurfDimensions *dstDims,
    UINT32 *srcStart,
    UINT32 *srcEnd,
    UINT32 *dstStart,
    UINT32 *dstEnd
)
{
    if (srcDims->surf == dstDims->surf)
    {
        m_mapDstPicker[srcDims->surf].PickTwoFromSameRange(
            srcStart, srcEnd, dstStart, dstEnd);
    }
    else
    {
        m_mapDstPicker[srcDims->surf].Pick(srcStart, srcEnd);
        m_mapDstPicker[dstDims->surf].Pick(dstStart, dstEnd);
    }
}

//-----------------------------------------------------------------------------

bool CpyEngTest::SetupSurfaceDims
(
    SurfDimensions *srcDims,
    SurfDimensions *dstDims,
    UINT32 *lCount,
    UINT32 *length,
    UINT32 pri
)
{
    UINT32 gobWidth = GetBoundGpuDevice()->GobWidth();
    UINT32 gobHeight = GetBoundGpuDevice()->GobHeight();

    switch (m_DmaCopyAlloc[0].GetClass())
    {
        // Pascal+ only support a single gob height
        case PASCAL_DMA_COPY_A:
        case PASCAL_DMA_COPY_B:
        case VOLTA_DMA_COPY_A:
        case TURING_DMA_COPY_A:
        case AMPERE_DMA_COPY_A:
        case AMPERE_DMA_COPY_B:
        case HOPPER_DMA_COPY_A:
        case BLACKWELL_DMA_COPY_A:
            break;

        case GF100_DMA_COPY:
        case KEPLER_DMA_COPY_A:
        case MAXWELL_DMA_COPY_A:
            // Allow for testing different gob heights (Tesla_4 or Fermi_8)
            gobHeight = (*m_pPickers)[FPK_COPYENG_GOB_HEIGHT].Pick();
            break;

        default:
            // unsupported engine
            return false;
    }
    srcDims->GobHeight = gobHeight;
    dstDims->GobHeight = gobHeight;

    Printf(pri, "Setting gobheights to %i\n",gobHeight);

    UINT32 srcStart, srcEnd, dstStart, dstEnd, srcSize, dstSize;
    // Get ranges for surfaces; use for setting sizes and offsets

    SetSrcAndDstRanges(srcDims, dstDims, &srcStart,
                       &srcEnd, &dstStart, &dstEnd);

    srcSize = srcEnd - srcStart;
    dstSize = dstEnd - dstStart;

    // FORMAT_IN
    if (srcDims->MemLayout == Surface2D::BlockLinear)
        srcDims->format = 1;
    else
        srcDims->format = (*m_pPickers)[FPK_COPYENG_FORMAT_IN].Pick();
    if (m_CheckInputForError) FixFormat(&srcDims->format);

    // FORMAT_OUT
    if (dstDims->MemLayout == Surface2D::BlockLinear)
        dstDims->format = 1;
    else
        dstDims->format = (*m_pPickers)[FPK_COPYENG_FORMAT_OUT].Pick();
    if (m_CheckInputForError)
        FixFormat(&dstDims->format);

    // Set lMaxYLines to the maximum allowable height depending on
    // dstDims->BlockHeight and srcDims->BlockHeight
    UINT32 lMaxYLines = (UINT32) m_MaxYLines;

    if (srcDims->MemLayout == Surface2D::BlockLinear)
    {
        srcDims->BlockWidth = GetBlockWidth(FPK_COPYENG_SRC_BLOCK_SIZE_WIDTH);
        srcDims->BlockHeight =
            (*m_pPickers)[FPK_COPYENG_SRC_BLOCK_SIZE_HEIGHT].Pick();
        srcDims->BlockDepth =
            (*m_pPickers)[FPK_COPYENG_SRC_BLOCK_SIZE_DEPTH].Pick();
        UINT32 srcHeightStep = gobHeight << srcDims->BlockHeight;
        lMaxYLines = min(lMaxYLines,
                         ((lMaxYLines/srcHeightStep)*srcHeightStep));

    }

    if (dstDims->MemLayout == Surface2D::BlockLinear)
    {
        dstDims->BlockWidth = GetBlockWidth(FPK_COPYENG_DST_BLOCK_SIZE_WIDTH);
        dstDims->BlockHeight =
            (*m_pPickers)[FPK_COPYENG_DST_BLOCK_SIZE_HEIGHT].Pick();
        dstDims->BlockDepth =
            (*m_pPickers)[FPK_COPYENG_DST_BLOCK_SIZE_DEPTH].Pick();
        UINT32 dstHeightStep = gobHeight << dstDims->BlockHeight;
        lMaxYLines = min(lMaxYLines,
                         ((lMaxYLines/dstHeightStep)*dstHeightStep));
    }

    // lineBytes is used to calc LINE_LENGTH_IN
    UINT32 lineBytes;
    UINT32 minBytes = max(srcDims->format, dstDims->format);

    if ((*m_pPickers)[FPK_COPYENG_LINE_BYTES].WasSetFromJs())
    {
        lineBytes = (*m_pPickers)[FPK_COPYENG_LINE_BYTES].Pick();
        if (m_CheckInputForError)
        {
            if (lineBytes < minBytes)
            {
                Printf(pri, "Line bytes %d lower than min %d, clamping to min\n",
                       lineBytes, minBytes);
                lineBytes = minBytes;
            }
            else if (lineBytes > m_MaxXBytes)
            {
                Printf(pri,
                       "Line bytes %d greater than max %d, clamping to max\n",
                       lineBytes, m_MaxXBytes);
                lineBytes = m_MaxXBytes;
            }
        }
    }
    else
    {
        lineBytes = max(srcDims->format, dstDims->format);
        lineBytes = max(lineBytes, m_pFpCtx->Rand.GetRandom(1, m_MaxXBytes));
    }

    // LINE_LENGTH_IN
    UINT32 lengthIn = lineBytes / max(srcDims->format, dstDims->format);

    // LINE_COUNT
    UINT32 lineCount = (*m_pPickers)[FPK_COPYENG_LINE_COUNT].Pick();
    if (lineCount > lMaxYLines)
    {
        Printf(pri, "Line Count %d greater than max %d, clamping to max.\n"
               , lineCount, lMaxYLines);
        lineCount = lMaxYLines;
    }

    UINT32 minInPitch = lengthIn*srcDims->format;

    if (!SetSurfSizes(srcDims,
                      minInPitch,
                      gobWidth,
                      gobHeight,
                      &lineCount,
                      FPK_COPYENG_PITCH_IN,
                      FPK_COPYENG_SRC_WIDTH,
                      FPK_COPYENG_SRC_HEIGHT,
                      lMaxYLines,
                      srcSize,
                      pri))
        return false; // Failed to fit block region in space allowed; abort

    // DST_WIDTH

    UINT32 minOutPitch = max(lengthIn * dstDims->format,
                             (UINT32)srcDims->Width);

    if (!SetSurfSizes(dstDims,
                      minOutPitch,
                      gobWidth,
                      gobHeight,
                      &lineCount,
                      FPK_COPYENG_PITCH_OUT,
                      FPK_COPYENG_DST_WIDTH,
                      FPK_COPYENG_DST_HEIGHT,
                      lMaxYLines,
                      dstSize,
                      pri))
        return false; // Failed to fit block region in space allowed; abort

    // Resize lengthIn and lineCount for source and destination sizes
    AdjustCopyDims(srcDims, dstDims, &lineCount, &lengthIn, srcStart,
                   srcEnd, dstStart, dstEnd);

    SetSurfOffset(srcDims, srcStart, srcEnd, lineCount, lengthIn,
                  FPK_COPYENG_OFFSET_IN, FPK_COPYENG_SRC_ORIGIN_X,
                  FPK_COPYENG_SRC_ORIGIN_Y, pri);

    SetSurfOffset(dstDims, dstStart, dstEnd, lineCount, lengthIn,
                  FPK_COPYENG_OFFSET_OUT, FPK_COPYENG_DST_ORIGIN_X,
                  FPK_COPYENG_DST_ORIGIN_Y, pri);

    // Recallwlate lengthIn and lineCount with new offsets
    *lCount = lineCount;
    *length = lengthIn;
    AdjustCopyDims(srcDims, dstDims, lCount, length, srcDims->offsetOrig,
                   srcEnd, dstDims->offsetOrig, dstEnd);

    if (!lineCount || !lengthIn)
        return false;

    // Must exclude source and dest ranges
    if (!ExcludeRanges(srcDims, dstDims, lineCount, lengthIn))
        return false;  // Ranges not valid; abort
    return true;
}

//-----------------------------------------------------------------------------

bool CpyEngTest::SetSurfSizes
(
    SurfDimensions *Dims,
    UINT32 minPitch,
    UINT32 gobWidth,
    UINT32 gobHeight,
    UINT32 *lineCt,
    UINT32 FpkPitch,
    UINT32 FpkWidth,
    UINT32 FpkHeight,
    UINT32 lMaxYLines,
    UINT32 Size,
    UINT32 pri
)
{
    UINT32 lineCount = *lineCt;
    if (Dims->MemLayout == Surface2D::Pitch)
    {
        if ((*m_pPickers)[FpkPitch].WasSetFromJs())
        {
            Dims->Pitch = (*m_pPickers)[FpkPitch].Pick();
            if (m_CheckInputForError)
            {
                if (Dims->Pitch < minPitch)
                {
                    Printf(pri,
                           "Pitch Out %d less than min %d, clamping to min\n",
                           Dims->Pitch, minPitch);
                    Dims->Pitch = minPitch;
                }
                else if (Dims->Pitch > m_MaxXBytes)
                {
                    Printf(pri,
                           "Pitch In %d greater than max %d, clamping to max\n",
                           Dims->Pitch, m_MaxXBytes);
                    Dims->Pitch = m_MaxXBytes;
                }

            }

        }
        else
        {
            Dims->Pitch = m_pFpCtx->Rand.GetRandom(minPitch, m_MaxXBytes);
        }
        ForcePitchEvenIfApplicable(FpkPitch, &Dims->Pitch,
                                   minPitch);
        lineCount = min(lineCount, Size/Dims->Pitch);
    }
    //BLOCK_LINEAR
    else
    {
        if ((*m_pPickers)[FpkWidth].WasSetFromJs())
        {
            Dims->Width = (*m_pPickers)[FpkWidth].Pick();
            if (m_CheckInputForError)
            {
                Dims->Width = ALIGN_UP(Dims->Width,
                                       gobWidth << Dims->BlockWidth);

                if (Dims->Width < minPitch)
                {
                    Printf(pri,
                           "Dst Width %d less than min %d, clamping to min\n",
                           Dims->Width, minPitch);
                    Dims->Width =
                        ALIGN_UP(minPitch, gobWidth << Dims->BlockWidth);
                }
                else if (Dims->Width > m_MaxXBytes)
                {
                    Printf(pri,
                           "Dst Width %d greater than max %d, clamping to max\n",
                           Dims->Width, m_MaxXBytes);
                    Dims->Width =
                        ALIGN_DOWN(m_MaxXBytes, gobWidth << Dims->BlockWidth);
                }

            }
        }
        else
        {
            Dims->Width = m_pFpCtx->Rand.GetRandom(minPitch, m_MaxXBytes);
            Dims->Width = ALIGN_UP(Dims->Width,
                                   gobWidth << Dims->BlockWidth);
            UINT32 maxWidth = min(m_MaxXBytes, Size);
            while (Dims->Width > maxWidth)
                Dims->Width -= gobWidth << Dims->BlockWidth;
            if (Dims->Width == 0)
                return false;
                // Failed to fit block region in space allowed; abort
        }

        if ((*m_pPickers)[FpkHeight].WasSetFromJs())
        {
            Dims->Height = (*m_pPickers)[FpkHeight].Pick();
            if (m_CheckInputForError)
            {
                Dims->Height = ALIGN_UP(Dims->Height,
                                        gobHeight << Dims->BlockHeight);
                if (Dims->Height < lineCount)
                {
                    Printf(pri,
                           "Dst Height %d less than min %d, clamping to min\n",
                           Dims->Height, lineCount);
                    Dims->Height =
                        ALIGN_UP(lineCount, gobHeight << Dims->BlockHeight);
                }
                else if (Dims->Height > lMaxYLines)
                {
                    Printf(pri,
                           "Dst Height %d greater than max %d, clamping to max\n",
                           Dims->Height, lMaxYLines);
                    Dims->Height =
                        ALIGN_DOWN(lMaxYLines, gobHeight << Dims->BlockHeight);
                }
            }
        }
        else
        {
            Dims->Height = m_pFpCtx->Rand.GetRandom(lineCount, lMaxYLines);
            Dims->Height = ALIGN_UP(Dims->Height,
                                    gobHeight << Dims->BlockHeight);
            UINT32 maxHeight = min(lMaxYLines, Size/Dims->Width);
            while (Dims->Height > maxHeight)
                Dims->Height -= (gobHeight << Dims->BlockHeight);
            if (Dims->Height == 0)
                return false;
                // Failed to fit block region in space allowed; abort
            if (Dims->Height < lineCount)
                lineCount = Dims->Height;
        }
        lineCount = min(lineCount, Size/Dims->Width);
    }
    *lineCt = lineCount;
    return true;
}

//-----------------------------------------------------------------------------

void CpyEngTest::AdjustCopyDims
(
    SurfDimensions *srcDims,
    SurfDimensions *dstDims,
    UINT32 *lCount,
    UINT32 *length,
    UINT32 srcStart,
    UINT32 srcEnd,
    UINT32 dstStart,
    UINT32 dstEnd
)
{
    UINT32 lineCount = *lCount;
    UINT32 lengthIn = *length;

    UINT32 dstSize = dstEnd - dstStart;
    UINT32 srcSize = srcEnd - srcStart;

    UINT32 minWidth;
    if (srcDims->MemLayout == Surface2D::BlockLinear)
        minWidth = srcDims->Width;
    else
        minWidth = srcDims->Pitch;

    if (dstDims->MemLayout == Surface2D::BlockLinear)
        minWidth = min(minWidth, dstDims->Width);
    else
        minWidth = min(minWidth, dstDims->Pitch);

    UINT32 copySize = lineCount * minWidth;
    UINT32 minSize = min(dstSize, srcSize);  // min In bytes, not bits
    if ( copySize > minSize)
    {
        lineCount = (minSize/m_MinLineWidthInBytes);
        if (lineCount == 0)
        {
            lineCount = 1;
            lengthIn = min(minSize, lengthIn);
        }
    }

    if (lineCount == 0)
    {
        lineCount = 1;
        lengthIn = min(minSize, lengthIn);
    }

    *lCount = lineCount;
    *length = lengthIn;
}

//-----------------------------------------------------------------------------

void CpyEngTest::SetSurfOffset
(
    SurfDimensions *Dims,
    UINT32 minOffset,
    UINT32 endOffset,
    UINT32 lineCount,
    UINT32 lengthIn,
    UINT32 FpkOffset,
    UINT32 FpkOrigX,
    UINT32 FpkOrigY,
    UINT32 pri
)
{

    UINT32 maxOffset;
    if (Dims->MemLayout == Surface2D::BlockLinear)
        maxOffset = endOffset - (Dims->Width * lineCount);
    else
        if (lineCount == 1)
            maxOffset = endOffset - (lengthIn * lineCount);
        else
            maxOffset = endOffset - (Dims->Pitch * lineCount);
    // The minimum for maxOffset should be minOffset
    if (maxOffset < minOffset)
        maxOffset = minOffset;

    if (Dims->MemLayout == Surface2D::Pitch)
    {
        if ((*m_pPickers)[FpkOffset].WasSetFromJs())
        {
            Dims->offsetOrig = (*m_pPickers)[FpkOffset].Pick();
            if (m_CheckInputForError)
            {
                if (Dims->offsetOrig > maxOffset)
                {
                    Printf(pri,
                           "Offset In %d greater than max %d, clamping to max\n",
                           Dims->offsetOrig, maxOffset);
                    Dims->offsetOrig = maxOffset;
                }
            }
        }
        else
        {
            Dims->offsetOrig = m_pFpCtx->Rand.GetRandom(minOffset, maxOffset);
        }
    }
    //BLOCKLINEAR
    else
    {
        if ((*m_pPickers)[FpkOffset].WasSetFromJs())
            Dims->offsetOrig = (*m_pPickers)[FpkOffset].Pick();
        else
            Dims->offsetOrig = minOffset;
        if ((*m_pPickers)[FpkOrigX].WasSetFromJs())
        {
            Dims->OriginX = (*m_pPickers)[FpkOrigX].Pick();
            if (m_CheckInputForError)
            {
                if (Dims->Width - lengthIn < Dims->OriginX)
                {
                    Printf(pri,
                           "Src Origin X %d > than max %d, clamping to max\n",
                           Dims->OriginX, Dims->Width - lengthIn);
                    Dims->OriginX = Dims->Width - lengthIn;
                }
            }
        }
        else
        {
            Dims->OriginX = 0;
        }
        if ((*m_pPickers)[FpkOrigY].WasSetFromJs())
        {
            Dims->OriginY = (*m_pPickers)[FpkOrigY].Pick();
            if (m_CheckInputForError)
            {
                if (Dims->Height - lineCount < Dims->OriginY)
                {
                    Printf(pri,
                           "Src Origin Y %d > than max %d, clamping to max\n",
                           Dims->OriginY, Dims->Height - lineCount);
                    Dims->OriginY = Dims->Height - lineCount;
                }
            }
        }
        else
        {
            Dims->OriginY = 0;
        }
    }
}

//-----------------------------------------------------------------------------

RC CpyEngTest::PerformAllChecks
(
    const vector<UINT32>& engineList,
    UINT32 pri
)
{
    StickyRC rc;
    Tee::Priority detailed = (Tee::Priority)m_DetailedPri;
    bool ErrorInThisLoop = false;

    //
    // Check the results of the copies
    //
    for (UINT32 engNum = 0; engNum < engineList.size(); engNum++)
    {
        const UINT32 engIdx = LW2080_ENGINE_TYPE_COPY_IDX(engineList[engNum]);
        // If Either region was not setup correcly, then don't test
        if (m_srcDims[engNum].skipRun || m_dstDims[engNum].skipRun)
            continue;
        if (!(m_EngMask & (1 << engIdx)))
            continue;
        rc = SetupAndCallCheck(engNum,
                               &m_srcDims[engNum], &m_dstDims[engNum],
                               m_testParams[engNum].lineCount,
                               m_testParams[engNum].lengthIn,
                               &ErrorInThisLoop,
                               detailed, pri);

        if (ErrorInThisLoop)
        {
            ++m_NumLoopsWithError;
            pri = Tee::PriHigh;
        }

        Printf(pri,
               "loop  engNum src->dst (fmt pitch off lineBytes lineCount) INPUT\n"
               "                      (fmt pitch off)                     OUTPUT\n"
               "=========================================================\n");
        Printf(pri,
               "%-5d %-6d %s->%s   (%1x %6x %6x %4x %4x)\n"
               "                           (%1x %6x %6x)\n",
               m_pFpCtx->LoopNum,
               engNum,
               strLoc[m_srcDims[engNum].surf->GetLocation()],
               strLoc[m_dstDims[engNum].surf->GetLocation()],
               m_srcDims[engNum].format, m_srcDims[engNum].Pitch,
               m_srcDims[engNum].offsetOrig,
               m_testParams[engNum].lengthIn, m_testParams[engNum].lineCount,
               m_dstDims[engNum].format, m_dstDims[engNum].Pitch,
               m_dstDims[engNum].offsetOrig);

        Printf(detailed, "\nSource was:\n");
        m_srcDims[engNum].surf->Print(detailed, "SRC: ");

        Printf(detailed, "\nDestination was:\n");
        m_dstDims[engNum].surf->Print(detailed, "DST: ");
        Printf(detailed, "\n");
    }
    return rc;
}

//-----------------------------------------------------------------------------

RC CpyEngTest::SetupAndCallCheck
(
    UINT32 id,
    SurfDimensions *srcDims,
    SurfDimensions *dstDims,
    UINT32 lineCount,
    UINT32 lengthIn,
    bool *ErrorInThisLoop,
    Tee::Priority detailed,
    UINT32 pri
)
{
    RC rc;
    // Don't transfer destination memory to coherent memory if
    // source is already there.  This may cause overwriting of
    // of source data.

    Surface2D *tmpSrc = srcDims->surf;
    Surface2D *tmpDst = dstDims->surf;
    bool opt = false;

    if (srcDims->surf->GetLocation() == Memory::Fb)
    {
        SurfDimensions tmpDims;
        opt = true;

        Platform::MemCopy(&tmpDims, srcDims, sizeof(SurfDimensions));
        tmpSrc = &m_SrcCheckSurf;
        m_SrcCheckSurf.SetLayout(srcDims->surf->GetLayout());
        CHECK_RC(tmpSrc->BindGpuChannel(m_hCh[id]));
        tmpDims.surf = tmpSrc;

        m_Semaphore.SetOnePayload(id,0);

        CHECK_RC(WriteMethods(id, srcDims, &tmpDims,
                              lengthIn, lineCount, pri));

        m_pCh[id]->Flush();
        CHECK_RC(m_Semaphore.Wait(m_pTstCfg->TimeoutMs()));
        Printf(detailed, "offset: %x, pitchOut %x, len %x, f %x\n",
               tmpDims.offsetOrig, srcDims->Pitch, lengthIn, srcDims->format);
        Printf(detailed, "l*f %x, lineCount, %x\n",
               lengthIn * srcDims->format, lineCount);

    }

    // If the destination was in the FB, copy it to the fastest system
    // memory possible
    if (dstDims->surf->GetLocation() == Memory::Fb)
    {
        SurfDimensions tmpDims;
        opt = true;

        Platform::MemCopy(&tmpDims, dstDims, sizeof(SurfDimensions));
        tmpDst = &m_DstCheckSurf;
        CHECK_RC(tmpDst->BindGpuChannel(m_hCh[id]));
        m_DstCheckSurf.SetLayout(dstDims->surf->GetLayout());
        tmpDims.surf = tmpDst;

        m_Semaphore.SetOnePayload(id,0);

        CHECK_RC(WriteMethods(id, dstDims, &tmpDims,
                              lengthIn, lineCount, pri));

        m_pCh[id]->Flush();
        CHECK_RC(m_Semaphore.Wait(m_pTstCfg->TimeoutMs()));
        Printf(detailed, "offset: %x, pitchOut %x, len %x, f %x\n",
               tmpDims.offsetOrig, dstDims->Pitch, lengthIn, dstDims->format);
        Printf(detailed, "l*f %x, lineCount, %x\n",
               lengthIn * dstDims->format, lineCount);

    }

    // If we have something to optimize, do this Check, otherwise fall
    // through to regular case without complaining
    if (opt)
    {
        Tee::Priority fallback = Tee::PriHigh;
        if (Xp::GetOperatingSystem() == Xp::OS_MAC ||
            Xp::GetOperatingSystem() == Xp::OS_MACRM)
        {
            // Expected to fail sometimes on MacOSX, dues to
            // an endian bug in Class039
            // Don't print verbosely.
            fallback = (Tee::Priority)pri;
        }

        if (OK != (rc = CheckResult(id,
                                    (UINT08 *)tmpSrc->GetAddress(),
                                    (UINT08 *)tmpDst->GetAddress(),
                                    srcDims,
                                    dstDims,
                                    lengthIn, lineCount, fallback,
                                    ErrorInThisLoop)))
        {
            Printf(fallback, "DMA Test fast compare FAILED.\n");
            Printf(fallback, "\tFalling through to slower compare.\n");
            pri = fallback;
            detailed = (Tee::Priority)pri;
        }
        else
        {
            Printf(detailed, "Fast compare worked for this loop\n");
            return OK;
        }
    }

    Printf(detailed, "Checking results for engine instance %i\n", id);
    if (OK != (rc = CheckResult(id,
                                (UINT08 *)srcDims->surf->GetAddress(),
                                ((UINT08 *)dstDims->surf->GetAddress()),
                                srcDims, dstDims,
                                lengthIn, lineCount, Tee::PriHigh,
                                ErrorInThisLoop)))
    {
        pri =  Tee::PriHigh;
        Printf(pri, "DMA Test FAILED.\n");
        return rc;
    }
    else
    {
        Printf(Tee::PriDebug, "CheckResult completed; no errors\n");
    }
    return OK;
}

//-----------------------------------------------------------------------------

RC CpyEngTest::CheckResult
(
    UINT32 id,
    UINT08 * AddressIn,
    UINT08 * AddressOut,
    SurfDimensions *srcDims,
    SurfDimensions *dstDims,
    UINT32 LengthIn,
    UINT32 LineCount,
    Tee::Priority pri,
    bool *ErrorInThisLoop
)
{
    INT32 x, y;
    UINT08 *AddIn, *AddOut;
    UINT08 Src, Dst;
    BlockLinear blSrc(srcDims->Width, srcDims->Height,
                      srcDims->BlockWidth, srcDims->BlockHeight,
                      0, 1, GetBoundGpuDevice());
    BlockLinear blDst(dstDims->Width, dstDims->Height,
                      dstDims->BlockWidth, dstDims->BlockHeight,
                      0, 1, GetBoundGpuDevice());
    blSrc.SetLog2BlockHeightGob(srcDims->GobHeight);
    blDst.SetLog2BlockHeightGob(dstDims->GobHeight);

    *ErrorInThisLoop = false;

    for (y = 0; y < (INT32) LineCount; ++y)
    {
        for (x = 0; x < (INT32) LengthIn; ++x)
        {
            if (srcDims->MemLayout == Surface2D::Pitch)
                AddIn = AddressIn + srcDims->offsetOrig +
                    y * srcDims->Pitch + x;
            else
                AddIn = AddressIn + srcDims->offsetOrig +
                    blSrc.Address(x, y, 0);
            if (dstDims->MemLayout == Surface2D::Pitch)
                AddOut = AddressOut + dstDims->offsetOrig +
                    y * dstDims->Pitch + x;
            else
                AddOut = AddressOut + dstDims->offsetOrig +
                    blDst.Address(x, y, 0);

            Src = MEM_RD08(AddIn);
            // Conform to the hardware polling requirement requested by
            // "-poll_hw_hz" (i.e. HW should not be accessed faster than a
            // certain rate).  By default this will not sleep or yield unless
            // the command line argument is present.
            Tasker::PollMemSleep();

            Dst = MEM_RD08(AddOut);
            if (Dst != Src)
            {
                *ErrorInThisLoop = true;
                UINT08 * AddInBack  = (AddIn -  (3 & (size_t)AddIn));
                // round address back to 8-byte aligned
                UINT08 * AddOutBack = (AddOut - (3 & (size_t)AddOut));
                // round address back to 8-byte aligned

                Printf(
                    pri,
                    "Miscompare at line %d of %d, byte %d of %d of copy %d\n",
                    y, LineCount, x, LengthIn, id);

                // I'm pretty sure this is only true in controlled situations,
                // like dst offset (and src?) 16 byte aligned etc.

                Printf(pri,
                       "source [%p]=%02x ( [%p]=",
                       AddIn, Src,
                       AddInBack);

                UINT32 b;

                if (m_DebuggingOnLA)
                {
                    for (b = 0; b < LengthIn; ++b)
                    {
                        if (b % 8 == 0) Printf(pri, "\n");
                        Printf(pri, " %02x", MEM_RD08(&AddInBack[b]) );
                        Tasker::PollMemSleep();
                    }
                }
                else
                {
                    for (b = 0; b < 8; ++b)
                    {
                        Printf(pri, " %02x", MEM_RD08(&AddInBack[b]) );
                    }
                }

                Printf(pri, " )\n");

                Printf(pri,
                       "destin [%p]=%02x ( [%p]=",
                       AddOut, Dst,
                       AddOutBack);

                if (m_DebuggingOnLA)
                {
                    for (b = 0; b < LengthIn; ++b)
                    {
                        if (b % 8 == 0) Printf(pri, "\n");
                        Printf(pri, " %02x", MEM_RD08(&AddOutBack[b]) );
                        Tasker::PollMemSleep();
                    }
                }
                else
                {
                    for (b = 0; b < 8; ++b)
                    {
                        Printf(pri, " %02x", MEM_RD08(&AddOutBack[b]) );
                    }
                }

                Printf(pri, " )\n");

                if (m_DumpChunks != 0)
                {
                    size_t addrMask = m_DumpChunks - 1;
                    UINT08 *lwrChunk = (UINT08 *)((size_t)AddOut & (~addrMask));
                    UINT08 *prevChunk = (lwrChunk - m_DumpChunks);
                    UINT08 *nextChunk = (lwrChunk + m_DumpChunks);

                    if (prevChunk >= AddressOut)
                    {
                        DumpOneChunk(pri, prevChunk, m_DumpChunks);
                    }
                    else
                    {
                        Printf(
                            pri,
                            "Cannot print previous 16 bytes chunk, start of copy is 0x%08x\n",
                            Platform::VirtualToPhysical32(AddressOut));
                    }

                    // TODO: These might go past the end of dst as
                    // they are lwrrently written
                    DumpOneChunk(pri, lwrChunk, m_DumpChunks);
                    DumpOneChunk(pri, nextChunk, m_DumpChunks);
                }

                if (m_LogErrors)
                {
                    ErrorInfo lwr;
                    lwr.virtAddr = AddOut;
                    lwr.expectedVal = Src;
                    lwr.actualVal = Dst;

                    ErrIterator found = m_ErrorLog.find(lwr);
                    if (found != m_ErrorLog.end())
                    {
                        found->second++;
                    }
                    else
                    {
                        m_ErrorLog[lwr] = 1;
                    }
                }
                else
                {
                    // Not logging errors, so bail on the first error we find
                    return RC::MEM_TO_MEM_RESULT_NOT_MATCH;
                }
            }
        }
    }
    return OK;
}

//------------------------------------------------------------------------------

RC CpyEngTest::PrintResults
(
    UINT08 * AddressIn,
    SurfDimensions *srcDims,
    UINT32 LengthIn,
    UINT32 LineCount,
    UINT32 idNum
)
{
    RC rc;
    INT32 x, y;
    UINT08 *AddIn;
    UINT32 Src;
    FILE *file;
    char Filename[32];
    sprintf(Filename, "debugout%i.txt",idNum);
    CHECK_RC(Utility::OpenFile(Filename, &file, "w"));
    BlockLinear blSrc(srcDims->Width, srcDims->Height,
                      srcDims->BlockWidth, srcDims->BlockHeight,
                      0, 1, GetBoundGpuDevice());

    for (y = 0; y < (INT32) LineCount; ++y)
    {
        UINT32 count = 0;
        for (x = 0; x < (INT32) LengthIn; x=x+4)
        {
            if (srcDims->MemLayout == Surface2D::Pitch)
                AddIn = AddressIn  + srcDims->offsetOrig  +
                    y * srcDims->Pitch  + x;
            else
                AddIn = AddressIn + srcDims->offsetOrig +
                    blSrc.Address(x, y, 0);
            Src = MEM_RD08(AddIn);
            fprintf(file,"%02x ",Src);
            if (++count == 16)
            {
                count = 0;
                fprintf(file,"\n");
            }
        }
        fprintf(file,"\n ***** y = %x *****\n", y);
    }
    fclose(file);
    return rc;
}

//------------------------------------------------------------------------------

static void DumpOneChunk(Tee::Priority pri, UINT08 *addr, UINT32 chunkSize)
{
    Printf(pri, "Data at phys addr 0x%08x:",
           Platform::VirtualToPhysical32(addr));

    for (UINT32 b = 0; b < chunkSize; ++b)
    {
        if (b % 8 == 0) Printf(pri, "\n");
        Printf(pri, " %02x", MEM_RD08(&addr[b]) );
        Tasker::PollMemSleep();
    }
    Printf(pri, "\n");
}

//-----------------------------------------------------------------------------

UINT32 CpyEngTest::ValidatePick(UINT32 pick)
{
    // Find a value we can return if pick isn't valid
    //
    UINT32 validPick = NUM_SURFACES - 1;
    while ((validPick > 0) && !m_pSurfaces[validPick].IsAllocated())
    {
        --validPick;
    }

    // If we picked an out-of-range value, default to validPick
    //
    if (pick >= NUM_SURFACES)
    {
        Printf(Tee::PriNormal,
               "WARNING:  FancyPicker returned pick %d > boundary %d\n"
               , pick, NUM_SURFACES - 1);
        MASSERT(false);
        return validPick;
    }

    // If the pick isn't supported, find an alternative
    //
    if (!m_pSurfaces[pick].IsAllocated())
    {
        if ((pick == COHERENT) && m_NonCoherentSupported)
            return NON_COHERENT;
        else if ((pick == NON_COHERENT) && m_CoherentSupported)
            return COHERENT;
        else
            return validPick;
    }

    return pick;
}

//------------------------------------------------------------------------------

RC CpyEngTest::AllocMem()
{
    RC rc;
    GpuDevice * pGpuDev = GetBoundGpuDevice();
    // Maintain test 100 coverage at 32 bpp surface
    ColorUtils::Format colorFmtToUse = ColorUtils::A8R8G8B8;

    // Source and Destination checking surfaces
    m_SrcCheckSurf.SetPitch(m_MinLineWidthInBytes);
    m_SrcCheckSurf.SetHeight(m_pTstCfg->SurfaceHeight());
    m_SrcCheckSurf.SetDepth(1);
    m_SrcCheckSurf.SetWidth(m_pTstCfg->SurfaceWidth());
    m_SrcCheckSurf.SetColorFormat(colorFmtToUse);
    m_SrcCheckSurf.SetKernelMapping(true);
    m_SrcCheckSurf.SetLocation(Memory::Coherent);
    m_SrcCheckSurf.SetLayout(Surface2D::Pitch);
    CHECK_RC(m_SrcCheckSurf.Alloc(pGpuDev));
    CHECK_RC(m_SrcCheckSurf.Map(
                 GetBoundGpuSubdevice()->GetSubdeviceInst()));
    m_DstCheckSurf.SetPitch(m_MinLineWidthInBytes);
    m_DstCheckSurf.SetHeight(m_pTstCfg->SurfaceHeight());
    m_DstCheckSurf.SetDepth(1);
    m_DstCheckSurf.SetWidth(m_pTstCfg->SurfaceWidth());
    m_DstCheckSurf.SetColorFormat(colorFmtToUse);
    m_DstCheckSurf.SetKernelMapping(true);
    m_DstCheckSurf.SetLocation(Memory::Coherent);
    m_DstCheckSurf.SetLayout(Surface2D::Pitch);
    CHECK_RC(m_DstCheckSurf.Alloc(pGpuDev));
    CHECK_RC(m_DstCheckSurf.Map(
                 GetBoundGpuSubdevice()->GetSubdeviceInst()));

    // Possible Source surfaces:
    m_pSurfaces[FB_LINEAR].SetPitch(m_MinLineWidthInBytes);
    m_pSurfaces[FB_LINEAR].SetHeight(m_pTstCfg->SurfaceHeight());
    m_pSurfaces[FB_LINEAR].SetDepth(1);
    m_pSurfaces[FB_LINEAR].SetWidth(m_pTstCfg->SurfaceWidth());
    m_pSurfaces[FB_LINEAR].SetColorFormat(colorFmtToUse);
    m_pSurfaces[FB_LINEAR].SetKernelMapping(true);
    m_pSurfaces[FB_LINEAR].SetLocation(Memory::Optimal);
    m_pSurfaces[FB_LINEAR].SetLayout(Surface2D::Pitch);
    m_pSurfaces[FB_LINEAR].SetDisplayable(true);
    CHECK_RC(m_pSurfaces[FB_LINEAR].Alloc(pGpuDev));
    CHECK_RC(m_pSurfaces[FB_LINEAR].Map(
                 GetBoundGpuSubdevice()->GetSubdeviceInst()));

    m_pSurfaces[FB_TILED].SetTiled(true);
    m_pSurfaces[FB_TILED].SetPitch(m_MinLineWidthInBytes);
    m_pSurfaces[FB_TILED].SetHeight(m_pTstCfg->SurfaceHeight());
    m_pSurfaces[FB_TILED].SetDepth(1);
    m_pSurfaces[FB_TILED].SetWidth(m_pTstCfg->SurfaceWidth());
    m_pSurfaces[FB_TILED].SetColorFormat(colorFmtToUse);
    m_pSurfaces[FB_TILED].SetKernelMapping(true);
    m_pSurfaces[FB_TILED].SetLocation(Memory::Optimal);
    m_pSurfaces[FB_TILED].SetLayout(Surface2D::Pitch);
    CHECK_RC(m_pSurfaces[FB_TILED].Alloc(pGpuDev));
    CHECK_RC(m_pSurfaces[FB_TILED].Map(
                 GetBoundGpuSubdevice()->GetSubdeviceInst()));

    if (m_CoherentSupported)
    {
        m_pSurfaces[COHERENT].SetPitch(m_MinLineWidthInBytes);
        m_pSurfaces[COHERENT].SetHeight(m_pTstCfg->SurfaceHeight());
        m_pSurfaces[COHERENT].SetDepth(1);
        m_pSurfaces[COHERENT].SetWidth(m_pTstCfg->SurfaceWidth());
        m_pSurfaces[COHERENT].SetColorFormat(colorFmtToUse);
        m_pSurfaces[COHERENT].SetKernelMapping(true);
        m_pSurfaces[COHERENT].SetLocation(Memory::Coherent);
        m_pSurfaces[COHERENT].SetLayout(Surface2D::Pitch);
        CHECK_RC(m_pSurfaces[COHERENT].Alloc(pGpuDev));
        CHECK_RC(m_pSurfaces[COHERENT].Map(
                     GetBoundGpuSubdevice()->GetSubdeviceInst()));
    }

    m_pSurfaces[FB_BLOCKLINEAR].SetPitch(m_MinLineWidthInBytes);
    m_pSurfaces[FB_BLOCKLINEAR].SetHeight(m_pTstCfg->SurfaceHeight());
    m_pSurfaces[FB_BLOCKLINEAR].SetDepth(1);
    m_pSurfaces[FB_BLOCKLINEAR].SetWidth(m_pTstCfg->SurfaceWidth());
    m_pSurfaces[FB_BLOCKLINEAR].SetColorFormat(colorFmtToUse);
    m_pSurfaces[FB_BLOCKLINEAR].SetKernelMapping(true);
    m_pSurfaces[FB_BLOCKLINEAR].SetLocation(Memory::Optimal);

    // For GF100, Attempting to access a Tesla_4 Gob is problematic if
    // the layout is block linear, so sety the layout to pitch.

    m_pSurfaces[FB_BLOCKLINEAR].SetLayout(Surface2D::Pitch);
    CHECK_RC(m_pSurfaces[FB_BLOCKLINEAR].Alloc(pGpuDev));
    CHECK_RC(m_pSurfaces[FB_BLOCKLINEAR].Map(
                 GetBoundGpuSubdevice()->GetSubdeviceInst()));

    if (m_NonCoherentSupported)
    {
        m_pSurfaces[NON_COHERENT].SetPitch(m_MinLineWidthInBytes);
        m_pSurfaces[NON_COHERENT].SetHeight(m_pTstCfg->SurfaceHeight());
        m_pSurfaces[NON_COHERENT].SetDepth(1);
        m_pSurfaces[NON_COHERENT].SetWidth(m_pTstCfg->SurfaceWidth());
        m_pSurfaces[NON_COHERENT].SetColorFormat(colorFmtToUse);
        m_pSurfaces[NON_COHERENT].SetKernelMapping(true);
        m_pSurfaces[NON_COHERENT].SetLocation(Memory::NonCoherent);
        m_pSurfaces[NON_COHERENT].SetLayout(Surface2D::Pitch);
        CHECK_RC(m_pSurfaces[NON_COHERENT].Alloc(pGpuDev));
        CHECK_RC(m_pSurfaces[NON_COHERENT].Map(
                     GetBoundGpuSubdevice()->GetSubdeviceInst()));
    }
    else
    {
        Printf(Tee::PriHigh,
               "NonCoherent memory is not supported, disabling that surface.\n");
    }

    return rc;
}

//-----------------------------------------------------------------------------

void CpyEngTest::FreeMem()
{
    // Don't create an LwRmPtr unless there is memory to free, in case
    // mods exits before the RM was loaded.  LwRmPtr asserts in that case...
    m_pSurfaces.clear();
}

//-----------------------------------------------------------------------------

RC CpyEngTest::FillSurfaces()
{
    RC rc;

    Printf(Tee::PriDebug, "CpyEngTest:  Filling surfaces\n");

    if (m_UserDataPattern.size() != 0)
    {
        for (int i = 0; i < NUM_SURFACES; ++i)
        {
            if (m_pSurfaces[i].IsAllocated())
            {
                UINT32 patSize = (UINT32) (m_pSurfaces[i].GetSize()/2);
                Printf(Tee::PriDebug, "CpyEngTest: FILLING %p for %x bytes\n",
                       m_pSurfaces[i].GetAddress(),
                       (UINT32) (m_pSurfaces[i].GetSize()/2));
                CHECK_RC(Memory::FillPattern(m_pSurfaces[i].GetAddress(),
                                             &m_UserDataPattern,
                                             patSize));

                Printf(Tee::PriDebug, "CpyEngTest: ZEROING %p for %x bytes\n",
                       ((char *)m_pSurfaces[i].GetAddress() +
                        m_pSurfaces[i].GetSize()/2),
                       patSize);
                Memory::Fill32(
                    (char *)m_pSurfaces[i].GetAddress() +
                    (m_pSurfaces[i].GetSize()/2),
                    0, patSize/4); // WORDS!
            }
        }
    }
    // If no data pattern supplied, use Red for src and BLACK for dst
    else
    {
        // If the surface is the size of the display, it's safe to fill random
        UINT32 pitch = m_pSurfaces[0].GetPitch();
        CHECK_RC(GpuTest::AdjustPitchForScanout(&pitch));
        if (m_MinLineWidthInBytes == pitch && (m_SurfSize == m_MinLineWidthInBytes *
                                    m_pTstCfg->SurfaceHeight()))
        {
            UINT32 w = m_pTstCfg->SurfaceWidth();
            UINT32 h = m_pTstCfg->SurfaceHeight();
            UINT32 d = m_pTstCfg->DisplayDepth();

            Printf(Tee::PriDebug,
                   "CpyEngTest:  Filling surfaces with random data\n");

            for (int i = 0; i < NUM_SURFACES; ++i)
            {
                if (m_pSurfaces[i].IsAllocated())
                {
                    UINT32 patSize = (UINT32) (m_pSurfaces[i].GetSize()/2);
                    Printf(Tee::PriDebug,
                           "CpyEngTest:  Filling %d with random data\n", i);
                    Memory::FillRandom(m_pSurfaces[i].GetAddress(), 0x12345678,
                                       0, 0xffffffff, w, h/2, d,
                                       m_MinLineWidthInBytes);

                    Printf(Tee::PriDebug,
                           "CpyEngTest:  Zeroing bottom of %d\n", i);
                    Memory::Fill32(
                        (char *)m_pSurfaces[i].GetAddress() + (
                            m_pSurfaces[i].GetSize()/2),
                        0, patSize/4);
                }
            }
        }
        else
        {
            Printf(Tee::PriDebug, "CpyEngTest:  Filling surfaces with red\n");

            for (int i = 0; i < NUM_SURFACES; ++i)
            {
                if (m_pSurfaces[i].IsAllocated())
                {
                    UINT32 patSize = (UINT32) (m_pSurfaces[i].GetSize()/2);
                    Memory::Fill32(m_pSurfaces[i].GetAddress(), 0x00FF0000,
                                   patSize/4);

                    Memory::Fill32(
                        (char *)m_pSurfaces[i].GetAddress() + (
                            m_pSurfaces[i].GetSize()/2),
                        0, patSize/4);
                }
            }
        }
    }

    Printf(Tee::PriDebug, "CpyEngTest:  DONE Filling surfaces\n");
    return OK;
}

//-----------------------------------------------------------------------------
// Fancy Picker stuff
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

RC CpyEngTest::SetDefaultsForPicker(UINT32 picker)
{
    FancyPickerArray * pPickers = GetFancyPickerArray();
    FancyPicker &fp = (*pPickers)[picker];

    switch(picker)
    {
        case FPK_COPYENG_SRC_SURF:
        case FPK_COPYENG_DST_SURF:
            fp.ConfigRandom();
            fp.AddRandRange(1, (UINT32)FB_LINEAR, (UINT32)NUM_SURFACES - 1);
            fp.CompileRandom();
            break;

        case FPK_COPYENG_FORMAT_IN:
        case FPK_COPYENG_FORMAT_OUT:
            fp.ConfigRandom();
            fp.AddRandItem(1, 1);
            fp.AddRandItem(1, 2);
            fp.AddRandItem(1, 4);
            fp.CompileRandom();
            break;
        case FPK_COPYENG_SRC_BLOCK_SIZE_WIDTH:
        case FPK_COPYENG_DST_BLOCK_SIZE_WIDTH:
        case FPK_COPYENG_SRC_BLOCK_SIZE_DEPTH:
        case FPK_COPYENG_DST_BLOCK_SIZE_DEPTH:
            fp.ConfigRandom();
            fp.AddRandItem(1,0);
            fp.CompileRandom();
            break;

        case FPK_COPYENG_SRC_BLOCK_SIZE_HEIGHT:
        case FPK_COPYENG_DST_BLOCK_SIZE_HEIGHT:
            fp.ConfigRandom();
            fp.AddRandRange(1,0,5);
            fp.CompileRandom();
            break;

        case FPK_COPYENG_GOB_HEIGHT:
            fp.ConfigRandom();
            fp.AddRandItem(1,4);
            fp.AddRandItem(1,8);
            fp.CompileRandom();
            break;

        case FPK_COPYENG_LINE_COUNT:
            fp.ConfigRandom();
            fp.AddRandRange(1, 0, 2047);
            fp.CompileRandom();
            break;

        case FPK_COPYENG_OFFSET_IN:
        case FPK_COPYENG_OFFSET_OUT:
        case FPK_COPYENG_PITCH_IN:
        case FPK_COPYENG_PITCH_OUT:
        case FPK_COPYENG_LINE_BYTES:
        case FPK_COPYENG_SRC_ORIGIN_X:
        case FPK_COPYENG_SRC_ORIGIN_Y:
        case FPK_COPYENG_DST_ORIGIN_X:
        case FPK_COPYENG_DST_ORIGIN_Y:
        case FPK_COPYENG_SRC_WIDTH:
        case FPK_COPYENG_DST_WIDTH:
        case FPK_COPYENG_SRC_HEIGHT:
        case FPK_COPYENG_DST_HEIGHT:
        case FPK_COPYENG_NUM_PICKERS:
            fp.ConfigConst(0);
            break;

        default:
            return RC::SOFTWARE_ERROR;
    }
    return OK;
}

//------------------------------------------------------------------------------

UINT32 CpyEngTest::GetTotalErrorCount() const
{
    if (!m_LogErrors)
    {
        Printf(Tee::PriLow, "Warning: previous run did not log all errors, "
               "use DmaTest.LogErrors\n");
    }

    return m_TotalErrorCount;
}

UINT32 CpyEngTest::GetBlockWidth(UINT32 pickIdx)
{
    switch (m_DmaCopyAlloc[0].GetClass())
    {
        // Pascal HSCE only supports one block width
        default:
        case PASCAL_DMA_COPY_A:
        case PASCAL_DMA_COPY_B:
        case VOLTA_DMA_COPY_A:
        case TURING_DMA_COPY_A:
        case AMPERE_DMA_COPY_A:
        case AMPERE_DMA_COPY_B:
        case HOPPER_DMA_COPY_A:
        case BLACKWELL_DMA_COPY_A:
            return 0;

        case GF100_DMA_COPY:
        case KEPLER_DMA_COPY_A:
        case MAXWELL_DMA_COPY_A:
            return (*m_pPickers)[pickIdx].Pick();
    }
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ CpyEngTest object.
//------------------------------------------------------------------------------

JS_CLASS_INHERIT(CpyEngTest, PexTest, "GPU dma test");

JS_STEST_LWSTOM(CpyEngTest, RunPatt, 0,
                "Run the test with user-specified data pattern).")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);
    void *     pvCpyEngTest = JS_GetPrivate(pContext, pObject);
    MASSERT(pvCpyEngTest);
    CpyEngTest * pCpyEngTest  = (CpyEngTest *)pvCpyEngTest;

    JavaScriptPtr pJavaScript;

    // If the user passed arguments, use them as the pattern
    if (NumArguments != 0 )
    {
        JsArray  ptnArray;
        *pReturlwalue = JSVAL_NULL;
        UINT32 i;

        // if the user passed in an array for the first argument
        if (OK == (pJavaScript->FromJsval(pArguments[0], &ptnArray)))
        {
            UINT32 Size = UINT32(ptnArray.size());

            Printf(Tee::PriLow, "Parsing an array of size %d\n", Size);
            if (Size == 0)
            {
                Printf(Tee::PriNormal, "Can't parse an array of size zero\n");
                return JS_FALSE;
            }

            if (!pCpyEngTest->ParseUserDataPattern(ptnArray))
                return JS_FALSE;
        }
        // if the user passed in a list of ints e.g. (1,2,3,4,5)
        else
        {
            UINT32 temp;

            for (i = 0; i < NumArguments; ++i)
            {
                if ( OK != pJavaScript->FromJsval(pArguments[i], &temp))
                {
                    JS_ReportError(pContext,
                                   "Can't colwert parameters from JavaScript");
                    return JS_FALSE;
                }
                pCpyEngTest->AddToUserDataPattern(temp);
            }
        }

        // print out the list of parameters
        pCpyEngTest->PrintUserDataPattern(Tee::PriLow);
    }
    else
    {
        pCpyEngTest->ClearUserDataPattern();
    }

    // Make sure to call the ModsTest::RunWrapper to pick up the Setup/Cleanup
    // functions.  Bug 405461.
    RETURN_RC( pCpyEngTest->RunWrapper() );
}

//------------------------------------------------------------------------------

JS_STEST_LWSTOM(CpyEngTest, ForcePitchEven, 1,
                "Force the specified pitch (FPK_COPYENG_PITCH_{IN,OUT}) to be even.")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);
    void *     pvCpyEngTest = JS_GetPrivate(pContext, pObject);
    MASSERT(pvCpyEngTest);
    CpyEngTest * pCpyEngTest  = (CpyEngTest *)pvCpyEngTest;

    UINT32 which;
    JavaScriptPtr pJs;

    if (NumArguments != 1 || (OK != pJs->FromJsval(pArguments[0], &which)))
    {
        JS_ReportError(pContext,
                       "Usage: CpyEngTest.ForcePitchEven(FPK_COPYENG_PITCH_{IN,OUT})");
        return JS_FALSE;
    }

    switch(which)
    {
        case FPK_COPYENG_PITCH_IN:
            pCpyEngTest->ForcePitchInEven(true);
            break;
        case FPK_COPYENG_PITCH_OUT:
            pCpyEngTest->ForcePitchOutEven(true);
            break;

        default:
            Printf(Tee::PriHigh, "Unknown parameter to ForcePitchEven %d\n",
                   which);
            RETURN_RC(RC::BAD_PARAMETER);
    }

    RETURN_RC(OK);
}

JS_SMETHOD_LWSTOM(CpyEngTest, PrintUserPattern, 0,
                  "Print the memory fill pattern specified by the user.")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);
    void *     pvCpyEngTest = JS_GetPrivate(pContext, pObject);

    if (pvCpyEngTest)
    {
        CpyEngTest * pCpyEngTest  = (CpyEngTest *)pvCpyEngTest;
        pCpyEngTest->PrintUserDataPattern(Tee::PriNormal);
    }

    return JS_TRUE;
}

//
// JS Properties
//

CLASS_PROP_READONLY(CpyEngTest, Loop, UINT32,
                    "Current test loop");
CLASS_PROP_READONLY(CpyEngTest, ErrorCount, UINT32,
                    "Error count from the previous run");
CLASS_PROP_READWRITE(CpyEngTest, SurfSize, UINT32,
                     "Size in bytes of src and dst surfaces (if 0, matches mode).");
CLASS_PROP_READWRITE(CpyEngTest, MinLineWidthInBytes, UINT32,
                     "Manually set minimum line width in bytes (if 0, matches mode).");
CLASS_PROP_READWRITE(CpyEngTest, MaxXBytes, UINT32,
                     "Manually set maximum line width (in bytes).");
CLASS_PROP_READWRITE(CpyEngTest, MaxYLines, UINT32,
                     "Manually maximum number of lines for the transfer");
CLASS_PROP_READWRITE(CpyEngTest, PrintPri, UINT32,
                     "Print priority for the Copy Engine test itself");
CLASS_PROP_READWRITE(CpyEngTest, DetailedPri, UINT32,
                     "Print priority for detailed information");
CLASS_PROP_READWRITE(CpyEngTest, TestStep, bool,
                     "Pause after each frame until keystroke, q quits");
CLASS_PROP_READWRITE(CpyEngTest, XOptThresh, UINT32,
                     "If the X dimension of the DMA exceeds this value (and the Y exceeds YOptThresh), optimize");
CLASS_PROP_READWRITE(CpyEngTest, YOptThresh, UINT32,
                     "If the Y dimension of the DMA exceeds this value (and the X exceeds XOptThresh), optimize");
CLASS_PROP_READWRITE(CpyEngTest, CheckInputForError, bool,
                     "Whether or not to sanity check fancy pickers' picks");

PROP_CONST(CpyEngTest, FB_LINEAR, CpyEngTest::FB_LINEAR);
PROP_CONST(CpyEngTest, FB_TILED, CpyEngTest::FB_TILED);
PROP_CONST(CpyEngTest, COHERENT, CpyEngTest::COHERENT);
PROP_CONST(CpyEngTest, NON_COHERENT, CpyEngTest::NON_COHERENT);
PROP_CONST(CpyEngTest, FB_BLOCKLINEAR, CpyEngTest::FB_BLOCKLINEAR);

CLASS_PROP_READWRITE(CpyEngTest, DebuggingOnLA, bool,
                     "Whether we're debugging on a logic analyzer");
CLASS_PROP_READWRITE(CpyEngTest, DumpChunks, UINT32,
                     "If non-zero, print out chunks of data surrounding a failure");
CLASS_PROP_READWRITE(CpyEngTest, LogErrors, bool,
                     "If true, we log all errors rather than failing on the first one");
CLASS_PROP_READWRITE(CpyEngTest, EngMask, UINT32,
                     "Mask of engines to execute on (default = ~0)");
CLASS_PROP_READWRITE(CpyEngTest, AllMappings, bool,
                     "If supported, test mapping all PCEs to each LCE");

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
