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
 * @file fancooler_active_pwm.h
 * @brief @copydoc fancooler_active_pwm.c
 */

#ifndef FAN_COOLER_ACTIVE_PWM_H
#define FAN_COOLER_ACTIVE_PWM_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "fan/fancooler_active.h"
#include "lib/lib_pwm.h"

/* ------------------------- Macros ---------------------------------------- */
// Determines if MAXFAN feature (SW/HW) is enabled.
#define FAN_COOLER_MAX_FAN_IS_ENABLED(_pCoolObj)                                         \
    ((_pCoolObj)->maxFanMinTime1024ns != 0)

#define fanCoolerActivePwmRpmGet(pCooler, pRpm) \
    fanCoolerActiveRpmGet((pCooler), (pRpm))

/* ------------------------- Datatypes ------------------------------------- */
typedef struct FAN_COOLER_ACTIVE_PWM FAN_COOLER_ACTIVE_PWM;

/*!
 * Extends FAN_COOLER providing attributes specific to ACTIVE_PWM cooler.
 */
struct FAN_COOLER_ACTIVE_PWM
{
    /*!
     * FAN_COOLER_ACTIVE super class. This should always be the first member!
     */
    FAN_COOLER_ACTIVE       super;

    /*!
     * Parameters obtained from RM.
     */

    /*!
     * Electrical Minimum Fan Speed (PWM rate in percents).
     */
    LwSFXP16_16             pwmMin;

    /*!
     * Electrical Maximum Fan Speed (PWM rate in percents).
     */
    LwSFXP16_16             pwmMax;

    /*!
     * PWM period.
     */
    LwU32                   period;

    /*!
     * RM_PMU_THERM_EVENT_<xyz> bitmask triggering MAX Fan speed (if engaged).
     */
    LwU32                   maxFanEvtMask;

#if !PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_COOLER_HW_MAX_FAN)
    /*!
     * Sum of violation counters from previous control period.
     */
    LwU32                   maxFalwiolationCountersSum;
#endif

    /*!
     * MIN time to spend at MAX Fan speed to prevent trashing at slowdown.
     * Units are multiple of 1024 nanoseconds.
     */
    LwU32                   maxFanMinTime1024ns;

    /*!
     * MAX Fan speed settings (PWM rate in percents).
     */
    LwSFXP16_16             maxFanPwm;

    /*!
     * Override value for Electrical Fan Speed (PWM rate in percents).
     * Applicable only when @ref bPctSimActive is LW_TRUE.
     */
    LwSFXP16_16             pwmSim;

    /*!
     * PMU specific parameters.
     */

    /*!
     * Current Fan Speed (PWM rate in percents).
     */
    LwSFXP16_16             pwmLwrrent;

    /*!
     * PWM requested to ACTIVE_PWM FAN_COOLER.
     */
    LwSFXP16_16             pwmRequested;

    /*!
     * Timestamp of the last moment when MAX Fan conditions were satisfied.
     * Units are multiple of 1024 nanoseconds.
     */
    LwU32                   maxFanTimestamp1024ns;

    /*!
     * Fan PWM source descriptor.
     */
    PWM_SOURCE_DESCRIPTOR  *pPwmSrcDesc;

    /*!
     * Fan PWM source.
     */
    RM_PMU_PMGR_PWM_SOURCE  pwmSource;

    /*!
     * Set while PWM Fan is in MAX-ed state.
     */
    LwBool                  bMaxFanActive;

    /*!
     * Set if Fan PWM needs to be ilwerted.
     */
    LwBool                  bPwmIlwerted;

    /*!
     * Reflects the state of Electrical Fan Speed (PWM) override.
     */
    LwBool                  bPwmSimActive;
};

/* ------------------------- Defines ---------------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
BoardObjGrpIfaceModel10ObjSet       (fanCoolerGrpIfaceModel10ObjSetImpl_ACTIVE_PWM)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommonConstruct", "fanCoolerGrpIfaceModel10ObjSetImpl_ACTIVE_PWM");
FanCoolerLoad           (fanCoolerActivePwmLoad);
FanCoolerPwmGet         (fanCoolerActivePwmPwmGet);
FanCoolerPwmSet         (fanCoolerActivePwmPwmSet);
BoardObjIfaceModel10GetStatus           (fanCoolerActivePwmIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommon", "fanCoolerActivePwmIfaceModel10GetStatus");

/* ------------------------ Include Derived Types -------------------------- */
#include "fan/fancooler_active_pwm_tach_corr.h"

#endif // FAN_COOLER_ACTIVE_PWM_H
