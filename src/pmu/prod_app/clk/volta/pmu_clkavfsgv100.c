/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clkavfsgv100.c
 * @brief  PMU Hal Functions for GV100 for Clocks AVFS
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_trim.h"

/* ------------------------- Application Includes --------------------------- */
#include "dbgprintf.h"
#include "pmu_objclk.h"
#include "pmu_objpmu.h"
#include "config/g_clk_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Get bitmask of all supported NAFLLs on this chip
 *
 * @param[out]   pMask     Pointer to the bitmask
 */
void
clkNafllAllMaskGet_GV100
(
    LwU32   *pMask
)
{
    *pMask = BIT32(LW2080_CTRL_CLK_NAFLL_ID_SYS) |
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_XBAR)|
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC0)|
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC1)|
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC2)|
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC3)|
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC4)|
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPC5)|
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_GPCS)|
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_LWD) |
             BIT32(LW2080_CTRL_CLK_NAFLL_ID_HOST);
}


/* ------------------------- Private Functions ------------------------------ */
