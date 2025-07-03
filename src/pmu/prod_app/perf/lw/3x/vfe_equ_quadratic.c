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
 * @file    vfe_equ_quadratic.c
 * @brief   VFE_EQU_QUADRATIC class implementation.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objperf.h"
#include "perf/3x/vfe_equ_quadratic.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjVirtualTableDynamicCast (s_vfeEquDynamicCast_QUADRATIC)
    GCC_ATTRIB_SECTION("imem_perf", "s_vfeEquDynamicCast_QUADRATIC")
    GCC_ATTRIB_USED();

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/*!
 * Virtual table for VFE_EQU_QUADRATIC object interfaces.
 */
BOARDOBJ_VIRTUAL_TABLE VfeEquQuadraticVirtualTable =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_DYNAMIC_CAST(s_vfeEquDynamicCast_QUADRATIC)
};

/* ------------------------ Inline Functions -------------------------------- */
/*!
 * @brief Constructor validator helper.
 *
 * @memberof VFE_EQU_QUADRATIC
 * @private
 *
 * Performs validation of the parameters passed to the object constructor.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
s_vfeEquConstructValidateHelper_QUADRATIC
(
    BOARDOBJGRP                *pBObjGrp,
    VFE_EQU_QUADRATIC          *pVfeEqu,
    RM_PMU_VFE_EQU_QUADRATIC   *pDescEqu
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
 * @memberof VFE_EQU_QUADRATIC
 * @public
 */
FLCN_STATUS
vfeEquGrpIfaceModel10ObjSetImpl_QUADRATIC
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_VFE_EQU_QUADRATIC   *pDescEqu =
        (RM_PMU_VFE_EQU_QUADRATIC *)pBoardObjDesc;
    VFE_EQU_QUADRATIC          *pVfeEqu;
    FLCN_STATUS                 status;
    LwU8                        i;

    PMU_ASSERT_OK_OR_GOTO(status,
        vfeEquGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc),
        vfeEquGrpIfaceModel10ObjSetImpl_QUADRATIC_exit);

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(
        pVfeEqu, *ppBoardObj, PERF, VFE_EQU, QUADRATIC, &status,
        vfeEquGrpIfaceModel10ObjSetImpl_QUADRATIC_exit);

    if (!PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJ_CONSTRUCT_VALIDATE))
    {
        PMU_ASSERT_OK_OR_GOTO(status, 
            s_vfeEquConstructValidateHelper_QUADRATIC(pBObjGrp, pVfeEqu, pDescEqu),
            vfeEquGrpIfaceModel10ObjSetImpl_QUADRATIC_exit);
    }

    // Set member variables.
    for (i = 0; i < LW2080_CTRL_PERF_VFE_EQU_QUADRATIC_COEFF_COUNT; i++)
    {
        pVfeEqu->coeffs[i] = lwF32MapFromU32(&pDescEqu->coeffs[i]);
    }

vfeEquGrpIfaceModel10ObjSetImpl_QUADRATIC_exit:
    return status;
}

/*!
 * @brief Constructor validator.
 *
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 * @memberof VFE_EQU_QUADRATIC
 * @public
 */
FLCN_STATUS
vfeEquGrpIfaceModel10ObjSetValidate_QUADRATIC
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_VFE_EQU_QUADRATIC   *pDescEqu =
        (RM_PMU_VFE_EQU_QUADRATIC *)pBoardObjDesc;
    VFE_EQU_QUADRATIC          *pVfeEqu;
    FLCN_STATUS                 status;

    PMU_ASSERT_OK_OR_GOTO(status,
        vfeEquGrpIfaceModel10ObjSetValidate_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc),
        vfeEquGrpIfaceModel10ObjSetValidate_QUADRATIC_exit);

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(
        pVfeEqu, *ppBoardObj, PERF, VFE_EQU, QUADRATIC, &status,
        vfeEquGrpIfaceModel10ObjSetValidate_QUADRATIC_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        s_vfeEquConstructValidateHelper_QUADRATIC(pBObjGrp, pVfeEqu, pDescEqu),
        vfeEquGrpIfaceModel10ObjSetValidate_QUADRATIC_exit);

vfeEquGrpIfaceModel10ObjSetValidate_QUADRATIC_exit:
    return status;
}

/*!
 * @brief Evaluates a quadratic equation node.
 *
 * @copydetails VfeEquEvalNode()
 * @memberof VFE_VAR_QUADRATIC
 * @public
 */
FLCN_STATUS
vfeEquEvalNode_QUADRATIC
(
    VFE_CONTEXT    *pContext,
    VFE_EQU        *pVfeEqu,
    LwF32          *pResult
)
{
    VFE_EQU_QUADRATIC  *pVfeEquQuadratic;
    FLCN_STATUS         status;
    LwF32               variable;
    LwF32               result;
    LwU32               index;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVfeEquQuadratic, &pVfeEqu->super, PERF, VFE_EQU, QUADRATIC,
                                  &status, vfeEquEvalNode_QUAD_exit);

    //
    // Force use of cached value to avoid stack usage for the VAR evaluation.
    // Caller @ref vfeEquEvaluate() assures that required variables are cached.
    //
    status = vfeVarEvalByIdxFromCache(pContext, pVfeEquQuadratic->super.varIdx, &variable);
    if (status != FLCN_OK)
    {
        goto vfeEquEvalNode_QUAD_exit;
    }

    // result = (((c2 * var) + c1) * var) + c0

    index  = (LW2080_CTRL_PERF_VFE_EQU_QUADRATIC_COEFF_COUNT - 1U);
    result = pVfeEquQuadratic->coeffs[index];

    while (index > 0U)
    {
        index--;
        result = lwF32Mul(result, variable);
        result = lwF32Add(result, pVfeEquQuadratic->coeffs[index]);
    }

    // Copy out only on success.
    *pResult = result;

    // Finally at the end call parent implementation.
    status = vfeEquEvalNode_SUPER(pContext, pVfeEqu, pResult);

vfeEquEvalNode_QUAD_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @brief   VFE_EQU_QUADRATIC inmplemenetation of
 *          @ref BoardObjVirtualTableDynamicCast()
 *
 * @copydoc BoardObjVirtualTableDynamicCast()
 */
static void *
s_vfeEquDynamicCast_QUADRATIC
(
    BOARDOBJ   *pBoardObj,
    LwU8        requestedType
)
{
    void              *pObject          = NULL;
    VFE_EQU_QUADRATIC *pVfeEquQuadratic = (VFE_EQU_QUADRATIC *)pBoardObj;

    if (BOARDOBJ_GET_TYPE(pBoardObj) !=
        LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_EQU, QUADRATIC))
    {
        PMU_BREAKPOINT();
        goto s_vfeEquDynamicCast_QUADRATIC_exit;
    }

    switch (requestedType)
    {
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_EQU, BASE):
        {
            VFE_EQU *pVfeEqu = &pVfeEquQuadratic->super;
            pObject = (void *)pVfeEqu;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_EQU, QUADRATIC):
        {
            pObject = (void *)pVfeEquQuadratic;
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            break;
        }
    }

s_vfeEquDynamicCast_QUADRATIC_exit:
    return pObject;
}
