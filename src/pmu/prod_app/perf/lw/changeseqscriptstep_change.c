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
 * @file    changeseqscriptstep_change.c
 * @brief   Implementation for handling CHANGE-based steps by the change sequencer.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmu_objclk.h"
#include "clk/clk_clockmon.h"
#include "perf/changeseqscriptstep_change.h"

/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Static Function Prototypes ---------------------- */
static FLCN_STATUS s_perfDaemonChangeSeqScriptStepNotifyClients(CHANGE_SEQ *pChangeSeq, LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper, RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION notification)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "s_perfDaemonChangeSeqScriptStepNotifyClients");
static FLCN_STATUS s_perfDaemonChangeSeqScriptStepNotifyClkMon(LwBool bIsStart)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "s_perfDaemonChangeSeqScriptStepNotifyClkMon");

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Builds a change-based step for a change sequencer script.
 * @memberof LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CHANGE
 * @public
 * @copydetails PerfDaemonChangeSeqScriptBuildStep()
 */
FLCN_STATUS
perfDaemonChangeSeqScriptBuildStep_CHANGE_IMPL
(
    CHANGE_SEQ                         *pChangeSeq,
    CHANGE_SEQ_SCRIPT                  *pScript,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLwrr,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLast
)
{
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CHANGE *pStepChange =
        &pScript->pStepLwrr->data.change;

    // Update the step information
    pStepChange->pstateIndex = pChangeLwrr->pstateIndex;

    return FLCN_OK;
}

/*!
 * @brief Exelwtes the pre-change step in the change sequencer script.
 * @memberof LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CHANGE
 * @public
 * @note This function is not present in automotive builds.
 * @copydetails PerfDaemonChangeSeqScriptExelwteStep()
 */
FLCN_STATUS
perfDaemonChangeSeqScriptExelwteStep_PRE_CHANGE_PMU_IMPL
(
    CHANGE_SEQ                                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper
)
{
    FLCN_STATUS status;

    status = s_perfDaemonChangeSeqScriptStepNotifyClients(
                pChangeSeq, pStepSuper,
                RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_PRE_CHANGE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfDaemonChangeSeqScriptExelwteStep_PRE_CHANGE_PMU_exit;
    }

    status = s_perfDaemonChangeSeqScriptStepNotifyClkMon(LW_TRUE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfDaemonChangeSeqScriptExelwteStep_PRE_CHANGE_PMU_exit;
    }

perfDaemonChangeSeqScriptExelwteStep_PRE_CHANGE_PMU_exit:
    return status;
}

/*!
 * @brief Exelwtes the post-change step in the change sequencer script.
 * @memberof LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CHANGE
 * @private
 * @note This function is not present in automotive builds.
 * @copydetails PerfDaemonChangeSeqScriptExelwteStep()
 */
FLCN_STATUS
perfDaemonChangeSeqScriptExelwteStep_POST_CHANGE_PMU_IMPL
(
    CHANGE_SEQ                                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper
)
{
    FLCN_STATUS status;

    status = s_perfDaemonChangeSeqScriptStepNotifyClients(
                pChangeSeq, pStepSuper,
                RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_POST_CHANGE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfDaemonChangeSeqScriptExelwteStep_POST_CHANGE_PMU_exit;
    }

    status = s_perfDaemonChangeSeqScriptStepNotifyClkMon(LW_FALSE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfDaemonChangeSeqScriptExelwteStep_POST_CHANGE_PMU_exit;
    }

perfDaemonChangeSeqScriptExelwteStep_POST_CHANGE_PMU_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/**
 * @brief Fires off notifications to any clients waiting on events signaling
 * that a change has started or ended.
 *
 * @param  pChangeSeq    The change sequencer.
 * @param  pStepSuper    The current step in the change sequence script.
 * @param  notification  The notification event to send to the clients.
 *
 * @return FLCN_OK if the notifications were successfully sent; a detailed error
 * code otherwise.
 */
static FLCN_STATUS
s_perfDaemonChangeSeqScriptStepNotifyClients
(
    CHANGE_SEQ                                        *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER *pStepSuper,
    RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION            notification
)
{
    FLCN_STATUS status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_NOTIFICATION))
    {
        LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CHANGE  *pStepChange =
            (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CHANGE *)pStepSuper;
        LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE                  *pChangeLast =
            pChangeSeq->pChangeLast;
        RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_DATA data = {0};

        data.notification    = notification;
        data.pstateLevelLast = pChangeLast->pstateIndex;
        data.pstateLevelLwrr = pStepChange->pstateIndex;

        status = perfDaemonChangeSeqFireNotifications(pChangeSeq, &data);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_perfDaemonChangeSeqScriptStepNotifyClients_exit;
        }
    }

s_perfDaemonChangeSeqScriptStepNotifyClients_exit:
    return status;
}

/*!
 * @brief Notifies the clock monitor that the change is starting/stopping.
 *
 * @param[in]  bIsStart  LW_TRUE if the change is beginning processing;
 *                       LW_FALSE if the change has been processed
 *
 * @return FLCN_OK if the clock monitor was successfully notified; a detailed
 * error code otherwise.
 */
static FLCN_STATUS
s_perfDaemonChangeSeqScriptStepNotifyClkMon
(
    LwBool      bIsStart
)
{
    FLCN_STATUS status = FLCN_OK;

    // 1. Notify clock monitors about the start of perf change.
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_CLK_MON) &&
        clkDomainsIsClkMonEnabled())
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClockMon)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            status = clkClockMonsHandlePrePostPerfChangeNotification(bIsStart);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_perfDaemonChangeSeqScriptStepNotifyClkMon_exit;
        }
    }

s_perfDaemonChangeSeqScriptStepNotifyClkMon_exit:
    return status;
}
