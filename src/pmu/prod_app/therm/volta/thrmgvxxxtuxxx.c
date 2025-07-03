/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    thrmgvxxxtuxxx.c
 * @brief   Thermal PMU HAL functions for GV10X to TU10X
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_pwr_csb.h"
#include "dev_gc6_island.h"
#include "pmu_bar0.h"
#include "pmu_oslayer.h"

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
 * @brief   Get GPU temp. of internal GPU temp-sensor.
 *
 * @param[in] thermDevProvIdx Device provider index to get the temperature
 *
 * @return  GPU temperature in 24.8 fixed point notation in [C]
 */
LwTemp
thermGetTempInternal_GV10X
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
    ct_assert(LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_MAX ==
              LW_CPWR_THERM_TEMP_SENSOR_ID_GPC_TSOSC_MAX);
    ct_assert(LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_OFFSET_MAX ==
              LW_CPWR_THERM_TEMP_SENSOR_ID_GPC_TSOSC_OFFSET_MAX);
    ct_assert(LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_AVG ==
              LW_CPWR_THERM_TEMP_SENSOR_ID_GPC_TSOSC_AVG);
    ct_assert(LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_OFFSET_AVG ==
              LW_CPWR_THERM_TEMP_SENSOR_ID_GPC_TSOSC_OFFSET_AVG);
    ct_assert(LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_DYNAMIC_HOTSPOT ==
              LW_CPWR_THERM_TEMP_SENSOR_ID_DYNAMIC_HOTSPOT);
    ct_assert(LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_DYNAMIC_HOTSPOT_OFFSET ==
              LW_CPWR_THERM_TEMP_SENSOR_ID_OFFSET_DYNAMIC_HOTSPOT);

    if (thermDevProvIdx < LW_CPWR_THERM_TEMP_SENSOR_ID_COUNT)
    {
        // Colwert from SFP9.5 -> LwTemp (SFP24.8).
        intTemp = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
            _CPWR_THERM, _TEMP_SENSOR, _FIXED_POINT,
            REG_RD32(CSB, LW_CPWR_THERM_TEMP_SENSOR((LwU32)thermDevProvIdx)));
    }

    return intTemp;
}

/*!
 * @brief   Get GPU temp. of internal GPU_GPC_TSOSC temp-sensor.
 *
 * @param[in] thermDevProvIdx Device provider index to get the temperature
 * @param[in] gpcTsoscIdx     Index that specifies GPU_GPC_TSOSC THERM_DEVICE for
 *                            this GPC
 *
 * @return  GPU_GPC_TSOSC's temperature in 24.8 fixed point notation in [C]
 */
LwTemp
thermGetGpuGpcTsoscTemp_GV10X
(
    LwU8   thermDevProvIdx,
    LwU8   gpcTsoscIdx
)
{
    LwTemp tsoscTemp = RM_PMU_LW_TEMP_0_C;
    LwU32  reg32;

    if (thermDevProvIdx < LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_GPC_TSOSC_PROV__NUM_PROVS)
    {
        // TSOSC temperature read should occur atomically as these are index registers.
        appTaskCriticalEnter();
        {
            // Program the GPC_TSOSC_INDEX.
            thermGpuGpcTsoscIdxSet_HAL(&Therm, gpcTsoscIdx);

            reg32 = REG_RD32(CSB, LW_CPWR_THERM_TEMP_SENSOR_GPC_TSOSC((LwU32)thermDevProvIdx));
            if (FLD_TEST_DRF(_CPWR_THERM, _TEMP_SENSOR_GPC_TSOSC, _STATE, _VALID, reg32))
            {
                // Colwert from SFP9.5 -> LwTemp (SFP24.8).
                tsoscTemp = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
                    _CPWR_THERM, _TEMP_SENSOR_GPC_TSOSC, _FIXED_POINT, reg32);
            }
        }
        appTaskCriticalExit();
    }

    return tsoscTemp;
}

/*!
 * @brief   Read event temperature threshold, negative hotspot offset and sensorId
 *
 * @param[in]   eventId         RM_PMU_THERM_EVENT_* thermal event
 * @param[out]  pTempThreshold  ptr to threshold temperature associated
 *                              with the specified eventId
 * @param[out]  pNegHSOffset    optional ptr to the HotSpot offset value
 * @param[out]  pSensorId       optional ptr to the sensor Id value
 *
 *
 * @return  FLCN_ERR_ILWALID_ARGUMENT   if invalid eventId was passed
 * @return  FLCN_ERR_NOT_SUPPORTED      if eventId is unused/inactive
 * @return  FLCN_OK                     otherwise
 */
FLCN_STATUS
thermSlctGetEventTempThresholdHSOffsetAndSensorId_GV10X
(
    LwU8    eventId,
    LwTemp *pTempThreshold,
    LwTemp *pNegHSOffset,
    LwU8   *pSensorId
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8        sensorId;

    // Ensure that eventId is applicable to this use case.
    if (RM_PMU_THERM_EVENT_SCI_FS_OVERT == eventId)
    {
        *pTempThreshold = LW_TYPES_CELSIUS_TO_LW_TEMP(
                    DRF_VAL_SIGNED(_PGC6, _SCI_FS_OVERT_THRESH, _TEMP,
                        REG_RD32(FECS, LW_PGC6_SCI_FS_OVERT_THRESH)));
        if (pNegHSOffset != NULL)
        {
            *pNegHSOffset = 0;
        }
    }
    else
    {
        status = thermSlctGetEventTempThresholdHSOffsetAndSensorId_GP10X(eventId,
                                        pTempThreshold, pNegHSOffset, &sensorId);

        if (status == FLCN_OK)
        {
            if ((pNegHSOffset != NULL) &&
                ((sensorId == LW_CPWR_THERM_EVT_SETTINGS_TEMP_SENSOR_ID_GPC_TSOSC_OFFSET_MAX) ||
                 (sensorId == LW_CPWR_THERM_EVT_SETTINGS_TEMP_SENSOR_ID_GPC_TSOSC_MAX)))
            {
#if PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_POLICY)
                *pNegHSOffset = -(Therm.policyMetadata.lwTempWorstCaseGpuMaxToAvgDelta);
#else
                *pNegHSOffset = 0;
#endif
            }
            if (pSensorId != NULL)
            {
                *pSensorId = sensorId;
            }
        }
    }

    return status;
}

/*!
 * @brief   Set the temperature override and temperature select override
 *
 * @param[in]   bEnable     Boolean flag to enable or disable TempSim
 * @param[in]   temp        Temperature to override the real temperature
 *
 * @return  FLCN_OK      In case we are able to successfully apply tempsim settings
 * @return  Other errors Unexpected errors propagated from other functions.
 */
FLCN_STATUS
thermTempSimSetControl_GV10X
(
    LwBool bEnable,
    LwTemp temp
)
{
    LwU32 reg32 = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSOSC_EN_MASK);

    if (reg32 != 0U)
    {
        reg32 = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSOSC_GLOBAL_OVERRIDE);
        reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_TSOSC_GLOBAL_OVERRIDE,
                    _TEMP_VALUE, RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(temp), reg32);
        reg32 = bEnable ? FLD_SET_DRF(_CPWR_THERM, _GPC_TSOSC_GLOBAL_OVERRIDE,
                                _TEMP_SELECT, _OVERRIDE, reg32) :
                          FLD_SET_DRF(_CPWR_THERM, _GPC_TSOSC_GLOBAL_OVERRIDE,
                                _TEMP_SELECT, _REAL, reg32);
        REG_WR32(CSB, LW_CPWR_THERM_GPC_TSOSC_GLOBAL_OVERRIDE, reg32);

        return FLCN_OK;
    }
    return thermTempSimSetControl_GP10X(bEnable, temp);
}

/*!
 * @brief  Sets GPU_GPC_TSOSC index and also disables Write/Read increment.
 *
 * @param[in]   gpuGpcTsoscIdx       Index to be set.
 */
void
thermGpuGpcTsoscIdxSet_GV10X
(
    LwU8  gpuGpcTsoscIdx
)
{
    LwU32 regTsoscIdx = REG_RD32(CSB, LW_CPWR_THERM_GPC_TSOSC_INDEX);

    // Set GPC index to select tsosc sensor.
    regTsoscIdx = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_TSOSC_INDEX,
                        _VALUE, gpuGpcTsoscIdx, regTsoscIdx);

    // Disable write and Read Increment.
    regTsoscIdx = FLD_SET_DRF(_CPWR_THERM, _GPC_TSOSC_INDEX,
                        _WRITEINCR, _DISABLED, regTsoscIdx);
    regTsoscIdx = FLD_SET_DRF(_CPWR_THERM, _GPC_TSOSC_INDEX,
                        _READINCR, _DISABLED, regTsoscIdx);

    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSOSC_INDEX, regTsoscIdx);
}
