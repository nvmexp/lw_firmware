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
 * @file    thrmga10x.c
 * @brief   Thermal PMU HAL functions for GA100+
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"
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
 * @brief   Get GPU temp. of internal GPU temp-sensor.
 *
 * @param[in] thermDevProvIdx Device provider index to get the temperature
 *
 * @return  GPU temperature in 24.8 fixed point notation in [C]
 */
LwTemp
thermGetTempInternal_GA10X
(
    LwU8 thermDevProvIdx
)
{
    LwTemp intTemp = RM_PMU_LW_TEMP_0_C;

    ct_assert(LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_TSENSE ==
              LW_CPWR_THERM_TEMP_SENSOR_ID_SYS_TSENSE);
    ct_assert(LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_TSENSE_OFFSET ==
              LW_CPWR_THERM_TEMP_SENSOR_ID_OFFSET_SYS_TSENSE);
    ct_assert(LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_CONST ==
              LW_CPWR_THERM_TEMP_SENSOR_ID_CONSTANT);
    ct_assert(LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_MAX ==
              LW_CPWR_THERM_TEMP_SENSOR_ID_MAX);
    ct_assert(LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_MAX ==
              LW_CPWR_THERM_TEMP_SENSOR_ID_GPU_MAX);
    ct_assert(LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_AVG ==
              LW_CPWR_THERM_TEMP_SENSOR_ID_GPU_AVG);
    ct_assert(LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_OFFSET_MAX ==
              LW_CPWR_THERM_TEMP_SENSOR_ID_GPU_OFFSET_MAX);
    ct_assert(LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_OFFSET_AVG ==
              LW_CPWR_THERM_TEMP_SENSOR_ID_GPU_OFFSET_AVG);

    if (thermDevProvIdx < LW_CPWR_THERM_TEMP_SENSOR_ID_COUNT)
    {
        // Colwert from SFP9.5 -> LwTemp (SFP24.8).
        intTemp = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
            _CPWR_THERM, _TEMP_SENSOR, _FIXED_POINT,
            REG_RD32(CSB, LW_CPWR_THERM_TEMP_SENSOR(thermDevProvIdx)));
    }

    return intTemp;
}

/*!
 * @brief   Set the temperature override and temperature select override
 *
 * @param[in]   bEnable     Boolean flag to enable or disable TempSim
 * @param[in]   temp        Temperature to override the real temperature
 *
 * @return      FLCN_OK     This call always succeeds.
 */
FLCN_STATUS
thermTempSimSetControl_GA10X
(
    LwBool bEnable,
    LwTemp temp
)
{
    LwU32 reg32 = REG_RD32(CSB, LW_CPWR_THERM_TSENSE_GLOBAL_OVERRIDE);

    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _TSENSE_GLOBAL_OVERRIDE,
                _VALUE, RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(temp), reg32);

    reg32 = bEnable ?
                FLD_SET_DRF(_CPWR_THERM, _TSENSE_GLOBAL_OVERRIDE,
                                _EN, _ENABLE, reg32) :
                FLD_SET_DRF(_CPWR_THERM, _TSENSE_GLOBAL_OVERRIDE,
                                _EN, _DISABLE, reg32);

    REG_WR32(CSB, LW_CPWR_THERM_TSENSE_GLOBAL_OVERRIDE, reg32);

    return FLCN_OK;
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
thermSlctGetEventTempThresholdHSOffsetAndSensorId_GA10X
(
    LwU8    eventId,
    LwTemp *pTempThreshold,
    LwTemp *pNegHSOffset,
    LwU8   *pSensorId
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       regAddr;
    LwU32       regVal;
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
        // Ensure that eventId is applicable to this use case.
        if (RM_PMU_THERM_EVENT_DEDICATED_OVERT == eventId)
        {
            regAddr = LW_CPWR_THERM_EVT_DEDICATED_OVERT;
        }
        else if (RM_PMU_THERM_IS_BIT_SET(eventId, LW_CPWR_THERM_EVENT_MASK_THERMAL))
        {
            if (RM_PMU_THERM_IS_BIT_SET(eventId, thermEventUseMaskGet_HAL(&Therm)))
            {
                regAddr = LW_CPWR_THERM_EVT_SETTINGS(eventId);
            }
            else
            {
                status = FLCN_ERR_NOT_SUPPORTED;
            }
        }
        else
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
        }

        if (status == FLCN_OK)
        {
            regVal = REG_RD32(CSB, regAddr);
            *pTempThreshold = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(_CPWR_THERM,
                                _EVT_SETTINGS, _TEMP_THRESHOLD, regVal);

            sensorId = (LwU8)DRF_VAL(_CPWR_THERM, _EVT_SETTINGS, _TEMP_SENSOR_ID, regVal);

            if (pNegHSOffset != NULL)
            {
                if (sensorId == LW_CPWR_THERM_EVT_SETTINGS_TEMP_SENSOR_ID_OFFSET_SYS_TSENSE)
                {
                    *pNegHSOffset = -RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
                                    _CPWR_THERM, _SYS_TSENSE_HS_OFFSET, _FIXED_POINT,
                                    REG_RD32(CSB, LW_CPWR_THERM_SYS_TSENSE_HS_OFFSET));
                }
                else
                {
                    *pNegHSOffset = 0;
                }
            }

            if (pSensorId != NULL)
            {
                *pSensorId = sensorId;
            }
        }

        if (status == FLCN_OK)
        {
            if ((pNegHSOffset != NULL) &&
                ((sensorId == LW_CPWR_THERM_EVT_SETTINGS_TEMP_SENSOR_ID_GPU_OFFSET_MAX) ||
                 (sensorId == LW_CPWR_THERM_EVT_SETTINGS_TEMP_SENSOR_ID_GPU_MAX)))
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
 * @brief   Gets HW MAXFAN state (active/inactive) for the requested PWM source.
 *
 * @param[in] pwmSource   Requested PWM source as RM_PMU_PMGR_PWM_SOURCE_<xyz>
 *
 * @return  Boolean flag indicating HW MAXFAN status.
 */
LwBool
thermHwMaxFanStatusGet_GA10X
(
    RM_PMU_PMGR_PWM_SOURCE  pwmSource
)
{
    LwBool  bStatus = LW_FALSE;
    LwU32   regVal  = REG_RD32(CSB, LW_CPWR_THERM_MAXFAN);

    if (pwmSource == RM_PMU_PMGR_PWM_SOURCE_SCI_FAN_0)
    {
        bStatus = FLD_TEST_DRF(_CPWR_THERM, _MAXFAN, _STATUS_SMARTFAN_SCI_0,
                                _PENDING, regVal);
    }
    else if (pwmSource == RM_PMU_PMGR_PWM_SOURCE_SCI_FAN_1)
    {
        bStatus = FLD_TEST_DRF(_CPWR_THERM, _MAXFAN, _STATUS_SMARTFAN_SCI_1,
                                _PENDING, regVal);
    }
    else if (pwmSource == RM_PMU_PMGR_PWM_SOURCE_THERM_PWM)
    {
        bStatus = FLD_TEST_DRF(_CPWR_THERM, _MAXFAN, _STATUS_SMARTFAN_THERM,
                                _PENDING, regVal);
    }
    else
    {
        PMU_BREAKPOINT();
    }

    return bStatus;
}

/*!
 * @brief   Activate/Deactivate HW MAXFAN status for the requested PWM source.
 *
 * @param[in] pwmSource Requested PWM source as RM_PMU_PMGR_PWM_SOURCE_<xyz>
 * @param[in] bSet      Boolean flag to activate or deactivate.
 */
void
thermHwMaxFanStatusSet_GA10X
(
    RM_PMU_PMGR_PWM_SOURCE  pwmSource,
    LwBool                  bSet
)
{
    LwU32 regVal = REG_RD32(CSB, LW_CPWR_THERM_MAXFAN);

    if (pwmSource == RM_PMU_PMGR_PWM_SOURCE_SCI_FAN_0)
    {
        regVal = bSet ?
                    FLD_SET_DRF(_CPWR_THERM, _MAXFAN, _FORCE, _TRIGGER, regVal) :
                    FLD_SET_DRF(_CPWR_THERM, _MAXFAN, _STATUS_SMARTFAN_SCI_0,
                                _CLEAR, regVal);
    }
    else if (pwmSource == RM_PMU_PMGR_PWM_SOURCE_SCI_FAN_1)
    {
        regVal = bSet ?
                    FLD_SET_DRF(_CPWR_THERM, _MAXFAN, _FORCE, _TRIGGER, regVal) :
                    FLD_SET_DRF(_CPWR_THERM, _MAXFAN, _STATUS_SMARTFAN_SCI_1,
                                _CLEAR, regVal);
    }
    else if (pwmSource == RM_PMU_PMGR_PWM_SOURCE_THERM_PWM)
    {
        regVal = bSet ?
                    FLD_SET_DRF(_CPWR_THERM, _MAXFAN, _FORCE, _TRIGGER, regVal) :
                    FLD_SET_DRF(_CPWR_THERM, _MAXFAN, _STATUS_SMARTFAN_THERM,
                                _CLEAR, regVal);
    }
    else
    {
        PMU_BREAKPOINT();
        goto thermHwMaxFanStatusSet_GA10X_exit;
    }

    REG_WR32(CSB, LW_CPWR_THERM_MAXFAN, regVal);

thermHwMaxFanStatusSet_GA10X_exit:
    lwosNOP();
}

/*!
 * @brief      Get GPC temperature of internal GPU_GPC_COMBINED temp-sensors.
 *
 * @param[in]  provIdx   Device provider index to get the temperature.
 * @param[out] pTemp     GPU_GPC_COMBINED's temperature in 24.8 fixed point notation in [C]. 
 *
 * @return  FLCN_ERR_ILWALID_FUNCTION   Temp is Invalid.
 * @return  FLCN_ERR_ILWALID_INDEX      Provide Index is Invalid.
 * @return  FLCN_OK                     Temp is valid and success.
 */
FLCN_STATUS
thermGetGpuGpcCombinedTemp_GA10X
(
    LwU8    provIdx,
    LwTemp *pTemp
)
{
    FLCN_STATUS status  = FLCN_OK;
    LwTemp      gpcTemp = RM_PMU_LW_TEMP_0_C;
    LwU32       reg32;

    // Sanity check for Sensor Id's supported for GPC_COMBINED DEVICE.
    ct_assert(LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_GPC_COMBINED_PROV_GPC_AVG_UNMUNGED ==
              LW_CPWR_THERM_TEMP_SENSOR_ID_GPC_AVG_UNMUNGED);
    ct_assert(LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_GPC_COMBINED_PROV_GPC_AVG_MUNGED ==
              LW_CPWR_THERM_TEMP_SENSOR_ID_GPC_AVG);
    ct_assert(LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_GPC_COMBINED_PROV__NUM_PROVS ==
              LW_CPWR_THERM_TEMP_SENSOR_GPC_COUNT);

    // Sanity check for pTemp argument.
    if (pTemp == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto thermGetGpuGpcCombinedTemp_GA10X_exit;
    }

    // Populate temp for the supported provider index.
    if (provIdx < LW_CPWR_THERM_TEMP_SENSOR_GPC_COUNT)
    {
        reg32 = REG_RD32(CSB, LW_CPWR_THERM_TEMP_SENSOR_GPC(provIdx));
        if (FLD_TEST_DRF(_CPWR_THERM, _TEMP_SENSOR_GPC, _STATE, _VALID,
                            reg32))
        {
            // Colwert from SFP9.5 -> LwTemp (SFP24.8).
            gpcTemp = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(_CPWR_THERM,
                           _TEMP_SENSOR_GPC, _FIXED_POINT, reg32);
        }
        else
        {
            // HW returs Temperature as INVALID, Throw error but avoid breakpoint here.
            status = FLCN_ERR_ILWALID_STATE;
            goto thermGetGpuGpcCombinedTemp_GA10X_exit;
        }
    }
    else
    {
        // Provider Index is Invalid.
        status = FLCN_ERR_ILWALID_INDEX;
        PMU_BREAKPOINT();
        goto thermGetGpuGpcCombinedTemp_GA10X_exit;
    }

    // Populate GPC Temp.
    *pTemp = gpcTemp;

thermGetGpuGpcCombinedTemp_GA10X_exit:
    return status;
}

/*!
 * @brief       Set SENSOR_9 Temperature source over I2CS.
 *
 * @param[in]   i2csSource   Temperature source to set.
 *
 * @return      FLCN_ERR_ILWALID_INDEX      I2CS source index is Invalid.
 * @return      FLCN_OK                     I2CS source set success.
 */
FLCN_STATUS
thermSensor9I2csSourceSet_GA10X
(
    LwU8 i2csSource
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       reg32;

    // Read initial settings.
    reg32 = REG_RD32(CSB, LW_CPWR_THERM_SENSOR_9);

    // Modify Register.
    switch (i2csSource)
    {
        case LW_CPWR_THERM_SENSOR_9_I2CS_GPU_MAX:
        {
            reg32 = FLD_SET_DRF(_CPWR_THERM, _SENSOR_9, _I2CS, _GPU_MAX, reg32); 
            break;
        }

        case LW_CPWR_THERM_SENSOR_9_I2CS_GPU_AVG:
        {
            reg32 = FLD_SET_DRF(_CPWR_THERM, _SENSOR_9, _I2CS, _GPU_AVG, reg32); 
            break;
        }

        default:
        {
            status = FLCN_ERR_ILWALID_INDEX;
            PMU_BREAKPOINT();
            break;
        }
    }

    // Update I2CS source Register.
    if (status == FLCN_OK)
    {
        REG_WR32(CSB, LW_CPWR_THERM_SENSOR_9, reg32);
    }

    return status;
}

/*!
 * @brief       Override SYS TSENSE Temperature.
 *
 * @param[in]   gpuTemp Temperature to be faked in FXP24.8.
 */
void
thermSysTsenseTempSet_GA10X
(
    LwTemp gpuTemp
)
{
    LwU32 reg32 = 0;

    // Modify Register, Read not required as Register is dedicated to override.
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _SYS_TSENSE_OVERRIDE, _TEMP_VALUE,
                            RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(gpuTemp), reg32);

    reg32 = FLD_SET_DRF(_CPWR_THERM, _SYS_TSENSE_OVERRIDE, _TEMP_SELECT,
                            _OVERRIDE, reg32);

    // Update Register and override Sys Tsense Temperature.
    REG_WR32(CSB, LW_CPWR_THERM_SYS_TSENSE_OVERRIDE, reg32);
}
