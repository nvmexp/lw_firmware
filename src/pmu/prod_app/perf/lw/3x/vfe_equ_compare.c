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
 * @file    vfe_equ_compare.c
 * @brief   VFE_EQU_COMPARE class implementation.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objperf.h"
#include "perf/3x/vfe_equ_compare.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjVirtualTableDynamicCast (s_vfeEquDynamicCast_COMPARE)
    GCC_ATTRIB_SECTION("imem_perf", "s_vfeEquDynamicCast_COMPARE")
    GCC_ATTRIB_USED();

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/*!
 * Virtual table for VFE_EQU_COMPARE object interfaces.
 */
BOARDOBJ_VIRTUAL_TABLE VfeEquCompareVirtualTable =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_DYNAMIC_CAST(s_vfeEquDynamicCast_COMPARE)
};

/* ------------------------ Inline Functions -------------------------------- */
/*!
 * @brief Constructor validator helper.
 *
 * @memberof VFE_EQU_COMPARE
 * @private
 *
 * Performs validation of the parameters passed to the object constructor.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
s_vfeEquConstructValidateHelper_COMPARE
(
    BOARDOBJGRP            *pBObjGrp,
    VFE_EQU_COMPARE        *pVfeEqu,
    RM_PMU_VFE_EQU_COMPARE *pDescEqu
)
{
    // Placeholder for more input parameter checks.

    return FLCN_OK;
}

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Constructor.
 *
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 * @memberof VFE_EQU_COMPARE
 * @public
 */
FLCN_STATUS
vfeEquGrpIfaceModel10ObjSetImpl_COMPARE
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_VFE_EQU_COMPARE *pDescEqu =
        (RM_PMU_VFE_EQU_COMPARE *)pBoardObjDesc;
    VFE_EQU_COMPARE        *pVfeEqu;
    FLCN_STATUS             status;

    PMU_ASSERT_OK_OR_GOTO(status,
        vfeEquGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc),
        vfeEquGrpIfaceModel10ObjSetImpl_COMPARE_exit);

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(
        pVfeEqu, *ppBoardObj, PERF, VFE_EQU, COMPARE, &status,
        vfeEquGrpIfaceModel10ObjSetImpl_COMPARE_exit);

    if (!PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJ_CONSTRUCT_VALIDATE))
    {
        PMU_ASSERT_OK_OR_GOTO(status, 
            s_vfeEquConstructValidateHelper_COMPARE(pBObjGrp, pVfeEqu, pDescEqu),
            vfeEquGrpIfaceModel10ObjSetImpl_COMPARE_exit);
    }

    // Set member variables.
    pVfeEqu->funcId      = pDescEqu->funcId;
    VFE_EQU_IDX_COPY_RM_TO_PMU(pVfeEqu->equIdxTrue, pDescEqu->equIdxTrue);
    VFE_EQU_IDX_COPY_RM_TO_PMU(pVfeEqu->equIdxFalse, pDescEqu->equIdxFalse);
    pVfeEqu->criteria    = lwF32MapFromU32(&pDescEqu->criteria);

vfeEquGrpIfaceModel10ObjSetImpl_COMPARE_exit:
    return status;
}

/*!
 * @brief Constructor validator.
 *
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 * @memberof VFE_EQU_COMPARE
 * @public
 */
FLCN_STATUS
vfeEquGrpIfaceModel10ObjSetValidate_COMPARE
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_VFE_EQU_COMPARE *pDescEqu =
        (RM_PMU_VFE_EQU_COMPARE *)pBoardObjDesc;
    VFE_EQU_COMPARE        *pVfeEqu;
    FLCN_STATUS             status;

    PMU_ASSERT_OK_OR_GOTO(status,
        vfeEquGrpIfaceModel10ObjSetValidate_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc),
        vfeEquGrpIfaceModel10ObjSetValidate_COMPARE_exit);

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(
        pVfeEqu, *ppBoardObj, PERF, VFE_EQU, COMPARE, &status,
        vfeEquGrpIfaceModel10ObjSetValidate_COMPARE_exit);

    PMU_ASSERT_OK_OR_GOTO(status, 
        s_vfeEquConstructValidateHelper_COMPARE(pBObjGrp, pVfeEqu, pDescEqu),
        vfeEquGrpIfaceModel10ObjSetValidate_COMPARE_exit);

vfeEquGrpIfaceModel10ObjSetValidate_COMPARE_exit:
    return status;
}

/*!
 * @brief Evaluates a compare equation node.
 *
 * @copydetails VfeEquEvalNode()
 * @memberof VFE_EQU_COMPARE
 * @public
 */
FLCN_STATUS
vfeEquEvalNode_COMPARE_IMPL
(
    VFE_CONTEXT    *pContext,
    VFE_EQU        *pVfeEqu,
    LwF32          *pResult
)
{
    VFE_EQU_COMPARE    *pVfeEquCompare;
    FLCN_STATUS         status;
    LwF32               variable;
    LwBool              bResult;
    LwVfeEquIdx         equIdx;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVfeEquCompare, &pVfeEqu->super, PERF, VFE_EQU, COMPARE,
                                  &status, vfeEquEvalNode_COMPARE_exit);

    //
    // Force use of cached value to avoid stack usage for the VAR evaluation.
    // Caller @ref vfeEquEvaluate() assures that required variables are cached.
    //
    PMU_HALT_OK_OR_GOTO(status,
        vfeVarEvalByIdxFromCache(pContext, pVfeEquCompare->super.varIdx, &variable),
        vfeEquEvalNode_COMPARE_exit);

    switch (pVfeEquCompare->funcId)
    {
        case LW2080_CTRL_PERF_VFE_EQU_COMPARE_FUNCTION_EQUAL:
            bResult = lwF32CmpEQ(variable, pVfeEquCompare->criteria);
            break;
        case LW2080_CTRL_PERF_VFE_EQU_COMPARE_FUNCTION_GREATER_EQ:
            bResult = lwF32CmpGE(variable, pVfeEquCompare->criteria);
            break;
        case LW2080_CTRL_PERF_VFE_EQU_COMPARE_FUNCTION_GREATER:
            bResult = lwF32CmpGT(variable, pVfeEquCompare->criteria);
            break;
        default:
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_STATE;
            break;
    }

    PMU_ASSERT_OK_OR_GOTO(status, status, vfeEquEvalNode_COMPARE_exit);

    equIdx = bResult ? pVfeEquCompare->equIdxTrue : pVfeEquCompare->equIdxFalse;

    PMU_HALT_OK_OR_GOTO(status,
        vfeEquEvalList(pContext, equIdx, pResult),
        vfeEquEvalNode_COMPARE_exit);

    // Finally at the end call parent implementation.
    status = vfeEquEvalNode_SUPER(pContext, pVfeEqu, pResult);

vfeEquEvalNode_COMPARE_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @brief   VFE_EQU_COMPARE inmplemenetation of
 *          @ref BoardObjVirtualTableDynamicCast()
 *
 * @copydoc BoardObjVirtualTableDynamicCast()
 */
static void *
s_vfeEquDynamicCast_COMPARE
(
    BOARDOBJ   *pBoardObj,
    LwU8        requestedType
)
{
    void            *pObject        = NULL;
    VFE_EQU_COMPARE *pVfeEquCompare = (VFE_EQU_COMPARE *)pBoardObj;

    if (BOARDOBJ_GET_TYPE(pBoardObj) !=
        LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_EQU, COMPARE))
    {
        PMU_BREAKPOINT();
        goto s_vfeEquDynamicCast_COMPARE_exit;
    }

    switch (requestedType)
    {
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_EQU, BASE):
        {
            VFE_EQU *pVfeEqu = &pVfeEquCompare->super;
            pObject = (void *)pVfeEqu;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_EQU, COMPARE):
        {
            pObject = (void *)pVfeEquCompare;
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            break;
        }
    }

s_vfeEquDynamicCast_COMPARE_exit:
    return pObject;
}
