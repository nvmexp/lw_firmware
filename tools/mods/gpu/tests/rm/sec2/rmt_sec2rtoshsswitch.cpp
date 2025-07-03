
/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_sec2rtoshsswitch.cpp
//! \brief rmtest for SEC2 RTOS light swlwre/heavy secure switching. Also tests
//! the RM managed DMEM heap by allocating payload on the DMEM heap.
//!

#include "gpu/tests/rmtest.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/utility/sec2rtossub.h"

#include "gpu/tests/rm/memsys/rmt_acrverify.h"
#include "rmlsfm.h"

#include "class/cl9070.h"
#include "class/cl9270.h"
#include "class/cl9470.h"
#include "class/cl9570.h"
#include "class/clb6b9.h"
#include "ctrl/ctrlb6b9.h"

class Sec2RtosHsSwitchTest : public RmTest
{
public:
    Sec2RtosHsSwitchTest();
    virtual ~Sec2RtosHsSwitchTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    Sec2Rtos     *m_pSec2Rtos;
    RC            Sec2WritePrivProtectedReg(UINT32 i, UINT32 val);
};

//! \brief Sec2RtosHsSwitchTest constructor
//!
//! Doesn't do much except SetName.
//!
//! \sa Setup
//------------------------------------------------------------------------------
Sec2RtosHsSwitchTest::Sec2RtosHsSwitchTest()
{
    SetName("Sec2RtosHsSwitchTest");
}

//! \brief Sec2RtosHsSwitchTest destructor
//!
//! Doesn't do much.
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
Sec2RtosHsSwitchTest::~Sec2RtosHsSwitchTest()
{
}

//! \brief IsTestSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string Sec2RtosHsSwitchTest::IsTestSupported()
{
    LwRmPtr m_pLwRm;
    const RegHal &regs  = GetBoundGpuSubdevice()->Regs();
    UINT32  regVal      = regs.Read32(MODS_PSEC_SCP_CTL_STAT);
    LwBool  bDebugBoard = !regs.TestField(regVal, MODS_PSEC_SCP_CTL_STAT_DEBUG_MODE_DISABLED);
    RC      rc;

    if (m_pLwRm->IsClassSupported(MAXWELL_SEC2, GetBoundGpuDevice()) != true)
    {
        return "[SEC2 RTOS HS Switch RMTest] : MAXWELL_SEC2 is not supported";
    }

    if (!bDebugBoard)
    {
        return "[SEC2 RTOS HS Switch RMTest] : Not supported in prod mode";
    }

    // Get SEC2 RTOS class instance
    rc = (GetBoundGpuSubdevice()->GetSec2Rtos(&m_pSec2Rtos));
    if (OK != rc)
    {
        return "[SEC2 RTOS RT Timer RMTest] : SEC2 RTOS not supported";
    }

    if (!m_pSec2Rtos->CheckTestSupported(RM_SEC2_CMD_TYPE(TEST, WR_PRIV_PROTECTED_REG)))
    {
        return "[SEC2 RTOS HS Switch RMTest] : Test is not supported";
    }

    return RUN_RMTEST_TRUE;
}

//! \brief Do Init from Js
//!
//! \return OK if no error happens, otherwise corresponding rc
//------------------------------------------------------------------------------
RC Sec2RtosHsSwitchTest::Setup()
{
    RC rc;

    CHECK_RC(InitFromJs());
    return OK;
}

//! \brief Run the test
//!
//! Make sure SEC2 is bootstrapped.  Then run test items.
//!
//! \return OK all tests pass, else corresponding rc
//------------------------------------------------------------------------------
RC Sec2RtosHsSwitchTest::Run()
{
    RC rc;
    AcrVerify lsSec2State;

    CHECK_RC(lsSec2State.BindTestDevice(GetBoundTestDevice()));

    rc = m_pSec2Rtos->WaitSEC2Ready();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
               "%d:Sec2RtosHsSwitchTest::%s: Ucode OS is not ready. Skipping tests. \n",
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
            Printf(Tee::PriHigh, "%d: Sec2RtosHsSwitchTest %s: SEC2 failed to boot in expected Mode\n",
                   __LINE__, __FUNCTION__);
            CHECK_RC(RC::SOFTWARE_ERROR);
        }
    }
    else
    {
        Printf(Tee::PriHigh, "%d: Sec2RtosHsSwitchTest %s: LS Falcon test not supported\n",
               __LINE__, __FUNCTION__);
        CHECK_RC(RC::SOFTWARE_ERROR);
    }

    // Run HS Switch Test
    Printf(Tee::PriHigh,
           "%d: Sec2RtosHsSwitchTest::%s: Starts HS Switch Test...\n",
            __LINE__,__FUNCTION__);

    for (int i = 0; i < 2; i++)
    {
        CHECK_RC(Sec2WritePrivProtectedReg(i, 0xbeefbeef));
    }

    return rc;
}

//! \brief Sec2WritePrivProtectedReg()
//!
//! \param i          : Type of priv protected register to write (supported: 0/1)
//! \param val        : Value to write to register
//!
//! \return OK if all the tests passed, specific RC if failed
//-----------------------------------------------------------------------------
RC Sec2RtosHsSwitchTest::Sec2WritePrivProtectedReg(UINT32 i, UINT32 val)
{
    RC                      rc = OK;
    RM_FLCN_CMD_SEC2        sec2Cmd;
    RM_FLCN_MSG_SEC2        sec2Msg;
    UINT32                  seqNum;
    UINT32                  ret;

    memset(&sec2Cmd, 0, sizeof(RM_FLCN_CMD_SEC2));
    memset(&sec2Msg, 0, sizeof(RM_FLCN_MSG_SEC2));

    //
    // Send an empty command to SEC2.  The NULL command does nothing but
    // make SEC2 returns corresponding message.
    //
    sec2Cmd.hdr.unitId            = RM_SEC2_UNIT_TEST;
    sec2Cmd.hdr.size              =
        RM_SEC2_CMD_SIZE(TEST, WR_PRIV_PROTECTED_REG);
    sec2Cmd.cmd.sec2Test.cmdType  =
        RM_SEC2_CMD_TYPE(TEST, WR_PRIV_PROTECTED_REG);
    sec2Cmd.cmd.sec2Test.wrPrivProtectedReg.val = val;

    switch(i)
    {
        case 0:
            sec2Cmd.cmd.sec2Test.wrPrivProtectedReg.regType =
                PRIV_PROTECTED_REG_I2CS_SCRATCH;
            break;
        case 1:
            sec2Cmd.cmd.sec2Test.wrPrivProtectedReg.regType =
                PRIV_PROTECTED_REG_SELWRE_SCRATCH;
            break;
        default:
            return RC::SOFTWARE_ERROR;
    }

    Printf(Tee::PriHigh,
           "%d:Sec2RtosHsSwitchTest::%s: Submitting a command to SEC2 to switch "
           "to heavy secure mode and write priv protected register...\n",
            __LINE__, __FUNCTION__);

    // submit the command
    rc = m_pSec2Rtos->Command(&sec2Cmd,
                              &sec2Msg,
                              &seqNum);
    CHECK_RC(rc);

    ret = sec2Msg.msg.sec2Test.wrPrivProtectedReg.val;
    if (ret != val)
    {
        Printf(Tee::PriHigh,
               "%d:Sec2RtosHsSwitchTest::%s: Value of priv protected "
               "register doesn't match value expected. Returned val = 0x%x, "
               "expected val = 0x%x, loop %d\n",
               __LINE__, __FUNCTION__, ret, val, (i+1));
        return RC::SOFTWARE_ERROR;
    }

    Printf(Tee::PriHigh,
           "%d:Sec2RtosHsSwitchTest::%s: Returned value from priv protected "
           "register matches expected value.\n",
            __LINE__, __FUNCTION__);
    return rc;
}

//! \brief Cleanup, does nothing for this test
//------------------------------------------------------------------------------
RC Sec2RtosHsSwitchTest::Cleanup()
{
    m_pSec2Rtos = NULL;
    return OK;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(Sec2RtosHsSwitchTest, RmTest, "SEC2 RTOS Basic Test. \n");
