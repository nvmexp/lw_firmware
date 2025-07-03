/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
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

/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
static BoardObjVirtualTableGetInterfaceFromBoardObj (s_clkDomainGetInterfaceFromBoardObj_40_PROG)
    GCC_ATTRIB_SECTION("imem_perfVf", "s_clkDomainGetInterfaceFromBoardObj_40_PROG");
static ClkDomain40ProgVoltAdjustDeltauV             (s_clkDomain40ProgVoltAdjustDeltauV_PRIMARY)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "s_clkDomain40ProgVoltAdjustDeltauV_PRIMARY");
static ClkDomain40ProgVoltAdjustDeltauV             (s_clkDomain40ProgVoltAdjustDeltauV_SECONDARY)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "s_clkDomain40ProgVoltAdjustDeltauV_SECONDARY");
static ClkDomain40ProgFreqAdjustDeltaMHz            (s_clkDomain40ProgFreqAdjustDeltaMHz_PRIMARY)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "s_clkDomain40ProgFreqAdjustDeltaMHz_PRIMARY");
static ClkDomain40ProgFreqAdjustDeltaMHz            (s_clkDomain40ProgFreqAdjustDeltaMHz_SECONDARY)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "s_clkDomain40ProgFreqAdjustDeltaMHz_SECONDARY");
static ClkDomain40ProgOffsetVFVoltAdjustDeltauV     (s_clkDomain40ProgOffsetVFVoltAdjustDeltauV_PRIMARY)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "s_clkDomain40ProgOffsetVFVoltAdjustDeltauV_PRIMARY");
static ClkDomain40ProgOffsetVFVoltAdjustDeltauV     (s_clkDomain40ProgOffsetVFVoltAdjustDeltauV_SECONDARY)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "s_clkDomain40ProgOffsetVFVoltAdjustDeltauV_SECONDARY");
static ClkDomain40ProgOffsetVFFreqAdjustDeltaMHz    (s_clkDomain40ProgOffsetVFFreqAdjustDeltaMHz_PRIMARY)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "s_clkDomain40ProgOffsetVFFreqAdjustDeltaMHz_PRIMARY");
static ClkDomain40ProgOffsetVFFreqAdjustDeltaMHz    (s_clkDomain40ProgOffsetVFFreqAdjustDeltaMHz_SECONDARY)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "s_clkDomain40ProgOffsetVFFreqAdjustDeltaMHz_SECONDARY");
static LwU8         s_clkDomain40ProgClkVfRelIdxGet(CLK_DOMAIN_40_PROG *pDomain40Prog, LwU8 railIdx, LwU16 freqMHz)
    GCC_ATTRIB_SECTION("imem_perfVf", "s_clkDomain40ProgClkVfRelIdxGet");
static FLCN_STATUS  s_clkDomain40ProgClkEnumCache(CLK_DOMAIN_40_PROG *pDomain40Prog)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "s_clkDomain40ProgClkEnumCache");

/* ------------------------ Global Variables ------------------------------- */
/*!
 * Main structure for all CLK_DOMAIN_40_PROG_VIRTUAL_TABLE data.
 */
BOARDOBJ_VIRTUAL_TABLE ClkDomain40ProgVirtualTable =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_GET_INTERFACE_FROM_BOARDOBJ(
        s_clkDomainGetInterfaceFromBoardObj_40_PROG)
};

/*!
 * Main structure for all CLK_DOMAIN_PROG_VIRTUAL_TABLE data.
 */
INTERFACE_VIRTUAL_TABLE ClkDomainProgVirtualTable_40 =
{
    LW_OFFSETOF(CLK_DOMAIN_40_PROG, prog)   // offset
};

/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_DOMAIN_40_PROG class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkDomainGrpIfaceModel10ObjSet_40_PROG
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_CLK_CLK_DOMAIN_40_PROG_BOARDOBJ_SET *pSet =
        (RM_PMU_CLK_CLK_DOMAIN_40_PROG_BOARDOBJ_SET *)pBoardObjSet;
    BOARDOBJ_VTABLE    *pBoardObjVtable;
    CLK_DOMAIN_40_PROG *pDomain40Prog;
    LwU8                railIdx;
    FLCN_STATUS         status;
    LwBool              bFbvddDataValid;

    status = clkDomainGrpIfaceModel10ObjSet_3X(pModel10, ppBoardObj, size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainGrpIfaceModel10ObjSet_40_PROG_exit;
    }
    pBoardObjVtable = (BOARDOBJ_VTABLE *)*ppBoardObj;
    pDomain40Prog   = (CLK_DOMAIN_40_PROG *)*ppBoardObj;

    status = clkDomainConstruct_PROG(pBObjGrp,
                &pDomain40Prog->prog.super,
                &pSet->prog.super,
                &ClkDomainProgVirtualTable_40);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainGrpIfaceModel10ObjSet_40_PROG_exit;
    }
    // Override the Vtable pointer.
    pBoardObjVtable->pVirtualTable = &ClkDomain40ProgVirtualTable;

    // Copy the CLK_DOMAIN_40_PROG-specific data.
    pDomain40Prog->clkEnumIdxFirst        = pSet->clkEnumIdxFirst;
    pDomain40Prog->clkEnumIdxLast         = pSet->clkEnumIdxLast;
    pDomain40Prog->preVoltOrderingIndex   = pSet->preVoltOrderingIndex;
    pDomain40Prog->postVoltOrderingIndex  = pSet->postVoltOrderingIndex;
    pDomain40Prog->clkVFLwrveCount        = pSet->clkVFLwrveCount;
    pDomain40Prog->factoryDelta           = pSet->factoryDelta;
    pDomain40Prog->grdFreqDelta           = pSet->grdFreqDelta;
    pDomain40Prog->freqDeltaMinMHz        = pSet->freqDeltaMinMHz;
    pDomain40Prog->freqDeltaMaxMHz        = pSet->freqDeltaMaxMHz;
    pDomain40Prog->clkMonInfo             = pSet->clkMonInfo;
    pDomain40Prog->clkMonCtrl             = pSet->clkMonCtrl;
    pDomain40Prog->fbvddData              = pSet->fbvddData;
    pDomain40Prog->railMask               = pSet->railMask;

    // Copy-in volt rail specific VF data.
    for (railIdx = 0; railIdx < LW2080_CTRL_CLK_CLK_DOMAIN_PROG_RAIL_VF_ITEM_MAX; railIdx++)
    {
        pDomain40Prog->railVfItem[railIdx].type   = pSet->railVfItem[railIdx].type;
        pDomain40Prog->railVfItem[railIdx].clkPos = pSet->railVfItem[railIdx].clkPos;

        if (pSet->railVfItem[railIdx].type ==
            LW2080_CTRL_CLK_CLK_DOMAIN_40_PROG_RAIL_VF_TYPE_PRIMARY)
        {
            pDomain40Prog->railVfItem[railIdx].data.primary.clkVfRelIdxFirst =
                pSet->railVfItem[railIdx].data.master.clkVfRelIdxFirst;
            pDomain40Prog->railVfItem[railIdx].data.primary.clkVfRelIdxLast  =
                pSet->railVfItem[railIdx].data.master.clkVfRelIdxLast;

            // Init and copy in the mask.
            boardObjGrpMaskInit_E32(
                        &(pDomain40Prog->railVfItem[railIdx].data.primary.primarySecondaryDomainsMask));

            // Export the mask.
            status = boardObjGrpMaskImport_E32(
                        &(pDomain40Prog->railVfItem[railIdx].data.primary.primarySecondaryDomainsMask),
                        &(pSet->railVfItem[railIdx].data.master.masterSlaveDomainsMask));
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkDomainGrpIfaceModel10ObjSet_40_PROG_exit;
            }

            // Init and copy in the mask.
            boardObjGrpMaskInit_E32(
                        &(pDomain40Prog->railVfItem[railIdx].data.primary.secondaryDomainsMask));

            // Export the mask.
            status = boardObjGrpMaskImport_E32(
                        &(pDomain40Prog->railVfItem[railIdx].data.primary.secondaryDomainsMask),
                        &(pSet->railVfItem[railIdx].data.master.slaveDomainsMask));
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkDomainGrpIfaceModel10ObjSet_40_PROG_exit;
            }
        }
        else if (pSet->railVfItem[railIdx].type ==
            LW2080_CTRL_CLK_CLK_DOMAIN_40_PROG_RAIL_VF_TYPE_SECONDARY)
        {
            pDomain40Prog->railVfItem[railIdx].data.secondary.primaryIdx =
                pSet->railVfItem[railIdx].data.slave.masterIdx;
        }
        else
        {
            continue;
        }
    }

    // Allocate the voltage rails array if not previously allocated.
    if (pDomain40Prog->deltas.pVoltDeltauV == NULL)
    {
        pDomain40Prog->deltas.pVoltDeltauV =
            lwosCallocType(pBObjGrp->ovlIdxDmem, Clk.domains.voltRailsMax, LwS32);
    }
    pDomain40Prog->deltas.freqDelta = pSet->deltas.freqDelta;
    memcpy(pDomain40Prog->deltas.pVoltDeltauV, pSet->deltas.voltDeltauV,
        (sizeof(LwS32) * Clk.domains.voltRailsMax));

    // If valid, check the consistency of the FBVDD data
    PMU_ASSERT_OK_OR_GOTO(status,
        clkDomainProgFbvddDataValid(
            &pDomain40Prog->prog,
            &bFbvddDataValid),
        clkDomainGrpIfaceModel10ObjSet_40_PROG_exit);
    if (bFbvddDataValid)
    {
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pDomain40Prog->fbvddData.pwrAdjustment.slope != 0U),
            FLCN_ERR_ILWALID_ARGUMENT,
            clkDomainGrpIfaceModel10ObjSet_40_PROG_exit);

        PMU_ASSERT_TRUE_OR_GOTO(status,
            pDomain40Prog->fbvddData.vfMappingTable.numMappings > 0U,
            FLCN_ERR_ILWALID_ARGUMENT,
            clkDomainGrpIfaceModel10ObjSet_40_PROG_exit);
    }

clkDomainGrpIfaceModel10ObjSet_40_PROG_exit:
    return status;
}

/*!
 * @copydoc ClkDomainsCache
 */
FLCN_STATUS
clkDomainsCache_40
(
    LwBool bVFEEvalRequired
)
{
    CLK_DOMAIN   *pDomain;
    LwBoardObjIdx i;
    FLCN_STATUS   status   = FLCN_ERR_NOT_SUPPORTED;

    //
    // Iterate over the _3X_PRIMARY objects to cache their PRIMARY VF lwrves.
    // On VF 3.5+, we are also caching their CLK_PROG @ref offsettedFreqMaxMHz
    //

    // Increment the VF Points cache counter.
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT))
    {
        clkVfPointsVfPointsCacheCounterIncrement();
    }

    // Generate VF lwrve for all primaries and their secondaries
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, i,
        &Clk.domains.progDomainsMask.super)
    {
        // Call type-specific implemenation.
        switch (BOARDOBJ_GET_TYPE(pDomain))
        {
            case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_40_PROG:
            {
                if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_40))
                {
                    status = clkDomainCache_40_PROG(pDomain, bVFEEvalRequired);
                }
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
            goto clkDomainsCache_40_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    // Adjust clock enum ranges with frequency offsets.
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, i,
        &Clk.domains.progDomainsMask.super)
    {
        CLK_DOMAIN_40_PROG *pDomain40Prog = (CLK_DOMAIN_40_PROG *)pDomain;

        status = s_clkDomain40ProgClkEnumCache(pDomain40Prog);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomainsCache_40_exit;
        }

    }
    BOARDOBJGRP_ITERATOR_END;

    // Adjust clock propagation topology relationship frequency with offsets.
    status = clkPropTopRelsCache();
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainsCache_40_exit;
    }

clkDomainsCache_40_exit:
    return status;
}

/*!
 * @copydoc ClkDomainsOffsetVFCache
 */
FLCN_STATUS
clkDomainsOffsetVFCache_40(void)
{
    CLK_DOMAIN   *pDomain;
    LwBoardObjIdx i;
    FLCN_STATUS   status   = FLCN_ERR_NOT_SUPPORTED;

    // Generate VF lwrve for all primaries and their secondaries
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, i,
        &Clk.domains.progDomainsMask.super)
    {
        // Call type-specific implemenation.
        switch (BOARDOBJ_GET_TYPE(pDomain))
        {
            case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_40_PROG:
            {
                if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_40))
                {
                    status = clkDomainOffsetVFCache_40_PROG(pDomain);
                }
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
            goto clkDomainsOffsetVFCache_40_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

clkDomainsOffsetVFCache_40_exit:
    return status;
}

/*!
 * @copydoc ClkDomainsBaseVFCache
 */
FLCN_STATUS
clkDomainsBaseVFCache_40
(
    LwU8    cacheIdx
)
{
    CLK_DOMAIN   *pDomain;
    LwBoardObjIdx i;
    FLCN_STATUS   status = FLCN_ERR_NOT_SUPPORTED;

    // Generate VF lwrve for all primaries and their secondaries
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, i,
        &Clk.domains.progDomainsMask.super)
    {
        // Call type-specific implementation.
        switch (BOARDOBJ_GET_TYPE(pDomain))
        {
            case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_40_PROG:
            {
                if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_40))
                {
                    status = clkDomainBaseVFCache_40_PROG(pDomain, cacheIdx);
                }
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
            goto clkDomainsBaseVFCache_40_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

clkDomainsBaseVFCache_40_exit:
    return status;
}

/*!
 * @copydoc ClkDomain40ProgVoltToFreq
 */
FLCN_STATUS
clkDomainProgVoltToFreq_40
(
    CLK_DOMAIN_PROG                    *pDomainProg,
    LwU8                                railIdx,
    LwU8                                voltageType,
    LW2080_CTRL_CLK_VF_INPUT           *pInput,
    LW2080_CTRL_CLK_VF_OUTPUT          *pOutput,
    LW2080_CTRL_CLK_VF_ITERATION_STATE *pVfIterState
)
{
    CLK_DOMAIN_40_PROG *pDomain40Prog =
        (CLK_DOMAIN_40_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    CLK_DOMAIN_40_PROG *pDomain40ProgPrimary = pDomain40Prog;
    CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY
                       *pVfPrimary           = NULL;
    LwU8                vfRelIdx;
    FLCN_STATUS         status              = FLCN_OK;

    // Sanity check input value.
    if ((pInput->value == LW2080_CTRL_CLK_VF_VALUE_ILWALID) ||
        !clkDomain40ProgRailIdxValid(pDomain40Prog, railIdx))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkDomainProgVoltToFreq_40_exit;
    }

    // Init the output to default/unmatched values.
    LW2080_CTRL_CLK_VF_OUTPUT_INIT(pOutput);

    // If secondary, get corresponding primary.
    if (clkDomain40ProgGetRailVfType(pDomain40Prog, railIdx) ==
        LW2080_CTRL_CLK_CLK_DOMAIN_40_PROG_RAIL_VF_TYPE_SECONDARY)
    {
        LwU8 primaryIdx = pDomain40Prog->railVfItem[railIdx].data.secondary.primaryIdx;

        pDomain40ProgPrimary = CLK_CLK_DOMAIN_40_PROG_GET_BY_IDX(primaryIdx);
    }

    if (pDomain40ProgPrimary == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto clkDomainProgVoltToFreq_40_exit;
    }

    pVfPrimary = &(pDomain40ProgPrimary->railVfItem[railIdx].data.primary);

    // Iterate over this VF_PRIMARY's CLK_VF_REL objects.
    for (vfRelIdx  = pVfPrimary->clkVfRelIdxFirst;
         vfRelIdx <= pVfPrimary->clkVfRelIdxLast;
         vfRelIdx++)
    {
        CLK_VF_REL *pVfRel;

        pVfRel = BOARDOBJGRP_OBJ_GET(CLK_VF_REL, vfRelIdx);
        if (pVfRel == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkDomainProgVoltToFreq_40_exit;
        }

        status = clkVfRelVoltToFreq(pVfRel,
                    pDomain40Prog,   // pDomain40Prog
                    voltageType,     // voltageType
                    pInput,          // pInput
                    pOutput);        // pOutput
        //
        // If the CLK_VF_REL encountered VF points above
        // pInput->value, the search is done.  Can short-circuit the
        // rest of the search and return FLCN_OK.
        //
        if (FLCN_ERR_ITERATION_END == status)
        {
            status = FLCN_OK;
            goto clkDomainProgVoltToFreq_40_exit;
        }

        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomainProgVoltToFreq_40_exit;
        }
    }

clkDomainProgVoltToFreq_40_exit:
    return status;
}

/*!
 * @copydoc ClkDomain40ProgFreqToVolt
 */
FLCN_STATUS
clkDomainProgFreqToVolt_40
(
    CLK_DOMAIN_PROG                    *pDomainProg,
    LwU8                                railIdx,
    LwU8                                voltageType,
    LW2080_CTRL_CLK_VF_INPUT           *pInput,
    LW2080_CTRL_CLK_VF_OUTPUT          *pOutput,
    LW2080_CTRL_CLK_VF_ITERATION_STATE *pVfIterState
)
{
    CLK_DOMAIN_40_PROG *pDomain40Prog =
        (CLK_DOMAIN_40_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    CLK_DOMAIN_40_PROG *pDomain40ProgPrimary = pDomain40Prog;
    CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY
                       *pVfPrimary           = NULL;
    LwU8                vfRelIdx;
    FLCN_STATUS         status              = FLCN_OK;

    // Sanity check input value.
    if ((pInput->value == LW2080_CTRL_CLK_VF_VALUE_ILWALID) ||
        !clkDomain40ProgRailIdxValid(pDomain40Prog, railIdx))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkDomainProgFreqToVolt_40_exit;
    }

    // Init the output to default/unmatched values.
    LW2080_CTRL_CLK_VF_OUTPUT_INIT(pOutput);

    // If secondary, get corresponding primary.
    if (clkDomain40ProgGetRailVfType(pDomain40Prog, railIdx) ==
        LW2080_CTRL_CLK_CLK_DOMAIN_40_PROG_RAIL_VF_TYPE_SECONDARY)
    {
        LwU8 primaryIdx = pDomain40Prog->railVfItem[railIdx].data.secondary.primaryIdx;

        pDomain40ProgPrimary = CLK_CLK_DOMAIN_40_PROG_GET_BY_IDX(primaryIdx);
    }

    if (pDomain40ProgPrimary == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto clkDomainProgFreqToVolt_40_exit;
    }

    pVfPrimary = &(pDomain40ProgPrimary->railVfItem[railIdx].data.primary);

    // Iterate over this VF_PRIMARY's CLK_VF_REL objects.
    for (vfRelIdx  = pVfPrimary->clkVfRelIdxFirst;
         vfRelIdx <= pVfPrimary->clkVfRelIdxLast;
         vfRelIdx++)
    {
        CLK_VF_REL *pVfRel;

        pVfRel = BOARDOBJGRP_OBJ_GET(CLK_VF_REL, vfRelIdx);
        if (pVfRel == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkDomainProgFreqToVolt_40_exit;
        }

        status = clkVfRelFreqToVolt(pVfRel,
                    pDomain40Prog,   // pDomain40Prog
                    voltageType,     // voltageType
                    pInput,          // pInput
                    pOutput);        // pOutput
        //
        // If the CLK_VF_REL encountered VF points above
        // pInput->value, the search is done.  Can short-circuit the
        // rest of the search and return FLCN_OK.
        //
        if (FLCN_ERR_ITERATION_END == status)
        {
            status = FLCN_OK;
            goto clkDomainProgFreqToVolt_40_exit;
        }

        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomainProgFreqToVolt_40_exit;
        }
    }

clkDomainProgFreqToVolt_40_exit:
    return status;
}

/*!
 * @copydoc ClkDomainProgVoltToFreqTuple
 */
FLCN_STATUS
clkDomainProgVoltToFreqTuple_40
(
    CLK_DOMAIN_PROG                                *pDomainProg,
    LwU8                                            railIdx,
    LwU8                                            voltageType,
    LW2080_CTRL_CLK_VF_INPUT                       *pInput,
    LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE    *pOutput,
    LW2080_CTRL_CLK_VF_ITERATION_STATE             *pVfIterState
)
{
    CLK_DOMAIN_40_PROG *pDomain40Prog =
        (CLK_DOMAIN_40_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    CLK_DOMAIN_40_PROG *pDomain40ProgPrimary = pDomain40Prog;
    CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY
                       *pVfPrimary           = NULL;
    CLK_VF_REL         *pVfRel              = NULL;
    LwU8                vfRelIdx;
    FLCN_STATUS         status              = FLCN_ERR_NOT_SUPPORTED;

    // Sanity checks.
    if ((pInput       == NULL) &&
        (pOutput      == NULL) &&
        (pVfIterState == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkDomain40ProgVoltToFreqTuple_exit;
    }

    if ((pInput->value == LW2080_CTRL_CLK_VF_VALUE_ILWALID) ||
        !clkDomain40ProgRailIdxValid(pDomain40Prog, railIdx))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkDomain40ProgVoltToFreqTuple_exit;
    }

    // Init the output struct
    memset(pOutput, 0, sizeof(*pOutput));

    // If secondary, get corresponding primary.
    if (clkDomain40ProgGetRailVfType(pDomain40Prog, railIdx) ==
        LW2080_CTRL_CLK_CLK_DOMAIN_40_PROG_RAIL_VF_TYPE_SECONDARY)
    {
        LwU8 primaryIdx = pDomain40Prog->railVfItem[railIdx].data.secondary.primaryIdx;

        pDomain40ProgPrimary = CLK_CLK_DOMAIN_40_PROG_GET_BY_IDX(primaryIdx);
    }

    if (pDomain40ProgPrimary == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto clkDomain40ProgVoltToFreqTuple_exit;
    }

    pVfPrimary = &(pDomain40ProgPrimary->railVfItem[railIdx].data.primary);

    // Init the iterator.
    pVfIterState->clkProgIdx = LW_MAX(pVfPrimary->clkVfRelIdxFirst, pVfIterState->clkProgIdx);

    // Iterate over this VF_PRIMARY's CLK_VF_REL objects.
    for (vfRelIdx  = pVfIterState->clkProgIdx;
         vfRelIdx <= pVfPrimary->clkVfRelIdxLast;
         vfRelIdx++)
    {
        pVfRel = BOARDOBJGRP_OBJ_GET(CLK_VF_REL, vfRelIdx);
        if (pVfRel == NULL)
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto clkDomain40ProgVoltToFreqTuple_exit;
        }

        status = clkVfRelVoltToFreqTupleVfIdxGet(pVfRel,
                    pDomain40Prog,   // pDomain40Prog
                    voltageType,     // voltageType
                    pInput,          // pInput
                    pVfIterState);   // pVfIterState
        if (status == FLCN_ERR_OUT_OF_RANGE)
        {
            //
            // If we hit the out of range error on the first VF point of the first
            // VF Rel entry, return Error unless user explicitly requested default
            // values.
            //
            if (vfRelIdx == pVfPrimary->clkVfRelIdxFirst)
            {
                if (FLD_TEST_DRF(2080_CTRL_CLK, _VF_INPUT_FLAGS, _VF_POINT_SET_DEFAULT, _NO,
                                 pInput->flags))
                {
                    PMU_BREAKPOINT();
                    goto clkDomain40ProgVoltToFreqTuple_exit;
                }

                pVfIterState->clkProgIdx = vfRelIdx;

                pVfRel = BOARDOBJGRP_OBJ_GET(CLK_VF_REL, pVfIterState->clkProgIdx);
                if (pVfRel == NULL)
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_INDEX;
                    goto clkDomain40ProgVoltToFreqTuple_exit;
                }

                pVfIterState->clkVfPointIdx =
                    clkVfRelVfPointIdxFirstGet(pVfRel,
                                               LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI);
            }
            else
            {
                //
                // Update itertor Vf Rel Idx to prev Vf Rel Idx and Vf Point Idx
                // to last vf point index of the prev Vf Rel Idx.
                //
                pVfIterState->clkProgIdx = (vfRelIdx - 1U);

                pVfRel = BOARDOBJGRP_OBJ_GET(CLK_VF_REL, pVfIterState->clkProgIdx);
                if (pVfRel == NULL)
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_INDEX;
                    goto clkDomain40ProgVoltToFreqTuple_exit;
                }

                pVfIterState->clkVfPointIdx =
                    clkVfRelVfPointIdxLastGet(pVfRel,
                                              LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI);
            }

            // This is not an error.
            status = FLCN_OK;

            break;
        }
        //
        // If the CLK_VF_REL encountered VF points above
        // pInput->value, the search is done.  Can short-circuit the
        // rest of the search and return FLCN_OK.
        //
        else if (status == FLCN_ERR_ITERATION_END)
        {
            // This is not an error.
            status = FLCN_OK;

            // Update the iterator index.
            pVfIterState->clkProgIdx = vfRelIdx;

            break;
        }
        else if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomain40ProgVoltToFreqTuple_exit;
        }
        else
        {
            //
            // If proceeding to the next VF_REL entry, clear the CLK_VF_POINT index
            // iterator state, as it's not applicable to the next VF_REL.
            //
            if (vfRelIdx < pVfPrimary->clkVfRelIdxLast)
            {
                pVfIterState->clkVfPointIdx = 0U;
            }
        }
    }

    // If input voltage is higher than max VF point voltage, select the last VF point.
    if (vfRelIdx > pVfPrimary->clkVfRelIdxLast)
    {
        pVfIterState->clkProgIdx = pVfPrimary->clkVfRelIdxLast;

        pVfRel = BOARDOBJGRP_OBJ_GET(CLK_VF_REL, pVfIterState->clkProgIdx);
        if (pVfRel == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkDomain40ProgVoltToFreqTuple_exit;
        }

        pVfIterState->clkVfPointIdx =
            clkVfRelVfPointIdxLastGet(pVfRel,
                LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI);
    }

    pVfRel = BOARDOBJGRP_OBJ_GET(CLK_VF_REL, pVfIterState->clkProgIdx);
    if (pVfRel == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto clkDomain40ProgVoltToFreqTuple_exit;
    }

    status = clkVfRelVoltToFreqTuple(pVfRel,    // pVfRel
                pDomain40Prog,                  // pDomain40Prog
                pVfIterState->clkVfPointIdx,    // clkVfPointIdx
                pOutput);                       // pOutput
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomain40ProgVoltToFreqTuple_exit;
    }

clkDomain40ProgVoltToFreqTuple_exit:
    return status;
}

/*!
 * @copydoc ClkDomainProgFreqQuantize
 */
FLCN_STATUS
clkDomainProgFreqQuantize_40
(
    CLK_DOMAIN_PROG            *pDomainProg,
    LW2080_CTRL_CLK_VF_INPUT   *pInputFreq,
    LW2080_CTRL_CLK_VF_OUTPUT  *pOutputFreq,
    LwBool                      bFloor,
    LwBool                      bBound
)
{
    CLK_DOMAIN_40_PROG *pDomain40Prog =
        (CLK_DOMAIN_40_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    FLCN_STATUS         status        = FLCN_ERR_ILWALID_ARGUMENT;
    LwU8                i;

    //
    // Iterate over set of CLK_ENUM structures, to find the highest quantized
    // frequency.
    //
    for (i = pDomain40Prog->clkEnumIdxFirst; i <= pDomain40Prog->clkEnumIdxLast;
        i++)
    {
        CLK_ENUM *pEnum = (CLK_ENUM *)BOARDOBJGRP_OBJ_GET(CLK_ENUM, i);

        if (pEnum == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkDomainProgFreqQuantize_40_exit;
        }

        // CLK_ENUM frequency quantization interface.
        status = clkEnumFreqQuantize(pEnum,
                                     pDomain40Prog,
                                     pInputFreq,
                                     pOutputFreq,
                                     bFloor,
                                     bBound);
        if (status == FLCN_ERR_OUT_OF_RANGE)
        {
            // Continue to next higher enum entry on out of range error.
        }
        else if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomainProgFreqQuantize_40_exit;
        }

        if (LW2080_CTRL_CLK_VF_VALUE_ILWALID != pOutputFreq->inputBestMatch)
        {
            // Match found. Jump to exit
            goto clkDomainProgFreqQuantize_40_exit;
        }
    }

    //
    // Handle case where input freq > max Freq of last CLK_ENUM.
    // Set bestMatch to max frequency to stop searching.
    //
    if ((bFloor) &&
        (LW2080_CTRL_CLK_VF_VALUE_ILWALID == pOutputFreq->inputBestMatch) &&
        (pOutputFreq->value != LW2080_CTRL_CLK_VF_VALUE_ILWALID))
    {
        pOutputFreq->inputBestMatch = pOutputFreq->value;

        // Update status to OK.
        status = FLCN_OK;
        goto clkDomainProgFreqQuantize_40_exit;
    }

    //
    // If the input frequency is above the max frequency supported by last
    // clock programming index and client requested !bFloor, set the output
    // frequency as max frequency supported by last clock programming index,
    // if the return error is @ref FLCN_ERR_OUT_OF_RANGE and clint requested
    // default values.
    //
    if ((FLCN_ERR_OUT_OF_RANGE == status) &&
        (!bFloor) &&
        (FLD_TEST_DRF(2080_CTRL_CLK, _VF_INPUT_FLAGS, _VF_POINT_SET_DEFAULT, _YES, pInputFreq->flags)) &&
        (LW2080_CTRL_CLK_VF_VALUE_ILWALID == pOutputFreq->inputBestMatch))
    {
        pOutputFreq->inputBestMatch = pOutputFreq->value;

        // Update status to OK.
        status = FLCN_OK;
        goto clkDomainProgFreqQuantize_40_exit;
    }

clkDomainProgFreqQuantize_40_exit:
    return status;
}

/*!
 * @copydoc ClkDomainProgFreqEnumIterate
 */
FLCN_STATUS
clkDomainProgFreqEnumIterate_40
(
    CLK_DOMAIN_PROG    *pDomainProg,
    LwU16              *pFreqMHz
)
{
    CLK_DOMAIN_40_PROG *pDomain40Prog =
        (CLK_DOMAIN_40_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    FLCN_STATUS         status        = FLCN_ERR_NOT_SUPPORTED;
    LwS16               i;

    // Iterate through the set of CLK_PROG entries to find best matches.
    for (i = pDomain40Prog->clkEnumIdxLast;
            i >= pDomain40Prog->clkEnumIdxFirst; i--)
    {
        CLK_ENUM *pEnum = (CLK_ENUM *)BOARDOBJGRP_OBJ_GET(CLK_ENUM, i);

        if (pEnum == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkDomainProgFreqEnumIterate_40_exit;
        }

        status  = clkEnumFreqsIterate(pEnum,
                                      pDomain40Prog,
                                      pFreqMHz);
        if (status == FLCN_ERR_ITERATION_END)
        {
            if (i == pDomain40Prog->clkEnumIdxFirst)
            {
                goto clkDomainProgFreqEnumIterate_40_exit;
            }

            // Go to next lower clk enum entry.
            continue;
        }
        else if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomainProgFreqEnumIterate_40_exit;
        }

        // Found the next lower frequency or reached the lowest end.
        break;
    }

clkDomainProgFreqEnumIterate_40_exit:
    return status;
}

/*!
 * @copydoc ClkDomainLoad
 */
FLCN_STATUS
clkDomainLoad_40_PROG
(
    CLK_DOMAIN *pDomain
)
{
    CLK_DOMAIN_40_PROG     *pDomain40Prog = (CLK_DOMAIN_40_PROG *)pDomain;
    CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY
                           *pVfPrimary     = NULL;
    LwU8                    railIdx;
    LwU8                    vfRelIdx;
    LwU8                    lwrveIdx;
    FLCN_STATUS             status = FLCN_OK;

    CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY_ITERATOR_BEGIN(pDomain40Prog, pVfPrimary, railIdx)
    {
        for (lwrveIdx = 0;
             lwrveIdx < clkDomain40ProgGetClkDomailwfLwrveCount(pDomain40Prog);
             lwrveIdx++)
        {
            //
            // Iterate over this VF_PRIMARY's CLK_VF_REL objects and load each
            // of them in order.
            //
            for (vfRelIdx =  pVfPrimary->clkVfRelIdxFirst;
                 vfRelIdx <= pVfPrimary->clkVfRelIdxLast;
                 vfRelIdx++)
            {
                CLK_VF_REL *pVfRel;

                pVfRel = BOARDOBJGRP_OBJ_GET(CLK_VF_REL, vfRelIdx);
                if (pVfRel == NULL)
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_INDEX;
                    goto clkDomainLoad_40_PROG_exit;
                }

                status = clkVfRelLoad(pVfRel, pDomain40Prog, lwrveIdx);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto clkDomainLoad_40_PROG_exit;
                }
            }
        }
    }
    CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY_ITERATOR_END;

clkDomainLoad_40_PROG_exit:
    return status;
}

/*!
 * @copydoc ClkDomainLoad
 */
FLCN_STATUS
clkDomainCache_40_PROG
(
    CLK_DOMAIN *pDomain,
    LwBool      bVFEEvalRequired
)
{
    CLK_DOMAIN_40_PROG     *pDomain40Prog = (CLK_DOMAIN_40_PROG *)pDomain;
    CLK_DOMAIN             *pDomainSecondary;
    CLK_VF_POINT_40        *pVfPoint40Last;
    CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY
                           *pVfPrimary     = NULL;
    LwU8                    railIdx;
    LwU8                    lwrveIdx;
    LwBoardObjIdx           secondaryClkIdx;
    FLCN_STATUS             status = FLCN_OK;

    //
    // Set the domain's index at which base lwrve trimming
    // (itself based on ENUM max frequency) starts, to invalid,
    // which will then be re-instantiated. To be done only if
    // bVFEEvalRequired is true (base lwrve will be regenerated).
    //
    if (bVFEEvalRequired)
    {
        CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY_ITERATOR_BEGIN(pDomain40Prog, pVfPrimary, railIdx)
        {
            for (lwrveIdx = 0;
                 lwrveIdx < clkDomain40ProgGetClkDomailwfLwrveCount(pDomain40Prog);
                 lwrveIdx++)
            {
                //
                // No need to set primary domain's index, since
                // this field only applies to secondary domains.
                //

                // Set the secondary domain's index
                BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomainSecondary, secondaryClkIdx,
                    (&(clkDomain40ProgGetSecondaryDomainsMask(pDomain40Prog, railIdx)->super)))
                {
                    CLK_DOMAIN_40_PROG *pDomain40ProgSecondary =
                        (CLK_DOMAIN_40_PROG *)pDomainSecondary;
                    clkDomain40ProgBaseFreqEnumMaxTrimVfPointIdxSet(
                        pDomain40ProgSecondary,
                        railIdx,
                        lwrveIdx,
                        LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID);
                }
                BOARDOBJGRP_ITERATOR_END;
            }
        }
        CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY_ITERATOR_END;
    }

    // 1. Generate Primary's VF Lwrve
    CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY_ITERATOR_BEGIN(pDomain40Prog, pVfPrimary, railIdx)
    {
        for (lwrveIdx = 0;
             lwrveIdx < clkDomain40ProgGetClkDomailwfLwrveCount(pDomain40Prog);
             lwrveIdx++)
        {
            LwU8 vfRelIdx;

            // Create a VF point pointer on stack to track the last VF point.
            pVfPoint40Last = NULL;

            //
            // Iterate over this VF_PRIMARY's CLK_VF_REL objects and cache each
            // of them in order.
            //
            for (vfRelIdx =  pVfPrimary->clkVfRelIdxFirst;
                 vfRelIdx <= pVfPrimary->clkVfRelIdxLast;
                 vfRelIdx++)
            {
                CLK_VF_REL *pVfRel;

                pVfRel = BOARDOBJGRP_OBJ_GET(CLK_VF_REL, vfRelIdx);
                if (pVfRel == NULL)
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_INDEX;
                    goto clkDomainCache_40_PROG_exit;
                }

                status = clkVfRelCache(pVfRel,
                                      pDomain40Prog,
                                      bVFEEvalRequired,
                                      lwrveIdx,
                                      pVfPoint40Last);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto clkDomainCache_40_PROG_exit;
                }
            }
        }
    }
    CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY_ITERATOR_END;

    // 2. Smooth Primary's VF lwrve based on POR max allowed step size.
    if (clkDomainsVfSmootheningEnforced())
    {
        CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY_ITERATOR_BEGIN(pDomain40Prog, pVfPrimary, railIdx)
        {
            for (lwrveIdx = 0;
                 lwrveIdx < clkDomain40ProgGetClkDomailwfLwrveCount(pDomain40Prog);
                 lwrveIdx++)
            {
                LwS16 vfRelIdx; // Must be signed for reverse iteration.

                // Create a VF point pointer on stack to track the last VF point.
                CLK_VF_POINT_40_VOLT *pVfPoint40VoltLast = NULL;

                //
                // Iterate over this VF_PRIMARY's CLK_VF_REL objects and smooth each
                // of them in order.
                //
                for (vfRelIdx =  pVfPrimary->clkVfRelIdxLast;
                     vfRelIdx >= pVfPrimary->clkVfRelIdxFirst;
                     vfRelIdx--)
                {
                    CLK_VF_REL *pVfRel;

                    pVfRel = BOARDOBJGRP_OBJ_GET(CLK_VF_REL, vfRelIdx);
                    if (pVfRel == NULL)
                    {
                        PMU_BREAKPOINT();
                        status = FLCN_ERR_ILWALID_INDEX;
                        goto clkDomainCache_40_PROG_exit;
                    }

                    status = clkVfRelSmoothing(pVfRel,
                                pDomain40Prog,
                                lwrveIdx,
                                CLK_CLK_VF_POINT_VF_CACHE_IDX_ILWALID,
                                pVfPoint40VoltLast);
                    if (status != FLCN_OK)
                    {
                        PMU_BREAKPOINT();
                        goto clkDomainCache_40_PROG_exit;
                    }
                }
            }
        }
        CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY_ITERATOR_END;
    }

    // 3. Generate Secondary's VF Lwrve
    CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY_ITERATOR_BEGIN(pDomain40Prog, pVfPrimary, railIdx)
    {
        // If the clock domain does not contain any secondary clocks, short-circuit.
        if ((clkDomain40ProgGetSecondaryDomainsMask(pDomain40Prog, railIdx) == NULL) ||
            (boardObjGrpMaskIsZero(clkDomain40ProgGetSecondaryDomainsMask(pDomain40Prog, railIdx))))
        {
            continue;
        }

        for (lwrveIdx = 0;
             lwrveIdx < clkDomain40ProgGetClkDomailwfLwrveCount(pDomain40Prog);
             lwrveIdx++)
        {
            LwU8 vfRelIdx;

            // Create a VF point pointer on stack to track the last VF point.
            pVfPoint40Last = NULL;

            //
            // Iterate over this VF_PRIMARY's CLK_VF_REL objects and cache each
            // of them in order.
            //
            for (vfRelIdx =  pVfPrimary->clkVfRelIdxFirst;
                 vfRelIdx <= pVfPrimary->clkVfRelIdxLast;
                 vfRelIdx++)
            {
                CLK_VF_REL *pVfRel;

                pVfRel = BOARDOBJGRP_OBJ_GET(CLK_VF_REL, vfRelIdx);
                if (pVfRel == NULL)
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_INDEX;
                    goto clkDomainCache_40_PROG_exit;
                }

                status = clkVfRelSecondaryCache(pVfRel,
                                            pDomain40Prog,
                                            bVFEEvalRequired,
                                            lwrveIdx,
                                            pVfPoint40Last);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto clkDomainCache_40_PROG_exit;
                }
            }
        }
    }
    CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY_ITERATOR_END;

    //
    // If the secondary domain base frequency never reaches enum-max, then
    // set the trimIdx to the last VF point index.
    //
    // This is only done when base lwrve is generated, as
    // the trimIdx is set when base lwrve is generated and
    // used when base lwrve is skipped.
    //
    if (bVFEEvalRequired)
    {
        CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY_ITERATOR_BEGIN(pDomain40Prog, pVfPrimary, railIdx)
        {
            for (lwrveIdx = 0;
                 lwrveIdx < clkDomain40ProgGetClkDomailwfLwrveCount(pDomain40Prog);
                 lwrveIdx++)
            {
                BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomainSecondary, secondaryClkIdx,
                    (&(clkDomain40ProgGetSecondaryDomainsMask(pDomain40Prog, railIdx)->super)))
                {
                    CLK_DOMAIN_40_PROG *pDomain40ProgSecondary  =
                            (CLK_DOMAIN_40_PROG *)pDomainSecondary;
                    LwBoardObjIdx freqEnumTrimIdx           =
                        clkDomain40ProgBaseFreqEnumMaxTrimVfPointIdxGet(
                            pDomain40ProgSecondary,
                            railIdx,
                            lwrveIdx);
                    LwU8 vfRelIdxLast                       =
                        pVfPrimary->clkVfRelIdxLast;
                    CLK_VF_REL *pVfRelLast                  =
                        BOARDOBJGRP_OBJ_GET(CLK_VF_REL, vfRelIdxLast);
                    //
                    // Not checking pVfRelLast == NULL, this would have been
                    // checked in the for-loop going through vf-rel just above.
                    //
                    LwBoardObjIdx vfPointIdxEnd = clkVfRelVfPointIdxLastGet(
                                                        pVfRelLast,
                                                        lwrveIdx);

                    // Do the actual check.
                    if (freqEnumTrimIdx == LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID)
                    {
                        clkDomain40ProgBaseFreqEnumMaxTrimVfPointIdxSet(
                                pDomain40ProgSecondary,
                                railIdx,
                                lwrveIdx,
                                vfPointIdxEnd);
                    }
                }
                BOARDOBJGRP_ITERATOR_END;
            }
        }
        CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY_ITERATOR_END;
    }

clkDomainCache_40_PROG_exit:
    return status;
}

/*!
 * @copydoc ClkDomainOffsetVFCache
 */
FLCN_STATUS
clkDomainOffsetVFCache_40_PROG
(
    CLK_DOMAIN *pDomain
)
{
    CLK_DOMAIN_40_PROG     *pDomain40Prog = (CLK_DOMAIN_40_PROG *)pDomain;
    CLK_VF_POINT_40        *pVfPoint40Last;
    CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY
                           *pVfPrimary     = NULL;
    LwU8                    railIdx;
    LwU8                    lwrveIdx;
    FLCN_STATUS             status = FLCN_OK;

    // 1. Generate Primary's VF Lwrve
    CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY_ITERATOR_BEGIN(pDomain40Prog, pVfPrimary, railIdx)
    {
        for (lwrveIdx = 0;
             lwrveIdx < clkDomain40ProgGetClkDomailwfLwrveCount(pDomain40Prog);
             lwrveIdx++)
        {
            LwU8 vfRelIdx;

            // Create a VF point pointer on stack to track the last VF point.
            pVfPoint40Last = NULL;

            //
            // Iterate over this VF_PRIMARY's CLK_VF_REL objects and cache each
            // of them in order.
            //
            for (vfRelIdx =  pVfPrimary->clkVfRelIdxFirst;
                 vfRelIdx <= pVfPrimary->clkVfRelIdxLast;
                 vfRelIdx++)
            {
                CLK_VF_REL *pVfRel;

                pVfRel = BOARDOBJGRP_OBJ_GET(CLK_VF_REL, vfRelIdx);
                if (pVfRel == NULL)
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_INDEX;
                    goto clkDomainOffsetVFCache_40_PROG_exit;
                }

                status = clkVfRelOffsetVFCache(pVfRel,
                                               pDomain40Prog,
                                               lwrveIdx,
                                               pVfPoint40Last);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto clkDomainOffsetVFCache_40_PROG_exit;
                }
            }
        }
    }
    CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY_ITERATOR_END;
    
    // 2. Generate Secondary's VF Lwrve
    CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY_ITERATOR_BEGIN(pDomain40Prog, pVfPrimary, railIdx)
    {
        // If the clock domain does not contain any secondary clocks, short-circuit.
        if ((clkDomain40ProgGetSecondaryDomainsMask(pDomain40Prog, railIdx) == NULL) ||
            (boardObjGrpMaskIsZero(clkDomain40ProgGetSecondaryDomainsMask(pDomain40Prog, railIdx))))
        {
            continue;
        }

        for (lwrveIdx = 0;
             lwrveIdx < clkDomain40ProgGetClkDomailwfLwrveCount(pDomain40Prog);
             lwrveIdx++)
        {
            LwU8 vfRelIdx;

            // Create a VF point pointer on stack to track the last VF point.
            pVfPoint40Last = NULL;

            //
            // Iterate over this VF_PRIMARY's CLK_VF_REL objects and cache each
            // of them in order.
            //
            for (vfRelIdx =  pVfPrimary->clkVfRelIdxFirst;
                 vfRelIdx <= pVfPrimary->clkVfRelIdxLast;
                 vfRelIdx++)
            {
                CLK_VF_REL *pVfRel;

                pVfRel = BOARDOBJGRP_OBJ_GET(CLK_VF_REL, vfRelIdx);
                if (pVfRel == NULL)
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_INDEX;
                    goto clkDomainOffsetVFCache_40_PROG_exit;
                }

                status = clkVfRelSecondaryOffsetVFCache(pVfRel,
                                                    pDomain40Prog,
                                                    lwrveIdx,
                                                    pVfPoint40Last);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto clkDomainOffsetVFCache_40_PROG_exit;
                }
            }
        }
    }
    CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY_ITERATOR_END;
clkDomainOffsetVFCache_40_PROG_exit:
    return status;
}

/*!
 * @copydoc ClkDomainBaseVFCache
 */
FLCN_STATUS
clkDomainBaseVFCache_40_PROG
(
    CLK_DOMAIN *pDomain,
    LwU8        cacheIdx
)
{
    CLK_DOMAIN_40_PROG     *pDomain40Prog = (CLK_DOMAIN_40_PROG *)pDomain;
    CLK_VF_POINT_40        *pVfPoint40Last;
    CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY
                           *pVfPrimary     = NULL;
    LwU8                    railIdx;
    LwU8                    lwrveIdx;
    FLCN_STATUS             status = FLCN_OK;

    // 1. Generate Primary's VF Lwrve
    CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY_ITERATOR_BEGIN(pDomain40Prog, pVfPrimary, railIdx)
    {
        for (lwrveIdx = 0;
             lwrveIdx < clkDomain40ProgGetClkDomailwfLwrveCount(pDomain40Prog);
             lwrveIdx++)
        {
            LwU8 vfRelIdx;

            // Create a VF point pointer on stack to track the last VF point.
            pVfPoint40Last = NULL;

            //
            // Iterate over this VF_PRIMARY's CLK_VF_REL objects and cache each
            // of them in order.
            //
            for (vfRelIdx =  pVfPrimary->clkVfRelIdxFirst;
                 vfRelIdx <= pVfPrimary->clkVfRelIdxLast;
                 vfRelIdx++)
            {
                CLK_VF_REL *pVfRel;

                pVfRel = BOARDOBJGRP_OBJ_GET(CLK_VF_REL, vfRelIdx);
                if (pVfRel == NULL)
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_INDEX;
                    goto clkDomainBaseVFCache_40_PROG_exit;
                }

                status = clkVfRelBaseVFCache(pVfRel,
                                      pDomain40Prog,
                                      lwrveIdx,
                                      cacheIdx,
                                      pVfPoint40Last);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto clkDomainBaseVFCache_40_PROG_exit;
                }
            }
        }
    }
    CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY_ITERATOR_END;

    // 2. Generate Secondary's VF Lwrve
    CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY_ITERATOR_BEGIN(pDomain40Prog, pVfPrimary, railIdx)
    {
        for (lwrveIdx = 0;
             lwrveIdx < clkDomain40ProgGetClkDomailwfLwrveCount(pDomain40Prog);
             lwrveIdx++)
        {
            LwU8 vfRelIdx;

            // Create a VF point pointer on stack to track the last VF point.
            pVfPoint40Last = NULL;

            //
            // Iterate over this VF_PRIMARY's CLK_VF_REL objects and cache each
            // of them in order.
            //
            for (vfRelIdx =  pVfPrimary->clkVfRelIdxFirst;
                 vfRelIdx <= pVfPrimary->clkVfRelIdxLast;
                 vfRelIdx++)
            {
                CLK_VF_REL *pVfRel;

                pVfRel = BOARDOBJGRP_OBJ_GET(CLK_VF_REL, vfRelIdx);
                if (pVfRel == NULL)
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_INDEX;
                    goto clkDomainBaseVFCache_40_PROG_exit;
                }

                status = clkVfRelSecondaryBaseVFCache(pVfRel,
                                            pDomain40Prog,
                                            lwrveIdx,
                                            cacheIdx,
                                            pVfPoint40Last);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto clkDomainBaseVFCache_40_PROG_exit;
                }
            }
        }
    }
    CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY_ITERATOR_END;

clkDomainBaseVFCache_40_PROG_exit:
    return status;
}

/*!
 * @copydoc ClkDomain40ProgVoltAdjustDeltauV
 */
FLCN_STATUS
clkDomain40ProgVoltAdjustDeltauV
(
    CLK_DOMAIN_40_PROG     *pDomain40Prog,
    CLK_VF_REL             *pVfRel,
    LwU32                  *pVoltuV
)
{
    LwU32       voltOriguV = *pVoltuV;
    FLCN_STATUS status     = FLCN_ERR_NOT_SUPPORTED;

    switch (clkDomain40ProgGetRailVfType(pDomain40Prog, pVfRel->railIdx))
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_40_PROG_RAIL_VF_TYPE_PRIMARY:
        {
            status = s_clkDomain40ProgVoltAdjustDeltauV_PRIMARY(pDomain40Prog,
                        pVfRel, pVoltuV);
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_40_PROG_RAIL_VF_TYPE_SECONDARY:
        {
            status = s_clkDomain40ProgVoltAdjustDeltauV_SECONDARY(pDomain40Prog,
                        pVfRel, pVoltuV);
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
        goto clkDomain40ProgVoltAdjustDeltauV_exit;
    }

    //
    // If voltage offset has been applied to this voltage rail, round up to the
    // next regulator step size.
    //
    if (voltOriguV != *pVoltuV)
    {
        VOLT_RAIL *pVoltRail = VOLT_RAIL_GET(pVfRel->railIdx);

        if (pVoltRail == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkDomain40ProgVoltAdjustDeltauV_exit;
        }

        status = voltRailRoundVoltage(pVoltRail,        // pRail
                                      (LwS32*)pVoltuV,  // pVoltageuV
                                      LW_TRUE,          // bRoundUp
                                      LW_TRUE);         // bBound
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomain40ProgVoltAdjustDeltauV_exit;
        }
    }

clkDomain40ProgVoltAdjustDeltauV_exit:
    return status;
}

/*!
 * @copydoc ClkDomain40ProgFreqAdjustDeltaMHz
 */
FLCN_STATUS
clkDomain40ProgFreqAdjustDeltaMHz
(
    CLK_DOMAIN_40_PROG     *pDomain40Prog,
    CLK_VF_REL             *pVfRel,
    LwU16                  *pFreqMHz
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (clkDomain40ProgGetRailVfType(pDomain40Prog, pVfRel->railIdx))
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_40_PROG_RAIL_VF_TYPE_PRIMARY:
        {
            status = s_clkDomain40ProgFreqAdjustDeltaMHz_PRIMARY(pDomain40Prog,
                        pVfRel, pFreqMHz);
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_40_PROG_RAIL_VF_TYPE_SECONDARY:
        {
            status = s_clkDomain40ProgFreqAdjustDeltaMHz_SECONDARY(pDomain40Prog,
                        pVfRel, pFreqMHz);
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
        goto clkDomain40ProgFreqAdjustDeltaMHz_exit;
    }

clkDomain40ProgFreqAdjustDeltaMHz_exit:
    return status;
}

/*!
 * @copydoc ClkDomain40ProgOffsetVFVoltAdjustDeltauV
 */
FLCN_STATUS
clkDomain40ProgOffsetVFVoltAdjustDeltauV
(
    CLK_DOMAIN_40_PROG     *pDomain40Prog,
    CLK_VF_REL             *pVfRel,
    LwS32                  *pVoltuV
)
{
    FLCN_STATUS status     = FLCN_ERR_NOT_SUPPORTED;

    switch (clkDomain40ProgGetRailVfType(pDomain40Prog, pVfRel->railIdx))
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_40_PROG_RAIL_VF_TYPE_PRIMARY:
        {
            status = s_clkDomain40ProgOffsetVFVoltAdjustDeltauV_PRIMARY(pDomain40Prog,
                        pVfRel, pVoltuV);
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_40_PROG_RAIL_VF_TYPE_SECONDARY:
        {
            status = s_clkDomain40ProgOffsetVFVoltAdjustDeltauV_SECONDARY(pDomain40Prog,
                        pVfRel, pVoltuV);
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
        goto clkDomain40ProgOffsetVFVoltAdjustDeltauV_exit;
    }

clkDomain40ProgOffsetVFVoltAdjustDeltauV_exit:
    return status;
}

/*!
 * @copydoc ClkDomain40ProgOffsetVFFreqAdjustDeltaMHz
 */
FLCN_STATUS
clkDomain40ProgOffsetVFFreqAdjustDeltaMHz
(
    CLK_DOMAIN_40_PROG     *pDomain40Prog,
    CLK_VF_REL             *pVfRel,
    LwS16                  *pFreqMHz
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (clkDomain40ProgGetRailVfType(pDomain40Prog, pVfRel->railIdx))
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_40_PROG_RAIL_VF_TYPE_PRIMARY:
        {
            status = s_clkDomain40ProgOffsetVFFreqAdjustDeltaMHz_PRIMARY(pDomain40Prog,
                        pVfRel, pFreqMHz);
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_40_PROG_RAIL_VF_TYPE_SECONDARY:
        {
            status = s_clkDomain40ProgOffsetVFFreqAdjustDeltaMHz_SECONDARY(pDomain40Prog,
                        pVfRel, pFreqMHz);
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
        goto clkDomain40ProgOffsetVFFreqAdjustDeltaMHz_exit;
    }

clkDomain40ProgOffsetVFFreqAdjustDeltaMHz_exit:
    return status;
}

/*!
 * @copydoc ClkDomainProgClientFreqDeltaAdj
 */
FLCN_STATUS
clkDomainProgClientFreqDeltaAdj_40
(
    CLK_DOMAIN_PROG    *pDomainProg,
    LwU16              *pFreqMHz
)
{
    CLK_DOMAIN_40_PROG *pDomain40Prog   =
        (CLK_DOMAIN_40_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    LwU16               offsetedFreqMHz = LW_U16_MAX;
    LwU8                railIdx;
    LwU8                vfRelIdx;
    FLCN_STATUS         status          = FLCN_OK;

    // Sanity check inputs.
    if ((pFreqMHz == NULL) ||
        ((*pFreqMHz) == 0U))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkDomainProgClientFreqDeltaAdj_40_exit;
    }

    // If OVOC is disabled, short circuit for early return.
    if (!clkDomain40ProgIsOCEnabled(pDomain40Prog))
    {
        goto clkDomainProgClientFreqDeltaAdj_40_exit;
    }

    // Iterate over each VOLTAGE_RAIL separately.
    FOR_EACH_INDEX_IN_MASK(32, railIdx, pDomain40Prog->railMask)
    {
        LwU16 railOffsetedFreqMHz = (*pFreqMHz);
        CLK_VF_REL *pVfRel;

        // Get VF rel index.
        vfRelIdx = s_clkDomain40ProgClkVfRelIdxGet(pDomain40Prog,
                                                   railIdx,
                                                  *pFreqMHz);
        if (vfRelIdx == LW2080_CTRL_CLK_CLK_VF_REL_IDX_ILWALID)
        {
            status = FLCN_ERR_ILWALID_INDEX;
            PMU_BREAKPOINT();
            goto clkDomainProgClientFreqDeltaAdj_40_exit;
        }

        pVfRel = BOARDOBJGRP_OBJ_GET(CLK_VF_REL, vfRelIdx);
        if (pVfRel == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkDomainProgClientFreqDeltaAdj_40_exit;
        }

        // Adjust frequency with offsets.
        status = clkVfRelClientFreqDeltaAdj(pVfRel,
                    pDomain40Prog, &railOffsetedFreqMHz);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomainProgClientFreqDeltaAdj_40_exit;
        }

        //
        // Min offset frequency of all rails as we want to hit the max allowed
        // freq without increasing voltage.
        //
        offsetedFreqMHz = LW_MIN(offsetedFreqMHz, railOffsetedFreqMHz);
    }
    FOR_EACH_INDEX_IN_MASK_END;

    // Adjust the input frequency by the offset.
    if (offsetedFreqMHz != LW_U16_MAX)
    {
        *pFreqMHz = offsetedFreqMHz;
    }
    //
    // offsetedFreqMHz == LW_U16_MAX means the input clock domain does not
    // have any dependency on available programmable voltage rails.
    //

    // Sanity check outputs.
    if ((*pFreqMHz) == 0U)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto clkDomainProgClientFreqDeltaAdj_40_exit;
    }

clkDomainProgClientFreqDeltaAdj_40_exit:
    return status;
}

/*!
 * @copydoc ClkDomainProgVfMaxFreqMHzGet
 */
FLCN_STATUS
clkDomainProgVfMaxFreqMHzGet_40
(
    CLK_DOMAIN_PROG    *pDomainProg,
    LwU32              *pFreqMaxMHz
)
{
    CLK_DOMAIN_40_PROG *pDomain40Prog =
        (CLK_DOMAIN_40_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    CLK_DOMAIN_40_PROG *pDomain40ProgPrimary = NULL;
    CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY
                       *pVfPrimary           = NULL;
    LwU8                railIdx;
    FLCN_STATUS         status              = FLCN_ERR_NOT_SUPPORTED;

    // Sanity check
    if (pFreqMaxMHz == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkDomainProgVfMaxFreqMHzGet_40_exit;
    }

    // Init to MAX.
    *pFreqMaxMHz = LW_U32_MAX;

    // Iterate over each VOLTAGE_RAIL separately.
    FOR_EACH_INDEX_IN_MASK(32, railIdx, pDomain40Prog->railMask)
    {
        LwU16 freqMaxMHz;
        CLK_VF_REL *pVfRel;

        // If secondary, get corresponding primary.
        if (clkDomain40ProgGetRailVfType(pDomain40Prog, railIdx) ==
            LW2080_CTRL_CLK_CLK_DOMAIN_40_PROG_RAIL_VF_TYPE_SECONDARY)
        {
            LwU8 primaryIdx = pDomain40Prog->railVfItem[railIdx].data.secondary.primaryIdx;

            pDomain40ProgPrimary = CLK_CLK_DOMAIN_40_PROG_GET_BY_IDX(primaryIdx);
        }
        else
        {
            // Update the primary pointer.
            pDomain40ProgPrimary = pDomain40Prog;
        }

        if (pDomain40ProgPrimary == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkDomainProgVfMaxFreqMHzGet_40_exit;
        }

        pVfPrimary = &(pDomain40ProgPrimary->railVfItem[railIdx].data.primary);

        pVfRel = BOARDOBJGRP_OBJ_GET(CLK_VF_REL, pVfPrimary->clkVfRelIdxLast);
        if (pVfRel == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkDomainProgVfMaxFreqMHzGet_40_exit;
        }

        status = clkVfRelVfMaxFreqMHzGet(pVfRel, pDomain40Prog, &freqMaxMHz);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomainProgVfMaxFreqMHzGet_40_exit;
        }

        // Capture min of all VF Max frequencies.
        (*pFreqMaxMHz) = LW_MIN(freqMaxMHz, (*pFreqMaxMHz));
    }
    FOR_EACH_INDEX_IN_MASK_END;

clkDomainProgVfMaxFreqMHzGet_40_exit:
    return status;
}

/*!
 * @copydoc ClkDomainProgIsFreqMHzNoiseAware
 */
LwBool
clkDomainProgIsFreqMHzNoiseAware_40
(
    CLK_DOMAIN_PROG    *pDomainProg,
    LwU32               freqMHz
)
{
    CLK_DOMAIN_40_PROG *pDomain40Prog =
        (CLK_DOMAIN_40_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    LwU8                railIdx;
    LwU8                vfRelIdx;
    LwBool              bNoiseAware   = LW_FALSE;

    FOR_EACH_INDEX_IN_MASK(32, railIdx, pDomain40Prog->railMask)
    {
        CLK_VF_REL *pVfRel;

        // Get VF rel index.
        vfRelIdx = s_clkDomain40ProgClkVfRelIdxGet(pDomain40Prog,
                                                   railIdx,
                                                   freqMHz);
        if (vfRelIdx == LW2080_CTRL_CLK_CLK_VF_REL_IDX_ILWALID)
        {
            bNoiseAware = LW_FALSE;
            PMU_BREAKPOINT();
            goto clkDomainProgIsFreqMHzNoiseAware_40_exit;
        }

        pVfRel = BOARDOBJGRP_OBJ_GET(CLK_VF_REL, vfRelIdx);
        if (pVfRel == NULL)
        {
            PMU_BREAKPOINT();
            bNoiseAware = LW_FALSE;
            goto clkDomainProgIsFreqMHzNoiseAware_40_exit;
        }

        bNoiseAware = clkVfRelIsNoiseAware(pVfRel);

        // If clock domain is noise aware on any of the rail, return TRUE.
        if (bNoiseAware)
        {
            break;
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

clkDomainProgIsFreqMHzNoiseAware_40_exit:
    return bNoiseAware;
}

/*!
 * @copydoc ClkDomainProgPreVoltOrderingIndexGet
 */
LwU8
clkDomainProgPreVoltOrderingIndexGet_40
(
    CLK_DOMAIN_PROG    *pDomainProg
)
{
    CLK_DOMAIN_40_PROG *pDomain40Prog =
        (CLK_DOMAIN_40_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);

    return pDomain40Prog->preVoltOrderingIndex;
}

/*!
 * @copydoc ClkDomainProgPostVoltOrderingIndexGet
 */
LwU8
clkDomainProgPostVoltOrderingIndexGet_40
(
    CLK_DOMAIN_PROG    *pDomainProg
)
{
    CLK_DOMAIN_40_PROG *pDomain40Prog =
        (CLK_DOMAIN_40_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);

    return pDomain40Prog->postVoltOrderingIndex;
}

/*!
 * @copydoc ClkDomainProgVoltRailIdxGet
 */
LwU8
clkDomainProgVoltRailIdxGet_40
(
    CLK_DOMAIN_PROG    *pDomainProg
)
{
    CLK_DOMAIN_40_PROG *pDomain40Prog =
        (CLK_DOMAIN_40_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    LwU32               railMask      = pDomain40Prog->railMask;
    LwU32               railCount     = lwPopCount32(railMask);

    //
    // We expect all programmable domain on TURING_AMPERE to have
    // exactly one voltage rail powering the clock domain.
    //
    if ((railCount == 0U) ||
        (railCount >  1U))
    {
        return LW2080_CTRL_BOARDOBJ_IDX_ILWALID_8BIT;
    }

    LOWESTBITIDX_32(railMask);

    return (LwU8)railMask;
}

/*!
 * @copydoc ClkDomainProgFbvddDataValid
 */
FLCN_STATUS
clkDomainProgFbvddDataValid_40
(
    CLK_DOMAIN_PROG    *pDomainProg,
    LwBool             *pbValid
)
{
    FLCN_STATUS status;
    CLK_DOMAIN_40_PROG *pDomain40Prog;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDomainProg != NULL) &&
        (pbValid != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        clkDomainProgFbvddDataValid_40_exit);

    // Get the 40_PROG pointer and ensure it's valid.
    pDomain40Prog =
        (CLK_DOMAIN_40_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDomain40Prog != NULL),
        FLCN_ERR_ILWALID_STATE,
        clkDomainProgFbvddDataValid_40_exit);

    *pbValid = pDomain40Prog->fbvddData.bValid;

clkDomainProgFbvddDataValid_40_exit:
    return status;
}

/*!
 * @copydoc ClkDomainProgFbvddPwrAdjust
 */
FLCN_STATUS
clkDomainProgFbvddPwrAdjust_40
(
    CLK_DOMAIN_PROG    *pDomainProg,
    LwU32              *pFbPwrmW,
    LwBool              bFromReference
)
{
    FLCN_STATUS status;
    CLK_DOMAIN_40_PROG *pDomain40Prog;
    union
    {
        LwUFXP52_12 u;
        LwSFXP52_12 s;
    } adjustedFbPwrmW52_12;
    LwS64 adjustedFbPwrmWS64;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDomainProg != NULL) && (pFbPwrmW != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        clkDomainProgFbvddPwrAdjust_40_exit);

    pDomain40Prog =
        (CLK_DOMAIN_40_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDomain40Prog != NULL),
        FLCN_ERR_ILWALID_STATE,
        clkDomainProgFbvddPwrAdjust_40_exit);

    // Ensure that the FBVDD data is available for this CLK_DOMAIN
    PMU_ASSERT_TRUE_OR_GOTO(status,
        pDomain40Prog->fbvddData.bValid,
        FLCN_ERR_ILWALID_ARGUMENT,
        clkDomainProgFbvddPwrAdjust_40_exit);

    if (bFromReference)
    {
        //
        // Numerical Analysis:
        // We need to perform the following callwlation to evaluate the
        // adjustment line with an "x" value of fbPwrmW = *pFbPwrmW
        //  adjustedFbPwrmW52_12 = fbPwrmW * slope + interceptmW
        //
        // fbPwrmW
        // We can assume that all power values are in the range:
        //  [0 kW, 1 kW)
        // Represented in mW, the upper limit requires 20 bits. This means that
        // fbPwrmW is represented in 32.0 with 20.0 significant bits.
        //
        // slope * fbPwrmW
        // We can assume that the slope is in the range:
        //  (1/16, 16)
        // Representing either of these requires 4 significant bits, so this
        // means that the slope is represented in unsigned 20.12 with 4.4
        // significant bits.
        //
        // As stated above, fbPwrmW is a 32.0 with 20.0 significant bits, so
        // multiplying it with slope results in an unsigned 52.12 with at most
        // 24.4 significant bits.
        //
        // To maintain precision, we will use a 64-bit 52.12 representation, so
        // we first cast both numbers to 64-bit and store the result in an
        // unsigned 52.12.
        //
        // + interceptmW
        // We can assume that interceptmW is in the range:
        //  [-16000 mW, 16000 mW]
        // Representing either of these requires 14 bits, so we will assume that
        // intercept is a signed 20.12 with 14.12 significant bits.
        //
        // The previous result, however, is an unsigned 52.12 with 24.4
        // significant bits, so the math must still be done in 64-bit.
        //
        // To do this, we:
        //  1.) Cast interceptmW to a signed 52.12 to sign extend it
        //  2.) Do the addition as unsigned by interpreting the cast interceptmW
        //      as an unsigned value.
        // This works because two's complement addition works irrespective of
        // signedness.
        //
        // The addition can add at most one significant bit, so the result is
        // then a signed 52.12 number with at most 25.12 significant bits.
        //
        // We round off the fractions of a mW to get back to a 32.0
        // representation, leaving us with 26.0 bits, which should be safe to
        // place back into the 32.0.
        //
        lwU64Mul(
            &adjustedFbPwrmW52_12.u,
            &(LwU64){ *pFbPwrmW },
            &(LwUFXP52_12){ pDomain40Prog->fbvddData.pwrAdjustment.slope });

        lw64Add(
            &adjustedFbPwrmW52_12.u,
            &adjustedFbPwrmW52_12.u,
            &(LwUFXP52_12){ (LwSFXP52_12)pDomain40Prog->fbvddData.pwrAdjustment.interceptmW });
    }
    else
    {
        //
        // Numerical Analysis:
        // We need to perform the following callwlation to evaluate the
        // ilwerse of the adjustment line with an "x" value of
        // fbPwrmW = *pFbPwrmW
        //  adjustedFbPwrmW52_12 = (fbPwrmW - interceptmW) / slope
        //
        // fbPwrmW
        // We can assume that all power values are in the range:
        //  [0 kW, 1 kW)
        // Represented in mW, the upper limit requires 20 bits. This means that
        // fbPwrmW is represented in 32.0 with 20.0 significant bits.
        //
        // fbPwrmW - interceptmW
        // We can assume that interceptmW is in the range:
        //  [-16000 mW, 16000 mW]
        // Representing either of these requires 14 bits, so we will assume that
        // intercept is a signed 20.12 with 14.12 significant bits.
        //
        // As stated above, fbPwrmW is a 32.0 with 20.0 significant bits. Since
        // interceptmW is signed, the subtraction may result in an additional
        // significant bit being added, so this means that the result may be a
        // 21.12 and so the subtraction must be done in 64-bit. Therefore, the
        // math will be done in signed 52.12.
        //
        // To do this, we:
        //  1.) Colwert fbPwrmW to an unsigned 52.12
        //  2.) Cast interceptmW to a signed 52.12 to sign extend it
        //  3.) Do the subtraction as unsigned by interpreting the cast
        //      interceptmW as an unsigned value.
        // This works because two's complement subtraction works irrespective of
        // signedness.
        //
        // This therefore results in a signed 52.12 with at most 21.12
        // significant bits.
        //
        // / slope
        // We can assume that the slope is in the range:
        //  (1/16, 16)
        // Representing either of these requires 4 significant bits, so this
        // means that the slope is represented in unsigned 20.12 with 4.4
        // significant bits.
        //
        // As callwlated above, the prior result is a signed 52.12 with at most
        // 21.12 significant bits. To divide by a 20.12, we need to pre-shift
        // the numerator by 12 to retain precision, resulting in a 40.24
        // representation, which still safely holds 21.12 significant bits.
        //
        // Then, slope is zero-extended to a 52.12 and the division is done in
        // unsigned 64-bit. Note that because at this point negative values are
        // meaningless anyway, we skip the division if the prior result is
        // negative.
        //
        // The result is then a signed 52.12 with at most 25.12 significant
        // bits.
        //
        // We round off the fractions of a mW to get back to a 32.0
        // representation, leaving us with 26.0 bits, which should be safe to
        // place back into the 32.0.
        //
        lw64Sub(
            &adjustedFbPwrmW52_12.u,
            &(LwUFXP52_12){ LW_TYPES_U64_TO_UFXP_X_Y(52, 12, *pFbPwrmW) },
            &(LwUFXP52_12){ (LwSFXP52_12)pDomain40Prog->fbvddData.pwrAdjustment.interceptmW });

        //
        // If the result is less than 0, we'll want to clamp it up to 0 anyway,
        // so only do the division if the result is greater than 0.
        //
        if (adjustedFbPwrmW52_12.s > 0)
        {
            lw64Lsl(&adjustedFbPwrmW52_12.u, &adjustedFbPwrmW52_12.u, 12);
            lwU64Div(
                &adjustedFbPwrmW52_12.u,
                &adjustedFbPwrmW52_12.u,
                &(LwUFXP52_12){ pDomain40Prog->fbvddData.pwrAdjustment.slope });
        }
    }

    //
    // First, retrieve the integer part of the adjusted value. Then (despite
    // the numerical analysis above), check to see if it's in the range of
    // values (i.e., [0, LW_U32_MAX]), and if not, clamp it to the range.
    //
    adjustedFbPwrmWS64 = LW_TYPES_SFXP_X_Y_TO_S64_ROUNDED(
        52, 12, adjustedFbPwrmW52_12.s);
    if (adjustedFbPwrmWS64 < 0)
    {
        adjustedFbPwrmWS64 = 0;
    }
    else if (adjustedFbPwrmWS64 > LW_U32_MAX)
    {
        adjustedFbPwrmWS64 = LW_U32_MAX;
    }
    else
    {
        // Result within valid range, nothing to correct.
    }

    *pFbPwrmW = (LwU32)adjustedFbPwrmWS64;

clkDomainProgFbvddPwrAdjust_40_exit:
    return status;
}

/*!
 * @copydoc ClkDomainProgFbvddFreqToVolt
 */
FLCN_STATUS
clkDomainProgFbvddFreqToVolt_40
(
    CLK_DOMAIN_PROG                *pDomainProg,
    const LW2080_CTRL_CLK_VF_INPUT *pInput,
    LW2080_CTRL_CLK_VF_OUTPUT      *pOutput
)
{
    FLCN_STATUS status;
    CLK_DOMAIN_40_PROG *pDomain40Prog;
    const LW2080_CTRL_CLK_CLK_DOMAIN_40_PROG_FBVDD_VF_MAPPING_TABLE *pMappingTable;
    LwBool bValid;
    LwU8 i;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDomainProg != NULL) &&
        (pInput != NULL) &&
        (pOutput != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        clkDomainProgFbvddFreqToVolt_40_exit);

    pDomain40Prog =
        (CLK_DOMAIN_40_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDomain40Prog != NULL),
        FLCN_ERR_ILWALID_STATE,
        clkDomainProgFbvddFreqToVolt_40_exit);

    // Ensure that the FBVDD data is available for this CLK_DOMAIN
    PMU_ASSERT_OK_OR_GOTO(status,
        clkDomainProgFbvddDataValid(
            &pDomain40Prog->prog,
            &bValid),
        clkDomainProgFbvddFreqToVolt_40_exit);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        bValid,
        FLCN_ERR_ILWALID_ARGUMENT,
        clkDomainProgFbvddFreqToVolt_40_exit);

    pMappingTable = &pDomain40Prog->fbvddData.vfMappingTable;
    for (i = 0U; i < pMappingTable->numMappings; i++)
    {
        if (pInput->value <= pMappingTable->mappings[i].maxFreqkHz)
        {
            pOutput->inputBestMatch = pMappingTable->mappings[i].maxFreqkHz;
            pOutput->value = pMappingTable->mappings[i].voltuV;
            break;
        }
    }

    //
    // If we walked the whole table without finding a match, then check the
    // _VF_POINT_SET_DEFAULT flag. If it's not set, then consider this an error.
    // Otherwise, use the last mapping entry as the voltage.
    //
    if (i == pMappingTable->numMappings)
    {
        PMU_ASSERT_TRUE_OR_GOTO(status,
            FLD_TEST_DRF(
                2080_CTRL_CLK, _VF_INPUT_FLAGS, _VF_POINT_SET_DEFAULT, _YES, pInput->flags),
            FLCN_ERR_OUT_OF_RANGE,
            clkDomainProgFbvddFreqToVolt_40_exit);

        pOutput->inputBestMatch = pMappingTable->mappings[i - 1U].maxFreqkHz;
        pOutput->value = pMappingTable->mappings[i - 1U].voltuV;
    }

clkDomainProgFbvddFreqToVolt_40_exit:
    return status;
}

/*!
 * @copydoc ClkDomainsProgFreqPropagate
 */
FLCN_STATUS
clkDomainsProgFreqPropagate_40
(
    CLK_DOMAIN_PROG_FREQ_PROP_IN_OUT_TUPLE *pFreqPropTuple,
    LwBoardObjIdx                           clkPropTopIdx
)
{
    FLCN_STATUS status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROP))
    {
        CLK_DOMAIN         *pDomainSrc;
        CLK_DOMAIN         *pDomainDst;
        CLK_PROP_TOP       *pPropTop;
        LwU16               freqMHz;
        LwBoardObjIdx       dstIdx;
        LwBoardObjIdx       srcIdx;

        // If the CLK_PROP_TOP index is invalid, get the active topology index
        if (clkPropTopIdx == LW2080_CTRL_CLK_CLK_PROP_TOP_IDX_ILWALID)
        {
            clkPropTopIdx = clkPropTopsGetTopologyIdxFromId(
                clkPropTopsGetActiveTopologyId(CLK_CLK_PROP_TOPS_GET()));
        }
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (clkPropTopIdx != LW2080_CTRL_CLK_CLK_PROP_TOP_IDX_ILWALID),
            FLCN_ERR_ILWALID_INDEX,
            clkDomainsProgFreqPropagate_40_exit);

        // Get the clock propagation topology pointer.
        pPropTop = CLK_PROP_TOP_GET(clkPropTopIdx);
        if (pPropTop == NULL)
        {
            status = FLCN_ERR_OBJECT_NOT_FOUND;
            PMU_BREAKPOINT();
            goto clkDomainsProgFreqPropagate_40_exit;
        }

        //
        // For each output clock domain, find the max frequency propagated from
        // all input clock domains frequency.
        //
        BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomainDst, dstIdx, &pFreqPropTuple->pDomainsMaskOut->super)
        {
            pFreqPropTuple->freqMHz[dstIdx] = LW_U16_MIN;

            BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomainSrc, srcIdx, &pFreqPropTuple->pDomainsMaskIn->super)
            {
                // Init to source frequency.
                freqMHz = pFreqPropTuple->freqMHz[srcIdx];

                // Propagate from src -> dst.
                PMU_ASSERT_OK_OR_GOTO(status,
                    clkPropTopFreqPropagate(pPropTop, pDomainSrc, pDomainDst, &freqMHz),
                    clkDomainsProgFreqPropagate_40_exit);

                // Capture max of all propagated frequencies.
                pFreqPropTuple->freqMHz[dstIdx] =
                    LW_MAX(pFreqPropTuple->freqMHz[dstIdx], freqMHz);
            }
            BOARDOBJGRP_ITERATOR_END;
        }
        BOARDOBJGRP_ITERATOR_END;
    }
    else
    {
        CLK_DOMAIN         *pDomain;
        CLK_DOMAIN_PROG    *pDomainProg;
        LwU32               vfRailMinsIntersect = LW_U32_MIN;
        LwBoardObjIdx       d;
        LwU8                voltRailIdx         =
            voltRailVoltDomainColwertToIdx(LW2080_CTRL_VOLT_VOLT_DOMAIN_LOGIC);

        LW2080_CTRL_CLK_VF_INPUT  vfInput;
        LW2080_CTRL_CLK_VF_OUTPUT vfOutput;

        if (voltRailIdx == LW2080_CTRL_VOLT_VOLT_RAIL_INDEX_ILWALID)
        {
            status = FLCN_ERR_ILWALID_INDEX;
            PMU_BREAKPOINT();
            goto clkDomainsProgFreqPropagate_40_exit;
        }

        //
        // Loop over all set CLK_DOMAINs and callwlate their Vmins and include
        // them in the Vmins for intersect.
        //
        BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, d,
            &pFreqPropTuple->pDomainsMaskIn->super)
        {
            pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
            if (pDomainProg == NULL)
            {
                status = FLCN_ERR_ILWALID_STATE;
                PMU_BREAKPOINT();
                goto clkDomainsProgFreqPropagate_40_exit;
            }

            // Pack VF input and output structs.
            LW2080_CTRL_CLK_VF_INPUT_INIT(&vfInput);
            LW2080_CTRL_CLK_VF_OUTPUT_INIT(&vfOutput);

            // Init
            vfInput.value = pFreqPropTuple->freqMHz[d];

            // Call into CLK_DOMAIN_PROG freqToVolt() interface.
            status = clkDomainProgFreqToVolt(
                        pDomainProg,
                        voltRailIdx,
                        LW2080_CTRL_CLK_VOLTAGE_TYPE_POR,
                        &vfInput,
                        &vfOutput,
                        NULL);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkDomainsProgFreqPropagate_40_exit;
            }

            //
            // Catch errors when no frequency >= input frequency.  Can't determine the
            // required voltage in this case.
            //
            if (LW2080_CTRL_CLK_VF_VALUE_ILWALID == vfOutput.inputBestMatch)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_OUT_OF_RANGE;
                goto clkDomainsProgFreqPropagate_40_exit;
            }

            // Include the Vmin in the intersect voltage callwlation.
            vfRailMinsIntersect =
                LW_MAX(vfRailMinsIntersect, vfOutput.value);
        }
        BOARDOBJGRP_ITERATOR_END;

        //
        // Loop over all unset strict CLK_DOMAINs and use voltage intersect to
        // determine their TUPLES' target frequencies.
        //
        BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, d,
            &pFreqPropTuple->pDomainsMaskOut->super)
        {
            pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
            if (pDomainProg == NULL)
            {
                status = FLCN_ERR_ILWALID_STATE;
                PMU_BREAKPOINT();
                goto clkDomainsProgFreqPropagate_40_exit;
            }

            // Pack VF input and output structs.
            LW2080_CTRL_CLK_VF_INPUT_INIT(&vfInput);
            LW2080_CTRL_CLK_VF_OUTPUT_INIT(&vfOutput);

            //
            // Init
            // bDefault = LW_FALSE - Voltage intersection to CLK_DOMAINs with sparse
            //                       VF points can lead to choosing frequencies which
            //                       would raise the PSTATE.
            //
            vfInput.value = vfRailMinsIntersect;

            // Call into CLK_DOMAIN_PROG voltToFreq() interface.
            status = clkDomainProgVoltToFreq(
                        pDomainProg,
                        voltRailIdx,
                        LW2080_CTRL_CLK_VOLTAGE_TYPE_POR,
                        &vfInput,
                        &vfOutput,
                        NULL);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkDomainsProgFreqPropagate_40_exit;
            }

            // Update the output
            if (LW2080_CTRL_CLK_VF_VALUE_ILWALID == vfOutput.inputBestMatch)
            {
                // This is not an error. Client (Arbiter) will set the default values.
                pFreqPropTuple->freqMHz[d] = LW2080_CTRL_CLK_VF_VALUE_ILWALID;
            }
            else
            {
                pFreqPropTuple->freqMHz[d] = vfOutput.value;
            }
        }
        BOARDOBJGRP_ITERATOR_END;
    }

clkDomainsProgFreqPropagate_40_exit:
    return status;
}

/*!
 * @copydoc ClkDomainProgFactoryDeltaAdj
 */
FLCN_STATUS
clkDomainProgFactoryDeltaAdj_40
(
    CLK_DOMAIN_PROG     *pDomainProg,
    LwU16               *pFreqMHz
)
{
    CLK_DOMAIN_40_PROG *pDomain40Prog =
        (CLK_DOMAIN_40_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    LwU16               cachedFreqMHz = *pFreqMHz;
    LwU8                vfRelIdx;
    LwU8                railIdx;
    FLCN_STATUS         status        = FLCN_OK;

    // Short-circuit if the OC/OV is disabled at clock domain level.
    if ((!clkDomain40ProgIsOCEnabled(pDomain40Prog)) ||
        (!clkDomain40ProgFactoryOCEnabled(pDomain40Prog)))
    {
        status = FLCN_OK;
        goto clkDomainProgFactoryDeltaAdj_40_exit;
    }

    // Iterate over each VOLTAGE_RAIL separately.
    FOR_EACH_INDEX_IN_MASK(32, railIdx, pDomain40Prog->railMask)
    {
        CLK_VF_REL *pVfRel;

        // Skip if clock domain does not have dependency on volt rail.
        if (clkDomain40ProgGetRailVfType(pDomain40Prog, railIdx) !=
            LW2080_CTRL_CLK_CLK_DOMAIN_40_PROG_RAIL_VF_TYPE_PRIMARY)
        {
            continue;
        }

        // Get VF rel index.
        vfRelIdx = s_clkDomain40ProgClkVfRelIdxGet(pDomain40Prog,
                                                   railIdx,
                                                  *pFreqMHz);
        if (vfRelIdx == LW2080_CTRL_CLK_CLK_VF_REL_IDX_ILWALID)
        {
            status = FLCN_ERR_ILWALID_INDEX;
            PMU_BREAKPOINT();
            goto clkDomainProgFactoryDeltaAdj_40_exit;
        }

        pVfRel = BOARDOBJGRP_OBJ_GET(CLK_VF_REL, vfRelIdx);
        if (pVfRel == NULL)
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto clkDomainProgFactoryDeltaAdj_40_exit;
        }

        // Short-circuit if the OC/OV is disabled at clock VF relationship level.
        if ((!clkVfRelOVOCEnabled(pVfRel)))
        {
            continue;
        }

        // Adjust the input frequency with factory OC offset.
        *pFreqMHz = clkFreqDeltaAdjust(*pFreqMHz,
                        clkDomain40ProgFinalFactoryDeltaGet(pDomain40Prog));

        // Quantize the frequency if required.
        if (*pFreqMHz != cachedFreqMHz)
        {
            LW2080_CTRL_CLK_VF_INPUT    inputFreq;
            LW2080_CTRL_CLK_VF_OUTPUT   outputFreq;

            // Init
            LW2080_CTRL_CLK_VF_INPUT_INIT(&inputFreq);
            LW2080_CTRL_CLK_VF_OUTPUT_INIT(&outputFreq);

            inputFreq.value = *pFreqMHz;
            status = clkDomainProgFreqQuantize_40(
                        pDomainProg,    // pDomainProg
                        &inputFreq,     // pInputFreq
                        &outputFreq,    // pOutputFreq
                        LW_TRUE,        // bFloor
                        LW_FALSE);      // bBound
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkDomainProgFactoryDeltaAdj_40_exit;
            }
            *pFreqMHz = (LwU16) outputFreq.value;
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

clkDomainProgFactoryDeltaAdj_40_exit:
    return status;
}


/*!
 * Accessor macro for final factory OC delta.
 * Final factory offset =
 *  MAX (AIC factory offset, (bGrdFreqOCEnabled ? GRD factory offset : 0))
 *
 * @param[in] pDomain40Prog   CLK_DOMAIN_40_PROG pointer
 *
 * @return pointer to final factory offset
 */
LW2080_CTRL_CLK_FREQ_DELTA *
clkDomain40ProgFinalFactoryDeltaGet
(
    CLK_DOMAIN_40_PROG *pDomain40Prog
)
{
    LW2080_CTRL_CLK_FREQ_DELTA  *pFactoryDelta = clkDomain40ProgFactoryDeltaGet(pDomain40Prog);
    LW2080_CTRL_CLK_FREQ_DELTA  *pGrdFreqDelta = clkDomain40ProgGrdFreqDeltaGet(pDomain40Prog);

    // If GRD offset is disable, return AIC factory offset.
    if (!clkDomainsGrdFreqOCEnabled())
    {
        return pFactoryDelta;
    }

    //
    // PP-TODO : We are assuming both GRD offset and AIC factory
    // offset are static offset. If this assumption breaks, we
    // will need to extend the support. For now, we will return
    // factory offset in this scenario.
    //
    if ((LW2080_CTRL_CLK_FREQ_DELTA_TYPE_GET(pFactoryDelta) !=
            LW2080_CTRL_CLK_CLK_FREQ_DELTA_TYPE_STATIC) ||
        (LW2080_CTRL_CLK_FREQ_DELTA_TYPE_GET(pGrdFreqDelta) !=
            LW2080_CTRL_CLK_CLK_FREQ_DELTA_TYPE_STATIC))
    {
        PMU_BREAKPOINT();
        return pFactoryDelta;
    }

    //
    // If GRD frequency offset is enabled and is greater than the
    // AIC factory offset, return GRD frequency offset as final
    // factory offset
    //
    if (LW2080_CTRL_CLK_FREQ_DELTA_GET_STATIC(pGrdFreqDelta) >
        LW2080_CTRL_CLK_FREQ_DELTA_GET_STATIC(pFactoryDelta))
    {
        return pGrdFreqDelta;
    }

    return pFactoryDelta;
}

/*!
 * Helper interface to get the CLK_DOMAIN_40_PROG pointer by the input
 * VF point and lwrve indexes.
 *
 * @param[in] vfPointIdx   VF point index.
 * @param[in] lwrveIdx     VF lwrve index.
 *
 * @return pointer to CLK_DOMAIN_40_PROG if matching entry found otherwise NULL.
 *
 */
CLK_DOMAIN_40_PROG *
clkDomain40ProgGetByVfPointIdx
(
    LwBoardObjIdx   vfPointIdx,
    LwU8            lwrveIdx
)
{
    CLK_DOMAIN   *pDomain = NULL;
    LwBoardObjIdx i;

    // iterate over all clock domains.
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, i,
        &(CLK_CLK_DOMAINS_GET()->progDomainsMask.super))
    {
        CLK_DOMAIN_40_PROG *pDomain40Prog = (CLK_DOMAIN_40_PROG *)pDomain;
        LwU8                railIdx;

        // Iterate over each VOLTAGE_RAIL separately.
        FOR_EACH_INDEX_IN_MASK(32, railIdx, pDomain40Prog->railMask)
        {
            CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY  *pVfPrimary = NULL;
            LwU8                                vfRelIdx;

            // Only primary has their own VF points.
            if (clkDomain40ProgGetRailVfType(pDomain40Prog, railIdx) !=
                LW2080_CTRL_CLK_CLK_DOMAIN_40_PROG_RAIL_VF_TYPE_PRIMARY)
            {
                continue;
            }

            pVfPrimary = &(pDomain40Prog->railVfItem[railIdx].data.primary);

            // Iterate over this VF_PRIMARY's CLK_VF_REL objects
            for (vfRelIdx =  pVfPrimary->clkVfRelIdxFirst;
                 vfRelIdx <= pVfPrimary->clkVfRelIdxLast;
                 vfRelIdx++)
            {
                CLK_VF_REL *pVfRel;

                pVfRel = BOARDOBJGRP_OBJ_GET(CLK_VF_REL, vfRelIdx);
                if (pVfRel == NULL)
                {
                    PMU_BREAKPOINT();
                    goto clkDomain40ProgGetByVfPointIdx_exit;
                }

                if ((vfPointIdx >= clkVfRelVfPointIdxFirstGet(pVfRel, lwrveIdx)) &&
                    (vfPointIdx <= clkVfRelVfPointIdxLastGet(pVfRel,  lwrveIdx)))
                {
                    return pDomain40Prog;
                }
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }
    BOARDOBJGRP_ITERATOR_END;

clkDomain40ProgGetByVfPointIdx_exit:
    return NULL;
}

/*!
 * @copydoc ClkDomainProgIsSecVFLwrvesEnabled
 */
FLCN_STATUS
clkDomainProgIsSecVFLwrvesEnabled_40
(
    CLK_DOMAIN_PROG                    *pDomainProg,
    LwBool                             *pBIsSecVFLwrvesEnabled
)
{
    CLK_DOMAIN_40_PROG *pDomain40Prog   =
        (CLK_DOMAIN_40_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);

    *pBIsSecVFLwrvesEnabled =
        (clkDomain40ProgGetClkDomailwfLwrveCount(pDomain40Prog) > 1) ?
        LW_TRUE :
        LW_FALSE;

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * Helper interface to cache the offset adjusted clock enumeration
 * ranges for a given input programmable clock domain.
 */
static FLCN_STATUS
s_clkDomain40ProgClkEnumCache
(
    CLK_DOMAIN_40_PROG *pDomain40Prog
)
{
    LwU8        enumIdx;
    FLCN_STATUS status = FLCN_OK;

    for (enumIdx  = pDomain40Prog->clkEnumIdxFirst;
         enumIdx <= pDomain40Prog->clkEnumIdxLast;
         enumIdx++)
    {
        CLK_ENUM *pEnum = (CLK_ENUM *)BOARDOBJGRP_OBJ_GET(CLK_ENUM, enumIdx);

        if (pEnum == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto s_clkDomain40ProgClkEnumCache_exit;
        }

        status = clkEnumFreqRangeAdjust(pEnum, pDomain40Prog);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_clkDomain40ProgClkEnumCache_exit;
        }
    }

s_clkDomain40ProgClkEnumCache_exit:
    return status;
}

/*!
 * @copydoc ClkDomain40ProgVoltAdjustDeltauV
 */
static FLCN_STATUS
s_clkDomain40ProgVoltAdjustDeltauV_PRIMARY
(
    CLK_DOMAIN_40_PROG     *pDomain40Prog,
    CLK_VF_REL             *pVfRel,
    LwU32                  *pVoltuV
)
{
    // Apply the global voltage offset.
    *pVoltuV = clkVoltDeltaAdjust(*pVoltuV,
                Clk.domains.deltas.pVoltDeltauV[pVfRel->railIdx]);

    // Apply this CLK_DOMAIN_40_PROG object's voltage offset.
    *pVoltuV = clkVoltDeltaAdjust(*pVoltuV,
                clkDomain40ProgVoltDeltauVGet(pDomain40Prog, pVfRel->railIdx));

    // Apply the VF_REL object's voltage offset.
    *pVoltuV = clkVoltDeltaAdjust(*pVoltuV,
                clkVfRelVoltDeltauVGet(pVfRel));

    return FLCN_OK;
}

/*!
 * @copydoc ClkDomain40ProgVoltAdjustDeltauV
 */
static FLCN_STATUS
s_clkDomain40ProgVoltAdjustDeltauV_SECONDARY
(
    CLK_DOMAIN_40_PROG     *pDomain40Prog,
    CLK_VF_REL             *pVfRel,
    LwU32                  *pVoltuV
)
{
    // Apply this CLK_DOMAIN_40_PROG object's voltage offset.
    *pVoltuV = clkVoltDeltaAdjust(*pVoltuV,
                clkDomain40ProgVoltDeltauVGet(pDomain40Prog, pVfRel->railIdx));

    return FLCN_OK;
}

/*!
 * @copydoc ClkDomain40ProgFreqAdjustDeltaMHz
 */
static FLCN_STATUS
s_clkDomain40ProgFreqAdjustDeltaMHz_PRIMARY
(
    CLK_DOMAIN_40_PROG     *pDomain40Prog,
    CLK_VF_REL             *pVfRel,
    LwU16                  *pFreqMHz
)
{
    // Offset by the global CLK_DOMAINS offset.
    *pFreqMHz = clkFreqDeltaAdjust(*pFreqMHz,
                    clkDomainsFreqDeltaGet());

    // Offset by the CLK_DOMAIN_40_PROG factory OC offset.
    *pFreqMHz = clkFreqDeltaAdjust(*pFreqMHz,
                    clkDomain40ProgFinalFactoryDeltaGet(pDomain40Prog));

    // Offset by the CLK_DOMAIN_40_PROG domain offset.
    *pFreqMHz = clkFreqDeltaAdjust(*pFreqMHz,
                    clkDomain3XProgFreqDeltaGet(pDomain40Prog));

   // Offset by the CLK_VF_REL offset
    *pFreqMHz = clkFreqDeltaAdjust(*pFreqMHz,
                    clkVfRelFreqDeltaGet(pVfRel));

    return FLCN_OK;
}

/*!
 * @copydoc ClkDomain40ProgFreqAdjustDeltaMHz
 */
static FLCN_STATUS
s_clkDomain40ProgFreqAdjustDeltaMHz_SECONDARY
(
    CLK_DOMAIN_40_PROG     *pDomain40Prog,
    CLK_VF_REL             *pVfRel,
    LwU16                  *pFreqMHz
)
{
    FLCN_STATUS status = FLCN_OK;

    // Offset by the CLK_DOMAIN_40_PROG factory OC offset.
    *pFreqMHz = clkFreqDeltaAdjust(*pFreqMHz,
                    clkDomain40ProgFinalFactoryDeltaGet(pDomain40Prog));

    // Offset by the CLK_DOMAIN_40_PROG domain offset.
    *pFreqMHz = clkFreqDeltaAdjust(*pFreqMHz,
                    clkDomain3XProgFreqDeltaGet(pDomain40Prog));

    //
    // In case of table relationship, Primary -> Secondary colwersion does not
    // scale frequency offset adjuments that were done on primary input
    // frequency to secondary output frequency. So we must explicitely adjust
    // secondary frequency with primary frequency offset adjustments. Here, we
    // are going to apply exact same offset of primary on its secondary domains.
    //
    if (BOARDOBJ_GET_TYPE(pVfRel) == LW2080_CTRL_CLK_CLK_VF_REL_TYPE_TABLE_FREQ)
    {
        LwU8 primaryIdx = pDomain40Prog->railVfItem[pVfRel->railIdx].data.secondary.primaryIdx;
        CLK_DOMAIN_40_PROG *pDomainProg =
            CLK_CLK_DOMAIN_40_PROG_GET_BY_IDX(primaryIdx);

        if (pDomainProg == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto s_clkDomain40ProgFreqAdjustDeltaMHz_SECONDARY_exit;
        }

        // Apply this VF Primary's voltage offset.
        status = s_clkDomain40ProgFreqAdjustDeltaMHz_PRIMARY(
                        pDomainProg,
                        pVfRel,
                        pFreqMHz);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_clkDomain40ProgFreqAdjustDeltaMHz_SECONDARY_exit;
        }
    }

s_clkDomain40ProgFreqAdjustDeltaMHz_SECONDARY_exit:
    return status;
}

/*!
 * @copydoc ClkDomain40ProgOffsetVFVoltAdjustDeltauV
 */
static FLCN_STATUS
s_clkDomain40ProgOffsetVFVoltAdjustDeltauV_PRIMARY
(
    CLK_DOMAIN_40_PROG     *pDomain40Prog,
    CLK_VF_REL             *pVfRel,
    LwS32                  *pVoltuV
)
{
    // Apply the global voltage offset.
    *pVoltuV = clkOffsetVFVoltDeltaAdjust(*pVoltuV,
                Clk.domains.deltas.pVoltDeltauV[pVfRel->railIdx]);

    // Apply this CLK_DOMAIN_40_PROG object's voltage offset.
    *pVoltuV = clkOffsetVFVoltDeltaAdjust(*pVoltuV,
                clkDomain40ProgVoltDeltauVGet(pDomain40Prog, pVfRel->railIdx));

    // Apply the VF_REL object's voltage offset.
    *pVoltuV = clkOffsetVFVoltDeltaAdjust(*pVoltuV,
                clkVfRelVoltDeltauVGet(pVfRel));

    return FLCN_OK;
}

/*!
 * @copydoc ClkDomain40ProgOffsetVFVoltAdjustDeltauV
 */
static FLCN_STATUS
s_clkDomain40ProgOffsetVFVoltAdjustDeltauV_SECONDARY
(
    CLK_DOMAIN_40_PROG     *pDomain40Prog,
    CLK_VF_REL             *pVfRel,
    LwS32                  *pVoltuV
)
{
    // Apply this CLK_DOMAIN_40_PROG object's voltage offset.
    *pVoltuV = clkOffsetVFVoltDeltaAdjust(*pVoltuV,
                clkDomain40ProgVoltDeltauVGet(pDomain40Prog, pVfRel->railIdx));

    return FLCN_OK;
}

/*!
 * @copydoc ClkDomain40ProgOffsetVFFreqAdjustDeltaMHz
 */
static FLCN_STATUS
s_clkDomain40ProgOffsetVFFreqAdjustDeltaMHz_PRIMARY
(
    CLK_DOMAIN_40_PROG     *pDomain40Prog,
    CLK_VF_REL             *pVfRel,
    LwS16                  *pFreqMHz
)
{
    // Offset by the global CLK_DOMAINS offset.
    *pFreqMHz = clkOffsetVFFreqDeltaAdjust(*pFreqMHz,
                    clkDomainsFreqDeltaGet());

    // Offset by the CLK_DOMAIN_40_PROG factory OC offset.
    *pFreqMHz = clkOffsetVFFreqDeltaAdjust(*pFreqMHz,
                    clkDomain40ProgFinalFactoryDeltaGet(pDomain40Prog));

    // Offset by the CLK_DOMAIN_40_PROG domain offset.
    *pFreqMHz = clkOffsetVFFreqDeltaAdjust(*pFreqMHz,
                    clkDomain3XProgFreqDeltaGet(pDomain40Prog));

   // Offset by the CLK_VF_REL offset
    *pFreqMHz = clkOffsetVFFreqDeltaAdjust(*pFreqMHz,
                    clkVfRelFreqDeltaGet(pVfRel));

    return FLCN_OK;
}

/*!
 * @copydoc ClkDomain40ProgOffsetVFFreqAdjustDeltaMHz
 */
static FLCN_STATUS
s_clkDomain40ProgOffsetVFFreqAdjustDeltaMHz_SECONDARY
(
    CLK_DOMAIN_40_PROG     *pDomain40Prog,
    CLK_VF_REL             *pVfRel,
    LwS16                  *pFreqMHz
)
{
    FLCN_STATUS status = FLCN_OK;

    // Offset by the CLK_DOMAIN_40_PROG factory OC offset.
    *pFreqMHz = clkOffsetVFFreqDeltaAdjust(*pFreqMHz,
                    clkDomain40ProgFinalFactoryDeltaGet(pDomain40Prog));

    // Offset by the CLK_DOMAIN_40_PROG domain offset.
    *pFreqMHz = clkOffsetVFFreqDeltaAdjust(*pFreqMHz,
                    clkDomain3XProgFreqDeltaGet(pDomain40Prog));

    //
    // In case of table relationship, Primary -> Secondary colwersion does not
    // scale frequency offset adjuments that were done on primary input
    // frequency to secondary output frequency. So we must explicitely adjust
    // secondary frequency with primary frequency offset adjustments. Here, we
    // are going to apply exact same offset of primary on its secondary domains.
    //
    if (BOARDOBJ_GET_TYPE(pVfRel) == LW2080_CTRL_CLK_CLK_VF_REL_TYPE_TABLE_FREQ)
    {
        LwU8 primaryIdx = pDomain40Prog->railVfItem[pVfRel->railIdx].data.secondary.primaryIdx;
        CLK_DOMAIN_40_PROG *pDomainProg =
            CLK_CLK_DOMAIN_40_PROG_GET_BY_IDX(primaryIdx);

        if (pDomainProg == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto s_clkDomain40ProgOffsetVFFreqAdjustDeltaMHz_SECONDARY_exit;
        }

        // Apply this VF Primary's voltage offset.
        status = s_clkDomain40ProgOffsetVFFreqAdjustDeltaMHz_PRIMARY(
                        pDomainProg,
                        pVfRel,
                        pFreqMHz);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_clkDomain40ProgOffsetVFFreqAdjustDeltaMHz_SECONDARY_exit;
        }
    }

s_clkDomain40ProgOffsetVFFreqAdjustDeltaMHz_SECONDARY_exit:
    return status;
}

/*!
 * Helper interface to get the VF_REL index for the given input frequency
 * and voltage rail.
 */
static LwU8
s_clkDomain40ProgClkVfRelIdxGet
(
    CLK_DOMAIN_40_PROG     *pDomain40Prog,
    LwU8                    railIdx,
    LwU16                   freqMHz
)
{
    CLK_DOMAIN_40_PROG *pDomain40ProgPrimary = pDomain40Prog;
    CLK_DOMAIN_40_PROG_RAIL_VF_PRIMARY
                       *pVfPrimary           = NULL;
    LwU8                vfRelIdx;
    LwBool              bValidMatch         = LW_FALSE;

    // If secondary, get corresponding primary.
    if (clkDomain40ProgGetRailVfType(pDomain40Prog, railIdx) ==
        LW2080_CTRL_CLK_CLK_DOMAIN_40_PROG_RAIL_VF_TYPE_SECONDARY)
    {
        LwU8 primaryIdx = pDomain40Prog->railVfItem[railIdx].data.secondary.primaryIdx;

        pDomain40ProgPrimary = CLK_CLK_DOMAIN_40_PROG_GET_BY_IDX(primaryIdx);
    }

    if (pDomain40ProgPrimary == NULL)
    {
        PMU_BREAKPOINT();
        vfRelIdx = 0;
        goto s_clkDomain40ProgClkVfRelIdxGet_exit;
    }

    pVfPrimary = &(pDomain40ProgPrimary->railVfItem[railIdx].data.primary);

    // Iterate over this VF_PRIMARY's CLK_VF_REL objects
    for (vfRelIdx =  pVfPrimary->clkVfRelIdxFirst;
         vfRelIdx <= pVfPrimary->clkVfRelIdxLast;
         vfRelIdx++)
    {
        CLK_VF_REL *pVfRel;

        pVfRel = BOARDOBJGRP_OBJ_GET(CLK_VF_REL, vfRelIdx);
        if (pVfRel == NULL)
        {
            PMU_BREAKPOINT();
            vfRelIdx = 0;
            goto s_clkDomain40ProgClkVfRelIdxGet_exit;
        }

        bValidMatch = clkVfRelGetIdxFromFreq(pVfRel, pDomain40Prog, freqMHz);

        // Break if valid match found.
        if (bValidMatch)
        {
            break;
        }
    }

    // Select last index as valid index if no match found.
    if (!bValidMatch)
    {
        vfRelIdx = pVfPrimary->clkVfRelIdxLast;
    }

s_clkDomain40ProgClkVfRelIdxGet_exit:
    return vfRelIdx;
}

/*!
 * @copydoc BoardObjVirtualTableGetInterfaceFromBoardObj
 */
static BOARDOBJ_INTERFACE *
s_clkDomainGetInterfaceFromBoardObj_40_PROG
(
    BOARDOBJ   *pBoardObj,
    LwU8        interfaceType
)
{
    CLK_DOMAIN_40_PROG *pDomain40Prog = (CLK_DOMAIN_40_PROG *)pBoardObj;
    BOARDOBJ_INTERFACE *pInterface    = NULL;

    switch (interfaceType)
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_INTERFACE_TYPE_PROG:
        {
            pInterface = &pDomain40Prog->prog.super;
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
