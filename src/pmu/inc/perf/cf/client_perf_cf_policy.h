/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    client_perf_cf_policy.h
 * @brief   Common CLIENT_PERF_CF Policy related defines.
 *
 * @copydoc client_perf_cf.h
 */

#ifndef CLIENT_PERF_CF_POLICY_H
#define CLIENT_PERF_CF_POLICY_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct CLIENT_PERF_CF_POLICY CLIENT_PERF_CF_POLICY, CLIENT_PERF_CF_POLICY_BASE;

typedef struct CLIENT_PERF_CF_POLICYS CLIENT_PERF_CF_POLICYS;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * Colwenience macro to look-up CLIENT_PERF_CF_POLICYS.
 */
#define CLIENT_PERF_CF_GET_POLICYS()                    \
    (&(PERF_CF_GET_OBJ()->clientPolicys))

/*!
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_CLIENT_PERF_CF_POLICY \
    (&(CLIENT_PERF_CF_GET_POLICYS()->super.super))

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Virtual BOARDOBJ child providing attributes common to all CLIENT_PERF_CF Policies.
 */
struct CLIENT_PERF_CF_POLICY
{
    /*!
     * BOARDOBJ super-class.
     * Must be first element of the structure!
     */
    BOARDOBJ        super;
    /*!
     * Label. @ref LW2080_CTRL_PERF_CLIENT_PERF_CF_POLICY_LABEL_<xyz>.
     */
    LwU8            label;
    /*!
     * Index of internal policy mapped to this client policy.
     * @note This is private internal variable and must not be exposed
     *       via RMCTRLs.
     */
    LwBoardObjIdx   policyIdx;
};

/*!
 * Collection of all CLIENT_PERF_CF Policies and related information.
 */
struct CLIENT_PERF_CF_POLICYS
{
    /*!
     * BOARDOBJGRP_E32 super class. This should always be the first member!
     */
    BOARDOBJGRP_E32 super;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// BOARDOBJGRP interfaces.
BoardObjGrpIfaceModel10CmdHandler   (perfCfClientPerfCfPolicyBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfClientPerfCfPolicyBoardObjGrpIfaceModel10Set");
BoardObjGrpIfaceModel10CmdHandler   (perfCfClientPerfCfPolicyBoardObjGrpIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfClientPerfCfPolicyBoardObjGrpIfaceModel10GetStatus");

// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet       (perfCfClientPerfCfPolicyGrpIfaceModel10ObjSetImpl_SUPER)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfClientPerfCfPolicyGrpIfaceModel10ObjSetImpl_SUPER");
BoardObjIfaceModel10GetStatus           (perfCfClientPerfCfPolicyIfaceModel10GetStatus_SUPER)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfClientPerfCfPolicyIfaceModel10GetStatus_SUPER");

// CLIENT_PERF_CF_POLICY interfaces.

/* ------------------------ Include Derived Types --------------------------- */
#include "perf/cf/client_perf_cf_policy_ctrl_mask.h"

#endif // CLIENT_PERF_CF_POLICY_H
