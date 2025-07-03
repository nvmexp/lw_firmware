
/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_grgmxxxtuxxx.c
 * @brief  HAL-interface for GKXXX/GMXXX/GPXXX/TUXXX Graphics Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objpg.h"
#include "pmu_objgr.h"
#include "dev_master.h"
#include "pmu_bar0.h"
#include "dbgprintf.h"
#include "config/g_gr_private.h"

/* ------------------------- Macros and Defines ----------------------------- */
// TODO: Move it to dev_master_addendum.h file
#ifndef LW_PMC_INTR_PGRAPH_PENDING
#define LW_PMC_INTR_PGRAPH_PENDING 0x00000001
#define LW_PMC_INTR_CE2_PENDING    0x00000001
#endif // LW_PMC_INTR_PGRAPH_PENDING

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Check for graphics sub system interrupts
 *
 * @return LW_TRUE  If Interrupt pending on GR
 *         LW_FALSE Otherwise
 */
LwBool
grIsIntrPending_GM10X(void)
{
    LwU32 intr0;
    LwU32 intr1;

    intr0 = REG_RD32(BAR0, LW_PMC_INTR(0));
    intr1 = REG_RD32(BAR0, LW_PMC_INTR(1));

    if (FLD_TEST_DRF(_PMC, _INTR, _PGRAPH, _PENDING, intr0) ||
        FLD_TEST_DRF(_PMC, _INTR, _CE2,    _PENDING, intr0) ||
        FLD_TEST_DRF(_PMC, _INTR, _PGRAPH, _PENDING, intr1) ||
        FLD_TEST_DRF(_PMC, _INTR, _CE2,    _PENDING, intr1))
    {
        return LW_TRUE;
    }

    return LW_FALSE;
}
