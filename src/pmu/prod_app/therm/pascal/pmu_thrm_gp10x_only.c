/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_thrm_gp10x_tu10x.c
 * @brief   Thermal PMU HAL functions for GP10X till TU10X.
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"

#include "config/g_therm_private.h"

/*!
 * @brief   Get GPU temp. of internal temp-sensor.
 *
 * @param[in] thermDevProvIdx Device provider index to get the temperature
 *
 * @return  GPU temperature in 24.8 fixed point notation in [C]
 */
LwTemp
thermGetTempInternal_GP10X
(
    LwU8 thermDevProvIdx
)
{
    LwTemp intTemp = RM_PMU_LW_TEMP_0_C;

    ct_assert(LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_TSENSE ==
              LW_CPWR_THERM_TEMP_SENSOR_ID_TSENSE);
    ct_assert(LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_TSENSE_OFFSET ==
              LW_CPWR_THERM_TEMP_SENSOR_ID_OFFSET_TSENSE);
    ct_assert(LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_CONST ==
              LW_CPWR_THERM_TEMP_SENSOR_ID_CONSTANT);
    ct_assert(LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_MAX ==
              LW_CPWR_THERM_TEMP_SENSOR_ID_MAX);

    if (thermDevProvIdx < LW_CPWR_THERM_TEMP_SENSOR_ID_COUNT)
    {
        // Colwert from SFP9.5 -> LwTemp (SFP24.8).
        intTemp = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
            _CPWR_THERM, _TEMP_SENSOR, _FIXED_POINT,
            REG_RD32(CSB, LW_CPWR_THERM_TEMP_SENSOR(thermDevProvIdx)));
    }

    return intTemp;
}
