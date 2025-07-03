/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrpolicy_workload.h
 * @brief @copydoc pwrpolicy_workload.c
 */

#ifndef PWRPOLICY_WORKLOAD_H
#define PWRPOLICY_WORKLOAD_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrequation.h"
#include "pmgr/pwrpolicy_domgrp.h"
#include "pmgr/pwrpolicy_workload_shared.h"
#include "pmgr/3x/pwrpolicy_3x.h"

/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_POLICY_WORKLOAD PWR_POLICY_WORKLOAD;

/*!
 * Structure representing a workload-based power policy.
 */
struct PWR_POLICY_WORKLOAD
{
    /*!
     * @copydoc PWR_POLICY_DOMGRP
     */
    PWR_POLICY_DOMGRP super;

    /*!
     * Cached status from filtering step - @ref pwrPolicyFilter_WORKLOAD().
     * Checked when evaluating - @ref pwrPolicyDomGrpEvaluate_WORKLOAD() - to
     * ensure that system is ready for power capping.
     */
    FLCN_STATUS       status;

    /*!
     * Ratio to by which scale clock changes up.
     */
    LwUFXP4_12        clkUpScale;
    /*!
     * Ratio to by which scale clock changes down.
     */
    LwUFXP4_12        clkDownScale;
    /*!
     * Pointer to PWR_EQUATION object which provides the corresponding leakage
     * power callwlations/estimations.
     */
    PWR_EQUATION     *pLeakage;

    /*!
     * Workload median filter state data.
     */
    PWR_POLICY_WORKLOAD_MEDIAN_FILTER median;

    /*!
     * Last counter and timestamp values to be used for callwlating average
     * frequency over the sampled power period.
     */
    CLK_CNTR_AVG_FREQ_START clkCntrStart;

    /*!
     * Boolean indicating if Therm slowdown violation threshold feature is
     * enabled or not. Refer to @violThreshold for more detail.
     */
    LwBool      bViolThresEnabled;
    /*!
     * Threshold percentage value.
     *
     * Once its violation time percentage for @ref RM_PMU_THERM_EVENT_META_ANY_HW_FS
     * exceeds @ref violThreshold, workload policy will not set a frequency limit
     * higher than current limit.
     * If this feature is not enabled, set this index to
     * @ref RM_PMU_THERM_EVENT_ILWALID.
     */
    LwU8        violThreshold;

    //
    // The following is algorithm-specific state, cached here to be easily
    // returned for query/status.
    //

    /*!
     * @copydoc RM_PMU_PMGR_PWR_POLICY_QUERY_POLICY_WORKLOAD_WORK_INPUT
     */
    RM_PMU_PMGR_PWR_POLICY_QUERY_POLICY_WORKLOAD_WORK_INPUT
        work;

    /*!
     * @copydoc RM_PMU_PMGR_PWR_POLICY_QUERY_POLICY_WORKLOAD_WORK_INPUT
     */
    RM_PMU_PMGR_PWR_POLICY_QUERY_POLICY_WORKLOAD_FREQ_INPUT
        freq;

    /*!
     * The current workload/active capacitance (w) callwlated by @ref
     * s_pwrPolicyWorkloadComputeWorkload().  This value is unfiltered.
     */
    LwUFXP20_12     workload;

    /*!
     * Previous evaluation time. Record the timestamp when this policy was last
     * evaluated.
     */
    FLCN_TIMESTAMP  lastEvalTime;
    /*!
     * Previous violation time. Record the total violation time due to
     * Therm event at @ref RM_PMU_THERM_EVENT_META_ANY_HW_FS till last evaluation.
     * Units in nano second.
     */
    LwU32           lastViolTime;
    /*!
     * The violation percentage for most recent power policy evaluation.
     */
    LwU8            violPct;
};

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * PWR_POLICY interfaces
 */
BoardObjGrpIfaceModel10ObjSet      (pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD");
BoardObjIfaceModel10GetStatus          (pwrPolicyIfaceModel10GetStatus_WORKLOAD)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrPolicyIfaceModel10GetStatus_WORKLOAD");
PwrPolicyFilter        (pwrPolicyFilter_WORKLOAD)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyFilter_WORKLOAD");
PwrPolicyValueLwrrGet  (pwrPolicyValueLwrrGet_WORKLOAD)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyClient", "pwrPolicyValueLwrrGet_WORKLOAD");

/*!
 * PWR_POLICY_3X interfaces
 */
PwrPolicy3XFilter      (pwrPolicy3XFilter_WORKLOAD)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicy3XFilter_WORKLOAD");

/*!
 * PWR_POLICY_DOMGRP interfaces
 */
PwrPolicyDomGrpEvaluate (pwrPolicyDomGrpEvaluate_WORKLOAD)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyDomGrpEvaluate_WORKLOAD");

/* ------------------------- Debug Macros ---------------------------------- */

#endif // PWRPOLICY_WORKLOAD_H
