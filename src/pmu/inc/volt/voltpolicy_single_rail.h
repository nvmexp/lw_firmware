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
 * @file   voltpolicy_single_rail.h
 * @brief @copydoc voltpolicy_single_rail.c
 */

#ifndef VOLTPOLICY_SINGLE_RAIL_H
#define VOLTPOLICY_SINGLE_RAIL_H

#include "g_voltpolicy_single_rail.h"

#ifndef G_VOLTPOLICY_SINGLE_RAIL_H
#define G_VOLTPOLICY_SINGLE_RAIL_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "volt/voltrail.h"
#include "volt/voltpolicy.h"

/* ------------------------- Macros ---------------------------------------- */
/*!
 * @brief   Colwenience macro for getting the vtable for the VOLT_POLICY_SINGLE_RAIL
 *          class type.
 *
 * @return  Pointer to the location of the VOLT_POLICY_SINGLE_RAIL class vtable.
 * @return  NULL if the vtable is not enabled in the build.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_SINGLE_RAIL)
#define VOLT_VOLT_POLICY_SINGLE_RAIL_VTABLE() &VoltPolicySingleRailVirtualTable
extern BOARDOBJ_VIRTUAL_TABLE VoltPolicySingleRailVirtualTable;
#else
#define VOLT_VOLT_POLICY_SINGLE_RAIL_VTABLE() NULL
#endif

/* ------------------------- Datatypes ------------------------------------- */
typedef struct VOLT_POLICY_SINGLE_RAIL VOLT_POLICY_SINGLE_RAIL;

/*!
 * @brief Extends @ref VOLT_POLICY providing attributes common to all @ref VOLT_POLICY_SINGLE_RAIL.
 */
struct VOLT_POLICY_SINGLE_RAIL
{
    /*!
     * @brief @ref VOLT_POLICY super class. This should always be the first member!
     */
    VOLT_POLICY super;

    /*!
     * @brief Pointer to the corresponding @ref VOLT_RAIL object in the Voltage Rail Table.
     */
    VOLT_RAIL  *pRail;

    /*!
     * @brief Cached value of most recently requested voltage without applying
     * any client offsets via PMU_VOLT_VOLT_POLICY_OFFSET or PMU_VOLT_VOLT_RAIL_OFFSET feature.
     */
    LwU32       lwrrVoltuV;
};

/* ------------------------- Defines --------------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
BoardObjGrpIfaceModel10ObjSet                (voltPolicyGrpIfaceModel10ObjSetImpl_SINGLE_RAIL)
    GCC_ATTRIB_SECTION("imem_libVoltConstruct", "voltPolicyGrpIfaceModel10ObjSetImpl_SINGLE_RAIL");

BoardObjIfaceModel10GetStatus                    (voltPolicyIfaceModel10GetStatus_SINGLE_RAIL);

mockable VoltPolicyLoad          (voltPolicyLoad_SINGLE_RAIL);

VoltPolicySanityCheck            (voltPolicySanityCheck_SINGLE_RAIL);

mockable VoltPolicySetVoltage    (voltPolicySetVoltage_SINGLE_RAIL);

VoltPolicyOffsetVoltage          (voltPolicyOffsetVoltage_SINGLE_RAIL);

VoltPolicyOffsetRangeGetInternal (voltPolicyOffsetRangeGetInternal_SINGLE_RAIL)
    GCC_ATTRIB_SECTION("imem_libVoltApi", "voltPolicyOffsetRangeGetInternal_SINGLE_RAIL");

/* ------------------------ Include Derived Types -------------------------- */

#endif // G_VOLTPOLICY_SINGLE_RAIL_H
#endif // VOLT_POLICY_SINGLE_RAIL_H
