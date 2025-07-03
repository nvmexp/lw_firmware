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
 * @file    cmdmgmt_rpc_impl.c
 *
 * @brief   Functions implementing the RM-to-PMU and PMU-to-RM RPC functionality
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "lwoscmdmgmt.h"
#include "osptimer.h"

/* ------------------------- Application Includes --------------------------- */
#include "cmdmgmt/cmdmgmt.h"
#include "cmdmgmt/cmdmgmt_queue_fb_heap.h"
#include "cmdmgmt/cmdmgmt_queue_fb_ptcb.h"
#include "cmdmgmt/cmdmgmt_rpc_impl.h"
#include "g_pmurmifrpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
LwU8
pmuRpcAttachAndLoadImemOverlayCond(LwU8 ovlIndex)
{
    if (OSTASK_ATTACH_IMEM_OVERLAY_COND(ovlIndex))
    {
        lwrtosYIELD();
        return ovlIndex;
    }

    return OVL_INDEX_ILWALID;
}

/*!
 * @copydoc cmdmgmtRmRpcRespond
 */
void
cmdmgmtRmRpcRespond
(
    DISP2UNIT_RM_RPC *pRequest
)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_PER_TASK_COMMAND_BUFFER))
    {
        queueFbPtcbRpcRespond(pRequest);
    }
    else if (PMUCFG_FEATURE_ENABLED(PMU_FBQ))
    {
        queueFbHeapRpcRespond(pRequest, &(pRequest->pRpc->hdr), &(pRequest->pRpc->cmd));
    }
    else
    {
        cmdmgmtRmCmdRespond(pRequest,
                            &(pRequest->pRpc->hdr),
                            &(pRequest->pRpc->cmd));
    }
}

void
cmdmgmtRmCmdRespond
(
    DISP2UNIT_RM_RPC   *pRequest,
    RM_FLCN_QUEUE_HDR  *pMsgHdr,
    const void         *pMsgBody
)
{
    PMU_RM_RPC_STRUCT_CMDMGMT_RM_RPC_RESPONSE rpc;
    FLCN_STATUS status;

    memset(&rpc, 0x00, sizeof(rpc));
    rpc.seqNumId = pRequest->pRpc->hdr.seqNumId;
    rpc.rpcFlags = pRequest->pRpc->cmd.rpc.flags;
    PMU_RM_RPC_EXELWTE_BLOCKING(status, CMDMGMT, RM_RPC_RESPONSE, &rpc);

    osCmdmgmtCmdQSweep(&pRequest->pRpc->hdr, pRequest->cmdQueueId);
}

/*!
 * @brief   This function is responsible for filling in the standard parts of
 *          the RPC's header, and sending that RPC event to the RM.
 *
 * @param[in]   pHdr    Pointer to the FLCN HDR.
 * @param[in]   pRpc    Pointer to the Event RPC to send.
 * @param[in]   bBlock  Indicates if blocking is desired.
 *
 * @return  FLCN_OK if sent without error
 * @return  Errors propogated from callees otherwise.
 */
FLCN_STATUS
pmuRmRpcExelwte_IMPL
(
    RM_PMU_RPC_HEADER  *pRpc,
    LwU32               rpcSize,
    LwBool              bBlock
)
{
    FLCN_TIMESTAMP      time;
    RM_FLCN_QUEUE_HDR   hdr;
    LwBool              bRet;

    // Common stuff for Event RPCs
    hdr.unitId = pRpc->unitId;
    hdr.size   = (LwU8)(rpcSize + RM_FLCN_QUEUE_HDR_SIZE);

    osPTimerTimeNsLwrrentGet(&time);
    pRpc->time.rpcTriggered = time.parts.lo;

    // Call osCmdmgmtRmQueuePostBlocking or osCmdmgmtRmQueuePostNonBlocking
    if (bBlock)
    {
        bRet = osCmdmgmtRmQueuePostBlocking(&hdr, pRpc);
    }
    else
    {
        bRet = osCmdmgmtRmQueuePostNonBlocking(&hdr, pRpc);
    }

    return bRet ? FLCN_OK : FLCN_ERROR;
}

/* ------------------------- Private Functions ------------------------------ */
