/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    task_cmdmgmt.c
 * @brief   Represents and overlay task that is responsible for initializing all
 *          queues (command and message queues) as well as dispatching all unit
 *          tasks as commands arrive in the command queues.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu/ssurface.h"

#include "lwoscmdmgmt.h"
#include "pmu_fbflcn_if.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "main_init_api.h"
#include "cmdmgmt/cmdmgmt.h"
#include "cmdmgmt/cmdmgmt_dispatch.h"
#include "cmdmgmt/cmdmgmt_queue.h"
#include "cmdmgmt/cmdmgmt_queue_dmem.h"
#include "cmdmgmt/cmdmgmt_queue_fb.h"
#include "cmdmgmt/cmdmgmt_queue_fsp.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "cmdmgmt/pbi.h"
#include "dbgprintf.h"
#include "pmu_objpmu.h"
#include "pmu_gid.h"
#include "pmu_objlsf.h"
#include "rmflcncmdif.h"
#ifndef EXCLUDE_LWOSDEBUG
#include "lwosdebug.h"
#endif

#include "g_pmurmifrpc.h"
#include "pmu_objic.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * Subsequent defines and code assume certain order of RM_PMU queue ID defines.
 */
ct_assert(RM_PMU_COMMAND_QUEUE_HPQ == 0U);
ct_assert(RM_PMU_COMMAND_QUEUE_LPQ == 1U);
ct_assert(PMU_CMD_QUEUE_IDX_FBFLCN == 2U);
ct_assert(PMU_CMD_QUEUE_FSP_RPC_MESSAGE == 3U);

/* ------------------------- Global Variables ------------------------------- */
static RM_FLCN_U64       SuperSurfaceFbOffset = {0, 0};

/* ------------------------- Under Construction Variables ------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
UPROC_STATIC void s_cmdmgmtInit(void)
    GCC_ATTRIB_SECTION("imem_cmdmgmtMisc", "s_cmdmgmtInit")
    GCC_ATTRIB_NOINLINE();
static void s_cmdmgmtTaskLoop(void)
    GCC_ATTRIB_NOINLINE();
UPROC_STATIC FLCN_STATUS s_cmdmgmtProcessEvent(DISPATCH_CMDMGMT *pDispatch);
static FLCN_STATUS s_cmdmgmtProcessSignal(DISPATCH_CMDMGMT_SIGNAL_GPIO *pSignal);
static void s_cmdmgmtInitMsgSend(FLCN_STATUS status)
    GCC_ATTRIB_SECTION("imem_cmdmgmtMisc", "s_cmdmgmtInitMsgSend")
    GCC_ATTRIB_NOINLINE();

lwrtosTASK_FUNCTION(task_cmdmgmt, pvParameters);

static void
s_cmdmgmtInitMsgSend
(
    FLCN_STATUS status
)
{
    PMU_RM_RPC_STRUCT_CMDMGMT_INIT rpc;

    //
    // RM can have more queue slots than declared in the PMU.
    // Zero out the RPC to avoid invalid queue data in the RM.
    //
    (void)memset((void *)&rpc, 0, sizeof(rpc));

    // Populate CMD/MSG queue data in the INIT RPC.
    queueInitRpcPopulate(&rpc);

    // Populate pointer to PMU debug information buffer.
#ifndef EXCLUDE_LWOSDEBUG
    rpc.osDebugEntryPoint = LW_ARCH_VAL((LwU16)(LwU32)(&OsDebugEntryPoint),
                                        RM_OS_DEBUG_ENTRY_POINT_ILWALID);
#else // EXCLUDE_LWOSDEBUG
    rpc.osDebugEntryPoint = RM_OS_DEBUG_ENTRY_POINT_ILWALID;
#endif // EXCLUDE_LWOSDEBUG

    if (PMUCFG_ENGINE_ENABLED(LSF))
    {
        // Copy BRRS data
        lsfCopyBrssData_HAL(&(Lsf.hal), &(rpc.brssData));
    }
    else
    {
        // Zero brssData. Implies that brssData.bIsValid = false
        (void)memset(&(rpc.brssData), 0, sizeof(rpc.brssData));
    }

    // NJ-TODO: Move this both up and into the critical section below.
    rpc.status = status;

    //
    // Release the MSG queue mutex before sending INIT message to RM.
    // Critical section is used to make sure no other task can grab the mutex
    // between it's release here, and where it is taken again to do the send.
    //
    appTaskCriticalEnter();
    {
        (void)lwrtosSemaphoreGive(OsCmdmgmtQueueDescriptorRM.mutex);
        // NJ-TODO: Properly handle return code.
        PMU_RM_RPC_EXELWTE_BLOCKING(status, CMDMGMT, INIT, &rpc);
    }
    appTaskCriticalExit();
}

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief      Initialize the CMDMGMT Task.
 *
 * @return     FLCN_OK on success
 * @return     descriptive error code otherwise
 */
FLCN_STATUS
cmdmgmtPreInitTask_IMPL(void)
{
    FLCN_STATUS status = FLCN_OK;

    OSTASK_CREATE(status, PMU, CMDMGMT);
    if (status != FLCN_OK)
    {
        goto cmdmgmtPreInitTask_exit;
    }

    status = lwrtosQueueCreateOvlRes(&LWOS_QUEUE(PMU, CMDMGMT), 4, sizeof(DISPATCH_CMDMGMT));
    if (status != FLCN_OK)
    {
        goto cmdmgmtPreInitTask_exit;
    }

    OSTASK_ATTACH_IMEM_OVERLAY_TO_TASK(OSTASK_TCB(CMDMGMT), OVL_INDEX_IMEM(cmdmgmtMisc));

cmdmgmtPreInitTask_exit:
    return status;
}

/*!
 * One-time init actions (TODO: consider moving GID to pre-init).
 */
UPROC_STATIC void
s_cmdmgmtInit(void)
{
    FLCN_STATUS status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_GID_SUPPORT))
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, gid)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            pmuGenerateGid();
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_SUPER_SURFACE_MEMBER_DESCRIPTORS))
    {
        // NJ-TODO: Notify RM (propagate status).
        (void)cmdmgmtSuperSurfaceMemberDescriptorsInit();
    }

    //
    // Copy the Super Surface FB Offset into DMEM, so that LwWatch can find it
    // using the PMU symbols (Falcon only).
    //
    if (FLCN_OK != ssurfaceRd((char *)&SuperSurfaceFbOffset,
            RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(hdr.data.superSurfaceFbOffset),
            sizeof(SuperSurfaceFbOffset)))
    {
        PMU_BREAKPOINT();
        // Even if this fails, we want to continue.  Only needed for LwWatch.
    }

    // NJ-TODO: These should move to pre-init time.
    if (PMUCFG_FEATURE_ENABLED(PMU_FBQ))
    {
        queuefbQueuesInit();
    }
    else
    {
        queueDmemQueuesInit();
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_FSP_RPC))
    {
        status = queueFspQueuesInit();
        if (status != FLCN_OK)
        {
            goto s_cmdmgmtInit_error;
        }
    }

    // No need to wait if we have already detected a failure.
    if (status == FLCN_OK)
    {
        status = lwosTaskInitWaitAll();
    }

s_cmdmgmtInit_error:
    // NJ-TODO: Assure that all pre-init errors are propagated here.
    s_cmdmgmtInitMsgSend(status);
}

/*!
 * Defines the entry-point for this task.  This function will execute forever
 * processing each command that arrives in the message queues and dispatching
 * unit task to handle each command.
 */
lwrtosTASK_FUNCTION(task_cmdmgmt, pvParameters)
{
    s_cmdmgmtInit();

    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(cmdmgmtMisc));

    s_cmdmgmtTaskLoop();
}

/*!
 * Non-inlined helper introduced to reduce stack usage within s_cmdmgmtInit().
 */
static void
s_cmdmgmtTaskLoop(void)
{
    DISPATCH_CMDMGMT dispatch;

    LWOS_TASK_LOOP_NO_CALLBACKS(LWOS_QUEUE(PMU, CMDMGMT), &dispatch, status, LWOS_TASK_CMD_BUFFER_ILWALID)
    {
         status = s_cmdmgmtProcessEvent(&dispatch);
    }
    LWOS_TASK_LOOP_END(status);
}

/*!
 * @brief   Processes a single event on the CMDMGMT task's queue
 *
 * @param[in]   pDispatch   Dispatch structure for event
 *
 * @return  FLCN_OK                                 Success
 * @return  FLCN_ERR_QUEUE_TASK_ILWALID_EVENT_TYPE  Unrecognized event provided
 * @return  Others                                  Errors from callees
 */
UPROC_STATIC FLCN_STATUS
s_cmdmgmtProcessEvent(DISPATCH_CMDMGMT *pDispatch)
{
    FLCN_STATUS status = FLCN_ERR_QUEUE_TASK_ILWALID_EVENT_TYPE;

    switch (pDispatch->disp2unitEvt)
    {
        case DISP2UNIT_EVT_SIGNAL:
            status = s_cmdmgmtProcessSignal(&pDispatch->signal);
            break;

        case DISP2UNIT_EVT_RM_RPC:
            status = pmuProcessQueues_HAL();
            break;

        default:
            // Nothing to do - unrecognized event
            break;
    }

    return status;
}

/*!
 * @brief   Issued when RM has completed INIT operations.
 *
 * @param[in/out]   pParams Pointer to RM_PMU_RPC_STRUCT_CMDMGMT_PMU_INIT_DONE
 */
FLCN_STATUS
pmuRpcCmdmgmtPmuInitDone
(
    RM_PMU_RPC_STRUCT_CMDMGMT_PMU_INIT_DONE *pParams
)
{
    // Add more actions here...

    // This must remain the last step of the PMU INIT sequence.
    lwosHeapAllocationsBlock();

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
/*
 * @brief   Processes a DISP2UNIT_EVT_SIGNAL event
 *
 * @param[in]   pSignal     The dispatched event data
 */
static FLCN_STATUS
s_cmdmgmtProcessSignal
(
    DISPATCH_CMDMGMT_SIGNAL_GPIO *pSignal
)
{
    FLCN_STATUS status = FLCN_ERR_ILWALID_STATE;

    if (PMUCFG_FEATURE_ENABLED(PMU_PBI_SUPPORT) &&
        (pSignal->gpioSignal == DISPATCH_CMDMGMT_SIGNAL_GPIO_PBI))
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, cmdmgmtMisc)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            status = pbiService();
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }

    return status;
}
