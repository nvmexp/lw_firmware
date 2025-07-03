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
 * @file    vfe_equ_minmax.c
 * @brief   VFE_EQU_MINMAX class implementation.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objperf.h"
#include "perf/3x/vfe_equ_minmax.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjVirtualTableDynamicCast (s_vfeEquDynamicCast_MINMAX)
    GCC_ATTRIB_SECTION("imem_perf", "s_vfeEquDynamicCast_MINMAX")
    GCC_ATTRIB_USED();

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/*!
 * Virtual table for VFE_EQU_MINMAX object interfaces.
 */
BOARDOBJ_VIRTUAL_TABLE VfeEquMinMaxVirtualTable =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_DYNAMIC_CAST(s_vfeEquDynamicCast_MINMAX)
};

/* ------------------------ Inline Functions -------------------------------- */
/*!
 * @brief Constructor validator helper.
 *
 * @memberof VFE_EQU_MINMAX
 * @private
 *
 * Performs validation of the parameters passed to the object constructor.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
s_vfeEquConstructValidateHelper_MINMAX
(
    BOARDOBJGRP            *pBObjGrp,
    VFE_EQU_MINMAX         *pVfeEqu,
    RM_PMU_VFE_EQU_MINMAX  *pDescEqu
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
 * @memberof VFE_EQU_MINMAX
 * @public
 */
FLCN_STATUS
vfeEquGrpIfaceModel10ObjSetImpl_MINMAX
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_VFE_EQU_MINMAX  *pDescEqu =
        (RM_PMU_VFE_EQU_MINMAX *)pBoardObjDesc;
    VFE_EQU_MINMAX         *pVfeEqu;
    FLCN_STATUS             status;

    PMU_ASSERT_OK_OR_GOTO(status,
        vfeEquGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc),
        vfeEquGrpIfaceModel10ObjSetImpl_MINMAX_exit);

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(
        pVfeEqu, *ppBoardObj, PERF, VFE_EQU, MINMAX, &status,
        vfeEquGrpIfaceModel10ObjSetImpl_MINMAX_exit);

    if (!PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJ_CONSTRUCT_VALIDATE))
    {
        PMU_ASSERT_OK_OR_GOTO(status, 
            s_vfeEquConstructValidateHelper_MINMAX(pBObjGrp, pVfeEqu, pDescEqu),
            vfeEquGrpIfaceModel10ObjSetImpl_MINMAX_exit);
    }

    // Set member variables.
    pVfeEqu->bMax    = pDescEqu->bMax;
    VFE_EQU_IDX_COPY_RM_TO_PMU(pVfeEqu->equIdx0, pDescEqu->equIdx0);
    VFE_EQU_IDX_COPY_RM_TO_PMU(pVfeEqu->equIdx1, pDescEqu->equIdx1);

vfeEquGrpIfaceModel10ObjSetImpl_MINMAX_exit:
    return status;
}

/*!
 * @brief Constructor validator.
 *
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 * @memberof VFE_EQU_MINMAX
 * @public
 */
FLCN_STATUS
vfeEquGrpIfaceModel10ObjSetValidate_MINMAX
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_VFE_EQU_MINMAX  *pDescEqu =
        (RM_PMU_VFE_EQU_MINMAX *)pBoardObjDesc;
    VFE_EQU_MINMAX         *pVfeEqu;
    FLCN_STATUS             status;

    PMU_ASSERT_OK_OR_GOTO(status,
        vfeEquGrpIfaceModel10ObjSetValidate_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc),
        vfeEquGrpIfaceModel10ObjSetValidate_MINMAX_exit);

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(
        pVfeEqu, *ppBoardObj, PERF, VFE_EQU, MINMAX, &status,
        vfeEquGrpIfaceModel10ObjSetValidate_MINMAX_exit);

    PMU_ASSERT_OK_OR_GOTO(status, 
        s_vfeEquConstructValidateHelper_MINMAX(pBObjGrp, pVfeEqu, pDescEqu),
        vfeEquGrpIfaceModel10ObjSetValidate_MINMAX_exit);

vfeEquGrpIfaceModel10ObjSetValidate_MINMAX_exit:
    return status;
}

/*!
 * @brief Evaluates a min/max equation node.
 *
 * @copydetails VfeEquEvalNode()
 * @memberof VFE_EQU_MINMAX
 * @public
 */
FLCN_STATUS
vfeEquEvalNode_MINMAX
(
    VFE_CONTEXT    *pContext,
    VFE_EQU        *pVfeEqu,
    LwF32          *pResult
)
{
    VFE_EQU_MINMAX *pVfeEquMinmax;
    FLCN_STATUS     status;
    LwF32           result0;
    LwF32           result1;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVfeEquMinmax, &pVfeEqu->super, PERF, VFE_EQU, MINMAX,
                                  &status, vfeEquEvalNode_MINMAX_exit);

    PMU_HALT_OK_OR_GOTO(status,
        vfeEquEvalList(pContext, pVfeEquMinmax->equIdx0, &result0),
        vfeEquEvalNode_MINMAX_exit);

    PMU_HALT_OK_OR_GOTO(status,
        vfeEquEvalList(pContext, pVfeEquMinmax->equIdx1, &result1),
        vfeEquEvalNode_MINMAX_exit);

    // Copy out only on success.
    if (lwF32CmpGT(result0, result1))
    {
        if (pVfeEquMinmax->bMax)
        {
            *pResult = result0;
        }
        else
        {
            *pResult = result1;
        }
    }
    else
    {
        if (pVfeEquMinmax->bMax)
        {
            *pResult = result1;
        }
        else
        {
            *pResult = result0;
        }
    }

    // Finally at the end call parent implementation.
    status = vfeEquEvalNode_SUPER(pContext, pVfeEqu, pResult);

vfeEquEvalNode_MINMAX_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @brief   VFE_EQU_MINMAX inmplemenetation of
 *          @ref BoardObjVirtualTableDynamicCast()
 *
 * @copydoc BoardObjVirtualTableDynamicCast()
 */
static void *
s_vfeEquDynamicCast_MINMAX
(
    BOARDOBJ   *pBoardObj,
    LwU8        requestedType
)
{
    void           *pObject       = NULL;
    VFE_EQU_MINMAX *pVfeEquMinmax = (VFE_EQU_MINMAX *)pBoardObj;

    if (BOARDOBJ_GET_TYPE(pBoardObj) !=
        LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_EQU, MINMAX))
    {
        PMU_BREAKPOINT();
        goto s_vfeEquDynamicCast_MINMAX_exit;
    }

    switch (requestedType)
    {
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_EQU, BASE):
        {
            VFE_EQU *pVfeEqu = &pVfeEquMinmax->super;
            pObject = (void *)pVfeEqu;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_EQU, MINMAX):
        {
            pObject = (void *)pVfeEquMinmax;
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            break;
        }
    }

s_vfeEquDynamicCast_MINMAX_exit:
    return pObject;
}
