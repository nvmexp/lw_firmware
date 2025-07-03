
/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task_hwv.c
 * @brief  Task that performs HW feature validation
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2_objhwv.h"
#include "sec2_chnmgmt.h"
#include "hwv/hwv_mthds.h"
#include "hwv/sec2_hwv.h"
#include "tsec_drv.h"
#include "class/cl95a1.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS _hwvDoCopy(void)
    GCC_ATTRIB_SECTION("imem_hwv", "_hwvDoCopy");

static FLCN_STATUS _hwvProcessMethod(void)
    GCC_ATTRIB_SECTION("imem_hwv", "_hwvProcessMethod");

/* ------------------------- Global Variables ------------------------------- */
/*!
 * This is the queue that may be used for dispatching work items to the task.
 * It is assumed that this queue is setup/created before this task is scheduled
 * to run. Work-items may be HWV commands.
 */
LwrtosQueueHandle HwvQueue;

/* ------------------------- Static Variables ------------------------------- */
/*!
 * Global buffer used for DMA by various sub routines of HWV task
 */
static LwU8 g_hwvBuf[HWV_DMA_BUFFER_SIZE] GCC_ATTRIB_ALIGN(256);

/* ------------------------- Private Functions ------------------------------ */
FLCN_STATUS
_hwvDoCopy(void)
{
    FLCN_STATUS      status    = FLCN_OK;
    LwU32            size      = HWV_GET_METHOD_PARAM_OFFSET_FROM_ID(DO_COPY);
    LwU64            address   = HWV_GET_METHOD_PARAM_OFFSET_FROM_ID(SET_DO_COPY_IO_BUFFER);
    LwU32            current   = 0;
    LwU32            tmp       = 0;
    RM_FLCN_MEM_DESC memDesc;

    // Address is by 256 byte aligned by default.
    address = address << 8;

    RM_FLCN_U64_PACK(&(memDesc.address), &address);
    memDesc.params = 0;
    memDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                                     RM_SEC2_DMAIDX_VIRT, memDesc.params);
    memDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE, size,
                                     memDesc.params);

    while (current < size)
    {
        tmp = size - current;
        if (tmp >= HWV_DMA_BUFFER_SIZE)
        {
            tmp = HWV_DMA_BUFFER_SIZE;
        }

        // Read max allowed size
        if ((status = dmaRead(&g_hwvBuf, &memDesc, current, tmp)) != FLCN_OK)
        {
            goto label_return;
        }

        // TODO: Callwlate CRC here

        // Write back the content
        if ((status = dmaWrite(&g_hwvBuf, &memDesc, current, tmp)) != FLCN_OK)
        {
            goto label_return;
        }
        current += tmp;
    }

label_return:

    // Notify COMPLETION or NACK
    if (status == FLCN_ERR_DMA_NACK)
    {
        NOTIFY_EXELWTE_NACK();
    }
    else
    {
        NOTIFY_EXELWTE_COMPLETE();
    }

    return status;
}

/*!
 * @brief Processes the methods received via the channel.
 *
 * Iterates over all the methods received from the channel. For each method,
 * the function reads in the parameter for the method, calls the actual handler
 * to process the method, the writes the parameter back to the channel.
 *
 * @return
 *      FLCN_OK if the method is processed succesfully; otherwise, a detailed
 *      error code is returned.
 */
FLCN_STATUS
_hwvProcessMethod(void)
{
    FLCN_STATUS status = FLCN_ERROR;
    LwU32       mthd;
    LwU32       i;

    FOR_EACH_VALID_METHOD(i, appMthdValidMask)
    {
        mthd        = HWV_GET_METHOD_ID(i);

        if (mthd == HWV_METHOD_ID(DO_COPY))
        {
            status = _hwvDoCopy();
            break;
        }
        else if (mthd == HWV_METHOD_ID(PERF_EVAL))
        {
            status = hwvRunPerfEval();
            break;
        }
    }

    return status;
}

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Defines the entry-point for this task.  This function will execute forever
 * processing each command that arrives in the message queues and exelwting
 * each command.
 */
lwrtosTASK_FUNCTION(task_hwv, pvParameters)
{
    DISPATCH_HWV dispatch;

    while (LW_TRUE)
    {
        if (OS_QUEUE_WAIT_FOREVER(HwvQueue, &dispatch))
        {
            //
            // Process the method(s) sent via the host/channel. This is
            // lwrrently the only way the HWV task receives commands.
            //
            (void) _hwvProcessMethod();

        }
    }
}
