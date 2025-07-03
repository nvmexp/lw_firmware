/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef RM_PROXY_CMDMGMT_H
#define RM_PROXY_CMDMGMT_H

#include <lwtypes.h>
#include <rmgspcmdif.h>
#include <flcnretval.h>

#define PROFILE_CMD_QUEUE_LENGTH    (0x80ul)
#define PROFILE_MSG_QUEUE_LENGTH    (0x80ul)
#define RM_DMEM_EMEM_OFFSET         0x1000000U
#define GSP_MSG_QUEUE_RM            0
#define GSP_CMD_QUEUE_RM            0

typedef struct
{
    LwU32 hdrOffset; // Offset of request in EMEM for sweeping
    RM_FLCN_CMD_GSP cmd; // TOCTOU Safe command
} RMPROXY_DISPATCHER_REQUEST;

void initCmdmgmt(void);
LwBool msgQueuePostBlocking(PRM_FLCN_QUEUE_HDR pHdr, const void *pBody);
void cmdmgmtCmdLoop(void);

/* Sweep ACKs command, so must get pointer to (unsafe) EMEM */
void cmdQueueSweep(LwU32 hdrOffset);

/* Separate copy of header for sweeping */
void cmdQueueProcess(RMPROXY_DISPATCHER_REQUEST *pCmd);

// Return true if msgqueue is empty.
LwBool msgQueueIsEmpty(void);

#endif // RM_PROXY_CMDMGMT_H
