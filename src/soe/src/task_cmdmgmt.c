/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task_cmdmgmt.c
 * @brief  Command management task that is responsible for initializing all
 *         queues (command and message queues) as well as dispatching all
 *         unit tasks as commands arrive in the command queues.
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "soesw.h"
/* ------------------------- Application Includes --------------------------- */
#include "soe_cmdmgmt.h"
#include "soe_objbif.h"
#include "soe_objcore.h"
#include "soe_objsmbpbi.h"
#include "soe_objsoe.h"
#include "soe_objspi.h"
#include "soe_objic.h"
#include "soe_objifr.h"

#include "main.h"
#ifndef EXCLUDE_LWOSDEBUG
#include "lwosdebug.h"
#endif //EXCLUDE_LWOSDEBUG
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */

/*!
 * Address defined in the linker script to mark the memory shared between SOE
 * and RM.  Specifically, this marks the starting offset for the chunk of DMEM
 * that is managed by the RM.
 */
extern LwU32    _end_rmqueue[];

/*!
 * Address defined in the linker script to mark the end of the SOE DMEM.  This
 * offset combined with @ref _end_rmqueue define the size of the RM-managed
 * portion of the DMEM.
 */
extern LwU32    _end_physical_dmem[];

/*!
 * Address defined in the linker script to mark the end of SOE EMEM
 */
extern LwU32    _end_emem[];


/* ------------------------- Global Variables ------------------------------- */
/*!
 * Array containing the start offsets in DMEM of all command queues.
 */
LwU32              SoeMgmtCmdOffset[SOE_CMDMGMT_CMD_QUEUE_NUM];

/*!
 * Array containing the start offsets in DMEM of all message queues.
 */
LwU32              SoeMgmtMsgOffset[SOE_CMDMGMT_CMD_QUEUE_NUM];

/*!
 * A dispatch queue for sending commands to the command dispatcher task
 */
LwrtosQueueHandle  SoeCmdMgmtCmdDispQueue;

/* ------------------------- Static Variables ------------------------------- */

/*!
 * Represents the SOE command queue(s). For now, we only have one such.
 */
static LwU32 RM_SOE_CmdQueue[SOE_CMDMGMT_CMD_QUEUE_NUM][PROFILE_CMD_QUEUE_LENGTH/sizeof(LwU32)]
    GCC_ATTRIB_USED()
    GCC_ATTRIB_SECTION("rmrtos", "RM_SOE_CmdQueue");

/*!
 * Represents the SOE message queue(s). For now, we only have one such.
 */
static LwU32 RM_SOE_MsgQueue[SOE_CMDMGMT_MSG_QUEUE_NUM][PROFILE_MSG_QUEUE_LENGTH/sizeof(LwU32)]
    GCC_ATTRIB_USED()
    GCC_ATTRIB_SECTION("rmrtos", "RM_SOE_MsgQueue");

/*!
 * Keeps track of the locations within each command queue where the next
 * command will be dispatched from.
 */
static LwU32 SoeMgmtLastHandledCmdPtr[SOE_CMDMGMT_MSG_QUEUE_NUM];

/* ------------------------- Prototypes ------------------------------------- */

/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * Responsible for initializing the head and tail pointers for the message
 * queue and all command queues as well as collecting that information and
 * sending it to the RM as part of the INIT message.
 */
static void
InitQueues(void)
{
    RM_FLCN_QUEUE_HDR           hdr;
    RM_SOE_INIT_MSG_SOE_INIT    msg = {0};
    LwU32                       i;
    // physical to logical mapping for cmd queues
    LwU8 cmdIndexToId[] = {SOE_RM_CMDQ_LOG_ID};
    // physical to logical mapping for msg queues
    LwU8 msgIndexToId[] = {SOE_RM_MSGQ_LOG_ID};

    //
    // Iterate over all command queues setting up the head and tail pointers
    // for each queue.  At the same time, start building up the INIT message.
    //
    for (i = SOE_CMDMGMT_CMD_QUEUE_START; i < SOE_CMDMGMT_CMD_QUEUE_NUM; i++)
    {
        SoeMgmtCmdOffset[i] = (LwU32)(RM_SOE_CmdQueue) +
                               (i * PROFILE_CMD_QUEUE_LENGTH/sizeof(LwU32));
        SoeMgmtLastHandledCmdPtr[i] = SoeMgmtCmdOffset[i];

        soeWriteCmdQueueTail_HAL(&Soe, i, SoeMgmtCmdOffset[i]);
        soeWriteCmdQueueHead_HAL(&Soe, i, SoeMgmtCmdOffset[i]);

        msg.qInfo[i].queueSize   = PROFILE_CMD_QUEUE_LENGTH;
        msg.qInfo[i].queueOffset = (LwU32)SoeMgmtCmdOffset[i];
        msg.qInfo[i].queuePhyId  = (LwU8)i;
        msg.qInfo[i].queueLogId  = cmdIndexToId[i];
    }

    msg.numQueues = (LwU8)(SOE_CMDMGMT_CMD_QUEUE_NUM + SOE_CMDMGMT_MSG_QUEUE_NUM);

    // clear the queues
    memset((void *)(RM_SOE_CmdQueue)   , 0, sizeof(RM_SOE_CmdQueue));
    memset((void *)(RM_SOE_MsgQueue)   , 0, sizeof(RM_SOE_MsgQueue));

    //
    // Command queues are initialized, so we can unmask command intr now.
    // Note that the first write to the queue head to initialize the command
    // queue will raise a command queue interrupt. HW raises an interrupt
    // whenever a write to head oclwrs - it does not check if head and tail are
    // the same before deciding to raise an interrupt. However, that is okay
    // since we will just quietly return, seeing empty queue(s).
    //
    icCmdQueueIntrUnmask_HAL();

    // We have only one message queue
    i = SOE_CMDMGMT_MSG_QUEUE_START;

    // setup the head and tail pointers of the message queue
    SoeMgmtMsgOffset[i] = (LwU32)(RM_SOE_MsgQueue) +
                           (i * PROFILE_MSG_QUEUE_LENGTH/sizeof(LwU32));
    soeQueueRmInit_HAL(&Soe, SoeMgmtMsgOffset[i]);

    // populate message queue data in the INIT message
    msg.qInfo[i + SOE_CMDMGMT_CMD_QUEUE_NUM].queueSize   = PROFILE_MSG_QUEUE_LENGTH;
    msg.qInfo[i + SOE_CMDMGMT_CMD_QUEUE_NUM].queueOffset = (LwU32)SoeMgmtMsgOffset[i];
    msg.qInfo[i + SOE_CMDMGMT_CMD_QUEUE_NUM].queuePhyId  = i;
    msg.qInfo[i + SOE_CMDMGMT_CMD_QUEUE_NUM].queueLogId  = msgIndexToId[i];

#ifndef EXCLUDE_LWOSDEBUG
    // populate pointer to SOE debug information buffer
    msg.osDebugEntryPoint = (LwU16)(LwU32)(&OsDebugEntryPoint);
#endif //EXCLUDE_LWOSDEBUG

    // fill-in the header of the INIT message and send it to the RM
    hdr.unitId    = RM_SOE_UNIT_INIT;
    hdr.ctrlFlags = RM_FLCN_QUEUE_HDR_FLAGS_EVENT;
    hdr.seqNumId  = 0;
    hdr.size      = RM_SOE_MSG_SIZE(INIT, SOE_INIT);
    msg.msgType   = RM_SOE_MSG_TYPE(INIT, SOE_INIT);

    {
        OsCmdmgmtQueueDescriptorRM.start =
            SoeMgmtMsgOffset[i];
        OsCmdmgmtQueueDescriptorRM.end =
            OsCmdmgmtQueueDescriptorRM.start + PROFILE_MSG_QUEUE_LENGTH;
        OsCmdmgmtQueueDescriptorRM.headGet  = soeQueueRmHeadGet_HAL_FNPTR();
        OsCmdmgmtQueueDescriptorRM.headSet  = soeQueueRmHeadSet_HAL_FNPTR();
        OsCmdmgmtQueueDescriptorRM.tailGet  = soeQueueRmTailGet_HAL_FNPTR();
        OsCmdmgmtQueueDescriptorRM.dataPost = &osCmdmgmtQueueWrite_DMEM;
    }

    // Release the MSG queue mutex before sending INIT message to RM.
    osCmdmgmtRmQueueMutexReleaseAndPost(&hdr, &msg);

    // Update the status to SOE MAILBOX register
    REG_WR32_STALL(CSB, SOE_STATUS_SCRATCH_REGISTER, SOE_BOOTSTRAP_SUCCESS);
}

/*!
 * Dispatches the appropriate task to process/service the given command.  The
 * task will be dispatched by writing a pointer to the command into the task's
 * respective dispatch queue.  From there, the scheduler will ensure that the
 * task will run when it is appropriate to do so.
 *
 * @param[in] pCmd     The command packet to needs serviced.
 * @param[in] queueId  The identifier for the queue that the command originated
 *                     from.  This is used by the task to retrieve the actual
 *                     command data.
 */
static void
cmdQueueDispatch
(
    RM_FLCN_CMD_SOE *pCmd,
    LwU8             queueId
)
{
    DISP2UNIT_CMD       disp2unitEvt;
    RM_FLCN_QUEUE_HDR   hdr;
    LwrtosQueueHandle   pQueue = NULL;

    disp2unitEvt.cmdQueueId = queueId;
    disp2unitEvt.pCmd       = pCmd;
    disp2unitEvt.eventType  = DISP2UNIT_EVT_COMMAND;

    switch(pCmd->hdr.unitId)
    {
        // UNIT_NULL simply replies with the same cmd
        case RM_SOE_UNIT_NULL:
            osCmdmgmtCmdQSweep(&pCmd->hdr, queueId);
            hdr.unitId      = pCmd->hdr.unitId;
            hdr.ctrlFlags   = 0;
            hdr.seqNumId    = pCmd->hdr.seqNumId;
            hdr.size        = RM_FLCN_QUEUE_HDR_SIZE;
            osCmdmgmtRmQueuePostBlocking(&hdr, NULL);
            return;

        case RM_SOE_UNIT_THERM:
            pQueue = Disp2QThermThd;
            break;

        case RM_SOE_UNIT_SMBPBI:
            pQueue = Disp2QSmbpbiThd;
            break;

        case RM_SOE_UNIT_SPI:
            pQueue = Disp2QSpiThd;
            break;

        case RM_SOE_UNIT_BIF:
            pQueue = Disp2QBifThd;
            break;

        case RM_SOE_UNIT_CORE:
            pQueue = Disp2QCoreThd;
            break;

        case RM_SOE_UNIT_IFR:
            pQueue = Disp2QIfrThd;
            break;

        case RM_SOE_UNIT_UNLOAD:
            hdr.unitId    = RM_SOE_UNIT_UNLOAD;
            hdr.ctrlFlags = 0;
            hdr.seqNumId  = pCmd->hdr.seqNumId;
            hdr.size      = RM_FLCN_QUEUE_HDR_SIZE;

            osCmdmgmtCmdQSweep(&pCmd->hdr, queueId);
            osCmdmgmtRmQueuePostBlocking(&hdr, NULL);

            //
            // Make sure this doesn't get switched out with the
            // priv level masks down.
            //
            lwrtosENTER_CRITICAL();
            {
                soeLowerPrivLevelMasks_HAL();

#if (SOECFG_FEATURE_ENABLED(BUG_200272812_HALT_INTR_MASK_WAR))
                //
                // Put falcon in wait instead of halt because of Bug 200272812
                // We are waiting inside critical section so this would never return
                //
                falc_wait(0);
#else
                // Mask the HALT interrupt before halting
                icHaltIntrMask_HAL();
                SOE_HALT();
#endif
            }
            lwrtosEXIT_CRITICAL();
            return;

        default:
            // Catch the case where we don't manage the UNIT ID
            DBG_PRINTF_CMDMGMT(("UNIT %d not managed by SOE\n", pCmd->hdr.unitId, 0, 0, 0));
            break;
    }

    if (pQueue != NULL)
    {
        lwrtosQueueSendBlocking(pQueue, &disp2unitEvt, sizeof(disp2unitEvt));
    }
}

/*!
 * Processes command from a queue
 *
 * @param[in]  queueId  Queue ID
 */
void
processCommand
(
    LwU8 queueId
)
{
    LwU32             head    = 0;
    RM_FLCN_CMD_SOE  *pCmd;

    DBG_PRINTF_CMDMGMT(("Task CmdMng Got cmd\n", 0, 0, 0, 0));

    //
    // We only support one cmd queue today, and only one cmd queue head intr is
    // enabled, so we can only get here if we got a cmd in that queue. When we
    // add support for more queues, we will need to use INTRSTAT to find out
    // which cmd queue(s) had pending items.
    //
    icCmdQueueIntrClear_HAL();
    head = soeReadCmdQueueHead_HAL(&Soe, queueId);

    while (SoeMgmtLastHandledCmdPtr[queueId] != head)
    {
        pCmd = (RM_FLCN_CMD_SOE *)SoeMgmtLastHandledCmdPtr[queueId];

        DBG_PRINTF_CMDMGMT(("SOE Rec soe_cmd 0x%x , head 0x%x\n",
            (LwU32)pCmd, head,0,0));

        DBG_PRINTF_CMDMGMT(("SOE UnitId %d size %d sq %d ctl %d\n",
                            pCmd->hdr.unitId,
                            pCmd->hdr.size,
                            pCmd->hdr.seqNumId,
                            pCmd->hdr.ctrlFlags));

        if (pCmd->hdr.unitId == RM_SOE_UNIT_REWIND)
        {
            SoeMgmtLastHandledCmdPtr[queueId] = SoeMgmtCmdOffset[queueId];
            DBG_PRINTF_CMDMGMT(("SOE Rec Rewind\n",0,0,0,0));
            // Head might have changed!
            head = soeReadCmdQueueHead_HAL(&Soe, queueId);
            osCmdmgmtCmdQSweep(&pCmd->hdr, queueId);
        }
        else
        {
            // must be done before dispatch
            SoeMgmtLastHandledCmdPtr[queueId] +=
                ALIGN_UP(pCmd->hdr.size, sizeof(LwU32));
            cmdQueueDispatch(pCmd, queueId);
        }
    }

    // Unmask the cmd queue interrupts (they were masked in ISR)
    icCmdQueueIntrUnmask_HAL();

    DBG_PRINTF_CMDMGMT(("SOE Done with Q%d\n", queueId,0,0,0));
}

/*!
 * Defines the entry-point for this task.  This function will execute forever
 * processing each command that arrives in the message queues and dispatching
 * unit task to handle each command.
 */
lwrtosTASK_FUNCTION(task_cmdmgmt, pvParameters)
{
    DISPATCH_CMDMGMT    dispatch;

    InitQueues();

    while (LW_TRUE) // Infinite loop
    {
        if (OS_QUEUE_WAIT_FOREVER(SoeCmdMgmtCmdDispQueue, &dispatch))
        {
            switch (dispatch.disp2unitEvt)
            {
                case DISP2UNIT_EVT_SIGNAL:
                    break;

                case DISP2UNIT_EVT_COMMAND:
                    processCommand(SOE_CMDMGMT_CMD_QUEUE_RM);
                    break;
            }
        }
    }
}
