/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */


#ifndef SOE_CMDMGMT_H
#define SOE_CMDMGMT_H

/*!
 * @file   soe_cmdmgmt.h
 * @brief  Provides the definitions and interfaces for all items related to
 *         command and message queue management.
 */

#include "unit_dispatch.h"

/*!
 * We only have one command queue for now. It is used by RM to send commands 
 * to SOE. 
 */
#define SOE_CMDMGMT_CMD_QUEUE_START                   0 // Do not change this
#define SOE_CMDMGMT_CMD_QUEUE_RM                      0
#define SOE_CMDMGMT_CMD_QUEUE_NUM                     1 // This should be the number of command queues

/*!
 * We only have one message queue for now. It is used by SOE to communicate 
 * with RM (send responses to commands or messages). 
 */
#define SOE_CMDMGMT_MSG_QUEUE_START                   0 // Do not change this
#define SOE_CMDMGMT_MSG_QUEUE_RM                      0
#define SOE_CMDMGMT_MSG_QUEUE_NUM                     1 // This should be the number of message queues

/*!
 * Array containing the start offsets in DMEM of all command queues.
 */
extern LwU32 SoeMgmtCmdOffset[SOE_CMDMGMT_CMD_QUEUE_NUM];

/*!
 * Array containing the start offsets in DMEM of all message queues.
 */
extern LwU32 SoeMgmtMsgOffset[SOE_CMDMGMT_CMD_QUEUE_NUM];

/*!
 * This is the generic structure that may be used to send any type of event to
 * the command-dispatcher.  The type of event may be EVT_COMMAND or EVT_SIGNAL
 * and is stored in 'disp2unitEvt'.  EVT_COMMAND is used when the ISR detects
 * a command in the SOE command queues.  EVT_SIGNAL is used when the ISR gets
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

#endif // SOE_CMDMGMT_H
