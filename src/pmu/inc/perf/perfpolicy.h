/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perfpolicy.h
 * @brief   Perf policies
 */

#ifndef PERF_POLICIES_H
#define PERF_POLICIES_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "dbgprintf.h"
#include "boardobj/boardobjgrp.h"
#include "ctrl/ctrl2080/ctrl2080perf.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_POLICY   PERF_POLICY, PERF_POLICY_BASE;
typedef struct PERF_POLICIES PERF_POLICIES;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * Base set of common PERF_POLICY overlays (both IMEM and DMEM). Expected to be
 * used with @ref OSTASK_ATTACH_OR_DETACH_OVERLAYS_<XYZ>().
 */
#define PERF_POLICY_OVERLAYS_BASE                                             \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPerfPolicy)                     \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfPolicy)

/*!
 * Set of PERF_POLICY overlays (both IMEM and DMEM) for use with BOARDOBJ.
 * Expected to be used with @ref OSTASK_ATTACH_OR_DETACH_OVERLAYS_<XYZ>().
 */
#define PERF_POLICY_OVERLAYS_BOARDOBJ                                         \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPerfPolicyBoardObj)              \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfPolicy)

/*!
 * Helper macro accessor to the PERF_POLICIES object.
 *
 * @return pointer to PERF_POLICIES object
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_POLICY))
#define PERF_POLICIES_GET()                                                   \
    (Perf.pPolicies)
#else
#define PERF_POLICIES_GET()                                                   \
    ((PERF_POLICIES *)NULL)
#endif

/*!
 * Helper macro accessor to the PERF_POLICY object.
 *
 * @param[in]  _idx  Policy ID of PERF_POLICY
 *
 * @return pointer to PERF_POLICY object
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_POLICY))
#define PERF_POLICY_GET(_idx)                                                 \
    (BOARDOBJGRP_OBJ_GET(PERF_POLICY, (_idx)))
#else
#define PERF_POLICY_GET(_idx)                                                 \
    ((PERF_POLICY *)NULL)
#endif

/*!
 * Macro to locate PERF_POLICIES BOARDOBJGRP.
 *
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_PERF_POLICY \
    (&(PERF_POLICIES_GET()->super.super))

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief An individual policy's state.
 */
struct PERF_POLICY
{
    /*!
     * Base class. Must always be first.
     */
    BOARDOBJ super;

    /*!
     * Version of the policy structure. Values specificed in
     * @ref LW2080_CTRL_PERF_POLICY_TYPE.
     */
    LwU8 type;

    /*!
     * Policy flags. See @ref LW2080_CTRL_PERF_POLICY_FLAGS flag for more details.
     */
    LwU32 flags;

    /*!
     * Status of the perf policy. Contains information about the
     * lwrrently violated and the violation times.
     */
    LW2080_CTRL_PERF_POINT_VIOLATION_STATUS violationStatus;
};

/*!
 * @brief Container of policies.
 *
 * Includes additional information to properly function.
 */
struct PERF_POLICIES
{
    /*!
     * Base class. Must always be first.
     */
    BOARDOBJGRP_E32 super;

    /*!
     * Version of the policies structure. Values specified in
     * @ref LW2080_CTRL_PERF_POLICIES_VERSION.
     */
     LwU8 version;

    /*!
     * Boolean indicating whether the PERF_POLICIES subsystem has been loaded
     * by the RM/CPU. When loaded, PERF_POLICIES is able to run the data
     * collection of limit violations.
     */
    LwBool bLoaded;

    /*!
     * Reference time's output. Units in nanoseconds.
     */
    FLCN_TIMESTAMP referenceTimeNs;

    /*!
     * Mask of policies lwrrently limiting GPU perf.
     */
    LwU32 limitingPolicyMask;

    /*!
     * Global status.
     */
    LW2080_CTRL_PERF_POINT_VIOLATION_STATUS globalViolationStatus;

    /*!
     * Mask of supported perf. points.
     */
    LwU32 pointMask;

    /*!
     * Mask of supported perf. points which can only be evaluated at global
     * time (after arbitration). This mask is to be used strictly as a subset
     * the board obj group's mask.
     */
    LwU32 pointMaskGlobalOnly;
};

/* ------------------------ Function Prototypes ----------------------------- */
/*!
 * @interface PERF_POLICIES
 *
 * Constructs all necessary object state for PERF_POLICIES functionality,
 * including allocating necessary DMEM for PERF_POLICIES BOARDOBJGRP as well as
 * any type-specific state.
 */
#define PerfPoliciesConstruct(fname) FLCN_STATUS (fname)(PERF_POLICIES **ppPolicies)

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
FLCN_STATUS perfPoliciesConstruct(void)
    // Called only at init time -> init overlay
    GCC_ATTRIB_SECTION("imem_libPerfInit", "perfPoliciesConstruct");
PerfPoliciesConstruct (perfPoliciesConstruct_SUPER)
    // Called only at init time -> init overlay
    GCC_ATTRIB_SECTION("imem_libPerfInit", "perfPoliciesConstruct_SUPER");

// BOARDOBJGRP interfaces
BoardObjGrpIfaceModel10CmdHandler (perfPolicyBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_libPerfBoardObj", "perfPolicyBoardObjGrpIfaceModel10Set");
BoardObjGrpIfaceModel10CmdHandler (perfPolicyBoardObjGrpIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_libPerfBoardObj", "perfPolicyBoardObjGrpIfaceModel10GetStatus");

// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet (perfPolicyGrpIfaceModel10ObjSetImpl_SUPER)
    GCC_ATTRIB_SECTION("imem_libPerfPolicyBoardObj", "perfPolicyGrpIfaceModel10ObjSetImpl_SUPER");
BoardObjIfaceModel10GetStatus     (perfPolicyIfaceModel10GetStatus_SUPER)
    GCC_ATTRIB_SECTION("imem_libPerfPolicyBoardObj", "perfPolicyIfaceModel10GetStatus_SUPER");

// PERF_POLICIES interfaces
FLCN_STATUS perfPolicyPmumonGetSample(PERF_POLICIES *pPolicies, RM_PMU_PERF_POLICY_BOARDOBJ_GRP_GET_STATUS *pSample)
    GCC_ATTRIB_SECTION("imem_libPerfPolicyBoardObj", "perfPolicyPmumonGetSample");

/* ------------------------ Include Derived Types --------------------------- */
#include "perf/35/perfpolicy_35.h"

#endif // PERF_POLICIES_H
