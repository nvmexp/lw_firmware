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
 * @file client_clk_vf_point_prog_20_freq.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "perf/3x/vfe.h"
#include "volt/objvolt.h"
#include "clk/client_clk_vf_point_prog_20_freq.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */

/*!
 * CLIENT_CLK_VF_POINT_PROG_20 super-class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clientClkVfPointGrpIfaceModel10ObjSet_PROG_20_FREQ
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_CLK_CLIENT_CLK_VF_POINT_PROG_20_FREQ_BOARDOBJ_SET *pSetProg20Freq =
         (RM_PMU_CLK_CLIENT_CLK_VF_POINT_PROG_20_FREQ_BOARDOBJ_SET *)pBoardObjDesc;
    CLIENT_CLK_VF_POINT    *pClientVfPoint;
    CLK_VF_POINT           *pVfPoint;
    LwU8                    idx;
    FLCN_STATUS             status;

    //
    // Call into BOARDOBJ super-constructor, which will do actual memory
    // allocation.
    //
    status = clientClkVfPointGrpIfaceModel10ObjSet_PROG_20(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clientClkVfPointGrpIfaceModel10ObjSet_PROG_20_FREQ_exit;
    }
    pClientVfPoint = (CLIENT_CLK_VF_POINT *)*ppBoardObj;

    for (idx = 0U; idx < pClientVfPoint->numLinks; idx++)
    {
        pVfPoint = CLK_VF_POINT_GET(PRI, pClientVfPoint->link[idx].super.vfPointIdx);
        if (pVfPoint == NULL)
        {
            status = FLCN_ERR_ILWALID_INDEX;
            PMU_BREAKPOINT();
            goto clientClkVfPointGrpIfaceModel10ObjSet_PROG_20_FREQ_exit;
        }

        status = clkVfPointClientVoltDeltaSet(pVfPoint, pSetProg20Freq->voltDeltauV);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clientClkVfPointGrpIfaceModel10ObjSet_PROG_20_FREQ_exit;
        }
    }

clientClkVfPointGrpIfaceModel10ObjSet_PROG_20_FREQ_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
