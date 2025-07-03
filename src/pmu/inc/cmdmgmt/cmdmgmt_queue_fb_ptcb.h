/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CMDMGMT_QUEUE_FB_PTCB_H
#define CMDMGMT_QUEUE_FB_PTCB_H

/*!
 * @file    cmdmgmt_queue_fb_ptcb.h
 * @brief   Interfaces implementing PTCB RM <-> PMU RPC FB queue functionality.
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
void queueFbPtcbInitRpcPopulate(PMU_RM_RPC_STRUCT_CMDMGMT_INIT *pRpc)
    GCC_ATTRIB_SECTION("imem_cmdmgmtMisc", "queueFbPtcbInitRpcPopulate");

/*!
 * @brief   Process requests from a single PTCB FB queue.
 *
 * @param[in]   headId  Index of the queue to process.
 *
 * @return  Error propagated from inner functions.
 */
FLCN_STATUS queueFbPtcbProcessCmdQueue(LwU32 headId)
    GCC_ATTRIB_SECTION("imem_cmdmgmt", "queueFbPtcbProcessCmdQueue");

/*!
 * @brief   Copies command data for a given FbQueue at a given position in a
 *          provided buffer.
 *
 * @note    Applicable only to FB queues using PTCB (per-task command buffer).
 *
 * @param[out]  pBuffer            Buffer into which to copy FbQueue data
 * @param[in]   bufferMax          Size of pBuffer; must be @ref DMA_MAX_READ_ALIGNMENT
 *                                 aligned
 * @param[in]   queuePos           Queue position/element index from which to copy
 * @param[in]   elementSize        The size of the data in the FbQueue element
 * @param[in]   bSweepSkip         Whether the function sweep() should be skipped.
 *
 * @return  FLCN_OK                     Success
 * @return  FLCN_ERR_ILWALID_ARGUMENT   Provided arguments invalid
 * @return  Others                      Errors propagated from callees
 */
FLCN_STATUS queueFbPtcbRpcCopyIn(void *pBuffer, LwU32 bufferMax, LwU32 queuePos, LwU16 elementSize, LwBool bSweepSkip)
    GCC_ATTRIB_SECTION("imem_resident", "queueFbPtcbRpcCopyIn");

/*!
 * @implements OsCmdmgmtQueueDataPost
 */
void queueFbPtcbQueueDataPost(LwU32 *pHead, RM_FLCN_QUEUE_HDR *pHdr, const void *pBody, LwU32 size)
    GCC_ATTRIB_SECTION("imem_resident", "queueFbPtcbQueueDataPost");

/*!
 * @breif   Responds to the RM request with the updated RPC structure
 *
 * @param[in]   pRequest    Dispatch structure used for the original request
 */
void queueFbPtcbRpcRespond(DISP2UNIT_RM_RPC *pRequest)
    GCC_ATTRIB_SECTION("imem_resident", "queueFbPtcbRpcRespond");

#endif // CMDMGMT_QUEUE_FB_PTCB_H
