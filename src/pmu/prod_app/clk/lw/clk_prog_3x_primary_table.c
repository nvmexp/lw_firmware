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
 * _PRIMARY_TABLE implementation.
 *
 * @copydoc ClkProg3XPrimaryFreqTranslateSecondaryToPrimary
 */
FLCN_STATUS
clkProg3XPrimaryFreqTranslateSecondaryToPrimary_TABLE
(
    CLK_PROG_3X_PRIMARY    *pProg3XPrimary,
    CLK_DOMAIN_3X_PRIMARY  *pDomain3XPrimary,
    LwU8                   clkDomIdx,
    LwU16                 *pFreqMHz
)
{
    CLK_PROG_3X_PRIMARY_TABLE   *p3XPrimaryTable =
        CLK_PROG_INTERFACE_TO_INTERFACE_CAST(pProg3XPrimary, 3X_PRIMARY_TABLE);
    CLK_PROG_3X                *pProg3X        =
        (CLK_PROG_3X *)INTERFACE_TO_BOARDOBJ_CAST(pProg3XPrimary);
    LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_TABLE_SECONDARY_ENTRY *pSecondaryEntry;
    FLCN_STATUS status = FLCN_ERR_ILWALID_ARGUMENT;
    LwU8        i;

    if (p3XPrimaryTable == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto clkProg3XPrimaryFreqTranslateSecondaryToPrimary_TABLE_exit;
    }

    // Loop over the secondary entries to find matching clkDomIdx.
    for (i = 0; i < Clk.progs.secondaryEntryCount; i++)
    {
        pSecondaryEntry = &p3XPrimaryTable->pSecondaryEntries[i];

        // Found matching clkDomIdx.  Pull out corresponding frequency table value.
        if (clkDomIdx == pSecondaryEntry->clkDomIdx)
        {
            status = FLCN_OK;

            // Retrieve the PRIMARY CLK_DOMAIN's TABLE value.
            *pFreqMHz = pProg3X->freqMaxMHz;
            break;
        }
    }

clkProg3XPrimaryFreqTranslateSecondaryToPrimary_TABLE_exit:
    return status;
}

/*!
 * _PRIMARY_TABLE implementation.
 *
 * @copydoc ClkProg3XPrimaryIsPrimaryProgIdxForSecondaryFreqMHz
 */
LwBool
clkProg3XPrimaryIsPrimaryProgIdxForSecondaryFreqMHz_TABLE
(
    CLK_PROG_3X_PRIMARY    *pProg3XPrimary,
    CLK_DOMAIN_3X_PRIMARY  *pDomain3XPrimary,
    LwU8                   secondaryClkDomIdx,
    LwU16                  freqMHz
)
{
    CLK_PROG_3X_PRIMARY_TABLE                             *p3XPrimaryTable =
        CLK_PROG_INTERFACE_TO_INTERFACE_CAST(pProg3XPrimary, 3X_PRIMARY_TABLE);
    LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_TABLE_SECONDARY_ENTRY *pSecondaryEntry;
    LwBool  bIsValidIdx = LW_FALSE;
    LwU8    i;

    if (p3XPrimaryTable == NULL)
    {
        // status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto clkProg3XPrimaryIsPrimaryProgIdxForSecondaryFreqMHz_TABLE_exit;
    }

    // Loop over the secondary entries to find matching clkDomIdx.
    for (i = 0; i < Clk.progs.secondaryEntryCount; i++)
    {
        pSecondaryEntry = &p3XPrimaryTable->pSecondaryEntries[i];

        // Found matching secondaryClkDomIdx.
        if (secondaryClkDomIdx == pSecondaryEntry->clkDomIdx)
        {
            if (freqMHz <= pSecondaryEntry->freqMHz)
            {
                bIsValidIdx = LW_TRUE;
                break;
            }
        }
    }

clkProg3XPrimaryIsPrimaryProgIdxForSecondaryFreqMHz_TABLE_exit:
    return bIsValidIdx;
}

/* ------------------------- Private Functions ------------------------------ */
