/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clkmonga100.c
 * @brief  PMU Hal Functions for handling chips specific implementation for
 *          clock monitors.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "config/g_clk_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
// 'UL' Addresses Misra violation 10.8 for an expression below
#define CLK_CLOCK_MON_XTAL_FREQ_MHZ                         27U
#define CLK_CLOCK_MON_XTAL4X_FREQ_MHZ                      108U

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Obtains the reference frequency for clock monitors
 *
 * @param[in]  clkApiDomain     Clock domain @ref LW2080_CTRL_CLK_DOMAIN_<xyz>
 * @param[out] pClkFreqMHz      Buffer to return the reference frequency
 * 
 * @return FLCN_OK  Always
 */
FLCN_STATUS
clkClockMonRefFreqGet_GA100
(
    LwU32   clkApiDomain,
    LwU32  *pClkFreqMHz
)
{
    //
    // Except UTILSCLK all the other clock monitors have been moved to using
    // XTAL4X as reference frequency.
    //
    if (clkApiDomain == LW2080_CTRL_CLK_DOMAIN_UTILSCLK)
    {
        *pClkFreqMHz = CLK_CLOCK_MON_XTAL_FREQ_MHZ;
    }
    else
    {
        *pClkFreqMHz = CLK_CLOCK_MON_XTAL4X_FREQ_MHZ;
    }

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
