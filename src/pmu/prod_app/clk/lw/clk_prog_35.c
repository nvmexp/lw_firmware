/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file clk_prog_35.c
 *
 * @brief Module managing all state related to the CLK_PROG_35 structure.  This
 * structure defines how to program a given frequency on given CLK_DOMAIN -
 * including which frequency generator to use and potentially the necessary VF
 * mapping, per the VBIOS Clock Programming Table 1.0 specification.
 *
 */
//! https://wiki.lwpu.com/engwiki/index.php/Resman/PState/Data_Tables/Clock_Programming_Table/1.0_Spec

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * @copydoc clkProg35CacheOffsettedFreqMaxMHz
 */
FLCN_STATUS
clkProg35CacheOffsettedFreqMaxMHz
(
    CLK_PROG_35        *pProg35,
    CLK_DOMAIN_3X_PROG *pDomain3XProg
)
{
    FLCN_STATUS status;

    // Reset the offseted max frequency value.
    pProg35->offsettedFreqMaxMHz = pProg35->super.freqMaxMHz;

    status = clkDomainProgClientFreqDeltaAdj_3X(
                CLK_DOMAIN_PROG_GET_FROM_3X_PROG(pDomain3XProg),
                &pProg35->offsettedFreqMaxMHz); // pFreqMHz
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkProg35CacheOffsettedFreqMaxMHz_exit;
    }

clkProg35CacheOffsettedFreqMaxMHz_exit:
    return status;
}

/*!
 * @copydoc clkProg3XFreqMaxAdjAndQuantize
 */
FLCN_STATUS
clkProg3XFreqMaxAdjAndQuantize_35
(
    CLK_PROG_3X         *pProg3X,
    CLK_DOMAIN_3X_PROG  *pDomain3XProg,
    LwU16               *pFreqMHz
)
{
    CLK_PROG_35 *pProg35 = (CLK_PROG_35 *)pProg3X;

    if (pProg35->offsettedFreqMaxMHz == 0U)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_STATE;

    }
    *pFreqMHz = pProg35->offsettedFreqMaxMHz;

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
