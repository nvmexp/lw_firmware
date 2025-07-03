/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task_vpr.c
 * @brief  Task that performs validation services related to vpr
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"
#include "sec2_hs.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2_objvpr.h"
#include "sec2_objsec2.h"
#include "vpr/vpr_mthds.h"
#include "rmsec2cmdif.h"
#include "config/g_vpr_hal.h"
#include "config/g_sec2_hal.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/*!
 * This is the queue that may be used for dispatching work items to the task.
 * It is assumed that this queue is setup/created before this task is scheduled
 * to run. Work-items may be VPR commands.
 */
LwrtosQueueHandle VprQueue;

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static  void            _vprProcessCommand(DISP2UNIT_CMD *pDispatchedCmd)
                        GCC_ATTRIB_SECTION("imem_vpr", "_vprProcessCommand");

static  FLCN_STATUS     _vprProcessCommandHs(DISP2UNIT_CMD *pDispatchedCmd)
                        GCC_ATTRIB_NOINLINE()
                        GCC_ATTRIB_SECTION("imem_vprHs", "start");

/* ------------------------- Public Functions ------------------------------- */

/*
 * @brief Moves to HS mode of VPR APP
 *
 * Moving into HS mode, setting the errorCode in mailbox and sending ACK to vpr client
 */
void
_vprProcessCommand(DISP2UNIT_CMD *pDispatchedCmd)
{
    FLCN_STATUS       status = FLCN_OK;
    RM_FLCN_QUEUE_HDR hdr    = {0};
    RM_SEC2_VPR_MSG   msg    = {0};

    // Load , execute and detach VPR HS overlay
    OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(vprHs));
    // TODO: CONFCOMP-463: Disable libVprPolicyHs overlay in APM profile
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libVprPolicyHs));

    OS_HS_MODE_SETUP_IN_TASK(OVL_INDEX_IMEM(vprHs), NULL, 0, HASH_SAVING_DISABLE);

    // Hs entry point
    status = _vprProcessCommandHs(pDispatchedCmd);

    OS_HS_MODE_CLEANUP_IN_TASK(NULL, 0);

    OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libVprPolicyHs));
    OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(vprHs));

    // Prepare and send vpr message
    hdr.unitId    = pDispatchedCmd->pCmd->hdr.unitId;
    hdr.ctrlFlags = 0;
    hdr.seqNumId  = pDispatchedCmd->pCmd->hdr.seqNumId;
    hdr.size      = RM_SEC2_MSG_SIZE(VPR, SETUP_VPR);

    msg.msgType        = RM_SEC2_MSG_TYPE(VPR, SETUP_VPR);
    msg.vprMsg.msgType = RM_SEC2_MSG_TYPE(VPR, SETUP_VPR);
    msg.vprMsg.status  = status;

    osCmdmgmtCmdQSweep(&pDispatchedCmd->pCmd->hdr, pDispatchedCmd->cmdQueueId);
    osCmdmgmtRmQueuePostBlocking(&hdr, &msg);
}

/*
 * This is the HS entry point of VPR APP
 *
 * @return
 *      FLCN_OK if the Command is processed succesfully; otherwise, a detailed
 *      error code is returned.
 */
FLCN_STATUS
_vprProcessCommandHs(DISP2UNIT_CMD *pDispatchedCmd)
{
    //
    // Check return PC to make sure that it is not in HS
    // This must always be the first statement in HS entry function
    //
    VALIDATE_RETURN_PC_AND_HALT_IF_HS();

    FLCN_STATUS      flcnStatus    = FLCN_OK;
    RM_SEC2_VPR_CMD* pCmd          = &pDispatchedCmd->pCmd->cmd.vpr;

    OS_SEC_HS_LIB_VALIDATE(libCommonHs, HASH_SAVING_DISABLE);

    // Check if VPR is supported
    CHECK_FLCN_STATUS(vprIsSupportedHS());

#if (!SEC2CFG_FEATURE_ENABLED(SEC2TASK_APM))
    //
    // Check if previous command had failed
    // If yes, then don't allow this new command and return error
    //
    CHECK_FLCN_STATUS(vprCheckPreviousCommandStatus_HAL(&VprHal));
#endif // !SEC2CFG_FEATURE_ENABLED(SEC2TASK_APM)

    // Setup VPR region
    if (pCmd->cmdType == RM_SEC2_VPR_CMD_ID_SETUP_VPR)
    {
        flcnStatus = vprCommandProgramRegion((LwU64)pCmd->vprCmd.startAddr, (LwU64)pCmd->vprCmd.size);
    }
    else
    {
        // Unknown Command encountered. Return an error.
        flcnStatus = FLCN_ERR_ILLEGAL_OPERATION;
    }

ErrorExit:
    vprWriteStatusToFalconMailbox_HAL(&VprHal, flcnStatus);

#if (!SEC2CFG_FEATURE_ENABLED(SEC2TASK_APM))
    //
    // Block further vpr commands if current command failed
    // by setting MAX_VPR_SIZE to zero
    //
    if (flcnStatus != FLCN_OK)
    {
        FLCN_STATUS cleanupStatus = vprBlockNewRequests_HAL(&VprHal);
        if (cleanupStatus != FLCN_OK)
        {
            //
            // This halt needs to be replaced with a call to HS monitor (while staying in HS mode)
            // to destroy the HS task and never schedule it
            // Also this halt will be hit only if blocking of further vpr command fails,
            // and not on vpr command processing failure. So probability is quite low here
            //
            falc_halt();
        }
    }
#endif // !SEC2CFG_FEATURE_ENABLED(SEC2TASK_APM)

    //
    // Enable the SCP RNG, and disable big hammer lockdown before returning
    // to light secure mode
    //
    EXIT_HS();

    return flcnStatus;
}

/*!
 * Defines the entry-point for VPR task
 */
lwrtosTASK_FUNCTION(task_vpr, pvParameters)
{
    DISPATCH_VPR dispatch;

    while (LW_TRUE)
    {
        if (OS_QUEUE_WAIT_FOREVER(VprQueue, &dispatch))
        {
            _vprProcessCommand(&dispatch.cmd);
        }
    }
}

