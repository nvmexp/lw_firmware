/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <lwstatus.h>
#include <dev_gsp.h>
#include <liblwriscv/io.h>
#include <liblwriscv/print.h>
#include <liblwriscv/libc.h>
#include <rmgspcmdif.h>
#include <gsp/gspifgsp.h>
#include "rm_proxy.h"
#include "cmdmgmt.h"
#include "partitions.h"

void dispatchRmProxyCommand(RM_FLCN_CMD_GSP *pRequest)
{
    RM_FLCN_MSG_GSP reply;
    FLCN_STATUS ret = FLCN_OK;
    RM_GSP_RMPROXY_CMD *pRmProxyRequest;

    if (pRequest == NULL)
    {
        printf("dispatchRmProxyCommand: Invalid Command object\n");
        return;
    }

    pRmProxyRequest = &pRequest->cmd.rmProxy;

    memset(&reply, 0, sizeof(reply));

    reply.hdr.unitId    = RM_GSP_UNIT_RMPROXY;
    reply.hdr.ctrlFlags = 0;
    reply.hdr.seqNumId  = pRequest->hdr.seqNumId;
    reply.hdr.size      = RM_FLCN_QUEUE_HDR_SIZE + sizeof(RM_GSP_RMPROXY_MSG);

    reply.msg.rmProxy.msgType = pRmProxyRequest->cmdType;;

    // Rough checking, we don't permit accessing GSP registers and want to make
    // sure we access PRI only (assume 16MiB)
    if (((pRmProxyRequest->addr >= DRF_BASE(LW_PGSP)) && (pRmProxyRequest->addr < DRF_EXTENT(LW_PGSP))) ||
        (pRmProxyRequest->addr >= (16 * 1024 * 1024)))
    {
        ret = FLCN_ERR_ILWALID_ARGUMENT;
    } else
    {
        switch (pRmProxyRequest->cmdType)
        {
        case RM_GSP_RMPROXY_CMD_ID_REG_READ:
            reply.msg.rmProxy.value = priRead(pRmProxyRequest->addr);
            break;
        case RM_GSP_RMPROXY_CMD_ID_REG_WRITE:
            priWrite(pRmProxyRequest->addr, pRmProxyRequest->value);
            break;
        }
    }

    reply.msg.rmProxy.result = ret;

    msgQueuePostBlocking(&reply.hdr, &reply.msg);
}
