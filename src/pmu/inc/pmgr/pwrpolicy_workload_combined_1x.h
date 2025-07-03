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
 * @file  pwrpolicy_workload_combined_1x.h
 * @brief @copydoc pwrpolicy_workload_combined_1x.c
 */

#ifndef PWRPOLICY_WORKLOAD_COMBINED_1X_H
#define PWRPOLICY_WORKLOAD_COMBINED_1X_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrpolicy_perf_cf_pwr_model.h"
#include "pmgr/3x/pwrpolicy_35.h"

/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_POLICY_WORKLOAD_COMBINED_1X PWR_POLICY_WORKLOAD_COMBINED_1X;

/*!
 * @brief Structure containing the observed metrics for WORKLOAD_COMBINED_1X.
 *
 * This structure is internal to PMU and similar to the structure @ref
 * LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_OBSERVED_METRICS_WORKLOAD_COMBINED_1X
 * with the only difference that the array of singles is right-sized to the
 * number of SINGLE_1X policies.
 */
typedef struct
{
    /*!
     * super - must be the first member of the structure
     */
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS  super;
    /*!
     * Power in mW/Current in mA which is sum of the observedVal of all
     * SINGLE_1X rails referenced by this policy.
     */
    LwU32 observedVal;
    /*!
     * Previous evaluation and sleep times for a lpwr feature.
     * Records the timestamp when this policy was last evaluated.
     */
    PWR_POLICY_WORKLOAD_LPWR_TIME lpwrTime[LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MAX];
    /*!
     * Pointer to buffer containing the observed metrics of SINGLE_1X policies
     * referenced by this policy.
     */
    LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_OBSERVED_METRICS_WORKLOAD_SINGLE_1X
         *pSingles;
} PWR_POLICY_WORKLOAD_COMBINED_1X_OBSERVED_METRICS;

/*!
 * @brief Structure containing the estimated metrics for WORKLOAD_COMBINED_1X.
 *
 * This structure is internal to PMU and similar to the structure @ref
 * LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_ESTIMATED_METRICS_WORKLOAD_COMBINED_1X
 * with the only difference that the array of singles is right-sized to the
 * number of SINGLE_1X policies.
 */
typedef struct
{
    /*!
     * super - must be the first member of the structure
     */
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS  super;
    /*!
     * Power in mW/Current in mA which is sum of the observedVal of all
     * SINGLE_1X rails referenced by this policy.
     */
    LwU32 estimatedVal;
    /*!
     * Pointer to buffer containing the estimated metrics of SINGLE_1X policies
     * referenced by this policy.
     */
    LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_ESTIMATED_METRICS_WORKLOAD_SINGLE_1X
        *pSingles;
} PWR_POLICY_WORKLOAD_COMBINED_1X_ESTIMATED_METRICS;

/*!
 * TODO 
 */
#define PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_CLK_PROP_TOP_IDX_PARENT_FLOOR (LW2080_CTRL_CLK_CLK_PROP_TOP_IDX_ILWALID - 1)

/*!
 * @brief Init data required to construct
 * @ref LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_SEARCH_REGIME
 * array
 */
typedef struct
{
    /*!
     * Mask of immediate child regimes
     */
    BOARDOBJGRPMASK_E32 childRegimeMask;
    /*!
     * Clock propogation topology index used by the regime.
     *
     * @note @ref
     * PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_CLK_PROP_TOP_IDX_PARENT_FLOOR
     * is a special value indicating that should take frequency floor
     * from parent REGIME instead of propagating.
     */
    LwBoardObjIdx       clkPropTopIdx;
    /*!
     * regime ID @ref LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_SEARCH_REGIME_ID
     */
    LwU8                regimeId;
    /*!
     * clock index @ref PWR_POLICY_WORKLOAD_COMBINED_1X_SINGLE_1X_XYZ
     */
    LwU8                singleIdx;
} PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_INIT_DATA;

/*!
 * @brief Data required to construct
 * @ref LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_SEARCH_REGIME
 * array
 */
typedef struct
{
    /*!
     * @copydoc PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_INIT_DATA
     */
    PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_INIT_DATA regimeInitData;
    /*!
     * Frequency end points i.e MAX frequency of a regime
     * indexed by @ref PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_CLK_XYZ
     */
    LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_SINGLES_FREQ_TUPLE regimeFreq;
    /*!
     * Boolean to indicate if limit is satisfied at the frequency
     * endpoints
     */
    LwBool bLimitSatisfied;
} PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_DATA;

/*!
 * Structure representing a workload-based power policy.
 */
struct PWR_POLICY_WORKLOAD_COMBINED_1X
{
    /*!
     * @copydoc PWR_POLICY
     */
    PWR_POLICY_DOMGRP             super;
    /*!
     * PERF_CF_PWR_MODEL interface
     */
    PWR_POLICY_PERF_CF_PWR_MODEL  pwrModel;
    /*!
     * Ratio by which to scale primary clock changes up.
     */
    LwUFXP4_12                    clkPrimaryUpScale;
    /*!
     * Ratio by which to scale primary clock changes down.
     */
    LwUFXP4_12                    clkPrimaryDownScale;
    /*!
     * Power Policy Table Relationship index corresponding to first SINGLE_1X
     * Policy to be used by this combined policy. The first SINGLE_1X policy
     * always needs to be associated with the primary clock domain.
     */
    LwBoardObjIdx                 singleRelIdxFirst;
    /*!
     * Power Policy Table Relationship index corresponding to last SINGLE_1X
     * Policy to be used by this combined policy.
     */
    LwBoardObjIdx                 singleRelIdxLast;
    /*!
     * Number of steps taken back to satisfy policy limit after binary search
     * colwerges to a single frequency point
     */
    LwU8                          numStepsBack;
    /*!
     * Used by @interface PwrPolicyDomGrpIsCapped to check if we are
     * actually capping
     */
    LwUFXP4_12                    capMultiplier;
     /*!
      * The un quantized frequency value to be used in the next evaluation cycle
      * to compare the newlimit value against.
      */
    PERF_DOMAIN_GROUP_LIMITS      domGrpLimitsUnQuant;
    /*!
     * @copydoc PWR_POLICY_WORKLOAD_COMBINED_1X_OBSERVED_METRICS
     */
    PWR_POLICY_WORKLOAD_COMBINED_1X_OBSERVED_METRICS obsMetrics;
    /*!
     * @copydoc PWR_POLICY_WORKLOAD_COMBINED_1X_ESTIMATED_METRICS
     */
    PWR_POLICY_WORKLOAD_COMBINED_1X_ESTIMATED_METRICS estMetrics;
    /*!
     * @copydoc LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_REGIMES_STATUS
     */
    LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_REGIMES_STATUS regimesStatus;
    /*!
     * Information of clock regimes to aid search.
     * @note - should not be indexed through
     * @ref LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_SEARCH_REGIME_ID
     */
    LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_SEARCH_REGIME
        searchRegimes[LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_SEARCH_REGIME_ID_MAX];
    /*!
     * Information to aid construction of @ref searchRegimes
     * @note - Can be indexed through
     * @ref LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_SEARCH_REGIME_ID
     */
    PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_DATA
        regimeData[LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_SEARCH_REGIME_ID_MAX];
    /*!
     * Total number of valid entries in @ref searchRegimes per evaluation
     * cycle
     */
    LwU8                          numRegimes;
    /*!
     * Mask of all the supported searchRegimes
     */
    BOARDOBJGRPMASK_E32           supportedRegimeMask;
    /*!
     * Stores the init information for REGIME(DUMMY_ROOT)
     */
    PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_DATA  rootRegimeData;
};

/* ------------------------- Defines --------------------------------------- */
/*!
 * Helper macro for the set of overlays required by WORKLOAD_COMBINED_1X when
 * @ref pwrPolicies3xCallbackBody() is called.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X)
#define PWR_POLICY_WORKLOAD_COMBINED_1X_EVALUATE_OVERLAYS                       \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfChangeSeqClient)                  \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfLimitVf)                          \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVoltApi)                           \
    OSTASK_OVL_DESC_DEFINE_LIB_LW64                                             \
    OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pmgrLibPwrPolicyClient)
#else
#define PWR_POLICY_WORKLOAD_COMBINED_1X_EVALUATE_OVERLAYS                       \
    OSTASK_OVL_DESC_ILWALID
#endif

/*!
 * Helper macro for the set of overlays required to call @ref
 * pwrPolicy3XFilter_WORKLOAD_COMBINED_1X().
 *
 * + perfLimitVf, LIB_LW64, and libPwrEquation used during observe metrics for single_1x
 *   perfLimitVf is added selectively near call sites so that  multirail filter
 *   path imem is not regressed
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X)
#define PWR_POLICY_WORKLOAD_COMBINED_1X_FILTER_OVERLAYS                         \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfChangeSeqClient)                  \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVoltApi)                           \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLpwr)                              \
    OSTASK_OVL_DESC_DEFINE_LIB_LW64                                             \
    OSTASK_OVL_DESC_DEFINE_PWR_POLICIES_EVALUATE_PWR_EQUATION_DYNAMIC           \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrLibPwrPolicyWorkloadShared)
#else
#define PWR_POLICY_WORKLOAD_COMBINED_1X_FILTER_OVERLAYS                         \
    OSTASK_OVL_DESC_ILWALID
#endif

/*!
 * Helper macro for the set of overlays required to call @ref
 * pwrPolicy3XFilter_WORKLOAD_COMBINED_1X().
 *
 * + perfLimitVf, LIB_LW64, and libPwrEquation used during observe metrics for single_1x
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X)
#define PWR_POLICY_WORKLOAD_COMBINED_1X_LOAD_OVERLAYS                           \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfChangeSeqClient)                  \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBoardObj)                          \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfLimitVf)                          \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVoltApi)                           \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLpwr)                              \
    OSTASK_OVL_DESC_DEFINE_LIB_LW64                                             \
    OSTASK_OVL_DESC_DEFINE_PWR_POLICIES_EVALUATE_PWR_EQUATION_DYNAMIC           \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrLibPwrPolicyWorkloadShared)
#else
#define PWR_POLICY_WORKLOAD_COMBINED_1X_LOAD_OVERLAYS                           \
    OSTASK_OVL_DESC_ILWALID
#endif

/*!
 * Helper macro for the set of overlays required to call @ref
 * pwrPolicyPerfCfPwrModelScale_WORKLOAD_COMBINED_1X().
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X)
#define PWR_POLICY_WORKLOAD_COMBINED_1X_SCALE_OVERLAYS                          \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfChangeSeqClient)                  \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfLimitVf)                          \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVoltApi)                           \
    OSTASK_OVL_DESC_DEFINE_LIB_LW64                                             \
    OSTASK_OVL_DESC_DEFINE_PWR_POLICIES_EVALUATE_PWR_EQUATION_DYNAMIC           \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libFxpBasic)                          \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libFxpExtended)
#else
#define PWR_POLICY_WORKLOAD_COMBINED_1X_SCALE_OVERLAYS                          \
    OSTASK_OVL_DESC_ILWALID
#endif

/*!
 * Mask of PG engine IDs that WORKLOAD_COMBINED_1X policy cares about.
 */
#define PWR_POLICY_WORKLOAD_COMBINED_1X_PG_ENGINE_ID_MASK                       \
    (                                                                           \
        BIT(RM_PMU_LPWR_CTRL_ID_GR_PG)                                         |\
        BIT(RM_PMU_LPWR_CTRL_ID_GR_RG)                                         |\
        BIT(RM_PMU_LPWR_CTRL_ID_MS_MSCG)                                        \
    )

/*!
 * Macro to check if engineId is part of
 * @ref PWR_POLICY_WORKLOAD_COMBINED_1X_PG_ENGINE_ID_MASK
 */
#define pwrPolicyWorkloadCombined1xPgEngineIdIsSupported(engineId)              \
    ((PWR_POLICY_WORKLOAD_COMBINED_1X_PG_ENGINE_ID_MASK & BIT(engineId)) != 0U)

/*!
 * @brief      Accessor for CWCS1X object @ singleIdx for a given CWCC1X
 *
 * @todo       Work out the header includes such that this can be made into a
 *             proper static inline function. Lwrrently PWR_POLICY_REL_GET
 *             chokes on a missing definition of Pmgr even if pmu_objpmgr.h is
 *             included before pwrpolicyrel.h.
 *
 * @param      pCombined1x  CWCC1X pointer
 * @param[in]  singleIdx    The single index being accessed
 * @param      ppSingle1x   Pointer to a CWCS1X pointer that will be modified
 * @param      status       status variable to be set in case of failure
 * @param      exitLabel    label to jump to in case of failure
 *
 * @return     The flcn status.
 */
#define PWR_POLICY_WORKLOAD_COMBINED_1X_GET_SINGLE_1X(pCombined1x, singleIdx, ppSingle1x, status, exitLabel)    \
    do                                                                                                          \
    {                                                                                                           \
        PWR_POLICY_RELATIONSHIP *_pPolicyRel;                                                                   \
        LwBoardObjIdx _numSingles;                                                                              \
                                                                                                                \
        PMU_ASSERT_TRUE_OR_GOTO((status),                                                                       \
            (((pCombined1x) != NULL) &&                                                                         \
             ((ppSingle1x) != NULL)),                                                                           \
            FLCN_ERR_ILWALID_ARGUMENT,                                                                          \
            exitLabel);                                                                                         \
                                                                                                                \
        _numSingles = (pCombined1x)->singleRelIdxLast -                                                         \
                      (pCombined1x)->singleRelIdxFirst + 1;                                                     \
                                                                                                                \
        PMU_ASSERT_TRUE_OR_GOTO((status),                                                                       \
            ((singleIdx) < _numSingles),                                                                        \
            FLCN_ERR_ILWALID_ARGUMENT,                                                                          \
            exitLabel);                                                                                         \
                                                                                                                \
        _pPolicyRel = PWR_POLICY_REL_GET((pCombined1x)->singleRelIdxFirst + (singleIdx));                       \
        PMU_ASSERT_TRUE_OR_GOTO((status),                                                                       \
            _pPolicyRel != NULL,                                                                                \
            FLCN_ERR_ILWALID_INDEX,                                                                             \
            exitLabel);                                                                                         \
                                                                                                                \
        *(ppSingle1x) = (PWR_POLICY_WORKLOAD_SINGLE_1X *)                                                       \
            PWR_POLICY_RELATIONSHIP_POLICY_GET(_pPolicyRel);                                                    \
        PMU_ASSERT_TRUE_OR_GOTO((status),                                                                       \
            *(ppSingle1x) != NULL,                                                                              \
            FLCN_ERR_ILWALID_INDEX,                                                                             \
            exitLabel);                                                                                         \
    } while (LW_FALSE)

/*!
 * @brief      Private macro that iterates over single_1x indexes in observed and
 *             estimated metrics starting at a startingIndex
 *
 * @note       Must be called in conjunction with
 *             PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_INDEX__PRIVATE_END
 *
 * @param      pCombined1x      CWCC1X pointer
 * @param      singleIdx        Loop iterator
 * @param      startingIndex    Index to start from
 */
#define PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_INDEX__PRIVATE_BEGIN(pCombined1x, singleIdx, startingIndex)  \
    do                                                                                                                  \
    {                                                                                                                   \
        CHECK_SCOPE_BEGIN(PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_INDEX__PRIVATE);                           \
        LwU8 _numSingles = (pCombined1x)->singleRelIdxLast - (pCombined1x)->singleRelIdxFirst + 1U;                     \
        for ((singleIdx) = (startingIndex); (singleIdx) < _numSingles; (singleIdx)++)                                   \
        {

/*!
 * Macro that must be called in conjunction with
 * PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_INDEX__PRIVATE_BEGIN.
 */
#define PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_INDEX__PRIVATE_END               \
        }                                                                                   \
        CHECK_SCOPE_END(PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_INDEX__PRIVATE); \
    } while (LW_FALSE)

/*!
 * @brief      Macro that iterates over single_1x indexes in observed and
 *             estimated metrics starting at a startingIndex
 *
 * @note       Must be called in conjunction with
 *             PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_INDEX_END
 *
 * @param      pCombined1x  CWCC1X pointer
 * @param      singleIdx    Loop iterator
 */
#define PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_INDEX_BEGIN(pCombined1x, singleIdx)              \
    do                                                                                                      \
    {                                                                                                       \
        CHECK_SCOPE_BEGIN(PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_INDEX);                        \
        PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_INDEX__PRIVATE_BEGIN(pCombined1x, singleIdx, 0U) \
        {

/*!
 * Macro that must be called in conjunction with
 * PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_INDEX_BEGIN.
 */
#define PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_INDEX_END                        \
        }                                                                                   \
        PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_INDEX__PRIVATE_END;              \
        CHECK_SCOPE_END(PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_INDEX); \
    } while (LW_FALSE)

/*!
 * @brief      Iterator for the CWCS1X references of a given CWCC1X
 *
 * @note       Must be called in conjunction with
 *             PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_END
 *
 * @param      pCombined1x  pointer to a CWCC1X
 * @param      pSingle1x    CWCS1X iterator
 * @param      singleIdx    Index into CWCC1X estimated and observed singles metrics.
 * @param      status       Status variable to be set in case of failure
 * @param      exitLabel    An exit label to jump to in case of failure
 */
#define PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_BEGIN(pCombined1x, pSingle1x, singleIdx, status, exitLabel)  \
    do                                                                                                                  \
    {                                                                                                                   \
        CHECK_SCOPE_BEGIN(PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X);                                          \
        PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_INDEX_BEGIN(pCombined1x, singleIdx)                          \
        {                                                                                                               \
            PWR_POLICY_WORKLOAD_COMBINED_1X_GET_SINGLE_1X(                                                              \
                pCombined1x,                                                                                            \
                singleIdx,                                                                                              \
                &pSingle1x,                                                                                             \
                status,                                                                                                 \
                exitLabel);

/*!
 * Macro that must be called in conjunction with
 * PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_BEGIN.
 */
#define PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_END                  \
        }                                                                       \
        PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_INDEX_END;           \
        CHECK_SCOPE_END(PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X);    \
    } while (LW_FALSE)

/*!
 * @brief      Iterator for only the secondary CWCS1X references of a given CWCC1X
 *
 * @note       Must be called in conjunction with
 *             PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SECONDARY_SINGLE_1X_END
 *
 * @param      pCombined1x  pointer to a CWCC1X
 * @param      pSingle1x    CWCS1X iterator
 * @param      singleIdx    Index into CWCC1X estimated and observed singles metrics.
 * @param      status       Status variable to be set in case of failure
 * @param      exitLabel    An exit label to jump to in case of failure
 */
#define PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SECONDARY_SINGLE_1X_BEGIN(pCombined1x, pSingle1x, singleIdx, status, exitLabel)    \
    do                                                                                                                              \
    {                                                                                                                               \
        CHECK_SCOPE_BEGIN(PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SECONDARY_SINGLE_1X);                                            \
        PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_INDEX__PRIVATE_BEGIN(pCombined1x, singleIdx , 1U)                        \
        {                                                                                                                           \
            PWR_POLICY_WORKLOAD_COMBINED_1X_GET_SINGLE_1X(                                                                          \
                pCombined1x,                                                                                                        \
                singleIdx,                                                                                                          \
                &pSingle1x,                                                                                                         \
                status,                                                                                                             \
                exitLabel);

/*!
 * Macro that must be called in conjunction with
 * PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SECONDARY_SINGLE_1X_BEGIN.
 */
#define PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SECONDARY_SINGLE_1X_END                \
        }                                                                               \
        PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_INDEX__PRIVATE_END;          \
        CHECK_SCOPE_END(PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SECONDARY_SINGLE_1X);  \
    } while (LW_FALSE)

/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */

/*!
 * PWR_POLICY interfaces
 */
BoardObjGrpIfaceModel10ObjSet         (pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_COMBINED_1X)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_COMBINED_1X");
BoardObjIfaceModel10GetStatus             (pwrPolicyIfaceModel10GetStatus_WORKLOAD_COMBINED_1X)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrPolicyIfaceModel10GetStatus_WORKLOAD_COMBINED_1X");

/*!
 * PWR_POLICY_3X interfaces
 */
PwrPolicy3XChannelMaskGet (pwrPolicy3XChannelMaskGet_WORKLOAD_COMBINED_1X)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicy3XChannelMaskGet_WORKLOAD_COMBINED_1X");
PwrPolicy3XFilter         (pwrPolicy3XFilter_WORKLOAD_COMBINED_1X)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicy3XFilter_WORKLOAD_COMBINED_1X");
PwrPolicyLoad             (pwrPolicyLoad_WORKLOAD_COMBINED_1X)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrPolicyLoad_WORKLOAD_COMBINED_1X");
PwrPolicy35PerfCfControllerMaskGet (pwrPolicy35PerfCfControllerMaskGet_WORKLOAD_COMBINED_1X)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicy35PerfCfControllerMaskGet_WORKLOAD_COMBINED_1X");

/*!
 * PWR_POLICY_DOMGRP interfaces
 */
PwrPolicyDomGrpEvaluate   (pwrPolicyDomGrpEvaluate_WORKLOAD_COMBINED_1X)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyDomGrpEvaluate_WORKLOAD_COMBINED_1X");
PwrPolicyDomGrpLimitsMaskSet (pwrPolicyDomGrpLimitsMaskSet_WORKLOAD_COMBINED_1X)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyDomGrpLimitsMaskSet_WORKLOAD_COMBINED_1X");
PwrPolicyDomGrpIsCapped (pwrPolicyDomGrpIsCapped_WORKLOAD_COMBINED_1X)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyDomGrpIsCapped_WORKLOAD_COMBINED_1X");
/*!
 * PWR_POLICY_PERF_CF_PWR_MODEL interfaces
 */
PwrPolicyPerfCfPwrModelObserveMetrics (pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_COMBINED_1X)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyPerfCfPwrModel", "pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_COMBINED_1X");
/*!
 * @deprecated  Use pwrPolicyPerfCfPwrModelScale_WORKLOAD_COMBINED_1X going
 *              forward, it better conforms to the PERF_CF_PWR_MODEL interface
 *              of the same name.
 */
PwrPolicyPerfCfPwrModelScaleMetrics   (pwrPolicyPerfCfPwrModelScaleMetrics_WORKLOAD_COMBINED_1X)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyPerfCfPwrModelScale", "pwrPolicyPerfCfPwrModelScaleMetrics_WORKLOAD_COMBINED_1X");
PwrPolicyPerfCfPwrModelScale          (pwrPolicyPerfCfPwrModelScale_WORKLOAD_COMBINED_1X)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyPerfCfPwrModelScale", "pwrPolicyPerfCfPwrModelScale_WORKLOAD_COMBINED_1X");

FLCN_STATUS pwrPolicyWorkloadCombined1xImportTgp1xWorkloadParameters(PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x,
                                                                     LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_OBSERVED_METRICS_WORKLOAD_COMBINED_1X *pObsMetrics,
                                                                     LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TGP_1X_WORKLOAD_PARAMETERS *pTgp1xWorkloadParams)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyPerfCfPwrModelScale", "pwrPolicyWorkloadCombined1xImportTgp1xWorkloadParameters");

/* ------------------------ Inline Functions -------------------------------- */
/*!
 * @brief      Export a copy of latest observed metrics
 *
 * @param      pPwrPolicyWorkloadCombined1x  CWCC1X pointer
 * @param      pObsMetrics                   Observed metrics to be copied into
 *
 * @return     FLCN_ERR_ILWALID_ARGUMENT in case of NULL inputs, otherwise FLCN_OK.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
pwrPolicyWorkloadCombined1xObservedMetricsExport
(
    PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x,
    LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_OBSERVED_METRICS_WORKLOAD_COMBINED_1X *pObsMetrics
)
{
    LwU8        i;
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pCombined1x != NULL) &&
         (pObsMetrics != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        pwrPolicyWorkloadCombined1xObservedMetricsExport_exit);

    pObsMetrics->observedVal = pCombined1x->obsMetrics.observedVal;

    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_INDEX_BEGIN(pCombined1x, i)
    {
        pObsMetrics->singles[i] = pCombined1x->obsMetrics.pSingles[i];
    }
    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_INDEX_END;

pwrPolicyWorkloadCombined1xObservedMetricsExport_exit:
    return status;
}

#endif // PWRPOLICY_WORKLOAD_COMBINED_1X_H
