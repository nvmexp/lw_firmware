/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrpolicy_march_vf.h
 * @brief @copydoc pwrpolicy_march_vf.c
 */

#ifndef PWRPOLICY_MARCH_VF_H
#define PWRPOLICY_MARCH_VF_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrpolicy_domgrp.h"
#include "pmgr/pwrpolicy_march.h"

/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_POLICY_MARCH_VF PWR_POLICY_MARCH_VF;

/*!
 * Structure representing an VF-based marching power policy.
 */
struct PWR_POLICY_MARCH_VF
{
    /*!
     * @copydoc PWR_POLICY_DOMGRP
     */
    PWR_POLICY_DOMGRP super;

    /*!
     * @copydoc PWR_POLICY_MARCH
     */
    PWR_POLICY_MARCH  march;
};

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * PWR_POLICY interfaces
 */
BoardObjGrpIfaceModel10ObjSet       (pwrPolicyGrpIfaceModel10ObjSetImpl_MARCH_VF)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrPolicyGrpIfaceModel10ObjSetImpl_MARCH_VF");
BoardObjIfaceModel10GetStatus           (pwrPolicyIfaceModel10GetStatus_MARCH_VF)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrPolicyIfaceModel10GetStatus_MARCH_VF");

/*!
 * PWR_POLICY_DOMGRP interfaces
 */
PwrPolicyDomGrpEvaluate (pwrPolicyDomGrpEvaluate_MARCH_VF)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyDomGrpEvaluate_MARCH_VF");

/* ------------------------- Debug Macros ---------------------------------- */

#endif // PWRPOLICY_MARCH_VF_H
