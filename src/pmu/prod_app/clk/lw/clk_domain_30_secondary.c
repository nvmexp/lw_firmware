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
static BoardObjVirtualTableGetInterfaceFromBoardObj (s_clkDomainGetInterfaceFromBoardObj_30_Secondary)
    GCC_ATTRIB_SECTION("imem_perfVf", "s_clkDomainGetInterfaceFromBoardObj_30_Secondary");

/* ------------------------ Global Variables ------------------------------- */
/*!
 * Main structure for all CLK_DOMAIN_30_SECONDARY_VIRTUAL_TABLE data.
 */
BOARDOBJ_VIRTUAL_TABLE ClkDomain30SecondaryVirtualTable =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_GET_INTERFACE_FROM_BOARDOBJ(
        s_clkDomainGetInterfaceFromBoardObj_30_Secondary)
};

/*!
 * Main structure for all CLK_DOMAIN_3X_SECONDARY_VIRTUAL_TABLE data.
 */
INTERFACE_VIRTUAL_TABLE ClkDomain3XSecondaryVirtualTable =
{
    LW_OFFSETOF(CLK_DOMAIN_30_SECONDARY, secondary)   // offset
};

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_DOMAIN_30_SECONDARY class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkDomainGrpIfaceModel10ObjSet_30_SECONDARY
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_CLK_CLK_DOMAIN_30_SECONDARY_BOARDOBJ_SET *pSet =
        (RM_PMU_CLK_CLK_DOMAIN_30_SECONDARY_BOARDOBJ_SET *)pBoardObjSet;
    CLK_DOMAIN_30_SECONDARY    *pDomain;
    BOARDOBJ_VTABLE        *pBoardObjVtable;
    FLCN_STATUS             status;

    status = clkDomainGrpIfaceModel10ObjSet_30_PROG(pModel10, ppBoardObj, size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainGrpIfaceModel10ObjSet_30_SECONDARY_exit;
    }
    pBoardObjVtable = (BOARDOBJ_VTABLE *)*ppBoardObj;
    pDomain         = (CLK_DOMAIN_30_SECONDARY *)*ppBoardObj;

    // Construct 3X_Secondary super class
    status = clkDomainConstruct_3X_SECONDARY(pBObjGrp,
                &pDomain->secondary.super,
                &pSet->secondary.super,
                &ClkDomain3XSecondaryVirtualTable);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainGrpIfaceModel10ObjSet_30_SECONDARY_exit;
    }

    // Override the Vtable pointer.
    pBoardObjVtable->pVirtualTable = &ClkDomain30SecondaryVirtualTable;

clkDomainGrpIfaceModel10ObjSet_30_SECONDARY_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @copydoc BoardObjVirtualTableGetInterfaceFromBoardObj
 */
static BOARDOBJ_INTERFACE *
s_clkDomainGetInterfaceFromBoardObj_30_Secondary
(
    BOARDOBJ   *pBoardObj,
    LwU8        interfaceType
)
{
    CLK_DOMAIN_30_SECONDARY *pDomain30Secondary = (CLK_DOMAIN_30_SECONDARY *)pBoardObj;

    switch (interfaceType)
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_INTERFACE_TYPE_3X_SECONDARY:
        {
            return &pDomain30Secondary->secondary.super;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_INTERFACE_TYPE_PROG:
        {
            return &pDomain30Secondary->super.super.prog.super;
        }
    }

    PMU_BREAKPOINT();
    return NULL;
}
