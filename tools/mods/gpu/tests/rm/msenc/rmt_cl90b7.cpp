/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_cl90b7.cpp
//! \brief To verify basic functionality of MSENC engine and
//!  its resman code paths.
//!

#include "core/include/channel.h"
#include "core/include/platform.h"
#include "gpu/tests/rm/utility/rmtestutils.h"
#include "gpu/tests/rmtest.h"
#include "ctrl/ctrl0080.h"
#include "ctrl/ctrl2080.h"
#include "core/utility/errloggr.h"
#include "gpu/tests/rmtest.h"
#include "gpu/utility/surf2d.h"
#include "gpu/include/gralloc.h"
#include "gpu/tests/rm/utility/clk/clkbase.h"
#include "gpu/tests/rm/memsys/rmt_acrverify.h"
#include "gpu/include/gpusbdev.h"

#include "class/cl90b7.h" // LW90B7_VIDEO_ENCODER
#include "class/clc0b7.h" // LWC0B7_VIDEO_ENCODER - For MAXWELL_and_later
#include "class/cld0b7.h" // LWD0B7_VIDEO_ENCODER - For GM20X_and_later
#include "class/clc1b7.h" // LWC1B7_VIDEO_ENCODER - For GP100
#include "class/clc2b7.h" // LWC2B7_VIDEO_ENCODER - For GP10X except GP100
#include "class/clc3b7.h" // LWC3B7_VIDEO_ENCODER - For GV100
#include "class/clc4b7.h" // LWC4B7_VIDEO_ENCODER - For TU10X except TU117
#include "class/clb4b7.h" // LWB4B7_VIDEO_ENCODER - For TU117
#include "class/clc7b7.h" // LWC7B7_VIDEO_ENCODER - For GA10X
#include "class/clc9b7.h" // LWC9B7_VIDEO_ENCODER - For AD10X
#include "core/include/memcheck.h"

// The number of non-RC tests to instance
#define TEST_INSTANCES        3

// The number of RC tests to instance
// Disabled due to Bug 738249
#define TEST_INSTANCES_RC     3
#define MAX_MSENC             3

typedef struct
{
    Channel      *pCh;
    Surface2D    *pSema;
    LwU32        subChannel;
    EncoderAlloc *pEencoderAlloc;
    LwU8         *pCaps;
    LwU32        engine;
} TestData;

class Class90b7Test: public RmTest
{
public:
    Class90b7Test();
    virtual ~Class90b7Test();

    virtual string IsTestSupported();
    virtual RC   Setup();
    virtual RC   Run();
    virtual RC   Cleanup();

private:
    FLOAT64      m_TimeoutMs;
    GpuSubdevice *m_pSubdevice;
    AcrVerify   acrVerify;

    RC TestMsencRc(TestData*);
    RC PerformDma(TestData*, bool);
    RC WaitAndVerify(TestData*);
    RC AllocTestData(TestData *&, LwU32);
    void FreeTestData(TestData*);
    vector<TestData*> m_TestData;
    vector<TestData*> m_RcTestData;
    LwU32 m_testInstances;
    LwU32 m_testInstancesRc;
    LwBool bLwencLsSupported;
};

//------------------------------------------------------------------------------
Class90b7Test::Class90b7Test()
{
    SetName("Class90b7Test");
}

//! \brief GpuCacheTest destructor
//!
//! Placeholder : doesnt do much.
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
Class90b7Test::~Class90b7Test()
{
}

//! \brief IsTestSupported(), Looks for whether test can execute in current elw.
//!
//! Determines is test is supported by checking for ENCODER class availability.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string Class90b7Test::IsTestSupported()
{
    EncoderAlloc    tmpEncoder;
    Gpu::LwDeviceId chip = GetBoundGpuSubdevice()->DeviceId();

    // Bug: 2080810: Disabling this test till it gets fixed for GA100
#if LWCFG(GLOBAL_GPU_IMPL_GA100)
    if (GetBoundGpuSubdevice()->DeviceId() == Gpu::GA100)
        return "GA100 doesn't support this test";
#endif

    tmpEncoder.SetNewestSupportedClass(LWC9B7_VIDEO_ENCODER);
    tmpEncoder.SetOldestSupportedClass(LW90B7_VIDEO_ENCODER);

    //
    // Supported class comes out of classList which will be parsed from Newest to oldest class supported
    // As we need to support all classes after 90B7 to class C4B7, we set 90B7 as oldest and C4B7 as newest
    //
    if (tmpEncoder.IsSupported(GetBoundGpuDevice()))
    {
        Printf(Tee::PriHigh,"Test is supported on the current GPU/platform, preparing to run...\n");
    }
    else
    {
        return "Test is not supported on the current GPU/platform as (LW90B7|LWC0B7|LWD0B7|LWC1B7|LWC2B7|LWC3B7|LWC4B7)_VIDEO_ENCODER class in not available";
    }

    // Set number of RC test instances
    if (IsGM20XorBetter(chip))
    {
        m_testInstancesRc = TEST_INSTANCES_RC;
    }
    else
    {
        m_testInstancesRc = 0;            // The number of RC tests to instance, Disabled due to Bug 738249
    }

    m_testInstances = TEST_INSTANCES;

    //
    // We are removing LS support for GP10X_and_later chips as it is not required
    // and we dont want to give unnecessary priviledge to any clients for security reasons
    // We support LWENC in LS mode only on GM20X and GP100
    // On Turing we could add LS support back as we have per falcon WPRs
    //
    acrVerify.BindTestDevice(GetBoundTestDevice());
    if ((chip >= Gpu::GM200) && (chip <= Gpu::GP100))
    {
        if (acrVerify.IsTestSupported() ==  RUN_RMTEST_TRUE)
        {
            bLwencLsSupported = LW_TRUE;
        }
        else
        {
            return "Test is not supported as we expect LWENC to be running in light secure mode";
        }
    }
    else
    {
        bLwencLsSupported = LW_FALSE;
    }

    return RUN_RMTEST_TRUE;
}

//! \brief  Setup(): Generally used for any test level allocation
//!
//! Allocates some number of test data structs for later use
//!
//! \return corresponding RC if any allocation fails
//------------------------------------------------------------------------------
RC Class90b7Test::Setup()
{
    TestData                              *pNewTest;
    RC                                    rc;
    UINT32                                availableMsenc[MAX_MSENC] = {0};
    LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS engineParams = {0};
    LwU32                                 i;
    LwRmPtr                               pLwRm;

    m_pSubdevice = GetBoundGpuSubdevice();

    CHECK_RC(InitFromJs());

    CHECK_RC_CLEANUP( pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                               LW2080_CTRL_CMD_GPU_GET_ENGINES_V2,
                               &engineParams,
                               sizeof (engineParams)) );

    for ( i = 0; i < engineParams.engineCount; i++ )
    {
          switch (engineParams.engineList[i])
          {
              case LW2080_ENGINE_TYPE_LWENC0:
                  availableMsenc[0] = true;
                  break;
              case LW2080_ENGINE_TYPE_LWENC1:
                  availableMsenc[1] = true;
                  break;
              case LW2080_ENGINE_TYPE_LWENC2:
                  availableMsenc[2] = true;
                  break;
          }
    }

    m_TimeoutMs = m_TestConfig.TimeoutMs();
    m_TestConfig.SetAllowMultipleChannels(true);

    Printf(Tee::PriHigh,
           "%s: Test will instance a total of %d engines across %d channels\n",
           __FUNCTION__, m_testInstances+m_testInstancesRc, m_testInstances+m_testInstancesRc);

    // Setup any non-RC instances
    if (m_testInstances)
    {
        for(LwU32 eng = 0; eng < MAX_MSENC; eng++)
        {
            if(!availableMsenc[eng])
                continue;

            for (i = 0; i < m_testInstances; i++)
            {
                CHECK_RC(AllocTestData(pNewTest, eng));
                m_TestData.push_back(pNewTest);

                Printf(Tee::PriHigh,
                       "%s: Added functional test for class: 0x%X, chid: 0x%X\n",
                       __FUNCTION__, pNewTest->pEencoderAlloc->GetClass(), pNewTest->pCh->GetChannelId());
            }
        }
    }

    // Setup any RC instances
    if (m_testInstancesRc)
    {
        for(LwU32 eng = 0; eng < MAX_MSENC; eng++)
        {
            if(!availableMsenc[eng])
                continue;

            for (LwU32 i = 0; i < m_testInstancesRc; i++)
            {
                CHECK_RC(AllocTestData(pNewTest, eng));
                m_RcTestData.push_back(pNewTest);

                Printf(Tee::PriHigh,
                       "%s: Added RC test for class: 0x%X, chid: 0x%X\n",
                       __FUNCTION__, pNewTest->pEencoderAlloc->GetClass(), pNewTest->pCh->GetChannelId());
            }
        }
    }

    if(bLwencLsSupported)
    {
       acrVerify.BindTestDevice(GetBoundTestDevice());
       acrVerify.Setup();
    }

Cleanup:
    return rc;
}

//! \brief TestMsencRc()
//!
//! This function calls attempts to verify the MSENC RC path.
//!
//! \return OK all the tests passed, specific RC if failed.
//! \sa Run()
//------------------------------------------------------------------------------
RC Class90b7Test::TestMsencRc(TestData *pTestData)
{
    RC      rc, errorRc, expectedRc;
    LwU32   falconId = LSF_FALCON_ID_ILWALID;
    Channel *pCh = pTestData->pCh;

    Printf(Tee::PriHigh,
           "%s: Beginning MSENC RC verification...\n", __FUNCTION__);

    CHECK_RC(StartRCTest());

    // Perform errant dma commands.
    CHECK_RC(PerformDma(pTestData, true));

    pCh->WaitIdle(m_TimeoutMs);

    errorRc = pCh->CheckError();

    // Check RC error and LS falcon status
    switch (pTestData->engine)
    {
        case 0:
                expectedRc = RC::RM_RCH_LWENC0_ERROR;
                falconId = LSF_FALCON_ID_LWENC0;
                break;
        case 1:
                expectedRc = RC::RM_RCH_LWENC1_ERROR;
                falconId = LSF_FALCON_ID_LWENC1;
                break;
        case 2:
                expectedRc = RC::RM_RCH_LWENC2_ERROR;
                falconId = LSF_FALCON_ID_LWENC2;
                break;
        default:
                Printf(Tee::PriHigh, "Class90b7Test: Unsupported engine Id %u\n",
                        pTestData->engine);
                MASSERT(0);
                return RC::UNEXPECTED_RESULT;
    }

    if (errorRc == expectedRc)
    {
        Printf(Tee::PriHigh,
               "%s: Successfuly completed RC verification for class: 0x%X, chid: 0x%X, for MSENC%d\n",
                __FUNCTION__, pTestData->pEencoderAlloc->GetClass(), pTestData->pCh->GetChannelId(), pTestData->engine);
        pTestData->pCh->ClearError();
    }
    else
    {
        Printf(Tee::PriHigh,
               "%d:%s: RC verification - error notifier incorrect for class: 0x%X, chid: 0x%X. error = '%s' for MSENC%u\n",
               __LINE__, __FUNCTION__,
               pTestData->pEencoderAlloc->GetClass(),
               pTestData->pCh->GetChannelId(),
               errorRc.Message(),
               (unsigned int) pTestData->engine);
        CHECK_RC_CLEANUP(RC::UNEXPECTED_ROBUST_CHANNEL_ERR);
    }

    if (bLwencLsSupported)
    {
        CHECK_RC(acrVerify.BindTestDevice(GetBoundTestDevice()));
        CHECK_RC_MSG(acrVerify.VerifyFalconStatus(falconId, LW_TRUE), "Error: LWENC failed to Boot in Expected Security Mode\n");
    }

Cleanup:
    EndRCTest();
    return rc;
}

//! \brief AllocTestData:: Allocate test data
//!
//! Allocate data used to run each instance of the test
//!
//! \return OK if the allocate passed, specific RC if failed
//------------------------------------------------------------------------
RC Class90b7Test::AllocTestData(TestData* &pNewTest, LwU32 engine)
{
    LwU32 engineType = LW2080_ENGINE_TYPE_LWENC(engine);
    LW0080_CTRL_MSENC_GET_CAPS_PARAMS msencCapsParams = {0};
    const LwU32 memSize = 0x1000;
    LwRm::Handle hCh;
    LwRmPtr pLwRm;
    RC rc;

    pNewTest = new TestData;

    if (!pNewTest)
        return RC::SOFTWARE_ERROR;

    memset(pNewTest, 0, sizeof(TestData));

    CHECK_RC_CLEANUP(m_TestConfig.AllocateChannel(&pNewTest->pCh, &hCh, engineType));

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

    pNewTest->pEencoderAlloc = new EncoderAlloc;
    pNewTest->engine = engine;
    CHECK_RC_CLEANUP(pNewTest->pEencoderAlloc->AllocOnEngine(hCh,
                                                             engineType,
                                                             GetBoundGpuDevice(),
                                                             0));

    // Query for LW0080_CTRL_MSENC_CAPS_MPECREWIND_BUG_775053.
    pNewTest->pCaps = new LwU8[LW0080_CTRL_MSENC_CAPS_TBL_SIZE];
    if (pNewTest->pCaps == NULL)
    {
        CHECK_RC_CLEANUP(RC::LWRM_INSUFFICIENT_RESOURCES);
    }

    msencCapsParams.capsTblSize = LW0080_CTRL_MSENC_CAPS_TBL_SIZE;
    msencCapsParams.capsTbl = (LwP64) pNewTest->pCaps;

    CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                LW0080_CTRL_CMD_MSENC_GET_CAPS,
                &msencCapsParams, sizeof(msencCapsParams)));

    // MSENC_CAPS_MPECREWIND_BUG_775053 must be exposed on and only on LW90B7_VIDEO_ENCODER
    if ( ((pNewTest->pEencoderAlloc->GetClass() == LW90B7_VIDEO_ENCODER) &&
         (!LW0080_CTRL_MSENC_GET_CAP(pNewTest->pCaps, LW0080_CTRL_MSENC_CAPS_MPECREWIND_BUG_775053))) ||
         ((pNewTest->pEencoderAlloc->GetClass() != LW90B7_VIDEO_ENCODER) &&
         ( LW0080_CTRL_MSENC_GET_CAP(pNewTest->pCaps, LW0080_CTRL_MSENC_CAPS_MPECREWIND_BUG_775053))) )
    {
       Printf(Tee::PriHigh,
              "%s: LW0080_CTRL_MSENC_CAPS_MPECREWIND_BUG_775053 not properly exported for class 0x%X, failing.\n",
              __FUNCTION__, pNewTest->pEencoderAlloc->GetClass());

       rc = RC::SOFTWARE_ERROR;
       goto Cleanup;
    }

    return rc;

Cleanup:
    FreeTestData(pNewTest);
    return rc;
}

//! \brief FreeTestData:: Free test data
//!
//! Free data used to run each instance of the test
//!
//! \return OK if the free passed, specific RC if failed
//------------------------------------------------------------------------
void Class90b7Test::FreeTestData(TestData* pTestData)
{
    if (!pTestData)
        return;

    if (pTestData->pEencoderAlloc)
    {
        pTestData->pEencoderAlloc->Free();
        delete pTestData->pEencoderAlloc;
    }

    if (pTestData->pCh)
        m_TestConfig.FreeChannel(pTestData->pCh);

    if (pTestData->pSema)
    {
        pTestData->pSema->Free();
        delete pTestData->pSema;
    }

    if (pTestData->pCaps != NULL)
    {
        delete []pTestData->pCaps;
    }

    delete pTestData;
}

//! \brief Cleanup::
//!
//! Cleans up after test completes
//!
//! \return OK if the tests passed, specific RC if failed
//------------------------------------------------------------------------
RC Class90b7Test::Cleanup()
{
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

//! \brief Run:: Used generally for placing all the testing stuff.
//!
//! Runs the required tests.
//!
//! \return OK if the tests passed, specific RC if failed
//------------------------------------------------------------------------
RC Class90b7Test::Run()
{
    RC     rc;

    vector<TestData*>::iterator testIt;

    // Launch any RC instances
    if (m_testInstancesRc)
    {
        for (testIt = m_RcTestData.begin(); testIt < m_RcTestData.end(); testIt++)
            CHECK_RC(TestMsencRc(*testIt));
    }

    // Launch any non-RC instances
    if (m_testInstances)
    {
        for (testIt = m_TestData.begin(); testIt < m_TestData.end(); testIt++)
            CHECK_RC(PerformDma(*testIt, false));

        for (testIt = m_TestData.begin(); testIt < m_TestData.end(); testIt++)
            CHECK_RC(WaitAndVerify(*testIt));
    }

    return rc;
}

//! \brief PerformDma:: Perform Dma
//!
//! Perform DMA commands
//!
//! \return OK if success, specific RC if failed
//------------------------------------------------------------------------
RC Class90b7Test::PerformDma
(
    TestData* pTestData,
    bool bInduceRcError
)
{
    Channel *pCh = pTestData->pCh;

    RC rc;

    Printf(Tee::PriHigh,
           "%s: Performing DMA sequence for class: 0x%X, chid: 0x%X, for MSENC%d\n",
           __FUNCTION__, pTestData->pEencoderAlloc->GetClass(), pTestData->pCh->GetChannelId(), pTestData->engine);

    pCh->SetObject(pTestData->subChannel, pTestData->pEencoderAlloc->GetHandle());

    // Induce an RC error if requested
    if (bInduceRcError)
    {
        // Send down an illegal method.
        pCh->Write(pTestData->subChannel, LW90B7_PM_TRIGGER_END+sizeof(LwU32),0);
    }
    else
    {
        pCh->Write(pTestData->subChannel, LW90B7_SEMAPHORE_A,
                        DRF_NUM(90B7, _SEMAPHORE_A, _UPPER, LwU64_HI32(pTestData->pSema->GetCtxDmaOffsetGpu())));
        pCh->Write(pTestData->subChannel, LW90B7_SEMAPHORE_B, LwU64_LO32(pTestData->pSema->GetCtxDmaOffsetGpu()));
        pCh->Write(pTestData->subChannel, LW90B7_SEMAPHORE_C,
                        DRF_NUM(90B7, _SEMAPHORE_C, _PAYLOAD, 0x12345678));
        pCh->Write(pTestData->subChannel, LW90B7_SEMAPHORE_D,
                        DRF_DEF(90B7, _SEMAPHORE_D, _STRUCTURE_SIZE, _ONE) |
                        DRF_DEF(90B7, _SEMAPHORE_D, _AWAKEN_ENABLE, _FALSE));
    }

    pCh->Flush();

    return rc;
}

//! \brief WaitAndVerify:: Wait and verify
//!
//! Wait for engine to finish and verify results
//!
//! \return OK if the tests passed, specific RC if failed
//------------------------------------------------------------------------
RC Class90b7Test::WaitAndVerify
(
    TestData *pTestData
)
{
    RC rc;
    LwU32 semVal;

    Printf(Tee::PriHigh,
           "%s: Polling for idle on for class: 0x%X, chid: 0x%X, for MSENC%d\n",
           __FUNCTION__, pTestData->pEencoderAlloc->GetClass(), pTestData->pCh->GetChannelId(), pTestData->engine);

    CHECK_RC(pTestData->pCh->WaitIdle(m_TimeoutMs));

    semVal = MEM_RD32(pTestData->pSema->GetAddress());

    if (semVal != 0x12345678)
    {
        Printf(Tee::PriHigh,
               "%s: Recieved incorrect semaphore for class: 0x%X, chid: 0x%X, for MSENC%d\n",
               __FUNCTION__, pTestData->pEencoderAlloc->GetClass(), pTestData->pCh->GetChannelId(),pTestData->engine);
        Printf(Tee::PriHigh,
              "%s: expected semaphore to be 0x%x, was 0x%x\n",
              __FUNCTION__, 0x12345678, semVal);
        CHECK_RC(RC::DATA_MISMATCH);
    }
    else
    {
        Printf(Tee::PriHigh,
               "%s: Recieved correct semaphore for class: 0x%X, chid: 0x%X, for MSENC%d\n",
               __FUNCTION__, pTestData->pEencoderAlloc->GetClass(), pTestData->pCh->GetChannelId(), pTestData->engine);
    }

    return rc;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ Class90b7Test object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(Class90b7Test, RmTest,
                 "Class90b7 RM test.");
