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
 * @file    perf_cf_policy_ctrl_mask.h
 * @brief   Control Mask PERF_CF Policy related defines.
 *
 * @copydoc perf_cf.h
 */

#ifndef PERF_CF_POLICY_CTRL_MASK_H
#define PERF_CF_POLICY_CTRL_MASK_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_policy.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_CF_POLICY_CTRL_MASK PERF_CF_POLICY_CTRL_MASK;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * PERF_CF_POLICY child class providing attributes of
 * Control Mask PERF_CF Policy.
 */
struct PERF_CF_POLICY_CTRL_MASK
{
    /*!
     * PERF_CF_POLICY parent class.
     * Must be first element of the structure!
     */
    PERF_CF_POLICY      super;

    /*!
     * Mask of PERF_CF controllers to enable for this policy.
     */
    BOARDOBJGRPMASK_E32 maskControllers;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet       (perfCfPolicyGrpIfaceModel10ObjSetImpl_CTRL_MASK)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfPolicyGrpIfaceModel10ObjSetImpl_CTRL_MASK");
BoardObjIfaceModel10GetStatus           (perfCfPolicyIfaceModel10GetStatus_CTRL_MASK);

// PERF_CF_POLICY interfaces.
PerfCfPolicyActivate    (perfCfPolicyActivate_CTRL_MASK);

/* ------------------------ Include Derived Types --------------------------- */

#endif // PERF_CF_POLICY_CTRL_MASK_H
