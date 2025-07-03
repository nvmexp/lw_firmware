/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrpolicy_perf_cf_pwr_model.h
 * @brief @copydoc pwrpolicy_perf_cf_pwr_model.c
 */

#ifndef PWRPOLICY_PERF_CF_PWR_MODEL_H
#define PWRPOLICY_PERF_CF_PWR_MODEL_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "boardobj/boardobj_interface.h"
#include "perf/cf/perf_cf_topology.h"
#include "perf/cf/perf_cf_pwr_model.h"

/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_POLICY_PERF_CF_PWR_MODEL PWR_POLICY_PERF_CF_PWR_MODEL;

/*!
 * Structure representing a PWR_POLICY_PERF_CF_PWR_MODEL interface.
 */
struct PWR_POLICY_PERF_CF_PWR_MODEL
{
    /*!
     * BOARDOBJ_INTERFACE super class. Must always be first object in the
     * structure.
     */
    BOARDOBJ_INTERFACE  super;
};

/*!
 * @interface   PWR_POLICY_PERF_CF_PWR_MODEL
 *
 * @brief   PWR Metrics Scale interface
 *
 * @param[in]      pPwrModel          Pointer to the PWR_POLICY_PERF_CF_PWR_MODEL object
 * @param[in]      pSampleData        Pointer to the PERF_CF_PWR_MODEL_SAMPLE_DATA
 * @param[in]      pObservedMetrics   Pointer to the observed pwrModel metrics @LW2080_CTRL_PMGR_PERF_CF_PWR_MODEL_METRICS
 *
 * @return  FLCN_OK     Metrics observed data initialized successfully
 * @return  other       Propagates return values from various calls
 */
#define PwrPolicyPerfCfPwrModelObserveMetrics(fname) FLCN_STATUS (fname)(  \
    PWR_POLICY_PERF_CF_PWR_MODEL                   *pPwrModel,             \
    PERF_CF_PWR_MODEL_SAMPLE_DATA                  *pSampleData,           \
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS     *pObservedMetrics       \
)

/*!
 * @interface   PWR_POLICY_PERF_CF_PWR_MODEL
 *
 * @brief   PWR Metrics Scale interface
 *
 * @deprecated  Use PwrPolicyPerfCfPwrModelScale going forward, it better
 *              conforms to the PERF_CF_PWR_MODEL interface of the same name.
 *
 * @param[in]      pPwrModel           Pointer to the PWR_POLICY_PERF_CF_PWR_MODEL object
 * @param[in]      pObservedMetrics    Pointer to the observed pwrModel metrics @LW2080_CTRL_PMGR_PWR_POLICY_PWR_MODEL_METRICS
 * @param[in]      pEstimatedMetrics   Pointer to the estimated pwrModel metrics @LW2080_CTRL_PMGR_PWR_POLICY_PWR_MODEL_METRICS
 *
 * @return  FLCN_OK     Metrics scale data initialized successfully
 * @return  other       Propagates return values from various calls
 */
#define PwrPolicyPerfCfPwrModelScaleMetrics(fname) FLCN_STATUS (fname)(  \
    PWR_POLICY_PERF_CF_PWR_MODEL                *pPwrModel,              \
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS  *pObservedMetrics,       \
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS  *pEstimatedMetrics       \
)

/*!
 * @interface   PWR_POLICY_PERF_CF_PWR_MODEL
 *
 * @brief   Scale all metrics in each PWR_POLICY_PERF_CF_PWR_MODEL
 *
 * @note    Similar to PwrPolicyPerfCfPwrModelScaleMetrics but interfaces well
 *          with PerfCfPwrModelScale
 *
 * @param[in]     pPwrModel         Pointer to the PWR_POLICY_PERF_CF_PWR_MODEL object
 * @param[in]     pObservedMetrics  Pointer to store observed metrics.
 * @param[in]     numMetricsInputs  Number of input structures to which to scale
 * @param[in]     pMetricsInputs    Array of input structures to which to scale
 * @param[out]    pEstimatedMetrics Array of pointers to structures in which to
 *                                  store estimated metrics.
 * @param[out]    pTypeParams       Class-specific parameters
 *
 * @return  FLCN_OK     All PWR_POLICY_PERF_CF_PWR_MODEL metrics scaled successfully
 * @return  other       Propagates return values from various calls
 */
#define PwrPolicyPerfCfPwrModelScale(fname) FLCN_STATUS (fname)(        \
    PWR_POLICY_PERF_CF_PWR_MODEL                   *pPwrModel,          \
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS     *pObservedMetrics,   \
    LwLength                                        numMetricsInputs,   \
    PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT          *pMetricsInputs,     \
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS    **ppEstimatedMetrics, \
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS            *pTypeParams         \
)

/* ------------------------- Defines --------------------------------------- */
/*!
 * Helper macro for the set of overlays required by PWR_POLICY_PERF_CF_PWR_MODEL
 * when @ref pwrPolicies3xCallbackBody() is called.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL)
#define PWR_POLICY_PERF_CF_PWR_MODEL_OVERLAYS                                  \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrPwrPolicyPerfCfPwrModel)         \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVoltApi)
#else
#define PWR_POLICY_PERF_CF_PWR_MODEL_OVERLAYS                                  \
    OSTASK_OVL_DESC_ILWALID
#endif

/*!
 * Helper macro for the set of overlays required by PWR_POLICY_PERF_CF_PWR_MODEL
 * when scale routines are called.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL)
#define PWR_POLICY_PERF_CF_PWR_MODEL_SCALE_OVERLAYS                            \
    PWR_POLICY_PERF_CF_PWR_MODEL_OVERLAYS                                      \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrPwrPolicyPerfCfPwrModelScale)
#else
#define PWR_POLICY_PERF_CF_PWR_MODEL_SCALE_OVERLAYS                            \
    OSTASK_OVL_DESC_ILWALID
#endif

/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */

/*!
 * BOARDOBJ_INTERFACE interfaces
 */
BoardObjInterfaceConstruct      (pwrPolicyConstructImpl_PWR_POLICY_PERF_CF_PWR_MODEL)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrPolicyConstructImpl_PWR_POLICY_PERF_CF_PWR_MODEL");

/*!
 * PWR_POLICY_PERF_CF_PWR_MODEL interfaces
 */
PwrPolicyPerfCfPwrModelObserveMetrics (pwrPolicyPerfCfPwrModelObserveMetrics)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyPerfCfPwrModel", "pwrPolicyPerfCfPwrModelObserveMetrics")
    GCC_ATTRIB_USED();
/*!
 * @deprecated  Use pwrPolicyPerfCfPwrModelScale going forward, it better
 *              conforms to the PERF_CF_PWR_MODEL interface of the same name.
 */
PwrPolicyPerfCfPwrModelScaleMetrics   (pwrPolicyPerfCfPwrModelScaleMetrics)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyPerfCfPwrModelScale", "pwrPolicyPerfCfPwrModelScaleMetrics");
PwrPolicyPerfCfPwrModelScale          (pwrPolicyPerfCfPwrModelScale)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyPerfCfPwrModelScale", "pwrPolicyPerfCfPwrModelScale");

#endif // PWRPOLICY_PERF_CF_PWR_MODEL_H
