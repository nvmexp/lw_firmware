/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file clk_prog_3x_primary.c
 *
 * @brief Module managing all state related to the CLK_PROG_3X_PRIMARY structure.
 * This structure defines how to program a given frequency on given PRIMARY
 * CLK_DOMAIN - a CLK_DOMAIN which has its own specified VF lwrve.
 *
 */
//! https://wiki.lwpu.com/engwiki/index.php/Resman/PState/Data_Tables/Clock_Programming_Table/1.0_Spec#Primary

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "volt/objvolt.h"
#include "dmemovl.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * @copydoc ClkProg3XPrimaryVoltToFreqBinarySearch
 */
FLCN_STATUS
clkProg3XPrimaryVoltToFreqBinarySearch
(
    CLK_PROG_3X_PRIMARY                 *pProg3XPrimary,
    CLK_DOMAIN_3X_PRIMARY               *pDomain3XPrimary,
    LwU8                                clkDomIdx,
    LwU8                                voltRailIdx,
    LwU8                                voltageType,
    LW2080_CTRL_CLK_VF_INPUT           *pInput,
    LW2080_CTRL_CLK_VF_OUTPUT          *pOutput
)
{
    CLK_PROG_3X             *pProg3X =
        (CLK_PROG_3X *)INTERFACE_TO_BOARDOBJ_CAST(pProg3XPrimary);
    CLK_VF_POINT            *pVfPoint;
    LW2080_CTRL_CLK_VF_PAIR  vfPair  = LW2080_CTRL_CLK_VF_PAIR_INIT();
    LwBool          bValidMatchFound     = LW_FALSE;
    FLCN_STATUS     status   = FLCN_OK;
    LwBoardObjIdx   midIdx   = LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID;
    LwBoardObjIdx   startIdx = pProg3XPrimary->pVfEntries[voltRailIdx].vfPointIdxFirst;
    LwBoardObjIdx   endIdx   = pProg3XPrimary->pVfEntries[voltRailIdx].vfPointIdxLast;

    //
    // Only CLK_PROG entries with source == _NAFLL support _SOURCE voltage type.
    // Skip entries which aren't NAFLL.
    //
    if ((LW2080_CTRL_CLK_VOLTAGE_TYPE_SOURCE == voltageType) &&
        (LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL !=
            clkProg3XFreqSourceGet(pProg3X)))
    {
        goto clkProg3XPrimaryVoltToFreqBinarySearch_exit;
    }

    //
    // Binary search through set of CLK_VF_POINTs for this CLK_PROG_3X_PRIMARY
    // to find best matches.
    // NOTE: Below code only works for the case where the VF lwrve is
    // monotonically increasing F -> V and this is enforced in RM/PMU
    //
    do
    {
        //
        // We want to round up to the higher index if (startIdx + endIdx)
        // is not exactly divisible by 2. This is to ensure that we always
        // test the highest index.
        //
        midIdx   = LW_UNSIGNED_ROUNDED_DIV((startIdx + endIdx), 2U);

        pVfPoint = CLK_VF_POINT_GET(PRI, midIdx);
        if (pVfPoint == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkProg3XPrimaryVoltToFreqBinarySearch_exit;
        }

        //
        // Retrieve the voltage and frequency for this CLK_VF_POINT via the
        // helper accessor.
        //
        status = clkVfPointAccessor(
                        pVfPoint,        // pVfPoint
                        pProg3XPrimary,   // pProg3XPrimary
                        pDomain3XPrimary, // pDomain3XPrimary
                        clkDomIdx,       // clkDomIdx
                        voltRailIdx,     // voltRailIdx
                        voltageType,     // voltageType
                        LW_TRUE,         // bOffseted
                        &vfPair);        // pVfPair
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkProg3XPrimaryVoltToFreqBinarySearch_exit;
        }

        // If the exact crossover point found, break out.
        if ((vfPair.voltageuV == pInput->value) ||
            (endIdx == startIdx))
        {
            LW2080_CTRL_CLK_VF_PAIR vfPairNext = LW2080_CTRL_CLK_VF_PAIR_INIT();

            if (midIdx == pProg3XPrimary->pVfEntries[voltRailIdx].vfPointIdxLast)
            {
                //
                // if the exact match found, update the flag. Do not terminate
                // the algorithm here as we observed that multiple clock progs
                // could have exact same voltage requirements. Also if the input 
                // voltage is higher then the voltage of last VF point consider
                // last VF point as valid match. Must terminate the algorithm only
                // when the cross over point is found.
                //
                if (vfPair.voltageuV <= pInput->value)
                {
                    bValidMatchFound = LW_TRUE;
                }

                // Always break as this is end of search.
                break;
            }
            else if ((midIdx ==
                        pProg3XPrimary->pVfEntries[voltRailIdx].vfPointIdxFirst) &&
                     (vfPair.voltageuV > pInput->value))
            {
                // Input voltage is outside of the VF lwrve, terminate the algorithm.
                status = FLCN_ERR_ITERATION_END;
                break;
            }

            //
            // check next VF point to ensure we found the crossover point
            // as we want Fmax @ V.
            //
            pVfPoint = CLK_VF_POINT_GET(PRI, (midIdx + 1U));
            if (pVfPoint == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_INDEX;
                goto clkProg3XPrimaryVoltToFreqBinarySearch_exit;
            }

            //
            // Retrieve the voltage and frequency for this CLK_VF_POINT via the
            // helper accessor.
            //
            status = clkVfPointAccessor(
                            pVfPoint,        // pVfPoint
                            pProg3XPrimary,   // pProg3XPrimary
                            pDomain3XPrimary, // pDomain3XPrimary
                            clkDomIdx,       // clkDomIdx
                            voltRailIdx,     // voltRailIdx
                            voltageType,     // voltageType
                            LW_TRUE,         // bOffseted
                            &vfPairNext);    // pVfPair
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkProg3XPrimaryVoltToFreqBinarySearch_exit;
            }

            if (vfPairNext.voltageuV > pInput->value)
            {
                //
                // If we reached end of search with two closest points,
                // this algorithm will select the lower VF point to close
                // the while loop. As we are looking for Fmax @ V, this
                // is the correct VF point.
                //
                bValidMatchFound = LW_TRUE;
                status = FLCN_ERR_ITERATION_END;
                break;
            }

            // Break if we reached the end of search.
            if (endIdx == startIdx)
            {
                break;
            }
        }

        //
        // If not found, update the indexes.
        // In case of V -> F we are looking for Fmax @ V, so exact
        // match will be consider below if it is not also a crossover
        // VF point.
        //
        if (vfPair.voltageuV <= pInput->value)
        {
            startIdx = midIdx;
        }
        else
        {
            endIdx = (midIdx - 1U);
        }
    } while (LW_TRUE);

    if (bValidMatchFound)
    {
        pOutput->inputBestMatch = vfPair.voltageuV;
        pOutput->value          = vfPair.freqMHz;
    }
    //
    // If the default flag is set, always update it.
    // If there is a better match it will be overwritten in the
    // next block.
    //
    else if ((FLD_TEST_DRF(2080_CTRL_CLK, _VF_INPUT_FLAGS, _VF_POINT_SET_DEFAULT, _YES,
                pInput->flags)) &&
        (pOutput->inputBestMatch == LW2080_CTRL_CLK_VF_VALUE_ILWALID) &&
        (pOutput->value == LW2080_CTRL_CLK_VF_VALUE_ILWALID))
    {
        pOutput->inputBestMatch = vfPair.voltageuV;
        pOutput->value          = vfPair.freqMHz;
    }

clkProg3XPrimaryVoltToFreqBinarySearch_exit:
    return status;
}

/*!
 * @copydoc ClkProg3XPrimaryFreqToVoltBinarySearch
 */
FLCN_STATUS
clkProg3XPrimaryFreqToVoltBinarySearch
(
    CLK_PROG_3X_PRIMARY                 *pProg3XPrimary,
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
    CLK_PROG_3X            *pProg3X =
        (CLK_PROG_3X *)INTERFACE_TO_BOARDOBJ_CAST(pProg3XPrimary);
    CLK_VF_POINT            *pVfPoint;
    LW2080_CTRL_CLK_VF_PAIR  vfPair  = LW2080_CTRL_CLK_VF_PAIR_INIT();
    LwBool          bValidMatchFound = LW_FALSE;
    FLCN_STATUS     status           = FLCN_OK;
    LwBoardObjIdx   midIdx   = LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID;
    LwBoardObjIdx   startIdx = pProg3XPrimary->pVfEntries[voltRailIdx].vfPointIdxFirst;
    LwBoardObjIdx   endIdx   = pProg3XPrimary->pVfEntries[voltRailIdx].vfPointIdxLast;

    //
    // Only CLK_PROG entries with source == _NAFLL support _SOURCE voltage type.
    // Skip entries which aren't NAFLL.
    //
    if ((LW2080_CTRL_CLK_VOLTAGE_TYPE_SOURCE == voltageType) &&
        (LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL !=
             clkProg3XFreqSourceGet(pProg3X)))
    {
        goto clkProg3XPrimaryFreqToVoltBinarySearch_exit;
    }

    //
    // Binary search through set of CLK_VF_POINTs for this CLK_PROG_3X_PRIMARY
    // to find best matches.
    // NOTE: Below code only works for the case where the VF lwrve is
    // monotonically increasing F -> V and this is enforced in RM/PMU
    //
    do
    {
        //
        // We want to round up to the higher index if (startIdx + endIdx)
        // is not exactly divisible by 2 as we are looking for Vmin @ F.
        //
        midIdx   = LW_UNSIGNED_ROUNDED_DIV((startIdx + endIdx), 2U);

        pVfPoint = CLK_VF_POINT_GET(PRI, midIdx);
        if (pVfPoint == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkProg3XPrimaryFreqToVoltBinarySearch_exit;
        }

        //
        // Retrieve the voltage and frequency for this CLK_VF_POINT via the
        // helper accessor.
        //
        status = clkVfPointAccessor(
                        pVfPoint,        // pVfPoint
                        pProg3XPrimary,   // pProg3XPrimary
                        pDomain3XPrimary, // pDomain3XPrimary
                        clkDomIdx,       // clkDomIdx
                        voltRailIdx,     // voltRailIdx
                        voltageType,     // voltageType
                        LW_TRUE,         // bOffseted
                        &vfPair);        // pVfPair
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkProg3XPrimaryFreqToVoltBinarySearch_exit;
        }

        // If the exact crossover point found, break out.
        if ((vfPair.freqMHz == pInput->value) ||
            (endIdx == startIdx))
        {
            LW2080_CTRL_CLK_VF_PAIR vfPairPrev = LW2080_CTRL_CLK_VF_PAIR_INIT();

            if (midIdx == pProg3XPrimary->pVfEntries[voltRailIdx].vfPointIdxFirst)
            {
                //
                // If the input frequency is below the frequency of first VF point,
                // consider first VF point as valid match.
                //
                if (vfPair.freqMHz >= pInput->value)
                {
                    bValidMatchFound = LW_TRUE;

                    // Terminate the algorithm here as we are looking for Vmin.
                    status = FLCN_ERR_ITERATION_END;
                }

                // Always break as we reached the end of search.
                break;
            }
            else if ((midIdx ==
                        pProg3XPrimary->pVfEntries[voltRailIdx].vfPointIdxLast) &&
                     (vfPair.freqMHz < pInput->value))
            {
                // Input frequency is outside of the VF lwrve check next clk prog.
                break;
            }

            //
            // In case of crossover, the midIdx MUST point to higher
            // frequency then input as we are doing round up in our
            // search algorithm. So if prev F point points to lower
            // frequency, we can consider this VF point as valid match
            // for Vmin @ F
            //
            pVfPoint = CLK_VF_POINT_GET(PRI, (midIdx - 1U));
            if (pVfPoint == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_INDEX;
                goto clkProg3XPrimaryFreqToVoltBinarySearch_exit;
            }

            //
            // Retrieve the voltage and frequency for this CLK_VF_POINT via the
            // helper accessor.
            //
            status = clkVfPointAccessor(
                            pVfPoint,        // pVfPoint
                            pProg3XPrimary,   // pProg3XPrimary
                            pDomain3XPrimary, // pDomain3XPrimary
                            clkDomIdx,       // clkDomIdx
                            voltRailIdx,     // voltRailIdx
                            voltageType,     // voltageType
                            LW_TRUE,         // bOffseted
                            &vfPairPrev);    // pVfPair
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkProg3XPrimaryFreqToVoltBinarySearch_exit;
            }

            if (vfPairPrev.freqMHz < pInput->value)
            {
                //
                // If we reached end of search with two closest points,
                // this algorithm will select the lower VF point to close
                // the while loop. As we are looking for Vmin @ F, we
                // must pick the higher VF point in this case.
                //
                if (vfPair.freqMHz < pInput->value)
                {
                    pVfPoint = CLK_VF_POINT_GET(PRI, (midIdx + 1U));
                    if (pVfPoint == NULL)
                    {
                        PMU_BREAKPOINT();
                        status = FLCN_ERR_ILWALID_INDEX;
                        goto clkProg3XPrimaryFreqToVoltBinarySearch_exit;
                    }

                    status = clkVfPointAccessor(
                                    pVfPoint,        // pVfPoint
                                    pProg3XPrimary,   // pProg3XPrimary
                                    pDomain3XPrimary, // pDomain3XPrimary
                                    clkDomIdx,       // clkDomIdx
                                    voltRailIdx,     // voltRailIdx
                                    voltageType,     // voltageType
                                    LW_TRUE,         // bOffseted
                                    &vfPair);        // pVfPair
                    if (status != FLCN_OK)
                    {
                        PMU_BREAKPOINT();
                        goto clkProg3XPrimaryFreqToVoltBinarySearch_exit;
                    }
                }

                bValidMatchFound = LW_TRUE;
                status = FLCN_ERR_ITERATION_END;
                break;
            }

            // Break if we reached the end of search.
            if (endIdx == startIdx)
            {
                break;
            }
        }

        //
        // If not found, update the indexes.
        // In case of F -> V we are looking for Vmin @ F, so exact
        // match will be consider above if it is not also a crossover
        // VF point.
        //
        if (vfPair.freqMHz < pInput->value)
        {
            startIdx = midIdx;
        }
        else
        {
            endIdx = (midIdx - 1U);
        }
    } while (LW_TRUE);


    // If the exact match found, short-circuit the search.
    if (bValidMatchFound)
    {
        pOutput->inputBestMatch = vfPair.freqMHz;
        pOutput->value          = vfPair.voltageuV;
    }
    //
    // If the default flag is set, always update it.
    // If there is a better match it will be overwritten in the
    // next block.
    //
    else if (FLD_TEST_DRF(2080_CTRL_CLK, _VF_INPUT_FLAGS, _VF_POINT_SET_DEFAULT, _YES, pInput->flags) &&
            LW2080_CTRL_CLK_VF_VALUE_ILWALID == pOutput->inputBestMatch &&
            LW2080_CTRL_CLK_VF_VALUE_ILWALID == pOutput->value &&
            BOARDOBJ_GET_GRP_IDX_8BIT(&pProg3X->super.super.super) == clkDomain3xProgLastIdxGet(pDomain3XProg) &&
            (midIdx  == pProg3XPrimary->pVfEntries[voltRailIdx].vfPointIdxLast))
    {
        //
        // For Freq -> Volt default value is the last VF point of the
        // last clock domain.
        //
        pOutput->inputBestMatch = vfPair.freqMHz;
        pOutput->value          = vfPair.voltageuV;
    }

clkProg3XPrimaryFreqToVoltBinarySearch_exit:
    return status;
}

/*!
 * @copydoc ClkProg3XPrimaryMaxFreqMHzGet
 */
FLCN_STATUS
clkProg3XPrimaryMaxFreqMHzGet
(
    CLK_PROG_3X_PRIMARY       *pProg3XPrimary,
    CLK_DOMAIN_3X_PRIMARY     *pDomain3XPrimary,
    LwU8                      clkDomIdx,
    LwU32                    *pFreqMaxMHz,
    LW2080_CTRL_CLK_VF_PAIR  *pVfPair
)
{
    VOLT_RAIL      *pRail;
    CLK_VF_POINT   *pVfPoint;
    LwBoardObjIdx   i;
    FLCN_STATUS     status   = FLCN_OK;

    if (pFreqMaxMHz == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }
    *pFreqMaxMHz = LW_U32_MAX;

    // Get the per volt rail MAX freq value.
    BOARDOBJGRP_ITERATOR_BEGIN(VOLT_RAIL, pRail, i, NULL)
    {
        pVfPoint = CLK_VF_POINT_GET(PRI,
                        pProg3XPrimary->pVfEntries[i].vfPointIdxLast);
        if (pVfPoint == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkProg3XPrimaryMaxFreqMHzGet_exit;
        }

        //
        // Call into accessor to retrieve the frequency, with accounting for the
        // various deltas and offsets.
        //
        status = clkVfPointAccessor(
                        pVfPoint,                         // pVfPoint
                        pProg3XPrimary,                    // pProg3XPrimary
                        pDomain3XPrimary,                  // pDomain3XPrimary
                        clkDomIdx,                        // clkDomIdx
                        BOARDOBJ_GRP_IDX_TO_8BIT(i),      // voltRailIdx
                        LW2080_CTRL_CLK_VOLTAGE_TYPE_POR, // voltageType
                        LW_TRUE,                          // bOffseted
                        pVfPair);                         // pVfPair
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkProg3XPrimaryMaxFreqMHzGet_exit;
        }

        *pFreqMaxMHz = LW_MIN((*pFreqMaxMHz), (LwU32)pVfPair->freqMHz);
    }
    BOARDOBJGRP_ITERATOR_END;

clkProg3XPrimaryMaxFreqMHzGet_exit:
    return status;
}

/*!
 * @copydoc ClkProg3XPrimaryFreqTranslateSecondaryToPrimary
 */
FLCN_STATUS
clkProg3XPrimaryFreqTranslateSecondaryToPrimary
(
    CLK_PROG_3X_PRIMARY    *pProg3XPrimary,
    CLK_DOMAIN_3X_PRIMARY  *pDomain3XPrimary,
    LwU8                   clkDomIdx,
    LwU16                 *pFreqMHz
)
{
    CLK_DOMAIN_3X_PROG *pDomain3XProg = (CLK_DOMAIN_3X_PROG *)
        INTERFACE_TO_BOARDOBJ_CAST(pDomain3XPrimary);

    //  No translation is required for primary clock domain, return FLCN_OK
    if (BOARDOBJ_GET_GRP_IDX_8BIT(&pDomain3XProg->super.super.super.super) == clkDomIdx)
    {
        return FLCN_OK;
    }

    // Otherwise, do type-specific translation.
    switch (BOARDOBJ_GET_TYPE(INTERFACE_TO_BOARDOBJ_CAST(pProg3XPrimary)))
    {
        case LW2080_CTRL_CLK_CLK_PROG_TYPE_30_PRIMARY_RATIO:
        case LW2080_CTRL_CLK_CLK_PROG_TYPE_35_PRIMARY_RATIO:
            return clkProg3XPrimaryFreqTranslateSecondaryToPrimary_RATIO(pProg3XPrimary,
                        pDomain3XPrimary, clkDomIdx, pFreqMHz);
        case LW2080_CTRL_CLK_CLK_PROG_TYPE_30_PRIMARY_TABLE:
        case LW2080_CTRL_CLK_CLK_PROG_TYPE_35_PRIMARY_TABLE:
            return clkProg3XPrimaryFreqTranslateSecondaryToPrimary_TABLE(pProg3XPrimary,
                        pDomain3XPrimary, clkDomIdx, pFreqMHz);
    }

    // No type-specific implementation found.  Error!
    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @copydoc ClkProg3XPrimaryIsPrimaryProgIdxForSecondaryFreqMHz
 */
LwBool
clkProg3XPrimaryIsPrimaryProgIdxForSecondaryFreqMHz
(
    CLK_PROG_3X_PRIMARY    *pProg3XPrimary,
    CLK_DOMAIN_3X_PRIMARY  *pDomain3XPrimary,
    LwU8                   secondaryClkDomIdx,
    LwU16                  freqMHz
)
{
    // Do type-specific translation.
    switch (BOARDOBJ_GET_TYPE(INTERFACE_TO_BOARDOBJ_CAST(pProg3XPrimary)))
    {
        case LW2080_CTRL_CLK_CLK_PROG_TYPE_30_PRIMARY_RATIO:
        case LW2080_CTRL_CLK_CLK_PROG_TYPE_35_PRIMARY_RATIO:
            return clkProg3XPrimaryIsPrimaryProgIdxForSecondaryFreqMHz_RATIO(pProg3XPrimary,
                        pDomain3XPrimary, secondaryClkDomIdx, freqMHz);
        case LW2080_CTRL_CLK_CLK_PROG_TYPE_30_PRIMARY_TABLE:
        case LW2080_CTRL_CLK_CLK_PROG_TYPE_35_PRIMARY_TABLE:
            return clkProg3XPrimaryIsPrimaryProgIdxForSecondaryFreqMHz_TABLE(pProg3XPrimary,
                        pDomain3XPrimary, secondaryClkDomIdx, freqMHz);
    }

    // No type-specific implementation found.  Error!
    PMU_BREAKPOINT();
    return LW_FALSE;
}

/* ------------------------- Private Functions ------------------------------ */
