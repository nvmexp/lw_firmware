/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <lwtypes.h>
#include <lwmisc.h>

#include <dev_gsp.h>

#include <rmgspcmdif.h>
#include <riscvifriscv.h>

#include <liblwriscv/print.h>
#include <liblwriscv/io.h>
#include <liblwriscv/libc.h>
#include <liblwriscv/ptimer.h>
#include <lwriscv/peregrine.h>
#include <lwriscv/csr.h>
#include <lwriscv/sbi.h>

#include "cmdmgmt.h"
#include "partitions.h"
#include "misc.h"

static LwU32 cmdqStartOffset;
static LwU32 msgqStartOffset;

static LwU32 mapPtrToRmOffset(void *pEmemPtr);
static void *mapRmOffsetToPtr(LwU32 offset);

/* ------------------------ Global variables ------------------------------- */
/* ------------------------ Static variables ------------------------------- */

// RM-Compatible offset of message queue (in DMEM/EMEM space)

// RM Message and Command queues (inside EMEM)
static volatile LwU32 *msgQueueBuffer = (LwU32*)LW_RISCV_AMAP_EMEM_START;
static volatile LwU32 *cmdQueueBuffer = (LwU32*)LW_RISCV_AMAP_EMEM_START + (PROFILE_MSG_QUEUE_LENGTH / sizeof(LwU32));

// RM-Compatible of next empty spot in command queue
static LwU32 cmdqLastHandledCmdOffset = 0;


/* ------------------------ RM queue handling ------------------------------ */

static LwU32 mapPtrToRmOffset(void *pEmemPtr)
{
    LwU64 offset = 0;

    if ((LwUPtr)pEmemPtr < LW_RISCV_AMAP_EMEM_START)
    {
        dbgRmPrintf(LEVEL_ERROR, "%s: Invalid queue pointer: %p.\n", __FUNCTION__, pEmemPtr);
        return 0;
    }

    offset = (LwUPtr)pEmemPtr - LW_RISCV_AMAP_EMEM_START;

    if (offset >= LW_RISCV_AMAP_EMEM_END)
    {
        dbgRmPrintf(LEVEL_ERROR, "%s: Invalid queue pointer: %p.\n", __FUNCTION__, pEmemPtr);
        return 0;
    }

    return (LwU32)offset + RM_DMEM_EMEM_OFFSET;
}

static void *mapRmOffsetToPtr(LwU32 offset)
{
    if ((offset < RM_DMEM_EMEM_OFFSET) ||
        ((offset - RM_DMEM_EMEM_OFFSET) >= (PROFILE_MSG_QUEUE_LENGTH + PROFILE_CMD_QUEUE_LENGTH)))
    {
        dbgRmPrintf(LEVEL_ERROR, "%s: Invalid queue offset: %x.\n", __FUNCTION__, offset);
        return NULL;
    }

    offset -= RM_DMEM_EMEM_OFFSET;

    return (void*)(LW_RISCV_AMAP_EMEM_START + offset);
}

// Write message to queue
static void
ememQueueWrite
(
    void               *pEmemBuffer,
    RM_FLCN_QUEUE_HDR  *pHdr,
    const void         *pBody,
    LwU64               size
)
{
    if ((pHdr == NULL) || (size < RM_FLCN_QUEUE_HDR_SIZE))
    {
        dbgRmPrintf(LEVEL_ERROR, "%s: Invalid arguments.", __FUNCTION__);
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
msgQueueRewind(LwU32 *pHeadOffset)
{
    RM_FLCN_QUEUE_HDR hdr;
    void              *pHeadPtr;

    hdr.unitId    = RM_FLCN_UNIT_ID_REWIND;
    hdr.size      = RM_FLCN_QUEUE_HDR_SIZE;
    hdr.ctrlFlags = 0;
    hdr.seqNumId  = 0;

    pHeadPtr = mapRmOffsetToPtr(*pHeadOffset);

    if (pHeadPtr == NULL)
    {
        ccPanic();
    }

    ememQueueWrite(pHeadPtr, &hdr, NULL, RM_FLCN_QUEUE_HDR_SIZE);

    *pHeadOffset = msgqStartOffset;
}

static LwBool
msgQueueHasRoom(LwU32 *pHead, LwU32 size)
{
    LwU32 head = *pHead;
    LwU32 tail = priRead(LW_PGSP_MSGQ_TAIL(GSP_MSG_QUEUE_RM));

    if (tail > head)
    {
        if ((head + size) < tail)
        {
            return LW_TRUE;
        }
    }
    else
    {
        if ((head + size) < (msgqStartOffset + PROFILE_MSG_QUEUE_LENGTH))
        {
            return LW_TRUE;
        }
        else
        {
            if ((msgqStartOffset + size) < tail)
            {
                msgQueueRewind(pHead);

                return LW_TRUE;
            }
        }
    }

    return LW_FALSE;
}

LwBool msgQueueIsEmpty(void)
{
    return priRead(LW_PGSP_MSGQ_HEAD(GSP_MSG_QUEUE_RM)) ==
           priRead(LW_PGSP_MSGQ_TAIL(GSP_MSG_QUEUE_RM));
}

LwBool
msgQueuePostBlocking(PRM_FLCN_QUEUE_HDR pHdr, const void *pBody)
{
    LwU32   size;
    LwU32   head;
    void    *headPtr;

    size = LW_ALIGN_UP((LwU32)pHdr->size, (LwU32)sizeof(LwU32));
    if (size == 0)
    {
        return LW_TRUE;
    }

    head = priRead(LW_PGSP_MSGQ_HEAD(GSP_MSG_QUEUE_RM));

    while (!msgQueueHasRoom(&head, size))
    {
        ;
    }

    headPtr = mapRmOffsetToPtr(head);
    if (headPtr == NULL)
    {
        return LW_FALSE;
    }

    // Write MSG to queue.
    ememQueueWrite(headPtr, pHdr, pBody, size);
    head += size;

    // Update head pointer and send an interrupt to the RM.
    priWrite(LW_PGSP_MSGQ_HEAD(GSP_MSG_QUEUE_RM), head);

    // Trigger interrupt
    localWrite(LW_PRGNLCL_FALCON_IRQSSET,
               DRF_DEF(_PRGNLCL, _FALCON_IRQSSET, _SWGEN0, _SET));

    return LW_TRUE;
}

static void
initMessageQueue(void)
{
    // setup the head and tail pointers of the message queue
    msgqStartOffset = mapPtrToRmOffset((void*)&msgQueueBuffer[0]);

    memset((void *)msgQueueBuffer, 0, PROFILE_MSG_QUEUE_LENGTH);

    priWrite(LW_PGSP_MSGQ_TAIL(GSP_MSG_QUEUE_RM), msgqStartOffset);
    priWrite(LW_PGSP_MSGQ_HEAD(GSP_MSG_QUEUE_RM), msgqStartOffset);
}

static void
initCommandQueue(void)
{
    cmdqStartOffset = mapPtrToRmOffset((void*)&cmdQueueBuffer[0]);
    cmdqLastHandledCmdOffset = cmdqStartOffset;

    memset((void *)cmdQueueBuffer, 0, PROFILE_CMD_QUEUE_LENGTH);

    priWrite(LW_PGSP_QUEUE_TAIL(GSP_CMD_QUEUE_RM), cmdqStartOffset);
    priWrite(LW_PGSP_QUEUE_HEAD(GSP_CMD_QUEUE_RM), cmdqStartOffset);
}

static void
sendRmInitMessage(FLCN_STATUS status)
{
    RM_FLCN_QUEUE_HDR         hdr;
    RM_GSP_INIT_MSG_GSP_INIT  msg;

    msg.qInfo[0].queueSize   = PROFILE_CMD_QUEUE_LENGTH;
    msg.qInfo[0].queueOffset = cmdqStartOffset;
    msg.qInfo[0].queuePhyId  = 0;
    msg.qInfo[0].queueLogId  = RM_GSP_CMDQ_LOG_ID;

    msg.qInfo[1].queueSize   = PROFILE_MSG_QUEUE_LENGTH;
    msg.qInfo[1].queueOffset = msgqStartOffset;
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

void initCmdmgmt(void)
{
    initCommandQueue();
    initMessageQueue();

    // Enable command queue interrupt
    priWrite(LW_PGSP_CMD_INTREN, priRead(LW_PGSP_CMD_INTREN) | 1);

    sendRmInitMessage(FLCN_OK);
}

void cmdQueueSweep(LwU32 hdrOffset)
{
    LwU32 tail;
    LwU32 head;
    RM_FLCN_QUEUE_HDR *pHdrUnsafe = (RM_FLCN_QUEUE_HDR *)mapRmOffsetToPtr(hdrOffset);

    tail = priRead(LW_PGSP_QUEUE_TAIL(GSP_CMD_QUEUE_RM));
    head = priRead(LW_PGSP_QUEUE_HEAD(GSP_CMD_QUEUE_RM));

    pHdrUnsafe->ctrlFlags |= RM_FLCN_QUEUE_HDR_FLAGS_UNIT_ACK;

    // If command is the last one, free as much as possible in the queue.
    if (pHdrUnsafe == (PRM_FLCN_QUEUE_HDR)mapRmOffsetToPtr(tail))
    {
        while (tail != head)
        {
            pHdrUnsafe = (PRM_FLCN_QUEUE_HDR)mapRmOffsetToPtr(tail);

            // Not much we can do here. but this should not happen ever.
            if (pHdrUnsafe == NULL)
            {
                ccPanic();
            }

            // If the command (REWIND included) is not yet ACKed, bail out.
            if ((pHdrUnsafe->ctrlFlags & RM_FLCN_QUEUE_HDR_FLAGS_UNIT_ACK) == 0)
            {
                break;
            }

            if (pHdrUnsafe->unitId == RM_FLCN_UNIT_ID_REWIND)
            {
                tail = cmdqStartOffset;
            }
            else
            {
                // Round up size to a multiple of 4.
                tail += LW_ALIGN_UP((LwU32)pHdrUnsafe->size, (LwU32)sizeof(LwU32));
            }

            priWrite(LW_PGSP_QUEUE_TAIL(GSP_CMD_QUEUE_RM), tail);
        }
    }
}

static void
processCommand(void)
{
    LwU32               cmdHeadOffset;

    cmdHeadOffset = priRead(LW_PGSP_QUEUE_HEAD(GSP_CMD_QUEUE_RM));

    while (cmdqLastHandledCmdOffset != cmdHeadOffset)
    {
        RM_GSP_COMMAND *pCmdUnsafe = (RM_GSP_COMMAND *)mapRmOffsetToPtr(cmdqLastHandledCmdOffset);

        // This should never happen.
        if (pCmdUnsafe == NULL)
        {
            ccPanic();
        }

        RM_FLCN_QUEUE_HDR hdr;
        RMPROXY_DISPATCHER_REQUEST cmdCopy;

        // Copy header
        memcpy(&hdr, &pCmdUnsafe->hdr, sizeof(hdr));
        if (hdr.size > sizeof(RM_GSP_COMMAND))
        {
                ccPanic();
        }

        cmdCopy.hdrOffset = cmdqLastHandledCmdOffset;

        memcpy(&cmdCopy.cmd, pCmdUnsafe, hdr.size);
        // Copy old, verified header just in case
        memcpy(&cmdCopy.cmd.hdr, &hdr, sizeof(hdr));

        if (cmdCopy.cmd.hdr.unitId == RM_GSP_UNIT_REWIND)
        {
            cmdqLastHandledCmdOffset = cmdqStartOffset;

            // Sweep queue
            cmdQueueSweep(cmdCopy.hdrOffset);

            // Head might have changed, re-read to process further commands !
            cmdHeadOffset = priRead(LW_PGSP_QUEUE_HEAD(GSP_CMD_QUEUE_RM));
        }
        else
        {
            // must be done before dispatch
            cmdqLastHandledCmdOffset += LW_ALIGN_UP((LwU32)hdr.size,
                                                    (LwU32)sizeof(LwU32));
            cmdQueueProcess(&cmdCopy);
        }
    }
}

void cmdmgmtCmdLoop(void)
{
    while (1)
    {
        while (FLD_TEST_DRF_NUM(_PGSP, _CMD_HEAD_INTRSTAT, _0, 0,
                            priRead(LW_PGSP_CMD_HEAD_INTRSTAT)))
        { } // wait for interrupt

        // Clear interrupt
        priWrite(LW_PGSP_CMD_HEAD_INTRSTAT, 1);

        // Skip suprious interrupts
        if (priRead(LW_PGSP_QUEUE_HEAD(GSP_CMD_QUEUE_RM)) == priRead(LW_PGSP_QUEUE_TAIL(GSP_CMD_QUEUE_RM)))
            continue;

        processCommand();
    }
}
