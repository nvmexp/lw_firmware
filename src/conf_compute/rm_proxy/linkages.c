/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <stdint.h>

#include <libc/string.h>
#include <liblwriscv/print.h>
#include <liblwriscv/io.h>

#include "rm_proxy.h"
#include "rpc.h"

static CONF_COMPUTE_PARTITION_REQUEST *pRpcRequest;
static CONF_COMPUTE_PARTITION_REPLY *pRpcReply;

CONF_COMPUTE_PARTITION_REQUEST *rpcGetRequest(void)
{
    return pRpcRequest;
}

CONF_COMPUTE_PARTITION_REPLY *rpcGetReply(void)
{
    return pRpcReply;
}

void rpcInit(CONF_COMPUTE_PARTITION_REQUEST *pRequest, CONF_COMPUTE_PARTITION_REPLY *pReply)
{
    pRpcRequest = pRequest;
    pRpcReply   = pReply;
}
