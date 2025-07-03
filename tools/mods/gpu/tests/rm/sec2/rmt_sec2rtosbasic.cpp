/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_Sec2RtosBasic.cpp
//! \brief rmtest for SEC2
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

class Sec2RtosBasicTest : public RmTest
{
public:
    Sec2RtosBasicTest();
    virtual ~Sec2RtosBasicTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    Sec2Rtos     *m_pSec2Rtos;
    RC            Sec2RtosBasicTestCmdAndMsgQTest();
};

//! \brief Sec2RtosBasicTest constructor
//!
//! Doesn't do much except SetName.
//!
//! \sa Setup
//------------------------------------------------------------------------------
Sec2RtosBasicTest::Sec2RtosBasicTest()
{
    SetName("Sec2RtosBasicTest");
}

//! \brief Sec2RtosBasicTest destructor
//!
//! Doesn't do much.
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
Sec2RtosBasicTest::~Sec2RtosBasicTest()
{
}

//! \brief IsTestSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string Sec2RtosBasicTest::IsTestSupported()
{
    LwRmPtr      m_pLwRm;

    if (m_pLwRm->IsClassSupported(MAXWELL_SEC2, GetBoundGpuDevice()) == true )
    {
        return RUN_RMTEST_TRUE;
    }

    return "[SEC2 RTOS RMTest] : MAXWELL_SEC2 is not supported on current platform";
}

//! \brief Do Init from Js
//!
//! \return OK if no error happens, otherwise corresponding rc
//------------------------------------------------------------------------------
RC Sec2RtosBasicTest::Setup()
{
    RC rc;

    // Get SEC2 RTOS class instance
    rc = (GetBoundGpuSubdevice()->GetSec2Rtos(&m_pSec2Rtos));
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "SEC2 RTOS not supported\n");
        CHECK_RC(rc);
    }

    CHECK_RC(InitFromJs());
    return OK;
}

//! \brief Run the test
//!
//! Make sure SEC2 is bootstrapped.  Then run test items.
//!
//! \return OK all tests pass, else corresponding rc
//------------------------------------------------------------------------------
RC Sec2RtosBasicTest::Run()
{
    RC rc;
    AcrVerify lsSec2State;

    CHECK_RC(lsSec2State.BindTestDevice(GetBoundTestDevice()));

    rc = m_pSec2Rtos->WaitSEC2Ready();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
               "%d:Sec2RtosBasicTest::%s: Ucode OS is not ready. Skipping tests. \n",
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
            Printf(Tee::PriHigh, "%d: Sec2RtosBasicTest %s: SEC2 failed to boot in expected Mode\n",
                   __LINE__, __FUNCTION__);
            CHECK_RC(RC::SOFTWARE_ERROR);
        }
    }
    else
    {
        Printf(Tee::PriHigh, "%d: Sec2RtosBasicTest %s: LS Falcon test not supported\n",
               __LINE__, __FUNCTION__);
        CHECK_RC(RC::SOFTWARE_ERROR);
    }

    // Run Cmd & Msg Queue Test
    Printf(Tee::PriHigh,
           "%d:Sec2RtosBasicTest::%s: Starts Cmd/Msg Queue Test...\n",
            __LINE__,__FUNCTION__);

    CHECK_RC(Sec2RtosBasicTestCmdAndMsgQTest());

    return rc;
}

//! \brief Sec2RtosBasicTestCmdAndMsgQTest()
//!
//! This function runs the following tests as per the specified order
//!   a. Sends an empty command to SEC2.
//!   b. Waits for the SEC2 message in response to the command
//!   c. Prints the received message.
//!
//! \return OK if all the tests passed, specific RC if failed
//! \sa Run()
//-----------------------------------------------------------------------------
RC Sec2RtosBasicTest::Sec2RtosBasicTestCmdAndMsgQTest()
{
    RC                      rc = OK;
    RM_FLCN_CMD_SEC2        sec2Cmd;
    RM_FLCN_MSG_SEC2        sec2Msg;
    UINT32                  seqNum;

    memset(&sec2Cmd, 0, sizeof(RM_FLCN_CMD_SEC2));
    memset(&sec2Msg, 0, sizeof(RM_FLCN_MSG_SEC2));

    //
    // Send an empty command to SEC2.  The NULL command does nothing but
    // make SEC2 returns corresponding message.
    //
    sec2Cmd.hdr.unitId  = RM_SEC2_UNIT_NULL;
    sec2Cmd.hdr.size    = RM_FLCN_QUEUE_HDR_SIZE;

    for (int i=0; i<10; ++i) {
        Printf(Tee::PriHigh,
               "%d:Sec2RtosBasicTest::%s: Submit a command to SEC2...\n",
                __LINE__, __FUNCTION__);

        // submit the command
        rc = m_pSec2Rtos->Command(&sec2Cmd,
                                  &sec2Msg,
                                  &seqNum);
        CHECK_RC(rc);

        Printf(Tee::PriHigh,
               "%d:Sec2RtosBasicTest::%s: CMD submission success, got seqNum = %u\n",
                __LINE__, __FUNCTION__, seqNum);

        //
        // In function SEC2::Command(), it waits message back if argument pMsg is
        // not NULL
        //

        // response message received, print out the details
        Printf(Tee::PriHigh,
               "%d:Sec2RtosBasicTest::%s: Received command response from SEC2\n",
                __LINE__, __FUNCTION__);
        Printf(Tee::PriHigh,
               "%d:Sec2RtosBasicTest::%s: Printing Header :-\n",
                __LINE__, __FUNCTION__);
        Printf(Tee::PriHigh,
               "unitID = 0x%x ,size = 0x%x ,ctrlFlags  = 0x%x ,seqNumID = 0x%x\n",
               sec2Msg.hdr.unitId, sec2Msg.hdr.size,
               sec2Msg.hdr.ctrlFlags, sec2Msg.hdr.seqNumId);
    }

    return rc;
}

//! \brief Cleanup, does nothing for this test
//------------------------------------------------------------------------------
RC Sec2RtosBasicTest::Cleanup()
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
JS_CLASS_INHERIT(Sec2RtosBasicTest, RmTest, "SEC2 RTOS Basic Test. \n");

