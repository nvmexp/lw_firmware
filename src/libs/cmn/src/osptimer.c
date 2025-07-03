/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    osptimer.c
 * @copydoc osptimer.h
 */

/* ------------------------ System includes --------------------------------- */
/* ------------------------ Application includes ---------------------------- */
#include "osptimer.h"
#include "csb.h"

#ifdef UPROC_RISCV
    #error "osptimer.c not supported for RISCV! Use osptimer_riscv.c instead!"
#endif // UPROC_RISCV

/* -- Messy includes -- */
#if DPU_RTOS
    #include "dev_disp_falcon.h"
    #define OS_TIMER_PTIMER0_GET()      REG_RD32(CSB, LW_PDISP_FALCON_PTIMER0)
    #define OS_TIMER_PTIMER1_GET()      REG_RD32(CSB, LW_PDISP_FALCON_PTIMER1)
#elif PMU_RTOS
    #include "dev_pwr_falcon_csb.h"
    #define OS_TIMER_PTIMER0_GET()      REG_RD32(CSB, LW_CMSDEC_FALCON_PTIMER0)
    #define OS_TIMER_PTIMER1_GET()      REG_RD32(CSB, LW_CMSDEC_FALCON_PTIMER1)
#elif SEC2_RTOS
    #include "dev_sec_csb.h"
    #define OS_TIMER_PTIMER0_GET()      REG_RD32(CSB, LW_CSEC_FALCON_PTIMER0)
    #define OS_TIMER_PTIMER1_GET()      REG_RD32(CSB, LW_CSEC_FALCON_PTIMER1)
#elif SOE_RTOS
    #include "dev_soe_csb.h"
    #define OS_TIMER_PTIMER0_GET()      REG_RD32(CSB, LW_CSOE_FALCON_PTIMER0)
    #define OS_TIMER_PTIMER1_GET()      REG_RD32(CSB, LW_CSOE_FALCON_PTIMER1)
#elif GSPLITE_RTOS
    #include "dev_gsp_csb.h"
    #define OS_TIMER_PTIMER0_GET()      REG_RD32(CSB, LW_CGSP_FALCON_PTIMER0)
    #define OS_TIMER_PTIMER1_GET()      REG_RD32(CSB, LW_CGSP_FALCON_PTIMER1)
#elif FBFLCN_BUILD
    #include "dev_fbfalcon_csb.h"
    #define OS_TIMER_PTIMER0_GET()      REG_RD32(CSB, LW_CFBFALCON_FALCON_PTIMER0)
    #define OS_TIMER_PTIMER1_GET()      REG_RD32(CSB, LW_CFBFALCON_FALCON_PTIMER1)
#else
    #error !!! Unsupported falcon !!!
#endif

/* ------------------------ Type definitions -------------------------------- */
/* ------------------------ External definitions ---------------------------- */
/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Prototypes -------------------------------------- */
/* ------------------------ Global variables -------------------------------- */
/* ------------------------ Macros ------------------------------------------ */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief      Get the current time in nanoseconds.
 *
 *             Takes a snapshot of the current PTIMER time registers (PTIMER0
 *             and PTIMER1) and stores them in the FLCN_TIMESTAMP structure.
 *             Time is expressed in elapsed nanoseconds since 00:00 GMT, January
 *             1, 1970 (zero hour). See 'dev_timer.ref' for additional
 *             information. @ref FLCN_TIMESTAMP for further information on using
 *             the timestamp structure directly
 *
 * @see        timerGetElapsedNs
 *
 * @note       A timestamp is a 64-bit unsigned integer.
 *
 * @param[out] pTime  Pointer to store the current timestamp.
 */
void
osPTimerTimeNsLwrrentGet_IMPL
(
    PFLCN_TIMESTAMP pTime
)
{
    LwU32 hi;
    LwU32 lo;

    // Read loop specified by 'dev_timer.ref'.
    do
    {
        hi = OS_TIMER_PTIMER1_GET();
        lo = OS_TIMER_PTIMER0_GET();
    }
    while (hi != OS_TIMER_PTIMER1_GET());

    // Stick the values into the timestamp.
    pTime->parts.hi = hi;
    pTime->parts.lo = lo;
}

/*!
 * @brief      Measure the time in nanoseconds elapsed since 'pTime'.
 *
 * @note       Since the time is measured in nanoseconds and returned as an
 *             LwU32, this function will only measure time-spans less than 4.29
 *             seconds (2^32-1 nanoseconds).
 *
 * @see        timerGetLwrrentNs
 *
 * @param[in]  pTime  Timestamp to measure from.
 *
 * @return     Number of nanoseconds elapsed since 'pTime'.
 */
LwU32
osPTimerTimeNsElapsedGet
(
    PFLCN_TIMESTAMP pTime
)
{
    //
    // Since time-spans are less than 4.29 secs, only the lower 32-bits of the
    // timestamps need to be compared. Subtracting these values will yield
    // the correct result. We also only need to read the PTIMER0 and avoid the
    // read loop ilwolved with reading both PTIMER0 and PTIMER1.
    //
    return OS_TIMER_PTIMER0_GET() - pTime->parts.lo;
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
 * @brief      Get the current time in nanoseconds at HS mode.
 *
 *             Takes a snapshot of the current PTIMER time registers (PTIMER0 and
 *             PTIMER1) and stores them in the FLCN_TIMESTAMP structure. Time is
 *             expressed in elapsed nanoseconds since 00:00 GMT, January 1, 1970
 *             (zero hour). See 'dev_timer.ref' for additional information.
 *
 *             A timestamp is a 64-bit unsigned integer.
 *
 * @see        FLCN_TIMESTAMP for further information on using the timestamp
 *             structure directly.
 *
 * @param[out] pTime  Current timestamp.
 *
 * @see        timerGetElapsedNs
 */
void
osPTimerTimeNsLwrrentGet_hs
(
    PFLCN_TIMESTAMP pTime
)
{
    LwU32 hi;
    LwU32 lo;

    // Read loop specified by 'dev_timer.ref'.
    do
    {
        hi = OS_TIMER_PTIMER1_GET();
        lo = OS_TIMER_PTIMER0_GET();
    }
    while (hi != OS_TIMER_PTIMER1_GET());

    // Stick the values into the timestamp.
    pTime->parts.hi = hi;
    pTime->parts.lo = lo;
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
 * @brief      Measure the time in nanoseconds elapsed since 'pTime' at HS mode.
 *
 *             Since the time is measured in nanoseconds and returned as an LwU32,
 *             this function will only measure time-spans less than 4.29 seconds
 *             (2^32-1 nanoseconds).
 *
 * @param[in]  pTime  Timestamp to measure from.
 *
 * @return     Number of nanoseconds elapsed since 'pTime'.
 *
 * @see        timerGetLwrrentNs
 */
LwU32
osPTimerTimeNsElapsedGet_hs
(
    PFLCN_TIMESTAMP pTime
)
{
    //
    // Since time-spans are less than 4.29 secs, only the lower 32-bits of the
    // timestamps need to be compared. Subtracting these values will yield
    // the correct result. We also only need to read the PTIMER0 and avoid the
    // read loop ilwolved with reading both PTIMER0 and PTIMER1.
    //
    return OS_TIMER_PTIMER0_GET() - pTime->parts.lo;
}

/*!
 * @brief      Spin-wait while checking a condition. Exit if either a timeout
 *             time has been reached or the condition has been reached.
 *
 * @note       This function is a spin-loop and not friendly to other tasks. It
 *             has no yields and thus will not allow other tasks to do work
 *             while waiting (aside from normal context-switching).
 *
 * @param[in]  fp       Function pointer to the function to be called for
 *                      checking whether the exit condition has been met.
 *                      Ignored if 'NULL'.
 * @param[in]  pArgs    Pointer to the list of arguments to pass to 'func'.
 * @param[in]  delayNs  Amount of nanoseconds to wait before timeout.
 *
 * @return     LW_TRUE     if the exit condition has been met before timeout
 * @return     LW_FALSE    if waiting for the exit condition has timed out
 */
LwBool
osPTimerCondSpinWaitNs_IMPL
(
    OS_PTIMER_COND_FUNC fp,
    void               *pArgs,
    LwU32               delayNs
)
{
    if (delayNs > 0U)
    {
        LwU32 ptimer0Start;

        //
        // Since we are only allowing time spans of less than 4.29 seconds,
        // we only need to look at the lower 32-bits of the PTIMER timestamps.
        // As such, we don't need to get a full time stamp.
        //
        ptimer0Start = OS_TIMER_PTIMER0_GET();
        do
        {
            if (fp != NULL)
            {
                if (fp(pArgs))
                {
                    return LW_TRUE;
                }
            }
        } while ((OS_TIMER_PTIMER0_GET() - ptimer0Start) < delayNs);
    }

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
 * (TODO - This is replicated from LS code for quickly support hdcp22wired, i2c
 * HS request.)
 *
 * WARNING: This timer is inselwre even though this reads "_hs". For secure
 * timer needs, we must use SCI PTIMER.  Bug 1809879 is filed tracking this.
 *
 * WARNING: Current design can only work for delay < 4.29 secs, we need to
 * consider the case when someone needs timeout > 4.29 secs.
 *
 * @copydoc osPTimerCondSpinWaitNs()
 *
 * @param[in]  fp       Function pointer to the function to be called for
 *                      checking whether the exit condition has been met.
 *                      Ignored if 'NULL'.
 * @param[in]  pArgs    Pointer to the list of arguments to pass to 'func'.
 * @param[in]  delayNs  Amount of nanoseconds to wait before timeout.
 *
 * @return     LW_TRUE     If the exit condition has been met before timeout.
 * @return     LW_FALSE    If waiting for the exit has timed out.
 */
LwBool
osPTimerCondSpinWaitNs_hs
(
    OS_PTIMER_COND_FUNC fp,
    void               *pArgs,
    LwU32               delayNs
)
{
    LwU32 ptimer0Start;

    //
    // Since we are only allowing time spans of less than 4.29 seconds, we only
    // need to look at the lower 32-bits of the PTIMER timestamps. As such, we
    // don't need to get a full time stamp.
    //
    ptimer0Start = OS_TIMER_PTIMER0_GET();
    do
    {
        if (fp != NULL)
        {
            if (fp(pArgs))
            {
                return LW_TRUE;
            }
        }
    } while ((OS_TIMER_PTIMER0_GET() - ptimer0Start) < delayNs);

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
 * @brief      Spin-wait until requested time expires.
 *
 * @param[in]  delayNs  Amount of nanoseconds to wait.
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
