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
 * @file   pmu_clkgv10x.c
 * @brief  PMU Hal Functions for GV100+ for Clocks
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

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_COUNTED_AVG))
/*!
 * @brief Construct and initialize the SW state for CLK_FREQ_COUNTED_AVG feature.
 */
void
clkFreqCountedAvgConstruct_GV10X(void)
{
    static CLK_FREQ_COUNTER clkFreqCounterArr_GV10X[5] = {{ 0 }};
    LwU8 domIdx;
    LwU8 count = 0;

    Clk.freqCountedAvg.pFreqCntr = clkFreqCounterArr_GV10X;

    Clk.freqCountedAvg.clkDomainMask =
                CLK_DOMAIN_MASK_GPC  |
                CLK_DOMAIN_MASK_SYS  |
                CLK_DOMAIN_MASK_XBAR |
#if PMUCFG_FEATURE_ENABLED(PMU_HOSTCLK_PRESENT)
                CLK_DOMAIN_MASK_HOST |
#endif
                CLK_DOMAIN_MASK_LWD;
                
    FOR_EACH_INDEX_IN_MASK(32, domIdx, Clk.freqCountedAvg.clkDomainMask)
    {
        Clk.freqCountedAvg.pFreqCntr[count++].clkDomain = BIT(domIdx);
    }
    FOR_EACH_INDEX_IN_MASK_END;
}
#endif

/* ------------------------- Private Functions ------------------------------ */
