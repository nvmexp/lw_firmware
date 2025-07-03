/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrdev_ina3221.c
 * @brief INA3221 Driver
 *
 * @extends    I2C_DEVICE
 * @implements PWR_DEVICE
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmgr.h"
#include "pmgr/pwrdev_ina3221.h"
#include "dbgprintf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

/*!
 * Read a INA3221 16-bit little-endian I2C Register. Does the necessary
 * endian-ness translation.
 *
 * @param[in]  pINA3221  Pointer to the INA3221 device object
 * @param[in]  reg       Register address to read
 * @param[out] pValue    Pointer to write with the data read from the device
 *
 * @return Return value of @ref i2cRegRead16
 */
#define INA3221_READ(pINA3221, reg, pValue)                                    \
    i2cDevReadReg16((pINA3221)->super.pI2cDev, Pmgr.i2cQueue, reg, pValue)

/*!
 * Writes a INA3221 16-bit little-endian I2C Register. Does the necessary
 * endian-ness translation.
 *
 * @param[in]  pINA3221  Pointer to the INA3221 device object
 * @param[in]  reg       Register address to write
 * @param[in]  value     Pointer to write with the data read from the device
 *
 * @return Return value of @ref i2cRegWrite16
 */
#define INA3221_WRITE(pINA3221, reg, value)                                    \
    i2cDevWriteReg16((pINA3221)->super.pI2cDev, Pmgr.i2cQueue, reg, value)

/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS s_pwrDevIsTamperingDetected_INA3221(PWR_DEVICE_INA3221 *pINA3221, LwBool *pBDetected);

/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- PWR_DEVICE Interfaces -------------------------- */

/*!
 * Construct a INA3221 PWR_DEVICE object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrDevGrpIfaceModel10ObjSetImpl_INA3221
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    PWR_DEVICE_INA3221                               *pINA3221;
    RM_PMU_PMGR_PWR_DEVICE_DESC_INA3221_BOARDOBJ_SET *pIna3221Set =
        (RM_PMU_PMGR_PWR_DEVICE_DESC_INA3221_BOARDOBJ_SET *)pBoardObjDesc;
    FLCN_STATUS                                       status;
    LwU32                                             i;
    LwU16                                             summationBits;
    LwU16                                             highestBitIdx;
    I2C_DEVICE                                       *pI2cDev;

    //
    // Move i2c device check before lwosCallocResidentType() in case we don't
    // waste memory when device check fails.
    //
    pI2cDev = I2C_DEVICE_GET(pIna3221Set->i2cDevIdx);
    if (pI2cDev == NULL)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        goto pwrDevGrpIfaceModel10ObjSetImpl_INA3221_exit;
    }

    // Construct and initialize parent-class object.
    status = pwrDevGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto pwrDevGrpIfaceModel10ObjSetImpl_INA3221_exit;
    }
    pINA3221 = (PWR_DEVICE_INA3221 *)*ppBoardObj;

    pINA3221->super.pI2cDev = pI2cDev;

    // Set parameters
    pINA3221->configuration = pIna3221Set->configuration;
    for (i = 0; i < RM_PMU_PMGR_PWR_DEVICE_INA3221_CH_NUM; i++)
    {
        pINA3221->rShuntmOhm[i] = pIna3221Set->rShuntmOhm[i];
    }
    pINA3221->maskEnable   = pIna3221Set->maskEnable;

    //
    // Sanity check that the value of Correction Factor M as received from RM
    // is non-zero.
    //
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pIna3221Set->lwrrCorrectM !=
            LW_TYPES_U32_TO_UFXP_X_Y(4, 12, 0)),
        FLCN_ERR_ILWALID_STATE,
        pwrDevGrpIfaceModel10ObjSetImpl_INA3221_exit);

    pINA3221->lwrrCorrectM = pIna3221Set->lwrrCorrectM;
    pINA3221->lwrrCorrectB = pIna3221Set->lwrrCorrectB;

    // Initialize critical and warning alert limit data structure.
    pINA3221->limit[LWRM_INA3221_LIMIT_IDX_CRITICAL].limitIdx =
        LWRM_INA3221_LIMIT_IDX_CRITICAL;
    pINA3221->limit[LWRM_INA3221_LIMIT_IDX_WARNING].limitIdx  =
        LWRM_INA3221_LIMIT_IDX_WARNING;

    //
    // ccs-TODO: Lwrrently we only support violation count on CRITICAL ALERT
    // LIMIT since most of work is using this limit now. Will provide WARNING
    // ALERT LIMIT violation count in very near future.
    // VBIOS has only one eventMask. Needs to expand to 2.
    //
    pINA3221->limit[LWRM_INA3221_LIMIT_IDX_CRITICAL].eventMask =
        pIna3221Set->eventMask;

    //
    // In case of provIdx == RM_PMU_PMGR_PWR_DEVICE_INA3221_PROV_SUM
    // we require rshuntohm and bus voltage of first enabled channel
    // to callwlate power and current. We also need number of enabled
    // channels to callwlate current. Hence caching this information
    // here to avoid duplication of code.
    //
    summationBits = (pINA3221->maskEnable &
                                 INA3221_SUMMATION_CONTROL_BITMASK);
    highestBitIdx = summationBits;

    //
    // Get the number of channels which are enabled to
    // to fill the Shunt Voltage Sum register
    //
    NUMSETBITS_32(summationBits);
    pINA3221->channelsNum = (LwU8)summationBits;

    //
    // Get the first channel which is enabled to
    // fill the Shunt Voltage Sum register
    //
    HIGHESTBITIDX_32(highestBitIdx);
    pINA3221->highestProvIdx = (LwU8)
                 (DRF_BASE(LWRM_INA3221_MASKENABLE_SUMMATION_CONTROL(0))
                     - highestBitIdx);

pwrDevGrpIfaceModel10ObjSetImpl_INA3221_exit:
    return status;
}

/*!
 * INA3221 power device implementation.
 *
 * @copydoc PwrDevLoad
 */
FLCN_STATUS
pwrDevLoad_INA3221
(
    PWR_DEVICE  *pDev
)
{
    PWR_DEVICE_INA3221 *pINA3221 = (PWR_DEVICE_INA3221 *)pDev;
    FLCN_STATUS         status;

    // Initialize tamper detect provider index to UNINITIALIZED.
    pINA3221->tamperIdx = PWR_DEVICE_INA3221_TAMPER_IDX_UNINITIALIZED;
    pINA3221->bTamperingDetected = LW_FALSE;

    // Configuration
    status = INA3221_WRITE(pINA3221, LWRM_INA3221_CONFIGURATION,
                           pINA3221->configuration);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Mask / Enable
    status = INA3221_WRITE(pINA3221, LWRM_INA3221_MASKENABLE,
                           pINA3221->maskEnable);
    return status;
}

/*!
 * INA3221 power device implementation.
 *
 * @copydoc PwrDevGetLwrrent
 */
FLCN_STATUS
pwrDevGetLwrrent_INA3221
(
    PWR_DEVICE  *pDev,
    LwU8         provIdx,
    LwU32       *pLwrrentmA
)
{
    PWR_DEVICE_INA3221 *pINA3221 = (PWR_DEVICE_INA3221 *)pDev;
    LwU16               reg;
    LwU32               tmp;
    LwU32               channelNum = 0;
    LwS64               lwrrentmAS64;
    LwS64               lwrrentmAS52_12;
    LwS64               lwrrentmACorrectedMS52_12;
    LwS64               lwrrentmACorrectedBS52_12;
    FLCN_STATUS         status;
    RM_PMU_PMGR_PWR_DEVICE_DESC_RSHUNT
                        rShuntmOhm;

    // Handle summation channel.
    if (provIdx == RM_PMU_PMGR_PWR_DEVICE_INA3221_PROV_SUM)
    {
        status = INA3221_READ(pINA3221, LWRM_INA3221_SHUNT_VOL_SUM, &reg);
        if (status != FLCN_OK)
        {
            return status;
        }
        channelNum = pINA3221->channelsNum;

        // check if we have enabled any channel.
        if (channelNum == 0)
        {
            return FLCN_ERR_NOT_SUPPORTED;
        }
        rShuntmOhm = pINA3221->rShuntmOhm[pINA3221->highestProvIdx];

        tmp = DRF_VAL(RM_INA3221, _SHUNT_VOL_SUM, _VALUE, reg);
        // check for negative value.
        if FLD_TEST_DRF(RM_INA3221, _SHUNT_VOL_SUM, _SIGN, _NEGATIVE, reg)
        {
            tmp = 0;
        }
    }
    else
    {
        channelNum = 1;
        rShuntmOhm = pINA3221->rShuntmOhm[provIdx];

        status = INA3221_READ(pINA3221, LWRM_INA3221_SHUNT_VOL(provIdx), &reg);
        if (status != FLCN_OK)
        {
            return status;
        }

        tmp = DRF_VAL(RM_INA3221, _SHUNT_VOL, _VALUE, reg);
        // check for negative value.
        if FLD_TEST_DRF(RM_INA3221, _SHUNT_VOL, _SIGN, _NEGATIVE, reg)
        {
            tmp = 0;
        }
    }

    if (rShuntmOhm.bUseFXP8_8)
    {
        // Scale it to FXP since we are going to divide it by FXP rShunt.
        tmp = LW_TYPES_U32_TO_UFXP_X_Y(24, 8, tmp);
        *pLwrrentmA = tmp * LWRM_INA3221_SHUNT_VOLTAGE_SCALE
            / rShuntmOhm.value.ufxp8_8;
    }
    else
    {
        *pLwrrentmA = tmp * LWRM_INA3221_SHUNT_VOLTAGE_SCALE
            / rShuntmOhm.value.u16;
    }

    // Use M and B to correct result.
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_32BIT_OVERFLOW_FIXES))
    {
        //
        // (M * I_measured_mA): 4.12 * 32.0 => 36.12
        // will not overflow since we are using 52.12 representation.
        //
        lwrrentmACorrectedMS52_12 = ((LwS64)(*pLwrrentmA) *
            (LwSFXP52_12)(pINA3221->lwrrCorrectM));

        //
        // (B_Amp * 1000): 4.12 * 10.0 => 14.12
        // ((B_Amp * 1000) * channelNum): 14.12 * 32.0 => 46.12
        // will not overflow since we are using 52.12 representation.
        //
        lwrrentmACorrectedBS52_12 = ((LwS64)(channelNum) *
            (LwSFXP52_12)(pINA3221->lwrrCorrectB * 1000));

        //
        // I_real_mA = (M * I_measured_mA) + (B_Amp * 1000)
        // 36.12 + 46.12 => 47.12
        // will not overflow since we are using 52.12 representation.
        //
        lwrrentmAS52_12 = lwrrentmACorrectedMS52_12 +
            lwrrentmACorrectedBS52_12;

        //
        // First, retrieve the integer part. Then check to see if it is in
        // the range of values (i.e., [0, LW_U32_MAX]), and if not, clamp
        // it to the range.
        //
        lwrrentmAS64 = LW_TYPES_SFXP_X_Y_TO_S64_ROUNDED(
            52, 12, lwrrentmAS52_12);
        if (lwrrentmAS64 < 0)
        {
            *pLwrrentmA = lwrrentmAS64 = 0;
        }
        else if (lwrrentmAS64 > LW_U32_MAX)
        {
            *pLwrrentmA = lwrrentmAS64 = LW_U32_MAX;
        }
        else
        {
            *pLwrrentmA = (LwU32)lwrrentmAS64;
        }
    }
    else
    {
        //
        // I_real_mA = (M * I_measured_mA) + (B_Amp * 1000)
        // 4.12 * 32.0 + 4.12 * 32.0 = 20.12
        // Will overflow at 1048.576A, that's a quite safe assumption.
        // For sum channel, multiply B by number of channels. For example:
        // I_real_mA = I1_real_mA + I2_real_mA
        //           = (M * I1_meas + B)+(M * I2_meas + B)
        //           = M * (I1_meas + I2_meas) + 2 * B
        //
        *pLwrrentmA = *pLwrrentmA * pINA3221->lwrrCorrectM +
                     pINA3221->lwrrCorrectB * 1000 * channelNum;
        // Check for possible underflow, since B might be negative.
        if ((LwS32)*pLwrrentmA < 0)
        {
            *pLwrrentmA = 0;
        }
        *pLwrrentmA = LW_TYPES_UFXP_X_Y_TO_U32(20, 12, *pLwrrentmA);
    }

    return FLCN_OK;
}

/*!
 * INA3221 power device implementation.
 *
 * @copydoc PwrDevGetVoltage
 */
FLCN_STATUS
pwrDevGetVoltage_INA3221
(
    PWR_DEVICE  *pDev,
    LwU8         provIdx,
    LwU32       *pVoltageuV
)
{
    PWR_DEVICE_INA3221 *pINA3221 = (PWR_DEVICE_INA3221 *)pDev;
    LwU16               reg;
    FLCN_STATUS         status;

    // Get voltage of the first enabled channel if provIdx = sum.
    if (provIdx == RM_PMU_PMGR_PWR_DEVICE_INA3221_PROV_SUM)
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_2X))
        {
            provIdx = pINA3221->highestProvIdx;
        }
        else
        {
            return FLCN_ERR_NOT_SUPPORTED;
        }
    }

    status = INA3221_READ(pINA3221, LWRM_INA3221_BUS_VOL(provIdx), &reg);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Scale from mW register -> uV interface
    *pVoltageuV = DRF_VAL(RM_INA3221, _BUS_VOL, _VALUE, reg) *
                    LWRM_INA3221_BUS_VOLTAGE_SCALE * 1000;

    return FLCN_OK;
}

/*!
 * INA3221 power device implementation.
 *
 * @copydoc PwrDevGetPower
 */
FLCN_STATUS
pwrDevGetPower_INA3221
(
    PWR_DEVICE  *pDev,
    LwU8         provIdx,
    LwU32       *pPowermW
)
{
    LW2080_CTRL_PMGR_PWR_TUPLE tuple;
    FLCN_STATUS status;

    // Do not support sum channel.
    if (provIdx == RM_PMU_PMGR_PWR_DEVICE_INA3221_PROV_SUM)
    {
        return FLCN_ERR_NOT_SUPPORTED;
    }

    status = pwrDevTupleGet_INA3221(pDev, provIdx, &tuple);

    if (status == FLCN_OK)
    {
        *pPowermW = tuple.pwrmW;
    }

    return status;
}

/*!
 * INA3221 power device implementation.
 *
 * @copydoc PwrDevTupleGet
 */
FLCN_STATUS
pwrDevTupleGet_INA3221
(
    PWR_DEVICE                 *pDev,
    LwU8                        provIdx,
    LW2080_CTRL_PMGR_PWR_TUPLE *pTuple
)
{
    PWR_DEVICE_INA3221 *pINA3221  = (PWR_DEVICE_INA3221 *)pDev;
    LwU32               voltagemV;
    FLCN_STATUS         status;

    //
    // Assign tamper detection channel index. Only do this for the
    // first provider that hits this point i.e. when index is
    // uninitialized.
    //
    if (pINA3221->tamperIdx == PWR_DEVICE_INA3221_TAMPER_IDX_UNINITIALIZED)
    {
        pINA3221->tamperIdx = provIdx;
    }

    //
    // Do tamper check and recovery only when current provider index
    // is the saved tamper detection channel index. This will reduce
    // unnecessary tamper check.
    //
    if (pINA3221->tamperIdx == provIdx)
    {
        status = s_pwrDevIsTamperingDetected_INA3221(pINA3221,
            &pINA3221->bTamperingDetected);

        // Return status if register read goes wrong
        if (status != FLCN_OK)
        {
            return status;
        }

        //
        // If tampering is detected, Load again when we are in tamper
        // detect channel index.
        //
        if (pINA3221->bTamperingDetected)
        {
            (void)pwrDevLoad_INA3221(pDev);
        }
    }

    //
    // If tampering is detected, return TAMPERED when we are not in tamper
    // detect channel index.
    //
    if (pINA3221->bTamperingDetected)
    {
        return FLCN_ERR_PWR_DEVICE_TAMPERED;
    }

    // Get Current
    status = pwrDevGetLwrrent_INA3221(pDev, provIdx, &(pTuple->lwrrmA));
    if (status != FLCN_OK)
    {
        return status;
    }

    // Get Voltage
    status = pwrDevGetVoltage_INA3221(pDev, provIdx, &(pTuple->voltuV));
    if (status != FLCN_OK)
    {
        return status;
    }
    voltagemV      = LW_UNSIGNED_ROUNDED_DIV(pTuple->voltuV, 1000);
    pTuple->pwrmW = voltagemV * (pTuple->lwrrmA) / 1000;

    return FLCN_OK;
}

/*!
 * INA3221 power device implementation.
 *
 * @copydoc PwrDevSetLimit
 */
FLCN_STATUS
pwrDevSetLimit_INA3221
(
    PWR_DEVICE *pDev,
    LwU8        provIdx,
    LwU8        limitIdx,
    LwU8        limitUnit,
    LwU32       limitValue
)
{
    PWR_DEVICE_INA3221 *pINA3221            = (PWR_DEVICE_INA3221 *)pDev;
    LwU32               busVoltagemV        = 0;
    LwU32               shuntVoltuV         = 0;
    LwU32               channelCount        = 0;
    RM_PMU_PMGR_PWR_DEVICE_DESC_RSHUNT
                        rShuntmOhm;
    LwU16               alertLimit          = 0;
    FLCN_STATUS         status              = FLCN_OK;
    LwUFXP20_12         scaledCorrectionB;
    LwS64               lwrrentmACorrectedBS52_12;
    union
    {
        LwUFXP52_12 u;
        LwSFXP52_12 s;
    }limitValue52_12;

    //
    // Handle summation provider. Find any channel that is enabled
    // and use its rShuntmOhm here.
    //
    if (provIdx == RM_PMU_PMGR_PWR_DEVICE_INA3221_PROV_SUM)
    {
        channelCount = pINA3221->channelsNum;
        rShuntmOhm   = pINA3221->rShuntmOhm[pINA3221->highestProvIdx];
    }
    else
    {
        channelCount = 1;
        rShuntmOhm   = pINA3221->rShuntmOhm[provIdx];
    }

    // Check if this is setting current or power limit value.
    if (limitUnit == LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_LWRRENT_MA)
    {
        //
        // Scale limitValue in reverse direction
        // I_reported = M * I_measured + B * channelCount
        // I_measured = (I_reported - B * channelCount) / M
        //
        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_32BIT_OVERFLOW_FIXES))
        {
            //
            // Since limitValue is 32.0, effectively, we obtain a 32.12 number
            // here in limitValue52_12. This will not overflow since we are
            // using 52.12 representation.
            //
            limitValue52_12.u = LW_TYPES_U64_TO_UFXP_X_Y(52, 12, limitValue);

            //
            // (B_Amp * 1000): 4.12 * 10.0 => 14.12
            // ((B_Amp * 1000) * channelNum): 14.12 * 32.0 => 46.12
            // will not overflow since we are using 52.12 representation.
            //
            lwrrentmACorrectedBS52_12 = ((LwS64)(channelCount) *
                (LwSFXP52_12)(pINA3221->lwrrCorrectB * 1000));

            //
            // 32.12 - 46.12 => 47.12 (signed arithmetic)
            // will not overflow since we are using 52.12 representation.
            //
            limitValue52_12.s = limitValue52_12.s - lwrrentmACorrectedBS52_12;
            if (limitValue52_12.s > 0)
            {
                //
                // 47.12 / 52.12 => 59.0 (limitValue52_12 now contains a 59.0
                //                        value)
                // will not overflow since we are using 64-bit representation.
                //
                lwU64DivRounded(
                    &limitValue52_12.u,
                    &limitValue52_12.u,
                    &(LwUFXP52_12){ pINA3221->lwrrCorrectM });
            }

            //
            // Check to see if we are within the range of values (i.e.,
            // [0, LW_U32_MAX]), and if not, flag an error.
            //
            if ((limitValue52_12.s < 0) ||
                (limitValue52_12.s > LW_U32_MAX))
            {
                //
                // !!!Please check the current correction factor in the VBIOS !!!
                // https://wiki.lwpu.com/engwiki/index.php/Resman/PState/Data_Tables/Power_Tables/Power_Sensors_Table_2.X/INA3221#Step_1_2
                //
                PMU_BREAKPOINT();
                return FLCN_ERR_ILWALID_ARGUMENT;
            }

            limitValue = limitValue52_12.u;
        }
        else
        {
            //
            // 1. Check and complain (PMU ERROR) if input limit will overflow
            //    after scaling (UFX20_12) (i.e. the fixed part >= 0xFFFFF000).
            //    We will only support an input HT limit < 1048575mA.
            // 2. Scale limitValue to UFXP20_12 format in mA. This step will
            //    overflow if limitValue > 2^20 mA = 1048.5A, which should be
            //    safe on our boards.
            // 3. Scale B to SFXP20_12 format in mA - simply multiply B by 1000.
            // 4. Callwlate limitValue - B * channelCount. Result is in UFXP20_12.
            // 5. Check and complain if incorrect config is causing a
            //    limitValue < value at step 3?
            // 6. Divide result of (3) by M. M is in UFXP4_12 so the result will
            //    be in 20.0 format (20.12 / 4.12 = 20.0).
            //
            if (limitValue <
                LW_TYPES_UFXP_X_Y_TO_U32(20, 12, (LwUFXP20_12)LW_U32_MAX))
            {
                limitValue = LW_TYPES_U32_TO_UFXP_X_Y(20, 12, limitValue);

                // Store the temporary scaling factor for step 3 above.
                scaledCorrectionB = pINA3221->lwrrCorrectB * channelCount * 1000;
                if (limitValue > scaledCorrectionB)
                {
                    limitValue -= scaledCorrectionB;
                }
                else
                {
                    //
                    // !!!Please check the current correction factor in the VBIOS !!!
                    // https://wiki.lwpu.com/engwiki/index.php/Resman/PState/Data_Tables/Power_Tables/Power_Sensors_Table_2.X/INA3221#Step_1_2
                    //
                    PMU_BREAKPOINT();
                    return FLCN_ERR_ILWALID_ARGUMENT;
                }
            }
            else
            {
                //
                // !!!Please check the input limit, it is probably incorrectly programmed!!!.
                // It must be less than 1048575mA and max that we can support. Also dolwmented at
                // https://wiki.lwpu.com/engwiki/index.php/Resman/PState/Data_Tables/Power_Tables/Power_Sensors_Table_2.X/INA3221#Step_1_2
                //
                PMU_BREAKPOINT();
                return FLCN_ERR_ILWALID_STATE;
            }
            limitValue /= pINA3221->lwrrCorrectM;
        }
    }
    else if (limitUnit == LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_POWER_MW)
    {
        // Get Bus Voltage.
        status = pwrDevGetVoltage_INA3221(pDev, provIdx, &busVoltagemV);
        if (status != FLCN_OK)
        {
            return status;
        }
        busVoltagemV = LW_UNSIGNED_ROUNDED_DIV(busVoltagemV, 1000);
        limitValue   = limitValue * 1000 / busVoltagemV;
    }
    else
    {
        return FLCN_ERR_NOT_SUPPORTED;
    }

    //
    // Callwlate and colwert limit value into register value to write in.
    // Always truncate value so that we will alwasy stay below the value
    // requested.
    //
    if (rShuntmOhm.bUseFXP8_8)
    {
        //
        // Scale it back to U32 since we multiplied FXP rShunt.
        // shuntVoltuV will overflow if shunt volt is greater than 16.7V. This
        // is a safe assumption.
        //
        shuntVoltuV = LW_TYPES_UFXP_X_Y_TO_U32(24, 8,
                limitValue * rShuntmOhm.value.ufxp8_8);
    }
    else
    {
        shuntVoltuV = limitValue * rShuntmOhm.value.u16;
    }

    switch (limitIdx)
    {
        case LWRM_INA3221_LIMIT_IDX_CRITICAL:
            // Handle sum channel limit.
            if (provIdx == RM_PMU_PMGR_PWR_DEVICE_INA3221_PROV_SUM)
            {
                alertLimit = DRF_NUM(RM_INA3221, _SHUNT_VOL_SUM_LIMIT, _VALUE,
                    shuntVoltuV/LWRM_INA3221_SHUNT_VOLTAGE_SCALE);
                return INA3221_WRITE(pINA3221, LWRM_INA3221_SHUNT_VOL_SUM_LIMIT,
                            alertLimit);
            }
            alertLimit = DRF_NUM(RM_INA3221, _CRIT_ALERT_LIMIT, _VALUE,
                shuntVoltuV/LWRM_INA3221_SHUNT_VOLTAGE_SCALE);
            return INA3221_WRITE(pINA3221, LWRM_INA3221_CRIT_ALERT_LIMIT(provIdx),
                        alertLimit);
        case LWRM_INA3221_LIMIT_IDX_WARNING:
            // Handle sum channel limit.
            if (provIdx == RM_PMU_PMGR_PWR_DEVICE_INA3221_PROV_SUM)
            {
                return FLCN_ERR_NOT_SUPPORTED;
            }
            alertLimit = DRF_NUM(RM_INA3221, _WARN_ALERT_LIMIT, _VALUE,
                shuntVoltuV/LWRM_INA3221_SHUNT_VOLTAGE_SCALE);
            return INA3221_WRITE(pINA3221, LWRM_INA3221_WARN_ALERT_LIMIT(provIdx),
                        alertLimit);
    }

    return FLCN_ERR_NOT_SUPPORTED;
}

/* ------------------------ Static Functions -------------------------------- */

/*!
 * @brief Perform consistency checking to test for device tampering.
 *
 * To detect for possible external device-tampering, read-back the
 * configuration and mask/enable registers and compare the values with what is
 * expected to be programmed.
 *
 * @param[in]   pINA3221     Pointer to the INA3221 device object
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
s_pwrDevIsTamperingDetected_INA3221
(
    PWR_DEVICE_INA3221 *pINA3221,
    LwBool             *pBDetected
)
{
    LwU16      reg = 0;
    FLCN_STATUS status;

    // validate Configuration register
    status = INA3221_READ(pINA3221, LWRM_INA3221_CONFIGURATION, &reg);
    if (status != FLCN_OK)
    {
        return status;
    }
    if (reg != pINA3221->configuration)
    {
        *pBDetected = LW_TRUE;
        return FLCN_OK;
    }

    *pBDetected = LW_FALSE;

    return FLCN_OK;
}
