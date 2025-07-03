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
 * @file clk_prop_top_rel_1x_ratio.c
 *
 * @brief Module managing all state related to the CLK_PROP_TOP_REL_1X_RATIO structure.
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "dmemovl.h"

/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_PROP_TOP_REL_1X_RATIO class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkPropTopRelGrpIfaceModel10ObjSet_1X_RATIO
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    RM_PMU_CLK_CLK_PROP_TOP_REL_1X_RATIO_BOARDOBJ_SET *pRel1xRatioSet =
        (RM_PMU_CLK_CLK_PROP_TOP_REL_1X_RATIO_BOARDOBJ_SET *)pBoardObjSet;
    CLK_PROP_TOP_REL_1X_RATIO *pRel1xRatio;
    FLCN_STATUS                status;

    status = clkPropTopRelGrpIfaceModel10ObjSet_1X(pModel10, ppBoardObj, size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        // Super class triggers PMU Break point on error.
        goto clkPropTopRelGrpIfaceModel10ObjSet_1X_RATIO_exit;
    }
    pRel1xRatio = (CLK_PROP_TOP_REL_1X_RATIO *)*ppBoardObj;

    // Copy the CLK_PROP_TOP_REL_1X_RATIO-specific data.
    pRel1xRatio->ratio        = pRel1xRatioSet->ratio;
    pRel1xRatio->ratioIlwerse = pRel1xRatioSet->ratioIlwerse;

clkPropTopRelGrpIfaceModel10ObjSet_1X_RATIO_exit:
    return status;
}

/*!
 * @copydoc ClkPropTopRelFreqPropagate
 */
FLCN_STATUS
clkPropTopRelFreqPropagate_1X_RATIO
(
    CLK_PROP_TOP_REL   *pPropTopRel,
    LwBool              bSrcToDstProp,
    LwU16              *pFreqMHz
)
{
    CLK_PROP_TOP_REL_1X_RATIO  *pRel1xRatio = (CLK_PROP_TOP_REL_1X_RATIO *)pPropTopRel;
    LwUFXP16_16                 tempRatio   = pRel1xRatio->ratio;
    LwU8                        clkIdx      = pPropTopRel->clkDomainIdxDst;
    FLCN_STATUS                 status      = FLCN_OK;

    // Switch to ilwerse ratio for Dst -> Src propagation.
    if (!bSrcToDstProp)
    {
        tempRatio = pRel1xRatio->ratioIlwerse;
        clkIdx    = pPropTopRel->clkDomainIdxSrc;
    }

    //
    // FXP 16.16 * U16 = FXP 32.16
    // Discarding 16 fractional bits, the result is 32.0.
    // Casting the result down to 16.0
    //
    (*pFreqMHz) = (LwU16)multUFXP16_16((*pFreqMHz), tempRatio);

    // Quantize.
    {
        CLK_DOMAIN_40_PROG         *pDomain40Prog =
            CLK_CLK_DOMAIN_40_PROG_GET_BY_IDX(clkIdx);
        LW2080_CTRL_CLK_VF_INPUT    inputFreq;
        LW2080_CTRL_CLK_VF_OUTPUT   outputFreq;

        if (pDomain40Prog == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkPropTopRelFreqPropagate_1X_RATIO_exit;
        }

        // Init
        LW2080_CTRL_CLK_VF_INPUT_INIT(&inputFreq);
        LW2080_CTRL_CLK_VF_OUTPUT_INIT(&outputFreq);

        inputFreq.value = (*pFreqMHz);

        //
        // Quantize based on the direction of propagation.
        // This is to ensure back propagation from destination to
        // source provide the same original source frequency.
        //
        status = clkDomainProgFreqQuantize(
                    CLK_DOMAIN_PROG_GET_FROM_40_PROG(pDomain40Prog),    // pDomainProg
                    &inputFreq,                                         // pInputFreq
                    &outputFreq,                                        // pOutputFreq
                    bSrcToDstProp,                                      // bFloor
                    LW_FALSE);                                          // bBound
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkPropTopRelFreqPropagate_1X_RATIO_exit;
        }
        (*pFreqMHz) = (LwU16) outputFreq.value;
    }

clkPropTopRelFreqPropagate_1X_RATIO_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
