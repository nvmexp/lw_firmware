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
 * @file    vfe_var_single.c
 * @brief   VFE_VAR_SINGLE class implementation.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/3x/vfe_var_single.h"
#include "pmu_objperf.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static LwBool   s_vfeVarTypeIsSingle(LwU8 varType)
    GCC_ATTRIB_SECTION("imem_perfVfe", "s_vfeVarTypeIsSingle");

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Constructor.
 * @memberof VFE_VAR_SINGLE
 * @public
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
vfeVarGrpIfaceModel10ObjSetImpl_SINGLE
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_VFE_VAR_SINGLE  *pDescVar = (RM_PMU_VFE_VAR_SINGLE *)pBoardObjDesc;
    VFE_VAR_SINGLE         *pVfeVar;
    FLCN_STATUS             status;

    PMU_ASSERT_OK_OR_GOTO(status,
        vfeVarGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc),
        vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_exit);

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVfeVar, *ppBoardObj, PERF, VFE_VAR, SINGLE,
                                  &status, vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_exit);

    // Set member variables.
    pVfeVar->overrideType  = pDescVar->overrideType;
    pVfeVar->overrideValue = lwF32MapFromU32(&pDescVar->overrideValue);

vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_exit:
    return status;
}

/*!
 * @copydoc VfeVarEval()
 * @memberof VFE_VAR_SINGLE
 * @public
 */
FLCN_STATUS
vfeVarEval_SINGLE
(
    VFE_CONTEXT    *pContext,
    VFE_VAR        *pVfeVar,
    LwF32          *pResult
)
{
    VFE_VAR_SINGLE *pVar;
    FLCN_STATUS     status = FLCN_OK;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVar, &pVfeVar->super, PERF, VFE_VAR, SINGLE,
                                  &status, vfeVarEval_SINGLE_exit);

    //
    // Use client provided value (must for non-sensed single variables).
    // Ignore status since it is OK to fail lookup (no client override).
    //
    (void)vfeVarSingleClientValueGet_SINGLE(pVar, pContext, pResult);

    // Apply RmCtrl override (if requested).
    switch (pVar->overrideType)
    {
        case LW2080_CTRL_PERF_VFE_VAR_SINGLE_OVERRIDE_TYPE_NONE:
            // No override requested.
            break;
        case LW2080_CTRL_PERF_VFE_VAR_SINGLE_OVERRIDE_TYPE_VALUE:
            *pResult = pVar->overrideValue;
            break;
        case LW2080_CTRL_PERF_VFE_VAR_SINGLE_OVERRIDE_TYPE_OFFSET:
            *pResult = lwF32Add(*pResult, pVar->overrideValue);
            break;
        case LW2080_CTRL_PERF_VFE_VAR_SINGLE_OVERRIDE_TYPE_SCALE:
            *pResult = lwF32Mul(*pResult, pVar->overrideValue);
            break;
        default:
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_STATE;
            break;
    }

    // Finally at the end call parent implementation.
    if (status == FLCN_OK)
    {
        status = vfeVarEval_SUPER(pContext, pVfeVar, pResult);
    }

vfeVarEval_SINGLE_exit:
    return status;
}

/*!
 * @copydoc VfeVarClientValuesLink()
 * @memberof VFE_VAR_SINGLE
 * @public
 */
FLCN_STATUS
vfeVarClientValuesLink
(
    VFE_CONTEXT                *pContext,
    RM_PMU_PERF_VFE_VAR_VALUE  *pValues,
    LwU8                        valCount,
    LwBool                      bLink
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8        i;

    if ((valCount > 0U) && (pValues == NULL))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto vfeVarClientValuesLink_exit;
    }

    for (i = 0; i < valCount; i++)
    {
        VFE_VAR      *pVar    = NULL;
        LwBoardObjIdx j;
        LwU8          varType = pValues[i].varType;

        if (bLink)
        {
            // Only VFE_VAR_SINGLE_<xyz> can be set by the caller.
            if (!s_vfeVarTypeIsSingle(varType))
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_ARGUMENT;
                goto vfeVarClientValuesLink_exit;
            }
        }

        // Ilwalidate caches of all dependent Variables and Equations.
        BOARDOBJGRP_ITERATOR_BEGIN(VFE_VAR, pVar, j, NULL)
        {
            VFE_VAR_SINGLE *pVarSingle;

            //
            // vfeVarSingleClientInputMatch_SINGLE() is expecting a SINGLE type
            // (or derived class). The dynamic cast will generate a breakpoint
            // if the VAR is not a SINGLE type. Since we're iterating over all
            // VAR types, we need this check prior to the casting.
            //
            if (!s_vfeVarTypeIsSingle(pVar->super.type))
            {
                continue;
            }

            BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVarSingle, &pVar->super, PERF, VFE_VAR, SINGLE,
                                            &status, vfeVarClientValuesLink_exit);

            if (vfeVarSingleClientInputMatch_SINGLE(pVarSingle, &pValues[i]) == FLCN_OK)
            {
                status = vfeVarDependentsCacheIlw(pContext, &pVarSingle->super);
                if (status != FLCN_OK)
                {
                    goto vfeVarClientValuesLink_exit;
                }
                break;
            }
        }
        BOARDOBJGRP_ITERATOR_END;
    }

    // Link/unlink client provided VFE Variable values with current context.
    if (bLink)
    {
        pContext->pClientVarValues   = pValues;
        pContext->clientVarValuesCnt = valCount;

        // Cache VARs so that EQUs can directly use cached values (saves stack).
        status = vfeVarsCacheAll(pContext);
        if (status != FLCN_OK)
        {
            goto vfeVarClientValuesLink_exit;
        }
    }
    else
    {
        pContext->pClientVarValues   = NULL;
        pContext->clientVarValuesCnt = 0;
    }

vfeVarClientValuesLink_exit:
    return status;
}

/*!
 * @copydoc VfeVarSingleClientValueGet()
 * @memberof VFE_VAR_SINGLE
 * @public
 */
FLCN_STATUS
vfeVarSingleClientValueGet_SINGLE
(
    VFE_VAR_SINGLE  *pVfeVar,
    VFE_CONTEXT     *pContext,
    LwF32           *pValue
)
{
    //
    // Returning OBJECT_NOT_FOUND to indicate that user didn't supply value for
    // the requested variable type. This is a fatal error unless we're within a
    // loop caching all (possible) veriables.
    //
    FLCN_STATUS status = FLCN_ERR_OBJECT_NOT_FOUND;
    LwU8        i;

    if ((pContext->clientVarValuesCnt > 0U) &&
        (pContext->pClientVarValues == NULL))
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto vfeVarClientValueGet_exit;
    }

    for (i = 0; i < pContext->clientVarValuesCnt; i++)
    {
        status = vfeVarSingleClientInputMatch_SINGLE(pVfeVar,
            &pContext->pClientVarValues[i]);
        if (status == FLCN_OK)
        {
            break;
        }
    }

    if ((pValue != NULL) && (status == FLCN_OK))
    {
        switch (BOARDOBJ_GET_TYPE(&pVfeVar->super))
        {
            case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_FREQUENCY:
            {
                *pValue = lwF32ColwertFromU32(pContext->pClientVarValues[i].frequency.varValue);
                break;
            }
            case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_VOLTAGE:
            {
                *pValue = lwF32ColwertFromU32(pContext->pClientVarValues[i].voltage.varValue);
                break;
            }
            case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_SENSED_TEMP:
            {
                *pValue = vfeVarInputClientValueGet_SINGLE_SENSED_TEMP(pVfeVar,
                        &pContext->pClientVarValues[i], pValue);
                break;
            }
            case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_CALLER_SPECIFIED:
            {
                if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_VAR_35))
                {
                    status = vfeVarInputClientValueGet_SINGLE_CALLER_SPECIFIED(pVfeVar,
                        &pContext->pClientVarValues[i], pValue);
                }
                break;
            }
            default:
            {
                status = FLCN_ERR_NOT_SUPPORTED;
                PMU_BREAKPOINT();
                break;
            }
        }
    }

vfeVarClientValueGet_exit:
    return status;
}

/*!
 * @copydoc VfeVarEval()
 * @memberof VFE_VAR_SINGLE
 * @protected
 */
FLCN_STATUS
vfeVarEval_SINGLE_CALLER
(
    VFE_CONTEXT    *pContext,
    VFE_VAR        *pVfeVar,
    LwF32          *pResult
)
{
    VFE_VAR_SINGLE *pVar;
    FLCN_STATUS     status;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVar, &pVfeVar->super, PERF, VFE_VAR, SINGLE,
                                    &status, vfeVarEval_SINGLE_CALLER_exit);

    // Must be provided by the caller of this interface, fail otherwise.
    status = vfeVarSingleClientValueGet_SINGLE(pVar, pContext, NULL);
    if (status != FLCN_OK)
    {
        // Not every variable can be evaluated, so skip BP while caching.
        if (!pContext->bCachingInProgress)
        {
            PMU_BREAKPOINT();
        }
        goto vfeVarEval_SINGLE_CALLER_exit;
    }

    *pResult = 0.0f;

    // Finally at the end call parent implementation.
    status = vfeVarEval_SINGLE(pContext, pVfeVar, pResult);

vfeVarEval_SINGLE_CALLER_exit:
    return status;
}

/*!
 * @copydoc VfeVarSingleClientInputMatch()
 * @memberof VFE_VAR_SINGLE
 * @protected
 */
FLCN_STATUS
vfeVarSingleClientInputMatch_SINGLE
(
    VFE_VAR_SINGLE            *pVfeVar,
    RM_PMU_PERF_VFE_VAR_VALUE *pClientVarValue
)
{
    FLCN_STATUS status = FLCN_ERR_OBJECT_NOT_FOUND;

    if (pClientVarValue->varType == BOARDOBJ_GET_TYPE(&pVfeVar->super))
    {
        switch (BOARDOBJ_GET_TYPE(&pVfeVar->super))
        {
            case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_FREQUENCY:
            {
                status = vfeVarSingleClientInputMatch_SINGLE_FREQUENCY(pVfeVar, pClientVarValue);
                break;
            }
            case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_VOLTAGE:
            {
                status = FLCN_OK;
                break;
            }
            case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_SENSED_TEMP:
            {
                status = FLCN_OK;
                break;
            }
            case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_CALLER_SPECIFIED:
            {
                if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_VAR_35))
                {
                    status = vfeVarSingleClientInputMatch_SINGLE_CALLER_SPECIFIED(pVfeVar, pClientVarValue);
                }
                break;
            }
            default:
            {
                status = FLCN_ERR_OBJECT_NOT_FOUND;
                break;
            }
        }
    }

    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 *
 * @brief Performs check if VFE Variable is child of VFE_VAR_SINGLE.
 * @memberof VFE_VAR_SINGLE
 * @private
 *
 * @note    This is temporary interface until PMU BOARDOBJ code generically
 *          support implements() interface.
 *          Additionally below code can possibly be reduced by using bit-mask.
 */
static LwBool
s_vfeVarTypeIsSingle
(
    LwU8    varType
)
{
    LwBool bIsSingle = LW_FALSE;

    switch (varType)
    {
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_FREQUENCY:
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_SENSED_FUSE:
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_SENSED_TEMP:
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_VOLTAGE:
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_CALLER_SPECIFIED:
            bIsSingle = LW_TRUE;
            break;
        default:
            bIsSingle = LW_FALSE;
            break;
    }

    return bIsSingle;
}
