/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file clk_prop.c
 *
 * @brief Module managing all state related to the CLK_PROP structure.
 *
 * @ref - https://confluence.lwpu.com/display/RMPER/Clock+Propagation
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
static BoardObjGrpIfaceModel10SetEntry  (s_clkPropIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "s_clkPropIfaceModel10SetEntry");

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_PROP handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
clkPropBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status;

    status = BOARDOBJGRP_IFACE_MODEL_10_SET(E32,             // _grpType
        CLK_PROP,                                   // _class
        pBuffer,                                    // _pBuffer
        boardObjGrpIfaceModel10SetHeader,                 // _hdrFunc
        s_clkPropIfaceModel10SetEntry,                    // _entryFunc
        ga10xAndLater.clk.clkPropGrpSet,            // _ssElement
        OVL_INDEX_DMEM(perf),                       // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
    {
        PMU_BREAKPOINT();
        goto clkPropBoardObjGrpIfaceModel10Set_exit;
    }

clkPropBoardObjGrpIfaceModel10Set_exit:
    return status;
}

/*!
 * CLK_PROP class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkPropGrpIfaceModel10ObjSet_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    RM_PMU_CLK_CLK_PROP_BOARDOBJ_SET *pPropSet =
        (RM_PMU_CLK_CLK_PROP_BOARDOBJ_SET *)pBoardObjSet;
    CLK_PROP       *pProp;
    FLCN_STATUS     status;

    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkPropGrpIfaceModel10ObjSet_SUPER_exit;
    }
    pProp = (CLK_PROP *)*ppBoardObj;

    // Copy the CLK_PROP-specific data.
    pProp->rsvd = pPropSet->rsvd;

clkPropGrpIfaceModel10ObjSet_SUPER_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * CLK_PROP implementation to parse BOARDOBJ entry.
 *
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
static FLCN_STATUS
s_clkPropIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    return clkPropGrpIfaceModel10ObjSet_SUPER(pModel10,
                                  ppBoardObj,
                                  sizeof(CLK_PROP),
                                  pBoardObjSet);
}
