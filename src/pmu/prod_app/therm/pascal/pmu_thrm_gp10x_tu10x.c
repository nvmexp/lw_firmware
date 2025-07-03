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
 * @brief   Set the temperature override and temperature select override
 *
 * @param[in]   bEnable     Boolean flag enable or disable TempSim
 * @param[in]   temp        Temperature to override the real temperature
 *
 * @return  FLCN_OK     Always succeeds
 */
FLCN_STATUS
thermTempSimSetControl_GP10X
(
    LwBool  bEnable,
    LwTemp  temp
)
{
    LwU32   regVal = REG_RD32(CSB, LW_CPWR_THERM_SENSOR_0);

    // RMW on signed field using unsigned macro!!!
    regVal = FLD_SET_DRF_NUM(_CPWR_THERM, _SENSOR_0, _TEMP_OVERRIDE,
                RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(temp), regVal);

    regVal = bEnable ? FLD_SET_DRF(_CPWR_THERM, _SENSOR_0, _TEMP_SELECT,
                           _OVERRIDE, regVal) :
                       FLD_SET_DRF(_CPWR_THERM, _SENSOR_0, _TEMP_SELECT,
                           _REAL, regVal);

    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_0, regVal);

    return FLCN_OK;
}
