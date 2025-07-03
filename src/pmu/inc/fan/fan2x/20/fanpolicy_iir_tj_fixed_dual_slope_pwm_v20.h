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
 * @file fanpolicy_iir_tj_fixed_dual_slope_pwm_v20.h
 * @brief @copydoc fanpolicy_iir_tj_fixed_dual_slope_pwm_v20.c
 */

#ifndef FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20_H
#define FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "fan/fan2x/20/fanpolicy_v20.h"
#include "fan/fanpolicy_iir_tj_fixed_dual_slope_pwm.h"

/* ------------------------- Macros ---------------------------------------- */
#define fanPolicyCallback_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20(pPolicy)          \
    fanPolicyCallback_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE(pPolicy)

#define fanPolicyFanPwmGet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20(pPolicy, pPwm)   \
    fanPolicyFanPwmGet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE(pPolicy, pPwm)

#define fanPolicyFanRpmGet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20(pPolicy, pRpm)   \
    fanPolicyFanRpmGet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE(pPolicy, pRpm)

/* ------------------------- Datatypes ------------------------------------- */
typedef struct FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20 FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20;

/*!
 * Extends FAN_POLICY_V20 providing attributes specific to IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20 policy.
 */
struct FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20
{
    /*!
     * FAN_POLICY_V20 super class. This should always be the first member!
     */
    FAN_POLICY_V20  super;

    /*!
     * FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM super class.
     */
    FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM
                    iirTFDSP;
};

/* ------------------------- Defines --------------------------------------- */
#define fanPolicyPwrChannelQueryResponse_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20(pPolicy, pPwrChannelQuery)     \
    fanPolicyPwrChannelQueryResponse_IIR_TJ_FIXED_DUAL_SLOPE_PWM_INTERFACE((pPolicy), (pPwrChannelQuery))

/* ------------------------ Function Prototypes ---------------------------- */
/*!
 * BOARDOBJ interfaces.
 */
BoardObjGrpIfaceModel10ObjSet   (fanPolicyGrpIfaceModel10ObjSetImpl_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommonConstruct", "fanPolicyGrpIfaceModel10ObjSetImpl_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20");
BoardObjIfaceModel10GetStatus       (fanPolicyIfaceModel10GetStatus_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommon", "fanPolicyIfaceModel10GetStatus_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20");

FanPolicyLoad       (fanPolicyLoad_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20);
FanPolicyFanPwmSet  (fanPolicyFanPwmSet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20);
FanPolicyFanRpmSet  (fanPolicyFanRpmSet_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20);

/* ------------------------ Include Derived Types -------------------------- */

#endif // FAN_POLICY_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20_H
