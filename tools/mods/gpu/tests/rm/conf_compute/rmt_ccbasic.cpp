/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
//!
//! \file rmt_ccbasic.cpp
//! \brief Sanity test for Confidential Compute.
//!

#include "gpu/tests/rmtest.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/tests/rm/utility/uprocrtos.h"
#include "gpu/tests/rm/utility/uprocgsprtos.h"
#include "gpu/tests/rm/memsys/rmt_acrverify.h"
#include "rmlsfm.h"
#include "class/cl9070.h"
#include "class/cl9270.h"
#include "class/cl9470.h"
#include "class/cl9570.h"
#include "class/clc310.h"
#include "ctrl/ctrlc310.h"
#include "rmgspcmdif.h"
#include "ampere/ga102/dev_gsp.h"
#include "ampere/ga102/dev_pwr_pri.h"

class CCBasicTest : public RmTest
{
public:
    CCBasicTest();
    virtual ~CCBasicTest()
    {
    };
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
private:
    unique_ptr<UprocGspRtos>      m_pGspRtos;
};

//! \brief CCBasicTest constructor
//!
//! Doesn't do much except SetName.
//!
//! \sa Setup
//------------------------------------------------------------------------------
CCBasicTest::CCBasicTest()
{
    SetName("CCBasicTest");
}

//! \brief IsTestSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string CCBasicTest::IsTestSupported()
{
    if (!(GetBoundGpuSubdevice()->DeviceId() >= Gpu::GA102))
        return "Test only supported on GA10X or later chips";

    return RUN_RMTEST_TRUE;
}

//! \brief Do Init from Js
//!
//! \return OK if no error happens, otherwise corresponding rc
//------------------------------------------------------------------------------
RC CCBasicTest::Setup()
{
    RC rc;

    // Get GSP UPROCRTOS class instance
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
RC CCBasicTest::Run()
{
    RC rc;


    // Temporary until as2 sanity issue gets resolved.
    return OK;

    rc = m_pGspRtos->WaitUprocReady();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
               "%d:CCBasicTest::%s: Ucode OS is not ready. Skipping tests. \n",
                __LINE__,__FUNCTION__);

        // fail the test, we need to figure out why GSP bootstrap failed.
        CHECK_RC(RC::LWRM_STATUS_ERROR_NOT_READY);
        return rc;
    }

    for (int i=0; i<10; ++i)
    {
        RM_FLCN_CMD_GSP       cmd = {0};
        RM_FLCN_MSG_GSP       msg = {0};
        UINT32                seqNum  = 0;
        RM_GSP_RMPROXY_CMD    *pRmCmd = &cmd.cmd.rmProxy;
        RM_GSP_RMPROXY_MSG    *pRmMsg = &msg.msg.rmProxy;
        UINT32                reg;

        cmd.hdr.unitId = RM_GSP_UNIT_RMPROXY;
        cmd.hdr.size   = RM_FLCN_QUEUE_HDR_SIZE + sizeof(RM_GSP_RMPROXY_CMD);

        // read register
        pRmCmd->cmdType = RM_GSP_RMPROXY_CMD_ID_REG_READ;
        pRmCmd->addr    = LW_PPWR_FALCON_MAILBOX1;

        rc = m_pGspRtos->Command(&cmd, &msg, &seqNum);
        if (rc != OK)
        {
            Printf(Tee::PriHigh, "CCBasicTest: Failed to send register read command\n");
            return rc;
        }

        if (pRmMsg->result != FLCN_OK)
        {
            Printf(Tee::PriHigh, "CCBasicTest: Read command returned error: 0x%x\n",
                   pRmMsg->result);
            return RC::SOFTWARE_ERROR;
        }

        // write register
        reg = ~pRmMsg->value;
        pRmCmd->cmdType = RM_GSP_RMPROXY_CMD_ID_REG_WRITE;
        pRmCmd->value   = reg;

        rc = m_pGspRtos->Command(&cmd, &msg, &seqNum);
        if (rc != OK)
        {
            Printf(Tee::PriHigh, "CCBasicTest: Failed to send register write command\n");
            return rc;
        }

        if (pRmMsg->result != FLCN_OK)
        {
            Printf(Tee::PriHigh, "CCBasicTest: Write command returned error: 0x%x\n",
                   pRmMsg->result);
            return RC::SOFTWARE_ERROR;
        }

        // read again
        pRmCmd->cmdType = RM_GSP_RMPROXY_CMD_ID_REG_READ;

        rc = m_pGspRtos->Command(&cmd, &msg, &seqNum);
        if (rc != OK)
        {
            Printf(Tee::PriHigh, "CCBasicTest: Failed to send register read command\n");
            return rc;
        }

        if (pRmMsg->result != FLCN_OK)
        {
            Printf(Tee::PriHigh, "CCBasicTest: Read command returned error: 0x%x\n",
                   pRmMsg->result);
            return RC::SOFTWARE_ERROR;
        }

        // Check if register value has changed as expected
        if ((pRmMsg->value != reg) ||
            (pRmMsg->value != GetBoundGpuSubdevice()->RegRd32(LW_PPWR_FALCON_MAILBOX1)))
        {
            Printf(Tee::PriHigh, "CCBasicTest: Register access test FAILED. Expected 0x%x, got hw 0x%x sw 0x%x.\n",
                   reg, GetBoundGpuSubdevice()->RegRd32(LW_PPWR_FALCON_MAILBOX1),
                   pRmMsg->value);
            return RC::SOFTWARE_ERROR;
        }
    }

    return OK;
}

//! \brief Cleanup, shuts down GSP test instance.
//------------------------------------------------------------------------------
RC CCBasicTest::Cleanup()
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
JS_CLASS_INHERIT(CCBasicTest, RmTest, "Confidential Compute Basic Test. \n");
