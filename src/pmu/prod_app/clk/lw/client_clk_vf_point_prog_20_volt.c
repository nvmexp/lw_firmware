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
 * @file client_clk_vf_point_prog_20_volt.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "perf/3x/vfe.h"
#include "volt/objvolt.h"
#include "clk/client_clk_vf_point_prog_20_volt.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */

/*!
 * CLIENT_CLK_VF_POINT_PROG_20_VOLT super-class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clientClkVfPointGrpIfaceModel10ObjSet_PROG_20_VOLT
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_CLK_CLIENT_CLK_VF_POINT_PROG_20_VOLT_BOARDOBJ_SET *pSetProg20Volt = 
        (RM_PMU_CLK_CLIENT_CLK_VF_POINT_PROG_20_VOLT_BOARDOBJ_SET *)pBoardObjDesc;
    CLIENT_CLK_VF_POINT    *pClientVfPoint;
    CLK_VF_POINT           *pVfPoint;
    LwU8                    idx;
    FLCN_STATUS             status;

    //
    // Call into CLIENT_CLK_VF_POINT_PROG_20 super-constructor, which will do actual memory
    // allocation.
    //
    status = clientClkVfPointGrpIfaceModel10ObjSet_PROG_20(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clientClkVfPointGrpIfaceModel10ObjSet_PROG_20_VOLT_exit;
    }
    pClientVfPoint = (CLIENT_CLK_VF_POINT *)*ppBoardObj;

    for (idx = 0U; idx < pClientVfPoint->numLinks; idx++)
    {
        pVfPoint = CLK_VF_POINT_GET(PRI, pClientVfPoint->link[idx].super.vfPointIdx);
        if (pVfPoint == NULL)
        {
            status = FLCN_ERR_ILWALID_INDEX;
            PMU_BREAKPOINT();
            goto clientClkVfPointGrpIfaceModel10ObjSet_PROG_20_VOLT_exit;
        }

        status = clkVfPointClientCpmMaxOffsetOverrideSet(pVfPoint, pSetProg20Volt->cpmMaxFreqOffsetOverrideMHz);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clientClkVfPointGrpIfaceModel10ObjSet_PROG_20_VOLT_exit;
        }
    }

clientClkVfPointGrpIfaceModel10ObjSet_PROG_20_VOLT_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
