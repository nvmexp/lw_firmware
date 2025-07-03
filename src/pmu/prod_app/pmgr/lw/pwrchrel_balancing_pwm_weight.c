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
 * @file  pwrchrel_balancing_pwm_weight.c
 * @brief Interface specification for a PWR_CHRELATIONSHIP_BALANCING_PWM_WEIGHT.
 *
 * The Balancing PWM Weight Power Channel Relationship Class computes power as a
 * proportion/weight of another channel's power, where the weight is the PWM
 * percentage associated with a Balancing Power Policy Relationship object.
 *
 * Given the following values:
 *     channelPwrmW - Observed power on the Power Channel corresponding to @ref
 *          PWR_CHRELATIONSHIP::chIdx.
 *     pwm - Current PWM percentage in range [0.0, 1.0] corresponding to the
 *          referenced Balancing Power Policy Relationship.
 *     bPrimary - Boolean value indicating whether the primary/normal or
 *          secondary/ilwerted PWM value should be used.
 *
 * Power for the Balancing PWM Weight Power Channel Relationship can be
 * callwlated as follows:
 *     pwrmW = channelPwrmW * (bPrimary ? pwm : 1 - pwm)
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objpmgr.h"
#include "pmgr/pwrchrel_balancing_pwm_weight.h"
#include "pmgr/pwrpolicyrel_balance.h"
#include "dbgprintf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Construct a _BALANCING_PWM_WEIGHT PWR_CHRELATIONSHIP object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrChRelationshipGrpIfaceModel10ObjSet_BALANCING_PWM_WEIGHT
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PMGR_PWR_CHRELATIONSHIP_BALANCING_PWM_WEIGHT_BOARDOBJ_SET *pDescBPWeight =
        (RM_PMU_PMGR_PWR_CHRELATIONSHIP_BALANCING_PWM_WEIGHT_BOARDOBJ_SET *)pBoardObjDesc;
    PWR_CHRELATIONSHIP_BALANCING_PWM_WEIGHT             *pBPWeight;
    FLCN_STATUS                                          status;

    status = pwrChRelationshipGrpIfaceModel10ObjSet_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        return status;
    }
    pBPWeight = (PWR_CHRELATIONSHIP_BALANCING_PWM_WEIGHT *)*ppBoardObj;

    // Copy in the _BALANCING_PWM_WEIGHT type-specific data
    pBPWeight->balancingRelIdx = pDescBPWeight->balancingRelIdx;
    pBPWeight->bPrimary        = pDescBPWeight->bPrimary;

    return status;
}

/*!
 * _BALANCING_PWM_WEIGHT power channel relationship implementation.
 *
 * @copydoc PwrChRelationshipWeightGet
 */
LwSFXP20_12
pwrChRelationshipWeightGet_BALANCING_PWM_WEIGHT
(
    PWR_CHRELATIONSHIP *pChRel,
    LwU8                chIdxEval
)
{
    PWR_CHRELATIONSHIP_BALANCING_PWM_WEIGHT *pBPWeight =
        (PWR_CHRELATIONSHIP_BALANCING_PWM_WEIGHT *)pChRel;
    PWR_POLICY_RELATIONSHIP_BALANCE *pBalance;
    LwUFXP20_12 weight;

    //
    // Query the PWM percentage from the PWR_POLICY_RELATIONSHIP_BALANCE object.
    // This PWM pct value is FXP 16.16, so need to right shift by 4 into FXP
    // 20.12.
    //

    pBalance = (PWR_POLICY_RELATIONSHIP_BALANCE *)
        PWR_POLICY_REL_GET(pBPWeight->balancingRelIdx);
    if (pBalance == NULL)
    {
        PMU_BREAKPOINT();
        return 0;
    }

    weight = pwrPolicyRelationshipBalancePwmPctGet(pBalance,
                    pBPWeight->bPrimary, LW_FALSE);
    weight = LW_RIGHT_SHIFT_ROUNDED(weight, 4);

    return (LwSFXP20_12)weight;
}

/* ------------------------- Private Functions ------------------------------ */
