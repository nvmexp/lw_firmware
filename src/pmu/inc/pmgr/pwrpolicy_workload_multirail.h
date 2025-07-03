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
 * @file  pwrpolicy_workload_multirail.h
 * @brief @copydoc pwrpolicy_workload_multirail.c
 */

#ifndef PWRPOLICY_WORKLOAD_MULTIRAIL_H
#define PWRPOLICY_WORKLOAD_MULTIRAIL_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrequation.h"
#include "pmgr/pwrpolicy_domgrp.h"
#include "pmgr/pwrpolicy_workload_shared.h"
#include "pmgr/3x/pwrpolicy_3x.h"
#include "pmgr/pwrpolicy_workload_multirail_iface.h"

/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_POLICY_WORKLOAD_MULTIRAIL PWR_POLICY_WORKLOAD_MULTIRAIL;

/*!
 * Structure representing a per rail workload-based power policy parameters.
 */
typedef struct
{
    /*!
     * Workload median filter state data.
     */
    PWR_POLICY_WORKLOAD_MEDIAN_FILTER median;
} PWR_POLICY_WORKLOAD_MULTIRAIL_VOLT_RAIL;

/*!
 * Structure representing a workload-based power policy.
 */
struct PWR_POLICY_WORKLOAD_MULTIRAIL
{
    /*!
     * @copydoc PWR_POLICY_DOMGRP
     */
    PWR_POLICY_DOMGRP                          super;
    /*!
     * Cached status from filtering step - @ref pwrPolicyFilter_WORKLOAD().
     * Checked when evaluating - @ref pwrPolicyDomGrpEvaluate_WORKLOAD() - to
     * ensure that system is ready for power capping.
     */
    FLCN_STATUS                                status;
    /*!
     * Boolean indicating if Bidirectional Search for VF point is enabled.
     */
    LwBool                                     bBidirectionalSearch;
    /*!
     * Previous evaluation and sleep times for MSCG.
     * Records the timestamp when this policy was last evaluated.
     */
    PWR_POLICY_WORKLOAD_LPWR_TIME    mscgLpwrTime;
    /*!
     * Previous evaluation and sleep times for PG.
     * Records the timestamp when this policy was last evaluated.
     */
    PWR_POLICY_WORKLOAD_LPWR_TIME    pgLpwrTime;
    /*!
     * Ratio to by which scale clock changes up.
     */
    LwUFXP4_12                                 clkUpScale;
    /*!
     * Ratio to by which scale clock changes down.
     */
    LwUFXP4_12                                 clkDownScale;
    /*!
     * Last counter and timestamp values to be used for callwlating average
     * frequency over the sampled power period.
     */
    CLK_CNTR_AVG_FREQ_START                    clkCntrStart;
    /*!
     * copydoc@ PWR_POLICY_WORKLOAD_MULTIRAIL_VOLT_RAIL
     */
    PWR_POLICY_WORKLOAD_MULTIRAIL_VOLT_RAIL
        rail[LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_MULTIRAIL_VOLT_RAIL_IDX_MAX];
    /*!
     * copydoc@ PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE
     */
    PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE    workIface;
};

/* ------------------------- Defines --------------------------------------- */
/*!
 * Accessor macro for a WORKLOAD_MULTIRAIL_INTERFACE interface from WORKLOAD_MULTIRAIL super class.
 */
#define PWR_POLICY_WORKLOAD_MULTIRAIL_IFACE_GET(pWorkload)                    \
    &((pWorkload)->workIface)

/*!
 * Helper macro to colwert a 2CLK clock frequncy in MHz to a 1CLK (e.g. GPC2CLK
 * vs GPCCLK) clock frequency in MHz.
 * The odd 2CLK frequency will be considered as +1 frequency.
 * 2961MHz (2CLK) => 2962MHz (2CLK) => 2962/2 = 1480 (1CLK).
 *
 */
#define PWR_POLICY_WORKLOAD_2MHZ_TO_MHZ(clkMHz)                                 \
    LW_UNSIGNED_ROUNDED_DIV((clkMHz), 2)

/*!
 * Overlays for CHANGE_SEQ, if feature is enabled.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ)
#define PWR_POLICY_WORKLOAD_MULTIRAIL_EVALUATE_CHANGE_SEQ_OVERLAYS              \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfChangeSeqClient)
#else
#define PWR_POLICY_WORKLOAD_MULTIRAIL_EVALUATE_CHANGE_SEQ_OVERLAYS              \
    OSTASK_OVL_DESC_ILWALID
#endif

/*!
 * Overlays for SENSED_VOLTAGE required during policy evaluate.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD_SENSED_VOLTAGE))
#define PWR_POLICY_WORKLOAD_MULTIRAIL_EVALUATE_SENSED_VOLTAGE_OVERLAYS          \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClkVolt)
#else
#define PWR_POLICY_WORKLOAD_MULTIRAIL_EVALUATE_SENSED_VOLTAGE_OVERLAYS          \
    OSTASK_OVL_DESC_ILWALID
#endif

/*!
 * Overlays for SENSED_VOLTAGE required during policy load.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD_SENSED_VOLTAGE))
#define PWR_POLICY_WORKLOAD_MULTIRAIL_LOAD_SENSED_VOLTAGE_OVERLAYS              \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perf)                                 \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfVf)                               \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClkVolt)                           \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVoltApi)
#else
#define PWR_POLICY_WORKLOAD_MULTIRAIL_LOAD_SENSED_VOLTAGE_OVERLAYS              \
    OSTASK_OVL_DESC_ILWALID
#endif

/*!
 * Helper Overlay sets for PWR_POLICY load of
 * PWR_POLICY_WORKLOAD_MULTIRAIL.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD))
#define PWR_POLICY_WORKLOAD_MULTIRAIL_LOAD_OVERLAYS                             \
    PWR_POLICY_WORKLOAD_MULTIRAIL_LOAD_SENSED_VOLTAGE_OVERLAYS
#else
#define PWR_POLICY_WORKLOAD_MULTIRAIL_LOAD_OVERLAYS                             \
    OSTASK_OVL_DESC_ILWALID
#endif

/*!
 * Helper Overlay sets for PWR_POLICY evaluation of
 * PWR_POLICY_WORKLOAD_MULTIRAIL.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD)
#define PWR_POLICY_WORKLOAD_MULTIRAIL_EVALUATE_OVERLAYS                         \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrLibPwrPolicyWorkloadMultirail)    \
    PWR_POLICY_WORKLOAD_MULTIRAIL_EVALUATE_CHANGE_SEQ_OVERLAYS                  \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVoltApi)                           \
    PWR_POLICY_WORKLOAD_MULTIRAIL_EVALUATE_SENSED_VOLTAGE_OVERLAYS
#else
#define PWR_POLICY_WORKLOAD_MULTIRAIL_EVALUATE_OVERLAYS                         \
    OSTASK_OVL_DESC_ILWALID
#endif

/*!
 * Helper macro for the set of overlays required to call @ref
 * pwrPolicyFilter_WORKLOAD_MULTIRAIL().
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD)
#define PWR_POLICY_WORKLOAD_MULTIRAIL_FILTER_OVERLAYS                           \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrLibPwrPolicyWorkloadMultirail)    \
    PWR_POLICY_WORKLOAD_MULTIRAIL_EVALUATE_CHANGE_SEQ_OVERLAYS                  \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVoltApi)                           \
    PWR_POLICY_WORKLOAD_MULTIRAIL_EVALUATE_SENSED_VOLTAGE_OVERLAYS              \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLpwr)                              \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrLibPwrPolicyWorkloadShared)
#else
#define PWR_POLICY_WORKLOAD_MULTIRAIL_FILTER_OVERLAYS                           \
    OSTASK_OVL_DESC_ILWALID
#endif

/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * PWR_POLICY interfaces
 */
BoardObjGrpIfaceModel10ObjSet         (pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_MULTIRAIL)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_MULTIRAIL");
BoardObjIfaceModel10GetStatus             (pwrPolicyIfaceModel10GetStatus_WORKLOAD_MULTIRAIL)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrPolicyIfaceModel10GetStatus_WORKLOAD_MULTIRAIL");
PwrPolicyFilter           (pwrPolicyFilter_WORKLOAD_MULTIRAIL)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyFilter_WORKLOAD_MULTIRAIL");

/*!
 * PWR_POLICY_3X interfaces
 */
PwrPolicy3XFilter         (pwrPolicy3XFilter_WORKLOAD_MULTIRAIL)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicy3XFilter_WORKLOAD_MULTIRAIL");
PwrPolicyLoad             (pwrPolicyLoad_WORKLOAD_MULTIRAIL)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrPolicyLoad_WORKLOAD_MULTIRAIL");

/*!
 * PWR_POLICY_DOMGRP interfaces
 */
PwrPolicyDomGrpEvaluate   (pwrPolicyDomGrpEvaluate_WORKLOAD_MULTIRAIL)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyDomGrpEvaluate_WORKLOAD_MULTIRAIL");

/* ---------------------------------- Macros ---------------------------------- */

#endif // PWRPOLICY_WORKLOAD_MULTIRAIL_H
