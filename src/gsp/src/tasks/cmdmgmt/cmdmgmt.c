/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */


/*!
 * @file   mgmt.c
 * @brief  Represents and overlay task that is responsible for initializing
 *         all queues (command and message queues) as well as dispatching all
 *         unit tasks as commands arrive in the command queues.
 *
 * This is copy, but I tried to keep as much of old code as possible.
 *
 * The difference here is message queue is also managed by this task.
 */

/* ------------------------ System Includes -------------------------------- */

/* ------------------------ LW Includes ------------------------------------ */
#include <lwtypes.h>
#include <lwuproc.h>
#include <rmgspcmdif.h>

/* ------------------------ Register Includes ------------------------------ */
#include <engine.h>

/* ------------------------ SafeRTOS Includes ------------------------------ */
#include <lwrtos.h>
#include <sections.h>

/* ------------------------ RISC-V system library  -------------------------- */
#include <lwriscv/print.h>
#include <syslib/syslib.h>

/* ------------------------ Module Includes -------------------------------- */
#include "config/g_sections_riscv.h"
#include "unit_dispatch.h"
#include "tasks.h"
#include "cmdmgmt.h"

#define tprintf(level, ...)                dbgPrintf(level, "cmd: " __VA_ARGS__)
/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ Global variables ------------------------------- */
/* ------------------------ Static variables ------------------------------- */

// Shared copy of offset (used by msg task)
sysTASK_DATA LwU32 cmdqOffsetShared;
sysTASK_DATA LwrtosSemaphoreHandle cmdqSemaphore;

// RM Command queue (inside EMEM)
static LwU32 cmdQueue[PROFILE_CMD_QUEUE_LENGTH/sizeof(LwU32)]
    GCC_ATTRIB_SECTION("rm_rtos_queues", "cmdQueue");

// RM-Compatible of next empty spot in command queue
static LwU32 cmdqLastHandledCmdOffset;

/* ------------------------ RM queue handling ------------------------------ */

// Map RM-compatible offset to valid EMEM pointer
GCC_ATTRIB_SECTION("globalCodeFb", "mapRmOffsetToPtr")
void *mapRmOffsetToPtr(LwU32 offset)
{
    if ((offset < RM_DMEM_EMEM_OFFSET) ||
        ((offset - RM_DMEM_EMEM_OFFSET) >= UPROC_SECTION_MAX_SIZE(rmQueue)))
    {
        dbgPrintf(LEVEL_ERROR, "%s: Invalid queue offset: %x.\n", __FUNCTION__, offset);
        return NULL;
    }

    offset -= RM_DMEM_EMEM_OFFSET;

    return (void*)(UPROC_SECTION_VIRT_OFFSET(rmQueue) + offset);
}

// Map EMEM pointer to RM-compatible offset
GCC_ATTRIB_SECTION("globalCodeFb", "mapPtrToRmOffset")
LwU32 mapPtrToRmOffset(void *pEmemPtr)
{
    LwU64 offset = 0;

    if ((LwUPtr)pEmemPtr < UPROC_SECTION_VIRT_OFFSET(rmQueue))
    {
        dbgPrintf(LEVEL_ERROR, "%s: Invalid queue pointer: %p.\n", __FUNCTION__, pEmemPtr);
        return 0;
    }

    offset = (LwUPtr)pEmemPtr - UPROC_SECTION_VIRT_OFFSET(rmQueue);

    if (offset >= UPROC_SECTION_MAX_SIZE(rmQueue))
    {
        dbgPrintf(LEVEL_ERROR, "%s: Invalid queue pointer: %p.\n", __FUNCTION__, pEmemPtr);
        return 0;
    }

    return offset + RM_DMEM_EMEM_OFFSET;
}

GCC_ATTRIB_SECTION("globalCodeFb", "cmdQueueSweep")
void
cmdQueueSweep(PRM_FLCN_QUEUE_HDR pHdr)
{
    LwU32 tail;
    LwU32 head;

    lwrtosSemaphoreTakeWaitForever(cmdqSemaphore);

    tail = csbRead(ENGINE_REG(_QUEUE_TAIL(GSP_CMD_QUEUE_RM)));
    head = csbRead(ENGINE_REG(_QUEUE_HEAD(GSP_CMD_QUEUE_RM)));

    pHdr->ctrlFlags |= RM_FLCN_QUEUE_HDR_FLAGS_UNIT_ACK;

    // If command is the last one, free as much as possible in the queue.
    if (pHdr == (PRM_FLCN_QUEUE_HDR)mapRmOffsetToPtr(tail))
    {
        while (tail != head)
        {
            pHdr = (PRM_FLCN_QUEUE_HDR)mapRmOffsetToPtr(tail);

            // If the command (REWIND included) is not yet ACKed, bail out.
            if ((pHdr->ctrlFlags & RM_FLCN_QUEUE_HDR_FLAGS_UNIT_ACK) == 0)
            {
                break;
            }

            if (pHdr->unitId == RM_FLCN_UNIT_ID_REWIND)
            {
                tail = cmdqOffsetShared;
            }
            else
            {
                // Round up size to a multiple of 4.
                tail += LW_ALIGN_UP(pHdr->size, sizeof(LwU32));
            }

            csbWrite(ENGINE_REG(_QUEUE_TAIL(GSP_CMD_QUEUE_RM)), tail);
            SYS_FLUSH_IO();
        }
    }

    lwrtosSemaphoreGive(cmdqSemaphore);
}

/* ------------------------ Local Functions --------------------------------- */

/*!
 * Dispatches the appropriate task to process/service the given command.  The
 * task will be dispatched by writing a pointer to the command into the task's
 * respective dispatch queue.  From there, the scheduler will ensure that the
 * task will run when it is appropriate to do so.
 *
 * @param[in] pCmd     The command packet to needs serviced.
 */
static void
cmdQueueDispatch(RM_GSP_COMMAND *pCmd)
{
    LwrtosQueueHandle queueHandle = NULL;
    RM_FLCN_QUEUE_HDR hdr;
    LwBool            bCmdDispatchNeeded = LW_TRUE;

    switch(pCmd->hdr.unitId)
    {

    case RM_GSP_UNIT_UNLOAD:
        // this will be forwarded to msgmgmt (that does deinit and notifies RM)
        bCmdDispatchNeeded = LW_FALSE;
        break;

    case RM_GSP_UNIT_NULL:
        bCmdDispatchNeeded = LW_FALSE;
        break;

    case RM_GSP_UNIT_TEST:
        queueHandle = testRequestQueue;
        break;

#if DO_TASK_HDCP1X
    case RM_GSP_UNIT_HDCP:
        queueHandle = Hdcp1xQueue;
        break;
#endif

#if DO_TASK_HDCP22WIRED
    case RM_GSP_UNIT_HDCP22WIRED:
        queueHandle = Hdcp22WiredQueue;
        break;
#endif

#if DO_TASK_SCHEDULER
    case RM_GSP_UNIT_SCHEDULER:
        queueHandle = schedulerRequestQueue;
        break;
#endif

    default:
        tprintf(LEVEL_CRIT, "UNIT %d not managed yet\n", pCmd->hdr.unitId);
        bCmdDispatchNeeded = LW_FALSE;
        break;
    }

    // Dispatch command to proper unit
    if (bCmdDispatchNeeded)
    {
        FLCN_STATUS ret;
        DISP2UNIT_CMD disp2Unit;

        disp2Unit.eventType  = DISP2UNIT_EVT_COMMAND;
        disp2Unit.cmdQueueId = GSP_CMD_QUEUE_RM;
        disp2Unit.pCmd       = pCmd;
        // For the moment, we block if the Unit queue is full
        ret = lwrtosQueueSendBlocking(queueHandle, &disp2Unit, sizeof(disp2Unit));
        if (ret != FLCN_OK)
        {
            tprintf(LEVEL_ERROR,
                    "Failed sending message to queue %p unit 0x%x: 0x%x\n",
                    queueHandle, pCmd->hdr.unitId, ret);
        }
    }
    else // ACK is good enough, handle invalid units here as well
    {
        FLCN_STATUS ret;

        cmdQueueSweep(&pCmd->hdr);

        hdr.unitId    = pCmd->hdr.unitId;
        hdr.ctrlFlags = 0;
        hdr.seqNumId  = pCmd->hdr.seqNumId;
        hdr.size      = RM_FLCN_QUEUE_HDR_SIZE;

        RM_FLCN_MSG_GSP gspMsg;
        gspMsg.hdr = hdr;

        ret = lwrtosQueueSendBlocking(rmMsgRequestQueue, &gspMsg, sizeof(gspMsg));
        if (ret != FLCN_OK)
        {
            tprintf(LEVEL_ERROR,
                    "Failed sending message to queue %p unit 0x%x: 0x%x\n",
                    queueHandle, pCmd->hdr.unitId, ret);
        }
    }
}

static void
processCommand(void)
{
    LwU32               head;
    RM_GSP_COMMAND     *pCmd;

    head = csbRead(ENGINE_REG(_QUEUE_HEAD(GSP_CMD_QUEUE_RM)));

    while (cmdqLastHandledCmdOffset != head)
    {
        pCmd = (RM_GSP_COMMAND *)mapRmOffsetToPtr(cmdqLastHandledCmdOffset);

        if (pCmd->hdr.unitId == RM_GSP_UNIT_REWIND)
        {
            tprintf(LEVEL_ERROR, "Unsupported command: rewind.\n");
            cmdqLastHandledCmdOffset = cmdqOffsetShared;
            // Head might have changed !
            head = csbRead(ENGINE_REG(_QUEUE_HEAD(GSP_CMD_QUEUE_RM)));
            // MK TODO: what if it changes while we're sweeping?
            cmdQueueSweep(&pCmd->hdr);
        }
        else
        {
            // must be done before dispatch
            cmdqLastHandledCmdOffset += LW_ALIGN_UP(pCmd->hdr.size, sizeof(LwU32));
            cmdQueueDispatch(pCmd);
        }
    }
}

/* ---------------------------------- Init Code ----------------------------- */

static void
initCommandQueue(void)
{
    cmdqOffsetShared = mapPtrToRmOffset(&cmdQueue[0]);
    cmdqLastHandledCmdOffset = cmdqOffsetShared;

    memset((void *)cmdQueue, 0, sizeof(cmdQueue));

    csbWrite(ENGINE_REG(_QUEUE_TAIL(GSP_CMD_QUEUE_RM)), cmdqOffsetShared);
    csbWrite(ENGINE_REG(_QUEUE_HEAD(GSP_CMD_QUEUE_RM)), cmdqOffsetShared);
    SYS_FLUSH_IO();
}

/*!
 * MK TL;DR This is task that recevies commands from RM and passes it to proper units
 */
lwrtosTASK_FUNCTION(rmCmdMain, pvParameters)
{
    FLCN_STATUS ret;
    LwrtosTaskHandle task_msg_mgmt = (LwrtosTaskHandle)pvParameters;

    ret = lwrtosSemaphoreCreateBinaryTaken(&cmdqSemaphore, 0);
    if (ret != FLCN_OK)
    {
        sysTaskExit(dbgStr(LEVEL_CRIT, "Failed to create cmdmgmt semaphore."), ret);
    }

    initCommandQueue();
    ret = lwrtosSemaphoreGive(cmdqSemaphore);
    if (ret != FLCN_OK)
    {
        sysTaskExit(dbgStr(LEVEL_CRIT, "Failed to create cmdmgmt semaphore."), ret);
    }

    sysCmdqRegisterNotifier(GSP_CMD_QUEUE_RM);
    sysCmdqEnableNotifier(GSP_CMD_QUEUE_RM);

    // Notify msg mgmt task that we're done.
    lwrtosTaskNotifySend(task_msg_mgmt, lwrtosTaskNOTIFICATION_NO_ACTION, 0);

    tprintf(LEVEL_INFO, "started command manager.\n");
    while (LW_TRUE)
    {
        FLCN_STATUS ret = -1;

        ret = lwrtosTaskNotifyWait(0, BIT(GSP_CMD_QUEUE_RM), NULL, lwrtosMAX_DELAY); // We get queue bit set
        if (ret == FLCN_OK)
        {
            tprintf(LEVEL_TRACE,
                    "Received RM message on queue head %x tail %x.\n",
                    csbRead(ENGINE_REG(_QUEUE_HEAD(GSP_CMD_QUEUE_RM))),
                    csbRead(ENGINE_REG(_QUEUE_TAIL(GSP_CMD_QUEUE_RM))));

            // MK TODO: we receive suprious interrupt, because we write head/tail
            if (csbRead(ENGINE_REG(_QUEUE_HEAD(GSP_CMD_QUEUE_RM))) == csbRead(ENGINE_REG(_QUEUE_TAIL(GSP_CMD_QUEUE_RM))))
            {
                sysCmdqEnableNotifier(GSP_CMD_QUEUE_RM);
                continue;
            }

            // TODO: dpu is able to listen to messages from others here (in theory), we don't :(
            processCommand();
            sysCmdqEnableNotifier(GSP_CMD_QUEUE_RM);
        }
        else
        {
            sysTaskExit(dbgStr(LEVEL_CRIT, "cmdmgmt: Faild waiting on RM queue. No RM communication is possible."), ret);
        }
    }
}
