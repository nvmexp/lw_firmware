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
 * @file  pwrpolicy_limit.h
 * @brief @copydoc pwrpolicy_limit.c
 */

#ifndef PWRPOLICY_LIMIT_H
#define PWRPOLICY_LIMIT_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrpolicy.h"

/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_POLICY_LIMIT PWR_POLICY_LIMIT;

/*!
 * Structure representing a limit-based power policy.
 */
struct PWR_POLICY_LIMIT
{
    /*!
     * @copydoc PWR_POLICY
     */
    PWR_POLICY super;
};

/*!
 * @interface PWR_POLICY_LIMIT
 *
 * Evaluates the PWR_POLICY_LIMIT implementation, taking the latest values from
 * the monitored power channel and then specifying any required corrective
 * actions as new limits on other PWR_POLICY objects.
 *
 * @param[in]  pLimit PWR_POLICY_LIMIT pointer.
 *
 * @return FLCN_OK
 *     PWR_POLICY structure succesfully evaluated.
 *
 * @return Other errors
 *     Unexpected errors propagated up from type-specific implemenations.
 */
#define PwrPolicyLimitEvaluate(fname) FLCN_STATUS (fname) (PWR_POLICY_LIMIT *pLimit)

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * PWR_POLICY_LIMIT interfaces
 */
BoardObjGrpIfaceModel10ObjSet        (pwrPolicyGrpIfaceModel10ObjSetImpl_LIMIT)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrPolicyGrpIfaceModel10ObjSetImpl_LIMIT");
PwrPolicyLimitEvaluate   (pwrPolicyLimitEvaluate)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyLimitEvaluate");

/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */
#include "pmgr/pwrpolicy_proplimit.h"
#include "pmgr/pwrpolicy_totalgpu_iface.h"
#include "pmgr/pwrpolicy_totalgpu.h"
#include "pmgr/pwrpolicy_hwthreshold.h"

#endif // PWRPOLICY_LIMIT_H
