/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CMDMGMT_QUEUE_FB_HEAP_H
#define CMDMGMT_QUEUE_FB_HEAP_H

/*!
 * @file    cmdmgmt_queue_fb_heap.h
 * @brief   Interfaces implementing heap RM <-> PMU RPC FB queue functionality.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "unit_api.h"

/* ------------------------- Application Includes --------------------------- */
#include "g_pmurmifrpc.h"

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/*!
 * @brief   Populate the INIT RPC with FB queue specific information.
 */
void queueFbHeapInitRpcPopulate(PMU_RM_RPC_STRUCT_CMDMGMT_INIT *pRpc)
    GCC_ATTRIB_SECTION("imem_cmdmgmtMisc", "queueFbHeapInitRpcPopulate");

/*!
 * @brief   Process requests from a single FB queue.
 *
 * @param[in]   headId  Index of the queue to process.
 *
 * @return  Error propagated from inner functions.
 */
FLCN_STATUS queueFbHeapProcessCmdQueue(LwU32 headId)
    GCC_ATTRIB_SECTION("imem_cmdmgmt", "queueFbHeapProcessCmdQueue");

void queueFbHeapQueueDataPost(LwU32 *pHead, RM_FLCN_QUEUE_HDR *pHdr, const void *pBody, LwU32 size)
    GCC_ATTRIB_SECTION("imem_resident", "queueFbHeapQueueDataPost");

void queueFbHeapRpcRespond(DISP2UNIT_RM_RPC *pRequest, RM_FLCN_QUEUE_HDR *pMsgHdr, const void *pMsgBody)
    GCC_ATTRIB_SECTION("imem_resident", "queueFbHeapRpcRespond");

#endif // CMDMGMT_QUEUE_FB_HEAP_H
