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
 * @file   voltpolicy_split_rail.h
 * @brief @copydoc voltpolicy_split_rail.c
 */

#ifndef VOLT_POLICY_SPLIT_RAIL_H
#define VOLT_POLICY_SPLIT_RAIL_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "volt/voltrail.h"
#include "volt/voltpolicy.h"

/* ------------------------- Macros ---------------------------------------- */
/* ------------------------- Datatypes ------------------------------------- */
typedef struct VOLT_POLICY_SPLIT_RAIL       VOLT_POLICY_SPLIT_RAIL;
typedef struct VOLT_POLICY_SPLIT_RAIL_TUPLE VOLT_POLICY_SPLIT_RAIL_TUPLE;

/*!
 * Extends VOLT_POLICY providing attributes common to all VOLT_POLICY_SPLIT_RAIL.
 */
struct VOLT_POLICY_SPLIT_RAIL
{
    /*!
     * VOLT_POLICY super class. This should always be the first member!
     */
    VOLT_POLICY         super;

    /*!
     * Pointer to the corresponding VOLT_RAIL object in the Voltage Rail Table
     * for primary rail.
     */
    VOLT_RAIL          *pRailPrimary;

    /*!
     * Pointer to the corresponding VOLT_RAIL object in the Voltage Rail Table
     * for secondary rail.
     */
    VOLT_RAIL          *pRailSecondary;

    /*!
     * VFE Equation Index for min voltage delta between secondary and primary rail.
     */
    LwVfeEquIdx         deltaMilwfeEquIdx;

    /*!
     * VFE Equation Index for max voltage delta between secondary and primary rail.
     */
    LwVfeEquIdx         deltaMaxVfeEquIdx;

    /*!
     * Boolean to indicate whether original delta constraints are violated.
     * The boolean is sticky and no code path clears it.
     */
    LwBool              bViolation;

    /*!
     * Offset for min voltage delta between secondary and primary rail.
     */
    LwS32               offsetDeltaMinuV;

    /*!
     * Offset for max voltage delta between secondary and primary rail.
     */
    LwS32               offsetDeltaMaxuV;

    /*!
     * Min voltage delta between secondary and primary rail.
     */
    LwS32               deltaMinuV;

    /*!
     * Max voltage delta between secondary and primary rail.
     */
    LwS32               deltaMaxuV;

    /*!
     * Original min voltage delta between secondary and primary rail.
     */
    LwS32               origDeltaMinuV;

    /*!
     * Original max voltage delta between secondary and primary rail.
     */
    LwS32               origDeltaMaxuV;

    /*!
     * Cached value of most recently requested voltage for primary rail without
     * applying any client offsets via PMU_VOLT_VOLT_POLICY_OFFSET or
     * PMU_VOLT_VOLT_RAIL_OFFSET feature.
     */
    LwU32               lwrrVoltPrimaryuV;

    /*!
     * Cached value of most recently requested voltage for secondary rail without
     * applying any client offsets via PMU_VOLT_VOLT_POLICY_OFFSET or
     * PMU_VOLT_VOLT_RAIL_OFFSET feature.
     */
    LwU32               lwrrVoltSecondaryuV;
};

/*!
 * Tuple consisting of current and target voltages for primary and secondary rail.
 */
struct VOLT_POLICY_SPLIT_RAIL_TUPLE
{
    /*!
     * Current voltage of the primary rail.
     */
    LwU32   lwrrVoltPrimaryuV;

    /*!
     * Current voltage of the secondary rail.
     */
    LwU32   lwrrVoltSecondaryuV;

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
    /*!
     * Rail action for the primary rail as per LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_<xyz>.
     */
    LwU8    railActionPrimary;

    /*!
     * Rail action for the secondary rail as per LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_<xyz>.
     */
    LwU8    railActionSecondary;
#endif

    /*!
     * Target voltage of the primary rail.
     */
    LwU32   targetVoltPrimaryuV;

    /*!
     * Target voltage of the secondary rail.
     */
    LwU32   targetVoltSecondaryuV;
};

/* ------------------------- Defines ---------------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
BoardObjGrpIfaceModel10ObjSet                  (voltPolicyGrpIfaceModel10ObjSetImpl_SPLIT_RAIL)
    GCC_ATTRIB_SECTION("imem_libVoltConstruct", "voltPolicyGrpIfaceModel10ObjSetImpl_SPLIT_RAIL");

BoardObjIfaceModel10GetStatus                      (voltPolicyIfaceModel10GetStatus_SPLIT_RAIL);

FLCN_STATUS                         voltPolicySplitRailPrimarySecondaryListIdxGet(VOLT_POLICY *pPolicy, LwU8 count, LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM *pList, LwU8 *pListIdxPrimary, LwU8 *pListIdxSecondary);

FLCN_STATUS                         voltPolicySplitRailPrimarySecondaryRoundVoltage(VOLT_POLICY *pPolicy, LwU32 *pRailPrimaryuV, LwU32 *pRailSecondaryuV);

FLCN_STATUS                         voltPolicySplitRailTupleAndOffsetGet(VOLT_POLICY *pPolicy, LwU8 count, LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM *pList, VOLT_POLICY_SPLIT_RAIL_TUPLE *pTuple, LwS32 *pOffsetVoltuV);

VoltPolicyLoad                     (voltPolicyLoad_SPLIT_RAIL);

VoltPolicySanityCheck              (voltPolicySanityCheck_SPLIT_RAIL);

VoltPolicyDynamilwpdate            (voltPolicyDynamilwpdate_SPLIT_RAIL);

VoltPolicyOffsetVoltage            (voltPolicyOffsetVoltage_SPLIT_RAIL);

VoltPolicyOffsetRangeGetInternal   (voltPolicyOffsetRangeGetInternal_SPLIT_RAIL)
    GCC_ATTRIB_SECTION("imem_libVoltApi", "voltPolicyOffsetRangeGetInternal_SPLIT_RAIL");

/* ------------------------ Include Derived Types -------------------------- */
#include "volt/voltpolicy_split_rail_multi_step.h"
#include "volt/voltpolicy_split_rail_single_step.h"

#endif // VOLT_POLICY_SPLIT_RAIL_H
