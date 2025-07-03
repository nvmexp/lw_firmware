/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pwrpolicies_2x.c
 * @brief  Interface specification for PWR_POLICIES.
 *
 * The PWR_POLICIES is a group of PWR_POLICY and some global data fields.
 * Functions inside this file applies to the whole PWR_POLICIES instead of
 * a single PWR_POLICY. for PWR_POLICY functions, please refer to
 * pwrpolicy_2x.c.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objpmgr.h"
#include "pmgr/2x/pwrpolicies_2x.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Power Policies 2.X main evaluation function.
 *
 * @copydoc PwrPoliciesEvaluate
 */
void
pwrPoliciesEvaluate_2X
(
    PWR_POLICIES    *pPolicies,
    LwU32            expiredPolicyMask,
    BOARDOBJGRPMASK *pExpiredViolationMask
)
{
    LwU8    i;

    //
    // Update all PWR_POLICYs to cache and filter their latest readings from
    // PWR_CHANNELs.
    //
    FOR_EACH_INDEX_IN_MASK(32, i, expiredPolicyMask)
    {
        PWR_POLICY *pPolicy;

        pPolicy = PWR_POLICY_GET(i);
        if (pPolicy == NULL)
        {
            PMU_BREAKPOINT();
            return;
        }

        (void)pwrPolicyFilter_2X(pPolicy, pPolicies->pMon);
    }
    FOR_EACH_INDEX_IN_MASK_END;

    pwrPoliciesEvaluate_SUPER(pPolicies, LW_U32_MAX, pExpiredViolationMask);
}
