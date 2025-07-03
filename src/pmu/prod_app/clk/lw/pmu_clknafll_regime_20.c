/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clknafll_regime_20.c
 * @brief  Clock code related to the V2.0 AVFS regime logic and clock programming.
 * TODO: to be removed as never been used after branching
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dbgprintf.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "pmu_objperf.h"
#include "config/g_clk_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Private Function Prototypes -------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Helper interface to program NAFLL clocks.
 *
 * @param[in]   pDomainList Pointer to the clock domain list structure.
 *
 * @return FLCN_OK                NAFLL clocks successfully programmed.
 * @return FLCN_ERR_ILWALID_STATE If the NAFLL device for the given domain is
 *                                not found
 * @return other                  Descriptive error code from sub-routines on
 *                                failure
 */
FLCN_STATUS
clkNafllGrpProgram_IMPL
(
    LW2080_CTRL_CLK_CLK_DOMAIN_LIST *pDomainList
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8        clkIdx;
    LwU32       nafllIdMask;
    LwU8        nafllId;
    RM_PMU_CLK_FREQ_TARGET_SIGNAL target = {{0}};

    for (clkIdx = 0; clkIdx < pDomainList->numDomains; clkIdx++)
    {
        // Ignore unused entries.
        if (pDomainList->clkDomains[clkIdx].clkDomain == LW2080_CTRL_CLK_DOMAIN_UNDEFINED)
        {
            continue;
        }

        // Get mask of NAFLLs for the given clock domain
        status = clkNafllDomainToIdMask(
                    pDomainList->clkDomains[clkIdx].clkDomain,
                    &nafllIdMask);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkNafllGrpProgram_exit;
        }

        // Program all NAFLLs
        FOR_EACH_INDEX_IN_MASK(32, nafllId, nafllIdMask)
        {
            CLK_NAFLL_DEVICE *pNafllDev = clkNafllDeviceGet(nafllId);
            if (pNafllDev == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_STATE;
                goto clkNafllGrpProgram_exit;
            }

            // Build the target signal based on input request
            target.super.freqKHz        = pDomainList->clkDomains[clkIdx].clkFreqKHz;
            target.super.regimeId       = pDomainList->clkDomains[clkIdx].regimeId;
            target.super.source         = pDomainList->clkDomains[clkIdx].source;
            target.super.dvcoMinFreqMHz = pDomainList->clkDomains[clkIdx].dvcoMinFreqMHz;

            // Config NAFLL dynamic status params based on the input target request.
            status = clkNafllConfig(pNafllDev, pNafllDev->pStatus, &target);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkNafllGrpProgram_exit;
            }

            // Program NAFLL based on the callwlated NAFLL configurations.
            status = clkNafllProgram(pNafllDev, pNafllDev->pStatus);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkNafllGrpProgram_exit;
            }

            // PP-TODO : Create NAFLL clean up interface.
            pNafllDev->pStatus->regime.lwrrentRegimeId =
                pNafllDev->pStatus->regime.targetRegimeId;
            pNafllDev->pStatus->regime.lwrrentFreqMHz  =
                pNafllDev->pStatus->regime.targetFreqMHz;

        }
        FOR_EACH_INDEX_IN_MASK_END;

        // Update counted average frequency, used by frequency controllers.
        if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_COUNTED_AVG))
        {
            status = clkFreqCountedAvgUpdate(
                pDomainList->clkDomains[clkIdx].clkDomain,
                (pDomainList->clkDomains[clkIdx].clkFreqKHz));
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkNafllGrpProgram_exit;
            }
        }
    }

clkNafllGrpProgram_exit:
    return status;
}
