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
 * @file    client_perf_cf_policy_ctrl_mask.h
 * @brief   Control Mask CLIENT_PERF_CF Policy related defines.
 *
 * @copydoc client_perf_cf.h
 */

#ifndef CLIENT_PERF_CF_POLICY_CTRL_MASK_H
#define CLIENT_PERF_CF_POLICY_CTRL_MASK_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/client_perf_cf_policy.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct CLIENT_PERF_CF_POLICY_CTRL_MASK CLIENT_PERF_CF_POLICY_CTRL_MASK;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * CLIENT_PERF_CF_POLICY child class providing attributes of
 * Control Mask CLIENT_PERF_CF Policy.
 */
struct CLIENT_PERF_CF_POLICY_CTRL_MASK
{
    /*!
     * CLIENT_PERF_CF_POLICY parent class.
     * Must be first element of the structure!
     */
    CLIENT_PERF_CF_POLICY   super;

    /*!
     * Mask of CLIENT_PERF_CF controllers to enable for this policy.
     */
    BOARDOBJGRPMASK_E32     maskControllers;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet       (perfCfClientPerfCfPolicyGrpIfaceModel10ObjSetImpl_CTRL_MASK)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfClientPerfCfPolicyGrpIfaceModel10ObjSetImpl_CTRL_MASK");
BoardObjIfaceModel10GetStatus           (perfCfClientPerfCfPolicyIfaceModel10GetStatus_CTRL_MASK)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfClientPerfCfPolicyIfaceModel10GetStatus_CTRL_MASK");

// CLIENT_PERF_CF_POLICY interfaces.

/* ------------------------ Include Derived Types --------------------------- */

#endif // CLIENT_PERF_CF_POLICY_CTRL_MASK_H
