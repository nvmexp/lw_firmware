/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrdev_nct3933u.c
 * @brief NCT3933U Driver
 *
 * @extends    I2C_DEVICE
 * @implements PWR_DEVICE
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmgr.h"
#include "pmgr/pwrdev_nct3933u.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

/*!
 * Reads an NCT3933U 8-bit I2C Register.
 *
 * @param[in]  pNct3933u  Pointer to the NCT3933U device object
 * @param[in]  reg        Register address to read
 * @param[in]  pValue     Pointer to write the data read from the device
 *
 * @return Return value of @ref i2cRegRead8
 */
#define NCT3933U_READ(pNct3933u, reg, pValue)                                   \
    i2cDevReadReg8((pNct3933u)->super.pI2cDev, Pmgr.i2cQueue, reg, pValue)

/*!
 * Writes an NCT3933U 8-bit I2C Register.
 *
 * @param[in]  pNct3933u  Pointer to the NCT3933U device object
 * @param[in]  reg        Register address to write
 * @param[in]  value      Value to write to the device
 *
 * @return Return value of @ref i2cRegWrite8
 */
#define NCT3933U_WRITE(pNct3933u, reg, value)                                   \
    i2cDevWriteReg8((pNct3933u)->super.pI2cDev, Pmgr.i2cQueue, reg, value)

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- PWR_DEVICE Interfaces -------------------------- */

/*!
 * Construct a NCT3933U PWR_DEVICE object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrDevGrpIfaceModel10ObjSetImpl_NCT3933U
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    PWR_DEVICE_NCT3933U                               *pNct3933u;
    RM_PMU_PMGR_PWR_DEVICE_DESC_NCT3933U_BOARDOBJ_SET *pNct3933uSet =
        (RM_PMU_PMGR_PWR_DEVICE_DESC_NCT3933U_BOARDOBJ_SET *)pBoardObjSet;
    FLCN_STATUS                                        status;
    I2C_DEVICE                                        *pI2cDev;

    //
    // Move i2c device check before lwosCallocResidentType() in case we don't
    // waste memory when device check fails.
    //
    pI2cDev = I2C_DEVICE_GET(pNct3933uSet->i2cDevIdx);
    if (pI2cDev == NULL)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        goto pwrDevGrpIfaceModel10ObjSetImpl_NCT3933U_exit;
    }

    // Construct and initialize parent-class object.
    status = pwrDevGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        goto pwrDevGrpIfaceModel10ObjSetImpl_NCT3933U_exit;
    }
    pNct3933u = (PWR_DEVICE_NCT3933U *)*ppBoardObj;

    pNct3933u->super.pI2cDev = pI2cDev;

    // Set parameters
    pNct3933u->defaultLimitmA = pNct3933uSet->defaultLimitmA;
    pNct3933u->reg04val       = pNct3933uSet->reg04val;
    pNct3933u->reg05val       = pNct3933uSet->reg05val;
    pNct3933u->stepSizemA     = pNct3933uSet->stepSizemA;

pwrDevGrpIfaceModel10ObjSetImpl_NCT3933U_exit:
    return status;
}

/*!
 * NCT3933U power device implementation.
 *
 * @copydoc PwrDevLoad
 */
FLCN_STATUS
pwrDevLoad_NCT3933U
(
    PWR_DEVICE  *pDev
)
{
    PWR_DEVICE_NCT3933U *pNct3933u = (PWR_DEVICE_NCT3933U *)pDev;
    FLCN_STATUS          status = FLCN_OK;

    // Program Control Register CR04h and CR05h in NCT3933U
    status = NCT3933U_WRITE(pNct3933u, LWRM_NCT3933U_CR04H,
                            pNct3933u->reg04val);
    if (status != FLCN_OK)
    {
        return status;
    }

    status = NCT3933U_WRITE(pNct3933u, LWRM_NCT3933U_CR05H,
                            pNct3933u->reg05val);

    return status;
}

/*!
 * NCT3933U power device implementation.
 *
 * @copydoc PwrDevSetLimit
 */
FLCN_STATUS
pwrDevSetLimit_NCT3933U
(
    PWR_DEVICE *pDev,
    LwU8        provIdx,
    LwU8        limitIdx,
    LwU8        limitUnit,
    LwU32       limitValue
)
{
    PWR_DEVICE_NCT3933U *pNct3933u     = (PWR_DEVICE_NCT3933U *)pDev;
    LwU32                offsetStep    = 0;
    LwU8                 limitRegister = 0;
    FLCN_STATUS          status        = FLCN_OK;

    // Sanity check the limitIdx requested
    PMU_HALT_COND(limitIdx < pwrDevThresholdNumGet_NCT3933U(pDev));
    PMU_HALT_COND(limitUnit == LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_LWRRENT_MA);

    if (limitValue >= pNct3933u->defaultLimitmA)
    {
        // Set the sign bit in the limit register to positive.
        limitRegister = DRF_DEF(RM_NCT3933U, _LIMIT, _SIGN, _POSITIVE);

        //
        // We need to truncate towards negative infinity. For positive numbers
        // division which truncates towards 0 is same as truncating towards
        // negative infinity.
        //
        offsetStep =
            (limitValue - pNct3933u->defaultLimitmA) / pNct3933u->stepSizemA;
    }
    else
    {
        // Set the sign bit in the limit register to negative.
        limitRegister = DRF_DEF(RM_NCT3933U, _LIMIT, _SIGN, _NEGATIVE);

        //
        // We need to truncate towards negative infinity so doing a round up
        // in unsigned and sign bit will take care of negating the quotient.
        // Eg: -3/2 = -2 =>
        //    LW_UNSIGNED_DIV_CEIL(3, 2) = 2
        //    2 * -1 = -2
        //
        offsetStep = LW_UNSIGNED_DIV_CEIL(
            (pNct3933u->defaultLimitmA - limitValue), pNct3933u->stepSizemA);
    }

    //
    // Offset step is the absolute value of the limit so clipping to _MAX (127)
    // to keep limit in the range -127:127.
    //
    offsetStep = LW_MIN(offsetStep, LWRM_NCT3933U_LIMIT_ABS_VALUE_MAX);

    // Update the value of offset step in the limitRegister
    limitRegister = FLD_SET_DRF_NUM(RM_NCT3933U, _LIMIT, _ABS_VALUE, offsetStep,
                                    limitRegister);

    // Write the value of limitRegister to the device output control register.
    status = NCT3933U_WRITE(pNct3933u, LWRM_NCT3933U_CR(provIdx),
                            limitRegister);

    return status;
}

/*!
 * NCT3933U power device implementation.
 *
 * @copydoc PwrDevGetLimit
 */
FLCN_STATUS
pwrDevGetLimit_NCT3933U
(
    PWR_DEVICE            *pDev,
    LwU8                   provIdx,
    LwU8                   limitIdx,
    RM_PMU_BOARDOBJ       *pBuf
)
{
    PWR_DEVICE_NCT3933U *pNct3933u     = (PWR_DEVICE_NCT3933U *)pDev;
    LwU8                 limitRegister = 0;
    LwS32                offsetmA      = 0;
    FLCN_STATUS          status        = FLCN_OK;
    RM_PMU_PMGR_PWR_DEVICE_BOARDOBJ_GET_STATUS *pStatus =
        (RM_PMU_PMGR_PWR_DEVICE_BOARDOBJ_GET_STATUS *)pBuf;

    // Sanity check the limitIdx requested
    PMU_HALT_COND(limitIdx < pwrDevThresholdNumGet_NCT3933U(pDev));

    status = NCT3933U_READ(pNct3933u, LWRM_NCT3933U_CR(provIdx),
                           &limitRegister);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pwrDevGetLimit_NCT3933U_exit;
    }

    // Copy in the absolute value of limitRegister
    offsetmA = (LwS32)DRF_VAL(RM_NCT3933U, _LIMIT, _ABS_VALUE, limitRegister);

    // Negate offsetmA based on the sign-bit of limitRegister
    if (FLD_TEST_DRF(RM_NCT3933U, _LIMIT, _SIGN, _NEGATIVE, limitRegister))
    {
        offsetmA *= -1;
    }

    // Multiply by the step size in mA to get the offset in mA
    offsetmA *= pNct3933u->stepSizemA;

    // Ensure that the limit is positive
    pStatus->providers[provIdx].thresholds[limitIdx] =
        (LwU32)LW_MAX(pNct3933u->defaultLimitmA + offsetmA, 0);

pwrDevGetLimit_NCT3933U_exit:
    return status;
}

