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
 * @file fanpolicy.h
 * @brief @copydoc fanpolicy.c
 */

#ifndef FAN_POLICY_H
#define FAN_POLICY_H

/* ------------------------- System Includes ------------------------------- */
#include "boardobj/boardobjgrp.h"
#include "pmu_fanctrl.h"
#include "therm/thrmchannel.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct FAN_POLICIES FAN_POLICIES;
typedef struct FAN_POLICY   FAN_POLICY, FAN_POLICY_BASE;

/* ------------------------- Application Includes -------------------------- */
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------- Macros ---------------------------------------- */
/*!
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_FAN_POLICY \
    (&(Fan.policies.super.super))

/*!
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define FAN_POLICY_GET(_objIdx) \
    (BOARDOBJGRP_OBJ_GET(FAN_POLICY, (_objIdx)))

/*!
  * Helper FAN_POLICY MACRO for @copydoc BOARDOBJ_TO_INTERFACE_CAST
  */
#define FAN_POLICY_BOARDOBJ_TO_INTERFACE_CAST(pPolicy, type)                   \
    BOARDOBJ_TO_INTERFACE_CAST((pPolicy), FAN, FAN_POLICY, type)

/* ------------------------- Datatypes ------------------------------------- */
// FAN_POLICY interfaces

/*!
 * @interface FAN_POLICY
 *
 * Loads a FAN_POLICY.
 *
 * @param[in]  pPolicy  FAN_POLICY object pointer
 *
 * @return FLCN_OK
 *      FAN_POLICY loaded successfully.
 */
#define FanPolicyLoad(fname) FLCN_STATUS (fname)(FAN_POLICY *pPolicy)

/*!
 * @interface FAN_POLICY
 *
 * Timer callback for the FAN_POLICY.
 *
 * @param[in]  pPolicy  FAN_POLICY object pointer
 *
 * @return  FLCN_ERR_NOT_SUPPORTED
 *      Policy object does not support this interface.
 * @return FLCN_OK
 *      FAN_POLICY timer callback exelwted successfully.
 */
#define FanPolicyCallback(fname) FLCN_STATUS (fname)(FAN_POLICY *pPolicy)

/*!
 * @interface FAN_POLICY
 *
 * Sets the desired fan PWM rate in percents (as SFXP16_16).
 *
 * @param[in]   pPolicy     FAN_POLICY object pointer
 * @param[in]   bBound      Set if PWM needs to be bounded
 * @param[out]  pwm         Desired PWM rate of the fan
 *
 * @return  FLCN_ERR_NOT_SUPPORTED
 *      Policy object does not support this interface.
 * @return  FLCN_OK
 *      Requested PWM rate was applied successfully.
 */
#define FanPolicyFanPwmSet(fname) FLCN_STATUS (fname)(FAN_POLICY *pPolicy, LwBool bBound, LwSFXP16_16 pwm)

/*!
 * @interface FAN_POLICY
 *
 * Sets the desired fan speed in RPMs.
 *
 * @param[in]   pPolicy     FAN_POLICY object pointer
 * @param[out]  rpm         Desired RPM speed of the fan
 *
 * @return  FLCN_ERR_NOT_SUPPORTED
 *      Policy object does not support this interface.
 * @return  FLCN_OK
 *      Requested RPM correction was performed successfully.
 */
#define FanPolicyFanRpmSet(fname) FLCN_STATUS (fname)(FAN_POLICY *pPolicy, LwS32 rpm)

/*!
 * @interface FAN_POLICY
 *
 * Gets the desired fan PWM rate in percents (as SFXP16_16).
 *
 * @param[in]   pPolicy     FAN_POLICY object pointer
 * @param[out]  pPwm        Buffer to store cooler's PWM rate
 *
 * @return  FLCN_ERR_NOT_SUPPORTED
 *      Policy object does not support this interface.
 * @return  FLCN_OK
 *      Requested PWM rate was successfully retrieved.
 */
#define FanPolicyFanPwmGet(fname) FLCN_STATUS (fname)(FAN_POLICY *pPolicy, LwSFXP16_16 *pPwm)

/*!
 * @interface FAN_POLICY
 *
 * Gets the desired fan speed in RPMs.
 *
 * @param[in]   pPolicy     FAN_POLICY object pointer
 * @param[out]  pRpm        Buffer to store fan RPM speed
 *
 * @return  FLCN_ERR_NOT_SUPPORTED
 *      Policy object does not support this interface.
 * @return  FLCN_OK
 *      Requested tachometer reading was successfully retrieved.
 */
#define FanPolicyFanRpmGet(fname) FLCN_STATUS (fname)(FAN_POLICY *pPolicy, LwS32 *pRpm)

/*!
 * @interface FAN_POLICY
 *
 * Gets the fan control action for the FAN_POLICY.
 *
 * @param[in]   pPolicy         FAN_POLICY object pointer
 * @param[out]  pFanCtrlAction  Buffer to store fan control action
 *
 * @return  FLCN_ERR_NOT_SUPPORTED
 *      Policy object does not support this interface.
 * @return  FLCN_OK
 *      Requested fan control action was successfully retrieved.
 */
#define FanPolicyFanCtrlActionGet(fname) FLCN_STATUS (fname)(FAN_POLICY *pPolicy, LwU8 *pFanCtrlAction)

/*!
 * Container to hold data for all FAN_POLICIES.
 */
struct FAN_POLICIES
{
    /*!
     * Board Object Group of all FAN_POLICY objects.
     */
    BOARDOBJGRP_E32 super;

#if !PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_MULTI_FAN)
    /*!
     * FAN_POLICY object pointer that is lwrrently supported i.e. its callback
     * being exelwted. In future, when multiple FAN_POLICIES will be supported
     * this will be removed. For now, only one FAN_POLICY is supported to
     * quickly unblock GTX7XX.
     */
    FAN_POLICY     *pPolicySupported;
#endif // PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_MULTI_FAN)

    /*!
     * Version of underlying Fan Policy Table in the driver @ref
     * LW2080_CTRL_FAN_POLICIES_VERSION_<xyz>.
     */
    LwU8            version;
};

/*!
 * Extends BOARDOBJ providing attributes common to all FAN_POLICIES.
 */
struct FAN_POLICY
{
    /*!
     * BOARDOBJ_VTABLE super class. This should always be the first member!
     */
    BOARDOBJ_VTABLE super;

    /*!
     * Parameters obtained from RM.
     */

    /*!
     * Index into the Thermal Channel Table.
     */
    LwU8            thermChIdx;

    /*!
     * PMU specific parameters.
     */

    /*!
     * Denotes the control action to be taken on the fan described via
     * @ref LW2080_CTRL_FAN_CTRL_ACTION_<xyz>.
     */
    LwU8            fanCtrlAction;

    /*!
     * Pointer to the channel to be used to read temperature.
     */
#if PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_POLICY_THERM_CHANNEL_USE)
    THERM_CHANNEL  *pThermChannel;
#endif // PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_POLICY_THERM_CHANNEL_USE)
};

/* ------------------------- Defines --------------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
FLCN_STATUS        fanPoliciesLoad(void);

FanPolicyLoad               (fanPolicyLoad);
FanPolicyLoad               (fanPolicyLoad_SUPER);
FanPolicyCallback           (fanPolicyCallback);
FanPolicyFanPwmSet          (fanPolicyFanPwmSet);
FanPolicyFanRpmSet          (fanPolicyFanRpmSet);
FanPolicyFanPwmGet          (fanPolicyFanPwmGet);
FanPolicyFanRpmGet          (fanPolicyFanRpmGet);
FanPolicyFanCtrlActionGet   (fanPolicyFanCtrlActionGet);

/*!
 * Board Object interfaces.
 */
BoardObjGrpIfaceModel10SetHeader   (fanFanPolicyIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommonConstruct", "fanFanPolicyIfaceModel10SetHeader");
BoardObjGrpIfaceModel10ObjSet           (fanPolicyGrpIfaceModel10ObjSetImpl_SUPER)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommonConstruct", "fanPolicyGrpIfaceModel10ObjSetImpl_SUPER");
BoardObjGrpIfaceModel10CmdHandler       (fanPoliciesBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommonConstruct", "fanPoliciesBoardObjGrpIfaceModel10Set");
BoardObjGrpIfaceModel10SetEntry   (fanFanPolicyIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommonConstruct", "fanFanPolicyIfaceModel10SetEntry");
BoardObjGrpIfaceModel10CmdHandler       (fanPoliciesBoardObjGrpIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommon", "fanPoliciesBoardObjGrpIfaceModel10GetStatus");
BoardObjIfaceModel10GetStatus               (fanFanPolicyIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommon", "fanFanPolicyIfaceModel10GetStatus");
BoardObjIfaceModel10GetStatus               (fanPolicyIfaceModel10GetStatus_SUPER)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommon", "fanPolicyIfaceModel10GetStatus_SUPER");

FLCN_STATUS fanPolicyPwrChannelQueryResponse(DISPATCH_FANCTRL_PWR_CHANNEL_QUERY_RESPONSE *pPwrChannelQuery);

/* ------------------------ Include Derived Types -------------------------- */
#include "fan/fan2x/20/fanpolicy_v20.h"
#include "fan/fan3x/30/fanpolicy_v30.h"
#include "fan/fan2x/20/fanpolicy_iir_tj_fixed_dual_slope_pwm_v20.h"
#include "fan/fan3x/30/fanpolicy_iir_tj_fixed_dual_slope_pwm_v30.h"

#endif // FAN_POLICY_H
