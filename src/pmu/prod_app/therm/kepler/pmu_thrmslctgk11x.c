/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_thrmslctgk11x.c
 * @brief   PMU HAL functions for GK11X+ for thermal slowdown control
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"
#include "pmu_objtimer.h"
#include "pmu_objpmu.h"

#include "config/g_therm_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * For the time being recommending 500us for a timeout value.
 */
#define PMU_THERM_MIN_STABLE_SLOWDOWN_DELAY_US  500

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
#if (PMU_PROFILE_GM10X || PMU_PROFILE_GM20X)
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
thermSlctSetEventTempThreshold_GM20X
(
    LwU8    eventId,
    LwTemp  tempThreshold
)
{
    LwS32   celsius;
    LwU32   reg32;

    // Ensure that eventId is applicable to this use case.
    if (!RM_PMU_THERM_IS_BIT_SET(eventId,
                                 LW_CPWR_THERM_EVENT_MASK_TEMP_THRESHOLD))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    celsius = RM_PMU_LW_TEMP_TO_CELSIUS_TRUNCED(tempThreshold);

    if (RM_PMU_THERM_IS_BIT_SET(eventId,
                                LW_CPWR_THERM_EVENT_MASK_TEMP_OFFSET_MODE_NEG))
    {
        celsius += LW_CPWR_THERM_I2CS_SENSOR_14_NEG_TEMP_OFFSET_INIT;
    }

    if (celsius < LW_CPWR_THERM_EVT_SETTINGS_TEMP_THRESHOLD_MIN)
    {
        celsius = LW_CPWR_THERM_EVT_SETTINGS_TEMP_THRESHOLD_MIN;
    }
    else if (celsius > LW_CPWR_THERM_EVT_SETTINGS_TEMP_THRESHOLD_MAX)
    {
        celsius = LW_CPWR_THERM_EVT_SETTINGS_TEMP_THRESHOLD_MAX;
    }

    reg32 = REG_RD32(CSB, LW_CPWR_THERM_EVT_SETTINGS(eventId));
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _EVT_SETTINGS, _TEMP_THRESHOLD,
                            celsius, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_EVT_SETTINGS(eventId), reg32);

    return FLCN_OK;
}
#endif // (PMU_PROFILE_GM10X || PMU_PROFILE_GM20X)

/*!
 * @brief   Retrieves event slowdown factor
 *
 * @param[in]   eventId     RM_PMU_THERM_EVENT_<xyz> HW failsafe event enum
 * @param[out]  pFactor     container to hold the result factor
 *
 * @return  FLCN_ERR_ILWALID_ARGUMENT    if invalid eventId was passed
 * @return  FLCN_OK                     otherwise
 */
FLCN_STATUS
thermSlctGetEventSlowdownFactor_GM10X
(
    LwU8    eventId,
    LwU8   *pFactor
)
{
    // Ensure that eventId is applicable to this use case.
    if (!RM_PMU_THERM_IS_BIT_SET(eventId, LW_CPWR_THERM_EVENT_MASK_HW_FAILSAFE))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    *pFactor = (LwU8)DRF_VAL(_CPWR_THERM, _EVT_SETTINGS, _SLOW_FACTOR,
                        REG_RD32(CSB, LW_CPWR_THERM_EVT_SETTINGS(eventId)));

    return FLCN_OK;
}

/*!
 * @brief   Set event slowdown factor
 *
 * @param[in]   eventId     RM_PMU_THERM_EVENT_<xyz> HW failsafe event enum
 * @param[in]   factor      new slowdown factor value for specified eventId
 *
 * @return  FLCN_ERR_ILWALID_ARGUMENT    if invalid eventId was passed
 * @return  FLCN_OK                     otherwise
 */
FLCN_STATUS
thermSlctSetEventSlowdownFactor_GM10X
(
    LwU8    eventId,
    LwU8    factor
)
{
    LwU32   reg32;

    // Ensure that eventId is applicable to this use case.
    if (!RM_PMU_THERM_IS_BIT_SET(eventId, LW_CPWR_THERM_EVENT_MASK_HW_FAILSAFE))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    if (factor > LW_CPWR_THERM_FPDIV_MAX)
    {
        factor = LW_CPWR_THERM_FPDIV_MAX;
    }

    reg32 = REG_RD32(CSB, LW_CPWR_THERM_EVT_SETTINGS(eventId));
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _EVT_SETTINGS, _SLOW_FACTOR,
                            factor, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_EVT_SETTINGS(eventId), reg32);

    return FLCN_OK;
}
