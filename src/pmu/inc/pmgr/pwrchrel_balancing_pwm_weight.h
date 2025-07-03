/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrchrel_balancing_pwm_weight.h
 * @brief @copydoc pwrchrel_balancing_pwm_weight.c
 */

#ifndef PWRCHREL_BALANCING_PWM_WEIGHT_H
#define PWRCHREL_BALANCING_PWM_WEIGHT_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrchrel.h"

/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_CHRELATIONSHIP_BALANCING_PWM_WEIGHT PWR_CHRELATIONSHIP_BALANCING_PWM_WEIGHT;

/*!
 * PWR_CHRELATIONSHIP class which evaluates as a weight of the specified
 * PWR_CHANNEL, where the weight is the PWM percentage of a
 * PWR_POLICY_RELATIONSHIP_BALANCE object.
 */
struct PWR_CHRELATIONSHIP_BALANCING_PWM_WEIGHT
{
    /*!
     * @copydoc PWR_CHRELATIONSHIP
     */
    PWR_CHRELATIONSHIP super;

    /*!
     * Index to PWR_POLICY_RELATIONSHIP object of type
     * PWR_POLICY_RELATIONSHIP_BALANCE which will be queried for its current PWM
     * percentage.
     */
    LwU8   balancingRelIdx;

    /*!
     * Boolean indicating whether to use the primary/normal or
     * secondary/ilwerted PWM percentage as the weight.
     */
    LwBool bPrimary;
};

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * PWR_CHRELATIONSHIP interfaces
 */
BoardObjGrpIfaceModel10ObjSet           (pwrChRelationshipGrpIfaceModel10ObjSet_BALANCING_PWM_WEIGHT)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrChRelationshipGrpIfaceModel10ObjSet_BALANCING_PWM_WEIGHT");
PwrChRelationshipWeightGet  (pwrChRelationshipWeightGet_BALANCING_PWM_WEIGHT)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrChRelationshipWeightGet_BALANCING_PWM_WEIGHT");

/* ------------------------- Debug Macros ---------------------------------- */

#endif // PWRCHREL_BALANCING_PWM_WEIGHT_H
