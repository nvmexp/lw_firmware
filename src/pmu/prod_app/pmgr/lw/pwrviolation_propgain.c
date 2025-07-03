/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pwrviolation_propgain.c
 * @brief   Interface specification for a PWR_VIOLATION_PROPGAIN.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "lib/lib_fxp.h"
#include "pmu_objpmgr.h"
#include "pmgr/pwrviolation_propgain.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Function Prototypes -------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Construct a _PROPGAIN PWR_VIOLATION object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrViolationGrpIfaceModel10ObjSetImpl_PROPGAIN
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PMGR_PWR_VIOLATION_PROPGAIN_BOARDOBJ_SET *pPGainSet;
    PWR_VIOLATION_PROPGAIN                          *pPGain;
    FLCN_STATUS                                      status;

    // Construct and initialize parent-class object.
    status = pwrViolationGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        return status;
    }
    pPGain    = (PWR_VIOLATION_PROPGAIN *)*ppBoardObj;
    pPGainSet = (RM_PMU_PMGR_PWR_VIOLATION_PROPGAIN_BOARDOBJ_SET *)pBoardObjDesc;

    // Set class-specific data.
    pPGain->propGain          = pPGainSet->propGain;
    pPGain->policyRelIdxFirst = pPGainSet->policyRelIdxFirst;
    pPGain->policyRelIdxLast  = pPGainSet->policyRelIdxLast;
    pPGain->pwrLimitAdj       = 0;

    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
pwrViolationIfaceModel10GetStatus_PROPGAIN
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PMGR_PWR_VIOLATION_PROPGAIN_BOARDOBJ_GET_STATUS *pGetStatus;
    PWR_VIOLATION_PROPGAIN *pPGain;
    FLCN_STATUS             status;

    status = pwrViolationIfaceModel10GetStatus_SUPER(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        return status;
    }
    pPGain     = (PWR_VIOLATION_PROPGAIN *)pBoardObj;
    pGetStatus = (RM_PMU_PMGR_PWR_VIOLATION_PROPGAIN_BOARDOBJ_GET_STATUS *)pPayload;

    // copyout VIOLATION class specific param
    pGetStatus->pwrLimitAdj = pPGain->pwrLimitAdj;

    return status;
}

/*!
 * @copydoc PwrViolationEvaluate
 */
FLCN_STATUS
pwrViolationEvaluate_PROPGAIN
(
    PWR_VIOLATION  *pViolation
)
{
    PWR_VIOLATION_PROPGAIN *pPGain = (PWR_VIOLATION_PROPGAIN *)pViolation;
    LwSFXP20_12             violDelta;
    FLCN_STATUS             status;
    LwS32                   pwrLimitAdj;
    LwU8                    idx;

    // Call super-class to evaluate and cache current violation rate.
    status = pwrViolationEvaluate_SUPER(pViolation);
    if (status != FLCN_OK)
    {
        goto pwrViolationEvaluate_PROPGAIN_exit;
    }

    //
    // If the current violation is less then the expected target value, we will
    // increase the power policy limit. On the other hand, if the current violation
    // is greater then the expected target value, we will reduce the power policy
    // limit.
    //
    violDelta = (LwSFXP20_12)pViolation->violation.violTarget -
                (LwSFXP20_12)pViolation->violation.violLwrrent;

    //
    // Compute limit adjustment by scaling gain and normalizing units.
    //
    // Numerical Analysis:
    //
    // violDelta: FXP20.12
    // pPGain->propGain: FXP20.12
    //
    // (violDelta * pPGain->propGain) is handled by multSFXP32()
    // which does intermediate math safely in 64-bit as follows:-
    //
    // 20.12 * 20.12 => 40.24
    // 40.24 >> 24 => 40.0
    // LWU64_LO(40.0) => 32.0 (extracts the least significant 32 bits after
    //                         checking for overflow)
    //
    // Thus, overflow will happen here if
    // (violDelta * pPGain->propGain) > 2^32W ~ 4.3MW
    //
    pwrLimitAdj = multSFXP32(24, violDelta, pPGain->propGain);
    pPGain->pwrLimitAdj = pwrLimitAdj;

    // Apply delta to selected PWR policy relationships.
    for (idx = pPGain->policyRelIdxFirst; idx <= pPGain->policyRelIdxLast; idx++)
    {
        PWR_POLICY_RELATIONSHIP *pPolicyRel = PWR_POLICY_REL_GET(idx);
        LwS32   newLimit;

        if (pPolicyRel == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto pwrViolationEvaluate_PROPGAIN_exit;
        }

        if (pwrLimitAdj > 0)
        {
            // Update the last requested pwr policy limit while uncapping
            newLimit  = (LwS32)pwrPolicyRelationshipLimitInputGet(pPolicyRel,
            LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_INPUT_CLIENT_IDX_PWR_VIOLATION(
                BOARDOBJ_GET_GRP_IDX_8BIT(&pViolation->super)));
        }
        else
        {
            // Update the observed value pwr policy while capping
            newLimit  = (LwS32)pwrPolicyRelationshipValueLwrrGet(pPolicyRel);
        }

        newLimit += pwrLimitAdj;
        newLimit  = LW_MAX(newLimit, 0);
        newLimit  = LW_MIN(newLimit, PWR_VIOLATION_PROPGAIN_LIMIT_INPUT_MAX);

        status = pwrPolicyRelationshipLimitInputSet(pPolicyRel,
            LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_INPUT_CLIENT_IDX_PWR_VIOLATION(
                BOARDOBJ_GET_GRP_IDX_8BIT(&pViolation->super)), LW_FALSE, (LwU32 *)&newLimit);
        if (status != FLCN_OK)
        {
            PMU_HALT();
            goto pwrViolationEvaluate_PROPGAIN_exit;
        }
    }

pwrViolationEvaluate_PROPGAIN_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
