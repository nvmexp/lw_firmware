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
//! \file rmt_GspUprocRtosMutex.cpp
//! \brief rmtest for GSP mutexes
//!

#include "gpu/tests/rmtest.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/tests/rm/utility/uprocrtos.h"
#include "gpu/tests/rm/utility/uprocgsprtos.h"
#include "mods_reg_hal.h"

#include "gpu/tests/rm/memsys/rmt_acrverify.h"
#include "rmlsfm.h"

#include "class/cl9070.h"
#include "class/cl9270.h"
#include "class/cl9470.h"
#include "class/cl9570.h"
#include "class/clc310.h"
#include "ctrl/ctrlc310.h"

class GspUprocRtosMutexTest : public RmTest
{
public:
    GspUprocRtosMutexTest();
    virtual ~GspUprocRtosMutexTest()
    {
    };
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    unique_ptr<UprocRtos>      m_pGspRtos;
};

//! \brief GspUprocRtosMutexTest constructor
//!
//! Doesn't do much except SetName.
//!
//! \sa Setup
//------------------------------------------------------------------------------
GspUprocRtosMutexTest::GspUprocRtosMutexTest()
{
    SetName("GspRtosMutexTest");
}

//! \brief IsTestSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string GspUprocRtosMutexTest::IsTestSupported()
{
    Gpu::LwDeviceId chipName = GetBoundGpuSubdevice()->DeviceId();
    if (IsClassSupported(VOLTA_GSP) && (chipName >= Gpu::GV100))
    {
        return RUN_RMTEST_TRUE;
    }
    return "[GSP UPROC RTOS RMTest] : Supported only on GV100+ chips with GSP RTOS";
}

//! \brief Do Init from Js
//!
//! \return OK if no error happens, otherwise corresponding rc
//------------------------------------------------------------------------------
RC GspUprocRtosMutexTest::Setup()
{
    RC rc;

    // Get GSP UPROC RTOS class instance
    m_pGspRtos = std::make_unique<UprocGspRtos>(GetBoundGpuSubdevice());
    if (m_pGspRtos == nullptr)
    {
        Printf(Tee::PriHigh, "Could not create UprocGspRtos instance.\n");
        return RC::SOFTWARE_ERROR;
    }
    rc = m_pGspRtos->Initialize();
    if (OK != rc)
    {
        m_pGspRtos->Shutdown();
        Printf(Tee::PriHigh, "GSP UPROC RTOS not supported\n");
        CHECK_RC(rc);
    }

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
RC GspUprocRtosMutexTest::Run()
{
    RC rc;
#if GSPUPROCRTOS_USE_ACR
    // Please refer to uprocgsprtos.h for why ACR is disabled for GSP tests.
    AcrVerify lsGspState;
#endif

    rc = m_pGspRtos->WaitUprocReady();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
               "%d:GspUprocRtosMutexTest::%s: Ucode OS is not ready. Skipping tests. \n",
                __LINE__,__FUNCTION__);

        // fail the test, we need to figure out why GSP bootstrap failed.
        CHECK_RC(RC::LWRM_STATUS_ERROR_NOT_READY);
        return rc;
    }
#if GSPUPROCRTOS_USE_ACR
    if (lsGspState.IsTestSupported() == RUN_RMTEST_TRUE)
    {
        lsGspState.Setup();
        // Verify whether GSP is booted in the expected mode or not
        rc = lsGspState.VerifyFalconStatus(LSF_FALCON_ID_DPU, LW_TRUE);
        if (rc != OK)
        {
            Printf(Tee::PriHigh, "%d: GspUprocRtosMutexTest %s: GSP failed to boot in expected Mode\n",
                   __LINE__, __FUNCTION__);
            CHECK_RC(RC::SOFTWARE_ERROR);
        }
    }
    else
    {
        Printf(Tee::PriHigh, "%d: GspUprocRtosMutexTest %s: LS Falcon test not supported\n",
               __LINE__, __FUNCTION__);
        CHECK_RC(RC::SOFTWARE_ERROR);
    }
#else
    Printf(Tee::PriHigh,
        "%d:GspUprocRtosMutexTest::%s: AcrVerify skipped! \n",
        __LINE__,__FUNCTION__);
#endif
    // Run Mutex Test
    Printf(Tee::PriHigh,
           "%d:GspUprocRtosMutexTest::%s: Starts Mutex Test...\n",
            __LINE__,__FUNCTION__);

    CHECK_RC(m_pGspRtos->UprocRtosRunTests(UPROC_TEST_TYPE_MUTEX));

    return rc;
}

//! \brief Cleanup, shuts down GSP test instance.
//------------------------------------------------------------------------------
RC GspUprocRtosMutexTest::Cleanup()
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
JS_CLASS_INHERIT(GspUprocRtosMutexTest, RmTest, "GSP UPROCRTOS Mutex Test. \n");
