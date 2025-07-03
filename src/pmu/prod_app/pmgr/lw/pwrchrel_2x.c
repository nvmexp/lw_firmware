/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrchrel_2x.c
 * @brief PMGR Power Channel Relationship Specific to Power Topology
 *        Relationship Table 2.0.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmgr.h"
#include "pmgr/pwrchrel_2x.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc PwrChRelationshipTupleEvaluate
 */
FLCN_STATUS
pwrChRelationshipTupleEvaluate
(
    PWR_CHRELATIONSHIP              *pChRel,
    LwU8                             chIdxEval,
    PWR_CHRELATIONSHIP_TUPLE_STATUS *pStatus
)
{
    switch (pChRel->super.type)
    {
        case LW2080_CTRL_PMGR_PWR_CHRELATIONSHIP_TYPE_WEIGHT:
        case LW2080_CTRL_PMGR_PWR_CHRELATIONSHIP_TYPE_BALANCED_PHASE_EST:
        case LW2080_CTRL_PMGR_PWR_CHRELATIONSHIP_TYPE_BALANCING_PWM_WEIGHT:
            return pwrChRelationshipTupleEvaluate_WEIGHTED(pChRel, chIdxEval, pStatus);
        case LW2080_CTRL_PMGR_PWR_CHRELATIONSHIP_TYPE_REGULATOR_LOSS_EST:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_MONITOR_CHANNEL_RELATIONSHIP_REGULATOR_LOSS_EST);
            return pwrChRelationshipTupleEvaluate_REGULATOR_LOSS_EST(pChRel, chIdxEval, pStatus);
        case LW2080_CTRL_PMGR_PWR_CHRELATIONSHIP_TYPE_REGULATOR_EFF_EST_V1:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_MONITOR_CHANNEL_RELATIONSHIP_REGULATOR_EFF_EST_V1);
            return pwrChRelationshipTupleEvaluate_REGULATOR_EFF_EST_V1(pChRel, chIdxEval, pStatus);
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
pwrChRelationshipIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    // Call type-specific status functions.
    switch (BOARDOBJ_GET_TYPE(pBoardObj))
    {
        case LW2080_CTRL_PMGR_PWR_CHRELATIONSHIP_TYPE_WEIGHT:
        case LW2080_CTRL_PMGR_PWR_CHRELATIONSHIP_TYPE_BALANCED_PHASE_EST:
        case LW2080_CTRL_PMGR_PWR_CHRELATIONSHIP_TYPE_BALANCING_PWM_WEIGHT:
        case LW2080_CTRL_PMGR_PWR_CHRELATIONSHIP_TYPE_REGULATOR_LOSS_EST:
        case LW2080_CTRL_PMGR_PWR_CHRELATIONSHIP_TYPE_REGULATOR_EFF_EST_V1:
            return FLCN_OK;
    }

    // Otherwise, fall back to _SUPER class.
    return pwrChRelationshipIfaceModel10GetStatus_SUPER(pModel10, pPayload);
}

/* ------------------------- Private Functions ------------------------------ */
