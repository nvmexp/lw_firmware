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
 * @file    perf_cf_controller_dlppc_1x.c
 * @copydoc perf_cf_controller_dlppc_1x.h
 */

/* ----------------------- SDK Includes ---------------------------------- */
#include "ctrl_addenda/ctrl2080pmgr_inlines.h"

/* ----------------------- System Includes ---------------------------------- */
#include "pmusw.h"

/* ----------------------- Application Includes ----------------------------- */
#include "perf/cf/perf_cf.h"
#include "perf/cf/perf_cf_controller_dlppc_1x.h"
#include "pmu_objperf.h"
#include "pmu_objclk.h"

/* ------------------------ Macros and Defines ------------------------------ */
#define DLPPC_1X_METRICS_SCRATCH_SIZE  (LW2080_CTRL_NNE_NNE_DESC_INFERENCE_LOOPS_MAX + 1)

/* ------------------------ Static Function Prototypes ---------------------- */
static FLCN_STATUS s_perfCfControllerCopyCoreRail_DLPPC_1X(
        LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_CORE_RAIL *pCoreDest,
        LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_CORE_RAIL *pCoreSrc)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfControllerCopyCoreRail_DLPPC_1X");
static FLCN_STATUS s_perfCfControllerPwrPolicyRelSetSanityCheck_DLPPC_1X(
        const LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_SET *pRelSet)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfControllerPwrPolicyRelSetSanityCheck_DLPPC_1X");
static FLCN_STATUS s_perfCfControllerRailSanityCheck_DLPPC_1X(
        const LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_RAIL *pRail)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfControllerRailSanityCheck_DLPPC_1X");
static FLCN_STATUS s_perfCfControllerPwrLimitsCache_DLPPC_1X(
        PERF_CF_CONTROLLER_DLPPC_1X *pDlppc1x,
        const PWR_POLICIES_STATUS *pPwrPoliciesStatus)
    GCC_ATTRIB_SECTION("imem_perfCf", "s_perfCfControllerPwrLimitsCache_DLPPC_1X");
static FLCN_STATUS s_perfCfControllerPwrPolicyRelSetLimitsCache_DLPPC_1X(
        const LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_SET *pRelSet,
        const PWR_POLICIES_STATUS *pPwrPoliciesStatus,
        LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PWR_POLICY_RELATIONSHIP_SET_LIMITS *pLimits)
    GCC_ATTRIB_SECTION("imem_perfCf", "s_perfCfControllerPwrPolicyRelSetLimitsCache_DLPPC_1X");
static FLCN_STATUS s_perfCfControllerRailLimitsCache_DLPPC_1X(
        const LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_RAIL *pRail,
        const PWR_POLICIES_STATUS *pPwrPoliciesStatus,
        LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_RAIL_LIMITS *pLimits)
    GCC_ATTRIB_SECTION("imem_perfCf", "s_perfCfControllerRailLimitsCache_DLPPC_1X");
static FLCN_STATUS s_perfCfControllerEffLwrveRangeEstimate_DLPPC_1X(
        PERF_CF_CONTROLLER_DLPPC_1X *pDlppc1x, LwU32 dramclkkHz,
        PERF_LIMITS_VF_RANGE *pPrimaryclkRangekHz,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *pEstimatedMetrics,
        LwU8 *pNumEstimatedMetrics, LwBool *pMorePoints,
        PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_DLPPM_1X *pDlppm1xParams)
    GCC_ATTRIB_SECTION("imem_perfCf", "s_perfCfControllerEffLwrveRangeEstimate_DLPPC_1X");
static FLCN_STATUS s_perfCfControllerInitialMetricsInsert_DLPPC_1X(
        PERF_CF_CONTROLLER_DLPPC_1X *pDlppc1x,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X (*pInitialEstimates)[DLPPC_1X_METRICS_SCRATCH_SIZE],
        LwU8 *pNumInitialEstimates,
        LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_DRAMCLK *pDramclk,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_DRAMCLK_ESTIMATES *pDlppmInitialEstimates,
        LwU8 firstInsertPrimaryClkIdx,
        LwBool bRespectMin)
    GCC_ATTRIB_SECTION("imem_perfCf", "s_perfCfControllerInitialMetricsInsert_DLPPC_1X");
static FLCN_STATUS s_perfCfControllerPowerCeilingEstimate_DLPPC_1X(
        PERF_CF_CONTROLLER_DLPPC_1X *pDlppc1x,
        LwU32 dramclkkHz,
        LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_DRAMCLK *pDramclk,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X (*pMetrics)[DLPPC_1X_METRICS_SCRATCH_SIZE],
        LwU8 numInitialMetrics,
        LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH_TYPE *pSearchTypeProfiling)
    GCC_ATTRIB_SECTION("imem_perfCf", "s_perfCfControllerPowerCeilingEstimate_DLPPC_1X");
static FLCN_STATUS s_perfCfControllerPerfFloorEstimate_DLPPC_1X(PERF_CF_CONTROLLER_DLPPC_1X *pDlppc1x,
        LwU32 dramclkkHz, LwUFXP20_12 perfTarget,
        LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_DRAMCLK *pDramclk,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X (*pMetrics)[DLPPC_1X_METRICS_SCRATCH_SIZE],
        LwU8 numInitialMetrics,
        LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH_TYPE *pSearchTypeProfiling)
    GCC_ATTRIB_SECTION("imem_perfCf", "s_perfCfControllerPerfFloorEstimate_DLPPC_1X");
static FLCN_STATUS s_perfCfControllerCheckPwrConstraintsSatisfied_DLPPC_1X(
        PERF_CF_CONTROLLER_DLPPC_1X *pDlppc1x,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *pDlppm1xMetrics,
        LwBool *pBSatisfied)
    GCC_ATTRIB_SECTION("imem_perfCf", "s_perfCfControllerCheckPwrConstraintsSatisfied_DLPPC_1X");
static FLCN_STATUS s_perfCfControllerDlppc1xPwrPoliciesStatusInit(
        const PERF_CF_CONTROLLER_DLPPC_1X *pDlppc1x,
        PERF_CF_CONTROLLER_MULTIPLIER_DATA *pMultData)
    GCC_ATTRIB_SECTION("imem_perfCf", "s_perfCfControllerDlppc1xPwrPoliciesStatusInit");
static FLCN_STATUS s_perfCfControllerDlppc1xPwrPoliciesStatusRelSetRegister(
        PWR_POLICIES_STATUS *pStatus,
        const LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_SET *pRelSet)
    GCC_ATTRIB_SECTION("imem_perfCf", "s_perfCfControllerDlppc1xPwrPoliciesStatusRelSetRegister");
static FLCN_STATUS s_perfCfControllerCheckPwrPolRelSetSatisfied_DLPPC_1X(
        LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_SET *pRelSet,
        LW2080_CTRL_PMGR_PWR_TUPLE *pPwrTuple,
        const LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PWR_POLICY_RELATIONSHIP_SET_LIMITS *pLimits,
        LwBool *pBSatisfied)
    GCC_ATTRIB_SECTION("imem_perfCf", "s_perfCfControllerCheckPwrPolRelSetSatisfied_DLPPC_1X");
static FLCN_STATUS s_perfCfControllerDlppc1xPwrPoliciesStatusRailRegister(
        PWR_POLICIES_STATUS *pStatus,
        const LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_RAIL *pRail)
    GCC_ATTRIB_SECTION("imem_perfCf", "s_perfCfControllerDlppc1xPwrPoliciesStatusRailRegister");
static FLCN_STATUS s_perfCfControllerRailCheckPwrPolRelSetSatisfied_DLPPC_1X(
        LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_RAIL *pRail,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_RAIL *pRailMetrics,
        const LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_RAIL_LIMITS *pLimits,
        LwBool *pBSatisfied)
    GCC_ATTRIB_SECTION("imem_perfCf", "s_perfCfControllerRailCheckPwrPolRelSetSatisfied_DLPPC_1X");
static FLCN_STATUS s_perfCfControllerDlppc1xSetLimitRecommendations(
        PERF_CF_CONTROLLER_DLPPC_1X *pDlppc1x,
        LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_FREQ_TUPLE *pTupleBestEstimatedLimits,
        LwBool bMetricsObserved,
        LwBoardObjIdx pstateIdxBelowSearchMin)
    GCC_ATTRIB_SECTION("imem_perfCf", "s_perfCfControllerDlppc1xSetLimitRecommendations");
static LwBool s_perfCfControllerDlppc1xDramclkAproxMatch(
        PERF_CF_CONTROLLER_DLPPC_1X *pDlppc1x,
        LwU32 actualDramclk,
        LwU32 targetDramclk)
    GCC_ATTRIB_SECTION("imem_perfCf", "s_perfCfControllerDlppc1xDramclkAproxMatch");
static FLCN_STATUS s_perfCfControllerDlppc1xInInflectionZone(
        const PERF_CF_CONTROLLER_DLPPC_1X *pDlppc1x,
        const PWR_POLICIES_STATUS *pPwrPoliciesStatus,
        LwBoardObjIdx pstateIdxBelowSearchMin,
        LwBool *pbInInflectionZone)
    GCC_ATTRIB_SECTION("imem_perfCf", "s_perfCfControllerDlppc1xInInflectionZone");
static FLCN_STATUS s_perfCfControllerDlppc1xInInflectionZoneForRail(
        const LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_RAIL *pRail,
        const PWR_POLICIES_STATUS *pPwrPoliciesStatus,
        LwBoardObjIdx pstateIdxBelowSearchMin,
        LwBool *pbInInflectionZone)
    GCC_ATTRIB_SECTION("imem_perfCf", "s_perfCfControllerDlppc1xInInflectionZoneForRail");
static FLCN_STATUS s_perfCfControllerDlppc1xInInflectionZoneForRelSet(
        const LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_SET *pRelSet,
        const PWR_POLICIES_STATUS *pPwrPoliciesStatus,
        LwBoardObjIdx pstateIdxBelowSearchMin,
        LwBool *pbInInflectionZone)
    GCC_ATTRIB_SECTION("imem_perfCf", "s_perfCfControllerDlppc1xInInflectionZoneForRelSet");
static FLCN_STATUS s_perfCfControllerDlppc1xDramclkFreqInP0(
        const PERF_CF_CONTROLLER_DLPPC_1X *pDlppc1x,
        LwU32 dramclkkHz,
        LwBool *pbFreqInP0)
    GCC_ATTRIB_SECTION("imem_perfCf", "s_perfCfControllerDlppc1xDramclkFreqInP0");
static LwBool s_perfCfControllerDlppc1xIsPowerLessThan(
        const PERF_CF_CONTROLLER_DLPPC_1X *pDlppc1x,
        LwU8 dramclkIdx1,
        LwU8 dramclkIdx2)
    GCC_ATTRIB_SECTION("imem_perfCf", "s_perfCfControllerDlppc1xIsPowerLessThan");

static inline LwUFXP20_12
s_perfCfControllerDlppc1xDramclkPerfGet(
        const PERF_CF_CONTROLLER_DLPPC_1X *pDlppc1x, LwU8 dramclkIdx, LwU8 metricsIdx);
static inline LwU32
s_perfCfControllerDlppc1xDramclkPwrGet(
        const PERF_CF_CONTROLLER_DLPPC_1X *pDlppc1x, LwU8 dramclkIdx, LwU8 metricsIdx);
static inline LwUFXP20_12
s_perfCfControllerDlppc1xDramclkCompPerfGet(
        const PERF_CF_CONTROLLER_DLPPC_1X *pDlppc1x, LwU8 dramclkIdx);
static inline LwUFXP20_12
s_perfCfControllerDlppc1xDramclkCompPwrGet(
        const PERF_CF_CONTROLLER_DLPPC_1X *pDlppc1x, LwU8 dramclkIdx);
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
s_perfCfControllerDlppc1xFreqTupleSet(
        LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_FREQ_TUPLE *pFreqTuple,
        LwU8 clkDomIdx,
        LwU32 freqKhz);
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
s_perfCfControllerDlppc1xFreqTupleGet(
        LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_FREQ_TUPLE *pFreqTuple,
        LwU8  clkDomIdx,
        LwU32 *pFreqkHz);
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
s_perfCfControllerDlppc1xFreqTupleCopy(
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_FREQ_TUPLE *pFreqTupleTo,
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_FREQ_TUPLE *pFreqTupleFrom);
GCC_ATTRIB_ALWAYSINLINE()
static inline LwBool
s_perfCfControllerDlppc1xPerfApproxMatch(
    const PERF_CF_CONTROLLER_DLPPC_1X *pDlppc1x,
    LwU32 dramclkIdx1,
    LwU32 dramclkIdx2);
GCC_ATTRIB_ALWAYSINLINE()
static inline LwBool
s_perfCfControllerDlppc1xIsPerfGreater(
    const PERF_CF_CONTROLLER_DLPPC_1X *pDlppc1x,
    LwU32 dramclkIdx1,
    LwU32 dramclkIdx2);

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
static LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X metricsScratch[DLPPC_1X_METRICS_SCRATCH_SIZE]
    GCC_ATTRIB_SECTION("dmem_dlppc1xMetricsScratch", "metricsScratch");

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
perfCfControllerGrpIfaceModel10ObjSetImpl_DLPPC_1X
(
    BOARDOBJGRP_IFACE_MODEL_10       *pModel10,
    BOARDOBJ         **ppBoardObj,
    LwLength           size,
    RM_PMU_BOARDOBJ   *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    PERF_CF_CONTROLLER_DLPPC_1X          *pControllerDlppc1x;
    RM_PMU_PERF_CF_CONTROLLER_DLPPC_1X   *pDescController =
        (RM_PMU_PERF_CF_CONTROLLER_DLPPC_1X *)pBoardObjDesc;
    FLCN_STATUS                           status          = FLCN_OK;
    LwU8                                  railIdx;

    // Sanity checking.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pBObjGrp      != NULL) &&
         (ppBoardObj    != NULL) &&
         (pBoardObjDesc != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllerGrpIfaceModel10ObjSetImpl_DLPPC_1X_exit);

    //
    // Check that DLPPC_1X is only being constructed for the merged-rail case.
    // TODO: Remove once DLPPC_1X supports split-rail.
    //
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDescController->coreRail.numRails == 1),
        FLCN_ERR_NOT_SUPPORTED,
        perfCfControllerGrpIfaceModel10ObjSetImpl_DLPPC_1X_exit);

    // Call the super-class constructor.
    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfControllerGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc),
        perfCfControllerGrpIfaceModel10ObjSetImpl_DLPPC_1X_exit);
    pControllerDlppc1x = (PERF_CF_CONTROLLER_DLPPC_1X *)*ppBoardObj;

    // Initialize PMU-side state.
    pControllerDlppc1x->observedMetrics.super.type = LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_DLPPM_1X;

    // Copy over type data.
    pControllerDlppc1x->fbRail                  = pDescController->fbRail;
    pControllerDlppc1x->tgpPolRelSet            = pDescController->tgpPolRelSet;
    pControllerDlppc1x->dlppmIdx                = pDescController->dlppmIdx;
    pControllerDlppc1x->dramHysteresisCountInc  = pDescController->dramHysteresisCountInc;
    pControllerDlppc1x->dramHysteresisCountDec  = pDescController->dramHysteresisCountDec;
    pControllerDlppc1x->dramclkMatchTolerance   = pDescController->dramclkMatchTolerance;
    pControllerDlppc1x->bOptpSupported          = pDescController->bOptpSupported;
    pControllerDlppc1x->bLwdaSupported          = pDescController->bLwdaSupported;

    pControllerDlppc1x->approxPerfThreshold =
                            pDescController->approxPerfThreshold;
    pControllerDlppc1x->approxPowerThresholdPct =
                            pDescController->approxPowerThresholdPct;

    //
    // Range check the thresholds. Note that a negative percentage < -1 is
    // essentially meaningless, because this would create a switchover point
    // that is itself negative, and negative perfRatios and and power values are
    // nonsensical. Is unlikely that we'd want a percentage greater than +1
    // either, so range check both directions here.
    //
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pControllerDlppc1x->approxPerfThreshold <=
            LW_TYPES_S32_TO_SFXP_X_Y(20, 12, 1)) &&
        (pControllerDlppc1x->approxPerfThreshold >=
            LW_TYPES_S32_TO_SFXP_X_Y(20, 12, -1)),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllerGrpIfaceModel10ObjSetImpl_DLPPC_1X_exit);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pControllerDlppc1x->approxPowerThresholdPct <=
            LW_TYPES_S32_TO_SFXP_X_Y(20, 12, 1)) &&
        (pControllerDlppc1x->approxPowerThresholdPct >=
            LW_TYPES_S32_TO_SFXP_X_Y(20, 12, -1)),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllerGrpIfaceModel10ObjSetImpl_DLPPC_1X_exit);

    // 
    // Ensure that if the thresholds are not disabled that their signs are
    // opposite. This is because the signs bias the choice of DRAMCLK in
    // opposite ways, and so for the bias to be coherent, the signs must be
    // opposite.
    //
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pControllerDlppc1x->approxPerfThreshold == 0) ||
        (pControllerDlppc1x->approxPowerThresholdPct == 0) ||
        (DRF_VAL(
            _TYPES, _SFXP_INTEGER, _SIGN(20, 12),
            pControllerDlppc1x->approxPerfThreshold) !=
         DRF_VAL(
            _TYPES, _SFXP_INTEGER, _SIGN(20, 12),
            pControllerDlppc1x->approxPowerThresholdPct)),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllerGrpIfaceModel10ObjSetImpl_DLPPC_1X_exit);

    // initialize all FREQ_TUPLE structs to contain no limit requests
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_FREQ_TUPLE_INIT(&pControllerDlppc1x->tupleBestEstimatedLimits);
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_FREQ_TUPLE_INIT(&pControllerDlppc1x->tupleLastChosenLimits);
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_FREQ_TUPLE_INIT(&pControllerDlppc1x->tupleLastOperatingFrequencies);

    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerCopyCoreRail_DLPPC_1X(&(pControllerDlppc1x->coreRail),
                                                &(pDescController->coreRail)),
        perfCfControllerGrpIfaceModel10ObjSetImpl_DLPPC_1X_exit);

    // Sanity check the individual core rail structures.
    for (railIdx = 0U; railIdx < pControllerDlppc1x->coreRail.numRails; railIdx++)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfControllerRailSanityCheck_DLPPC_1X(
                &pControllerDlppc1x->coreRail.rails[railIdx]),
            perfCfControllerGrpIfaceModel10ObjSetImpl_DLPPC_1X_exit);
    }

    // Sanity check the whole-core structures.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerPwrPolicyRelSetSanityCheck_DLPPC_1X(
            &pControllerDlppc1x->coreRail.pwrInRelSet),
        perfCfControllerGrpIfaceModel10ObjSetImpl_DLPPC_1X_exit);

    // Sanity check the FB rail structures
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerRailSanityCheck_DLPPC_1X(
            &pControllerDlppc1x->fbRail),
        perfCfControllerGrpIfaceModel10ObjSetImpl_DLPPC_1X_exit);

    // Sanity check the TGP structure
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerPwrPolicyRelSetSanityCheck_DLPPC_1X(
            &pControllerDlppc1x->tgpPolRelSet),
        perfCfControllerGrpIfaceModel10ObjSetImpl_DLPPC_1X_exit);

perfCfControllerGrpIfaceModel10ObjSetImpl_DLPPC_1X_exit:
    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
perfCfControllerIfaceModel10GetStatus_DLPPC_1X
(
    BOARDOBJ_IFACE_MODEL_10          *pModel10,
    RM_PMU_BOARDOBJ   *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PERF_CF_CONTROLLER_DLPPC_1X_GET_STATUS   *pStatus =
            (RM_PMU_PERF_CF_CONTROLLER_DLPPC_1X_GET_STATUS *)pPayload;
    PERF_CF_CONTROLLER_DLPPC_1X                     *pControllerDlppc1x =
            (PERF_CF_CONTROLLER_DLPPC_1X *)pBoardObj;
    LwU32         dramclkIdx;
    FLCN_STATUS   status = FLCN_OK;
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING *pProfiling;

    // Sanity checking.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pBoardObj != NULL) &&
         (pPayload  != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllerIfaceModel10GetStatus_DLPPC_1X_exit);

    // Call the super-class implementation.
    PMU_ASSERT_OK_OR_GOTO(status,
                           perfCfControllerIfaceModel10GetStatus_SUPER(pModel10,
                                                           pPayload),
                           perfCfControllerIfaceModel10GetStatus_DLPPC_1X_exit);

    // Copy over type specific data.
    pStatus->observedMetrics               = pControllerDlppc1x->observedMetrics;
    pStatus->perfTarget                    = pControllerDlppc1x->perfTarget;
    pStatus->numDramclk                    = pControllerDlppc1x->numDramclk;
    pStatus->dramHysteresisLwrrCountDec    = pControllerDlppc1x->dramHysteresisLwrrCountDec;
    pStatus->dramHysteresisLwrrCountInc    = pControllerDlppc1x->dramHysteresisLwrrCountInc;

    pStatus->tupleLastOperatingFrequencies = pControllerDlppc1x->tupleLastOperatingFrequencies;
    pStatus->tupleLastChosenLimits         = pControllerDlppc1x->tupleLastChosenLimits;
    pStatus->tupleBestEstimatedLimits      = pControllerDlppc1x->tupleBestEstimatedLimits;

    for (dramclkIdx = 0; dramclkIdx < pControllerDlppc1x->numDramclk; dramclkIdx++)
    {
        pStatus->dramclk[dramclkIdx] = pControllerDlppc1x->dramclk[dramclkIdx];
    }

    pStatus->coreRailStatus = pControllerDlppc1x->coreRailStatus;
    pStatus->fbRailStatus = pControllerDlppc1x->fbRailStatus;
    pStatus->tgpLimits = pControllerDlppc1x->tgpLimits;

    pStatus->bInInflectionZone = pControllerDlppc1x->bInInflectionZone;

    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfControllerDlppc1xProfilingGet(pControllerDlppc1x, &pProfiling),
        perfCfControllerIfaceModel10GetStatus_DLPPC_1X_exit);
    if (pProfiling != NULL)
    {
        //
        // Note: this is lwrrently temporarily removed from the RM-PMU structure
        // because of some size issues. See the structure definition comments for
        // more details.
        //
        // pStatus->profiling = *pProfiling;
        //
    }

perfCfControllerIfaceModel10GetStatus_DLPPC_1X_exit:
    return status;
}

/*!
 * @copydoc PerfCfControllerExelwte
 */
FLCN_STATUS
perfCfControllerExelwte_DLPPC_1X
(
    PERF_CF_CONTROLLER *pController
)
{
    PERF_CF_CONTROLLER_DLPPC_1X   *pDlppc1x = (PERF_CF_CONTROLLER_DLPPC_1X *)pController;
    PERF_CF_PWR_MODEL             *pPwrModel;
    LwU8                           firstValidDramclkIdx;
    LwU8                           dramclkIdx, bestDramclkIdx = 0;
    FLCN_STATUS                    status   = FLCN_OK;
    VPSTATE_3X                    *pVpstate3x;
    LwBoardObjIdx                  pstateIdxBelowSearchMin =
        LW2080_CTRL_PERF_PSTATE_INDEX_ILWALID;
    LwU32                          dramclkSearchMinkHz = 0U;
    LwU32                          primaryClkSearchMinkHz = 0U;
    PWR_POLICIES_STATUS           *pPwrPoliciesStatus;
    PERF_CF_CONTROLLERS *pControllers =
        PERF_CF_GET_CONTROLLERS();
    LwBool                         bMetricsObserved = LW_FALSE;
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING *pProfiling = NULL;

    //
    // DLPPC_1X grows imem_perfCf sufficiently large that the PMU thrashes
    // during LPWR code paths.
    //
#ifndef ON_DEMAND_PAGING_BLK
    ct_assert(!PMUCF_FEATURE_ENABLED());
#endif

    // Sanity checking.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pControllers != NULL &&
         pController  != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllerExelwte_DLPPC_1X_exit);

    // Retrieve and reset the profiling structure
    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfControllerDlppc1xProfilingGet(pDlppc1x, &pProfiling),
        perfCfControllerExelwte_DLPPC_1X_exit);
    perfCfControllerDlppc1xProfilingReset(pProfiling);

    perfCfControllerDlppc1xProfileRegionBegin(
        pProfiling,
        LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_REGION_TOTAL);

    // initialize the request to give no limit request
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_FREQ_TUPLE_INIT(&pDlppc1x->tupleBestEstimatedLimits);

    // Retrieve the pPwrPoliciesStatus for later use
    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfControllerMultiplierDataPwrPoliciesStatusGet(
            pDlppc1x->super.pMultData, &pPwrPoliciesStatus),
        perfCfControllerExelwte_DLPPC_1X_exit);
    PMU_ASSERT_OK_OR_GOTO(status,
        pPwrPoliciesStatus != NULL ? FLCN_OK : FLCN_ERR_ILWALID_STATE,
        perfCfControllerExelwte_DLPPC_1X_exit);

    // Call the super-class implementation.
    status = perfCfControllerExelwte_SUPER(pController);
    if (status != FLCN_OK)
    {
        if (status == FLCN_ERR_STATE_RESET_NEEDED)
        {
            pDlppc1x->perfTarget = LW2080_CTRL_PERF_PERF_CF_PERF_TARGET_AS_FAST_AS_POSSIBLE;
            status               = FLCN_OK;
        }
        goto perfCfControllerExelwte_DLPPC_1X_exit;
    }

    //
    // TODO @achaudhry: deprecate the pDlppc1x->perfTarget field
    // and replace all instances with pwrModelCallerSpecifiedData.perfTarget
    //
    // optpPerfRatio is a 8.24 FXP. To have it fit into
    // pDlppc1x->perfTarget we have to colwert it into a x.12
    // FXP value. To accomplish, we shift the value right by 12
    // creating a 20.12. It is safe to this as we only want .12
    // precision and don't expect the integer bits to exceed a value
    // of 100 (defined by OPTP) which fits into
    //
    if (pControllers->optpPerfRatio !=
        LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_OPTP_PERF_RATIO_INACTIVE)
    {
        pDlppc1x->perfTarget = pControllers->optpPerfRatio >> 12;

        //
        // If OPTP is not supported, the controller should not be running when
        // there is a perf target, so check the flag.
        //
        PMU_ASSERT_TRUE_OR_GOTO(status,
            pDlppc1x->bOptpSupported,
            FLCN_ERR_ILWALID_STATE,
            perfCfControllerExelwte_DLPPC_1X_exit);
    }
    else
    {
        pDlppc1x->perfTarget = LW2080_CTRL_PERF_PERF_CF_PERF_TARGET_AS_FAST_AS_POSSIBLE;
    }

    // Copy over the perfTarget into the caller specfied sample data
    pDlppc1x->pwrModelCallerSpecifiedData.perfTarget =
        pDlppc1x->perfTarget;

    // Reset the number of DRAMCLKs we have before beginning exlwtion.
    pDlppc1x->numDramclk = 0U;

    // Compute the initial metrics.
    pPwrModel = BOARDOBJGRP_OBJ_GET(PERF_CF_PWR_MODEL, pDlppc1x->dlppmIdx);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pPwrModel != NULL),
        FLCN_ERR_ILWALID_STATE,
        perfCfControllerExelwte_DLPPC_1X_exit);

    perfCfControllerDlppc1xProfileRegionBegin(
        pProfiling,
        LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_REGION_OBSERVE_METRICS);

    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfPwrModelObserveMetrics(pPwrModel,
                                     &(pDlppc1x->sampleData),
                                     &(pDlppc1x->observedMetrics.super)),
        perfCfControllerExelwte_DLPPC_1X_exit);

    perfCfControllerDlppc1xProfileRegionEnd(
        pProfiling,
        LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_REGION_OBSERVE_METRICS);

    //
    // At this point, the metrics have been successfully observed (though they
    // may not be bValid)
    //
    bMetricsObserved = LW_TRUE;

    perfCfControllerDlppc1xProfileRegionBegin(
        pProfiling,
        LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_REGION_PWR_LIMITS_CACHE);

    // Cache the power limits into DLPPC_1X
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerPwrLimitsCache_DLPPC_1X(pDlppc1x, pPwrPoliciesStatus),
        perfCfControllerExelwte_DLPPC_1X_exit);

    perfCfControllerDlppc1xProfileRegionEnd(
        pProfiling,
        LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_REGION_PWR_LIMITS_CACHE);

    //
    // If the DLPPC_1X_SEARCH_MINIMUM VPSTATE is available, then save off the
    // minimum frequencies associated with it, and also check it against the
    // PWR_POLICY inflection points.
    //
    pVpstate3x = (VPSTATE_3X *)vpstateGetBySemanticIdx(
        RM_PMU_PERF_VPSTATE_IDX_DLPPC_1X_SEARCH_MINIMUM);
    if (pVpstate3x != NULL)
    {
        const LwBoardObjIdx vpstateIdx = BOARDOBJ_GET_GRP_IDX(
            &pVpstate3x->super.super);
        BOARDOBJGRPMASK_E32 vpstateClkDomainsMask;
        PERF_LIMITS_VF dramclkMilwf =
        {
            .idx = pDlppc1x->fbRail.clkDomIdx,
        };
        PERF_LIMITS_VF primaryClkMilwf =
        {
            .idx = pDlppc1x->coreRail.rails[0].clkDomIdx,
        };
        LwBoardObjIdx pstateIdxSearchMin;
        BOARDOBJGRPMASK_E32 pstatesMask;

        perfCfControllerDlppc1xProfileRegionBegin(
            pProfiling,
            LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_REGION_VPSTATE_TO_FREQ);

        //
        // Get the DRAMCLK frequency from the VPSTATE, falling back to the
        // PSTATE range if necessary.
        //
        PMU_ASSERT_OK_OR_GOTO(status,
            perfLimitsVpstateIdxToFreqkHz(
                vpstateIdx,
                LW_TRUE,
                LW_TRUE,
                &dramclkMilwf),
            perfCfControllerExelwte_DLPPC_1X_exit);
        dramclkSearchMinkHz = dramclkMilwf.value;

        //
        // Get the primary clock frequency from the VPSTATE, but allow this to
        // be optional if it is not specified in the VPSTATE itself.
        //
        PMU_ASSERT_OK_OR_GOTO(status,
            perfLimitsVpstateIdxToClkDomainsMask(
                vpstateIdx,&vpstateClkDomainsMask),
            perfCfControllerExelwte_DLPPC_1X_exit);
        if (boardObjGrpMaskBitGet(&vpstateClkDomainsMask, primaryClkMilwf.idx))
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                perfLimitsVpstateIdxToFreqkHz(
                    vpstateIdx,
                    LW_TRUE,
                    LW_FALSE,
                    &primaryClkMilwf),
                perfCfControllerExelwte_DLPPC_1X_exit);
            primaryClkSearchMinkHz = primaryClkMilwf.value;
        }

        perfCfControllerDlppc1xProfileRegionEnd(
            pProfiling,
            LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_REGION_VPSTATE_TO_FREQ);

        perfCfControllerDlppc1xProfileRegionBegin(
            pProfiling,
            LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_REGION_INFLECTION_ZONE);

        //
        // Check to see if the PWR_POLICY inflection points are being imposed
        // on a range over which DLPPC does not search. If they are, we'll let
        // the inflection points take back over.
        //
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pstateIdxSearchMin = PSTATE_GET_INDEX_BY_LEVEL(
                vpstate3xPstateIdxGet(pVpstate3x))) !=
                    LW2080_CTRL_PERF_PSTATE_INDEX_ILWALID,
            FLCN_ERR_ILWALID_STATE,
            perfCfControllerExelwte_DLPPC_1X_exit);

        //
        // For comparing against the inflection limits, we actually need to get
        // the next PSTATE index below DLPPC_1X's active search min, because we
        // want DLPPPC to have control over coming back into the search space.
        //
        // We do this with mask operations:
        //  pstateIdxBelowSearchMin =
        //      highest_bit(
        //          pstates & ~bits[pstateIdxSearchMin:LW2080_CTRL_BOARDOBJGRP_E32_MAX_OBJECTS - 1])
        //
        boardObjGrpMaskInit_E32(&pstatesMask);
        PMU_ASSERT_OK_OR_GOTO(status,
            boardObjGrpMaskBitRangeSet(
                &pstatesMask,
                pstateIdxSearchMin,
                LW2080_CTRL_BOARDOBJGRP_E32_MAX_OBJECTS - 1U),
            perfCfControllerExelwte_DLPPC_1X_exit);
        PMU_ASSERT_OK_OR_GOTO(status,
            boardObjGrpMaskIlw(&pstatesMask),
            perfCfControllerExelwte_DLPPC_1X_exit);
        PMU_ASSERT_OK_OR_GOTO(status,
            boardObjGrpMaskAnd(
                &pstatesMask, &pstatesMask, &PSTATES_GET()->super.objMask),
            perfCfControllerExelwte_DLPPC_1X_exit);
        pstateIdxBelowSearchMin = boardObjGrpMaskBitIdxHighest(&pstatesMask);

        //
        // If there are no PSTATEs below DLPPC_1X's active search min, then
        // simply use the min itself.
        //
        if (pstateIdxBelowSearchMin == LW2080_CTRL_BOARDOBJ_IDX_ILWALID)
        {
            pstateIdxBelowSearchMin = pstateIdxSearchMin;
        }

        perfCfControllerDlppc1xProfileRegionBegin(
            pProfiling,
            LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_REGION_INFLECTION_ZONE);

        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfControllerDlppc1xInInflectionZone(
                pDlppc1x,
                pPwrPoliciesStatus,
                pstateIdxBelowSearchMin,
                &pDlppc1x->bInInflectionZone),
            perfCfControllerExelwte_DLPPC_1X_exit);

        perfCfControllerDlppc1xProfileRegionEnd(
            pProfiling,
            LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_REGION_INFLECTION_ZONE);

        //
        // If we are in an inflection zone, then just exit early, because DLPPC
        // will hand control back over to the inflection points anyway.
        //
        if (pDlppc1x->bInInflectionZone)
        {
            goto perfCfControllerExelwte_DLPPC_1X_exit;
        }
    }
    else
    {
        //
        // If there is no minimum VPSTATE, then we want to disable inflection
        // across the entire PSTATE range, so get the index of the lowest PSTATE
        //
        pstateIdxBelowSearchMin = boardObjGrpMaskBitIdxLowest(
            &PSTATES_GET()->super.objMask);
    }

    //
    // If the observed metrics are not valid or return 0 numDramclks,
    // exit early as there are no frequencies  to search through
    //
    if ((!pDlppc1x->observedMetrics.super.bValid) ||
        (pDlppc1x->observedMetrics.numInitialDramclkEstimates == 0U))
    {
        goto perfCfControllerExelwte_DLPPC_1X_exit;
    }

    //
    // If we have any DRAMCLKs below the minimum, we need to completely skip
    // over all but the last one, which we will refer to as a fallback only if
    // necessary.
    //
    perfCfControllerDlppc1xProfileRegionBegin(
        pProfiling,
        LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_REGION_DRAMCLK_FILTER);
    if (pDlppc1x->observedMetrics.initialDramclkEstimates[0].estimatedMetrics[0].fbRail.freqkHz <
            dramclkSearchMinkHz)
    {
        LwU8 firstValidInitialIdx;
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_DRAMCLK_ESTIMATES *pLastIlwalidInitial;

         //
         // Skip ahead until we find the last initial estimates with a DRAMCLK
         // below our search floor
         //
        for (firstValidInitialIdx = 1;
             (firstValidInitialIdx < pDlppc1x->observedMetrics.numInitialDramclkEstimates) &&
             (pDlppc1x->observedMetrics.initialDramclkEstimates[firstValidInitialIdx].estimatedMetrics[0].fbRail.freqkHz <
                dramclkSearchMinkHz);
             firstValidInitialIdx++)
        {
        }

        //
        // If we couldn't find a DRAMCLK we consider valid, then DLPPM didn't
        // provide us any metrics we want to consider, so exit early and set the
        // recommendations as if DLPPM did not provide us any initial estimates.
        //
        if (firstValidInitialIdx ==
                pDlppc1x->observedMetrics.numInitialDramclkEstimates)
        {
            perfCfControllerDlppc1xProfileRegionEnd(
                pProfiling,
                LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_REGION_DRAMCLK_FILTER);
            goto perfCfControllerExelwte_DLPPC_1X_exit;
        }

        //
        // For the fallback DRAMCLK, set the primary clock min estimate, the
        // power ceiling, and the perf floor to the initial estimated metrics
        // with the highest clock, and assume that it is not saturated.
        //
        pLastIlwalidInitial =
            &pDlppc1x->observedMetrics.initialDramclkEstimates[firstValidInitialIdx - 1U];
        pDlppc1x->dramclk[0].primaryClkMinEstimated =
            pLastIlwalidInitial->estimatedMetrics[pLastIlwalidInitial->numEstimatedMetrics - 1U];
        pDlppc1x->dramclk[0].compMetricsIdx =
            LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_DRAMCLK_METRICS_IDX_POWER_CEILING;
        pDlppc1x->dramclk[0].dramclkMetrics[LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_DRAMCLK_METRICS_IDX_POWER_CEILING].metrics =
            pDlppc1x->dramclk[0].primaryClkMinEstimated;
        pDlppc1x->dramclk[0].dramclkMetrics[LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_DRAMCLK_METRICS_IDX_POWER_CEILING].bSaturated =
            LW_FALSE;
        pDlppc1x->dramclk[0].dramclkMetrics[LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_DRAMCLK_METRICS_IDX_PERF_FLOOR] =
            pDlppc1x->dramclk[0].dramclkMetrics[LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_DRAMCLK_METRICS_IDX_POWER_CEILING];

        //
        // We only want to include the last invalid DRAMCLK in our own set of
        // DRAMCLKs (and not any other invalid ones), so adjust the numDramclk
        // based on the last invalid index.
        //
        pDlppc1x->numDramclk =
            pDlppc1x->observedMetrics.numInitialDramclkEstimates -
                (firstValidInitialIdx - 1U);

        //
        // The first valid index in pDlppc1x->dramclk is always 1 in this case,
        // because we saved only one invalid DRAMCLK
        //
        firstValidDramclkIdx = 1U;
    }
    else
    {
        firstValidDramclkIdx = 0U;
        pDlppc1x->numDramclk =
            pDlppc1x->observedMetrics.numInitialDramclkEstimates;
    }
    perfCfControllerDlppc1xProfileRegionEnd(
        pProfiling,
        LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_REGION_DRAMCLK_FILTER);

    //
    // Loop over the valid initial set of DRAMCLK estimates observed by DLPPM
    // and find each's perf ceiling and power floor.
    //
    // Default the lowest valid DRAMCLK as the "best", and search each higher DRAMCLK for a
    // "better" operating point. A "better" operating point is one that achieves
    // better perf, or lower power at iso-perf, within all power contraints.
    //

    perfCfControllerDlppc1xProfileRegionBegin(
        pProfiling,
        LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_REGION_SEARCH);

    bestDramclkIdx      = firstValidDramclkIdx; // Default lowest valid DRAMCLK as "best", as required by algorithm.
    for (dramclkIdx = firstValidDramclkIdx;
         dramclkIdx < pDlppc1x->numDramclk;
         dramclkIdx++)
    {
        //
        // To get to the initial estimates for this dramclkIdx, we have to
        // adjust the index by the number of initial estimates skipped
        //
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_DRAMCLK_ESTIMATES *const pDlppmInitialEstimates =
            &pDlppc1x->observedMetrics.initialDramclkEstimates[
                dramclkIdx + (pDlppc1x->observedMetrics.numInitialDramclkEstimates - pDlppc1x->numDramclk)];
        LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH *pDramclkProfiling;
        LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH_TYPE *pPowerProfiling;
        LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH_TYPE *pPerfProfiling;
        LwU8 numMetrics;
        LwU8 firstValidPrimaryClkIdx;
        LwU8 firstInsertPrimaryClkIdx;

        PMU_ASSERT_OK_OR_GOTO(status,
            perfCfControllerDlppc1xProfilingDramclkSearchProfilingGet(
                pProfiling, dramclkIdx, &pDramclkProfiling),
            perfCfControllerExelwte_DLPPC_1X_exit);

        perfCfControllerDlppc1xProfileDramclkSearchBegin(
            pDramclkProfiling);

        //
        // Find the first estimation that has a primary clock frequency above
        // the minimum.
        //
        for (firstValidPrimaryClkIdx = 0U;
             (firstValidPrimaryClkIdx < pDlppmInitialEstimates->numEstimatedMetrics) &&
             (pDlppmInitialEstimates->estimatedMetrics[firstValidPrimaryClkIdx].coreRail.rails[0].freqkHz <
                primaryClkSearchMinkHz);
             firstValidPrimaryClkIdx++)
        {
        }

        // Ensure that we have at least one valid estimate
        PMU_ASSERT_OK_OR_GOTO(status,
            firstValidPrimaryClkIdx < pDlppmInitialEstimates->numEstimatedMetrics ?
                FLCN_OK : FLCN_ERR_ILWALID_STATE,
            perfCfControllerExelwte_DLPPC_1X_exit);

        //
        // If all of the initial estimates are above the minimum or the minimum
        // itself is part of the initial estimates, we don't have to estimate
        // it, but otherwise, we do have to estimate the minimum itself.
        //
        if ((firstValidPrimaryClkIdx != 0U) &&
            (pDlppmInitialEstimates->estimatedMetrics[firstValidPrimaryClkIdx].coreRail.rails[0].freqkHz !=
                primaryClkSearchMinkHz))
        {
            PERF_LIMITS_VF_RANGE primaryClkMinRange =
            {
                .idx = pDlppc1x->coreRail.rails[0].clkDomIdx,
                .values =
                {
                    [PERF_LIMITS_VF_RANGE_IDX_MIN] = primaryClkSearchMinkHz,
                    [PERF_LIMITS_VF_RANGE_IDX_MAX] = primaryClkSearchMinkHz,
                }
            };
            PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_DLPPM_1X dlppm1xParams =
            {
                .super =
                {
                    .type =
                        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_DLPPM_1X
                },
                .bounds =
                {
                    .pEndpointLo =
                        &pDlppmInitialEstimates->estimatedMetrics[firstValidPrimaryClkIdx - 1U],
                    .pEndpointHi =
                        &pDlppmInitialEstimates->estimatedMetrics[firstValidPrimaryClkIdx],
                    .bGuardRailsApply = LW_TRUE,
                },
                // .pProfiling initialized below,
            };
            // We have to initialize metricsScratch to the size of the buffer.
            LwU8 numEstimated = LW_ARRAY_ELEMENTS(metricsScratch);

            // Initialize the profiling parameter
            PMU_ASSERT_OK_OR_GOTO(status,
                perfCfControllerDlppc1xProfilingDramclkSearchPrimaryClkMinEstimatedProfilingGet(
                    pDramclkProfiling, &dlppm1xParams.pProfiling),
                perfCfControllerExelwte_DLPPC_1X_exit);

            PMU_ASSERT_OK_OR_GOTO(status,
                s_perfCfControllerEffLwrveRangeEstimate_DLPPC_1X(
                    pDlppc1x,
                    pDlppmInitialEstimates->estimatedMetrics[0].fbRail.freqkHz,
                    &primaryClkMinRange,
                    metricsScratch,
                    &numEstimated,
                    &(LwBool){LW_FALSE},
                    &dlppm1xParams),
                perfCfControllerExelwte_DLPPC_1X_exit);

            // Ensure that we didn't get some weird behavior
            PMU_ASSERT_OK_OR_GOTO(status,
                numEstimated == 1U ? FLCN_OK : FLCN_ERR_ILWALID_STATE,
                perfCfControllerExelwte_DLPPC_1X_exit);

            // Copy out the estimate at the min
            pDlppc1x->dramclk[dramclkIdx].primaryClkMinEstimated =
                metricsScratch[0];

            // Begin inserting from the firstValidPrimaryClkIdx
            firstInsertPrimaryClkIdx = firstValidPrimaryClkIdx;
        }
        else
        {
            pDlppc1x->dramclk[dramclkIdx].primaryClkMinEstimated =
                pDlppmInitialEstimates->estimatedMetrics[firstValidPrimaryClkIdx];

            //
            // In this case, we will insert the first valid metric from
            // primaryClkMinEstimated, not the initial estimates array, so we
            // want to make sure we skip it.
            //
            firstInsertPrimaryClkIdx = firstValidPrimaryClkIdx + 1U;
        }

        PMU_ASSERT_OK_OR_GOTO(status,
            perfCfControllerDlppc1xProfilingDramclkSearchPowerCeilingProfilingGet(
                pDramclkProfiling, &pPowerProfiling),
            perfCfControllerExelwte_DLPPC_1X_exit);
        perfCfControllerDlppc1xProfileDramclkSearchTypeBegin(
            pPowerProfiling);

        //
        // Create the initial estimates array for the power ceiling. If the
        // DRAMCLK is strictly above the DRAMCLK min, we only want to consider
        // metrics above the primary clock min. Otherwise, we allow ourselves
        // to fall below the primary clock min, for the lowest considered
        // DRAMCLK.
        //
        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfControllerInitialMetricsInsert_DLPPC_1X(
                pDlppc1x,
                &metricsScratch,
                &numMetrics,
                &pDlppc1x->dramclk[dramclkIdx],
                pDlppmInitialEstimates,
                firstInsertPrimaryClkIdx,
                pDlppmInitialEstimates->estimatedMetrics[0].fbRail.freqkHz >
                    dramclkSearchMinkHz),
            perfCfControllerExelwte_DLPPC_1X_exit);

        // Estimate the power ceiling.
        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfControllerPowerCeilingEstimate_DLPPC_1X(
                pDlppc1x,
                metricsScratch[0].fbRail.freqkHz,
                &(pDlppc1x->dramclk[dramclkIdx]),
                &metricsScratch,
                numMetrics,
                pPowerProfiling),
            perfCfControllerExelwte_DLPPC_1X_exit);

        perfCfControllerDlppc1xProfileDramclkSearchTypeEnd(pPowerProfiling);


        PMU_ASSERT_OK_OR_GOTO(status,
            perfCfControllerDlppc1xProfilingDramclkSearchPerfFloorProfilingGet(
                pDramclkProfiling, &pPerfProfiling),
            perfCfControllerExelwte_DLPPC_1X_exit);
        perfCfControllerDlppc1xProfileDramclkSearchTypeBegin(pPerfProfiling);

        //
        // Create the initial estimates array for the perf floor search. Note
        // that in this case, we always search only above the primary clock min.
        //
        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfControllerInitialMetricsInsert_DLPPC_1X(
                pDlppc1x,
                &metricsScratch,
                &numMetrics,
                &pDlppc1x->dramclk[dramclkIdx],
                pDlppmInitialEstimates,
                firstInsertPrimaryClkIdx,
                LW_TRUE),
            perfCfControllerExelwte_DLPPC_1X_exit);

        // Estimate the perf floor.
        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfControllerPerfFloorEstimate_DLPPC_1X(
                pDlppc1x,
                metricsScratch[0].fbRail.freqkHz,
                pDlppc1x->perfTarget,
                &(pDlppc1x->dramclk[dramclkIdx]),
                &metricsScratch,
                numMetrics,
                pPerfProfiling),
            perfCfControllerExelwte_DLPPC_1X_exit);

        perfCfControllerDlppc1xProfileDramclkSearchTypeEnd(pPerfProfiling);

        //
        // Set whether the perf floor or the power ceiling
        // should be used as the optimal operating point for this DRAMCLK.
        // This VF point will be compared against the optimal operating
        // point for other DRAMCLKs.
        //
        pDlppc1x->dramclk[dramclkIdx].compMetricsIdx =
            (s_perfCfControllerDlppc1xDramclkPerfGet(
                 pDlppc1x,
                 dramclkIdx,
                 LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_DRAMCLK_METRICS_IDX_PERF_FLOOR) <=
             s_perfCfControllerDlppc1xDramclkPerfGet(
                 pDlppc1x,
                 dramclkIdx,
                 LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_DRAMCLK_METRICS_IDX_POWER_CEILING)) ?
            LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_DRAMCLK_METRICS_IDX_PERF_FLOOR :
                LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_DRAMCLK_METRICS_IDX_POWER_CEILING;

        //
        // If the current dramclkIdx matches the bestDramclkIdx then there
        // is no reason to run through the comparison logic as the dramclk
        // has already been defined as the best.
        //
        if (dramclkIdx == bestDramclkIdx)
        {
            continue;
        }

        //
        // Track the 'best' DRAMCLK computed. The 'best' DRAMCLK is the one that
        // has the best perf at lowest power, without violating power contraints
        // for the rail (i.e. the power ceiling for the DRAMCLK being examinedis saturated).
        //
        if (!pDlppc1x->dramclk[dramclkIdx].dramclkMetrics[LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_DRAMCLK_METRICS_IDX_POWER_CEILING].bSaturated)
        {
            //
            // If we are given a valid PerfTarget, then we need to make sure that the current dramclk at dramclkIdx meets that perfTarget
            // If the dramclk at dramclkIdx does meet the perfTarget then ensure that choosing the dramclk would lead to a decrease in power
            // compared to our current bestDramclkIdx
            //
            if(pDlppc1x->perfTarget != LW2080_CTRL_PERF_PERF_CF_PERF_TARGET_AS_FAST_AS_POSSIBLE                 &&
                (((s_perfCfControllerDlppc1xDramclkCompPerfGet(pDlppc1x, dramclkIdx) >= pDlppc1x->perfTarget)   &&
                  s_perfCfControllerDlppc1xIsPowerLessThan(pDlppc1x, bestDramclkIdx, dramclkIdx))               ||
                ((s_perfCfControllerDlppc1xDramclkCompPerfGet(pDlppc1x, bestDramclkIdx) < pDlppc1x->perfTarget) &&
                 (s_perfCfControllerDlppc1xIsPerfGreater(pDlppc1x, bestDramclkIdx, dramclkIdx)))))
            {
                bestDramclkIdx = dramclkIdx;
            }
            //
            // If we are not given a perfTarget then do a strict perf comparison. If the dramclk at dramclkIdx offers better performance
            // then the current bestDramclkIdx or if the dramclk at dramclkIdx offer same perf at lower power then choose it as the
            // bestDramclk
            //

            //
            // TODO: @achaudhry
            // Assumption: current search is w.r.t ascending DRAMCLKs so bestDramclkIdx is guaranteed to be < lwrrDramclkIdx
            // if s_perfCfControllerDlppc1xPerfApproxMatch is TRUE then there are two cases:
            // 1. the power @bestDramclkIdx is well-below the current TGP limit while the power @lwrrDramclkIdx meets TGP limits
            //      -- This would occur if the bestDramclk was low (e.x. P5/P8) such that the maxGpcClk at the dramclk would never saturate power and same if
            //              at a higher dramclk (e.x. P3/P0) the VF point chooses the min gpcclk
            //      -- In this case we would want to make choose whichever dramclk has the lowest power since the power difference is "great enough" and
            //              we are seeing comparative perf
            //
            // 2. the power @bestDramclkIdx and @lwrrDramclkIdx are roughly the same but still different
            //      -- In our re-lwrsive search we are trying to find the power ceiling. So when we get to this point, it is likely the VF point
            //           for each dramclk is as close to the TGP limit as possible. In this case, the estimates for power won't be an exact match
            //           since the probability of having exact mW approaches 0%.
            //      -- In this case we would want the controller to find the power between dramclk ~=.
            //              -- Arch said to do a +-x% bounds around power for equality
            //      -- Since perf and power are both found ~=, arch made the recommendation that we take the polarity of approxPerfThreshold
            //              and use that to bias to either the upper or lower dramclk.
            //
            //      Notes: need to think through the perf target case above and how this could potentially need to be used up there.
            //
            //    Search algo would change for power to be something along the lines of: &&
            //                      (s_perfCfControllerDlppc1xIsPwrLessThan(pDlppc1x, bestDramclkIdx, dramclkIdx))  ||
            //                        (s_perfCfControllerDlppc1xIsPwrApproxEqual(pDlppc1x, bestDramclkIdx, dramclkIdx) &&
            //                          pDlppc1x->approxPerfThrehsold < 0 )
            //
            //    s_perfCfControllerDlppc1xIsPwrLessThan and s_perfCfControllerDlppc1xIsPwrApproxEqual would become fuzzy comparator functions where the threshold will be a +- % difference
            //     (i.e. have symetric thresholding on power)
            //     pDlppc1x->approxPerfThrehsold < 0 -- only choose dramclkIdx as the best if we are biasing towards a higher dramclk (only works because the search is done by
            //                                           ascending dramclk order)
            //
            else if ((pDlppc1x->perfTarget == LW2080_CTRL_PERF_PERF_CF_PERF_TARGET_AS_FAST_AS_POSSIBLE) &&
            ((s_perfCfControllerDlppc1xIsPerfGreater(pDlppc1x, bestDramclkIdx, dramclkIdx))             ||
             (s_perfCfControllerDlppc1xPerfApproxMatch(pDlppc1x, bestDramclkIdx, dramclkIdx)            &&
              s_perfCfControllerDlppc1xIsPowerLessThan(pDlppc1x, bestDramclkIdx, dramclkIdx))))
            {
                bestDramclkIdx = dramclkIdx;
            }
            else
            {
                // continue on, this is not an error case
            }
        }

        perfCfControllerDlppc1xProfileDramclkSearchEnd(
            pDramclkProfiling);
    }

    perfCfControllerDlppc1xProfileRegionEnd(
        pProfiling,
        LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_REGION_SEARCH);

    //
    // If we couldn't find any point above the lowest valid DRAMCLK that did not
    // have a saturated power ceiling, then we want to choose the lowest DRAMCLK
    // we have available, which may be the one below the actual search minimum.
    //
    if (pDlppc1x->dramclk[bestDramclkIdx].dramclkMetrics[LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_DRAMCLK_METRICS_IDX_POWER_CEILING].bSaturated)
    {
        bestDramclkIdx = 0U;
    }

    //
    // Set the limit recommendation to be the perf floor of the chosenDramclkIdx. This will
    // handle cases like OPTP where there is an explicit perf target. In cases with
    // no explicit perf target, it will request the maximal clock value and let the
    // 20ms PWR_POLICY limit arbitrate the actual clock value to run at depending on the current
    // GPU power budget, letting the GPU potentially 'boost' in cases where the power budget increases.
    //
    s_perfCfControllerDlppc1xFreqTupleSet(&pDlppc1x->tupleBestEstimatedLimits,
                LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_CLKDOM_DRAM,
                pDlppc1x->dramclk[bestDramclkIdx].dramclkMetrics[LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_DRAMCLK_METRICS_IDX_PERF_FLOOR]
                    .metrics.fbRail.freqkHz);

    s_perfCfControllerDlppc1xFreqTupleSet(&pDlppc1x->tupleBestEstimatedLimits,
                LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_CLKDOM_GPC,
                pDlppc1x->dramclk[bestDramclkIdx].dramclkMetrics[LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_DRAMCLK_METRICS_IDX_PERF_FLOOR]
                    .metrics.coreRail.rails[0].freqkHz);

perfCfControllerExelwte_DLPPC_1X_exit:
    //
    // If we succeed or we fail for any reason but the controller pointer being
    // NULL, we still want to set the limits.
    //
    if (pDlppc1x != NULL)
    {
        perfCfControllerDlppc1xProfileRegionBegin(
            pProfiling,
            LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_REGION_HYSTERESIS);

        PMU_ASSERT_OK_OR_CAPTURE_FIRST_ERROR(status,
            s_perfCfControllerDlppc1xSetLimitRecommendations(
                pDlppc1x,
                &pDlppc1x->tupleBestEstimatedLimits,
                bMetricsObserved,
                pstateIdxBelowSearchMin));

        perfCfControllerDlppc1xProfileRegionEnd(
            pProfiling,
            LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_REGION_HYSTERESIS);

        perfCfControllerDlppc1xProfileRegionEnd(
            pProfiling,
            LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_REGION_TOTAL);
    }
    return status;
}

/*!
 * @copydoc PerfCfControllerSetMultData
 */
FLCN_STATUS
perfCfControllerSetMultData_DLPPC_1X
(
    PERF_CF_CONTROLLER                 *pController,
    PERF_CF_CONTROLLER_MULTIPLIER_DATA *pMultData
)
{
    FLCN_STATUS status = FLCN_OK;
    const PERF_CF_CONTROLLER_DLPPC_1X *const pDlppc1x =
        (PERF_CF_CONTROLLER_DLPPC_1X *)pController;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pController == NULL) ||
         (pMultData   == NULL)) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
        perfCfControllerSetMultData_DLPPC_1X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerDlppc1xPwrPoliciesStatusInit(pDlppc1x, pMultData),
        perfCfControllerSetMultData_DLPPC_1X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfControllerMultiplierDataPwrChannelsStatusInit(pMultData),
        perfCfControllerSetMultData_DLPPC_1X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfControllerMultiplierDataPmSensorsStatusInit(pMultData),
        perfCfControllerSetMultData_DLPPC_1X_exit);

    //
    // Note: data for PWR_CHANNELs and PM_SENSORs is initialized in
    // perfCfControllerLoad -> perfCfPwrModelSampleDataInit
    //

perfCfControllerSetMultData_DLPPC_1X_exit:
    return status;
}

/*!
 * @copydoc PerfCfControllerLoad
 */
FLCN_STATUS
perfCfControllerLoad_DLPPC_1X
(
    PERF_CF_CONTROLLER *pController
)
{
    FLCN_STATUS                        status = FLCN_OK;
    PERF_CF_CONTROLLER_DLPPC_1X *const pControllerDlppc1x =
        (PERF_CF_CONTROLLER_DLPPC_1X *)pController;
    PERF_CF_PWR_MODEL_DLPPM_1X        *pDlppm1x;
    LwU8                               railIdx;

    // Sanity checking.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pControllerDlppc1x != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllerLoad_DLPPC_1X_exit);

    pDlppm1x = (PERF_CF_PWR_MODEL_DLPPM_1X *)BOARDOBJGRP_OBJ_GET(PERF_CF_PWR_MODEL,
                                                                pControllerDlppc1x->dlppmIdx);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDlppm1x != NULL),
        FLCN_ERR_ILWALID_INDEX,
        perfCfControllerLoad_DLPPC_1X_exit);

    // Sample the initial state of all relevant PERF_CF sensors and topologies.
    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfPwrModelSampleDataInit(
            &pDlppm1x->super,
            &pControllerDlppc1x->sampleData,
            &pController->pMultData->topologysStatus,
            perfCfControllerMultiplierDataPwrChannelsStatusGet(pController->pMultData),
            perfCfControllerMultiplierDataPmSensorsStatusGet(pController->pMultData),
            NULL,
            &pControllerDlppc1x->pwrModelCallerSpecifiedData),
        perfCfControllerLoad_DLPPC_1X_exit);

    // Initialize the metrics.
    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfPwrModelObservedMetricsInit(
            &(pDlppm1x->super),
            &(pControllerDlppc1x->observedMetrics.super)),
        perfCfControllerLoad_DLPPC_1X_exit);

    // Compute the initial metrics.
    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfPwrModelObserveMetrics(
            &pDlppm1x->super,
            &pControllerDlppc1x->sampleData,
            &pControllerDlppc1x->observedMetrics.super),
        perfCfControllerLoad_DLPPC_1X_exit);

    // Cache the limit inidicies.
    for (railIdx = 0; railIdx < pControllerDlppc1x->coreRail.numRails; railIdx++)
    {
        // TODO: For now we, only set the primary rail.
        if (railIdx == 0)
        {
            pControllerDlppc1x->coreRail.rails[railIdx].maxLimitIdx =
                perfCfControllersGetMaxLimitIdxFromClkDomIdx(PERF_CF_GET_CONTROLLERS(),
                                                             pControllerDlppc1x->coreRail.rails[railIdx].clkDomIdx);
        }
        else
        {
            pControllerDlppc1x->coreRail.rails[railIdx].maxLimitIdx =
                PERF_CF_CONTROLLERS_LIMIT_IDX_ILWALID;
        }
    }

    pControllerDlppc1x->fbRail.maxLimitIdx =
        perfCfControllersGetMaxLimitIdxFromClkDomIdx(PERF_CF_GET_CONTROLLERS(),
                                                     pControllerDlppc1x->fbRail.clkDomIdx);

    pControllerDlppc1x->dramHysteresisLwrrCountDec = 0;
    pControllerDlppc1x->dramHysteresisLwrrCountInc = 0;

perfCfControllerLoad_DLPPC_1X_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */

/*!
 * @brief Helper to copy over a LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_CORE_RAIL
 *        structure.
 *
 * @param[OUT]   pCoreDest   Core rail structure to copy to.
 * @param[IN]    pCoreSrc    Core rail structure to copy from.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pCoreDest or @ref pCoreSrc are NULL.
 * @return FLCN_OK                     If the copy was sucessful.
 */
static FLCN_STATUS
s_perfCfControllerCopyCoreRail_DLPPC_1X
(
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_CORE_RAIL *pCoreDest,
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_CORE_RAIL *pCoreSrc
)
{
    LwU8          railIdx;
    FLCN_STATUS   status = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pCoreDest == NULL) ||
         (pCoreSrc  == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        s_perfCfControllerCopyCoreRail_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        (pCoreSrc->numRails >
         LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_MAX_CORE_RAILS) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        s_perfCfControllerCopyCoreRail_exit);

    // Copy over fields.
    pCoreDest->numRails     = pCoreSrc->numRails;
    pCoreDest->pwrInRelSet  = pCoreSrc->pwrInRelSet;

    for (railIdx = 0; railIdx < pCoreDest->numRails; railIdx++)
    {
        pCoreDest->rails[railIdx] = pCoreSrc->rails[railIdx];
    }

s_perfCfControllerCopyCoreRail_exit:
    return status;
}

/*!
 * @brief   Sanity checks an
 *          @ref LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_RAIL to ensure
 *          that it is consistent with DLPPC_1X's assumptions.
 *
 * @param[in]   pRail   Rail to sanity check
 *
 * @return  @ref FLCN_OK    Success
 * @return  Others          Errors propagated from callees.
 */
static FLCN_STATUS
s_perfCfControllerRailSanityCheck_DLPPC_1X
(
    const LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_RAIL *pRail
)
{
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerPwrPolicyRelSetSanityCheck_DLPPC_1X(
            &pRail->pwrInRelSet),
        s_perfCfControllerRailSanityCheck_DLPPC_1X_exit);
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerPwrPolicyRelSetSanityCheck_DLPPC_1X(
            &pRail->pwrOutRelSet),
        s_perfCfControllerRailSanityCheck_DLPPC_1X_exit);

s_perfCfControllerRailSanityCheck_DLPPC_1X_exit:
    return status;
}

/*!
 * @brief   Sanity checks an @ref LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_SET.
 *
 * @details In particular, this fucntion checks if the policies specified by
 *          @ref LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_SET can fit into
 *          @ref LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PWR_POLICY_RELATIONSHIP_SET_LIMITS
 *
 * @param[in]   pRelSet @ref PWR_POLICY_RELATIONSHIP set to check
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  pRelSet is inconsistent with
 *                                          DLPPC_1X's assumptions.
 */
static FLCN_STATUS
s_perfCfControllerPwrPolicyRelSetSanityCheck_DLPPC_1X
(
    const LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_SET *pRelSet
)
{
    FLCN_STATUS status;

    //
    // Check that the number of PWR_POLICY_RELATIONSHIP objects spcified by the
    // set can fit in one of the limit structures. The relationship set normally
    // has inclusive bounds, except when both the start and end are set to
    // LW2080_CTRL_BOARDOBJ_IDX_ILWALID, in which case the set is empty. Allow
    // for the empty set, and then otherwise ensure that all of the specified
    // elements will fit.
    //
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pRelSet->policyRelStart == LW2080_CTRL_BOARDOBJ_IDX_ILWALID) ||
        (pRelSet->policyRelStart - pRelSet->policyRelEnd + 1U <
            LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PWR_POLICY_RELATIONSHIP_SET_LIMITS_MAX),
        FLCN_ERR_ILWALID_ARGUMENT,
        s_perfCfControllerPwrPolicyRelSetSanityCheck_DLPPC_1X_exit);

s_perfCfControllerPwrPolicyRelSetSanityCheck_DLPPC_1X_exit:
    return status;
}

/*!
 * @brief   Caches all @ref PWR_POLICIES_STATUS data in which DLPPC_1X is
 *          interested into itself.
 *
 * @param[in,out]   pDlppc1x            DLPPC_1X object pointer into which to
 *                                      cache
 * @param[in]       pPwrPoliciesStatus  @ref PWR_POLICIES_STATUS from which to
 *                                      cache
 *
 * @return  @ref FLCN_OK    Success
 * @return  Others          Errors propagated from callees.
 */
static FLCN_STATUS
s_perfCfControllerPwrLimitsCache_DLPPC_1X
(
    PERF_CF_CONTROLLER_DLPPC_1X *pDlppc1x,
    const PWR_POLICIES_STATUS *pPwrPoliciesStatus
)
{
    FLCN_STATUS status;
    LwU8 railIdx;

    // Cache the individual core rail limit structures.
    for (railIdx = 0U; railIdx < pDlppc1x->coreRail.numRails; railIdx++)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfControllerRailLimitsCache_DLPPC_1X(
                &pDlppc1x->coreRail.rails[railIdx],
                pPwrPoliciesStatus,
                &pDlppc1x->coreRailStatus.rails[railIdx].limits),
            s_perfCfControllerPwrLimitsCache_DLPPC_1X_exit);
    }

    // Cache the whole-core limit structures.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerPwrPolicyRelSetLimitsCache_DLPPC_1X(
            &pDlppc1x->coreRail.pwrInRelSet,
            pPwrPoliciesStatus,
            &pDlppc1x->coreRailStatus.pwrInLimits),
        s_perfCfControllerPwrLimitsCache_DLPPC_1X_exit);

    // Cache the FB rail limit structures
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerRailLimitsCache_DLPPC_1X(
            &pDlppc1x->fbRail,
            pPwrPoliciesStatus,
            &pDlppc1x->fbRailStatus.limits),
        s_perfCfControllerPwrLimitsCache_DLPPC_1X_exit);

    // Cache the TGP limit structure
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerPwrPolicyRelSetLimitsCache_DLPPC_1X(
            &pDlppc1x->tgpPolRelSet, pPwrPoliciesStatus, &pDlppc1x->tgpLimits),
        s_perfCfControllerPwrLimitsCache_DLPPC_1X_exit);

s_perfCfControllerPwrLimitsCache_DLPPC_1X_exit:
    return status;
}

/*!
 * @brief   Caches all @ref PWR_POLICIES_STATUS data for a given
 *          @ref PWR_POLICY_RELATIONSHIP set.
 *
 * @param[in]   pRelSet             Relationship set for which to cache
 * @param[in]   pPwrPoliciesStatus  @ref PWR_POLICIES_STATUS from which to cache
 * @param[out]  pLimits             Limits into which to cache
 *
 * @return  @ref FLCN_OK                Success
 * @return  @ref FLCN_ERR_ILWALID_STATE A @ref PWR_POLICY_RELATIONSHIP pointed
 *                                      at by pRelSet is invalid.
 */
static FLCN_STATUS
s_perfCfControllerPwrPolicyRelSetLimitsCache_DLPPC_1X
(
    const LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_SET *pRelSet,
    const PWR_POLICIES_STATUS *pPwrPoliciesStatus,
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PWR_POLICY_RELATIONSHIP_SET_LIMITS *pLimits
)
{
    FLCN_STATUS status;
    LwBoardObjIdx polRelIdx;

    LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_SET_FOR_EACH_BEGIN(pRelSet, polRelIdx)
    {
        const PWR_POLICY_RELATIONSHIP *const pPolRel =
            PWR_POLICY_REL_GET(polRelIdx);
        const RM_PMU_PMGR_PWR_POLICY_BOARDOBJ_GET_STATUS_UNION *pPolicyStatus;

        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pPolRel != NULL) && (pPolRel->pPolicy != NULL),
            FLCN_ERR_ILWALID_STATE,
            s_perfCfControllerPwrPolicyRelSetLimitsCache_DLPPC_1X_exit);

        pPolicyStatus = pwrPoliciesStatusPolicyStatusGetConst(
            pPwrPoliciesStatus,
            BOARDOBJ_GET_GRP_IDX(&pPolRel->pPolicy->super.super));

        PMU_ASSERT_OK_OR_GOTO(status,
            perfCfControllerDlppc1xPwrPolicyRelationshipSetLimitsLimitSet(
                pRelSet,
                pLimits,
                polRelIdx,
                LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_ARBITRATION_OUTPUT_GET(
                    &pPolicyStatus->pwrPolicy.limitArbLwrr)),
            s_perfCfControllerPwrPolicyRelSetLimitsCache_DLPPC_1X_exit);
    }
    LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_SET_FOR_EACH_END

s_perfCfControllerPwrPolicyRelSetLimitsCache_DLPPC_1X_exit:
    return status;
}

/*!
 * @brief   Caches all @ref PWR_POLICIES_STATUS data for a given
 *          @ref LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_RAIL.
 *
 * @param[in]   pRail               Rail for which to cache
 * @param[in]   pPwrPoliciesStatus  @ref PWR_POLICIES_STATUS from which to cache
 * @param[out]  pLimits             Limits into which to cache
 *
 * @return  @ref FLCN_OK    Success
 * @return  Others          Errors propagated from callees.
 */
static FLCN_STATUS
s_perfCfControllerRailLimitsCache_DLPPC_1X
(
    const LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_RAIL *pRail,
    const PWR_POLICIES_STATUS *pPwrPoliciesStatus,
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_RAIL_LIMITS *pLimits
)
{
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerPwrPolicyRelSetLimitsCache_DLPPC_1X(
            &pRail->pwrInRelSet, pPwrPoliciesStatus, &pLimits->pwrInLimits),
        s_perfCfControllerRailLimitsCache_DLPPC_1X_exit);
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerPwrPolicyRelSetLimitsCache_DLPPC_1X(
            &pRail->pwrOutRelSet, pPwrPoliciesStatus, &pLimits->pwrOutLimits),
        s_perfCfControllerRailLimitsCache_DLPPC_1X_exit);

s_perfCfControllerRailLimitsCache_DLPPC_1X_exit:
    return status;
}

/*!
 * @brief   Wrapper function around @ref perfCfPwrModelScalePrimaryClkRange
 *
 * @param[IN]  pDlppc1x               DLPPC_1X object pointer.
 * @param[IN]  dramclkkHz             DRAMCLK frequency to estimate the efficiency lwrve for.
 * @param[IN]  pPrimaryclkRangekHz     Range of primary clock frequncies to consider, inclusive, in kHz.
 * @param[OUT] pEstimatedMetrics      Array of efficiency lwrve metrics estimated at points.
 *                                    This array is assumed to have at least @ref LW2080_CTRL_NNE_NNE_DESC_INFERENCE_LOOPS_MAX
 *                                    elements.
 * @param[OUT] pNumEstimatedMetrics   Number of estimated metrics.
 * @param[OUT] pBMorePoints           Pointer to a Boolean indicating if there are VF points bounded by @ref *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pDlppc1x, @ref pEstimatedMetrics, or @ref pNumEstimatedMetrics are NULL.
 * @return FLCN_OK                     If the estimation was successful.
 * @return Others                      Errors propagated from callees.
 */
static FLCN_STATUS
s_perfCfControllerEffLwrveRangeEstimate_DLPPC_1X
(
    PERF_CF_CONTROLLER_DLPPC_1X                            *pDlppc1x,
    LwU32                                                   dramclkkHz,
    PERF_LIMITS_VF_RANGE                                   *pPrimaryclkRangekHz,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X    *pEstimatedMetrics,
    LwU8                                                   *pNumEstimatedMetrics,
    LwBool                                                 *pBMorePoints,
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_DLPPM_1X           *pDlppm1xParams
)
{
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS
                              *ppEstimatedMetrics[DLPPC_1X_METRICS_SCRATCH_SIZE];
    PERF_CF_PWR_MODEL         *pPwrModel;
    LwU8                       idx;
    FLCN_STATUS                status = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pDlppc1x             == NULL) ||
         (pPrimaryclkRangekHz   == NULL) ||
         (pEstimatedMetrics    == NULL) ||
         (pNumEstimatedMetrics == NULL) ||
         (pBMorePoints         == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        s_perfCfControllerEffLwrveRangeEstimate_DLPPC_1X_exit);

    // Evaluate the points.
    pPwrModel = BOARDOBJGRP_OBJ_GET(PERF_CF_PWR_MODEL, pDlppc1x->dlppmIdx);
    PMU_ASSERT_OK_OR_GOTO(status,
        (pPwrModel == NULL) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
        s_perfCfControllerEffLwrveRangeEstimate_DLPPC_1X_exit);

    for (idx = 0; idx < *pNumEstimatedMetrics; idx++)
    {
        pEstimatedMetrics[idx].super.type = LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_DLPPM_1X;
        ppEstimatedMetrics[idx] = &(pEstimatedMetrics[idx].super);
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfPwrModelScalePrimaryClkRange(pPwrModel,
                                           &(pDlppc1x->observedMetrics.super),
                                           dramclkkHz,
                                           pPrimaryclkRangekHz,
                                           ppEstimatedMetrics,
                                           pNumEstimatedMetrics,
                                           pBMorePoints,
                                           &pDlppm1xParams->super),
        s_perfCfControllerEffLwrveRangeEstimate_DLPPC_1X_exit);


s_perfCfControllerEffLwrveRangeEstimate_DLPPC_1X_exit:
    return status;
}

/*!
 * @brief   Inserts the appropriate intial metrics pointers into an array of
 *          pointers.
 *
 * @param[in]   pDlppc1x                    DLPPC_1X object pointer
 * @param[out]  pInitialEstimates           Pointer to Array of metrics to
 *                                          initialize
 * @param[out]  pNumInitialEstimates        Number of estimates inserted into
 *                                          pInitialEstimates
 * @param[in]   pDramclk                    DRAMCLK status structure
 * @param[in]   pDlppmInitialEstimates      Initial estimates provided by DLPPM
 * @param[in]   firstInsertPrimaryClkIdx    Index in
 *                                          pDlppmInitialEstimates->estimatedMetrics
 *                                          of the first estimate to be inserted
 *                                          if respecting the minimum.
 * @param[in]   bRespectMin                 Whether the primary clock minimum
 *                                          should be respected in this case.
 *
 * @return  @ref FLCN_OK    Success
 */
static FLCN_STATUS
s_perfCfControllerInitialMetricsInsert_DLPPC_1X
(
    PERF_CF_CONTROLLER_DLPPC_1X *pDlppc1x,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X (*pInitialEstimates)[DLPPC_1X_METRICS_SCRATCH_SIZE],
    LwU8 *pNumInitialEstimates,
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_DRAMCLK *pDramclk,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_DRAMCLK_ESTIMATES *pDlppmInitialEstimates,
    LwU8 firstInsertPrimaryClkIdx,
    LwBool bRespectMin
)
{
    FLCN_STATUS status;
    LwU8 metricsIdx;

    if (bRespectMin)
    {
        //
        // Ensure that the metrics can fit in the buffer. We copy
        // numEstimatedMetrics - firstInsertPrimaryClkIdx from the DLPPM
        // estimates and one more for the primaryClkMinEstimated
        //
        PMU_ASSERT_OK_OR_GOTO(status,
            (pDlppmInitialEstimates->numEstimatedMetrics - firstInsertPrimaryClkIdx) + 1U <=
                    LW_ARRAY_ELEMENTS(*pInitialEstimates) ?
                FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
            s_perfCfControllerInitialMetricsInsert_DLPPC_1X_exit);

        metricsIdx = 0U;
        (*pInitialEstimates)[metricsIdx++] =
            pDramclk->primaryClkMinEstimated;

        //
        // metricsIdx indexes into (*pInitialEstimates). When indexing into
        // pDlppmInitialEstimates, we offset metricsIdx - 1 from
        // firstInsertPrimaryClkIdx; the "- 1" is because the
        // primaryClkMinEstimated resides at (*pInitialEstimates)[0] but is not in
        // pDlppmInitialEstimates
        //
        for (;
             firstInsertPrimaryClkIdx + (metricsIdx - 1U) <
                pDlppmInitialEstimates->numEstimatedMetrics;
             metricsIdx++)
        {
            (*pInitialEstimates)[metricsIdx] =
                pDlppmInitialEstimates->estimatedMetrics[firstInsertPrimaryClkIdx + (metricsIdx - 1U)];
        }
    }
    else
    {
        // Ensure that the metrics can fit in the buffer.
        PMU_ASSERT_OK_OR_GOTO(status,
            pDlppmInitialEstimates->numEstimatedMetrics <=
                    LW_ARRAY_ELEMENTS(*pInitialEstimates) ?
                FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
            s_perfCfControllerInitialMetricsInsert_DLPPC_1X_exit);

        //
        // If not respecting the min, then just directly copy over all of the
        // DLPPM initial estimates
        //
        for (metricsIdx = 0U;
             metricsIdx <
                pDlppmInitialEstimates->numEstimatedMetrics;
             metricsIdx++)
        {
            (*pInitialEstimates)[metricsIdx] =
                pDlppmInitialEstimates->estimatedMetrics[metricsIdx];
        }
    }

    *pNumInitialEstimates = metricsIdx;

s_perfCfControllerInitialMetricsInsert_DLPPC_1X_exit:
    return status;
}

/*!
 * @brief Helper to estimate the power ceiling for a DRAMCLK.
 *
 * @param[in]       pDlppc1x            DLPPC_1X controller.
 * @param[in]       dramclkkHz          Frequency of the DRAMCLK, in kHz.
 * @param[in,out]   pDramclk            DLPPC_1X DRAMCLK structure.
 * @param[in]       pMetrics            Pointer to array both containing the
 *                                      initial set of metrics from which to
 *                                      search and to use as a scratch buffer
 *                                      while searching.
 * @param[in]       numInitialMetrics   Initial number of valid metrics in
 *                                      pMetrics
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pDlppc1x or @ref pDramclk are NULL.
 * @return FLCN_ERR_ILWALID_STATE      If there was no PWR_MODEL object associated
 *                                     with PERF_CF_CONTROLLER_DLPPC_1X::dlppmIdx.
 * @return FLCN_OK                     A power ceiling was successfully found.
 */
static FLCN_STATUS
s_perfCfControllerPowerCeilingEstimate_DLPPC_1X
(
    PERF_CF_CONTROLLER_DLPPC_1X                                                    *pDlppc1x,
    LwU32                                                                           dramclkkHz,
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_DRAMCLK                    *pDramclk,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X                           (*pMetrics)[DLPPC_1X_METRICS_SCRATCH_SIZE],
    LwU8                                                                            numInitialMetrics,
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH_TYPE     *pSearchTypeProfiling
)
{
    PERF_LIMITS_VF_RANGE   primaryclkRangekHz;
    LwU8                   idx;
    LwU8                   numMetrics  = numInitialMetrics;
    LwBool                 bSatisfied;
    LwBool                 bMorePoints = LW_TRUE;
    FLCN_STATUS            status      = FLCN_OK;
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH_TYPE_ITERATION *pIterationProfiling;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pDlppc1x == NULL) ||
         (pDramclk == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        s_perfCfControllerPowerCeilingEstimate_DLPPC_1X_exit);

    //
    // Iteratively drill down into the section of the VF lwrve above
    // the highest point that satisfies all power policies, until
    // we cannot drill down any more.
    //
    while (LW_TRUE)
    {
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X upperBound;
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_SCALE_PROFILING *pScaleProfiling;

        PMU_ASSERT_OK_OR_GOTO(status,
            perfCfControllerDlppc1xProfilingDramclkSearchTypeIterationGetNext(
                pSearchTypeProfiling, &pIterationProfiling),
            s_perfCfControllerPowerCeilingEstimate_DLPPC_1X_exit);

        perfCfControllerDlppc1xProfileDramclkSearchTypeIterationBegin(
            pIterationProfiling);

        //
        // Iterate through the set of metrics in descending order to find
        // the highest primary clock frequency with corresponding power
        // that satisfies all power policies.
        //
        idx = numMetrics;
        bSatisfied = LW_FALSE;
        do
        {
            idx--;
            PMU_ASSERT_OK_OR_GOTO(status,
                s_perfCfControllerCheckPwrConstraintsSatisfied_DLPPC_1X(
                    pDlppc1x,
                    &((*pMetrics)[idx]),
                    &bSatisfied),
                s_perfCfControllerPowerCeilingEstimate_DLPPC_1X_exit);
        } while (!bSatisfied && (idx > 0U));

        //
        // Exit on any of the following conditions:
        //  1.) We were unable to find any point that satisfies all power
        //      policies. In this case, we use the lowest quantized primary
        //      clock as the power ceiling.
        //  2.) There are no more points to search, so the current point is
        //      optimal.
        //  3.) We found the highest current point satisfied the power policies.
        //      This means we can't go any higher, so choose this point and exit
        //      the loop.
        //
        if (!bSatisfied ||
            !bMorePoints ||
            (idx == numMetrics - 1U))
        {
            break;
        }

        //
        // Estimate the efficiency lwrve between the highest primary
        // clock that satisfies all power policies, and the next highest
        // primary clock.
        //
        // While doing this, ensure the max never goes below the min.
        //
        primaryclkRangekHz.values[PERF_LIMITS_VF_RANGE_IDX_MIN] =
            (*pMetrics)[idx].coreRail.rails[0].freqkHz + 1000;
        primaryclkRangekHz.values[PERF_LIMITS_VF_RANGE_IDX_MAX] =
            LW_MAX((*pMetrics)[idx + 1].coreRail.rails[0].freqkHz - 1000,
                   primaryclkRangekHz.values[PERF_LIMITS_VF_RANGE_IDX_MIN]);


        (*pMetrics)[0] = (*pMetrics)[idx];
        upperBound = (*pMetrics)[idx + 1];
        // Subtract one for the extra scratch metrics at index 0
        numMetrics = LW_ARRAY_ELEMENTS((*pMetrics)) - 1U;

        PMU_ASSERT_OK_OR_GOTO(status,
            perfCfControllerDlppc1xProfilingDramclkSearchTypeIterationScaleProfilingGet(
                pIterationProfiling, &pScaleProfiling),
            s_perfCfControllerPowerCeilingEstimate_DLPPC_1X_exit);

        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfControllerEffLwrveRangeEstimate_DLPPC_1X(
                pDlppc1x,
                dramclkkHz,
                &primaryclkRangekHz,
                &((*pMetrics)[1]),
                &numMetrics,
                &bMorePoints,
                &(PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_DLPPM_1X)
                {
                    .super =
                    {
                        .type = LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_DLPPM_1X,
                    },
                    .bounds =
                    {
                        .pEndpointLo = &(*pMetrics)[0],
                        .pEndpointHi = &upperBound,
                        .bGuardRailsApply = LW_TRUE,
                    },
                    .pProfiling = pScaleProfiling,
                }),
            s_perfCfControllerPowerCeilingEstimate_DLPPC_1X_exit);

        // Add one for our additional metrics saved from previous iteration.
        numMetrics++;

        perfCfControllerDlppc1xProfileDramclkSearchTypeIterationEnd(
            pIterationProfiling);
    }

    // Need to end the profiling for the last iteration still.
    perfCfControllerDlppc1xProfileDramclkSearchTypeIterationEnd(
        pIterationProfiling);

    PMU_ASSERT_OK_OR_GOTO(status,
         idx >= DLPPC_1X_METRICS_SCRATCH_SIZE ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
        s_perfCfControllerPowerCeilingEstimate_DLPPC_1X_exit);

    pDramclk->dramclkMetrics[LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_DRAMCLK_METRICS_IDX_POWER_CEILING]
        .bSaturated = !bSatisfied;

    // Copy the power ceiling metrics to the DRAM structure.
    pDramclk->dramclkMetrics[LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_DRAMCLK_METRICS_IDX_POWER_CEILING]
        .metrics = (*pMetrics)[idx];

s_perfCfControllerPowerCeilingEstimate_DLPPC_1X_exit:
    return status;
}

/*!
 * @brief Helper to estimate the perf floor for a DRAMCLK.
 *
 * @param[in]       pDlppc1x            DLPPC_1X controller.
 * @param[in]       dramclkkHz          Frequency of the DRAMCLK, in kHz.
 * @param[in]       perfTarget          Performance target to satisfy
 * @param[in,out]   pDramclk            DLPPC_1X DRAMCLK structure.
 * @param[in]       pMetrics            Pointer to array both containing the
 *                                      initial set of metrics from which to
 *                                      search and to use as a scratch buffer
 *                                      while searching.
 * @param[in]       numInitialMetrics   Initial number of valid metrics in
 *                                      pMetrics
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pDlppc1x or @ref pDramclk are NULL.
 * @return FLCN_ERR_ILWALID_STATE      If there was no PWR_MODEL object associated
 *                                     with PERF_CF_CONTROLLER_DLPPC_1X::dlppmIdx.
 * @return FLCN_OK                     A perf floor was successfully found.
 */
static FLCN_STATUS
s_perfCfControllerPerfFloorEstimate_DLPPC_1X
(
    PERF_CF_CONTROLLER_DLPPC_1X                                                    *pDlppc1x,
    LwU32                                                                           dramclkkHz,
    LwUFXP20_12                                                                     perfTarget,
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_DRAMCLK                    *pDramclk,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X                           (*pMetrics)[DLPPC_1X_METRICS_SCRATCH_SIZE],
    LwU8                                                                            numInitialMetrics,
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH_TYPE     *pSearchTypeProfiling
)
{
    PERF_LIMITS_VF_RANGE       primaryclkRangekHz;
    PERF_LIMITS_PSTATE_RANGE   pstateRange;
    VOLT_RAIL                 *pRail0;
    PERF_LIMITS_VF             vfDomain;
    PERF_LIMITS_VF             vfRail;
    BOARDOBJGRPMASK_E32       *pClkDomProgMask;
    LwU8                       idx;
    LwU8                       numMetrics  = numInitialMetrics;
    LwBool                     bSatisfied;
    LwBool                     bMorePoints = LW_TRUE;
    FLCN_STATUS                status      = FLCN_OK;
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PROFILING_DRAMCLK_SEARCH_TYPE_ITERATION *pIterationProfiling;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pDlppc1x == NULL) ||
         (pDramclk == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        s_perfCfControllerPerfFloorEstimate_DLPPC_1X_exit);

    //
    // Compute the minimim voltage needed to support @ref dramclkkHz, if
    // DRAMCLK resides on core rail [0].
    //
    pRail0 = VOLT_RAIL_GET(pDlppc1x->coreRail.rails[0].voltRailIdx);
    PMU_ASSERT_OK_OR_GOTO(status,
        (pRail0 == NULL) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
        s_perfCfControllerPerfFloorEstimate_DLPPC_1X_exit);

    pClkDomProgMask = voltRailGetClkDomainsProgMask(pRail0);
    PMU_ASSERT_OK_OR_GOTO(status,
        (pClkDomProgMask == NULL) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
        s_perfCfControllerPerfFloorEstimate_DLPPC_1X_exit);

    vfDomain.idx   = BOARDOBJ_GRP_IDX_TO_8BIT(pDlppc1x->fbRail.clkDomIdx);
    vfDomain.value = dramclkkHz;
    vfRail.idx     = pDlppc1x->coreRail.rails[0].voltRailIdx;
    if (boardObjGrpMaskBitGet(pClkDomProgMask, pDlppc1x->fbRail.clkDomIdx))
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            perfLimitsFreqkHzToVoltageuV(&vfDomain, &vfRail, LW_TRUE),
            s_perfCfControllerPerfFloorEstimate_DLPPC_1X_exit);

        PMU_ASSERT_OK_OR_GOTO(status,
            (vfRail.value == 0) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
            s_perfCfControllerPerfFloorEstimate_DLPPC_1X_exit);

        PMU_ASSERT_OK_OR_GOTO(status,
            perfLimitsFreqkHzToPstateIdxRange(&vfDomain, &pstateRange),
            s_perfCfControllerPerfFloorEstimate_DLPPC_1X_exit);
    }
    else
    {
        vfRail.value = 0;

        pstateRange.values[LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MIN] =
            boardObjGrpMaskBitIdxLowest(&(PSTATES_GET()->super.objMask));
        pstateRange.values[LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MAX] =
            boardObjGrpMaskBitIdxHighest(&(PSTATES_GET()->super.objMask));
    }

    //
    // Compute the maximum achievable frequency corresponding to the current
    // minimum voltage of core rail [0], as determined by the other indpendent clock
    // domains residing on core rail [0].
    //
    vfDomain.idx = pDlppc1x->coreRail.rails[0].clkDomIdx;
    vfRail.value = LW_MAX(vfRail.value,
                          LW_MAX(pDlppc1x->observedMetrics.coreRail.rails[0].maxIndependentClkDomVoltMinuV,
                                 pDlppc1x->observedMetrics.coreRail.rails[0].vminLimituV));
    PMU_ASSERT_OK_OR_GOTO(status,
        perfLimitsVoltageuVToFreqkHz(&vfRail, &pstateRange, &vfDomain, LW_TRUE, LW_TRUE),
        s_perfCfControllerPerfFloorEstimate_DLPPC_1X_exit);
    PMU_ASSERT_OK_OR_GOTO(status,
        (vfDomain.value == 0) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
        s_perfCfControllerPerfFloorEstimate_DLPPC_1X_exit);

    //
    // Iteratively drill down into the section of the VF lwrve below
    // the lowest point that satisfies the perf floor, until
    // we cannot drill down any more.
    //
    while (LW_TRUE)
    {
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X lowerBound;
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_SCALE_PROFILING *pScaleProfiling;

        PMU_ASSERT_OK_OR_GOTO(status,
            perfCfControllerDlppc1xProfilingDramclkSearchTypeIterationGetNext(
                pSearchTypeProfiling, &pIterationProfiling),
            s_perfCfControllerPerfFloorEstimate_DLPPC_1X_exit);

        perfCfControllerDlppc1xProfileDramclkSearchTypeIterationBegin(
            pIterationProfiling);

        //
        // Iterate through the set of metrics in ascending order to find
        // the lowest primary clock frequency with corresponding perf
        // that satisfies the perf floor and the primary clock frequency
        // corresponding to the minimum voltage for the rail.
        //
        // Note: we initialize idx to LW_U8_MAX, and the first step of the loop
        // increments idx, so the first examined idx is 0. This scheme allows us
        // to exit the loop when idx == numMetrics - 1.
        //
        idx = LW_U8_MAX;
        bSatisfied = LW_FALSE;
        do
        {
            idx++;
            bSatisfied =
                ((*pMetrics)[idx].perfMetrics.perfRatio >= perfTarget) &&
                ((*pMetrics)[idx].coreRail.rails[0].freqkHz >= vfDomain.value);
        } while (!bSatisfied && (idx < numMetrics - 1U));

        //
        // Exit on any of the following conditions:
        //  1.) We were unable to find any point that satisfies the perf floor.
        //      In this case, we use the highest quantized primary clock as the
        //      perf floor.
        //  2.) There are no more points to search, so the current point is
        //      optimal.
        //  3.) We found the lowest current point satisfied the perf floor. This
        //      means we can't go any lower, so choose this point and exit the
        //      loop.
        //
        if (!bSatisfied ||
            !bMorePoints ||
            (idx == 0U))
        {
            break;
        }

        //
        // Estimate the efficiency lwrve between the lowest primary
        // clock that satisfies the perf floor, and the next lowest
        // primary clock.
        //
        // While doing this, ensure the min never goes above the max.
        //
        primaryclkRangekHz.values[PERF_LIMITS_VF_RANGE_IDX_MAX] =
            (*pMetrics)[idx].coreRail.rails[0].freqkHz - 1000;
        primaryclkRangekHz.values[PERF_LIMITS_VF_RANGE_IDX_MIN] =
            LW_MIN((*pMetrics)[idx - 1].coreRail.rails[0].freqkHz + 1000,
                   primaryclkRangekHz.values[PERF_LIMITS_VF_RANGE_IDX_MAX]);


        (*pMetrics)[LW_ARRAY_ELEMENTS(*pMetrics) - 1] = (*pMetrics)[idx];
        lowerBound = (*pMetrics)[idx - 1];
        // Subtract one for the extra scratch metrics at index DLPPC_1X_METRICS_SCRATCH_SIZE - 1
        numMetrics = LW_ARRAY_ELEMENTS(*pMetrics) - 1;

        PMU_ASSERT_OK_OR_GOTO(status,
            perfCfControllerDlppc1xProfilingDramclkSearchTypeIterationScaleProfilingGet(
                pIterationProfiling, &pScaleProfiling),
            s_perfCfControllerPerfFloorEstimate_DLPPC_1X_exit);

        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfControllerEffLwrveRangeEstimate_DLPPC_1X(
                pDlppc1x,
                dramclkkHz,
                &primaryclkRangekHz,
                (*pMetrics),
                &numMetrics,
                &bMorePoints,
                &(PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_DLPPM_1X)
                {
                    .super =
                    {
                        .type = LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_DLPPM_1X,
                    },
                    .bounds =
                    {
                        .pEndpointLo = &lowerBound,
                        .pEndpointHi = &(*pMetrics)[DLPPC_1X_METRICS_SCRATCH_SIZE - 1],
                        .bGuardRailsApply = LW_TRUE,
                    },
                    .pProfiling = pScaleProfiling,
                }),
            s_perfCfControllerPerfFloorEstimate_DLPPC_1X_exit);

        // Add one for our additional metrics saved from previous iteration.
        numMetrics++;
        (*pMetrics)[numMetrics - 1] = (*pMetrics)[DLPPC_1X_METRICS_SCRATCH_SIZE - 1];

        perfCfControllerDlppc1xProfileDramclkSearchTypeIterationEnd(
            pIterationProfiling);
    }

    // Need to end the profiling for the last iteration still.
    perfCfControllerDlppc1xProfileDramclkSearchTypeIterationEnd(
        pIterationProfiling);

    PMU_ASSERT_OK_OR_GOTO(status,
        (idx >= DLPPC_1X_METRICS_SCRATCH_SIZE) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
        s_perfCfControllerPerfFloorEstimate_DLPPC_1X_exit);

    pDramclk->dramclkMetrics[LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_DRAMCLK_METRICS_IDX_PERF_FLOOR]
        .bSaturated = !bSatisfied;

    // Copy the power ceiling metrics to the DRAM structure.
    pDramclk->dramclkMetrics[LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_DRAMCLK_METRICS_IDX_PERF_FLOOR]
        .metrics = (*pMetrics)[idx];

s_perfCfControllerPerfFloorEstimate_DLPPC_1X_exit:
    return status;
}

/*!
 * @brief Determine if a VF point satisfies all power constraints
 *
 * @param[IN]  pDlppc1x          Controller to validate metrics against.
 * @param[IN]  pDlppm1xMetrics   Metrics to check for meeting power constraints.
 * @param[OUT] pBSatisfied       Boolean denoting whether @pDlppm1xMetrics satisfies
 *                               all power policies that @ref pDlppc1x considers.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pDlppc1x, @ref pDlppm1xMetrics, or
 *                                     @ref pBSatisfied are NULL.
 * @return FLCN_OK                     If @ref pDlppm1xMetrics was validated against
 *                                     the power policies of @ref pDlppc1x.
 */
static FLCN_STATUS
s_perfCfControllerCheckPwrConstraintsSatisfied_DLPPC_1X
(
    PERF_CF_CONTROLLER_DLPPC_1X                           *pDlppc1x,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X   *pDlppm1xMetrics,
    LwBool                                                *pBSatisfied
)
{
    LwU8          coreRailIdx;
    LwBool        bSatisfied;
    FLCN_STATUS   status = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pDlppc1x        == NULL) ||
         (pDlppm1xMetrics == NULL) ||
         (pBSatisfied     == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        s_perfCfControllerCheckPwrConstraintsSatisfied_DLPPC_1X_exit);

    // Check that each of the core-rail's sub-rails relationship sets are met.
    *pBSatisfied = LW_TRUE;
    for (coreRailIdx = 0; coreRailIdx < pDlppc1x->coreRail.numRails; coreRailIdx++)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfControllerRailCheckPwrPolRelSetSatisfied_DLPPC_1X(
                &(pDlppc1x       ->coreRail.rails[coreRailIdx]),
                &(pDlppm1xMetrics->coreRail.rails[coreRailIdx]),
                &pDlppc1x->coreRailStatus.rails[coreRailIdx].limits,
                &bSatisfied),
            s_perfCfControllerCheckPwrConstraintsSatisfied_DLPPC_1X_exit);
        if (!bSatisfied)
        {
             goto s_perfCfControllerCheckPwrConstraintsSatisfied_DLPPC_1X_done;
        }
    }

    // Check the core-rail's global relationship sets.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerCheckPwrPolRelSetSatisfied_DLPPC_1X(
            &(pDlppc1x       ->coreRail.pwrInRelSet),
            &(pDlppm1xMetrics->coreRail.pwrInTuple),
            &pDlppc1x->coreRailStatus.pwrInLimits,
            &bSatisfied),
            s_perfCfControllerCheckPwrConstraintsSatisfied_DLPPC_1X_exit);
    if (!bSatisfied)
    {
        goto s_perfCfControllerCheckPwrConstraintsSatisfied_DLPPC_1X_done;
    }

    // Check the FB rail relationship set.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerRailCheckPwrPolRelSetSatisfied_DLPPC_1X(
            &(pDlppc1x       ->fbRail),
            &(pDlppm1xMetrics->fbRail),
            &pDlppc1x->fbRailStatus.limits,
            &bSatisfied),
        s_perfCfControllerCheckPwrConstraintsSatisfied_DLPPC_1X_exit);
    if (!bSatisfied)
    {
        goto s_perfCfControllerCheckPwrConstraintsSatisfied_DLPPC_1X_done;
    }

    // Check the TGP relationship set.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerCheckPwrPolRelSetSatisfied_DLPPC_1X(
            &(pDlppc1x       ->tgpPolRelSet),
            &(pDlppm1xMetrics->tgpPwrTuple),
            &pDlppc1x->tgpLimits,
            &bSatisfied),
        s_perfCfControllerCheckPwrConstraintsSatisfied_DLPPC_1X_exit);
    if (!bSatisfied)
    {
        goto s_perfCfControllerCheckPwrConstraintsSatisfied_DLPPC_1X_done;
    }

s_perfCfControllerCheckPwrConstraintsSatisfied_DLPPC_1X_done:
    *pBSatisfied = bSatisfied;

s_perfCfControllerCheckPwrConstraintsSatisfied_DLPPC_1X_exit:
    return status;
}

static FLCN_STATUS
s_perfCfControllerDlppc1xPwrPoliciesStatusInit
(
    const PERF_CF_CONTROLLER_DLPPC_1X *pDlppc1x,
    PERF_CF_CONTROLLER_MULTIPLIER_DATA *pMultData
)
{
    FLCN_STATUS status;
    PWR_POLICIES_STATUS *pPwrPoliciesStatus;
    LwU8 coreRailIdx;

    // Initialize the PWR_POLICY status
    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfControllerMultiplierDataPwrPoliciesStatusInit(pMultData),
        s_perfCfControllerDlppc1xPwrPoliciesStatusInit_exit);

    // Get the status we just initialized.
    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfControllerMultiplierDataPwrPoliciesStatusGet(
            pDlppc1x->super.pMultData, &pPwrPoliciesStatus),
        s_perfCfControllerDlppc1xPwrPoliciesStatusInit_exit);
    PMU_ASSERT_OK_OR_GOTO(status,
        pPwrPoliciesStatus != NULL ? FLCN_OK : FLCN_ERR_ILWALID_STATE,
        s_perfCfControllerDlppc1xPwrPoliciesStatusInit_exit);

    // Register each of the individual core rails
    for (coreRailIdx = 0U; coreRailIdx < pDlppc1x->coreRail.numRails; coreRailIdx++)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfControllerDlppc1xPwrPoliciesStatusRailRegister(
                pPwrPoliciesStatus, &pDlppc1x->coreRail.rails[coreRailIdx]),
            s_perfCfControllerDlppc1xPwrPoliciesStatusInit_exit);
    }

    // Register the global core rail relationship sets
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerDlppc1xPwrPoliciesStatusRelSetRegister(
            pPwrPoliciesStatus, &pDlppc1x->coreRail.pwrInRelSet),
        s_perfCfControllerDlppc1xPwrPoliciesStatusInit_exit);

    // Register the FB rail
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerDlppc1xPwrPoliciesStatusRailRegister(
            pPwrPoliciesStatus, &pDlppc1x->fbRail),
        s_perfCfControllerDlppc1xPwrPoliciesStatusInit_exit);

    // Register the TGP relationship set
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerDlppc1xPwrPoliciesStatusRelSetRegister(
            pPwrPoliciesStatus, &pDlppc1x->tgpPolRelSet),
        s_perfCfControllerDlppc1xPwrPoliciesStatusInit_exit);

s_perfCfControllerDlppc1xPwrPoliciesStatusInit_exit:
    return status;
}

/*!
 * @brief   Registers the PWR_POLICY objects for a given PWR_POLICY_RELATIONSHIP
 *          set in the @ref PWR_POLICIES_STATUS
 *
 * @param[in]       pRelSet The relationship set to register
 * @param[in,out]   pStatus Status structure in which to register
 *
 * @return  @ref FLCN_OK                Success
 * @return  @ref FLCN_ERR_ILWALID_STATE A @ref PWR_POLICY_RELATIONSHIP pointed
 *                                      at by pRelSet is invalid.
 */
static FLCN_STATUS
s_perfCfControllerDlppc1xPwrPoliciesStatusRelSetRegister
(
    PWR_POLICIES_STATUS *pStatus,
    const LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_SET *pRelSet
)
{
    FLCN_STATUS status = FLCN_OK;
    LwBoardObjIdx polRelIdx;

    LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_SET_FOR_EACH_BEGIN(pRelSet, polRelIdx)
    {
        const PWR_POLICY_RELATIONSHIP *const pPolRel =
            PWR_POLICY_REL_GET(polRelIdx);

        PMU_ASSERT_OK_OR_GOTO(status,
            pPolRel != NULL ? FLCN_OK : FLCN_ERR_ILWALID_STATE,
            s_perfCfControllerDlppc1xPwrPoliciesStatusRelSetRegister_exit);

        boardObjGrpMaskBitSet(
            &pStatus->mask,
            BOARDOBJ_GET_GRP_IDX(&pPolRel->pPolicy->super.super));
    }
    LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_SET_FOR_EACH_END

s_perfCfControllerDlppc1xPwrPoliciesStatusRelSetRegister_exit:
    return status;
}

/*!
 * @brief Verify that a power tuple satisfies all power policy limits.
 *
 * @param[IN]   pRelSet     Power policy relationship set to verify against.
 * @param[IN]   pPwrTuple   Power tuple to verify satisfies all power policies.
 * @param[in]   pLimits     Cached power limits against which to check.
 * @param[OUT]  pBSatisfied Boolean specifying if all power policies were satisfied.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pRelSet or @ref pBSatisfied are NULL.
 * @return FLCN_OK                     If @ref pwrmW was checked against @ref pRelSet.
 */
static FLCN_STATUS
s_perfCfControllerCheckPwrPolRelSetSatisfied_DLPPC_1X
(
    LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_SET                                           *pRelSet,
    LW2080_CTRL_PMGR_PWR_TUPLE                                                             *pPwrTuple,
    const LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_PWR_POLICY_RELATIONSHIP_SET_LIMITS  *pLimits,
    LwBool                                                                                 *pBSatisfied
)
{
    PWR_POLICY_RELATIONSHIP   *pPolRel;
    LwBoardObjIdx              policyIdx;
    LwBool                     bSatisfied = LW_TRUE;
    FLCN_STATUS                status     = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pRelSet     == NULL) ||
         (pPwrTuple   == NULL) ||
         (pBSatisfied == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        s_perfCfControllerCheckPwrPolRelSetSatisfied_DLPPC_1X_exit);

    //
    // Iterate through the policy relationships, inclusive of the start
    // and end indicies.
    //
    LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_SET_FOR_EACH_BEGIN(pRelSet, policyIdx)
    {
        LwU32 limit;

        pPolRel = PWR_POLICY_REL_GET(policyIdx);
        PMU_ASSERT_OK_OR_GOTO(status,
            (pPolRel == NULL) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
            s_perfCfControllerCheckPwrPolRelSetSatisfied_DLPPC_1X_exit);

        PMU_ASSERT_OK_OR_GOTO(status,
            perfCfControllerDlppc1xPwrPolicyRelationshipSetLimitsLimitGet(
                pRelSet, pLimits, policyIdx, &limit),
            s_perfCfControllerCheckPwrPolRelSetSatisfied_DLPPC_1X_exit);

        switch (pwrPolicyLimitUnitGet(pPolRel->pPolicy))
        {
            case LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_POWER_MW:
                bSatisfied = (pPwrTuple->pwrmW <= limit);
                break;

            case LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_LWRRENT_MA:
                bSatisfied = (pPwrTuple->lwrrmA <= limit);
                break;

            default:
                PMU_ASSERT_OK_OR_GOTO(status,
                    FLCN_ERR_ILWALID_STATE,
                    s_perfCfControllerCheckPwrPolRelSetSatisfied_DLPPC_1X_exit);
                break;
        }

        if (!bSatisfied)
        {
            break;
        }
    }
    LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_SET_FOR_EACH_END

    *pBSatisfied = bSatisfied;

s_perfCfControllerCheckPwrPolRelSetSatisfied_DLPPC_1X_exit:
    return status;
}

/*!
 * @brief   Registers the @ref PWR_POLICY objects for a given rail in the
 *          @ref PWR_POLICIES_STATUS
 *
 * @param[in]       pRail   The rail for which to register
 * @param[in,out]   pStatus Status structure in which to register
 *
 * @return  @ref FLCN_OK    Success
 * @return  Others          Errors propagated from callees.
 */
static FLCN_STATUS
s_perfCfControllerDlppc1xPwrPoliciesStatusRailRegister
(
    PWR_POLICIES_STATUS *pStatus,
    const LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_RAIL *pRail
)
{
    FLCN_STATUS status;

    // Register both the "in" and "out" relationship sets
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerDlppc1xPwrPoliciesStatusRelSetRegister(
            pStatus, &pRail->pwrInRelSet),
        s_perfCfControllerDlppc1xPwrPoliciesStatusRailRegister_exit);
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerDlppc1xPwrPoliciesStatusRelSetRegister(
            pStatus, &pRail->pwrOutRelSet),
        s_perfCfControllerDlppc1xPwrPoliciesStatusRailRegister_exit);

s_perfCfControllerDlppc1xPwrPoliciesStatusRailRegister_exit:
    return status;
}

/*!
 * @brief Helper to check that a set of rail metrics satisfies a set of
 *        rail power policy relationships.
 *
 * @param[IN]   pRail           Rail whose power policy relationships to verify against.
 * @param[IN]   pRailMetrics    Rail metrics to verify.
 * @param[in]   pLimits         Cached power limits against which to check.
 * @param[OUT]  pBSatisfied     LW_TRUE if all of the metrics in @ref pRailMetrics satisfy
 *                              the power policy relationships of @ref pRail.
 *                              LW_FALSE otherwise.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pRail, @ref pRailMetrics, or @ref pBSatisfied
 *                                     are NULL.
 * @return FLCN_OK                     If @ref pRailMetrics was verified against @ref pRail.
 */
static FLCN_STATUS
s_perfCfControllerRailCheckPwrPolRelSetSatisfied_DLPPC_1X
(
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_RAIL              *pRail,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_RAIL       *pRailMetrics,
    const LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_RAIL_LIMITS *pLimits,
    LwBool                                                         *pBSatisfied
)
{
    LwBool        bSatisfied;
    FLCN_STATUS   status = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pRail        == NULL) ||
         (pRailMetrics == NULL) ||
         (pBSatisfied  == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        s_perfCfControllerRailCheckPwrPolRelSetSatisfied_DLPPC_1X_exit);

    //
    // Check that both input and output power policy relationships are
    // satisfied.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerCheckPwrPolRelSetSatisfied_DLPPC_1X(
            &(pRail->pwrInRelSet),
            &(pRailMetrics->pwrInTuple),
            &pLimits->pwrInLimits,
            &bSatisfied),
        s_perfCfControllerRailCheckPwrPolRelSetSatisfied_DLPPC_1X_exit);
    if (!bSatisfied)
    {
        goto s_perfCfControllerRailCheckPwrPolRelSetSatisfied_DLPPC_1X_done;
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerCheckPwrPolRelSetSatisfied_DLPPC_1X(
            &(pRail->pwrOutRelSet),
            &(pRailMetrics->pwrOutTuple),
            &pLimits->pwrOutLimits,
            &bSatisfied),
        s_perfCfControllerRailCheckPwrPolRelSetSatisfied_DLPPC_1X_exit);
    if (!bSatisfied)
    {
        goto s_perfCfControllerRailCheckPwrPolRelSetSatisfied_DLPPC_1X_done;
    }

s_perfCfControllerRailCheckPwrPolRelSetSatisfied_DLPPC_1X_done:
    *pBSatisfied = bSatisfied;

s_perfCfControllerRailCheckPwrPolRelSetSatisfied_DLPPC_1X_exit:
    return status;
}

/*!
 * @brief Helper to run through hysteresis on the best estimated limits so far
 *        and set a final recommendation for the arbiter.
 *
 * @param[IN]     pDlppc1x                    DLPPC_1X controller.
 * @param[IN]     pTupleBestEstimatedLimits   Tuple of frequencies that are estimated
 *                                            to be the best limit recommendations for
 *                                            this controller cycle
 * @param[in]     bMetricsObserved            Whether the metrics were
 *                                            successfully observed or not.
 * @param[in]     pstateIdxBelowSearchMin     The first @ref PSTATE index at
 *                                            which DLPPC_1X will not actively
 *                                            search.
 *
 * @note    The hysteresis logic in this function will cause DLPPC_1X to
 *          misbehave for several cycles when DRAMCLK overclocking is applied.
 *          The logic does comparisons ilwolving previously chosen DRAMCLK
 *          frequencies, but after OC, those DRAMCLK frequencies will no longer
 *          "exist," but each instead directly corresponds to one new
 *          overclocked DRAMCLK frequency. Because the actual DRAMCLK
 *          frequencies don't match, even though they may be the "same"
 *          conceptual DRAMCLK, the hysteresis can end up making decisions
 *          opposite of what it intends to until enough cycles complete that
 *          DLPPC's hysteresis history has caught up to the new overclocked
 *          DRAMCLK frequencies. This will be fixed in a follow-up if/when it
 *          needs to be.
 *
 * @return FLCN_ERR_ILWALID_STATE      No bestEstimatedLimits were passed into the function
 * @return FLCN_OK                     Limits were set succesfully
 * @return Others                      Errors propagated from callees.
 */
static FLCN_STATUS
s_perfCfControllerDlppc1xSetLimitRecommendations
(
    PERF_CF_CONTROLLER_DLPPC_1X                                    *pDlppc1x,
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_FREQ_TUPLE *pTupleBestEstimatedLimits,
    LwBool                                                          bMetricsObserved,
    LwBoardObjIdx                                                   pstateIdxBelowSearchMin
)
{
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_FREQ_TUPLE tupleChosenLimits;
    FLCN_STATUS    status = FLCN_OK;
    PERF_LIMITS_VF vfDomain;
    LwU32          lastOperatingDramclkkHz;
    LwU32          lwrrOperatingDramclkkHz;
    LwU32          lastChosenDramclkkHz;
    LwU32          bestEstimatedLimitsDramclkkHz;
    LwBool         bChoiceChange = LW_FALSE;
    LwBool         bEstimatedLimitMeaningful = LW_TRUE;

    PMU_ASSERT_OK_OR_GOTO(status,
        (pTupleBestEstimatedLimits == NULL) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
        s_perfCfControllerDlppc1xSetLimitRecommendations_initialFailure);

    //
    // Initialize chosenLimits to lastChosenLimits, so if we exit early we do not change
    // DLPPC_1X's rquest to the arbiter. Extract dramclks from all other freqTuple
    // structures in DLPPC_1X
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerDlppc1xFreqTupleCopy(&tupleChosenLimits, &pDlppc1x->tupleLastChosenLimits),
        s_perfCfControllerDlppc1xSetLimitRecommendations_initialFailure);

    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerDlppc1xFreqTupleGet(&pDlppc1x->tupleLastOperatingFrequencies,
                        LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_CLKDOM_DRAM, &lastOperatingDramclkkHz),
        s_perfCfControllerDlppc1xSetLimitRecommendations_chosenLimitsValid);

    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerDlppc1xFreqTupleGet(&pDlppc1x->tupleLastChosenLimits,
                        LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_CLKDOM_DRAM, &lastChosenDramclkkHz),
        s_perfCfControllerDlppc1xSetLimitRecommendations_chosenLimitsValid);

    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerDlppc1xFreqTupleGet(pTupleBestEstimatedLimits,
                        LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_CLKDOM_DRAM, &bestEstimatedLimitsDramclkkHz),
        s_perfCfControllerDlppc1xSetLimitRecommendations_chosenLimitsValid);

    //
    // If the metrics were correctly observed, then update the last operating
    // frequencies. Otherwise, clear them to invalid/NO_REQUEST values.
    //
    if (bMetricsObserved)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfControllerDlppc1xFreqTupleSet(&pDlppc1x->tupleLastOperatingFrequencies,
                    LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_CLKDOM_DRAM,
                    pDlppc1x->observedMetrics.fbRail.super.freqkHz),
            s_perfCfControllerDlppc1xSetLimitRecommendations_chosenLimitsValid);

        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfControllerDlppc1xFreqTupleSet(&pDlppc1x->tupleLastOperatingFrequencies,
                    LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_CLKDOM_GPC,
                    pDlppc1x->observedMetrics.coreRail.rails[0].super.freqkHz),
            s_perfCfControllerDlppc1xSetLimitRecommendations_chosenLimitsValid);
    }
    else
    {
        //
        // Note that in this case, bestEstimatedLimitsDramclkkHz should be
        // NO_REQUEST.
        //
        LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_FREQ_TUPLE_INIT(
            &pDlppc1x->tupleLastOperatingFrequencies);
    }

    //
    // If the last operating frequency doesn't match the newly-updated current
    // one, then clear the hysteresis.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerDlppc1xFreqTupleGet(
            &pDlppc1x->tupleLastOperatingFrequencies,
            LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_CLKDOM_DRAM,
            &lwrrOperatingDramclkkHz),
        s_perfCfControllerDlppc1xSetLimitRecommendations_chosenLimitsValid);
    if (!s_perfCfControllerDlppc1xDramclkAproxMatch(
            pDlppc1x, lwrrOperatingDramclkkHz, lastOperatingDramclkkHz))
    {
        pDlppc1x->dramHysteresisLwrrCountDec = 0;
        pDlppc1x->dramHysteresisLwrrCountInc = 0;
    }

    //
    // bestEstimatedLimitsDramclkkHz comes through as NO_REQUEST in any case
    // where DLPPC couldn't make a meaningful decision for some reason. If this
    // happens consistently, we want to eventually emit NO_REQUEST as our
    // tupleChosenLimits. To do that, we want to slowly release the limit as we
    // continue to receive NO_REQUEST here, going through all PSTATEs and then
    // at that point emitting NO_REQUEST (all with hysteresis).
    //
    // To do that, on each iteration we get NO_REQUEST here, we increase our
    // bestEstimatedLimitsDramclkkHz by one PSTATE. Then, once we are actively
    // requesting P0 in the tupleLastChosenLimits, we begin choosing NO_REQUEST
    // as our best estimated limit, and we continue to choose NO_REQUEST if the
    // previous request was NO_REQUEST and the current
    // bestEstimatedLimitsDramclkkHz is NO_REQUEST.
    //
    // Note that this means that we can only get NO_REQUEST as a true
    // bestEstimatedLimitsDramclkkHz (after this process) if
    // tupleLastChosenLimits was P0 or NO_REQUEST itself.
    //
    if (bestEstimatedLimitsDramclkkHz ==
            LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_FREQ_KHZ_NO_REQUEST)
    {
        //
        // If bestEstimatedLimitsDramclkkHz is NO_REQUEST, then DLPPC is not
        // making a "meaningful" choice, so mark that that's the case.
        //
        bEstimatedLimitMeaningful = LW_FALSE;

        if (lastChosenDramclkkHz !=
                LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_FREQ_KHZ_NO_REQUEST)
        {
            LwBool bLastChosenP0;

            PMU_ASSERT_OK_OR_GOTO(status,
                s_perfCfControllerDlppc1xDramclkFreqInP0(
                    pDlppc1x, lastChosenDramclkkHz, &bLastChosenP0),
                s_perfCfControllerDlppc1xSetLimitRecommendations_chosenLimitsValid);

            //
            // If the last PSTATE we chose was P0, then we really do want to start
            // choosing  NO_REQUEST as our best estimate, so only update the tuple
            // if that wasn't the case.
            //
            if (!bLastChosenP0)
            {
                vfDomain.idx   = pDlppc1x->fbRail.clkDomIdx;
                vfDomain.value = lastChosenDramclkkHz + 1000U;

                PMU_ASSERT_OK_OR_GOTO(status,
                    perfLimitsFreqkHzQuantize(&vfDomain, NULL, LW_FALSE),
                    s_perfCfControllerDlppc1xSetLimitRecommendations_chosenLimitsValid);

                bestEstimatedLimitsDramclkkHz = vfDomain.value;
                PMU_ASSERT_OK_OR_GOTO(status,
                    s_perfCfControllerDlppc1xFreqTupleSet(pTupleBestEstimatedLimits,
                                LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_CLKDOM_DRAM,
                                bestEstimatedLimitsDramclkkHz),
                    s_perfCfControllerDlppc1xSetLimitRecommendations_chosenLimitsValid);

                PMU_ASSERT_OK_OR_GOTO(status,
                    s_perfCfControllerDlppc1xFreqTupleSet(pTupleBestEstimatedLimits,
                                LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_CLKDOM_GPC,
                                LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_FREQ_KHZ_NO_REQUEST),
                    s_perfCfControllerDlppc1xSetLimitRecommendations_chosenLimitsValid);
            }
        }
    }

    //
    // If our lastChoseLimits did not give a request, then tupleChosenLimits should be updated to contain
    // the best estimated limit values. There is no need to do hysteresis as we don't have a previous
    // point to base comparisons off of.
    //
    // Otherwise, ensure that the dramclk we have predicted to have the best perf has been seen
    // the last few cycles DLPPC has ran. If it has not met the hysteresis conditions,
    // recommend the last chosen dramclk frequency and increment the correct hysteresis count.
    // If hysteresis conditions are met, update tupleLastChosenLimits to tupleChosenLimits for the next
    // loop.
    //
    // Note that we treat NO_REQUEST specially as an increasing case.
    //
    if (lastChosenDramclkkHz == LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_FREQ_KHZ_NO_REQUEST)
    {
        bChoiceChange = LW_TRUE;
        pDlppc1x->dramHysteresisLwrrCountDec = 0;
        pDlppc1x->dramHysteresisLwrrCountInc = 0;
    }
    else if ((bestEstimatedLimitsDramclkkHz > lastChosenDramclkkHz) ||
             (bestEstimatedLimitsDramclkkHz ==
                LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_FREQ_KHZ_NO_REQUEST))
    {
        pDlppc1x->dramHysteresisLwrrCountDec = 0;

        if (++pDlppc1x->dramHysteresisLwrrCountInc >= pDlppc1x->dramHysteresisCountInc)
        {
            bChoiceChange = LW_TRUE;
            pDlppc1x->dramHysteresisLwrrCountInc = 0;
        }
    }
    else if (bestEstimatedLimitsDramclkkHz < lastChosenDramclkkHz)
    {
        pDlppc1x->dramHysteresisLwrrCountInc = 0;
        if (++pDlppc1x->dramHysteresisLwrrCountDec >= pDlppc1x->dramHysteresisCountDec)
        {
            bChoiceChange = LW_TRUE;
            pDlppc1x->dramHysteresisLwrrCountDec = 0;
        }
    }
    else
    {
        //
        // In this case, we'll copy over the bestEtimatedLimits as it contains the same
        // DRAMCLK freq as previous itterations but may have an updated GPCCLK freq request
        //
        bChoiceChange = LW_TRUE;
        pDlppc1x->dramHysteresisLwrrCountDec = 0;
        pDlppc1x->dramHysteresisLwrrCountInc = 0;
    }

    //
    // If we passed hysteresis and want to change our limits, we have to do two
    // things:
    //  1.) Update the actually chosen limits.
    //  2.) Update inflection disablement. If DLPPE is making a meaningful
    //      choice then we can disable inflection points, but if DLPPE could not
    //      make a meaningful choice, then we need to re-enable inflection
    //      points.
    //
    if (bChoiceChange)
    {
        LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST *pDisableRequest;

        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfControllerDlppc1xFreqTupleCopy(
                &tupleChosenLimits, pTupleBestEstimatedLimits),
            s_perfCfControllerDlppc1xSetLimitRecommendations_chosenLimitsValid);

        // Retrieve the disable request structure pointer.
        PMU_ASSERT_OK_OR_GOTO(status,
            perfCfControllerInflectionPointsDisableRequestGet(
                &pDlppc1x->super, &pDisableRequest),
            s_perfCfControllerDlppc1xSetLimitRecommendations_chosenLimitsValid);
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pDisableRequest != NULL),
            FLCN_ERR_ILWALID_STATE,
            s_perfCfControllerDlppc1xSetLimitRecommendations_chosenLimitsValid);

        //
        // If the estimated limits were meaningful, then set the lowest disabled
        // PSTATE to below DLPPE's search minimum. Otherwise, re-enable
        // inflection by setting the index to INVALID
        //
        pDisableRequest->pstateIdxLowest = bEstimatedLimitMeaningful ?
            pstateIdxBelowSearchMin : LW2080_CTRL_PERF_PSTATE_INDEX_ILWALID;
    }

s_perfCfControllerDlppc1xSetLimitRecommendations_chosenLimitsValid:
    // Set the limit recommendations for the arbirter
    PMU_ASSERT_OK_OR_CAPTURE_FIRST_ERROR(status,
        s_perfCfControllerDlppc1xFreqTupleGet(
            &tupleChosenLimits,
            LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_CLKDOM_DRAM,
            &pDlppc1x->super.limitFreqkHz[pDlppc1x->fbRail.maxLimitIdx]));

    PMU_ASSERT_OK_OR_CAPTURE_FIRST_ERROR(status,
        s_perfCfControllerDlppc1xFreqTupleGet(
            &tupleChosenLimits,
            LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_CLKDOM_GPC,
            &pDlppc1x->super.limitFreqkHz[pDlppc1x->coreRail.rails[0].maxLimitIdx]));

    // TODO: Intentionally do not set other core-rails due to XBAR Fmax @ Vmin request from SSG.

    PMU_ASSERT_OK_OR_CAPTURE_FIRST_ERROR(status,
        s_perfCfControllerDlppc1xFreqTupleCopy(
            &(pDlppc1x->tupleLastChosenLimits), &tupleChosenLimits));

s_perfCfControllerDlppc1xSetLimitRecommendations_initialFailure:
    return status;
}

/*!
 * @brief   Helper to do a fuzzy match between dramclks
 *
 * @param[IN] pDlppc1x        Pointer to the DLPPC_1X controller.
 * @param[IN] actualDramclk   Dramclk frequency wanting to compare with
 * @param[IN] targetDramclk   Dramclk frequency to compare against
 *
 * @return    Boolean where true indicates the actualDramclk closely matches targetDramclk
 */
static LwBool
s_perfCfControllerDlppc1xDramclkAproxMatch
(
    PERF_CF_CONTROLLER_DLPPC_1X *pDlppc1x,
    LwU32 actualDramclk,
    LwU32 targetDramclk
)
{
    //
    // Numerical Analysis:
    //
    // We need to callwlate the percent difference between actual
    // DRAMCLK frequency and the expected one, to see if they should be
    // considered the "same" DRAMCLK or not. That is, we are callwlating:
    //  abs(dramclkkHz_{actual} - dramclkkHz_{expected}) / dramclkkHz_{expected}
    //
    // The percentage should be callwlated as a x.12.
    //
    // The maximum supported DRAMCLK is expected to be 8GHz; in kHz, this
    // requires 23 bits to represent. This means to support this value with
    // 12 fractional bits, a 52.12 is required.
    //
    // The only math performed is a division by a number greater than 1,
    // which can only remove bits, so this is all safe to do in 52.12.
    //
    LwU32 dramclkDiffkHz =
        actualDramclk > targetDramclk ?
            actualDramclk - targetDramclk :
            targetDramclk - actualDramclk;
    LwUFXP52_12 percentDiff =
        LW_TYPES_U64_TO_UFXP_X_Y_SCALED(52, 12,
            dramclkDiffkHz,
            targetDramclk);

    return lwU64CmpLT(
        &percentDiff,
        &(LwU64) { pDlppc1x->dramclkMatchTolerance });
}

/*!
 * @brief   Determines whether any of the @ref PWR_POLICY objects with which
 *          DLPPC is concerned lwrrently has an inflection limit below
 *          DLPPC_1X's search minimum.
 *
 * @param[in]   pDlppc1x                DLPPC_1X object pointer
 * @param[in]   pPwrPoliciesStatus      Current @ref PWR_POLICIES status
 * @param[in]   pstateIdxBelowSearchMin Lowest @ref PSTATE
 *                                      @ref LwBoardObjIdx at which DLPPC_1X
 *                                      will not actively make decisions.
 * @param[out]  pbInInflectionZone      @ref LW_TRUE if any @ref PWR_POLICY
 *                                      has an inflection limit below the
 *                                      search min, and @ref LW_FALSE
 *                                      otherwise.
 *
 * @return  @ref FLCN_OK    Success
 * @return  Others          Errors propagated from callees.
 */
static FLCN_STATUS
s_perfCfControllerDlppc1xInInflectionZone
(
    const PERF_CF_CONTROLLER_DLPPC_1X *pDlppc1x,
    const PWR_POLICIES_STATUS *pPwrPoliciesStatus,
    LwBoardObjIdx pstateIdxBelowSearchMin,
    LwBool *pbInInflectionZone
)
{
    FLCN_STATUS status;
    LwU8 railIdx;

    // Check the individual core rails' policies.
    for (railIdx = 0U; railIdx < pDlppc1x->coreRail.numRails; railIdx++)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfControllerDlppc1xInInflectionZoneForRail(
                &pDlppc1x->coreRail.rails[railIdx],
                pPwrPoliciesStatus,
                pstateIdxBelowSearchMin,
                pbInInflectionZone),
            s_perfCfControllerDlppc1xInInflectionZone_exit);
        if (*pbInInflectionZone)
        {
            goto s_perfCfControllerDlppc1xInInflectionZone_exit;
        }
    }

    // Check the whole-core policies.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerDlppc1xInInflectionZoneForRelSet(
            &pDlppc1x->coreRail.pwrInRelSet,
            pPwrPoliciesStatus,
            pstateIdxBelowSearchMin,
            pbInInflectionZone),
        s_perfCfControllerDlppc1xInInflectionZone_exit);
    if (*pbInInflectionZone)
    {
        goto s_perfCfControllerDlppc1xInInflectionZone_exit;
    }

    // Check the FB rail policies.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerDlppc1xInInflectionZoneForRail(
            &pDlppc1x->fbRail,
            pPwrPoliciesStatus,
            pstateIdxBelowSearchMin,
            pbInInflectionZone),
        s_perfCfControllerDlppc1xInInflectionZone_exit);
    if (*pbInInflectionZone)
    {
        goto s_perfCfControllerDlppc1xInInflectionZone_exit;
    }

    // Check the TGP policies.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerDlppc1xInInflectionZoneForRelSet(
            &pDlppc1x->tgpPolRelSet,
            pPwrPoliciesStatus,
            pstateIdxBelowSearchMin,
            pbInInflectionZone),
        s_perfCfControllerDlppc1xInInflectionZone_exit);
    if (*pbInInflectionZone)
    {
        goto s_perfCfControllerDlppc1xInInflectionZone_exit;
    }

s_perfCfControllerDlppc1xInInflectionZone_exit:
    return status;
}

/*!
 * @brief   Determines whether any of the @ref PWR_POLICY objects with which
 *          a rail is concerned lwrrently has an inflection limit below
 *          DLPPC_1X's search minimum.
 *
 * @param[in]   pDlppc1x                DLPPC_1X rail pointer.
 * @param[in]   pPwrPoliciesStatus      Current @ref PWR_POLICIES status
 * @param[in]   pstateIdxBelowSearchMin Lowest @ref PSTATE
 *                                      @ref LwBoardObjIdx at which DLPPC_1X
 *                                      will not actively make decisions.
 * @param[out]  pbInInflectionZone      @ref LW_TRUE if any @ref PWR_POLICY
 *                                      has an inflection limit below the
 *                                      search min, and @ref LW_FALSE
 *                                      otherwise.
 *
 * @return  @ref FLCN_OK    Success
 * @return  Others          Errors propagated from callees.
 */
static FLCN_STATUS
s_perfCfControllerDlppc1xInInflectionZoneForRail
(
    const LW2080_CTRL_PERF_PERF_CF_CONTROLLER_DLPPC_1X_RAIL *pRail,
    const PWR_POLICIES_STATUS *pPwrPoliciesStatus,
    LwBoardObjIdx pstateIdxBelowSearchMin,
    LwBool *pbInInflectionZone
)
{
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerDlppc1xInInflectionZoneForRelSet(
            &pRail->pwrInRelSet,
            pPwrPoliciesStatus,
            pstateIdxBelowSearchMin,
            pbInInflectionZone),
        s_perfCfControllerDlppc1xInInflectionZoneForRail_exit);
    if (*pbInInflectionZone)
    {
        goto s_perfCfControllerDlppc1xInInflectionZoneForRail_exit;
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerDlppc1xInInflectionZoneForRelSet(
            &pRail->pwrInRelSet,
            pPwrPoliciesStatus,
            pstateIdxBelowSearchMin,
            pbInInflectionZone),
        s_perfCfControllerDlppc1xInInflectionZoneForRail_exit);
    if (*pbInInflectionZone)
    {
        goto s_perfCfControllerDlppc1xInInflectionZoneForRail_exit;
    }

s_perfCfControllerDlppc1xInInflectionZoneForRail_exit:
    return status;
}

/*!
 * @brief   Determines whether any of the @ref PWR_POLICY objects in a
 *          @ref PWR_POLICY_RELATIONSHIP set lwrrently has an inflection limit
 *          below DLPPC_1X's search minimum.
 *
 * @param[in]   pRelSet                 Relationship set pointer
 * @param[in]   pPwrPoliciesStatus      Current @ref PWR_POLICIES status
 * @param[in]   pstateIdxBelowSearchMin Lowest @ref PSTATE
 *                                      @ref LwBoardObjIdx at which DLPPC_1X
 *                                      will not actively make decisions.
 * @param[out]  pbInInflectionZone      @ref LW_TRUE if any @ref PWR_POLICY
 *                                      has an inflection limit below the
 *                                      search min, and @ref LW_FALSE
 *                                      otherwise.
 *
 * @return  @ref FLCN_OK                Success
 * @return  @ref FLCN_ERR_ILWALID_STATE Any @ref PWR_POLICY_RELATIONSHIP or its 
 *                                      @ref PWR_POLICY pointer in pRelSet is
 *                                      @ref NULL
 */
static FLCN_STATUS
s_perfCfControllerDlppc1xInInflectionZoneForRelSet
(
    const LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_SET *pRelSet,
    const PWR_POLICIES_STATUS *pPwrPoliciesStatus,
    LwBoardObjIdx pstateIdxBelowSearchMin,
    LwBool *pbInInflectionZone
)
{
    FLCN_STATUS status;
    LwBoardObjIdx polRelIdx;

    *pbInInflectionZone = LW_FALSE;
    LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_SET_FOR_EACH_BEGIN(pRelSet, polRelIdx)
    {
        const PWR_POLICY_RELATIONSHIP *const pPolRel =
            PWR_POLICY_REL_GET(polRelIdx);

        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pPolRel != NULL) && (pPolRel->pPolicy != NULL),
            FLCN_ERR_ILWALID_STATE,
            s_perfCfControllerDlppc1xInInflectionZoneForRelSet_exit);

        if (LW2080_CTRL_PMGR_PWR_POLICY_TYPE_IMPLEMENTS_DOMGRP(
                BOARDOBJ_GET_TYPE(&pPolRel->pPolicy->super.super)))
        {
            const RM_PMU_PMGR_PWR_POLICY_BOARDOBJ_GET_STATUS_UNION *const pPolicyStatus =
                pwrPoliciesStatusPolicyStatusGetConst(
                    pPwrPoliciesStatus,
                    BOARDOBJ_GET_GRP_IDX(&pPolRel->pPolicy->super.super));
            LwBoardObjIdx pstateIdxDomGrpCeiling;

            PMU_ASSERT_TRUE_OR_GOTO(status,
                (pstateIdxDomGrpCeiling = PSTATE_GET_INDEX_BY_LEVEL(
                    pPolicyStatus->domGrp.domGrpCeiling.values[RM_PMU_PERF_DOMAIN_GROUP_PSTATE])) !=
                        LW2080_CTRL_PERF_PSTATE_INDEX_ILWALID,
                FLCN_ERR_ILWALID_STATE,
                s_perfCfControllerDlppc1xInInflectionZoneForRelSet_exit);

            if (pstateIdxDomGrpCeiling < pstateIdxBelowSearchMin)
            {
                *pbInInflectionZone = LW_TRUE;
                break;
            }
        }
    }
    LW2080_CTRL_PMGR_PWR_POLICY_RELATIONSHIP_SET_FOR_EACH_END

s_perfCfControllerDlppc1xInInflectionZoneForRelSet_exit:
    return status;
}

/*!
 * @brief   Determines if a given DRAMCLK frequency is in P0
 *
 * @param[in]   pDlppc1x    DLPPC_1X object pointer
 * @param[in]   dramclkkHz  DRAMCLK frequency to check
 * @param[out]  pbFreqInP0  Set to @ref LW_TRUE if dramclkkHz is in P0, and
 *                          @ref LW_FALSE otherwise
 *
 * @return  @ref FLCN_OK                Success
 * @return  @ref FLCN_ERR_ILWALID_STATE @ref PSTATE for P0 did not exist
 * @return  Others                      Errors propagated from callees.
 */
static FLCN_STATUS
s_perfCfControllerDlppc1xDramclkFreqInP0
(
    const PERF_CF_CONTROLLER_DLPPC_1X *pDlppc1x,
    LwU32 dramclkkHz,
    LwBool *pbFreqInP0
)
{
    FLCN_STATUS status;
    PSTATE *const pPstateP0 = PSTATE_GET_BY_HIGHEST_INDEX();
    PERF_LIMITS_PSTATE_RANGE pstateIdxRangeP0;
    PERF_LIMITS_VF_RANGE dramclkRangeP0 =
    {
        .idx = pDlppc1x->fbRail.clkDomIdx,
    };

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pPstateP0 != NULL),
        FLCN_ERR_ILWALID_STATE,
        s_perfCfControllerDlppc1xDramclkFreqInP0_exit);

    PERF_LIMITS_RANGE_SET_VALUE(
        &pstateIdxRangeP0, BOARDOBJ_GET_GRP_IDX(&pPstateP0->super));

    PMU_ASSERT_OK_OR_GOTO(status,
        perfLimitsPstateIdxRangeToFreqkHzRange(
            &pstateIdxRangeP0,
            &dramclkRangeP0),
        s_perfCfControllerDlppc1xDramclkFreqInP0_exit);

    *pbFreqInP0 = PERF_LIMITS_RANGE_CONTAINS_VALUE(&dramclkRangeP0, dramclkkHz);

s_perfCfControllerDlppc1xDramclkFreqInP0_exit:
    return status;
}

/*!
 * @brief   Compares the power values for the DRAMCLKs at the given two indices
 *          in @ref PERF_CF_CONTROLLER_DLPPC_1X::dramclk and determines which
 *          one uses less power
 *
 * @details This is a fuzzy comparison based on
 *          @ref PERF_CF_CONTROLLER_DLPPC_1X::approxPowerThresholdPct
 *
 * @param[in]   pDlppc1x    DLPPC_1X object pointer
 * @param[in]   dramclkIdx1 Index of the DRAMCLK against which to compare
 * @param[in]   dramclkIdx2 Index of the DRAMCLK to compare
 *
 * @return  @ref LW_TRUE    If the power@dramclkIdx2 < power@dramclkIdx1
 * @return  @ref LW_FALSE   If the power@dramclkIdx2 >= power@dramclkIdx1
 */
static LwBool
s_perfCfControllerDlppc1xIsPowerLessThan
(
    const PERF_CF_CONTROLLER_DLPPC_1X *pDlppc1x,
    LwU8 dramclkIdx1,
    LwU8 dramclkIdx2
)
{
    //
    // If the indices are the same, return false, because the power can't be
    // less than itself
    //
    if (dramclkIdx1 == dramclkIdx2)
    {
        return LW_FALSE;
    }

    //
    // The "equality point" is always callwlated from the power at the lower
    // DRAMCLK, so we need to determine from which DRAMCLK to callwlate it.
    // 
    if (dramclkIdx2 > dramclkIdx1)
    {
        //
        // Numerical Analysis:
        // We need to callwlate:
        //  multiplierPct = (1 + approxPowerThresholdPct)
        //  equalityPointmW = power1mW * multiplierPct
        // 
        // multiplierPct = 1 + approxPowerThresholdPct
        // approxPowerThresholdPct is a signed x.12 percentage that should
        // always have an aboslute value <= 1. This therefore yields at most a
        // 2.12 that can be cast back to unsigned.
        //
        // power1mW * multiplierPct
        // power1mW is a power value, and power values are expected to always be
        // less than 10 kW. In milliwatts, 10 kW requires:
        //  ceil(log2(10 * 1000 * 1000)) = 24
        // bits.
        //
        // As noted, multiplierPct is at most a 2.12. Conservatively, this means
        // that up to 2 integer and 12 fractional bits may be added in the
        // multiplication, resulting in a 26.12. This intermediate
        // representation is greater than 32 bits, so the math must be done in
        // 64-bit.
        //
        // Then, however, to get the result back to a 64.0 representation, the
        // 12 fractional bits are shifted off.
        // 
        const LwU32 power1mW = s_perfCfControllerDlppc1xDramclkCompPwrGet(
            pDlppc1x, dramclkIdx1);
        const LwU32 power2mW = s_perfCfControllerDlppc1xDramclkCompPwrGet(
            pDlppc1x, dramclkIdx2);
        const LwUFXP52_12 multiplierPct = (LwUFXP52_12)
            (LW_TYPES_S64_TO_SFXP_X_Y(52, 12, 1) +
             ((LwSFXP52_12)pDlppc1x->approxPowerThresholdPct));
        const LwU64 equalityPointmW = multUFXP64(12U, multiplierPct, power1mW);

        //
        // In this case, the equality point defines the highest value at which
        // power2mW can be considered < power1mW. This means that we can just
        // return power2mW < equalityPointmW. In the positive case, note that
        // the equality point itself is considered part of the range in which
        // power2mW is considered less than power1mW.
        //
        return power2mW < equalityPointmW;
    }

    //
    // In this case, we can just call the function with the arguments reversed
    // and ilwert the condition.
    //
    return !s_perfCfControllerDlppc1xIsPowerLessThan(
        pDlppc1x, dramclkIdx2, dramclkIdx1);
}

/*!
 * @brief   Helper to retrieve the perf estimated for a VF-point on a given DRAMCLK.
 *
 * @param[IN] pDlppc1x     Pointer to the DLPPC_1X controller.
 * @param[IN] dramclkIdx   Index into PERF_CF_CONTORLLER_DLPPC_1X::dramclk of the
 *                         DRAMCLK data to access.
 * @param[IN] metricsIdx   Index into LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_DRAMCLK::dramclkMetrics
 *                         to access.
 *
 * @return DLPPC_1X estimated performance of the VF point, @ref metricsIdx,
 *         for the DRAMCLK, @ref dramclkIdx.
 */
static inline LwUFXP20_12
s_perfCfControllerDlppc1xDramclkPerfGet
(
    const PERF_CF_CONTROLLER_DLPPC_1X   *pDlppc1x,
    LwU8                           dramclkIdx,
    LwU8                           metricsIdx
)
{
    //
    // Intentional omission of sanity checking parameters as
    // max number instructions inlined by this function is reached.
    //
    return pDlppc1x->dramclk[dramclkIdx].dramclkMetrics[metricsIdx].metrics.perfMetrics.perfRatio;
}

/*!
 * @brief   Helper to retrieve the power estimated for a VF-point on a given DRAMCLK.
 *
 * @param[IN] pDlppc1x     Pointer to the DLPPC_1X controller.
 * @param[IN] dramclkIdx   Index into PERF_CF_CONTORLLER_DLPPC_1X::dramclk of the
 *                         DRAMCLK data to access.
 * @param[IN] metricsIdx   Index into LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_DRAMCLK::dramclkMetrics
 *                         to access.
 *
 * @return DLPPC_1X estimated TGP of the VF point, @ref metricsIdx,
 *         for the DRAMCLK, @ref dramclkIdx.
 */
static inline LwU32
s_perfCfControllerDlppc1xDramclkPwrGet
(
    const PERF_CF_CONTROLLER_DLPPC_1X   *pDlppc1x,
    LwU8                           dramclkIdx,
    LwU8                           metricsIdx
)
{
    //
    // Intentional omission of sanity checking parameters as
    // max number instructions inlined by this function is reached.
    //
    return pDlppc1x->dramclk[dramclkIdx].dramclkMetrics[metricsIdx].metrics.tgpPwrTuple.pwrmW;
}

/*!
 * @brief   Helper to retrieve the perf for a DRAMCLK's best VF-point.
 *
 * @param[IN] pDlppc1x     Pointer to the DLPPC_1X controller.
 * @param[IN] dramclkIdx   Index into PERF_CF_CONTORLLER_DLPPC_1X::dramclk of the
 *                         DRAMCLK data to access.
 *
 * @return DLPPC_1X estimated performance of the of the best VF point for
 *         the DRAMCLK specified by @ref dramclkIdx.
 */
static inline LwUFXP20_12
s_perfCfControllerDlppc1xDramclkCompPerfGet
(
    const PERF_CF_CONTROLLER_DLPPC_1X   *pDlppc1x,
    LwU8                           dramclkIdx
)
{
    //
    // Intentional omission of sanity checking parameters as
    // max number instructions inlined by this function is reached.
    //
    return s_perfCfControllerDlppc1xDramclkPerfGet(pDlppc1x,
                                                   dramclkIdx,
                                                   pDlppc1x->dramclk[(dramclkIdx)].compMetricsIdx);
}

/*!
 * @brief   Helper to retrieve the power for a DRAMCLK's best VF-point.
 *
 * @param[IN] pDlppc1x     Pointer to the DLPPC_1X controller.
 * @param[IN] dramclkIdx   Index into PERF_CF_CONTORLLER_DLPPC_1X::dramclk of the
 *                         DRAMCLK data to access.
 *
 * @return DLPPC_1X estimated power of the of the best VF point for
 *         the DRAMCLK specified by @ref dramclkIdx.
 */
static inline LwU32
s_perfCfControllerDlppc1xDramclkCompPwrGet
(
    const PERF_CF_CONTROLLER_DLPPC_1X   *pDlppc1x,
    LwU8                           dramclkIdx
)
{
    //
    // Intentional omission of sanity checking parameters as
    // max number instructions inlined by this function is reached.
    //
    return s_perfCfControllerDlppc1xDramclkPwrGet(pDlppc1x,
                                                  dramclkIdx,
                                                  pDlppc1x->dramclk[(dramclkIdx)].compMetricsIdx);
}

/*!
 * @brief   Helper to set a specific clock to the given frequency.
 *
 * @param[IN] pFreqTuple   Pointer to a PERF_CF_CONTROLLER_DLPPC_1X_STATUS_FREQ_TUPLE structure.
 * @param[IN] clkDomIdx    Index into PERF_CF_CONTROLLER_DLPPC_1X_STATUS_FREQ::freqKhz
 *                         to set with the requested value
 * @param[IN] freqkHz      Frequency value to set the given clkDomIdx to
 *
 * @return   @ref FLCN_OK                    Success
             @ref FLCN_ERR_ILWALID_ARGUMENT  Invalid cldDomIdx
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
s_perfCfControllerDlppc1xFreqTupleSet
(
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_FREQ_TUPLE *pFreqTuple,
    LwU8 clkDomIdx,
    LwU32 freqkHz
)
{
    FLCN_STATUS status = FLCN_OK;
    PMU_ASSERT_OK_OR_GOTO(status,
        (clkDomIdx >= LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_MAX_CLKDOMS) ?
        FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        s_perfCfControllerDlppc1xFreqTupleSet_exit);

    pFreqTuple->freqkHz[clkDomIdx] = freqkHz;

s_perfCfControllerDlppc1xFreqTupleSet_exit:
    return status;
}

/*!
 * @brief   Helper to set a specific clock to the given frequency.
 *
 * @param[IN]  pFreqTuple   Pointer to a PERF_CF_CONTROLLER_DLPPC_1X_STATUS_FREQ_TUPLE structure.
 * @param[IN]  clkDomIdx    Index into PERF_CF_CONTROLLER_DLPPC_1X_STATUS_FREQ::freqKhz
 *                          to get the frequency value from
 * @param[OUT] pFreqkHz     Pointer to LwU32 to fill with the given frequency
 * @return   @ref FLCN_OK                    Success
             @ref FLCN_ERR_ILWALID_ARGUMENT  Invalid cldDomIdx
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
s_perfCfControllerDlppc1xFreqTupleGet
(
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_FREQ_TUPLE *pFreqTuple,
    LwU8 clkDomIdx,
    LwU32 *pFreqkHz
)
{
    FLCN_STATUS status = FLCN_OK;

    PMU_ASSERT_OK_OR_GOTO(status,
        ((clkDomIdx >= LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_MAX_CLKDOMS) ?
            FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK),
        s_perfCfControllerDlppc1xFreqTupleGet_exit);

    *pFreqkHz = pFreqTuple->freqkHz[clkDomIdx];

s_perfCfControllerDlppc1xFreqTupleGet_exit:
    return status;
}

/*!
 * @brief   Helper to copy LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_FREQ_TUPLE
 *
 * @param[OUT] pFreqTupleTo   Pointer to a PERF_CF_CONTROLLER_DLPPC_1X_STATUS_FREQ_TUPLE wanting
 *                            to copy to
 * @param[IN]  pFreqTupleFrom Pointer to a PERF_CF_CONTROLLER_DLPPC_1X_STATUS_FREQ_TUPLE wanting
 *                            to copy from
 * @return   @ref FLCN_OK                    Success
             @ref FLCN_ERR_ILWALID_ARGUMENT  Invalid cldDomIdx
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
s_perfCfControllerDlppc1xFreqTupleCopy
(
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_FREQ_TUPLE *pFreqTupleTo,
    LW2080_CTRL_PERF_PERF_CF_CONTROLLER_STATUS_DLPPC_1X_FREQ_TUPLE *pFreqTupleFrom
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       freqkHz;

    // Copy over DRAMCLK
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerDlppc1xFreqTupleGet(pFreqTupleFrom,
            LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_CLKDOM_DRAM,
            &freqkHz),
        s_perfCfControllerDlppc1xFreqTupleCopy_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerDlppc1xFreqTupleSet(pFreqTupleTo,
            LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_CLKDOM_DRAM,
            freqkHz),
        s_perfCfControllerDlppc1xFreqTupleCopy_exit);

    // Copy over GPCCLK
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerDlppc1xFreqTupleGet(pFreqTupleFrom,
            LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_CLKDOM_GPC,
            &freqkHz),
        s_perfCfControllerDlppc1xFreqTupleCopy_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfControllerDlppc1xFreqTupleSet(pFreqTupleTo,
            LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_CLKDOM_GPC,
            freqkHz),
        s_perfCfControllerDlppc1xFreqTupleCopy_exit);

s_perfCfControllerDlppc1xFreqTupleCopy_exit:
    return status;
}

/*!
 * @brief   Helper to check if two perf ratio values are approximately equal to one another
 *
 * @param[IN] dramclkIdx1  Dramclk to compare against
 * @param[IN] dramclkIdx   Dramclk being compared
 * @return   @ref LW_TRUE if the values are approximately equal
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwBool
s_perfCfControllerDlppc1xPerfApproxMatch
(
    const PERF_CF_CONTROLLER_DLPPC_1X *pDlppc1x,
    LwU32 dramclkIdx1,
    LwU32 dramclkIdx2
)
{
    LwSFXP20_12 diffPerfRatio;
    LwSFXP20_12 percentDiff;
    LwUFXP20_12 perfRatio1, perfRatio2;

    LwSFXP20_12 lowerThreshold, upperThreshold;

    perfRatio1 = s_perfCfControllerDlppc1xDramclkCompPerfGet(pDlppc1x, dramclkIdx1);
    perfRatio2 = s_perfCfControllerDlppc1xDramclkCompPerfGet(pDlppc1x, dramclkIdx2);

    if (perfRatio1 == 0 || perfRatio2 == 0)
    {
        return LW_FALSE;
    }

    if (dramclkIdx1 == dramclkIdx2)
    {
        return LW_TRUE;
    }

    //
    // Comparison for approx match is done by the following:
    // %diffPerfRatio == (higherPerfRatio - lowPerfRatio)/lowPerfRatio
    // where higherPerfRatio is the perfRatio value of the higher dramclk and
    // lowerPerfRatio is the perfRatio of the lower dramclk
    //
    // For a theshold value of -x%:
    // the %diffPerfRatio must lie between -x% and 0%
    //
    // For a threshold value of +x%:
    // the %diffPerfRatio must lie between 0% and x%
    //
    // Numerical Analysis:
    // perfRatio values are 8.12 numbers, because of this the upper
    // 12 bits are always unused. This means colwering the values between
    // signed and unsigned integers should not have an effect on the absolute
    // value of the numbers.
    //
    // subtracting the 2 perfRatio numbers gets a 8.12 FXP value.
    // Shifting that over to the left by 12 gets a 8.24 FXP number.
    // Dividing diffPerfRatio by perfRatio1 results back into a 1.12 value.
    //

    diffPerfRatio = (dramclkIdx2 > dramclkIdx1) ?
                        (LwS32){perfRatio2} - (LwS32){perfRatio1} :
                        (LwS32){perfRatio1} - (LwS32){perfRatio2};

    diffPerfRatio = diffPerfRatio << 12;
    percentDiff = diffPerfRatio / ((dramclkIdx2 > dramclkIdx1) ?
                    (LwS32){perfRatio1}:(LwS32){perfRatio2});

    lowerThreshold = (DRF_VAL(_TYPES, _SFXP, _INTEGER_SIGN(20, 12), pDlppc1x->approxPerfThreshold) ==
                            LW_TYPES_SFXP_INTEGER_SIGN_NEGATIVE) ?
                      pDlppc1x->approxPerfThreshold : LW_TYPES_S32_TO_SFXP_X_Y(20, 12, 0);

    upperThreshold = (DRF_VAL(_TYPES, _SFXP, _INTEGER_SIGN(20, 12), pDlppc1x->approxPerfThreshold) ==
                            LW_TYPES_SFXP_INTEGER_SIGN_NEGATIVE)?
                      LW_TYPES_S32_TO_SFXP_X_Y(20, 12, 0) : pDlppc1x->approxPerfThreshold;

    return (percentDiff <= upperThreshold) && (percentDiff >= lowerThreshold);
}

/*!
 * @brief   Helper to compare two perf ratio values
 *
 * @param[IN] dramclkIdx1  Dramclk to compare against
 * @param[IN] dramclkIdx   Dramclk being compared
 * @return   @ref LW_TRUE if the values fits the passed in comparison criteris
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwBool
s_perfCfControllerDlppc1xIsPerfGreater
(
    const PERF_CF_CONTROLLER_DLPPC_1X *pDlppc1x,
    LwU32 dramclkIdx1,
    LwU32 dramclkIdx2
)
{
    LwSFXP20_12 diffPerfRatio;
    LwSFXP20_12 percentDiff;
    LwUFXP20_12 perfRatio1, perfRatio2;
    LwSFXP20_12 upperThreshold;

    perfRatio1 = s_perfCfControllerDlppc1xDramclkCompPerfGet(pDlppc1x, dramclkIdx1);
    perfRatio2 = s_perfCfControllerDlppc1xDramclkCompPerfGet(pDlppc1x, dramclkIdx2);

    if (perfRatio1 == 0 || perfRatio2 == 0 || dramclkIdx1 == dramclkIdx2)
    {
        return LW_FALSE;
    }

    //
    // For dramclkIdx2 to have a greater perf then dramclkIdx1, the
    // percentDifference in perf w.r.t the dramclks has to be >= x%.
    // Where x% represents a Minimum percent difference needed to be seen
    // between perf@dramclkIdx2 and perf@dramclk1Idx.
    //
    // For dramclk2Idx > dramclk1Idx
    // If approxPerfThreshold is negative, then x% is == to 0% since
    // a negative approxPerfThreshold represents perf@dramclkIdx2 and
    // perf@dramclkIdx being found equal in the range (approxPerfTrheshold, 0)
    //
    // If approxPerfThreshold is positive, then x% is == to approxPerfThreshold
    // since a positive approxPerfThreshold represents perf@dramclkIdx2 and
    // perf@dramclkIdx1 being equal in the range (0, approxPerfThrehsold).
    //
    // In the case where dramclk1Idx > dramclk2Idx
    // We want to do the same checks as above, but in order for
    // dramclkIdx2 to be > then dramclkIdx1 then our checks have be "ilwerted"
    // since dramclkIdx2 is the smaller dramclk (i.e. the threshold is being
    // applied in reverse)
    //
    // Numerical Analysis:
    // perfRatio values are 8.12 numbers, because of this the upper
    // 12 bits are always unused. This means colwering the values between
    // signed and unsigned integers should not have an effect on the absolute
    // value of the numbers.
    //
    // subtracting the 2 perfRatio numbers gets a 8.12 FXP value.
    // Shifting that over to the left by 12 gets a 8.24 FXP number.
    // Dividing diffPerfRatio by perfRatio1 results back into a 1.12 value.
    //

    diffPerfRatio = (dramclkIdx2 > dramclkIdx1) ?
                        (LwS32){perfRatio2} - (LwS32){perfRatio1} :
                        (LwS32){perfRatio1} - (LwS32){perfRatio2};

    diffPerfRatio = diffPerfRatio << 12;
    percentDiff = diffPerfRatio / ((dramclkIdx2 > dramclkIdx1) ?
                    (LwS32){perfRatio1}:(LwS32){perfRatio2});

    if (dramclkIdx2 > dramclkIdx1)
    {
        upperThreshold = (DRF_VAL(_TYPES, _SFXP, _INTEGER_SIGN(20, 12), pDlppc1x->approxPerfThreshold)==
                        LW_TYPES_SFXP_INTEGER_SIGN_NEGATIVE) ?
                  LW_TYPES_S32_TO_SFXP_X_Y(20, 12, 0) : pDlppc1x->approxPerfThreshold;
    }
    else
    {
        upperThreshold = (DRF_VAL(_TYPES, _SFXP, _INTEGER_SIGN(20, 12), pDlppc1x->approxPerfThreshold)==
                        LW_TYPES_SFXP_INTEGER_SIGN_NEGATIVE) ?
                        pDlppc1x->approxPerfThreshold:LW_TYPES_S32_TO_SFXP_X_Y(20, 12, 0);
    }

    return percentDiff > upperThreshold;
}


