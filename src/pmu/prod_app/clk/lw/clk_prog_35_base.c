/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file clk_prog_35.c
 *
 * @brief Module managing all state related to the CLK_PROG_35 structure.  This
 * structure defines how to program a given frequency on given CLK_DOMAIN -
 * including which frequency generator to use and potentially the necessary VF
 * mapping, per the VBIOS Clock Programming Table 1.0 specification.
 *
 */
//! https://wiki.lwpu.com/engwiki/index.php/Resman/PState/Data_Tables/Clock_Programming_Table/1.0_Spec

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_PROG_3X class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkProgGrpIfaceModel10ObjSet_35
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_CLK_CLK_PROG_3X_BOARDOBJ_SET *p3XSet =
        (RM_PMU_CLK_CLK_PROG_3X_BOARDOBJ_SET *)pBoardObjDesc;
    CLK_PROG_35    *pProg35;
    FLCN_STATUS     status;

    status = clkProgGrpIfaceModel10ObjSet_3X(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkProgGrpIfaceModel10ObjSet_35_exit;
    }
    pProg35 = (CLK_PROG_35 *)*ppBoardObj;

    // Init offset adjusted frequency to base value on construct.
    pProg35->offsettedFreqMaxMHz = p3XSet->freqMaxMHz;

clkProgGrpIfaceModel10ObjSet_35_exit:
    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
clkProgIfaceModel10GetStatus_35
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_CLK_CLK_PROG_35_BOARDOBJ_GET_STATUS *pProg35Status =
        (RM_PMU_CLK_CLK_PROG_35_BOARDOBJ_GET_STATUS *)pPayload;
    CLK_PROG_35    *pProg35 = (CLK_PROG_35 *)pBoardObj;
    FLCN_STATUS     status  = FLCN_OK;

    status = boardObjIfaceModel10GetStatus(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto clkProgIfaceModel10GetStatus_35_exit;
    }

    // Copy-out the _35 specific params.
    pProg35Status->offsettedFreqMaxMHz = pProg35->offsettedFreqMaxMHz;

clkProgIfaceModel10GetStatus_35_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
