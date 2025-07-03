/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task_core.c
 * @brief  CORE task
 *
 * This code is the main CORE task, which handles servicing commmands for Driver.
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwmisc.h"
/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "soe_bar0.h"
#include "soe_objcore.h"



/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static void  _coreRequestHandler(LwU8 cmdType, LwU8 cmdQueueId, RM_FLCN_CMD_SOE *pCmd)
    GCC_ATTRIB_SECTION("imem_core", "_coreRequestHandler");
/* ------------------------- Global Variables ------------------------------- */
LwU8 dmaBuf[SOE_DMA_TEST_BUF_SIZE] __attribute__ ((aligned (SOE_DMA_MAX_SIZE))) __attribute__((section ("dmaBuf")));
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Main task entry point and event processing loop.
 */
lwrtosTASK_FUNCTION(task_core, pvParameters)
{
    DISP2UNIT_CMD  disp2Core = { 0 };
    for (;;)
    {
        OS_TIMER_WAIT_FOR_QUEUE_OR_CALLBACK_START(&CoreOsTimer, Disp2QCoreThd,
                                                  &disp2Core, lwrtosMAX_DELAY);
        {
            RM_FLCN_CMD_SOE *pCmd = disp2Core.pCmd;
            switch (pCmd->hdr.unitId)
            {
                case RM_SOE_UNIT_CORE:
                {
                     _coreRequestHandler(pCmd->cmd.core.cmdType,
                                        disp2Core.cmdQueueId,
                                        pCmd);
                    break;
                }

                default:
                {
                    //
                    // Do nothing. Don't halt since this is a security risk. In
                    // the future, we could return an error code to RM.
                    //
                }
            }
        }
        OS_TIMER_WAIT_FOR_QUEUE_OR_CALLBACK_STOP(&CoreOsTimer, lwrtosMAX_DELAY);
    }
}

/* ------------------------- Private Functions ------------------------------ */

static void
_dmaWriteTest(RM_SOE_CORE_CMD_DMA_TEST *pTestCmd)
{
    memset(dmaBuf, pTestCmd->dataPattern, pTestCmd->xferSize);

    soeDmaWriteToSysmem_HAL(&Soe, &dmaBuf[0], pTestCmd->dmaHandle, 0, pTestCmd->xferSize);
}

static FLCN_STATUS
_dmaReadTest(RM_SOE_CORE_CMD_DMA_TEST *pTestCmd)
{
    LwU32            iter;

    soeDmaReadFromSysmem_HAL(&Soe, &dmaBuf[0], pTestCmd->dmaHandle, 0, pTestCmd->xferSize);

    // Validate the result of the read test
    for (iter = 0; iter < SOE_DMA_TEST_BUF_SIZE; iter++)
    {
        // Only expect the Falcon to read in data as large as xferSize
        if (iter < pTestCmd->xferSize)
        {
            if (dmaBuf[iter] != pTestCmd->dataPattern)
            {
                return FLCN_ERR_ILWALID_STATE;
            }
        }
        // Expect the rest of the data to be still at 0.
        else
        {
            if (dmaBuf[iter] != 0)
            {
                return FLCN_ERR_ILWALID_STATE;
            }
        }
    }

    return FLCN_OK;
}

static void
_coreRequestHandler
(
    LwU8 cmdType,
    LwU8 cmdQueueId,
    RM_FLCN_CMD_SOE *pCmd
)
{
    FLCN_STATUS status = FLCN_OK;
    RM_FLCN_QUEUE_HDR hdr;
    RM_SOE_CORE_CMD coreCmd = pCmd->cmd.core;

    // We got a event/command, so process that
    switch (cmdType)
    {
        // Handle commands from driver
        case RM_SOE_CORE_CMD_READ_BIOS_SIZE:
        {
            RM_SOE_CORE_CMD_BIOS biosCmd = coreCmd.bios;
            status = coreReadBiosSize_HAL(&Core,
                                         biosCmd.dmaHandle);
            break;
        }

        case RM_SOE_CORE_CMD_READ_BIOS:
        {
            RM_SOE_CORE_CMD_BIOS biosCmd = coreCmd.bios;
            status = coreReadBios_HAL(&Core,
                                     biosCmd.offset,
                                     biosCmd.sizeInBytes,
                                     biosCmd.dmaHandle);
            break;
        }

        case RM_SOE_CORE_CMD_DMA_SELFTEST:
        {
            RM_SOE_CORE_CMD_DMA_TEST dmaCmd = coreCmd.dma_test;
            memset(dmaBuf, 0, SOE_DMA_TEST_BUF_SIZE);

            if (dmaCmd.subCmdType == RM_SOE_DMA_WRITE_TEST_SUBCMD)
            {
                _dmaWriteTest(&(dmaCmd));
                status = FLCN_OK;
            }
            else
            {
                status = _dmaReadTest(&(dmaCmd));
            }

            break;
        }

        default:
        {
            status = FLCN_ERR_QUEUE_TASK_ILWALID_CMD_TYPE;
        }
    }

    if (status == FLCN_OK)
    {
        hdr.unitId = RM_SOE_UNIT_NULL;
        hdr.ctrlFlags = 0;
        hdr.seqNumId = pCmd->hdr.seqNumId;
        hdr.size = RM_FLCN_QUEUE_HDR_SIZE;

        osCmdmgmtCmdQSweep(&pCmd->hdr, cmdQueueId);
        osCmdmgmtRmQueuePostBlocking(&hdr, NULL);
    }
    else
    {
        SOE_HALT();
    }

    return;
}
