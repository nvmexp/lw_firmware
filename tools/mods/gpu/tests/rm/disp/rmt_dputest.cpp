/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2016,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_dputest.cpp
//! \brief rmtest for DPU
//!

#include "gpu/tests/rmtest.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/display/dpusub.h"

#include "gpu/tests/rm/memsys/rmt_acrverify.h"
#include "rmlsfm.h"

#include "class/cl9070.h"
#include "class/cl9170.h"
#include "class/cl9270.h"
#include "class/cl9470.h"
#include "class/cl9570.h"
#include "class/cl9770.h"
#include "ctrl/ctrl0073.h"

class DpuTest : public RmTest
{
public:
    DpuTest();
    virtual ~DpuTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    DPU            *m_pDpu;
    GpuSubdevice   *m_Parent;    // GpuSubdevice that owns this DPU

    RC              DpuTestCmdAndMsgQTest();
};

//! \brief DpuTest constructor
//!
//! Doesn't do much except SetName.
//!
//! \sa Setup
//------------------------------------------------------------------------------
DpuTest::DpuTest()
{
    SetName("DpuTest");
    m_pDpu      = nullptr;
    m_Parent    = nullptr;
}

//! \brief DpuTest destructor
//!
//! Doesn't do much.
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
DpuTest::~DpuTest()
{
}

//! \brief IsTestSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string DpuTest::IsTestSupported()
{
    Display    *pDisplay     = GetDisplay();
    UINT32      displayClass = pDisplay->GetClass();

    if ((pDisplay->GetDisplayClassFamily() == Display::EVO) &&
        (displayClass >= LW9070_DISPLAY))
    {
        return RUN_RMTEST_TRUE;
    }
    return "[DPU RMTest] : DPU is not supported on current platform";

}

//! \brief Do Init from Js, allocate m_pDpu and do initialization
//!
//! \return OK if no error happens, otherwise corresponding rc
//------------------------------------------------------------------------------
RC DpuTest::Setup()
{
    RC rc = OK;

    m_Parent = GetBoundGpuSubdevice();

    MASSERT(!m_pDpu);
    m_pDpu = new DPU(m_Parent);

    //
    // For PMU, the only instance m_pPmu is owned by GPU SubDevice
    // (gpusbdev.cpp).
    //
    // But DPU needs display handle LW04_DISPLAY_COMMON, and the
    // handle should only be allocated within a Test and needs to
    // be freed at the end of the Test. Otherwise, it causes failure
    // of other tests need LW04_DISPLAY_COMMON.  Therefore, unlike PMU,
    // m_pDpu is inside DpuTest and initialized in Setup().
    //

    rc = m_pDpu->Initialize();
    if (rc != OK)
    {
        m_pDpu->Shutdown();

        if (rc == RC::UNSUPPORTED_FUNCTION)
        {
            rc.Clear(); // Suppress this error.
        }
        else
        {
            return rc;
        }
    }

    CHECK_RC(InitFromJs());

    return rc;
}

//! \brief Run the test
//!
//! Make sure DPU is bootstrapped.  Then run test items.
//!
//! \return OK all tests pass, else corresponding rc
//------------------------------------------------------------------------------
RC DpuTest::Run()
{
    RC rc;
    Display *pDisplay = GetDisplay();
    AcrVerify lsDpuState;
    UINT32   lwrDpuUcodeState;

    CHECK_RC(lsDpuState.BindTestDevice(GetBoundTestDevice()));

    // Intializing Display HW
    CHECK_RC(pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    rc = m_pDpu->WaitDPUReady();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
               "%d:DpuTest::%s: Ucode OS is not ready. Skips all DPU tests. \n",
                __LINE__,__FUNCTION__);

        // fail the test, we need to figure out why DPU bootstrap failed.
        CHECK_RC(RC::LWRM_STATUS_ERROR_NOT_READY);
    }

    // Before each test, check the ucode state if the test needs OS to be alive
    CHECK_RC(m_pDpu->GetUcodeState(&lwrDpuUcodeState));
    if (lwrDpuUcodeState == LW0073_CTRL_DPU_UCODE_STATE_READY)
    {
        // Run Cmd & Msg Queue Test
        Printf(Tee::PriHigh,
               "%d:DpuTest::%s: Starts Cmd/Msg Queue Test...\n",
                __LINE__,__FUNCTION__);

        CHECK_RC(DpuTestCmdAndMsgQTest());
    }
    else
    {
        Printf(Tee::PriHigh,
               "%d:DpuTest::%s: Skips DpuTestCmdAndMsgQTest because ucode OS"
               "is not ready.\n",
                __LINE__,__FUNCTION__);
    }

    if (lsDpuState.IsTestSupported() == RUN_RMTEST_TRUE)
    {
        lsDpuState.Setup();
        // Verify whether DPU is booted in the expected mode or not
        rc = lsDpuState.VerifyFalconStatus(LSF_FALCON_ID_DPU, LW_FALSE);
        if(rc != OK)
        {
            Printf(Tee::PriHigh, "%d: DpuTest %s: DPU failed to boot in expected Mode\n",
                    __LINE__, __FUNCTION__);
        }
    }
    return rc;
}

//! \brief DpuTestCmdAndMsgQTest()
//!
//! This function runs the following tests as per the specified order
//!   a. Sends an empty command to DPU.
//!   b. Waits for the DPU message in response to the command
//!   c. Prints the received message.
//!
//! \return OK if all the tests passed, specific RC if failed
//! \sa Run()
//-----------------------------------------------------------------------------
RC DpuTest::DpuTestCmdAndMsgQTest()
{
    RC                      rc = OK;
    RM_DPU_COMMAND          dpuCmd;
    RM_FLCN_MSG_DPU         dpuMsg;
    UINT32                  seqNum;

    memset(&dpuCmd, 0, sizeof(RM_DPU_COMMAND));
    memset(&dpuMsg, 0, sizeof(RM_FLCN_MSG_DPU));

    //
    // Send an empty command to DPU.  The NULL command does nothing but
    // make DPU returns corresponding message.
    //
    dpuCmd.hdr.unitId   = RM_DPU_UNIT_NULL;
    dpuCmd.hdr.size     = RM_FLCN_QUEUE_HDR_SIZE;

    Printf(Tee::PriHigh,
           "%d:DpuTest::%s: Submit a command to DPU...\n",
            __LINE__,__FUNCTION__);

    // submit the command
    rc = m_pDpu->Command(&dpuCmd,
                         &dpuMsg,
                         &seqNum);
    CHECK_RC(rc);

    Printf(Tee::PriHigh,
           "%d:DpuTest::%s: CMD submission success, got seqNum = %u\n",
            __LINE__,__FUNCTION__,seqNum);

    //
    // In function DPU::Command(), it waits message back if arrgument pMsg is
    // not NULL.  The design is different than PMU::Command();
    //

    // response message received, print out the details
    Printf(Tee::PriHigh,
               "%d:DpuTest::%s: Received command response from DPU\n",
                __LINE__,__FUNCTION__);
    Printf(Tee::PriHigh,
           "%d:DpuTest::%s: Printing Header :-\n",
            __LINE__,__FUNCTION__);
    Printf(Tee::PriHigh,
           "unitID = 0x%x ,size = 0x%x ,ctrlFlags  = 0x%x ,seqNumID = 0x%x\n",
           dpuMsg.hdr.unitId, dpuMsg.hdr.size ,
           dpuMsg.hdr.ctrlFlags , dpuMsg.hdr.seqNumId);

    return rc;
}

//! \brief Cleanup, does nothing for this test
//------------------------------------------------------------------------------
RC DpuTest::Cleanup()
{
    m_pDpu->Shutdown();
    delete m_pDpu;
    m_pDpu = NULL;

    return OK;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(DpuTest, RmTest, "DPU Test. \n");
