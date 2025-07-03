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
 * @file clk_prog_3x_primary_ratio.c
 *
 * @brief Module managing all state related to the CLK_PROG_3X_PRIMARY_RATIO structure.
 * This structure defines how to program a given frequency on given PRIMARY
 * CLK_DOMAIN - a CLK_DOMAIN which has its own specified VF lwrve.
 *
 * The PRIMARY_RATIO class specifies SECONDARY Clock Domains' VF lwrve as ratio of
 * the PRIMARY Clock Domain.
 *
 */
//! https://wiki.lwpu.com/engwiki/index.php/Resman/PState/Data_Tables/Clock_Programming_Table/1.0_Spec#Ratio

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "dmemovl.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * _3X_PRIMARY_RATIO class constructor.
 *
 * @copydoc BoardObjInterfaceConstruct
 */
FLCN_STATUS
clkProgConstruct_3X_PRIMARY_RATIO
(
    BOARDOBJGRP                *pBObjGrp,
    BOARDOBJ_INTERFACE         *pInterface,
    RM_PMU_BOARDOBJ_INTERFACE  *pInterfaceDesc,
    INTERFACE_VIRTUAL_TABLE    *pVirtualTable
)
{
    RM_PMU_CLK_CLK_PROG_3X_PRIMARY_RATIO_BOARDOBJ_SET *p3xPrimaryRatioSet =
        (RM_PMU_CLK_CLK_PROG_3X_PRIMARY_RATIO_BOARDOBJ_SET *)pInterfaceDesc;
    CLK_PROG_3X_PRIMARY_RATIO   *p3xPrimaryRatio;
    FLCN_STATUS                 status;

    status = boardObjInterfaceConstruct(pBObjGrp, pInterface, pInterfaceDesc, pVirtualTable);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkProgConstruct_3X_PRIMARY_RATIO_exit;
    }
    p3xPrimaryRatio = (CLK_PROG_3X_PRIMARY_RATIO *)pInterface;

    // Copy the _3X_PRIMARY_RATIO-specific data.

    // Allocate the VF entries array if not previously allocated.
    if (p3xPrimaryRatio->pSecondaryEntries == NULL)
    {
        p3xPrimaryRatio->pSecondaryEntries =
            lwosCallocType(pBObjGrp->ovlIdxDmem, Clk.progs.secondaryEntryCount,
                           LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_RATIO_SECONDARY_ENTRY);
        if (p3xPrimaryRatio->pSecondaryEntries == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NO_FREE_MEM;
            goto clkProgConstruct_3X_PRIMARY_RATIO_exit;
        }
    }

    memcpy(p3xPrimaryRatio->pSecondaryEntries, p3xPrimaryRatioSet->secondaryEntries,
        sizeof(LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_RATIO_SECONDARY_ENTRY) *
                            Clk.progs.secondaryEntryCount);

clkProgConstruct_3X_PRIMARY_RATIO_exit:
    return status;
}

/*!
 * _PRIMARY_RATIO implementation.
 *
 * @copydoc ClkProg3XPrimaryFreqTranslatePrimaryToSecondary
 */
FLCN_STATUS
clkProg3XPrimaryFreqTranslatePrimaryToSecondary_RATIO
(
    CLK_PROG_3X_PRIMARY    *pProg3XPrimary,
    CLK_DOMAIN_3X_PRIMARY  *pDomain3XPrimary,
    LwU8                   clkDomIdx,
    LwU16                 *pFreqMHz,
    LwBool                 bReqFreqDeltaAdj,
    LwBool                 bQuantize
)
{
    CLK_PROG_3X_PRIMARY_RATIO   *p3XPrimaryRatio =
        CLK_PROG_INTERFACE_TO_INTERFACE_CAST(pProg3XPrimary, 3X_PRIMARY_RATIO);
    CLK_PROG_3X                *pProg3X        =
        (CLK_PROG_3X *)INTERFACE_TO_BOARDOBJ_CAST(pProg3XPrimary);
    LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_RATIO_SECONDARY_ENTRY *pSecondaryEntry;
    FLCN_STATUS status   = FLCN_ERR_ILWALID_ARGUMENT;
    LwU8       i;
    LwU32      freqMHz;

    if (p3XPrimaryRatio == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto clkProg3XPrimaryFreqTranslatePrimaryToSecondary_RATIO_exit;
    }

    // Loop over the secondary entries to find matching clkDomIdx.
    for (i = 0; i < Clk.progs.secondaryEntryCount; i++)
    {
        pSecondaryEntry = &p3XPrimaryRatio->pSecondaryEntries[i];

        // Found matching clkDomIdx.  Compute the ratio frequency for this domain.
        if (clkDomIdx == pSecondaryEntry->clkDomIdx)
        {
            CLK_DOMAIN_3X_PROG     *pDomain3XProg = (CLK_DOMAIN_3X_PROG *)
                BOARDOBJGRP_OBJ_GET(CLK_DOMAIN, clkDomIdx);
            CLK_DOMAIN_3X_SECONDARY    *pDomain3XSecondary;

            if (pDomain3XProg == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_INDEX;
                break;
            }

            // Initialization
            status          = FLCN_OK;
            pDomain3XSecondary  = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain3XProg, 3X_SECONDARY);
            if (pDomain3XSecondary == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_STATE;
                goto clkProg3XPrimaryFreqTranslatePrimaryToSecondary_RATIO_exit;
            }

            //
            // Do ratio callwlation in 32-bit to avoid overflow.
            //
            // Note: Intentionally not using LW_UNSIGNED_ROUNDED_DIV() - don't
            // want to round up in frequency, as that could cause timing
            // failures (F too high for V).
            //
            freqMHz = ((*pFreqMHz) * (LwU32)pSecondaryEntry->ratio) / 100U;
            *pFreqMHz = (LwU16)freqMHz;

            if (bQuantize)
            {
                //
                // Quantize before any frequency offsets - the properly quantized
                // value should be the base against which offsets are applied.
                // Otherwise, can see effective offsets > offset value - e.g. a
                // 27MHz delta parameter becomes a 40 MHz effective delta.
                //
                status = clkProg3XFreqMHzQuantize(
                            pProg3X,                            // pProg3X
                            pDomain3XProg,                      // pDomain3XProg
                            pFreqMHz,                           // pFreqMHz
                            LW_TRUE);                           // bFloor
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto clkProg3XPrimaryFreqTranslatePrimaryToSecondary_RATIO_exit;
                }
            }

            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_OVOC_SUPPORTED) &&
                bReqFreqDeltaAdj)
            {
                //
                // Adjust for only SECONDARY deltas for RATIO SECONDARIES,
                // as RATIO SECONDARY frequency is a direct function of
                // the PRIMARY frequency which has already been offset.
                //
                status = clkDomain3XSecondaryFreqAdjustDeltaMHz(
                            pDomain3XSecondary, // pDomain3XSecondary
                            pProg3XPrimary,     // pProg3XPrimary
                            LW_FALSE,           // bIncludePrimary
                            LW_TRUE,            // bQuantize
                            LW_FALSE,           // bVFOCAdjReq
                            pFreqMHz);          // pFreqMHz
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto clkProg3XPrimaryFreqTranslatePrimaryToSecondary_RATIO_exit;
                }
            }
            break;
        }
    }

clkProg3XPrimaryFreqTranslatePrimaryToSecondary_RATIO_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
