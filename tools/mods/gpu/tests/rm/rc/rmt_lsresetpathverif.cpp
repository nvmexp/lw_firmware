/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */


//!
//! \file rmt_lsresetpathverif.cpp
//! \brief To verify reset recovery of LWDEC engine
//!


#include "core/include/channel.h"
#include "core/include/platform.h"
#include "gpu/tests/rmtest.h"
#include "ctrl/ctrl2080.h"
#include "gpu/tests/rmtest.h"
#include "gpu/include/gralloc.h"
#include "gpu/tests/rm/utility/clk/clkbase.h"
#include "gpu/tests/rm/memsys/rmt_acrverify.h"
#include "gpu/include/gpusbdev.h"

#include "class/cla0b0.h" // LWA0B0_VIDEO_DECODER
#include "class/clb0b0.h" // LWB0B0_VIDEO_DECODER
#include "class/clb6b0.h" // LWB6B0_VIDEO_DECODER
#include "class/clc1b0.h" // LWC1B0_VIDEO_DECODER
#include "class/clc2b0.h" // LWC2B0_VIDEO_DECODER
#include "class/clc3b0.h" // LWC3B0_VIDEO_DECODER
#include "class/clc4b0.h" // LWC4B0_VIDEO_DECODER
#include "class/clc6b0.h" // LWC6B0_VIDEO_DECODER
#include "class/clb8b0.h" // LWB8B0_VIDEO_DECODER
#include "core/include/memcheck.h"

// The number of RC tests to instance
#define TEST_INSTANCES_RC     3

typedef struct
{
    Channel      *pCh;
    LwdecAlloc   *pDecoderAlloc;
} TestData;

class LsResetPathVerifTest: public RmTest
{
public:
    LsResetPathVerifTest();
    virtual ~LsResetPathVerifTest();
    virtual string IsTestSupported();
    virtual RC   Setup();
    virtual RC   Run();
    virtual RC   Cleanup();

private:    
    FLOAT64          m_TimeoutMs;
    AcrVerify        acrVerify;
    GrClassAllocator *m_classAllocator;

    RC TestLwdecRc(TestData*);
    RC PerformIlwalidDma(TestData*);
    RC AllocTestData(TestData *&);
    void FreeTestData(TestData*);
    vector<TestData*> m_RcTestData;

    LwU32        m_testInstancesRc;
    LwBool       bLwdecLsSupported;
    UINT64       m_gpuAddr;
    UINT32       *m_cpuAddr;
};

//------------------------------------------------------------------------------
LsResetPathVerifTest::LsResetPathVerifTest()
{
    SetName("LsResetPathVerifTest");

    m_classAllocator = new LwdecAlloc();
    m_gpuAddr        = 0LL;
    m_cpuAddr        = NULL;
}


//! \brief GpuCacheTest destructor
//!
//! Placeholder : doesnt do much.
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
LsResetPathVerifTest::~LsResetPathVerifTest()
{
}


//! \brief IsTestSupported(), Looks for whether test can execute in current elw. 
//!
//! Determines is test is supported by checking for DECODER class availability.
//! 
//! \return True if the test can be run in the current environment, 
//!         false otherwise
//------------------------------------------------------------------------------
string LsResetPathVerifTest::IsTestSupported()
{
    GpuDevice *m_pGpuDev = GetBoundGpuDevice();

    if (m_classAllocator->IsSupported(m_pGpuDev))
    {
        Printf(Tee::PriHigh,"Test is supported on the current GPU/platform, preparing to run...\n");
    }
    else
    {
        switch(m_classAllocator->GetClass())
        {
            case LWA0B0_VIDEO_DECODER:
                return "Class LWA0B0_VIDEO_DECODER Not supported on GPU";
            case LWB0B0_VIDEO_DECODER:
                return "Class LWB0B0_VIDEO_DECODER Not supported on GPU";
            case LWB6B0_VIDEO_DECODER:
                return "Class LWB6B0_VIDEO_DECODER Not supported on GPU";
            case LWC1B0_VIDEO_DECODER:
                return "Class LWC1B0_VIDEO_DECODER Not supported on GPU";
            case LWC2B0_VIDEO_DECODER:
                return "Class LWC2B0_VIDEO_DECODER Not supported on GPU";
            case LWC3B0_VIDEO_DECODER:
                return "Class LWC3B0_VIDEO_DECODER Not supported on GPU";
            case LWC4B0_VIDEO_DECODER:
                return "Class LWC4B0_VIDEO_DECODER Not supported on GPU";
            case LWC6B0_VIDEO_DECODER:
                return "Class LWC6B0_VIDEO_DECODER Not supported on GPU";
            case LWB8B0_VIDEO_DECODER:
                return "Class LWB8B0_VIDEO_DECODER Not supported on GPU";
            default:
                return "Invalid Class Specified";
        }
    }

    GpuSubdevice *m_pSubdevice = GetBoundGpuSubdevice();

    // Set number of RC test instances
    if (IsGM20XorBetter(m_pSubdevice->DeviceId()))
    {
        if(TEST_INSTANCES_RC)
        {
            m_testInstancesRc = TEST_INSTANCES_RC;
        }
        else
        {
            return "Test instances should be non zero";
        }
    }
    else
    {
        return "Supported only on GM20X+ chips";
    }

    acrVerify.BindTestDevice(GetBoundTestDevice());
    if (acrVerify.IsTestSupported() ==  RUN_RMTEST_TRUE)
    {
        bLwdecLsSupported = LW_TRUE;
    }
    else
    {
        return "Test is not supported as we expect LWDEC to be running in light secure mode";
    }

    return RUN_RMTEST_TRUE;
}


//! \brief  Setup(): Generally used for any test level allocation
//!
//! Allocates some number of test data structs for later use
//!
//! \return corresponding RC if any allocation fails
//------------------------------------------------------------------------------
RC LsResetPathVerifTest::Setup()
{
    TestData    *pNewTest;
    RC          rc;
    LwRmPtr     pLwRm;

    CHECK_RC(InitFromJs());

    m_TimeoutMs = m_TestConfig.TimeoutMs();
    m_TestConfig.SetAllowMultipleChannels(true);

    Printf(Tee::PriHigh,
           "%s: Test will instance a total of %d engines across %d channels\n",
           __FUNCTION__, m_testInstancesRc, m_testInstancesRc);

    // Setup any RC instances
    for (LwU32 i = 0; i < m_testInstancesRc; i++)
    {
        CHECK_RC_CLEANUP(AllocTestData(pNewTest));
        m_RcTestData.push_back(pNewTest);

        Printf(Tee::PriHigh,
                "%s: Added RC test for class: 0x%X, chid: 0x%X\n",
                __FUNCTION__, pNewTest->pDecoderAlloc->GetClass(), pNewTest->pCh->GetChannelId());
    }

    if(bLwdecLsSupported)
    {
       acrVerify.BindTestDevice(GetBoundTestDevice());
       acrVerify.Setup();
    }

Cleanup:
    return rc;
}


//! \brief TestLwdecRc()
//!
//! This function attempts to verify the LWDEC RC path.
//!
//! \return OK all the tests passed, specific RC if failed.
//! \sa Run()
//------------------------------------------------------------------------------
RC LsResetPathVerifTest::TestLwdecRc(TestData *pTestData)
{
    RC      rc, errorRc, expectedRc;
    Channel *pCh = pTestData->pCh;

    Printf(Tee::PriHigh,
           "%s: Beginning LWDEC RC verification...\n", __FUNCTION__);

    CHECK_RC(StartRCTest());

    // Perform errant dma commands.
    CHECK_RC(PerformIlwalidDma(pTestData));

    pCh->WaitIdle(m_TimeoutMs);

    errorRc = pCh->CheckError();

    expectedRc = RC::RM_RCH_LWDEC0_ERROR;

    if (errorRc == expectedRc)
    {
        Printf(Tee::PriHigh,
               "%s: Successfuly completed RC verification for class: 0x%X, chid: 0x%X, for LWDEC\n",
                __FUNCTION__, pTestData->pDecoderAlloc->GetClass(), pTestData->pCh->GetChannelId());
        pTestData->pCh->ClearError();
    }
    else
    {
        Printf(Tee::PriHigh,
               "%d:%s: RC verification - error notifier incorrect for class: 0x%X, chid: 0x%X. error = '%s' for LWDEC\n",
               __LINE__, __FUNCTION__,
               pTestData->pDecoderAlloc->GetClass(),
               pTestData->pCh->GetChannelId(),
               errorRc.Message());
        CHECK_RC_CLEANUP(RC::UNEXPECTED_ROBUST_CHANNEL_ERR);
    }

    if (bLwdecLsSupported)
    {
        CHECK_RC(acrVerify.BindTestDevice(GetBoundTestDevice()));
        CHECK_RC_MSG(acrVerify.VerifyFalconStatus(LSF_FALCON_ID_LWDEC, LW_TRUE), "Error: LWDEC failed to Boot in Expected Security Mode\n");
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
RC LsResetPathVerifTest::AllocTestData(TestData* &pNewTest)
{
    LwRm::Handle   hCh;
    RC             rc;

    pNewTest = new TestData;

    if (!pNewTest)
        return RC::SOFTWARE_ERROR;

    memset(pNewTest, 0, sizeof(TestData));

    // Allocate channel

    CHECK_RC_CLEANUP(m_TestConfig.AllocateChannel(&pNewTest->pCh, &hCh, LW2080_ENGINE_TYPE_LWDEC(0)));

    pNewTest->pDecoderAlloc = new LwdecAlloc;

    CHECK_RC_CLEANUP(pNewTest->pDecoderAlloc->AllocOnEngine(hCh,
                                                            LW2080_ENGINE_TYPE_LWDEC(0),
                                                            GetBoundGpuDevice(),
                                                            0));

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
void LsResetPathVerifTest::FreeTestData(TestData* pTestData)
{
    if (!pTestData)
        return;

    if (pTestData->pDecoderAlloc)
    {
        pTestData->pDecoderAlloc->Free();
        delete pTestData->pDecoderAlloc;
    }

    if (pTestData->pCh)
        m_TestConfig.FreeChannel(pTestData->pCh);

    delete pTestData;
}


//! \brief Cleanup:: 
//!
//! Cleans up after test completes
//!
//! \return OK if the tests passed, specific RC if failed
//------------------------------------------------------------------------
RC LsResetPathVerifTest::Cleanup()
{
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
RC LsResetPathVerifTest::Run()
{
    RC     rc;
    vector<TestData*>::iterator testIt;

    // Launch any RC instances
    for (testIt = m_RcTestData.begin(); testIt < m_RcTestData.end(); testIt++)
    {
        CHECK_RC(TestLwdecRc(*testIt));
    }
    return rc; 
}

//! \brief PerformIlwalidDma:: Perform invalid Dma
//!
//! Perform DMA commands
//!
//! \return OK if success, specific RC if failed
//------------------------------------------------------------------------
RC LsResetPathVerifTest::PerformIlwalidDma
(
    TestData* pTestData
)
{
    Channel *pCh  = pTestData->pCh;
    UINT32  subch = 0;
    RC      rc;

    Printf(Tee::PriHigh,
            "%s: Performing DMA sequence for class: 0x%X, chid: 0x%X, for LWDEC\n",
            __FUNCTION__, pTestData->pDecoderAlloc->GetClass(), pTestData->pCh->GetChannelId());

    pCh->SetObject(subch, pTestData->pDecoderAlloc->GetHandle());

    // Send down an illegal method.
    switch(pTestData->pDecoderAlloc->GetClass())
    {
        case LWA0B0_VIDEO_DECODER:
        {
            pCh->Write(subch, LWA0B0_PM_TRIGGER_END+sizeof(LwU32), 0);
        }
        break;

        case LWB0B0_VIDEO_DECODER:
        case LWB6B0_VIDEO_DECODER:
        case LWB8B0_VIDEO_DECODER:
        case LWC1B0_VIDEO_DECODER:
        case LWC2B0_VIDEO_DECODER:
        case LWC3B0_VIDEO_DECODER:
        case LWC4B0_VIDEO_DECODER:
        case LWC6B0_VIDEO_DECODER:
        {
            pCh->Write(subch, LWB0B0_PM_TRIGGER_END+sizeof(LwU32), 0);
        }
        break;

        default:
        {
            Printf(Tee::PriHigh,"Class 0x%x not supported.. skipping the  Semaphore Exelwtion\n",
                m_classAllocator->GetClass());
            return RC::LWRM_ILWALID_CLASS;
        }
    }

    pCh->Flush();

    return rc;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ LsResetPathVerifTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(LsResetPathVerifTest, RmTest,
                 "LsResetPathVerif RM test.");

