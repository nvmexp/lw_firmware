/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_queuegf10x.c
 * @brief   PMU HAL functions for GF10X implementing queue handling.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "cmdmgmt/cmdmgmt_queue_dmem.h"
#include "cmdmgmt/cmdmgmt_queue_fb_heap.h"
#include "cmdmgmt/cmdmgmt_queue_fb_ptcb.h"
#include "cmdmgmt/cmdmgmt_queue_fbflcn.h"
#include "cmdmgmt/cmdmgmt_queue_fsp.h"
#include "pmu_fbflcn_if.h"
#include "pmu_objic.h"

#include "config/g_pmu_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Compile Time Checks ---------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Process all PMU incoming queues.
 *
 * @return  Errors propagated from callees.
 */
FLCN_STATUS
pmuProcessQueues_GMXXX(void)
{
    LwU32       headIntrStat;
    LwU32       headIntrMask = IC_CMD_INTREN_SETTINGS;
    LwU32       headId       = 0;
    FLCN_STATUS status       = FLCN_OK;

    headIntrStat = REG_RD32(CSB, LW_CPWR_PMU_CMD_HEAD_INTRSTAT) & headIntrMask;

    while (headIntrStat != 0U)
    {
        if ((headIntrStat & 0x1U) != 0U)
        {
            // Clear an interrupt pending bit for given queue.
            REG_WR32(CSB, LW_CPWR_PMU_CMD_HEAD_INTRSTAT, LWBIT32(headId));

            // Process all requests pending in given queue.
            if (PMUCFG_FEATURE_ENABLED(PMU_FBQ))
            {
                if (headId < RM_PMU_FBQ_CMD_COUNT)
                {
                    if (PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_PER_TASK_COMMAND_BUFFER))
                    {
                        status = queueFbPtcbProcessCmdQueue(headId);
                    }
                    else
                    {
                        status = queueFbHeapProcessCmdQueue(headId);
                    }
                }
                else if (PMUCFG_FEATURE_ENABLED(PMU_CLK_FBFLCN_SUPPORTED) &&
                         (headId == PMU_CMD_QUEUE_IDX_FBFLCN))
                {
                    status = queueFbflcnProcessCmdQueue(headId);
                }
                else if (PMUCFG_FEATURE_ENABLED(PMU_FSP_RPC) &&
                         (headId == PMU_CMD_QUEUE_FSP_RPC_MESSAGE))
                {
                    status = queueFspProcessCmdQueue(headId);
                }
                else
                {
                    // Queue not supported on given build.
                    status = FLCN_ERR_ILWALID_STATE;
                }
            }
            else
            {
                status = queueDmemProcessCmdQueue(headId);
            }

            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto pmuProcessQueues_GMXXX_exit;
            }
        }
        headId++;
        headIntrStat = headIntrStat >> 1;
    }

    // Re-enable the Queue interrupts.
    REG_WR32(CSB, LW_CPWR_PMU_CMD_INTREN, headIntrMask);

pmuProcessQueues_GMXXX_exit:
    return status;
}

/*!
 * @brief   Retrieve PMU to RM (MSG) queue head register.
 *
 * @return  LwU32   Value of the PMU to RM (MSG) queue head.
 */
LwU32
pmuQueuePmuToRmHeadGet_GMXXX(void)
{
    return REG_RD32(CSB, LW_CPWR_PMU_MSGQ_HEAD);
}

/*!
 * @brief   Update PMU to RM (MSG) queue head register and trigger interrupt to host.
 *
 * @param[in]   head    New value of the PMU to RM (MSG) queue head.
 */
void
pmuQueuePmuToRmHeadSet_GMXXX
(
    LwU32 head
)
{
    REG_WR32(CSB, LW_CPWR_PMU_MSGQ_HEAD, head);

    REG_WR32(CSB, LW_CMSDEC_FALCON_IRQSSET, BIT(6));
}

/*!
 * @brief   Retrieve PMU to RM (MSG) queue tail register.
 *
 * @return  LwU32   Value of the PMU to RM (MSG) queue tail.
 */
LwU32
pmuQueuePmuToRmTailGet_GMXXX(void)
{
    return REG_RD32(CSB, LW_CPWR_PMU_MSGQ_TAIL);
}

/*!
 * @brief   Initializes the head and tail pointers of the PMU to RM (MSG) queue.
 *
 * @param[in]   start   Start value to initialize the head/tail pointers.
 */
void
pmuQueuePmuToRmInit_GMXXX
(
    LwU32 start
)
{
    REG_WR32(CSB, LW_CPWR_PMU_MSGQ_TAIL, start);
    REG_WR32(CSB, LW_CPWR_PMU_MSGQ_HEAD, start);
}

/*!
 * @brief   Retrieve RM to PMU (CMD) queue head register.
 *
 * @param[in]   queueId     Queue index
 *
 * @return  LwU32   Value of the RM to PMU (CMD) queue head.
 */
LwU32
pmuQueueRmToPmuHeadGet_GMXXX
(
    LwU32 queueId
)
{
    if (queueId < LW_CPWR_PMU_QUEUE_HEAD__SIZE_1)
    {
        return REG_RD32(CSB, LW_CPWR_PMU_QUEUE_HEAD(queueId));
    }
    else
    {
        PMU_BREAKPOINT();
        return 0;
    }
}

/*!
 * @brief   Retrieve RM to PMU (CMD) queue tail register.
 *
 * @param[in]   queueId     Queue index
 *
 * @return  LwU32   Value of the RM to PMU (CMD) queue tail.
 */
LwU32
pmuQueueRmToPmuTailGet_GMXXX
(
    LwU32 queueId
)
{
    if (queueId < LW_CPWR_PMU_QUEUE_TAIL__SIZE_1)
    {
        return REG_RD32(CSB, LW_CPWR_PMU_QUEUE_TAIL(queueId));
    }
    else
    {
        PMU_BREAKPOINT();
        return 0;
    }
}

/*!
 * @brief   Update RM to PMU (CMD) queue tail register.
 *
 * @param[in]   queueId     Queue index
 * @param[in]   tail        New value of the RM to PMU (CMD) queue head.
 */
void
pmuQueueRmToPmuTailSet_GMXXX
(
    LwU32 queueId,
    LwU32 tail
)
{
    if (queueId < LW_CPWR_PMU_QUEUE_TAIL__SIZE_1)
    {
        REG_WR32(CSB, LW_CPWR_PMU_QUEUE_TAIL(queueId), tail);
    }
    else
    {
        PMU_BREAKPOINT();
    }
}

/*!
 * @brief   Initializes the head and tail pointers of the RM to PMU (CMD) queue.
 *
 * @param[in]   queueId Queue index
 * @param[in]   start   Start value to initialize the head/tail pointers.
 */
void
pmuQueueRmToPmuInit_GMXXX
(
    LwU32 queueId,
    LwU32 start
)
{
    REG_WR32(CSB, LW_CPWR_PMU_QUEUE_TAIL(queueId), start);
    REG_WR32(CSB, LW_CPWR_PMU_QUEUE_HEAD(queueId), start);
}

/* ------------------------- Private Functions ------------------------------ */
