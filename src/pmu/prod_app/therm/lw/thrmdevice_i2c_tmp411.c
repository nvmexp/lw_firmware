/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  thrmdevice_i2c_tmp411.c
 * @brief Thermal Device code specific to TMP411 I2C devices (and compatible
 *        devices like ADM1032 and MAX6649)
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "i2c/dev_tmp411.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/thrmdevice.h"
#include "therm/objtherm.h"
#include "pmu_objpmgr.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * Read a TMP411 8-bit little-endian I2C Register. Does the necessary
 * endian-ness translation.
 *
 * @param[in]  pI2cDev  Pointer to the TMP411 I2C device object
 * @param[in]  reg      Register address to read
 * @param[out] pData    Pointer to write with the data read from the device
 *
 * @return Return value of @ref i2cDevReadReg8
 */
#define TMP411_READ(pI2cDev, reg, pData)                                      \
    i2cDevReadReg8(pI2cDev, ThermI2cQueue, reg, pData)

/* ------------------------- Public Functions ------------------------------- */

/*!
 * Construct a THERM_DEVICE_I2C_TMP411 object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
thermDeviceGrpIfaceModel10ObjSetImpl_I2C_TMP411
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    // Construct and initialize parent-class object.
    return thermDeviceGrpIfaceModel10ObjSetImpl_I2C(pModel10, ppBoardObj, size, pBoardObjDesc);
}

/*!
 * @copydoc ThermDeviceTempValueGet
 */
FLCN_STATUS
thermDeviceTempValueGet_I2C_TMP411
(
    THERM_DEVICE  *pDev,
    LwU8           thermDevProvIdx,
    LwTemp        *pLwTemp
)
{
    THERM_DEVICE_I2C_TMP411    *pTMP411 = (THERM_DEVICE_I2C_TMP411 *)pDev;
    I2C_DEVICE                 *pI2cDev;
    LwU8                        data    = 0;
    FLCN_STATUS                 status  = FLCN_ERR_NOT_SUPPORTED;

    pI2cDev = I2C_DEVICE_GET(pTMP411->super.i2cDevIdx);
    if (pI2cDev == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto thermDeviceTempValueGet_I2C_TMP411_exit;
    }

    // Provider specific handling.
    switch (thermDevProvIdx)
    {
        case LW2080_CTRL_THERMAL_THERM_DEVICE_I2C_TMP411_PROV_LOW_PRECISION_EXT:
        {
            status = TMP411_READ(pI2cDev,
                        LWRM_TMP411_RD_TEMP_VALUE_LOC_BYTE_HIGH, &data);
            break;
        }
        case LW2080_CTRL_THERMAL_THERM_DEVICE_I2C_TMP411_PROV_LOW_PRECISION_INT:
        {
            status = TMP411_READ(pI2cDev,
                        LWRM_TMP411_RD_TEMP_VALUE_EXT_BYTE_HIGH, &data);
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto thermDeviceTempValueGet_I2C_TMP411_exit;
    }

    // Colwert the temperature to F24.8 format.
    *pLwTemp = RM_PMU_CELSIUS_TO_LW_TEMP(data);

thermDeviceTempValueGet_I2C_TMP411_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
