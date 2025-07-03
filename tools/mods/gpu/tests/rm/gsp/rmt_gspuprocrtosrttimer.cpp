/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_GspRtosRtTimer.cpp
//! \brief rmtest for GSP
//!

#include "gpu/tests/rmtest.h"
#include "gpu/tests/rm/utility/uprocrtos.h"
#include "gpu/tests/rm/utility/uprocgsprtos.h"
#include "rmlsfm.h"
#include "gpu/include/gpusbdev.h"

#include "core/include/platform.h"

#include "class/clc310.h"
#include "ctrl/ctrlc310.h"
#include "rmgspcmdif.h"
#include "uprocifcmn.h"
#include "uprociftest.h"

/* Default timer count. Written to the LW_CGSP_TIMER_0_INTERVAL register */
#define RTTIMER_TEST_DEFAULT_COUNT      (1000)
#define RTTIMER_TEST_NUM_PASSES         (5)

class GspUprocRtosRtTimerTest : public RmTest
{
public:
    GspUprocRtosRtTimerTest();
    virtual ~GspUprocRtosRtTimerTest()
    {
    };
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    unique_ptr<UprocRtos>      m_pGspRtos;
};

//! \brief GspUprocRtosRtTimerTest constructor
//!
//! Doesn't do much except SetName.
//!
//! \sa Setup
//------------------------------------------------------------------------------
GspUprocRtosRtTimerTest::GspUprocRtosRtTimerTest()
{
    SetName("GspRtosRtTimerTest");
}

//! \brief IsTestSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string GspUprocRtosRtTimerTest::IsTestSupported()
{
    LwRmPtr      m_pLwRm;
    RC rc;

    if (m_pLwRm->IsClassSupported(VOLTA_GSP, GetBoundGpuDevice()) != true)
    {
        return "[GSP Uproc RTOS RtTimer RMTest] : Gsp is not supported";
    }

    m_pGspRtos = std::make_unique<UprocGspRtos>(GetBoundGpuSubdevice());
    if (m_pGspRtos == nullptr)
    {
        return "Could not create UprocGspRtos instance.";
    }
    rc = m_pGspRtos->Initialize();
    if (OK != rc)
    {
        m_pGspRtos->Shutdown();
        return "[GSP Uproc RTOS RtTimer RMTest] : GSP RTOS not supported";
    }

    if (!m_pGspRtos->CheckTestSupported(RM_UPROC_CMD_TYPE(TEST, RTTIMER_TEST)))
    {
        return "[GSP Uproc RTOS RtTimer RMTest] : Test is not supported";
    }
    return RUN_RMTEST_TRUE;
}

//! \brief Do Init from Js
//!
//! \return OK if no error happens, otherwise corresponding rc
//------------------------------------------------------------------------------
RC GspUprocRtosRtTimerTest::Setup()
{
    RC rc;

    // Initialize RmTest handle
    m_pGspRtos->SetRmTestHandle(this);

    CHECK_RC(InitFromJs());
    return OK;
}

//! \brief Run the test
//!
//! Make sure GSP is bootstrapped.  Then run test items.
//!
//! \return OK all tests pass, else corresponding rc
//------------------------------------------------------------------------------
RC GspUprocRtosRtTimerTest::Run()
{
    RC rc;
    LwU32 isRunningOnFmodel;
    rc = m_pGspRtos->WaitUprocReady();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
               "%d:GspUprocRtosRtTimerTest::%s: Ucode OS is not ready. Skipping tests. \n",
                __LINE__,__FUNCTION__);

        // fail the test, we need to figure out why GSP bootstrap failed.
        CHECK_RC(RC::LWRM_STATUS_ERROR_NOT_READY);
        return rc;
    }
    Printf(Tee::PriHigh,
           "%d:GspUprocRtosRtTimerTest::%s: Starts RtTimer Test...\n",
            __LINE__,__FUNCTION__);
    isRunningOnFmodel = (Platform::GetSimulationMode() != Platform::Fmodel);
    CHECK_RC(m_pGspRtos->UprocRtosRunTests(UPROC_TEST_TYPE_RTTIMER, RTTIMER_TEST_DEFAULT_COUNT, RTTIMER_TEST_NUM_PASSES, isRunningOnFmodel));
    return rc;
}

//! \brief Cleanup, shuts down GSP test instance.
//------------------------------------------------------------------------------
RC GspUprocRtosRtTimerTest::Cleanup()
{
    m_pGspRtos->Shutdown();
    return OK;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(GspUprocRtosRtTimerTest, RmTest, "GSP Uproc RTOS RT Timer Test. \n");
