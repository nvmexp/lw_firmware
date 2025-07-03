/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    soe_queuelr10.c
 * @brief   SOE HAL functions for LR10 implementing queue handling.
 */

/* ------------------------- System Includes -------------------------------- */
#include "soesw.h"

#include "dev_soe_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_soe_private.h"

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
soeReadCmdQueueHead_LR10
(
    LwU32 queueIndex
)
{
    return REG_RD32(CSB, LW_CSOE_QUEUE_HEAD(queueIndex));
}

/*!
 * @brief Read command queue tail register
 *
 * @param[in]  queueIndex  Index of command queue whose tail to read
 * @returns    LwU32       DMEM address of the command queue tail
 *
 */
LwU32
soeReadCmdQueueTail_LR10
(
    LwU32 queueIndex
)
{
    return REG_RD32(CSB, LW_CSOE_QUEUE_TAIL(queueIndex));
}

/*!
 * @brief Write command queue head register
 *
 * @param[in]  queueIndex  Index of command queue whose head to write
 * @param[in]  val         New DMEM address of the command queue head
 */
void
soeWriteCmdQueueHead_LR10
(
    LwU32 queueIndex,
    LwU32 val
)
{
    REG_WR32(CSB, LW_CSOE_QUEUE_HEAD(queueIndex), val);
}

/*!
 * @brief Write command queue tail register
 *
 * @param[in]  queueIndex  Index of command queue whose tail to write
 * @param[in]  val         New DMEM address of the command queue head
 */
void
soeWriteCmdQueueTail_LR10
(
    LwU32 queueIndex,
    LwU32 val
)
{
    REG_WR32(CSB, LW_CSOE_QUEUE_TAIL(queueIndex), val);
}

/*!
 * @brief   Retrieve RM (MSG) queue head register.
 *
 * @return  LwU32   DMEM address of the RM (MSG) queue head.
 */
LwU32
soeQueueRmHeadGet_LR10(void)
{
    return REG_RD32(CSB, LW_CSOE_MSGQ_HEAD(0));
}

/*!
 * @brief   Update RM (MSG) queue head register and trigger interrupt to host.
 *
 * @param[in]   head    New DMEM address of the RM (MSG) queue head.
 */
void
soeQueueRmHeadSet_LR10
(
    LwU32 head
)
{
    REG_WR32(CSB, LW_CSOE_MSGQ_HEAD(0), head);

    REG_WR32(CSB, LW_CSOE_FALCON_IRQSSET,
             DRF_SHIFTMASK(LW_CSOE_FALCON_IRQSSET_SWGEN0));
}

/*!
 * @brief   Retrieve RM (MSG) queue tail register.
 *
 * @return  LwU32   DMEM address of the RM (MSG) queue tail.
 */
LwU32
soeQueueRmTailGet_LR10(void)
{
    return REG_RD32(CSB, LW_CSOE_MSGQ_TAIL(0));
}

/*!
 * @brief   Initializes the head and tail pointers of the RM (MSG) queue.
 *
 * @param[in]   start   Queue start value to initialize the head/tail pointers.
 */
void
soeQueueRmInit_LR10
(
    LwU32 start
)
{
    REG_WR32(CSB, LW_CSOE_MSGQ_TAIL(0), start);
    REG_WR32(CSB, LW_CSOE_MSGQ_HEAD(0), start);
}

/* ------------------------- Private Functions ------------------------------ */
