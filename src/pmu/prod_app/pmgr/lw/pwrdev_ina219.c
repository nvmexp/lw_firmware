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
 * @file  pwrdev_ina219.c
 * @brief INA219 Driver
 *
 * @implements PWR_DEVICE
 *
 * Note - Only supports/implements single PWR_DEVICE Provider - index 0.
 * Provider index parameter will be ignored in all interfaces.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmgr.h"
#include "pmgr/pwrdev_ina219.h"
#include "dbgprintf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

/*!
 * Read a INA219 16-bit little-endian I2C Register. Does the necessary
 * endian-ness translation.
 *
 * @param[in]  pINA219   Pointer to the INA219 device object
 * @param[in]  reg       Register address to read
 * @param[out] pValue    Pointer to write with the data read from the device
 *
 * @return Return value of @ref i2cRegRead16
 */
#define INA219_READ(pINA219, reg, pValue)                                     \
    i2cDevReadReg16((pINA219)->super.pI2cDev, Pmgr.i2cQueue, reg, pValue)

/*!
 * Writes a INA219 16-bit little-endian I2C Register. Does the necessary
 * endian-ness translation.
 *
 * @param[in]  pINA219   Pointer to the INA219 device object
 * @param[in]  reg       Register address to write
 * @param[in]  value     Pointer to write with the data read from the device
 *
 * @return Return value of @ref i2cRegWrite16
 */
#define INA219_WRITE(pINA219, reg, value)                                     \
    i2cDevWriteReg16((pINA219)->super.pI2cDev, Pmgr.i2cQueue, reg, value)

/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS s_pwrDevIsTamperingDetected_INA219(PWR_DEVICE_INA219 *pINA219, LwBool *pBDetected)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "s_pwrDevIsTamperingDetected_INA219");
static FLCN_STATUS s_pwrDevTamperDetectRecover_INA219(PWR_DEVICE *pDev)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "s_pwrDevTamperDetectRecover_INA219");

/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- PWR_DEVICE Interfaces -------------------------- */

/*!
 * Construct a INA219 PWR_DEVICE object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrDevGrpIfaceModel10ObjSetImpl_INA219
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    PWR_DEVICE_INA219                               *pINA219;
    RM_PMU_PMGR_PWR_DEVICE_DESC_INA219_BOARDOBJ_SET *pIna219Set =
        (RM_PMU_PMGR_PWR_DEVICE_DESC_INA219_BOARDOBJ_SET *)pBoardObjDesc;
    FLCN_STATUS                                      status;

    // Construct and initialize parent-class object.
    status = pwrDevGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto pwrDevGrpIfaceModel10ObjSetImpl_INA219_exit;
    }
    pINA219 = (PWR_DEVICE_INA219 *)*ppBoardObj;

    pINA219->super.pI2cDev = I2C_DEVICE_GET(pIna219Set->i2cDevIdx);
    if (pINA219->super.pI2cDev == NULL)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        goto pwrDevGrpIfaceModel10ObjSetImpl_INA219_exit;
    }

    // Copy in the three main parameters
    pINA219->rShuntmOhm    = pIna219Set->rShuntmOhm;
    pINA219->configuration = pIna219Set->configuration;
    pINA219->calibration   = pIna219Set->calibration;
    pINA219->powerCoeff    = pIna219Set->powerCoeff;
    pINA219->lwrrentCoeff  = pIna219Set->lwrrentCoeff;

pwrDevGrpIfaceModel10ObjSetImpl_INA219_exit:
    return status;
}

/*!
 * Program all device context (such as the configuration and calibration
 * values). The programming sequence is aborted upon the first failure (if
 * any).
 *
 * @copydoc PwrDevLoad
 */
FLCN_STATUS
pwrDevLoad_INA219
(
    PWR_DEVICE  *pDev
)
{
    PWR_DEVICE_INA219 *pINA219 = (PWR_DEVICE_INA219 *)pDev;
    FLCN_STATUS        status;

    // Configuration
    status = INA219_WRITE(pINA219, LWRM_INA219_CONFIGURATION,
                          pINA219->configuration);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Calibration
    status = INA219_WRITE(pINA219, LWRM_INA219_CALIBRATION,
                          pINA219->calibration);
    return status;
}

/*!
 * Current is returned in units of milli-amps.
 *
 * Will overflow when current > 1048576 mA
 *
 * @copydoc PwrDevGetLwrrent
 */
FLCN_STATUS
pwrDevGetLwrrent_INA219
(
    PWR_DEVICE  *pDev,
    LwU8         provIdx,
    LwU32       *pLwrrentmA
)
{
    PWR_DEVICE_INA219 *pINA219 = (PWR_DEVICE_INA219 *)pDev;
    LwU16              reg;
    LwU32              tmp;
    FLCN_STATUS        status;

    status = INA219_READ(pINA219, LWRM_INA219_LWRRENT, &reg);
    if (status != FLCN_OK)
    {
        return status;
    }

    //
    // The math here holds so long as our current is <= 1048576 mA (~1 kA),
    // otherwise we'll overflow 20 integer bits. This is a safe assumption
    // for one power rail.
    //
    // 20.12 * 16.0 => 20.12
    //
    tmp = reg * pINA219->lwrrentCoeff;

    // 20.12 >> 12 => 20.0
    *pLwrrentmA = LW_TYPES_UFXP_X_Y_TO_U32_ROUNDED(20, 12, tmp);

//    DBG_PRINTF_PMGR(("INAC: r: 0x%04x, t: 0x%08x, c: %d\n", reg, tmp, *pLwrrentmA, 0));

    return FLCN_OK;
}

/*!
 * Voltage is returned in units of micro-volts.
 *
 * @copydoc PwrDevGetVoltage
 */
FLCN_STATUS
pwrDevGetVoltage_INA219
(
    PWR_DEVICE  *pDev,
    LwU8         provIdx,
    LwU32       *pVoltageuV
)
{
    PWR_DEVICE_INA219 *pINA219 = (PWR_DEVICE_INA219 *)pDev;
    LwU16              reg;
    FLCN_STATUS        status;

    status = INA219_READ(pINA219, LWRM_INA219_BUS_VOLTAGE, &reg);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Scale from mW register -> uV interface
    *pVoltageuV = DRF_VAL(RM_INA219, _BUS_VOLTAGE, _VALUE, reg) *
                    LWRM_INA219_VOLTAGE_SCALE * 1000;

    return FLCN_OK;
}

/*!
 * Power is returned in units of milli-watts
 *
 * Will overflow when power > 1048576 mW
 *
 * @copydoc PwrDevGetPower
 */
FLCN_STATUS
pwrDevGetPower_INA219
(
    PWR_DEVICE  *pDev,
    LwU8         provIdx,
    LwU32       *pPowermW
)
{
    PWR_DEVICE_INA219 *pINA219 = (PWR_DEVICE_INA219 *)pDev;
    LwU32              tmp;
    LwU16              reg = 0;
    FLCN_STATUS        status;

    status = s_pwrDevTamperDetectRecover_INA219(pDev);
    if (status != FLCN_OK)
    {
        return status;
    }

    status = INA219_READ(pINA219, LWRM_INA219_POWER, &reg);
    if (status != FLCN_OK)
    {
        return status;
    }

    //
    // The math here holds so long as our power is <= 1048576 mW (~1 kW),
    // otherwise we'll overflow 20 integer bits. This is a safe assumption
    // for one power rail.
    //
    // 20.12 * 16.0 => 20.12
    //
    tmp = reg * pINA219->powerCoeff;

    // 20.12 >> 12 => 20.0
    *pPowermW = LW_TYPES_UFXP_X_Y_TO_U32_ROUNDED(20, 12, tmp);

    DBG_PRINTF_PMGR(("I0%02xP: r: 0x%04x, t: 0x%08x, p: %d\n",
        pINA219->super.pI2cDev->i2cAddress, reg, tmp, *pPowermW));

    return FLCN_OK;
}

/*!
 * INA219 power device implementation.
 *
 * @copydoc PwrDevTupleGet
 */
FLCN_STATUS
pwrDevTupleGet_INA219
(
    PWR_DEVICE                 *pDev,
    LwU8                        provIdx,
    LW2080_CTRL_PMGR_PWR_TUPLE *pTuple
)
{
    FLCN_STATUS status;
    LwU32       voltagemV;

    status = s_pwrDevTamperDetectRecover_INA219(pDev);
    if (status != FLCN_OK)
    {
        return status;
    }

    status = pwrDevGetVoltage_INA219(pDev, provIdx, &(pTuple->voltuV));
    if (status != FLCN_OK)
    {
        return status;
    }

    // Get current
    status = pwrDevGetLwrrent_INA219(pDev, provIdx, &(pTuple->lwrrmA));
    if (status != FLCN_OK)
    {
        return status;
    }

    voltagemV      = LW_UNSIGNED_ROUNDED_DIV(pTuple->voltuV, 1000);
    pTuple->pwrmW = voltagemV * (pTuple->lwrrmA) / 1000;

    return FLCN_OK;
}

/* ------------------------ Static Functions -------------------------------- */

/*!
 * @brief Perform consistency checking to test for device tampering.
 *
 * To detect for possible external device-tampering, read-back the
 * configuration and calibration registers and compare the values with what is
 * expected to be programmed.
 *
 * @param[in]   pINA219     Pointer to the INA219 device object
 * @param[out]  pBDetected
 *     Written when LW_TRUE if tampering is detected; LW_FALSE otherwise. It
 *     may not always be possible to detect tampering based on the state of
 *     the I2C bus. The return status of this function will indicate if this
 *     value can be trusted. See return information for more information.
 *
 * @return  FLCN_OK
 *     Indicates that the tampering detection was successful and that the
 *     boolean value in pBDetected can be used by the caller to know if
 *     tampering was detected (or not detected).
 *
 * @return  other
 *     Bubbles up errors from @ref i2cDevReadReg16. All errors indicate that
 *     the detection was not successful and that pBDetected cannot be used.
 */
static FLCN_STATUS
s_pwrDevIsTamperingDetected_INA219
(
    PWR_DEVICE_INA219  *pINA219,
    LwBool             *pBDetected
)
{
    LwU16      reg = 0;
    FLCN_STATUS status;

    // validate configuration register
    status = INA219_READ(pINA219, LWRM_INA219_CONFIGURATION, &reg);
    if (status != FLCN_OK)
    {
        return status;
    }
    if (reg != pINA219->configuration)
    {
        *pBDetected = LW_TRUE;
        return FLCN_OK;
    }

    // validate calibration register
    status = INA219_READ(pINA219, LWRM_INA219_CALIBRATION, &reg);
    if (status != FLCN_OK)
    {
        return status;
    }
    if (reg != pINA219->calibration)
    {
        *pBDetected = LW_TRUE;
        return FLCN_OK;
    }

    *pBDetected = LW_FALSE;
    return FLCN_OK;
}

/*!
 * @brief Perform consistency checking to test for device tampering
 * and recovery.
 *
 * To detect for possible external device-tampering, read-back the
 * configuration and calibration registers and compare the values with what is
 * expected to be programmed. If inconsistency detected, recover from it.
 *
 * @param[in]  pDev  Pointer to power device object.
 *
 * @return  FLCN_OK
 *     Indicates that the tampering detection and recovery was successful.
 *
 * @return FLCN_ERR_PWR_DEVICE_TAMPERED
 *     Indicates that power device has been tampered.
 *
 * @return other
 *     Bubbles up errors from @ref i2cDevReadReg16. All errors indicate that
 *     the detection was not successful and that pBDetected cannot be used.
 */
static FLCN_STATUS
s_pwrDevTamperDetectRecover_INA219
(
    PWR_DEVICE  *pDev
)
{
    PWR_DEVICE_INA219  *pINA219 = (PWR_DEVICE_INA219 *)pDev;
    FLCN_STATUS         status;
    LwBool              bTamperingDetected = LW_FALSE;

    status = s_pwrDevIsTamperingDetected_INA219(pINA219, &bTamperingDetected);
    if (status != FLCN_OK)
    {
        return status;
    }

    if (bTamperingDetected)
    {
        (void)pwrDevLoad_INA219(pDev);
        return FLCN_ERR_PWR_DEVICE_TAMPERED;
    }

    return FLCN_OK;
}
