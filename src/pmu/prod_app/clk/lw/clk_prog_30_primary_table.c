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
 * @file clk_prog_30_primary_table.c
 *
 * @brief Module managing all state related to the CLK_PROG_30_PRIMARY_TABLE structure.
 * This structure defines how to program a given frequency on given PRIMARY
 * CLK_DOMAIN - a CLK_DOMAIN which has its own specified VF lwrve.
 *
 * The PRIMARY_TABLE class specifies SECONDARY Clock Domains' VF lwrve as table of
 * the PRIMARY Clock Domain.
 *
 */
//! https://wiki.lwpu.com/engwiki/index.php/Resman/PState/Data_Tables/Clock_Programming_Table/1.0_Spec#Table

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "dmemovl.h"

/* ------------------------ Static Function Prototypes --------------------- */
static BoardObjVirtualTableGetInterfaceFromBoardObj (s_clkProgGetInterfaceFromBoardObj_30_PRIMARY_TABLE)
    GCC_ATTRIB_SECTION("imem_perfVf", "s_clkProgGetInterfaceFromBoardObj_30_PRIMARY_TABLE");

/* ------------------------ Global Variables ------------------------------- */
/*!
 * Main structure for all CLK_PROG_30_PRIMARY_TABLE_VIRTUAL_TABLE data.
 */
BOARDOBJ_VIRTUAL_TABLE ClkProg30PrimaryTableVirtualTable =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_GET_INTERFACE_FROM_BOARDOBJ(
        s_clkProgGetInterfaceFromBoardObj_30_PRIMARY_TABLE)
};

/*!
 * Main structure for all CLK_PROG_3X_PRIMARY_TABLE_VIRTUAL_TABLE data.
 */
INTERFACE_VIRTUAL_TABLE ClkProg3XPrimaryTableVirtualTable_30 =
{
    LW_OFFSETOF(CLK_PROG_30_PRIMARY_TABLE, table)   // offset
};

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_PROG_30_PRIMARY_TABLE class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkProgGrpIfaceModel10ObjSet_30_PRIMARY_TABLE
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_CLK_CLK_PROG_30_PRIMARY_TABLE_BOARDOBJ_SET *p30PrimaryTableSet =
        (RM_PMU_CLK_CLK_PROG_30_PRIMARY_TABLE_BOARDOBJ_SET *)pBoardObjSet;
    CLK_PROG_30_PRIMARY_TABLE   *p30PrimaryTable;
    BOARDOBJ_VTABLE            *pBoardObjVtable;
    FLCN_STATUS                 status;

    status = clkProgGrpIfaceModel10ObjSet_30_PRIMARY(pModel10, ppBoardObj, size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkProgGrpIfaceModel10ObjSet_30_PRIMARY_TABLE_exit;
    }
    pBoardObjVtable = (BOARDOBJ_VTABLE *)*ppBoardObj;
    p30PrimaryTable  = (CLK_PROG_30_PRIMARY_TABLE *)*ppBoardObj;

    status = clkProgConstruct_3X_PRIMARY_TABLE(pBObjGrp,
                &p30PrimaryTable->table.super,
                &p30PrimaryTableSet->table.super,
                &ClkProg3XPrimaryTableVirtualTable_30);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkProgGrpIfaceModel10ObjSet_30_PRIMARY_TABLE_exit;
    }

    // Override the Vtable pointer.
    pBoardObjVtable->pVirtualTable = &ClkProg30PrimaryTableVirtualTable;

clkProgGrpIfaceModel10ObjSet_30_PRIMARY_TABLE_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @copydoc BoardObjVirtualTableGetInterfaceFromBoardObj
 */
static BOARDOBJ_INTERFACE *
s_clkProgGetInterfaceFromBoardObj_30_PRIMARY_TABLE
(
    BOARDOBJ   *pBoardObj,
    LwU8        interfaceType
)
{
    CLK_PROG_30_PRIMARY_TABLE *p30PrimaryTable = (CLK_PROG_30_PRIMARY_TABLE *)pBoardObj;

    switch (interfaceType)
    {
        case LW2080_CTRL_CLK_CLK_PROG_INTERFACE_TYPE_3X_PRIMARY_TABLE:
        {
            return &p30PrimaryTable->table.super;
        }
        case LW2080_CTRL_CLK_CLK_PROG_INTERFACE_TYPE_3X_PRIMARY:
        {
            return &p30PrimaryTable->super.primary.super;
        }
    }

    PMU_BREAKPOINT();
    return NULL;
}
