/*! _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   hdcp1x.c
 * @brief  This task implements HDCP 1.x validation services.
 */

/* ------------------------- System Includes -------------------------------- */
#include "gspsw.h"
#include "rmgspcmdif.h"
#include "unit_dispatch.h"
#include "tasks.h"
#include <syslib/syslib.h>

/* ------------------------- OpenRTOS Includes ------------------------------ */
/* ------------------------- Application includes --------------------------- */
#include "lib_intfchdcp.h"

/* ------------------------- Function Prototypes ---------------------------- */
static void _hdcp1xEventHandle(DISPATCH_HDCP *pRequest);

/* ------------------------- Global Variables ------------------------------- */
extern LwrtosQueueHandle    Hdcp1xQueue;

/* ------------------------- Static Variables ------------------------------- */
//
// TODO: Check "warning: section `.task_hdcp1x_data' type changed to
// PROGBITS" shows by linker.
//
static HDCP_CONTEXT         HdcpContext;

/* ------------------------- Private Functions ------------------------------ */
static void
_hdcp1xEventHandle
(
    DISPATCH_HDCP *pRequest
)
{
    RM_GSP_COMMAND  hdcp1xCmd;   // Mirroring Hdcp1.x command for security purpose.
    RM_GSP_COMMAND *pCmd = &hdcp1xCmd;

    switch (pRequest->eventType)
    {
        //
        // Commands sent from the RM to the Hdcp task.
        //
        case DISP2UNIT_EVT_COMMAND:
        {
            //
            // Hdcp1x command copy is done here instead of command queue mirroring
            // After returning from hdcpProcessCmd hdcp1xCmd will not be used again.
            // So, it will not be overwritten by another command
            //
            memcpy((LwU8 *)&hdcp1xCmd, (LwU8 *)pRequest->command.pCmd,
                   sizeof(RM_GSP_COMMAND));

            memset(&HdcpContext, 0, sizeof(HDCP_CONTEXT));
            HdcpContext.pOriginalCmd = (RM_FLCN_CMD *)pRequest->command.pCmd;
            HdcpContext.cmdQueueId = pRequest->command.cmdQueueId;
            HdcpContext.seqNumId = pCmd->hdr.seqNumId;
            hdcpProcessCmd(&HdcpContext, (RM_FLCN_CMD *)pCmd);

            break;
        }

        default:
            break;
    }
}

/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Function Definitions ----------------------------*/
/*!
 * Defines the entry-point for this task.  This function will execute forever
 * processing each command that arrives in the message queues and exelwting
 * each command.
 */
lwrtosTASK_FUNCTION(hdcp1xMain, pvParameters)
{
    DISPATCH_HDCP   disp2Hdcp1x;

    //
    // Event processing loop. Process commands sent from the RM to the Hdcp
    // task. Also, handle Hdcp specific signals such as the start of frame
    // signal.
    //
    LWOS_TASK_LOOP(Hdcp1xQueue, &disp2Hdcp1x, status, LWOS_TASK_CMD_BUFFER_ILWALID)
    {
        _hdcp1xEventHandle(&disp2Hdcp1x);

        // GSP is not yet targeted for the error propagation changes.
        status = FLCN_OK;
    }
    LWOS_TASK_LOOP_END(status);
}
