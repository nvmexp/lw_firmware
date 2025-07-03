/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrpolicy_workload_single_1x.h
 * @brief @copydoc pwrpolicy_workload_single_1x.c
 */

#ifndef PWRPOLICY_WORKLOAD_SINGLE_1X_H
#define PWRPOLICY_WORKLOAD_SINGLE_1X_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrpolicy_perf_cf_pwr_model.h"
#include "pmgr/3x/pwrpolicy_35.h"

/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_POLICY_WORKLOAD_SINGLE_1X PWR_POLICY_WORKLOAD_SINGLE_1X;

/*!
 * Structure representing a workload-based power policy.
 */
struct PWR_POLICY_WORKLOAD_SINGLE_1X
{
    /*!
     * @copydoc PWR_POLICY
     */
    PWR_POLICY                    super;
    /*!
     * PERF_CF_PWR_MODEL interface
     */
    PWR_POLICY_PERF_CF_PWR_MODEL  pwrModel;
    /*!
     * Voltage policy table index required for applying voltage delta.
     */
    LwBoardObjIdx                 voltRailIdx;
    /*!
     * Clock that controls the rail @voltRailIdx
     */
    LwBoardObjIdx                 clkDomainIdx;
    /*!
     * Mask of clock domains where voltage stops scaling
     */
    BOARDOBJGRPMASK_E32           independentClkDomainMask;
    /*!
     * Sensed voltage mode. Specifies whether to use minimum, maximum or
     * average of the ADC samples for callwlating sensed voltage.
     * @ref LW2080_CTRL_VOLT_VOLT_RAIL_SENSED_VOLTAGE_<xyz>
     */
    LwU8                          voltMode;
    /*!
     * Boolean representing whether the dynamic power callwlation accounts for
     * Clock Gating on this rail or not
     */
    LwBool                        bClkGatingAware;
    /*!
     * Internal state of sensed voltage.
     * @note: Use unternal functions to copy in and out of
     * LW2080_CTRL_VOLT_RAIL_SENSED_VOLTAGE_DATA
     */
    VOLT_RAIL_SENSED_VOLTAGE_DATA voltData;
    /*!
     * Last counter and timestamp values to be used for callwlating average
     * frequency over the sampled power period.
     */
    LW2080_CTRL_CLK_CNTR_SAMPLE_ALIGNED   clkCntrStart;
    /*!
     * Boolean representing Dummy instance of SINGLE_1X (can be true in case
     * of merge rail design)
     */
    LwBool                        bDummy;
    /*!
     * Clock index to the Clock domain entry whose propogation logic needs
     * to be ignored.
     */
    LwBoardObjIdx                 ignoreClkDomainIdx;
     /*!
     *  Strulwtre to control soft floor behavior on secondary rail
     */
    LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_SINGLE_1X_SOFT_FLOOR softFloor;
};

/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Defines --------------------------------------- */
/*!
 * Macro to check if soft floor behavior on secondary rail is enabled
 */
#define pwrPolicySingle1xIsSoftFloorEnabled(pSingle1x)                                \
    (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X_LWDCLK_CTRL) &&  \
    (pSingle1x != NULL)                                                           &&  \
    pSingle1x->softFloor.bSoftFloor)

/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */

/*!
 * @brief   Helper function to determine if
 *          SINGLE_1X is a dummy policy
 *
 * @param[in]   pSingle1x    PWR_POLICY_WORKLOAD_SINGLE_1X pointer.
 *
 * @return
 *     LW_TRUE  if policy is a dummy
 *     LW_FALSE if policy is not a dummy
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwBool
pwrPolicyWorkloadSingle1xIsDummy
(
    PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1x
)
{
    if ((pSingle1x != NULL) &&
        (pSingle1x->bDummy == LW_TRUE))
    {
        return LW_TRUE; 
    }

    return LW_FALSE;
}

/*!
 * PWR_POLICY interfaces
 */
BoardObjGrpIfaceModel10ObjSet   (pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_SINGLE_1X)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_SINGLE_1X");
BoardObjIfaceModel10GetStatus       (pwrPolicyIfaceModel10GetStatus_WORKLOAD_SINGLE_1X)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrPolicyIfaceModel10GetStatus_WORKLOAD_SINGLE_1X");

/*!
 * PWR_POLICY_3X interfaces
 */
PwrPolicy3XChannelMaskGet (pwrPolicy3XChannelMaskGet_WORKLOAD_SINGLE_1X)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicy3XChannelMaskGet_WORKLOAD_SINGLE_1X");
PwrPolicyLoad             (pwrPolicyLoad_WORKLOAD_SINGLE_1X)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrPolicyLoad_WORKLOAD_SINGLE_1X");
PwrPolicy35PerfCfControllerMaskGet (pwrPolicy35PerfCfControllerMaskGet_WORKLOAD_SINGLE_1X)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicy35PerfCfControllerMaskGet_WORKLOAD_SINGLE_1X");

#define pwrPolicy3XFilter_WORKLOAD_SINGLE_1X pwrPolicy3XFilter_SUPER

/*!
 * PWR_POLICY_PERF_CF_PWR_MODEL interfaces
 */
PwrPolicyPerfCfPwrModelObserveMetrics (pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_SINGLE_1X)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyPerfCfPwrModel", "pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_SINGLE_1X");
/*!
 * @deprecated  Use pwrPolicyPerfCfPwrModelScale_WORKLOAD_SINGLE_1X going
 *              forward, it better conforms to the PERF_CF_PWR_MODEL interface
 *              of the same name.
 */
PwrPolicyPerfCfPwrModelScaleMetrics   (pwrPolicyPerfCfPwrModelScaleMetrics_WORKLOAD_SINGLE_1X)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyPerfCfPwrModelScale", "pwrPolicyPerfCfPwrModelScaleMetrics_WORKLOAD_SINGLE_1X");
PwrPolicyPerfCfPwrModelScale          (pwrPolicyPerfCfPwrModelScale_WORKLOAD_SINGLE_1X)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyPerfCfPwrModelScale", "pwrPolicyPerfCfPwrModelScale_WORKLOAD_SINGLE_1X");

FLCN_STATUS pwrPolicyMetricsInit_WORKLOAD_SINGLE_1X(PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1x, LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS *pMetrics)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyPerfCfPwrModel", "pwrPolicyMetricsInit_WORKLOAD_SINGLE_1X");

FLCN_STATUS pwrPolicyWorkloadSingle1xImportTgp1xWorkloadParameters(PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1x,
                                                                   LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_OBSERVED_METRICS_WORKLOAD_SINGLE_1X *pObsMetrics,
                                                                   LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TGP_1X_WORKLOAD_PARAMETERS *pTgp1xWorkloadParams)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyPerfCfPwrModelScale", "pwrPolicyWorkloadSingle1xImportTgp1xWorkloadParameters");

#endif // PWRPOLICY_WORKLOAD_SINGLE_1X_H
