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
 * @file  voltpolicy_single_rail.c
 * @brief VOLT Voltage Policy Model specific to single rail.
 *
 * This module is a collection of functions managing and manipulating state
 * related to the single rail voltage policy.
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "volt/objvolt.h"
#include "volt/voltpolicy.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjVirtualTableDynamicCast (s_voltPolicyDynamicCast_SINGLE_RAIL)
    GCC_ATTRIB_SECTION("imem_libVolt", "s_voltPolicyDynamicCast_SINGLE_RAIL")
    GCC_ATTRIB_USED();

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/*!
 * Virtual table for VOLT_POLICY_SINGLE_RAIL object interfaces.
 */
BOARDOBJ_VIRTUAL_TABLE VoltPolicySingleRailVirtualTable =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_DYNAMIC_CAST(s_voltPolicyDynamicCast_SINGLE_RAIL)
};

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Constructor for the VOLTAGE_POLICY_SINGLE_RAIL class.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
voltPolicyGrpIfaceModel10ObjSetImpl_SINGLE_RAIL
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_VOLT_VOLT_POLICY_SINGLE_RAIL_BOARDOBJ_SET   *pSet    =
        (RM_PMU_VOLT_VOLT_POLICY_SINGLE_RAIL_BOARDOBJ_SET *)pBoardObjDesc;
    VOLT_POLICY_SINGLE_RAIL                            *pPolicy;
    FLCN_STATUS                                         status;
    VOLT_RAIL                                          *pRail;

    pRail = VOLT_RAIL_GET(pSet->railIdx);
    if (pRail == NULL)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        goto voltPolicyGrpIfaceModel10ObjSetImpl_SINGLE_RAIL_exit;
    }

    // Call super-class constructor.
    status = voltPolicyGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto voltPolicyGrpIfaceModel10ObjSetImpl_SINGLE_RAIL_exit;
    }

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pPolicy, *ppBoardObj, VOLT, VOLT_POLICY, SINGLE_RAIL,
                  &status, voltPolicyGrpIfaceModel10ObjSetImpl_SINGLE_RAIL_exit);

    // Set member variables.
    pPolicy->pRail = pRail;

voltPolicyGrpIfaceModel10ObjSetImpl_SINGLE_RAIL_exit:
    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus()
 */
FLCN_STATUS
voltPolicyIfaceModel10GetStatus_SINGLE_RAIL
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_VOLT_VOLT_POLICY_SINGLE_RAIL_BOARDOBJ_GET_STATUS    *pGetStatus;
    VOLT_POLICY_SINGLE_RAIL                                    *pPolicy;
    FLCN_STATUS                                                 status;

    // Call super-class implementation.
    status = voltPolicyIfaceModel10GetStatus(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto voltPolicyIfaceModel10GetStatus_SINGLE_RAIL_exit;
    }
    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pPolicy, pBoardObj, VOLT, VOLT_POLICY, SINGLE_RAIL,
                  &status, voltPolicyIfaceModel10GetStatus_SINGLE_RAIL_exit);

    pGetStatus  = (RM_PMU_VOLT_VOLT_POLICY_SINGLE_RAIL_BOARDOBJ_GET_STATUS *)pPayload;

    // Set VOLT_POLICY_SINGLE_RAIL query parameters.
    pGetStatus->lwrrVoltuV  = pPolicy->lwrrVoltuV;

voltPolicyIfaceModel10GetStatus_SINGLE_RAIL_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @copydoc VoltPolicyLoad()
 */
FLCN_STATUS
voltPolicyLoad_SINGLE_RAIL_IMPL
(
    VOLT_POLICY    *pPolicy
)
{
    VOLT_POLICY_SINGLE_RAIL    *pPol;
    VOLT_RAIL                  *pRail;
    FLCN_STATUS                 status;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pPol, &pPolicy->super, VOLT, VOLT_POLICY, SINGLE_RAIL,
                  &status, voltPolicyLoad_SINGLE_RAIL_IMPL_exit);

    pRail = pPol->pRail;

    // Obtain the current voltage via internal wrapper function.
    status = voltRailGetVoltageInternal(pRail, &pPol->lwrrVoltuV);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltPolicyLoad_SINGLE_RAIL_IMPL_exit;
    }

voltPolicyLoad_SINGLE_RAIL_IMPL_exit:
    return status;
}

/*!
 * @copydoc VoltPolicySanityCheck()
 */
FLCN_STATUS
voltPolicySanityCheck_SINGLE_RAIL
(
    VOLT_POLICY    *pPolicy,
    LwU8            count,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM
                   *pList
)
{
    VOLT_POLICY_SINGLE_RAIL    *pPol;
    VOLT_RAIL                  *pRail;
    FLCN_STATUS                 status;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pPol, &pPolicy->super, VOLT, VOLT_POLICY, SINGLE_RAIL,
                  &status, voltPolicySanityCheck_SINGLE_RAIL_exit);

    pRail = pPol->pRail;

    // Sanity checks.
    if ((count != 1U) ||
        (pList->railIdx != BOARDOBJ_GET_GRP_IDX_8BIT(&pRail->super)))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto voltPolicySanityCheck_SINGLE_RAIL_exit;
    }

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
    // SINGLE_RAIL policy supports only VF_SWITCH action on a rail.
    if (pList->railAction != LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_VF_SWITCH)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto voltPolicySanityCheck_SINGLE_RAIL_exit;
        
    }
#endif

voltPolicySanityCheck_SINGLE_RAIL_exit:
    return status;
}

/*!
 * @copydoc VoltPolicySetVoltage()
 */
FLCN_STATUS
voltPolicySetVoltage_SINGLE_RAIL_IMPL
(
    VOLT_POLICY    *pPolicy,
    LwU8            count,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM
                   *pList
)
{
    VOLT_POLICY_SINGLE_RAIL    *pPol;
    VOLT_RAIL                  *pRail;
    LwU32                       targetVoltuV;
    LwS32                       tmpTargetVoltuV;
    LwU32                       maxVoltuV;
    FLCN_STATUS                 status;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pPol, &pPolicy->super, VOLT, VOLT_POLICY, SINGLE_RAIL,
                  &status, voltPolicySetVoltage_SINGLE_RAIL_IMPL_exit);

    pRail = pPol->pRail;

    // Round the requested target voltage.
    status = voltRailRoundVoltage(pRail, (LwS32*)&pList->voltageuV,
                LW_TRUE, LW_TRUE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltPolicySetVoltage_SINGLE_RAIL_IMPL_exit;
    }

    // Obtain the max allowed rail voltage.
    status = voltRailGetVoltageMax(pRail, &maxVoltuV);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltPolicySetVoltage_SINGLE_RAIL_IMPL_exit;
    }

    // Make sure requested voltage doesn't exceed max limit.
    pList->voltageuV = LW_MIN(pList->voltageuV, maxVoltuV);

    // Apply offset on requested voltage.
#if PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_OFFSET)
    tmpTargetVoltuV = LW_MAX(0, ((LwS32)pList->voltageuV + pPolicy->offsetVoltRequV));

    // Avoided cast from composite expression to fix MISRA Rule 10.8.
    targetVoltuV = (LwU32)tmpTargetVoltuV;
#else
    LwU8  clientOffsetType;

    tmpTargetVoltuV = (LwS32)pList->voltageuV;
    for (clientOffsetType = 0; clientOffsetType < LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_MAX; clientOffsetType++)
    {
        tmpTargetVoltuV += pList->voltOffsetuV[clientOffsetType];
    }

    tmpTargetVoltuV = LW_MAX(0, tmpTargetVoltuV);

    // Avoided cast from composite expression to fix MISRA Rule 10.8.
    targetVoltuV = (LwU32)tmpTargetVoltuV;
#endif // PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_OFFSET)

    // Make sure offsetted voltage doesn't exceed max limit.
    targetVoltuV = LW_MIN(targetVoltuV, maxVoltuV);

    // Change the rail voltage.
    status = voltRailSetVoltage(pRail, targetVoltuV, LW_TRUE, LW_TRUE, pList->railAction);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltPolicySetVoltage_SINGLE_RAIL_IMPL_exit;
    }

    // Cache value of most recently requested voltage without offset.
    pPol->lwrrVoltuV = pList->voltageuV;

#if PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_OFFSET)
    LwU32  tmpVoltLwrruV;

    // Cache the current voltage offset.
    tmpVoltLwrruV = (targetVoltuV - pList->voltageuV);
    pPol->super.offsetVoltLwrruV = (LwS32)tmpVoltLwrruV;
#endif // PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_OFFSET)

voltPolicySetVoltage_SINGLE_RAIL_IMPL_exit:
    return status;
}

#if PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_OFFSET)
/*!
 * @copydoc VoltPolicyOffsetVoltage()
 */
FLCN_STATUS
voltPolicyOffsetVoltage_SINGLE_RAIL
(
    VOLT_POLICY    *pPolicy
)
{
    VOLT_POLICY_SINGLE_RAIL    *pPol;
    VOLT_RAIL                  *pRail;
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM
                                list;
    FLCN_STATUS                 status;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pPol, &pPolicy->super, VOLT, VOLT_POLICY, SINGLE_RAIL,
                  &status, voltPolicyOffsetVoltage_SINGLE_RAIL_exit);

    pRail = pPol->pRail;

    (void)memset(&list, 0, sizeof(list));

    // Round up the offset to avoid voltage undershoot.
    status = voltRailRoundVoltage(pRail, &pPolicy->offsetVoltRequV,
                LW_TRUE, LW_FALSE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltPolicyOffsetVoltage_SINGLE_RAIL_exit;
    }

    // Build the target voltage list for setting the voltage.
    list.railIdx    = BOARDOBJ_GET_GRP_IDX_8BIT(&pRail->super);
    list.voltageuV  = pPol->lwrrVoltuV;

    // Call VOLT_POLICYs SET_VOLTAGE interface.
    status = voltPolicySetVoltageInternal(pPolicy, 1, &list);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltPolicyOffsetVoltage_SINGLE_RAIL_exit;
    }

voltPolicyOffsetVoltage_SINGLE_RAIL_exit:
    return status;
}
#endif // PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_OFFSET)

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_OFFSET))
/*!
 * @copydoc VoltPolicyOffsetRangeGetInternal()
 */
FLCN_STATUS
voltPolicyOffsetRangeGetInternal_SINGLE_RAIL
(
    VOLT_POLICY                      *pVoltPolicy,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST  *pRailList,
    VOLT_RAIL_OFFSET_RANGE_LIST      *pRailOffsetList
)
{
    // No SINGLE_RAIL policy specific constraints.
    return FLCN_OK;
}
#endif // PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_OFFSET)

/*!
 * @brief   VOLT_POLICY_SINGLE_RAIL implementation of
 *          @ref BoardObjVirtualTableDynamicCast()
 *
 * @copydoc BoardObjVirtualTableDynamicCast()
 */
static void *
s_voltPolicyDynamicCast_SINGLE_RAIL
(
    BOARDOBJ   *pBoardObj,
    LwU8        requestedType
)
{
    void                     *pObject               = NULL;
    VOLT_POLICY_SINGLE_RAIL  *pVoltPolicySingleRail = (VOLT_POLICY_SINGLE_RAIL *)pBoardObj;

    if (BOARDOBJ_GET_TYPE(pBoardObj) !=
        LW2080_CTRL_BOARDOBJ_TYPE(VOLT, VOLT_POLICY, SINGLE_RAIL))
    {
        PMU_BREAKPOINT();
        goto s_voltPolicyDynamicCast_SINGLE_RAIL_exit;
    }

    switch (requestedType)
    {
        case LW2080_CTRL_BOARDOBJ_TYPE(VOLT, VOLT_POLICY, BASE):
        {
            VOLT_POLICY *pVoltPolicy = &pVoltPolicySingleRail->super;
            pObject = (void *)pVoltPolicy;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(VOLT, VOLT_POLICY, SINGLE_RAIL):
        {
            pObject = (void *)pVoltPolicySingleRail;
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            break;
        }
    }

s_voltPolicyDynamicCast_SINGLE_RAIL_exit:
    return pObject;
}
