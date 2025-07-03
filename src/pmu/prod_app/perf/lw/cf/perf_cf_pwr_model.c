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
 * @file    perf_cf_pwr_model.c
 * @copydoc perf_cf_pwr_model.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "pmu/ssurface.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf.h"
#include "perf/cf/perf_cf_pwr_model.h"
#include "pmu_objperf.h"

#include "g_pmurpc.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjGrpIfaceModel10SetHeader             (s_perfCfPwrModelIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfPwrModelIfaceModel10SetHeader");
static BoardObjGrpIfaceModel10SetEntry              (s_perfCfPwrModelIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfPwrModelIfaceModel10SetEntry");
static BoardObjIfaceModel10GetStatus                          (s_perfCfPwrModelIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfPwrModelIfaceModel10GetStatus");
static PerfCfPwrModelLoad                     (s_perfCfPwrModelLoad)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfPwrModelLoad");

/*!
 * @brief   Retrieves the appropriate "super" pointer from the union of all
 *          metrics structures.
 *
 * @details The metrics union cannot contain a "super" structure due to how XAPI
 *          works, so extracting a super pointer is not straightforward. This
 *          function retrieves it from the struct of appropriate type.
 *
 * @param[in]   pObserved   The struct containing the union of all metrics
 *                          structs
 *
 * @return  NULL if pObserved is not of a known type
 * @return  A pointer to the appropriate "super" struct otherwise.
 */
static LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS *s_perfCfPwrModelObservedMetricsGet(
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_OBSERVED *pObserved)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelObservedMetricsGet");

/*!
 * @brief   Iinitializes the array of pointers to metrics structures for
 *          initialization.
 *
 * @details Due to the polymorphic behavior required for
 *          @ref perfCfPwrModelScale, it takes an array of pointers to metrics
 *          structs in which to store the estimated information. This function
 *          initializes such an array for the PWR_MODEL_SCALE RPC path.
 *
 * @param[in]   type                Type of the estimation structs
 * @param[in]   numEstimatedMetrics Number of estimations that will be made.
 * @param[in]   pEstimated          Array containing unions of all metrics
 *                                  structs
 * @param[out]  ppEstimatedPointers Array in which to initialize pointers to
 *                                  appropriate pEstimated super structs.
 *
 * @return  FLCN_OK                     ppEstimatedPointers initialized
 *                                      correctly
 * @return  FLCN_ERR_ILWALID_ARGUMENT   Invalid type provided.
 */
static FLCN_STATUS s_perfCfPwrModelEstimatedMetricsGet(
    LwU8 type,
    LwU8 numEstimatedMetrics,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_ESTIMATED *pEstimated,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS **ppEstimatedPointers)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelEstimatedMetricsGet");

/*!
 * @brief      ODP variant of pmuRpcPerfCfPwrModelScale
 *
 * @param      pParams  The RPC parameters
 *
 * @return     FLCN_OK on success, FLCN_ERR_* otherwise
 */
static FLCN_STATUS s_pmuRpcPerfCfPwrModelScale_ODP(RM_PMU_RPC_STRUCT_PERF_CF_PWR_MODEL_SCALE *pParams)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_pmuRpcPerfCfPwrModelScale_ODP");

/*!
 * @brief      NON-ODP variant of pmuRpcPerfCfPwrModelScale
 *
 * @param      pParams  The RPC parameters
 *
 * @return     FLCN_OK on success, FLCN_ERR_* otherwise
 */
static FLCN_STATUS s_pmuRpcPerfCfPwrModelScale_NON_ODP(RM_PMU_RPC_STRUCT_PERF_CF_PWR_MODEL_SCALE *pParams)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_pmuRpcPerfCfPwrModelScale_NON_ODP");

/*!
 * @brief      Popagate provided frequency inputs to remaining required clk
 *             domains of pPwrModel.
 *
 * @param           pPwrModel         The power model
 * @param[in]       numMetricsInputs  The number metrics inputs
 * @param[inout]    pMetricsInputs    The metrics inputs
 *
 * @return     FLCN_OK on success, FLCN_ERR_* otherwise
 */
static FLCN_STATUS s_perfCfPwrModelPropagateInputs(PERF_CF_PWR_MODEL *pPwrModel, LwLength numMetricsInputs, PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT *pMetricsInputs)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelPropagateInputs");


/*!
 * @brief      Cap provided frequency inputs to vf max
 *
 * @param           pPwrModel         The power model
 * @param[in]       numMetricsInputs  The number metrics inputs
 * @param[inout]    pMetricsInputs    The metrics inputs
 *
 * @return     FLCN_OK on success,
               FLCN_ERR_FREQ_NOT_SUPPORTED _without_ a breakpoint if provided frequency is outside of valid VF range,
               FLCN_ERR_* otherwise
 */
static FLCN_STATUS s_perfCfPwrModelQuantizeInputs(PERF_CF_PWR_MODEL *pPwrModel, LwLength numMetricsInputs, PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT *pMetricsInputs)
    GCC_ATTRIB_SECTION("imem_perfCfModel", "s_perfCfPwrModelQuantizeInputs");

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief      Utility macro to check that two structures share the same fields
 *             of the same size at the same offset at compile time.
 *
 * @param      structA  typedef'd struct
 * @param      structB  another typedef'd struct
 * @param      field    field within both structs
 */
#define CT_OFFSET_AND_SIZE_EQUAL(structA, structB, field)                               \
    ct_assert((LW_OFFSETOF(structA, field) == LW_OFFSETOF(structB, field)) &&           \
              (sizeof(((structA*)(NULL))->field) == sizeof(((structB*)(NULL))->field)))

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief   Union representing superset of all scale params types supported for
 *          ODP profiles.
 */
typedef union
{
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS super;
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_DLPPM_1X dlppm1x;
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_TGP_1X tgp1x;
} PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_DATA_ODP;

/*!
 * @brief   Copy of PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_DATA_ODP with types omitted
 *          that are only supported on ODP profiles.
 */
typedef union
{
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS super;
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_TGP_1X tgp1x;
    // Add types here as they are supported for NON_ODP RPC...
} PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_DATA_NON_ODP;

/*!
 * @brief   Copy of LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_OBSERVED with
 *          types omitted that are only supported on ODP profiles.
 */
typedef struct
{
    /*!
     * @copydoc LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_OBSERVED::type
     */
    LwU8 type;

    /*!
     * @copydoc LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_OBSERVED_DATA
     */
    union
    {
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TGP_1X_OBSERVED tgp1x;
        // Add types here as they are supported for NON_ODP RPC...
    } data;
} PERF_CF_PWR_MODEL_METRICS_OBSERVED_NON_ODP;

CT_OFFSET_AND_SIZE_EQUAL(PERF_CF_PWR_MODEL_METRICS_OBSERVED_NON_ODP,
                         LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_OBSERVED,
                         type);

CT_OFFSET_AND_SIZE_EQUAL(PERF_CF_PWR_MODEL_METRICS_OBSERVED_NON_ODP,
                         LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_OBSERVED,
                         data.tgp1x);

/*!
 * @brief   Copy of LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_ESTIMATED with
 *          types omitted that are only supported on ODP profiles.
 */
typedef struct
{
    /*!
     * @copydoc LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_ESTIMATED::type
     */
    LwU8 type;

    /*!
     * @copydoc LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_ESTIMATED_DATA
     */
    union
    {
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TGP_1X tgp1x;
        // Add types here as they are supported for NON_ODP RPC...
    } data;
} PERF_CF_PWR_MODEL_METRICS_ESTIMATED_NON_ODP;

CT_OFFSET_AND_SIZE_EQUAL(PERF_CF_PWR_MODEL_METRICS_ESTIMATED_NON_ODP,
                         LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_ESTIMATED,
                         type);

CT_OFFSET_AND_SIZE_EQUAL(PERF_CF_PWR_MODEL_METRICS_ESTIMATED_NON_ODP,
                         LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_ESTIMATED,
                         data.tgp1x);

/*!
 * @brief   Copy of LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS with
 *          types omitted that are only supported on ODP profiles.
 */
typedef struct
{
    /*!
     * @copydoc LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS::type
     */
    LwU8 type;

    /*!
     * @copydoc LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_DATA
     */
    union
    {
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_TGP_1X tgp1x;
        // Add types here as they are supported for NON_ODP RPC...
    } data;
} PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_NON_ODP;

CT_OFFSET_AND_SIZE_EQUAL(PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_NON_ODP,
                         LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS,
                         type);

CT_OFFSET_AND_SIZE_EQUAL(PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_NON_ODP,
                         LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS,
                         data.tgp1x);

/*!
 * @brief   Type definition for all data needed in dmem scratch buffer for
 *          s_pmuRpcPerfCfPwrModelScale_ODP.
 */
typedef struct
{
    /*!
     * @brief   Buffer for holding data required for a scale during the
     *          PWR_MODEL_SCALE RPC
     */
    RM_PMU_PERF_CF_PWR_MODEL_SCALE              buffer;

    /*!
     * @brief   Array of metrics inputs colwerted from SDK structures in ::buffer.
     */
    PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT       metricsInput[LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_ESTIMATED_METRICS_MAX];

    /*!
     * @brief   Array of pointers pointer into the estimated metrics in ::buffer
     */
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS* estimatedPointers[LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_ESTIMATED_METRICS_MAX];
} PERF_CF_PWR_MODEL_SCALE_RPC_BUF_ODP;

/*!
 * @brief   Type definition for all data needed in dmem scratch buffer for
 *          s_pmuRpcPerfCfPwrModelScale_NON_ODP.
 */
typedef struct
{
    /*!
     * @brief Observed metrics to be scaled.
     */
    PERF_CF_PWR_MODEL_METRICS_OBSERVED_NON_ODP              observedMetrics;

    /*!
     * @brief Buffer holding type-specific data from RM/PMU interface.
     */
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_NON_ODP                  typeParamsBuffer;

    /*!
     * @brief Internal PMU representation of ::typeParamsBuffer to be used in scale
     *        routine.
     */
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_DATA_NON_ODP             typeParams;

    /*!
     * @brief Buffer holding a single metrics input from RM/PMU interface.
     */
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT  metricsInputBuffer;

    /*!
     * @brief Internal PMU representation of ::metricsInputBuffer to be used in
     *        scale routine.
     */
    PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT                   metricsInput;

    /*!
     * @brief Buffer holding a single estimated metric.
     */
    PERF_CF_PWR_MODEL_METRICS_ESTIMATED_NON_ODP             estimatedMetrics;
} PERF_CF_PWR_MODEL_SCALE_RPC_BUF_NON_ODP;

/* ------------------------ Global Variables -------------------------------- */
/*!
 * Scatch data needed for s_pmuRpcPerfCfPwrModelScale_ODP
 */
static PERF_CF_PWR_MODEL_SCALE_RPC_BUF_ODP pwrModelScaleRpcScratch_ODP
    GCC_ATTRIB_SECTION("dmem_perfCfPwrModelScaleRpcBufOdp", "pwrModelScaleRpcScratch_ODP");

/*!
 * Scatch data needed for s_pmuRpcPerfCfPwrModelScale_NON_ODP
 */
static PERF_CF_PWR_MODEL_SCALE_RPC_BUF_NON_ODP pwrModelScaleRpcScratch_NON_ODP
    GCC_ATTRIB_SECTION("dmem_perfCfPwrModelScaleRpcBufNonOdp", "pwrModelScaleRpcScratch_NON_ODP");

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
perfCfPwrModelBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS     status = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_PERF_CF_PWR_MODEL_CONSTRUCT
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Construct using header and entries.
        status = BOARDOBJGRP_IFACE_MODEL_10_SET(E32,                 // _grpType
                PERF_CF_PWR_MODEL,                          // _class
                pBuffer,                                    // _pBuffer
                s_perfCfPwrModelIfaceModel10SetHeader,            // _hdrFunc
                s_perfCfPwrModelIfaceModel10SetEntry,             // _entryFunc
                turingAndLater.perfCf.pwrModelGrpSet,       // _ssElement
                OVL_INDEX_DMEM(perfCfModel),                // _ovlIdxDmem
                BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfCfPwrModelBoardObjGrpIfaceModel10Set_exit; // NJ??
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

perfCfPwrModelBoardObjGrpIfaceModel10Set_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
perfCfPwrModelBoardObjGrpIfaceModel10GetStatus
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS(E32,                         // _grpType
            PERF_CF_PWR_MODEL,                              // _class
            pBuffer,                                        // _pBuffer
            NULL,                                           // _hdrFunc
            s_perfCfPwrModelIfaceModel10GetStatus,                      // _entryFunc
            ga10xAndLater.perfCf.pwrModelsGrpGetStatus),    // _ssElement
        perfCfPwrModelBoardObjGrpIfaceModel10GetStatus_exit);

perfCfPwrModelBoardObjGrpIfaceModel10GetStatus_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
perfCfPwrModelGrpIfaceModel10ObjSetImpl_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PERF_CF_PWR_MODEL   *pTmpModel = (RM_PMU_PERF_CF_PWR_MODEL *)pBoardObjDesc;
    PERF_CF_PWR_MODEL          *pModel;
    FLCN_STATUS                 status;

    // Sanity checking.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pBObjGrp      != NULL) &&
         (ppBoardObj    != NULL) &&
         (pBoardObjDesc != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfPwrModelGrpIfaceModel10ObjSetImpl_SUPER_exit);

    // Ilwoke the super-class constructor.
    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc),
        perfCfPwrModelGrpIfaceModel10ObjSetImpl_SUPER_exit);
    pModel = (PERF_CF_PWR_MODEL *)(*ppBoardObj);

    // Set member variables.
    (void)pTmpModel;
    boardObjGrpMaskInit_E32(&pModel->requiredDomainsMask);

perfCfPwrModelGrpIfaceModel10ObjSetImpl_SUPER_exit:
    return status;
}

/*!
 * @copydoc PerfCfPwrModelSampleDataInit
 */
FLCN_STATUS
perfCfPwrModelSampleDataInit
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
    FLCN_STATUS   status = FLCN_ERR_NOT_SUPPORTED;

    // Initialize super class data members
    pSampleData->pPerfTopoStatus          = pPerfTopoStatus;
    pSampleData->pPwrChannelsStatus       = pPwrChannelsStatus;
    pSampleData->pPmSensorsStatus         = pPmSensorsStatus;
    pSampleData->pPerfCfControllersStatus = pPerfCfControllersStatus;
    pSampleData->pPerfCfPwrModelCallerSpecifiedData =
                            pPerfCfPwrModelCallerSpecifiedData;

    switch (BOARDOBJ_GET_TYPE(pPwrModel))
    {
        case LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_TYPE_DLPPM_1X:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL_DLPPM_1X))
            {
                status = perfCfPwrModelSampleDataInit_DLPPM_1X(pPwrModel, pSampleData,
                    pPerfTopoStatus, pPwrChannelsStatus, pPmSensorsStatus,
                    pPerfCfControllersStatus, pPerfCfPwrModelCallerSpecifiedData);
            }
            else
            {
                PMU_BREAKPOINT();
                goto perfCfPwrModelSampleDataInit_exit;
            }
            break;

        case LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_TYPE_TGP_1X:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL_TGP_1X))
            {
                status = perfCfPwrModelSampleDataInit_TGP_1X(pPwrModel, pSampleData,
                    pPerfTopoStatus, pPwrChannelsStatus, pPmSensorsStatus,
                    pPerfCfControllersStatus, pPerfCfPwrModelCallerSpecifiedData);
            }
            else
            {
                PMU_BREAKPOINT();
                goto perfCfPwrModelSampleDataInit_exit;
            }
            break;

        default:
            PMU_BREAKPOINT();
            goto perfCfPwrModelSampleDataInit_exit;
            break;
    }

perfCfPwrModelSampleDataInit_exit:
    return status;
}

/*!
 * @copydoc PerfCfPwrModelObserveMetrics
 */
FLCN_STATUS
perfCfPwrModelObserveMetrics(
    PERF_CF_PWR_MODEL                          *pPwrModel,
    PERF_CF_PWR_MODEL_SAMPLE_DATA              *pSampleData,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS *pObservedMetrics
)
{
    FLCN_STATUS   status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pPwrModel))
    {
        case LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_TYPE_DLPPM_1X:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL_DLPPM_1X))
            {
                return perfCfPwrModelObserveMetrics_DLPPM_1X(pPwrModel, pSampleData,
                    pObservedMetrics);
            }
            else
            {
                PMU_BREAKPOINT();
                goto perfCfPwrModelObserveMetrics_exit;
            }
            break;

        case LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_TYPE_TGP_1X:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL_TGP_1X))
            {
                return perfCfPwrModelObserveMetrics_TGP_1X(pPwrModel, pSampleData,
                    pObservedMetrics);
            }
            else
            {
                PMU_BREAKPOINT();
                goto perfCfPwrModelObserveMetrics_exit;
            }
            break;

        default:
            PMU_BREAKPOINT();
            goto perfCfPwrModelObserveMetrics_exit;
            break;
    }

perfCfPwrModelObserveMetrics_exit:
    return status;
}

/*!
 * @copydoc PerfCfPwrModelScale
 */
FLCN_STATUS
perfCfPwrModelScale
(
    PERF_CF_PWR_MODEL                              *pPwrModel,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS     *pObservedMetrics,
    LwLength                                        numMetricsInputs,
    PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT          *pMetricsInputs,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS    **ppEstimatedMetrics,
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS            *pTypeParams
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    // Quantize the inputs
    status = s_perfCfPwrModelQuantizeInputs(pPwrModel,
                numMetricsInputs, pMetricsInputs);
    if (status == FLCN_ERR_FREQ_NOT_SUPPORTED)
    {
        // This is an RPC failure scenario we want to handle without a breakpoint.
        // TODO: once  PMU_TRUE_BP() actually works, revert this to PMU_ASSERT_OK_OR_GOTO()
        goto perfCfPwrModelScale_exit;
    }
    PMU_ASSERT_OK_OR_GOTO(status,
        status,
        perfCfPwrModelScale_exit);

    // Popagate inputs based on pPwrModel's required domain inputs
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelPropagateInputs(pPwrModel, numMetricsInputs, pMetricsInputs),
        perfCfPwrModelScale_exit);

    switch (BOARDOBJ_GET_TYPE(pPwrModel))
    {
        case LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_TYPE_DLPPM_1X:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL_DLPPM_1X))
            {
                PMU_HALT_OK_OR_GOTO(status,
                    perfCfPwrModelScale_DLPPM_1X(pPwrModel, pObservedMetrics,
                        numMetricsInputs, pMetricsInputs, ppEstimatedMetrics, pTypeParams),
                    perfCfPwrModelScale_exit);
            }
            else
            {
                PMU_BREAKPOINT();
                goto perfCfPwrModelScale_exit;
            }
            break;

        case LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_TYPE_TGP_1X:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL_TGP_1X))
            {
                PMU_HALT_OK_OR_GOTO(status,
                    perfCfPwrModelScale_TGP_1X(pPwrModel, pObservedMetrics,
                        numMetricsInputs, pMetricsInputs, ppEstimatedMetrics, pTypeParams),
                    perfCfPwrModelScale_exit);
            }
            else
            {
                PMU_BREAKPOINT();
                goto perfCfPwrModelScale_exit;
            }
            break;

        default:
            PMU_BREAKPOINT();
            goto perfCfPwrModelScale_exit;
            break;
    }

perfCfPwrModelScale_exit:
    return status;
}

/*!
 * @copydoc PerfCfPwrModelScalePrimaryClkRange
 */
FLCN_STATUS
perfCfPwrModelScalePrimaryClkRange
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
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        PERF_LIMITS_RANGE_IS_COHERENT(pPrimaryClkRangekHz) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
        perfCfPwrModelScalePrimaryClkRange_exit);

    status = FLCN_ERR_NOT_SUPPORTED;
    switch (BOARDOBJ_GET_TYPE(pPwrModel))
    {
        case LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_TYPE_DLPPM_1X:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL_DLPPM_1X))
            {
                status = perfCfPwrModelScalePrimaryClkRange_DLPPM_1X(
                    pPwrModel,
                    pObservedMetrics,
                    dramclkkHz,
                    pPrimaryClkRangekHz,
                    ppEstimatedMetrics,
                    pNumEstimatedMetrics,
                    pBMorePoints,
                    pTypeParams);
            }
            break;

        case LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_TYPE_TGP_1X:
            // Interface not implemented
            status = FLCN_ERR_NOT_SUPPORTED;
            break;

        default:
            status = FLCN_ERR_NOT_SUPPORTED;
            break;
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        status,
        perfCfPwrModelScalePrimaryClkRange_exit);

perfCfPwrModelScalePrimaryClkRange_exit:
    return status;
}

/*!
 * @copydoc PerfCfPwrModelsLoad
 */
FLCN_STATUS
perfCfPwrModelsLoad
(
    PERF_CF_PWR_MODELS *pPwrModels,
    LwBool              bLoad
)
{
    CLK_DOMAIN         *pClkDom;
    PERF_CF_PWR_MODEL  *pPwrModel;
    LwBoardObjIdx       i;
    FLCN_STATUS         status          = FLCN_OK;
    OSTASK_OVL_DESC     ovlDescList[]   = {
        OSTASK_OVL_DESC_DEFINE_PERF_CF_MODEL
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Nothing to do when there are no PWR_MODEL objects.
        if (boardObjGrpMaskIsZero(&(pPwrModels->super.objMask)))
        {
            goto perfCfPwrModelsLoad_exit;
        }

        if (bLoad)
        {
            // We cannot support PERF_CF models when there is no P-state.
            if (BOARDOBJGRP_IS_EMPTY(PSTATE))
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_NOT_SUPPORTED;
                goto perfCfPwrModelsLoad_exit;
            }

            // Find the clock domain index for DRAMCLK.
            pClkDom = clkDomainsGetByDomain(clkWhich_MClk);
            if (pClkDom == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_NOT_SUPPORTED;
                goto perfCfPwrModelsLoad_exit;
            }
            pPwrModels->fbClkDomIdx = BOARDOBJ_GET_GRP_IDX_8BIT(&pClkDom->super.super);

            // Find the clock domain index for GPCCLK.
            pClkDom = clkDomainsGetByDomain(clkWhich_GpcClk);
            if (pClkDom == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_NOT_SUPPORTED;
                goto perfCfPwrModelsLoad_exit;
            }
            pPwrModels->grClkDomIdx = BOARDOBJ_GET_GRP_IDX_8BIT(&pClkDom->super.super);

            // Load all power models.
            BOARDOBJGRP_ITERATOR_BEGIN(PERF_CF_PWR_MODEL, pPwrModel, i, NULL)
            {
                PMU_HALT_OK_OR_GOTO(status,
                    s_perfCfPwrModelLoad(pPwrModel),
                    perfCfPwrModelsLoad_exit);
            }
            BOARDOBJGRP_ITERATOR_END;
        }
        else
        {
            // Add unload() actions here.
        }
    }
perfCfPwrModelsLoad_exit:
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    return status;
}

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL_SCALE_RPC)
//
// The buffers required for the PWR_MODEL_SCALE RPC are rather large, so we want
// to ensure that only the ODP variant can only be called if on-demand paging is
// enabled.
//
#ifndef ON_DEMAND_PAGING_BLK
    ct_assert(!PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL_SCALE_RPC_ODP));
#endif // ON_DEMAND_PAGING_BLK
#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL_SCALE_RPC)

FLCN_STATUS
pmuRpcPerfCfPwrModelScale
(
    RM_PMU_RPC_STRUCT_PERF_CF_PWR_MODEL_SCALE *pParams
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL_SCALE_RPC_ODP))
    {
        perfReadSemaphoreTake();
        status = s_pmuRpcPerfCfPwrModelScale_ODP(pParams);
        perfReadSemaphoreGive();
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL_SCALE_RPC_NON_ODP))
    {
        // Perf semaphore acquired inside function to ensure perf dmem is attached
        status = s_pmuRpcPerfCfPwrModelScale_NON_ODP(pParams);
    }

    return status;
}

/*!
 * PerfCfPwrModelObservedMetricsInit
 */
FLCN_STATUS
perfCfPwrModelObservedMetricsInit
(
    PERF_CF_PWR_MODEL                            *pPwrModel,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS   *pObservedMetrics
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;
    switch (pPwrModel->super.type)
    {
        case LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_TYPE_DLPPM_1X:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL_DLPPM_1X))
            {
                status = perfCfPwrModelObservedMetricsInit_DLPPM_1X(pPwrModel, pObservedMetrics);
            }
            break;

        default:
            // nothing to do
            break;
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        status,
        perfCfPwrModelObservedMetricsInit_exit);

perfCfPwrModelObservedMetricsInit_exit:
    return status;
}

/*!
 * @copydoc PerfCfPwrModelScaleTypeParamsInit
 */
FLCN_STATUS
perfCfPwrModelScaleTypeParamsInit
(
    PERF_CF_PWR_MODEL *pPwrModel,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS *pCtrlParams,
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS *pTypeParams
)
{
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        (pPwrModel != NULL) &&
        (pCtrlParams != NULL) &&
        (pTypeParams != NULL) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
        perfCfPwrModelScaleTypeParamsInit_exit);

    status = FLCN_ERR_NOT_SUPPORTED;
    switch (BOARDOBJ_GET_TYPE(&pPwrModel->super))
    {
        case LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_TYPE_DLPPM_1X:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL_DLPPM_1X))
            {
                status = perfCfPwrModelScaleTypeParamsInit_DLPPM_1X(
                    pPwrModel,
                    pCtrlParams,
                    pTypeParams);
            }
            break;
        case LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_TYPE_TGP_1X:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL_TGP_1X))
            {
                status = perfCfPwrModelScaleTypeParamsInit_TGP_1X(
                    pPwrModel,
                    pCtrlParams,
                    pTypeParams);
            }
            break;
        default:
            // Nothing to do
            break;
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        status,
        perfCfPwrModelScaleTypeParamsInit_exit);

perfCfPwrModelScaleTypeParamsInit_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10SetHeader
 */
static FLCN_STATUS
s_perfCfPwrModelIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PERF_CF_PWR_MODEL_BOARDOBJGRP_SET_HEADER *pSet =
        (RM_PMU_PERF_CF_PWR_MODEL_BOARDOBJGRP_SET_HEADER *)pHdrDesc;
    PERF_CF_PWR_MODELS *pPwrModels = (PERF_CF_PWR_MODELS *)pBObjGrp;
    FLCN_STATUS status;
    LwLength    i;

    status = boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfCfPwrModelIfaceModel10SetHeader_exit;
    }

    // Copy global PERF_CF_PWR_MODELS state.
    ct_assert(LW_ARRAY_ELEMENTS(pPwrModels->semanticIdxMap) ==
              LW_ARRAY_ELEMENTS(pSet->semanticIdxMap));
    for (i = 0; i < LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SEMANTIC_INDEX__MAX; i++)
    {
        pPwrModels->semanticIdxMap[i] = pSet->semanticIdxMap[i];
    }

s_perfCfPwrModelIfaceModel10SetHeader_exit:
    return status;
}
/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
static FLCN_STATUS
s_perfCfPwrModelIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    FLCN_STATUS   status = FLCN_ERR_NOT_SUPPORTED;

    switch (pBuf->type)
    {
        case LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_TYPE_DLPPM_1X:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL_DLPPM_1X))
            {
                PMU_HALT_OK_OR_GOTO(status,
                    perfCfPwrModelGrpIfaceModel10ObjSetImpl_DLPPM_1X(pModel10, ppBoardObj,
                        sizeof(PERF_CF_PWR_MODEL_DLPPM_1X), pBuf),
                    s_perfCfPwrModelIfaceModel10SetEntry_exit);
            }
            else
            {
                PMU_BREAKPOINT();
                goto s_perfCfPwrModelIfaceModel10SetEntry_exit;
            }
            break;

        case LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_TYPE_TGP_1X:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL_TGP_1X))
            {
                PMU_HALT_OK_OR_GOTO(status,
                    perfCfPwrModelGrpIfaceModel10ObjSetImpl_TGP_1X(pModel10, ppBoardObj,
                        sizeof(PERF_CF_PWR_MODEL_TGP_1X), pBuf),
                    s_perfCfPwrModelIfaceModel10SetEntry_exit);
            }
            else
            {
                PMU_BREAKPOINT();
                goto s_perfCfPwrModelIfaceModel10SetEntry_exit;
            }
            break;

        default:
            PMU_BREAKPOINT();
            goto s_perfCfPwrModelIfaceModel10SetEntry_exit;
            break;
    }

s_perfCfPwrModelIfaceModel10SetEntry_exit:
    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
static FLCN_STATUS
s_perfCfPwrModelIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJ *pBuf
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pBoardObj))
    {
        case LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_TYPE_DLPPM_1X:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL_DLPPM_1X))
            {
                status = perfCfPwrModelIfaceModel10GetStatus_DLPPM_1X(pModel10, pBuf);
            }
            break;
        case LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_TYPE_TGP_1X:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL_TGP_1X))
            {
                status = perfCfPwrModelIfaceModel10GetStatus_TGP_1X(pModel10, pBuf);
            }
            break;
        default:
            status = perfCfPwrModelIfaceModel10GetStatus_SUPER(pModel10, pBuf);
            break;
    }

    PMU_ASSERT_OK_OR_GOTO(status, status, s_perfCfPwrModelIfaceModel10GetStatus_exit);

s_perfCfPwrModelIfaceModel10GetStatus_exit:
    return status;
}

/*!
 * @copydoc PerfCfPwrModelLoad
 */
static FLCN_STATUS
s_perfCfPwrModelLoad
(
    PERF_CF_PWR_MODEL *pPwrModel
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pPwrModel))
    {
        case LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_TYPE_DLPPM_1X:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL_DLPPM_1X))
            {
                PMU_HALT_OK_OR_GOTO(status,
                    perfCfPwrModelLoad_DLPPM_1X(pPwrModel),
                    s_perfCfPwrModelLoad_exit);
            }
            break;

        case LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_TYPE_TGP_1X:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PWR_MODEL_TGP_1X))
            {
                PMU_HALT_OK_OR_GOTO(status,
                    perfCfPwrModelLoad_TGP_1X(pPwrModel),
                    s_perfCfPwrModelLoad_exit);
            }
            break;

        default:
            break;
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfCfPwrModelLoad_exit;
    }

s_perfCfPwrModelLoad_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelObservedMetricsGet
 */
static LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS *
s_perfCfPwrModelObservedMetricsGet
(
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_OBSERVED *pObserved
)
{
    // Determine the correct super based on the object type.
    switch (pObserved->type)
    {
        case LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_WORKLOAD_SINGLE_1X:
            return &pObserved->data.single1x.super;
        case LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_WORKLOAD_COMBINED_1X:
            return &pObserved->data.combined1x.super;
        case LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_DLPPM_1X:
            return &pObserved->data.dlppm1x.super;
        case LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_TGP_1X:
            return &pObserved->data.tgp1x.super;
        default:
            PMU_BREAKPOINT();
            return NULL;
    }
}

/*!
 * @copydoc s_perfCfPwrModelEstimatedMetricsGet
 */
static FLCN_STATUS
s_perfCfPwrModelEstimatedMetricsGet
(
    LwU8 type,
    LwU8 numEstimatedMetrics,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_ESTIMATED *pEstimated,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS **ppEstimatedPointers
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8        i;

    for (i = 0U; i < numEstimatedMetrics; i++)
    {
        // Initialize the array based on the correct super pointer 
        switch (type)
        {
            case LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_WORKLOAD_SINGLE_1X:
                ppEstimatedPointers[i] = &pEstimated[i].data.single1x.super;
                break;
            case LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_WORKLOAD_COMBINED_1X:
                ppEstimatedPointers[i] = &pEstimated[i].data.combined1x.super;
                break;
            case LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_DLPPM_1X:
                ppEstimatedPointers[i] = &pEstimated[i].data.dlppm1x.super;
                break;
            case LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_TGP_1X:
                ppEstimatedPointers[i] = &pEstimated[i].data.tgp1x.super;
                break;
            default:
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_ARGUMENT;
                break;
        }

        if (status != FLCN_OK)
        {
            goto s_perfCfPwrModelEstimatedMetricsGet_exit;
        }

        pEstimated[i].type = type;
        ppEstimatedPointers[i]->type = type;
        ppEstimatedPointers[i]->bValid = LW_FALSE;
    }

s_perfCfPwrModelEstimatedMetricsGet_exit:
    return status;
}

/*!
 * @copydoc s_pmuRpcPerfCfPwrModelScale_ODP
 */
FLCN_STATUS
s_pmuRpcPerfCfPwrModelScale_ODP
(
    RM_PMU_RPC_STRUCT_PERF_CF_PWR_MODEL_SCALE *pParams
)
{
    FLCN_STATUS status;
    PERF_CF_PWR_MODEL *const pPwrModel =
        PERF_CF_PWR_MODEL_GET(pParams->pwrModelIdx);
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS *pObservedMetrics;
    LwU8 i;
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS_DATA_ODP typeParams; // TODO - move this to pwrModelScaleRpcScratch_ODP

    // Sanity check the inputs
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pPwrModel != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        s_pmuRpcPerfCfPwrModelScale_ODP_exit);

    // Read the observed metrics
    PMU_ASSERT_OK_OR_GOTO(status,
        SSURFACE_RD(
            &(pwrModelScaleRpcScratch_ODP.buffer.observedMetrics),
            ga100AndLater.perfCf.pwrModelScale.observedMetrics),
        s_pmuRpcPerfCfPwrModelScale_ODP_exit);

    // Read the type-specific params for this scale.
    PMU_ASSERT_OK_OR_GOTO(status,
        SSURFACE_RD(
            &(pwrModelScaleRpcScratch_ODP.buffer.typeParams),
            ga100AndLater.perfCf.pwrModelScale.typeParams),
        s_pmuRpcPerfCfPwrModelScale_ODP_exit);

    // Read the inputs to scale
    PMU_ASSERT_OK_OR_GOTO(status,
        SSURFACE_RD(
            &(pwrModelScaleRpcScratch_ODP.buffer.inputs),
            ga100AndLater.perfCf.pwrModelScale.inputs),
        s_pmuRpcPerfCfPwrModelScale_ODP_exit);

    // Build up MetricsInput
    for (i = 0; i < LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_ESTIMATED_METRICS_MAX; i++)
    {
        perfCfPwrModelScaleMeticsInputInit(&pwrModelScaleRpcScratch_ODP.metricsInput[i]);

        PMU_ASSERT_OK_OR_GOTO(status,
            perfCfPwrModelScaleMeticsInputImport(
                &pwrModelScaleRpcScratch_ODP.buffer.inputs.data.inputs[i],
                &pwrModelScaleRpcScratch_ODP.metricsInput[i]),
            s_pmuRpcPerfCfPwrModelScale_ODP_exit);
    }

    //
    // There is no "super" pointer for the union, so extract it
    // from the struct of appropriate type.
    //
    pObservedMetrics = s_perfCfPwrModelObservedMetricsGet(
        &pwrModelScaleRpcScratch_ODP.buffer.observedMetrics.data);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pObservedMetrics != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        s_pmuRpcPerfCfPwrModelScale_ODP_exit);

    // Build the array of pointers to pass to perfCfPwrModelScale
    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPwrModelEstimatedMetricsGet(
            pObservedMetrics->type,
            pParams->numEstimatedMetrics,
            pwrModelScaleRpcScratch_ODP.buffer.estimatedMetrics.data.estimatedMetrics,
            pwrModelScaleRpcScratch_ODP.estimatedPointers),
        s_pmuRpcPerfCfPwrModelScale_ODP_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfPwrModelScaleTypeParamsInit(
            pPwrModel,
            &pwrModelScaleRpcScratch_ODP.buffer.typeParams.data,
            &typeParams.super),
        s_pmuRpcPerfCfPwrModelScale_ODP_exit);

    status = perfCfPwrModelScale(
                pPwrModel,
                pObservedMetrics,
                pParams->numEstimatedMetrics,
                pwrModelScaleRpcScratch_ODP.metricsInput,
                pwrModelScaleRpcScratch_ODP.estimatedPointers,
                &typeParams.super);
    if (status == FLCN_ERR_FREQ_NOT_SUPPORTED)
    {
        // This is an RPC failure scenario we want to handle without a breakpoint.
        // TODO: once  PMU_TRUE_BP() actually works, revert this to PMU_ASSERT_OK_OR_GOTO()
        goto s_pmuRpcPerfCfPwrModelScale_ODP_exit;
    }

    // Copy out the estimated metrics and the type-specific params 
    PMU_ASSERT_OK_OR_GOTO(status,
        SSURFACE_WR(
            &(pwrModelScaleRpcScratch_ODP.buffer.estimatedMetrics),
            ga100AndLater.perfCf.pwrModelScale.estimatedMetrics),
        s_pmuRpcPerfCfPwrModelScale_ODP_exit);
    PMU_ASSERT_OK_OR_GOTO(status,
        SSURFACE_WR(
            &(pwrModelScaleRpcScratch_ODP.buffer.typeParams),
            ga100AndLater.perfCf.pwrModelScale.typeParams),
        s_pmuRpcPerfCfPwrModelScale_ODP_exit);

s_pmuRpcPerfCfPwrModelScale_ODP_exit:
    return status;
}

/*!
 * @brief      NON_ODP variant of pmuRpcPerfCfPwrModelScale.
 *
 * @note       Compared to ODP version, the metrics inputs are scaled one at a
 *             time so as to reduce resident DMEM footprint.
 *
 * @note       Only a subset of PWR_MODEL types are supported on NON_ODP
 *             profiles, as such structures encapsulating less data are used in
 *             common functions (thus a few instances of odd casting). These
 *             structures holding a subset of data are asserted at compile time
 *             to be compatabile with field offset checks.
 *
 * @param      pParams  The parameters
 *
 * @return     The flcn status.
 */
FLCN_STATUS
s_pmuRpcPerfCfPwrModelScale_NON_ODP
(
    RM_PMU_RPC_STRUCT_PERF_CF_PWR_MODEL_SCALE *pParams
)
{
    FLCN_STATUS status;
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS *pObservedMetrics;
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS *pEstimatedMetrics;
    LwU8 i;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_PERF_CF_PWR_MODEL_SCALE_RPC_NON_ODP
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        perfReadSemaphoreTake();

        PERF_CF_PWR_MODEL *const pPwrModel =
            PERF_CF_PWR_MODEL_GET(pParams->pwrModelIdx);

        // Sanity check the inputs
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pPwrModel != NULL),
            FLCN_ERR_ILWALID_ARGUMENT,
            s_pmuRpcPerfCfPwrModelScale_NON_ODP_detach);

        // 1. Copy in observed metrics to dmem
        PMU_ASSERT_OK_OR_GOTO(status,
            ssurfaceRd(
                &(pwrModelScaleRpcScratch_NON_ODP.observedMetrics),
                RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ga100AndLater.perfCf.pwrModelScale.observedMetrics.data),
                sizeof(pwrModelScaleRpcScratch_NON_ODP.observedMetrics)),
            s_pmuRpcPerfCfPwrModelScale_NON_ODP_detach);

        // 2. Extract LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS pointer (requires cast)
        pObservedMetrics = s_perfCfPwrModelObservedMetricsGet(
            (LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_OBSERVED *)
            &pwrModelScaleRpcScratch_NON_ODP.observedMetrics);
        PMU_ASSERT_TRUE_OR_GOTO(status,
            pObservedMetrics != NULL,
            FLCN_ERR_NOT_SUPPORTED,
            s_pmuRpcPerfCfPwrModelScale_NON_ODP_detach);

        // 3. Copy in type-specific params to dmem
        PMU_ASSERT_OK_OR_GOTO(status,
            ssurfaceRd(
                &(pwrModelScaleRpcScratch_NON_ODP.typeParamsBuffer),
                RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ga100AndLater.perfCf.pwrModelScale.typeParams.data),
                sizeof(pwrModelScaleRpcScratch_NON_ODP.typeParamsBuffer)),
            s_pmuRpcPerfCfPwrModelScale_NON_ODP_detach);

        // 4. Colwert type-specific params to internal type (requires cast)
        PMU_ASSERT_OK_OR_GOTO(status,
            perfCfPwrModelScaleTypeParamsInit(
                pPwrModel,
                (LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS *)
                &pwrModelScaleRpcScratch_NON_ODP.typeParamsBuffer,
                &pwrModelScaleRpcScratch_NON_ODP.typeParams.super),
            s_pmuRpcPerfCfPwrModelScale_NON_ODP_detach);

        for (i = 0; i < pParams->numEstimatedMetrics; i++)
        {
            // 5. Copy in metrics input to dmem
            PMU_ASSERT_OK_OR_GOTO(status,
                ssurfaceRd(
                    &(pwrModelScaleRpcScratch_NON_ODP.metricsInputBuffer),
                    RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ga100AndLater.perfCf.pwrModelScale.inputs.data.inputs[i]),
                    sizeof(pwrModelScaleRpcScratch_NON_ODP.metricsInputBuffer)),
                s_pmuRpcPerfCfPwrModelScale_NON_ODP_detach);

            // 6. Colwert metrics input to internal type
            perfCfPwrModelScaleMeticsInputInit(&pwrModelScaleRpcScratch_NON_ODP.metricsInput);
            PMU_ASSERT_OK_OR_GOTO(status,
                perfCfPwrModelScaleMeticsInputImport(
                    &pwrModelScaleRpcScratch_NON_ODP.metricsInputBuffer,
                    &pwrModelScaleRpcScratch_NON_ODP.metricsInput),
                s_pmuRpcPerfCfPwrModelScale_NON_ODP_detach);

            // 7. Copy in estimated metrics to dmem
            PMU_ASSERT_OK_OR_GOTO(status,
                ssurfaceRd(
                    &(pwrModelScaleRpcScratch_NON_ODP.estimatedMetrics),
                    RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ga100AndLater.perfCf.pwrModelScale.estimatedMetrics.data.estimatedMetrics[i]),
                    sizeof(pwrModelScaleRpcScratch_NON_ODP.estimatedMetrics)),
                s_pmuRpcPerfCfPwrModelScale_NON_ODP_detach);

            // 8. Get single estimated metrics pointer (requires cast)
            PMU_ASSERT_OK_OR_GOTO(status,
                s_perfCfPwrModelEstimatedMetricsGet(
                    pObservedMetrics->type,
                    1U,
                    (LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_ESTIMATED *)
                    &pwrModelScaleRpcScratch_NON_ODP.estimatedMetrics,
                    &pEstimatedMetrics),
                s_pmuRpcPerfCfPwrModelScale_NON_ODP_detach);

            // 9. Call perfCfPwrModelScale with single estimated metric and metrics input
            status = perfCfPwrModelScale(
                        pPwrModel,
                        pObservedMetrics,
                        1U,
                        &pwrModelScaleRpcScratch_NON_ODP.metricsInput,
                        &pEstimatedMetrics,
                        &pwrModelScaleRpcScratch_NON_ODP.typeParams.super);
            if (status == FLCN_ERR_FREQ_NOT_SUPPORTED)
            {
                // This is an RPC failure scenario we want to handle without a breakpoint.
                goto s_pmuRpcPerfCfPwrModelScale_NON_ODP_detach;
            }
            PMU_ASSERT_OK_OR_GOTO(status,
                status,
                s_pmuRpcPerfCfPwrModelScale_NON_ODP_detach);

            // 10. Copy out estimated metrics and type-specific params to super-surface
            PMU_ASSERT_OK_OR_GOTO(status,
                ssurfaceWr(
                    &(pwrModelScaleRpcScratch_NON_ODP.estimatedMetrics),
                    RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ga100AndLater.perfCf.pwrModelScale.estimatedMetrics.data.estimatedMetrics[i]),
                    sizeof(pwrModelScaleRpcScratch_NON_ODP.estimatedMetrics)),
                s_pmuRpcPerfCfPwrModelScale_NON_ODP_detach);
            PMU_ASSERT_OK_OR_GOTO(status,
                ssurfaceWr(
                    &(pwrModelScaleRpcScratch_NON_ODP.typeParamsBuffer),
                    RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ga100AndLater.perfCf.pwrModelScale.typeParams.data),
                    sizeof(pwrModelScaleRpcScratch_NON_ODP.typeParamsBuffer)),
                s_pmuRpcPerfCfPwrModelScale_NON_ODP_detach);
        }

s_pmuRpcPerfCfPwrModelScale_NON_ODP_detach:
        perfReadSemaphoreGive();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @copydoc s_perfCfPwrModelPropagateInputs
 */
static FLCN_STATUS
s_perfCfPwrModelPropagateInputs
(
    PERF_CF_PWR_MODEL                      *pPwrModel,
    LwLength                                numMetricsInputs,
    PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT  *pMetricsInputs
)
{
    LwLength    i;
    FLCN_STATUS status;

    // Sanity check the inputs
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pPwrModel          != NULL) &&
         (numMetricsInputs   != 0)    &&
         (pMetricsInputs     != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        s_perfCfPwrModelPropagateInputs_exit);

    for (i = 0; i < numMetricsInputs; i++)
    {
        LwBoardObjIdx           maskIdx;
        PERF_LIMITS_VF_ARRAY    vfArrayIn;
        PERF_LIMITS_VF_ARRAY    vfArrayOut;
        BOARDOBJGRPMASK_E32     tmpMask;

        boardObjGrpMaskInit_E32(&tmpMask);

        PMU_ASSERT_OK_OR_GOTO(status,
            boardObjGrpMaskCopy(
                &tmpMask,
                &pPwrModel->requiredDomainsMask),
            s_perfCfPwrModelPropagateInputs_exit);

        // Setup outputs
        PERF_LIMITS_VF_ARRAY_INIT(&vfArrayOut);
        vfArrayOut.pMask = &tmpMask;

        // Setup inputs
        PERF_LIMITS_VF_ARRAY_INIT(&vfArrayIn);

        PMU_ASSERT_OK_OR_GOTO(status,
            perfCfPwrModelScaleMeticsInputGetDomainsMaskRef(
                &pMetricsInputs[i],
                &vfArrayIn.pMask),
            s_perfCfPwrModelPropagateInputs_exit);

        // Skip if all required domains are already present
        if (boardObjGrpMaskIsSubset(vfArrayOut.pMask, vfArrayIn.pMask))
        {
            continue;
        }

        BOARDOBJGRPMASK_FOR_EACH_BEGIN(vfArrayIn.pMask, maskIdx)
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                perfCfPwrModelScaleMeticsInputGetFreqkHz(
                    &pMetricsInputs[i],
                    maskIdx,
                    &vfArrayIn.values[maskIdx]),
                s_perfCfPwrModelPropagateInputs_exit);

            // Don't need to propagate to output if already specified.
            boardObjGrpMaskBitClr(vfArrayOut.pMask, maskIdx);
        }
        BOARDOBJGRPMASK_FOR_EACH_END;

        // Propagate
        PMU_ASSERT_OK_OR_GOTO(status,
            perfLimitsFreqPropagate(
                NULL,
                &vfArrayIn,
                &vfArrayOut,
                LW2080_CTRL_CLK_CLK_PROP_TOP_IDX_ILWALID),
            s_perfCfPwrModelPropagateInputs_exit);

        // Fill in the metrics input with propagated frequencies
        BOARDOBJGRPMASK_FOR_EACH_BEGIN(vfArrayOut.pMask, maskIdx)
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                perfCfPwrModelScaleMeticsInputSetFreqkHz(
                    &pMetricsInputs[i],
                    maskIdx,
                    vfArrayOut.values[maskIdx]),
                s_perfCfPwrModelPropagateInputs_exit);
        }
        BOARDOBJGRPMASK_FOR_EACH_END;
    }

s_perfCfPwrModelPropagateInputs_exit:
    return status;
}

/*!
 * @copydoc s_perfCfPwrModelQuantizeInputs
 */
static FLCN_STATUS
s_perfCfPwrModelQuantizeInputs
(
    PERF_CF_PWR_MODEL                      *pPwrModel,
    LwLength                                numMetricsInputs,
    PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT  *pMetricsInputs
)
{
    LwLength    i;
    FLCN_STATUS status;

    // Sanity check the inputs
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pPwrModel          != NULL) &&
         (numMetricsInputs   != 0)    &&
         (pMetricsInputs     != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        s_perfCfPwrModelQuantizeInputs_exit);

    for (i = 0; i < numMetricsInputs; i++)
    {
        BOARDOBJGRPMASK_E32    *pMask;
        LwBoardObjIdx           maskIdx;

        PMU_ASSERT_OK_OR_GOTO(status,
            perfCfPwrModelScaleMeticsInputGetDomainsMaskRef(
                &pMetricsInputs[i],
                &pMask),
            s_perfCfPwrModelQuantizeInputs_exit);

        BOARDOBJGRPMASK_FOR_EACH_BEGIN(pMask, maskIdx)
        {
            LwU32           freqkHz;
            PERF_LIMITS_VF  vfInput;
            const CLK_DOMAIN *const pDomain =
                CLK_DOMAIN_GET(maskIdx);
            LwU32 freqkHzScale;
            LwU32 freqkHzLimit;

            PMU_ASSERT_TRUE_OR_GOTO(status,
                (pDomain != NULL),
                FLCN_ERR_ILWALID_ARGUMENT,
                s_perfCfPwrModelQuantizeInputs_exit);

            // Read
            PMU_ASSERT_OK_OR_GOTO(status,
                perfCfPwrModelScaleMeticsInputGetFreqkHz(
                    &pMetricsInputs[i],
                    maskIdx,
                    &freqkHz),
                s_perfCfPwrModelQuantizeInputs_exit);

            vfInput.idx = BOARDOBJ_GRP_IDX_TO_8BIT(maskIdx);
            vfInput.value = freqkHz;

            // Quantize UP
            PMU_ASSERT_OK_OR_GOTO(status,
                perfLimitsFreqkHzQuantize(
                    &vfInput, NULL, LW_FALSE),
                s_perfCfPwrModelQuantizeInputs_exit);

            //
            // Check to see if the provided frequency was outside the valid VF
            // range, which happens if we try to quantize up but get a lower
            // frequency.
            //
            // Note that quantization rounds to the closest MHz before
            // quantizing, so we can't do a strict less-than check, in case
            // freqkHz is rounded down before the quantization.
            //
            // Instead, we can make sure that the input is not more than 1 MHz
            // away from the quantized frequency.
            //
            // Also note that this has to be different if this is a CLK_DOMAIN
            // that does not operate in kHz (i.e., the scale is 1). In that
            // case, we do the strict linearity check.
            //
            freqkHzScale = clkDomainFreqkHzScaleGet(pDomain);
            freqkHzLimit = freqkHzScale == 1U ?
                vfInput.value :
                vfInput.value + freqkHzScale;
            if (freqkHzLimit < freqkHz)
            {
                // TODO: once  PMU_TRUE_BP() actually works, add one here
                status = FLCN_ERR_FREQ_NOT_SUPPORTED;
                goto s_perfCfPwrModelQuantizeInputs_exit;
            }

            // Write
            PMU_ASSERT_OK_OR_GOTO(status,
                perfCfPwrModelScaleMeticsInputSetFreqkHz(
                    &pMetricsInputs[i],
                    maskIdx,
                    vfInput.value),
                s_perfCfPwrModelQuantizeInputs_exit);
        }
        BOARDOBJGRPMASK_FOR_EACH_END;
    }

s_perfCfPwrModelQuantizeInputs_exit:
    return status;
}
