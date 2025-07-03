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
 * @file clk_vf_point_35_volt_sec.c
 *
 * @brief Module managing all state related to the CLK_VF_POINT_35_VOLT_SEC structure.
 * This structure defines a point on the VF lwrve for which voltage is the
 * independent variable (i.e. fixed for this VF point) and frequency is the
 * dependent variable (i.e. varies with process, temperature, etc.).
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "perf/3x/vfe.h"
#include "volt/objvolt.h"

ct_assert(CLK_CLK_VF_POINT_35_VOLT_SEC_FREQ_TUPLE_MAX_SIZE <=
    LW2080_CTRL_CLK_CLK_VF_POINT_FREQ_TUPLE_MAX_SIZE);

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjVirtualTableDynamicCast (s_vfeVarDynamicCast_35_VOLT_SEC)
    GCC_ATTRIB_SECTION("imem_perf", "s_vfeVarDynamicCast_35_VOLT_SEC")
    GCC_ATTRIB_USED();

/* ------------------------ Global Variables ------------------------------- */
/*!
 * Virtual table for CLK_VF_POINT_35_VOLT_SEC object interfaces.
 */
BOARDOBJ_VIRTUAL_TABLE ClkVfPoint35VoltSecVirtualTable =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_DYNAMIC_CAST(s_vfeVarDynamicCast_35_VOLT_SEC)
};

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_VF_POINT_35_VOLT_SEC class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkVfPointGrpIfaceModel10ObjSet_35_VOLT_SEC
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_CLK_CLK_VF_POINT_35_VOLT_SEC_BOARDOBJ_SET *pSet =
        (RM_PMU_CLK_CLK_VF_POINT_35_VOLT_SEC_BOARDOBJ_SET *)pBoardObjDesc;
    CLK_VF_POINT_35_VOLT_SEC   *pVfPoint35VoltSec;
    FLCN_STATUS                 status;

    // Call into CLK_VF_POINT_35_VOLT super-constructor
    status = clkVfPointGrpIfaceModel10ObjSet_35_VOLT(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPointGrpIfaceModel10ObjSet_35_VOLT_SEC_exit;
    }

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVfPoint35VoltSec, *ppBoardObj,
                                  CLK, CLK_VF_POINT, 35_VOLT_SEC, &status,
                                  clkVfPointGrpIfaceModel10ObjSet_35_VOLT_SEC_exit);

    // Copy the CLK_VF_POINT_35_VOLT_SEC-specific data.
    pVfPoint35VoltSec->dvcoOffsetCodeOverride = pSet->dvcoOffsetCodeOverride;

clkVfPointGrpIfaceModel10ObjSet_35_VOLT_SEC_exit:
    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
clkVfPointIfaceModel10GetStatus_35_VOLT_SEC
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_CLK_CLK_VF_POINT_35_VOLT_SEC_BOARDOBJ_GET_STATUS *pVfPoint35VoltSecStatus =
        (RM_PMU_CLK_CLK_VF_POINT_35_VOLT_SEC_BOARDOBJ_GET_STATUS *)pPayload;
    CLK_VF_POINT_35_VOLT_SEC   *pVfPoint35VoltSec;
    LwU8                        idx;
    FLCN_STATUS                 status             = FLCN_OK;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVfPoint35VoltSec, pBoardObj,
                                  CLK, CLK_VF_POINT, 35_VOLT_SEC, &status,
                                  clkVfPointIfaceModel10GetStatus_35_VOLT_SEC_exit);

    status = clkVfPointIfaceModel10GetStatus_35(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto clkVfPointIfaceModel10GetStatus_35_VOLT_SEC_exit;
    }

    // Copy-out the _35 specific params.
    pVfPoint35VoltSecStatus->baseVFTuple = pVfPoint35VoltSec->baseVFTuple;

    // Copy-out the active DVCO offset code.
    pVfPoint35VoltSecStatus->baseVFTuple.dvcoOffsetCode =
        clkVfPoint35VoltSecActiveDvcoOffsetCodeGet(pVfPoint35VoltSec);

    // Init the output struct.
    memset(pVfPoint35VoltSecStatus->offsetedVFTuple, 0,
        (sizeof(LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE) * LW2080_CTRL_CLK_CLK_VF_POINT_FREQ_TUPLE_MAX_SIZE));

    for (idx = 0; idx < CLK_CLK_VF_POINT_35_VOLT_SEC_FREQ_TUPLE_MAX_SIZE; idx++)
    {
        pVfPoint35VoltSecStatus->offsetedVFTuple[idx] =
            pVfPoint35VoltSec->offsetedVFTuple[idx];
    }

clkVfPointIfaceModel10GetStatus_35_VOLT_SEC_exit:
    return status;
}

/*!
 * @copydoc ClkVfPointLoad
 */
FLCN_STATUS
clkVfPointLoad_35_VOLT_SEC
(
    CLK_VF_POINT            *pVfPoint,
    CLK_PROG_3X_PRIMARY      *pProg3XPrimary,
    CLK_DOMAIN_3X_PRIMARY    *pDomain3XPrimary,
    LwU8                     voltRailIdx,
    LwU8                     lwrveIdx
)
{
    CLK_DOMAIN_35_PRIMARY   *pDomain35Primary =
        (CLK_DOMAIN_35_PRIMARY *)INTERFACE_TO_BOARDOBJ_CAST(pDomain3XPrimary);
    CLK_DOMAIN             *pDomain;
    LwBoardObjIdx           clkIdx;
    FLCN_STATUS             status;

    status = clkVfPointLoad_35_VOLT(pVfPoint,
                pProg3XPrimary, pDomain3XPrimary,
                voltRailIdx, lwrveIdx);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPointLoad_35_VOLT_SEC_exit;
    }

    // Sanity check all position mapping.
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, clkIdx,
        &pDomain35Primary->primarySecondaryDomainsGrpMask.super)
    {
        LwU8 clkPos =
            clkDomain35ProgGetClkDomainPosByIdx((CLK_DOMAIN_35_PROG *)pDomain, lwrveIdx);

        //
        // Validate the position.
        // (clkPos == LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID) is NOT an error as NOT
        // all secondaries support a secondary VF lwrve.
        //
        if ((clkPos >= CLK_CLK_VF_POINT_35_VOLT_SEC_FREQ_TUPLE_MAX_SIZE) &&
            (clkPos != LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto clkVfPointLoad_35_VOLT_SEC_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

clkVfPointLoad_35_VOLT_SEC_exit:
    return status;
}

/*!
 * _VOLT implemenation.
 *
 * @copydoc ClkVfPoint35Cache
 */
FLCN_STATUS
clkVfPoint35Cache_VOLT_SEC
(
    CLK_VF_POINT_35            *pVfPoint35,
    CLK_VF_POINT_35            *pVfPoint35Last,
    CLK_DOMAIN_35_PRIMARY       *pDomain35Primary,
    CLK_PROG_35_PRIMARY         *pProg35Primary,
    LwU8                        voltRailIdx,
    LwU8                        lwrveIdx,
    LwBool                      bVFEEvalRequired
)
{
    CLK_VF_POINT_35_VOLT_SEC   *pVfPoint35VoltSec;
    CLK_DOMAIN_3X_PRIMARY       *pDomain3XPrimary    =
        CLK_CLK_DOMAIN_3X_PRIMARY_GET_FROM_35_PRIMARY(pDomain35Primary);
    LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE *pOffsetedVFTuple  =
        clkVfPoint35OffsetedVFTupleGet(pVfPoint35);
    LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTuple =
        clkVfPoint35BaseVFTupleGet(pVfPoint35);
    CLK_DOMAIN                 *pDomain;
    RM_PMU_PERF_VFE_VAR_VALUE   vfeVarVal = {0};
    RM_PMU_PERF_VFE_EQU_RESULT  result    = {0};
    LwBoardObjIdx               clkIdx;
    LwU8                        clkPos  =
        clkDomain35ProgGetClkDomainPosByIdx(
            &pDomain35Primary->super, lwrveIdx);
    LwU16                       freqMHz;
    FLCN_STATUS                 status  = FLCN_OK;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVfPoint35VoltSec, &pVfPoint35->super.super,
                                  CLK, CLK_VF_POINT, 35_VOLT_SEC, &status,
                                  clkVfPointCache_35_VOLT_SEC_exit);

    // Sanity check.
    if (!clkVfPoint35VoltSecSanityCheck(pVfPoint35, lwrveIdx))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto clkVfPointCache_35_VOLT_SEC_exit;
    }

    if ((pBaseVFTuple == NULL) ||
        (pOffsetedVFTuple == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkVfPointCache_35_VOLT_SEC_exit;
    }

    // Validate the position.
    if (clkPos == LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkVfPointCache_35_VOLT_SEC_exit;
    }

    // Init to base frequency.
    freqMHz = pBaseVFTuple->freqTuple[clkPos].freqMHz;

    // Evaluate the VFE on client's request
    if (bVFEEvalRequired)
    {
        LwU8 dvcoOffsetVfeIdx = clkProg35PrimaryDvcoOffsetVfeIdxGet(pProg35Primary,
                                    voltRailIdx,
                                    lwrveIdx);
        if (dvcoOffsetVfeIdx == PMU_PERF_VFE_EQU_INDEX_ILWALID)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkVfPointCache_35_VOLT_SEC_exit;
        }

        // 0. Compute the DVCO offset code for the voltage.
        vfeVarVal.voltage.varType  = LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_VOLTAGE;
        vfeVarVal.voltage.varValue = pVfPoint35VoltSec->super.sourceVoltageuV;
        status = vfeEquEvaluate(
            dvcoOffsetVfeIdx,                                   // vfeEquIdx
            &vfeVarVal,                                         // pValues
            1,                                                  // valCount
            LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_FREQ_MHZ,      // outputType
            &result);                                           // pResult
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfPointCache_35_VOLT_SEC_exit;
        }
        pVfPoint35VoltSec->baseVFTuple.dvcoOffsetCode = (LwU8)result.freqMHz;

        // 1. Compute the frequency for the voltage.
        result.freqMHz = 0;
        memset(&vfeVarVal, 0, sizeof(RM_PMU_PERF_VFE_VAR_VALUE));

        vfeVarVal.voltage.varType  = LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_VOLTAGE;
        vfeVarVal.voltage.varValue = pVfPoint35VoltSec->super.sourceVoltageuV;
        status = vfeEquEvaluate(
            clkProg35PrimaryVfeIdxGet(pProg35Primary,
                voltRailIdx,
                lwrveIdx),                                      // vfeEquIdx
            &vfeVarVal,                                         // pValues
            1,                                                  // valCount
            LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_FREQ_MHZ,      // outputType
            &result);                                           // pResult
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfPointCache_35_VOLT_SEC_exit;
        }
        freqMHz = (LwU16)result.freqMHz;

        // 2. VF lwrve smoothening code
        if (clkProg3XPrimaryIsVFSmoothingRequired(&pProg35Primary->primary,
                pBaseVFTuple->voltageuV) &&
            (LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL == (pProg35Primary)->super.super.source) &&
            (pVfPoint35Last != NULL))
        {
            LwU16 expectedMaxFreqMHz;
            LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTupleLast =
                clkVfPoint35BaseVFTupleGet(pVfPoint35Last);

            // Sanity check.
            if (pBaseVFTupleLast == NULL)
            {
                status = FLCN_ERR_ILWALID_ARGUMENT;
                goto clkVfPointCache_35_VOLT_SEC_exit;
            }

            expectedMaxFreqMHz =
                pBaseVFTupleLast->freqTuple[clkPos].freqMHz +
                clkProg3XPrimaryMaxFreqStepSizeMHzGet(&pProg35Primary->primary);

            freqMHz = LW_MIN(freqMHz, expectedMaxFreqMHz);
        }

        // 3. Trim VF lwrve to frequency enumeration max.
        if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_TRIM_VF_LWRVE_BY_ENUM_MAX))
        {
            freqMHz = LW_MIN(pProg35Primary->super.super.freqMaxMHz, freqMHz);
        }

        // 4. Round frequency to closest step supported by frequency generator.
        status = clkProg3XFreqMHzQuantize(
                    &pProg35Primary->super.super,            // pProg3X
                    (CLK_DOMAIN_3X_PROG *)pDomain35Primary,   // pDomain3XProg
                    &freqMHz,                               // pFreqMHz
                    LW_TRUE);                               // bFloor
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfPointCache_35_VOLT_SEC_exit;
        }

        // 5. Update the base frequency tuple
        pBaseVFTuple->freqTuple[clkPos].freqMHz = freqMHz;
    }

    //
    // 6. Update the secondary clock domains base VF lwrve.
    //    If VF lwrve trim is not supported then base lwrve of secondaries does
    //    not need to be regenerated except for VFE evaluation.
    //
    if (bVFEEvalRequired ||
        PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_TRIM_VF_LWRVE_BY_ENUM_MAX))
    {
        BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, clkIdx,
            &pDomain3XPrimary->secondaryIdxsMask.super)
        {
            CLK_DOMAIN_35_PROG *pDomain35Prog;

            pDomain35Prog = CLK_DOMAIN_35_PROG_GET(clkIdx);
            if (pDomain35Prog == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_INDEX;
                goto clkVfPointCache_35_VOLT_SEC_exit;
            }

            clkPos = clkDomain35ProgGetClkDomainPosByIdx(pDomain35Prog, lwrveIdx);

            // Validate the position.
            if (clkPos == LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID)
            {
                // NOT all secondaries supports secondary lwrves.
                continue;
            }

            // Init the primary frequency.
            pBaseVFTuple->freqTuple[clkPos].freqMHz = freqMHz;

            status = clkProg3XPrimaryFreqTranslatePrimaryToSecondary(
                        &pProg35Primary->primary,                             // pProg3XPrimary
                        pDomain3XPrimary,                                    // pDomain3XPrimary
                        BOARDOBJ_GRP_IDX_TO_8BIT(clkIdx),                   // clkDomIdx
                        &pBaseVFTuple->freqTuple[clkPos].freqMHz,           // pFreqMHz
                        LW_FALSE,                                           // bOCAjdAndQuantize
                        LW_TRUE);                                           // bQuantize
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkVfPointCache_35_VOLT_SEC_exit;
            }

            // Smooth the secondary VF lwrve using its ratio'd ramp rate.
            if (clkProg3XPrimaryIsVFSmoothingRequired(&pProg35Primary->primary,
                    pBaseVFTuple->voltageuV) &&
                (LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL == (pProg35Primary)->super.super.source) &&
                (pVfPoint35Last != NULL))
            {
                LwU8                secondaryProgIdx;
                LwU16               freqStepSizeMHz;
                LwU16               expectedMaxFreqMHz;
                CLK_PROG_3X        *pProg3xSecondary;
                CLK_DOMAIN_3X_PROG *pDomain3xProgSecondary =
                    CLK_CLK_DOMAIN_3X_PROG_GET_BY_IDX(CLK_CLK_DOMAINS_GET(), clkIdx);
                LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTupleLast =
                    clkVfPoint35BaseVFTupleGet(pVfPoint35Last);

                // Get the secondary VF smoothing step size (ramp rate) from primary.
                freqStepSizeMHz = clkProg3XPrimaryMaxFreqStepSizeMHzGet(&pProg35Primary->primary);

                status = clkProg3XPrimaryFreqTranslatePrimaryToSecondary(
                            &pProg35Primary->primary,                 // pProg3XPrimary
                            pDomain3XPrimary,                        // pDomain3XPrimary
                            clkIdx,                                 // clkDomIdx
                            &freqStepSizeMHz,                       // pFreqMHz
                            LW_FALSE,                               // bReqFreqDeltaAdj
                            LW_FALSE);                              // bQuantize
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto clkVfPointCache_35_VOLT_SEC_exit;
                }

                // Get the secondary programming entry for this frequency
                secondaryProgIdx = clkDomain3XProgGetClkProgIdxFromFreq(
                                    pDomain3xProgSecondary, freqStepSizeMHz, LW_FALSE);
                if (secondaryProgIdx == LW2080_CTRL_CLK_CLK_PROG_IDX_ILWALID)
                {
                    status = FLCN_ERR_ILWALID_ARGUMENT;
                    PMU_BREAKPOINT();
                    goto clkVfPointCache_35_VOLT_SEC_exit;
                }
                pProg3xSecondary = (CLK_PROG_3X *)BOARDOBJGRP_OBJ_GET(CLK_PROG, secondaryProgIdx);

                //
                // Note that we are quantizing down the Primary -> Secondary translated
                // frequency, we MUST quantize up the step size.
                //
                status = clkProg3XFreqMHzQuantize(
                            pProg3xSecondary,           // pProg3X
                            pDomain3xProgSecondary,     // pDomain3XProg
                            &freqStepSizeMHz,       // pFreqMHz
                            LW_FALSE);              // bFloor
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto clkVfPointCache_35_VOLT_SEC_exit;
                }

                // Sanity check.
                if (pBaseVFTupleLast == NULL)
                {
                    status = FLCN_ERR_ILWALID_ARGUMENT;
                    goto clkVfPointCache_35_VOLT_SEC_exit;
                }

                expectedMaxFreqMHz =
                    pBaseVFTupleLast->freqTuple[clkPos].freqMHz +
                    freqStepSizeMHz;

                pBaseVFTuple->freqTuple[clkPos].freqMHz =
                    LW_MIN(pBaseVFTuple->freqTuple[clkPos].freqMHz, expectedMaxFreqMHz);
            }
        }
        BOARDOBJGRP_ITERATOR_END;
    }

    // 7. Adjust by the VF point frequency delta.
    if ((PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_OVOC_SUPPORTED)) &&
        (clkDomain3XProgIsFreqDeltaNonZero(&pVfPoint35VoltSec->super.freqDelta)))
    {
        freqMHz = clkFreqDeltaAdjust(freqMHz, &pVfPoint35VoltSec->super.freqDelta);

        status = clkProg3XFreqMHzQuantize(
                    &pProg35Primary->super.super,            // pProg3X
                    (CLK_DOMAIN_3X_PROG *)pDomain35Primary,   // pDomain3XProg
                    &freqMHz,                               // pFreqMHz
                    LW_TRUE);                               // bFloor
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfPointCache_35_VOLT_SEC_exit;
        }
    }

    //
    // 8. Update the offseted VF tuple
    //    PP-TODO : There is no point in callwlating intermediate offseted
    //    secondary clock frequency. This can be deleted.
    //
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, clkIdx,
        &pDomain35Primary->primarySecondaryDomainsGrpMask.super)
    {
        CLK_DOMAIN_35_PROG *pDomain35Prog;

        pDomain35Prog = CLK_DOMAIN_35_PROG_GET(clkIdx);
        if (pDomain35Prog == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkVfPointCache_35_VOLT_SEC_exit;
        }

        clkPos = clkDomain35ProgGetClkDomainPosByIdx(pDomain35Prog, lwrveIdx);

        // Validate the position.
        if (clkPos == LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID)
        {
            // NOT all secondaries supports secondary lwrves.
            continue;
        }

        // Use the base voltage value.
        pOffsetedVFTuple[clkPos].voltageuV =
            pBaseVFTuple->voltageuV;

        // Use the above callwlated VF point frequency value.
        pOffsetedVFTuple[clkPos].freqMHz   = freqMHz;

        // Colwert primary -> secondary
        if (BOARDOBJ_GET_TYPE(pDomain) !=
            LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY)
        {
            status = clkProg3XPrimaryFreqTranslatePrimaryToSecondary(
                        &pProg35Primary->primary,                     // pProg3XPrimary
                        pDomain3XPrimary,                            // pDomain3XPrimary
                        BOARDOBJ_GRP_IDX_TO_8BIT(clkIdx),           // clkDomIdx
                        &pOffsetedVFTuple[clkPos].freqMHz,          // pFreqMHz
                        LW_FALSE,                                   // bOCAjdAndQuantize
                        LW_TRUE);                                   // bQuantize
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkVfPointCache_35_VOLT_SEC_exit;
            }
        }
    }
    BOARDOBJGRP_ITERATOR_END;

clkVfPointCache_35_VOLT_SEC_exit:
    return status;
}

/*!
 * @copydoc ClkVfPoint35Smoothing
 */
FLCN_STATUS
clkVfPoint35Smoothing_VOLT_SEC
(
    CLK_VF_POINT_35            *pVfPoint35,
    CLK_VF_POINT_35            *pVfPoint35Last,
    CLK_DOMAIN_35_PRIMARY       *pDomain35Primary,
    CLK_PROG_35_PRIMARY         *pProg35Primary,
    LwU8                        lwrveIdx
)
{
    CLK_DOMAIN_3X_PRIMARY   *pDomain3XPrimary =
        CLK_CLK_DOMAIN_3X_PRIMARY_GET_FROM_35_PRIMARY(pDomain35Primary);
    CLK_DOMAIN             *pDomain;
    LwBoardObjIdx           clkIdx;
    LwU8                    clkPos;
    FLCN_STATUS             status = FLCN_OK;
    LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTuple =
        clkVfPoint35BaseVFTupleGet(pVfPoint35);
    LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE *pOffsetedVFTuple  =
        clkVfPoint35OffsetedVFTupleGet(pVfPoint35);
    LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE *pOffsetedVFTupleLast;

    // Sanity check.
    if (!clkVfPoint35VoltSecSanityCheck(pVfPoint35, lwrveIdx))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto clkVfPoint35Smoothing_VOLT_SEC_exit;
    }

    if ((pBaseVFTuple == NULL) ||
        (pOffsetedVFTuple == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkVfPoint35Smoothing_VOLT_SEC_exit;
    }

    //
    // If VF lwrve smoothing is not required, bail early
    // Use thebase voltage to ensure that we always smooth
    // the same subset of VF lwrve. It should not change
    // based on offset.
    //
    if (!((clkProg3XPrimaryIsVFSmoothingRequired(&pProg35Primary->primary, pBaseVFTuple->voltageuV)) &&
          (LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL == (pProg35Primary)->super.super.source)))
    {
        goto clkVfPoint35Smoothing_VOLT_SEC_exit;
    }

    // If this is the first iteration, exit.
    if (pVfPoint35Last == NULL)
    {
        goto clkVfPoint35Smoothing_VOLT_SEC_exit;
    }
    pOffsetedVFTupleLast = clkVfPoint35OffsetedVFTupleGet(pVfPoint35Last);

    // Sanity check.
    if (pOffsetedVFTupleLast == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkVfPoint35Smoothing_VOLT_SEC_exit;
    }

    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, clkIdx,
        &pDomain35Primary->primarySecondaryDomainsGrpMask.super)
    {
        CLK_DOMAIN_35_PROG *pDomain35Prog;
        LwU16               freqMHz;

        pDomain35Prog = CLK_DOMAIN_35_PROG_GET(clkIdx);
        if (pDomain35Prog == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkVfPoint35Smoothing_VOLT_SEC_exit;
        }

        clkPos = clkDomain35ProgGetClkDomainPosByIdx(pDomain35Prog, lwrveIdx);

        // Validate the position.
        if (clkPos == LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID)
        {
            // NOT all secondaries supports secondary lwrves.
            continue;
        }

        // Get the primary clock domain's frequency step size.
        freqMHz = clkProg3XPrimaryMaxFreqStepSizeMHzGet(&pProg35Primary->primary);

        // Colwert primary -> secondary
        if (BOARDOBJ_GET_TYPE(pDomain) !=
            LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY)
        {
            LwU8                secondaryProgIdx;
            CLK_PROG_3X        *pProg3xSecondary;
            CLK_DOMAIN_3X_PROG *pDomain3xProgSecondary =
                CLK_CLK_DOMAIN_3X_PROG_GET_BY_IDX(CLK_CLK_DOMAINS_GET(), clkIdx);

            status = clkProg3XPrimaryFreqTranslatePrimaryToSecondary(
                        &pProg35Primary->primary,                         // pProg3XPrimary
                        pDomain3XPrimary,                                // pDomain3XPrimary
                        BOARDOBJ_GRP_IDX_TO_8BIT(clkIdx),               // clkDomIdx
                        &freqMHz,                                       // pFreqMHz
                        LW_FALSE,                                       // bReqFreqDeltaAdj
                        LW_FALSE);                                      // bQuantize
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkVfPoint35Smoothing_VOLT_SEC_exit;
            }

            // Get the secondary programming entry for this frequency
            secondaryProgIdx = clkDomain3XProgGetClkProgIdxFromFreq(
                                pDomain3xProgSecondary, freqMHz, LW_FALSE);
            if (secondaryProgIdx == LW2080_CTRL_CLK_CLK_PROG_IDX_ILWALID)
            {
                status = FLCN_ERR_ILWALID_ARGUMENT;
                PMU_BREAKPOINT();
                goto clkVfPoint35Smoothing_VOLT_SEC_exit;
            }
            pProg3xSecondary = (CLK_PROG_3X *)BOARDOBJGRP_OBJ_GET(CLK_PROG, secondaryProgIdx);

            //
            // The smoothing step size must be identical for both base
            // and offseted VF lwrve generation to ensure that they
            // are identical lwrve in absence of any frequency OC.
            //
            status = clkProg3XFreqMHzQuantize(
                        pProg3xSecondary,           // pProg3X
                        pDomain3xProgSecondary,     // pDomain3XProg
                        &freqMHz,               // pFreqMHz
                        LW_FALSE);              // bFloor
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkVfPoint35Smoothing_VOLT_SEC_exit;
            }
        }

        // Callwlate the expected frequency based on last evaluated VF point.
        if (pOffsetedVFTupleLast[clkPos].freqMHz >= freqMHz)
        {
            freqMHz = pOffsetedVFTupleLast[clkPos].freqMHz - freqMHz;
        }
        else
        {
            freqMHz = 0;
        }

        // Compare the current vs expected frequency
        if (freqMHz > pOffsetedVFTuple[clkPos].freqMHz)
        {
            // Set the current VF point frequency to expected frequency
            pOffsetedVFTuple[clkPos].freqMHz = freqMHz;

            //
            // Quantize up the frequency to ensure that quantized frequency
            // is within the allowed bound
            //
            status = clkProg3XFreqMHzQuantize(
                        &pProg35Primary->super.super,        // pProg3X
                        (CLK_DOMAIN_3X_PROG *)pDomain,       // pDomain3XProg
                        &pOffsetedVFTuple[clkPos].freqMHz,  // pFreqMHz
                        LW_FALSE);                          // bFloor
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkVfPoint35Smoothing_VOLT_SEC_exit;
            }
        }
    }
    BOARDOBJGRP_ITERATOR_END;

clkVfPoint35Smoothing_VOLT_SEC_exit:
    return status;
}

/*!
 * @Copydoc ClkVfPoint35FreqTupleAccessor
 */
FLCN_STATUS
clkVfPoint35FreqTupleAccessor_VOLT_SEC
(
    CLK_VF_POINT_35                                *pVfPoint35,
    CLK_DOMAIN_3X_PRIMARY                           *pDomain3XPrimary,
    LwU8                                            clkDomIdx,
    LwU8                                            lwrveIdx,
    LwBool                                          bOffseted,
    LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE    *pOutput
)
{
    CLK_VF_POINT_35_VOLT_SEC *p35VoltSec    = (CLK_VF_POINT_35_VOLT_SEC *)pVfPoint35;
    CLK_DOMAIN_35_PROG       *pDomain35Prog = CLK_DOMAIN_35_PROG_GET(clkDomIdx);
    FLCN_STATUS               status        = FLCN_OK;
    LwU8                      clkPos;
    LwU8                      secLwrveIdx;

    if (pDomain35Prog == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto clkVfPoint35FreqTupleAccessor_VOLT_SEC_exit;
    }

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(p35VoltSec, &pVfPoint35->super.super,
                                  CLK, CLK_VF_POINT, 35_VOLT_SEC, &status,
                                  clkVfPoint35FreqTupleAccessor_VOLT_SEC_exit);

    clkPos = clkDomain35ProgGetClkDomainPosByIdx(pDomain35Prog, lwrveIdx);

    // Sanity check.
    if (!clkVfPoint35VoltSecSanityCheck(pVfPoint35, lwrveIdx))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto clkVfPoint35FreqTupleAccessor_VOLT_SEC_exit;
    }

    secLwrveIdx =
        (lwrveIdx - LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_SEC_0);

    // Validate the position.
    if (clkPos == LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkVfPoint35FreqTupleAccessor_VOLT_SEC_exit;
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
            p35VoltSec->offsetedVFTuple[clkPos].freqMHz;
    }
    else
    {
        pOutput->data.v20.sec[secLwrveIdx].freqMHz =
            p35VoltSec->baseVFTuple.super.freqTuple[clkPos].freqMHz;
    }

    // Copy-out the DVCO offset code.
    pOutput->data.v20.sec[secLwrveIdx].dvcoOffsetCode =
        clkVfPoint35VoltSecActiveDvcoOffsetCodeGet(p35VoltSec);

clkVfPoint35FreqTupleAccessor_VOLT_SEC_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief   CLK_VF_POINT_35_VOLT_SEC implementation of
 *          @ref BoardObjVirtualTableDynamicCast()
 *
 * @copydoc BoardObjVirtualTableDynamicCast()
 */
static void *
s_vfeVarDynamicCast_35_VOLT_SEC
(
    BOARDOBJ   *pBoardObj,
    LwU8        requestedType
)
{
    void                     *pObject        = NULL;
    CLK_VF_POINT_35_VOLT_SEC *pVfPt35VoltSec = (CLK_VF_POINT_35_VOLT_SEC *)pBoardObj;

    if (BOARDOBJ_GET_TYPE(pBoardObj) !=
        LW2080_CTRL_BOARDOBJ_TYPE(CLK, CLK_VF_POINT, 35_VOLT_SEC))
    {
        PMU_BREAKPOINT();
        goto s_vfeVarDynamicCast_35_VOLT_SEC_exit;
    }

    switch (requestedType)
    {
        case LW2080_CTRL_BOARDOBJ_TYPE(CLK, CLK_VF_POINT, BASE):
        {
            CLK_VF_POINT *pVfPt = &pVfPt35VoltSec->super.super.super;
            pObject = (void *)pVfPt;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(CLK, CLK_VF_POINT, 35):
        {
            CLK_VF_POINT_35 *pVfPt35 = &pVfPt35VoltSec->super.super;
            pObject = (void *)pVfPt35;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(CLK, CLK_VF_POINT, 35_VOLT):
        {
            CLK_VF_POINT_35_VOLT *pVfPt35Volt = &pVfPt35VoltSec->super;
            pObject = (void *)pVfPt35Volt;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(CLK, CLK_VF_POINT, 35_VOLT_SEC):
        {
            pObject = (void *)pVfPt35VoltSec;
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            break;
        }
    }

s_vfeVarDynamicCast_35_VOLT_SEC_exit:
    return pObject;
}
