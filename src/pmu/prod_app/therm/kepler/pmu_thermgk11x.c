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
 * @file    pmu_thermgk11x.c
 * @brief   PMU HAL functions for GK11X+ for misc. thermal functionality
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu_oslayer.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"
#include "pmu_objpmu.h"

#include "config/g_therm_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
#ifdef LW_CPWR_THERM_TS_TEMP
/*!
 * @brief   Get GPU temp. of internal temp-sensor.
 *
 * @param[in] thermDevProvIdx Device provider index to get the temperature
 *
 * @return  GPU temperature in 24.8 fixed point notation in [C]
 */
LwTemp
thermGetTempInternal_GM10X
(
    LwU8 thermDevProvIdx
)
{
    // Colwert from SFP9.5 -> LwTemp (SFP24.8).
    return RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(
        _CPWR_THERM, _TS_TEMP, _FIXED_POINT,
        REG_RD32(CSB, LW_CPWR_THERM_TS_TEMP));
}
#endif // LW_CPWR_THERM_TS_TEMP

#ifdef LW_CPWR_THERM_FAN_TACH0
/*!
 * @brief   Get tachometer frequency as edgecount per sec.
 *
 * Reading is to be interpreted as numerator / denominator.
 * Returning this way to avoid truncation issues.
 *
 * @param[in]   tachSource      Requested TACH source as PMU_PMGR_TACH_SOURCE_<xyz>
 * @param[out]  pNumerator      Edgecount per sec numerator
 * @param[out]  pDenominator    Edgecount per sec denominator
 *
 * @return FLCN_OK
 *      Tachometer frequency obtained successfully.
 *
 * @note Input parameter @ref tachSource is ignored as HW only supports
 *       PMU_PMGR_THERM_TACH_SOURCE_0.
 */
FLCN_STATUS
thermGetTachFreq_GM10X
(
    PMU_PMGR_TACH_SOURCE    tachSource,
    LwU32                  *pNumerator,
    LwU32                  *pDenominator
)
{
    LwU32 tach0;

    tach0 = REG_RD32(CSB, LW_CPWR_THERM_FAN_TACH0);

    // Reported period is in units of 1 microseconds.
    *pNumerator = 1000000;

    if (FLD_TEST_DRF(_CPWR_THERM, _FAN_TACH0, _OVERFLOW, _DETECTED, tach0))
    {
        // Fan is stalled, clear state and report it.
        REG_WR32(CSB, LW_CPWR_THERM_FAN_TACH0,
            FLD_SET_DRF(_CPWR_THERM, _FAN_TACH0, _OVERFLOW, _CLEAR, tach0));
        *pDenominator = 0;
    }
    else if (FLD_TEST_DRF(_CPWR_THERM, _FAN_TACH0, _PERIOD, _MIN, tach0) ||
             FLD_TEST_DRF(_CPWR_THERM, _FAN_TACH0, _PERIOD, _MAX, tach0))
    {
        //
        // Check for boundary conditions. Fan tachometer reporting MAX
        // can be treated as garbage.
        //
        *pDenominator = 0;
    }
    else
    {
        // Now obtain the legitimate value.
        *pDenominator = DRF_VAL(_CPWR_THERM, _FAN_TACH0, _PERIOD, tach0);
    }

    return FLCN_OK;
}
#endif // LW_CPWR_THERM_FAN_TACH0

/* ------------------------- Private Functions ------------------------------ */

