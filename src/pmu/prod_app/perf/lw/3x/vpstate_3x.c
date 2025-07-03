/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    vpstate_3x.c
 * @copydoc vpstate_3x.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

#include "dmemovl.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/3x/vpstate_3x.h"
#include "pmu_objperf.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* -------------------------- Compile time checks --------------------------- */
ct_assert((PERF_DOMAIN_GROUP_MAX_GROUPS <= 3) && (PERF_DOMAIN_GROUP_GPC2CLK == 1) && (PERF_DOMAIN_GROUP_XBARCLK == 2));
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
vpstateGrpIfaceModel10ObjSetImpl_3X
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PERF_VPSTATE_3X *pSet;
    VPSTATE_3X             *pVpstate3x;
    FLCN_STATUS             status;
    LwU32                   i;

    // Call super-class constructor.
    status = vpstateGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto vpstateGrpIfaceModel10ObjSetImpl_3X_exit;
    }
    pVpstate3x = (VPSTATE_3X *)*ppBoardObj;
    pSet       = (RM_PMU_PERF_VPSTATE_3X *)pBoardObjDesc;

    pVpstate3x->pstateIdx      = pSet->pstateIdx;
    pVpstate3x->clkDomainsMask = pSet->clkDomainsMask;

    //
    // Allocate the memory for clocks specified by this Vpstate,
    // if not already allocated.
    //
    if (pVpstate3x->pClocks == NULL)
    {
        pVpstate3x->pClocks =
            lwosCallocType(pBObjGrp->ovlIdxDmem, Perf.vpstates.nDomainGroups,
                           VPSTATE_3X_CLOCK_ENTRY);
        if (pVpstate3x->pClocks == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NO_FREE_MEM;
            goto vpstateGrpIfaceModel10ObjSetImpl_3X_exit;
        }
    }

    for (i = 0; i < Perf.vpstates.nDomainGroups; i++)
    {
        pVpstate3x->pClocks[i].targetFreqMHz = pSet->clocks[i].targetFreqMHz;
        pVpstate3x->pClocks[i].minEffFreqMHz = pSet->clocks[i].minEffFreqMHz;
    }

vpstateGrpIfaceModel10ObjSetImpl_3X_exit:
    return status;
}

/*!
 * @copydoc@ VpstateDomGrpGet
 */
FLCN_STATUS
vpstateDomGrpGet_3X
(
    VPSTATE    *pVpstate,
    LwU8        domGrpIdx,
    LwU32      *pValue
)
{
    VPSTATE_3X *pVpstate3x = (VPSTATE_3X *)pVpstate;

    if (domGrpIdx == RM_PMU_DOMAIN_GROUP_PSTATE)
    {
        *pValue = pVpstate3x->pstateIdx;
        goto VpstateDomGrpGet_exit;
    }

    // To Do: Create the clk domain helper function to colwert the domGrpIdx
    *pValue = pVpstate3x->pClocks[(domGrpIdx - 1U)].targetFreqMHz * 1000U;

VpstateDomGrpGet_exit:
    return FLCN_OK;
}

/*!
 * @copydoc Vpstate3xClkDomainsMaskGet
 */
FLCN_STATUS
vpstate3xClkDomainsMaskGet
(
    VPSTATE_3X          *pVpstate3x,
    BOARDOBJGRPMASK_E32 *pClkDomainsMask
)
{
    // KO-TODO: change VPSTATE_3X::clkDomainsMask to LW2080_CTRL_BOARDOBJGRP_MASK_E32
    return boardObjGrpMaskImport_E32(pClkDomainsMask,
            (LW2080_CTRL_BOARDOBJGRP_MASK_E32 *)&pVpstate3x->clkDomainsMask);
}

/*!
 * @copydoc Vpstate3xClkDomainGet
 */
FLCN_STATUS
vpstate3xClkDomainGet
(
    VPSTATE_3X             *pVpstate3x,
    LwU8                    clkDomainIdx,
    VPSTATE_3X_CLOCK_ENTRY *pVpstateClkEntry
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check the requested CLK_DOMAIN index.
    if ((BIT(clkDomainIdx) & pVpstate3x->clkDomainsMask) != BIT(clkDomainIdx))
    {
        //
        // No breakpoint here. Caller can query any clock domain. If the clock
        // domain is not defined for the Vpstate, the caller will handle the error.
        //
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto vpstate3xClkDomainGet_exit;
    }

    // Return the requested VPSTATE_3X_CLK_ENTRY into the caller's structure.
    *pVpstateClkEntry = pVpstate3x->pClocks[clkDomainIdx];

vpstate3xClkDomainGet_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
