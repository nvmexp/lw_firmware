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
 * @file    changeseq_daemon.c
 * @copydoc changeseq_daemon.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "pmu/ssurface.h"

#if (PMU_PROFILE_GA10X_RISCV)
#include <lwriscv/print.h>
#endif

/* ------------------------ Application Includes ---------------------------- */
#include "cmdmgmt/cmdmgmt.h"
#include "pmu_objperf.h"
#include "pmu_objclk.h"
#include "pmu_objbif.h"
#include "perf/perf_daemon.h"
#include "task_perf_daemon.h"
#include "task_perf.h"
#include "clk/clk_domain.h"
#include "clk/clk_clockmon.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "lib/lib_avg.h"
#include "dev_pwr_csb.h"

#include "g_pmurpc.h"
#include "g_pmurmifrpc.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
LwU32 g_CHSEQ_DEBUG[12] = {0,};

// PP-TODO : Send RPC to RM with debug info instead of abusing PMU MAILBOX.
GCC_ATTRIB_ALWAYSINLINE()
static inline void CHSEQ_FAILURE_EX_PRINT(void)
{
    LwU8 i;

    for (i = 0; i < 12; i++)
    {
#if (PMU_PROFILE_GA10X_RISCV)
        {
            dbgPrintf(LEVEL_ALWAYS, "Change Sequencer Restore:: g_CHSEQ_DEBUG[%d] = %d\n.",
                i,
                g_CHSEQ_DEBUG[i]);
        }
#endif
    }
    return;
}

// PP-TODO : Send RPC to RM with debug info instead of abusing PMU MAILBOX.
#define CHSEQ_FAILURE_EX(status)                                        \
    do                                                                  \
    {                                                                   \
        LwU8 i;                                                         \
        g_CHSEQ_DEBUG[0] =                                              \
            ((((status) & 0xffff) << 16) | (__LINE__ & 0xffff));        \
                                                                        \
        for (i = 0; i < 12; i++)                                        \
        {                                                               \
            REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(i), g_CHSEQ_DEBUG[i]);    \
        }                                                               \
        CHSEQ_FAILURE_EX_PRINT();                                       \
        PMU_HALT();                                                     \
    } while (LW_FALSE)

/* ------------------------ Function Prototypes ----------------------------- */
static FLCN_STATUS  s_perfDaemonChangeSeqScriptSendToRM(CHANGE_SEQ *pChangeSeq, CHANGE_SEQ_SCRIPT *pScript)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "s_perfDaemonChangeSeqScriptSendToRM");

static PerfDaemonChangeSeqScriptInit           (s_perfDaemonChangeSeqScriptInit)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "s_perfDaemonChangeSeqScriptInit");

static PerfDaemonChangeSeqScriptExelwteStep    (s_perfDaemonChangeSeqScriptExelwteStep)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "s_perfDaemonChangeSeqScriptExelwteStep");

static FLCN_STATUS  s_perfDaemonChangeSeqFireNotificationCallback(CHANGE_SEQ *pChangeSeq, PERF_CHANGE_SEQ_NOTIFICATION_CLIENT_TASK_INFO *pClientTaskInfo, RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_DATA *pData)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "s_perfDaemonChangeSeqFireNotificationCallback");

static FLCN_STATUS s_perfDaemonChangeSeqSendValidatePerfState(CHANGE_SEQ *pChangeSeq)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "s_perfDaemonChangeSeqSendValidatePerfState");

/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief PERF RPC interface for change sequence script execute completion cmd.
 * @memberof CHANGE_SEQ
 * @public
 *
 * @param[out]  pParams     RM_PMU_RPC_STRUCT_PERF_CHANGE_SEQ_INFO_GET pointer
 *
 * @return FLCN_OK if the function completed successfully
 * 2return FLCN_ERR_ILWALID_ARGUMENT if NULL pointer provided
 * @return Detailed status code on error
 */
FLCN_STATUS
pmuRpcCmdmgmtChangeSeqScriptExelwteCompletion
(
    RM_PMU_RPC_STRUCT_CMDMGMT_CHANGE_SEQ_SCRIPT_EXELWTE_COMPLETION *pParams
)
{
    DISPATCH_PERF_DAEMON_COMPLETION     disp2perfDaemonCompletion   = {{0}};
    FLCN_STATUS                         status;

    if (pParams == NULL)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Queue the completion signal to Perf Daemon completion queue
    disp2perfDaemonCompletion.hdr.eventType =
        PERF_DAEMON_COMPLETION_EVENT_ID_PERF_CHANGE_SEQ_SCRIPT_EXELWTE_COMPLETION;

    status = lwrtosQueueSendBlocking(LWOS_QUEUE(PMU, PERF_DAEMON_RESPONSE),
                &disp2perfDaemonCompletion, sizeof(disp2perfDaemonCompletion));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @brief Builds and exelwtes a change sequencer script.
 * @memberof CHANGE_SEQ
 * @public
 *
 * @note    This interface will be called from PERF task. To reduce the perf
 *          daemon queue size, this interface will directly access the perf
 *          DMEM overlay.
 *
 * @param[in]   pChangeSeq    CHANGE_SEQ pointer
 *
 * @return FLCN_OK if the function completed successfully
 * @return Detailed status code on error
 */
FLCN_STATUS
perfDaemonChangeSeqScriptExelwte
(
    CHANGE_SEQ *pChangeSeq
)
{
    DISPATCH_PERF                       disp2perf;
    CHANGE_SEQ_SCRIPT                  *pScript     = &pChangeSeq->script;
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLwrr = pChangeSeq->pChangeLwrr;
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLast = pChangeSeq->pChangeLast;
    FLCN_STATUS                         status      = FLCN_OK;
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_HEADER
                                       *pHdr        = &pScript->hdr.data;

    // Attach overlays
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVolt)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfClkAvfs)
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLKS_ON_PMU))           // Disabled on AUTO
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clk3x)
#endif
    };

    OSTASK_OVL_DESC ovlDescListBuild[] = {
        OSTASK_OVL_DESC_DEFINE_VFE(FULL)
    };

    OSTASK_OVL_DESC ovlDescListExec[] = {
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLKS_ON_PMU))
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, clkLibClk3)
#endif
    };

    // Attach common overlays.
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Attach overlays needed only for build.
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListBuild);
        {
            // Begin counting build time.
            PERF_CHANGE_SEQ_PROFILE_BEGIN(&pHdr->profiling.totalBuildTimens);

            // Sanity test the GPC-RG Perf restore.
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_LPWR_PERF_RESTORE_SANITY))
            {
                status = s_perfDaemonChangeSeqSendValidatePerfState(pChangeSeq);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto perfDaemonChangeSeqScriptExelwte_exit;
                }
            }
            status = s_perfDaemonChangeSeqScriptInit(pChangeSeq, pScript,
                                                     pChangeLwrr, pChangeLast);
            // Stop counting build time.
            PERF_CHANGE_SEQ_PROFILE_END(&pHdr->profiling.totalBuildTimens);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListBuild);

        if (status != FLCN_OK)
        {
            // Ignore the warnings as they will be handled in PERF task.
            if (status != FLCN_WARN_NOTHING_TO_DO)
            {
                PMU_BREAKPOINT();
            }

            goto perfDaemonChangeSeqScriptExelwte_exit;
        }

        // Attach overlays needed only for exelwtion.
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListExec);
        {
            // Begin conting exelwtion time.
            PERF_CHANGE_SEQ_PROFILE_BEGIN(&pHdr->profiling.totalExelwtionTimens);

            // Execute perf change sequence script.
            status = perfDaemonChangeSeqScriptExelwteAllSteps(pChangeSeq, pScript);

            // Stop counting exelwtion time.
            PERF_CHANGE_SEQ_PROFILE_END(&pHdr->profiling.totalExelwtionTimens);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListExec);

        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfDaemonChangeSeqScriptExelwte_exit;
        }

perfDaemonChangeSeqScriptExelwte_exit:

        // Queue the change request to PERF_DAEMON task
        disp2perf.hdr.eventType =
            PERF_EVENT_ID_PERF_CHANGE_SEQ_SCRIPT_COMPLETION;
        disp2perf.scriptCompletion.completionStatus = status;

        // Send completion signal to PERF task.
        (void)lwrtosQueueIdSendBlocking(LWOS_QUEUE_ID(PMU, PERF),
                                        &disp2perf, sizeof(disp2perf));
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return FLCN_OK;
}

/*!
 * @brief Build and insert specific perf change sequence script step.
 * @memberof CHANGE_SEQ
 * @public
 *
 * @param[in]  pChangeSeq       CHANGE_SEQ pointer.
 * @param[in]  pScript          Pointer to CHANGE_SEQ_SCRIPT buffer.
 * @param[in]  pChangeLwrr      Pointer to current LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE.
 * @param[in]  pChangeLast      Pointer to last completed LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE.
 * @param[in]  stepId           ref@ LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID
 *
 * @return FLCN_OK if the function completed successfully
 * @return Detailed status code on error
 */
FLCN_STATUS
perfDaemonChangeSeqScriptBuildAndInsert
(
    CHANGE_SEQ                                 *pChangeSeq,
    CHANGE_SEQ_SCRIPT                          *pScript,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE         *pChangeLwrr,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE         *pChangeLast,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID     stepId
)
{
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_HEADER     *pHdr   =
        &pScript->hdr.data;
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER *pSuper =
        NULL;
    FLCN_STATUS status = FLCN_ERR_ILWALID_ARGUMENT;

    // Sanity check the numsteps. Reserve one more space for upcoming 'numSteps++'.
    if (pHdr->numSteps >= LW2080_CTRL_PERF_CHANGE_SEQ_SCRIPT_VF_SWITCH_MAX_STEPS)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto perfDaemonChangeSeqScriptBuildAndInsert_exit;
    }

    status = perfDaemonChangeSeqRead_STEP(pChangeSeq, pScript,
                &pSuper, pHdr->numSteps, LW_FALSE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfDaemonChangeSeqScriptBuildAndInsert_exit;
    }

    // Clear the step buffer.
    (void)memset(pSuper, 0,
           sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_DATA_ALIGNED));

    // Build the request step
    pSuper->stepId = stepId;
    switch (stepId)
    {
        case LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_PRE_CHANGE_RM:
        case LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_POST_CHANGE_RM:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_CHANGE_RM))
            {
                status = perfDaemonChangeSeqScriptBuildStep_CHANGE(
                                                            pChangeSeq,
                                                            pScript,
                                                            pChangeLwrr,
                                                            pChangeLast);
            }
            break;
        }
        case LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_PRE_CHANGE_PMU:
        case LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_POST_CHANGE_PMU:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_CHANGE_PMU))
            {
                status = perfDaemonChangeSeqScriptBuildStep_CHANGE(
                                                            pChangeSeq,
                                                            pScript,
                                                            pChangeLwrr,
                                                            pChangeLast);
            }
            break;
        }
        case LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_PRE_PSTATE_RM:
        case LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_POST_PSTATE_RM:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_PSTATE_RM))
            {
                status = perfDaemonChangeSeqScriptBuildStep_PSTATE(
                                                            pChangeSeq,
                                                            pScript,
                                                            pChangeLwrr,
                                                            pChangeLast);
            }
            break;
        }
        case LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_PRE_PSTATE_PMU:
        case LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_POST_PSTATE_PMU:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_PSTATE_PMU))
            {
                status = perfDaemonChangeSeqScriptBuildStep_PSTATE(
                                                            pChangeSeq,
                                                            pScript,
                                                            pChangeLwrr,
                                                            pChangeLast);
            }
            break;
        }
        case LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_LPWR:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_LPWR))
            {
                status = perfDaemonChangeSeqScriptBuildStep_LPWR(
                                                            pChangeSeq,
                                                            pScript,
                                                            pChangeLwrr,
                                                            pChangeLast);
            }
            break;
        }
        case LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_BIF:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_BIF))
            {
                status = perfDaemonChangeSeqScriptBuildStep_BIF(
                                                            pChangeSeq,
                                                            pScript,
                                                            pChangeLwrr,
                                                            pChangeLast);
            }
            break;
        }
        case LW2080_CTRL_PERF_CHANGE_SEQ_35_STEP_ID_PRE_VOLT_CLKS:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_VOLT_CLKS))
            {
                status = perfDaemonChangeSeq35ScriptBuildStep_PRE_VOLT_CLKS(
                                                            pChangeSeq,
                                                            pScript,
                                                            pChangeLwrr,
                                                            pChangeLast);
            }
            break;
        }
        case LW2080_CTRL_PERF_CHANGE_SEQ_35_STEP_ID_POST_VOLT_CLKS:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_VOLT_CLKS))
            {
                status = perfDaemonChangeSeq35ScriptBuildStep_POST_VOLT_CLKS(
                                                            pChangeSeq,
                                                            pScript,
                                                            pChangeLwrr,
                                                            pChangeLast);
            }
            break;
        }
        case LW2080_CTRL_PERF_CHANGE_SEQ_35_STEP_ID_CLK_MON:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_CLK_MON))
            {
                status = perfDaemonChangeSeq35ScriptBuildStep_CLK_MON(
                                                            pChangeSeq,
                                                            pScript,
                                                            pChangeLwrr,
                                                            pChangeLast);
            }
            break;
        }
        case LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_VOLT:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_VOLT))
            {
                status = perfDaemonChangeSeqScriptBuildStep_VOLT(
                                                            pChangeSeq,
                                                            pScript,
                                                            pChangeLwrr,
                                                            pChangeLast);
            }
            break;
        }
        case LW2080_CTRL_PERF_CHANGE_SEQ_35_STEP_ID_PRE_VOLT_NAFLL_CLKS:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_VOLT_NAFLL_CLKS))
            {
                status = perfDaemonChangeSeq35ScriptBuildStep_PRE_VOLT_CLKS(
                                                            pChangeSeq,
                                                            pScript,
                                                            pChangeLwrr,
                                                            pChangeLast);
            }
            break;
        }
        case LW2080_CTRL_PERF_CHANGE_SEQ_35_STEP_ID_POST_VOLT_NAFLL_CLKS:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_VOLT_NAFLL_CLKS))
            {
                status = perfDaemonChangeSeq35ScriptBuildStep_POST_VOLT_CLKS(
                                                            pChangeSeq,
                                                            pScript,
                                                            pChangeLwrr,
                                                            pChangeLast);
            }
            break;
        }
        case LW2080_CTRL_PERF_CHANGE_SEQ_35_STEP_ID_XBAR_BOOST_PRE_VOLT_CLKS:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_XBAR_BOOST_FOR_MCLK))
            {
                status = perfDaemonChangeSeq35ScriptBuildStep_PRE_VOLT_CLKS(
                                                            pChangeSeq,
                                                            pScript,
                                                            pChangeLwrr,
                                                            pChangeLast);
            }
            break;
        }
        case LW2080_CTRL_PERF_CHANGE_SEQ_35_STEP_ID_XBAR_BOOST_POST_VOLT_CLKS:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_XBAR_BOOST_FOR_MCLK))
            {
                status = perfDaemonChangeSeq35ScriptBuildStep_POST_VOLT_CLKS(
                                                            pChangeSeq,
                                                            pScript,
                                                            pChangeLwrr,
                                                            pChangeLast);
            }
            break;
        }
        case LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_XBAR_BOOST_VOLT:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_XBAR_BOOST_FOR_MCLK))
            {
                status = perfDaemonChangeSeqScriptBuildStep_VOLT(
                                                            pChangeSeq,
                                                            pScript,
                                                            pChangeLwrr,
                                                            pChangeLast);
            }
            break;
        }
        case LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_MEM_TUNE:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_MEM_TUNE_CHANGE))
            {
                status = perfDaemonChangeSeqScriptBuildStep_MEM_TUNE(
                                                            pChangeSeq,
                                                            pScript,
                                                            pChangeLwrr,
                                                            pChangeLast);
            }
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_STATE;
            break;
        }
    }
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfDaemonChangeSeqScriptBuildAndInsert_exit;
    }

    // Insert the step.
    status = perfDaemonChangeSeqScriptStepInsert(pChangeSeq,
                pScript,
                pSuper);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfDaemonChangeSeqScriptBuildAndInsert_exit;
    }

perfDaemonChangeSeqScriptBuildAndInsert_exit:
    return status;
}

/*!
 * @brief Inserts a given step into perf change sequence script.
 * @memberof CHANGE_SEQ
 * @protected
 *
 * @param[in]  pChangeSeq  CHANGE_SEQ pointer
 * @param[in]  pScript     CHANGE_SEQ_SCRIPT pointer
 * @param[in]  pSuper      @ref LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER
 *
 * @return FLCN_OK if the function completed successfully
 * @return Detailed status code on error
 */
FLCN_STATUS
perfDaemonChangeSeqScriptStepInsert_IMPL
(
    CHANGE_SEQ                                         *pChangeSeq,
    CHANGE_SEQ_SCRIPT                                  *pScript,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pSuper
)
{
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_HEADER *pHdr   = &pScript->hdr.data;
    FLCN_STATUS                                    status = FLCN_OK;

    // Sanity check the numsteps. Reserve one more space for upcoming 'numSteps++'.
    if (pHdr->numSteps >= LW2080_CTRL_PERF_CHANGE_SEQ_SCRIPT_VF_SWITCH_MAX_STEPS)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto perfDaemonChangeSeqScriptStepInsert_exit;
    }

    // Flush the step information.
    status = perfDaemonChangeSeqFlush_STEP(pChangeSeq, pScript, pSuper,
                                            pHdr->numSteps, LW_FALSE);
    if (status != FLCN_OK)
    {
        goto perfDaemonChangeSeqScriptStepInsert_exit;
    }

    // Update the header
    pHdr->numSteps++;

perfDaemonChangeSeqScriptStepInsert_exit:
    return status;
}

/*!
 * @brief Reads the script header from the FB surface.
 * @memberof CHANGE_SEQ
 * @private
 *
 * @param[in]  pChangeSeq  pointer to the CHANGE_SEQ
 * @param[in]  pScript     pointer to the change sequencer script
 *
 * @return FLCN_OK if the function completed successfully
 * @return Detailed status code on error
 */
LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_HEADER *
perfDaemonChangeSeqRead_HEADER
(
    CHANGE_SEQ         *pChangeSeq,
    CHANGE_SEQ_SCRIPT  *pScript
)
{
    if (FLCN_OK != ssurfaceRd(&pScript->hdr,
            CHANGE_SEQ_SUPER_SURFACE_OFFSET_BY_SCRIPT_OFFSET(pScript->scriptOffsetLwrr, hdr),
            sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_HEADER_ALIGNED)))
    {
        PMU_BREAKPOINT();
        return NULL;
    }

    return &pScript->hdr.data;
}

/*!
 * @brief Flushes the script header to the FB surface.
 * @memberof CHANGE_SEQ
 * @private
 *
 * @param[in]  pChangeSeq  pointer to the CHANGE_SEQ
 * @param[in]  pScript     pointer to the change sequencer script
 *
 * @return FLCN_OK if the function completed successfully
 * @return Detailed status code on error
 */
FLCN_STATUS
perfDaemonChangeSeqFlush_HEADER
(
    CHANGE_SEQ         *pChangeSeq,
    CHANGE_SEQ_SCRIPT  *pScript
)
{
    FLCN_STATUS status;

    status = ssurfaceWr(&pScript->hdr,
        CHANGE_SEQ_SUPER_SURFACE_OFFSET_BY_SCRIPT_OFFSET(pScript->scriptOffsetLwrr, hdr),
        sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_HEADER_ALIGNED));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @brief Reads a step from the change sequencer script stored in the FB
 *        surface.
 * @memberof CHANGE_SEQ
 * @private
 *
 * @param[in]  pChangeSeq       Pointer to the CHANGE_SEQ
 * @param[in]  pScript          Pointer to the change sequencer script
 * @param[in]  pSuper           Pointer to address of step buffer.
 * @param[in]  lwrStepIndex     Current active CPU step index.
 * @param[in]  bDmaReadRequired DMA read from FB required?
 *
 * @return FLCN_OK if the function completed successfully
 * @return Detailed status code on error
 */
FLCN_STATUS
perfDaemonChangeSeqRead_STEP_IMPL
(
    CHANGE_SEQ                                         *pChangeSeq,
    CHANGE_SEQ_SCRIPT                                  *pScript,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER **pSuper,
    LwU8                                                lwrStepIndex,
    LwBool                                              bDmaReadRequired
)
{
    FLCN_STATUS status = FLCN_OK;

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_ODP))

    pScript->pStepLwrr = &pScript->steps[lwrStepIndex];
    (*pSuper)          = &pScript->pStepLwrr->data.super;

    // In case of CPU based steps, DMA read required after CPU completes exelwtion.
    if (bDmaReadRequired)
    {
        status = ssurfaceRd((*pSuper),
            CHANGE_SEQ_SUPER_SURFACE_OFFSET_BY_SCRIPT_OFFSET(pScript->scriptOffsetLwrr, steps[lwrStepIndex]),
            sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_DATA_ALIGNED));
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
        }
    }

#else

    (*pSuper) = &pScript->pStepLwrr->data.super;

    status = ssurfaceRd((*pSuper),
        CHANGE_SEQ_SUPER_SURFACE_OFFSET_BY_SCRIPT_OFFSET(pScript->scriptOffsetLwrr, steps[lwrStepIndex]),
        sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_DATA_ALIGNED));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }
#endif

    return status;
}

/*!
 * @brief Flush a step from the change sequencer script to the FB surface.
 * @memberof CHANGE_SEQ
 * @private
 *
 * @param[in]  pChangeSeq        Pointer to the CHANGE_SEQ
 * @param[in]  pScript           Pointer to the change sequencer script
 * @param[in]  pSuper            Pointer to step buffer.
 * @param[in]  lwrStepIndex      Current active CPU step index.
 * @param[in]  bDmaWriteRequired DMA write to FB required?
 *
 * @return FLCN_OK if the function completed successfully
 * @return Detailed status code on error
 */
FLCN_STATUS
perfDaemonChangeSeqFlush_STEP_IMPL
(
    CHANGE_SEQ                                         *pChangeSeq,
    CHANGE_SEQ_SCRIPT                                  *pScript,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pSuper,
    LwU8                                                lwrStepIndex,
    LwBool                                              bDmaWriteRequired
)
{
    FLCN_STATUS status = FLCN_OK;

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_ODP))

    if (bDmaWriteRequired)
    {
        status = ssurfaceWr(pSuper,
            CHANGE_SEQ_SUPER_SURFACE_OFFSET_BY_SCRIPT_OFFSET(pScript->scriptOffsetLwrr, steps[lwrStepIndex]),
            sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_DATA_ALIGNED));
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
        }
    }

#else
    status = ssurfaceWr(pSuper,
        CHANGE_SEQ_SUPER_SURFACE_OFFSET_BY_SCRIPT_OFFSET(pScript->scriptOffsetLwrr, steps[lwrStepIndex]),
        sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_DATA_ALIGNED));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }
#endif

    return status;
}

/*!
 * @brief Flush the change request to the FB surface.
 * @memberof CHANGE_SEQ
 * @protected
 *
 * @param[in]  pChangeSeq  pointer to the CHANGE_SEQ
 * @param[in]  pScript     pointer to the change sequencer script
 *
 * @return FLCN_OK if the function completed successfully
 * @return Detailed status code on error
 */
FLCN_STATUS
perfDaemonChangeSeqFlush_CHANGE_IMPL
(
    CHANGE_SEQ         *pChangeSeq,
    CHANGE_SEQ_SCRIPT  *pScript
)
{
    FLCN_STATUS status;

    status = ssurfaceWr(pChangeSeq->pChangeLwrr,
        CHANGE_SEQ_SUPER_SURFACE_OFFSET_BY_SCRIPT_OFFSET(pScript->scriptOffsetLwrr, change),
        sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_ALIGNED));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @todo PP-TODO
 * @memberof CHANGE_SEQ
 * @private
 *
 * @param[in]  pChangeSeq
 * @param[in]  pData
 *
 * @return FLCN_OK if the function completed successfully
 * @return Detailed status code on error
 */
FLCN_STATUS
perfDaemonChangeSeqFireNotifications
(
    CHANGE_SEQ                                     *pChangeSeq,
    RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_DATA   *pData
)
{
    PERF_CHANGE_SEQ_NOTIFICATIONS  *pNotifications  = NULL;
    PERF_CHANGE_SEQ_NOTIFICATION   *pNotification   = NULL;
    FLCN_STATUS                     status          = FLCN_OK;

    if (pData->notification >= RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_MAX)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfDaemonChangeSeqFireNotifications_exit;
    }
    pNotifications = &pChangeSeq->notifications[pData->notification];
    pNotification = pNotifications->pNotification;

    while (pNotification != NULL)
    {
        // Perform the notification if not marked for removal
        status = s_perfDaemonChangeSeqFireNotificationCallback(pChangeSeq,
                    &pNotification->clientTaskInfo,
                    pData);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfDaemonChangeSeqFireNotifications_exit;
        }
        pNotification = pNotification->pNext;
    }

perfDaemonChangeSeqFireNotifications_exit:
    return status;
}


/*!
 * @brief Validates the input change request
 * @memberof CHANGE_SEQ
 * @protected
 *
 * @param[in]   pChangeSeq   CHANGE_SEQ pointer
 * @param[in]   pChangeLwrr  LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE pointer
 *
 * @return FLCN_OK if the function completed successfully
 * @return Detailed status code on error
 */
FLCN_STATUS
perfDaemonChangeSeqValidateChange_IMPL
(
    CHANGE_SEQ                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLwrr
)
{
    FLCN_STATUS status  = FLCN_OK;

    // 1. Validate P-state index
    if (!PSTATE_INDEX_IS_VALID(pChangeLwrr->pstateIndex))
    {
        status = FLCN_ERR_ILWALID_INDEX;
        PMU_BREAKPOINT();
        goto perfDaemonChangeSeqValidateChange_exit;
    }

    // 2. Call volt sanity check interface.
    status = voltVoltSanityCheck(LW2080_CTRL_VOLT_VOLT_POLICY_CLIENT_PERF_CORE_VF_SEQ,
                                            &pChangeLwrr->voltList, LW_FALSE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfDaemonChangeSeqValidateChange_exit;
    }

    // 3. Call clk list sanity check interface.
    status = clkDomainsClkListSanityCheck(&pChangeLwrr->clkList,
                &pChangeLwrr->voltList,
                pChangeSeq->bVfPointCheckIgnore);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfDaemonChangeSeqValidateChange_exit;
    }

perfDaemonChangeSeqValidateChange_exit:
    return status;
}

/*!
 * @copydoc PerfDaemonChangeSeqScriptExelwteStep()
 * @memberof CHANGE_SEQ
 * @protected
 */
FLCN_STATUS
perfDaemonChangeSeqScriptExelwteStep_SUPER
(
    CHANGE_SEQ                                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper
)
{
    FLCN_STATUS status = FLCN_OK;

    switch (pStepSuper->stepId)
    {
        case LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_PRE_PSTATE_PMU:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_PSTATE_PMU))
            {
                status = perfDaemonChangeSeqScriptExelwteStep_PRE_PSTATE_PMU(
                                                                    pChangeSeq,
                                                                    pStepSuper);
            }
            break;
        }
        case LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_POST_PSTATE_PMU:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_PSTATE_PMU))
            {
                status = perfDaemonChangeSeqScriptExelwteStep_POST_PSTATE_PMU(
                                                                    pChangeSeq,
                                                                    pStepSuper);
            }
            break;
        }
        case LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_PRE_CHANGE_PMU:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_CHANGE_PMU))
            {
                status = perfDaemonChangeSeqScriptExelwteStep_PRE_CHANGE_PMU(
                                                                    pChangeSeq,
                                                                    pStepSuper);
            }
            break;
        }
        case LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_POST_CHANGE_PMU:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_CHANGE_PMU))
            {
                status = perfDaemonChangeSeqScriptExelwteStep_POST_CHANGE_PMU(
                                                                    pChangeSeq,
                                                                    pStepSuper);
            }
            break;
        }
        case LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_LPWR:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_LPWR))
            {
                status = perfDaemonChangeSeqScriptExelwteStep_LPWR(
                                                                    pChangeSeq,
                                                                    pStepSuper);
            }
            break;
        }
        case LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_BIF:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_BIF))
            {
                status = perfDaemonChangeSeqScriptExelwteStep_BIF(
                                                                    pChangeSeq,
                                                                    pStepSuper);
            }
            break;
        }
        case LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_VOLT:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_VOLT))
            {
                status = perfDaemonChangeSeqScriptExelwteStep_VOLT(
                                                                    pChangeSeq,
                                                                    pStepSuper);
            }
            break;
        }
        case LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_XBAR_BOOST_VOLT:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_XBAR_BOOST_FOR_MCLK))
            {
                status = perfDaemonChangeSeqScriptExelwteStep_VOLT(
                                                                    pChangeSeq,
                                                                    pStepSuper);
            }
            break;
        }
        case LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_MEM_TUNE:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_MEM_TUNE_CHANGE))
            {
                status = perfDaemonChangeSeqScriptExelwteStep_MEM_TUNE(
                                                                    pChangeSeq,
                                                                    pStepSuper);
            }
            break;
        }
        default:
        {
            //
            // This is not a error. The step could you version specific.
            // Child class will handle their version specific steps.
            //
            status = FLCN_ERR_ILWALID_VERSION;
            break;
        }
    }

    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @copydoc PerfDaemonChangeSeqScriptInit()
 * @memberof CHANGE_SEQ
 * @private
 */
static FLCN_STATUS
s_perfDaemonChangeSeqScriptInit
(
    CHANGE_SEQ                         *pChangeSeq,
    CHANGE_SEQ_SCRIPT                  *pScript,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLwrr,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLast
)
{
    FLCN_STATUS status;

    switch (pChangeSeq->version)
    {
        case LW2080_CTRL_PERF_CHANGE_SEQ_VERSION_35:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_35))
            {
                status = perfDaemonChangeSeqScriptInit_35(pChangeSeq,
                        pScript, pChangeLwrr, pChangeLast);
            }
            break;
        }
        default:
        {
            status = FLCN_ERR_NOT_SUPPORTED;
            break;
        }
    }

    if (status != FLCN_OK)
    {
        // Ignore the warnings as they will be handled in PERF task.
        if (status != FLCN_WARN_NOTHING_TO_DO)
        {
            PMU_BREAKPOINT();
        }
    }

    return status;
}

/*!
 * @brief Exelwtes a change sequencer PMU script.
 * @memberof CHANGE_SEQ
 * @private
 *
 * @param[in]   pChangeSeq  CHANGE_SEQ pointer
 * @param[in]   pScript     CHANGE_SEQ_SCRIPT pointer
 *
 * @return FLCN_OK if the function completed successfully
 * @return Detailed status code on error
 */
FLCN_STATUS
perfDaemonChangeSeqScriptExelwteAllSteps
(
    CHANGE_SEQ         *pChangeSeq,
    CHANGE_SEQ_SCRIPT  *pScript
)
{
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_HEADER
                                           *pHdr       = &pScript->hdr.data;
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER
                                           *pStepSuper = NULL;
    FLCN_STATUS                             status     = FLCN_OK;

    // Execute all conselwtive PMU based steps.
    while (pHdr->lwrStepIndex < pHdr->numSteps)
    {
        LwU8 cachedStepIndex = pHdr->lwrStepIndex;

        // Read in the step.
        status = perfDaemonChangeSeqRead_STEP(pChangeSeq, pScript,
                    &pStepSuper, cachedStepIndex, LW_FALSE);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfDaemonChangeSeqScriptExelwteAllSteps_exit;
        }

        // Init total time
        pStepSuper->profiling.totalTimens.lo = 0;
        pStepSuper->profiling.totalTimens.hi = 0;

        // Begin counting total time for each step
        PERF_CHANGE_SEQ_PROFILE_BEGIN(
            &pStepSuper->profiling.totalTimens);

        //
        // Given that RM will execute all conselwtive RM steps the profiling
        // data will be sum of time required to process them all.
        //

        // If the next step is CPU based step, notify CPU.
        if ((pChangeSeq->cpuStepIdMask & LWBIT32(pStepSuper->stepId)) != 0U)
        {

            // Flush the current step to upload the time stamps.
            status = perfDaemonChangeSeqFlush_STEP(pChangeSeq, pScript,
                        pStepSuper, cachedStepIndex, LW_TRUE);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto perfDaemonChangeSeqScriptExelwteAllSteps_exit;
            }

            status = s_perfDaemonChangeSeqScriptSendToRM(pChangeSeq, pScript);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto perfDaemonChangeSeqScriptExelwteAllSteps_exit;
            }

            // Read back the RM step of cached index to update the timestamps.
            status = perfDaemonChangeSeqRead_STEP(pChangeSeq, pScript,
                    &pStepSuper, cachedStepIndex, LW_TRUE);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto perfDaemonChangeSeqScriptExelwteAllSteps_exit;
            }


        }
        // Consider CPU unclaimed steps as NOP.
        else if ((pChangeSeq->cpuAdvertisedStepIdMask & LWBIT32(pStepSuper->stepId)) != 0U)
        {
            // Increment the step index.
            pHdr->lwrStepIndex++;
            continue;
        }
        // PMU based step.
        else
        {
            // Init total time
            pStepSuper->profiling.pmuThreadTimens.lo = 0;
            pStepSuper->profiling.pmuThreadTimens.hi = 0;

            // Begin counting PMU thread exelwtion time.
            PERF_CHANGE_SEQ_PROFILE_BEGIN(
                &pStepSuper->profiling.pmuThreadTimens);

            status = s_perfDaemonChangeSeqScriptExelwteStep(pChangeSeq,
                        pStepSuper);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto perfDaemonChangeSeqScriptExelwteAllSteps_exit;
            }

            // Increment the step index.
            pHdr->lwrStepIndex++;

            // Stop counting PMU thread exelwtion time.
            PERF_CHANGE_SEQ_PROFILE_END(
                &pStepSuper->profiling.pmuThreadTimens);
        }

        // Stop counting PMU thread exelwtion time.
        PERF_CHANGE_SEQ_PROFILE_END(
            &pStepSuper->profiling.totalTimens);

        status = perfDaemonChangeSeqFlush_STEP(pChangeSeq, pScript,
                    pStepSuper, cachedStepIndex, LW_FALSE);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfDaemonChangeSeqScriptExelwteAllSteps_exit;
        }
    }

perfDaemonChangeSeqScriptExelwteAllSteps_exit:
    return status;
}

/*!
 * @brief Exelwtes the a step in the change sequencer script.
 * @memberof CHANGE_SEQ
 * @private
 * @copydetails PerfDaemonChangeSeqScriptExelwteStep()
 */
static FLCN_STATUS
s_perfDaemonChangeSeqScriptExelwteStep
(
    CHANGE_SEQ                                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper
)
{
    FLCN_STATUS status;

    switch (pChangeSeq->version)
    {
        case LW2080_CTRL_PERF_CHANGE_SEQ_VERSION_35:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_35))
            {
                status = perfDaemonChangeSeqScriptExelwteStep_35(
                                                        pChangeSeq,
                                                        pStepSuper);
            }
            break;
        }
        default:
        {
            status = FLCN_ERR_NOT_SUPPORTED;
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @brief Send the perf change sequence script to RM and wait for the completion
 *        signal from RM.
 * @memberof CHANGE_SEQ
 * @private
 *
 * @note This function is not present in automotive builds.
 *
 * @param[in]   pChangeSeq  CHANGE_SEQ pointer
 * @param[in]   pScript     CHANGE_SEQ_SCRIPT pointer
 *
 * @return FLCN_OK if the RM exelwted the steps in the script; detailed error
 *         code otherwise
 */
static FLCN_STATUS
s_perfDaemonChangeSeqScriptSendToRM
(
    CHANGE_SEQ         *pChangeSeq,
    CHANGE_SEQ_SCRIPT  *pScript
)
{
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_HEADER  *pHdr     = NULL;
    DISPATCH_PERF_DAEMON_COMPLETION                 dispatch = {{0}};
    PMU_RM_RPC_STRUCT_PERF_SCRIPT_EXELWTE           rpc;
    FLCN_STATUS                                     status;

    // Upload the perf change sequence header to FB surface.
    status = perfDaemonChangeSeqFlush_HEADER(pChangeSeq, pScript);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfDaemonChangeSeqScriptSendToRM_exit;
    }

    PMU_RM_RPC_EXELWTE_BLOCKING(status, PERF, SCRIPT_EXELWTE, &rpc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfDaemonChangeSeqScriptSendToRM_exit;
    }

    // Wait for completion signal from RM.
    status = perfDaemonWaitForCompletionFromRM(&dispatch,
                NULL,
                PERF_DAEMON_COMPLETION_EVENT_ID_PERF_CHANGE_SEQ_SCRIPT_EXELWTE_COMPLETION,
                lwrtosMAX_DELAY);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfDaemonChangeSeqScriptSendToRM_exit;
    }

    // Download the perf change sequence header from FB surface.
    pHdr = perfDaemonChangeSeqRead_HEADER(pChangeSeq, pScript);
    if (pHdr == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto s_perfDaemonChangeSeqScriptSendToRM_exit;
    }

s_perfDaemonChangeSeqScriptSendToRM_exit:
    return status;
}

/*!
 * @todo PP-TODO
 * @memberof CHANGE_SEQ
 * @private
 * @note This function is not present in automotive builds.
 */
static FLCN_STATUS
s_perfDaemonChangeSeqFireNotificationCallback
(
    CHANGE_SEQ                                     *pChangeSeq,
    PERF_CHANGE_SEQ_NOTIFICATION_CLIENT_TASK_INFO  *pClientTaskInfo,
    RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_DATA   *pData
)
{
    DISPATCH_PERF_CHANGE_SEQ_NOTIFICATION   dispatchNotification = {{0}, };
    DISPATCH_PERF_DAEMON_COMPLETION         dispatchCompletion   = {{0}};
    FLCN_STATUS                             status               = FLCN_OK;

    // Create client notification
    dispatchNotification.hdr.eventType = pClientTaskInfo->taskEventId;
    dispatchNotification.data          = *pData;

    // Send notification signal to client task.
    status = lwrtosQueueSendBlocking(pClientTaskInfo->taskQueue,
                            &dispatchNotification,
                            sizeof(dispatchNotification));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfDaemonChangeSeqFireNotificationCallback_exit;
    }

    // Wait for completion signal from client
    status = perfDaemonWaitForCompletion(&dispatchCompletion,
                PERF_DAEMON_COMPLETION_EVENT_ID_PERF_CHANGE_SEQ_NOTIFICATION_COMPLETION,
                lwrtosMAX_DELAY);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfDaemonChangeSeqFireNotificationCallback_exit;
    }

s_perfDaemonChangeSeqFireNotificationCallback_exit:
    return status;
}

/*!
 * @brief   Helper interface to validate perf state is intact before
 *          injecting new perf change.
 *
 * @memberof CHANGE_SEQ
 * @private
 *
 * @param[in]   pChangeSeq    Change Sequencer pointer.
 */
static FLCN_STATUS
s_perfDaemonChangeSeqSendValidatePerfState
(
    CHANGE_SEQ *pChangeSeq
)
{
    LwU32               gpcClkIdx;
    LwU8                lwvddRailIdx;
    CLK_NAFLL_DEVICE   *pNafllDev;
    VOLT_RAIL          *pRail;
    FLCN_STATUS         status;

    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLast = pChangeSeq->pChangeLast;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVoltApi)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfClkAvfs)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Skip first perf change request.
        if (pChangeLast->data.pmu.seqId ==
                    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_SEQ_ID_ILWALID)
        {
            status = FLCN_OK;
            goto s_perfDaemonChangeSeqSendValidatePerfState_exit;
        }

        // Get GPC clock index in CLK HAL.
        status = clkDomainsGetIndexByApiDomain(CLK_DOMAIN_MASK_GPC, &gpcClkIdx);
        if (status != FLCN_OK)
        {
            CHSEQ_FAILURE_EX(status);
            goto s_perfDaemonChangeSeqSendValidatePerfState_exit;
        }

        // Get LWVDD volt rail index in VOLT HAL.
        lwvddRailIdx = voltRailVoltDomainColwertToIdx(LW2080_CTRL_VOLT_VOLT_DOMAIN_LOGIC);
        if (lwvddRailIdx == LW2080_CTRL_VOLT_VOLT_RAIL_INDEX_ILWALID)
        {
            status = FLCN_ERR_ILWALID_INDEX;
            CHSEQ_FAILURE_EX(status);
            goto s_perfDaemonChangeSeqSendValidatePerfState_exit;
        }

        // Get volt rail pointer.
        pRail = VOLT_RAIL_GET(lwvddRailIdx);
        if (pRail == NULL)
        {
            status = FLCN_ERR_ILWALID_INDEX;
            CHSEQ_FAILURE_EX(status);
            goto s_perfDaemonChangeSeqSendValidatePerfState_exit;
        }

        // Get NAFLL pointer.
        pNafllDev = clkNafllPrimaryDeviceGet(LW2080_CTRL_CLK_DOMAIN_GPCCLK);
        if (pNafllDev == NULL)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            CHSEQ_FAILURE_EX(status);
            goto s_perfDaemonChangeSeqSendValidatePerfState_exit;
        }

        // Counter tracking all changes.
        g_CHSEQ_DEBUG[10]++;
        // Exact negation of when we skip perf restore.
        if (!(((pChangeLast->vfPointsCacheCounter !=
                clkVfPointsVfPointsCacheCounterGet()) &&
              (pChangeLast->vfPointsCacheCounter !=
                LW2080_CTRL_CLK_CLK_VF_POINT_CACHE_COUNTER_TOOLS)) &&
             ((pChangeLast->clkList.clkDomains[gpcClkIdx].regimeId ==
                LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR) ||
              (pChangeLast->clkList.clkDomains[gpcClkIdx].regimeId ==
                LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR_BELOW_DVCO_MIN))))
        {
            LwU32 voltageuV                 = RM_PMU_VOLT_VALUE_0V_IN_UV;
            LwU32 voltageNoiseUnawareVminuV = RM_PMU_VOLT_VALUE_0V_IN_UV;

            // Counter tracking all changes where we should have restored perf after GPC-RG.
            g_CHSEQ_DEBUG[11]++;

            status = voltRailGetVoltage(pRail, &voltageuV);
            if (status != FLCN_OK)
            {
                CHSEQ_FAILURE_EX(status);
                goto s_perfDaemonChangeSeqSendValidatePerfState_exit;
            }

            status = voltRailGetNoiseUnawareVmin(pRail, &voltageNoiseUnawareVminuV);
            if (status != FLCN_OK)
            {
                CHSEQ_FAILURE_EX(status);
                goto s_perfDaemonChangeSeqSendValidatePerfState_exit;
            }

            g_CHSEQ_DEBUG[1] = pChangeLast->voltList.rails[lwvddRailIdx].voltageuV;
            g_CHSEQ_DEBUG[2] = voltageuV;
            g_CHSEQ_DEBUG[3] = pChangeLast->voltList.rails[lwvddRailIdx].voltageMinNoiseUnawareuV;
            g_CHSEQ_DEBUG[4] = voltageNoiseUnawareVminuV;
            g_CHSEQ_DEBUG[5] = pChangeLast->clkList.clkDomains[gpcClkIdx].regimeId;
            g_CHSEQ_DEBUG[6] = clkNafllRegimeGet(pNafllDev);
            g_CHSEQ_DEBUG[7] = pChangeLast->clkList.clkDomains[gpcClkIdx].clkFreqKHz / 1000;
            g_CHSEQ_DEBUG[8] = clkNafllFreqMHzGet(pNafllDev);
            g_CHSEQ_DEBUG[9] = pChangeLast->clkList.clkDomains[gpcClkIdx].clkDomain;

            if (pChangeLast->clkList.clkDomains[gpcClkIdx].clkDomain != LW2080_CTRL_CLK_DOMAIN_GPCCLK)
            {
                status = FLCN_ERR_ILWALID_STATE;
                CHSEQ_FAILURE_EX(status);
                goto s_perfDaemonChangeSeqSendValidatePerfState_exit;
            }

            if (pChangeLast->clkList.clkDomains[gpcClkIdx].regimeId != clkNafllRegimeGet(pNafllDev))
            {
                status = FLCN_ERR_ILWALID_STATE;
                CHSEQ_FAILURE_EX(status);
                goto s_perfDaemonChangeSeqSendValidatePerfState_exit;
            }

            if ((pChangeLast->clkList.clkDomains[gpcClkIdx].clkFreqKHz / 1000) != clkNafllFreqMHzGet(pNafllDev))
            {
                status = FLCN_ERR_ILWALID_STATE;
                CHSEQ_FAILURE_EX(status);
                goto s_perfDaemonChangeSeqSendValidatePerfState_exit;
            }

            //
            // PP-TODO Disable voltage restore sanity check as to aclwrately perform
            // it change sequencer needs to implement lot of logic for controller delta
            // adjustment and Vmax - Vmin bound trimming.
            //
            // if (pChangeLast->voltList.rails[lwvddRailIdx].voltageuV != voltageuV)
            // {
            //     status = FLCN_ERR_ILWALID_STATE;
            //     CHSEQ_FAILURE_EX(status);
            //     goto s_perfDaemonChangeSeqSendValidatePerfState_exit;
            // }

            // if (pChangeLast->voltList.rails[lwvddRailIdx].voltageMinNoiseUnawareuV != voltageNoiseUnawareVminuV)
            // {
            //     status = FLCN_ERR_ILWALID_STATE;
            //     CHSEQ_FAILURE_EX(status);
            //     goto s_perfDaemonChangeSeqSendValidatePerfState_exit;
            // }
        }
s_perfDaemonChangeSeqSendValidatePerfState_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

