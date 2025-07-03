/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    cmdmgmt_queue_dmem.c
 * @copydoc cmdmgmt_queue_dmem.h
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "pmu_objpmu.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "cmdmgmt/cmdmgmt.h"
#include "cmdmgmt/cmdmgmt_dispatch.h"
#include "cmdmgmt/cmdmgmt_queue_dmem.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "dev_top.h" // Part of WAR for http://lwbugs/2767551

/* ------------------------- Type Definitions ------------------------------- */
/*!
 * @brief   Assure that the RM_PMU_RPC_HEADER fulfills FBQ requirements.
 */
ct_assert((sizeof(RM_PMU_RPC_HEADER) <= DMA_MAX_READ_ALIGNMENT) &&
          ONEBITSET(sizeof(RM_PMU_RPC_HEADER)));

/* ------------------------- External Definitions --------------------------- */
/*!
 * Address defined in the linker script to mark the memory shared between PMU
 * and RM.  Specifically, this marks the starting offset for the chunk of DMEM
 * that is managed by the RM.
 */
extern LwU32    _end_rmqueue[];

/*!
 * Address defined in the linker script to mark the end of the PMU DMEM.  This
 * offset combined with @ref _end_rmqueue define the size of the RM-managed
 * portion of the DMEM.
 */
extern LwU32    _end_physical_dmem[];

/* ------------------------- Compile Time Checks ---------------------------- */
// Assure that this file compiles only when required.
ct_assert(!PMUCFG_FEATURE_ENABLED(PMU_FBQ));

/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * @brief   Defines for the legacy DMEM queue, previously defined in Profiles.pm
 */
#define QUEUE_DMEM_QUEUE_COUNT_CMD              (0x2U)
#define QUEUE_DMEM_QUEUE_LENGTH_CMD_HPQ         (0x80U)
#define QUEUE_DMEM_QUEUE_LENGTH_CMD_LPQ         (0x80U)
#if (PMU_PROFILE_GM10X)
#define QUEUE_DMEM_QUEUE_LENGTH_MESSAGE         (0x80U)
#else // (PMU_PROFILE_GM10X)
#define QUEUE_DMEM_QUEUE_LENGTH_MESSAGE         (0x100U)
#endif // (PMU_PROFILE_GM10X)

/*!
 * Callwlate (static) individual queue offsets based on profile information.
 */
#define QUEUE_DMEM_QUEUE_OFFSET_CMD_HPQ    0U
#define QUEUE_DMEM_QUEUE_OFFSET_CMD_LPQ \
    (QUEUE_DMEM_QUEUE_OFFSET_CMD_HPQ  + (QUEUE_DMEM_QUEUE_LENGTH_CMD_HPQ  / sizeof(LwU32)))
#define QUEUE_DMEM_QUEUE_OFFSET_MESSAGE \
    (QUEUE_DMEM_QUEUE_OFFSET_CMD_LPQ  + (QUEUE_DMEM_QUEUE_LENGTH_CMD_LPQ  / sizeof(LwU32)))
#define QUEUE_DMEM_QUEUE_BUFFER_SIZE \
    (QUEUE_DMEM_QUEUE_OFFSET_MESSAGE  + (QUEUE_DMEM_QUEUE_LENGTH_MESSAGE  / sizeof(LwU32)))

/* ------------------------- Global Variables ------------------------------- */
/*!
 * Allocates a DMEM space oclwpied by all PMU queues (CMD + MSG).
 */
LwU32 QueuesExtOrig[QUEUE_DMEM_QUEUE_BUFFER_SIZE]
    GCC_ATTRIB_SECTION("rmrtos", "QueuesExtOrig");

/*!
 * Statically initialized start DMEM offsets of all queues.
 */
LwU32 QueueDmemQueuePtrStart[RM_PMU_QUEUE_COUNT] =
{
    (LwU32)&QueuesExtOrig[QUEUE_DMEM_QUEUE_OFFSET_CMD_HPQ],
    (LwU32)&QueuesExtOrig[QUEUE_DMEM_QUEUE_OFFSET_CMD_LPQ],
    (LwU32)&QueuesExtOrig[QUEUE_DMEM_QUEUE_OFFSET_MESSAGE],
};

/*!
 * Statically initialized current DMEM offsets of all command queues.
 * Current offset -> locations where the next command will be dispatched from.
 */
UPROC_STATIC LwU32 QueueDmemQueuePtrLwrrent[QUEUE_DMEM_QUEUE_COUNT_CMD] =
{
    (LwU32)&QueuesExtOrig[QUEUE_DMEM_QUEUE_OFFSET_CMD_HPQ],
    (LwU32)&QueuesExtOrig[QUEUE_DMEM_QUEUE_OFFSET_CMD_LPQ],
};

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc queueDmemQueuesInit
 */
void
queueDmemQueuesInit(void)
{
    // Clear the memory oclwpied by the RM<->PMU queues (CMD + MSG).
    (void)memset(QueuesExtOrig, 0, sizeof(QueuesExtOrig));

    // Configure HEAD/TAIL pointers for the RM => PMU high priority queue.
    pmuQueueRmToPmuInit_HAL(&Pmu, RM_PMU_COMMAND_QUEUE_HPQ,
                            QueueDmemQueuePtrStart[RM_PMU_COMMAND_QUEUE_HPQ]);

    // Configure HEAD/TAIL pointers for the RM => PMU low priority queue.
    pmuQueueRmToPmuInit_HAL(&Pmu, RM_PMU_COMMAND_QUEUE_LPQ,
                            QueueDmemQueuePtrStart[RM_PMU_COMMAND_QUEUE_LPQ]);

    // Setup the message queue head and tail ptrs.
    pmuQueuePmuToRmInit_HAL(&Pmu, QueueDmemQueuePtrStart[RM_PMU_MESSAGE_QUEUE]);

    // Setup the message queue descriptor.
    OsCmdmgmtQueueDescriptorRM.start =
        QueueDmemQueuePtrStart[RM_PMU_MESSAGE_QUEUE];
    OsCmdmgmtQueueDescriptorRM.end =
        OsCmdmgmtQueueDescriptorRM.start + QUEUE_DMEM_QUEUE_LENGTH_MESSAGE;
    OsCmdmgmtQueueDescriptorRM.headGet  = pmuQueuePmuToRmHeadGet_HAL_FNPTR();
    OsCmdmgmtQueueDescriptorRM.headSet  = pmuQueuePmuToRmHeadSet_HAL_FNPTR();
    OsCmdmgmtQueueDescriptorRM.tailGet  = pmuQueuePmuToRmTailGet_HAL_FNPTR();
    OsCmdmgmtQueueDescriptorRM.dataPost = &osCmdmgmtQueueWrite_DMEM;
}

/*!
 * @copydoc queueDmemInitRpcPopulate
 */
void
queueDmemInitRpcPopulate
(
    PMU_RM_RPC_STRUCT_CMDMGMT_INIT *pRpc
)
{
    LwU16 offset;

    pRpc->bufferStart = (LwU32)QueuesExtOrig;

    pRpc->queueSize[RM_PMU_COMMAND_QUEUE_HPQ] = (LwU16)QUEUE_DMEM_QUEUE_LENGTH_CMD_HPQ;
    pRpc->queueSize[RM_PMU_COMMAND_QUEUE_LPQ] = (LwU16)QUEUE_DMEM_QUEUE_LENGTH_CMD_LPQ;
    pRpc->queueSize[RM_PMU_MESSAGE_QUEUE]     = (LwU16)QUEUE_DMEM_QUEUE_LENGTH_MESSAGE;

    // Populate the RM managed heap data in the INIT message.
    offset = (LwU16)OS_LABEL(_end_rmqueue);
    pRpc->rmManagedAreaOffset = offset;
    pRpc->rmManagedAreaSize = (LwU16)OS_LABEL(_end_physical_dmem) - offset;
}

/*!
 * @copydoc queueDmemProcessCmdQueue
 */
FLCN_STATUS
queueDmemProcessCmdQueue
(
    LwU32 headId
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 head = pmuQueueRmToPmuHeadGet_HAL(&Pmu, headId);

    if (headId > RM_PMU_COMMAND_QUEUE_LPQ)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        PMU_BREAKPOINT();
        goto queueDmemProcessCmdQueue_exit;
    }

    while (QueueDmemQueuePtrLwrrent[headId] != head)
    {
        RM_FLCN_CMD_PMU *pRpc;
        LwU32 cmdSize;

        pRpc = (RM_FLCN_CMD_PMU *)QueueDmemQueuePtrLwrrent[headId];
        cmdSize = LWUPROC_ALIGN_UP(pRpc->hdr.size, sizeof(LwU32));

        if (pRpc->hdr.unitId == RM_PMU_UNIT_REWIND)
        {
            QueueDmemQueuePtrLwrrent[headId] = QueueDmemQueuePtrStart[headId];

            // Head might have changed!
            head = pmuQueueRmToPmuHeadGet_HAL(&Pmu, headId);
            osCmdmgmtCmdQSweep(&pRpc->hdr, (LwU8)headId);
        }
        else
        {
            DISP2UNIT_RM_RPC rpcDisp;

            // Must be done before dispatch.
            QueueDmemQueuePtrLwrrent[headId] += cmdSize;

            // Build dispatching structure.
            rpcDisp.hdr.eventType = DISP2UNIT_EVT_RM_RPC;
            rpcDisp.hdr.unitId    = pRpc->hdr.unitId;
            rpcDisp.cmdQueueId    = (LwU8)headId;
            rpcDisp.pRpc          = pRpc;

            // Dispatch command to the handling task.
            status = cmdmgmtExtCmdDispatch(&rpcDisp);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                break;
            }
        }
    }

queueDmemProcessCmdQueue_exit:
    return status;
}

/*!
 * @brief   Callwlate pointer within RM managed dmem heap and
 *          return the pointer. On FBQ systems RM provides offset into
 *          heap. On non-FBQ systems, RM provides full offset into dmem
 *          which doesnt need to be modified.
 *
 * @param[in]       offset      LwU32 heap offset
 *
 * @return          Pointer to location in heap (32/64 bit)
 */
void *
cmdmgmtOffsetToPtr
(
    LwU32 offset
)
{
    return (void *)offset;
}

/* ------------------------- Private Functions ------------------------------ */
