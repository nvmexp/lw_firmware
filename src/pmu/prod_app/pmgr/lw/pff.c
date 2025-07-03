/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pff.c
 * @brief  Library of functions used for evaluating the output frequency
 *         of a piecewise linear frequency floor lwrve given an input observed
 *         value.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmgr/pff.h"
#include "pmu_objperf.h"
#include "pmu_objpmgr.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static FLCN_STATUS  s_pffGetMaxSupportedFrequencykHz(LwU32 *pFreqMaxkHz, LwBool bGetOrigFreq)
    GCC_ATTRIB_SECTION("imem_pmgrLibPff", "s_pffGetMaxSupportedFrequencykHz");
static LwU32        s_pffLinearInterpolate(LwU32 x0, LwU32 y0, LwU32 x1, LwU32 y1, LwU32 x)
    GCC_ATTRIB_SECTION("imem_pmgrLibPff", "s_pffLinearInterpolate");

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc PffEvaluate
 */
FLCN_STATUS
pffEvaluate
(
    PIECEWISE_FREQUENCY_FLOOR  *pPff,
    LwU32                       domainInput,
    LwU32                       domainMin
)
{
    LwU8        tupleIndex;
    LwU32      *pFreqkHz;
    LwU32       maxFreqkHz  = 0;
    FLCN_STATUS status      = FLCN_OK;

    // Bail: Feauture not supported on this build
    if (pPff == NULL)
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        PMU_BREAKPOINT();
        goto pffEvaluate_exit;
    }

    // Map local variables
    pFreqkHz = &pPff->status.lastFloorkHz;

    // Bail: Feature not enabled
    if (!(pPff->control.bFlooringEnabled))
    {
        *pFreqkHz = 0;
        goto pffEvaluate_exit;
    }

    // Get upper bound on the range of the pff lwrve
    status = s_pffGetMaxSupportedFrequencykHz(&maxFreqkHz, LW_FALSE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pffEvaluate_exit;
    }

    // We are not in the "active" region for flooring, set floor to max and exit
    if (domainInput < domainMin)
    {
        *pFreqkHz = maxFreqkHz;
        goto pffEvaluate_exit;
    }

    // Determine intersection of domainInput and the pff lwrve
    for (tupleIndex = 0; tupleIndex < LW2080_CTRL_PMGR_PFF_TUPLES_MAX; tupleIndex++)
    {
        if (domainInput < pPff->status.lwrve.tuples[tupleIndex].domain.value)
        {
            LwU32 prevValue = (tupleIndex > 0) ?
                                pPff->status.lwrve.tuples[tupleIndex - 1].domain.value :
                                domainMin;
            LwU32 prevFreq  = (tupleIndex > 0) ?
                                LW_MIN(pPff->status.lwrve.tuples[tupleIndex - 1].freqkHz, maxFreqkHz) :
                                maxFreqkHz;
            LwU32 lwrrValue = pPff->status.lwrve.tuples[tupleIndex].domain.value;
            LwU32 lwrrFreq  = LW_MIN(pPff->status.lwrve.tuples[tupleIndex].freqkHz, maxFreqkHz);

            *pFreqkHz = s_pffLinearInterpolate(prevValue,   // x0
                                              prevFreq,     // y0
                                              lwrrValue,    // x1
                                              lwrrFreq,     // y1
                                              domainInput); // x
            goto pffEvaluate_exit;
        }
    }

    // Here, domainInput > tuple[LW2080_CTRL_PMGR_FREQ_FLOOR_TUPLES_MAX - 1].domain.value => freqkHz = flat;
    *pFreqkHz = pPff->status.lwrve.tuples[LW2080_CTRL_PMGR_PFF_TUPLES_MAX - 1].freqkHz;

pffEvaluate_exit:
    return status;
}

/*!
 * @copydoc PffSanityCheck
 */
FLCN_STATUS
pffSanityCheck
(
    PIECEWISE_FREQUENCY_FLOOR  *pPff,
    LwU32                       domainMin
)
{
    LwU8        tupleIndex;
    FLCN_STATUS status      = FLCN_OK;

    // If NULL, feature is not enabled for this profile
    if (pPff == NULL)
    {
        goto pffSanityCheck_exit;
    }

    // If the functionality of the pff is not enabled, exit
    if (!(pPff->control.bFlooringEnabled))
    {
        goto pffSanityCheck_exit;
    }

    // Check that the first tuple is to the right of the domainMin
    if (domainMin > pPff->control.lwrve.tuples[0].domain.value)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto pffSanityCheck_exit;
    }

    // Check that the pff lwrve is monitonically decreasing starting from the first tuple
    for (tupleIndex = 1; tupleIndex < LW2080_CTRL_PMGR_PFF_TUPLES_MAX; tupleIndex++)
    {
        LwU32 prevValue = pPff->control.lwrve.tuples[tupleIndex - 1].domain.value;
        LwU32 prevFreq  = pPff->control.lwrve.tuples[tupleIndex - 1].freqkHz;

        // X-Value of tuples should be increasing or constant
        if (prevValue > pPff->control.lwrve.tuples[tupleIndex].domain.value)
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto pffSanityCheck_exit;
        }

        // Y-Value of tuples should be decreasing or constant
        if (prevFreq < pPff->control.lwrve.tuples[tupleIndex].freqkHz)
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto pffSanityCheck_exit;
        }
    }

pffSanityCheck_exit:
    return status;
}

/*!
 * @copydoc PffIlwalidate
 */
FLCN_STATUS
pffIlwalidate
(
    PIECEWISE_FREQUENCY_FLOOR  *pPff,
    LwU32                       domainInput,
    LwU32                       domainMin
)
{
    LwU8                tupleIdx;
    CLK_DOMAIN         *pDomain;
    CLK_DOMAIN_PROG    *pDomainProg;
    FLCN_STATUS         status = FLCN_OK;

    // Set the status to some initial defaults
    pPff->status.lastFloorkHz = 0;
    for (tupleIdx = 0; tupleIdx < LW2080_CTRL_PMGR_PFF_TUPLES_MAX; tupleIdx++)
    {
        pPff->status.lwrve.tuples[tupleIdx] = pPff->control.lwrve.tuples[tupleIdx];
    }

    // Grab the clk domain for GPC
    pDomain = clkDomainsGetByDomain(clkWhich_GpcClk);
    if (pDomain == NULL)
    {
        // Try to get the clk domain for GPC2 if GPC is not found
        pDomain = clkDomainsGetByDomain(clkWhich_Gpc2Clk);
        if (pDomain == NULL)
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto pffIlwalidate_exit;
        }
    }

    pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
    if (pDomainProg == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto pffIlwalidate_exit;
    }

    // Begin process of adding OC to each individual tuple
    for (tupleIdx = 0; tupleIdx < LW2080_CTRL_PMGR_PFF_TUPLES_MAX; tupleIdx++)
    {
        LwU16                       postAdjustMHz;
        LwU32                       maxFreqkHz;
        LW2080_CTRL_CLK_VF_INPUT    inputFreq;
        LW2080_CTRL_CLK_VF_OUTPUT   outputFreq;

        // 1) Quantize down tuple
        LW2080_CTRL_CLK_VF_INPUT_INIT(&inputFreq);
        LW2080_CTRL_CLK_VF_OUTPUT_INIT(&outputFreq);
        inputFreq.value = pPff->status.lwrve.tuples[tupleIdx].freqkHz / clkDomainFreqkHzScaleGet(pDomain);
        status = clkDomainProgFreqQuantize(
                    pDomainProg,                    // pDomainProg
                    &inputFreq,                     // pInputFreq
                    &outputFreq,                    // pOutputFreq
                    LW_TRUE,                        // bFloor
                    LW_TRUE);                       // bBound
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto pffIlwalidate_exit;
        }

        postAdjustMHz = (LwU16) outputFreq.value;

        // 2) Look up offseted frequency (for all points but the last one)
        if ((tupleIdx != (LW2080_CTRL_PMGR_PFF_TUPLES_MAX - 1)) &&
            (postAdjustMHz != 0U))
        {
            status = clkDomainProgClientFreqDeltaAdj(
                        pDomainProg,                    // pDomainProg
                        &postAdjustMHz);                // pFreqMHz
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto pffIlwalidate_exit;
            }
        }

        // 3) Override the status tuple to reflect OC & quantization
        pPff->status.lwrve.tuples[tupleIdx].freqkHz =
            (LwU32) postAdjustMHz * clkDomainFreqkHzScaleGet(pDomain);

        // 4) Trim the status tuple by the max supported frequency
        status = s_pffGetMaxSupportedFrequencykHz(&maxFreqkHz, LW_FALSE);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto pffIlwalidate_exit;
        }
        pPff->status.lwrve.tuples[tupleIdx].freqkHz =
            LW_MIN(pPff->status.lwrve.tuples[tupleIdx].freqkHz, maxFreqkHz);
    }

    // Re-evaluate the floor that was reset earlier
    status = pffEvaluate(pPff, domainInput, domainMin);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pffIlwalidate_exit;
    }

pffIlwalidate_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
static FLCN_STATUS
s_pffGetMaxSupportedFrequencykHz
(
    LwU32  *pFreqMaxkHz,
    LwBool  bGetOrigFreq
)
{
    LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY pstateClkEntry;
    PSTATE                             *pPstate;
    FLCN_STATUS                         status = FLCN_OK;

    // Sanity check
    if (pFreqMaxkHz == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto s_pffGetMaxSupportedFrequencykHz_exit;
    }

    // Get the highest performance pstate
    pPstate = PSTATE_GET_BY_HIGHEST_INDEX();
    if (pPstate == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto s_pffGetMaxSupportedFrequencykHz_exit;
    }

    // Get clks for that pstate
    status = perfPstateClkFreqGet(pPstate,
        clkDomainsGetIdxByDomainGrpIdx(RM_PMU_DOMAIN_GROUP_GPC2CLK),
        &pstateClkEntry);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_pffGetMaxSupportedFrequencykHz_exit;
    }

    // Needed for early stage sanity check
    if (bGetOrigFreq)
    {
        *pFreqMaxkHz = pstateClkEntry.max.origFreqkHz;
    }
    else
    {
        *pFreqMaxkHz = pstateClkEntry.max.freqVFMaxkHz;
    }

    if (*pFreqMaxkHz == 0)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto s_pffGetMaxSupportedFrequencykHz_exit;
    }

s_pffGetMaxSupportedFrequencykHz_exit:
    return status;
}

/*!
 * y0----+--\
 *      |   ------\
 *      |          ------\
 *      x0                ------\
 *                               ------\
 *                                 y----+-----\
 *                                      |      ------\
 *                                      |             ------\
 *                                      |                    ------\
 *                                      x                           --+----y1
 *                                                                    |
 *                                                                    |
 *                                                                    |
 *                                                                    x1
 * Linear interpolation between two points and evaluated somewhere in between.
 * Output (y) is the lwrve intersection at value x
 *
 * @param[in] x0 x-value of left point
 * @param[in] y0 y-value of left point
 * @param[in] x1 x-value of right point (must be larger than x0)
 * @param[in] y1 y-value of right point (must be smaller than y0)
 * @param[in]  x x-value intersecting line, y-value of interection is returned
 *
 * @return y-value of linear line segment evaluated at x
 */
static LwU32
s_pffLinearInterpolate
(
    LwU32 x0,
    LwU32 y0,
    LwU32 x1,
    LwU32 y1,
    LwU32 x
)
{
    if (x1 == x)
    {
        return y1;
    }
    else if (x0 == x)
    {
        return y0;
    }
    else if (y1 == y0)
    {
        return y0;
    }

    LwU32 slope = LW_UNSIGNED_ROUNDED_DIV((y0 - y1), (x1 - x0));

    LwU32 offsetFromY1 = (x1 - x) * slope;

    return (y1 + offsetFromY1);
}

