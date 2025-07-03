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
 * @file    perf_cf_policy.h
 * @brief   Common PERF_CF Policy related defines.
 *
 * @copydoc perf_cf.h
 */

#ifndef PERF_CF_POLICY_H
#define PERF_CF_POLICY_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp.h"
#include "perf/cf/perf_cf_controller.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_CF_POLICY PERF_CF_POLICY, PERF_CF_POLICY_BASE;

typedef struct PERF_CF_POLICYS PERF_CF_POLICYS;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * Colwenience macro to look-up PERF_CF_POLICYS.
 */
#define PERF_CF_GET_POLICYS()                                                 \
    (&(PERF_CF_GET_OBJ()->policys))

/*!
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_PERF_CF_POLICY                              \
    (&(PERF_CF_GET_POLICYS()->super.super))

/*!
 * @brief   Retrieves the index of a @ref PERF_CF_POLICY corresponding to the
 *          given label.
 *
 * @param[in]   _pPolicys   @ref PERF_CF_POLICYS group pointer
 * @param[in]   _label      Label for which to get index
 *
 * @return  Index for the given label, if the label correspond to a valid
 *          @ref PERF_CF_POLICYS
 * @return  @ref LW2080_CTRL_BOARDOBJ_IDX_ILWALID if the label is invalid or the
 *          policy does not exist.
 */
#define PERF_CF_POLICYS_LABEL_GET_IDX(_pPolicys, _label)                      \
    ((_label) < LW2080_CTRL_PERF_PERF_CF_POLICY_LABEL_NUM ?                   \
        (_pPolicys)->labelToIdxMap[(_label)] :                                \
        LW2080_CTRL_BOARDOBJ_IDX_ILWALID)

/*!
 * Checks whether a PERF_CF Policy for a label exists
 *
 * @param[in]   _pPolicys   @ref PERF_CF_POLICYS group pointer
 * @param[in]   _label      LW2080_CTRL_PERF_PERF_CF_POLICY_LABEL value
 *
 * @return  @ref LW_TRUE    There is a @ref PERF_CF_POLICY for the given label
 * @return  @ref LW_FALSE   There is no @ref PERF_CF_POLICY for the given label
 */
#define PERF_CF_POLICYS_LABEL_IS_VALID(_pPolicys, _label)                      \
    (PERF_CF_POLICYS_LABEL_GET_IDX((_pPolicys), (_label)) !=                   \
        LW2080_CTRL_BOARDOBJ_IDX_ILWALID)

/*!
 * @brief   Retrieves the @ref PERF_CF_POLICY corresponding to the given label.
 *
 * @param[in]   _pPolicys   @ref PERF_CF_POLICYS group pointer
 * @param[in]   _label      Label for which to get @ref PERF_CF_POLICY
 *
 * @return  Pointer to the @ref PERF_CF_POLICY, if the label corresponds to a
 *          valid @ref PERF_CF_POLICY
 * @return  @ref LW2080_CTRL_BOARDOBJ_IDX_ILWALID if the label is invalid or the
 *          policy does not exist.
 */
#define PERF_CF_POLICYS_LABEL_GET(_pPolicys, _label)                          \
    (PERF_CF_POLICYS_LABEL_IS_VALID((_pPolicys), (_label)) ?                    \
        (PERF_CF_POLICY *)BOARDOBJGRP_OBJ_GET(                                \
            PERF_CF_POLICY,                                                   \
            PERF_CF_POLICYS_LABEL_GET_IDX((_pPolicys), (_label))) :           \
        NULL)

/*!
 * @brief Accessor macro for PERF_CF_POLICYS::activeMaskArbitrated
 */
#define perfCfPolicysActiveMaskArbitratedGet()                                \
    (&(PERF_CF_GET_POLICYS())->activeMaskArbitrated)

/*!
 * @brief   Initializes a deactivate request ID mask.
 *
 * @param[in,out]   pDeactivate Pointer to
 *                              @ref PERF_CF_POLICY_DEACTIVATE_REQUEST_ID_MASK
 *                              structure to initialize
 */
#define perfCfPolicyDeactivateRequestIdMaskInit(pDeactivate) \
    boardObjGrpMaskInit_E32((pDeactivate))

/*!
 * @brief   Sets a deactivation request in the mask.
 *
 * @param[in,out]   pDeactivate Pointer to
 *                              @ref PERF_CF_POLICY_DEACTIVATE_REQUEST_ID_MASK
 *                              structure in which to set
 * @param[in]       requestId   Request ID of the request
 */
#define perfCfPolicyDeactivateRequestIdMaskSet(pDeactivate, requestId) \
    boardObjGrpMaskBitSet((pDeactivate), (requestId))

/*!
 * @brief   Clears a deactivation request in the mask.
 *
 * @param[in,out]   pDeactivate Pointer to
 *                              @ref PERF_CF_POLICY_DEACTIVATE_REQUEST_ID_MASK
 *                              structure in which to clear
 * @param[in]       requestId   @ref LW2080_CTRL_PERF_PERF_CF_POLICY_DEACTIVATE_REQUEST_ID
 *                              for the request
 */
#define perfCfPolicyDeactivateRequestIdMaskClr(pDeactivate, requestId) \
    boardObjGrpMaskBitClr((pDeactivate), (requestId))

/*!
 * @brief   Queries the deactivation request in the mask.
 *
 * @param[in,out]   pDeactivate Pointer to
 *                              @ref PERF_CF_POLICY_DEACTIVATE_REQUEST_ID_MASK
 *                              structure to check
 * @param[in]       requestId   @ref LW2080_CTRL_PERF_PERF_CF_POLICY_DEACTIVATE_REQUEST_ID
 *                              for the request
 *
 * @return  @ref LW_TRUE    Bit is set in the mask
 * @return  @ref LW_FALSE   Bit is not set in the mask
 */
#define perfCfPolicyDeactivateRequestIdMaskGet(pDeactivate, requestId) \
    boardObjGrpMaskBitGet((pDeactivate), (requestId))

/*!
 * @brief   Determines whether a @ref PERF_CF_POLICY_DEACTIVATE_REQUEST_ID_MASK
 *          indicates the policy should be activated or deactivated.
 *
 * @param[in]   pDeactivate Pointer to
 *                          @ref PERF_CF_POLICY_DEACTIVATE_REQUEST_ID_MASK
 *                          structure to check
 *
 * @return  @ref LW_TRUE    Mask indicates deactivated
 * @return  @ref LW_TRUE    Mask indicates activated
 */
#define perfCfPolicyDeactivateRequestIdMaskIsDeactivated(pDeactivate) \
    (!boardObjGrpMaskIsZero((pDeactivate)))

/*!
 * @brief   Imports an external @ref PERF_CF_POLICY deactivate mask to an
 *          internal one.
 *
 * @param[in]   pDeactivate     Pointer to internal
 *                              @ref PERF_CF_POLICY_DEACTIVATE_REQUEST_ID_MASK
 *                              into which to import
 * @param[out]  pCtrlDeactivate Pointer to external
 *                              @ref LW2080_CTRL_PERF_PERF_CF_POLICY_DEACTIVATE_REQUEST_ID_MASK
 *                              from which to export
 *
 * @return  @ref FLCN_OK    Success
 * @return  Others          Errors propagated from callees
 */
#define perfCfPolicyDeactivateRequestIdMaskImport(pDeactivate, pCtrlDeactivate) \
    boardObjGrpMaskImport_E32((pDeactivate), (pCtrlDeactivate))

/*!
 * @brief   Exports an internal @ref PERF_CF_POLICY deactivate mask to an
 *          external one.
 *
 * @param[in]   pDeactivate     Pointer to internal
 *                              @ref PERF_CF_POLICY_DEACTIVATE_REQUEST_ID_MASK
 *                              from which to export
 * @param[out]  pCtrlDeactivate Pointer to external
 *                              @ref LW2080_CTRL_PERF_PERF_CF_POLICY_DEACTIVATE_REQUEST_ID_MASK
 *                              into which to import
 *
 * @return  @ref FLCN_OK    Success
 * @return  Others          Errors propagated from callees
 */
#define perfCfPolicyDeactivateRequestIdMaskExport(pDeactivate, pCtrlDeactivate) \
    boardObjGrpMaskExport_E32((pDeactivate), (pCtrlDeactivate))

/* ------------------------ Function Prototypes ----------------------------- */
/*!
 * @interface   PERF_CF_POLICY
 *
 * @brief   Load all PERF_CF_POLICYs.
 *
 * @param[in]   pPolicys    PERF_CF_POLICYS object pointer.
 * @param[in]   bLoad       LW_TRUE on load(), LW_FALSE on unload().
 *
 * @return  FLCN_OK     All PERF_CF_POLICYs loaded successfully
 * @return  other       Propagates return values from various calls
 */
#define PerfCfPolicysLoad(fname) FLCN_STATUS (fname)(PERF_CF_POLICYS *pPolicys, LwBool bLoad)

/*!
 * @interface   PERF_CF_POLICY
 *
 * @brief
 *
 * @return  FLCN_OK     Selected policy successfully activated
 * @return  other       Propagates return values from various calls
 */
#define PerfCfPolicyActivate(fname) FLCN_STATUS (fname)(PERF_CF_POLICY *pPolicy, LwBool bActivate, BOARDOBJGRPMASK_E32 *pMaskControllers)

/*!
 * @interface   PERF_CF_POLICY
 *
 * @brief   Activate/deactivate the specified policy by label.
 *
 * @param[in]   pPolicys    PERF_CF_POLICYS object pointer.
 * @param[in]   label       Find the policy with matching label
 * @param[in]   bActivate   LW_TRUE = activate, LW_FALSE = deactivate
 *
 * @return  FLCN_OK   Policy successfully activated/deactivated.
 * @return  errors  Unexpected errors propagated from other functions.
 */
#define PerfCfPolicysActivate(fname) FLCN_STATUS (fname)(PERF_CF_POLICYS *pPolicys, LwU8 label, LwBool bActivate)

/*!
 * @interface   PERF_CF_POLICY
 *
 * @brief   Set the deactivate state for the given request ID to the given
 *          state.
 *
 * @param[in]   pPolicys    PERF_CF_POLICYS object pointer.
 * @param[in]   label       Label for the policy for which to set the state.
 * @param[in]   requestId   Request ID for which to activate/deactivate. Must be
 *                          in the range
 *                          [@ref LW2080_CTRL_PERF_PERF_CF_POLICY_DEACTIVATE_REQUEST_ID_PMU__START,
 *                          @ref LW2080_CTRL_PERF_PERF_CF_POLICY_DEACTIVATE_REQUEST_ID_PMU__END].
 * @param[in]   bSet        Whether to deactivate (@ref LW_TRUE) or activate
 *                          (@ref LW_FALSE)
 *
 * @return  @ref LW_OK                      Success
 * @return  @ref LW_ERR_ILWALID_ARGUMENT    requestId or label outside valid
 *                                          range or label does nto correspond
 *                                          to a valid @ref PERF_CF_POLICY
 */
#define PerfCfPolicysDeactivateMaskSet(fname) FLCN_STATUS (fname)(PERF_CF_POLICYS *pPolicys, LwU8 label, LW2080_CTRL_PERF_PERF_CF_POLICY_DEACTIVATE_REQUEST_ID requestId, LwBool bSet)

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Mask of requests that can be used to deactivate a @ref PERF_CF_POLICY
 */
typedef BOARDOBJGRPMASK_E32 PERF_CF_POLICY_DEACTIVATE_REQUEST_ID_MASK;

/*!
 * Virtual BOARDOBJ child providing attributes common to all PERF_CF Policies.
 */
struct PERF_CF_POLICY
{
    /*!
     * BOARDOBJ super-class.
     * Must be first element of the structure!
     */
    BOARDOBJ            super;
    /*!
     * Mask of policies this policy is compatible with (i.e. bit-wise-NOT of conflict mask).
     */
    BOARDOBJGRPMASK_E32 compatibleMask;
    /*!
     * When multiple active policys are in conflict with each other, higher priority wins.
     */
    LwU8                priority;
    /*!
     * Label. @ref LW2080_CTRL_PERF_PERF_CF_POLICY_LABEL_<xyz>.
     */
    LwU8                label;
    /*!
     * Activate/deactivate control state for the policy.
     */
    LwBool              bActivate;

    /*!
     * Mask of requests to deactivate the policy.
     */
    PERF_CF_POLICY_DEACTIVATE_REQUEST_ID_MASK deactivateMask;
};

/*!
 * Collection of all PERF_CF Policies and related information.
 */
struct PERF_CF_POLICYS
{
    /*!
     * BOARDOBJGRP_E32 super class. This should always be the first member!
     */
    BOARDOBJGRP_E32     super;
    /*!
     * Mask of policies requested (disregarding potential conflicts).
     */
    BOARDOBJGRPMASK_E32 activeMaskRequested;
    /*!
     * Mask of policies lwrrently active (after resolving conflict).
     */
    BOARDOBJGRPMASK_E32 activeMaskArbitrated;

    /*!
     * @copydoc LW2080_CTRL_PERF_PERF_CF_POLICYS_INFO::labelToIdxMap
     */
    LwBoardObjIdx       labelToIdxMap[
        LW2080_CTRL_PERF_PERF_CF_POLICY_LABEL_NUM];

    /*!
     * Array of policy indexes, sorted by priority from highest to lowest.
     */
    LwU8                prioritySortedIdx[LW2080_CTRL_BOARDOBJGRP_E32_MAX_OBJECTS];
    LwU8                prioritySortedIdxN;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// BOARDOBJGRP interfaces.
BoardObjGrpIfaceModel10CmdHandler   (perfCfPolicyBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfPolicyBoardObjGrpIfaceModel10Set");
BoardObjGrpIfaceModel10CmdHandler   (perfCfPolicyBoardObjGrpIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfPolicyBoardObjGrpIfaceModel10GetStatus");

// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet       (perfCfPolicyGrpIfaceModel10ObjSetImpl_SUPER)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfPolicyGrpIfaceModel10ObjSetImpl_SUPER");
BoardObjIfaceModel10GetStatus           (perfCfPolicyIfaceModel10GetStatus_SUPER)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfPolicyIfaceModel10GetStatus_SUPER");

// PERF_CF_POLICY interfaces.
PerfCfPolicysLoad       (perfCfPolicysLoad)
    GCC_ATTRIB_SECTION("imem_perfCfLoad", "perfCfPolicysLoad");

FLCN_STATUS perfCfPolicysPostInit(PERF_CF_POLICYS *pPolicys)
    GCC_ATTRIB_SECTION("imem_perfCfInit", "perfCfPolicysPostInit");

PerfCfPolicysActivate (perfCfPolicysActivate)
    GCC_ATTRIB_SECTION("imem_perfCf", "perfCfPolicysActivate");
PerfCfPolicysDeactivateMaskSet (perfCfPolicysDeactivateMaskSet)
    GCC_ATTRIB_SECTION("imem_perfCf", "perfCfPolicysDeactivateMaskSet");

/* ------------------------ Inline Functions -------------------------------- */
/*!
 * @brief   Imports the RM controlled portion of an external @ref PERF_CF_POLICY
 *          deactivate mask to an internal one.
 *
 * @param[in]   pDeactivate     Pointer to internal
 *                              @ref PERF_CF_POLICY_DEACTIVATE_REQUEST_ID_MASK
 *                              into which to import
 * @param[out]  pCtrlDeactivate Pointer to external
 *                              @ref LW2080_CTRL_PERF_PERF_CF_POLICY_DEACTIVATE_REQUEST_ID_MASK
 *                              from which to export
 *
 * @return  @ref FLCN_OK    Success
 * @return  Others          Errors propagated from callees
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfCfPolicyDeactivateRequestIdMaskRmImport
(
    PERF_CF_POLICY_DEACTIVATE_REQUEST_ID_MASK                  *pDeactivate,
    LW2080_CTRL_PERF_PERF_CF_POLICY_DEACTIVATE_REQUEST_ID_MASK *pCtrlDeactivate
)
{
    FLCN_STATUS status;
    PERF_CF_POLICY_DEACTIVATE_REQUEST_ID_MASK rmMask;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDeactivate != NULL) && (pCtrlDeactivate != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfPolicyDeactivateRequestIdMaskRmImport_exit);

    perfCfPolicyDeactivateRequestIdMaskInit(&rmMask);

    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfPolicyDeactivateRequestIdMaskImport(&rmMask, pCtrlDeactivate),
        perfCfPolicyDeactivateRequestIdMaskRmImport_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpMaskRangeCopy(
            pDeactivate,
            &rmMask,
            LW2080_CTRL_PERF_PERF_CF_POLICY_DEACTIVATE_REQUEST_ID_RM__START,
            LW2080_CTRL_PERF_PERF_CF_POLICY_DEACTIVATE_REQUEST_ID_RM__END),
        perfCfPolicyDeactivateRequestIdMaskRmImport_exit);

perfCfPolicyDeactivateRequestIdMaskRmImport_exit:
    return status;
}

/* ------------------------ Include Derived Types --------------------------- */
#include "perf/cf/perf_cf_policy_ctrl_mask.h"

#endif // PERF_CF_POLICY_H
