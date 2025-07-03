/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "uprocrtos.h"
#include "uprocsec2rtos.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/utility/sec2rtossub.h"
#include "lwRmApi.h"
#include "core/include/platform.h"
#include "class/clb6b9.h"   // MAXWELL_SEC2
#include "ctrl/ctrlb6b9.h"  // MAXWELL_SEC2
#include "class/cl95a1.h"   // LW95A1_TSEC
#include "class/cl0070.h"   // LW01_MEMORY_VIRTUAL
#include "gpu/tests/rmtest.h"
#include "rmsec2cmdif.h"
#include "uprocifcmn.h"
#include "uprociftest.h"

//!
//! \brief Constructor
//!
UprocSec2Rtos::UprocSec2Rtos(GpuSubdevice *pParent)
 : UprocRtos (pParent)
{
    bHandleIsOursToFree = true;
    m_UprocId = UPROC_ID_SEC2;
    m_bSetupFbSurface = false;
    m_bSetupUprocChannel = false;
    m_pTestConfig = nullptr;
    m_pCh = nullptr;
    m_gpuAddr = 0;
    m_cpuAddr = nullptr;
    m_hCh = 0;
    m_hObj = 0;
    m_hSemMem = 0;
    m_hVA = 0;
}

//!
//! \brief Destructor
//!
UprocSec2Rtos::~UprocSec2Rtos()
{
    Shutdown();
}

//!
//! \brief Shutdown the SEC2
//!
void UprocSec2Rtos::Shutdown()
{
    LwRmPtr pLwRm;

    if (m_Handle && bHandleIsOursToFree)
    {
        pLwRm->Free(m_Handle);
    }

    m_Handle = 0;
    m_Initialized = false;
}

//!
//! \brief      Allocate the SEC2 handle
//!
//! \return     OK if no errors, otherwise corresponding RC
//!
RC UprocSec2Rtos::Initialize()
{
    RC      rc;
    LwRmPtr pLwRm;

    Printf(Tee::PriLow, "SEC2 UPROC::Initialize\n");
    // Validate that this GPU has a SEC2, if so, allocate and initialze it
    if (!pLwRm->IsClassSupported(MAXWELL_SEC2,
                                 m_Subdevice->GetParentDevice()))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
/* !! DEPRECATION ALERT - This code will be deprecated when we remove legacy Sec2RtosSub test framework!! !! */
    Sec2Rtos *pSec2Rtos;
    CHECK_RC(m_Subdevice->GetSec2Rtos(&pSec2Rtos));

    if (pSec2Rtos->GetAllocatedRmHandle(&m_Handle))
    {
        bHandleIsOursToFree = false;
    }
    else
    {
/* !! DEPRECATION ALERT - This code will be deprecated when we remove legacy Sec2RtosSub test framework!! !! */
        // Allocate Subdevice SEC2 handle
        CHECK_RC(pLwRm->Alloc(pLwRm->GetSubdeviceHandle(m_Subdevice),
                              &m_Handle,
                              MAXWELL_SEC2,
                              NULL));
        bHandleIsOursToFree = true;
    }
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
RC UprocSec2Rtos::IsCommandComplete(LwU32 seqNum, bool *pbComplete)
{
    RC        rc = OK;
    LwRmPtr   pLwRm;
    LWB6B9_CTRL_SEC2_CMD_STATUS_PARAMS  sec2CmdStatus = {0};

    MASSERT(pbComplete);

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
    else if (sec2CmdStatus.seqStatus == LWB6B9_CTRL_SEC2_CMD_STATUS_RUNNING)
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
bool UprocSec2Rtos::PollMsgReceived(void *pVoidPollArgs)
{
    PollMsgArgs         *pPollArgs = static_cast<PollMsgArgs *> (pVoidPollArgs);
    UprocSec2Rtos       *pThis;
    UINT32              seqNum;
    bool                bCommandComplete;
    RC                  rc;

    pThis  = pPollArgs->pSec2Rtos;
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

//! \brief Wait for SEC2 Message with seqnum to be received
//!        The implementation is different from PMU::WaitMessage()
//!
//! \return OK if the Message has been received, not OK otherwise
//!
RC UprocSec2Rtos::WaitMessage(LwU32 seqNum)
{
    PollMsgArgs pollArgs;
    RC          rc;
    LwU32       timeout;

    pollArgs.pSec2Rtos = this;
    pollArgs.seqNum = seqNum;

    // Use a larger timeout for emulation
    timeout = m_Subdevice->IsEmulation() ? CMD_EMU_TIMEOUT_MS : CMD_TIMEOUT_MS;

    rc = POLLWRAP_HW(UprocSec2Rtos::PollMsgReceived, &pollArgs, timeout);

    if (rc == OK)
    {
        return pollArgs.pollRc;
    }
    else
    {
        return rc;
    }
}

//! \brief Wait for the SEC2 to be ready to receive commands
//!
//! \return OK if the SEC2 is ready, not OK otherwise
//!
RC UprocSec2Rtos::WaitUprocReady()
{
    PollSec2Args    pollArgs;
    RC              rc;

    pollArgs.pSec2Rtos = this;

    rc = POLLWRAP_HW(UprocSec2Rtos::PollSec2Ready, &pollArgs, BOOT_TIMEOUT_MS);
    if (rc == OK)
    {
        return pollArgs.pollRc;
    }
    else
    {
        return rc;
    }
}

//! \brief Static polling function to determine if SEC2 is ready
//!
//! \return true if SEC2 is ready or an error stops polling, false otherwise
//!
bool UprocSec2Rtos::PollSec2Ready(void *pVoidPollArgs)
{
    PollSec2Args        *pPollArgs = static_cast<PollSec2Args *> (pVoidPollArgs);
    UprocSec2Rtos       *pThis;
    RC                  rc;
    LwRmPtr             pLwRm;
    LWB6B9_CTRL_SEC2_UCODE_STATE_PARAMS  sec2StateParam = {0};

    pThis = pPollArgs->pSec2Rtos;

    rc = pLwRm->Control(pThis->m_Handle,
                        LWB6B9_CTRL_CMD_SEC2_UCODE_STATE,
                        &sec2StateParam,
                        sizeof (sec2StateParam));

    pPollArgs->pollRc = rc;

    Printf(Tee::PriDebug,
           "SEC2 UPROC Polling for ready\n");

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

//! \brief Send a command to the SEC2 uproc
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
RC UprocSec2Rtos::Command
(
    RM_UPROC_TEST_CMD   *inCmd,
    RM_UPROC_TEST_MSG   *inMsg,
    RM_FLCN_QUEUE_HDR   *inQueueHdr,
    LwU32               *pSeqNum
)
{
    RC                                    rc;
    LWB6B9_CTRL_SEC2_SEND_CMD_PARAMS      sec2Cmd;
    Surface2D                             Command;
    Surface2D                             Message;
    RM_FLCN_CMD_SEC2                      sec2FlcCmd;
    RM_FLCN_MSG_SEC2                      sec2FlcMsg;

    MASSERT(m_Initialized);

    if (!inCmd || !inQueueHdr || !pSeqNum)
    {
        Printf(Tee::PriHigh,
                "%d:UprocSec2Rtos::%s: Bad args passed!\n",
                __LINE__, __FUNCTION__);
        return RC::BAD_PARAMETER;
    }

    memset(&sec2FlcCmd, 0, sizeof(RM_FLCN_CMD_SEC2));
    memset(&sec2FlcMsg, 0, sizeof(RM_FLCN_MSG_SEC2));
    memset(&sec2Cmd, 0, sizeof(LWB6B9_CTRL_SEC2_SEND_CMD_PARAMS));

    Utility::CleanupOnReturn<Surface2D, void>
                                          FreeCommand(&Command,&Surface2D::Free);
    Utility::CleanupOnReturn<Surface2D, void>
                                          FreeMessage(&Message,&Surface2D::Free);

    sec2FlcCmd.hdr.unitId            = (inQueueHdr->unitId == RM_UPROC_UNIT_NULL)
                                                 ? RM_SEC2_UNIT_NULL : RM_SEC2_UNIT_TEST;
    sec2FlcCmd.hdr.size              = inQueueHdr->size;

    // inCmd & sec2FlcCmd.cmd are identical struct layout for test commands - hence this will work
    //But only copy bytes we know we have set in inCmd, otherwise we read off the end of inCmd.
    Platform::MemCopy(&sec2FlcCmd.cmd, inCmd, sizeof(RM_UPROC_TEST_CMD));

    CHECK_RC(CreateSurface(&Command, sizeof(RM_FLCN_CMD_SEC2)));

    // Copy the command into the surface
    Platform::MemCopy(Command.GetAddress(), &sec2FlcCmd, sec2FlcCmd.hdr.size);
    Platform::FlushCpuWriteCombineBuffer();

    sec2Cmd.cmd.hMemory  = Command.GetMemHandle();
    sec2Cmd.cmd.offset   = 0;
    sec2Cmd.cmd.size     = sizeof(RM_FLCN_CMD_SEC2);
    sec2Cmd.queueId      = SEC2_RM_CMDQ_LOG_ID;


    CHECK_RC(CreateSurface(&Message, sizeof(RM_FLCN_MSG_SEC2)));

    sec2Cmd.msg.hMemory  = Message.GetMemHandle();
    sec2Cmd.msg.offset   = 0;
    sec2Cmd.msg.size     = sizeof(RM_FLCN_MSG_SEC2);


    CHECK_RC(LwRmPtr()->Control(m_Handle,
                                LWB6B9_CTRL_CMD_SEC2_SEND_CMD,
                                &sec2Cmd,
                                sizeof(sec2Cmd)));

    *pSeqNum = sec2Cmd.seqDesc;

    Printf(Tee::PriDebug, "SEC2::Command(): Command queued with seqDesc=%d."
                          "\n", *pSeqNum);

    DumpHeader((UINT08 *)Command.GetAddress() +
               offsetof(RM_FLCN_CMD_SEC2, hdr), COMMAND_HDR);

    if (inMsg)
    {
        Printf(Tee::PriDebug, "SEC2::Command(): Wait Message with seqDesc=%d "
                              "comes back.\n", *pSeqNum);

        //
        // When inMsg is requested, it *MUST* wait until the callback function
        // finishes its work of using 'Message'.
        //
        CHECK_RC(WaitMessage(sec2Cmd.seqDesc));


        Printf(Tee::PriHigh, "SEC2::Command(): Received Message with seqDesc"
                              "=%d.\n", *pSeqNum);

        // Copy the head
        Platform::MemCopy(&sec2FlcMsg, Message.GetAddress(), RM_FLCN_QUEUE_HDR_SIZE);

        if (sec2FlcMsg.hdr.size > RM_FLCN_QUEUE_HDR_SIZE)
        {
            // Copy head + body
            Platform::MemCopy(&sec2FlcMsg, Message.GetAddress(), sec2FlcMsg.hdr.size);
        }

        // response message received, print out the details
        Printf(Tee::PriHigh,
               "%d:UprocSec2Rtos::%s: Received command response from SEC2 UPROC\n",
                __LINE__, __FUNCTION__);
        Printf(Tee::PriHigh,
               "%d:UprocSec2Rtos::%s: Printing Header :-\n",
                __LINE__, __FUNCTION__);
        Printf(Tee::PriHigh,
               "unitID = 0x%x ,size = 0x%x ,ctrlFlags  = 0x%x ,seqNumID = 0x%x\n",
                sec2FlcMsg.hdr.unitId, sec2FlcMsg.hdr.size,
                sec2FlcMsg.hdr.ctrlFlags, sec2FlcMsg.hdr.seqNumId);

        Platform::FlushCpuWriteCombineBuffer();

        // copy back the test msg
        Platform::MemCopy(inMsg, &sec2FlcMsg.msg.test, sizeof(RM_UPROC_TEST_MSG));
    }

    return rc;
}

//! \brief Check if the given testId is supported or not
//!
//! \return true if the testId is supported, false if not
//!
bool UprocSec2Rtos::CheckTestSupported(LwU8 testId)
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

//! \brief Allocate buffer in FB for MSCG fb access test.
//!
//! \return OK if we succeed in allocating surface, error RC otherwise
//!
RC UprocSec2Rtos::SetupFbSurface(LwU32 size)
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

//! \brief Allocate channel for MSCG method access test.
//!
//! \return OK if we succeed in setting channel up, error RC otherwise
//!
RC UprocSec2Rtos::SetupUprocChannel(GpuTestConfiguration *testConfig)
{
    const LwU32 memSize = 0x1000;
    LwRmPtr     pLwRm;
    RC          rc;

    m_hCh = m_pRt->m_hCh;
    m_pCh = m_pRt->m_pCh;

    CHECK_RC(testConfig->AllocateChannel(&m_pCh, &m_hCh));

    CHECK_RC(pLwRm->Alloc(m_hCh, &m_hObj, LW95A1_TSEC, NULL));

    CHECK_RC(pLwRm->AllocSystemMemory(&m_hSemMem,
             DRF_DEF(OS02, _FLAGS, _PHYSICALITY, _NONCONTIGUOUS) |
             DRF_DEF(OS02, _FLAGS, _LOCATION, _PCI) |
             DRF_DEF(OS02, _FLAGS, _COHERENCY, _UNCACHED),
             memSize, m_Subdevice->GetParentDevice()));

    LW_MEMORY_VIRTUAL_ALLOCATION_PARAMS vmparams = { 0 };
    CHECK_RC(pLwRm->Alloc(pLwRm->GetDeviceHandle(m_Subdevice->GetParentDevice()),
                          &m_hVA, LW01_MEMORY_VIRTUAL, &vmparams));

    CHECK_RC(pLwRm->MapMemoryDma(m_hVA, m_hSemMem, 0, memSize,
                                 LW04_MAP_MEMORY_FLAGS_NONE, &m_gpuAddr, m_Subdevice->GetParentDevice()));
    CHECK_RC(pLwRm->MapMemory(m_hSemMem, 0, memSize, (void **)&m_cpuAddr, 0, m_Subdevice));

    // [NJ] I moved this from the test layer into the library function
    //
    // Write a known pattern to the semaphore location to initialize it
    // and don't keep writing it over and over again for every loop of
    // the test to avoid FB accesses.
    //
    MEM_WR32(m_cpuAddr, 0xdeaddead);
    m_bSetupUprocChannel = true;
    m_pTestConfig = testConfig; //save pointer so we can clean up in FreeResources
    return rc;
}

//! \brief Free allocated resources used in MSCG test.
//!
//! \return OK if we successfully free resources, else RC
//!
RC UprocSec2Rtos::FreeResources()
{
    LwRmPtr pLwRm;
    RC rc = OK;
    if (m_bSetupFbSurface == true)
    {
        m_fbmemSurface.Unmap();
        m_fbmemSurface.Free();
    }
    if (m_bSetupUprocChannel == true)
    {
        pLwRm->UnmapMemory(m_hSemMem, m_cpuAddr, 0, m_Subdevice);
        pLwRm->UnmapMemoryDma(m_hVA, m_hSemMem,
                              LW04_MAP_MEMORY_FLAGS_NONE, m_gpuAddr, m_Subdevice->GetParentDevice());
        pLwRm->Free(m_hVA);
        pLwRm->Free(m_hSemMem);
        pLwRm->Free(m_hObj);
        m_pTestConfig->FreeChannel(m_pCh);
        m_pCh = NULL;
    }
    return rc;
}

//! \brief Poll channel for value
//!
//! \return true if the polled value matches expected value, false otherwise
//!
bool UprocSec2Rtos::PollUprocSemaphoreVal(void *pVoidPollArgs)
{
    PollSemaphoreArgs *pArgLocal = (PollSemaphoreArgs *)pVoidPollArgs;
    if (MEM_RD32(pArgLocal->pAddr) == pArgLocal->value)
    {
        Printf(Tee::PriDebug, "Poll exit Success \n");
        return true;
    }
    else
    {
        return false;
    }
}

//! \brief Send command to Uproc to disengage MSCG.
//!
//! \return OK if we successfully send command to uproc, else RC
//!
RC UprocSec2Rtos::SendUprocTraffic(LwU32 wakeupMethod, FLOAT64 timeOutMs)
{
    RC                      rc     = OK;
    UINT32                  seqNum = 0;
    UINT32                  ret    = 0;

    if (wakeupMethod == UPROCRTOS_UPROC_PB_ACTIVITY)
    {
        PollSemaphoreArgs args;
        static UINT32 loop = 0;
        UINT32 semaphoreVal = (0x12341234 + loop++); // Update semaphore release value in every loop
        UINT32 gotVal;

        m_pCh->SetObject(0, m_hObj);

        Printf(Tee::PriHigh, "Sending methods to SEC2 channel.\n");
        // Send methods to release semaphore with a different value
        m_pCh->Write(0, LW95A1_SEMAPHORE_A,
            DRF_NUM(95A1, _SEMAPHORE_A, _UPPER, (UINT32)(m_gpuAddr >> 32LL)));
        m_pCh->Write(0, LW95A1_SEMAPHORE_B, (UINT32)(m_gpuAddr & 0xffffffffLL));
        m_pCh->Write(0, LW95A1_SEMAPHORE_C,
            DRF_NUM(95A1, _SEMAPHORE_C, _PAYLOAD, semaphoreVal));
        m_pCh->Write(0, LW95A1_SEMAPHORE_D,
            DRF_DEF(95A1, _SEMAPHORE_D, _STRUCTURE_SIZE, _ONE) |
            DRF_DEF(95A1, _SEMAPHORE_D, _AWAKEN_ENABLE, _FALSE));
        m_pCh->Flush();

        // Poll for semaphore value to change
        Printf(Tee::PriHigh, "Polling on semaphore release by SEC2.\n");
        args.value  = semaphoreVal;
        args.pAddr  = m_cpuAddr;

        rc = POLLWRAP(&UprocSec2Rtos::PollUprocSemaphoreVal, &args, timeOutMs);
        gotVal = MEM_RD32(args.pAddr);
        if (rc)
        {
            Printf(Tee::PriHigh,
                  "PollVal: expected value is 0x%x, got value 0x%x\n",
                  args.value, gotVal);
            return RC::DATA_MISMATCH;
        }
        Printf(Tee::PriHigh, "Succeeded semaphore release\n");
    }
    else if (wakeupMethod == UPROCRTOS_UPROC_REG_ACCESS)
    {
        RM_UPROC_TEST_CMD       uprocTestCmd = {0};
        RM_UPROC_TEST_MSG       uprocTestMsg = {0};
        RM_FLCN_QUEUE_HDR       falconHdr    = {0};

        memset(&uprocTestCmd, 0, sizeof(RM_UPROC_TEST_CMD));
        memset(&uprocTestMsg, 0, sizeof(RM_UPROC_TEST_MSG));
        memset(&falconHdr,    0, sizeof(RM_FLCN_QUEUE_HDR));

        // Send a TEST command to SEC2
        falconHdr.unitId            = RM_SEC2_UNIT_TEST;
        falconHdr.size              = RM_UPROC_CMD_SIZE(TEST, RD_BLACKLISTED_REG);
        uprocTestCmd.cmdType        = RM_UPROC_CMD_TYPE(TEST, RD_BLACKLISTED_REG);

        Printf(Tee::PriHigh, "Submitting a command to SEC2 to read a clock gated register.\n");

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
        memset(&falconHdr,    0, sizeof(RM_FLCN_QUEUE_HDR));

        falconHdr.unitId                       = RM_SEC2_UNIT_TEST;
        falconHdr.size                         = RM_UPROC_CMD_SIZE(TEST, MSCG_ISSUE_FB_ACCESS);
        uprocTestCmd.cmdType                   = RM_UPROC_CMD_TYPE(TEST, MSCG_ISSUE_FB_ACCESS);
        uprocTestCmd.mscgFbAccess.fbOffsetLo32 = physAddr & 0xFFFFFFFFULL;
        uprocTestCmd.mscgFbAccess.fbOffsetHi32 = physAddr >> 32;
        uprocTestCmd.mscgFbAccess.op           = MSCG_ISSUE_FB_ACCESS_WRITE;

        Printf(Tee::PriHigh, "Submitting a command to SEC2 to write on FB region.\n");

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

        Printf(Tee::PriHigh, "Submitting a command to SEC2 to read on FB region.\n");

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
        Printf(Tee::PriHigh, "Unknown SEC2 wakeup request recieved.\n");
        return RC::SOFTWARE_ERROR;
    }
    return rc;
}

//! \brief Send command to Uproc to disengage MSCG.
//!
//! \return mask of acceptable MSCG wakeup reason values. Returns 0 if invalid
//!         wakeup reason is specified, which will automatically fail MSCG test.
//!
LwU32 UprocSec2Rtos::GetMscgWakeupReasonMask(LwU32 wakeupMethod)
{
    if (wakeupMethod == UPROCRTOS_UPROC_REG_ACCESS)
    {
        return 0x100000; //PG_WAKEUP_EVENT_SEC2_PRIV_BLOCKER in pmu_objpg.h - which cannot be included here.
    }
    else if (wakeupMethod == UPROCRTOS_UPROC_FB_ACCESS)
    {
        return 0x80000; //PG_WAKEUP_EVENT_SEC2_FB_BLOCKER in pmu_objpg.h - which cannot be included here.
    }
    else if (wakeupMethod == UPROCRTOS_UPROC_PB_ACTIVITY)
    {
        return 0xFFFFFFFF; //PB_ACTIVITY does not produce unique signature. Waive strict test.
    }
    else
    {
        return 0; //Unknown wakeup method, fail strict test.
    }
}
