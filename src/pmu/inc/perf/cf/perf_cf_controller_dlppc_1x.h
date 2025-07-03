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
 * @file    perf_cf_controller_dlppc_1x.h
 * @brief   DLPPC 1.X PERF_CF Controller related defines.
 *
 * @copydoc perf_cf.h
 */

#ifndef PERF_CF_CONTROLLER_DLPPC_1X_H
#define PERF_CF_CONTROLLER_DLPPC_1X_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_controller.h"
#include "pmu_objpmgr.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_CF_CONTROLLER_DLPPC_1X PERF_CF_CONTROLLER_DLPPC_1X;

/* ------------------------ Macros and Defines ------------------------------ */
// Overlays needed for DLPPC_1X evaluate
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_DLPPC_1X))
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_CONTROLLER_DLPPC_1X_EVALUATE           \
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfLimitVf)                    \
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, dlppc1xMetricsScratch)

#else
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_CONTROLLER_DLPPC_1X_EVALUATE           \
        OSTASK_OVL_DESC_ILWALID

#endif // PMU_PERF_CF_CONTROLLER_DLPPC_1X

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * PERF_CF_CONTROLLER child class providing attributes of
 * DLPPC 1.X PERF_CF Controller.
 */
struct PERF_CF_CONTROLLER_DLPPC_1X
{
    /*!
     * PERF_CF_CONTROLLER parent class.
     *
     * Must be first element of the structure!
     */
    PERF_CF_CONTROLLER                                             super;

    /*!
     * Latest estimated metrics based off of @ref observedMetrics.
     */
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_DRAMCLK
        dramclk[LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_DRAMCLK_NUM];

    /*!
     * Core rail state.
     */
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_CORE_RAIL         coreRail;

    /*!
     * @copydoc LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X::coreRailStatus
     */
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_CORE_RAIL_STATUS  coreRailStatus;

    /*!
     * FBVDD rail state.
     */
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_RAIL              fbRail;

    /*!
     * @copydoc LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X::fbRailStatus
     */
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_RAIL_STATUS       fbRailStatus;

    /*!
     * TGP policy relationship set.
     */
    LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_SET                   tgpPolRelSet;

    /*!
     * @copydoc LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X::tgpLimits
     */
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PWR_POLICY_RELATIONSHIP_SET_LIMITS tgpLimits;

    /*!
     * Data sampled for this controller.
     */
    PERF_CF_PWR_MODEL_SAMPLE_DATA                                  sampleData;

    /*!
     * Latest observed metrics from the sampled data.
     */
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED   observedMetrics;

    /*!
     * Latest perf target that DLPPC_1X is trying to achieve.
     */
    LwUFXP20_12                                                    perfTarget;

    /*!
     * Index of the DLPPM model to query for perf and power estimates.
     */
    LwBoardObjIdx                                                  dlppmIdx;

    /*!
     * Number of elements in @ref dramclk that are valid.
     */
    LwU8                                                           numDramclk;

    /*!
     * Number of conselwtive cycles of estimating a higher dramclk freq
     * before switching to that freq
     */
    LwU8                                                           dramHysteresisCountInc;

    /*!
     * Number of conselwtive cycles of estimating a lower dramclk freq
     * before switching to that freq
     */
    LwU8                                                           dramHysteresisCountDec;

    /*!
     * Current count of conselwtive cycles of estimating a higher dramclk freq
     */
    LwU8                                                           dramHysteresisLwrrCountInc;

    /*!
     * Current count of conselwtive cycles of estimating a lower dramclk freq
     */
    LwU8                                                           dramHysteresisLwrrCountDec;

    /*!
     * Tuple of frequencies that hold the previous loop's operating frequncies
     */
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_FREQ_TUPLE    tupleLastOperatingFrequencies;

    /*!
     * Structure holding the frequency recommendations that the controller chose in its
     * previous exelwtion for the arbirter.
     */
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_FREQ_TUPLE    tupleLastChosenLimits;

    /*!
     * Structure holding the limit recommendations that the controller estimated
     * to be the best operating frequencies based on it's current exelwtion.
     */
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_FREQ_TUPLE    tupleBestEstimatedLimits;

    /*!
     * Maximum percent error in dramclk frequency deviation. 1.12 FXP number
     */
    LwU32                                                             dramclkMatchTolerance;

    /*!
     * Whether the controller supports OPTP behavior
     */
    LwBool                                                            bOptpSupported;

    /*!
     * Whether the controller supports LWCA
     */
    LwBool                                                            bLwdaSupported;

    /*!
     * Whether any of the @ref LW2080_CTRL_PMGR_PWR_POLICY_INFO objects with
     * which DLPPC_1X is concerned have an inflection limit below DLPPC_1X's
     * @ref LW2080_CTRL_PERF_VPSTATES_IDX_DLPPC_1X_SEARCH_MINIMUM corresponding
     * @ref LW2080_CTRL_PERF_PSTATE_INFO.
     *
     * If that is the case, DLPPC_1X defers back to the enabling the inflection
     * points.
     */
    LwBool                                                            bInInflectionZone;

    /*!
     * Callers can set specified data that is passed to PERF_CF_POWER_MODELS as
     * part of SAMPLE_DATA
     */
    PERF_CF_PWR_MODEL_CALLER_SPECIFIED_SAMPLE_DATA                    pwrModelCallerSpecifiedData;

    /*!
     * Signed Value between (-1,1) indicating the minimum percent
     * difference in two perfRatio values needed to be seen
     * for them to be consiered not equal to one another.
     * Note: this value is asymetric due to being signed.
     *
     * When positive, the threshold indicates the MIN percent diff GAIN in perf
     * a higher dramclk has to see compared to a lower dramclk in order for the
     * two dramclks to have "different" perf values
     *
     * When negative, the threshold indicates the amount of LOSS in perf
     * a higher dramclk can see compared to a lower dramclk in order for the
     * perf values of the two dramclks to be considered roughly equal.
     */
    LwSFXP20_12                                                        approxPerfThreshold;

    /*!
     * @copydoc LW2080_CTRL_PERF_PERF_CF_CONTROLLER_CONTROL_DLPPC_1X::approxPowerThresholdPct
     */
    LwSFXP20_12                                                       approxPowerThresholdPct;

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING)
    /*!
     * Profiling data for last iteration of @ref perfCfControllerExelwte_DLPPC_1X
     */
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING            profiling;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING)
};

/* ------------------------ Global Variables -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet   (perfCfControllerGrpIfaceModel10ObjSetImpl_DLPPC_1X)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfControllerGrpIfaceModel10ObjSetImpl_DLPPC_1X");
BoardObjIfaceModel10GetStatus       (perfCfControllerIfaceModel10GetStatus_DLPPC_1X)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfControllerIfaceModel10GetStatus_DLPPC_1X");

// PERF_CF_CONTROLLER interfaces
PerfCfControllerExelwte   (perfCfControllerExelwte_DLPPC_1X);
PerfCfControllerLoad      (perfCfControllerLoad_DLPPC_1X)
    GCC_ATTRIB_SECTION("imem_perfCfLoad", "perfCfControllerLoad_DLPPC_1X");
PerfCfControllerSetMultData   (perfCfControllerSetMultData_DLPPC_1X)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfControllerSetMultData_DLPPC_1X");

/* ------------------------ Inline Functions -------------------------------- */
/*!
 * @brief   Retrieves the limit for a particular @ref PWR_POLICY_RELATIONSHIP in
 *          a given @ref LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_SET
 *
 * @param[in]   pRelSet     Relationship set for set of limits.
 * @param[in]   pLimits     Set of limits for pRelSet from which to retrieve a
 *                          particular limit.
 * @param[in]   polRelIdx   Index of the @ref PWR_POLICY_RELATIONSHIP for which
 *                          to retrieve limit.
 * @param[out]  pLimit      Limit for given polRelIdx
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  pRelSet, pLimits, or pLimit is NULL,
 *                                          or pRelSet and polRelIdx do not
 *                                          correspond to a valid index in
 *                                          pLimits
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfCfControllerDlppc1xPwrPolicyRelationshipSetLimitsLimitGet
(
    const LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_SET *pRelSet,
    const LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PWR_POLICY_RELATIONSHIP_SET_LIMITS *pLimits,
    LwBoardObjIdx polRelIdx,
    LwU32 *pLimit
)
{
    FLCN_STATUS status;
    LwBoardObjIdx limitIdx;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pRelSet != NULL) && (pLimits != NULL) && (pLimit != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllerDlppc1xPwrPolicyRelationshipSetLimitsLimitGet_exit);

    limitIdx = LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PWR_POLICY_RELATIONSHIP_SET_LIMITS_IDX_FROM_POLICY_REL_IDX(
        pRelSet, polRelIdx);

    PMU_ASSERT_TRUE_OR_GOTO(status,
        limitIdx < LW_ARRAY_ELEMENTS(pLimits->policyLimits),
        LW_ERR_ILWALID_ARGUMENT,
        perfCfControllerDlppc1xPwrPolicyRelationshipSetLimitsLimitGet_exit);

    *pLimit = pLimits->policyLimits[limitIdx];

perfCfControllerDlppc1xPwrPolicyRelationshipSetLimitsLimitGet_exit:
    return status;
}

/*!
 * @brief   Retrieves the limit for a particular @ref PWR_POLICY_RELATIONSHIP in
 *          a given @ref LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_SET
 *
 * @param[in]   pRelSet     Relationship set for set of limits.
 * @param[in]   pLimits     Set of limits for pRelSet from which to retrieve a
 *                          particular limit.
 * @param[in]   polRelIdx   Index of the @ref PWR_POLICY_RELATIONSHIP for which
 *                          to retrieve limit.
 * @param[out]  limit       Limit to set for given polRelIdx
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  pRelSet or pLimits is NULL,
 *                                          or pRelSet and polRelIdx do not
 *                                          correspond to a valid index in
 *                                          pLimits
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfCfControllerDlppc1xPwrPolicyRelationshipSetLimitsLimitSet
(
    const LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_SET *pRelSet,
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PWR_POLICY_RELATIONSHIP_SET_LIMITS *pLimits,
    LwBoardObjIdx polRelIdx,
    LwU32 limit
)
{
    FLCN_STATUS status;
    LwBoardObjIdx limitIdx;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pRelSet != NULL) && (pLimits != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllerDlppc1xPwrPolicyRelationshipSetLimitsLimitSet_exit);

    limitIdx = LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PWR_POLICY_RELATIONSHIP_SET_LIMITS_IDX_FROM_POLICY_REL_IDX(
        pRelSet, polRelIdx);

    PMU_ASSERT_TRUE_OR_GOTO(status,
        limitIdx < LW_ARRAY_ELEMENTS(pLimits->policyLimits),
        LW_ERR_ILWALID_ARGUMENT,
        perfCfControllerDlppc1xPwrPolicyRelationshipSetLimitsLimitSet_exit);

    pLimits->policyLimits[limitIdx] = limit;

perfCfControllerDlppc1xPwrPolicyRelationshipSetLimitsLimitSet_exit:
    return status;
}

/*!
 * @brief   Retrieves @ref PERF_CF_CONTROLLER_DLPPC_1X::profiling, if enabled
 *
 * @param[in]   pDlppc1x    Pointer to DLPPC_1X from which to retrieve
 * @param[out]  ppProfiling Pointer into which to store pointer to
 *                          pDlppc1x->profiling; NULL if disabled
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  A pointer argument was NULL
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfCfControllerDlppc1xProfilingGet
(
    PERF_CF_CONTROLLER_DLPPC_1X *pDlppc1x,
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING **ppProfiling
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDlppc1x != NULL) &&
        (ppProfiling != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllerDlppc1xProfilingGet_exit);

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING)
    *ppProfiling = &pDlppc1x->profiling;
#else
    *ppProfiling = NULL;
#endif // PMU_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING

perfCfControllerDlppc1xProfilingGet_exit:
    return status;
}

/*!
 * @brief   Retrieves an element of
 *          @ref LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING::dramclkSearchProfiling[],
 *          if enabled
 *
 * @param[in]   pProfiling          Pointer to structure from which to retrieve
 * @param[in]   dramclkIdx          Index of element to retrieve
 * @param[out]  ppDramclkProfiling  Pointer into which to store pointer to
 *                                  DRAMCLK search profiling structure; NULL if
 *                                  not enabled
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  A pointer argument was NULL
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfCfControllerDlppc1xProfilingDramclkSearchProfilingGet
(
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING *pProfiling,
    LwU8 dramclkIdx,
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH **ppDramclkProfiling
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (ppDramclkProfiling != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllerDlppc1xProfilingDramclkSearchProfilingGet_exit);

    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING))
    {
        *ppDramclkProfiling = NULL;
        goto perfCfControllerDlppc1xProfilingDramclkSearchProfilingGet_exit;
    }

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pProfiling != NULL) &&
        (dramclkIdx < LW_ARRAY_ELEMENTS(pProfiling->dramclkSearchProfiling)),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllerDlppc1xProfilingDramclkSearchProfilingGet_exit);

    *ppDramclkProfiling = &pProfiling->dramclkSearchProfiling[dramclkIdx];

perfCfControllerDlppc1xProfilingDramclkSearchProfilingGet_exit:
    return status;
}

/*!
 * @brief   Resets a @ref LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING
 *          structure
 *
 * @param[out]  pProfiling Pointer to structure to reset
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void 
perfCfControllerDlppc1xProfilingReset
(
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING *pProfiling
)
{
    FLCN_STATUS status;

    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING))
    {
        goto perfCfControllerDlppc1xProfilingReset_exit;
    }

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pProfiling != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllerDlppc1xProfilingReset_exit);

    (void)memset(pProfiling, 0, sizeof(*pProfiling));

perfCfControllerDlppc1xProfilingReset_exit:
    lwosNOP();
}

/*!
 * @brief   Begins the profiling for a given region of code in
 *          @ref perfCfControllerExelwte_DLPPC_1X.
 *
 * @details This works by subtracting the current time from zero, giving us
 *          NEGATIVE(lwrrentTime). The "end" function can then add the
 *          then-current time to the value to get the elapsed time, i.e.,
 *          endTime - starTime == (-startTime) + endTime.
 *
 * @param[out]  pProfiling      Pointer to
 *                              @ref LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING
 *                              structure in which to start profiling.
 * @param[in]   region          Region to start profiling.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
perfCfControllerDlppc1xProfileRegionBegin
(
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING *pProfiling,
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_REGION_ENUM region
)
{
    FLCN_STATUS status;
    FLCN_TIMESTAMP timensLwrr;
    LwU64 timensResult;

    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING))
    {
        goto perfCfControllerDlppc1xProfileRegionBegin_exit;
    }

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pProfiling != NULL) &&
        (region < LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_REGION__COUNT),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllerDlppc1xProfileRegionBegin_exit);

    osPTimerTimeNsLwrrentGet(&timensLwrr);

    lw64Sub_MOD(&timensResult, &(LwU64){0U}, &timensLwrr.data);
    LwU64_ALIGN32_PACK(
        &pProfiling->profiledTimesns[region], &timensResult);

perfCfControllerDlppc1xProfileRegionBegin_exit:
    lwosNOP();
}

/*!
 * @brief   Ends the profiling for a given region of code in
 *          @ref perfCfControllerExelwte_DLPPC_1X, storing the elapsed time.
 *
 * @param[out]  pProfiling      Pointer to
 *                              @ref LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING
 *                              structure in which to end profiling.
 * @param[in]   region          Region for which to end profiling.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
perfCfControllerDlppc1xProfileRegionEnd
(
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING *pProfiling,
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_REGION_ENUM region
)
{
    FLCN_STATUS status;
    FLCN_TIMESTAMP timensLwrr;
    LwU64 timensResult;

    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING))
    {
        goto perfCfControllerDlppc1xProfileRegionEnd_exit;
    }

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pProfiling != NULL) &&
        (region < LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_REGION__COUNT),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllerDlppc1xProfileRegionEnd_exit);

    osPTimerTimeNsLwrrentGet(&timensLwrr);

    LwU64_ALIGN32_UNPACK(
        &timensResult, &pProfiling->profiledTimesns[region]);
    lw64Add_MOD(&timensResult, &timensResult, &timensLwrr.data);
    LwU64_ALIGN32_PACK(
        &pProfiling->profiledTimesns[region], &timensResult);

perfCfControllerDlppc1xProfileRegionEnd_exit:
    lwosNOP();
}

/*!
 * @brief   Retrieves
 *          @ref LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH::primaryClkMinEstimated,
 *          if enabled
 *
 * @param[in]   pDramclkProfiling   Pointer to structure from which to retrieve
 * @param[out]  ppScaleProfiling    Pointer into which to store pointer to
 *                                  scale profiling structure; NULL if
 *                                  not enabled
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  A pointer argument was NULL
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfCfControllerDlppc1xProfilingDramclkSearchPrimaryClkMinEstimatedProfilingGet
(
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH *pDramclkProfiling,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_SCALE_PROFILING **ppScaleProfiling
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (ppScaleProfiling != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllerDlppc1xProfilingDramclkSearchPrimaryClkMinEstimatedProfilingGet_exit);

    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING))
    {
        *ppScaleProfiling = NULL;
        goto perfCfControllerDlppc1xProfilingDramclkSearchPrimaryClkMinEstimatedProfilingGet_exit;
    }

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDramclkProfiling != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllerDlppc1xProfilingDramclkSearchPrimaryClkMinEstimatedProfilingGet_exit);

    *ppScaleProfiling = &pDramclkProfiling->primaryClkMinEstimatedProfiling;

perfCfControllerDlppc1xProfilingDramclkSearchPrimaryClkMinEstimatedProfilingGet_exit:
    return status;
}

/*!
 * @brief   Retrieves
 *          @ref LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH::powerCeilingProfiling,
 *          if enabled
 *
 * @param[in]   pDramclkProfiling       Pointer to structure from which to retrieve
 * @param[out]  ppPowerCeilingProfiling Pointer into which to store pointer to
 *                                      profiling structure; NULL if not enabled
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  A pointer argument was NULL
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfCfControllerDlppc1xProfilingDramclkSearchPowerCeilingProfilingGet
(
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH *pDramclkProfiling,
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH_TYPE **ppPowerCeilingProfiling
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (ppPowerCeilingProfiling != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllerDlppc1xProfilingDramclkSearchPowerCeilingProfilingGet_exit);

    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING))
    {
        *ppPowerCeilingProfiling = NULL;
        goto perfCfControllerDlppc1xProfilingDramclkSearchPowerCeilingProfilingGet_exit;
    }

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDramclkProfiling != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllerDlppc1xProfilingDramclkSearchPowerCeilingProfilingGet_exit);

    *ppPowerCeilingProfiling = &pDramclkProfiling->powerCeilingProfiling;

perfCfControllerDlppc1xProfilingDramclkSearchPowerCeilingProfilingGet_exit:
    return status;
}

/*!
 * @brief   Retrieves
 *          @ref LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH::perfFloorProfiling,
 *          if enabled
 *
 * @param[in]   pDramclkProfiling       Pointer to structure from which to retrieve
 * @param[out]  ppPerfFloorProfiling    Pointer into which to store pointer to
 *                                      profiling structure; NULL if not enabled
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  A pointer argument was NULL
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfCfControllerDlppc1xProfilingDramclkSearchPerfFloorProfilingGet
(
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH *pDramclkProfiling,
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH_TYPE **ppPerfFloorProfiling
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (ppPerfFloorProfiling != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllerDlppc1xProfilingDramclkSearchPerfFloorProfilingGet_exit);

    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING))
    {
        *ppPerfFloorProfiling = NULL;
        goto perfCfControllerDlppc1xProfilingDramclkSearchPerfFloorProfilingGet_exit;
    }

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDramclkProfiling != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllerDlppc1xProfilingDramclkSearchPerfFloorProfilingGet_exit);

    *ppPerfFloorProfiling = &pDramclkProfiling->perfFloorProfiling;

perfCfControllerDlppc1xProfilingDramclkSearchPerfFloorProfilingGet_exit:
    return status;
}

/*!
 * @brief   Begins the profiling for the search of a given DRAMCLK in
 *          @ref perfCfControllerExelwte_DLPPC_1X.
 *
 * @details This works by subtracting the current time from zero, giving us
 *          NEGATIVE(lwrrentTime). The "end" function can then add the
 *          then-current time to the value to get the elapsed time, i.e.,
 *          endTime - starTime == (-startTime) + endTime.
 *
 * @param[out]  pProfiling      Pointer to
 *                              @ref LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH
 *                              structure in which to start profiling.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
perfCfControllerDlppc1xProfileDramclkSearchBegin
(
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH *pProfiling
)
{
    FLCN_STATUS status;
    FLCN_TIMESTAMP timensLwrr;
    LwU64 timensResult;

    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING))
    {
        goto perfCfControllerDlppc1xProfileDramclkSearchBegin_exit;
    }

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pProfiling != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllerDlppc1xProfileDramclkSearchBegin_exit);

    osPTimerTimeNsLwrrentGet(&timensLwrr);

    lw64Sub_MOD(&timensResult, &(LwU64){0U}, &timensLwrr.data);
    LwU64_ALIGN32_PACK(
        &pProfiling->profiledTimens, &timensResult);

perfCfControllerDlppc1xProfileDramclkSearchBegin_exit:
    lwosNOP();
}

/*!
 * @brief   Ends the profiling for the search of a given DRAMCLK in
 *          @ref perfCfControllerExelwte_DLPPC_1X, storing the elapsed time.
 *
 * @param[out]  pProfiling      Pointer to
 *                              @ref LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH
 *                              structure in which to end profiling.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
perfCfControllerDlppc1xProfileDramclkSearchEnd
(
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH *pProfiling
)
{
    FLCN_STATUS status;
    FLCN_TIMESTAMP timensLwrr;
    LwU64 timensResult;

    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING))
    {
        goto perfCfControllerDlppc1xProfileDramclkSearchEnd_exit;
    }

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pProfiling != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllerDlppc1xProfileDramclkSearchEnd_exit);

    osPTimerTimeNsLwrrentGet(&timensLwrr);

    LwU64_ALIGN32_UNPACK(
        &timensResult, &pProfiling->profiledTimens);
    lw64Add_MOD(&timensResult, &timensResult, &timensLwrr.data);
    LwU64_ALIGN32_PACK(
        &pProfiling->profiledTimens, &timensResult);

perfCfControllerDlppc1xProfileDramclkSearchEnd_exit:
    lwosNOP();
}

/*!
 * @brief   Retrieves the next
 *          @ref LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH_TYPE::iteration
 *          element, if enabled and available
 *
 * @param[in]   pProfiling  Pointer to structure from which to retrieve
 * @param[out]  ppIteration Pointer into which to store pointer to
 *                          profiling structure; NULL if not enabled
 *                          or no structures available
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  A pointer argument was NULL
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfCfControllerDlppc1xProfilingDramclkSearchTypeIterationGetNext
(
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH_TYPE *pProfiling,
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH_TYPE_ITERATION **ppIteration
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (ppIteration != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllerDlppc1xProfilingDramclkSearchTypeIterationGetNext_exit);

    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING))
    {
        *ppIteration = NULL;
        goto perfCfControllerDlppc1xProfilingDramclkSearchTypeIterationGetNext_exit;
    }

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pProfiling != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllerDlppc1xProfilingDramclkSearchTypeIterationGetNext_exit);

    //
    // Increment the number of iterations. Do this regardless of whether we're
    // beyond the max number, so that we can know if we drop any.
    //
    pProfiling->numIterations++;

    //
    // If we've run out of free iteration structures, then return a NULL
    // pointer. Otherwise, return the next unused structure.
    //
    if (pProfiling->numIterations > LW_ARRAY_ELEMENTS(pProfiling->iterations))
    {
        *ppIteration = NULL;
    }
    else
    {
        *ppIteration = &pProfiling->iterations[pProfiling->numIterations - 1];
    }

perfCfControllerDlppc1xProfilingDramclkSearchTypeIterationGetNext_exit:
    return status;
}

/*!
 * @brief   Begins the profiling for a particular search type for a a given
 *          DRAMCLK in @ref perfCfControllerExelwte_DLPPC_1X.
 *
 * @details This works by subtracting the current time from zero, giving us
 *          NEGATIVE(lwrrentTime). The "end" function can then add the
 *          then-current time to the value to get the elapsed time, i.e.,
 *          endTime - starTime == (-startTime) + endTime.
 *
 * @param[out]  pProfiling      Pointer to
 *                              @ref LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH_TYPE
 *                              structure in which to start profiling.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
perfCfControllerDlppc1xProfileDramclkSearchTypeBegin
(
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH_TYPE *pProfiling
)
{
    FLCN_STATUS status;
    FLCN_TIMESTAMP timensLwrr;
    LwU64 timensResult;

    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING))
    {
        goto perfCfControllerDlppc1xProfileDramclkSearchTypeBegin_exit;
    }

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pProfiling != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllerDlppc1xProfileDramclkSearchTypeBegin_exit);

    osPTimerTimeNsLwrrentGet(&timensLwrr);

    lw64Sub_MOD(&timensResult, &(LwU64){0U}, &timensLwrr.data);
    LwU64_ALIGN32_PACK(
        &pProfiling->profiledTimens, &timensResult);

perfCfControllerDlppc1xProfileDramclkSearchTypeBegin_exit:
    lwosNOP();
}

/*!
 * @brief   Ends the profiling for a particular search type for a a given
 *          DRAMCLK in @ref perfCfControllerExelwte_DLPPC_1X, storing the
 *          elapsed time.
 *
 * @param[out]  pProfiling      Pointer to
 *                              @ref LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH_TYPE
 *                              structure in which to end profiling.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
perfCfControllerDlppc1xProfileDramclkSearchTypeEnd
(
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH_TYPE *pProfiling
)
{
    FLCN_STATUS status;
    FLCN_TIMESTAMP timensLwrr;
    LwU64 timensResult;

    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING))
    {
        goto perfCfControllerDlppc1xProfileDramclkSearchTypeEnd_exit;
    }

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pProfiling != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllerDlppc1xProfileDramclkSearchTypeEnd_exit);

    osPTimerTimeNsLwrrentGet(&timensLwrr);

    LwU64_ALIGN32_UNPACK(
        &timensResult, &pProfiling->profiledTimens);
    lw64Add_MOD(&timensResult, &timensResult, &timensLwrr.data);
    LwU64_ALIGN32_PACK(
        &pProfiling->profiledTimens, &timensResult);

perfCfControllerDlppc1xProfileDramclkSearchTypeEnd_exit:
    lwosNOP();
}

/*!
 * @brief   Retrieves
 *          @ref LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH_TYPE_ITERATION::scaleProfiling,
 *          if enabled
 *
 * @param[in]   pProfiling          Pointer to structure from which to retrieve;
 *                                  *may* be NULL to indicate unavailability
 * @param[out]  ppScaleProfiling    Pointer into which to store pointer to
 *                                  profiling structure; NULL if not enabled or
 *                                  pProfiling not available
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  Output pointer argument was NULL
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfCfControllerDlppc1xProfilingDramclkSearchTypeIterationScaleProfilingGet
(
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH_TYPE_ITERATION *pProfiling,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_SCALE_PROFILING **ppScaleProfiling
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (ppScaleProfiling != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllerDlppc1xProfilingDramclkSearchTypeIterationScaleProfilingGet_exit);

    //
    // Exit early either if the feature is not enabled or the pointer is NULL.
    // Allowing the pointer to be NULL lets us handle cases where we run out of
    // available iteration profiling structures.
    //
    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING) ||
        (pProfiling == NULL))
    {
        *ppScaleProfiling = NULL;
        goto perfCfControllerDlppc1xProfilingDramclkSearchTypeIterationScaleProfilingGet_exit;
    }

    *ppScaleProfiling = &pProfiling->scaleProfiling;

perfCfControllerDlppc1xProfilingDramclkSearchTypeIterationScaleProfilingGet_exit:
    return status;
}

/*!
 * @brief   Begins the profiling for a new iteration of particular search type
 *          for a given DRAMCLK in @ref perfCfControllerExelwte_DLPPC_1X.
 *
 * @details This works by subtracting the current time from zero, giving us
 *          NEGATIVE(lwrrentTime). The "end" function can then add the
 *          then-current time to the value to get the elapsed time, i.e.,
 *          endTime - starTime == (-startTime) + endTime.
 *
 * @param[out]  pProfiling      Pointer to
 *                              @ref LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH_TYPE_ITERATION
 *                              structure in which to start profiling.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
perfCfControllerDlppc1xProfileDramclkSearchTypeIterationBegin
(
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH_TYPE_ITERATION *pProfiling
)
{
    FLCN_TIMESTAMP timensLwrr;
    LwU64 timensResult;

    //
    // Exit early either if the feature is not enabled or the pointer is NULL.
    // Allowing the pointer to be NULL lets us handle cases where we run out of
    // available iteration profiling structures.
    //
    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING) ||
        (pProfiling == NULL))
    {
        goto perfCfControllerDlppc1xProfileDramclkSearchTypeIterationBegin_exit;
    }

    osPTimerTimeNsLwrrentGet(&timensLwrr);

    lw64Sub_MOD(&timensResult, &(LwU64){0U}, &timensLwrr.data);
    LwU64_ALIGN32_PACK(
        &pProfiling->profiledTimens, &timensResult);

perfCfControllerDlppc1xProfileDramclkSearchTypeIterationBegin_exit:
    lwosNOP();
}

/*!
 * @brief   Ends the profiling for a new iteration of particular search type
 *          for a given DRAMCLK in @ref perfCfControllerExelwte_DLPPC_1X,
 *          storing the elapsed time.
 *
 * @param[out]  pProfiling      Pointer to
 *                              @ref LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH_TYPE_ITERATION
 *                              structure in which to end profiling.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
perfCfControllerDlppc1xProfileDramclkSearchTypeIterationEnd
(
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH_TYPE_ITERATION *pProfiling
)
{
    FLCN_TIMESTAMP timensLwrr;
    LwU64 timensResult;

    //
    // Exit early either if the feature is not enabled or the pointer is NULL.
    // Allowing the pointer to be NULL lets us handle cases where we run out of
    // available iteration profiling structures.
    //
    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING) ||
        (pProfiling == NULL))
    {
        goto perfCfControllerDlppc1xProfileDramclkSearchTypeIterationEnd_exit;
    }

    osPTimerTimeNsLwrrentGet(&timensLwrr);

    LwU64_ALIGN32_UNPACK(
        &timensResult, &pProfiling->profiledTimens);
    lw64Add_MOD(&timensResult, &timensResult, &timensLwrr.data);
    LwU64_ALIGN32_PACK(
        &pProfiling->profiledTimens, &timensResult);

perfCfControllerDlppc1xProfileDramclkSearchTypeIterationEnd_exit:
    lwosNOP();
}

#endif // PERF_CF_CONTROLLER_DLPPC_1X_H
