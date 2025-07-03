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
 * @file   pwrpolicy_limit.c
 * @brief  Interface specification for a PWR_POLICY_LIMIT.
 *
 * The Limit class is a virtual class which provides a generic mechanism to
 * control power/current by updating the limit values of other Power Policy
 * objects.  This module does not include any knowledge/policy for how to adjust
 * those limits.  That functionality will be in classes which extend/implement
 * this call.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objperf.h"
#include "pmu_objpmgr.h"
#include "pmgr/pwrpolicy_limit.h"
#include "dbgprintf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrPolicyGrpIfaceModel10ObjSetImpl_LIMIT
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    return pwrPolicyGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
}

/*!
 * @copydoc PwrPolicyLimitEvaluate
 */
FLCN_STATUS
pwrPolicyLimitEvaluate
(
    PWR_POLICY_LIMIT *pLimit
)
{
    FLCN_STATUS status = FLCN_OK;

    // First evaluate the Super policy
    status = pwrPolicyEvaluate(&pLimit->super);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Call the class-specific logic.
    switch (BOARDOBJ_GET_TYPE(pLimit))
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_PROP_LIMIT:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_PROP_LIMIT);
            return pwrPolicyLimitEvaluate_PROP_LIMIT(pLimit);
            break;

        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_TOTAL_GPU:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_TOTAL_GPU);
            return pwrPolicyLimitEvaluate_TOTAL_GPU(pLimit);
            break;
    }

    return FLCN_ERR_NOT_SUPPORTED;
}

/* ------------------------- Private Functions ------------------------------ */
