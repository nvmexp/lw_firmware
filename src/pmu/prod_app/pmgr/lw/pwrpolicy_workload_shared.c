/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pwrpolicy_workload_shared.c
 * @copydoc pwrpolicy_workload_shared.h
 *
 * This file contains the functions that are shared between the legacy single
 * rail workload controller and new multirail workload controller.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objpmgr.h"
#include "pmu_objpg.h"
#include "pmgr/pwrpolicy_workload_shared.h"
#include "dbgprintf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

/*!
 * Applies a median filter to the workload/active capacitance term (w).  For
 * more information about median filters see:
 * http://en.wikipedia.org/wiki/Median_filter.
 *
 * @param[in]      workloadmWperMHzmV2
 *     Current workload value to insert into the median filter, in unsigned FXP
 *     20.12 format.
 * @param[in/out]  pMedian
 *     PWR_POLICY_WORKLOAD_MEDIAN_FILTER pointer which holds the state of the
 *     median filter, including the current set of samples.
 *
 * @return Filtered workload value in unsigned FXP 20.12 format.
 */
void
pwrPolicyWorkloadMedianFilterInsert_SHARED
(
    PWR_POLICY_WORKLOAD_MEDIAN_FILTER       *pMedian,
    PWR_POLICY_WORKLOAD_MEDIAN_FILTER_ENTRY *pEntryInput
)
{
    PWR_POLICY_WORKLOAD_MEDIAN_FILTER_ENTRY tmp;
    LwU8        i;
    LwU8        j;

    // Add latest sample into filter at head.
    pMedian->idx = (pMedian->idx + 1) % pMedian->sizeTotal;
    pMedian->pEntries[pMedian->idx] = *pEntryInput;

    // Update the size to handle case of starting the filter.
    if (pMedian->sizeLwrr < pMedian->sizeTotal)
    {
        pMedian->sizeLwrr++;
    }

    // Dump the current values of the median filter buffer.
    DBG_PRINTF_PMGR(("WM: w: 0x%08x, i: %d, s: %d, fs: %d\n", pEntryInput->workload, pMedian->idx, pMedian->sizeLwrr, pMedian->sizeTotal));

    // Copy the pEntries to the buffer to sort.
    memcpy(pMedian->pEntriesSort, pMedian->pEntries,
        pMedian->sizeLwrr * sizeof(PWR_POLICY_WORKLOAD_MEDIAN_FILTER_ENTRY));

    //
    // Sort the buffer - Naive sort should be just fine as our buffer sizes
    // should never be incredibly large.
    //
    for (i = 0; i < pMedian->sizeLwrr; i++)
    {
        for (j = i + 1; j < pMedian->sizeLwrr; j++)
        {
            if (pMedian->pEntriesSort[j].workload
                    < pMedian->pEntriesSort[i].workload)
            {
                tmp = pMedian->pEntriesSort[j];
                pMedian->pEntriesSort[j] = pMedian->pEntriesSort[i];
                pMedian->pEntriesSort[i] = tmp;
            }
        }
    }

    // Dump the sorted array of values.
    DBG_PRINTF_PMGR(("WMs:", 0, 0, 0, 0));
    for (i = 0; i < pMedian->sizeLwrr; i++)
    {
        DBG_PRINTF_PMGR((" 0x%08x", pMedian->pEntriesSort[i].workload, 0, 0, 0));
    }
    DBG_PRINTF_PMGR(("\n", 0, 0, 0, 0));

    //
    // If size of pMedian->pEntriesSort buffer is odd, return the middle/median
    // value.
    //
    i = pMedian->sizeLwrr / 2;
    if ((pMedian->sizeLwrr % 2) != 0)
    {
        pMedian->entryLwrrOutput = pMedian->pEntriesSort[i];
    }
    //
    // If size of pMedian->pEntriesSort buffer is even, return the average of
    // thte two middle values.
    //
    else
    {
        pMedian->entryLwrrOutput.workload =
            LW_UNSIGNED_ROUNDED_DIV(
                pMedian->pEntriesSort[i].workload +
                    pMedian->pEntriesSort[i - 1].workload,
                2);
        pMedian->entryLwrrOutput.value =
            LW_UNSIGNED_ROUNDED_DIV(
                pMedian->pEntriesSort[i].value +
                    pMedian->pEntriesSort[i - 1].value,
                2);
    }
}

/*!
 * Callwlates the engine residency in the period from last policy evaluation
 * to current policy evaluation.
 *
 * @param[inout]     pLpwrTime   Pointer to the last evaluation and sleep times
 * @param[in]        engine      Engine identifier
 *
 * @return Returns computed residency value
 */
LwUFXP20_12
pwrPolicyWorkloadResidencyCompute
(
    PWR_POLICY_WORKLOAD_LPWR_TIME    *pLpwrTime,
    LwU8                              engine
)
{
    LwU64 evalTime  = pLpwrTime->lastEvalTime.data;
    LwU64 sleepTime = pLpwrTime->lastSleepTime.data;
    LwU64 residency = 0;

    // Code should not get interrupted/preempted when sampling timers.
    appTaskCriticalEnter();
    {
        osPTimerTimeNsLwrrentGet(&pLpwrTime->lastEvalTime);
        pgStatLwrrentSleepTimeGetNs(engine, &pLpwrTime->lastSleepTime);
    }
    appTaskCriticalExit();

    //
    // Time in [ns] expired since 'lastEvalTime' (limited to 4.29 sec.).
    // Avoiding use of osPTimerTimeNsElapsedGet() to save on reg. accesses.
    //
    lw64Sub(&evalTime, &pLpwrTime->lastEvalTime.data, &evalTime);

    // Residency time in [ns] since 'lastEvalTime'.
    lw64Sub(&sleepTime, &pLpwrTime->lastSleepTime.data, &sleepTime);

    lw64Lsl(&residency, &sleepTime, 12); // residency = 52.12
    lwU64Div(&residency, &residency, &evalTime); // residency = 52.12 / 64.0 = 52.12

    if (LwU64_HI32(residency) != 0)
    {
        // catch overflow
        residency = LW_U32_MAX;
        PMU_BREAKPOINT();
    }
    return LW_MIN(LwU64_LO32(residency), LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 1));
}
/* ------------------------- Private Functions ------------------------------ */
