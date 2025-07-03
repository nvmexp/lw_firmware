/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrpolicies_30.h
 * @brief Structure specification for the Power Policy functionality container -
 * all as specified by the Power Policy Table 3.X.
 *
 * https://wiki.lwpu.com/engwiki/index.php/Resman/PState/Data_Tables/Power_Tables/Power_Policy_Table_3.X
 *
 * All of the actual functionality is found in @ref pwrpolicy.c.
 */

#ifndef PWRPOLICIES_30_H
#define PWRPOLICIES_30_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/3x/pwrpolicies_3x.h"

/* ------------------------- Types Definitions ----------------------------- */
/*!
 * PWR_POLICIES_30 structure.
 * @note Lwrrently unused.
 */
typedef struct PWR_POLICIES_30
{
    /*
     * All PWR_POLICIES fields.
     */
    PWR_POLICIES_3X super;
} PWR_POLICIES_30;

/* ------------------------- Macros ---------------------------------------- */
/*!
 * Alias pwrPolicies3XCallbackBody virtual function for PWR_POLICY_30.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_30)
#define pwrPolicies3xCallbackBody(ticksExpired)                                \
    pwrPolicies3xCallbackBody_30((ticksExpired))
#endif

/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * Global PWR_POLICIES interfaces
 */
PwrPoliciesEvaluate (pwrPoliciesEvaluate_30)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyCallback", "pwrPoliciesEvaluate_30");

/*!
 * PWR_POLICIES_3X interfaces
 */
PwrPolicies3xCallbackBody (pwrPolicies3xCallbackBody_30)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyCallback", "pwrPolicies3xCallbackBody_30");

#endif // PWRPOLICIES_30_H

