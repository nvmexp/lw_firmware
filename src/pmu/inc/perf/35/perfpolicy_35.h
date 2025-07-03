/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PERFPOLICY_35_H
#define PERFPOLICY_35_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/perfpolicy.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_POLICY_35   PERF_POLICY_35;
typedef struct PERF_POLICIES_35 PERF_POLICIES_35;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/*!
 * Resets the limit input of the PERF_POLICIES.
 *
 * @param[in,out]  pPolicies35  PERF_POLIICES to clear the limit inputs
 *
 * @return FLCN_OK is successful; detailed error code otherwise
 */
#define PerfPolicies35ResetLimitInput(fname) FLCN_STATUS (fname)(PERF_POLICIES_35 *pPolicies35)

/*!
 * Updates the violation times of the perf points specified.
 *
 * @param[in]  pPolicy35  the PERF_POLICY to query for point violations
 * @param[in]  pointMask  mask of perf points to iterate over
 *
 * @return FLCN_OK if violation times were successfully updated; detailed error
 *         code otherwise
 */
#define PerfPolicies35UpdateViolationTime(fname) FLCN_STATUS (fname)(PERF_POLICIES_35 *pPolicies35, PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE *pArbOutputTuple)

/*!
 * Updates the limit inputs of an individual PERF_POLICY.
 *
 * @param[in]  pPolicy35    PERF_POLICY to update status
 * @param[in]  pArbInput35  Arbitration input tuple
 * @param[in]  bMin         Specifies if the tuple is a min or max input
 *
 * @return FLCN_OK if status updated successfully; detailed error code otherwise
 */
#define PerfPolicy35UpdateLimitInput(fname) FLCN_STATUS (fname)(PERF_POLICY_35 *pPolicy35, PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE *pArbInput35Tuple, LwBool bMin)

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief An individual's policy data for P-states 3.5.
 *
 * Contains data about the policy, including violation times for each
 * PERF_POINT.
 */
struct PERF_POLICY_35
{
    /*!
     * Base class. Must always be first.
     */
    PERF_POLICY super;

    /*!
     * The limit values.
     */
    RM_PMU_PERF_POLICY_35_LIMIT_INPUT limits;
};

/*!
 * @brief Container of 3.5 policies.
 */
struct PERF_POLICIES_35
{
    /*!
     * Base class. Must always be first.
     */
    PERF_POLICIES super;

    /*!
     * Cache of p-state/clock values for perf. points.
     */
    RM_PMU_PERF_POLICY_35_LIMIT_INPUT pointValues[LW2080_CTRL_PERF_POINT_ID_NUM];
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// PERF_POLICIES interfaces
PerfPoliciesConstruct (perfPoliciesConstruct_35)
    // Called only at init time -> init overlay.
    GCC_ATTRIB_SECTION("imem_libPerfInit", "perfPoliciesConstruct_35");

// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet          (perfPolicyGrpIfaceModel10ObjSetImpl_35)
    GCC_ATTRIB_SECTION("imem_libPerfPolicyBoardObj", "perfPolicyGrpIfaceModel10ObjSetImpl_35");
BoardObjGrpIfaceModel10GetStatusHeader (perfPoliciesIfaceModel10GetStatusHeader_35)
    GCC_ATTRIB_SECTION("imem_libPerfPolicyBoardObj", "perfPoliciesIfaceModel10GetStatusHeader_35");
BoardObjIfaceModel10GetStatus              (perfPolicyIfaceModel10GetStatus_35)
    GCC_ATTRIB_SECTION("imem_libPerfPolicyBoardObj", "perfPolicyIfaceModel10GetStatus_35");

// PERF_POLICIES interfaces
PerfPolicies35ResetLimitInput (perfPolicies35ResetLimitInput)
    GCC_ATTRIB_SECTION("imem_libPerfPolicy", "perfPolicies35ResetLimitInput");
PerfPolicies35UpdateViolationTime (perfPolicies35UpdateViolationTime)
    GCC_ATTRIB_SECTION("imem_libPerfPolicy", "perfPolicies35UpdateViolationTime");

// PERF_POLICY interfaces
PerfPolicy35UpdateLimitInput (perfPolicy35UpdateLimitInput)
    GCC_ATTRIB_SECTION("imem_libPerfPolicy", "perfPolicy35UpdateLimitInput");

/* ------------------------ Include Derived Types --------------------------- */

#endif // PERFPOLICY_35_H
