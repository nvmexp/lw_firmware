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
//! \file rmt_Sec2RtosRtTimer.cpp
//! \brief rmtest for SEC2
//!

#include "gpu/tests/rmtest.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/utility/sec2rtossub.h"

#include "gpu/tests/rm/memsys/rmt_acrverify.h"
#include "rmlsfm.h"

#include "pascal/gp102/dev_sec_pri.h"
#include "core/include/platform.h"

#include "class/clb6b9.h"
#include "ctrl/ctrlb6b9.h"

#define IsGP102orBetter(p)   ((p >= Gpu::GP102))

/* Default timer count. Written to the LW_CSEC_TIMER_0_INTERVAL register */
#define RTTIMER_TEST_DEFAULT_COUNT      (1000)
#define RTTIMER_TEST_NUM_PASSES         (5)

class Sec2RtosRtTimerTest : public RmTest
{
public:
    Sec2RtosRtTimerTest();
    virtual ~Sec2RtosRtTimerTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    Sec2Rtos *m_pSec2Rtos;
    RC        Sec2RtosRtTimerTestCmdAndMsg(UINT32 count);
};

//! \brief Sec2RtosRtTimerTest constructor
//!
//! Doesn't do much except SetName.
//!
//! \sa Setup
//------------------------------------------------------------------------------
Sec2RtosRtTimerTest::Sec2RtosRtTimerTest()
{
    SetName("Sec2RtosRtTimerTest");
}

//! \brief Sec2RtosRtTimerTest destructor
//!
//! Doesn't do much.
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
Sec2RtosRtTimerTest::~Sec2RtosRtTimerTest()
{
}

//! \brief IsTestSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string Sec2RtosRtTimerTest::IsTestSupported()
{
    const RegHal &regs       = GetBoundGpuSubdevice()->Regs();
    UINT32  regVal           = regs.Read32(MODS_PSEC_SCP_CTL_STAT);
    Gpu::LwDeviceId chipName = GetBoundGpuSubdevice()->DeviceId();
    LwBool bDebugBoard       = !regs.TestField(regVal, MODS_PSEC_SCP_CTL_STAT_DEBUG_MODE_DISABLED);
    RC rc;

    if (Platform::GetSimulationMode() == Platform::Fmodel)
    {
        return "[SEC2 RTOS RT Timer RMTest] : Not supported on FMODEL";
    }

    if (!(IsClassSupported(MAXWELL_SEC2) && IsGP102orBetter(chipName)))
    {
        return "[SEC2 RTOS RMTest] : Supported only on GP10X (>= GP102) chips with SEC2 RTOS";
    }

    if (!bDebugBoard)
    {
        return "[SEC2 RTOS RT Timer RMTest] : Not supported in prod mode";
    }

    // Get SEC2 RTOS class instance
    rc = (GetBoundGpuSubdevice()->GetSec2Rtos(&m_pSec2Rtos));
    if (OK != rc)
    {
        return "[SEC2 RTOS RT Timer RMTest] : SEC2 RTOS not supported";
    }

    if (!m_pSec2Rtos->CheckTestSupported(RM_SEC2_CMD_TYPE(TEST, RTTIMER_TEST)))
    {
        return "[SEC2 RTOS RT Timer RMTest] : Test is not supported";
    }

    return RUN_RMTEST_TRUE;
}

//! \brief Do Init from Js
//!
//! \return OK if no error happens, otherwise corresponding rc
//------------------------------------------------------------------------------
RC Sec2RtosRtTimerTest::Setup()
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
RC Sec2RtosRtTimerTest::Run()
{
    RC rc;
    AcrVerify lsSec2State;

    CHECK_RC(lsSec2State.BindTestDevice(GetBoundTestDevice()));

    rc = m_pSec2Rtos->WaitSEC2Ready();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
               "%d:Sec2RtosRtTimerTest::%s: Ucode OS is not ready. Skipping tests. \n",
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
            Printf(Tee::PriHigh, "%d: Sec2RtosRtTimerTest %s: SEC2 failed to boot in expected Mode\n",
                   __LINE__, __FUNCTION__);
            CHECK_RC(RC::SOFTWARE_ERROR);
        }
    }
    else
    {
        Printf(Tee::PriHigh, "%d: Sec2RtosRtTimerTest %s: LS Falcon test not supported\n",
               __LINE__, __FUNCTION__);
        CHECK_RC(RC::SOFTWARE_ERROR);
    }

    Printf(Tee::PriHigh,
           "%d:Sec2RtosRtTimerTest::%s: Starts RT Timer Test...\n",
            __LINE__,__FUNCTION__);

    CHECK_RC(Sec2RtosRtTimerTestCmdAndMsg(RTTIMER_TEST_DEFAULT_COUNT));

    return rc;
}

//! \brief Sec2RtosRtTimerTestCmdAndMsg()
//!
//! This function sends an RT TIMER test cmd, waits for the msg back
//! and checks that the test passed.
//!
//! \return OK if the RT Timer test passed, SOFTWARE_ERROR if failed
//! \sa Run()
//-----------------------------------------------------------------------------
RC Sec2RtosRtTimerTest::Sec2RtosRtTimerTestCmdAndMsg(UINT32 count)
{
    RC                      rc = OK;
    RM_FLCN_CMD_SEC2        sec2Cmd;
    RM_FLCN_MSG_SEC2        sec2Msg;
    UINT32                  seqNum;
    UINT32                  i;

    memset(&sec2Cmd, 0, sizeof(RM_FLCN_CMD_SEC2));
    memset(&sec2Msg, 0, sizeof(RM_FLCN_MSG_SEC2));

    /* Set the cmd members */
    sec2Cmd.hdr.unitId            = RM_SEC2_UNIT_TEST;
    sec2Cmd.hdr.size              =
                RM_SEC2_CMD_SIZE(TEST, RTTIMER_TEST);
    sec2Cmd.cmd.sec2Test.cmdType  =
        RM_SEC2_CMD_TYPE(TEST, RTTIMER_TEST);

    sec2Cmd.cmd.sec2Test.rttimer.count  = count;

    /* Don't check for timing accuracy on fmodel */
    sec2Cmd.cmd.sec2Test.rttimer.bCheckTime =
       (Platform::GetSimulationMode() != Platform::Fmodel);

    Printf(Tee::PriHigh,
           "%d:Sec2RtosRtTimerTest::%s: Set count to %d. Src is PTimer_1US. Timing verification is %s\n",
            __LINE__, __FUNCTION__,
            sec2Cmd.cmd.sec2Test.rttimer.count,
            sec2Cmd.cmd.sec2Test.rttimer.bCheckTime ? "on" : "off");

    for (i = 1; i <= RTTIMER_TEST_NUM_PASSES; i++)
    {
        Printf(Tee::PriHigh,
           "%d:Sec2RtosRtTimerTest::%s: Starting iteration %d\n",
            __LINE__, __FUNCTION__, i);
        // submit the command
        rc = m_pSec2Rtos->Command(&sec2Cmd,
                                  &sec2Msg,
                                  &seqNum);
        CHECK_RC(rc);

        Printf(Tee::PriHigh,
               "%d:Sec2RtosRtTimerTest::%s: CMD submission success, got seqNum = %u\n",
                __LINE__, __FUNCTION__, seqNum);

        // Check for the correct unitID
        if ((sec2Msg.hdr.unitId != RM_SEC2_UNIT_TEST) ||
            (sec2Msg.msg.sec2Test.msgType != RM_SEC2_TEST_CMD_ID_RTTIMER_TEST))
        {
            Printf(Tee::PriHigh,
           "%d:Sec2RtosRtTimerTest::%s: Wrong Unit or Test ID! Expected 0x%X 0x%X. Got 0x%X 0x%X\n",
                __LINE__, __FUNCTION__,
                RM_SEC2_UNIT_TEST, sec2Msg.hdr.unitId,
                RM_SEC2_TEST_CMD_ID_RTTIMER_TEST, sec2Msg.msg.sec2Test.msgType);
            rc = RC::SOFTWARE_ERROR;
        }
        // Check if it passed
        if (sec2Msg.msg.sec2Test.rttimer.status != RM_SEC2_TEST_MSG_STATUS_OK)
        {
            rc = RC::SOFTWARE_ERROR;
        }
        Printf(Tee::PriHigh,
           "%d:Sec2RtosRtTimerTest::%s: Iteration %d %s\n",
            __LINE__, __FUNCTION__,
            i, (rc == OK) ? "PASSED" : "FAILED");
        Printf(Tee::PriHigh,
            "%d:Sec2RtosRtTimerTest::%s: OneShot mode took %d ns, Continuous mode took %d ns\n",
            __LINE__, __FUNCTION__,
            sec2Msg.msg.sec2Test.rttimer.oneShotNs,
            sec2Msg.msg.sec2Test.rttimer.continuousNs);
        CHECK_RC(rc);
    }
    return rc;
}

//! \brief Cleanup, does nothing for this test
//------------------------------------------------------------------------------
RC Sec2RtosRtTimerTest::Cleanup()
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
JS_CLASS_INHERIT(Sec2RtosRtTimerTest, RmTest, "SEC2 RTOS RT Timer Test. \n");

