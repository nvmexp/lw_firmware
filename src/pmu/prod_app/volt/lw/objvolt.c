/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  objvolt.c
 * @brief Container-object for PMU VOLT Management routines.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "cmdmgmt/cmdmgmt.h"
#include "volt/objvolt.h"
#include "pmu_objpmu.h"
#include "boardobj/boardobjgrp.h"
#include "main.h"
#include "main_init_api.h"
#include "rmpmusupersurfif.h"
#include "volt/voltrail_pmumon.h"

#include "g_pmurpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Static Function Prototypes --------------------- */
/* ------------------------- Global Variables ------------------------------- */
OBJVOLT Volt;

/*!
 * @brief Array of VOLT RM_PMU_BOARDOBJ_CMD_GRP_*** handler entries.
 */
BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_ENTRY voltBoardObjGrpHandlers[]
    GCC_ATTRIB_SECTION(LW_ARCH_VAL("dmem_perf", "dmem_globalRodata"), "voltBoardObjGrpHandlers") =
{
#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA(VOLT, VOLT_RAIL,
        voltRailBoardObjGrpIfaceModel10Set, pascalAndLater.volt.voltRailGrpSet,
        voltRailBoardObjGrpIfaceModel10GetStatus, pascalAndLater.volt.voltRailGrpGetStatus),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL))

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_DEVICE))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA_SET_ONLY(VOLT, VOLT_DEVICE,
        voltDeviceBoardObjGrpIfaceModel10Set, pascalAndLater.volt.voltDeviceGrpSet),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_DEVICE))

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY))
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_AUTO_DMA(VOLT, VOLT_POLICY,
        voltPolicyBoardObjGrpIfaceModel10Set, pascalAndLater.volt.voltPolicyGrpSet,
        voltPolicyBoardObjGrpIfaceModel10GetStatus, pascalAndLater.volt.voltPolicyGrpGetStatus),
#endif // (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY))
};

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Construct the VOLT object.
 *
 * @return  FLCN_OK     On success
 */
FLCN_STATUS
constructVolt_IMPL(void)
{
    memset(&Volt, 0, sizeof(OBJVOLT));
    return FLCN_OK;
}

/*!
 * @brief Initialize RW semaphore for VOLT synchronization.
 */
FLCN_STATUS
voltPostInit_IMPL(void)
{
    FLCN_STATUS status = FLCN_OK;

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_SEMAPHORE))
    Volt.pVoltSemaphoreRW = voltSemaphoreRWInitialize();
    if (Volt.pVoltSemaphoreRW == NULL)
    {
        status = FLCN_ERR_NO_FREE_MEM;
        PMU_HALT();
        goto voltPostInit_IMPL_exit;
    }
#endif

    if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAILS_PMUMON))
    {
        status = voltVoltRailsPmumonConstruct();
        if (status != FLCN_OK)
        {
            goto voltPostInit_IMPL_exit;
        }
    }

voltPostInit_IMPL_exit:
    return status;
}

/*!
 * @brief   Exelwtes generic BOARD_OBJ_GRP_CMD RPC.
 *
 * @param[in/out]   pParams Pointer to RM_PMU_RPC_STRUCT_VOLT_BOARD_OBJ_GRP_CMD
 */
FLCN_STATUS
pmuRpcVoltBoardObjGrpCmd
(
    RM_PMU_RPC_STRUCT_VOLT_BOARD_OBJ_GRP_CMD *pParams,
    PMU_DMEM_BUFFER                          *pBuffer
)
{
    FLCN_STATUS status  = FLCN_ERR_FEATURE_NOT_ENABLED;

    // Note: pBuffer->size is validated in boardObjGrpIfaceModel10GetStatus_SUPER

    switch (pParams->commandId)
    {
        case RM_PMU_BOARDOBJGRP_CMD_SET:
        {
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBoardObj)
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVoltConstruct)
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPwmConstruct)
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVolt)
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                voltWriteSemaphoreTake();
                status = boardObjGrpIfaceModel10CmdHandlerDispatch(
                    pParams->classId,                               // classId
                    pBuffer,                                        // pBuffer
                    LW_ARRAY_ELEMENTS(voltBoardObjGrpHandlers),     // numEntries
                    voltBoardObjGrpHandlers,                        // pEntries
                    pParams->commandId);                            // commandId
                voltWriteSemaphoreGive();
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            break;
        }

        case RM_PMU_BOARDOBJGRP_CMD_GET_STATUS:
        {
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBoardObj)
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVolt)
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                voltReadSemaphoreTake();
                status = boardObjGrpIfaceModel10CmdHandlerDispatch(
                    pParams->classId,                               // classId
                    pBuffer,                                        // pBuffer
                    LW_ARRAY_ELEMENTS(voltBoardObjGrpHandlers),     // numEntries
                    voltBoardObjGrpHandlers,                        // pEntries
                    pParams->commandId);                            // commandId
                voltReadSemaphoreGive();
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
 * @brief Top-level command-handler function for all VOLT commands from within the PERF
 * task. Responsible for sending the acknowledgment for each command back to
 * the RM.
 *
 * @param[in]  pDispatch
 *      Dispatch structure containing the command to handle and information on
 *      the queue the command originated from.
 */
FLCN_STATUS
voltProcessRmCmdFromPerf_IMPL
(
    DISP2UNIT_RM_RPC *pDispatch
)
{
    FLCN_STATUS status;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVolt)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = pmuRpcProcessUnitVolt(pDispatch);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief   Exelwtes LOAD RPC.
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_VOLT_LOAD
 */
FLCN_STATUS
pmuRpcVoltLoad
(
    RM_PMU_RPC_STRUCT_VOLT_LOAD *pParams
)
{
    FLCN_STATUS status  = FLCN_OK;

    // Load all VOLT_RAILs.
    if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL))
    {
        status = voltRailsLoad();
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto pmuRpcVoltLoad_exit;
        }
    }

    // Load all VOLT_DEVICEs.
    if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_DEVICE))
    {
        status = voltDevicesLoad();
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto pmuRpcVoltLoad_exit;
        }
    }

    // Load all VOLT_POLICYs.
    if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY))
    {
        status = voltPoliciesLoad();
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto pmuRpcVoltLoad_exit;
        }
    }

    // PMU VOLT infrastructure is now loaded.
    Volt.bLoaded = LW_TRUE;

pmuRpcVoltLoad_exit:
    return status;
}

/*!
 * @brief Update dynamic parameters that depend on VFE. Dynamic parameters can change
 * at run-time so cache them for all VOLT objects via this interface.
 *
 * @note Callers must attach required VFE overlays before calling the function.
 */
FLCN_STATUS
voltProcessDynamilwpdate(void)
{
    FLCN_STATUS status = FLCN_OK;

    voltWriteSemaphoreTake();
    // Process dynamic update only if PMU VOLT infrastructure is loaded.
    if (PMU_VOLT_IS_LOADED())
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL))
        {
            status = voltRailsDynamilwpdate(LW_TRUE);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto voltProcessDynamilwpdate_exit;
            }
        }

        if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY))
        {
            status = voltPoliciesDynamilwpdate();
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto voltProcessDynamilwpdate_exit;
            }
        }
    }

voltProcessDynamilwpdate_exit:
    voltWriteSemaphoreGive();
    return status;
}

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_30))
/*!
 * @brief   Exelwtes VOLT_SET_VOLTAGE RPC.
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_VOLT_VOLT_SET_VOLTAGE
 */
FLCN_STATUS
pmuRpcVoltVoltSetVoltage
(
    RM_PMU_RPC_STRUCT_VOLT_VOLT_SET_VOLTAGE *pParams
)
{

    return voltVoltSetVoltage(pParams->clientID, &(pParams->railList));
}

/*!
 * @brief   Exelwtes VOLT_RAIL_GET_VOLTAGE RPC.
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_VOLT_VOLT_RAIL_GET_VOLTAGE
 */
FLCN_STATUS
pmuRpcVoltVoltRailGetVoltage
(
    RM_PMU_RPC_STRUCT_VOLT_VOLT_RAIL_GET_VOLTAGE *pParams
)
{
    FLCN_STATUS status = FLCN_OK;

    status = voltRailGetVoltage_RPC(pParams->railIdx, &(pParams->voltageuV));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @brief   Exelwtes VOLT_SANITY_CHECK RPC.
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_VOLT_VOLT_SANITY_CHECK
 */
FLCN_STATUS
pmuRpcVoltVoltSanityCheck
(
    RM_PMU_RPC_STRUCT_VOLT_VOLT_SANITY_CHECK *pParams
)
{
    FLCN_STATUS status = FLCN_OK;

    status = voltVoltSanityCheck(pParams->clientID, &(pParams->railList), LW_TRUE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}
#endif // PMUCFG_FEATURE_ENABLED(PMU_VOLT_30)

/*!
 * @brief   Sets requested target voltage and noise-unaware Vmin.
 *
 * @param[in]  clientID  ID of the client that wants to set the voltage
 * @param[in]  pRailList  Pointer that holds the list containing target voltage
 *                        for the VOLT_RAILs.
 * @return  FLCN_OK
 *      Requested voltage parameters were set successfully.
 * @return  Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
FLCN_STATUS
voltVoltSetVoltage
(
    LwU8                                clientID,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST    *pRailList
)
{
    FLCN_STATUS status;

    voltWriteSemaphoreTake();

    // Sanity check input data before taking any action.
    status = voltVoltSanityCheck(clientID, pRailList, LW_TRUE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltVoltSetVoltage_exit;
    }

    status = voltRailSetNoiseUnawareVmin(pRailList);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltVoltSetVoltage_exit;
    }

    status = voltPolicySetVoltage(clientID, pRailList);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltVoltSetVoltage_exit;
    }

voltVoltSetVoltage_exit:
    voltWriteSemaphoreGive();
    return status;
}

/*!
 * @brief Sanity checks voltage data on the one or more VOLT_RAILs.
 *
 * @param[in]  clientID
 *      ID of the client that wants to set the voltage data
 * @param[in]  pRailList
 *      Pointer that holds the list containing target voltage data for the VOLT_RAILs.
 * @param[in]  bCheckState
 *      Boolean that indicates if we need to sanity check rail state.
 *
 * @return  FLCN_ERR_ILWALID_ARGUMENT
 *      Invalid input parameters.
 * @return  FLCN_ERR_ILWALID_STATE
 *      PMU VOLT infrastructure isn't loaded.
 * @return  FLCN_OK
 *      Requested target voltages were sanitized successfully.
 * @return  Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
FLCN_STATUS
voltVoltSanityCheck_IMPL
(
    LwU8                                clientID,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST    *pRailList,
    LwBool                              bCheckState
)
{
    VOLT_POLICY    *pPolicy;
    FLCN_STATUS     status;
    LwU8            policyIdx;

    if (!PMU_VOLT_IS_LOADED())
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto voltVoltSanityCheck_IMPL_exit;
    }

    // Sanity check input rail list.
    status = voltRailSanityCheck(pRailList, bCheckState);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltVoltSanityCheck_IMPL_exit;
    }

    // Obtain VOLT_POLICY pointer from the Voltage Policy Table Index.
    policyIdx = voltPolicyClientColwertToIdx(clientID);
    pPolicy = VOLT_POLICY_GET(policyIdx);
    if (pPolicy == NULL)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        PMU_BREAKPOINT();
        goto voltVoltSanityCheck_IMPL_exit;
    }

    // Call VOLT_POLICYs SANITY_CHECK interface.
    status = voltPolicySanityCheck(pPolicy, pRailList->numRails,
                pRailList->rails);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltVoltSanityCheck_IMPL_exit;
    }

voltVoltSanityCheck_IMPL_exit:
    return status;
}

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_OFFSET))
/*!
 * @brief Compute the maximum positive and maximum negative voltage offset that can be
 * applied to a set of voltage rails.
 *
 * @param[in]  clientID
 *      ID of the client that wants to get the voltage offset information as defined in
 *      LW2080_CTRL_VOLT_VOLT_POLICY_CLIENT_<xyz>.
 * @param[in]  pRailList
 *      Pointer that holds the list containing target voltage data for the VOLT_RAILs.
 * @param[out]  pRailOffsetList
 *      Pointer that holds the list containing voltage offset for each voltage rail.
 * @param[in]   bSkipVoltRangeTrim
 *      Boolean tracking whether client explcitly requested to skip the
 *      voltage range trimming.
 *
 * @return  FLCN_OK
 *      Max positive and negative offset was computed successfully.
 * @return  Other unexpected errors
 *      Unexpected errors propagated from other functions.
 *
 * @note Callers of this API are expected to attach imem_perf and dmem_perf overlay
 *       since this interface uses functions which are contained in imem_perf overlay
 *       and data that is present in dmem_perf overlay.
 *       Example - @ref voltRailRoundVoltage().
 *
 */
FLCN_STATUS
voltVoltOffsetRangeGet
(
    LwU8                              clientID,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST  *pRailList,
    VOLT_RAIL_OFFSET_RANGE_LIST      *pRailOffsetList,
    LwBool                            bSkipVoltRangeTrim
)
{
    FLCN_STATUS status;

    // Perform sanity check on input rail list.
    status = voltVoltSanityCheck(clientID, pRailList, LW_FALSE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltVoltOffsetRangeGet_exit;
    }

    // Sanitize the output structure.
    memset(pRailOffsetList, 0, sizeof(VOLT_RAIL_OFFSET_RANGE_LIST));

    // Check for VOLT_RAIL specific constraints.
    status = voltRailOffsetRangeGet(pRailList, pRailOffsetList, bSkipVoltRangeTrim);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltVoltOffsetRangeGet_exit;
    }

    // Check for VOLT_POLICY specific constraints.
    status = voltPolicyOffsetRangeGet(clientID, pRailList, pRailOffsetList);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltVoltOffsetRangeGet_exit;
    }

voltVoltOffsetRangeGet_exit:
    return status;
}
#endif // PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_OFFSET)

/* ------------------------- Private Functions ------------------------------ */
