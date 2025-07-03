/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pwrpolicy_30.c
 * @brief  Interface specification for a PWR_POLICY.
 *
 * A "Power Policy" is a logical construct representing a behavior (a limit
 * value) to enforce on a power rail, as monitored by a Power Channel.  This
 * limit is enforced via a mechanism specified in the Power Policy
 * implementation.
 *
 * PWR_POLICY is a virtual class/interface.  It alone does not specify a full
 * power policy/mechanism.  Classes which implement PWR_POLICY specify a full
 * mechanism for control.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objpmgr.h"
#include "pmu_objperf.h"
#include "pmgr/3x/pwrpolicy_30.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc PwrPolicyLoad
 */
FLCN_STATUS
pwrPolicyLoad_SUPER_30
(
    PWR_POLICY *pPolicy,
    LwU32       ticksNow
)
{

    // Initialize the policy expiration time (time of the next callback).
    pPolicy->ticksNext = ticksNow + pPolicy->ticksPeriod;

    return FLCN_OK;
}

/*!
 * @copydoc PwrPolicy30CheckExpiredAndUpdate
 */
LwBool
pwrPolicy30CheckExpiredAndUpdate
(
    PWR_POLICY *pPolicy,
    LwU32       ticksNow
)
{
    LwU32   expiredCount;

    // Overflow (wrap-around) safe check of the power policy expiration.
    if (OS_TMR_IS_BEFORE(ticksNow, pPolicy->ticksNext))
    {
        // Policy did not expire.
        return LW_FALSE;
    }

    // If policy expired more than once we'll process it only once.
    expiredCount =
        1 + (ticksNow - pPolicy->ticksNext) / pPolicy->ticksPeriod;
    pPolicy->ticksNext += (expiredCount * pPolicy->ticksPeriod);

    return LW_TRUE;
}

/* ------------------------- Private Functions ------------------------------ */
