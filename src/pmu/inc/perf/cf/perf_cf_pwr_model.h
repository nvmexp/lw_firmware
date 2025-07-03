/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perf_cf_pwr_model.h
 * @brief   Common PERF_CF Pwr Model related defines.
 *
 * @copydoc perf_cf.h
 */

#ifndef PERF_CF_PWR_MODEL_H
#define PERF_CF_PWR_MODEL_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp.h"
#include "perf/3x/vfe.h"
#include "perf/cf/perf_cf_pm_sensor.h"
#include "perf/cf/perf_cf_controller_status.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_CF_PWR_MODEL PERF_CF_PWR_MODEL, PERF_CF_PWR_MODEL_BASE;

typedef struct PERF_CF_PWR_MODELS PERF_CF_PWR_MODELS;

typedef struct PERF_CF_PWR_MODEL_SAMPLE_DATA PERF_CF_PWR_MODEL_SAMPLE_DATA;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * Colwenience macro to look-up PERF_CF_PWR_MODELS.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL))
#define PERF_CF_GET_PWR_MODELS() \
    (&(PERF_CF_GET_OBJ()->pwrModels))
#else
#define PERF_CF_GET_PWR_MODELS() NULL
#endif

/*!
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL))
#define BOARDOBJGRP_DATA_LOCATION_PERF_CF_PWR_MODEL \
    (&(PERF_CF_GET_PWR_MODELS()->super.super))
#else
#define BOARDOBJGRP_DATA_LOCATION_PERF_CF_PWR_MODEL \
    ((BOARDOBJGRP *)NULL)
#endif

/*!
 * @brief   List of ovl. descriptors required by PERF_CF PwrModel code paths.
 */
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_MODEL                                   \
        OSTASK_OVL_DESC_DEFINE_PERF_CF_BASE                                    \
        OSTASK_OVL_DESC_DEFINE_PERF_CF_MODEL_DLPPM_1X                          \
        OSTASK_OVL_DESC_DEFINE_PERF_CF_MODEL_TGP_1X                            \
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfCfModel)                     \
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfCfModel)                     \
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfVf)                          \
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perf)

/*!
 * @brief   List of ovl.descriptors required to construct PERF_CF pwr models.
 */
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_PWR_MODEL_CONSTRUCT                     \
        OSTASK_OVL_DESC_DEFINE_PERF_CF_PWR_MODEL_CONSTRUCT_DLPPM_1X

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL)
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_PWR_MODEL_LOAD                          \
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfCfModel)
#else
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_PWR_MODEL_LOAD                          \
        OVL_INDEX_ILWALID
#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL)

/*!
 * @brief   Base list of ovl. descriptors required when running the PWR_MODEL or
 *          CLIENT_PWR_MODEL_PROFILE scaling RPCs.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL_SCALE_RPC_NON_ODP)
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_PWR_MODEL_SCALE_RPC_NON_ODP_BASE        \
        OSTASK_OVL_DESC_DEFINE_PERF_CF_MODEL                                   \
        OSTASK_OVL_DESC_DEFINE_PERF_PWR_MODEL_SCALE_TGP_1X
#else
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_PWR_MODEL_SCALE_RPC_NON_ODP_BASE        \
        OVL_INDEX_ILWALID
#endif

/*!
 * @brief   List of ovl. descriptors required when running the PWR_MODEL scale RPC.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL_SCALE_RPC_NON_ODP)
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_PWR_MODEL_SCALE_RPC_NON_ODP             \
        OSTASK_OVL_DESC_DEFINE_PERF_CF_PWR_MODEL_SCALE_RPC_NON_ODP_BASE        \
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfCfPwrModelScaleRpcBufNonOdp)
#else
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_PWR_MODEL_SCALE_RPC_NON_ODP             \
        OVL_INDEX_ILWALID
#endif

/*!
 * Helper macro to colwert given clock frequency from MHz to kHz
 *
 * MHz * 1000 => kHz
 */
#define PERF_CF_PWR_MODEL_COLWERT_FREQ_MHZ_TO_KHZ(_MHz)                       \
    ((_MHz) * 1000)

/*!
 * Helper macro to colwert given clock frequency from kHz to MHz
 *
 * MHz => kHz / 1000
 */
#define PERF_CF_PWR_MODEL_COLWERT_FREQ_KHZ_TO_MHZ(_kHz)                        \
    LW_UNSIGNED_ROUNDED_DIV((_kHz), 1000)

/*!
 * @copydoc boardObjIfaceModel10GetStatus
 */
#define perfCfPwrModelIfaceModel10GetStatus_SUPER                                          \
    boardObjIfaceModel10GetStatus

/*!
 * Macro to get BOARDOBJ pointer from PERF_CF_PWR_MODELS BOARDOBJGRP.
 *
 * @param[in]  idx         PERF_CF_PWR_MODEL index.
 *
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define PERF_CF_PWR_MODEL_GET(_idx)                                            \
    (BOARDOBJGRP_OBJ_GET(PERF_CF_PWR_MODEL, (_idx)))

/*!
 * Macro to get BOARDOBJ pointer from PERF_CF_PWR_MODELS BOARDOBJGRP by
 * semantic index.
 *
 * @param[in]  idx         PERF_CF_PWR_MODEL semantic index.
 *
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define PERF_CF_PWR_MODEL_GET_BY_SEMANTIC_IDX(_semanticIdx)                         \
    (((_semanticIdx) < LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SEMANTIC_INDEX__MAX) ?    \
            (PERF_CF_PWR_MODEL_GET(PERF_CF_GET_PWR_MODELS()->semanticIdxMap[(_semanticIdx)])) : NULL)

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Virtual BOARDOBJ child providing attributes common to all PERF_CF Pwr Models.
 */
struct PERF_CF_PWR_MODEL
{
    /*!
     * BOARDOBJ super-class.
     * Must be first element of the structure!
     */
    BOARDOBJ            super;

    /*!
     * Mask of CLK_DOMAINs that are needed as input when scaling. If certain
     * domains are not provided as input, they will be propogated on the
     * caller's behalf via the other inputs.
     */
    BOARDOBJGRPMASK_E32 requiredDomainsMask;
};

/*!
 * Collection of all PERF_CF Pwr Models and related information.
 */
struct PERF_CF_PWR_MODELS
{
    /*!
     * BOARDOBJGRP_E32 super class. This should always be the first member!
     */
    BOARDOBJGRP_E32     super;
    /*!
     * Clock domain index for graphics.
     */
    LwU8                grClkDomIdx;
    /*!
     * Clock domain index for frame buffer.
     */
    LwU8                fbClkDomIdx;
    /*!
     * Indexes of named PWR_MODELs
     */
    LwBoardObjIdx       semanticIdxMap[LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SEMANTIC_INDEX__MAX];
};

/*!
 * Data callers using PERF_CF_PWR_MODELS can set before passing in SAMPLE_DATA
 */
typedef struct
{
    /*!
     * Perf Target that is passed in by the caller
     */
    LwUFXP20_12 perfTarget;
} PERF_CF_PWR_MODEL_CALLER_SPECIFIED_SAMPLE_DATA;

/*!
 * PERF_CF sample data used by PWR_MODEL
 */
struct PERF_CF_PWR_MODEL_SAMPLE_DATA
{
   /*!
     * Topology status that contains recent readings of the Perf metrics.
     */
    PERF_CF_TOPOLOGYS_STATUS    *pPerfTopoStatus;
    /*!
     * Pwr channels status that contains recent readings of Pwr metrics.
     */
    PWR_CHANNELS_STATUS         *pPwrChannelsStatus;

    /*!
     * PM sensors status that contains recent readings of PM sensor metrics.
     */
    PERF_CF_PM_SENSORS_STATUS   *pPmSensorsStatus;

    /*!
     * PERF CF Controllers status that contains recent readings of CF Controllers metrics.
     */
    PERF_CF_CONTROLLERS_REDUCED_STATUS
                                *pPerfCfControllersStatus;

    /*!
     *  Pwr Model variables that the caller can set as part of the sample data
     */
    PERF_CF_PWR_MODEL_CALLER_SPECIFIED_SAMPLE_DATA
                                *pPerfCfPwrModelCallerSpecifiedData;
};

/*!
 * @copydoc LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS
 */
typedef struct
{
    /*!
     * METRICS type of the implementing PWR_MODEL.
     */
    LwU8 type;
} PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS;

/*!
 * @copydoc LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT
 */
typedef struct
{
    /*!
     * @brief   Mask of CLK_DOMAIN indices to be selected from ::freqkHz as
     *          overriden input to PWR_MODEL_SCALE.
     */
    BOARDOBJGRPMASK_E32 domainsMask;

    /*!
     * @brief   Frequencies (indexed by CLK_DOMAIN indices) to use as input to
     *          the estimation.
     */
    LwU32 freqkHz[LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT_MAX];
} PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT;

/*!
 * @interface   PERF_CF_PWR_MODEL
 *
 * @brief   Initialize metrics data in each PERF_CF_PWR_MODEL
 *
 * @param[in]   pPwrModel             Pointer to the PERF_CF_PWR_MODEL object
 * @param[in]   pSampleData           Pointer to sample data to initialize
 * @param[in]   pPerfTopoStatus       Pointer to PERF_CF topology status to use for Perf sampling
 * @param[in]   pPwrChannelsStatus    Pointer to PWR_POLICY channel status to use for Pwr sampling
 * @param[in]   pPmSensorsStatus      Pointer to PM sensors status to use for PM sensor sampling
 *
 * @return  FLCN_OK     Metrics sample data initialized successfully
 * @return  other       Propagates return values from various calls
 */
#define PerfCfPwrModelSampleDataInit(fname) FLCN_STATUS (fname)(                        \
    PERF_CF_PWR_MODEL                              *pPwrModel,                          \
    PERF_CF_PWR_MODEL_SAMPLE_DATA                  *pSampleData,                        \
    PERF_CF_TOPOLOGYS_STATUS                       *pPerfTopoStatus,                    \
    PWR_CHANNELS_STATUS                            *pPwrChannelsStatus,                 \
    PERF_CF_PM_SENSORS_STATUS                      *pPmSensorsStatus,                   \
    PERF_CF_CONTROLLERS_REDUCED_STATUS             *pPerfCfControllersStatus,           \
    PERF_CF_PWR_MODEL_CALLER_SPECIFIED_SAMPLE_DATA *pPerfCfPwrModelCallerSpecifiedData  \
)

/*!
 * @interface   PERF_CF_PWR_MODEL
 *
 * @brief   Colwert sampled data into metrics for PERF_CF_PWR_MODEL
 *
 * @param[in]     pPwrModel         Pointer to the PERF_CF_PWR_MODEL object
 * @param[in/out] pSampleData       Pointer to metrics sample data
 * @param[out]    pObservedMetrics  Pointer to store observed metrics
 *
 * @return  FLCN_OK     All PERF_CF_PWR_MODELs metrics observed successfully
 * @return  other       Propagates return values from various calls
 */
#define PerfCfPwrModelObserveMetrics(fname) FLCN_STATUS (fname)(    \
    PERF_CF_PWR_MODEL                             *pPwrModel,       \
    PERF_CF_PWR_MODEL_SAMPLE_DATA                 *pSampleData,     \
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS    *pObservedMetrics \
)

/*!
 * @interface   PERF_CF_PWR_MODEL
 *
 * @brief   Scale all metrics in each PERF_CF_PWR_MODEL
 *
 * @param[in]     pPwrModel         Pointer to the PERF_CF_PWR_MODEL object
 * @param[in]     pObservedMetrics  Pointer to store observed metrics.
 * @param[in]     numMetricsInputs  Number of input structures to which to scale
 * @param[in]     pMetricsInputs    Array of input structures to which to scale
 * @param[out]    pEstimatedMetrics Array of pointers to structures in which to
 *                                  store estimated metrics.
 *
 * @return  FLCN_OK     All PERF_CF_PWR_MODELs metrics scaled successfully
 * @return  other       Propagates return values from various calls
 */
#define PerfCfPwrModelScale(fname) FLCN_STATUS (fname)(                \
    PERF_CF_PWR_MODEL                              *pPwrModel,         \
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS     *pObservedMetrics,  \
    LwLength                                        numMetricsInputs,  \
    PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT          *pMetricsInputs,    \
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS    **ppEstimatedMetrics,\
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS            *pTypeParams        \
)

/*!
 * @brief Estimate the perf/power across a primary clock frequency range.
 *
 * Return the set of estimated metrics corresponding to the primary clock frequency
 * range @ref pPrimaryClkRangekHz, inclusive, sampled as evenly as possible,
 * with a max resolution of @ref LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_ESTIMATED_METRICS_MAX
 *
 * @param[in]       pPwrModel               DLPPC_1X object pointer.
 * @param[in]       dramclkkHz              DRAMCLK frequency to estimate the efficiency lwrve for.
 * @param[in]       pPrimaryClkRangekHz     Range of primary clock frequncies to consider, inclusive, in kHz.
 * @param[out]      ppEstimatedMetrics      Array of efficiency lwrve metrics estimated at points.
 *                                          This array should have at least
 *                                          @ref LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_ESTIMATED_METRICS_MAX
 *                                          elements.
 * @param[in,out]   pNumEstimatedMetrics    On in, the number of elements in
 *                                          ppEstimatedMetrics array. On out,
 *                                          the number of valid estimated
 *                                          metrics in ppEstimatedMetrics.
 * @param[out]      pBMorePoints            Pointer to a Boolean indicating if there are VF points bounded by @ref *
 *
 * @return  FLCN_OK                     If the estimation was successful.
 * @return  FLCN_ERR_ILWALID_ARGUMENT   If any pointer argument is NULL.
 * @return  Others                      Errors propagated from callees
 */
#define PerfCfPwrModelScalePrimaryClkRange(fname) FLCN_STATUS (fname)(         \
    PERF_CF_PWR_MODEL *pPwrModel,                                              \
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS *pObservedMetrics,              \
    LwU32 dramclkkHz,                                                          \
    PERF_LIMITS_VF_RANGE *pPrimaryClkRangekHz,                                 \
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS **ppEstimatedMetrics,           \
    LwU8 *pNumEstimatedMetrics,                                                \
    LwBool *pBMorePoints,                                                      \
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS *pTypeParams)                          \

/*!
 * @interface   PERF_CF_PWR_MODEL
 *
 * @brief   Loads a PERF_CF_PWR_MODEL.
 *
 * @param[in]   pPwrModel   PERF_CF_PWR_MODEL object pointer
 *
 * @return  FLCN_OK     PERF_CF_PWR_MODEL loaded successfully
 * @return  other       Propagates return values from various calls
 */
#define PerfCfPwrModelLoad(fname) FLCN_STATUS (fname)(PERF_CF_PWR_MODEL *pPwrModel)

/*!
 * @interface   PERF_CF_PWR_MODEL
 *
 * @brief   Load all PERF_CF_PWR_MODELs.
 *
 * @param[in]   pPwrModels      PERF_CF_PWR_MODEL object pointer.
 * @param[in]   bLoad           LW_TRUE on load(), LW_FALSE on unload().
 *
 * @return  FLCN_OK     All PERF_CF_PWR_MODELs loaded successfully
 * @return  other       Propagates return values from various calls
 */
#define PerfCfPwrModelsLoad(fname) FLCN_STATUS (fname)(PERF_CF_PWR_MODELS *pPwrModels, LwBool bLoad)

/*!
 * @interface   PERF_CF_PWR_MODEL
 *
 * @brief   Initialize observed metrics, if necessary.
 *
 * @param[in]    pPwrModel   PERF_CF_PWR_MODEL object pointer.
 * @param[out]   pMetrics    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS pointer.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   @ref pMetrcs is NULL.
 * @return other                       Propagates return values from various calls.
 * @return FLCN_OK                     @ref pMetrics was successfully initialized.
 */
#define PerfCfPwrModelObservedMetricsInit(fname) FLCN_STATUS (fname)(          \
    PERF_CF_PWR_MODEL *pPwrModel,                                              \
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS *pObserveMetrics)

/*!
 * @interface   PERF_CF_PWR_MODEL
 *
 * @brief   Initializes a PMU @ref PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS structure\
 *          from the control call layer structure.
 *
 * @param[in]   pPwrModel   PERF_CF_PWR_MODEL object pointer.
 * @param[in]   pCtrlParams Control call layer structure.
 * @param[out]  pTypeParams @ref PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS structure
 *                          to initialize. *Must* point to a structure of the
 *                          appropriate sub-type for pPwrModel's
 *                          @ref PERF_CF_PWR_MODEL::super.type field. This can
 *                          normally be accomplished by passing a pointer to a
 *                          union of all types as this argument.
 *
 * @return  FLCN_OK                     Success
 * @return  FLCN_ERR_ILWALID_ARGUMENT   If pPwrModel, pCtrlParams, or
 *                                      pTypeParams are NULL
 * @return  FLCN_ERR_NOT_SUPPORTED      If pPwrModel->super.type is not a
 *                                      recognized PWR_MODEL type.
 * @return  Others                      Errors propagated from callees.
 */
#define PerfCfPwrModelScaleTypeParamsInit(fname) FLCN_STATUS (fname)(          \
    PERF_CF_PWR_MODEL *pPwrModel,                                              \
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS *pCtrlParams,         \
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS *pTypeParams)

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// BOARDOBJGRP interfaces.
BoardObjGrpIfaceModel10CmdHandler                         (perfCfPwrModelBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfPwrModelBoardObjGrpIfaceModel10Set");
BoardObjGrpIfaceModel10CmdHandler                         (perfCfPwrModelBoardObjGrpIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfPwrModelBoardObjGrpIfaceModel10GetStatus");

// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet                             (perfCfPwrModelGrpIfaceModel10ObjSetImpl_SUPER)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfPwrModelGrpIfaceModel10ObjSetImpl_SUPER");

// PERF_CF Pwr Model related interfaces.
PerfCfPwrModelSampleDataInit                  (perfCfPwrModelSampleDataInit)
    GCC_ATTRIB_SECTION("imem_perfCfLoad", "perfCfPwrModelSampleDataInit");
PerfCfPwrModelsLoad                           (perfCfPwrModelsLoad)
    GCC_ATTRIB_SECTION("imem_perfCfLoad", "perfCfPwrModelsLoad");

PerfCfPwrModelObserveMetrics               (perfCfPwrModelObserveMetrics)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "perfCfPwrModelObserveMetrics");
PerfCfPwrModelScale                        (perfCfPwrModelScale)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "perfCfPwrModelScale");
PerfCfPwrModelScalePrimaryClkRange         (perfCfPwrModelScalePrimaryClkRange)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "perfCfPwrModelScalePrimaryClkRange");
PerfCfPwrModelObservedMetricsInit           (perfCfPwrModelObservedMetricsInit)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "perfCfPwrModelObservedMetricsInit");
PerfCfPwrModelScaleTypeParamsInit           (perfCfPwrModelScaleTypeParamsInit)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "perfCfPwrModelScaleTypeParamsInit");

/* ------------------------ Inline Functions -------------------------------- */
/*!
 * @copydoc PerfCfPwrModelScaleTypeParamsInit
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfCfPwrModelScaleTypeParamsInit_SUPER
(
    PERF_CF_PWR_MODEL *pPwrModel,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS *pCtrlParams,
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS *pTypeParams
)
{
    pTypeParams->type = pCtrlParams->type;
    return FLCN_OK;
}

/*!
 * @brief      Initialize a PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT.
 *
 * @param      pMetricsInput  The metrics input
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
perfCfPwrModelScaleMeticsInputInit
(
    PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT *pMetricsInput
)
{
    boardObjGrpMaskInit_E32(&pMetricsInput->domainsMask);
    memset(pMetricsInput->freqkHz, 0 , sizeof(pMetricsInput->freqkHz));
}

/*!
 * @brief      Import data from an SDK structure into native PMU
 *             PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT structure.
 *
 * @param      pCtrlMetricsInput  The control metrics input
 * @param      pMetricsInput      The metrics input
 *
 * @return     The flcn status.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfCfPwrModelScaleMeticsInputImport
(
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT *pCtrlMetricsInput,
    PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT *pMetricsInput
)
{
    FLCN_STATUS status;
    LwBoardObjIdx i;

    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpMaskImport_E32(&pMetricsInput->domainsMask,
                                  &pCtrlMetricsInput->domainsMask),
        perfCfPwrModelScaleMeticsInputImport_exit);

    BOARDOBJGRPMASK_FOR_EACH_BEGIN(&pMetricsInput->domainsMask, i)
    {
        pMetricsInput->freqkHz[i] = pCtrlMetricsInput->freqkHz[i];
    }
    BOARDOBJGRPMASK_FOR_EACH_END;

perfCfPwrModelScaleMeticsInputImport_exit:
    return status;
}

/*!
 * @brief      Check to see if the clock domain at clkDomainIndex is specified
 *             in this set of metrics input.
 *
 * @param      pMetricsInput   The metrics input
 * @param[in]  clkDomainIndex  The clock domain index
 *
 * @return     LW_TRUE if the domain is specified, LW_FALSE otherwise
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwBool
perfCfPwrModelScaleMeticsInputCheckIndex
(
    PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT *pMetricsInput,
    LwBoardObjIdx clkDomainIndex
)
{
    if (clkDomainIndex >= LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT_MAX)
    {
        return LW_FALSE;
    }

    return boardObjGrpMaskBitGet(&pMetricsInput->domainsMask, clkDomainIndex);
}

/*!
 * @brief      Accessor for a copy of ::domainsMask
 *
 * @param      pMetricsInput  The metrics input
 * @param      pMask          pointer to mask to be set
 *
 * @return     FLCN_ERR_ILWALID_ARGUMENT for any NULL inputs, FLCN_OK otherwise
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfCfPwrModelScaleMeticsInputGetDomainsMask
(
    PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT *pMetricsInput,
    BOARDOBJGRPMASK_E32 *pMask
)
{
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pMetricsInput == NULL) ||
         (pMask == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        perfCfPwrModelScaleMeticsInputGetDomainsMask_exit);

    *pMask = pMetricsInput->domainsMask;

perfCfPwrModelScaleMeticsInputGetDomainsMask_exit:
    return status;
}

/*!
 * @brief      Accessor for a reference of ::domainsMask
 *
 * @param      pMetricsInput  The metrics input
 * @param      ppMask         pointer to mask reference to be set
 *
 * @return     FLCN_ERR_ILWALID_ARGUMENT for any NULL inputs, FLCN_OK otherwise
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfCfPwrModelScaleMeticsInputGetDomainsMaskRef
(
    PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT *pMetricsInput,
    BOARDOBJGRPMASK_E32 **ppMask
)
{
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pMetricsInput == NULL) ||
         (ppMask == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        perfCfPwrModelScaleMeticsInputGetDomainsMaskRef_exit);

    *ppMask = &pMetricsInput->domainsMask;

perfCfPwrModelScaleMeticsInputGetDomainsMaskRef_exit:
    return status;
}

/*!
 * @brief      Accessor for PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT::freqkHz[]
 *             with index validation
 *
 * @param      pMetricsInput   The metrics input
 * @param[in]  clkDomainIndex  The clock domain index
 * @param      pFreqkHz        The frequency pointer to be filled in
 *
 * @return     FLCN_OK in case of success, error otherwise.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfCfPwrModelScaleMeticsInputGetFreqkHz
(
    PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT *pMetricsInput,
    LwBoardObjIdx clkDomainIndex,
    LwU32 *pFreqkHz
)
{
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pMetricsInput == NULL) ||
         (clkDomainIndex == LW2080_CTRL_BOARDOBJ_IDX_ILWALID) ||
         (pFreqkHz == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        perfCfPwrModelScaleMeticsInputGetFreqkHz_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfPwrModelScaleMeticsInputCheckIndex(pMetricsInput,
            clkDomainIndex) ? FLCN_OK : FLCN_ERR_ILWALID_INDEX,
        perfCfPwrModelScaleMeticsInputGetFreqkHz_exit);

    *pFreqkHz = pMetricsInput->freqkHz[clkDomainIndex];

perfCfPwrModelScaleMeticsInputGetFreqkHz_exit:
    return status;
}

/*!
 * @brief      Mutator for PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT::freqkHz[]
 *             with index validation. Will also set the specified index in
 *             domainsMask.
 *
 * @param      pPwrModel       The power model
 * @param      pMetricsInput   The metrics input
 * @param[in]  clkDomainIndex  The clock domain index
 * @param      pFreqkHz        The frequency pointer to be filled in
 *
 * @return     FLCN_OK in case of success, error otherwise.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfCfPwrModelScaleMeticsInputSetFreqkHz
(
    PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT *pMetricsInput,
    LwBoardObjIdx clkDomainIndex,
    LwU32 freqkHz
)
{
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pMetricsInput == NULL) ||
         (clkDomainIndex >= LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT_MAX)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        perfCfPwrModelScaleMeticsInputSetFreqkHz_exit);

    boardObjGrpMaskBitSet(&pMetricsInput->domainsMask, clkDomainIndex);
    pMetricsInput->freqkHz[clkDomainIndex] = freqkHz;

perfCfPwrModelScaleMeticsInputSetFreqkHz_exit:
    return status;
}

/* ------------------------ Include Derived Types --------------------------- */
#include "perf/cf/perf_cf_pwr_model_dlppm_1x.h"
#include "perf/cf/perf_cf_pwr_model_tgp_1x.h"

#endif // PERF_CF_PWR_MODEL_H
