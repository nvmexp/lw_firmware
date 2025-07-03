/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pwrdev_ina219.h
 * @copydoc pwrdev_ina219.c
 */

#ifndef PWRDEV_INA219_H
#define PWRDEV_INA219_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrdev.h"
#include "pmgr/i2cdev.h"
#include "i2c/dev_ina219.h"

/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_DEVICE_INA219 PWR_DEVICE_INA219;

struct PWR_DEVICE_INA219
{
    PWR_DEVICE  super;

    RM_PMU_PMGR_PWR_DEVICE_DESC_RSHUNT
                rShuntmOhm;       //<! Shunt resistor resistance (mOhm)
    LwU16       configuration;    //<! Configuration (register 0x0) value
    LwU16       calibration;      //<! Calibration (register 0x5) value

    LwUFXP20_12 powerCoeff;       //<! Coefficient for power callwlation
    LwUFXP20_12 lwrrentCoeff;     //<! Coefficient for current callwlation
};

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/* ------------------------- PWR_DEVICE Interfaces ------------------------- */

BoardObjGrpIfaceModel10ObjSet   (pwrDevGrpIfaceModel10ObjSetImpl_INA219)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrDevGrpIfaceModel10ObjSetImpl_INA219");
PwrDevLoad          (pwrDevLoad_INA219);
PwrDevGetVoltage    (pwrDevGetVoltage_INA219)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGetVoltage_INA219");
PwrDevGetLwrrent    (pwrDevGetLwrrent_INA219)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGetLwrrent_INA219");
PwrDevGetPower      (pwrDevGetPower_INA219)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevGetPower_INA219");
PwrDevTupleGet      (pwrDevTupleGet_INA219)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevTupleGet_INA219");

/* ------------------------- Debug Macros ---------------------------------- */

#endif // PWRDEV_INA219_H
