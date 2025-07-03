/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    cmdmgmt_queue_fb.c
 * @copydoc cmdmgmt_queue_fb.h
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "pmu_objpmu.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "cmdmgmt/cmdmgmt.h"
#include "cmdmgmt/cmdmgmt_queue_fb.h"
#include "cmdmgmt/cmdmgmt_queue_fb_heap.h"
#include "cmdmgmt/cmdmgmt_queue_fb_ptcb.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Compile Time Checks ---------------------------- */
// Assure that this file compiles only when required.
ct_assert(PMUCFG_FEATURE_ENABLED(PMU_FBQ));

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc queuefbQueuesInit
 */
void
queuefbQueuesInit(void)
{
    // Configure HEAD/TAIL pointers for the RM => PMU high priority queue.
    pmuQueueRmToPmuInit_HAL(&Pmu, RM_PMU_COMMAND_QUEUE_HPQ, 0U);

    if (!PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_PER_TASK_COMMAND_BUFFER))
    {
        // Configure HEAD/TAIL pointers for the RM => PMU low priority queue.
        pmuQueueRmToPmuInit_HAL(&Pmu, RM_PMU_COMMAND_QUEUE_LPQ, 0U);
    }

    // Configure HEAD/TAIL pointers for the PMU => RM queue.
    pmuQueuePmuToRmInit_HAL(&Pmu, 0U);

    // Setup the message queue descriptor.
    OsCmdmgmtQueueDescriptorRM.start    = 0U;
    OsCmdmgmtQueueDescriptorRM.end      = RM_PMU_FBQ_MSG_NUM_ELEMENTS;
    OsCmdmgmtQueueDescriptorRM.headGet  = pmuQueuePmuToRmHeadGet_HAL_FNPTR();
    OsCmdmgmtQueueDescriptorRM.headSet  = pmuQueuePmuToRmHeadSet_HAL_FNPTR();
    OsCmdmgmtQueueDescriptorRM.tailGet  = pmuQueuePmuToRmTailGet_HAL_FNPTR();
    OsCmdmgmtQueueDescriptorRM.dataPost =
        PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_PER_TASK_COMMAND_BUFFER) ?
        &queueFbPtcbQueueDataPost : &queueFbHeapQueueDataPost;
}

/*!
 * @copydoc queueFbChecksum16
 */
LwU16
queueFbChecksum16
(
    const void *pBuffer,
    LwU32       size
)
{
    LwU16   checksum = 0U;
    LwU32   i;

    // Size is in bytes, so divide by 2
    for (i = 0U; i < (size / 2U); i++)
    {
        checksum += ((const LwU16 *)pBuffer)[i];
    }

    return checksum;
}

/* ------------------------- Private Functions ------------------------------ */
