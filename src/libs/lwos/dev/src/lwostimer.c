/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    lwostimer.c
 * @copydoc lwostimer.h
 */

/* ------------------------ System includes -------------------------------- */
#include "lwrtos.h"
/* ------------------------ Application includes --------------------------- */
#include "lwostimer.h"
#include "lwosdebug.h"
#include "dmemovl.h"

/* ------------------------ Application includes --------------------------- */
/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Prototypes ------------------------------------- */
static LwU32 _osTimerGetEntryDelay(OS_TIMER_ENTRY *pEntry);

/* ------------------------ Global variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions  ------------------------------ */
/*!
 * Initializes a OS_TIMER structure.  Should be called when initializing a
 * falcon task.
 *
 * @param[in] pOsTimer   Pointer to OS_TIMER object
 * @param[in] numEntries Number of entries this falcon task needs to support.
 */
void
osTimerInitTracked // NJ-TODO: Remove "tracked" part
(
    LwrtosTaskHandle pxTask,
    OS_TIMER   *pOsTimer,
    LwU8        numEntries
)
{
    //
    // Check that the requested number of entries are supported.  This should be
    // something we're checking/ensuring statically.
    //
    OS_ASSERT(numEntries <= OS_TIMER_MAX_ENTRIES);

    pOsTimer->numEntries = numEntries;
    pOsTimer->entryMask  = 0;
    pOsTimer->pEntries   = lwosCallocResidentType(numEntries, OS_TIMER_ENTRY);
}

/*!
 * Schedules a callback at a relative time delay (specified in us).
 *
 * This function schedules a callback corresponding to a statically-allocated
 * index into the OS_TIMER object.  These indexes are statically
 * reserved/defined per task/OS_TIMER use.  As such, calling
 * osTimerScheduleCallback() with an index which is lwrrently active will
 * overwrite the settings for that index, and will not throw an error.  It is
 * expected that the calling function will be aware of this and will
 * explicitly desire it.
 *
 * This function can also be used to de-schedule a previously scheduled
 * callback. To accomplish this, simply pass in NULL for the callback function
 * on the particular callback index you wish to de-schedule. It is NOT
 * considered and error to de-schedule on an index which is not lwrrently
 * scheduled.
 *
 * @param[in] pOsTimer  Pointer to OS_TIMER object
 * @param[in] index     Entry index for this callback - these indexes are
 *     statically allocated for each task.
 * @param[in] ticksPrev Falcon OS scheduler tick timestamp from which to base
 *     the relative delay.
 * @param[in] usDelay   Delay for the callback in us. Function will colwert
 *     to ticks as appropriate.
 * @param[in] flags     Flags value to specify various options for the callback
 *     entry.  See @ref LW_OS_TIMER_FLAGS_<xyz>.
 * @param[in] pCallback Function pointer for the callback
 * @param[in] pParam    Parameter given to pCallback function
 * @param[in] ovlIdx
 *     Index of the overlay to attach and load before and detach after calling
 *     the specified callback.  When this index is OVL_INDEX_ILWALID,
 *     the OS_TIMER will not perform any special overlay handling.
 *
 * @return OS_TIMER_MAX_ENTRIES if the requested index exceeds the number of
 *                              entries supported by this OS_TIMER
 * @return the value of index if the callback is scheduled successfully
 */
LwU8
osTimerScheduleCallback
(
    OS_TIMER         *pOsTimer,
    LwU32             index,
    LwU32             ticksPrev,
    LwU32             usDelay,
    LwU8              flags,
    OS_TIMER_FUNCTION pCallback,
    void             *pParam,
    LwU8              ovlIdx
)
{
    //
    // Ensure that the requested index is within the number of entries supported
    // by this OS_TIMER.  We should be able to enforce this statically.
    //
    OS_ASSERT(index < pOsTimer->numEntries);

    // schedule
    if (pCallback != NULL)
    {
        pOsTimer->pEntries[index].flags      = flags;
        pOsTimer->pEntries[index].ovlIdx     = ovlIdx;
        pOsTimer->pEntries[index].ticksPrev  = ticksPrev;
        pOsTimer->pEntries[index].ticksDelay = usDelay / LWRTOS_TICK_PERIOD_US;
        pOsTimer->pEntries[index].pCallback  = pCallback;
        pOsTimer->pEntries[index].pParam     = pParam;
        pOsTimer->entryMask |= BIT(index);
    }
    // de-schedule
    else
    {
        pOsTimer->pEntries[index].pCallback  = NULL;
        pOsTimer->entryMask &= ~BIT(index);
    }

    return index;
}

/*!
 * Determines the next delay for OS_TIMER based on the next callback to fire.
 *
 * @param[in] pOsTimer  Pointer to OS_TIMER object
 *
 * @return Number of ticks for which the task should on the queue before calling
 *     expired callbacks.
 */
LwU32
osTimerGetNextDelay
(
    OS_TIMER *pOsTimer
)
{
    LwU32 delay = lwrtosMAX_DELAY;
    LwU32 entry;
    LwU8  e;

    // Loop through entries finding task with soonest tick target.
    FOR_EACH_INDEX_IN_MASK(32, e, pOsTimer->entryMask)
    {
        entry = _osTimerGetEntryDelay(&pOsTimer->pEntries[e]);

        delay = LW_MIN(delay, entry);
    }
    FOR_EACH_INDEX_IN_MASK_END;

    return delay;
}

/*!
 * Calls all expired callbacks.
 *
 * NOTE: For the sake of simplicity, expired callbacks are called in the order
 * of their indexes, not relative to their expiration timestamps.
 *
 * @param[in] pOsTimer  Pointer to OS_TIMER object
 */
void
osTimerCallExpiredCallbacks
(
    OS_TIMER *pOsTimer
)
{
    LwU8  e;

    FOR_EACH_INDEX_IN_MASK(32, e, pOsTimer->entryMask)
    {
        OS_TIMER_ENTRY *pEntry = &pOsTimer->pEntries[e];

        if (0 == _osTimerGetEntryDelay(pEntry))
        {
            LwU32   ticksPrev;
            LwU32   skippedCnt = 0;

            //
            // Update the exelwtion time by incrementing the previous timestamp.
            // We don't want to use the current one, as that introduces drift.
            //
            pEntry->ticksPrev += pEntry->ticksDelay;

            //
            // Capture time when callback was supposed to be exelwted so that it
            // can compensate for any drifts (if required).
            //
            ticksPrev = pEntry->ticksPrev;

            if (FLD_TEST_DRF(_OS_TIMER, _FLAGS, _RELWRRING, _YES_MISSED_SKIP,
                             pEntry->flags))
            {
                LwU32   now     = lwrtosTaskGetTickCount32();
                LwU32   elapsed = now - pEntry->ticksPrev;

                //
                // Updated relwrring callback can be expired multiple times. In
                // this mode of operation we skip all subsequnet exelwtions and
                // proceed strait to the first one in future. Expired callback
                // will receive count of skipped exelwtions.
                //
                if (elapsed >= pEntry->ticksDelay)
                {
                    skippedCnt        =       (elapsed / pEntry->ticksDelay);
                    pEntry->ticksPrev = now - (elapsed % pEntry->ticksDelay);
                }
            }
            // If not relwrring, mark the entry as expired.
            else if (FLD_TEST_DRF(_OS_TIMER, _FLAGS, _RELWRRING, _NO,
                                  pEntry->flags))
            {
                pOsTimer->entryMask &= ~BIT(e);
            }

            //
            // Ensure that the callback is not NULL before ilwoking it. A
            // callback function may become NULL while ilwoking expired
            // callbacks if a callback function cancels callbacks other than
            // itself.
            //
            if (pEntry->pCallback != NULL)
            {
                // If specified, attach and load associated overlay.
                if (OVL_INDEX_ILWALID != pEntry->ovlIdx)
                {
                    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(pEntry->ovlIdx);
                }

                pEntry->pCallback(pEntry->pParam, ticksPrev, skippedCnt);

                // If specified, detach associated overlay.
                if (OVL_INDEX_ILWALID != pEntry->ovlIdx)
                {
                    OSTASK_DETACH_IMEM_OVERLAY(pEntry->ovlIdx);
                }
            }
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;
}

/* ------------------------ Static Functions  ------------------------------ */
/*!
 * @brief   Computes remaining timer ticks for given callback entry.
 *
 * @param[in]   pEntry  Callback entry
 *
 * @return  Remaining timer ticks or zero if callback has expired
 */
static LwU32
_osTimerGetEntryDelay
(
    OS_TIMER_ENTRY *pEntry
)
{
    LwU32 ticks = lwrtosTaskGetTickCount32();
    LwU32 diff;

    //
    // Note: When the timeout has expired, i.e. ticks > ticksPrev +
    // ticksDelay (including cases of overflow), the diff value will be
    // greater than ticksDelay.  Handle these cases by clamping to 0 and
    // immediately expiring the delay.
    //
    diff = pEntry->ticksPrev + pEntry->ticksDelay - ticks;
    //
    // If diff is greater than the delay period, we already missed the
    // target. If so, clamp to 0 to call immediately.
    //
    diff = (diff > pEntry->ticksDelay) ? 0 : diff;

    return diff;
}
