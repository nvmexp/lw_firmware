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
 * @file clk_vf_point_40_volt.c
 *
 * @brief Module managing all state related to the CLK_VF_POINT_VOLT structure.
 * This structure defines a point on the VF lwrve for which voltage is the
 * independent variable (i.e. fixed for this VF point) and frequency is the
 * dependent variable (i.e. varies with process, temperature, etc.).
 *
 * The CLK_VF_POINT_VOLT class is intended to be used to describe frequency
 * regions which are generated by the NAFLL, whose ADC/LUT has fixed voltage
 * steps.  The CLK_PROG entry for this region will generate a CLK_VF_POINT_VOLT
 * object for each voltage in the ADC/LUT.
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "perf/3x/vfe.h"
#include "volt/objvolt.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
static FLCN_STATUS
s_clkVfPoint40OffsetedVFCache_VOLT_HELPER
(
    CLK_VF_POINT_40                             *pVfPoint40,
    CLK_VF_POINT_40                             *pVfPoint40Last,
    CLK_DOMAIN_40_PROG                          *pDomain40Prog,
    CLK_VF_REL                                  *pVfRel,
    LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE  *pBaseVFTuple,
    LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE       *pOffsetedVFTuple
) GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "s_clkVfPoint40OffsetedVFCache_VOLT_HELPER");
static FLCN_STATUS
s_clkVfPoint40OffsetVFCache_VOLT_HELPER
(
    CLK_VF_POINT_40                             *pVfPoint40,
    CLK_VF_POINT_40                             *pVfPoint40Last,
    CLK_DOMAIN_40_PROG                          *pDomain40Prog,
    CLK_VF_REL                                  *pVfRel,
    LW2080_CTRL_CLK_OFFSET_VF_TUPLE             *pOffsetVFTuple
) GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "s_clkVfPoint40OffsetVFCache_VOLT_HELPER");
static FLCN_STATUS
s_clkVfPoint40VoltSmoothing_HELPER
(
    CLK_VF_POINT_40_VOLT                       *pVfPoint40Volt,
    CLK_DOMAIN_40_PROG                         *pDomain40Prog,
    CLK_VF_REL_RATIO_VOLT                      *pVfRelRatioVolt,
    LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTuple,
    LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE      *pOffsetedVFTuple,
    LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE      *pOffsetedVFTupleLast
) GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "s_clkVfPoint40VoltSmoothing_HELPER");

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_VF_POINT_40_VOLT class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkVfPointGrpIfaceModel10ObjSet_40_VOLT
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_CLK_CLK_VF_POINT_40_VOLT_BOARDOBJ_SET *pSet =
        (RM_PMU_CLK_CLK_VF_POINT_40_VOLT_BOARDOBJ_SET *)pBoardObjDesc;
    CLK_VF_POINT_40_VOLT   *pVfPoint40Volt;
    FLCN_STATUS             status;

    // Call into CLK_VF_POINT_40 super-constructor
    status = clkVfPointGrpIfaceModel10ObjSet_40(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPointGrpIfaceModel10ObjSet_40_VOLT_exit;
    }

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVfPoint40Volt, *ppBoardObj, CLK, CLK_VF_POINT, 40_VOLT,
                                  &status, clkVfPointGrpIfaceModel10ObjSet_40_VOLT_exit);

    // Copy the CLK_VF_POINT_40_VOLT-specific data.
    pVfPoint40Volt->sourceVoltageuV = pSet->sourceVoltageuV;
    pVfPoint40Volt->freqDelta       = pSet->freqDelta;

clkVfPointGrpIfaceModel10ObjSet_40_VOLT_exit:
    return status;
}

/*!
 * @copydoc ClkVfPointLoad
 */
FLCN_STATUS
clkVfPoint40Load_VOLT
(
    CLK_VF_POINT_40        *pVfPoint40,
    CLK_DOMAIN_40_PROG     *pDomain40Prog,
    CLK_VF_REL             *pVfRel,
    LwU8                    lwrveIdx
)
{
    CLK_VF_POINT_40_VOLT   *pVfPoint40Volt;
    VOLT_RAIL              *pVoltRail;
    FLCN_STATUS             status         = FLCN_ERR_NOT_SUPPORTED;
    LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTuple;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVfPoint40Volt, &pVfPoint40->super.super, CLK, CLK_VF_POINT, 40_VOLT,
                                  &status, clkVfPoint40Load_VOLT_exit);
    pBaseVFTuple = clkVfPoint40BaseVFTupleGet(pVfPoint40);

    // Sanity check.
    if (pBaseVFTuple == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkVfPoint40Load_VOLT_exit;
    }

    pBaseVFTuple->voltageuV = pVfPoint40Volt->sourceVoltageuV;

    // Round and bound the base voltage.
    pVoltRail = VOLT_RAIL_GET(pVfRel->railIdx);
    if (pVoltRail == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto clkVfPoint40Load_VOLT_exit;
    }
    status = voltRailRoundVoltage(pVoltRail,            // pRail
                (LwS32*)&pBaseVFTuple->voltageuV,       // pVoltageuV
                LW_TRUE,                                // bRoundUp
                LW_TRUE);                               // bBound
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPoint40Load_VOLT_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_BASE_VF_CACHE))
    {
        // Update the cache
        LwU16 vfIdx = BOARDOBJ_GET_GRP_IDX(&pVfPoint40->super.super);
        (void)vfIdx;

        clkVfPointsBaseVfCacheVoltSet(lwrveIdx, 0U, vfIdx, CLK_VF_PRIMARY_POS, pBaseVFTuple->voltageuV);
    }

clkVfPoint40Load_VOLT_exit:
    return status;
}

/*!
 * @copydoc ClkVfPoint40Cache
 */
FLCN_STATUS
clkVfPoint40Cache_VOLT
(
    CLK_VF_POINT_40            *pVfPoint40,
    CLK_VF_POINT_40            *pVfPoint40Last,
    CLK_DOMAIN_40_PROG         *pDomain40Prog,
    CLK_VF_REL                 *pVfRel,
    LwU8                        lwrveIdx,
    LwBool                      bVFEEvalRequired
)
{
    LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE      *pOffsetedVFTuple =
        clkVfPoint40OffsetedVFTupleGet(pVfPoint40);
    LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTuple     =
        clkVfPoint40BaseVFTupleGet(pVfPoint40);
    CLK_VF_POINT_40_VOLT       *pVfPoint40Volt  = (CLK_VF_POINT_40_VOLT *)pVfPoint40;
    RM_PMU_PERF_VFE_VAR_VALUE   vfeVarVal;
    RM_PMU_PERF_VFE_EQU_RESULT  result;
    LW2080_CTRL_CLK_VF_INPUT    inputFreq;
    LW2080_CTRL_CLK_VF_OUTPUT   outputFreq;
    FLCN_STATUS                 status  = FLCN_OK;

    // Sanity check.
    if ((pBaseVFTuple     == NULL) ||
        (pOffsetedVFTuple == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkVfPointCache_40_VOLT_exit;
    }

    // Evaluate the VFE on client's request
    if (bVFEEvalRequired)
    {
        LwU16 vfIdx = BOARDOBJ_GET_GRP_IDX(&pVfPoint40->super.super);
        (void)vfIdx;

        if (clkVfPointsVfCacheIsValidGet(lwrveIdx, 0U))
        {
            pBaseVFTuple->freqTuple[CLK_VF_PRIMARY_POS].freqMHz =
                clkVfPointsBaseVfCacheFreqGet(lwrveIdx, 0U, vfIdx, CLK_VF_PRIMARY_POS);

            if (pBaseVFTuple->freqTuple[CLK_VF_PRIMARY_POS].freqMHz == 0U)
            {
                status = FLCN_ERR_ILWALID_STATE;
                PMU_BREAKPOINT();
                goto clkVfPointCache_40_VOLT_exit;
            }
        }
        else
        {
            LwU16 freqMHz;

            // Compute the frequency for the voltage.
            vfeVarVal.voltage.varType  = LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_VOLTAGE;
            vfeVarVal.voltage.varValue = pVfPoint40Volt->sourceVoltageuV;
            status = vfeEquEvaluate(
                        clkVfRelVfeIdxGet(pVfRel, lwrveIdx),            // vfeEquIdx
                        &vfeVarVal,                                     // pValues
                        1,                                              // valCount
                        LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_FREQ_MHZ,  // outputType
                        &result);                                       // pResult
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkVfPointCache_40_VOLT_exit;
            }
            freqMHz = (LwU16)result.freqMHz;

            // VF lwrve smoothening code
            if  ((pVfPoint40Last != NULL) &&
                 (BOARDOBJ_GET_TYPE(pVfRel) ==
                    LW2080_CTRL_CLK_CLK_VF_REL_TYPE_RATIO_VOLT))
            {
                CLK_VF_REL_RATIO_VOLT *pVfRelRatioVolt = (CLK_VF_REL_RATIO_VOLT *)pVfRel;

                if (clkVfRelRatioVoltIsVFSmoothingRequired(pVfRelRatioVolt,
                                                           pBaseVFTuple->voltageuV))
                {
                    LwU16                                       expectedMaxFreqMHz;
                    LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTupleLast =
                        clkVfPoint40BaseVFTupleGet(pVfPoint40Last);

                    // Sanity check.
                    if (pBaseVFTupleLast == NULL)
                    {
                        status = FLCN_ERR_ILWALID_ARGUMENT;
                        PMU_BREAKPOINT();
                        goto clkVfPointCache_40_VOLT_exit;
                    }

                    expectedMaxFreqMHz =
                        pBaseVFTupleLast->freqTuple[CLK_VF_PRIMARY_POS].freqMHz +
                        clkVfRelRatioVoltMaxFreqStepSizeMHzGet(pVfRelRatioVolt,
                                                               pBaseVFTuple->voltageuV);

                    freqMHz = LW_MIN(freqMHz, expectedMaxFreqMHz);
                }
            }

            // Trim VF lwrve to VF max and frequency enumeration max.
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_TRIM_VF_LWRVE_BY_ENUM_MAX))
            {
                freqMHz = LW_MIN(clkVfRelFreqMaxMHzGet(pVfRel), freqMHz);
                freqMHz = LW_MIN(clkDomain40ProgEnumFreqMaxMHzGet(pDomain40Prog), freqMHz);
            }

            // Init
            LW2080_CTRL_CLK_VF_INPUT_INIT(&inputFreq);
            LW2080_CTRL_CLK_VF_OUTPUT_INIT(&outputFreq);

            inputFreq.value = freqMHz;
            status = clkDomainProgFreqQuantize_40(
                        CLK_DOMAIN_PROG_GET_FROM_40_PROG(pDomain40Prog),    // pDomainProg
                        &inputFreq,                                         // pInputFreq
                        &outputFreq,                                        // pOutputFreq
                        LW_TRUE,                                            // bFloor
                        LW_FALSE);                                          // bBound
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkVfPointCache_40_VOLT_exit;
            }
            pBaseVFTuple->freqTuple[CLK_VF_PRIMARY_POS].freqMHz  = (LwU16) outputFreq.value;
        }
    }

    // Adjust by the VF point frequency delta.
    status = s_clkVfPoint40OffsetedVFCache_VOLT_HELPER(
                pVfPoint40,
                pVfPoint40Last,
                pDomain40Prog,
                pVfRel,
                pBaseVFTuple,
                pOffsetedVFTuple);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPointCache_40_VOLT_exit;
    }

clkVfPointCache_40_VOLT_exit:
    return status;
}

/*!
 * @copydoc ClkVfPoint40Cache
 */
static FLCN_STATUS
s_clkVfPoint40OffsetedVFCache_VOLT_HELPER
(
    CLK_VF_POINT_40                             *pVfPoint40,
    CLK_VF_POINT_40                             *pVfPoint40Last,
    CLK_DOMAIN_40_PROG                          *pDomain40Prog,
    CLK_VF_REL                                  *pVfRel,
    LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE  *pBaseVFTuple,
    LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE       *pOffsetedVFTuple
)
{
    CLK_VF_POINT_40_VOLT       *pVfPoint40Volt  = (CLK_VF_POINT_40_VOLT *)pVfPoint40;
    LW2080_CTRL_CLK_VF_INPUT    inputFreq;
    LW2080_CTRL_CLK_VF_OUTPUT   outputFreq;
    FLCN_STATUS                 status  = FLCN_OK;

    // Sanity check.
    if ((pBaseVFTuple     == NULL) ||
        (pOffsetedVFTuple == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto s_clkVfPoint40OffsetedVFCache_VOLT_HELPER_exit;
    }

    // Adjust by the VF point frequency delta.
    if ((PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_OVOC_SUPPORTED)) &&
        (clkDomain40ProgIsOCEnabled(pDomain40Prog))                 &&
        (clkVfRelOVOCEnabled(pVfRel))                               &&
        (clkDomain40ProgIsFreqDeltaNonZero(&pVfPoint40Volt->freqDelta)))
    {
        // Set offseted voltage equal to base voltage.
        pOffsetedVFTuple[CLK_VF_PRIMARY_POS].voltageuV =
            pBaseVFTuple->voltageuV;

        // Adjust offseted frequency with freq delta.
        pOffsetedVFTuple[CLK_VF_PRIMARY_POS].freqMHz =
             clkFreqDeltaAdjust(pBaseVFTuple->freqTuple[CLK_VF_PRIMARY_POS].freqMHz,
                                &pVfPoint40Volt->freqDelta);

        // Init
        LW2080_CTRL_CLK_VF_INPUT_INIT(&inputFreq);
        LW2080_CTRL_CLK_VF_OUTPUT_INIT(&outputFreq);

        inputFreq.value = pOffsetedVFTuple[CLK_VF_PRIMARY_POS].freqMHz;
        status = clkDomainProgFreqQuantize_40(
                    CLK_DOMAIN_PROG_GET_FROM_40_PROG(pDomain40Prog),    // pDomainProg
                    &inputFreq,                                         // pInputFreq
                    &outputFreq,                                        // pOutputFreq
                    LW_TRUE,                                            // bFloor
                    LW_FALSE);                                          // bBound
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_clkVfPoint40OffsetedVFCache_VOLT_HELPER_exit;
        }
        pOffsetedVFTuple[CLK_VF_PRIMARY_POS].freqMHz = (LwU16) outputFreq.value;
    }
    else
    {
        // Set offseted tuple equal to base tuple.
        pOffsetedVFTuple[CLK_VF_PRIMARY_POS].voltageuV =
            pBaseVFTuple->voltageuV;
        pOffsetedVFTuple[CLK_VF_PRIMARY_POS].freqMHz =
            pBaseVFTuple->freqTuple[CLK_VF_PRIMARY_POS].freqMHz;
    }

s_clkVfPoint40OffsetedVFCache_VOLT_HELPER_exit:
    return status;
}

/*!
 * @copydoc ClkVfPoint40OffsetVFCache
 */
FLCN_STATUS
clkVfPoint40OffsetVFCache_VOLT
(
    CLK_VF_POINT_40            *pVfPoint40,
    CLK_VF_POINT_40            *pVfPoint40Last,
    CLK_DOMAIN_40_PROG         *pDomain40Prog,
    CLK_VF_REL                 *pVfRel
)
{
    LW2080_CTRL_CLK_OFFSET_VF_TUPLE   *pOffsetVFTuple =
        clkVfPoint40OffsetVFTupleGet(pVfPoint40);

    FLCN_STATUS                 status  = FLCN_OK;

    // Sanity check.
    if (pOffsetVFTuple == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkVfPointOffsetVFCache_40_VOLT_exit;
    }

    // Adjust by the VF point frequency delta.
    status = s_clkVfPoint40OffsetVFCache_VOLT_HELPER(
                pVfPoint40,
                pVfPoint40Last,
                pDomain40Prog,
                pVfRel,
                pOffsetVFTuple);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPointOffsetVFCache_40_VOLT_exit;
    }

clkVfPointOffsetVFCache_40_VOLT_exit:
    return status;
}

/*!
 * @copydoc ClkVfPoint40OffsetVFCache
 */
static FLCN_STATUS
s_clkVfPoint40OffsetVFCache_VOLT_HELPER
(
    CLK_VF_POINT_40                     *pVfPoint40,
    CLK_VF_POINT_40                     *pVfPoint40Last,
    CLK_DOMAIN_40_PROG                  *pDomain40Prog,
    CLK_VF_REL                          *pVfRel,
    LW2080_CTRL_CLK_OFFSET_VF_TUPLE     *pOffsetVFTuple
)
{
    CLK_VF_POINT_40_VOLT       *pVfPoint40Volt  = (CLK_VF_POINT_40_VOLT *)pVfPoint40;
    FLCN_STATUS                 status  = FLCN_OK;

    // Sanity check.
    if (pOffsetVFTuple == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto s_clkVfPoint40OffsetVFCache_VOLT_HELPER_exit;
    }

    // Adjust by the VF point frequency delta.
    if ((PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_OVOC_SUPPORTED)) &&
        (clkDomain40ProgIsOCEnabled(pDomain40Prog))                 &&
        (clkVfRelOVOCEnabled(pVfRel))                               &&
        (clkDomain40ProgIsFreqDeltaNonZero(&pVfPoint40Volt->freqDelta)))
    {
        // Adjust offseted frequency with freq delta.
        pOffsetVFTuple[CLK_VF_PRIMARY_POS].freqMHz = 
            clkOffsetVFFreqDeltaAdjust(0, &pVfPoint40Volt->freqDelta);

        pOffsetVFTuple[CLK_VF_PRIMARY_POS].voltageuV = 0;
    }
    else
    {
        // Set offseted tuple equal to base tuple.
        pOffsetVFTuple[CLK_VF_PRIMARY_POS].voltageuV = 0;
        pOffsetVFTuple[CLK_VF_PRIMARY_POS].freqMHz   = 0;
    }

s_clkVfPoint40OffsetVFCache_VOLT_HELPER_exit:
    return status;
}

/*!
 * @copydoc ClkVfPoint40BaseVFCache
 */
FLCN_STATUS
clkVfPoint40BaseVFCache_VOLT
(
    CLK_VF_POINT_40            *pVfPoint40,
    CLK_VF_POINT_40            *pVfPoint40Last,
    CLK_DOMAIN_40_PROG         *pDomain40Prog,
    CLK_VF_REL                 *pVfRel,
    LwU8                        lwrveIdx,
    LwU8                        cacheIdx
)
{
    LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTuple     =
        clkVfPointsBaseVfCacheBaseVFTupleGet(lwrveIdx, cacheIdx,
            BOARDOBJ_GET_GRP_IDX(&pVfPoint40->super.super));
    CLK_VF_POINT_40_VOLT       *pVfPoint40Volt  = (CLK_VF_POINT_40_VOLT *)pVfPoint40;
    RM_PMU_PERF_VFE_VAR_VALUE   vfeVarVal[2];
    RM_PMU_PERF_VFE_EQU_RESULT  result;
    LW2080_CTRL_CLK_VF_INPUT    inputFreq;
    LW2080_CTRL_CLK_VF_OUTPUT   outputFreq;
    LwU16                       freqMHz;
    FLCN_STATUS                 status  = FLCN_OK;

    // Sanity check.
    if (pBaseVFTuple == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkVfPointBaseVFCache_40_VOLT_exit;
    }

    // Compute the frequency for the voltage at specified temperature.
    vfeVarVal[0].voltage.varType        = LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_VOLTAGE;
    vfeVarVal[0].voltage.varValue       = pVfPoint40Volt->sourceVoltageuV;

    vfeVarVal[1].temperature.varType    = LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_SENSED_TEMP;
    vfeVarVal[1].temperature.varValue   = RM_PMU_CELSIUS_TO_LW_TEMP(cacheIdx * 5);

    status = vfeEquEvaluate(
                clkVfRelVfeIdxGet(pVfRel, lwrveIdx),            // vfeEquIdx
                vfeVarVal,                                      // pValues
                2U,                                             // valCount
                LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_FREQ_MHZ,  // outputType
                &result);                                       // pResult
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPointBaseVFCache_40_VOLT_exit;
    }
    freqMHz = (LwU16)result.freqMHz;

    if (freqMHz == 0U)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto clkVfPointBaseVFCache_40_VOLT_exit;
    }

    // VF lwrve smoothening code
    if  ((pVfPoint40Last != NULL) &&
         (BOARDOBJ_GET_TYPE(pVfRel) ==
            LW2080_CTRL_CLK_CLK_VF_REL_TYPE_RATIO_VOLT))
    {
        CLK_VF_REL_RATIO_VOLT *pVfRelRatioVolt = (CLK_VF_REL_RATIO_VOLT *)pVfRel;

        if (clkVfRelRatioVoltIsVFSmoothingRequired(pVfRelRatioVolt,
                                                   pBaseVFTuple->voltageuV))
        {
            LwU16                                       expectedMaxFreqMHz;
            LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTupleLast =
                clkVfPointsBaseVfCacheBaseVFTupleGet(lwrveIdx, cacheIdx,
                    BOARDOBJ_GET_GRP_IDX(&pVfPoint40Last->super.super));

            // Sanity check.
            if (pBaseVFTupleLast == NULL)
            {
                status = FLCN_ERR_ILWALID_ARGUMENT;
                PMU_BREAKPOINT();
                goto clkVfPointBaseVFCache_40_VOLT_exit;
            }

            expectedMaxFreqMHz =
                pBaseVFTupleLast->freqTuple[CLK_VF_PRIMARY_POS].freqMHz +
                clkVfRelRatioVoltMaxFreqStepSizeMHzGet(pVfRelRatioVolt,
                                                       pBaseVFTuple->voltageuV);

            freqMHz = LW_MIN(freqMHz, expectedMaxFreqMHz);
        }
    }

    // Trim VF lwrve to VF max and frequency enumeration max.
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_TRIM_VF_LWRVE_BY_ENUM_MAX))
    {
        freqMHz = LW_MIN(clkVfRelFreqMaxMHzGet(pVfRel), freqMHz);
        freqMHz = LW_MIN(clkDomain40ProgEnumFreqMaxMHzGet(pDomain40Prog), freqMHz);
    }

    // Init
    LW2080_CTRL_CLK_VF_INPUT_INIT(&inputFreq);
    LW2080_CTRL_CLK_VF_OUTPUT_INIT(&outputFreq);

    inputFreq.value = freqMHz;
    status = clkDomainProgFreqQuantize_40(
                CLK_DOMAIN_PROG_GET_FROM_40_PROG(pDomain40Prog),    // pDomainProg
                &inputFreq,                                         // pInputFreq
                &outputFreq,                                        // pOutputFreq
                LW_TRUE,                                            // bFloor
                LW_FALSE);                                          // bBound
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPointBaseVFCache_40_VOLT_exit;
    }
    pBaseVFTuple->freqTuple[CLK_VF_PRIMARY_POS].freqMHz  = (LwU16) outputFreq.value;

clkVfPointBaseVFCache_40_VOLT_exit:
    return status;
}

/*!
 * @copydoc ClkVfPoint40VoltSmoothing
 */
FLCN_STATUS
clkVfPoint40VoltSmoothing
(
    CLK_VF_POINT_40_VOLT       *pVfPoint40Volt,
    CLK_VF_POINT_40_VOLT       *pVfPoint40VoltLast,
    CLK_DOMAIN_40_PROG         *pDomain40Prog,
    CLK_VF_REL_RATIO_VOLT      *pVfRelRatioVolt,
    LwU8                        lwrveIdx,
    LwU8                        cacheIdx
)
{
    LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTuple;
    LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE      *pOffsetedVFTuple;
    LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE      *pOffsetedVFTupleLast;
    FLCN_STATUS                                 status  = FLCN_OK;

    // Sanity check.
    if (pVfPoint40VoltLast == NULL)
    {
        status  = FLCN_OK;
        goto clkVfPoint40VoltSmoothing_exit;
    }

    if (cacheIdx == CLK_CLK_VF_POINT_VF_CACHE_IDX_ILWALID)
    {
        pBaseVFTuple         =
            clkVfPoint40BaseVFTupleGet(&pVfPoint40Volt->super);
        pOffsetedVFTuple     =
            clkVfPoint40OffsetedVFTupleGet(&pVfPoint40Volt->super);
        pOffsetedVFTupleLast =
            clkVfPoint40OffsetedVFTupleGet(&pVfPoint40VoltLast->super);
    }
    else
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkVfPoint40VoltSmoothing_exit;

        // PP-TOOD : Preparation for follow up cl.

        // pBaseVFTuple         =
        //     clkVfPointsBaseVfCacheBaseVFTupleGet(lwrveIdx, cacheIdx,
        //         BOARDOBJ_GET_GRP_IDX(&pVfPoint40Volt->super.super.super));
        // pOffsetedVFTuple     =
        //     clkVfPointsVfCacheOffsetedVFTupleGet(lwrveIdx, cacheIdx,
        //         BOARDOBJ_GET_GRP_IDX(&pVfPoint40Volt->super.super.super));
        // pOffsetedVFTupleLast =
        //     clkVfPointsVfCacheOffsetedVFTupleGet(lwrveIdx, cacheIdx,
        //         BOARDOBJ_GET_GRP_IDX(&pVfPoint40VoltLast->super.super.super));
    }

    // Call the helper interface.
    status = s_clkVfPoint40VoltSmoothing_HELPER(pVfPoint40Volt,
                                                pDomain40Prog,
                                                pVfRelRatioVolt,
                                                pBaseVFTuple,
                                                pOffsetedVFTuple,
                                                pOffsetedVFTupleLast);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPoint40VoltSmoothing_exit;
    }

clkVfPoint40VoltSmoothing_exit:
    return status;
}

/*!
 * @copydoc ClkVfPoint40VoltSmoothing
 */
static FLCN_STATUS
s_clkVfPoint40VoltSmoothing_HELPER
(
    CLK_VF_POINT_40_VOLT                       *pVfPoint40Volt,
    CLK_DOMAIN_40_PROG                         *pDomain40Prog,
    CLK_VF_REL_RATIO_VOLT                      *pVfRelRatioVolt,
    LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTuple,
    LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE      *pOffsetedVFTuple,
    LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE      *pOffsetedVFTupleLast
)
{
    LwU16                       freqMHz;
    LW2080_CTRL_CLK_VF_INPUT    inputFreq;
    LW2080_CTRL_CLK_VF_OUTPUT   outputFreq;
    FLCN_STATUS                 status  = FLCN_OK;

    // Sanity check.
    if ((pBaseVFTuple         == NULL) ||
        (pOffsetedVFTuple     == NULL) ||
        (pOffsetedVFTupleLast == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto s_clkVfPoint40VoltSmoothing_HELPER_exit;
    }

    if (!clkVfRelRatioVoltIsVFSmoothingRequired(pVfRelRatioVolt,
                                               pBaseVFTuple->voltageuV))
    {
        goto s_clkVfPoint40VoltSmoothing_HELPER_exit;
    }

    //
    // Get the step size.
    // Use the same step size for primary and all its secondaries.
    //
    freqMHz = clkVfRelRatioVoltMaxFreqStepSizeMHzGet(pVfRelRatioVolt,
                                                     pBaseVFTuple->voltageuV);

    // Callwlate the expected frequency based on last evaluated VF point.
    if (pOffsetedVFTupleLast[CLK_VF_PRIMARY_POS].freqMHz > freqMHz)
    {
        freqMHz = pOffsetedVFTupleLast[CLK_VF_PRIMARY_POS].freqMHz - freqMHz;
    }
    else
    {
        freqMHz = 0U;
    }

    // Compare the current vs expected frequency
    if (freqMHz > pOffsetedVFTuple[CLK_VF_PRIMARY_POS].freqMHz)
    {
        //
        // Quantize up the frequency to ensure that quantized frequency
        // is within the allowed bound
        //

        // Init
        LW2080_CTRL_CLK_VF_INPUT_INIT(&inputFreq);
        LW2080_CTRL_CLK_VF_OUTPUT_INIT(&outputFreq);

        inputFreq.value = freqMHz;
        status = clkDomainProgFreqQuantize_40(
                    CLK_DOMAIN_PROG_GET_FROM_40_PROG(pDomain40Prog),    // pDomainProg
                    &inputFreq,                                         // pInputFreq
                    &outputFreq,                                        // pOutputFreq
                    LW_FALSE,                                           // bFloor
                    LW_FALSE);                                          // bBound
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_clkVfPoint40VoltSmoothing_HELPER_exit;
        }
        pOffsetedVFTuple[CLK_VF_PRIMARY_POS].freqMHz = (LwU16) outputFreq.value;
    }

s_clkVfPoint40VoltSmoothing_HELPER_exit:
    return status;
}

/*!
 * @copydoc ClkVfPoint40Cache
 */
FLCN_STATUS
clkVfPoint40SecondaryCache_VOLT
(
    CLK_VF_POINT_40            *pVfPoint40,
    CLK_VF_POINT_40            *pVfPoint40Last,
    CLK_DOMAIN_40_PROG         *pDomain40Prog,
    CLK_VF_REL                 *pVfRel,
    LwU8                        lwrveIdx,
    LwBool                      bVFEEvalRequired
)
{
    FLCN_STATUS   status = FLCN_OK;

    // Base frequency tuple remains unchanged when VFE equations is skipped.
    if (bVFEEvalRequired)
    {
        LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTuple     =
            clkVfPoint40BaseVFTupleGet(pVfPoint40);
        CLK_DOMAIN   *pDomain;
        LwBoardObjIdx clkIdx;

        // Sanity check.
        if (pBaseVFTuple == NULL)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto clkVfPoint40SecondaryCache_VOLT_exit;
        }

        // Update the secondary clock domains base VF lwrve.
        BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, clkIdx,
            &(clkDomain40ProgGetSecondaryDomainsMask(pDomain40Prog, pVfRel->railIdx)->super))
        {
            CLK_DOMAIN_40_PROG *pDomain40ProgSecondary = (CLK_DOMAIN_40_PROG *)pDomain;

            LwU8 clkPos = clkDomain40ProgGetClkDomainPosByIdx(pDomain40ProgSecondary,
                                                         pVfRel->railIdx);
            if (clkPos == LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID)
            {
                // Primary lwrve must be supported on all secondary clk domains.
                if (lwrveIdx == LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI)
                {
                    status = FLCN_ERR_ILWALID_INDEX;
                    PMU_BREAKPOINT();
                    goto clkVfPoint40SecondaryCache_VOLT_exit;
                }
                continue;
            }

            // Init to primary frequency.
            pBaseVFTuple->freqTuple[clkPos].freqMHz =
                pBaseVFTuple->freqTuple[CLK_VF_PRIMARY_POS].freqMHz;

            // Perform primary -> secondary translation.
            status = clkVfRelFreqTranslatePrimaryToSecondary(pVfRel,     // pVfRel
                        BOARDOBJ_GRP_IDX_TO_8BIT(clkIdx),           // secondaryIdx
                        &pBaseVFTuple->freqTuple[clkPos].freqMHz,   // pFreqMHz
                        LW_TRUE);                                   // bQuantize
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkVfPoint40SecondaryCache_VOLT_exit;
            }
        }
        BOARDOBJGRP_ITERATOR_END;
    }

clkVfPoint40SecondaryCache_VOLT_exit:
    return status;
}


/*!
 * @copydoc ClkVfPoint40BaseVFCache
 */
FLCN_STATUS
clkVfPoint40SecondaryBaseVFCache_VOLT
(
    CLK_VF_POINT_40            *pVfPoint40,
    CLK_VF_POINT_40            *pVfPoint40Last,
    CLK_DOMAIN_40_PROG         *pDomain40Prog,
    CLK_VF_REL                 *pVfRel,
    LwU8                        lwrveIdx,
    LwU8                        cacheIdx
)
{
    LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTuple     =
        clkVfPointsBaseVfCacheBaseVFTupleGet(lwrveIdx, cacheIdx,
            BOARDOBJ_GET_GRP_IDX(&pVfPoint40->super.super));
    CLK_DOMAIN   *pDomain;
    LwBoardObjIdx clkIdx;
    FLCN_STATUS   status = FLCN_OK;

    // Sanity check.
    if (pBaseVFTuple == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkVfPoint40SecondaryBaseVFCache_VOLT_exit;
    }

    // Update the secondary clock domains base VF lwrve.
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, clkIdx,
        &(clkDomain40ProgGetSecondaryDomainsMask(pDomain40Prog, pVfRel->railIdx)->super))
    {
        CLK_DOMAIN_40_PROG *pDomain40ProgSecondary = (CLK_DOMAIN_40_PROG *)pDomain;

        LwU8 clkPos = clkDomain40ProgGetClkDomainPosByIdx(pDomain40ProgSecondary,
                                                     pVfRel->railIdx);
        if (clkPos == LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID)
        {
            // Primary lwrve must be supported on all secondary clk domains.
            if (lwrveIdx == LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI)
            {
                status = FLCN_ERR_ILWALID_INDEX;
                PMU_BREAKPOINT();
                goto clkVfPoint40SecondaryBaseVFCache_VOLT_exit;
            }
            continue;
        }

        // Init to primary frequency.
        pBaseVFTuple->freqTuple[clkPos].freqMHz =
            pBaseVFTuple->freqTuple[CLK_VF_PRIMARY_POS].freqMHz;

        // Perform primary -> secondary translation.
        status = clkVfRelFreqTranslatePrimaryToSecondary(pVfRel,     // pVfRel
                    BOARDOBJ_GRP_IDX_TO_8BIT(clkIdx),           // secondaryIdx
                    &pBaseVFTuple->freqTuple[clkPos].freqMHz,   // pFreqMHz
                    LW_TRUE);                                   // bQuantize
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfPoint40SecondaryBaseVFCache_VOLT_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

clkVfPoint40SecondaryBaseVFCache_VOLT_exit:
    return status;
}

/*!
 * @copydoc ClkVfPointClientCpmMaxOffsetOverrideSet
 */
FLCN_STATUS
clkVfPointClientCpmMaxOffsetOverrideSet_40_VOLT
(
    CLK_VF_POINT    *pVfPoint,
    LwU16            cpmMaxFreqOffsetOverrideMHz
)
{
    CLK_VF_POINT_40_VOLT *pVfPoint40Volt = (CLK_VF_POINT_40_VOLT *)pVfPoint;

    pVfPoint40Volt->clientCpmMaxFreqOffsetOverrideMHz = cpmMaxFreqOffsetOverrideMHz;

    return FLCN_OK;
}

/*!
 * @copydoc ClkVfPointVoltageuVGet
 */
FLCN_STATUS
clkVfPointVoltageuVGet_40_VOLT
(
    CLK_VF_POINT  *pVfPoint,
    LwU8           voltageType,
    LwU32         *pVoltageuV
)
{
    CLK_VF_POINT_40_VOLT *pVfPoint40Volt = (CLK_VF_POINT_40_VOLT *)pVfPoint;
    FLCN_STATUS           status         = FLCN_OK;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVfPoint40Volt, &pVfPoint->super, CLK, CLK_VF_POINT, 40_VOLT,
                                  &status, clkVfPointVoltageuVGet_40_VOLT_exit);

    // Call super class implementation.
    status = clkVfPointVoltageuVGet_40(pVfPoint, voltageType, pVoltageuV);
    if (status == FLCN_OK)
    {
        return status;
    }

    if (LW2080_CTRL_CLK_VOLTAGE_TYPE_SOURCE == voltageType)
    {
        // Return the source voltage if requested.
        *pVoltageuV = pVfPoint40Volt->sourceVoltageuV;
        status = FLCN_OK;
    }
    else
    {
        // All other voltage types are un-supported.  Return error.
        PMU_BREAKPOINT();
        status = FLCN_ERR_NOT_SUPPORTED;
    }

clkVfPointVoltageuVGet_40_VOLT_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
