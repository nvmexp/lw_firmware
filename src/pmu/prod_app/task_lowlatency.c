/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task_lowlatency.c
 * @brief  handles tasks which have less threshold for latency
 *
 * This task lwrrently handles all the items which cannot tolerate more latency
 */

/* ------------------------- System Includes ------------------------------- */
#include "pmusw.h"
#include "dbgprintf.h"
#include "pmu_objtimer.h"
#include "pmu/pmumailboxid.h"

/* ------------------------- Application Includes -------------------------- */
#include "main.h"
#include "main_init_api.h"
#include "cmdmgmt/cmdmgmt.h"
#include "os/pmu_lwos_task.h"
#include "pmu_objlowlatency.h"
#include "pmu_objpmu.h"
#include "pmu_objbif.h"

/* ------------------------- Types Definitions ----------------------------- */
/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Global Variables ------------------------------ */
LwrtosTaskHandle OsTaskLowlatency;

/* ------------------------- Static Variables ------------------------------ */
/*!
 * @brief Union of all possible RPC subsurface data that can be handled by the
 *        LOWLATENCY task.
 */
typedef union
{
    /*!
     * @brief Default size, as defined by configuration
     */
    LwU8 defaultSize[OSTASK_ATTRIBUTE_LOWLATENCY_CMD_SCRATCH_SIZE];
} TASK_SURFACE_UNION_LOWLATENCY;

/*!
 * @brief   Defines the command buffer for the LOWLATENCY task.
 */
PMU_TASK_CMD_BUFFER_DEFINE_SCRATCH_SIZE(LOWLATENCY, "dmem_lowlatencyCmdBuffer");

/* ------------------------- Prototypes ------------------------------------ */
static FLCN_STATUS s_lowlatencyEventHandle(DISPATCH_LOWLATENCY *pRequest);

lwrtosTASK_FUNCTION(task_lowlatency, pvParameters);

/* ------------------------- Private Functions ------------------------------ */
static FLCN_STATUS
s_lowlatencyEventHandle
(
    DISPATCH_LOWLATENCY *pRequest
)
{
    FLCN_STATUS status = FLCN_ERR_QUEUE_TASK_ILWALID_EVENT_TYPE;

    switch (pRequest->hdr.eventType)
    {
        case LOWLATENCY_EVENT_ID_LANE_MARGINING_INTERRUPT:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_BIF_LANE_MARGINING))
            {
                OSTASK_OVL_DESC ovlDescList[] = {
                    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBifMargining)
                    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, bifLaneMargining)
                };

                OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
                {
                    status = bifServiceMarginingIntr_HAL();
                }
                OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            }
            break;
        }
    }

    return status;
}

/* ------------------------- Public Functions ------------------------------ */
/*!
 * @brief      Initialize the LOWLATENCY Task.
 *
 * @return     FLCN_OK on success,
 * @return     descriptive error code otherwise.
 */
FLCN_STATUS
lowlatencyPreInitTask(void)
{
    FLCN_STATUS status = FLCN_OK;

    // Due to bug 200609848, init value is 1 instead of 0
    LwU32 queueSize = 1;

    //
    // 16 bins of 2^15[ns] == ~32.8[us] ending at ~524[us]
    // 16 bins of 2^18[ns] == ~262[us] last bin starting at ~4.2[ms]
    //
    OSTASK_CREATE(status, PMU, LOWLATENCY);
    if (status != FLCN_OK)
    {
        goto lowlatencyPreInitTask_exit;
    }

    status = lwrtosQueueCreateOvlRes(&LWOS_QUEUE(PMU, LOWLATENCY), queueSize,
                                     sizeof(DISPATCH_LOWLATENCY));
    if (status != FLCN_OK)
    {
        goto lowlatencyPreInitTask_exit;
    }

lowlatencyPreInitTask_exit:
    return status;
}

/*!
 * Entry point for the LOWLATENCY task. This task does not receive any parameters,
 * and never returns.
 */
lwrtosTASK_FUNCTION(task_lowlatency, pvParameters)
{
    DISPATCH_LOWLATENCY dispatch;

    LWOS_TASK_LOOP_NO_CALLBACKS(LWOS_QUEUE(PMU, LOWLATENCY), &dispatch, status, PMU_TASK_CMD_BUFFER_PTRS_GET(LOWLATENCY))
    {
        status = s_lowlatencyEventHandle(&dispatch);
    }
    LWOS_TASK_LOOP_END(status);
}
