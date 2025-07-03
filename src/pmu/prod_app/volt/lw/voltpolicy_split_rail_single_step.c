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
 * @file  voltpolicy_split_rail_single_step.c
 * @brief VOLT Voltage Policy Model specific to Split Rail Single Step.
 *
 * This module is a collection of functions managing and manipulating state
 * related to the split rail single step voltage policy.
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "volt/objvolt.h"
#include "volt/voltpolicy.h"
#include "therm/objtherm.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Constructor for the VOLTAGE_POLICY_SPLIT_RAIL_SINGLE_STEP class.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
voltPolicyGrpIfaceModel10ObjSetImpl_SPLIT_RAIL_SINGLE_STEP
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    VOLT_POLICY_SPLIT_RAIL_SINGLE_STEP *pPolicy;
    FLCN_STATUS                         status;

    // Call super-class constructor.
    status = voltPolicyGrpIfaceModel10ObjSetImpl_SPLIT_RAIL(pModel10, ppBoardObj, size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        goto voltPolicyGrpIfaceModel10ObjSetImpl_SPLIT_RAIL_SINGLE_STEP_exit;
    }
    pPolicy = (VOLT_POLICY_SPLIT_RAIL_SINGLE_STEP *)*ppBoardObj;

    // Set member variables.
    pPolicy->pDevPrimary = voltRailDefaultVoltDevGet(pPolicy->super.pRailPrimary);
    pPolicy->pDevSecondary  = voltRailDefaultVoltDevGet(pPolicy->super.pRailSecondary);

voltPolicyGrpIfaceModel10ObjSetImpl_SPLIT_RAIL_SINGLE_STEP_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @copydoc VoltPolicySetVoltage
 */
FLCN_STATUS
voltPolicySetVoltage_SPLIT_RAIL_SINGLE_STEP
(
    VOLT_POLICY    *pPolicy,
    LwU8            count,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM
                   *pList
)
{
    VOLT_POLICY_SPLIT_RAIL_SINGLE_STEP *pPol      =
        (VOLT_POLICY_SPLIT_RAIL_SINGLE_STEP *)pPolicy;
    VOLT_RAIL                   *pRailPrimary      = pPol->super.pRailPrimary;
    VOLT_RAIL                   *pRailSecondary       = pPol->super.pRailSecondary;
    LwU8                         railActionPrimary;
    LwU8                         railActionSecondary;
    VOLT_POLICY_SPLIT_RAIL_TUPLE tuple;
    LwS32                        offsetVoltuV;
    FLCN_STATUS                  status;

    // Obtain the split rail tuple & target voltage offset for both the rails.
    status = voltPolicySplitRailTupleAndOffsetGet(pPolicy,
                count, pList, &tuple, &offsetVoltuV);
    if (status != FLCN_OK)
    {
        PMU_HALT();
        goto voltPolicySetVoltage_SPLIT_RAIL_SINGLE_STEP_exit;
    }

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
    railActionPrimary = tuple.railActionPrimary;
    railActionSecondary  = tuple.railActionSecondary;
#else
    railActionPrimary = LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_VF_SWITCH;
    railActionSecondary  = LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_VF_SWITCH;
#endif

    // Exit if current and target voltages for both rails are the same.
    if ((tuple.lwrrVoltPrimaryuV == tuple.targetVoltPrimaryuV) &&
        (tuple.lwrrVoltSecondaryuV == tuple.targetVoltSecondaryuV))
    {
        goto voltPolicySetVoltage_SPLIT_RAIL_SINGLE_STEP_exit;
    }

    // Set target voltages on both the rails w/o applying latch and settle time.
    if (tuple.lwrrVoltPrimaryuV != tuple.targetVoltPrimaryuV)
    {
        status = voltRailSetVoltage(pRailPrimary,
                    tuple.targetVoltPrimaryuV, LW_FALSE, LW_FALSE, railActionPrimary);
        if (status != FLCN_OK)
        {
            PMU_HALT();
            goto voltPolicySetVoltage_SPLIT_RAIL_SINGLE_STEP_exit;
        }
    }
    if (tuple.lwrrVoltSecondaryuV != tuple.targetVoltSecondaryuV)
    {
        status = voltRailSetVoltage(pRailSecondary,
                    tuple.targetVoltSecondaryuV, LW_FALSE, LW_FALSE, railActionSecondary);
        if (status != FLCN_OK)
        {
            PMU_HALT();
            goto voltPolicySetVoltage_SPLIT_RAIL_SINGLE_STEP_exit;
        }
    }

    //
    // Apply the atomic trigger for simultaneously changing voltages on one or
    // voltage rails. Consider either primary or secondary voltage rail for checking
    // voltage device type that is driving the underlying voltage rail. Atomic
    // trigger feature is dependent on the voltage device type.
    //
    switch (BOARDOBJ_GET_TYPE(pPol->pDevPrimary))
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
            goto voltPolicySetVoltage_SPLIT_RAIL_SINGLE_STEP_exit;
        }
    }

    //
    // Wait for the voltage to settle. Consider either primary or secondary voltage
    // rail for applying the voltage settle time.
    //
    voltDeviceSwitchDelayApply(pPol->pDevPrimary);

    //
    // Cache value of most recently requested voltage without offset for
    // primary and secondary rail.
    //
    pPol->super.lwrrVoltPrimaryuV =
        (LwU32)LW_MAX(0, ((LwS32)tuple.targetVoltPrimaryuV - offsetVoltuV));
    pPol->super.lwrrVoltSecondaryuV =
        (LwU32)LW_MAX(0, ((LwS32)tuple.targetVoltSecondaryuV - offsetVoltuV));

#if PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_OFFSET)
    // Cache the current voltage offset.
    pPol->super.super.offsetVoltLwrruV = offsetVoltuV;
#endif // PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_OFFSET)

voltPolicySetVoltage_SPLIT_RAIL_SINGLE_STEP_exit:
    return status;
}
