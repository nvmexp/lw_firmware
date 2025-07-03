/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CMDMGMT_QUEUE_H
#define CMDMGMT_QUEUE_H

/*!
 * @file    cmdmgmt_queue.h
 * @brief   Interfaces shared between all queue implementations.
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
 * @brief   Populate the INIT RPC with queue specific information.
 */
void queueInitRpcPopulate(PMU_RM_RPC_STRUCT_CMDMGMT_INIT *pRpc)
    GCC_ATTRIB_SECTION("imem_cmdmgmtMisc", "queueInitRpcPopulate");

#endif // CMDMGMT_QUEUE_H
