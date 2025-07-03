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
 * @file   task_smbpbi.c
 * @brief  SMBPBI task
 *
 * This code is the main SMBPBI task, which services SMBPBI protocol requests
 * received from the BMC over SMBus.
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwrtos.h"
/* ------------------------- Application Includes --------------------------- */
#include "tasks.h"
#include "cmdmgmt.h"
#include "soe_objsmbpbi.h"
#include "soe_objcore.h"
#include "soe_objsoe.h"

#include <lwriscv/print.h>
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static void             _smbpbiEventHandle(DISP2UNIT_CMD *pRequest);
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Pre-init the SMBPBI task
 */
void
smbpbiPreInit
(
    void
)
{
    memset(&SmbpbiLwlink, 0, sizeof(SMBPBI_LWLINK));
}

/*!
 * @brief Main task entry point and event processing loop.
 */
lwrtosTASK_FUNCTION(smbpbiMain, pvParameters)
{
    DISPATCH_SMBPBI  disp2Smbpbi = { 0 };

    smbpbiInit_HAL();

    LWOS_TASK_LOOP(Disp2QSmbpbiThd, &disp2Smbpbi, status, LWOS_TASK_CMD_BUFFER_ILWALID)
    {
        RM_FLCN_CMD_SOE *pCmd = disp2Smbpbi.command.pCmd;
        switch (pCmd->hdr.unitId)
        {
            case RM_SOE_UNIT_SMBPBI:
            {
                _smbpbiEventHandle(&disp2Smbpbi.command);
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

/*!
 * @brief   Helper call handling events sent to SMBPBI task.
 */
static void
_smbpbiEventHandle
(
    DISP2UNIT_CMD *pRequest
)
{
    smbpbiDispatch(pRequest);
}
