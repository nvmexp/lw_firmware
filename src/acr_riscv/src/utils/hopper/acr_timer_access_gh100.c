/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: acr_timer_access_gh100.c
 */
#include "acr.h"

#include "config/g_acr_private.h"
/*!
 * @brief Get current time in NS
 *
 * @param[out] pTime TIMESTAMP struct to hold the current time
 */
void
acrGetLwrrentTimeNs_GH100
(
    PACR_TIMESTAMP pTime
)
{
    LwU32 hi;
    LwU32 lo;

    // Read loop specified by 'dev_timer.ref'.
    do
    {
        hi = ACR_REG_RD32(PRGNLCL, LW_PRGNLCL_FALCON_PTIMER1);
        lo = ACR_REG_RD32(PRGNLCL, LW_PRGNLCL_FALCON_PTIMER0);
    }
    while (hi != ACR_REG_RD32(PRGNLCL, LW_PRGNLCL_FALCON_PTIMER1));

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
 * @return ACR_ERROR_TIMEOUT if timed out
 */
ACR_STATUS
acrCheckTimeout_GH100
(
    LwU32         timeoutNs,
    ACR_TIMESTAMP startTimeNs,
    LwS32         *pTimeoutLeftNs
)
{
    LwU32 elapsedTime = acrGetElapsedTimeNs_HAL(pAcr, &startTimeNs);

    if ((timeoutNs - elapsedTime) > LW_S32_MAX)
    {
        return ACR_ERROR_VARIABLE_SIZE_OVERFLOW;
    }

    *pTimeoutLeftNs = (LwS32)(timeoutNs - elapsedTime);
    
    return (elapsedTime >= timeoutNs)?ACR_ERROR_TIMEOUT:ACR_OK;
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
acrGetElapsedTimeNs_GH100
(
    const PACR_TIMESTAMP pTime
)
{
    //
    // Since time-spans are less than 4.29 secs, only the lower 32-bits of the
    // timestamps need to be compared. Subtracting these values will yield
    // the correct result. We also only need to read the PTIMER0 and avoid the
    // read loop ilwolved with reading both PTIMER0 and PTIMER1.
    //
    return ACR_REG_RD32(PRGNLCL, LW_PRGNLCL_FALCON_PTIMER0) - pTime->parts.lo;
}
