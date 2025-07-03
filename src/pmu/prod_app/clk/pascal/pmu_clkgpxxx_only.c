/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clkgpxxx.c
 * @brief  PMU Hal Functions for Pascal (but not Volta) for Clocks.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_trim.h"
#include "dev_trim_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "config/g_clk_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */



/*!
 * @brief Get the mask of clock domains having 1x distribution mode.
 *
 * @return Clock domain mask
 */
LwU32
clkDist1xDomMaskGet_GP100()
{
    LwU32  clkDistReg = REG_RD32(FECS, LW_PTRIM_SYS_CLK_DIST_MODE);
    LwU32  domMask    = 0;

    if (FLD_TEST_DRF(_PTRIM_SYS, _CLK_DIST_MODE, _SYS2CLK_DIST_MODE,
                     _1X_PLL, clkDistReg))
    {
        domMask |= LW2080_CTRL_CLK_DOMAIN_SYS2CLK;
    }

    if (FLD_TEST_DRF(_PTRIM_SYS, _CLK_DIST_MODE, _LTC2CLK_DIST_MODE,
                     _1X_PLL, clkDistReg))
    {
        domMask |= LW2080_CTRL_CLK_DOMAIN_LTC2CLK;
    }

    if (FLD_TEST_DRF(_PTRIM_SYS, _CLK_DIST_MODE, _XBAR2CLK_DIST_MODE,
                     _1X_PLL, clkDistReg))
    {
        domMask |= LW2080_CTRL_CLK_DOMAIN_XBAR2CLK;
    }

    if (FLD_TEST_DRF(_PTRIM_SYS, _CLK_DIST_MODE, _GPC2CLK_DIST_MODE,
                     _1X_PLL, clkDistReg))
    {
        domMask |= LW2080_CTRL_CLK_DOMAIN_GPC2CLK;
    }

    return domMask;
}

/*!
 * Construct and initialize the SW state for CLK_FREQ_COUNTED_AVG feature.
 */
void
clkFreqCountedAvgConstruct_GP100()
{
    static CLK_FREQ_COUNTER clkFreqCounterArr_GP100[4] = {{ 0 }};
    LwU8 domIdx;
    LwU8 count = 0;

    Clk.freqCountedAvg.pFreqCntr = clkFreqCounterArr_GP100;
    Clk.freqCountedAvg.clkDomainMask = LW2080_CTRL_CLK_DOMAIN_GPC2CLK |
                                       LW2080_CTRL_CLK_DOMAIN_SYS2CLK |
                                       LW2080_CTRL_CLK_DOMAIN_LTC2CLK |
                                       LW2080_CTRL_CLK_DOMAIN_XBAR2CLK;

    FOR_EACH_INDEX_IN_MASK(32, domIdx, Clk.freqCountedAvg.clkDomainMask)
    {
        Clk.freqCountedAvg.pFreqCntr[count++].clkDomain = BIT(domIdx);
    }
    FOR_EACH_INDEX_IN_MASK_END;
}

/* ------------------------- Private Functions ------------------------------ */

