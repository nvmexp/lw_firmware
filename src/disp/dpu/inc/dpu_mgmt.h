/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DPU_MGMT_H
#define DPU_MGMT_H

/*!
 * @file   dpu_mgmt.h
 * @brief  Provides the definitions and interfaces for all items related to
 *         command and message queue management.
 */

/* ------------------------ System includes -------------------------------- */
#include "dpusw.h"

/* ------------------------ Application includes --------------------------- */
#include "unit_dispatch.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */


/*!
 * Command queue 0 is used by RM.
 */
#define DPU_MGMT_CMD_QUEUE_RM       0
#define DPU_MGMT_CMD_QUEUE_NUM      1

/*!
 * Message queue 0 is used by RM.
 */
#define DPU_MGMT_MSG_QUEUE_RM       0
#define DPU_MGMT_MSG_QUEUE_NUM      1

/*!
 * Array containing the start offsets in DMEM of all command queues.
 */
extern LwU32 DpuMgmtCmdOffset[DPU_MGMT_CMD_QUEUE_NUM];


/*!
 * Array containing the start offsets in DMEM of all message queues.
 */
extern LwU32 DpuMgmtMsgOffset[DPU_MGMT_MSG_QUEUE_NUM];


/*!
 * This is the generic structure that may be used to send any type of event to
 * the command-dispatcher.  The type of event may be EVT_COMMAND or EVT_SIGNAL
 * and is stored in 'disp2unitEvt'.  EVT_COMMAND is used when the ISR detects
 * a command in the DPU command queues.  EVT_SIGNAL is used when the ISR gets
 * an interrupt it is not capable of handling.  No data is passed with the
 * EVT_COMMAND event.  For signals, the 'signal' structure may be used to
 * determine the signal type and signal attributes.
 */
typedef union 
{
    LwU8  disp2unitEvt;
}DISPATCH_CMDMGMT;

#endif // DPU_MGMT_H

