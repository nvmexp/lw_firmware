/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "volt/objvolt.h"
#include "dmemovl.h"

/* ------------------------ Global Variables ------------------------------- */
/*!
 * Main structure for all CLK_DOMAIN_PROG_VIRTUAL_TABLE data.
 */
static INTERFACE_VIRTUAL_TABLE ClkDomainProgVirtualTable_3X =
{
    LW_OFFSETOF(CLK_DOMAIN_3X_PROG, prog)   // offset
};

/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_DOMAIN_3X_PROG class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkDomainGrpIfaceModel10ObjSet_3X_PROG
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_CLK_CLK_DOMAIN_3X_PROG_BOARDOBJ_SET *pSet =
        (RM_PMU_CLK_CLK_DOMAIN_3X_PROG_BOARDOBJ_SET *)pBoardObjDesc;
    CLK_DOMAIN_3X_PROG *pDomain;
    FLCN_STATUS         status;

    status = clkDomainGrpIfaceModel10ObjSet_3X(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainGrpIfaceModel10ObjSet_3X_PROG_exit;
    }
    pDomain = (CLK_DOMAIN_3X_PROG *)*ppBoardObj;

    status = clkDomainConstruct_PROG(pBObjGrp,
                &pDomain->prog.super,
                &pSet->prog.super,
                &ClkDomainProgVirtualTable_3X);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainGrpIfaceModel10ObjSet_3X_PROG_exit;
    }

    // Copy the CLK_DOMAIN_3X_PROG-specific data.
    pDomain->clkProgIdxFirst            = pSet->clkProgIdxFirst;
    pDomain->clkProgIdxLast             = pSet->clkProgIdxLast;
    pDomain->bForceNoiseUnawareOrdering = pSet->bForceNoiseUnawareOrdering;
    pDomain->factoryDelta               = pSet->factoryDelta;
    pDomain->freqDeltaMinMHz            = pSet->freqDeltaMinMHz;
    pDomain->freqDeltaMaxMHz            = pSet->freqDeltaMaxMHz;
    pDomain->deltas.freqDelta           = pSet->deltas.freqDelta;

    // Allocate the voltage rails array if not previously allocated.
    if (pDomain->deltas.pVoltDeltauV == NULL)
    {
        pDomain->deltas.pVoltDeltauV =
            lwosCallocType(pBObjGrp->ovlIdxDmem, Clk.domains.voltRailsMax, LwS32);
        if (pDomain->deltas.pVoltDeltauV == NULL)
        {
            status = FLCN_ERR_NO_FREE_MEM;
            PMU_BREAKPOINT();
            goto clkDomainGrpIfaceModel10ObjSet_3X_PROG_exit;
        }
    }
    (void)memcpy(pDomain->deltas.pVoltDeltauV, pSet->deltas.voltDeltauV,
        (sizeof(LwS32) * Clk.domains.voltRailsMax));

clkDomainGrpIfaceModel10ObjSet_3X_PROG_exit:
    return status;
}

/*!
 * @copydoc ClkDomainsCache
 */
FLCN_STATUS
clkDomainsCache_3X
(
    LwBool bVFEEvalRequired
)
{
    CLK_DOMAIN   *pDomain;
    LwBoardObjIdx i;
    FLCN_STATUS   status = FLCN_ERR_NOT_SUPPORTED;

    //
    // Iterate over the _3X_PRIMARY objects to cache their PRIMARY VF lwrves.
    // On VF 3.5+, we are also caching their CLK_PROG @ref offsettedFreqMaxMHz
    //

    // Increment the VF Points cache counter.
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT))
    {
        clkVfPointsVfPointsCacheCounterIncrement();
    }

    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, i,
        &Clk.domains.progDomainsMask.super)
    {
        // Call type-specific implemenation.
        switch (BOARDOBJ_GET_TYPE(pDomain))
        {
            case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY:
            {
                if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_BASE_35))
                {
                    status = clkDomainCache_35_Primary(pDomain, bVFEEvalRequired);
                }
                break;
            }
            case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_PRIMARY:
            {
                if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_30))
                {
                    status = clkDomainCache_30_Primary(pDomain, bVFEEvalRequired);
                }
                break;
            }
            case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_SECONDARY:
            case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY:
            {
                // Nothing to do as secondaries will be cached by their primary.
                status = FLCN_OK;
                break;
            }
            default:
            {
                // Nothing to do.
                break;
            }
        }
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomainsCache_3X_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

clkDomainsCache_3X_exit:
    return status;
}

/*!
 * @copydoc ClkDomainProgVoltToFreq
 */
FLCN_STATUS
clkDomainProgVoltToFreq_3X
(
    CLK_DOMAIN_PROG                    *pDomainProg,
    LwU8                                voltRailIdx,
    LwU8                                voltageType,
    LW2080_CTRL_CLK_VF_INPUT           *pInput,
    LW2080_CTRL_CLK_VF_OUTPUT          *pOutput,
    LW2080_CTRL_CLK_VF_ITERATION_STATE *pVfIterState
)
{
    CLK_DOMAIN_3X_PROG *pDomain3XProg =
        (CLK_DOMAIN_3X_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    //
    // Init to _NOT_SUPPORTED for case where no type-specific implemenation is
    // found below.
    //
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    // Call type-specific implemenation.
    switch (BOARDOBJ_GET_TYPE(pDomain3XProg))
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_PRIMARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY:
        {
            status = clkDomainProgVoltToFreq_3X_PRIMARY(pDomainProg,
                        voltRailIdx, voltageType, pInput, pOutput,
                        pVfIterState);
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_SECONDARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY:
        {
            status = clkDomainProgVoltToFreq_3X_SECONDARY(pDomainProg,
                        voltRailIdx, voltageType, pInput, pOutput,
                        pVfIterState);
            break;
        }
        default:
        {
            // Nothing to do
            break;
        }
    }

    // Trap errors from type-specific implemenations.
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainProgVoltToFreq_3X_exit;
    }

clkDomainProgVoltToFreq_3X_exit:
    return status;
}

/*!
 * @copydoc ClkDomainProgFreqToVolt_3X
 */
FLCN_STATUS
clkDomainProgFreqToVolt_3X
(
    CLK_DOMAIN_PROG                    *pDomainProg,
    LwU8                                voltRailIdx,
    LwU8                                voltageType,
    LW2080_CTRL_CLK_VF_INPUT           *pInput,
    LW2080_CTRL_CLK_VF_OUTPUT          *pOutput,
    LW2080_CTRL_CLK_VF_ITERATION_STATE *pVfIterState
)
{
    CLK_DOMAIN_3X_PROG *pDomain3XProg =
        (CLK_DOMAIN_3X_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);

    //
    // Init to _NOT_SUPPORTED for case where no type-specific implemenation is
    // found below.
    //
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    // Call type-specific implemenation.
    switch (BOARDOBJ_GET_TYPE(pDomain3XProg))
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_PRIMARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY:
        {
            status = clkDomainProgFreqToVolt_3X_PRIMARY(pDomainProg,
                        voltRailIdx, voltageType, pInput, pOutput,
                        pVfIterState);
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_SECONDARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY:
        {
            status = clkDomainProgFreqToVolt_3X_SECONDARY(pDomainProg,
                        voltRailIdx, voltageType, pInput, pOutput,
                        pVfIterState);
            break;
        }
        default:
        {
            // Nothing to do
            break;
        }
    }

    // Trap errors from type-specific implemenations.
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainProgFreqToVolt_3X_exit;
    }

clkDomainProgFreqToVolt_3X_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
