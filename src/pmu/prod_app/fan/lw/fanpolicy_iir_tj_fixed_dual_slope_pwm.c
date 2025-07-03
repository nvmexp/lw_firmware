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
 * @file  fanpolicy_iir_tj_fixed_dual_slope_pwm.c
 * @brief FAN Fan Policy Specific to IIR_TJ_FIXED_DUAL_SLOPE_PWM
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "main.h"

/* ------------------------- Application Includes --------------------------- */
#include "fan/objfan.h"
#include "fan/fanpolicy.h"
#include "therm/objtherm.h"
#include "lib/lib_fxp.h"
#include "task_pmgr.h"
#include "pmu_objtimer.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static LwSFXP16_16       s_fanPolicyITFDSPIntegerPower(LwSFXP16_16 base, LwU8 exp);
static LwSFXP10_22       s_fanPolicyITFDSPAverageTemp(LwSFXP10_22 tjSource, LwSFXP10_22 tjTarget, LwSFXP16_16 gain);
static LwBool            s_fanPolicyITFDSFanStopProcess(FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM *pPol, LwTemp tjDriving);
static FLCN_STATUS       s_fanPolicyITFDSPTempValueGet(FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM *pPol, LwTemp *pLwTemp);

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Construct a FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM object.
 *
 * @copydoc boardObjInterfaceConstruct
 */
FLCN_STATUS
fanPolicyConstructImpl_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE
(
    BOARDOBJGRP                *pBObjGrp,
    BOARDOBJ_INTERFACE         *pInterface,
    RM_PMU_BOARDOBJ_INTERFACE  *pInterfaceDesc,
    INTERFACE_VIRTUAL_TABLE    *pVirtualTable
)
{
    RM_PMU_FAN_FAN_POLICY_BOARDOBJ_SET_IIR_TJ_FIXED_DUAL_SLOPE_PWM *pPolicyITFDSPSet =
        (RM_PMU_FAN_FAN_POLICY_BOARDOBJ_SET_IIR_TJ_FIXED_DUAL_SLOPE_PWM *)pInterfaceDesc;
    FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM        *pPolicyITFDSP;
    FLCN_STATUS                                    status;
    LwU8                                           i;

    // Construct and initialize parent-class object.
    status = boardObjInterfaceConstruct(pBObjGrp, pInterface, pInterfaceDesc, pVirtualTable);
    if (status != FLCN_OK)
    {
        goto fanPolicyConstructImpl_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE_exit;
    }
    pPolicyITFDSP = (FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM *)pInterface;

    // Set member variables.
    pPolicyITFDSP->bUsePwm                   = pPolicyITFDSPSet->bUsePwm;
    pPolicyITFDSP->iirGainMin                = pPolicyITFDSPSet->iirGainMin;
    pPolicyITFDSP->iirGainMax                = pPolicyITFDSPSet->iirGainMax;
    pPolicyITFDSP->iirGainShortTerm          = pPolicyITFDSPSet->iirGainShortTerm;
    pPolicyITFDSP->iirFilterPower            = pPolicyITFDSPSet->iirFilterPower;
    pPolicyITFDSP->iirLongTermSamplingRatio  = pPolicyITFDSPSet->iirLongTermSamplingRatio;
    pPolicyITFDSP->iirFilterWidthUpper       = pPolicyITFDSPSet->iirFilterWidthUpper;
    pPolicyITFDSP->iirFilterWidthLower       = pPolicyITFDSPSet->iirFilterWidthLower;
    pPolicyITFDSP->midPoint.tj               = pPolicyITFDSPSet->midPoint.tj;
    pPolicyITFDSP->midPoint.pwm              = pPolicyITFDSPSet->midPoint.pwm;
    pPolicyITFDSP->midPoint.rpm              = pPolicyITFDSPSet->midPoint.rpm;
    for (i = 0;
         i < RM_PMU_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_NUM_SLOPES;
         i++)
    {
        pPolicyITFDSP->slope[i].pwm = pPolicyITFDSPSet->slope[i].pwm;
        pPolicyITFDSP->slope[i].rpm = pPolicyITFDSPSet->slope[i].rpm;
    }
#if PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_FAN_STOP)
    pPolicyITFDSP->bFanStopSupported         = pPolicyITFDSPSet->bFanStopSupported;
    pPolicyITFDSP->bFanStopEnable            = pPolicyITFDSPSet->bFanStopEnable;
    pPolicyITFDSP->fanStopTempLimitLower     = pPolicyITFDSPSet->fanStopTempLimitLower;
    pPolicyITFDSP->fanStartTempLimitUpper    = pPolicyITFDSPSet->fanStartTempLimitUpper;
    pPolicyITFDSP->fanStopPowerLimitLowermW  = pPolicyITFDSPSet->fanStopPowerLimitLowermW;
    pPolicyITFDSP->fanStartPowerLimitUppermW = pPolicyITFDSPSet->fanStartPowerLimitUppermW;
    pPolicyITFDSP->fanStopIIRGainPower       = pPolicyITFDSPSet->fanStopIIRGainPower;
    pPolicyITFDSP->fanStartMinHoldTime1024ns = pPolicyITFDSPSet->fanStartMinHoldTime1024ns;
    pPolicyITFDSP->fanStopPowerTopologyIdx   = pPolicyITFDSPSet->fanStopPowerTopologyIdx;
#endif // PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_FAN_STOP)

fanPolicyConstructImpl_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE_exit:
    return status;
}

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_FAN_STOP)
/*!
 * Dispatch function for processing PMGR event that sends latest
 * PWR_CHANNEL status.
 *
 * @param[in]  pPolicy                  FAN_POLICY object pointer
 * @param[in]  pPwrChannelQuery         Pointer to the dispatch structure
 *
 * @return FLCN_OK
 */
FLCN_STATUS
fanPolicyPwrChannelQueryResponse_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE
(
    FAN_POLICY                                  *pPolicy,
    DISPATCH_FANCTRL_PWR_CHANNEL_QUERY_RESPONSE *pPwrChannelQuery
)
{
    FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM *pPol =
        FAN_POLICY_BOARDOBJ_TO_INTERFACE_CAST(pPolicy, IIR_TJ_FIXED_DUAL_SLOPE_PWM);

    pPol->pmuStatus = pPwrChannelQuery->pmuStatus;
    if (pPol->pmuStatus == FLCN_OK)
    {
        // Do not apply filtering for first sample.
        if (pPol->filteredPwrmW == 0)
        {
            pPol->filteredPwrmW = (LwS32)pPwrChannelQuery->pwrmW;
        }
        else
        {
            //
            // Will overflow at 1048576 mW, which should be a safe assumption.
            // F32.0 = F32.0 + ((F32.0 * F20.12) >> 12).
            //
            pPol->filteredPwrmW +=
                multSFXP20_12(((LwS32)pPwrChannelQuery->pwrmW - pPol->filteredPwrmW),
                                pPol->fanStopIIRGainPower);
        }
    }

    return FLCN_OK;
}
#endif // PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_FAN_STOP)

/*!
 * Class implementation.
 *
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
fanPolicyQuery_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE
(
    FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM                                 *pPol,
    RM_PMU_FAN_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_BOARDOBJ_GET_STATUS  *pITFDSP
)
{
    pITFDSP->tjLwrrent       = pPol->tjLwrrent;
    pITFDSP->tjAvgShortTerm  = pPol->tjAvgShortTerm;
    pITFDSP->tjAvgLongTerm   = pPol->tjAvgLongTerm;
    if (pPol->bUsePwm)
    {
        pITFDSP->target.pwm  = pPol->target.pwm;
    }
    else
    {
        pITFDSP->target.rpm  = pPol->target.rpm;
    }
#if PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_FAN_STOP)
    pITFDSP->filteredPwrmW   = pPol->filteredPwrmW;
    pITFDSP->bFanStopActive  = pPol->bFanStopActive;
#else
    pITFDSP->filteredPwrmW   = 0;
    pITFDSP->bFanStopActive  = LW_FALSE;
#endif // PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_FAN_STOP)

    return FLCN_OK;
}

/*!
 * Class implementation.
 *
 * @copydoc FanPolicyLoad
 */
FLCN_STATUS
fanPolicyLoad_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE
(
    FAN_POLICY *pPolicy
)
{
    FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM *pPol =
        FAN_POLICY_BOARDOBJ_TO_INTERFACE_CAST(pPolicy, IIR_TJ_FIXED_DUAL_SLOPE_PWM);
    FLCN_STATUS status = FLCN_OK;

    // Set current Tj, STA and LTA.
    PMU_HALT_OK_OR_GOTO(status,
        s_fanPolicyITFDSPTempValueGet(pPol, &pPol->tjLwrrent),
        fanPolicyLoad_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE_exit);

    pPol->tjAvgShortTerm =
        (LwSFXP10_22)LW_TYPES_S32_TO_SFXP_X_Y(18, 14, pPol->tjLwrrent);
    pPol->tjAvgLongTerm  = pPol->tjAvgShortTerm;

    // Covering both PWM and RPM case.
    pPol->target.rpm = 0;

    // Set current sampling count.
    pPol->iirLongTermSamplingCount = pPol->iirLongTermSamplingRatio;

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_FAN_STOP)
    // Set default PMU status code.
    pPol->pmuStatus = FLCN_ERR_MORE_PROCESSING_REQUIRED;

    // Check if Fan Stop sub-policy needs power limits.
    pPol->bFanStopPwrLimitsApply = (pPol->fanStopPowerTopologyIdx !=
                                    LW2080_CTRL_PMGR_PWR_CHANNEL_INDEX_ILWALID);
#endif // PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_FAN_STOP)

fanPolicyLoad_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE_exit:
    return status;
}

/*!
 * Class implementation.
 *
 * @copydoc FanPolicyCallback
 */
FLCN_STATUS
fanPolicyCallback_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE
(
    FAN_POLICY *pPolicy
)
{
    FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM *pPol =
        FAN_POLICY_BOARDOBJ_TO_INTERFACE_CAST(pPolicy, IIR_TJ_FIXED_DUAL_SLOPE_PWM);
    FLCN_STATUS status  = FLCN_OK;
    LwTemp      tjDriving;

    // Obtain current temperature to update STA.
    PMU_HALT_OK_OR_GOTO(status,
        s_fanPolicyITFDSPTempValueGet(pPol, &pPol->tjLwrrent),
        fanPolicyCallback_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE_exit);

    // (F10.22 * F0.16) >> 16 => F10.22.
    pPol->tjAvgShortTerm =
        s_fanPolicyITFDSPAverageTemp(pPol->tjAvgShortTerm,
            LW_TYPES_S32_TO_SFXP_X_Y(18, 14, pPol->tjLwrrent), pPol->iirGainShortTerm);

    // Check if LTA needs to be updated.
    if (pPol->iirLongTermSamplingCount == pPol->iirLongTermSamplingRatio)
    {
        LwTemp      tjWidth;
        LwTemp      tjAvgDelta;
        LwSFXP16_16 powerTerm;
        LwSFXP16_16 avgGainLongTerm;

        // Check if average temperature is increasing or decreasing.
        tjWidth = (pPol->tjAvgLongTerm < pPol->tjAvgShortTerm) ?
                    pPol->iirFilterWidthUpper : pPol->iirFilterWidthLower;

         // (F10.22 - F10.22) >> 14 => F24.8.
        tjAvgDelta = LW_TYPES_SFXP_X_Y_TO_S32_ROUNDED(18, 14,
                        LW_ABS(pPol->tjAvgLongTerm - pPol->tjAvgShortTerm));

        // F24.8 << 16 => F8.24, F8.24 / F24.8 => F16.16.
        powerTerm =
            s_fanPolicyITFDSPIntegerPower(LW_TYPES_S32_TO_SFXP_X_Y_SCALED(16, 16,
                tjAvgDelta, tjWidth), pPol->iirFilterPower);

        // Callwlate LTAG and update LTA.
        avgGainLongTerm =
            pPol->iirGainMin + multSFXP16_16(pPol->iirGainMax, powerTerm);
        avgGainLongTerm = LW_MIN(avgGainLongTerm, pPol->iirGainMax);

        // (F10.22 * F16.16) >> 16 => F10.22.
        pPol->tjAvgLongTerm  =
            s_fanPolicyITFDSPAverageTemp(pPol->tjAvgLongTerm,
                pPol->tjAvgShortTerm, avgGainLongTerm);
    }

    // F10.22 >> 14 => F24.8.
    tjDriving = LW_TYPES_SFXP_X_Y_TO_S32_ROUNDED(18, 14, pPol->tjAvgLongTerm);

    // Use PWM/RPM fan lwrves if Fan Stop sub-policy isn't enabled/active.
    if (!s_fanPolicyITFDSFanStopProcess(pPol, tjDriving))
    {
        LwTemp  tjDrivingDelta;

        // Check by how much driving Tj is above or below midpoint B's Tj.
        tjDrivingDelta = tjDriving - pPol->midPoint.tj;

        // Callwlate and apply target PWM/RPM depending on policy control selection.
        if (pPol->bUsePwm)
        {
            LwSFXP8_24  pwmSlope;
            LwSFXP16_16 pwmTarget;

            // Choose slope based on whether driving Tj is above or below midpoint B's Tj.
            pwmSlope = (tjDrivingDelta < RM_PMU_LW_TEMP_0_C) ?
                        pPol->slope[RM_PMU_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_SLOPE_AB_IDX].pwm :
                        pPol->slope[RM_PMU_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_SLOPE_BC_IDX].pwm;

            // F16.16 = F16.16 + ((F24.8 * F8.24) >> 16)
            pwmTarget =
                pPol->midPoint.pwm + multSFXP16_16(tjDrivingDelta, pwmSlope);

            // Set the target PWM and cache it.
            status  = fanPolicyFanPwmSet(pPolicy, LW_TRUE, pwmTarget);
        }
        else
        {
            LwSFXP24_8  rpmSlope;
            LwS32       rpmTarget;

            // Choose slope based on whether driving Tj is above or below midpoint B's Tj.
            rpmSlope = (tjDrivingDelta < RM_PMU_LW_TEMP_0_C) ?
                        pPol->slope[RM_PMU_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_SLOPE_AB_IDX].rpm :
                        pPol->slope[RM_PMU_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_SLOPE_BC_IDX].rpm;

            // F32.0 = F32.0 + ((F24.8 * F24.8) >> 16)
            rpmTarget =
                (LwS32)pPol->midPoint.rpm + multSFXP16_16(tjDrivingDelta, rpmSlope);

            // Set the target RPM and cache it.
            status  = fanPolicyFanRpmSet(pPolicy, rpmTarget);
        }

        // Halt the PMU for now.
        if (status != FLCN_OK)
        {
            PMU_HALT();
        }
    }

    //
    // Update sampling count. LTA would be updated only when sampling count
    // equals sampling ratio.
    //
    if ((--pPol->iirLongTermSamplingCount) == 0)
    {
        pPol->iirLongTermSamplingCount = pPol->iirLongTermSamplingRatio;
    }

fanPolicyCallback_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE_exit:
    return status;
}

/*!
 * Class implementation.
 *
 * @copydoc FanPolicyFanPwmSet
 */
FLCN_STATUS
fanPolicyFanPwmSet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE
(
    FAN_POLICY *pPolicy,
    LwBool      bBound,
    LwSFXP16_16 pwm
)
{
    FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM *pPol =
        FAN_POLICY_BOARDOBJ_TO_INTERFACE_CAST(pPolicy, IIR_TJ_FIXED_DUAL_SLOPE_PWM);

    pPol->target.pwm = pwm;

    return FLCN_OK;
}

/*!
 * Class implementation.
 *
 * @copydoc FanPolicyFanRpmSet
 */
FLCN_STATUS
fanPolicyFanRpmSet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE
(
    FAN_POLICY *pPolicy,
    LwS32       rpm
)
{
    FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM *pPol =
        FAN_POLICY_BOARDOBJ_TO_INTERFACE_CAST(pPolicy, IIR_TJ_FIXED_DUAL_SLOPE_PWM);

    pPol->target.rpm = rpm;

    return FLCN_OK;
}

/*!
 * Class implementation.
 *
 * @copydoc FanPolicyFanPwmGet
 */
FLCN_STATUS
fanPolicyFanPwmGet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE
(
    FAN_POLICY     *pPolicy,
    LwSFXP16_16    *pPwm
)
{
    FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM *pPol =
        FAN_POLICY_BOARDOBJ_TO_INTERFACE_CAST(pPolicy, IIR_TJ_FIXED_DUAL_SLOPE_PWM);
    FLCN_STATUS status = FLCN_OK;

    // Sanity checks.
    if (pPwm == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto fanPolicyFanPwmGet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE_exit;
    }

    *pPwm = pPol->target.pwm;

fanPolicyFanPwmGet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE_exit:
    return status;
}

/*!
 * Class implementation.
 *
 * @copydoc FanPolicyFanRpmGet
 */
FLCN_STATUS
fanPolicyFanRpmGet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE
(
    FAN_POLICY     *pPolicy,
    LwS32          *pRpm
)
{
    FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM *pPol =
        FAN_POLICY_BOARDOBJ_TO_INTERFACE_CAST(pPolicy, IIR_TJ_FIXED_DUAL_SLOPE_PWM);
    FLCN_STATUS status = FLCN_OK;

    // Sanity checks.
    if (pRpm == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto fanPolicyFanRpmGet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE_exit;
    }

    *pRpm = pPol->target.rpm;

fanPolicyFanRpmGet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE_exit:
    return status;
}

/* ------------------------ Private Static Functions ------------------------*/

/*!
 * Helper which implements an integer power function.
 *
 * @param[in]  base Base
 * @param[in]  exp  Exponent
 */
static LwSFXP16_16
s_fanPolicyITFDSPIntegerPower
(
    LwSFXP16_16 base,
    LwU8        exp
)
{
    LwSFXP16_16 result = LW_TYPES_S32_TO_SFXP_X_Y(16, 16, 1);
    LwSFXP16_16 limit;

    //
    // WAR: Avoiding overflow by limiting result to max supported value. Impacts
    // only testing when TempSim can fake enormous temperature gradients.
    //
    limit = LW_S32_MAX / (LW_TYPES_SFXP_X_Y_TO_S32(16, 16, base) + 1);

    while (exp-- > 0)
    {
        if (result > limit)
        {
            result = LW_S32_MAX;
            break;
        }
        result = multSFXP16_16(result, base);
    }

    return result;
}

/*!
 * Helper which callwlates average temperature by multiplying the specified
 * gain with the difference between target and source temperature.
 *
 * @param[in]  tjSource Source temperature
 * @param[in]  tjTarget Target temperature
 * @param[in]  gain     Associated gain
 */
static LwSFXP10_22
s_fanPolicyITFDSPAverageTemp
(
    LwSFXP10_22 tjSource,
    LwSFXP10_22 tjTarget,
    LwSFXP16_16 gain
)
{
    LwSFXP10_22 tjDelta;

    tjDelta = tjTarget - tjSource;
    tjDelta = multSFXP32(16, tjDelta, gain);

    return (tjSource + tjDelta);
}

/*!
 * Helper which implements following Fan Stop sub-policy -
 *
 * Fan will be stopped when both GPU Tj and power (if applicable) fall below
 * their respective lower limits. It will be re-started when either GPU Tj or
 * power is at or above their respective upper limits. The gain value is used
 * for filtering power before being compared against the lower or upper limit.
 * After fan is re-started, it is driven at Electrical Fan Speed Min for
 * certain specified time to prevent fan overshoot.
 *
 * Fan will also be re-started if Fan Stop sub-policy is disabled on the fly
 * or THERM task is detached.
 *
 * @param[in]  pPol         FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM object pointer
 * @param[in]  tjDriving    Driving temperature for Fan Stop sub-policy
 *
 * @return LW_TRUE
 *      If this helper set the target PWM based on the Fan Stop sub-policy
 * @return LW_FALSE
 *      If fan speed needs to be decided based on PWM/RPM fan lwrves
 */
static LwBool
s_fanPolicyITFDSFanStopProcess
(
    FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM *pPol,
    LwTemp                                  tjDriving
)
{
    FAN_POLICY *pPolicy     = (FAN_POLICY *)INTERFACE_TO_BOARDOBJ_CAST(pPol);
    LwBool      bProcessed  = LW_FALSE;

    // Set default fan control action to SPEED_CTRL.
    pPolicy->fanCtrlAction = LW2080_CTRL_FAN_CTRL_ACTION_SPEED_CTRL;

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_FAN_STOP)
    if (pPol->bFanStopSupported)
    {
        LwBool          bFanRestart   = LW_FALSE;
        FLCN_TIMESTAMP  now;
        LwU32           now1024ns;

        // Obtain current time.
        osPTimerTimeNsLwrrentGet(&now);
        now1024ns = TIMER_TO_1024_NS(now);

        // Check if Fan Stop sub-policy needs to be enabled.
        if (pPol->bFanStopEnable &&
            !Fan.bTaskDetached)
        {
            // If yes, send an event to PMGR task for obtaining PWR_CHANNEL status.
            if (pPol->bFanStopPwrLimitsApply)
            {
                DISPATCH_PMGR   disp2Pmgr;

                disp2Pmgr.hdr.eventType      = PMGR_EVENT_ID_THERM_PWR_CHANNEL_QUERY;
                disp2Pmgr.query.pwrChIdx     = pPol->fanStopPowerTopologyIdx;
                disp2Pmgr.query.fanPolicyIdx = BOARDOBJ_GET_GRP_IDX_8BIT(&pPolicy->super.super);

                lwrtosQueueIdSendBlocking(LWOS_QUEUE_ID(PMU, PMGR),
                                          &disp2Pmgr, sizeof(disp2Pmgr));
            }

            //
            // If lower temperature and/or power limit is crossed, stop the fan
            // provided it wasn't just re-started.
            //
            if (!pPol->bFanStopActive)
            {
                if ((tjDriving < pPol->fanStopTempLimitLower) &&
                    (pPol->fanStartTimestamp1024ns == 0))
                {
                    if (pPol->bFanStopPwrLimitsApply)
                    {
                        if ((pPol->pmuStatus == FLCN_OK) &&
                            (pPol->filteredPwrmW < (LwS32)pPol->fanStopPowerLimitLowermW))
                        {
                            pPol->bFanStopActive = LW_TRUE;
                        }
                    }
                    else
                    {
                        pPol->bFanStopActive = LW_TRUE;
                    }
                }
            }
            else
            {
                //
                // If upper temperature or power limit is reached, re-start
                // the fan.
                //
                if ((tjDriving >= pPol->fanStartTempLimitUpper) ||
                    ((pPol->bFanStopPwrLimitsApply) &&
                     (pPol->pmuStatus == FLCN_OK) &&
                     (pPol->filteredPwrmW >= (LwS32)pPol->fanStartPowerLimitUppermW)))
                {
                    pPol->fanStartTimestamp1024ns = now1024ns;
                    pPol->bFanStopActive = LW_FALSE;
                    bFanRestart = LW_TRUE;
                }
            }
        }
        else if (pPol->bFanStopActive)
        {
            //
            // Fan Stop sub-policy is either disabled on the fly or THERM task is
            // detached. In such cases, if fan is stopped re-start it.
            //
            pPol->fanStartTimestamp1024ns = now1024ns;
            pPol->bFanStopActive = LW_FALSE;
            bFanRestart = LW_TRUE;
        }

        //
        // After fan is re-started, make sure it spins at Electrical Fan Speed
        // Min for certain specified time to prevent overshoot.
        //
        if (pPol->fanStartTimestamp1024ns != 0)
        {
            if (pPol->fanStartMinHoldTime1024ns >
                (now1024ns - pPol->fanStartTimestamp1024ns))
            {
                bFanRestart = LW_TRUE;
            }
            else
            {
                pPol->fanStartTimestamp1024ns = 0;
            }
        }

        // Set the target PWM based on current state of Fan Stop sub-policy.
        if (pPol->bFanStopActive ||
            bFanRestart)
        {
            if (fanPolicyFanPwmSet(pPolicy, !pPol->bFanStopActive, 0) != FLCN_OK)
            {
                PMU_HALT();
            }
            bProcessed = LW_TRUE;

            // Fan either needs to be stopped or restarted.
            if (pPol->bFanStopActive)
            {
                pPolicy->fanCtrlAction = LW2080_CTRL_FAN_CTRL_ACTION_STOP;
            }
            else
            {
                pPolicy->fanCtrlAction = LW2080_CTRL_FAN_CTRL_ACTION_RESTART;
            }
        }
    }
#endif // PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_FAN_STOP)

    return bProcessed;
}

/*!
 * Helper which obtains the current temperature.
 *
 * @param[in]   pPol        FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM object pointer
 * @param[out]  pLwTemp     Pointer to obtained temperature
 *
 * @return FLCN_OK
 *      If the temperature is obtained successfully.
 * @return Other unexpected errors
 *     Unexpected errors propagated from other functions.
 */
static FLCN_STATUS
s_fanPolicyITFDSPTempValueGet
(
    FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM  *pPol,
    LwTemp                                  *pLwTemp
)
{
    FLCN_STATUS status = FLCN_OK;

    // Obtain the current temperature.
#if PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_POLICY_THERM_CHANNEL_USE)
    FAN_POLICY *pPolicy =
        (FAN_POLICY *)INTERFACE_TO_BOARDOBJ_CAST(pPol);

    status = thermChannelTempValueGet(pPolicy->pThermChannel, pLwTemp);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        return status;
    }
#else
    *pLwTemp = thermGetTempInternal_HAL(&Therm,
                            LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_TSENSE);
#endif // PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_POLICY_THERM_CHANNEL_USE)

    return status;
}
