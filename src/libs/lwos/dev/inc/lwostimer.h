/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LWOSTIMER_H
#define LWOSTIMER_H

/*!
 * @file  lwostimer.h
 * @brief Infrastructure for timer object used for falcon task event/callback
 * looping.
 *
 * The usual design of a falcon task is that main task function is an infinite
 * loop waiting on a queue for incoming events from the RM or other falcon
 * tasks.  If the task requires any type of periodic callbacks, the task usually
 * adds a timeout for waiting on the queue such that timeout will expire when
 * the next iteration of the callback needs to fire.  For examples, see both
 * task12_therm.c and task6_pmgr.c in the PMU source code.  This code/structure
 * helps eliminate the code duplication.
 *
 * Even further, this structure has the added benefit of handling multiple
 * callbacks (i.e. different frequency/period, or possibly out of phase) within
 * a task.  Each callback has its own separate entry, and OS_TIMER takes care of
 * picking the appropriate queue wait timeout and then calling all the
 * applicable expired timeouts.
 *
 * The entry structure is very simple - each callback has its own static entry
 * number (determined statically in each task's header files).  An entry
 * consists of a callback function pointer, the specified delay (in OS scheduler
 * ticks) and the last time the callback fired (i.e. the relative time from
 * which to count the delay).
 *
 * Callback delays are specified by the caller in units of us, and the
 * OS_TIMER code colwerts as appropriate into units of OS scheduler ticks
 * (rounding down) based on the OS scheduler's tick duration.  The actual
 * callback delay will (in most cases, due to task time-slicing) be accurate
 * within one OS scheduler tick.
 *
 * The maximum possible delay is 0x7FFFFFFF * tick duration.  See
 * osTimerGetNextDelay comments for the limiting factors.
 * Assuming current tick duration of ~30us that means a maximum delay of ~64424
 * seconds (17.8 hours).
 */

/* ------------------------ System includes -------------------------------- */
#include "lwuproc.h"
#include "lwrtos.h"
#include "lwostmrcmn.h"

/* ------------------------ Application includes --------------------------- */
/* ------------------------ Defines ---------------------------------------- */
/*!
 * Maximum number of callback entries supported by the OS_TIMER.
 */
#define OS_TIMER_MAX_ENTRIES                                             32U

/*!
 * Macro for 1s wait time, in us.
 */
#define OS_TIMER_DELAY_1_S                                          1000000U

/*!
 * Macro for 1ms wait time, in us.
 */
#define OS_TIMER_DELAY_1_MS                                            1000U

/*!
 * Flags for configuring callback entries with the OS_TIMER.
 *
 *  _RELWRRING - Specifies that the callback should be relwrring at the specific
 *      delay period.  The OS_TIMER will take care of rescheduling the callback
 *      after every time it is called.  In case that the callback has expired
 *      mutliple times we have options:
 *      _MISSED_EXEC - execute each expired callback instance as soon as possible
 *      _MISSED_SKIP - execute it only once and immediately sync to delay period
 *
 *  _RSVD - These bits are lwrrently RSVD and should always be 0.
 */
#define LW_OS_TIMER_FLAGS_RELWRRING                                      1:0
#define LW_OS_TIMER_FLAGS_RELWRRING_NO                           0x00000000U
#define LW_OS_TIMER_FLAGS_RELWRRING_YES_MISSED_EXEC              0x00000001U
#define LW_OS_TIMER_FLAGS_RELWRRING_YES_MISSED_SKIP              0x00000002U
#define LW_OS_TIMER_FLAGS_RELWRRING_RSVD                         0x00000003U
#define LW_OS_TIMER_FLAGS_RSVD                                           7:2

/*!
 * Macro for waiting on a queue for the next message or for the next callback to
 * fire, whichever comes first.
 *
 * Should be followed in the code by a call to @ref OS_TIMER_WAIT_END.
 *
 * @param[in] pOsTimer    Pointer to OS_TIMER struct
 * @param[in] queue       Queue handle on which to wait
 * @param[in] pDispStruct Pointer to struct in which to retrieve the next
 *                        message/command.
 * @param[in] maxDelay    Maximum ticks new waitDelay will take. will use
 *                        maxDelay when waitDelay is greater than maxDelay.
 *                        i.e. delay will be capped to maxDelay.
 */
#define OS_TIMER_WAIT_FOR_QUEUE_OR_CALLBACK_START(pOsTimer, queue, pDispStruct,\
        maxDelay)                                                              \
{                                                                              \
    LwU32 waitDelay = osTimerGetNextDelay(pOsTimer);                           \
    waitDelay = LW_MIN((waitDelay), (maxDelay));                               \
    while (OS_QUEUE_WAIT_TICKS((queue), (pDispStruct), (waitDelay)))           \
    {

/*!
 * Matching macro for @ref OS_TIMER_WAIT_START.  Updates the waitDelay if
 * a command received, and then calls all expired callbacks after waitDelay
 * expires.
 *
 * @param[in] pOsTimer  Pointer to OS_TIMER struct
 * @param[in] maxDelay  Maximum ticks new waitDelay will take. will use
 *                      maxDelay when waitDelay is greater than maxDelay.
 *                      i.e. delay will be capped to maxDelay.
 */
#define OS_TIMER_WAIT_FOR_QUEUE_OR_CALLBACK_STOP(pOsTimer, maxDelay)           \
        waitDelay = osTimerGetNextDelay(pOsTimer);                             \
        waitDelay = LW_MIN((waitDelay), (maxDelay));                           \
    }                                                                          \
    osTimerCallExpiredCallbacks(pOsTimer);                                     \
}


/*!
 * A simple colwenience-wrapper for @ref osTimerScheduleCallback to allow for
 * easy cancellation/descheduling of a OS timer callback.
 *
 * @param[in] pOsTimer  Pointer to OS_TIMER object
 * @param[in] index     Entry index for the callback to deschedule
 *
 * @return OS_TIMER_MAX_ENTRIES if the requested index exceeds the number of
 *                              entries supported by this OS_TIMER
 * @return the value of index if the callback is scheduled successfully
 */
#define osTimerDeScheduleCallback(pOsTimer, index)                             \
    osTimerScheduleCallback(pOsTimer, index, 0, 0, 0, NULL, NULL,              \
            OVL_INDEX_ILWALID)

/*!
 * Macro for implementing non-busy-waiting loop while applying a delay.
 */
#define OS_TIMER_WAIT_US(_delayUs)                                             \
    lwrtosLwrrentTaskDelayTicks(LWRTOS_TIME_US_TO_TICKS((_delayUs)))

/* ------------------------ Types definitions ------------------------------ */
/*!
 * @brief   Prototype for OS_TIMER callback function.
 *
 * @param[in]   pParam          Parameter provided at callback scheduling time.
 * @param[in]   ticksExpired    OS ticks time when callback expired.
 * @param[in]   skipped         Number of skipped callbacks due to @ref
 *      LW_OS_TIMER_FLAGS_RELWRRING == _YES_MISSED_SKIP (otherwise zero).
 */
#define OsTimerCallback(fname) void (fname)(void *pParam, LwU32 ticksExpired, LwU32 skipped)

/*!
 * Typedef for OS_TIMER callback function pointer type (@ref OsTimerCallback).
 */
typedef OsTimerCallback (*OS_TIMER_FUNCTION);

/*!
 * A callback entry in the OS_TIMER.  Represents a callback based on a number of
 * falcon ticks.
 */
typedef struct
{
    /*!
     * Flags field.  Defined by LW_OS_TIMER_FLAGS_<xyz> above.
     */
    LwU8              flags;
    /*!
     * Index of an overlay to load before and unload after calling the specified
     * @ref pCallback. This overlay argument should be specified if the callback
     * is not in the associated task's native overlay (nor can it be loaded in
     * conlwrrently as the task processes), but needs to be loaded in on demand.
     *
     * If this index is OVL_INDEX_ILWALID, the OS_TIMER structure will not try
     * to load in an associated overlay.
     */
    LwU8              ovlIdx;
    /*!
     * Tick count for the relative time from which the callback should fire.
     * Used to prevent drift for both relwrring and non-relwrring callbacks.
     */
    LwU32             ticksPrev;
    /*!
     * Tick number at which callback should fire.
     */
    LwU32             ticksDelay;
    /*!
     * Callback function to call after target tick delay.
     */
    OS_TIMER_FUNCTION pCallback;
    /*!
     * Parameter to callback function.
     */
    void             *pParam;
} OS_TIMER_ENTRY;

/*!
 * Main OS_TIMER struct.
 */
typedef struct
{
    /*!
     * Number of valid entires supported by this OS_TIMER.
     */
    LwU8             numEntries;
    /*!
     * Mask of lwrrently active entries - i.e. entries which have yet to fire.
     * The size of this mask must be kept in sync with @ref
     * OS_TIMER_MAX_ENTRIES.
     */
    LwU32            entryMask;
    /*!
     * Array of entries of callbacks
     */
    OS_TIMER_ENTRY  *pEntries;
} OS_TIMER;

/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */

//
// All functions are intentionally resident so they can be called from all
// tasks/overlays.
//
void  osTimerInitTracked(LwrtosTaskHandle pxTask, OS_TIMER *pOsTimer, LwU8 numEntries)
    GCC_ATTRIB_SECTION("imem_init", "osTimerInitTracked");

LwU8  osTimerScheduleCallback(OS_TIMER *pOsTimer, LwU32 index, LwU32 ticksPrev, LwU32 usDelay, LwU8 flags, OS_TIMER_FUNCTION pCallback, void *pParam, LwU8 ovlIdx);

LwU32 osTimerGetNextDelay(OS_TIMER *pOsTimer);

void  osTimerCallExpiredCallbacks(OS_TIMER *pOsTimer);

/* ------------------------ Inline Functions ------------------------------- */

/*!
 * Helper function exelwtes all expired callbacks until there is no callback to
 * execute. It returns number of ticks to next OS_TIMER callback.
 *
 * NOTE: ENSURE THAT THIS FUNCTION GETS INLINED IN FILE. COMPILER WILL
 *       AUTOMATICALLY MAKE IT INLINE IF FUNTION GETS CALLED ONLY ONCE
 *       IN A FILE.
 *
 * @param[in] pOsTimer   Pointer to OS_TIMER struct
 *
 * @return    Number of ticks to next OS_TIMER callback
 */
static inline LwU32
osTimerCallExpiredAndNextDelay
(
    OS_TIMER *pOsTimer
)
{
    LwU32 nextDelay;

    while (LW_TRUE)
    {
        nextDelay = osTimerGetNextDelay(pOsTimer);
        if (nextDelay != 0U)
        {
            break;
        }
        osTimerCallExpiredCallbacks(pOsTimer);
    }

    return nextDelay;
}

#endif // LWOSTIMER_H
