/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */


/*!
 * @file   msgmgmt.c
 * @brief  Represents task that is responsible for initializing
 *         all queues (command and message queues) and sending init ACK to RM
 *
 */

/* ------------------------ System Includes -------------------------------- */

/* ------------------------ LW Includes ------------------------------------ */
#include <lwtypes.h>
#include <lwuproc.h>
#include <rmsec2cmdif.h>

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

// 
// TODO sahilc : This is a placeholder value required for CMD QUEUE entry in RM_SEC2_INIT_MSG_SEC2_INIT (always index 0) 
//               since MSQ QUEUE entry is always index 1.It should be replaced with correct entry once
//               command queue mgmt task is setup.
//
sysTASK_DATA LwU32 cmdqOffsetShared;

/* ------------------------ Global variables ------------------------------- */
/* ------------------------ Static variables ------------------------------- */

// RM-Compatible offset of message queue (in DMEM/EMEM space)
static LwU32 msgqOffset  GCC_ATTRIB_SECTION("globalData", "msgqOffset");

// RM Message queue (inside EMEM)
static LwU32 msgQueue[PROFILE_MSG_QUEUE_LENGTH/sizeof(LwU32)]
    GCC_ATTRIB_SECTION("rm_rtos_queues", "msgQueue");

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

// Write message to queue
sysSYSLIB_CODE void
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

sysSYSLIB_CODE void
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

sysSYSLIB_CODE LwBool
msgQueueHasRoom(LwU32 *pHead, LwU32 size)
{
    LwU32 head = *pHead;
    LwU32 tail = csbRead(ENGINE_REG(_MSGQ_TAIL(SEC2_MSG_QUEUE_RM)));

    if (tail > head)
    {
        if (((LW_U32_MAX - head) >= size) && ((head + size) < tail))
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

sysSYSLIB_CODE LwBool
msgQueuePostBlocking(PRM_FLCN_QUEUE_HDR pHdr, const void *pBody)
{
    LwU32   size;
    LwU32   head;

    size = LW_ALIGN_UP(pHdr->size, sizeof(LwU32));
    if (size == 0)
    {
        return LW_TRUE;
    }

    head = csbRead(ENGINE_REG(_MSGQ_HEAD(SEC2_MSG_QUEUE_RM)));

    while (!msgQueueHasRoom(&head, size))
    {
        lwrtosLwrrentTaskDelayTicks(1);
    }

    // Write MSG to queue.
    ememQueueWrite(mapRmOffsetToPtr(head), pHdr, pBody, size);
    head += size;

    // Update head pointer and send an interrupt to the RM.
    csbWrite(ENGINE_REG(_MSGQ_HEAD(SEC2_MSG_QUEUE_RM)), head);
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

    csbWrite(ENGINE_REG(_MSGQ_TAIL(SEC2_MSG_QUEUE_RM)), msgqOffset);
    csbWrite(ENGINE_REG(_MSGQ_HEAD(SEC2_MSG_QUEUE_RM)), msgqOffset);
}

static void
sendRmInitMessage(FLCN_STATUS status)
{
    RM_FLCN_QUEUE_HDR           hdr;
    RM_SEC2_INIT_MSG_SEC2_INIT  msg;

    msg.qInfo[0].queueSize   = PROFILE_CMD_QUEUE_LENGTH;
    msg.qInfo[0].queueOffset = cmdqOffsetShared;
    msg.qInfo[0].queuePhyId  = 0;
    msg.qInfo[0].queueLogId  = SEC2_RM_CMDQ_LOG_ID;

    msg.qInfo[1].queueSize   = PROFILE_MSG_QUEUE_LENGTH;
    msg.qInfo[1].queueOffset = msgqOffset;
    msg.qInfo[1].queuePhyId  = 0;
    msg.qInfo[1].queueLogId  = SEC2_RM_MSGQ_LOG_ID;

    msg.numQueues = 2;

    msg.status = status;
    // fill-in the header of the INIT message and send it to the RM
    hdr.unitId    = RM_SEC2_UNIT_INIT;
    hdr.ctrlFlags = RM_FLCN_QUEUE_HDR_FLAGS_EVENT;
    hdr.seqNumId  = 0;
    hdr.size      = RM_SEC2_MSG_SIZE(INIT, SEC2_INIT);
    msg.msgType   = RM_SEC2_MSG_TYPE(INIT, SEC2_INIT);
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
    FLCN_STATUS status = FLCN_OK;

    initMessageQueue();

    //
    // MK TODO: better serialization?
    // We have to wait until command task is initialized
    lwrtosTaskNotifyWait(0, 0, NULL, lwrtosMAX_DELAY);

    sendRmInitMessage(status);
 
    while (LW_TRUE)
    {
        MSG_FROM_TASK msg;
        FLCN_STATUS ret = FLCN_ERROR;
 
        ret = lwrtosQueueReceive(rmMsgRequestQueue, &msg, sizeof(msg), lwrtosMAX_DELAY);
 
        if (ret == FLCN_OK)
        {        
           tprintf(LEVEL_TRACE, "Passing message from unit %d size %d to RM.\n", msg.hdr.unitId, msg.hdr.size);
            // MK TODO: probably verify payload pointer or sth?
            msgQueuePostBlocking(&msg.hdr, msg.payload);
        }
        else
        {
            sysTaskExit("msgmgmt: Faild waiting on queue. No RM communication is possible.", ret);
        }
    }

}

