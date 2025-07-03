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
 * @file clk_prog_30.c
 *
 * @brief Module managing all state related to the CLK_PROG_30 structure.  This
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
 * @copydoc clkProg3XFreqMaxAdjAndQuantize
 */
FLCN_STATUS
clkProg3XFreqMaxAdjAndQuantize_30
(
    CLK_PROG_3X         *pProg3X,
    CLK_DOMAIN_3X_PROG  *pDomain3XProg,
    LwU16               *pFreqMHz
)
{
    FLCN_STATUS  status  = FLCN_OK;

    // Adjust the max freq with the applied freq delta values.
    status = clkDomain3XProgFreqAdjustDeltaMHz(pDomain3XProg,
                pFreqMHz,        // pFreqMHz
                LW_FALSE,        // bQuantize
                LW_TRUE);        // bVFOCAdjReq
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkProg3XFreqMaxAdjAndQuantize_30_exit;
    }

    //
    // Quantize based on the parameters corresponding to the SOURCE of this
    // CLK_PROG_3X object.
    //
    status = clkProg3XFreqMHzQuantize(pProg3X,          // pProg3X
                                      pDomain3XProg,    // pDomain3XProg
                                      pFreqMHz,         // pFreqMHz
                                      LW_TRUE);         // bFloor
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkProg3XFreqMaxAdjAndQuantize_30_exit;
    }

clkProg3XFreqMaxAdjAndQuantize_30_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
