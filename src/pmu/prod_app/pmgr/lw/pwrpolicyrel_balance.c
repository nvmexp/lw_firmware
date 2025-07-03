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
 * @file  pwrpolicyrel_balance.c
 * @brief Interface specification for a PWR_POLICY_RELATIONSHIP_BALANCE.
 *
 * To add theory documentation in follow-up CL which adds implementation.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "lib/lib_fxp.h"
#include "pmu_objhal.h"
#include "pmu_objpmgr.h"
#include "pmgr/pwrmonitor.h"
#include "pmgr/pwrchannel.h"
#include "pmgr/pwrpolicy.h"
#include "pmgr/lib_pmgr.h"
#include "therm/thrmintr.h"
#include "pmgr/pwrpolicyrel_balance.h"
#include "dbgprintf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static FLCN_STATUS s_pwrPolicyRelationshipBalancePwmSet(PWR_POLICY_RELATIONSHIP_BALANCE *pBalance, LwUFXP16_16 pwmPct)
    GCC_ATTRIB_NOINLINE();
static void       s_pwrPolicyRelationshipBalanceLimitsGet(PWR_POLICY_RELATIONSHIP_BALANCE *pBalance, LwU32 *pLimitPri, LwU32 *pLimitSec)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyRelationshipBalanceLimitsGet");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Construct a _BALANCE PWR_POLICY_RELATIONSHIP object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrPolicyRelationshipGrpIfaceModel10ObjSetImpl_BALANCE
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PMGR_PWR_POLICY_RELATIONSHIP_BALANCE_BOARDOBJ_SET *pBalanceSet     =
        (RM_PMU_PMGR_PWR_POLICY_RELATIONSHIP_BALANCE_BOARDOBJ_SET *)pBoardObjDesc;
    LwBool                                                    bFirstConstruct = (*ppBoardObj == NULL);
    PWR_POLICY_RELATIONSHIP_BALANCE                          *pBalance;
    FLCN_STATUS                                               status;

    // Construct and initialize parent-class object.
    status = pwrPolicyRelationshipGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj,
                                                      size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pwrPolicyRelationshipGrpIfaceModel10ObjSetImpl_BALANCE_exit;
    }
    pBalance = (PWR_POLICY_RELATIONSHIP_BALANCE *)*ppBoardObj;

    // Set class-specific data.
    pBalance->pSecPolicy           = PWR_POLICY_GET(pBalanceSet->secPolicyIdx);
    if (pBalance->pSecPolicy == NULL)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        goto pwrPolicyRelationshipGrpIfaceModel10ObjSetImpl_BALANCE_exit;
    }
    pBalance->bPwmSim              = pBalanceSet->bPwmSim;
    pBalance->pwmPeriod            = pBalanceSet->pwmPeriod;
    pBalance->pwmDutyCycleInitial  = pBalanceSet->pwmDutyCycleInitial;
    pBalance->pwmDutyCycleStepSize = pBalanceSet->pwmDutyCycleStepSize;
    pBalance->phaseEstimateChIdx   = pBalanceSet->phaseEstimateChIdx;
    pBalance->phaseCountMultiplier = pBalanceSet->phaseCountMultiplier;
    if (pBalance->phaseCountMultiplier == 0)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto pwrPolicyRelationshipGrpIfaceModel10ObjSetImpl_BALANCE_exit;
    }

#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_BALANCE_VIOLATION_AWARE))
    pBalance->violation.violTarget = 0x0100;                            // TODO - Add it to VBIOS POR
    pBalance->thrmEventIdx         = RM_PMU_THERM_EVENT_META_ANY_EDP;   // TODO - Add it to VBIOS POR
#endif

    if (bFirstConstruct)
    {
        // Construct PWM source descriptor data.
        pBalance->pPwmSrcDesc =
            pwmSourceDescriptorConstruct(pBObjGrp->ovlIdxDmem, pBalanceSet->pwmSource);
        if (pBalance->pPwmSrcDesc == NULL)
        {
            status = FLCN_ERR_ILWALID_STATE;
            goto pwrPolicyRelationshipGrpIfaceModel10ObjSetImpl_BALANCE_exit;
        }

        pBalance->pwmSource = pBalanceSet->pwmSource;
    }
    else
    {
        if (pBalance->pwmSource != pBalanceSet->pwmSource)
        {
            status = FLCN_ERR_ILWALID_STATE;
            goto pwrPolicyRelationshipGrpIfaceModel10ObjSetImpl_BALANCE_exit;
        }
    }

pwrPolicyRelationshipGrpIfaceModel10ObjSetImpl_BALANCE_exit:
    return status;
}

/*!
 * _BALANCE implemention of PwrPolicyRelationshipLoad interface.  Applies the
 * default initial PWM values to the circuit.
 *
 * @copydoc PwrPolicyRelationshipLoad
 */
FLCN_STATUS
pwrPolicyRelationshipLoad_BALANCE
(
    PWR_POLICY_RELATIONSHIP *pPolicyRel
)
{
    PWR_POLICY_RELATIONSHIP_BALANCE *pBalance =
        (PWR_POLICY_RELATIONSHIP_BALANCE *)pPolicyRel;

#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_BALANCE_VIOLATION_AWARE))
    LwU32 lastTimeViol;

    //
    // Code should not get interrupted/preempted when sampling timers.
    //
    // Warning:
    //      If you instead call ref@ libPmgrViolationLwrrGet, please
    // attach the overlay pmgrLibPwrPolicy
    //
    appTaskCriticalEnter();
    {
        osPTimerTimeNsLwrrentGet(&pBalance->violation.lastTimeEval);
        (void)thermEventViolationGetTimer32(pBalance->thrmEventIdx,
                                          &lastTimeViol);
    }
    appTaskCriticalExit();

    // Copy out the lastTimeViol from LwU32 to FLCN_TIMESTAMP.
    pBalance->violation.lastTimeViol.parts.lo = lastTimeViol;
#endif

    // Program the initial pct/duty cycle.
    return s_pwrPolicyRelationshipBalancePwmSet(pBalance,
                pBalance->pwmDutyCycleInitial);
}

/*!
 * _BALANCE implemention of BoardObjIfaceModel10GetStatus interface.
 *
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
pwrPolicyRelationshipIfaceModel10GetStatus_BALANCE
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PMGR_PWR_POLICY_RELATIONSHIP_BALANCE_BOARDOBJ_GET_STATUS *pGetStatus =
        (RM_PMU_PMGR_PWR_POLICY_RELATIONSHIP_BALANCE_BOARDOBJ_GET_STATUS *)pPayload;
    PWR_POLICY_RELATIONSHIP_BALANCE *pBalance =
        (PWR_POLICY_RELATIONSHIP_BALANCE *)pBoardObj;

    pGetStatus->action                 = pBalance->action;
    pGetStatus->pwmPctLwrr             = pBalance->pwmPctLwrr;
    pGetStatus->phaseEstimateValueLwrr = pBalance->phaseEstimateValueLwrr;
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_BALANCE_VIOLATION_AWARE))
    pGetStatus->violLwrrent            = pBalance->violation.violLwrrent;
#endif
    memcpy(
        pGetStatus->propLimits,
        pBalance->propLimits,
        LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_STATUS_BALANCE_MAX_PROP_LIMITS *
            sizeof(LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_STATUS_BALANCE_PROP_LIMIT));

    return FLCN_OK;
}

/*!
 * @copydoc PwrPolicyRelationshipBalanceLimitsGet()
 */
void
pwrPolicyRelationshipBalanceLimitsDiffGet
(
    PWR_POLICY_RELATIONSHIP_BALANCE  *pBalance,
    LwU32                            *pLimitLower,
    LwU32                            *pLimitDiff
)
{
    LwU32 limitPri;
    LwU32 limitSec;

    s_pwrPolicyRelationshipBalanceLimitsGet(pBalance, &limitPri, &limitSec);

    if (limitPri < limitSec)
    {
        *pLimitLower = limitPri;
        *pLimitDiff  = limitSec - limitPri;
    }
    else
    {
        *pLimitLower = limitSec;
        *pLimitDiff  = limitPri - limitSec;
    }
}

/*!
 * @copydoc PwrPolicyRelationshipBalanceEvaluate
 */
FLCN_STATUS
pwrPolicyRelationshipBalanceEvaluate
(
    PWR_POLICY_RELATIONSHIP_BALANCE *pBalance
)
{
    FLCN_STATUS status     = FLCN_OK;
    LwS32       pwrShiftmW;
    LwSFXP16_16 pwmPctNew  = (LwSFXP16_16)pBalance->pwmPctLwrr;
    PWR_POLICY_PROP_LIMIT *pPropLimitPri = (PWR_POLICY_PROP_LIMIT *)pBalance->super.pPolicy;
    PWR_POLICY_PROP_LIMIT *pPropLimitSec = (PWR_POLICY_PROP_LIMIT *)pBalance->pSecPolicy;
    LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_STATUS_BALANCE_PROP_LIMIT *pStatusPri =
        &pBalance->propLimits[LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_STATUS_BALANCE_PROP_LIMIT_PRI];
    LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_STATUS_BALANCE_PROP_LIMIT *pStatusSec =
        &pBalance->propLimits[LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_STATUS_BALANCE_PROP_LIMIT_SEC];

    // Initialize the action to NONE, will determine below if action is required.
    pBalance->action =
        LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_STATUS_BALANCE_ACTION_NONE;
    // Clear out the status data with garbage so untouched values are obvious.
    memset(pBalance->propLimits, 0x69,
        LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_STATUS_BALANCE_MAX_PROP_LIMITS *
            sizeof(LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_STATUS_BALANCE_PROP_LIMIT));

    //
    // Query the initial state of the balanced PWR_POLICY_PROP_LIMIT objects:
    //
    // 1. Dirty flags.
    pStatusPri->bBalanceDirty = pwrPolicyPropLimitBalanceDirtyGet(pPropLimitPri);
    pStatusSec->bBalanceDirty = pwrPolicyPropLimitBalanceDirtyGet(pPropLimitSec);
    // 2. Original/old input values.
    pStatusPri->valueOld = (LwS32)pwrPolicyValueLwrrGet(&pPropLimitPri->super.super);
    pStatusSec->valueOld = (LwS32)pwrPolicyValueLwrrGet(&pPropLimitSec->super.super);
    // 3. Original/old requested limits.
    s_pwrPolicyRelationshipBalanceLimitsGet(
        pBalance,                      // pBalance
        (LwU32*)&pStatusPri->limitRequestOld,  // pLimitPri
        (LwU32*)&pStatusSec->limitRequestOld); // pLimitSec

    //
    // If PWM simulation is engaged or either PWR_POLICY_PROP_LIMIT object is
    // dirty, can't do any balancing.
    //
    if (pBalance->bPwmSim ||
        pStatusPri->bBalanceDirty ||
        pStatusSec->bBalanceDirty)
    {
        goto pwrPolicyRelationshipBalanceEvaluate_done;
    }

    //
    // Determine toward which PWR_POLICY_PROP_LIMIT object this
    // PWR_POLICY_RELATIONSHIP_BALANCE object balance - i.e. the
    // PWR_POLICY_PROP_LIMIT requesting the lowest limit.
    //

#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_BALANCE_VIOLATION_AWARE))
    //
    // Check if the violation is greater than the expected target value. If so,
    // does not take any power balancing action.
    //
    if (pBalance->violation.violLwrrent > pBalance->violation.violTarget)
    {
        goto pwrPolicyRelationshipBalanceEvaluate_done;
    }
    // Otherwise, decide the balacing action based on the previous requested limit value
    else
#endif
    {
        if ((pStatusPri->limitRequestOld > pStatusSec->limitRequestOld) &&
            (pwmPctNew < LW_TYPES_S32_TO_SFXP_X_Y(16, 16, 1)))
        {
            pBalance->action =
                LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_STATUS_BALANCE_ACTION_TO_PRI;
        }
        else if ((pStatusPri->limitRequestOld < pStatusSec->limitRequestOld) &&
                 (pwmPctNew > LW_TYPES_FXP_ZERO))
        {
            pBalance->action =
                LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_STATUS_BALANCE_ACTION_TO_SEC;
        }
    }

    // If no action is required, bail out.
    if (LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_STATUS_BALANCE_ACTION_NONE ==
            pBalance->action)
    {
        goto pwrPolicyRelationshipBalanceEvaluate_done;
    }

    //
    //  If no phase estimate available, assume the shift is okay, but mark the
    //  PROP_LIMIT objects as dirty.
    //
    if (LW2080_CTRL_PMGR_PWR_CHANNEL_INDEX_ILWALID ==
            pBalance->phaseEstimateChIdx)
    {
        pwrPolicyPropLimitBalanceDirtySet(pPropLimitPri, LW_TRUE);
        pwrPolicyPropLimitBalanceDirtySet(pPropLimitSec, LW_TRUE);

        // Set the new values to be same as the old.
        pStatusPri->valueNew        = pStatusPri->valueOld;
        pStatusSec->valueNew        = pStatusSec->valueOld;
        pStatusPri->limitRequestNew = pStatusPri->limitRequestOld;
        pStatusSec->limitRequestNew = pStatusSec->limitRequestOld;
    }
    // If have phase estimate, ensure the resulting new state would be better than old state.
    else
    {
        //
        // A. Determine how much power would be shifted as a result:
        //      pwrShiftmW = phasePower * stepSize.
        //
        pwrShiftmW = (LwS32)multUFXP32(16,
                        pBalance->phaseEstimateValueLwrr * pBalance->phaseCountMultiplier,
                        pBalance->pwmDutyCycleStepSize);

        //
        // B. Update the PWR_POLICY_PROP_LIMIT input values with the estimated
        // power changes.
        //
        pStatusPri->valueNew = pStatusPri->valueOld + pBalance->action * pwrShiftmW;
        pStatusPri->valueNew = LW_MAX(pStatusPri->valueNew, 0);
        pStatusSec->valueNew = pStatusSec->valueOld - pBalance->action * pwrShiftmW;
        pStatusSec->valueNew = LW_MAX(pStatusSec->valueNew, 0);
        pwrPolicyValueLwrrSet((&pPropLimitPri->super.super), (LwU32)pStatusPri->valueNew);
        pwrPolicyValueLwrrSet((&pPropLimitSec->super.super), (LwU32)pStatusSec->valueNew);

        //
        // C. Update the PWR_POLICY_PROP_LIMIT requested values based on the new
        // input values.
        //
        pwrPolicyLimitEvaluate(&pPropLimitPri->super);
        pwrPolicyLimitEvaluate(&pPropLimitSec->super);
        s_pwrPolicyRelationshipBalanceLimitsGet(
            pBalance,                               // pBalance
            (LwU32 *)&pStatusPri->limitRequestNew,  // pLimitPri
            (LwU32 *)&pStatusSec->limitRequestNew); // pLimitSec

        //
        // D. Confirm that the new limit values are better than the previous
        // ones.  If not, revert back to original configuration.
        //
        if (LW_MIN(pStatusPri->limitRequestNew, pStatusSec->limitRequestNew) <
            LW_MIN(pStatusPri->limitRequestOld, pStatusSec->limitRequestOld))
        {
            pwrPolicyValueLwrrSet((&pPropLimitPri->super.super), (LwU32)pStatusPri->valueOld);
            pwrPolicyValueLwrrSet((&pPropLimitSec->super.super), (LwU32)pStatusSec->valueOld);

            pwrPolicyLimitEvaluate(&pPropLimitPri->super);
            pwrPolicyLimitEvaluate(&pPropLimitSec->super);

            pBalance->action =
                LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_STATUS_BALANCE_ACTION_NONE;
            goto pwrPolicyRelationshipBalanceEvaluate_done;
        }
    }

    // Apply new PWM settings to balancing circuit.
    pwmPctNew += pBalance->action * (LwSFXP16_16)pBalance->pwmDutyCycleStepSize;
    pwmPctNew = LW_MIN(pwmPctNew, LW_TYPES_S32_TO_SFXP_X_Y(16, 16, 1));
    pwmPctNew = LW_MAX(pwmPctNew, LW_TYPES_FXP_ZERO);
    status = s_pwrPolicyRelationshipBalancePwmSet(pBalance, (LwUFXP16_16)pwmPctNew);

pwrPolicyRelationshipBalanceEvaluate_done:
    return status;
}

/*!
 * @copydoc PwrPolicyRelationshipBalancePwmPctGetIdx
 */
LwUFXP16_16
pwrPolicyRelationshipBalancePwmPctGetIdx
(
    PWR_POLICY_RELATIONSHIP_BALANCE *pBalance,
    LwU8                             chIdx,
    LwU8                             chIdxEval,
    LwU8                             bScaleWithPhaseCount
)
{
    if (pwrChannelContains(
            PWR_CHANNEL_GET(pBalance->super.pPolicy->chIdx),
            chIdx,
            chIdxEval) ||
        pwrChannelContains(
            PWR_CHANNEL_GET(chIdx),
            pBalance->super.pPolicy->chIdx,
            chIdxEval))
    {
        return pwrPolicyRelationshipBalancePwmPctGet(pBalance, LW_TRUE,  bScaleWithPhaseCount);
    }
    else if (pwrChannelContains(
                PWR_CHANNEL_GET(pBalance->pSecPolicy->chIdx),
                chIdx,
                chIdxEval) ||
            pwrChannelContains(
                PWR_CHANNEL_GET(chIdx),
                pBalance->pSecPolicy->chIdx,
                chIdxEval))
    {
        return pwrPolicyRelationshipBalancePwmPctGet(pBalance, LW_FALSE, bScaleWithPhaseCount);
    }

    PMU_HALT();
    return LW_TYPES_FXP_ZERO;
}

/*!
 * @copydoc PwrPolicyRelationshipBalancePwmPctGet
 */
LwUFXP16_16
pwrPolicyRelationshipBalancePwmPctGet
(
    PWR_POLICY_RELATIONSHIP_BALANCE *pBalance,
    LwBool                           bPrimary,
    LwBool                           bScaleWithPhaseCount
)
{
    LwUFXP16_16 pctLwrr;

    if (bPrimary)
    {
        pctLwrr = pBalance->pwmPctLwrr;
    }
    else
    {
        pctLwrr = (LW_TYPES_U32_TO_UFXP_X_Y(16, 16, 1) - pBalance->pwmPctLwrr);
    }
    return (bScaleWithPhaseCount ? pBalance->phaseCountMultiplier : 1) * pctLwrr;
}

/*!
 * @copydoc PwrPolicyRelationshipChannelPwrGet
 */
FLCN_STATUS
pwrPolicyRelationshipChannelPwrGet_BALANCE
(
    PWR_POLICY_RELATIONSHIP             *pPolicyRel,
    PWR_MONITOR                         *pMon,
    PWR_POLICY_3X_FILTER_PAYLOAD_TYPE   *pPayload
)
{
    PWR_POLICY_RELATIONSHIP_BALANCE  *pBalance =
        (PWR_POLICY_RELATIONSHIP_BALANCE *)pPolicyRel;
    LW2080_CTRL_PMGR_PWR_TUPLE       *pTuple;

    // Sanity check for the channel index
    if (LW2080_CTRL_PMGR_PWR_CHANNEL_INDEX_ILWALID ==
            pBalance->phaseEstimateChIdx)
    {
        return FLCN_OK;
    }

    // Fetch the reading depending on current or power based policy type.
    pTuple = pwrPolicy3xFilterPayloadTupleGet(pPayload, pBalance->phaseEstimateChIdx);
    switch (pwrPolicyLimitUnitGet(pPolicyRel->pPolicy))
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_POWER_MW:
            pBalance->phaseEstimateValueLwrr = pTuple->pwrmW;
            break;
        case LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_LWRRENT_MA:
            pBalance->phaseEstimateValueLwrr = pTuple->lwrrmA;
            break;
        default:
            PMU_HALT();
            return FLCN_ERR_NOT_SUPPORTED;
    }

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * Helper function to program a PWM percentage value.
 *
 * @pre This function requires that caller has already attached .libPwm.
 *
 * @param[in] pBalance   PWR_POLICY_RELATIONSHIP_BALANCE pointer
 * @param[in] pwmPct     Desired PWM percentage
 *
 * @return Status returned from @ref pmgrPwmParamsSet().
 */
static FLCN_STATUS
s_pwrPolicyRelationshipBalancePwmSet
(
    PWR_POLICY_RELATIONSHIP_BALANCE *pBalance,
    LwUFXP16_16                      pwmPct
)
{
    LwU32 dutyCycle;

    // Callwlate actual RAW duty cycle value based on percent and RAW period.
    dutyCycle = pwmPct * pBalance->pwmPeriod;
    dutyCycle = LW_TYPES_UFXP_X_Y_TO_U32_ROUNDED(16, 16, dutyCycle);

    // Cache the latest PWM value as current.
    pBalance->pwmPctLwrr = pwmPct;

    return pwmParamsSet(pBalance->pPwmSrcDesc,  // pwmSrcDesc
                        dutyCycle,              // pwmDutyCycle
                        pBalance->pwmPeriod,    // pwmPeriod
                        LW_TRUE,                // bTrigger
                        LW_FALSE);              // bIlwert
}

/*!
 * Helper function which retrieves the requested limit values of the two
 * PWR_POLICY_PROP_LIMIT objects to which this PWR_POLICY_RELATIONSHIP_BALANCE
 * object points.
 *
 * @param[in]  pBalance     PWR_POLICY_RELATIONSHIP_BALANCE pointer
 * @param[out] pLimitPri
 *     Requested limit of the primary policy (@ref
 *     PWR_POLICY_RELATIONSHIP::pPolicy).
 * @param[out] pLimitSec
 *     Requested limit of the secondary policy (@ref
 *     PWR_POLICY_RELATIONSHIP_BALANCE::pSecPolicy).
 */
static void
s_pwrPolicyRelationshipBalanceLimitsGet
(
    PWR_POLICY_RELATIONSHIP_BALANCE  *pBalance,
    LwU32                            *pLimitPri,
    LwU32                            *pLimitSec
)
{
    //
    // Retrieve the limits requested by the two PWR_POLICY_PROP_LIMIT objects
    // being balanced by this circuit.
    //
    // @Note: This code assumes that each PWR_POLICY_PROP_LIMIT object only
    // affects a single PWR_POLICY, so can use hardocded relIdx = 0.
    //
    pwrPolicyPropLimitLimitGet(
        (PWR_POLICY_PROP_LIMIT *)pBalance->super.pPolicy, // pPropLimit
        0,                                               // relIdx
        pLimitPri);                                      // pLimitValue
    pwrPolicyPropLimitLimitGet(
        (PWR_POLICY_PROP_LIMIT *)pBalance->pSecPolicy, // pPropLimit
        0,                                            // relIdx
        pLimitSec);                                   // pLimitValue
}
