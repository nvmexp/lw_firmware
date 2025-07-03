/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_Sec2UprocRtosMutex.cpp
//! \brief rmtest for SEC2 mutexes
//!

#include "gpu/tests/rmtest.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/tests/rm/utility/uprocrtos.h"
#include "gpu/tests/rm/utility/uprocsec2rtos.h"
#include "mods_reg_hal.h"

#include "gpu/tests/rm/memsys/rmt_acrverify.h"
#include "rmlsfm.h"

#include "class/cl9070.h"
#include "class/cl9270.h"
#include "class/cl9470.h"
#include "class/cl9570.h"
#include "class/clb6b9.h"
#include "ctrl/ctrlb6b9.h"

class Sec2UprocRtosMutexTest : public RmTest
{
public:
    Sec2UprocRtosMutexTest();
    virtual ~Sec2UprocRtosMutexTest()
    {
    };
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    unique_ptr<UprocRtos>      m_pSec2Rtos;
};

//! \brief Sec2UprocRtosMutexTest constructor
//!
//! Doesn't do much except SetName.
//!
//! \sa Setup
//------------------------------------------------------------------------------
Sec2UprocRtosMutexTest::Sec2UprocRtosMutexTest()
{
    SetName("Sec2RtosMutexTest");
}

//! \brief IsTestSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string Sec2UprocRtosMutexTest::IsTestSupported()
{
    Gpu::LwDeviceId chipName = GetBoundGpuSubdevice()->DeviceId();
    if (IsClassSupported(MAXWELL_SEC2) && (chipName >= Gpu::GV100))
    {
        return RUN_RMTEST_TRUE;
    }
    return "[SEC2Uproc RTOS RMTest] : Supported only on GV100+ chips with SEC2 RTOS";
}

//! \brief Do Init from Js
//!
//! \return OK if no error happens, otherwise corresponding rc
//------------------------------------------------------------------------------
RC Sec2UprocRtosMutexTest::Setup()
{
    RC rc;

    // Get SEC2 UPROCRTOS class instance
    m_pSec2Rtos = std::make_unique<UprocSec2Rtos>(GetBoundGpuSubdevice());
    if (m_pSec2Rtos == nullptr)
    {
        Printf(Tee::PriHigh, "Could not create UprocSec2Rtos instance.\n");
        return RC::SOFTWARE_ERROR;
    }
    rc = m_pSec2Rtos->Initialize();
    if (OK != rc)
    {
        m_pSec2Rtos->Shutdown();
        Printf(Tee::PriHigh, "SEC2 UPROCRTOS not supported\n");
        CHECK_RC(rc);
    }

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
RC Sec2UprocRtosMutexTest::Run()
{
    RC rc;
    AcrVerify lsSec2State;

    CHECK_RC(lsSec2State.BindTestDevice(GetBoundTestDevice()));

    rc = m_pSec2Rtos->WaitUprocReady();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
               "%d:Sec2UprocRtosMutexTest::%s: Ucode OS is not ready. Skipping tests. \n",
                __LINE__,__FUNCTION__);

        // fail the test, we need to figure out why SEC2 bootstrap failed.
        CHECK_RC(RC::LWRM_STATUS_ERROR_NOT_READY);
        return rc;
    }

    if (lsSec2State.IsTestSupported() == RUN_RMTEST_TRUE)
    {
        lsSec2State.Setup();
        // Verify whether SEC2 is booted in the expected mode or not
        rc = lsSec2State.VerifyFalconStatus(LSF_FALCON_ID_SEC2, LW_TRUE);
        if (rc != OK)
        {
            Printf(Tee::PriHigh, "%d: Sec2UprocRtosMutexTest %s: SEC2 failed to boot in expected Mode\n",
                   __LINE__, __FUNCTION__);
            CHECK_RC(RC::SOFTWARE_ERROR);
        }
    }
    else
    {
        Printf(Tee::PriHigh, "%d: Sec2UprocRtosMutexTest %s: LS Falcon test not supported\n",
               __LINE__, __FUNCTION__);
        CHECK_RC(RC::SOFTWARE_ERROR);
    }

    // Run Mutex Test
    Printf(Tee::PriHigh,
           "%d:Sec2UprocRtosMutexTest::%s: Starts Mutex Test...\n",
            __LINE__,__FUNCTION__);

    CHECK_RC(m_pSec2Rtos->UprocRtosRunTests(UPROC_TEST_TYPE_MUTEX));

    return rc;
}

//! \brief Cleanup, shuts down SEC2 test instance.
//------------------------------------------------------------------------------
RC Sec2UprocRtosMutexTest::Cleanup()
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
JS_CLASS_INHERIT(Sec2UprocRtosMutexTest, RmTest, "SEC2 UPROCRTOS Mutex Test. \n");
