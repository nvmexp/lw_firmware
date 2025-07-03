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
 * @file    changeseqscriptstep_pstate.c
 * @brief   Implementation for handling PSTATE-based steps by the change sequencer.
 */

/* ------------------------ System Includes --------------------------------- */
#include "perf/changeseqscriptstep_pstate.h"

/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Static Function Prototypes ---------------------- */
static FLCN_STATUS s_perfDaemonChangeSeqSciptNotifyClients_PSTATE(CHANGE_SEQ *pChangeSeq, LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER *pStepSuper, RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION notification)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "s_perfDaemonChangeSeqSciptNotifyClients_PSTATE");

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Builds a P-state based step for a change sequencer script.
 * @memberof LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_PSTATE
 * @public
 * @copydetails PerfDaemonChangeSeqScriptBuildStep()
 */
FLCN_STATUS
perfDaemonChangeSeqScriptBuildStep_PSTATE_IMPL
(
    CHANGE_SEQ                         *pChangeSeq,
    CHANGE_SEQ_SCRIPT                  *pScript,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLwrr,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLast
)
{
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_PSTATE *pStepPstate =
        &pScript->pStepLwrr->data.pstate;

    // Update the step information
    pStepPstate->pstateIndex = pChangeLwrr->pstateIndex;

    return FLCN_OK;
}

/*!
 * @brief Exelwtes the pre-Pstate change step in the change sequencer script.
 * @memberof LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_PSTATE
 * @public
 * @note This function is not present in automotive builds.
 * @copydetails PerfDaemonChangeSeqScriptExelwteStep()
 */
FLCN_STATUS
perfDaemonChangeSeqScriptExelwteStep_PRE_PSTATE_PMU_IMPL
(
    CHANGE_SEQ                                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper
)
{
    return s_perfDaemonChangeSeqSciptNotifyClients_PSTATE(
        pChangeSeq, pStepSuper,
        RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_PRE_PSTATE
    );
}

/*!
 * @brief Exelwtes the post-Pstate change step in the change sequencer script.
 * @memberof LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_PSTATE
 * @public
 * @note This function is not present in automotive builds.
 * @copydetails PerfDaemonChangeSeqScriptExelwteStep()
 */
FLCN_STATUS
perfDaemonChangeSeqScriptExelwteStep_POST_PSTATE_PMU_IMPL
(
    CHANGE_SEQ                                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper
)
{
    return s_perfDaemonChangeSeqSciptNotifyClients_PSTATE(
        pChangeSeq, pStepSuper,
        RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_POST_PSTATE
    );
}

/* ------------------------ Private Functions ------------------------------- */
static FLCN_STATUS
s_perfDaemonChangeSeqSciptNotifyClients_PSTATE
(
    CHANGE_SEQ                                        *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER *pStepSuper,
    RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION            notification
)
{
    FLCN_STATUS status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_NOTIFICATION))
    {
        LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_PSTATE  *pStepPstate =
            (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_PSTATE *)pStepSuper;
        LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE                  *pChangeLast =
            pChangeSeq->pChangeLast;
        RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_DATA         data        = {0};


        data.notification    = notification;
        data.pstateLevelLast = pChangeLast->pstateIndex;
        data.pstateLevelLwrr = pStepPstate->pstateIndex;

        status = perfDaemonChangeSeqFireNotifications(pChangeSeq, &data);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfDaemonChangeSeqScriptExelwteStep_POST_PSTATE_PMU_exit;
        }
    }

perfDaemonChangeSeqScriptExelwteStep_POST_PSTATE_PMU_exit:
    return status;
}
