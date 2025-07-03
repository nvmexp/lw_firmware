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
 * @file   thrmpolicy_dtc.c
 * @brief  Interface specification for a THERM_POLICY_DTC.
 *
 * THERM_POLICY_DTC is an interface class; it is not a full THERM_POLICY
 * implementation. Other THERM_POLICY classes will implement/extend this
 * interface to utilize the basic Distance-To-Critical (DTC) behavior and
 * figure out how to actually implement limit behavior with policy decisions.
 *
 * The DTC algorithm functions based on threshold ranges. It determines which
 * region the current threshold lies within and specifies whether to go up or
 * down on an implementation scale.
 *
 * Algorithm:
 *  if (lwrrVal > limitCritical)
 *      stepDown(L)
 *  else if (lwrrVal > limitAggressive)
 *      stepDown(Nd)
 *  else if (lwrrVal > limitModerate)
 *      stepDown(1)
 *  else if (lwrrVal < limitRelease)
 *      stepUp(Nu)
 *  else if (lwrrVal < limitDisengage)
 *      stepUp(L)
 *
 * Where,
 *  L  = Number of levels on the scale.
 *  Nd = Number of levels to step down.
 *  Nu = Number of levels to step up.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmgr.h"
#include "therm/objtherm.h"
#include "therm/thrmpolicy_dtc.h"
#include "pmu_objperf.h"

/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions  ------------------------------ */
/*!
 * Constructor for the THERM_POLICY_DOMGRP class.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
thermPolicyGrpIfaceModel10ObjSetImpl_DTC
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_THERM_THERM_POLICY_DTC_BOARDOBJ_SET *pDtcSet =
        (RM_PMU_THERM_THERM_POLICY_DTC_BOARDOBJ_SET *)pBoardObjDesc;
    THERM_POLICY_DTC                           *pDtc     =
        (THERM_POLICY_DTC *)*ppBoardObj;

    // Set class-specific data.
    pDtc->aggressiveStep      = pDtcSet->aggressiveStep;
    pDtc->releaseStep         = pDtcSet->releaseStep;
    pDtc->sampleCount         = 0;
    pDtc->holdSampleThreshold = pDtcSet->holdSampleThreshold;
    pDtc->stepSampleThreshold = pDtcSet->stepSampleThreshold;

    pDtc->thresholds.critical   = pDtcSet->thresholds.critical;
    pDtc->thresholds.aggressive = pDtcSet->thresholds.aggressive;
    pDtc->thresholds.moderate   = pDtcSet->thresholds.moderate;
    pDtc->thresholds.release    = pDtcSet->thresholds.release;
    pDtc->thresholds.disengage  = pDtcSet->thresholds.disengage;

    return FLCN_OK;
}

/*!
 * @copydoc thermPolicyDtcGetLevelChange
 */
LwS32
thermPolicyDtcGetLevelChange
(
    THERM_POLICY_DTC *pDtc,
    LwTemp            temperature
)
{
    // TODO-JBH: Add all behavior exceptions.
    LwS32 levelChange = 0;

    if (temperature > pDtc->thresholds.critical)
    {
        levelChange = LW_S32_MIN;
    }
    else if (temperature > pDtc->thresholds.aggressive)
    {
        levelChange = (LwS32)(-pDtc->aggressiveStep);
    }
    else if (temperature > pDtc->thresholds.moderate)
    {
        levelChange = -1;
    }
    else if (temperature < pDtc->thresholds.disengage)
    {
        levelChange = THERM_POLICY_DTC_LIMIT_CHANGE_IF_DISENGAGED;
    }
    else if (temperature < pDtc->thresholds.release)
    {
        levelChange = (LwS32)(pDtc->releaseStep);
    }

    return levelChange;
}

/*!
 * @copydoc thermPolicyDtcQuery
 */
FLCN_STATUS
thermPolicyDtcQuery
(
    THERM_POLICY_DTC              *pDtc,
    RM_PMU_THERM_POLICY_QUERY_DTC *pPayload
)
{
    pPayload->sampleCount = pDtc->sampleCount;

    return FLCN_OK;
}
