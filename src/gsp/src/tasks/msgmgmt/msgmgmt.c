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
#include <shlib/syscall.h>

/* ------------------------ Module Includes -------------------------------- */
#include "config/g_sections_riscv.h"
#include "unit_dispatch.h"
#include "tasks.h"
#include "cmdmgmt.h"

#define tprintf(level, ...)                dbgPrintf(level, "msg: " __VA_ARGS__)

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
// Give 5 seconds for self-test
#define INIT_ACK_TIMEOUT_TICKS    (LWRTOS_TICK_RATE_HZ * 5)
// shared with command queue task
extern sysTASK_DATA LwU32 cmdqOffsetShared;
extern LwU32 mapPtrToRmOffset(void *pEmemPtr);
extern void *mapRmOffsetToPtr(LwU32 offset);

/* ------------------------ Global variables ------------------------------- */
/* ------------------------ Static variables ------------------------------- */

// RM-Compatible offset of message queue (in DMEM/EMEM space)
static LwU32 msgqOffset;

// RM Message queue (inside EMEM)
static LwU32 msgQueue[PROFILE_MSG_QUEUE_LENGTH/sizeof(LwU32)]
    GCC_ATTRIB_SECTION("rm_rtos_queues", "msgQueue");

/* ------------------------ RM queue handling ------------------------------ */

// Write message to queue
static void
ememQueueWrite
(
    void               *pEmemBuffer,
    RM_FLCN_QUEUE_HDR  *pHdr,
    const void         *pBody,
    LwU32               size
)
{
    if ((pHdr == NULL) || (size < RM_FLCN_QUEUE_HDR_SIZE))
    {
        dbgPrintf(LEVEL_ERROR, "%s: Invalid arguments.", __FUNCTION__);
        return;
    }

    memcpy(pEmemBuffer, pHdr, RM_FLCN_QUEUE_HDR_SIZE);
    pEmemBuffer = (LwU8*)pEmemBuffer + RM_FLCN_QUEUE_HDR_SIZE;

    size -= RM_FLCN_QUEUE_HDR_SIZE;

    if (pBody != NULL)
    {
        memcpy(pEmemBuffer, pBody, size);
    }
}

static void
msgQueueRewind(LwU32 *pHead)
{
    RM_FLCN_QUEUE_HDR hdr;

    hdr.unitId    = RM_FLCN_UNIT_ID_REWIND;
    hdr.size      = RM_FLCN_QUEUE_HDR_SIZE;
    hdr.ctrlFlags = 0;
    hdr.seqNumId  = 0;

    ememQueueWrite(mapRmOffsetToPtr(*pHead), &hdr, NULL, RM_FLCN_QUEUE_HDR_SIZE);

    *pHead = msgqOffset;
}

static LwBool
msgQueueHasRoom(LwU32 *pHead, LwU32 size)
{
    LwU32 head = *pHead;
    LwU32 tail = csbRead(ENGINE_REG(_MSGQ_TAIL(GSP_MSG_QUEUE_RM)));

    if (tail > head)
    {
        if ((head + size) < tail)
        {
            return LW_TRUE;
        }
    }
    else
    {
        if ((head + size) < (msgqOffset + PROFILE_MSG_QUEUE_LENGTH))
        {
            return LW_TRUE;
        }
        else
        {
            if ((msgqOffset + size) < tail)
            {
                msgQueueRewind(pHead);

                return LW_TRUE;
            }
        }
    }

    return LW_FALSE;
}

static LwBool
msgQueuePostBlocking(PRM_FLCN_QUEUE_HDR pHdr, const void *pBody)
{
    LwU32   size;
    LwU32   head;

    size = LW_ALIGN_UP(pHdr->size, sizeof(LwU32));
    if (size == 0)
    {
        return LW_TRUE;
    }

    head = csbRead(ENGINE_REG(_MSGQ_HEAD(GSP_MSG_QUEUE_RM)));

    while (!msgQueueHasRoom(&head, size))
    {
        lwrtosLwrrentTaskDelayTicks(1);
    }

    // Write MSG to queue.
    ememQueueWrite(mapRmOffsetToPtr(head), pHdr, pBody, size);
    head += size;

    // Update head pointer and send an interrupt to the RM.
    csbWrite(ENGINE_REG(_MSGQ_HEAD(GSP_MSG_QUEUE_RM)), head);
    SYS_FLUSH_IO();

    sysSendSwGen(SYS_INTR_SWGEN0);

    return LW_TRUE;
}

/* ---------------------------------- Init Code ----------------------------- */

static void
initMessageQueue(void)
{
    // setup the head and tail pointers of the message queue
    msgqOffset = mapPtrToRmOffset(&msgQueue[0]);

    memset((void *)msgQueue, 0, sizeof(msgQueue));

    csbWrite(ENGINE_REG(_MSGQ_TAIL(GSP_MSG_QUEUE_RM)), msgqOffset);
    csbWrite(ENGINE_REG(_MSGQ_HEAD(GSP_MSG_QUEUE_RM)), msgqOffset);
    SYS_FLUSH_IO();
}

static void
sendRmInitMessage(FLCN_STATUS status)
{
    RM_FLCN_QUEUE_HDR         hdr;
    RM_GSP_INIT_MSG_GSP_INIT  msg;

    msg.qInfo[0].queueSize   = PROFILE_CMD_QUEUE_LENGTH;
    msg.qInfo[0].queueOffset = cmdqOffsetShared;
    msg.qInfo[0].queuePhyId  = 0;
    msg.qInfo[0].queueLogId  = RM_GSP_CMDQ_LOG_ID;

    msg.qInfo[1].queueSize   = PROFILE_MSG_QUEUE_LENGTH;
    msg.qInfo[1].queueOffset = msgqOffset;
    msg.qInfo[1].queuePhyId  = 0;
    msg.qInfo[1].queueLogId  = RM_GSP_MSGQ_LOG_ID;

    msg.numQueues = 2;

    msg.status = status;
    // fill-in the header of the INIT message and send it to the RM
    hdr.unitId    = RM_GSP_UNIT_INIT;
    hdr.ctrlFlags = RM_FLCN_QUEUE_HDR_FLAGS_EVENT;
    hdr.seqNumId  = 0;
    hdr.size      = RM_GSP_MSG_SIZE(INIT, GSP_INIT);
    msg.msgType   = RM_GSP_MSG_TYPE(INIT, GSP_INIT);
    msgQueuePostBlocking(&hdr, &msg);
}

/*!
 *
 * This is task that receives messages from units and passess it back to RM.
 *
 * TODO: This task may become obsolete if WHIS provides poll()
 */
lwrtosTASK_FUNCTION(rmMsgMain, pvParameters)
{
    /*!
     * First we wait for INIT Ack from 'TEST' Task on a conservative
     * TIMEOUT value(set to 10000 clock ticks now). Once Init'ed it is
     * reset to lwrtosMAX_DELAY to keep the message queue available
     * for Post Init Messages.
     * bInitialized records INIT State reported.
     */
    LwBool bInitialized = LW_FALSE;
    LwUPtr ticksToWait = INIT_ACK_TIMEOUT_TICKS;

    initMessageQueue();

    // MK TODO: better serialization?
    // We have to wait until command task is initialized
    lwrtosTaskNotifyWait(0, 0, NULL, lwrtosMAX_DELAY);

    tprintf(LEVEL_INFO, "started message manager.\n");
    while (LW_TRUE)
    {
        RM_FLCN_MSG_GSP gspMsg;
        FLCN_STATUS ret = -1;

        ret = lwrtosQueueReceive(rmMsgRequestQueue, &gspMsg, sizeof(gspMsg), ticksToWait);
        if (ret == FLCN_OK)
        {
            tprintf(LEVEL_TRACE,
                    "Passing message from unit %d size %d to RM.\n",
                    gspMsg.hdr.unitId, gspMsg.hdr.size);

            if (gspMsg.hdr.unitId == RM_GSP_UNIT_UNLOAD)
            {
                tprintf(LEVEL_INFO, "[%s] GSP system UNLOAD starting ...\n",
                        __FUNCTION__);

                // send back notification to RM (no payload needed)
                // MK: we probably sanity check hdr for size or sth
                msgQueuePostBlocking(&gspMsg.hdr, &gspMsg.msg);

                sysShutdown();
            }

            if (!bInitialized)
            {
                FLCN_STATUS status = FLCN_OK;
#ifdef DO_TASK_TEST
                if (gspMsg.hdr.unitId == RM_GSP_UNIT_INIT &&
                    gspMsg.msg.init.msgType == RM_GSP_INIT_MSG_ID_UNIT_READY &&
                    gspMsg.msg.init.msgUnitState.taskId == RM_GSP_TASK_ID_TEST)
                {
                    status = gspMsg.msg.init.msgUnitState.taskStatus;
                }
                else
                {
                    sysTaskExit(dbgStr(LEVEL_CRIT, "Received incorrect message while waiting for UNIT_READY.\n"), FLCN_ERROR);
                }
#endif
                // Make sure to pass on the error code
                bInitialized = LW_TRUE;
                ticksToWait = lwrtosMAX_DELAY;
                sendRmInitMessage(status);
            }
            else
            {
                // MK TODO: probably verify payload pointer or sth?
                msgQueuePostBlocking(&gspMsg.hdr, &gspMsg.msg);
            }
        }
        else
        {
            sysTaskExit(dbgStr(LEVEL_CRIT, "msgmgmt: Failed waiting on queue. No RM communication is possible."), ret);
        }
    }
}

