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
 * @file  pwrchrel_balanced_phase_est.c
 * @brief Interface specification for a PWR_CHRELATIONSHIP_BALANCED_PHASE_EST.
 *
 * The Balanced Phase Estimate Power Channel Relationship Class computes power
 * as proportion/ratio of another channel's power, where the ratio is a dynamic
 * factor depending on how certain phases are being balanced to/from the given
 * referenced channel.
 *
 * This Channel Relationship is intended to be used to compute the total input
 * power of a voltage regulator of which a _BALANCE Power Policy and set of
 * _BALANCE Power Policy Relationships are balancing one or more phases.  It
 * should reference a Channel which is sourcing one or more static phases of the
 * input and one or more dynamic phases.
 *
 * Given the following values:
 *     channelPwrmW - Observed power on the Power Channel corresponding to @ref
 *          PWR_CHRELATIONSHIP::chIdx.
 *     numTotalPhases - Total number of voltage regulator phases, by which to
 *         scale up the callwlated value.
 *     numStaticPhases - Number of voltage regualtor phases which are statically
 *         sourced by the referenced Power Channel.
 *     balancedPhasePolicyRelIdxFirst - Index of first _BALANCE Power Policy
 *         Relationship associated with the referenced Power Channel.
 *     numBalancedPhases - Number of _BALANCE Power Policy Relationships
 *         associated with the referenced Power Channel.
 *     pwmPct(i) - Percentage of the dynamically balanced phase i which is
 *         drawing from the referenced Power Channel.
 *
 * Power for the Blanaced Phase Estimate Power Channel Relationship can be
 * callwlated as follows:
 *
 *     pwrmW = channelPwrmW * numTotalPhases /
 *         (numStaticPhases +
 *             sum(i=balancedPhasePolicyRelIdxFirst..balancedPhasePolicyRelIdxFirst+numBalancedPhases,
 *                  pwmPct(i)))
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objpmgr.h"
#include "pmgr/pwrchrel_balanced_phase_est.h"
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
 * Construct a _BALANCED_PHASE_EST PWR_CHRELATIONSHIP object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrChRelationshipGrpIfaceModel10ObjSet_BALANCED_PHASE_EST
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PMGR_PWR_CHRELATIONSHIP_BALANCED_PHASE_EST_BOARDOBJ_SET *pDescBalanced =
        (RM_PMU_PMGR_PWR_CHRELATIONSHIP_BALANCED_PHASE_EST_BOARDOBJ_SET *)pBoardObjDesc;
    PWR_CHRELATIONSHIP_BALANCED_PHASE_EST             *pBalanced;
    FLCN_STATUS                                        status;

    status = pwrChRelationshipGrpIfaceModel10ObjSet_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        return status;
    }
    pBalanced = (PWR_CHRELATIONSHIP_BALANCED_PHASE_EST *)*ppBoardObj;

    // Copy in the _BALANCED_PHASE_EST type-specific data
    pBalanced->numTotalPhases                 = pDescBalanced->numTotalPhases;
    pBalanced->numStaticPhases                = pDescBalanced->numStaticPhases;
    pBalanced->balancedPhasePolicyRelIdxFirst =
        pDescBalanced->balancedPhasePolicyRelIdxFirst;
    pBalanced->numBalancedPhases              = pDescBalanced->numBalancedPhases;

    return status;
}

/*!
 * _BALANCED_PHASE_EST power channel relationship implementation.
 *
 * @copydoc PwrChRelationshipWeightGet
 */
LwSFXP20_12
pwrChRelationshipWeightGet_BALANCED_PHASE_EST
(
    PWR_CHRELATIONSHIP *pChRel,
    LwU8                chIdxEval
)
{
    PWR_CHRELATIONSHIP_BALANCED_PHASE_EST *pBalanced =
        (PWR_CHRELATIONSHIP_BALANCED_PHASE_EST *)pChRel;
    LwUFXP20_12 weight;
    LwUFXP20_12 numPhases;
    LwU8        i;

    //
    // Compute the number of phases which are lwrrently sourcing from the given channel:
    //     numPhases = numStaticPhases + pwmRatioOfBalancedPhase(i)
    //
    // This does initial summation in 16.16, as the PWM pct values are in 16.16.
    // However, the eventual weight value needs to be in 20.12, so shift down
    // after sum is computed.
    //
    numPhases = LW_TYPES_U32_TO_UFXP_X_Y(16, 16, pBalanced->numStaticPhases);
    for (i = 0; i < pBalanced->numBalancedPhases; i++)
    {
        PWR_POLICY_RELATIONSHIP_BALANCE *pRelationship;

        pRelationship = (PWR_POLICY_RELATIONSHIP_BALANCE *)
            PWR_POLICY_REL_GET(pBalanced->balancedPhasePolicyRelIdxFirst + i);
        if (pRelationship == NULL)
        {
            PMU_BREAKPOINT();
            return 0;
        }

        numPhases += pwrPolicyRelationshipBalancePwmPctGetIdx(pRelationship,
                    pBalanced->super.chIdx,
                    chIdxEval, LW_TRUE);
    }
    // Shift down 16.16 -> 20.12
    numPhases  = LW_RIGHT_SHIFT_ROUNDED(numPhases, 4);

    //
    // Compute weight = totalNumPhases / numPhases
    //
    // Mathematical Analysis:  8.24 / 20.12 => 20.12
    //
    weight = LW_TYPES_U32_TO_UFXP_X_Y(8, 24, pBalanced->numTotalPhases);
    weight = LW_UNSIGNED_ROUNDED_DIV(weight, numPhases);

    return (LwSFXP20_12)weight;
}

/* ------------------------- Private Functions ------------------------------ */
