/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef RM_PROXY_H
#define RM_PROXY_H
#include <lwtypes.h>
#include <flcnifcmn.h>
#include <riscvifriscv.h>
#include <gsp/gspifgsp.h>
#include <gsp/gspifrmproxy.h>
#include <liblwriscv/mpu.h>

#include "rpc.h"
#include "cmdmgmt.h"
#include "cc_rmproxy.h"

#define PRE_LOCKDOWN_TIMEOUT_MS     500

/* Set to handle pri read/write commands */
#define RM_PROXY_ENABLE_PROXY_COMMANDS        0

void dispatchAcrCommand(RM_FLCN_CMD_GSP *pRequest);
void dispatchSpdmCommand(RM_FLCN_CMD_GSP *pRequest);

#if RM_PROXY_ENABLE_PROXY_COMMANDS
void dispatchRmProxyCommand(RM_FLCN_CMD_GSP *pRequest);
#endif // RM_PROXY_ENABLE_PRI_COMMANDS

#if !CC_IS_RESIDENT
void rpcInit(CONF_COMPUTE_PARTITION_REQUEST *pRequest,
             CONF_COMPUTE_PARTITION_REPLY *pReply);
#endif // CC_IS_RESIDENT

/*
 * Wait until it's safe to lockdown the core
 */
LwBool preLockdownSync(LwU64 timeout_ms);

LwU64 proxyPartitionSwitch(LwU64 arg0, LwU64 arg1, LwU64 arg2, LwU64 arg3,
                           LwU64 partition_origin, LwU64 partition);

#endif // RM_PROXY_H
