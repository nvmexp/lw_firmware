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
static BoardObjVirtualTableGetInterfaceFromBoardObj (s_clkDomainGetInterfaceFromBoardObj_35_Secondary)
    GCC_ATTRIB_SECTION("imem_perfVf", "s_clkDomainGetInterfaceFromBoardObj_35_Secondary");

/* ------------------------ Global Variables ------------------------------- */
/*!
 * Main structure for all CLK_DOMAIN_35_SECONDARY_VIRTUAL_TABLE data.
 */
static BOARDOBJ_VIRTUAL_TABLE ClkDomain35SecondaryVirtualTable =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_GET_INTERFACE_FROM_BOARDOBJ(
        s_clkDomainGetInterfaceFromBoardObj_35_Secondary)
};

/*!
 * Main structure for all CLK_DOMAIN_3X_SECONDARY_VIRTUAL_TABLE data.
 */
static INTERFACE_VIRTUAL_TABLE ClkDomain3XSecondaryVirtualTable =
{
    LW_OFFSETOF(CLK_DOMAIN_35_SECONDARY, secondary)   // offset
};

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_DOMAIN_35_SECONDARY class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkDomainGrpIfaceModel10ObjSet_35_SECONDARY
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_CLK_CLK_DOMAIN_35_SECONDARY_BOARDOBJ_SET *pSet35Secondary =
        (RM_PMU_CLK_CLK_DOMAIN_35_SECONDARY_BOARDOBJ_SET *)pBoardObjDesc;
    CLK_DOMAIN_35_SECONDARY    *pDomain35Secondary;
    BOARDOBJ_VTABLE        *pBoardObjVtable;
    FLCN_STATUS             status;

    status = clkDomainGrpIfaceModel10ObjSet_35_PROG(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainGrpIfaceModel10ObjSet_35_SECONDARY_exit;
    }
    pBoardObjVtable = (BOARDOBJ_VTABLE *)*ppBoardObj;
    pDomain35Secondary  = (CLK_DOMAIN_35_SECONDARY *)*ppBoardObj;

    // Construct 3X_Secondary super class
    status = clkDomainConstruct_3X_SECONDARY(pBObjGrp,
                &pDomain35Secondary->secondary.super,
                &pSet35Secondary->secondary.super,
                &ClkDomain3XSecondaryVirtualTable);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainGrpIfaceModel10ObjSet_35_SECONDARY_exit;
    }

    // Override the Vtable pointer.
    pBoardObjVtable->pVirtualTable = &ClkDomain35SecondaryVirtualTable;

clkDomainGrpIfaceModel10ObjSet_35_SECONDARY_exit:
    return status;
}

/*!
 * @copydoc ClkDomainProgVoltToFreqTuple
 */
FLCN_STATUS
clkDomainProgVoltToFreqTuple_35_SECONDARY_IMPL
(
    CLK_DOMAIN_PROG                                *pDomainProg,
    LwU8                                            voltRailIdx,
    LwU8                                            voltageType,
    LW2080_CTRL_CLK_VF_INPUT                       *pInput,
    LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE    *pOutput,
    LW2080_CTRL_CLK_VF_ITERATION_STATE             *pVfIterState
)
{
    CLK_DOMAIN_35_PROG         *pDomain35Prog =
        (CLK_DOMAIN_35_PROG *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    CLK_DOMAIN_3X_SECONDARY        *pDomain3XSecondary  =
        CLK_CLK_DOMAIN_3X_SECONDARY_GET_FROM_35_SECONDARY((CLK_DOMAIN_35_SECONDARY *)pDomain35Prog);
    CLK_DOMAIN_35_PRIMARY       *pDomain35Primary = (CLK_DOMAIN_35_PRIMARY *)
        BOARDOBJGRP_OBJ_GET(CLK_DOMAIN, pDomain3XSecondary->primaryIdx);
    FLCN_STATUS                 status;

    if (pDomain35Primary == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto clkDomainProgVoltToFreqTuple_35_SECONDARY_exit;
    }

    status = clkDomain35PrimaryVoltToFreqTuple(pDomain35Primary, // pDomain35Primary
                BOARDOBJ_GET_GRP_IDX_8BIT(&pDomain35Prog->super.super.super.super.super), // clkDomIdx
                voltRailIdx,                                 // voltRailIdx
                voltageType,                                 // voltageType
                pInput,                                      // pInput
                pOutput,                                     // pOutput
                pVfIterState);                               // pVfIterState

clkDomainProgVoltToFreqTuple_35_SECONDARY_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @copydoc BoardObjVirtualTableGetInterfaceFromBoardObj
 */
static BOARDOBJ_INTERFACE *
s_clkDomainGetInterfaceFromBoardObj_35_Secondary
(
    BOARDOBJ   *pBoardObj,
    LwU8        interfaceType
)
{
    CLK_DOMAIN_35_SECONDARY *pDomain35Secondary = (CLK_DOMAIN_35_SECONDARY *)pBoardObj;
    BOARDOBJ_INTERFACE  *pInterface     = NULL;

    switch (interfaceType)
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_INTERFACE_TYPE_3X_SECONDARY:
        {
            pInterface = &pDomain35Secondary->secondary.super;
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_INTERFACE_TYPE_PROG:
        {
            pInterface = &pDomain35Secondary->super.super.prog.super;
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
