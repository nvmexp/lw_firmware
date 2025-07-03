/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "rpc.h"

static CONF_COMPUTE_PARTITION_REQUEST request;
static CONF_COMPUTE_PARTITION_REPLY reply;

CONF_COMPUTE_PARTITION_REQUEST *rpcGetRequest(void)
{
    return &request;
}

CONF_COMPUTE_PARTITION_REPLY *rpcGetReply(void)
{
    return &reply;
}
