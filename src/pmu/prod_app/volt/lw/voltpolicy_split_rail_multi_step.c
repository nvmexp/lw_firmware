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
 * @file  voltpolicy_split_rail_multi_step.c
 * @brief VOLT Voltage Policy Model specific to Split Rail Multi Step.
 *
 * This module is a collection of functions managing and manipulating state
 * related to the split rail multi step voltage policy.
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "volt/objvolt.h"
#include "volt/voltpolicy.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Constructor for the VOLTAGE_POLICY_SPLIT_RAIL_MULTI_STEP class.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
voltPolicyGrpIfaceModel10ObjSetImpl_SPLIT_RAIL_MULTI_STEP
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    RM_PMU_VOLT_VOLT_POLICY_SPLIT_RAIL_MULTI_STEP_BOARDOBJ_SET  *pSet    =
        (RM_PMU_VOLT_VOLT_POLICY_SPLIT_RAIL_MULTI_STEP_BOARDOBJ_SET *)pBoardObjSet;
    VOLT_POLICY_SPLIT_RAIL_MULTI_STEP  *pPolicy;
    FLCN_STATUS                         status;

    // Call super-class constructor.
    status = voltPolicyGrpIfaceModel10ObjSetImpl_SPLIT_RAIL(pModel10, ppBoardObj, size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        goto voltPolicyGrpIfaceModel10ObjSetImpl_SPLIT_RAIL_MULTI_STEP_exit;
    }
    pPolicy = (VOLT_POLICY_SPLIT_RAIL_MULTI_STEP *)*ppBoardObj;

    // Set member variables.
    pPolicy->interSwitchDelayus = pSet->interSwitchDelayus;

voltPolicyGrpIfaceModel10ObjSetImpl_SPLIT_RAIL_MULTI_STEP_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @copydoc VoltPolicySetVoltage
 */
FLCN_STATUS
voltPolicySetVoltage_SPLIT_RAIL_MULTI_STEP
(
    VOLT_POLICY    *pPolicy,
    LwU8            count,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM
                   *pList
)
{
    VOLT_POLICY_SPLIT_RAIL_MULTI_STEP *pPol  =
        (VOLT_POLICY_SPLIT_RAIL_MULTI_STEP *)pPolicy;
    VOLT_RAIL                   *pRailPrimary = pPol->super.pRailPrimary;
    VOLT_RAIL                   *pRailSecondary  = pPol->super.pRailSecondary;
    LwU8                         railAction  = LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_VF_SWITCH;
    VOLT_POLICY_SPLIT_RAIL_TUPLE tuple;
    LwS32                        offsetVoltuV;
    LwBool                       bPrimaryIncreasing;
    LwBool                       bSecondaryIncreasing;
    LwBool                       bPrimary;
    FLCN_STATUS                  status;

    // Obtain the split rail tuple & target voltage offset for both the rails.
    status = voltPolicySplitRailTupleAndOffsetGet(pPolicy,
                count, pList, &tuple, &offsetVoltuV);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltPolicySetVoltage_SPLIT_RAIL_MULTI_STEP_exit;
    }

    // Determine whether the rail voltages are increasing or decreasing.
    bPrimaryIncreasing = (tuple.targetVoltPrimaryuV > tuple.lwrrVoltPrimaryuV);
    bSecondaryIncreasing = (tuple.targetVoltSecondaryuV > tuple.lwrrVoltSecondaryuV);

    // The initial voltage switch depends on direction of primary rail.
    bPrimary = !bPrimaryIncreasing;

    // Change voltage in steps until both rails reach their target voltages.
    while ((tuple.lwrrVoltPrimaryuV != tuple.targetVoltPrimaryuV) ||
           (tuple.lwrrVoltSecondaryuV != tuple.targetVoltSecondaryuV))
    {
        LwBool      bSwitch = LW_FALSE;
        VOLT_RAIL  *pRail;
        LwU32       voltuV;

        if (bPrimary)
        {
            // Change primary rail voltage if it hasn't reached its target yet.
            if (tuple.lwrrVoltPrimaryuV != tuple.targetVoltPrimaryuV)
            {
                if (bPrimaryIncreasing)
                {
                    //
                    // While increasing, only min delta constraint needs to be
                    // checked as max delta constraint won't get violated.
                    //
                    tuple.lwrrVoltPrimaryuV =
                        LW_MIN(tuple.targetVoltPrimaryuV,
                                ((LwU32)((LwS32)tuple.lwrrVoltSecondaryuV - pPol->super.deltaMinuV)));
                }
                else
                {
                    //
                    // While decreasing, only max delta constraint needs to be
                    // checked as min delta constraint won't get violated.
                    //
                    tuple.lwrrVoltPrimaryuV =
                        LW_MAX(tuple.targetVoltPrimaryuV,
                                ((LwU32)((LwS32)tuple.lwrrVoltSecondaryuV - pPol->super.deltaMaxuV)));
                }

                // Switch the primary rail to its intermediate or final value.
                bSwitch    = LW_TRUE;
                pRail      = pRailPrimary;
                voltuV     = tuple.lwrrVoltPrimaryuV;
#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
                railAction = tuple.railActionPrimary;
#endif
            }
        }
        else
        {
            // Change secondary rail voltage if it hasn't reached its target yet.
            if (tuple.lwrrVoltSecondaryuV != tuple.targetVoltSecondaryuV)
            {
                if (bSecondaryIncreasing)
                {
                    //
                    // While increasing, only max delta constraint needs to be
                    // checked as min delta constraint won't get violated.
                    //
                    tuple.lwrrVoltSecondaryuV =
                        LW_MIN(tuple.targetVoltSecondaryuV,
                                ((LwU32)((LwS32)tuple.lwrrVoltPrimaryuV + pPol->super.deltaMaxuV)));
                }
                else
                {
                    //
                    // While decreasing, only min delta constraint needs to be
                    // checked as max delta constraint won't get violated.
                    //
                    tuple.lwrrVoltSecondaryuV =
                        LW_MAX(tuple.targetVoltSecondaryuV,
                                ((LwU32)((LwS32)tuple.lwrrVoltPrimaryuV + pPol->super.deltaMinuV)));
                }

                // Switch the secondary rail to its intermediate or final value.
                bSwitch    = LW_TRUE;
                pRail      = pRailSecondary;
                voltuV     = tuple.lwrrVoltSecondaryuV;
#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
                railAction = tuple.railActionSecondary;
#endif
            }
        }

        if (bSwitch)
        {
            // Check whether this is the intermediate switch.
            LwBool  bInterSwitch =
                ((tuple.lwrrVoltPrimaryuV != tuple.targetVoltPrimaryuV) ||
                 (tuple.lwrrVoltSecondaryuV != tuple.targetVoltSecondaryuV));

            // Change the appropriate rail voltage.
            status = voltRailSetVoltage(pRail, voltuV, LW_TRUE, !bInterSwitch, railAction);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto voltPolicySetVoltage_SPLIT_RAIL_MULTI_STEP_exit;
            }

            // Apply intermediate voltage switch settle time.
            if ((bInterSwitch) &&
                (pPol->interSwitchDelayus != 0))
            {
                OS_TIMER_WAIT_US(pPol->interSwitchDelayus);
            }
        }

        // Flip the rail for the next cycle.
        bPrimary = !bPrimary;
    }

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

voltPolicySetVoltage_SPLIT_RAIL_MULTI_STEP_exit:
    return status;
}
