/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <lwmisc.h>
#include <sections.h>
#include <shlib/partswitch.h>
#include "partrpc.h"
#include "partswitch.h"


/*
 * This file implements simple RTOS <-> partition communication protocol for
 * RTOS tasks/kernel. We can't reuse copy that is in worklaunch, because
 * we're not linking RTOS and worklaunch images together (that would be diffilwlt).
 *
 * It assumess offsets for RPC requests/responses are fixed (just like worklaunch
 * side does).
 *
 * MK TODO: It may be viable to harden it in future, by making RTOS side decide
 * on structure offset/size and passing them to worklaunch. That way we make
 * sure addresses match.
 */

sysSHARED_CODE WL_REQUEST *rpcGetRequest(void)
{
    WL_REQUEST * req = (WL_REQUEST *)(PARTSWITCH_SHARED_CARVEOUT_VA);
    return req;
}

sysSHARED_CODE WL_REPLY *rpcGetReply(void)
{
    WL_REPLY * rep = (WL_REPLY *)(PARTSWITCH_SHARED_CARVEOUT_VA + LW_ALIGN_UP(sizeof(WL_REQUEST), 0x10));
    return rep;
}
