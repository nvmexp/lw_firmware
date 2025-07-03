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
 * @file    vfe_var_single_caller_specified.c
 * @brief   VFE_VAR_SINGLE_CALLER_SPECIFIED class implementation.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objperf.h"
#include "perf/3x/vfe_var_single.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjVirtualTableDynamicCast (s_vfeVarDynamicCast_SINGLE_CALLER_SPECIFIED)
    GCC_ATTRIB_SECTION("imem_perf", "s_vfeVarDynamicCast_SINGLE_CALLER_SPECIFIED")
    GCC_ATTRIB_USED();

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/*!
 * Virtual table for VFE_VAR_SINGLE_CALLER_SPECIFIED object interfaces.
 */
BOARDOBJ_VIRTUAL_TABLE VfeVarSingleCallerSpecifiedVirtualTable =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_DYNAMIC_CAST(s_vfeVarDynamicCast_SINGLE_CALLER_SPECIFIED)
};

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Constructor.
 * @memberof VFE_VAR_SINGLE_CALLER_SPECIFIED
 * @public
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_CALLER_SPECIFIED
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_VFE_VAR_SINGLE_CALLER_SPECIFIED *pDescVar =
        (RM_PMU_VFE_VAR_SINGLE_CALLER_SPECIFIED *)pBoardObjDesc;
    VFE_VAR_SINGLE_CALLER_SPECIFIED *pVfeVar;
    FLCN_STATUS                      status;

    PMU_ASSERT_OK_OR_GOTO(status,
        vfeVarGrpIfaceModel10ObjSetImpl_SINGLE(pModel10, ppBoardObj, size, pBoardObjDesc),
        vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_CALLER_SPECIFIED_exit);

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVfeVar, *ppBoardObj, PERF, VFE_VAR, SINGLE_CALLER_SPECIFIED,
                                  &status, vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_CALLER_SPECIFIED_exit);

    // Set member variables.
    pVfeVar->uniqueId = pDescVar->uniqueId;

vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_CALLER_SPECIFIED_exit:
    return status;
}

/*!
 * @copydoc VfeVarSingleClientInputMatch()
 * @memberof VFE_VAR_SINGLE_CALLER_SPECIFIED
 * @public
 */
FLCN_STATUS
vfeVarSingleClientInputMatch_SINGLE_CALLER_SPECIFIED
(
    VFE_VAR_SINGLE            *pVfeVar,
    RM_PMU_PERF_VFE_VAR_VALUE *pClientVarValue
)
{
    VFE_VAR_SINGLE_CALLER_SPECIFIED *pVar;
    FLCN_STATUS                      status;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVar, &pVfeVar->super.super, PERF, VFE_VAR, SINGLE_CALLER_SPECIFIED,
                                  &status, vfeVarSingleClientInputMatch_SINGLE_CALLER_SPECIFIED_exit);

    if (pClientVarValue->varType !=
        LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, SINGLE_CALLER_SPECIFIED))
    {
        PMU_BREAKPOINT();
        goto vfeVarSingleClientInputMatch_SINGLE_CALLER_SPECIFIED_exit;
    }

    // Match against the Unique ID
    if (pClientVarValue->callerSpecified.uniqueId == pVar->uniqueId)
    {
        status = FLCN_OK;
    }
    else
    {
        status = FLCN_ERR_OBJECT_NOT_FOUND;
    }

vfeVarSingleClientInputMatch_SINGLE_CALLER_SPECIFIED_exit:
    return status;
}

/*!
 * @copydoc VfeVarInputClientValueGet()
 * @memberof VFE_VAR_SINGLE_CALLER_SPECIFIED
 * @public
 */
FLCN_STATUS
vfeVarInputClientValueGet_SINGLE_CALLER_SPECIFIED
(
    VFE_VAR_SINGLE             *pVfeVar,
    RM_PMU_PERF_VFE_VAR_VALUE  *pClientVarValue,
    LwF32                      *pValue
)
{
    FLCN_STATUS status = FLCN_ERR_OBJECT_NOT_FOUND;

    if (pClientVarValue->varType !=
        LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, SINGLE_CALLER_SPECIFIED))
    {
        PMU_BREAKPOINT();
        goto vfeVarInputClientValueGet_SINGLE_CALLER_SPECIFIED_exit;
    }

    switch (pClientVarValue->callerSpecified.uniqueId)
    {
        case LW2080_CTRL_PERF_VFE_VAR_CALLER_SPECIFIED_UID_WORK_TYPE:
        case LW2080_CTRL_PERF_VFE_VAR_CALLER_SPECIFIED_UID_UTIL_RATIO:
            //
            // For UID, work type and util ratio the read is FXP20.12 equivalent.
            // Scale back wrt floating part (2^12) from the above FXP20.12 read
            //
            *pValue = LW_TYPES_UFXP_X_Y_TO_F32(20, 12, pClientVarValue->callerSpecified.varValue);
            status = FLCN_OK;
            break;
        default:
            status = FLCN_ERR_OBJECT_NOT_FOUND;
            break;
    }

vfeVarInputClientValueGet_SINGLE_CALLER_SPECIFIED_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @brief   VFE_VAR_SINGLE_CALLER_SPECIFIED implementation of
 *          @ref BoardObjVirtualTableDynamicCast()
 *
 * @copydoc BoardObjVirtualTableDynamicCast()
 */
static void *
s_vfeVarDynamicCast_SINGLE_CALLER_SPECIFIED
(
    BOARDOBJ   *pBoardObj,
    LwU8        requestedType
)
{
    void                            *pObject                = NULL;
    VFE_VAR_SINGLE_CALLER_SPECIFIED *pVfeVarCallerSpecified =
        (VFE_VAR_SINGLE_CALLER_SPECIFIED *)pBoardObj;

    if (BOARDOBJ_GET_TYPE(pBoardObj) !=
        LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, SINGLE_CALLER_SPECIFIED))
    {
        PMU_BREAKPOINT();
        goto s_vfeVarDynamicCast_SINGLE_CALLER_SPECIFIED_exit;
    }

    switch (requestedType)
    {
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, BASE):
        {
            VFE_VAR *pVfeVar = &pVfeVarCallerSpecified->super.super;
            pObject = (void *)pVfeVar;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, SINGLE):
        {
            VFE_VAR_SINGLE *pVfeVarSingle = &pVfeVarCallerSpecified->super;
            pObject = (void *)pVfeVarSingle;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, SINGLE_CALLER_SPECIFIED):
        {
            pObject = (void *)pVfeVarCallerSpecified;
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            break;
        }
    }

s_vfeVarDynamicCast_SINGLE_CALLER_SPECIFIED_exit:
    return pObject;
}
