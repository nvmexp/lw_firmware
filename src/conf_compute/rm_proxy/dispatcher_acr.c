/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* ------------------------- System Includes -------------------------------- */
#include <lwtypes.h>
#include <lwmisc.h>
#include <rmgspcmdif.h>
#include <liblwriscv/libc.h>
#include "rpc.h"
#include "cmdmgmt.h"
#include "partitions.h"
#include "acr_status_codes.h"
#include "rm_proxy.h"

/* *!
 * @brief dispatchAcrCommand
 *        Handles Commands recieved from RM and send it to ACR partition
 *        Communicates ACR partition returned status back to RM.
 *
 * @param[in] pRequest: GSP CMD request recieved.
 *
 * @return ACR_STATUS:  OK if successful else error
*/
void dispatchAcrCommand(RM_FLCN_CMD_GSP *pRequest)
{
    RM_FLCN_QUEUE_HDR hdr;
    RM_GSP_ACR_MSG    msg;
    RM_GSP_ACR_CMD   *pRpcAcrCmd;
    RM_GSP_ACR_MSG   *pRpcAcrMsg;
    RM_GSP_ACR_CMD   *pAcrRequest;

    if (pRequest == NULL)
    {
        printf("dispatchAcrCommand: Invalid Command object\n");
        return;
    }

    pAcrRequest   = &pRequest->cmd.acr;

    hdr.unitId    = RM_GSP_UNIT_ACR;
    hdr.seqNumId  = pRequest->hdr.seqNumId;
    hdr.ctrlFlags = 0;


    switch (pAcrRequest->cmdType)
    {
        case RM_GSP_ACR_CMD_ID_BOOT_GSP_RM:
            {
                hdr.size = RM_GSP_MSG_SIZE(ACR, BOOT_GSP_RM);
            }
            break;
        case RM_GSP_ACR_CMD_ID_LOCK_WPR:
            {
                hdr.size = RM_GSP_MSG_SIZE(ACR, LOCK_WPR);
            }
            break;
        case RM_GSP_ACR_CMD_ID_BOOTSTRAP_ENGINE:
            {
                hdr.size = RM_GSP_MSG_SIZE(ACR, BOOTSTRAP_ENGINE);
            }
            break;
        case RM_GSP_ACR_CMD_ID_UNLOCK_WPR:
            {
                hdr.size = RM_GSP_MSG_SIZE(ACR, UNLOCK_WPR);
            }
            break;
        default:
                msg.acrStatus.errorCode       = ACR_ERROR_ILWALID_OPERATION;
                msg.msgType                   = RM_GSP_MSG_TYPE(ACR, ILWALID_COMMAND);
                hdr.size                      = RM_GSP_MSG_SIZE(ACR, STATUS);
                printf("dispatchAcrCommand: Invalid Command\n");
                goto errExit;
            break;
    }

    pRpcAcrCmd = rpcGetRequestAcr();
    memcpy(pRpcAcrCmd, pAcrRequest, sizeof(RM_GSP_ACR_CMD));

    // Call RPC mechanism for ACR Partition
    proxyPartitionSwitch(0, 0, 0, 0, PARTITION_ID_RM, PARTITION_ID_ACR);

    pRpcAcrMsg = rpcGetReplyAcr();
    memcpy(&msg, pRpcAcrMsg, sizeof(RM_GSP_ACR_MSG));

errExit:
    // Post the reply.
    msgQueuePostBlocking(&hdr, &msg);
}
