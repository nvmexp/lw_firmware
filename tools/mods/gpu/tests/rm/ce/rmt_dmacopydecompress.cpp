/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// DmaCopy and DecompressClass test.
//

#include "gpu/include/gpudev.h"
#include "core/include/platform.h"
#include "lwos.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "class/cl2080.h"       // LW2080_ENGINE_TYPE*
#include "class/clb0b5.h"       // MAXWELL_DMA_COPY_A
#include "class/clb0b5sw.h"
#include "class/clc0b5.h"       // PASCAL_DMA_COPY_A
#include "class/clc1b5.h"       // PASCAL_DMA_COPY_B
#include "class/clc3b5.h"       // VOLTA_DMA_COPY_A
#include "class/clc5b5.h"       // TURING_DMA_COPY_A
#include "class/clc6b5.h"       // AMPERE_DMA_COPY_A
#include "class/clc7b5.h"       // AMPERE_DMA_COPY_B
#include "class/clc8b5.h"       // HOPPER_DMA_COPY_A
#include "class/clc9b5.h"       // BLACKWELL_DMA_COPY_A
#include "class/cla06f.h"       // KEPLER_CHANNEL_GPFIFO_A
#include "class/cla06fsubch.h"  // LWA06F_SUBCHANNEL_COPY_ENGINE
#include "core/utility/errloggr.h"
#include "gpu/utility/surf2d.h"
#include "gpu/include/gralloc.h"
#include "gpu/tests/rm/utility/changrp.h"
#include <vector>
#include "core/include/threadsync.h"
#include "gpu/tests/rm/utility/rmtestsyncpoints.h"
#include "core/include/utility.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/rmtest.h"
#include "core/include/memcheck.h"
#include "gpu/tests/rm/lwlink/rmt_lwlinkverify.h"

enum
{
    TEST_TYPE_NORMAL,   // Run a normal test
    TEST_TYPE_RC,       // Run an RC test
    TEST_TYPE_RC_PBDMA  // Run a PBDMA RC test
};

typedef struct
{
    Channel      *pCh;
    Surface2D    *pSrc;
    Surface2D    *pDst;
    Surface2D    *pSema;
    ModsEvent    *pModsEvent;
    LwU32         intrTypeIndex;
    LwRm::Handle  hCeObj;
    LwRm::Handle  hNotifyEvent;
    LwU32         ceEngineType;
    LwU32         hClass;
    LwU32         subChannel;
    RC            errorRc;
    LwU32         testType;
    const char   *testTypeDesc;

    bool          bIsTsg;
    ChannelGroup *pChGrp;
    ChannelGroup::SplitMethodStream *pStream;
} TestData;

#define FILL_PATTERN 0xbad0cafe

typedef struct
{
    LwU32 type;
    const char * name;
} IntrTypeInfo;

// Added supported types you want included in the test
static const IntrTypeInfo intrTypeInfo[] = {
    { LWB0B5_LAUNCH_DMA_INTERRUPT_TYPE_NON_BLOCKING,    "NON_BLOCKING"  },
    { LWB0B5_LAUNCH_DMA_INTERRUPT_TYPE_BLOCKING,        "BLOCKING"      },
    { LWB0B5_LAUNCH_DMA_INTERRUPT_TYPE_NONE,            "NONE"          }
};
#define INTERRUPT_TYPE_INDEX_NON_BLOCKING    0
#define INTERRUPT_TYPE_INDEX_BLOCKING        1
#define INTERRUPT_TYPE_INDEX_NONE            2

// Add engines you WANT EXCLUDED from the test, for example LW2080_ENGINE_TYPE_COPY2
static const LwU32 enginesToSkip[] = {
    LW2080_ENGINE_TYPE_NULL
};

class DmaCopyDecompressClassTest: public RmTest
{
public:
    DmaCopyDecompressClassTest();
    virtual ~DmaCopyDecompressClassTest();

    virtual string IsTestSupported();

    virtual RC   Setup();
    virtual RC   Run();
    virtual RC Cleanup();

private:
    FLOAT64 m_TimeoutMs = Tasker::NO_TIMEOUT;

    RC BasicDmaCopyDecompressClassTest();
    RC TestCeRc(TestData*);

    RC LaunchDma(TestData*);
    RC WaitAndVerify(TestData*);

    RC WriteMethod(TestData *pTestData, UINT32 mthd, UINT32 data);
    RC AllocTestData(LwU32 ceEngineType, LwU32 hClass, LwU32 intrTypeIndex,
                     LwU32 testType, TestData *&);
    void FreeTestData(TestData*);
    RC GetRunqueueForGrce(UINT32 ceEngineType, UINT32 *runqueue);

    vector<TestData*> m_TestData;
    vector<TestData*> m_RcTestData;
    LwlinkVerify lwlinkVerif;
};

//------------------------------------------------------------------------------
// Used to allow test to ignore nonblocking interrupts.
static bool ErrorLogFilter(const char *errMsg)
{
    const char * patterns[] =
    {
         "LogError called due to unexpected HW interrupt!*",
         "LW_PCE0_FALCON_IRQSTAT*",
         "LW_PCE0_FALCON_MAILBOX0*",
         "LWRM: processing * nonstall interrupt\n"
    };
    for (size_t i = 0; i < NUMELEMS(patterns); i++)
    {
        if (Utility::MatchWildCard(errMsg, patterns[i]))
        {
            Printf(Tee::PriHigh,
                "Ignoring error message: %s\n",
                errMsg);
            return false;
        }
    }
    return true; // log this error, it's not one of the filtered items.
}

//------------------------------------------------------------------------------
DmaCopyDecompressClassTest::DmaCopyDecompressClassTest()
{
    SetName("DmaCopyDecompressClassTest");
}

//------------------------------------------------------------------------------
DmaCopyDecompressClassTest::~DmaCopyDecompressClassTest()
{
    //Taken care of in Cleanup().
}

//------------------------------------------------------------------------
string DmaCopyDecompressClassTest::IsTestSupported()
{

    if( IsClassSupported(MAXWELL_DMA_COPY_A) ||
        IsClassSupported(PASCAL_DMA_COPY_A)  ||
        IsClassSupported(PASCAL_DMA_COPY_B)  ||
        IsClassSupported(VOLTA_DMA_COPY_A)   ||
        IsClassSupported(TURING_DMA_COPY_A)  ||
        IsClassSupported(AMPERE_DMA_COPY_A)  ||
        IsClassSupported(AMPERE_DMA_COPY_B)  ||
        IsClassSupported(HOPPER_DMA_COPY_A))
        return RUN_RMTEST_TRUE;
    return "No known supported DMA_COPY class";
}

//------------------------------------------------------------------------------
RC DmaCopyDecompressClassTest::Setup()
{
    LwRmPtr pLwRm;
    RC           rc;
    LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS engineParams = {0};
    LW2080_CTRL_GPU_GET_ENGINE_CLASSLIST_PARAMS classParams = {0};
    GpuSubdevice *pSubdevice = GetBoundGpuSubdevice();

    if(lwlinkVerif.IsSupported(pSubdevice))
    {
        lwlinkVerif.Setup(((GpuDevMgr*)DevMgrMgr::d_GraphDevMgr), 1);
    }

    SYNC_POINT(DMACOPYDECOMPRESSTEST_SP_SETUP_START);

    CHECK_RC(InitFromJs());

    m_TimeoutMs = m_TestConfig.TimeoutMs();
    m_TestConfig.SetAllowMultipleChannels(true);

    //
    // Get a list of supported engines
    //
    CHECK_RC_CLEANUP( pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                               LW2080_CTRL_CMD_GPU_GET_ENGINES_V2,
                               &engineParams,
                               sizeof (engineParams)) );

    //
    // Get the class list for each engine, if a supported CE class exists
    // generate test cases for it.
    //
    for (LwU32 i = 0; i < engineParams.engineCount; i++ )
    {
        bool bSkipEngine = false;

        for (LwU32 e = 0; e < sizeof(enginesToSkip)/sizeof(LwU32); e++ )
        {
            if (enginesToSkip[e] == engineParams.engineList[i])
            {
                bSkipEngine = true;
                break;
            }
        }

        if (bSkipEngine) continue;

        classParams.engineType = engineParams.engineList[i];
        classParams.numClasses = 0;
        classParams.classList = 0;
        CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                                LW2080_CTRL_CMD_GPU_GET_ENGINE_CLASSLIST,
                                &classParams, sizeof(classParams)));

        vector<UINT32> classList(classParams.numClasses);
        classParams.classList = LW_PTR_TO_LwP64(&(classList[0]));

        CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                                LW2080_CTRL_CMD_GPU_GET_ENGINE_CLASSLIST,
                                &classParams, sizeof(classParams)));

        for (LwU32 j = 0; j < classParams.numClasses; j++)
        {
            if (MAXWELL_DMA_COPY_A   == classList[j] ||
                PASCAL_DMA_COPY_A    == classList[j] ||
                PASCAL_DMA_COPY_B    == classList[j] ||
                VOLTA_DMA_COPY_A     == classList[j] ||
                TURING_DMA_COPY_A    == classList[j] ||
                AMPERE_DMA_COPY_A    == classList[j] ||
                AMPERE_DMA_COPY_B    == classList[j] ||
                HOPPER_DMA_COPY_A    == classList[j])
            {
                TestData *pNewTest;

                Printf(Tee::PriHigh,
                       "%s: Class 0x%X supported on CE%d, adding tests\n",
                        __FUNCTION__, classList[j], engineParams.engineList[i] - LW2080_ENGINE_TYPE_COPY0);

                for (LwU32 k = 0; k < sizeof(intrTypeInfo)/sizeof(IntrTypeInfo); k++)
                {
                    CHECK_RC_CLEANUP(AllocTestData(engineParams.engineList[i], classList[j],
                                                   k, TEST_TYPE_NORMAL, pNewTest));
                    m_TestData.push_back(pNewTest);

                    Printf(Tee::PriHigh,
                           "%s: Added copy test for CE%d, class: 0x%X, interrupt type: %s\n",
                           __FUNCTION__, pNewTest->ceEngineType - LW2080_ENGINE_TYPE_COPY0, pNewTest->hClass, intrTypeInfo[k].name);
                }

                if (Platform::GetSimulationMode() == Platform::Hardware)
                {
                    // Default RC test
                    CHECK_RC_CLEANUP(AllocTestData(engineParams.engineList[i], classList[j],
                                                   INTERRUPT_TYPE_INDEX_NONE,
                                                   TEST_TYPE_RC, pNewTest));
                    m_RcTestData.push_back(pNewTest);

                    // Add in a PBDMA test if the GPU supports it
                    CHECK_RC_CLEANUP(AllocTestData(engineParams.engineList[i], classList[j],
                                                   INTERRUPT_TYPE_INDEX_NONE,
                                                   TEST_TYPE_RC_PBDMA, pNewTest));
                    m_RcTestData.push_back(pNewTest);
                }
                break;
            }
        }
    }

Cleanup:
    SYNC_POINT(DMACOPYDECOMPRESSTEST_SP_SETUP_STOP);
    return rc;
}

//------------------------------------------------------------------------
RC DmaCopyDecompressClassTest::Run()
{
    RC rc;

    CHECK_RC(BasicDmaCopyDecompressClassTest());

    if(lwlinkVerif.IsSupported(GetBoundGpuSubdevice()))
    {
        if(!lwlinkVerif.verifySysmemTraffic(GetBoundGpuSubdevice()))
        {
            return RC::LWLINK_BUS_ERROR;
        }
    }

    return rc;
}

RC DmaCopyDecompressClassTest::BasicDmaCopyDecompressClassTest()
{
    RC     rc;

    // Flag used to control conlwrremt test runs
    LwBool bContinue = LW_FALSE;
    vector<TestData*>::iterator testIt;
    vector<TestData*>::reverse_iterator testRIt;

    //
    // Turn off unexpected interrupt checking because nonblocking interrupts
    // may be mis-attributed
    //
    ErrorLogger::StartingTest();
    ErrorLogger::InstallErrorLogFilter(ErrorLogFilter);

    do
    {
        SYNC_POINT(DMACOPYDECOMPRESSTEST_SP_RUN_START);

        // Not run on fmodel for now as it has some issues, Bug:581612
        if (Platform::GetSimulationMode() == Platform::Hardware)
        {
            for (testIt = m_RcTestData.begin(); testIt < m_RcTestData.end(); testIt++)
                CHECK_RC(TestCeRc(*testIt));
        }

        for (testIt = m_TestData.begin(); testIt < m_TestData.end(); testIt++)
            CHECK_RC(LaunchDma(*testIt));

        // Swapping the order of the verify so the NON_BLOCKING case is checked first.
        // For blocking intr cases, the test relies on a intr event instead of polling the semaphore.
        // It seems that we dont always get the intr routed to the correct CE per Bug 715362.
        // By seding the NON_BLOCKING work last and checking for it first, we ensure the test
        // is blocked on the semaphore and all test cases would have been done.
        for (testRIt = m_TestData.rbegin(); testRIt != m_TestData.rend(); ++testRIt)
            CHECK_RC(WaitAndVerify(*testRIt));

        bContinue = IS_CONLWRRENT() &&
                    (IS_TEST_RUNNING("FermiClockSanityTest") ||
                     IS_TEST_RUNNING("FermiClockPathTest"));

        SYNC_POINT(DMACOPYDECOMPRESSTEST_SP_RUN_STOP);
    } while (bContinue);

    ErrorLogger::TestCompleted();

    return rc;
}

//------------------------------------------------------------------------------
RC DmaCopyDecompressClassTest::Cleanup()
{
    lwlinkVerif.Cleanup();
    while (!m_TestData.empty())
    {
        FreeTestData(m_TestData.back());
        m_TestData.pop_back();
    }

    while (!m_RcTestData.empty())
    {
        FreeTestData(m_RcTestData.back());
        m_RcTestData.pop_back();
    }

    return OK;
}

RC DmaCopyDecompressClassTest::TestCeRc(TestData *pTestData)
{
    RC errorRc;
    RC rc;
    Channel *pCh = pTestData->pCh;

    //
    // We're going to be generating error interrupts.
    // Don't want to choke on those.
    //
    CHECK_RC(StartRCTest());

    // Perform an errant dma launch.
    LaunchDma(pTestData);

    if (pTestData->bIsTsg)
    {
        pTestData->pChGrp->WaitIdle();
    }
    else
    {
        pCh->WaitIdle(m_TimeoutMs);
    }

    //
    // Bug# 200074742
    // For Pascal, the launch error handling is different from the previous chips.
    // When a launch error is hit, the state machine gets locked in IDLE state.
    // Hence, pCh->WaitIdle will return true even before the rc error has been
    // handled and the message has been updated resulting in an error mismatch.
    // Include Sleep() to wait for the rc updates before error comparison
    //
    Tasker::Sleep(5000);

    errorRc = pCh->CheckError();

    if (errorRc != pTestData->errorRc)
    {
        Printf(Tee::PriHigh,
            "%d:DmaCopyDecompressClassTest: RC verification - error notifier incorrect. error = '%s'\n",
            __LINE__,errorRc.Message());
        CHECK_RC_CLEANUP(RC::UNEXPECTED_ROBUST_CHANNEL_ERR);
    }
    else
    {
        pCh->ClearError();
    }

Cleanup:
    EndRCTest();
    return rc;
}

RC DmaCopyDecompressClassTest::AllocTestData(LwU32 ceEngineType, LwU32 hClass,
                                             LwU32 intrTypeIndex, LwU32 testType,
                                             TestData* &pNewTest)
{
    RC rc;
    const LwU32 memSize = 0x1000;
    LwRmPtr pLwRm;
    LWB0B5_ALLOCATION_PARAMETERS params;
    LwRm::Handle hCh;
    void *pEventAddr;
    LW2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS eventParams = {0};
    LwU32 notifyIndex = ceEngineType - LW2080_ENGINE_TYPE_COPY0;
    LW2080_CTRL_CE_GET_CAPS_V2_PARAMS paramsCe = {0};
    UINT32 runqueue;

    pNewTest = new TestData;

    if (!pNewTest)
        return RC::SOFTWARE_ERROR;

    memset(pNewTest, 0, sizeof(TestData));

    pNewTest->hClass = hClass;

    pNewTest->pSema = new Surface2D;
    pNewTest->pSema->SetForceSizeAlloc(true);
    pNewTest->pSema->SetArrayPitch(1);
    pNewTest->pSema->SetArraySize(memSize);
    pNewTest->pSema->SetColorFormat(ColorUtils::VOID32);
    pNewTest->pSema->SetAddressModel(Memory::Paging);
    pNewTest->pSema->SetLayout(Surface2D::Pitch);
    pNewTest->pSema->SetLocation(Memory::NonCoherent);
    CHECK_RC_CLEANUP(pNewTest->pSema->Alloc(GetBoundGpuDevice()));
    CHECK_RC_CLEANUP(pNewTest->pSema->Map());
    pNewTest->pSema->Fill(0);

    pNewTest->pSrc = new Surface2D;
    pNewTest->pSrc->SetForceSizeAlloc(true);
    pNewTest->pSrc->SetArrayPitch(1);
    pNewTest->pSrc->SetArraySize(memSize);
    pNewTest->pSrc->SetColorFormat(ColorUtils::VOID32);
    pNewTest->pSrc->SetAddressModel(Memory::Paging);
    pNewTest->pSrc->SetLayout(Surface2D::Pitch);
    pNewTest->pSrc->SetLocation(Memory::Fb);
    CHECK_RC_CLEANUP(pNewTest->pSrc->Alloc(GetBoundGpuDevice()));
    CHECK_RC_CLEANUP(pNewTest->pSrc->Map());
    pNewTest->pSrc->Fill(FILL_PATTERN);

    pNewTest->pDst = new Surface2D;
    pNewTest->pDst->SetForceSizeAlloc(true);
    pNewTest->pDst->SetArrayPitch(1);
    pNewTest->pDst->SetArraySize(memSize);
    pNewTest->pDst->SetColorFormat(ColorUtils::VOID32);
    pNewTest->pDst->SetAddressModel(Memory::Paging);
    pNewTest->pDst->SetLayout(Surface2D::Pitch);
    pNewTest->pDst->SetLocation(Memory::Fb);
    CHECK_RC_CLEANUP(pNewTest->pDst->Alloc(GetBoundGpuDevice()));
    CHECK_RC_CLEANUP(pNewTest->pDst->Map());
    pNewTest->pDst->Fill(0);
    pNewTest->bIsTsg = false;

    // Query engine capabilities
    paramsCe.ceEngineType = ceEngineType;
    CHECK_RC_CLEANUP( pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                                     LW2080_CTRL_CMD_CE_GET_CAPS_V2,
                                     &paramsCe,
                                     sizeof (paramsCe)) );

    Printf(Tee::PriHigh,
           "%s: Getting CAPS for CE%d \n",
           __FUNCTION__, ceEngineType - LW2080_ENGINE_TYPE_COPY0);
    Printf(Tee::PriHigh,
           "%s: LW2080_CTRL_CE_CAPS_CE_GRCE: %d \n",
           __FUNCTION__, LW2080_CTRL_CE_GET_CAP(paramsCe.capsTbl, LW2080_CTRL_CE_CAPS_CE_GRCE));
    Printf(Tee::PriHigh,
           "%s: LW2080_CTRL_CE_CAPS_CE_SHARED: %d \n",
           __FUNCTION__, LW2080_CTRL_CE_GET_CAP(paramsCe.capsTbl, LW2080_CTRL_CE_CAPS_CE_SHARED));
    Printf(Tee::PriHigh,
           "%s: LW2080_CTRL_CE_CAPS_CE_SYSMEM_READ: %d \n",
           __FUNCTION__, LW2080_CTRL_CE_GET_CAP(paramsCe.capsTbl, LW2080_CTRL_CE_CAPS_CE_SYSMEM_READ));
    Printf(Tee::PriHigh,
           "%s: LW2080_CTRL_CE_CAPS_CE_SYSMEM_WRITE: %d \n",
           __FUNCTION__, LW2080_CTRL_CE_GET_CAP(paramsCe.capsTbl, LW2080_CTRL_CE_CAPS_CE_SYSMEM_WRITE));
    Printf(Tee::PriHigh,
           "%s: LW2080_CTRL_CE_CAPS_CE_LWLINK_P2P: %d \n",
           __FUNCTION__, LW2080_CTRL_CE_GET_CAP(paramsCe.capsTbl, LW2080_CTRL_CE_CAPS_CE_LWLINK_P2P));
    Printf(Tee::PriHigh,
           "%s: LW2080_CTRL_CE_CAPS_CE_SYSMEM: %d \n",
           __FUNCTION__, LW2080_CTRL_CE_GET_CAP(paramsCe.capsTbl, LW2080_CTRL_CE_CAPS_CE_SYSMEM));
    Printf(Tee::PriHigh,
           "%s: LW2080_CTRL_CE_CAPS_CE_P2P: %d \n",
           __FUNCTION__, LW2080_CTRL_CE_GET_CAP(paramsCe.capsTbl, LW2080_CTRL_CE_CAPS_CE_P2P));

    if (LW2080_CTRL_CE_GET_CAP(paramsCe.capsTbl, LW2080_CTRL_CE_CAPS_CE_GRCE))
    {
        CHECK_RC_CLEANUP(GetRunqueueForGrce(ceEngineType, &runqueue));

        if (runqueue > 0)
            pNewTest->bIsTsg = true;
    }

    // Allocate actual channel
    params.version = LWB0B5_ALLOCATION_PARAMETERS_VERSION_1;
    params.engineType = ceEngineType;

    //
    // KEPLER_CHANNEL_GPFIFO_A requires CE to be in a particular subchannel.
    // For the test, just use this value for all classes.
    //
    pNewTest->subChannel = LWA06F_SUBCHANNEL_COPY_ENGINE;

    if (pNewTest->bIsTsg)
    {
        // Create a TSG with SCG Type 1 that contains 1 GRCE channel on runqueue1
        UINT32        flags = 0;

        LW_CHANNEL_GROUP_ALLOCATION_PARAMETERS chGrpParams = {0};
        chGrpParams.engineType = ceEngineType;

        pNewTest->pChGrp  = new ChannelGroup(&m_TestConfig, &chGrpParams);
        pNewTest->pStream = new ChannelGroup::SplitMethodStream(pNewTest->pChGrp);

        pNewTest->pChGrp->SetUseVirtualContext(false);
        CHECK_RC(pNewTest->pChGrp->Alloc());

        flags = FLD_SET_DRF_NUM(OS04, _FLAGS, _GROUP_CHANNEL_RUNQUEUE, runqueue, flags);

        CHECK_RC(pNewTest->pChGrp->AllocChannel(&pNewTest->pCh, flags));
        CHECK_RC(pLwRm->Alloc(pNewTest->pCh->GetHandle(), &pNewTest->hCeObj, hClass, &params));
    }
    else
    {
        CHECK_RC_CLEANUP(
            m_TestConfig.AllocateChannel(&pNewTest->pCh, &hCh, ceEngineType));

        CHECK_RC_CLEANUP(pLwRm->Alloc(hCh, &pNewTest->hCeObj, hClass, &params));
    }

    {
        LW2080_CTRL_GPU_GET_ENGINE_PARTNERLIST_PARAMS partnerParams = {0};
        partnerParams.engineType = ceEngineType;
        partnerParams.partnershipClassId = pNewTest->pCh->GetClass();
        CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                LW2080_CTRL_CMD_GPU_GET_ENGINE_PARTNERLIST,
                &partnerParams, sizeof(partnerParams)));

        for (LwU32 eng = 0; eng < partnerParams.numPartners; eng++)
        {
            if (partnerParams.partnerList[eng] == LW2080_ENGINE_TYPE_GRAPHICS)
            {
                Printf(Tee::PriHigh,
                       "%s: According to the partnership API (GET_ENGINE_PARTNERLIST), CE%d should partner with GR.\n",
                       __FUNCTION__, ceEngineType - LW2080_ENGINE_TYPE_COPY0);
                // GRCE from Partnership API and CE CAPS API should match
                MASSERT(LW2080_CTRL_CE_GET_CAP(paramsCe.capsTbl, LW2080_CTRL_CE_CAPS_CE_GRCE));
                break;
            }
        }
    }

    switch (intrTypeInfo[intrTypeIndex].type)
    {
        case LWB0B5_LAUNCH_DMA_INTERRUPT_TYPE_NONE:
            break;

        case LWB0B5_LAUNCH_DMA_INTERRUPT_TYPE_NON_BLOCKING:

            pNewTest->pModsEvent = Tasker::AllocEvent();
            pEventAddr = Tasker::GetOsEvent(
                    pNewTest->pModsEvent,
                    pLwRm->GetClientHandle(),
                    pLwRm->GetDeviceHandle(GetBoundGpuDevice()));

            // Associate Event to Object
            CHECK_RC_CLEANUP(pLwRm->AllocEvent(
                pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                &pNewTest->hNotifyEvent,
                LW01_EVENT_OS_EVENT,
                LW2080_NOTIFIERS_CE(notifyIndex) | LW01_EVENT_NONSTALL_INTR,
                pEventAddr));

            eventParams.event = LW2080_NOTIFIERS_CE(notifyIndex);
            eventParams.action = LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_SINGLE;

            CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                        LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION,
                        &eventParams, sizeof(eventParams)));

            break;

        case LWB0B5_LAUNCH_DMA_INTERRUPT_TYPE_BLOCKING:
            pNewTest->pModsEvent = Tasker::AllocEvent();
            pEventAddr = Tasker::GetOsEvent(
                    pNewTest->pModsEvent,
                    pLwRm->GetClientHandle(),
                    pLwRm->GetDeviceHandle(GetBoundGpuDevice()));

            // Associate Event to Object
            CHECK_RC_CLEANUP(pLwRm->AllocEvent(
                pNewTest->hCeObj,
                &pNewTest->hNotifyEvent,
                LW01_EVENT_OS_EVENT,
                0,
                pEventAddr));
            break;
    }

    pNewTest->testType = testType;
    switch (testType)
    {
        case TEST_TYPE_NORMAL:
            pNewTest->errorRc = OK;
            pNewTest->testTypeDesc = "NORMAL";
            break;

        case TEST_TYPE_RC:
            pNewTest->testTypeDesc = "RC";
            switch (ceEngineType)
            {
                case LW2080_ENGINE_TYPE_COPY0:
                    pNewTest->errorRc = RC::RM_RCH_CE0_ERROR;
                    break;
                case LW2080_ENGINE_TYPE_COPY1:
                    pNewTest->errorRc = RC::RM_RCH_CE1_ERROR;
                    break;
                case LW2080_ENGINE_TYPE_COPY2:
                    pNewTest->errorRc = RC::RM_RCH_CE2_ERROR;
                    break;
                case LW2080_ENGINE_TYPE_COPY3:
                    pNewTest->errorRc = RC::RM_RCH_CE3_ERROR;
                    break;
                case LW2080_ENGINE_TYPE_COPY4:
                    pNewTest->errorRc = RC::RM_RCH_CE4_ERROR;
                    break;
                case LW2080_ENGINE_TYPE_COPY5:
                    pNewTest->errorRc = RC::RM_RCH_CE5_ERROR;
                    break;
                case LW2080_ENGINE_TYPE_COPY6:
                    pNewTest->errorRc = RC::RM_RCH_CE6_ERROR;
                    break;
                case LW2080_ENGINE_TYPE_COPY7:
                    pNewTest->errorRc = RC::RM_RCH_CE7_ERROR;
                    break;
                case LW2080_ENGINE_TYPE_COPY8:
                    pNewTest->errorRc = RC::RM_RCH_CE8_ERROR;
                    break;
                case LW2080_ENGINE_TYPE_COPY9:
                    pNewTest->errorRc = RC::RM_RCH_CE9_ERROR;
                    break;
                default:
                    MASSERT(0);
            }
            break;

        case TEST_TYPE_RC_PBDMA:
            pNewTest->testTypeDesc = "RC PBDMA";
            pNewTest->errorRc = RC::RM_RCH_PBDMA_ERROR;
            break;

        default:
            MASSERT(0);
    }

    pNewTest->intrTypeIndex = intrTypeIndex;
    pNewTest->ceEngineType = ceEngineType;

    return rc;
Cleanup:
    FreeTestData(pNewTest);
    return rc;
}

void DmaCopyDecompressClassTest::FreeTestData(TestData* pTestData)
{
    LwRmPtr pLwRm;

    if (!pTestData)
        return;

    if (pTestData->hNotifyEvent)
        pLwRm->Free(pTestData->hNotifyEvent);

    if (pTestData->pModsEvent)
        Tasker::FreeEvent(pTestData->pModsEvent);

    if (pTestData->hCeObj)
        pLwRm->Free(pTestData->hCeObj);

    if (pTestData->pCh)
        m_TestConfig.FreeChannel(pTestData->pCh);

    if (pTestData->pSema)
    {
        pTestData->pSema->Free();
        delete pTestData->pSema;
    }

    if (pTestData->pSrc)
    {
        pTestData->pSrc->Free();
        delete pTestData->pSrc;
    }

    if (pTestData->pDst)
    {
        pTestData->pDst->Free();
        delete pTestData->pDst;
    }

    if (pTestData->bIsTsg)
    {
        pTestData->pChGrp->Free();

        delete pTestData->pChGrp;
        delete pTestData->pStream;
    }

    delete pTestData;
}

RC DmaCopyDecompressClassTest::GetRunqueueForGrce(UINT32 ceEngineType, UINT32 *runqueue)
{
    RC rc;
    LwRmPtr pLwRm;
    GpuSubdevice *pSubdevice = GetBoundGpuSubdevice();
    LW2080_CTRL_GPU_GET_ENGINE_PARTNERLIST_PARAMS params;
    UINT32 ChClass = 0;

    *runqueue = 0;

    CHECK_RC(pLwRm->GetFirstSupportedClass(m_TestConfig.GetNumFifoClasses(),
                                           m_TestConfig.GetFifoClasses(),
                                           &ChClass, pSubdevice->GetParentDevice()));

    // Get engine partnerlist
    //
    for (UINT32 rq = 0; rq < pSubdevice->GetNumGraphicsRunqueues(); ++rq)
    {
        memset(&params, 0, sizeof(params));
        params.engineType = LW2080_ENGINE_TYPE_GRAPHICS;
        params.partnershipClassId = ChClass;
        params.runqueue = rq;
        CHECK_RC(pLwRm->ControlBySubdevice(pSubdevice,
            LW2080_CTRL_CMD_GPU_GET_ENGINE_PARTNERLIST,
            &params,
            sizeof(params)));

        // Loop over find the partner ce of graphic engine
        //
        for (UINT32 partnerIdx = 0; partnerIdx < params.numPartners; partnerIdx++)
        {
            if (params.partnerList[partnerIdx] == ceEngineType)
            {
                *runqueue = rq;
                return OK;
            }
        }
    }
    return RC::BAD_PARAMETER;
}

RC DmaCopyDecompressClassTest::WriteMethod(TestData* pTestData, UINT32 mthd, UINT32 data)
{
    RC rc;

    if (pTestData->bIsTsg)
    {
        rc = pTestData->pStream->Write(pTestData->subChannel, mthd, data);
    }
    else
    {
        rc = pTestData->pCh->Write(pTestData->subChannel, mthd, data);
    }

    return rc;
}

RC DmaCopyDecompressClassTest::LaunchDma(TestData* pTestData)
{
    RC rc;

    pTestData->pCh->SetObject(pTestData->subChannel, pTestData->hCeObj);

    if (pTestData->bIsTsg)
    {
        CHECK_RC(pTestData->pChGrp->Schedule());
    }

    CHECK_RC(WriteMethod(pTestData, LWB0B5_OFFSET_IN_UPPER, LwU64_HI32(pTestData->pSrc->GetCtxDmaOffsetGpu())));
    CHECK_RC(WriteMethod(pTestData, LWB0B5_OFFSET_IN_LOWER, LwU64_LO32(pTestData->pSrc->GetCtxDmaOffsetGpu())));

    CHECK_RC(WriteMethod(pTestData, LWB0B5_OFFSET_OUT_UPPER, LwU64_HI32(pTestData->pDst->GetCtxDmaOffsetGpu())));
    CHECK_RC(WriteMethod(pTestData, LWB0B5_OFFSET_OUT_LOWER, LwU64_LO32(pTestData->pDst->GetCtxDmaOffsetGpu())));

    CHECK_RC(WriteMethod(pTestData, LWB0B5_PITCH_IN, pTestData->pSrc->GetPitch()));
    CHECK_RC(WriteMethod(pTestData, LWB0B5_PITCH_OUT, pTestData->pDst->GetPitch()));

    CHECK_RC(WriteMethod(pTestData, LWB0B5_LINE_COUNT, 1));
    CHECK_RC(WriteMethod(pTestData, LWB0B5_LINE_LENGTH_IN, LwU64_LO32(pTestData->pSrc->GetSize())));

    CHECK_RC(WriteMethod(pTestData, LWB0B5_SET_SRC_DEPTH, 1));
    CHECK_RC(WriteMethod(pTestData, LWB0B5_SET_SRC_LAYER, 0));
    CHECK_RC(WriteMethod(pTestData, LWB0B5_SET_SRC_WIDTH, LwU64_LO32(pTestData->pSrc->GetSize())));
    CHECK_RC(WriteMethod(pTestData, LWB0B5_SET_SRC_HEIGHT, 1));

    CHECK_RC(WriteMethod(pTestData, LWB0B5_SET_SEMAPHORE_PAYLOAD, 0x12345678));

    CHECK_RC(WriteMethod(pTestData, LWB0B5_SET_SEMAPHORE_A,
                DRF_NUM(B0B5, _SET_SEMAPHORE_A, _UPPER, LwU64_HI32(pTestData->pSema->GetCtxDmaOffsetGpu()))));

    //
    // Normal test, no RC errors please
    //
    if (pTestData->testType == TEST_TYPE_NORMAL)
    {
        CHECK_RC(WriteMethod(pTestData, LWB0B5_SET_SEMAPHORE_B, LwU64_LO32(pTestData->pSema->GetCtxDmaOffsetGpu())));
        CHECK_RC(WriteMethod(pTestData, LWB0B5_LAUNCH_DMA,
                     DRF_DEF(B0B5, _LAUNCH_DMA, _SEMAPHORE_TYPE, _RELEASE_ONE_WORD_SEMAPHORE) |
                     DRF_NUM(B0B5, _LAUNCH_DMA, _INTERRUPT_TYPE, intrTypeInfo[pTestData->intrTypeIndex].type) |
                     DRF_DEF(B0B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE) |
                     DRF_DEF(B0B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _PITCH) |
                     DRF_DEF(B0B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH) |
                     DRF_DEF(B0B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, _NON_PIPELINED)));
    }

    //
    // Induce an RC error (misaligned semaphore address)
    //
    else if (pTestData->testType == TEST_TYPE_RC)
    {
        CHECK_RC(WriteMethod(pTestData, LWB0B5_SET_SEMAPHORE_B, LwU64_LO32(pTestData->pSema->GetCtxDmaOffsetGpu() | 1)));
        CHECK_RC(WriteMethod(pTestData, LWB0B5_LAUNCH_DMA,
                     DRF_DEF(B0B5, _LAUNCH_DMA, _SEMAPHORE_TYPE, _RELEASE_FOUR_WORD_SEMAPHORE) |
                     DRF_NUM(B0B5, _LAUNCH_DMA, _INTERRUPT_TYPE, intrTypeInfo[pTestData->intrTypeIndex].type) |
                     DRF_DEF(B0B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE) |
                     DRF_DEF(B0B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _PITCH) |
                     DRF_DEF(B0B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH) |
                     DRF_DEF(B0B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, _NON_PIPELINED)));
    }

    //
    // Induce an PBDMA RC error (send down an illegal method)
    //
    else if (pTestData->testType == TEST_TYPE_RC_PBDMA)
    {
        CHECK_RC(WriteMethod(pTestData, LWB0B5_SET_SEMAPHORE_B, LwU64_LO32(pTestData->pSema->GetCtxDmaOffsetGpu())));
        CHECK_RC(WriteMethod(pTestData, LWB0B5_LAUNCH_DMA,
                     DRF_DEF(B0B5, _LAUNCH_DMA, _SEMAPHORE_TYPE, _RELEASE_ONE_WORD_SEMAPHORE) |
                     DRF_NUM(B0B5, _LAUNCH_DMA, _INTERRUPT_TYPE, intrTypeInfo[pTestData->intrTypeIndex].type) |
                     DRF_DEF(B0B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE) |
                     DRF_DEF(B0B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _PITCH) |
                     DRF_DEF(B0B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH) |
                     DRF_DEF(B0B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, _NON_PIPELINED)));

        // Send down an illegal method.
        CHECK_RC(WriteMethod(pTestData, LWB0B5_PM_TRIGGER_END+sizeof(UINT32),0));
    }

    if (pTestData->bIsTsg)
    {
        CHECK_RC(pTestData->pChGrp->Flush());
    }
    else
    {
        pTestData->pCh->Flush();
    }

    Printf(Tee::PriHigh,
           "%s: Launching DMA for CE%d, class: 0x%X, interrupt type: %s, test type: %s\n",
           __FUNCTION__, pTestData->ceEngineType - LW2080_ENGINE_TYPE_COPY0, pTestData->hClass, intrTypeInfo[pTestData->intrTypeIndex].name, pTestData->testTypeDesc);

    return rc;
}

//
// WaitAndVerify - Waits for a CE engine to finish and then checks the copied values.
// Note that on Fermi, WaitAndVerify won't work if there are ever multiple channels
// running on a single CE all using the non-blocking interrupt, see bug 715362.
//
RC DmaCopyDecompressClassTest::WaitAndVerify
(
    TestData *pTestData
)
{
    RC rc;
    LwU32 semVal;
    vector<UINT32> fillVec(1);

    Printf(Tee::PriHigh,
           "%s: Polling for idle on CE%d, class: 0x%X, interrupt type: %s\n",
           __FUNCTION__, pTestData->ceEngineType - LW2080_ENGINE_TYPE_COPY0, pTestData->hClass, intrTypeInfo[pTestData->intrTypeIndex].name);

    switch (intrTypeInfo[pTestData->intrTypeIndex].type)
    {
        case LWB0B5_LAUNCH_DMA_INTERRUPT_TYPE_NON_BLOCKING:
            do
            {
                if (MEM_RD32(pTestData->pSema->GetAddress()) == 0x12345678)
                    break;
            } while (Tasker::WaitOnEvent(pTestData->pModsEvent, m_TimeoutMs));

            break;
        case LWB0B5_LAUNCH_DMA_INTERRUPT_TYPE_BLOCKING:
            Tasker::WaitOnEvent(pTestData->pModsEvent, m_TimeoutMs);
            break;
        case LWB0B5_LAUNCH_DMA_INTERRUPT_TYPE_NONE:

            if (pTestData->bIsTsg)
            {
                CHECK_RC(pTestData->pChGrp->WaitIdle());
            }
            else
            {
                CHECK_RC(pTestData->pCh->WaitIdle(m_TimeoutMs));
            }
            break;
    }

    semVal = MEM_RD32(pTestData->pSema->GetAddress());

    if (semVal != 0x12345678)
    {
        Printf(Tee::PriHigh,
              "%s: expected semaphore to be 0x%x, was 0x%x\n",
              __FUNCTION__, 0x12345678, semVal);
        CHECK_RC(RC::DATA_MISMATCH);
    }
    else
    {
        Printf(Tee::PriHigh,
               "%s: Recieved correct semaphore for CE%d, class: 0x%X, interrupt type: %s\n",
               __FUNCTION__, pTestData->ceEngineType - LW2080_ENGINE_TYPE_COPY0, pTestData->hClass, intrTypeInfo[pTestData->intrTypeIndex].name);
    }

    fillVec[0] = FILL_PATTERN;

    rc = Memory::ComparePattern(pTestData->pDst->GetAddress(), &fillVec,
                (UINT32) pTestData->pDst->GetSize());

    if (rc != OK)
    {
        Printf(Tee::PriHigh,
               "%s: Data was not copied successfully for CE%d, class: 0x%X, interrupt type: %s\n",
               __FUNCTION__, pTestData->ceEngineType - LW2080_ENGINE_TYPE_COPY0, pTestData->hClass, intrTypeInfo[pTestData->intrTypeIndex].name);
        CHECK_RC(RC::DATA_MISMATCH);
    }
    else
    {
        Printf(Tee::PriHigh,
               "%s: Data was copied successfully for CE%d, class: 0x%X, interrupt type: %s\n",
               __FUNCTION__, pTestData->ceEngineType - LW2080_ENGINE_TYPE_COPY0, pTestData->hClass, intrTypeInfo[pTestData->intrTypeIndex].name);
    }

    return rc;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ DmaCopyDecompressClassTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(DmaCopyDecompressClassTest, RmTest,
                 "DmaCopyDecompressClass RM test.");
