/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef CC_RPC_H
#define CC_RPC_H

#include <lwtypes.h>
#include <lwstatus.h>
#include <gsp/gspifccrpc.h>
#include <gsp/gspifacr.h>
#include <gsp/gspifspdm.h>

CONF_COMPUTE_PARTITION_REQUEST *rpcGetRequest(void);
CONF_COMPUTE_PARTITION_REPLY *rpcGetReply(void);

// Helper functions
static inline RM_GSP_ACR_CMD *rpcGetRequestAcr(void) { return &rpcGetRequest()->acr; }
static inline RM_GSP_ACR_MSG *rpcGetReplyAcr(void)   { return &rpcGetReply()->acr; }

static inline RM_GSP_SPDM_CMD *rpcGetGspSpdmCmd(void) { return &rpcGetRequest()->spdm; }

#endif // CC_RPC_H

