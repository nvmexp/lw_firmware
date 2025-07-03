/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file
 * @see https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 */


/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes --------------------------- */

#include "clk3/dom/clkfreqdomain_multinafll.h"
#include "pmu_objclk.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */

CLK_DEFINE_VTABLE_CLK3__DOMAIN(MultiNafllFreqDomain);

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief   Configure the clkmultinafllfreqdomain to produce target
 *          frequency and regime.
 */
FLCN_STATUS
clkConfig_MultiNafllFreqDomain
(
    ClkMultiNafllFreqDomain *this,
    const ClkTargetSignal   *pTarget
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8        idx;

    //
    // Current domain has no multiplexers, as such target path is empty
    //
    if (!CLK_IS_EMPTY__SIGNAL_PATH(pTarget->super.path))
    {
        return FLCN_ERR_ILWALID_PATH;
    }

    // Update the phase count.
    this->super.phaseCount = 2;

    //
    // Now that the paths have been determined and the range set, the source is
    // ready to be configured
    //
    for (idx = 0; idx < NAFLL_COUNT_MAX__MULTINAFLLFREQDOMAIN; idx++)
    {
        // Override the root for each NAFLL source.
        this->super.pRoot = &this->nafll[idx].super;

        // Configure the NAFLL source.
        status = clkConfig_FreqSrc_VIP(this->super.pRoot,
                    &CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(&this->super),
                    this->super.phaseCount,
                    pTarget,
                    LW_TRUE);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkConfig_MultiNafllFreqDomain_exit;
        }
    }

clkConfig_MultiNafllFreqDomain_exit:
    return status;
}

/*!
 * @brief   Program NAFLL Frequency domain's hardware.
 */
FLCN_STATUS
clkProgram_MultiNafllFreqDomain
(
    ClkMultiNafllFreqDomain *this
)
{
    FLCN_STATUS status;
    LwU8        idx;

    // Program ALL NAFLL source.
    for (idx = 0; idx < NAFLL_COUNT_MAX__MULTINAFLLFREQDOMAIN; idx++)
    {
        // Override the root for each NAFLL source.
        this->super.pRoot = &this->nafll[idx].super;

        status = clkProgram_ProgrammableFreqDomain(&this->super);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkProgram_ProgrammableFreqDomain_exit;
        }
    }

    // Update counted average frequency, used by frequency controllers.
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_COUNTED_AVG))
    {
        status = clkFreqCountedAvgUpdate(
                    this->super.clkDomain,
                    CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(&this->super).freqKHz);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkProgram_ProgrammableFreqDomain_exit;
        }
    }

clkProgram_ProgrammableFreqDomain_exit:
    return status;
}

