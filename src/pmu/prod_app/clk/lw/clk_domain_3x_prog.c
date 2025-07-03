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
#include "volt/objvolt.h"
#include "dmemovl.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * @copydoc ClkDomainProgMaxFreqMHzGet
 */
FLCN_STATUS
clkDomainProgMaxFreqMHzGet_3X
(
    CLK_DOMAIN_PROG        *pDomainProg,
    LwU32                  *pFreqMaxMHz
)
{
    CLK_DOMAIN_3X_PROG *pDomain3XProg =
        (CLK_DOMAIN_3X_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);

    //
    // Init to _NOT_SUPPORTED for case where no type-specific implemenation is
    // found below.
    //
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    // Call type-specific implemenation.
    switch (BOARDOBJ_GET_TYPE(pDomain3XProg))
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_PRIMARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY:
        {
            status = clkDomainProgMaxFreqMHzGet_3X_PRIMARY(pDomainProg,
                        pFreqMaxMHz);
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_SECONDARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY:
        {
            status = clkDomainProgMaxFreqMHzGet_3X_SECONDARY(pDomainProg,
                        pFreqMaxMHz);
            break;
        }
    }

    // Trap errors from type-specific implemenations.
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainProgMaxFreqMHzGet_3X_exit;
    }

clkDomainProgMaxFreqMHzGet_3X_exit:
    return status;
}

/*!
 * @copydoc ClkDomainProgFreqsIterate
 */
FLCN_STATUS
clkDomainProgFreqsIterate_3X
(
    CLK_DOMAIN_PROG     *pDomainProg,
    LwU16               *pFreqMHz
)
{
    CLK_DOMAIN_3X_PROG *pDomain3XProg =
        (CLK_DOMAIN_3X_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    CLK_PROG_3X        *pProg3X;
    FLCN_STATUS         status        = FLCN_ERR_NOT_SUPPORTED;
    LwU16               prevFreqMHz   = *pFreqMHz;
    LwU16               lwrrFreqMHz   = *pFreqMHz;
    LwU8                i;

    // Iterate through the set of CLK_PROG entries to find best matches.
    for (i = pDomain3XProg->clkProgIdxLast;
            i >= pDomain3XProg->clkProgIdxFirst; i--)
    {
        pProg3X = (CLK_PROG_3X *)BOARDOBJGRP_OBJ_GET(CLK_PROG, i);
        if (pProg3X == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkDomainProgFreqsIterate_3X_exit;
        }

        status  = clkProg3XFreqsIterate(pProg3X,
            (((CLK_DOMAIN *)(pDomain3XProg))->apiDomain), &lwrrFreqMHz);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomainProgFreqsIterate_3X_exit;
        }

        // Test the END of iteration.
        if (i == pDomain3XProg->clkProgIdxFirst)
        {
            if ((lwrrFreqMHz <= 0U) || ((*pFreqMHz) == lwrrFreqMHz))
            {
                status = FLCN_ERR_ITERATION_END;
                goto clkDomainProgFreqsIterate_3X_exit;
            }
            (*pFreqMHz) = lwrrFreqMHz;
            break;
        }
        // Test the first iteration.
        else if ((*pFreqMHz) == CLK_DOMAIN_3X_PROG_ITERATE_MAX_FREQ)
        {
            (*pFreqMHz) = lwrrFreqMHz;
            break;
        }
        // Make sure that we DO NOT go below the MAX freq of next prog index
        else if (lwrrFreqMHz < (*pFreqMHz))
        {
            //
            // Logic:
            // As we start with clkProgIdxLast, we need to get the correct prog
            // index whose range support the next lowest freq. This is covered
            // by the first condition.
            // Once we get the next lowest freq, we need to store that value in
            // ref@ prevFreqMHz.
            // Next step is to make sure that the lowest freq value is not out
            // of the range for given prog index. So we get the next prog index
            // MAX value and compare them with the stored freq value.
            //
            if (prevFreqMHz < lwrrFreqMHz)
            {
                (*pFreqMHz) = lwrrFreqMHz;
                break;
            }
            else if (prevFreqMHz < (*pFreqMHz))
            {
                (*pFreqMHz) = prevFreqMHz;
                break;
            }
            prevFreqMHz = lwrrFreqMHz;
        }
    }

clkDomainProgFreqsIterate_3X_exit:
    return status;
}

/*!
 * @copydoc ClkDomainProgFreqQuantize_3X
 */
FLCN_STATUS
clkDomainProgFreqQuantize_3X
(
    CLK_DOMAIN_PROG            *pDomainProg,
    LW2080_CTRL_CLK_VF_INPUT   *pInputFreq,
    LW2080_CTRL_CLK_VF_OUTPUT  *pOutputFreq,
    LwBool                      bFloor,
    LwBool                      bReqFreqDeltaAdj
)
{
    CLK_DOMAIN_3X_PROG *pDomain3XProg =
        (CLK_DOMAIN_3X_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    CLK_PROG_3X        *pProg3X;
    FLCN_STATUS         status        = FLCN_ERR_ILWALID_ARGUMENT;
    LwU8                i;

    //
    // Iterate over set of CLK_PROG_3X structures, to find the highest quantized
    // frequency.
    //
    for (i = pDomain3XProg->clkProgIdxFirst; i <= pDomain3XProg->clkProgIdxLast;
        i++)
    {
        pProg3X = (CLK_PROG_3X *)BOARDOBJGRP_OBJ_GET(CLK_PROG, i);
        if (pProg3X == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkDomainProgFreqQuantize_3X_exit;
        }

        // CLK_PROG frequency quantization interface for single input Frequency.
        status = clkProg3XFreqQuantize(pProg3X, pDomain3XProg,
                    pInputFreq, pOutputFreq, bFloor, bReqFreqDeltaAdj);
        if (FLCN_ERR_OUT_OF_RANGE == status)
        {
            // While flooring bail out early if clk prog is more than input freq
            if (bFloor)
            {
                break;
            }
            // Continue on ceiling request.
        }
        else if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomainProgFreqQuantize_3X_exit;
        }

        if (LW2080_CTRL_CLK_VF_VALUE_ILWALID != pOutputFreq->inputBestMatch)
        {
            // Match found. Jump to exit
            goto clkDomainProgFreqQuantize_3X_exit;
        }
    }

    //
    // Handle case where input freq > max Freq of last CLK_PROG_3X.
    // Set bestMatch to max frequency to stop searching.
    //
    if ((bFloor) &&
        (LW2080_CTRL_CLK_VF_VALUE_ILWALID == pOutputFreq->inputBestMatch) &&
        (pOutputFreq->value != LW2080_CTRL_CLK_VF_VALUE_ILWALID))
    {
        pOutputFreq->inputBestMatch = pOutputFreq->value;

        // Update status to OK.
        status = FLCN_OK;
    }
    //
    // If the default flag is set and no matching frequency found,
    // set default frequency values as output.
    //
    else if ((FLCN_ERR_OUT_OF_RANGE == status) &&
        (FLD_TEST_DRF(2080_CTRL_CLK, _VF_INPUT_FLAGS, _VF_POINT_SET_DEFAULT, _YES, pInputFreq->flags)) &&
        (LW2080_CTRL_CLK_VF_VALUE_ILWALID == pOutputFreq->inputBestMatch))
    {
        //
        // If the input frequency is below the min frequency supported by first
        // clock programming index and client requested bFloor, set the output
        // frequency as min frequency supported by first clock programming index
        // if the return error is @ref FLCN_ERR_OUT_OF_RANGE and clint requested
        // default values.
        //
        if (bFloor)
        {
            LwU16 freqMaxMHz = LW2080_CTRL_CLK_VF_VALUE_ILWALID;

            pProg3X = (CLK_PROG_3X *)BOARDOBJGRP_OBJ_GET(CLK_PROG,
                        pDomain3XProg->clkProgIdxFirst);
            if (pProg3X == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_INDEX;
                goto clkDomainProgFreqQuantize_3X_exit;
            }

            freqMaxMHz = pProg3X->freqMaxMHz;
            if (bReqFreqDeltaAdj)
            {
                // Adjust the frequency delta offset and quantize the result.
                status = clkProg3XFreqMaxAdjAndQuantize(pProg3X, pDomain3XProg,
                    &freqMaxMHz);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_STATE;
                    goto clkDomainProgFreqQuantize_3X_exit;
                }
            }
            pOutputFreq->inputBestMatch = freqMaxMHz;
            pOutputFreq->value          = freqMaxMHz;
        }
        //
        // If the input frequency is above the max frequency supported by last
        // clock programming index and client requested !bFloor, set the output
        // frequency as max frequency supported by last clock programming index,
        // if the return error is @ref FLCN_ERR_OUT_OF_RANGE and clint requested
        // default values.
        //
        else
        {
            // The CLK PROG interface always init the output value to its max.
            pOutputFreq->inputBestMatch = pOutputFreq->value;
        }

        // Update status to OK.
        status = FLCN_OK;
    }

clkDomainProgFreqQuantize_3X_exit:
    return status;
}

/*!
 * @copydoc ClkDomain3XProgFreqAdjustDeltaMHz
 */
FLCN_STATUS
clkDomain3XProgFreqAdjustDeltaMHz_SUPER
(
    CLK_DOMAIN_3X_PROG  *pDomain3XProg,
    LwU16               *pFreqMHz,
    LwBool               bQuantize,
    LwBool               bVFOCAdjReq
)
{
    // Offset by the CLK_DOMAIN_3X_PROG factory OC offset, if enabled
    if (clkDomain3XProgFactoryOCEnabled(pDomain3XProg))
    {
        *pFreqMHz = clkFreqDeltaAdjust(*pFreqMHz,
                        clkDomain3XProgFactoryDeltaGet(pDomain3XProg));
    }

    // Offset by the CLK_DOMAIN_3X_PROG domain offset.
    if (clkDomain3XProgIsOVOCEnabled(pDomain3XProg))
    {
        *pFreqMHz = clkFreqDeltaAdjust(*pFreqMHz,
                        clkDomain3XProgFreqDeltaGet(pDomain3XProg));
    }

    return FLCN_OK;
}

/*!
 * @copydoc ClkDomain3XProgFreqAdjustDeltaMHz
 */
FLCN_STATUS
clkDomain3XProgFreqAdjustDeltaMHz
(
    CLK_DOMAIN_3X_PROG  *pDomain3XProg,
    LwU16               *pFreqMHz,
    LwBool               bQuantize,
    LwBool               bVFOCAdjReq
)
{
    //
    // Init to _NOT_SUPPORTED for case where no type-specific implemenation is
    // found below.
    //
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    // Call type-specific implemenation.
    switch (BOARDOBJ_GET_TYPE(pDomain3XProg))
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_PRIMARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY:
        {
            status = clkDomain3XProgFreqAdjustDeltaMHz_PRIMARY(pDomain3XProg,
                        pFreqMHz, bQuantize, bVFOCAdjReq);
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_SECONDARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY:
        {
            status = clkDomain3XProgFreqAdjustDeltaMHz_SECONDARY(pDomain3XProg,
                        pFreqMHz, bQuantize, bVFOCAdjReq);
            break;
        }
    }

    // Trap errors from type-specific implemenations.
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomain3XProgFreqAdjustDeltaMHz_exit;
    }

clkDomain3XProgFreqAdjustDeltaMHz_exit:
    return status;
}

/*!
 * @copydoc ClkDomain3XProgFreqAdjustQuantizeMHz
 */
FLCN_STATUS
clkDomain3XProgFreqAdjustQuantizeMHz
(
    CLK_DOMAIN_3X_PROG *pDomain3XProg,
    LwU16              *pFreqMHz,
    LwBool              bFloor,
    LwBool              bReqFreqDeltaAdj
)
{
    LW2080_CTRL_CLK_VF_INPUT    inputFreq;
    LW2080_CTRL_CLK_VF_OUTPUT   outputFreq;
    FLCN_STATUS                 status;

    LW2080_CTRL_CLK_VF_INPUT_INIT(&inputFreq);
    LW2080_CTRL_CLK_VF_OUTPUT_INIT(&outputFreq);

    inputFreq.flags = FLD_SET_DRF(2080_CTRL_CLK, _VF_INPUT_FLAGS,
                            _VF_POINT_SET_DEFAULT, _YES, inputFreq.flags);
    inputFreq.value = *pFreqMHz;

    status = clkDomainProgFreqQuantize_3X(
        CLK_DOMAIN_PROG_GET_FROM_3X_PROG(pDomain3XProg), &inputFreq,
        &outputFreq, bFloor, bReqFreqDeltaAdj);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomain3XProgFreqAdjustQuantizeMHz_exit;
    }

    if (LW2080_CTRL_CLK_VF_VALUE_ILWALID == outputFreq.inputBestMatch)
    {
        status = FLCN_ERR_OUT_OF_RANGE;
        PMU_BREAKPOINT();
        goto clkDomain3XProgFreqAdjustQuantizeMHz_exit;
    }

    *pFreqMHz = (LwU16)(outputFreq.value);

clkDomain3XProgFreqAdjustQuantizeMHz_exit:
    return status;
}

/*!
 * @copydoc ClkDomain3XProgVoltAdjustDeltauV
 */
FLCN_STATUS
clkDomain3XProgVoltAdjustDeltauV
(
    CLK_DOMAIN_3X_PROG  *pDomain3XProg,
    CLK_PROG_3X_PRIMARY  *pProg3XPrimary,
    LwU8                 voltRailIdx,
    LwU32               *pVoltuV
)
{
    LwU32       voltOriguV = *pVoltuV;
    FLCN_STATUS status     = FLCN_OK;
    VOLT_RAIL  *pVoltRail;

    // Call into type-specific implementations.
    switch (BOARDOBJ_GET_TYPE(pDomain3XProg))
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_PRIMARY:
        PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_CLK_CLK_DOMAIN_30);
        {
            status = clkDomain3XProgVoltAdjustDeltauV_PRIMARY(pDomain3XProg,
                        pProg3XPrimary, voltRailIdx, pVoltuV);
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_SECONDARY:
        PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_CLK_CLK_DOMAIN_30);
        {
            status = clkDomain3XProgVoltAdjustDeltauV_SECONDARY(pDomain3XProg,
                        pProg3XPrimary, voltRailIdx, pVoltuV);
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY:
        PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_CLK_CLK_DOMAIN_EXTENDED_35);
        {
            status = clkDomain3XProgVoltAdjustDeltauV_35_PRIMARY(pDomain3XProg,
                        pProg3XPrimary, voltRailIdx, pVoltuV);
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY:
        PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_CLK_CLK_DOMAIN_EXTENDED_35);
        {
            status = clkDomain3XProgVoltAdjustDeltauV_35_SECONDARY(pDomain3XProg,
                        pProg3XPrimary, voltRailIdx, pVoltuV);
            break;
        }
        default:
        {
            status = FLCN_ERR_NOT_SUPPORTED;
            PMU_BREAKPOINT();
            break;
        }
    }

    // Catch errors from type-specific implementations.
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomain3XProgVoltAdjustDeltauV_exit;
    }

    //
    // If voltage offset has been applied to this voltage rail, round up to the
    // next regulator step size.
    //
    if (voltOriguV != *pVoltuV)
    {
        pVoltRail = VOLT_RAIL_GET(voltRailIdx);
        if (pVoltRail == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkDomain3XProgVoltAdjustDeltauV_exit;
        }

        status = voltRailRoundVoltage(pVoltRail,
                    (LwS32*)pVoltuV, LW_TRUE, LW_TRUE);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomain3XProgVoltAdjustDeltauV_exit;
        }
    }

clkDomain3XProgVoltAdjustDeltauV_exit:
    return status;
}

/*!
 * @copydoc ClkDomain3XProgGetClkProgIdxFromFreq
 */
LwU8
clkDomain3XProgGetClkProgIdxFromFreq
(
    CLK_DOMAIN_3X_PROG *pDomain3XProg,
    LwU32               freqMHz,
    LwBool              bReqFreqDeltaAdj
)
{
    CLK_PROG_3X    *pProg3X;
    LwU8            progIdx;
    LwU8            i;

    // Iterate through the set of CLK_PROG entries to find best matches.
    for (i = pDomain3XProg->clkProgIdxFirst;
            i <= pDomain3XProg->clkProgIdxLast; i++)
    {
        pProg3X = (CLK_PROG_3X *)BOARDOBJGRP_OBJ_GET(CLK_PROG, i);
        if (pProg3X == NULL)
        {
            PMU_BREAKPOINT();
            progIdx = LW2080_CTRL_CLK_CLK_PROG_IDX_ILWALID;
            goto clkDomain3XProgGetClkProgIdxFromFreq_exit;
        }
        // Call into this CLK_PROG to check if this can be used to set i/p freq.
        progIdx  = clkProg3XGetClkProgIdxFromFreq(pProg3X,
                    pDomain3XProg, freqMHz, bReqFreqDeltaAdj);
        if (progIdx != LW2080_CTRL_CLK_CLK_PROG_IDX_ILWALID)
        {
            goto clkDomain3XProgGetClkProgIdxFromFreq_exit;
        }
    }

    // No matching programming index found. default = last prog index
    progIdx = pDomain3XProg->clkProgIdxLast;

clkDomain3XProgGetClkProgIdxFromFreq_exit:
    return progIdx;
}

/*!
 * @copydoc ClkDomainGetSource
 */
LwU8
clkDomainGetSource_3X_PROG
(
    CLK_DOMAIN *pDomain,
    LwU32       freqMHz
)
{
    CLK_DOMAIN_3X_PROG *p3xProg = (CLK_DOMAIN_3X_PROG *)pDomain;
    LwU8                progIdx;
    LwU8                source  = LW2080_CTRL_CLK_PROG_1X_SOURCE_ILWALID;
    CLK_PROG_3X        *pProg3X;

    // Call type-specific implementation.
    progIdx = clkDomain3XProgGetClkProgIdxFromFreq(p3xProg, freqMHz, LW_TRUE);

    pProg3X = (CLK_PROG_3X *)BOARDOBJGRP_OBJ_GET(CLK_PROG, progIdx);
    if (pProg3X == NULL)
    {
        PMU_BREAKPOINT();
        goto clkDomainGetSource_3X_PROG_exit;
    }

    source = clkProg3XFreqSourceGet(pProg3X);

clkDomainGetSource_3X_PROG_exit:
    return source;
}

/*!
 * @copydoc ClkDomainProgClientFreqDeltaAdj_3X
 */
FLCN_STATUS
clkDomainProgClientFreqDeltaAdj_3X
(
    CLK_DOMAIN_PROG     *pDomainProg,
    LwU16               *pFreqMHz
)
{
    CLK_DOMAIN_3X_PROG *pDomain3XProg =
        (CLK_DOMAIN_3X_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);

    //
    // Init to _NOT_SUPPORTED for case where no type-specific implemenation is
    // found below.
    //
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    // Short-circuit if the OC/OV is disabled.
    if (!clkDomain3XProgIsOVOCEnabled(pDomain3XProg))
    {
        status = FLCN_OK;
        goto clkDomainProgClientFreqDeltaAdj_3X_exit;
    }

    // Call type-specific implemenation.
    switch (BOARDOBJ_GET_TYPE(pDomain3XProg))
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY:
        PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_CLK_CLK_DOMAIN_EXTENDED_35);
        {
            status = clkDomainProgClientFreqDeltaAdj_35_PRIMARY(pDomainProg,
                        pFreqMHz);
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY:
        PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_CLK_CLK_DOMAIN_EXTENDED_35);
        {
            status = clkDomainProgClientFreqDeltaAdj_35_SECONDARY(pDomainProg,
                        pFreqMHz);
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_PRIMARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_SECONDARY:
        PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_CLK_CLK_DOMAIN_30);
        {
            status = clkDomainProgClientFreqDeltaAdj_30(pDomainProg,
                        pFreqMHz);
            break;
        }
    }

    // Trap errors from type-specific implemenations.
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainProgClientFreqDeltaAdj_3X_exit;
    }

clkDomainProgClientFreqDeltaAdj_3X_exit:
    return status;
}

/*!
 * @copydoc ClkDomainProgFactoryDeltaAdj
 */
FLCN_STATUS
clkDomainProgFactoryDeltaAdj_3X
(
    CLK_DOMAIN_PROG     *pDomainProg,
    LwU16               *pFreqMHz
)
{
    CLK_DOMAIN_3X_PROG *pDomain3XProg =
        (CLK_DOMAIN_3X_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    CLK_PROG_3X_PRIMARY *pProg3XPrimary = NULL;
    CLK_PROG_3X        *pProg3X       = NULL;
    LwU16               cachedFreqMHz = *pFreqMHz;
    LwU8                progIdx;
    FLCN_STATUS         status        = FLCN_OK;

    // Short-circuit if the OC/OV is disabled at clock domain level.
    if ((!clkDomain3XProgIsOVOCEnabled(pDomain3XProg)) ||
        (!clkDomain3XProgFactoryOCEnabled(pDomain3XProg)))
    {
        status = FLCN_OK;
        goto clkDomainProgFactoryDeltaAdj_3X_exit;
    }

    if ((BOARDOBJ_GET_TYPE(pDomain3XProg) == LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY) ||
        (BOARDOBJ_GET_TYPE(pDomain3XProg) == LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_SECONDARY))
    {
        //
        // If you hit this error, POR enabled factory offset on secondary clock.
        // Time to implement the code to find the primary clock programming entry
        // for the input secondary frequency and ensure that it supports the OC/OV.
        //
        PMU_BREAKPOINT();
        status = FLCN_ERR_NOT_SUPPORTED;
        goto clkDomainProgFactoryDeltaAdj_3X_exit;
    }

    // Get the clock programming index from input base frequency.
    progIdx = clkDomain3XProgGetClkProgIdxFromFreq(pDomain3XProg, *pFreqMHz, LW_FALSE);

    pProg3X = (CLK_PROG_3X *)BOARDOBJGRP_OBJ_GET(CLK_PROG, progIdx);
    if (pProg3X == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto clkDomainProgFactoryDeltaAdj_3X_exit;
    }
    pProg3XPrimary = CLK_PROG_BOARDOBJ_TO_INTERFACE_CAST(pProg3X, 3X_PRIMARY);
    if (pProg3XPrimary == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto clkDomainProgFactoryDeltaAdj_3X_exit;
    }

    // Short-circuit if the OC/OV is disabled at clock programming level.
    if ((!clkProg3XPrimaryOVOCEnabled(pProg3XPrimary)))
    {
        status = FLCN_OK;
        goto clkDomainProgFactoryDeltaAdj_3X_exit;
    }

    // Adjust the input frequency with factory OC offset.
    *pFreqMHz = clkFreqDeltaAdjust(*pFreqMHz,
                    clkDomain3XProgFactoryDeltaGet(pDomain3XProg));

    // Quantize the frequency if required.
    if (*pFreqMHz != cachedFreqMHz)
    {
        status = clkProg3XFreqMHzQuantize(
                    pProg3X,                    // pProg3X
                    pDomain3XProg,              // pDomain3XProg
                    pFreqMHz,                   // pFreqMHz
                    LW_TRUE);                   // bFloor
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomainProgFactoryDeltaAdj_3X_exit;
        }
    }

clkDomainProgFactoryDeltaAdj_3X_exit:
    return status;
}

/*!
 * @copydoc ClkDomainProgIsFreqMHzNoiseAware
 */
LwBool
clkDomainProgIsFreqMHzNoiseAware_3X
(
    CLK_DOMAIN_PROG    *pDomainProg,
    LwU32               freqMHz
)
{
    CLK_DOMAIN_3X_PROG *pDomain3XProg =
        (CLK_DOMAIN_3X_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    LwU8                progIdx;
    LwBool              bNoiseAware   = LW_FALSE;
    CLK_PROG_3X        *pProg3X;

    // Call type-specific implemenation.
    progIdx = clkDomain3XProgGetClkProgIdxFromFreq(pDomain3XProg, freqMHz, LW_TRUE);

    pProg3X = (CLK_PROG_3X *)BOARDOBJGRP_OBJ_GET(CLK_PROG, progIdx);
    if (pProg3X == NULL)
    {
        PMU_BREAKPOINT();
        goto clkDomainProgIsFreqMHzNoiseAware_3X_exit;
    }

    bNoiseAware =
        (LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL == clkProg3XFreqSourceGet(pProg3X));

clkDomainProgIsFreqMHzNoiseAware_3X_exit:
    return bNoiseAware;
}

/*!
 * @copydoc ClkDomain3XProgIsOVOCEnabled
 */
LwBool
clkDomain3XProgIsOVOCEnabled
(
    CLK_DOMAIN_3X_PROG  *pDomain3XProg
)
{
    // Call type-specific implemenation.
    switch (BOARDOBJ_GET_TYPE(pDomain3XProg))
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_PRIMARY:
        {
            return clkDomain3XProgIsOVOCEnabled_3X_PRIMARY(pDomain3XProg);
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_SECONDARY:
        {
            return clkDomain3XProgIsOVOCEnabled_3X_SECONDARY(pDomain3XProg);
        }
    }

    PMU_BREAKPOINT();

    return LW_FALSE;
}

/* ------------------------- Private Functions ------------------------------ */
