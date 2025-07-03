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
 * @file    i2cdev_stm32l031g6u6tr.c
 * @brief   MLW BRIDGE I2C Device implementation
 * @extends I2C_DEVICE
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_i2capi.h"
#include "pmgr/i2cdev.h"
#include "flcnifi2c.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- I2C_DEVICE Interfaces -------------------------- */

/*!
 * Construct a new STM32L031G6U6TR I2C device object.
 *
 * @copydoc I2cDevConstruct
 */
FLCN_STATUS
i2cDevGrpIfaceModel10ObjSet_STM32L031G6U6TR
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    FLCN_STATUS status;

    // Call constructor for I2C_DEVICE super class
    status = i2cDevGrpIfaceModel10ObjSet_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto i2cDevGrpIfaceModel10ObjSet_STM32L031G6U6TR_exit;
    }

    // STM32L031G6U6TR is a Little-Endian Device.
    ((I2C_DEVICE *)*ppBoardObj)->bIsBigEndian   = LW_FALSE;
    ((I2C_DEVICE *)*ppBoardObj)->bBlockProtocol = LW_FALSE;
    ((I2C_DEVICE *)*ppBoardObj)->i2cFlags       =
        FLD_SET_DRF(_RM_FLCN, _I2C_FLAGS, _SPEED_MODE, _400KHZ,
                (((I2C_DEVICE *)*ppBoardObj)->i2cFlags));

i2cDevGrpIfaceModel10ObjSet_STM32L031G6U6TR_exit:
    return status;
}
