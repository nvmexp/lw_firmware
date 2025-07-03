/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task_perf_cf.c
 * @brief  PMU PERF_CF Interface Task
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "main.h"
#include "main_init_api.h"
#include "cmdmgmt/cmdmgmt.h"
#include "task_perf_cf.h"
#include "os/pmu_lwos_task.h"
#include "perf/cf/perf_cf.h"
#include "pmu_objpmu.h"

#include "g_pmurpc.h"

/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Defines ----------------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/*!
 * PERF_CF task handle.
 */
LwrtosTaskHandle OsTaskPerfCf;

/* ------------------------ Static Variables -------------------------------- */
/*!
 * @brief Union of all possible RPC subsurface data that can be handled by the
 *        PERF_CF task.
 */
typedef union
{
    /*!
     * @brief Default size, as defined by configuration
     */
    LwU8 defaultSize[OSTASK_ATTRIBUTE_PERF_CF_CMD_SCRATCH_SIZE];
} TASK_SURFACE_UNION_PERF_CF;

/*!
 * @brief   Defines the command buffer for the PERF_CF task.
 */
PMU_TASK_CMD_BUFFER_DEFINE_SCRATCH_SIZE(PERF_CF, "dmem_perfCfCmdBuffer");

/* ------------------------ Prototypes -------------------------------------- */
static FLCN_STATUS s_perfCfEventHandle(DISPATCH_PERF_CF *pRequest);

lwrtosTASK_FUNCTION(task_perf_cf, pvParameters);

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief      Initialize the PERF_CF Task.
 *
 * @return     FLCN_OK on success,
 * @return     descriptive error code otherwise.
 */
FLCN_STATUS
perfCfPreInitTask(void)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 queueSize = 0;

    queueSize++;    // Synchronous RM RPC-s
    queueSize++;    // for Topology callbacks
    queueSize++;    // for Controller callbacks
    queueSize++;    // Async RPC from OPTP

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_TOPOLOGIES_PMUMON))
    {
        queueSize++;    // OsCBPerfCfTopologiesPmuMon
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSORS_PMUMON))
    {
        queueSize++;    // OsCBPerfCfPmSensorsPmuMon
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MEM_TUNE))
    {
        queueSize++;    // perfCfControllerMemTuneMonitorLoad();
    }

    //
    // 16 bins of 2^15[ns] == ~32.8[us] ending at ~524[us]
    // 16 bins of 2^18[ns] == ~262[us] last bin starting at ~4.2[ms]
    //
    OSTASK_CREATE(status, PMU, PERF_CF);
    if (status != FLCN_OK)
    {
        goto perfCfPreInitTask_exit;
    }

    status = lwrtosQueueCreateOvlRes(&LWOS_QUEUE(PMU, PERF_CF), queueSize,
                                     sizeof(DISPATCH_PERF_CF));
    if (status != FLCN_OK)
    {
        goto perfCfPreInitTask_exit;
    }

perfCfPreInitTask_exit:
    return status;
}

/*!
 * Entry point for the PERF_CF task. This task does not receive any parameters,
 * and never returns.
 */
lwrtosTASK_FUNCTION(task_perf_cf, pvParameters)
{
    DISPATCH_PERF_CF dispatch;

    // NJ-TODO: Propagate error code.
    (void)perfCfPostInit();

    LWOS_TASK_LOOP(LWOS_QUEUE(PMU, PERF_CF), &dispatch, status, PMU_TASK_CMD_BUFFER_PTRS_GET(PERF_CF))
    {
        status = s_perfCfEventHandle(&dispatch);
    }
    LWOS_TASK_LOOP_END(status);
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @brief   Helper call handling events sent to PERF_CF task.
 */
static FLCN_STATUS
s_perfCfEventHandle
(
    DISPATCH_PERF_CF *pRequest
)
{
    FLCN_STATUS status = FLCN_ERR_QUEUE_TASK_ILWALID_EVENT_TYPE;

    switch (pRequest->hdr.eventType)
    {
        case DISP2UNIT_EVT_RM_RPC:
        {
            status = FLCN_ERR_QUEUE_TASK_ILWALID_UNIT_ID;

            switch (pRequest->hdr.unitId)
            {
                case RM_PMU_UNIT_PERF_CF:
                {
                    status = pmuRpcProcessUnitPerfCf(&(pRequest->rpc));
                    break;
                }

                case RM_PMU_UNIT_LOGGER:
                {
                    PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_LOGGER);

                    OSTASK_OVL_DESC ovlDescList[] = {
                        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libGpumon)
                    };

                    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
                    {
                        status = pmuRpcProcessUnitLogger(&(pRequest->rpc));
                    }
                    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

                    break;
                }
            }
            break;
        }
        case PERF_CF_EVENT_ID_PERF_CF_CONTROLLER_MEM_TUNE_MONITOR_LOAD:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MEM_TUNE))
            {
                status = perfCfControllerMemTuneMonitorLoad();
            }
            break;
        }
    }

    return status;
}
