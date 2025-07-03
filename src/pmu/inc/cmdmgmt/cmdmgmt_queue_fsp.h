/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CMDMGMT_QUEUE_FSP_H
#define CMDMGMT_QUEUE_FSP_H

/*!
 * @file    cmdmgmt_queue_fsp.h
 * @brief   Interfaces implementing FSP <-> PMU RPC functionality.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "unit_api.h"

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/*!
 * @brief   Initialize FSP RPC infrastructure.
 *
 * @return  Error propagated from inner functions.
 */
FLCN_STATUS queueFspQueuesInit(void)
    GCC_ATTRIB_SECTION("imem_cmdmgmtMisc", "queueFspQueuesInit");

/*!
 * @brief   Process message posted by FSP.
 *
 * @param[in]   headId  Index of the queue to process.
 *
 * @return  Error propagated from inner functions.
 */
FLCN_STATUS queueFspProcessCmdQueue(LwU32 headId)
    GCC_ATTRIB_SECTION("imem_cmdmgmt", "queueFspProcessCmdQueue");

#endif // CMDMGMT_QUEUE_FSP_H
