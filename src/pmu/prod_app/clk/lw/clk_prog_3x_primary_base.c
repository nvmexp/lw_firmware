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
 * CLK_PROG_3X_PRIMARY class constructor.
 *
 * @copydoc BoardObjInterfaceConstruct
 */
FLCN_STATUS
clkProgConstruct_3X_PRIMARY
(
    BOARDOBJGRP                *pBObjGrp,
    BOARDOBJ_INTERFACE         *pInterface,
    RM_PMU_BOARDOBJ_INTERFACE  *pInterfaceDesc,
    INTERFACE_VIRTUAL_TABLE    *pVirtualTable
)
{
    RM_PMU_CLK_CLK_PROG_3X_PRIMARY_BOARDOBJ_SET *p3xPrimarySet =
        (RM_PMU_CLK_CLK_PROG_3X_PRIMARY_BOARDOBJ_SET *)pInterfaceDesc;
    CLK_PROG_3X_PRIMARY *p3xPrimary;
    FLCN_STATUS         status;

    status = boardObjInterfaceConstruct(pBObjGrp,
                pInterface, pInterfaceDesc, pVirtualTable);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkProgConstruct_3X_PRIMARY_exit;
    }
    p3xPrimary = (CLK_PROG_3X_PRIMARY *)pInterface;

    // Copy the CLK_PROG_3X_PRIMARY-specific data.
    p3xPrimary->bOCOVEnabled = p3xPrimarySet->bOCOVEnabled;
    p3xPrimary->freqDelta    = p3xPrimarySet->deltas.freqDelta;
    p3xPrimary->sourceData   = p3xPrimarySet->sourceData;

    // Allocate the VF entries array if not previously allocated.
    if (p3xPrimary->pVfEntries == NULL)
    {
        p3xPrimary->pVfEntries =
            lwosCallocType(pBObjGrp->ovlIdxDmem, Clk.progs.vfEntryCount,
                           LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_VF_ENTRY);
        if (p3xPrimary->pVfEntries == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NO_FREE_MEM;
            goto clkProgConstruct_3X_PRIMARY_exit;
        }
    }

    memcpy(p3xPrimary->pVfEntries, p3xPrimarySet->vfEntries,
        sizeof(LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_VF_ENTRY) *
                            Clk.progs.vfEntryCount);

    // Allocate the Voltage Delta entries array if not previously allocated.
    if (p3xPrimary->pVoltDeltauV == NULL)
    {
        p3xPrimary->pVoltDeltauV =
            lwosCallocType(pBObjGrp->ovlIdxDmem, Clk.progs.vfEntryCount, LwS32);
        if (p3xPrimary->pVoltDeltauV == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NO_FREE_MEM;
            goto clkProgConstruct_3X_PRIMARY_exit;
        }
    }

    memcpy(p3xPrimary->pVoltDeltauV, p3xPrimarySet->deltas.voltDeltauV,
        (sizeof(LwS32) * Clk.progs.vfEntryCount));

clkProgConstruct_3X_PRIMARY_exit:
    return status;
}

/*!
 * @copydoc ClkProg3XPrimaryVoltToFreqLinearSearch
 */
FLCN_STATUS
clkProg3XPrimaryVoltToFreqLinearSearch
(
    CLK_PROG_3X_PRIMARY                 *pProg3XPrimary,
    CLK_DOMAIN_3X_PRIMARY               *pDomain3XPrimary,
    LwU8                                clkDomIdx,
    LwU8                                voltRailIdx,
    LwU8                                voltageType,
    LW2080_CTRL_CLK_VF_INPUT           *pInput,
    LW2080_CTRL_CLK_VF_OUTPUT          *pOutput,
    LW2080_CTRL_CLK_VF_ITERATION_STATE *pVfIterState
)
{
    CLK_PROG_3X             *pProg3X =
        (CLK_PROG_3X *)INTERFACE_TO_BOARDOBJ_CAST(pProg3XPrimary);
    CLK_VF_POINT            *pVfPoint;
    LW2080_CTRL_CLK_VF_PAIR *pVfPair      = &pVfIterState->vfPair;
    FLCN_STATUS   status   = FLCN_OK;
    LwBoardObjIdx i;

    //
    // Only CLK_PROG entries with source == _NAFLL support _SOURCE voltage type.
    // Skip entries which aren't NAFLL.
    //
    if ((LW2080_CTRL_CLK_VOLTAGE_TYPE_SOURCE == voltageType) &&
        (LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL !=
            clkProg3XFreqSourceGet(pProg3X)))
    {
        goto clkProg3XPrimaryVoltToFreqLinearSearch_exit;
    }

    //
    // Iterate through set of CLK_VF_POINTs for this CLK_PROG_3X_PRIMARY to find
    // best matches.
    //
    for (i = LW_MAX(pProg3XPrimary->pVfEntries[voltRailIdx].vfPointIdxFirst,
                        pVfIterState->clkVfPointIdx);
            i <= pProg3XPrimary->pVfEntries[voltRailIdx].vfPointIdxLast;
            i++)
    {
        pVfPoint = CLK_VF_POINT_GET(PRI, i);
        if (pVfPoint == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkProg3XPrimaryVoltToFreqLinearSearch_exit;
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
                        pVfPair);        // pVfPair
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkProg3XPrimaryVoltToFreqLinearSearch_exit;
        }

        //
        // NOTE: Below code only works for the case where the VF lwrve is
        // monotonically increasing F -> V and this is enforced in RM/PMU
        //

        //
        // If the default flag is set and a matching VF point is not yet found
        // then set the default value here. If there is a better match it will
        // be overwritten in the next block
        // For Volt -> Freq first VF point is the default
        //
        if ((FLD_TEST_DRF(2080_CTRL_CLK_, VF_INPUT_FLAGS, _VF_POINT_SET_DEFAULT, _YES, pInput->flags)) &&
            (pOutput->inputBestMatch == LW2080_CTRL_CLK_VF_VALUE_ILWALID) &&
            (pOutput->value == LW2080_CTRL_CLK_VF_VALUE_ILWALID))
        {
            pOutput->inputBestMatch = pVfPair->voltageuV;
            pOutput->value = pVfPair->freqMHz;
        }

        //
        // If this CLK_VF_POINT's voltage <= input maxValue, then save the best
        // match found "so far"
        //
        if (pVfPair->voltageuV <= pInput->value)
        {
            pOutput->inputBestMatch = pVfPair->voltageuV;
            pOutput->value = pVfPair->freqMHz;
        }
        //
        // Once VF points above the maxValue, search is done.  Short-circuit the
        // search here.
        //
        else
        {
            status = FLCN_ERR_ITERATION_END;
            goto clkProg3XPrimaryVoltToFreqLinearSearch_exit;
        }

        // Save off the last matching VF_POINT index to iteration state.
        pVfIterState->clkVfPointIdx = i;
    }

clkProg3XPrimaryVoltToFreqLinearSearch_exit:
    return status;
}

/*!
 * @copydoc ClkProg3XPrimaryFreqToVoltLinearSearch
 */
FLCN_STATUS
clkProg3XPrimaryFreqToVoltLinearSearch
(
    CLK_PROG_3X_PRIMARY                 *pProg3XPrimary,
    CLK_DOMAIN_3X_PRIMARY               *pDomain3XPrimary,
    LwU8                                clkDomIdx,
    LwU8                                voltRailIdx,
    LwU8                                voltageType,
    LW2080_CTRL_CLK_VF_INPUT           *pInput,
    LW2080_CTRL_CLK_VF_OUTPUT          *pOutput,
    LW2080_CTRL_CLK_VF_ITERATION_STATE *pVfIterState
)
{
    CLK_DOMAIN_3X_PROG     *pDomain3XProg =
        (CLK_DOMAIN_3X_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomain3XPrimary);
    CLK_PROG_3X            *pProg3X =
        (CLK_PROG_3X *)INTERFACE_TO_BOARDOBJ_CAST(pProg3XPrimary);
    CLK_VF_POINT            *pVfPoint;
    LW2080_CTRL_CLK_VF_PAIR *pVfPair  = &pVfIterState->vfPair;
    FLCN_STATUS   status   = FLCN_OK;
    LwBoardObjIdx i;

    //
    // Only CLK_PROG entries with source == _NAFLL support _SOURCE voltage type.
    // Skip entries which aren't NAFLL.
    //
    if ((LW2080_CTRL_CLK_VOLTAGE_TYPE_SOURCE == voltageType) &&
        (LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL !=
             clkProg3XFreqSourceGet(pProg3X)))
    {
        goto clkProg3XPrimaryFreqToVoltLinearSearch_exit;
    }

    //
    // Iterate through set of CLK_VF_POINTs for this CLK_PROG_3X_PRIMARY to find
    // best matches.
    //
    for (i = LW_MAX(pProg3XPrimary->pVfEntries[voltRailIdx].vfPointIdxFirst,
                        pVfIterState->clkVfPointIdx);
            i <= pProg3XPrimary->pVfEntries[voltRailIdx].vfPointIdxLast;
            i++)
    {
        pVfPoint = CLK_VF_POINT_GET(PRI, i);
        if (pVfPoint == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkProg3XPrimaryFreqToVoltLinearSearch_exit;
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
                        pVfPair);        // pVfPair
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkProg3XPrimaryFreqToVoltLinearSearch_exit;
        }

        //
        // NOTE: Below code only works for the case where the VF lwrve is
        // monotonically increasing F -> V and this is enforced in RM/PMU
        //

        //
        // If this CLK_VF_POINT's frequency >= input milwalue, it might be a
        // better candidate for the minimum CLK_VF_POINT.
        //
        if (pVfPair->freqMHz >= pInput->value)
        {
            //
            // If inputBestMatch is _ILWALID, this is the first match/lowest
            // voltage that can support the input frequency.
            //
            if (LW2080_CTRL_CLK_VF_VALUE_ILWALID ==
                    pOutput->inputBestMatch)
            {
                pOutput->inputBestMatch = pVfPair->freqMHz;
                pOutput->value = pVfPair->voltageuV;
                //
                // We have found a match, search is done.  Short-circuit the
                // search here.
                //
                status = FLCN_ERR_ITERATION_END;
                goto clkProg3XPrimaryFreqToVoltLinearSearch_exit;
            }
        }

        //
        // Save off the last matching (i.e. criteria <= maxValue) VF_POINT index
        // to iteration state.
        //
        pVfIterState->clkVfPointIdx = i;
    }

    if (FLD_TEST_DRF(2080_CTRL_CLK, _VF_INPUT_FLAGS, _VF_POINT_SET_DEFAULT, _YES, pInput->flags) &&
            LW2080_CTRL_CLK_VF_VALUE_ILWALID == pOutput->inputBestMatch &&
            LW2080_CTRL_CLK_VF_VALUE_ILWALID == pOutput->value &&
            BOARDOBJ_GET_GRP_IDX_8BIT(&pProg3X->super.super.super) == clkDomain3xProgLastIdxGet(pDomain3XProg) &&
            ((i - 1U)  == pProg3XPrimary->pVfEntries[voltRailIdx].vfPointIdxLast)) // i is 1U past last index
    {
        //
        // For Freq -> Volt default value is the last VF point of the
        // last clock domain.
        //
        pOutput->inputBestMatch = pVfPair->freqMHz;
        pOutput->value = pVfPair->voltageuV;
    }

clkProg3XPrimaryFreqToVoltLinearSearch_exit:
    return status;
}

/*!
 * @copydoc ClkProg3XPrimaryFreqTranslatePrimaryToSecondary
 */
FLCN_STATUS
clkProg3XPrimaryFreqTranslatePrimaryToSecondary
(
    CLK_PROG_3X_PRIMARY    *pProg3XPrimary,
    CLK_DOMAIN_3X_PRIMARY  *pDomain3XPrimary,
    LwU8                   clkDomIdx,
    LwU16                 *pFreqMHz,
    LwBool                 bReqFreqDeltaAdj,
    LwBool                 bQuantize
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
            return clkProg3XPrimaryFreqTranslatePrimaryToSecondary_RATIO(pProg3XPrimary,
                        pDomain3XPrimary, clkDomIdx, pFreqMHz, bReqFreqDeltaAdj, bQuantize);
        case LW2080_CTRL_CLK_CLK_PROG_TYPE_30_PRIMARY_TABLE:
        case LW2080_CTRL_CLK_CLK_PROG_TYPE_35_PRIMARY_TABLE:
            return clkProg3XPrimaryFreqTranslatePrimaryToSecondary_TABLE(pProg3XPrimary,
                        pDomain3XPrimary, clkDomIdx, pFreqMHz, bReqFreqDeltaAdj, bQuantize);
    }

    // No type-specific implementation found.  Error!
    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/* ------------------------- Private Functions ------------------------------ */
