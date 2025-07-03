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
 * @file clk_prog_3x_primary_table.c
 *
 * @brief Module managing all state related to the CLK_PROG_3X_PRIMARY_TABLE structure.
 * This structure defines how to program a given frequency on given PRIMARY
 * CLK_DOMAIN - a CLK_DOMAIN which has its own specified VF lwrve.
 *
 * The PRIMARY_TABLE class specifies SECONDARY Clock Domains' VF lwrve as table of
 * the PRIMARY Clock Domain.
 *
 */
//! https://wiki.lwpu.com/engwiki/index.php/Resman/PState/Data_Tables/Clock_Programming_Table/1.0_Spec#Table

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "dmemovl.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * _3X_PRIMARY_TABLE class constructor.
 *
 * @copydoc BoardObjInterfaceConstruct
 */
FLCN_STATUS
clkProgConstruct_3X_PRIMARY_TABLE
(
    BOARDOBJGRP                *pBObjGrp,
    BOARDOBJ_INTERFACE         *pInterface,
    RM_PMU_BOARDOBJ_INTERFACE  *pInterfaceDesc,
    INTERFACE_VIRTUAL_TABLE    *pVirtualTable
)
{
    RM_PMU_CLK_CLK_PROG_3X_PRIMARY_TABLE_BOARDOBJ_SET *p3xPrimaryTableSet =
        (RM_PMU_CLK_CLK_PROG_3X_PRIMARY_TABLE_BOARDOBJ_SET *)pInterfaceDesc;
    CLK_PROG_3X_PRIMARY_TABLE   *p3xPrimaryTable;
    FLCN_STATUS                 status;

    status = boardObjInterfaceConstruct(pBObjGrp, pInterface, pInterfaceDesc, pVirtualTable);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkProgConstruct_3X_PRIMARY_TABLE_exit;
    }
    p3xPrimaryTable = (CLK_PROG_3X_PRIMARY_TABLE *)pInterface;

    // Copy the _3X_PRIMARY_TABLE-specific data.

    // Allocate the VF entries array if not previously allocated.
    if (p3xPrimaryTable->pSecondaryEntries == NULL)
    {
        p3xPrimaryTable->pSecondaryEntries =
            lwosCallocType(pBObjGrp->ovlIdxDmem, Clk.progs.secondaryEntryCount,
                           LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_TABLE_SECONDARY_ENTRY);
        if (p3xPrimaryTable->pSecondaryEntries == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NO_FREE_MEM;
            goto clkProgConstruct_3X_PRIMARY_TABLE_exit;
        }
    }

    // Copy the CLK_PROG_35_PRIMARY_TABLE-specific data.
    memcpy(p3xPrimaryTable->pSecondaryEntries, p3xPrimaryTableSet->secondaryEntries,
        sizeof(LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_TABLE_SECONDARY_ENTRY) *
                            Clk.progs.secondaryEntryCount);

clkProgConstruct_3X_PRIMARY_TABLE_exit:
    return status;
}

/*!
 * _PRIMARY_TABLE implementation.
 *
 * @copydoc ClkProg3XPrimaryFreqTranslatePrimaryToSecondary
 */
FLCN_STATUS
clkProg3XPrimaryFreqTranslatePrimaryToSecondary_TABLE
(
    CLK_PROG_3X_PRIMARY    *pProg3XPrimary,
    CLK_DOMAIN_3X_PRIMARY  *pDomain3XPrimary,
    LwU8                   clkDomIdx,
    LwU16                 *pFreqMHz,
    LwBool                 bReqFreqDeltaAdj,
    LwBool                 bQuantize
)
{
    CLK_PROG_3X_PRIMARY_TABLE                             *p3XPrimaryTable =
        CLK_PROG_INTERFACE_TO_INTERFACE_CAST(pProg3XPrimary, 3X_PRIMARY_TABLE);
    LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_TABLE_SECONDARY_ENTRY *pSecondaryEntry;
    FLCN_STATUS status = FLCN_ERR_ILWALID_ARGUMENT;
    LwU8       i;

    if (p3XPrimaryTable == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto clkProg3XPrimaryFreqTranslatePrimaryToSecondary_TABLE_exit;
    }

    // Loop over the secondary entries to find matching clkDomIdx.
    for (i = 0; i < Clk.progs.secondaryEntryCount; i++)
    {
        pSecondaryEntry = &p3XPrimaryTable->pSecondaryEntries[i];

        // Found matching clkDomIdx.  Pull out corresponding frequency table value.
        if (clkDomIdx == pSecondaryEntry->clkDomIdx)
        {
            CLK_DOMAIN          *pDomain = BOARDOBJGRP_OBJ_GET(CLK_DOMAIN, clkDomIdx);
            CLK_DOMAIN_3X_SECONDARY *pDomain3XSecondary;

            if (pDomain == NULL)
            {
                status = FLCN_ERR_ILWALID_INDEX;
                PMU_BREAKPOINT();
                goto clkProg3XPrimaryFreqTranslatePrimaryToSecondary_TABLE_exit;
            }

            pDomain3XSecondary = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, 3X_SECONDARY);
            if (pDomain3XSecondary == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_STATE;
                goto clkProg3XPrimaryFreqTranslatePrimaryToSecondary_TABLE_exit;
            }

            // Initialization
            status = FLCN_OK;

            // Retrieve the SECONDARY CLK_DOMAIN's TABLE value.
            *pFreqMHz = pSecondaryEntry->freqMHz;

            // Ignore the quantization flag as it is not required in table relation.

            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_OVOC_SUPPORTED) &&
                bReqFreqDeltaAdj)
            {
                //
                // Adjust for PRIMARY + SECONDARY deltas for TABLE SECONDARIES,
                // as TABLE SECONDARY frequency isn't a direct function of
                // the PRIMARY frequency.
                //
                status = clkDomain3XSecondaryFreqAdjustDeltaMHz(pDomain3XSecondary,
                            pProg3XPrimary, LW_TRUE, LW_TRUE, LW_FALSE, pFreqMHz);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto clkProg3XPrimaryFreqTranslatePrimaryToSecondary_TABLE_exit;
                }
            }
            break;
        }
    }

clkProg3XPrimaryFreqTranslatePrimaryToSecondary_TABLE_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
