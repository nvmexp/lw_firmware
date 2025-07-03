/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* ------------------------- Application Includes --------------------------- */
#include "soe_objspi.h"
#include "main.h"
#include "config/g_spi_private.h"

/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS _spiEventHandle(DISP2UNIT_CMD *pRequest);

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Entry point for the SPI task. This task does not receive any parameters.
 */
lwrtosTASK_FUNCTION(task_spi, pvParameters)
{
    DISP2UNIT_CMD  disp2Spi;
    RM_FLCN_QUEUE_HDR hdr;
    FLCN_STATUS status;

    for (;;)
    {
        if (OS_QUEUE_WAIT_FOREVER(Disp2QSpiThd, &disp2Spi))
        {
        	RM_FLCN_CMD_SOE *pCmd = disp2Spi.pCmd;
        	switch (pCmd->hdr.unitId)
        	{
        		case RM_SOE_UNIT_SPI:
                    status = _spiEventHandle(&disp2Spi);
                    osCmdmgmtCmdQSweep(&pCmd->hdr, disp2Spi.cmdQueueId);
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
                        osCmdmgmtRmQueuePostBlocking(&hdr, NULL);
                    }
        			break;

        		default:
        			break;
        	}
        }
    }
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
            break;
    }

    return status;
}
