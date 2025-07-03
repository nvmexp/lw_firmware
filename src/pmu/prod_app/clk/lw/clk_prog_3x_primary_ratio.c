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
 * _PRIMARY_RATIO implementation.
 *
 * @copydoc ClkProg3XPrimaryFreqTranslateSecondaryToPrimary
 */
FLCN_STATUS
clkProg3XPrimaryFreqTranslateSecondaryToPrimary_RATIO
(
    CLK_PROG_3X_PRIMARY    *pProg3XPrimary,
    CLK_DOMAIN_3X_PRIMARY  *pDomain3XPrimary,
    LwU8                   clkDomIdx,
    LwU16                 *pFreqMHz
)
{
    CLK_DOMAIN_3X_PROG         *pDomain3XProg  =
        (CLK_DOMAIN_3X_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomain3XPrimary);
    CLK_PROG_3X_PRIMARY_RATIO   *p3XPrimaryRatio =
        CLK_PROG_INTERFACE_TO_INTERFACE_CAST(pProg3XPrimary, 3X_PRIMARY_RATIO);
    CLK_PROG_3X                *pProg3X        =
        (CLK_PROG_3X *)INTERFACE_TO_BOARDOBJ_CAST(pProg3XPrimary);
    LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_RATIO_SECONDARY_ENTRY *pSecondaryEntry;
    FLCN_STATUS status   = FLCN_ERR_ILWALID_ARGUMENT;
    LwU8        i;
    LwU32       freqMHz;

    if (p3XPrimaryRatio == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto clkProg3XPrimaryFreqTranslateSecondaryToPrimary_RATIO_exit;
    }

    // Loop over the secondary entries to find matching clkDomIdx.
    for (i = 0; i < Clk.progs.secondaryEntryCount; i++)
    {
        pSecondaryEntry = &p3XPrimaryRatio->pSecondaryEntries[i];

        // Found matching clkDomIdx.  Compute the ratio frequency for this domain.
        if (clkDomIdx == pSecondaryEntry->clkDomIdx)
        {
            status = FLCN_OK;

            //
            // Note: The clock quantization code always quantize the frequency
            // to floor for MAX, I am using the LW_UNSIGNED_DIV_CEIL() macro and
            // quantizing the Freq up to the next valid NDIV point.
            // As of now the interface is only being used to adjust the max PSTATE
            // range with the applied VF point delta. The input to this interface
            // is actual PSTATE range without any delta adjustment. If you remove
            // the quantization code, it will break the VF point delta adjustment
            //
            freqMHz   = *pFreqMHz;
            freqMHz   = LW_UNSIGNED_DIV_CEIL((freqMHz * 100U), pSecondaryEntry->ratio);
            *pFreqMHz = (LwU16)freqMHz;

            //
            // TODO: This needs to be removed once we have full quantization
            // support in PMU
            //
            if (LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL ==
                clkProg3XFreqSourceGet(pProg3X))
            {
                //
                // Quantize up the primary clk domain freq value. Also skipping the
                // freq delta adjustment for MAX clk prog freq as the input is non
                // offset freq value and we are trying to do the reverse maths here.
                //
                status = clkDomain3XProgFreqAdjustQuantizeMHz(
                            pDomain3XProg,
                            pFreqMHz,   // pFreqMHz
                            LW_FALSE,   // bFloor
                            LW_FALSE);  // bReqFreqDeltaAdj
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto clkProg3XPrimaryFreqTranslateSecondaryToPrimary_RATIO_exit;
                }
            }
            break;
        }
    }

clkProg3XPrimaryFreqTranslateSecondaryToPrimary_RATIO_exit:
    return status;
}

/*!
 * _PRIMARY_RATIO implementation.
 *
 * @copydoc ClkProg3XPrimaryIsPrimaryProgIdxForSecondaryFreqMHz
 */
LwBool
clkProg3XPrimaryIsPrimaryProgIdxForSecondaryFreqMHz_RATIO
(
    CLK_PROG_3X_PRIMARY    *pProg3XPrimary,
    CLK_DOMAIN_3X_PRIMARY  *pDomain3XPrimary,
    LwU8                   secondaryClkDomIdx,
    LwU16                  freqMHz
)
{
    CLK_PROG_3X_PRIMARY_RATIO   *p3XPrimaryRatio =
        CLK_PROG_INTERFACE_TO_INTERFACE_CAST(pProg3XPrimary, 3X_PRIMARY_RATIO);
    CLK_PROG_3X                *pProg3X        =
        (CLK_PROG_3X *)INTERFACE_TO_BOARDOBJ_CAST(pProg3XPrimary);
    LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_RATIO_SECONDARY_ENTRY *pSecondaryEntry;
    LwBool  bIsValidIdx = LW_FALSE;
    LwU8    i;

    if (p3XPrimaryRatio == NULL)
    {
        // status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto clkProg3XPrimaryIsPrimaryProgIdxForSecondaryFreqMHz_RATIO_exit;
    }

    // Loop over the secondary entries to find matching clkDomIdx.
    for (i = 0; i < Clk.progs.secondaryEntryCount; i++)
    {
        pSecondaryEntry = &p3XPrimaryRatio->pSecondaryEntries[i];

        // Found matching secondaryClkDomIdx.
        if (secondaryClkDomIdx == pSecondaryEntry->clkDomIdx)
        {
            if (freqMHz <= ((pSecondaryEntry->ratio * pProg3X->freqMaxMHz)/100U))
            {
                bIsValidIdx = LW_TRUE;
                break;
            }
        }
    }

clkProg3XPrimaryIsPrimaryProgIdxForSecondaryFreqMHz_RATIO_exit:
    return bIsValidIdx;
}

/* ------------------------- Private Functions ------------------------------ */
