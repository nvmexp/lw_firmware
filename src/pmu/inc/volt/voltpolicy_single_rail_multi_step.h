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
 * @file   voltpolicy_single_rail_multi_step.h
 * @brief @copydoc voltpolicy_single_rail_multi_step.c
 */

#ifndef VOLT_POLICY_SINGLE_RAIL_MULTI_STEP_H
#define VOLT_POLICY_SINGLE_RAIL_MULTI_STEP_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "volt/voltpolicy.h"

/* ------------------------- Macros ---------------------------------------- */
#define voltPolicyIfaceModel10GetStatus_SINGLE_RAIL_MULTI_STEP(pModel10, pPayload)                                 \
    voltPolicyIfaceModel10GetStatus_SINGLE_RAIL((pModel10), (pPayload))
#define voltPolicyOffsetVoltage_SINGLE_RAIL_MULTI_STEP(pPolicy)                                     \
    voltPolicyOffsetVoltage_SINGLE_RAIL((pPolicy))
#define voltPolicyOffsetRangeGetInternal_SINGLE_RAIL_MULTI_STEP(pPolicy, pRailList, pRailOffsetList)   \
    voltPolicyOffsetRangeGetInternal_SINGLE_RAIL((pPolicy), (pRailList), (pRailOffsetList))
#define voltPolicySanityCheck_SINGLE_RAIL_MULTI_STEP(pPolicy, count, pList)                         \
    voltPolicySanityCheck_SINGLE_RAIL((pPolicy), (count), (pList))

/* ------------------------- Datatypes ------------------------------------- */
typedef struct VOLT_POLICY_SINGLE_RAIL_MULTI_STEP VOLT_POLICY_SINGLE_RAIL_MULTI_STEP;

/*!
 * Extends VOLT_POLICY_SINGLE_RAIL providing attributes common to all VOLT_POLICY_SINGLE_RAIL_MULTI_STEP.
 */
struct VOLT_POLICY_SINGLE_RAIL_MULTI_STEP
{
    /*!
     * VOLT_POLICY super class. This should always be the first member!
     */
    VOLT_POLICY_SINGLE_RAIL super;

    /*!
     * Settle time in microseconds for intermediate voltage switches.
     */
    LwU16   interSwitchDelayus;

    /*!
     * Ramp up step size in uV.
     * A value of @ref RM_PMU_VOLT_VALUE_0V_IN_UV indicates that ramp isn't required.
     */
    LwU32   rampUpStepSizeuV;

    /*!
     * Ramp down step size in uV.
     * A value of @ref RM_PMU_VOLT_VALUE_0V_IN_UV indicates that ramp isn't required.
     */
    LwU32   rampDownStepSizeuV;
};

/* ------------------------- Defines --------------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
BoardObjGrpIfaceModel10ObjSet       (voltPolicyGrpIfaceModel10ObjSetImpl_SINGLE_RAIL_MULTI_STEP)
    GCC_ATTRIB_SECTION("imem_libVoltConstruct", "voltPolicyGrpIfaceModel10ObjSetImpl_SINGLE_RAIL_MULTI_STEP");

VoltPolicyLoad          (voltPolicyLoad_SINGLE_RAIL_MULTI_STEP);

VoltPolicySetVoltage    (voltPolicySetVoltage_SINGLE_RAIL_MULTI_STEP);

/* ------------------------ Include Derived Types -------------------------- */

#endif // VOLT_POLICY_SINGLE_RAIL_MULTI_STEP_H
