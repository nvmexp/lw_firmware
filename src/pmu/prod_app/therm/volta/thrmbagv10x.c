/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    thrmbagv10x.c
 * @brief   Thermal PMU HAL functions for GV10X+
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_pwr_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"

#include "config/g_therm_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Initialize BA debug
 *
 * @return LW_OK      debug setup was successful.
 */
FLCN_STATUS
thermBaInitDebug_GV10X(void)
{
    /*
     * The PEAKPOWER_DEBUG0 register directs BA power estimates to the DP2 interface
     * for debug purposes. HW requested that the DP2 interface be hardcoded to output
     * the power estimates for the 0th BA window, show the total power estimated by
     * that window, and be right shifted by 5 bits.
     * See Bug 2352797, comment #10.
     */
    LwU32 reg = DRF_DEF(_CPWR_THERM, _PEAKPOWER_DEBUG0, _WINDOW_IDX, _0) |
                DRF_DEF(_CPWR_THERM, _PEAKPOWER_DEBUG0, _DP2_SELECT, _TOTAL) |
                DRF_NUM(_CPWR_THERM, _PEAKPOWER_DEBUG0, _DP2_SHIFT, 5);

    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_DEBUG0, reg);

    return FLCN_OK;
}
