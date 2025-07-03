/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file fanpolicy_iir_tj_fixed_dual_slope_pwm_v30.h
 * @brief @copydoc fanpolicy_iir_tj_fixed_dual_slope_pwm_v30.c
 */

#ifndef FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30_H
#define FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "fan/fan3x/30/fanpolicy_v30.h"
#include "fan/fanpolicy_iir_tj_fixed_dual_slope_pwm.h"

/* ------------------------- Macros ---------------------------------------- */
#define fanPolicyPwrChannelQueryResponse_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30(pPolicy, pPwrChannelQuery)     \
    fanPolicyPwrChannelQueryResponse_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE(pPolicy, pPwrChannelQuery)

#define fanPolicyCallback_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30(pPolicy)                  \
    fanPolicyCallback_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE(pPolicy)

#define fanPolicyFanPwmGet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30(pPolicy, pPwm)           \
    fanPolicyFanPwmGet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE(pPolicy, pPwm)

#define fanPolicyFanRpmGet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30(pPolicy, pRpm)           \
    fanPolicyFanRpmGet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE(pPolicy, pRpm)

/* ------------------------- Datatypes ------------------------------------- */
typedef struct FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30 FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30;

/*!
 * Extends FAN_POLICY_V30 providing attributes specific to IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30 policy.
 */
struct FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30
{
    /*!
     * FAN_POLICY_V30 super class. This should always be the first member!
     */
    FAN_POLICY_V30  super;

    /*!
     * PWM Min for the Fan Lwrve.
     */
    LwSFXP16_16 pwmMin;

    /*!
     * PWM Max for the Fan Lwrve.
     */
    LwSFXP16_16 pwmMax;

    /*!
     * RPM Min for the Fan Lwrve.
     */
    LwU32       rpmMin;

    /*!
     * RPM Max for the Fan Lwrve.
     */
    LwU32       rpmMax;

    /*!
     * FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM super class.
     */
    FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM
                    iirTFDSP;
};

/* ------------------------- Defines --------------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/*!
 * BOARDOBJ interfaces.
 */
BoardObjGrpIfaceModel10ObjSet   (fanPolicyGrpIfaceModel10ObjSetImpl_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommonConstruct", "fanPolicyGrpIfaceModel10ObjSetImpl_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30");
BoardObjIfaceModel10GetStatus       (fanPolicyIfaceModel10GetStatus_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommon", "fanPolicyIfaceModel10GetStatus_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30");

FanPolicyLoad       (fanPolicyLoad_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30);
FanPolicyFanPwmSet  (fanPolicyFanPwmSet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30);
FanPolicyFanRpmSet  (fanPolicyFanRpmSet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30);

/* ------------------------ Include Derived Types -------------------------- */

#endif // FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30_H
