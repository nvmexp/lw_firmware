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

//
// SeBasicTest test.
// Tests Security Engine(SE) by sending a command to DPU which
// inturn interacts with SE to do a RSA operation.
//

#include "gpu/tests/rmtest.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/display/dpusub.h"
#include "gpu/tests/rm/utility/rmtestutils.h"
#include "core/include/memcheck.h"

#include "gpu/include/gpudev.h"
#include "core/include/mgrmgr.h"

#include "class/cl9070.h"
#include "ctrl/ctrl0073.h"

class SeBasicTest: public RmTest
{
public:
    SeBasicTest();
    virtual ~SeBasicTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    DPU            *m_pDpu;
    GpuSubdevice   *m_Parent;    // GpuSubdevice that owns this DPU

    RC             SeTestSendCmdToDpu();
};

//------------------------------------------------------------------------------
SeBasicTest::SeBasicTest() :
    m_pDpu(nullptr),
    m_Parent(nullptr)
{
    SetName("SeBasicTest");
}

//------------------------------------------------------------------------------
SeBasicTest::~SeBasicTest()
{
}

//------------------------------------------------------------------------
string SeBasicTest::IsTestSupported()
{
    Display    *pDisplay     = GetDisplay();
    UINT32      displayClass = pDisplay->GetClass();
    UINT32      gpuFamily    = (GetBoundGpuDevice())->GetFamily();

    if ((pDisplay->GetDisplayClassFamily() == Display::EVO) &&
        (displayClass >= LW9070_DISPLAY))
    {
        if (gpuFamily < GpuDevice::Pascal)
        {
            return "Test only supported on GP100 or later chips";
        }
        else
        {
            return RUN_RMTEST_TRUE;
        }
    }

    return "[SeBasic RMTest] : DPU is not supported on current platform";
}

//------------------------------------------------------------------------------
RC SeBasicTest::Setup()
{
    RC rc = OK;

    m_Parent = GetBoundGpuSubdevice();

    m_pDpu = new DPU(m_Parent);

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

//------------------------------------------------------------------------
RC SeBasicTest::Run()
{
    RC rc;
    Display *pDisplay = GetDisplay();
    UINT32   lwrDpuUcodeState;

    // Intializing Display HW
    CHECK_RC(pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    rc = m_pDpu->WaitDPUReady();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
               "%d:SeBasicTest::%s: Ucode is not ready. Skip running test.\n",
                __LINE__,__FUNCTION__);
    }

    CHECK_RC(m_pDpu->GetUcodeState(&lwrDpuUcodeState));
    if (lwrDpuUcodeState == LW0073_CTRL_DPU_UCODE_STATE_READY)
    {
        CHECK_RC(SeTestSendCmdToDpu());
    }

    return rc;
}

RC SeBasicTest::SeTestSendCmdToDpu()
{
    RC                      rc = OK;
    RM_DPU_COMMAND          dpuCmd;
    RM_FLCN_MSG_DPU         dpuMsg;
    UINT32                  seqNum;

    memset(&dpuCmd, 0, sizeof(RM_DPU_COMMAND));
    memset(&dpuMsg, 0, sizeof(RM_FLCN_MSG_DPU));

    //
    // Send HDCP test SE command to DPU. This in turn calls
    // seSampleRsaCode() in the SE library which does a 256 and
    // 512 bit RSA operation and compares to the golden value.
    //
    dpuCmd.hdr.unitId            = RM_DPU_UNIT_HDCP22WIRED;
    dpuCmd.hdr.size              = RM_FLCN_CMD_SIZE(HDCP22, TEST_SE);
    dpuCmd.cmd.hdcp.cmdType      = RM_FLCN_CMD_TYPE(HDCP22, TEST_SE);

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

    if (dpuMsg.msg.hdcp22wired.msgGeneric.flcnStatus == RM_FLCN_HDCP22_STATUS_TEST_SE_SUCCESS)
        return OK;
    else
        return RC::SOFTWARE_ERROR;
}

//------------------------------------------------------------------------------
RC SeBasicTest::Cleanup()
{
    m_pDpu->Shutdown();
    delete m_pDpu;
    m_pDpu = NULL;

    return OK;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ SeBasicTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(SeBasicTest, RmTest,
                 "Security Engine basic RM test.");
