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
 * @file  thrmdevice_i2c.c
 * @brief THERM_DEVICE specific to I2C.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/thrmdevice.h"
#include "pmu_objpmgr.h"
#include "pmgr/i2cdev.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Construct a THERM_DEVICE_I2C object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
thermDeviceGrpIfaceModel10ObjSetImpl_I2C
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_THERM_THERM_DEVICE_I2C_BOARDOBJ_SET *pSet =
        (RM_PMU_THERM_THERM_DEVICE_I2C_BOARDOBJ_SET *)pBoardObjDesc;
    THERM_DEVICE_I2C        *pDeviceI2C;
    FLCN_STATUS              status;

    // Construct and initialize parent-class object.
    status = thermDeviceGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        return status;
    }
    pDeviceI2C = (THERM_DEVICE_I2C *)*ppBoardObj;

    // Set member variables.
    pDeviceI2C->i2cDevIdx = pSet->i2cDevIdx;

    return status;
}

/* ------------------------- Private Functions ------------------------------ */
