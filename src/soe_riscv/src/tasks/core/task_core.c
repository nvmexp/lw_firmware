/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
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
#include "lwrtos.h"
/* ------------------------- Application Includes --------------------------- */
#include "tasks.h"
#include "cmdmgmt.h"
#include "soe_objcore.h"
#include "soe_objsoe.h"

#include <lwriscv/print.h>

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static void  _coreRequestHandler(LwU8 cmdType, LwU8 cmdQueueId, RM_FLCN_CMD_SOE *pCmd);
/* ------------------------- Global Variables ------------------------------- */
sysTASK_DATA LwU8 dmaBuf[SOE_DMA_TEST_BUF_SIZE] __attribute__ ((aligned (SOE_DMA_MAX_SIZE)));
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Main task entry point and event processing loop.
 */
lwrtosTASK_FUNCTION(coreMain, pvParameters)
{
    DISPATCH_CORE  disp2Core = { 0 };

    LWOS_TASK_LOOP(Disp2QCoreThd, &disp2Core, status, LWOS_TASK_CMD_BUFFER_ILWALID)
    {
        RM_FLCN_CMD_SOE *pCmd = disp2Core.command.pCmd;

        switch (pCmd->hdr.unitId)
        {
            case RM_SOE_UNIT_CORE:
            {
                 _coreRequestHandler(pCmd->cmd.core.cmdType,
                                    disp2Core.command.cmdQueueId,
                                    pCmd);
                break;
            }

            default:
            {
                dbgPrintf(LEVEL_ALWAYS, "coreMain: default\n");
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

static FLCN_STATUS
_dmaWriteTest(RM_SOE_CORE_CMD_DMA_TEST *pTestCmd)
{
    FLCN_STATUS      status;
    memset(dmaBuf, pTestCmd->dataPattern, pTestCmd->xferSize);

    status = soeDmaWriteToSysmem_HAL(&Soe, &dmaBuf[0], pTestCmd->dmaHandle, 0, pTestCmd->xferSize);
    if (status != FLCN_OK)
    {
        dbgPrintf(LEVEL_ALWAYS, "soeDmaWriteToSysmem FAILED %d\n", status);
    }

    return status;
}

static FLCN_STATUS
_dmaReadTest(RM_SOE_CORE_CMD_DMA_TEST *pTestCmd)
{
    LwU32            iter;
    FLCN_STATUS      status;

    status = soeDmaReadFromSysmem_HAL(&Soe, &dmaBuf[0], pTestCmd->dmaHandle, 0, pTestCmd->xferSize);
    if (status != FLCN_OK)
    {
        dbgPrintf(LEVEL_ALWAYS, "soeDmaReadFromSysmem FAILED %d\n", status);
        return status;
    }

    // Validate the result of the read test
    for (iter = 0; iter < SOE_DMA_TEST_BUF_SIZE; iter++)
    {
        // Only expect the Falcon to read in data as large as xferSize
        if (iter < pTestCmd->xferSize)
        {
            if (dmaBuf[iter] != pTestCmd->dataPattern)
            {
                dbgPrintf(LEVEL_ALWAYS, "_dmaReadTest failed.\n");
                return FLCN_ERR_ILWALID_STATE;
            }
        }
        // Expect the rest of the data to be still at 0.
        else
        {
            if (dmaBuf[iter] != 0)
            {
                dbgPrintf(LEVEL_ALWAYS, "_dmaReadTest failed.\n");
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
                status = _dmaWriteTest(&(dmaCmd));
                if (status != FLCN_OK)
                {
                    dbgPrintf(LEVEL_ALWAYS, "dmaWriteTest failed with status %d\n", status);
                }

                status = FLCN_OK;
            }
            else
            {
                status = _dmaReadTest(&(dmaCmd));
                if (status != FLCN_OK)
                {
                    dbgPrintf(LEVEL_ALWAYS, "dmaReadTest failed with status %d\n", status);
                }

                status = FLCN_OK;
            }

            break;
        }

        default:
        {
            status = FLCN_ERR_QUEUE_TASK_ILWALID_CMD_TYPE;
            dbgPrintf(LEVEL_ALWAYS, "_coreRequestHandler: Recieved invalid command %d\n", cmdType);
        }
    }

    if (status == FLCN_OK)
    {
        hdr.unitId = RM_SOE_UNIT_NULL;
        hdr.ctrlFlags = 0;
        hdr.seqNumId = pCmd->hdr.seqNumId;
        hdr.size = RM_FLCN_QUEUE_HDR_SIZE;

        if (!msgQueuePostBlocking(&hdr, NULL))
        {
            dbgPrintf(LEVEL_ALWAYS, "_coreRequestHandler: Failed sending response back to driver\n");
        }
    }

    //
    // always sweep the commands in the queue, regardless of the validity of
    // the command.
    //
    cmdQueueSweep(&pCmd->hdr, cmdQueueId);
}
