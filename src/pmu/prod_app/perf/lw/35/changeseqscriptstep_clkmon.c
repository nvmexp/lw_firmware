/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    changeseqscriptstep_clkmon.c
 * @brief   Implementation for handling CLK_MON-based steps by the change sequencer.
 */

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objclk.h"
#include "clk/clk_clockmon.h"
#include "perf/35/changeseqscriptstep_clkmon.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Evaluate clock monitors thresholds for requested change.
 * @memberof CHANGE_SEQ_35
 * @private
 * @copydetails PerfDaemonChangeSeqScriptBuildStep()
 */
FLCN_STATUS
perfDaemonChangeSeq35ScriptBuildStep_CLK_MON_IMPL
(
    CHANGE_SEQ                         *pChangeSeq,
    CHANGE_SEQ_SCRIPT                  *pScript,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLwrr,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLast
)
{
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CLK_MON *pStepClkMon =
                                &pScript->pStepLwrr->data.clkMon;
    CLK_DOMAIN    *pDomain;
    LwBoardObjIdx  idx;
    LwU8           monIdx;
    FLCN_STATUS    status;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClockMon)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Evaluate VFE threshold idx for clock monitor programming
        status = clkClockMonsThresholdEvaluate(&pChangeLwrr->clkList, &pChangeLwrr->voltList, pStepClkMon);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    // Fill in the target freq values for all clock monitors
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, idx, &pScript->clkDomainsActiveMask.super)
    {
        LW2080_CTRL_CLK_CLK_DOMAIN_LIST_ITEM   *pClkListItemLwrr  =
            &pChangeLwrr->clkList.clkDomains[idx];

        // Sanity check
        if (clkDomainApiDomainGet(pDomain) != pClkListItemLwrr->clkDomain)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto perfDaemonChangeSeq35ScriptBuildStep_CLK_MON_exit;
        }

        for (monIdx = 0; monIdx < pStepClkMon->clkMonList.numDomains; monIdx++)
        {
            if (pStepClkMon->clkMonList.clkDomains[monIdx].clkApiDomain == pClkListItemLwrr->clkDomain)
            {
                pStepClkMon->clkMonList.clkDomains[monIdx].clkFreqMHz = (pClkListItemLwrr->clkFreqKHz / 1000U);
            }
        }
    }
    BOARDOBJGRP_ITERATOR_END;

perfDaemonChangeSeq35ScriptBuildStep_CLK_MON_exit:
    return status;
}

/*!
 * @brief Exelwtes a clock monitor threshold step for a change sequencer script.
 * @note This function is not enabled on automotive builds.
 * @memberof CHANGE_SEQ_35
 * @private
 * @copydetails PerfDaemonChangeSeqScriptExelwteStep()
 */
FLCN_STATUS
perfDaemonChangeSeq35ScriptExelwteStep_CLK_MON_IMPL
(
    CHANGE_SEQ                                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper
)
{
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CLK_MON *pStepClkMon =
        (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CLK_MON *)pStepSuper;
    FLCN_STATUS     status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_CLK_MON) &&
        clkDomainsIsClkMonEnabled())
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClockMon)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            // Program clock monitors threshold
            status = clkClockMonsProgram(pStepClkMon);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto perfDaemonChangeSeq35ScriptExelwteStep_CLK_MON_exit;
            }

perfDaemonChangeSeq35ScriptExelwteStep_CLK_MON_exit:
            lwosNOP();
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }

    return status;
}

/* ------------------------ Private Functions ------------------------------- */
