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
 * @file    vfe_var_derived_product.c
 * @brief   VFE_VAR_DERIVED_PRODUCT class implementation.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objperf.h"
#include "perf/3x/vfe_var_derived_product.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjVirtualTableDynamicCast (s_vfeVarDynamicCast_DERIVED_PRODUCT)
    GCC_ATTRIB_SECTION("imem_perf", "s_vfeVarDynamicCast_DERIVED_PRODUCT")
    GCC_ATTRIB_USED();

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/*!
 * Virtual table for VFE_VAR_DERIVED_PRODUCT object interfaces.
 */
BOARDOBJ_VIRTUAL_TABLE VfeVarDerivedProductVirtualTable =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_DYNAMIC_CAST(s_vfeVarDynamicCast_DERIVED_PRODUCT)
};

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @memberof VFE_VAR_DERIVED_PRODUCT
 *
 * @public
 *
 * @brief Constructor.
 *
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
vfeVarGrpIfaceModel10ObjSetImpl_DERIVED_PRODUCT
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_VFE_VAR_DERIVED_PRODUCT *pDescVar =
        (RM_PMU_VFE_VAR_DERIVED_PRODUCT *)pBoardObjDesc;
    VFE_VAR_DERIVED_PRODUCT        *pVfeVar;
    FLCN_STATUS                     status;

    PMU_ASSERT_OK_OR_GOTO(status,
        vfeVarGrpIfaceModel10ObjSetImpl_DERIVED(pModel10, ppBoardObj, size, pBoardObjDesc),
        vfeVarGrpIfaceModel10ObjSetImpl_DERIVED_PRODUCT_exit);

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVfeVar, *ppBoardObj, PERF, VFE_VAR, DERIVED_PRODUCT,
                                  &status, vfeVarGrpIfaceModel10ObjSetImpl_DERIVED_PRODUCT_exit);

    // Set member variables.
    pVfeVar->varIdx0 = pDescVar->varIdx0;
    pVfeVar->varIdx1 = pDescVar->varIdx1;

vfeVarGrpIfaceModel10ObjSetImpl_DERIVED_PRODUCT_exit:
    return status;
}

/*!
 * @memberof VFE_VAR_DERIVED_PRODUCT
 *
 * @public
 *
 * @copydoc VfeVarEval()
 */
FLCN_STATUS
vfeVarEval_DERIVED_PRODUCT
(
    VFE_CONTEXT    *pContext,
    VFE_VAR        *pVfeVar,
    LwF32          *pResult
)
{
    VFE_VAR_DERIVED_PRODUCT    *pVar;
    LwF32                       op0;
    LwF32                       op1;
    FLCN_STATUS                 status;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVar, &pVfeVar->super, PERF, VFE_VAR, DERIVED_PRODUCT,
                                  &status, vfeVarEval_DERIVED_PRODUCT_exit);

    status = vfeVarEvalByIdx(pContext, pVar->varIdx0, &op0);
    if (status != FLCN_OK)
    {
        goto vfeVarEval_DERIVED_PRODUCT_exit;
    }

    status = vfeVarEvalByIdx(pContext, pVar->varIdx1, &op1);
    if (status != FLCN_OK)
    {
        goto vfeVarEval_DERIVED_PRODUCT_exit;
    }

    *pResult = lwF32Mul(op0, op1);

    // Finally at the end call parent implementation.
    status = vfeVarEval_DERIVED(pContext, pVfeVar, pResult);

vfeVarEval_DERIVED_PRODUCT_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @brief   VFE_VAR_DERIVED_PRODUCT implementation of
 *          @ref BoardObjVirtualTableDynamicCast()
 *
 * @copydoc BoardObjVirtualTableDynamicCast()
 */
static void *
s_vfeVarDynamicCast_DERIVED_PRODUCT
(
    BOARDOBJ   *pBoardObj,
    LwU8        requestedType
)
{
    void                    *pObject               = NULL;
    VFE_VAR_DERIVED_PRODUCT *pVfeVarDerivedProduct = (VFE_VAR_DERIVED_PRODUCT *)pBoardObj;

    if (BOARDOBJ_GET_TYPE(pBoardObj) !=
        LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, DERIVED_PRODUCT))
    {
        PMU_BREAKPOINT();
        goto s_vfeVarDynamicCast_DERIVED_PRODUCT_exit;
    }

    switch (requestedType)
    {
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, BASE):
        {
            VFE_VAR *pVfeVar = &pVfeVarDerivedProduct->super.super;
            pObject = (void *)pVfeVar;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, DERIVED):
        {
            VFE_VAR_DERIVED *pVfeVarDerived = &pVfeVarDerivedProduct->super;
            pObject = (void *)pVfeVarDerived;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, DERIVED_PRODUCT):
        {
            pObject = (void *)pVfeVarDerivedProduct;
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            break;
        }
    }

s_vfeVarDynamicCast_DERIVED_PRODUCT_exit:
    return pObject;
}
