/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    changeseq_lpwr_daemon.c
 * @copydoc changeseq_lpwr_daemon.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#if (PMU_PROFILE_GA10X_RISCV)
#include <lwriscv/print.h>
#endif

/* ------------------------ Application Includes ---------------------------- */
#include "cmdmgmt/cmdmgmt.h"
#include "pmu_objperf.h"
#include "pmu_objclk.h"
#include "pmu_objbif.h"
#include "pmu_objpmgr.h"
#include "pmu_objpg.h"
#include "pmu_objgr.h"
#include "pmu_objlpwr.h"
#include "volt/objvolt.h"
#include "task_perf_daemon.h"
#include "task_perf.h"
#include "perf/perf_daemon.h"
#include "clk/pmu_clklpwr.h"

#include "g_pmurpc.h"
#include "g_pmurmifrpc.h"
#include "dev_pwr_csb.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Global variables Defines   ---------------------- */
LwU32 g_CHSEQ_LPWR_DEBUG[18] = {0,};

/* ------------------------ Macros and Defines ------------------------------ */
// PP-TODO : Send RPC to RM with debug info instead of abusing PMU MAILBOX.
GCC_ATTRIB_ALWAYSINLINE()
static inline void CHSEQ_FAILURE_EX_PRINT(void)
{
    LwU8 i;
    for (i = 0; i < 12; i++)
    {
#if (PMU_PROFILE_GA10X_RISCV)
        {
            dbgPrintf(LEVEL_ALWAYS, "Change Sequencer LPWR Restore:: g_CHSEQ_LPWR_DEBUG[%d] = %d\n.",
                i,
                g_CHSEQ_LPWR_DEBUG[i]);
        }
#endif
    }
    return;
}

// PP-TODO : Send RPC to RM with debug info instead of abusing PMU MAILBOX.
#define CHSEQ_FAILURE_EX(status)                                               \
    do                                                                         \
    {                                                                          \
        LwU8 i;                                                                \
        g_CHSEQ_LPWR_DEBUG[0] =                                                \
            ((((status) & 0xffff) << 16) | (__LINE__ & 0xffff));               \
        for (i = 0; i < 12; i++)                                               \
        {                                                                      \
            REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(i), g_CHSEQ_LPWR_DEBUG[i]);      \
        }                                                                      \
        CHSEQ_FAILURE_EX_PRINT();                                              \
        PMU_HALT();                                                            \
    } while (LW_FALSE)

/* ------------------------ Function Prototypes ----------------------------- */
static FLCN_STATUS  s_perfDaemonChangeSeqLpwrScriptExelwte(CHANGE_SEQ *pChangeSeq, LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_SCRIPT *pScript)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeqLpwr", "s_perfDaemonChangeSeqLpwrScriptExelwte");
static PerfDaemonChangeSeqScriptExelwteStep    (s_perfDaemonChangeSeqLpwrScriptExelwteStep)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeqLpwr", "s_perfDaemonChangeSeqLpwrScriptExelwteStep");
static PerfDaemonChangeSeqScriptExelwteStep    (s_perfDaemonChangeSeqScriptExelwteStep_CLK_RESTORE)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeqLpwr", "s_perfDaemonChangeSeqScriptExelwteStep_CLK_RESTORE");
static PerfDaemonChangeSeqScriptExelwteStep    (s_perfDaemonChangeSeqScriptExelwteStep_CLK_DEINIT)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeqLpwr", "s_perfDaemonChangeSeqScriptExelwteStep_CLK_DEINIT");
static PerfDaemonChangeSeqScriptExelwteStep    (s_perfDaemonChangeSeqScriptExelwteStep_CLK_INIT)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeqLpwr", "s_perfDaemonChangeSeqScriptExelwteStep_CLK_INIT");
static PerfDaemonChangeSeqScriptExelwteStep    (s_perfDaemonChangeSeqScriptExelwteStep_VOLT_GATE)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeqLpwr", "s_perfDaemonChangeSeqScriptExelwteStep_VOLT_GATE");
static PerfDaemonChangeSeqScriptExelwteStep    (s_perfDaemonChangeSeqScriptExelwteStep_VOLT_UNGATE)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeqLpwr", "s_perfDaemonChangeSeqScriptExelwteStep_VOLT_UNGATE");
static PerfDaemonChangeSeqScriptExelwteStep    (s_perfDaemonChangeSeqScriptExelwteStep_PRE_VOLT_GATE_LPWR)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeqLpwr", "s_perfDaemonChangeSeqScriptExelwteStep_PRE_VOLT_GATE_LPWR");
static PerfDaemonChangeSeqScriptExelwteStep    (s_perfDaemonChangeSeqScriptExelwteStep_POST_VOLT_GATE_LPWR)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeqLpwr", "s_perfDaemonChangeSeqScriptExelwteStep_POST_VOLT_GATE_LPWR");
static PerfDaemonChangeSeqScriptExelwteStep    (s_perfDaemonChangeSeqScriptExelwteStep_PRE_VOLT_UNGATE_LPWR)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeqLpwr", "s_perfDaemonChangeSeqScriptExelwteStep_PRE_VOLT_UNGATE_LPWR");
static PerfDaemonChangeSeqScriptExelwteStep    (s_perfDaemonChangeSeqScriptExelwteStep_POST_VOLT_UNGATE_LPWR)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeqLpwr", "s_perfDaemonChangeSeqScriptExelwteStep_POST_VOLT_UNGATE_LPWR");
static FLCN_STATUS  s_perfDaemonChangeSeqScriptVoltRailGate(CHANGE_SEQ *pChangeSeq, LwBool bGate)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeqLpwr", "s_perfDaemonChangeSeqScriptVoltRailGate");
static FLCN_STATUS  s_perfDaemonChangeSeqLpwrPerfRestore(CHANGE_SEQ *pChangeSeq)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeqLpwr", "s_perfDaemonChangeSeqLpwrPerfRestore");
static PerfDaemonChangeSeqScriptInit           (s_perfDaemonChangeSeqLpwrPerfRestoreBuild)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeqLpwr", "s_perfDaemonChangeSeqLpwrPerfRestoreBuild");
static PerfDaemonChangeSeqScriptInit           (s_perfDaemonChangeSeqLpwrPerfRestoreVfValidation)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeqLpwr", "s_perfDaemonChangeSeqLpwrPerfRestoreVfValidation");

/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief   Public interface to execute perf change sequencer LPWR feature scripts.
 *
 * @note    Dependening on the latency requirements, LPWR task will either
 *          directly call this interface after proper synchronization with
 *          perf daemon task through shared semaphore or queue the request
 *          to the perf daemon task queue. On GA10x, LPWR task will directly
 *          call this interface for entry and exit script processing but
 *          use the inter task communication for post processing script
 *          exelwtion. @ref perfDaemonChangeSeqLpwrPostScriptExelwte
 *
 * @param[in] lpwrScriptId  LPWR script id.
 *                          @ref LW2080_CTRL_PERF_CHANGE_SEQ_SCRIPT_ID_LPWR_<xyz>
 */
FLCN_STATUS
perfDaemonChangeSeqLpwrScriptExelwte
(
    LwU8    lpwrScriptId
)
{
    CHANGE_SEQ         *pChangeSeq     = PERF_CHANGE_SEQ_GET();
    CHANGE_SEQ_LPWR    *pChangeSeqLpwr = PERF_CHANGE_SEQ_LPWR_GET(pChangeSeq);
    FLCN_STATUS         status     = FLCN_OK;

    LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_SCRIPT        *pScript =
        PERF_CHANGE_SEQ_LPWR_SCRIPT_GET(pChangeSeqLpwr, lpwrScriptId);
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_HEADER  *pHdr    = NULL;

    // Update debugging info.
    g_CHSEQ_LPWR_DEBUG[1] = (lpwrScriptId);

    if (pScript == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        CHSEQ_FAILURE_EX(status);
        goto perfDaemonChangeSeqLpwrScriptExelwte_exit;
    }
    pHdr = &pScript->hdr;

    // Update debugging info.
    g_CHSEQ_LPWR_DEBUG[2] = (pHdr->scriptId);
    g_CHSEQ_LPWR_DEBUG[3] = (pScript->seqId);
    g_CHSEQ_LPWR_DEBUG[4] = (pChangeSeqLpwr->pChangeSeqLpwrScripts->seqId);
    g_CHSEQ_LPWR_DEBUG[5] = (pChangeSeq->stepIdExclusionMask);

    // Validate the script.
    if (pHdr->scriptId != lpwrScriptId)
    {
        status = FLCN_ERR_ILWALID_STATE;
        CHSEQ_FAILURE_EX(status);
        goto perfDaemonChangeSeqLpwrScriptExelwte_exit;
    }

    // Update global sequence id on entry script exelwtion request.
    if (lpwrScriptId == LW2080_CTRL_PERF_CHANGE_SEQ_SCRIPT_ID_LPWR_ENTRY)
    {
        // Update debugging info.
        g_CHSEQ_LPWR_DEBUG[16]++;

        // Update the perf state with respect to this LPWR feature.
        pChangeSeqLpwr->pChangeSeqLpwrScripts->bEngaged = LW_TRUE;

        // Increment the global sequence id.
        LW2080_CTRL_PERF_PERF_CHANGE_SEQ_SEQ_ID_INCREMENT(
            &pChangeSeqLpwr->pChangeSeqLpwrScripts->seqId);
    }

    // Update per script sequence id and validate.
    LW2080_CTRL_PERF_PERF_CHANGE_SEQ_SEQ_ID_INCREMENT(&pScript->seqId);
    if (pChangeSeqLpwr->pChangeSeqLpwrScripts->seqId != pScript->seqId)
    {
        // Update debugging info.
        g_CHSEQ_LPWR_DEBUG[6] = (pScript->seqId);
        g_CHSEQ_LPWR_DEBUG[7] = (pChangeSeqLpwr->pChangeSeqLpwrScripts->seqId);

        status = FLCN_ERR_ILWALID_STATE;
        CHSEQ_FAILURE_EX(status);
        goto perfDaemonChangeSeqLpwrScriptExelwte_exit;
    }

    // Reset the current step.
    pHdr->lwrStepIndex = 0;

    // Execute script.

    // Begin counting exelwtion time.
    PERF_CHANGE_SEQ_PROFILE_BEGIN(&pHdr->profiling.totalBuildTimens);
    {
        status = s_perfDaemonChangeSeqLpwrScriptExelwte(pChangeSeq, pScript);
    }
    // Stop counting exelwtion time.
    PERF_CHANGE_SEQ_PROFILE_END(&pHdr->profiling.totalBuildTimens);

    if (status != FLCN_OK)
    {
        CHSEQ_FAILURE_EX(status);
        goto perfDaemonChangeSeqLpwrScriptExelwte_exit;
    }

perfDaemonChangeSeqLpwrScriptExelwte_exit:
    return status;
}

/*!
 * @brief   Public interface to execute perf change sequencer LPWR
 *          feature post processing scripts.
 *
 * @copydoc perfDaemonChangeSeqLpwrScriptExelwte
 */
FLCN_STATUS
perfDaemonChangeSeqLpwrPostScriptExelwte
(
    LwU8    lpwrScriptId
)
{
    CHANGE_SEQ         *pChangeSeq     = PERF_CHANGE_SEQ_GET();
    CHANGE_SEQ_LPWR    *pChangeSeqLpwr = PERF_CHANGE_SEQ_LPWR_GET(pChangeSeq);
    FLCN_STATUS         status         = FLCN_OK;

    DISPATCH_LPWR                               dispatchLpwr = {{0}};
    LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_SCRIPT    *pScriptPost;
    LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_SCRIPT    *pScriptExit;

    // Update debugging info.
    g_CHSEQ_LPWR_DEBUG[1] = (lpwrScriptId);

    // Sanity check.
    if (pChangeSeqLpwr == NULL)
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        CHSEQ_FAILURE_EX(status);
        goto perfDaemonChangeSeqLpwrPostScriptExelwte_exit;
    }

    pScriptPost = PERF_CHANGE_SEQ_LPWR_SCRIPT_GET(pChangeSeqLpwr, lpwrScriptId);
    if (pScriptPost == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        CHSEQ_FAILURE_EX(status);
        goto perfDaemonChangeSeqLpwrPostScriptExelwte_exit;
    }

    pScriptExit = PERF_CHANGE_SEQ_LPWR_SCRIPT_GET(pChangeSeqLpwr,
                    LW2080_CTRL_PERF_CHANGE_SEQ_SCRIPT_ID_LPWR_EXIT);
    if (pScriptExit == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        CHSEQ_FAILURE_EX(status);
        goto perfDaemonChangeSeqLpwrPostScriptExelwte_exit;
    }

    //
    // Skip redundant exelwtion requests by comparing the post processing
    // script sequence id with exit script sequence id. We cannot use
    // global sequence id which tracks entry script as with very low idle
    // threshold we might re-enter GPC-RG before change sequencer get chance
    // to flush out the inflight redundant post processing request.
    //
    if (pScriptPost->seqId == pScriptExit->seqId)
    {
        // Update debugging info.
        g_CHSEQ_LPWR_DEBUG[10]++;

        //
        // Explicit inline return to skip sending completion signal
        // on redundant cmd.
        //
        return FLCN_OK;
    }
    else if (pScriptPost->seqId > pScriptExit->seqId)
    {
        // Update debugging info.
        g_CHSEQ_LPWR_DEBUG[6] = (pScriptPost->seqId);
        g_CHSEQ_LPWR_DEBUG[7] = (pScriptExit->seqId);

        status = FLCN_ERR_ILWALID_STATE;
        CHSEQ_FAILURE_EX(status);
        goto perfDaemonChangeSeqLpwrPostScriptExelwte_exit;
    }

    status = perfDaemonChangeSeqLpwrScriptExelwte(lpwrScriptId);
    if (status != FLCN_OK)
    {
        CHSEQ_FAILURE_EX(status);
        goto perfDaemonChangeSeqLpwrPostScriptExelwte_exit;
    }

    // Perform perf restore to last programmed perf state.
    if ((PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_LPWR_PERF_RESTORE)) &&
        (!perfChangeSeqLpwrPerfRestoreDisableGet(pChangeSeq)))
    {
        status = s_perfDaemonChangeSeqLpwrPerfRestore(pChangeSeq);
        if (status != FLCN_OK)
        {
            CHSEQ_FAILURE_EX(status);
            goto perfDaemonChangeSeqLpwrPostScriptExelwte_exit;
        }
    }

perfDaemonChangeSeqLpwrPostScriptExelwte_exit:

    // Update debugging info.
    g_CHSEQ_LPWR_DEBUG[15]++;

    // Update the perf state with respect to this LPWR feature.
    pChangeSeqLpwr->pChangeSeqLpwrScripts->bEngaged = LW_FALSE;

    // Send completion event to LPWR task.
    dispatchLpwr.hdr.eventType = LPWR_EVENT_ID_GR_RG_PERF_SCRIPT_COMPLETION;

    (void)lwrtosQueueIdSendBlocking(LWOS_QUEUE_ID(PMU, LPWR),
            &dispatchLpwr, sizeof(dispatchLpwr));

    return status;
}

/*!
 * Helper interface to de-init clocks on LPWR feature entry.
 *
 * @copydoc LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_CLK_DEINIT
 * @copydoc PerfDaemonChangeSeqScriptExelwteStep
 */
FLCN_STATUS
perfDaemonChangeSeqScriptExelwteStep_CLK_DEINIT()
{
    FLCN_STATUS     status;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, clkLpwr)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // De initialize clocks for GPC-RG
        status = clkGrRgDeInit(LW2080_CTRL_CLK_DOMAIN_GPCCLK);
        if (status != FLCN_OK)
        {
            CHSEQ_FAILURE_EX(status);
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * Helper interface to init NAFLL clocks on LPWR feature exit.
 *
 * @copydoc LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_CLK_INIT
 * @copydoc PerfDaemonChangeSeqScriptExelwteStep
 */
FLCN_STATUS
perfDaemonChangeSeqScriptExelwteStep_CLK_INIT()
{
    FLCN_STATUS     status;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, clkLpwr)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Initialize clocks for GPC-RG critical part of exit
        status = clkGrRgInit(LW2080_CTRL_CLK_DOMAIN_GPCCLK);
        if (status != FLCN_OK)
        {
            CHSEQ_FAILURE_EX(status);
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * Helper interface to restore clock settings, this is for non-critical exit.
 *
 * @copydoc LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_CLK_RESTORE
 * @copydoc PerfDaemonChangeSeqScriptExelwteStep
 */
FLCN_STATUS
perfDaemonChangeSeqScriptExelwteStep_CLK_RESTORE()
{
    FLCN_STATUS     status;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, clkLpwr)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Restore clock settings for GPC-RG non-critical exit
        status = clkGrRgRestore(LW2080_CTRL_CLK_DOMAIN_GPCCLK);
        if (status != FLCN_OK)
        {
            CHSEQ_FAILURE_EX(status);
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * Helper interface to execute the perf change sequence LPWR script.
 *
 * @param[in]   pChangeSeq  CHANGE_SEQ pointer
 * @param[in]   pScript     LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_SCRIPT pointer
 */
static FLCN_STATUS
s_perfDaemonChangeSeqLpwrScriptExelwte
(
    CHANGE_SEQ                                 *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_SCRIPT    *pScript
)
{
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_HEADER
                                            *pHdr       = &pScript->hdr;
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER
                                            *pStepSuper = NULL;
    FLCN_STATUS                             status     = FLCN_OK;

    // Execute all conselwtive PMU based steps.
    while (pHdr->lwrStepIndex < pHdr->numSteps)
    {
        // Get the step pointer.
        pStepSuper = &pScript->steps[pHdr->lwrStepIndex].data.super;

        // Update debugging info
        g_CHSEQ_LPWR_DEBUG[8] = (pHdr->lwrStepIndex);
        g_CHSEQ_LPWR_DEBUG[9] = (pHdr->numSteps);

        // Init profile data
        (void)memset(&(pStepSuper->profiling), 0, sizeof(pStepSuper->profiling));

        // Begin counting total time for each step
        PERF_CHANGE_SEQ_PROFILE_BEGIN(
            &pStepSuper->profiling.totalTimens);

        // Skip excluded steps from processing.
        if ((pChangeSeq->stepIdExclusionMask & LWBIT32(pStepSuper->stepId)) == 0U)
        {
            // Begin counting PMU thread exelwtion time.
            PERF_CHANGE_SEQ_PROFILE_BEGIN(
                &pStepSuper->profiling.pmuThreadTimens);

            status = s_perfDaemonChangeSeqLpwrScriptExelwteStep(pChangeSeq,
                                                                pStepSuper);
            if (status != FLCN_OK)
            {
                CHSEQ_FAILURE_EX(status);
                goto s_perfDaemonChangeSeqLpwrScriptExelwte_exit;
            }

            // Stop counting PMU thread exelwtion time.
            PERF_CHANGE_SEQ_PROFILE_END(
                &pStepSuper->profiling.pmuThreadTimens);
        }

        // Increment the step index.
        pHdr->lwrStepIndex++;

        // Stop counting PMU thread exelwtion time.
        PERF_CHANGE_SEQ_PROFILE_END(
            &pStepSuper->profiling.totalTimens);
    }

s_perfDaemonChangeSeqLpwrScriptExelwte_exit:
    return status;
}

/*!
 * Helper interface to execute the perf change sequence
 * LPWR script step.
 *
 * @copydoc PerfDaemonChangeSeqScriptExelwteStep
 */
static FLCN_STATUS
s_perfDaemonChangeSeqLpwrScriptExelwteStep
(
    CHANGE_SEQ                                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (pStepSuper->stepId)
    {
        case LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_CLK_RESTORE:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_LPWR_STEP_CLK))
            {
                status = s_perfDaemonChangeSeqScriptExelwteStep_CLK_RESTORE(
                                                                    pChangeSeq,
                                                                    pStepSuper);
            }
            break;
        }
        case LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_CLK_DEINIT:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_LPWR_STEP_CLK))
            {
                status = s_perfDaemonChangeSeqScriptExelwteStep_CLK_DEINIT(
                                                                    pChangeSeq,
                                                                    pStepSuper);
            }
            break;
        }
        case LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_CLK_INIT:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_LPWR_STEP_CLK))
            {
                status = s_perfDaemonChangeSeqScriptExelwteStep_CLK_INIT(
                                                                    pChangeSeq,
                                                                    pStepSuper);
            }
            break;
        }
        case LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_VOLT_GATE:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_LPWR_STEP_VOLT))
            {
                status = s_perfDaemonChangeSeqScriptExelwteStep_VOLT_GATE(
                                                                    pChangeSeq,
                                                                    pStepSuper);
            }
            break;
        }
        case LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_VOLT_UNGATE:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_LPWR_STEP_VOLT))
            {
                status = s_perfDaemonChangeSeqScriptExelwteStep_VOLT_UNGATE(
                                                                    pChangeSeq,
                                                                    pStepSuper);

            }
            break;
        }
        case LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_PRE_VOLT_GATE_LPWR:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_LPWR_STEP_LPWR))
            {
                status = s_perfDaemonChangeSeqScriptExelwteStep_PRE_VOLT_GATE_LPWR(
                                                                    pChangeSeq,
                                                                    pStepSuper);

            }
            break;
        }
        case LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_POST_VOLT_GATE_LPWR:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_LPWR_STEP_LPWR))
            {
                status = s_perfDaemonChangeSeqScriptExelwteStep_POST_VOLT_GATE_LPWR(
                                                                    pChangeSeq,
                                                                    pStepSuper);
            }
            break;
        }
        case LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_PRE_VOLT_UNGATE_LPWR:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_LPWR_STEP_LPWR))
            {
                status = s_perfDaemonChangeSeqScriptExelwteStep_PRE_VOLT_UNGATE_LPWR(
                                                                    pChangeSeq,
                                                                    pStepSuper);
            }
            break;
        }
        case LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_POST_VOLT_UNGATE_LPWR:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_LPWR_STEP_LPWR))
            {
                status = s_perfDaemonChangeSeqScriptExelwteStep_POST_VOLT_UNGATE_LPWR(
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
        CHSEQ_FAILURE_EX(status);
    }

    return status;
}

/*!
 * Helper interface to de-init clocks on LPWR feature entry.
 *
 * @copydoc LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_CLK_DEINIT
 * @copydoc PerfDaemonChangeSeqScriptExelwteStep
 */
static FLCN_STATUS
s_perfDaemonChangeSeqScriptExelwteStep_CLK_DEINIT
(
    CHANGE_SEQ                                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper
)
{
    FLCN_STATUS status;

    //
    // PP-TODO : Update in follow up CLs.
    // Create public OBJCLK interface for the following.
    //  1. Disable NAFLL LUT and Frequency Override registers Access.
    //  2. Disable CLVC and CLFC controllers on GPC clock domain.
    //  3. Cache Clock Counters and disable Clock Counters HW Access
    //  4. Cache ADC Counters and disable ADC Counters HW Access
    //

    //
    // Call helper interface to perform rest of the de-init that does not
    // require NAFLL NL. This interface will be used by LPWR GPC-RG code
    // in RTL where we do not have support for NAFLL.
    //
    status = perfDaemonChangeSeqScriptExelwteStep_CLK_DEINIT();
    if (status != FLCN_OK)
    {
        CHSEQ_FAILURE_EX(status);
    }

    return status;
}

/*!
 * Helper interface to init NAFLL clocks on LPWR feature exit.
 *
 * @copydoc LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_CLK_INIT
 * @copydoc PerfDaemonChangeSeqScriptExelwteStep
 */
static FLCN_STATUS
s_perfDaemonChangeSeqScriptExelwteStep_CLK_INIT
(
    CHANGE_SEQ                                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper
)
{
    FLCN_STATUS status;

    //
    // Call helper interface to perform rest of the init that does not
    // require change sequencer. This interface will be used by LPWR GPC-RG
    // code in RTL where we do not have support for change sequencer.
    //
    status = perfDaemonChangeSeqScriptExelwteStep_CLK_INIT();
    if (status != FLCN_OK)
    {
        CHSEQ_FAILURE_EX(status);
    }

    return status;
}

/*!
 * Helper interface to init clocks on LPWR feature exit.
 *
 * @copydoc LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_CLK_RESTORE
 * @copydoc PerfDaemonChangeSeqScriptExelwteStep
 */
static FLCN_STATUS
s_perfDaemonChangeSeqScriptExelwteStep_CLK_RESTORE
(
    CHANGE_SEQ                                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper
)
{
    FLCN_STATUS status;

    //
    // Call helper interface to perform non-critical clocks restore that does
    // not require change sequencer. This interface will be used by LPWR GPC-RG
    // code in RTL where we do not have support for change sequencer.
    //
    status = perfDaemonChangeSeqScriptExelwteStep_CLK_RESTORE();
    if (status != FLCN_OK)
    {
        CHSEQ_FAILURE_EX(status);
    }

    return status;
}

/*!
 * Helper interface to gate voltage rail specificed in VOLT LIST.
 *
 * @copydoc LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_VOLT_GATE
 * @copydoc PerfDaemonChangeSeqScriptExelwteStep
 */
static FLCN_STATUS
s_perfDaemonChangeSeqScriptExelwteStep_VOLT_GATE
(
    CHANGE_SEQ                                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper
)
{
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_VOLT    *pStepVolt =
        (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_VOLT *)pStepSuper;
    FLCN_STATUS status;

    if (VOLT_RAILS_GET_VOLT_DOMAIN_HAL() != LW2080_CTRL_VOLT_VOLT_DOMAIN_HAL_GA10X_MULTI_RAIL)
    {
        //
        // PP-TODO : Update in follow up CLs.
        // Create public OBJVOLT interface for the following.
        //  1. Program LWVDD to ZERO.
        //  2. Power Down LWVDD Regulator by programming EN GPIO
        //
        status = s_perfDaemonChangeSeqScriptVoltRailGate(pChangeSeq, LW_TRUE);
    }
    else
    {
        status = voltVoltSetVoltage(LW2080_CTRL_VOLT_VOLT_POLICY_CLIENT_PERF_CORE_VF_SEQ,
                                        &(pStepVolt->voltList));
    }
    if (status != FLCN_OK)
    {
        CHSEQ_FAILURE_EX(status);
    }

    return status;
}

/*!
 * Helper interface to ungate voltage rail specificed in VOLT LIST.
 *
 * @copydoc LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_VOLT_UNGATE
 * @copydoc PerfDaemonChangeSeqScriptExelwteStep
 */
static FLCN_STATUS
s_perfDaemonChangeSeqScriptExelwteStep_VOLT_UNGATE
(
    CHANGE_SEQ                                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper
)
{
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_VOLT    *pStepVolt =
        (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_VOLT *)pStepSuper;
    FLCN_STATUS status;

    if (VOLT_RAILS_GET_VOLT_DOMAIN_HAL() != LW2080_CTRL_VOLT_VOLT_DOMAIN_HAL_GA10X_MULTI_RAIL)
    {
        //
        // PP-TODO : Update in follow up CLs.
        // Create public OBJVOLT interface for the following.
        //  1. Power Up LWVDD Regulator by programming EN GPIO
        //  2. Program LWVDD to boot voltage.
        //
        status = s_perfDaemonChangeSeqScriptVoltRailGate(pChangeSeq, LW_FALSE);
    }
    else
    {
        status = voltVoltSetVoltage(LW2080_CTRL_VOLT_VOLT_POLICY_CLIENT_PERF_CORE_VF_SEQ,
                                        &(pStepVolt->voltList));
    }
    if (status != FLCN_OK)
    {
        CHSEQ_FAILURE_EX(status);
    }

    return status;
}

/*!
 * Helper interface to perform LPWR sequence prior to voltage gate.
 *
 * @copydoc LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_PRE_VOLT_GATE_LPWR
 * @copydoc PerfDaemonChangeSeqScriptExelwteStep
 */
static FLCN_STATUS
s_perfDaemonChangeSeqScriptExelwteStep_PRE_VOLT_GATE_LPWR
(
    CHANGE_SEQ                                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper
)
{
    FLCN_STATUS status;

    status = grGrRgEntryPreVoltGateLpwr_HAL(&Gr);
    if (status != FLCN_OK)
    {
        CHSEQ_FAILURE_EX(status);
    }

    return status;
}

/*!
 * Helper interface to perform LPWR sequence post voltage gate.
 *
 * @copydoc LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_PRE_VOLT_GATE_LPWR
 * @copydoc PerfDaemonChangeSeqScriptExelwteStep
 */
static FLCN_STATUS
s_perfDaemonChangeSeqScriptExelwteStep_POST_VOLT_GATE_LPWR
(
    CHANGE_SEQ                                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper
)
{
    // This step is not required for the POR GPC-RG LPWR Sequences.
    CHSEQ_FAILURE_EX(FLCN_ERR_NOT_SUPPORTED);

    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * Helper interface to perform LPWR sequence pre voltage ungate.
 *
 * @copydoc LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_PRE_VOLT_UNGATE_LPWR
 * @copydoc PerfDaemonChangeSeqScriptExelwteStep
 */
static FLCN_STATUS
s_perfDaemonChangeSeqScriptExelwteStep_PRE_VOLT_UNGATE_LPWR
(
    CHANGE_SEQ                                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper
)
{
    FLCN_STATUS status;

    status = grGrRgExitPreVoltUngateLpwr_HAL(&Gr);
    if (status != FLCN_OK)
    {
        CHSEQ_FAILURE_EX(status);
    }

    return status;
}

/*!
 * Helper interface to perform LPWR sequence post voltage ungate.
 *
 * @copydoc LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_POST_VOLT_UNGATE_LPWR
 * @copydoc PerfDaemonChangeSeqScriptExelwteStep
 */
static FLCN_STATUS
s_perfDaemonChangeSeqScriptExelwteStep_POST_VOLT_UNGATE_LPWR
(
    CHANGE_SEQ                                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper
)
{
    FLCN_STATUS status;

    status = grGrRgExitPostVoltUngateLpwr_HAL(&Gr);
    if (status != FLCN_OK)
    {
        CHSEQ_FAILURE_EX(status);
    }

    return status;
}

/*!
 * @brief Interface to gate/ungate LWVDD rail
 *
 * @note  This is a temporary API for Arch testing. It will be replaced by
 *        the production level API from Volt team.
 *        Here, we are using GPIO 31 as its unused on both Turing and
 *        and Ampere pre-silicon as per VBIOS table
 *
 * @param[in] bGate     Whether to gate or ungate the rail
 */
static FLCN_STATUS
s_perfDaemonChangeSeqScriptVoltRailGate
(
    CHANGE_SEQ *pChangeSeq,
    LwBool      bGate
)
{
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libGr)
    };
    FLCN_STATUS     status;

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = grRgRailGate_HAL(&Gr, bGate);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    if (status != FLCN_OK)
    {
        CHSEQ_FAILURE_EX(status);
        goto s_perfDaemonChangeSeqScriptVoltRailGate_exit;
    }

s_perfDaemonChangeSeqScriptVoltRailGate_exit:
    return status;
}

/*!
 * @brief Interface to build and execute perf restore script.
 *
 * Change last - Boot clocks and voltage programmed by low power
 *               feature exit sequence.
 * Change lwrr - Last programmed VF switch clocks and voltage.
 *
 * @param[in] pChangeSeq    Change Sequencer pointer.
 *
 * @return FLCN_OK  Perf restore completed successfully
 * @return other errors coming from function calls.
 */
static FLCN_STATUS
s_perfDaemonChangeSeqLpwrPerfRestore
(
    CHANGE_SEQ     *pChangeSeq
)
{
    CHANGE_SEQ_LPWR                    *pChangeSeqLpwr  = PERF_CHANGE_SEQ_LPWR_GET(pChangeSeq);
    CHANGE_SEQ_SCRIPT                  *pScript         = &pChangeSeqLpwr->script;
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLwrr     = pChangeSeq->pChangeLast;
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLast     = pChangeSeqLpwr->pChangeLpwr;
    FLCN_STATUS                         status          = FLCN_OK;
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_HEADER
                                       *pHdr            = &pScript->hdr.data;

    // Attach overlays
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVolt)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVoltApi)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfClkAvfs)
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLKS_ON_PMU))           // Disabled on AUTO
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clk3x)
#endif
    };

    OSTASK_OVL_DESC ovlDescListExec[] = {
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLKS_ON_PMU))
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, clkLibClk3)
#endif
    };

    // Init the header structure
    (void)memset(&pScript->hdr, 0,
           sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_HEADER_ALIGNED));

    // Begin counting build time.
    PERF_CHANGE_SEQ_PROFILE_BEGIN(&pHdr->profiling.totalTimens);

    // Attach common overlays.
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // VF stability validation
        status = s_perfDaemonChangeSeqLpwrPerfRestoreVfValidation(pChangeSeq,
                                                                  pScript,
                                                                  pChangeLwrr,
                                                                  pChangeLast);
        if (status != FLCN_OK)
        {
            if (status == FLCN_WARN_NOTHING_TO_DO)
            {
                status = FLCN_OK;
            }
            else
            {
                CHSEQ_FAILURE_EX(status);
            }
            goto s_perfDaemonChangeSeqLpwrPerfRestore_exit;
        }

        // Build script
        status = s_perfDaemonChangeSeqLpwrPerfRestoreBuild(pChangeSeq,
                                                           pScript,
                                                           pChangeLwrr,
                                                           pChangeLast);
        if (status != FLCN_OK)
        {
            CHSEQ_FAILURE_EX(status);
            goto s_perfDaemonChangeSeqLpwrPerfRestore_exit;
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
            CHSEQ_FAILURE_EX(status);
            goto s_perfDaemonChangeSeqLpwrPerfRestore_exit;
        }

        // Stop counting build time.
        PERF_CHANGE_SEQ_PROFILE_END(&pHdr->profiling.totalTimens);

        // Update the last script with latest completed script.
        status = perfChangeSeqCopyScriptLwrrToScriptLast(pChangeSeq,
                                                         pScript,
                                                         pChangeLwrr);
        if (status != FLCN_OK)
        {
            CHSEQ_FAILURE_EX(status);
            goto s_perfDaemonChangeSeqLpwrPerfRestore_exit;
        }

s_perfDaemonChangeSeqLpwrPerfRestore_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @copydoc PerfDaemonChangeSeqScriptInit()
 * @memberof CHANGE_SEQ
 * @private
 */
static FLCN_STATUS
s_perfDaemonChangeSeqLpwrPerfRestoreVfValidation
(
    CHANGE_SEQ                         *pChangeSeq,
    CHANGE_SEQ_SCRIPT                  *pScript,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLwrr,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLast
)
{
    LwU32       gpcClkIdx;
    LwU8        lwvddRailIdx;
    FLCN_STATUS status = FLCN_OK;

    // Get GPC clock index in CLK HAL.
    status = clkDomainsGetIndexByApiDomain(CLK_DOMAIN_MASK_GPC, &gpcClkIdx);
    if (status != FLCN_OK)
    {
        CHSEQ_FAILURE_EX(status);
        goto s_perfDaemonChangeSeqLpwrPerfRestoreVfValidation_exit;
    }

    // Get LWVDD volt rail index in VOLT HAL.
    lwvddRailIdx = voltRailVoltDomainColwertToIdx(LW2080_CTRL_VOLT_VOLT_DOMAIN_LOGIC);
    if (lwvddRailIdx == LW2080_CTRL_VOLT_VOLT_RAIL_INDEX_ILWALID)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        CHSEQ_FAILURE_EX(status);
        goto s_perfDaemonChangeSeqLpwrPerfRestoreVfValidation_exit;
    }

    //
    // If the VF lwrve changed and the last programmed GPC clock was in
    // FFR regime, we cannot restore the same frequency as Vmin requirement
    // may have changed with change in VF lwrve. In this case we expect
    // a change already in queue with the latest VF lwrve so we will
    // directly program the latest VF switch request.
    //
    if (((pChangeLwrr->vfPointsCacheCounter !=
            clkVfPointsVfPointsCacheCounterGet()) &&
         (pChangeLwrr->vfPointsCacheCounter !=
            LW2080_CTRL_CLK_CLK_VF_POINT_CACHE_COUNTER_TOOLS)) &&
        ((pChangeLwrr->clkList.clkDomains[gpcClkIdx].regimeId ==
            LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR) ||
         (pChangeLwrr->clkList.clkDomains[gpcClkIdx].regimeId ==
            LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR_BELOW_DVCO_MIN)))
    {
        // Override gpc clock domain information
        pChangeLwrr->clkList.clkDomains[gpcClkIdx].clkDomain  =
            pChangeLast->clkList.clkDomains[gpcClkIdx].clkDomain;
        pChangeLwrr->clkList.clkDomains[gpcClkIdx].clkFreqKHz =
            pChangeLast->clkList.clkDomains[gpcClkIdx].clkFreqKHz;
        pChangeLwrr->clkList.clkDomains[gpcClkIdx].regimeId   =
            pChangeLast->clkList.clkDomains[gpcClkIdx].regimeId;
        pChangeLwrr->clkList.clkDomains[gpcClkIdx].source     =
            pChangeLast->clkList.clkDomains[gpcClkIdx].source;

        // Override lwvdd volt rail information
        pChangeLwrr->voltList.rails[lwvddRailIdx].railIdx   =
            pChangeLast->voltList.rails[lwvddRailIdx].railIdx;
        pChangeLwrr->voltList.rails[lwvddRailIdx].voltageuV =
            pChangeLast->voltList.rails[lwvddRailIdx].voltageuV;
        pChangeLwrr->voltList.rails[lwvddRailIdx].voltageMinNoiseUnawareuV =
            pChangeLast->voltList.rails[lwvddRailIdx].voltageMinNoiseUnawareuV;

        status = FLCN_WARN_NOTHING_TO_DO;
    }

s_perfDaemonChangeSeqLpwrPerfRestoreVfValidation_exit:
    return status;
}

/*!
 * @copydoc PerfDaemonChangeSeqScriptInit()
 * @memberof CHANGE_SEQ
 * @private
 */
static FLCN_STATUS
s_perfDaemonChangeSeqLpwrPerfRestoreBuild
(
    CHANGE_SEQ                         *pChangeSeq,
    CHANGE_SEQ_SCRIPT                  *pScript,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLwrr,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLast
)
{
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_HEADER
                   *pHdr               = &pScript->hdr.data;
    FLCN_STATUS     status             = FLCN_OK;
    LwU8            i;
    OSTASK_OVL_DESC ovlDescListBuild[] = {
        OSTASK_OVL_DESC_DEFINE_VFE(FULL)
    };

    //
    // The order of steps built/exelwted in LPWR perf restore. Steps may be excluded
    // if the feature is disabled or is not necessary.
    //
    struct
    {
        const LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID stepId;
        const LwBool bInclude;
    } steps[] =
    {
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_PRE_CHANGE_RM,
            PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_CHANGE_RM)
        },
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_PRE_CHANGE_PMU,
            PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_CHANGE_PMU)
        },
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_35_STEP_ID_PRE_VOLT_NAFLL_CLKS,
            PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_VOLT_NAFLL_CLKS)
        },
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_35_STEP_ID_PRE_VOLT_CLKS,
            PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_VOLT_CLKS)
        },
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_VOLT,
            PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_VOLT)
        },
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_35_STEP_ID_POST_VOLT_NAFLL_CLKS,
            PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_VOLT_NAFLL_CLKS)
        },
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_35_STEP_ID_POST_VOLT_CLKS,
            PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_VOLT_CLKS)
        },
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_35_STEP_ID_CLK_MON,
            PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_CLK_MON) && clkDomainsIsClkMonEnabled()
        },
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_POST_CHANGE_PMU,
            PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_CHANGE_PMU)
        },
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_POST_CHANGE_RM,
            PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_CHANGE_RM)
        }
    };

    // Attach overlays needed only for build.
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListBuild);
    {
        // Begin counting build time.
        PERF_CHANGE_SEQ_PROFILE_BEGIN(&pHdr->profiling.totalBuildTimens);

        // Take the clock VF points semaphore
        perfReadSemaphoreTake();

        // Build the script
        for (i = 0; i < LW_ARRAY_ELEMENTS(steps); ++i)
        {
            if (steps[i].bInclude)
            {
                status = perfDaemonChangeSeqScriptBuildAndInsert(
                    pChangeSeq, pScript, pChangeLwrr, pChangeLast, steps[i].stepId);
                if (status != FLCN_OK)
                {
                    // Update debugging info.
                    g_CHSEQ_LPWR_DEBUG[6] = steps[i].stepId;
                    CHSEQ_FAILURE_EX(status);
                    goto s_perfDaemonChangeSeqLpwrPerfRestoreBuild_exit;
                }
            }
        }

        // Stop counting build time.
        PERF_CHANGE_SEQ_PROFILE_END(&pHdr->profiling.totalBuildTimens);

s_perfDaemonChangeSeqLpwrPerfRestoreBuild_exit:
        // Give the clock VF points semaphore
        perfReadSemaphoreGive();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListBuild);

    return status;
}
