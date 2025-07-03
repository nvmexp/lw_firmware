/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pwrpolicy_march.c
 * @brief  Interface specification for a PWR_POLICY_MARCH.
 *
 * PWR_POLICY_MARCH is an interface class, it is not a full PWR_POLICY
 * implementation.  Other PWR_POLICY classes will implement/extend this
 * interface to utilize the basic marching behavior (i.e. getting cap/uncap
 * actions) and figure out how to actually implement marching behavior with
 * policy decisions (e.g. dropping NDIV coeffs or VF points).
 *
 * Marcher Algorithm Theory:
 *
 * The MARCH interface is a controller object which regulates to a control value
 * via a simple cap/uncap marching algorithm with an associated hysteresis
 * value.
 *
 * The actual algorithm works as follows:
 *
 * if (valueLwrr > limitLwrr)
 *     cap()
 * else if (valueLwrr <= hysteresis(limitLwrr))
 *     uncap()
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "pmu_objperf.h"
#include "pmu_objpmgr.h"
#include "therm/thrmintr.h"
#include "pmgr/pwrpolicy_march.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Construct a PWR_POLICY_MARCH object.
 *
 * @copydoc PwrPolicyMarchConstruct
 */
FLCN_STATUS
pwrPolicyMarchConstruct
(
    PWR_POLICY_MARCH             *pMarch,
    RM_PMU_PMGR_PWR_POLICY_MARCH *pMarchDesc,
    LwBool                        bFirstConstruct
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    // Call class-specific implementations.
    switch (pMarchDesc->type)
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_MARCH:
            status = pwrPolicyMarchConstruct_SUPER(pMarch, pMarchDesc, bFirstConstruct);
            break;
    }

    return status;
}

/*!
 * Construct a PWR_POLICY_MARCH super-class object.
 *
 * @copydoc PwrPolicyMarchConstruct
 */
FLCN_STATUS
pwrPolicyMarchConstruct_SUPER
(
    PWR_POLICY_MARCH             *pMarch,
    RM_PMU_PMGR_PWR_POLICY_MARCH *pMarchDesc,
    LwBool                        bFirstConstruct
)
{
    // Set the MARCH-specific parameters.
    pMarch->type       = pMarchDesc->type;
    pMarch->stepSize   = pMarchDesc->stepSize;
    pMarch->hysteresis = pMarchDesc->hysteresis;

    // Initialize to default state.
    pMarch->action = LW2080_CTRL_PMGR_PWR_POLICY_MARCH_ACTION_NONE;

    //
    // If first construction, init HW_FAILSAFE violation timer to the current
    // reading.  All subsequent updates will be handled by @ref
    // pwrPolicyMarchEvaluateAction().
    //
    if (bFirstConstruct)
    {
        if (thermEventViolationGetTimer32(RM_PMU_THERM_EVENT_META_ANY_HW_FS,
                                    &pMarch->thermHwFsTimernsPrev) !=  FLCN_OK)
        {
            PMU_HALT();
            return FLCN_ERR_ILWALID_STATE;
        }
    }

    return FLCN_OK;
}

/*!
 * @copydoc PwrPolicyMarchQuery
 */
FLCN_STATUS
pwrPolicyMarchQuery
(
    PWR_POLICY_MARCH                                 *pMarch,
    RM_PMU_PMGR_PWR_POLICY_MARCH_BOARDOBJ_GET_STATUS *pGetStatus
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (pMarch->type)
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_MARCH:
            status = FLCN_OK;
            break;
    }

    // Copy out type-specific state to the RM.
    pGetStatus->type       = pMarch->type;
    pGetStatus->action     = pMarch->action;
    pGetStatus->uncapLimit = pMarch->uncapLimit;

    return status;
}

/*!
 * @copydoc PwrPolicyMarchEvaluateAction
 */
LwU8
pwrPolicyMarchEvaluateAction
(
    PWR_POLICY       *pPolicy,
    PWR_POLICY_MARCH *pMarch
)
{
    LwU32 limitLwrr;
    LwU32 thermHwFsTimerns;

    //
    // If current voltage is zero, then assume haven't gotten PERF_STATUS,
    // and all PERF interfaces will be slightly broken.  Bail here.
    //
    if (perfVoltageuVGet() == 0)
    {
        return pMarch->action;
    }

    //
    // Query the THERM HW_FAILSAFE violation time.  This counter tracks the
    // amount of time any HW_FAILSAFEs are engaged.
    //
    if (thermEventViolationGetTimer32(RM_PMU_THERM_EVENT_META_ANY_HW_FS,
                                      &thermHwFsTimerns) != FLCN_OK)
    {
        PMU_HALT();
        return pMarch->action;
    }

    // Determine the limit value against which to cap.
    limitLwrr = pwrPolicyLimitLwrrGet(pPolicy);

    // Determine the hysteresis uncap value as a function of the limit value.
    switch (pMarch->hysteresis.type)
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_MARCH_HYSTERESIS_TYPE_RATIO:
            //
            // Note: 32BIT_OVERFLOW - Possible FXP20.12 overflow, not going to fix
            // because this code segment is tied to the @ref PMU_PMGR_PWR_POLICY_MARCH
            // feature and is not used on AD10X and later chips.
            //
            pMarch->uncapLimit = LW_TYPES_UFXP_X_Y_TO_U32_ROUNDED(20, 12,
                    limitLwrr * pMarch->hysteresis.data.ratio);
            break;
        case LW2080_CTRL_PMGR_PWR_POLICY_MARCH_HYSTERESIS_TYPE_STATIC_VALUE:
            pMarch->uncapLimit =
                (LwU32)LW_MAX(
                    0,
                    (LwS32)limitLwrr -
                        (LwS32)pMarch->hysteresis.data.staticValue);
            break;
    }

    // If the value is over the limit, cap.
    if (pPolicy->valueLwrr > limitLwrr)
    {
        pMarch->action = LW2080_CTRL_PMGR_PWR_POLICY_MARCH_ACTION_CAP;
    }
    //
    // If the value is under the uncap limit and no HW_FAILSAFE timer increase,
    // uncap.
    //
    else if ((pPolicy->valueLwrr <= pMarch->uncapLimit) &&
             (thermHwFsTimerns == pMarch->thermHwFsTimernsPrev))
    {
        pMarch->action = LW2080_CTRL_PMGR_PWR_POLICY_MARCH_ACTION_UNCAP;
    }
    // No action.
    else
    {
        pMarch->action = LW2080_CTRL_PMGR_PWR_POLICY_MARCH_ACTION_NONE;
    }

    // Cache the latest HW_FAILSAFE counter value.
    pMarch->thermHwFsTimernsPrev = thermHwFsTimerns;
    return pMarch->action;
}

/* ------------------------- Private Functions ------------------------------ */
