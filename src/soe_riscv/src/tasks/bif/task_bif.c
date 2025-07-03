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
 * @file   task_bif.c
 * @brief  BIF task
 *
 * This code is the main BIF task, which handles servicing commmands for Driver.
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwrtos.h"

/* ------------------------- Application Includes --------------------------- */
#include "tasks.h"
#include "cmdmgmt.h"
#include "soe_objbif.h"

#include <lwriscv/print.h>

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static void  _bifRequestHandler(LwU8 cmdType, LwU8 cmdQueueId, RM_FLCN_CMD_SOE *pCmd);
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Main task entry point and event processing loop.
 */
lwrtosTASK_FUNCTION(bifMain, pvParameters)
{
    DISP2UNIT_CMD  disp2Bif = { 0 };

    LWOS_TASK_LOOP_NO_CALLBACKS(Disp2QBifThd, &disp2Bif, status, LWOS_TASK_CMD_BUFFER_ILWALID)
    {
        RM_FLCN_CMD_SOE *pCmd = disp2Bif.pCmd;
        switch (pCmd->hdr.unitId)
        {
            case RM_SOE_UNIT_BIF:
            {
                 _bifRequestHandler(pCmd->cmd.bif.cmdType, 
                                    disp2Bif.cmdQueueId,
                                    pCmd);
                break;
            }

            default:
            {
                dbgPrintf(LEVEL_ALWAYS, "bifMain: default\n");
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

static void
_bifRequestHandler
(
    LwU8 cmdType,
    LwU8 cmdQueueId,
    RM_FLCN_CMD_SOE *pCmd
)
{
    FLCN_STATUS status = FLCN_OK;
    RM_FLCN_QUEUE_HDR hdr;
    RM_SOE_BIF_CMD bifCmd = pCmd->cmd.bif;

    // We got a event/command, so process that
    switch (cmdType)
    {
        // Handle commands from driver
        case RM_SOE_BIF_CMD_UPDATE_EOM:
        {
            status = bifSetEomParameters_HAL(&Bif, 
                                             bifCmd.eomctl.mode,
                                             bifCmd.eomctl.nblks, 
                                             bifCmd.eomctl.nerrs,
                                             bifCmd.eomctl.berEyeSel);
            if (status != FLCN_OK)
            {
                dbgPrintf(LEVEL_ALWAYS, "bifSetEomParameters_HAL failed with status %d\n", status);
            }

            break;
        }

        case RM_SOE_BIF_CMD_GET_UPHY_DLN_CFG_SPACE:
        {
            status = bifGetUphyDlnCfgSpace_HAL(&Bif,
                                               bifCmd.cfgctl.regAddress,
                                               bifCmd.cfgctl.laneSelectMask);
            if (status != FLCN_OK)
            {
                dbgPrintf(LEVEL_ALWAYS, "bifGetUphyDlnCfgSpace_HAL failed with status %d\n", status);
            }

            break;
        }

        case RM_SOE_BIF_CMD_SET_PCIE_LINK_SPEED:
        {
            status = bifSetPcieLinkSpeed_HAL(&Bif, bifCmd.speedctl.linkSpeed);
            if (status != FLCN_OK)
            {
                dbgPrintf(LEVEL_ALWAYS, "bifSetPcieLinkSpeed_HAL failed with status %d\n", status);
            }

            break;
        }

        case RM_SOE_BIF_CMD_GET_EOM_STATUS:
        {
            status = bifGetEomStatus_HAL(&Bif,
                                         bifCmd.eomStatus.mode,
                                         bifCmd.eomStatus.nblks,
                                         bifCmd.eomStatus.nerrs,
                                         bifCmd.eomStatus.berEyeSel,
                                         bifCmd.eomStatus.laneMask,
                                         bifCmd.eomStatus.dmaHandle);
            if (status != FLCN_OK)
            {
                dbgPrintf(LEVEL_ALWAYS, "bifGetEomStatus_HAL failed with status %d\n", status);
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
            dbgPrintf(LEVEL_ALWAYS, "_bifRequestHandler: Failed sending response back to driver\n");
        }
    }

    //
    // always sweep the commands in the queue, regardless of the validity of
    // the command.
    //
    cmdQueueSweep(&pCmd->hdr, cmdQueueId);

    return;
}
