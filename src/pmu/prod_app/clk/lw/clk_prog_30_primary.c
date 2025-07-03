/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file clk_prog_30_primary.c
 *
 * @brief Module managing all state related to the CLK_PROG_30_PRIMARY structure.
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
/*!
 * Main structure for all CLK_PROG_3X_PRIMARY_VIRTUAL_TABLE data.
 */
INTERFACE_VIRTUAL_TABLE ClkProg3XPrimaryVirtualTable_30 =
{
    LW_OFFSETOF(CLK_PROG_30_PRIMARY, primary)   // offset
};

/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_PROG_30_PRIMARY class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkProgGrpIfaceModel10ObjSet_30_PRIMARY
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_CLK_CLK_PROG_30_PRIMARY_BOARDOBJ_SET *p30PrimarySet =
        (RM_PMU_CLK_CLK_PROG_30_PRIMARY_BOARDOBJ_SET *)pBoardObjSet;
    CLK_PROG_30_PRIMARY *p30Primary;
    FLCN_STATUS         status;

    status = clkProgGrpIfaceModel10ObjSet_30(pModel10, ppBoardObj, size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkProgGrpIfaceModel10ObjSet_30_PRIMARY_exit;
    }
    p30Primary = (CLK_PROG_30_PRIMARY *)*ppBoardObj;

    status = clkProgConstruct_3X_PRIMARY(pBObjGrp,
                &p30Primary->primary.super,
                &p30PrimarySet->primary.super,
                &ClkProg3XPrimaryVirtualTable_30);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkProgGrpIfaceModel10ObjSet_30_PRIMARY_exit;
    }
    // No need to update the Boardobj Vtable pointer, leaf class will do it.

clkProgGrpIfaceModel10ObjSet_30_PRIMARY_exit:
    return status;
}

/*!
 * @copydoc clkProg30PrimaryCache
 */
FLCN_STATUS
clkProg30PrimaryCache
(
    CLK_PROG_30_PRIMARY         *pProg30Primary,
    CLK_DOMAIN_30_PRIMARY       *pDomain30Primary,
    LwU8                        voltRailIdx,
    LW2080_CTRL_CLK_VF_PAIR    *pVFPairLast,
    LW2080_CTRL_CLK_VF_PAIR    *pBaseVFPairLast
)
{
    CLK_PROG_3X_PRIMARY *pProg3XPrimary =
        CLK_CLK_PROG_3X_PRIMARY_GET_FROM_30_PRIMARY(pProg30Primary);
    CLK_VF_POINT_30    *pVfPoint30;
    FLCN_STATUS         status = FLCN_OK;
    LwBoardObjIdx       i;

    //
    // Iterate over this CLK_PROG_30_PRIMARY's CLK_VF_POINTs and cache each of
    // them in order, passing the vFPairLast structure to ensure that the
    // CLK_VF_POINTs are monotonically increasing.
    //
    for (i = pProg3XPrimary->pVfEntries[voltRailIdx].vfPointIdxFirst;
            i <= pProg3XPrimary->pVfEntries[voltRailIdx].vfPointIdxLast;
            i++)
    {
        pVfPoint30 = (CLK_VF_POINT_30 *)CLK_VF_POINT_GET(PRI, i);
        if (pVfPoint30 == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkProg30PrimaryCache_exit;
        }

        status = clkVfPoint30Cache(pVfPoint30,
                                   pDomain30Primary,
                                   pProg30Primary,
                                   voltRailIdx,
                                   pVFPairLast,
                                   pBaseVFPairLast);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkProg30PrimaryCache_exit;
        }
    }

clkProg30PrimaryCache_exit:
    return status;
}

/*!
 * @copydoc clkProg30PrimarySmoothing
 */
FLCN_STATUS
clkProg30PrimarySmoothing
(
    CLK_PROG_30_PRIMARY         *pProg30Primary,
    CLK_DOMAIN_30_PRIMARY       *pDomain30Primary,
    LwU8                        voltRailIdx
)
{
    CLK_PROG_3X_PRIMARY *pProg3XPrimary =
        CLK_CLK_PROG_3X_PRIMARY_GET_FROM_30_PRIMARY(pProg30Primary);
    CLK_VF_POINT_30    *pVfPoint30;
    FLCN_STATUS         status = FLCN_OK;
    LwS16               i;

    // Initialize the expected frequency to zero.
    LwU16   freqMHzExpected = 0;

    //
    // Iterate over this CLK_PROG_30_PRIMARY's CLK_VF_POINTs and smoothen each
    // of them in order, passing the freqMHzExpected to ensure the max allowed
    // discontinuity is within the expected bound between two conselwtive
    // VF points.
    //
    for (i = pProg3XPrimary->pVfEntries[voltRailIdx].vfPointIdxLast;
            i >= pProg3XPrimary->pVfEntries[voltRailIdx].vfPointIdxFirst;
            i--)
    {
        pVfPoint30 = (CLK_VF_POINT_30 *)CLK_VF_POINT_GET(PRI, i);
        if (pVfPoint30 == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkProg30PrimarySmoothing_exit;
        }

        status = clkVfPoint30Smoothing(pVfPoint30,
                                       pDomain30Primary,
                                       pProg30Primary,
                                       &freqMHzExpected);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkProg30PrimarySmoothing_exit;
        }
    }

clkProg30PrimarySmoothing_exit:
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
    CLK_PROG_3X        *pProg3X =
        (CLK_PROG_3X *)INTERFACE_TO_BOARDOBJ_CAST(pProg3XPrimary);
    CLK_VF_POINT_30    *pVfPoint30;
    LwU8                voltRailIdx;
    LwS16               freqDeltaMHz = 0;
    LwU16               freqMHz      = LW_U16_MAX;
    LwS16               i;

    if (LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL !=
            clkProg3XFreqSourceGet(pProg3X))
    {
        return FLCN_OK;
    }

    // Iterate over each VOLTAGE_RAIL separately.
    for (voltRailIdx = 0; voltRailIdx < Clk.domains.voltRailsMax; voltRailIdx++)
    {
        LwS16   localFreqDeltaMHz = 0;
        LwU16   localFreqMHz      = LW_U16_MAX;

        // Iterate over this CLK_PROG_3X_PRIMARY's CLK_VF_POINTs
        for (i = pProg3XPrimary->pVfEntries[voltRailIdx].vfPointIdxLast;
                i >= pProg3XPrimary->pVfEntries[voltRailIdx].vfPointIdxFirst;
                i--)
        {
            pVfPoint30 = (CLK_VF_POINT_30 *)CLK_VF_POINT_GET(PRI, i);
            if (pVfPoint30 == NULL)
            {
                PMU_BREAKPOINT();
                return FLCN_ERR_ILWALID_INDEX;
            }

            // Retrieve the frequency and offset for this CLK_VF_POINT.
            localFreqMHz      = clkVfPoint30FreqMHzGet(pVfPoint30);
            localFreqDeltaMHz = clkVfPoint30FreqDeltaMHzGet(pVfPoint30);

            if ((localFreqMHz - localFreqDeltaMHz) <= *pFreqMaxMHz)
            {
                break;
            }
        }

        if (freqMHz > localFreqMHz)
        {
            freqMHz      = localFreqMHz;
            freqDeltaMHz = localFreqDeltaMHz;
        }
    }

    // Update the input freq based on the max VF point freq delta.
    if (freqDeltaMHz > 0)
    {
        *pFreqMaxMHz = *pFreqMaxMHz + freqDeltaMHz;
    }

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
