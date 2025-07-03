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
 * @file    perf_cf_pwr_model_dlppm_1x.h
 * @brief   DLPPM 1x PERF_CF Pwr Model related defines.
 *
 * @copydoc perf_cf.h
 */

#ifndef PERF_CF_PWR_MODEL_DLPPM_1X_H
#define PERF_CF_PWR_MODEL_DLPPM_1X_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_pm_sensor.h"
#include "perf/cf/perf_cf_pwr_model.h"
#include "ctrl/ctrl2080/ctrl2080perf_cf.h"
#include "ctrl/ctrl2080/ctrl2080volt.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_CF_PWR_MODEL_DLPPM_1X_RAIL          PERF_CF_PWR_MODEL_DLPPM_1X_RAIL;

typedef struct PERF_CF_PWR_MODEL_DLPPM_1X_CORE_RAIL     PERF_CF_PWR_MODEL_DLPPM_1X_CORE_RAIL;

typedef struct PERF_CF_PWR_MODEL_DLPPM_1X_FBVDD_RAIL    PERF_CF_PWR_MODEL_DLPPM_1X_FBVDD_RAIL;

typedef struct PERF_CF_PWR_MODEL_DLPPM_1X               PERF_CF_PWR_MODEL_DLPPM_1X;

/* ------------------------ Macros and Defines ------------------------------ */
 /*!
 * @brief   List of ovl. descriptors required by PERF_CF DLPPM_1X.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL_DLPPM_1X)
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_MODEL_DLPPM_1X                          \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPwrEquation)                      \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLeakage)                          \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVoltApi)                          \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libFxpBasic)                         \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libFxpExtended)                      \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, thermLibSensor2X)                    \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libi2c)                              \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLw64)                             \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLw64Umul)                         \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, nneInferenceClient)                  \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, dlppm1xRuntimeScratch)

#else
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_MODEL_DLPPM_1X                          \
        OSTASK_OVL_DESC_ILWALID

#endif

 /*!
 * @brief   List of ovl. descriptors required by PERF_CF DLPPM_1X at construct time.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL_DLPPM_1X)
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_PWR_MODEL_CONSTRUCT_DLPPM_1X            \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libDevInfo)

#else
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_PWR_MODEL_CONSTRUCT_DLPPM_1X            \
    OSTASK_OVL_DESC_ILWALID

#endif

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Structure holding all rail information for DLPPM_1X.
 */
struct PERF_CF_PWR_MODEL_DLPPM_1X_RAIL
{
    /*!
     * Mask of independent clock domains that reside on this rail.
     */
    BOARDOBJGRPMASK_E32                                   independentClkDomMask;

    /*!
     * PMU-only voltage sensing structure used as a scratch to ilwoke voltage
     * sensing APIs.
     */
    VOLT_RAIL_SENSED_VOLTAGE_DATA                         voltData;

    /*!
     * Index to VOLT_RAIL table of the rail that the clock is for.
     */
    LwBoardObjIdx                                         voltRailIdx;

    /*!
     * Index into CLK_DOMAINS of the clock associated with this rail.
     */
    LwBoardObjIdx                                         clkDomIdx;

    /*!
     * Index into PERF_CF_TOPOLOGYS of the topology sensing the clock frequency
     * of @ref clkDomIdx.
     */
    LwBoardObjIdx                                         clkDomTopIdx;

    /*!
     * Index into PERF_CF_TOPOLOGY table of the topology providing the
     * utiliazation percentage of the clkDomIdx CLK_DOMAIN.
     */
    LwBoardObjIdx                                         clkDomUtilTopIdx;

    /*!
     * Index into PWR_CHANNEL table of the power channel measuring the input power
     * of the rail.
     */
    LwBoardObjIdx                                         inPwrChIdx;

    /*!
     * Index into PWR_CHANNEL table of the power channel measuring the output power
     * of the rail.
     */
    LwBoardObjIdx                                         outPwrChIdx;

    /*!
     * Index into PWR_CHRELATIONSHIP table of the VR efficiency equation for this rail
     * from input to output.
     */
    LwBoardObjIdx                                         vrEfficiencyChRelIdxInToOut;

    /*!
     * Index into PWR_CHRELATIONSHIP table of the VR efficiency equation for this rail.
     * from output to input.
     */
    LwBoardObjIdx                                         vrEfficiencyChRelIdxOutToIn;

    /*!
     * Mode that voltage should be sensed with.
     */
    LW2080_CTRL_VOLT_VOLT_RAIL_SENSED_VOLTAGE_MODE_ENUM   voltMode;
};

/*!
 * Structure holding all core rail information for DLPPM_1X.
 */
struct PERF_CF_PWR_MODEL_DLPPM_1X_CORE_RAIL
{
    /*!
     * Set of rails belonging to the core.
     */
    PERF_CF_PWR_MODEL_DLPPM_1X_RAIL   *pRails;

    /*!
     * Number of rails in the core. The first @ref numRails are valid in
     * @ref coreRail.
     */
    LwU8                               numRails;
};

/*!
 * Structure holding all FBVDD rail information for DLPPM_1X.
 */
struct PERF_CF_PWR_MODEL_DLPPM_1X_FBVDD_RAIL
{
    /*!
     * Super-class.
     */
    PERF_CF_PWR_MODEL_DLPPM_1X_RAIL   super;
    
    /*!
     * Name of the rail.
     */
    LwU8                              voltRailName;
};

/*!
 * PERF_CF_PWR_MODEL child class providing attributes of DLPPM 1x PERF_CF PwrModel
 */
struct PERF_CF_PWR_MODEL_DLPPM_1X
{
    /*!
     * PERF_CF_PWR_MODEL parent class.
     * Must be first element of the structure!
     */
    PERF_CF_PWR_MODEL                                          super;

    /*!
     * Core rail state.
     */
    PERF_CF_PWR_MODEL_DLPPM_1X_CORE_RAIL                       coreRail;

    /*!
     * FBVDD rail state.
     */
    PERF_CF_PWR_MODEL_DLPPM_1X_FBVDD_RAIL                      fbRail;

    /*!
     * Chip information used by DLPPM_1X.
     */
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_DLPPM_1X_CHIP_CONFIG    chipConfig;

    /*!
     * @copydoc LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_STATUS_DLPPM_1X::guardRailsStatus
     */
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAILS_STATUS guardRailsStatus;

    /*!
     * @copydoc LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_INFO_DLPPM_1X::coreRailPwrFitTolerance
     */
    LwUFXP20_12                                                coreRailPwrFitTolerance;

    /*!
     * Index into NNE_DESCS of the DL perf and power estimator.
     */
    LwBoardObjIdx                                              nneDescIdx;

    /*!
     * Index into PERF_CF sensor table for PERF_CF_SENSOR representing
     * all PM's.
     */
    LwBoardObjIdx                                              pmSensorIdx;

   /*!
    * Specifies which specific PM sensors the PERF_CF_PM_SENSOR should sample
    * for this PWR_MODEL.
    */
    BOARDOBJGRPMASK_E1024                                      pmSensorSignalMask;

    /*!
     * @copydoc LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_INFO_DLPPM_1X::dramclkMatchTolerance
     */
    LwUFXP20_12                                                dramclkMatchTolerance;

    /*!
     * Threshold value in between (0,1] that determines how smalll the callwlated
     * perfMs value from gpcclk count can be before the observed data is marked
     * as invalid
     */
    LwUFXP20_12                                              observedPerfMsMinPct;

    /*!
     * @copydoc LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_INFO_DLPPM_1X::correctionBounds
     */
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_DLPPM_1X_CORRECTION_BOUNDS correctionBounds;

    /*!
     * Index into the PWR_CHANNEL table for the small rails.
     *
     * NOTE: If this is set to invalid, DLPPM_1X shall assume that small rails
     *       should be omitted from consideration.
     */
    LwBoardObjIdx                                             smallRailPwrChIdx;

    /*!
     * Index into the PWR_CHANNEL table for the TGP.
     */
    LwBoardObjIdx                                             tgpPwrChIdx;

    /*!
     * Index into the PERF_CF_TOPOLOGY table for the topology to sense PTIMER
     */
    LwBoardObjIdx                                             ptimerTopologyIdx;

    /*!
     * Number of VF points to infer in a batch, during efficiency lwrve search.
     * May be at most @ref LW2080_CTRL_NNE_NNE_DESC_INFERENCE_LOOPS_MAX.
     */
    LwU8                                                      vfInferenceBatchSize;

    /*
    * Boolean on whether to scale GPCCLK counts by the utilPct
    */
    LwBool                                                   bFakeGpcClkCounts;

    /*
     * Index into DLPPM_1X_OBSERVED::pmSensorDiff where the gpcclk counts
     * are located
     */
    LwBoardObjIdx                                            gpcclkPmIndex;


    /*
     * Index into DLPPM_1X_OBSERVED::pmSensorDiff where the ltcclk counts
     * are located
     */
    LwBoardObjIdx                                            xbarclkPmIndex;
};

/*!
 * @copydoc LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_BOUNDS_DLPPM_1X
 */
typedef struct
{
    /*!
     * @copydoc LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_BOUNDS_DLPPM_1X::endpointLo
     */
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *pEndpointLo;

    /*!
     * @copydoc LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_BOUNDS_DLPPM_1X::endpointHi
     */
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *pEndpointHi;

    /*!
     * @copydoc LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_BOUNDS_DLPPM_1X::bApply
     */
    LwBool  bGuardRailsApply;
} PERF_CF_PWR_MODEL_SCALE_BOUNDS_DLPPM_1X;

/*!
 * @copydoc LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_DLPPM_1X
 */
typedef struct
{
    /*!
     * Super member.
     */
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS super;

    /*!
     * @copydoc LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_DLPPM_1X::bounds
     */
    PERF_CF_PWR_MODEL_SCALE_BOUNDS_DLPPM_1X bounds;

    /*!
     * @copydoc LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_DLPPM_1X::profiling
     */
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_SCALE_PROFILING *pProfiling;
} PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_DLPPM_1X;

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet                             (perfCfPwrModelGrpIfaceModel10ObjSetImpl_DLPPM_1X)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfPwrModelGrpIfaceModel10ObjSetImpl_DLPPM_1X");
BoardObjIfaceModel10GetStatus                                 (perfCfPwrModelIfaceModel10GetStatus_DLPPM_1X)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfPwrModelIfaceModel10GetStatus_DLPPM_1X");

// PERF_CF Pwr Model related interfaces.
PerfCfPwrModelSampleDataInit                  (perfCfPwrModelSampleDataInit_DLPPM_1X)
    GCC_ATTRIB_SECTION("imem_perfCfLoad", "perfCfPwrModelSampleDataInit_DLPPM_1X");
PerfCfPwrModelLoad                            (perfCfPwrModelLoad_DLPPM_1X)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfPwrModelLoad_DLPPM_1X");

PerfCfPwrModelObserveMetrics                  (perfCfPwrModelObserveMetrics_DLPPM_1X)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "perfCfPwrModelObserveMetrics_DLPPM_1X");
PerfCfPwrModelScale                           (perfCfPwrModelScale_DLPPM_1X)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "perfCfPwrModelScale_DLPPM_1X");
PerfCfPwrModelScalePrimaryClkRange            (perfCfPwrModelScalePrimaryClkRange_DLPPM_1X)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "perfCfPwrModelScalePrimaryClkRange_DLPPM_1X");
PerfCfPwrModelObservedMetricsInit             (perfCfPwrModelObservedMetricsInit_DLPPM_1X)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "perfCfPwrModelObservedMetricsInit_DLPPM_1X");
PerfCfPwrModelScaleTypeParamsInit             (perfCfPwrModelScaleTypeParamsInit_DLPPM_1X)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "perfCfPwrModelScaleTypeParamsInit_DLPPM_1X");

/* ------------------------ Include Derived Types --------------------------- */
/* ------------------------ Inline Functions -------------------------------- */
/*!
 * @brief   Begins the profiling for a given region of code in
 *          @ref perfCfPwrModelObserveMetrics_DLPPM_1X
 *
 * @details This works by subtracting the current time from zero, giving us
 *          NEGATIVE(lwrrentTime). The "end" function can then add the
 *          then-current time to the value to get the elapsed time, i.e.,
 *          endTime - starTime == (-startTime) + endTime.
 *
 * @param[out]  pProfiling      Pointer to
 *                              @ref LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_SCALE_PROFILING
 *                              structure in which to start profiling. Must be
 *                              non-NULL.
 * @param[in]   region          Region to start profiling.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
perfCfPwrModelDlppm1xObserveMetricsProfileRegionBegin
(
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING *pProfiling,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION_ENUM region
)
{
    FLCN_STATUS status;
    FLCN_TIMESTAMP timensLwrr;
    LwU64 timensResult;

    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL_DLPPM_1X_PROFILING))
    {
        goto perfCfPwrModelDlppm1xObserveMetricsProfileRegionBegin_exit;
    }

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pProfiling != NULL) &&
        (region < LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION__COUNT),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfPwrModelDlppm1xObserveMetricsProfileRegionBegin_exit);

    osPTimerTimeNsLwrrentGet(&timensLwrr);

    lw64Sub_MOD(&timensResult, &(LwU64){0U}, &timensLwrr.data);
    LwU64_ALIGN32_PACK(
        &pProfiling->profiledTimesns[region], &timensResult);

perfCfPwrModelDlppm1xObserveMetricsProfileRegionBegin_exit:
    lwosNOP();
}

/*!
 * @brief   Ends the profiling for a given region of code in
 *          @ref perfCfPwrModelObserveMetrics_DLPPM_1X, storing the elapsed time.
 *
 * @param[out]  pProfiling      Pointer to
 *                              @ref LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_SCALE_PROFILING
 *                              structure in which to end profiling.Must be
 *                              non-NULL.
 * @param[in]   region          Region for which to end profiling.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
perfCfPwrModelDlppm1xObserveMetricsProfileRegionEnd
(
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING *pProfiling,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION_ENUM region
)
{
    FLCN_STATUS status;
    FLCN_TIMESTAMP timensLwrr;
    LwU64 timensResult;

    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL_DLPPM_1X_PROFILING))
    {
        goto perfCfPwrModelDlppm1xObserveMetricsProfileRegionEnd_exit;
    }

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pProfiling != NULL) &&
        (region < LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION__COUNT),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfPwrModelDlppm1xObserveMetricsProfileRegionEnd_exit);

    osPTimerTimeNsLwrrentGet(&timensLwrr);

    LwU64_ALIGN32_UNPACK(
        &timensResult, &pProfiling->profiledTimesns[region]);
    lw64Add_MOD(&timensResult, &timensResult, &timensLwrr.data);
    LwU64_ALIGN32_PACK(
        &pProfiling->profiledTimesns[region], &timensResult);

perfCfPwrModelDlppm1xObserveMetricsProfileRegionEnd_exit:
    lwosNOP();
}

/*!
 * @brief   Begins the profiling for a given region of code in
 *          @ref perfCfPwrModelScale_DLPPM_1X
 *
 * @details This works by subtracting the current time from zero, giving us
 *          NEGATIVE(lwrrentTime). The "end" function can then add the
 *          then-current time to the value to get the elapsed time, i.e.,
 *          endTime - starTime == (-startTime) + endTime.
 *
 * @param[out]  pProfiling      Pointer to
 *                              @ref LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_SCALE_PROFILING
 *                              structure in which to start profiling. *May* be
 *                              NULL so callers of @ref perfCfPwrModelScale_DLPPM_1X
 *                              can skip profiling.
 * @param[in]   region          Region to start profiling.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
perfCfPwrModelDlppm1xScaleProfileRegionBegin
(
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_SCALE_PROFILING *pProfiling,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_DLPPM_1X_SCALE_PROFILING_REGION_ENUM region
)
{
    FLCN_STATUS status;
    FLCN_TIMESTAMP timensLwrr;
    LwU64 timensResult;

    //
    // If the feature is not enabled or the pointer is NULL exit early. The
    // NULL pointer condition allows for scale profiling to be optionally
    // specified by the caller.
    //
    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL_DLPPM_1X_PROFILING) ||
        (pProfiling == NULL))
    {
        goto perfCfPwrModelDlppm1xScaleProfileRegionBegin_exit;
    }

    // Check that the region is valid.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (region < LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_DLPPM_1X_SCALE_PROFILING_REGION__COUNT),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfPwrModelDlppm1xScaleProfileRegionBegin_exit);

    osPTimerTimeNsLwrrentGet(&timensLwrr);

    lw64Sub_MOD(&timensResult, &(LwU64){0U}, &timensLwrr.data);
    LwU64_ALIGN32_PACK(
        &pProfiling->profiledTimesns[region], &timensResult);

perfCfPwrModelDlppm1xScaleProfileRegionBegin_exit:
    lwosNOP();
}

/*!
 * @brief   Ends the profiling for a given region of code in
 *          @ref perfCfPwrModelScale_DLPPM_1X, storing the elapsed time.
 *
 * @param[out]  pProfiling      Pointer to
 *                              @ref LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_SCALE_PROFILING
 *                              structure in which to start profiling. *May* be
 *                              NULL so callers of @ref perfCfPwrModelScale_DLPPM_1X
 *                              can skip profiling.
 * @param[in]   region          Region for which to end profiling.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
perfCfPwrModelDlppm1xScaleProfileRegionEnd
(
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_SCALE_PROFILING *pProfiling,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_DLPPM_1X_SCALE_PROFILING_REGION_ENUM region
)
{
    FLCN_STATUS status;
    FLCN_TIMESTAMP timensLwrr;
    LwU64 timensResult;

    //
    // If the feature is not enabled or the pointer is NULL exit early. The
    // NULL pointer condition allows for scale profiling to be optionally
    // specified by the caller.
    //
    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL_DLPPM_1X_PROFILING) ||
        (pProfiling == NULL))
    {
        goto perfCfPwrModelDlppm1xScaleProfileRegionEnd_exit;
    }

    // Check that the region is valid.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (region < LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_DLPPM_1X_SCALE_PROFILING_REGION__COUNT),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfPwrModelDlppm1xScaleProfileRegionEnd_exit);

    osPTimerTimeNsLwrrentGet(&timensLwrr);

    LwU64_ALIGN32_UNPACK(
        &timensResult, &pProfiling->profiledTimesns[region]);
    lw64Add_MOD(&timensResult, &timensResult, &timensLwrr.data);
    LwU64_ALIGN32_PACK(
        &pProfiling->profiledTimesns[region], &timensResult);

perfCfPwrModelDlppm1xScaleProfileRegionEnd_exit:
    lwosNOP();
}

/*!
 * @brief   Copies the @ref NNE_DESC inference profiling over to the profiling
 *          structure
 *
 * @param[out]  pProfiling          Pointer to
 *                                  @ref LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_SCALE_PROFILING
 *                                  into which to copy. *May* be NULL so callers
 *                                  of @ref perfCfPwrModelScale_DLPPM_1X can
 *                                  skip profiling.
 * @param[in]   pInferenceProfiling Pointer to
 *                                  @ref LW2080_CTRL_NNE_NNE_DESC_INFERENCE_PROFILING
 *                                  from which to copy. Must be non-NULL
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
perfCfPwrModelDlppm1xScaleProfileNneDescInferenceProfilingSet
(
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_SCALE_PROFILING *pProfiling,
    const LW2080_CTRL_NNE_NNE_DESC_INFERENCE_PROFILING *pInferenceProfiling
)
{
    FLCN_STATUS status;

    //
    // If the feature is not enabled or the pointer is NULL exit early. The
    // NULL pointer condition allows for scale profiling to be optionally
    // specified by the caller.
    //
    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL_DLPPM_1X_PROFILING) ||
        (pProfiling == NULL))
    {
        goto perfCfPwrModelDlppm1xScaleProfileNneDescInferenceProfilingSet_exit;
    }

    // Check that the inference profiling pointer is valid
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pInferenceProfiling != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfPwrModelDlppm1xScaleProfileNneDescInferenceProfilingSet_exit);

    pProfiling->nneDescInferenceProfiling = *pInferenceProfiling;

perfCfPwrModelDlppm1xScaleProfileNneDescInferenceProfilingSet_exit:
    lwosNOP();
}

#endif // PERF_CF_PWR_MODEL_DLPPM_1X_H
