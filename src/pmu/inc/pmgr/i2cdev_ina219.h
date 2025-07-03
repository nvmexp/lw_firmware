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
 * @file    i2cdev_ina219.h
 * @copydoc i2cdev_ina219.c
 */

#ifndef I2CDEV_INA219_H
#define I2CDEV_INA219_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
/* ------------------------- Types Definitions ----------------------------- */
/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/* ------------------------- PWR_DEVICE Interfaces ------------------------- */

BoardObjGrpIfaceModel10ObjSet (i2cDevGrpIfaceModel10ObjSet_INA219)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "i2cDevGrpIfaceModel10ObjSet_INA219");

/* ------------------------- Debug Macros ---------------------------------- */

#endif // I2CDEV_INA219_H
