/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    changeseq.h
 * @brief   Common perf change sequence related defines.
 */

#ifndef CHANGESEQ_H
#define CHANGESEQ_H

#include "g_changeseq.h"

#ifndef G_CHANGESEQ_H
#define G_CHANGESEQ_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct CHANGE_SEQ                       CHANGE_SEQ;
typedef struct CHANGE_SEQ_SCRIPT                CHANGE_SEQ_SCRIPT;
typedef struct PERF_CHANGE_SEQ_NOTIFICATION     PERF_CHANGE_SEQ_NOTIFICATION;
typedef struct CHANGE_SEQ_LPWR                  CHANGE_SEQ_LPWR;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief Accessor to retrieve a pointer to the global CHANGE_SEQ object.
 * @memberof CHANGE_SEQ
 * @public
 *
 * @return CHANGE_SEQ object pointer
 * @return NULL if the CHANGE_SEQ object does not exist
 */
#define PERF_CHANGE_SEQ_GET()                       (Perf.changeSeqs.pChangeSeq)

/*!
 * @brief Checks if the input change is valid.
 * @memberof CHANGE_SEQ
 * @public
 *
 * @param[in]  pChange  Pointer to change to validate (type
 *                      @ref LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE).
 *
 * @return LW_TRUE if the input change is valid
 * @return LW_FALSE if the input change is not valid
 */
#define PERF_CHANGE_SEQ_IS_CHANGE_VALID(pChange)                              \
    ((pChange)->pstateIndex != LW2080_CTRL_PERF_PSTATE_INDEX_ILWALID)

/*!
 * @brief Set the input change as invalid.
 * @memberof CHANGE_SEQ
 * @public
 *
 * @param[out]  pChangeSeq  CHANGE_SEQ pointer to mark as invalid
 * @param[in]   pChange     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE pointer
 *
 * @post @a pChangeSeq is marked as invalid
 */
#define PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(pChangeSeq, pChange)                \
do {                                                                          \
        (void)memset((pChange), 0, (pChangeSeq)->changeSize);                 \
        ((pChange)->pstateIndex = LW2080_CTRL_PERF_PSTATE_INDEX_ILWALID);     \
} while (LW_FALSE)

/*!
 * @brief Base set of common CHANGE_SEQ overlays (both IMEM and DMEM).
 * @memberof CHANGE_SEQ
 * @public
 *
 * Expected to be used with @ref OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER() and
 * @ref OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT().
 */
#define CHANGE_SEQ_OVERLAYS_BASE                                              \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfDaemon)                         \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfChangeSeqClient)                \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libChangeSeq)

/*!
 * @brief Set of overlays required for loading the change sequencer via the RPC
 * (both IMEM and DMEM).
 * @memberof CHANG_SEQ
 * @public
 *
 * Expected to be used with @ref OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER() and
 * @ref OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT().
 */
#define CHANGE_SEQ_OVERLAYS_LOAD                                              \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfDaemon)                         \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfChangeSeqClient)                \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, changeSeqLoad)

/*!
 * @brief Gets the last set frequency for given clock domain.
 * @memberof CHANGE_SEQ
 * @public
 *
 * @param[in]  clkIdx  Clock Domain Index
 *
 * @return Clock domain frequency if the input is valid clock domain
 * @return LW_U32_MIN otherwise
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ))
#define perfChangeSeqChangeLastClkFreqkHzGet(clkIdx)                          \
    ((((Perf.changeSeqs.pChangeLast) != NULL) &&                              \
     (BOARDOBJGRP_IS_VALID(CLK_DOMAIN, (clkIdx)))) ?                          \
    (Perf.changeSeqs.pChangeLast->clkList.clkDomains[(clkIdx)].clkFreqKHz) :  \
    LW_U32_MIN)
#else
#define perfChangeSeqChangeLastClkFreqkHzGet(clkIdx) LW_U32_MIN
#endif

/*!
 * @brief Gets the last set voltage for given volt rail.
 * @memberof CHANGE_SEQ
 * @public
 *
 * @param[in] railIdx  Voltage rail Index
 *
 * @return Voltage if the input is valid volt rail index
 * @return RM_PMU_VOLT_VALUE_0V_IN_UV_SIGNED otherwise
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ))
#define perfChangeSeqChangeLastVoltageuVGet(railIdx)                           \
    ((((Perf.changeSeqs.pChangeLast) != NULL) &&                               \
      (VOLT_RAIL_INDEX_IS_VALID(railIdx))) ?                                   \
    (Perf.changeSeqs.pChangeLast->voltList.rails[(railIdx)].voltageuV) :       \
    (RM_PMU_VOLT_VALUE_0V_IN_UV_SIGNED))
#else
#define perfChangeSeqChangeLastVoltageuVGet(railIdx)                           \
            RM_PMU_VOLT_VALUE_0V_IN_UV_SIGNED
#endif

/*!
 * @brief Gets the last set volt offset for given volt rail.
 * @memberof CHANGE_SEQ
 * @public
 *
 * @todo PP-TODO : This is not sync using PERF R-W Semaphore. So if client is
 * outside PERF Task (e.g. PMGR Task), it needs to request synchronization
 * support.
 *
 * @param[in] railIdx  Voltage rail Index
 * @param[in] ctrlId   Voltage Offset controller Id.
 *                     @ref LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_<xyz>
 *
 * @return Volt offset if the input is valid volt rail index and controller ID
 * @return RM_PMU_VOLT_VALUE_0V_IN_UV_SIGNED otherwise
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ))
#define perfChangeSeqChangeLastVoltOffsetuVGet(railIdx, ctrlId)                       \
((((Perf.changeSeqs.pChangeLast) != NULL) &&                                          \
        (VOLT_RAIL_INDEX_IS_VALID(railIdx)) &&                                        \
        ((ctrlId) < LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_MAX)) ?                         \
    (Perf.changeSeqs.pChangeLast->voltList.rails[(railIdx)].voltOffsetuV[(ctrlId)]) : \
    (RM_PMU_VOLT_VALUE_0V_IN_UV_SIGNED))
#else
#define perfChangeSeqChangeLastVoltOffsetuVGet(railIdx, ctrlId)                       \
            RM_PMU_VOLT_VALUE_0V_IN_UV_SIGNED
#endif

/*!
 * @brief Gets the last set pstate index.
 * @memberof CHANGE_SEQ
 * @public
 *
 * @return Pstate index of last set pstate
 * @return LW2080_CTRL_PERF_PSTATE_INDEX_ILWALID otherwise
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ))
#define perfChangeSeqChangeLastPstateIndexGet()                               \
    ((((Perf.changeSeqs.pChangeLast) != NULL) &&                              \
      ((PSTATE_GET(Perf.changeSeqs.pChangeLast->pstateIndex)) != NULL)) ?     \
     (Perf.changeSeqs.pChangeLast->pstateIndex) :                             \
     LW2080_CTRL_PERF_PSTATE_INDEX_ILWALID)
#else
#define perfChangeSeqChangeLastPstateIndexGet()                               \
    LW2080_CTRL_PERF_PSTATE_INDEX_ILWALID
#endif

/*!
 * @brief Gets the last set tFAW settings.
 * @memberof CHANGE_SEQ
 * @public
 *
 * @return tFAW of last set perf change.
 * @return LW_U8_MIN otherwise
 */
#define perfChangeSeqChangeLasttFAWGet()                            \
    (((Perf.changeSeqs.pChangeLast) != NULL) ?                      \
        (Perf.changeSeqs.pChangeLast->tFAW) : LW_U8_MIN)

/*!
 * @brief Initializes PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE structure.
 * @memberof CHANGE_SEQ
 * @public
 *
 * @param[out] _pRequest  Pointer to PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE
 *                        change request.
 * @param[in]  _clientId  [RM|PMU] shorthand notation for
 *                        [@ref LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_RM|
 *                         @ref LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_PMU].
 */
#define PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE_INIT(_pRequest, _clientId)             \
do {                                                                                    \
    (void)memset(_pRequest, 0, sizeof(PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE));       \
    (_pRequest)->clientId = LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_##_clientId; \
    (_pRequest)->seqId    = LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_SEQ_ID_ILWALID;          \
} while (LW_FALSE)

/*!
 * @brief Gets the byte offset of the change sequencer super-surface's field.
 * @memberof CHANGE_SEQ
 * @public
 *
 * @param[in] _script  Change Sequencer script name in super surface struct.
 * @param[in] _param   Change Sequencer script field name in script struct.
 *
 * @return The offset into the super surface where the change sequencer script
 *         field resides.
 */
#define CHANGE_SEQ_SUPER_SURFACE_OFFSET_BY_SCRIPT_NAME(_script, _param)        \
     (RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(_script) +                            \
        LW_OFFSETOF(RM_PMU_PERF_CHANGE_SEQ_PMU_SCRIPT, _param))

/*!
 * @brief Gets the byte offset of the change sequencer super-surface's field.
 * @memberof CHANGE_SEQ
 * @public
 *
 * @param[in] _scriptOffset  Change Sequencer script offset in surface struct.
 * @param[in] _param         Change Sequencer script field name in script struct.
 *
 * @return The offset into the super surface where the change sequencer script
 *         field resides.
 */
#define CHANGE_SEQ_SUPER_SURFACE_OFFSET_BY_SCRIPT_OFFSET(_scriptOffset, _param) \
     ((_scriptOffset) + LW_OFFSETOF(RM_PMU_PERF_CHANGE_SEQ_PMU_SCRIPT, _param))

/*!
 * @brief Helper macro to set certain script step into stepIdExclusionMask.
 * @memberof CHANGE_SEQ
 * @public
 *
 * @param[in]   stepId  step Id to be excluded.
 *                      @ref LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_<XYZ>
 *
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ))
#define perfChangeSeqChangeStepIdExclusionMaskSet(stepId)                      \
    do                                                                         \
    {                                                                          \
        if (((PERF_CHANGE_SEQ_GET()) != NULL) &&                               \
            ((stepId) < LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_MAX_STEPS))    \
        {                                                                      \
            ((PERF_CHANGE_SEQ_GET())->stepIdExclusionMask |= LWBIT32(stepId)); \
        }                                                                      \
        else                                                                   \
        { /* Nothing to do as in pre silicon LPWR may not use change seq. */ } \
    } while (LW_FALSE)
#else
#define perfChangeSeqChangeStepIdExclusionMaskSet(stepId)   {}
#endif

/*!
 * @brief Helper macro to unset certain script step into stepIdExclusionMask.
 * @memberof CHANGE_SEQ
 * @public
 *
 * @param[in]   stepId  step Id to be included.
 *                      @ref LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_<XYZ>
 *
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ))
#define perfChangeSeqChangeStepIdExclusionMaskUnset(stepId)                     \
    do                                                                          \
    {                                                                           \
        if (((PERF_CHANGE_SEQ_GET()) != NULL) &&                                \
            ((stepId) < LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_MAX_STEPS))     \
        {                                                                       \
            ((PERF_CHANGE_SEQ_GET())->stepIdExclusionMask &= ~LWBIT32(stepId)); \
        }                                                                       \
        else                                                                    \
        { /* Nothing to do as in pre silicon LPWR may not use change seq. */ }  \
    } while (LW_FALSE)
#else
#define perfChangeSeqChangeStepIdExclusionMaskUnset(stepId)   {}
#endif

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief   Construct perf change sequence object.
 *
 * @param[out]  ppChangeSeq  double pointer to the CHANGE_SEQ data field to be
 *                           constructed.
 * @param[in]   size         size of the ppChangeSeq buffer.
 * @param[in]   ovlIdx       Dmem overlay index.
 *
 * @return FLCN_OK if the CHANGE_SEQ is constructed successfully
 * @return Detailed error code on error
 */
#define PerfChangeSeqConstruct(fname) FLCN_STATUS (fname)(CHANGE_SEQ **ppChangeSeq, LwU16 size, LwU8 ovlIdx)

/*!
 * @brief   Copy out perf change sequence object information to RM buffer.
 *
 * RM will use this information to validate the RM/PMU version and update the RM
 * perf change sequence object based on information received from PMU.
 *
 * @param[in]   pChangeSeq  CHANGE_SEQ pointer
 * @param[out]  pInfo       RM_PMU_PERF_CHANGE_SEQ_INFO_GET pointer
 *
 * @return FLCN_OK if the change sequencer object was read
 * @return Detailed error code on error
 */
#define PerfChangeSeqInfoGet(fname) FLCN_STATUS (fname)(CHANGE_SEQ *pChangeSeq, RM_PMU_PERF_CHANGE_SEQ_INFO_GET *pInfo)

/*!
 * @brief   Copy in perf change sequence object information send from RM/CPU.
 *
 * PMU will use this information to update the assumption made about RM/CPU
 * change sequence model. PMU will also initialize itself with the new
 * information received from RM about the FB surface.
 *
 * @param[in]   pChangeSeq  CHANGE_SEQ pointer
 * @param[in]   pInfo       RM_PMU_PERF_CHANGE_SEQ_INFO_SET pointer
 *
 * @return FLCN_OK if the change sequencer object info was set
 * @return Detailed error code on error
 */
#define PerfChangeSeqInfoSet(fname) FLCN_STATUS (fname)(CHANGE_SEQ *pChangeSeq, RM_PMU_PERF_CHANGE_SEQ_INFO_SET *pInfo)

/*!
 * @brief   Interface to lock/unlock perf change sequence.
 *
 * @note    This is a private interface of perf change sequence. Only change
 *          sequence code can call this interface.
 *
 * @param[in]   pChangeSeq  CHANGE_SEQ pointer
 * @param[in]   pLock       RM_PMU_PERF_CHANGE_SEQ_LOCK pointer
 *
 * @return FLCN_OK if the change sequencer was locked/unlocked
 * @return Detailed error code on error
 */
#define PerfChangeSeqLock(fname) FLCN_STATUS (fname)(CHANGE_SEQ *pChangeSeq, RM_PMU_PERF_CHANGE_SEQ_LOCK *pLock)

/*!
 * @brief   Interface to load/unload perf change sequence.
 *
 * @param[in]   pChangeSeq  CHANGE_SEQ pointer
 * @param[in]   bLoad       Cmd to load / unload perf change sequence
 *
 * @return FLCN_OK if the change sequencer was loaded/unloaded
 * @return Detailed error code on error
 */
#define PerfChangeSeqLoad(fname) FLCN_STATUS (fname)(CHANGE_SEQ *pChangeSeq, LwBool bLoad)

/*!
 * @brief   Interface to query perf change sequence status.
 *
 * @note    This is a private interface of perf change sequence. Only change
 *          sequence code can call this interface.
 *
 * @param[in]   pQuery  RM_PMU_PERF_CHANGE_SEQ_QUERY pointer
 * @param[in]   pBuf    RM provided temporary DMEM buffer (for DMA-s)
 */
#define PerfChangeSeqQuery(fname) FLCN_STATUS (fname)(RM_PMU_PERF_CHANGE_SEQ_QUERY *pQuery, void *pBuf)

/*!
 * @brief Struct containing client task info.
 *
 * Client of perf change sequence will share this details while registering for
 * perf change sequence notification.
 *
 * @note This structure is not used in automotive builds. The feature it is
 * associated with, PMU_PERF_CHANGE_SEQ_NOTIFICATION, is disabled.
 */
typedef struct
{
    /*!
     * @brief Task event id.
     *
     * @note This param will be the unique identifier of the linked list.
     *       It is not allowed to do multiple registeration using the same
     *       @ref taskEventId for the same notification id.
     */
    LwU8                taskEventId;

    /*!
     * @brief Task queue to be used when communicating with client.
     */
    LwrtosQueueHandle   taskQueue;
} PERF_CHANGE_SEQ_NOTIFICATION_CLIENT_TASK_INFO;

/*!
 * @brief Perf change sequence notification.
 *
 * Issues from perf change sequence to its clients.
 *
 * This structure defines the notification information that perf change seq
 * script execute interface will send to notify client about the exeuction
 * of different steps of perf change sequence script.
 *
 * @note This structure is not used in automotive builds. The feature it is
 * associated with, PMU_PERF_CHANGE_SEQ_NOTIFICATION, is disabled.
 */
struct PERF_CHANGE_SEQ_NOTIFICATION
{
    /*!
     * @copydoc PERF_CHANGE_SEQ_NOTIFICATION_CLIENT_TASK_INFO
     */
    PERF_CHANGE_SEQ_NOTIFICATION_CLIENT_TASK_INFO   clientTaskInfo;

    /*!
     * @brief Keeps track of the next listener in the active listener-list.
     */
    PERF_CHANGE_SEQ_NOTIFICATION                   *pNext;

    /*!
     * @brief Back up in-active listener-list.
     *
     * Perf task will always update this back up list next pointer. When perf
     * task receives completion signal from perf change sequence, it will update
     * the @ref pNextActive pointer to use the updated back up copy. This logic
     * will allow us to accept update request from clients while daemon task is
     * exelwting the change request.
     */
    PERF_CHANGE_SEQ_NOTIFICATION                   *pNextInactive;
};

/*!
 * @brief CHANGE_SEQ notification event Listener's tracking information.
 *
 * @note This structure is not used in automotive builds. The feature it is
 * associated with, PMU_PERF_CHANGE_SEQ_NOTIFICATION, is disabled.
 */
typedef struct
{
    /*!
     * @brief Tracks if @ref pNotification and @ref pNotificationInactive
     * are in sync with each other.
     */
    LwBool                          bInSync;
    /*!
     * @brief Active notification linked-list head.
     *
     * Perf daemon task will use it to send perf change sequence notifications.
     */
    PERF_CHANGE_SEQ_NOTIFICATION   *pNotification;

    /*!
     * @brief In-active notification linked-list head.
     *
     * Perf task will use it to update the notification linked-list based on
     * clients request.
     */
    PERF_CHANGE_SEQ_NOTIFICATION   *pNotificationInactive;
} PERF_CHANGE_SEQ_NOTIFICATIONS;

/*!
 * @brief PMU-specific data.
 */
typedef struct
{
    /*!
     * @brief Handle to the task queue.
     */
    LwrtosQueueHandle   queueHandle;
} PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE_DATA_PMU;

/*!
 * @brief Union of type-specific CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE data.
 */
typedef union
{
    PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE_DATA_PMU pmu;
} PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_DATA;

/*!
 * @brief Structure to store all the pending sync change requests callback
 * information.
 */
typedef struct
{
    /*!
     * @brief Perf change sequence id.
     *
     * It is assigned at the time of enqueue. After completion of each perf
     * change request, pmu perf change sequence will send notifications to all
     * the clients blocked on the sync perf change requests whose sequence id
     * is less than or equal to the latest completed change.
     */
    LwU32                                           seqId;

    /*!
     * @brief Client associated with the @ref seqId.
     *
     * PMU perf change sequence will use the client information to send the
     * async notifications to the client.
     */
    LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT  clientId;

    /*!
     * @brief Start time of PMU client's change request pass down from clients.
     */
    FLCN_TIMESTAMP                                  timeStart;

    /*!
     * @brief Version/type-specific data. Determined by @ref clientId.
     */
    PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_DATA         data;
} PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE;

/*!
 * @brief Structure representing the dynamic parameters associated with PMU
 * perf change sequence.
 */
typedef struct
{
    /*!
     * @brief State of the CHANGE_SEQ_PMU_SCRIPT object.
     *
     * Tracks where the CHANGE_SEQ is in-processing this script.
     */
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE    state;

    /*!
     * @brief Boolean indicating if there is any pending lock request.
     *
     * If the change sequence is in the middle of processing any change while
     * it receives request to lock change seq, it will set this flag to LW_TRUE.
     * When perf task receives a completion signal from perf daemon task, it
     * will check this flag. If the flag is set, it will send async message
     * to the client (RM) about the availability of lock and set the state to
     * LOCKED.
     */
    LwBool  bLockWaiting;

    /*!
     * @brief Global perf change sequence id counter.
     *
     * It will be incremented when new perf change req. is enqueued. The latest
     * counter value will be assigned to each input change request.
     *
     * After completion of each perf change request, pmu perf change sequence
     * will send notifications to all the clients blocked on the sync perf
     * change requests whose sequence id is less than or equal to the latest
     * completed change.
     */
    LwU32   seqIdCounter;

    /*!
     * @copydoc LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE
     *
     * The trick to remove something from the queue is by setting the client to
     * @ref LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_ILWALID
     *
     * @note - This struct is private to perf change sequence and clients of
     * perf change sequence MUST not access it for any purpose.
     */
    PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE
        syncChangeQueue[LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_QUEUE_SIZE];

    /*!
     * @brief Profiling data for perf change requests.
     */
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_CHANGE_PROFILING profiling;
} PERF_CHANGE_SEQ_PMU;

/*!
 * @brief Perf Change Sequence script dmem buffers.
 *
 * This structure will hold the DMEM buffers required by perf daemon task to
 * build and execute the perf change sequence script.
 *
 * @note This struct is private to perf daemon task. Other task should not
 *       access this structure.
 */
struct CHANGE_SEQ_SCRIPT
{
    /*!
     * @brief Offset of script buffers in the super surface buffer.
     * @private
     */
    LwU32 scriptOffsetLwrr;
    LwU32 scriptOffsetLast;
    LwU32 scriptOffsetQuery;

    /*!
     * @brief Mask of clock domain entries that needs to be programmed during
     * the perf change sequence script.
     *
     * VF switch change sequence script generation code will build this mask
     * based on all other masks as follows.
     *
     * @code
     * clkDomainsActiveMask =
     * ((progDomainsMask & ~clkDomainsExclusionMask) | clkDomainsInclusionMask)
     * @endcode
     *
     * Low Power GPC-RG script will hard code this mask to GPC clock domain.
     *
     * @protected
     */
    BOARDOBJGRPMASK_E32     clkDomainsActiveMask;

    /*!
     * @brief This buffer will be used to hold the current perf change script
     * header (@ref LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_HEADER).
     */
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_HEADER_ALIGNED    hdr;

    /*!
     * @brief Pointer to current step data.
     */
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_DATA_ALIGNED *pStepLwrr;

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_ODP))
    /*!
     * @ref LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_DATA
     */
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_DATA_ALIGNED
        steps[LW2080_CTRL_PERF_CHANGE_SEQ_SCRIPT_VF_SWITCH_MAX_STEPS];
#else
    /*!
     * @brief This buffer will be used to hold the current perf change script
     * step.
     *
     * As complete perf change sequence script is stored in FB surface, the
     * perf daemon task will R-U-W the current step using this buffer
     * (@ref LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_DATA).
     */
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_DATA_ALIGNED stepLwrr;
#endif
};

/*!
 * @brief PMU component responsible for programming the clocks and volt rails to
 * the desired values.
 *
 * The CHANGE_SEQ receives a set of clock, voltage, and P-state values from a
 * client (typically the arbiter) and is responsible for applying those values
 * to the hardware. To do so, the CHANGE_SEQ builds a set of steps, called a
 * CHANGE_SEQ_SCRIPT, to properly program the hardware. These steps are
 * dependent on other supported software features and the chip architecture.
 *
 * The CHANGE_SEQ divided across two tasks. One part resides in the PERF task,
 * where it responsible for queuing requests made by both PMU and RM clients.
 * The second part resides in the PERF_DAEMON task. This portion of the
 * CHANGE_SEQ is responsible for the actual programming of the clocks, voltages,
 * and P-states.
 */
struct CHANGE_SEQ
{
    /*!
     * @brief Structure verion.
     *
     * Perf Change Sequence Structure Version -
     * @ref LW2080_CTRL_PERF_CHANGE_SEQ_VERSION_<xyz>.
     *
     * @protected
     */
    LwU8                                    version;

    /*!
     * @brief Flag to specify whether to ignore validity checks of the target
     * V/F point.
     *
     * This flag allows external RM clients to set higher frequencies at a
     * particular voltage OR a lower voltage for a parilwlar frequency.
     *
     * Controlled by the Regkey : "RmVFPointCheckIgnore"
     *
     * Possible clients API : Perf Inject
     *
     * @protected
     */
    LwBool                                  bVfPointCheckIgnore;

    /*!
     * @brief Boolean tracking whether client explcitly requested to skip the
     *      voltage range trimming.
     *
     * This flag allows external RM clients like MODS to request voltage
     * outside of the Vmin - Vmax bounds.
     *
     * Controlled by the Regkey : "RmPerfChangeSeqOverride""
     *
     * @protected
     */
    LwBool                                  bSkipVoltRangeTrim;
    /*!
     * Current tFAW value to be programmed in HW
     */
    LwU8                                    tFAW;
    /*!
     * Boolean tracking whether first perf change completed.
     */
    LwBool                                  bFirstChangeCompleted;
    /*!
     * @ref LW2080_CTRL_PERF_CHANGE_SEQ_PMU
     *
     * @protected
     */
    PERF_CHANGE_SEQ_PMU                     pmu;

    /*!
     * Mask of the steps Ids that PMU is advertizing to CPU.
     * CPU must claim them back to PMU via SET_INFO.
     *
     * @protected
     */
    LwU32                                   cpuAdvertisedStepIdMask;

    /*!
     * @brief Mask of the steps Ids that CPU claimed based on available features.
     *
     * All the step IDs that were not claimed CPU will be consider NOP.
     * RM will set this via SET_INFO call to PMU.
     *
     * @protected
     */
    LwU32                                   cpuStepIdMask;

    /*!
     * @brief Perf change sequencer step id exclusion mask.
     *
     * Each bit represents bit position of
     * LW2080_CTRL_PERF_CHANGE_SEQ_<abc>_STEP_ID_<xyz>. Client could update
     * this mask to exclude specific change seq script step from the script
     * exelwtion.
     *
     * @note Change Sequencer assumes client knows what action they are
     * requesting and therefor it will blindly exclude the step based on
     * client's request. It does not perform any dependency tracking.
     */
    LwU32                                   stepIdExclusionMask;

    /*!
     * @brief Mask of clock domain entries that needs to be excluded during the
     * perf change sequence process.
     *
     * Client will provide this mask through set control RMCTRL.
     *
     * @note This is the MASK of clock domain index.
     *
     * @protected
     */
    BOARDOBJGRPMASK_E32                     clkDomainsExclusionMask;

    /*!
     * @brief Mask of clock domain entries that needs to be included during the
     * perf change sequence process.
     *
     * Client will provide this mask through set control RMCTRL.
     *
     * @note This is the MASK of clock domain index.
     *
     * @protected
     */
    BOARDOBJGRPMASK_E32                     clkDomainsInclusionMask;

    /*!
     * @ref CHANGE_SEQ_SCRIPT
     *
     * @protected
     */
    CHANGE_SEQ_SCRIPT                       script;

    /*!
     * @brief Represents the size of change structs allocated by the children of
     * perf change class.
     *
     * We will use this size to memcpy perf change structs.
     *
     * @note It is the responsibility of the child class to populate this value.
     *
     * @protected
     */
    LwU32                                   changeSize;

    /*!
     * @brief Force perf change request info.
     *
     * This field is exclusive from @ref CHANGE_SEQ::pChangeNext. The two
     * cannot both contain valid requests.
     *
     * @note It is the responsibility of the child class to allocate the memory
     * for this structure and assign this pointer to the allocated memory.
     *
     * @protected
     */
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE     *pChangeForce;

    /*!
     * @brief Next perf change request info.
     *
     * This field is exclusive from @ref CHANGE_SEQ::pChangeForce. The two
     * cannot both contain valid requests.
     *
     * @note It is the responsibility of the child class to allocate the memory
     * for this structure and assign this pointer to the allocated memory.
     *
     * @protected
     */
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE     *pChangeNext;

    /*!
     * @brief Last successfully applied change request.
     *
     * Store last completed perf change request. On completion signal from
     * perf daemon task, perf task will update this with current change.
     *
     * @note It is the responsibility of the child class to allocate the memory
     * for this structure and assign this pointer to the allocated memory.
     *
     * @protected
     */
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE     *pChangeLast;

    /*!
     * @brief Scratch buffer of change to be used for temporarily XBAR boost
     *        around MCLK switch.

     * @protected
     */
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE     *pChangeScratch;

    /*!
     * @brief The current/in-progress change request.
     *
     * If there is no in-progress change, @ref pChangeLwrr = @ref pChangeLast
     *
     * @note It is the responsibility of the child class to allocate the memory
     * for this structure and assign this pointer to the allocated memory.
     *
     * @protected
     */
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE     *pChangeLwrr;

    /*!
     * @brief Perf change sequence notification's listener tracking-lists.
     *
     * This array list is indexed by LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_<XYZ>
     *
     * @todo Remove this field for automotive builds. The feature this field is
     * tied to, PMU_PERF_CHANGE_SEQ_NOTIFICATION, is disabled.
     *
     * @protected
     */
    PERF_CHANGE_SEQ_NOTIFICATIONS
        notifications[RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_MAX];

    /*!
     * @brief Cached copy of volt offset adjusted with last programmed as well
     * as new coming requests.
     *
     * This copy will be used to update the regular perf change request with
     * the volt offset values.
     *
     * @ref LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT_VOLT_OFFSET
     *
     * @protected
     */
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT_VOLT_OFFSET  voltOffsetCached;

    /*!
     * @brief Low Power Perf Change Sequencer params buffer.
     *
     * @ref CHANGE_SEQ_LPWR
     *
     * @protected
     */
    CHANGE_SEQ_LPWR    *pChangeSeqLpwr;
};

/*!
 * @brief Message sent from the change sequencer to a PMU client indicating a
 * synchronous change request has completed.
 */
typedef struct
{
    /*!
     * @brief The sequence ID of the completed change.
     *
     * Shall be greater than or equal to the senquence ID of the synchronous
     * request.
     */
    LwU32 seqId;
} PERF_CHANGE_SEQ_PMU_CLIENT_COMPLETE_NOTIFICATION;


/*!
 * @brief Perf Change Sequence group Table
 */
typedef struct
{
    /*!
     * @brief Pointer to version specific Change Sequencer.
     */
    CHANGE_SEQ                         *pChangeSeq;

    /*!
     * @brief Last completed perf change request.
     *
     * Store last completed perf change request. On completion signal from
     * perf daemon task, perf task will update this with current change.
     * This struct memory will be allocated in version specific leaf class.
     * It is kept here so that a client does NOT need to attach the complete
     * perf change sequencer DMEM overlay; it just the bare minimum required
     * to get the client specific data.
     */
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLast;

    /*!
     * @brief Queued perf change request.
     */
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeNext;

    /*!
     * @brief Scratch buffer of change to be used for temporarily XBAR boost
     *        around MCLK switch.
     *
     * @protected
     */
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeScratch;
} CHANGE_SEQS;

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
mockable PerfChangeSeqConstruct (perfChangeSeqConstruct_SUPER)
    GCC_ATTRIB_SECTION("imem_libPerfInit", "perfChangeSeqConstruct_SUPER");
mockable PerfChangeSeqInfoGet   (perfChangeSeqInfoGet_SUPER)
    GCC_ATTRIB_SECTION("imem_libChangeSeq", "perfChangeSeqInfoGet_SUPER");
mockable PerfChangeSeqInfoSet   (perfChangeSeqInfoSet_SUPER)
    GCC_ATTRIB_SECTION("imem_libChangeSeq", "perfChangeSeqInfoSet_SUPER");

FLCN_STATUS perfChangeSeqConstruct(void)
    GCC_ATTRIB_SECTION("imem_libPerfInit", "perfChangeSeqConstruct");

FLCN_STATUS perfChangeSeqInfoGet(RM_PMU_PERF_CHANGE_SEQ_INFO_GET *pInfo)
    GCC_ATTRIB_SECTION("imem_libChangeSeq", "perfChangeSeqInfoGet");
FLCN_STATUS perfChangeSeqInfoSet(RM_PMU_PERF_CHANGE_SEQ_INFO_SET *pInfo)
    GCC_ATTRIB_SECTION("imem_libChangeSeq", "perfChangeSeqInfoSet");
FLCN_STATUS perfChangeSeqQueueChange(LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT *pChangeInput, PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE *pChangeRequest)
    GCC_ATTRIB_SECTION("imem_libChangeSeq", "perfChangeSeqQueueChange");
FLCN_STATUS perfChangeSeqLock(RM_PMU_PERF_CHANGE_SEQ_LOCK *pLock)
    GCC_ATTRIB_SECTION("imem_libChangeSeq", "perfChangeSeqLock");
FLCN_STATUS perfChangeSeqLoad(LwBool bLoad)
    GCC_ATTRIB_SECTION("imem_changeSeqLoad", "perfChangeSeqLoad");
PerfChangeSeqQuery (perfChangeSeqQuery)
    GCC_ATTRIB_SECTION("imem_libChangeSeq", "perfChangeSeqQuery");
void        perfChangeSeqScriptCompletion(FLCN_STATUS completionStatus)
    GCC_ATTRIB_SECTION("imem_libChangeSeq", "perfChangeSeqScriptCompletion");
FLCN_STATUS perfChangeSeqRegisterNotification(RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION notification, PERF_CHANGE_SEQ_NOTIFICATION *pRegister)
    GCC_ATTRIB_SECTION("imem_libChangeSeq", "perfChangeSeqRegisterNotification");
FLCN_STATUS perfChangeSeqUnregisterNotification(RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION notification, PERF_CHANGE_SEQ_NOTIFICATION *pRegister)
    GCC_ATTRIB_SECTION("imem_libChangeSeq", "perfChangeSeqUnregisterNotification");
FLCN_STATUS perfChangeSeqSendCompletionNotification(LwBool bSync, PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE *pChangeRequest)
    GCC_ATTRIB_SECTION("imem_libChangeSeq", "perfChangeSeqSendCompletionNotification");
FLCN_STATUS perfChangeSeqQueueVoltOffset(LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT_VOLT_OFFSET *pInputVoltOffset)
    GCC_ATTRIB_SECTION("imem_libChangeSeq", "perfChangeSeqQueueVoltOffset");
FLCN_STATUS perfChangeSeqQueueMemTuneChange(LwU8 tFAW)
      GCC_ATTRIB_SECTION("imem_libChangeSeq", "perfChangeSeqQueueMemTuneChange");
FLCN_STATUS perfChangeSeqCopyScriptLwrrToScriptLast(CHANGE_SEQ *pChangeSeq, CHANGE_SEQ_SCRIPT *pScript, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLwrr)
    GCC_ATTRIB_SECTION("imem_libChangeSeq", "perfChangeSeqCopyScriptLwrrToScriptLast");

/* ------------------------ Exported for Unit Testing ----------------------- */
mockable FLCN_STATUS perfChangeSeqProcessPendingChange(CHANGE_SEQ *pChangeSeq)
    GCC_ATTRIB_SECTION("imem_libChangeSeq", "perfChangeSeqProcessPendingChange");
mockable FLCN_STATUS perfChangeSeqValidateClkDomainInputMask(CHANGE_SEQ *pChangeSeq, BOARDOBJGRPMASK_E32 *pClkDomainsMask)
    GCC_ATTRIB_SECTION("imem_libChangeSeq", "perfChangeSeqValidateClkDomainInputMask");
mockable FLCN_STATUS perfChangeSeqSyncChangeQueueEnqueue(CHANGE_SEQ *pChangeSeq, PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE *pChangeRequest)
    GCC_ATTRIB_SECTION("imem_libChangeSeq", "perfChangeSeqSyncChangeQueueEnqueue");
         FLCN_STATUS perfChangeSeqAdjustVoltOffsetValues(CHANGE_SEQ *pChangeSeq, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLwrr)
    GCC_ATTRIB_SECTION("imem_libChangeSeq", "perfChangeSeqAdjustVoltOffsetValues");
mockable void perfChangeSeqUpdateProfilingHistogram(CHANGE_SEQ *pChangeSeq)
    GCC_ATTRIB_SECTION("imem_libChangeSeq", "perfChangeSeqUpdateProfilingHistogram");

/* ------------------------ Include Derived Types --------------------------- */
#include "perf/35/changeseq_35.h"
#include "perf/changeseq_lpwr.h"
#include "perf/changeseq_daemon.h"

#endif // G_CHANGESEQ_H
#endif // CHANGESEQ_H
