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
 * @file clk_vf_point_40_volt_sec.c
 *
 * @brief Module managing all state related to the CLK_VF_POINT_40_VOLT_SEC structure.
 * This structure defines a point on the VF lwrve for which voltage is the
 * independent variable (i.e. fixed for this VF point) and frequency is the
 * dependent variable (i.e. varies with process, temperature, etc.).
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "perf/3x/vfe.h"
#include "volt/objvolt.h"

ct_assert(CLK_CLK_VF_POINT_40_VOLT_SEC_FREQ_TUPLE_MAX_SIZE <=
    LW2080_CTRL_CLK_CLK_VF_POINT_FREQ_TUPLE_MAX_SIZE);

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjVirtualTableDynamicCast (s_vfeVarDynamicCast_40_VOLT_SEC)
    GCC_ATTRIB_SECTION("imem_perf", "s_vfeVarDynamicCast_40_VOLT_SEC")
    GCC_ATTRIB_USED();

/* ------------------------ Global Variables ------------------------------- */
/*!
 * Virtual table for CLK_VF_POINT_40_VOLT_SEC object interfaces.
 */
BOARDOBJ_VIRTUAL_TABLE ClkVfPoint40VoltSecVirtualTable =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_DYNAMIC_CAST(s_vfeVarDynamicCast_40_VOLT_SEC)
};

/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_VF_POINT_40_VOLT_SEC class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkVfPointGrpIfaceModel10ObjSet_40_VOLT_SEC
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_CLK_CLK_VF_POINT_40_VOLT_SEC_BOARDOBJ_SET *pSet =
        (RM_PMU_CLK_CLK_VF_POINT_40_VOLT_SEC_BOARDOBJ_SET *)pBoardObjDesc;
    CLK_VF_POINT_40_VOLT_SEC   *pVfPoint40VoltSec;
    FLCN_STATUS                 status;

    // Call into CLK_VF_POINT_40_VOLT super-constructor
    status = clkVfPointGrpIfaceModel10ObjSet_40_VOLT(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPointGrpIfaceModel10ObjSet_40_VOLT_SEC_exit;
    }

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVfPoint40VoltSec, *ppBoardObj,
                                  CLK, CLK_VF_POINT, 40_VOLT_SEC, &status,
                                  clkVfPointGrpIfaceModel10ObjSet_40_VOLT_SEC_exit);

    // Copy the CLK_VF_POINT_40_VOLT_SEC-specific data.
    pVfPoint40VoltSec->dvcoOffsetCodeOverride = pSet->dvcoOffsetCodeOverride;

clkVfPointGrpIfaceModel10ObjSet_40_VOLT_SEC_exit:
    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
clkVfPointIfaceModel10GetStatus_40_VOLT_SEC
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_CLK_CLK_VF_POINT_40_VOLT_SEC_BOARDOBJ_GET_STATUS *pVfPoint40VoltSecStatus =
        (RM_PMU_CLK_CLK_VF_POINT_40_VOLT_SEC_BOARDOBJ_GET_STATUS *)pPayload;
    CLK_VF_POINT_40_VOLT_SEC   *pVfPoint40VoltSec;
    LwU8                        idx;
    FLCN_STATUS                 status             = FLCN_OK;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVfPoint40VoltSec, pBoardObj,
                                  CLK, CLK_VF_POINT, 40_VOLT_SEC, &status,
                                  clkVfPointIfaceModel10GetStatus_40_VOLT_SEC_exit);

    status = clkVfPointIfaceModel10GetStatus_40(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto clkVfPointIfaceModel10GetStatus_40_VOLT_SEC_exit;
    }

    // Copy-out the _40 specific params.
    pVfPoint40VoltSecStatus->baseVFTuple = pVfPoint40VoltSec->baseVFTuple;

    // Copy-out the active DVCO offset code.
    pVfPoint40VoltSecStatus->baseVFTuple.dvcoOffsetCode =
        clkVfPoint40VoltSecActiveDvcoOffsetCodeGet(pVfPoint40VoltSec);

    // Init the output struct.
    memset(pVfPoint40VoltSecStatus->offsetedVFTuple, 0U,
        (sizeof(LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE) *
            LW2080_CTRL_CLK_CLK_VF_POINT_FREQ_TUPLE_MAX_SIZE));

    for (idx = 0U; idx < CLK_CLK_VF_POINT_40_VOLT_SEC_FREQ_TUPLE_MAX_SIZE; idx++)
    {
        pVfPoint40VoltSecStatus->offsetedVFTuple[idx] =
            pVfPoint40VoltSec->offsetedVFTuple[idx];
    }

clkVfPointIfaceModel10GetStatus_40_VOLT_SEC_exit:
    return status;
}


/*!
 * @copydoc ClkVfPoint40Load
 */
FLCN_STATUS
clkVfPoint40Load_VOLT_SEC
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
        goto clkVfPoint40Load_VOLT_SEC_exit;
    }

    // Sanity check all position mapping.
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, clkIdx,
        clkDomain40ProgGetPrimarySecondaryDomainsMask(pDomain40Prog, pVfRel->railIdx))
    {
        CLK_DOMAIN_40_PROG *pDomain40ProgLocal = (CLK_DOMAIN_40_PROG *)pDomain;
        LwU8                clkPos             =
            clkDomain40ProgGetClkDomainPosByIdx(pDomain40ProgLocal, pVfRel->railIdx);

        //
        // Validate the position.
        // (clkPos == LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID) is NOT an error as NOT
        // all secondaries support a secondary VF lwrve.
        //
        if ((clkPos >= CLK_CLK_VF_POINT_40_VOLT_SEC_FREQ_TUPLE_MAX_SIZE) &&
            (clkPos != LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_STATE;
            goto clkVfPoint40Load_VOLT_SEC_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

clkVfPoint40Load_VOLT_SEC_exit:
    return status;
}

/*!
 * @copydoc ClkVfPoint40Cache
 */
FLCN_STATUS
clkVfPoint40Cache_VOLT_SEC
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

    // Sanity check.
    if (!clkVfPoint40VoltSecSanityCheck(pVfPoint40, lwrveIdx))
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto clkVfPointCache_40_VOLT_SEC_exit;
    }

    status = clkVfPoint40Cache_VOLT(pVfPoint40,
                                    pVfPoint40Last,
                                    pDomain40Prog,
                                    pVfRel,
                                    lwrveIdx,
                                    bVFEEvalRequired);
    if (status != FLCN_OK)
    {
        goto clkVfPointCache_40_VOLT_SEC_exit;
    }

    // Evaluate the DVCO Offset Code.
    if ((bVFEEvalRequired)  &&
        (clkVfRelDvcoOffsetVfeIdxGet(pVfRel, lwrveIdx) !=
            PMU_PERF_VFE_EQU_INDEX_ILWALID))
    {
        CLK_VF_POINT_40_VOLT_SEC   *pVfPoint40VoltSec  =
            (CLK_VF_POINT_40_VOLT_SEC *)pVfPoint40;
        RM_PMU_PERF_VFE_VAR_VALUE   vfeVarVal;
        RM_PMU_PERF_VFE_EQU_RESULT  result;

        // Compute the CPM Max frequency offset for the voltage.
        vfeVarVal.voltage.varType  = LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_VOLTAGE;
        vfeVarVal.voltage.varValue = pVfPoint40VoltSec->super.sourceVoltageuV;
        status = vfeEquEvaluate(
                    clkVfRelDvcoOffsetVfeIdxGet(pVfRel, lwrveIdx),  // vfeEquIdx
                    &vfeVarVal,                                     // pValues
                    1,                                              // valCount
                    LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_FREQ_MHZ,  // outputType
                    &result);                                       // pResult
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfPointCache_40_VOLT_SEC_exit;
        }
        pVfPoint40VoltSec->baseVFTuple.dvcoOffsetCode = (LwU16)result.freqMHz;
    }

clkVfPointCache_40_VOLT_SEC_exit:
    return status;
}

/*!
 * @copydoc ClkVfPoint40OffsetVFCache
 */
FLCN_STATUS
clkVfPoint40OffsetVFCache_VOLT_SEC
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
    if (status != FLCN_OK)
    {
        goto clkVfPointOffsetVFCache_40_VOLT_SEC_exit;
    }

clkVfPointOffsetVFCache_40_VOLT_SEC_exit:
    return status;
}

/*!
 * @copydoc ClkVfPoint40BaseVFCache
 */
FLCN_STATUS
clkVfPoint40BaseVFCache_VOLT_SEC
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

    // Sanity check.
    if (!clkVfPoint40VoltSecSanityCheck(pVfPoint40, lwrveIdx))
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto clkVfPointBaseVFCache_40_VOLT_SEC_exit;
    }

    status = clkVfPoint40BaseVFCache_VOLT(pVfPoint40,
                                    pVfPoint40Last,
                                    pDomain40Prog,
                                    pVfRel,
                                    lwrveIdx,
                                    cacheIdx);
    if (status != FLCN_OK)
    {
        goto clkVfPointBaseVFCache_40_VOLT_SEC_exit;
    }

    // Evaluate the DVCO Offset Code.
    if ((clkVfRelDvcoOffsetVfeIdxGet(pVfRel, lwrveIdx) !=
            PMU_PERF_VFE_EQU_INDEX_ILWALID))
    {
        LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE_SEC *pBaseVFTuple     =
            clkVfPointsSecBaseVfCacheBaseVFTupleGet(lwrveIdx, cacheIdx,
                BOARDOBJ_GET_GRP_IDX(&pVfPoint40->super.super));

        CLK_VF_POINT_40_VOLT_SEC   *pVfPoint40VoltSec  =
            (CLK_VF_POINT_40_VOLT_SEC *)pVfPoint40;
        RM_PMU_PERF_VFE_VAR_VALUE   vfeVarVal[2];
        RM_PMU_PERF_VFE_EQU_RESULT  result;

        // Compute the CPM Max frequency offset for the voltage at specified temperature.
        vfeVarVal[0].voltage.varType        = LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_VOLTAGE;
        vfeVarVal[0].voltage.varValue       = pVfPoint40VoltSec->super.sourceVoltageuV;

        vfeVarVal[1].temperature.varType    = LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_SENSED_TEMP;
        vfeVarVal[1].temperature.varValue   = RM_PMU_CELSIUS_TO_LW_TEMP(cacheIdx * 5);

        status = vfeEquEvaluate(
                    clkVfRelDvcoOffsetVfeIdxGet(pVfRel, lwrveIdx),  // vfeEquIdx
                    vfeVarVal,                                      // pValues
                    2U,                                             // valCount
                    LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_FREQ_MHZ,  // outputType
                    &result);                                       // pResult
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfPointBaseVFCache_40_VOLT_SEC_exit;
        }
        pBaseVFTuple->dvcoOffsetCode = (LwU16)result.freqMHz;
    }

clkVfPointBaseVFCache_40_VOLT_SEC_exit:
    return status;
}

/*!
 * @Copydoc ClkVfPoint40FreqTupleAccessor
 */
FLCN_STATUS
clkVfPoint40FreqTupleAccessor_VOLT_SEC
(
    CLK_VF_POINT_40                                *pVfPoint40,
    CLK_DOMAIN_40_PROG                             *pDomain40Prog,
    CLK_VF_REL                                     *pVfRel,
    LwU8                                            lwrveIdx,
    LwBool                                          bOffseted,
    LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE    *pOutput
)
{
    CLK_VF_POINT_40_VOLT_SEC   *p40VoltSec = (CLK_VF_POINT_40_VOLT_SEC *)pVfPoint40;
    FLCN_STATUS status      = FLCN_OK;
    LwU8        clkPos      =
        clkDomain40ProgGetClkDomainPosByIdx(pDomain40Prog, pVfRel->railIdx);
    LwU8        secLwrveIdx;

    // Sanity check.
    if (!clkVfPoint40VoltSecSanityCheck(pVfPoint40, lwrveIdx))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto clkVfPoint40FreqTupleAccessor_VOLT_SEC_exit;
    }

    secLwrveIdx =
        (lwrveIdx - LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_SEC_0);

    // Validate the position.
    if (clkPos == LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkVfPoint40FreqTupleAccessor_VOLT_SEC_exit;
    }

    // Update version.
    if (pOutput->version < LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE_VERSION_20)
    {
        pOutput->version = LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE_VERSION_20;
    }

    // Copy-out frequency tuple.
    if (bOffseted)
    {
        pOutput->data.v20.sec[secLwrveIdx].freqMHz =
            p40VoltSec->offsetedVFTuple[clkPos].freqMHz;
    }
    else
    {
        pOutput->data.v20.sec[secLwrveIdx].freqMHz =
            p40VoltSec->baseVFTuple.super.freqTuple[clkPos].freqMHz;
    }

    // Copy-out the DVCO offset code.
    pOutput->data.v20.sec[secLwrveIdx].dvcoOffsetCode =
        clkVfPoint40VoltSecActiveDvcoOffsetCodeGet(p40VoltSec);

clkVfPoint40FreqTupleAccessor_VOLT_SEC_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief   CLK_VF_POINT_40_VOLT_SEC implementation of
 *          @ref BoardObjVirtualTableDynamicCast()
 *
 * @copydoc BoardObjVirtualTableDynamicCast()
 */
static void *
s_vfeVarDynamicCast_40_VOLT_SEC
(
    BOARDOBJ   *pBoardObj,
    LwU8        requestedType
)
{
    void                     *pObject        = NULL;
    CLK_VF_POINT_40_VOLT_SEC *pVfPt40VoltSec = (CLK_VF_POINT_40_VOLT_SEC *)pBoardObj;

    if (BOARDOBJ_GET_TYPE(pBoardObj) !=
        LW2080_CTRL_BOARDOBJ_TYPE(CLK, CLK_VF_POINT, 40_VOLT_SEC))
    {
        PMU_BREAKPOINT();
        goto s_vfeVarDynamicCast_40_VOLT_SEC_exit;
    }

    switch (requestedType)
    {
        case LW2080_CTRL_BOARDOBJ_TYPE(CLK, CLK_VF_POINT, BASE):
        {
            CLK_VF_POINT *pVfPt = &pVfPt40VoltSec->super.super.super;
            pObject = (void *)pVfPt;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(CLK, CLK_VF_POINT, 40):
        {
            CLK_VF_POINT_40 *pVfPt40 = &pVfPt40VoltSec->super.super;
            pObject = (void *)pVfPt40;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(CLK, CLK_VF_POINT, 40_VOLT):
        {
            CLK_VF_POINT_40_VOLT *pVfPt40Volt = &pVfPt40VoltSec->super;
            pObject = (void *)pVfPt40Volt;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(CLK, CLK_VF_POINT, 40_VOLT_SEC):
        {
            pObject = (void *)pVfPt40VoltSec;
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            break;
        }
    }

s_vfeVarDynamicCast_40_VOLT_SEC_exit:
    return pObject;
}
