/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  voltpolicy_multi_rail.c
 * @brief VOLT Voltage Policy Model specific to multi rail.
 *
 * This module is a collection of functions managing and manipulating state
 * related to the multi rail voltage policy.
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "volt/objvolt.h"
#include "volt/voltpolicy.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static FLCN_STATUS s_voltPolicyRoundAndBound_MULTI_RAIL(VOLT_RAIL *pRail, LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM *pListItem, LwBool bRound, LwBool bBound, LwBool bCap, LwU32 *pMaxVoltuV);
static FLCN_STATUS s_voltPolicyComputeVoltage_MULTI_RAIL_VF_SWITCH(VOLT_RAIL *pRail, LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM *pListItem, LwU32 *pTargetVoltuV);
#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
static FLCN_STATUS s_voltPolicyComputeVoltage_MULTI_RAIL_UNGATE(VOLT_RAIL *pRail, LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM *pListItem);
#endif

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Constructor for the VOLTAGE_POLICY_MULTI_RAIL class.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
voltPolicyGrpIfaceModel10ObjSetImpl_MULTI_RAIL
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_VOLT_VOLT_POLICY_MULTI_RAIL_BOARDOBJ_SET   *pSet    =
        (RM_PMU_VOLT_VOLT_POLICY_MULTI_RAIL_BOARDOBJ_SET *)pBoardObjDesc;
    VOLT_POLICY_MULTI_RAIL                            *pPolicy;
    FLCN_STATUS                                        status;

    // Call super-class constructor.
    status = voltPolicyGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto voltPolicyGrpIfaceModel10ObjSetImpl_MULTI_RAIL_exit;
    }
    pPolicy = (VOLT_POLICY_MULTI_RAIL *)*ppBoardObj;

    // Set member variables.
    boardObjGrpMaskInit_E32(&(pPolicy->voltRailMask));
    status = boardObjGrpMaskImport_E32(&(pPolicy->voltRailMask),
                                       &(pSet->voltRailMask));
    if (status != FLCN_OK)
    {
        goto voltPolicyGrpIfaceModel10ObjSetImpl_MULTI_RAIL_exit;
    }

    memset(&pPolicy->voltData, 0, sizeof(LW2080_CTRL_VOLT_VOLT_POLICY_STATUS_DATA_MULTI_RAIL));

voltPolicyGrpIfaceModel10ObjSetImpl_MULTI_RAIL_exit:
    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus()
 */
FLCN_STATUS
voltPolicyIfaceModel10GetStatus_MULTI_RAIL
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_VOLT_VOLT_POLICY_MULTI_RAIL_BOARDOBJ_GET_STATUS    *pGetStatus;
    VOLT_POLICY_MULTI_RAIL                                    *pPolicy;
    FLCN_STATUS                                                status;

    // Call super-class implementation.
    status = voltPolicyIfaceModel10GetStatus(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto voltPolicyIfaceModel10GetStatus_MULTI_RAIL_exit;
    }
    pPolicy     = (VOLT_POLICY_MULTI_RAIL *)pBoardObj;
    pGetStatus  = (RM_PMU_VOLT_VOLT_POLICY_MULTI_RAIL_BOARDOBJ_GET_STATUS *)pPayload;

    // Ensure that the railMask does not have more than the max number of rails that we can accommodate.
    PMU_HALT_COND(boardObjGrpMaskBitSetCount(&pPolicy->voltRailMask) <=
                    LW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS);

    // Set VOLT_POLICY_MULTI_RAIL query parameters.
    pGetStatus->numRails = pPolicy->voltData.numRails;

    memcpy(pGetStatus->railItems, pPolicy->voltData.railItems, pPolicy->voltData.numRails *
                sizeof(LW2080_CTRL_VOLT_VOLT_POLICY_STATUS_DATA_MULTI_RAIL_ITEM));

voltPolicyIfaceModel10GetStatus_MULTI_RAIL_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @copydoc VoltPolicyLoad()
 */
FLCN_STATUS
voltPolicyLoad_MULTI_RAIL
(
    VOLT_POLICY    *pPolicy
)
{
    // Nothing to load (for now).
    return FLCN_OK;
}

/*!
 * @copydoc VoltPolicySanityCheck()
 */
FLCN_STATUS
voltPolicySanityCheck_MULTI_RAIL
(
    VOLT_POLICY    *pPolicy,
    LwU8            count,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM
                   *pList
)
{
    VOLT_POLICY_MULTI_RAIL *pPol =
        (VOLT_POLICY_MULTI_RAIL *)pPolicy;
    FLCN_STATUS status = FLCN_OK;
    LwU8        i;

    // Check that the rail is a part of railMask for this policy.
    for (i = 0; i < count; i++)
    {
        if (!boardObjGrpMaskBitGet(&pPol->voltRailMask, pList[i].railIdx))
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto voltPolicySanityCheck_MULTI_RAIL_exit;
        }
    }

voltPolicySanityCheck_MULTI_RAIL_exit:
    return status;
}

/*!
 * @copydoc VoltPolicySetVoltage()
 */
FLCN_STATUS
voltPolicySetVoltage_MULTI_RAIL
(
    VOLT_POLICY    *pPolicy,
    LwU8            count,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM
                   *pList
)
{
    FLCN_STATUS     status              = FLCN_OK;
    VOLT_DEVICE    *pDefaultDevice      = NULL;
    VOLT_RAIL      *pRail               = NULL;
    LwBool          bWait               = LW_FALSE;
    LwBool          bTrigger            = LW_FALSE;
    VOLT_POLICY_MULTI_RAIL
                   *pPolicyMR           = (VOLT_POLICY_MULTI_RAIL *)pPolicy;
    LwU8            i;

    for (i = 0; i < count; i++)
    {
        LwU32 targetVoltuV = RM_PMU_VOLT_VALUE_0V_IN_UV;

        pRail = VOLT_RAIL_GET(pList[i].railIdx);

        if (pRail == NULL)
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto voltPolicySetVoltage_MULTI_RAIL_exit;
        }

        pPolicyMR->voltData.railItems[i].railIdx    = pList[i].railIdx;
        pPolicyMR->voltData.railItems[i].lwrrVoltuV = pList[i].voltageuV;

        switch (pList[i].railAction)
        {
            case LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_VF_SWITCH:
            {
                //
                // bTrigger - not needed as it will be applied once for all rails
                //            in the list.
                // bWait    - not needed here since we can apply it once for all
                //         rails after the whole rail list is parsed.
                //
                bTrigger = LW_FALSE;
                bWait    = LW_FALSE;
                status = s_voltPolicyComputeVoltage_MULTI_RAIL_VF_SWITCH(pRail, &(pList[i]), &targetVoltuV);
                break;
            }
#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
            case LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_GATE:
            {
                //
                // bTrigger - not needed here since we don't touch voltage HW in
                //            gate sequence.
                // bWait    - required as we need to wait for power down delay time
                //            to ensure proper rail gating.
                //
                bTrigger = LW_FALSE;
                bWait    = LW_TRUE;
                targetVoltuV = pList[i].voltageuV;
                break;
            }
            case LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_UNGATE:
            {
                //
                // bTrigger - required as we program the votlage value before
                //            ungating the rail.
                // bWait    - required as we need to wait for power up delay time
                //            to ensure proper rail ungating.
                //
                bTrigger = LW_TRUE;
                bWait    = LW_TRUE;

                //
                // PP-TODO: On TURING_AMPERE, we have single voltage rail so we cannot
                // program the LWVDD to boot voltage on ungate.
                //
                if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ELW_PSTATES_40_TEST))
                {
                    bTrigger = LW_FALSE;
                }
                status = s_voltPolicyComputeVoltage_MULTI_RAIL_UNGATE(pRail, &(pList[i]));
                targetVoltuV = pList[i].voltageuV;
                break;
            }
#endif
            default:
            {
                status = FLCN_ERR_ILWALID_STATE;
                PMU_BREAKPOINT();
                goto voltPolicySetVoltage_MULTI_RAIL_exit;
            }
        }

        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto voltPolicySetVoltage_MULTI_RAIL_exit;
        }

        status = voltRailSetVoltage(pRail, targetVoltuV, bTrigger, bWait,
                pList[i].railAction);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto voltPolicySetVoltage_MULTI_RAIL_exit;
        }
    }

    pPolicyMR->voltData.numRails = count;

    pDefaultDevice = voltRailDefaultVoltDevGet(pRail);
    if (pDefaultDevice == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto voltPolicySetVoltage_MULTI_RAIL_exit;
    }

    //
    // Apply the atomic trigger for simultaneously changing voltages on one or
    // voltage rails. Atomic trigger feature is dependent on the voltage device type.
    //
    switch (BOARDOBJ_GET_TYPE(pDefaultDevice))
    {
        case LW2080_CTRL_VOLT_VOLT_DEVICE_TYPE_PWM:
        {
            thermVidPwmTriggerMaskSet_HAL();
            break;
        }
        default:
        {
            PMU_HALT();
            status = FLCN_ERR_ILWALID_STATE;
            goto voltPolicySetVoltage_MULTI_RAIL_exit;
        }
    }

    // Wait for the voltage to settle.
    voltDeviceSwitchDelayApply(pDefaultDevice);

voltPolicySetVoltage_MULTI_RAIL_exit:
    return status;
}

/*!
 * @copydoc VoltPolicyOffsetRangeGetInternal()
 */
FLCN_STATUS
voltPolicyOffsetRangeGetInternal_MULTI_RAIL
(
    VOLT_POLICY                      *pVoltPolicy,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST  *pRailList,
    VOLT_RAIL_OFFSET_RANGE_LIST      *pRailOffsetList
)
{
    // No MULTI_RAIL policy specific constraints.
    return FLCN_OK;
}

/*!
 * @brief Helper function to round (with bounding) and cap a given voltage value.
 *
 * @param[in]  pRail        VOLT_RAIL object pointer
 * @param[in]  pListItem    Pointer to the voltage rail item in the input rail list.
 * @param[in]  bRoundUp     Boolean to round up or down
 * @param[in]  bBound
 *      Boolean flag indicating whether the rounded value should be bound to
 *      the range of voltages supported on the regulator.  If this flag is
 *      LW_FALSE and the provided value is outside the range, the value will
 *      be rounded (if possible) but outside the range of supported voltages.
 * @param[in]  bCap         Boolean to indicate whether the function should cap
 *                          the voltage value to the rail's maximum voltage limit.
 * @param[out] pMaxVoltuV   Pointer to the buffer to store the maximum voltage limit.
 *                          Note - This is an optional parameter.
 *
 * @return  FLCN_OK
 *      Requested operation was done successfully.
 * @return  Errors propagated from other functions inside this function.
 */
static FLCN_STATUS
s_voltPolicyRoundAndBound_MULTI_RAIL
(
    VOLT_RAIL *pRail,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM
              *pListItem,
    LwBool     bRound,
    LwBool     bBound,
    LwBool     bCap,
    LwU32     *pMaxVoltuV
)
{
    FLCN_STATUS status;

    // Round the requested target voltage.
    status = voltRailRoundVoltage(pRail, (LwS32*)&pListItem->voltageuV,
                bRound, bBound);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_voltPolicyRoundAndBound_MULTI_RAIL_exit;
    }

    if (bCap)
    {
        LwU32 maxVoltuV;

        // Obtain the max allowed rail voltage.
        status = voltRailGetVoltageMax(pRail, &maxVoltuV);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_voltPolicyRoundAndBound_MULTI_RAIL_exit;
        }

        // Make sure requested voltage doesn't exceed max limit.
        pListItem->voltageuV = LW_MIN(pListItem->voltageuV, maxVoltuV);
        
        if (pMaxVoltuV != NULL)
        {
            *pMaxVoltuV = maxVoltuV;
        }
    }

s_voltPolicyRoundAndBound_MULTI_RAIL_exit:
    return status;
}

/*!
 * @brief Computes voltage based on the VOLT_POLICY in response to VF_SWITCH rail action.
 *
 * @param[in]  pRail        VOLT_RAIL object pointer
 * @param[in]  pListItem    Pointer to the voltage rail item in the input rail list.
 * @param[in]  bTrigger     Boolean to trigger the voltage switch into the HW
 * @param[in]  bWait        Boolean to wait for settle time after switch
 *
 * @return  FLCN_ERR_NOT_SUPPORTED
 *      Device object does not support this interface.
 * @return  FLCN_OK
 *      Requested target voltage was set successfully.
 */
static FLCN_STATUS
s_voltPolicyComputeVoltage_MULTI_RAIL_VF_SWITCH
(
    VOLT_RAIL *pRail,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM
              *pListItem,
    LwU32     *pTargetVoltuV
)
{
    LwS32           tmpTargetVoltuV;
    LwU32           maxVoltuV;
    FLCN_STATUS     status;

    status = s_voltPolicyRoundAndBound_MULTI_RAIL(pRail, pListItem, LW_TRUE, LW_TRUE, LW_TRUE, &maxVoltuV);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_voltPolicySetVoltage_MULTI_RAIL_VF_SWITCH_exit;
    }

    // Apply offset on requested voltage.
    tmpTargetVoltuV = (LwS32)pListItem->voltageuV;
#if PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_OFFSET)
    LwU8    clientOffsetType;
    for (clientOffsetType = 0; clientOffsetType < LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_MAX; clientOffsetType++)
    {
        tmpTargetVoltuV += pListItem->voltOffsetuV[clientOffsetType];
    }

    tmpTargetVoltuV = LW_MAX(0, tmpTargetVoltuV);
#endif // PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_OFFSET)

    // Avoided cast from composite expression to fix MISRA Rule 10.8.
    *pTargetVoltuV = (LwU32)tmpTargetVoltuV;

    // Make sure offsetted voltage doesn't exceed max limit.
    *pTargetVoltuV = LW_MIN(*pTargetVoltuV, maxVoltuV);

s_voltPolicySetVoltage_MULTI_RAIL_VF_SWITCH_exit:
    return status;
}

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
/*!
 * @brief Computes voltage based on the VOLT_POLICY in response to UNGATE rail action.
 *
 * @param[in]  pRail        VOLT_RAIL object pointer
 * @param[in]  pListItem    Pointer to the voltage rail item in the input rail list.
 *
 * @return  FLCN_ERR_NOT_SUPPORTED
 *      Device object does not support this interface.
 * @return  FLCN_OK
 *      Requested ungate action was performed successfully.
 */
static FLCN_STATUS
s_voltPolicyComputeVoltage_MULTI_RAIL_UNGATE
(
    VOLT_RAIL *pRail,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM
              *pListItem
)
{
    FLCN_STATUS status;

    //
    // While ungating, round the input voltage, bound and cap it so that it
    // doesn't exceed maximum voltage limit.
    //
    status = s_voltPolicyRoundAndBound_MULTI_RAIL(pRail, pListItem, LW_TRUE,
                LW_TRUE, LW_TRUE, NULL);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_voltPolicySetVoltage_MULTI_RAIL_UNGATE_exit;
    }

s_voltPolicySetVoltage_MULTI_RAIL_UNGATE_exit:
    return status;
}
#endif
