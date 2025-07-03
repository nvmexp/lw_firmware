/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    lwoscmdmgmt.c
 * @copydoc lwoscmdmgmt.h
 */

/* ------------------------ System includes --------------------------------- */
#include "lwrtos.h"

/* ------------------------ Application includes ---------------------------- */
#include "lwoscmdmgmt.h"
#include "lwos_cmdmgmt_priv.h"
#include "lwoslayer.h"
#include "csb.h"

/* ------------------------ Type definitions -------------------------------- */
/* ------------------------ External definitions ---------------------------- */
/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Prototypes -------------------------------------- */
static LwBool s_osCmdmgmtQueueHasRoom(OS_CMDMGMT_QUEUE_DESCRIPTOR *pQD, LwU32 *pHead, LwU32 size);
static LwBool s_osCmdmgmtQueueIsEmpty(void *pQueueDesc);
#ifndef FB_QUEUES
static void   s_osCmdmgmtQueueRewind (OS_CMDMGMT_QUEUE_DESCRIPTOR *pQD, LwU32 *pHead);
#endif // FB_QUEUES

/* ------------------------ Global variables -------------------------------- */
/*!
 * Mutex managing tail pointers of CMD queues from different threads.
 */
LwrtosSemaphoreHandle       OsCmdmgmtQueueMutexCmd;

/*!
 * Descriptor of the queue used for sending messages from a FLCN to the RM.
 */
OS_CMDMGMT_QUEUE_DESCRIPTOR OsCmdmgmtQueueDescriptorRM = { 0 };

/* ------------------------ Macros ------------------------------------------ */

// MMINTS-TODO: 500 ms for now, re-evalute timeout to a more reasonable value later.
#define OS_CMDMGMT_QUEUE_WAIT_TIMEOUT_MS (500U)
#define OS_CMDMGMT_QUEUE_WAIT_TIMEOUT_US (OS_CMDMGMT_QUEUE_WAIT_TIMEOUT_MS * 1000U)

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief   Initializes mutexes protecting access to CMD/MSG queues.
 *
 * @return  FLCN_OK On success
 * @return  Others  Failures propogated from callees
 */
FLCN_STATUS
osCmdmgmtPreInit_IMPL(void)
{
    FLCN_STATUS status;

#ifndef FB_QUEUES
    status = lwrtosSemaphoreCreateBinaryGiven(&OsCmdmgmtQueueMutexCmd, OVL_INDEX_OS_HEAP);
    if (status != FLCN_OK)
    {
        OS_BREAKPOINT();
        goto osCmdmgmtPreInit_exit;
    }
#endif // FB_QUEUES

    // Allocate the MSG queue mutex in taken state until the INIT message is sent to the RM.
    status = lwrtosSemaphoreCreateBinaryTaken(&OsCmdmgmtQueueDescriptorRM.mutex, OVL_INDEX_OS_HEAP);
    if (status != FLCN_OK)
    {
        OS_BREAKPOINT();
        goto osCmdmgmtPreInit_exit;
    }

osCmdmgmtPreInit_exit:
    return status;
}

/*!
 * This function is to be called by unit thread when the unit is finished with
 * the command it is processing. This allows the queue manager to release the
 * space in the command queue that the command oclwpies.
 *
 * @param[in]   pHdr        A pointer to the acknowledged command's header.
 * @param[in]   queueId     The identifier of the queue containing command.
 */
void
osCmdmgmtCmdQSweep
(
    RM_FLCN_QUEUE_HDR  *pHdr,
    LwU8                queueId
)
{
    if (pHdr == NULL)
    {
        OS_BREAKPOINT();
        return;
    }

    (void)lwrtosSemaphoreTake(OsCmdmgmtQueueMutexCmd, lwrtosMAX_DELAY);
    {
        LwU32 tail = REG_RD32(CSB, OS_CMDMGMT_CMDQ_TAIL_REG((LwU32)queueId));
        LwU32 head = REG_RD32(CSB, OS_CMDMGMT_CMDQ_HEAD_REG((LwU32)queueId));

        pHdr->ctrlFlags |= RM_FLCN_QUEUE_HDR_FLAGS_UNIT_ACK;

        // If command is the last one, free as much as possible in the queue.
        if (pHdr == (RM_FLCN_QUEUE_HDR *)LWOS_SHARED_MEM_PA_TO_VA(tail))
        {
            while (tail != head)
            {
                pHdr = (RM_FLCN_QUEUE_HDR *)LWOS_SHARED_MEM_PA_TO_VA(tail);

                // If the command (REWIND included) is not yet ACKed, bail out.
                if ((pHdr->ctrlFlags & RM_FLCN_QUEUE_HDR_FLAGS_UNIT_ACK) == 0U)
                {
                    break;
                }

                if (pHdr->unitId == RM_FLCN_UNIT_ID_REWIND)
                {
                    tail = OS_CMDMGMT_QUEUE_START(queueId);
                }
                else
                {
                    // Round up size to a multiple of 4.
                    tail += LWUPROC_ALIGN_UP(pHdr->size, sizeof(LwU32));
                }

                REG_WR32(CSB, OS_CMDMGMT_CMDQ_TAIL_REG((LwU32)queueId), tail);
            }
        }
#ifdef DPU_RTOS
        //
        // On DPU, CMDQ intr will keep fired if HEAD != TAIL, and SW has no way
        // to clear it. Thus we can't unmask CMDQ intr at the same place as other
        // falcons (i.e. at the end of command dispatcher), otherwise we will have
        // interrupt storm. So we unmask the CMDQ intr here (i.e. after the
        // command is completely handled).
        //
        // Note that for now, we only support one command queue, so we only unmask
        // that one queue head interrupt.
        //
        REG_WR32(CSB, LW_PDISP_FALCON_CMDQ_INTRMASK,
                 DRF_DEF(_PDISP, _FALCON_CMDQ, _INTRMASK0, _ENABLE));
#endif // DPU_RTOS
    }
    (void)lwrtosSemaphoreGive(OsCmdmgmtQueueMutexCmd);
}

/*!
 * This function is to be called by unit thread when the Unit wants to send a
 * status message to the RM.
 *
 * @param[in]   pQD         RM message queue descriptor.
 * @param[in]   pHdr        A pointer to the header of the message.
 * @param[in]   pBody
 *    A pointer to the body of the message. The size of the body must be
 *    included in the 'size' field of the message header.
 * @param[in]   bBlock
 *    if set to LW_TRUE, this function will wait until the message queue has
 *    enough room to take the message.
 *    if set to LW_FALSE, it will return immediately if the queue is full.
 *
 * @return 'LW_TRUE'  if the message has been queued successfully.
 * @return 'LW_FALSE' if the messsge couldn't be queued. (e.g. queue is full
 *                    and bBlock is LW_TRUE)
 *
 * @pre     Both pHdr and pBody must be GCC_ATTRIB_ALIGN_QUEUE() aligned.
 */
LwBool
osCmdmgmtQueuePost_IMPL
(
    OS_CMDMGMT_QUEUE_DESCRIPTOR    *pQD,
    RM_FLCN_QUEUE_HDR              *pHdr,
    const void                     *pBody,
    LwBool                          bBlock
)
{
    LwU32   size;
    LwU32   head;
    LwBool  bQueued = LW_TRUE;

    if (pHdr == NULL)
    {
        OS_BREAKPOINT();
        return LW_FALSE;
    }

    // Note: @ref size is 32-bit but @ref pHdr->size is 8-bit (limited to 255).
    size = LWUPROC_ALIGN_UP(pHdr->size, sizeof(LwU32));
    if (size == 0U)
    {
        return bQueued;
    }

    (void)lwrtosSemaphoreTake(pQD->mutex, lwrtosMAX_DELAY);
    {
        head = pQD->headGet();

        while (!s_osCmdmgmtQueueHasRoom(pQD, &head, size))
        {
            if (bBlock)
            {
                (void)lwrtosLwrrentTaskDelayTicks(1);
            }
            else
            {
                bQueued = LW_FALSE;
                goto osCmdmgmtQueuePost_exit;
            }
        }

        // Write MSG to queue.
        pQD->dataPost(&head, pHdr, pBody, size);

        //
        // Head pointer of MSG queue being write protected at priv level 2,
        // we will elevate to level 2 before updating and switch back
        // after completion.
        //
        OS_TASK_SET_PRIV_LEVEL_BEGIN(RM_FALC_PRIV_LEVEL_LS, RM_FALC_PRIV_LEVEL_LS)
        {
#if LWRISCV_PARTITION_SWITCH
            lwosTaskCriticalEnter();
            {
                //
                // When partition switch is enabled, ucodes may want this to be atomic.
                // For example: in RISCV PMU's current implementation of
                // lwrtosHookPartitionPreSwitch, before entering an HS partition (which
                // is under lockdown), we ensure that RM has processed all data sent
                // from PMU. However, if this block was not atomic, a partition switch
                // triggered between updating head and sending the interrupt would never
                // satisfy the queue-empty condition in lwrtosHookPartitionPreSwitch,
                // which would result in a crash.
                //
#endif // LWRISCV_PARTITION_SWITCH

                // Update head pointer and send an interrupt to the RM.
                pQD->headSet(head);

#if LWRISCV_PARTITION_SWITCH
            }
            lwosTaskCriticalExit();
#endif // LWRISCV_PARTITION_SWITCH
        }
        OS_TASK_SET_PRIV_LEVEL_END();
    }
osCmdmgmtQueuePost_exit:
    (void)lwrtosSemaphoreGive(pQD->mutex);

    return bQueued;
}

/*!
 * @brief   Write message to the queue located within falcon's DMEM.
 *
 * @param[in,out]   pHead   Current location within the queue.
 * @param[in]       pHdr    A pointer to the header of the message.
 * @param[in]       pBody   A pointer to the body of the message.
 * @param[in]       size    The number of bytes to write.
 */
void
osCmdmgmtQueueWrite_DMEM
(
    LwU32              *pHead,
    RM_FLCN_QUEUE_HDR  *pHdr,
    const void         *pBody,
    LwU32               size
)
{
    (void)memcpy((LwU32 *)LWOS_SHARED_MEM_PA_TO_VA(*pHead), pHdr, RM_FLCN_QUEUE_HDR_SIZE);
    *pHead += RM_FLCN_QUEUE_HDR_SIZE;

    size -= RM_FLCN_QUEUE_HDR_SIZE;

    (void)memcpy((LwU32 *)LWOS_SHARED_MEM_PA_TO_VA(*pHead), pBody, size);
    *pHead += size;
}

/*!
 * @brief   Function wrapper for queueing blocking messages to the RM.
 *
 * @copydoc osCmdmgmtQueuePost_IMPL
 */
LwBool
osCmdmgmtRmQueuePostBlocking_IMPL
(
    RM_FLCN_QUEUE_HDR  *pHdr,
    const void         *pBody
)
{
    return osCmdmgmtQueuePost_IMPL(&OsCmdmgmtQueueDescriptorRM, pHdr, pBody, LW_TRUE);
}

/*!
 * @brief   Function wrapper for queueing non-blocking messages to the RM.
 *
 * @copydoc osCmdmgmtQueuePost_IMPL
 */
LwBool
osCmdmgmtRmQueuePostNonBlocking_IMPL
(
    RM_FLCN_QUEUE_HDR  *pHdr,
    const void         *pBody
)
{
    return osCmdmgmtQueuePost_IMPL(&OsCmdmgmtQueueDescriptorRM, pHdr, pBody, LW_FALSE);
}

/*!
 * This function is to be called by code that wants to wait until a queue is empty.
 * Does a busy wait b.c. it will be used in atomic and ISR contexts.
 *
 * @param[in]   pQD   RM message queue descriptor.
 *
 * @return 'FLCN_OK'          when the queue is empty.
 * @return 'FLCN_ERR_TIMEOUT' if a timeout has been hit on waiting for the queue to be empty.
 */
FLCN_STATUS
osCmdmgmtQueueWaitForEmpty(OS_CMDMGMT_QUEUE_DESCRIPTOR *pQD)
{
    // Wait until debug buffer is empty
    if (!OS_PTIMER_SPIN_WAIT_US_COND(s_osCmdmgmtQueueIsEmpty,
        pQD, OS_CMDMGMT_QUEUE_WAIT_TIMEOUT_US))
    {
        return FLCN_ERR_TIMEOUT;
    }

    return FLCN_OK;
}

/* ------------------------ Static Functions -------------------------------- */
/*!
 * This function is responsible for determining if the message queue, in its
 * current state, has room to fit a message of requested size. If the message
 * queue does not have enough space but would after a REWIND operation, than
 * the REWIND is issued and head pointer updated accordingly.
 *
 * @param[in,out]   pHead   Current location within the queue.
 * @param[in]       size    The number of bytes the caller desires to write.
 *
 * @return  LW_TRUE if message queue has enough room, LW_FALSE otherwise.
 */
static LwBool
s_osCmdmgmtQueueHasRoom
(
    OS_CMDMGMT_QUEUE_DESCRIPTOR    *pQD,
    LwU32                          *pHead,
    LwU32                           size
)
#ifdef FB_QUEUES
{
    LwU32 tail = pQD->tailGet();
    LwU32 nextHead = *pHead;

    nextHead++;
    if (nextHead >= pQD->end)
    {
        nextHead = pQD->start;
    }

    return tail != nextHead;
}
#else // FB_QUEUES
{
    LwU32 head = *pHead;
    LwU32 tail = pQD->tailGet();

    if (tail > head)
    {
        if ((head + size) < tail)
        {
            return LW_TRUE;
        }
    }
    else
    {
        if ((head + size) < pQD->end)
        {
            return LW_TRUE;
        }
        else
        {
            if ((pQD->start + size) < tail)
            {
                s_osCmdmgmtQueueRewind(pQD, pHead);

                return LW_TRUE;
            }
        }
    }

    return LW_FALSE;
}
#endif // FB_QUEUES

/*!
 * This function is responsible for determining if a queue, in its
 * current state, is empty.
 *
 * @param[in]   pQueueDesc   queue descriptor.
 *
 * @return  LW_TRUE if queue is empty, LW_FALSE otherwise.
 */
static LwBool
s_osCmdmgmtQueueIsEmpty(void *pQueueDesc)
{
    OS_CMDMGMT_QUEUE_DESCRIPTOR *pQD = pQueueDesc;
    return pQD->headGet() == pQD->tailGet();
}

#ifndef FB_QUEUES
/*!
 * @brief   Writes a rewind message to the message queue and updates 'pHead'.
 *
 * @param[in,out]   pHead   Current location within the queue. After the
 *                          operation points to the start of the queue.
 */
static void
s_osCmdmgmtQueueRewind
(
    OS_CMDMGMT_QUEUE_DESCRIPTOR    *pQD,
    LwU32                          *pHead
)
{
    RM_FLCN_QUEUE_HDR hdr;

    //
    // Going to cast this down to an LwU8 below, so assert that we won't lose
    // any bits
    //
    ct_assert(RM_FLCN_QUEUE_HDR_SIZE <= LW_U8_MAX);

    hdr.unitId    = RM_FLCN_UNIT_ID_REWIND;
    hdr.size      = (LwU8)RM_FLCN_QUEUE_HDR_SIZE;
    hdr.ctrlFlags = 0;
    hdr.seqNumId  = 0;

    pQD->dataPost(pHead, &hdr, NULL, RM_FLCN_QUEUE_HDR_SIZE);

    *pHead = pQD->start;
}
#endif // FB_QUEUES
