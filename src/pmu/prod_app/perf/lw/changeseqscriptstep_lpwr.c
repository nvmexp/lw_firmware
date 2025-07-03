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
 * @file    changeseqscriptstep_lpwr.c
 * @brief   Implementation for handling LPWR-based steps by the change sequencer.
 */

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/changeseqscriptstep_lpwr.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Builds a low power based step for a change sequencer script.
 * @memberof LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_LPWR
 * @public
 * @copydetails PerfDaemonChangeSeqScriptBuildStep()
 */
FLCN_STATUS
perfDaemonChangeSeqScriptBuildStep_LPWR_IMPL
(
    CHANGE_SEQ                         *pChangeSeq,
    CHANGE_SEQ_SCRIPT                  *pScript,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLwrr,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLast
)
{
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_LPWR *pStepLpwr =
        &pScript->pStepLwrr->data.lpwr;

    // Update the step information
    pStepLpwr->pstateIndex = pChangeLwrr->pstateIndex;

    return FLCN_OK;
}

/*!
 * @brief Exelwtes a low-power step in the change sequencer script.
 * @memberof LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_LPWR
 * @public
 * @note This function is not present in automotive builds.
 * @copydetails PerfDaemonChangeSeqScriptExelwteStep()
 */
FLCN_STATUS
perfDaemonChangeSeqScriptExelwteStep_LPWR_IMPL
(
    CHANGE_SEQ                                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper
)
{
    PMU_BREAKPOINT();

    return FLCN_ERR_ILWALID_STATE;
}

/* ------------------------ Private Functions ------------------------------- */
