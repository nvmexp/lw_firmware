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
#include "dmemovl.h"

/* ------------------------ Static Function Prototypes --------------------- */
static BoardObjVirtualTableGetInterfaceFromBoardObj (s_clkDomainGetInterfaceFromBoardObj_35_Primary)
    GCC_ATTRIB_SECTION("imem_perfVf", "s_clkDomainGetInterfaceFromBoardObj_35_Primary");

/* ------------------------ Global Variables ------------------------------- */
/*!
 * Main structure for all CLK_DOMAIN_35_PRIMARY_VIRTUAL_TABLE data.
 */
static BOARDOBJ_VIRTUAL_TABLE ClkDomain35PrimaryVirtualTable =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_GET_INTERFACE_FROM_BOARDOBJ(
        s_clkDomainGetInterfaceFromBoardObj_35_Primary)
};

/*!
 * Main structure for all CLK_DOMAIN_3X_PRIMARY_VIRTUAL_TABLE data.
 */
static INTERFACE_VIRTUAL_TABLE ClkDomain3XPrimaryVirtualTable =
{
    LW_OFFSETOF(CLK_DOMAIN_35_PRIMARY, primary)   // offset
};

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_DOMAIN_35_PRIMARY class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkDomainGrpIfaceModel10ObjSet_35_PRIMARY
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_CLK_CLK_DOMAIN_35_PRIMARY_BOARDOBJ_SET *pSet35Primary =
        (RM_PMU_CLK_CLK_DOMAIN_35_PRIMARY_BOARDOBJ_SET *)pBoardObjDesc;
    CLK_DOMAIN_35_PRIMARY   *pDomain35Primary;
    BOARDOBJ_VTABLE        *pBoardObjVtable;
    FLCN_STATUS             status;

    status = clkDomainGrpIfaceModel10ObjSet_35_PROG(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainGrpIfaceModel10ObjSet_35_PRIMARY_exit;
    }
    pBoardObjVtable = (BOARDOBJ_VTABLE *)*ppBoardObj;
    pDomain35Primary = (CLK_DOMAIN_35_PRIMARY *)*ppBoardObj;

    // Construct 3X_Primary super class
    status = clkDomainConstruct_3X_PRIMARY(pBObjGrp,
                &pDomain35Primary->primary.super,
                &pSet35Primary->primary.super,
                &ClkDomain3XPrimaryVirtualTable);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainGrpIfaceModel10ObjSet_35_PRIMARY_exit;
    }

    // Override the Vtable pointer.
    pBoardObjVtable->pVirtualTable = &ClkDomain35PrimaryVirtualTable;

    // Init and copy in the mask.
    boardObjGrpMaskInit_E32(&(pDomain35Primary->primarySecondaryDomainsGrpMask));

    status = boardObjGrpMaskImport_E32(&(pDomain35Primary->primarySecondaryDomainsGrpMask),
                                       &(pSet35Primary->primarySecondaryDomainsGrpMask));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainGrpIfaceModel10ObjSet_35_PRIMARY_exit;
    }

clkDomainGrpIfaceModel10ObjSet_35_PRIMARY_exit:
    return status;
}

/*!
 * @copydoc ClkDomainLoad
 */
FLCN_STATUS
clkDomainLoad_35_Primary
(
    CLK_DOMAIN *pDomain
)
{
    CLK_DOMAIN_35_PRIMARY   *pDomain35Primary = (CLK_DOMAIN_35_PRIMARY *)pDomain;
    CLK_PROG_35_PRIMARY     *pProg35Primary;
    LwU8                    voltRailIdx;
    LwU8                    i;
    LwU8                    lwrveIdx;
    FLCN_STATUS             status = FLCN_OK;
    CLK_DOMAIN_35_PROG     *pDomain35Prog = &pDomain35Primary->super;

    if (pDomain35Prog == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto clkDomainLoad_35_Primary_exit;
    }

    for (lwrveIdx = 0;
         lwrveIdx < clkDomain35ProgGetClkDomailwfLwrveCount(pDomain35Prog);
         lwrveIdx++)
    {
        // Iterate over each VOLTAGE_RAIL separately.
        for (voltRailIdx = 0; voltRailIdx < Clk.domains.voltRailsMax; voltRailIdx++)
        {
            //
            // Iterate over this CLK_DOMAIN_35_PRIMARY's CLK_PROG_35_PRIMARY objects
            // and load each of them in order.
            //
            for (i = pDomain35Primary->super.super.clkProgIdxFirst;
                    i <= pDomain35Primary->super.super.clkProgIdxLast;
                    i++)
            {
                pProg35Primary = (CLK_PROG_35_PRIMARY *)BOARDOBJGRP_OBJ_GET(CLK_PROG, i);
                if (pProg35Primary == NULL)
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_INDEX;
                    goto clkDomainLoad_35_Primary_exit;
                }

                status = clkProg35PrimaryLoad(pProg35Primary,
                                             pDomain35Primary,
                                             voltRailIdx,
                                             lwrveIdx);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto clkDomainLoad_35_Primary_exit;
                }
            }
        }
    }

clkDomainLoad_35_Primary_exit:
    return status;
}

/*!
 * @copydoc ClkDomainCache
 */
FLCN_STATUS
clkDomainCache_35_Primary
(
    CLK_DOMAIN *pDomain,
    LwBool      bVFEEvalRequired
)
{
    CLK_DOMAIN_35_PRIMARY   *pDomain35Primary = (CLK_DOMAIN_35_PRIMARY *)pDomain;
    CLK_PROG_35_PRIMARY     *pProg35Primary;
    FLCN_STATUS             status = FLCN_OK;
    LwU8                    voltRailIdx;
    LwU8                    i;
    LwS16                   j;
    LwU8                    lwrveIdx;
    CLK_DOMAIN_35_PROG     *pDomain35Prog = &pDomain35Primary->super;

    if (pDomain35Prog == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto clkDomainCache_35_Primary_exit;
    }

    for (lwrveIdx = 0;
         lwrveIdx < clkDomain35ProgGetClkDomailwfLwrveCount(pDomain35Prog);
         lwrveIdx++)
    {
        // Iterate over each VOLTAGE_RAIL separately.
        for (voltRailIdx = 0; voltRailIdx < Clk.domains.voltRailsMax; voltRailIdx++)
        {
            //
            // Create a VF point pointer on stack to track the last VF point in case
            // of caching and expected vf point in case of VF smoothing.
            //
            CLK_VF_POINT_35 *pVfPoint35 = NULL;

            // Generate primary VF lwrve

            //
            // Iterate over this CLK_DOMAIN_35_PRIMARY's CLK_PROG_35_PRIMARY objects
            // and cache each of them in order, passing the vFPairLast structure to
            // ensure that the CLK_VF_POINTs are monotonically increasing.
            //
            for (i = pDomain35Primary->super.super.clkProgIdxFirst;
                    i <= pDomain35Primary->super.super.clkProgIdxLast;
                    i++)
            {
                // Cache the previous programming entry's last VF point.
                if (i != pDomain35Primary->super.super.clkProgIdxFirst)
                {
                    pProg35Primary = (CLK_PROG_35_PRIMARY *)BOARDOBJGRP_OBJ_GET(CLK_PROG, (i - 1U));
                    if (pProg35Primary == NULL)
                    {
                        PMU_BREAKPOINT();
                        status = FLCN_ERR_ILWALID_INDEX;
                        goto clkDomainCache_35_Primary_exit;
                    }

                    pVfPoint35 = (CLK_VF_POINT_35 *)CLK_VF_POINT_GET_BY_LWRVE_ID(lwrveIdx,
                         clklkProg35PrimaryVfPointIdxLastGet(pProg35Primary, voltRailIdx, lwrveIdx));
                }

                pProg35Primary = (CLK_PROG_35_PRIMARY *)BOARDOBJGRP_OBJ_GET(CLK_PROG, i);
                if (pProg35Primary == NULL)
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_INDEX;
                    goto clkDomainCache_35_Primary_exit;
                }

                status = clkProg35PrimaryCache(pProg35Primary,
                                              pDomain35Primary,
                                              voltRailIdx,
                                              bVFEEvalRequired,
                                              lwrveIdx,
                                              pVfPoint35);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto clkDomainCache_35_Primary_exit;
                }
            }

            // Smooth Primary's VF lwrve based on POR max allowed step size.
            if (clkDomainsVfSmootheningEnforced())
            {
                // Re-init the VF point to track the expected VF point values.
                pVfPoint35 = NULL;

                //
                // Iterate over this CLK_DOMAIN_35_PRIMARY's CLK_PROG_35_PRIMARY objects
                // and smoothen each of them in order.
                //
                for (j = pDomain35Primary->super.super.clkProgIdxLast;
                        j >= pDomain35Primary->super.super.clkProgIdxFirst;
                        j--)
                {
                    // Cache the previously evaluated programming entry's first VF point.
                    if (j != (LwS16)pDomain35Primary->super.super.clkProgIdxLast)
                    {
                        pProg35Primary = (CLK_PROG_35_PRIMARY *)BOARDOBJGRP_OBJ_GET(CLK_PROG, (j + 1));
                        if (pProg35Primary == NULL)
                        {
                            PMU_BREAKPOINT();
                            status = FLCN_ERR_ILWALID_INDEX;
                            goto clkDomainCache_35_Primary_exit;
                        }

                        pVfPoint35 = (CLK_VF_POINT_35 *)CLK_VF_POINT_GET_BY_LWRVE_ID(lwrveIdx,
                             clklkProg35PrimaryVfPointIdxFirstGet(pProg35Primary, voltRailIdx, lwrveIdx));
                    }

                    pProg35Primary = (CLK_PROG_35_PRIMARY *)BOARDOBJGRP_OBJ_GET(CLK_PROG, j);
                    if (pProg35Primary == NULL)
                    {
                        PMU_BREAKPOINT();
                        status = FLCN_ERR_ILWALID_INDEX;
                        goto clkDomainCache_35_Primary_exit;
                    }

                    status = clkProg35PrimarySmoothing(pProg35Primary,
                                                      pDomain35Primary,
                                                      voltRailIdx,
                                                      lwrveIdx,
                                                      pVfPoint35);
                    if (status != FLCN_OK)
                    {
                        PMU_BREAKPOINT();
                        goto clkDomainCache_35_Primary_exit;
                    }
                }
            }
        }
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_OVOC_SUPPORTED))
    {
        CLK_DOMAIN   *pDomainLocal;
        LwBoardObjIdx clkIdx;

        // Cache the clock programming entries max supported frequency.
        BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomainLocal, clkIdx,
            &pDomain35Primary->primarySecondaryDomainsGrpMask.super)
        {
            CLK_DOMAIN_3X_PROG *pDomain3XProg = (CLK_DOMAIN_3X_PROG *)pDomainLocal;
            CLK_PROG_35        *pProg35;

            for (i = pDomain3XProg->clkProgIdxFirst; i <= pDomain3XProg->clkProgIdxLast;
                i++)
            {
                pProg35 = (CLK_PROG_35 *)BOARDOBJGRP_OBJ_GET(CLK_PROG, i);
                if (pProg35 == NULL)
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_INDEX;
                    goto clkDomainCache_35_Primary_exit;
                }

                status = clkProg35CacheOffsettedFreqMaxMHz(pProg35, pDomain3XProg);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto clkDomainCache_35_Primary_exit;
                }
            }
        }
        BOARDOBJGRP_ITERATOR_END;
    }

    // Trim VF lwrve to frequency enumeration max.
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_TRIM_VF_LWRVE_BY_ENUM_MAX))
    {
        pDomain35Prog = &pDomain35Primary->super;

        if (pDomain35Prog == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkDomainCache_35_Primary_exit;
        }

        // Iterate over all VF lwrves.
        for (lwrveIdx = 0;
             lwrveIdx < clkDomain35ProgGetClkDomailwfLwrveCount(pDomain35Prog);
             lwrveIdx++)
        {
            // Iterate over each VOLTAGE_RAIL separately.
            for (voltRailIdx = 0; voltRailIdx < Clk.domains.voltRailsMax; voltRailIdx++)
            {
                //
                // Iterate over this CLK_DOMAIN_35_PRIMARY's CLK_PROG_35_PRIMARY objects
                // and trim each of them to enumeration max.
                //
                for (j = pDomain35Primary->super.super.clkProgIdxLast;
                        j >= pDomain35Primary->super.super.clkProgIdxFirst;
                        j--)
                {
                    pProg35Primary = (CLK_PROG_35_PRIMARY *)BOARDOBJGRP_OBJ_GET(CLK_PROG, j);
                    if (pProg35Primary == NULL)
                    {
                        PMU_BREAKPOINT();
                        status = FLCN_ERR_ILWALID_INDEX;
                        goto clkDomainCache_35_Primary_exit;
                    }

                    status = clkProg35PrimaryTrim(pProg35Primary,
                                                 pDomain35Primary,
                                                 voltRailIdx,
                                                 lwrveIdx);
                    if (status != FLCN_OK)
                    {
                        PMU_BREAKPOINT();
                        goto clkDomainCache_35_Primary_exit;
                    }
                }
            }
        }
    }

clkDomainCache_35_Primary_exit:
    return status;
}

/*!
 * @copydoc ClkDomainProgVoltToFreqTuple
 */
FLCN_STATUS
clkDomainProgVoltToFreqTuple_35_PRIMARY_IMPL
(
    CLK_DOMAIN_PROG                                *pDomainProg,
    LwU8                                            voltRailIdx,
    LwU8                                            voltageType,
    LW2080_CTRL_CLK_VF_INPUT                       *pInput,
    LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE    *pOutput,
    LW2080_CTRL_CLK_VF_ITERATION_STATE             *pVfIterState
)
{
    CLK_DOMAIN_35_PROG     *pDomain35Prog =
        (CLK_DOMAIN_35_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    CLK_DOMAIN_35_PRIMARY *pDomain35Primary = (CLK_DOMAIN_35_PRIMARY *)pDomain35Prog;

    return clkDomain35PrimaryVoltToFreqTuple(pDomain35Primary, // pDomain35Primary
                BOARDOBJ_GET_GRP_IDX_8BIT(&pDomain35Prog->super.super.super.super.super), // clkDomIdx
                voltRailIdx,                                 // voltRailIdx
                voltageType,                                 // voltageType
                pInput,                                      // pInput
                pOutput,                                     // pOutput
                pVfIterState);                               // pVfIterState
}

/*!
 * @copydoc ClkDomain35PrimaryVoltToFreqTuple
 */
FLCN_STATUS
clkDomain35PrimaryVoltToFreqTuple
(
    CLK_DOMAIN_35_PRIMARY                           *pDomain35Primary,
    LwU8                                            clkDomIdx,
    LwU8                                            voltRailIdx,
    LwU8                                            voltageType,
    LW2080_CTRL_CLK_VF_INPUT                       *pInput,
    LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE    *pOutput,
    LW2080_CTRL_CLK_VF_ITERATION_STATE             *pVfIterState
)
{
    CLK_DOMAIN_3X_PRIMARY       *pDomain3XPrimary;
    CLK_VF_POINT_35            *pVfPoint35;
    LW2080_CTRL_CLK_VF_OUTPUT   outputVFPair;
    LwU8                        lwrveIdx;
    FLCN_STATUS                 status;
    CLK_DOMAIN_35_PROG         *pDomain35Prog;

    pDomain3XPrimary = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain35Primary, 3X_PRIMARY);
    if (pDomain3XPrimary == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto clkDomain35PrimaryVoltToFreqTuple_exit;
    }

    // Init the output struct.
    LW2080_CTRL_CLK_VF_OUTPUT_INIT(&outputVFPair);

    status = clkDomain3XPrimaryVoltToFreqLinearSearch(
                pDomain3XPrimary,                            // pDomain3XPrimary
                clkDomIdx,                                  // clkDomIdx
                voltRailIdx,                                // voltRailIdx
                voltageType,                                // voltageType
                pInput,                                     // pInput
                &outputVFPair,                              // pOutput
                pVfIterState);                              // pVfIterState
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomain35PrimaryVoltToFreqTuple_exit;
    }

    pOutput->version         =
        LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE_VERSION_10;
    pOutput->inputBestMatch  = outputVFPair.inputBestMatch;
    pOutput->data.v10.pri.freqMHz = outputVFPair.value;

    // Sanity check for best match.
    if (LW2080_CTRL_CLK_VF_VALUE_ILWALID == outputVFPair.inputBestMatch)
    {
        // This is not an error. Client will handle it.
        goto clkDomain35PrimaryVoltToFreqTuple_exit;
    }

    pDomain35Prog = CLK_DOMAIN_35_PROG_GET(clkDomIdx);
    if (pDomain35Prog == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto clkDomain35PrimaryVoltToFreqTuple_exit;
    }

    // Copy-in frequency tuple of secondary VF lwrves.
    for (lwrveIdx = LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_SEC_0;
         lwrveIdx < clkDomain35ProgGetClkDomailwfLwrveCount(pDomain35Prog);
         lwrveIdx++)
    {
        pVfPoint35 = (CLK_VF_POINT_35 *)CLK_VF_POINT_GET(SEC,
                        pVfIterState->clkVfPointIdx);
        if (pVfPoint35 == NULL)
        {
            PMU_BREAKPOINT();
            goto clkDomain35PrimaryVoltToFreqTuple_exit;
        }

        // Get the frequency tuple from VF point index.
        status = clkVfPoint35FreqTupleAccessor(
                    pVfPoint35,                             // pVfPoint35
                    pDomain3XPrimary,                        // pDomain3XPrimary
                    clkDomIdx,                              // clkDomIdx
                    lwrveIdx,                               // lwrveIdx
                    LW_TRUE,                                // bOffseted
                    pOutput);                               // pOutput
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomain35PrimaryVoltToFreqTuple_exit;
        }
    }

    //
    // CPM must be updated after updating secondary lwrve.
    // We do not want to override the versions 30 -> 20.
    //

    // If CPM enabled, update it.
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_CPM))
    {
        pVfPoint35 = (CLK_VF_POINT_35 *)CLK_VF_POINT_GET(PRI,
                        pVfIterState->clkVfPointIdx);
        if (pVfPoint35 == NULL)
        {
            PMU_BREAKPOINT();
            goto clkDomain35PrimaryVoltToFreqTuple_exit;
        }

        // Get the frequency tuple from VF point index.
        status = clkVfPoint35FreqTupleAccessor(
                    pVfPoint35,                                             // pVfPoint35
                    pDomain3XPrimary,                                        // pDomain3XPrimary
                    LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_PRI,    // clkDomIdx
                    lwrveIdx,                                               // lwrveIdx
                    LW_TRUE,                                                // bOffseted
                    pOutput);                                               // pOutput
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomain35PrimaryVoltToFreqTuple_exit;
        }
    }

clkDomain35PrimaryVoltToFreqTuple_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @copydoc BoardObjVirtualTableGetInterfaceFromBoardObj
 */
static BOARDOBJ_INTERFACE *
s_clkDomainGetInterfaceFromBoardObj_35_Primary
(
    BOARDOBJ   *pBoardObj,
    LwU8        interfaceType
)
{
    CLK_DOMAIN_35_PRIMARY *pDomain35Primary = (CLK_DOMAIN_35_PRIMARY *)pBoardObj;
    BOARDOBJ_INTERFACE   *pInterface      = NULL;

    switch (interfaceType)
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_INTERFACE_TYPE_3X_PRIMARY:
        {
            pInterface = &pDomain35Primary->primary.super;
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_INTERFACE_TYPE_PROG:
        {
            pInterface = &pDomain35Primary->super.super.prog.super;
            break;
        }
        default:
        {
            // Nothing to do
            break;
        }
    }

    if (pInterface == NULL)
    {
        PMU_BREAKPOINT();
    }

    return pInterface;
}
