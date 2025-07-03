/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "uprocrtos.h"
#include "uprocgsprtos.h"
#include "gpu/include/gpusbdev.h"
#include "lwRmApi.h"
#include "core/include/platform.h"
#include "class/clc310.h"
#include "ctrl/ctrlc310.h"
#include "gpu/tests/rmtest.h"
#include "rmgspcmdif.h"
#include "uprocifcmn.h"
#include "uprociftest.h"

//TODO: def somewhere else
#define GSP_RM_CMDQ_LOG_ID  0

//!
//! \brief Constructor
//!
UprocGspRtos::UprocGspRtos(GpuSubdevice *pParent)
 : UprocRtos (pParent)
{
    m_UprocId = UPROC_ID_GSP;
    m_bSetupFbSurface = false;
}

//!
//! \brief Destructor
//!
UprocGspRtos::~UprocGspRtos()
{
    Shutdown();
}

//!
//! \brief Shutdown the GSP
//!
void UprocGspRtos::Shutdown()
{
    LwRmPtr pLwRm;

    if (m_Handle)
    {
        pLwRm->Free(m_Handle);
        m_Handle = 0;
    }

    m_Initialized = false;
}

//!
//! \brief      Allocate the GSP handle
//!
//! \return     OK if no errors, otherwise corresponding RC
//!
RC UprocGspRtos::Initialize()
{
    RC      rc;
    LwRmPtr pLwRm;

    Printf(Tee::PriLow, "GSP UPROC::Initialize\n");
    // Validate that this GPU has a GSP, if so, allocate and initialze it
    if (!pLwRm->IsClassSupported(VOLTA_GSP,
                                 m_Subdevice->GetParentDevice()))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    // Allocate Subdevice GSP handle
    CHECK_RC(pLwRm->Alloc(pLwRm->GetSubdeviceHandle(m_Subdevice),
                          &m_Handle,
                          VOLTA_GSP,
                          NULL));

    m_Initialized = true;

    return rc;
}

//!
//! \brief Check if a particular command has completed
//!
//! \param SeqNum      : Sequence number of the command to check for completion
//! \param pbComplete  : Returned boolean to indicate if the command is complete
//!
//! \return OK if successful, not OK otherwise
//!
RC UprocGspRtos::IsCommandComplete(LwU32 seqNum, bool *pbComplete)
{
    RC        rc = OK;
    LwRmPtr   pLwRm;
    LWC310_CTRL_GSP_CMD_STATUS_PARAMS  gspCmdStatus = {0};

    MASSERT(pbComplete);

    // Get the status for the current command
    gspCmdStatus.seqDesc = seqNum;
    CHECK_RC(pLwRm->Control(m_Handle,
                            LWC310_CTRL_CMD_GSP_CMD_STATUS,
                            &gspCmdStatus,
                            sizeof(gspCmdStatus)));

    // There are three possible status returns (NONE which means
    // the command does not exist, RUNNING which means that the
    // command has not yet completed, and DONE which means the
    // command has completed
    if (gspCmdStatus.seqStatus == LWC310_CTRL_GSP_CMD_STATUS_DONE)
    {
        *pbComplete = true;
    }
    else if (gspCmdStatus.seqStatus == LWC310_CTRL_GSP_CMD_STATUS_RUNNING)
    {
        *pbComplete = false;
    }
    else //_NONE and invalid cases
    {
        MASSERT(!"IsCommandComplete : invalid command sequence\n");
        rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}

//! \brief Static polling function to determine if a GSP message have been
//!        received
//!
//! \param pVoidPollArgs  : Pointer to data structure PollMsgArgs that includes
//!            pThis      : Pointer to the GSP instance
//!            seqNum     : The sequence number of the target message
//!
//! \return true if the GSP message has been received or an error stops polling,
//!         false otherwise
//!
bool UprocGspRtos::PollMsgReceived(void *pVoidPollArgs)
{
    PollMsgArgs   *pPollArgs = static_cast<PollMsgArgs *> (pVoidPollArgs);
    UprocGspRtos  *pThis;
    UINT32        seqNum;
    bool          bCommandComplete;
    RC            rc;

    pThis  = pPollArgs->pGspRtos;
    seqNum = pPollArgs->seqNum;

    rc = pThis->IsCommandComplete(seqNum, &bCommandComplete);

    pPollArgs->pollRc = rc;

    if (rc == OK && !bCommandComplete)
    {
        // keep polling
        return false;
    }
    return true;
}

//! \brief Wait for GSP Message with seqnum to be received
//!
//! \return OK if the Message has been received, not OK otherwise
//!
RC UprocGspRtos::WaitMessage(LwU32 seqNum)
{
    PollMsgArgs pollArgs;
    RC          rc;
    LwU32       timeout;

    pollArgs.pGspRtos = this;
    pollArgs.seqNum = seqNum;

    // Use a larger timeout for emulation
    timeout = m_Subdevice->IsEmulation() ? CMD_EMU_TIMEOUT_MS : CMD_TIMEOUT_MS;

    rc = POLLWRAP_HW(UprocGspRtos::PollMsgReceived, &pollArgs, timeout);

    if (rc == OK)
    {
        return pollArgs.pollRc;
    }
    else
    {
        return rc;
    }
}

//! \brief Wait for the GSP to be ready to receive commands
//!
//! \return OK if the GSP is ready, not OK otherwise
//!
RC UprocGspRtos::WaitUprocReady()
{
    PollGspArgs     pollArgs;
    RC              rc;
    LwU32           timeout;

    pollArgs.pGspRtos = this;

    timeout = m_Subdevice->IsEmulation() ? BOOT_EMU_TIMEOUT_MS : BOOT_TIMEOUT_MS;

    rc = POLLWRAP_HW(UprocGspRtos::PollGspReady, &pollArgs, timeout);
    if (rc == OK)
    {
        return pollArgs.pollRc;
    }
    else
    {
        return rc;
    }
}

//! \brief Static polling function to determine if GSP is ready
//!
//! \return true if GSP is ready or an error stops polling, false otherwise
//!
bool UprocGspRtos::PollGspReady(void *pVoidPollArgs)
{
    PollGspArgs   *pPollArgs = static_cast<PollGspArgs *> (pVoidPollArgs);
    UprocGspRtos  *pThis;
    RC            rc;
    LwRmPtr       pLwRm;
    LWC310_CTRL_GSP_UCODE_STATE_PARAMS  gspStateParam = {0};

    pThis = pPollArgs->pGspRtos;

    rc = pLwRm->Control(pThis->m_Handle,
                        LWC310_CTRL_CMD_GSP_UCODE_STATE,
                        &gspStateParam,
                        sizeof (gspStateParam));

    pPollArgs->pollRc = rc;

    Printf(Tee::PriDebug,
           "GSP UPROC Polling for ready\n");

    if (rc != OK)
    {
        Printf(Tee::PriHigh,
               "Error RC: 0x%08x\n", rc.Get());
    }

    if (rc == OK &&
        gspStateParam.ucodeState != LWC310_CTRL_GSP_UCODE_STATE_READY)
    {
        // keep polling
        return false;
    }

    return true;
}

//! \brief Send a command to the GSP uproc
//!
//! \param inCmd         : Pointer to the contents of the command to send.
//! \param inMsg         : Pointer to storage for the command response message,
//!                        if NULL then the response is not returned and this
//!                        function exits right after command is sent. When the
//!                        pointer is not NULL, this function waits the response
//!                        until timeout.
//! \param inQueueHdr    : Pointer to the queue header of the command to send.
//! \param pSeqNum       : Returned sequence number of the sent command, used to
//!                        filter on a particular command response. Output.
//!
//! \return OK if the command was successfuly sent and the requested msg
//!         returned, not OK otherwise
//!
RC UprocGspRtos::Command
(
    RM_UPROC_TEST_CMD  *inCmd,
    RM_UPROC_TEST_MSG  *inMsg,
    RM_FLCN_QUEUE_HDR  *inQueueHdr,
    LwU32              *pSeqNum
)
{
    RC                                    rc;
    RM_FLCN_CMD_GSP                       gspFlcCmd;
    RM_FLCN_MSG_GSP                       gspFlcMsg;

    MASSERT(m_Initialized);

    if (!inCmd || !inQueueHdr || !pSeqNum)
    {
        Printf(Tee::PriHigh,
                "%d:UprocGspRtos::%s: Bad args passed!\n",
                __LINE__, __FUNCTION__);
        return RC::BAD_PARAMETER;
    }
    memset(&gspFlcCmd, 0, sizeof(RM_FLCN_CMD_GSP));
    memset(&gspFlcMsg, 0, sizeof(RM_FLCN_MSG_GSP));

    gspFlcCmd.hdr.unitId = (inQueueHdr->unitId == RM_UPROC_UNIT_NULL)
                           ? RM_GSP_UNIT_NULL : RM_GSP_UNIT_TEST;
    gspFlcCmd.hdr.size   = inQueueHdr->size;

    Platform::MemCopy(&gspFlcCmd.cmd, inCmd, sizeof(RM_UPROC_TEST_CMD));

    rc = Command(&gspFlcCmd, &gspFlcMsg, pSeqNum);

    if ((rc == OK) && (inMsg != NULL))
    {
        // copy back the test msg
        Platform::MemCopy(inMsg, &gspFlcMsg.msg.test, sizeof(RM_UPROC_TEST_MSG));
    }

    return rc;
}

//! \brief Send arbitrary command to the GSP uproc
//!
//! \param inCmd         : Pointer to the contents of the command to send.
//! \param inMsg         : Pointer to storage for the command response message,
//!                        if NULL then the response is not returned and this
//!                        function exits right after command is sent. When the
//!                        pointer is not NULL, this function waits the response
//!                        until timeout.
//! \param pSeqNum       : Returned sequence number of the sent command, used to
//!                        filter on a particular command response. Output.
//!
//! \return OK if the command was successfuly sent and the requested msg
//!         returned, not OK otherwise
//!
RC UprocGspRtos::Command
(
    RM_FLCN_CMD_GSP  *inCmd,
    RM_FLCN_MSG_GSP  *inMsg,
    LwU32            *pSeqNum
)
{
    RC                                    rc;
    LWC310_CTRL_GSP_SEND_CMD_PARAMS       gspCmd;
    Surface2D                             Command;
    Surface2D                             Message;

    MASSERT(m_Initialized);

    if (!inCmd || !pSeqNum)
    {
        Printf(Tee::PriHigh,
                "%d:UprocGspRtos::%s: Bad args passed!\n",
                __LINE__, __FUNCTION__);
        return RC::BAD_PARAMETER;
    }
    memset(&gspCmd, 0, sizeof(LWC310_CTRL_GSP_SEND_CMD_PARAMS));

    Utility::CleanupOnReturn<Surface2D, void>
                                          FreeCommand(&Command,&Surface2D::Free);
    Utility::CleanupOnReturn<Surface2D, void>
                                          FreeMessage(&Message,&Surface2D::Free);

    CHECK_RC(CreateSurface(&Command, sizeof(RM_FLCN_CMD_GSP)));

    // Copy the command into the surface
    Platform::MemCopy(Command.GetAddress(), inCmd, inCmd->hdr.size);
    Platform::FlushCpuWriteCombineBuffer();

    gspCmd.cmd.hMemory  = Command.GetMemHandle();
    gspCmd.cmd.offset   = 0;
    gspCmd.cmd.size     = sizeof(RM_FLCN_CMD_GSP);
    gspCmd.queueId      = GSP_RM_CMDQ_LOG_ID;

    if (inMsg)
    {
        CHECK_RC(CreateSurface(&Message, sizeof(RM_FLCN_MSG_GSP)));

        gspCmd.msg.hMemory  = Message.GetMemHandle();
        gspCmd.msg.offset   = 0;
        gspCmd.msg.size     = sizeof(RM_FLCN_MSG_GSP);
    }

    CHECK_RC(LwRmPtr()->Control(m_Handle,
                                LWC310_CTRL_CMD_GSP_SEND_CMD,
                                &gspCmd,
                                sizeof(gspCmd)));

    *pSeqNum = gspCmd.seqDesc;

    Printf(Tee::PriDebug, "GSP::Command(): Command queued with seqDesc=%d."
                          "\n", *pSeqNum);

    DumpHeader((UINT08 *)Command.GetAddress() +
               offsetof(RM_FLCN_CMD_GSP, hdr), COMMAND_HDR);

    if (inMsg)
    {
        Printf(Tee::PriDebug, "GSP::Command(): Wait Message with seqDesc=%d "
                              "comes back.\n", *pSeqNum);

        //
        // When inMsg is requested, it *MUST* wait until the callback function
        // finishes its work of using 'Message'.
        //
        CHECK_RC(WaitMessage(gspCmd.seqDesc));


        Printf(Tee::PriHigh, "GSP::Command(): Received Message with seqDesc"
                              "=%d.\n", *pSeqNum);

        // Copy the head
        Platform::MemCopy(inMsg, Message.GetAddress(),
                          RM_FLCN_QUEUE_HDR_SIZE);

        if (inMsg->hdr.size > RM_FLCN_QUEUE_HDR_SIZE)
        {
            // Copy head + body
            Platform::MemCopy(inMsg, Message.GetAddress(),
                              inMsg->hdr.size);
        }

        // response message received, print out the details
        Printf(Tee::PriHigh,
               "%d:UprocGspRtos::%s: Received command response from GSP UPROC\n",
                __LINE__, __FUNCTION__);
        Printf(Tee::PriHigh,
               "%d:UprocGspRtos::%s: Printing Header :-\n",
                __LINE__, __FUNCTION__);
        Printf(Tee::PriHigh,
               "unitID = 0x%x ,size = 0x%x ,ctrlFlags  = 0x%x ,seqNumID = 0x%x\n",
                inMsg->hdr.unitId, inMsg->hdr.size,
                inMsg->hdr.ctrlFlags, inMsg->hdr.seqNumId);

        Platform::FlushCpuWriteCombineBuffer();
    }

    return rc;
}


//! \brief Send arbitrary command to the GSP uproc with CC ilwoloved
//!
//! \param inCmd         : Pointer to the contents of the command to send.
//! \param inMsg         : Pointer to storage for the command response message,
//!                        if NULL then the response is not returned and this
//!                        function exits right after command is sent. When the
//!                        pointer is not NULL, this function waits the response
//!                        until timeout.
//! \param pSeqNum       : Returned sequence number of the sent command, used to
//!                        filter on a particular command response. Output.
//! \param hMemory       : The memory handle used to indicate a memory buffer to save request and response message
//  \param reqMsgSize    : The request message size
//! \param rspBufferSize : The response buffer size for hMemory
//
//! \return OK if the command was successfully sent and the requested message
//!         returned, not OK otherwise
//!
RC UprocGspRtos::CommandCC
(
    RM_FLCN_CMD_GSP   *inCmd,
    RM_FLCN_MSG_GSP   *inMsg,
    LwU32             *pSeqNum,
    LwRm::Handle       hMemory,
    LwU32              reqMsgSize,
    LwU32              rspBufferSize
)
{    
    RC                                    rc;
    LWC310_CTRL_GSP_SEND_CMD_PARAMS       gspCmd;
    Surface2D                             Command;
    Surface2D                             Message;

    MASSERT(m_Initialized);

    if (!inCmd || !pSeqNum)
    {
        Printf(Tee::PriHigh,
                "%d:UprocGspRtos::%s: Bad args passed!\n",
                __LINE__, __FUNCTION__);
        return RC::BAD_PARAMETER;
    }
    memset(&gspCmd, 0, sizeof(LWC310_CTRL_GSP_SEND_CMD_PARAMS));

    Utility::CleanupOnReturn<Surface2D, void>
                                          FreeCommand(&Command,&Surface2D::Free);
    Utility::CleanupOnReturn<Surface2D, void>
                                          FreeMessage(&Message,&Surface2D::Free);

    CHECK_RC(CreateSurface(&Command, sizeof(RM_FLCN_CMD_GSP)));

    // Copy the command into the surface
    Platform::MemCopy(Command.GetAddress(), inCmd, inCmd->hdr.size);
    Platform::FlushCpuWriteCombineBuffer();

    gspCmd.cmd.hMemory   = Command.GetMemHandle();
    gspCmd.cmd.offset    = 0;
    gspCmd.cmd.size      = sizeof(RM_FLCN_CMD_GSP);
    gspCmd.queueId       = GSP_RM_CMDQ_LOG_ID;
    gspCmd.hMemory       = hMemory;
    gspCmd.reqMsgSize    = reqMsgSize;
    gspCmd.rspBufferSize = rspBufferSize;
    gspCmd.flags         = LWC310_CTRL_GSP_SEND_CMD_FLAGS_ROUTE_CC_ENABLE;

    if (inMsg)
    {
        CHECK_RC(CreateSurface(&Message, sizeof(RM_FLCN_MSG_GSP)));

        gspCmd.msg.hMemory  = Message.GetMemHandle();
        gspCmd.msg.offset   = 0;
        gspCmd.msg.size     = sizeof(RM_FLCN_MSG_GSP);
    }

    CHECK_RC(LwRmPtr()->Control(m_Handle,
                                LWC310_CTRL_CMD_GSP_SEND_CMD,
                                &gspCmd,
                                sizeof(gspCmd)));

    *pSeqNum = gspCmd.seqDesc;

    Printf(Tee::PriDebug, "GSP::Command(): Command queued with seqDesc=%d."
                          "\n", *pSeqNum);

    DumpHeader((UINT08 *)Command.GetAddress() +
               offsetof(RM_FLCN_CMD_GSP, hdr), COMMAND_HDR);

    if (inMsg)
    {
        Printf(Tee::PriDebug, "GSP::Command(): Wait Message with seqDesc=%d "
                              "comes back.\n", *pSeqNum);

        //
        // When inMsg is requested, it *MUST* wait until the callback function
        // finishes its work of using 'Message'.
        //
        CHECK_RC(WaitMessage(gspCmd.seqDesc));

        Printf(Tee::PriHigh, "GSP::Command(): Received Message with seqDesc"
                              "=%d.\n", *pSeqNum);

        // Copy the head
        Platform::MemCopy(inMsg, Message.GetAddress(),
                          RM_FLCN_QUEUE_HDR_SIZE);

        if (inMsg->hdr.size > RM_FLCN_QUEUE_HDR_SIZE)
        {
            // Copy head + body
            Platform::MemCopy(inMsg, Message.GetAddress(),
                              inMsg->hdr.size);
        }

        // response message received, print out the details
        Printf(Tee::PriHigh,
               "%d:UprocGspRtos::%s: Received command response from GSP UPROC\n",
                __LINE__, __FUNCTION__);
        Printf(Tee::PriHigh,
               "%d:UprocGspRtos::%s: Printing Header :-\n",
                __LINE__, __FUNCTION__);
        Printf(Tee::PriHigh,
               "unitID = 0x%x ,size = 0x%x ,ctrlFlags  = 0x%x ,seqNumID = 0x%x\n",
                inMsg->hdr.unitId, inMsg->hdr.size,
                inMsg->hdr.ctrlFlags, inMsg->hdr.seqNumId);

        Platform::FlushCpuWriteCombineBuffer();
    }

    return rc;
}

//! \brief Check if the given testId is supported or not
//!
//! \return true if the testId is supported, false if not
//!
bool UprocGspRtos::CheckTestSupported(LwU8 testId)
{
    LWC310_CTRL_GSP_TEST_SUPPORTED_PARAMS gspTestSupported;

    gspTestSupported.testId = testId;
    gspTestSupported.bTestSupported = false;

    if (LwRmPtr()->Control(m_Handle,
                          LWC310_CTRL_CMD_GSP_TEST_SUPPORTED,
                          &gspTestSupported,
                          sizeof(gspTestSupported)) == OK)
    {
        return (gspTestSupported.bTestSupported != LW_FALSE);
    }
    return false;
}

//! \brief Allocate buffer in FB for MSCG fb access test.
//!
//! \return OK if we succeed in allocating surface, error RC otherwise
//!
RC UprocGspRtos::SetupFbSurface(LwU32 size)
{
    RC rc = OK;

    m_fbmemSurface.SetArrayPitch(size);
    m_fbmemSurface.SetForceSizeAlloc(true);
    m_fbmemSurface.SetLayout(Surface2D::Pitch);
    m_fbmemSurface.SetLocation(Memory::Fb);
    m_fbmemSurface.SetColorFormat(ColorUtils::VOID32);
    m_fbmemSurface.SetPhysContig(true);
    m_fbmemSurface.SetArraySize(size);
    m_fbmemSurface.SetAddressModel(Memory::Paging);

    Printf(Tee::PriHigh, "%s: Allocating a surface in FB of size 0x%x\n", __FUNCTION__, size);
    CHECK_RC(m_fbmemSurface.Alloc(m_Subdevice->GetParentDevice()));
    CHECK_RC(m_fbmemSurface.Map());
    m_bSetupFbSurface = true;
    return rc;
}

//! \brief Stub for MSCG method access test on GSP, which isn't supported.
//!
//! \return RC::UNSUPPORTED_FUNCTION
//!
RC UprocGspRtos::SetupUprocChannel(GpuTestConfiguration* testConfig)
{
    Printf(Tee::PriHigh,
           "%d:UprocGspRtos::%s: not implemented, HW does not support\n",
            __LINE__, __FUNCTION__);
    return RC::UNSUPPORTED_FUNCTION;
}

//! \brief Free allocated resources used in MSCG test.
//!
//! \return OK if we successfully free resources, else RC
//!
RC UprocGspRtos::FreeResources()
{
    LwRmPtr pLwRm;
    RC rc = OK;
    if (m_bSetupFbSurface == true)
    {
        m_fbmemSurface.Unmap();
        m_fbmemSurface.Free();
    }
    return rc;
}

//! \brief Send command to Uproc to disengage MSCG.
//!
//! \return OK if we successfully send command to uproc, else RC
//!
RC UprocGspRtos::SendUprocTraffic(LwU32 wakeupMethod, FLOAT64 timeOutMs)
{
    RC rc = OK;
    //GSP does not support UPROCRTOS_UPROC_PB_ACTIVITY
    if (wakeupMethod == UPROCRTOS_UPROC_REG_ACCESS)
    {
        RM_UPROC_TEST_CMD     uprocTestCmd = {0};
        RM_UPROC_TEST_MSG     uprocTestMsg = {0};
        RM_FLCN_QUEUE_HDR     falconHdr = {0};
        UINT32                seqNum;
        UINT32                ret;

        // Send a TEST command to GSP
        falconHdr.unitId           = RM_GSP_UNIT_TEST;
        falconHdr.size             = RM_UPROC_CMD_SIZE(TEST, RD_BLACKLISTED_REG);
        uprocTestCmd.cmdType       = RM_UPROC_CMD_TYPE(TEST, RD_BLACKLISTED_REG);

        Printf(Tee::PriHigh, "Submitting a command to GSP to read a clock gated register.\n");

        // submit the command.
        rc = this->Command(&uprocTestCmd, &uprocTestMsg, &falconHdr, &seqNum);
        if (OK != rc)
        {
            Printf(Tee::PriHigh, "Error with the register read request.\n");
            return rc;
        }

        ret = uprocTestMsg.rdBlacklistedReg.val;
        Printf(Tee::PriHigh, "\tValue Returned by blacklisted reg access = 0x%x\n",ret);
    }
    else if (wakeupMethod == UPROCRTOS_UPROC_FB_ACCESS)
    {
        RM_UPROC_TEST_CMD       uprocTestCmd = {0};
        RM_UPROC_TEST_MSG       uprocTestMsg = {0};
        RM_FLCN_QUEUE_HDR       falconHdr    = {0};
        UINT32                  seqNum;
        PHYSADDR                physAddr;

        if (m_bSetupFbSurface == false)
        {
            Printf(Tee::PriHigh, "Attempted to use FB_ACCESS wakeup before initialization!!\n");
            return RC::SOFTWARE_ERROR;
        }

        CHECK_RC(m_fbmemSurface.GetDevicePhysAddress(0, Surface2D::GPUVASpace, &physAddr));
        Printf(Tee::PriHigh, "fb physAddr = %016llx\n",physAddr);

        memset(&uprocTestCmd, 0, sizeof(RM_UPROC_TEST_CMD));
        memset(&uprocTestMsg, 0, sizeof(RM_UPROC_TEST_MSG));
        memset(&falconHdr, 0, sizeof(RM_FLCN_QUEUE_HDR));

        falconHdr.unitId                       = RM_GSP_UNIT_TEST;
        falconHdr.size                         = RM_UPROC_CMD_SIZE(TEST, MSCG_ISSUE_FB_ACCESS);
        uprocTestCmd.cmdType                   = RM_UPROC_CMD_TYPE(TEST, MSCG_ISSUE_FB_ACCESS);
        uprocTestCmd.mscgFbAccess.fbOffsetLo32 = physAddr & 0xFFFFFFFFULL;
        uprocTestCmd.mscgFbAccess.fbOffsetHi32 = physAddr >> 32;
        uprocTestCmd.mscgFbAccess.op           = MSCG_ISSUE_FB_ACCESS_WRITE;

        Printf(Tee::PriHigh, "Submitting a command to GSP to write on FB region.\n");

        rc = this->Command(&uprocTestCmd, &uprocTestMsg, &falconHdr, &seqNum);
        if (OK != rc)
        {
            Printf(Tee::PriHigh, "MSCG FB write command failed.\n");
            return rc;
        }
        else
        {
            if (uprocTestMsg.mscgFbAccess.status == RM_UPROC_TEST_MSG_STATUS_OK)
            {
                Printf(Tee::PriHigh, "Successful write on FB region\n");
            }
            else
            {
                Printf(Tee::PriHigh, "MSCG FB write access failed.\n");
                return RC::SOFTWARE_ERROR;
            }
        }

        //Making MSCG engaged again
        Tasker::Sleep(500);
        uprocTestCmd.mscgFbAccess.op   = MSCG_ISSUE_FB_ACCESS_READ;

        Printf(Tee::PriHigh, "Submitting a command to GSP to read on FB region.\n");

        rc = this->Command(&uprocTestCmd, &uprocTestMsg, &falconHdr, &seqNum);
        if (OK != rc)
        {
            Printf(Tee::PriHigh, "MSCG FB read command failed.\n");
            return rc;
        }
        else
        {
            if (uprocTestMsg.mscgFbAccess.status == RM_UPROC_TEST_MSG_STATUS_OK)
            {
                Printf(Tee::PriHigh, "Successful read on FB region\n");
            }
            else
            {
                Printf(Tee::PriHigh, "MSCG FB read access failed.\n");
                return RC::SOFTWARE_ERROR;
            }
        }
    }
    else
    {
        Printf(Tee::PriHigh, "Unknown GSP wakeup request recieved.\n");
        return RC::SOFTWARE_ERROR;
    }
    return rc;
}

//! \brief Send command to Uproc to disengage MSCG.
//!
//! \return mask of acceptable MSCG wakeup reason values. Returns 0 if invalid
//!         wakeup reason is specified, which will automatically fail MSCG test.
//!
LwU32 UprocGspRtos::GetMscgWakeupReasonMask(LwU32 wakeupMethod)
{
    if (wakeupMethod == UPROCRTOS_UPROC_REG_ACCESS)
    {
        return 0x2000000; //PG_WAKEUP_EVENT_GSP_PRIV_BLOCKER in pmu_objpg.h - which cannot be included here.
    }
    else if (wakeupMethod == UPROCRTOS_UPROC_FB_ACCESS)
    {
        return 0x1000000; //PG_WAKEUP_EVENT_GSP_FB_BLOCKER in pmu_objpg.h - which cannot be included here.
    }
    else
    {
        return 0; //will always fail to match
    }
}

//! \brief Allocate message buffer for SPDM request/response.
//!
//! \return OK if we successfully allocate message buffer, else RC
//!
RC UprocGspRtos::AllocateMessageBuffer(Surface2D *pSurface, LwU32 Size)
{
    RC rc = OK;

    // Calling UprocRtos::CreateSurface(), this will allocate memory in Memory::Coherent(not Memory::Fb)
    rc = CreateSurface(pSurface, Size);

    if (rc != OK)
    {
       Printf(Tee::PriHigh, "AllocateMessageBuffer failed, size = 0x%x \n.", Size);
    }

    return rc;
}
