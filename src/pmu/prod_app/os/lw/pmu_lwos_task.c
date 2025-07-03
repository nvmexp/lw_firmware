/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_lwos_task.c
 * @copydoc pmu_lwos_task.h
  */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "os/pmu_lwos_task.h"
#include "lwostask.h"
#include "unit_api.h"

/* ------------------------- Application Includes --------------------------- */
#include "cmdmgmt/cmdmgmt_queue_fb_ptcb.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc lwosTaskCmdCopyIn
 */
FLCN_STATUS
lwosTaskCmdCopyIn
(
    void *pRequest,
    void *pFbqBuffer,
    LwLength fbqBufferSize,
    void *pScratchBuffer,
    LwLength scratchBufferSize
)
{
    DISP2UNIT_RM_RPC *const pCmdRequest = (DISP2UNIT_RM_RPC *)pRequest;
    DISP2UNIT_RM_RPC_FBQ_PTCB *pFbqPtcb;
    FLCN_STATUS status;

    PMU_HALT_OK_OR_GOTO(status,
        pCmdRequest != NULL ? FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
        lwosTaskCmdCopyIn_exit);

    //
    // If not a command request or command does not use a per-task buffer,
    // return early.
    //
    if ((pCmdRequest->hdr.eventType != DISP2UNIT_EVT_RM_RPC) ||
        !PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_PER_TASK_COMMAND_BUFFER))
    {
        status = FLCN_OK;
        goto lwosTaskCmdCopyIn_exit;
    }

    // Get the FBQ_INFO for this command and ensure it's not NULL
    PMU_HALT_OK_OR_GOTO(status,
        (pFbqPtcb = disp2unitCmdFbqPtcbGet(pCmdRequest)) != NULL ?
            FLCN_OK : FLCN_ERR_ILWALID_STATE,
        lwosTaskCmdCopyIn_exit);

    PMU_HALT_OK_OR_GOTO(status,
        queueFbPtcbRpcCopyIn(
            pFbqBuffer,
            fbqBufferSize,
            pFbqPtcb->elementIndex,
            pFbqPtcb->elementSize,
            pFbqPtcb->bSweepSkip),
        lwosTaskCmdCopyIn_exit);

    pCmdRequest->pRpc = pFbqBuffer; // pRpc is no longer type safe
    pFbqPtcb->pScratch = pScratchBuffer;
    pFbqPtcb->scratchSize = scratchBufferSize;

lwosTaskCmdCopyIn_exit:
    return status;
}
