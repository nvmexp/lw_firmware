
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
//! \file rmt_gspuprocrtoshsswitch.cpp
//! \brief rmtest for GSP RTOS light swlwre/heavy secure switching. Also tests
//! the RM managed DMEM heap by allocating payload on the DMEM heap.
//!

#include "gpu/tests/rmtest.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/tests/rm/utility/uprocrtos.h"
#include "gpu/tests/rm/utility/uprocgsprtos.h"
#include "rmgspcmdif.h"
#include "gpu/tests/rm/memsys/rmt_acrverify.h"
#include "rmlsfm.h"
#include "class/cl9070.h"
#include "class/cl9270.h"
#include "class/cl9470.h"
#include "class/cl9570.h"
#include "class/clc310.h"
#include "ctrl/ctrlc310.h"
#include "uprocifcmn.h"

class GspUprocRtosHsSwitchTest : public RmTest
{
public:
    GspUprocRtosHsSwitchTest();
    virtual ~GspUprocRtosHsSwitchTest()
    {
    };
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    unique_ptr<UprocRtos>      m_pGspRtos;
};

//! \brief GspUprocRtosHsSwitchTest constructor
//!
//! Doesn't do much except SetName.
//!
//! \sa Setup
//------------------------------------------------------------------------------
GspUprocRtosHsSwitchTest::GspUprocRtosHsSwitchTest()
{

}

//! \brief IsTestSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string GspUprocRtosHsSwitchTest::IsTestSupported()
{
    LwRmPtr m_pLwRm;
    const RegHal &regs  = GetBoundGpuSubdevice()->Regs();
    UINT32  regVal      = regs.Read32(MODS_PGSP_SCP_CTL_STAT);
    LwBool  bProdBoard  = regs.TestField(regVal, MODS_PGSP_SCP_CTL_STAT_DEBUG_MODE_DISABLED);
    RC      rc;

    if (m_pLwRm->IsClassSupported(VOLTA_GSP, GetBoundGpuDevice()) != true)
    {
        return "[GSP Uproc RTOS HS Switch RMTest] : VOLTA_GSP is not supported";
    }

    if (bProdBoard)
    {
        return "[GSP Uproc RTOS HS Switch RMTest] : Not supported in prod mode";
    }

    // Get GSP RTOS class instance
    m_pGspRtos = std::make_unique<UprocGspRtos>(GetBoundGpuSubdevice());
    if (m_pGspRtos == nullptr)
    {
        return "Could not create UprocGspRtos instance.";
    }
    rc = m_pGspRtos->Initialize();
    if (OK != rc)
    {
        m_pGspRtos->Shutdown();
        return "[GSP Uproc RTOS HS Switch RMTest] : GSP RTOS not supported";
    }

    if (!m_pGspRtos->CheckTestSupported(RM_UPROC_CMD_TYPE(TEST, WR_PRIV_PROTECTED_REG)))
    {
        return "[GSP Uproc RTOS HS Switch RMTest] : Test is not supported";
    }

    return RUN_RMTEST_TRUE;
}

//! \brief Do Init from Js
//!
//! \return OK if no error happens, otherwise corresponding rc
//------------------------------------------------------------------------------
RC GspUprocRtosHsSwitchTest::Setup()
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
RC GspUprocRtosHsSwitchTest::Run()
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
               "%d:GspUprocRtosHsSwitchTest::%s: Ucode OS is not ready. Skipping tests. \n",
                __LINE__,__FUNCTION__);

        // fail the test, we need to figure out why GSP bootstrap failed.
        CHECK_RC(RC::LWRM_STATUS_ERROR_NOT_READY);
        return rc;
    }
#if GSPUPROCRTOS_USE_ACR
    if (lsGspState.IsTestSupported() == RUN_RMTEST_TRUE)
    {
        lsGspState.Setup();
        // Verify whether GSP is booted in LS mode
        rc = lsGspState.VerifyFalconStatus(LSF_FALCON_ID_DPU, LW_TRUE);
        if (rc != OK)
        {
            Printf(Tee::PriHigh, "%d: GspUprocRtosHsSwitchTest %s: GSP failed to boot in expected Mode\n",
                   __LINE__, __FUNCTION__);
            CHECK_RC(RC::SOFTWARE_ERROR);
        }
    }
    else
    {
        Printf(Tee::PriHigh, "%d: GspUprocRtosHsSwitchTest %s: LS Falcon test not supported\n",
               __LINE__, __FUNCTION__);
        CHECK_RC(RC::SOFTWARE_ERROR);
    }
#else
Printf(Tee::PriHigh,
       "%d:GspUprocRtosHsSwitchTest::%s: AcrVerify skipped! \n",
       __LINE__,__FUNCTION__);
#endif
    // Run HS Switch Test
    Printf(Tee::PriHigh,
           "%d: GspUprocRtosHsSwitchTest::%s: Starts HS Switch Test...\n",
            __LINE__,__FUNCTION__);

    CHECK_RC(m_pGspRtos->UprocRtosRunTests(UPROC_TEST_TYPE_SWITCH));

    return rc;
}

//! \brief Cleanup, shuts down GSP test instance
//------------------------------------------------------------------------------
RC GspUprocRtosHsSwitchTest::Cleanup()
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
JS_CLASS_INHERIT(GspUprocRtosHsSwitchTest, RmTest, "GSP UPROCRTOS HSSwitch Test. \n");
