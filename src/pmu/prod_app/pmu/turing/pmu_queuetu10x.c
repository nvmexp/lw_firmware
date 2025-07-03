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
 * @file    pmu_queuetu10x.c
 * @brief   PMU HAL functions for TU10X implementing queue handling.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_pri.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "pmu_fbflcn_if.h"
#include "config/g_pmu_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief   Set PLM for head and tail pointers for CMD queues.
 *
 * @param[in]   queueIdx    ID of CMD queue to protect.
 */
void
pmuQueueCmdProtect_TU10X
(
    LwU32 queueIdx
)
{
    // In LS mode only allow privilege level 2 writes to the tail CMD queue pointer.
    if (Pmu.bLSMode)
    {
        LwU32 cmdQueueTailPriv = REG_RD32(CSB, LW_CPWR_PMU_QUEUE_TAIL_PRIV_LEVEL_MASK(queueIdx));
        cmdQueueTailPriv = FLD_SET_DRF(_CPWR_PMU, _QUEUE_TAIL_PRIV_LEVEL_MASK,
                                       _WRITE_PROTECTION_LEVEL0, _DISABLE, cmdQueueTailPriv);
        cmdQueueTailPriv = FLD_SET_DRF(_CPWR_PMU, _QUEUE_TAIL_PRIV_LEVEL_MASK,
                                       _WRITE_PROTECTION_LEVEL2, _ENABLE, cmdQueueTailPriv);
        cmdQueueTailPriv = FLD_SET_DRF(_CPWR_PMU, _QUEUE_TAIL_PRIV_LEVEL_MASK,
                                       _WRITE_VIOLATION, _REPORT_ERROR, cmdQueueTailPriv);

        REG_WR32(CSB, LW_CPWR_PMU_QUEUE_TAIL_PRIV_LEVEL_MASK(queueIdx), cmdQueueTailPriv);

        // Bug 2020850: make sure that PLM writes stick
        LwU32 cmdQueueTailPriv2 = REG_RD32(CSB, LW_CPWR_PMU_QUEUE_TAIL_PRIV_LEVEL_MASK(queueIdx));
        PMU_HALT_COND(cmdQueueTailPriv2 == cmdQueueTailPriv);

        // For FBFLCN=>PMU command queue, protect the head pointer too.
        if (queueIdx == PMU_CMD_QUEUE_IDX_FBFLCN) 
        {
            LwU32 cmdQueueHeadPriv = REG_RD32(CSB, LW_CPWR_PMU_QUEUE_HEAD_PRIV_LEVEL_MASK(queueIdx));
            cmdQueueHeadPriv = FLD_SET_DRF(_CPWR_PMU, _QUEUE_HEAD_PRIV_LEVEL_MASK,
                                        _WRITE_PROTECTION_LEVEL0, _DISABLE, cmdQueueHeadPriv);
            cmdQueueHeadPriv = FLD_SET_DRF(_CPWR_PMU, _QUEUE_HEAD_PRIV_LEVEL_MASK,
                                        _WRITE_PROTECTION_LEVEL2, _ENABLE, cmdQueueHeadPriv);
            cmdQueueHeadPriv = FLD_SET_DRF(_CPWR_PMU, _QUEUE_HEAD_PRIV_LEVEL_MASK,
                                        _WRITE_VIOLATION, _REPORT_ERROR, cmdQueueHeadPriv);

            REG_WR32(CSB, LW_CPWR_PMU_QUEUE_HEAD_PRIV_LEVEL_MASK(queueIdx), cmdQueueHeadPriv);

            // Bug 2020850: make sure that PLM writes stick
            LwU32 cmdQueueHeadPriv2 = REG_RD32(CSB, LW_CPWR_PMU_QUEUE_HEAD_PRIV_LEVEL_MASK(queueIdx));
            PMU_HALT_COND(cmdQueueHeadPriv2 == cmdQueueHeadPriv);

        }

    }
}

/*!
 * @brief   Initializes the head and tail pointers of the RM (MSG) queue.
 *
 * @param[in]   start   Queue start value to initialize the head/tail pointers.
 */
void
pmuQueueMsgProtect_TU10X(void)
{
    // In LS mode only allow privilege level 2 writes to the head MSG queue pointer.
    if (Pmu.bLSMode)
    {
        LwU32 msgQueueHeadPriv = REG_RD32(CSB, LW_CPWR_PMU_MSGQ_HEAD_PRIV_LEVEL_MASK);
        msgQueueHeadPriv = FLD_SET_DRF(_CPWR_PMU, _MSGQ_HEAD_PRIV_LEVEL_MASK,
                                       _WRITE_PROTECTION_LEVEL0, _DISABLE, msgQueueHeadPriv);
        msgQueueHeadPriv = FLD_SET_DRF(_CPWR_PMU, _MSGQ_HEAD_PRIV_LEVEL_MASK,
                                       _WRITE_PROTECTION_LEVEL2, _ENABLE, msgQueueHeadPriv);
        msgQueueHeadPriv = FLD_SET_DRF(_CPWR_PMU, _MSGQ_HEAD_PRIV_LEVEL_MASK,
                                       _WRITE_VIOLATION, _REPORT_ERROR, msgQueueHeadPriv);

        REG_WR32(CSB, LW_CPWR_PMU_MSGQ_HEAD_PRIV_LEVEL_MASK, msgQueueHeadPriv);

        // Bug 2020850: make sure that PLM writes stick
        LwU32 msgQueueHeadPriv2 = REG_RD32(CSB, LW_CPWR_PMU_MSGQ_HEAD_PRIV_LEVEL_MASK);
        PMU_HALT_COND(msgQueueHeadPriv2 == msgQueueHeadPriv);
    }
}

/* ------------------------- Private Functions ------------------------------ */
