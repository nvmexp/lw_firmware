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
 * @file   voltpolicy_split_rail_multi_step.h
 * @brief @copydoc voltpolicy_split_rail_multi_step.c
 */

#ifndef VOLTPOLICY_SPLIT_RAIL_MULTI_STEP_H
#define VOLTPOLICY_SPLIT_RAIL_MULTI_STEP_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "volt/voltpolicy.h"

/* ------------------------- Macros ---------------------------------------- */
#define voltPolicyLoad_SPLIT_RAIL_MULTI_STEP(pPol)                                              \
    voltPolicyLoad_SPLIT_RAIL(pPol)
#define voltPolicySanityCheck_SPLIT_RAIL_MULTI_STEP(pPol, count, pList)                         \
    voltPolicySanityCheck_SPLIT_RAIL(pPol, count, pList)
#define voltPolicyDynamilwpdate_SPLIT_RAIL_MULTI_STEP(pPol)                                     \
    voltPolicyDynamilwpdate_SPLIT_RAIL(pPol)
#define voltPolicyOffsetVoltage_SPLIT_RAIL_MULTI_STEP(pPol)                                     \
    voltPolicyOffsetVoltage_SPLIT_RAIL(pPol)
#define voltPolicyOffsetRangeGetInternal_SPLIT_RAIL_MULTI_STEP(pPol, pRailList, pRailOffsetList)   \
    voltPolicyOffsetRangeGetInternal_SPLIT_RAIL((pPol), (pRailList), (pRailOffsetList))

/* ------------------------- Datatypes ------------------------------------- */
typedef struct VOLT_POLICY_SPLIT_RAIL_MULTI_STEP VOLT_POLICY_SPLIT_RAIL_MULTI_STEP;

/*!
 * Extends VOLT_POLICY_SPLIT_RAIL providing attributes common to all
 * VOLT_POLICY_SPLIT_RAIL_MULTI_STEP.
 */
struct VOLT_POLICY_SPLIT_RAIL_MULTI_STEP
{
    /*!
     * VOLT_POLICY_SPLIT_RAIL super class. This should always be the first member!
     */
    VOLT_POLICY_SPLIT_RAIL   super;

    /*!
     * Settle time in microseconds for intermediate voltage switches.
     */
    LwU16   interSwitchDelayus;
};

/* ------------------------- Defines ---------------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
BoardObjGrpIfaceModel10ObjSet       (voltPolicyGrpIfaceModel10ObjSetImpl_SPLIT_RAIL_MULTI_STEP)
    GCC_ATTRIB_SECTION("imem_libVoltConstruct", "voltPolicyGrpIfaceModel10ObjSetImpl_SPLIT_RAIL_MULTI_STEP");

VoltPolicySetVoltage    (voltPolicySetVoltage_SPLIT_RAIL_MULTI_STEP);

/* ------------------------ Include Derived Types -------------------------- */

#endif // VOLTPOLICY_SPLIT_RAIL_MULTI_STEP_H
