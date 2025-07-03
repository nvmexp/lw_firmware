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
 * @file  voltpolicy.c
 * @brief Volt Policy Model
 *
 * This module is a collection of functions managing and manipulating state
 * related to volt policies.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "volt/objvolt.h"
#include "volt/voltpolicy.h"
#include "pmu_objpmgr.h"
#include "pmgr/pwrdev.h"
#include "main.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static void  s_voltPolicyVoltageChangeNotify(void);

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/*!
 * @brief   Array of all vtables pertaining to different VOLT_POLICY object types.
 */
BOARDOBJ_VIRTUAL_TABLE *VoltPolicyVirtualTables[LW2080_CTRL_BOARDOBJ_TYPE(VOLT, VOLT_POLICY, MAX)] =
{
    [LW2080_CTRL_BOARDOBJ_TYPE(VOLT, VOLT_POLICY, BASE)                  ] = NULL,
    [LW2080_CTRL_BOARDOBJ_TYPE(VOLT, VOLT_POLICY, SINGLE_RAIL)           ] = VOLT_VOLT_POLICY_SINGLE_RAIL_VTABLE(),
    [LW2080_CTRL_BOARDOBJ_TYPE(VOLT, VOLT_POLICY, SPLIT_RAIL_MULTI_STEP) ] = NULL,
    [LW2080_CTRL_BOARDOBJ_TYPE(VOLT, VOLT_POLICY, SPLIT_RAIL_SINGLE_STEP)] = NULL,
    [LW2080_CTRL_BOARDOBJ_TYPE(VOLT, VOLT_POLICY, SINGLE_RAIL_MULTI_STEP)] = NULL,
    [LW2080_CTRL_BOARDOBJ_TYPE(VOLT, VOLT_POLICY, SPLIT_RAIL)            ] = NULL,
};

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief VOLT_POLICY handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler()
 */
FLCN_STATUS
voltPolicyBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    return BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E32,  // _grpType
        VOLT_POLICY,                            // _class
        pBuffer,                                // _pBuffer
        voltIfaceModel10SetVoltPolicyHdr,             // _hdrFunc
        voltVoltPolicyIfaceModel10SetEntry,           // _entryFunc
        pascalAndLater.volt.voltPolicyGrpSet,   // _ssElement
        OVL_INDEX_DMEM(perf),                   // _ovlIdxDmem
        VoltPolicyVirtualTables);               // _ppObjectVtables
}

/*!
 * @brief Super-class implementation
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
voltPolicyGrpIfaceModel10ObjSetImpl_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    return boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
}

/*!
 * @copydoc BoardObjGrpIfaceModel10SetHeader()
 */
FLCN_STATUS
voltIfaceModel10SetVoltPolicyHdr
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    RM_PMU_VOLT_VOLT_POLICY_BOARDOBJGRP_SET_HEADER     *pSetHdr;
    FLCN_STATUS                                         status;

    // Call to Board Object Group constructor must always be first!
    status = boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc);
    if (status != FLCN_OK)
    {
        goto voltIfaceModel10SetVoltPolicyHdr_exit;
    }

    pSetHdr = (RM_PMU_VOLT_VOLT_POLICY_BOARDOBJGRP_SET_HEADER *)pHdrDesc;

    Volt.policyMetadata.perfCoreVFSeqPolicyIdx = pSetHdr->perfCoreVFSeqPolicyIdx;

voltIfaceModel10SetVoltPolicyHdr_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry()
 */
FLCN_STATUS
voltVoltPolicyIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    // Can be made static.
    switch (pBuf->type)
    {
        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_SINGLE_RAIL))
            {
                status = voltPolicyGrpIfaceModel10ObjSetImpl_SINGLE_RAIL(pModel10, ppBoardObj,
                        sizeof(VOLT_POLICY_SINGLE_RAIL), pBuf);
            }
            break;
        }

        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL_MULTI_STEP:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_SINGLE_RAIL_MULTI_STEP))
            {
                status = voltPolicyGrpIfaceModel10ObjSetImpl_SINGLE_RAIL_MULTI_STEP(pModel10, ppBoardObj,
                        sizeof(VOLT_POLICY_SINGLE_RAIL_MULTI_STEP), pBuf);
            }
            break;
        }

        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SPLIT_RAIL_MULTI_STEP:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_SPLIT_RAIL_MULTI_STEP))
            {
                status = voltPolicyGrpIfaceModel10ObjSetImpl_SPLIT_RAIL_MULTI_STEP(pModel10, ppBoardObj,
                        sizeof(VOLT_POLICY_SPLIT_RAIL_MULTI_STEP), pBuf);
            }
            break;
        }

        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SPLIT_RAIL_SINGLE_STEP:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_SPLIT_RAIL_SINGLE_STEP))
            {
                status = voltPolicyGrpIfaceModel10ObjSetImpl_SPLIT_RAIL_SINGLE_STEP(pModel10, ppBoardObj,
                        sizeof(VOLT_POLICY_SPLIT_RAIL_SINGLE_STEP), pBuf);
            }
            break;
        }

        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_MULTI_RAIL:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_MULTI_RAIL))
            {
                status = voltPolicyGrpIfaceModel10ObjSetImpl_MULTI_RAIL(pModel10, ppBoardObj,
                        sizeof(VOLT_POLICY_MULTI_RAIL), pBuf);
            }
            break;
        }

        default:
        {
            // To pacify coverity.
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus()
 */
FLCN_STATUS
voltPolicyIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_VOLT_VOLT_POLICY_BOARDOBJ_GET_STATUS    *pGetStatus;
    VOLT_POLICY                                    *pPolicy;
    FLCN_STATUS                                     status;

    status = boardObjIfaceModel10GetStatus(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto voltPolicyIfaceModel10GetStatus_exit;
    }
    pPolicy     = (VOLT_POLICY *)pBoardObj;
    pGetStatus  = (RM_PMU_VOLT_VOLT_POLICY_BOARDOBJ_GET_STATUS *)pPayload;

    // Set VOLT_POLICY query parameters.
#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_OFFSET))
    pGetStatus->offsetVoltRequV     = pPolicy->offsetVoltRequV;
    pGetStatus->offsetVoltLwrruV    = pPolicy->offsetVoltLwrruV;
#else
    // Mark pPolicy void to fix unused variable error.
    (void)pPolicy;

    pGetStatus->offsetVoltRequV     = RM_PMU_VOLT_VALUE_0V_IN_UV;
    pGetStatus->offsetVoltLwrruV    = RM_PMU_VOLT_VALUE_0V_IN_UV;
#endif // PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_OFFSET)

voltPolicyIfaceModel10GetStatus_exit:
    return status;
}

/*!
 * @brief Queries all VOLT_POLICIES.
 */
FLCN_STATUS
voltPolicyBoardObjGrpIfaceModel10GetStatus
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    return BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS_AUTO_DMA(E32,         // _grpType
        VOLT_POLICY,                                    // _class
        pBuffer,                                        // _pBuffer
        NULL,                                           // _hdrFunc
        voltVoltPolicyIfaceModel10GetStatus,                        // _entryFunc
        pascalAndLater.volt.voltPolicyGrpGetStatus);    // _ssElement
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus()
 */
FLCN_STATUS
voltVoltPolicyIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pBuf
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pBoardObj))
    {
        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_SINGLE_RAIL))
            {
                status = voltPolicyIfaceModel10GetStatus_SINGLE_RAIL(pModel10, pBuf);
            }
            break;
        }

        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL_MULTI_STEP:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_SINGLE_RAIL_MULTI_STEP))
            {
                status = voltPolicyIfaceModel10GetStatus_SINGLE_RAIL_MULTI_STEP(pModel10, pBuf);
            }
            break;
        }

        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SPLIT_RAIL_MULTI_STEP:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_SPLIT_RAIL_MULTI_STEP))
            {
                status = voltPolicyIfaceModel10GetStatus_SPLIT_RAIL(pModel10, pBuf);
            }
            break;
        }

        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SPLIT_RAIL_SINGLE_STEP:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_SPLIT_RAIL_SINGLE_STEP))
            {
                status = voltPolicyIfaceModel10GetStatus_SPLIT_RAIL(pModel10, pBuf);
            }
            break;
        }

        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_MULTI_RAIL:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_MULTI_RAIL))
            {
                status = voltPolicyIfaceModel10GetStatus_MULTI_RAIL(pModel10, pBuf);
            }
            break;
        }

        default:
        {
            // To pacify coverity.
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @brief Load all VOLT_POLICYs.
 *
 * @return  FLCN_OK
 *      All VOLT_POLICYs were loaded successfully.
 * @return  Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
FLCN_STATUS
voltPoliciesLoad(void)
{
    FLCN_STATUS     status = FLCN_OK;
    VOLT_POLICY    *pPolicy;
    LwBoardObjIdx   i;

    BOARDOBJGRP_ITERATOR_BEGIN(VOLT_POLICY, pPolicy, i, NULL)
    {
        status = voltPolicyLoad(pPolicy);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto voltPoliciesLoad_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

voltPoliciesLoad_exit:
    return status;
}

/*!
 * @brief Sets voltage on the one or more VOLT_RAILs according to the
 * appropriate VOLT_POLICY.
 *
 * @param[in]  clientID
 *      ID of the client that wants to set the voltage
 * @param[in]  pRailList
 *      Pointer that holds the list containing target voltage for the VOLT_RAILs.
 *
 * @return  FLCN_ERR_ILWALID_ARGUMENT
 *      Invalid input parameters.
 * @return  FLCN_ERR_ILWALID_STATE
 *      PMU VOLT infrastructure isn't loaded.
 * @return  FLCN_OK
 *      Requested target voltages were set successfully.
 * @return  Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
FLCN_STATUS
voltPolicySetVoltage_IMPL
(
    LwU8                                clientID,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST    *pRailList
)
{
    VOLT_POLICY    *pPolicy;
    FLCN_STATUS     status;

    // Obtain VOLT_POLICY pointer from the Voltage Policy Table Index.
    pPolicy = VOLT_POLICY_GET(voltPolicyClientColwertToIdx(clientID));
    if (pPolicy == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto voltPolicySetVoltage_IMPL_exit;
    }

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_OFFSET))
    // Reset the policy offset.
    pPolicy->offsetVoltRequV = RM_PMU_VOLT_VALUE_0V_IN_UV_SIGNED;
#endif // PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_OFFSET)

    // Call VOLT_POLICYs SET_VOLTAGE interface.
    status = voltPolicySetVoltageInternal(pPolicy, pRailList->numRails,
                pRailList->rails);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltPolicySetVoltage_IMPL_exit;
    }

voltPolicySetVoltage_IMPL_exit:
    return status;
}

/*!
 * @brief Update VOLT_POLICY dynamic parameters that depend on VFE.
 *
 * @return  FLCN_OK
 *      Dynamic update successfully completed.
 * @return  Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
FLCN_STATUS
voltPoliciesDynamilwpdate_IMPL(void)
{
    FLCN_STATUS     status = FLCN_OK;
    VOLT_POLICY    *pPolicy;
    LwBoardObjIdx   i;

    BOARDOBJGRP_ITERATOR_BEGIN(VOLT_POLICY, pPolicy, i, NULL)
    {
        // Call VOLT_POLICYs DYNAMIC_UPDATE interface.
        status = voltPolicyDynamilwpdate(pPolicy);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto voltPoliciesDynamilwpdate_IMPL_exit;
        }

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_OFFSET))
        // Re-callwlate the policy offset and apply it.
        if (pPolicy->offsetVoltRequV != RM_PMU_VOLT_VALUE_0V_IN_UV_SIGNED)
        {
            status = voltPolicyOffsetVoltageInternal(pPolicy);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto voltPoliciesDynamilwpdate_IMPL_exit;
            }
        }
#endif // PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_OFFSET)
    }
    BOARDOBJGRP_ITERATOR_END;

voltPoliciesDynamilwpdate_IMPL_exit:
    return status;
}

#if PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_OFFSET)
/*!
 * @brief Offsets voltage on the one or more VOLT_RAILs according to the
 * appropriate VOLT_POLICY.
 *
 * @param[in]  policyIdx    Voltage Policy Index
 * @param[in]  offsetVoltuV Voltage Offset
 *
 * @return  FLCN_ERR_ILWALID_ARGUMENT
 *      Invalid input parameters.
 * @return  FLCN_ERR_ILWALID_STATE
 *      PMU VOLT infrastructure isn't loaded.
 * @return  FLCN_OK
 *      Operation completed successfully.
 * @return  Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
FLCN_STATUS
voltPolicyOffsetVoltage
(
    LwU8    policyIdx,
    LwS32   offsetVoltuV
)
{
    VOLT_POLICY    *pPolicy;
    FLCN_STATUS     status;

    if (!PMU_VOLT_IS_LOADED())
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto voltPolicyOffsetVoltage_exit;
    }

    // Obtain VOLT_POLICY pointer.
    pPolicy = VOLT_POLICY_GET(policyIdx);
    if (pPolicy == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto voltPolicyOffsetVoltage_exit;
    }

    // Skip processing redundant requests.
    if (offsetVoltuV == pPolicy->offsetVoltRequV)
    {
        status = FLCN_OK;
        goto voltPolicyOffsetVoltage_exit;
    }

    voltWriteSemaphoreTake();
    // Cache the requested voltage offset.
    pPolicy->offsetVoltRequV = offsetVoltuV;

    // Call VOLT_POLICYs OFFSET_VOLTAGE interface.
    status = voltPolicyOffsetVoltageInternal(pPolicy);
    voltWriteSemaphoreGive();

voltPolicyOffsetVoltage_exit:
    return status;
}

/*!
 * @brief Obtain current voltage offset that is applied on the one or more VOLT_RAILs
 * according to the appropriate VOLT_POLICY.
 *
 * @param[in]  policyIdx        Voltage Policy Index
 * @param[out] pOffsetVoltuV    Current Voltage Offset
 *
 * @return  FLCN_ERR_ILWALID_ARGUMENT
 *      Invalid input parameters.
 * @return  FLCN_ERR_ILWALID_STATE
 *      PMU VOLT infrastructure isn't loaded.
 * @return  FLCN_OK
 *      Requested current voltage offset was obtained successfully.
 * @return  Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
FLCN_STATUS
voltPolicyOffsetGet
(
    LwU8     policyIdx,
    LwS32   *pOffsetVoltuV
)
{
    FLCN_STATUS     status = FLCN_OK;
    VOLT_POLICY    *pPolicy;

    // Sanity checks.
    if (!PMU_VOLT_IS_LOADED())
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto voltPolicyOffsetGet_exit;
    }

    // Obtain VOLT_POLICY pointer.
    pPolicy = VOLT_POLICY_GET(policyIdx);
    if (pPolicy == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto voltPolicyOffsetGet_exit;
    }

    voltReadSemaphoreTake();
    // Copy out the current voltage offset.
    *pOffsetVoltuV = pPolicy->offsetVoltLwrruV;
    voltReadSemaphoreGive();

voltPolicyOffsetGet_exit:
    return status;
}
#endif // PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_OFFSET)

/*!
 * @copydoc VoltPolicyLoad()
 */
FLCN_STATUS
voltPolicyLoad_IMPL
(
    VOLT_POLICY    *pPolicy
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pPolicy))
    {
        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_SINGLE_RAIL))
            {
                status = voltPolicyLoad_SINGLE_RAIL(pPolicy);
            }
            break;
        }

        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL_MULTI_STEP:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_SINGLE_RAIL_MULTI_STEP))
            {
                status = voltPolicyLoad_SINGLE_RAIL_MULTI_STEP(pPolicy);
            }
            break;
        }

        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SPLIT_RAIL_MULTI_STEP:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_SPLIT_RAIL_MULTI_STEP))
            {
                status = voltPolicyLoad_SPLIT_RAIL_MULTI_STEP(pPolicy);
            }
            break;
        }

        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SPLIT_RAIL_SINGLE_STEP:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_SPLIT_RAIL_SINGLE_STEP))
            {
                status = voltPolicyLoad_SPLIT_RAIL_SINGLE_STEP(pPolicy);
            }
            break;
        }

        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_MULTI_RAIL:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_MULTI_RAIL))
            {
                status = voltPolicyLoad_MULTI_RAIL(pPolicy);
            }
            break;
        }

        default:
        {
            // To pacify coverity.
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @copydoc VoltPolicySanityCheck()
 */
FLCN_STATUS
voltPolicySanityCheck_IMPL
(
    VOLT_POLICY    *pPolicy,
    LwU8            count,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM
                   *pList
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pPolicy))
    {
        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_SINGLE_RAIL))
            {
                status = voltPolicySanityCheck_SINGLE_RAIL(pPolicy, count, pList);
            }
            break;
        }

        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL_MULTI_STEP:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_SINGLE_RAIL_MULTI_STEP))
            {
                status = voltPolicySanityCheck_SINGLE_RAIL_MULTI_STEP(pPolicy, count, pList);
            }
            break;
        }

        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SPLIT_RAIL_MULTI_STEP:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_SPLIT_RAIL_MULTI_STEP))
            {
                status = voltPolicySanityCheck_SPLIT_RAIL_MULTI_STEP(pPolicy, count, pList);
            }
            break;
        }

        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SPLIT_RAIL_SINGLE_STEP:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_SPLIT_RAIL_SINGLE_STEP))
            {
                status = voltPolicySanityCheck_SPLIT_RAIL_SINGLE_STEP(pPolicy, count, pList);
            }
            break;
        }

        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_MULTI_RAIL:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_MULTI_RAIL))
            {
                status = voltPolicySanityCheck_MULTI_RAIL(pPolicy, count, pList);
            }
            break;
        }

        default:
        {
            // To pacify coverity.
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @copydoc VoltPolicySetVoltage()
 */
FLCN_STATUS
voltPolicySetVoltageInternal_IMPL
(
    VOLT_POLICY    *pPolicy,
    LwU8            count,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM
                   *pList
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pPolicy))
    {
        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_VOLT_VOLT_POLICY_SINGLE_RAIL);
            status = voltPolicySetVoltage_SINGLE_RAIL(pPolicy, count, pList);
            break;
        }

        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL_MULTI_STEP:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_VOLT_VOLT_POLICY_SINGLE_RAIL_MULTI_STEP);
            status = voltPolicySetVoltage_SINGLE_RAIL_MULTI_STEP(pPolicy, count, pList);
            break;
        }

        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SPLIT_RAIL_MULTI_STEP:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_VOLT_VOLT_POLICY_SPLIT_RAIL_MULTI_STEP);
            status = voltPolicySetVoltage_SPLIT_RAIL_MULTI_STEP(pPolicy, count, pList);
            break;
        }

        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SPLIT_RAIL_SINGLE_STEP:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_VOLT_VOLT_POLICY_SPLIT_RAIL_SINGLE_STEP);
            status = voltPolicySetVoltage_SPLIT_RAIL_SINGLE_STEP(pPolicy, count, pList);
            break;
        }

        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_MULTI_RAIL:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_VOLT_VOLT_POLICY_MULTI_RAIL);
            status = voltPolicySetVoltage_MULTI_RAIL(pPolicy, count, pList);
            break;
        }

        default:
        {
            // To pacify coverity.
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltPolicySetVoltageInternal_exit;
    }

    // Notify PMGR task about voltage change.
    s_voltPolicyVoltageChangeNotify();

voltPolicySetVoltageInternal_exit:
    return status;
}

/*!
 * @copydoc VoltPolicyDynamilwpdate()
 */
FLCN_STATUS
voltPolicyDynamilwpdate_IMPL
(
    VOLT_POLICY    *pPolicy
)
{
    FLCN_STATUS status = FLCN_OK;

    switch (BOARDOBJ_GET_TYPE(pPolicy))
    {
        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SPLIT_RAIL_MULTI_STEP:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_SPLIT_RAIL_MULTI_STEP))
            {
                status = voltPolicyDynamilwpdate_SPLIT_RAIL_MULTI_STEP(pPolicy);
            }
            break;
        }

        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SPLIT_RAIL_SINGLE_STEP:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_SPLIT_RAIL_SINGLE_STEP))
            {
                status = voltPolicyDynamilwpdate_SPLIT_RAIL_SINGLE_STEP(pPolicy);
            }
            break;
        }
        
        default:
        {
            // To pacify coverity.
            break;
        }
    }

    return status;
}

#if PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_OFFSET)
/*!
 * @copydoc VoltPolicyOffsetVoltage()
 */
FLCN_STATUS
voltPolicyOffsetVoltageInternal
(
    VOLT_POLICY    *pPolicy
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pPolicy))
    {
        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_SINGLE_RAIL))
            {
                status = voltPolicyOffsetVoltage_SINGLE_RAIL(pPolicy);
            }
            break;
        }

        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL_MULTI_STEP:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_SINGLE_RAIL_MULTI_STEP))
            {
                status = voltPolicyOffsetVoltage_SINGLE_RAIL_MULTI_STEP(pPolicy);
            }
            break;
        }

        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SPLIT_RAIL_MULTI_STEP:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_SPLIT_RAIL_MULTI_STEP))
            {
                status = voltPolicyOffsetVoltage_SPLIT_RAIL_MULTI_STEP(pPolicy);
            }
            break;
        }

        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SPLIT_RAIL_SINGLE_STEP:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_SPLIT_RAIL_SINGLE_STEP))
            {
                status = voltPolicyOffsetVoltage_SPLIT_RAIL_SINGLE_STEP(pPolicy);
            }
            break;
        }

        default:
        {
            // To pacify coverity.
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}
#endif // PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_OFFSET)

/*!
 * @brief Colwert a @ref VOLT_POLICY client LW2080_CTRL_VOLT_VOLT_POLICY_CLIENT_<xyz> to
 * Voltage Policy Table Index.
 *
 * @param[in]  clientID     Voltage Policy Client ID
 *
 * @return  Voltage Policy Table Index
 */
LwU8
voltPolicyClientColwertToIdx_IMPL
(
    LwU8 clientID
)
{
    LwU8 seqPolicyIndex = LW2080_CTRL_VOLT_VOLT_POLICY_INDEX_ILWALID;

    switch (clientID)
    {
        case LW2080_CTRL_VOLT_VOLT_POLICY_CLIENT_PERF_CORE_VF_SEQ:
        {
            seqPolicyIndex = Volt.policyMetadata.perfCoreVFSeqPolicyIdx;
            break;
        }

        default:
        {
            // To pacify coverity.
            break;
        }
    }

    return seqPolicyIndex;
}

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_OFFSET))
/*!
 * @brief Compute the maximum positive and maximum negative voltage offset that can be
 * applied to a set of voltage rails.
 *
 * @param[in]  clientID
 *      ID of the client that wants to get the voltage offset information.
 * @param[in]  pRailList
 *      Pointer that holds the list containing target voltage data for the VOLT_RAILs.
 * @param[out]  pRailOffsetList
 *      Pointer that holds the list containing voltage offset for each voltage rail.
 *
 * @return  FLCN_ERR_ILWALID_ARGUMENT
 *      Invalid input parameters.
 * @return  FLCN_OK
 *      Max positive and negative offset was computed successfully.
 * @return  Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
FLCN_STATUS
voltPolicyOffsetRangeGet
(
    LwU8                              clientID,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST  *pRailList,
    VOLT_RAIL_OFFSET_RANGE_LIST      *pRailOffsetList
)
{
    FLCN_STATUS     status = FLCN_ERR_NOT_SUPPORTED;
    VOLT_POLICY    *pPolicy;

    // Obtain VOLT_POLICY pointer from the Voltage Policy Table Index.
    pPolicy = VOLT_POLICY_GET(voltPolicyClientColwertToIdx(clientID));
    if (pPolicy == NULL)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_INDEX;
    }

    switch (BOARDOBJ_GET_TYPE(pPolicy))
    {
        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_SINGLE_RAIL))
            {
                status = voltPolicyOffsetRangeGetInternal_SINGLE_RAIL(pPolicy, pRailList, pRailOffsetList);
            }
            break;
        }

        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SINGLE_RAIL_MULTI_STEP:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_SINGLE_RAIL_MULTI_STEP))
            {
                status = voltPolicyOffsetRangeGetInternal_SINGLE_RAIL_MULTI_STEP(pPolicy, pRailList, pRailOffsetList);
            }
            break;
        }

        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SPLIT_RAIL_MULTI_STEP:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_SPLIT_RAIL_MULTI_STEP))
            {
                status = voltPolicyOffsetRangeGetInternal_SPLIT_RAIL_MULTI_STEP(pPolicy, pRailList, pRailOffsetList);
            }
            break;
        }

        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_SPLIT_RAIL_SINGLE_STEP:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_SPLIT_RAIL_SINGLE_STEP))
            {
                status = voltPolicyOffsetRangeGetInternal_SPLIT_RAIL_SINGLE_STEP(pPolicy, pRailList, pRailOffsetList);
            }
            break;
        }

        case LW2080_CTRL_VOLT_VOLT_POLICY_TYPE_MULTI_RAIL:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_MULTI_RAIL))
            {
                status = voltPolicyOffsetRangeGetInternal_MULTI_RAIL(pPolicy, pRailList, pRailOffsetList);
            }
            break;
        }

        default:
        {
            // To pacify coverity.
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}
#endif // PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_OFFSET)

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief Helper which sends the command to the PMGR task for notifying the voltage change.
 */
static void
s_voltPolicyVoltageChangeNotify(void)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_VOLT_STATE_SYNC))
    {
        DISPATCH_PMGR   disp2Pmgr;

        if (PMGR_PERF_VOLT_CHANGE_NOTIFY(&Pmgr))
        {
            disp2Pmgr.hdr.eventType = PMGR_EVENT_ID_PERF_VOLT_CHANGE_NOTIFY;
            lwrtosQueueIdSendBlocking(LWOS_QUEUE_ID(PMU, PMGR),
                                      &disp2Pmgr, sizeof(disp2Pmgr));
        }
    }
}
