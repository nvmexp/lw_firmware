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

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_DOMAIN_30_PROG class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkDomainGrpIfaceModel10ObjSet_30_PROG
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    RM_PMU_CLK_CLK_DOMAIN_30_PROG_BOARDOBJ_SET *pSet =
        (RM_PMU_CLK_CLK_DOMAIN_30_PROG_BOARDOBJ_SET *)pBoardObjSet;
    CLK_DOMAIN_30_PROG *pDomain;
    FLCN_STATUS         status;

    status = clkDomainGrpIfaceModel10ObjSet_3X_PROG(pModel10, ppBoardObj, size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainGrpIfaceModel10ObjSet_30_PROG_exit;
    }
    pDomain = (CLK_DOMAIN_30_PROG *)*ppBoardObj;

    // Copy the CLK_DOMAIN_30_PROG-specific data.
    pDomain->noiseUnawareOrderingIndex  = pSet->noiseUnawareOrderingIndex;
    pDomain->noiseAwareOrderingIndex    = pSet->noiseAwareOrderingIndex;

clkDomainGrpIfaceModel10ObjSet_30_PROG_exit:
    return status;
}

/*!
 * @copydoc ClkDomainProgIsSecVFLwrvesEnabled
 *
 * Secondary VF Lwrves are not supported in _30_PROG type clock domains.
 */
FLCN_STATUS
clkDomainProgIsSecVFLwrvesEnabled_30
(
    CLK_DOMAIN_PROG                    *pDomainProg,
    LwBool                             *pBIsSecVFLwrvesEnabled
)
{
    *pBIsSecVFLwrvesEnabled = LW_FALSE;

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
