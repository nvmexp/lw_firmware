/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    lwos_instrumentation.c
 * @brief   LWOS instrumentation interfaces.
 */


/* ------------------------ System Includes --------------------------------- */
#include "lwos_instrumentation.h"
#include "lwos_utility.h"
#include "lwoslayer.h"
#include "lwos_dma.h"
#include "lwmisc.h"
#include "lwrtos.h"
#include "osptimer.h"
#include "lib_lw64.h"
#include "pmu_oslayer.h"
#ifdef UPROC_RISCV
#include "drivers/drivers.h"
#include <riscv_csr.h>
#endif // UPROC_RISCV
#include "lwrtosHooks.h"

/* ------------------------ External Definitions ---------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
//! RESIDENT_QUEUE_SIZE_BYTES must be a power of two greater than 256
#define LWOS_INSTR_RESIDENT_QUEUE_SIZE_BLOCKS   4U
#define LWOS_INSTR_RESIDENT_QUEUE_SIZE_BYTES    \
    (LWOS_INSTR_RESIDENT_QUEUE_SIZE_BLOCKS * DMEM_BLOCK_SIZE)
#define LWOS_INSTR_RESIDENT_QUEUE_SIZE          \
    (LWOS_INSTR_RESIDENT_QUEUE_SIZE_BYTES / sizeof(LWOS_INSTRUMENTATION_EVENT))

ct_assert((LWOS_INSTR_RESIDENT_QUEUE_SIZE_BYTES &
           (LWOS_INSTR_RESIDENT_QUEUE_SIZE_BYTES - 1U)) == 0U);
ct_assert(LWOS_INSTR_RESIDENT_QUEUE_SIZE_BYTES >= 256U);

/*!
 * LWOS_INSTR_RESIDENT_QUEUE_FLUSH_SIZE_BYTES must be a power of two less than
 * or equal to 256 because of DMA restrictions. It is set to 128 bytes to allow
 * the queue size to be as small as 256 bytes (the queue should not need to be
 * full before it is flushed.
 */
#define LWOS_INSTR_RESIDENT_QUEUE_FLUSH_SIZE_BYTES \
    (LWOS_INSTR_RESIDENT_QUEUE_SIZE_BYTES / 2)

#define LWOS_INSTR_RESIDENT_QUEUE_FLUSH_SIZE       \
    (LWOS_INSTR_RESIDENT_QUEUE_FLUSH_SIZE_BYTES /  \
     sizeof(LWOS_INSTRUMENTATION_EVENT))

//! Callwlate a valid in-range index into the resident queue
#define LWOS_INSTR_COERCE_RESIDENT_INDEX(index)     \
    ((index) % LWOS_INSTR_RESIDENT_QUEUE_SIZE)

//! The maximum number of events that can be recorded in one "skipped" event
#define LWOS_INSTR_MAX_SKIPPED_EVENTS               255U

/*!
 * The maximum dt that can be stored in a single message. In the case that
 * this would be overflowed a recalibration event is inserted.
 */
#define LWOS_INSTR_DTIME_MAX                        \
    (LWBIT(DRF_SIZE(LW_INSTRUMENTATION_EVENT_DATA_TIME_DELTA)) - 1U)

// The timer resolution is 1 << 5 = 32 nanoseconds.
#define LWOS_INSTR_TIMER_RESOLUTION_BITS            5U

/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Public Variables -------------------------------- */
/* ------------------------ Static Variables -------------------------------- */
static volatile LwU64 LwosInstrumentationLastTimestamp = 0U;

/*!
 * The count of missed events since the last time we flushed. Each time we flush,
 * a new "skipped" event will be added if this is nonzero, and then it will be
 * reset to zero.
 */
static volatile LwU16 LwosInstrumentationMissedEvents = 0U;

/*!
 * A resident-memory cirlwlar buffer used to stage events.
 *
 * Because this must be written to in multiple locations, writes to any of these
 * variables must occur in a critical section.
 *
 * Writing code must:
 *  - Enter a critical section (if interrupts enabled)
 *  - Check that LwosInstrumentationResidentQueueSize is not RESIDENT_QUEUE_SIZE
 *      (exit otherwise)
 *  - Increment LwosInstrumentationResidentQueueSize and
 *      LwosInstrumentationResidentQueueHead
 *  - Write the data
 *  - Exit the critical section (if interrupts enabled)
 *
 * Only one flush can occur at a time. The flushing code must:
 *  - Do a DMA transfer from the section of the queue protected
 *    by LwosInstrumentationResidentQueueFlushBlock and
 *      LwosInstrumentationResidentQueueFlushSize
 *  - Enter a critical section
 *  - Decrease LwosInstrumentationResidentQueueSize by the correct amount
 *      (this is not atomic)
 *  - Exit the critical section
 */
static volatile LWOS_INSTRUMENTATION_EVENT
    LwosInstrumentationResidentQueue[LWOS_INSTR_RESIDENT_QUEUE_SIZE]
    GCC_ATTRIB_ALIGN(DMEM_BLOCK_SIZE)
    GCC_ATTRIB_SECTION("alignedData256",
                       "LwosInstrumentationResidentQueue");

static volatile LwU16 LwosInstrumentationResidentQueueSize = 0U;
static volatile LwU16 LwosInstrumentationResidentQueueHead = 0U;

/*!
 * A surface in FB that we can flush to, or null if instrumentation hasn't been
 * initialized by the RM yet.
 */
static RM_FLCN_MEM_DESC LwosInstrumentationFbQueueMemDesc;
static LwU32 LwosInstrumentationFbQueueCapacity = 0U;
static LwU32 LwosInstrumentationFbQueueHead = 0U;

#define INSTRUMENTATION_ENTRIES(_eidx, _es)                                    \
    (                                                                          \
    LWOS_BM_INIT(LW2080_CTRL_FLCN_LWOS_INST_EVT_RECALIBRATE, (_eidx), (_es)) | \
    LWOS_BM_INIT(LW2080_CTRL_FLCN_LWOS_INST_EVT_CTXSW_TICK,  (_eidx), (_es)) | \
    LWOS_BM_INIT(LW2080_CTRL_FLCN_LWOS_INST_EVT_CTXSW_YIELD, (_eidx), (_es)) | \
    LWOS_BM_INIT(LW2080_CTRL_FLCN_LWOS_INST_EVT_CTXSW_INT0,  (_eidx), (_es)) | \
    LWOS_BM_INIT(LW2080_CTRL_FLCN_LWOS_INST_EVT_CTXSW_BLOCK, (_eidx), (_es)) | \
    LWOS_BM_INIT(LW2080_CTRL_FLCN_LWOS_INST_EVT_SKIPPED,     (_eidx), (_es))   \
    )

/*!
 * A mask of which events to log. Each bit corresponds to whether one event
 * type is enabled or disabled.
 *
 * Has size LW2080_CTRL_FLCN_LWOS_INST_MASK_SIZE_BYTES.
 */
LwU8 LwosInstrumentationEventMask[LW2080_CTRL_FLCN_LWOS_INST_MASK_SIZE_BYTES] =
{
    INSTRUMENTATION_ENTRIES(0, 8),
    INSTRUMENTATION_ENTRIES(1, 8),
    INSTRUMENTATION_ENTRIES(2, 8),
    INSTRUMENTATION_ENTRIES(3, 8)
};

/* ------------------------ Static Function Prototypes ---------------------- */
static LW_FORCEINLINE FLCN_STATUS s_lwosInstrumentationWriteQueueMeta(
                                        LwU32 capacity, LwU32 head,
                                        RM_FLCN_MEM_DESC *pQueueMemDesc)
    GCC_ATTRIB_SECTION("imem_instRuntime", "s_lwosInstrumentationWriteQueueMeta");

static LW_FORCEINLINE FLCN_STATUS s_lwosInstrumentationWriteQueueHead(
                                        LwU32 *pHead,
                                        RM_FLCN_MEM_DESC *pQueueMemDesc)
    GCC_ATTRIB_SECTION("imem_instRuntime", "s_lwosInstrumentationWriteQueueHead");

static FLCN_STATUS s_lwosInstrumentationFlushBlock(PLWOS_INSTRUMENTATION_EVENT data,
                                                   LwU32 size,
                                                   RM_FLCN_MEM_DESC *pQueueMemDesc)
    GCC_ATTRIB_SECTION("imem_instRuntime", "s_lwosInstrumentationFlushBlock");

static FLCN_STATUS s_lwosInstrumentationLogStamped(LwU8 eventType, LwU8 trackingId,
                                                   LwBool bNeedsCritical)
    GCC_ATTRIB_SECTION("imem_instRuntime", "s_lwosInstrumentationLogStamped");

static FLCN_STATUS s_lwosInstrumentationLog(PLWOS_INSTRUMENTATION_EVENT event)
    GCC_ATTRIB_SECTION("imem_instRuntime", "s_lwosInstrumentationLog");

static LwU64       s_lwosInstrumentationGetTimestampReduced(void)
    GCC_ATTRIB_SECTION("imem_instRuntime", "s_lwosInstrumentationGetTimestampReduced");

static LwU32       s_lwosInstrumentationGetFlushBeginIndex(void)
     GCC_ATTRIB_SECTION("imem_instRuntime", "s_lwosInstrumentationGetFlushBeginIndex")
     GCC_ATTRIB_NOINLINE();

static FLCN_STATUS s_lwosInstrumentationRecalibrateISR(LwU64 timestamp)
    GCC_ATTRIB_SECTION("imem_instRuntime", "s_lwosInstrumentationRecalibrate");

/* ------------------------ Public Functions -------------------------------- */

/*!
 * @brief   Enable instrumentation flushing.
 *
 * Prior to a call to this function, no instrumentation data will be flushed.
 * If instrumentation was already enabled, it will cease to flush data
 * to the original buffer and will only flush to the new one.
 *
 * Call this functions before any instrumentation calls occur.
 *
 * @param[in]   pQueueMem
 *      A memory descriptor to the beginning of the queue block. The first block
 *      is used for metadata; the rest is used for queued events. Should have a
 *      size of 2^N+1 blocks.
 * @return  FLCN_OK    - the flush target was successfully set and
 *              instrumentation flushing is now enabled.
 * @return  FLCN_ERROR - the flush target was not successfully set because it
 *              was null, too small, or misaligned.
 */
FLCN_STATUS
lwosInstrumentationBegin
(
    RM_FLCN_MEM_DESC *pQueueMem
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       memDescSize;
    LwU32       fbQueueSizeBytes;

    memDescSize = DRF_VAL(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE, pQueueMem->params);

    //
    // If the new memory descriptor is null or too small or misaligned,
    // then flushing should not begin. Error out.
    //
    if (LwU64_ALIGN32_IS_ZERO(&(pQueueMem->address)) ||
        !LW_IS_ALIGNED(pQueueMem->address.lo, DMEM_BLOCK_SIZE) ||
        (memDescSize < DMEM_BLOCK_SIZE))
    {
        status = FLCN_ERROR;
        OS_BREAKPOINT();
    }
    else
    {
        lwrtosENTER_CRITICAL();
        {
            LwosInstrumentationFbQueueMemDesc = *pQueueMem;

            // The first block is dedicated to metadata.
            fbQueueSizeBytes = memDescSize - DMEM_BLOCK_SIZE;

            LwosInstrumentationFbQueueCapacity =
                fbQueueSizeBytes / sizeof(LWOS_INSTRUMENTATION_EVENT);
            LwosInstrumentationFbQueueHead = 0;

            LwosInstrumentationResidentQueueSize = 0;
            LwosInstrumentationResidentQueueHead = 0;
        }
        lwrtosEXIT_CRITICAL();

        //
        // Write the metadata. This doesn't have to be in a critical section -
        // in the worst case, if this generates a race condition with flushing
        // code, this will temporarily incorrectly zero out the queue size and
        // head. Readers should be able to account for zero size, so this is
        // acceptable, and no messages will be lost because the size/head will
        // be restored during the next flush. In addition, this case can only
        // ever happen during instrumentation begin.
        //
        status = s_lwosInstrumentationWriteQueueMeta(
            LwosInstrumentationFbQueueCapacity,
            LwosInstrumentationFbQueueHead,
            pQueueMem);
    }

    return status;
}

/*!
 * @brief   Disable instrumentation flushing.
 *
 * After a call to this function, no instrumentation data will be flushed.
 * If instrumentation flushing was already disabled, this function will return
 * a FLCN_ERROR and do nothing.
 *
 * @return  FLCN_OK    - instrumentation was successfully ended.
 * @return  FLCN_ERROR - instrumentation was not already running.
 */
FLCN_STATUS
lwosInstrumentationEnd(void)
{
    FLCN_STATUS status = FLCN_OK;

    lwrtosENTER_CRITICAL();
    {
        // Access to RM_FLCN_MEM_DESC is non-atomic
        if (LwU64_ALIGN32_IS_ZERO(&LwosInstrumentationFbQueueMemDesc.address))
        {
            status = FLCN_ERROR;
            OS_BREAKPOINT();
        }
        else
        {
            LwosInstrumentationFbQueueMemDesc.address.hi = 0;
            LwosInstrumentationFbQueueMemDesc.address.lo = 0;
            LwosInstrumentationFbQueueMemDesc.params = 0;
        }
    }
    lwrtosEXIT_CRITICAL();

    return status;
}

/*!
 * @copydoc s_lwosInstrumentationLogStamped
 */
inline FLCN_STATUS
lwosInstrumentationLog
(
    LwU8 eventType,
    LwU8 trackingId
)
{
    // Require critical sections because interrupts are enabled.
    return s_lwosInstrumentationLogStamped(eventType, trackingId, LW_TRUE);
}

/*!
 * @copydoc s_lwosInstrumentationLogStamped
 */
inline FLCN_STATUS
lwosInstrumentationLogFromISR
(
    LwU8 eventType,
    LwU8 trackingId
)
{
    // No critical section required, because interrupts are already disabled.
    return s_lwosInstrumentationLogStamped(eventType, trackingId, LW_FALSE);
}

/*!
 * @brief   Flush some of the resident queue out to FB.
 *
 * Flushes 128 bytes (1/2 block) at a time to maintain DMA alignment. This
 * function will also insert EVT_SKIPPED events if any events were missed
 * because of an overfilled queue.
 *
 * @note This function contains blocking DMA and should not be called from
 * within a critical section. It should not be called while DMA is suspended.
 *
 * @return  FLCN_OK    - the flush completed successfully.
 * @return  FLCN_ERROR - flushing is lwrrently disabled or the flush failed
 *              for some other reason.
 */
FLCN_STATUS
lwosInstrumentationFlush(void)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       flushBeginIndex;
    LwU32       fbqHeadPrevVal;

    lwrtosENTER_CRITICAL();
    {
        fbqHeadPrevVal = LwosInstrumentationFbQueueHead;

        if (lwosDmaIsSuspended())
        {
            goto lwosInstrumentationFlush_exit;
        }

        while (LwosInstrumentationResidentQueueSize >
               LWOS_INSTR_RESIDENT_QUEUE_FLUSH_SIZE)
        {
            // Please keep as non-inlined helper to conserve stack size.
            flushBeginIndex = s_lwosInstrumentationGetFlushBeginIndex();

            //
            // Flush to FB.
            // Casting away volatile on LwosInstrumentationResidentQueue is safe
            // because nothing else should be touching that section of it right now.
            //
            status = s_lwosInstrumentationFlushBlock(
                (void *)&LwosInstrumentationResidentQueue[flushBeginIndex],
                LWOS_INSTR_RESIDENT_QUEUE_FLUSH_SIZE_BYTES,
                &LwosInstrumentationFbQueueMemDesc);
            if (status != FLCN_OK)
            {
                // If flushing failed, then the queue hasn't changed. Exit early.
                goto lwosInstrumentationFlush_exit;
            }

            LwosInstrumentationResidentQueueSize -=
                LWOS_INSTR_RESIDENT_QUEUE_FLUSH_SIZE;

            // Catch up on missed events by logging that we've skipped some.
            while ((LwosInstrumentationMissedEvents > 0) &&
                   (LwosInstrumentationResidentQueueSize <
                    LWOS_INSTR_RESIDENT_QUEUE_SIZE))
            {
                LwU32 numMissed = LwosInstrumentationMissedEvents;

                //
                // Don't log more than the maximum number of skipped events in
                // one event. If there are more than MAX_SKIPPED_EVENTS, we can
                // log multiple messages.
                //
                if (numMissed > LWOS_INSTR_MAX_SKIPPED_EVENTS)
                {
                    numMissed = LWOS_INSTR_MAX_SKIPPED_EVENTS;
                }

                LwosInstrumentationMissedEvents -= numMissed;

                //
                // We're already in a critical section, so we don't need to
                // disable interrupts again.
                //
                lwosInstrumentationLogFromISR(
                    LW2080_CTRL_FLCN_LWOS_INST_EVT_SKIPPED, numMissed);
            }
        }

        if (fbqHeadPrevVal != LwosInstrumentationFbQueueHead)
        {
            status = s_lwosInstrumentationWriteQueueHead(
                &LwosInstrumentationFbQueueHead,
                &LwosInstrumentationFbQueueMemDesc);
        }
lwosInstrumentationFlush_exit:
        lwosNOP();
    }
    lwrtosEXIT_CRITICAL();

    return status;
}

static FLCN_STATUS
s_lwosInstrumentationRecalibrateISR
(
    LwU64 timestamp
)
{
    LWOS_INSTRUMENTATION_EVENT  event;
    LwU32                       timestampUpper;

    // Store the relevant bits only.
    LwosInstrumentationLastTimestamp =
        timestamp & ~DRF_MASK64(LW_INSTRUMENTATION_EVENT_DATA_TIME_DELTA);

    timestampUpper = LW_U32_MAX &
        (timestamp >> DRF_SIZE(LW_INSTRUMENTATION_EVENT_DATA_TIME_DELTA));

    // Build a recalibrate event with the correct type and timestamp.
    event.data =
        DRF_NUM(_INSTRUMENTATION_EVENT, _DATA, _EVENT_TYPE,
                LW2080_CTRL_FLCN_LWOS_INST_EVT_RECALIBRATE) |
        DRF_NUM(_INSTRUMENTATION_EVENT, _DATA, _TIME_ABS, timestampUpper);

    return s_lwosInstrumentationLog(&event);
}

/*!
 * Insert a recalibration event into the event stream. Allows the consumer
 * of instrumentation data to resynchronize time.
 *
 * @return  FLCN_OK    - the recalibration event was successfully inserted.
 * @return  FLCN_ERROR - the queue was full and the event could not be logged.
 */
FLCN_STATUS
lwosInstrumentationRecalibrate(void)
{
    FLCN_STATUS status;
    LwU64       timestamp;

    lwrtosENTER_CRITICAL();
    {
        timestamp = s_lwosInstrumentationGetTimestampReduced();

        status = s_lwosInstrumentationRecalibrateISR(timestamp);
    }
    lwrtosEXIT_CRITICAL();

    return status;
}

#ifdef UPROC_RISCV
sysKERNEL_DATA volatile LwrtosTaskHandle pPrevTaskHandle;
sysKERNEL_DATA volatile LwU8 ctxSwitchType = LW2080_CTRL_FLCN_LWOS_INST_EVT_CTXSW_YIELD;
sysKERNEL_DATA volatile LwU8 trapType = 0;
sysKERNEL_DATA volatile LwU8 trackingId = 0;


sysKERNEL_CODE void
lwrtosHookInstrumentationBegin(void)
{
    LwUPtr scause = csr_read(LW_RISCV_CSR_SCAUSE);
    pPrevTaskHandle = lwrtosTaskGetLwrrentTaskHandle();

    if (DRF_VAL64(_RISCV_CSR, _SCAUSE, _INT, scause))
    {
        switch (scause & ~DRF_NUM64(_RISCV_CSR, _MCAUSE, _INT, 0x1))
        {
            case LW_RISCV_CSR_SCAUSE_EXCODE_S_EINT:
                ctxSwitchType = LW2080_CTRL_FLCN_LWOS_INST_EVT_CTXSW_INT0;
                trackingId = 0;
                trapType = LW2080_CTRL_FLCN_LWOS_INST_EVT_HANDLER_BEGIN;
                break;
            case LW_RISCV_CSR_SCAUSE_EXCODE_S_TINT:
                ctxSwitchType = LW2080_CTRL_FLCN_LWOS_INST_EVT_CTXSW_TICK;
                break;
            case LW_RISCV_CSR_SCAUSE_EXCODE_S_SWINT:
            default:
                break;
        }
    }
    else
    {
        switch (scause & 0xFF)
        {
            // ODP - instrumented in handler
            case LW_RISCV_CSR_SCAUSE_EXCODE_IACC_FAULT:
            case LW_RISCV_CSR_SCAUSE_EXCODE_LACC_FAULT:
            case LW_RISCV_CSR_SCAUSE_EXCODE_SACC_FAULT:
            case LW_RISCV_CSR_SCAUSE_EXCODE_IPAGE_FAULT:
            case LW_RISCV_CSR_SCAUSE_EXCODE_LPAGE_FAULT:
            case LW_RISCV_CSR_SCAUSE_EXCODE_SPAGE_FAULT:
                break;

            // Illegal exception, going to crash anyways
            default:
                break;
        }
    }
    if (trapType != 0)
    {
        lwosInstrumentationLogFromISR(trapType, trackingId);
    }
}

sysKERNEL_CODE void
lwrtosHookInstrumentationEnd(void)
{
    LwrtosTaskHandle pNewTaskHandle = lwrtosTaskGetLwrrentTaskHandle();
    RM_RTOS_TCB_PVT *pLwTcbExt;

    if (pNewTaskHandle != NULL)
    {
        lwrtosTaskGetLwTcbExt(NULL, (void **)(&pLwTcbExt));
    }

    // Don't log event if there is no associated running task
    if (pLwTcbExt == NULL)
    {
        return;
    }

    // Only log CTXSW if the TCBs don't match (the task has changed)
    if (pNewTaskHandle != pPrevTaskHandle)
    {
        if (lwrtosTaskIsBlocked((LwrtosTaskHandle)pPrevTaskHandle))
        {
            ctxSwitchType = LW2080_CTRL_FLCN_LWOS_INST_EVT_CTXSW_BLOCK;
        }

        lwosInstrumentationLogFromISR(ctxSwitchType, pLwTcbExt->taskID);
    }

    // Reset cause for next time since we "deduce" yields by having it be the default ctxSwitchType
    ctxSwitchType = LW2080_CTRL_FLCN_LWOS_INST_EVT_CTXSW_YIELD;

    if (trapType == LW2080_CTRL_FLCN_LWOS_INST_EVT_HANDLER_BEGIN)
    {
         lwosInstrumentationLogFromISR(
            LW2080_CTRL_FLCN_LWOS_INST_EVT_HANDLER_END,
            trackingId);
         trapType = 0;
    }
}
#endif // UPROC_RISCV


GCC_ATTRIB_SECTION(LW_ARCH_VAL("imem_resident", "kernel_code"),
                   "lwrtosHookOdpInstrumentationEvent")
void
lwrtosHookOdpInstrumentationEvent
(
    LwBool bIsCodeMiss,
    LwBool bIsBegin
)
{
    RM_RTOS_TCB_PVT *pLwTcbExt = NULL;
    LwU8 eventType;

    if (lwrtosTaskGetLwrrentTaskHandle() != NULL)
    {
        lwrtosTaskGetLwTcbExt(NULL, (void **)(&pLwTcbExt));
    }

    // Don't log event if there is no associated running task
    if (pLwTcbExt == NULL)
    {
        return;
    }

    if (bIsBegin)
    {
        eventType = (bIsCodeMiss) ? LW2080_CTRL_FLCN_LWOS_INST_EVT_TASK_ODP_CODE_BEGIN :
                                    LW2080_CTRL_FLCN_LWOS_INST_EVT_TASK_ODP_DATA_BEGIN;
    }
    else
    {
        eventType = (bIsCodeMiss) ? LW2080_CTRL_FLCN_LWOS_INST_EVT_TASK_ODP_CODE_END :
                                    LW2080_CTRL_FLCN_LWOS_INST_EVT_TASK_ODP_DATA_END;
    }

    lwosInstrumentationLogFromISR(eventType, pLwTcbExt->taskID);
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @brief   Helper function to write the FB queue's metadata (capacity and head)
 *      out to the correct place in the frame buffer.
 *
 * @param[in]   capacity
 *      The capacity to write to the queue metadata page.
 * @param[in]   head
 *      The index of the head (in events, not bytes) to write to the queue
 *      metadata page.
 * @return  FLCN_OK    - the metadata was correctly written.
 * @return  FLCN_ERROR - flushing is lwrrently disabled or the write failed
 *              for some other reason.
 */
static LW_FORCEINLINE FLCN_STATUS
s_lwosInstrumentationWriteQueueMeta
(
    LwU32 capacity,
    LwU32 head,
    RM_FLCN_MEM_DESC *pQueueMemDesc
)
{
    FLCN_STATUS status = FLCN_OK;

    // DMA writes must be properly aligned
    LwU32 GCC_ATTRIB_ALIGN(sizeof(LwU32) * 2) metadata[2];

    metadata[0] = capacity;
    metadata[1] = head;

    status = dmaWrite(metadata, pQueueMemDesc, 0, sizeof(metadata));

    return status;
}

/*!
 * @brief   Helper function to write the FB queue's head out to the correct
 *      place in the frame buffer.
 *
 * @param[in]   pHead
 *      A pointer to the index of the head (in events, not bytes) to write to
 *      the queue metadata page.
 * @return  FLCN_OK    - the metadata was correctly written.
 * @return  FLCN_ERROR - flushing is lwrrently disabled or the write failed
 *              for some other reason.
 */
static LW_FORCEINLINE FLCN_STATUS
s_lwosInstrumentationWriteQueueHead
(
    LwU32 *pHead,
    RM_FLCN_MEM_DESC *pQueueMemDesc
)
{
    // Offset of one 32-bit integer (capacity)
    return dmaWrite(pHead, pQueueMemDesc, 1 * sizeof(LwU32), sizeof(*pHead));
}

/*!
 * @brief   Write out a block of specified size from resident queue to FB.
 *
 * This function does not change any resident queue metadata (size/head)
 *
 * @param[in]   data
 *      A pointer to the beginning of the data (in resident queue) to be
 *      flushed.
 * @param[in]   size
 *      The size of the data to be flushed.
 * @param[in]   pQueueMemDesc
 *      The memory descriptor of the queue data. Access is non-atomic, so
 *      we grab this once from the global during a critical section and pass
 *      around the result to avoid extra critical sections
 *
 * @return  FLCN_OK    - the block was correctly flushed.
 * @return  FLCN_ERROR - flushing is lwrrently disabled or the flush failed
 *              for some other reason.
 */
static FLCN_STATUS
s_lwosInstrumentationFlushBlock
(
    PLWOS_INSTRUMENTATION_EVENT data,
    LwU32                       size,
    RM_FLCN_MEM_DESC           *pQueueMemDesc
)
{
    FLCN_STATUS status       = FLCN_OK;
    LwU32       queueOffset  =
        LwosInstrumentationFbQueueHead * sizeof(LWOS_INSTRUMENTATION_EVENT);
    LwU32       totalOffset  = DMEM_BLOCK_SIZE + queueOffset;

    if (LwU64_ALIGN32_IS_ZERO(&(pQueueMemDesc->address)))
    {
        //
        // Address is NULL, so we haven't been initialized yet!
        // No need for a breakpoint here, as the program is expected to
        // continue successfully.
        //
        status = FLCN_ERROR;
        goto s_lwosInstrumentationFlushBlock_fail;
    }

    status = dmaWrite(data, pQueueMemDesc, totalOffset, size);

    LwosInstrumentationFbQueueHead += size / sizeof(LWOS_INSTRUMENTATION_EVENT);

    // Wrap the queue head into the correct range.
    while (LwosInstrumentationFbQueueHead >= LwosInstrumentationFbQueueCapacity)
    {
        LwosInstrumentationFbQueueHead -= LwosInstrumentationFbQueueCapacity;
    }

s_lwosInstrumentationFlushBlock_fail:
    return status;
}

/*!
 * @brief   Stage a new event to be logged.
 *
 * This function does _not_ do DMA IO. It only creates the required events and
 * puts them in the "resident queue", a DMEM/DTCM-resident block of memory
 * used for staging instrumentation events before they are flushed to FB.
 *
 * @param[in]   eventType
 *      The type of event, LW2080_CTRL_FLCN_LWOS_INST_EVT_*. Should not be
 *      RECALIBRATE.
 * @param[in]   trackingId
 *      Some tracking ID associated with a resource that created the event.
 *      Could be a semaphore, task, or overlay ID. In the case of EVT_SKIPPED,
 *      this is instead the number of skipped events.
 * @param[in]   bNeedsCritical
 *      LW_TRUE if this function is called outside of a critical section or
 *      ISR (and needs an additional critical section for synchronization).
 *      LW_FALSE otherwise.
 * @return  FLCN_OK    - the event was logged successfully.
 * @return  FLCN_ERROR - the queue is full or logging failed for another reason
 */
FLCN_STATUS
s_lwosInstrumentationLogStamped
(
    LwU8    eventType,
    LwU8    trackingId,
    LwBool  bNeedsCritical
)
{
    LWOS_INSTRUMENTATION_EVENT  event;
    FLCN_STATUS                 status = FLCN_OK;
    LwU64                       timestamp;
    LwU64                       dt;

    if (!LWOS_BM_GET(LwosInstrumentationEventMask, eventType, 8))
    {
        // This event type isn't supposed to be logged, so we should ignore it.
        goto s_lwosInstrumentationLogStamped_abort;
    }

    if (bNeedsCritical)
    {
        appTaskCriticalResEnter();
    }
    {
        timestamp = s_lwosInstrumentationGetTimestampReduced();

        dt = timestamp - LwosInstrumentationLastTimestamp;

        if (dt > LWOS_INSTR_DTIME_MAX)
        {
            status = s_lwosInstrumentationRecalibrateISR(timestamp);
            if (status == FLCN_OK)
            {
                //
                // Because this event is preceeded by a recalibration event
                // re-compute time delta based on the new timestamp.
                //
                dt = timestamp - LwosInstrumentationLastTimestamp;
            }
            else
            {
                goto s_lwosInstrumentationLogStamped_fail;
            }
        }

        // Construct the real event and log it to resident queue.
        event.data =
            DRF_NUM(_INSTRUMENTATION_EVENT, _DATA, _EVENT_TYPE, eventType) |
            DRF_NUM(_INSTRUMENTATION_EVENT, _DATA, _EXTRA, trackingId)     |
            DRF_NUM(_INSTRUMENTATION_EVENT, _DATA, _TIME_DELTA, dt);

        status = s_lwosInstrumentationLog(&event);
        if (status == FLCN_OK)
        {
            // The message was recorded, so we can set the last timestamp.
            LwosInstrumentationLastTimestamp = timestamp;
        }
        else
        {
            goto s_lwosInstrumentationLogStamped_fail;
        }
s_lwosInstrumentationLogStamped_fail:
        lwosNOP();
    }
    if (bNeedsCritical)
    {
        appTaskCriticalResExit();
    }

s_lwosInstrumentationLogStamped_abort:
    return status;
}

/*!
 * @brief   Stage an event for logging by placing it in the resident queue.
 *
 * @param[in]   pEvent
 *      A fully-constructed event structure ready for logging.
 *
 * @return  FLCN_OK    - the event was added to the log.
 * @return  FLCN_ERROR - the queue was full and this event was missed.
 */
static FLCN_STATUS
s_lwosInstrumentationLog
(
    PLWOS_INSTRUMENTATION_EVENT pEvent
)
{
    FLCN_STATUS status = FLCN_OK;

    LWOS_INSTRUMENTATION_EVENT event = *pEvent;

    // If instrumentation protection is ON, we only want to write dummy events.
    if (LWOS_INSTR_IS_PROTECTED())
    {
        event.data = 0;
    }

    if (LwosInstrumentationResidentQueueSize == LWOS_INSTR_RESIDENT_QUEUE_SIZE)
    {
        //
        // Queue full.
        // No need to OS_BREAKPOINT here; this is an expected error and
        // not an illegal state, and the program is expected to recover.
        //

        // TODO-KS: Flush if the compile-time option is enabled
        LwosInstrumentationMissedEvents++;
        status = FLCN_ERROR;
    }
    else
    {
        LwosInstrumentationResidentQueue[LwosInstrumentationResidentQueueHead] = event;
        LwosInstrumentationResidentQueueHead =
            LWOS_INSTR_COERCE_RESIDENT_INDEX(LwosInstrumentationResidentQueueHead + 1);
        LwosInstrumentationResidentQueueSize++;
    }

    return status;
}

/*!
 * @brief   Helper function to callwlate a timestamp.
 *
 * @return  A timestamp with 32ns units (@ref LWOS_INSTR_TIMER_RESOLUTION_BITS).
 *          If PTIMER0 is not used resolution matches RTOS tick period.
 */
static LwU64
s_lwosInstrumentationGetTimestampReduced(void)
{
    LwU64 timestamp;

    if (LWOS_INSTR_USE_PTIMER())
    {
        osPTimerTimeNsLwrrentGet((void*) &timestamp);
    }
    else
    {
        //
        // Hacks to skip loading the LwU64 mul overlay, because we only need to
        // do a fixed multiply. Multiply the timer tick by LWRTOS_TICK_PERIOD_NS.
        //
        // Warning: This does not work when tick period exceeds 65536ns.
        //
        ct_assert(LWRTOS_TICK_PERIOD_NS <= LW_U16_MAX);

        LwU32 tc = lwrtosTaskGetTickCount32();
        LwU32 lower = tc & 0xFFFF;
        LwU32 upper = tc >> 16;
        LwU32 ltUpper = upper * LWRTOS_TICK_PERIOD_NS;
        LwU32 ltLower = lower * LWRTOS_TICK_PERIOD_NS;
        // Warning: Compiler uses here a non-efficient library call.
        timestamp = (((LwU64) ltUpper) << 16) + ltLower;
    }

    //
    // Shift timestamps by @ref LWOS_INSTR_TIMER_RESOLUTION_BITS bits,
    // because the best resolution we can get (with PTIMER0) is 32ns.
    //
    timestamp >>= LWOS_INSTR_TIMER_RESOLUTION_BITS;

    return timestamp;
}

static LwU32
s_lwosInstrumentationGetFlushBeginIndex(void)
{
    LwU32 flushBeginIndex;

    // Resident queue index access ok since we should already be in critical section
    flushBeginIndex =
        LWOS_INSTR_COERCE_RESIDENT_INDEX(LwosInstrumentationResidentQueueHead -
                                         LwosInstrumentationResidentQueueSize);

    return flushBeginIndex;
}
