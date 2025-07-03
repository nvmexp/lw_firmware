/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
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
#include "sec2sw.h"
#include "lwosselwreovly.h"
/* ------------------------- Application Includes --------------------------- */
#include "sec2_cmdmgmt.h"
#include "sec2_chnmgmt.h"
#include "sec2_objsec2.h"
#include "sec2_objic.h"
#include "main.h"
#include "sec2_objacr.h"
#ifndef EXCLUDE_LWOSDEBUG
#include "lwosdebug.h"
#endif //EXCLUDE_LWOSDEBUG
#include "scp_crypt.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */

/*!
 * Address defined in the linker script to mark the memory shared between SEC2
 * and RM.  Specifically, this marks the starting offset for the chunk of DMEM
 * that is managed by the RM.
 */
extern LwU32    _end_rmqueue[];

/*!
 * Address defined in the linker script to mark the end of the SEC2 DMEM.  This
 * offset combined with @ref _end_rmqueue define the size of the RM-managed
 * portion of the DMEM.
 */
extern LwU32    _end_physical_dmem[];

/*!
 * Address defined in the linker script to mark the end of SEC2 EMEM
 */
extern LwU32    _end_emem[];

/*!
 * DEVID encryption specific variables initialized in INIT_HS overlay
 * that needs to be copied to RM
 */
#if (SEC2CFG_FEATURE_ENABLED(SEC2_ENCRYPT_DEVID))
extern LwU8 g_devIdEnc[SCP_AES_SIZE_IN_BYTES];
extern LwU8 g_devIdDer[SCP_AES_SIZE_IN_BYTES];
#endif

#define SEC2_FALCON_SCTL1_EXTLVL3_VAL       (0x3)
#define SEC2_FALCON_SCTL1_CSBLVL3_VAL       (0x3)

/* ------------------------- Global Variables ------------------------------- */
/*!
 * Array containing the start offsets in DMEM of all command queues.
 */
LwU32              Sec2MgmtCmdOffset[SEC2_CMDMGMT_CMD_QUEUE_NUM];

/*!
 * Array containing the start offsets in DMEM of all message queues.
 */
LwU32              Sec2MgmtMsgOffset[SEC2_CMDMGMT_CMD_QUEUE_NUM];

/*!
 * A dispatch queue for sending commands to the command dispatcher task
 */
LwrtosQueueHandle  Sec2CmdMgmtCmdDispQueue;

/* ------------------------- Static Variables ------------------------------- */

/*!
 * Represents the SEC2 command queue(s). For now, we only have one such.
 */
static LwU32 RM_SEC2_CmdQueue[SEC2_CMDMGMT_CMD_QUEUE_NUM][PROFILE_CMD_QUEUE_LENGTH/sizeof(LwU32)]
    GCC_ATTRIB_USED()
    GCC_ATTRIB_SECTION("rmrtos", "RM_SEC2_CmdQueue");

/*!
 * Represents the SEC2 message queue(s). For now, we only have one such.
 */
static LwU32 RM_SEC2_MsgQueue[SEC2_CMDMGMT_MSG_QUEUE_NUM][PROFILE_MSG_QUEUE_LENGTH/sizeof(LwU32)]
    GCC_ATTRIB_USED()
    GCC_ATTRIB_SECTION("rmrtos", "RM_SEC2_MsgQueue");

/*!
 * Keeps track of the locations within each command queue where the next
 * command will be dispatched from.
 */
static LwU32 Sec2MgmtLastHandledCmdPtr[SEC2_CMDMGMT_MSG_QUEUE_NUM];

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
    RM_SEC2_INIT_MSG_SEC2_INIT  msg;
    LwU32                       i;
    // physical to logical mapping for cmd queues
    LwU8 cmdIndexToId[] = {SEC2_RM_CMDQ_LOG_ID};
    // physical to logical mapping for msg queues
    LwU8 msgIndexToId[] = {SEC2_RM_MSGQ_LOG_ID};

    //
    // Iterate over all command queues setting up the head and tail pointers
    // for each queue.  At the same time, start building up the INIT message.
    //
    for (i = SEC2_CMDMGMT_CMD_QUEUE_START; i < SEC2_CMDMGMT_CMD_QUEUE_NUM; i++)
    {
        Sec2MgmtCmdOffset[i] = (LwU32)(RM_SEC2_CmdQueue) +
                               (i * PROFILE_CMD_QUEUE_LENGTH/sizeof(LwU32));
        Sec2MgmtLastHandledCmdPtr[i] = Sec2MgmtCmdOffset[i];

        sec2WriteCmdQueueTail_HAL(&Sec2, i, Sec2MgmtCmdOffset[i]);
        sec2WriteCmdQueueHead_HAL(&Sec2, i, Sec2MgmtCmdOffset[i]);

        msg.qInfo[i].queueSize   = PROFILE_CMD_QUEUE_LENGTH;
        msg.qInfo[i].queueOffset = (LwU32)Sec2MgmtCmdOffset[i];
        msg.qInfo[i].queuePhyId  = (LwU8)i;
        msg.qInfo[i].queueLogId  = cmdIndexToId[i];
    }

    msg.numQueues = (LwU8)(SEC2_CMDMGMT_CMD_QUEUE_NUM + SEC2_CMDMGMT_MSG_QUEUE_NUM);

    // clear the queues
    memset((void *)(RM_SEC2_CmdQueue)   , 0, sizeof(RM_SEC2_CmdQueue));
    memset((void *)(RM_SEC2_MsgQueue)   , 0, sizeof(RM_SEC2_MsgQueue));

    //
    // Command queues are initialized, so we can unmask command intr now.
    // Note that the first write to the queue head to initialize the command
    // queue will raise a command queue interrupt. HW raises an interrupt
    // whenever a write to head oclwrs - it does not check if head and tail are
    // the same before deciding to raise an interrupt. However, that is okay
    // since we will just quietly return, seeing empty queue(s).
    //
    icCmdQueueIntrUnmask_HAL(&Ic, SEC2_CMDMGMT_CMD_QUEUE_RM);
    if (SEC2CFG_FEATURE_ENABLED(SEC2_GPC_RG_SUPPORT))
    {
        icCmdQueueIntrUnmask_HAL(&Ic, SEC2_CMDMGMT_CMD_QUEUE_PMU);
    }

    // We have only one message queue
    i = SEC2_CMDMGMT_MSG_QUEUE_START;

    // setup the head and tail pointers of the message queue
    Sec2MgmtMsgOffset[i] = (LwU32)(RM_SEC2_MsgQueue) +
                           (i * PROFILE_MSG_QUEUE_LENGTH/sizeof(LwU32));
    sec2QueueRmInit_HAL(&Sec2, Sec2MgmtMsgOffset[i]);

    // populate message queue data in the INIT message
    msg.qInfo[i + SEC2_CMDMGMT_CMD_QUEUE_NUM].queueSize   = PROFILE_MSG_QUEUE_LENGTH;
    msg.qInfo[i + SEC2_CMDMGMT_CMD_QUEUE_NUM].queueOffset = (LwU32)Sec2MgmtMsgOffset[i];
    msg.qInfo[i + SEC2_CMDMGMT_CMD_QUEUE_NUM].queuePhyId  = i;
    msg.qInfo[i + SEC2_CMDMGMT_CMD_QUEUE_NUM].queueLogId  = msgIndexToId[i];

#ifndef EXCLUDE_LWOSDEBUG
    // populate pointer to SEC2 debug information buffer
    msg.osDebugEntryPoint = (LwU16)(LwU32)(&OsDebugEntryPoint);
#endif //EXCLUDE_LWOSDEBUG

#if (SEC2CFG_FEATURE_ENABLED(SEC2_ENCRYPT_DEVID))
    // Populate devid details
    memcpy(msg.devidEnc, g_devIdEnc, SCP_AES_SIZE_IN_BYTES);
    memcpy(msg.devidDerivedKey, g_devIdDer, SCP_AES_SIZE_IN_BYTES);
#endif

    // fill-in the header of the INIT message and send it to the RM
    hdr.unitId    = RM_SEC2_UNIT_INIT;
    hdr.ctrlFlags = RM_FLCN_QUEUE_HDR_FLAGS_EVENT;
    hdr.seqNumId  = 0;
    hdr.size      = RM_SEC2_MSG_SIZE(INIT, SEC2_INIT);
    msg.msgType   = RM_SEC2_MSG_TYPE(INIT, SEC2_INIT);

    {
        OsCmdmgmtQueueDescriptorRM.start =
            Sec2MgmtMsgOffset[i];
        OsCmdmgmtQueueDescriptorRM.end =
            OsCmdmgmtQueueDescriptorRM.start + PROFILE_MSG_QUEUE_LENGTH;
        OsCmdmgmtQueueDescriptorRM.headGet  = sec2QueueRmHeadGet_HAL_FNPTR();
        OsCmdmgmtQueueDescriptorRM.headSet  = sec2QueueRmHeadSet_HAL_FNPTR();
        OsCmdmgmtQueueDescriptorRM.tailGet  = sec2QueueRmTailGet_HAL_FNPTR();
        OsCmdmgmtQueueDescriptorRM.dataPost = &osCmdmgmtQueueWrite_DMEM;
    }

    // Release the MSG queue mutex before sending INIT message to RM.
    osCmdmgmtRmQueueMutexReleaseAndPost(&hdr, &msg);
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
    RM_FLCN_CMD_SEC2 *pCmd,
    LwU8             queueId
)
{
    DISP2UNIT_CMD       disp2unitEvt;
    RM_FLCN_QUEUE_HDR   hdr;
    LwrtosQueueHandle   pQueue = NULL;
    LwU8 privLevelExtDefault  = 0;
    LwU8 privLevelCsbDefault  = 0;

    disp2unitEvt.cmdQueueId = queueId;
    disp2unitEvt.pCmd       = pCmd;
    disp2unitEvt.eventType  = DISP2UNIT_EVT_COMMAND;

    switch(pCmd->hdr.unitId)
    {
        case RM_SEC2_UNIT_TEST:
#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_WKRTHD))
            pQueue = Disp2QWkrThd;
#endif
            break;

        case RM_SEC2_UNIT_VPR:
#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_VPR))
                pQueue = VprQueue;
#endif
            break;

        // UNIT_NULL simply replies with the same cmd
        case RM_SEC2_UNIT_NULL:
            osCmdmgmtCmdQSweep(&pCmd->hdr, queueId);
            hdr.unitId      = pCmd->hdr.unitId;
            hdr.ctrlFlags   = 0;
            hdr.seqNumId    = pCmd->hdr.seqNumId;
            hdr.size        = RM_FLCN_QUEUE_HDR_SIZE;
            osCmdmgmtRmQueuePostBlocking(&hdr, NULL);

            return;

        case RM_SEC2_UNIT_CHNMGMT:
            disp2unitEvt.eventType = DISP2CHNMGMT_EVT_CMD;
            pQueue = Sec2ChnMgmtCmdDispQueue;
            break;

        case RM_SEC2_UNIT_UNLOAD:
            hdr.unitId    = RM_SEC2_UNIT_UNLOAD;
            hdr.ctrlFlags = 0;
            hdr.seqNumId  = pCmd->hdr.seqNumId;
            hdr.size      = RM_FLCN_QUEUE_HDR_SIZE;

#if 0 // TODO: Uncomment the below call when PMU ACR task is disabled.
            if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_ACR))
            {
                // Reset the CBC_BASE register since the SEC2 is being detached.

                acrResetCbcBase_HAL(&Acr);
            }
#endif

#if (SEC2CFG_FEATURE_ENABLED(SEC2_INIT_HS))
            OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(cleanupHs));
#endif
            osCmdmgmtCmdQSweep(&pCmd->hdr, queueId);
            osCmdmgmtRmQueuePostBlocking(&hdr, NULL);

            // Get the current value of the Priv levels for EXT and CSB
            sec2GetFlcnPrivLevel_HAL(&Sec2, &privLevelExtDefault, &privLevelCsbDefault);

            //
            // Make sure this doesn't get switched out with the
            // priv level masks down.
            //
            lwrtosENTER_CRITICAL();
            {
#if (SEC2CFG_FEATURE_ENABLED(SEC2_INIT_HS))

                //
                // Temporarily raise the Priv Levels so that the new Source mask can be written in
                // LW_PPRIV_SYS_PRIV_ERROR_PRIV_LEVEL_MASK register by the sec2UpdatePrivErrorPlm_GA10X
                // function (present in cleanupHs.)
                //
                sec2FlcnPrivLevelSet_HAL(&Sec2, SEC2_FALCON_SCTL1_EXTLVL3_VAL, SEC2_FALCON_SCTL1_CSBLVL3_VAL);

                // Setup entry into HS mode
                OS_HS_MODE_SETUP_IN_TASK(OVL_INDEX_IMEM(cleanupHs), NULL, 0, HASH_SAVING_DISABLE);

                // Do any HS cleanup
                sec2CleanupHs_HAL();

                // Cleanup after returning from HS mode
                OS_HS_MODE_CLEANUP_IN_TASK(NULL, 0);
                OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(cleanupHs));

                // Restore back the original falcon priv levels for EXT and CSB
                sec2FlcnPrivLevelSet_HAL(&Sec2, privLevelExtDefault, privLevelCsbDefault);
#endif

                sec2LowerPrivLevelMasks_HAL();

#if (SEC2CFG_FEATURE_ENABLED(BUG_200272812_HALT_INTR_MASK_WAR))
                //
                // Put falcon in wait instead of halt because of Bug 200272812
                // We are waiting inside critical section so this would never return
                //
                falc_wait(0);
#else
                // Mask the HALT interrupt before halting
                icHaltIntrMask_HAL();
                SEC2_HALT();
#endif
            }
            lwrtosEXIT_CRITICAL();
            return;

        case RM_SEC2_UNIT_ACR:
            if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_ACR))
            {
                pQueue = AcrQueue;
            }
            break;

        case RM_SEC2_UNIT_SPDM:
#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_SPDM))
                pQueue = SpdmQueue;
#endif
            break;

        default:
            // Catch the case where we don't manage the UNIT ID
            DBG_PRINTF_CMDMGMT(("UNIT %d not managed by SEC2\n", pCmd->hdr.unitId, 0, 0, 0));
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
    RM_FLCN_CMD_SEC2  *pCmd;

    DBG_PRINTF_CMDMGMT(("Task CmdMng Got cmd\n", 0, 0, 0, 0));

    //
    // We only support one cmd queue today, and only one cmd queue head intr is
    // enabled, so we can only get here if we got a cmd in that queue. When we
    // add support for more queues, we will need to use INTRSTAT to find out
    // which cmd queue(s) had pending items.
    //
    icCmdQueueIntrClear_HAL(&Ic, queueId);
    head = sec2ReadCmdQueueHead_HAL(&Sec2, queueId);

    while (Sec2MgmtLastHandledCmdPtr[queueId] != head)
    {
        pCmd = (RM_FLCN_CMD_SEC2 *)Sec2MgmtLastHandledCmdPtr[queueId];

        DBG_PRINTF_CMDMGMT(("SEC2 Rec sec2_cmd 0x%x , head 0x%x\n",
            (LwU32)pCmd, head,0,0));

        DBG_PRINTF_CMDMGMT(("SEC2 UnitId %d size %d sq %d ctl %d\n",
                            pCmd->hdr.unitId,
                            pCmd->hdr.size,
                            pCmd->hdr.seqNumId,
                            pCmd->hdr.ctrlFlags));

        if (pCmd->hdr.unitId == RM_SEC2_UNIT_REWIND)
        {
            Sec2MgmtLastHandledCmdPtr[queueId] = Sec2MgmtCmdOffset[queueId];
            DBG_PRINTF_CMDMGMT(("SEC2 Rec Rewind\n",0,0,0,0));
            // Head might have changed!
            head = sec2ReadCmdQueueHead_HAL(&Sec2, queueId);
            osCmdmgmtCmdQSweep(&pCmd->hdr, queueId);
        }
        else
        {
            // must be done before dispatch
            Sec2MgmtLastHandledCmdPtr[queueId] +=
                ALIGN_UP(pCmd->hdr.size, sizeof(LwU32));
            cmdQueueDispatch(pCmd, queueId);
        }
    }

    // Unmask the cmd queue interrupts (they were masked in ISR)
    icCmdQueueIntrUnmask_HAL(&Ic, queueId);

    DBG_PRINTF_CMDMGMT(("SEC2 Done with Q%d\n", queueId,0,0,0));
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
        if (OS_QUEUE_WAIT_FOREVER(Sec2CmdMgmtCmdDispQueue, &dispatch))
        {
            switch (dispatch.disp2unitEvt)
            {
                case DISP2UNIT_EVT_SIGNAL:
                    break;

                case DISP2UNIT_EVT_COMMAND:
                    processCommand(SEC2_CMDMGMT_CMD_QUEUE_RM);
                    break;
            }
        }
    }
}
