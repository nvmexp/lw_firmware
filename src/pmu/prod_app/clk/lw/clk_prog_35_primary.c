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
 * @file clk_prog_35_primary.c
 *
 * @brief Module managing all state related to the CLK_PROG_35_PRIMARY structure.
 * This structure defines how to program a given frequency on given PRIMARY
 * CLK_DOMAIN - a CLK_DOMAIN which has its own specified VF lwrve.
 *
 */
//!  https://wiki.lwpu.com/engwiki/index.php/Resman/PState/Data_Tables/Clock_Programming_Table/1.0_Spec#Primary

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "volt/objvolt.h"
#include "dmemovl.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * @copydoc clkProg35PrimarySmoothing
 */
FLCN_STATUS
clkProg35PrimarySmoothing
(
    CLK_PROG_35_PRIMARY         *pProg35Primary,
    CLK_DOMAIN_35_PRIMARY       *pDomain35Primary,
    LwU8                        voltRailIdx,
    LwU8                        lwrveIdx,
    CLK_VF_POINT_35            *pVfPoint35Last
)
{
    CLK_VF_POINT_35    *pVfPoint35;
    FLCN_STATUS         status        = FLCN_OK;
    LwS16               i;

    // Sanity check.
    if ((LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_ILWALID == lwrveIdx) ||
        (LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID ==
            clklkProg35PrimaryVfPointIdxFirstGet(pProg35Primary, voltRailIdx, lwrveIdx)) ||
        (LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID ==
            clklkProg35PrimaryVfPointIdxLastGet(pProg35Primary, voltRailIdx, lwrveIdx)))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkProg35PrimarySmoothing_exit;
    }

    //
    // Iterate over this CLK_PROG_35_PRIMARY's CLK_VF_POINTs and smoothen
    // each of them in order, passing the pVfPoint35Last to ensure
    // the max allowed discontinuity is within the expected bound between
    // two conselwtive VF points.
    //
    for (i = clklkProg35PrimaryVfPointIdxLastGet(pProg35Primary, voltRailIdx, lwrveIdx);
            i >= clklkProg35PrimaryVfPointIdxFirstGet(pProg35Primary, voltRailIdx, lwrveIdx);
            i--)
    {
        if (i != clklkProg35PrimaryVfPointIdxLastGet(pProg35Primary, voltRailIdx, lwrveIdx))
        {
            pVfPoint35Last = (CLK_VF_POINT_35 *)CLK_VF_POINT_GET_BY_LWRVE_ID(lwrveIdx, (i+1));
            if (pVfPoint35Last == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_INDEX;
                goto clkProg35PrimarySmoothing_exit;
            }
        }

        pVfPoint35 = (CLK_VF_POINT_35 *)CLK_VF_POINT_GET_BY_LWRVE_ID(lwrveIdx, i);
        if (pVfPoint35 == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkProg35PrimarySmoothing_exit;
        }

        status = clkVfPoint35Smoothing(pVfPoint35,
                                       pVfPoint35Last,
                                       pDomain35Primary,
                                       pProg35Primary,
                                       lwrveIdx);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkProg35PrimarySmoothing_exit;
        }
    }

clkProg35PrimarySmoothing_exit:
    return status;
}

/*!
 * @copydoc ClkProg35PrimaryTrim
 */
FLCN_STATUS
clkProg35PrimaryTrim
(
    CLK_PROG_35_PRIMARY         *pProg35Primary,
    CLK_DOMAIN_35_PRIMARY       *pDomain35Primary,
    LwU8                        voltRailIdx,
    LwU8                        lwrveIdx
)
{
    CLK_VF_POINT_35    *pVfPoint35;
    FLCN_STATUS         status        = FLCN_OK;
    LwS16               i;

    // Sanity check.
    if ((LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_ILWALID == lwrveIdx) ||
        (LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID ==
            clklkProg35PrimaryVfPointIdxFirstGet(pProg35Primary, voltRailIdx, lwrveIdx)) ||
        (LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID ==
            clklkProg35PrimaryVfPointIdxLastGet(pProg35Primary, voltRailIdx, lwrveIdx)))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkProg35PrimaryTrim_exit;
    }

    // Trim all of the VF points for this clock programming primary - secondary group.
    for (i = clklkProg35PrimaryVfPointIdxLastGet(pProg35Primary, voltRailIdx, lwrveIdx);
            i >= clklkProg35PrimaryVfPointIdxFirstGet(pProg35Primary, voltRailIdx, lwrveIdx);
            i--)
    {
        pVfPoint35 = (CLK_VF_POINT_35 *)CLK_VF_POINT_GET_BY_LWRVE_ID(lwrveIdx, i);
        if (pVfPoint35 == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkProg35PrimaryTrim_exit;
        }

        status = clkVfPoint35Trim(pVfPoint35,
                                  pDomain35Primary,
                                  pProg35Primary,
                                  lwrveIdx);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkProg35PrimaryTrim_exit;
        }
    }

clkProg35PrimaryTrim_exit:
    return status;
}

/*!
 * @copydoc ClkProg3XPrimaryMaxVFPointFreqDeltaAdj
 */
FLCN_STATUS
clkProg3XPrimaryMaxVFPointFreqDeltaAdj
(
    CLK_PROG_3X_PRIMARY     *pProg3XPrimary,
    CLK_DOMAIN_3X_PRIMARY   *pDomain3XPrimary,
    LwU16                  *pFreqMaxMHz
)
{
    // This is not required from VF 35+
    PMU_HALT();

    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @copydoc ClkProg35PrimaryClientFreqDeltaAdj
 *
 * For each rail, This interface iterates over the VF lwrve starting from
 * MAX -> MIN VF point to find the first VF point whose base frequency is
 * lower than or equal to the input frequency. The next step is to find the
 * offseted frequency value of that pertilwlar VF point and adjust the input
 * frequency based on offseted frequency so that they are always monotonically
 * increasing.
 * The final output will be the MIN of all output frequency obtained per rail.
 * 
 */
FLCN_STATUS
clkProg35PrimaryClientFreqDeltaAdj
(
    CLK_PROG_35_PRIMARY     *pProg35Primary,
    CLK_DOMAIN_35_PRIMARY   *pDomain35Primary,
    LwU8                    clkDomIdx,
    LwU16                  *pFreqMHz
)
{
    CLK_DOMAIN_3X_PRIMARY   *pDomain3XPrimary =
        CLK_CLK_DOMAIN_3X_PRIMARY_GET_FROM_35_PRIMARY(pDomain35Primary);
    CLK_PROG_3X_PRIMARY     *pProg3XPrimary   =
        CLK_CLK_PROG_3X_PRIMARY_GET_FROM_35_PRIMARY(pProg35Primary);
    CLK_VF_POINT           *pVfPoint;
    LwU8                    voltRailIdx;
    LW2080_CTRL_CLK_VF_PAIR vfPair;
    LwU16                   railOffsetedFreqMHz = 0;
    LwU16                   offsetedFreqMHz     = LW_U16_MAX;
    LwS16                   i;
    FLCN_STATUS             status          = FLCN_OK;

    // Short-circuit if OC is NOT allowed on this clock programing entry
    if (!clkProg3XPrimaryOVOCEnabled(pProg3XPrimary))
    {
        goto clkProg35PrimaryClientFreqDeltaAdj_exit;
    }

    // Iterate over each VOLTAGE_RAIL separately.
    for (voltRailIdx = 0; voltRailIdx < Clk.domains.voltRailsMax; voltRailIdx++)
    {
        // Reset the per rail offseted frequency.
        railOffsetedFreqMHz = *pFreqMHz;

        // Iterate over this CLK_PROG_3X_PRIMARY's CLK_VF_POINTs
        for (i = pProg3XPrimary->pVfEntries[voltRailIdx].vfPointIdxLast;
                i >= pProg3XPrimary->pVfEntries[voltRailIdx].vfPointIdxFirst;
                i--)
        {
            pVfPoint = CLK_VF_POINT_GET(PRI, i);

            if (pVfPoint == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_INDEX;
                goto clkProg35PrimaryClientFreqDeltaAdj_exit;
            }

            // Retrieve the base frequency tuple.
            status = clkVfPointAccessor(pVfPoint,                           // pVfPoint
                                        pProg3XPrimary,                      // pProg3XPrimary
                                        pDomain3XPrimary,                    // pDomain3XPrimary
                                        clkDomIdx,                          // clkDomIdx
                                        voltRailIdx,                        // voltRailIdx
                                        LW2080_CTRL_CLK_VOLTAGE_TYPE_POR,   // voltageType
                                        LW_FALSE,                           // bOffseted
                                        &vfPair);                           // pVfPair
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkProg35PrimaryClientFreqDeltaAdj_exit;
            }

            // Locate the VF point.
            if (vfPair.freqMHz <= *pFreqMHz)
            {
                LwU16 origFreqMHz = vfPair.freqMHz;

                // Retrieve the offseted frequency tuple.
                status = clkVfPointAccessor(pVfPoint,                           // pVfPoint
                                            pProg3XPrimary,                      // pProg3XPrimary 
                                            pDomain3XPrimary,                    // pDomain3XPrimary 
                                            clkDomIdx,                          // clkDomIdx 
                                            voltRailIdx,                        // voltRailIdx 
                                            LW2080_CTRL_CLK_VOLTAGE_TYPE_POR,   // voltageType 
                                            LW_TRUE,                            // bOffseted 
                                            &vfPair);                           // pVfPair 
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto clkProg35PrimaryClientFreqDeltaAdj_exit;
                }

                // All Frequency based VF points.
                if (!pDomain35Primary->super.super.super.bNoiseAwareCapable)
                {
                    //
                    // With Frequency based VF points, we MUST have an exact
                    // match to client request in our base VF lwrve and we MUST
                    // select the offseted value of
                    // that VF point. If we do not match it then we could end up
                    // with VF point not available in supproted PSTATE range.
                    // For example,. Let's consider DRAM CLK on GV100 where we have
                    // PSTATE min = max = nom = 850 MHz. If client applied -50 MHz,
                    // the only point available in VF lwrve will be 800 MHz frequency.
                    // We must adjust CLK PROG offseted MAX and PSTATE MIN = MAX = NOM
                    // = 800 MHz to keep the sane driver state.
                    //
                    if (origFreqMHz != *pFreqMHz)
                    {
                        PMU_HALT();
                    }

                    railOffsetedFreqMHz = vfPair.freqMHz;
                }
                //
                // All voltage based VF Points.
                // Assume 30 MHz clk domain offset
                // EX)  BASE | OFFSETTED 
                // N)   525  | 555
                // N+1) 570  | 600
                // BUG: 
                // Ex1 - (client input = 555 , output = 555, expected = 585.
                // Ex2 - (client input = 540 , output = 540, expected = 570.
                // FIX: *pFreqMHz -> origFreqMHz
                // Exaplantion - If the base and offset adjusted frequency are
                // not same, the input frequency shall be adjusted with the
                // same offset followed by trimming based on next higer VF point
                // offset adjusted freq to ensure monotonically increasing nature.
                //
                else if (vfPair.freqMHz > origFreqMHz)
                {
                    //
                    // For voltage based VF points, we only take the postive
                    // offets while adjusting the frequency for PSTATE tuple
                    // as well as  CLK PROG MAX. In this case, Our VF lwrve
                    // is determined by voltage as input param, we can always
                    // support the MAX POR value as long as chip can allow the
                    // voltage (reliablility voltage max) required to support
                    // the offseted frequency point. So we only increase the max
                    // with positive offset but we do not dicrease the max if
                    // client apply negative offset. In case of negative offset,
                    // we want to allow user to hit the POR max but with higher
                    // voltage if chips can support it. If not, the voltage max
                    // limit will cap the max allowed frequency but the enumeration
                    // points till the max frequency will be still available for
                    // arbiter to choose.
                    //

                    // If the exact match found, pick the offseted frequency value.
                    if (origFreqMHz == *pFreqMHz)
                    {
                        railOffsetedFreqMHz = vfPair.freqMHz;
                    }
                    else
                    {
                        CLK_PROG_3X   *pProg3X;
                        LwU8           progIdx;
                        LwU16          prevVFPointOffset = (vfPair.freqMHz - origFreqMHz); // We know its positive.

                        railOffsetedFreqMHz = *pFreqMHz + (vfPair.freqMHz - origFreqMHz);

                        //
                        // If offseted frequency value is greater then next VF point, cap it to next VF
                        // point frequency value as we do not want to increase the voltage.
                        // Otherwise, select the max offset value of the two closest VF points.
                        //
                        if (i != pProg3XPrimary->pVfEntries[voltRailIdx].vfPointIdxLast)
                        {
                            pVfPoint = CLK_VF_POINT_GET(PRI, (i + 1));
                            if (pVfPoint == NULL)
                            {
                                PMU_BREAKPOINT();
                                status = FLCN_ERR_ILWALID_INDEX;
                                goto clkProg35PrimaryClientFreqDeltaAdj_exit;
                            }

                            // Retrieve the offseted frequency tuple.
                            status = clkVfPointAccessor(pVfPoint,                           // pVfPoint
                                                        pProg3XPrimary,                      // pProg3XPrimary 
                                                        pDomain3XPrimary,                    // pDomain3XPrimary 
                                                        clkDomIdx,                          // clkDomIdx 
                                                        voltRailIdx,                        // voltRailIdx 
                                                        LW2080_CTRL_CLK_VOLTAGE_TYPE_POR,   // voltageType 
                                                        LW_TRUE,                            // bOffseted 
                                                        &vfPair);                           // pVfPair 
                            if (status != FLCN_OK)
                            {
                                PMU_BREAKPOINT();
                                goto clkProg35PrimaryClientFreqDeltaAdj_exit;
                            }

                            if (railOffsetedFreqMHz >= vfPair.freqMHz)
                            {
                                railOffsetedFreqMHz = vfPair.freqMHz;
                                break;
                            }
                            else
                            {
                                LwU16 lwrrVFPointOffsetedValue = vfPair.freqMHz;

                                pVfPoint = CLK_VF_POINT_GET(PRI, (i + 1));
                                if (pVfPoint == NULL)
                                {
                                    PMU_BREAKPOINT();
                                    status = FLCN_ERR_ILWALID_INDEX;
                                    goto clkProg35PrimaryClientFreqDeltaAdj_exit;
                                }

                                // Retrieve the offseted frequency tuple.
                                status = clkVfPointAccessor(pVfPoint,                           // pVfPoint
                                                            pProg3XPrimary,                      // pProg3XPrimary 
                                                            pDomain3XPrimary,                    // pDomain3XPrimary 
                                                            clkDomIdx,                          // clkDomIdx 
                                                            voltRailIdx,                        // voltRailIdx 
                                                            LW2080_CTRL_CLK_VOLTAGE_TYPE_POR,   // voltageType 
                                                            LW_FALSE,                           // bOffseted 
                                                            &vfPair);                           // pVfPair 
                                if (status != FLCN_OK)
                                {
                                    PMU_BREAKPOINT();
                                    goto clkProg35PrimaryClientFreqDeltaAdj_exit;
                                }

                                if ((lwrrVFPointOffsetedValue > vfPair.freqMHz) &&
                                    ((lwrrVFPointOffsetedValue - vfPair.freqMHz) > prevVFPointOffset))
                                {
                                    railOffsetedFreqMHz = *pFreqMHz + (lwrrVFPointOffsetedValue - vfPair.freqMHz);
                                }
                            }
                        }

                        //
                        // As this interface is also used to adjust to CLK PROG offseted
                        // max frequency, we can not use generic Clk Domain interface
                        // which does runtime adjustment for CLK PROG MAX.
                        //
                        progIdx = clkDomain3XProgGetClkProgIdxFromFreq(
                                    &pDomain35Primary->super.super,  // pDomain3XProg
                                    railOffsetedFreqMHz,            // freqMHz
                                    LW_FALSE);                      // bReqFreqDeltaAdj

                        pProg3X = (CLK_PROG_3X *)BOARDOBJGRP_OBJ_GET(CLK_PROG, progIdx);
                        if (pProg3X == NULL)
                        {
                            PMU_BREAKPOINT();
                            status = FLCN_ERR_ILWALID_INDEX;
                            goto clkProg35PrimaryClientFreqDeltaAdj_exit;
                        }

                        // Quantize the offseted frequency
                        status = clkProg3XFreqMHzQuantize(
                                    pProg3X,                        // pProg3X
                                    &pDomain35Primary->super.super,  // pDomain3XProg
                                    &railOffsetedFreqMHz,           // pFreqMHz
                                    LW_TRUE);                       // bFloor
                        if (status != FLCN_OK)
                        {
                            PMU_BREAKPOINT();
                            goto clkProg35PrimaryClientFreqDeltaAdj_exit;
                        }
                    }
                }
                break;
            }
        }

        //
        // Min offset frequency of all rails as we want to hit the max allowed
        // freq without increasing voltage.
        //
        offsetedFreqMHz = LW_MIN(offsetedFreqMHz, railOffsetedFreqMHz);
    }

    // Adjust the input frequency by the offset.
    if (offsetedFreqMHz != LW_U16_MAX)
    {
        *pFreqMHz = offsetedFreqMHz;
    }

clkProg35PrimaryClientFreqDeltaAdj_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
