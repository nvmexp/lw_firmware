/*
 * Copyright 2009-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_idle_counter_GMXXX.c
 * @brief  PMU Hal Functions for dealing with idle counters for GK10X
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"

#include "config/g_pmu_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/*!
 * Get the current value of a specified idle-counter register.
 *
 * @param[in]  index  Index of the idle-counter register
 */
LwU32
pmuIdleCounterGet_GMXXX
(
    LwU8 index
)
{
    // Do a simple bounds check on the index.
    if (index >= LW_CPWR_PMU_IDLE_COUNT__SIZE_1)
    {
        PMU_HALT();
        return 0;
    }
    return REG_FLD_IDX_RD_DRF(CSB, _CPWR, _PMU_IDLE_COUNT, (LwU32)index, _VALUE);
}

/*!
 * Reset a specified idle-counter register.
 *
 * @param[in]  index  Index of the idle-counter register
 */
void
pmuIdleCounterReset_GMXXX
(
    LwU8 index
)
{
    // Do a simple bounds check on the index.
    if (index >= LW_CPWR_PMU_IDLE_COUNT__SIZE_1)
    {
        PMU_HALT();
        return;
    }
    REG_WR32(CSB, LW_CPWR_PMU_IDLE_COUNT((LwU32)index),
        DRF_NUM(_CPWR_PMU, _IDLE_COUNT, _RESET, 1));
}
