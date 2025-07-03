
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
 * @file   task_apm.c
 * @brief  Task that supports Protected Memory for Confidential Compute.
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2_objapm.h"
#include "sec2_chnmgmt.h"
#include "apm/apm_mthds.h"
#include "apm/sec2_apm.h"
#include "class/cl95a1.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/*!
 * This is the queue that may be used for dispatching work items to the task.
 * It is assumed that this queue is setup/created before this task is scheduled
 * to run. Work-items may be APM commands.
 */
LwrtosQueueHandle ApmQueue;

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static void _apmProcessMethod(void)
    GCC_ATTRIB_SECTION("imem_apm", "_apmProcessMethod");

/*!
 * @brief Dispatches methods received via channel to private handler functions.
 * 
 *        On completion of all valid method handlers, or failure of handler,
 *        signal to channel management task that we have completed (or failed).
 */
static void
_apmProcessMethod(void)
{
    LwU32  errorCode = LW95A1_ERROR_NONE;
    LwU32  i         = 0;
    LwU32  mthd      = 0;

    FOR_EACH_VALID_METHOD(i, appMthdValidMask)
    {
        mthd = APM_GET_METHOD_ID(i);

        switch (mthd)
        {
            case APM_METHOD_ID(COPY):
            {
                errorCode = apmInitiateCopy();
                break;
            }
            default:
            {
                break;
            }
        }
    }

    // Signal chnmgmt on task completion, or failure due to DMA fault.
    if (errorCode == LW95A1_ERROR_NONE)
    {
        NOTIFY_EXELWTE_COMPLETE();
    }
    else
    {
        NOTIFY_EXELWTE_TRIGGER_RC(errorCode);
    }
}

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Defines the entry-point for this task.  This function will execute forever
 * processing each command that arrives in the message queues and exelwting
 * each command.
 */
lwrtosTASK_FUNCTION(task_apm, pvParameters)
{
    DISPATCH_APM dispatch;

    while (LW_TRUE)
    {
        if (OS_QUEUE_WAIT_FOREVER(ApmQueue, &dispatch))
        {
            //
            // Process the method(s) sent via the host/channel. This is
            // lwrrently the only way the APM task receives commands.
            //
            (void) _apmProcessMethod();
        }
    }
}
