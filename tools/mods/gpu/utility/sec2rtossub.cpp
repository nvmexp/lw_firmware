/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "sec2rtossub.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/lwrm.h"
#include "lwRmApi.h"
#include "lwmisc.h"
#include "core/include/utility.h"
#include "lwtypes.h"
#include "lwos.h"
#include "rmsec2cmdif.h"
#include "core/include/platform.h"
#include "class/clb6b9.h"  // MAXWELL_SEC2
#include "ctrl/ctrlb6b9.h" // MAXWELL_SEC2

#define ROUND32(a) (((a) + 3) & ~0x03U)

#define CMD_TIMEOUT_MS     1000    //!< Maximum time to wait for a command
#define CMD_EMU_TIMEOUT_MS 1200000 //!< Maximum time to wait for a command when running in emulation
#define BOOT_TIMEOUT_MS    10000   //!< Maximum time to wait for bootstrap

//-----------------------------------------------------------------------------
//! \brief Constructor
//!
Sec2Rtos::Sec2Rtos(GpuSubdevice *pParent)
 : m_Initialized(false)
 , m_Handle((LwRm::Handle)0)
 , m_Parent(pParent)
{
    m_Handle = 0;
}

//-----------------------------------------------------------------------------
//! \brief Destructor
//!
Sec2Rtos::~Sec2Rtos()
{
    Shutdown();
}

//-----------------------------------------------------------------------------
//! \brief Initialize the SEC2 object
//!
//! \return OK if initialization succeeds, not OK otherwise
//!
RC Sec2Rtos::Initialize()
{
    RC      rc;
    LwRmPtr pLwRm;

    Printf(Tee::PriLow, "SEC2::Initialize\n");
    // Validate that this GPU has a SEC2, if so, allocate and initialze it
    if (!pLwRm->IsClassSupported(MAXWELL_SEC2,
                                 m_Parent->GetParentDevice()))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    // Allocate Subdevice SEC2 handle
    CHECK_RC(pLwRm->Alloc(pLwRm->GetSubdeviceHandle(m_Parent),
                          &m_Handle,
                          MAXWELL_SEC2,
                          NULL));

    m_Initialized = true;

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Shutdown the SEC2
//!
void Sec2Rtos::Shutdown()
{
    LwRmPtr pLwRm;

    if (m_Handle)
    {
        pLwRm->Free(m_Handle);
        m_Handle = 0;
    }

    m_Initialized = false;
}

//-----------------------------------------------------------------------------
//! \brief Send a command to the SEC2
//!
//! \param pCmd          : Pointer to the command to send.
//! \param pMsg          : Pointer to storage for the command response message,
//!                        if NULL then the response is not returned and this
//!                        function exits right after command is sent. When the
//!                        pointer is not NULL, this function waits the response
//!                        until timeout.
//! \param pSeqNum       : Returned sequence number of the sent command, used to
//!                        filter on a particular command response
//!
//! \return OK if the command was successfuly sent and the requested msg
//!         returned, not OK otherwise
//!
RC Sec2Rtos::Command
(
    RM_FLCN_CMD_SEC2 *pCmd,
    RM_FLCN_MSG_SEC2 *pMsg,
    UINT32           *pSeqNum
)
{
    RC                                    rc;
    LWB6B9_CTRL_SEC2_SEND_CMD_PARAMS      sec2Cmd;
    Surface2D                             Command;
    Surface2D                             Message;
    Utility::CleanupOnReturn<Surface2D, void>
                                          FreeCommand(&Command,&Surface2D::Free);
    Utility::CleanupOnReturn<Surface2D, void>
                                          FreeMessage(&Message,&Surface2D::Free);

    MASSERT(m_Initialized);

    memset(&sec2Cmd, 0, sizeof(LWB6B9_CTRL_SEC2_SEND_CMD_PARAMS));

    CHECK_RC(CreateSurface(&Command, sizeof(RM_FLCN_CMD_SEC2)));

    // Copy the command into the surface
    Platform::MemCopy(Command.GetAddress(), pCmd, pCmd->hdr.size);
    Platform::FlushCpuWriteCombineBuffer();

    sec2Cmd.cmd.hMemory  = Command.GetMemHandle();
    sec2Cmd.cmd.offset   = 0;
    sec2Cmd.cmd.size     = sizeof(RM_FLCN_CMD_SEC2);
    sec2Cmd.queueId      = SEC2_RM_CMDQ_LOG_ID;

    if (pMsg)
    {
        CHECK_RC(CreateSurface(&Message, sizeof(RM_FLCN_MSG_SEC2)));

        sec2Cmd.msg.hMemory  = Message.GetMemHandle();
        sec2Cmd.msg.offset   = 0;
        sec2Cmd.msg.size     = sizeof(RM_FLCN_MSG_SEC2);
    }

    CHECK_RC(LwRmPtr()->Control(m_Handle,
                                LWB6B9_CTRL_CMD_SEC2_SEND_CMD,
                                &sec2Cmd,
                                sizeof(sec2Cmd)));

    if (pSeqNum)
    {
        *pSeqNum = sec2Cmd.seqDesc;
    }

    Printf(Tee::PriDebug, "SEC2::Command(): Command queued with seqDesc=%d."
                          "\n", *pSeqNum);
    DumpHeader((UINT08 *)Command.GetAddress() +
               offsetof(RM_FLCN_CMD_SEC2, hdr), COMMAND_HDR);

    if (pMsg)
    {
        Printf(Tee::PriDebug, "SEC2::Command(): Wait Message with seqDesc=%d "
                              "comes back.\n", *pSeqNum);
        //
        // When pMsg is requested, it *MUST* wait until the callback function
        // finishes its work of using 'Message'.
        //
        CHECK_RC(WaitMessage(sec2Cmd.seqDesc));

        Printf(Tee::PriHigh, "SEC2::Command(): Received Message with seqDesc"
                              "=%d.\n", *pSeqNum);

        // Copy the head
        Platform::MemCopy(pMsg, Message.GetAddress(), RM_FLCN_QUEUE_HDR_SIZE);

        if (pMsg->hdr.size > RM_FLCN_QUEUE_HDR_SIZE)
        {
            // Copy head + body
            Platform::MemCopy(pMsg, Message.GetAddress(), pMsg->hdr.size);
        }

        Platform::FlushCpuWriteCombineBuffer();
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Gets the current SEC2 ucode
//!
//! \param ucodeState : Pointer to the ucode state in which the current ucode
//!                     state will be saved.
//!
//! \return OK if the ucode state was fetched and saved, not OK otherwise
//!
RC Sec2Rtos::GetUcodeState
(
    UINT32 *ucodeState
)
{
    RC           rc;
    LwRmPtr      pLwRm;
    LWB6B9_CTRL_SEC2_UCODE_STATE_PARAMS   sec2StateParam = {0};

    MASSERT(m_Initialized);

    if (!ucodeState)
    {
        return RC::LWRM_ILWALID_PARAMETER;
    }

    CHECK_RC(pLwRm->Control(m_Handle,
                            LWB6B9_CTRL_CMD_SEC2_UCODE_STATE,
                            &sec2StateParam,
                            sizeof (LWB6B9_CTRL_SEC2_UCODE_STATE_PARAMS)));

    *ucodeState = sec2StateParam.ucodeState;

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Allocate and map a surface for use with the SEC2
//!
//! \param pSurface : Pointer to the surface to allocate
//! \param Size     : Size in bytes for the surface
//!
//! \return OK if successful
RC Sec2Rtos::CreateSurface(Surface2D *pSurface, UINT32 Size)
{
    RC rc;

    Size = ROUND32(Size);
    pSurface->SetWidth(GetWidth(&Size));
    pSurface->SetPitch(Size);
    pSurface->SetHeight(1);
    pSurface->SetColorFormat(ColorUtils::VOID32);
    pSurface->SetLocation(Memory::Coherent);
    pSurface->SetAddressModel(Memory::Paging);
    pSurface->SetKernelMapping(true);

    CHECK_RC(pSurface->Alloc(m_Parent->GetParentDevice()));
    Utility::CleanupOnReturn<Surface2D,void>
        FreeSurface(pSurface,&Surface2D::Free);
    CHECK_RC(pSurface->Fill(0, m_Parent->GetSubdeviceInst()));
    CHECK_RC(pSurface->Map(m_Parent->GetSubdeviceInst()));
    FreeSurface.Cancel();

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Get the width for a SEC2 surface based on the desired pitch.  If the
//!        pitch requires alignment to meet bytes per pixel requirement, then
//!        the pitch will also be updated
//!
//! \param pPitch     : Pointer to the desired pitch (altered it it does not
//!                     meet the bytes-per-pixel alignment requirement)
//!
//! \return Width of the surface in pixels
UINT32 Sec2Rtos::GetWidth(UINT32 *pPitch)
{
    UINT32 bytesPerPixel = ColorUtils::PixelBytes(ColorUtils::VOID32);
    UINT32 width;
    width   = (*pPitch + bytesPerPixel - 1) / bytesPerPixel;
    *pPitch = width * bytesPerPixel;

    return width;
}

//-----------------------------------------------------------------------------
//! \brief Dump the header from the SEC2 command, event or message
//!
//! \param pHdr    : Pointer to the header to dump
//! \param hdrType : Type of header to dump
//!
void Sec2Rtos::DumpHeader(void *pHdr, Sec2HeaderType hdrType)
{
    if (!Tee::WillPrint(Tee::PriDebug))
        return;

    if (hdrType == COMMAND_HDR)
    {
        RM_FLCN_QUEUE_HDR hdr;
        Platform::MemCopy(&hdr, pHdr, RM_FLCN_QUEUE_HDR_SIZE);
        Platform::FlushCpuWriteCombineBuffer();

        Printf(Tee::PriDebug,
               "SEC2 Command : unitId=0x%02x, size=0x%02x, "
               "ctrlFlags=0x%02x, seqNumId=0x%02x\n",
               hdr.unitId, hdr.size, hdr.ctrlFlags, hdr.seqNumId);
    }
    else
    {
        RM_FLCN_QUEUE_HDR hdr;
        Platform::MemCopy(&hdr, pHdr, RM_FLCN_QUEUE_HDR_SIZE);
        Platform::FlushCpuWriteCombineBuffer();

        Printf(Tee::PriDebug,
               "SEC2 %s : unitId=0x%02x, size=0x%02x, "
               "ctrlFlags=0x%02x, seqNumId=0x%02x\n",
               (hdrType == EVENT_HDR) ? "Event" : "Message",
               hdr.unitId, hdr.size, hdr.ctrlFlags, hdr.seqNumId);
    }
}

//-----------------------------------------------------------------------------
//! \brief Wait for SEC2 Message with seqnum to be received
//!        The implementation is different from PMU::WaitMessage()
//!
//! \return OK if the Message has been received, not OK otherwise
//!
RC Sec2Rtos::WaitMessage(UINT32 seqNum)
{
    PollMsgArgs pollArgs;
    RC          rc;
    UINT32      timeout;

    pollArgs.pThis = this;
    pollArgs.seqNum = seqNum;

    // Use a larger timeout for emulation
    timeout = m_Parent->IsEmulation() ? CMD_EMU_TIMEOUT_MS : CMD_TIMEOUT_MS;

    rc = POLLWRAP_HW(Sec2Rtos::PollMsgReceived, &pollArgs, timeout);

    if (rc == OK)
    {
        return pollArgs.pollRc;
    }
    else
    {
        return rc;
    }
}

//-----------------------------------------------------------------------------
//! \brief Static polling function to determine if a SEC2 message have been
//!        received
//!
//! \param pVoidPollArgs  : Pointer to data structure PollMsgArgs that includes
//!            pThis      : Pointer to the SEC2 instance
//!            seqNum     : The sequence number of the target message
//!
//! \return true if the SEC2 message has been received or an error stops polling,
//!         false otherwise
//!
bool Sec2Rtos::PollMsgReceived(void *pVoidPollArgs)
{
    PollMsgArgs *pPollArgs = static_cast<PollMsgArgs *> (pVoidPollArgs);
    Sec2Rtos    *pThis;
    UINT32       seqNum;
    bool         bCommandComplete;
    RC           rc;

    pThis  = pPollArgs->pThis;
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

//-----------------------------------------------------------------------------
//! \brief Check if a particular command has completed
//!
//! \param SeqNum      : Sequence number of the command to check for completion
//! \param pbComplete  : Returned boolean to indicate if the command is complete
//!
//! \return OK if successful, not OK otherwise
//!
RC Sec2Rtos::IsCommandComplete(UINT32 seqNum, bool *pbComplete)
{
    RC        rc;
    LwRmPtr   pLwRm;
    LWB6B9_CTRL_SEC2_CMD_STATUS_PARAMS  sec2CmdStatus = {0};

    MASSERT(pbComplete);
    *pbComplete = false;

    // Get the status for the current command
    sec2CmdStatus.seqDesc = seqNum;
    CHECK_RC(pLwRm->Control(m_Handle,
                            LWB6B9_CTRL_CMD_SEC2_CMD_STATUS,
                            &sec2CmdStatus,
                            sizeof(sec2CmdStatus)));

    // There are three possible status returns (NONE which means
    // the command does not exist, RUNNING which means that the
    // command has not yet completed, and DONE which means the
    // command has completed
    if (sec2CmdStatus.seqStatus == LWB6B9_CTRL_SEC2_CMD_STATUS_DONE)
    {
        *pbComplete = true;
    }
    else if (sec2CmdStatus.seqStatus == LWB6B9_CTRL_SEC2_CMD_STATUS_NONE)
    {
        MASSERT(!"IsCommandComplete : invalid command sequence\n");
        rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}
//-----------------------------------------------------------------------------
//! \brief Wait for the SEC2 to be ready to receive commands
//!
//! \return OK if the SEC2 is ready, not OK otherwise
//!
RC Sec2Rtos::WaitSEC2Ready()
{
    PollSec2Args    pollArgs;
    RC              rc;

    pollArgs.pThis = this;

    rc = POLLWRAP_HW(Sec2Rtos::PollSec2Ready, &pollArgs, BOOT_TIMEOUT_MS);
    if (rc == OK)
    {
        return pollArgs.pollRc;
    }
    else
    {
        return rc;
    }
}

//-----------------------------------------------------------------------------
//! \brief Static polling function to determine if SEC2 is ready
//!
//! \return true if SEC2 is ready or an error stops polling, false otherwise
//!
bool Sec2Rtos::PollSec2Ready(void *pVoidPollArgs)
{
    PollSec2Args  *pPollArgs = static_cast<PollSec2Args *> (pVoidPollArgs);
    Sec2Rtos      *pThis;
    RC             rc;
    LwRmPtr        pLwRm;
    LWB6B9_CTRL_SEC2_UCODE_STATE_PARAMS  sec2StateParam = {0};

    pThis = pPollArgs->pThis;

    rc = pLwRm->Control(pThis->m_Handle,
                        LWB6B9_CTRL_CMD_SEC2_UCODE_STATE,
                        &sec2StateParam,
                        sizeof (sec2StateParam));

    pPollArgs->pollRc = rc;

    Printf(Tee::PriDebug,
           "SEC2 Polling for ready\n");

    if (rc != OK)
    {
        Printf(Tee::PriHigh,
               "Error RC: 0x%08x\n", rc.Get());
    }

    if (rc == OK &&
        sec2StateParam.ucodeState != LWB6B9_CTRL_SEC2_UCODE_STATE_READY)
    {
        // keep polling
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
//! \brief Check if the given testId is supported or not
//!
//! \return true if the testId is supported, false if not
//!
bool Sec2Rtos::CheckTestSupported(UINT08 testId)
{
    LWB6B9_CTRL_SEC2_TEST_SUPPORTED_PARAMS sec2TestSupported;

    sec2TestSupported.testId = testId;
    sec2TestSupported.bTestSupported = false;

    if (LwRmPtr()->Control(m_Handle,
                          LWB6B9_CTRL_CMD_SEC2_TEST_SUPPORTED,
                          &sec2TestSupported,
                          sizeof(sec2TestSupported)) == OK)
    {
        return (sec2TestSupported.bTestSupported != LW_FALSE);
    }
    return false;
}

//-----------------------------------------------------------------------------
//! \brief Check if an RM Handle to SEC2 has been allocated before, if so
//! copy over the handle into the argument
//!
//! \return true if the handle exists, false if not
//!
bool Sec2Rtos::GetAllocatedRmHandle(LwRm::Handle *pHandle)
{
    if (!m_Handle || !pHandle)
    {
        return false;
    }

    *pHandle = m_Handle;
    return true;
}
