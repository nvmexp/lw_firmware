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
 * @file    lwos_ustreamer.c
 * @brief   Implementation and static functions for uStreamer logging framework.
 */

// TODO: yaotianf: Cleanup imports once functional implemtation is complete.
#include "lwos_ustreamer.h"
#include "lwos_utility.h"
#include "lwoslayer.h"
#include "lwos_dma.h"
#include "lwmisc.h"
#include "dmemovl.h"
#include "lwrtos.h"
#include "osptimer.h"
#include "lib_lw64.h"
#include "pmu_oslayer.h"

#ifdef UPROC_RISCV
#include "drivers/drivers.h"
#include "config/g_sections_riscv.h"
#include <riscv_csr.h>
#else // UPROC_RISCV
#error  "uStreamer is only supported on RISCV ODP profiles"
#endif // UPROC_RISCV

#include "lwrtosHooks.h"


/* --------------------------- Private Defines ------------------------------ */
#define LWOS_USTREAMER_QUEUE_METADATA_PAGENUMBER_DEFAULT                       1
#define USTREAER_EVENT_PAYLOADCOMPACT_SIZE_BYTES                               \
    ((DRF_SIZE(LW2080_CTRL_FLCN_USTREAMER_EVENT_TAIL_PAYLOADCOMPACT) + 7) / 8)

//! The timer resolution is 1 << 5 = 32 nanoseconds.
#define LWOS_INSTR_TIMER_RESOLUTION_BITS                                      5U

//! Colwert event ID into relevant index in event mask
#define EVENT_ID_MASK_IDX(_eventId, _bIsCompact)                               \
    ((_eventId) +                                                              \
     ((_bIsCompact) ? 0U : LW2080_CTRL_FLCN_USTREAMER_NUM_EVT_TYPES_COMPACT))

//! Write to pQ's local buffer; handles zeroing data when protection is enabled
#define LOCAL_BUFFER_WRITE(_pQ, _idx, _data)                                   \
do                                                                             \
{                                                                              \
    if (LWOS_USTREAMER_IS_PROTECTED())                                         \
    {                                                                          \
        (_pQ)->pBuffer[(_idx)] = 0U;                                           \
    }                                                                          \
    else                                                                       \
    {                                                                          \
        (_pQ)->pBuffer[(_idx)] = (_data);                                      \
    }                                                                          \
} while (LW_FALSE)

/* -------------------------- Private Structures ---------------------------- */

typedef struct
{
    //! Bitfield describing the flushing policy and idle flush threshold.
    LwU32 flushingPolicy;

    //! Timestamp of previous event. Used to callwlate the offset value.
    LwU64  lastTimestamp;

    //! Pointer to the buffer.
    LwU32 *pBuffer;

    //! Size of queue, in 32bit-words.
    LwU32 capacity;

    //! Number of DWORDs used in the current queue.
    LwU32 filledDwords;

    //! Index of the enqueue location.
    LwU32 enqueueIndex;

    //! Index of the dequeue location.
    LwU32 dequeueIndex;

    //! Number of event lost due to local buffer full.
    LwU32 lostEventCount;

    //! Page size of the queue.
    LwU32 pageSize;

    //!
    // Page is the unit of transfer between PMU and FB. It is always 1/2 the
    // queue capacity. Page number starts from 1. This allows an empty buffer to
    // not be misidentified as a filled buffer. This field keeps track of the
    // current page number that is being filled.
    //
    LwU32 pageNumber;

    //! Number of pages that the fb buffer can hold.
    LwU32 numberOfPages;

    //!
    // A surface in FB that we can flush to, or NULL if no if this queue is not
    // in an active(ready to flush) state.
    //
    RM_FLCN_MEM_DESC fbQueueMemDesc;

    //! Offset of the matching fbBuffer in super surface.
    LwU32 fbBufferOffset;

    //! Queue Feature Id.
    LwU8 featureId;

    //! Is the local buffer for this queue resident or ODP.
    LwBool bResident;
} LWOS_USTREAMER_QUEUE_METADATA;

/* -------------------------- Private Variables ----------------------------- */
static LWOS_USTREAMER_QUEUE_METADATA              *pLwosUstreamerQueueMetadataArray = NULL;
static LwU8                                        lwosUstreamerQueueCount = 0U;
static LwU8                                        lwosUstreamerQueueNextId = 0U;
static LwBool                                      lwosUstreamerStopped = LW_FALSE;

/* -------------------------- Public Variables ------------------------------ */
//! Implementing uStreamer default queueId.
LwU8 ustreamerDefaultQueueId;

//! Bitmask to filter which events are logged to default queue.
LW2080_CTRL_FLCN_USTREAMER_EVENT_FILTER LwosUstreamerEventFilter;

/* -------------------------- Static Prototypes ----------------------------- */
static FLCN_STATUS s_lwosUstreamerInsertDWORD (LWOS_USTREAMER_QUEUE_METADATA *pQ,
    LwU32 data, LwBool bIsHead, LwBool bRecordFailureOnFull)
    GCC_ATTRIB_SECTION("imem_ustreamerRuntime", "s_lwosUstreamerInsertDWORD");
static LwU64 s_lwosUstreamerGetTimestamp(void)
    GCC_ATTRIB_SECTION("imem_ustreamerRuntime", "s_lwosUstreamerGetTimestamp");
static LW_FORCEINLINE
    LwU32 s_lwosUstreamerGetMissedEvent(LWOS_USTREAMER_QUEUE_METADATA *pQ)
    GCC_ATTRIB_SECTION("imem_ustreamerRuntime", "s_lwosUstreamerGetMissedEvent");
static LWOS_USTREAMER_QUEUE_METADATA *s_lwosUstreamerGetQueue(LwU8 queueId)
    GCC_ATTRIB_SECTION("imem_ustreamerRuntime", "s_lwosUstreamerGetQueue");
static FLCN_STATUS s_lwosUstreamerFlush(LWOS_USTREAMER_QUEUE_METADATA *pQ,
    LwBool bImmediateFlush, LwBool bEnterCritical)
    GCC_ATTRIB_SECTION("imem_ustreamerRuntime", "s_lwosUstreamerFlush");
static FLCN_STATUS s_lwosUstreamerLog(LwU8 queueId, LwU8 eventId, LwU8 *pPayload,
    LwU32 payloadSize, LwBool bIsCompact, LwBool bEnterCritical)
    GCC_ATTRIB_SECTION("imem_ustreamerRuntime", "s_lwosUstreamerLog");

/* -------------------------- Public Functions ------------------------------ */
/*!
 * @copydoc lwosUstreamerGetEventFilter
 */
FLCN_STATUS
lwosUstreamerGetEventFilter(LwU8 queueId, LW2080_CTRL_FLCN_USTREAMER_EVENT_FILTER *pEventFilter)
{
    FLCN_STATUS status = FLCN_OK;
    LWOS_USTREAMER_QUEUE_METADATA  *pQueue;

    pQueue = s_lwosUstreamerGetQueue(queueId);
    if (NULL == pQueue)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
    }
    else
    {
        // TODO: Access event filter from queue struct once it's moved there
        *pEventFilter = LwosUstreamerEventFilter;
    }

    return status;
}
/*!
 * @copydoc lwosUstreamerSetEventFilter
 */
FLCN_STATUS
lwosUstreamerSetEventFilter(LwU8 queueId, LW2080_CTRL_FLCN_USTREAMER_EVENT_FILTER *pEventFilter)
{
    FLCN_STATUS status = FLCN_OK;
    LWOS_USTREAMER_QUEUE_METADATA  *pQueue;

    pQueue = s_lwosUstreamerGetQueue(queueId);
    if (NULL == pQueue)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
    }
    else
    {
        // TODO: Access event filter from queue struct once it's moved there
        LwosUstreamerEventFilter = *pEventFilter;
    }
    return status;
}

/*!
 * @copydoc lwosUstreamerActiveQueueCountGet
 */
LwU8
lwosUstreamerActiveQueueCountGet(void)
{
    return lwosUstreamerQueueNextId;
}

/*!
 * @copydoc lwosUstreamerGetQueueInfo
 */
FLCN_STATUS
lwosUstreamerGetQueueInfo
(
    LwU8                                queueId,
    RM_FLCN_USTREAMER_QUEUE_DESCRIPTOR  *pQueueDescriptor
)
{
    LWOS_USTREAMER_QUEUE_METADATA  *pQueue;
    pQueue = s_lwosUstreamerGetQueue(queueId);

    if ((pQueueDescriptor == NULL) ||
        (pQueue == NULL))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    pQueueDescriptor->queueFeatureId            = pQueue->featureId;
    pQueueDescriptor->pageSize                  = pQueue->pageSize;
    pQueueDescriptor->superSurfaceBufferOffset  = pQueue->fbBufferOffset;
    pQueueDescriptor->superSurfaceBufferSize    =
        DRF_VAL(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE, pQueue->fbQueueMemDesc.params);

    return FLCN_OK;
}

/*!
 * @copydoc lwosUstreamerInitialize
 */
FLCN_STATUS
lwosUstreamerInitialize(void)
{
    FLCN_STATUS status = FLCN_OK;

    lwrtosENTER_CRITICAL();
    {
        lwosUstreamerQueueCount             = lwosUstreamerGetQueueCount();
        pLwosUstreamerQueueMetadataArray    =
            lwosMallocResidentType(lwosUstreamerQueueCount, LWOS_USTREAMER_QUEUE_METADATA);

        if (pLwosUstreamerQueueMetadataArray == NULL)
        {
            status = FLCN_ERR_NO_FREE_MEM;
            OS_BREAKPOINT();
            goto lwosUstreamerInitialize_fail;
        }
        lwosUstreamerQueueNextId = 0U;

lwosUstreamerInitialize_fail:
        lwosNOP();
    }
    lwrtosEXIT_CRITICAL();

    return status;
}

// TODO: add argument for flushing policy.
/*!
 * @copydoc lwosUstreamerQueueConstruct
 */
FLCN_STATUS
lwosUstreamerQueueConstruct
(
    LwU32              *pLocalBuffer,
    LwU32               localBufferSize,
    RM_FLCN_MEM_DESC   *pBaseMemDesc,
    LwU32               fbBufferOffset,
    LwU32               fbBufferSize,
    LwU8               *pQueueId,
    LwU8                queueFeatureId,
    LwBool              bResident
)
{
    FLCN_STATUS         status  = FLCN_OK;
    RM_FLCN_MEM_DESC    bufDesc = *pBaseMemDesc;
    LwU64               superSurfaceQueueAddress;
    LwU32               pageSize;
    LwU32               superSurfaceSize;
    LwBool              bMappingFound = LW_FALSE;
    // Index to loop through all data sections to perform residency check.
    LwU8                sectionIdx;

    if ((pLocalBuffer == NULL) ||
        (pBaseMemDesc == NULL) ||
        (pQueueId == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto lwosUstreamerQueueConstruct_exit_no_critical;
    }

    // Check if the local buffer residency match the passed in argument.
    for (sectionIdx = UPROC_DATA_SECTION_FIRST;
        sectionIdx < UPROC_DATA_SECTION_FIRST + UPROC_DATA_SECTION_COUNT;
        sectionIdx++)
    {
        LwUPtr startAddr = SectionDescStartVirtual[sectionIdx];
        LwUPtr endAddr   = startAddr + SectionDescHeapSize[sectionIdx];

        if (((LwUPtr)pLocalBuffer >= startAddr) &&
            (((LwUPtr)pLocalBuffer + localBufferSize) >= startAddr) &&
            (((LwUPtr)pLocalBuffer + localBufferSize) <= endAddr))
        {
            bMappingFound = LW_TRUE;
            const LwBool bIsOdpCheck = UPROC_SECTION_LOCATION_IS_ODP(SectionDescLocation[sectionIdx]);
            if (bIsOdpCheck != (!bResident))
            {
                status = FLCN_ERR_ILWALID_ARGUMENT;
                dbgPrintf(LEVEL_CRIT, "bResident mismatch with section metadata!\n");
                OS_BREAKPOINT();
                goto lwosUstreamerQueueConstruct_exit_no_critical;
            }
        }
    }

    if (!bMappingFound)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto lwosUstreamerQueueConstruct_exit_no_critical;
    }

    // Callwlate the super surface offset.
    LwU64_ALIGN32_UNPACK(&superSurfaceQueueAddress, &bufDesc.address);
    lw64Add32(&superSurfaceQueueAddress, fbBufferOffset);

    superSurfaceSize =
        DRF_VAL(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE, pBaseMemDesc->params);

    //
    // If the allocated super surface size doesn't contain the whole buffer,
    // we messed up something RM-side when allocating the surface.
    //
    if ((fbBufferOffset + fbBufferSize) > superSurfaceSize)
    {
        status = FLCN_ERR_ILWALID_STATE;
        OS_BREAKPOINT();
        goto lwosUstreamerQueueConstruct_exit_no_critical;
    }

    LwU64_ALIGN32_PACK(&bufDesc.address, &superSurfaceQueueAddress);
    bufDesc.params =
        FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE, fbBufferSize, bufDesc.params);

    pageSize = localBufferSize / 2;

    fbBufferSize = DRF_VAL(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE, bufDesc.params);

    if
    (
        (fbBufferSize < localBufferSize) ||
        (fbBufferSize % localBufferSize != 0) ||
        (queueFeatureId >= LW2080_CTRL_FLCN_USTREAMER_FEATURE__COUNT) ||
        (!LW_IS_ALIGNED(bufDesc.address.lo, DMA_XFER_SIZE_BYTES(DMA_XFER_ESIZE_MIN_WRITE))) ||
        (!LW_IS_ALIGNED((LwLength)pLocalBuffer, DMA_XFER_SIZE_BYTES(DMA_XFER_ESIZE_MIN_WRITE))) ||
        (!LW_IS_ALIGNED(pageSize, DMA_XFER_SIZE_BYTES(DMA_XFER_ESIZE_MIN_WRITE)))
    )
    {
        OS_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto lwosUstreamerQueueConstruct_exit_no_critical;
    }

    lwrtosENTER_CRITICAL();
    {
        if (lwosUstreamerQueueNextId == lwosUstreamerQueueCount)
        {
            // Constructing more queues than initially specified.
            status = FLCN_ERR_ILLEGAL_OPERATION;
            dbgPrintf(LEVEL_CRIT, "lwosUstreamerQueueConstruct is called more times than lwosUstreamerQueueCount\n");
            OS_BREAKPOINT();
            goto lwosUstreamerQueueConstruct_exit;
        }

        LWOS_USTREAMER_QUEUE_METADATA *pNewQueue;

        *pQueueId                   = lwosUstreamerQueueNextId++;
        pNewQueue                   = &pLwosUstreamerQueueMetadataArray[*pQueueId];
        pNewQueue->lastTimestamp    = 0U;
        pNewQueue->pBuffer          = pLocalBuffer;
        pNewQueue->capacity         = localBufferSize / sizeof(LwU32);
        pNewQueue->filledDwords     = 0U;
        pNewQueue->pageSize         = pageSize;
        pNewQueue->enqueueIndex     = 0U;
        pNewQueue->dequeueIndex     = 0U;
        pNewQueue->pageNumber       = LWOS_USTREAMER_QUEUE_METADATA_PAGENUMBER_DEFAULT;
        pNewQueue->numberOfPages    = fbBufferSize / pageSize;
        pNewQueue->fbQueueMemDesc   = bufDesc;
        pNewQueue->fbBufferOffset   = fbBufferOffset;
        pNewQueue->featureId        = queueFeatureId;
        pNewQueue->bResident        = bResident;

        // Default policy
        pNewQueue->flushingPolicy   =
            DRF_DEF(2080_CTRL_FLCN_USTREAMER, _QUEUE_POLICY, _IDLE_FLUSH, _ENABLED);
    } // lwrtos_CRITICAL
lwosUstreamerQueueConstruct_exit:
    lwrtosEXIT_CRITICAL();
lwosUstreamerQueueConstruct_exit_no_critical:
    return status;
}

/*!
 * @copydoc lwosUstreamerShutdown
 */
FLCN_STATUS
lwosUstreamerShutdown(void)
{
    lwrtosENTER_CRITICAL();
    {
        lwosUstreamerQueueNextId    = 0U;
        lwosUstreamerStopped        = LW_TRUE;
    }
    lwrtosEXIT_CRITICAL();

    return FLCN_OK;
}

/*!
 * @copydoc lwosUstreamerLog
 */
FLCN_STATUS
lwosUstreamerLog
(
    LwU8 queueId,
    LwU8 eventId,
    LwU8 *pPayload,
    LwU32 size
)
{
    return s_lwosUstreamerLog(queueId, eventId, pPayload, size, LW_FALSE, LW_TRUE);
}

/*!
 * @copydoc lwosUstreamerLogCompact
 */
FLCN_STATUS
lwosUstreamerLogCompact
(
    LwU8 queueId,
    LwU8 eventId,
    LwU32 payload
)
{
    return s_lwosUstreamerLog(queueId, eventId, (LwU8 *)&payload,
        USTREAER_EVENT_PAYLOADCOMPACT_SIZE_BYTES, LW_TRUE, LW_TRUE);
}

/*!
 * @copydoc lwosUstreamerLogFromISR
 */
FLCN_STATUS
lwosUstreamerLogFromISR
(
    LwU8 queueId,
    LwU8 eventId,
    LwU8 *pPayload,
    LwU32 size
)
{
    return s_lwosUstreamerLog(queueId, eventId, pPayload, size, LW_FALSE, LW_FALSE);
}

/*!
 * @copydoc lwosUstreamerLogCompactFromISR
 */
FLCN_STATUS
lwosUstreamerLogCompactFromISR
(
    LwU8 queueId,
    LwU8 eventId,
    LwU32 payload
)
{
    return s_lwosUstreamerLog(queueId, eventId, (LwU8 *)&payload,
        USTREAER_EVENT_PAYLOADCOMPACT_SIZE_BYTES, LW_TRUE, LW_FALSE);
}

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

    lwosUstreamerLogCompactFromISR(ustreamerDefaultQueueId, eventType, pLwTcbExt->taskID);
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
        switch (scause & ~DRF_NUM64(_RISCV_CSR, _SCAUSE, _INT, 0x1))
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
        lwosUstreamerLogCompactFromISR(ustreamerDefaultQueueId, trapType, trackingId);
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

        lwosUstreamerLogCompactFromISR(ustreamerDefaultQueueId, ctxSwitchType, pLwTcbExt->taskID);
    }

    // Reset cause for next time since we "deduce" yields by having it be the default ctxSwitchType
    ctxSwitchType = LW2080_CTRL_FLCN_LWOS_INST_EVT_CTXSW_YIELD;

    if (trapType == LW2080_CTRL_FLCN_LWOS_INST_EVT_HANDLER_BEGIN)
    {
         lwosUstreamerLogCompactFromISR(
            ustreamerDefaultQueueId,
            LW2080_CTRL_FLCN_LWOS_INST_EVT_HANDLER_END,
            trackingId);
         trapType = 0;
    }
}
#endif // UPROC_RISCV

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @brief   Insert a DWORD into a queue. Flush when needed.
 *
 * The caller of this function should guarantee that this function has exclusive
 * access to pQ for the duration of this function. (i.e. in a critical section)
 * This function handle the insertion of page number and timestamp on the beginning
 * of each page. This function will also handle the case when the local buffer is
 * full and flushing is not possible. It will replace the second to last DWORD in
 * current page with a data lost event. This will also increment the lost event counter.
 * When this function returns FLCN_ERROR, the caller should also abort the event
 * insertion operation, as on each insertion failure the lostEventCount will be
 * incremeneted.
 *
 * @param[in] pQ
 *      Pointer to the queue to insert DWORD into.
 * @param[in] data
 *      DWORD to insert.
 * @param[in] bIsHead
 *      Is the DWORD an event head. If true, the data is ignored and an event
 *      head with the correct timestamp is inserted. This is special because
 *      when the head is the first event in a page, the "base" timestamp would
 *      change, thus this will allow it to always insert the correct timestamp.
 *
 * @return FLCN_OK on success, FLCN_ERR_NO_FREE_MEM when the queue is full.
 */
FLCN_STATUS
s_lwosUstreamerInsertDWORD
(
    LWOS_USTREAMER_QUEUE_METADATA  *pQ,
    LwU32                           data,
    LwBool                          bIsHead,
    LwBool                          bRecordFailureOnFull
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 halfFullPoint;
    LwU32 pageEnd;

    halfFullPoint   = pQ->capacity / 2;
    pageEnd         = (pQ->enqueueIndex < halfFullPoint) ?
        (halfFullPoint - 1) : (pQ->capacity - 1);

    //
    // If we are at the end of a page, we must be pointing to a page number. We
    // should start a new page.
    //
    if (pQ->enqueueIndex == pageEnd)
    {
        LwU32 newEnqueueIndex = (pQ->enqueueIndex + 1) % pQ->capacity;

        // Full queue and flushing failed.
        if (newEnqueueIndex == pQ->dequeueIndex)
        {
            if (bRecordFailureOnFull)
            {
                if (pQ->lostEventCount < DRF_MASK(LW2080_CTRL_FLCN_USTREAMER_EVENT_TAIL_PAYLOAD))
                {
                    pQ->lostEventCount++;
                }
                LOCAL_BUFFER_WRITE(pQ, pQ->enqueueIndex-1, s_lwosUstreamerGetMissedEvent(pQ));
            }
            status = FLCN_ERR_NO_FREE_MEM;
            goto s_lwosUstreamerInsertDWORD_fail;
        }
        else
        {
            pQ->enqueueIndex = newEnqueueIndex;
            pageEnd = newEnqueueIndex + halfFullPoint - 1;
        }
    }

    // Insert page number and timestamp
    if ((pQ->enqueueIndex == 0) || (pQ->enqueueIndex == halfFullPoint))
    {
        LwU32 newBaseTimestamp          =
            s_lwosUstreamerGetTimestamp() >> (DRF_SIZE(LW2080_CTRL_FLCN_USTREAMER_EVENT_HEAD_TIME) - 1);

        pQ->pBuffer[pQ->enqueueIndex] = pQ->pageNumber;
        pQ->enqueueIndex++;
        pQ->pBuffer[pageEnd] = pQ->pageNumber;
        pQ->pageNumber++;
        LOCAL_BUFFER_WRITE(pQ, pQ->enqueueIndex, newBaseTimestamp);
        pQ->enqueueIndex++;
        pQ->lastTimestamp =
            (LwU64)newBaseTimestamp << (DRF_SIZE(LW2080_CTRL_FLCN_USTREAMER_EVENT_HEAD_TIME) - 1);

        // Page metadata takes 3 DWORDs
        pQ->filledDwords += 3;
    }

    if (bIsHead)
    {
        LwU64   lwrrentTimestamp;
        LwU32   timeDelta;

        //
        // Note: timeDelta could wrap around / overflow after ~1 min or so.
        // We create the event head in this function because the "base time"
        // could change because we flipped to a new page.
        //
        lwrrentTimestamp            = s_lwosUstreamerGetTimestamp();
        timeDelta                   = (LwU32)(lwrrentTimestamp - pQ->lastTimestamp);
        pQ->lastTimestamp  = lwrrentTimestamp;

        data = DRF_NUM(2080_CTRL_FLCN_USTREAMER_EVENT, _COMM, _FLAG, 1) |
               DRF_NUM(2080_CTRL_FLCN_USTREAMER_EVENT, _COMM, _HEAD, 1) |
               DRF_NUM(2080_CTRL_FLCN_USTREAMER_EVENT, _HEAD, _TIME, timeDelta);
    }

    LOCAL_BUFFER_WRITE(pQ, pQ->enqueueIndex, data);
    pQ->enqueueIndex++;
    pQ->filledDwords++;

s_lwosUstreamerInsertDWORD_fail:
    return status;
}

/*!
 * @brief    Get the current time with 32ns resloution. (1 tick = 32ns)
 *
 * @return   Current timer value.
 */
LwU64
s_lwosUstreamerGetTimestamp(void)
{
    LwU64 timestamp;

    if (LWOS_USTREAMER_USE_PTIMER())
    {
        osPTimerTimeNsLwrrentGet((void*)&timestamp);
    }
    else
    {
        // TODO: extract time from timer tick count
        OS_BREAKPOINT();
    }

    //
    // Shift timestamps by @ref LWOS_INSTR_TIMER_RESOLUTION_BITS bits,
    // because the best resolution we can get (with PTIMER0) is 32ns.
    //
    timestamp >>= LWOS_INSTR_TIMER_RESOLUTION_BITS;

    return timestamp;
}

/*!
 * @brief   Get the missed count event for the specified queue.
 *
 * This function will also handle limiting the number of missed event to fit
 * inside the field.
 *
 * @param[in] pQ    Pointer to the queue metadata structure.
 *
 * @return  The DWORD contains the missed count event.
 */
LwU32
s_lwosUstreamerGetMissedEvent
(
    LWOS_USTREAMER_QUEUE_METADATA *pQ
)
{
    LwU32 lostEventCount = pQ->lostEventCount;
    if (lostEventCount > DRF_MASK(LW2080_CTRL_FLCN_USTREAMER_EVENT_TAIL_PAYLOAD))
    {
        lostEventCount = DRF_MASK(LW2080_CTRL_FLCN_USTREAMER_EVENT_TAIL_PAYLOAD);
    }

    return DRF_NUM(2080_CTRL_FLCN_USTREAMER_EVENT, _COMM, _FLAG, 1) |
           DRF_NUM(2080_CTRL_FLCN_USTREAMER_EVENT, _TAIL, _VARIABLE, 1) |
           DRF_NUM(2080_CTRL_FLCN_USTREAMER_EVENT, _TAIL, _EVENTID, 0) |
           DRF_NUM(2080_CTRL_FLCN_USTREAMER_EVENT, _TAIL, _PAYLOAD, lostEventCount);
}

/*!
 * @brief   Get queue metadata using queueId
 *
 * @param[in] queueId   Id of the queue.
 *
 * @returns NULL    If invalid queueId is provided.
 * @returns         Pointer to the LWOS_USTREAMER_QUEUE_METADATA structure.
 */
LWOS_USTREAMER_QUEUE_METADATA *
s_lwosUstreamerGetQueue
(
    LwU8 queueId
)
{
    if (queueId < lwosUstreamerQueueNextId)
    {
        return &pLwosUstreamerQueueMetadataArray[queueId];
    }
    return NULL;
}

/*!
 * @copydoc lwosUstreamerFlush
 */
FLCN_STATUS
lwosUstreamerFlush
(
    LwU8 queueId
)
{
    LWOS_USTREAMER_QUEUE_METADATA   *pQ     = s_lwosUstreamerGetQueue(queueId);
    FLCN_STATUS                      status = FLCN_OK;

    if (pQ != NULL)
    {
        status = s_lwosUstreamerFlush(pQ, LW_TRUE, LW_TRUE);
    }
    else
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
    }

    return status;
}

/*!
 * @copydoc lwosUstreamerIdleHook
 */
void
lwosUstreamerIdleHook()
{
    // Iterate over all the queue and flush those specified with IDLE policy
    LwU8 queueId;
    for(queueId = 0; queueId < lwosUstreamerActiveQueueCountGet(); queueId++)
    {
        LWOS_USTREAMER_QUEUE_METADATA   *pQ = s_lwosUstreamerGetQueue(queueId);
        if (pQ != NULL)
        {
            //
            // Note: this is _technically_ a race condition by not
            // entering a critical section and reading pQ. However,
            // the s_lwosUstreamerFlush will double check everything
            // properly before actually changing state / flushing data.
            //
            if (DRF_VAL(2080_CTRL_FLCN_USTREAMER, _QUEUE_POLICY, _IDLE_FLUSH,
                    pQ->flushingPolicy) == LW_TRUE)
            {
                const LwU32 forceFlushThreshold = DRF_VAL(2080_CTRL_FLCN_USTREAMER,
                    _QUEUE_POLICY, _IDLE_THRESHOLD, pQ->flushingPolicy);
                if ((forceFlushThreshold != 0) &&
                    (pQ->filledDwords > forceFlushThreshold))
                {
                    s_lwosUstreamerFlush(pQ, LW_TRUE, LW_TRUE);
                }
                else if(pQ->filledDwords > 0)
                {
                    s_lwosUstreamerFlush(pQ, LW_FALSE, LW_TRUE);
                }
            }
        }
    }
}


/*!
 * @brief   Attempt to flush the specified local buffer to respective FB buffer.
 *      Note that flushing policy is primarly handled in the wrapper function,
 *      this function just simply perform the core queue flushing logic.
 *
 * @param[in]   pQueue
 *      Pointer to the queue metadata structure describing the queue to flush.
 *
 * @param[in]   bImmediateFlush
 *      Immediate flush will flush the local buffer even if it is not full yet. =
 *  This will cause an "early termination" event to be inserted and queue flushed
 *  to FB immediately.
 *
 * @return FLCN_OK      Flush success. Dequeue pointer is also moved.
 * @return FLCN_ERROR   Failed to flush data into FB.
 */
FLCN_STATUS
s_lwosUstreamerFlush
(
    LWOS_USTREAMER_QUEUE_METADATA   *pQ,
    LwBool                           bImmediateFlush,
    LwBool                           bEnterCritical
)
{
    FLCN_STATUS status = FLCN_OK;

    if (pQ == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_lwosUstreamerFlush_fail_no_critical;
    }

    const LwU32   halfBufferSize = pQ->capacity / 2;

    void   *pLocalBufferStart;
    LwU32   newDequeueIndex;
    LwU32   newEnqueueIndex;
    LwU32   lwrrentPageStart;
    LwU32   lwrrentPageEnd;
    LwU32   flushSizeBytes = 0;
    LwBool  bIsQueueResident = pQ->bResident;

    if (bEnterCritical)
    {
        if (bIsQueueResident)
        {
            lwrtosENTER_CRITICAL();
        }
        else
        {
            // non-resident queue can only be logged from task.
            if (!lwrtosIS_KERNEL())
            {
                appTaskCriticalEnter();
            }
            else
            {
                status = FLCN_ERR_ILWALID_STATE;
                goto s_lwosUstreamerFlush_fail_no_critical;
            }
        }
    }
    {
        if (lwosDmaIsSuspended())
        {
            // TODO: yaotianf: handle exit MSCG when adding flush policy support.
            status = FLCN_ERR_DMA_SUSPENDED;
            goto s_lwosUstreamerFlush_fail;
        }

        if (bImmediateFlush)
        {
            if (pQ->enqueueIndex >= halfBufferSize)
            {
                lwrrentPageStart = halfBufferSize;
                lwrrentPageEnd = pQ->capacity - 1;
            }
            else
            {
                lwrrentPageStart = 0;
                lwrrentPageEnd = halfBufferSize - 1;
            }

            // Checks if the buffer is at a page boundary.
            if ((pQ->enqueueIndex > lwrrentPageStart) &&
                (pQ->enqueueIndex < lwrrentPageEnd))
            {
                // Insert variable event tail with eventId 0 and size 0
                pQ->pBuffer[pQ->enqueueIndex] =
                    DRF_NUM(2080_CTRL_FLCN_USTREAMER_EVENT, _COMM, _FLAG, 1) |
                    DRF_NUM(2080_CTRL_FLCN_USTREAMER_EVENT, _TAIL, _VARIABLE, 1);

                //
                // Precondition: enqueueIndex != lwrrentPageStart.
                // The (-1) accounts for the trailing metadata dword.
                //
                pQ->filledDwords += (lwrrentPageEnd - pQ->enqueueIndex - 1);
                pQ->enqueueIndex = lwrrentPageEnd;
            }
        }



        // Queue is full if ((enqueue + 1) % capacity == dequeue)

        //
        // enqueueIndex here is checked against halfBufferSize - 1 since the last DWORD
        // of each page is page number and popluated initially. This also help solve the
        // case where a page gets stuck full for a while before a flush can be exelwted.
        //
        if ((pQ->dequeueIndex == 0) &&
            (pQ->enqueueIndex >= halfBufferSize - 1))
        {
            pLocalBufferStart = (void*) pQ->pBuffer;
            if (pQ->enqueueIndex == (pQ->capacity - 1))
            {
                flushSizeBytes = pQ->capacity * sizeof(LwU32);
                newDequeueIndex = 0;
                newEnqueueIndex = 0;
            }
            else
            {
                flushSizeBytes = halfBufferSize * sizeof(LwU32);
                newDequeueIndex = halfBufferSize;
                if (pQ->enqueueIndex == (halfBufferSize - 1))
                {
                    newEnqueueIndex = halfBufferSize;
                }
                else
                {
                    newEnqueueIndex = pQ->enqueueIndex;
                }
            }
        }
        else
        {
            if (
                (pQ->dequeueIndex == halfBufferSize) &&
                (
                    (pQ->enqueueIndex < pQ->dequeueIndex) ||
                    (pQ->enqueueIndex == (pQ->capacity - 1))
                )
            )
            {
                pLocalBufferStart = (void*) (pQ->pBuffer + halfBufferSize);
                // This condition is true when we have are wrapped around and full
                if (pQ->enqueueIndex == (halfBufferSize - 1))
                {
                    flushSizeBytes = pQ->capacity * sizeof(LwU32);
                    newEnqueueIndex = 0;
                    newDequeueIndex = 0;
                }
                else
                {
                    flushSizeBytes = halfBufferSize * sizeof(LwU32);
                    newDequeueIndex = 0;
                    if (pQ->enqueueIndex == (pQ->capacity - 1))
                    {
                        newEnqueueIndex = 0;
                    }
                    else
                    {
                        newEnqueueIndex = pQ->enqueueIndex;
                    }
                }
            }
        }


        pQ->filledDwords -= flushSizeBytes / sizeof(LwU32);

        if (flushSizeBytes > 0)
        {
            LwU32 flushFirstPageNumber;
            LwU32 totalOffset;

            if (LwU64_ALIGN32_IS_ZERO(&(pQ->fbQueueMemDesc.address)))
            {
                //
                // Address is NULL, so we haven't been initialized yet!
                // No need for a breakpoint here, as the program is expected to
                // continue successfully.
                //
                status = FLCN_ERROR;
                goto s_lwosUstreamerFlush_fail;
            }

            flushFirstPageNumber = *((LwU32*)(pLocalBufferStart));
            totalOffset = ((flushFirstPageNumber - 1) % pQ->numberOfPages) *
                halfBufferSize * sizeof(LwU32);

            if ((pLocalBufferStart != pQ->pBuffer) &&
                (flushSizeBytes == pQ->capacity * sizeof(LwU32)))
            {
                status = dmaWrite(pLocalBufferStart, &pQ->fbQueueMemDesc, totalOffset,
                    halfBufferSize * sizeof(LwU32));
                if (status != FLCN_OK)
                    goto s_lwosUstreamerFlush_fail;

                flushSizeBytes = halfBufferSize * sizeof(LwU32);
                pLocalBufferStart = (void*)pQ->pBuffer;
                flushFirstPageNumber = *((LwU32*)(pLocalBufferStart));
                totalOffset = ((flushFirstPageNumber - 1) % pQ->numberOfPages) *
                    halfBufferSize * sizeof(LwU32);
            }

            status = dmaWrite(pLocalBufferStart, &pQ->fbQueueMemDesc, totalOffset,
                flushSizeBytes);
            if (status != FLCN_OK)
                goto s_lwosUstreamerFlush_fail;

            pQ->dequeueIndex = newDequeueIndex;
            pQ->enqueueIndex = newEnqueueIndex;
            pQ->lostEventCount = 0;
        }
    } // lwrtos_CRITICAL
s_lwosUstreamerFlush_fail:
    if (bEnterCritical)
    {
        if (bIsQueueResident)
        {
            lwrtosEXIT_CRITICAL();
        }
        else
        {
            appTaskCriticalExit();
        }
    }
s_lwosUstreamerFlush_fail_no_critical:
    return status;
}

/*!
 * @brief   internal logging function that handles all logging requests.
 *
 * This will handle both fixed and variable length event. The caller should
 * determine if critical section is needed.
 *
 * @param[in]   queueId
 *      Id of the queue that event should be inserted into.
 * @param[in]   eventId
 *      Up to 8 bits of eventId for an variable length event and up to 5 bits for
 *      a fixed length event.
 * @param[in]   pPayload
 *      Pointer to the data that should be inserted into the event as payload.
 *      This is limited to 3 bytes for a compact event.
 * @param[in]   payloadSize
 *      Size of the payload buffer pPayload.
 * @param[in]   bIsCompact
 *      Should the event be inserted as a compact event or not.
 * @param[in]   bEnterCritical
 *      Should this function enter critical section before operating on queues.
 *
 * @return FLCN_OK
 *      Log completed successfully.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *      Invalid argument is supplied. Check if queueId is valid and payload size
 *      is in range for specified event type.
 */
FLCN_STATUS
s_lwosUstreamerLog
(
    LwU8    queueId,
    LwU8    eventId,
    LwU8   *pPayload,
    LwU32   payloadSize,
    LwBool  bIsCompact,
    LwBool  bEnterCritical
)
{
    FLCN_STATUS                     status = FLCN_OK;
    LwBool                          bIsQueueResident;
    LwU32                           tmpData;
    LWOS_USTREAMER_QUEUE_METADATA  *pQ;

    if ((queueId == ustreamerDefaultQueueId) &&
        !LWOS_BM_GET(LwosUstreamerEventFilter.mask,
                     EVENT_ID_MASK_IDX(eventId, bIsCompact),
                     8))
    {
        // Ignore event since this type id is being filtered out. Not an error.
        goto s_lwosUstreamerLog_fail;
    }

    pQ = s_lwosUstreamerGetQueue(queueId);
    if (pQ == NULL)
    {
        OS_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_lwosUstreamerLog_fail;
    }

    bIsQueueResident = pQ->bResident;

    if ((!bIsCompact) && (payloadSize == 0))
    {
        OS_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_lwosUstreamerLog_fail;
    }

    if (bIsCompact &&
        (payloadSize > USTREAER_EVENT_PAYLOADCOMPACT_SIZE_BYTES))
    {
        OS_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_lwosUstreamerLog_fail;
    }

    if (bEnterCritical)
    {
        if (bIsQueueResident)
            lwrtosENTER_CRITICAL();
        else
        {
            // non-resident queue can only be logged from task.
            if (!lwrtosIS_KERNEL())
            {
                appTaskCriticalEnter();
            }
            else
            {
                status = FLCN_ERR_ILWALID_STATE;
                goto s_lwosUstreamerLog_fail;
            }
        }
    }
    {
        // Insert event head.
        status = s_lwosUstreamerInsertDWORD(pQ, 0, LW_TRUE, LW_TRUE);
        if (status != FLCN_OK)
        {
            goto s_lwosUstreamerLog_earlyExitCriticalSection;
        }

        if (bIsCompact)
        {
            LwU32 payload = *pPayload;
            if (payloadSize > 1)
            {
                payload |= (LwU32)(*(pPayload + 1)) << 8U;
            }
            if (payloadSize > 2)
            {
                payload |= (LwU32)(*(pPayload + 2)) << 16U;
            }
            tmpData = DRF_NUM(2080_CTRL_FLCN_USTREAMER_EVENT, _COMM, _FLAG, 1) |
                      DRF_NUM(2080_CTRL_FLCN_USTREAMER_EVENT, _TAIL, _EVENTIDCOMPACT, eventId) |
                      DRF_NUM(2080_CTRL_FLCN_USTREAMER_EVENT, _TAIL, _PAYLOADCOMPACT, payload);
            status = s_lwosUstreamerInsertDWORD(pQ, tmpData, LW_FALSE, LW_TRUE);
            if (status != FLCN_OK)
            {
                goto s_lwosUstreamerLog_earlyExitCriticalSection;
            }
        }
        else
        {
            LwU32   packedBytes                         = 0;
            LwU8    lwrrentEventPayloadCountInDwords    = 0;
            LwU32   lwrrentHighestBitPacked             = 0;
            LwU32   highestBitPacking                   = 0;

            while (packedBytes < payloadSize)
            {
                LwU32   lwrrentDword = 0;
                LwBool  bEventTermination;

                LwU8    i;
                for (i = 0; i < sizeof(LwU32); i++)
                {
                    if (packedBytes >= payloadSize)
                        break;
                    lwrrentDword |= ((LwU32)pPayload[packedBytes++]) << (i * 8);
                }

                tmpData = DRF_NUM(2080_CTRL_FLCN_USTREAMER_EVENT, _DATA, _PAYLOAD, lwrrentDword);
                status = s_lwosUstreamerInsertDWORD(pQ, tmpData, LW_FALSE, LW_TRUE);
                if (status != FLCN_OK)
                {
                    goto s_lwosUstreamerLog_earlyExitCriticalSection;
                }

                highestBitPacking = (highestBitPacking << 1U) |
                    DRF_VAL(2080_CTRL_FLCN_USTREAMER_EVENT, _COMM, _FLAG, lwrrentDword);
                lwrrentEventPayloadCountInDwords++;
                lwrrentHighestBitPacked++;

                if (lwrrentHighestBitPacked > DRF_SIZE(LW2080_CTRL_FLCN_USTREAMER_EVENT_DATA_PAYLOAD))
                {
                    // Highest bit packing overrun. This should never happen.
                    dbgPrintf(LEVEL_CRIT, "uStreamer: highestBitPacking overrun\n");
                    goto s_lwosUstreamerLog_earlyExitCriticalSection;
                }

                bEventTermination = (packedBytes >= payloadSize) ||
                    (lwrrentEventPayloadCountInDwords == DRF_MASK(LW2080_CTRL_FLCN_USTREAMER_EVENT_TAIL_LENGTH));

                if ((lwrrentHighestBitPacked == DRF_SIZE(LW2080_CTRL_FLCN_USTREAMER_EVENT_DATA_PAYLOAD)) ||
                    (bEventTermination &&
                        (lwrrentHighestBitPacked > DRF_SIZE(LW2080_CTRL_FLCN_USTREAMER_EVENT_TAIL_PAYLOAD))
                    ))
                {
                    status = s_lwosUstreamerInsertDWORD(pQ,
                        DRF_NUM(2080_CTRL_FLCN_USTREAMER_EVENT, _DATA, _PAYLOAD, highestBitPacking),
                        LW_FALSE, LW_TRUE);
                    if (status != FLCN_OK)
                    {
                        goto s_lwosUstreamerLog_earlyExitCriticalSection;
                    }
                    lwrrentHighestBitPacked = 0;
                    highestBitPacking = 0;
                }

                if (bEventTermination)
                {
                    tmpData = DRF_NUM(2080_CTRL_FLCN_USTREAMER_EVENT, _COMM, _FLAG, 1) |
                              DRF_NUM(2080_CTRL_FLCN_USTREAMER_EVENT, _TAIL, _VARIABLE, 1) |
                              DRF_NUM(2080_CTRL_FLCN_USTREAMER_EVENT, _TAIL, _EXTEND, packedBytes < payloadSize) |
                              DRF_NUM(2080_CTRL_FLCN_USTREAMER_EVENT, _TAIL, _EVENTID, eventId) |
                              DRF_NUM(2080_CTRL_FLCN_USTREAMER_EVENT, _TAIL, _LENGTH, lwrrentEventPayloadCountInDwords) |
                              DRF_NUM(2080_CTRL_FLCN_USTREAMER_EVENT, _TAIL, _PAYLOAD, highestBitPacking);
                    status = s_lwosUstreamerInsertDWORD(pQ, tmpData, LW_FALSE, LW_TRUE);
                    if (status != FLCN_OK)
                    {
                        goto s_lwosUstreamerLog_earlyExitCriticalSection;
                    }
                    lwrrentHighestBitPacked = 0;
                    highestBitPacking = 0;
                }
            }
        }

// We jump to here when the queue is both full and flushing is not possible.
s_lwosUstreamerLog_earlyExitCriticalSection:
        lwosNOP();
    }
    if (bEnterCritical)
    {
        if (bIsQueueResident)
        {
            lwrtosEXIT_CRITICAL();
        }
        else
        {
            appTaskCriticalExit();
        }
    }

s_lwosUstreamerLog_fail:
    return status;
}
