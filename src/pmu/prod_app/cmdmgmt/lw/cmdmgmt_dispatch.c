/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    cmdmgmt_dispatch.c
 * @brief   Contains logic for dispatching RM commands to PMU tasks.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "lwoswatchdog.h"
#include "lwostask.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "cmdmgmt/cmdmgmt.h"
#include "cmdmgmt/cmdmgmt_dispatch.h"
#include "cmdmgmt/cmdmgmt_detach.h"
#if (PMUCFG_FEATURE_ENABLED(PMUTASK_ACR))
#include "acr/pmu_objacr.h"
#endif
#include "os/pmu_lwos_task.h"
#include "pmu_objdisp.h"
#include "pmu_objpmu.h"
#include "pmu_oslayer.h"
#include "task_therm.h"

#include "g_pmurpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * @brief   Represents an invalid entry in the @ref CmdmgmtDispatchTable
 */
#define CMDMGMT_DISPATCH_ILWALID 0U

//
// The below CmdmgmtDispatchTable will default initialize the destination queue
// for unspecified UNITs to 0, so we want to treat "0" as "INVALID." Ensure that
// 0 indicates the CmdMgmt queue, which we should never have to dispatch to,
// letting us "re-use it" as INVALID.
//
ct_assert((CMDMGMT_DISPATCH_ILWALID == 0U) && (LWOS_QUEUE_ID(PMU, CMDMGMT) == 0U));

/*!
 * @brief   Create a @ref CmdmgmtDispatchTable entry guarded by task enablement.
 *
 * @details If the given task is enabled, the macro maps to the task's queue ID,
 *          and if not, it marks the entry invalid.
 *
 * @param[in]   task    The "base" task name
 *
 * @return  PMU_QUEUE_ID_##task         If the task is enabled
 * @return  CMDMGMT_DISPATCH_ILWALID    If the task is not enabled
 */
#define CMDMGMT_DISPATCH_TASK_CASE(task) \
    (OSTASK_ENABLED(task) ? \
        LWOS_QUEUE_ID(PMU, task) : CMDMGMT_DISPATCH_ILWALID)

/*!
 * @brief   Create a @ref CmdmgmtDispatchTable entry guarded by feature and task
 *          enablement
 *
 * @details If the given feature and task are enabled, the macro maps to the
 *          task's queue ID and if not, it marks the entry invalid.
 *
 * @param[in]   feature PMU feature for which to check
 * @param[in]   task    The "base" task name
 *
 * @return  PMU_QUEUE_ID_##task         If the feature and task are enabled
 * @return  CMDMGMT_DISPATCH_ILWALID    If either feature or task is not enabled
 */
#define CMDMGMT_DISPATCH_FEATURE_CASE(feature, task) \
    PMUCFG_FEATURE_ENABLED(feature) ? \
        CMDMGMT_DISPATCH_TASK_CASE(task) : CMDMGMT_DISPATCH_ILWALID

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/*!
 * @brief Union of all possible RPC subsurface data that can be handled by the
 *        CMDMGMT task.
 */
typedef union
{
    /*!
     * @brief Default size, as defined by configuration
     */
    LwU8 defaultSize[OSTASK_ATTRIBUTE_CMDMGMT_CMD_SCRATCH_SIZE];
} TASK_SURFACE_UNION_CMDMGMT;

/*!
 * @brief   Defines the command buffer for the CMDMGMT task.
 */
PMU_TASK_CMD_BUFFER_DEFINE_SCRATCH_SIZE(CMDMGMT, "dmem_cmdmgmtCmdBuffer");

/*!
 * @brief   Creates mappings from an RM_PMU_UNIT_* ID to the appropriate
 *          destination queue PMU_QUEUE_ID_*
 *
 * @details If a task (and/or feature) is enabled on a particular profile, the
 *          table will indicate the appropriate destination queue. Otherwise, it will
 *          specify @ref CMDMGMT_DISPATCH_ILWALID.
 */
static LwU8
CmdmgmtDispatchTable[RM_PMU_UNIT_END] =
{
    [RM_PMU_UNIT_ACR]                       = CMDMGMT_DISPATCH_TASK_CASE(ACR),
    [RM_PMU_UNIT_SEQ]                       = CMDMGMT_DISPATCH_TASK_CASE(SEQ),
    [RM_PMU_UNIT_I2C]                       = CMDMGMT_DISPATCH_TASK_CASE(I2C),
    [RM_PMU_UNIT_SPI]                       = CMDMGMT_DISPATCH_TASK_CASE(SPI),
    [RM_PMU_UNIT_LPWR]                      = CMDMGMT_DISPATCH_TASK_CASE(LPWR),
    [RM_PMU_UNIT_LPWR_LOADIN]               = CMDMGMT_DISPATCH_TASK_CASE(LPWR),
    [RM_PMU_UNIT_LPWR_LP]                   = CMDMGMT_DISPATCH_TASK_CASE(LPWR_LP),
    [RM_PMU_UNIT_GCX]                       = CMDMGMT_DISPATCH_TASK_CASE(GCX),
    [RM_PMU_UNIT_PERFMON]                   = CMDMGMT_DISPATCH_TASK_CASE(PERFMON),
    [RM_PMU_UNIT_DISP]                      = CMDMGMT_DISPATCH_TASK_CASE(DISP),
    [RM_PMU_UNIT_THERM]                     = CMDMGMT_DISPATCH_TASK_CASE(THERM),
    [RM_PMU_UNIT_SMBPBI]                    = CMDMGMT_DISPATCH_FEATURE_CASE(PMU_SMBPBI, THERM),
    [RM_PMU_UNIT_FAN]                       = CMDMGMT_DISPATCH_FEATURE_CASE(PMU_THERM_FAN, THERM),
    [RM_PMU_UNIT_HDCP]                      = CMDMGMT_DISPATCH_TASK_CASE(HDCP),
    [RM_PMU_UNIT_PERF]                      = CMDMGMT_DISPATCH_TASK_CASE(PERF),
    [RM_PMU_UNIT_CLK]                       = CMDMGMT_DISPATCH_FEATURE_CASE(PMU_CLK_IN_PERF, PERF),
    [RM_PMU_UNIT_CLK_FREQ]                  = CMDMGMT_DISPATCH_FEATURE_CASE(PMU_CLK_IN_PERF, PERF),
    [RM_PMU_UNIT_VOLT]                      = CMDMGMT_DISPATCH_FEATURE_CASE(PMU_VOLT_IN_PERF, PERF),
    [RM_PMU_UNIT_PMGR]                      = CMDMGMT_DISPATCH_TASK_CASE(PMGR),
    [RM_PMU_UNIT_LOGGER]                    = OSTASK_ENABLED(PERF_CF) ?
                                                LWOS_QUEUE_ID(PMU, PERF_CF) : CMDMGMT_DISPATCH_TASK_CASE(PERFMON),
    [RM_PMU_UNIT_BIF]                       = CMDMGMT_DISPATCH_TASK_CASE(BIF),
    [RM_PMU_UNIT_PERF_CF]                   = CMDMGMT_DISPATCH_TASK_CASE(PERF_CF),
    [RM_PMU_UNIT_PERF_DAEMON_COMPLETION]    = OSTASK_ENABLED(PERF_DAEMON) ?
                                                LWOS_QUEUE_ID(PMU, PERF_DAEMON_RESPONSE) : CMDMGMT_DISPATCH_ILWALID,
    [RM_PMU_UNIT_NNE]                       = OSTASK_ENABLED(NNE) ?
                                                CMDMGMT_DISPATCH_TASK_CASE(NNE) : CMDMGMT_DISPATCH_ILWALID,
};

/* ------------------------- Prototypes ------------------------------------- */
/*!
 * @brief   Handles special processing required for commands sent to
 *          RM_PMU_UNIT_CMDMGMT.
 *
 * @details CmdMgmt is the task that is itself lwrrently processing the command,
 *          so it has to execute the command "inline."
 *
 * @param[in]   pRequest    Dispatch structure including pointer to command
 *
 * @return FLCN_OK                              Command was processed successfully
 * @return FLCN_ERR_QUEUE_TASK_ILWALID_CMD_TYPE Command was not an RPC
 * @return Other                                Errors propogated by RPCs themselves
 */
static FLCN_STATUS s_cmdmgmtDispatchProcessCmdmgmt(DISP2UNIT_RM_RPC *pRequest);

/*!
 * @brief   Sends a command to a queue based on the destination unit ID
 *
 * @param[in]   pRequest    Dispatch structure including pointer to command
 *
 * @return  FLCN_OK                             Command was sent to queue successfully
 * @return  FLCN_ERR_QUEUE_TASK_ILWALID_UNIT_ID Given unit ID cannot process commands on this profile
 * @return  Other                               Errors propogated by callees
 */
static FLCN_STATUS s_cmdmgmtDispatchToQueue(DISP2UNIT_RM_RPC *pRequest);


/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Dispatch given external command to the handling queue/task.
 *
 * @details Command will be dispatched by writing its pointer into the respective
 *          queue. The scheduler will ensure that the respective task will be woken
 *          to process it.
 *
 * @param[in]    pRequest   Ext. request to be dispatched to the handling task.
 *
 * @return  FLCN_OK                     On successful exelwtion.
 * @return  FLCN_ERR_ILWALID_ARGUMENT   On handling invalid RPC DMEM pointer
 * @return  Other                       Errors propogated by callees
 */
FLCN_STATUS
cmdmgmtExtCmdDispatch_IMPL
(
    DISP2UNIT_RM_RPC *pRequest
)
{
    FLCN_STATUS status;

#ifdef FB_QUEUES
    // Sanity check -- applicable to all RPC commands
    if (!PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_PER_TASK_COMMAND_BUFFER))
    {
        LwUPtr rpcOffset = LW_PTR_TO_LWUPTR(cmdmgmtOffsetToPtr(pRequest->pRpc->cmd.rpc.rpcDmemPtr));
        if (rpcOffset !=
            (LW_PTR_TO_LWUPTR(pRequest->pRpc) + RM_FLCN_QUEUE_HDR_SIZE + sizeof(RM_PMU_RPC_CMD)))
        {
            return FLCN_ERR_ILWALID_ARGUMENT;
        }
    }
#endif // FB_QUEUES

    if (pRequest->hdr.unitId == RM_PMU_UNIT_CMDMGMT)
    {
        status = s_cmdmgmtDispatchProcessCmdmgmt(pRequest);
    }
#if 0 // defined(INSTRUMENTATION) || defined(USTREAMER)
    else if (pRequest->hdr.unitId == RM_PMU_UNIT_INSTRUMENTATION)
    {
        status = s_cmdmgmtDispatchProcessCmdmgmt(pRequest);
    }
#endif // defined(INSTRUMENTATION) || defined(USTREAMER)
    else
    {
        status = s_cmdmgmtDispatchToQueue(pRequest);
    }

    return status;
}

/* ------------------------- Private Functions ------------------------------ */
static FLCN_STATUS
s_cmdmgmtDispatchProcessCmdmgmt
(
    DISP2UNIT_RM_RPC *pRequest
)
{
    FLCN_STATUS status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_RPC))
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_PER_TASK_COMMAND_BUFFER))
        {
            const LWOS_TASK_CMD_BUFFER_PTRS *const pCmdBufPtrs =
                PMU_TASK_CMD_BUFFER_PTRS_GET(CMDMGMT);

            PMU_HALT_OK_OR_GOTO(status,
                (pCmdBufPtrs != LWOS_TASK_CMD_BUFFER_ILWALID) ?
                    FLCN_OK : FLCN_ERR_ILWALID_STATE,
                s_cmdmgmtDispatchProcessCmdmgmt_exit);

            PMU_HALT_OK_OR_GOTO(status,
                lwosTaskCmdCopyIn(
                    pRequest,
                    pCmdBufPtrs->pFbqBuffer,
                    pCmdBufPtrs->fbqBufferSize,
                    pCmdBufPtrs->pScratchBuffer,
                    pCmdBufPtrs->scratchBufferSize),
                s_cmdmgmtDispatchProcessCmdmgmt_exit);
        }

        {
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, cmdmgmtRpc)
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                if (pRequest->hdr.unitId == RM_PMU_UNIT_CMDMGMT)
                {
                    status = pmuRpcProcessUnitCmdmgmt(pRequest);
                }
#if 0 // defined(INSTRUMENTATION) || defined(USTREAMER)
                else if (pRequest->hdr.unitId == RM_PMU_UNIT_INSTRUMENTATION)
                {
                    status = pmuRpcProcessUnitInstrumentation(pRequest);
                }
#endif // defined(INSTRUMENTATION) || defined(USTREAMER)
                else
                {
                    status = FLCN_ERR_ILWALID_STATE;
                }

#if (PMUCFG_FEATURE_ENABLED(PMU_DETACH))                
                //
                // After DETACH is completed PMU can no longer perform
                // FB transactions (i.e. fetch code/data) and therefore
                // all required code must be preloaded (see the THERM
                // event handling), DMA suspended, and task priority
                // raised. That may cause a race condition between the
                // generated code for RPC response and other tasks that
                // might hold message queue mutex. So we process what
                // we can in RPC itself and defer problematic steps.
                //
                cmdmgmtDetachDeferredProcessing();
#endif // (PMUCFG_FEATURE_ENABLED(PMU_DETACH))                
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
        }
        // NJ-TODO: Add RPC error handling (for now succeed).
    }
    else
    {
        status = FLCN_ERR_QUEUE_MGMT_ILWALID_UNIT_ID;
    }

s_cmdmgmtDispatchProcessCmdmgmt_exit:
    return status;
}

static FLCN_STATUS
s_cmdmgmtDispatchToQueue
(
    DISP2UNIT_RM_RPC *pRequest
)
{
    const LwU8 unitId = pRequest->hdr.unitId;

    if ((unitId >= LW_ARRAY_ELEMENTS(CmdmgmtDispatchTable)) ||
        (CmdmgmtDispatchTable[unitId] == CMDMGMT_DISPATCH_ILWALID))
    {
        return FLCN_ERR_QUEUE_MGMT_ILWALID_UNIT_ID;
    }

    // Dispatch command to the respective queue.
    (void)lwrtosQueueIdSendBlocking(CmdmgmtDispatchTable[unitId],
                                    pRequest, sizeof(DISP2UNIT_RM_RPC));

    return FLCN_OK;
}
