/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
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
#include <liblwriscv/shutdown.h>

#include <lwriscv/peregrine.h>
#include <lwriscv/csr.h>
#include <lwriscv/sbi.h>

#define PROFILE_CMD_QUEUE_LENGTH    (0x80ul)
#define PROFILE_MSG_QUEUE_LENGTH    (0x80ul)
#define RM_DMEM_EMEM_OFFSET         0x1000000U
#define GSP_MSG_QUEUE_RM            0
#define GSP_CMD_QUEUE_RM            0

static LwU32 cmdqOffset;
static LwU32 msgqOffset;

static LwU32 mapPtrToRmOffset(void *pEmemPtr);
static void *mapRmOffsetToPtr(LwU32 offset);

/* ------------------------ Global variables ------------------------------- */
/* ------------------------ Static variables ------------------------------- */

// RM-Compatible offset of message queue (in DMEM/EMEM space)

// RM Message and Command queues (inside EMEM)
static volatile LwU32 *msgQueue = (LwU32*)LW_RISCV_AMAP_EMEM_START;
static volatile LwU32 *cmdQueue = (LwU32*)LW_RISCV_AMAP_EMEM_START + (PROFILE_MSG_QUEUE_LENGTH / sizeof(LwU32));

// RM-Compatible of next empty spot in command queue
static LwU32 cmdqLastHandledCmdOffset = 0;


/* ------------------------ RM queue handling ------------------------------ */

static LwU32 mapPtrToRmOffset(void *pEmemPtr)
{
    LwU64 offset = 0;

    if ((LwUPtr)pEmemPtr < LW_RISCV_AMAP_EMEM_START)
    {
        dbgPrintf(LEVEL_ERROR, "%s: Invalid queue pointer: %p.\n", __FUNCTION__, pEmemPtr);
        return 0;
    }

    offset = (LwUPtr)pEmemPtr - LW_RISCV_AMAP_EMEM_START;

    if (offset >= LW_RISCV_AMAP_EMEM_END)
    {
        dbgPrintf(LEVEL_ERROR, "%s: Invalid queue pointer: %p.\n", __FUNCTION__, pEmemPtr);
        return 0;
    }

    return (LwU32)offset + RM_DMEM_EMEM_OFFSET;
} // OK

static void *mapRmOffsetToPtr(LwU32 offset)
{
    if ((offset < RM_DMEM_EMEM_OFFSET) ||
        ((offset - RM_DMEM_EMEM_OFFSET) >= (PROFILE_MSG_QUEUE_LENGTH + PROFILE_CMD_QUEUE_LENGTH)))
    {
        dbgPrintf(LEVEL_ERROR, "%s: Invalid queue offset: %x.\n", __FUNCTION__, offset);
        return NULL;
    }

    offset -= RM_DMEM_EMEM_OFFSET;

    return (void*)(LW_RISCV_AMAP_EMEM_START + offset);
} // ok

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
} // OK

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
} // OK

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
} // OK

LwBool
msgQueuePostBlocking(PRM_FLCN_QUEUE_HDR pHdr, const void *pBody)
{
    LwU32   size;
    LwU32   head;

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

    // Write MSG to queue.
    ememQueueWrite(mapRmOffsetToPtr(head), pHdr, pBody, size);
    head += size;

    // Update head pointer and send an interrupt to the RM.
    priWrite(LW_PGSP_MSGQ_HEAD(GSP_MSG_QUEUE_RM), head);

    // Trigger interrupt
    localWrite(LW_PRGNLCL_FALCON_IRQSSET,
               DRF_DEF(_PRGNLCL, _FALCON_IRQSSET, _SWGEN0, _SET));

    return LW_TRUE;
} // OK

static void
initMessageQueue(void)
{
    // setup the head and tail pointers of the message queue
    msgqOffset = mapPtrToRmOffset((void*)&msgQueue[0]);

    memset((void *)msgQueue, 0, PROFILE_MSG_QUEUE_LENGTH);

    priWrite(LW_PGSP_MSGQ_TAIL(GSP_MSG_QUEUE_RM), msgqOffset);
    priWrite(LW_PGSP_MSGQ_HEAD(GSP_MSG_QUEUE_RM), msgqOffset);
} // OK

static void
initCommandQueue(void)
{
    cmdqOffset = mapPtrToRmOffset((void*)&cmdQueue[0]);
    cmdqLastHandledCmdOffset = cmdqOffset;

    memset((void *)cmdQueue, 0, PROFILE_CMD_QUEUE_LENGTH);

    priWrite(LW_PGSP_QUEUE_TAIL(GSP_CMD_QUEUE_RM), cmdqOffset);
    priWrite(LW_PGSP_QUEUE_HEAD(GSP_CMD_QUEUE_RM), cmdqOffset);
} // OK

void
sendRmInitMessage(FLCN_STATUS status)
{
    RM_FLCN_QUEUE_HDR         hdr;
    RM_GSP_INIT_MSG_GSP_INIT  msg;

    msg.qInfo[0].queueSize   = PROFILE_CMD_QUEUE_LENGTH;
    msg.qInfo[0].queueOffset = cmdqOffset;
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
} // OK

static void cmdQueueSweep(PRM_FLCN_QUEUE_HDR pHdr)
{
    LwU32 tail;
    LwU32 head;

    tail = priRead(LW_PGSP_QUEUE_TAIL(GSP_CMD_QUEUE_RM));
    head = priRead(LW_PGSP_QUEUE_HEAD(GSP_CMD_QUEUE_RM));

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
                tail = cmdqOffset;
            }
            else
            {
                // Round up size to a multiple of 4.
                tail += LW_ALIGN_UP((LwU32)pHdr->size, (LwU32)sizeof(LwU32));
            }

            priWrite(LW_PGSP_QUEUE_TAIL(GSP_CMD_QUEUE_RM), tail);
        }
    }
} // OK

void initCmdmgmt(void)
{
    initCommandQueue();
    initMessageQueue();

    // Enable command queue interrupt
    priWrite(LW_PGSP_CMD_INTREN,
             priRead(LW_PGSP_CMD_INTREN) | 1);

    sendRmInitMessage(FLCN_OK);
} // OK

// returns true if we're unloaded
LwBool pollUnloadCommand(LwBool ackUnload)
{
    if  (!ackUnload && FLD_TEST_DRF_NUM(_PGSP, _CMD_HEAD_INTRSTAT, _0, 0, priRead(LW_PGSP_CMD_HEAD_INTRSTAT)))
        return LW_FALSE;

    // Clear interrupt
    priWrite(LW_PGSP_CMD_HEAD_INTRSTAT, 1);

    // Skip suprious interrupts
    if (priRead(LW_PGSP_QUEUE_HEAD(GSP_CMD_QUEUE_RM)) == priRead(LW_PGSP_QUEUE_TAIL(GSP_CMD_QUEUE_RM)))
        return LW_FALSE;

    LwU32               head;
    RM_GSP_COMMAND     *pCmd;

    head = priRead(LW_PGSP_QUEUE_HEAD(GSP_CMD_QUEUE_RM));

    while (cmdqLastHandledCmdOffset != head)
    {
        pCmd = (RM_GSP_COMMAND *)mapRmOffsetToPtr(cmdqLastHandledCmdOffset);


        if (pCmd->hdr.unitId == RM_GSP_UNIT_REWIND)
        {
            // MK TODO: why did we have it? and do we care about rewind?
            cmdqLastHandledCmdOffset = cmdqOffset;
            // Head might have changed !
            head = priRead(LW_PGSP_QUEUE_HEAD(GSP_CMD_QUEUE_RM));
            // MK TODO: what if it changes while we're sweeping?
            cmdQueueSweep(&pCmd->hdr);
        }
        else if (!ackUnload && (pCmd->hdr.unitId == RM_GSP_UNIT_UNLOAD))
        {
            return LW_TRUE;
        }
        else // just ack whatever we have
        {
            // must be done before dispatch
            cmdqLastHandledCmdOffset += LW_ALIGN_UP((LwU32)pCmd->hdr.size, (LwU32)sizeof(LwU32));
            // ACK is good enough, handle invalid units here as well
            RM_FLCN_QUEUE_HDR hdr;

            cmdQueueSweep(&pCmd->hdr);

            hdr.unitId    = pCmd->hdr.unitId;
            hdr.ctrlFlags = 0;
            hdr.seqNumId  = pCmd->hdr.seqNumId;
            hdr.size      = RM_FLCN_QUEUE_HDR_SIZE;

            RM_FLCN_MSG_GSP gspMsg;
            gspMsg.hdr = hdr;
            if (gspMsg.hdr.unitId == RM_GSP_UNIT_UNLOAD)
            {
                printf("Shutdown requested. So shutting down.\n");
            }

            msgQueuePostBlocking(&gspMsg.hdr, &gspMsg.msg);

            if (gspMsg.hdr.unitId == RM_GSP_UNIT_UNLOAD)
            {
                // At this point "plug" (reset) may be taken out anytime
                return LW_TRUE;
            }
        }
    }
    return LW_FALSE;
}
