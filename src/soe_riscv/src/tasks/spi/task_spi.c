/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* ------------------------- Application Includes --------------------------- */
#include "soe_objspi.h"
#include "tasks.h"
#include "cmdmgmt.h"
#include "config/g_spi_hal.h"

#include <lwriscv/print.h>

/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS _spiEventHandle(DISP2UNIT_CMD *pRequest);

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Entry point for the SPI task. This task does not receive any parameters.
 */
lwrtosTASK_FUNCTION(spiMain, pvParameters)
{
    DISP2UNIT_CMD  disp2Spi;
    RM_FLCN_QUEUE_HDR hdr;

    LWOS_TASK_LOOP_NO_CALLBACKS(Disp2QSpiThd, &disp2Spi, status, LWOS_TASK_CMD_BUFFER_ILWALID)
    {
        RM_FLCN_CMD_SOE *pCmd = disp2Spi.pCmd;
        switch (pCmd->hdr.unitId)
        {
            case RM_SOE_UNIT_SPI:
                status = _spiEventHandle(&disp2Spi);
                if (status == FLCN_OK)
                {
                    //
                    // Bug 200564601: Ideally we want to be able to transmit
                    // the exact error condition here instead of just
                    // forcing a timeout by not responding.
                    //
                    hdr.unitId  = RM_SOE_UNIT_NULL;
                    hdr.ctrlFlags = 0;
                    hdr.seqNumId = pCmd->hdr.seqNumId;
                    hdr.size = RM_FLCN_QUEUE_HDR_SIZE;
                    if (!msgQueuePostBlocking(&hdr, NULL))
                    {
                        dbgPrintf(LEVEL_ALWAYS, "spiMain: Failed sending response back to driver\n");
                    }
                }
                break;

            default:
            {
                dbgPrintf(LEVEL_ALWAYS, "spiMain: default\n");
            }
        }

        //
        // always sweep the commands in the queue, regardless of the validity of
        // the command.
        //
        cmdQueueSweep(&pCmd->hdr, disp2Spi.cmdQueueId);
    }
    LWOS_TASK_LOOP_END(status);
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief   Helper call handling events sent to SPI task.
 */
static FLCN_STATUS
_spiEventHandle
(
    DISP2UNIT_CMD *pRequest
)
{
    FLCN_STATUS status = FLCN_OK;

    switch (pRequest->pCmd->cmd.spi.cmdType)
    {
        case RM_SOE_SPI_INIT:
                status = spiRomInit(&spiRom.super);
            break;

        default:
            dbgPrintf(LEVEL_ALWAYS, "_spiEventHandle: Invalid command\n");
            break;
    }

    return status;
}
