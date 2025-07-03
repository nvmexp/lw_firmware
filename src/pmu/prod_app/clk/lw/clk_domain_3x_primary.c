/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * @copydoc ClkDomain3XPrimaryVoltToFreqBinarySearch
 */
FLCN_STATUS
clkDomain3XPrimaryVoltToFreqBinarySearch
(
    CLK_DOMAIN_3X_PRIMARY               *pDomain3XPrimary,
    LwU8                                clkDomIdx,
    LwU8                                voltRailIdx,
    LwU8                                voltageType,
    LW2080_CTRL_CLK_VF_INPUT           *pInput,
    LW2080_CTRL_CLK_VF_OUTPUT          *pOutput
)
{
    CLK_DOMAIN_3X_PROG     *pDomain3XProg =
        (CLK_DOMAIN_3X_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomain3XPrimary);
    CLK_PROG_3X_PRIMARY     *pProg3XPrimary;
    FLCN_STATUS             status = FLCN_OK;
    LwU8                    i;

    // Sanity check input value.
    if (pInput->value == LW2080_CTRL_CLK_VF_VALUE_ILWALID)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkDomain3XPrimaryVoltToFreqBinarySearch_exit;
    }

    // Init the output to default/unmatched values.
    LW2080_CTRL_CLK_VF_OUTPUT_INIT(pOutput);

    // Iterate through the set of CLK_PROG entries to find best matches.
    for (i = pDomain3XProg->clkProgIdxFirst;
            i <= pDomain3XProg->clkProgIdxLast; i++)
    {
        CLK_PROG *pProg3X = BOARDOBJGRP_OBJ_GET(CLK_PROG, i);

        if (pProg3X == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkDomain3XPrimaryVoltToFreqBinarySearch_exit;
        }

        pProg3XPrimary = CLK_PROG_BOARDOBJ_TO_INTERFACE_CAST(pProg3X, 3X_PRIMARY);
        if (pProg3XPrimary == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkDomain3XPrimaryVoltToFreqBinarySearch_exit;
        }

        status = clkProg3XPrimaryVoltToFreqBinarySearch(
                    pProg3XPrimary,   // pProg3XPrimary
                    pDomain3XPrimary, // pDomain3XPrimary
                    clkDomIdx,       // clkDomIdx
                    voltRailIdx,     // voltRailIdx
                    voltageType,     // voltageType
                    pInput,          // pInput
                    pOutput);        // pOutput
        //
        // If the CLK_PROG_3X_PRIMARY encountered VF points above
        // pInput->value, the search is done.  Can short-circuit the
        // rest of the search and return FLCN_OK.
        //
        if (FLCN_ERR_ITERATION_END == status)
        {
            status = FLCN_OK;
            goto clkDomain3XPrimaryVoltToFreqBinarySearch_exit;
        }
        else if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomain3XPrimaryVoltToFreqBinarySearch_exit;
        }
    }

clkDomain3XPrimaryVoltToFreqBinarySearch_exit:
    return status;
}

/*!
 * @copydoc ClkDomain3XPrimaryFreqToVoltBinarySearch
 */
FLCN_STATUS
clkDomain3XPrimaryFreqToVoltBinarySearch
(
    CLK_DOMAIN_3X_PRIMARY               *pDomain3XPrimary,
    LwU8                                clkDomIdx,
    LwU8                                voltRailIdx,
    LwU8                                voltageType,
    LW2080_CTRL_CLK_VF_INPUT           *pInput,
    LW2080_CTRL_CLK_VF_OUTPUT          *pOutput
)
{
    CLK_DOMAIN_3X_PROG     *pDomain3XProg =
        (CLK_DOMAIN_3X_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomain3XPrimary);
    CLK_PROG_3X_PRIMARY     *pProg3XPrimary;
    FLCN_STATUS             status = FLCN_OK;
    LwU8                    i;

    // Sanity check input value.
    if (pInput->value == LW2080_CTRL_CLK_VF_VALUE_ILWALID)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkDomain3XPrimaryFreqToVoltBinarySearch_exit;
    }

    // Init the output to default/unmatched values.
    LW2080_CTRL_CLK_VF_OUTPUT_INIT(pOutput);

    // Iterate through the set of CLK_PROG entries to find best matches.
    for (i = pDomain3XProg->clkProgIdxFirst;
            i <= pDomain3XProg->clkProgIdxLast; i++)
    {
        CLK_PROG *pProg3X = BOARDOBJGRP_OBJ_GET(CLK_PROG, i);

        if (pProg3X == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkDomain3XPrimaryFreqToVoltBinarySearch_exit;
        }

        pProg3XPrimary = CLK_PROG_BOARDOBJ_TO_INTERFACE_CAST(pProg3X, 3X_PRIMARY);
        if (pProg3XPrimary == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkDomain3XPrimaryFreqToVoltBinarySearch_exit;
        }

        status = clkProg3XPrimaryFreqToVoltBinarySearch(
                    pProg3XPrimary,   // pProg3XPrimary
                    pDomain3XPrimary, // pDomain3XPrimary
                    clkDomIdx,       // clkDomIdx
                    voltRailIdx,     // voltRailIdx
                    voltageType,     // voltageType
                    pInput,          // pInput
                    pOutput);        // pOutput
        //
        // If the CLK_PROG_3X_PRIMARY encountered VF points above
        // pInput->value, the search is done.  Can short-circuit the
        // rest of the search and return FLCN_OK.
        //
        if (FLCN_ERR_ITERATION_END == status)
        {
            status = FLCN_OK;
            goto clkDomain3XPrimaryFreqToVoltBinarySearch_exit;
        }
        else if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomain3XPrimaryFreqToVoltBinarySearch_exit;
        }
    }

clkDomain3XPrimaryFreqToVoltBinarySearch_exit:
    return status;
}

/*!
 * @copydoc ClkDomain3XProgFreqAdjustDeltaMHz
 */
FLCN_STATUS
clkDomain3XProgFreqAdjustDeltaMHz_PRIMARY
(
    CLK_DOMAIN_3X_PROG  *pDomain3XProg,
    LwU16               *pFreqMHz,
    LwBool               bQuantize,
    LwBool               bVFOCAdjReq
)
{
    CLK_DOMAIN_3X_PRIMARY *pDomain3XPrimary;
    CLK_PROG_3X_PRIMARY   *pProg3XPrimary;
    LwU8                  progIdx;
    FLCN_STATUS           status          = FLCN_OK;
    CLK_PROG             *pProg3X;

    pDomain3XPrimary = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain3XProg, 3X_PRIMARY);
    if (pDomain3XPrimary == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto clkDomain3XProgFreqAdjustDeltaMHz_PRIMARY_exit;
    }

    progIdx = clkDomain3XProgGetClkProgIdxFromFreq(pDomain3XProg,
                *pFreqMHz, LW_FALSE);

    pProg3X = BOARDOBJGRP_OBJ_GET(CLK_PROG, progIdx);
    if (pProg3X == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto clkDomain3XProgFreqAdjustDeltaMHz_PRIMARY_exit;
    }

    pProg3XPrimary = CLK_PROG_BOARDOBJ_TO_INTERFACE_CAST(pProg3X, 3X_PRIMARY);
    if (pProg3XPrimary == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto clkDomain3XProgFreqAdjustDeltaMHz_PRIMARY_exit;
    }

    status = clkDomain3XPrimaryFreqAdjustDeltaMHz(pDomain3XPrimary,
                pProg3XPrimary, bQuantize, bVFOCAdjReq, pFreqMHz);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomain3XProgFreqAdjustDeltaMHz_PRIMARY_exit;
    }

clkDomain3XProgFreqAdjustDeltaMHz_PRIMARY_exit:
    return status;
}

/*!
 * @copydoc ClkDomain3XPrimaryFreqAdjustDeltaMHz
 */
FLCN_STATUS
clkDomain3XPrimaryFreqAdjustDeltaMHz
(
    CLK_DOMAIN_3X_PRIMARY  *pDomain3XPrimary,
    CLK_PROG_3X_PRIMARY    *pProg3XPrimary,
    LwBool                 bQuantize,
    LwBool                 bVFOCAdjReq,
    LwU16                 *pFreqMHz
)
{
    CLK_DOMAIN_3X_PROG *pDomain3XProg =
        (CLK_DOMAIN_3X_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomain3XPrimary);
    LwU16       freqOrigMHz = *pFreqMHz;
    FLCN_STATUS status      = FLCN_OK;

    // If OVOC is not enabled for given pProg3XPrimary, short circuit for early return.
    if (!clkProg3XPrimaryOVOCEnabled(pProg3XPrimary))
    {
        goto clkDomain3XPrimaryFreqAdjustDeltaMHz_exit;
    }

    // This should always be the first as the function accept non-offseted input.
    if (bVFOCAdjReq &&
        clkDomain3XProgIsOVOCEnabled(pDomain3XProg))
    {
        // Update the max pstate freq with max valid VF point freq delta.
        status = clkProg3XPrimaryMaxVFPointFreqDeltaAdj(pProg3XPrimary,
                    pDomain3XPrimary, pFreqMHz);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomain3XPrimaryFreqAdjustDeltaMHz_exit;
        }
    }

    // Offset by the CLK_DOMAIN_3X_PROG domain offset.
    status = clkDomain3XProgFreqAdjustDeltaMHz_SUPER(pDomain3XProg,
                pFreqMHz, bQuantize, bVFOCAdjReq);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomain3XPrimaryFreqAdjustDeltaMHz_exit;
    }

    // If OVOC is not enabled, skip the clock domain delta adjustment
    if (clkDomain3XProgIsOVOCEnabled(pDomain3XProg))
    {
        // Offset by the global CLK_DOMAINS offset.
        *pFreqMHz = clkFreqDeltaAdjust(*pFreqMHz,
                        clkDomainsFreqDeltaGet());

        // Offset by the CLK_PROG_3X offset
        *pFreqMHz = clkFreqDeltaAdjust(*pFreqMHz,
                        clkProg3XPrimaryFreqDeltaGet(pProg3XPrimary));
    }

    //
    // If quantization requested and the value has been offset, quantize the
    // result to the frequencies supported by the CLK_DOMAIN.
    //
    if (bQuantize &&
        (freqOrigMHz != *pFreqMHz))
    {
        status = clkDomain3XProgFreqAdjustQuantizeMHz(pDomain3XProg,
                    pFreqMHz,   // pFreqMHz
                    LW_TRUE,    // bFloor
                    LW_TRUE);   // bReqFreqDeltaAdj
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomain3XPrimaryFreqAdjustDeltaMHz_exit;
        }
    }

clkDomain3XPrimaryFreqAdjustDeltaMHz_exit:
    return status;
}

/*!
 * _3X_PRIMARY implementation.
 *
 * @copydoc ClkDomain3XProgVoltAdjustDeltauV
 */
FLCN_STATUS
clkDomain3XProgVoltAdjustDeltauV_PRIMARY
(
    CLK_DOMAIN_3X_PROG  *pDomain3XProg,
    CLK_PROG_3X_PRIMARY  *pProg3XPrimary,
    LwU8                 voltRailIdx,
    LwU32               *pVoltuV
)
{
    // Apply the global voltage offset.
    *pVoltuV = clkVoltDeltaAdjust(*pVoltuV,
                Clk.domains.deltas.pVoltDeltauV[voltRailIdx]);

    // Apply this CLK_DOMAIN_3X_PROG object's voltage offset.
    *pVoltuV = clkVoltDeltaAdjust(*pVoltuV,
                clkDomain3XProgVoltDeltauVGet(pDomain3XProg, voltRailIdx));

    // Apply the CLK_PROG_3X_PRIMARY object's voltage offset.
    *pVoltuV = clkVoltDeltaAdjust(*pVoltuV,
                clkProg3XPrimaryVoltDeltauVGet(pProg3XPrimary, voltRailIdx));

    return FLCN_OK;
}

/*!
 * _3X_PRIMARY implementation
 *
 * @copydoc ClkDomainProgMaxFreqMHzGet
 */
FLCN_STATUS
clkDomainProgMaxFreqMHzGet_3X_PRIMARY
(
    CLK_DOMAIN_PROG        *pDomainProg,
    LwU32                  *pFreqMaxMHz
)
{
    CLK_DOMAIN_3X_PROG     *pDomain3XProg;
    CLK_DOMAIN_3X_PRIMARY   *pDomain3XPrimary;
    FLCN_STATUS             status;

    pDomain3XProg = (CLK_DOMAIN_3X_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);

    pDomain3XPrimary = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain3XProg, 3X_PRIMARY);
    if (pDomain3XPrimary == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto clkDomainProgMaxFreqMHzGet_3X_PRIMARY_exit;
    }

    //
    // Call into PRIMARY interface for this PRIMARY domain with secondary domain index
    // = _ILWALID.
    //
    status = clkDomain3XPrimaryMaxFreqMHzGet(pDomain3XPrimary,
        BOARDOBJ_GET_GRP_IDX_8BIT(&pDomain3XProg->super.super.super.super),
        pFreqMaxMHz);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainProgMaxFreqMHzGet_3X_PRIMARY_exit;
    }

clkDomainProgMaxFreqMHzGet_3X_PRIMARY_exit:
    return status;
}

/*!
 * @copydoc ClkDomain3XPrimaryMaxFreqMHzGet
 */
FLCN_STATUS
clkDomain3XPrimaryMaxFreqMHzGet
(
    CLK_DOMAIN_3X_PRIMARY  *pDomain3XPrimary,
    LwU8                   clkDomIdx,
    LwU32                 *pFreqMaxMHz
)
{
    CLK_DOMAIN_3X_PROG     *pDomain3XProg =
        (CLK_DOMAIN_3X_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomain3XPrimary);
    CLK_PROG_3X_PRIMARY     *pProg3XPrimary;
    LW2080_CTRL_CLK_VF_PAIR vfPair        = { 0 };
    FLCN_STATUS status;
    CLK_PROG *pProg3X;

    pProg3X = BOARDOBJGRP_OBJ_GET(CLK_PROG, pDomain3XProg->clkProgIdxLast);
    if (pProg3X == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto clkDomain3XPrimaryMaxFreqMHzGet_exit;
    }

    pProg3XPrimary = CLK_PROG_BOARDOBJ_TO_INTERFACE_CAST(pProg3X, 3X_PRIMARY);
    if (pProg3XPrimary == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto clkDomain3XPrimaryMaxFreqMHzGet_exit;
    }

    status = clkProg3XPrimaryMaxFreqMHzGet(pProg3XPrimary,
                pDomain3XPrimary, clkDomIdx, pFreqMaxMHz, &vfPair);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

clkDomain3XPrimaryMaxFreqMHzGet_exit:
    return status;
}

/*!
 * @copydoc ClkDomain3XPrimaryGetPrimaryProgIdxFromSecondaryFreqMHz
 */
LwU8
clkDomain3XPrimaryGetPrimaryProgIdxFromSecondaryFreqMHz
(
    CLK_DOMAIN_3X_PRIMARY   *pDomain3XPrimary,
    LwU8                    secondaryClkDomIdx,
    LwU16                   freqMHz
)
{
    CLK_DOMAIN_3X_PROG *pDomain3XProg = (CLK_DOMAIN_3X_PROG *)
        (CLK_DOMAIN_3X_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomain3XPrimary);
    CLK_PROG_3X_PRIMARY *pProg3XPrimary;
    LwU8                progIdx     = LW2080_CTRL_CLK_CLK_PROG_IDX_ILWALID;
    LwBool              bIsValidIdx = LW_FALSE;
    LwU8                i;

    //
    // Iterate through the set of CLK_PROG entries to find best matches.
    // If we do not find the best match, we will consider the last clock
    // programming entry as the best match. This is a temp WAR provided
    // to support the existing knobs to update the ratio value. It will
    // work as long as we have only single prog entry for GPC clock domain.
    //
    for (i = pDomain3XProg->clkProgIdxFirst;
            i <= pDomain3XProg->clkProgIdxLast;
            i++)
    {
        CLK_PROG *pProg3X = BOARDOBJGRP_OBJ_GET(CLK_PROG, i);

        if (pProg3X == NULL)
        {
            PMU_BREAKPOINT();
            progIdx = LW2080_CTRL_CLK_CLK_PROG_IDX_ILWALID;
            break;
        }

        pProg3XPrimary = CLK_PROG_BOARDOBJ_TO_INTERFACE_CAST(pProg3X, 3X_PRIMARY);
        if (pProg3XPrimary == NULL)
        {
            PMU_BREAKPOINT();
            progIdx = LW2080_CTRL_CLK_CLK_PROG_IDX_ILWALID;
            break;
        }

        progIdx       = i;
        bIsValidIdx   =
            clkProg3XPrimaryIsPrimaryProgIdxForSecondaryFreqMHz(pProg3XPrimary,
                pDomain3XPrimary, secondaryClkDomIdx, freqMHz);
        if (bIsValidIdx)
        {
            break;
        }
    }

    return progIdx;
}

/*!
 * @copydoc ClkDomain3XProgIsOVOCEnabled
 */
LwBool
clkDomain3XProgIsOVOCEnabled_3X_PRIMARY
(
    CLK_DOMAIN_3X_PROG  *pDomain3XProg
)
{
    return  ((!clkDomainsDebugModeEnabled())            &&
             (LW2080_CTRL_CLK_DOMAIN_PCIEGENCLK !=
                clkDomainApiDomainGet(pDomain3XProg))   &&
             (Clk.domains.bOverrideOVOC                 ||
              (((pDomain3XProg)->freqDeltaMinMHz != 0)  ||
               ((pDomain3XProg)->freqDeltaMaxMHz != 0))));
}

/* ------------------------- Private Functions ------------------------------ */
