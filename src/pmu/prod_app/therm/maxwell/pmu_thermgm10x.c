/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_thermgm10x.c
 * @brief   PMU HAL functions for GM10X+ for misc. thermal functionality
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_pwr_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"

#include "config/g_therm_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * @brief   Maps GPU-HAL independent to GPU-HAL dependent THERM event mask.
 *
 * @note    Avoiding to create HAL for single use-case mapping, since we do not
 *          need this for pre-Maxwell chips where implementation is ugly.
 */
#define thermEventMapGenericToHwMask_HAL(pTherm, mask)                         \
                                        ((mask) & (pTherm)->evt.maskSupportedHw)

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Applies into HW RM_PMU_SW_SLOW slowdown factor for given CLK index.
 *
 * @param[in]   clkIndex    Thermal index of the affected clock
 * @param[in]   factor      New slowdown factor for given clock
 */
void
thermSlctRmPmuSwSlowHwSet_GM10X
(
    LwU8    clkIndex,
    LwU8    factor
)
{
    LwU32   reg32;

    reg32 = REG_RD32(CSB, LW_CPWR_THERM_CLK_SLOWDOWN_1((LwU32)clkIndex));
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _CLK_SLOWDOWN_1, _SW_THERM_FACTOR,
                            factor, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_CLK_SLOWDOWN_1((LwU32)clkIndex), reg32);
}

/*!
 * @brief   Enables/disables slowdown events.
 *
 * @param[in]   maskEnabled
 *      A generic RM_PMU_THERM_EVENT_<xyz> mask of enabled slowdown events.
 */
FLCN_STATUS
thermSlctEventsEnablementSet_GM10X
(
    LwU32   maskEnabled
)
{
    LwU32   reg32 = 0;

    // Handle HW failsafe slowdowns.
    REG_WR32(CSB, LW_CPWR_THERM_USE_A,
             thermEventMapGenericToHwMask_HAL(&Therm, maskEnabled));

    // Handle SW emulated slowdowns.
    if (RM_PMU_THERM_IS_BIT_SET(RM_PMU_THERM_EVENT_HW_FAILSAFE, maskEnabled))
    {
        reg32 = FLD_SET_DRF(_CPWR_THERM, _POWER_SLOWDOWN_FACTOR,
                            _HW_FAILSAFE_FORCE_DISABLE, _NO, reg32);
    }
    else
    {
        reg32 = FLD_SET_DRF(_CPWR_THERM, _POWER_SLOWDOWN_FACTOR,
                            _HW_FAILSAFE_FORCE_DISABLE, _YES, reg32);
    }
    if (RM_PMU_THERM_IS_BIT_SET(RM_PMU_THERM_EVENT_IDLE_SLOW, maskEnabled))
    {
        reg32 = FLD_SET_DRF(_CPWR_THERM, _POWER_SLOWDOWN_FACTOR,
                            _IDLE_FORCE_DISABLE, _NO, reg32);
    }
    else
    {
        reg32 = FLD_SET_DRF(_CPWR_THERM, _POWER_SLOWDOWN_FACTOR,
                            _IDLE_FORCE_DISABLE, _YES, reg32);
    }
    if (RM_PMU_THERM_IS_BIT_SET(RM_PMU_THERM_EVENT_SW_RM_PMU, maskEnabled))
    {
        reg32 = FLD_SET_DRF(_CPWR_THERM, _POWER_SLOWDOWN_FACTOR,
                            _SW_FORCE_DISABLE, _NO, reg32);
    }
    else
    {
        reg32 = FLD_SET_DRF(_CPWR_THERM, _POWER_SLOWDOWN_FACTOR,
                            _SW_FORCE_DISABLE, _YES, reg32);
    }
    REG_WR32(CSB, LW_CPWR_THERM_POWER_SLOWDOWN_FACTOR, reg32);

    return FLCN_OK;
}
