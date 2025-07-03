/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "g_pmurpc.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_DOMAIN_35_PROG class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkDomainGrpIfaceModel10ObjSet_35_PROG
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_CLK_CLK_DOMAIN_35_PROG_BOARDOBJ_SET *pSet =
        (RM_PMU_CLK_CLK_DOMAIN_35_PROG_BOARDOBJ_SET *)pBoardObjDesc;
    CLK_DOMAIN_35_PROG *pDomain;
    FLCN_STATUS         status;

    status = clkDomainGrpIfaceModel10ObjSet_3X_PROG(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainGrpIfaceModel10ObjSet_35_PROG_exit;
    }
    pDomain = (CLK_DOMAIN_35_PROG *)*ppBoardObj;

    // Copy the CLK_DOMAIN_35_PROG-specific data.
    pDomain->preVoltOrderingIndex               = pSet->preVoltOrderingIndex;
    pDomain->postVoltOrderingIndex              = pSet->postVoltOrderingIndex;
    pDomain->clkPos                             = pSet->clkPos;
    pDomain->clkVFLwrveCount                    = pSet->clkVFLwrveCount;
    pDomain->clkMonInfo.lowThresholdVfeIdx      = pSet->clkMonInfo.lowThresholdVfeIdx;
    pDomain->clkMonInfo.highThresholdVfeIdx     = pSet->clkMonInfo.highThresholdVfeIdx;
    pDomain->clkMonCtrl.lowThresholdOverride    = pSet->clkMonCtrl.lowThresholdOverride;
    pDomain->clkMonCtrl.highThresholdOverride   = pSet->clkMonCtrl.highThresholdOverride;

    // Allocate the array of size equal to max voltage rails
    if (pDomain->pPorVoltDeltauV == NULL)
    {
        pDomain->pPorVoltDeltauV =
            lwosCallocType(pBObjGrp->ovlIdxDmem, Clk.domains.voltRailsMax, LwS32);
        if (pDomain->pPorVoltDeltauV == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NO_FREE_MEM;
            goto clkDomainGrpIfaceModel10ObjSet_35_PROG_exit;
        }
    }
    (void)memcpy(pDomain->pPorVoltDeltauV, pSet->porVoltDeltauV,
        (sizeof(LwS32) * Clk.domains.voltRailsMax));

clkDomainGrpIfaceModel10ObjSet_35_PROG_exit:
    return status;
}

/*!
 * @copydoc ClkDomainProgVoltToFreqTuple
 */
FLCN_STATUS
clkDomainProgVoltToFreqTuple_35
(
    CLK_DOMAIN_PROG                                *pDomainProg,
    LwU8                                            voltRailIdx,
    LwU8                                            voltageType,
    LW2080_CTRL_CLK_VF_INPUT                       *pInput,
    LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE    *pOutput,
    LW2080_CTRL_CLK_VF_ITERATION_STATE             *pVfIterState
)
{
    CLK_DOMAIN_35_PROG *pDomain35Prog =
        (CLK_DOMAIN_35_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);

    //
    // Init to _NOT_SUPPORTED for case where no type-specific implemenation is
    // found below.
    //
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    if (pVfIterState == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkDomain35ProgVoltToFreqTuple_exit;
    }

    // Init the output struct
    memset(pOutput, 0, sizeof(LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE));

    // Call type-specific implemenation.
    switch (BOARDOBJ_GET_TYPE(pDomain35Prog))
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY:
        {
            status = clkDomainProgVoltToFreqTuple_35_PRIMARY(pDomainProg,
                        voltRailIdx,
                        voltageType,
                        pInput,
                        pOutput,
                        pVfIterState);
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY:
        {
            status = clkDomainProgVoltToFreqTuple_35_SECONDARY(pDomainProg,
                        voltRailIdx,
                        voltageType,
                        pInput,
                        pOutput,
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
        goto clkDomain35ProgVoltToFreqTuple_exit;
    }

clkDomain35ProgVoltToFreqTuple_exit:
    return status;
}

/*!
 * @copydoc ClkDomainProgPreVoltOrderingIndexGet
 */
LwU8
clkDomainProgPreVoltOrderingIndexGet_35
(
    CLK_DOMAIN_PROG    *pDomainProg
)
{
    CLK_DOMAIN_35_PROG *pDomain35Prog =
        (CLK_DOMAIN_35_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);

    return pDomain35Prog->preVoltOrderingIndex;
}

/*!
 * @copydoc ClkDomainProgPostVoltOrderingIndexGet
 */
LwU8
clkDomainProgPostVoltOrderingIndexGet_35
(
    CLK_DOMAIN_PROG    *pDomainProg
)
{
    CLK_DOMAIN_35_PROG *pDomain35Prog =
        (CLK_DOMAIN_35_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);

    return pDomain35Prog->postVoltOrderingIndex;
}

/* ------------------------- Private Functions ------------------------------ */
