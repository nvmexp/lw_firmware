/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    debugger.c
 * @brief   Stub task for task level debugger.
 */

/* ------------------------ System Includes -------------------------------- */

/* ------------------------ Register Includes ------------------------------ */
#include <engine.h>

/* ------------------------ SafeRTOS Includes ------------------------------ */
#include <lwrtos.h>

/* ------------------------ RISC-V system library  -------------------------- */
#include <syslib/syslib.h>
#include <lwriscv/print.h>

/* ------------------------ Module Includes -------------------------------- */
#include "tasks.h"

lwrtosTASK_FUNCTION(debuggerMain, pvParameters)
{
    sysCmdqRegisterNotifier(GSP_CMD_QUEUE_DEBUG);
    sysCmdqEnableNotifier(GSP_CMD_QUEUE_DEBUG);

    dbgPrintf(LEVEL_INFO, "Task level debugging started on queue %d (0x%x)\n", GSP_CMD_QUEUE_DEBUG, LW_PGSP_QUEUE_HEAD(GSP_CMD_QUEUE_DEBUG));

    while (LW_TRUE)
    {
        FLCN_STATUS ret = -1;
        ret = lwrtosTaskNotifyWait(0, BIT(GSP_CMD_QUEUE_DEBUG), NULL, lwrtosMAX_DELAY); // We get queue bit set
        if (ret == FLCN_OK)
        {
            dbgPrintf(LEVEL_ERROR,
                      "tld: Implement message handling, queue head = %x.\n",
                      csbRead(ENGINE_REG(_QUEUE_HEAD(GSP_CMD_QUEUE_DEBUG))));
            sysCmdqEnableNotifier(GSP_CMD_QUEUE_DEBUG);
        }
        else
        {
            sysTaskExit(dbgStr(LEVEL_CRIT, "debugger: Failed waiting for notification."), ret);
        }
    }
}
