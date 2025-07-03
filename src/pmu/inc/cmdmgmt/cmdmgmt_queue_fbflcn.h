/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CMDMGMT_QUEUE_FBFLCN_H
#define CMDMGMT_QUEUE_FBFLCN_H

/*!
 * @file    cmdmgmt_queue_fbflcn.h
 * @brief   Interfaces implementing FBFLCN <-> PMU msgbox queue functionality.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "unit_api.h"

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/*!
 * @brief   Process request from a FBFLCN queue.
 *
 * @param[in]   headId  Index of the queue to process.
 *
 * @return  Error propagated from inner functions.
 */
FLCN_STATUS queueFbflcnProcessCmdQueue(LwU32 headId)
    GCC_ATTRIB_SECTION("imem_cmdmgmt", "queueFbflcnProcessCmdQueue");

#endif // CMDMGMT_QUEUE_FBFLCN_H
