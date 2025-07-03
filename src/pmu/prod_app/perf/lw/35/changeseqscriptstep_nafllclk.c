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
#include "perf/35/changeseqscriptstep_nafllclk.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Exelwtes a NAFLL clock step for a change sequencer script.
 * @memberof CHANGE_SEQ_35
 * @private
 * @copydetails PerfDaemonChangeSeqScriptExelwteStep()
 */
FLCN_STATUS
perfDaemonChangeSeq35ScriptExelwteStep_NAFLL_CLKS_IMPL
(
    CHANGE_SEQ                                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper
)
{
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CLKS *pClkStep =
        (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CLKS *)pStepSuper;
    LwU8        idx;
    FLCN_STATUS status = FLCN_OK;

    //
    // Attach all the IMEM overlays required.
    // imem_perf
    // imem_perfDaemon
    // imem_perfDaemonChangeSeq
    // imem_clkLibClk3
    // imem_perfClkAvfs
    // imem_libClkFreqController
    // imem_libClkFreqCountedAvg
    // imem_clkLibCntr
    // imem_clkLibCntrSwSync
    // imem_libLw64
    // imem_libLw64Umul
    //

    //
    // Detach all the IMEM overlays that are NOT required.
    // imem_libVolt
    // imem_libBoardObj - Permanently attached to perfdaemon task.
    // imem_perfVf - Next low hanging fruit, only used for clkNafllNdivFromFreqCompute
    //

    // Attach overlays
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, libBoardObj)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, libVolt)
        OSTASK_OVL_DESC_DEFINE_LIB_CLK_FREQ_CONTROLLER
        OSTASK_OVL_DESC_DEFINE_LIB_FREQ_COUNTED_AVG
    };

    // Sanity check that all clock domains are NAFLL clock domains.
    for (idx = 0; idx < pClkStep->clkList.numDomains; idx++)
    {
        // Ignore unused entries.
        if (pClkStep->clkList.clkDomains[idx].clkDomain == LW2080_CTRL_CLK_DOMAIN_UNDEFINED)
        {
            continue;
        }

        // Sanity check the source of frequency generator.
        if (pClkStep->clkList.clkDomains[idx].source != LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NOT_SUPPORTED;
            goto perfDaemonChangeSeq35ScriptExelwteStep_NAFLL_CLKS_exit;
        }
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_ADC_OVERRIDE_20))
    {
        // Program ADC to final target overrides before programming final target clocks.
        status = clkAdcsProgram(&pClkStep->adcSwOverrideList);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfDaemonChangeSeq35ScriptExelwteStep_NAFLL_CLKS_exit;
        }
    }

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = clkNafllGrpProgram(&pClkStep->clkList);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfDaemonChangeSeq35ScriptExelwteStep_NAFLL_CLKS_exit;
    }

perfDaemonChangeSeq35ScriptExelwteStep_NAFLL_CLKS_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
