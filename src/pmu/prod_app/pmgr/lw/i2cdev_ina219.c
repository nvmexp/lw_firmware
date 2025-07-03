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
 * @file    i2cdev_ina219.c
 * @brief   INA219 I2C Driver
 * @extends I2C_DEVICE
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmgr/i2cdev.h"
#include "pmu_i2capi.h"
#include "flcnifi2c.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- I2C_DEVICE Interfaces -------------------------- */

/*!
 * Construct a new INA219 I2C device object.
 *
 * @copydoc I2cDevConstruct
 */
FLCN_STATUS
i2cDevGrpIfaceModel10ObjSet_INA219
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    FLCN_STATUS status;

    // call constructor for I2C_DEVICE super class
    status = i2cDevGrpIfaceModel10ObjSet_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto i2cDevGrpIfaceModel10ObjSet_INA219_exit;
    }

    // INA219 is a Big-Endian Device
    ((I2C_DEVICE *)*ppBoardObj)->bIsBigEndian = LW_TRUE;

i2cDevGrpIfaceModel10ObjSet_INA219_exit:
    return status;
}
