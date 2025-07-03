/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_DOMAIN_3X_FIXED class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkDomainGrpIfaceModel10ObjSet_3X_FIXED
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_CLK_CLK_DOMAIN_3X_FIXED_BOARDOBJ_SET *pSet =
        (RM_PMU_CLK_CLK_DOMAIN_3X_FIXED_BOARDOBJ_SET *)pBoardObjDesc;
    CLK_DOMAIN_3X_FIXED *pDomain;
    FLCN_STATUS    status;

    status = clkDomainGrpIfaceModel10ObjSet_3X(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainGrpIfaceModel10ObjSet_3X_FIXED_exit;
    }
    pDomain = (CLK_DOMAIN_3X_FIXED *)*ppBoardObj;

    // Copy the CLK_DOMAIN_3X_FIXED-specific data.
    pDomain->freqMHz = pSet->freqMHz;

clkDomainGrpIfaceModel10ObjSet_3X_FIXED_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
