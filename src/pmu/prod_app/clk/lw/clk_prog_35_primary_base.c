/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file clk_prog_35_primary.c
 *
 * @brief Module managing all state related to the CLK_PROG_35_PRIMARY structure.
 * This structure defines how to program a given frequency on given PRIMARY
 * CLK_DOMAIN - a CLK_DOMAIN which has its own specified VF lwrve.
 *
 */
//!  https://wiki.lwpu.com/engwiki/index.php/Resman/PState/Data_Tables/Clock_Programming_Table/1.0_Spec#Primary

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "volt/objvolt.h"
#include "dmemovl.h"

/* ------------------------ Global Variables ------------------------------- */
/*!
 * Main structure for all CLK_PROG_3X_PRIMARY_VIRTUAL_TABLE data.
 */
INTERFACE_VIRTUAL_TABLE ClkProg3XPrimaryVirtualTable_35 =
{
    LW_OFFSETOF(CLK_PROG_35_PRIMARY, primary)   // offset
};

/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_PROG_35_PRIMARY class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkProgGrpIfaceModel10ObjSet_35_PRIMARY
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_CLK_CLK_PROG_35_PRIMARY_BOARDOBJ_SET *p35PrimarySet =
        (RM_PMU_CLK_CLK_PROG_35_PRIMARY_BOARDOBJ_SET *)pBoardObjDesc;
    CLK_PROG_35_PRIMARY *p35Primary;
    FLCN_STATUS         status;

    status = clkProgGrpIfaceModel10ObjSet_35(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkProgGrpIfaceModel10ObjSet_35_PRIMARY_exit;
    }
    p35Primary = (CLK_PROG_35_PRIMARY *)*ppBoardObj;

    status = clkProgConstruct_3X_PRIMARY(pBObjGrp,
                &p35Primary->primary.super,
                &p35PrimarySet->primary.super,
                &ClkProg3XPrimaryVirtualTable_35);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkProgGrpIfaceModel10ObjSet_35_PRIMARY_exit;
    }
    // No need to update the Boardobj Vtable pointer, leaf class will do it.

    // Allocate memory for secondary VF lwrve date if supported.
    if (Clk.progs.vfSecEntryCount != 0U)
    {
        // Allocate the VF entries array if not previously allocated.
        if (p35Primary->pVoltRailSecVfEntries == NULL)
        {
            p35Primary->pVoltRailSecVfEntries =
                lwosCallocType(pBObjGrp->ovlIdxDmem, Clk.progs.vfEntryCount,
                               LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_SEC_VF_ENTRY_VOLTRAIL);
            if (p35Primary->pVoltRailSecVfEntries == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_NO_FREE_MEM;
                goto clkProgGrpIfaceModel10ObjSet_35_PRIMARY_exit;
            }
        }

        memcpy(p35Primary->pVoltRailSecVfEntries, p35PrimarySet->voltRailSecVfEntries,
            sizeof(LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_SEC_VF_ENTRY_VOLTRAIL) *
                                Clk.progs.vfEntryCount);
    }

clkProgGrpIfaceModel10ObjSet_35_PRIMARY_exit:
    return status;
}

/*!
 * @copydoc clkProg35PrimaryLoad
 */
FLCN_STATUS
clkProg35PrimaryLoad
(
    CLK_PROG_35_PRIMARY     *pProg35Primary,
    CLK_DOMAIN_35_PRIMARY   *pDomain35Primary,
    LwU8                    voltRailIdx,
    LwU8                    lwrveIdx
)
{
    CLK_DOMAIN_3X_PRIMARY *pDomain3XPrimary =
        CLK_CLK_DOMAIN_3X_PRIMARY_GET_FROM_35_PRIMARY(pDomain35Primary);
    CLK_VF_POINT       *pVfPoint;
    FLCN_STATUS         status = FLCN_OK;
    LwBoardObjIdx       i;

    // Sanity check.
    if ((LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_ILWALID == lwrveIdx) ||
        (LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID ==
            clklkProg35PrimaryVfPointIdxFirstGet(pProg35Primary, voltRailIdx, lwrveIdx)) ||
        (LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID ==
            clklkProg35PrimaryVfPointIdxLastGet(pProg35Primary, voltRailIdx, lwrveIdx)))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkProg35PrimaryLoad_exit;
    }

    //
    // Iterate over this CLK_PROG_35_PRIMARY's CLK_VF_POINTs and load each of
    // them in order.
    //

    // Load primary VF lwrve
    for (i = clklkProg35PrimaryVfPointIdxFirstGet(pProg35Primary, voltRailIdx, lwrveIdx);
            i <= clklkProg35PrimaryVfPointIdxLastGet(pProg35Primary, voltRailIdx, lwrveIdx);
            i++)
    {
        pVfPoint = (CLK_VF_POINT *)CLK_VF_POINT_GET_BY_LWRVE_ID(lwrveIdx, i);
        if (pVfPoint == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkProg35PrimaryLoad_exit;
        }

        status = clkVfPointLoad(pVfPoint,
                                &pProg35Primary->primary,
                                pDomain3XPrimary,
                                voltRailIdx,
                                lwrveIdx);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkProg35PrimaryLoad_exit;
        }
    }

clkProg35PrimaryLoad_exit:
    return status;
}

/*!
 * @copydoc clkProg35PrimaryCache
 */
FLCN_STATUS
clkProg35PrimaryCache
(
    CLK_PROG_35_PRIMARY         *pProg35Primary,
    CLK_DOMAIN_35_PRIMARY       *pDomain35Primary,
    LwU8                        voltRailIdx,
    LwBool                      bVFEEvalRequired,
    LwU8                        lwrveIdx,
    CLK_VF_POINT_35            *pVfPoint35Last
)
{
    CLK_VF_POINT_35    *pVfPoint35;
    FLCN_STATUS         status = FLCN_OK;
    LwBoardObjIdx       i;

    // Sanity check.
    if ((LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_ILWALID == lwrveIdx) ||
        (LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID ==
            clklkProg35PrimaryVfPointIdxFirstGet(pProg35Primary, voltRailIdx, lwrveIdx)) ||
        (LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID ==
            clklkProg35PrimaryVfPointIdxLastGet(pProg35Primary, voltRailIdx, lwrveIdx)))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkProg35PrimaryCache_exit;
    }

    //
    // Iterate over this CLK_PROG_35_PRIMARY's CLK_VF_POINTs and cache each of
    // them in order, passing the vFPairLast structure to ensure that the
    // CLK_VF_POINTs are monotonically increasing.
    //
    for (i = clklkProg35PrimaryVfPointIdxFirstGet(pProg35Primary, voltRailIdx, lwrveIdx);
            i <= clklkProg35PrimaryVfPointIdxLastGet(pProg35Primary, voltRailIdx, lwrveIdx);
            i++)
    {
        pVfPoint35 = (CLK_VF_POINT_35 *)CLK_VF_POINT_GET_BY_LWRVE_ID(lwrveIdx, i);
        if (pVfPoint35 == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkProg35PrimaryCache_exit;
        }

        status = clkVfPoint35Cache(pVfPoint35,
                                   pVfPoint35Last,
                                   pDomain35Primary,
                                   pProg35Primary,
                                   voltRailIdx,
                                   lwrveIdx,
                                   bVFEEvalRequired);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkProg35PrimaryCache_exit;
        }

        pVfPoint35Last = pVfPoint35;
    }

clkProg35PrimaryCache_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
