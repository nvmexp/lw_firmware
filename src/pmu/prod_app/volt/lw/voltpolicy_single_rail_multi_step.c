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
 * @file  voltpolicy_single_rail_multi_step.c
 * @brief VOLT Voltage Policy Model specific to single rail multi step.
 *
 * This module is a collection of functions managing and manipulating state
 * related to the single rail multi step voltage policy.
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
 * Constructor for the VOLTAGE_POLICY_SINGLE_RAIL_MULTI_STEP class.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
voltPolicyGrpIfaceModel10ObjSetImpl_SINGLE_RAIL_MULTI_STEP
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    RM_PMU_VOLT_VOLT_POLICY_SINGLE_RAIL_MULTI_STEP_BOARDOBJ_SET   *pSet =
        (RM_PMU_VOLT_VOLT_POLICY_SINGLE_RAIL_MULTI_STEP_BOARDOBJ_SET *)pBoardObjSet;
    VOLT_POLICY_SINGLE_RAIL_MULTI_STEP                            *pPolicy;
    FLCN_STATUS                                                    status;

    // Call super-class constructor.
    status = voltPolicyGrpIfaceModel10ObjSetImpl_SINGLE_RAIL(pModel10, ppBoardObj, size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        goto voltPolicyGrpIfaceModel10ObjSetImpl_SINGLE_RAIL_MULTI_STEP_exit;
    }
    pPolicy = (VOLT_POLICY_SINGLE_RAIL_MULTI_STEP *)*ppBoardObj;

    // Set member variables.
    pPolicy->rampUpStepSizeuV   = pSet->rampUpStepSizeuV;
    pPolicy->rampDownStepSizeuV = pSet->rampDownStepSizeuV;
    pPolicy->interSwitchDelayus = pSet->interSwitchDelayus;

voltPolicyGrpIfaceModel10ObjSetImpl_SINGLE_RAIL_MULTI_STEP_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @copydoc VoltPolicyLoad
 */
FLCN_STATUS
voltPolicyLoad_SINGLE_RAIL_MULTI_STEP
(
    VOLT_POLICY    *pPolicy
)
{
    VOLT_POLICY_SINGLE_RAIL_MULTI_STEP *pPol  =
        (VOLT_POLICY_SINGLE_RAIL_MULTI_STEP *)pPolicy;
    VOLT_RAIL                          *pRail = pPol->super.pRail;
    FLCN_STATUS                         status;

    // Call SUPER-class implementation.
    status = voltPolicyLoad_SINGLE_RAIL(pPolicy);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltPolicyLoad_SINGLE_RAIL_MULTI_STEP_exit;
    }

    // Round down the voltage ramp up step value.
    status = voltRailRoundVoltage(pRail, (LwS32 *)&pPol->rampUpStepSizeuV,
                LW_FALSE, LW_FALSE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltPolicyLoad_SINGLE_RAIL_MULTI_STEP_exit;
    }

    // Round down the voltage ramp down step value.
    status = voltRailRoundVoltage(pRail, (LwS32 *)&pPol->rampDownStepSizeuV,
                LW_FALSE, LW_FALSE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltPolicyLoad_SINGLE_RAIL_MULTI_STEP_exit;
    }

voltPolicyLoad_SINGLE_RAIL_MULTI_STEP_exit:
    return status;
}

/*!
 * @copydoc VoltPolicySetVoltage
 */
FLCN_STATUS
voltPolicySetVoltage_SINGLE_RAIL_MULTI_STEP
(
    VOLT_POLICY    *pPolicy,
    LwU8            count,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM
                   *pList
)
{
    VOLT_POLICY_SINGLE_RAIL_MULTI_STEP *pPol         =
        (VOLT_POLICY_SINGLE_RAIL_MULTI_STEP *)pPolicy;
    VOLT_RAIL                          *pRail        = pPol->super.pRail;
    LwBool                              bInterSwitch = LW_FALSE;
    LwBool                              bIncreasing;
    LwS32                               tmpTargetVoltuV;
    LwU32                               targetVoltuV;
    LwU32                               maxVoltuV;
    LwU32                               voltStepuV;
    LwU32                               lwrrVoltuV;
    LwU32                               voltuV;
    FLCN_STATUS                         status;

    // Round up the requested target voltage.
    status = voltRailRoundVoltage(pRail, (LwS32 *)&pList->voltageuV,
                LW_TRUE, LW_TRUE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltPolicySetVoltage_SINGLE_RAIL_MULTI_STEP_exit;
    }

    // Obtain the max allowed rail voltage.
    status = voltRailGetVoltageMax(pRail, &maxVoltuV);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltPolicySetVoltage_SINGLE_RAIL_MULTI_STEP_exit;
    }

    // Make sure requested voltage doesn't exceed max limit.
    pList->voltageuV = LW_MIN(pList->voltageuV, maxVoltuV);

    // Apply offset on requested voltage.
#if PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_OFFSET)
    tmpTargetVoltuV =
            LW_MAX(0, ((LwS32)pList->voltageuV + pPolicy->offsetVoltRequV));

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

    // Make sure requested voltage doesn't exceed max limit.
    targetVoltuV = LW_MIN(targetVoltuV, maxVoltuV);

    // Obtain current rail voltage via internal wrapper function.
    status = voltRailGetVoltageInternal(pRail, &lwrrVoltuV);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto voltPolicySetVoltage_SINGLE_RAIL_MULTI_STEP_exit;
    }

    // Determine the voltage switching direction.
    if (targetVoltuV < lwrrVoltuV)
    {
        bIncreasing = LW_FALSE;
        voltStepuV = pPol->rampDownStepSizeuV;
    }
    else
    {
        bIncreasing = LW_TRUE;
        voltStepuV = pPol->rampUpStepSizeuV;
    }

    // Change the rail voltage in multiple small steps.
    while (targetVoltuV != lwrrVoltuV)
    {
        // Apply voltage in steps only if requested.
        if (voltStepuV != RM_PMU_VOLT_VALUE_0V_IN_UV)
        {
            if (bIncreasing)
            {
                voltuV = LW_MIN(targetVoltuV, (lwrrVoltuV + voltStepuV));
            }
            else
            {
                voltuV = LW_MAX(targetVoltuV, (lwrrVoltuV - voltStepuV));
            }
        }
        else // Apply voltage in a single step.
        {
            voltuV = targetVoltuV;
        }

        // Check whether this is the intermediate switch.
        if (voltuV != targetVoltuV)
        {
            bInterSwitch = LW_TRUE;
        }

        // Change the rail voltage.
        status = voltRailSetVoltage(pRail, voltuV, LW_TRUE, !bInterSwitch, pList->railAction);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto voltPolicySetVoltage_SINGLE_RAIL_MULTI_STEP_exit;
        }

        lwrrVoltuV = voltuV;

        // Apply intermediate voltage switch settle time if required.
        if ((bInterSwitch) &&
            (pPol->interSwitchDelayus != 0))
        {
            OS_TIMER_WAIT_US(pPol->interSwitchDelayus);
        }
    }

    // Cache value of most recently requested voltage without offset.
    pPol->super.lwrrVoltuV = pList->voltageuV;

#if PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_OFFSET)
    // Cache the current voltage offset.
    pPol->super.super.offsetVoltLwrruV = (LwS32)(targetVoltuV - pList->voltageuV);
#endif // PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_OFFSET)

voltPolicySetVoltage_SINGLE_RAIL_MULTI_STEP_exit:
    return status;
}
