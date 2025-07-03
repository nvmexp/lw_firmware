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
 * @file   pwrpolicy_workload_combined_1x.c
 * @brief  Specification for a PWR_POLICY_WORKLOAD_COMBINED_1X.
 */
/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objpmgr.h"
#include "pmgr/pwrpolicy_workload_shared.h"
#include "pmgr/pwrpolicy_workload_combined_1x.h"
#include "pmu_objpg.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

/*!
 * Helper defines to compact the names of regimes
 */
#define REGIME(x) LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_SEARCH_REGIME_ID_##x
#define SINGLE1X(x) LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_SINGLE_1X_IDX_##x
#define HAS_SINGLE1X(pCombined1X, x) (((pCombined1x)->singleRelIdxLast - (pCombined1x)->singleRelIdxFirst) >= SINGLE1X(x))
#define SEARCH_REGIME_FREQ_TUPLE_IDX(x) LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_SEARCH_REGIME_TUPLE_IDX_##x

/* ------------------------- Private Functions ------------------------------ */
static FLCN_STATUS s_pwrPolicyWorkloadCombined1xComputeClkMHz(PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x, PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimits)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyWorkloadCombined1xComputeClkMHz");
static FLCN_STATUS s_pwrPolicyWorkloadCombined1xScaleClocks(PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x, PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimits)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyWorkloadCombined1xScaleClocks");
static BoardObjVirtualTableGetInterfaceFromBoardObj (s_pwrPolicyGetInterfaceFromBoardObj_WORKLOAD_COMBINED_1X)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyPerfCfPwrModel", "s_pwrPolicyGetInterfaceFromBoardObj_WORKLOAD_COMBINED_1X");
static FLCN_STATUS s_pwrPolicyWorkloadCombined1xFreqFloorStateGet(PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x, PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1x, PERF_LIMITS_PSTATE_RANGE *pPstateRange, LwBoardObjIdx singleIdx, LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_ESTIMATED_METRICS_WORKLOAD_SINGLE_1X *pEstFloorMetrics)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyWorkloadCombined1xFreqFloorStateGet");
static FLCN_STATUS s_pwrPolicyWorkloadCombined1xResidencyGet(PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyPerfCfPwrModel", "s_pwrPolicyWorkloadCombined1xResidencyGet");
static FLCN_STATUS s_pwrPolicyWorkloadCombined1xIsLimitSatisfiedAtFreq(PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x, LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_SINGLES_FREQ_TUPLE  *pRegimeFreq, LwBool *pbLimitSatisfied)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyWorkloadCombined1xIsLimitSatisfiedAtFreq");
static FLCN_STATUS s_pwrPolicyWorkloadCombined1xClkPropagate(LwU32 propFromFreqMHz, LwBoardObjIdx propFromClkIdx, LwBoardObjIdx propToClkIdx, LwBoardObjIdx clkPropTopIdx, LwU32 *pPropToFreqMHz)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyWorkloadCombined1xClkPropagate");
static FLCN_STATUS s_pwrPolicyWorkloadCombined1xRegimeSpaceBuild(PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyWorkloadCombined1xRegimeSpaceBuild");
static FLCN_STATUS s_pwrPolicyWorkloadCombined1xInitRegimeData(PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyWorkloadCombined1xInitRegimeData");
static FLCN_STATUS s_pwrPolicyWorkloadCombined1xInitRegimeDataByIdx(PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x, LwBoardObjIdx idx, PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_INIT_DATA *pRegimeData)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyWorkloadCombined1xInitRegimeDataByIdx");
static FLCN_STATUS s_pwrPolicyWorkloadCombined1xGetIndexByFreqMHz(LwBoardObjIdx clkDomainIdx, LwU32 freqMhz, LwBool bFloor, LwU16 *pClkIdx)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyWorkloadCombined1xGetIndexByFreqMHz");
static FLCN_STATUS s_pwrPolicyWorkloadCombined1xSearchTupleIdxBuild(PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyWorkloadCombined1xSearchTupleIdxBuild");
static FLCN_STATUS s_pwrPolicyWorkloadCombined1xComputeRegimeClkMHz(PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x, PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimits)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyWorkloadCombined1xComputeRegimeClkMHz");
static FLCN_STATUS s_pwrPolicyWorkloadCombined1xGetFreqByTupleIdx(PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x, LwU16 tupleIdx, LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_SINGLES_FREQ_TUPLE  *pRegimeFreq)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyWorkloadCombined1xGetFreqByTupleIdx");
static FLCN_STATUS s_pwrPolicyWorkloadCombined1xSearchRegimeGetFreqTupleByTupleIdx(PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x, LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_SEARCH_REGIME *pSearchRegime, LwU16 tupleIdx, LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_SINGLES_FREQ_TUPLE *pRegimeFreq)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyWorkloadCombined1xSearchRegimeGetFreqTupleByTupleIdx");
static FLCN_STATUS s_pwrPolicyWorkloadCombined1xGetTupleIdxByPrimaryFreq(PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x, LwU32 freqMHz, LwBool bMax, LwU16 *pTupleIdx)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyWorkloadCombined1xGetTupleIdxByPrimaryFreq");
static FLCN_STATUS s_pwrPolicyWorkloadCombined1xGetSearchRegimeIdxByTupleIdx(PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x, LwU16 tupleIdx, LwU8 *pSearchRegimeIdx)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyWorkloadCombined1xGetSearchRegimeIdxByTupleIdx");
static FLCN_STATUS s_pwrPolicyWorkloadCombined1xGetClkDomainIdx(PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x, LwU8 singleIdx, LwBoardObjIdx *pClkDomainIdx)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyWorkloadCombined1xGetClkDomainIdx");
static FLCN_STATUS s_pwrPolicyWorkloadCombined1xInitChildRegimeMask(PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x, LwBoardObjIdx  childRegimeId, BOARDOBJGRPMASK_E32 *pChildRegimeMask)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyWorkloadCombined1xInitChildRegimeMask");
static FLCN_STATUS s_pwrPolicyWorkloadCombined1xRegimeFreqEndPoints(PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x, PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_DATA  *pParentRegime, PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_DATA *pRegime)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyWorkloadCombined1xRegimeFreqEndPoints");
static FLCN_STATUS s_pwrPolicyWorkloadCombined1xRegimeSpaceIlwalidate(PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyWorkloadCombined1xRegimeSpaceIlwalidate");
static FLCN_STATUS s_pwrPolicyWorkloadCombined1xRegimesEnabled(PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x, LwBool *pbRegimesEnabled)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyWorkloadCombined1xRegimesEnabled");

/* ------------------------- Global Variables ------------------------------- */
/*!
 * Main structure for all PWR_POLICY_WORKLOAD_COMBINED_1X VIRTUAL_TABLE data.
 */
BOARDOBJ_VIRTUAL_TABLE pwrPolicyWorkloadCombined1xVirtualTable =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_GET_INTERFACE_FROM_BOARDOBJ(
        s_pwrPolicyGetInterfaceFromBoardObj_WORKLOAD_COMBINED_1X)
};

/*!
 * Main structure for all PWR_POLICY_WORKLOAD_COMBINED_1X VIRTUAL_TABLE data.
 */
INTERFACE_VIRTUAL_TABLE pwrPolicyVirtualTable_WORKLOAD_COMBINED_1X =
{
    LW_OFFSETOF(PWR_POLICY_WORKLOAD_COMBINED_1X, pwrModel)   // offset
};

/*!
 * Scratch buffer to temporarily store the estimated metrics for floor freq of
 * secondary clocks
 */
LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_ESTIMATED_METRICS_WORKLOAD_SINGLE_1X estFloorMetrics
    GCC_ATTRIB_SECTION("dmem_pmgr", "estFloorMetrics");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Construct a _WORKLOAD_COMBINED_1X PWR_POLICY object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_COMBINED_1X
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_BOARDOBJ_SET  *pCombined1xSet =
        (RM_PMU_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_BOARDOBJ_SET *)pBoardObjDesc;
    PWR_POLICY_WORKLOAD_COMBINED_1X                           *pCombined1x;
    BOARDOBJ_VTABLE                                           *pBoardObjVtable;
    FLCN_STATUS                                                status;
    LwU8                                                       numSingles;

    // Construct and initialize parent-class object.
    PMU_ASSERT_OK_OR_GOTO(status,
        pwrPolicyGrpIfaceModel10ObjSetImpl_DOMGRP(pModel10,
                    ppBoardObj, size, pBoardObjDesc),
        pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_COMBINED_1X_exit);

    pCombined1x     = (PWR_POLICY_WORKLOAD_COMBINED_1X *)*ppBoardObj;
    pBoardObjVtable = &pCombined1x->super.super.super;

    // Construct PWR_POLICY_PERF_CF_PWR_MODEL interface class
    PMU_ASSERT_OK_OR_GOTO(status,
        pwrPolicyConstructImpl_PWR_POLICY_PERF_CF_PWR_MODEL(pBObjGrp,
                &pCombined1x->pwrModel.super,
                &pCombined1xSet->pwrModel.super,
                &pwrPolicyVirtualTable_WORKLOAD_COMBINED_1X),
        pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_COMBINED_1X_exit);

    // Input filtering is not supported
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (!PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_INPUT_FILTERING) ||
         (pCombined1x->super.super.filter.type ==
            LW2080_CTRL_PMGR_PWR_POLICY_3X_FILTER_TYPE_NONE)),
        FLCN_ERR_ILWALID_STATE,
        pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_COMBINED_1X_exit);

    numSingles = pCombined1xSet->singleRelIdxLast -
        pCombined1xSet->singleRelIdxFirst + 1;

    // Sanity check that the number of SINGLE_1X policies is within bounds
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (numSingles <= LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_SINGLE_1X_MAX),
        FLCN_ERR_ILWALID_STATE,
        pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_COMBINED_1X_exit);

    // Allocate memory for SINGLE_1X observed metrics
    if (pCombined1x->obsMetrics.pSingles == NULL)
    {
        pCombined1x->obsMetrics.pSingles = lwosCallocType(
            pBObjGrp->ovlIdxDmem, numSingles,
            LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_OBSERVED_METRICS_WORKLOAD_SINGLE_1X);
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pCombined1x->obsMetrics.pSingles != NULL),
            FLCN_ERR_NO_FREE_MEM,
            pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_COMBINED_1X_exit);
    }
    else
    {
        // Sanity check that numSingles hasn't changed
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (numSingles ==
             (pCombined1x->singleRelIdxLast - pCombined1x->singleRelIdxFirst + 1)),
            FLCN_ERR_ILWALID_STATE,
            pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_COMBINED_1X_exit);
    }

    // Allocate memory for SINGLE_1X estimated metrics
    if (pCombined1x->estMetrics.pSingles == NULL)
    {
        pCombined1x->estMetrics.pSingles = lwosCallocType(
            pBObjGrp->ovlIdxDmem, numSingles,
            LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_ESTIMATED_METRICS_WORKLOAD_SINGLE_1X);
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pCombined1x->estMetrics.pSingles != NULL),
            FLCN_ERR_NO_FREE_MEM,
            pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_COMBINED_1X_exit);
    }
    else
    {
        // Sanity check that numSingles hasn't changed
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (numSingles ==
                (pCombined1x->singleRelIdxLast - pCombined1x->singleRelIdxFirst + 1)),
            FLCN_ERR_ILWALID_STATE,
            pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_COMBINED_1X_exit);
    }

    // Copy in the _WORKLOAD_COMBINED_1X type-specific data.
    pCombined1x->clkPrimaryUpScale   = pCombined1xSet->clkPrimaryUpScale;
    pCombined1x->clkPrimaryDownScale = pCombined1xSet->clkPrimaryDownScale;
    pCombined1x->singleRelIdxFirst  = pCombined1xSet->singleRelIdxFirst;
    pCombined1x->singleRelIdxLast   = pCombined1xSet->singleRelIdxLast;
    pCombined1x->capMultiplier      = pCombined1xSet->capMultiplier;

    // Override the Vtable pointer.
    pBoardObjVtable->pVirtualTable = &pwrPolicyWorkloadCombined1xVirtualTable;

pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_COMBINED_1X_exit:
    return status;
}

/*!
 * Load _WORKLOAD_COMBINED_1X PWR_POLICY object.
 *
 * @copydoc PwrPolicyLoad
 */
FLCN_STATUS
pwrPolicyLoad_WORKLOAD_COMBINED_1X
(
    PWR_POLICY *pPolicy,
    LwU32       ticksNow
)
{
    PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x =
        (PWR_POLICY_WORKLOAD_COMBINED_1X *)pPolicy;
    FLCN_STATUS                    status;
    LwU8                           i;
    PERF_CF_PWR_MODEL_SAMPLE_DATA  sampleData;
    PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1x;

    // Call super class implementation
    PMU_ASSERT_OK_OR_GOTO(status,
        pwrPolicyLoad_SUPER(pPolicy, ticksNow),
        pwrPolicyLoad_WORKLOAD_COMBINED_1X_exit);

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X_LWDCLK_CTRL))
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrPolicyWorkloadCombined1xInitRegimeData(pCombined1x),
            pwrPolicyLoad_WORKLOAD_COMBINED_1X_exit);
        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrPolicyWorkloadCombined1xRegimeSpaceIlwalidate(pCombined1x),
            pwrPolicyLoad_WORKLOAD_COMBINED_1X_exit);
        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrPolicyWorkloadCombined1xInitRegimeDataByIdx(pCombined1x,
            REGIME(DUMMY_ROOT), &pCombined1x->rootRegimeData.regimeInitData),
            pwrPolicyLoad_WORKLOAD_COMBINED_1X_exit);
    }

    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_BEGIN(
        pCombined1x, pSingle1x, i, status,
        pwrPolicyLoad_WORKLOAD_COMBINED_1X_exit)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            pwrPolicyMetricsInit_WORKLOAD_SINGLE_1X(pSingle1x,
                &pCombined1x->obsMetrics.pSingles[i].super),
            pwrPolicyLoad_WORKLOAD_COMBINED_1X_exit);
    }
    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_END;

    // Initialise @ref domGrpLimitsUnQuant
    pwrPolicyDomGrpLimitsInit(&pCombined1x->domGrpLimitsUnQuant);

    sampleData.pPerfTopoStatus          = NULL;
    sampleData.pPmSensorsStatus         = NULL;
    sampleData.pPwrChannelsStatus       = NULL;
    sampleData.pPerfCfControllersStatus = NULL;

    // Burn the first call to observeMetrics
    PMU_ASSERT_OK_OR_GOTO(status,
        pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_COMBINED_1X(
            &pCombined1x->pwrModel, &sampleData, &pCombined1x->obsMetrics.super),
        pwrPolicyLoad_WORKLOAD_COMBINED_1X_exit);

pwrPolicyLoad_WORKLOAD_COMBINED_1X_exit:
    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
pwrPolicyIfaceModel10GetStatus_WORKLOAD_COMBINED_1X
(
    BOARDOBJ_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJ *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_BOARDOBJ_GET_STATUS *pGetStatus =
        (RM_PMU_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_BOARDOBJ_GET_STATUS *)pPayload;
    PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x =
        (PWR_POLICY_WORKLOAD_COMBINED_1X *)pBoardObj;
    FLCN_STATUS                      status;
    LwBoardObjIdx                    i;

    // Call _DOMGRP super-class implementation.
    PMU_ASSERT_OK_OR_GOTO(status,
        pwrPolicyIfaceModel10GetStatus_DOMGRP(pModel10, pPayload),
        pwrPolicyIfaceModel10GetStatus_WORKLOAD_COMBINED_1X_exit);

    // Copy out the state to the RM.
    pGetStatus->numStepsBack        = pCombined1x->numStepsBack;
    pGetStatus->obsMet.super        = pCombined1x->obsMetrics.super;
    pGetStatus->obsMet.observedVal  = pCombined1x->obsMetrics.observedVal;
    pGetStatus->estMet.super        = pCombined1x->estMetrics.super;
    pGetStatus->estMet.estimatedVal = pCombined1x->estMetrics.estimatedVal;
    pGetStatus->numRegimes          = pCombined1x->numRegimes;
    pGetStatus->regimesStatus       = pCombined1x->regimesStatus;

    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_INDEX_BEGIN(pCombined1x, i)
    {
        pGetStatus->obsMet.singles[i] = pCombined1x->obsMetrics.pSingles[i];
        pGetStatus->estMet.singles[i] = pCombined1x->estMetrics.pSingles[i];
    }
    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_INDEX_END;

    for (i = 0; i < LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_SEARCH_REGIME_ID_MAX; i++)
    {
        pGetStatus->searchRegimes[i] = pCombined1x->searchRegimes[i];
    }

pwrPolicyIfaceModel10GetStatus_WORKLOAD_COMBINED_1X_exit:
    return status;
}

/*!
 * @copydoc PwrPolicy3XChannelMaskGet
 */
LwU32
pwrPolicy3XChannelMaskGet_WORKLOAD_COMBINED_1X
(
    PWR_POLICY *pPolicy
)
{
    PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x =
        (PWR_POLICY_WORKLOAD_COMBINED_1X *)pPolicy;
    LwU32                            channelMask = 0;
    LwBoardObjIdx                    i;
    PWR_POLICY_WORKLOAD_SINGLE_1X   *pSingle1x;
    FLCN_STATUS                      status;

    // Create the channelMask with chIdx of each SINGLE_1X.
    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_BEGIN(
        pCombined1x, pSingle1x, i, status,
        pwrPolicy3XChannelMaskGet_WORKLOAD_COMBINED_1X_exit)
    {
        channelMask |=
            pwrPolicy3XChannelMaskGet_WORKLOAD_SINGLE_1X(&pSingle1x->super);
    }
    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_END;

pwrPolicy3XChannelMaskGet_WORKLOAD_COMBINED_1X_exit:
    return channelMask;
}

/*!
 * WORKLOAD_COMBINED_1X-implementation of filtering.  Applies filter
 * on the workload term and stores the information related to the filtered out.
 *
 * @copydoc PwrPolicy3XFilter
 */
FLCN_STATUS
pwrPolicy3XFilter_WORKLOAD_COMBINED_1X
(
    PWR_POLICY                        *pPolicy,
    PWR_MONITOR                       *pMon,
    PWR_POLICY_3X_FILTER_PAYLOAD_TYPE *pPayload
)
{
    PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x =
        (PWR_POLICY_WORKLOAD_COMBINED_1X *)pPolicy;
    PERF_CF_PWR_MODEL_SAMPLE_DATA    sampleData;
    FLCN_STATUS                      status;

    OSTASK_OVL_DESC ovlDescList[] = {
        PWR_POLICY_PERF_CF_PWR_MODEL_OVERLAYS
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pmgrLibPwrPolicyClient)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pmgrLibPwrPolicyWorkloadMultirail)
    };

    sampleData.pPerfTopoStatus          = NULL;
    sampleData.pPmSensorsStatus         = NULL;
    sampleData.pPwrChannelsStatus       = &pPayload->channelsStatus;
    sampleData.pPerfCfControllersStatus =
        pwrPolicy35FilterPayloadPerfCfControllersStatusGet(pPayload);

    // Optimization to save IMEM (see overlay combinations Tasks.pm)
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_COMBINED_1X(
                    &pCombined1x->pwrModel,
                    &sampleData,
                    &pCombined1x->obsMetrics.super);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    PMU_ASSERT_OK_OR_GOTO(
        status,
        status,
        pwrPolicy3XFilter_WORKLOAD_COMBINED_1X_exit);

    // Copy out observedVal into the policy valueLwrr
    pCombined1x->super.super.valueLwrr = pCombined1x->obsMetrics.observedVal;

pwrPolicy3XFilter_WORKLOAD_COMBINED_1X_exit:
    return status;
}

/*!
 * WORKLOAD_COMBINED_1X-implementation to evaluate the power limit and callwlate
 * the corresponding domain group limits which will provide power <= the limit.
 *
 * @pre @ref pwrPolicyFilter_WORKLOAD_COMBINED_1X() must be called every iteration
 * to callwlate and filter the workload term before calling this function.
 *
 * @copydoc PwrPolicyDomGrpEvaluate
 */
FLCN_STATUS
pwrPolicyDomGrpEvaluate_WORKLOAD_COMBINED_1X
(
    PWR_POLICY_DOMGRP *pDomGrp
)
{
    PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x =
        (PWR_POLICY_WORKLOAD_COMBINED_1X *)pDomGrp;
    PERF_DOMAIN_GROUP_LIMITS         domGrpLimits = {{ 0 }};
    FLCN_STATUS                      status;
    LwBool                           bRegimesEnabled;


    //
    // 1. Callwlate clocks for workload and power limit.
    //
    // Use stack variable domGrpLimits because we depend on current values in
    // PWR_POLICY_DOMGRP::domGrpLimits for ramp-rate control in
    // s_pwrPolicyWorkloadCombined1xScaleClocks() below.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        s_pwrPolicyWorkloadCombined1xRegimesEnabled(pCombined1x, &bRegimesEnabled),
        pwrPolicyDomGrpEvaluate_WORKLOAD_COMBINED_1X_exit);
    if (bRegimesEnabled)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrPolicyWorkloadCombined1xRegimeSpaceBuild(pCombined1x),
            pwrPolicyDomGrpEvaluate_WORKLOAD_COMBINED_1X_exit);

        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrPolicyWorkloadCombined1xComputeRegimeClkMHz(pCombined1x,
                &domGrpLimits),
            pwrPolicyDomGrpEvaluate_WORKLOAD_COMBINED_1X_exit);
    }
    else
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrPolicyWorkloadCombined1xComputeClkMHz(pCombined1x,
                &domGrpLimits),
            pwrPolicyDomGrpEvaluate_WORKLOAD_COMBINED_1X_exit);
    }

    // 2. Scale clocks for ramp-rate control
    PMU_ASSERT_OK_OR_GOTO(status,
        s_pwrPolicyWorkloadCombined1xScaleClocks(pCombined1x,
            &domGrpLimits),
        pwrPolicyDomGrpEvaluate_WORKLOAD_COMBINED_1X_exit);

    // 3. Update/cache the domain group limits for this policy.
    pCombined1x->super.domGrpLimits = domGrpLimits;

pwrPolicyDomGrpEvaluate_WORKLOAD_COMBINED_1X_exit:
    return status;
}

/*!
 * @copydoc PwrPolicyDomGrpIsCapped
 */
LwBool
pwrPolicyDomGrpIsCapped_WORKLOAD_COMBINED_1X
(
    PWR_POLICY_DOMGRP *pDomGrp
)
{
    PWR_POLICY_WORKLOAD_COMBINED_1X    *pCombined1x =
        (PWR_POLICY_WORKLOAD_COMBINED_1X *)pDomGrp;
    LwBoardObjIdx                       i;
    LwUFXP20_12                         temp;
    PWR_POLICY_WORKLOAD_SINGLE_1X      *pSingle1x;
    FLCN_STATUS                         status;

    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_BEGIN(
        pCombined1x, pSingle1x, i, status,
        pwrPolicyDomGrpIsCapped_WORKLOAD_COMBINED_1X_exit)
    {
        CLK_DOMAIN *pDomain;
        RM_PMU_PMGR_PWR_POLICY_DOMGRP_MAP_ENTRY domGrpData;

        pDomain = CLK_DOMAIN_GET(pSingle1x->clkDomainIdx);
        PMU_ASSERT_TRUE_OR_GOTO(status,
            pDomain != NULL,
            FLCN_ERR_ILWALID_INDEX,
            pwrPolicyDomGrpIsCapped_WORKLOAD_COMBINED_1X_exit);

        // Get the domain group data from clkDomainIdx
        PMU_ASSERT_OK_OR_GOTO(status,
            pwrPolicies35DomGrpDataGet(
                PWR_POLICIES_35_GET(),
                &domGrpData,
                pSingle1x->clkDomainIdx),
            pwrPolicyDomGrpIsCapped_WORKLOAD_COMBINED_1X_exit);

        // 4.12 * 32 = 20.12
        temp = pCombined1x->capMultiplier * pCombined1x->obsMetrics.pSingles[i].freqMHz;
        if ((LW_TYPES_UFXP_X_Y_TO_U32_ROUNDED(20, 12, temp) *
            clkDomainFreqkHzScaleGet(pDomain)) >=
            pCombined1x->super.domGrpLimits.values[domGrpData.domGrpLimitIdx])
        {
            return LW_TRUE;
        }
    }
    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_END;

pwrPolicyDomGrpIsCapped_WORKLOAD_COMBINED_1X_exit:
    return  LW_FALSE;
}

/*!
 * @copydoc PwrPolicyPerfCfPwrModelObserveMetrics
 */
FLCN_STATUS
pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_COMBINED_1X
(
    PWR_POLICY_PERF_CF_PWR_MODEL               *pPwrModel,
    PERF_CF_PWR_MODEL_SAMPLE_DATA              *pSampleData,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS *pObservedMetrics
)
{
    PWR_POLICY                      *pPolicy =
        (PWR_POLICY *)INTERFACE_TO_BOARDOBJ_CAST(pPwrModel);
    PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x =
        (PWR_POLICY_WORKLOAD_COMBINED_1X *)pPolicy;
    PWR_POLICY_WORKLOAD_COMBINED_1X_OBSERVED_METRICS *pObserved =
        (PWR_POLICY_WORKLOAD_COMBINED_1X_OBSERVED_METRICS *)pObservedMetrics;
    FLCN_STATUS                      status;
    LwU8                             i;
    PWR_POLICY_WORKLOAD_SINGLE_1X   *pSingle1x;

    pObserved->observedVal = 0;

    // Get lpwr feature residency
    PMU_ASSERT_OK_OR_GOTO(status,
        s_pwrPolicyWorkloadCombined1xResidencyGet(pCombined1x),
        pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_COMBINED_1X_exit);

    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_BEGIN(
        pCombined1x, pSingle1x, i, status,
        pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_COMBINED_1X_exit)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_SINGLE_1X(
                &pSingle1x->pwrModel,
                pSampleData,
                &pObserved->pSingles[i].super),
            pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_COMBINED_1X_exit);

        pObserved->observedVal += pObserved->pSingles[i].observedVal;
    }
    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_END;

pwrPolicyPerfCfPwrModelObserveMetrics_WORKLOAD_COMBINED_1X_exit:
    return status;
}

/*!
 * @copydoc     PwrPolicyPerfCfPwrModelScaleMetrics
 *
 * @deprecated  Use pwrPolicyPerfCfPwrModelScale_WORKLOAD_COMBINED_1X going
 *              forward, it better conforms to the PERF_CF_PWR_MODEL interface
 *              of the same name.
 */
FLCN_STATUS
pwrPolicyPerfCfPwrModelScaleMetrics_WORKLOAD_COMBINED_1X
(
    PWR_POLICY_PERF_CF_PWR_MODEL               *pPwrModel,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS *pObservedMetrics,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS *pEstimatedMetrics
)
{
    PWR_POLICY                         *pPolicy =
        (PWR_POLICY *)INTERFACE_TO_BOARDOBJ_CAST(pPwrModel);
    PWR_POLICY_WORKLOAD_COMBINED_1X    *pCombined1x =
        (PWR_POLICY_WORKLOAD_COMBINED_1X *)pPolicy;
    PWR_POLICY_WORKLOAD_COMBINED_1X_OBSERVED_METRICS *pObserved =
        (PWR_POLICY_WORKLOAD_COMBINED_1X_OBSERVED_METRICS *)pObservedMetrics;
    PWR_POLICY_WORKLOAD_COMBINED_1X_ESTIMATED_METRICS *pEstimated =
        (PWR_POLICY_WORKLOAD_COMBINED_1X_ESTIMATED_METRICS *)pEstimatedMetrics;
    FLCN_STATUS                         status = FLCN_ERR_ILWALID_STATE;
    LwU8                                i;
    PWR_POLICY_WORKLOAD_SINGLE_1X      *pSingle1x;

    pEstimated->estimatedVal = 0;

    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_BEGIN(
        pCombined1x, pSingle1x, i, status,
        pwrPolicyPerfCfPwrModelScaleMetrics_WORKLOAD_COMBINED_1X_exit)
    {
        //
        // Temporary step needed until pwrPolicyPerfCfPwrModelScaleMetrics_WORKLOAD_SINGLE_1X
        // is replaced with pwrPolicyPerfCfPwrModelScale_WORKLOAD_SINGLE_1X
        //
        pEstimated->pSingles[i].independentDomainsVmin = pObserved->pSingles[i].independentDomainsVmin;

        PMU_ASSERT_OK_OR_GOTO(status,
            pwrPolicyPerfCfPwrModelScaleMetrics_WORKLOAD_SINGLE_1X(
                &pSingle1x->pwrModel, &pObserved->pSingles[i].super,
                &pEstimated->pSingles[i].super),
            pwrPolicyPerfCfPwrModelScaleMetrics_WORKLOAD_COMBINED_1X_exit);

        pEstimated->estimatedVal += pEstimated->pSingles[i].estimatedVal;
    }
    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_END;

pwrPolicyPerfCfPwrModelScaleMetrics_WORKLOAD_COMBINED_1X_exit:
    return status;
}

/*!
 * @copydoc PwrPolicyPerfCfPwrModelScale
 */
FLCN_STATUS
pwrPolicyPerfCfPwrModelScale_WORKLOAD_COMBINED_1X
(
    PWR_POLICY_PERF_CF_PWR_MODEL                   *pPwrModel,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS     *pObservedMetrics,
    LwLength                                        numMetricsInputs,
    PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT          *pMetricsInputs,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS    **ppEstimatedMetrics,
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS            *pTypeParams
)
{
    PWR_POLICY_WORKLOAD_COMBINED_1X    *pCombined1x =
        (PWR_POLICY_WORKLOAD_COMBINED_1X *)INTERFACE_TO_BOARDOBJ_CAST(pPwrModel);
    LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_OBSERVED_METRICS_WORKLOAD_COMBINED_1X
                                       *pObserved =
        (LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_OBSERVED_METRICS_WORKLOAD_COMBINED_1X *)pObservedMetrics;
    LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_ESTIMATED_METRICS_WORKLOAD_COMBINED_1X
                                       *pEstimated;
    LwLength                            loopIdx;
    FLCN_STATUS                         status   = FLCN_OK;
    LwU8                                i;
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS
                                       *localEstimatedMetrics[LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_ESTIMATED_METRICS_MAX];
    PWR_POLICY_WORKLOAD_SINGLE_1X      *pSingle1x;

    // Sanity checking.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pPwrModel          != NULL) &&
         (pObservedMetrics   != NULL) &&
         (pMetricsInputs     != NULL) &&
         (ppEstimatedMetrics != NULL) &&
         (pTypeParams            != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        pwrPolicyPerfCfPwrModelScale_WORKLOAD_COMBINED_1X_exit);

    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((numMetricsInputs != 0U) &&
         (numMetricsInputs <= LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_ESTIMATED_METRICS_MAX)),
        FLCN_ERR_ILWALID_ARGUMENT,
        pwrPolicyPerfCfPwrModelScale_WORKLOAD_COMBINED_1X_exit);

    // Ensure metrics type is correct
    for (loopIdx = 0; loopIdx < numMetricsInputs; loopIdx++)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            ppEstimatedMetrics[loopIdx]->type == LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_WORKLOAD_COMBINED_1X ?
                FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
            pwrPolicyPerfCfPwrModelScale_WORKLOAD_COMBINED_1X_exit);
    }

    // Populate estimated metrics for each of the singles
    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_BEGIN(
        pCombined1x, pSingle1x, i, status,
        pwrPolicyPerfCfPwrModelScale_WORKLOAD_COMBINED_1X_exit)
    {
        // Setup the array of estimated metrics pointers and set type
        for (loopIdx = 0; loopIdx < numMetricsInputs; loopIdx++)
        {
            pEstimated = (LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_ESTIMATED_METRICS_WORKLOAD_COMBINED_1X *)ppEstimatedMetrics[loopIdx];
            localEstimatedMetrics[loopIdx] = &pEstimated->singles[i].super;
            localEstimatedMetrics[loopIdx]->type = LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TYPE_WORKLOAD_SINGLE_1X;
        }

        // Note: pTypeParams not lwrrently used, should be translated to a SINGLE_1X bounds type if ever needed
        PMU_ASSERT_OK_OR_GOTO(status,
            pwrPolicyPerfCfPwrModelScale_WORKLOAD_SINGLE_1X(
                &pSingle1x->pwrModel,
                &pObserved->singles[i].super,
                numMetricsInputs,
                pMetricsInputs,
                localEstimatedMetrics,
                pTypeParams),
            pwrPolicyPerfCfPwrModelScale_WORKLOAD_COMBINED_1X_exit);
    }
    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_END;

    // Populate the estimated metrics
    for (loopIdx = 0; loopIdx < numMetricsInputs; loopIdx++)
    {
        pEstimated = (LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_ESTIMATED_METRICS_WORKLOAD_COMBINED_1X *)ppEstimatedMetrics[loopIdx];

        pEstimated->estimatedVal = 0;

        PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_INDEX_BEGIN(pCombined1x, i)
        {
            pEstimated->estimatedVal += pEstimated->singles[i].estimatedVal;
        }
        PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_INDEX_END;
    }

pwrPolicyPerfCfPwrModelScale_WORKLOAD_COMBINED_1X_exit:
    return status;
}

/*!
 * @copydoc PwrPolicyDomGrpLimitsMaskSet
 */
FLCN_STATUS
pwrPolicyDomGrpLimitsMaskSet_WORKLOAD_COMBINED_1X
(
    PWR_POLICY_DOMGRP *pPolicy
)
{
    PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x =
        (PWR_POLICY_WORKLOAD_COMBINED_1X *)pPolicy;
    LwU8                             i;
    FLCN_STATUS                      status = FLCN_OK;
    PWR_POLICY_WORKLOAD_SINGLE_1X   *pSingle1x;

    pwrPolicyDomGrpLimitsMaskSet_SUPER(pPolicy);

    // Create @ref domGrpLimitMask from each SINGLE_1X.
    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_BEGIN(
        pCombined1x, pSingle1x, i, status,
        pwrPolicyDomGrpLimitsMaskSet_WORKLOAD_COMBINED_1X_exit)
    {
        RM_PMU_PMGR_PWR_POLICY_DOMGRP_MAP_ENTRY domGrpData;

        // Get the domain group data from clkDomainIdx
        PMU_ASSERT_OK_OR_GOTO(status,
            pwrPolicies35DomGrpDataGet(
                PWR_POLICIES_35_GET(),
                &domGrpData,
                pSingle1x->clkDomainIdx),
            pwrPolicyDomGrpLimitsMaskSet_WORKLOAD_COMBINED_1X_exit);

        boardObjGrpMaskBitSet(&(pCombined1x->super.domGrpLimitMask),
            domGrpData.domGrpLimitIdx);
    }
    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_END;

pwrPolicyDomGrpLimitsMaskSet_WORKLOAD_COMBINED_1X_exit:
    return status;
}

/*!
 * @brief      Copy in workload-specific parameters into observed metrics.
 *
 * @param      pCombined1x           CWCC1X pointer
 * @param      pObsMetrics           CWCC1X observed metrics
 * @param      pTgp1xWorkloadParams  Tgp1x workload parameters
 *
 * @return     FLCN_OK in case of success, propogated error otherwise.
 */
FLCN_STATUS
pwrPolicyWorkloadCombined1xImportTgp1xWorkloadParameters
(
    PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x,
    LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_OBSERVED_METRICS_WORKLOAD_COMBINED_1X *pObsMetrics,
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS_TGP_1X_WORKLOAD_PARAMETERS *pTgp1xWorkloadParams
)
{
    LwBoardObjIdx                   i;
    FLCN_STATUS                     status;
    PWR_POLICY_WORKLOAD_SINGLE_1X  *pSingle1x;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pCombined1x != NULL) &&
         (pObsMetrics != NULL) &&
         (pTgp1xWorkloadParams != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        pwrPolicyWorkloadCombined1xImportTgp1xWorkloadParameters_exit);

    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_BEGIN(
        pCombined1x, pSingle1x, i, status,
        pwrPolicyWorkloadCombined1xImportTgp1xWorkloadParameters_exit)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            pwrPolicyWorkloadSingle1xImportTgp1xWorkloadParameters(
                pSingle1x,
                &pObsMetrics->singles[i],
                pTgp1xWorkloadParams),
            pwrPolicyWorkloadCombined1xImportTgp1xWorkloadParameters_exit);
    }
    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_END;

pwrPolicyWorkloadCombined1xImportTgp1xWorkloadParameters_exit:
    return status;
}

/*!
 * @copydoc PwrPolicy35PerfCfControllerMaskGet
 */
FLCN_STATUS
pwrPolicy35PerfCfControllerMaskGet_WORKLOAD_COMBINED_1X
(
    PWR_POLICY          *pPolicy,
    BOARDOBJGRPMASK_E32 *pPerfCfControllerMask
)
{
    PWR_POLICY_WORKLOAD_COMBINED_1X      *pCombined1x =
        (PWR_POLICY_WORKLOAD_COMBINED_1X *)pPolicy;
    PWR_POLICY_WORKLOAD_SINGLE_1X        *pSingle1x;
    FLCN_STATUS                           status;
    LwU8                                  i;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pPerfCfControllerMask != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        pwrPolicy35PerfCfControllerMaskGet_WORKLOAD_COMBINED_1X_exit);

    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_BEGIN(
        pCombined1x, pSingle1x, i, status,
        pwrPolicy35PerfCfControllerMaskGet_WORKLOAD_COMBINED_1X_exit)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            pwrPolicy35PerfCfControllerMaskGet_WORKLOAD_SINGLE_1X(
            &pSingle1x->super, pPerfCfControllerMask),
            pwrPolicy35PerfCfControllerMaskGet_WORKLOAD_COMBINED_1X_exit);
    }
    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_END;

pwrPolicy35PerfCfControllerMaskGet_WORKLOAD_COMBINED_1X_exit:
   return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * Computes the output domain group limit values this power policy wishes to
 * apply.  This is determined as the maximum frequency and voltage values which
 * would yield power at the budget/limit value given the filtered workload
 * value, based on the power equation:
 *
 *      Solve for f using the equations:
 *          P_Limit_Combined >= P_Combined(f)
 *          P_Limit_Single   >= P_Single(f) for each SINGLE_1X
 *      where,
 *          P_Combined(f) is the sum of power of all rails at f and
 *          P_Single(f) is the power of each single rail at f
 *
 * @param[in]   pCombined1x   PWR_POLICY_WORKLOAD_COMBINED_1X pointer.
 * @param[out]  pDomGrpLimits
 *     RM_PMU_PERF_DOMAIN_GROUP_LIMITS structure to populate with the limits the
 *     PWR_POLICY object wants to apply.
 *
 * @return FLCN_OK
 *     Domain group limit values computed successfully.
 * @return FLCN_ERR_ILWALID_STATE
 *     Encountered an invalid state during computation.
 * @return Other errors
 *     Propagates return values from various calls.
 */
static FLCN_STATUS
s_pwrPolicyWorkloadCombined1xComputeClkMHz
(
    PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x,
    PERF_DOMAIN_GROUP_LIMITS        *pDomGrpLimits
)
{
    PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1xPri;
    PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1x;
    CLK_DOMAIN                    *pDomainPrimary;
    CLK_DOMAIN                    *pDomain;
    CLK_DOMAIN_PROG               *pDomainProgPrimary;
    PERF_LIMITS_PSTATE_RANGE       pstateRange;
    PERF_LIMITS_VF_RANGE           vfDomainRange;
    PERF_LIMITS_VF_ARRAY           vfArrayIn;
    PERF_LIMITS_VF_ARRAY           vfArrayOut;
    BOARDOBJGRPMASK_E32            priClkDomainMask;
    BOARDOBJGRPMASK_E32            secClkDomainMask;
    PSTATE                        *pPstate;
    LwU32                          freqMHz;
    LwU16                          minIdx;
    LwU16                          startIdx;
    LwU16                          midIdx;
    LwU16                          endIdx;
    LwBool                         bLimitSatisfied;
    FLCN_STATUS                    status;
    LwBoardObjIdx                  singleIdx;

    //
    // Get the SINGLE_1X policy for primary clock domain which is always the
    // policy pointed by singleIdx = 0
    //
    PWR_POLICY_WORKLOAD_COMBINED_1X_GET_SINGLE_1X(
        pCombined1x, 0U, &pSingle1xPri, status,
        s_pwrPolicyWorkloadCombined1xComputeClkMHz_exit);

    // Initialize the required parameters.
    pDomainPrimary = CLK_DOMAIN_GET(pSingle1xPri->clkDomainIdx);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDomainPrimary != NULL),
        FLCN_ERR_ILWALID_STATE,
        s_pwrPolicyWorkloadCombined1xComputeClkMHz_exit);

    pDomainProgPrimary = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomainPrimary, PROG);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDomainProgPrimary != NULL),
        FLCN_ERR_ILWALID_STATE,
        s_pwrPolicyWorkloadCombined1xComputeClkMHz_exit);

    // TODO-Tariq: LPWR residency and median filter

    // Get the highest allowed frequency for the primary clock.
    vfDomainRange.idx = BOARDOBJ_GRP_IDX_TO_8BIT(pSingle1xPri->clkDomainIdx);
    pPstate = PSTATE_GET_BY_LEVEL(perfGetPstateCount() - 1);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pPstate != NULL),
        FLCN_ERR_ILWALID_STATE,
        s_pwrPolicyWorkloadCombined1xComputeClkMHz_exit);
    PERF_LIMITS_RANGE_SET_VALUE(&pstateRange,
        BOARDOBJ_GET_GRP_IDX(&pPstate->super));

    PMU_ASSERT_OK_OR_GOTO(status,
        perfLimitsPstateIdxRangeToFreqkHzRange(&pstateRange,
            &vfDomainRange),
        s_pwrPolicyWorkloadCombined1xComputeClkMHz_exit);

    // Get the start and end index for binary search
    startIdx = clkDomainProgGetIndexByFreqMHz(pDomainProgPrimary,
        vfDomainRange.values[LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MIN] /
            clkDomainFreqkHzScaleGet(pDomainPrimary), LW_FALSE);
    endIdx   = clkDomainProgGetIndexByFreqMHz(pDomainProgPrimary,
        vfDomainRange.values[LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MAX] /
            clkDomainFreqkHzScaleGet(pDomainPrimary), LW_TRUE);

    // Save startIdx as minIdx indicating bottom of VF lwrve
    minIdx   = startIdx;

    // Initialize @ref numStepsBack to zero
    pCombined1x->numStepsBack = 0;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((startIdx != LW_U16_MAX) && (endIdx != LW_U16_MAX)),
        FLCN_ERR_ILWALID_STATE,
        s_pwrPolicyWorkloadCombined1xComputeClkMHz_exit);

    // Init the primary and secondary clock domain masks
    boardObjGrpMaskInit_E32(&priClkDomainMask);
    boardObjGrpMaskInit_E32(&secClkDomainMask);

    // Init the input/output struct.
    PERF_LIMITS_VF_ARRAY_INIT(&vfArrayIn);
    PERF_LIMITS_VF_ARRAY_INIT(&vfArrayOut);

    //
    // Populate the primary and secondary clock domain masks and copy it into the
    // input/output struct
    //
    boardObjGrpMaskBitSet(&priClkDomainMask, pSingle1xPri->clkDomainIdx);
    vfArrayIn.pMask = &priClkDomainMask;

    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SECONDARY_SINGLE_1X_BEGIN(
        pCombined1x, pSingle1x, singleIdx, status,
        s_pwrPolicyWorkloadCombined1xComputeClkMHz_exit)
    {
        boardObjGrpMaskBitSet(&secClkDomainMask, pSingle1x->clkDomainIdx);

        // Get freq floor for secondary clock and the estimated power/current
        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrPolicyWorkloadCombined1xFreqFloorStateGet(pCombined1x,
                pSingle1x, &pstateRange, singleIdx, &estFloorMetrics),
            s_pwrPolicyWorkloadCombined1xComputeClkMHz_exit);

        // Copy the floor freq and estimatedVal to the the pSingles structure
        pCombined1x->estMetrics.pSingles[singleIdx].freqFloorMHz =
            estFloorMetrics.freqMHz;
        pCombined1x->estMetrics.pSingles[singleIdx].estimatedValFloor =
            estFloorMetrics.estimatedVal;
    }
    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SECONDARY_SINGLE_1X_END;

    vfArrayOut.pMask = &secClkDomainMask;

    // Binary search over set of primary clock freq [freqMHz(startIdx), freqMHz(endIdx)]
    while (LW_TRUE)
    {
        //
        // We want to round up to the higher index if (startIdx + endIdx)
        // is not exactly divisible by 2. This is to ensure that we always
        // test the highest index.
        //
        midIdx = LW_UNSIGNED_ROUNDED_DIV((startIdx + endIdx), 2);

        freqMHz = clkDomainProgGetFreqMHzByIndex(pDomainProgPrimary, midIdx);
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (freqMHz != LW_U16_MAX),
            FLCN_ERR_ILWALID_STATE,
            s_pwrPolicyWorkloadCombined1xComputeClkMHz_exit);

        // Populate the primary clock freq at which we need the estimate of power
        pCombined1x->estMetrics.pSingles[0].freqMHz = freqMHz;

        // Propagate primary clock to all other clocks.
        vfArrayIn.values[pSingle1xPri->clkDomainIdx] =
            pCombined1x->estMetrics.pSingles[0].freqMHz *
            clkDomainFreqkHzScaleGet(pDomainPrimary);

        if (!boardObjGrpMaskIsZero(&secClkDomainMask))
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                perfLimitsFreqPropagate(&pstateRange,
                                        &vfArrayIn,
                                        &vfArrayOut,
                                        LW2080_CTRL_CLK_CLK_PROP_TOP_IDX_ILWALID),
                s_pwrPolicyWorkloadCombined1xComputeClkMHz_exit);
        }

        // Populate the secondary clock freq at which we need the estimate of power
        PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SECONDARY_SINGLE_1X_BEGIN(
            pCombined1x, pSingle1x, singleIdx, status,
            s_pwrPolicyWorkloadCombined1xComputeClkMHz_exit)
        {
            pDomain = CLK_DOMAIN_GET(pSingle1x->clkDomainIdx);
            PMU_ASSERT_TRUE_OR_GOTO(status,
                (pDomain != NULL),
                FLCN_ERR_ILWALID_STATE,
                s_pwrPolicyWorkloadCombined1xComputeClkMHz_exit);

            //
            // If limit is satisfied for this rail at freq floor, do not reduce
            // freq further
            //
            if (pCombined1x->estMetrics.pSingles[singleIdx].estimatedValFloor <=
                    pwrPolicyLimitLwrrGet(&pSingle1x->super))
            {
                pCombined1x->estMetrics.pSingles[singleIdx].freqMHz =
                    LW_MAX(vfArrayOut.values[pSingle1x->clkDomainIdx] /
                               clkDomainFreqkHzScaleGet(pDomain),
                           pCombined1x->estMetrics.pSingles[singleIdx].freqFloorMHz);
            }
        }
        PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SECONDARY_SINGLE_1X_END;

        PMU_ASSERT_OK_OR_GOTO(status,
            pwrPolicyPerfCfPwrModelScaleMetrics_WORKLOAD_COMBINED_1X(
                &pCombined1x->pwrModel, &pCombined1x->obsMetrics.super,
                &pCombined1x->estMetrics.super),
            s_pwrPolicyWorkloadCombined1xComputeClkMHz_exit);

        bLimitSatisfied = (pCombined1x->estMetrics.estimatedVal <=
            pwrPolicyLimitLwrrGet(&pCombined1x->super.super));

        // Check for individual rail limits being satisfied
        PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_BEGIN(
            pCombined1x, pSingle1x, singleIdx, status,
            s_pwrPolicyWorkloadCombined1xComputeClkMHz_exit)
        {
            bLimitSatisfied &= pCombined1x->estMetrics.pSingles[singleIdx].estimatedVal <=
                pwrPolicyLimitLwrrGet(&pSingle1x->super);
        }
        PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_END;

        if (startIdx == endIdx)
        {
            if (!bLimitSatisfied &&
                (startIdx > minIdx))
            {
                // Back up by one point and check if we satisfy
                startIdx = endIdx = startIdx - 1;
                pCombined1x->numStepsBack++;
                continue;
            }
            else
            {
                break;
            }
        }

        // Search up or down based on condition at freqMHz(midIdx)
        if (bLimitSatisfied)
        {
            startIdx = midIdx;
        }
        else
        {
            endIdx = midIdx - 1;
        }
    }

    // Set the new frequency values
    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_BEGIN(
        pCombined1x, pSingle1x, singleIdx, status,
        s_pwrPolicyWorkloadCombined1xComputeClkMHz_exit)
    {
        RM_PMU_PMGR_PWR_POLICY_DOMGRP_MAP_ENTRY domGrpData;

        pDomain = CLK_DOMAIN_GET(pSingle1x->clkDomainIdx);
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pDomain != NULL),
            FLCN_ERR_ILWALID_STATE,
            s_pwrPolicyWorkloadCombined1xComputeClkMHz_exit);

        // Get the domain group data from clkDomainIdx
        PMU_ASSERT_OK_OR_GOTO(status,
            pwrPolicies35DomGrpDataGet(
                PWR_POLICIES_35_GET(),
                &domGrpData,
                pSingle1x->clkDomainIdx),
            s_pwrPolicyWorkloadCombined1xComputeClkMHz_exit);

        pDomGrpLimits->values[domGrpData.domGrpLimitIdx] =
            pCombined1x->estMetrics.pSingles[singleIdx].freqMHz *
            clkDomainFreqkHzScaleGet(pDomain);
    }
    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_END;

    // As PSTATE 0 contains the entire freq range, we will always be in P0.
    pDomGrpLimits->values[PERF_DOMAIN_GROUP_PSTATE] =
        (perfGetPstateCount() - 1);

s_pwrPolicyWorkloadCombined1xComputeClkMHz_exit:
    return status;
}

/*!
 * Helper function to determine if the limit is satisfied at
 * the input frequency
 *
 * @param[in]  pCombined1x  PWR_POLICY_WORKLOAD_COMBINED_1X pointer.
 * @param[in]  pFreqMHz     Array of SINGLE1X(MAX)which holds the
 *                              frequency values
 * @param[out] pbLimitSatisfied
 *    Bool to indicate if limit is satisfied at
 *    frequency
 *
 * @return FLCN_OK
 *     Function exelwted successfully.
 * @return Other errors
 *     Propagates return values from various calls.
 */
static  FLCN_STATUS
s_pwrPolicyWorkloadCombined1xIsLimitSatisfiedAtFreq
(
    PWR_POLICY_WORKLOAD_COMBINED_1X                                            *pCombined1x,
    LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_SINGLES_FREQ_TUPLE *pRegimeFreq,
    LwBool                                                                     *pbLimitSatisfied
)
{
    PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1x;
    LwBoardObjIdx                  singleIdx;
    FLCN_STATUS                    status;
    LwU8                           numSingles;

    numSingles = pCombined1x->singleRelIdxLast -
        pCombined1x->singleRelIdxFirst + 1;
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (numSingles == SINGLE1X(MAX)),
        FLCN_ERR_ILWALID_STATE,
        s_pwrPolicyWorkloadCombined1xIsLimitSatisfiedAtFreq_exit);

    // Get scale metrics at the input frequency
    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_BEGIN(
        pCombined1x, pSingle1x, singleIdx, status,
        s_pwrPolicyWorkloadCombined1xIsLimitSatisfiedAtFreq_exit)
    {
        if (pRegimeFreq->freqMHz[singleIdx] == 0)
        {
            *pbLimitSatisfied = LW_TRUE;
            goto s_pwrPolicyWorkloadCombined1xIsLimitSatisfiedAtFreq_exit;
        }
        pCombined1x->estMetrics.pSingles[singleIdx].freqMHz =
            pRegimeFreq->freqMHz[singleIdx];
    }
    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_END;

    PMU_ASSERT_OK_OR_GOTO(status,
        pwrPolicyPerfCfPwrModelScaleMetrics_WORKLOAD_COMBINED_1X(
            &pCombined1x->pwrModel, &pCombined1x->obsMetrics.super,
            &pCombined1x->estMetrics.super),
        s_pwrPolicyWorkloadCombined1xIsLimitSatisfiedAtFreq_exit);

    *pbLimitSatisfied = (pCombined1x->estMetrics.estimatedVal <=
        pwrPolicyLimitLwrrGet(&pCombined1x->super.super));

    // Check for individual rail limits being satisfied
    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_BEGIN(
        pCombined1x, pSingle1x, singleIdx, status,
        s_pwrPolicyWorkloadCombined1xIsLimitSatisfiedAtFreq_exit)
    {
        *pbLimitSatisfied &=
            pCombined1x->estMetrics.pSingles[singleIdx].estimatedVal <=
            pwrPolicyLimitLwrrGet(&pSingle1x->super);
    }
    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_END;

s_pwrPolicyWorkloadCombined1xIsLimitSatisfiedAtFreq_exit:
    return status;
}

/*!
 * Helper function to initialise @ref PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_INIT_DATA
 * which defines the SEARCH_REGIME tree.
 *
 * This tree specifies the possible SEARCH_REGIME combinations for a
 * given iteration of the PWR_POLICY_WORKLOAD_COMBINED_1X.  This tree
 * is ordered by priority, where left branches have higher priority
 * than right branches - i.e. if the two choices have the same
 * frequency, priority will be given to the left choice.
 *
 *                           DUMMY_ROOT --+
 *                           |            |
 *                DEFAULT_RATIO           |
 *               |           |            |
 *  SEC_VMIN_FLOOR           SEC_SOFT_FLOOR
 *                           |            |
 *              PRI_VMIN_FLOOR            +-------+
 *              |            |                    |
 * SEC_VMIN_FLOOR            MIN_RATIO            MAX_RATIO
 *                           |                    |       |
 *                           SEC_VMIN_FLOOR       |       |
 *                                                |       |
 *                                   SEC_VMIN_FLOOR       PRI_VMIN_FLOOR
 *                                                        |            |
 *                                           SEC_VMIN_FLOOR            MIN_RATIO
 *                                                                     |
 *                                                                     SEC_VMIN_FLOOR
 *
 * @param[in]      pCombined1x   PWR_POLICY_WORKLOAD_COMBINED_1X pointer.
 *
 * @return FLCN_OK
 *     Function exelwted successfully.
 * @return Other errors
 *     Propagates return values from various calls.
 */
static FLCN_STATUS
s_pwrPolicyWorkloadCombined1xInitRegimeData
(
    PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x
)
{
    PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1xPri;
    FLCN_STATUS                    status;
    LwBoardObjIdx                  idx;
    PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1xSec = NULL;

    PWR_POLICY_WORKLOAD_COMBINED_1X_GET_SINGLE_1X(
        pCombined1x, SINGLE1X(PRI), &pSingle1xPri, status,
        s_pwrPolicyWorkloadCombined1xInitRegimeData_exit);

    if (HAS_SINGLE1X(pCombined1x, SEC))
    {
        PWR_POLICY_WORKLOAD_COMBINED_1X_GET_SINGLE_1X(
            pCombined1x, SINGLE1X(SEC), &pSingle1xSec, status,
            s_pwrPolicyWorkloadCombined1xInitRegimeData_exit);
    }

    boardObjGrpMaskInit_E32(&pCombined1x->supportedRegimeMask);

    boardObjGrpMaskBitSet(&pCombined1x->supportedRegimeMask,
        REGIME(DEFAULT_RATIO));
    if (pSingle1xSec != NULL)
    {
        boardObjGrpMaskBitSet(&pCombined1x->supportedRegimeMask,
            REGIME(SEC_VMIN_FLOOR));
    }

    if (pwrPolicySingle1xIsSoftFloorEnabled(pSingle1xSec))
    {
        boardObjGrpMaskBitSet(&pCombined1x->supportedRegimeMask,
            REGIME(SEC_SOFT_FLOOR));
        boardObjGrpMaskBitSet(&pCombined1x->supportedRegimeMask,
            REGIME(PRI_VMIN_FLOOR));
        boardObjGrpMaskBitSet(&pCombined1x->supportedRegimeMask,
            REGIME(MAX_RATIO));
        boardObjGrpMaskBitSet(&pCombined1x->supportedRegimeMask,
            REGIME(MIN_RATIO));
    }

    BOARDOBJGRPMASK_FOR_EACH_BEGIN(&pCombined1x->supportedRegimeMask, idx)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrPolicyWorkloadCombined1xInitRegimeDataByIdx(
            pCombined1x, idx, &pCombined1x->regimeData[idx].regimeInitData),
            s_pwrPolicyWorkloadCombined1xInitRegimeData_exit);
    }
    BOARDOBJGRPMASK_FOR_EACH_END;

s_pwrPolicyWorkloadCombined1xInitRegimeData_exit:
    return status;
}

/*!
 * Helper function to initialise @ref PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_INIT_DATA
 * for the specified index
 *
 * @param[in]      pCombined1x   PWR_POLICY_WORKLOAD_COMBINED_1X pointer.
 * @param[in]      regimeId
 *                      Index of the @ref PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_INIT_DATA
 *                      to be initialised
 * @param[out]     pRegimeData 
 *                      PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_INIT_DATA pointer
 *
 * @return FLCN_OK
 *     Function exelwted successfully.
 * @return Other errors
 *     Propagates return values from various calls.
 */
static FLCN_STATUS
s_pwrPolicyWorkloadCombined1xInitRegimeDataByIdx
(
    PWR_POLICY_WORKLOAD_COMBINED_1X                  *pCombined1x,
    LwBoardObjIdx                                     regimeId,
    PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_INIT_DATA *pRegimeData
)
{
    PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1xPri;
    FLCN_STATUS                    status;
    PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1xSec = NULL;

    // Sanity check input arguments.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pRegimeData != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        s_pwrPolicyWorkloadCombined1xInitRegimeDataByIdx_exit);

    PWR_POLICY_WORKLOAD_COMBINED_1X_GET_SINGLE_1X(
        pCombined1x, SINGLE1X(PRI), &pSingle1xPri, status,
        s_pwrPolicyWorkloadCombined1xInitRegimeDataByIdx_exit);

    if (HAS_SINGLE1X(pCombined1x, SEC))
    {
        PWR_POLICY_WORKLOAD_COMBINED_1X_GET_SINGLE_1X(
            pCombined1x, SINGLE1X(SEC), &pSingle1xSec, status,
            s_pwrPolicyWorkloadCombined1xInitRegimeDataByIdx_exit);
    }

    pRegimeData->regimeId = regimeId;

    // Initialize required data
    boardObjGrpMaskInit_E32(
        &pRegimeData->childRegimeMask);

    switch(regimeId)
    {
        case REGIME(DUMMY_ROOT):
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                s_pwrPolicyWorkloadCombined1xInitChildRegimeMask(pCombined1x,
                REGIME(SEC_SOFT_FLOOR), &pRegimeData->childRegimeMask),
                s_pwrPolicyWorkloadCombined1xInitRegimeDataByIdx_exit);
            PMU_ASSERT_OK_OR_GOTO(status,
                s_pwrPolicyWorkloadCombined1xInitChildRegimeMask(pCombined1x,
                REGIME(DEFAULT_RATIO), &pRegimeData->childRegimeMask),
                s_pwrPolicyWorkloadCombined1xInitRegimeDataByIdx_exit);
            pRegimeData->clkPropTopIdx = LW2080_CTRL_CLK_CLK_PROP_TOP_IDX_ILWALID;
            pRegimeData->singleIdx     = SINGLE1X(SEC);
            break;
        }
        case REGIME(DEFAULT_RATIO):
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                s_pwrPolicyWorkloadCombined1xInitChildRegimeMask(pCombined1x,
                REGIME(SEC_SOFT_FLOOR), &pRegimeData->childRegimeMask),
                s_pwrPolicyWorkloadCombined1xInitRegimeDataByIdx_exit);
            PMU_ASSERT_OK_OR_GOTO(status,
                s_pwrPolicyWorkloadCombined1xInitChildRegimeMask(pCombined1x,
                REGIME(SEC_VMIN_FLOOR), &pRegimeData->childRegimeMask),
                s_pwrPolicyWorkloadCombined1xInitRegimeDataByIdx_exit);
            pRegimeData->clkPropTopIdx = LW2080_CTRL_CLK_CLK_PROP_TOP_IDX_ILWALID;
            pRegimeData->singleIdx     = SINGLE1X(PRI);
            break;
        }
        case REGIME(SEC_SOFT_FLOOR):
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X_LWDCLK_CTRL);
            PMU_ASSERT_OK_OR_GOTO(status,
                s_pwrPolicyWorkloadCombined1xInitChildRegimeMask(pCombined1x,
                REGIME(PRI_VMIN_FLOOR), &pRegimeData->childRegimeMask),
                s_pwrPolicyWorkloadCombined1xInitRegimeDataByIdx_exit);
            PMU_ASSERT_OK_OR_GOTO(status,
                s_pwrPolicyWorkloadCombined1xInitChildRegimeMask(pCombined1x,
                REGIME(MAX_RATIO), &pRegimeData->childRegimeMask),
                s_pwrPolicyWorkloadCombined1xInitRegimeDataByIdx_exit);
            pRegimeData->clkPropTopIdx = PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_CLK_PROP_TOP_IDX_PARENT_FLOOR;
            pRegimeData->singleIdx     = SINGLE1X(PRI);
            break;
        }
        case REGIME(PRI_VMIN_FLOOR):
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X_LWDCLK_CTRL);
            PMU_ASSERT_OK_OR_GOTO(status,
                s_pwrPolicyWorkloadCombined1xInitChildRegimeMask(pCombined1x,
                REGIME(SEC_VMIN_FLOOR), &pRegimeData->childRegimeMask),
                s_pwrPolicyWorkloadCombined1xInitRegimeDataByIdx_exit);
            PMU_ASSERT_OK_OR_GOTO(status,
                s_pwrPolicyWorkloadCombined1xInitChildRegimeMask(pCombined1x,
                REGIME(MIN_RATIO), &pRegimeData->childRegimeMask),
                s_pwrPolicyWorkloadCombined1xInitRegimeDataByIdx_exit);
            pRegimeData->clkPropTopIdx = PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_CLK_PROP_TOP_IDX_PARENT_FLOOR;
            pRegimeData->singleIdx     = SINGLE1X(SEC);
            break;
        }
        case REGIME(MAX_RATIO):
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X_LWDCLK_CTRL);
            PMU_ASSERT_OK_OR_GOTO(status,
                s_pwrPolicyWorkloadCombined1xInitChildRegimeMask(pCombined1x,
                REGIME(PRI_VMIN_FLOOR), &pRegimeData->childRegimeMask),
                s_pwrPolicyWorkloadCombined1xInitRegimeDataByIdx_exit);
            PMU_ASSERT_OK_OR_GOTO(status,
                s_pwrPolicyWorkloadCombined1xInitChildRegimeMask(pCombined1x,
                REGIME(SEC_VMIN_FLOOR), &pRegimeData->childRegimeMask),
                s_pwrPolicyWorkloadCombined1xInitRegimeDataByIdx_exit);
            pRegimeData->clkPropTopIdx = pSingle1xSec->softFloor.clkPropTopIdx;
            pRegimeData->singleIdx     = SINGLE1X(PRI);
            break;
        }
        case REGIME(MIN_RATIO):
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X_LWDCLK_CTRL);
            PMU_ASSERT_OK_OR_GOTO(status,
                s_pwrPolicyWorkloadCombined1xInitChildRegimeMask(pCombined1x,
                REGIME(SEC_VMIN_FLOOR), &pRegimeData->childRegimeMask),
                s_pwrPolicyWorkloadCombined1xInitRegimeDataByIdx_exit);
            pRegimeData->clkPropTopIdx = LW2080_CTRL_CLK_CLK_PROP_TOP_IDX_ILWALID;
            pRegimeData->singleIdx     = SINGLE1X(PRI);
            break;
        }
        case REGIME(SEC_VMIN_FLOOR):
        {
            pRegimeData->clkPropTopIdx = PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_CLK_PROP_TOP_IDX_PARENT_FLOOR;
            pRegimeData->singleIdx     = SINGLE1X(PRI);
            break;
        }
        default:
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_TRUE_BP();
            goto s_pwrPolicyWorkloadCombined1xInitRegimeDataByIdx_exit;
        }
    }

s_pwrPolicyWorkloadCombined1xInitRegimeDataByIdx_exit:
    return status;
}

/*!
 * Helper function callwlate frequency end points of a regime.
 * The end points of a regime can be set to
 * @ref LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_SEARCH_REGIME_FREQ_ILWALID
 * in case the potential child regime frequency is >= than the parent
 * regime freqeuncy
 *
 * @note In cases where the the requested regime (@ref pRegime) should
 * not be considered due to current system state, this interface will
 * return @ref
 * LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_SEARCH_REGIME_FREQ_ILWALID.
 * This will cover cases such as when the SEC_SOFT_FLOOR falls at or
 * below PRI_VMIN_FLOOR.
 *
 * @param[in]  pCombined1x   PWR_POLICY_WORKLOAD_COMBINED_1X pointer.
 * @param[in]  pParentRegime PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_DATA
 *                           pointer to parent regime
 * @param[out] pRegime       PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_DATA
 *                           pointer
 *
 * @return FLCN_OK
 *     Function exelwted successfully.
 * @return Other errors
 *     Propagates return values from various calls.
 */
static FLCN_STATUS
s_pwrPolicyWorkloadCombined1xRegimeFreqEndPoints
(
    PWR_POLICY_WORKLOAD_COMBINED_1X              *pCombined1x,
    PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_DATA  *pParentRegime,
    PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_DATA  *pRegime
)
{
    PERF_LIMITS_PSTATE_RANGE       pstateRange;
    PERF_LIMITS_VF_RANGE           vfDomainRange;
    PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1xPri;
    PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1xSec = NULL;
    PSTATE                        *pPstate;
    FLCN_STATUS                    status;
    LwU8                           singleIdx;

    // Sanity check input arguments.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pRegime != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        s_pwrPolicyWorkloadCombined1xRegimeFreqEndPoints_exit);

    PWR_POLICY_WORKLOAD_COMBINED_1X_GET_SINGLE_1X(
        pCombined1x, SINGLE1X(PRI), &pSingle1xPri, status,
        s_pwrPolicyWorkloadCombined1xRegimeFreqEndPoints_exit);
    if (HAS_SINGLE1X(pCombined1x, SEC))
    {
        PWR_POLICY_WORKLOAD_COMBINED_1X_GET_SINGLE_1X(
            pCombined1x, SINGLE1X(SEC), &pSingle1xSec, status,
            s_pwrPolicyWorkloadCombined1xRegimeFreqEndPoints_exit);
    }

    pPstate = PSTATE_GET_BY_LEVEL(perfGetPstateCount() - 1);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pPstate != NULL), FLCN_ERR_ILWALID_STATE,
        s_pwrPolicyWorkloadCombined1xRegimeFreqEndPoints_exit);

    PERF_LIMITS_RANGE_SET_VALUE(&pstateRange,
        BOARDOBJ_GET_GRP_IDX(&pPstate->super));

    status = FLCN_ERR_NOT_SUPPORTED;
    switch(pRegime->regimeInitData.regimeId)
    {
        case REGIME(DEFAULT_RATIO):
        {
            CLK_DOMAIN  *pDomainPri;

            status = FLCN_OK;
            pDomainPri = CLK_DOMAIN_GET(pSingle1xPri->clkDomainIdx);
            PMU_ASSERT_TRUE_OR_GOTO(status,
                (pDomainPri != NULL),
                FLCN_ERR_ILWALID_STATE,
            s_pwrPolicyWorkloadCombined1xRegimeFreqEndPoints_exit);

            vfDomainRange.idx = BOARDOBJ_GRP_IDX_TO_8BIT(pSingle1xPri->clkDomainIdx);
            PMU_ASSERT_OK_OR_GOTO(status,
                perfLimitsPstateIdxRangeToFreqkHzRange(&pstateRange,
                &vfDomainRange),
            s_pwrPolicyWorkloadCombined1xRegimeFreqEndPoints_exit);

            // Get frequency endpoint for regime DEFAULT_RATIO
            pRegime->regimeFreq.freqMHz[SINGLE1X(PRI)] =
                (vfDomainRange.values[LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MAX] /
                clkDomainFreqkHzScaleGet(pDomainPri));

            if (pSingle1xSec != NULL)
            {
                PMU_ASSERT_OK_OR_GOTO(status,
                    s_pwrPolicyWorkloadCombined1xClkPropagate(
                        pRegime->regimeFreq.freqMHz[SINGLE1X(PRI)],
                        pSingle1xPri->clkDomainIdx, pSingle1xSec->clkDomainIdx,
                        pRegime->regimeInitData.clkPropTopIdx,
                        &pRegime->regimeFreq.freqMHz[SINGLE1X(SEC)]),
                    s_pwrPolicyWorkloadCombined1xRegimeFreqEndPoints_exit);
            }
            break;
        }
        case REGIME(SEC_SOFT_FLOOR):
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X_LWDCLK_CTRL);

            status = FLCN_OK;
            // Sanity check pointers before referencing.
            PMU_ASSERT_TRUE_OR_GOTO(status,
                ((pParentRegime != NULL) &&
                 (pSingle1xSec != NULL)),
                FLCN_ERR_ILWALID_STATE,
                s_pwrPolicyWorkloadCombined1xRegimeFreqEndPoints_exit);
            //
            // Sanity check parent REGIME meets all expectations for
            // DEFAULT_RATIO or DUMMY_ROOT.
            //
            PMU_ASSERT_TRUE_OR_GOTO(status,
                (((pParentRegime->regimeInitData.regimeId == REGIME(DEFAULT_RATIO)) ||
                    (pParentRegime->regimeInitData.regimeId == REGIME(DUMMY_ROOT))) &&
                 (pParentRegime->regimeInitData.clkPropTopIdx !=
                     PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_CLK_PROP_TOP_IDX_PARENT_FLOOR)),
                FLCN_ERR_ILWALID_STATE,
                s_pwrPolicyWorkloadCombined1xRegimeFreqEndPoints_exit);

            // Set SEC rail to soft-floor frequency.
            pRegime->regimeFreq.freqMHz[SINGLE1X(SEC)] =
                pCombined1x->obsMetrics.pSingles[SINGLE1X(SEC)].freqSoftFloorMHz;
            //
            // Propagate SEC rail soft-floor frequency to PRI based on
            // propagation of parent REGIME.
            //
            PMU_ASSERT_OK_OR_GOTO(status,
                s_pwrPolicyWorkloadCombined1xClkPropagate(
                    pRegime->regimeFreq.freqMHz[SINGLE1X(SEC)],
                    pSingle1xSec->clkDomainIdx, pSingle1xPri->clkDomainIdx,
                    pParentRegime->regimeInitData.clkPropTopIdx,
                    &pRegime->regimeFreq.freqMHz[SINGLE1X(PRI)]),
                s_pwrPolicyWorkloadCombined1xRegimeFreqEndPoints_exit);

            //
            // If PRI_VMIN_FLOOR >=SEC_SOFT_FLOOR, we
            // should not consider SEC_SOFT_FLOOR, and will
            // return some invalid value here which @ref
            // s_pwrPolicyWorkloadCombined1xRegimeSpaceBuild() can
            // check for.
            //
            if (pCombined1x->obsMetrics.pSingles[SINGLE1X(PRI)].freqFmaxVminMHz >=
                pRegime->regimeFreq.freqMHz[SINGLE1X(PRI)])
            {
                pRegime->regimeFreq.freqMHz[SINGLE1X(PRI)] =
                    LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_SEARCH_REGIME_FREQ_ILWALID;
                pRegime->regimeFreq.freqMHz[SINGLE1X(SEC)] =
                    LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_SEARCH_REGIME_FREQ_ILWALID;
            }
            break;
        }
        case REGIME(MAX_RATIO):
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X_LWDCLK_CTRL);

            status = FLCN_OK;
            // Sanity check pointers before referencing.
            PMU_ASSERT_TRUE_OR_GOTO(status,
                ((pParentRegime != NULL) &&
                 (pSingle1xSec != NULL)),
                FLCN_ERR_ILWALID_STATE,
                s_pwrPolicyWorkloadCombined1xRegimeFreqEndPoints_exit);
            //
            // Sanity check parent REGIME meets all expectations for
            // SEC_SOFT_FLOOR.
            //
            PMU_ASSERT_TRUE_OR_GOTO(status,
                ((pParentRegime->regimeInitData.regimeId == REGIME(SEC_SOFT_FLOOR)) &&
                 (pParentRegime->regimeInitData.clkPropTopIdx ==
                     PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_CLK_PROP_TOP_IDX_PARENT_FLOOR) &&
                 (pParentRegime->regimeInitData.singleIdx == SINGLE1X(PRI))),
                FLCN_ERR_ILWALID_STATE,
                s_pwrPolicyWorkloadCombined1xRegimeFreqEndPoints_exit);
            //
            // When have floor on SEC rail, PRI rail is varying, In
            // this case, the ratio between SEC and PRI (i.e. SEC =
            // PRI * ratio) will be increasing, as SEC stays constant
            // and PRI is decreasing.  Thus, must constrain to the
            // maximum propagation ratio - i.e. SEC <= PRI * maxRatio,
            // so compute endpoint as PRI = SEC / maxRatio.
            //
            PMU_ASSERT_OK_OR_GOTO(status,
                s_pwrPolicyWorkloadCombined1xClkPropagate(
                    pParentRegime->regimeFreq.freqMHz[SINGLE1X(SEC)],
                    pSingle1xSec->clkDomainIdx, pSingle1xPri->clkDomainIdx,
                    pRegime->regimeInitData.clkPropTopIdx,
                    &pRegime->regimeFreq.freqMHz[SINGLE1X(PRI)]),
                s_pwrPolicyWorkloadCombined1xRegimeFreqEndPoints_exit);
            pRegime->regimeFreq.freqMHz[SINGLE1X(SEC)] =
                pParentRegime->regimeFreq.freqMHz[SINGLE1X(SEC)];
            break;
        }
        case REGIME(MIN_RATIO):
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X_LWDCLK_CTRL);

            status = FLCN_OK;
            // Sanity check pointers.
            PMU_ASSERT_TRUE_OR_GOTO(status,
                ((pParentRegime != NULL) &&
                 (pSingle1xSec != NULL)),
                FLCN_ERR_ILWALID_STATE,
                s_pwrPolicyWorkloadCombined1xRegimeFreqEndPoints_exit);
            //
            // Sanity check parent REGIME meets all expectations for
            // PRI_VMIN_FLOOR.
            //
            PMU_ASSERT_TRUE_OR_GOTO(status,
                ((pParentRegime->regimeInitData.regimeId == REGIME(PRI_VMIN_FLOOR)) &&
                 (pParentRegime->regimeInitData.clkPropTopIdx ==
                     PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_CLK_PROP_TOP_IDX_PARENT_FLOOR) &&
                 (pParentRegime->regimeInitData.singleIdx == SINGLE1X(SEC))),
                FLCN_ERR_ILWALID_STATE,
                s_pwrPolicyWorkloadCombined1xRegimeFreqEndPoints_exit);
            //
            // When have floor on PRI rail, SEC rail is varying, In
            // this case, the ratio between SEC and PRI (i.e. SEC =
            // PRI * ratio) will be decreasing, as PRI stays constant
            // and SEC is decreasing.  Thus, must constrain to the
            // minimum/default propagation ratio - i.e. SEC >= PRI *
            // minRaio, so compute endpoint as SEC = PRI * minRatio.
            //
            PMU_ASSERT_OK_OR_GOTO(status,
                s_pwrPolicyWorkloadCombined1xClkPropagate(
                    pParentRegime->regimeFreq.freqMHz[SINGLE1X(PRI)],
                    pSingle1xPri->clkDomainIdx, pSingle1xSec->clkDomainIdx,
                    pRegime->regimeInitData.clkPropTopIdx,
                    &pRegime->regimeFreq.freqMHz[SINGLE1X(SEC)]),
                s_pwrPolicyWorkloadCombined1xRegimeFreqEndPoints_exit);
            pRegime->regimeFreq.freqMHz[SINGLE1X(PRI)] =
                pParentRegime->regimeFreq.freqMHz[SINGLE1X(PRI)];
            break;
        }
        case REGIME(PRI_VMIN_FLOOR):
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X_LWDCLK_CTRL);

            status = FLCN_OK;
            // Sanity check pointers before referencing.
            PMU_ASSERT_TRUE_OR_GOTO(status,
                ((pParentRegime != NULL) &&
                 (pSingle1xSec != NULL)),
                FLCN_ERR_ILWALID_STATE,
                s_pwrPolicyWorkloadCombined1xRegimeFreqEndPoints_exit);

            // Pull out Fmax @ Vmin from the OBSERVED_METRIC.
            pRegime->regimeFreq.freqMHz[SINGLE1X(PRI)] =
                    pCombined1x->obsMetrics.pSingles[SINGLE1X(PRI)].freqFmaxVminMHz;
            //
            // If the parent REGIME has a propagation ratio between
            // the rails (i.e. @ref clkPropTopIdx != @ref
            // PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_CLK_PROP_TOP_IDX_PARENT_FLOOR),
            // propagate from the Fmax @ Vmin value to the other rail.
            //
            if (pParentRegime->regimeInitData.clkPropTopIdx !=
                    PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_CLK_PROP_TOP_IDX_PARENT_FLOOR)
            {
                PMU_ASSERT_OK_OR_GOTO(status,
                    s_pwrPolicyWorkloadCombined1xClkPropagate(
                        pRegime->regimeFreq.freqMHz[SINGLE1X(PRI)],
                        pSingle1xPri->clkDomainIdx, pSingle1xSec->clkDomainIdx,
                        pParentRegime->regimeInitData.clkPropTopIdx,
                        &pRegime->regimeFreq.freqMHz[SINGLE1X(SEC)]),
                    s_pwrPolicyWorkloadCombined1xRegimeFreqEndPoints_exit);
            }
            //
            // Otherwise (i.e. @ref clkPropTopIdx != @ref
            // PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_CLK_PROP_TOP_IDX_PARENT_FLOOR),
            // pull out the other rail's frequency from the parent
            // REGIME, as it will be constant/floored.
            //
            else
            {
                pRegime->regimeFreq.freqMHz[SINGLE1X(SEC)] =
                    pParentRegime->regimeFreq.freqMHz[SINGLE1X(SEC)];
            }

            break;
        }
        case REGIME(SEC_VMIN_FLOOR):
        {
            status = FLCN_OK;
            // Sanity check pointers before referencing.
            PMU_ASSERT_TRUE_OR_GOTO(status,
                ((pParentRegime != NULL) &&
                 (pSingle1xSec != NULL)),
                FLCN_ERR_ILWALID_STATE,
                s_pwrPolicyWorkloadCombined1xRegimeFreqEndPoints_exit);

            // Pull out Fmax @ Vmin from the OBSERVED_METRIC.
            pRegime->regimeFreq.freqMHz[SINGLE1X(SEC)] =
                    pCombined1x->obsMetrics.pSingles[SINGLE1X(SEC)].freqFmaxVminMHz;
            //
            // If the parent REGIME has a propagation ratio between
            // the rails (i.e. @ref clkPropTopIdx != @ref
            // PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_CLK_PROP_TOP_IDX_PARENT_FLOOR),
            // propagate from the Fmax @ Vmin value to the other rail.
            //
            if (pParentRegime->regimeInitData.clkPropTopIdx !=
                    PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_CLK_PROP_TOP_IDX_PARENT_FLOOR)
            {
                PMU_ASSERT_OK_OR_GOTO(status,
                    s_pwrPolicyWorkloadCombined1xClkPropagate(
                        pRegime->regimeFreq.freqMHz[SINGLE1X(SEC)],
                        pSingle1xSec->clkDomainIdx, pSingle1xPri->clkDomainIdx,
                        pParentRegime->regimeInitData.clkPropTopIdx,
                        &pRegime->regimeFreq.freqMHz[SINGLE1X(PRI)]),
                    s_pwrPolicyWorkloadCombined1xRegimeFreqEndPoints_exit);
            }
            //
            // Otherwise (i.e. @ref clkPropTopIdx != @ref
            // PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_CLK_PROP_TOP_IDX_PARENT_FLOOR),
            // pull out the other rail's frequency from the parent
            // REGIME, as it will be constant/floored.
            //
            else
            {
                pRegime->regimeFreq.freqMHz[SINGLE1X(PRI)] =
                    pParentRegime->regimeFreq.freqMHz[SINGLE1X(PRI)];
            }
            break;
        }
    }
    // Catch unsupported REGIME ID errors above.
    PMU_ASSERT_OK_OR_GOTO(status, status,
        s_pwrPolicyWorkloadCombined1xRegimeFreqEndPoints_exit);

    //
    // Sanity check this REGIME's generated frequency endpoint tuple against
    // the parent REGIME's endpoint tuple.
    //
    if ((pParentRegime != NULL) &&
        (pParentRegime->regimeInitData.regimeId != REGIME(DUMMY_ROOT)))
    {
        PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_INDEX_BEGIN(pCombined1x, singleIdx)
        {
            //
            // Sanity check that this child REGIME's generated
            // frequency endpoint tuple is < parent REGIME's.  This
            // means must be strictly less than on parent REGIME's
            // singleIdx, less than or equal on all others.
            //
            PMU_ASSERT_TRUE_OR_GOTO(status,
                ((pRegime->regimeFreq.freqMHz[singleIdx] ==
                    LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_SEARCH_REGIME_FREQ_ILWALID) ||
                 (pRegime->regimeFreq.freqMHz[singleIdx] < pParentRegime->regimeFreq.freqMHz[singleIdx]) ||
                 ((singleIdx != pParentRegime->regimeInitData.singleIdx) &&
                  (pRegime->regimeFreq.freqMHz[singleIdx] <= pParentRegime->regimeFreq.freqMHz[singleIdx]))),
                FLCN_ERR_ILWALID_STATE,
                s_pwrPolicyWorkloadCombined1xRegimeFreqEndPoints_exit);
        }
        PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_INDEX_END;
    }

s_pwrPolicyWorkloadCombined1xRegimeFreqEndPoints_exit:
    return status;
}

/*!
 * @brief Helper function to construct
 * @ref LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_SEARCH_REGIME array
 * by picking the correct branch of the SEARCH_REGIME tree.
 *
 * For more information on the SEARCH_REGIME tree see @ref s_pwrPolicyWorkloadCombined1xInitRegimeData().
 *
 * @param[in]      pCombined1x   PWR_POLICY_WORKLOAD_COMBINED_1X pointer.
 *
 * @return FLCN_OK
 *     Function exelwted successfully.
 * @return Other errors
 *     Propagates return values from various calls.
 */
static FLCN_STATUS
s_pwrPolicyWorkloadCombined1xRegimeSpaceBuild
(
    PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x
)
{
    PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_DATA                    *pParentRegimeData;
    PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_DATA                    *pRegimeData         = NULL;
    LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_SEARCH_REGIME *pSearchRegime       = NULL;
    LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_SEARCH_REGIME *pParentSearchRegime = NULL;
    BOARDOBJGRPMASK_E32            childRegimeMask;
    BOARDOBJGRPMASK_E32            regimeNotInsertedMask;
    LwBoardObjIdx                  clkDomainIdx;
    LwU16                          clkIdx;
    FLCN_STATUS                    status;
    LwU8                           regimeNum = 0;

    // Initialise required variables
    PMU_ASSERT_OK_OR_GOTO(status,
        s_pwrPolicyWorkloadCombined1xRegimeSpaceIlwalidate(pCombined1x),
        s_pwrPolicyWorkloadCombined1xRegimeSpaceBuild_exit);

    //
    // TODO - Should sanity check that maxRatio is always
    // >= defaultRatio.
    //

    // Initialize masks of REGIMEs
    boardObjGrpMaskInit_E32(&childRegimeMask);
    boardObjGrpMaskInit_E32(&regimeNotInsertedMask);
    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpMaskCopy(&regimeNotInsertedMask,
            &pCombined1x->supportedRegimeMask),
        s_pwrPolicyWorkloadCombined1xRegimeSpaceBuild_exit);

    // Initialize parent to point to REGIME(DUMMY_ROOT)
    pParentRegimeData = &pCombined1x->rootRegimeData;

    //
    // Search graph starting at DUMMY_ROOT to find the set of
    // SEARCH_REGIMEs to use for this iteration.
    //
    while (LW_TRUE)
    {
        LwU8                           childRegimeId;
        LwU32                          freqMHz;
        LwU16                          d;

        // Build mask of potential child REGIMEs.
        PMU_ASSERT_OK_OR_GOTO(status,
            boardObjGrpMaskCopy(&childRegimeMask,
                &pParentRegimeData->regimeInitData.childRegimeMask),
            s_pwrPolicyWorkloadCombined1xRegimeSpaceBuild_exit);
        PMU_ASSERT_OK_OR_GOTO(status,
            boardObjGrpMaskAnd(&childRegimeMask,
                 &childRegimeMask, &regimeNotInsertedMask),
            s_pwrPolicyWorkloadCombined1xRegimeSpaceBuild_exit);

        //
        // Exit loop when no potential child REGIMEs remain -
        // i.e. have reached a leaf node in graph.
        //
        if (boardObjGrpMaskIsZero(&childRegimeMask))
        {
            break;
        }

        // Search among potential child REGIMEs for highest.
        childRegimeId = REGIME(MAX);
        freqMHz = 0;
        BOARDOBJGRPMASK_FOR_EACH_BEGIN(&childRegimeMask, d)
        {
            pRegimeData = &pCombined1x->regimeData[d];

            // Evaluate frequency endpoints of each child
            PMU_ASSERT_OK_OR_GOTO(status,
               s_pwrPolicyWorkloadCombined1xRegimeFreqEndPoints(
                   pCombined1x, pParentRegimeData, pRegimeData),
               s_pwrPolicyWorkloadCombined1xRegimeSpaceBuild_exit);

            //
            // To-Do : Build lowest Regime (in this case SEC_VMIN_FLOOR) only
            // if limits are satisfied at its frequency end points.
            //
            if ((pRegimeData->regimeFreq.freqMHz[pParentRegimeData->regimeInitData.singleIdx] !=
                    LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_SEARCH_REGIME_FREQ_ILWALID) &&
                (pRegimeData->regimeFreq.freqMHz[pParentRegimeData->regimeInitData.singleIdx] > freqMHz) /*
                TODO - Follow-on CL will account for priority between regimes here */)
            {
                childRegimeId = d;
                freqMHz =
                    pRegimeData->regimeFreq.freqMHz[pParentRegimeData->regimeInitData.singleIdx];
            }
        }
        BOARDOBJGRPMASK_FOR_EACH_END;

        //
        // Sanity check childRegimeId and regimeNum to ensure valid
        // after search above, and will be used to index into arrays
        // below.
        //
        PMU_ASSERT_TRUE_OR_GOTO(status,
            ((childRegimeId < REGIME(MAX)) &&
                (regimeNum < REGIME(MAX))),
            FLCN_ERR_ILWALID_STATE,
            s_pwrPolicyWorkloadCombined1xRegimeSpaceBuild_exit);

        //Insert child Regime
        pRegimeData   = &pCombined1x->regimeData[childRegimeId];
        pSearchRegime = &pCombined1x->searchRegimes[regimeNum++];
        pSearchRegime->regimeId = childRegimeId;

        // Since child regime has been inserted, clear the mask at that index.
        boardObjGrpMaskBitClr(&regimeNotInsertedMask, childRegimeId);

        // Set freqStartIdx for Parent if the parent is a valid regime
        if (pParentSearchRegime != NULL)
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                s_pwrPolicyWorkloadCombined1xGetClkDomainIdx(
                pCombined1x, pParentRegimeData->regimeInitData.singleIdx,
                &clkDomainIdx),
            s_pwrPolicyWorkloadCombined1xRegimeSpaceBuild_exit);
            PMU_ASSERT_OK_OR_GOTO(status,
                s_pwrPolicyWorkloadCombined1xGetIndexByFreqMHz(
                clkDomainIdx,
                pRegimeData->regimeFreq.freqMHz[pParentRegimeData->regimeInitData.singleIdx],
                LW_FALSE, &clkIdx),
            s_pwrPolicyWorkloadCombined1xRegimeSpaceBuild_exit);
            pParentSearchRegime->clkFreqStartIdx = clkIdx + 1;
        }

        // Set freqEndIdx for Child.
        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrPolicyWorkloadCombined1xGetClkDomainIdx(
                pCombined1x, pRegimeData->regimeInitData.singleIdx,
                &clkDomainIdx),
            s_pwrPolicyWorkloadCombined1xRegimeSpaceBuild_exit);
        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrPolicyWorkloadCombined1xGetIndexByFreqMHz(
                clkDomainIdx,
                pRegimeData->regimeFreq.freqMHz[pRegimeData->regimeInitData.singleIdx],
                LW_FALSE, &clkIdx),
            s_pwrPolicyWorkloadCombined1xRegimeSpaceBuild_exit);
        pSearchRegime->clkFreqEndIdx = clkIdx;

        // Make the newly inserted regime the parent for the next iteration
        pParentRegimeData = pRegimeData;
        pParentSearchRegime = pSearchRegime;
    }

    // Sanity check that we found at least one REGIME above.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((regimeNum > 0) &&
         (pSearchRegime != NULL) &&
         (pRegimeData != NULL)),
        FLCN_ERR_ILWALID_STATE,
        s_pwrPolicyWorkloadCombined1xRegimeSpaceBuild_exit);

    // Set Minimum frequency of last REGIME from PSTATE range.
    {
        CLK_DOMAIN                *pDomain;
        PERF_LIMITS_PSTATE_RANGE   pstateRange;
        PERF_LIMITS_VF_RANGE       vfDomainRange;
        PSTATE                    *pPstate;

        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrPolicyWorkloadCombined1xGetClkDomainIdx(
                pCombined1x,
                pRegimeData->regimeInitData.singleIdx,
                &clkDomainIdx),
            s_pwrPolicyWorkloadCombined1xRegimeSpaceBuild_exit);

        pPstate = PSTATE_GET_BY_LEVEL(perfGetPstateCount() - 1);
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pPstate != NULL), FLCN_ERR_ILWALID_STATE,
             s_pwrPolicyWorkloadCombined1xRegimeSpaceBuild_exit);
        PERF_LIMITS_RANGE_SET_VALUE(&pstateRange,
            BOARDOBJ_GET_GRP_IDX(&pPstate->super));

        vfDomainRange.idx = BOARDOBJ_GRP_IDX_TO_8BIT(clkDomainIdx);
        PMU_ASSERT_OK_OR_GOTO(status,
            perfLimitsPstateIdxRangeToFreqkHzRange(&pstateRange,
                &vfDomainRange),
            s_pwrPolicyWorkloadCombined1xRegimeSpaceBuild_exit);

        pDomain = CLK_DOMAIN_GET(clkDomainIdx);
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pDomain != NULL),
            FLCN_ERR_ILWALID_STATE,
            s_pwrPolicyWorkloadCombined1xRegimeSpaceBuild_exit);

        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrPolicyWorkloadCombined1xGetIndexByFreqMHz(
                clkDomainIdx,
                (vfDomainRange.values[LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MIN] /
                clkDomainFreqkHzScaleGet(pDomain)), LW_FALSE, &clkIdx),
            s_pwrPolicyWorkloadCombined1xRegimeSpaceBuild_exit);
        pSearchRegime->clkFreqStartIdx = clkIdx;
    }
    pCombined1x->numRegimes = regimeNum;

    //
    // Complete the SEARCH_REGIMEs by building their tupleIdxs and
    // sanity checking for monotonicity.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        s_pwrPolicyWorkloadCombined1xSearchTupleIdxBuild(pCombined1x),
        s_pwrPolicyWorkloadCombined1xRegimeSpaceBuild_exit);

s_pwrPolicyWorkloadCombined1xRegimeSpaceBuild_exit:
    return status;
}

/*!
 * Helper function to populate tuple indices for the search regimes
 *
 */
static FLCN_STATUS
s_pwrPolicyWorkloadCombined1xSearchTupleIdxBuild
(
    PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x
)
{
    LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_SEARCH_REGIME *pSearchRegime;
    LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_SEARCH_REGIME *pChildSearchRegime = NULL;
    LwU32 numFreqPoints;
    LwS8  i;
    FLCN_STATUS status = FLCN_OK;

    // Get the index of the lowest regime
    for (i = (pCombined1x->numRegimes - 1); i >= 0; i--)
    {
        pSearchRegime = &pCombined1x->searchRegimes[i];

        // Sanity check SEARCH_REGIME clkFreq idxs.
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pSearchRegime->clkFreqEndIdx >= pSearchRegime->clkFreqStartIdx),
            FLCN_ERR_ILWALID_STATE,
            s_pwrPolicyWorkloadCombined1xSearchTupleIdxBuild_exit);

        numFreqPoints = pSearchRegime->clkFreqEndIdx -
            pSearchRegime->clkFreqStartIdx;
        if (pChildSearchRegime == NULL)
        {
            pSearchRegime->tupleStartIdx = 0;
        }
        else
        {
            pSearchRegime->tupleStartIdx =
                pChildSearchRegime->tupleEndIdx + 1;
        }
        pSearchRegime->tupleEndIdx =
                pSearchRegime->tupleStartIdx + numFreqPoints;

        //
        // Compute the @ref
        // LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_SINGLES_FREQ_TUPLE
        // corresponding to both @ref tupleStartIdx and @ref
        // tupleEndIdx
        //
        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrPolicyWorkloadCombined1xSearchRegimeGetFreqTupleByTupleIdx(pCombined1x,
                pSearchRegime, pSearchRegime->tupleStartIdx,
                &pSearchRegime->freqTuples[SEARCH_REGIME_FREQ_TUPLE_IDX(START)]),
            s_pwrPolicyWorkloadCombined1xSearchTupleIdxBuild_exit);
        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrPolicyWorkloadCombined1xSearchRegimeGetFreqTupleByTupleIdx(pCombined1x,
                pSearchRegime, pSearchRegime->tupleEndIdx,
                &pSearchRegime->freqTuples[SEARCH_REGIME_FREQ_TUPLE_IDX(END)]),
            s_pwrPolicyWorkloadCombined1xSearchTupleIdxBuild_exit);

        //
        // Sanity check child freqTuples[END] vs. parent
        // freqTuples[START] for monotonicity.
        //
        if (pChildSearchRegime != NULL)
        {
            LwBool bAtLeastOneSingleLessThan = LW_FALSE;
            LwU8   singleIdx;

            PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_INDEX_BEGIN(pCombined1x, singleIdx)
            {
                // Ensure that every child SINGLE's frequency is <= parent.
                PMU_ASSERT_TRUE_OR_GOTO(status,
                    (pChildSearchRegime->freqTuples[SEARCH_REGIME_FREQ_TUPLE_IDX(END)].freqMHz[singleIdx] <=
                        pSearchRegime->freqTuples[SEARCH_REGIME_FREQ_TUPLE_IDX(START)].freqMHz[singleIdx]),
                    FLCN_ERR_ILWALID_STATE,
                    s_pwrPolicyWorkloadCombined1xSearchTupleIdxBuild_exit);

                bAtLeastOneSingleLessThan |=
                    (pChildSearchRegime->freqTuples[SEARCH_REGIME_FREQ_TUPLE_IDX(END)].freqMHz[singleIdx] <
                        pSearchRegime->freqTuples[SEARCH_REGIME_FREQ_TUPLE_IDX(START)].freqMHz[singleIdx]);
            }
            PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_INDEX_END;

            // Ensure that at least one child SINGLE's frequency is strictly < parent.
            PMU_ASSERT_TRUE_OR_GOTO(status,
                bAtLeastOneSingleLessThan,
                FLCN_ERR_ILWALID_STATE,
                s_pwrPolicyWorkloadCombined1xSearchTupleIdxBuild_exit);
        }

        pChildSearchRegime = pSearchRegime;
    }

s_pwrPolicyWorkloadCombined1xSearchTupleIdxBuild_exit:
    return status;
}

/*!
 * Scales the final output clock value to account for ramp rate control
 *
 * @param[in]     pCombined1x   PWR_POLICY_WORKLOAD_COMBINED_1X pointer.
 * @param[in/out] pDomGrpLimits
 *     RM_PMU_PERF_DOMAIN_GROUP_LIMITS structure which holds the desired maximum
 *     output clocks as callwlated by the WORKLOAD power policy algorithm. This
 *     is the value which should be scaled/colwerted.
 *
 * @return FLCN_OK
 *     Successfully scaled clocks or no scaling required.
 * @return FLCN_ERR_ILWALID_STATE
 *     Could not get a valid pstate.
 * @return Other errors
 *     Propagates return values from various calls.
 */
static FLCN_STATUS
s_pwrPolicyWorkloadCombined1xScaleClocks
(
    PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x,
    PERF_DOMAIN_GROUP_LIMITS        *pDomGrpLimits
)
{
    PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1xPri;
    PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1x;
    CLK_DOMAIN                    *pDomain;
    PSTATE                        *pPstate;
    PERF_LIMITS_PSTATE_RANGE       pstateRange;
    PERF_LIMITS_VF                 vfDomain;
    LwUFXP52_12                    scaledDiff;
    LwU64                          temp;
    LwU32                          oldLimitValue =
        pCombined1x->domGrpLimitsUnQuant.values[RM_PMU_DOMAIN_GROUP_GPC2CLK];
    LwU32                          newLimitValue =
        pDomGrpLimits->values[RM_PMU_DOMAIN_GROUP_GPC2CLK];
    FLCN_STATUS                    status = FLCN_OK;
    LwU8                           singleIdx;
    LwBool                         bRegimesEnabled;

    //
    // Ramp rate requires that both old limit (i.e limit issued in the last
    // iteration) and new limit values are != _DISABLE.
    //
    if ((RM_PMU_PREF_DOMAIN_GROUP_LIMIT_VALUE_DISABLE == newLimitValue) ||
        (RM_PMU_PREF_DOMAIN_GROUP_LIMIT_VALUE_DISABLE ==
        pCombined1x->super.domGrpLimits.values[RM_PMU_DOMAIN_GROUP_GPC2CLK]))
    {
        goto s_pwrPolicyWorkloadCombined1xScaleClocks_exit;
    }

    // 1. Scale for clock up
    if ((newLimitValue > oldLimitValue) &&
        (pCombined1x->clkPrimaryUpScale != LW_TYPES_FXP_ZERO))
    {
        //
        // scaledDiff = clkPrimaryUpScale * (newLimitValue - oldLimitValue)
        //
        // Numerical Analysis:
        // clkPrimaryUpScale is 4.12 and (newLimitValue - oldLimitValue) is 32.0
        // 4.12 * 32.0 = 36.12
        // scaledDiff is 52.12 and hence we will not overflow
        //
        temp       = (LwU64)(newLimitValue - oldLimitValue);
        scaledDiff = (LwUFXP52_12)pCombined1x->clkPrimaryUpScale;
        lwU64Mul(&scaledDiff, &scaledDiff, &temp);

        temp = LW_TYPES_UFXP_X_Y_TO_U64_ROUNDED(52, 12, scaledDiff);
        PMU_ASSERT_OK_OR_GOTO(status,
            (LwU64_HI32(temp) != 0) ? FLCN_ERROR : FLCN_OK,
            s_pwrPolicyWorkloadCombined1xScaleClocks_exit);
        newLimitValue = oldLimitValue + LwU64_LO32(temp);
        PMU_ASSERT_OK_OR_GOTO(status,
            (newLimitValue < oldLimitValue) ? FLCN_ERROR : FLCN_OK,
            s_pwrPolicyWorkloadCombined1xScaleClocks_exit);
    }
    // 2. Scale for clock down.
    else if ((newLimitValue < oldLimitValue) &&
             (pCombined1x->clkPrimaryDownScale != LW_TYPES_FXP_ZERO))
    {
        //
        // scaledDiff = clkPrimaryDownScale * (oldLimitValue - newLimitValue)
        //
        // Numerical Analysis:
        // clkPrimaryDownScale is 4.12 and (oldLimitValue - newLimitValue) is 32.0
        // 4.12 * 32.0 = 36.12
        // scaledDiff is 52.12 and hence we will not overflow
        //
        temp       = (LwU64)(oldLimitValue - newLimitValue);
        scaledDiff = (LwUFXP52_12)pCombined1x->clkPrimaryDownScale;
        lwU64Mul(&scaledDiff, &scaledDiff, &temp);

        temp = LW_TYPES_UFXP_X_Y_TO_U64_ROUNDED(52, 12, scaledDiff);
        PMU_ASSERT_OK_OR_GOTO(status,
            (LwU64_HI32(temp) != 0) ? FLCN_ERROR : FLCN_OK,
            s_pwrPolicyWorkloadCombined1xScaleClocks_exit);
        newLimitValue = oldLimitValue - LwU64_LO32(temp);
        PMU_ASSERT_OK_OR_GOTO(status,
            (newLimitValue > oldLimitValue) ? FLCN_ERROR : FLCN_OK,
            s_pwrPolicyWorkloadCombined1xScaleClocks_exit);
    }

    //
    // If scaling did not affect the GPCCLK value, no further scaling
    // needs to be applied.
    //
    if (newLimitValue == pDomGrpLimits->values[RM_PMU_DOMAIN_GROUP_GPC2CLK])
    {
        goto s_pwrPolicyWorkloadCombined1xScaleClocks_exit;
    }

    // 3. Quantize-down freq over P0 PSTATE ranges and VF lwrve data.
    pPstate = PSTATE_GET_BY_LEVEL(perfGetPstateCount() - 1);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pPstate != NULL),
        FLCN_ERR_ILWALID_STATE,
        s_pwrPolicyWorkloadCombined1xScaleClocks_exit);
    PERF_LIMITS_RANGE_SET_VALUE(&pstateRange,
        BOARDOBJ_GET_GRP_IDX(&pPstate->super));

    PWR_POLICY_WORKLOAD_COMBINED_1X_GET_SINGLE_1X(
        pCombined1x, SINGLE1X(PRI), &pSingle1xPri, status,
        s_pwrPolicyWorkloadCombined1xScaleClocks_exit);
    vfDomain.idx = BOARDOBJ_GRP_IDX_TO_8BIT(pSingle1xPri->clkDomainIdx);
    vfDomain.value = newLimitValue;
    PMU_ASSERT_OK_OR_GOTO(status,
        perfLimitsFreqkHzQuantize(&vfDomain,
                                  &pstateRange,
                                  LW_TRUE),
        s_pwrPolicyWorkloadCombined1xScaleClocks_exit);

    //
    // Propagate the quantized, scaled primary clock to any secondary clocks
    // based on either REGIME or non-REGIME logic, as applicable.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        s_pwrPolicyWorkloadCombined1xRegimesEnabled(pCombined1x, &bRegimesEnabled),
        s_pwrPolicyWorkloadCombined1xScaleClocks_exit);
    if (bRegimesEnabled)
    {
        LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_SINGLES_FREQ_TUPLE regimeFreq;
        LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_REGIMES_STATUS_TUPLE
              *pTupleStatus;
        LwU16  tupleIdx;
        LwU8   searchRegimeIdx;

        pDomain = CLK_DOMAIN_GET(pSingle1xPri->clkDomainIdx);
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pDomain != NULL),
            FLCN_ERR_ILWALID_STATE,
            s_pwrPolicyWorkloadCombined1xScaleClocks_exit);

        // 3. Colwert the primary clock to a tupleIdx
        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrPolicyWorkloadCombined1xGetTupleIdxByPrimaryFreq(pCombined1x,
                vfDomain.value / clkDomainFreqkHzScaleGet(pDomain),
                (pDomGrpLimits->values[RM_PMU_DOMAIN_GROUP_GPC2CLK] < oldLimitValue),
                &tupleIdx),
            s_pwrPolicyWorkloadCombined1xScaleClocks_exit);

        // 4. Colwert the tupleIdx into a full set of clock frequencies
        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrPolicyWorkloadCombined1xGetFreqByTupleIdx(pCombined1x,
                tupleIdx, &regimeFreq),
            s_pwrPolicyWorkloadCombined1xScaleClocks_exit);
        PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_BEGIN(
            pCombined1x, pSingle1x, singleIdx, status,
            s_pwrPolicyWorkloadCombined1xScaleClocks_exit)
        {
            RM_PMU_PMGR_PWR_POLICY_DOMGRP_MAP_ENTRY domGrpData;

            pDomain = CLK_DOMAIN_GET(pSingle1x->clkDomainIdx);
            PMU_ASSERT_TRUE_OR_GOTO(status,
                (pDomain != NULL),
                FLCN_ERR_ILWALID_STATE,
                s_pwrPolicyWorkloadCombined1xScaleClocks_exit);

            // Get the domain group data from clkDomainIdx
            PMU_ASSERT_OK_OR_GOTO(status,
                pwrPolicies35DomGrpDataGet(
                    PWR_POLICIES_35_GET(),
                    &domGrpData,
                    pSingle1x->clkDomainIdx),
                s_pwrPolicyWorkloadCombined1xScaleClocks_exit);

            // Colwert MHz -> kHz
            pDomGrpLimits->values[domGrpData.domGrpLimitIdx] =
                (regimeFreq.freqMHz[singleIdx] * clkDomainFreqkHzScaleGet(pDomain));
        }
        PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_END;

        // 5.  Save scaling tupleStatus
        pTupleStatus =
            &pCombined1x->regimesStatus.tuples[LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_REGIMES_STATUS_TUPLE_IDX_SCALING];
        pTupleStatus->tupleIdx = tupleIdx;
        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrPolicyWorkloadCombined1xGetSearchRegimeIdxByTupleIdx(
            pCombined1x, tupleIdx, &searchRegimeIdx),
            s_pwrPolicyWorkloadCombined1xScaleClocks_exit);

        // Confirm match found above.
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (searchRegimeIdx < pCombined1x->numRegimes), FLCN_ERR_ILWALID_INDEX,
            s_pwrPolicyWorkloadCombined1xScaleClocks_exit);
        pTupleStatus->searchRegimeIdx =
            pCombined1x->searchRegimes[searchRegimeIdx].regimeId;
    }
    else
    {
        PERF_LIMITS_VF_ARRAY           vfArrayIn;
        PERF_LIMITS_VF_ARRAY           vfArrayOut;
        BOARDOBJGRPMASK_E32            priClkDomainMask;
        BOARDOBJGRPMASK_E32            secClkDomainMask;

        // Init the primary and secondary clock domain masks
        boardObjGrpMaskInit_E32(&priClkDomainMask);
        boardObjGrpMaskInit_E32(&secClkDomainMask);

        // Init the input/output struct.
        PERF_LIMITS_VF_ARRAY_INIT(&vfArrayIn);
        PERF_LIMITS_VF_ARRAY_INIT(&vfArrayOut);

        //
        // Populate the primary and secondary clock domain masks and copy it into the
        // input/output struct
        //
        boardObjGrpMaskBitSet(&priClkDomainMask, pSingle1xPri->clkDomainIdx);
        vfArrayIn.pMask = &priClkDomainMask;

        PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SECONDARY_SINGLE_1X_BEGIN(
            pCombined1x, pSingle1x, singleIdx, status,
            s_pwrPolicyWorkloadCombined1xScaleClocks_exit)
        {
            boardObjGrpMaskBitSet(&secClkDomainMask, pSingle1x->clkDomainIdx);
        }
        PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SECONDARY_SINGLE_1X_END;

        vfArrayOut.pMask = &secClkDomainMask;

        // 4. Propagate primary clock to all other clocks.
        vfArrayIn.values[pSingle1xPri->clkDomainIdx] = vfDomain.value;
        if (!boardObjGrpMaskIsZero(&secClkDomainMask))
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                perfLimitsFreqPropagate(&pstateRange,
                                        &vfArrayIn,
                                        &vfArrayOut,
                                        LW2080_CTRL_CLK_CLK_PROP_TOP_IDX_ILWALID),
                s_pwrPolicyWorkloadCombined1xScaleClocks_exit);
        }

        // 5. Set the new frequency values
        PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_BEGIN(
            pCombined1x, pSingle1x, singleIdx, status,
            s_pwrPolicyWorkloadCombined1xScaleClocks_exit)
        {
            RM_PMU_PMGR_PWR_POLICY_DOMGRP_MAP_ENTRY domGrpData;

            pDomain = CLK_DOMAIN_GET(pSingle1x->clkDomainIdx);
            PMU_ASSERT_TRUE_OR_GOTO(status,
                (pDomain != NULL),
                FLCN_ERR_ILWALID_STATE,
                s_pwrPolicyWorkloadCombined1xScaleClocks_exit);

            // Get the domain group data from clkDomainIdx
            PMU_ASSERT_OK_OR_GOTO(status,
                pwrPolicies35DomGrpDataGet(
                    PWR_POLICIES_35_GET(),
                    &domGrpData,
                    pSingle1x->clkDomainIdx),
                s_pwrPolicyWorkloadCombined1xScaleClocks_exit);

            //
            // singleIdx is 0 for primary clock and we need to get the freq from
            // vfarrayIn for primary clock.
            //
            if (singleIdx == 0)
            {
                pDomGrpLimits->values[domGrpData.domGrpLimitIdx] =
                    vfArrayIn.values[pSingle1x->clkDomainIdx];
            }
            else
            {
                if (pDomGrpLimits->values[domGrpData.domGrpLimitIdx] >=
                        pCombined1x->estMetrics.pSingles[singleIdx].freqFloorMHz *
                            clkDomainFreqkHzScaleGet(pDomain))
                {
                    pDomGrpLimits->values[domGrpData.domGrpLimitIdx] =
                        LW_MAX(vfArrayOut.values[pSingle1x->clkDomainIdx],
                            pCombined1x->estMetrics.pSingles[singleIdx].freqFloorMHz *
                                clkDomainFreqkHzScaleGet(pDomain));
                }
                else
                {
                    pDomGrpLimits->values[domGrpData.domGrpLimitIdx] =
                        vfArrayOut.values[pSingle1x->clkDomainIdx];
                }
            }
        }
        PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_END;
    }

s_pwrPolicyWorkloadCombined1xScaleClocks_exit:
    if (status == FLCN_OK)
    {
        //
        // Store the scaled newLimit value so that it can be used as old Limit in the
        // next evaluation cycle.
        //
        pCombined1x->domGrpLimitsUnQuant.values[RM_PMU_DOMAIN_GROUP_GPC2CLK] =
            newLimitValue;
    }

    return status;
}

/*!
 * @copydoc BoardObjVirtualTableGetInterfaceFromBoardObj
 */
static BOARDOBJ_INTERFACE *
s_pwrPolicyGetInterfaceFromBoardObj_WORKLOAD_COMBINED_1X
(
    BOARDOBJ   *pBoardObj,
    LwU8        interfaceType
)
{
    PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x =
        (PWR_POLICY_WORKLOAD_COMBINED_1X *)pBoardObj;

    switch (interfaceType)
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_INTERFACE_TYPE_PERF_CF_PWR_MODEL:
        {
            return &pCombined1x->pwrModel.super;
        }
    }

    PMU_TRUE_BP();
    return NULL;
}

/*!
 * Helper function to get the floor freq and power at that freq.
 *
 * @param[in]   pCombined1x       PWR_POLICY_WORKLOAD_COMBINED_1X pointer.
 * @param[in]   pPstateRange      PERF_LIMITS_PSTATE_RANGE pointer specifying
 *     the pstate range in which to look for the freq
 * @param[in]   singleIdx         Index of the SINGLE_1X metrics in the
 *     pSingles[] array
 * @param[out]  pEstFloorMetrics  Pointer to buffer in which to return the
 *     floor freq, power and other estimated metrics.
 *
 * @return FLCN_OK
 *     Freq floor state get operation completed successfully.
 * @return FLCN_ERR_ILWALID_STATE
 *     Encountered an invalid state during computation.
 * @return Other errors
 *     Propagates return values from various calls.
 *
 */
static FLCN_STATUS
s_pwrPolicyWorkloadCombined1xFreqFloorStateGet
(
    PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x,
    PWR_POLICY_WORKLOAD_SINGLE_1X   *pSingle1x,
    PERF_LIMITS_PSTATE_RANGE        *pPstateRange,
    LwBoardObjIdx                    singleIdx,
    LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_ESTIMATED_METRICS_WORKLOAD_SINGLE_1X *pEstFloorMetrics
)
{
    CLK_DOMAIN    *pDomain;
    PERF_LIMITS_VF vfRail;
    PERF_LIMITS_VF vfDomain;
    FLCN_STATUS    status;

    // Initialize the required parameters.
    pDomain = CLK_DOMAIN_GET(pSingle1x->clkDomainIdx);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDomain != NULL),
        FLCN_ERR_ILWALID_STATE,
        s_pwrPolicyWorkloadCombined1xFreqFloorStateGet_exit);

    vfRail.idx   = pSingle1x->voltRailIdx;
    vfDomain.idx = pSingle1x->clkDomainIdx;
    vfRail.value = pCombined1x->obsMetrics.pSingles[singleIdx].independentDomainsVmin.voltFlooruV;
    PMU_ASSERT_OK_OR_GOTO(status,
        perfLimitsVoltageuVToFreqkHz(&vfRail, pPstateRange, &vfDomain, LW_TRUE, LW_TRUE),
        s_pwrPolicyWorkloadCombined1xFreqFloorStateGet_exit);

    // Colwert the freq into MHz
    pEstFloorMetrics->freqMHz =
        vfDomain.value / clkDomainFreqkHzScaleGet(pDomain);

    //
    // Temporary step needed until pwrPolicyPerfCfPwrModelScaleMetrics_WORKLOAD_SINGLE_1X
    // is replaced with pwrPolicyPerfCfPwrModelScale_WORKLOAD_SINGLE_1X
    //
    pEstFloorMetrics->independentDomainsVmin = pCombined1x->obsMetrics.pSingles[singleIdx].independentDomainsVmin;

    // Get the estimated metrics at the freq floor
    PMU_ASSERT_OK_OR_GOTO(status,
        pwrPolicyPerfCfPwrModelScaleMetrics_WORKLOAD_SINGLE_1X(
            &pSingle1x->pwrModel,
            &pCombined1x->obsMetrics.pSingles[singleIdx].super,
            &pEstFloorMetrics->super),
        s_pwrPolicyWorkloadCombined1xFreqFloorStateGet_exit);

s_pwrPolicyWorkloadCombined1xFreqFloorStateGet_exit:
    return status;
}

/*!
 * Callwlates the lpwr residency in the period from last policy evaluation
 * to current policy evaluation.
 *
 * @param[in/out] pCombined1x  PWR_POLICY_WORKLOAD_COMBINED_1X *pointer.
 * @param[in]     engineId     RM_PMU_LPWR_CTRL_ID_xyz.
 */
static FLCN_STATUS
s_pwrPolicyWorkloadCombined1xResidencyGet
(
    PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x
)
{
    LwUFXP20_12 residency;
    LwU8        i;
    LwU8        engineId;

    for (engineId = 0; engineId < RM_PMU_LPWR_CTRL_ID_MAX; engineId++)
    {
        //
        // As a temporary WAR for bug 200729026, removing the residency
        // computation of engines that are not lwrrently used by PWR code.
        // TODO: LPWR team to root cause and provide actual fix.
        //
        if (pgIsCtrlSupported(engineId) &&
            pwrPolicyWorkloadCombined1xPgEngineIdIsSupported(engineId))
        {
            residency = pwrPolicyWorkloadResidencyCompute(
                &pCombined1x->obsMetrics.lpwrTime[engineId],
                engineId);
        }
        else
        {
            residency = 0;
        }

        PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_INDEX_BEGIN(pCombined1x, i)
        {
            pCombined1x->obsMetrics.pSingles[i].lpwrResidency[engineId] =
                residency;
        }
        PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_INDEX_END;
    }
    return FLCN_OK;
}

/*!
 * Wrapper function to @ref perfLimitsFreqPropagate to hide init data
 *
 * @param[in]   propFromFreqMHz Frequency to propogate from
 * @param[in]   propFromClkIdx  Clock index to propogate from
 * @param[in]   clkPropTopIdx   Clock index to propogate to
 * @param[in]   propToClkIdx    Clock [prop topology to use
 * @param[out]  pPropToFreqMHz  Propogated Frequency Value
 *
 * @return FLCN_OK
 *     Successfully propogated frequency
 * @return Other errors
 *     Propagates return values from various calls.
 *
 */
static FLCN_STATUS
s_pwrPolicyWorkloadCombined1xClkPropagate
(
    LwU32           propFromFreqMHz,
    LwBoardObjIdx   propFromClkIdx,
    LwBoardObjIdx   propToClkIdx,
    LwBoardObjIdx   clkPropTopIdx,
    LwU32          *pPropToFreqMHz
)
{
    PSTATE                   *pPstate;
    PERF_LIMITS_PSTATE_RANGE  pstateRange;
    PERF_LIMITS_VF_ARRAY      vfArrayIn;
    PERF_LIMITS_VF_ARRAY      vfArrayOut;
    BOARDOBJGRPMASK_E32       propFromClkDomainMask;
    BOARDOBJGRPMASK_E32       propToClkDomainMask;
    CLK_DOMAIN               *pDomain;
    FLCN_STATUS               status;

    // Sanity-check input parameters.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pPropToFreqMHz != NULL) &&
         ((clkPropTopIdx == LW2080_CTRL_CLK_CLK_PROP_TOP_IDX_ILWALID) ||
          (clkPropTopIdx != PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_CLK_PROP_TOP_IDX_PARENT_FLOOR))),
        FLCN_ERR_ILWALID_ARGUMENT,
        s_pwrPolicyWorkloadCombined1xClkPropagate_exit);

    if (propFromFreqMHz == 0)
    {
        *pPropToFreqMHz = 0;
        status = FLCN_OK;
        goto s_pwrPolicyWorkloadCombined1xClkPropagate_exit;
    }

    // Init the primary and secondary clock domain masks
    boardObjGrpMaskInit_E32(&propFromClkDomainMask);
    boardObjGrpMaskInit_E32(&propToClkDomainMask);

    // Init the input/output struct.
    PERF_LIMITS_VF_ARRAY_INIT(&vfArrayIn);
    PERF_LIMITS_VF_ARRAY_INIT(&vfArrayOut);

    pDomain = CLK_DOMAIN_GET(propFromClkIdx);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDomain != NULL),
        FLCN_ERR_ILWALID_STATE,
        s_pwrPolicyWorkloadCombined1xClkPropagate_exit);

    pPstate = PSTATE_GET_BY_LEVEL(perfGetPstateCount() - 1);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pPstate != NULL), FLCN_ERR_ILWALID_STATE,
        s_pwrPolicyWorkloadCombined1xClkPropagate_exit);

    PERF_LIMITS_RANGE_SET_VALUE(&pstateRange,
        BOARDOBJ_GET_GRP_IDX(&pPstate->super));

    boardObjGrpMaskBitSet(&propFromClkDomainMask, propFromClkIdx);
    vfArrayIn.pMask = &propFromClkDomainMask;
    vfArrayIn.values[propFromClkIdx] = propFromFreqMHz *
        clkDomainFreqkHzScaleGet(pDomain);

    // populate vfarraymask.out
    boardObjGrpMaskBitSet(&propToClkDomainMask, propToClkIdx);
    vfArrayOut.pMask = &propToClkDomainMask;

    PMU_ASSERT_OK_OR_GOTO(status,
        perfLimitsFreqPropagate(&pstateRange,
            &vfArrayIn, &vfArrayOut, clkPropTopIdx),
        s_pwrPolicyWorkloadCombined1xClkPropagate_exit);

    pDomain = CLK_DOMAIN_GET(propToClkIdx);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDomain != NULL),
        FLCN_ERR_ILWALID_STATE,
        s_pwrPolicyWorkloadCombined1xClkPropagate_exit);

    *pPropToFreqMHz = vfArrayOut.values[propToClkIdx] /
        clkDomainFreqkHzScaleGet(pDomain);
s_pwrPolicyWorkloadCombined1xClkPropagate_exit:
    return status;
}

/*!
 * @brief Helper interface to get the index of the input frequency 
 * and clock domain index
 *
 * @param[in]   clkDomainIdx clock domain index
 * @param[in]   freqMhz      Input frequency (MHz)
 * @param[in]   bFloor
 *     Boolean indicating whether the index should be found via a floor
 *     (LW_TRUE) or ceiling (LW_FALSE) to the next closest supported value.
 * @param[out]  pClkIdx
 *     Index at which the input frequency is mapped
 *
 * @return FLCN_OK
 *      Index returned successfully
 * @return other errors
 *      Unexpected errors.
 */
static FLCN_STATUS
s_pwrPolicyWorkloadCombined1xGetIndexByFreqMHz
(
    LwBoardObjIdx    clkDomainIdx,
    LwU32            freqMHz,
    LwBool           bFloor,
    LwU16           *pClkIdx
)
{
    CLK_DOMAIN        *pDomain;
    CLK_DOMAIN_PROG   *pDomainProg;
    FLCN_STATUS        status;

    pDomain = CLK_DOMAIN_GET(clkDomainIdx);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDomain != NULL),
        FLCN_ERR_ILWALID_STATE,
        s_pwrPolicyWorkloadCombined1xGetIndexByFreqMHz_exit);

    pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDomainProg != NULL),
        FLCN_ERR_ILWALID_STATE,
        s_pwrPolicyWorkloadCombined1xGetIndexByFreqMHz_exit);

    *pClkIdx = clkDomainProgGetIndexByFreqMHz(pDomainProg,
        freqMHz, bFloor);

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (*pClkIdx != LW_U16_MAX), FLCN_ERR_ILWALID_INDEX,
        s_pwrPolicyWorkloadCombined1xGetIndexByFreqMHz_exit);

s_pwrPolicyWorkloadCombined1xGetIndexByFreqMHz_exit:
    return status;
}

/*!
 * Helper function to get a frequency tuple from the tupleIdx
 *
 * @param[in]   pCombined1x   PWR_POLICY_WORKLOAD_COMBINED_1X pointer.
 * @param[in]   tupleIdx      tuple index
 * @param[out]  freqMHz       array of SINGLE1X(MAX)which holds the
 *                            frequency values
 * @return FLCN_OK
 *     operation computed successfully.
 * @return Other errors
 *     Propagates return values from various calls.
 */
static FLCN_STATUS
s_pwrPolicyWorkloadCombined1xGetFreqByTupleIdx
(
    PWR_POLICY_WORKLOAD_COMBINED_1X                                            *pCombined1x,
    LwU16                                                                       tupleIdx,
    LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_SINGLES_FREQ_TUPLE *pRegimeFreq
)
{
    LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_SEARCH_REGIME
                                  *pSearchRegime;
    LwU8                           searchRegimeIdx = LW_U8_MAX;
    FLCN_STATUS                    status = FLCN_OK;

    //
    // Find the searchRegimeIdx for the SEARCH_REGIME which
    // contains the requested tupleIdx.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        s_pwrPolicyWorkloadCombined1xGetSearchRegimeIdxByTupleIdx(pCombined1x,
        tupleIdx, &searchRegimeIdx),
        s_pwrPolicyWorkloadCombined1xGetFreqByTupleIdx_exit);

    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((searchRegimeIdx < pCombined1x->numRegimes) &&
         ((pSearchRegime = &pCombined1x->searchRegimes[searchRegimeIdx]) != NULL)),
        FLCN_ERR_ILWALID_STATE,
        s_pwrPolicyWorkloadCombined1xGetFreqByTupleIdx_exit);

    //
    // Call into SEARCH_REGIME helper to colwert tupleIdx to full @ref
    // LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_SINGLES_FREQ_TUPLE.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        s_pwrPolicyWorkloadCombined1xSearchRegimeGetFreqTupleByTupleIdx
                          (pCombined1x, pSearchRegime, tupleIdx, pRegimeFreq),
        s_pwrPolicyWorkloadCombined1xGetFreqByTupleIdx_exit);

s_pwrPolicyWorkloadCombined1xGetFreqByTupleIdx_exit:
    return status;
}

/*!
 * Helper function to get a frequency tuple from the tupleIdx
 *
 * @param[in]   pCombined1x   PWR_POLICY_WORKLOAD_COMBINED_1X pointer.
 * @param[in]   pSearchRegime
 *     LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_SEARCH_REGIME
 *     structure pointer corresponding to the tupleIdx
 * @param[in]   tupleIdx      tuple index
 * @param[out]  freqMHz       array of SINGLE1X(MAX)which holds the
 *                            frequency values
 * @return FLCN_OK
 *     operation computed successfully.
 * @return Other errors
 *     Propagates return values from various calls.
 */
static FLCN_STATUS
s_pwrPolicyWorkloadCombined1xSearchRegimeGetFreqTupleByTupleIdx
(
    PWR_POLICY_WORKLOAD_COMBINED_1X                                            *pCombined1x,
    LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_SEARCH_REGIME             *pSearchRegime,
    LwU16                                                                       tupleIdx,
    LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_SINGLES_FREQ_TUPLE *pRegimeFreq
)
{
    PWR_POLICY_WORKLOAD_SINGLE_1X               *pSingle1xPri;
    PWR_POLICY_WORKLOAD_SINGLE_1X               *pSingle1xSec = NULL;
    PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_DATA *pRegimeData;
    CLK_DOMAIN                                  *pDomain;
    CLK_DOMAIN_PROG                             *pDomainProg;
    PSTATES                                     *pPstates;
    PERF_LIMITS_PSTATE_RANGE                     pstateRange;
    LwU32                                        propFromFreqMHz;
    LwU16                                        clkFreqIdx;
    LwBoardObjIdx                                clkDomainIdx;
    LwU8                                         singleIdx;
    FLCN_STATUS                                  status = FLCN_OK;

    // Sanity-check input arguments
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pRegimeFreq != NULL) &&
         (pSearchRegime != NULL) &&
         (pSearchRegime->regimeId < REGIME(MAX)) &&
         (tupleIdx >= pSearchRegime->tupleStartIdx) &&
         (tupleIdx <= pSearchRegime->tupleEndIdx)),
        FLCN_ERR_ILWALID_ARGUMENT,
        s_pwrPolicyWorkloadCombined1xSearchRegimeGetFreqTupleByTupleIdx_exit);

    // Initialize the required parameters.
    pRegimeData = &pCombined1x->regimeData[pSearchRegime->regimeId];
    PWR_POLICY_WORKLOAD_COMBINED_1X_GET_SINGLE_1X(
        pCombined1x, SINGLE1X(PRI), &pSingle1xPri, status,
        s_pwrPolicyWorkloadCombined1xSearchRegimeGetFreqTupleByTupleIdx_exit);

    if (HAS_SINGLE1X(pCombined1x, SEC))
    {
        PWR_POLICY_WORKLOAD_COMBINED_1X_GET_SINGLE_1X(
            pCombined1x, SINGLE1X(SEC), &pSingle1xSec, status,
            s_pwrPolicyWorkloadCombined1xSearchRegimeGetFreqTupleByTupleIdx_exit);
    }

    pPstates = PSTATES_GET();
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pPstates != NULL),
        FLCN_ERR_ILWALID_STATE,
        s_pwrPolicyWorkloadCombined1xSearchRegimeGetFreqTupleByTupleIdx_exit);
    PERF_LIMITS_RANGE_SET_VALUE(&pstateRange,
        boardObjGrpMaskBitIdxHighest(&(pPstates->super.objMask)));

    PMU_ASSERT_OK_OR_GOTO(status,
        s_pwrPolicyWorkloadCombined1xGetClkDomainIdx(
            pCombined1x,
            pCombined1x->regimeData[pSearchRegime->regimeId].regimeInitData.singleIdx,
            &clkDomainIdx),
        s_pwrPolicyWorkloadCombined1xSearchRegimeGetFreqTupleByTupleIdx_exit);

    pDomain = CLK_DOMAIN_GET(clkDomainIdx);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDomain != NULL),
        FLCN_ERR_ILWALID_STATE,
        s_pwrPolicyWorkloadCombined1xSearchRegimeGetFreqTupleByTupleIdx_exit);

    pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDomainProg != NULL),
        FLCN_ERR_ILWALID_STATE,
        s_pwrPolicyWorkloadCombined1xSearchRegimeGetFreqTupleByTupleIdx_exit);

    clkFreqIdx = tupleIdx - pSearchRegime->tupleStartIdx + pSearchRegime->clkFreqStartIdx;

    propFromFreqMHz = clkDomainProgGetFreqMHzByIndex(pDomainProg, clkFreqIdx);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (propFromFreqMHz != LW_U16_MAX),
        FLCN_ERR_ILWALID_STATE,
        s_pwrPolicyWorkloadCombined1xSearchRegimeGetFreqTupleByTupleIdx_exit);

    status = FLCN_ERR_NOT_SUPPORTED;
    switch (pSearchRegime->regimeId)
    {
        case REGIME(DEFAULT_RATIO):
        {
            pRegimeFreq->freqMHz[SINGLE1X(PRI)] = propFromFreqMHz;
            if (pSingle1xSec != NULL)
            {
                PMU_ASSERT_OK_OR_GOTO(status,
                    s_pwrPolicyWorkloadCombined1xClkPropagate(
                    pRegimeFreq->freqMHz[SINGLE1X(PRI)],
                    pSingle1xPri->clkDomainIdx, pSingle1xSec->clkDomainIdx,
                    pCombined1x->regimeData[pSearchRegime->regimeId].regimeInitData.clkPropTopIdx,
                    &pRegimeFreq->freqMHz[SINGLE1X(SEC)]),
                s_pwrPolicyWorkloadCombined1xSearchRegimeGetFreqTupleByTupleIdx_exit);
            }
            status = FLCN_OK;
            break;
        }
        case REGIME(SEC_SOFT_FLOOR):
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X_LWDCLK_CTRL))
            {
                PMU_ASSERT_TRUE_OR_GOTO(status,
                    (pSingle1xSec != NULL), FLCN_ERR_ILWALID_STATE,
                    s_pwrPolicyWorkloadCombined1xSearchRegimeGetFreqTupleByTupleIdx_exit);

                pRegimeFreq->freqMHz[SINGLE1X(PRI)] = propFromFreqMHz;
                pRegimeFreq->freqMHz[SINGLE1X(SEC)] =
                    pCombined1x->obsMetrics.pSingles[SINGLE1X(SEC)].freqSoftFloorMHz;
                status = FLCN_OK;
            }
            break;
        }
        case REGIME(MAX_RATIO):
        case REGIME(MIN_RATIO):
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X_LWDCLK_CTRL))
            {
                PMU_ASSERT_TRUE_OR_GOTO(status,
                    (pSingle1xSec != NULL), FLCN_ERR_ILWALID_STATE,
                    s_pwrPolicyWorkloadCombined1xSearchRegimeGetFreqTupleByTupleIdx_exit);

                pRegimeFreq->freqMHz[SINGLE1X(PRI)] = propFromFreqMHz;
                PMU_ASSERT_OK_OR_GOTO(status,
                    s_pwrPolicyWorkloadCombined1xClkPropagate(pRegimeFreq->freqMHz[SINGLE1X(PRI)],
                        pSingle1xPri->clkDomainIdx, pSingle1xSec->clkDomainIdx,
                        pCombined1x->regimeData[pSearchRegime->regimeId].regimeInitData.clkPropTopIdx,
                        &pRegimeFreq->freqMHz[SINGLE1X(SEC)]),
                    s_pwrPolicyWorkloadCombined1xSearchRegimeGetFreqTupleByTupleIdx_exit);
                status = FLCN_OK;
            }
            break;
        }
        case REGIME(PRI_VMIN_FLOOR):
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X_LWDCLK_CTRL))
            {
                PMU_ASSERT_TRUE_OR_GOTO(status,
                    (pSingle1xSec != NULL), FLCN_ERR_ILWALID_STATE,
                    s_pwrPolicyWorkloadCombined1xSearchRegimeGetFreqTupleByTupleIdx_exit);

                pRegimeFreq->freqMHz[SINGLE1X(SEC)] = propFromFreqMHz;
                pRegimeFreq->freqMHz[SINGLE1X(PRI)] =
                    pCombined1x->obsMetrics.pSingles[SINGLE1X(PRI)].freqFmaxVminMHz;
                status = FLCN_OK;
            }
            break;
        }
        case REGIME(SEC_VMIN_FLOOR):
        {
            PMU_ASSERT_TRUE_OR_GOTO(status,
                (pSingle1xSec != NULL), FLCN_ERR_ILWALID_STATE,
                s_pwrPolicyWorkloadCombined1xSearchRegimeGetFreqTupleByTupleIdx_exit);

            pRegimeFreq->freqMHz[SINGLE1X(PRI)] = propFromFreqMHz;
            pRegimeFreq->freqMHz[SINGLE1X(SEC)] =
                    pCombined1x->obsMetrics.pSingles[SINGLE1X(SEC)].freqFmaxVminMHz;
            status = FLCN_OK;
            break;
        }
    }
    // Catch unsupported REGIME errors above.
    PMU_ASSERT_OK_OR_GOTO(
        status, status, s_pwrPolicyWorkloadCombined1xSearchRegimeGetFreqTupleByTupleIdx_exit);

    // Sanity check generated FREQ_TUPLE value.
    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_INDEX_BEGIN(pCombined1x, singleIdx)
    {

        //
        // Apply ceiling to tupleEndIdx generated FREQ_TUPLE at the REGIME's endpoint
        // FREQ_TUPLE.  Generated FREQ_TUPLE may be above endpoint due to
        // quantization issues related to propagation ratios.  When
        // PRI:SEC ratio > 1, will have holes in PRI->SEC mappings such
        // that SEC->PRI->SEC' results in SEC' > SEC.  Colwersely, when
        // PRI:SEC ratio < 1, will have non 1-1 PRI->SEC mappings such
        // that PRI->SEC->PRI' results in PRI' > PRI.  These inequalities
        // should be of no greater magnituded than then propagation ratio,
        // and further we've confirmed that the endpoint FREQ_TUPLEs are
        // monotonic.  So, can safely bound the generated FREQ_TUPLEs here
        // and ensure that will have monotonic, non-overlapping results.
        //
        if (tupleIdx == pSearchRegime->tupleEndIdx)
        {
            pRegimeFreq->freqMHz[singleIdx] =
                LW_MIN(pRegimeFreq->freqMHz[singleIdx],
                        pRegimeData->regimeFreq.freqMHz[singleIdx]);
        }
        //
        // All other tupleIdxs should strictly satisfy monotonicity
        // checks against the REGIME's endpoint FREQ_TUPLE.
        //
        else
        {
            PMU_ASSERT_TRUE_OR_GOTO(status,
                (pRegimeFreq->freqMHz[singleIdx] <=
                    pRegimeData->regimeFreq.freqMHz[singleIdx]),
                FLCN_ERR_ILWALID_STATE,
                s_pwrPolicyWorkloadCombined1xSearchRegimeGetFreqTupleByTupleIdx_exit);
        }
    }
    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_INDEX_END;

 s_pwrPolicyWorkloadCombined1xSearchRegimeGetFreqTupleByTupleIdx_exit:
    return status;
}

/*!
 * Helper function to get a tuple index of a regime from the frequency
 *
 * @param[in]   pCombined1x   PWR_POLICY_WORKLOAD_COMBINED_1X pointer.
 * @param[in]   freqMHz       Input frequency
 * @param[in]   bMax          Bool to indicate if clocks are going up or down
 * @param[out]  pTupleIdx     tuple index of the input frequency
 *
 * @return FLCN_OK
 *     operation computed successfully.
 * @return Other errors
 *     Propagates return values from various calls.
 */
static FLCN_STATUS
s_pwrPolicyWorkloadCombined1xGetTupleIdxByPrimaryFreq
(
    PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x,
    LwU32                            freqMHz,
    LwBool                           bMax,
    LwU16                           *pTupleIdx
)
{
    LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_SEARCH_REGIME
               *pSearchRegime;
    LwS16       i;
    FLCN_STATUS status = FLCN_OK;

    // Sanity-check parameters.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pCombined1x != NULL) &&
         (pTupleIdx != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        s_pwrPolicyWorkloadCombined1xGetTupleIdxByPrimaryFreq_exit);

    //
    // Iterate over SEARCH_REGIMES from top-down, taking highest
    // REGIME with a max PRIMARY frequency >= the requested frequency.
    //
    for (i = (pCombined1x->numRegimes - 1); i >= 0; i--)
    {
        pSearchRegime = &pCombined1x->searchRegimes[i];
        if (pCombined1x->regimeData[pSearchRegime->regimeId].regimeFreq.freqMHz[SINGLE1X(PRI)] >= freqMHz)
        {
            break;
        }
    }

    // Confirm found matching SEARCH_REGIME above.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (i >= 0),
        FLCN_ERR_ILWALID_STATE,
        s_pwrPolicyWorkloadCombined1xGetTupleIdxByPrimaryFreq_exit);

    // Based on REGIME, colwert to a tupleIdx
    if (pSearchRegime->regimeId == REGIME(PRI_VMIN_FLOOR))
    {
        //
        // With PRI_VMIN_FLOOR REGIME, GPCCLK is constant.  Thus, the
        // GPCCLK value corresponds to a range of tupleIdxs.  Will
        // return either highest (tupleEndIdx) or lowest
        // (tupleStartIdx) tupleIdx based on requested behavior.
        //
        *pTupleIdx =
            (bMax) ? pSearchRegime->tupleEndIdx : pSearchRegime->tupleStartIdx;
    }
    else
    {
        LwBoardObjIdx clkDomainIdx;
        LwU16         clkFreqIdx;

        // Colwert freqMhz -> clkFreqIdx
        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrPolicyWorkloadCombined1xGetClkDomainIdx(
                pCombined1x, SINGLE1X(PRI),
                &clkDomainIdx),
            s_pwrPolicyWorkloadCombined1xGetTupleIdxByPrimaryFreq_exit);

        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrPolicyWorkloadCombined1xGetIndexByFreqMHz(
                clkDomainIdx,
                freqMHz,
                LW_FALSE, &clkFreqIdx),
            s_pwrPolicyWorkloadCombined1xGetTupleIdxByPrimaryFreq_exit);

        //
        // Sanity-check that clkFreqIdx is greater than
        // SEARCH_REGIME::clkFreqStartIdx.
        //
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (clkFreqIdx >= pSearchRegime->clkFreqStartIdx),
            FLCN_ERR_ILWALID_STATE,
            s_pwrPolicyWorkloadCombined1xGetTupleIdxByPrimaryFreq_exit);

        //
        // Callwlate pTupleIdx = SEARCH_REGIME::tupleStartIdx +
        // (clkFreqIdx - SEARCH_REGIME::clkFreqStartIdx)
        //
        *pTupleIdx = pSearchRegime->tupleStartIdx + (clkFreqIdx - pSearchRegime->clkFreqStartIdx);
    }

s_pwrPolicyWorkloadCombined1xGetTupleIdxByPrimaryFreq_exit:
    return status;
}

/*!
 * Helper function to get a regime index from a tuple index
 *
 * @param[in]   pCombined1x   PWR_POLICY_WORKLOAD_COMBINED_1X pointer.
 * @param[in]   tupleIdx      Input tuple index
 * @param[out]  pSearchRegimeIdx
 *                            Output regime index corresponding to the tupleIdx
 *
 * @return FLCN_OK
 *     operation computed successfully.
 * @return Other errors
 *     Propagates return values from various calls.
 */
static FLCN_STATUS
s_pwrPolicyWorkloadCombined1xGetSearchRegimeIdxByTupleIdx
(
    PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x,
    LwU16                            tupleIdx,
    LwU8                            *pSearchRegimeIdx
)
{
    LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_SEARCH_REGIME
               *pSearchRegime;
    FLCN_STATUS status = FLCN_OK;
    LwU8        i;

    // Sanity-check parameters.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pCombined1x != NULL) &&
         (pSearchRegimeIdx != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        s_pwrPolicyWorkloadCombined1xGetSearchRegimeIdxByTupleIdx_exit);

    //
    // Iterate through seachRegimes[] array to find searchRegime which
    // contains specified @ref tupleIdx.
    //
    for (i = 0; i < pCombined1x->numRegimes; i++)
    {
        pSearchRegime = &pCombined1x->searchRegimes[i];
        if ((tupleIdx >= pSearchRegime->tupleStartIdx) &&
            (tupleIdx <= pSearchRegime->tupleEndIdx))
        {
            *pSearchRegimeIdx = i;
            break;
        }
    }
    // Confirm match found above.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (i < pCombined1x->numRegimes), FLCN_ERR_ILWALID_INDEX,
        s_pwrPolicyWorkloadCombined1xGetSearchRegimeIdxByTupleIdx_exit);

s_pwrPolicyWorkloadCombined1xGetSearchRegimeIdxByTupleIdx_exit:
    return status;
}

/*!
 * Computes the output domain group limit values this power policy wishes to
 * apply.  This is determined as the maximum frequency and voltage values which
 * would yield power at the budget/limit value given the filtered workload
 * value, based on the power equation:
 *
 *      Solve for f using the equations:
 *          P_Limit_Combined >= P_Combined(f)
 *          P_Limit_Single   >= P_Single(f) for each SINGLE_1X
 *      where,
 *          P_Combined(f) is the sum of power of all rails at f and
 *          P_Single(f) is the power of each single rail at f
 *
 * @param[in]   pCombined1x   PWR_POLICY_WORKLOAD_COMBINED_1X pointer.
 * @param[out]  pDomGrpLimits
 *     RM_PMU_PERF_DOMAIN_GROUP_LIMITS structure to populate with the limits the
 *     PWR_POLICY object wants to apply.
 *
 * @return FLCN_OK
 *     Domain group limit values computed successfully.
 * @return FLCN_ERR_ILWALID_STATE
 *     Encountered an invalid state during computation.
 * @return Other errors
 *     Propagates return values from various calls.
 */
static FLCN_STATUS
s_pwrPolicyWorkloadCombined1xComputeRegimeClkMHz
(
    PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x,
    PERF_DOMAIN_GROUP_LIMITS        *pDomGrpLimits
)
{
    LwU16                          tupleStartIdx;
    LwU16                          tupleEndIdx;
    LwU16                          tupleMidIdx;
    FLCN_STATUS                    status;
    PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1x;
    LwBoardObjIdx                  i;
    LwBool                         bLimitSatisfied;
    CLK_DOMAIN                    *pDomain;
    LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_SINGLES_FREQ_TUPLE
                                   regimeFreq;
    LwU8                           searchRegimeIdx;

    // Get the start and end index for binary search
    tupleStartIdx = pCombined1x->searchRegimes[pCombined1x->numRegimes - 1].tupleStartIdx;
    tupleEndIdx   = pCombined1x->searchRegimes[0].tupleEndIdx;

    // Initialize @ref numStepsBack to zero
    pCombined1x->numStepsBack = 0;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((tupleStartIdx != LW_U16_MAX) && (tupleEndIdx != LW_U16_MAX)),
        FLCN_ERR_ILWALID_STATE,
        s_pwrPolicyWorkloadCombined1xComputeRegimeClkMHz_exit);

    // Binary search over set of tupleStartIdx and tupleEndIdx
    while (LW_TRUE)
    {
        //
        // We want to round up to the higher index if (tupleStartIdx + endIdx)
        // is not exactly divisible by 2. This is to ensure that we always
        // test the highest index.
        //
        tupleMidIdx = LW_UNSIGNED_ROUNDED_DIV((tupleStartIdx + tupleEndIdx), 2);

        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrPolicyWorkloadCombined1xGetFreqByTupleIdx(
                pCombined1x, tupleMidIdx, &regimeFreq),
            s_pwrPolicyWorkloadCombined1xComputeRegimeClkMHz_exit);

        PMU_ASSERT_OK_OR_GOTO(status,
            s_pwrPolicyWorkloadCombined1xIsLimitSatisfiedAtFreq(
                pCombined1x, &regimeFreq, &bLimitSatisfied),
            s_pwrPolicyWorkloadCombined1xComputeRegimeClkMHz_exit);

        if (tupleStartIdx == tupleEndIdx)
        {
            if (!bLimitSatisfied &&
                (tupleStartIdx > 0))
            {
                // Back up by one point and check if we satisfy
                tupleStartIdx = tupleEndIdx = tupleStartIdx - 1;
                pCombined1x->numStepsBack++;
                continue;
            }
            else
            {
                break;
            }
        }

        // Search up or down based on condition at freqMHz(midIdx)
        if (bLimitSatisfied)
        {
            tupleStartIdx = tupleMidIdx;
        }
        else
        {
            tupleEndIdx = tupleMidIdx - 1;
        }
    }

    // Save off the final TUPLE_STATUS the search
    LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_REGIMES_STATUS_TUPLE *pTupleStatus =
        &pCombined1x->regimesStatus.tuples[LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_REGIMES_STATUS_TUPLE_IDX_SEARCH];
    pTupleStatus->tupleIdx = tupleStartIdx;

    PMU_ASSERT_OK_OR_GOTO(status,
        s_pwrPolicyWorkloadCombined1xGetSearchRegimeIdxByTupleIdx(pCombined1x, pTupleStatus->tupleIdx, &searchRegimeIdx),
        s_pwrPolicyWorkloadCombined1xComputeRegimeClkMHz_exit);

    // Confirm match found above.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (searchRegimeIdx < pCombined1x->numRegimes), FLCN_ERR_ILWALID_INDEX,
        s_pwrPolicyWorkloadCombined1xComputeRegimeClkMHz_exit);
    pTupleStatus->searchRegimeIdx =
        pCombined1x->searchRegimes[searchRegimeIdx].regimeId;

    // Set the new frequency values
    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_BEGIN(
        pCombined1x, pSingle1x, i, status,
        s_pwrPolicyWorkloadCombined1xComputeRegimeClkMHz_exit)
    {
        RM_PMU_PMGR_PWR_POLICY_DOMGRP_MAP_ENTRY domGrpData;

        pDomain = CLK_DOMAIN_GET(pSingle1x->clkDomainIdx);
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pDomain != NULL),
            FLCN_ERR_ILWALID_STATE,
            s_pwrPolicyWorkloadCombined1xComputeRegimeClkMHz_exit);

        // Get the domain group data from clkDomainIdx
        PMU_ASSERT_OK_OR_GOTO(status,
            pwrPolicies35DomGrpDataGet(
                PWR_POLICIES_35_GET(),
                &domGrpData,
                pSingle1x->clkDomainIdx),
            s_pwrPolicyWorkloadCombined1xComputeRegimeClkMHz_exit);

        pDomGrpLimits->values[domGrpData.domGrpLimitIdx] =
            pCombined1x->estMetrics.pSingles[i].freqMHz *
            clkDomainFreqkHzScaleGet(pDomain);
    }
    PWR_POLICY_WORKLOAD_COMBINED_1X_FOR_EACH_SINGLE_1X_END;

    // As PSTATE 0 contains the entire freq range, we will always be in P0.
    pDomGrpLimits->values[PERF_DOMAIN_GROUP_PSTATE] =
        (perfGetPstateCount() - 1);

s_pwrPolicyWorkloadCombined1xComputeRegimeClkMHz_exit:
    return status;
}

/*!
 * Helper function to get Clock Domain Index based on singleIdx
 *
 * @param[in]   pCombined1x     PWR_POLICY_WORKLOAD_COMBINED_1X pointer.
 * @param[in]   singleIdx       Index to the single instance
 * @param[out]  pClockDomainIdx clock domain index
 *
 * @return FLCN_OK
 *     Domain group limit values computed successfully.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Invalid singleIdx.
 * @return Other errors
 *     Propagates return values from various calls.
 */
static FLCN_STATUS
s_pwrPolicyWorkloadCombined1xGetClkDomainIdx
(
    PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x,
    LwU8                             singleIdx,
    LwBoardObjIdx                   *pClkDomainIdx
)
{
    FLCN_STATUS                    status;
    PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1x;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (singleIdx < SINGLE1X(MAX)),
        FLCN_ERR_ILWALID_ARGUMENT,
        s_pwrPolicyWorkloadCombined1xGetClkDomainIdx_exit);

    PWR_POLICY_WORKLOAD_COMBINED_1X_GET_SINGLE_1X(
        pCombined1x, singleIdx, &pSingle1x, status,
        s_pwrPolicyWorkloadCombined1xGetClkDomainIdx_exit);

    *pClkDomainIdx = pSingle1x->clkDomainIdx;

s_pwrPolicyWorkloadCombined1xGetClkDomainIdx_exit:
    return status;
}

/*!
 * Helper function to initialise 
 * @ref PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_INIT_DATA::childRegimeMask
 *
 * @param[in]   pCombined1x      PWR_POLICY_WORKLOAD_COMBINED_1X pointer.
 * @param[in]   childRegimeId    Child Regime Index
 * @param[out]  pChildRegimeMask Pointer to the child Regime Mas
 *
 * @return FLCN_OK
 *     success
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Invalid child regime mask.
 * @return Other errors
 *     Propagates return values from various calls.
 */
static FLCN_STATUS
s_pwrPolicyWorkloadCombined1xInitChildRegimeMask
(
    PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x,
    LwBoardObjIdx                    childRegimeId,
    BOARDOBJGRPMASK_E32             *pChildRegimeMask
)
{
    FLCN_STATUS status = FLCN_OK;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pChildRegimeMask != NULL), FLCN_ERR_ILWALID_ARGUMENT,
        s_pwrPolicyWorkloadCombined1xInitChildRegimeMask_exit);

    if (boardObjGrpMaskBitGet(&pCombined1x->supportedRegimeMask, childRegimeId))
    {
        boardObjGrpMaskBitSet(pChildRegimeMask, childRegimeId);
    }

s_pwrPolicyWorkloadCombined1xInitChildRegimeMask_exit:
    return status;
}

/*!
 * @brief Helper function to ilwalidate search regimes
 *
 * @param[in]  pCombined1x   PWR_POLICY_WORKLOAD_COMBINED_1X pointer.
 *
 * @return FLCN_OK
 *     Function exelwted successfully.
 * @return Other errors
 *     Propagates return values from various calls.
 */
static FLCN_STATUS
s_pwrPolicyWorkloadCombined1xRegimeSpaceIlwalidate
(
    PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x
)
{
    LwU8 i;

    memset(pCombined1x->searchRegimes, 0xFF,
        sizeof(LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_SEARCH_REGIME) *
        LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_SEARCH_REGIME_ID_MAX);

    memset(&pCombined1x->regimesStatus, 0xFF,
        sizeof(LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_REGIMES_STATUS));

    for (i = 0; i < REGIME(MAX); i++)
    {
        memset(&pCombined1x->regimeData[i].regimeFreq,
            LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_SEARCH_REGIME_FREQ_ILWALID,
            sizeof(LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_COMBINED_1X_REGIME_SINGLES_FREQ_TUPLE));
    }
    return FLCN_OK;
}

/*!
 * @brief Helper function to check if regime behavior is enabled
 *
 * @param[in]  pCombined1x   PWR_POLICY_WORKLOAD_COMBINED_1X pointer.
 * @param[out] pbRegimesEnabled bool to indicate if search regimes are enabled
 *
 * @return FLCN_OK
 *     Function exelwted successfully.
 * @return Other errors
 *     Propagates return values from various calls.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
s_pwrPolicyWorkloadCombined1xRegimesEnabled
(
    PWR_POLICY_WORKLOAD_COMBINED_1X *pCombined1x,
    LwBool                          *pbRegimesEnabled
)
{
    PWR_POLICY_WORKLOAD_SINGLE_1X *pSingle1x = NULL;
    FLCN_STATUS                    status = FLCN_OK;

    // Sanity-check parameters
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pCombined1x != NULL) &&
         (pbRegimesEnabled != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        s_pwrPolicyWorkloadCombined1xRegimesEnabled_exit);

    // Default to FALSE
    *pbRegimesEnabled = LW_FALSE;

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X_LWDCLK_CTRL) &&
        HAS_SINGLE1X(pCombined1x, SEC))
    {
        PWR_POLICY_WORKLOAD_COMBINED_1X_GET_SINGLE_1X(
            pCombined1x, SINGLE1X(SEC), &pSingle1x, status,
            s_pwrPolicyWorkloadCombined1xRegimesEnabled_exit);
    }

    //
    // For now, regimes are enabled if SOFT_FLOOR is enabled on SEC
    // SINGLE.
    //
    *pbRegimesEnabled = ((pSingle1x != NULL) &&
                         pwrPolicySingle1xIsSoftFloorEnabled(pSingle1x));

s_pwrPolicyWorkloadCombined1xRegimesEnabled_exit:
    return status;
}
