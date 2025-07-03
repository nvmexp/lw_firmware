/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
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
#include "lwmisc.h"
/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "soe_bar0.h"
#include "soe_objbif.h"


/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static void  _bifRequestHandler(LwU8 cmdType, LwU8 cmdQueueId, RM_FLCN_CMD_SOE *pCmd)
    GCC_ATTRIB_SECTION("imem_bif", "_bifRequestHandler");
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Main task entry point and event processing loop.
 */
lwrtosTASK_FUNCTION(task_bif, pvParameters)
{
    DISP2UNIT_CMD  disp2Bif = { 0 };
    for (;;)
    {
        if (OS_QUEUE_WAIT_FOREVER(Disp2QBifThd, &disp2Bif))
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
                    //
                    // Do nothing. Don't halt since this is a security risk. In
                    // the future, we could return an error code to RM.
                    //
                }
            }
        }
    }
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
            break;
        }

        case RM_SOE_BIF_CMD_GET_UPHY_DLN_CFG_SPACE:
        {
            status = bifGetUphyDlnCfgSpace_HAL(&Bif,
                                               bifCmd.cfgctl.regAddress,
                                               bifCmd.cfgctl.laneSelectMask);
            break;
        }

        case RM_SOE_BIF_CMD_SET_PCIE_LINK_SPEED:
        {
            status = bifSetPcieLinkSpeed_HAL(&Bif, bifCmd.speedctl.linkSpeed);
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
            break;
        }

        case RM_SOE_BIF_CMD_SERVICE_MARGINING_INTERRUPTS:
        {
            bifServiceMarginingIntr_HAL();
            break;
        }

        case RM_SOE_BIF_CMD_SIGNAL_LANE_MARGINING:
        {
            status = bifDoLaneMargining_HAL(&Bif, bifCmd.laneMargining.laneNum);
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

    return;
}
