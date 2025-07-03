/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */


/*!
 * @file   task1_mgmt.c
 * @brief  Represents and overlay task that is responsible for initializing
 *         all queues (command and message queues) as well as dispatching all
 *         unit tasks as commands arrive in the command queues.
 */

/* ------------------------ System includes -------------------------------- */
#include "dpusw.h"

/* ------------------------ Application includes --------------------------- */
#include "dpu_task.h"
#include "rmdpucmdif.h"
#include "dpu_mgmt.h"
#include "unit_dispatch.h"
#include "dpu_objdpu.h"
#include "dpu_objic.h"
#include "dpu_objlsf.h"
#ifndef EXCLUDE_LWOSDEBUG
#include "lwosdebug.h"
#endif //EXCLUDE_LWOSDEBUG

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */

/*!
 * Address defined in the linker script to mark the memory shared between DPU
 * and RM.  Specifically, this marks the starting offset for the chunk of DMEM
 * that is managed by the RM.
 */
extern LwU32       _end_rmqueue[];

/*!
 * Address defined in the linker script to mark the end of the DPU DMEM.  This
 * offset combined with @ref _end_rmqueue define the size of the RM-managed
 * portion of the DMEM.
 */
extern LwU32       _end_physical_dmem[];

/*!
 * Address defined in the linker script to mark the end of GSP-Lite EMEM
 */
extern LwU32       _end_emem[];

/* ------------------------ Global variables ------------------------------- */

/*!
 * Array containing the start offsets in DMEM of all command queues.
 */
LwU32              DpuMgmtCmdOffset[DPU_MGMT_CMD_QUEUE_NUM];

/*!
 * Array containing the start offsets in DMEM of all message queues.
 */
LwU32              DpuMgmtMsgOffset[DPU_MGMT_MSG_QUEUE_NUM];

/*!
 * A dispatch queue for sending commands to the command dispatcher task
 */
LwrtosQueueHandle  DpuMgmtCmdDispQueue;

/* ------------------------ Static variables ------------------------------- */

/*!
 * Creates a 2-d array representing each of the two DPU command queues.
 */
static LwU32 RM_DPU_CmdQueue[DPU_MGMT_CMD_QUEUE_NUM]
                            [PROFILE_CMD_QUEUE_LENGTH/sizeof(LwU32)]
    GCC_ATTRIB_SECTION("rmrtos", "RM_DPU_CmdQueue");

/*!
 * Creates a 2-d array representing each of the two DPU message queues.
 */
static LwU32 RM_DPU_MsgQueue[DPU_MGMT_MSG_QUEUE_NUM]
                            [PROFILE_MSG_QUEUE_LENGTH/sizeof(LwU32)]
    GCC_ATTRIB_SECTION("rmrtos", "RM_DPU_MsgQueue");

/*!
 * Keeps track of the locations within each command queue where the next
 * command will be dispatched from.
 */
static LwU32 DpuMgmtLastHandledCmdPtr[DPU_MGMT_CMD_QUEUE_NUM];

/* ------------------------ Local Function Prototyes ----------------------- */

static void cmdQueueDispatch(RM_DPU_COMMAND *pCmd, LwU8 queueId);

/* ------------------------ Defines ---------------------------------------- */

/*!
 * Responsible for initializing the head and tail pointers for the message
 * queues and command queues as well as collecting that information and sending
 * it to the RM as part of the INIT message.
 */
static void
InitQueues(void)
{
    RM_FLCN_QUEUE_HDR         hdr;
    RM_DPU_INIT_MSG_DPU_INIT  msg = {0};
    LwU32                     i;
    // physical to logical mapping for cmd queues
    LwU8 cmdIndexToId[] = {RM_DPU_CMDQ_LOG_ID};
    // physical to logical mapping for msg queues
    LwU8 msgIndexToId[] = {RM_DPU_MSGQ_LOG_ID};

    for (i = 0; i < DPU_MGMT_CMD_QUEUE_NUM; i++)
    {
        DpuMgmtCmdOffset[i] = LW_UPROC_VA_TO_DMEM_PA(&RM_DPU_CmdQueue[i][0]);
        DpuMgmtLastHandledCmdPtr[i] = DpuMgmtCmdOffset[i];

        dpuWriteCmdQueueTail_HAL(&Dpu.hal, i, DpuMgmtCmdOffset[i]);
        dpuWriteCmdQueueHead_HAL(&Dpu.hal, i, DpuMgmtCmdOffset[i]);

        msg.qInfo[i].queueSize   = PROFILE_CMD_QUEUE_LENGTH;
        msg.qInfo[i].queueOffset = (LwU32)DpuMgmtCmdOffset[i];
        msg.qInfo[i].queuePhyId  = (LwU8)i;
        msg.qInfo[i].queueLogId  = cmdIndexToId[i];
    }

    for (i = 0; i < DPU_MGMT_MSG_QUEUE_NUM; i++)
    {
        // setup the head and tail pointers of the message queue
        DpuMgmtMsgOffset[i] = LW_UPROC_VA_TO_DMEM_PA(&RM_DPU_MsgQueue[i][0]);
        dpuQueueRmInit_HAL(&Dpu, DpuMgmtMsgOffset[i]);

        // populate message queue data in the INIT message
        msg.qInfo[i + DPU_MGMT_CMD_QUEUE_NUM].queueSize   = PROFILE_MSG_QUEUE_LENGTH;
        msg.qInfo[i + DPU_MGMT_CMD_QUEUE_NUM].queueOffset = (LwU32)DpuMgmtMsgOffset[i];
        msg.qInfo[i + DPU_MGMT_CMD_QUEUE_NUM].queuePhyId  = (LwU8)i;
        msg.qInfo[i + DPU_MGMT_CMD_QUEUE_NUM].queueLogId  = msgIndexToId[i];
    }

    msg.numQueues = (LwU8)(DPU_MGMT_CMD_QUEUE_NUM + DPU_MGMT_MSG_QUEUE_NUM);

    // clear the entire command queue and message queue
    memset((void *)RM_DPU_CmdQueue, 0, sizeof(RM_DPU_CmdQueue));
    memset((void *)RM_DPU_MsgQueue, 0, sizeof(RM_DPU_MsgQueue));

    // Command queues are initialized, so we can unmask command intr now.
    icCmdQueueIntrUnmask_HAL(&IcHal);

#ifndef EXCLUDE_LWOSDEBUG
    // populate pointer to DPU debug information buffer
    msg.osDebugEntryPoint = (LwU16)LW_UPROC_VA_TO_DMEM_PA(&OsDebugEntryPoint);
#endif //EXCLUDE_LWOSDEBUG

    // fill-in the header of the INIT message and send it to the RM
    hdr.unitId    = RM_DPU_UNIT_INIT;
    hdr.ctrlFlags = RM_FLCN_QUEUE_HDR_FLAGS_EVENT;
    hdr.seqNumId  = 0;
    hdr.size      = RM_DPU_MSG_SIZE(INIT, DPU_INIT);
    msg.msgType   = RM_DPU_MSG_TYPE(INIT, DPU_INIT);

    {
        OsCmdmgmtQueueDescriptorRM.start =
            DpuMgmtMsgOffset[DPU_MGMT_MSG_QUEUE_RM];
        OsCmdmgmtQueueDescriptorRM.end =
            OsCmdmgmtQueueDescriptorRM.start + PROFILE_MSG_QUEUE_LENGTH;
        OsCmdmgmtQueueDescriptorRM.headGet  = dpuQueueRmHeadGet_HAL_FNPTR();
        OsCmdmgmtQueueDescriptorRM.headSet  = dpuQueueRmHeadSet_HAL_FNPTR();
        OsCmdmgmtQueueDescriptorRM.tailGet  = dpuQueueRmTailGet_HAL_FNPTR();
        OsCmdmgmtQueueDescriptorRM.dataPost = &osCmdmgmtQueueWrite_DMEM;
    }

    // Release the MSG queue mutex before sending INIT message to RM.
    osCmdmgmtRmQueueMutexReleaseAndPost(&hdr, &msg);
}

static void
processCommand(void)
{
    LwU32               head;
    LwU8                headId = DPU_MGMT_CMD_QUEUE_RM;
    RM_DPU_COMMAND     *pCmd;

    //
    // We only support one cmd queue today, and only one cmd queue head intr is
    // enabled, so we can only get here if we got a cmd in that queue. When we
    // add support for more queues, we will need to use INTRSTAT to find out
    // which cmd queue(s) had pending items.
    //

#ifdef GSPLITE_RTOS
    //
    // We don't need to clear INTRSTAT on DPU since it's not controllable by SW.
    // On DPU, INTRSTAT gets cleared automatically by HW when TAIL becomes equal
    // to HEAD.
    //
    icCmdQueueIntrClear_HAL(&IcHal);
#endif

    head = dpuReadCmdQueueHead_HAL(&Dpu.hal, headId);

    while (DpuMgmtLastHandledCmdPtr[headId] != head)
    {
        pCmd = (RM_DPU_COMMAND *)LW_DMEM_PA_TO_UPROC_VA(DpuMgmtLastHandledCmdPtr[headId]);

        if (pCmd->hdr.unitId == RM_DPU_UNIT_REWIND)
        {
            DpuMgmtLastHandledCmdPtr[headId] = DpuMgmtCmdOffset[headId];
            // Head might have changed !
            head = dpuReadCmdQueueHead_HAL(&Dpu.hal, headId);
            osCmdmgmtCmdQSweep(&pCmd->hdr, headId);
        }
        else
        {
            // must be done before dispatch
            DpuMgmtLastHandledCmdPtr[headId] +=
                ALIGN_UP(pCmd->hdr.size, sizeof(LwU32));
            cmdQueueDispatch(pCmd, headId);
        }
    }

#ifdef GSPLITE_RTOS
    //
    // Unmask the cmd queue interrupt as they were masked in ISR. For DPU, we
    // can't unmask the cmd queue interrupt here since DPU HW generates interrupt
    // automatically when HEAD != TAIL, so we will have interrupt storm if we
    // unmask the cmd queue interrupt at this point. Thus we can only unmask the
    // interrupt after the command is completely handled by its corresponding task.
    // However, for certain command (e.g. RM_DPU_UNIT_NULL or RM_DPU_UNIT_TIMER),
    // we handle it in cmdQueueDispatch directly without queuing event to another
    // task. That means after it is handled in cmdQueueDispatch, we will unmask
    // cmd queue interrupt in the osCmdmgmtCmdQSweep call which may cause a
    // redundant cmd queue intr if there is still some other command data queued
    // in command queue. But isr will mask cmd queue interrupt again, so it won't
    // cause any interrupt storm issue.
    //
    icCmdQueueIntrUnmask_HAL(&IcHal);
#endif
}

/*!
 * Defines the entry-point for this task.  This function will execute forever
 * processing each command that arrives in the message queues and dispatching
 * unit task to handle each command.
 */
lwrtosTASK_FUNCTION(task1_mgmt, pvParameters)
{
    DISPATCH_CMDMGMT dispatch;

    InitQueues();

    while (LW_TRUE)
    {
        if (OS_QUEUE_WAIT_FOREVER(DpuMgmtCmdDispQueue, &dispatch))
        {
            switch (dispatch.disp2unitEvt)
            {
                case DISP2UNIT_EVT_COMMAND:
                    processCommand();
                    break;
            }
        }
    }
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
    RM_DPU_COMMAND *pCmd,
    LwU8            queueId
)
{
    LwrtosQueueHandle   pQueueHandle = NULL;
    RM_FLCN_QUEUE_HDR   hdr;
    LwBool              bCmdDispatchNeeded = LW_TRUE;

    switch(pCmd->hdr.unitId)
    {
        case RM_DPU_UNIT_TEST:
            if (DPUCFG_FEATURE_ENABLED(DPUTASK_WKRTHD))
            {
                pQueueHandle = Disp2QWkrThd;
            }
            break;

        case RM_DPU_UNIT_HDCP:
            if (DPUCFG_FEATURE_ENABLED(DPUTASK_HDCP))
            {
                pQueueHandle = HdcpQueue;
            }
            break;

        case RM_DPU_UNIT_HDCP22WIRED:
            if (DPUCFG_FEATURE_ENABLED(DPUTASK_HDCP22WIRED))
            {
                pQueueHandle = Hdcp22WiredQueue;
            }
            break;

        case RM_DPU_UNIT_VRR:
            if (DPUCFG_FEATURE_ENABLED(DPUTASK_VRR))
            {
                pQueueHandle = VrrQueue;
            }
            break;

        case RM_DPU_UNIT_SCANOUTLOGGING:
            if (DPUCFG_FEATURE_ENABLED(DPUTASK_SCANOUTLOGGING))
            {
                pQueueHandle = ScanoutLoggingQueue;
            }
            break;

        case RM_DPU_UNIT_MSCGWITHFRL:
            if (DPUCFG_FEATURE_ENABLED(DPUTASK_MSCGWITHFRL))
            {
                pQueueHandle = MscgWithFrlQueue;
            }
            break;

        case RM_DPU_UNIT_TIMER:
            {
#if (DPUCFG_FEATURE_ENABLED(DPURTTIMER_FOR_OS_TICKS))
                DPU_HALT();
#else
                LwU32 freqHz = pCmd->cmd.timer.cmdUpdateFreq.freqKhz;

                // Check whether the integer is not overflowed
                if ((freqHz*1000) > freqHz)
                {
                    dpuGptmrSet_HAL(&Dpu.hal, freqHz*1000, LW_TRUE);
                }
                else
                {
                    // TODO: Remove this halt and propogate the error back to RM
                    DPU_HALT();
                }

                bCmdDispatchNeeded = LW_FALSE;
#endif
            }
            break;

        case RM_DPU_UNIT_NULL:
            {
                bCmdDispatchNeeded = LW_FALSE;
            }
            break;

        case RM_DPU_UNIT_UNLOAD:
            hdr.unitId    = RM_DPU_UNIT_UNLOAD;
            hdr.ctrlFlags = 0;
            hdr.seqNumId  = pCmd->hdr.seqNumId;
            hdr.size      = RM_FLCN_QUEUE_HDR_SIZE;

            osCmdmgmtCmdQSweep(&pCmd->hdr, queueId);
            //
            // We are unloading Falcon, so no matter osCmdmgmtRmQueuePostBlocking
            // succeeds or not, we will continue putting Falcon into a stable idle
            // state.
            //
            (void)osCmdmgmtRmQueuePostBlocking(&hdr, NULL);

            //
            // Make sure this doesn't get switched out with the
            // priv level masks down.
            //
            lwrtosENTER_CRITICAL();

            lsfLowerResetPrivLevelMask_HAL(&LsfHal);   

            icHaltIntrMask_HAL();
            DPU_HALT();

            lwrtosEXIT_CRITICAL();

            return;

        default:
            DBG_PRINTF_CMDMGMT(("UNIT %d not managed yet\n", pCmd->hdr.unitId, 0, 0, 0));
            break;
    }

    if (!bCmdDispatchNeeded)
    {
        osCmdmgmtCmdQSweep(&pCmd->hdr, queueId);

        hdr.unitId    = pCmd->hdr.unitId;
        hdr.ctrlFlags = 0;
        hdr.seqNumId  = pCmd->hdr.seqNumId;
        hdr.size      = RM_FLCN_QUEUE_HDR_SIZE;

        if (!osCmdmgmtRmQueuePostBlocking(&hdr, NULL))
        {
            DPU_HALT();
        }
    }

    // Dispatch command to respective queue.
    if (pQueueHandle != NULL)
    {
        DISP2UNIT_CMD  disp2unitMsg;

        disp2unitMsg.cmdQueueId = queueId;
        disp2unitMsg.pCmd       = pCmd;
        disp2unitMsg.eventType  = DISP2UNIT_EVT_COMMAND;
        // For the moment, we block in the Unit queue if full
        lwrtosQueueSendBlocking(pQueueHandle, &disp2unitMsg,
                                sizeof(disp2unitMsg));
    }

    // If the command has not been dispatched to any unit when necessary, panic.
    if ((bCmdDispatchNeeded) && (pQueueHandle == NULL))
    {
        DPU_HALT();
    }
}

