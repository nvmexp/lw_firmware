/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perf_limit_client.h
 * @brief   PMU PERF_LIMIT client interface file.
 */

#ifndef PERF_LIMIT_CLIENT_H
#define PERF_LIMIT_CLIENT_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "pmu_oslayer.h"
#include "ctrl/ctrl2080/ctrl2080perf.h"

/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * Locks a client, allowing the caller to make updates to limits and issue the
 * arbitration command.
 *
 * @param[in]  _pClient  client to lock
 */
#define PERF_LIMITS_CLIENT_SEMAPHORE_TAKE(_pClient)                            \
    do {                                                                       \
        lwrtosSemaphoreTakeWaitForever((_pClient)->hdr.clientSemaphore);       \
        (_pClient)->hdr.taskOwner = osTaskIdGet();                             \
    } while (LW_FALSE)

/*!
 * Unlocks a client, allowing other callers access to the client.
 *
 * @param[in]  _pClient  client to unlock
 */
#define PERF_LIMITS_CLIENT_SEMAPHORE_GIVE(_pClient)                            \
    do {                                                                       \
        (_pClient)->hdr.taskOwner = RM_PMU_TASK_ID__IDLE;                      \
        lwrtosSemaphoreGive((_pClient)->hdr.clientSemaphore);                  \
    } while (LW_FALSE)

/*!
 * Helper macro to set a P-state limit for the given entry
 *
 * @param[in,out]  _pEntry     pointer to the entry to populate
 * @param[in]      _limitId    limit to set
 * @param[in]      _pstateIdx  P-state index to set the limit to
 * @param[in]      _pt         P-state point to set the limit to
 */
#define PERF_LIMITS_CLIENT_PSTATE_LIMIT(_pEntry, _limitId, _pstateIdx, _pt)    \
do {                                                                           \
    (_pEntry)->limitId = (_limitId);                                           \
    (_pEntry)->clientInput.type =                                              \
        LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_TYPE_PSTATE_IDX;              \
    (_pEntry)->clientInput.data.pstate.pstateIdx = (_pstateIdx);               \
    (_pEntry)->clientInput.data.pstate.point = (_pt);                          \
} while (LW_FALSE)

/*!
 * Helper macro to set a frequency limit for the given entry
 *
 * @param[in,out]  _pEntry     pointer to the entry to populate
 * @param[in]      _limitId    limit to set
 * @param[in]      _clkDomIdx  clock domain index to limit
 * @param[in]      _freqkHz    frequency value to limit to
 */
#define PERF_LIMITS_CLIENT_FREQ_LIMIT(_pEntry, _limitId, _clkDomIdx, _freqkHz) \
do {                                                                           \
    (_pEntry)->limitId = (_limitId);                                           \
    (_pEntry)->clientInput.type =                                              \
        LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_TYPE_FREQUENCY;               \
    (_pEntry)->clientInput.data.freq.clkDomainIdx = (_clkDomIdx);              \
    (_pEntry)->clientInput.data.freq.freqKHz = (_freqkHz);                     \
} while (LW_FALSE)

/*!
 * Helper macro to set a virtual P-state limit for the given entry
 *
 * @param[in,out]  _pEntry      pointer to the entry to populate
 * @param[in]      _limitId     limit to set
 * @param[in]      _vpstateIdx  virtual P-state to limit to
 */
#define PERF_LIMITS_CLIENT_VPSTATE_LIMIT(_pEntry, _limitId, _vpstateIdx)       \
do {                                                                           \
    (_pEntry)->limitId = (_limitId);                                           \
    (_pEntry)->clientInput.type =                                              \
        LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_TYPE_VPSTATE_IDX;             \
    (_pEntry)->clientInput.data.vpstate.vpstateIdx = (_vpstateidx);            \
} while (LW_FALSE)

/*!
 * Helper macro to clear the limit for the given entry
 *
 * @param[in,out]  _pEntry   pointer to the entry to populate
 * @param[in]      _limitId  limit to clear
 */
#define PERF_LIMIT_CLIENT_CLEAR_LIMIT(_pEntry, _limitId)                       \
do {                                                                           \
    (_pEntry)->limitId = (_limitId);                                           \
    (_pEntry)->clientInput.type =                                              \
        LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_TYPE_DISABLED;                \
} while (LW_FALSE)

/*!
 * @brief   List of the overlay descriptors required to initialize perf limit
 *          client overlays.
 */
#define OSTASK_OVL_DESC_DEFINE_PERF_PERF_LIMITS_CLIENT_INIT                    \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libInit)                             \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPerfLimitClient)

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Header for a @ref PERF_LIMITS_CLIENT. Maintains the semaphore to synchronize
 * the data structures between the client and the arbiter. Also allows the
 * client to queue up multiple changes without issuing multiple arbitrate
 * commands.
 */
typedef struct
{
    /*!
     * Semaphore to syncronize the data found in the PERF_LIMITS_CLIENT between
     * the actual PMU client and the arbiter.
     */
    LwrtosSemaphoreHandle   clientSemaphore;

    /*!
     * Handle to the queue for the change sequencer to use when specifying a
     * synchronous operation has completed.
     */
    LwrtosQueueHandle       syncQueueHandle;

    /*!
     * Cache the start time of PMU client's change request.
     */
    FLCN_TIMESTAMP          timeStart;

    /*!
     * Flags to apply during arbitration.
     */
    LwU32 applyFlags;

    /*!
     * Since the RTOS does not provide support for semaphore ownership, we need
     * to keep track of which task is holding the semaphore.
     */
    LwU8 taskOwner;

    /*!
     * DMEM overlay used by the client. While this information is stored within
     * the header, the client must still attach the overlay before using
     * the PERF_LIMITS_CLIENT as the client structure is still store within
     * the overlay.
     */
    LwU8 dmemOvl;

    /*!
     * Specifies the number of PERF_LIMIT_CLIENT_INPUT structures in the
     * overlay.
     */
    LwU8 numEntries;

    /*!
     * Specifies if an arbitrate command has been queued. When the client
     * issues the arbitrate command, the flag is set. When the arbiter has
     * copied the client's request to its buffer, the flag is cleared.
     */
    LwBool bQueued;
} PERF_LIMITS_CLIENT_HEADER;

/*!
 * The PERF_LIMIT updates made by the client.
 */
typedef struct
{
    /*!
     * Specifies the limit to modify.
     */
    LW2080_CTRL_PERF_PERF_LIMIT_ID limitId;

    /*!
     * Specifies the values the client wishes to limit to.
     */
    LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT clientInput;
} PERF_LIMITS_CLIENT_ENTRY;

/*!
 * The data structure resides in the client's DMEM space and resides at the
 * start of that DMEM space.
 */
typedef struct
{
    /*!
     * Data to track the DMEM overlay containing the PERF_LIMITS_CLIENT data.
     */
    PERF_LIMITS_CLIENT_HEADER hdr;

    /*!
     * Pointer to the PERF_LIMITS_CLIENT_ENTRYs.
     */
    PERF_LIMITS_CLIENT_ENTRY *pEntries;
} PERF_LIMITS_CLIENT;

/* ------------------------ Function Prototypes ----------------------------- */
/*!
 * @interface PERF_LIMITS_CLIENT
 *
 * Initializes a PERF_LIMITS_CLIENT.
 *
 * @param[out]  pClient           pointer to the allocated PERF_LIMITS_CLIENT
 * @param[in]   dmemOvl           the DMEM overlay to store the client information
 * @param[in]   numEntries        number of PERF_LIMIT entries used by the client
 * @param[in]   bSyncArbRequired  synchronous arbitration is required by the client
 *
 * @return FLCN_OK if the PERF_LIMITS_CLIENT was initialized successfully;
 *         otherwise, a detailed error code is returned.
 */
#define PerfLimitsClientInit(fname) FLCN_STATUS (fname)(PERF_LIMITS_CLIENT **ppClient, LwU8 dmemOvl, LwU8 numEntries, LwBool bSyncArbRequired)

/*!
 * @interface PERF_LIMITS_CLIENT
 *
 * Searches the client's entry to either a) find the entry containing data
 * about @ref limitId or b) find an empty entry the client may use for setting
 * and clearing limits.
 *
 * @param[in]  pClient  pointer to the PERF_LIMITS_CLIENT to search
 * @param[in]  limitId  limit to find the entry of
 *
 * @return a pointer to the entry the client may use to set/clear limits; NULL
 *         no entry is available
 */
#define PerfLimitsClientEntryGet(fname) PERF_LIMITS_CLIENT_ENTRY * (fname)(PERF_LIMITS_CLIENT *pClient, LW2080_CTRL_PERF_PERF_LIMIT_ID limitId)

/*!
 * @interface PERF_LIMITS_CLIENT
 *
 * Issues an ARBITRATE_AND_APPLY command to the arbiter.
 *
 * @param[in]  pClient  Pointer to PERF_LIMITS_CLIENT.
 *
 * @return FLCN_OK if the ARBITRATE_AND_APPLY command was successfully queued;
 *         otherwise, a detailed error code is returned.
 */
#define PerfLimitsClientArbitrateAndApply(fname) FLCN_STATUS (fname)(PERF_LIMITS_CLIENT *pClient)

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
PerfLimitsClientInit (perfLimitsClientInit)
    GCC_ATTRIB_SECTION("imem_libPerfLimitClient", "perfLimitsClientInit");
PerfLimitsClientEntryGet (perfLimitsClientEntryGet)
    GCC_ATTRIB_SECTION("imem_libPerfLimitClient", "perfLimitsClientEntryGet");
PerfLimitsClientArbitrateAndApply (perfLimitsClientArbitrateAndApply)
    GCC_ATTRIB_SECTION("imem_libPerfLimitClient", "perfLimitsClientArbitrateAndApply");

#endif // PERF_LIMITS_CLIENT_H
