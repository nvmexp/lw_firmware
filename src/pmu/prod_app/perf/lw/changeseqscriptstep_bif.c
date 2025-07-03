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
 * @file    changeseqscriptstep_bif.c
 * @brief   Implementation for handling BIF-based steps by the change sequencer.
 */

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
//
// pmu_objperf.h _must_ be included before pmu_objbif.h; otherwise, there are
// compiler errors.
//
#include "pmu_objperf.h"
#include "pmu_objbif.h"
#include "perf/changeseqscriptstep_bif.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Builds a BIF-based step for a change sequencer script.
 * @memberof LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_BIF
 * @public
 * @copydetails PerfDaemonChangeSeqScriptBuildStep()
 */
FLCN_STATUS
perfDaemonChangeSeqScriptBuildStep_BIF_IMPL
(
    CHANGE_SEQ                         *pChangeSeq,
    CHANGE_SEQ_SCRIPT                  *pScript,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLwrr,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLast
)
{
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_BIF *pStepBif =
        &pScript->pStepLwrr->data.bif;
    PSTATE_35 *pPstate35 = (PSTATE_35 *)PSTATE_GET(pChangeLwrr->pstateIndex);
    FLCN_STATUS         status = FLCN_OK;

    if (pPstate35 == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto perfDaemonChangeSeqScriptBuildStep_BIF_exit;
    }

    // Update the step information
    pStepBif->pstateIndex = pChangeLwrr->pstateIndex;
    pStepBif->pcieIdx     = pPstate35->pcieIdx;
    pStepBif->lwlinkIdx   = pPstate35->lwlinkIdx;

perfDaemonChangeSeqScriptBuildStep_BIF_exit:
    return status;
}

/*!
 * @brief Exelwtes a BIF step in the change sequencer script.
 * @memberof LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_BIF
 * @public
 * @note This function is not present in automotive builds.
 * @copydetails PerfDaemonChangeSeqScriptExelwteStep()
 */
FLCN_STATUS
perfDaemonChangeSeqScriptExelwteStep_BIF_IMPL
(
    CHANGE_SEQ                                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper
)
{
    FLCN_STATUS status;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, bif)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBif)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_BIF    *pStepBif =
            (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_BIF *)pStepSuper;

        status = bifPerfChangeStep(pStepBif->pstateIndex, pStepBif->pcieIdx);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfDaemonChangeSeqScriptExelwteStep_BIF_exit;
        }

perfDaemonChangeSeqScriptExelwteStep_BIF_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/* ------------------------ Private Functions ------------------------------- */
