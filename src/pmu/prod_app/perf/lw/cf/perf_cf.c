/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perf_cf.c
 * @copydoc perf_cf.h
 */

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "cmdmgmt/cmdmgmt.h"
#include "perf/cf/perf_cf.h"
#include "pmu_objclk.h"
#include "rmpmusupersurfif.h"
#include "main_init_api.h"

#include "g_pmurpc.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/*!
 * Structure holding all PERF_CF related data.
 */
OBJPERF_CF PerfCf
    GCC_ATTRIB_SECTION("dmem_perfCf", "PerfCf");

/*!
 * Array of PERF_CF RM_PMU_BOARDOBJ_CMD_GRP_*** handler entries.
 */
BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_ENTRY         PerfCfBoardObjGrpHandlers[]
    GCC_ATTRIB_SECTION(LW_ARCH_VAL("dmem_perfCf", "dmem_globalRodata"), "PerfCfBoardObjGrpHandlers") =
{
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_SENSOR))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE(PERF_CF, SENSOR,
        perfCfSensorBoardObjGrpIfaceModel10Set,
        perfCfSensorBoardObjGrpIfaceModel10GetStatus),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_SENSOR))

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE(PERF_CF, PM_SENSOR,
        perfCfPmSensorBoardObjGrpIfaceModel10Set,
        perfCfPmSensorBoardObjGrpIfaceModel10GetStatus),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR))

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_TOPOLOGY))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE(PERF_CF, TOPOLOGY,
        perfCfTopologyBoardObjGrpIfaceModel10Set,
        perfCfTopologyBoardObjGrpIfaceModel10GetStatus),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_TOPOLOGY))

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER))
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLERS_1X)
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE(PERF_CF, CONTROLLER,
        perfCfControllerBoardObjGrpIfaceModel10Set_1X,
        perfCfControllerBoardObjGrpIfaceModel10GetStatus_1X),
#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLERS_1X)
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLERS_2X)
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE(PERF_CF, CONTROLLER,
        perfCfControllerBoardObjGrpIfaceModel10Set_2X,
        perfCfControllerBoardObjGrpIfaceModel10GetStatus_2X),
#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLERS_2X)
#endif // (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLIENT_PERF_CF_CONTROLLER))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE(PERF_CF, CLIENT_PERF_CF_CONTROLLER,
        perfCfClientPerfCfControllerBoardObjGrpIfaceModel10Set,
        perfCfClientPerfCfControllerBoardObjGrpIfaceModel10GetStatus),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLIENT_PERF_CF_CONTROLLER))

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_POLICY))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE(PERF_CF, POLICY,
        perfCfPolicyBoardObjGrpIfaceModel10Set,
        perfCfPolicyBoardObjGrpIfaceModel10GetStatus),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_POLICY))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLIENT_PERF_CF_POLICY))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE(PERF_CF, CLIENT_PERF_CF_POLICY,
        perfCfClientPerfCfPolicyBoardObjGrpIfaceModel10Set,
        perfCfClientPerfCfPolicyBoardObjGrpIfaceModel10GetStatus),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLIENT_PERF_CF_POLICY))

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE(PERF_CF, PWR_MODEL,
        perfCfPwrModelBoardObjGrpIfaceModel10Set,
        perfCfPwrModelBoardObjGrpIfaceModel10GetStatus),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLIENT_PERF_CF_PWR_MODEL_PROFILE))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_SET_ONLY(PERF_CF, CLIENT_PWR_MODEL_PROFILE,
        clientPerfCfPwrModelProfileBoardObjGrpIfaceModel10Set),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLIENT_PERF_CF_PWR_MODEL_PROFILE))
};

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief   PERF_CF engine constructor.
 *
 * @return  FLCN_OK     On success
 */
FLCN_STATUS
constructPerfCf(void)
{
    return FLCN_OK;
}

/*!
 * @brief   PERF_CF post-init function.
 */
FLCN_STATUS
perfCfPostInit(void)
{
    FLCN_STATUS     status        = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfCfInit)
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfCfTopology)
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfCfModel)
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfCfController)
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_PERF_LIMIT)
        OSTASK_OVL_DESC_DEFINE_PERF_PERF_LIMITS_CLIENT_INIT
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfCfLimitClient)
#endif
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        PerfCf.bLoaded = LW_FALSE;

        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_SENSOR))
        {
            status = perfCfSensorsPostInit(PERF_CF_GET_SENSORS());
            if (status != FLCN_OK)
            {
                goto perfPerfCfPostInit_exit;
            }
        }
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR)
            status = perfCfPmSensorsPostInit(PERF_CF_PM_GET_SENSORS());
            if (status != FLCN_OK)
            {
                goto perfPerfCfPostInit_exit;
            }
#endif
        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_TOPOLOGY))
        {
            status = perfCfTopologysPostInit(PERF_CF_GET_TOPOLOGYS());
            if (status != FLCN_OK)
            {
                goto perfPerfCfPostInit_exit;
            }
        }

        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER))
        {
            status = perfCfControllersPostInit();
            if (status != FLCN_OK)
            {
                goto perfPerfCfPostInit_exit;
            }
        }

        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_POLICY))
        {
            status = perfCfPolicysPostInit(PERF_CF_GET_POLICYS());
            if (status != FLCN_OK)
            {
                goto perfPerfCfPostInit_exit;
            }
        }
    }
perfPerfCfPostInit_exit:
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief   Exelwtes generic PERF_CF_LOAD RPC.
 *
 * @param[in/out]   pParams Pointer to RM_PMU_RPC_STRUCT_PERF_CF_LOAD
 */
FLCN_STATUS
pmuRpcPerfCfLoad
(
    RM_PMU_RPC_STRUCT_PERF_CF_LOAD *pParams
)
{
    FLCN_STATUS     status        = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_PERF_CF_BASE
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        OBJPERF_CF *pPerfCf = PERF_CF_GET_OBJ();
        //
        // NJ-TODO: If request matches current state simply ignore it. Once we
        // assure that the caller issues just a single load() we should upgrade
        // this to failure with error code and breakpoint.
        //
        LwBool bLoad = (!!pParams->bLoad);

        if (bLoad == pPerfCf->bLoaded)
        {
            goto pmuRpcPerfPerfCfLoad_exit;
        }

        if (bLoad)
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_SENSOR))
            {
                status = perfCfSensorsLoad(PERF_CF_GET_SENSORS(), bLoad);
                if (status != FLCN_OK)
                {
                    goto pmuRpcPerfPerfCfLoad_exit;
                }
            }

            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR))
            {
                status = perfCfPmSensorsLoad(PERF_CF_PM_GET_SENSORS(), bLoad);
                if (status != FLCN_OK)
                {
                    goto pmuRpcPerfPerfCfLoad_exit;
                }
            }

            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_TOPOLOGY))
            {
                status = perfCfTopologysLoad(PERF_CF_GET_TOPOLOGYS(), bLoad);
                if (status != FLCN_OK)
                {
                    goto pmuRpcPerfPerfCfLoad_exit;
                }
            }

            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL))
            {
                status = perfCfPwrModelsLoad(PERF_CF_GET_PWR_MODELS(), bLoad);
                if (status != FLCN_OK)
                {
                    goto pmuRpcPerfPerfCfLoad_exit;
                }
            }

            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER))
            {
                status = perfCfControllersLoad(PERF_CF_GET_CONTROLLERS(), bLoad);
                if (status != FLCN_OK)
                {
                    goto pmuRpcPerfPerfCfLoad_exit;
                }
            }

            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_POLICY))
            {
                status = perfCfPolicysLoad(PERF_CF_GET_POLICYS(), bLoad);
                if (status != FLCN_OK)
                {
                    goto pmuRpcPerfPerfCfLoad_exit;
                }
            }
        }
        else
        {
            FLCN_STATUS tmpStatus;

            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_POLICY))
            {
                tmpStatus = perfCfPolicysLoad(PERF_CF_GET_POLICYS(), bLoad);
                if (status == FLCN_OK)
                {
                    status = tmpStatus;
                }
            }

            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER))
            {
                tmpStatus = perfCfControllersLoad(PERF_CF_GET_CONTROLLERS(), bLoad);
                if (status == FLCN_OK)
                {
                    status = tmpStatus;
                }
            }

            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL))
            {
                tmpStatus = perfCfPwrModelsLoad(PERF_CF_GET_PWR_MODELS(), bLoad);
                if (status == FLCN_OK)
                {
                    status = tmpStatus;
                }
            }

            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_TOPOLOGY))
            {
                tmpStatus = perfCfTopologysLoad(PERF_CF_GET_TOPOLOGYS(), bLoad);
                if (status == FLCN_OK)
                {
                    status = tmpStatus;
                }
            }

            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_SENSOR))
            {
                tmpStatus = perfCfSensorsLoad(PERF_CF_GET_SENSORS(), bLoad);
                if (status == FLCN_OK)
                {
                    status = tmpStatus;
                }
            }

            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR))
            {
                tmpStatus = perfCfPmSensorsLoad(PERF_CF_PM_GET_SENSORS(), bLoad);
                if (status == FLCN_OK)
                {
                    status = tmpStatus;
                }
            }
        }

        pPerfCf->bLoaded = bLoad;
    }
pmuRpcPerfPerfCfLoad_exit:
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    return status;
}

/*!
 * @brief   Exelwtes generic PERF_CF_OPTP_UPDATE RPC.
 *
 * @param[in/out]   pParams Pointer to RM_PMU_RPC_STRUCT_PERF_CF_OPTP_UPDATE
 */
FLCN_STATUS
pmuRpcPerfCfOptpUpdate
(
    RM_PMU_RPC_STRUCT_PERF_CF_OPTP_UPDATE *pParams
)
{
    FLCN_STATUS     status        = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfCfController)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = perfCfControllersOptpUpdate(PERF_CF_GET_CONTROLLERS(), pParams->perfRatio);
        if (status != FLCN_OK)
        {
            goto pmuRpcPerfPerfCfOptpUpdate_exit;
        }
    }
pmuRpcPerfPerfCfOptpUpdate_exit:
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    return status;
}

/*!
 * @brief   Exelwtes generic BOARD_OBJ_GRP_CMD RPC.
 *
 * @param[in/out]   pParams Pointer to RM_PMU_RPC_STRUCT_PERF_CF_BOARD_OBJ_GRP_CMD
 */
FLCN_STATUS
pmuRpcPerfCfBoardObjGrpCmd
(
    RM_PMU_RPC_STRUCT_PERF_CF_BOARD_OBJ_GRP_CMD *pParams,
    PMU_DMEM_BUFFER                             *pBuffer
)
{
    FLCN_STATUS status = FLCN_ERR_FEATURE_NOT_ENABLED;

    // Note: pBuffer->size is validated in boardObjGrpIfaceModel10GetStatus_SUPER

    switch (pParams->commandId)
    {
        case RM_PMU_BOARDOBJGRP_CMD_SET:
        {
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_DEFINE_PERF_CF_BOARD_OBJ_COMMON
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                status = boardObjGrpIfaceModel10CmdHandlerDispatch(
                    pParams->classId,                               // classId
                    pBuffer,                                        // pBuffer
                    LW_ARRAY_ELEMENTS(PerfCfBoardObjGrpHandlers),   // numEntries
                    PerfCfBoardObjGrpHandlers,                      // pEntries
                    pParams->commandId);                            // commandId
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            break;
        }

        case RM_PMU_BOARDOBJGRP_CMD_GET_STATUS:
        {
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_DEFINE_PERF_CF_BOARD_OBJ_COMMON
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                status = boardObjGrpIfaceModel10CmdHandlerDispatch(
                    pParams->classId,                               // classId
                    pBuffer,                                        // pBuffer
                    LW_ARRAY_ELEMENTS(PerfCfBoardObjGrpHandlers),   // numEntries
                    PerfCfBoardObjGrpHandlers,                      // pEntries
                    pParams->commandId);                            // commandId
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            break;
        }

        default:
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            break;
        }
    }

    return status;
}

/* ------------------------ Private Functions ------------------------------- */

