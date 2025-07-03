/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    sec2_queuegp10x.c
 * @brief   SEC2 HAL functions for GP10X implementing queue handling.
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"

#include "dev_sec_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_sec2_private.h"

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
sec2ReadCmdQueueHead_GP10X
(
    LwU32 queueIndex
)
{
    return REG_RD32(CSB, LW_CSEC_QUEUE_HEAD(queueIndex));
}

/*!
 * @brief Read command queue tail register
 *
 * @param[in]  queueIndex  Index of command queue whose tail to read
 * @returns    LwU32       DMEM address of the command queue tail
 *
 */
LwU32
sec2ReadCmdQueueTail_GP10X
(
    LwU32 queueIndex
)
{
    return REG_RD32(CSB, LW_CSEC_QUEUE_TAIL(queueIndex));
}

/*!
 * @brief Write command queue head register
 *
 * @param[in]  queueIndex  Index of command queue whose head to write
 * @param[in]  val         New DMEM address of the command queue head
 */
void
sec2WriteCmdQueueHead_GP10X
(
    LwU32 queueIndex,
    LwU32 val
)
{
    REG_WR32(CSB, LW_CSEC_QUEUE_HEAD(queueIndex), val);
}

/*!
 * @brief Write command queue tail register
 *
 * @param[in]  queueIndex  Index of command queue whose tail to write
 * @param[in]  val         New DMEM address of the command queue head
 */
void
sec2WriteCmdQueueTail_GP10X
(
    LwU32 queueIndex,
    LwU32 val
)
{
    REG_WR32(CSB, LW_CSEC_QUEUE_TAIL(queueIndex), val);
}

/*!
 * @brief   Retrieve RM (MSG) queue head register.
 *
 * @return  LwU32   DMEM address of the RM (MSG) queue head.
 */
LwU32
sec2QueueRmHeadGet_GP10X(void)
{
    return REG_RD32(CSB, LW_CSEC_MSGQ_HEAD(0));
}

/*!
 * @brief   Update RM (MSG) queue head register and trigger interrupt to host.
 *
 * @param[in]   head    New DMEM address of the RM (MSG) queue head.
 */
void
sec2QueueRmHeadSet_GP10X
(
    LwU32 head
)
{
    REG_WR32(CSB, LW_CSEC_MSGQ_HEAD(0), head);

    REG_WR32(CSB, LW_CSEC_FALCON_IRQSSET,
             DRF_SHIFTMASK(LW_CSEC_FALCON_IRQSSET_SWGEN0));
}

/*!
 * @brief   Retrieve RM (MSG) queue tail register.
 *
 * @return  LwU32   DMEM address of the RM (MSG) queue tail.
 */
LwU32
sec2QueueRmTailGet_GP10X(void)
{
    return REG_RD32(CSB, LW_CSEC_MSGQ_TAIL(0));
}

/*!
 * @brief   Initializes the head and tail pointers of the RM (MSG) queue.
 *
 * @param[in]   start   Queue start value to initialize the head/tail pointers.
 */
void
sec2QueueRmInit_GP10X
(
    LwU32 start
)
{
    REG_WR32(CSB, LW_CSEC_MSGQ_TAIL(0), start);
    REG_WR32(CSB, LW_CSEC_MSGQ_HEAD(0), start);
}

/* ------------------------- Private Functions ------------------------------ */
