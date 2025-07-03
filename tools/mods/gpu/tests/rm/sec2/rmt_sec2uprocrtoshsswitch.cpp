
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
//! \file rmt_sec2uprocrtoshsswitch.cpp
//! \brief rmtest for SEC2 RTOS light swlwre/heavy secure switching. Also tests
//! the RM managed DMEM heap by allocating payload on the DMEM heap.
//!

#include "gpu/tests/rmtest.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/tests/rm/utility/uprocrtos.h"
#include "gpu/tests/rm/utility/uprocsec2rtos.h"
#include "rmsec2cmdif.h"
#include "gpu/tests/rm/memsys/rmt_acrverify.h"
#include "rmlsfm.h"
#include "class/cl9070.h"
#include "class/cl9270.h"
#include "class/cl9470.h"
#include "class/cl9570.h"
#include "class/clb6b9.h"
#include "ctrl/ctrlb6b9.h"
#include "uprocifcmn.h"

class Sec2UprocRtosHsSwitchTest : public RmTest
{
public:
    Sec2UprocRtosHsSwitchTest();
    virtual ~Sec2UprocRtosHsSwitchTest()
    {
    };
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    unique_ptr<UprocRtos>      m_pSec2Rtos;
};

//! \brief Sec2UprocRtosHsSwitchTest constructor
//!
//! Doesn't do much except SetName.
//!
//! \sa Setup
//------------------------------------------------------------------------------
Sec2UprocRtosHsSwitchTest::Sec2UprocRtosHsSwitchTest()
{
    SetName("Sec2RtosHsSwitchTest");
}

//! \brief IsTestSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string Sec2UprocRtosHsSwitchTest::IsTestSupported()
{
    LwRmPtr m_pLwRm;
    const RegHal &regs  = GetBoundGpuSubdevice()->Regs();
    UINT32  regVal      = regs.Read32(MODS_PSEC_SCP_CTL_STAT);
    LwBool  bProdBoard  = regs.TestField(regVal, MODS_PSEC_SCP_CTL_STAT_DEBUG_MODE_DISABLED);
    RC      rc;

    if (m_pLwRm->IsClassSupported(MAXWELL_SEC2, GetBoundGpuDevice()) != true)
    {
        return "[SEC2Uproc RTOS HS Switch RMTest] : MAXWELL_SEC2 is not supported";
    }

    if (bProdBoard)
    {
        return "[SEC2Uproc RTOS HS Switch RMTest] : Not supported in prod mode";
    }

    // Get SEC2 RTOS class instance
    m_pSec2Rtos = std::make_unique<UprocSec2Rtos>(GetBoundGpuSubdevice());
    if (m_pSec2Rtos == nullptr)
    {
        return "Could not create UprocSec2Rtos instance.";
    }
    rc = m_pSec2Rtos->Initialize();
    if (OK != rc)
    {
        m_pSec2Rtos->Shutdown();
        return "[SEC2Uproc RTOS HS Switch RMTest] : SEC2 RTOS not supported";
    }

    if (!m_pSec2Rtos->CheckTestSupported(RM_UPROC_CMD_TYPE(TEST, WR_PRIV_PROTECTED_REG)))
    {
        return "[SEC2Uproc RTOS HS Switch RMTest] : Test is not supported";
    }

    return RUN_RMTEST_TRUE;
}

//! \brief Do Init from Js
//!
//! \return OK if no error happens, otherwise corresponding rc
//------------------------------------------------------------------------------
RC Sec2UprocRtosHsSwitchTest::Setup()
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
RC Sec2UprocRtosHsSwitchTest::Run()
{
    RC rc;
    AcrVerify lsSec2State;

    CHECK_RC(lsSec2State.BindTestDevice(GetBoundTestDevice()));

    rc = m_pSec2Rtos->WaitUprocReady();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
               "%d:Sec2UprocRtosHsSwitchTest::%s: Ucode OS is not ready. Skipping tests. \n",
                __LINE__,__FUNCTION__);

        // fail the test, we need to figure out why SEC2 bootstrap failed.
        CHECK_RC(RC::LWRM_STATUS_ERROR_NOT_READY);
        return rc;
    }

    if (lsSec2State.IsTestSupported() == RUN_RMTEST_TRUE)
    {
        lsSec2State.Setup();
        // Verify whether SEC2 is booted in LS mode
        rc = lsSec2State.VerifyFalconStatus(LSF_FALCON_ID_SEC2, LW_TRUE);
        if (rc != OK)
        {
            Printf(Tee::PriHigh, "%d: Sec2UprocRtosHsSwitchTest %s: SEC2 failed to boot in expected Mode\n",
                   __LINE__, __FUNCTION__);
            CHECK_RC(RC::SOFTWARE_ERROR);
        }
    }
    else
    {
        Printf(Tee::PriHigh, "%d: Sec2UprocRtosHsSwitchTest %s: LS Falcon test not supported\n",
               __LINE__, __FUNCTION__);
        CHECK_RC(RC::SOFTWARE_ERROR);
    }

    // Run HS Switch Test
    Printf(Tee::PriHigh,
           "%d: Sec2UprocRtosHsSwitchTest::%s: Starts HS Switch Test...\n",
            __LINE__,__FUNCTION__);

    CHECK_RC(m_pSec2Rtos->UprocRtosRunTests(UPROC_TEST_TYPE_SWITCH));

    return rc;
}

//! \brief Cleanup, shuts down SEC2 test instance.
//------------------------------------------------------------------------------
RC Sec2UprocRtosHsSwitchTest::Cleanup()
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
JS_CLASS_INHERIT(Sec2UprocRtosHsSwitchTest, RmTest, "SEC2 UPROCRTOS HSSwitch Test. \n");
