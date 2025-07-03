/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    lwostmrcmn.c
 * @copydoc lwostmrcmn.h
 */

/* ------------------------ System includes --------------------------------- */
/* ------------------------ Application includes ---------------------------- */
#include "lwostmrcmn.h"
#include "lwrtos.h"

/* ------------------------ Types definitions ------------------------------- */
/* ------------------------ External definitions ---------------------------- */
/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Prototypes -------------------------------------- */
/* ------------------------ Global variables -------------------------------- */
/* ------------------------ Macros ------------------------------------------ */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief      Compares two time stamps.
 *
 * @details    Due to BUG in GCC compiler (BUG@1802031), we are modifying the
 *             MACRO such that it does not use the combination of SUB.W + CMP
 *             instructions.
 *
 *             Warning:: Please do NOT colwert this to MACRO as the MACRO will
 *             generete the SUB.w instruction which which set the overflow
 *             flags. Due to compiler bug (BUG@1802031) the next compare
 *             instruction will use the overflow flag instead of re-callwlating
 *             it and causing incorrect behavior.
 *
 * @note       Even though the numbers are LwU32, if the difference between
 *             ticksA & ticksB is positive and greater than 0x80000000 this
 *             function will consider them as negative number. In Ideal
 *             scenario, we never expect this to occur, so this is safe.
 *
 * @param[in]  ticksA  First time stamp to compare.
 * @param[in]  ticksB  Second time stamp to compare.
 *
 * @return     LW_TRUE if time stamp ticksA is before ticksB
 * @return     LW_FALSE if ticksB is before or the same time as ticksA.
 */
LwBool
osTmrIsBefore
(
    LwU32 ticksA,
    LwU32 ticksB
)
{
    return (osTmrGetTicksDiff(ticksA, ticksB) > 0);
}

/*!
 * @brief      Callwlate the difference between two time stamps.
 *
 * @param[in]  ticksA  The first time stamp, unsigned.
 * @param[in]  ticksB  The second time stamp, unsigned.
 *
 * @return     Difference between ticksB and ticksA, signed.
 */
LwS32
osTmrGetTicksDiff
(
    LwU32 ticksA,
    LwU32 ticksB
)
{
    return ((LwS32)(ticksB - ticksA));
}

/*!
 * @brief   Wait while checking a condition. Exit if either a timeout time has
 *          been reached or the condition has been reached.
 *
 * @note    Unlike @ref osPTimerCondSpinWaitNs_IMPL this function is not using
  *         a spin-loop and therefore it is friendly to other tasks.
 *
 * @param[in]   fp      Function pointer to the function to be called for
 *                      checking whether the exit condition has been met.
 *                      Ignored if 'NULL'.
 * @param[in]   pArgs   Pointer to the list of arguments to pass to 'func'.
 * @param[in]   spinDelayNs     Amount of nanoseconds to spin-wait.
 * @param[in]   totalDelayNs    Amount of nanoseconds to yield-wait.
 *
 * @return     LW_TRUE     if the exit condition has been met before timeout
 * @return     LW_FALSE    if waiting for the exit condition has timed out
 */
LwBool
osTmrCondWaitNs
(
    OS_PTIMER_COND_FUNC fp,
    void               *pArgs,
    LwU32               spinDelayNs,
    LwU32               totalDelayNs
)
{
    FLCN_TIMESTAMP timerStart;

    osPTimerTimeNsLwrrentGet(&timerStart);

    totalDelayNs = LW_MAX(totalDelayNs, spinDelayNs);

    if (osPTimerCondSpinWaitNs(fp, pArgs, spinDelayNs))
    {
        return LW_TRUE;
    }

    if (totalDelayNs > spinDelayNs)
    {
        do
        {
            if (osPTimerCondSpinWaitNs(fp, pArgs, 0U))
            {
                return LW_TRUE;
            }
            lwrtosYield();
        } while (osPTimerTimeNsElapsedGet(&timerStart) < totalDelayNs);
    }

    //
    // Check condition one more time to prevent false positive timeouts.
    // These can occur when the loop is preempted by yielding after the
    // condition check. By the time this task is rescheduled the timeout
    // may have elapsed, but the condition may have been met as well.
    //
    // This extra check does have the side-effect of possibly admitting false
    // negatives - it can fail to report actual timeouts. But in these cases
    // the timeout does not indicate a hang since the condition has been met.
    // In this sense false negatives are less harmful than false positives
    // (which usually trigger halts/breakpoints that lead to system failure).
    //
    return osPTimerCondSpinWaitNs(fp, pArgs, 0U);
}

/*!
 * @brief   Wait until requested time expires.
 *
 * @note    Suited for longer time periods than @ref osPTimerSpinWaitNs.
 *
 * @param[in]   delayNs     Amount of nanoseconds to yield-wait.
 */
void
osTmrWaitNs
(
    LwU32 delayNs
)
{
    (void)osTmrCondWaitNs(NULL, NULL, 0U, delayNs);
}
