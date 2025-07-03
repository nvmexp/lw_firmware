/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pwrchrel.c
 * @brief  Interface specification for a PWR_CHRELATIONSHIP.
 *
 * Power Channel Relationships are objects to model how other power channels are
 * used to callwlate the power of a given power channel.  Channels who depend on
 * other channels for their power callwlation will index to Power Channel
 * Relationships to describe how to use those other channels for callwlation.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objpmgr.h"
#include "pmgr/pwrchrel.h"
#include "dbgprintf.h"
#include "lib/lib_fxp.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
FLCN_STATUS
pmgrPwrChRelIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    switch (pBuf->type)
    {
        case LW2080_CTRL_PMGR_PWR_CHRELATIONSHIP_TYPE_WEIGHT:
            return pwrChRelationshipGrpIfaceModel10ObjSet_WEIGHT(pModel10, ppBoardObj,
                sizeof(PWR_CHRELATIONSHIP_WEIGHT), pBuf);

        case LW2080_CTRL_PMGR_PWR_CHRELATIONSHIP_TYPE_BALANCED_PHASE_EST:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_MONITOR_CHANNEL_RELATIONSHIP_BALANCED_PHASE_EST);
            return pwrChRelationshipGrpIfaceModel10ObjSet_BALANCED_PHASE_EST(pModel10, ppBoardObj,
                sizeof(PWR_CHRELATIONSHIP_BALANCED_PHASE_EST), pBuf);

        case LW2080_CTRL_PMGR_PWR_CHRELATIONSHIP_TYPE_BALANCING_PWM_WEIGHT:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_MONITOR_CHANNEL_RELATIONSHIP_BALANCING_PWM_WEIGHT);
            return pwrChRelationshipGrpIfaceModel10ObjSet_BALANCING_PWM_WEIGHT(pModel10, ppBoardObj,
                sizeof(PWR_CHRELATIONSHIP_BALANCING_PWM_WEIGHT), pBuf);

        case LW2080_CTRL_PMGR_PWR_CHRELATIONSHIP_TYPE_REGULATOR_LOSS_EST:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_MONITOR_CHANNEL_RELATIONSHIP_REGULATOR_LOSS_EST);
            return pwrChRelationshipGrpIfaceModel10ObjSet_REGULATOR_LOSS_EST(pModel10, ppBoardObj,
                sizeof(PWR_CHRELATIONSHIP_REGULATOR_LOSS_EST), pBuf);

        case LW2080_CTRL_PMGR_PWR_CHRELATIONSHIP_TYPE_REGULATOR_EFF_EST_V1:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_MONITOR_CHANNEL_RELATIONSHIP_REGULATOR_EFF_EST_V1);
            return pwrChRelationshipGrpIfaceModel10ObjSet_REGULATOR_EFF_EST_V1(pModel10, ppBoardObj,
                sizeof(PWR_CHRELATIONSHIP_REGULATOR_EFF_EST_V1), pBuf);
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @copydoc PwrChRelationshipEvaluate
 */
LwS32
pwrChRelationshipEvaluatemW
(
    PWR_CHRELATIONSHIP *pChRel,
    LwU8                chIdxEval
)
{
    LwU32       powermW = 0;
    LwSFXP20_12 weight;

    // Query the power for the channel.
    (void)pwrMonitorChannelStatusGet(Pmgr.pPwrMonitor, pChRel->chIdx, &powermW);

    // Obtain weight associated with this relationship.
    weight = pwrChRelationshipWeightGet(pChRel, chIdxEval);

    //
    // Relationship's power callwlation = power from channel * weight.
    //
    // From FXP20.12 point of view, this is safe since the multSFXP32 API
    // is designed to handle intermediate overflow. However, 32-bit overflow
    // is still possible (at ~4.29MW).
    //
    return multSFXP32(12, (LwS32)powermW, weight);
}

/*!
 * @copydoc PwrChRelationshipWeightGet
 */
LwSFXP20_12
pwrChRelationshipWeightGet
(
    PWR_CHRELATIONSHIP *pChRel,
    LwU8                chIdxEval
)
{
    switch (pChRel->super.type)
    {
        case LW2080_CTRL_PMGR_PWR_CHRELATIONSHIP_TYPE_WEIGHT:
            return pwrChRelationshipWeightGet_WEIGHT(pChRel, chIdxEval);

        case LW2080_CTRL_PMGR_PWR_CHRELATIONSHIP_TYPE_BALANCED_PHASE_EST:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_MONITOR_CHANNEL_RELATIONSHIP_BALANCED_PHASE_EST);
            return pwrChRelationshipWeightGet_BALANCED_PHASE_EST(pChRel, chIdxEval);

        case LW2080_CTRL_PMGR_PWR_CHRELATIONSHIP_TYPE_BALANCING_PWM_WEIGHT:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_MONITOR_CHANNEL_RELATIONSHIP_BALANCING_PWM_WEIGHT);
            return pwrChRelationshipWeightGet_BALANCING_PWM_WEIGHT(pChRel, chIdxEval);
    }

    PMU_BREAKPOINT();
    return LW_TYPES_S32_TO_SFXP_X_Y(20, 12, 1);
}

/*!
 * Constructor for the PWR_CHRELATIONSHIP super-class.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrChRelationshipGrpIfaceModel10ObjSet_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PMGR_PWR_CHRELATIONSHIP_BOARDOBJ_SET *pDescTmp  =
        (RM_PMU_PMGR_PWR_CHRELATIONSHIP_BOARDOBJ_SET *)pBoardObjDesc;
    PWR_CHRELATIONSHIP             *pChRel;
    FLCN_STATUS                     status;

    // Make sure the specified power channel (PWR_CHANNEL) is available.
    if ((Pmgr.pwr.channels.objMask.super.pData[0] & BIT(pDescTmp->chIdx)) == 0)
    {
        return FLCN_ERR_NOT_SUPPORTED;
    }

    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        return status;
    }
    pChRel = (PWR_CHRELATIONSHIP *)*ppBoardObj;

    pChRel->chIdx = pDescTmp->chIdx;

    return status;
}

/* ------------------------- Private Functions ------------------------------ */
