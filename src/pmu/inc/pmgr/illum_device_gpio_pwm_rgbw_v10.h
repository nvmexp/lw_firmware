/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    illum_device_gpio_pwm_rgbw_v10.h
 * @brief   ILLUM_DEVICE_GPIO_PWM_RGBW_V10 related defines.
 *
 * @copydoc illum_device.h
 */

#ifndef ILLUM_DEVICE_GPIO_PWM_RGBW_V10_H
#define ILLUM_DEVICE_GPIO_PWM_RGBW_V10_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ OpenRTOS Includes ------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "pmgr/illum_device.h"
#include "lib/lib_pwm.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct ILLUM_DEVICE_GPIO_PWM_RGBW_V10 ILLUM_DEVICE_GPIO_PWM_RGBW_V10;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * ILLUM_DEVICE_GPIO_PWM_RGBW_V10 child class providing attributes of
 * ILLUM_DEVICE of type GPIO_PWM_RGBW_V10.
 */
struct ILLUM_DEVICE_GPIO_PWM_RGBW_V10
{
    /*!
     * ILLUM_DEVICE parent class.
     * Must be first element of the structure!
     */
    ILLUM_DEVICE    super;

    /*!
     * Red drive GPIO pin.
     */
    LwU8  gpioPinRed;

    /*!
     * Green drive GPIO pin.
     */
    LwU8  gpioPinGreen;

    /*!
     * Blue drive GPIO pin.
     */
    LwU8  gpioPinBlue;

    /*!
     * White drive GPIO pin.
     */
    LwU8  gpioPinWhite;

    /*!
     * PWM source driving the red GPIO pin.
     */
    RM_PMU_PMGR_PWM_SOURCE
          pwmSourceRed;

    /*!
     * PWM source driving the green GPIO pin.
     */
    RM_PMU_PMGR_PWM_SOURCE
          pwmSourceGreen;

    /*!
     * PWM source driving the blue GPIO pin.
     */
    RM_PMU_PMGR_PWM_SOURCE
          pwmSourceBlue;

    /*!
     * PWM source driving the white GPIO pin.
     */
    RM_PMU_PMGR_PWM_SOURCE
          pwmSourceWhite;

    /*!
     * PWM period. This is common to all R,G,B and W.
     */
    LwU32 rawPeriod;

    /*!
     * @brief Red color source descriptor.
     */
    PWM_SOURCE_DESCRIPTOR  *pPwmSrcDescRed;

    /*!
     * @brief Green color source descriptor.
     */
    PWM_SOURCE_DESCRIPTOR  *pPwmSrcDescGreen;

    /*!
     * @brief Blue color source descriptor.
     */
    PWM_SOURCE_DESCRIPTOR  *pPwmSrcDescBlue;

    /*!
     * @brief White color source descriptor.
     */
    PWM_SOURCE_DESCRIPTOR  *pPwmSrcDescWhite;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet     (illumDeviceGrpIfaceModel10ObjSetImpl_GPIO_PWM_RGBW_V10)
    GCC_ATTRIB_SECTION("imem_pmgrLibIllumConstruct", "illumDeviceGrpIfaceModel10ObjSetImpl_GPIO_PWM_RGBW_V10");

// ILLUM_DEVICE_GPIO_PWM_RGBW_V10 interfaces.
IllumDeviceDataSet    (illumDeviceDataSet_GPIO_PWM_RGBW_V10)
    GCC_ATTRIB_SECTION("imem_pmgrLibIllum", "illumDeviceDataSet_GPIO_PWM_RGBW_V10");
IllumDeviceSync       (illumDeviceSync_GPIO_PWM_RGBW_V10)
    GCC_ATTRIB_SECTION("imem_pmgrLibIllum", "illumDeviceSync_GPIO_PWM_RGBW_V10");

/* ------------------------ Include Derived Types --------------------------- */

#endif // ILLUM_DEVICE_GPIO_PWM_RGBW_V10_H
