/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file clk_prog_3x.c
 *
 * @brief Module managing all state related to the CLK_PROG_3X structure.  This
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
clkProgGrpIfaceModel10ObjSet_3X
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_CLK_CLK_PROG_3X_BOARDOBJ_SET *p3XSet =
        (RM_PMU_CLK_CLK_PROG_3X_BOARDOBJ_SET *)pBoardObjDesc;
    CLK_PROG_3X *p3X;
    FLCN_STATUS  status;

    status = clkProgGrpIfaceModel10ObjSet_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkProgGrpIfaceModel10ObjSet_3X_exit;
    }
    p3X = (CLK_PROG_3X *)*ppBoardObj;

    // Copy the CLK_PROG_3X-specific data.
    p3X->source     = p3XSet->source;
    p3X->freqMaxMHz = p3XSet->freqMaxMHz;
    p3X->sourceData = p3XSet->sourceData;

clkProgGrpIfaceModel10ObjSet_3X_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
