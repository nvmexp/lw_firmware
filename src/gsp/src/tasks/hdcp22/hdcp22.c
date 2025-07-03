/*! _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   hdcp22.c
 * @brief  This task implements HDCP 2.2 state mechine
 *         Here is SW IAS describes flow //sw/docs/resman/chips/Maxwell/gm20x/Display/HDCP-2-2_SW_IAS.doc
 */

/* ------------------------- System Includes -------------------------------- */
#include "gspsw.h"
#include "rmgspcmdif.h"
#include "unit_dispatch.h"
#include "tasks.h"
#include <syslib/syslib.h>
#include <lwriscv/print.h>

/* ------------------------- OpenRTOS Includes ------------------------------ */
/* ------------------------- Application includes --------------------------- */
#include "lib_intfchdcp22wired.h"

/* ------------------------- Function Prototypes ---------------------------- */
/* ------------------------- Global Variables ------------------------------- */
extern LwrtosQueueHandle Hdcp22WiredQueue;
#ifndef OS_CALLBACKS
OS_TIMER       Hdcp22WiredOsTimer;
#endif
extern HDCP22_SESSION lwrrentSession;
extern void           cmdQueueSweep(PRM_FLCN_QUEUE_HDR pHdr);
extern LwBool         osCmdmgmtRmQueuePostBlocking(PRM_FLCN_QUEUE_HDR pHdr, const void *pBody);

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static void _hdcp22EventHandle(DISPATCH_HDCP22WIRED *pRequest);

static void
_hdcp22EventHandle
(
    DISPATCH_HDCP22WIRED *pRequest
)
{
    // This buffer is used for mirroring Hdcp22wired command for security purpose
    RM_GSP_COMMAND  gspHdcp22WiredCmd;
    RM_GSP_COMMAND* pCmd;

    switch (pRequest->eventType)
    {
        case DISP2UNIT_EVT_SIGNAL:
        {
            if (pRequest->hdcp22Evt.subType == HDCP22_TIMER0_INTR)
            {
                // Handles the Timer0 Event
                hdcp22wiredHandleTimerEvent(&lwrrentSession,
                                            pRequest->hdcp22Evt.eventInfo.timerEvtType);
            }
            else if (pRequest->hdcp22Evt.subType == HDCP22_SOR_INTR)
            {
                // Disables HDCP22 if SOR is power down.
                hdcp22wiredHandleSorInterrupt(pRequest->hdcp22Evt.eventInfo.sorNum);
            }
            break;
        }
        case DISP2UNIT_EVT_COMMAND:
        {
            //
            // Handles the New Command from RM
            // Lwrrently only valid value the QueueID can take is QUEUE_RM.
            // Else case should not be happening, so putting HALT.
            //
            if (pRequest->command.cmdQueueId == GSP_CMD_QUEUE_RM)
            {
                //
                // Hdcp22wired command copy is done here, instead of command queue mirroring
                // After returning from hdcp22wired ProcessCmd gspHdcp22WiredCmd will not be used again.
                // So, it will not be overwritten by another command
                //
                memcpy((LwU8 *)&gspHdcp22WiredCmd, (LwU8 *)pRequest->command.pCmd, sizeof(RM_GSP_COMMAND));
                pCmd = &gspHdcp22WiredCmd;
                hdcp22wiredProcessCmd(&lwrrentSession,
                                      (RM_FLCN_CMD*)pCmd,
                                      pCmd->hdr.seqNumId,
                                      pRequest->command.cmdQueueId);
                cmdQueueSweep(&(((RM_FLCN_CMD *)(pRequest->command.pCmd))->cmdGen.hdr));
            }
            else
            {
                // No support for non-RM clients for this task.
                GSP_HALT();
            }
            break;
        }
        default:
        {
            break;
        }
    }
}

/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Function Definitions ----------------------------*/
/*!
 * Defines the entry-point for this task.  This function will execute forever
 * processing each command that arrives in the message queues and exelwting
 * each command.
 */
lwrtosTASK_FUNCTION(hdcp22WiredMain, pvParameters)
{
    DISPATCH_HDCP22WIRED disp2Hdcp22;

    dbgPrintf(LEVEL_INFO, "hdcp22: task sarted.\n");

    hdcp22wiredInitialize();

#ifdef OS_CALLBACKS
    LWOS_TASK_LOOP(Hdcp22WiredQueue, &disp2Hdcp22, status, LWOS_TASK_CMD_BUFFER_ILWALID)
    {
        _hdcp22EventHandle(&disp2Hdcp22);
    }
    LWOS_TASK_LOOP_END(status);
#else
    // Dispatch event handling.
    while (LW_TRUE)
    {
        OS_TIMER_WAIT_FOR_QUEUE_OR_CALLBACK_START(&Hdcp22WiredOsTimer,
                                                  Hdcp22WiredQueue,
                                                  &disp2Hdcp22, lwrtosMAX_DELAY);
        {
            _hdcp22EventHandle(&disp2Hdcp22);
        }
        OS_TIMER_WAIT_FOR_QUEUE_OR_CALLBACK_STOP(&Hdcp22WiredOsTimer,
                                                 lwrtosMAX_DELAY);
    }
#endif
}

