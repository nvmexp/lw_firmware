/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file client_clk_domain_prog.c
 *
 * @brief Functions and interfaces for programmable client clock domains.
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"
#include "dmemovl.h"
#include "clk/client_clk_domain_prog.h"
#include "boardobj/boardobj.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * Client clock domain programmable constructor
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clientClkDomainGrpIfaceModel10ObjSet_PROG
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_CLK_CLIENT_CLK_DOMAIN_PROG_BOARDOBJ_SET *pSet = 
        (RM_PMU_CLK_CLIENT_CLK_DOMAIN_PROG_BOARDOBJ_SET *)pBoardObjDesc;
    CLIENT_CLK_DOMAIN_PROG     *pClientClkDomainProg;
    FLCN_STATUS                 status;

    // Call into super-class constructor
    status = clientClkDomainGrpIfaceModel10ObjSet_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clientClkDomainGrpIfaceModel10ObjSet_PROG_exit;
    }

    pClientClkDomainProg = (CLIENT_CLK_DOMAIN_PROG *)*ppBoardObj;

    // Set CLIENT_CLK_DOMAIN_PROG state
    pClientClkDomainProg->vfPointIdxFirst = pSet->vfPointIdxFirst;
    pClientClkDomainProg->vfPointIdxLast  = pSet->vfPointIdxLast;
    pClientClkDomainProg->freqDelta       = pSet->freqDelta;

clientClkDomainGrpIfaceModel10ObjSet_PROG_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
