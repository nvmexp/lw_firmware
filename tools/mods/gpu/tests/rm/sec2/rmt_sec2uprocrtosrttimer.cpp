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
//! \file rmt_Sec2RtosRtTimer.cpp
//! \brief rmtest for SEC2
//!

#include "gpu/tests/rmtest.h"
#include "gpu/tests/rm/utility/uprocrtos.h"
#include "gpu/tests/rm/utility/uprocsec2rtos.h"
#include "rmlsfm.h"
#include "gpu/include/gpusbdev.h"

#include "core/include/platform.h"

#include "class/cl9070.h"
#include "class/cl9270.h"
#include "class/cl9470.h"
#include "class/cl9570.h"
#include "class/clb6b9.h"
#include "ctrl/ctrlb6b9.h"
#include "uprocifcmn.h"
#include "uprociftest.h"

/* Default timer count. Written to the LW_CSEC_TIMER_0_INTERVAL register */
#define RTTIMER_TEST_DEFAULT_COUNT      (1000)
#define RTTIMER_TEST_NUM_PASSES         (5)

class Sec2UprocRtosRtTimerTest : public RmTest
{
public:
    Sec2UprocRtosRtTimerTest();
    virtual ~Sec2UprocRtosRtTimerTest()
    {
    };
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    unique_ptr<UprocRtos>      m_pSec2Rtos;
};

//! \brief Sec2UprocRtosRtTimerTest constructor
//!
//! Doesn't do much except SetName.
//!
//! \sa Setup
//------------------------------------------------------------------------------
Sec2UprocRtosRtTimerTest::Sec2UprocRtosRtTimerTest()
{
    SetName("Sec2RtosRtTimerTest");
}

//! \brief IsTestSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string Sec2UprocRtosRtTimerTest::IsTestSupported()
{
    LwRmPtr      m_pLwRm;
    RC rc;

    if (m_pLwRm->IsClassSupported(MAXWELL_SEC2, GetBoundGpuDevice()) != true)
    {
        return "[SEC2 Uproc RTOS RtTimer RMTest] : Sec2 is not supported";
    }

    m_pSec2Rtos = std::make_unique<UprocSec2Rtos>(GetBoundGpuSubdevice());
    if (m_pSec2Rtos == nullptr)
    {
        return "Could not create UprocSec2Rtos instance.";
    }
    rc = m_pSec2Rtos->Initialize();
    if (OK != rc)
    {
        m_pSec2Rtos->Shutdown();
        return "[SEC2 Uproc RTOS RtTimer RMTest] : SEC2 RTOS not supported";
    }

    if (!m_pSec2Rtos->CheckTestSupported(RM_UPROC_CMD_TYPE(TEST, RTTIMER_TEST)))
    {
        return "[SEC2 Uproc RTOS RtTimer RMTest] : Test is not supported";
    }
    return RUN_RMTEST_TRUE;
}

//! \brief Do Init from Js
//!
//! \return OK if no error happens, otherwise corresponding rc
//------------------------------------------------------------------------------
RC Sec2UprocRtosRtTimerTest::Setup()
{
    RC rc;

    // Initialize RmTest handle
    m_pSec2Rtos->SetRmTestHandle(this);

    CHECK_RC(InitFromJs());
    return OK;
}

//! \brief Run the test
//!
//! Make sure SEC2 is bootstrapped.  Then run test items.
//!
//! \return OK all tests pass, else corresponding rc
//------------------------------------------------------------------------------
RC Sec2UprocRtosRtTimerTest::Run()
{
    RC rc;
    LwU32 isRunningOnFmodel;
    rc = m_pSec2Rtos->WaitUprocReady();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
               "%d:Sec2UprocRtosRtTimerTest::%s: Ucode OS is not ready. Skipping tests. \n",
                __LINE__,__FUNCTION__);

        // fail the test, we need to figure out why SEC2 bootstrap failed.
        CHECK_RC(RC::LWRM_STATUS_ERROR_NOT_READY);
        return rc;
    }
    Printf(Tee::PriHigh,
           "%d:Sec2UprocRtosRtTimerTest::%s: Starts RtTimer Test...\n",
            __LINE__,__FUNCTION__);
    isRunningOnFmodel = (Platform::GetSimulationMode() != Platform::Fmodel);
    CHECK_RC(m_pSec2Rtos->UprocRtosRunTests(UPROC_TEST_TYPE_RTTIMER, RTTIMER_TEST_DEFAULT_COUNT, RTTIMER_TEST_NUM_PASSES, isRunningOnFmodel));
    return rc;
}

//! \brief Cleanup, shuts down SEC2 test instance.
//------------------------------------------------------------------------------
RC Sec2UprocRtosRtTimerTest::Cleanup()
{
    m_pSec2Rtos->Shutdown();
    return OK;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(Sec2UprocRtosRtTimerTest, RmTest, "SEC2 Uproc RTOS RT Timer Test. \n");
