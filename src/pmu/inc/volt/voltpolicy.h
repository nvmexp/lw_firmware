/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   voltpolicy.h
 * @brief  VOLT Voltage Policy Model
 *
 * This module is a collection of functions managing and manipulating state
 * related to the volt policies.
 */

#ifndef VOLTPOLICY_H
#define VOLTPOLICY_H

#include "g_voltpolicy.h"

#ifndef G_VOLTPOLICY_H
#define G_VOLTPOLICY_H
/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------- Macros ---------------------------------------- */
/*!
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_VOLT_POLICY \
    (&(Volt.policies.super))

/*!
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define VOLT_POLICY_GET(_objIdx) \
    (BOARDOBJGRP_OBJ_GET(VOLT_POLICY, (_objIdx)))

/* ------------------------- Datatypes ------------------------------------- */
typedef struct VOLT_POLICY          VOLT_POLICY, VOLT_POLICY_BASE;
typedef struct VOLT_POLICY_METADATA VOLT_POLICY_METADATA;

/*!
 * @brief Loads a VOLT_POLICY.
 *
 * @param[in]  pPolicy  VOLT_POLICY object pointer
 *
 * @return FLCN_OK
 *      VOLT_POLICY loaded successfully.
 */
#define VoltPolicyLoad(fname) FLCN_STATUS (fname)(VOLT_POLICY *pPolicy)

/*!
 * @brief Sanity check voltage on the one or more VOLT_RAILs according to the
 * appropriate VOLT_POLICY.
 *
 * @param[in]  pPolicy  VOLT_POLICY object pointer
 * @param[in]  count    Number of items in the list
 * @param[in]  pList    List containing target voltage for the rails
 *
 * @return  FLCN_ERR_NOT_SUPPORTED
 *      Policy object does not support this interface.
 * @return  FLCN_ERR_ILWALID_ARGUMENT
 *      Invalid input parameters.
 * @return  FLCN_OK
 *      Requested target voltages were sanitized successfully.
 */
#define VoltPolicySanityCheck(fname) FLCN_STATUS (fname)(VOLT_POLICY *pPolicy, LwU8 count, LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM *pList)

/*!
 * @brief Sets voltage on the one or more VOLT_RAILs according to the
 * appropriate VOLT_POLICY.
 *
 * @param[in]  pPolicy  VOLT_POLICY object pointer
 * @param[in]  count    Number of items in the list
 * @param[in]  pList    List containing target voltage for the rails
 *
 * @return  FLCN_ERR_NOT_SUPPORTED
 *      Policy object does not support this interface.
 * @return  FLCN_ERR_ILWALID_ARGUMENT
 *      Invalid input parameters.
 * @return  FLCN_OK
 *      Requested target voltages were set successfully.
 */
#define VoltPolicySetVoltage(fname) FLCN_STATUS (fname)(VOLT_POLICY *pPolicy, LwU8 count, LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM *pList)

/*!
 * @brief Update VOLT_POLICY dynamic parameters that depend on VFE.
 *
 * @param[in]  pPolicy  VOLT_POLICY object pointer
 *
 * @return FLCN_OK
 *      Dynamic update successfully completed.
 * @return Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
#define VoltPolicyDynamilwpdate(fname) FLCN_STATUS (fname)(VOLT_POLICY *pPolicy)

/*!
 * @brief Offsets voltage on the one or more VOLT_RAILs according to the
 * appropriate VOLT_POLICY.
 *
 * @param[in]  pPolicy      VOLT_POLICY object pointer
 *
 * @return  FLCN_ERR_NOT_SUPPORTED
 *      Policy object does not support this interface.
 * @return  FLCN_ERR_ILWALID_ARGUMENT
 *      Invalid input parameters.
 * @return  FLCN_OK
 *      Requested offset successfully applied to one or more VOLT_RAILs.
 */
#define VoltPolicyOffsetVoltage(fname) FLCN_STATUS (fname)(VOLT_POLICY *pPolicy)

/*!
 * @brief Compute the maximum positive and maximum negative voltage offset that can be applied to a set of
 * voltage rails.
 *
 * @param[in]   VOLT_POLICY
 *      Pointer to the voltage policy.
 * @param[in]   pList
 *      List containing target voltage for the rails
 * @param[out]  pRailOffsetList
 *      Pointer that holds the list to store voltage offset values for the VOLT_RAILs.
 *
 * @return  FLCN_ERR_ILWALID_ARGUMENT
 *      Invalid input parameters.
 * @return  FLCN_OK
 *      Max positive and max negative offset was computed successfully.
 * @return  Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
#define VoltPolicyOffsetRangeGetInternal(fname) FLCN_STATUS (fname)(VOLT_POLICY *pVoltPolicy, LW2080_CTRL_VOLT_VOLT_RAIL_LIST *pList, VOLT_RAIL_OFFSET_RANGE_LIST *pRailOffsetList)

/*!
 * @brief Extends @ref BOARDOBJ providing attributes common to all VOLT_POLICIES.
 */
struct VOLT_POLICY
{
    /*!
     * @brief @ref BOARDOBJ super-class.
     */
    BOARDOBJ    super;

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_OFFSET))
    /*!
     * @brief Cached value of requested voltage offset to be applied on one or more
     * VOLT_RAILs according to the appropriate VOLT_POLICY.
     */
    LwS32       offsetVoltRequV;

    /*!
     * @brief Cached value of current voltage offset that is applied on one or more
     * VOLT_RAILs according to the appropriate @ref VOLT_POLICY.
     * Access to this member is synchronized by PMU_VOLT_SEMAPHORE feature
     * since it is read/written by/from multiple tasks.
     */
    LwS32       offsetVoltLwrruV;
#endif // PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_OFFSET)
};

/*!
 * @brief Structure to store some metadata about VOLT_POLICY-s.
 */
struct VOLT_POLICY_METADATA
{
    /*!
     * @brief Voltage Policy Table Index for Perf Core VF Sequence client.
     *
     * @note LW2080_CTRL_VOLT_VOLT_POLICY_INDEX_ILWALID indicates that
     * this policy is not present/specified.
     */
    LwU8    perfCoreVFSeqPolicyIdx;
};

/* ------------------------- Defines ---------------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
BoardObjGrpIfaceModel10CmdHandler   (voltPolicyBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_libVoltConstruct", "voltPolicyBoardObjGrpIfaceModel10Set");

BoardObjGrpIfaceModel10ObjSet       (voltPolicyGrpIfaceModel10ObjSetImpl_SUPER)
    GCC_ATTRIB_SECTION("imem_libVoltConstruct", "voltPolicyGrpIfaceModel10ObjSetImpl_SUPER");

FLCN_STATUS             voltPoliciesLoad(void);

mockable FLCN_STATUS             voltPoliciesDynamilwpdate(void);

BoardObjGrpIfaceModel10CmdHandler   (voltPolicyBoardObjGrpIfaceModel10GetStatus);

BoardObjIfaceModel10GetStatus           (voltPolicyIfaceModel10GetStatus);

FLCN_STATUS             voltPolicyOffsetVoltage(LwU8 policyIdx, LwS32 offsetVoltuV);

FLCN_STATUS             voltPolicyOffsetGet(LwU8 policyIdx, LwS32 *pOffsetVoltuV)
    GCC_ATTRIB_SECTION("imem_libVoltApi", "voltPolicyOffsetGet");

FLCN_STATUS             voltPolicyOffsetRangeGet(LwU8 clientID, LW2080_CTRL_VOLT_VOLT_RAIL_LIST *pRailList, VOLT_RAIL_OFFSET_RANGE_LIST *pRailOffsetList)
    GCC_ATTRIB_SECTION("imem_libVoltApi", "voltPolicyOffsetRangeGet");

VoltPolicyOffsetRangeGetInternal   (voltPolicyOffsetRangeGetInternal)
    GCC_ATTRIB_SECTION("imem_libVoltApi", "voltPolicyOffsetRangeGetInternal");

mockable VoltPolicyLoad            (voltPolicyLoad);

mockable VoltPolicySetVoltage      (voltPolicySetVoltageInternal);

mockable VoltPolicyDynamilwpdate   (voltPolicyDynamilwpdate);

VoltPolicyOffsetVoltage            (voltPolicyOffsetVoltageInternal);

mockable VoltPolicySanityCheck     (voltPolicySanityCheck);

mockable LwU8                      voltPolicyClientColwertToIdx(LwU8 clientID);

mockable FLCN_STATUS               voltPolicySetVoltage(LwU8 clientID, LW2080_CTRL_VOLT_VOLT_RAIL_LIST *pRailList);

/*!
 * Board Object interfaces.
 */
BoardObjGrpIfaceModel10SetEntry   (voltVoltPolicyIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_libVoltConstruct", "voltVoltPolicyIfaceModel10SetEntry");
BoardObjGrpIfaceModel10SetHeader  (voltIfaceModel10SetVoltPolicyHdr)
    GCC_ATTRIB_SECTION("imem_libVoltConstruct", "voltIfaceModel10SetVoltPolicyHdr");
BoardObjIfaceModel10GetStatus               (voltVoltPolicyIfaceModel10GetStatus);

/* ------------------------ Include Derived Types -------------------------- */
#include "volt/voltpolicy_single_rail.h"
#include "volt/voltpolicy_single_rail_multi_step.h"
#include "volt/voltpolicy_split_rail.h"
#include "volt/voltpolicy_split_rail_multi_step.h"
#include "volt/voltpolicy_split_rail_single_step.h"
#include "volt/voltpolicy_multi_rail.h"

#endif // G_VOLTPOLICY_H
#endif // VOLTPOLICY_H
