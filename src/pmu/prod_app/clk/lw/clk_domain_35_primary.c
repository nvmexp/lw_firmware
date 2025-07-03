/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "dmemovl.h"

/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * @copydoc ClkDomainProgClientFreqDeltaAdj
 */
FLCN_STATUS
clkDomainProgClientFreqDeltaAdj_35_PRIMARY
(
    CLK_DOMAIN_PROG     *pDomainProg,
    LwU16               *pFreqMHz
)
{
    CLK_DOMAIN_3X_PROG   *pDomain3XProg   =
        (CLK_DOMAIN_3X_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    CLK_DOMAIN_35_PRIMARY *pDomain35Primary = (CLK_DOMAIN_35_PRIMARY *)pDomain3XProg;
    CLK_PROG_35_PRIMARY   *pProg35Primary;
    LwU8                  progIdx;
    FLCN_STATUS           status          = FLCN_OK;

    progIdx = clkDomain3XProgGetClkProgIdxFromFreq(
                pDomain3XProg,  // pDomain3XProg
                *pFreqMHz,      // freqMHz
                LW_FALSE);      // bReqFreqDeltaAdj

    pProg35Primary = (CLK_PROG_35_PRIMARY *)BOARDOBJGRP_OBJ_GET(CLK_PROG, progIdx);
    if (pProg35Primary == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto clkDomainProgClientFreqDeltaAdj_35_PRIMARY_exit;
    }

    status = clkProg35PrimaryClientFreqDeltaAdj(
                                        pProg35Primary,
                                        pDomain35Primary,
                                        BOARDOBJ_GET_GRP_IDX_8BIT(&pDomain35Primary->super.super.super.super.super.super),
                                        pFreqMHz);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainProgClientFreqDeltaAdj_35_PRIMARY_exit;
    }

clkDomainProgClientFreqDeltaAdj_35_PRIMARY_exit:
    return status;
}

/*!
 * _35_PRIMARY implementation.
 *
 * @copydoc ClkDomain3XProgVoltAdjustDeltauV
 */
FLCN_STATUS
clkDomain3XProgVoltAdjustDeltauV_35_PRIMARY
(
    CLK_DOMAIN_3X_PROG  *pDomain3XProg,
    CLK_PROG_3X_PRIMARY  *pProg3XPrimary,
    LwU8                 voltRailIdx,
    LwU32               *pVoltuV
)
{
    FLCN_STATUS status;

    // Apply the global voltage offset.
    status = clkDomain3XProgVoltAdjustDeltauV_PRIMARY(pDomain3XProg,
                pProg3XPrimary, voltRailIdx, pVoltuV);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomain3XProgVoltAdjustDeltauV_35_PRIMARY_exit;
    }

    // Apply the CLK_DOMAIN_35_PROG object's por voltage offset.
    *pVoltuV = clkVoltDeltaAdjust(*pVoltuV,
                clkDomain35ProgPorVoltDeltauVGet((CLK_DOMAIN_35_PROG *)pDomain3XProg,
                                                 voltRailIdx));

clkDomain3XProgVoltAdjustDeltauV_35_PRIMARY_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */

