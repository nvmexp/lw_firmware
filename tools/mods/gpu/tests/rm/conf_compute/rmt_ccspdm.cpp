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
//! \file rmt_ccspdm.cpp
//! \brief Sanity test for Confidential Compute.
//!

#include "gpu/tests/rmtest.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/tests/rm/utility/uprocrtos.h"
#include "gpu/tests/rm/utility/uprocgsprtos.h"
#include "gpu/tests/rm/memsys/rmt_acrverify.h"
#include "gpu/tests/rm/spdm/rmt_spdmdefines.h"
#include "core/include/utility.h"
#include "core/include/platform.h"
#include "rmlsfm.h"
#include "class/clc310.h"
#include "ctrl/ctrlc310.h"

#include "rmgspcmdif.h"
#include "rmt_ccspdm.h"

//! \brief CCSpdmTest constructor
//!
//! Doesn't do much except SetName.
//!
//! \sa Setup
//------------------------------------------------------------------------------
CCSpdmTest::CCSpdmTest()
{
    SetName("CCSpdmTest");
}

//! \brief IsTestSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string CCSpdmTest::IsTestSupported()
{
    if (!(GetBoundGpuSubdevice()->DeviceId() >= Gpu::GH100))
        return "Test only supported on GH100 or later chips";

    return RUN_RMTEST_TRUE;
}

//! \brief Do Init from Js
//!
//! \return OK if no error happens, otherwise corresponding rc
//------------------------------------------------------------------------------
RC CCSpdmTest::Setup()
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
RC CCSpdmTest::Run()
{
    RC rc;

    rc = SpdmInit();

    if (rc != OK)
    {
        return rc;
    }


    CHECK_RC(SpdmGetVersionTest());
    CHECK_RC(SpdmGetCapabilitiesTest());
    CHECK_RC(SpdmNegotiateAlgorithmsTest());
    CHECK_RC(SpdmGetDigestsTest());
    CHECK_RC(SpdmGetCertificateTest());
    CHECK_RC(SpdmVendorDefinedRequestTest(LW_FALSE));
    CHECK_RC(SpdmKeyExchangeTest());
    CHECK_RC(SpdmFinishTest());
    CHECK_RC(SpdmKeyUpdateTest());
    CHECK_RC(SpdmVendorDefinedRequestTest(LW_TRUE));
    CHECK_RC(SpdmGetMeasurementsTest());
    CHECK_RC(SpdmEndSessionTest());

    rc = SpdmDeinit();

    return rc;
}

//! \brief Cleanup, shuts down GSP test instance.
//------------------------------------------------------------------------------
RC CCSpdmTest::Cleanup()
{
    m_pGspRtos->Shutdown();

    return OK;
}

//! \brief Send SPDM initialize request.
//------------------------------------------------------------------------------
RC CCSpdmTest::SpdmInit()
{
    RC rc;
    Surface2D   spdmSurface;
    Utility::CleanupOnReturn<Surface2D, void>
                                            FreeSpdmSurfaceReq(&spdmSurface, &Surface2D::Free);

    // Allocate surface to get endpointId
    rc = m_pGspRtos->AllocateMessageBuffer(&spdmSurface, SPDM_SURFACE_SIZE);
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
               "%d:CCSpdmTest::%s: can't allocate request request message buffer, size =  0x%x \n",
                __LINE__,__FUNCTION__, SPDM_SURFACE_SIZE);

        // fail the test, we need to figure out why GSP bootstrap failed.
        CHECK_RC(RC::SOFTWARE_ERROR);
        return rc;
    }

    rc = m_pGspRtos->WaitUprocReady();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
               "%d:CCSpdmTest::%s: Ucode OS is not ready. Skipping tests. \n",
                __LINE__,__FUNCTION__);

        // fail the test, we need to figure out why GSP bootstrap failed.
        CHECK_RC(RC::LWRM_STATUS_ERROR_NOT_READY);
        return rc;
    }

    guestId = TEST_GUEST_ID;

    RM_FLCN_CMD_GSP       cmd = {0};
    RM_FLCN_MSG_GSP       msg = {0};
    UINT32                seqNum  = 0;

    RM_GSP_SPDM_CMD *pRmCmd = &cmd.cmd.spdm;
    RM_GSP_SPDM_MSG *pRmMsg = &msg.msg.spdm;
    RM_GSP_SPDM_MSG_CC_INIT *pMsgInit = &pRmMsg->msgCcInit;
    RM_GSP_SPDM_CMD_CC_INIT *pCcInit = &pRmCmd->ccInit;
    RM_GSP_SPDM_CC_INIT_CTX *pCcInitCtx = &pCcInit->ccInitCtx;

    pCcInitCtx->guestId    = guestId;

    cmd.hdr.unitId = RM_GSP_UNIT_SPDM;
    cmd.hdr.size   = RM_FLCN_QUEUE_HDR_SIZE + sizeof(RM_GSP_SPDM_CMD);

    // send CC INIT command to GSP
    pRmCmd->cmdType = RM_GSP_SPDM_CMD_ID_CC_INIT;

    rc = m_pGspRtos->CommandCC(&cmd, &msg, &seqNum, spdmSurface.GetMemHandle(), 0, SPDM_SURFACE_SIZE);

    if (rc != OK)
    {
        Printf(Tee::PriHigh, "CCSpdmTest: Failed to initialize SPDM context \n");
        return rc;
    }

    if (pMsgInit->errorCode != FLCN_OK)
    {
        Printf(Tee::PriHigh, "CCSpdmTest: SPDM INIT command returned error: 0x%x\n",
                   pMsgInit->errorCode);
        return RC::SOFTWARE_ERROR;
    }

    // retrieve return endpointId
    Platform::MemCopy(&endpointId, spdmSurface.GetAddress(), sizeof(LwU32));
    Platform::FlushCpuWriteCombineBuffer();

    return OK;
}

//! \brief Send SPDM De-initialize request.
//------------------------------------------------------------------------------
RC CCSpdmTest::SpdmDeinit()
{
    RC rc;

    rc = m_pGspRtos->WaitUprocReady();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
               "%d:CCSpdmTest::%s: Ucode OS is not ready. Skipping tests. \n",
                __LINE__,__FUNCTION__);

        // fail the test, we need to figure out why GSP bootstrap failed.
        CHECK_RC(RC::LWRM_STATUS_ERROR_NOT_READY);
        return rc;
    }

    RM_FLCN_CMD_GSP       cmd = {0};
    RM_FLCN_MSG_GSP       msg = {0};
    UINT32                seqNum  = 0;

    RM_GSP_SPDM_CMD *pRmCmd = &cmd.cmd.spdm;
    RM_GSP_SPDM_MSG *pRmMsg = &msg.msg.spdm;
    RM_GSP_SPDM_MSG_CC_DEINIT *pMsgDeinit = &pRmMsg->msgCcDeinit;
    RM_GSP_SPDM_CMD_CC_DEINIT *pCcDeinit = &pRmCmd->ccDeinit;
    RM_GSP_SPDM_CC_DEINIT_CTX *pCcDeinitCtx = &pCcDeinit->ccDeinitCtx;

    pCcDeinitCtx->guestId     = guestId;
    pCcDeinitCtx->endpointId  = endpointId;

    cmd.hdr.unitId = RM_GSP_UNIT_SPDM;
    cmd.hdr.size   = RM_FLCN_QUEUE_HDR_SIZE + sizeof(RM_GSP_SPDM_CMD);

    // send CC INIT command to GSP
    pRmCmd->cmdType = RM_GSP_SPDM_CMD_ID_CC_DEINIT;
    rc = m_pGspRtos->CommandCC(&cmd, &msg, &seqNum, 0, 0, 0);
    if (rc != OK)
    {
        Printf(Tee::PriHigh, "CCSpdmTest: Failed to De-initialize SPDM context \n");
        return rc;
    }

    if (pMsgDeinit->errorCode != FLCN_OK)
    {
        Printf(Tee::PriHigh, "CCSpdmTest: SPDM DEINIT command returned error: 0x%x\n",
                   pMsgDeinit->errorCode);
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

//! \brief Send SPDM request message.
//------------------------------------------------------------------------------
RC CCSpdmTest::SpdmMessageProcess
(
    UINT08 *pMsgBufferReq,
    UINT32  reqMsgSize,
    UINT08 *pMsgBufferRsp,
    UINT32  rspBufferSize
)
{

    RC rc;
    Surface2D   spdmSurface;

    Utility::CleanupOnReturn<Surface2D, void>
                                            FreeSpdmSurfaceReq(&spdmSurface, &Surface2D::Free);

    if (reqMsgSize > SPDM_SURFACE_SIZE || rspBufferSize > SPDM_SURFACE_SIZE)
    {
        CHECK_RC(RC::SOFTWARE_ERROR);
        return rc;
    }

    rc = m_pGspRtos->AllocateMessageBuffer(&spdmSurface, SPDM_SURFACE_SIZE);
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
               "%d:CCSpdmTest::%s: can't allocate request request message buffer, size =  0x%x \n",
                __LINE__,__FUNCTION__, SPDM_SURFACE_SIZE);

        // fail the test, we need to figure out why GSP bootstrap failed.
        CHECK_RC(RC::SOFTWARE_ERROR);
        return rc;
    }

    // Copy the request message into the surface
    Platform::MemCopy(spdmSurface.GetAddress(), pMsgBufferReq, reqMsgSize);
    Platform::FlushCpuWriteCombineBuffer();

    rc = m_pGspRtos->WaitUprocReady();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
               "%d:CCSpdmTest::%s: Ucode OS is not ready. Skipping tests. \n",
                __LINE__,__FUNCTION__);

        // fail the test, we need to figure out why GSP bootstrap failed.
        CHECK_RC(RC::LWRM_STATUS_ERROR_NOT_READY);
        return rc;
    }

    RM_FLCN_CMD_GSP       cmd = {0};
    RM_FLCN_MSG_GSP       msg = {0};
    UINT32                seqNum  = 0;

    RM_GSP_SPDM_CMD *pRmCmd = &cmd.cmd.spdm;
    RM_GSP_SPDM_MSG *pRmMsg = &msg.msg.spdm;
    RM_GSP_SPDM_MSG_CC_CTRL *pMsgCcCtrl = &pRmMsg->msgCcCtrl;
    RM_GSP_SPDM_CMD_CC_CTRL *pCcCtrl = &pRmCmd->ccCtrl;
    RM_GSP_SPDM_CC_CTRL_CTX *pCcCtrlCtx = &pCcCtrl->ccCtrlCtx;

    pCcCtrlCtx->guestId    = guestId;
    pCcCtrlCtx->endpointId = endpointId;
    pCcCtrlCtx->ctrlCode   = CC_CTRL_CODE_SPDM_MESSAGE_PROCESS;
    pCcCtrlCtx->ctrlParam  = TEST_SEQUENCE_NUM;

    cmd.hdr.unitId = RM_GSP_UNIT_SPDM;
    cmd.hdr.size   = RM_FLCN_QUEUE_HDR_SIZE + sizeof(RM_GSP_SPDM_CMD);

    // send CC CTRL command to GSP
    pRmCmd->cmdType = RM_GSP_SPDM_CMD_ID_CC_CTRL;
    rc = m_pGspRtos->CommandCC(&cmd, &msg, &seqNum,
                               spdmSurface.GetMemHandle(),
                               reqMsgSize,
                               rspBufferSize);

    if (rc != OK)
    {
        Printf(Tee::PriHigh, "CCSpdmTest: Failed to send SPDM request \n");
        return rc;
    }

    if (pMsgCcCtrl->errorCode != FLCN_OK)
    {
        Printf(Tee::PriHigh, "CCSpdmTest: SPDM CTRL command returned error: 0x%x\n",
                   pMsgCcCtrl->errorCode);
        return RC::SOFTWARE_ERROR;
    }

    // retrieve return response message
    Platform::MemCopy(pMsgBufferRsp, (LwU8 *)spdmSurface.GetAddress(), rspBufferSize);
    Platform::FlushCpuWriteCombineBuffer();

    return OK;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(CCSpdmTest, RmTest, "Confidential Compute Spdm Test. \n");

