/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    thrmga100.c
 * @brief   Thermal PMU HAL functions for GA100+
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"

#include "config/g_therm_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define THERM_TEST_OFFSET_TEMP              RM_PMU_CELSIUS_TO_LW_TEMP(20)

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief   Test the _OFFSET_TSENSE and _SENSOR_MAX registers
 *
 * @param[out]   pParam       Test parameters, indicating status/output data
 *                 outStatus  Indicates status of the test
 *                 outData    Indicates failure reason of the test
*/
void
thermTestOffsetMaxTempExelwte_GP10X
(
    RM_PMU_RPC_STRUCT_THERM_TEST_EXELWTE *pParams
)
{
    LwU32  origOffset;
    LwU32  origMax;
    LwU32  reg32;
    LwTemp tsenseOffsetTemp;
    LwTemp tsenseTemp;
    LwTemp maxTemp;

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC;

    // Step1: Cache current HW state
    origOffset  = REG_RD32(CSB, LW_CPWR_THERM_SENSOR_8);
    origMax     = REG_RD32(CSB, LW_CPWR_THERM_SENSOR_9);

    //
    // Step 2: Configure OFFSET value for _OFFSET_TSENSE using _SENSOR_8
    //
    // Note: HW uses 9.5 fixed point notation. Colwert LwTemp to 9.5 when
    // writing to HW
    //
    reg32 = origOffset;
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _SENSOR_8, _TSENSE_OFFSET,
                                RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(THERM_TEST_OFFSET_TEMP), reg32);
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_8, reg32);

    // Step 3: Configure SENSOR_MAX to point to _TSENSE and _OFFSET_TSENSE
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_9,
        DRF_DEF(_CPWR_THERM, _SENSOR_9_MAX, _TSENSE, _YES) |
        DRF_DEF(_CPWR_THERM, _SENSOR_9_MAX, _OFFSET_TSENSE, _YES));

    // Step 4: Check if OFFSET_TSENSE reflects correct value
    tsenseTemp = thermGetTempInternal_HAL(&Therm, 
              LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_TSENSE);

    tsenseOffsetTemp = thermGetTempInternal_HAL(&Therm, 
              LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_TSENSE_OFFSET);

    if (tsenseOffsetTemp != (tsenseTemp + THERM_TEST_OFFSET_TEMP))
    {
        pParams->outData = LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS,
                               TSENSE_OFFSET_VALUE_FAILURE);
        goto thermTestOffsetMaxTempExelwte_GP10X_exit;
    }

    // Step 5: Check if _SENSOR_MAX reflects correct value
    maxTemp = thermGetTempInternal_HAL(&Therm,
                  LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_MAX);

    if (maxTemp != tsenseOffsetTemp)
    {
        pParams->outData =
            LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS, MAX_VALUE_FAILURE);
        goto thermTestOffsetMaxTempExelwte_GP10X_exit;
    }

    pParams->outStatus = LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS;

thermTestOffsetMaxTempExelwte_GP10X_exit:

    // Restore original values
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_9, origMax);
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_8, origOffset);
}
