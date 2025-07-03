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
 * @file clk_prop_top_rel_1x_vfe.c
 *
 * @brief Module managing all state related to the CLK_PROP_TOP_REL_1X_VFE structure.
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "perf/3x/vfe.h"
#include "dmemovl.h"

/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_PROP_TOP_REL_1X_VFE class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkPropTopRelGrpIfaceModel10ObjSet_1X_VFE
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    RM_PMU_CLK_CLK_PROP_TOP_REL_1X_VFE_BOARDOBJ_SET *pRel1xVfeSet =
        (RM_PMU_CLK_CLK_PROP_TOP_REL_1X_VFE_BOARDOBJ_SET *)pBoardObjSet;
    CLK_PROP_TOP_REL_1X_VFE *pRel1xVfe;
    FLCN_STATUS              status;

    status = clkPropTopRelGrpIfaceModel10ObjSet_1X(pModel10, ppBoardObj, size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        // Super class triggers PMU Break point on error.
        goto clkPropTopRelGrpIfaceModel10ObjSet_1X_VFE_exit;
    }
    pRel1xVfe = (CLK_PROP_TOP_REL_1X_VFE *)*ppBoardObj;

    // Copy the CLK_PROP_TOP_REL_1X_VFE-specific data.
    VFE_EQU_IDX_COPY_RM_TO_PMU(pRel1xVfe->vfeIdx, pRel1xVfeSet->vfeIdx);

clkPropTopRelGrpIfaceModel10ObjSet_1X_VFE_exit:
    return status;
}

/*!
 * @copydoc ClkPropTopRelFreqPropagate
 *
 * @prereq Client are expected to attach all required overlays.
 */
FLCN_STATUS
clkPropTopRelFreqPropagate_1X_VFE
(
    CLK_PROP_TOP_REL   *pPropTopRel,
    LwBool              bSrcToDstProp,
    LwU16              *pFreqMHz
)
{
    CLK_PROP_TOP_REL_1X_VFE    *pRel1xVfe = (CLK_PROP_TOP_REL_1X_VFE *)pPropTopRel;
    LwU8                        clkIdx    = pPropTopRel->clkDomainIdxDst;
    RM_PMU_PERF_VFE_VAR_VALUE   vfeVarVal;
    RM_PMU_PERF_VFE_EQU_RESULT  result;
    FLCN_STATUS                 status;

    // Sanity check the input arguments.
    if (!bSrcToDstProp)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkPropTopRelFreqPropagate_1X_VFE_exit;
    }

    // Propagate src to dst frequency.
    vfeVarVal.frequency.varType      = LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_FREQUENCY;
    vfeVarVal.frequency.varValue     = (*pFreqMHz);
    vfeVarVal.frequency.clkDomainIdx = LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID;
    status = vfeEquEvaluate(
                pRel1xVfe->vfeIdx,                              // vfeEquIdx
                &vfeVarVal,                                     // pValues
                1,                                              // valCount
                LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_FREQ_MHZ,  // outputType
                &result);                                       // pResult
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkPropTopRelFreqPropagate_1X_VFE_exit;
    }
    (*pFreqMHz) = (LwU16)result.freqMHz;

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
            goto clkPropTopRelFreqPropagate_1X_VFE_exit;
        }

        // Init
        LW2080_CTRL_CLK_VF_INPUT_INIT(&inputFreq);
        LW2080_CTRL_CLK_VF_OUTPUT_INIT(&outputFreq);

        inputFreq.value = (*pFreqMHz);

        // Quantize down the propagated frequency.
        status = clkDomainProgFreqQuantize(
                    CLK_DOMAIN_PROG_GET_FROM_40_PROG(pDomain40Prog),    // pDomainProg
                    &inputFreq,                                         // pInputFreq
                    &outputFreq,                                        // pOutputFreq
                    LW_TRUE,                                            // bFloor
                    LW_FALSE);                                          // bBound
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkPropTopRelFreqPropagate_1X_VFE_exit;
        }
        (*pFreqMHz) = (LwU16) outputFreq.value;
    }

clkPropTopRelFreqPropagate_1X_VFE_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
