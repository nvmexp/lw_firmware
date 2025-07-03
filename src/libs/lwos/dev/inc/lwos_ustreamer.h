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
 * @file    lwos_ustreamer.h
 * @brief   Defines used by uStreamer code.
 */

#ifndef LWOS_USTREAMER_H
#define LWOS_USTREAMER_H

#include "lwrtos.h"
#include "lwtypes.h"
#include "lwuproc.h"
#include "ctrl/ctrl2080/ctrl2080flcn.h"
#include "rmustreamerif.h"

/* ------------------------------- Safety Check ------------------------------ */
// This ensures uStreamer and Instrumentation v1 are never both enabled.
#if defined(INSTRUMENTATION) && defined(USTREAMER)
    #error "Both Instrumentation and uStreamer are enabled. This is not allowed"
#endif

/* ------------------------------- Defines ---------------------------------- */
#ifdef USTREAMER
    #define LWOS_USTREAMER_IS_ENABLED() LW_TRUE
#else // USTREAMER
    #define LWOS_USTREAMER_IS_ENABLED() LW_FALSE
#endif // USTREAMER

#ifdef USTREAMER_PROTECT
    #define LWOS_USTREAMER_IS_PROTECTED() LW_TRUE
#else // USTREAMER_PROTECT
    #define LWOS_USTREAMER_IS_PROTECTED() LW_FALSE
#endif // USTREAMER_PROTECT

#ifdef USTREAMER_PTIMER
    #define LWOS_USTREAMER_USE_PTIMER() LW_TRUE
#else // LWOS_USTREAMER_USE_PTIMER
    #define LWOS_USTREAMER_USE_PTIMER() LW_FALSE
#endif // LWOS_USTREAMER_USE_PTIMER

/* ------------------------ Type / Structures ------------------------------- */
/* ------------------------ Public Variables -------------------------------- */
#ifdef USTREAMER
//! Implemented by each client to hold an id for the default queue.
extern LwU8 ustreamerDefaultQueueId;
#else // USTREAMER
#define ustreamerDefaultQueueId 0
#endif // USTREAMER

extern LW2080_CTRL_FLCN_USTREAMER_EVENT_FILTER LwosUstreamerEventFilter;

/* ------------------------ Function Prototypes ----------------------------- */
/*!
 * @brief   uStreamer queue count callback function.
 *
 * This function must be implemented by each client of the uStreamer. Client
 * here is defined as a uController using a binary. E.g. PMU as a whole would
 * count as one client. It will be called during initialization of uStreamer to
 * create an array that holds metadata about each queue.
 *
 * @return  The max number of queues the client will use.
 */
LwU8 lwosUstreamerGetQueueCount(void)
    GCC_ATTRIB_SECTION("imem_ustreamerInit", "lwosUstreamerGetQueueCount");

/*!
 * @brief   uStreamer get current active queue count.
 *
 * @return  The number of queues that are lwrrently registered.
 */
LwU8 lwosUstreamerActiveQueueCountGet(void)
    GCC_ATTRIB_SECTION("imem_ustreamerRuntime", "lwosUstreamerActiveQueueCountGet");

/*!
 * @brief   uStreamer get information about a queue.
 *
 * @param[in]   queueId
 *      Id of the queue to query information about.
 * @param[out]  pQueueDescriptor
 *      Pointer to the queue descriptor where info should be populated into.
 *
 * @return FLCN_OK      Queue info populated with the specified queueId.
 * @return FLCN_ERROR   Specified queue does not exist.
 */
FLCN_STATUS lwosUstreamerGetQueueInfo(LwU8 queueId,
    RM_FLCN_USTREAMER_QUEUE_DESCRIPTOR *pQueueDescriptor)
    GCC_ATTRIB_SECTION("imem_ustreamerRuntime", "lwosUstreamerGetQueueInfo");

/*!
 * @brief   uStreamer initialization routine.
 *
 * This function should be called once during initialization. It must be called
 * while allocation is still availiable, and before the first call to
 * lwosUstreamerQueueConstruct.
 *
 * @return FLCN_OK    uStreamer initialization complete.
 */
FLCN_STATUS lwosUstreamerInitialize(void)
    GCC_ATTRIB_SECTION("imem_ustreamerInit", "lwosUstreamerInitialize");

// TODO: yaotianf: add support for flushing policy.
/*!
 * @brief   Initialize a queue using the provided information.
 *
 * This should be called only once for each queue. The invocation should happen
 * before the first logging request.
 *
 * @param[in]  pLocalBuffer
 *      Pointer to the local buffer uStreamer should use for the queue. The
 *      buffer and half buffer point (for default flushing policy) should be
 *      both aligned to the DMA requirement.
 * @param[in]  localBufferSize
 *      Size of the local buffer in bytes.
 * @param[in]  pBaseMemDesc
 *      Pointer to a RM supersurface memory descriptor.
 * @param[in]  fbBufferOffset
 *      Offset of the queue buffer inside the RM Supersurface.
 * @param[in]  fbBufferSize
 *      Size of the queue buffer inside the RM Supersurface.
 * @param[in]  queueFeatureId
 *      A constant value uniquely identifying the queue. This value should be
 *      defined in ctrl2080flcn.h. @ref LW2080_CTRL_FLCN_USTREAMER_FEATURE
 * @param[in]  bResident
 *      LW_TRUE if the local buffer passed in is resident.
 *
 * @pre Both the local and fb buffer has to be aligned to at least
 *      DMA_XFER_SIZE_MIN_READ/WRITE. This is to ensure a successful DMA transfer.
 *
 * @return FLCN_OK
 *      Queue construction success.
 * @return FLCN_ERR_ILWALID_OPERATION
 *      Failed to construct due to invalid state. (Adding more queue than declared)
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *      Invalid local buffer or FB buffer.
 */
FLCN_STATUS lwosUstreamerQueueConstruct(LwU32 *pLocalBuffer, LwU32 localBufferSize,
    RM_FLCN_MEM_DESC *pBaseMemDesc, LwU32 fbBufferOffset, LwU32 fbBufferSize,
    LwU8 *pQueueId, LwU8 queueFeatureId, LwBool bResident)
    GCC_ATTRIB_SECTION("imem_ustreamerInit", "lwosUstreamerQueueConstruct");

/*!
 * @brief   Shuts down the uStreamer. No more logging is allowed afterwards.
 *
 * @return FLCN_OK shutdown complete.
 */
FLCN_STATUS lwosUstreamerShutdown(void)
    GCC_ATTRIB_SECTION("imem_ustreamerInit", "lwosUstreamerShutdown");


/*!
 * @brief   Insert a variable sized event.
 *
 * @param[in] queueId   Index of queue for event insertion.
 * @param[in] eventId   Event identifier, up to 8 bits. Id 0 is reserved.
 * @param[in] pPayload  Pointer to payload buffer.
 * @param[in] size      Payload size in unit of bytes Note: the value
 *                      passed into this field is only used for event
 *                      construction, the event received on the SRT side might
 *                      be rounded up to the next dword in size. Padding bytes
 *                      will always be zero.
 *
 * @return FLCN_OK      Logging completed successfully.
 * @return FLCN_ERROR   Logging operation failed.
 */
FLCN_STATUS lwosUstreamerLog(LwU8 queueId, LwU8 eventId, LwU8 *pPayload, LwU32 size)
    GCC_ATTRIB_SECTION("imem_ustreamerRuntime", "lwosUstreamerLog");

/*!
 * @brief   Insert a fixed-size (3 bytes) event.
 *
 * @param[in] queueId   Index of queue for event insertion.
 * @param[in] eventId   Event identifier, 0 is reserved, and only lower 5 bits
 *                      are valid. [1,31]
 * @param[in] payload   Three bytes of payload. Only the lower 24 bits are used.
 *
 * @return FLCN_OK      Logging completed successfully.
 * @return FLCN_ERROR   Logging operation failed.
 */
FLCN_STATUS lwosUstreamerLogCompact(LwU8 queueId, LwU8 eventId, LwU32 payload)
    GCC_ATTRIB_SECTION("imem_ustreamerRuntime", "lwosUstreamerLogCompact");

/*!
 * @brief   Insert a variable sized event. Only call from ISR
 *
 * @param[in] queueId   Index of queue for event insertion. The queue must
 *                      use the default flushing policy.
 * @param[in] eventId   Event identifier, up to 8 bits. 0 is reserved.
 * @param[in] pPayload  Pointer to payload buffer.
 * @param[in] size      Payload size in unit of bytes. Note: the value
 *                      passed into this field is only used for event
 *                      construction, the event received on the SRT side might
 *                      be rounded up to the next dword in size. (Padding is
 *                      always zero.)
 *
 * @return FLCN_OK      Logging completed successfully.
 * @return FLCN_ERROR   Logging operation failed.
 */
FLCN_STATUS lwosUstreamerLogFromISR(LwU8 queueId, LwU8 eventId, LwU8 *pPayload, LwU32 size)
    GCC_ATTRIB_SECTION("imem_ustreamerRuntime", "lwosUstreamerLogFromISR");

/*!
 * @brief   Insert a fix-sized (3 bytes) event. Only call from ISR
 *
 * @param[in] queueId   Index of queue for event insertion
 * @param[in] eventId   Event identifier, 0 is reserved, and only lower 5 bits
 *                      are valid. [1,31]
 * @param[in] payload   Three bytes of payload. Only the lower 24 bits are used.
 *
 * @return FLCN_OK      Logging completed successfully.
 * @return FLCN_ERROR   Logging operation failed.
 */
FLCN_STATUS lwosUstreamerLogCompactFromISR(LwU8 queueId, LwU8 eventId, LwU32 payload)
    GCC_ATTRIB_SECTION("imem_ustreamerRuntime", "lwosUstreamerLogCompactFromISR");

/*!
 * @brief   Request an immediate flush for a certain queue
 *
 * @param[in] queueId   QueueId to execute immediate flush on.
 *
 * @return FLCN_OK      Flush exelwted without error. Note: when exelwted on an
 *                      empty queue, it will not flush but still return FLCN_OK
 * @return FLCN_ERROR   Failed to flush events to framebuffer.
 */
FLCN_STATUS lwosUstreamerFlush(LwU8 queueId)
    GCC_ATTRIB_SECTION("imem_ustreamerRuntime", "lwosUstreamerFlush");

/*!
 * @brief   Callback hook for uStreamer during IDLE task.
 */
void lwosUstreamerIdleHook(void)
    GCC_ATTRIB_SECTION("imem_ustreamerRuntime", "lwosUstreamerIdleHook");

/*!
 * @brief   Retrieve the uStreamer requested queue's event filter.
 *
 * @return  The current event filter.
 */
FLCN_STATUS lwosUstreamerGetEventFilter(LwU8 queueId, LW2080_CTRL_FLCN_USTREAMER_EVENT_FILTER *pEventFilter)
     GCC_ATTRIB_SECTION("imem_ustreamerRuntime", "lwosUstreamerGetEventFilter");

/*!
 * @brief   Set the uStreamer specified queue's event filter to the requested one.
 *
 * @param[in] eventFilter   The new desired event filter.
 */
FLCN_STATUS lwosUstreamerSetEventFilter(LwU8 queueId, LW2080_CTRL_FLCN_USTREAMER_EVENT_FILTER *pEventFilter)
     GCC_ATTRIB_SECTION("imem_ustreamerRuntime", "lwosUstreamerSetEventFilter");

/*!
 * @defgroup LWOS_USTREAMER function wrappers.
 *
 * @note for all the logging functions, this will cast away the status value,
 * as it is often not used when logging is optional. If the status/success is
 * required, please call the logging functions directly. These wrappers are
 * meant for colwenience, i.e. not needing to check for uStreamer enablement,
 * and use-cases that rely on the success of these function calls will likely
 * already be checking that uStreamer is enabled anyways.
 *
 * @{
 */
#ifdef USTREAMER
#define LWOS_USTREAMER_LOG(_queueId, _eventId, _pPayload, _size)               \
    (void)lwosUstreamerLog((_queueId), (_eventId), (_pPayload), (_size))

#define LWOS_USTREAMER_LOG_COMPACT(_queueId, _eventId, _payload)               \
    (void)lwosUstreamerLogCompact((_queueId), (_eventId), (_payload))

#define LWOS_USTREAMER_LOG_COMPACT_LWR_TASK(_queueId, _eventId)                \
    (void)lwosUstreamerLogCompact((_queueId), (_eventId), osTaskIdGet())

#define LWOS_USTREAMER_LOG_FROM_ISR(_queueId, _eventId, _pPayload, _size)      \
    (void)lwosUstreamerLogFromISR((_queueId), (_eventId), (_pPayload), (_size))

#define LWOS_USTREAMER_LOG_COMPACT_FROM_ISR(_queueId, _eventId, _payload)      \
    (void)lwosUstreamerLogCompactFromISR((_queueId), (_eventId), (_payload))

#define LWOS_USTREAMER_FLUSH(_queueId)                                         \
    lwosUstreamerFlush((_queueId))

#define LWOS_USTREAMER_ACTIVE_QUEUE_COUNT_GET()                                \
    lwosUstreamerActiveQueueCountGet()

#define LWOS_USTREAMER_GET_QUEUE_INFO(_queueId, _pQueueDescriptor)             \
    lwosUstreamerGetQueueInfo((_queueId), (_pQueueDescriptor))

#else // USTREAMER

#define LWOS_USTREAMER_LOG(_queueId, _eventId, _pPayload, _size)               \
    lwosVarNOP((_queueId), (_eventId), (_pPayload), (_size))

#define LWOS_USTREAMER_LOG_COMPACT(_queueId, _eventId, _payload)               \
    lwosVarNOP((_queueId), (_eventId), (_payload))

#define LWOS_USTREAMER_LOG_COMPACT_LWR_TASK(_queueId, _eventId)                \
    lwosVarNOP((_queueId), (_eventId))

#define LWOS_USTREAMER_LOG_FROM_ISR(_queueId, _eventId, _pPayload, _size)      \
    lwosVarNOP((_queueId), (_eventId), (_pPayload), (_size))

#define LWOS_USTREAMER_LOG_COMPACT_FROM_ISR(_queueId, _eventId, _payload)      \
    lwosVarNOP((_queueId), (_eventId), (_payload))

#define LWOS_USTREAMER_FLUSH(_queueId)                                         \
    FLCN_OK

#define LWOS_USTREAMER_ACTIVE_QUEUE_COUNT_GET()                                \
    0U

#define LWOS_USTREAMER_GET_QUEUE_INFO(_queueId, _pQueueDescriptor)             \
    FLCN_ERROR

#endif // USTREAMER
/*!@}*/

#endif // LWOS_USTREAMER_H
