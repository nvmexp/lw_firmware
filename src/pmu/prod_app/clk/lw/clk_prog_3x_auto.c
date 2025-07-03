/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file clk_prog_3x_auto.c
 *
 * @brief Module managing all state related to the CLK_PROG_3X structure.  This
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
 * @copydoc ClkProg3XFreqMHzQuantize
 */
FLCN_STATUS
clkProg3XFreqMHzQuantize
(
    CLK_PROG_3X         *pProg3X,
    CLK_DOMAIN_3X_PROG  *pDomain3XProg,
    LwU16               *pFreqMHz,
    LwBool               bFloor
)
{
    //
    // On AUTO, we lwrrently only support NAFLL clock domains. AFA frequency
    // quantization is concerned, we do not need to quantize the non NAFLL
    // clock domains even if they are supported. So following implementation
    // is good enough based on AUTO POR and current HW capabilities.
    //
    if (pProg3X->source != LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL)
    {
        return FLCN_OK;
    }

    //
    // Call into NAFLL code for frequency quantization.
    // This only supports NAFLL clocks.
    //
    return clkNafllFreqMHzQuantize(pDomain3XProg->super.super.apiDomain, pFreqMHz, bFloor);
}

/* ------------------------- Private Functions ------------------------------ */
