/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrpolicy_bangbang.h
 * @brief @copydoc pwrpolicy_bangbang.c
 */

#ifndef PWRPOLICY_BANGBANG_H
#define PWRPOLICY_BANGBANG_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrpolicy_domgrp.h"

/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_POLICY_BANG_BANG_VF PWR_POLICY_BANG_BANG_VF;

/*!
 * Structure representing a workload-based power policy.
 */
struct PWR_POLICY_BANG_BANG_VF
{
    /*!
     * @copydoc PWR_POLICY_DOMGRP
     */
    PWR_POLICY_DOMGRP super;

    /*!
     * Ratio of the limit (PWR_POLICY::limitLwrr) below which the controlled
     * value (PWR_POLICY::valueLwrr) must fall in order for the Bang-Bang
     * algorithm to initiate the uncap action.
     *
     * Unitless Unsigned FXP 4.12.
     */
    LwUFXP4_12        uncapLimitRatio;

    /*!
     * Last timer value tracking the amount of time any HW_FAILSAFE slowdown (as
     * represented by RM_PMU_THERM_EVENT_META_ANY_HW_FS) has been violating, as
     * returned via @ref thermEventViolationGetTimer32().
     *
     * If this timer value increments over a sampling period, a HW_FAILSAFE
     * slowdown has been engaged, poisoning the sampled power data (i.e. dynamic
     * power was artificially low due to LDIV slowdown) and the _PSTATE capping
     * algorithm should not uncap due to power less than threshold.
     */
    LwU32             thermHwFsTimernsPrev;

    /*!
     * Last action taken by the _BANG_BANG_VF algorithm.
     *
     * @ref LW2080_CTRL_PMGR_PWR_POLICY_BANG_BANG_VF_ACTION_<xyz>
     */
    LwU8              action;
    /*!
     * Input VF point to the _BANG_BANG_VF algorithm for @ref action.
     */
    RM_PMU_PMGR_PWR_POLICY_QUERY_POLICY_BANG_BANG_VF_INDEXES
                      input;
    /*!
     * Output VF point of the _BANG_BANG_VF algorithm as decided by the @ref
     * action.
     */
    RM_PMU_PMGR_PWR_POLICY_QUERY_POLICY_BANG_BANG_VF_INDEXES
                      output;
};

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * PWR_POLICY interfaces
 */
BoardObjGrpIfaceModel10ObjSet       (pwrPolicyGrpIfaceModel10ObjSetImpl_BANG_BANG_VF)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrPolicyGrpIfaceModel10ObjSetImpl_BANG_BANG_VF");
BoardObjIfaceModel10GetStatus           (pwrPolicyIfaceModel10GetStatus_BANG_BANG_VF)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrPolicyIfaceModel10GetStatus_BANG_BANG_VF");

/*!
 * PWR_POLICY_DOMGRP interfaces
 */
PwrPolicyDomGrpEvaluate (pwrPolicyDomGrpEvaluate_BANG_BANG_VF)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyDomGrpEvaluate_BANG_BANG_VF");

/* ------------------------- Debug Macros ---------------------------------- */

#endif // PWRPOLICY_BANGBANG_H
