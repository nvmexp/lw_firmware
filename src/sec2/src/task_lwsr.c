/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task_lwsr.c
 * @brief  Task that performs validation services related to LWSR
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "sec2_chnmgmt.h"

#include "lwsr/lwsr_mthds.h"
#include "lwsr/sec2_lwsr.h"
#include "class/cl95a1.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/*!
 * This is the queue that may be used for dispatching work items to the task.
 * It is assumed that this queue is setup/created before this task is scheduled
 * to run. Work-items may be LWSR commands.
 */
LwrtosQueueHandle LwsrQueue;

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static void _lwsrProcessMethod(void)
    GCC_ATTRIB_SECTION("imem_lwsr", "_lwsrProcessMethod");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Defines the entry-point for this task.  This function will execute forever
 * processing each command that arrives in the message queues and exelwting
 * each command.
 */
lwrtosTASK_FUNCTION(task_lwsr, pvParameters)
{
    DISPATCH_LWSR dispatch;

    while (LW_TRUE)
    {
        if (OS_QUEUE_WAIT_FOREVER(LwsrQueue, &dispatch))
        {
            //
            // Process the method(s) sent via the host/channel. This is
            // lwrrently the only way the LWSR task receives commands.
            //
            _lwsrProcessMethod();
        }
    }
}

/*!
 * @brief Processes the methods received via the channel.
 *
 * Iterates over all the methods received from the channel. For each method,
 * the function reads in the parameter for the method, calls the actual handler
 * to process the method, the writes the parameter back to the channel.
 */
static void
_lwsrProcessMethod(void)
{
    FLCN_STATUS retVal = FLCN_OK;
    LwU32       i;
    LwU32       mthd;

    FOR_EACH_VALID_METHOD(i, appMthdValidMask)
    {
        mthd = LWSR_GET_METHOD_ID(i);
        switch (mthd)
        {
            case LWSR_METHOD_ID(MUTEX_ACQUIRE):
                retVal = lwsrMutexKeyComputation();
                break;
            default:
                break;
        }
        if (retVal != FLCN_OK)
        {
            break;
        }
    }

    if (retVal == FLCN_ERR_ILLEGAL_OPERATION)
    {
        NOTIFY_EXELWTE_NACK();
    }
    else
    {
        NOTIFY_EXELWTE_COMPLETE();
    }
}

