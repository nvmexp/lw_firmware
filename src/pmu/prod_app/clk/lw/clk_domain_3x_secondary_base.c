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

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_DOMAIN_3X_SECONDARY class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkDomainConstruct_3X_SECONDARY
(
    BOARDOBJGRP                *pBObjGrp,
    BOARDOBJ_INTERFACE         *pInterface,
    RM_PMU_BOARDOBJ_INTERFACE  *pInterfaceDesc,
    INTERFACE_VIRTUAL_TABLE    *pVirtualTable
)
{
    RM_PMU_CLK_CLK_DOMAIN_3X_SECONDARY_BOARDOBJ_SET *pSet =
        (RM_PMU_CLK_CLK_DOMAIN_3X_SECONDARY_BOARDOBJ_SET *)pInterfaceDesc;
    CLK_DOMAIN_3X_SECONDARY    *p3XSecondary;
    FLCN_STATUS             status;

    status = boardObjInterfaceConstruct(pBObjGrp, pInterface, pInterfaceDesc, pVirtualTable);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainConstruct_3X_SECONDARY_exit;
    }
    p3XSecondary = (CLK_DOMAIN_3X_SECONDARY *)pInterface;

    // Copy the CLK_DOMAIN_3X_SECONDARY-specific data.
    p3XSecondary->primaryIdx = pSet->primaryIdx;

clkDomainConstruct_3X_SECONDARY_exit:
    return status;
}

/*!
 * _SECONDARY implemenation.
 *
 * @copydoc ClkDomainProgVoltToFreq
 */
FLCN_STATUS
clkDomainProgVoltToFreq_3X_SECONDARY
(
    CLK_DOMAIN_PROG                    *pDomainProg,
    LwU8                                voltRailIdx,
    LwU8                                voltageType,
    LW2080_CTRL_CLK_VF_INPUT           *pInput,
    LW2080_CTRL_CLK_VF_OUTPUT          *pOutput,
    LW2080_CTRL_CLK_VF_ITERATION_STATE *pVfIterState
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
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto clkDomainProgVoltToFreq_3X_SECONDARY_exit;
    }

    pDomain = BOARDOBJGRP_OBJ_GET(CLK_DOMAIN, pDomain3XSecondary->primaryIdx);
    if (pDomain == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto clkDomainProgVoltToFreq_3X_SECONDARY_exit;
    }

    pDomain3XPrimary = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, 3X_PRIMARY);
    if (pDomain3XPrimary == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto clkDomainProgVoltToFreq_3X_SECONDARY_exit;
    }

    if ((PMUCFG_FEATURE_ENABLED(PMU_CLK_VF_LOOK_UP_USING_BINARY_SEARCH)) &&
        (pVfIterState == NULL))
    {
        status = clkDomain3XPrimaryVoltToFreqBinarySearch(
                    pDomain3XPrimary,                        // pDomain3XPrimary
                    BOARDOBJ_GET_GRP_IDX_8BIT(&pDomain3XProg->super.super.super.super),    // clkDomIdx
                    voltRailIdx,                            // voltRailIdx
                    voltageType,                            // voltageType
                    pInput,                                 // pInput
                    pOutput);                               // pOutput
        goto clkDomainProgVoltToFreq_3X_SECONDARY_exit;
    }

    status = clkDomain3XPrimaryVoltToFreqLinearSearch(
                pDomain3XPrimary,                        // pDomain3XPrimary
                BOARDOBJ_GET_GRP_IDX_8BIT(&pDomain3XProg->super.super.super.super),    // clkDomIdx
                voltRailIdx,                            // voltRailIdx
                voltageType,                            // voltageType
                pInput,                                 // pInput
                pOutput,                                // pOutput
                pVfIterState);                          // pVfIterState

clkDomainProgVoltToFreq_3X_SECONDARY_exit:
    return status;
}

/*!
 * _SECONDARY implemenation.
 *
 * @copydoc ClkDomainProgFreqToVolt
 */
FLCN_STATUS
clkDomainProgFreqToVolt_3X_SECONDARY
(
    CLK_DOMAIN_PROG                    *pDomainProg,
    LwU8                                voltRailIdx,
    LwU8                                voltageType,
    LW2080_CTRL_CLK_VF_INPUT           *pInput,
    LW2080_CTRL_CLK_VF_OUTPUT          *pOutput,
    LW2080_CTRL_CLK_VF_ITERATION_STATE *pVfIterState
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
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto clkDomainProgFreqToVolt_3X_SECONDARY_exit;
    }

    pDomain = BOARDOBJGRP_OBJ_GET(CLK_DOMAIN, pDomain3XSecondary->primaryIdx);
    if (pDomain == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto clkDomainProgFreqToVolt_3X_SECONDARY_exit;
    }

    pDomain3XPrimary = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, 3X_PRIMARY);
    if (pDomain3XPrimary == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto clkDomainProgFreqToVolt_3X_SECONDARY_exit;
    }

    if ((PMUCFG_FEATURE_ENABLED(PMU_CLK_VF_LOOK_UP_USING_BINARY_SEARCH)) &&
        (pVfIterState == NULL))
    {
        status = clkDomain3XPrimaryFreqToVoltBinarySearch(
            pDomain3XPrimary,                        // pDomain3XPrimary
            BOARDOBJ_GET_GRP_IDX_8BIT(&pDomain3XProg->super.super.super.super),    // clkDomIdx
            voltRailIdx,                            // voltRailIdx
            voltageType,                            // voltageType
            pInput,                                 // pInput
            pOutput);                               // pOutput
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT(); 
            goto clkDomainProgFreqToVolt_3X_SECONDARY_exit;
        }
    }
    else
    {
        status = clkDomain3XPrimaryFreqToVoltLinearSearch(
            pDomain3XPrimary,                        // pDomain3XPrimary
            BOARDOBJ_GET_GRP_IDX_8BIT(&pDomain3XProg->super.super.super.super),    // clkDomIdx
            voltRailIdx,                            // voltRailIdx
            voltageType,                            // voltageType
            pInput,                                 // pInput
            pOutput,                                // pOutput
            pVfIterState);                          // pVfIterState
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomainProgFreqToVolt_3X_SECONDARY_exit;
        }
    }

clkDomainProgFreqToVolt_3X_SECONDARY_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
