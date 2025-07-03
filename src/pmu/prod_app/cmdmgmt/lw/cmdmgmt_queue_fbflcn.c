/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    cmdmgmt_queue_fbflcn.c
 * @copydoc cmdmgmt_queue_fbflcn.h
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "pmu_objpmu.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "cmdmgmt/cmdmgmt_queue_fbflcn.h"
#include "pmu_fbflcn_if.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
// NJ-TODO: Temporary until more FBFLCN code moved here.
extern LwrtosSemaphoreHandle FbflcnRpcMutex;

/* ------------------------- Compile Time Checks ---------------------------- */
// Assure that this file compiles only when required.
ct_assert(PMUCFG_FEATURE_ENABLED(PMU_CLK_FBFLCN_SUPPORTED));

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc queueFbflcnProcessCmdQueue
 */
FLCN_STATUS
queueFbflcnProcessCmdQueue
(
    LwU32 headId
)
{
    FLCN_STATUS status = FLCN_OK;

    if (headId != PMU_CMD_QUEUE_IDX_FBFLCN)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        PMU_BREAKPOINT();
        goto queueFbflcnProcessCmdQueue_exit;
    }

    // FBFLCN operates as secondary so a task must be waiting for this response.
    status = lwrtosSemaphoreGive(FbflcnRpcMutex);

queueFbflcnProcessCmdQueue_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
