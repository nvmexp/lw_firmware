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
 * @file    pmu_thrm_gp10x.c
 * @brief   Thermal PMU HAL functions for GP10X+
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmu_oslayer.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"

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
 * @brief   Set event temperature threshold
 *
 * @param[in]   eventId         RM_PMU_THERM_EVENT_* thermal event
 * @param[in]   tempThreshold   threshold temperature associated with the
 *                              specified eventId
 *
 * @return  FLCN_ERR_ILWALID_ARGUMENT    if invalid eventId was passed
 * @return  FLCN_OK                     otherwise
 */
FLCN_STATUS
thermSlctSetEventTempThreshold_GP10X
(
    LwU8    eventId,
    LwTemp  tempThreshold
)
{
    LwTemp  temp;
    LwU32   reg32;

    // Ensure that eventId is applicable to this use case.
    if (!RM_PMU_THERM_IS_BIT_SET((LwU32)eventId, LW_CPWR_THERM_EVENT_MASK_THERMAL))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Colwert from SFP9.5 -> LwTemp (SFP24.8).
    temp = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_DEF(
        _CPWR_THERM, _EVT_SETTINGS, _TEMP_THRESHOLD, _MIN);
    if (tempThreshold < temp)
    {
        tempThreshold = temp;
    }

    // Colwert from SFP9.5 -> LwTemp (SFP24.8).
    temp = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_DEF(
        _CPWR_THERM, _EVT_SETTINGS, _TEMP_THRESHOLD, _MAX);
    if (tempThreshold > temp)
    {
        tempThreshold = temp;
    }

    // RMW on signed field using unsigned macro!!!
    reg32 = REG_RD32(CSB, LW_CPWR_THERM_EVT_SETTINGS((LwU32)eventId));
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _EVT_SETTINGS, _TEMP_THRESHOLD,
                            RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(tempThreshold),
                            reg32);
    REG_WR32(CSB, LW_CPWR_THERM_EVT_SETTINGS((LwU32)eventId), reg32);

    return FLCN_OK;
}

#ifdef LW_CPWR_THERM_MAXFAN_STATUS
/*!
 * @brief   Gets HW MAXFAN state (active/inactive) for the requested PWM source.
 *
 * @param[in] pwmSource   Requested PWM source as RM_PMU_PMGR_PWM_SOURCE_<xyz>
 *
 * @return  Boolean flag indicating HW MAXFAN status.
 *
 * @note Input parameter @ref pwmSource is ignored as HW doesn't support per
 *       PWM source HW MAXFAN status.
 */
LwBool
thermHwMaxFanStatusGet_GP10X
(
    RM_PMU_PMGR_PWM_SOURCE  pwmSource
)
{
    return FLD_TEST_DRF(_CPWR_THERM, _MAXFAN, _STATUS, _PENDING,
                        REG_RD32(CSB, LW_CPWR_THERM_MAXFAN));
}

/*!
 * @brief   Activate/Deactivate HW MAXFAN status for the requested PWM source.
 *
 * @param[in] pwmSource Requested PWM source as RM_PMU_PMGR_PWM_SOURCE_<xyz>
 * @param[in] bSet      Boolean flag to activate or deactivate.
 *
 * @note Input parameter @ref pwmSource is ignored as HW doesn't support per
 *       PWM source HW MAXFAN status.
 */
void
thermHwMaxFanStatusSet_GP10X
(
    RM_PMU_PMGR_PWM_SOURCE  pwmSource,
    LwBool                  bSet
)
{
    LwU32 regVal = REG_RD32(CSB, LW_CPWR_THERM_MAXFAN);

    regVal = bSet ?
                FLD_SET_DRF(_CPWR_THERM, _MAXFAN, _FORCE, _TRIGGER, regVal) :
                FLD_SET_DRF(_CPWR_THERM, _MAXFAN, _STATUS, _CLEAR, regVal);

    REG_WR32(CSB, LW_CPWR_THERM_MAXFAN, regVal);
}
#endif

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_MXM_OVERRIDE)
/*!
 * @brief   Applies MXM override to thermal POR settings.
 *
 * @param[in]   mxmMaxTemp      Max MXM Temp
 * @param[in]   mxmAssertTemp   Assert Temp
 *
 * PASCAL+ MXM POR settings:
 *  - THERMAL_5 is used to ASSERT
 *  - THERMAL_3 is used for TJ_THP1 and can be overrided by MXM-SIS MaxTemp
 *
 * @note    Keep in mind that HW reacts once temperature is greater than value
 *          in threshold register so we need to modify reads/writes by +1/-1.
 */
void
thermHwPorMxmOverride_GP10X
(
    LwTemp  mxmMaxTemp,
    LwTemp  mxmAssertTemp
)
{
    LwTemp  tjThp1;
    LwTemp  offsetTemp;
    LwU32   regVal;

    // Read HS offset. Colwert from SFP9.5 -> LwTemp (SFP24.8).
    offsetTemp = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
        _CPWR_THERM, _SENSOR_8, _TSENSE_OFFSET,
        REG_RD32(CSB, LW_CPWR_THERM_SENSOR_8));

    if (mxmMaxTemp != 0)
    {
        regVal = REG_RD32(CSB, LW_CPWR_THERM_EVT_SETTINGS(
                                RM_PMU_THERM_EVENT_THERMAL_3));

        // Colwert from SFP9.5 -> LwTemp (SFP24.8).
        tjThp1 = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
            _CPWR_THERM, _EVT_SETTINGS, _TEMP_THRESHOLD, regVal);

        //
        // MXM always uses TSENSE, but _THERMAL_3 source can be any
        // (TSENSE/OFFSET_TSENSE/CONST/MAX). So we can't directly compare
        // mxmMaxTemp with _THERMAL_3 threshold. If the temp source is
        // OFFSET_TSENSE then we have to add offset temp to mxmMaxTemp.
        // We are not considering MAX sensor since there is no use case
        // for that now.
        //
        if (FLD_TEST_DRF(_CPWR_THERM, _EVT_SETTINGS, _TEMP_SENSOR_ID,
                            _OFFSET_TSENSE, regVal))
        {
            mxmMaxTemp += offsetTemp;
        }

        if (mxmMaxTemp < tjThp1)
        {
            //
            // Directly set threshold instead of calling
            // thermSlctSetEventTempThreshold_HAL to save on extra priv read.
            //
            regVal =
                FLD_SET_DRF_NUM(_CPWR_THERM, _EVT_SETTINGS, _TEMP_THRESHOLD,
                                RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(mxmMaxTemp),
                                regVal);

            REG_WR32(CSB, LW_CPWR_THERM_EVT_SETTINGS(
                            RM_PMU_THERM_EVENT_THERMAL_3), regVal);
        }
    }

    if (mxmAssertTemp != 0)
    {
        regVal = REG_RD32(CSB, LW_CPWR_THERM_EVT_SETTINGS(
                                RM_PMU_THERM_EVENT_THERMAL_5));

        //
        // MXM always uses TSENSE, but _THERMAL_5 source can be any
        // (TSENSE/OFFSET_TSENSE/CONST/MAX). So we can't directly set
        // mxmAssertTemp with _THERMAL_5 threshold. If the temp source is
        // OFFSET_TSENSE then we have to add offset temp to mxmAssertTemp.
        // We are not considering MAX sensor since there is no use case
        // for that now.
        //
        if (FLD_TEST_DRF(_CPWR_THERM, _EVT_SETTINGS, _TEMP_SENSOR_ID,
                            _OFFSET_TSENSE, regVal))
        {
            mxmAssertTemp += offsetTemp;
        }

        //
        // Directly set threshold instead of calling
        // thermSlctSetEventTempThreshold_HAL to save on extra priv read.
        //
        regVal =
            FLD_SET_DRF_NUM(_CPWR_THERM, _EVT_SETTINGS, _TEMP_THRESHOLD,
                            RM_PMU_HW_TEMP_EXTRACT_FROM_LW_TEMP(mxmAssertTemp),
                            regVal);

        REG_WR32(CSB, LW_CPWR_THERM_EVT_SETTINGS(RM_PMU_THERM_EVENT_THERMAL_5),
                    regVal);
    }
    else
    {
        // System does not want GPU to assert TH_ALERT.
        regVal = REG_RD32(CSB, LW_CPWR_THERM_ALERT_EN);
        regVal =
            FLD_SET_DRF(_CPWR_THERM, _ALERT_EN, _THERMAL_5, _DISABLED, regVal);
        REG_WR32(CSB, LW_CPWR_THERM_ALERT_EN, regVal);
    }
}
#endif

/*!
 * @brief   Enable/Disable thermal monitor counter
 *
 * @param[in]   monIdx      Physical instance of the monitor.
 * @param[in]   bEnable     Enable/Disable count.
 */
void
thermMonEnablementSet_GP10X
(
    LwU8    monIdx,
    LwBool  bEnable
)
{
    LwU32 val = REG_RD32(CSB, LW_CPWR_THERM_INTR_MONITOR_CTRL((LwU32)monIdx));

    if (bEnable)
    {
        val = FLD_SET_DRF(_CPWR_THERM, _INTR_MONITOR_CTRL, _EN, _ENABLE, val);

        // Clear counter to 0.
        val = FLD_SET_DRF(_CPWR_THERM, _INTR_MONITOR_CTRL, _CLEAR, _TRIGGER, val);
    }
    else
    {
        val = FLD_SET_DRF(_CPWR_THERM, _INTR_MONITOR_CTRL, _EN, _DISABLE, val);
    }

    REG_WR32(CSB, LW_CPWR_THERM_INTR_MONITOR_CTRL((LwU32)monIdx), val);
}

/*!
 * @brief   Gets the type of the thermal monitor (level/edge).
 *
 * @param[in]   monIdx  Physical instance of the monitor.
 *
 * @return  LW2080_CTRL_THERMAL_THERM_MONITOR_TYPE_<xyz>.
 */
LwU8
thermMonTypeGet_GP10X
(
    LwU8 monIdx
)
{
    LwU32 val = REG_RD32(CSB, LW_CPWR_THERM_INTR_MONITOR_CTRL((LwU32)monIdx));

    if (FLD_TEST_DRF(_CPWR_THERM, _INTR_MONITOR_CTRL, _TYPE, _EDGE, val))
    {
        return LW2080_CTRL_THERMAL_THERM_MONITOR_TYPE_EDGE;
    }
    else if (FLD_TEST_DRF(_CPWR_THERM, _INTR_MONITOR_CTRL, _TYPE, _LEVEL, val))
    {
        return LW2080_CTRL_THERMAL_THERM_MONITOR_TYPE_LEVEL;
    }
    else
    {
        return LW2080_CTRL_THERMAL_THERM_MONITOR_TYPE_ILWALID;
    }
}

/*!
 * @brief   Returns number of Thermal Monitor instances
 */
LwU8
thermMonNumInstanceGet_GP10X(void)
{
    return LW_CPWR_THERM_INTR_MONITOR_CTRL__SIZE_1;
}

/*!
 * @brief   Update tracker[index] shared with the RM.
 *
 * @param[in]   segmentId   Index of the tracker to update
 * @param[in]   value       New value for the tracker to set
 *
 * @return  FLCN_OK                 Tracker sucessfully updated
 * @return  FLCN_ERR_ILWALID_INDEX  Tracker not supported
 */
FLCN_STATUS
thermLoggerTrackerSet_GP10X
(
    LwU8    segmentId,
    LwU32   value
)
{
    FLCN_STATUS status = FLCN_OK;

    // Assure that the logger code does not exceed HW capabilities.
    ct_assert(RM_PMU_LOGGER_SEGMENT_ID__COUNT <= LW_CPWR_THERM_SCRATCH__SIZE_1);

    if (segmentId >= RM_PMU_LOGGER_SEGMENT_ID__COUNT)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        PMU_TRUE_BP();
        goto thermLoggerTrackerSet_GP10X_exit;
    }

    REG_WR32(CSB, LW_CPWR_THERM_SCRATCH(segmentId), value);

thermLoggerTrackerSet_GP10X_exit:
    return status;
}

/* ------------------------- Static Functions ------------------------------- */
