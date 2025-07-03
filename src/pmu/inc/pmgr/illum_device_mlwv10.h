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
 * @file    illum_device_mlwv10.h
 * @brief   ILLUM_DEVICE_MLWV10 related defines.
 *
 * @copydoc illum_device.h
 */

#ifndef ILLUM_DEVICE_MLWV10_H
#define ILLUM_DEVICE_MLWV10_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ OpenRTOS Includes ------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "pmgr/illum_device.h"
#include "pmgr/i2cdev.h"
#include "i2c/dev_gmac_mlwv10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct ILLUM_DEVICE_MLWV10 ILLUM_DEVICE_MLWV10;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * ILLUM_DEVICE_MLWV10 child class providing attributes of
 * ILLUM_DEVICE of type MLWV10.
 */
struct ILLUM_DEVICE_MLWV10
{
    /*!
     * ILLUM_DEVICE parent class.
     * Must be first element of the structure!
     */
    ILLUM_DEVICE    super;

    /*!
     * Pointer to the I2C device representing this device.
     */
    I2C_DEVICE *pI2cDev;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet     (illumDeviceGrpIfaceModel10ObjSetImpl_MLWV10)
    GCC_ATTRIB_SECTION("imem_pmgrLibIllumConstruct", "illumDeviceGrpIfaceModel10ObjSetImpl_MLWV10");

// ILLUM_DEVICE_MLWV10 interfaces.
IllumDeviceDataSet    (illumDeviceDataSet_MLWV10)
    GCC_ATTRIB_SECTION("imem_pmgrLibIllum", "illumDeviceDataSet_MLWV10");
IllumDeviceSync       (illumDeviceSync_MLWV10)
    GCC_ATTRIB_SECTION("imem_pmgrLibIllum", "illumDeviceSync_MLWV10");

/* ------------------------ Include Derived Types --------------------------- */

#endif // ILLUM_DEVICE_MLWV10_H
