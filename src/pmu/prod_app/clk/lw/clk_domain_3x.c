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
 * CLK_DOMAIN_3X class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkDomainGrpIfaceModel10ObjSet_3X
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_CLK_CLK_DOMAIN_3X_BOARDOBJ_SET *pSet =
        (RM_PMU_CLK_CLK_DOMAIN_3X_BOARDOBJ_SET *)pBoardObjDesc;
    CLK_DOMAIN_3X *pDomain;
    FLCN_STATUS    status;

    status = clkDomainGrpIfaceModel10ObjSet_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainGrpIfaceModel10ObjSet_3X_exit;
    }
    pDomain = (CLK_DOMAIN_3X *)*ppBoardObj;

    // Copy the CLK_DOMAIN_3X-specific data.
    pDomain->bNoiseAwareCapable = pSet->bNoiseAwareCapable;

clkDomainGrpIfaceModel10ObjSet_3X_exit:
    return status;
}

/*!
 * @copydoc ClkDomainProgIsNoiseAware
 */
FLCN_STATUS
clkDomainProgIsNoiseAware_3X
(
    CLK_DOMAIN_PROG *pDomainProg,
    LwBool          *pbIsNoiseAware
)
{
    CLK_DOMAIN_3X *pDomain3x =
        (CLK_DOMAIN_3X *)INTERFACE_TO_BOARDOBJ_CAST(pDomainProg);
    FLCN_STATUS    status    = FLCN_OK;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDomain3x != NULL), FLCN_ERR_ILWALID_INDEX,
        clkDomainProgIsNoiseAware_3X_exit);

    *pbIsNoiseAware = pDomain3x->bNoiseAwareCapable;

clkDomainProgIsNoiseAware_3X_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
