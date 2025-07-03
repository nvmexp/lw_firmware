/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_idle_counter_gp10x.c
 *          PMU HAL Functions for dealing with idle counters for GP10x and later
 *          GPUs
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "pmusw.h"
#include "pmu_objpmu.h"
#include "dev_pwr_csb.h"
#include "dev_fbpa.h"

#include "config/g_pmu_private.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Get the current value of a specified idle-counter register, and the
 *        difference since previous.
 *
 * @param[in]  index  Index of the idle-counter register
 */
FLCN_STATUS
pmuIdleCounterDiff_GP10X
(
    LwU8   index,
    LwU32 *pDiff,
    LwU32 *pLast
)
{
    LwU32 old = *pLast;

    // Do a simple bounds check on the index.
    if (index >= LW_CPWR_PMU_IDLE_COUNT__SIZE_1)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_INDEX;
    }

    *pLast = REG_FLD_IDX_RD_DRF(CSB, _CPWR, _PMU_IDLE_COUNT, (LwU32)index, _VALUE);

    if (pDiff != NULL)
    {
        // Bit-wise-and with mask to handle overflow (not a full 32 bits value).
        *pDiff = (*pLast - old) & DRF_MASK(LW_CPWR_PMU_IDLE_COUNT_VALUE);
    }

    return FLCN_OK;
}

/*!
 * @brief Get the total number of HW idle-counters.
 */
LwU32
pmuIdleCounterSize_GP10X(void)
{
    return LW_CPWR_PMU_IDLE_COUNT__SIZE_1;
}

/*!
 * @brief Setup a specified idle-counter.
 */
FLCN_STATUS
pmuIdleCounterInit_GP10X
(
    LwU8    index,
    LwU32   idleMask0,
    LwU32   idleMask1,
    LwU32   idleMask2
)
{
    // Do a simple bounds check on the index.
    if (index >= LW_CPWR_PMU_IDLE_COUNT__SIZE_1)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_INDEX;
    }

    REG_WR32(CSB, LW_CPWR_PMU_IDLE_MASK((LwU32)index),   idleMask0);
    REG_WR32(CSB, LW_CPWR_PMU_IDLE_MASK_1((LwU32)index), idleMask1);
    REG_WR32(CSB, LW_CPWR_PMU_IDLE_CTRL((LwU32)index),
        DRF_DEF(_CPWR_PMU, _IDLE_CTRL, _VALUE, _BUSY));

    return FLCN_OK;
}

/*!
 * @brief Setup PMU FB idle-counter PWM signal.
 */
FLCN_STATUS
pmuIdleCounterFbInit_GP10X(void)
{
    LwU32 miscCtrl;
    LwU32 old;

    old = miscCtrl = REG_RD32(FECS, LW_PFB_FBPA_MISC_CTRL);
    miscCtrl = FLD_SET_DRF(_PFB_FBPA, _MISC_CTRL, _INTERVAL_TIMER_EN, _ENABLED, miscCtrl);

    if (old != miscCtrl)
    {
        REG_WR32(FECS, LW_PFB_FBPA_MISC_CTRL, miscCtrl);
    }

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
