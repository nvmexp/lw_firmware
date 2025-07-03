/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    vfe_equ_equation_scalar.c
 * @brief   VFE_EQU_EQUATION_SCALAR class implementation.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objperf.h"
#include "perf/3x/vfe_equ_equation_scalar.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjVirtualTableDynamicCast (s_vfeEquDynamicCast_EQUATION_SCALAR)
    GCC_ATTRIB_SECTION("imem_perf", "s_vfeEquDynamicCast_EQUATION_SCALAR")
    GCC_ATTRIB_USED();

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/*!
 * Virtual table for VFE_EQU_EQUATION_SCALAR object interfaces.
 */
BOARDOBJ_VIRTUAL_TABLE VfeEquEquationScalarVirtualTable =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_DYNAMIC_CAST(s_vfeEquDynamicCast_EQUATION_SCALAR)
};

/* ------------------------ Inline Functions -------------------------------- */
/*!
 * @brief Constructor validator helper.
 *
 * @memberof VFE_EQU_EQUATION_SCALAR
 * @private
 *
 * Performs validation of the parameters passed to the object constructor.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
s_vfeEquConstructValidateHelper_EQUATION_SCALAR
(
    BOARDOBJGRP                    *pBObjGrp,
    VFE_EQU_EQUATION_SCALAR        *pVfeEqu,
    RM_PMU_VFE_EQU_EQUATION_SCALAR *pDescEqu
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
 * @memberof VFE_EQU_EQUATION_SCALAR
 * @public
 */
FLCN_STATUS
vfeEquGrpIfaceModel10ObjSetImpl_EQUATION_SCALAR
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_VFE_EQU_EQUATION_SCALAR *pDescEqu =
        (RM_PMU_VFE_EQU_EQUATION_SCALAR *)pBoardObjDesc;
    VFE_EQU_EQUATION_SCALAR        *pVfeEqu;
    FLCN_STATUS                     status;

    PMU_ASSERT_OK_OR_GOTO(status,
        vfeEquGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc),
        vfeEquGrpIfaceModel10ObjSetImpl_EQUATION_SCALAR_exit);

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(
        pVfeEqu, *ppBoardObj, PERF, VFE_EQU, EQUATION_SCALAR, &status,
        vfeEquGrpIfaceModel10ObjSetImpl_EQUATION_SCALAR_exit);

    if (!PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJ_CONSTRUCT_VALIDATE))
    {
        PMU_ASSERT_OK_OR_GOTO(status, 
            s_vfeEquConstructValidateHelper_EQUATION_SCALAR(pBObjGrp, pVfeEqu, pDescEqu),
            vfeEquGrpIfaceModel10ObjSetImpl_EQUATION_SCALAR_exit);
    }

    // Set member variables.
    VFE_EQU_IDX_COPY_RM_TO_PMU(pVfeEqu->equIdxToScale, pDescEqu->equIdxToScale);

vfeEquGrpIfaceModel10ObjSetImpl_EQUATION_SCALAR_exit:
    return status;
}

/*!
 * @brief Constructor validator.
 *
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 * @memberof VFE_EQU_EQUATION_SCALAR
 * @public
 */
FLCN_STATUS
vfeEquGrpIfaceModel10ObjSetValidate_EQUATION_SCALAR
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_VFE_EQU_EQUATION_SCALAR *pDescEqu =
        (RM_PMU_VFE_EQU_EQUATION_SCALAR *)pBoardObjDesc;
    VFE_EQU_EQUATION_SCALAR        *pVfeEqu;
    FLCN_STATUS                     status;

    PMU_ASSERT_OK_OR_GOTO(status,
        vfeEquGrpIfaceModel10ObjSetValidate_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc),
        vfeEquGrpIfaceModel10ObjSetValidate_EQUATION_SCALAR);

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(
        pVfeEqu, *ppBoardObj, PERF, VFE_EQU, EQUATION_SCALAR, &status,
        vfeEquGrpIfaceModel10ObjSetValidate_EQUATION_SCALAR);

    PMU_ASSERT_OK_OR_GOTO(status, 
        s_vfeEquConstructValidateHelper_EQUATION_SCALAR(pBObjGrp, pVfeEqu, pDescEqu),
        vfeEquGrpIfaceModel10ObjSetValidate_EQUATION_SCALAR);

vfeEquGrpIfaceModel10ObjSetValidate_EQUATION_SCALAR:
    return status;
}

/*!
 * @brief Evaluates an equation scalar equation node.
 *
 * @copydetails VfeEquEvalNode()
 * @memberof VFE_EQU_EQUATION_SCALAR
 * @public
 */
FLCN_STATUS
vfeEquEvalNode_EQUATION_SCALAR
(
    VFE_CONTEXT    *pContext,
    VFE_EQU        *pVfeEqu,
    LwF32          *pResult
)
{
    VFE_EQU_EQUATION_SCALAR  *pVfeEquEqScalar;
    FLCN_STATUS               status;
    LwF32                     variable;
    LwF32                     result;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVfeEquEqScalar, &pVfeEqu->super, PERF, VFE_EQU, EQUATION_SCALAR,
                                  &status, vfeEquEvalNode_EQUATION_SCALAR_exit);

    //
    // Force use of cached value to avoid stack usage for the VAR evaluation.
    // Caller @ref vfeEquEvaluate() assures that required variables are cached.
    //
    PMU_HALT_OK_OR_GOTO(status,
        vfeVarEvalByIdxFromCache(pContext, pVfeEquEqScalar->super.varIdx, &variable),
        vfeEquEvalNode_EQUATION_SCALAR_exit);

    // Evaluate the quation that needs to be scaled
    PMU_HALT_OK_OR_GOTO(status,
        vfeEquEvalList(pContext, pVfeEquEqScalar->equIdxToScale, &result),
        vfeEquEvalNode_EQUATION_SCALAR_exit);

    // Scale the result by variable
    *pResult = lwF32Mul(result, variable);

    // Finally at the end call parent implementation.
    status = vfeEquEvalNode_SUPER(pContext, pVfeEqu, pResult);

vfeEquEvalNode_EQUATION_SCALAR_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @brief   VFE_EQU_EQUATION_SCALAR inmplemenetation of
 *          @ref BoardObjVirtualTableDynamicCast()
 *
 * @copydoc BoardObjVirtualTableDynamicCast()
 */
static void *
s_vfeEquDynamicCast_EQUATION_SCALAR
(
    BOARDOBJ   *pBoardObj,
    LwU8        requestedType
)
{
    void                    *pObject       = NULL;
    VFE_EQU_EQUATION_SCALAR *pVfeEquScalar = (VFE_EQU_EQUATION_SCALAR *)pBoardObj;

    if (BOARDOBJ_GET_TYPE(pBoardObj) !=
        LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_EQU, EQUATION_SCALAR))
    {
        PMU_BREAKPOINT();
        goto s_vfeEquDynamicCast_EQUATION_SCALAR_exit;
    }

    switch (requestedType)
    {
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_EQU, BASE):
        {
            VFE_EQU *pVfeEqu = &pVfeEquScalar->super;
            pObject = (void *)pVfeEqu;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_EQU, EQUATION_SCALAR):
        {
            pObject = (void *)pVfeEquScalar;
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            break;
        }
    }

s_vfeEquDynamicCast_EQUATION_SCALAR_exit:
    return pObject;
}
