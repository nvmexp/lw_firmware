/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <lwrtosHooks.h>
#include <sections.h>
#include <syslib/idle.h>

/*!
 * @brief User-mode replacement for the SafeRTOS vIdleTask.
 *        Assumes that idle task runs at priority 0 and no other
 *        tasks run at priority 0.
 */
sysSYSLIB_CODE
lwrtosTASK_FUNCTION(task_idle, pvParameters)
{
    (void)pvParameters;

    while (LW_TRUE)
    {
        lwrtosHookIdle();
    }
}
