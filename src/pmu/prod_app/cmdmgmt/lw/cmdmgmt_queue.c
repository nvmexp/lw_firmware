/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    cmdmgmt_queue.c
 * @copydoc cmdmgmt_queue.h
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "pmu_objpmu.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "cmdmgmt/cmdmgmt_queue.h"
#include "cmdmgmt/cmdmgmt_queue_dmem.h"
#include "cmdmgmt/cmdmgmt_queue_fb_heap.h"
#include "cmdmgmt/cmdmgmt_queue_fb_ptcb.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Compile Time Checks ---------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc queueInitRpcPopulate
 */
void
queueInitRpcPopulate
(
    PMU_RM_RPC_STRUCT_CMDMGMT_INIT *pRpc
)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_FBQ))
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_PER_TASK_COMMAND_BUFFER))
        {
            queueFbPtcbInitRpcPopulate(pRpc);
        }
        else
        {
            queueFbHeapInitRpcPopulate(pRpc);
        }
    }
    else
    {
        queueDmemInitRpcPopulate(pRpc);
    }
}

/* ------------------------- Private Functions ------------------------------ */
