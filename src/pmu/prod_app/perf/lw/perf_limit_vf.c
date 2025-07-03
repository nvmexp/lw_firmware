/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perf_limit_vf.c
 * @copydoc perf_limit_vf.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/perf_limit_vf.h"

#include "pmu_objperf.h"
#include "pmu_objclk.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
//
// Verify that PERF_LIMITS_VF_ARRAY can support at least as many
// CLK_DOMAINS as PERF_LIMITS ARB structures.
//
ct_assert(PERF_PERF_LIMITS_VF_ARRAY_VALUES_MAX >= PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS);

//
// Verify that the PERF_LIMITS_VF_RANGE index size is the same as the
// ARBITRATION TUPLE index size.  Many pieces of code use these
// interchangeably.
//
ct_assert(PERF_LIMITS_VF_RANGE_IDX_NUM_IDXS == LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_NUM_IDXS);

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc PerfLimitsFreqMHzToVoltageuV
 */
FLCN_STATUS
perfLimitsFreqkHzToVoltageuV
(
    const PERF_LIMITS_VF *pCVfDomain,
    PERF_LIMITS_VF       *pVfRail,
    LwBool                bDefault
)
{
    CLK_DOMAIN               *pDomain = NULL;
    CLK_DOMAIN_PROG          *pDomainProg;
    LW2080_CTRL_CLK_VF_INPUT  freqMHzInput;
    LW2080_CTRL_CLK_VF_OUTPUT voltuVOutput;
    FLCN_STATUS               status        = FLCN_OK;

    pDomain = CLK_DOMAIN_GET(pCVfDomain->idx);
    if (pDomain == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfLimitsFreqMHzToVoltageuV_exit;
    }

    pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
    if (pDomainProg == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto perfLimitsFreqMHzToVoltageuV_exit;
    }

    // Pack VF input and output structs.
    LW2080_CTRL_CLK_VF_INPUT_INIT(&freqMHzInput);
    LW2080_CTRL_CLK_VF_OUTPUT_INIT(&voltuVOutput);
    // If applicable, scale frequency kHz -> MHz.
    freqMHzInput.value = pCVfDomain->value /
        clkDomainFreqkHzScaleGet(pDomain);

    if (bDefault)
    {
        freqMHzInput.flags = FLD_SET_DRF(2080_CTRL_CLK, _VF_INPUT_FLAGS,
                                _VF_POINT_SET_DEFAULT, _YES, freqMHzInput.flags);
    }

    // Call into CLK_DOMAIN_PROG freqToVolt() interface.
    status = clkDomainProgFreqToVolt(
                pDomainProg,
                pVfRail->idx,
                LW2080_CTRL_CLK_VOLTAGE_TYPE_POR,
                &freqMHzInput,
                &voltuVOutput,
                NULL);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfLimitsFreqMHzToVoltageuV_exit;
    }

    //
    // Catch errors when no frequency >= input frequency.  Can't determine the
    // required voltage in this case.
    //
    if (LW2080_CTRL_CLK_VF_VALUE_ILWALID == voltuVOutput.inputBestMatch)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_OUT_OF_RANGE;
        goto perfLimitsFreqMHzToVoltageuV_exit;
    }

    // Copy out the minimum voltage (uV) for the VOLT_RAIL.
    pVfRail->value = voltuVOutput.value;

perfLimitsFreqMHzToVoltageuV_exit:
    return status;
}

/*!
 * @copydoc PerfLimitsFreqPropagate
 */
FLCN_STATUS
perfLimitsFreqPropagate
(
    const PERF_LIMITS_PSTATE_RANGE *pCPstateRange,
    const PERF_LIMITS_VF_ARRAY     *pCVfArrayIn, 
    PERF_LIMITS_VF_ARRAY           *pVfArrayOut,
    LwBoardObjIdx                   clkPropTopIdx
)
{
    CLK_DOMAIN     *pDomain;
    PERF_LIMITS_VF  vfDomain;
    LwBoardObjIdx   d;
    FLCN_STATUS     status = FLCN_OK;

    CLK_DOMAIN_PROG_FREQ_PROP_IN_OUT_TUPLE freqPropTuple;

    // Init input
    CLK_DOMAIN_PROG_FREQ_PROP_IN_OUT_TUPLE_INIT(&freqPropTuple);
    freqPropTuple.pDomainsMaskIn  = pCVfArrayIn->pMask;
    freqPropTuple.pDomainsMaskOut = pVfArrayOut->pMask;

    //
    // Bail if there are no outputs to be set
    // In future, consider asserting that input and output masks do not
    // intersect.
    //
    if (boardObjGrpMaskIsZero(pVfArrayOut->pMask))
    {
        goto perfLimitsFreqPropagate_exit;
    }

    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, d,
        &pCVfArrayIn->pMask->super)
    {
        freqPropTuple.freqMHz[d] =
            pCVfArrayIn->values[d] / clkDomainFreqkHzScaleGet(pDomain);
    }
    BOARDOBJGRP_ITERATOR_END;

    // Propagate from input -> output
    status = clkDomainsProgFreqPropagate(&freqPropTuple,
                 clkPropTopIdx);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfLimitsFreqPropagate_exit;
    }

    // Update the output frequency.
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, d,
        &pVfArrayOut->pMask->super)
    {
        pVfArrayOut->values[d] =
            freqPropTuple.freqMHz[d] * clkDomainFreqkHzScaleGet(pDomain);

        //
        // When inputBestMask == _ILWALID, no VF lwrve voltage <= input voltage.
        //
        if (LW2080_CTRL_CLK_VF_VALUE_ILWALID == pVfArrayOut->values[d])
        {
            PERF_LIMITS_VF_RANGE vfDomainRange;

            //
            // If pCPstateRange = NULL, can't determine the frequency
            // intersect in this case.  Technically, this should be impossible.
            //
            if (pCPstateRange == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_OUT_OF_RANGE;
                goto perfLimitsFreqPropagate_exit;
            }

            //
            // If _DEFAULT_VF_POINT_SET = _FALSE, the frequency intersect will fall
            // back to the minimum PSTATE's CLK_DOMAIN frequency range.
            //
            vfDomainRange.idx = BOARDOBJ_GRP_IDX_TO_8BIT(d);
            status = perfLimitsPstateIdxRangeToFreqkHzRange(pCPstateRange,
                        &vfDomainRange);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_OUT_OF_RANGE;
                goto perfLimitsFreqPropagate_exit;
            }
            pVfArrayOut->values[d] = vfDomainRange.values[
                LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MIN];
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    //
    // Quantize the propagated frequency tuple to available PSTATE ranges
    // and VF lwrve data.
    //
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, d,
        &pVfArrayOut->pMask->super)
    {
        vfDomain.idx    = BOARDOBJ_GRP_IDX_TO_8BIT(d);
        vfDomain.value  = pVfArrayOut->values[d];

        status = perfLimitsFreqkHzQuantize(&vfDomain, pCPstateRange, LW_TRUE);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfLimitsFreqPropagate_exit;
        }
        pVfArrayOut->values[d] = vfDomain.value;
    }
    BOARDOBJGRP_ITERATOR_END;

perfLimitsFreqPropagate_exit:
    return status;
}

/*!
 * @copydoc PerfLimitsVoltageuVToFreqkHz
 */
FLCN_STATUS
perfLimitsVoltageuVToFreqkHz
(
    const PERF_LIMITS_VF           *pCVfRail,
    const PERF_LIMITS_PSTATE_RANGE *pCPstateRange,
    PERF_LIMITS_VF                 *pVfDomain,
    LwBool                          bDefault,
    LwBool                          bQuantize
)
{
    CLK_DOMAIN               *pDomain = NULL;
    CLK_DOMAIN_PROG          *pDomainProg;
    LW2080_CTRL_CLK_VF_INPUT  voltuVInput;
    LW2080_CTRL_CLK_VF_OUTPUT freqMHzOutput;
    FLCN_STATUS status = FLCN_OK;

    pDomain = CLK_DOMAIN_GET(pVfDomain->idx);
    if (pDomain == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfLimitsVoltageuVToFreqkHz_exit;
    }

    pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
    if (pDomainProg == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto perfLimitsVoltageuVToFreqkHz_exit;
    }

    // Pack VF input and output structs.
    LW2080_CTRL_CLK_VF_INPUT_INIT(&voltuVInput);
    LW2080_CTRL_CLK_VF_OUTPUT_INIT(&freqMHzOutput);
    voltuVInput.value = pCVfRail->value;
    if (bDefault)
    {
        voltuVInput.flags = FLD_SET_DRF(2080_CTRL_CLK, _VF_INPUT_FLAGS,
                                _VF_POINT_SET_DEFAULT, _YES, voltuVInput.flags);
    }

    // Call into CLK_DOMAIN_PROG voltToFreq() interface.
    status = clkDomainProgVoltToFreq(
                pDomainProg,
                pCVfRail->idx,
                LW2080_CTRL_CLK_VOLTAGE_TYPE_POR,
                &voltuVInput,
                &freqMHzOutput,
                NULL);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfLimitsVoltageuVToFreqkHz_exit;
    }

    //
    // When inputBestMask == _ILWALID, no VF lwrve voltage <= input voltage.
    //
    if (LW2080_CTRL_CLK_VF_VALUE_ILWALID == freqMHzOutput.inputBestMatch)
    {
        PERF_LIMITS_VF_RANGE vfDomainRange;

        //
        // If _DEFAULT_VF_POINT_SET = _TRUE, can't determine the frequency
        // intersect in this case.  Technically, this should be impossible.
        //
        if (bDefault ||
            (pCPstateRange == NULL))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_OUT_OF_RANGE;
            goto perfLimitsVoltageuVToFreqkHz_exit;
        }

        //
        // If _DEFAULT_VF_POINT_SET = _FALSE, the frequency intersect will fall
        // back to the minimum PSTATE's CLK_DOMAIN frequency range.
        //
        vfDomainRange.idx = pVfDomain->idx;
        status = perfLimitsPstateIdxRangeToFreqkHzRange(pCPstateRange,
                    &vfDomainRange);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_OUT_OF_RANGE;
            goto perfLimitsVoltageuVToFreqkHz_exit;
        }
        pVfDomain->value = vfDomainRange.values[
            LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MIN];
    }
    else
    {
        //
        // Copy out the maximum frequency (kHz) for the CLK_DOMAIN, scaling MHz ->
        // kHz if appropriate.
        //
        pVfDomain->value = freqMHzOutput.value *
            clkDomainFreqkHzScaleGet(pDomain);
    }

    // If requested by the caller, quantize the output frequency.
    if (bQuantize)
    {
        //
        // Quantize the frequency from the V->F lookup to available PSTATE ranges
        // and VF lwrve data (which should be redundant).  Must quantize down
        // (bFloor = LW_TRUE) so as not to violate the F_{max} for V.
        //
        status = perfLimitsFreqkHzQuantize(pVfDomain, pCPstateRange, LW_TRUE);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfLimitsVoltageuVToFreqkHz_exit;
        }
    }

perfLimitsVoltageuVToFreqkHz_exit:
    return status;
}

/*!
 * @copydoc PerfLimitsFreqkHzQuantize
 */
FLCN_STATUS
perfLimitsFreqkHzQuantize
(
    PERF_LIMITS_VF                 *pVfDomain,
    const PERF_LIMITS_PSTATE_RANGE *pCPstateRange,
    LwBool                          bFloor
)
{
    FLCN_STATUS    status = FLCN_OK;

    // 1. Quantize value to VF frequencies.
    {
        LW2080_CTRL_CLK_VF_INPUT  vfInput;
        LW2080_CTRL_CLK_VF_OUTPUT vfOutput;
        CLK_DOMAIN               *pDomain;
        CLK_DOMAIN_PROG          *pDomainProg;

        pDomain = CLK_DOMAIN_GET(pVfDomain->idx);
        if (pDomain == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto perfLimitsFreqkHzQuantize_exit;
        }

        pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
        if (pDomainProg == NULL)
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto perfLimitsFreqkHzQuantize_exit;
        }

        // Quantize frequency from CLK_DOMAIN_3X_PROG (i.e. the VF data).
        LW2080_CTRL_CLK_VF_INPUT_INIT(&vfInput);
        LW2080_CTRL_CLK_VF_OUTPUT_INIT(&vfOutput);
        vfInput.flags = FLD_SET_DRF(2080_CTRL_CLK, _VF_INPUT_FLAGS,
                                _VF_POINT_SET_DEFAULT, _YES, vfInput.flags);
        vfInput.value = pVfDomain->value /
            clkDomainFreqkHzScaleGet(pDomain);
        status = clkDomainProgFreqQuantize(
                    pDomainProg,                    // pDomainProg
                    &vfInput,                       // pInputFreq
                    &vfOutput,                      // pOutputFreq
                    bFloor,                         // bFloor
                    LW_TRUE);                       // bBound
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfLimitsFreqkHzQuantize_exit;
        }
        if (LW2080_CTRL_CLK_VF_VALUE_ILWALID == vfOutput.inputBestMatch)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_OUT_OF_RANGE;
            goto perfLimitsFreqkHzQuantize_exit;
        }

        // Use the VF_quantized frequency.
        pVfDomain->value = vfOutput.value *
            clkDomainFreqkHzScaleGet(pDomain);
    }

    // 2. Quantize value to PSTATE ranges.
    {
        PERF_LIMITS_VF_RANGE vfDomainPstateRange;
        BOARDOBJGRPMASK_E32  pstatesMask;
        PSTATE              *pPstate;
        LwU32                freqkHzQuant = 0;
        LwBoardObjIdx        p;

        //
        // Build the mask of PSTATEs to use for quantization:
        // 1. Default to the full PSTATE mask.
        //
        boardObjGrpMaskInit_E32(&pstatesMask);
        status = boardObjGrpMaskCopy(&pstatesMask, &(PSTATES_GET()->super.objMask));
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfLimitsFreqkHzQuantize_exit;
        }
        //
        // 2. If a PSTATE range is specified, limit to only the PSTATEs within
        // that range.
        //
        if (pCPstateRange != NULL)
        {
            BOARDOBJGRPMASK_E32 pstatesIncludeMask;

            // Build mask of indexes within the PSTATE_RANGE.
            boardObjGrpMaskInit_E32(&pstatesIncludeMask);
            status = boardObjGrpMaskBitRangeSet(&pstatesIncludeMask,
                        pCPstateRange->values[
                            LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MIN],
                        pCPstateRange->values[
                            LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MAX]);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto perfLimitsFreqkHzQuantize_exit;
            }

            // And (&) include mask into the full PSTATE mask.
            status = boardObjGrpMaskAnd(&pstatesMask, &pstatesMask, &pstatesIncludeMask);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto perfLimitsFreqkHzQuantize_exit;
            }
        }
        vfDomainPstateRange.idx = pVfDomain->idx;

        BOARDOBJGRP_ITERATOR_DYNAMIC_CAST_BEGIN(PERF, PSTATE, BASE, pPstate,
                                                p, &pstatesMask.super, &status,
                                                perfLimitsFreqkHzQuantize_exit)
        {
            PERF_LIMITS_PSTATE_RANGE pstateRange;

            //
            // Pack the pstate range to the PSTATE idx.  Will be used to retrieve
            // the frequency range below.
            //
            PERF_LIMITS_RANGE_SET_VALUE(&pstateRange, p);

            // Query the frequency values for the PSTATE.
            status = perfLimitsPstateIdxRangeToFreqkHzRange(
                        &pstateRange, &vfDomainPstateRange);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto perfLimitsFreqkHzQuantize_exit;
            }

            //
            // Compare input frequency against PSTATE min and max to see if
            // frequency is within this PSTATE range.  If so, can keep this
            // quantization regardless of quantization direction.
            //
            if (PERF_LIMITS_RANGE_CONTAINS_VALUE(&vfDomainPstateRange,
                    pVfDomain->value))
            {
                freqkHzQuant = pVfDomain->value;
                break;
            }
            //
            // If below PSTATE.min, can stop search. Due to PSTATE monotonicity,
            // either one of the following will be true:
            //
            // a. Can quantize up (bFloor = LW_FALSE) to PSTATE.min as the
            // best/closest match.
            //
            // b. If quantizing down (bFloor = LW_TRUE), no succeeding PSTATE
            // could be a match, so must pick this one if no other preceding
            // PSTATE matched.
            //
            else if (PERF_LIMITS_RANGE_ABOVE_VALUE(&vfDomainPstateRange,
                        pVfDomain->value))
            {
                // If requested, quantize up.
                if (!bFloor ||
                    (freqkHzQuant == 0))
                {
                    freqkHzQuant = vfDomainPstateRange.values[
                        LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MIN];
                }
                break;
            }
            //
            // If above PSTATE.max, can consider PSTATE.max as a result
            // regardless of quantization direction (bFloor)*, but must continue
            // the search to succeeding PSTATEs.  Due to PSTATE monotonicty, a
            // succeeding PSTATE may be a better/closer match.
            //
            // * - An explanation of why we don't care about quantization direction:
            //
            // 1.  Either direction - A succeeding PSTATE may contain the input frequency
            // within its frequency range.
            //
            // 2.  Down (bFloor == LW_TRUE) - A succeeding PSTATE.max < the
            // input frequency may be a better match.
            //
            // 3.  Up (bFloor == LW_FALSE) - A succeeding PSTATE.min > the input
            // frequency may be a better match.  However, if no PSTATE.min is
            // found to be > the input frequency, must fall back to the last
            // PSTATE.max.
            //
            else
            {
                freqkHzQuant = vfDomainPstateRange.values[
                    LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MAX];
            }
        }
        BOARDOBJGRP_ITERATOR_DYNAMIC_CAST_END;

        // Store the quantized value from the search above.
        if (freqkHzQuant == 0)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto perfLimitsFreqkHzQuantize_exit;
        }
        pVfDomain->value = freqkHzQuant;
    }

perfLimitsFreqkHzQuantize_exit:
    return status;
}

/*!
 * @copydoc PerfLimitsPstateIdxRangeToFreqkHzRange
 */
FLCN_STATUS
perfLimitsPstateIdxRangeToFreqkHzRange
(
    const PERF_LIMITS_PSTATE_RANGE *pCPstateRange,
    PERF_LIMITS_VF_RANGE           *pVfDomainRange
)
{
    FLCN_STATUS  status = FLCN_OK;
    LwU8         i;

    // Iterate over the min and max separately.
    PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR(i)
    {
        const PERF_PSTATE_35_CLOCK_ENTRY *pcPstateClkFreq;
        PSTATE_35 *pPstate35 = (PSTATE_35 *)PSTATE_GET(pCPstateRange->values[i]);
        LwU32   freqkHz;

        if (pPstate35 == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto perfLimitsPstateIdxRangeToFreqkHzRange_exit;
        }

        // Query the frequency values for the PSTATE.
        status = perfPstate35ClkFreqGetRef(pPstate35, pVfDomainRange->idx, &pcPstateClkFreq);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfLimitsPstateIdxRangeToFreqkHzRange_exit;
        }

        // Pull the corresponding min vs. max value out of the PSTATE.
        if (LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MIN == i)
        {
            freqkHz = pcPstateClkFreq->min.freqVFMaxkHz;
        }
        else if (LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MAX == i)
        {
            freqkHz = pcPstateClkFreq->max.freqVFMaxkHz;
        }
        else
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_STATE;
            goto perfLimitsPstateIdxRangeToFreqkHzRange_exit;
        }

        // Store the trimmed value in the output range structure.
        pVfDomainRange->values[i] = freqkHz;
    }
    PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR_END;

perfLimitsPstateIdxRangeToFreqkHzRange_exit:
    return status;
}

/*!
 * @copydoc PerfLimitsFreqkHzToPstateIdxRange
 */
FLCN_STATUS
perfLimitsFreqkHzToPstateIdxRange
(
    const PERF_LIMITS_VF     *pCVfDomain,
    PERF_LIMITS_PSTATE_RANGE *pPstateRange
)
{
    PERF_LIMITS_VF_RANGE vfDomainPstateRange;
    PSTATE              *pPstate;
    FLCN_STATUS          status = FLCN_OK;
    LwBoardObjIdx        p;

    //
    // Init the PSTATE index range to completely ilwerse - [LW_U32_MAX,
    // LW_U32_MIN].  Will use these values to find the loosest possible range.
    //
    pPstateRange->values[
        LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MIN] = LW_U32_MAX;
    pPstateRange->values[
        LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MAX] = LW_U32_MIN;

    //
    // Search though the PSTATEs in ascending order and find the loosest
    // possible bounds for the range.
    //
    vfDomainPstateRange.idx = pCVfDomain->idx;

    BOARDOBJGRP_ITERATOR_DYNAMIC_CAST_BEGIN(PERF, PSTATE, BASE, pPstate,
                                            p, NULL, &status,
                                            perfLimitsFreqkHzToPstateIdxRange_exit)
    {
        PERF_LIMITS_PSTATE_RANGE pstateRange;

        //
        // Pack the pstate range to the PSTATE idx.  Will be used to retrieve
        // the frequency range below.
        //
        PERF_LIMITS_RANGE_SET_VALUE(&pstateRange, p);

        // Query the frequency values for the PSTATE.
        status = perfLimitsPstateIdxRangeToFreqkHzRange(
                    &pstateRange, &vfDomainPstateRange);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfLimitsFreqkHzToPstateIdxRange_exit;
        }

        //
        // If PSTATE frequency range contains the value, see if this PSTATE idx
        // would expand the PSTATE idx range.
        //
        if (PERF_LIMITS_RANGE_CONTAINS_VALUE(&vfDomainPstateRange,
                pCVfDomain->value))
        {
            pPstateRange->values[
                    LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MIN] =
                LW_MIN(pPstateRange->values[
                           LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MIN],
                       p);
            pPstateRange->values[
                    LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MAX] =
                LW_MAX(pPstateRange->values[
                           LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MAX],
                       p);
        }
    }
    BOARDOBJGRP_ITERATOR_DYNAMIC_CAST_END;

    // Ensure that found coherent range - min <= max.
    if (!PERF_LIMITS_RANGE_IS_COHERENT(pPstateRange))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfLimitsFreqkHzToPstateIdxRange_exit;
    }

perfLimitsFreqkHzToPstateIdxRange_exit:
    return status;
}

/*!
 * @copydoc PerfLimitsVpstateIdxToClkDomainsMask
 */
FLCN_STATUS
perfLimitsVpstateIdxToClkDomainsMask
(
    LwU8                 vpstateIdx,
    BOARDOBJGRPMASK_E32 *pClkDomainsMask
)
{
    VPSTATE_3X *pVpstate3x;
    FLCN_STATUS status     = FLCN_OK;

    // Check that VPSTATE idx is valid.
    if (!BOARDOBJGRP_IS_VALID(VPSTATE, vpstateIdx))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfLimitsVpstateIdxToClkDomainsMask_exit;
    }

    // Fetch the VPSTATE pointer from the BOARDOBJGRP.
    pVpstate3x = (VPSTATE_3X *)BOARDOBJGRP_OBJ_GET(VPSTATE, vpstateIdx);
    if (pVpstate3x == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto perfLimitsVpstateIdxToClkDomainsMask_exit;
    }
    boardObjGrpMaskInit_E32(pClkDomainsMask);
    status = vpstate3xClkDomainsMaskGet(pVpstate3x, pClkDomainsMask);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfLimitsVpstateIdxToClkDomainsMask_exit;
    }

perfLimitsVpstateIdxToClkDomainsMask_exit:
    return status;
}

/*!
 * @copydoc PerfLimitsVpstateIdxToFreqkHz
 */
FLCN_STATUS
perfLimitsVpstateIdxToFreqkHz
(
    LwU8            vpstateIdx,
    LwBool          bFloor,
    LwBool          bFallbackToPstateRange,
    PERF_LIMITS_VF *pVfDomain
)
{
    VPSTATE_3X              *pVpstate3x;
    VPSTATE_3X_CLOCK_ENTRY   clkEntry;
    CLK_DOMAIN              *pDomain;
    PERF_LIMITS_PSTATE_RANGE pstateRange;
    FLCN_STATUS              status = FLCN_OK;
    LwU8                     pstateIdx;

    // Check that VPSTATE idx is valid.
    if (!BOARDOBJGRP_IS_VALID(VPSTATE, vpstateIdx))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfLimitsVpstateIdxToFreqkHz_exit;
    }

    // Fetch the VPSTATE pointer from the BOARDOBJGRP.
    pVpstate3x = (VPSTATE_3X *)BOARDOBJGRP_OBJ_GET(VPSTATE, vpstateIdx);
    if (pVpstate3x == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto perfLimitsVpstateIdxToFreqkHz_exit;
    }

    // Set the PSTATE index from the VPSTATE.
    pstateIdx = PSTATE_GET_INDEX_BY_LEVEL(vpstate3xPstateIdxGet(pVpstate3x));
    if (pstateIdx == LW2080_CTRL_PERF_PSTATE_INDEX_ILWALID)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto perfLimitsVpstateIdxToFreqkHz_exit;
    }
    PERF_LIMITS_RANGE_SET_VALUE(&pstateRange, pstateIdx);

    pDomain = CLK_DOMAIN_GET(pVfDomain->idx);
    if (pDomain == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto perfLimitsVpstateIdxToFreqkHz_exit;
    }

    //
    // Ensure that the CLK_DOMAIN specified implements CLK_DOMAIN_3X_PROG so we
    // can do VF look-ups.
    //
    if (clkDomainIsFixed(pDomain))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfLimitsVpstateIdxToFreqkHz_exit;
    }

    // Query the VPSTATE for the CLK_DOMAIN's clock entry.
    status = vpstate3xClkDomainGet(pVpstate3x, pVfDomain->idx, &clkEntry);
    if (status == FLCN_OK)
    {
        pVfDomain->value = clkEntry.targetFreqMHz *
            clkDomainFreqkHzScaleGet(pDomain);
    }
    //
    // If the VPSTATE does not specify the CLK_DOMAIN frequency, can
    // fallback to PSTATE range, if requested.  This will happen
    // automatically per the call to @perfLimitsFreqkHzQuantize()
    // below.
    //
    else if ((status == FLCN_ERR_ILWALID_ARGUMENT) &&
             bFallbackToPstateRange)
    {
        status = FLCN_OK;
        pVfDomain->value = (bFloor) ? 0 : LW_U32_MAX;
    }
    //
    // The VPSTATE does not specify a frequency for this CLK_DOMAIN
    // and no PSTATE fallback requested, so return error back to caller.
    //
    else
    {
        PMU_BREAKPOINT();
        goto perfLimitsVpstateIdxToFreqkHz_exit;
    }

    //
    // Quantize the VPSTATE frequency to the PSTATE specified in the
    // VPSTATE, as well as to the current VF lwrve.
    //
    status = perfLimitsFreqkHzQuantize(pVfDomain,
                &pstateRange, bFloor);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfLimitsVpstateIdxToFreqkHz_exit;
    }

perfLimitsVpstateIdxToFreqkHz_exit:
    return status;
}

/*!
 * @copydoc PerfLimitsFreqkHzIsNoiseUnaware
 */
FLCN_STATUS
perfLimitsFreqkHzIsNoiseUnaware
(
    PERF_LIMITS_VF  *pVfDomain,
    LwBool          *pBNoiseUnaware
)
{
    CLK_DOMAIN         *pDomain;
    CLK_DOMAIN_PROG    *pDomainProg;
    LwU32               freqMHz;
    FLCN_STATUS         status  = FLCN_OK;

    pDomain = CLK_DOMAIN_GET(pVfDomain->idx);
    if (pDomain == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfLimitsFreqkHzIsNoiseUnaware_exit;
    }

    pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
    if (pDomainProg == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto perfLimitsFreqkHzIsNoiseUnaware_exit;
    }

    // Colwert kHz -> MHz.
    freqMHz = pVfDomain->value / clkDomainFreqkHzScaleGet(pDomain);

    // Query if the clock domain is noise-aware for the given freq.
    *pBNoiseUnaware = (!clkDomainProgIsFreqMHzNoiseAware(pDomainProg, freqMHz));

perfLimitsFreqkHzIsNoiseUnaware_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
