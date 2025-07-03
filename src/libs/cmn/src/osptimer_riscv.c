/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    osptimer_riscv.c
 * @copydoc osptimer_riscv.h
 */

/* ------------------------ System includes --------------------------------- */
#include <riscv_csr.h>

/* ------------------------ Application includes ---------------------------- */
#include "osptimer.h"

//
// Pre-GH100 the timer needs to be multiplied by 1 << 5 = 32 since the 5
// lowest bits that are always clear in PTIMER are actually removed in the
// timer CSR (so the CSR records 32ns-increments instead of ns).
// On GH100+ this behavior is changed so that the timer CSR is in units
// of nanoseconds.
//
#define OS_TIMER_64B_GET()  ((LwU64)(csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME) << \
                                     LWRISCV_MTIME_TICK_SHIFT))

/* ------------------------ Type definitions -------------------------------- */
/* ------------------------ External definitions ---------------------------- */
/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Prototypes -------------------------------------- */
/* ------------------------ Global variables -------------------------------- */
/* ------------------------ Macros ------------------------------------------ */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief   Get the current time in nanoseconds.
 *
 * A timestamp is a 64-bit unsigned integer. @see FLCN_TIMESTAMP for further
 * information on using the timestamp structure directly.
 *
 * @param[out]  pTime   Current timestamp, 8-byte-aligned pointer.
 *
 * @sa  timerGetElapsedNs
 */
void
osPTimerTimeNsLwrrentGet_IMPL
(
    PFLCN_TIMESTAMP pTime
)
{
    pTime->data = OS_TIMER_64B_GET();
}

/*!
 * @brief   Get the current time in nanoseconds and place into a timestamp,
 *          using a pointer to 32-bit-aligned time data.
 *
 * A timestamp is a 64-bit unsigned integer. @see FLCN_TIMESTAMP for further
 * information on using the timestamp structure directly.
 *
 * This function doesn't assume that the timestamp is 8-byte aligned,
 * it will work with a 4-byte aligned timestamp pointer as well.
 *
 * @param[out]  pTimeData   Current timestamp, 4-byte-aligned pointer.
 *
 * @sa  timerGetElapsedNs
 */
void
osPTimerTimeNsLwrrentGetUnaligned
(
    LwU64_ALIGN32 *pTimeData
)
{
    FLCN_TIMESTAMP time;
    time.data = OS_TIMER_64B_GET();
    *pTimeData = time.parts;
}

/*!
 * @brief   Measure the time in nanoseconds elapsed since 'pTime'.
 *
 * Since the time is measured in nanoseconds and returned as an LwU32, this
 * function will only measure time-spans less than 4.29 seconds (2^32-1
 * nanoseconds).
 *
 * @param[in]   pTime   Timestamp to measure from.
 *
 * @return Number of nanoseconds elapsed since 'pTime'.
 *
 * @sa  timerGetLwrrentNs
 */
LwU32
osPTimerTimeNsElapsedGet
(
    PFLCN_TIMESTAMP pTime
)
{
    FLCN_TIMESTAMP elapsed;

    elapsed.data = OS_TIMER_64B_GET() - pTime->data;

    //
    // Since time-spans are less than 4.29 secs, only the lower 32-bits of the
    // timestamps need to be compared. Subtracting these values will yield
    // the correct result.
    //
    return elapsed.parts.lo;
}

/*!
 * @brief   Measure the time in nanoseconds elapsed since 'pTime',
 *          using a pointer to 32-bit-aligned time data.
 *
 * Since the time is measured in nanoseconds and returned as an LwU32, this
 * function will only measure time-spans less than 4.29 seconds (2^32-1
 * nanoseconds).
 *
 * @param[in]   pTime   Timestamp to measure from.
 *
 * @return Number of nanoseconds elapsed since 'pTime'.
 *
 * @sa  timerGetLwrrentNs
 */
LwU32
osPTimerTimeNsElapsedGetUnaligned
(
    LwU64_ALIGN32 *pTimeData
)
{
    FLCN_TIMESTAMP time;
    FLCN_TIMESTAMP elapsed;

    time.parts = *pTimeData;

    elapsed.data = OS_TIMER_64B_GET() - time.data;

    //
    // Since time-spans are less than 4.29 secs, only the lower 32-bits of the
    // timestamps need to be compared. Subtracting these values will yield
    // the correct result.
    //
    return elapsed.parts.lo;
}

/*!
 * (TODO - This is replicated from LS code for quickly upgrading mutex infra to
 * support HS)
 *
 * WARNING: This timer is inselwre even though this reads "_hs". For secure
 * timer needs, we must use SCI PTIMER.  Bug 1809879 is filed tracking this.
 *
 * WARNING: Current design can only work for timeout < 4.29 secs, we need to
 * consider the case when someone needs timeout > 4.29 secs.
 *
 * @brief   Get the current time in nanoseconds at HS mode.
 * A timestamp is a 64-bit unsigned integer. @see FLCN_TIMESTAMP for further
 * information on using the timestamp structure directly.
 *
 * @param[out]  pTime   Current timestamp.
 *
 * @sa  timerGetElapsedNs
 */
void
osPTimerTimeNsLwrrentGet_hs
(
    PFLCN_TIMESTAMP pTime
)
{
    pTime->data = OS_TIMER_64B_GET();
}

/*!
 * (TODO - This is replicated from LS code for quickly upgrading mutex infra to
 * support HS)
 *
 * WARNING: This timer is inselwre even though this reads "_hs". For secure
 * timer needs, we must use SCI PTIMER.  Bug 1809879 is filed tracking this.
 *
 * WARNING: Current design can only work for timeout < 4.29 secs, we need to
 * consider the case when someone needs timeout > 4.29 secs.
 *
 * @brief   Measure the time in nanoseconds elapsed since 'pTime' at HS mode.
 *
 * Since the time is measured in nanoseconds and returned as an LwU32, this
 * function will only measure time-spans less than 4.29 seconds (2^32-1
 * nanoseconds).
 *
 * @param[in]   pTime   Timestamp to measure from.
 *
 * @return Number of nanoseconds elapsed since 'pTime'.
 *
 * @sa  timerGetLwrrentNs
 */
LwU32
osPTimerTimeNsElapsedGet_hs
(
    PFLCN_TIMESTAMP pTime
)
{
    LwU64 elapsed = OS_TIMER_64B_GET() - pTime->data;

    //
    // Since time-spans are less than 4.29 secs, only the lower 32-bits of the
    // timestamps need to be compared. Subtracting these values will yield
    // the correct result.
    //
    return (LwU32)(elapsed & 0xFFFFFFFFULL);
}

/*!
 * @brief   Spin-wait while checking a condition. Exit if either a timeout time
 *          has been reached or the condition has been reached.
 *
 * Note that this function is a spin-loop and not friendly to other tasks. It
 * has no yields and thus will not allow other tasks to do work while waiting
 * (aside from normal context-switching).
 *
 * @param[in]   fp      Function pointer to the function to be called for
 *                      checking whether the exit condition has been met.
 *                      Ignored if 'NULL'.
 * @param[in]   pArgs   Pointer to the list of arguments to pass to 'func'.
 * @param[in]   delayNs Amount of nanoseconds to wait before timeout.
 *
 * @return  LW_TRUE     if the exit condition has been met before timeout
 * @return  LW_FALSE    if waiting for the exit has timed out
 */
LwBool
osPTimerCondSpinWaitNs_IMPL
(
    OS_PTIMER_COND_FUNC fp,
    void               *pArgs,
    LwU32               delayNs
)
{
    LwU64 ptimer0Start;

    ptimer0Start = OS_TIMER_64B_GET();
    do
    {
        if ((fp != NULL))
        {
            if (fp(pArgs)) {
                return LW_TRUE;
            }
        }
    } while ((OS_TIMER_64B_GET() - ptimer0Start) < delayNs);

    //
    // Check condition one more time to prevent false positive timeouts.
    // These can occur when the spinloop is preempted after the condition check
    // but before the timeout check. By the time this task is rescheduled
    // the timeout may have elapsed, but the condition may have been met
    // as well.
    //
    // This extra check does have the side-effect of possibly admitting false
    // negatives - it can fail to report actual timeouts. But in these cases
    // the timeout does not indicate a hang since the condition has been met.
    // In this sense false negatives are less harmful than false positives
    // (which usually trigger halts/breakpoints that lead to system failure).
    //
    return (fp != NULL) ? fp(pArgs) : LW_FALSE;
}

/*!
 * (TODO - This is replicated from LS code for quickly support hdcp22wired, i2c HS request.)
 *
 * WARNING: This timer is inselwre even though this reads "_hs". For secure
 * timer needs, we must use SCI PTIMER.  Bug 1809879 is filed tracking this.
 *
 * WARNING: Current design can only work for delay < 4.29 secs, we need to
 * consider the case when someone needs timeout > 4.29 secs.
 *
 * @copydoc osPTimerCondSpinWaitNs()
 */
LwBool
osPTimerCondSpinWaitNs_hs
(
    OS_PTIMER_COND_FUNC fp,
    void               *pArgs,
    LwU32               delayNs
)
{
    LwU64 ptimer0Start;

    ptimer0Start = OS_TIMER_64B_GET();
    do
    {
        if ((fp != NULL))
        {
            if (fp(pArgs)) {
                return LW_TRUE;
            }
        }
    } while ((OS_TIMER_64B_GET() - ptimer0Start) < delayNs);

    //
    // Check condition one more time to prevent false positive timeouts.
    // These can occur when the spinloop is preempted after the condition check
    // but before the timeout check. By the time this task is rescheduled
    // the timeout may have elapsed, but the condition may have been met
    // as well.
    //
    // This extra check does have the side-effect of possibly admitting false
    // negatives - it can fail to report actual timeouts. But in these cases
    // the timeout does not indicate a hang since the condition has been met.
    // In this sense false negatives are less harmful than false positives
    // (which usually trigger halts/breakpoints that lead to system failure).
    //
    return (fp != NULL) ? fp(pArgs) : LW_FALSE;
}

/*!
 * @brief   Spin-wait until requested time expires.
 *
 * @param[in]   delayNs Amount of nanoseconds to wait.
 */
void
osPTimerSpinWaitNs
(
    LwU32   delayNs
)
{
    (void)osPTimerCondSpinWaitNs(NULL, NULL, delayNs);
}

/* ------------------------ Static Functions -------------------------------- */
