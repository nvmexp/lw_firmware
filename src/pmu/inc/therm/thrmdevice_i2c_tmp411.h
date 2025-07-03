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
 * @file thrmdevice_i2c_tmp411.h
 * @brief @copydoc thrmdevice_i2c_tmp411.c
 */

#ifndef THERMDEVICE_I2C_TMP411_H
#define THERMDEVICE_I2C_TMP411_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "therm/thrmdevice_i2c.h"

/* ------------------------- Macros ---------------------------------------- */
/* ------------------------- Datatypes ------------------------------------- */
typedef struct THERM_DEVICE_I2C_TMP411 THERM_DEVICE_I2C_TMP411;

/*!
 * Extends THERM_DEVICE providing attributes specific to I2C_TMP411 device.
 */
struct THERM_DEVICE_I2C_TMP411
{
    /*!
     * THERM_DEVICE_I2C super class. This should always be the first member!
     */
    THERM_DEVICE_I2C    super;
};

/* ------------------------- Defines ---------------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
ThermDeviceTempValueGet (thermDeviceTempValueGet_I2C_TMP411)
    GCC_ATTRIB_SECTION("imem_thermLibSensor2X", "thermDeviceTempValueGet_I2C_TMP411");

BoardObjGrpIfaceModel10ObjSet       (thermDeviceGrpIfaceModel10ObjSetImpl_I2C_TMP411)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "thermDeviceGrpIfaceModel10ObjSetImpl_I2C_TMP411");

/* ------------------------ Include Derived Types -------------------------- */

#endif // THERMDEVICE_I2C_TMP411_H
