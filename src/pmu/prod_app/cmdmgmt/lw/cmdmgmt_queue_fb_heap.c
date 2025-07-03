/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    cmdmgmt_queue_fb_heap.c
 * @copydoc cmdmgmt_queue_fb_heap.h
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu/ssurface.h"

#include "pmu_objpmu.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "cmdmgmt/cmdmgmt.h"
#include "cmdmgmt/cmdmgmt_dispatch.h"
#include "cmdmgmt/cmdmgmt_queue_fb.h"
#include "cmdmgmt/cmdmgmt_queue_fb_heap.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "lib/lib_nocat.h"

#include "dev_top.h" // Part of WAR for http://lwbugs/2767551

/* ------------------------- Type Definitions ------------------------------- */
/*!
 * @brief   Assure that the RM_PMU_RPC_HEADER fulfills FBQ requirements.
 */
ct_assert((sizeof(RM_PMU_RPC_HEADER) <= DMA_MAX_READ_ALIGNMENT) &&
          ONEBITSET(sizeof(RM_PMU_RPC_HEADER)));

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Compile Time Checks ---------------------------- */
// Assure that this file compiles only when required.
ct_assert(PMUCFG_FEATURE_ENABLED(PMU_FBQ) &&
          !PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_PER_TASK_COMMAND_BUFFER));

/* ------------------------- Global Variables ------------------------------- */
static RM_PMU_FBQ_MSG_Q_ELEMENT QueueFbDmaWriteBuffer
#ifdef UPROC_RISCV
    GCC_ATTRIB_SECTION("dmem_resident", "QueueFbDmaWriteBuffer")
#endif // UPROC_RISCV
    GCC_ATTRIB_ALIGN(DMA_MIN_READ_ALIGNMENT * 4); // MMINTS-TODO: this is a TEMP WAR, a HACK meant to unblock https://lwbugs/3363339 until fmodel issue is fixed

/* ------------------------- Macros and Defines ----------------------------- */
#define QUEUE_FB_HEAP_RM_HEAP_MASK_ELEMENT_TYPE        LwU32

/*!
 * Defines number of bits stored in each element of array.
 */
#define QUEUE_FB_HEAP_RM_HEAP_MASK_ELEMENT_BIT_SIZE \
    (sizeof(QUEUE_FB_HEAP_RM_HEAP_MASK_ELEMENT_TYPE) * 8U)

#define QUEUE_FB_HEAP_RM_HEAP_SIZE_BYTES \
    (DMEM_BLOCK_SIZE * PROFILE_RM_MANAGED_HEAP_SIZE_BLOCKS)

/*!
 * How much heap is covered by each mask element
 */
#define QUEUE_FB_HEAP_RM_HEAP_BYTES_COVERED_BY_MASK_ELEMENT \
    (RM_PMU_DMEM_HEAP_ALLOC_ALIGNMENT * QUEUE_FB_HEAP_RM_HEAP_MASK_ELEMENT_BIT_SIZE)

#define QUEUE_FB_HEAP_RM_HEAP_MASK_SIZE \
    (QUEUE_FB_HEAP_RM_HEAP_SIZE_BYTES / QUEUE_FB_HEAP_RM_HEAP_BYTES_COVERED_BY_MASK_ELEMENT)

/* ------------------------- Global Variables ------------------------------- */
ct_assert(QUEUE_FB_HEAP_RM_HEAP_SIZE_BYTES);

/*!
 * NJ-TODO
 */
static LwU8 QueueFbRmManagedHeap[QUEUE_FB_HEAP_RM_HEAP_SIZE_BYTES]
#ifdef UPROC_RISCV
    GCC_ATTRIB_SECTION("dmem_resident", "QueueFbRmManagedHeap")
    GCC_ATTRIB_ALIGN(32);
#else // UPROC_RISCV
    GCC_ATTRIB_SECTION("alignedData32", "QueueFbRmManagedHeap");
#endif // UPROC_RISCV

/*!
 * In use bits for the RM Manged DMEM Heap.  Used to track which chunks of DMEM
 * are in use.
 */
static QUEUE_FB_HEAP_RM_HEAP_MASK_ELEMENT_TYPE QueueFbRmManagedHeapAllocation[QUEUE_FB_HEAP_RM_HEAP_MASK_SIZE] = {0};

ct_assert(LW_IS_ALIGNED(QUEUE_FB_HEAP_RM_HEAP_SIZE_BYTES, RM_PMU_DMEM_HEAP_ALLOC_ALIGNMENT));

/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS s_queueFbCmdCopyIn(RM_FLCN_QUEUE_HDR **ppHdr, LwU8 queueId, LwU32 queuePos)
    GCC_ATTRIB_SECTION("imem_cmdmgmt", "s_queueFbCmdCopyIn");

static FLCN_STATUS s_queueFbHeapUseTracking(LwU32 allocStart, LwU16 allocSize, LwBool bSet)
    GCC_ATTRIB_SECTION("imem_resident", "s_queueFbHeapUseTracking");

static void s_queueFbCmdCopyOut(RM_FLCN_QUEUE_HDR *pHdr, LwU8 queueId)
    GCC_ATTRIB_SECTION("imem_resident", "s_queueFbCmdCopyOut");

static void s_queueFbSweep(LwU8 queueId, LwU32 index)
    GCC_ATTRIB_SECTION("imem_resident", "s_queueFbSweep");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc queueFbHeapInitRpcPopulate
 */
void
queueFbHeapInitRpcPopulate
(
    PMU_RM_RPC_STRUCT_CMDMGMT_INIT *pRpc
)
{
    // Populate pointer to PMU NOCAT diagnostic data buffer.
    pRpc->bufferStart = nocatBufferDmemPhysOffsetGet();

    //
    // Populate the RM managed heap data in the INIT message.
    //

    //
    // Sending back 0 to use relative offsets while allocating space from RM.
    // The offset will be adjusted when used in the PMU while processing cmd
    //
    pRpc->rmManagedAreaOffset = 0;

    //
    // This constant is going to be casted down to 16-bits, so make sure that
    // won't lose any actual data.
    //
    ct_assert(sizeof(QueueFbRmManagedHeap) <= LW_U16_MAX);
    pRpc->rmManagedAreaSize = (LwU16)sizeof(QueueFbRmManagedHeap);
}

/*!
 * @copydoc queueFbHeapProcessCmdQueue
 */
FLCN_STATUS
queueFbHeapProcessCmdQueue
(
    LwU32 headId
)
{
    static LwU32 QueueFbHeapQueuePtrLwrrent[RM_PMU_FBQ_CMD_COUNT] = { 0U, 0U };
    FLCN_STATUS status = FLCN_OK;
    LwU32 head = pmuQueueRmToPmuHeadGet_HAL(&Pmu, headId);

    if (headId >= RM_PMU_FBQ_CMD_COUNT)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        PMU_BREAKPOINT();
        goto queueFbHeapProcessCmdQueue_exit;
    }

    while (QueueFbHeapQueuePtrLwrrent[headId] != head)
    {
        DISP2UNIT_RM_RPC    rpcDisp;
        RM_FLCN_CMD_PMU    *pCmd;
        RM_FLCN_QUEUE_HDR  *pFlcnHdr;

        // Take DMA lock before DMA in.
        lwosDmaLockAcquire();

        status = s_queueFbCmdCopyIn(&pFlcnHdr, (LwU8)headId, QueueFbHeapQueuePtrLwrrent[headId]);
        if (status != FLCN_OK)
        {
            lwosDmaLockRelease();
            PMU_BREAKPOINT();
            break;
        }

        // Going through a cast to void * to avoid MISRA violations.
        pCmd = (void *)pFlcnHdr;

        if (!QUEUE_FB_IS_RPC_TARGET_LPWR(pFlcnHdr->unitId))
        {
            lwosDmaLockRelease();
        }

        QueueFbHeapQueuePtrLwrrent[headId]++;
        if (QueueFbHeapQueuePtrLwrrent[headId] >= RM_PMU_FBQ_CMD_NUM_ELEMENTS)
        {
            QueueFbHeapQueuePtrLwrrent[headId] = 0;
        }

        // Build dispatching structure.
        rpcDisp.hdr.eventType = DISP2UNIT_EVT_RM_RPC;
        rpcDisp.hdr.unitId    = pFlcnHdr->unitId;
        rpcDisp.cmdQueueId    = (LwU8)headId;
        rpcDisp.pRpc          = pCmd;

        // Dispatch command to the handling task.
        status = cmdmgmtExtCmdDispatch(&rpcDisp);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            break;
        }

        if (QUEUE_FB_IS_RPC_TARGET_LPWR(pFlcnHdr->unitId))
        {
            lwosDmaLockRelease();
        }
    }

queueFbHeapProcessCmdQueue_exit:
    return status;
}

/*!
 * @brief   Track dmem allocations to prevent double allocs and double frees.
 *
 * @param[in]   allocStart  Allocation start pointer.
 * @param[in]   allocSize   Allocation size.
 * @param[in]   bSet        LW_TRUE to set, LW_FALSE to clear.
 *
 * @return          LW_FALSE if there is an error.
 */
static FLCN_STATUS
s_queueFbHeapUseTracking
(
    LwU32   allocStart,
    LwU16   allocSize,
    LwBool  bSet
)
{
    FLCN_STATUS status;
    LwU32 iStart;
    LwU32 iEnd;
    LwU32 i;
    LwU16 allocSizeAligned;

    appTaskCriticalEnter();
    {
        // Verify Alignment and allocation is within the heap.
        if ((!LW_IS_ALIGNED(allocStart, RM_PMU_DMEM_HEAP_ALLOC_ALIGNMENT)) ||
            (allocStart >= QUEUE_FB_HEAP_RM_HEAP_SIZE_BYTES) ||
            ((allocStart + allocSize) > QUEUE_FB_HEAP_RM_HEAP_SIZE_BYTES))
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto s_queueFbHeapUseTracking_exit;
        }

        iStart = (allocStart / RM_PMU_DMEM_HEAP_ALLOC_ALIGNMENT);
        allocSizeAligned = (allocSize + RM_PMU_DMEM_HEAP_ALLOC_ALIGNMENT - 1U) /
                           RM_PMU_DMEM_HEAP_ALLOC_ALIGNMENT;
        iEnd = iStart + allocSizeAligned;

        // Assure bSet is really a Boolean.
        bSet = !!bSet;

        // Assure that the required chunks are in the correct state.
        for (i = iStart; i < iEnd; i++)
        {
            if (bSet == LWOS_BM_GET(QueueFbRmManagedHeapAllocation, i, QUEUE_FB_HEAP_RM_HEAP_MASK_ELEMENT_BIT_SIZE))
            {
                // Double alloc or double free.
                status = FLCN_ERR_ILWALID_ARGUMENT;
                goto s_queueFbHeapUseTracking_exit;
            }
        }

        // Set or clear the mask.
        for (i = iStart; i < iEnd; i++)
        {
            if (bSet)
            {
                LWOS_BM_SET(QueueFbRmManagedHeapAllocation, i, QUEUE_FB_HEAP_RM_HEAP_MASK_ELEMENT_BIT_SIZE);
            }
            else
            {
                LWOS_BM_CLR(QueueFbRmManagedHeapAllocation, i, QUEUE_FB_HEAP_RM_HEAP_MASK_ELEMENT_BIT_SIZE);
            }
        }

        status = FLCN_OK;

s_queueFbHeapUseTracking_exit:
        lwosNOP();
    }
    appTaskCriticalExit();

    return status;
}

/*!
 * @brief   Copies a FBQueue client payload from DMEM to the FB Queue in the
 *          Super Surface.
 *
 * @param[in]   pHdr        A pointer to the command's header.
 * @param[in]   queueId     The identifier of the queue containing command.
 *
 * @note    We do not mark heap as free since this command will still be used
 *          after this point.
 */
static void
s_queueFbCmdCopyOut
(
    RM_FLCN_QUEUE_HDR  *pHdr,
    LwU8                queueId
)
{
    RM_FLCN_FBQ_HDR    *pFbqHdr     = NULL;
    LwU32               entryOffset = 0;

    if (pHdr == NULL)
    {
        PMU_BREAKPOINT();
        goto s_queueFbCmdCopyOut_exit;
    }

    if (queueId >= RM_PMU_FBQ_CMD_COUNT)
    {
        PMU_BREAKPOINT();
        goto s_queueFbCmdCopyOut_exit;
    }

    pFbqHdr = FBQ_GET_FBQ_HDR_FROM_QUEUE_HDR(pHdr);

    if (pFbqHdr->elementIndex >= RM_PMU_FBQ_CMD_NUM_ELEMENTS)
    {
        PMU_BREAKPOINT();
        goto s_queueFbCmdCopyOut_exit;
    }

    entryOffset = LW_OFFSETOF(RM_PMU_FBQ_CMD_QUEUES,
        queue[queueId].qElement[pFbqHdr->elementIndex]);

    // DMA the CMD Queue Element back to FB, so payloads are available to RM.
    if (ssurfaceWr(pFbqHdr,
            RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(fbq.cmdQueues) + entryOffset,
            pFbqHdr->heapSize) != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_queueFbCmdCopyOut_exit;
    }

s_queueFbCmdCopyOut_exit:
    // Always perform DMEM heap book-keeping to preserve original code behavior.
    if (s_queueFbHeapUseTracking(
            pFbqHdr->heapOffset, pFbqHdr->heapSize, LW_FALSE) != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }
}

/*!
 * @brief   Copies a FBQueue Element from the FB Queue to DMEM.  Also performs
 *          all in use book keeping for this element.
 *
 * @param[out]  ppHdr       A pointer to a pointer the command's header.
 * @param[in]   queueId     The identifier of the queue containing command.
 * @param[in]   queuePos    Indexs of the Queue Element.
 *
 * @return      LW_OK       if all OK.
 * @return      propagate error from interfaces called within.
 */
FLCN_STATUS
s_queueFbCmdCopyIn
(
    RM_FLCN_QUEUE_HDR **ppHdr,
    LwU8                queueId,
    LwU32               queuePos
)
{
    RM_FLCN_FBQ_HDR             fbqHdr;
    RM_PMU_FBQ_CMD_Q_ELEMENT   *pFbqElement;
    FLCN_STATUS                 status = FLCN_OK;

    LwU32 entryOffset =
        RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(fbq.cmdQueues) +
        LW_OFFSETOF(RM_PMU_FBQ_CMD_QUEUES, queue[queueId].qElement[queuePos]);

    if ((ppHdr == NULL) ||
        (queuePos >= RM_PMU_FBQ_CMD_NUM_ELEMENTS) ||
        (queueId >= RM_PMU_FBQ_CMD_COUNT))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto s_queueFbCmdCopyIn_exit;
    }

    *ppHdr = NULL;

    // DMA just the FBQ HDR in to see how much to transfer where.
    status = ssurfaceRd(&fbqHdr, entryOffset, sizeof(fbqHdr));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_queueFbCmdCopyIn_exit;
    }

    // Claim ownership of the queue element and heap memory
    status = s_queueFbHeapUseTracking(fbqHdr.heapOffset, fbqHdr.heapSize, LW_TRUE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_queueFbCmdCopyIn_exit;
    }

    // Now know the size and the DMEM location
    pFbqElement = (RM_PMU_FBQ_CMD_Q_ELEMENT *)cmdmgmtOffsetToPtr(fbqHdr.heapOffset);

    status = ssurfaceRd(pFbqElement, entryOffset, fbqHdr.heapSize);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_queueFbCmdCopyIn_exit;
    }

    //
    // Pointer to normal CMD message is just past the FBQ HDR.
    // Return a pointer to the Falcon queue header with the payload behind it.
    //
    *ppHdr = &(pFbqElement->hdrs.oldCmd.hdr);

s_queueFbCmdCopyIn_exit:
    return status;
}

/*!
 * @brief   Write message to the queue located within falcon's FB super-surface.
 *
 * @note    Caller MUST obtain the pQD->mutex before calling this funciton.
 *
 * @param[in,out]   pHead   Current location within the queue.
 * @param[in]       pHdr    A pointer to the header of the message.
 * @param[in]       pBody   A pointer to the body of the message.
 * @param[in]       size    The number of bytes to write.
 *
 * @return      FLCN_OK                     Success.
 * @return      FLCN_ERR_ILWALID_ARGUMENT   If invalid size, pHead, or pHdr given.
 * @return      FLCN_ERR_TIMEOUT            Timeout on FB flush.
 * @return      FLCN_ERR_xxx                returned from dmaWrite or fbFlush.
 */
void
queueFbHeapQueueDataPost
(
    LwU32              *pHead,
    RM_FLCN_QUEUE_HDR  *pHdr,
    const void         *pBody,
    LwU32               size
)
{
    static LwU16        FbqMsgSequenceNumber = 0;
    RM_PMU_RPC_HEADER  *pRpcHdr = (RM_PMU_RPC_HEADER *)(QueueFbDmaWriteBuffer.bytes);
    FLCN_STATUS         status  = FLCN_OK;

    // Initial checks
    if ((size > RM_PMU_FBQ_MSG_ELEMENT_SIZE) ||
        (size < RM_FLCN_QUEUE_HDR_SIZE) ||
        (pHead == NULL) ||
        (pHdr == NULL) ||
        (pBody == NULL))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto osCmdmgmtFbqMsgCopyOut_exit;
    }

    // Copy HDR and possible Body to aligned buffer.
    (void)memcpy(&(QueueFbDmaWriteBuffer.oldHdr), pHdr, RM_FLCN_QUEUE_HDR_SIZE);
    (void)memcpy(QueueFbDmaWriteBuffer.bytes, pBody, size - RM_FLCN_QUEUE_HDR_SIZE);
    pRpcHdr->sequenceNumber = FbqMsgSequenceNumber++;
    pRpcHdr->sc.checksum = 0;
    pRpcHdr->size = size;

    // Callwlate checksum.
    pRpcHdr->sc.checksum -= queueFbChecksum16(&QueueFbDmaWriteBuffer, size);

    // Take DMA lock to prevent MSCG
    lwosDmaLockAcquire();
    {
        // DMA to FB the RM_FLCN_QUEUE_HDR.
        status = ssurfaceWr(&QueueFbDmaWriteBuffer,
            RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(fbq.msgQueue.qElement[*pHead]),
            size);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
        }
        else
        {
#ifdef UPROC_RISCV
            // Skip FB flush on fmodel as a WAR for issue in http://lwbugs/2767551
            if (!REG_FLD_TEST_DRF_DEF(BAR0, _PTOP, _PLATFORM, _TYPE, _FMODEL))
#endif // UPROC_RISCV
            {
                // Flush the write to FB
                (void)pmuFbFlush_HAL();
            }
        }
    }
    lwosDmaLockRelease();

osCmdmgmtFbqMsgCopyOut_exit:
    // Update Queue Position.
    if (status == FLCN_OK)
    {
        (*pHead)++;
        if (*pHead >= RM_PMU_FBQ_MSG_NUM_ELEMENTS)
        {
            *pHead = 0;
        }
    }
}

/*!
 * @brief   Callwlate pointer within RM managed dmem heap and
 *          return the pointer. On FBQ systems RM provides offset into
 *          heap. On non-FBQ systems, RM provides full offset into dmem
 *          which doesnt need to be modified.
 *
 * @param[in]       offset      LwU32 heap offset
 *
 * @return          Pointer to location in heap (32/64 bit)
 */
void *
cmdmgmtOffsetToPtr
(
    LwU32 offset
)
{
    return (QueueFbRmManagedHeap + offset);
}

void
queueFbHeapRpcRespond
(
    DISP2UNIT_RM_RPC   *pRequest,
    RM_FLCN_QUEUE_HDR  *pMsgHdr,
    const void         *pMsgBody
)
{
    RM_FLCN_FBQ_HDR *pFbqHdr = FBQ_GET_FBQ_HDR_FROM_QUEUE_HDR(&pRequest->pRpc->hdr);
    PMU_RM_RPC_STRUCT_CMDMGMT_RM_RPC_RESPONSE rpc;
    FLCN_STATUS status;

    //
    // If this is not true than we have corruption and sending RPC is useless.
    // Not adding breakpoint to avoid changes to allready shipped systems.
    //
    if (pRequest->cmdQueueId < RM_PMU_FBQ_CMD_COUNT)
    {
        s_queueFbCmdCopyOut(&pRequest->pRpc->hdr, pRequest->cmdQueueId);

        s_queueFbSweep(pRequest->cmdQueueId, pFbqHdr->elementIndex);
    }

    memset(&rpc, 0x00, sizeof(rpc));
    rpc.seqNumId = pRequest->pRpc->hdr.seqNumId;
    rpc.rpcFlags = pRequest->pRpc->cmd.rpc.flags;
    PMU_RM_RPC_EXELWTE_BLOCKING(status, CMDMGMT, RM_RPC_RESPONSE, &rpc);
}

/* ------------------------- Private Functions ------------------------------ */
static void
s_queueFbSweep
(
    LwU8 queueId,
    LwU32 index
)
{
    static LwU16 QueueFbHeapSweepMask[RM_PMU_FBQ_CMD_COUNT][1] = {{ 0U }, { 0U }};
#define QUEUE_FB_HEAP_SWEEP_MASK_BIT_SIZE           (sizeof(QueueFbHeapSweepMask[0]) * 8U)
#define QUEUE_FB_HEAP_SWEEP_MASK_ELEMENT_BIT_SIZE   (sizeof(QueueFbHeapSweepMask[0][0]) * 8U)
    ct_assert(QUEUE_FB_HEAP_SWEEP_MASK_BIT_SIZE >= RM_PMU_FBQ_CMD_NUM_ELEMENTS);

    appTaskCriticalEnter();
    {
        LwU32 tail;
        LwU32 head;

        // We cannot set the bit twice (break on future regression).
        if (LWOS_BM_GET(QueueFbHeapSweepMask[queueId], index, QUEUE_FB_HEAP_SWEEP_MASK_ELEMENT_BIT_SIZE))
        {
            PMU_BREAKPOINT();
            goto s_queueFbSweep_exit_critical;
        }

        // Mark queue entry as completed to include it in sweep attempt.
        LWOS_BM_SET(QueueFbHeapSweepMask[queueId], index, QUEUE_FB_HEAP_SWEEP_MASK_ELEMENT_BIT_SIZE);

        tail = pmuQueueRmToPmuTailGet_HAL(&Pmu, queueId);
        head = pmuQueueRmToPmuHeadGet_HAL(&Pmu, queueId);

        if ((head >= RM_PMU_FBQ_CMD_NUM_ELEMENTS) ||
            (tail >= RM_PMU_FBQ_CMD_NUM_ELEMENTS))
        {
            OS_BREAKPOINT();
            goto s_queueFbSweep_exit_critical;
        }

        //
        // Step from tail forward in the queue, to see how many conselwtive
        // entries can be made available.
        //
        while (tail != head)
        {
            if (!LWOS_BM_GET(QueueFbHeapSweepMask[queueId], tail, QUEUE_FB_HEAP_SWEEP_MASK_ELEMENT_BIT_SIZE))
            {
                // Not yet completed, abort further sweeping.
                break;
            }

            // Clear the state of swept queue entries.
            LWOS_BM_CLR(QueueFbHeapSweepMask[queueId], tail, QUEUE_FB_HEAP_SWEEP_MASK_ELEMENT_BIT_SIZE);

            tail++;
            if (tail >= RM_PMU_FBQ_CMD_NUM_ELEMENTS)
            {
                tail = 0;
            }
        }

        //
        // Tail pointer of CMD queue being write protected at priv level 2,
        // we will elevate to level 2 before updating and switch back
        // after completion.
        //
        OS_TASK_SET_PRIV_LEVEL_BEGIN(RM_FALC_PRIV_LEVEL_LS, RM_FALC_PRIV_LEVEL_LS)
        {
            // If found conselwtive free entries from the tail, then move it.
            pmuQueueRmToPmuTailSet_HAL(&Pmu, queueId, tail);
        }
        OS_TASK_SET_PRIV_LEVEL_END();

s_queueFbSweep_exit_critical:
        lwosNOP();
    }
    appTaskCriticalExit();
}
