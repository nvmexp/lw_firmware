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
 * @file client_clk_prop_top_pol_1x_slider.c
 *
 * @brief Module managing all state related to the CLIENT_CLK_PROP_TOP_POL_1X_SLIDER structure.
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "dmemovl.h"

/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLIENT_CLK_PROP_TOP_POL_1X_SLIDER class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkClientClkPropTopPolGrpIfaceModel10ObjSet_1X_SLIDER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    RM_PMU_CLK_CLIENT_CLK_PROP_TOP_POL_1X_SLIDER_BOARDOBJ_SET *pPol1xSliderSet =
        (RM_PMU_CLK_CLIENT_CLK_PROP_TOP_POL_1X_SLIDER_BOARDOBJ_SET *)pBoardObjSet;
    CLIENT_CLK_PROP_TOP_POL_1X_SLIDER *pPol1xSlider;
    FLCN_STATUS                        status;

    status = clkClientClkPropTopPolGrpIfaceModel10ObjSet_1X(pModel10, ppBoardObj, size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        // Super class triggers PMU Break point on error.
        goto clkClientClkPropTopPolGrpIfaceModel10ObjSet_1X_SLIDER_exit;
    }
    pPol1xSlider = (CLIENT_CLK_PROP_TOP_POL_1X_SLIDER *)*ppBoardObj;

    PMU_ASSERT_OK_OR_GOTO(status,
        ((pPol1xSliderSet->numPoints <= LW2080_CTRL_CLK_CLIENT_CLK_PROP_TOP_POL_SLIDER_POINT_IDX_MAX) ?
            FLCN_OK : FLCN_ERR_ILWALID_STATE),
        clkClientClkPropTopPolGrpIfaceModel10ObjSet_1X_SLIDER_exit);

    // Copy the CLIENT_CLK_PROP_TOP_POL_1X_SLIDER-specific data.
    pPol1xSlider->numPoints     = pPol1xSliderSet->numPoints;
    pPol1xSlider->defaultPoint  = pPol1xSliderSet->defaultPoint;
    pPol1xSlider->chosenPoint   = pPol1xSliderSet->chosenPoint;

    // Init and copy in the mask.
    boardObjGrpMaskInit_E32(&(pPol1xSlider->propTopsMask));
    status = boardObjGrpMaskImport_E32(&(pPol1xSlider->propTopsMask),
                                       &(pPol1xSliderSet->propTopsMask));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkClientClkPropTopPolGrpIfaceModel10ObjSet_1X_SLIDER_exit;
    }

    (void)memcpy(pPol1xSlider->points, pPol1xSliderSet->points,
        (sizeof(RM_PMU_CLK_CLIENT_CLK_PROP_TOP_POL_1X_SLIDER_POINT) *
         pPol1xSlider->numPoints));

clkClientClkPropTopPolGrpIfaceModel10ObjSet_1X_SLIDER_exit:
    return status;
}

/*!
 * CLK_PROP_TOP_POL_1X_SLIDER implementation.
 *
 * @copydoc ClkClientClkPropTopPolGetChosenTopId
 */
FLCN_STATUS
clkClientClkPropTopPolGetChosenTopId_1X_SLIDER
(
    CLIENT_CLK_PROP_TOP_POL *pPropTopPol,
    LwU8                    *pTopId
)
{
    CLIENT_CLK_PROP_TOP_POL_1X_SLIDER *pPol1xSlider =
        (CLIENT_CLK_PROP_TOP_POL_1X_SLIDER *)pPropTopPol;
    LwU8            pointIdx =
        ((pPol1xSlider->chosenPoint != LW2080_CTRL_CLK_CLIENT_CLK_PROP_TOP_POL_SLIDER_POINT_IDX_ILWALID) ?
            pPol1xSlider->chosenPoint : pPol1xSlider->defaultPoint);

    (*pTopId) = pPol1xSlider->points[pointIdx].topId;

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
