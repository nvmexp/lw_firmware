/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_pgtu10x.c
 * @brief  PMU Power Gating Object
 *
 * PG - VOLTA PG
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objpg.h"
#include "pmu_objpmu.h"

#include "config/g_pg_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions  ------------------------------ */

/*!
 * @brief Enable/Disable holdoff interrupts for requested engines
 *
 * @param[in]   holdoffEngMask  Mask of engine IDs for which holdoff
 *                              interrupt needs to be enabled/disabled
 * @param[in]   bEnable         Determines whether holdoff interrupts
 *                              need to be enabled or disabled for 
 *                              requested engines.
 */
void
pgEnableHoldoffIntr_TU10X
(
    LwU32  holdoffEngMask,
    LwBool bEnable
)
{
#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    LwU32 holdoffIntrSettings;

    holdoffIntrSettings = REG_RD32(CSB, LW_CPWR_THERM_ENG_HOLDOFF_INTR_EN);

    if (bEnable)
    {
        holdoffIntrSettings |= holdoffEngMask;
    }
    else
    {
        holdoffIntrSettings &= ~holdoffEngMask;
    }

    REG_WR32_STALL(CSB, LW_CPWR_THERM_ENG_HOLDOFF_INTR_EN, holdoffIntrSettings);
#endif
}
