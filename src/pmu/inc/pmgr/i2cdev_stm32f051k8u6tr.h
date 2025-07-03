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
 * @file    i2cdev_stm32f051k8u6tr.h
 * @copydoc i2cdev_stm32f051k8u6tr.c
 */

#ifndef I2CDEV_STM32F051K8U6TR_H
#define I2CDEV_STM32F051K8U6TR_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/i2cdev.h"

/* ------------------------- Types Definitions ----------------------------- */
/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/* ------------------------- PWR_DEVICE Interfaces ------------------------- */

BoardObjGrpIfaceModel10ObjSet (i2cDevGrpIfaceModel10ObjSet_STM32F051K8U6TR)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "i2cDevGrpIfaceModel10ObjSet_STM32F051K8U6TR");

/* ------------------------- Debug Macros ---------------------------------- */

#endif // I2CDEV_STM32F051K8U6TR_H
