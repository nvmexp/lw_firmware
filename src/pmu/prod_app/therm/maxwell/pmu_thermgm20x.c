/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_thermgm20x.c
 * @brief   PMU HAL functions for GM20X+ for misc. thermal functionality
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"

#include "config/g_therm_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Load tach functionality (init HW state).
 *
 * @param[in]   tachSource  Requested TACH source as PMU_PMGR_TACH_SOURCE_<xyz>
 * @param[in]   tachRate    Tachometer Rate (PPR).
 *
 * @return FLCN_OK
 *      Tachometer loaded successfully.
 *
 * @note Input parameter @ref tachSource is ignored as HW only supports
 *       PMU_PMGR_THERM_TACH_SOURCE_0.
 */
FLCN_STATUS
thermTachLoad_GM20X
(
    PMU_PMGR_TACH_SOURCE    tachSource,
    LwU8                    tachRate
)
{
    LwU32 tach0;

    tach0 = REG_RD32(CSB, LW_CPWR_THERM_FAN_TACH0);
    tach0 = FLD_SET_DRF_NUM(_CPWR_THERM, _FAN_TACH0, _WIN_LENGTH,
                HIGHESTBITIDX_32(tachRate), tach0);
    REG_WR32(CSB, LW_CPWR_THERM_FAN_TACH0, tach0);

    return FLCN_OK;
}

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
thermGetTachFreq_GM20X
(
    PMU_PMGR_TACH_SOURCE    tachSource,
    LwU32                  *pNumerator,
    LwU32                  *pDenominator
)
{
    LwU32 tach0;

    tach0 = REG_RD32(CSB, LW_CPWR_THERM_FAN_TACH0);

    //
    // Reported period is in units of 1 microseconds. From GM20X, HW is capable
    // of monitoring long periods as specified by window length so pre-multiply
    // numerator by window length to account for tach rate division done by SW
    // while callwlating edgecount per second.
    //
    *pNumerator = 1000000U << DRF_VAL(_CPWR_THERM, _FAN_TACH0, _WIN_LENGTH, tach0);

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

/* ------------------------- Private Functions ------------------------------ */

