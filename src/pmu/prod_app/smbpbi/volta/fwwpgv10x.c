/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019  by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    fwwpgv10x.c
 * @brief   PMU HAL functions for GV10X, handling SMBPBI queries for
 *          firmware write protection info.
 */

/* ------------------------ System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu_objsmbpbi.h"
#include "dev_bus.h"
#include "dev_bus_addendum.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"

/* ------------------------ Application Includes --------------------------- */
#include "pmu_bar0.h"

#include "config/g_smbpbi_private.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Function Prototypes  -------------------- */
/* ------------------------ Static Function Prototypes  -------------------- */
/* ------------------------ Public Functions  ------------------------------ */

/*!
 * @brief   Read status of firmware write protection mode
 * @param[in]   pMode   Status of write protection (enabled/disabled)
 */
void
smbpbiGetFwWriteProtMode_GV10X
(
    LwBool *pMode
)
{
    LwU32 reg = REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_1);
    *pMode = FLD_TEST_REF(LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_1_WRITE_PROTECT_MODE, _ENABLED, reg);
}
