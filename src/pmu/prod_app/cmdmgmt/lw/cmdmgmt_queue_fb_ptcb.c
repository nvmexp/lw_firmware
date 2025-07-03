/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    cmdmgmt_queue_fb_ptcb.c
 * @copydoc cmdmgmt_queue_fb_ptcb.h
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
#include "cmdmgmt/cmdmgmt_queue_fb_ptcb.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "lib/lib_nocat.h"

#include "dev_top.h" // Part of WAR for http://lwbugs/2767551

#include "g_pmurpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Compile Time Checks ---------------------------- */
// Assure that this file compiles only when required.
ct_assert(PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_PER_TASK_COMMAND_BUFFER));

/*!
 * @brief   Assure that the alignment restriction has not been broken.
 */
ct_assert(LW_IS_ALIGNED(RM_PMU_FBQ_CMD_ELEMENT_SIZE, DMA_MAX_READ_ALIGNMENT));

ct_assert(!PROFILE_RM_MANAGED_HEAP_SIZE_BLOCKS);

/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * @brief   Helper macro returning the super-surface's offset of queue element
 *
 * @param[in]   _queueId    The identifier of the queue
 * @param[in]   _queuePos   Queue position/element index
 */
#define QUEUE_FB_PTCB_SS_OFFSET(_queueId, _queuePos) \
    RM_PMU_SUPER_SURFACE_MEMBER_OFFSET( \
        fbq.cmdQueues.queue[RM_PMU_SUPER_SURFACE_FBQ_PTCB_INDEX_##_queueId].qElement[(_queuePos)])

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/*!
 * @brief   Copies in the RPC header for a given FbQueue at a given position
 *          into an existing buffer.
 *
 * @note    Applicable only to FB queues using PTCB (per-task command buffer).
 *
 * @param[out]  pRpcHdr     Command header into which to copy
 * @param[in]   queuePos    Queue position/element index from which to copy
 *
 * @return  FLCN_OK                     Success
 * @return  FLCN_ERR_ILWALID_ARGUMENT   Provided arguments invalid
 * @return  Others                      Errors propagated from callees
 */
FLCN_STATUS s_queueFbPtcbRpcCopyInHdr(RM_PMU_RPC_HEADER *pRpcHdr, LwU32 queuePos)
    GCC_ATTRIB_SECTION("imem_cmdmgmt", "s_queueFbPtcbRpcCopyInHdr");

/*!
 * @brief   Look-up the size of the incoming RPC request.
 *
 * @note    Previously we relied on the value sent by the RM.
 *          Now we have one less input parameter to validate.
 *
 * @param[in]   pRpcHdr     Header of the incoming RPC request.
 *
 * @return  RPC size in bytes
 */
static LwU16 s_queueFbPtcbRpcSizeGet(RM_PMU_RPC_HEADER *pRpcHdr)
    GCC_ATTRIB_SECTION("imem_cmdmgmt", "s_queueFbPtcbRpcSizeGet");

/*!
 * @brief   Look-up whether the function for sweeping the element
 *          should be skipped.
 *
 * @param[in]   pRpcHdr     Header of the incoming RPC request.
 *
 * @return  Boolean indicating whether function sweep() should
 *          be skipped.
 */
static LwBool s_queueFbqPrcbRpcSweepSkipGet(RM_PMU_RPC_HEADER *pRpcHdr)
    GCC_ATTRIB_SECTION("imem_cmdmgmt", "s_queueFbqPrcbRpcSweepSkipGet");

/*!
 * @brief   Attempt to move queue tail pointer after fetching queue entry.
 *
 * @param[in]   index   Queue index of the completed entry.
 */
static void s_queueFbPtcbSweep(LwU32 index)
    GCC_ATTRIB_SECTION("imem_resident", "s_queueFbPtcbSweep");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc queueFbPtcbInitRpcPopulate
 */
void
queueFbPtcbInitRpcPopulate
(
    PMU_RM_RPC_STRUCT_CMDMGMT_INIT *pRpc
)
{
    // Populate pointer to PMU NOCAT diagnostic data buffer.
    pRpc->bufferStart = nocatBufferDmemPhysOffsetGet();

    // With per-task command buffers RM managed heap is no longer used.
    pRpc->rmManagedAreaOffset = 0U;
    pRpc->rmManagedAreaSize   = 0U;
}

/*!
 * @copydoc queueFbPtcbProcessCmdQueue
 */
FLCN_STATUS
queueFbPtcbProcessCmdQueue
(
    LwU32 headId
)
{
    static LwU32 QueueFbPtcbQueuePtrLwrrent = 0U;
    FLCN_STATUS status = FLCN_OK;
    LwU32 head = pmuQueueRmToPmuHeadGet_HAL(&Pmu, headId);

    if (headId != RM_PMU_FBQ_PTCB_CMD_QUEUE_ID)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        PMU_BREAKPOINT();
        goto queueFbPtcbProcessCmdQueue_exit;
    }

    while (QueueFbPtcbQueuePtrLwrrent != head)
    {
        DISP2UNIT_RM_RPC    rpcDisp;
        RM_PMU_RPC_HEADER   rpcHdr;

        // Take DMA lock before DMA in.
        lwosDmaLockAcquire();

        DISP2UNIT_RM_RPC_FBQ_PTCB *const pFbqPtcb =
            disp2unitCmdFbqPtcbGet(&rpcDisp);
        if (pFbqPtcb == NULL)
        {
            lwosDmaLockRelease();
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            break;
        }

        status = s_queueFbPtcbRpcCopyInHdr(&rpcHdr, QueueFbPtcbQueuePtrLwrrent);
        if (status != FLCN_OK)
        {
            lwosDmaLockRelease();
            PMU_BREAKPOINT();
            break;
        }

        pFbqPtcb->elementIndex = QueueFbPtcbQueuePtrLwrrent;
        pFbqPtcb->elementSize  = s_queueFbPtcbRpcSizeGet(&rpcHdr);
        pFbqPtcb->bSweepSkip   = s_queueFbqPrcbRpcSweepSkipGet(&rpcHdr);

        if (!QUEUE_FB_IS_RPC_TARGET_LPWR(rpcHdr.unitId))
        {
            lwosDmaLockRelease();
        }

        QueueFbPtcbQueuePtrLwrrent++;
        if (QueueFbPtcbQueuePtrLwrrent >= RM_PMU_FBQ_CMD_NUM_ELEMENTS)
        {
            QueueFbPtcbQueuePtrLwrrent = 0;
        }

        // Build dispatching structure.
        rpcDisp.hdr.eventType = DISP2UNIT_EVT_RM_RPC;
        rpcDisp.hdr.unitId    = rpcHdr.unitId;
        rpcDisp.pRpc          = NULL;

        // Dispatch command to the handling task.
        status = cmdmgmtExtCmdDispatch(&rpcDisp);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            break;
        }

        if (QUEUE_FB_IS_RPC_TARGET_LPWR(rpcHdr.unitId))
        {
            lwosDmaLockRelease();
        }
    }

queueFbPtcbProcessCmdQueue_exit:
    return status;
}

/*!
 * @copydoc s_queueFbPtcbRpcCopyInHdr
 */
FLCN_STATUS
s_queueFbPtcbRpcCopyInHdr
(
    RM_PMU_RPC_HEADER  *pRpcHdr,
    LwU32               queuePos
)
{
    FLCN_STATUS status;
    LwU8 data[(sizeof(RM_PMU_RPC_HEADER) * 2) - 1U];
    RM_PMU_RPC_HEADER *const pData = PMU_ALIGN_UP_PTR(data, sizeof(*pData));

    // Check parameters
    PMU_HALT_OK_OR_GOTO(status,
        (pRpcHdr != NULL) &&
        (queuePos < RM_PMU_FBQ_CMD_NUM_ELEMENTS) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
        s_queueFbPtcbRpcCopyInHdr_exit);

    // Read in the data.
    PMU_HALT_OK_OR_GOTO(status,
        ssurfaceRd(pData, QUEUE_FB_PTCB_SS_OFFSET(CMD, queuePos), sizeof(*pData)),
        s_queueFbPtcbRpcCopyInHdr_exit);

    // Copy out the data
    *pRpcHdr = *pData;

s_queueFbPtcbRpcCopyInHdr_exit:
    return status;
}

/*!
 * @copydoc queueFbPtcbRpcCopyIn
 */
FLCN_STATUS
queueFbPtcbRpcCopyIn
(
    void   *pBuffer,
    LwU32   bufferMax,
    LwU32   queuePos,
    LwU16   elementSize,
    LwBool  bSweepSkip
)
{
    RM_PMU_RPC_HEADER *pRpcHdr = (RM_PMU_RPC_HEADER *)pBuffer;
    FLCN_STATUS status;
    LwU16 transferSize;

    // Check parameters
    PMU_HALT_OK_OR_GOTO(status,
        (pBuffer != NULL) &&
        (bufferMax >= elementSize) &&
        (bufferMax >= RM_PMU_FBQ_CMD_ELEMENT_SIZE) &&
        LW_IS_ALIGNED(bufferMax, DMA_MAX_READ_ALIGNMENT) &&
        (queuePos < RM_PMU_FBQ_CMD_NUM_ELEMENTS) &&
        (elementSize <= RM_PMU_FBQ_CMD_ELEMENT_SIZE) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
        queueFbPtcbRpcCopyIn_exit);

    //
    // DMA is most efficient if transfering DMA_MAX_READ_ALIGNMENT blocks.
    // This is safe since (elementSize <= RM_PMU_FBQ_CMD_ELEMENT_SIZE) and
    // the RM_PMU_FBQ_CMD_ELEMENT_SIZE is already aligned (ct_assert() above).
    //
    transferSize = LW_ALIGN_UP(elementSize, DMA_MAX_READ_ALIGNMENT);

    // Read the data directly into the provided buffer
    PMU_HALT_OK_OR_GOTO(status,
        ssurfaceRd(pBuffer, QUEUE_FB_PTCB_SS_OFFSET(CMD, queuePos), transferSize),
        queueFbPtcbRpcCopyIn_exit);

    // Capture the size of the RPC to be used during response.
    pRpcHdr->sc.rpcSize = elementSize;

    //
    // If current task should be the one processing the request, mark queue
    // element as fetched.
    //
    if (!bSweepSkip)
    {
       s_queueFbPtcbSweep(queuePos);
    }

queueFbPtcbRpcCopyIn_exit:
    return status;
}

/*!
 * @copydoc queueFbPtcbQueueDataPost
 */
void
queueFbPtcbQueueDataPost
(
    LwU32              *pHead,
    RM_FLCN_QUEUE_HDR  *pHdr,
    const void         *pBody,
    LwU32               size
)
{
    static LwU16        QueueFbPtcbMsgSequenceNumber = 0U;
    RM_PMU_RPC_HEADER  *pRpcHdr = (RM_PMU_RPC_HEADER *)pBody;
    FLCN_STATUS         status  = FLCN_OK;
    LwU32               dmaSize;

    if (FLD_TEST_DRF(_RM_PMU, _RPC_FLAGS, _PMU_RESPONSE, _NO, pRpcHdr->flags))
    {
        // Initial checks
        if ((size > RM_PMU_FBQ_MSG_ELEMENT_SIZE) ||
            (size < RM_FLCN_QUEUE_HDR_SIZE) ||
            (pHead == NULL) ||
            (pBody == NULL))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto queueFbPtcbQueueDataPost_exit;
        }

        // Remove Falcon queue header used on pre_PTCB implementations.
        size -= RM_FLCN_QUEUE_HDR_SIZE;

        // DMA size equals to the size of the PMU->RM RPC.
        dmaSize = size;
    }
    else
    {
        // We do not need FLCN queue header.
        size -= RM_FLCN_QUEUE_HDR_SIZE;

        // Initial checks
        if ((size != sizeof(RM_PMU_RPC_HEADER)) ||
            (pHead == NULL) ||
            (pBody == NULL))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto queueFbPtcbQueueDataPost_exit;
        }

        //
        // When responding to the RM request DMA out entire RPC while perform
        // checksum only on the header.
        //
        dmaSize = pRpcHdr->sc.rpcSize;
    }

    // Update the RPC header with flow control data.
    pRpcHdr->sequenceNumber = QueueFbPtcbMsgSequenceNumber++;
    pRpcHdr->size           = size;
    pRpcHdr->sc.checksum    = 0;
    pRpcHdr->sc.checksum   -= queueFbChecksum16(pRpcHdr, size);

    // Take DMA lock to prevent MSCG
    lwosDmaLockAcquire();
    {
        status = ssurfaceWr(pRpcHdr, QUEUE_FB_PTCB_SS_OFFSET(MSG, *pHead), dmaSize);
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

queueFbPtcbQueueDataPost_exit:
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
 * @copydoc queueFbPtcbRpcRespond
 */
void
queueFbPtcbRpcRespond
(
    DISP2UNIT_RM_RPC *pRequest
)
{
    RM_PMU_RPC_HEADER *pRpcHdr = (RM_PMU_RPC_HEADER *)(pRequest->pRpc);

    pRpcHdr->flags =
        FLD_SET_DRF(_RM_PMU, _RPC_FLAGS, _PMU_RESPONSE, _YES, pRpcHdr->flags);

    (void)pmuRmRpcExelwte_IMPL(pRpcHdr, sizeof(RM_PMU_RPC_HEADER), LW_TRUE);
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @copydoc s_queueFbPtcbRpcSizeGet
 */
static LwU16
s_queueFbPtcbRpcSizeGet
(
    RM_PMU_RPC_HEADER *pRpcHdr
)
{
    FLCN_STATUS status  = FLCN_ERR_ILWALID_ARGUMENT;
    LwU16       rpcSize = 0U;

    switch (pRpcHdr->unitId)
    {
        case RM_PMU_UNIT_I2C:
            if (OSTASK_ENABLED(I2C))
            {
                status = pmuRpcSizeGetUnitI2c(pRpcHdr->function, &rpcSize);
            }
            break;
        case RM_PMU_UNIT_LPWR:
            if (OSTASK_ENABLED(LPWR))
            {
                status = pmuRpcSizeGetUnitLpwr(pRpcHdr->function, &rpcSize);
            }
            break;
        case RM_PMU_UNIT_LOGGER:
            if (PMUCFG_FEATURE_ENABLED(PMU_LOGGER))
            {
                status = pmuRpcSizeGetUnitLogger(pRpcHdr->function, &rpcSize);
            }
            break;
        case RM_PMU_UNIT_LPWR_LOADIN:
            if (OSTASK_ENABLED(LPWR))
            {
                status = pmuRpcSizeGetUnitLpwrLoadin(pRpcHdr->function, &rpcSize);
            }
            break;
        case RM_PMU_UNIT_SMBPBI:
            if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI) && OSTASK_ENABLED(THERM))
            {
                status = pmuRpcSizeGetUnitSmbpbi(pRpcHdr->function, &rpcSize);
            }
            break;
        case RM_PMU_UNIT_ACR:
            if (OSTASK_ENABLED(ACR))
            {
                status = pmuRpcSizeGetUnitAcr(pRpcHdr->function, &rpcSize);
            }
            break;
        case RM_PMU_UNIT_SPI:
            if (OSTASK_ENABLED(SPI))
            {
                status = pmuRpcSizeGetUnitSpi(pRpcHdr->function, &rpcSize);
            }
            break;
        case RM_PMU_UNIT_CLK:
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_IN_PERF) && OSTASK_ENABLED(PERF))
            {
                status = pmuRpcSizeGetUnitClk(pRpcHdr->function, &rpcSize);
            }
            break;
        case RM_PMU_UNIT_VOLT:
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_IN_PERF) && OSTASK_ENABLED(PERF))
            {
                status = pmuRpcSizeGetUnitVolt(pRpcHdr->function, &rpcSize);
            }
            break;
        case RM_PMU_UNIT_CMDMGMT:
            status = pmuRpcSizeGetUnitCmdmgmt(pRpcHdr->function, &rpcSize);
            break;
        case RM_PMU_UNIT_PERFMON:
            if (OSTASK_ENABLED(PERFMON))
            {
                status = pmuRpcSizeGetUnitPerfmon(pRpcHdr->function, &rpcSize);
            }
            break;
        case RM_PMU_UNIT_FAN:
            if (PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN) && OSTASK_ENABLED(THERM))
            {
                status = pmuRpcSizeGetUnitFan(pRpcHdr->function, &rpcSize);
            }
            break;
        case RM_PMU_UNIT_PERF:
            if (OSTASK_ENABLED(PERF))
            {
                status = pmuRpcSizeGetUnitPerf(pRpcHdr->function, &rpcSize);
            }
            break;
        case RM_PMU_UNIT_THERM:
            if (OSTASK_ENABLED(THERM))
            {
                status = pmuRpcSizeGetUnitTherm(pRpcHdr->function, &rpcSize);
            }
            break;
        case RM_PMU_UNIT_DISP:
            if (OSTASK_ENABLED(DISP))
            {
                status = pmuRpcSizeGetUnitDisp(pRpcHdr->function, &rpcSize);
            }
            break;
        case RM_PMU_UNIT_PMGR:
            if (OSTASK_ENABLED(PMGR))
            {
                status = pmuRpcSizeGetUnitPmgr(pRpcHdr->function, &rpcSize);
            }
            break;
        case RM_PMU_UNIT_CLK_FREQ:
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_IN_PERF) && OSTASK_ENABLED(PERF))
            {
                status = pmuRpcSizeGetUnitClkFreq(pRpcHdr->function, &rpcSize);
            }
            break;
        case RM_PMU_UNIT_BIF:
            if (OSTASK_ENABLED(BIF))
            {
                status = pmuRpcSizeGetUnitBif(pRpcHdr->function, &rpcSize);
            }
            break;
        case RM_PMU_UNIT_PERF_CF:
            if (OSTASK_ENABLED(PERF_CF))
            {
                status = pmuRpcSizeGetUnitPerfCf(pRpcHdr->function, &rpcSize);
            }
            break;
        case RM_PMU_UNIT_LPWR_LP:
            if (OSTASK_ENABLED(LPWR_LP))
            {
                status = pmuRpcSizeGetUnitLpwrLp(pRpcHdr->function, &rpcSize);
            }
            break;
        case RM_PMU_UNIT_NNE:
            if (OSTASK_ENABLED(NNE))
            {
                status = pmuRpcSizeGetUnitNne(pRpcHdr->function, &rpcSize);
            }
            break;
        default:
            status = FLCN_ERR_ILWALID_ARGUMENT;
            break;
    }

    if (status != FLCN_OK)
    {
        rpcSize = 0U;
    }

    return rpcSize;
}

/*!
 * @copydoc s_queueFbqPrcbRpcSweepSkipGet
 */
static LwBool
s_queueFbqPrcbRpcSweepSkipGet
(
    RM_PMU_RPC_HEADER *pRpcHdr
)
{
    // Initialize to default value
    LwBool bSweepSkip = LW_FALSE;

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_IN_PERF_DAEMON_PASS_THROUGH_PERF) &&
       (pRpcHdr->unitId == RM_PMU_UNIT_CLK_FREQ))
    {
        // Skipping sweep() of queue TAIL pointer since this RPC request can be forwarded
        bSweepSkip = LW_TRUE;
    }

    return bSweepSkip;
}

/*!
 * @copydoc s_queueFbPtcbSweep
 */
static void
s_queueFbPtcbSweep
(
    LwU32 index
)
{
    static LwU16 QueueFbPtcbSweepMask[1] = { 0U };
#define QUEUE_FB_PTCB_MASK_BIT_SIZE         (sizeof(QueueFbPtcbSweepMask) * 8U)
#define QUEUE_FB_PTCB_MASK_ELEMENT_BIT_SIZE (sizeof(QueueFbPtcbSweepMask[0]) * 8U)
    ct_assert(QUEUE_FB_PTCB_MASK_BIT_SIZE >= RM_PMU_FBQ_CMD_NUM_ELEMENTS);

    if (index >= RM_PMU_FBQ_CMD_NUM_ELEMENTS)
    {
        PMU_BREAKPOINT();
        return;
    }

    appTaskCriticalEnter();
    {
        LwU32 tail;
        LwU32 head;

        // We cannot set the bit twice (break on future regression).
        if (LWOS_BM_GET(QueueFbPtcbSweepMask, index, QUEUE_FB_PTCB_MASK_ELEMENT_BIT_SIZE))
        {
            PMU_BREAKPOINT();
            goto s_queueFbPtcbSweep_exit_critical;
        }

        // Mark queue entry as completed to include it in sweep attempt.
        LWOS_BM_SET(QueueFbPtcbSweepMask, index, QUEUE_FB_PTCB_MASK_ELEMENT_BIT_SIZE);

        tail = pmuQueueRmToPmuTailGet_HAL(&Pmu, RM_PMU_FBQ_PTCB_CMD_QUEUE_ID);
        head = pmuQueueRmToPmuHeadGet_HAL(&Pmu, RM_PMU_FBQ_PTCB_CMD_QUEUE_ID);

        if ((head >= RM_PMU_FBQ_CMD_NUM_ELEMENTS) ||
            (tail >= RM_PMU_FBQ_CMD_NUM_ELEMENTS))
        {
            PMU_BREAKPOINT();
            goto s_queueFbPtcbSweep_exit_critical;
        }

        //
        // Step from tail forward in the queue, to see how many conselwtive
        // entries can be made available.
        //
        while (tail != head)
        {
            if (!LWOS_BM_GET(QueueFbPtcbSweepMask, tail, QUEUE_FB_PTCB_MASK_ELEMENT_BIT_SIZE))
            {
                // Not yet completed, abort further sweeping.
                break;
            }

            // Clear the state of swept queue entries.
            LWOS_BM_CLR(QueueFbPtcbSweepMask, tail, QUEUE_FB_PTCB_MASK_ELEMENT_BIT_SIZE);

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
            pmuQueueRmToPmuTailSet_HAL(&Pmu, RM_PMU_FBQ_PTCB_CMD_QUEUE_ID, tail);
        }
        OS_TASK_SET_PRIV_LEVEL_END();

s_queueFbPtcbSweep_exit_critical:
        lwosNOP();
    }
    appTaskCriticalExit();
}
