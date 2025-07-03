/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   clkrmcmd.c
 * @brief  OBJCLK Interfaces for RM/PMU Communication
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "cmdmgmt/cmdmgmt.h"
#include "pmu_objclk.h"
#include "pmu_objpg.h"
#include "pmu_objlpwr.h"
#include "pmu_objperf.h"

#include "pmu_objpmu.h"
#include "boardobj/boardobjgrp.h"
#include "rmpmusupersurfif.h"

#include "g_pmurpc.h"
#include "cmdmgmt/cmdmgmt.h"
#include "g_pmurmifrpc.h"

/* ------------------------- Static Function Prototypes --------------------- */
/* ------------------------- Global Variables ------------------------------- */
/*!
 * Array of CLK RM_PMU_BOARDOBJ_CMD_GRP_*** handler entries.
 */
BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_ENTRY s_clkBoardObjGrpHandlers[]
    GCC_ATTRIB_SECTION(LW_ARCH_VAL("dmem_perf", "dmem_globalRodata"), "s_clkBoardObjGrpHandlers") =
{
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA_SET_ONLY(CLK, CLK_DOMAIN,
        clkDomainBoardObjGrpIfaceModel10Set, pascalAndLater.clk.clkDomainGrpSet),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROG))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA(CLK, CLK_PROG,
        clkProgBoardObjGrpIfaceModel10Set, pascalAndLater.clk.clkProgGrpSet,
        clkProgBoardObjGrpIfaceModel10GetStatus,  pascalAndLater.clk.clkProgGrpGetStatus),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROG))

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICES))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA(CLK, ADC_DEVICE,
        clkAdcDevBoardObjGrpIfaceModel10Set, pascalAndLater.clk.clkAdcDeviceGrpSet,
        clkAdcDevicesBoardObjGrpIfaceModel10GetStatus, pascalAndLater.clk.clkAdcDeviceGrpGetStatus),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICES))

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_DEVICES))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA(CLK, NAFLL_DEVICE,
        clkNafllDevBoardObjGrpIfaceModel10Set, pascalAndLater.clk.clkNafllDeviceGrpSet,
        clkNafllDevicesBoardObjGrpIfaceModel10GetStatus, pascalAndLater.clk.clkNafllDeviceGrpGetStatus),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_DEVICES))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE(CLK, CLK_VF_POINT,
        clkVfPointBoardObjGrpIfaceModel10Set,
        clkVfPointBoardObjGrpIfaceModel10GetStatus),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLIENT_CLK_VF_POINT))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA(CLK, CLIENT_CLK_VF_POINT,
        clientClkVfPointBoardObjGrpIfaceModel10Set, ga10xAndLater.clk.clientClkVfPointGrpSet,
        clientClkVfPointBoardObjGrpIfaceModel10GetStatus, ga10xAndLater.clk.clientClkVfPointGrpGetStatus),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLIENT_CLK_VF_POINT))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA(CLK, CLK_FREQ_CONTROLLER,
        clkFreqControllerBoardObjGrpIfaceModel10Set, pascalAndLater.clk.clkFreqControllerGrpSet,
        clkFreqControllerBoardObjGrpIfaceModel10GetStatus, pascalAndLater.clk.clkFreqControllerGrpGetStatus),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VOLT_CONTROLLER))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA(CLK, CLK_VOLT_CONTROLLER,
        clkVoltControllerBoardObjGrpIfaceModel10Set, turingAndLater.clk.clkVoltControllerGrpSet,
        clkVoltControllerBoardObjGrpIfaceModel10GetStatus, turingAndLater.clk.clkVoltControllerGrpGetStatus),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VOLT_CONTROLLER))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_PLL_DEVICE))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA_SET_ONLY(CLK, PLL_DEVICE,
        clkPllDevBoardObjGrpIfaceModel10Set, pascalAndLater.clk.clkPllDeviceGrpSet),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_PLL_DEVICE))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLKS_ON_PMU))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA(CLK, FREQ_DOMAIN,
        clkFreqDomainBoardObjGrpIfaceModel10Set, pascalAndLater.clk.clkFreqDomainGrpSet,
        clkFreqDomainBoardObjGrpIfaceModel10GetStatus, pascalAndLater.clk.clkFreqDomainGrpGetStatus),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLKS_ON_PMU))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_ENUM))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA_SET_ONLY(CLK, CLK_ENUM,
        clkEnumBoardObjGrpIfaceModel10Set, ga10xAndLater.clk.clkEnumGrpSet),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_ENUM))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_REL))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA(CLK, CLK_VF_REL,
        clkVfRelBoardObjGrpIfaceModel10Set, ga10xAndLater.clk.clkVfRelGrpSet,
        clkVfRelBoardObjGrpIfaceModel10GetStatus, ga10xAndLater.clk.clkVfRelGrpGetStatus),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_REL))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROP_REGIME))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA_SET_ONLY(CLK, CLK_PROP_REGIME,
        clkPropRegimeBoardObjGrpIfaceModel10Set, ga10xAndLater.clk.clkPropRegimeGrpSet),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROP_REGIME))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROP_TOP))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA(CLK, CLK_PROP_TOP,
        clkPropTopBoardObjGrpIfaceModel10Set, ga10xAndLater.clk.clkPropTopGrpSet,
        clkPropTopBoardObjGrpIfaceModel10GetStatus, ga10xAndLater.clk.clkPropTopGrpGetStatus),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROP_TOP))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROP_TOP_REL))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA_SET_ONLY(CLK, CLK_PROP_TOP_REL,
        clkPropTopRelBoardObjGrpIfaceModel10Set, ga10xAndLater.clk.clkPropTopRelGrpSet),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLIENT_CLK_PROP_TOP_POL))

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLIENT_CLK_PROP_TOP_POL))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA_SET_ONLY(CLK, CLIENT_CLK_PROP_TOP_POL,
        clkClientClkPropTopPolBoardObjGrpIfaceModel10Set, ga10xAndLater.clk.clkClientClkPropTopPolGrpSet),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROP_TOP_REL))
};

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Exelwtes generic BOARD_OBJ_GRP_CMD RPC.
 *
 * @param[in/out]   pParams Pointer to RM_PMU_RPC_STRUCT_THERM_BOARD_OBJ_GRP_CMD
 * @param[in]       pBuffer Scratch buffer address and size structure
 */
FLCN_STATUS
pmuRpcClkBoardObjGrpCmd
(
    RM_PMU_RPC_STRUCT_CLK_BOARD_OBJ_GRP_CMD *pParams,
    PMU_DMEM_BUFFER                         *pBuffer
)
{
    FLCN_STATUS status = FLCN_ERR_FEATURE_NOT_ENABLED;

    // Note: pBuffer->size is validated in boardObjGrpIfaceModel10GetStatus_SUPER

    switch (pParams->commandId)
    {
        case RM_PMU_BOARDOBJGRP_CMD_SET:
        {
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClkConstruct)
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                //
                // This code update the Frequency delta which may change the VF
                // could and thus potentilally break the VF look up funcitons.
                // Thus synchronize this change with the use if semaphore.
                //
                perfWriteSemaphoreTake();
                status = boardObjGrpIfaceModel10CmdHandlerDispatch(
                    pParams->classId,                           // classId
                    pBuffer,                                    // pBuffer
                    LW_ARRAY_ELEMENTS(s_clkBoardObjGrpHandlers),// numEntries
                    s_clkBoardObjGrpHandlers,                   // pEntries
                    pParams->commandId);                        // commandId
                perfWriteSemaphoreGive();
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            break;
        }

        case RM_PMU_BOARDOBJGRP_CMD_GET_STATUS:
        {
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClkStatus)
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                status = boardObjGrpIfaceModel10CmdHandlerDispatch(
                    pParams->classId,                           // classId
                    pBuffer,                                    // pBuffer
                    LW_ARRAY_ELEMENTS(s_clkBoardObjGrpHandlers),// numEntries
                    s_clkBoardObjGrpHandlers,                   // pEntries
                    pParams->commandId);                        // commandId
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            break;
        }

        default:
        {
            status = FLCN_ERR_ILWALID_INDEX;
            break;
        }
    }

    return status;
}

/*!
 * @brief   Execute CLK_CNTR_SAMPLE_DOMAIN RPC.
 */
FLCN_STATUS
pmuRpcClkCntrSampleDomain
(
    RM_PMU_RPC_STRUCT_CLK_CNTR_SAMPLE_DOMAIN *pParams
)
{
    FLCN_STATUS status;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_CLK_COUNTER
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = clkCntrSample(&pParams->sample);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief   Execute CLK_VF_CHANGE_INJECT RPC.
 */
FLCN_STATUS
pmuRpcClkVfChangeInject
(
    RM_PMU_RPC_STRUCT_CLK_VF_CHANGE_INJECT *pParams
)
{
    FLCN_STATUS status;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfClkAvfs)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVolt)
#if PMUCFG_FEATURE_ENABLED(PMU_PG_PMU_ONLY)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLpwr)
#endif
    };

    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
    {
        if (pParams->clkList.numDomains > RM_PMU_VF_INJECT_MAX_CLOCK_DOMAINS)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto pmuRpcClkVfChangeInject_exit;
        }
        if (pParams->voltList.numRails > RM_PMU_VF_INJECT_MAX_VOLT_RAILS)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto pmuRpcClkVfChangeInject_exit;
        }
    }

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_MS))
        {
            lpwrGrpDisallowExt(RM_PMU_LPWR_GRP_CTRL_ID_MS);
        }

        status = clkVfChangeInject(&pParams->clkList, &pParams->voltList);

        if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_MS))
        {
            lpwrGrpAllowExt(RM_PMU_LPWR_GRP_CTRL_ID_MS);
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

pmuRpcClkVfChangeInject_exit:
    return status;
}

/*!
 * @brief   Execute CLK_FREQ_EFFECTIVE_AVG RPC.
 */
FLCN_STATUS
pmuRpcClkFreqEffectiveAvg
(
    RM_PMU_RPC_STRUCT_CLK_FREQ_EFFECTIVE_AVG *pParams
)
{
    return clkFreqEffAvgGet(pParams->clkDomainMask, pParams->freqkHz);
}

/*!
 * @brief   Execute CLK_LOAD RPC.
 *
 * TODO: Break CLK_LOAD into individual CLK_LOAD_FEATURE RPCs
 */
FLCN_STATUS
pmuRpcClkLoad
(
    RM_PMU_RPC_STRUCT_CLK_LOAD *pParams
)
{
    FLCN_STATUS      status   = FLCN_OK;
    RM_PMU_CLK_LOAD *pClkLoad = &pParams->clkLoad;

    switch (pClkLoad->feature)
    {
        case LW_RM_PMU_CLK_LOAD_FEATURE_NAFLL:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_AVFS))
            {
                OSTASK_OVL_DESC ovlDescList[] = {
                    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfClkAvfs)
                };

                OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
                {
                    status = clkNafllsLoad(pClkLoad->actionMask);
                }
                OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            }
            break;
        }
        case LW_RM_PMU_CLK_LOAD_FEATURE_ADC:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_AVFS))
            {
                OSTASK_OVL_DESC ovlDescList[] = {
                    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfClkAvfs)
                };

                OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
                {
                    status = clkAdcsLoad(pClkLoad->actionMask);
                }
                OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            }
            break;
        }
        case LW_RM_PMU_CLK_LOAD_FEATURE_CLK_CONTROLLER:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_CONTROLLER))
            {
                OSTASK_OVL_DESC ovlDescList[] = {
                    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClkController)
                };

                OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
                {
                    status = clkClkControllersLoad(pClkLoad);
                }
                OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            }
            break;
        }
        case LW_RM_PMU_CLK_LOAD_FEATURE_FREQ_CONTROLLER:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_V10))
            {
                OSTASK_OVL_DESC ovlDescList[] = {
                    OSTASK_OVL_DESC_DEFINE_LIB_CLK_FREQ_CONTROLLER
                };

                OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
                {
                    status = clkFreqControllersLoad_10(pClkLoad);
                }
                OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            }
            break;
        }
        case LW_RM_PMU_CLK_LOAD_FEATURE_FREQ_EFFECTIVE_AVG:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_EFFECTIVE_AVG))
            {
                OSTASK_OVL_DESC ovlDescList[] = {
                    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClkEffAvg)
                };

                OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
                {
                    status = clkFreqEffAvgLoad(pClkLoad->actionMask);
                }
                OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            }
            break;
        }
        case LW_RM_PMU_CLK_LOAD_FEATURE_CLK_DOMAIN:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN))
            {
                perfWriteSemaphoreTake();
                {
                    OSTASK_OVL_DESC ovlDescList[] = {
                        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, vfLoad)
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_SEC))
                        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clkVfPointSec)
#endif
                    };

                    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
                    {
                        status = clkDomainsLoad();
                    }
                    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
                }
                perfWriteSemaphoreGive();
            }
            break;
        }
        default:
        {
            status = FLCN_ERROR;
            PMU_BREAKPOINT();
            break;
        }
    }

    return status;
}

/*!
 * @brief   RPC for MCLK Switch request from RM. This should be deferred to PERF_DAEMON
 *
 * @param[in,out]   pParams Pointer to the mclk switch request structure.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *      Invalid argument(s) are passed as input.
 * @return FLCN_OK
 *      Successfully deferred the mclk switch request to PERF_DAEMON task
 */
FLCN_STATUS
pmuRpcClkMclkSwitch
(
    RM_PMU_RPC_STRUCT_CLK_MCLK_SWITCH *pParams
)
{
    DISPATCH_PERF_DAEMON    disp2perfDaemon;
    FLCN_STATUS             status = FLCN_OK;

    // Queue the change request to PERF_DAEMON task
    disp2perfDaemon.hdr.eventType = PERF_DAEMON_EVENT_ID_PERF_MCLK_SWITCH;

    disp2perfDaemon.mclkSwitch.mclkSwitchRequest.targetFreqKHz = pParams->mClkSwitch.targetFreqKHz;
    disp2perfDaemon.mclkSwitch.mclkSwitchRequest.flags         = pParams->mClkSwitch.flags;
    disp2perfDaemon.mclkSwitch.mclkSwitchRequest.bAsync        = pParams->mClkSwitch.bAsync;

    status = lwrtosQueueIdSendBlocking(LWOS_QUEUE_ID(PMU, PERF_DAEMON),
                                &disp2perfDaemon, sizeof(disp2perfDaemon));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}
