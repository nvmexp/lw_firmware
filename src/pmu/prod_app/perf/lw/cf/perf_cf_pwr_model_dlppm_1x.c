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
 * @file    perf_cf_pwr_model_dlppm_1x.c
 * @copydoc perf_cf_pwr_model_dlppm_1x.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf.h"
#include "perf/cf/perf_cf_pwr_model_dlppm_1x.h"
#include "pmu_objfuse.h"
#include "nne/nne_desc_client.h"
#include "volt/voltrail.h"

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @todo    Move this to @ref lwtypes.h. Adding this to lwtypes.h right now
 *          causes a build glitch in the video ucodes. This then requires
 *          re-signing, which fails the build. To avoid that entirely, simply
 *          add this typedef locally.
 */
typedef LwUFXP64                                                   LwUFXP24_40;

/*!
 * @brief   Structure containing metrics necessary for guard railing across
 *          primary clocks.
 */
typedef struct
{
    /*!
     * @brief   Pointer to perf metrics
     */
     const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_PERF *pPerfMetrics;

    /*!
     * @brief   Pointer to core rail metrics
     */
     const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_RAIL *pCoreRailMetrics;

    /*!
     * @brief   Pointer to FB rail metrics
     */
     const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_RAIL *pFbRailMetrics;
} PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS;

/*!
 * @brief   Maximum number of metrics that can be adjacent to the observed
 *          metrics.
 */
#define PERF_CF_PWR_MODEL_DLPPM_1X_OBSERVED_ADJACENT_MAX \
    2U

/*!
 * @brief   Structure containing information about metrics adjacent to the
 *          observed metrics.
 */
typedef struct
{
    /*!
     * @brief   Indices of the estimated metrics closest in primary clock
     *          frequency to the observed metrics. Entries are sorted in
     *          increasing primary clock order.
     */
    LwU8 observedAdjacent[
        PERF_CF_PWR_MODEL_DLPPM_1X_OBSERVED_ADJACENT_MAX];

    /*!
     * @brief   Number of valid entries in
     *          @ref PERF_CF_PWR_MODEL_DLPPM_1X_OBSERVED_ADJACENT::observedAdjacent
     */
    LwU8 numObservedAdjacent;
} PERF_CF_PWR_MODEL_DLPPM_1X_OBSERVED_ADJACENT;

/* ------------------------ Static Function Prototypes ---------------------- */
/*!
 * @brief A deep copy for PERF_CF_PWR_MODEL_DLPPM_1X_RAIL.
 *
 * @param[OUT] pDest   Destination structure to copy to.
 * @param[IN]  pSrc    Source structure to copy from.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT if @ref pDest or @ref pSrc are NULL.
 * @return FLCN_OK if the dep copy was successful.
 */
static FLCN_STATUS
s_perfCfPwrModelRailCopy_DLPPM_1X(
        PERF_CF_PWR_MODEL_DLPPM_1X_RAIL                    *pDest,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_DLPPM_1X_RAIL   *pSrc)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfPwrModelRailCopy_DLPPM_1X");

/*!
 * @brief A deep copy for PERF_CF_PWR_MODEL_DLPPM_1X_CORE_RAIL.
 *
 * @param[OUT] pDest   Destination structure to copy to.
 * @param[IN]  pSrc    Source structure to copy from.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT if @ref pDest or @ref pSrc are NULL.
 * @return FLCN_OK if the dep copy was successful.
 */
static FLCN_STATUS
s_perfCfPwrModelCoreRailCopy_DLPPM_1X(
        PERF_CF_PWR_MODEL_DLPPM_1X_CORE_RAIL                    *pDest,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_DLPPM_1X_CORE_RAIL   *pSrc)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfPwrModelCoreRailCopy_DLPPM_1X");

/*!
 * @brief A deep copy for PERF_CF_PWR_MODEL_DLPPM_1X_FBVDD_RAIL.
 *
 * @param[OUT] pDest   Destination structure to copy to.
 * @param[IN]  pSrc    Source structure to copy from.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT if @ref pDest or @ref pSrc are NULL.
 * @return FLCN_OK if the dep copy was successful.
 */
static FLCN_STATUS
s_perfCfPwrModelFbvddRailCopy_DLPPM_1X(
        PERF_CF_PWR_MODEL_DLPPM_1X_FBVDD_RAIL                    *pDest,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_DLPPM_1X_FBVDD_RAIL   *pSrc)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfPwrModelFbvddRailCopy_DLPPM_1X");

/*!
 * @brief   Observe performance-based metrics.
 *
 * @details In particular, this function observes the time since the last call
 *          to @ref perfCfPwrModelObserveMetrics_DLPPM_1X. This is used to
 *          indicate for how long the BA PM signals were aclwmulating.
 *
 * @todo    Factor in utilization to the measurement done by this function.
 *
 * @param[in]   pDlppm1x            DLPPM_1X pointer
 * @param[in]   pSampleData         Pointer to the current sample data
 * @param[out]  pDlppm1xObserved    Pointer to observed metrics structure in which
                                    to store perfMs
 */
static FLCN_STATUS
s_perfCfPwrModelObservePerfMetrics_DLPPM_1X(
        const PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
        const PERF_CF_PWR_MODEL_SAMPLE_DATA *pSampleData,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
        LwBool * bWithinBounds)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelObservePerfMetrics_DLPPM_1X");

/*!
 * @brief   Observe Input metrics common to all rails
 *
 * @param[in]   pSampleData     Pointer to current sample data
 * @param[in]   pRail           Pointer to rail structure
 * @param[out]  pRailMetrics    Pointer to structure in which to store observed
 *                              metrics rail
 */
static FLCN_STATUS
s_perfCfPwrModelObserveInputRailMetrics_DLPPM_1X(
        const PERF_CF_PWR_MODEL_SAMPLE_DATA                                 *pSampleData,
        const PERF_CF_PWR_MODEL_DLPPM_1X_RAIL                               *pRail,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_OBSERVED_METRICS_DLPPM_1X_RAIL   *pRailMetrics)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelObserveInputRailMetrics_DLPPM_1X");

/*!
 * @brief   Observe metrics common to all core rails
 *
 * @param[in]   pSampleData     Pointer to current sample data
 * @param[in]   pRail           Pointer to rail structure
 * @param[out]  pRailMetrics    Pointer to structure in which to store observed
 *                              metrics rail
 */
static FLCN_STATUS
s_perfCfPwrModelObserveCoreRailMetrics_DLPPM_1X(
        PERF_CF_PWR_MODEL_SAMPLE_DATA                                       *pSampleData,
        PERF_CF_PWR_MODEL_DLPPM_1X_RAIL                                     *pRail,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_OBSERVED_METRICS_DLPPM_1X_RAIL   *pRailMetrics)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelObserveCoreRailMetrics_DLPPM_1X");

/*!
 * @brief   Observe metrics on the FB rail
 *
 * @param[in]   pSampleData     Pointer to current sample data
 * @param[in]   pRail           Pointer to rail structure for FB rail
 * @param[out]  pRailMetrics    Pointer to structure in which to store observed
 *                              metrics for the FB rail
 */
static FLCN_STATUS
s_perfCfPwrModelObserveFbRailMetrics_DLPPM_1X(
        const PERF_CF_PWR_MODEL_DLPPM_1X                                    *pDlppm1x,
        const PERF_CF_PWR_MODEL_SAMPLE_DATA                                 *pSampleData,
        const PERF_CF_PWR_MODEL_DLPPM_1X_FBVDD_RAIL                         *pRail,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_OBSERVED_METRICS_DLPPM_1X_RAIL   *pRailMetrics)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelObserveFbRailMetrics_DLPPM_1X");

/*!
 * @brief   Determines the corrections that need to be applied to all neural
 *          network outputs.
 *
 * @param[in]       pDlppm1x            DLPPM_1X object pointer
 * @param[in,out]   pDlppm1xObserved    Pointer to observed metrics from which
 *                                      and for which to determine correction.
 * @param[out]      pbWithinBounds      Whether the corrections are all within
 *                                      their bounds.
 *
 * @return  @ref FLCN_OK    Success
 * @return  Others          Errors propagated by callees.
 */
static FLCN_STATUS
s_perfCfPwrModelObserveCorrection_DLPPM_1X(
        PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
        LwBool *pbWithinBounds)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelObserveCorrection_DLPPM_1X");

/*!
 * @brief   Determines if a correction percentage is within specified bounds.
 *
 * @param[in]   pct                 Correction percentage to check.
 * @param[in]   pCorrectionBound    Bounds against which to check pct.
 * @param[out]  pbWithinBound       Whether pct is within the bounds.
 *
 * @return  @ref FLCN_OK    Success
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
s_perfCfPwrModelCorrectionWithinBound_DLPPM_1X(
        LwUFXP20_12 pct,
        const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_DLPPM_1X_CORRECTION_BOUND *pCorrectionBound,
        LwBool *pbWithinBound)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelCorrectionWithinBound_DLPPM_1X");

/*!
 * @brief   Observes a set of coarse grained metrics across the VF lwrve for the
 *          DRAMCLKs that should be considered for this set of observed metrics.
 *
 * @param[in]       pDlppm1x            DLPPM_1X pointer
 * @param[in,out]   pDlppm1xObserved    Observed metrics from which and for which to
 *                                      generate the DRAMCLK metrics.
 * @param[in]       dramclkMinkHz       Minimum DRAMCLK at which estimations
 *                                      can validly be made.
 * @param[in]       pCoreRailsMinkHz    Minimum core rail clocks at which
 *                                      estimations can validly be made.
 *
 * @return  FLCN_OK     Success
 * @return  
 */
static FLCN_STATUS
s_perfCfPwrModelObserveInitialDramclkEstimates_DLPPM_1X(
        PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
        LwU32 dramclkMinkHz,
        const LwU32 *pCoreRailsMinkHz)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelObserveInitialDramclkEstimates_DLPPM_1X");

/*!
 * @brief   Helper to find the unique quantized frequencies immediately adjacent
 *          to the observed.
 *
 * @details This helper will write the frequencies, including the quantized
 *          observed frequency, to @ref pAdjFreqkHz, in ascending order. Note
 *          that only one adjacent quantized frequency and the quantized
 *          observed frequency would be returned if the observed frequency is at
 *          or outside the quantization range of the clock domain.
 *
 * @param[in]   clkDomIdx       Clock domain index.
 * @param[in]   observedFreqkHz Observed frequency in kHz.
 * @param[in]   minFreqkHz      Minimum frequency to allow in the adjacent
 *                              frequencies
 * @param[out]  pAdjFreqkHz     Array to write the adjacent quantized
 *                              frequencies
 *                              to in ascending order. This array is assumed to
 *                              have exactly 3 elements, in kHz.
 * @param[out]  pNumAdjFreq     Number of unique quantized adjacent frequencies
 *                              found.
 * @param[out]  pLwrrDramclkIdx Index for frequency in the pAdjFreqkHz array
 *                              that matches the current frequency.
 *
 * @return  FLCN_ERR_ILWALID_ARGUMENT   If @ref pAdjFreqkHz or @ref pNumAdjFreq
 *                                      are NULL
 * @return  FLCN_ERR_ILWALID_STATE      If @ref clkDomIdx doesn't point to a
 *                                      valid CLK_DOMAIN object.
 * @return  FLCN_OK                     Adjacent quantized frequencies found.
 * @return  Others                      Errors propagated from callees.
 */
static FLCN_STATUS s_perfCfPwrModelFindAdjFreq_DLPPM_1X(
        LwBoardObjIdx clkDomIdx,
        LwU32 observedFreqkHz,
        LwU32 minFreqkHz,
        LwU32 *pAdjFreqkHz,
        LwU8 *pNumAdjFreq,
        LwU8 *pLwrrDramclkIdx)
    GCC_ATTRIB_SECTION("imem_perfCf", "s_perfCfPwrModelFindAdjFreq_DLPPM_1X");

/*!
 * @brief   Gets the primary clock range for a given DRAMCLK
 *
 * @param[in]   pDlppm1x            DLPPM_1X object pointer
 * @param[in]   dramclkkHz          DRAMCLK for which to find primary clock
 *                                  range
 * @param[in]   pCoreRailsMinkHz    Minimum frequency values to use for core
 *                                  rails
 * @param[out]  pVfRangePrimary     Primary clock range for given DRAMCLK
 *
 * @return  @ref FLCN_OK    Success
 */
static FLCN_STATUS s_perfCfPwrModelPrimaryClkRangeForDramclkGet_DLPPM_1X(
        const PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
        LwU32 dramclkkHz,
        const LwU32 *pCoreRailsMinkHz,
        PERF_LIMITS_VF_RANGE *pVfRangePrimary)
    GCC_ATTRIB_SECTION("imem_perfCf", "s_perfCfPwrModelPrimaryClkRangeForDramclkGet_DLPPM_1X");

/*!
 * @brief   Callwlate and store the linear fit for core rail power against
 *          performance for a set of iso-DRAMCLK estiamtes.
 *
 * @param[in,out]   pDramclkEstimates   The set of ISO-dramclk estimates for
 *                                      which to callwalte the linear fit.
 *
 * @return  FLCN_OK Success
 */
static FLCN_STATUS
s_perfCfPwrModelCoreRailPwrFitCallwlate_DLPPM_1X(
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_DRAMCLK_ESTIMATES *pDramclkEstimates)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelCoreRailPwrFitCallwlate_DLPPM_1X");

/*!
 * @brief   Returns the FB voltage used for a given MCLK frequency.
 *
 * @param[in]   pDlppm1x    @ref PERF_CF_PWR_MODEL_DLPPM_1X pointer
 * @param[in]   freqkHz     Frequency observed on the FB rail.
 * @param[out]  pVoltuV     The associated voltage for freqkHz in uV
 *
 * @return  @ref FLCN_OK                Success
 * @return  @ref FLCN_ERR_ILWALID_STATE No @ref CLK_DOMAIN object available for
 *                                      DRAMCLK
 * @return  Others                      Errors propagated from callees.
 */
static FLCN_STATUS
s_perfCfPwrModelFbVoltageLookup_DLPPM_1X(
        const PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
        LwU32 freqkHz,
        LwU32 *pVoltuV)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelFbVoltageLookup_DLPPM_1X");

/*!
 * @brief Helper to add two power tuples' fields together.
 *
 * @param[IN]  pOp0      Pointer to the first power tuple to sum.
 * @param[IN]  pOp1      Pointer to the second power tuple to sum.
 * @param[OUT] pResult   Power tuple to write the sum to.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pOp0, @ref pOp1, or @ref pResult
 *                                     are NULL.
 * @return FLCN_OK                     If the power tuples were successfully summed.
 */
static FLCN_STATUS
s_perfCfPwrModelPowerTupleAdd_DLPPM_1X(
        LW2080_CTRL_PMGR_PWR_TUPLE *pOp0,
        LW2080_CTRL_PMGR_PWR_TUPLE *pOp1,
        LW2080_CTRL_PMGR_PWR_TUPLE *pResult)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelPowerTupleAdd_DLPPM_1X");

/*!
 * @brief   Initializes the @ref PERF_CF_TOPOLOGYS_STATUS for DLPPM
 *
 * @param[in]   pPwrModelDlppm1x    Power model for which to initialize status
 * @param[out]  pPerfTopoStatus     Status structure to initialize
 *
 * @return  FLCN_OK                     Status structure initialized
 * @return  FLCN_ERR_ILWALID_ARGUMENT   Status structure pointer is NULL
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xPerfTopoStatusInit(
        const PERF_CF_PWR_MODEL_DLPPM_1X *pPwrModelDlppm1x,
        PERF_CF_TOPOLOGYS_STATUS *pPerfTopoStatus)
    GCC_ATTRIB_SECTION("imem_perfCfLoad", "s_perfCfPwrModelDlppm1xPerfTopoStatusInit");

/*!
 * @brief   Initializes the @ref PWR_CHANNELS_STATUS for DLPPM
 *
 * @param[in]   pPwrModelDlppm1x    Power model for which to initialize status
 * @param[out]  pPwrChannelsStatus  Status structure to initialize
 *
 * @return  FLCN_OK                     Status structure initialized
 * @return  FLCN_ERR_ILWALID_ARGUMENT   Status structure pointer is NULL
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xPwrChannelsStatusInit(
        const PERF_CF_PWR_MODEL_DLPPM_1X *pPwrModelDlppm1x,
        PWR_CHANNELS_STATUS *pPwrChannelsStatus)
    GCC_ATTRIB_SECTION("imem_perfCfLoad", "s_perfCfPwrModelDlppm1xPwrChannelsStatusInit");

/*!
 * @brief   Initializes the @ref PWR_CHANNELS_STATUS for a particular
 *          @ref LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_DLPPM_1X_CORE_RAIL
 *
 * @param[in]   pPwrModelDlppm1x    Power model for which to initialize status
 * @param[out]  pPwrChannelsStatus  Status structure to initialize
 *
 * @return  FLCN_OK                     Status structure initialized
 * @return  FLCN_ERR_ILWALID_ARGUMENT   Status structure pointer is NULL
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xPwrChannelsStatusRailInit(
        const PERF_CF_PWR_MODEL_DLPPM_1X *pPwrModelDlppm1x,
        const PERF_CF_PWR_MODEL_DLPPM_1X_RAIL *pRail,
        PWR_CHANNELS_STATUS *pPwrChannelsStatus)
    GCC_ATTRIB_SECTION("imem_perfCfLoad", "s_perfCfPwrModelDlppm1xPwrChannelsStatusRailInit");

/*!
 * @brief   Initializes the @ref PERF_CF_PM_SENSORS_STATUS for DLPPM
 *
 * @param[in]   pPwrModelDlppm1x    Power model for which to initialize status
 * @param[out]  pPmSensorsStatus    Status structure to initialize
 *
 * @return  FLCN_OK                     Status structure initialized
 * @return  FLCN_ERR_ILWALID_ARGUMENT   Status structure pointer is NULL
 * @return  Others                      Errors propagated from callees
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xPmSensorsStatusInit(
        PERF_CF_PWR_MODEL_DLPPM_1X *pPwrModelDlppm1x,
        PERF_CF_PM_SENSORS_STATUS *pPmSensorsStatus)
    GCC_ATTRIB_SECTION("imem_perfCfLoad", "s_perfCfPwrModelDlppm1xPmSensorsStatusInit");

/*!
 * @brief   Helper to set all the static (inference-loop independent) inputs.
 *
 * @param[IN]  pDlppm1x           DLPPM_1X power model pointer.
 * @param[IN]  pDlppm1xObserved   DLPPM_1X observed metrics.
 * @param[OUT] pInference         NNE inference request structure to populate.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pDlppm1x, @ref pObservedMetrics,
 *                                     or @ref pInference are NULL.
 * @return FLCN_OK                     If @ref pInference was successfully populated
 *                                     with the static inputs.
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneStaticInputsSet(
        PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
        NNE_DESC_INFERENCE *pInference)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xNneStaticInputsSet");

/*!
 * @brief   Helper to set all the inference-loop dependent inputs.
 *
 * @param[IN]  pDlppm1x             DLPPM_1X power model pointer.
 * @param[IN]  pDlppm1xObserved     DLPPM_1X observed metrics.
 * @param[IN]  numMetricsInputs     Number of elements in @ref pMetricsInputs.
 * @param[IN]  ppEstimatedMetrics   Array of estimated metrics.
 * @param[OUT] pInference           NNE inference request structure to populate.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pDlppm1x, @ref pMetricsInputs, or
 *                                     @ref pInference are NULL.
 * @return FLCN_OK                     If @ref pInference was successfully populated
 *                                     with the dynamic inputs.
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneLoopInputsSet(
        PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
        LwLength numMetricsInputs, LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS **ppEstimatedMetrics,
        NNE_DESC_INFERENCE *pInference)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xNneLoopInputsSet");

/*!
 * @brief   Helper to read all of an NNE inference request's outputs.
 *
 * @param[IN]  pDlppm1x             DLPPM_1X power model pointer.
 * @param[in]  pDlppm1xBounds       Bounding parameters
 * @param[in]  pDlppm1xObserved     Observed metrics to reference when guard
 *                                  railing.
 * @param[IN]  pInference           NNE inference request structure to retrieve outputs from.
 * @param[IN]  numMetricsInputs     Number of metrics scaled points that were requested.
 * @param[OUT] ppEstimatedMetrics   Array of pointers to pointers of estimated metrics to populate.
 * @param[in]   pProfiling          Pointer to profiling structure.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pDlppm1x, @ref pInference, or @ref ppEstimatedMetrics
 *                                     is NULL.
 * @return FLCN_OK                     If the inference outputs were successfully parsed.
 * @return Others                      Errors propagated from callees.
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneOutputsParseAndGuardRail(
        PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x, NNE_DESC_INFERENCE *pInference,
        const PERF_CF_PWR_MODEL_SCALE_BOUNDS_DLPPM_1X *pDlppm1xBounds,
        const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
        LwLength numMetricsInputs, LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS **ppEstimatedMetrics,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_SCALE_PROFILING *pProfiling)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xNneOutputsParseAndGuardRail");

/*!
 * @brief   Propagates the pure NNE output values to additional fields in each
 *          @ref LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X structure.
 *
 * @param[in]       pDlppm1x            DLPPM_1X object pointer.
 * @param[in]       pDlppm1xObserved    Observed metrics (for reference while
 *                                      deriving some metrics)
 * @param[in]       numMetricsInputs    Number of metrics scaled points that
 *                                      were requested.
 * @param[in,out]   ppEstimatedMetrics  Array of pointers to pointers of
 *                                      estimated
 *                                      metrics to populate.
 *
 * @return  FLCN_OK Success
 * @return  Others  Errors propagated from callees.
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneOutputsPropagate(
        PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
        const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
        LwLength numMetricsInputs,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS **ppEstimatedMetrics)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xNneOutputsPropagate");

/*!
 * @brief   Propagate all NNE power metrics to additional metrics fields.
 *
 * @param[in]       pDlppm1x            DLPPM_1X object pointer
 * @param[in,out]   pDlppm1xEstimated   Estimated metrics from/to which to
 *                                      propagate
 *
 * @return  FLCN_OK Success
 * @return  Others  Errors propagated from callees
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneEstimatedPowerMetricsPropagate(
        PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *pDlppm1xEstimated)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xNneEstimatedPowerMetricsPropagate");

/*!
 * @brief   Helper to retrieve a rail's NNE-estimated dynamic output power.
 *
 * @param[IN]   pRail               Pointer to structure holding all rail information relevant to DLPPM_1X.
 * @param[in]   railIdx             Index of this rail among all core rails.
 * @param[IN]   pOutputs            Array of outputs returned from NNE.
 * @param[IN]   numOutputs          Number of elements in @ref pOutputs.
 * @param[OUT]  pRailMetrics        Rail metrics structure that the estimated power
 *                                  metrics should be put into.
 * @param[in]   pDlppm1xObserved    Observed metrics for this estimation.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pOutputs or @ref pPwrmW is NULL.
 * @return FLCN_ERR_ILWALID_STATE      If output power was not estimated for @ref outPwrChIdx.
 * @return FLCN_OK                     If the output power was successfully returned.
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneEstimatedPowerDynNormParse(
        PERF_CF_PWR_MODEL_DLPPM_1X_RAIL *pRail,
        LwU8 railIdx,
        LW2080_CTRL_NNE_NNE_DESC_OUTPUT *pOutputs, LwU8 numOutputs,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_RAIL *pRailMetrics,
        const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xNneEstimatedPowerDynNormParse");

/*!
 * @brief   Propagates the
 *          @ref LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_RAIL::pwrOutDynamicNormalizedmW
 *          power value to additional metrics fields.
 *
 * @param[in]       pRail           Pointer to structure holding all rail
 *                                  information relevant to DLPPM_1X.
 * @param[in,out]   pRailMetrics    Rail metrics structure from which to
 *                                  propagate the dynamic normalized power.
 *
 * @return  FLCN_OK                     If the output power was successfully
 *                                      returned.
 * @return  FLCN_ERR_ILWALID_ARGUMENT   If pRail->voltRailIdx is invalid
 * @return  FLCN_ERR_ILWALID_STATE      Volt rail's dynamic power equation or
 *                                      overflow in  fixed point math.
 * @return  Others                      Errors propagated from callees.
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneEstimatedPowerDynNormPropagate(
        PERF_CF_PWR_MODEL_DLPPM_1X_RAIL *pRail,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_RAIL *pRailMetrics)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xNneEstimatedPowerDynNormPropagate");

/*!
 * @brief   Helper to retrieve FBVDD's NNE-estimated total output power.
 *
 * @param[IN]  pRail                Pointer to structure holding all FBVDD rail information
 *                                  relevant to DLPPM_1X.
 * @param[IN]   pOutputs            Array of outputs returned from NNE.
 * @param[IN]   numOutputs          Number of elements in @ref pOutputs.
 * @param[OUT]  pRailMetrics        FBVDD rail metrics structure that the estimated power
 *                                  metrics should be put into.
 * @param[in]   pDlppm1xObserved    Observed metrics for this estimation.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pOutputs or @ref pPwrmW is NULL.
 * @return FLCN_ERR_ILWALID_STATE      If output power was not estimated for @ref outPwrChIdx.
 * @return FLCN_OK                     If the output power was successfully returned.
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneEstimatedFbvddPowerParse(
        PERF_CF_PWR_MODEL_DLPPM_1X_FBVDD_RAIL *pRail,
        LW2080_CTRL_NNE_NNE_DESC_OUTPUT *pOutputs, LwU8 numOutputs,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_RAIL *pRailMetrics,
        const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xNneEstimatedFbvddPowerParse");

/*!
 * @brief   Propagates the
 *          @ref LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_RAIL::pwrOuTuple.pwrmW
 *          power value to additional metrics fields.
 *
 * @param[in]       pRail           Pointer to structure holding all rail
 *                                  information relevant to DLPPM_1X.
 * @param[in,out]   pRailMetrics    Rail metrics structure from which to propagate
 *                                  the power
 *
 * @return FLCN_OK  Success
 * @return Others   Errors propagated from callees.
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneEstimatedFbvddPowerPropagate(
        PERF_CF_PWR_MODEL_DLPPM_1X_FBVDD_RAIL *pRail,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_RAIL *pRailMetrics)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xNneEstimatedFbvddPowerPropagate");

/*!
 * @brief   Helper to colwert VR-input power to VR-output power.
 *
 * @param[IN]     pRail          Pointer to structure holding all rail information relevant to DLPPM_1X.
 * @param[IN/OUT] pRailMetrics   Pointer to rail metrics structure that output power
 *                               should be read from and the colwerted input power should
 *                               be written to.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pRail and @ref pRailMetrics are NULL.
 * @return FLCN_OK                     If the output power in @ref pRailMetrics was
 *                                     successfully colwerted to input power.
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xOutPwrToInPwrMetrics(
        PERF_CF_PWR_MODEL_DLPPM_1X_RAIL *pRail,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_RAIL *pRailMetrics)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xOutPwrToInPwrMetrics");

/*!
 * @brief   Helper to retrieve a rail's NNE-estimated perf.
 *
 * @param[in]   pOutputs                Array of outputs returned from NNE.
 * @param[in]   numOutputs              Number of elements in @ref pOutputs.
 * @param[out]  pPerfMetrics            Estimated perf metrics into which to
 *                                      parse
 * @param[in]   pDlppm1xObserved        Observed metrics for this estimation.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pOutputs or @ref pPerfMetrics is NULL.
 * @return FLCN_ERR_ILWALID_STATE      If perf was not estimated.
 * @return FLCN_OK                     If the estimated perf was successfully returned.
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneEstimatedPerfParse(
        LW2080_CTRL_NNE_NNE_DESC_OUTPUT *pOutputs,
        LwU8 numOutputs,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_PERF *pPerfMetrics,
        const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xNneEstimatedPerfParse");

/*!
 * @brief   Propagates the
 *          @ref LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_PERF::perfRatio
 *          performance value to additional metrics fields.
 *
 * @param[in,out]   pPerfMetrics            Pointer to estimated perf metrics structure
 *                                          from/to which to propagate.
 * @param[in]       pObservedPerfMetrics    Observed metrics structure to
 *                                          reference for propagation.
 *
 * @return FLCN_OK  Success
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneEstimatedPerfPropagate(
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_PERF *pPerfMetrics,
        const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_PERF *pObservedPerfMetrics)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xNneEstimatedPerfPropagate");

/*!
 * @brief   Helper to solve for current, given power and voltage.
 *
 * @param[IN]  pwrmW     Power, in mW.
 * @param[IN]  voltuV    Voltage, in uV.
 * @param[OUT] pLwrrmA   Computed current, in mA.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If pLwrrmW is NULL.
 * @return FLCN_ERR_ILWALID_STATE      If computed current in mA is greater than LW_U32_MAX.
 * @return FLCN_OK                     If current was successfully computed.
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xLwrrentSolve(
        LwU32 pwrmW, LwU32 voltuV, LwU32 *pLwrrmA)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xLwrrentSolve");

/*!
 * @brief   Normalizes the metrics as required for inputs into NNE inferences.
 *
 * @param[in]       pDlppm1x            DLPPM_1X pointer
 * @param[in,out]   pDlppm1xObserved    Observed metrics to normalize
 * @param[out]      bWithinBounds       Status where LW_TRUE implies normalization
 *                                      was applied to all inputs and LW_FALSE
 *                                      implies the inputs are invalid
 *
 * @return  FLNC_OK     Success
 * @return  Others      Errors propagated from callees
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xObservedMetricsNormalize(
        PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
        LwBool *bWithinBounds)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xObservedMetricsNormalize");

/*!
 * @brief   Gets the rail metrics structure for a given @ref NNE_VAR_FREQ's ID
 *          from the overall observed metrics structure.
 *
 * @param[in]   pDlppm1x            DLPPM_1X pointer
 * @param[in]   pId                 Frequency ID for which to retrieve rail
 *                                  metrics
 * @param[in]   pDlppm1xObserved    Observed metrics from which to extract
 *                                  pointer
 * @param[out]  ppRailMetrics       Pointer to pointer to rail metrics structure
 *                                  for given ID
 *
 * @return  FLCN_OK                     Success
 * @return  FLCN_ERR_OBJECT_NOT_FOUND   No match found for given ID
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xRailGetMetricsForFreqId(
        PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
        const LW2080_CTRL_NNE_VAR_ID_FREQ *pId,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_RAIL **ppRailMetrics)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xRailGetMetricsForFreqId");

/*!
 * @brief   Extracts values from inference inputs back into an observed metrics
 *          structure.
 *
 * @param[in]   pDlppm1x            DLPPM_1X pointer
 * @param[out]  pDlppm1xObserved    Observed metrics into which to write input
 * @param[in]   pInputs             Array of inputs from which to extract
 * @param[in]   numInputs           Number of inputs
 *
 * @return  FLCN_OK     Success
 *
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneInputsExtract(
        PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
        const LW2080_CTRL_NNE_NNE_VAR_INPUT *pInputs,
        LwLength numInputs)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xNneInputsExtract");

/*!
 * @brief   Extracts the FREQ value from the inference input back into the
 *          observed metrics.
 *
 * @param[in]   pDlppm1x            DLPPM_1X pointer
 * @param[out]  pDlppm1xObserved    Observed metrics into which to write input
 * @param[in]   pInput              Input from which to extract
 *
 * @return  FLCN_OK Successs.
 * @return  Others  Errors propagated from callees.
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneInputExtract_FREQ(
        PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
        const LW2080_CTRL_NNE_NNE_VAR_INPUT_FREQ *pInput)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xNneInputExtract_FREQ");

/*!
 * @brief   Extracts the PM value from the inference input back into the
 *          observed metrics.
 *
 * @param[in]   pDlppm1x            DLPPM_1X pointer
 * @param[out]  pDlppm1xObserved    Observed metrics into which to write input
 * @param[in]   pInput              Input from which to extract
 *
 * @return  FLCN_OK                 Successs.
 * @return  FLCN_ERR_NOT_SUPPORTED  Invalid baIdx provided in input.
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneInputExtract_PM(
        PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
        const LW2080_CTRL_NNE_NNE_VAR_INPUT_PM *pInput)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xNneInputExtract_PM");

/*!
 * @brief   Extracts the CHIP_CONFIG value from the inference input back into
 *          the observed metrics.
 *
 * @param[in]   pDlppm1x            DLPPM_1X pointer
 * @param[out]  pDlppm1xObserved    Observed metrics into which to write input
 * @param[in]   pInput              Input from which to extract
 *
 * @return  FLCN_OK                 Successs.
 * @return  FLCN_ERR_NOT_SUPPORTED  Invalid baIdx provided in input.
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneInputExtract_CHIP_CONFIG(
        PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
        const LW2080_CTRL_NNE_NNE_VAR_INPUT_CHIP_CONFIG *pInput)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xNneInputExtract_CHIP_CONFIG");

/*!
 * @brief   Extracts the POWER_DN value from the inference input back into
 *          the observed metrics.
 *
 * @param[in]   pDlppm1x            DLPPM_1X pointer
 * @param[out]  pDlppm1xObserved    Observed metrics into which to write input
 * @param[in]   pInput              Input from which to extract
 *
 * @return  FLCN_ERR_NOT_SUPPORTED  Always
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneInputExtract_POWER_DN(
        PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
        const LW2080_CTRL_NNE_NNE_VAR_INPUT_POWER_DN *pInput)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xNneInputExtract_POWER_DN");

/*!
 * @brief   Extracts the POWER_TOTAL value from the inference input back into
 *          the observed metrics.
 *
 * @param[in]   pDlppm1x            DLPPM_1X pointer
 * @param[out]  pDlppm1xObserved    Observed metrics into which to write input
 * @param[in]   pInput              Input from which to extract
 *
 * @return  FLCN_ERR_NOT_SUPPORTED  Always
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneInputExtract_POWER_TOTAL(
        PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
        const LW2080_CTRL_NNE_NNE_VAR_INPUT_POWER_TOTAL *pInput)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xNneInputExtract_POWER_TOTAL");

/*!
 * @brief   Enforce iso-DRAMCLK/cross-primary-clock guard railing policies on
 *          the estimated metrics.
 *
 * @param[in]       pDlppm1x            DLPPM_1X object pointer
 * @param[in]       pDlppm1xBounds      Bounding parameters
 * @param[in]       pDlppm1xObserved    Observed metrics to reference when guard
 *                                      railing.
 * @param[in,out]   ppDlppm1xEstimated  Estimated metrics structures to guard
 *                                      rail.
 * @param[in]       numEstimatedMetrics Number of elements in ppEstimatedMetrics
 *
 * @return  FLCN_OK     Success
 * @return  FLCN_ERR_ILWALID_ARGUMENT   pDlppm1xBounds->super.type is not
 *                                      @ref LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_DLPPM_1X
 *                                      or the DRAMCLK of the estimated metrics
 *                                      does not match the DRAMCLK of any
 *                                      @ref LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED::initialDramclkEstimates
 * @return  Others                      Errors returned by callees.
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xGuardRailPrimaryClk(
        PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
        const PERF_CF_PWR_MODEL_SCALE_BOUNDS_DLPPM_1X *pDlppm1xBounds,
        const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X **ppDlppm1xEstimated,
        LwU8 numEstimatedMetrics)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xGuardRailPrimaryClk");

/*!
 * @brief   Retrieves the index(es) in a set of estimated metrics that are
 *          adjacent to an observed metrics in primary clock frequency.
 *
 * @param[in]   pDlppm1xObserved    Pointer to observed metrics for which to
 *                                  find adjacent estimated metrics
 * @param[out]  pObservedAdjacent   Pointer to structure into which to place
 *                                  indices of adjacent metrics
 * @param[in]   ppDlppm1xEstimated  Set of estimated metrics for which to find
 *                                  the indices adjacent to observed
 * @param[in]   numEstimatedMetrics Number of elements in ppDlppm1xEstimated
 *
 * @return  @ref FLCN_OK    Success
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xObservedAdjacentGet(
        const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
        PERF_CF_PWR_MODEL_DLPPM_1X_OBSERVED_ADJACENT *pObservedAdjacent,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X **ppDlppm1xEstimated,
        LwU8 numEstimatedMetrics)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xObservedAdjacentGet");

/*!
 * @brief   Retrieves the index of the estimated metrics closest to an observed
 *          metrics in primary clock frequency.
 *
 * @param[in]   pDlppm1xObserved    Pointer to observed metrics for which to
 *                                  find closest estimated metrics
 * @param[in]   pObservedAdjacent   Pointer to structure containing indices
 *                                  in ppDlppm1xEstimated adjacent to
 *                                  pDlppm1xObserved
 * @param[in]   ppDlppm1xEstimated  Set of estimated metrics for which to find
 *                                  the closest index
 * @param[out]  pClosestIdx         Contains index in ppDlppm1xEstimated of
 *                                  metrics closest to pDlppm1xObserved in
 *                                  primary clock frequency
 *
 * @return  @ref FLCN_OK    Success
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xObservedAdjacentClosestGet(
    const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
    const PERF_CF_PWR_MODEL_DLPPM_1X_OBSERVED_ADJACENT *pObservedAdjacent,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X **ppDlppm1xEstimated,
    LwU8 *pClosestIdx);

/*!
 * @brief   Guard rail a given DLPPM_1X metrics structure across changing
 *          primary clocks, given various parameters.
 *
 * @param[in,out]   pDlppm1x            DLPPM_1X object pointer. Guard rail
 *                                      status is updated after guard railing is
 *                                      applied.
 * @param[in,out]   pLwrr               Current estimated metrics to guard rail
 * @param[in]       pAdjacent           Metrics for next-lowest or next-highest
 *                                      primary clock against which to guard
 *                                      rail
 * @param[in]       pDlppm1xBounds      DLPPM_1X bounding parameters
 * @param[in]       pCoreRailFit        Pointer to fit structure for core rail
 *                                      power
 * @param[in]       pDlppm1xObserved    Pointer to observed metrics structure.
 *                                      Used as a reference point when some
 *                                      metrics may have to be recallwlated
 *                                      (e.g., pLwrr->perfMetrics.perfRatio)
 *
 * @return  FLCN_OK Success
 * @return  Others  Errors propagated by callees.
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xGuardRailMetricsStructurePrimaryClk(
        PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *pLwrr,
        const PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS *pAdjacent,
        const PERF_CF_PWR_MODEL_SCALE_BOUNDS_DLPPM_1X *pDlppm1xBounds,
        const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_CORE_RAIL_POWER_FIT *pCoreRailFit,
        const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xGuardRailMetricsStructurePrimaryClk");

/*!
 * @brief   Ensure that a given estimated performance respects the line between
 *          two points, either as a minimum or a maximum.
 *
 * @param[in,out]   pDlppm1x            DLPPM_1X object pointer
 * @param[in,out]   pDlppm1xEstimated   Estimated metrics to guard rail
 * @param[in]       pLo                 Lower endpoint of line
 * @param[in]       pHi                 Higher endpoint of line
 * @param[in]       bMinimum            Whether the line between pLo and pHi is
 *                                      a minimum (or a maximum)
 *
 * @return  FLCN_OK                     Success
 * @return  FLCN_ERR_ILWALID_ARGUMENT   pLo and pHi are not monotonic
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xGuardRailMetricsStructurePrimaryClkPerfRatioLinearity(
        PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *pDlppm1xEstimated,
        const PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS *pLo,
        const PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS *pHi,
        LwBool bMinimum)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xGuardRailMetricsStructurePrimaryClkPerfRatioLinearity");

/*!
 * @brief   Checks linearity of core rail power with respect to
 *          @ref LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_PERF::perfRatio
 *          given various points that may or may not be available.
 *
 * @param[in,out]   pDlppm1x        DLPPM_1X object pointer
 * @param[in,out]   pLwrr           Pointer to estimated metrics to guard rail
 * @param[in]       pAdjacent       Pointer to adjacent metrics in primary clock
 *                                  to use for checking linearity. May be NULL.
 * @param[in]       pEndpointLo     Pointer to low endpoint metrics.
 * @param[in]       pEndpointHi     Pointer to high endpoint metrics.
 * @param[in]       bLeftToRight    Whether the guard railing is being
 *                                  done left-to-right across primary clock
 *                                  frequency. Should be @ref LW_TRUE
 *                                  if frequency is equal.
 * @param[in]       bRightToLeft    Whether the guard railing is being
 *                                  done right-to-left across primary clock
 *                                  frequency. Should be @ref LW_TRUE
 *                                  if frequency is equal.
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrLinearityFromPoints(
        PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *pLwrr,
        const PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS *pAdjacent,
        const PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS *pEndpointLo,
        const PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS *pEndpointHi,
        LwBool bLeftToRight,
        LwBool bRightToLeft)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrLinearityFromPoints");

/*!
 * @brief   Ensure that a given estimated core rail power is within range of
 *          expected value from a fit.
 *
 * @param[in,out]   pDlppm1x            DLPPM_1X object pointer
 * @param[in,out]   pDlppm1xEstimated   Estimated metrics to guard rail
 * @param[in]       pCoreRailFit        Fit to use for checking approximate
 *                                      linearity
 */
static void
s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrFit(
        PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *pDlppm1xEstimated,
        const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_CORE_RAIL_POWER_FIT *pCoreRailFit)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrFit");

/*!
 * @brief   Ensure that a given estimated core rail power respects the line between
 *          two points, either as a minimum or a maximum.
 *
 * @param[in,out]   pDlppm1x            DLPPM_1X object pointer
 * @param[in,out]   pDlppm1xEstimated   Estimated metrics to guard rail
 * @param[in]       pLo                 Lower endpoint of line
 * @param[in]       pHi                 Higher endpoint of line
 * @param[in]       bMinimum            Whether the line between pLo and pHi is
 *                                      a minimum (or a maximum)
 *
 * @return  FLCN_OK                     Success
 * @return  FLCN_ERR_ILWALID_ARGUMENT   pLo and pHi are not monotonic
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrLinearity(
        PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *pLwrr,
        const PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS *pLo,
        const PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS *pHi,
        LwBool bMinimum)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrLinearity");

/*!
 * @brief   Get the number of Loop Variables for a given batch inference
 *
 * @param[in]   pDlppm1x    pointer to the DLPPM_1X Power Model
 *
 * @return                  number of loop variables
 */
static inline LwU16
s_perfCfPwrModelDlppm1xGetNumLoopVars(
        const PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x);

/*!
 * @brief   Guard rail a given DLPPM_1X metrics structure across changing
 *          DRAMCLKs, given various parameters.
 *
 * @param[in,out]   pDlppm1x    DLPPM_1X object pointer
 * @param[in,out]   pLwrr       Current estimated metrics to guard rail
 * @param[in]       pAdjacent   Adjacent metrics structure against which to
 *                              gurad rail
 *
 * @return  FLCN_OK Success
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xGuardRailMetricsStructureDramclk(
        PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *pLwrr,
        const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *pAdjacent)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xGuardRailMetricsStructureDramclk");

/*!
 * @brief   Ensure that a given estimated performance is no better or worse than
 *          linear in terms of DRAMCLK with respect to some known point.
 *
 * @param[in,out]   pDlppm1x            DLPPM_1X object pointer
 * @param[in,out]   pDlppm1xEstimated   Estimated metrics to guard rail
 * @param[in]       pKnownMetrics       Known metrics structure against
 *                                      which to guard rail
 */
static void
s_perfCfPwrModelDlppm1xGuardRailMetricsStructureDramclkPerfRatioLinearity(
        PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *pDlppm1xEstimated,
        const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *pKnownMetrics)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xGuardRailMetricsStructureDramclkPerfRatioLinearity");

/*!
 * @brief   Determines if a DRAMCLK value is approximately equal to an expected
 *          DRAMCLK value, within some tolerance.
 *
 * @param[in]   pDlppm1x    DLPPM_1X object pointer
 * @param[in]   actualkHz   DRAMCLK frequency to check for approximate equality
 * @param[in]   expectedkHz DRAMCLK frequency against which to check
 *
 * @return  @ref LW_TRUE    The DRAMCLK frequencies are approximately equal
 * @return  @ref LW_FALSE   The DRAMCLK frequencies are not approximately equal
 */
static LwBool
s_perfCfPwrModelDlppm1xDramclkApproxEqual(
        const PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
        LwU32 actualkHz,
        LwU32 expectedkHz)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xDramclkApproxEqual");

/*!
 * @brief   Re-initializes the observed metrics structure with various default
 *          values necessary before beginning observation.
 *
 * @param[in]   pDlppm1x            DLPPM_1X object pointer
 * @param[out]  pDlppm1xEstimated   Estimated metrics to initialize
 *
 * @return  @ref FLCN_OK    Success
 * @return  Others          Errors returned by callees
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xObservedMetricsReInit(
        PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xObservedMetricsReInit");

/*!
 * @brief   Initializes the estimated metrics structure with various defaults as
 *          necessary before use.
 *
 * @param[out]   pDlppm1xEstimated   Estimated metrics to initialize
 */
static void
s_perfCfPwrModelDlppm1xEstimatedMetricsInit(
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *pDlppm1xEstimated)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xEstimatedMetricsInit");

/*!
 * @brief   Sets a guard rail flag for a metric and updates the tracking of the
 *          guard rail counts.
 *
 * @param[in,out]   pDlppm1x            DLPPM_1X object pointer. Will have
 *                                      updated guard rail counts on return.
 * @param[out]      pDlppm1xEstimated   Metrics structure in which to set guard
 *                                      rail flag
 * @param[in]       metric              Metric for which to set guard rail flag
 * @param[in]       guardRail           Guard rail to set for metric
 *
 * @return  @ref FLCN_OK    Success
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xGuardRailFlagsSet(
        PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *pDlppm1xEstimated,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_METRIC metric,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_GUARD_RAIL guardRail)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xGuardRailFlagsSet");

/*!
 * @brief   Indicates whether or not a given metric was guard railed in a given
 *          estimated metrics
 *
 * @param[in]   pDlppm1xEstimated   Estimated metrics structure to check
 * @param[in]   metric              Metric for which to check for guard rails
 *
 * @return  @ref LW_TRUE    Metric was guard railed
 * @return  @ref LW_FALSE   Metric was not guard railed
 */
static LwBool
s_perfCfPwrModelDlppm1xGuardRailFlagsMetricGuardRailed(
        const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *pDlppm1xEstimated,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_METRIC metric)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xGuardRailFlagsMetricGuardRailed");

/*!
 * @brief   Retrieves the frequency specified for the given rail in the
 *          @ref VPSTATE_3X object, if available.
 *
 * @param[in]   pRail               Rail for which to retrieve frequency
 * @param[in]   pVpstate3x          @ref VPSTATE_3X from which to retrieve frequency
 * @param[in]   bFallbackToPstate   Whether the frequency should be looked up
 *                                  via the underlying @ref PSTATE if the
 *                                  @ref VPSTATE does not explicitly specify the
 *                                  @ref CLK_DOMAIN for pRail
 * @param[out]  pFreqkHz            The frequency for the given rail in the
 *                                  @ref VPSTATE_3X
 * @param[out]  pbAvailable         Whether clock domain had an available frequency in
 *                                  the @ref VPSTATE_3X
 *
 * @return  @ref FLCN_OK                Success
 * @return  Others                      Errors propagated from callees
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xVpstateRailFreqkHzGet(
        VPSTATE_3X *pVpstate3x,
        const PERF_CF_PWR_MODEL_DLPPM_1X_RAIL *pRail,
        LwBool bFallbackToPstate,
        LwU32 *pFreqkHz,
        LwBool *pbAvailable)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xVpstateRailFreqkHzGet");

/*!
 * @brief   Observe output metrics common to all rails
 *
 * @param[in]   pSampleData     Pointer to current sample data
 * @param[in]   pRail           Pointer to rail structure
 * @param[out]  pRailMetrics    Pointer to structure in which to store observed
 *                              metrics rail
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xObserveOutputRailMetrics(
        const PERF_CF_PWR_MODEL_SAMPLE_DATA                                 *pSampleData,
        const PERF_CF_PWR_MODEL_DLPPM_1X_RAIL                               *pRail,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_OBSERVED_METRICS_DLPPM_1X_RAIL   *pRailMetrics)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xCoreRailObserveOutputMetrics");

/*!
 * @brief   Observe output metrics common to all rails
 *
 * @param[in]   pSampleData     Pointer to current sample data
 * @param[in]   pRail           Pointer to rail structure
 * @param[out]  pRailMetrics    Pointer to structure in which to store observed
 *                              metrics rail
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xObserveOutputRailMetrics(
        const PERF_CF_PWR_MODEL_SAMPLE_DATA                                 *pSampleData,
        const PERF_CF_PWR_MODEL_DLPPM_1X_RAIL                               *pRail,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_OBSERVED_METRICS_DLPPM_1X_RAIL   *pRailMetrics)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xCoreRailObserveOutputMetrics");

/*!
 * @brief   Determines if the frequencies used for correction are valid to use
 *          for the given observed metrics.
 *
 * @details In particular, this means checking the frequencies in the
 *          observedEstimated field to determine if quantization has moved them
 *          too far from the observed frequencies.
 *
 * @param[in]   pDlppm1x            DLPPM_1X object pointer
 * @param[in]   pDlppm1xObserved    Pointer to observed metrics for which to
 *                                  check the frequencies
 *
 * @return  @ref LW_TRUE    Frequencies are valid
 * @return  @ref LW_FALSE   Frequencies are invalid
 */
static LwBool
s_perfCfPwrModelDlppm1xCorrectionFrequenciesValid(
        const PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
        const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelDlppm1xCorrectionFrequenciesValid");

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief   Declare an invalid @ref PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS
 *
 * @param[in]   name    Name with which to declare
 *                      @ref PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS
 */
#define PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS_ILWALID(name) \
    const PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS (name) = \
    { \
        .pPerfMetrics = NULL, \
        .pCoreRailMetrics = NULL, \
        .pFbRailMetrics = NULL, \
    }

/*!
 * @brief   Declares a @ref PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS
 *          and initializes it from an observed metrics pointer.
 *
 * @param[in]   name        Name with which to declare
 *                          @ref PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS
 * @param[in]   pObserved   Pointer to observed metrics from which to initialize
 */
#define PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS_FROM_OBSERVED(name, pObserved) \
    const PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS (name) = \
    { \
        .pPerfMetrics = (pObserved) == NULL ? NULL : &(pObserved)->perfMetrics, \
        .pCoreRailMetrics = (pObserved) == NULL ? NULL : &(pObserved)->coreRail.rails[0].super, \
        .pFbRailMetrics = (pObserved) == NULL ? NULL : &(pObserved)->fbRail.super, \
    } \

/*!
 * @brief   Declares a @ref PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS
 *          and initializes it from an estimated metrics pointer.
 *
 * @param[in]   name        Name with which to declare
 *                          @ref PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS
 * @param[in]   pEstimated  Pointer to estimated metrics from which to initialize
 */
#define PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS_FROM_ESTIMATED(name, pEstimated) \
    const PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS (name) = \
    { \
        .pPerfMetrics = (pEstimated) == NULL ? NULL : &(pEstimated)->perfMetrics, \
        .pCoreRailMetrics = (pEstimated) == NULL ? NULL : &(pEstimated)->coreRail.rails[0], \
        .pFbRailMetrics = (pEstimated) == NULL ? NULL : &(pEstimated)->fbRail, \
    } \

/*!
 * @brief   Declares a @ref PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS
 *          and initializes it with all 0s, so that it can be treated as point
 *          at the "origin" in various dimensions.
 *
 * @param[in]   name    Name with which to declare
 *                      @ref PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS
 */
#define PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS_ORIGIN(name) \
    const PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS (name) = \
    { \
        .pPerfMetrics = &(LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_PERF){0}, \
        .pCoreRailMetrics = &(LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_RAIL){{0}}, \
        .pFbRailMetrics = &(LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_RAIL){{0}}, \
    }

/*!
 * @brief   Indicates whether a given
 *          @ref PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS
 *          is valid or not.
 *
 * @param[in]   pMetrics    @ref PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS
 *                          to check
 *
 * @return  LW_TRUE     pMetrics is valid
 * @return  LW_FALSE    pMetrics is not valid
 */
#define PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS_IS_VALID(pMetrics) \
    (((pMetrics)->pPerfMetrics != NULL) && \
     ((pMetrics)->pCoreRailMetrics != NULL) && \
     ((pMetrics)->pFbRailMetrics != NULL))

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Global Variables -------------------------------- */
NNE_DESC_INFERENCE   inferenceStage
    GCC_ATTRIB_SECTION("dmem_dlppm1xRuntimeScratch", "inferenceStage");

static PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT PerfCfPwrModelScaleInputs[
        LW2080_CTRL_NNE_NNE_DESC_INFERENCE_LOOPS_MAX]
    GCC_ATTRIB_SECTION("dmem_dlppm1xRuntimeScratch", "PerfCfPwrModelScaleInputs");

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
perfCfPwrModelGrpIfaceModel10ObjSetImpl_DLPPM_1X
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);

    RM_PMU_PERF_CF_PWR_MODEL_DLPPM_1X   *pTmpDlppm1x
        = (RM_PMU_PERF_CF_PWR_MODEL_DLPPM_1X *)pBoardObjDesc;
    PERF_CF_PWR_MODEL_DLPPM_1X          *pDlppm1x;
    FLCN_STATUS                          status   = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pBObjGrp      == NULL) ||
         (ppBoardObj    == NULL) ||
         (pBoardObjDesc == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
         perfCfPwrModelGrpIfaceModel10ObjSetImpl_DLPPM_1X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pTmpDlppm1x->nneDescIdx  == LW2080_CTRL_BOARDOBJ_IDX_ILWALID) ||
         (pTmpDlppm1x->pmSensorIdx == LW2080_CTRL_BOARDOBJ_IDX_ILWALID) ||
         (pTmpDlppm1x->tgpPwrChIdx == LW2080_CTRL_BOARDOBJ_IDX_ILWALID ||
         (pTmpDlppm1x->ptimerTopologyIdx == LW2080_CTRL_BOARDOBJ_IDX_ILWALID))) ?
            FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        perfCfPwrModelGrpIfaceModel10ObjSetImpl_DLPPM_1X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        (pTmpDlppm1x->vfInferenceBatchSize > 0U) &&
        (pTmpDlppm1x->vfInferenceBatchSize <=
            LW2080_CTRL_NNE_NNE_DESC_INFERENCE_LOOPS_MAX) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
        perfCfPwrModelGrpIfaceModel10ObjSetImpl_DLPPM_1X_exit);

    // Ilwoke the super-class constructor.
    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfPwrModelGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc),
        perfCfPwrModelGrpIfaceModel10ObjSetImpl_DLPPM_1X_exit);
    pDlppm1x = (PERF_CF_PWR_MODEL_DLPPM_1X *)(*ppBoardObj);

    // Set member variables.
    pDlppm1x->coreRailPwrFitTolerance   = pTmpDlppm1x->coreRailPwrFitTolerance;
    pDlppm1x->nneDescIdx                = pTmpDlppm1x->nneDescIdx;
    pDlppm1x->pmSensorIdx               = pTmpDlppm1x->pmSensorIdx;
    pDlppm1x->dramclkMatchTolerance     = pTmpDlppm1x->dramclkMatchTolerance;
    pDlppm1x->observedPerfMsMinPct        = pTmpDlppm1x->observedPerfMsMinPct;
    pDlppm1x->correctionBounds          = pTmpDlppm1x->correctionBounds;
    pDlppm1x->smallRailPwrChIdx         = pTmpDlppm1x->smallRailPwrChIdx;
    pDlppm1x->tgpPwrChIdx               = pTmpDlppm1x->tgpPwrChIdx;
    pDlppm1x->ptimerTopologyIdx         = pTmpDlppm1x->ptimerTopologyIdx;
    pDlppm1x->vfInferenceBatchSize      = pTmpDlppm1x->vfInferenceBatchSize;
    pDlppm1x->bFakeGpcClkCounts         = pTmpDlppm1x->bFakeGpcClkCounts;
    pDlppm1x->gpcclkPmIndex             = pTmpDlppm1x->gpcclkPmIndex;
    pDlppm1x->xbarclkPmIndex             = pTmpDlppm1x->xbarclkPmIndex;

    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelCoreRailCopy_DLPPM_1X(&(pDlppm1x->coreRail),
                                              &(pTmpDlppm1x->coreRail)),
        perfCfPwrModelGrpIfaceModel10ObjSetImpl_DLPPM_1X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelFbvddRailCopy_DLPPM_1X(&(pDlppm1x->fbRail),
                                               &(pTmpDlppm1x->fbRail)),
        perfCfPwrModelGrpIfaceModel10ObjSetImpl_DLPPM_1X_exit);

    // Cache the device-info data.
    PMU_ASSERT_OK_OR_GOTO(status,
        fuseNumTpcGet_HAL(&Fuse, &(pDlppm1x->chipConfig.numTpc)),
        perfCfPwrModelGrpIfaceModel10ObjSetImpl_DLPPM_1X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        fuseNumFbpGet_HAL(&Fuse, &(pDlppm1x->chipConfig.numFbp)),
        perfCfPwrModelGrpIfaceModel10ObjSetImpl_DLPPM_1X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        fuseNumLtcsGet_HAL(&Fuse, &(pDlppm1x->chipConfig.numLtc)),
        perfCfPwrModelGrpIfaceModel10ObjSetImpl_DLPPM_1X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        fuseNumGpcsGet_HAL(&Fuse, &(pDlppm1x->chipConfig.numGpc)),
        perfCfPwrModelGrpIfaceModel10ObjSetImpl_DLPPM_1X_exit);

    // Sanity check to make sure the chipConfigs returned are non-zero
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDlppm1x->chipConfig.numTpc != 0 && 
         pDlppm1x->chipConfig.numFbp != 0 &&
         pDlppm1x->chipConfig.numLtc != 0 &&
         pDlppm1x->chipConfig.numGpc != 0),
        FLCN_ERR_ILWALID_STATE,
        perfCfPwrModelGrpIfaceModel10ObjSetImpl_DLPPM_1X_exit);

    // boardObjGrpMaskInit_E1024 does not return an error.
    boardObjGrpMaskInit_E1024(&(pDlppm1x->pmSensorSignalMask));
    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpMaskImport_E1024(&(pDlppm1x->pmSensorSignalMask),
                                   &(pTmpDlppm1x->pmSensorSignalMask)),
        perfCfPwrModelGrpIfaceModel10ObjSetImpl_DLPPM_1X_exit);

    //
    // Initialize the NNE inference structure, if it is not initialized.
    //
    // Number of loop-independent inputs =
    //     Number of sampled BA PM's +
    //     Number of chip-config settings
    //
    // Number of loop-dependent inputs =
    //     Number of absolute frequencies +
    //     Number of relative frequencies
    // Which in turn equals:
    //     (num rails) * 2 frequencies (absolute + relative) per rail
    //
    // TODO: This implementation only works for one model. To support multiple model,
    //       must initialize the NNE client with the largest value of all models,
    //       for each dimension. Alternatively, one NNE client per model.
    //
    if ((inferenceStage.pVarInputsStatic == NULL) ||
        (inferenceStage.pLoops           == NULL))
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            nneDescInferenceInit(
                &inferenceStage,
                OVL_INDEX_DMEM(dlppm1xRuntimeScratch),
                pDlppm1x->nneDescIdx,
                boardObjGrpMaskBitSetCount(&(pDlppm1x->pmSensorSignalMask)) +
                    LW2080_CTRL_NNE_NNE_VAR_CHIP_CONFIG_CONFIG_TYPE_NUM, // # of loop-independent vars.
                s_perfCfPwrModelDlppm1xGetNumLoopVars(pDlppm1x),         // # of loop-dependent vars.
                LW2080_CTRL_NNE_NNE_DESC_INFERENCE_LOOPS_MAX,
                NULL),                                     // NULL sync queue handle.
            perfCfPwrModelGrpIfaceModel10ObjSetImpl_DLPPM_1X_exit);

        // Populate constant inputs.
        inferenceStage.hdr.descIdx = pDlppm1x->nneDescIdx;
    }

perfCfPwrModelGrpIfaceModel10ObjSetImpl_DLPPM_1X_exit:
    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
perfCfPwrModelIfaceModel10GetStatus_DLPPM_1X
(
    BOARDOBJ_IFACE_MODEL_10          *pModel10,
    RM_PMU_BOARDOBJ   *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    FLCN_STATUS status;
    RM_PMU_PERF_CF_PWR_MODEL_DLPPM_1X_GET_STATUS *pStatus =
        (RM_PMU_PERF_CF_PWR_MODEL_DLPPM_1X_GET_STATUS *)pPayload;
    PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x =
            (PERF_CF_PWR_MODEL_DLPPM_1X *)pBoardObj;

    // Sanity checking.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pBoardObj != NULL) &&
         (pPayload  != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfPwrModelIfaceModel10GetStatus_DLPPM_1X_exit);

    // Call the super-class implementation.
    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfPwrModelIfaceModel10GetStatus_SUPER(
            pModel10,
            pPayload),
        perfCfPwrModelIfaceModel10GetStatus_DLPPM_1X_exit);

    // Copy over type specific data.
    pStatus->guardRailsStatus = pDlppm1x->guardRailsStatus;

perfCfPwrModelIfaceModel10GetStatus_DLPPM_1X_exit:
    return status;
}

/*!
 * @copydoc PerfCfPwrModelSampleDataInit
 */
FLCN_STATUS
perfCfPwrModelSampleDataInit_DLPPM_1X
(
    PERF_CF_PWR_MODEL                              *pPwrModel,
    PERF_CF_PWR_MODEL_SAMPLE_DATA                  *pSampleData,
    PERF_CF_TOPOLOGYS_STATUS                       *pPerfTopoStatus,
    PWR_CHANNELS_STATUS                            *pPwrChannelsStatus,
    PERF_CF_PM_SENSORS_STATUS                      *pPmSensorsStatus,
    PERF_CF_CONTROLLERS_REDUCED_STATUS             *pPerfCfControllersStatus,
    PERF_CF_PWR_MODEL_CALLER_SPECIFIED_SAMPLE_DATA *pPerfCfPwrModelCallerSpecifiedData
)
{
    FLCN_STATUS status;
    PERF_CF_PWR_MODEL_DLPPM_1X *const pPwrModelDlppm1x =
        (PERF_CF_PWR_MODEL_DLPPM_1X *)pPwrModel;

    // Sanity checking
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pPwrModelDlppm1x != NULL) &&
         (pSampleData != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfPwrModelSampleDataInit_DLPPM_1X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelDlppm1xPerfTopoStatusInit(pPwrModelDlppm1x, pPerfTopoStatus),
        perfCfPwrModelSampleDataInit_DLPPM_1X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelDlppm1xPwrChannelsStatusInit(pPwrModelDlppm1x, pPwrChannelsStatus),
        perfCfPwrModelSampleDataInit_DLPPM_1X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelDlppm1xPmSensorsStatusInit(pPwrModelDlppm1x, pPmSensorsStatus),
        perfCfPwrModelSampleDataInit_DLPPM_1X_exit);

    //
    // Ensure that the caller of DLLPM_1X has passed in a
    // valid callerSpecifiedSampleData struct
    //
    PMU_ASSERT_TRUE_OR_GOTO(status,
        pPerfCfPwrModelCallerSpecifiedData != NULL,
        FLCN_ERR_OBJECT_NOT_FOUND,
        perfCfPwrModelSampleDataInit_DLPPM_1X_exit);

perfCfPwrModelSampleDataInit_DLPPM_1X_exit:
    return status;
}

/*!
 * @copydoc PerfCfPwrModelObserveMetrics
 */
FLCN_STATUS
perfCfPwrModelObserveMetrics_DLPPM_1X
(
    PERF_CF_PWR_MODEL                             *pPwrModel,
    PERF_CF_PWR_MODEL_SAMPLE_DATA                 *pSampleData,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS    *pObservedMetrics
)
{
    LwU8                          coreRailIdx;
    PERF_CF_PWR_MODEL_DLPPM_1X   *pDlppm1x = (PERF_CF_PWR_MODEL_DLPPM_1X *)pPwrModel;
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED
                                 *pDlppm1xObservedMetrics = (LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *)pObservedMetrics;
    LwU16                         baIdx;
    FLCN_STATUS                   status   = FLCN_OK;
    LwBool                        bWithinBounds;
    VPSTATE_3X                   *pVpstate3x;
    // Initialize the minimums to 0
    LwU32                         dramclkMinkHz = 0;
    LwU32                         coreRailsMinkHz[LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_DLPPM_1X_MAX_CORE_RAILS] = {0};
    LwU64                         baPmValue;

    // Sanity checking.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pPwrModel        != NULL) &&
         (pSampleData      != NULL) &&
         (pObservedMetrics != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfPwrModelObserveMetrics_DLPPM_1X_exit);

    // Sanity checking.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pSampleData->pPerfTopoStatus != NULL)          &&
         (pSampleData->pPwrChannelsStatus != NULL)       &&
         (pSampleData->pPmSensorsStatus != NULL)         &&
         (pSampleData->pPerfCfPwrModelCallerSpecifiedData != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfPwrModelObserveMetrics_DLPPM_1X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        (pDlppm1xObservedMetrics->super.type !=
         LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_DLPPM_1X) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        perfCfPwrModelObserveMetrics_DLPPM_1X_exit);

    perfCfPwrModelDlppm1xObserveMetricsProfileRegionBegin(
        &pDlppm1xObservedMetrics->profiling,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION_TOTAL);

    // Initialize the observed metrics state first.
    s_perfCfPwrModelDlppm1xObservedMetricsReInit(
        pDlppm1x,
        pDlppm1xObservedMetrics);

    perfCfPwrModelDlppm1xObserveMetricsProfileRegionBegin(
        &pDlppm1xObservedMetrics->profiling,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION_SAMPLE_DATA);

    // Copy over individual fields.
    // TODO: Move chip-config information to LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_INFO_DLPPM_1X
    pDlppm1xObservedMetrics->chipConfig  = pDlppm1x->chipConfig;
    pDlppm1xObservedMetrics->tgpPwrTuple =
        pSampleData->pPwrChannelsStatus->channels[pDlppm1x->tgpPwrChIdx].channel.tuple;
    pDlppm1xObservedMetrics->perfTarget = pSampleData->pPerfCfPwrModelCallerSpecifiedData->perfTarget;

    BOARDOBJGRPMASK_FOR_EACH_BEGIN(&pDlppm1x->pmSensorSignalMask, baIdx)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            LW2080_CTRL_BOARDOBJGRP_MASK_BIT_GET(&(pSampleData->pPmSensorsStatus->pmSensors[pDlppm1x->pmSensorIdx].pmSensor.signalsMask.super), baIdx)
                == 0 ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
            perfCfPwrModelObserveMetrics_DLPPM_1X_exit);
        if (baIdx == pDlppm1x->gpcclkPmIndex)
        {
            LwU64_ALIGN32_UNPACK(&baPmValue,
                        &pSampleData->pPmSensorsStatus->pmSensors[pDlppm1x->pmSensorIdx].pmSensor.signals[baIdx].cntDiff);

            lwU64Div(&baPmValue, &baPmValue, &(LwU64){pDlppm1x->chipConfig.numGpc});

            LwU64_ALIGN32_PACK(&pDlppm1xObservedMetrics->pmSensorDiff[baIdx], &baPmValue);
        }
        else if (baIdx == pDlppm1x->xbarclkPmIndex)
        {
            LwU64_ALIGN32_UNPACK(&baPmValue,
                        &pSampleData->pPmSensorsStatus->pmSensors[pDlppm1x->pmSensorIdx].pmSensor.signals[baIdx].cntDiff);

            lwU64Div(&baPmValue, &baPmValue, &(LwU64){pDlppm1x->chipConfig.numFbp});

            LwU64_ALIGN32_PACK(&pDlppm1xObservedMetrics->pmSensorDiff[baIdx], &baPmValue);
        }
        else
        {
            pDlppm1xObservedMetrics->pmSensorDiff[baIdx] = pSampleData->pPmSensorsStatus->pmSensors[pDlppm1x->pmSensorIdx].pmSensor.signals[baIdx].cntDiff;
        }
    }
    BOARDOBJGRPMASK_FOR_EACH_END;

    // Observe FB rail's metrics.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelObserveFbRailMetrics_DLPPM_1X(pDlppm1x,
                                                      pSampleData,
                                                    &(pDlppm1x->fbRail),
                                                    &(pDlppm1xObservedMetrics->fbRail)),
        perfCfPwrModelObserveMetrics_DLPPM_1X_exit);

    //
    // Observe each of the core rails' metrics and sum them together to
    // get the combined core rail power tuple.
    //
    for (coreRailIdx = 0; coreRailIdx < pDlppm1x->coreRail.numRails; coreRailIdx++)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfPwrModelObserveCoreRailMetrics_DLPPM_1X(
                pSampleData,
                &(pDlppm1x->coreRail.pRails[coreRailIdx]),
                &(pDlppm1xObservedMetrics->coreRail.rails[coreRailIdx])),
            perfCfPwrModelObserveMetrics_DLPPM_1X_exit);

        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfPwrModelPowerTupleAdd_DLPPM_1X(
                &(pDlppm1xObservedMetrics->coreRail.rails[coreRailIdx].super.pwrInTuple),
                &(pDlppm1xObservedMetrics->coreRail.pwrInTuple),
                &(pDlppm1xObservedMetrics->coreRail.pwrInTuple)),
            perfCfPwrModelObserveMetrics_DLPPM_1X_exit);
    }

    // Observe small rails' metrics, if a small rail power channel has been provided.
    if (pDlppm1x->smallRailPwrChIdx != LW2080_CTRL_BOARDOBJ_IDX_ILWALID)
    {
        pDlppm1xObservedMetrics->smallRailPwrTuple =
            pSampleData->pPwrChannelsStatus->channels[pDlppm1x->smallRailPwrChIdx].channel.tuple;
    }

    perfCfPwrModelDlppm1xObserveMetricsProfileRegionEnd(
        &pDlppm1xObservedMetrics->profiling,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION_SAMPLE_DATA);

    //
    // If the DLPPM_1X_ESTIMATION_MINIMUM VPSTATE exists, then use it to
    // determine if the observed clocks are valid for estimation.
    //

    perfCfPwrModelDlppm1xObserveMetricsProfileRegionBegin(
        &pDlppm1xObservedMetrics->profiling,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION_VPSTATE_VALIDITY);

    {
        pVpstate3x = (VPSTATE_3X *)vpstateGetBySemanticIdx(
            RM_PMU_PERF_VPSTATE_IDX_DLPPM_1X_ESTIMATION_MINIMUM);

        // If the VPSTATE IDX does not exist, error out
        PMU_ASSERT_TRUE_OR_GOTO(status,
            pVpstate3x != NULL,
            FLCN_ERROR,
            perfCfPwrModelObserveMetrics_DLPPM_1X_clkValidityExit);

        LwBool bDramclkAvailable;

        // If the VPSTATE has a 0 dramclk value, error out
        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfPwrModelDlppm1xVpstateRailFreqkHzGet(
                pVpstate3x,
                &pDlppm1x->fbRail.super,
                LW_TRUE,
                &dramclkMinkHz,
                &bDramclkAvailable),
            perfCfPwrModelObserveMetrics_DLPPM_1X_clkValidityExit);

        PMU_ASSERT_TRUE_OR_GOTO(status,
            dramclkMinkHz != 0,
            FLCN_ERROR,
            perfCfPwrModelObserveMetrics_DLPPM_1X_clkValidityExit);

        //
        // If we are able to get a valid minimum dramclk, then error out
        // if the current observed dramclk is less than the minimum dramclk
        // and observed dramclk is not aproximately equal to the minimum value
        //
        if (bDramclkAvailable &&
            (pDlppm1xObservedMetrics->fbRail.super.freqkHz <
                dramclkMinkHz) &&
            !s_perfCfPwrModelDlppm1xDramclkApproxEqual(
                pDlppm1x,
                pDlppm1xObservedMetrics->fbRail.super.freqkHz,
                dramclkMinkHz))
        {
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_ILWALID_METRICS_SET_REASON(
                pDlppm1xObservedMetrics->ilwalidMetricsReason,
                _LOW_MEMCLK);
            goto perfCfPwrModelObserveMetrics_DLPPM_1X_clkValidityExit;
        }

        for (coreRailIdx = 0; coreRailIdx < pDlppm1x->coreRail.numRails; coreRailIdx++)
        {
            LwBool bCoreRailClkAvailable;

            PMU_ASSERT_OK_OR_GOTO(status,
                s_perfCfPwrModelDlppm1xVpstateRailFreqkHzGet(
                    pVpstate3x,
                    &pDlppm1x->coreRail.pRails[coreRailIdx],
                    LW_FALSE,
                    &coreRailsMinkHz[coreRailIdx],
                    &bCoreRailClkAvailable),
               perfCfPwrModelObserveMetrics_DLPPM_1X_clkValidityExit);

            if (bCoreRailClkAvailable &&
                (pDlppm1xObservedMetrics->coreRail.rails[coreRailIdx].super.freqkHz <
                    coreRailsMinkHz[coreRailIdx]))
            {
                LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_ILWALID_METRICS_SET_REASON(
                    pDlppm1xObservedMetrics->ilwalidMetricsReason,
                    _LOW_GPCCLK);
                goto perfCfPwrModelObserveMetrics_DLPPM_1X_clkValidityExit;
            }
        }
perfCfPwrModelObserveMetrics_DLPPM_1X_clkValidityExit:
        lwosNOP();
    }

    perfCfPwrModelDlppm1xObserveMetricsProfileRegionEnd(
        &pDlppm1xObservedMetrics->profiling,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION_VPSTATE_VALIDITY);

    if (pDlppm1xObservedMetrics->ilwalidMetricsReason !=
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_ILWALID_METRICS_REASON_NONE)
    {
        goto perfCfPwrModelObserveMetrics_DLPPM_1X_exit;
    }

    perfCfPwrModelDlppm1xObserveMetricsProfileRegionBegin(
        &pDlppm1xObservedMetrics->profiling,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION_OBSERVE_PERF);

    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelObservePerfMetrics_DLPPM_1X(
            pDlppm1x,
            pSampleData,
            pDlppm1xObservedMetrics,
            &bWithinBounds),
        perfCfPwrModelObserveMetrics_DLPPM_1X_exit);

    perfCfPwrModelDlppm1xObserveMetricsProfileRegionEnd(
        &pDlppm1xObservedMetrics->profiling,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION_OBSERVE_PERF);

    //
    // If the perfMs value callwlated from the BAPM gpcclk counter is too low,
    // then return early with invalid metrics
    //
    if (!bWithinBounds)
    {
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_ILWALID_METRICS_SET_REASON(
            pDlppm1xObservedMetrics->ilwalidMetricsReason,
            _LOW_OBS_PERFMS);
        goto perfCfPwrModelObserveMetrics_DLPPM_1X_exit;
    }

    perfCfPwrModelDlppm1xObserveMetricsProfileRegionBegin(
        &pDlppm1xObservedMetrics->profiling,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION_NORMALIZE);

    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelDlppm1xObservedMetricsNormalize(
            pDlppm1x,
            pDlppm1xObservedMetrics,
            &bWithinBounds),
        perfCfPwrModelObserveMetrics_DLPPM_1X_exit);

    perfCfPwrModelDlppm1xObserveMetricsProfileRegionEnd(
        &pDlppm1xObservedMetrics->profiling,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION_NORMALIZE);

    // If normalization fails, exit early with invalid metrics
    if (!bWithinBounds)
    {
        goto perfCfPwrModelObserveMetrics_DLPPM_1X_exit;
    }

    perfCfPwrModelDlppm1xObserveMetricsProfileRegionBegin(
        &pDlppm1xObservedMetrics->profiling,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION_OBSERVE_CORRECTION);

    status = s_perfCfPwrModelObserveCorrection_DLPPM_1X(
        pDlppm1x,
        pDlppm1xObservedMetrics,
        &bWithinBounds);

    perfCfPwrModelDlppm1xObserveMetricsProfileRegionEnd(
        &pDlppm1xObservedMetrics->profiling,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION_OBSERVE_CORRECTION);

    //
    // We can potentially end up in situations where the observed frequencies
    // are not valid frequencies for observing correction. If so, since that
    // meant we couldn't observe correction at all, we want to clear the status
    // but ilwalidate the metrics and return early.
    //
    if (status == FLCN_ERR_FREQ_NOT_SUPPORTED)
    {
        status = FLCN_OK;
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_ILWALID_METRICS_SET_REASON(
            pDlppm1xObservedMetrics->ilwalidMetricsReason,
            _BAD_FREQUENCY_FOR_CORRECTION);
        goto perfCfPwrModelObserveMetrics_DLPPM_1X_exit;
    }

    // Check the remaining error codes.
    PMU_ASSERT_OK_OR_GOTO(status,
        status,
        perfCfPwrModelObserveMetrics_DLPPM_1X_exit);

    //
    // If the corrections were not within their bounds, then return early with
    // invalid metrics
    //
    if (!bWithinBounds)
    {
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_ILWALID_METRICS_SET_REASON(
            pDlppm1xObservedMetrics->ilwalidMetricsReason,
            _OUT_OF_BOUNDS_CORRECTION);
        goto perfCfPwrModelObserveMetrics_DLPPM_1X_exit;
    }

    perfCfPwrModelDlppm1xObserveMetricsProfileRegionBegin(
        &pDlppm1xObservedMetrics->profiling,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION_OBSERVE_INITIAL_DRAMCLK_ESTIMATES);

    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelObserveInitialDramclkEstimates_DLPPM_1X(
            pDlppm1x,
            pDlppm1xObservedMetrics,
            dramclkMinkHz,
            coreRailsMinkHz),
        perfCfPwrModelObserveMetrics_DLPPM_1X_exit);

    perfCfPwrModelDlppm1xObserveMetricsProfileRegionEnd(
        &pDlppm1xObservedMetrics->profiling,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION_OBSERVE_INITIAL_DRAMCLK_ESTIMATES);

    //
    // Ensure that we have initial estimates and that the current DRAMCLK is
    // valid before marking the metrics valid.
    //
    if((pDlppm1xObservedMetrics->numInitialDramclkEstimates == 0) ||
        (pDlppm1xObservedMetrics->lwrrDramclkInitialEstimatesIdx == LW_U8_MAX))
    {
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_ILWALID_METRICS_SET_REASON(
            pDlppm1xObservedMetrics->ilwalidMetricsReason,
            _BAD_INITIAL_ESTIMATES);
        goto perfCfPwrModelObserveMetrics_DLPPM_1X_exit;
    }

    pDlppm1xObservedMetrics->super.bValid = LW_TRUE;


perfCfPwrModelObserveMetrics_DLPPM_1X_exit:
    if (pDlppm1xObservedMetrics != NULL)
    {
        // if the return status is not FLCN_OK, log the ilwalidMetricsReason and return
        if ((status != FLCN_OK) &&
            (pDlppm1xObservedMetrics->ilwalidMetricsReason == 
                LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_ILWALID_METRICS_REASON_NONE))
        {
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_ILWALID_METRICS_SET_REASON(
                pDlppm1xObservedMetrics->ilwalidMetricsReason,
                _FLCN_ERROR);
        }

        perfCfPwrModelDlppm1xObserveMetricsProfileRegionEnd(
            &pDlppm1xObservedMetrics->profiling,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION_TOTAL);
    }

    return status;
}

/*!
 * @copydoc PerfCfPwrModelScale
 */
FLCN_STATUS
perfCfPwrModelScale_DLPPM_1X
(
    PERF_CF_PWR_MODEL                              *pPwrModel,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS     *pObservedMetrics,
    LwLength                                        numMetricsInputs,
    PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT          *pMetricsInputs,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS    **ppEstimatedMetrics,
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS            *pTypeParams
)
{
    PERF_CF_PWR_MODEL_DLPPM_1X   *pDlppm1x = (PERF_CF_PWR_MODEL_DLPPM_1X *)pPwrModel;
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_DLPPM_1X *const pDlppm1xParams =
        (PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_DLPPM_1X *)pTypeParams;
    PERF_LIMITS_VF                vfDomain;
    PERF_LIMITS_VF                vfRail;
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED
                                 *pDlppm1xObserved = (LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *)pObservedMetrics;
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X
                                 *pDlppm1xEstimated;
    VOLT_RAIL                    *pRail;
    BOARDOBJGRPMASK_E32          *pClkDomProgMask;
    LwU8                          loopIdx;
    LwU8                          railIdx;
    FLCN_STATUS                   status   = FLCN_OK;
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_SCALE_PROFILING *pProfiling = NULL;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pPwrModel          == NULL) ||
         (pObservedMetrics   == NULL) ||
         (pMetricsInputs     == NULL) ||
         (ppEstimatedMetrics == NULL) ||
         (pTypeParams == NULL) ||
         (pTypeParams->type != LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_DLPPM_1X)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        perfCfPwrModelScale_DLPPM_1X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        (numMetricsInputs == 0U) ||
        (numMetricsInputs > LW2080_CTRL_NNE_NNE_DESC_INFERENCE_LOOPS_MAX) ?
            FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        perfCfPwrModelScale_DLPPM_1X_exit);

    pProfiling = pDlppm1xParams->pProfiling;

    perfCfPwrModelDlppm1xScaleProfileRegionBegin(
        pProfiling,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_DLPPM_1X_SCALE_PROFILING_REGION_TOTAL);

    //
    // Ensure:
    //  1.) METRICS type is correct
    //  2.) Primary clock is monotonically increasing.
    //  3.) All DRAMCLKs are the same
    //  4.) Primary clock and DRAMCLK frequencies are > 0
    //
    for (loopIdx = 0; loopIdx < numMetricsInputs; loopIdx++)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            ppEstimatedMetrics[loopIdx]->type == LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_DLPPM_1X ?
                FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
            perfCfPwrModelScale_DLPPM_1X_exit);

        if (loopIdx > 0U)
        {
            LwU32 coreClk     = 0;
            LwU32 coreClkPrev = 0;
            LwU32 fbClk       = 0;
            LwU32 fbClkPrev   = 0;

            PMU_ASSERT_OK_OR_GOTO(status,
                perfCfPwrModelScaleMeticsInputGetFreqkHz(
                    &pMetricsInputs[loopIdx],
                    pDlppm1x->coreRail.pRails[0].clkDomIdx,
                    &coreClk),
                perfCfPwrModelScale_DLPPM_1X_exit);

            PMU_ASSERT_OK_OR_GOTO(status,
                perfCfPwrModelScaleMeticsInputGetFreqkHz(
                    &pMetricsInputs[loopIdx - 1U],
                    pDlppm1x->coreRail.pRails[0].clkDomIdx,
                    &coreClkPrev),
                perfCfPwrModelScale_DLPPM_1X_exit);

            PMU_ASSERT_OK_OR_GOTO(status,
                perfCfPwrModelScaleMeticsInputGetFreqkHz(
                    &pMetricsInputs[loopIdx],
                    pDlppm1x->fbRail.super.clkDomIdx,
                    &fbClk),
                perfCfPwrModelScale_DLPPM_1X_exit);

            PMU_ASSERT_OK_OR_GOTO(status,
                perfCfPwrModelScaleMeticsInputGetFreqkHz(
                    &pMetricsInputs[loopIdx - 1U],
                    pDlppm1x->fbRail.super.clkDomIdx,
                    &fbClkPrev),
                perfCfPwrModelScale_DLPPM_1X_exit);

            PMU_ASSERT_OK_OR_GOTO(status,
                ((coreClk > coreClkPrev) &&
                 (fbClk == fbClkPrev) &&
                 (fbClk != 0) &&
                 (coreClk != 0))?
                    FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
                perfCfPwrModelScale_DLPPM_1X_exit);
        }
    }

    perfCfPwrModelDlppm1xScaleProfileRegionBegin(
        pProfiling,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_DLPPM_1X_SCALE_PROFILING_REGION_METRICS_POPULATE);

    // Populate known values in the estimated metrics.
    for (loopIdx = 0; loopIdx < numMetricsInputs; loopIdx++)
    {
        LwU32 fbFreqkHzInput = 0;

        PMU_ASSERT_OK_OR_GOTO(status,
            perfCfPwrModelScaleMeticsInputGetFreqkHz(
                &pMetricsInputs[loopIdx],
                pDlppm1x->fbRail.super.clkDomIdx,
                &fbFreqkHzInput),
            perfCfPwrModelScale_DLPPM_1X_exit);

        pDlppm1xEstimated = (LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *)ppEstimatedMetrics[loopIdx];

        s_perfCfPwrModelDlppm1xEstimatedMetricsInit(pDlppm1xEstimated);

        //
        // For each of the core rails and FBVDD:
        // 1) Set the observed/baseline rail frequency.
        // 2) Set the estimated metrics' input voltage to the observed input
        //    voltage under the assumption input voltage does not change
        //    materially with respect to output voltage.
        //
        for (railIdx = 0; railIdx < pDlppm1x->coreRail.numRails; railIdx++)
        {
            LwU32 coreFreqkHzInput = 0;

            PMU_ASSERT_OK_OR_GOTO(status,
                perfCfPwrModelScaleMeticsInputGetFreqkHz(
                    &pMetricsInputs[loopIdx],
                    pDlppm1x->coreRail.pRails[railIdx].clkDomIdx,
                    &coreFreqkHzInput),
                perfCfPwrModelScale_DLPPM_1X_exit);

            pDlppm1xEstimated->coreRail.rails[railIdx].freqkHz = coreFreqkHzInput;
            pDlppm1xEstimated->coreRail.rails[railIdx].pwrInTuple.voltuV =
                pDlppm1xObserved->coreRail.rails[railIdx].super.pwrInTuple.voltuV;

            //
            // Compute the minimum voltage needed to support all independent clock
            // domains on this rail, excluding the primary rail that will be shmoo-ed.
            //
            pRail = VOLT_RAIL_GET(pDlppm1x->coreRail.pRails[railIdx].voltRailIdx);
            PMU_ASSERT_OK_OR_GOTO(status,
                (pRail == NULL) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
                perfCfPwrModelScale_DLPPM_1X_exit);

            pClkDomProgMask = voltRailGetClkDomainsProgMask(pRail);
            PMU_ASSERT_OK_OR_GOTO(status,
                (pClkDomProgMask == NULL) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
                perfCfPwrModelScale_DLPPM_1X_exit);

            vfRail.idx   = pDlppm1x->coreRail.pRails[railIdx].voltRailIdx;
            vfRail.value = 0;
            if (boardObjGrpMaskBitGet(pClkDomProgMask,
                                      pDlppm1x->fbRail.super.clkDomIdx))
            {
                vfDomain.idx   = pDlppm1x->fbRail.super.clkDomIdx;
                vfDomain.value = fbFreqkHzInput;
                PMU_ASSERT_OK_OR_GOTO(status,
                    perfLimitsFreqkHzToVoltageuV(&vfDomain, &vfRail, LW_TRUE),
                    perfCfPwrModelScale_DLPPM_1X_exit);
                PMU_ASSERT_OK_OR_GOTO(status,
                    (vfRail.value == 0) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
                    perfCfPwrModelScale_DLPPM_1X_exit);
            }

            //
            // Compute the minimum voltage needed to support all clock domains on
            // this rail, not going below the lowest voltage allowable by the rail.
            //
            pDlppm1xEstimated->coreRail.rails[railIdx].voltMinuV
                = LW_MAX(vfRail.value,
                         LW_MAX(pDlppm1xObserved->coreRail.rails[railIdx].maxIndependentClkDomVoltMinuV,
                                pDlppm1xObserved->coreRail.rails[railIdx].vminLimituV));

            //
            // Compute the Vmin corresponding to the frequency at which the rail
            // power is being estimated at.
            //
            vfDomain.idx   = pDlppm1x->coreRail.pRails[railIdx].clkDomIdx;
            vfDomain.value = coreFreqkHzInput;
            PMU_ASSERT_OK_OR_GOTO(status,
                perfLimitsFreqkHzToVoltageuV(&vfDomain, &vfRail, LW_TRUE),
                perfCfPwrModelScale_DLPPM_1X_exit);
            PMU_ASSERT_OK_OR_GOTO(status,
                (vfRail.value == 0) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
                perfCfPwrModelScale_DLPPM_1X_exit);

            // Bound the voltage of the rail to be at least voltMinuV.
            pDlppm1xEstimated->coreRail.rails[railIdx].pwrOutTuple.voltuV
                = LW_MAX(pDlppm1xEstimated->coreRail.rails[railIdx].voltMinuV,
                         vfRail.value);
            pDlppm1xEstimated->coreRail.rails[railIdx].voltuV = 
                pDlppm1xEstimated->coreRail.rails[railIdx].pwrOutTuple.voltuV;
        }

        pDlppm1xEstimated->fbRail.freqkHz           = fbFreqkHzInput;
        pDlppm1xEstimated->fbRail.pwrInTuple.voltuV = pDlppm1xObserved->fbRail.super.pwrInTuple.voltuV;
        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfPwrModelFbVoltageLookup_DLPPM_1X(
                pDlppm1x,
                pDlppm1xEstimated->fbRail.freqkHz,
                &pDlppm1xEstimated->fbRail.voltuV),
            perfCfPwrModelScale_DLPPM_1X_exit);
        pDlppm1xEstimated->fbRail.pwrOutTuple.voltuV =
            pDlppm1xEstimated->fbRail.voltuV;

        //
        // Small rails' power does not change materially with respect to the
        // shmooed metrics (i.e. core clocks and DRAMCLK) that the estimated
        // metrics are computed for. Set the estimated small rail power metrics
        // to what was observed.
        //
        pDlppm1xEstimated->smallRailPwrTuple = pDlppm1xObserved->smallRailPwrTuple;
    }

    perfCfPwrModelDlppm1xScaleProfileRegionEnd(
        pProfiling,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_DLPPM_1X_SCALE_PROFILING_REGION_METRICS_POPULATE);

    perfCfPwrModelDlppm1xScaleProfileRegionBegin(
        pProfiling,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_DLPPM_1X_SCALE_PROFILING_REGION_NNE_INPUTS_SET);

    // Populate the static inputs.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelDlppm1xNneStaticInputsSet(
            pDlppm1x,
            pDlppm1xObserved,
            &inferenceStage),
        perfCfPwrModelScale_DLPPM_1X_exit);

    // Populate the dynamic inputs.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelDlppm1xNneLoopInputsSet(
            pDlppm1x,
            pDlppm1xObserved,
            numMetricsInputs,
            ppEstimatedMetrics,
            &inferenceStage),
        perfCfPwrModelScale_DLPPM_1X_exit);

    perfCfPwrModelDlppm1xScaleProfileRegionEnd(
        pProfiling,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_DLPPM_1X_SCALE_PROFILING_REGION_NNE_INPUTS_SET);

    perfCfPwrModelDlppm1xScaleProfileRegionBegin(
        pProfiling,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_DLPPM_1X_SCALE_PROFILING_REGION_NNE_EVALUATE);

    // Ilwoke NNE to evaluate the inference request.
    PMU_ASSERT_OK_OR_GOTO(status,
        nneDescInferenceClientEvaluate(&inferenceStage),
        perfCfPwrModelScale_DLPPM_1X_exit);

    perfCfPwrModelDlppm1xScaleProfileRegionEnd(
        pProfiling,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_DLPPM_1X_SCALE_PROFILING_REGION_NNE_EVALUATE);

    // Copy out the NNE_DESC inference profiling data.
    perfCfPwrModelDlppm1xScaleProfileNneDescInferenceProfilingSet(
        pProfiling, &inferenceStage.hdr.profiling);

    // Copy out the caching information from the inference.
    pDlppm1xObserved->staticVarCache = inferenceStage.hdr.staticVarCache;

    // Copy out outputs.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelDlppm1xNneOutputsParseAndGuardRail(
            pDlppm1x,
            &inferenceStage,
            &pDlppm1xParams->bounds,
            pDlppm1xObserved,
            numMetricsInputs,
            ppEstimatedMetrics,
            pProfiling),
    perfCfPwrModelScale_DLPPM_1X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelDlppm1xNneOutputsPropagate(
            pDlppm1x,
            pDlppm1xObserved,
            numMetricsInputs,
            ppEstimatedMetrics),
        perfCfPwrModelScale_DLPPM_1X_exit);

    //
    // Have successfully completed the function, so now all of the metrics are
    // valid.
    //
    for (loopIdx = 0; loopIdx < numMetricsInputs; loopIdx++)
    {
        ppEstimatedMetrics[loopIdx]->bValid = LW_TRUE;
    }

perfCfPwrModelScale_DLPPM_1X_exit:
    perfCfPwrModelDlppm1xScaleProfileRegionEnd(
        pProfiling,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_DLPPM_1X_SCALE_PROFILING_REGION_TOTAL);

    return status;
}

/*!
 * @copydoc PerfCfPwrModelScalePrimaryClkRange
 */
FLCN_STATUS
perfCfPwrModelScalePrimaryClkRange_DLPPM_1X
(
    PERF_CF_PWR_MODEL *pPwrModel,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS *pObservedMetrics,
    LwU32 dramclkkHz,
    PERF_LIMITS_VF_RANGE *pPrimaryClkRangekHz,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS **ppEstimatedMetrics,
    LwU8 *pNumEstimatedMetrics,
    LwBool *pBMorePoints,
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS *pTypeParams
)
{
    const PERF_CF_PWR_MODEL_DLPPM_1X *const pDlppm1x =
        (const PERF_CF_PWR_MODEL_DLPPM_1X *)pPwrModel;
    const PERF_CF_PWR_MODEL_DLPPM_1X_CORE_RAIL *pCoreRail;
    PERF_LIMITS_VF             vfDomain;
    PERF_LIMITS_PSTATE_RANGE   pstateRange;
    CLK_DOMAIN                *pDomain;
    CLK_DOMAIN_PROG           *pDomainProg;
    LwU32                      stepkHz;
    LwU32                      scalingFactorkHz;
    LwU8                       idx;
    LwU8                       numEstimatedPoints;
    LwU32                      coreFreqkHzMetricInput = 0;
    FLCN_STATUS                status = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        (pPwrModel != NULL) &&
        (pObservedMetrics != NULL) &&
        (pPrimaryClkRangekHz != NULL) &&
        (ppEstimatedMetrics != NULL) &&
        (pNumEstimatedMetrics != NULL) &&
        (*pNumEstimatedMetrics >= pDlppm1x->vfInferenceBatchSize) &&
        (pBMorePoints != NULL) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
        perfCfPwrModelScalePrimaryClkRange_DLPPM_1X_exit);

    // Reset the scratch buffer
    for (idx = 0; idx < LW2080_CTRL_NNE_NNE_DESC_INFERENCE_LOOPS_MAX; idx++)
    {
        perfCfPwrModelScaleMeticsInputInit(&PerfCfPwrModelScaleInputs[idx]);
    }

    pCoreRail = &pDlppm1x->coreRail;

    // Get the PSTATE range for this DRAMCLK
    vfDomain.idx   = pDlppm1x->fbRail.super.clkDomIdx;
    vfDomain.value = dramclkkHz;
    PMU_ASSERT_OK_OR_GOTO(status,
        perfLimitsFreqkHzToPstateIdxRange(&vfDomain,
                                          &pstateRange),
        perfCfPwrModelScalePrimaryClkRange_DLPPM_1X_exit);

    // Quantize-up the lower bound of the primary clock.
    vfDomain.idx   = pCoreRail->pRails[0].clkDomIdx;
    vfDomain.value = pPrimaryClkRangekHz->values[PERF_LIMITS_VF_RANGE_IDX_MIN];
    PMU_ASSERT_OK_OR_GOTO(status,
        perfLimitsFreqkHzQuantize(&vfDomain,
                                  &pstateRange,
                                  LW_FALSE), // Quantize up
        perfCfPwrModelScalePrimaryClkRange_DLPPM_1X_exit);
    pPrimaryClkRangekHz->values[PERF_LIMITS_VF_RANGE_IDX_MIN] = vfDomain.value;

    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfPwrModelScaleMeticsInputSetFreqkHz(
            &PerfCfPwrModelScaleInputs[0],
            pCoreRail->pRails[0].clkDomIdx,
            vfDomain.value),
        perfCfPwrModelScalePrimaryClkRange_DLPPM_1X_exit);

    // Quantize-down the upper bound of the primary clock.
    vfDomain.value = pPrimaryClkRangekHz->values[PERF_LIMITS_VF_RANGE_IDX_MAX];
    PMU_ASSERT_OK_OR_GOTO(status,
        perfLimitsFreqkHzQuantize(&vfDomain,
                                  &pstateRange,
                                  LW_TRUE), // Quantize down
        perfCfPwrModelScalePrimaryClkRange_DLPPM_1X_exit);
    pPrimaryClkRangekHz->values[PERF_LIMITS_VF_RANGE_IDX_MAX] = vfDomain.value;

    //
    // If primaryClkRangekHz overlaps after quantization, then no more
    // points are available within.  Can early terminate and tell
    // caller that no more points are left.
    //
    if (!PERF_LIMITS_RANGE_IS_COHERENT(pPrimaryClkRangekHz))
    {
        *pNumEstimatedMetrics = 0;
        *pBMorePoints = LW_FALSE;
        goto perfCfPwrModelScalePrimaryClkRange_DLPPM_1X_exit;
    }

    //
    // Compute the primary clock step size, assuming the primary clock frequencies we want to
    // estimate perf/power for are evenly spaced between quantized @ref pPrimaryClkRangekHz.
    //
    // Note: If we are to sample @ref PERF_CF_PWR_MODEL_DLPPM_1X::vfInferenceBatchSize number
    //       of VF points, then the number of regions is one less than that.
    //
    stepkHz = (pPrimaryClkRangekHz->values[PERF_LIMITS_VF_RANGE_IDX_MAX] -
                   pPrimaryClkRangekHz->values[PERF_LIMITS_VF_RANGE_IDX_MIN]) /
              (pDlppm1x->vfInferenceBatchSize - 1);

    // Build the set of quantized primary clock frequencies to examine.
    numEstimatedPoints = 1;
    for (idx = 1; idx < (pDlppm1x->vfInferenceBatchSize - 1U); idx++)
    {
        LwU32 coreFreqkHz            = 0;
        LwU32 coreFreqkHzMetricInput = 0;

        PMU_ASSERT_OK_OR_GOTO(status,
            perfCfPwrModelScaleMeticsInputGetFreqkHz(
                &PerfCfPwrModelScaleInputs[0],
                pCoreRail->pRails[0].clkDomIdx,
                &coreFreqkHz),
            perfCfPwrModelScalePrimaryClkRange_DLPPM_1X_exit);

        PMU_ASSERT_OK_OR_GOTO(status,
            perfCfPwrModelScaleMeticsInputGetFreqkHz(
                &PerfCfPwrModelScaleInputs[numEstimatedPoints - 1],
                pCoreRail->pRails[0].clkDomIdx,
                &coreFreqkHzMetricInput),
            perfCfPwrModelScalePrimaryClkRange_DLPPM_1X_exit);

        // Compute the primary clock frequency to examine.
        vfDomain.value = (idx * stepkHz) + coreFreqkHz;

        // Quantize the primary clock frequency.
        PMU_ASSERT_OK_OR_GOTO(status,
            perfLimitsFreqkHzQuantize(&vfDomain,
                                      NULL,     // Do not quantize by PSTATE
                                      LW_TRUE), // Quantize down
            perfCfPwrModelScalePrimaryClkRange_DLPPM_1X_exit);

        // If the point is unique, add it to the set of points we're going to estimate)
        if (vfDomain.value != coreFreqkHzMetricInput)
        {
            // Add it to the set of inputs.
            PMU_ASSERT_OK_OR_GOTO(status,
                perfCfPwrModelScaleMeticsInputSetFreqkHz(
                    &PerfCfPwrModelScaleInputs[numEstimatedPoints],
                    pCoreRail->pRails[0].clkDomIdx,
                    vfDomain.value),
                perfCfPwrModelScalePrimaryClkRange_DLPPM_1X_exit);
            numEstimatedPoints++;
        }
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfPwrModelScaleMeticsInputGetFreqkHz(
            &PerfCfPwrModelScaleInputs[numEstimatedPoints - 1],
            pCoreRail->pRails[0].clkDomIdx,
            &coreFreqkHzMetricInput),
        perfCfPwrModelScalePrimaryClkRange_DLPPM_1X_exit);

    // Specifically handle the upper bound.
    if (pPrimaryClkRangekHz->values[PERF_LIMITS_VF_RANGE_IDX_MAX] != coreFreqkHzMetricInput)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            perfCfPwrModelScaleMeticsInputSetFreqkHz(
                &PerfCfPwrModelScaleInputs[numEstimatedPoints],
                pCoreRail->pRails[0].clkDomIdx,
                pPrimaryClkRangekHz->values[PERF_LIMITS_VF_RANGE_IDX_MAX]),
            perfCfPwrModelScalePrimaryClkRange_DLPPM_1X_exit);

        numEstimatedPoints++;
    }
    *pNumEstimatedMetrics = numEstimatedPoints;

    //
    // Propagate the unique quantized primary clocks to all other core clocks to
    // build the set of input core clock sets to estimate perf/power at.
    //
    if (pCoreRail->numRails > 1)
    {
        PERF_LIMITS_VF_ARRAY vfArrayIn;
        PERF_LIMITS_VF_ARRAY vfArrayOut;
        LwU8 coreRailIdx;

        PERF_LIMITS_VF_ARRAY_INIT(&vfArrayIn);
        boardObjGrpMaskBitSet(vfArrayIn.pMask, pCoreRail->pRails[0].clkDomIdx);

        PERF_LIMITS_VF_ARRAY_INIT(&vfArrayOut);
        for (coreRailIdx = 1; coreRailIdx < pCoreRail->numRails; coreRailIdx++)
        {
            boardObjGrpMaskBitSet(vfArrayOut.pMask, pCoreRail->pRails[coreRailIdx].clkDomIdx);
        }

        for (idx = 0; idx < numEstimatedPoints; idx++)
        {
            LwU32 coreFreqkHz = 0;

            PMU_ASSERT_OK_OR_GOTO(status,
                perfCfPwrModelScaleMeticsInputGetFreqkHz(
                    &PerfCfPwrModelScaleInputs[idx],
                    pCoreRail->pRails[0].clkDomIdx,
                    &coreFreqkHz),
                perfCfPwrModelScalePrimaryClkRange_DLPPM_1X_exit);

            //
            // Propagate primary clock to all other core rail clocks.
            //
            // TODO: Floor propagation to MSVDD to MSVDD's Vmin when we add
            // split-rail support for DLPPE.
            //
            vfArrayIn.values[pCoreRail->pRails[0].clkDomIdx] = coreFreqkHz;
            PMU_ASSERT_OK_OR_GOTO(status,
                perfLimitsFreqPropagate(&pstateRange,
                    &vfArrayIn, &vfArrayOut,
                    LW2080_CTRL_CLK_CLK_PROP_TOP_IDX_ILWALID),
                perfCfPwrModelScalePrimaryClkRange_DLPPM_1X_exit);

            // Copy out the other core rail frequencies propagated from the primary rail.
            for (coreRailIdx = 1; coreRailIdx < pCoreRail->numRails; coreRailIdx++)
            {
                PMU_ASSERT_OK_OR_GOTO(status,
                    perfCfPwrModelScaleMeticsInputSetFreqkHz(
                        &PerfCfPwrModelScaleInputs[idx],
                        pCoreRail->pRails[coreRailIdx].clkDomIdx,
                        vfArrayOut.values[pCoreRail->pRails[coreRailIdx].clkDomIdx]),
                    perfCfPwrModelScalePrimaryClkRange_DLPPM_1X_exit);
            }
        }
    }

    for (idx = 0; idx < numEstimatedPoints; idx++)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            perfCfPwrModelScaleMeticsInputSetFreqkHz(
                &PerfCfPwrModelScaleInputs[idx],
                pDlppm1x->fbRail.super.clkDomIdx,
                dramclkkHz),
            perfCfPwrModelScalePrimaryClkRange_DLPPM_1X_exit);
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfPwrModelScale(pPwrModel,
                            pObservedMetrics,
                            numEstimatedPoints,
                            PerfCfPwrModelScaleInputs,
                            ppEstimatedMetrics,
                            pTypeParams),
        perfCfPwrModelScalePrimaryClkRange_DLPPM_1X_exit);

    //
    // Check if we've fully explored all quantized points bounded by
    // @ref pPrimaryClkRangekHz.
    //
    pDomain = CLK_DOMAIN_GET(pCoreRail->pRails[0].clkDomIdx);
    PMU_ASSERT_OK_OR_GOTO(status,
        (pDomain == NULL) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
        perfCfPwrModelScalePrimaryClkRange_DLPPM_1X_exit);

    pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
    PMU_ASSERT_OK_OR_GOTO(status,
        (pDomainProg == NULL) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
        perfCfPwrModelScalePrimaryClkRange_DLPPM_1X_exit);

    scalingFactorkHz = clkDomainFreqkHzScaleGet(pDomain);
    *pBMorePoints = (clkDomainProgGetIndexByFreqMHz(
                         pDomainProg,
                         pPrimaryClkRangekHz->values[PERF_LIMITS_VF_RANGE_IDX_MAX] / scalingFactorkHz,
                         LW_TRUE) -
                     clkDomainProgGetIndexByFreqMHz(
                         pDomainProg,
                         pPrimaryClkRangekHz->values[PERF_LIMITS_VF_RANGE_IDX_MIN] / scalingFactorkHz,
                         LW_FALSE) + 1) >
                    numEstimatedPoints;

perfCfPwrModelScalePrimaryClkRange_DLPPM_1X_exit:
    return status;
}

/*!
 * @copydoc PerfCfPwrModelLoad
 */
FLCN_STATUS
perfCfPwrModelLoad_DLPPM_1X
(
    PERF_CF_PWR_MODEL *pPwrModel
)
{
    PERF_CF_PWR_MODEL_DLPPM_1X   *pDlppm1x = (PERF_CF_PWR_MODEL_DLPPM_1X *)pPwrModel;
    CLK_PROP_REGIME              *pPropRegime;
    VOLT_RAIL                    *pRail;
    BOARDOBJGRPMASK_E32           tmpMask;
    BOARDOBJGRPMASK_E32          *pClkDomMask;
    LwU8                          coreRailIdx;
    FLCN_STATUS                   status   = FLCN_OK;
    VPSTATE_3X                   *pVpstate3x;
    CLK_DOMAIN                   *pDomainDram;
    CLK_DOMAIN_PROG              *pDomainProgDram;
    LwBool                        bFbvddDataValid;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        (pPwrModel == NULL) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        perfCfPwrModelLoad_DLPPM_1X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        (!PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROP_REGIME)) ? FLCN_ERR_NOT_SUPPORTED : FLCN_OK,
        perfCfPwrModelLoad_DLPPM_1X_exit);

    pVpstate3x = (VPSTATE_3X *)vpstateGetBySemanticIdx(
        RM_PMU_PERF_VPSTATE_IDX_DLPPM_1X_ESTIMATION_MINIMUM);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        pVpstate3x != NULL,
        FLCN_ERR_ILWALID_STATE,
        perfCfPwrModelLoad_DLPPM_1X_exit);

    // Ensure we have one way to get output power when observing fbRail's telemetry
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDlppm1x->fbRail.super.outPwrChIdx != LW2080_CTRL_BOARDOBJ_IDX_ILWALID ||
        pDlppm1x->fbRail.super.vrEfficiencyChRelIdxInToOut != LW2080_CTRL_BOARDOBJ_IDX_ILWALID),
        FLCN_ERR_ILWALID_STATE,
        perfCfPwrModelLoad_DLPPM_1X_exit);

    // Ensure the VR efficiency channel relationship for FB Out -> In power is valid
    PMU_ASSERT_TRUE_OR_GOTO(status,
        pDlppm1x->fbRail.super.vrEfficiencyChRelIdxOutToIn != LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
        FLCN_ERR_ILWALID_STATE,
        perfCfPwrModelLoad_DLPPM_1X_exit);

    //
    // Ensure that the fbRail CLK_DOMAIN is available and that it has FBVDD data
    // available.
    //
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pDomainDram = BOARDOBJGRP_OBJ_GET(
                CLK_DOMAIN, pDlppm1x->fbRail.super.clkDomIdx)) != NULL),
        FLCN_ERR_ILWALID_STATE,
        perfCfPwrModelLoad_DLPPM_1X_exit);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pDomainProgDram = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(
                pDomainDram, PROG)) != NULL),
        FLCN_ERR_ILWALID_STATE,
        perfCfPwrModelLoad_DLPPM_1X_exit);
    PMU_ASSERT_OK_OR_GOTO(status,
        clkDomainProgFbvddDataValid(pDomainProgDram, &bFbvddDataValid),
        perfCfPwrModelLoad_DLPPM_1X_exit);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        bFbvddDataValid,
        FLCN_ERR_ILWALID_STATE,
        perfCfPwrModelLoad_DLPPM_1X_exit);

    //
    // Observe the independent clock domains that reside on each core rail, excluding
    // the clock domains that DLPPM_1X will shmoo (i.e. GPCCLK, DRAMCLK) and their
    // secondary clock domains.
    //
    // TODO: Update this logic for split rail.
    //
    for (coreRailIdx = 0; coreRailIdx < pDlppm1x->coreRail.numRails; coreRailIdx++)
    {
        boardObjGrpMaskInit_E32(&(pDlppm1x->coreRail.pRails[coreRailIdx].independentClkDomMask));
        boardObjGrpMaskInit_E32(&tmpMask);

        // Get the mask of GPCCLK and DRAMCLK secondary clock domains.
        pPropRegime = clkPropRegimesGetByRegimeId(LW2080_CTRL_CLK_CLK_PROP_REGIME_ID_GPC_STRICT);
        PMU_ASSERT_OK_OR_GOTO(status,
            (pPropRegime == NULL) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
            perfCfPwrModelLoad_DLPPM_1X_exit);

        pClkDomMask = clkPropRegimeClkDomainMaskGet(pPropRegime);
        PMU_ASSERT_OK_OR_GOTO(status,
            (pClkDomMask == NULL) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
            perfCfPwrModelLoad_DLPPM_1X_exit);

        PMU_ASSERT_OK_OR_GOTO(status,
            boardObjGrpMaskCopy(&tmpMask, pClkDomMask),
            perfCfPwrModelLoad_DLPPM_1X_exit);

        pPropRegime = clkPropRegimesGetByRegimeId(LW2080_CTRL_CLK_CLK_PROP_REGIME_ID_DRAM_LOCK);
        PMU_ASSERT_OK_OR_GOTO(status,
            (pPropRegime == NULL) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
            perfCfPwrModelLoad_DLPPM_1X_exit);

        pClkDomMask = clkPropRegimeClkDomainMaskGet(pPropRegime);
        PMU_ASSERT_OK_OR_GOTO(status,
            (pClkDomMask == NULL) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
            perfCfPwrModelLoad_DLPPM_1X_exit);

        PMU_ASSERT_OK_OR_GOTO(status,
            boardObjGrpMaskOr(&tmpMask, &tmpMask, pClkDomMask),
            perfCfPwrModelLoad_DLPPM_1X_exit);

        //
        // Ilwert to get a mask of clock domains that are not GPCCLK or DRAMCLK
        // secondaries.
        //
        PMU_ASSERT_OK_OR_GOTO(status,
            boardObjGrpMaskIlw(&tmpMask),
            perfCfPwrModelLoad_DLPPM_1X_exit);

        //
        // Build mask of independent clock domains that are not GPCCLK or DRAMCLK
        // secondaries.
        //
        pRail = VOLT_RAIL_GET(pDlppm1x->coreRail.pRails[coreRailIdx].voltRailIdx);
        PMU_ASSERT_OK_OR_GOTO(status,
            (pRail == NULL) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
            perfCfPwrModelLoad_DLPPM_1X_exit);

        pClkDomMask = voltRailGetClkDomainsProgMask(pRail);
        PMU_ASSERT_OK_OR_GOTO(status,
            (pClkDomMask == NULL) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
            perfCfPwrModelLoad_DLPPM_1X_exit);

        PMU_ASSERT_OK_OR_GOTO(status,
            boardObjGrpMaskAnd(&(pDlppm1x->coreRail.pRails[coreRailIdx].independentClkDomMask),
                               pClkDomMask,
                               &tmpMask),
            perfCfPwrModelLoad_DLPPM_1X_exit);

        //
        // If the core rail has a valid input power channel, ensure that there is a way to get output power
        // either through a sensor or through a valid VR efficiency channel relationship AND there is a valid VR
        // efficiency channel relationship idx to colwert from output to input power.
        //
        if (pDlppm1x->coreRail.pRails[coreRailIdx].inPwrChIdx != LW2080_CTRL_BOARDOBJ_IDX_ILWALID)
        {
            PMU_ASSERT_TRUE_OR_GOTO(status,
                (pDlppm1x->coreRail.pRails[coreRailIdx].outPwrChIdx != LW2080_CTRL_BOARDOBJ_IDX_ILWALID ||
                pDlppm1x->coreRail.pRails[coreRailIdx].vrEfficiencyChRelIdxInToOut != LW2080_CTRL_BOARDOBJ_IDX_ILWALID),
                FLCN_ERR_ILWALID_STATE,
                perfCfPwrModelLoad_DLPPM_1X_exit);

            PMU_ASSERT_TRUE_OR_GOTO(status,
                pDlppm1x->coreRail.pRails[coreRailIdx].vrEfficiencyChRelIdxOutToIn != LW2080_CTRL_BOARDOBJ_IDX_ILWALID,
                FLCN_ERR_ILWALID_STATE,
                perfCfPwrModelLoad_DLPPM_1X_exit);
        }
    }

perfCfPwrModelLoad_DLPPM_1X_exit:
    return status;
}

/*!
 * @copydoc PerfCfPwrModelObservedMetricsInit
 */
FLCN_STATUS
perfCfPwrModelObservedMetricsInit_DLPPM_1X
(
    PERF_CF_PWR_MODEL                            *pPwrModel,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS   *pMetrics
)
{
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED
       *pDlppm1xObserved = (LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *)pMetrics;
    PERF_CF_PWR_MODEL_DLPPM_1X   *pDlppm1x = (PERF_CF_PWR_MODEL_DLPPM_1X *)pPwrModel;
    VOLT_RAIL                    *pVoltRail;
    LwU8                          coreRailIdx;
    FLCN_STATUS                   status = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pPwrModel == NULL) ||
         (pMetrics  == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        perfCfPwrModelObservedMetricsInit_DLPPM_1X_exit);

    //
    // s_perfCfPwrModelDlppm1xObservedMetricsReInit assumes that the observed
    // voltData has been initialized, so do that here.
    //
    for (coreRailIdx = 0; coreRailIdx < pDlppm1x->coreRail.numRails; coreRailIdx++)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            voltRailSensedVoltageDataCopyOut(
                &pDlppm1xObserved->coreRail.rails[coreRailIdx].voltData,
                &pDlppm1x->coreRail.pRails[coreRailIdx].voltData),
            perfCfPwrModelObservedMetricsInit_DLPPM_1X_exit);
    }

    s_perfCfPwrModelDlppm1xObservedMetricsReInit(
        pDlppm1x,
        pDlppm1xObserved);

    // Do the initial voltage sensing for all core rails.
    for (coreRailIdx = 0; coreRailIdx < pDlppm1x->coreRail.numRails; coreRailIdx++)
    {
        pVoltRail = VOLT_RAIL_GET(pDlppm1x->coreRail.pRails[coreRailIdx].voltRailIdx);
        PMU_ASSERT_OK_OR_GOTO(status,
            (pVoltRail == NULL) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
            perfCfPwrModelObservedMetricsInit_DLPPM_1X_exit);

        PMU_ASSERT_OK_OR_GOTO(status,
            voltRailGetVoltageSensed(
                pVoltRail,
                &(pDlppm1x->coreRail.pRails[coreRailIdx].voltData)),
            perfCfPwrModelObservedMetricsInit_DLPPM_1X_exit);

        PMU_ASSERT_OK_OR_GOTO(status,
            voltRailSensedVoltageDataCopyOut(
                &(pDlppm1xObserved->coreRail.rails[coreRailIdx].voltData),
                &(pDlppm1x->coreRail.pRails[coreRailIdx].voltData)),
            perfCfPwrModelObservedMetricsInit_DLPPM_1X_exit);
    }

perfCfPwrModelObservedMetricsInit_DLPPM_1X_exit:
    return status;
}

/*!
 * @copydoc perfCfPwrModelScaleTypeParamsInit_DLPPM_1X
 */
FLCN_STATUS
perfCfPwrModelScaleTypeParamsInit_DLPPM_1X
(
    PERF_CF_PWR_MODEL *pPwrModel,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS *pCtrlParams,
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS *pTypeParams
)
{
    FLCN_STATUS status;
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_DLPPM_1X *pDlppm1xParams =
        (PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_DLPPM_1X *)pTypeParams;

    PMU_ASSERT_OK_OR_GOTO(status,
        (pPwrModel != NULL) &&
        (pCtrlParams != NULL) &&
        (pTypeParams != NULL) &&
        (pCtrlParams->type == LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_DLPPM_1X) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
        perfCfPwrModelScaleTypeParamsInit_DLPPM_1X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfPwrModelScaleTypeParamsInit_SUPER(
            pPwrModel,
            pCtrlParams,
            pTypeParams),
        perfCfPwrModelScaleTypeParamsInit_DLPPM_1X_exit);

    // Initialize the internal bounds structure
    if (pCtrlParams->data.dlppm1x.bounds.bEndpointsValid)
    {
        pDlppm1xParams->bounds.pEndpointLo = &pCtrlParams->data.dlppm1x.bounds.endpointLo;
        pDlppm1xParams->bounds.pEndpointHi = &pCtrlParams->data.dlppm1x.bounds.endpointHi;
    }
    else
    {
        pDlppm1xParams->bounds.pEndpointLo = NULL;
        pDlppm1xParams->bounds.pEndpointHi = NULL;
    }

    pDlppm1xParams->bounds.bGuardRailsApply = pCtrlParams->data.dlppm1x.bounds.bGuardRailsApply;

    pDlppm1xParams->pProfiling = &pCtrlParams->data.dlppm1x.profiling;

perfCfPwrModelScaleTypeParamsInit_DLPPM_1X_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @copydoc s_perfCfPwrModelRailCopy_DLPPM_1X
 */
static FLCN_STATUS
s_perfCfPwrModelRailCopy_DLPPM_1X
(
    PERF_CF_PWR_MODEL_DLPPM_1X_RAIL                    *pDest,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_DLPPM_1X_RAIL   *pSrc
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pDest == NULL) ||
         (pSrc  == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        s_perfCfPwrModelRailCopy_DLPPM_1X_exit);

    // Copy all members.
    pDest->voltRailIdx                 = pSrc->voltRailIdx;
    pDest->clkDomIdx                   = pSrc->clkDomIdx;
    pDest->clkDomTopIdx                = pSrc->clkDomTopIdx;
    pDest->inPwrChIdx                  = pSrc->inPwrChIdx;
    pDest->outPwrChIdx                 = pSrc->outPwrChIdx;
    pDest->vrEfficiencyChRelIdxInToOut = pSrc->vrEfficiencyChRelIdxInToOut;
    pDest->vrEfficiencyChRelIdxOutToIn = pSrc->vrEfficiencyChRelIdxOutToIn;

    // Import the independent clock domain mask.
    boardObjGrpMaskInit_E32(&(pDest->independentClkDomMask));
    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpMaskImport_E32(&(pDest->independentClkDomMask),
                                  &(pSrc->independentClkDomMask)),
        s_perfCfPwrModelRailCopy_DLPPM_1X_exit);

s_perfCfPwrModelRailCopy_DLPPM_1X_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelCoreRailCopy_DLPPM_1X
 */
static FLCN_STATUS
s_perfCfPwrModelCoreRailCopy_DLPPM_1X
(
    PERF_CF_PWR_MODEL_DLPPM_1X_CORE_RAIL                    *pDest,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_DLPPM_1X_CORE_RAIL   *pSrc
)
{
    VOLT_RAIL  *pRail;
    LwU8        coreRailIdx;
    FLCN_STATUS status = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pDest == NULL) ||
         (pSrc  == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        s_perfCfPwrModelCoreRailCopy_DLPPM_1X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        (pSrc->numRails > LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_DLPPM_1X_MAX_CORE_RAILS) ?
            FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        s_perfCfPwrModelCoreRailCopy_DLPPM_1X_exit);

    // Allocate the rails array if it hasn't already been.
    if (pDest->pRails == NULL)
    {
        pDest->numRails = pSrc->numRails;
        pDest->pRails   =
            lwosCallocType(OVL_INDEX_DMEM(perfCfModel), pDest->numRails,
                           PERF_CF_PWR_MODEL_DLPPM_1X_RAIL);
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pDest->pRails != NULL),
            FLCN_ERR_NO_FREE_MEM,
            s_perfCfPwrModelCoreRailCopy_DLPPM_1X_exit);
    }
    else
    {
        // If the rails array
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pDest->numRails == pSrc->numRails),
            FLCN_ERR_ILWALID_ARGUMENT,
            s_perfCfPwrModelCoreRailCopy_DLPPM_1X_exit);
    }

    for (coreRailIdx = 0; coreRailIdx < pDest->numRails; coreRailIdx++)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfPwrModelRailCopy_DLPPM_1X(&(pDest->pRails[coreRailIdx]),
                                              &(pSrc->rails[coreRailIdx])),
            s_perfCfPwrModelCoreRailCopy_DLPPM_1X_exit);

        // Allocate the ADC sample buffer, if it has not already been done.
        if (pDest->pRails[coreRailIdx].voltData.pClkAdcAccSample == NULL)
        {
            pRail = VOLT_RAIL_GET(pDest->pRails[coreRailIdx].voltRailIdx);
            PMU_ASSERT_OK_OR_GOTO(status,
                pRail == NULL ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
                s_perfCfPwrModelCoreRailCopy_DLPPM_1X_exit);

            PMU_ASSERT_OK_OR_GOTO(status,
                voltRailSensedVoltageDataConstruct(
                    &(pDest->pRails[coreRailIdx].voltData),
                    pRail,
                    pDest->pRails[coreRailIdx].voltMode,
                    OVL_INDEX_DMEM(perfCf)),
                s_perfCfPwrModelCoreRailCopy_DLPPM_1X_exit);
        }
    }

s_perfCfPwrModelCoreRailCopy_DLPPM_1X_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelFbvddRailCopy_DLPPM_1X
 */
static FLCN_STATUS
s_perfCfPwrModelFbvddRailCopy_DLPPM_1X
(
    PERF_CF_PWR_MODEL_DLPPM_1X_FBVDD_RAIL                    *pDest,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_DLPPM_1X_FBVDD_RAIL   *pSrc
)
{
    FLCN_STATUS   status = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
    ((pDest == NULL) ||
     (pSrc  == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
    s_perfCfPwrModelFbvddRailCopy_DLPPM_1X_exit);

    // Call the super class.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelRailCopy_DLPPM_1X(&(pDest->super),
                                          &(pSrc->super)),
        s_perfCfPwrModelFbvddRailCopy_DLPPM_1X_exit);

    // Set type-specific member variables.
    pDest->voltRailName = pSrc->voltRailName;

s_perfCfPwrModelFbvddRailCopy_DLPPM_1X_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelObservePerfMetrics_DLPPM_1X
 */
static FLCN_STATUS
s_perfCfPwrModelObservePerfMetrics_DLPPM_1X
(
    const PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
    const PERF_CF_PWR_MODEL_SAMPLE_DATA *pSampleData,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
    LwBool *pbWithinBounds
)
{
    LwUFXP40_24 gpcClkCounts;
    LwUFXP40_24 coreClkUtilPct40_24;
    LwUFXP40_24 perfns;
    FLCN_STATUS status = FLCN_OK;

    PMU_ASSERT_OK_OR_GOTO(status, (pbWithinBounds != NULL) ? FLCN_OK:FLCN_ERR_ILWALID_ARGUMENT,
        s_perfCfPwrModelObservePerfMetrics_DLPPM_1X_exit);

    // Default bWithinBounds to be LW_TRUE
    *pbWithinBounds = LW_TRUE;

    LwU64_ALIGN32_UNPACK(
        &gpcClkCounts,
        &pDlppm1xObserved->pmSensorDiff[pDlppm1x->gpcclkPmIndex]);

    LwU64_ALIGN32_UNPACK(
        &perfns,
        &pSampleData->pPerfTopoStatus->topologys[pDlppm1x->ptimerTopologyIdx].topology.reading);

    //
    // If util is valid and bFakeGpcClkCounts == LW_TRUE, then change
    // the gpcclk counts in  DLPPM_1X_OBSERVED::pmSensorDiff[gpcclkIdx]
    // to be PTIMER * util * numGpc * gpcclk_freq. Doing so will encode
    // idleness of the GPU into the data fed into NNE.
    //

    LwU64_ALIGN32_UNPACK(
        &coreClkUtilPct40_24,
        &pDlppm1xObserved->coreRail.rails[0].super.utilPct);

    if (!lw64CmpEQ(&coreClkUtilPct40_24, &(LwU64){ LW_U64_MAX }) &&
        pDlppm1x->bFakeGpcClkCounts)
    {

        LwUFXP40_24 perfms;

        //
        // Unpack the perfTimer value, which is a 40.24 FXP, into perfns.
        // Divide by 10^6 to colwert from ns to ms
        //

        lwU64Div(&perfms, &perfns, &(LwU64){ 1000U * 1000U });

        //
        // Numerical Analysis:
        //
        // We expect coreClkUtilPct40_24 to be in the range [0, 1], making it a
        // 1.24.
        //
        // We expect perfMs to be ~200ms. Conservatively we can say that it will be
        // in the range [0,1024]. Making it a 10.24
        //
        // Multiplying coreClkUtilPct40_24 and perfms we will get a 10.48.
        //
        // The 10 bits for the integer are fine but we need to shift by 24 to remove
        // the extra fractional bits and get a scaled perfms by utilization
        //

        perfms = multUFXP64(24, perfms, coreClkUtilPct40_24);

        //
        // To get gpcclkcounts we need to take the scaled perfms and multiply
        // it by the gpc freq lwrrently observed and num gpcs
        //
        // (gpc_freq * numGpc) can be expressed as a 24.0 because the worst case
        // value of the multiplication would be:
        //      highest gpc clk * num gpc = highest val
        //      2100000 * 6 =  12600000
        //
        // multiplying this by the scaled perfms will result in 35.24 FXP value
        //

        lwU64Mul(&gpcClkCounts, &perfms, &(LwU64){pDlppm1xObserved->coreRail.rails[0].super.freqkHz});

        //
        // We can adjust back to a LwU64 by shifting to the right by 24 and removing the remaining
        // fractional bits. In this case, the loss of precision is ok as the BA PM values
        // are expected to be whole counts.
        //
        lw64Lsr(&gpcClkCounts, &gpcClkCounts, 24);
        LwU64_ALIGN32_PACK(
            &pDlppm1xObserved->pmSensorDiff[pDlppm1x->gpcclkPmIndex],
            &gpcClkCounts);
    }

    //
    // Numerical Analysis:
    //
    // We expect gpcClkCounts to be:
    //      NUM_GPC * Highest GPCCLK * Loop time
    // In this case that would ideally be:
    //        6 * 2100000 (2.1GHz) * 200ms = 2520000000
    // Being conservative we can set a max value of 3000000000
    // which fits into 32 bits.
    //
    // This can be shifted to the left by 12 because a 40.12 will have
    // engough integer bits (40 vs 32) to fit the value, and since
    // gpcClkCounts is an integer, there is no loss in precision.
    // In the end the value would be a 32.12 FXP
    //

    lw64Lsl(&gpcClkCounts, &gpcClkCounts, 12);


    // If the GPC rail frequency is 0, mark perfms and perfRatio as invalid
    if (pDlppm1xObserved->coreRail.rails[0].super.freqkHz == 0)
    {
        pDlppm1xObserved->perfMetrics.perfms    = LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_PERF_PERFMS_ILWALID;
        pDlppm1xObserved->perfMetrics.perfRatio = LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_PERF_PERF_RATIO_ILWALID;
        *pbWithinBounds = LW_FALSE;
    }
    else
    {
        LwUFXP52_12 perfmsFloor;
        LwUFXP52_12 perfms;

        //
        // To get perfMs we want to divide gpcClkCounts40_24 by gpcFreq
        //
        // Since gpcClkCounts is a 32.12 and gpcFreq a x.0 number,
        // dividing the two would result in "8.12" FXP
        //

        lwU64Div(&perfms, &gpcClkCounts, &(LwU64){pDlppm1xObserved->coreRail.rails[0].super.freqkHz});

        //
        // perfMs should not overflow, but if it does return all Fs
        //
        if (lwU64CmpLE(&perfms, &(LwU64){ LW_U32_MAX }))
        {
            pDlppm1xObserved->perfMetrics.perfms = perfms;
        }
        else
        {
            pDlppm1xObserved->perfMetrics.perfms = LW_U32_MAX;
        }

        //
        // Numerical Analysis:
        // Divide perfns by 10^6 to colwert from ns to ms
        //
        // We expect observedPerfMsMinPct to be in the range [0, 1], making it a
        // 1.12.
        //
        // We expect perfmsFloor to be ~200ms. Conservatively
        // we can say that it will be in the range [0,1024]. Making it a 10.24
        //
        // Multiplying observedPerfMsMinPct and perfmsFloor we will get a 10.36.
        //
        // The 10 bits for the integer are fine but we need to shift by 24 to remove
        // the extra fractional bits, creating a 10.12 value. With this, we can do a
        // direct comparison to perfMs
        //

        lwU64Div(&perfmsFloor, &perfns, &(LwU64){ 1000U * 1000U });

        perfmsFloor = multUFXP64(24, perfmsFloor, pDlppm1x->observedPerfMsMinPct);

        if (perfms < perfmsFloor)
        {
            *pbWithinBounds = LW_FALSE;
        }

        //
        // The ratio represents the improvement over observed, so it is simply "1"
        // for the observed metrics.
        //
        pDlppm1xObserved->perfMetrics.perfRatio = LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 1);
    }
s_perfCfPwrModelObservePerfMetrics_DLPPM_1X_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelObserveInputRailMetrics_DLPPM_1X
 */
static FLCN_STATUS
s_perfCfPwrModelObserveInputRailMetrics_DLPPM_1X
(
    const PERF_CF_PWR_MODEL_SAMPLE_DATA                                 *pSampleData,
    const PERF_CF_PWR_MODEL_DLPPM_1X_RAIL                               *pRail,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_OBSERVED_METRICS_DLPPM_1X_RAIL   *pRailMetrics
)
{
    LwU64         mulgHz2kHz = 1000000;
    LwU64         tmp;
    FLCN_STATUS   status;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pSampleData  == NULL) ||
         (pRail        == NULL) ||
         (pRailMetrics == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
         s_perfCfPwrModelObserveInputRailMetrics_DLPPM_1X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        ((!BOARDOBJGRP_IS_VALID(PERF_CF_TOPOLOGY, pRail->clkDomTopIdx)) ||
         (!BOARDOBJGRP_IS_VALID(PWR_CHANNEL,      pRail->inPwrChIdx))) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
         s_perfCfPwrModelObserveInputRailMetrics_DLPPM_1X_exit);

    // Get the clock frequency
    LwU64_ALIGN32_UNPACK(&tmp, &(pSampleData->pPerfTopoStatus->topologys[pRail->clkDomTopIdx].topology.reading));
    lwU64Mul(&tmp, &tmp, &mulgHz2kHz);
    tmp = LW_TYPES_UFXP_X_Y_TO_U64_ROUNDED(40, 24, tmp);
    PMU_ASSERT_OK_OR_GOTO(status,
        (LwU64_HI32(tmp) != 0) ? FLCN_ERROR : FLCN_OK,
        s_perfCfPwrModelObserveInputRailMetrics_DLPPM_1X_exit);
    pRailMetrics->super.freqkHz = LwU64_LO32(tmp);

    if (pRail->clkDomUtilTopIdx != LW2080_CTRL_BOARDOBJ_IDX_ILWALID)
    {
        pRailMetrics->super.utilPct =
            pSampleData->pPerfTopoStatus->topologys[pRail->clkDomUtilTopIdx].topology.reading;
    }
    else
    {
        // Otherwise, set the utilization to an "invalid" value.
        LwU64_ALIGN32_PACK(
            &pRailMetrics->super.utilPct,
            &(LwU64){ LW_U64_MAX });
    }

    // Copy the rail input power readings.
    pRailMetrics->super.pwrInTuple  = pSampleData->pPwrChannelsStatus->channels[pRail->inPwrChIdx].channel.tuple;

s_perfCfPwrModelObserveInputRailMetrics_DLPPM_1X_exit:
    return status;
}

/*!
 * @brief Helper function to retrieve the metrics of a DLPPM_1X_RAIL on the core
 *        rail
 *
 * @param[IN]  pSampleData    Sampled topology and sensor data from PERF_CF.
 * @param[IN]  pRail          PWR_MODEL_DLPPM_1X_RAIL to collect
 * @param[OUT] pRailMetrics   METRICS_DLPPM_1X structure to set.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pRail or pRailMetrics are NULL.
 * @return Other errors                If internal function return an error.
 * @return FLCN_OK                     If the rail metrics were successfully observed.
 */
static FLCN_STATUS
s_perfCfPwrModelObserveCoreRailMetrics_DLPPM_1X
(
    PERF_CF_PWR_MODEL_SAMPLE_DATA                                       *pSampleData,
    PERF_CF_PWR_MODEL_DLPPM_1X_RAIL                                     *pRail,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_OBSERVED_METRICS_DLPPM_1X_RAIL   *pRailMetrics
)
{
    PERF_LIMITS_VF          vfDomain;
    PERF_LIMITS_VF          vfRail;
    PWR_EQUATION_DYNAMIC   *pDyn;
    VOLT_RAIL              *pVoltRail;
    LwUFXP52_12             tmpUFXP52_12_0;
    LwUFXP52_12             tmpUFXP52_12_1;
    LwBoardObjIdx           clkDomIdx;
    LwU8                    dynamicEquIdx;
    FLCN_STATUS             status     = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pSampleData  == NULL) ||
         (pRail        == NULL) ||
         (pRailMetrics == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
         s_perfCfPwrModelObserveCoreRailMetrics_DLPPM_1X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
         !BOARDOBJGRP_IS_VALID(VOLT_RAIL, pRail->voltRailIdx) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
         s_perfCfPwrModelObserveCoreRailMetrics_DLPPM_1X_exit);

    // Call the super class.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelObserveInputRailMetrics_DLPPM_1X(
            pSampleData,
            pRail,
            pRailMetrics),
        s_perfCfPwrModelObserveCoreRailMetrics_DLPPM_1X_exit);

    // Observe the minimum possible voltage of the rail.
    pVoltRail = VOLT_RAIL_GET(pRail->voltRailIdx);
    PMU_ASSERT_OK_OR_GOTO(status,
        (pVoltRail == NULL) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
        s_perfCfPwrModelObserveCoreRailMetrics_DLPPM_1X_exit);
    pRailMetrics->vminLimituV = pVoltRail->vminLimituV;

    // Observe Vmin of all independent clock domains on this rail.
    pRailMetrics->maxIndependentClkDomVoltMinuV = 0;
    vfRail.idx = pRail->voltRailIdx;
    BOARDOBJGRPMASK_FOR_EACH_BEGIN(&(pRail->independentClkDomMask), clkDomIdx)
    {
        vfDomain.idx   = BOARDOBJ_GRP_IDX_TO_8BIT(clkDomIdx);
        vfDomain.value = perfChangeSeqChangeLastClkFreqkHzGet(clkDomIdx);
        PMU_ASSERT_OK_OR_GOTO(status,
            perfLimitsFreqkHzToVoltageuV(&vfDomain, &vfRail, LW_TRUE),
            s_perfCfPwrModelObserveCoreRailMetrics_DLPPM_1X_exit);

        // Record the Vmin for this clock domain and the max of all the Vmin values.
        pRailMetrics->maxIndependentClkDomVoltMinuV = LW_MAX(pRailMetrics->maxIndependentClkDomVoltMinuV,
                                                             vfRail.value);
    }
    BOARDOBJGRPMASK_FOR_EACH_END;

    // Sense the voltage.
    pVoltRail = VOLT_RAIL_GET(pRail->voltRailIdx);
    PMU_ASSERT_OK_OR_GOTO(status,
        (pVoltRail == NULL) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
         s_perfCfPwrModelObserveCoreRailMetrics_DLPPM_1X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        voltRailSensedVoltageDataCopyIn(
            &(pRail->voltData),
            &(pRailMetrics->voltData)),
        s_perfCfPwrModelObserveCoreRailMetrics_DLPPM_1X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        voltRailGetVoltageSensed(pVoltRail, &(pRail->voltData)),
        s_perfCfPwrModelObserveCoreRailMetrics_DLPPM_1X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        voltRailSensedVoltageDataCopyOut(
            &(pRailMetrics->voltData),
            &(pRail->voltData)),
        s_perfCfPwrModelObserveCoreRailMetrics_DLPPM_1X_exit);

    pRailMetrics->super.voltuV = pRailMetrics->voltData.voltageuV;

    //
    // Handle rail gating. When rail gating is engaged, rail voltage is 0,
    // and therefore both dynamic and leakage power are 0.
    //
    if (pRailMetrics->super.voltuV == 0)
    {
        pRailMetrics->super.pwrOutDynamicNormalizedmW = 0;
        pRailMetrics->super.leakagePwrmW              = 0;
        goto s_perfCfPwrModelObserveCoreRailMetrics_DLPPM_1X_exit;
    }

    // Observe the Output metrics of the core rail
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelDlppm1xObserveOutputRailMetrics(
            pSampleData,
            pRail,
            pRailMetrics),
        s_perfCfPwrModelObserveCoreRailMetrics_DLPPM_1X_exit);

    // Get the leakage power.
    PMU_ASSERT_OK_OR_GOTO(status,
        voltRailGetLeakage(pVoltRail,
                           pRailMetrics->super.voltuV,
                           LW_FALSE,
                           NULL,
                           &(pRailMetrics->super.leakagePwrmW)),
        s_perfCfPwrModelObserveCoreRailMetrics_DLPPM_1X_exit);

    //
    // Since the sensed output power includes the leakage, it should never
    // be the less than the leakage. If it is, set the dynamic normalized power
    // 0 and early return. This is not an error.
    //
    if (pRailMetrics->super.pwrOutTuple.pwrmW < pRailMetrics->super.leakagePwrmW)
    {
        pRailMetrics->super.pwrOutDynamicNormalizedmW = 0;
        goto s_perfCfPwrModelObserveCoreRailMetrics_DLPPM_1X_exit;
    }

    // Compute the normalized output dynamic power.
    dynamicEquIdx = voltRailVoltScaleExpPwrEquIdxGet(pVoltRail);
    PMU_ASSERT_OK_OR_GOTO(status,
        (dynamicEquIdx == LW2080_CTRL_PMGR_PWR_EQUATION_INDEX_ILWALID) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
        s_perfCfPwrModelObserveCoreRailMetrics_DLPPM_1X_exit);

    pDyn = (PWR_EQUATION_DYNAMIC *)PWR_EQUATION_GET(dynamicEquIdx);
    PMU_ASSERT_OK_OR_GOTO(status,
        (pDyn == NULL) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
        s_perfCfPwrModelObserveCoreRailMetrics_DLPPM_1X_exit);

    //
    // Compute the voltage scaling factor.
    //
    // Numerical Analysis:
    //
    // Vmax = 1.5V = 1500000uV => 21-bit integer
    //
    // Colwerting to UFXP52_12 in volts ilwolves a 12-bit left shift,
    // then a division by 1000000.
    //
    // 21-bit integer << 12 = UFXP 21.12 = 33-bits.
    //
    // Division will only decrease the number of bits used. Ergo,
    // the maximum number of bits ever used is 33-bits.
    //
    tmpUFXP52_12_0 = LW_TYPES_U64_TO_UFXP_X_Y_SCALED(52, 12, pRailMetrics->super.voltuV, 1000000);
    PMU_ASSERT_OK_OR_GOTO(status,
        (LW_TYPES_UFXP_X_Y_TO_U64(52, 12, tmpUFXP52_12_0) >
            LW_TYPES_UFXP_INTEGER_MAX(20, 12)) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
        s_perfCfPwrModelObserveCoreRailMetrics_DLPPM_1X_exit);
    tmpUFXP52_12_0 = (LwUFXP52_12)pwrEquationDynamicScaleVoltagePower(pDyn,
                                                                      (LwUFXP20_12)LwU64_LO32(tmpUFXP52_12_0));

    //
    // Sanity check to make sure tmpUFXP52_12_0 is zero
    // If it is handle it by setting Rail Power Metrics to 0
    //
    if (tmpUFXP52_12_0 == 0)
    {
        pRailMetrics->super.pwrOutDynamicNormalizedmW = 0;
        pRailMetrics->super.leakagePwrmW              = 0;
        goto s_perfCfPwrModelObserveCoreRailMetrics_DLPPM_1X_exit;
    }

    //
    // Numerical analysis:
    // Maximum expected dynamic power is 500[W] = 500000[mW] => 19-bits.
    //
    // 19-bit integer will fit into the 52-bit integer portion of a UFXP52_12.
    //
    tmpUFXP52_12_1 = LW_TYPES_U64_TO_UFXP_X_Y(52, 12,
                                              pRailMetrics->super.pwrOutTuple.pwrmW - pRailMetrics->super.leakagePwrmW);

    //
    // Do the normalized output dynamic power callwlation with the voltage
    // scaling factor left shifted by 12 to maintain all precision bits.
    //
    // Numerical analysis:
    // Maximum expected dynamic power is 500[W] = 500000[mW] => 19-bit integer => 19.12 UFXP
    //
    // Division will only reduce the number of bits needed to represent
    // intermediate results. Therefore, the most number of bits used
    // for any intermediate result is 19.12 UFXP, which fits inside of 52.12 UFXP.
    //
    lwU64Div(&tmpUFXP52_12_1, &tmpUFXP52_12_1, &tmpUFXP52_12_0);
    PMU_ASSERT_OK_OR_GOTO(status,
        (LwU64_HI32(tmpUFXP52_12_1) != 0) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
        s_perfCfPwrModelObserveCoreRailMetrics_DLPPM_1X_exit);

    pRailMetrics->super.pwrOutDynamicNormalizedmWPostPerfCorrection = LwU64_LO32(tmpUFXP52_12_1);

    //
    // Power coming out from observed should be the postPerfCorrection
    // power.
    // TODO: @achaudhry -- Add functionality to back-callwlate pwrOutDynamicNormalizedmW
    // based on gpu utilization. For now copy the value.
    //
    pRailMetrics->super.pwrOutDynamicNormalizedmW =
            pRailMetrics->super.pwrOutDynamicNormalizedmWPostPerfCorrection;

s_perfCfPwrModelObserveCoreRailMetrics_DLPPM_1X_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelObserveFbRailMetrics_DLPPM_1X
 */
static FLCN_STATUS
s_perfCfPwrModelObserveFbRailMetrics_DLPPM_1X
(
    const PERF_CF_PWR_MODEL_DLPPM_1X                                    *pDlppm1x,
    const PERF_CF_PWR_MODEL_SAMPLE_DATA                                 *pSampleData,
    const PERF_CF_PWR_MODEL_DLPPM_1X_FBVDD_RAIL                         *pRail,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_OBSERVED_METRICS_DLPPM_1X_RAIL   *pRailMetrics
)
{
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelObserveInputRailMetrics_DLPPM_1X(
            pSampleData,
            &(pRail->super),
            pRailMetrics),
        s_perfCfPwrModelObserveFbRailMetrics_DLPPM_1X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelFbVoltageLookup_DLPPM_1X(
            pDlppm1x,
            pRailMetrics->super.freqkHz,
            &pRailMetrics->super.voltuV),
        s_perfCfPwrModelObserveFbRailMetrics_DLPPM_1X_exit);

    pRailMetrics->super.pwrOutDynamicNormalizedmW = 0;
    pRailMetrics->super.leakagePwrmW = 0;

    // Observe the Output metrics of the core rail
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelDlppm1xObserveOutputRailMetrics(
            pSampleData,
            &(pRail->super),
            pRailMetrics),
        s_perfCfPwrModelObserveFbRailMetrics_DLPPM_1X_exit);


s_perfCfPwrModelObserveFbRailMetrics_DLPPM_1X_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelObserveCorrection_DLPPM_1X
 */
static FLCN_STATUS
s_perfCfPwrModelObserveCorrection_DLPPM_1X
(
    PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
    LwBool *pbWithinBounds
)
{
    FLCN_STATUS status;
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS *pEstimated =
        &pDlppm1xObserved->observedEstimated.super;
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_DLPPM_1X dlppm1xParams =
    {
        .super =
        {
            .type = LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_DLPPM_1X,
        },
        .bounds =
        {
            .pEndpointLo = NULL,
            .pEndpointHi = NULL,
            // We don't want to and cannot have guard railing yet.
            .bGuardRailsApply = LW_FALSE,
        },
        .pProfiling = &pDlppm1xObserved->profiling.scaleProfiling[LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_SCALE_CORRECTION],
    };
    LwU8 railIdx;

    // Default to assuming that we're within the bounds.
    *pbWithinBounds = LW_TRUE;

    // Initialize one of the global buffers to use as input to scale.
    perfCfPwrModelScaleMeticsInputInit(&PerfCfPwrModelScaleInputs[0]);

    // Copy the core rail frequencies to the input
    for (railIdx = 0U; railIdx < pDlppm1x->coreRail.numRails; railIdx++)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            perfCfPwrModelScaleMeticsInputSetFreqkHz(
                &PerfCfPwrModelScaleInputs[0],
                pDlppm1x->coreRail.pRails[railIdx].clkDomIdx,
                pDlppm1xObserved->coreRail.rails[railIdx].super.freqkHz),
            s_perfCfPwrModelObserveCorrection_DLPPM_1X_exit);
    }

    // Copy the FB rail frequency to the input
    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfPwrModelScaleMeticsInputSetFreqkHz(
            &PerfCfPwrModelScaleInputs[0],
            pDlppm1x->fbRail.super.clkDomIdx,
            pDlppm1xObserved->fbRail.super.freqkHz),
        s_perfCfPwrModelObserveCorrection_DLPPM_1X_exit);

    pDlppm1xObserved->observedEstimated.super.type =
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_DLPPM_1X;
    status = perfCfPwrModelScale(
        &pDlppm1x->super,
        &pDlppm1xObserved->super,
        1U,
        &PerfCfPwrModelScaleInputs[0],
        &pEstimated,
        &dlppm1xParams.super);

    //
    // We have to check if we tried to call scale with frequencies that are now
    // invalid. First check if scale itself reported that the frequency is not
    // supported. Then, after checking for other errors, check if quantization
    // changed the frequencies such that they aren't valid for the observed
    // metrics anymore. In either case, return the FLCN_ERR_FREQ_NOT_SUPPORTED
    // error.
    //
    if (status == FLCN_ERR_FREQ_NOT_SUPPORTED)
    {
        goto s_perfCfPwrModelObserveCorrection_DLPPM_1X_exit;
    }

    // Catch all other errors before continuing
    PMU_ASSERT_OK_OR_GOTO(status,
        status,
        s_perfCfPwrModelObserveCorrection_DLPPM_1X_exit);

    // Now check if the frequencies remained valid.
    if (!s_perfCfPwrModelDlppm1xCorrectionFrequenciesValid(
            pDlppm1x, pDlppm1xObserved))
    {
        status = FLCN_ERR_FREQ_NOT_SUPPORTED;
        goto s_perfCfPwrModelObserveCorrection_DLPPM_1X_exit;
    }


    {
        //
        // Numerical Analysis:
        // We need to callwlate:
        //  perfPct = perfms_{observed} / perfms_{estimated}
        //
        // perfms_{estimated} is a 20.12, and we want perfPct to also be a
        // 20.12; perfms_{observed} is a 20.12, so to do this, we must first
        // left-shift perfms_{observed} by 12 to retain precision.
        //
        // We expect perfms_{observed} to be ~200ms. Being extremely conservative,
        // we can round its expected range up to [0, 4096). 4096 = 2^12, so
        // this means perfms_{observed} requires 12.12 significant bits. Therefore,
        // to left-shift it by 12 bits, we have to do the intermediate math in
        // 64-bit. In particular, we left-shift perfms_{observed} into a 40.24.
        //
        // We then divide the 40.24 by the 20.12 to end up with a 52.12.
        //
        // We see if this fits into a 20.12 and if so, set the perfPct
        // to this value. Otherwise, we set it to the maximum possible value.
        //
        LwUFXP40_24 perfmsObserved40_24 =
            pDlppm1xObserved->perfMetrics.perfms;
        lw64Lsl(&perfmsObserved40_24, &perfmsObserved40_24, 12U);
        LwUFXP52_12 pct52_12;
        LwBool bPerfWithinBounds;

        if (pDlppm1xObserved->observedEstimated.perfMetrics.perfms != 0U)
        {
            lwU64Div(
                &pct52_12,
                &perfmsObserved40_24,
                &(LwUFXP52_12){ pDlppm1xObserved->observedEstimated.perfMetrics.perfms });

            if (lwU64CmpLE(&pct52_12, &(LwU64){ LW_U32_MAX }))
            {
                pDlppm1xObserved->correction.perfPct =
                    (LwUFXP20_12)pct52_12;
            }
            else
            {
                pDlppm1xObserved->correction.perfPct =
                    (LwUFXP20_12)LW_U32_MAX;
            }
        }
        else
        {
            //
            // If we estimate a perfms of 0, then treat the correction as
            // essentially "infinite."
            //
            pDlppm1xObserved->correction.perfPct = LW_U32_MAX;
        }

        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfPwrModelCorrectionWithinBound_DLPPM_1X(
                pDlppm1xObserved->correction.perfPct,
                &pDlppm1x->correctionBounds.perf,
                &bPerfWithinBounds),
            s_perfCfPwrModelObserveCorrection_DLPPM_1X_exit);

        *pbWithinBounds &= bPerfWithinBounds;
        if (bPerfWithinBounds != LW_TRUE)
        {
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_CORRECTION_OUT_OF_BOUNDS_SET(
                pDlppm1xObserved->correction.outOfBoundsMask,
                LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_CORRECTION_OUT_OF_BOUNDS_PERF);
        }
    }

    for (railIdx = 0U; railIdx < pDlppm1x->coreRail.numRails; railIdx++)
    {
        //
        // Numerical Analysis:
        // We need to callwlate:
        // coreRailPwrPct = power_{observed} / power_{estimated}
        //
        // pDlppm1xObserved->coreRail.rails[railIdx].super.pwrOutDynamicNormalizedmW is
        // colwerted to 52.12
        // pDlppm1xObserved->observedEstimated.coreRail.rails[railIdx].pwrOutDynamicNormalizedmW
        //
        // 52.12 / 32.0 => 52.12
        // LWU64_LO(52.12) => 20.12
        //
        // Thus, overflow will happen here if
        // power_{observed} / power_{estimated} >= 2^20
        // which should be safe.
        //
        LwUFXP52_12 coreRailPwrPctU52_12;
        LwBool      bCoreRailPwrWithinBounds;

        if (pDlppm1xObserved->observedEstimated.coreRail.rails[railIdx].pwrOutDynamicNormalizedmW != 0U)
        {
            coreRailPwrPctU52_12 =
                LW_TYPES_U64_TO_UFXP_X_Y_SCALED(52, 12,
                    pDlppm1xObserved->coreRail.rails[railIdx].super.pwrOutDynamicNormalizedmW,
                    pDlppm1xObserved->observedEstimated.coreRail.rails[railIdx].pwrOutDynamicNormalizedmW);
            if (LwU64_HI32(coreRailPwrPctU52_12) > 0)
            {
                pDlppm1xObserved->correction.coreRailPwrPct[railIdx] =
                    (LwUFXP20_12)LW_U32_MAX;
            }
            else
            {
                pDlppm1xObserved->correction.coreRailPwrPct[railIdx] =
                    LwU64_LO32(coreRailPwrPctU52_12);
            }
        }
        else
        {
            //
            // If we estimate a power of 0, then treat the correction as
            // essentially "infinite."
            //
            pDlppm1xObserved->correction.coreRailPwrPct[railIdx] = LW_U32_MAX;
        }

        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfPwrModelCorrectionWithinBound_DLPPM_1X(
                pDlppm1xObserved->correction.coreRailPwrPct[railIdx],
                &pDlppm1x->correctionBounds.coreRailPwr[railIdx],
                &bCoreRailPwrWithinBounds),
            s_perfCfPwrModelObserveCorrection_DLPPM_1X_exit);

        *pbWithinBounds &= bCoreRailPwrWithinBounds;
        if (bCoreRailPwrWithinBounds != LW_TRUE)
        {
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_CORRECTION_OUT_OF_BOUNDS_SET(
                pDlppm1xObserved->correction.outOfBoundsMask,
                LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_CORRECTION_OUT_OF_BOUNDS_CORE_RAIL);
        }
    }

    {
        //
        // Numerical Analysis:
        // We need to callwlate:
        // fbPwrPct = power_{observed} / power_{estimated}
        //
        // pDlppm1xObserved->fbRail.super.pwrOutTuple.pwrmW is colwerted to 52.12
        // pDlppm1xObserved->observedEstimated.fbRail.pwrOutTuple.pwrmW: 32.0
        //
        // 52.12 / 32.0 => 52.12
        // LWU64_LO(52.12) => 20.12
        //
        // Thus, overflow will happen here if
        // power_{observed} / power_{estimated} >= 2^20
        // which should be safe.
        //
        LwUFXP52_12 fbPwrPctU52_12;
        LwBool      bFbPwrWithinBounds;

        if (pDlppm1xObserved->observedEstimated.fbRail.pwrOutTuple.pwrmW != 0U)
        {
            fbPwrPctU52_12 =
                LW_TYPES_U64_TO_UFXP_X_Y_SCALED(52, 12,
                    pDlppm1xObserved->fbRail.super.pwrOutTuple.pwrmW,
                    pDlppm1xObserved->observedEstimated.fbRail.pwrOutTuple.pwrmW);
            if (LwU64_HI32(fbPwrPctU52_12) > 0)
            {
                pDlppm1xObserved->correction.fbPwrPct =
                    (LwUFXP20_12)LW_U32_MAX;
            }
            else
            {
                pDlppm1xObserved->correction.fbPwrPct =
                    LwU64_LO32(fbPwrPctU52_12);
            }
        }
        else
        {
            //
            // If we estimate a power of 0, then treat the correction as
            // essentially "infinite."
            //
            pDlppm1xObserved->correction.fbPwrPct = LW_U32_MAX;
        }

        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfPwrModelCorrectionWithinBound_DLPPM_1X(
                pDlppm1xObserved->correction.fbPwrPct,
                &pDlppm1x->correctionBounds.fbPwr,
                &bFbPwrWithinBounds),
            s_perfCfPwrModelObserveCorrection_DLPPM_1X_exit);

        *pbWithinBounds &= bFbPwrWithinBounds;
        if (bFbPwrWithinBounds != LW_TRUE)
        {
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_CORRECTION_OUT_OF_BOUNDS_SET(
                pDlppm1xObserved->correction.outOfBoundsMask,
                LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_CORRECTION_OUT_OF_BOUNDS_FB_RAIL);
        }
    }

s_perfCfPwrModelObserveCorrection_DLPPM_1X_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelCorrectionWithinBound_DLPPM_1X
 */
static FLCN_STATUS
s_perfCfPwrModelCorrectionWithinBound_DLPPM_1X
(
    LwUFXP20_12 pct,
    const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_DLPPM_1X_CORRECTION_BOUND *pCorrectionBound,
    LwBool *pbWithinBounds
)
{
    *pbWithinBounds =
        (pct >= pCorrectionBound->minPct) &&
        (pct <= pCorrectionBound->maxPct);

    return FLCN_OK;
}

/*!
 * @copydoc s_perfCfPwrModelObserveInitialDramclkEstimates_DLPPM_1X
 */
static FLCN_STATUS
s_perfCfPwrModelObserveInitialDramclkEstimates_DLPPM_1X
(
    PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
    LwU32 dramclkMinkHz,
    const LwU32 *pCoreRailsMinkHz
)
{
    FLCN_STATUS status;
    LwU32 dramclkskHz[
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_INITIAL_DRAMCLK_ESTIMATES_MAX];
    LwU8 dramclkIdx;
    LwU8 metricsIdx;
    PERF_LIMITS_VF_RANGE vfRangePrimary;

    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelFindAdjFreq_DLPPM_1X(
            pDlppm1x->fbRail.super.clkDomIdx,
            pDlppm1xObserved->fbRail.super.freqkHz,
            dramclkMinkHz,
            dramclkskHz,
            &pDlppm1xObserved->numInitialDramclkEstimates,
            &pDlppm1xObserved->lwrrDramclkInitialEstimatesIdx),
        s_perfCfPwrModelObserveInitialDramclkEstimates_DLPPM_1X_exit);

    //
    // Get the primary clock range for the highest DRAMCLK. We'll use this as
    // the primary clock range for all of the DRAMCLKs, to ensure that they're
    // consistent so that we can then do iso-primary-clock guard railing.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelPrimaryClkRangeForDramclkGet_DLPPM_1X(
            pDlppm1x,
            dramclkskHz[pDlppm1xObserved->numInitialDramclkEstimates - 1U],
            pCoreRailsMinkHz,
            &vfRangePrimary),
        s_perfCfPwrModelObserveInitialDramclkEstimates_DLPPM_1X_exit);

    perfCfPwrModelDlppm1xObserveMetricsProfileRegionBegin(
        &pDlppm1xObserved->profiling,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION_OBSERVE_INITIAL_DRAMCLK_ESTIMATES_SCALE);

    ct_assert(
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION_OBSERVE_INITIAL_DRAMCLK_ESTIMATES_SCALE_ITERATION__COUNT >=
        LW_ARRAY_ELEMENTS(pDlppm1xObserved->initialDramclkEstimates));
    for (dramclkIdx = 0;
         dramclkIdx < pDlppm1xObserved->numInitialDramclkEstimates;
         dramclkIdx++)
    {
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS *ppMetrics[
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_ESTIMATED_METRICS_MAX];
        PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_DLPPM_1X dlppm1xParams =
        {
            .super =
            {
                .type = LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_DLPPM_1X,
            },
            .bounds =
            {
                .pEndpointLo = NULL,
                .pEndpointHi = NULL,
                .bGuardRailsApply = LW_TRUE,
            },
            .pProfiling =
                &pDlppm1xObserved->profiling.scaleProfiling[LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_SCALE_INITIAL_ESTIMATES__START + dramclkIdx],
        };

        perfCfPwrModelDlppm1xObserveMetricsProfileRegionBegin(
            &pDlppm1xObserved->profiling,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION_OBSERVE_INITIAL_DRAMCLK_ESTIMATES_SCALE_ITERATION__START + dramclkIdx);

        for (metricsIdx = 0;
             metricsIdx < LW_ARRAY_ELEMENTS(ppMetrics);
             metricsIdx++)
        {
            pDlppm1xObserved->initialDramclkEstimates[dramclkIdx].estimatedMetrics[metricsIdx].super.type =
                LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_DLPPM_1X;
            ppMetrics[metricsIdx] =
                &pDlppm1xObserved->initialDramclkEstimates[dramclkIdx].estimatedMetrics[metricsIdx].super;
        }

        // Set the size of the array as input to perfCfPwrModelScalePrimaryClkRange
        pDlppm1xObserved->initialDramclkEstimates[dramclkIdx].numEstimatedMetrics = 
            LW_ARRAY_ELEMENTS(ppMetrics);

        // Estimate the metrics over this primary clock range.
        PMU_ASSERT_OK_OR_GOTO(status,
            perfCfPwrModelScalePrimaryClkRange(
                &pDlppm1x->super,
                &pDlppm1xObserved->super,
                dramclkskHz[dramclkIdx],
                &vfRangePrimary,
                ppMetrics,
                &pDlppm1xObserved->initialDramclkEstimates[dramclkIdx].numEstimatedMetrics,
                &(LwBool){0},
                &dlppm1xParams.super),
            s_perfCfPwrModelObserveInitialDramclkEstimates_DLPPM_1X_exit);

        perfCfPwrModelDlppm1xObserveMetricsProfileRegionEnd(
            &pDlppm1xObserved->profiling,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION_OBSERVE_INITIAL_DRAMCLK_ESTIMATES_SCALE_ITERATION__START + dramclkIdx);
    }

    perfCfPwrModelDlppm1xObserveMetricsProfileRegionEnd(
        &pDlppm1xObserved->profiling,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION_OBSERVE_INITIAL_DRAMCLK_ESTIMATES_SCALE);

    perfCfPwrModelDlppm1xObserveMetricsProfileRegionBegin(
        &pDlppm1xObserved->profiling,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION_OBSERVE_INITIAL_DRAMCLK_ESTIMATES_CROSS_DRAMCLK_GUARD_RAIL);

    for (metricsIdx = 0;
         metricsIdx < pDlppm1xObserved->initialDramclkEstimates[pDlppm1xObserved->lwrrDramclkInitialEstimatesIdx].numEstimatedMetrics;
         metricsIdx++)
    {
        for (dramclkIdx = pDlppm1xObserved->lwrrDramclkInitialEstimatesIdx - 1U;
             dramclkIdx != LW_U8_MAX;
             dramclkIdx--)
        {
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *const pDlppm1xEstimated =
                &pDlppm1xObserved->initialDramclkEstimates[dramclkIdx].estimatedMetrics[metricsIdx];

            PMU_ASSERT_OK_OR_GOTO(status,
                s_perfCfPwrModelDlppm1xGuardRailMetricsStructureDramclk(
                    pDlppm1x,
                    pDlppm1xEstimated,
                    &pDlppm1xObserved->initialDramclkEstimates[dramclkIdx + 1U].estimatedMetrics[metricsIdx]),
                s_perfCfPwrModelObserveInitialDramclkEstimates_DLPPM_1X_exit);
        }

        for (dramclkIdx = pDlppm1xObserved->lwrrDramclkInitialEstimatesIdx + 1U;
             dramclkIdx < pDlppm1xObserved->numInitialDramclkEstimates;
             dramclkIdx++)
        {
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *const pDlppm1xEstimated =
                &pDlppm1xObserved->initialDramclkEstimates[dramclkIdx].estimatedMetrics[metricsIdx];
            PMU_ASSERT_OK_OR_GOTO(status,
                s_perfCfPwrModelDlppm1xGuardRailMetricsStructureDramclk(
                    pDlppm1x,
                    pDlppm1xEstimated,
                    &pDlppm1xObserved->initialDramclkEstimates[dramclkIdx - 1U].estimatedMetrics[metricsIdx]),
                s_perfCfPwrModelObserveInitialDramclkEstimates_DLPPM_1X_exit);
        }
    }

    //
    // We just guard-railed the performance, so now we have to go back over the
    // core rail power to make sure it respects the perf -> core rail power
    // relationships.
    //
    for (dramclkIdx = 0U;
         dramclkIdx < pDlppm1xObserved->numInitialDramclkEstimates;
         dramclkIdx++)
    {
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *ppDlppm1xEstimated[
            LW2080_CTRL_NNE_NNE_DESC_INFERENCE_LOOPS_MAX];
        PERF_CF_PWR_MODEL_DLPPM_1X_OBSERVED_ADJACENT observedAdjacent;
        LwU8 closestIdx;
        PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS_ILWALID(
            endpointIlwalid);

        // The current DRAMCLK was not guard-railed, so we can skip it.
        if (dramclkIdx == pDlppm1xObserved->lwrrDramclkInitialEstimatesIdx)
        {
            continue;
        }

        // Build a temporary array of pointers.
        for (metricsIdx = 0U;
             metricsIdx < pDlppm1xObserved->initialDramclkEstimates[dramclkIdx].numEstimatedMetrics;
             metricsIdx++)
        {
            ppDlppm1xEstimated[metricsIdx] =
                &pDlppm1xObserved->initialDramclkEstimates[dramclkIdx].estimatedMetrics[metricsIdx];
        }

        //
        // Get indices adjacent to observed, and then get the one that is
        // actually closer to observed in primary clock frequency.
        //
        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfPwrModelDlppm1xObservedAdjacentGet(
                pDlppm1xObserved,
                &observedAdjacent,
                ppDlppm1xEstimated,
                pDlppm1xObserved->initialDramclkEstimates[dramclkIdx].numEstimatedMetrics),
            s_perfCfPwrModelObserveInitialDramclkEstimates_DLPPM_1X_exit);
        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfPwrModelDlppm1xObservedAdjacentClosestGet(
                pDlppm1xObserved,
                &observedAdjacent,
                ppDlppm1xEstimated,
                &closestIdx),
            s_perfCfPwrModelObserveInitialDramclkEstimates_DLPPM_1X_exit);

        // Go downwards from the point we determined is closest to observed.
        for (metricsIdx = closestIdx - 1U;
             metricsIdx != LW_U8_MAX;
             metricsIdx--)
        {
            PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS_FROM_ESTIMATED(
                next,
                ppDlppm1xEstimated[metricsIdx + 1U]);

            PMU_ASSERT_OK_OR_GOTO(status,
                s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrLinearityFromPoints(
                    pDlppm1x,
                    ppDlppm1xEstimated[metricsIdx],
                    &next,
                    &endpointIlwalid,
                    &endpointIlwalid,
                    LW_FALSE,
                    LW_TRUE),
                s_perfCfPwrModelObserveInitialDramclkEstimates_DLPPM_1X_exit);
        }

        // Go upwards from the point we determined is closest to observed.
        for (metricsIdx = closestIdx + 1U;
             metricsIdx < pDlppm1xObserved->initialDramclkEstimates[dramclkIdx].numEstimatedMetrics;
             metricsIdx++)
        {
            PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS_FROM_ESTIMATED(
                prev,
                ppDlppm1xEstimated[metricsIdx - 1U]);

            PMU_ASSERT_OK_OR_GOTO(status,
                s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrLinearityFromPoints(
                    pDlppm1x,
                    ppDlppm1xEstimated[metricsIdx],
                    &prev,
                    &endpointIlwalid,
                    &endpointIlwalid,
                    LW_TRUE,
                    LW_FALSE),
                s_perfCfPwrModelObserveInitialDramclkEstimates_DLPPM_1X_exit);
        }
    }

    perfCfPwrModelDlppm1xObserveMetricsProfileRegionEnd(
        &pDlppm1xObserved->profiling,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION_OBSERVE_INITIAL_DRAMCLK_ESTIMATES_CROSS_DRAMCLK_GUARD_RAIL);

    //
    // Callwlate the core rail power fit for every DRAMCLK structure. Then, try
    // to apply it to every estimated metrics structure. Then, see if any of the
    // metrics were guard railed, and if so, re-propagate the metrics.
    //
    ct_assert(
        (LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION_OBSERVE_INITIAL_DRAMCLK_ESTIMATES_CALLWLATE_FIT_ITERATION__COUNT >=
            LW_ARRAY_ELEMENTS(pDlppm1xObserved->initialDramclkEstimates)) &&
        (LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION_OBSERVE_INITIAL_DRAMCLK_ESTIMATES_APPLY_FIT_AND_REPROPAGATE_ITERATION__COUNT >=
            LW_ARRAY_ELEMENTS(pDlppm1xObserved->initialDramclkEstimates)));
    for (dramclkIdx = 0;
         dramclkIdx < pDlppm1xObserved->numInitialDramclkEstimates;
         dramclkIdx++)
    {
        perfCfPwrModelDlppm1xObserveMetricsProfileRegionBegin(
            &pDlppm1xObserved->profiling,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION_OBSERVE_INITIAL_DRAMCLK_ESTIMATES_CALLWLATE_FIT_ITERATION__START + dramclkIdx);

        // TODO: add the observed point into the fit for the appropriate DRAMCLK.
        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfPwrModelCoreRailPwrFitCallwlate_DLPPM_1X(
                &pDlppm1xObserved->initialDramclkEstimates[dramclkIdx]),
            s_perfCfPwrModelObserveInitialDramclkEstimates_DLPPM_1X_exit);

        perfCfPwrModelDlppm1xObserveMetricsProfileRegionEnd(
            &pDlppm1xObserved->profiling,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION_OBSERVE_INITIAL_DRAMCLK_ESTIMATES_CALLWLATE_FIT_ITERATION__START + dramclkIdx);

        perfCfPwrModelDlppm1xObserveMetricsProfileRegionBegin(
            &pDlppm1xObserved->profiling,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION_OBSERVE_INITIAL_DRAMCLK_ESTIMATES_APPLY_FIT_AND_REPROPAGATE_ITERATION__START + dramclkIdx);

        for (metricsIdx = 0;
             metricsIdx < pDlppm1xObserved->initialDramclkEstimates[dramclkIdx].numEstimatedMetrics;
             metricsIdx++)
        {
           LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *const pDlppm1xEstimated =
                &pDlppm1xObserved->initialDramclkEstimates[dramclkIdx].estimatedMetrics[metricsIdx];

            //
            // If we have a fit, make sure that the original points actually fit
            // within the fit's tolerance.
            //
            if (pDlppm1xObserved->initialDramclkEstimates[dramclkIdx].coreRailPwrFit.bValid)
            {
                s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrFit(
                    pDlppm1x,
                    &pDlppm1xObserved->initialDramclkEstimates[dramclkIdx].estimatedMetrics[metricsIdx],
                    &pDlppm1xObserved->initialDramclkEstimates[dramclkIdx].coreRailPwrFit);
            }

            //
            // Check if the perf metric was guard railed and if so,
            // re-propagate the metric.
            //
            if (s_perfCfPwrModelDlppm1xGuardRailFlagsMetricGuardRailed(
                    pDlppm1xEstimated,
                    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_METRIC_PERF))
            {
                PMU_ASSERT_OK_OR_GOTO(status,
                    s_perfCfPwrModelDlppm1xNneEstimatedPerfPropagate(
                        &pDlppm1xEstimated->perfMetrics,
                        &pDlppm1xObserved->perfMetrics),
                    s_perfCfPwrModelObserveInitialDramclkEstimates_DLPPM_1X_exit);
            }

            //
            // Check if either power metric was guard railed and if so,
            // re-propagate the power metrics.
            //
            if (s_perfCfPwrModelDlppm1xGuardRailFlagsMetricGuardRailed(
                    pDlppm1xEstimated,
                    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_METRIC_CORE_RAIL_PWR) ||
                s_perfCfPwrModelDlppm1xGuardRailFlagsMetricGuardRailed(
                    pDlppm1xEstimated,
                    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_METRIC_FB_PWR))
            {
                PMU_ASSERT_OK_OR_GOTO(status,
                    s_perfCfPwrModelDlppm1xNneEstimatedPowerMetricsPropagate(
                        pDlppm1x,
                        pDlppm1xEstimated),
                    s_perfCfPwrModelObserveInitialDramclkEstimates_DLPPM_1X_exit);
            }
        }

        perfCfPwrModelDlppm1xObserveMetricsProfileRegionEnd(
            &pDlppm1xObserved->profiling,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION_OBSERVE_INITIAL_DRAMCLK_ESTIMATES_APPLY_FIT_AND_REPROPAGATE_ITERATION__START + dramclkIdx);
    }

s_perfCfPwrModelObserveInitialDramclkEstimates_DLPPM_1X_exit:
    return status;

}

/*!
 * @copydoc s_perfCfPwrModelFindAdjFreq_DLPPM_1X
 */
static FLCN_STATUS
s_perfCfPwrModelFindAdjFreq_DLPPM_1X
(
    LwBoardObjIdx   clkDomIdx,
    LwU32           observedFreqkHz,
    LwU32           minFreqkHz,
    LwU32          *pAdjFreqkHz,
    LwU8           *pNumAdjFreq,
    LwU8           *pLwrrDramclkIdx
)
{
    PERF_LIMITS_VF   vfDomain;
    LwU32            quantHikHz;
    LwU32            quantLokHz;
    LwU8             idx;
    LwU8             numUnique;
    FLCN_STATUS      status  = FLCN_OK;
    LwU32            lwrrDramclkFreqkHz;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pAdjFreqkHz == NULL) ||
         (pNumAdjFreq == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        s_perfCfPwrModelFindAdjFreq_DLPPM_1X_exit);

    // Quantize-up the observed frequency.
    vfDomain.idx   = clkDomIdx;
    vfDomain.value = observedFreqkHz;
    PMU_ASSERT_OK_OR_GOTO(status,
        perfLimitsFreqkHzQuantize(&vfDomain,
                                  NULL,      // Quantize over all PSTATES.
                                  LW_FALSE), // Round up.
        s_perfCfPwrModelFindAdjFreq_DLPPM_1X_exit);
    quantHikHz = vfDomain.value;

    // Quantize-down from the quantHikHz.
    vfDomain.value = quantHikHz - 1000;
    PMU_ASSERT_OK_OR_GOTO(status,
        perfLimitsFreqkHzQuantize(&vfDomain,
                                  NULL,     // Quantize over all PSTATES.
                                  LW_TRUE), // Round down.
        s_perfCfPwrModelFindAdjFreq_DLPPM_1X_exit);
    quantLokHz = vfDomain.value;

    // Ensure the observed frequency is within the bounds of this clock domain.
    observedFreqkHz = LW_MAX(observedFreqkHz, quantLokHz);
    observedFreqkHz = LW_MIN(observedFreqkHz, quantHikHz);

    //
    // Populate the output.
    //
    // Note: It is critical to quantize-up if quantHikHz and quantLokHz
    //       are equidistant from observedFreqkHz to correctly handle when
    //       observedFreqkHz is at or below the lower qunatized limit of this
    //       clock domain. In that case, quantHikHz == quantLokHz.
    //
    if (LW_SUBTRACT_NO_UNDERFLOW(quantHikHz, observedFreqkHz) <=
        LW_SUBTRACT_NO_UNDERFLOW(observedFreqkHz, quantLokHz))
    {
        pAdjFreqkHz[0] = quantLokHz;
        pAdjFreqkHz[1] = quantHikHz;

        vfDomain.value = quantHikHz + 1000;
        PMU_ASSERT_OK_OR_GOTO(status,
            perfLimitsFreqkHzQuantize(&vfDomain,
                                      NULL,      // Quantize over all PSTATES.
                                      LW_FALSE), // Round up.
            s_perfCfPwrModelFindAdjFreq_DLPPM_1X_exit);
        pAdjFreqkHz[2] = vfDomain.value;
    }
    else
    {
        pAdjFreqkHz[2] = quantHikHz;
        pAdjFreqkHz[1] = quantLokHz;

        vfDomain.value = LW_SUBTRACT_NO_UNDERFLOW(quantLokHz, 1000);
        PMU_ASSERT_OK_OR_GOTO(status,
            perfLimitsFreqkHzQuantize(&vfDomain,
                                      NULL,     // Quantize over all PSTATES.
                                      LW_TRUE), // Round down.
            s_perfCfPwrModelFindAdjFreq_DLPPM_1X_exit);
        pAdjFreqkHz[0] = vfDomain.value;
    }

    //
    // The current DRAMCLK's frequency is in the middle position, i.e., 1. Save
    // this off for comparison later.
    //
    lwrrDramclkFreqkHz = pAdjFreqkHz[1];

    // Skip the frequencies that are too low to search.
    for (idx = 0;
         (idx < LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_INITIAL_DRAMCLK_ESTIMATES_MAX) &&
         (pAdjFreqkHz[idx] < minFreqkHz);
         idx++)
    {
    }

    //
    // Start with the first DRAMCLK above the minimum, and then remove duplicate
    // points.
    //
    // Also ilwalidate the current DRAMLCK index, and then if the current
    // DRAMCLK frequency gets inserted during the loop, update it then.
    //
    numUnique = 0;
    *pLwrrDramclkIdx = LW_U8_MAX;
    for (; idx < LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_INITIAL_DRAMCLK_ESTIMATES_MAX; idx++)
    {
        if ((numUnique == 0U) ||
            (pAdjFreqkHz[idx] != pAdjFreqkHz[numUnique - 1]))
        {
            pAdjFreqkHz[numUnique] = pAdjFreqkHz[idx];

            // Set the current DRAMCLK index if we just inserted its frequency.
            if (pAdjFreqkHz[numUnique] == lwrrDramclkFreqkHz)
            {
                *pLwrrDramclkIdx = numUnique;
            }

            numUnique++;
        }
    }
    *pNumAdjFreq = numUnique;

s_perfCfPwrModelFindAdjFreq_DLPPM_1X_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelPrimaryClkRangeForDramclkGet_DLPPM_1X
 */
static FLCN_STATUS
s_perfCfPwrModelPrimaryClkRangeForDramclkGet_DLPPM_1X
(
    const PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
    LwU32 dramclkkHz,
    const LwU32 *pCoreRailsMinkHz,
    PERF_LIMITS_VF_RANGE *pVfRangePrimary
)
{
    FLCN_STATUS status;
    PERF_LIMITS_VF vfDomainDramclk =
    {
        .idx = pDlppm1x->fbRail.super.clkDomIdx,
        .value = dramclkkHz,
    };
    PERF_LIMITS_PSTATE_RANGE pstateRangeDramclk;
    const PERF_LIMITS_VF_RANGE primaryClkLimitsRange =
    {
        .idx = pDlppm1x->coreRail.pRails[0].clkDomIdx,
        .values =
        {
            [PERF_LIMITS_VF_RANGE_IDX_MIN] = pCoreRailsMinkHz[0],
            [PERF_LIMITS_VF_RANGE_IDX_MAX] = LW_U32_MAX,
        },
    };

    // Set the primary clock domain index
    pVfRangePrimary->idx = pDlppm1x->coreRail.pRails[0].clkDomIdx;

    //
    // Compute the primary clock frequency range we must examine for this
    // DRAMCLK.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        perfLimitsFreqkHzToPstateIdxRange(
            &vfDomainDramclk,
            &pstateRangeDramclk),
        s_perfCfPwrModelPrimaryClkRangeForDramclkGet_DLPPM_1X_exit);
    PMU_ASSERT_OK_OR_GOTO(status,
        perfLimitsPstateIdxRangeToFreqkHzRange(
            &pstateRangeDramclk,
            pVfRangePrimary),
        s_perfCfPwrModelPrimaryClkRangeForDramclkGet_DLPPM_1X_exit);

    // Bound the primary clock range to the valid estimation range.
    PERF_LIMITS_RANGE_RANGE_BOUND(&primaryClkLimitsRange, pVfRangePrimary);

s_perfCfPwrModelPrimaryClkRangeForDramclkGet_DLPPM_1X_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelCoreRailPwrFitCallwlate_DLPPM_1X
 */
static FLCN_STATUS
s_perfCfPwrModelCoreRailPwrFitCallwlate_DLPPM_1X
(
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_DRAMCLK_ESTIMATES *pDramclkEstimates
)
{
    LwUFXP52_12 sumX = 0;
    LwUFXP52_12 sumXX = 0;
    LwU64 sumY = 0;
    LwUFXP52_12 sumXY = 0;
    LwU8 i;
    FLCN_STATUS status = FLCN_OK;

    // Start off by marking the fit as invalid
    pDramclkEstimates->coreRailPwrFit.bValid = LW_FALSE;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        pDramclkEstimates->numEstimatedMetrics != 0,
        FLCN_ERR_ILWALID_STATE,
        s_perfCfPwrModelCoreRailPwrFitCallwlate_DLPPM_1X_exit
    );

    //
    // Numerical Analysis:
    //
    // The first step of callwlating a linear fit is to callwlate:
    //  1.) Sum of x for each point
    //  2.) Sum of x^2 for each point
    //  3.) Sum of y for each point
    //  4.) Sum of x * y for each point
    //
    // General notes:
    // The "x" value is the perfRatio ratio = perfms_{obs} / perfms_{est}. We
    // can assume that perfms_[est} is at least 1/10th of perfms_{obs}, i.e.:
    //  perfms_{est} >= perfms_{obs} / 10
    //  10 >= perfms_{obs} / perfms_{est}
    //  perfms_{obs} / perfms_{est} <= 10
    //
    // Colwersely, this also means that it is >= 0.1.
    //
    // In other words, the ratio is in the range [0.1, 10]. We can make this
    // assumption because we enforce that performance increases are not better
    // than linear with respect to the primary clock, and primary clock range
    // only spans a factor of 10.
    //
    // TODO-aherring: Revisit the expected range once cross-DRAMCLK guard
    // railing is implemented.
    //
    // Using a fractional resolution of 12 bits, this means that this ratio uses
    // 4.4 significant bits.
    //
    // The "y" value is the primary core rail's pwrOutDynamicNormalizedmW value.
    // Power values are never lwrrently expected to exceed a kW; the closest
    // power of two in milliwatts is 2^20 = 1048576. Therefore, the y value uses
    // 20.0 significant bits.
    //
    // When doing a sum, the maximum number of bits that can be added is
    // log_{2}(number of inputs). The inputs are passed via:
    //  LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_DRAMCLK_ESTIMATES::estimatedMetrics
    // which is sized to LW2080_CTRL_NNE_NNE_DESC_INFERENCE_LOOPS_MAX.
    // Therefore, the maximum number of bits added to any initial value via
    // summing is:
    //  log_{2}(LW2080_CTRL_NNE_NNE_DESC_INFERENCE_LOOPS_MAX)
    // LW2080_CTRL_NNE_NNE_DESC_INFERENCE_LOOPS_MAX is lwrrently 8, so this
    // simplifies to:
    //  log_{2}(8)
    //  3
    // So 3 significant bits can be added to an initial value via summation.
    //
    // 1.) Sum of x for each point
    // Per above, the x values have 4.4 significant bits, and summation can add
    // 3 significant bits, so the result has 7.4 significant bits.
    //
    // 2.) Sum of x^2 for each point
    // Per above, the x values have 4.4 significant bits. The multiplication
    // will result in an 8.8. However, the callwlation is done in 52.12, so
    // after multiplication, we shift off the lower 12 bits to retain 12
    // bit fractional resolution. Per above, the summation can add 3 significant
    // bits, so the result has 11.8 significant bits.
    //
    // 3.) Sum of y for each point
    // Per above, the y values have 20.0 significant bits, and summation can add
    // 3 significant bits, so the result has 23.0 significant bits.
    //
    // 4.) Sum of x * y for each point
    // Per above, the x values have 4.4 significant bits and the y values have
    // 20.0 significant bits, so the product will have 24.4 significant bits.
    // Per above, the summation can add 3 significant bits, so the result has
    // 27.4 significant bits.
    //
    for (i = 0; i < pDramclkEstimates->numEstimatedMetrics; i++)
    {
        LwUFXP52_12 x =
            pDramclkEstimates->estimatedMetrics[i].perfMetrics.perfRatio;
        LwU64 y =
            pDramclkEstimates->estimatedMetrics[i].coreRail.rails[0].pwrOutDynamicNormalizedmW;
        LwUFXP52_12 xy;

        lw64Add(&sumX, &sumX, &x);
        lw64Add(&sumXX, &sumXX, &(LwU64){ multUFXP64(12, x, x) });
        lw64Add(&sumY, &sumY, &y);

        lwU64Mul(&xy, &x, &y);
        lw64Add(&sumXY, &sumXY, &xy);
    }

    //
    // Numerical Analysis:
    //
    // The linear least squares fit for a set of points is callwlated as
    // follows:
    //  slope = numerator / denominator
    //        = (n * sumXY - sumX * sumY) / (n * sumXX - sumX^2)
    //  intercept = meanY - slope * meanX
    //            = (sumY / n) - slope * (sumX / n)
    // where "n" is the number of points.
    //
    // General notes:
    // Per above, n can be at most
    // LW2080_CTRL_NNE_NNE_DESC_INFERENCE_LOOPS_MAX. Multiplying by it can
    // therefore add at most log_{2}(LW2080_CTRL_NNE_NNE_DESC_INFERENCE_LOOPS_MAX)
    // significant bits. Per above, LW2080_CTRL_NNE_NNE_DESC_INFERENCE_LOOPS_MAX
    // is lwrrently 8, so the multiplication can add:
    //  log_{2}(8)
    //  3
    // significant bits.
    //
    // n * sumXY
    // Per above, sumXY is 27.4 significant bits, and multiplying by n can add
    // 3 significant bits, resulting in 30.4 significant bits.
    //
    // sumX * sumY
    // Per above, sumX is 7.4 significant bits and sumY is 23.0 significant
    // bits, resulting in 30.4 significant bits.
    //
    // numerator = n * sumXY - sumX * sumY
    // This is a simple difference. Without any further information, this would
    // mean that the result remains a 30.4, which still fits in a 52.12.
    //
    // n * sumXX
    // Per above, sumXX is 11.8 significant bits, and multiplying by n can add
    // 3 significant bits, resulting in a 14.8.
    //
    // sumX^2
    // Per above, sumX is 7.4 significant bits. The multiplication will result
    // in a 14.8, However, the callwlation is done in 52.12, so after
    // multiplication, we shift off the lower 12 bits to retain 12 bits of
    // fractional resolution. The end result remains therefore a 14.8.
    //
    // denominator = n * sumXX - sumX^2
    // This is a simple difference. Without any further information, this would
    // mean that the result remains a 30.8.
    //
    // slope = numerator / denominator
    // To retain 12 bit fractional resolution in the result, we first pre-shift
    // the numerator left by 12 (to divide it by a 52.12), making it a 40.24.
    // This is safe because the numerator had only 30 bits of integer precision
    // to begin with.
    //
    // After the division, the result is back to a 52.12 representation. The
    // division may have added at most 8 integer bits to the result, meaning a
    // requirement of 38.12 (with 12 fractional bits of resolution).
    //
    // However, truly having a slope of 38 integer bits would be ludicrous. This
    // would mean that, for example, going from a ratio of x = 1 to a ratio of
    // x = 2 (i.e., doubling the perf) would raise the power by:
    //  2^38 mW ~= 274877 kW.
    //
    // In fact, anything that would cause the slope to require more than 20
    // integer bits is ludicrous; again, this would mean something like an
    // increase of 1 kW in the step from x = 1 to x = 2, when in reality, 1kW is
    // lwrrently above the maximum expected power.
    //
    // Therefore, we assume that the resultant slope will fit in a 20.12.
    // However, if it does not for some reason, we set a Boolean to indicate
    // that the fit is invalid so that it can be ignored.
    //
    // We also ilwalidate the fit if, before doing the division, we see that a
    // division by 0 will happen. (This may occur when all of the x values are
    // the same, i.e., a vertical line and undefined slope.)
    //
    // intercept = meanY - slope * meanX
    //           = (sumY / n) - slope * (sumX / n)
    // The mean meanY and meanX values should have the same number of
    // significant bits as the original sums; therefore, meanY has 23.0
    // significant bits and meanX has 7.4 significant bits
    //
    // Per above, we have assumed that the slope has a max number of significant
    // bits of 20.12, the result of slope * meanX will have 27.12 significant
    // bits.
    //
    // Therefore, the result of the subtraction will have at most 27.12
    // significant bits.
    //
    // However, again, this would be ludicrous. This would mean that dropping
    // the estimated performance to an infinitesimally small number would imply
    // a power of:
    //  2^27 mW ~= 134 kW
    //
    // Therefore, because the intercept is a power value and we expect power to
    // always be less than 1 kW, which requires 20.0 significant bits, we assume
    // the intercept fits in a 32.0. However, in case the math works out to
    // betray this fact, we again ilwalidate the fit.
    //
    // We also ilwalidate the fit fit if the intercept would be negative; this
    // is a nonsensical physical result and is an indication that the fit is
    // inappropriate.
    //
    // Finally, we also check that evaluating the fit at the maximum expected x
    // value (which is given by the last element in the initial set of
    // estimates) does not overflow 32 bits; again, the output of the fit is a
    // power value, and these are always expected to be less than 1 kW, which
    // fits in 20 bits.
    //
    {
        LwU64 n = pDramclkEstimates->numEstimatedMetrics;
        LwUFXP52_12 slope;
        LwU64 intercept;

        // Callwlate the slope
        {
            LwUFXP52_12 nTimesSumXY;
            LwUFXP52_12 nTimesSumXX;
            LwUFXP52_12 sumXTimesSumY;
            LwUFXP52_12 slopeNumerator;
            LwUFXP52_12 slopeDenominator;

            lwU64Mul(&nTimesSumXY, &n, &sumXY);
            lwU64Mul(&sumXTimesSumY, &sumX, &sumY);
            lw64Sub(&slopeNumerator, &nTimesSumXY, &sumXTimesSumY);

            lwU64Mul(&nTimesSumXX, &n, &sumXX);
            lw64Sub(&slopeDenominator, &nTimesSumXX, &(LwU64){ multUFXP64(12, sumX, sumX) });

            //
            // Check for if we would experience a division by 0. This happens
            // when all of the x values are the same, i.e., a vertical line and
            // undefined slope.
            //
            if (lw64CmpEQ(&slopeDenominator, &(LwU64){ 0 }))
            {
                goto s_perfCfPwrModelCoreRailPwrFitCallwlate_DLPPM_1X_exit;
            }

            lw64Lsl(&slopeNumerator, &slopeNumerator, 12);
            lwU64Div(&slope, &slopeNumerator, &slopeDenominator);

            if (lwU64CmpGT(&slope, &(LwU64){ LW_U32_MAX }))
            {
                goto s_perfCfPwrModelCoreRailPwrFitCallwlate_DLPPM_1X_exit;
            }
        }

        // Compute the intercept
        {
            LwUFXP52_12 meanX;
            LwUFXP52_12 meanY;
            LwUFXP52_12 slopeTimesMeanX;

            lwU64Div(&meanX, &sumX, &n);
            slopeTimesMeanX = multUFXP64(12, slope, meanX);

            meanY = LW_TYPES_U64_TO_UFXP_X_Y_SCALED(52, 12, sumY, n);

            if (lwU64CmpGT(&slopeTimesMeanX, &meanY))
            {
                goto s_perfCfPwrModelCoreRailPwrFitCallwlate_DLPPM_1X_exit;
            }

            // Compute the intercept and shift off the fractional bits.
            lw64Sub(&intercept, &meanY, &slopeTimesMeanX);
            lw64Lsr(&intercept, &intercept, 12);

            if (lwU64CmpGT(&intercept, &(LwU64){ LW_U32_MAX }))
            {
                goto s_perfCfPwrModelCoreRailPwrFitCallwlate_DLPPM_1X_exit;
            }
        }

        // Ensure that evaluating the fit at the max x value does not overflow
        {
            LwUFXP52_12 maxX =
                pDramclkEstimates->estimatedMetrics[n - 1].perfMetrics.perfRatio;
            LwU64 output;

            lw64Add(&output, &(LwU64){ multUFXP64(24, slope, maxX) }, &intercept);

            if (lwU64CmpGT(&output, &(LwU64){ LW_U32_MAX } ))
            {
                goto s_perfCfPwrModelCoreRailPwrFitCallwlate_DLPPM_1X_exit;
            }
        }

        pDramclkEstimates->coreRailPwrFit.slopemWPerRatio = LwU64_LO32(slope);
        pDramclkEstimates->coreRailPwrFit.interceptmW = LwU64_LO32(intercept);
        pDramclkEstimates->coreRailPwrFit.bValid = LW_TRUE;
    }

s_perfCfPwrModelCoreRailPwrFitCallwlate_DLPPM_1X_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelFbVoltageLookup_DLPPM_1X
 */
FLCN_STATUS
s_perfCfPwrModelFbVoltageLookup_DLPPM_1X
(
    const PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
    LwU32 freqkHz,
    LwU32 *pVoltuV
)
{
    FLCN_STATUS status;
    CLK_DOMAIN *pDomainDram;
    CLK_DOMAIN_PROG *pDomainProgDram;
    LW2080_CTRL_CLK_VF_INPUT input;
    LW2080_CTRL_CLK_VF_OUTPUT output;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pDomainDram = BOARDOBJGRP_OBJ_GET(
                CLK_DOMAIN, pDlppm1x->fbRail.super.clkDomIdx)) != NULL),
        FLCN_ERR_ILWALID_STATE,
        s_perfCfPwrModelFbVoltageLookup_DLPPM_1X_exit);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pDomainProgDram = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(
                pDomainDram, PROG)) != NULL),
        FLCN_ERR_ILWALID_STATE,
        s_perfCfPwrModelFbVoltageLookup_DLPPM_1X_exit);

    LW2080_CTRL_CLK_VF_INPUT_INIT(&input);
    input.flags = FLD_SET_DRF(
        2080_CTRL_CLK, _VF_INPUT_FLAGS, _VF_POINT_SET_DEFAULT, _YES,
        input.flags);
    input.value = freqkHz;

    LW2080_CTRL_CLK_VF_OUTPUT_INIT(&output);

    PMU_ASSERT_OK_OR_GOTO(status,
        clkDomainProgFbvddFreqToVolt(pDomainProgDram, &input, &output),
        s_perfCfPwrModelFbVoltageLookup_DLPPM_1X_exit);

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (output.inputBestMatch != LW2080_CTRL_CLK_VF_VALUE_ILWALID),
        FLCN_ERR_ILWALID_STATE,
        s_perfCfPwrModelFbVoltageLookup_DLPPM_1X_exit);

    *pVoltuV = output.value;

s_perfCfPwrModelFbVoltageLookup_DLPPM_1X_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xPerfTopoStatusInit
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xPerfTopoStatusInit
(
    const PERF_CF_PWR_MODEL_DLPPM_1X   *pPwrModelDlppm1x,
    PERF_CF_TOPOLOGYS_STATUS           *pPerfTopoStatus
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8        i;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pPerfTopoStatus != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        s_perfCfPwrModelDlppm1xPerfTopoStatusInit_exit);

    for (i = 0U; i < pPwrModelDlppm1x->coreRail.numRails; i++)
    {
        const PERF_CF_PWR_MODEL_DLPPM_1X_RAIL *const pRail =
            &pPwrModelDlppm1x->coreRail.pRails[i];

        boardObjGrpMaskBitSet(&pPerfTopoStatus->mask, pRail->clkDomTopIdx);
        if (pRail->clkDomUtilTopIdx != LW2080_CTRL_BOARDOBJ_IDX_ILWALID)
        {
            boardObjGrpMaskBitSet(&pPerfTopoStatus->mask, pRail->clkDomUtilTopIdx);
        }
    }

    boardObjGrpMaskBitSet(
        &pPerfTopoStatus->mask, pPwrModelDlppm1x->fbRail.super.clkDomTopIdx);
    if (pPwrModelDlppm1x->fbRail.super.clkDomUtilTopIdx != LW2080_CTRL_BOARDOBJ_IDX_ILWALID)
    {
        boardObjGrpMaskBitSet(
            &pPerfTopoStatus->mask, pPwrModelDlppm1x->fbRail.super.clkDomUtilTopIdx);
    }

    boardObjGrpMaskBitSet(
        &pPerfTopoStatus->mask, pPwrModelDlppm1x->ptimerTopologyIdx);

s_perfCfPwrModelDlppm1xPerfTopoStatusInit_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xPwrChannelsStatusInit
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xPwrChannelsStatusInit
(
    const PERF_CF_PWR_MODEL_DLPPM_1X   *pPwrModelDlppm1x,
    PWR_CHANNELS_STATUS                *pPwrChannelsStatus
)
{
    FLCN_STATUS status;
    LwU8 i;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pPwrChannelsStatus != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        s_perfCfPwrModelDlppm1xPwrChannelsStatusInit_exit);

    for (i = 0U; i < pPwrModelDlppm1x->coreRail.numRails; i++)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfPwrModelDlppm1xPwrChannelsStatusRailInit(
                pPwrModelDlppm1x,
                &pPwrModelDlppm1x->coreRail.pRails[i],
                pPwrChannelsStatus),
            s_perfCfPwrModelDlppm1xPwrChannelsStatusInit_exit);
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelDlppm1xPwrChannelsStatusRailInit(
            pPwrModelDlppm1x,
            &pPwrModelDlppm1x->fbRail.super,
            pPwrChannelsStatus),
        s_perfCfPwrModelDlppm1xPwrChannelsStatusInit_exit);

    if (pPwrModelDlppm1x->smallRailPwrChIdx != LW2080_CTRL_BOARDOBJ_IDX_ILWALID)
    {
        boardObjGrpMaskBitSet(
            &pPwrChannelsStatus->mask,
            pPwrModelDlppm1x->smallRailPwrChIdx);
    }

    boardObjGrpMaskBitSet(
        &pPwrChannelsStatus->mask,
        pPwrModelDlppm1x->tgpPwrChIdx);

s_perfCfPwrModelDlppm1xPwrChannelsStatusInit_exit:
    return status;
}

static FLCN_STATUS
s_perfCfPwrModelDlppm1xPwrChannelsStatusRailInit
(
    const PERF_CF_PWR_MODEL_DLPPM_1X        *pPwrModelDlppm1x,
    const PERF_CF_PWR_MODEL_DLPPM_1X_RAIL   *pRail,
    PWR_CHANNELS_STATUS                     *pPwrChannelsStatus
)
{
    if (pRail->outPwrChIdx != LW2080_CTRL_BOARDOBJ_IDX_ILWALID)
    {
        boardObjGrpMaskBitSet(
        	&pPwrChannelsStatus->mask,
        	pRail->outPwrChIdx);
    }

    boardObjGrpMaskBitSet(
        &pPwrChannelsStatus->mask,
        pRail->inPwrChIdx);

    return FLCN_OK;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xPmSensorsStatusInit
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xPmSensorsStatusInit
(
    PERF_CF_PWR_MODEL_DLPPM_1X *pPwrModelDlppm1x,
    PERF_CF_PM_SENSORS_STATUS  *pPmSensorsStatus
)
{
    FLCN_STATUS status;
    RM_PMU_PERF_CF_PM_SENSOR_GET_STATUS *pRmPmSensorStatus;
    BOARDOBJGRPMASK_E1024 pmSensorSignalMask;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pPmSensorsStatus != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        s_perfCfPwrModelDlppm1xPmSensorsStatusInit_exit);

    boardObjGrpMaskBitSet(
        &pPmSensorsStatus->mask,
        pPwrModelDlppm1x->pmSensorIdx);

    pRmPmSensorStatus =
        &pPmSensorsStatus->pmSensors[pPwrModelDlppm1x->pmSensorIdx].pmSensor;

    boardObjGrpMaskInit_E1024(&pmSensorSignalMask);

    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpMaskImport_E1024(
            &pmSensorSignalMask,
            &pRmPmSensorStatus->signalsMask),
        s_perfCfPwrModelDlppm1xPmSensorsStatusInit_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpMaskOr(
            &pmSensorSignalMask,
            &pmSensorSignalMask,
            &pPwrModelDlppm1x->pmSensorSignalMask),
        s_perfCfPwrModelDlppm1xPmSensorsStatusInit_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpMaskExport_E1024(
            &pmSensorSignalMask,
            &pRmPmSensorStatus->signalsMask),
        s_perfCfPwrModelDlppm1xPmSensorsStatusInit_exit);

s_perfCfPwrModelDlppm1xPmSensorsStatusInit_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelPowerTupleAdd_DLPPM_1X
 */
static FLCN_STATUS
s_perfCfPwrModelPowerTupleAdd_DLPPM_1X
(
    LW2080_CTRL_PMGR_PWR_TUPLE   *pOp0,
    LW2080_CTRL_PMGR_PWR_TUPLE   *pOp1,
    LW2080_CTRL_PMGR_PWR_TUPLE   *pResult
)
{
    FLCN_STATUS   status = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pOp0    == NULL) ||
         (pOp1    == NULL) ||
         (pResult == NULL) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK),
        s_perfCfPwrModelPowerTupleAdd_DLPPM_1X_exit);

    // Do per-field summation.
    pResult->pwrmW    = pOp0->pwrmW    + pOp1->pwrmW;
    pResult->lwrrmA   = pOp0->lwrrmA   + pOp1->lwrrmA;
    pResult->voltuV   = pOp0->voltuV   + pOp1->voltuV;
    LwU64_ALIGN32_ADD(&(pResult->energymJ), &(pOp0->energymJ), &(pOp1->energymJ));

s_perfCfPwrModelPowerTupleAdd_DLPPM_1X_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xNneStaticInputsSet
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneStaticInputsSet
(
    PERF_CF_PWR_MODEL_DLPPM_1X                                     *pDlppm1x,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED   *pDlppm1xObserved,
    NNE_DESC_INFERENCE                                             *pInference
)
{
    LwBoardObjIdx   baIdx;
    LwU16           dataIdx;
    FLCN_STATUS     status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pDlppm1x         == NULL) ||
         (pDlppm1xObserved == NULL) ||
         (pInference       == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        s_perfCfPwrModelDlppm1xNneStaticInputsSet_exit);

    //
    // If the cache has not been ilwalidated and the inference buffer still
    // holds the sequence number for this PWR_MODEL's metrics, then the inputs
    // are alredy set, so return early.
    //
    if ((pDlppm1xObserved->staticVarCache.parmRamSeqNum !=
            LW2080_CTRL_NNE_NNE_DESC_INFERENCE_PARM_RAM_SEQ_NUM_ILWALID) &&
        (pDlppm1xObserved->staticVarCache.parmRamSeqNum ==
            pInference->hdr.staticVarCache.parmRamSeqNum))
    {
        status = FLCN_OK;
        goto s_perfCfPwrModelDlppm1xNneStaticInputsSet_exit;
    }

    //
    // The inference structure is going to be completely reset, so ilwalidate
    // the cache.
    //
    LW2080_CTRL_NNE_NNE_DESC_INFERENCE_STATIC_VAR_CACHE_ILWALIDATE(
        &pInference->hdr.staticVarCache);

    // Write out the chip-config data.
    for (dataIdx = 0;
         dataIdx < LW2080_CTRL_NNE_NNE_VAR_CHIP_CONFIG_CONFIG_TYPE_NUM;
         dataIdx++)
    {
        pInference->pVarInputsStatic[dataIdx].type = LW2080_CTRL_NNE_NNE_VAR_TYPE_CHIP_CONFIG;
        pInference->pVarInputsStatic[dataIdx].data.config.configId.configType = dataIdx;

        switch (dataIdx)
        {
            case LW2080_CTRL_NNE_NNE_VAR_CHIP_CONFIG_CONFIG_TYPE_TPC:
                pInference->pVarInputsStatic[dataIdx].data.config.config
                     = pDlppm1xObserved->chipConfig.numTpc;
                break;

            case LW2080_CTRL_NNE_NNE_VAR_CHIP_CONFIG_CONFIG_TYPE_FBP:
                pInference->pVarInputsStatic[dataIdx].data.config.config
                     = pDlppm1xObserved->chipConfig.numFbp;
                break;
            case LW2080_CTRL_NNE_NNE_VAR_CHIP_CONFIG_CONFIG_TYPE_LTC_SLICE:
                pInference->pVarInputsStatic[dataIdx].data.config.config
                     = pDlppm1xObserved->chipConfig.numLtc;
                break;

            default:
                PMU_ASSERT_OK_OR_GOTO(status,
                    FLCN_ERR_NOT_SUPPORTED,
                    s_perfCfPwrModelDlppm1xNneStaticInputsSet_exit);
                break;
        }
    }

    // Write out the BA PM data.
    BOARDOBJGRPMASK_FOR_EACH_BEGIN(&pDlppm1x->pmSensorSignalMask, baIdx)
    {
        pInference->pVarInputsStatic[dataIdx].type               = LW2080_CTRL_NNE_NNE_VAR_TYPE_PM;
        pInference->pVarInputsStatic[dataIdx].data.pm.pmId.baIdx = baIdx;
        pInference->pVarInputsStatic[dataIdx].data.pm.pmCount    = pDlppm1xObserved->pmSensorDiff[baIdx];
        dataIdx++;
    }
    BOARDOBJGRPMASK_FOR_EACH_END;

    pInference->hdr.varInputCntStatic = dataIdx;

s_perfCfPwrModelDlppm1xNneStaticInputsSet_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xNneLoopInputsSet
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneLoopInputsSet
(
    PERF_CF_PWR_MODEL_DLPPM_1X                                     *pDlppm1x,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED   *pDlppm1xObserved,
    LwLength                                                        numMetricsInputs,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS                    **ppEstimatedMetrics,
    NNE_DESC_INFERENCE                                             *pInference
)
{
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X   *pDlppm1xEstimated;
    LwU8                                                   loopIdx;
    LwU8                                                   railIdx;
    LwU8                                                   varIdx = 0;
    FLCN_STATUS                                            status = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pDlppm1x           == NULL) ||
         (pDlppm1xObserved   == NULL) ||
         (ppEstimatedMetrics == NULL) ||
         (pInference         == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        s_perfCfPwrModelDlppm1xNneLoopInputsSet_exit);

    //
    // Populate the inputs that change across inference loops.
    // Number of loop variables consists of both Absolute and Relative frequencies
    // per rail which puts the count at 2*(#core rails + 1fb rail)
    // Absolute frequencies should be the frequency at which inference should occur
    // Relative frequencies should be such that: absolute-relative = observed
    //

    pInference->hdr.varInputCntLoop = s_perfCfPwrModelDlppm1xGetNumLoopVars(pDlppm1x);
    pInference->hdr.loopCnt         = numMetricsInputs;
    for (loopIdx = 0; loopIdx < pInference->hdr.loopCnt; loopIdx++)
    {
        varIdx            = 0;
        pDlppm1xEstimated = (LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *)ppEstimatedMetrics[loopIdx];

        for (railIdx = 0; railIdx < pDlppm1x->coreRail.numRails; railIdx++)
        {
            pInference->pLoops[loopIdx].pVarInputs[varIdx].type = LW2080_CTRL_NNE_NNE_VAR_TYPE_FREQ;

            pInference->pLoops[loopIdx].pVarInputs[varIdx]
                .data.freq.freqId.clkDomainIdx = pDlppm1x->coreRail.pRails[railIdx].clkDomIdx;
            pInference->pLoops[loopIdx].pVarInputs[varIdx]
                .data.freq.freqId.bAbsolute    = LW_TRUE;
            pInference->pLoops[loopIdx].pVarInputs[varIdx]
                .data.freq.freqMhz             = pDlppm1xEstimated->coreRail.rails[railIdx].freqkHz / 1000;
            varIdx++;

            pInference->pLoops[loopIdx].pVarInputs[varIdx].type = LW2080_CTRL_NNE_NNE_VAR_TYPE_FREQ;

            pInference->pLoops[loopIdx].pVarInputs[varIdx]
                .data.freq.freqId.clkDomainIdx = pDlppm1x->coreRail.pRails[railIdx].clkDomIdx;
            pInference->pLoops[loopIdx].pVarInputs[varIdx]
                .data.freq.freqId.bAbsolute    = LW_FALSE;
            pInference->pLoops[loopIdx].pVarInputs[varIdx]
                .data.freq.freqMhz             = ((LwS32)(pDlppm1xEstimated->coreRail.rails[railIdx].freqkHz -
                                                  pDlppm1xObserved->coreRail.rails[railIdx].super.freqkHz)) / 1000;
            varIdx++;
        }

        pInference->pLoops[loopIdx].pVarInputs[varIdx].type = LW2080_CTRL_NNE_NNE_VAR_TYPE_FREQ;

        pInference->pLoops[loopIdx].pVarInputs[varIdx]
            .data.freq.freqId.clkDomainIdx = pDlppm1x->fbRail.super.clkDomIdx;
        pInference->pLoops[loopIdx].pVarInputs[varIdx]
            .data.freq.freqId.bAbsolute    = LW_TRUE;
        pInference->pLoops[loopIdx].pVarInputs[varIdx]
            .data.freq.freqMhz             = pDlppm1xEstimated->fbRail.freqkHz / 1000;
        varIdx++;

        pInference->pLoops[loopIdx].pVarInputs[varIdx].type = LW2080_CTRL_NNE_NNE_VAR_TYPE_FREQ;

        pInference->pLoops[loopIdx].pVarInputs[varIdx]
                .data.freq.freqId.clkDomainIdx = pDlppm1x->fbRail.super.clkDomIdx;
        pInference->pLoops[loopIdx].pVarInputs[varIdx]
                .data.freq.freqId.bAbsolute    = LW_FALSE;
        pInference->pLoops[loopIdx].pVarInputs[varIdx].data.freq.freqMhz =
            ((LwS32)(pDlppm1xEstimated->fbRail.freqkHz - pDlppm1xObserved->fbRail.super.freqkHz)) / 1000;

        PMU_ASSERT_OK_OR_GOTO(status,
            (varIdx < pInference->hdr.varInputCntLoop) ? FLCN_OK:FLCN_ERR_ILWALID_STATE,
            s_perfCfPwrModelDlppm1xNneLoopInputsSet_exit);
    }

s_perfCfPwrModelDlppm1xNneLoopInputsSet_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xNneOutputsParseAndGuardRail
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneOutputsParseAndGuardRail
(
    PERF_CF_PWR_MODEL_DLPPM_1X                                             *pDlppm1x,
    NNE_DESC_INFERENCE                                                     *pInference,
    const PERF_CF_PWR_MODEL_SCALE_BOUNDS_DLPPM_1X                          *pDlppm1xBounds,
    const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED     *pDlppm1xObserved,
    LwLength                                                                numMetricsInputs,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS                            **ppEstimatedMetrics,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_SCALE_PROFILING    *pProfiling
)
{
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *ppDlppm1xEstimatedMetrics[
        LW2080_CTRL_NNE_NNE_DESC_INFERENCE_LOOPS_MAX];
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X   *pDlppm1xEstimated;
    LwU8                                                   loopIdx;
    LwU8                                                   railIdx;
    FLCN_STATUS                                            status = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pDlppm1x           == NULL) ||
         (pInference         == NULL) ||
         (ppEstimatedMetrics == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        s_perfCfPwrModelDlppm1xNneOutputsParseAndGuardRail_exit);

    perfCfPwrModelDlppm1xScaleProfileRegionBegin(
        pProfiling,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_DLPPM_1X_SCALE_PROFILING_REGION_NNE_OUTPUTS_PARSE);

    // Extract the metrics for each VF point that was estimated.
    for (loopIdx = 0; loopIdx < pInference->hdr.loopCnt; loopIdx++)
    {
        pDlppm1xEstimated =
            (LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *)ppEstimatedMetrics[loopIdx];
        ppDlppm1xEstimatedMetrics[loopIdx] = pDlppm1xEstimated;

        // Extract the core rail metrics.
        for (railIdx = 0; railIdx < pDlppm1x->coreRail.numRails; railIdx++)
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                s_perfCfPwrModelDlppm1xNneEstimatedPowerDynNormParse(
                    &(pDlppm1x->coreRail.pRails[railIdx]),
                    railIdx,
                    pInference->pLoops[loopIdx].pDescOutputs,
                    pInference->hdr.descOutputCnt,
                    &(pDlppm1xEstimated->coreRail.rails[railIdx]),
                    pDlppm1xObserved),
                s_perfCfPwrModelDlppm1xNneOutputsParseAndGuardRail_exit);
        }

        // Extract the FB rail metrics.
        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfPwrModelDlppm1xNneEstimatedFbvddPowerParse(
                &(pDlppm1x->fbRail),
                pInference->pLoops[loopIdx].pDescOutputs,
                pInference->hdr.descOutputCnt,
                &(pDlppm1xEstimated->fbRail),
                pDlppm1xObserved),
            s_perfCfPwrModelDlppm1xNneOutputsParseAndGuardRail_exit);

        // Parse the estimated perf.
        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfPwrModelDlppm1xNneEstimatedPerfParse(
                pInference->pLoops[loopIdx].pDescOutputs,
                pInference->hdr.descOutputCnt,
                &pDlppm1xEstimated->perfMetrics,
                pDlppm1xObserved),
            s_perfCfPwrModelDlppm1xNneOutputsParseAndGuardRail_exit);
    }

    perfCfPwrModelDlppm1xScaleProfileRegionEnd(
        pProfiling,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_DLPPM_1X_SCALE_PROFILING_REGION_NNE_OUTPUTS_PARSE);

    perfCfPwrModelDlppm1xScaleProfileRegionBegin(
        pProfiling,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_DLPPM_1X_SCALE_PROFILING_REGION_GUARD_RAIL);

    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelDlppm1xGuardRailPrimaryClk(
            pDlppm1x,
            pDlppm1xBounds,
            pDlppm1xObserved,
            ppDlppm1xEstimatedMetrics,
            pInference->hdr.loopCnt),
        s_perfCfPwrModelDlppm1xNneOutputsParseAndGuardRail_exit);

    perfCfPwrModelDlppm1xScaleProfileRegionEnd(
        pProfiling,
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_DLPPM_1X_SCALE_PROFILING_REGION_GUARD_RAIL);

s_perfCfPwrModelDlppm1xNneOutputsParseAndGuardRail_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xNneOutputsPropagate
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneOutputsPropagate
(
    PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
    const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
    LwLength numMetricsInputs,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS **ppEstimatedMetrics
)
{
    LwU8 i;
    FLCN_STATUS status = FLCN_OK;

    // Propagate the metrics for each VF point that was estimated.
    for (i = 0; i < numMetricsInputs; i++)
    {
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *const pDlppm1xEstimated =
            (LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *)ppEstimatedMetrics[i];

        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfPwrModelDlppm1xNneEstimatedPowerMetricsPropagate(
                pDlppm1x,
                pDlppm1xEstimated),
            s_perfCfPwrModelDlppm1xNneOutputsPropagate_exit);

        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfPwrModelDlppm1xNneEstimatedPerfPropagate(
                &pDlppm1xEstimated->perfMetrics,
                &pDlppm1xObserved->perfMetrics),
            s_perfCfPwrModelDlppm1xNneOutputsPropagate_exit);
    }

s_perfCfPwrModelDlppm1xNneOutputsPropagate_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xNneEstimatedPowerMetricsPropagate
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneEstimatedPowerMetricsPropagate
(
    PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *pDlppm1xEstimated
)
{
    FLCN_STATUS status;
    LwU8 railIdx;

    // Zero the core rail power tuple before summing the sub-rails into it.
    memset(&pDlppm1xEstimated->coreRail.pwrInTuple,  0,
           sizeof(pDlppm1xEstimated->coreRail.pwrInTuple));

    // Propagate the core rail metrics.
    for (railIdx = 0; railIdx < pDlppm1x->coreRail.numRails; railIdx++)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfPwrModelDlppm1xNneEstimatedPowerDynNormPropagate(
                &(pDlppm1x->coreRail.pRails[railIdx]),
                &(pDlppm1xEstimated->coreRail.rails[railIdx])),
            s_perfCfPwrModelDlppm1xNneEstimatedPowerMetricsPropagate_exit);

        // Accumulate the core rail power.
        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfPwrModelPowerTupleAdd_DLPPM_1X(
                &(pDlppm1xEstimated->coreRail.pwrInTuple),
                &(pDlppm1xEstimated->coreRail.rails[railIdx].pwrInTuple),
                &(pDlppm1xEstimated->coreRail.pwrInTuple)),
            s_perfCfPwrModelDlppm1xNneEstimatedPowerMetricsPropagate_exit);
    }

    // Propagate the FB rail metrics.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelDlppm1xNneEstimatedFbvddPowerPropagate(
            &pDlppm1x->fbRail,
            &pDlppm1xEstimated->fbRail),
        s_perfCfPwrModelDlppm1xNneEstimatedPowerMetricsPropagate_exit);

    //
    // Compute TGP metrics.
    // TGP = Core rail total input power + FB rail input power
    //       + small rail power.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelPowerTupleAdd_DLPPM_1X(
            &(pDlppm1xEstimated->coreRail.pwrInTuple),
            &(pDlppm1xEstimated->fbRail.pwrInTuple),
            &(pDlppm1xEstimated->tgpPwrTuple)),
        s_perfCfPwrModelDlppm1xNneEstimatedPowerMetricsPropagate_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelPowerTupleAdd_DLPPM_1X(
            &(pDlppm1xEstimated->tgpPwrTuple),
            &(pDlppm1xEstimated->smallRailPwrTuple),
            &(pDlppm1xEstimated->tgpPwrTuple)),
        s_perfCfPwrModelDlppm1xNneEstimatedPowerMetricsPropagate_exit);

s_perfCfPwrModelDlppm1xNneEstimatedPowerMetricsPropagate_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xNneEstimatedPowerDynNormParse
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneEstimatedPowerDynNormParse
(
    PERF_CF_PWR_MODEL_DLPPM_1X_RAIL                                    *pRail,
    LwU8                                                                railIdx,
    LW2080_CTRL_NNE_NNE_DESC_OUTPUT                                    *pOutputs,
    LwU8                                                                numOutputs,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_RAIL           *pRailMetrics,
    const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved
)
{
    LwU8                    outputIdx;
    FLCN_STATUS             status = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pRail        == NULL) ||
         (pOutputs     == NULL) ||
         (pRailMetrics == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        s_perfCfPwrModelDlppm1xNneEstimatedPowerDynNormParse_exit);

    // Search for the normalized estimated power output for the requested rail.
    for (outputIdx = 0; outputIdx < numOutputs; outputIdx++)
    {
        if ((pOutputs[outputIdx].type                        == LW2080_CTRL_NNE_NNE_DESC_OUTPUT_TYPE_POWER_DN) &&
            (pOutputs[outputIdx].data.powerDN.id.voltRailIdx == pRail->voltRailIdx))
        {
            pRailMetrics->pwrOutDynamicNormalizedmW = pOutputs[outputIdx].data.powerDN.powermW;
            break;
        }
    }

    // Error out if a estimated power wasn't found for the requested rail.
    PMU_ASSERT_OK_OR_GOTO(status,
        (outputIdx == numOutputs) ? FLCN_ERR_NOT_SUPPORTED : FLCN_OK,
        s_perfCfPwrModelDlppm1xNneEstimatedPowerDynNormParse_exit);

    //
    // Numerical Analysis:
    // We need to callwlate:
    //  pwrOutDynamicNormalizedmW' = pwrOutDynamicNormalizedmW * correction
    //
    // pwrOutDynamicNormalizedmW is a 32.0; correction is a 20.12; and
    // we need pwrOutDynamicNormalizedmW' to again be a 32.0.
    //
    // To get this, we simply multiply the 32.0 and 20.12 and then shift off the
    // additional fractional bits.
    //
    // This is safe because we can assume, via CORRECTION_BOUND, that the
    // correction percentage is between 1/10 and 10; this can add at most 3
    // significant integer or fractional bits, and since we assume power values
    // are at most 20.0 significant bits, the result is at most 23.3 significant
    // bits. The integer portion still fits within a 32.0 and the fractional
    // bits are fractions of a milliwatt, which are safe to discard.
    //
    // TODO-aherring: revisit the bounds percentage if we tighten them.
    //
    pRailMetrics->pwrOutDynamicNormalizedmW = multUFXP32(
        12,
        pRailMetrics->pwrOutDynamicNormalizedmW,
        pDlppm1xObserved->correction.coreRailPwrPct[railIdx]);

s_perfCfPwrModelDlppm1xNneEstimatedPowerDynNormParse_exit:
    return status;
}
/*!
 * @copydoc s_perfCfPwrModelDlppm1xNneEstimatedPowerDynNormPropagate
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneEstimatedPowerDynNormPropagate
(
    PERF_CF_PWR_MODEL_DLPPM_1X_RAIL *pRail,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_RAIL *pRailMetrics
)
{
    FLCN_STATUS             status;
    PWR_EQUATION_DYNAMIC   *pDyn;
    VOLT_RAIL              *pVoltRail;
    LwUFXP52_12             tmpUFXP52_12;
    LwU64                   tmpU64;
    LwU8                    dynamicEquIdx;

    //
    // TODO @achaudhry--add function that will correct
    // pwrOutDynamicNormalizedmW and updates
    // pwrOutDynamicNormalizedmWPostPerfCorrection with the corrected value.
    // For now, copy over pwrOutDynamicNormalizedmW to pwrOutDynamicNormalizedmWPostPerfCorrection
    //
    pRailMetrics->pwrOutDynamicNormalizedmWPostPerfCorrection =
        pRailMetrics->pwrOutDynamicNormalizedmW;

    // De-normalize the output power.
    pVoltRail = VOLT_RAIL_GET(pRail->voltRailIdx);
    PMU_ASSERT_OK_OR_GOTO(status,
        (pVoltRail == NULL) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        s_perfCfPwrModelDlppm1xNneEstimatedPowerDynNormPropagate_exit);

    dynamicEquIdx = voltRailVoltScaleExpPwrEquIdxGet(pVoltRail);
    PMU_ASSERT_OK_OR_GOTO(status,
        (dynamicEquIdx == LW2080_CTRL_PMGR_PWR_EQUATION_INDEX_ILWALID) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
        s_perfCfPwrModelDlppm1xNneEstimatedPowerDynNormPropagate_exit);

    pDyn = (PWR_EQUATION_DYNAMIC *)PWR_EQUATION_GET(dynamicEquIdx);
    PMU_ASSERT_OK_OR_GOTO(status,
        (pDyn == NULL) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
        s_perfCfPwrModelDlppm1xNneEstimatedPowerDynNormPropagate_exit);

    //
    // Compute the voltage scaling factor.
    //
    // Numerical Analysis:
    //
    // Vmax = 1.5V = 1500000uV => 21-bit integer
    //
    // Colwerting to UFXP52_12 in volts ilwolves a 12-bit left shift,
    // then a division by 1000000.
    //
    // 21-bit integer << 12 = UFXP 21.12 = 33-bits.
    //
    // Division will only decrease the number of bits used. Ergo,
    // the maximum number of bits ever used is 33-bits.
    //
    tmpUFXP52_12 = LW_TYPES_U64_TO_UFXP_X_Y_SCALED(52, 12, pRailMetrics->pwrOutTuple.voltuV, 1000000);
    PMU_ASSERT_OK_OR_GOTO(status,
        (LW_TYPES_UFXP_X_Y_TO_U64(52, 12, tmpUFXP52_12) >
             LW_TYPES_UFXP_INTEGER_MAX(20, 12)) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
        s_perfCfPwrModelDlppm1xNneEstimatedPowerDynNormPropagate_exit);
    tmpUFXP52_12 = (LwUFXP52_12)pwrEquationDynamicScaleVoltagePower(pDyn,
                                                                    (LwUFXP20_12)tmpUFXP52_12);

    //
    // Compute dynamic power:
    // Pdyn = Pdyn_norm * Vscale_factor
    //
    // Numerical Analysis:
    //
    // Assume Vmax = 1.5[V], dynamicExp = 2.4.
    // Therefore, max voltScaleFactor = 1.5^2.4 = 2.65 => UFXP 3.12.
    //
    // Assume max 1.0[V] normalized dynamic power is 500[W] = 500000[mW] => 19-bit integer.
    // 19-bit integer * UFXP 3.12 = UFXP 22.12[mW]
    //
    // Desired output is U32 power in mW. Integer portion of the dynamic
    // power is guarenteed to be at most 22-bits, which fits in 32-bits.
    //
    tmpU64 = pRailMetrics->pwrOutDynamicNormalizedmWPostPerfCorrection;
    lwU64Mul(&tmpUFXP52_12, &tmpUFXP52_12, &tmpU64);
    tmpU64 = LW_TYPES_UFXP_X_Y_TO_U64_ROUNDED(52, 12, tmpUFXP52_12);
    PMU_ASSERT_OK_OR_GOTO(status,
        (LwU64_HI32(tmpU64) != 0) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
        s_perfCfPwrModelDlppm1xNneEstimatedPowerDynNormPropagate_exit);
    pRailMetrics->pwrOutTuple.pwrmW = LwU64_LO32(tmpU64);

    // Compute the rail current.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelDlppm1xLwrrentSolve(
             pRailMetrics->pwrOutTuple.pwrmW,
             pRailMetrics->pwrOutTuple.voltuV,
             &pRailMetrics->pwrOutTuple.lwrrmA),
        s_perfCfPwrModelDlppm1xNneEstimatedPowerDynNormPropagate_exit);

    // Add the leakage to get the total output power.
    PMU_ASSERT_OK_OR_GOTO(status,
        voltRailGetLeakage(
            pVoltRail,
            pRailMetrics->pwrOutTuple.voltuV,
            LW_FALSE,
            NULL,
            &(pRailMetrics->leakagePwrmW)),
        s_perfCfPwrModelDlppm1xNneEstimatedPowerDynNormPropagate_exit);
    pRailMetrics->pwrOutTuple.pwrmW += pRailMetrics->leakagePwrmW;

    // Colwert from output to input power.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelDlppm1xOutPwrToInPwrMetrics(
            pRail,
            pRailMetrics),
        s_perfCfPwrModelDlppm1xNneEstimatedPowerDynNormPropagate_exit);

s_perfCfPwrModelDlppm1xNneEstimatedPowerDynNormPropagate_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xNneEstimatedFbvddPowerParse
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneEstimatedFbvddPowerParse
(
    PERF_CF_PWR_MODEL_DLPPM_1X_FBVDD_RAIL                              *pRail,
    LW2080_CTRL_NNE_NNE_DESC_OUTPUT                                    *pOutputs,
    LwU8                                                                numOutputs,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_RAIL           *pRailMetrics,
    const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved
)
{
    LwU8                outputIdx;
    FLCN_STATUS         status = FLCN_OK;
    CLK_DOMAIN          *pDomainDram;
    CLK_DOMAIN_PROG     *pDomainProgDram;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pRail        == NULL) ||
         (pOutputs     == NULL) ||
         (pRailMetrics == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        s_perfCfPwrModelDlppm1xNneEstimatedFbvddPowerParse_exit);

    // Search for the normalized estimated power output for the requested rail.
    for (outputIdx = 0; outputIdx < numOutputs; outputIdx++)
    {
        if ((pOutputs[outputIdx].type                            == LW2080_CTRL_NNE_NNE_DESC_OUTPUT_TYPE_POWER_TOTAL) &&
            (pOutputs[outputIdx].data.powerTotal.id.voltRailName == pRail->voltRailName))
        {
            pRailMetrics->pwrOutTuple.pwrmW = pOutputs[outputIdx].data.powerTotal.powermW;
            break;
        }
    }

    // Error out if a estimated power wasn't found for the requested rail.
    PMU_ASSERT_OK_OR_GOTO(status,
        (outputIdx == numOutputs) ? FLCN_ERR_NOT_SUPPORTED : FLCN_OK,
        s_perfCfPwrModelDlppm1xNneEstimatedFbvddPowerParse_exit);

    //
    // Retrieve the DRAMCLK clock domain and adjust the power to the current
    // memory configuration.
    //
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pDomainDram = BOARDOBJGRP_OBJ_GET(
                CLK_DOMAIN, pRail->super.clkDomIdx)) != NULL),
        FLCN_ERR_ILWALID_STATE,
        s_perfCfPwrModelDlppm1xNneEstimatedFbvddPowerParse_exit);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pDomainProgDram = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(
                pDomainDram, PROG)) != NULL),
        FLCN_ERR_ILWALID_STATE,
        s_perfCfPwrModelDlppm1xNneEstimatedFbvddPowerParse_exit);
    PMU_ASSERT_OK_OR_GOTO(status,
        clkDomainProgFbvddPwrAdjust(
            pDomainProgDram,
            &pRailMetrics->pwrOutTuple.pwrmW,
            LW_TRUE),
        s_perfCfPwrModelDlppm1xNneEstimatedFbvddPowerParse_exit);

    //
    // Numerical Analysis:
    // We need to callwlate:
    //  pwrmW' = pwrmW * correction
    //
    // pwrmW is a 32.0; correction is a 20.12; and we need pwrmW' to again be a
    // 32.0.
    //
    // To get this, we simply multiply the 32.0 and 20.12 and then shift off the
    // additional fractional bits.
    //
    // This is safe because we can assume, via CORRECTION_BOUND, that the
    // correction percentage is between 1/10 and 10; this can add at most 3
    // significant integer or fractional bits, and since we assume power values
    // are at most 20.0 significant bits, the result is at most 23.3 significant
    // bits. The integer portion still fits within a 32.0 and the fractional
    // bits are fractions of a milliwatt, which are safe to discard.
    //
    // TODO-aherring: revisit the bounds percentage if we tighten them.
    //
    pRailMetrics->pwrOutTuple.pwrmW = multUFXP32(
        12,
        pRailMetrics->pwrOutTuple.pwrmW,
        pDlppm1xObserved->correction.fbPwrPct);

s_perfCfPwrModelDlppm1xNneEstimatedFbvddPowerParse_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xNneEstimatedFbvddPowerPropagate
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneEstimatedFbvddPowerPropagate
(
    PERF_CF_PWR_MODEL_DLPPM_1X_FBVDD_RAIL *pRail,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_RAIL *pRailMetrics
)
{
    FLCN_STATUS status;

    // Compute rail current.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelDlppm1xLwrrentSolve(
             pRailMetrics->pwrOutTuple.pwrmW,
             pRailMetrics->pwrOutTuple.voltuV,
             &pRailMetrics->pwrOutTuple.lwrrmA),
        s_perfCfPwrModelDlppm1xNneEstimatedFbvddPowerPropagate_exit);

    // Colwert output power metrics to input power metrics.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelDlppm1xOutPwrToInPwrMetrics(
            &(pRail->super),
            pRailMetrics),
        s_perfCfPwrModelDlppm1xNneEstimatedFbvddPowerPropagate_exit);

s_perfCfPwrModelDlppm1xNneEstimatedFbvddPowerPropagate_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xOutPwrToInPwrMetrics
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xOutPwrToInPwrMetrics
(
    PERF_CF_PWR_MODEL_DLPPM_1X_RAIL                            *pRail,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_RAIL   *pRailMetrics
)
{
    FLCN_STATUS   status = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pRail        == NULL) ||
         (pRailMetrics == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        s_perfCfPwrModelDlppm1xOutPwrToInPwrMetrics_exit);

    // Callwlate output power using the VR efficency
    PWR_CHRELATIONSHIP_REGULATOR_EFF_EST_V1 *pVrEfficencyChRel =
            (PWR_CHRELATIONSHIP_REGULATOR_EFF_EST_V1 *)PWR_CHREL_GET(pRail->vrEfficiencyChRelIdxOutToIn);

    PMU_ASSERT_TRUE_OR_GOTO(status,
        pVrEfficencyChRel != NULL,
        FLCN_ERR_OBJECT_NOT_FOUND,
        s_perfCfPwrModelDlppm1xOutPwrToInPwrMetrics_exit);

    //Note: pwrInTuple is pre-populated with the observed input voltage
    PMU_ASSERT_OK_OR_GOTO(status,
        pwrChRelationshipRegulatorEffEstV1EvalSecondary(
            pVrEfficencyChRel,
            &(pRailMetrics->pwrOutTuple),
            &(pRailMetrics->pwrInTuple)),
        s_perfCfPwrModelDlppm1xOutPwrToInPwrMetrics_exit);

    // Do not populate energy field as it is not needed.

s_perfCfPwrModelDlppm1xOutPwrToInPwrMetrics_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xNneEstimatedPerfParse
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneEstimatedPerfParse
(
    LW2080_CTRL_NNE_NNE_DESC_OUTPUT                                    *pOutputs,
    LwU8                                                                numOutputs,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_PERF           *pPerfMetrics,
    const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved
)
{
    LwU8          outputIdx;
    FLCN_STATUS   status                        = FLCN_OK;
    LwUFXP40_24 perfmsObserved40_24;
    LwU64 result64;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pOutputs == NULL) ||
         (pPerfMetrics == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        s_perfCfPwrModelDlppm1xNneEstimatedPerfParse_exit);

    // Search for the normalized estimated power output for the requested rail.
    for (outputIdx = 0; outputIdx < numOutputs; outputIdx++)
    {
        // Search for the perf output
        if (pOutputs[outputIdx].type == LW2080_CTRL_NNE_NNE_DESC_OUTPUT_TYPE_PERF)
        {
            pPerfMetrics->perfms = pOutputs[outputIdx].data.perf.perf;
            break;
        }
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        (outputIdx == numOutputs) ? FLCN_ERR_ILWALID_INDEX : FLCN_OK,
        s_perfCfPwrModelDlppm1xNneEstimatedPerfParse_exit);

    //
    // Numerical Analysis:
    // We need to callwlate:
    //  perfms' = perfms * correction
    //
    // perfms is a 20.12.; correction is a 20.12; and we need perfms' to again
    // be a 20.12.
    //
    // To get this, we simply multiply the two 20.12s and then shift off the
    // additional fractional bits.
    //
    // This is safe because we can assume, via CORRECTION_BOUND, that the
    // correction percentage is between 1/10 and 10; this can add at most 3
    // significant integer or fractional bits. Being conservative, we might
    // expect the estimated perfms to be up to 4096 ms, which requires 12 bits
    // to represent, so the result is at most 15.15 significant bits. The
    // integer portion still fits within a 20.12, and the additional fractional
    // bits are small enough to discard.
    //
    // TODO-aherring: revisit the bounds percentage if we tighten them.
    //
    pPerfMetrics->perfms = multUFXP32(
        12,
        pPerfMetrics->perfms,
        pDlppm1xObserved->correction.perfPct);

    //
    // If either of the perfms values is INVALID, then set the ratio to INVALID
    // as well and then return.
    //
    if ((pPerfMetrics->perfms == LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_PERF_PERFMS_ILWALID) ||
        (pDlppm1xObserved->perfMetrics.perfms == LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_PERF_PERFMS_ILWALID))
    {
        pPerfMetrics->perfRatio =
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_PERF_PERF_RATIO_ILWALID;
        goto s_perfCfPwrModelDlppm1xNneEstimatedPerfParse_exit;
    }

    // 
    // Numerical Analysis:
    //
    // We need to divide a 20.12 by a 20.12 to get a result of 20.12.
    //
    // We want to first shift the numerator left by 12 to retain
    // precision. It is possible that the observed perfms could overflow a
    // 20.12 when shifted. The observed perfms is expected to be ~200 ms.
    // Rounding this up to 256 means that 9 bits are required, and shifting
    // 0x100 left by twelve would require 21 bits. Therefore, we first cast
    // to LwU64, shift left by 12, and place the result in a 40.24.
    //
    // We then divide the 40.24 by a 20.12. This produces a result that is
    // 20.12 in precision, as we wanted.
    //
    lw64Lsl(
        &perfmsObserved40_24,
        &(LwU64){ pDlppm1xObserved->perfMetrics.perfms },
        12);

    lwU64Div(
        &result64,
        &perfmsObserved40_24,
        &(LwU64){ pPerfMetrics->perfms });

    // If we've overflowed 32 bits, then set the ratio to the maximum.
    if (lwU64CmpLE(&result64, &(LwU64){ LW_U32_MAX }))
    {
        pPerfMetrics->perfRatio = (LwUFXP20_12)result64;
    }
    else
    {
        pPerfMetrics->perfRatio = LW_U32_MAX;
    }

s_perfCfPwrModelDlppm1xNneEstimatedPerfParse_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xNneEstimatedPerfPropagate
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneEstimatedPerfPropagate
(
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_PERF *pPerfMetrics,
    const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_PERF *pObservedPerfMetrics
)
{
    LwUFXP40_24 perfmsObserved40_24;
    LwUFXP52_12 perfmsEstimated52_12;

    // If the perfRatio is invalid, mark the perfms as invalid too.
    if (pPerfMetrics->perfRatio ==
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_PERF_PERF_RATIO_ILWALID)
    {
        pPerfMetrics->perfms =
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_PERF_PERFMS_ILWALID;
        goto s_perfCfPwrModelDlppm1xNneEstimatedPerfPropagate_exit;
    }

    //
    // If pPerfMetrics->perfRatio is 0 then pPerfMetrics->perfMs is
    // infinetly large, modelling as LW_U32_MAX.
    //
    if (pPerfMetrics->perfRatio == 0)
    {
        pPerfMetrics->perfms = LW_U32_MAX;
        goto s_perfCfPwrModelDlppm1xNneEstimatedPerfPropagate_exit;
    }

    //
    // Numerical Analysis:
    // perfms_{estimated} = perfms_{observed} / perfRatio_{estimated}
    //
    // We expect the perfms_{observed} to be ~200ms. Being extremely
    // conservative with this, we can round its expected range up to [0, 4096].
    // 4096 = 2^12, so perfms_{observed} has 12.12 significant bits.
    //
    // perfRatio_{estimated} is a 20.12, however, so to retain precision, we
    // must first shift permfs_{observed} left by 12 bits. This requires 12.24
    // significant bits, so we do the intermediate callwlations in 64-bit.
    //
    // Via guard-railing, we have ensured that perfRatio_{estimated} is linear
    // with respect to both primary clock and DRAMCLK. The maximum expected
    // primary clock range is [210 MHz, 2100 MHz] and the maximum expected
    // DRAMCLK range is [405 MHz, 10000 MHz]. The maximum increase across primary
    // clock is therefore:
    //  2100 MHz / 210 MHz
    //  10
    // and the maximum increase across DRAMCLK is therefore:
    //  10000 MHz / 405 MHz
    //  24.69 ~= 25
    //
    // The observed perfRatio is hard-coded to be 1. Therefore, conservatively,
    // perfRatio can be between [1 / 250, 1 * 250]. This corresponds to
    // approximately 8.8 significant bits for perffRatio_{estimated}.
    //
    // TODO-aherring: The above isn't quite true, because we don't always
    // guard-rail the estimated metrics against the observed metrics, so there's
    // not always something ensuring the relationship between observed and a
    // set of estimated metrics. This can be removed if/when we do
    // non-matching-DRAMCLK guard-railing.
    //
    // This means the intermediate callwlation may require up to 20.24
    // significant bits, but we shift off the additional precision and end up
    // with 20.12 significant bits, which fits safely back into a 20.12.
    //
    lw64Lsl(
        &perfmsObserved40_24,
        &(LwU64){ pObservedPerfMetrics->perfms },
        12U);
    lwU64Div(
        &perfmsEstimated52_12,
        &perfmsObserved40_24,
        &(LwU64){ pPerfMetrics->perfRatio });

    // If overflow oclwrs, then simply use the maximum value.
    if (lwU64CmpLE(&perfmsEstimated52_12, &(LwU64){ LW_U32_MAX }))
    {
        pPerfMetrics->perfms = (LwUFXP20_12)perfmsEstimated52_12;
    }
    else
    {
        pPerfMetrics->perfms = (LwUFXP20_12)LW_U32_MAX;
    }

s_perfCfPwrModelDlppm1xNneEstimatedPerfPropagate_exit:
    return FLCN_OK;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xLwrrentSolve
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xLwrrentSolve
(
    LwU32    pwrmW,
    LwU32    voltuV,
    LwU32   *pLwrrmA
)
{
    LwU64         tmpU64_0;
    LwU64         tmpU64_1;
    FLCN_STATUS   status = FLCN_OK;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        (pLwrrmA == NULL) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        s_perfCfPwrModelDlppm1xLwrrentSolve_exit);

    // Handle if voltage is 0 due to rail-gating.
    if (voltuV == 0)
    {
        *pLwrrmA = 0;
        goto s_perfCfPwrModelDlppm1xLwrrentSolve_exit;
    }

    //
    // current[mA] = power[mW] * 100000 / voltage[uV]
    //
    // Numerical Analysis:
    // Max dynamic power = 500[W] = 500000[mW] => 19-bit integer.
    // 100000 => 17-bit integer.
    //
    // 19-bit integer * 17-bit integer = 36-bit integer.
    //
    // Dividing by voltage[uV] can only decrease the number of bits used.
    // Therefore, the maximum intermediate bitwidth is 36-bits.
    //
    tmpU64_0 = pwrmW;
    tmpU64_1 = 1000000;
    lwU64Mul(&tmpU64_0, &tmpU64_0, &tmpU64_1);

    tmpU64_1 = voltuV;
    lwU64Div(&tmpU64_0, &tmpU64_0, &tmpU64_1);
    PMU_ASSERT_OK_OR_GOTO(status,
        (LwU64_HI32(tmpU64_0) != 0) ? FLCN_ERROR : FLCN_OK,
        s_perfCfPwrModelDlppm1xLwrrentSolve_exit);
    *pLwrrmA = LwU64_LO32(tmpU64_0);

s_perfCfPwrModelDlppm1xLwrrentSolve_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xObservedMetricsNormalize
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xObservedMetricsNormalize
(
    PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
    LwBool *bWithinBounds
)
{
    FLCN_STATUS status;
    LwF32 normRatio;

    // Sanity Check
    PMU_ASSERT_TRUE_OR_GOTO(status,
        bWithinBounds != NULL,
        FLCN_ERR_ILWALID_ARGUMENT,
        s_perfCfPwrModelDlppm1xObservedMetricsNormalize_exit);

    *bWithinBounds = LW_TRUE;

    // Populate the static inputs into the staging buffer.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelDlppm1xNneStaticInputsSet(
            pDlppm1x,
            pDlppm1xObserved,
            &inferenceStage),
        s_perfCfPwrModelDlppm1xObservedMetricsNormalize_exit);

    // Use the inference buffer to normalize the inputs
    PMU_ASSERT_OK_OR_GOTO(status,
        nneVarsVarInputsNormalize(
            inferenceStage.pVarInputsStatic,
            inferenceStage.hdr.varInputCntStatic,
            pDlppm1x->nneDescIdx,
            &(pDlppm1xObserved->inputNormStatus),
            &normRatio),
        s_perfCfPwrModelDlppm1xObservedMetricsNormalize_exit);

    //
    // If normalization sees that the inputs are not valid, set bWithinBounds
    // to False and exit early
    //
    if (pDlppm1xObserved->inputNormStatus.appliedMode ==
            LW2080_CTRL_NNE_NNE_DESC_INPUT_NORM_MODE_POISON)
    {
        *bWithinBounds = LW_FALSE;
        goto s_perfCfPwrModelDlppm1xObservedMetricsNormalize_exit;
    }

    // Extract the normalized inputs from the inference buffer.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelDlppm1xNneInputsExtract(
            pDlppm1x,
            pDlppm1xObserved,
            inferenceStage.pVarInputsStatic,
            inferenceStage.hdr.varInputCntStatic),
        s_perfCfPwrModelDlppm1xObservedMetricsNormalize_exit);

    // Save off the normalization ratio
    pDlppm1xObserved->normRatio = lwF32MapToU32(&normRatio);

    //
    // If we applied secondary normalization to the PM signals, then we should
    // normalize perfms by the same amount.
    //
    if (pDlppm1xObserved->inputNormStatus.appliedMode ==
            LW2080_CTRL_NNE_NNE_DESC_INPUT_NORM_MODE_SECONDARY)
    {
        //
        // Normalize perfms. The perfms value is in 20.12 FXP and normRatio is
        // in F32. First colwert the FXP value to F32, then do the math, and
        // then colwert it back to U32 (i.e., 20.12 FXP).
        //
        // Numerical Analysis:
        // When colwerted to floating point, the 20.12 is just an integer-valued
        // floating point number.
        //
        // When divided by a floating point, this produces a smaller floating
        // point integer, though now with potentially fractional bits.
        //
        // When casted back to LwU32/20.12, this is still the original integer
        // value divided by the floating point divisor, so this is still an
        // appropriate 20.12.
        //
        pDlppm1xObserved->perfMetrics.perfms = lwF32ColwertToU32(
            lwF32Div(
                lwF32ColwertFromU32(pDlppm1xObserved->perfMetrics.perfms),
                normRatio));
    }

s_perfCfPwrModelDlppm1xObservedMetricsNormalize_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xRailGetMetricsForFreqId
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xRailGetMetricsForFreqId
(
    PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
    const LW2080_CTRL_NNE_VAR_ID_FREQ *pId,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_RAIL **ppRailMetrics
)
{
    FLCN_STATUS status;
    LwU8 railIdx;

    for (railIdx = 0U; railIdx < pDlppm1x->coreRail.numRails; railIdx++)
    {
        PERF_CF_PWR_MODEL_DLPPM_1X_RAIL *const pRail =
            &pDlppm1x->coreRail.pRails[railIdx];
        if (pId->clkDomainIdx == pRail->clkDomIdx)
        {
            *ppRailMetrics =
                &pDlppm1xObserved->coreRail.rails[railIdx].super;
            status = FLCN_OK;
            goto s_perfCfPwrModelDlppm1xRailGetMetricsForFreqId_exit;
        }
    }

    if (pId->clkDomainIdx == pDlppm1x->fbRail.super.clkDomIdx)
    {
        *ppRailMetrics =
            &pDlppm1xObserved->fbRail.super;
        status = FLCN_OK;
        goto s_perfCfPwrModelDlppm1xRailGetMetricsForFreqId_exit;
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        FLCN_ERR_OBJECT_NOT_FOUND,
        s_perfCfPwrModelDlppm1xRailGetMetricsForFreqId_exit);

s_perfCfPwrModelDlppm1xRailGetMetricsForFreqId_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xNneInputsExtract
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneInputsExtract
(
    PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
    const LW2080_CTRL_NNE_NNE_VAR_INPUT *pInputs,
    LwLength numInputs
)
{
    FLCN_STATUS status;
    LwLength inputIdx;

    for (inputIdx = 0U; inputIdx < numInputs; inputIdx++)
    {
        const LW2080_CTRL_NNE_NNE_VAR_INPUT *const pInput = &pInputs[inputIdx];

        switch (pInput->type)
        {
            case LW2080_CTRL_NNE_NNE_VAR_TYPE_FREQ:
                status = s_perfCfPwrModelDlppm1xNneInputExtract_FREQ(
                    pDlppm1x,
                    pDlppm1xObserved,
                    &pInput->data.freq);
                break;
            case LW2080_CTRL_NNE_NNE_VAR_TYPE_PM:
                status = s_perfCfPwrModelDlppm1xNneInputExtract_PM(
                    pDlppm1x,
                    pDlppm1xObserved,
                    &pInput->data.pm);
                break;
            case LW2080_CTRL_NNE_NNE_VAR_TYPE_CHIP_CONFIG:
                status = s_perfCfPwrModelDlppm1xNneInputExtract_CHIP_CONFIG(
                    pDlppm1x,
                    pDlppm1xObserved,
                    &pInput->data.config);
                break;
            case LW2080_CTRL_NNE_NNE_VAR_TYPE_POWER_DN:
                status = s_perfCfPwrModelDlppm1xNneInputExtract_POWER_DN(
                    pDlppm1x,
                    pDlppm1xObserved,
                    &pInput->data.powerDn);
                break;
            case LW2080_CTRL_NNE_NNE_VAR_TYPE_POWER_TOTAL:
                status = s_perfCfPwrModelDlppm1xNneInputExtract_POWER_TOTAL(
                    pDlppm1x,
                    pDlppm1xObserved,
                    &pInput->data.powerTotal);
                break;
            default:
                status = FLCN_ERR_NOT_SUPPORTED;
                break;
        }

        PMU_ASSERT_OK_OR_GOTO(status, status,
            s_perfCfPwrModelDlppm1xNneInputsExtract_exit);
    }

s_perfCfPwrModelDlppm1xNneInputsExtract_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xNneInputExtract_FREQ
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneInputExtract_FREQ
(
    PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
    const LW2080_CTRL_NNE_NNE_VAR_INPUT_FREQ *pInput
)
{
    FLCN_STATUS status;
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_RAIL *pRailMetrics;

    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelDlppm1xRailGetMetricsForFreqId(
            pDlppm1x,
            &pInput->freqId,
            pDlppm1xObserved,
            &pRailMetrics),
        s_perfCfPwrModelDlppm1xNneInputExtract_FREQ_exit);

    pRailMetrics->freqkHz = pInput->freqMhz * 1000U;

s_perfCfPwrModelDlppm1xNneInputExtract_FREQ_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xNneInputExtract_PM
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneInputExtract_PM
(
    PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
    const LW2080_CTRL_NNE_NNE_VAR_INPUT_PM *pInput
)
{
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        pInput->pmId.baIdx < LW_ARRAY_ELEMENTS(pDlppm1xObserved->pmSensorDiff) ?
            FLCN_OK : FLCN_ERR_NOT_SUPPORTED,
        s_perfCfPwrModelDlppm1xNneInputExtract_PM_exit);

    pDlppm1xObserved->pmSensorDiff[pInput->pmId.baIdx] = pInput->pmCount;

s_perfCfPwrModelDlppm1xNneInputExtract_PM_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xNneInputExtract_CHIP_CONFIG
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneInputExtract_CHIP_CONFIG
(
    PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
    const LW2080_CTRL_NNE_NNE_VAR_INPUT_CHIP_CONFIG *pInput
)
{
    FLCN_STATUS status;

    status = FLCN_OK;
    switch (pInput->configId.configType)
    {
        case LW2080_CTRL_NNE_NNE_VAR_CHIP_CONFIG_CONFIG_TYPE_TPC:
            pDlppm1x->chipConfig.numTpc = pInput->config;
            break;
        case LW2080_CTRL_NNE_NNE_VAR_CHIP_CONFIG_CONFIG_TYPE_FBP:
            pDlppm1x->chipConfig.numFbp = pInput->config;
            break;
        case LW2080_CTRL_NNE_NNE_VAR_CHIP_CONFIG_CONFIG_TYPE_LTC_SLICE:
            pDlppm1x->chipConfig.numLtc = pInput->config;
            break;
        default:
            status = FLCN_ERR_NOT_SUPPORTED;
            break;
    }

    PMU_ASSERT_OK_OR_GOTO(status, status,
        s_perfCfPwrModelDlppm1xNneInputExtract_CHIP_CONFIG_exit);

s_perfCfPwrModelDlppm1xNneInputExtract_CHIP_CONFIG_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xNneInputExtract_POWER_DN
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneInputExtract_POWER_DN
(
    PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
    const LW2080_CTRL_NNE_NNE_VAR_INPUT_POWER_DN *pInput
)
{
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        FLCN_ERR_NOT_SUPPORTED,
        s_perfCfPwrModelDlppm1xNneInputExtract_POWER_DN_exit);

s_perfCfPwrModelDlppm1xNneInputExtract_POWER_DN_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xNneInputExtract_POWER_TOTAL
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xNneInputExtract_POWER_TOTAL
(
    PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
    const LW2080_CTRL_NNE_NNE_VAR_INPUT_POWER_TOTAL *pInput
)
{
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        FLCN_ERR_NOT_SUPPORTED,
        s_perfCfPwrModelDlppm1xNneInputExtract_POWER_TOTAL_exit);

s_perfCfPwrModelDlppm1xNneInputExtract_POWER_TOTAL_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xGuardRailPrimaryClk
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xGuardRailPrimaryClk
(
    PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
    const PERF_CF_PWR_MODEL_SCALE_BOUNDS_DLPPM_1X *pDlppm1xBounds,
    const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X **ppDlppm1xEstimated,
    LwU8 numEstimatedMetrics
)
{
    FLCN_STATUS status;
    const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_CORE_RAIL_POWER_FIT *pCoreRailFit;
    LwU8 i;
    LwU8 downStartIdx;
    LwU8 upStartIdx;

    // Exit early if the guard rails shouldn't be applied.
    if (!pDlppm1xBounds->bGuardRailsApply)
    {
        status = FLCN_OK;
        goto s_perfCfPwrModelDlppm1xGuardRailPrimaryClk_exit;
    }

    //
    // If this is a call with endpoints, it should match an initial DRAMCLK
    // estimate, so find that estimate, and get the appropriate core rail power
    // vs. perf fit.
    //
    if (pDlppm1xBounds->pEndpointLo != NULL)
    {
        LwU8 dramclkIdx;

        // If we have one endpoint, we should have both
        PMU_ASSERT_OK_OR_GOTO(status,
            pDlppm1xBounds->pEndpointHi != NULL ?
                FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
            s_perfCfPwrModelDlppm1xGuardRailPrimaryClk_exit);

        for (dramclkIdx = 0U;
             dramclkIdx < pDlppm1xObserved->numInitialDramclkEstimates;
             dramclkIdx++)
        {
            if (pDlppm1xObserved->initialDramclkEstimates[dramclkIdx].estimatedMetrics[0].fbRail.freqkHz ==
                    ppDlppm1xEstimated[0]->fbRail.freqkHz)
            {
                pCoreRailFit =
                    &pDlppm1xObserved->initialDramclkEstimates[dramclkIdx].coreRailPwrFit;
                break;
            }
        }

        PMU_ASSERT_OK_OR_GOTO(status,
            dramclkIdx < pDlppm1xObserved->numInitialDramclkEstimates ?
                FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
            s_perfCfPwrModelDlppm1xGuardRailPrimaryClk_exit);
    }
    else
    {
        pCoreRailFit = NULL;
    }

    //
    // We want to guard rail starting from the smallest frequency delta to the
    // observed point (or at the observed point if possible) and go "outwards"
    // from there.
    //
    // We have three cases:
    //  1.) We have a left endpoint and the observed frequency is less than
    //      that. In this case, we need to guard rail the first estimated metric
    //      against the left endpoint and then guard rail upwards from there.
    //  2.) We have a right endpoint and the observed frequency is more than
    //      that. In this case, we need to guard rail the last estimated metric
    //      against the right endpoint and then guard rail downwards from there.
    //  3.) The observed frequency is between the two endpoints. This means that
    //      the observed frequency is closest to one of the estimated metrics,
    //      so we search for that metric.
    //
    if ((pDlppm1xBounds->pEndpointLo != NULL) &&
        (pDlppm1xObserved->coreRail.rails[0].super.freqkHz <
            pDlppm1xBounds->pEndpointLo->coreRail.rails[0].freqkHz))
    {
        PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS_FROM_ESTIMATED(
            endpointLo,
            pDlppm1xBounds->pEndpointLo);

        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfPwrModelDlppm1xGuardRailMetricsStructurePrimaryClk(
                pDlppm1x,
                ppDlppm1xEstimated[0],
                &endpointLo,
                pDlppm1xBounds,
                pCoreRailFit,
                pDlppm1xObserved),
            s_perfCfPwrModelDlppm1xGuardRailPrimaryClk_exit);

        upStartIdx = 1U;
        downStartIdx = LW_U8_MAX;
    }
    else if ((pDlppm1xBounds->pEndpointHi != NULL) &&
             (pDlppm1xObserved->coreRail.rails[0].super.freqkHz >
                pDlppm1xBounds->pEndpointHi->coreRail.rails[0].freqkHz))
    {
        PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS_FROM_ESTIMATED(
            endpointHi,
            pDlppm1xBounds->pEndpointHi);

        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfPwrModelDlppm1xGuardRailMetricsStructurePrimaryClk(
                pDlppm1x,
                ppDlppm1xEstimated[numEstimatedMetrics - 1U],
                &endpointHi,
                pDlppm1xBounds,
                pCoreRailFit,
                pDlppm1xObserved),
            s_perfCfPwrModelDlppm1xGuardRailPrimaryClk_exit);

        upStartIdx = numEstimatedMetrics;
        downStartIdx = numEstimatedMetrics - 2U;
    }
    else
    {
        //
        // We know that the observed metrics is within the endpoints, so we now
        // find the estimated metrics adjacent to the observed metrics. There
        // are then two possible cases:
        //  1.) There is a DRAMCLK match with the observed point. In
        //      this case, we guard rail the point(s) adjacent to the
        //      observed metrics against the observed metrics, and then we set
        //      the start indices to just beyond that/those point(s).
        //  2.) There is not a DRAMCLK match with the observed point. We
        //      then find which of the adjacent points is closer to the
        //      observed and guard rail that point against any bounds and core
        //      rail power fit we might have. Then, we start guard
        //      guard railing from both sides of the closer point.
        //
        PERF_CF_PWR_MODEL_DLPPM_1X_OBSERVED_ADJACENT observedAdjacent;

        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfPwrModelDlppm1xObservedAdjacentGet(
                pDlppm1xObserved,
                &observedAdjacent,
                ppDlppm1xEstimated,
                numEstimatedMetrics),
            s_perfCfPwrModelDlppm1xGuardRailPrimaryClk_exit);

        if (s_perfCfPwrModelDlppm1xDramclkApproxEqual(
                pDlppm1x,
                pDlppm1xObserved->fbRail.super.freqkHz,
                ppDlppm1xEstimated[0]->fbRail.freqkHz))
        {
            PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS_FROM_OBSERVED(
                observedMetrics,
                pDlppm1xObserved);

            for (i = 0U; i < observedAdjacent.numObservedAdjacent; i++)
            {
                LwU8 metricsIdx = observedAdjacent.observedAdjacent[i];

                PMU_ASSERT_OK_OR_GOTO(status,
                    s_perfCfPwrModelDlppm1xGuardRailMetricsStructurePrimaryClk(
                        pDlppm1x,
                        ppDlppm1xEstimated[metricsIdx],
                        &observedMetrics,
                        pDlppm1xBounds,
                        pCoreRailFit,
                        pDlppm1xObserved),
                    s_perfCfPwrModelDlppm1xGuardRailPrimaryClk_exit);
            }

            //
            // Start above the highest adjacent metrics to go up and start
            // below the lowest adjacent metrics to go down.
            //
            upStartIdx =
                observedAdjacent.observedAdjacent[observedAdjacent.numObservedAdjacent - 1U] + 1U;
            downStartIdx =
                observedAdjacent.observedAdjacent[0] - 1U;
        }
        else
        {
            LwU8 closestIdx;

            PMU_ASSERT_OK_OR_GOTO(status,
                s_perfCfPwrModelDlppm1xObservedAdjacentClosestGet(
                    pDlppm1xObserved,
                    &observedAdjacent,
                    ppDlppm1xEstimated,
                    &closestIdx),
            s_perfCfPwrModelDlppm1xGuardRailPrimaryClk_exit);

            PMU_ASSERT_OK_OR_GOTO(status,
                s_perfCfPwrModelDlppm1xGuardRailMetricsStructurePrimaryClk(
                    pDlppm1x,
                    ppDlppm1xEstimated[closestIdx],
                    NULL,
                    pDlppm1xBounds,
                    pCoreRailFit,
                    pDlppm1xObserved),
                s_perfCfPwrModelDlppm1xGuardRailPrimaryClk_exit);

            upStartIdx = closestIdx + 1U;
            downStartIdx = closestIdx - 1U;
        }
    }

    //
    // We use Lw_U8_MAX to indicate the end of iteration when going from
    // right-to-left, so ensure that numEstimatedMetrics can never validly be
    // that.
    //
    ct_assert(LW2080_CTRL_NNE_NNE_DESC_INFERENCE_LOOPS_MAX < LW_U8_MAX);

    // Go downwards from the point we determined is closest to observed.
    for (i = downStartIdx; i != LW_U8_MAX; i--)
    {
        PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS_FROM_ESTIMATED(
            next,
            ppDlppm1xEstimated[i + 1U]);

        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfPwrModelDlppm1xGuardRailMetricsStructurePrimaryClk(
                pDlppm1x,
                ppDlppm1xEstimated[i],
                &next,
                pDlppm1xBounds,
                pCoreRailFit,
                pDlppm1xObserved),
            s_perfCfPwrModelDlppm1xGuardRailPrimaryClk_exit);
    }

    // Go upwards from the point we determined is closest to observed.
    for (i = upStartIdx; i < numEstimatedMetrics; i++)
    {
        PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS_FROM_ESTIMATED(
            prev,
            ppDlppm1xEstimated[i - 1U]);

        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfPwrModelDlppm1xGuardRailMetricsStructurePrimaryClk(
                pDlppm1x,
                ppDlppm1xEstimated[i],
                &prev,
                pDlppm1xBounds,
                pCoreRailFit,
                pDlppm1xObserved),
            s_perfCfPwrModelDlppm1xGuardRailPrimaryClk_exit);
    }

s_perfCfPwrModelDlppm1xGuardRailPrimaryClk_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xObservedAdjacentGet
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xObservedAdjacentGet
(
    const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
    PERF_CF_PWR_MODEL_DLPPM_1X_OBSERVED_ADJACENT *pObservedAdjacent,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X **ppDlppm1xEstimated,
    LwU8 numEstimatedMetrics
)
{
    LwU8 i;

    pObservedAdjacent->numObservedAdjacent = 0U;

    // Find the point(s) closest to the observed metrics
    for (i = 0U; i < numEstimatedMetrics; i++)
    {
        //
        // Check for the first metrics structure with a higher primary clock
        // than the observed primary clock.
        //
        if (ppDlppm1xEstimated[i]->coreRail.rails[0].freqkHz >=
                pDlppm1xObserved->coreRail.rails[0].super.freqkHz)
        {
            //
            // If the observed point is below the first point, then there's no
            // adjacent point below observed.
            //
            if (i != 0U)
            {
                pObservedAdjacent->observedAdjacent[
                    pObservedAdjacent->numObservedAdjacent++] = i - 1U;
            }

            pObservedAdjacent->observedAdjacent[
                pObservedAdjacent->numObservedAdjacent++] = i;

            break;
        }
    }

    //
    // If we didn't find any metrics structure with primary clock greater than
    // the observed, then the observed frequency is above the entire range, so
    // mark only the highest point as adjacent to observed.
    //
    if (i == numEstimatedMetrics)
    {
        pObservedAdjacent->observedAdjacent[
            pObservedAdjacent->numObservedAdjacent++] = i - 1U;
    }

    return FLCN_OK;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xObservedAdjacentClosestGet
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xObservedAdjacentClosestGet
(
    const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved,
    const PERF_CF_PWR_MODEL_DLPPM_1X_OBSERVED_ADJACENT *pObservedAdjacent,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X **ppDlppm1xEstimated,
    LwU8 *pClosestIdx
)
{
    if (pObservedAdjacent->numObservedAdjacent == 1U)
    {
        *pClosestIdx = pObservedAdjacent->observedAdjacent[0];
    }
    else
    {
        const LwU8 belowIdx = pObservedAdjacent->observedAdjacent[0];
        const LwU8 aboveIdx = pObservedAdjacent->observedAdjacent[1];
        *pClosestIdx =
            (pDlppm1xObserved->coreRail.rails[0].super.freqkHz -
                    ppDlppm1xEstimated[belowIdx]->coreRail.rails[0].freqkHz) <
            (ppDlppm1xEstimated[aboveIdx]->coreRail.rails[0].freqkHz -
                    pDlppm1xObserved->coreRail.rails[0].super.freqkHz) ?
                belowIdx :
                aboveIdx;
    }

    return FLCN_OK;
}


/*!
 * @copydoc s_perfCfPwrModelDlppm1xGuardRailMetricsStructurePrimaryClk
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xGuardRailMetricsStructurePrimaryClk
(
    PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *pLwrr,
    const PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS *pAdjacent,
    const PERF_CF_PWR_MODEL_SCALE_BOUNDS_DLPPM_1X *pDlppm1xBounds,
    const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_CORE_RAIL_POWER_FIT *pCoreRailFit,
    const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved
)
{
    FLCN_STATUS status;
    const LwBool bLeftToRight =
        (pAdjacent != NULL) && (pLwrr->coreRail.rails[0].freqkHz >= pAdjacent->pCoreRailMetrics->freqkHz);
    const LwBool bRightToLeft =
        (pAdjacent != NULL) && (pLwrr->coreRail.rails[0].freqkHz <= pAdjacent->pCoreRailMetrics->freqkHz);
    PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS_ORIGIN(originMetrics);
    PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS_FROM_ESTIMATED(
        endpointLo,
        pDlppm1xBounds->pEndpointLo);
    PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS_FROM_ESTIMATED(
        endpointHi,
        pDlppm1xBounds->pEndpointHi);

    LwU64_ALIGN32_ADD(
        &pDlppm1x->guardRailsStatus.isoDramclkGuardRailCount,
        &pDlppm1x->guardRailsStatus.isoDramclkGuardRailCount,
        &((LwU64_ALIGN32){ .hi = 0U, .lo = 1U, }));

    //
    // Ensure monotonicity of perfRatio - as frequency increases/decreases,
    // perfRatio must increase/decrease, respectively. Check this against the
    // immediately adjacent point and each endpoint.
    //
    if ((bLeftToRight &&
         (pLwrr->perfMetrics.perfRatio < pAdjacent->pPerfMetrics->perfRatio)) ||
        (bRightToLeft &&
         (pLwrr->perfMetrics.perfRatio > pAdjacent->pPerfMetrics->perfRatio)))
    {
        pLwrr->perfMetrics.perfRatio = pAdjacent->pPerfMetrics->perfRatio;
        s_perfCfPwrModelDlppm1xGuardRailFlagsSet(
            pDlppm1x,
            pLwrr,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_METRIC_PERF,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_GUARD_RAIL_ADJACENT_PRIMARY_CLK);
    }
    else if ((pDlppm1xBounds->pEndpointLo != NULL) &&
             (pLwrr->perfMetrics.perfRatio < pDlppm1xBounds->pEndpointLo->perfMetrics.perfRatio))
    {
        pLwrr->perfMetrics.perfRatio = pDlppm1xBounds->pEndpointLo->perfMetrics.perfRatio;
        s_perfCfPwrModelDlppm1xGuardRailFlagsSet(
            pDlppm1x,
            pLwrr,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_METRIC_PERF,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_GUARD_RAIL_ENDPOINT_PRIMARY_CLK);
   }
    else if ((pDlppm1xBounds->pEndpointHi != NULL) &&
             (pLwrr->perfMetrics.perfRatio > pDlppm1xBounds->pEndpointHi->perfMetrics.perfRatio))
    {
        pLwrr->perfMetrics.perfRatio = pDlppm1xBounds->pEndpointHi->perfMetrics.perfRatio;
        s_perfCfPwrModelDlppm1xGuardRailFlagsSet(
            pDlppm1x,
            pLwrr,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_METRIC_PERF,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_GUARD_RAIL_ENDPOINT_PRIMARY_CLK);
    }
    else
    {
        // Did not violate monotonicity
    }

    // Check linearity of perfRatio with respect to primary clock frequency.
    if (bLeftToRight)
    {
        //
        // If going left-to-right, ensure that we don't go:
        //  1.) Below the line between the adjacent point and the high
        //      endpoint, if available.
        //  2.) Above the line between the adjacent point and the origin
        //
        if (PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS_IS_VALID(&endpointHi))
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                s_perfCfPwrModelDlppm1xGuardRailMetricsStructurePrimaryClkPerfRatioLinearity(
                    pDlppm1x,
                    pLwrr,
                    pAdjacent,
                    &endpointHi,
                    LW_TRUE),
                s_perfCfPwrModelDlppm1xGuardRailMetricsStructurePrimaryClk_exit);
        }

        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfPwrModelDlppm1xGuardRailMetricsStructurePrimaryClkPerfRatioLinearity(
                pDlppm1x,
                pLwrr,
                &originMetrics,
                pAdjacent,
                LW_FALSE),
            s_perfCfPwrModelDlppm1xGuardRailMetricsStructurePrimaryClk_exit);
    }
    else if (bRightToLeft)
    {
        //
        // If going right-to-left, ensure that we don't go:
        //  1.) Below the line between the low endpoint, if available, and the
        //      adjacent point. Otherwise, ensure that we don't go below the
        //      line between the origin and the adjacent point.
        //  2.) Above the line between the origin and the low endpoint,
        //      if available.
        //
        if (PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS_IS_VALID(&endpointLo))
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                s_perfCfPwrModelDlppm1xGuardRailMetricsStructurePrimaryClkPerfRatioLinearity(
                    pDlppm1x,
                    pLwrr,
                    &endpointLo,
                    pAdjacent,
                    LW_TRUE),
                s_perfCfPwrModelDlppm1xGuardRailMetricsStructurePrimaryClk_exit);

            PMU_ASSERT_OK_OR_GOTO(status,
                s_perfCfPwrModelDlppm1xGuardRailMetricsStructurePrimaryClkPerfRatioLinearity(
                    pDlppm1x,
                    pLwrr,
                    &originMetrics,
                    &endpointLo,
                    LW_FALSE),
                s_perfCfPwrModelDlppm1xGuardRailMetricsStructurePrimaryClk_exit);
        }
        else
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                s_perfCfPwrModelDlppm1xGuardRailMetricsStructurePrimaryClkPerfRatioLinearity(
                    pDlppm1x,
                    pLwrr,
                    &originMetrics,
                    pAdjacent,
                    LW_TRUE),
                s_perfCfPwrModelDlppm1xGuardRailMetricsStructurePrimaryClk_exit);
        }
    }
    else if (PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS_IS_VALID(&endpointLo) &&
             PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS_IS_VALID(&endpointHi))
    {
        //
        // If we're not going left-to-right or right-to-left, then if we have
        // endpoints, we want to make sure that we don't go:
        //  1.) Below the line between the low endpoint and high endpoint
        //  2.) Above the line between the low endpoint and the origin
        //
        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfPwrModelDlppm1xGuardRailMetricsStructurePrimaryClkPerfRatioLinearity(
                pDlppm1x,
                pLwrr,
                &endpointLo,
                &endpointHi,
                LW_TRUE),
            s_perfCfPwrModelDlppm1xGuardRailMetricsStructurePrimaryClk_exit);

        PMU_ASSERT_OK_OR_GOTO(status,
             s_perfCfPwrModelDlppm1xGuardRailMetricsStructurePrimaryClkPerfRatioLinearity(
                pDlppm1x,
                pLwrr,
                &originMetrics,
                &endpointLo,
                LW_FALSE),
            s_perfCfPwrModelDlppm1xGuardRailMetricsStructurePrimaryClk_exit);
    }
    else
    {
        // No points against which to check linearity
        status = FLCN_OK;
    }

    if ((bLeftToRight &&
         (pLwrr->coreRail.rails[0].pwrOutDynamicNormalizedmW < pAdjacent->pCoreRailMetrics->pwrOutDynamicNormalizedmW)) ||
        (bRightToLeft &&
         (pLwrr->coreRail.rails[0].pwrOutDynamicNormalizedmW > pAdjacent->pCoreRailMetrics->pwrOutDynamicNormalizedmW)))
    {
        pLwrr->coreRail.rails[0].pwrOutDynamicNormalizedmW = pAdjacent->pCoreRailMetrics->pwrOutDynamicNormalizedmW;
        s_perfCfPwrModelDlppm1xGuardRailFlagsSet(
            pDlppm1x,
            pLwrr,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_METRIC_CORE_RAIL_PWR,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_GUARD_RAIL_ADJACENT_PRIMARY_CLK);
    }
    else if ((pDlppm1xBounds->pEndpointLo != NULL) &&
             (pLwrr->coreRail.rails[0].pwrOutDynamicNormalizedmW < pDlppm1xBounds->pEndpointLo->coreRail.rails[0].pwrOutDynamicNormalizedmW))
    {
        pLwrr->coreRail.rails[0].pwrOutDynamicNormalizedmW = pDlppm1xBounds->pEndpointLo->coreRail.rails[0].pwrOutDynamicNormalizedmW;
        s_perfCfPwrModelDlppm1xGuardRailFlagsSet(
            pDlppm1x,
            pLwrr,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_METRIC_CORE_RAIL_PWR,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_GUARD_RAIL_ENDPOINT_PRIMARY_CLK);
    }
    else if ((pDlppm1xBounds->pEndpointHi != NULL) &&
             (pLwrr->coreRail.rails[0].pwrOutDynamicNormalizedmW > pDlppm1xBounds->pEndpointHi->coreRail.rails[0].pwrOutDynamicNormalizedmW))
    {
        pLwrr->coreRail.rails[0].pwrOutDynamicNormalizedmW = pDlppm1xBounds->pEndpointHi->coreRail.rails[0].pwrOutDynamicNormalizedmW;
        s_perfCfPwrModelDlppm1xGuardRailFlagsSet(
            pDlppm1x,
            pLwrr,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_METRIC_CORE_RAIL_PWR,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_GUARD_RAIL_ENDPOINT_PRIMARY_CLK);
    }
    else
    {
        // Did not violate monotonicity
    }

    if ((bLeftToRight &&
         (pLwrr->fbRail.pwrOutTuple.pwrmW < pAdjacent->pFbRailMetrics->pwrOutTuple.pwrmW)) ||
        (bRightToLeft &&
         (pLwrr->fbRail.pwrOutTuple.pwrmW > pAdjacent->pFbRailMetrics->pwrOutTuple.pwrmW)))
    {
        pLwrr->fbRail.pwrOutTuple.pwrmW = pAdjacent->pFbRailMetrics->pwrOutTuple.pwrmW;
        s_perfCfPwrModelDlppm1xGuardRailFlagsSet(
            pDlppm1x,
            pLwrr,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_METRIC_FB_PWR,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_GUARD_RAIL_ADJACENT_PRIMARY_CLK);
    }
    else if ((pDlppm1xBounds->pEndpointLo != NULL) &&
             (pLwrr->fbRail.pwrOutTuple.pwrmW < pDlppm1xBounds->pEndpointLo->fbRail.pwrOutTuple.pwrmW))
    {
        pLwrr->fbRail.pwrOutTuple.pwrmW = pDlppm1xBounds->pEndpointLo->fbRail.pwrOutTuple.pwrmW;
        s_perfCfPwrModelDlppm1xGuardRailFlagsSet(
            pDlppm1x,
            pLwrr,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_METRIC_FB_PWR,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_GUARD_RAIL_ENDPOINT_PRIMARY_CLK);
    }
    else if ((pDlppm1xBounds->pEndpointHi != NULL) &&
             (pLwrr->fbRail.pwrOutTuple.pwrmW > pDlppm1xBounds->pEndpointHi->fbRail.pwrOutTuple.pwrmW))
    {
        pLwrr->fbRail.pwrOutTuple.pwrmW = pDlppm1xBounds->pEndpointHi->fbRail.pwrOutTuple.pwrmW;
        s_perfCfPwrModelDlppm1xGuardRailFlagsSet(
            pDlppm1x,
            pLwrr,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_METRIC_FB_PWR,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_GUARD_RAIL_ENDPOINT_PRIMARY_CLK);
    }
    else
    {
        // Did not violate monotonicity
    }

    //
    // Check the approximate linearity of core rail power vs. performance
    // (measured as perfRatio). We can only do this if perfRatio is valid, and
    // if it is, we have two options:
    //  1.) If there is a valid linear fit, then we use it to callwlate the
    //      expected power and make sure the estimated power is within a tolerance.
    //  2.) If there is not a valid fit, then we consider a few checks for
    //      linearity between various points.
    //
    if (pLwrr->perfMetrics.perfRatio !=
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_PERF_PERF_RATIO_ILWALID)
    {
        if ((pCoreRailFit != NULL) &&
            (pCoreRailFit->bValid))
        {
            s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrFit(
                pDlppm1x,
                pLwrr,
                pCoreRailFit);
        }
        else
        {
            //
            // Fall back to checking simple linearity if the fit is not
            // available or valid.
            //
            PMU_ASSERT_OK_OR_GOTO(status,
                s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrLinearityFromPoints(
                    pDlppm1x,
                    pLwrr,
                    pAdjacent,
                    &endpointLo,
                    &endpointHi,
                    bLeftToRight,
                    bRightToLeft),
                s_perfCfPwrModelDlppm1xGuardRailMetricsStructurePrimaryClk_exit);
        }
    }

s_perfCfPwrModelDlppm1xGuardRailMetricsStructurePrimaryClk_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xGuardRailMetricsStructurePrimaryClkPerfRatioLinearity
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xGuardRailMetricsStructurePrimaryClkPerfRatioLinearity
(
    PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *pLwrr,
    const PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS *pLo,
    const PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS *pHi,
    LwBool bMinimum
)
{
    //
    // Numerical Analysis:
    // We are callwlating the line between pLo and pHi and ensuring that pLwrr
    // is above or below that line. Using the point-slope form of the line,
    // with pLo as the known point, we can callwlate this as follows:
    //  extremumRatio =
    //      ((perfRatio_{hi} - perfRatio_{lo}) / (freqkHz_{hi} - freqkHz_{lo})) *
    //          (freqkHz_{lwrr} - freqkHz_{lo}) + perfRatio_{lo}
    //
    // Via prior guard-railing, we have ensured that perfRatio_{lo} and
    // perfRatio_{hi} are monotonic and not super-linear with respect to
    // both primary clock and DRAMCLK. The maximum expected primary clock range
    // is [210 MHz, 2100 MHz] and the maximum expected DRAMCLK range is
    // [405 MHz, 10000 MHz]. The maximum proportional increase across across
    // primary clock is therefore:
    //  2100 MHz / 210 MHz
    //  10
    // and the maximum proportional increase across DRAMCLK is therefore:
    //  10000 MHz / 405 MHz
    //  24.69 ~= 25
    //
    // The observed perfRatio is hard-coded to be 1. Therefore, conservatively,
    // the perfRatios can be between [1 / 250, 1 * 250], while additionally
    // allowing 0 for the origin. This corresponds to approximately 8.8
    // significant bits.
    //
    // TODO-aherring: The above isn't quite true, because we don't always
    // guard-rail the estimated metrics against the observed metrics, so there's
    // not always something ensuring the relationship between observed and a
    // set of estimated metrics. This can be removed if/when we do
    // non-matching-DRAMCLK guard-railing.
    //
    // numeratorRatio = perfRatio_{hi} - perfRatio_{lo}
    // Per above, perfRatio_{hi} and perfRatio_{lo} have 8.8 significant bits.
    // The difference therefore has at most 8 significant integer bits. The
    // representation is 20.12, however, so the difference can have as many as
    // 12 significant fractional bits. Therefore, the difference has at most
    // 8.12 significant bits. Note that by monotonicity, we know that this
    // value must be non-negative.
    //
    // denominatorkHz = freqkHz_{hi} - freqkHz_{lo}
    // Per above, the maximum expected primary clock frequency is 2100 MHz. This
    // requires 21 bits to represent in kHz, so each frequency has 21.0
    // significant bits and so does the difference.
    //
    // slopeRatioPerkHz = numeratorRatio / denominatorkHz
    // We are dividing a 52.12 with 8.12 significant bits by a 32.0 with 21.0
    // significant bits. This can only maintain or reduce the number of
    // significant integer bits necessary. However, it can increase the number
    // of significant fractional bits by up to 21, resulting in an 8.33.
    // Therefore, we need to do this intermediate division in 64-bit.
    // In particular, we will use 24.40.
    //
    // We first leftcast numeratorRatio to and left-shift it by 28 to colwert
    // it from 52.12 representation to 24.40. This is safe because it has only
    // 8 significant integer bits.
    //
    // We then divide by the 21.0 to get at most 8.33 significant bits in 24.40
    // representation.
    //
    // diffkHz = freqkHz_{lwrr} - freqkHz_{lo}
    // Per above, the maximum expected primary clock frequency is 2100 MHz. This
    // requires 21 bits to represent in kHz, so each frequency has 21.0
    // significant bits and so does the difference.
    //
    // extremumRatio = slopeRatioPerkHz * diffkHz + perfRatio_{lo}
    //
    // We are callwlating one of two things here:
    //  1.) A value on the line between (0 kHz, 0) and
    //      (freqkHz_{hi}, perfRatio_{hi}) at freqkHz_{lwrr}, where
    //      freqkHz_{lwrr} > freqkHz_{hi}
    //  OR
    //  2.) A value on the line between (freqkHz_{lo}, perfRatio_{lo}) and
    //      (freqkHz_{hi}, perfRatio_{hi}) at freqkHz_{lwrr}, where
    //      freqkHz_{lo} < freqkHz_{lwrr} < freqkHz_{hi}
    //
    // In case 1.), we know that perfRatio_{lo} is 0, and we know that
    // extremumRatio must be between [1 / 250, 1 * 250] to not violate linearity
    // with respect to the observed metrics, so slopeRatioPerkHz * diffkHz must
    // produce at most 8.8 significant bits, which is safe to place back in a
    // 20.12.
    //
    // In case 2.), we are callwlating a point on the line between
    // freqkHz_{lo} and freqkHz_{hi}, so we know that:
    //  perfRatio_{lo} <= extremumRatio <= perfRatio_{hi}
    //  perfRatio_{lo} <= extremumRatio <= perfRatio_{hi}
    //  0 <= extremumRatio  - perfRatio_{lo} <= perfRatio_{hi} - perfRatio_{lo}
    // Rearranging the above callwlation, we get:
    //  extremumRatio - perfRatio_{lo} = slopeRatioPerkHz * diffkHz
    // Substituting this into the inequaility, we get:
    //  0 <= slopeRatioPerkHz * diffkHz <= perfRatio_{hi} - perfRatio_{lo}
    // Per above, we know that perfRatio_{hi} - perfRatio_{lo} has at most 8.12
    // significant bits, so so does slopeRatioPerkHz * diffkHz, so this
    // callwlation is safe in 24.40.
    //
    // Finally, then, as bove, we know that extremumRatio is between
    // perfRatio_{lo} and perfRatio_{hi}; each of those requires 8.8 significant
    // bits, so extremumRatio will as well, and it is safe to place it back in a
    // 20.12.
    //
    // In both cases, we shift off the bottom 28 bits of precision to return
    // from 24.40 representation to 20.12.
    //
    FLCN_STATUS status;
    LwUFXP52_12 numeratorRatio;
    LwU32 denominatorkHz;
    LwUFXP24_40 slopeRatioPerkHz;
    LwU32 diffkHz;
    LwUFXP52_12 extremumRatio52_12;
    LwUFXP20_12 extremumRatio;

    PMU_ASSERT_OK_OR_GOTO(status,
        (pLo->pCoreRailMetrics->freqkHz < pHi->pCoreRailMetrics->freqkHz) &&
        (pLo->pPerfMetrics->perfRatio <= pHi->pPerfMetrics->perfRatio) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
        s_perfCfPwrModelDlppm1xGuardRailMetricsStructurePrimaryClkPerfRatioLinearity_exit);

    numeratorRatio =
        (LwUFXP52_12)(pHi->pPerfMetrics->perfRatio -
            pLo->pPerfMetrics->perfRatio);
    denominatorkHz =
        pHi->pCoreRailMetrics->freqkHz - pLo->pCoreRailMetrics->freqkHz;

    lw64Lsl(&numeratorRatio, &numeratorRatio, 28);
    lwU64Div(
        &slopeRatioPerkHz,
        &numeratorRatio,
        &(LwU64){ denominatorkHz });

    diffkHz =
        pLwrr->coreRail.rails[0].freqkHz - pLo->pCoreRailMetrics->freqkHz;

    //
    // Callwlate the extremumRatio and check for overflow. If we do overflow,
    // then default to the max value.
    //
    lw64Add(
        &extremumRatio52_12,
        &(LwUFXP52_12){ multUFXP64(28, slopeRatioPerkHz, diffkHz) },
        &(LwUFXP52_12){ pLo->pPerfMetrics->perfRatio });
    if (lwU64CmpLE(&extremumRatio52_12, &(LwU64) { LW_U32_MAX } ))
    {
        extremumRatio = (LwUFXP20_12)extremumRatio52_12;
    }
    else
    {
        extremumRatio = (LwUFXP20_12)LW_U32_MAX;
    }

    if (bMinimum)
    {
        if (pLwrr->perfMetrics.perfRatio < extremumRatio)
        {
            pLwrr->perfMetrics.perfRatio = extremumRatio;
            s_perfCfPwrModelDlppm1xGuardRailFlagsSet(
                pDlppm1x,
                pLwrr,
                LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_METRIC_PERF,
                LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_GUARD_RAIL_LINEAR_MINIMUM_PRIMARY_CLK);
        }
    }
    else if (pLwrr->perfMetrics.perfRatio > extremumRatio)
    {
        pLwrr->perfMetrics.perfRatio = extremumRatio;
        s_perfCfPwrModelDlppm1xGuardRailFlagsSet(
            pDlppm1x,
            pLwrr,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_METRIC_PERF,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_GUARD_RAIL_LINEAR_MAXIMUM_PRIMARY_CLK);
    }
    else
    {
        // Did not violate linearity.
    }

s_perfCfPwrModelDlppm1xGuardRailMetricsStructurePrimaryClkPerfRatioLinearity_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrLinearityFromPoints
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrLinearityFromPoints
(
    PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *pLwrr,
    const PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS *pAdjacent,
    const PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS *pEndpointLo,
    const PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS *pEndpointHi,
    LwBool bLeftToRight,
    LwBool bRightToLeft
)
{
    FLCN_STATUS status;
    PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS_ORIGIN(originMetrics);

    if (bLeftToRight)
    {
        //
        // If going left-to-right, ensure that we don't go:
        //  1.) Below the line between the adjacent point and the high
        //      endpoint, if available.
        //  2.) Above the line between the adjacent point and the origin
        //
        if (PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS_IS_VALID(pEndpointHi))
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                    s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrLinearity(
                    pDlppm1x,
                    pLwrr,
                    pAdjacent,
                    pEndpointHi,
                    LW_TRUE),
                s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrLinearityFromPoints_exit);
        }

        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrLinearity(
                pDlppm1x,
                pLwrr,
                &originMetrics,
                pAdjacent,
                LW_FALSE),
            s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrLinearityFromPoints_exit);
    }
    else if (bRightToLeft)
    {
        //
        // If going right-to-left, ensure that we don't go:
        //  1.) Below the line between the low endpoint, if available, and the
        //      adjacent point. Otherwise, ensure that we don't go below the
        //      line between the origin and the adjacent point.
        //  2.) Above the line between the origin and the low endpoint,
        //      if available.
        //
        if (PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS_IS_VALID(pEndpointLo))
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrLinearity(
                    pDlppm1x,
                    pLwrr,
                    pEndpointLo,
                    pAdjacent,
                    LW_TRUE),
                s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrLinearityFromPoints_exit);

            PMU_ASSERT_OK_OR_GOTO(status,
                s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrLinearity(
                    pDlppm1x,
                    pLwrr,
                    &originMetrics,
                    pEndpointLo,
                    LW_FALSE),
                s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrLinearityFromPoints_exit);
        }
        else
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrLinearity(
                    pDlppm1x,
                    pLwrr,
                    &originMetrics,
                    pAdjacent,
                    LW_TRUE),
                s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrLinearityFromPoints_exit);
        }
    }
    else if (PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS_IS_VALID(pEndpointLo) &&
             PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS_IS_VALID(pEndpointHi))
    {
        //
        // If we're not going left-to-right or right-to-left, then if we have
        // endpoints, we want to make sure that we don't go:
        //  1.) Below the line between the low endpoint and high endpoint
        //  2.) Above the line between the low endpoint and the origin
        //
        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrLinearity(
                pDlppm1x,
                pLwrr,
                pEndpointLo,
                pEndpointHi,
                LW_TRUE),
            s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrLinearityFromPoints_exit);

        PMU_ASSERT_OK_OR_GOTO(status,
             s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrLinearity(
                pDlppm1x,
                pLwrr,
                &originMetrics,
                pEndpointLo,
                LW_FALSE),
            s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrLinearityFromPoints_exit);
    }
    else
    {
        // No points against which to check linearity
        status = FLCN_OK;
    }

s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrLinearityFromPoints_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrFit
 */
static void
s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrFit
(
    PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *pDlppm1xEstimated,
    const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_CORE_RAIL_POWER_FIT *pCoreRailFit
)
{
    //
    // Numerical Analysis:
    //
    // We first need to callwlate the expected output power using the
    // fit:
    //  expectCoreRailPwrmW = slopemWPerRatio * perfRatio + interceptmW
    //
    // When the fit was callwlated, it was ensured that the output of
    // this fit would never overflow 32 bits, so this is safe to
    // callwlate.
    //
    // We have to callwlate the maximum and minimum allowed
    // wattages based on the tolerance:
    //  toleranceMaxmW = (expectCoreRailPwrmW * (1 + coreRailPwrFitTolerance))
    //  toleranceMinmW = (expectCoreRailPwrmW * (1 - coreRailPwrFitTolerance))
    //
    // The coreRailPwrFitTolerance is a percentage less than 1, so it
    // is a 0.12. Therefore, multiplying 1 +/- it against
    // expectCoreRailPwrmW produces 32.12. However, we shift off the
    // fractional bits (which represent only fractions of a milliwatt)
    // and retain only the integer portion. This remains a 32.0, which is safe
    // to store back in a 32 bit unsigned integer.
    //
    LwU32 expectCoreRailPwrmW;
    LwU32 toleranceMaxmW;

    //
    // If the perfRatio is invalid, then it doesn't make sense to try to apply
    // the fit, so return early.
    //
    if (pDlppm1xEstimated->perfMetrics.perfRatio ==
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_PERF_PERF_RATIO_ILWALID)
    {
        goto s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrFit_exit;
    }

    expectCoreRailPwrmW =
        multUFXP32(24, pCoreRailFit->slopemWPerRatio, pDlppm1xEstimated->perfMetrics.perfRatio) +
            pCoreRailFit->interceptmW;
    toleranceMaxmW =
        multUFXP32(12, expectCoreRailPwrmW, LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 1U) + pDlppm1x->coreRailPwrFitTolerance);

    if (pDlppm1xEstimated->coreRail.rails[0].pwrOutDynamicNormalizedmW >
            toleranceMaxmW)
    {
        pDlppm1xEstimated->coreRail.rails[0].pwrOutDynamicNormalizedmW =
            toleranceMaxmW;
        s_perfCfPwrModelDlppm1xGuardRailFlagsSet(
            pDlppm1x,
            pDlppm1xEstimated,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_METRIC_CORE_RAIL_PWR,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_GUARD_RAIL_LINEAR_FIT);
    }
    else
    {
        LwU32 toleranceMinmW = multUFXP32(12,
            expectCoreRailPwrmW,
            (LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 1U) - pDlppm1x->coreRailPwrFitTolerance));
        if (pDlppm1xEstimated->coreRail.rails[0].pwrOutDynamicNormalizedmW <
                toleranceMinmW)
        {
            pDlppm1xEstimated->coreRail.rails[0].pwrOutDynamicNormalizedmW =
                toleranceMinmW;
            s_perfCfPwrModelDlppm1xGuardRailFlagsSet(
                pDlppm1x,
                pDlppm1xEstimated,
                LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_METRIC_CORE_RAIL_PWR,
                LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_GUARD_RAIL_LINEAR_FIT);
        }
    }

s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrFit_exit:
    lwosNOP();
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrLinearity
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrLinearity
(
    PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *pLwrr,
    const PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS *pLo,
    const PERF_CF_PWR_MODEL_DLPPM_1X_GUARD_RAIL_PRIMARY_CLK_METRICS *pHi,
    LwBool bMinimum
)
{
    //
    // Numerical Analysis:
    // We are callwlating the line between pLo and pHi and ensuring that pLwrr
    // is above or below that line. Using the point-slope form of the line,
    // with pLo as the known point, we can callwlate this as follows:
    //  extremummW =
    //      ((pwrOutDynamicNormalizedmW_{hi} - pwrOutDynamicNormalizedmW_{lo}) / (perfRatio_{hi} - perfRatio_{lo})) *
    //          (perfRatio_{lwrr} - perfRatio_{lo}) + pwrOutDynamicNormalizedmW_{lo}
    //
    // Via prior guard-railing, we have ensured that perfRatio_{lo} and
    // perfRatio_{hi} are monotonic and not super-linear with respect to
    // both primary clock and DRAMCLK. The maximum expected primary clock range
    // is [210 MHz, 2100 MHz] and the maximum expected DRAMCLK range is
    // [405 MHz, 10000 MHz]. The maximum proportional increase across across
    // primary clock is therefore:
    //  2100 MHz / 210 MHz
    //  10
    // and the maximum proportional increase across DRAMCLK is therefore:
    //  10000 MHz / 405 MHz
    //  24.69 ~= 25
    //
    // The observed perfRatio is hard-coded to be 1. Therefore, conservatively,
    // the perfRatios can be between [1 / 250, 1 * 250], while additionally
    // allowing 0 for the origin. This corresponds to approximately 8.8
    // significant bits.
    //
    // TODO-aherring: The above isn't quite true, because we don't always
    // guard-rail the estimated metrics against the observed metrics, so there's
    // not always something ensuring the relationship between observed and a
    // set of estimated metrics. This can be removed if/when we do
    // non-matching-DRAMCLK guard-railing.
    //
    // numeratormW = pwrOutDynamicNormalizedmW_{hi} - pwrOutDynamicNormalizedmW_{lo}
    // We never expect to see a power value that is above 1 kW, which requires
    // 20 bits to represent in mW. Note that by monotonicity, we know this value
    // must be non-negative.
    //
    // denominatorRatio = perfRatio_{hi} - perfRatio_{lo}
    // Per above, perfRatio_{hi} and perfRatio_{lo} have 8.8 significant bits.
    // The difference therefore has at most 8 significant integer bits. The
    // representation is 20.12, however, so the difference can have as many as
    // 12 significant fractional bits. Therefore, the difference has at most
    // 8.12 significant bits. Note that by monotonicity, we know that this
    // value must be non-negative. Also note that the perfRatios can be equal.
    // If that happens, we cannot callwlate a slope, so we exit early without
    // applying the guard rail.
    //
    // slopemWPerRatio = numeratormW / denominatorRatio
    // We are dividing a 64.0 with 20.0 significant bits by a 20.12 with 8.12
    // significant bits. This can result in up to 32.8 significant bits.
    // therefore, we need to do this intermediate division in 64-bit. In
    // particular, we will use 40.24.
    //
    // We first cast numeratormW to 64-bit and left-shift it by 12 to colwert
    // it from 20.12 representation to 40.24. This is safe because it has only
    // 20 significant integer bits.
    //
    // We then divide by the 8.12 to get at most 32.8 significant bits in 52.12
    // representation.
    //
    // diffRatio = perfRatio_{lwrr} - perfRatio_{lo}
    // Per above, perfRatio_{lwrr} and perfRatio_{lo} have 8.8 significant bits.
    // The difference therefore has at most 8 significant integer bits. The
    // representation is 20.12, however, so the difference can have as many as
    // 12 significant fractional bits. Therefore, the difference has at most
    // 8.12 significant bits. Note that by monotonicity, we know that this
    // value must be non-negative.
    //
    // extremummW = slopemWPerRatio * diffRatio + pwrOutDynamicNormalizedmW_{lo}
    //
    // We are callwlating one of two things here:
    //  1.) A value on the line between (0, 0 mW) and
    //      (perfRatio_{hi}, pwrOutDynamicNormalizedmW_{hi}) at perfRatio_{lwrr}, where
    //      perfRatio_{lwrr} >= perfRatio_{hi}
    //  OR
    //  2.) A value on the line between (perfRatio_{lo}, pwrOutDynamicNormalizedmW_{lo}) and
    //      (perfRatio_{hi}, pwrOutDynamicNormalizedmW_{hi}) at perfRatio_{lwrr}, where
    //      perfRatio_{lo} <= perfRatio_{lwrr} <= perfRatio_{hi}
    //
    // In case 1.), we do not have a good bound on extremummW. We have assumed
    // the value of pwrOutDynamicNormalizedmW_{hi} can be up to 1 kW, and
    // perfRatio_{hi} can be up to 1 * 250, so this could theoretically be up to
    // 250 kW, which requires 28 bits to represent in mW. This safely fits, but
    // having a power above 1 kW could be an issue for numerical callwlations
    // downstream of this, so we make sure to bound this value to 1 kW.
    //
    // In case 2.), we are callwlating a point on the line between
    // perfRatio_{lo} and perfRatio_{hi}, so we know that:
    //  pwrOutDynamicNormalizedmW_{lo} <= extremummW <= pwrOutDynamicNormalizedmW_{hi}
    //  pwrOutDynamicNormalizedmW_{lo} <= extremummW <= pwrOutDynamicNormalizedmW_{hi}
    //  0 <= extremummW  - pwrOutDynamicNormalizedmW_{lo} <= pwrOutDynamicNormalizedmW_{hi} - pwrOutDynamicNormalizedmW_{lo}
    // Rearranging the above callwlation, we get:
    //  extremummW - pwrOutDynamicNormalizedmW_{lo} = slopemWPerRatio * diffRatio
    // Substituting this into the inequaility, we get:
    //  0 <= slopemWPerRatio * diffRatio <= pwrOutDynamicNormalizedmW_{hi} - pwrOutDynamicNormalizedmW_{lo}
    // Per above, we know that pwrOutDynamicNormalizedmW_{hi} - pwrOutDynamicNormalizedmW_{lo}
    // has at most 20.0 significant bits, so so does slopemWPerRatio * diffRatio
    // so this callwlation is safe in 52.12.
    //
    // Finally, then, as bove, we know that extremummW is between
    // pwrOutDynamicNormalizedmW_{lo} and pwrOutDynamicNormalizedmW_{hi}; each
    // of those requires 20.0 significant bits, so extremummW will as well, and
    // it is safe to place it back in a 32.0.
    //
    // In both cases, after the multiplication, we shift off the bottom 24 bits
    // of precision to return from 104.24 representation to 32.0.
    //
    FLCN_STATUS status;
    LwU64 numeratormW;
    LwUFXP20_12 denominatorRatio;
    LwUFXP52_12 slopemWPerRatio;
    LwUFXP20_12 diffRatio;
    LwU64 extremummW64;
    LwU32 extremummW;

    PMU_ASSERT_OK_OR_GOTO(status,
        (pLo->pPerfMetrics->perfRatio <=
                pHi->pPerfMetrics->perfRatio) &&
        (pLo->pCoreRailMetrics->pwrOutDynamicNormalizedmW <=
                pHi->pCoreRailMetrics->pwrOutDynamicNormalizedmW) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
        s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrLinearity_exit);

    //
    // If we have two points with the same perfRatio, we can't callwlate a slope
    // using them. Instead, revert to a straight monotonicity check.
    //
    if (pLo->pPerfMetrics->perfRatio == pHi->pPerfMetrics->perfRatio)
    {
        if ((pLwrr->perfMetrics.perfRatio == pHi->pPerfMetrics->perfRatio) ||
            ((pLwrr->perfMetrics.perfRatio < pLo->pPerfMetrics->perfRatio) &&
             (pLwrr->coreRail.rails[0].pwrOutDynamicNormalizedmW > pLo->pCoreRailMetrics->pwrOutDynamicNormalizedmW)) ||
            ((pLwrr->perfMetrics.perfRatio > pHi->pPerfMetrics->perfRatio) &&
             (pLwrr->coreRail.rails[0].pwrOutDynamicNormalizedmW < pHi->pCoreRailMetrics->pwrOutDynamicNormalizedmW)))
        {
            pLwrr->coreRail.rails[0].pwrOutDynamicNormalizedmW =
                pHi->pCoreRailMetrics->pwrOutDynamicNormalizedmW;
            s_perfCfPwrModelDlppm1xGuardRailFlagsSet(
                pDlppm1x,
                pLwrr,
                LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_METRIC_CORE_RAIL_PWR,
                bMinimum ?
                    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_GUARD_RAIL_LINEAR_MINIMUM_PRIMARY_CLK :
                    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_GUARD_RAIL_LINEAR_MAXIMUM_PRIMARY_CLK);
        }

        goto s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrLinearity_exit;
    }

    numeratormW =
        (LwU64)(pHi->pCoreRailMetrics->pwrOutDynamicNormalizedmW -
            pLo->pCoreRailMetrics->pwrOutDynamicNormalizedmW);
    denominatorRatio =
        pHi->pPerfMetrics->perfRatio - pLo->pPerfMetrics->perfRatio;

    lw64Lsl(&numeratormW, &numeratormW, 24);
    lwU64Div(
        &slopemWPerRatio,
        &numeratormW,
        &(LwUFXP52_12){ denominatorRatio });

    diffRatio = pLwrr->perfMetrics.perfRatio - pLo->pPerfMetrics->perfRatio;

    //
    // Callwlate the extremummW and check for overflow. If we do overflow, then
    // default to the max value.
    //
    lw64Add(
        &extremummW64,
        &(LwU64){ multUFXP64(24, slopemWPerRatio, diffRatio) },
        &(LwU64){ pLo->pCoreRailMetrics->pwrOutDynamicNormalizedmW });
    if (lwU64CmpLE(&extremummW64, &(LwU64) { LW_U32_MAX }))
    {
        extremummW = (LwU32)extremummW64;
    }
    else
    {
        extremummW = LW_U32_MAX;
    }

    if (bMinimum)
    {
        if (pLwrr->coreRail.rails[0].pwrOutDynamicNormalizedmW < extremummW)
        {
            pLwrr->coreRail.rails[0].pwrOutDynamicNormalizedmW = extremummW;
            s_perfCfPwrModelDlppm1xGuardRailFlagsSet(
                pDlppm1x,
                pLwrr,
                LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_METRIC_CORE_RAIL_PWR,
                LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_GUARD_RAIL_LINEAR_MINIMUM_PRIMARY_CLK);
        }
    }
    else if (pLwrr->coreRail.rails[0].pwrOutDynamicNormalizedmW > extremummW)
    {
        pLwrr->coreRail.rails[0].pwrOutDynamicNormalizedmW = extremummW;
        s_perfCfPwrModelDlppm1xGuardRailFlagsSet(
            pDlppm1x,
            pLwrr,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_METRIC_CORE_RAIL_PWR,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_GUARD_RAIL_LINEAR_MAXIMUM_PRIMARY_CLK);
    }
    else
    {
        // Did not violate linearity.
    }

s_perfCfPwrModelDlppm1xGuardRailMetricsStructureCoreRailPwrLinearity_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xGetNumLoopVars
 */
static inline LwU16
s_perfCfPwrModelDlppm1xGetNumLoopVars
(
        const PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x
)
{
    //
    // Number of loop-dependent inputs =
    //     Number of absolute frequencies +
    //     Number of relative frequencies
    // Which in turn equals:
    //     (num rails) * 2 frequencies (absolute + relative) per rail
    return (pDlppm1x->coreRail.numRails + 1)*2;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xGuardRailMetricsStructureDramclk
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xGuardRailMetricsStructureDramclk
(
    PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *pLwrr,
    const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *pAdjacent
)
{
    LwU64_ALIGN32_ADD(
        &pDlppm1x->guardRailsStatus.isoPrimaryClkGuardRailCount,
        &pDlppm1x->guardRailsStatus.isoPrimaryClkGuardRailCount,
        &((LwU64_ALIGN32){ .hi = 0U, .lo = 1U, }));

    //
    // Ensure that the render time performance is monotonically
    // decreasing/increasing with respect to adjacent point, and if it is,
    // ensure the performance change is not super-linear.
    //
    if (((pLwrr->fbRail.freqkHz >= pAdjacent->fbRail.freqkHz) &&
         (pLwrr->perfMetrics.perfRatio < pAdjacent->perfMetrics.perfRatio)) ||
        ((pLwrr->fbRail.freqkHz <= pAdjacent->fbRail.freqkHz) &&
         (pLwrr->perfMetrics.perfRatio > pAdjacent->perfMetrics.perfRatio)))
    {
        pLwrr->perfMetrics.perfRatio = pAdjacent->perfMetrics.perfRatio;
        s_perfCfPwrModelDlppm1xGuardRailFlagsSet(
            pDlppm1x,
            pLwrr,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_METRIC_PERF,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_GUARD_RAIL_ADJACENT_DRAMCLK);
    }
    else
    {
        s_perfCfPwrModelDlppm1xGuardRailMetricsStructureDramclkPerfRatioLinearity(
            pDlppm1x,
            pLwrr,
            pAdjacent);
    }

    //
    // Ensure that the FB rail power is monotonically increasing/decreasing
    // with respect to the adjacent point.
    //
    if (((pLwrr->fbRail.freqkHz >= pAdjacent->fbRail.freqkHz) &&
         (pLwrr->fbRail.pwrOutTuple.pwrmW < pAdjacent->fbRail.pwrOutTuple.pwrmW)) ||
        ((pLwrr->fbRail.freqkHz <= pAdjacent->fbRail.freqkHz) &&
         (pLwrr->fbRail.pwrOutTuple.pwrmW > pAdjacent->fbRail.pwrOutTuple.pwrmW)))
    {
        pLwrr->fbRail.pwrOutTuple.pwrmW = pAdjacent->fbRail.pwrOutTuple.pwrmW;
        s_perfCfPwrModelDlppm1xGuardRailFlagsSet(
            pDlppm1x,
            pLwrr,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_METRIC_FB_PWR,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_GUARD_RAIL_ADJACENT_DRAMCLK);
    }

    return FLCN_OK;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xGuardRailMetricsStructureDramclkPerfRatioLinearity
 */
static void
s_perfCfPwrModelDlppm1xGuardRailMetricsStructureDramclkPerfRatioLinearity
(
    PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *pDlppm1xEstimated,
    const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *pKnownMetrics
)
{
    //
    // Numerical Analysis:
    //
    // dramclkRatio
    // Lwrrently, the minimum expected DRAMCLK is 405 MHz and the
    // (conservatively) maximum expected DRAMCLK is 10 GHz.
    //
    // This means the maximum ratio between DRAMCLKs is:
    //  (10 GHz * 1000 MHz / GHz) / (405 MHz)
    //  ~24.6914
    //
    // Colwserely, the minimum ratio is therefore:
    //  (405 MHz) / (10 GHz * 1000 MHz / GHz)
    //  0.0405
    //
    // This means that 5 integer bits and 5 fractional bis are required to
    // represent the ratio.
    //
    // However, 10 GHz requires 23 bits to be represented in kHz.
    //
    // This means tha the intermediate math must be 64-bit. However, the ned
    // result, as above, will require only 5 significant integer bits, which is
    // safe to place in a 20.12.
    //
    // perfRatioMultiplied = perfRatio_{known} * dramclkRatio
    // Deferred. This code will change soon in an immediate follow-up, so the
    // numerical anlaysis is deferred until then.
    //
    LwUFXP20_12 dramclkRatio =
        (LwUFXP20_12)LW_TYPES_U64_TO_UFXP_X_Y_SCALED(52, 12,
            pDlppm1xEstimated->fbRail.freqkHz,
            pKnownMetrics->fbRail.freqkHz);
    LwUFXP20_12 perfRatioMultiplied =
        multUFXP32(12, pKnownMetrics->perfMetrics.perfRatio, dramclkRatio);

    if (((pDlppm1xEstimated->fbRail.freqkHz > pKnownMetrics->fbRail.freqkHz) &&
         (pDlppm1xEstimated->perfMetrics.perfRatio > perfRatioMultiplied)) ||
        ((pDlppm1xEstimated->fbRail.freqkHz < pKnownMetrics->fbRail.freqkHz) &&
         (pDlppm1xEstimated->perfMetrics.perfRatio < perfRatioMultiplied)))
    {
        pDlppm1xEstimated->perfMetrics.perfRatio = perfRatioMultiplied;
        s_perfCfPwrModelDlppm1xGuardRailFlagsSet(
            pDlppm1x,
            pDlppm1xEstimated,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_METRIC_PERF,
            LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_GUARD_RAIL_LINEAR_DRAMCLK);
    }
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xDramclkApproxEqual
 */
static LwBool
s_perfCfPwrModelDlppm1xDramclkApproxEqual
(
    const PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
    LwU32 actualkHz,
    LwU32 expectedkHz
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
        actualkHz > expectedkHz ?
            actualkHz - expectedkHz :
            expectedkHz - actualkHz;
    LwUFXP52_12 percentDiff =
        LW_TYPES_U64_TO_UFXP_X_Y_SCALED(52, 12,
            dramclkDiffkHz,
            expectedkHz);

    return lwU64CmpLT(
        &percentDiff,
        &(LwU64) { pDlppm1x->dramclkMatchTolerance });
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xObservedMetricsReInit
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xObservedMetricsReInit
(
    PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8 railIdx;
    LwU64_ALIGN32 profileTimeStartns;

    //
    // We want to completely zero the structure, but we need to retain the volt
    // data from the previous iteration. We temporarily save this off to DLPPM
    // itself, and then we'll copy it back to the observed metrics after
    // zeroing.
    //
    for (railIdx = 0U; railIdx < pDlppm1x->coreRail.numRails; railIdx++)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            voltRailSensedVoltageDataCopyIn(
                &pDlppm1x->coreRail.pRails[railIdx].voltData,
                &pDlppm1xObserved->coreRail.rails[railIdx].voltData),
            s_perfCfPwrModelDlppm1xObservedMetricsReInit_exit);
    }

    // Similarly, save off the beginning exelwtion time as well.
    profileTimeStartns = pDlppm1xObserved->profiling.profiledTimesns[
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION_TOTAL];

    (void)memset(pDlppm1xObserved, 0, sizeof(*pDlppm1xObserved));

    // Copy the voltData back over
    for (railIdx = 0U; railIdx < pDlppm1x->coreRail.numRails; railIdx++)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            voltRailSensedVoltageDataCopyOut(
                &pDlppm1xObserved->coreRail.rails[railIdx].voltData,
                &pDlppm1x->coreRail.pRails[railIdx].voltData),
            s_perfCfPwrModelDlppm1xObservedMetricsReInit_exit);
    }

    // Copy the start time back over
     pDlppm1xObserved->profiling.profiledTimesns[LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED_PROFILING_REGION_TOTAL] =
         profileTimeStartns;

    // Ensure the type is set.
    pDlppm1xObserved->super.type =
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_DLPPM_1X;

    // Ensure that the current DRAMCLK initial estimates index is invalid
    pDlppm1xObserved->lwrrDramclkInitialEstimatesIdx = LW_U8_MAX;

    // Initializing the metrics, so need to ilwalidate any now-stale cache state
    LW2080_CTRL_NNE_NNE_DESC_INFERENCE_STATIC_VAR_CACHE_ILWALIDATE(
        &pDlppm1xObserved->staticVarCache);

    //
    // We need to start off with no correction; we will do one estimate of the
    // observed metrics themselves first and use that to determine the
    // corrections. So, to avoid the output of the observed estimate being
    // changed, set the corrections to "1."
    //
    pDlppm1xObserved->correction.perfPct =
        LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 1U);
    for (railIdx = 0U; railIdx < pDlppm1x->coreRail.numRails; railIdx++)
    {
        pDlppm1xObserved->correction.coreRailPwrPct[railIdx] =
            LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 1U);
    }
    pDlppm1xObserved->correction.fbPwrPct =
        LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 1U);

    s_perfCfPwrModelDlppm1xEstimatedMetricsInit(
        &pDlppm1xObserved->observedEstimated);

s_perfCfPwrModelDlppm1xObservedMetricsReInit_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xEstimatedMetricsInit
 */
static void
s_perfCfPwrModelDlppm1xEstimatedMetricsInit
(
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *pDlppm1xEstimated
)
{
    // Start by completely zeroing the structure.
    (void)memset(pDlppm1xEstimated, 0, sizeof(*pDlppm1xEstimated));

    // Ensure that the type gets re-set.
    pDlppm1xEstimated->super.type = LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_DLPPM_1X;

    // Ensure the guard rail flags are initialized.
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_GUARD_RAIL_FLAGS_INIT(
        &pDlppm1xEstimated->guardRailFlags);
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xGuardRailFlagsSet
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xGuardRailFlagsSet
(
    PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *pDlppm1xEstimated,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_METRIC metric,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_GUARD_RAIL guardRail
)
{
    const LwU32 flagIndex =
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_GUARD_RAIL_FLAGS_GUARD_RAIL_INDEX(
            metric,
            guardRail);

    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_GUARD_RAIL_FLAGS_SET(
        &pDlppm1xEstimated->guardRailFlags,
        metric,
        guardRail);

    LwU64_ALIGN32_ADD(
        &pDlppm1x->guardRailsStatus.guardRailCounts[flagIndex],
        &pDlppm1x->guardRailsStatus.guardRailCounts[flagIndex],
        &((LwU64_ALIGN32){ .hi = 0U, .lo = 1U, }));

    return FLCN_OK;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xGuardRailFlagsMetricGuardRailed
 */
static LwBool
s_perfCfPwrModelDlppm1xGuardRailFlagsMetricGuardRailed
(
    const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X *pDlppm1xEstimated,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_METRIC metric
)
{
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_GUARD_RAIL i;
    for (i = 0U;
         i < LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_GUARD_RAIL_NUM;
         i++)
    {
        if (LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_GUARD_RAIL_FLAGS_GET(
                &pDlppm1xEstimated->guardRailFlags,
                metric,
                i))
        {
            return LW_TRUE;
        }
    }

    return LW_FALSE;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xVpstateRailFreqkHzGet
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xVpstateRailFreqkHzGet
(
    VPSTATE_3X *pVpstate3x,
    const PERF_CF_PWR_MODEL_DLPPM_1X_RAIL *pRail,
    LwBool bFallbackToPstate,
    LwU32 *pFreqkHz,
    LwBool *pbAvailable
)
{
    FLCN_STATUS status;
    const LwBoardObjIdx vpstateIdx = BOARDOBJ_GET_GRP_IDX(&pVpstate3x->super.super);
    BOARDOBJGRPMASK_E32 clkDomainsMask;

    // Default to assuming that a frequency for the clock domain is not available.
    *pbAvailable = LW_FALSE;

    PMU_ASSERT_OK_OR_GOTO(status,
        perfLimitsVpstateIdxToClkDomainsMask(vpstateIdx, &clkDomainsMask),
        s_perfCfPwrModelDlppm1xVpstateRailFreqkHzGet_exit);

    //
    // If the clock domain for the rail is specified in the VPSTATE, then
    // we can always extract it. Otherwise, fallback to the PSTATE if instructed
    // to.
    //
    if (boardObjGrpMaskBitGet(&clkDomainsMask, pRail->clkDomIdx) ||
        bFallbackToPstate)
    {
        PERF_LIMITS_VF perfLimitsVf =
        {
            .idx = pRail->clkDomIdx,
        };

        PMU_ASSERT_OK_OR_GOTO(status,
            perfLimitsVpstateIdxToFreqkHz(
                vpstateIdx,
                LW_TRUE,
                bFallbackToPstate,
                &perfLimitsVf),
            s_perfCfPwrModelDlppm1xVpstateRailFreqkHzGet_exit);

        *pFreqkHz = perfLimitsVf.value;
    }
    else
    {
        // Exit early if the frequency is not available.
        goto s_perfCfPwrModelDlppm1xVpstateRailFreqkHzGet_exit;
    }

    // Mark the frequency available if we successfully made it here.
    *pbAvailable = LW_TRUE;

s_perfCfPwrModelDlppm1xVpstateRailFreqkHzGet_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xObservedOutputRailMetrics
 */
static FLCN_STATUS
s_perfCfPwrModelDlppm1xObserveOutputRailMetrics
(
    const PERF_CF_PWR_MODEL_SAMPLE_DATA                                 *pSampleData,
    const PERF_CF_PWR_MODEL_DLPPM_1X_RAIL                               *pRail,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_OBSERVED_METRICS_DLPPM_1X_RAIL   *pRailMetrics
)
{
    FLCN_STATUS status = FLCN_OK;

    PMU_ASSERT_TRUE_OR_GOTO(status,
            (pSampleData != NULL &&
            pRail        != NULL &&
            pRailMetrics != NULL),
            FLCN_ERR_ILWALID_ARGUMENT,
            s_perfCfPwrModelDlppm1xObserveOutputRailMetrics_exit);

    // If output channel is valid, read power reading directly
    if (pRail->outPwrChIdx != LW2080_CTRL_BOARDOBJ_IDX_ILWALID)
    {
        pRailMetrics->super.pwrOutTuple = pSampleData->pPwrChannelsStatus->channels[pRail->outPwrChIdx].channel.tuple;
    }
    // Else do an input to output colwersion using the VR efficencies
    else
    {
        PWR_CHRELATIONSHIP_REGULATOR_EFF_EST_V1 *pVrEfficencyChRel =
                (PWR_CHRELATIONSHIP_REGULATOR_EFF_EST_V1 *)PWR_CHREL_GET(pRail->vrEfficiencyChRelIdxInToOut);

        PMU_ASSERT_TRUE_OR_GOTO(status,
            pVrEfficencyChRel != NULL,
            FLCN_ERR_OBJECT_NOT_FOUND,
            s_perfCfPwrModelDlppm1xObserveOutputRailMetrics_exit);

        // Use the sensed voltage as the output voltage value
        pRailMetrics->super.pwrOutTuple.voltuV = pRailMetrics->super.voltuV;

        //
        // Else callwlate output power using the VR efficency
        // Note: pwrOutTuple.voltageuV is pre-populated with the sensed voltage.
        //
        PMU_ASSERT_OK_OR_GOTO(status,
            pwrChRelationshipRegulatorEffEstV1EvalSecondary(
                pVrEfficencyChRel,
                &(pRailMetrics->super.pwrInTuple),
                &(pRailMetrics->super.pwrOutTuple)),
            s_perfCfPwrModelDlppm1xObserveOutputRailMetrics_exit);
    }

s_perfCfPwrModelDlppm1xObserveOutputRailMetrics_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelDlppm1xCorrectionFrequenciesValid
 */
static LwBool
s_perfCfPwrModelDlppm1xCorrectionFrequenciesValid
(
    const PERF_CF_PWR_MODEL_DLPPM_1X *pDlppm1x,
    const LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_DLPPM_1X_OBSERVED *pDlppm1xObserved
)
{
    //
    // Note that we explicitly do not check the core rails here. This is because
    // for the core rails, quantization can only move the frequency at most one
    // step, which should be ~15MHz, and this should never be enough to
    // ilwalidate a match between an observed frequency and an estimated one.
    // Therefore, the only concern here is if the observed frequency was
    // entirely out of the clock range, and this will already be handled by
    // other mechanisms:
    //  1.) If the observed frequency is higher than the max of the range, the
    //      quantization in perfCfPwrModelScale catches this.
    //  2.) If the observed frequency is less than the min of the range, the
    //      DLPPM_1X_ESTIMATION_MINIMUM VPSTATE's minimum primary clock should
    //      already have ilwalidated the metrics.
    // Given this, there's no need to check the quantization of the core rails.
    //

    //
    // Check if the quantized DRAMCLK frequency approximately equals the
    // unquantized one
    //
    if (!s_perfCfPwrModelDlppm1xDramclkApproxEqual(
            pDlppm1x,
            pDlppm1xObserved->observedEstimated.fbRail.freqkHz,
            pDlppm1xObserved->fbRail.super.freqkHz))
    {
        return LW_FALSE;
    }

    // If all checked rails are approximately equal, then return true
    return LW_TRUE;
}
