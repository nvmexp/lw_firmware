/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_thrm_gp10x_ga100.c
 * @brief   Thermal PMU HAL functions for GP10X till GA100.
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"

#include "config/g_therm_private.h"

/*!
 * @brief   Read event temperature threshold, negative hotspot offset and sensorId
 *
 * @param[in]   eventId         RM_PMU_THERM_EVENT_* thermal event
 * @param[out]  pTempThreshold  ptr to threshold temperature associated
 *                              with thespecified eventId
 * @param[out]  pNegHSOffset    optional ptr to the HotSpot offset value
 * @param[out]  pSensorId       optional ptr to the sensor Id value
 *
 * @return  FLCN_ERR_ILWALID_ARGUMENT    if invalid eventId was passed
 * @return  FLCN_ERR_NOT_SUPPORTED       if eventId is unused/inactive
 * @return  FLCN_OK                     otherwise
 */
FLCN_STATUS
thermSlctGetEventTempThresholdHSOffsetAndSensorId_GP10X
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
            if (sensorId == LW_CPWR_THERM_EVT_SETTINGS_TEMP_SENSOR_ID_OFFSET_TSENSE)
            {
                *pNegHSOffset = -RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
                                _CPWR_THERM, _SENSOR_8, _TSENSE_OFFSET,
                                REG_RD32(CSB, LW_CPWR_THERM_SENSOR_8));
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

    return status;
}
