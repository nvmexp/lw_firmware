/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   voltdev_pwm.h
 * @brief  @copydoc voltdev_pwm.c
 */

#ifndef VOLT_DEV_PWM_H
#define VOLT_DEV_PWM_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "volt/voltdev.h"
#include "lib/lib_pwm.h"

/* ------------------------- Macros ---------------------------------------- */
/*!
 * @brief   Colwenience macro for getting the vtable for the VOLT_DEVICE_PWM
 *          class type.
 *
 * @return  Pointer to the location of the VOLT_DEVICE_PWM class vtable.
 * @return  NULL if the vtable is not enabled in the build.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_DEVICE_PWM)
#define VOLT_VOLT_DEVICE_PWM_VTABLE() &VoltDevicePwmVirtualTable
extern BOARDOBJ_VIRTUAL_TABLE VoltDevicePwmVirtualTable;
#else
#define VOLT_VOLT_DEVICE_PWM_VTABLE() NULL
#endif

/* ------------------------- Datatypes ------------------------------------- */
typedef struct VOLT_DEVICE_PWM VOLT_DEVICE_PWM;

/*!
 * @brief Extends @ref VOLT_DEVICE providing attributes specific to PWM Serial VID
 * voltage devices.
 */
struct VOLT_DEVICE_PWM
{
    /*!
     * @brief @ref VOLT_DEVICE super-class. This should always be the first member!
     */
    VOLT_DEVICE             super;

    /*!
     * @brief PWM period
     */
    LwU32                   rawPeriod;

    /*!
     * @brief Base voltage is the y-intercept of liner equation y = mx + c
     */
    LwS32                   voltageBaseuV;

    /*!
     * @brief Offset constitutes the slope(m) when computing the target voltage in the
     * above equation. x represents the duty cycle for which the target
     * voltage(y) is callwlated which varies from 0 to period of the regulator
     * in discrete steps. The period and step size are obtained form VBIOS
     */
    LwS32                   voltageOffsetScaleuV;

    /*!
     * @brief Volt PWM source descriptor.
     */
    PWM_SOURCE_DESCRIPTOR  *pPwmSrcDesc;

    /*!
     * @brief PWM source for VOLT_DEVICE_PWM.
     */
    RM_PMU_PMGR_PWM_SOURCE  pwmSource;

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
    /*!
     * @brief GPIO pin indicating voltage rail enablement.
     */
    LwU8                    voltEnGpioPin;

    /*!
     * Gate state voltage in uV.
     */
    LwU32                   gateVoltageuV;
#endif // PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION)
};

/* ------------------------- Defines ---------------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
BoardObjGrpIfaceModel10ObjSet           (voltDeviceGrpIfaceModel10ObjSetImpl_PWM)
    GCC_ATTRIB_SECTION("imem_libVoltConstruct", "voltDeviceGrpIfaceModel10ObjSetImpl_PWM");

VoltDeviceGetVoltage        (voltDeviceGetVoltage_PWM);

VoltDeviceSetVoltage        (voltDeviceSetVoltage_PWM);

VoltDeviceConfigure         (voltDeviceConfigure_PWM);

VoltDeviceSanityCheck       (voltDeviceSanityCheck_PWM);

VoltDeviceLoadSanityCheck   (voltDeviceLoadSanityCheck_PWM);

/* ------------------------ Include Derived Types -------------------------- */

#endif // VOLT_DEV_PWM_H
