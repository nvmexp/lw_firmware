/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    thrmgv10x.c
 * @brief   Thermal PMU HAL functions for GV10X+
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_pwr_csb.h"
#include "dev_gc6_island.h"
#include "pmu_bar0.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"

#include "config/g_therm_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Get GPU temp. of internal GPU_SCI temp-sensor.
 *
 * @param[in] thermDevProvIdx Device provider index to get the temperature
 *
 * @return  GPU_SCI's temperature in 24.8 fixed point notation in [C]
 */
LwTemp
thermGetGpuSciTemp_GV10X
(
    LwU8   thermDevProvIdx
)
{
    LwTemp intTemp = RM_PMU_LW_TEMP_0_C;

    if (thermDevProvIdx < LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_SCI_PROV__NUM_PROVS)
    {
        switch (thermDevProvIdx)
        {
            case LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_SCI_PROV_MINI_TSENSE:
            {
                // SCI_FS_OVERT HW uses signed integer value to store temperature.
                intTemp = LW_TYPES_CELSIUS_TO_LW_TEMP((DRF_VAL_SIGNED(_PGC6,
                            _SCI_FS_OVERT_TEMPERATURE, _INTEGER,
                            REG_RD32(FECS, LW_PGC6_SCI_FS_OVERT_TEMPERATURE))));
                break;
            }

            default:
            {
                // To pacify coverity.
                break;
            }
        }
    }

    return intTemp;
}
