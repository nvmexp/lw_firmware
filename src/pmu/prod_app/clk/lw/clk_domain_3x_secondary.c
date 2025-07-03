/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * _3X_SECONDARY implementation
 *
 * @copydoc ClkDomainProgMaxFreqMHzGet
 */
FLCN_STATUS
clkDomainProgMaxFreqMHzGet_3X_SECONDARY
(
    CLK_DOMAIN_PROG        *pDomainProg,
    LwU32                  *pFreqMaxMHz
)
{
    CLK_DOMAIN_3X_PROG     *pDomain3XProg   =
        (CLK_DOMAIN_3X_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    CLK_DOMAIN_3X_SECONDARY    *pDomain3XSecondary;
    CLK_DOMAIN             *pDomain;
    CLK_DOMAIN_3X_PRIMARY   *pDomain3XPrimary;
    FLCN_STATUS             status;

    pDomain3XSecondary = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain3XProg, 3X_SECONDARY);
    if (pDomain3XSecondary == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto clkDomainProgMaxFreqMHzGet_3X_SECONDARY_exit;
    }

    pDomain = BOARDOBJGRP_OBJ_GET(CLK_DOMAIN, pDomain3XSecondary->primaryIdx);
    if (pDomain == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto clkDomainProgMaxFreqMHzGet_3X_SECONDARY_exit;
    }

    pDomain3XPrimary = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, 3X_PRIMARY);
    if (pDomain3XPrimary == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto clkDomainProgMaxFreqMHzGet_3X_SECONDARY_exit;
    }

    //
    // Call into PRIMARY interface for this SECONDARY domain with secondary domain index
    // corresponding to this SECONDARY domain.
    //
    status = clkDomain3XPrimaryMaxFreqMHzGet(pDomain3XPrimary,
        BOARDOBJ_GET_GRP_IDX_8BIT(&pDomain3XProg->super.super.super.super),
        pFreqMaxMHz);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainProgMaxFreqMHzGet_3X_SECONDARY_exit;
    }

clkDomainProgMaxFreqMHzGet_3X_SECONDARY_exit:
    return status;
}

/*!
 * @copydoc ClkDomain3XProgFreqAdjustDeltaMHz
 */
FLCN_STATUS
clkDomain3XProgFreqAdjustDeltaMHz_SECONDARY
(
    CLK_DOMAIN_3X_PROG  *pDomain3XProg,
    LwU16               *pFreqMHz,
    LwBool               bQuantize,
    LwBool               bVFOCAdjReq
)
{
    CLK_DOMAIN_3X_SECONDARY  *pDomain3XSecondary;
    CLK_DOMAIN           *pDomain;
    CLK_PROG             *pProg3X;
    CLK_DOMAIN_3X_PRIMARY *pDomain3XPrimary;
    CLK_PROG_3X_PRIMARY   *pProg3XPrimary;
    LwU8                  progIdx;
    FLCN_STATUS           status = FLCN_OK;

    pDomain3XSecondary = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain3XProg, 3X_SECONDARY);
    if (pDomain3XSecondary == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto clkDomain3XProgFreqAdjustDeltaMHz_SECONDARY_exit;
    }

    pDomain = BOARDOBJGRP_OBJ_GET(CLK_DOMAIN, pDomain3XSecondary->primaryIdx);
    if (pDomain == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto clkDomain3XProgFreqAdjustDeltaMHz_SECONDARY_exit;
    }

    pDomain3XPrimary = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, 3X_PRIMARY);
    if (pDomain3XPrimary == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto clkDomain3XProgFreqAdjustDeltaMHz_SECONDARY_exit;
    }

    progIdx = clkDomain3XPrimaryGetPrimaryProgIdxFromSecondaryFreqMHz(pDomain3XPrimary,
                BOARDOBJ_GET_GRP_IDX_8BIT(&pDomain3XProg->super.super.super.super), *pFreqMHz);

    pProg3X = BOARDOBJGRP_OBJ_GET(CLK_PROG, progIdx);
    if (pProg3X == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto clkDomain3XProgFreqAdjustDeltaMHz_SECONDARY_exit;
    }

    pProg3XPrimary = CLK_PROG_BOARDOBJ_TO_INTERFACE_CAST(pProg3X, 3X_PRIMARY);
    if (pProg3XPrimary == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto clkDomain3XProgFreqAdjustDeltaMHz_SECONDARY_exit;
    }

    status = clkDomain3XSecondaryFreqAdjustDeltaMHz(pDomain3XSecondary,
                pProg3XPrimary, LW_TRUE, bQuantize, bVFOCAdjReq, pFreqMHz);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomain3XProgFreqAdjustDeltaMHz_SECONDARY_exit;
    }

clkDomain3XProgFreqAdjustDeltaMHz_SECONDARY_exit:
    return status;
}

/*!
 * @copydoc ClkDomain3XSecondaryFreqAdjustDeltaMHz
 */
FLCN_STATUS
clkDomain3XSecondaryFreqAdjustDeltaMHz
(
    CLK_DOMAIN_3X_SECONDARY  *pDomain3XSecondary,
    CLK_PROG_3X_PRIMARY   *pProg3XPrimary,
    LwBool                bIncludePrimary,
    LwBool                bQuantize,
    LwBool                bVFOCAdjReq,
    LwU16                *pFreqMHz
)
{
    CLK_DOMAIN_3X_PROG *pDomain3XProg =
        (CLK_DOMAIN_3X_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomain3XSecondary);
    LwU16               freqOrigMHz   = *pFreqMHz;
    FLCN_STATUS         status        = FLCN_OK;

    // If OVOC is not enabled for given pProg3XPrimary, short circuit for early return.
    if (!clkProg3XPrimaryOVOCEnabled(pProg3XPrimary))
    {
        goto clkDomain3XSecondaryFreqAdjustDeltaMHz_exit;
    }

    // If requested, add in CLK_DOMAIN_3X_PRIMARY and CLK_PROG_3X_PRIMARY offsets.
    if (bIncludePrimary &&
        (LW2080_CTRL_CLK_DOMAIN_PCIEGENCLK != clkDomainApiDomainGet(pDomain3XProg)))
    {
        CLK_DOMAIN           *pDomain =
            BOARDOBJGRP_OBJ_GET(CLK_DOMAIN, pDomain3XSecondary->primaryIdx);
        CLK_DOMAIN_3X_PRIMARY *pDomain3XPrimary;

        if (pDomain == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkDomain3XSecondaryFreqAdjustDeltaMHz_exit;
        }

        pDomain3XPrimary = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, 3X_PRIMARY);
        if (pDomain3XPrimary == NULL)
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto clkDomain3XSecondaryFreqAdjustDeltaMHz_exit;
        }

        // colwert the secondary freq to corresponding primary freq value
        status = clkProg3XPrimaryFreqTranslateSecondaryToPrimary(pProg3XPrimary,
                        pDomain3XPrimary,
                        BOARDOBJ_GET_GRP_IDX_8BIT(&pDomain3XProg->super.super.super.super),
                        pFreqMHz);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomain3XSecondaryFreqAdjustDeltaMHz_exit;
        }

        //
        // Pass the bQuantize flag, we want the primary delta quatization to
        // maintain the ratio relationship. This will make the behavior
        // similar to _clkProg3XPrimaryVfPointAccessor.
        // Offset Primary -> quantize -> get secondary -> quantize
        //
        status = clkDomain3XPrimaryFreqAdjustDeltaMHz(pDomain3XPrimary,
                    pProg3XPrimary, bQuantize, bVFOCAdjReq, pFreqMHz);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomain3XSecondaryFreqAdjustDeltaMHz_exit;
        }

        // colwert the primary freq to corresponding secondary freq value
        status = clkProg3XPrimaryFreqTranslatePrimaryToSecondary(pProg3XPrimary,
                        pDomain3XPrimary,                        // pDomain3XPrimary
                        BOARDOBJ_GET_GRP_IDX_8BIT(&pDomain3XProg->super.super.super.super), // clkDomIdx
                        pFreqMHz,                               // pFreqMHz
                        LW_FALSE,                               // bReqFreqDeltaAdj
                        LW_FALSE);                              // Quantization will be done at the end 
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomain3XSecondaryFreqAdjustDeltaMHz_exit;
        }
    }

    // Offset by the CLK_DOMAIN_3X_PROG domain offset.
    status = clkDomain3XProgFreqAdjustDeltaMHz_SUPER(pDomain3XProg,
                pFreqMHz, bQuantize, bVFOCAdjReq);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomain3XSecondaryFreqAdjustDeltaMHz_exit;
    }

    // Quantize the result to the frequencies supported by the CLK_DOMAIN.
    if (bQuantize &&
        (freqOrigMHz != *pFreqMHz))
    {
        status = clkDomain3XProgFreqAdjustQuantizeMHz(pDomain3XProg,
                    pFreqMHz,   // pFreqMHz
                    LW_TRUE,    // bFloor
                    LW_TRUE);   // bReqFreqDeltaAdj
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomain3XSecondaryFreqAdjustDeltaMHz_exit;
        }
    }

clkDomain3XSecondaryFreqAdjustDeltaMHz_exit:
    return status;
}

/*!
 * _3X_SECONDARY implementation.
 *
 * @copydoc ClkDomain3XProgVoltAdjustDeltauV
 */
FLCN_STATUS
clkDomain3XProgVoltAdjustDeltauV_SECONDARY
(
    CLK_DOMAIN_3X_PROG  *pDomain3XProg,
    CLK_PROG_3X_PRIMARY  *pProg3XPrimary,
    LwU8                 voltRailIdx,
    LwU32               *pVoltuV
)
{
    CLK_DOMAIN_3X_SECONDARY *pDomain3XSecondary;
    CLK_DOMAIN_3X_PROG  *pDomain3XProgPrimary;
    FLCN_STATUS          status = FLCN_OK;

    pDomain3XSecondary = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain3XProg, 3X_SECONDARY);
    if (pDomain3XSecondary == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto clkDomain3XProgVoltAdjustDeltauV_SECONDARY_exit;
    }

    pDomain3XProgPrimary =
        (CLK_DOMAIN_3X_PROG *)BOARDOBJGRP_OBJ_GET(CLK_DOMAIN, pDomain3XSecondary->primaryIdx);
    if (pDomain3XProgPrimary == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto clkDomain3XProgVoltAdjustDeltauV_SECONDARY_exit;
    }

    //
    // Call into the PRIMARY interface to handle the global, _3X_PRIMARY, and
    // CLK_PROG_3X_PRIMARY offsets.
    //
    status = clkDomain3XProgVoltAdjustDeltauV_PRIMARY(pDomain3XProgPrimary,
                pProg3XPrimary, voltRailIdx, pVoltuV);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomain3XProgVoltAdjustDeltauV_SECONDARY_exit;
    }

    // Apply _3X_SECONDARY offset
    *pVoltuV = clkVoltDeltaAdjust(*pVoltuV,
                clkDomain3XProgVoltDeltauVGet(pDomain3XProg, voltRailIdx));

clkDomain3XProgVoltAdjustDeltauV_SECONDARY_exit:
    return status;
}

/*!
 * @copydoc ClkDomain3XProgIsOVOCEnabled
 */
LwBool
clkDomain3XProgIsOVOCEnabled_3X_SECONDARY
(
    CLK_DOMAIN_3X_PROG *pDomain3XProg
)
{
    CLK_DOMAIN_3X_PROG     *pDomain3XProgPrimary;
    CLK_DOMAIN_3X_SECONDARY    *pDomain3XSecondary;

    pDomain3XSecondary = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain3XProg, 3X_SECONDARY);
    if (pDomain3XSecondary == NULL)
    {
        // status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        // goto clkDomain3XProgIsOVOCEnabled_3X_SECONDARY_exit;
        return LW_FALSE;
    }

    //
    // Debug mode disables all OCOV capabilities.
    // OCOV is NOT supported on PCIEGENCLK
    //
    if ((clkDomainsDebugModeEnabled())                  ||
        (LW2080_CTRL_CLK_DOMAIN_PCIEGENCLK ==
            clkDomainApiDomainGet(pDomain3XProg)))
    {
        return LW_FALSE;
    }

    // Check if secondary clock domain supports OVOC
    if (Clk.domains.bOverrideOVOC                       || 
        (((pDomain3XProg)->freqDeltaMinMHz != 0)        || 
         ((pDomain3XProg)->freqDeltaMaxMHz != 0)))
    {
        return LW_TRUE; 
    }

    // Check if primary clock domain supports OVOC
    pDomain3XProgPrimary =
        (CLK_DOMAIN_3X_PROG *)BOARDOBJGRP_OBJ_GET(CLK_DOMAIN,
            pDomain3XSecondary->primaryIdx);

    if (pDomain3XProgPrimary == NULL)
    {
        PMU_BREAKPOINT();
        return LW_FALSE;
    }

    if (((pDomain3XProgPrimary)->freqDeltaMinMHz != 0)     || 
        ((pDomain3XProgPrimary)->freqDeltaMaxMHz != 0))
    {
        return LW_TRUE; 
    }

    return LW_FALSE;
}

/* ------------------------- Private Functions ------------------------------ */
