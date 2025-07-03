/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
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
#include "rm_proxy.h"

/* *!
 * @brief dispatchSpdmCommand
 *        Handles Commands recieved from RM and send it to SPDM partition
 *        Communicates SPDM partition returned status back to RM.
 *
 * @return FLCN_STATUS:     OK if successful else error
*/
void dispatchSpdmCommand(RM_FLCN_CMD_GSP *pRequest)
{
    FLCN_STATUS        status = FLCN_OK;
    RM_FLCN_QUEUE_HDR  hdr;
    RM_GSP_SPDM_MSG    msg;
    RM_GSP_SPDM_CMD   *pSpdmRequest;

    if (pRequest == NULL)
    {
        printf("dispatchSpdmCommand: Invalid Command object\n");
        return;
    }

    pSpdmRequest = &pRequest->cmd.spdm;

    memset(&hdr, 0, sizeof(hdr));
    memset(&msg, 0, sizeof(msg));

    hdr.unitId    = RM_GSP_UNIT_SPDM;
    hdr.seqNumId  = pRequest->hdr.seqNumId;
    hdr.ctrlFlags = 0;

    switch (pSpdmRequest->cmdType)
    {
        case RM_GSP_SPDM_CMD_ID_PROGRAM_CE_KEYS:
            {
                RM_GSP_SPDM_CMD_PROGRAM_CE_KEYS programCeKeys = pSpdmRequest->programCeKeys;
                RM_GSP_SPDM_CE_KEY_INFO         ceKeyInfo     = programCeKeys.ceKeyInfo;

                rmPrintf("dispatchSpdmCommand SPDM Command received. \n");
                rmPrintf("dispatchSpdmCommand ceIndex = %d, keyIndex = %d, ivSlotIndex = %d\n", programCeKeys.ceKeyInfo.ceIndex, programCeKeys.ceKeyInfo.keyIndex, programCeKeys.ceKeyInfo.ivSlotIndex);

                // Make partition switch request to SPDM
                status = (FLCN_STATUS)proxyPartitionSwitch(RM_GSP_CMD_TYPE(SPDM, PROGRAM_CE_KEYS), (LwU64)ceKeyInfo.ceIndex, (LwU64)ceKeyInfo.keyIndex, (LwU64)ceKeyInfo.ivSlotIndex, PARTITION_ID_RM, PARTITION_ID_SPDM);

                rmPrintf("dispatchSpdmCommand Returned after partition switch request. %x \n", status);

                msg.msgProgramCeKeys.errorCode = status;
                msg.msgType                    = RM_GSP_MSG_TYPE(SPDM, PROGRAM_CE_KEYS);
                hdr.size                       = RM_GSP_MSG_SIZE(SPDM, PROGRAM_CE_KEYS);
            }
            break;

        case RM_GSP_SPDM_CMD_ID_CC_INIT:
            {
                rmPrintf("dispatchSpdmCommand SPDM Command received (0x%x). \n", pSpdmRequest->cmdType);

                PRM_GSP_SPDM_CMD          pRpcSpdmCmd = rpcGetGspSpdmCmd();
                memcpy(pRpcSpdmCmd, pSpdmRequest, sizeof(RM_GSP_SPDM_CMD));

                // Make partition switch request to SPDM
                status = (FLCN_STATUS)proxyPartitionSwitch(RM_GSP_CMD_TYPE(SPDM, CC_INIT), 0, 0, 0, PARTITION_ID_RM, PARTITION_ID_SPDM);

                rmPrintf("dispatchSpdmCommand Returned after partition switch request. %x \n", status);

                msg.msgCcInit.errorCode = status;
                msg.msgType             = RM_GSP_MSG_TYPE(SPDM, CC_INIT);
                hdr.size                = RM_GSP_MSG_SIZE(SPDM, CC_INIT);
            }
            break;

        case RM_GSP_SPDM_CMD_ID_CC_DEINIT:
            {
                rmPrintf("dispatchSpdmCommand SPDM Command received (0x%x). \n", pSpdmRequest->cmdType);
                PRM_GSP_SPDM_CMD          pRpcSpdmCmd = rpcGetGspSpdmCmd();
                memcpy(pRpcSpdmCmd, pSpdmRequest, sizeof(RM_GSP_SPDM_CMD));

                // Make partition switch request to SPDM
                status = (FLCN_STATUS)proxyPartitionSwitch(RM_GSP_CMD_TYPE(SPDM, CC_DEINIT), 0, 0, 0, PARTITION_ID_RM, PARTITION_ID_SPDM);

                rmPrintf("dispatchSpdmCommand Returned after partition switch request. %x \n", status);

                msg.msgCcInit.errorCode = status;
                msg.msgType             = RM_GSP_MSG_TYPE(SPDM, CC_DEINIT);
                hdr.size                = RM_GSP_MSG_SIZE(SPDM, CC_DEINIT);
            }
            break;

        case RM_GSP_SPDM_CMD_ID_CC_CTRL:
            {
                rmPrintf("dispatchSpdmCommand SPDM Command received (0x%x). \n", pSpdmRequest->cmdType);
                PRM_GSP_SPDM_CMD           pRpcSpdmCmd = rpcGetGspSpdmCmd();
                memcpy(pRpcSpdmCmd, pSpdmRequest, sizeof(RM_GSP_SPDM_CMD));

                // Make partition switch request to SPDM
                status = (FLCN_STATUS)proxyPartitionSwitch(RM_GSP_CMD_TYPE(SPDM, CC_CTRL), 0, 0, 0, PARTITION_ID_RM, PARTITION_ID_SPDM);

                msg.msgCcCtrl.errorCode = status;
                msg.msgType             = RM_GSP_MSG_TYPE(SPDM, CC_CTRL);
                hdr.size                = RM_GSP_MSG_SIZE(SPDM, CC_CTRL);
            }
            break;

        default:
            {
                msg.msgProgramCeKeys.errorCode = FLCN_ERR_ILLEGAL_OPERATION;
                msg.msgType                    = RM_GSP_MSG_TYPE(SPDM, ILWALID_COMMAND);
                hdr.size                       = RM_GSP_MSG_SIZE(SPDM, PROGRAM_CE_KEYS);
                rmPrintf("dispatchSpdmCommand Invalid Command recived by SPDM module");
            }
            break;
    }

    // Post the reply.
    msgQueuePostBlocking(&hdr, &msg);
}
