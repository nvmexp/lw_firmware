/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_pgstat.c
 * @brief  PMU Power Gating Statistics
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpg.h"
#include "pmu_objdi.h"
#include "pmu_objtimer.h"

#include "dbgprintf.h"

/* ------------------------- Macros and Defines ----------------------------- */
#define PG_STAT_ELAPSED_TIME_GET_NS(startTimestampNs, endTimeStampNs)       \
        (endTimeStampNs.parts.lo - startTimestampNs.parts.lo)
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
//
// Move this to Engine specific object once
// offload code is in place
//
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions  ------------------------------ */
/*!
 * @brief Record PG Statistics.
 *
 * @param[in]  ctrlId   PG-controller ID that the event is associated with
 * @param[in]  event    The type of ELPG event that was raised
 */
void
pgStatRecordEvent
(
    LwU8  ctrlId,
    LwU32 event
)
{
    OBJPGSTATE     *pPgState = GET_OBJPGSTATE(ctrlId);
    FLCN_TIMESTAMP  timeStampNs;

    switch (event)
    {
        case PMU_PG_EVENT_PG_ON:
        {
            // Update powered up time
            pgStatPoweredUpTimeUpdate(ctrlId);

            break;
        }

        case PMU_PG_EVENT_POWERED_DOWN:
        {
            // Get entry end timeStampNs.
            osPTimerTimeNsLwrrentGet(&timeStampNs);

            //
            // Callwlate entry latency
            // = Entry end for current cycle - Entry start for current cycle
            //
            pPgState->stats.entryLatencyUs =
                PG_STAT_ELAPSED_TIME_GET_NS(pPgState->timeStampCacheNs, timeStampNs) / 1000;

            //
            // Callwlate rolling average entry latency
            //
            // For first cycle, average latency is same as current latency. For
            // successive cycles we use rolling average to callwlate average latency.
            //
            // Rolling average entry latency =
            // (Average entry latency * (Rolling average count - 1) + entry latency of current cycle) /
            // (Rolling average count)
            //
            if (pPgState->stats.entryCount == 0)
            {
                pPgState->stats.entryLatencyAvgUs = pPgState->stats.entryLatencyUs;
            }
            else
            {
                pPgState->stats.entryLatencyAvgUs =
                    (pPgState->stats.entryLatencyAvgUs * (RM_PMU_PG_ROLLING_AVG_COUNT - 1) +
                     pPgState->stats.entryLatencyUs) / RM_PMU_PG_ROLLING_AVG_COUNT;
            }

            //
            // Callwlate max entry latency
            // Max entry latency = MAX(Max entry latency, Entry latency of current cycle)
            //
            pPgState->stats.entryLatencyMaxUs = LW_MAX(pPgState->stats.entryLatencyMaxUs,
                                                       pPgState->stats.entryLatencyUs);

            // Update total non-sleep time
            pPgState->stats.totalNonSleepTimeUs += pPgState->stats.entryLatencyUs;

            // Update entry count
            pPgState->stats.entryCount++;

            //
            // Cache entry end timeStampNs. It will be used for callwlating
            // resident time.
            //
            pPgState->timeStampCacheNs.data = timeStampNs.data;

            break;
        }

        case PMU_PG_EVENT_POWERING_UP:
        {
            // Update sleep time
            pgStatSleepTimeUpdate(ctrlId);

            break;
        }

        case PMU_PG_EVENT_POWERED_UP:
        {
            // Get exit end timeStampNs.
            osPTimerTimeNsLwrrentGet(&timeStampNs);

            //
            // Callwlate exit latency
            // = Exit end for current cycle - Exit start for current cycle
            //
            pPgState->stats.exitLatencyUs =
                PG_STAT_ELAPSED_TIME_GET_NS(pPgState->timeStampCacheNs, timeStampNs) / 1000;

            //
            // Callwlate rolling average exit latencies
            //
            // For first cycle, average latency is same as current latency. For
            // successive cycles we use rolling average to callwlate average latency.
            //
            // Rolling average exit latency =
            // (Average exit latency * (Rolling average count - 1) + exit latency of current cycle) /
            // (Rolling average count)
            //
            if (pPgState->stats.exitCount == 0)
            {
                pPgState->stats.exitLatencyAvgUs = pPgState->stats.exitLatencyUs;
            }
            else
            {
                pPgState->stats.exitLatencyAvgUs =
                    (pPgState->stats.exitLatencyAvgUs * (RM_PMU_PG_ROLLING_AVG_COUNT - 1) +
                     pPgState->stats.exitLatencyUs) / RM_PMU_PG_ROLLING_AVG_COUNT;
            }

            //
            // Callwlate max exit latency
            // Max exit latency  = MAX(Max exit latency , Exit latency of current cycle)
            //
            pPgState->stats.exitLatencyMaxUs = LW_MAX(pPgState->stats.exitLatencyMaxUs,
                                                      pPgState->stats.exitLatencyUs);

            // Update total non-sleep time
            pPgState->stats.totalNonSleepTimeUs += pPgState->stats.exitLatencyUs;

            // Update ungating count
            pPgState->stats.exitCount++;

            //
            // Cache exit end timeStampNs. It will be used for callwlating
            // steady state time.
            //
            pPgState->timeStampCacheNs.data = timeStampNs.data;

            break;
        }

        case PMU_PG_EVENT_DENY_PG_ON:
        {
            LwU32 abortedTimeUs = 0;

            // Get aborted exit timeStampNs.
            osPTimerTimeNsLwrrentGet(&timeStampNs);

            //
            // Callwlate time required to abort the entry
            // = Entry start for current cycle - Aborted exit
            //
            abortedTimeUs = PG_STAT_ELAPSED_TIME_GET_NS(pPgState->timeStampCacheNs, timeStampNs) / 1000;

            // Update total non-sleep time
            pPgState->stats.totalNonSleepTimeUs += abortedTimeUs;

            // Increment abort counter
            pPgState->stats.abortCount++;

            //
            // Cache timeStampNs. It will be used for callwlating non-sleep time
            // on next PG_ON.
            //
            pPgState->timeStampCacheNs.data = timeStampNs.data;

            break;
        }

        default:
        {
            break;
        }
    }
}

/*!
 * @brief Update the LPWR stats
 *
 * @param[in]  ctrlId   PgCtrl Id
 */
void
pgStatUpdate
(
    LwU8  ctrlId
)
{
    OBJPGSTATE  *pPgState = GET_OBJPGSTATE(ctrlId);

    switch (pPgState->state)
    {
        case PMU_PG_STATE_PWR_ON:
        case PMU_PG_STATE_DISALLOW:
        {
            // Update powered up time
            pgStatPoweredUpTimeUpdate(ctrlId);

            break;
        }
        case PMU_PG_STATE_PWR_OFF:
        {
            // Update sleep time
            pgStatSleepTimeUpdate(ctrlId);

            break;
        }
    }
}

/*!
 * @brief Update Sleep time for given PgCtrl
 *
 * @param[in]  ctrlId   PgCtrl Id
 */
void
pgStatSleepTimeUpdate
(
    LwU8  ctrlId
)
{
    OBJPGSTATE       *pPgState   = GET_OBJPGSTATE(ctrlId);
    PGSTATE_STATS_64 *pPgStats64 = PGSTATE_STATS_64_GET(pPgState);
    FLCN_TIMESTAMP    timeStampNs;
    LwU32             residentTimeUs;

    // Get current time
    osPTimerTimeNsLwrrentGet(&timeStampNs);

    // Callwlate resident time
    residentTimeUs = PG_STAT_ELAPSED_TIME_GET_NS(pPgState->timeStampCacheNs, timeStampNs) / 1000;

    // Callwlate total sleep time
    pPgState->stats.totalSleepTimeUs += residentTimeUs;

    // Do 64 bit callwlations for resident and sleep time
    if (PMUCFG_FEATURE_ENABLED(PMU_PG_STATS_64) &&
        (pPgStats64 != NULL))
    {
        lw64Sub(&pPgStats64->residentTimeNs,
                &timeStampNs.data, &pPgState->timeStampCacheNs.data);

        lw64Add(&pPgStats64->totalSleepTimeNs,
                &pPgStats64->totalSleepTimeNs,
                &pPgStats64->residentTimeNs);
    }

    // Update the cached time
    pPgState->timeStampCacheNs.data = timeStampNs.data;
}

/*!
 * @brief Update Powered up time for given PgCtrl
 *
 * @param[in]  ctrlId   PgCtrl Id
 */
void
pgStatPoweredUpTimeUpdate
(
    LwU8  ctrlId
)
{
    OBJPGSTATE     *pPgState = GET_OBJPGSTATE(ctrlId);
    FLCN_TIMESTAMP  timeStampNs;
    LwU32           poweredUpTimeUs;

    // Get current time
    osPTimerTimeNsLwrrentGet(&timeStampNs);

    // Update powered up time and total non-sleep time
    if (pPgState->stats.entryCount != 0)
    {
        poweredUpTimeUs = PG_STAT_ELAPSED_TIME_GET_NS(pPgState->timeStampCacheNs, timeStampNs) / 1000;

        pPgState->stats.totalNonSleepTimeUs += poweredUpTimeUs;
    }

    // Update the cached time
    pPgState->timeStampCacheNs.data = timeStampNs.data;
}

/*!
 * @brief Return sleep time related info of given PgCtrl
 *
 * It returns the following info -
 *    a. Delta of sleep time from last sample
 *    b. Current Sleep time
 *
 * @param[in]  ctrlId   PgCtrl Id
 */
void
pgStatDeltaSleepTimeGetUs
(
    LwU8                  ctrlId,
    PGSTATE_STAT_SAMPLE  *pSleep
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);

    // Update current sleep time
    if (pPgState->state == PMU_PG_STATE_PWR_OFF)
    {
        pgStatSleepTimeUpdate(ctrlId);
    }

    // Update the delta sleep time (Current sleep time - Last sample time)
    if (pPgState->stats.totalSleepTimeUs >= pSleep->prevTimeUs)
    {
        pSleep->deltaTimeUs = pPgState->stats.totalSleepTimeUs - pSleep->prevTimeUs;
    }
    else
    {
        pSleep->deltaTimeUs = (LW_U32_MAX - pSleep->prevTimeUs) + pPgState->stats.totalSleepTimeUs;
    }

    // Update the previous sample sleep time
    pSleep->prevTimeUs = pPgState->stats.totalSleepTimeUs;
}

/*!
 * @brief Return powered up time related info of given PgCtrl
 *
 * It returns the following info -
 *    a. Delta of powered up time from last sample
 *    b. Current powered up time
 *
 * @param[in]  ctrlId   PgCtrl Id
 */
void
pgStatDeltaPoweredUpTimeGetUs
(
    LwU8                  ctrlId,
    PGSTATE_STAT_SAMPLE  *pPoweredUp
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);

    // Update current poweredUp time
    if ((pPgState->state == PMU_PG_STATE_PWR_ON) ||
        (pPgState->state == PMU_PG_STATE_DISALLOW))
    {
        pgStatPoweredUpTimeUpdate(ctrlId);
    }

    // Update the delta poweredUp time (Current poweredUp time - Last sample time)
    if (pPgState->stats.totalNonSleepTimeUs >= pPoweredUp->prevTimeUs)
    {
        pPoweredUp->deltaTimeUs = pPgState->stats.totalNonSleepTimeUs - pPoweredUp->prevTimeUs;
    }
    else
    {
        pPoweredUp->deltaTimeUs = (LW_U32_MAX - pPoweredUp->prevTimeUs) + pPgState->stats.totalNonSleepTimeUs;
    }

    // Update the previous sample poweredUp time
    pPoweredUp->prevTimeUs = pPgState->stats.totalNonSleepTimeUs;
}

/*!
 * @brief Get the current total sleep time for the specified controller.
 *
 * The evaluation of sleep time is done within a critical section to ensure no
 * context switch. This may delay the exit process and add to exit latency.
 *
 * @param[in]  ctrlId        PG-controller ID
 * @param[out] pTimeStampNs  Pointer to FLCN_TIMESTAMP to return evaluated time
 */
void
pgStatLwrrentSleepTimeGetNs
(
    LwU8            ctrlId,
    FLCN_TIMESTAMP *pTimeStampNs
)
{
    OBJPGSTATE       *pPgState   = GET_OBJPGSTATE(ctrlId);
    PGSTATE_STATS_64 *pPgStats64 = PGSTATE_STATS_64_GET(pPgState);
    FLCN_TIMESTAMP    timeStampNs;
    FLCN_TIMESTAMP    timeStampNsDiff;
    FLCN_TIMESTAMP    lwrrentSleepTimeNs = { 0 };

    //
    // This interface can't work without the PMU_PG_STATS_64 feature.  Follow-up
    // CLs should move this to its own module so it will be completely
    // compiled/linked out.
    //
    if (!PMUCFG_FEATURE_ENABLED(PMU_PG_STATS_64) ||
        (pPgStats64 == NULL))
    {
        PMU_BREAKPOINT();
        goto pgStatLwrrentSleepTimeGetNs_exit;
    }

    // Code should not get interrupted/preempted when sampling timers.
    appTaskCriticalEnter();
    {
        lwrrentSleepTimeNs.data = pPgStats64->totalSleepTimeNs;

        if (pPgState->state == PMU_PG_STATE_PWR_OFF)
        {
            osPTimerTimeNsLwrrentGet(&timeStampNs);

            //
            // (timeStampNs - pPgState->timeStampCacheNs) gives sleep time for
            // specified controller after totalSleepTimeUs was last updated
            // i.e. at event PMU_PG_EVENT_POWERED_DOWN.
            //
            lw64Sub(&timeStampNsDiff.data, &timeStampNs.data,
                    &pPgState->timeStampCacheNs.data);
            lw64Add(&lwrrentSleepTimeNs.data, &lwrrentSleepTimeNs.data,
                    &timeStampNsDiff.data);
        }
    }
    appTaskCriticalExit();

pgStatLwrrentSleepTimeGetNs_exit:
    *pTimeStampNs = lwrrentSleepTimeNs;
}
