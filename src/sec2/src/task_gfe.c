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
 * @file   task_gfe.c
 * @brief  Task that performs validation services related to GFE
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2_objgfe.h"
#include "sec2_chnmgmt.h"

#include "gfe/gfe_mthds.h"
#include "sha256.h"
#include "class/cl95a1.h"
#include "tsec_drv.h"
#include "scp_internals.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/*!
 * This is the queue that may be used for dispatching work items to the task.
 * It is assumed that this queue is setup/created before this task is scheduled
 * to run. Work-items may be GFE commands.
 */
LwrtosQueueHandle GfeQueue;

static void _gfeProcessMethod(void)
    GCC_ATTRIB_SECTION("imem_gfe", "_gfeProcessMethod");
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Defines the entry-point for this task.  This function will execute forever
 * processing each command that arrives in the message queues and exelwting
 * each command.
 */
lwrtosTASK_FUNCTION(task_gfe, pvParameters)
{
    DISPATCH_GFE dispatch;

    while (LW_TRUE)
    {
        if (OS_QUEUE_WAIT_FOREVER(GfeQueue, &dispatch))
        {
            //
            // Process the method(s) sent via the host/channel. This is
            // lwrrently the only way the GFE task receives commands.
            //
            _gfeProcessMethod();
        }
    }
}

static void
_gfeProcessMethod(void)
{
    FLCN_STATUS retVal = FLCN_OK;
    LwU32       i;
    LwU32       mthd;

    FOR_EACH_VALID_METHOD(i, appMthdValidMask)
    {
        mthd = GFE_GET_METHOD_ID(i);
        switch (mthd)
        {
            case GFE_METHOD_ID(READ_ECID):
                retVal = gfeHandleReadEcid();
                break;
            default:
                break;
        }
        if (retVal != FLCN_OK)
        {
            break;
        }
    }

    if (retVal == FLCN_ERR_DMA_NACK)
    {
        NOTIFY_EXELWTE_NACK();
    }
    else
    {
        NOTIFY_EXELWTE_COMPLETE();
    }
}

