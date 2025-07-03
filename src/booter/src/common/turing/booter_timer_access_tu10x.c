/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: booter_timer_access_tu10x.c
 */

#include "booter.h"

#include "config/g_booter_private.h"
/*!
 * @brief Get current time in NS
 *
 * @param[out] pTime TIMESTAMP struct to hold the current time
 */
void
booterGetLwrrentTimeNs_TU10X
(
    PBOOTER_TIMESTAMP pTime
)
{
    LwU32 hi;
    LwU32 lo;

    // Read loop specified by 'dev_timer.ref'.
    do
    {
#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
        hi = BOOTER_REG_RD32(CSB, LW_CSEC_FALCON_PTIMER1);
        lo = BOOTER_REG_RD32(CSB, LW_CSEC_FALCON_PTIMER0);
#elif defined(BOOTER_RELOAD)
        hi = BOOTER_REG_RD32(CSB, LW_CLWDEC_FALCON_PTIMER1);
        lo = BOOTER_REG_RD32(CSB, LW_CLWDEC_FALCON_PTIMER0);
#endif
    }
#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    while (hi != BOOTER_REG_RD32(CSB, LW_CSEC_FALCON_PTIMER1));
#elif defined(BOOTER_RELOAD)
    while (hi != BOOTER_REG_RD32(CSB, LW_CLWDEC_FALCON_PTIMER1));
#endif

    // Stick the values into the timestamp.
    pTime->parts.lo = lo;
    pTime->parts.hi = hi;
}

/*!
 * @brief Check if given timeout has elapsed or not.
 *
 * @param[in] timeoutNs      Timeout in nano seconds
 * @param[in] startTimeNs    Start time for this timeout cycle
 * @param[in] pTimeoutLeftNs Remaining time in nanoseconds
 *
 * @return BOOTER_ERROR_TIMEOUT if timed out
 */
BOOTER_STATUS
booterCheckTimeout_TU10X
(
    LwU32            timeoutNs,
    BOOTER_TIMESTAMP startTimeNs,
    LwS32           *pTimeoutLeftNs
)
{
    LwU32 elapsedTime = booterGetElapsedTimeNs_HAL(pBooter, &startTimeNs);
    *pTimeoutLeftNs = timeoutNs - elapsedTime;
    return (elapsedTime >= timeoutNs)?BOOTER_ERROR_TIMEOUT:BOOTER_OK;
}

/*!
 * @brief Measure the time in nanoseconds elapsed since 'pTime'.
 *
 * Since the time is measured in nanoseconds and returned as an LwU32, this
 * function will only measure time-spans less than 4.29 seconds (2^32-1
 * nanoseconds).
 *
 * @param[in]   pTime   Timestamp to measure from.
 *
 * @return Number of nanoseconds elapsed since 'pTime'.
 *
 */
LwU32
booterGetElapsedTimeNs_TU10X
(
    const PBOOTER_TIMESTAMP pTime
)
{
    //
    // Since time-spans are less than 4.29 secs, only the lower 32-bits of the
    // timestamps need to be compared. Subtracting these values will yield
    // the correct result. We also only need to read the PTIMER0 and avoid the
    // read loop ilwolved with reading both PTIMER0 and PTIMER1.
    //
#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    return BOOTER_REG_RD32(CSB, LW_CSEC_FALCON_PTIMER0) - pTime->parts.lo;
#elif defined(BOOTER_RELOAD)
    return BOOTER_REG_RD32(CSB, LW_CLWDEC_FALCON_PTIMER0) - pTime->parts.lo;
#endif
}
