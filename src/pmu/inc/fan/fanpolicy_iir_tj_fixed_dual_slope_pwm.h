/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file fanpolicy_iir_tj_fixed_dual_slope_pwm.h
 * @brief @copydoc fanpolicy_iir_tj_fixed_dual_slope_pwm.c
 */

#ifndef FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_H
#define FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
/* ------------------------- Macros ---------------------------------------- */
/* ------------------------- Datatypes ------------------------------------- */
typedef struct FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM;

/*!
 * Extends FAN_POLICY providing attributes specific to IIR_TJ_FIXED_DUAL_SLOPE_PWM policy.
 */
struct FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM
{
    /*!
     * BOARDOBJ_INTERFACE super class. This should always be the first member!
     */
    BOARDOBJ_INTERFACE  super;

    /*!
     * Parameters obtained from RM.
     */

    /*!
     * Policy Control Selection.
     */
    LwBool      bUsePwm;

    /*!
     * Minimum IIR Gain.
     */
    LwSFXP16_16 iirGainMin;

    /*!
     * Maximum IIR Gain.
     */
    LwSFXP16_16 iirGainMax;

    /*!
     * Short Term IIR Gain.
     */
    LwSFXP16_16 iirGainShortTerm;

    /*!
     * IIR Filter Power.
     */
    LwU8        iirFilterPower;

    /*!
     * IIR Long Term Sampling Ratio.
     */
    LwU8        iirLongTermSamplingRatio;

    /*!
     * IIR Filter Lower Width (C).
     */
    LwTemp      iirFilterWidthUpper;

    /*!
     * IIR Filter Upper Width (C).
     */
    LwTemp      iirFilterWidthLower;

    /*!
     * Fan Lwrve Mid Point.
     */
    struct
    {
        /*!
         * Tj Fan Lwrve Point (C).
         */
        LwTemp      tj;

        /*!
         * PWM Fan Lwrve Point (PWM rate in percents).
         */
        LwSFXP16_16 pwm;

        /*!
         * RPM Fan Lwrve Point.
         */
        LwU32       rpm;
    } midPoint;

    /*!
     * Fan Lwrve Slopes which are interpreted as slope between point
     * 'i' and 'i + 1'.
     */
    struct
    {
        /*!
         * Slope callwlated using PWM and Tj as reference points.
         */
        LwSFXP8_24  pwm;

        /*!
         * Slope callwlated using RPM and Tj as reference points.
         */
        LwSFXP24_8  rpm;
    } slope[RM_PMU_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_NUM_SLOPES];

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_FAN_STOP)
    /*!
     * Specifies whether Fan Stop sub-policy is supported.
     */
    LwBool          bFanStopSupported;

    /*!
     * Specifies whether Fan Stop sub-policy needs to be enabled.
     */
    LwBool          bFanStopEnable;

    /*!
     * Fan Stop Lower Temperature Limit - Tj (C). Fan will be stopped when GPU
     * Tj falls below this temperature limit.
     */
    LwTemp          fanStopTempLimitLower;

    /*!
     * Fan Start Upper Temperature Limit - Tj (C). Fan will be re-started when
     * GPU Tj is at or above this temperature limit.
     */
    LwTemp          fanStartTempLimitUpper;

    /*!
     * Fan Stop Lower Power Limit - (mW). Fan will be stopped when both power
     * (specified by @ref fanStopPowerTopologyIdx) and GPU Tj (specified by
     * @ref fanStopTempLimitLower) fall below their respective limits.
     */
    LwU32           fanStopPowerLimitLowermW;

    /*!
     * Fan Start Upper Power Limit - (mW). Fan will be re-started when either
     * power (specified by @ref fanStopPowerTopologyIdx) or GPU Tj (specified
     * by @ref fanStartTempLimitUpper) is at or above their respective limits.
     */
    LwU32           fanStartPowerLimitUppermW;

    /*!
     * Fan Stop IIR Power Gain. The gain value is used for filtering power
     * (specified by @ref fanStopPowerTopologyIdx) before being compared
     * against @ref fanStopPowerLimitLower and @ref fanStartPowerLimitUpper.
     */
    LwSFXP20_12     fanStopIIRGainPower;

    /*!
     * Fan Start Min Hold Time. Minimum time to spend at Electrical Fan Speed
     * Min to prevent fan overshoot after being re-started. Units are multiple
     * of 1024 nanoseconds.
     */
    LwU32           fanStartMinHoldTime1024ns;

    /*!
     * Power Topology Index in the Power Topology Table. If the value is 0xFF,
     * only GPU Tj needs to be considered for Fan Stop sub-policy.
     */
    LwU8            fanStopPowerTopologyIdx;
#endif // PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_FAN_STOP)

    /*!
     * PMU specific parameters.
     */

    /*!
     * Current GPU Tj.
     */
    LwTemp          tjLwrrent;

    /*!
     * Current Short Term Average Temperature (C).
     */
    LwSFXP10_22     tjAvgShortTerm;

    /*!
     * Current Long Term Average Temperature (C).
     */
    LwSFXP10_22     tjAvgLongTerm;

    /*!
     * Target settings requested by the policy.
     */
    union
    {
        /*!
         * Target PWM (in percents), used when @ref bUsePwm is set.
         */
        LwSFXP16_16 pwm;

        /*!
         * Target RPM, used when @ref bUsePwm is not set.
         */
        LwS32       rpm;
    } target;

    /*!
     * Current IIR Long Term Sampling Count.
     */
    LwU8            iirLongTermSamplingCount;

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_FAN_STOP)
    /*!
     * Specifies whether Fan Stop sub-policy needs power limits.
     */
    LwBool      bFanStopPwrLimitsApply;

    /*!
     * Specifies whether Fan Stop sub-policy is active.
     */
    LwBool      bFanStopActive;

    /*!
     * PMU status code to describe failures while obtaining PWR_CHANNEL status.
     */
    FLCN_STATUS pmuStatus;

    /*!
     * Filtered PWR_CHANNEL value.
     */
    LwS32       filteredPwrmW;

    /*!
     * Timestamp of the last moment when Fan re-start conditions were satisfied.
     * Units are multiple of 1024 nanoseconds.
     */
    LwU32       fanStartTimestamp1024ns;
#endif // PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_FAN_STOP)
};

/* ------------------------- Defines ---------------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/*!
 * BOARDOBJ_INTERFACE interfaces.
 */
BoardObjInterfaceConstruct  (fanPolicyConstructImpl_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommonConstruct", "fanPolicyConstructImpl_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE");

FanPolicyLoad               (fanPolicyLoad_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE);
FanPolicyCallback           (fanPolicyCallback_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE);
FanPolicyFanPwmSet          (fanPolicyFanPwmSet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE);
FanPolicyFanRpmSet          (fanPolicyFanRpmSet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE);
FanPolicyFanPwmGet          (fanPolicyFanPwmGet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE);
FanPolicyFanRpmGet          (fanPolicyFanRpmGet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE);

FLCN_STATUS fanPolicyQuery_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE(FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM *pPol, RM_PMU_FAN_FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_BOARDOBJ_GET_STATUS *pITFDSP);
FLCN_STATUS fanPolicyPwrChannelQueryResponse_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE(FAN_POLICY *pPolicy, DISPATCH_FANCTRL_PWR_CHANNEL_QUERY_RESPONSE *pPwrChannelQuery);

/* ------------------------ Include Derived Types -------------------------- */

#endif // FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_H
