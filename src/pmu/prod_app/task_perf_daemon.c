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
 * @file   task_perf_daemon.c
 * @brief  PMU PERF DAEMON Task
 *
 * <more detailed description to follow>
 */
/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "main_init_api.h"
#include "cmdmgmt/cmdmgmt.h"
#include "os/pmu_lwos_task.h"
#include "pmu_objpmu.h"
#include "pmu_objperf.h"
#include "pmu_objclk.h"
#include "pmu_objpg.h"
#include "pmu_objgr.h"
#include "pmu_objlpwr.h"
#include "task_perf_daemon.h"
#include "perf/perf_daemon.h"
#include "perf/perf_daemon_fbflcn.h"
#include "clk/pmu_clkfbflcn.h"

#include "g_pmurpc.h"
#include "g_pmurmifrpc.h"

/* ------------------------- Static Function Prototypes  -------------------- */
static FLCN_STATUS s_perfDaemonEventHandle(DISPATCH_PERF_DAEMON *pRequest);
static void s_perfDaemonInitFbflcnQueue(void)
    GCC_ATTRIB_SECTION("imem_init", "s_perfDaemonInitFbflcnQueue");

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Defines ---------------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
lwrtosTASK_FUNCTION(task_perf_daemon, pvParameters);

/* ------------------------- Static Variables ------------------------------- */
/*!
 * @brief Union of all possible RPC subsurface data that can be handled by the
 *        PERF_DAEMON task.
 */
typedef union
{
    /*!
     * @brief Default size, as defined by configuration
     */
    LwU8 defaultSize[OSTASK_ATTRIBUTE_PERF_DAEMON_CMD_SCRATCH_SIZE];
} TASK_SURFACE_UNION_PERF_DAEMON;

/*!
 * @brief   Defines the command buffer for the PERF_DAEMON task.
 */
PMU_TASK_CMD_BUFFER_DEFINE_SCRATCH_SIZE(PERF_DAEMON, "dmem_perfDaemonCmdBuffer");

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief      Initialize the Perf Daemon Task.
 *
 * @return     FLCN_OK on success
 * @return     descriptive error code otherwise.
 */
FLCN_STATUS
perfDaemonPreInitTask_IMPL(void)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       queueSize = 2U;  // PP-TODO: Need to analyze why we need two.

    //
    // 16 bins of 2^15[ns] == ~32.8[us] ending at ~524[us]
    // 16 bins of 2^18[ns] == ~262[us] last bin starting at ~4.2[ms]
    //
    OSTASK_CREATE(status, PMU, PERF_DAEMON);
    if (status != FLCN_OK)
    {
        goto perfDaemonPreInitTask_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_IN_PERF_DAEMON_PASS_THROUGH_PERF))
    {
        queueSize++;    // CLK 3.0
    }

    status = lwrtosQueueCreateOvlRes(&LWOS_QUEUE(PMU, PERF_DAEMON),
                                     queueSize, sizeof(DISPATCH_PERF_DAEMON));
    if (status != FLCN_OK)
    {
        goto perfDaemonPreInitTask_exit;
    }

    //
    // Completion queue will be used by perf daemon task to wait for
    // completion of the offloaded work. At any point of time, perf
    // daemon task can only wait for one completion signal.
    //
    status = lwrtosQueueCreateOvlRes(&LWOS_QUEUE(PMU, PERF_DAEMON_RESPONSE),
                                     1, sizeof(DISPATCH_PERF_DAEMON_COMPLETION));
    if (status != FLCN_OK)
    {
        goto perfDaemonPreInitTask_exit;
    }

    // Place holder for initializing PERF DAEMON object
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_FBFLCN_SUPPORTED))
    {
        s_perfDaemonInitFbflcnQueue();
    }

perfDaemonPreInitTask_exit:
    return status;
}

/*!
 * Entry point for the PERF DAEMON task.
 * This task does not receive any parameters.
 */
lwrtosTASK_FUNCTION(task_perf_daemon, pvParameters)
{
    DISPATCH_PERF_DAEMON dispatch;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfVf)
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perf)
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_ELW_PSTATES_3X)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBoardObj)
#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_ELW_PSTATES_3X)
        };

    // Permanently attach necessary overlays to TASK. NJ-TODO: Move to pre-init.
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);

    LWOS_TASK_LOOP_NO_CALLBACKS(LWOS_QUEUE(PMU, PERF_DAEMON), &dispatch, status, PMU_TASK_CMD_BUFFER_PTRS_GET(PERF_DAEMON))
    {
        status = s_perfDaemonEventHandle(&dispatch);
    }
    LWOS_TASK_LOOP_END(status);
}

/*!
 * Helper interface to wait for completion signal. Perf daemon task will block
 * on queue @ref PMU_QUEUE_ID_PERF_DAEMON_RESPONSE till it receives completion signal.
 *
 * @note  Timeout error will be consider a fatal failure.
 */
FLCN_STATUS
perfDaemonWaitForCompletion_IMPL
(
    DISPATCH_PERF_DAEMON_COMPLETION *pRequest,
    LwU8                             eventType,
    LwUPtr                           timeout
)
{
    if (FLCN_OK != lwrtosQueueReceive(LWOS_QUEUE(PMU, PERF_DAEMON_RESPONSE), pRequest,
                                      sizeof(*pRequest), timeout))
    {
        //
        // Client must implement the proper error handling.
        // Note that client must always treat time out as fatal error and as
        // follow up they should either recall this interface with new timeout
        // or execute PMU HALT.
        //
        return FLCN_ERR_TIMEOUT;
    }

    // Sanity check the completion signal.
    if (pRequest->rpc.hdr.eventType != eventType)
    {
        PMU_HALT();
        return FLCN_ERR_ILWALID_STATE;
    }

    return FLCN_OK;
}

FLCN_STATUS
perfDaemonWaitForCompletionFromRM_IMPL
(
    DISPATCH_PERF_DAEMON_COMPLETION *pRequest,
    RM_FLCN_MSG_PMU                 *pMsg,
    LwU8                             eventType,
    LwUPtr                           timeout
)
{
    // Let client handle the timeout error.
    return perfDaemonWaitForCompletion(pRequest, eventType, timeout);;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * Initialize the FB falcon communication elements.
 */
static void
s_perfDaemonInitFbflcnQueue(void)
{
    //
    // Initialize mutex allowing PERF_DAEMON task to wait for
    // the MCLK switch completion.
    //
    lwrtosSemaphoreCreateBinaryTaken(&FbflcnRpcMutex, OVL_INDEX_OS_HEAP);
    PMU_HALT_COND(FbflcnRpcMutex != NULL);
}

/*!
 * @brief   Helper call handling events sent to PERF DAEMON task.
 */
static FLCN_STATUS
s_perfDaemonEventHandle
(
    DISPATCH_PERF_DAEMON *pRequest
)
{
    FLCN_STATUS status = FLCN_ERR_QUEUE_TASK_ILWALID_EVENT_TYPE;

    switch (pRequest->hdr.eventType)
    {
        case DISP2UNIT_EVT_RM_RPC:
        {
            status = FLCN_ERR_QUEUE_TASK_ILWALID_UNIT_ID;

            switch (pRequest->hdr.unitId)
            {
                case RM_PMU_UNIT_CLK_FREQ:
                {
                    status = pmuRpcProcessUnitClkFreq(&(pRequest->rpc));
                    break;
                }
                case RM_PMU_UNIT_VOLT:
                {
                    //
                    // In future perf task will pass through the volt
                    // request to perf daemon
                    //
                    status = FLCN_ERR_ILWALID_ARGUMENT;
                    break;
                }
                default:
                {
                    // unused
                    break;
                }
            }
            break;
        }
        case PERF_DAEMON_EVENT_ID_PERF_CHANGE_SEQ_SCRIPT_EXELWTE:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ))
            {
                //
                // Synchronize with LPWR task for LPWR features (GPC-RG) that are
                // mutually exclusive with VF switch.
                //
                perfDaemonChangeSeqPrepareForVfSwitch();
                {
                    // Attach overlays
                    OSTASK_OVL_DESC ovlDescList[] = {
                        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfDaemonChangeSeq)
                        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfChangeSeqClient)
                    };

                    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
                    {
                        (void)perfDaemonChangeSeqScriptExelwte(pRequest->scriptExelwte.pChangeSeq);
                    }
                    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
                }
                perfDaemonChangeSeqUnprepareForVfSwitch();

                status = FLCN_OK; // NJ-TODO: Why?
            }

            break;
        }
        case PERF_DAEMON_EVENT_ID_PERF_CHANGE_SEQ_LPWR_POST_SCRIPT_EXELWTE:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_LPWR))
            {
                // Attach overlays
                OSTASK_OVL_DESC ovlDescList[] = {
                    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfDaemonChangeSeqLpwr)
                    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfChangeSeqLpwr)
                    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, lpwr)
                };

                OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
                {
                    (void)perfDaemonChangeSeqLpwrPostScriptExelwte(
                            pRequest->lpwrPostScriptExelwte.lpwrScriptId);
                }
                OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

                status = FLCN_OK; // NJ-TODO: Why?
            }

            break;
        }
        case PERF_DAEMON_EVENT_ID_PERF_MCLK_SWITCH:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_RM_MCLK_RPC_SUPPORTED))
            {
                status = perfDaemonProcessMclkSwitchEvent(pRequest);
            }
            else
            {
                status = FLCN_ERR_NOT_SUPPORTED;
            }
            break;
        }
        default:
        {
            // unused
            break;
        }
    }

    return status;
}
