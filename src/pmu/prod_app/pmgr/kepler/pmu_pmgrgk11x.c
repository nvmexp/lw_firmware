/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_pmgrgk11x.c
 * @brief  Contains all PCB Management (PGMR) routines specific to GK11X+.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pmgr.h"

/* ------------------------- Application Includes --------------------------- */
#include "lwosreg.h"
#include "pmu_objpmu.h"
#include "pmu_objpmgr.h"
#include "therm/objtherm.h"
#include "lib/lib_pwm.h"

#include "config/g_pmgr_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions  ------------------------------ */
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
 */
FLCN_STATUS
pmgrGetTachFreq_GMXXX
(
    PMU_PMGR_TACH_SOURCE    tachSource,
    LwU32                  *pNumerator,
    LwU32                  *pDenominator
)
{
    // On GK110+ we're using new, higher precision tachometer circuit.
    return thermGetTachFreq_HAL(&Therm, tachSource, pNumerator, pDenominator);
}

#ifdef LW_PMGR_GPIO_INPUT_FUNC_TACH
/*!
 * @brief Load tach functionality (init HW state).
 *
 * @param[in]   tachSource      Requested TACH source as PMU_PMGR_TACH_SOURCE_<xyz>
 * @param[in]   gpioRequested   Requested GPIO pin to load tachometer
 *
 * @return FLCN_OK
 *
 * @note Input parameter @ref tachSource is ignored as HW only supports
 *       PMU_PMGR_PMGR_TACH_SOURCE.
 */
FLCN_STATUS
pmgrTachLoad_GMXXX
(
    PMU_PMGR_TACH_SOURCE    tachSource,
    LwU8                    gpioRequested
)
{
    static const LwU32  hwEnumIndex = LW_PMGR_GPIO_INPUT_FUNC_TACH;
    LwU8                gpioLwrrent;

    // Get the pin on which TACH is right now.
    gpioLwrrent = REG_FLD_IDX_RD_DRF(BAR0, _PMGR, _GPIO_INPUT_CNTL, hwEnumIndex, _PINNUM);

    // If current assignment does not match DCB.
    if (gpioLwrrent != gpioRequested)
    {
        // Program TACH on requested pin. Recovery.
        // NJ-TODO: Contains redundant read
        REG_FLD_IDX_WR_DRF_NUM(BAR0, _PMGR, _GPIO_INPUT_CNTL, hwEnumIndex, _PINNUM, gpioRequested);
    }

    return FLCN_OK;
}
#endif

/*!
 * @brief Map TACH GPIO pin to TACH source.
 *
 * @param[in]  gpioPin  Requested TACH GPIO pin for mapping
 *
 * @return PMU_PMGR_PMGR_TACH_SOURCE
 */
PMU_PMGR_TACH_SOURCE
pmgrMapTachPinToTachSource_GMXXX
(
    LwU8    tachPin
)
{
    return  PMU_PMGR_THERM_TACH_SOURCE_0;
}

