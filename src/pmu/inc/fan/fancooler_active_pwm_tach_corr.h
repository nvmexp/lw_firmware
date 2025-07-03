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
 * @file fancooler_active_pwm_tach_corr.h
 * @brief @copydoc fancooler_active_pwm_tach_corr.c
 */

#ifndef FAN_COOLER_ACTIVE_PWM_TACH_CORR_H
#define FAN_COOLER_ACTIVE_PWM_TACH_CORR_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "fan/fancooler_active_pwm.h"

/* ------------------------- Macros ---------------------------------------- */
#define fanCoolerActivePwmTachCorrRpmGet(pCooler, pRpm) \
    fanCoolerActiveRpmGet((pCooler), (pRpm))
#define fanCoolerActivePwmTachCorrPwmGet(pCooler, bActual, pPwm) \
    fanCoolerActivePwmPwmGet((pCooler), (bActual), (pPwm))

/* ------------------------- Datatypes ------------------------------------- */
typedef struct FAN_COOLER_ACTIVE_PWM_TACH_CORR FAN_COOLER_ACTIVE_PWM_TACH_CORR;

/*!
 * Extends FAN_COOLER providing attributes specific to ACTIVE_PWM_TACH_CORR cooler.
 */
struct FAN_COOLER_ACTIVE_PWM_TACH_CORR
{
    /*!
     * FAN_COOLER_ACTIVE_PWM super class. This should always be the first member!
     */
    FAN_COOLER_ACTIVE_PWM   super;

    /*!
     * Parameters obtained from RM.
     */

    /*!
     * Tachometer Feedback Proportional Gain.
     */
    LwSFXP16_16             propGain;

    /*!
     * An absolute offset (in PWM percents) from actual/measured PWM speed, used
     * on systems exploiting fan-sharing, preventing tachometer driven policies
     * to reduce PWM speed to @ref pwmMin (see bug 1398842).
     */
    LwSFXP16_16             pwmFloorLimitOffset;

    /*!
     * Reflects the state of RPM override.
     */
    LwBool                  bRpmSimActive;

    /*!
     * Count of control iterations that are allowed to ignore zero RPM        .
     * readback. Used as WAR for glitches in fan control during startup.
     */
    LwU8                    tachWarCycleCount;

    /*!
     * Override value for RPM settings.
     * Applicable only when @ref bRpmSimActive is LW_TRUE.
     */
    LwU32                   rpmSim;

    /*!
     * PMU specific parameters.
     */

    /*!
     * Last RPM reading used for tach correction.
     */
    LwU32                   rpmLast;

    /*!
     * Target RPM requested by tach correction.
     */
    LwU32                   rpmTarget;

    /*!
     * Actual measured PWM used for applying floor limit offset on Gemini fan
     * sharing designs.
     */
    LwSFXP16_16             pwmActual;
};

/* ------------------------- Defines ---------------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
BoardObjGrpIfaceModel10ObjSet   (fanCoolerGrpIfaceModel10ObjSetImpl_ACTIVE_PWM_TACH_CORR)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommonConstruct", "fanCoolerGrpIfaceModel10ObjSetImpl_ACTIVE_PWM_TACH_CORR");
BoardObjIfaceModel10GetStatus       (fanCoolerActivePwmTachCorrIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommon", "fanCoolerActivePwmTachCorrIfaceModel10GetStatus");
FanCoolerLoad       (fanCoolerActivePwmTachCorrLoad);
FanCoolerRpmSet     (fanCoolerActivePwmTachCorrRpmSet);
FanCoolerPwmSet     (fanCoolerActivePwmTachCorrPwmSet);

/* ------------------------ Include Derived Types -------------------------- */

#endif // FAN_COOLER_ACTIVE_PWM_TACH_CORR_H
