/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CMDMGMT_QUEUE_DMEM_H
#define CMDMGMT_QUEUE_DMEM_H

/*!
 * @file    cmdmgmt_queue_dmem.h
 * @brief   Interfaces implementing RM <-> PMU RPC DMEM queue functionality.
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
 * @brief   Initialize head/tail pointers of queues between the RM and the PMU.
 *          Additionally setup the PMU->RM queue descriptor.
 */
void queueDmemQueuesInit(void)
    GCC_ATTRIB_SECTION("imem_cmdmgmtMisc", "queueDmemQueuesInit");

/*!
 * @brief   Populate the INIT RPC with DMEM queue specific information.
 */
void queueDmemInitRpcPopulate(PMU_RM_RPC_STRUCT_CMDMGMT_INIT *pRpc)
    GCC_ATTRIB_SECTION("imem_cmdmgmtMisc", "queueDmemInitRpcPopulate");

/*!
 * @brief   Process requests from a single DMEM queue.
 *
 * @param[in]   headId  Index of the queue to process.
 *
 * @return  Error propagated from inner functions.
 */
FLCN_STATUS queueDmemProcessCmdQueue(LwU32 headId)
    GCC_ATTRIB_SECTION("imem_cmdmgmt", "queueDmemProcessCmdQueue");

#endif // CMDMGMT_QUEUE_DMEM_H
