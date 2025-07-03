/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task_nne.c
 * @brief  PMU Neural Net Engine (NNE) Task
 * @copydoc task_nne.h
 *
 * This engine task evaluates neural-nets at low-latency. It is intended for
 * LWPU internal purposes (example: controlling the GPU).
 */
/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "main_init_api.h"
#include "cmdmgmt/cmdmgmt.h"
#include "task_nne.h"
#include "os/pmu_lwos_task.h"
#include "objnne.h"
#include "g_pmurpc.h"

/* ------------------------- Static Function Prototypes  -------------------- */
static FLCN_STATUS s_nneEventHandle(DISPATCH_NNE *pRequest);

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Defines ---------------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
lwrtosTASK_FUNCTION(task_nne, pvParameters);

/* ------------------------- Static Variables ------------------------------- */
/*!
 * @brief Union of all possible RPC subsurface data that can be handled by the NNE task.
 */
typedef union
{
    RM_PMU_NNE_NNE_VAR_BOARDOBJ_GRP_SET      nneVarGrpSet;
    RM_PMU_NNE_NNE_LAYER_BOARDOBJ_GRP_SET    nneLayerGrpSet;
    RM_PMU_NNE_NNE_DESC_BOARDOBJ_GRP_SET     nneDescGrpSet;

    /*!
     * @brief Default size, as defined by configuration
     */
    LwU8 defaultSize[OSTASK_ATTRIBUTE_NNE_CMD_SCRATCH_SIZE];
} TASK_SURFACE_UNION_NNE;

/*!
 * @brief   Defines the command buffer for the NNE task.
 */
PMU_TASK_CMD_BUFFER_DEFINE_SCRATCH_SIZE(NNE, "dmem_nneCmdBuffer");

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief      Initialize the NNE Task.
 *
 * @return     FLCN_OK on success,
 * @return     descriptive error code otherwise.
 */
FLCN_STATUS
nnePreInitTask(void)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 queueSize = 0;

    queueSize++;    // for commands from RM

    OSTASK_CREATE(status, PMU, NNE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto nnePreInitTask_exit;
    }

    status = lwrtosQueueCreateOvlRes(&LWOS_QUEUE(PMU, NNE), queueSize,
                                     sizeof(DISPATCH_NNE));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto nnePreInitTask_exit;
    }

nnePreInitTask_exit:
    return status;
}

/*!
 * Entry point for the Neural Net Engine (NNE) task. This task does not receive any parameters.
 */
lwrtosTASK_FUNCTION(task_nne, pvParameters)
{
    DISPATCH_NNE dispatch;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, nneInit)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        if (nnePostInit() != FLCN_OK)
        {
            PMU_HALT();
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    LWOS_TASK_LOOP(LWOS_QUEUE(PMU, NNE), &dispatch, status, PMU_TASK_CMD_BUFFER_PTRS_GET(NNE))
    {
        status = s_nneEventHandle(&dispatch);
    }
    LWOS_TASK_LOOP_END(status);
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief   Helper call handling events sent to Neural Net Engine (NNE) task.
 */
static FLCN_STATUS
s_nneEventHandle
(
    DISPATCH_NNE *pRequest
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
                case RM_PMU_UNIT_NNE:
                {
                    status = pmuRpcProcessUnitNne(&(pRequest->rpc));
                    if (status != FLCN_OK)
                    {
                        PMU_BREAKPOINT();
                        goto s_nneEventHandle_exit;
                    }
                    break;
                }
            }
            break;
        }

        case NNE_EVENT_ID_NNE_DESC_INFERENCE:
        {
            status = nneDescInferenceClientHandler(&(pRequest->inference));
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto s_nneEventHandle_exit;
            }

            break;
        }
    }

s_nneEventHandle_exit:
    return status;
}
