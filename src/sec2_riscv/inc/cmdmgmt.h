/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
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

/* ------------------------- Application includes --------------------------- */
#include <tasks.h>
#include "unit_dispatch.h"

/* ------------------------- Defines ---------------------------------------- */
#define PROFILE_CMD_QUEUE_LENGTH    (0x80ul)
#define PROFILE_MSG_QUEUE_LENGTH    (0x80ul)
#define RM_DMEM_EMEM_OFFSET         0x1000000U

//
// TODO @vsurana : that probably should be derived from hal or profile
// Task (HW) CMD/MSG queue allocation (message has the same #)
//
#define SEC2_MSG_QUEUE_RM       SEC2_CMD_QUEUE_RM


/*!
 * We only have one command queue for now. It is used by RM to send commands
 * to SEC2.
 *
 * E.g. on GA10X, queue allocation is as follows -
 *     a. Queue 0 - used by RM to send generic commands to SEC2
 *     b. Queue 7 - used by PMU to send GPCCS bootstrap command to SEC2
 */
#define SEC2_CMDMGMT_CMD_QUEUE_START                   0 // Do not change this
#define SEC2_CMDMGMT_CMD_QUEUE_RM                      0
#define SEC2_CMDMGMT_CMD_QUEUE_NUM                     1 // This should be the number of command queues

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
extern sysTASK_DATA LwU32 Sec2MgmtCmdOffset[SEC2_CMDMGMT_CMD_QUEUE_NUM];

// Compile time asserts incase any of the queue indexes change!
ct_assert(SEC2_RM_CMDQ_LOG_ID == 0);
ct_assert(SEC2_RM_MSGQ_LOG_ID == 1);
ct_assert(SEC2_QUEUE_NUM == 2);


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
sysSYSLIB_CODE LwBool msgQueuePostBlocking(PRM_FLCN_QUEUE_HDR pHdr, const void *pBody);
void cmdQueueSweep(PRM_FLCN_QUEUE_HDR pHdr, LwU8 queueId);

#endif // SEC2_CMDMGMT_H
