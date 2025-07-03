/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task_therm.c
 * @brief  THERM task
 *
 * This code is the main THERM task, which handles servicing commmands for both
 * itself and various sub-tasks (e.g. I2C, LR10 and SMBPBI) as well as maintaining
 * its own 1Hz callback.
 *
 */

/* ------------------------- Application Includes --------------------------- */
#include "soe_objtherm.h"
#include "tasks.h"
#include "cmdmgmt.h"

#include <lwriscv/print.h>

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static void _thermEventHandle(LwU8 cmdType, LwU8 cmdQueueId, RM_FLCN_CMD_SOE *pCmd);

static void _thermSendAsyncMessageToDriver(LwU8 msgType, LwU8 cmdQueueId, RM_FLCN_CMD_SOE *pCmd,
                                           RM_SOE_THERM_CMD_SEND_ASYNC_MSG msg);

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Main task entry point and event processing loop.
 */
lwrtosTASK_FUNCTION(thermMain, pvParameters)
{
    DISPATCH_THERM  disp2Therm = { 0 };

    LWOS_TASK_LOOP(Disp2QThermThd, &disp2Therm, status, LWOS_TASK_CMD_BUFFER_ILWALID)
    {
        RM_FLCN_CMD_SOE *pCmd = disp2Therm.command.pCmd;
        switch (pCmd->hdr.unitId)
        {
            case RM_SOE_UNIT_THERM:
            {
                 _thermEventHandle(pCmd->cmd.therm.cmdType,
                                   disp2Therm.command.cmdQueueId, pCmd);
                break;
            }

            default:
            {
                dbgPrintf(LEVEL_ALWAYS, "thermMain: default\n");
                //
                // Do nothing. Don't halt since this is a security risk. In
                // the future, we could return an error code to RM.
                //
            }
        }
    }
    LWOS_TASK_LOOP_END(status);
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief Helper call handling events sent to THERM task.
 */
static void
_thermEventHandle
(
    LwU8 cmdType,
    LwU8 cmdQueueId,
    RM_FLCN_CMD_SOE *pCmd
)
{
    FLCN_STATUS status = FLCN_OK;
    RM_FLCN_QUEUE_HDR hdr;
    RM_SOE_THERM_CMD_FORCE_SLOWDOWN cmd;

    // We got a event/command, so process that
    switch (cmdType)
    {
        // Handle commands from driver
        case RM_SOE_THERM_FORCE_SLOWDOWN:
        {
            cmd = pCmd->cmd.therm.slowdown;
            status = thermForceSlowdown_HAL(&Therm, cmd.slowdown, cmd.periodUs);
            if (status != FLCN_OK)
            {
                 dbgPrintf(LEVEL_ALWAYS, "thermForceSlowdown_HAL failed with status %d\n", status);
            }
            break;
        }

        // Send async message to the driver
        case RM_SOE_THERM_SEND_MSG_TO_DRIVER:
        {
            _thermSendAsyncMessageToDriver(pCmd->cmd.therm.msg.status.msgType, cmdQueueId,
                                           pCmd, pCmd->cmd.therm.msg);
            return;
        }

        default:
        {
            status = FLCN_ERR_QUEUE_TASK_ILWALID_UNIT_ID;
            dbgPrintf(LEVEL_ALWAYS, "_thermEventHandle Recieved invalid command %d\n", cmdType);
        }
    }

    if (status == FLCN_OK)
    {
        hdr.unitId = RM_SOE_UNIT_THERM;
        hdr.ctrlFlags = 0;
        hdr.seqNumId = pCmd->hdr.seqNumId;
        hdr.size = RM_FLCN_QUEUE_HDR_SIZE;

        if (!msgQueuePostBlocking(&hdr, NULL))
        {
            dbgPrintf(LEVEL_ALWAYS, "_thermEventHandle: Failed sending response back to driver\n");
        }
    }

    //
    // always sweep the commands in the queue, regardless of the validity of
    // the command.
    //
    cmdQueueSweep(&pCmd->hdr, cmdQueueId);
}

/*!
 * @brief Send non-blocking async message to the driver.
 */
static void
_thermSendAsyncMessageToDriver
(
    LwU8 msgType,
    LwU8 cmdQueueId,
    RM_FLCN_CMD_SOE *pCmd,
    RM_SOE_THERM_CMD_SEND_ASYNC_MSG msg
)
{
    RM_FLCN_QUEUE_HDR hdr;
    RM_SOE_THERM_MSG_SLOWDOWN_STATUS slowdown;
    RM_SOE_THERM_MSG_SHUTDOWN_STATUS shutdown;

    cmdQueueSweep(&pCmd->hdr, cmdQueueId);
    hdr.unitId = RM_SOE_UNIT_THERM;
    hdr.ctrlFlags = RM_FLCN_QUEUE_HDR_FLAGS_EVENT;
    hdr.seqNumId = 0;

    switch (msgType)
    {
        case RM_SOE_THERM_MSG_ID_SLOWDOWN_STATUS:
        {
            slowdown = pCmd->cmd.therm.msg.status.slowdown;
            hdr.size = RM_SOE_MSG_SIZE(THERM, SLOWDOWN_STATUS);
            msgQueuePostBlocking(&hdr, &slowdown);
            break;
        }

        case RM_SOE_THERM_MSG_ID_SHUTDOWN_STATUS:
        {
            shutdown = pCmd->cmd.therm.msg.status.shutdown;
            hdr.size = RM_SOE_MSG_SIZE(THERM, SHUTDOWN_STATUS);
            msgQueuePostBlocking(&hdr, &shutdown);
            break;
        }

        default:
        {
            SOE_HALT();
        }
    }
}
