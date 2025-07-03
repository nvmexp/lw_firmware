
/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_grgkxxxgpxxx.c
 * @brief  HAL-interface for the GKXXX/GMXXX/GPXXX Graphics Engine.
 */
/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objpg.h"
#include "pmu_objgr.h"

#include "dev_ltc.h"
#include "dev_graphics_nobundle.h"
#include "dev_graphics_nobundle_addendum.h"

#include "config/g_gr_private.h"

/* ------------------------- Macros and Defines ----------------------------- */
//
// GR L1 scrub time duration in usec
// Each L1 partition scrub can take 256 cycles and works on gpcclk. Considering
// that we can have ELPG cycles when the gpcclk is in slowdown (worst case
// 3MHZ, but round down to 1MHZ), the max duration should be (1 * 256 = 256us)
//
#define GR_MAX_L1_ECC_CSR_SCRUB_DURATION_US_GMXXX (256)

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*
 * @brief Scrub GR L1 before restoring ELPG state during ELPG exit.
 * Only required for kepler.
 */
void
grPgL1Scrub_GMXXX(void)
{

    REG_FLD_WR_DRF_DEF(BAR0, _PGRAPH, _PRI_GPCS_TPCS_L1C_ECC_CSR, _SCRUB, _PENDING);

    if (!PMU_REG32_POLL_US(
        LW_PGRAPH_PRI_GPCS_TPCS_L1C_ECC_CSR,
        DRF_SHIFTMASK(LW_PGRAPH_PRI_GPCS_TPCS_L1C_ECC_CSR_SCRUB),
        DRF_DEF(_PGRAPH, _PRI_GPCS_TPCS_L1C_ECC_CSR, _SCRUB, _NOT_PENDING),
        GR_MAX_L1_ECC_CSR_SCRUB_DURATION_US_GMXXX,
        PMU_REG_POLL_MODE_BAR0_EQ))
    {
        //
        // If this breakpoint is hit, then check the SCRUB_DURATION_US. Each
        // L1 partition can take a max of 256 gpcclk cycles. Considering that
        // we can go to as low as 1MHz gpcclk, 1 * 256 = 256 us, should be
        // enough for the scrub to complete.
        //
        PMU_HALT();
    }
}

/* ------------------------- Private Functions ------------------------------ */
