/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file clk_prog_3x.c
 *
 * @brief Module managing all state related to the CLK_PROG_3X structure.  This
 * structure defines how to program a given frequency on given CLK_DOMAIN -
 * including which frequency generator to use and potentially the necessary VF
 * mapping, per the VBIOS Clock Programming Table 1.0 specification.
 *
 */
//! https://wiki.lwpu.com/engwiki/index.php/Resman/PState/Data_Tables/Clock_Programming_Table/1.0_Spec

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * Given the input frequency @ref pFreqMHz, found the next lower freq
 * supported by the given clock programming entry.
 *
 * @param[in]       pProg3X     CLK_PROG_3X pointer
 * @param[in]       clkDomain   Clock domain @ref LW2080_CTRL_CLK_DOMAIN_<xyz>
 * @param[in/out]   pFreqMHz
 *     Pointer to the buffer which is used to iterate over the frequency. The IN
 *     is the last frequency value that we read where as the OUT is the next freq
 *     value that follows the last frequency value represented by ref@ pFreqMHz.
 *     Note we are iterating from highest -> lowest freq value.
 *
 * @return FLCN_OK Successfully found the next lower freq value
 * @return Other errors
 *     An unexpected error coming from higher functions.
 *     ref@ clkNafllFreqsIterate
 */
FLCN_STATUS
clkProg3XFreqsIterate
(
    CLK_PROG_3X    *pProg3X,
    LwU32           clkDomain,
    LwU16          *pFreqMHz
)
{
    FLCN_STATUS         status        = FLCN_OK;
    LwU16               freqMaxMHz    = pProg3X->freqMaxMHz;
    CLK_DOMAIN_3X_PROG *pDomain3XProg =
        (CLK_DOMAIN_3X_PROG *)clkDomainsGetByApiDomain(clkDomain);

    // Sanity check
    if (pDomain3XProg == NULL)
    {
        status = FLCN_ERR_OBJECT_NOT_FOUND;
        goto clkProg3XFreqsIterate_exit;
    }

    // quantize the delta adjusted freqMaxMHz to freq step size.
    status = clkProg3XFreqMaxAdjAndQuantize(pProg3X, pDomain3XProg, &freqMaxMHz);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkProg3XFreqsIterate_exit;
    }

    // For the first time, return the MAX value.
    if ((*pFreqMHz) == CLK_DOMAIN_3X_PROG_ITERATE_MAX_FREQ)
    {
        *pFreqMHz  = freqMaxMHz;
        goto clkProg3XFreqsIterate_exit;
    }

    // Enumerate specific in the source-specific manner.
    switch (pProg3X->source)
    {
        // PLL -> Quantize to step size listed in entry.
        case LW2080_CTRL_CLK_PROG_1X_SOURCE_PLL:
        {
            //
            // On first call, return Max frequency value.
            // If the step size is zero, always return MAX value.
            //
            if (((*pFreqMHz) > freqMaxMHz) || 
                (0U == pProg3X->sourceData.pll.freqStepSizeMHz))
            {
                *pFreqMHz = freqMaxMHz;
                break;
            }

            //
            // Substract the step size to get the next highest freq lower than
            // the current freq and quatize the freq output.
            //
            LwU16 numSteps;

            numSteps = ((freqMaxMHz - (*pFreqMHz)) /
                    pProg3X->sourceData.pll.freqStepSizeMHz);

            (*pFreqMHz) = (freqMaxMHz -
                ((numSteps + 1U) * pProg3X->sourceData.pll.freqStepSizeMHz));
            break;
        }
        // ONE_SOURCE only supports a single frequency - freqMaxMHz.
        case LW2080_CTRL_CLK_PROG_1X_SOURCE_ONE_SOURCE:
        {
            *pFreqMHz = freqMaxMHz;
            break;
        }
        // NAFLL -> Quantize to nearest supported frequency per NAFLL settings.
        case LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL:
        {
            //
            // Call into NAFLL code to enumerate the frequency points in this
            // CLK_PROG's frequency range.
            //
            status = clkNafllFreqsIterate(clkDomain, pFreqMHz);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkProg3XFreqsIterate_exit;
            }
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            goto clkProg3XFreqsIterate_exit;
        }
    }

clkProg3XFreqsIterate_exit:
    return status;
}

/*!
 * @copydoc ClkProg3XGetClkProgIdxFromFreq
 */
LwU8
clkProg3XGetClkProgIdxFromFreq
(
    CLK_PROG_3X        *pProg3X,
    CLK_DOMAIN_3X_PROG *pDomain3XProg,
    LwU32               freqMHz,
    LwBool              bReqFreqDeltaAdj
)
{
    LwU8 progIdx = LW2080_CTRL_CLK_CLK_PROG_IDX_ILWALID;

    if (freqMHz <= pProg3X->freqMaxMHz)
    {
        progIdx = BOARDOBJ_GET_GRP_IDX_8BIT(&pProg3X->super.super.super);
    }

    return progIdx;
}

/*!
 * @copydoc ClkProg3XFreqQuantize
 */
FLCN_STATUS
clkProg3XFreqQuantize
(
    CLK_PROG_3X                *pProg3X,
    CLK_DOMAIN_3X_PROG         *pDomain3XProg,
    LW2080_CTRL_CLK_VF_INPUT   *pInputFreq,
    LW2080_CTRL_CLK_VF_OUTPUT  *pOutputFreq,
    LwBool                      bFloor,
    LwBool                      bReqFreqDeltaAdj
)
{
    FLCN_STATUS status     = FLCN_OK;
    LwU16       freqMaxMHz = pProg3X->freqMaxMHz;

    if (bReqFreqDeltaAdj)
    {
        // Adjust the frequency delta offset and quantize the result.
        status = clkProg3XFreqMaxAdjAndQuantize(pProg3X, pDomain3XProg,
            &freqMaxMHz);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_STATE;
            goto clkProg3XFreqQuantize_exit;
        }
    }

    // Can't quantize above freqMaxMHz, so bound as applicable.
    if (pInputFreq->value >= freqMaxMHz)
    {
        //
        // If floor (rounding down) or frequency is equal to freqMaxMHz,
        // quantize to freqMaxMHz and early return.
        //
        if (bFloor ||
            (pInputFreq->value == freqMaxMHz))
        {
            pOutputFreq->value = freqMaxMHz;
            goto clkProg3XFreqQuantize_exit;
        }
        //
        // If ceiling and frequency is greater than freqMaxMHz, the frequency is
        // outside of the range of and cannot be quantized by this CLK_PROG_3X
        // object.  Should try with the next CLK_PROG_3X object.
        //
        else
        {
            // If requested, set default value.
            if (FLD_TEST_DRF(2080_CTRL_CLK, _VF_INPUT_FLAGS, _VF_POINT_SET_DEFAULT, _YES, pInputFreq->flags))
            {
                pOutputFreq->value = freqMaxMHz;
            }
            status = FLCN_ERR_OUT_OF_RANGE;
            goto clkProg3XFreqQuantize_exit;
        }
    }

    //
    // Requested frequency is less than freqMaxMHz, so attempt to quantize based
    // on the parameters corresponding to the SOURCE of this CLK_PROG_3X object.
    //
    switch (pProg3X->source)
    {
        // PLL -> Quantize to step size listed in entry.
        case LW2080_CTRL_CLK_PROG_1X_SOURCE_PLL:
        {
            LwU32 n;

            //
            // If there is no step size (zero/0), can only use this entry if
            // quantized to the maximum frequency.
            //
            if (0U == pProg3X->sourceData.pll.freqStepSizeMHz)
            {
                // If floor, fail as can't round up to freqMaxMHz.
                if (bFloor)
                {
                    status = FLCN_ERR_OUT_OF_RANGE;
                    goto clkProg3XFreqQuantize_exit;
                }
                // If ceiling, round up to freqMaxMHz.
                else
                {
                    pOutputFreq->value = freqMaxMHz;
                }
                break;
            }

            //
            // If there is step size, quantize to nearest step.
            //
            // Quantized frequencies are defined as:
            //     targetFreqMHz = freqMaxMHz - n * freqStepSizeMHz
            //     =>
            //     n = (freqMaxMHz - targetFreqMHz) / freqStepSizeMHz
            //
            // If floor (rounding down), subtract freqStepSizeMHz - 1 from
            // targetFreqMHz so that frequencies will be quantized to floor.
            // Without this adjustment, will always quantize to ceiling.
            //
            pOutputFreq->value = pInputFreq->value;
            if (bFloor)
            {
                pOutputFreq->value -= pProg3X->sourceData.pll.freqStepSizeMHz - 1U;
            }
            n = (freqMaxMHz - pOutputFreq->value) /
                    pProg3X->sourceData.pll.freqStepSizeMHz;

            //
            // Callwlate quantized frequency as:
            //     targetFreqMHz = freqMaxMHz - n * freqStepSizeMHz
            //
            pOutputFreq->value = freqMaxMHz -
                n * pProg3X->sourceData.pll.freqStepSizeMHz;
            break;
        }
        //
        // ONE_SOURCE only supports a single frequency - freqMaxMHz.  Attempt to
        // quantize to that value.
        //
        case LW2080_CTRL_CLK_PROG_1X_SOURCE_ONE_SOURCE:
        {
            // If floor, fail as can't round up to freqMaxMHz.
            if (bFloor)
            {
                status = FLCN_ERR_OUT_OF_RANGE;
                goto clkProg3XFreqQuantize_exit;
            }
            // If ceiling, round up to freqMaxMHz.
            else
            {
                pOutputFreq->value = freqMaxMHz;
            }
            break;
        }
        // NAFLL -> Quantize to nearest supported frequency per NAFLL settings.
        case LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL:
        {
            pOutputFreq->value = pInputFreq->value;
            status = clkNafllFreqMHzQuantize(pDomain3XProg->super.super.apiDomain,
                       ((LwU16*)&(pOutputFreq->value)), bFloor);
            if (status != FLCN_OK)
            {
                 PMU_BREAKPOINT();
                 goto clkProg3XFreqQuantize_exit;
            }
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_STATE;
            goto clkProg3XFreqQuantize_exit;
            break;
        }
    }

clkProg3XFreqQuantize_exit:
    //
    // If we have found a valid match set best match to stop searching in the
    // next CLK_PROG_3X
    //
    if ((status == FLCN_OK) &&
        ((!bFloor) || (pInputFreq->value <= freqMaxMHz)))
    {
        pOutputFreq->inputBestMatch = pOutputFreq->value;
    }
    return status;
}

/*!
 * @copydoc ClkProg3XFreqMHzQuantize
 */
FLCN_STATUS
clkProg3XFreqMHzQuantize
(
    CLK_PROG_3X         *pProg3X,
    CLK_DOMAIN_3X_PROG  *pDomain3XProg,
    LwU16               *pFreqMHz,
    LwBool               bFloor
)
{
    FLCN_STATUS status = FLCN_OK;

    //
    // Quantize based on the parameters corresponding to the SOURCE of this
    // CLK_PROG_3X object.
    //
    switch (pProg3X->source)
    {
        // PLL -> Quantize to step size listed in entry.
        case LW2080_CTRL_CLK_PROG_1X_SOURCE_PLL:
        {
            LwU16 numstep;

            // If there is no step size (zero/0), quantization is NOT required.
            if (0U == pProg3X->sourceData.pll.freqStepSizeMHz)
            {
                break;
            }

            //
            // If there is step size, quantize the input freq to
            // the step size
            //
            // Quantized frequencies are defined as:
            // Quantized(freqMHzDelta) + pProg3X->freqMaxMHz
            //

            // Quantized(freqMHzDelta) to the step size based on sign of delta
            if (pProg3X->freqMaxMHz > *pFreqMHz)
            {
                //
                // IF bFloor
                //      Substract (step size - 1) from freqMHzDelta
                //
                //  targetFreqMaxMHz =
                //  ((freqMHzDelta / freqStepSizeMHz) * freqStepSizeMHz) + pProg3X->freqMaxMHz
                //
                // Example : freqMaxMHz = 1005, freqStepSizeMHz = 100, *pFreqMHz = 906
                // targetFreqMaxMHz = 1005 - (((1005 - (906 - 99)) / 100) * 100)
                // targetFreqMaxMHz = 1005 - 100 = 905
                //
                if (bFloor)
                {
                    *pFreqMHz -= pProg3X->sourceData.pll.freqStepSizeMHz - 1U;
                }

                numstep = (pProg3X->freqMaxMHz - *pFreqMHz) /
                                pProg3X->sourceData.pll.freqStepSizeMHz;

                *pFreqMHz = numstep * pProg3X->sourceData.pll.freqStepSizeMHz;

                // Substract the quantized frequency delta from max clk programming freq
                *pFreqMHz = pProg3X->freqMaxMHz - *pFreqMHz;
            }
            else
            {
                //
                // IF !bFloor
                //      Add (step size - 1) to freqMHzDelta
                //
                //  targetFreqMaxMHz =
                //  ((freqMHzDelta / freqStepSizeMHz) * freqStepSizeMHz) + pProg3X->freqMaxMHz
                //
                // Example : freqMaxMHz = 1005, freqStepSizeMHz = 100, *pFreqMHz = 1104
                // targetFreqMaxMHz = 1005 + ((((1104 + 99) - 1005) / 100) * 100)
                // targetFreqMaxMHz = 1005 + 100 = 1105
                //
                if (!bFloor)
                {
                    *pFreqMHz += pProg3X->sourceData.pll.freqStepSizeMHz - 1U;
                }

                numstep = (*pFreqMHz - pProg3X->freqMaxMHz) /
                                pProg3X->sourceData.pll.freqStepSizeMHz;

                *pFreqMHz = numstep * pProg3X->sourceData.pll.freqStepSizeMHz;

                // Add the quantized frequency delta to the max clk programming freq
                *pFreqMHz = pProg3X->freqMaxMHz + *pFreqMHz;
            }

            break;
        }
        // ONE_SOURCE only supports a single frequency - freqMaxMHz.
        case LW2080_CTRL_CLK_PROG_1X_SOURCE_ONE_SOURCE:
        {
            // No need for quantization as there is NO step size.
            break;
        }
        // NAFLL -> Quantize to nearest supported frequency per NAFLL settings.
        case LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL:
        {
            status = clkNafllFreqMHzQuantize(pDomain3XProg->super.super.apiDomain,
                                             pFreqMHz,
                                             bFloor);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkProg3XFreqMHzQuantize_exit;
            }
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            goto clkProg3XFreqMHzQuantize_exit;
        }
    }

clkProg3XFreqMHzQuantize_exit:
    return status;
}

/*!
 * @copydoc ClkProg3XFreqMaxAdjAndQuantize
 */
FLCN_STATUS
clkProg3XFreqMaxAdjAndQuantize
(
    CLK_PROG_3X         *pProg3X,
    CLK_DOMAIN_3X_PROG  *pDomain3XProg,
    LwU16               *pFreqMHz
)
{
    FLCN_STATUS  status  = FLCN_ERR_NOT_SUPPORTED;

    // Call type-specific implemenation.
    switch (BOARDOBJ_GET_TYPE(pProg3X))
    {
        case LW2080_CTRL_CLK_CLK_PROG_TYPE_30:
        case LW2080_CTRL_CLK_CLK_PROG_TYPE_30_PRIMARY_RATIO:
        case LW2080_CTRL_CLK_CLK_PROG_TYPE_30_PRIMARY_TABLE:
        PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_CLK_CLK_PROG_30);
        {
            status = clkProg3XFreqMaxAdjAndQuantize_30(pProg3X, pDomain3XProg, pFreqMHz);
            break;
        }
        case LW2080_CTRL_CLK_CLK_PROG_TYPE_35:
        case LW2080_CTRL_CLK_CLK_PROG_TYPE_35_PRIMARY_RATIO:
        case LW2080_CTRL_CLK_CLK_PROG_TYPE_35_PRIMARY_TABLE:
        PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_CLK_CLK_PROG_EXTENDED_35);
        {
            status = clkProg3XFreqMaxAdjAndQuantize_35(pProg3X, pDomain3XProg, pFreqMHz);
            break;
        }
    }

    // Sanity check the output.
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/* ------------------------- Private Functions ------------------------------ */
