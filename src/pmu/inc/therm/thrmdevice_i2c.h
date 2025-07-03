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
 * @file thrmdevice_i2c.h
 * @brief @copydoc thrmdevice_i2c.c
 */

#ifndef THERMDEVICE_I2C_H
#define THERMDEVICE_I2C_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "therm/thrmdevice.h"

/* ------------------------- Macros ---------------------------------------- */
/* ------------------------- Datatypes ------------------------------------- */
typedef struct THERM_DEVICE_I2C THERM_DEVICE_I2C;

/*!
 * Extends THERM_DEVICE providing attributes specific to I2C device.
 */
struct THERM_DEVICE_I2C
{
    /*!
     * THERM_DEVICE super class. This should always be the first member!
     */
    THERM_DEVICE    super;

    /*!
     * Specifies the I2C Device Index in the DCB I2C Devices Table.
     */
    LwU8    i2cDevIdx;
};

/* ------------------------ Function Prototypes ---------------------------- */
BoardObjGrpIfaceModel10ObjSet   (thermDeviceGrpIfaceModel10ObjSetImpl_I2C)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "thermDeviceGrpIfaceModel10ObjSetImpl_I2C");

/* ------------------------ Include Derived Types -------------------------- */
#include "therm/thrmdevice_i2c_tmp411.h"

#endif // THERMDEVICE_I2C_H
