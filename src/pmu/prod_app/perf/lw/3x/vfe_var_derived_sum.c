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
 * @file    vfe_var_derived_sum.c
 * @brief   VFE_VAR_DERIVED_SUM class implementation.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objperf.h"
#include "perf/3x/vfe_var_derived_sum.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjVirtualTableDynamicCast (s_vfeVarDynamicCast_DERIVED_SUM)
    GCC_ATTRIB_SECTION("imem_perf", "s_vfeVarDynamicCast_DERIVED_SUM")
    GCC_ATTRIB_USED();

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/*!
 * Virtual table for VFE_VAR_DERIVED_SUM object interfaces.
 */
BOARDOBJ_VIRTUAL_TABLE VfeVarDerivedSumVirtualTable =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_DYNAMIC_CAST(s_vfeVarDynamicCast_DERIVED_SUM)
};

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @memberof VFE_VAR_DERIVED_SUM
 *
 * @public
 *
 * @brief Constructor.
 *
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
vfeVarGrpIfaceModel10ObjSetImpl_DERIVED_SUM
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_VFE_VAR_DERIVED_SUM *pDescVar =
        (RM_PMU_VFE_VAR_DERIVED_SUM *)pBoardObjDesc;
    VFE_VAR_DERIVED_SUM        *pVfeVar;
    FLCN_STATUS                 status;

    PMU_ASSERT_OK_OR_GOTO(status,
        vfeVarGrpIfaceModel10ObjSetImpl_DERIVED(pModel10, ppBoardObj, size, pBoardObjDesc),
        vfeVarGrpIfaceModel10ObjSetImpl_DERIVED_SUM_exit);

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVfeVar, *ppBoardObj, PERF, VFE_VAR, DERIVED_SUM,
                                  &status, vfeVarGrpIfaceModel10ObjSetImpl_DERIVED_SUM_exit);

    // Set member variables.
    pVfeVar->varIdx0 = pDescVar->varIdx0;
    pVfeVar->varIdx1 = pDescVar->varIdx1;

vfeVarGrpIfaceModel10ObjSetImpl_DERIVED_SUM_exit:
    return status;
}

/*!
 * @memberof VFE_VAR_DERIVED_SUM
 *
 * @public
 *
 * @copydoc VfeVarEval()
 */
FLCN_STATUS
vfeVarEval_DERIVED_SUM
(
    VFE_CONTEXT    *pContext,
    VFE_VAR        *pVfeVar,
    LwF32          *pResult
)
{
    VFE_VAR_DERIVED_SUM    *pVar;
    LwF32                   op0;
    LwF32                   op1;
    FLCN_STATUS             status;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVar, &pVfeVar->super, PERF, VFE_VAR, DERIVED_SUM,
                                  &status, vfeVarEval_DERIVED_SUM_exit);

    status = vfeVarEvalByIdx(pContext, pVar->varIdx0, &op0);
    if (status != FLCN_OK)
    {
        goto vfeVarEval_DERIVED_SUM_exit;
    }

    status = vfeVarEvalByIdx(pContext, pVar->varIdx1, &op1);
    if (status != FLCN_OK)
    {
        goto vfeVarEval_DERIVED_SUM_exit;
    }

    *pResult = lwF32Add(op0, op1);

    // Finally at the end call parent implementation.
    status = vfeVarEval_DERIVED(pContext, pVfeVar, pResult);

vfeVarEval_DERIVED_SUM_exit:
    return status;

}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @brief   VFE_VAR_DERIVED_SUM implementation of
 *          @ref BoardObjVirtualTableDynamicCast()
 *
 * @copydoc BoardObjVirtualTableDynamicCast()
 */
static void *
s_vfeVarDynamicCast_DERIVED_SUM
(
    BOARDOBJ   *pBoardObj,
    LwU8        requestedType
)
{
    void                *pObject           = NULL;
    VFE_VAR_DERIVED_SUM *pVfeVarDerivedSum = (VFE_VAR_DERIVED_SUM *)pBoardObj;

    if (BOARDOBJ_GET_TYPE(pBoardObj) !=
        LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, DERIVED_SUM))
    {
        PMU_BREAKPOINT();
        goto s_vfeVarDynamicCast_DERIVED_SUM_exit;
    }

    switch (requestedType)
    {
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, BASE):
        {
            VFE_VAR *pVfeVar = &pVfeVarDerivedSum->super.super;
            pObject = (void *)pVfeVar;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, DERIVED):
        {
            VFE_VAR_DERIVED *pVfeVarDerived = &pVfeVarDerivedSum->super;
            pObject = (void *)pVfeVarDerived;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, DERIVED_SUM):
        {
            pObject = (void *)pVfeVarDerivedSum;
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            break;
        }
    }

s_vfeVarDynamicCast_DERIVED_SUM_exit:
    return pObject;
}
