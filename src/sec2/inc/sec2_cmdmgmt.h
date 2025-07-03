/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */


#ifndef SEC2_CMDMGMT_H
#define SEC2_CMDMGMT_H

/*!
 * @file   sec2_cmdmgmt.h
 * @brief  Provides the definitions and interfaces for all items related to
 *         command and message queue management.
 */


/* ------------------------- System includes -------------------------------- */
#include "dev_sec_csb.h"

/* ------------------------- Application includes --------------------------- */
#include "unit_dispatch.h"

/* ------------------------- Defines ---------------------------------------- */

/*!
 * We only have one command queue for now. It is used by RM to send commands 
 * to SEC2.
 *
 * NOTE : Last queue (LW_CSEC_QUEUE_HEAD__SIZE_1 - 1) is reserved for PMU
 *        (used by GPC-RG feature to bootstrap GPCCS falcon)
 *        Available queues are from 0 to (LW_CSEC_QUEUE_HEAD__SIZE_1 - 2)
 *
 * E.g. on GA10X, queue allocation is as follows -
 *     a. Queue 0 - used by RM to send generic commands to SEC2
 *     b. Queue 7 - used by PMU to send GPCCS bootstrap command to SEC2
 */
#define SEC2_CMDMGMT_CMD_QUEUE_START                   0 // Do not change this
#define SEC2_CMDMGMT_CMD_QUEUE_RM                      0
#define SEC2_CMDMGMT_CMD_QUEUE_NUM                     1 // This should be the number of command queues

/*!
 * Last queue (size -1) is dedicated to PMU for GPC-RG feature (to bootstrap GPCCS)
 * We only use the queue head interrupt but not the full fledged queue.
 * So its defined separately and not considered in the number of queues
 * defined above.
 */
#define SEC2_CMDMGMT_CMD_QUEUE_PMU                     (LW_CSEC_QUEUE_HEAD__SIZE_1 - 1)

/*!
 * We only have one message queue for now. It is used by SEC2 to communicate 
 * with RM (send responses to commands or messages). 
 */
#define SEC2_CMDMGMT_MSG_QUEUE_START                   0 // Do not change this
#define SEC2_CMDMGMT_MSG_QUEUE_RM                      0
#define SEC2_CMDMGMT_MSG_QUEUE_NUM                     1 // This should be the number of message queues

/*!
 * Array containing the start offsets in DMEM of all command queues.
 */
extern LwU32 Sec2MgmtCmdOffset[SEC2_CMDMGMT_CMD_QUEUE_NUM];

/*!
 * Array containing the start offsets in DMEM of all message queues.
 */
extern LwU32 Sec2MgmtMsgOffset[SEC2_CMDMGMT_CMD_QUEUE_NUM];

/*!
 * This is the generic structure that may be used to send any type of event to
 * the command-dispatcher.  The type of event may be EVT_COMMAND or EVT_SIGNAL
 * and is stored in 'disp2unitEvt'.  EVT_COMMAND is used when the ISR detects
 * a command in the SEC2 command queues.  EVT_SIGNAL is used when the ISR gets
 * an interrupt it is not capable of handling.  No data is passed with the
 * EVT_COMMAND event.  For signals, the 'signal' structure may be used to
 * determine the signal type and signal attributes.
 */
typedef union
{
    LwU8                            disp2unitEvt;
}DISPATCH_CMDMGMT;

/*
 * extern function declarations
 */
void processCommand(LwU8 queueId);

#endif // SEC2_CMDMGMT_H
