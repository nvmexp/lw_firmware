/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    changeseqscriptstep_mem_tune.c
 * @brief   Implementation for handling memory settings tuning steps by the change sequencer.
 */

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objperf.h"
#include "pmu_objclk.h"
#include "clk/pmu_clkfbflcn.h"
#include "perf/changeseqscriptstep_mem_tune.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Builds a memory settings tuning step for a change sequencer script.
 * @memberof LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_MEM_TUNE
 * @public
 * @copydetails PerfDaemonChangeSeqScriptBuildStep()
 */
FLCN_STATUS
perfDaemonChangeSeqScriptBuildStep_MEM_TUNE_IMPL
(
    CHANGE_SEQ                         *pChangeSeq,
    CHANGE_SEQ_SCRIPT                  *pScript,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLwrr,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLast
)
{
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_MEM_TUNE *pStepMemTune =
        &pScript->pStepLwrr->data.memTune;
    LwU32           mclkDomainIdx;
    FLCN_STATUS     status = FLCN_OK;

    if (pStepMemTune == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto perfDaemonChangeSeqScriptBuildStep_MEM_TUNE_exit;
    }

    // Get current memory clock value.
    PMU_ASSERT_OK_OR_GOTO(status,
        clkDomainsGetIndexByApiDomain(LW2080_CTRL_CLK_DOMAIN_MCLK, &mclkDomainIdx),
        perfDaemonChangeSeqScriptBuildStep_MEM_TUNE_exit);

    // Validate the domain
    PMU_ASSERT_OK_OR_GOTO(status,
        (LW2080_CTRL_CLK_DOMAIN_MCLK != pChangeLwrr->clkList.clkDomains[mclkDomainIdx].clkDomain) ?
            FLCN_ERR_ILWALID_STATE : FLCN_OK,
        perfDaemonChangeSeqScriptBuildStep_MEM_TUNE_exit);

    // Update the step information
    pStepMemTune->tFAW           = pChangeLwrr->tFAW;
    pStepMemTune->mclkFreqKHz    = pChangeLwrr->clkList.clkDomains[mclkDomainIdx].clkFreqKHz;

perfDaemonChangeSeqScriptBuildStep_MEM_TUNE_exit:
    return status;
}

/*!
 * @brief Exelwtes a MEM_TUNE step in the change sequencer script.
 * @memberof LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_MEM_TUNE
 * @public
 * @note This function is not present in automotive builds.
 * @copydetails PerfDaemonChangeSeqScriptExelwteStep()
 */
FLCN_STATUS
perfDaemonChangeSeqScriptExelwteStep_MEM_TUNE_IMPL
(
    CHANGE_SEQ                                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper
)
{
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_MEM_TUNE *pStepMemTune =
        (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_MEM_TUNE *)pStepSuper;
    FLCN_STATUS status = FLCN_OK;

    status = clkTriggerMClkTrrdModeSwitch(pStepMemTune->tFAW, pStepMemTune->mclkFreqKHz);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfDaemonChangeSeqScriptExelwteStep_MEM_TUNE_exit;
    }

perfDaemonChangeSeqScriptExelwteStep_MEM_TUNE_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
