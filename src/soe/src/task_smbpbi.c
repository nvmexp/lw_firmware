/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
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

/* ------------------------- Application Includes --------------------------- */
#include "soe_objsmbpbi.h"
#include "main.h"
#include "config/g_smbpbi_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static void             _smbpbiEventHandle(DISP2UNIT_CMD *pRequest)
GCC_ATTRIB_SECTION("imem_saw", "smbpbiEventHandle");


/* ------------------------- Global Variables ------------------------------- */
LwrtosQueueHandle   Disp2QSmbpbiThd;
OS_TIMER            SmbpbiOsTimer;

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
    osTimerInitTracked(OSTASK_TCB(SMBPBI), &SmbpbiOsTimer,
                       SMBPBI_OS_TIMER_ENTRY_NUM_ENTRIES);
}

/*!
 * @brief Main task entry point and event processing loop.
 */
lwrtosTASK_FUNCTION(task_smbpbi, pvParameters)
{
    DISP2UNIT_CMD  disp2Smbpbi = { 0 };

    smbpbiInit_HAL();

    for (;;)
    {
        OS_TIMER_WAIT_FOR_QUEUE_OR_CALLBACK_START(&SmbpbiOsTimer, Disp2QSmbpbiThd,
                                                  &disp2Smbpbi, lwrtosMAX_DELAY);
        {
            RM_FLCN_CMD_SOE *pCmd = disp2Smbpbi.pCmd;
            switch (pCmd->hdr.unitId)
            {
                case RM_SOE_UNIT_SMBPBI:
                {
                    _smbpbiEventHandle(&disp2Smbpbi);
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
        OS_TIMER_WAIT_FOR_QUEUE_OR_CALLBACK_STOP(&SmbpbiOsTimer, lwrtosMAX_DELAY);
    }
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
