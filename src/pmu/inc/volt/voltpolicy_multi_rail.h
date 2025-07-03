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
 * @file   voltpolicy_multi_rail.h
 * @brief @copydoc voltpolicy_multi_rail.c
 */

#ifndef VOLT_POLICY_MULTI_RAIL_H
#define VOLT_POLICY_MULTI_RAIL_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "volt/voltrail.h"
#include "volt/voltpolicy.h"

/* ------------------------- Macros ---------------------------------------- */
/* ------------------------- Datatypes ------------------------------------- */
typedef struct VOLT_POLICY_MULTI_RAIL VOLT_POLICY_MULTI_RAIL;

/*!
 * @brief Extends @ref VOLT_POLICY providing attributes common to all @ref VOLT_POLICY_MULTI_RAIL.
 */
struct VOLT_POLICY_MULTI_RAIL
{
    /*!
     * @brief @ref VOLT_POLICY super class. This should always be the first member!
     */
    VOLT_POLICY super;

    /*!
     * Mask of VOLT_RAILs for programming voltage.
     */
    BOARDOBJGRPMASK_E32
                voltRailMask;

    /*!
     * Voltage data for this policy.
     */
    LW2080_CTRL_VOLT_VOLT_POLICY_STATUS_DATA_MULTI_RAIL
                voltData;
};

/* ------------------------- Defines --------------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
BoardObjGrpIfaceModel10ObjSet                (voltPolicyGrpIfaceModel10ObjSetImpl_MULTI_RAIL)
    GCC_ATTRIB_SECTION("imem_libVoltConstruct", "voltPolicyGrpIfaceModel10ObjSetImpl_MULTI_RAIL");

VoltPolicyLoad                   (voltPolicyLoad_MULTI_RAIL);

VoltPolicySanityCheck            (voltPolicySanityCheck_MULTI_RAIL);

VoltPolicySetVoltage             (voltPolicySetVoltage_MULTI_RAIL);

VoltPolicyOffsetVoltage          (voltPolicyOffsetVoltage_MULTI_RAIL);

VoltPolicyOffsetRangeGetInternal (voltPolicyOffsetRangeGetInternal_MULTI_RAIL)
    GCC_ATTRIB_SECTION("imem_libVoltApi", "voltPolicyOffsetRangeGetInternal_MULTI_RAIL");

BoardObjIfaceModel10GetStatus                    (voltPolicyIfaceModel10GetStatus_MULTI_RAIL);

/* ------------------------ Include Derived Types -------------------------- */

#endif // VOLT_POLICY_MULTI_RAIL_H
