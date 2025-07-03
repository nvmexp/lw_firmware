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
 * @file    pwrpolicyrel_weight.h
 * @copydoc pwrpolicyrel_weight.c
 */

#ifndef PWRPOLICYREL_WEIGHT_H
#define PWRPOLICYREL_WEIGHT_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrpolicyrel.h"

/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_POLICY_RELATIONSHIP_WEIGHT PWR_POLICY_RELATIONSHIP_WEIGHT;

struct PWR_POLICY_RELATIONSHIP_WEIGHT
{
    /*!
     * @copydoc PWR_POLICY_RELATIONSHIP
     */
    PWR_POLICY_RELATIONSHIP super;

    /*!
     * Weight to apply to limits and values for this PWR_POLICY_RELATIONSHIP.
     */
    LwUFXP4_12              weight;
};

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * PWR_POLICY_RELATIONSHIP interfaces
 */
BoardObjGrpIfaceModel10ObjSet                  (pwrPolicyRelationshipGrpIfaceModel10ObjSetImpl_WEIGHT)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrPolicyRelationshipGrpIfaceModel10ObjSetImpl_WEIGHT");
PwrPolicyRelationshipValueLwrrGet  (pwrPolicyRelationshipValueLwrrGet_WEIGHT)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyRelationshipValueLwrrGet_WEIGHT");
PwrPolicyRelationshipLimitLwrrGet  (pwrPolicyRelationshipLimitLwrrGet_WEIGHT)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyRelationshipLimitLwrrGet_WEIGHT");
PwrPolicyRelationshipLimitInputGet (pwrPolicyRelationshipLimitInputGet_WEIGHT)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyRelationshipLimitInputGet_WEIGHT");
PwrPolicyRelationshipLimitInputSet (pwrPolicyRelationshipLimitInputSet_WEIGHT)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyRelationshipLimitInputSet_WEIGHT");

/* ------------------------- Debug Macros ---------------------------------- */

#endif // PWRPOLICYREL_WEIGHT_H
