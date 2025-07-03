/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
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
 * CLK_DOMAIN_PROG class constructor.
 *
 * @copydoc BoardObjInterfaceConstruct
 */
FLCN_STATUS
clkDomainConstruct_PROG
(
    BOARDOBJGRP                *pBObjGrp,
    BOARDOBJ_INTERFACE         *pInterface,
    RM_PMU_BOARDOBJ_INTERFACE  *pInterfaceDesc,
    INTERFACE_VIRTUAL_TABLE    *pVirtualTable
)
{
    return boardObjInterfaceConstruct(pBObjGrp, pInterface, pInterfaceDesc, pVirtualTable);
}

/*!
 * @copydoc ClkDomainProgVoltToFreq
 */
FLCN_STATUS
clkDomainProgVoltToFreq
(
    CLK_DOMAIN_PROG                    *pDomainProg,
    LwU8                                voltRailIdx,
    LwU8                                voltageType,
    LW2080_CTRL_CLK_VF_INPUT           *pInput,
    LW2080_CTRL_CLK_VF_OUTPUT          *pOutput,
    LW2080_CTRL_CLK_VF_ITERATION_STATE *pVfIterState
)
{
    return clkDomainProgVoltToFreq_3X(pDomainProg,
                            voltRailIdx, voltageType, pInput, pOutput,
                            pVfIterState);
}

/*!
 * @copydoc ClkDomainProgFreqToVolt
 */
FLCN_STATUS
clkDomainProgFreqToVolt
(
    CLK_DOMAIN_PROG                    *pDomainProg,
    LwU8                                voltRailIdx,
    LwU8                                voltageType,
    LW2080_CTRL_CLK_VF_INPUT           *pInput,
    LW2080_CTRL_CLK_VF_OUTPUT          *pOutput,
    LW2080_CTRL_CLK_VF_ITERATION_STATE *pVfIterState
)
{
    return clkDomainProgFreqToVolt_3X(pDomainProg,
                            voltRailIdx, voltageType, pInput, pOutput,
                            pVfIterState);
}

/*!
 * @copydoc ClkDomainProgVoltToFreqTuple
 */
FLCN_STATUS
clkDomainProgVoltToFreqTuple
(
    CLK_DOMAIN_PROG                                *pDomainProg,
    LwU8                                            voltRailIdx,
    LwU8                                            voltageType,
    LW2080_CTRL_CLK_VF_INPUT                       *pInput,
    LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE    *pOutput,
    LW2080_CTRL_CLK_VF_ITERATION_STATE             *pVfIterState
)
{
    return clkDomainProgVoltToFreqTuple_35(pDomainProg,
                            voltRailIdx, voltageType, pInput, pOutput,
                            pVfIterState);
}

/*!
 * @copydoc ClkDomainProgPreVoltOrderingIndexGet
 */
LwU8
clkDomainProgPreVoltOrderingIndexGet
(
    CLK_DOMAIN_PROG    *pDomainProg
)
{
    return clkDomainProgPreVoltOrderingIndexGet_35(pDomainProg);
}

/*!
 * @copydoc ClkDomainProgPostVoltOrderingIndexGet
 */
LwU8
clkDomainProgPostVoltOrderingIndexGet
(
    CLK_DOMAIN_PROG    *pDomainProg
)
{
    return clkDomainProgPostVoltOrderingIndexGet_35(pDomainProg);
}

/*!
 * @brief   Exelwtes generic CLK_DOMAIN_PROG_VOLT_TO_FREQ RPC.
 *
 * @param[in/out]   pParams Pointer to RM_PMU_RPC_STRUCT_CLK_CLK_DOMAIN_PROG_VOLT_TO_FREQ
 */
FLCN_STATUS
pmuRpcClkClkDomainProgVoltToFreq
(
    RM_PMU_RPC_STRUCT_CLK_CLK_DOMAIN_PROG_VOLT_TO_FREQ *pParams
)
{
    CLK_DOMAIN         *pDomain =
        (CLK_DOMAIN *)BOARDOBJGRP_OBJ_GET(CLK_DOMAIN, pParams->clkDomainIdx);
    CLK_DOMAIN_PROG    *pDomainProg;
    FLCN_STATUS         status;

    // Sanity check the input clock domain index.
    if ((pDomain == NULL) ||
        (BOARDOBJ_GET_TYPE(pDomain) == LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_3X_FIXED))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto pmuRpcClkClkDomainProgVoltToFreq_exit;
    }

    pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
    if (pDomainProg == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto pmuRpcClkClkDomainProgVoltToFreq_exit;
    }

    status = clkDomainProgVoltToFreq(pDomainProg,
                                     pParams->voltRailIdx,
                                     pParams->voltageType,
                                     &pParams->input,
                                     &pParams->output,
                                     NULL);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pmuRpcClkClkDomainProgVoltToFreq_exit;
    }

pmuRpcClkClkDomainProgVoltToFreq_exit:
    return status;
}

/*!
 * @brief   Exelwtes generic CLK_DOMAIN_PROG_FREQ_TO_VOLT RPC.
 *
 * @param[in/out]   pParams Pointer to RM_PMU_RPC_STRUCT_CLK_CLK_DOMAIN_PROG_FREQ_TO_VOLT
 */
FLCN_STATUS
pmuRpcClkClkDomainProgFreqToVolt
(
    RM_PMU_RPC_STRUCT_CLK_CLK_DOMAIN_PROG_FREQ_TO_VOLT *pParams
)
{
    CLK_DOMAIN         *pDomain =
        (CLK_DOMAIN *)BOARDOBJGRP_OBJ_GET(CLK_DOMAIN, pParams->clkDomainIdx);
    CLK_DOMAIN_PROG    *pDomainProg;
    FLCN_STATUS         status;

    // Sanity check the input clock domain index.
    if ((pDomain == NULL) ||
        (BOARDOBJ_GET_TYPE(pDomain) == LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_3X_FIXED))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto pmuRpcClkClkDomainProgFreqToVolt_exit;
    }

    pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
    if (pDomainProg == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto pmuRpcClkClkDomainProgFreqToVolt_exit;
    }

    status = clkDomainProgFreqToVolt(pDomainProg,
                                     pParams->voltRailIdx,
                                     pParams->voltageType,
                                     &pParams->input,
                                     &pParams->output,
                                     NULL);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pmuRpcClkClkDomainProgFreqToVolt_exit;
    }

pmuRpcClkClkDomainProgFreqToVolt_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
