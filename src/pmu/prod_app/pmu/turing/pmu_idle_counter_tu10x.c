/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_idle_counter_tu10x.c
 * @brief   PMU HAL functions for dealing with idle counters for TU10X.
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "dev_pwr_csb.h"

#include "config/g_pmu_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Setup a specified idle-counter.
 */
FLCN_STATUS
pmuIdleCounterInit_TU10X
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
    REG_WR32(CSB, LW_CPWR_PMU_IDLE_MASK_2((LwU32)index), idleMask2);
    REG_WR32(CSB, LW_CPWR_PMU_IDLE_CTRL((LwU32)index),
        DRF_DEF(_CPWR_PMU, _IDLE_CTRL, _VALUE, _BUSY));

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
