/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "partrpc.h"

static WL_REQUEST request __attribute__((section(".rpc.request")));
static WL_REPLY reply __attribute__((section(".rpc.reply")));

WL_REQUEST *rpcGetRequest(void)
{
    return &request;
}

WL_REPLY *rpcGetReply(void)
{
    return &reply;
}
