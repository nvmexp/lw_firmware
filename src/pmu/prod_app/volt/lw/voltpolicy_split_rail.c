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
 * @file  voltpolicy_split_rail.c
 * @brief VOLT Voltage Policy Model specific to split rail.
 *
 * This module is a collection of functions managing and manipulating state
 * related to the split rail voltage policy.
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "perf/3x/vfe.h"
#include "volt/objvolt.h"
#include "volt/voltpolicy.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static FLCN_STATUS  s_voltPolicySplitRailPrimarySecondaryDeltaConstraintsCheck(VOLT_POLICY *pPolicy, LwU32 railPrimaryuV, LwU32 railSecondaryuV);
static FLCN_STATUS  s_voltPolicySplitRailPrimarySecondaryListIdxGet(VOLT_POLICY *pPolicy, LwU8 count, LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM *pList, LwU8 *pListIdxPrimary, LwU8 *pListIdxSecondary);
static FLCN_STATUS  s_voltPolicySplitRailPrimarySecondaryRoundVoltage(VOLT_POLICY *pPolicy, LwU32 *pRailPrimaryuV, LwU32 *pRailSecondaryuV);

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Constructor for the VOLTAGE_POLICY_SPLIT_RAIL class.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
voltPolicyGrpIfaceModel10ObjSetImpl_SPLIT_RAIL
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    RM_PMU_VOLT_VOLT_POLICY_SPLIT_RAIL_BOARDOBJ_SET    *pSet    =
        (RM_PMU_VOLT_VOLT_POLICY_SPLIT_RAIL_BOARDOBJ_SET *)pBoardObjSet;
    VOLT_POLICY_SPLIT_RAIL         *pPolicy;
    FLCN_STATUS                     status;
    VOLT_RAIL                      *pRailPrimary;
    VOLT_RAIL                      *pRailSecondary;

    // Sanity check.
    pRailPrimary = VOLT_RAIL_GET(pSet->railIdxPrimary);
    if (pRailPrimary == NULL)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        goto voltPolicyGrpIfaceModel10ObjSetImpl_SPLIT_RAIL_exit;
    }
    pRailSecondary = VOLT_RAIL_GET(pSet->railIdxSecondary);
    if (pRailSecondary == NULL)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        goto voltPolicyGrpIfaceModel10ObjSetImpl_SPLIT_RAIL_exit;
    }

    // Call super-class constructor.
    status = voltPolicyGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        goto voltPolicyGrpIfaceModel10ObjSetImpl_SPLIT_RAIL_exit;
    }
    pPolicy     = (VOLT_POLICY_SPLIT_RAIL *)*ppBoardObj;

    // Set member variables.
    VFE_EQU_IDX_COPY_RM_TO_PMU(pPolicy->deltaMilwfeEquIdx,
                               pSet->deltaMilwfeEquIdx);
    VFE_EQU_IDX_COPY_RM_TO_PMU(pPolicy->deltaMaxVfeEquIdx,
                               pSet->deltaMaxVfeEquIdx);
    pPolicy->pRailPrimary        = pRailPrimary;
    pPolicy->pRailSecondary         = pRailSecondary;
    pPolicy->offsetDeltaMinuV   = pSet->offsetDeltaMinuV;
    pPolicy->offsetDeltaMaxuV   = pSet->offsetDeltaMaxuV;

voltPolicyGrpIfaceModel10ObjSetImpl_SPLIT_RAIL_exit:
    return status;
}

/*!
 * @copydoc VoltPolicyOffsetRangeGetInternal
 */
FLCN_STATUS
voltPolicyOffsetRangeGetInternal_SPLIT_RAIL
(

    VOLT_POLICY                      *pVoltPolicy,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST  *pRailList,
    VOLT_RAIL_OFFSET_RANGE_LIST      *pRailOffsetList
)
{
    // NG-TODO: Need to implement this interface.
    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * Obtains the primary and secondary rail index from the client supplied list.
 *
 * @param[in]  pPolicy          VOLT_POLICY object pointer
 * @param[in]  count            Number of items in the list
 * @param[in]  pList            List containing target voltage for the rails
 * @param[out] pListIdxPrimary   Index of the primary voltage rail
 * @param[out] pListIdxSecondary    Index of the secondary voltage rail
 *
 * @return  FLCN_ERR_ILWALID_ARGUMENT
 *      Invalid input parameters.
 * @return  FLCN_OK
 *      Requested indexes were obtained successfully.
 */
static FLCN_STATUS
s_voltPolicySplitRailPrimarySecondaryListIdxGet
(
    VOLT_POLICY    *pPolicy,
    LwU8            count,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM
                   *pList,
    LwU8           *pListIdxPrimary,
    LwU8           *pListIdxSecondary
)
{
    VOLT_POLICY_SPLIT_RAIL *pPol    = (VOLT_POLICY_SPLIT_RAIL *)pPolicy;
    FLCN_STATUS             status  = FLCN_ERR_ILWALID_ARGUMENT;
    LwU8                    i;

    // Sanity check.
    if (count != 2)
    {
        PMU_BREAKPOINT();
        goto s_voltPolicySplitRailPrimarySecondaryListIdxGet_exit;
    }

    // Set default value to invalid.
    *pListIdxPrimary = LW_U8_MAX;
    *pListIdxSecondary  = LW_U8_MAX;

    // Pull out primary and secondary rail index based on voltage domain.
    for (i = 0; i < count; i++)
    {
        if (pList[i].railIdx == BOARDOBJ_GET_GRP_IDX_8BIT(&pPol->pRailPrimary->super))
        {
            *pListIdxPrimary = i;
        }
        else if (pList[i].railIdx == BOARDOBJ_GET_GRP_IDX_8BIT(&pPol->pRailSecondary->super))
        {
            *pListIdxSecondary = i;
        }
        else
        {
            PMU_BREAKPOINT();
            goto s_voltPolicySplitRailPrimarySecondaryListIdxGet_exit;
        }
    }

    // If either of the indexes aren't valid, return error.
    if ((*pListIdxPrimary == LW_U8_MAX) ||
        (*pListIdxSecondary == LW_U8_MAX))
    {
        PMU_BREAKPOINT();
        goto s_voltPolicySplitRailPrimarySecondaryListIdxGet_exit;
    }

    // At this point both primary and secondary rail index is set.
    status = FLCN_OK;

s_voltPolicySplitRailPrimarySecondaryListIdxGet_exit:
    return status;
}

/*!
 * Wrapper function that rounds the primary and secondary voltage pair.
 *
 * @param[in]     pPolicy           VOLT_POLICY object pointer
 * @param[in/out] pRailPrimaryuV    Primary rail voltage
 * @param[in/out] pRailSecondaryuV      Secondary rail voltage
 *
 * @return FLCN_OK
 *      Voltage pair successfully rounded to their supported values.
 * @return Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
static FLCN_STATUS
s_voltPolicySplitRailPrimarySecondaryRoundVoltage
(
    VOLT_POLICY    *pPolicy,
    LwU32          *pRailPrimaryuV,
    LwU32          *pRailSecondaryuV
)
{
    VOLT_POLICY_SPLIT_RAIL *pPol        = (VOLT_POLICY_SPLIT_RAIL *)pPolicy;
    VOLT_RAIL              *pRailPrimary = pPol->pRailPrimary;
    VOLT_RAIL              *pRailSecondary  = pPol->pRailSecondary;
    FLCN_STATUS             status;

    status = voltRailRoundVoltage(pRailPrimary, (LwS32*)pRailPrimaryuV,
                LW_TRUE, LW_TRUE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_voltPolicySplitRailPrimarySecondaryRoundVoltage_exit;
    }

    status = voltRailRoundVoltage(pRailSecondary, (LwS32*)pRailSecondaryuV,
                LW_TRUE, LW_TRUE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_voltPolicySplitRailPrimarySecondaryRoundVoltage_exit;
    }

s_voltPolicySplitRailPrimarySecondaryRoundVoltage_exit:
    return status;
}

/*!
 * Obtains the split rail tuple consisting of current and target voltages
 * for primary and secondary rail.
 *
 * The interface also rounds the client supplied list of primary and secondary
 * voltage pair and ceils the values to the maximum allowed voltage for the
 * corresponding rails. It also callwlates the voltage offset from the
 * requested voltage offset and accordingly adjusts the target voltages.
 *
 * @param[in]  pPolicy          VOLT_POLICY object pointer
 * @param[in]  count            Number of items in the list
 * @param[in]  pList            List containing target voltage for the rails
 * @param[out] pTuple           Buffer containing split rail tuple
 * @param[out] pOffsetVoltuV    Voltage offset applied to target voltages
 *
 * @return  FLCN_OK
 *      Requested parameters were obtained successfully.
 * @return Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
FLCN_STATUS
voltPolicySplitRailTupleAndOffsetGet
(
    VOLT_POLICY    *pPolicy,
    LwU8            count,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM
                   *pList,
    VOLT_POLICY_SPLIT_RAIL_TUPLE
                   *pTuple,
    LwS32          *pOffsetVoltuV
)
{
    VOLT_POLICY_SPLIT_RAIL *pPol        = (VOLT_POLICY_SPLIT_RAIL *)pPolicy;
    VOLT_RAIL              *pRailPrimary = pPol->pRailPrimary;
    VOLT_RAIL              *pRailSecondary  = pPol->pRailSecondary;
    LwU32                   maxVoltPrimaryuV;
    LwU32                   maxVoltSecondaryuV;
    LwU8                    listIdxPrimary;
    LwU8                    listIdxSecondary;
    FLCN_STATUS             status;

    // Obtain the current voltage pair for primary and secondary rail via internal wrapper function.
    status = voltRailGetVoltageInternal(pRailPrimary, &pTuple->lwrrVoltPrimaryuV);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltPolicySplitRailPrimarySecondaryTargetVoltageGet_exit;
    }
    status = voltRailGetVoltageInternal(pRailSecondary, &pTuple->lwrrVoltSecondaryuV);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltPolicySplitRailPrimarySecondaryTargetVoltageGet_exit;
    }

    // Obtain the max allowed voltage pair for primary and secondary rail.
    status = voltRailGetVoltageMax(pRailPrimary, &maxVoltPrimaryuV);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltPolicySplitRailPrimarySecondaryTargetVoltageGet_exit;
    }
    status = voltRailGetVoltageMax(pRailSecondary, &maxVoltSecondaryuV);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltPolicySplitRailPrimarySecondaryTargetVoltageGet_exit;
    }

    // Obtain the primary and secondary rail index from the client supplied list.
    status = s_voltPolicySplitRailPrimarySecondaryListIdxGet(pPolicy,
                count, pList, &listIdxPrimary, &listIdxSecondary);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltPolicySplitRailPrimarySecondaryTargetVoltageGet_exit;
    }

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
    pTuple->railActionPrimary = pList[listIdxPrimary].railAction;
    pTuple->railActionSecondary  = pList[listIdxSecondary].railAction;
#endif

    // Round the requested target voltage pair before changing voltages.
    status = s_voltPolicySplitRailPrimarySecondaryRoundVoltage(pPolicy,
                &pList[listIdxPrimary].voltageuV,
                &pList[listIdxSecondary].voltageuV);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltPolicySplitRailPrimarySecondaryTargetVoltageGet_exit;
    }

    // Make sure requested voltages doesn't exceed max limit for either rails.
    pList[listIdxPrimary].voltageuV =
        LW_MIN(pList[listIdxPrimary].voltageuV, maxVoltPrimaryuV);
    pList[listIdxSecondary].voltageuV =
        LW_MIN(pList[listIdxSecondary].voltageuV, maxVoltSecondaryuV);

    // Choose offset so that it doesn't exceed max limit for either rails.
#if PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_OFFSET)
    *pOffsetVoltuV = LW_MIN(pPolicy->offsetVoltRequV,
                      LW_MIN((LwS32)(maxVoltPrimaryuV - pList[listIdxPrimary].voltageuV),
                             (LwS32)(maxVoltSecondaryuV - pList[listIdxSecondary].voltageuV)));
#else
    LwS32 totalVoltDeltaPrimaryuV = RM_PMU_VOLT_VALUE_0V_IN_UV_SIGNED;
    LwS32 totalVoltDeltaSecondaryuV   = RM_PMU_VOLT_VALUE_0V_IN_UV_SIGNED;
    LwU8  clientOffsetType;

    for (clientOffsetType = 0; clientOffsetType < LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_MAX; clientOffsetType++)
    {
        totalVoltDeltaPrimaryuV += pList[listIdxPrimary].voltOffsetuV[clientOffsetType];
        totalVoltDeltaSecondaryuV   += pList[listIdxSecondary].voltOffsetuV[clientOffsetType];
    }

    *pOffsetVoltuV = LW_MIN(
                        LW_MIN(totalVoltDeltaPrimaryuV, (LwS32)(maxVoltPrimaryuV - pList[listIdxPrimary].voltageuV)),
                        LW_MIN(totalVoltDeltaSecondaryuV,   (LwS32)(maxVoltSecondaryuV  - pList[listIdxSecondary].voltageuV))
                        );
#endif // PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_OFFSET)

    // Apply offset on requested voltages for primary and secondary rail.
    pTuple->targetVoltPrimaryuV =
        (LwU32)LW_MAX(0, ((LwS32)pList[listIdxPrimary].voltageuV + *pOffsetVoltuV));
    pTuple->targetVoltSecondaryuV =
        (LwU32)LW_MAX(0, ((LwS32)pList[listIdxSecondary].voltageuV + *pOffsetVoltuV));

voltPolicySplitRailPrimarySecondaryTargetVoltageGet_exit:
    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
voltPolicyIfaceModel10GetStatus_SPLIT_RAIL
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_VOLT_VOLT_POLICY_SPLIT_RAIL_BOARDOBJ_GET_STATUS *pGetStatus;
    VOLT_POLICY_SPLIT_RAIL                                 *pPolicy;
    FLCN_STATUS                                             status;

    // Call super-class implementation.
    status = voltPolicyIfaceModel10GetStatus(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto voltPolicyIfaceModel10GetStatus_SPLIT_RAIL_exit;
    }
    pPolicy     = (VOLT_POLICY_SPLIT_RAIL *)pBoardObj;
    pGetStatus  = (RM_PMU_VOLT_VOLT_POLICY_SPLIT_RAIL_BOARDOBJ_GET_STATUS *)pPayload;

    // Set VOLT_POLICY_SPLIT_RAIL query parameters.
    pGetStatus->deltaMinuV          = pPolicy->deltaMinuV;
    pGetStatus->deltaMaxuV          = pPolicy->deltaMaxuV;
    pGetStatus->origDeltaMinuV      = pPolicy->origDeltaMinuV;
    pGetStatus->origDeltaMaxuV      = pPolicy->origDeltaMaxuV;
    pGetStatus->lwrrVoltPrimaryuV   = pPolicy->lwrrVoltPrimaryuV;
    pGetStatus->lwrrVoltSecondaryuV     = pPolicy->lwrrVoltSecondaryuV;
    pGetStatus->bViolation          = pPolicy->bViolation;

voltPolicyIfaceModel10GetStatus_SPLIT_RAIL_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * Sanity checks the split rail min and max voltage delta constraints
 * between primary and secondary voltage rail.
 *
 * @param[in]     pPolicy           VOLT_POLICY object pointer
 * @param[in/out] railPrimaryuV     Primary rail voltage
 * @param[in/out] railSecondaryuV       Secondary rail voltage
 *
 * @return FLCN_OK
 *      Requested target voltage pair is satisfying split rail constraints.
 * @return Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
static FLCN_STATUS
s_voltPolicySplitRailPrimarySecondaryDeltaConstraintsCheck
(
    VOLT_POLICY    *pPolicy,
    LwU32           railPrimaryuV,
    LwU32           railSecondaryuV
)
{
    VOLT_POLICY_SPLIT_RAIL *pPol    = (VOLT_POLICY_SPLIT_RAIL *)pPolicy;
    FLCN_STATUS             status  = FLCN_ERR_ILWALID_STATE;
    LwS32                   deltauV;

    // Obtain difference between primary and secondary voltage rail.
    deltauV = (LwS32)railSecondaryuV - (LwS32)railPrimaryuV;

    // The difference should honor min and max voltage delta constraints.
    if ((deltauV < pPol->deltaMinuV) ||
        (deltauV > pPol->deltaMaxuV))
    {
        goto s_voltPolicySplitRailPrimarySecondaryDeltaConstraintsCheck_exit;
    }

    // If original delta constraints are violated, set the violation flag.
    if ((deltauV < pPol->origDeltaMinuV) ||
        (deltauV > pPol->origDeltaMaxuV))
    {
        pPol->bViolation = LW_TRUE;
    }

    // At this point all the constraints are met.
    status = FLCN_OK;

s_voltPolicySplitRailPrimarySecondaryDeltaConstraintsCheck_exit:
    return status;
}

/*!
 * @copydoc VoltPolicyLoad
 */
FLCN_STATUS
voltPolicyLoad_SPLIT_RAIL
(
    VOLT_POLICY    *pPolicy
)
{
    VOLT_POLICY_SPLIT_RAIL *pPol        = (VOLT_POLICY_SPLIT_RAIL *)pPolicy;
    VOLT_RAIL              *pRailPrimary = pPol->pRailPrimary;
    VOLT_RAIL              *pRailSecondary  = pPol->pRailSecondary;
    FLCN_STATUS             status;

    // Obtain the current voltage pair for primary and secondary rail via internal wrapper function.
    status = voltRailGetVoltageInternal(pRailPrimary, &pPol->lwrrVoltPrimaryuV);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltPolicyLoad_SPLIT_RAIL_exit;
    }
    status = voltRailGetVoltageInternal(pRailSecondary, &pPol->lwrrVoltSecondaryuV);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltPolicyLoad_SPLIT_RAIL_exit;
    }

voltPolicyLoad_SPLIT_RAIL_exit:
    return status;
}

/*!
 * @copydoc VoltPolicySanityCheck
 */
FLCN_STATUS
voltPolicySanityCheck_SPLIT_RAIL
(
    VOLT_POLICY    *pPolicy,
    LwU8            count,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM
                   *pList
)
{
    LwU32       targetVoltPrimaryuV;
    LwU32       targetVoltSecondaryuV;
    LwU8        listIdxPrimary;
    LwU8        listIdxSecondary;
    FLCN_STATUS status;

    // Obtain the primary and secondary rail index from the client supplied list.
    status = s_voltPolicySplitRailPrimarySecondaryListIdxGet(pPolicy,
                count, pList, &listIdxPrimary, &listIdxSecondary);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltPolicySanityCheck_SPLIT_RAIL_exit;
    }

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
    // SPLIT_RAIL policy supports only VF_SWITCH action on the rails.
    if ((pList[listIdxPrimary].railAction != LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_VF_SWITCH) ||
        (pList[listIdxSecondary].railAction != LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_VF_SWITCH))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto voltPolicySanityCheck_SPLIT_RAIL_exit;
    }
#endif

    // Cache the DMEM variables on the stack for IMEM savings.
    targetVoltPrimaryuV  = pList[listIdxPrimary].voltageuV;
    targetVoltSecondaryuV   = pList[listIdxSecondary].voltageuV;

    // Round the requested target voltage pair before constraints check.
    status = s_voltPolicySplitRailPrimarySecondaryRoundVoltage(pPolicy,
                &targetVoltPrimaryuV, &targetVoltSecondaryuV);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltPolicySanityCheck_SPLIT_RAIL_exit;
    }

    // Sanity check for split rail voltage delta constraints.
    status = s_voltPolicySplitRailPrimarySecondaryDeltaConstraintsCheck(pPolicy,
                targetVoltPrimaryuV, targetVoltSecondaryuV);
    if (status != FLCN_OK)
    {
        goto voltPolicySanityCheck_SPLIT_RAIL_exit;
    }

voltPolicySanityCheck_SPLIT_RAIL_exit:
    return status;
}

/*!
 * @copydoc VoltPolicyDynamilwpdate
 */
FLCN_STATUS
voltPolicyDynamilwpdate_SPLIT_RAIL
(
    VOLT_POLICY    *pPolicy
)
{
    VOLT_POLICY_SPLIT_RAIL     *pPol        = (VOLT_POLICY_SPLIT_RAIL *)pPolicy;
    VOLT_RAIL                  *pRailPrimary = pPol->pRailPrimary;
    RM_PMU_PERF_VFE_EQU_RESULT  result;
    FLCN_STATUS                 status;

    // Evaluate min voltage delta VFE equation index.
    status = vfeEquEvaluate(pPol->deltaMilwfeEquIdx, NULL, 0,
                            LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_VOLT_DELTA_UV,
                            &result);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltPolicyDynamilwpdate_SPLIT_RAIL_exit;
    }
    pPol->origDeltaMinuV = result.voltDeltauV;

    // Evaluate max voltage delta VFE equation index.
    status = vfeEquEvaluate(pPol->deltaMaxVfeEquIdx, NULL, 0,
                            LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_VOLT_DELTA_UV,
                            &result);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltPolicyDynamilwpdate_SPLIT_RAIL_exit;
    }
    pPol->origDeltaMaxuV = result.voltDeltauV;

    // Cache the split rail min and max voltage delta constraints.
    pPol->deltaMinuV = (pPol->origDeltaMinuV + pPol->offsetDeltaMinuV);
    pPol->deltaMaxuV = (pPol->origDeltaMaxuV + pPol->offsetDeltaMaxuV);

    //
    // Round up the min voltage delta constraints. Consider either primary or
    // secondary voltage rail for the rounding.
    //
    status = voltRailRoundVoltage(pRailPrimary, &pPol->origDeltaMinuV,
                LW_TRUE, LW_FALSE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltPolicyDynamilwpdate_SPLIT_RAIL_exit;
    }
    status = voltRailRoundVoltage(pRailPrimary, &pPol->deltaMinuV,
                LW_TRUE, LW_FALSE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltPolicyDynamilwpdate_SPLIT_RAIL_exit;
    }

    //
    // Round down the max voltage delta constraints. Consider either primary or
    // secondary voltage rail for the rounding.
    //
    status = voltRailRoundVoltage(pRailPrimary, &pPol->origDeltaMaxuV,
                LW_FALSE, LW_FALSE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltPolicyDynamilwpdate_SPLIT_RAIL_exit;
    }
    status = voltRailRoundVoltage(pRailPrimary, &pPol->deltaMaxuV,
                LW_FALSE, LW_FALSE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltPolicyDynamilwpdate_SPLIT_RAIL_exit;
    }

voltPolicyDynamilwpdate_SPLIT_RAIL_exit:
    return status;
}

#if PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_OFFSET)
/*!
 * @copydoc VoltPolicyOffsetVoltage
 */
FLCN_STATUS
voltPolicyOffsetVoltage_SPLIT_RAIL
(
    VOLT_POLICY    *pPolicy
)
{
    VOLT_POLICY_SPLIT_RAIL *pPol        = (VOLT_POLICY_SPLIT_RAIL *)pPolicy;
    VOLT_RAIL              *pRailPrimary = pPol->pRailPrimary;
    VOLT_RAIL              *pRailSecondary  = pPol->pRailSecondary;
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM
                            list[2];
    FLCN_STATUS             status;

    (void)memset(list, 0, sizeof(list));

    //
    // Round up the offset to avoid voltage undershoot. Consider either primary
    // or secondary voltage rail for the rounding.
    //
    status = voltRailRoundVoltage(pRailPrimary, &pPolicy->offsetVoltRequV,
                LW_TRUE, LW_FALSE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        return status;
    }

    // Build the target voltage list for setting the voltage.
    list[0].railIdx     = BOARDOBJ_GET_GRP_IDX_8BIT(&pRailPrimary->super);
    list[0].voltageuV   = pPol->lwrrVoltPrimaryuV;
    list[1].railIdx     = BOARDOBJ_GET_GRP_IDX_8BIT(&pRailSecondary->super);
    list[1].voltageuV   = pPol->lwrrVoltSecondaryuV;

    // Call VOLT_POLICYs SET_VOLTAGE interface.
    return voltPolicySetVoltageInternal(pPolicy, 2, list);
}
#endif // PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_OFFSET)
