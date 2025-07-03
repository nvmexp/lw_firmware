/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file clk_vf_point_40_volt_pri.c
 *
 * @brief Module managing all state related to the CLK_VF_POINT_VOLT_PRI structure.
 * This structure defines a point on the VF lwrve for which voltage is the
 * independent variable (i.e. fixed for this VF point) and frequency is the
 * dependent variable (i.e. varies with process, temperature, etc.).
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "perf/3x/vfe.h"
#include "volt/objvolt.h"
#if (PMU_PROFILE_GA10X_RISCV)
#include <lwriscv/print.h>
#endif 

ct_assert(CLK_CLK_VF_POINT_40_VOLT_PRI_FREQ_TUPLE_MAX_SIZE <=
    LW2080_CTRL_CLK_CLK_VF_POINT_FREQ_TUPLE_MAX_SIZE);

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjVirtualTableDynamicCast (s_vfeVarDynamicCast_40_VOLT_PRI)
    GCC_ATTRIB_SECTION("imem_perf", "s_vfeVarDynamicCast_40_VOLT_PRI")
    GCC_ATTRIB_USED();

/* ------------------------ Global Variables ------------------------------- */
/*!
 * Virtual table for CLK_VF_POINT_40_VOLT_PRI object interfaces.
 */
BOARDOBJ_VIRTUAL_TABLE ClkVfPoint40VoltPriVirtualTable =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_DYNAMIC_CAST(s_vfeVarDynamicCast_40_VOLT_PRI)
};

/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
clkVfPointIfaceModel10GetStatus_40_VOLT_PRI
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_CLK_CLK_VF_POINT_40_VOLT_PRI_BOARDOBJ_GET_STATUS *pVfPoint40VoltPriStatus =
        (RM_PMU_CLK_CLK_VF_POINT_40_VOLT_PRI_BOARDOBJ_GET_STATUS *)pPayload;
    CLK_VF_POINT_40_VOLT_PRI   *pVfPoint40VoltPri;
    LwU8                        idx;
    FLCN_STATUS                 status             = FLCN_OK;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVfPoint40VoltPri, pBoardObj, CLK, CLK_VF_POINT, 40_VOLT_PRI,
                                  &status, clkVfPointIfaceModel10GetStatus_40_VOLT_PRI_exit);

    status = clkVfPointIfaceModel10GetStatus_40(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto clkVfPointIfaceModel10GetStatus_40_VOLT_PRI_exit;
    }

    // Copy-out the _40 specific params.
    pVfPoint40VoltPriStatus->baseVFTuple = pVfPoint40VoltPri->baseVFTuple;

    // Init the output struct.
    memset(pVfPoint40VoltPriStatus->offsetedVFTuple, 0U,
        (sizeof(LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE) *
            LW2080_CTRL_CLK_CLK_VF_POINT_FREQ_TUPLE_MAX_SIZE));

    memset(pVfPoint40VoltPriStatus->offsetVFTuple, 0U,
        (sizeof(LW2080_CTRL_CLK_OFFSET_VF_TUPLE) *
            LW2080_CTRL_CLK_CLK_VF_POINT_FREQ_TUPLE_MAX_SIZE));

    for (idx = 0U; idx < CLK_CLK_VF_POINT_40_VOLT_PRI_FREQ_TUPLE_MAX_SIZE; idx++)
    {
        pVfPoint40VoltPriStatus->offsetedVFTuple[idx] =
            pVfPoint40VoltPri->offsetedVFTuple[idx];

        pVfPoint40VoltPriStatus->offsetVFTuple[idx] =
            pVfPoint40VoltPri->offsetVFTuple[idx];
    }

clkVfPointIfaceModel10GetStatus_40_VOLT_PRI_exit:
    return status;
}

/*!
 * @copydoc ClkVfPoint40Load
 */
FLCN_STATUS
clkVfPoint40Load_VOLT_PRI
(
    CLK_VF_POINT_40        *pVfPoint40,
    CLK_DOMAIN_40_PROG     *pDomain40Prog,
    CLK_VF_REL             *pVfRel,
    LwU8                    lwrveIdx
)
{
    CLK_DOMAIN   *pDomain;
    LwBoardObjIdx clkIdx;
    FLCN_STATUS   status;

    status = clkVfPoint40Load_VOLT(pVfPoint40,
                pDomain40Prog, pVfRel, lwrveIdx);
    if (status != FLCN_OK)
    {
        goto clkVfPoint40Load_VOLT_PRI_exit;
    }

    // Sanity check all position mapping.
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, clkIdx,
        clkDomain40ProgGetPrimarySecondaryDomainsMask(pDomain40Prog, pVfRel->railIdx))
    {
        CLK_DOMAIN_40_PROG *pDomain40ProgLocal = (CLK_DOMAIN_40_PROG *)pDomain;
        LwU8                clkPos             =
            clkDomain40ProgGetClkDomainPosByIdx(pDomain40ProgLocal, pVfRel->railIdx);

        // Validate the position.
        if ((clkPos == LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID) ||
            (clkPos >= CLK_CLK_VF_POINT_40_VOLT_PRI_FREQ_TUPLE_MAX_SIZE))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_STATE;
            goto clkVfPoint40Load_VOLT_PRI_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

clkVfPoint40Load_VOLT_PRI_exit:
    return status;
}

/*!
 * @copydoc ClkVfPoint40Cache
 */
FLCN_STATUS
clkVfPoint40Cache_VOLT_PRI
(
    CLK_VF_POINT_40            *pVfPoint40,
    CLK_VF_POINT_40            *pVfPoint40Last,
    CLK_DOMAIN_40_PROG         *pDomain40Prog,
    CLK_VF_REL                 *pVfRel,
    LwU8                        lwrveIdx,
    LwBool                      bVFEEvalRequired
)
{
    FLCN_STATUS status  = FLCN_OK;

    status = clkVfPoint40Cache_VOLT(pVfPoint40,
                                    pVfPoint40Last,
                                    pDomain40Prog,
                                    pVfRel,
                                    lwrveIdx,
                                    bVFEEvalRequired);
    if (status != FLCN_OK)
    {
        goto clkVfPointCache_40_VOLT_PRI_exit;
    }

    // Evaluate the CPM Max allowed frequency offset.
    if ((PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_CPM)) &&
        (bVFEEvalRequired) &&
        (clkVfRelCpmMaxFreqOffsetVfeIdxGet(pVfRel) !=
            PMU_PERF_VFE_EQU_INDEX_ILWALID))
    {
        LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTuple     =
            clkVfPoint40BaseVFTupleGet(pVfPoint40);
        CLK_VF_POINT_40_VOLT_PRI   *pVfPoint40Volt  =
            (CLK_VF_POINT_40_VOLT_PRI *)pVfPoint40;
        RM_PMU_PERF_VFE_VAR_VALUE   vfeVarVal;
        RM_PMU_PERF_VFE_EQU_RESULT  result;
        LW2080_CTRL_CLK_VF_INPUT    inputFreq;
        LW2080_CTRL_CLK_VF_OUTPUT   outputFreq;

        // Sanity check.
        if (pBaseVFTuple == NULL)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto clkVfPointCache_40_VOLT_PRI_exit;
        }

        // Compute the CPM Max frequency offset for the voltage.
        vfeVarVal.voltage.varType  = LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_VOLTAGE;
        vfeVarVal.voltage.varValue = pVfPoint40Volt->super.sourceVoltageuV;
        status = vfeEquEvaluate(
                    clkVfRelCpmMaxFreqOffsetVfeIdxGet(pVfRel),      // vfeEquIdx
                    &vfeVarVal,                                     // pValues
                    1,                                              // valCount
                    LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_FREQ_MHZ,  // outputType
                    &result);                                       // pResult
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfPointCache_40_VOLT_PRI_exit;
        }
        pBaseVFTuple->cpmMaxFreqOffsetMHz = (LwU16)result.freqMHz;

        // Quantize down if non zero
        if (pBaseVFTuple->cpmMaxFreqOffsetMHz != 0U)
        {
            // Init
            LW2080_CTRL_CLK_VF_INPUT_INIT(&inputFreq);
            LW2080_CTRL_CLK_VF_OUTPUT_INIT(&outputFreq);

            inputFreq.value = pBaseVFTuple->cpmMaxFreqOffsetMHz;
            status = clkDomainProgFreqQuantize_40(
                        CLK_DOMAIN_PROG_GET_FROM_40_PROG(pDomain40Prog),    // pDomainProg
                        &inputFreq,                                         // pInputFreq
                        &outputFreq,                                        // pOutputFreq
                        LW_TRUE,                                            // bFloor
                        LW_FALSE);                                          // bBound
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkVfPointCache_40_VOLT_PRI_exit;
            }
            pBaseVFTuple->cpmMaxFreqOffsetMHz = (LwU16) outputFreq.value;
        }
    }

clkVfPointCache_40_VOLT_PRI_exit:
    return status;
}


/*!
 * @copydoc ClkVfPointOffsetVF40Cache
 */
FLCN_STATUS
clkVfPoint40OffsetVFCache_VOLT_PRI
(
    CLK_VF_POINT_40            *pVfPoint40,
    CLK_VF_POINT_40            *pVfPoint40Last,
    CLK_DOMAIN_40_PROG         *pDomain40Prog,
    CLK_VF_REL                 *pVfRel
)
{
    FLCN_STATUS status  = FLCN_OK;

    status = clkVfPoint40OffsetVFCache_VOLT(pVfPoint40,
                                    pVfPoint40Last,
                                    pDomain40Prog,
                                    pVfRel);

    return status;
}

/*!
 * @copydoc ClkVfPoint40BaseVFCache
 */
FLCN_STATUS
clkVfPoint40BaseVFCache_VOLT_PRI
(
    CLK_VF_POINT_40            *pVfPoint40,
    CLK_VF_POINT_40            *pVfPoint40Last,
    CLK_DOMAIN_40_PROG         *pDomain40Prog,
    CLK_VF_REL                 *pVfRel,
    LwU8                        lwrveIdx,
    LwU8                        cacheIdx
)
{
    FLCN_STATUS status  = FLCN_OK;

    status = clkVfPoint40BaseVFCache_VOLT(pVfPoint40,
                                    pVfPoint40Last,
                                    pDomain40Prog,
                                    pVfRel,
                                    lwrveIdx,
                                    cacheIdx);
    if (status != FLCN_OK)
    {
        goto clkVfPointBaseVFCache_40_VOLT_PRI_exit;
    }

    // Evaluate the CPM Max allowed frequency offset.
    if ((PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_CPM)) &&
        (clkVfRelCpmMaxFreqOffsetVfeIdxGet(pVfRel) !=
            PMU_PERF_VFE_EQU_INDEX_ILWALID))
    {
        LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTuple     =
            clkVfPointsBaseVfCacheBaseVFTupleGet(lwrveIdx, cacheIdx,
                BOARDOBJ_GET_GRP_IDX(&pVfPoint40->super.super));
        CLK_VF_POINT_40_VOLT_PRI   *pVfPoint40Volt  =
            (CLK_VF_POINT_40_VOLT_PRI *)pVfPoint40;
        RM_PMU_PERF_VFE_VAR_VALUE   vfeVarVal[2];
        RM_PMU_PERF_VFE_EQU_RESULT  result;
        LW2080_CTRL_CLK_VF_INPUT    inputFreq;
        LW2080_CTRL_CLK_VF_OUTPUT   outputFreq;

        // Sanity check.
        if (pBaseVFTuple == NULL)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto clkVfPointBaseVFCache_40_VOLT_PRI_exit;
        }

        // Compute the CPM Max frequency offset for the voltage at specified temperature.
        vfeVarVal[0].voltage.varType        = LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_VOLTAGE;
        vfeVarVal[0].voltage.varValue       = pVfPoint40Volt->super.sourceVoltageuV;

        vfeVarVal[1].temperature.varType    = LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_SENSED_TEMP;
        vfeVarVal[1].temperature.varValue   = RM_PMU_CELSIUS_TO_LW_TEMP(cacheIdx * 5);

        status = vfeEquEvaluate(
                    clkVfRelCpmMaxFreqOffsetVfeIdxGet(pVfRel),      // vfeEquIdx
                    vfeVarVal,                                      // pValues
                    2U,                                             // valCount
                    LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_FREQ_MHZ,  // outputType
                    &result);                                       // pResult
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfPointBaseVFCache_40_VOLT_PRI_exit;
        }
        pBaseVFTuple->cpmMaxFreqOffsetMHz = (LwU16)result.freqMHz;

        // Quantize down if non zero
        if (pBaseVFTuple->cpmMaxFreqOffsetMHz != 0U)
        {
            // Init
            LW2080_CTRL_CLK_VF_INPUT_INIT(&inputFreq);
            LW2080_CTRL_CLK_VF_OUTPUT_INIT(&outputFreq);

            inputFreq.value = pBaseVFTuple->cpmMaxFreqOffsetMHz;
            status = clkDomainProgFreqQuantize_40(
                        CLK_DOMAIN_PROG_GET_FROM_40_PROG(pDomain40Prog),    // pDomainProg
                        &inputFreq,                                         // pInputFreq
                        &outputFreq,                                        // pOutputFreq
                        LW_TRUE,                                            // bFloor
                        LW_FALSE);                                          // bBound
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkVfPointBaseVFCache_40_VOLT_PRI_exit;
            }
            pBaseVFTuple->cpmMaxFreqOffsetMHz = (LwU16) outputFreq.value;
        }
    }

clkVfPointBaseVFCache_40_VOLT_PRI_exit:
    return status;
}

/*!
 * @Copydoc ClkVfPoint40FreqTupleAccessor
 */
FLCN_STATUS
clkVfPoint40FreqTupleAccessor_VOLT_PRI
(
    CLK_VF_POINT_40                                *pVfPoint40,
    CLK_DOMAIN_40_PROG                             *pDomain40Prog,
    CLK_VF_REL                                     *pVfRel,
    LwU8                                            lwrveIdx,
    LwBool                                          bOffseted,
    LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE    *pOutput
)
{
    LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE      *pOffsetedVFTuple =
        clkVfPoint40OffsetedVFTupleGet(pVfPoint40);
    LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTuple =
        clkVfPoint40BaseVFTupleGet(pVfPoint40);
    LwU8        clkPos      =
        clkDomain40ProgGetClkDomainPosByIdx(pDomain40Prog, pVfRel->railIdx);
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    if (lwrveIdx != LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI)
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        PMU_BREAKPOINT();
        goto clkVfPoint40FreqTupleAccessor_VOLT_PRI_exit;
    }

    if ((pBaseVFTuple     == NULL) ||
        (pOffsetedVFTuple == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkVfPoint40FreqTupleAccessor_VOLT_PRI_exit;
    }

    pOutput->version              =
        LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE_VERSION_10;
    pOutput->inputBestMatch       = pOffsetedVFTuple[clkPos].voltageuV;
    pOutput->data.v10.pri.freqMHz = pOffsetedVFTuple[clkPos].freqMHz;

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_CPM))
    {
        pOutput->version =
            LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE_VERSION_30;
        pOutput->data.v30.pri.cpmMaxFreqOffsetMHz = pBaseVFTuple->cpmMaxFreqOffsetMHz;
    }

clkVfPoint40FreqTupleAccessor_VOLT_PRI_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief   CLK_VF_POINT_40_VOLT_PRI implementation of
 *          @ref BoardObjVirtualTableDynamicCast()
 *
 * @copydoc BoardObjVirtualTableDynamicCast()
 */
static void *
s_vfeVarDynamicCast_40_VOLT_PRI
(
    BOARDOBJ   *pBoardObj,
    LwU8        requestedType
)
{
    void                     *pObject        = NULL;
    CLK_VF_POINT_40_VOLT_PRI *pVfPt40VoltPri = (CLK_VF_POINT_40_VOLT_PRI *)pBoardObj;

    if (BOARDOBJ_GET_TYPE(pBoardObj) !=
        LW2080_CTRL_BOARDOBJ_TYPE(CLK, CLK_VF_POINT, 40_VOLT_PRI))
    {
        PMU_BREAKPOINT();
        goto s_vfeVarDynamicCast_40_VOLT_PRI_exit;
    }

    switch (requestedType)
    {
        case LW2080_CTRL_BOARDOBJ_TYPE(CLK, CLK_VF_POINT, BASE):
        {
            CLK_VF_POINT *pVfPt = &pVfPt40VoltPri->super.super.super;
            pObject = (void *)pVfPt;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(CLK, CLK_VF_POINT, 40):
        {
            CLK_VF_POINT_40 *pVfPt40 = &pVfPt40VoltPri->super.super;
            pObject = (void *)pVfPt40;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(CLK, CLK_VF_POINT, 40_VOLT):
        {
            CLK_VF_POINT_40_VOLT *pVfPt40Volt = &pVfPt40VoltPri->super;
            pObject = (void *)pVfPt40Volt;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(CLK, CLK_VF_POINT, 40_VOLT_PRI):
        {
            pObject = (void *)pVfPt40VoltPri;
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            break;
        }
    }

s_vfeVarDynamicCast_40_VOLT_PRI_exit:
    return pObject;
}
