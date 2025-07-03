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


/* ------------------------ Static Function Prototypes --------------------- */
static BoardObjVirtualTableGetInterfaceFromBoardObj (s_clkDomainGetInterfaceFromBoardObj_30_Primary)
    GCC_ATTRIB_SECTION("imem_perfVf", "s_clkDomainGetInterfaceFromBoardObj_30_Primary");

/* ------------------------ Global Variables ------------------------------- */
/*!
 * Main structure for all CLK_DOMAIN_30_PRIMARY_VIRTUAL_TABLE data.
 */
BOARDOBJ_VIRTUAL_TABLE ClkDomain30PrimaryVirtualTable =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_GET_INTERFACE_FROM_BOARDOBJ(
        s_clkDomainGetInterfaceFromBoardObj_30_Primary)
};

/*!
 * Main structure for all CLK_DOMAIN_3X_PRIMARY_VIRTUAL_TABLE data.
 */
INTERFACE_VIRTUAL_TABLE ClkDomain3XPrimaryVirtualTable =
{
    LW_OFFSETOF(CLK_DOMAIN_30_PRIMARY, primary)   // offset
};

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_DOMAIN_30_PRIMARY class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkDomainGrpIfaceModel10ObjSet_30_PRIMARY
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_CLK_CLK_DOMAIN_30_PRIMARY_BOARDOBJ_SET *pSet =
        (RM_PMU_CLK_CLK_DOMAIN_30_PRIMARY_BOARDOBJ_SET *)pBoardObjSet;
    CLK_DOMAIN_30_PRIMARY   *pDomain30Primary;
    BOARDOBJ_VTABLE        *pBoardObjVtable;
    FLCN_STATUS             status;

    // Construct 30_Prog super class
    status = clkDomainGrpIfaceModel10ObjSet_30_PROG(pModel10, ppBoardObj, size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainGrpIfaceModel10ObjSet_30_PRIMARY_exit;
    }
    pBoardObjVtable = (BOARDOBJ_VTABLE *)*ppBoardObj;
    pDomain30Primary = (CLK_DOMAIN_30_PRIMARY *)*ppBoardObj;

    // Construct 3X_Primary super class
    status = clkDomainConstruct_3X_PRIMARY(pBObjGrp,
                &pDomain30Primary->primary.super,
                &pSet->primary.super,
                &ClkDomain3XPrimaryVirtualTable);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainGrpIfaceModel10ObjSet_30_PRIMARY_exit;
    }

    // Override the Vtable pointer.
    pBoardObjVtable->pVirtualTable = &ClkDomain30PrimaryVirtualTable;

clkDomainGrpIfaceModel10ObjSet_30_PRIMARY_exit:
    return status;
}


/*!
 * @copydoc ClkDomainCache
 */
FLCN_STATUS
clkDomainCache_30_Primary
(
    CLK_DOMAIN *pDomain,
    LwBool      bVFEEvalRequired
)
{
    CLK_DOMAIN_30_PRIMARY   *pDomain30Primary = (CLK_DOMAIN_30_PRIMARY *)pDomain;
    CLK_PROG_30_PRIMARY     *pProg30Primary;
    FLCN_STATUS             status = FLCN_OK;
    LwU8                    voltRailIdx;
    LwU8                    i;
    LwS16                   j;

    // On VF 3.0, we always evaluate VFE.
    PMU_HALT_COND(bVFEEvalRequired);

    // Iterate over each VOLTAGE_RAIL separately.
    for (voltRailIdx = 0; voltRailIdx < Clk.domains.voltRailsMax; voltRailIdx++)
    {
        //
        // Initialize the last pair to (0,0) as there is no initial VF pair
        // floor for the first VF_POINT.
        //
        LW2080_CTRL_CLK_VF_PAIR vFPairLast = LW2080_CTRL_CLK_VF_PAIR_INIT();

        //
        // Initialize the last base VF pair to (0,0) as there is no initial
        // base VF pair floor for the first VF_POINT.
        //
        LW2080_CTRL_CLK_VF_PAIR baseVFPairLast = LW2080_CTRL_CLK_VF_PAIR_INIT();

        //
        // Iterate over this CLK_DOMAIN_30_PRIMARY's CLK_PROG_30_PRIMARY objects
        // and cache each of them in order, passing the vFPairLast structure to
        // ensure that the CLK_VF_POINTs are monotonically increasing.
        //
        for (i = pDomain30Primary->super.super.clkProgIdxFirst;
                i <= pDomain30Primary->super.super.clkProgIdxLast;
                i++)
        {
            pProg30Primary = (CLK_PROG_30_PRIMARY *)BOARDOBJGRP_OBJ_GET(CLK_PROG, i);
            if (pProg30Primary == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_INDEX;
                goto clkDomainCache_30_Primary_exit;
            }

            status = clkProg30PrimaryCache(pProg30Primary,
                                          pDomain30Primary,
                                          voltRailIdx,
                                          &vFPairLast,
                                          &baseVFPairLast);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkDomainCache_30_Primary_exit;
            }
        }

        // Smooth Primary's VF lwrve based on POR max allowed step size.
        if (clkDomainsVfSmootheningEnforced())
        {
            //
            // Iterate over this CLK_DOMAIN_30_PRIMARY's CLK_PROG_30_PRIMARY objects
            // and smoothen each of them in order.
            //
            for (j = pDomain30Primary->super.super.clkProgIdxLast;
                    j >= pDomain30Primary->super.super.clkProgIdxFirst;
                    j--)
            {
                pProg30Primary = (CLK_PROG_30_PRIMARY *)BOARDOBJGRP_OBJ_GET(CLK_PROG, j);
                if (pProg30Primary == NULL)
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_INDEX;
                    goto clkDomainCache_30_Primary_exit;
                }

                status = clkProg30PrimarySmoothing(pProg30Primary,
                                                  pDomain30Primary,
                                                  voltRailIdx);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto clkDomainCache_30_Primary_exit;
                }
            }
        }
    }

clkDomainCache_30_Primary_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @copydoc BoardObjVirtualTableGetInterfaceFromBoardObj
 */
static BOARDOBJ_INTERFACE *
s_clkDomainGetInterfaceFromBoardObj_30_Primary
(
    BOARDOBJ   *pBoardObj,
    LwU8        interfaceType
)
{
    CLK_DOMAIN_30_PRIMARY *pDomain30Primary = (CLK_DOMAIN_30_PRIMARY *)pBoardObj;

    switch (interfaceType)
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_INTERFACE_TYPE_3X_PRIMARY:
        {
            return &pDomain30Primary->primary.super;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_INTERFACE_TYPE_PROG:
        {
            return &pDomain30Primary->super.super.prog.super;
        }
    }

    PMU_BREAKPOINT();
    return NULL;
}
