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
 * CLK_DOMAIN_3X_PRIMARY class constructor.
 *
 * @copydoc BoardObjInterfaceConstruct
 */
FLCN_STATUS
clkDomainConstruct_3X_PRIMARY
(
    BOARDOBJGRP                *pBObjGrp,
    BOARDOBJ_INTERFACE         *pInterface,
    RM_PMU_BOARDOBJ_INTERFACE  *pInterfaceDesc,
    INTERFACE_VIRTUAL_TABLE    *pVirtualTable
)
{
    RM_PMU_CLK_CLK_DOMAIN_3X_PRIMARY_OBJECT_SET *pSet =
        (RM_PMU_CLK_CLK_DOMAIN_3X_PRIMARY_OBJECT_SET *)pInterfaceDesc;
    CLK_DOMAIN_3X_PRIMARY   *p3XPrimary;
    FLCN_STATUS             status;

    status = boardObjInterfaceConstruct(pBObjGrp, pInterface, pInterfaceDesc, pVirtualTable);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainConstruct_3X_PRIMARY_exit;
    }
    p3XPrimary = (CLK_DOMAIN_3X_PRIMARY *)pInterface;

    // Copy the CLK_DOMAIN_30_PRIMARY-specific data.
    boardObjGrpMaskInit_E32(&(p3XPrimary->secondaryIdxsMask));
    status = boardObjGrpMaskImport_E32(&(p3XPrimary->secondaryIdxsMask),
                                       &(pSet->secondaryIdxsMask));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainConstruct_3X_PRIMARY_exit;
    }

clkDomainConstruct_3X_PRIMARY_exit:
    return status;
}

/*!
 * _PRIMARY implemenation.
 *
 * @copydoc ClkDomainProgVoltToFreq
 */
FLCN_STATUS
clkDomainProgVoltToFreq_3X_PRIMARY
(
    CLK_DOMAIN_PROG                    *pDomainProg,
    LwU8                                voltRailIdx,
    LwU8                                voltageType,
    LW2080_CTRL_CLK_VF_INPUT           *pInput,
    LW2080_CTRL_CLK_VF_OUTPUT          *pOutput,
    LW2080_CTRL_CLK_VF_ITERATION_STATE *pVfIterState
)
{
    CLK_DOMAIN_3X_PROG     *pDomain3XProg =
        (CLK_DOMAIN_3X_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    CLK_DOMAIN_3X_PRIMARY   *pDomain3XPrimary;
    FLCN_STATUS             status;

    pDomain3XPrimary = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain3XProg, 3X_PRIMARY);
    if (pDomain3XPrimary == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto clkDomainProgVoltToFreq_3X_PRIMARY_exit;
    }

    if ((PMUCFG_FEATURE_ENABLED(PMU_CLK_VF_LOOK_UP_USING_BINARY_SEARCH)) &&
        (pVfIterState == NULL))
    {
        status = clkDomain3XPrimaryVoltToFreqBinarySearch(
            pDomain3XPrimary,                            // pDomain3XPrimary
            BOARDOBJ_GET_GRP_IDX_8BIT(&pDomain3XProg->super.super.super.super),  // clkDomIdx
            voltRailIdx,                                // voltRailIdx
            voltageType,                                // voltageType
            pInput,                                     // pInput
            pOutput);                                   // pOutput
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomainProgVoltToFreq_3X_PRIMARY_exit;
        }
    }
    else
    {
        status = clkDomain3XPrimaryVoltToFreqLinearSearch(
            pDomain3XPrimary,                            // pDomain3XPrimary
            BOARDOBJ_GET_GRP_IDX_8BIT(&pDomain3XProg->super.super.super.super),  // clkDomIdx
            voltRailIdx,                                // voltRailIdx
            voltageType,                                // voltageType
            pInput,                                     // pInput
            pOutput,                                    // pOutput
            pVfIterState);                              // pVfIterState
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomainProgVoltToFreq_3X_PRIMARY_exit;
        }
    }

clkDomainProgVoltToFreq_3X_PRIMARY_exit:
    return status;
}

/*!
 * @copydoc ClkDomain3XPrimaryVoltToFreqLinearSearch
 */
FLCN_STATUS
clkDomain3XPrimaryVoltToFreqLinearSearch
(
    CLK_DOMAIN_3X_PRIMARY               *pDomain3XPrimary,
    LwU8                                clkDomIdx,
    LwU8                                voltRailIdx,
    LwU8                                voltageType,
    LW2080_CTRL_CLK_VF_INPUT           *pInput,
    LW2080_CTRL_CLK_VF_OUTPUT          *pOutput,
    LW2080_CTRL_CLK_VF_ITERATION_STATE *pVfIterState
)
{
    CLK_DOMAIN_3X_PROG                 *pDomain3XProg =
        (CLK_DOMAIN_3X_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomain3XPrimary);
    CLK_PROG_3X_PRIMARY                 *pProg3XPrimary;
    LW2080_CTRL_CLK_VF_ITERATION_STATE  vfIterState   = LW2080_CTRL_CLK_VF_ITERATION_STATE_INIT();
    FLCN_STATUS status = FLCN_OK;
    LwU8        i;

    // Sanity check input value.
    if (pInput->value == LW2080_CTRL_CLK_VF_VALUE_ILWALID)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkDomain3XPrimaryVoltToFreqLinearSearch_exit;
    }

    //
    // If client did not request VF iteration state tracking, use local stack
    // variable just to track state during this singular call.
    //
    if (pVfIterState == NULL)
    {
        pVfIterState = &vfIterState;
    }

    // Init the output to default/unmatched values.
    LW2080_CTRL_CLK_VF_OUTPUT_INIT(pOutput);

    // Iterate through the set of CLK_PROG entries to find best matches.
    for (i = LW_MAX(pDomain3XProg->clkProgIdxFirst,
                        pVfIterState->clkProgIdx);
            i <= pDomain3XProg->clkProgIdxLast; i++)
    {
        CLK_PROG *pProg3X = BOARDOBJGRP_OBJ_GET(CLK_PROG, i);

        if (pProg3X == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkDomain3XPrimaryVoltToFreqLinearSearch_exit;
        }

        pProg3XPrimary = CLK_PROG_BOARDOBJ_TO_INTERFACE_CAST(pProg3X, 3X_PRIMARY);
        if (pProg3XPrimary == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkDomain3XPrimaryVoltToFreqLinearSearch_exit;
        }

        status = clkProg3XPrimaryVoltToFreqLinearSearch(
                    pProg3XPrimary,   // pProg3XPrimary
                    pDomain3XPrimary, // pDomain3XPrimary
                    clkDomIdx,       // clkDomIdx
                    voltRailIdx,     // voltRailIdx
                    voltageType,     // voltageType
                    pInput,          // pInput
                    pOutput,         // pOutput
                    pVfIterState);   // pVfIterState
        //
        // If the CLK_PROG_3X_PRIMARY encountered VF points above
        // pInput->value, the search is done.  Can short-circuit the
        // rest of the search and return FLCN_OK.
        //
        if (FLCN_ERR_ITERATION_END == status)
        {
            status = FLCN_OK;

            //
            // Since status = FLCN_OK, the future of the branch is known.
            // To satisfy MISRA compliance of rule 14.2, "i" is immediately 
            // saved and exits the function.
            //
            pVfIterState->clkProgIdx = i;
            goto clkDomain3XPrimaryVoltToFreqLinearSearch_exit;
        }
        else if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomain3XPrimaryVoltToFreqLinearSearch_exit;
        }
        else
        {
            lwosNOP(); // do nothing to satisfy misra rule 15.7
        }

        //
        // If proceeding to the next CLK_PROG, clear the CLK_VF_POINT index
        // iterator state, as it's not applicable to the next CLK_PROG.
        //
        if (i < pDomain3XProg->clkProgIdxLast)
        {
            pVfIterState->clkVfPointIdx = 0;
        }
    }

    // If successfully completed, save off the CLK_PROG iterator index.
    if (status == FLCN_OK)
    {
        pVfIterState->clkProgIdx = i - 1U;
    }

clkDomain3XPrimaryVoltToFreqLinearSearch_exit:
    return status;
}

/*!
 * _PRIMARY implemenation.
 *
 * @copydoc ClkDomainProgFreqToVolt
 */
FLCN_STATUS
clkDomainProgFreqToVolt_3X_PRIMARY
(
    CLK_DOMAIN_PROG                    *pDomainProg,
    LwU8                                voltRailIdx,
    LwU8                                voltageType,
    LW2080_CTRL_CLK_VF_INPUT           *pInput,
    LW2080_CTRL_CLK_VF_OUTPUT          *pOutput,
    LW2080_CTRL_CLK_VF_ITERATION_STATE *pVfIterState
)
{
    CLK_DOMAIN_3X_PROG     *pDomain3XProg =
        (CLK_DOMAIN_3X_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    CLK_DOMAIN_3X_PRIMARY   *pDomain3XPrimary;
    FLCN_STATUS             status;

    pDomain3XPrimary = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain3XProg, 3X_PRIMARY);
    if (pDomain3XPrimary == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto clkDomainProgFreqToVolt_3X_PRIMARY_esit;
    }

    if ((PMUCFG_FEATURE_ENABLED(PMU_CLK_VF_LOOK_UP_USING_BINARY_SEARCH)) &&
        (pVfIterState == NULL))
    {
        status = clkDomain3XPrimaryFreqToVoltBinarySearch(
            pDomain3XPrimary,                          // pDomain3XPrimary
            BOARDOBJ_GET_GRP_IDX_8BIT(&pDomain3XProg->super.super.super.super),  // clkDomIdx
            voltRailIdx,                              // voltRailIdx
            voltageType,                              // voltageType
            pInput,                                   // pInput
            pOutput);                                 // pOutput
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomainProgFreqToVolt_3X_PRIMARY_esit;
        }
    }
    else
    {
        status = clkDomain3XPrimaryFreqToVoltLinearSearch(
            pDomain3XPrimary,                          // pDomain3XPrimary
            BOARDOBJ_GET_GRP_IDX_8BIT(&pDomain3XProg->super.super.super.super),  // clkDomIdx
            voltRailIdx,                              // voltRailIdx
            voltageType,                              // voltageType
            pInput,                                   // pInput
            pOutput,                                  // pOutput
            pVfIterState);                            // pVfIterState
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomainProgFreqToVolt_3X_PRIMARY_esit;
        }
    }

clkDomainProgFreqToVolt_3X_PRIMARY_esit:
    return status;
}

/*!
 * @copydoc ClkDomain3XPrimaryFreqToVoltLinearSearch
 */
FLCN_STATUS
clkDomain3XPrimaryFreqToVoltLinearSearch
(
    CLK_DOMAIN_3X_PRIMARY               *pDomain3XPrimary,
    LwU8                                clkDomIdx,
    LwU8                                voltRailIdx,
    LwU8                                voltageType,
    LW2080_CTRL_CLK_VF_INPUT           *pInput,
    LW2080_CTRL_CLK_VF_OUTPUT          *pOutput,
    LW2080_CTRL_CLK_VF_ITERATION_STATE *pVfIterState
)
{
    CLK_DOMAIN_3X_PROG                 *pDomain3XProg =
        (CLK_DOMAIN_3X_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomain3XPrimary);
    CLK_PROG_3X_PRIMARY                 *pProg3XPrimary;
    LW2080_CTRL_CLK_VF_ITERATION_STATE  vfIterState   = LW2080_CTRL_CLK_VF_ITERATION_STATE_INIT();
    FLCN_STATUS status = FLCN_OK;
    LwU8       i;

    // Sanity check input value.
    if (pInput->value == LW2080_CTRL_CLK_VF_VALUE_ILWALID)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkDomain3XPrimaryFreqToVoltLinearSearch_exit;
    }

    //
    // If client did not request VF iteration state tracking, use local stack
    // variable just to track state during this singular call.
    //
    if (pVfIterState == NULL)
    {
        pVfIterState = &vfIterState;
    }

    // Init the output to default/unmatched values.
    LW2080_CTRL_CLK_VF_OUTPUT_INIT(pOutput);

    // Iterate through the set of CLK_PROG entries to find best matches.
    for (i = LW_MAX(pDomain3XProg->clkProgIdxFirst,
                        pVfIterState->clkProgIdx);
            i <= pDomain3XProg->clkProgIdxLast; i++)
    {
        CLK_PROG *pProg3X = BOARDOBJGRP_OBJ_GET(CLK_PROG, i);

        if (pProg3X == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkDomain3XPrimaryFreqToVoltLinearSearch_exit;
        }

        pProg3XPrimary = CLK_PROG_BOARDOBJ_TO_INTERFACE_CAST(pProg3X, 3X_PRIMARY);
        if (pProg3XPrimary == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkDomain3XPrimaryFreqToVoltLinearSearch_exit;
        }

        status = clkProg3XPrimaryFreqToVoltLinearSearch(
                    pProg3XPrimary,   // pProg3XPrimary
                    pDomain3XPrimary, // pDomain3XPrimary
                    clkDomIdx,       // clkDomIdx
                    voltRailIdx,     // voltRailIdx
                    voltageType,     // voltageType
                    pInput,          // pInput
                    pOutput,         // pOutput
                    pVfIterState);   // pVfIterState
        //
        // If the CLK_PROG_3X_PRIMARY encountered VF points above
        // pInput->value, the search is done.  Can short-circuit the
        // rest of the search and return FLCN_OK.
        //
        if (FLCN_ERR_ITERATION_END == status)
        {
            status = FLCN_OK;

            //
            // Since status = FLCN_OK, the future path of the branch is known.
            // To satisfy MISRA compliance of rule 14.2, "i" is immediately 
            // saved and exits the function.
            //
            pVfIterState->clkProgIdx = i;
            goto clkDomain3XPrimaryFreqToVoltLinearSearch_exit;
        }
        else if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomain3XPrimaryFreqToVoltLinearSearch_exit;
        }
        else
        {
            lwosNOP(); // do nothing to satisfy misra rule 15.7
        }

        //
        // If proceeding to the next CLK_PROG, clear the CLK_VF_POINT index
        // iterator state, as it's not applicable to the next CLK_PROG.
        //
        if (i < pDomain3XProg->clkProgIdxLast)
        {
            pVfIterState->clkVfPointIdx = 0;
        }
    }

    // If successfully completed, save off the CLK_PROG iterator index.
    if (status == FLCN_OK)
    {
        pVfIterState->clkProgIdx = i - 1U;
    }

clkDomain3XPrimaryFreqToVoltLinearSearch_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
