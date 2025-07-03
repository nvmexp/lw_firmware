/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: acr_timer_access_tu10x.c
 */
#include "acr.h"
#ifdef SEC2_PRESENT
#include "dev_sec_csb.h"
#endif

#ifdef ACR_BUILD_ONLY
#include "config/g_acrlib_private.h"
#include "config/g_acr_private.h"
#else
#include "g_acrlib_private.h"
#endif
/*!
 * @brief Get current time in NS
 *
 * @param[out] pTime TIMESTAMP struct to hold the current time
 */
void
acrlibGetLwrrentTimeNs_TU10X
(
    PACR_TIMESTAMP pTime
)
{
    LwU32 hi;
    LwU32 lo;

    // Read loop specified by 'dev_timer.ref'.
#if defined(AHESASC) || defined(ASB) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
    do
    {
        hi = ACR_REG_RD32(CSB, LW_CSEC_FALCON_PTIMER1);
        lo = ACR_REG_RD32(CSB, LW_CSEC_FALCON_PTIMER0);
    }
    while (hi != ACR_REG_RD32(CSB, LW_CSEC_FALCON_PTIMER1));
#elif defined(ACR_UNLOAD)
    do
    {
        hi = ACR_REG_RD32(CSB, LW_CPWR_FALCON_PTIMER1);
        lo = ACR_REG_RD32(CSB, LW_CPWR_FALCON_PTIMER0);
    }
    while (hi != ACR_REG_RD32(CSB, LW_CPWR_FALCON_PTIMER1));
#else
    ct_assert(0);
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
 * @return ACR_ERROR_TIMEOUT if timed out
 */
ACR_STATUS
acrlibCheckTimeout_TU10X
(
    LwU32         timeoutNs,
    ACR_TIMESTAMP startTimeNs,
    LwS32         *pTimeoutLeftNs
)
{
    LwU32 elapsedTime = acrlibGetElapsedTimeNs_HAL(pAcrlib, &startTimeNs);

    *pTimeoutLeftNs = timeoutNs - elapsedTime;
    
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
acrlibGetElapsedTimeNs_TU10X
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
#if defined(AHESASC) || defined(ASB) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
    return ACR_REG_RD32(CSB, LW_CSEC_FALCON_PTIMER0) - pTime->parts.lo;
#elif defined(ACR_UNLOAD)
    return ACR_REG_RD32(CSB, LW_CPWR_FALCON_PTIMER0) - pTime->parts.lo;
#else
    ct_assert(0);
#endif
}
