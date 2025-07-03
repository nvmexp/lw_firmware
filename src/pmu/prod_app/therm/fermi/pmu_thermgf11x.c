/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_thermgf11x.c
 * @brief   PMU HAL functions for GF11X+ for misc. thermal functionality
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_pwr_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "task_therm.h"
#include "pmu_objic.h"
#include "therm/objtherm.h"
#include "dbgprintf.h"

#include "config/g_therm_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Initializes the sensor's A and B values and stores them in static variables
 * in normalized form(F0.16) to be used later by functions to read the internal
 * GPU temperature. For example, must be called before thermGetTempInternal()
 * can be called requesting the high precision.
 *
 * @return FLCN_OK
 */
FLCN_STATUS
thermInitializeIntSensorAB_GM10X(void)
{
    LwU32 sensor1;
    LwU32 sensor2;
    LwU32 sensor3;

    sensor1 = REG_RD32(CSB, LW_CPWR_THERM_SENSOR_1);
    sensor2 = REG_RD32(CSB, LW_CPWR_THERM_SENSOR_2);
    sensor3 = REG_RD32(CSB, LW_CPWR_THERM_SENSOR_3);

    // A (on GF11x: 10-bit positive integer (F0.14))
    if (FLD_TEST_DRF(_CPWR_THERM, _SENSOR_1, _SELECT_SW_A, _ON, sensor1))
    {
        ThermSensorA = (LwS32) DRF_VAL(_CPWR_THERM, _SENSOR_2, _SW_A, sensor2);
    }
    else
    {
        ThermSensorA = (LwS32) DRF_VAL(_CPWR_THERM, _SENSOR_3, _HW_A, sensor3);
    }

    //
    // B (on GF11x: 11-bit positive fixed point number with one decimal place,
    // and formula has changed to T = A * RAW - B)
    //
    ThermSensorB = 0;
    if (FLD_TEST_DRF(_CPWR_THERM, _SENSOR_1, _SELECT_SW_B, _ON, sensor1))
    {
        ThermSensorB -= (LwS32) DRF_VAL(_CPWR_THERM, _SENSOR_2, _SW_B, sensor2);
    }
    else
    {
        ThermSensorB -= (LwS32) DRF_VAL(_CPWR_THERM, _SENSOR_3, _HW_B, sensor3);
    }

    // Normalize to F0.16
    ThermSensorA = ThermSensorA << (16-14);
    ThermSensorB = ThermSensorB << (16-1);

    return FLCN_OK;
}

/*!
 * @brief   Set the temperature override and temperature select override
 *
 * @param[in]   bEnable     Boolean flag enable or disable TempSim
 * @param[in]   temp        Temperature to override the real temperature
 *
 * @return  FLCN_OK     Always succeeds
 */
FLCN_STATUS
thermTempSimSetControl_GM10X
(
    LwBool  bEnable,
    LwTemp  temp
)
{
    LwU32   regVal = REG_RD32(CSB, LW_CPWR_THERM_SENSOR_7);

    // Set the raw override
    regVal = FLD_SET_DRF_NUM(_CPWR_THERM, _SENSOR_7, _DEBUG_TS_ADC_OUT,
                             thermGetRawFromTempInt_HAL(&Therm, temp), regVal);

    // Set the raw select override
    regVal = bEnable ? FLD_SET_DRF(_CPWR_THERM, _SENSOR_7,
                           _DEBUG_TS_ADC_OVERRIDE, _ENABLE, regVal) :
                       FLD_SET_DRF(_CPWR_THERM, _SENSOR_7,
                           _DEBUG_TS_ADC_OVERRIDE, _DISABLE, regVal);

    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_7, regVal);

    return FLCN_OK;
}

/*!
 * Colwerts a temperature in celsius signed F16.16 format to a raw temperature
 * code.
 *
 * @param[in]   temp signed F16.16 celsius temperature to colwert to a raw
 *              temperature code.
 *
 * @return      Raw temperature code as signed 32-bit value
 *
 * @pre         Must initialize ThermSensorA and ThermSensorB values before
 *              calling this function, via thermInitializeIntSensorAB(void).
 */
LwU32
thermGetRawFromTempInt_GM10X
(
    LwTemp  temp
)
{
    LwU32       rawTemp = 0;
    LwSFXP16_16 coeffA = (LwSFXP16_16)ThermSensorA;
    LwSFXP16_16 coeffB = (LwSFXP16_16)ThermSensorB;
    LwSFXP16_16 tmp;

    // Need to protect against divide by zero!
    if (coeffA != LW_TYPES_FXP_ZERO)
    {
        //
        // We need a slope to proceed. If its "0" someone is already playing
        // with thermal control so simply bail out (and return "0"). All math
        // is done in F16.16 format.
        //
        tmp = (LwSFXP16_16)(temp << 8) - coeffB;
        if (tmp >= LW_TYPES_FXP_ZERO)
        {
            //
            // Since we are using normalized A&B values temperature in [C] is
            // callwlated as T =(A*RAW+B)>>16 (same as on pre_GF11X).  We are
            // using ilwerse formula RAW =((T<<16)-B)/A corrected by adding
            // (A-1) to compensate for integer arithmetics error, that will
            // happen in HW.
            //
            rawTemp = (LwU32)((tmp + coeffA - 1) / coeffA);
        }
    }

    return rawTemp;
}
