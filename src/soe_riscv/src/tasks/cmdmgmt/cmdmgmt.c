/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   cmdmgmt.c
 * @brief  Command management task that is responsible for initializing all
 *         queues (command) as well as dispatching all
 *         unit tasks as commands arrive in the command queues.
 */


/* ------------------------ LW Includes ------------------------------------ */
#include <lwtypes.h>
#include <lwuproc.h>
#include <rmsoecmdif.h>

/* ------------------------ Register Includes ------------------------------ */
#include <engine.h>

/* ------------------------ SafeRTOS Includes ------------------------------ */
#include <lwrtos.h>
#include <sections.h>

/* ------------------------ RISC-V system library  -------------------------- */
#include <lwriscv/print.h>
#include <syslib/syslib.h>
#include <shlib/syscall.h>

/* ------------------------ Module Includes -------------------------------- */
#include "config/g_sections_riscv.h"
#include "config/g_ic_hal.h"
#include "config/g_soe_hal.h"
#include "unit_dispatch.h"
#include "tasks.h"
#include "cmdmgmt.h"
#include "chnlmgmt.h"
#include "lwoscmdmgmt.h"
#include "dev_soe_csb.h"
#include "dev_soe_ip.h"

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
/* ------------------------- Application Includes --------------------------- */


#ifndef EXCLUDE_LWOSDEBUG
#include "lwosdebug.h"
#endif //EXCLUDE_LWOSDEBUG
#include "scp_crypt.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */

extern void *mapRmOffsetToPtr(LwU32 offset) GCC_ATTRIB_SECTION("globalCode", "mapRmOffsetToPtr");
extern LwU32 mapPtrToRmOffset(void *pEmemPtr) GCC_ATTRIB_SECTION("globalCode", "mapPtrToRmOffset");

/* ------------------------- Global Variables ------------------------------- */

// Array containing the start offsets in DMEM of all command queues.
sysTASK_DATA LwU32 SoeMgmtCmdOffset[SOE_CMDMGMT_CMD_QUEUE_NUM];

// Semaphore handle
sysTASK_DATA LwrtosSemaphoreHandle cmdqSemaphore;

// Shared copy of offset (used by msg task)
sysTASK_DATA LwU32 cmdqOffsetShared; 

/* ------------------------- Static Variables ------------------------------- */

// Represents the SOE command queue(s). For now, we only have one such.
static LwU32 cmdQueue[SOE_CMDMGMT_CMD_QUEUE_NUM][PROFILE_CMD_QUEUE_LENGTH/sizeof(LwU32)]
    GCC_ATTRIB_USED()
    GCC_ATTRIB_SECTION("rm_rtos_queues", "cmdQueue");

/*!
 * Keeps track of the locations within each command queue where the next
 * command will be dispatched from.
 */
static LwU32 SoeMgmtLastHandledCmdPtr[SOE_CMDMGMT_CMD_QUEUE_NUM];

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define tprintf(level, ...)                dbgPrintf(level, "cmd: " __VA_ARGS__)

/*!
 * This function is to be called by unit thread when the unit is finished with
 * the command it is processing. This allows the queue manager to release the
 * space in the command queue that the command oclwpies.
 *
 * @param[in]   pHdr        A pointer to the acknowledged command's header.
 * @param[in]   queueId     The identifier of the queue containing command.
 */

GCC_ATTRIB_SECTION("globalCode", "cmdQueueSweep")
void
cmdQueueSweep
(
    PRM_FLCN_QUEUE_HDR pHdr,
    LwU8 queueId
)
{
    LwU32 tail;
    LwU32 head;

    lwrtosSemaphoreTakeWaitForever(cmdqSemaphore);

    tail = csbRead(ENGINE_REG(_QUEUE_TAIL((LwU32)queueId)));
    head = csbRead(ENGINE_REG(_QUEUE_HEAD((LwU32)queueId)));

    pHdr->ctrlFlags |= RM_FLCN_QUEUE_HDR_FLAGS_UNIT_ACK;

    // If command is the last one, free as much as possible in the queue.
    if (pHdr == (PRM_FLCN_QUEUE_HDR)mapRmOffsetToPtr(tail))
    {
        while (tail != head)
        {
            pHdr = (PRM_FLCN_QUEUE_HDR)mapRmOffsetToPtr(tail);

            if (pHdr != NULL)
            {
                // If the command (REWIND included) is not yet ACKed, bail out.
                if ((pHdr->ctrlFlags & RM_FLCN_QUEUE_HDR_FLAGS_UNIT_ACK) == 0)
                {
                    break;
                }

                if (pHdr->unitId == RM_FLCN_UNIT_ID_REWIND)
                {
                    tail = SoeMgmtCmdOffset[queueId];
                }
                else
                {
                // Round up size to a multiple of 4.
                tail += LW_ALIGN_UP(pHdr->size, sizeof(LwU32));
                }

                csbWrite(ENGINE_REG(_QUEUE_TAIL((LwU32)queueId)), tail);
            }
            else
            {
                tprintf(LEVEL_ERROR,"Dereferencing a pointer pHdr which is NULL");
            }
        }
    }

    lwrtosSemaphoreGive(cmdqSemaphore);
}


/*!
 * Responsible for initializing the head and tail pointers for the message
 * queue and all command queues as well as collecting that information and
 * sending it to the RM as part of the INIT message.
 */
static void
InitQueues(void)
{
    LwU32 i      = 0;
    LwU32 index  = 0;

    //
    // Iterate over all command queues setting up the head and tail pointers
    // for each queue.
    //
    for (i = SOE_CMDMGMT_CMD_QUEUE_START; i < SOE_CMDMGMT_CMD_QUEUE_NUM; i++)
    {
        index = i * (PROFILE_CMD_QUEUE_LENGTH/sizeof(LwU32));
        SoeMgmtCmdOffset[i] = mapPtrToRmOffset(&cmdQueue[index]);
        SoeMgmtLastHandledCmdPtr[i] = SoeMgmtCmdOffset[i];
        cmdqOffsetShared = SoeMgmtCmdOffset[i];

        csbWrite(ENGINE_REG(_QUEUE_TAIL(i)), SoeMgmtCmdOffset[i]);
        csbWrite(ENGINE_REG(_QUEUE_HEAD(i)), SoeMgmtCmdOffset[i]);
    }

    // clear the queues
    memset((void *)(cmdQueue), 0, sizeof(cmdQueue));

    //
    // Command queues are initialized, so we can unmask command intr now.
    // Note that the first write to the queue head to initialize the command
    // queue will raise a command queue interrupt. HW raises an interrupt
    // whenever a write to head oclwrs - it does not check if head and tail are
    // the same before deciding to raise an interrupt. However, that is okay
    // since we will just quietly return, seeing empty queue(s).
    //
    icCmdQueueIntrUnmask_HAL(&Ic, SOE_CMDMGMT_CMD_QUEUE_RM);
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
    FLCN_STATUS         retVal               = FLCN_OK;
    LwrtosQueueHandle   pQueue               = NULL;
    DISP2UNIT_CMD       disp2unitEvt;
    RM_FLCN_QUEUE_HDR   hdr;
    RM_FLCN_MSG_SOE    soeMsg;

    disp2unitEvt.cmdQueueId = queueId;
    disp2unitEvt.pCmd       = pCmd;
    disp2unitEvt.eventType  = DISP2UNIT_EVT_COMMAND;

    switch(pCmd->hdr.unitId)
    {
        // UNIT_NULL simply replies with the same cmd
        case RM_SOE_UNIT_NULL:
            cmdQueueSweep(&pCmd->hdr, queueId);
            hdr.unitId      = pCmd->hdr.unitId;
            hdr.ctrlFlags   = 0;
            hdr.seqNumId    = pCmd->hdr.seqNumId;
            hdr.size        = RM_FLCN_QUEUE_HDR_SIZE;

            // Send the ACK message
            soeMsg.hdr = hdr;
            retVal = lwrtosQueueSendBlocking(rmMsgRequestQueue, &soeMsg, sizeof(soeMsg));
            if (retVal != FLCN_OK)
            {
                tprintf(LEVEL_ERROR,
                        "Failed sending message to queue %p unit 0x%x: 0x%x\n",
                        rmMsgRequestQueue, pCmd->hdr.unitId, retVal);
            }

            return;

        case RM_SOE_UNIT_THERM:
            pQueue = Disp2QThermThd;
            break;

#if DO_TASK_RM_SPI
        case RM_SOE_UNIT_SPI:
            pQueue = Disp2QSpiThd;
            break;
#endif // DO_TASK_RM_SPI

#if DO_TASK_RM_CORE
        case RM_SOE_UNIT_CORE:
            cmdQueueSweep(&pCmd->hdr, queueId);
            pQueue = Disp2QCoreThd;
            break;
#endif // DO_TASK_RM_CORE

#if DO_TASK_RM_BIF
        case RM_SOE_UNIT_BIF:
            pQueue = Disp2QBifThd;
            break;
#endif // DO_TASK_RM_BIF

        case RM_SOE_UNIT_UNLOAD:
            hdr.unitId    = RM_SOE_UNIT_UNLOAD;
            hdr.ctrlFlags = 0;
            hdr.seqNumId  = pCmd->hdr.seqNumId;
            hdr.size      = RM_FLCN_QUEUE_HDR_SIZE;

            cmdQueueSweep(&pCmd->hdr, queueId);

            // Send the ACK message
            soeMsg.hdr = hdr;
            if (!msgQueuePostBlocking(&hdr, &soeMsg))
            {
                dbgPrintf(LEVEL_ALWAYS, "UNLOAD response not sent\n");
            }

            //
            // Make sure this doesn't get switched out with the
            // priv level masks down.
            //
            lwrtosENTER_CRITICAL();
            {
                soeLowerPrivLevelMasks_HAL();
                //
                // TODO @vsurana : Check if this needs to be replaced
                // or kept as it is
                //
                sysShutdown();
            }
            lwrtosEXIT_CRITICAL();
            return;

        default:
            // Catch the case where we don't manage the UNIT ID
            tprintf(LEVEL_CRIT, "UNIT %d not managed yet\n", pCmd->hdr.unitId);
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

    //
    // We only support one cmd queue today, and only one cmd queue head intr is
    // enabled, so we can only get here if we got a cmd in that queue. When we
    // add support for more queues, we will need to use INTRSTAT to find out
    // which cmd queue(s) had pending items.
    //
    icCmdQueueIntrClear_HAL(&Ic, queueId);
    head = csbRead(ENGINE_REG(_QUEUE_HEAD(queueId)));

    while (SoeMgmtLastHandledCmdPtr[queueId] != head)
    {
        pCmd = (RM_FLCN_CMD_SOE *)mapRmOffsetToPtr(SoeMgmtLastHandledCmdPtr[queueId]);

        if (pCmd != NULL)
        {
            if (pCmd->hdr.unitId == RM_SOE_UNIT_REWIND)
            {
                SoeMgmtLastHandledCmdPtr[queueId] = SoeMgmtCmdOffset[queueId];
                // Head might have changed!
                head = csbRead(ENGINE_REG(_QUEUE_HEAD(queueId)));
                cmdQueueSweep(&pCmd->hdr, queueId);
            }
            else
            {
                // must be done before dispatch
                SoeMgmtLastHandledCmdPtr[queueId] += ALIGN_UP(pCmd->hdr.size, sizeof(LwU32));
                cmdQueueDispatch(pCmd, queueId);
            }
        }
        else
        {
            tprintf(LEVEL_ERROR,"Dereferencing a pointer pCmd which is NULL");
        }
    }

    // Unmask the cmd queue interrupts (they were masked in ISR)
    icCmdQueueIntrUnmask_HAL(&Ic, queueId);
}

/*!
 * Defines the entry-point for this task.  This function will execute forever
 * processing each command that arrives in the message queues and dispatching
 * unit task to handle each command.
 */
lwrtosTASK_FUNCTION(rmCmdMain, pvParameters)
{
    dbgPrintf(LEVEL_ALWAYS, "We are now inside the Command Management task\n");
    FLCN_STATUS retVal;
    LwrtosTaskHandle task_msg_mgmt = (LwrtosTaskHandle)pvParameters;

    retVal = lwrtosSemaphoreCreateBinaryTaken(&cmdqSemaphore, 0);
    if (retVal != FLCN_OK)
    {
        sysTaskExit(dbgStr(LEVEL_CRIT, "Failed to create cmdmgmt semaphore."), retVal);
    }

    InitQueues();
    retVal = lwrtosSemaphoreGive(cmdqSemaphore);
    if (retVal != FLCN_OK)
    {
        sysTaskExit(dbgStr(LEVEL_CRIT, "Failed to create cmdmgmt semaphore."), retVal);
    }

    sysCmdqRegisterNotifier(SOE_CMDMGMT_CMD_QUEUE_RM);
    sysCmdqEnableNotifier(SOE_CMDMGMT_CMD_QUEUE_RM);

    // Notify msg mgmt task that we're done.
    lwrtosTaskNotifySend(task_msg_mgmt, lwrtosTaskNOTIFICATION_NO_ACTION, 0);

    tprintf(LEVEL_INFO, "started command manager.\n");
    while (LW_TRUE)
    {
        FLCN_STATUS retVal = -1;

        retVal = lwrtosTaskNotifyWait(0, BIT(SOE_CMDMGMT_CMD_QUEUE_RM), NULL, lwrtosMAX_DELAY); // We get queue bit set
        if (retVal == FLCN_OK)
        {
            tprintf(LEVEL_TRACE,
                    "Received RM message on queue head %x tail %x.\n",
                    csbRead(ENGINE_REG(_QUEUE_HEAD(SOE_CMDMGMT_CMD_QUEUE_RM))),
                    csbRead(ENGINE_REG(_QUEUE_TAIL(SOE_CMDMGMT_CMD_QUEUE_RM))));

            // vsurana TODO: we receive suprious interrupt, because we write head/tail
            if (csbRead(ENGINE_REG(_QUEUE_HEAD(SOE_CMDMGMT_CMD_QUEUE_RM))) == csbRead(ENGINE_REG(_QUEUE_TAIL(SOE_CMDMGMT_CMD_QUEUE_RM))))
            {
                sysCmdqEnableNotifier(SOE_CMDMGMT_CMD_QUEUE_RM);
                continue;
            }

            // Process the incoming command
            processCommand(SOE_CMDMGMT_CMD_QUEUE_RM);
            sysCmdqEnableNotifier(SOE_CMDMGMT_CMD_QUEUE_RM);
        }
        else
        {
            sysTaskExit(dbgStr(LEVEL_CRIT, "cmdmgmt: Faild waiting on RM queue. No RM communication is possible."), retVal);
        }
    }
}
