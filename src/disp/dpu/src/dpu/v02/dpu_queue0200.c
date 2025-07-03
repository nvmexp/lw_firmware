/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    dpu_queue0200.c
 * @brief   DPU v02.00 HAL functions implementing queue handling.
 */

/* ------------------------- System Includes -------------------------------- */
#include "dpusw.h"
#include "dispflcn_regmacros.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_dpu_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Read command queue head register
 *
 * @param[in]  queueIndex  Index of command queue whose head to read
 * @returns    LwU32       DMEM address of the command queue head
 */
LwU32
dpuReadCmdQueueHead_v02_00
(
    LwU32 queueIndex
)
{
    return DFLCN_REG_RD32(CMDQ_HEAD(queueIndex));
}

/*!
 * @brief Read command queue tail register
 *
 * @param[in]  queueIndex  Index of command queue whose tail to read
 * @returns    LwU32       DMEM address of the command queue tail
 *
 */
LwU32
dpuReadCmdQueueTail_v02_00
(
    LwU32 queueIndex
)
{
    return DFLCN_REG_RD32(CMDQ_TAIL(queueIndex));
}

/*!
 * @brief Write command queue head register
 *
 * @param[in]  queueIndex  Index of command queue whose head to write
 * @param[in]  val         New DMEM address of the command queue head
 */
void
dpuWriteCmdQueueHead_v02_00
(
    LwU32 queueIndex,
    LwU32 val
)
{
    DFLCN_REG_WR32(CMDQ_HEAD(queueIndex), val);
}

/*!
 * @brief Write command queue tail register
 *
 * @param[in]  queueIndex  Index of command queue whose tail to write
 * @param[in]  val         New DMEM address of the command queue tail
 */
void
dpuWriteCmdQueueTail_v02_00
(
    LwU32 queueIndex,
    LwU32 val
)
{
    DFLCN_REG_WR32(CMDQ_TAIL(queueIndex), val);
}

/*!
 * @brief   Retrieve RM (MSG) queue head register.
 *
 * @return  LwU32   DMEM address of the RM (MSG) queue head.
 */
LwU32
dpuQueueRmHeadGet_v02_00(void)
{
    return DFLCN_REG_RD32(MSGQ_HEAD(0));
}

/*!
 * @brief   Update RM (MSG) queue head register and trigger interrupt to host.
 *
 * @param[in]   head    New DMEM address of the RM (MSG) queue head.
 */
void
dpuQueueRmHeadSet_v02_00
(
    LwU32 head
)
{
    DFLCN_REG_WR32(MSGQ_HEAD(0), head);

    DFLCN_REG_WR32(IRQSSET,
             DFLCN_DRF_SHIFTMASK(IRQSSET_SWGEN0));
}

/*!
 * @brief   Retrieve RM (MSG) queue tail register.
 *
 * @return  LwU32   DMEM address of the RM (MSG) queue tail.
 */
LwU32
dpuQueueRmTailGet_v02_00(void)
{
    return DFLCN_REG_RD32(MSGQ_TAIL(0));
}

/*!
 * @brief   Initializes the head and tail pointers of the RM (MSG) queue.
 *
 * @param[in]   start   Queue start value to initialize the head/tail pointers.
 */
void
dpuQueueRmInit_v02_00
(
    LwU32 start
)
{
    DFLCN_REG_WR32(MSGQ_TAIL(0), start);
    DFLCN_REG_WR32(MSGQ_HEAD(0), start);
}

/* ------------------------- Private Functions ------------------------------ */
