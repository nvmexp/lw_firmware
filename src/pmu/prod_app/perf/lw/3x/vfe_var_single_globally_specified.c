/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    vfe_var_single_globally_specified.c
 * @brief   VFE_VAR_SINGLE_GLOBALLY_SPECIFIED class implementation.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objperf.h"
#include "perf/3x/vfe_var_single.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjVirtualTableDynamicCast (s_vfeVarDynamicCast_SINGLE_GLOBALLY_SPECIFIED)
    GCC_ATTRIB_SECTION("imem_perf", "s_vfeVarDynamicCast_SINGLE_GLOBALLY_SPECIFIED")
    GCC_ATTRIB_USED();

static FLCN_STATUS s_vfeVarSingleGloballySpecifiedValueLwrrSet (VFE_VAR_SINGLE_GLOBALLY_SPECIFIED *pVarGloballySpecified)
    GCC_ATTRIB_SECTION("imem_perf", "s_vfeVarSingleGloballySpecifiedValueLwrrSet");
static FLCN_STATUS s_vfeVarSingleGloballySpecifiedValueSet(VFE_VAR_SINGLE_GLOBALLY_SPECIFIED *pVarGloballySpecified, RM_PMU_PERF_VFE_VAR_VALUE *pValues)
    GCC_ATTRIB_SECTION("imem_perfVfe", "s_vfeVarSingleGloballySpecifiedValueSet");

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/*!
 * Virtual table for VFE_VAR_SINGLE_GLOBALLY_SPECIFIED object interfaces.
 */
BOARDOBJ_VIRTUAL_TABLE VfeVarSingleGloballySpecifiedVirtualTable =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_DYNAMIC_CAST(s_vfeVarDynamicCast_SINGLE_GLOBALLY_SPECIFIED)
};

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Constructor.
 * @memberof VFE_VAR_SINGLE_GLOBALLY_SPECIFIED
 * @public
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_GLOBALLY_SPECIFIED
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_VFE_VAR_SINGLE_GLOBALLY_SPECIFIED   *pDescVar =
        (RM_PMU_VFE_VAR_SINGLE_GLOBALLY_SPECIFIED *)pBoardObjDesc;
    VFE_VAR_SINGLE_GLOBALLY_SPECIFIED          *pVfeVar;
    LwBool                                      bFirstTime = (*ppBoardObj == NULL);
    FLCN_STATUS                                 status;

    PMU_ASSERT_OK_OR_GOTO(status,
        vfeVarGrpIfaceModel10ObjSetImpl_SINGLE(pModel10, ppBoardObj, size, pBoardObjDesc),
        vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_GLOBALLY_SPECIFIED_exit);

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVfeVar, *ppBoardObj, PERF, VFE_VAR, SINGLE_GLOBALLY_SPECIFIED,
                                  &status, vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_GLOBALLY_SPECIFIED_exit);

    // Set member variables.
    pVfeVar->uniqueId    = pDescVar->uniqueId;
    pVfeVar->numFracBits = pDescVar->numFracBits;
    pVfeVar->valDefault  = pDescVar->valDefault;
    pVfeVar->valOverride = pDescVar->valOverride;

    // On first construct init the globally specified value to default value.
    if (bFirstTime)
    {
        pVfeVar->valSpecified = pDescVar->valDefault;
    }

    // Init to default.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_vfeVarSingleGloballySpecifiedValueLwrrSet(pVfeVar),
        vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_GLOBALLY_SPECIFIED_exit);

vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_GLOBALLY_SPECIFIED_exit:
    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
vfeVarIfaceModel10GetStatus_SINGLE_GLOBALLY_SPECIFIED
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    VFE_VAR_SINGLE_GLOBALLY_SPECIFIED *pVar;
    RM_PMU_PERF_VFE_VAR_SINGLE_GLOBALLY_SPECIFIED_GET_STATUS *pGetStatus =
        (RM_PMU_PERF_VFE_VAR_SINGLE_GLOBALLY_SPECIFIED_GET_STATUS *)pPayload;
    FLCN_STATUS status = FLCN_OK;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVar, pBoardObj, PERF, VFE_VAR, SINGLE_GLOBALLY_SPECIFIED,
                                  &status, vfeVarIfaceModel10GetStatus_SINGLE_GLOBALLY_SPECIFIED_exit);

    PMU_HALT_OK_OR_GOTO(status,
        vfeVarIfaceModel10GetStatus_SINGLE(pModel10, pPayload),
        vfeVarIfaceModel10GetStatus_SINGLE_GLOBALLY_SPECIFIED_exit);

    pGetStatus->valLwrr  = pVar->valLwrr;

vfeVarIfaceModel10GetStatus_SINGLE_GLOBALLY_SPECIFIED_exit:
    return status;
}

/*!
 * @copydoc VfeVarSingleGloballySpecifiedValueSet
 */
FLCN_STATUS
vfeVarSingleGloballySpecifiedValueSet
(
    RM_PMU_PERF_VFE_VAR_VALUE  *pValues,
    LwU8                        valCount,
    LwBool                     *pBTriggerIlw
)
{
    VFE_VAR        *pVar        = NULL;
    LwBoardObjIdx   j;
    LwU8            i;
    FLCN_STATUS     status      = FLCN_OK;

    // Sanity check
    if ((valCount > 0U) && (pValues == NULL))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto vfeVarSingleGloballySpecifiedValueSet_exit;
    }

    //
    // Ensure that ONLY PERF task can call this interface. In future,
    // we will likely remove VFE_CONTEXT -> VFE_VAR pointer mappings
    // everywhere, once we can deprecate PERF_CF use of VFE.
    //
    PMU_HALT_COND(osTaskIdGet() == RM_PMU_TASK_ID_PERF);

    // Iterate over all client specified variables.
    for (i = 0; i < valCount; i++)
    {
        // Sanity checks.
        if (pValues[i].varType != LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_GLOBALLY_SPECIFIED)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto vfeVarSingleGloballySpecifiedValueSet_exit;
        }

        // Find the matching variable.
        BOARDOBJGRP_ITERATOR_BEGIN(VFE_VAR, pVar, j, NULL)
        {
            VFE_VAR_SINGLE_GLOBALLY_SPECIFIED  *pVarGloballySpecified;
            LwU32                               contextID;

            //
            // The dynamic cast will generate a breakpoint if the VAR is not a
            // _SINGLE_GLOBALLY_SPECIFIED type. Since we're iterating over all
            // VAR types, we need this check prior to the casting.
            //
            if (pVar->super.type != LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_GLOBALLY_SPECIFIED)
            {
                continue;
            }

            BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVarGloballySpecified, &pVar->super,
                PERF, VFE_VAR, SINGLE_GLOBALLY_SPECIFIED,
                &status, vfeVarSingleGloballySpecifiedValueSet_exit);

            // Trigger update for active value.
            status = s_vfeVarSingleGloballySpecifiedValueSet(pVarGloballySpecified, &pValues[i]);
            if (status == FLCN_ERR_OBJECT_NOT_FOUND)
            {
                continue;
            }
            else if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto vfeVarSingleGloballySpecifiedValueSet_exit;
            }

            for (contextID = 0; contextID < (LwU32)VFE_CONTEXT_ID__COUNT; contextID++)
            {
                VFE_CONTEXT *pContext = VFE_CONTEXT_GET(&Perf, contextID);

                status = vfeVarDependentsCacheIlw(pContext, pVar);
                if (status != FLCN_OK)
                {
                    goto vfeVarSingleGloballySpecifiedValueSet_exit;
                }
            }

            // Trigger ilwalidation
            (*pBTriggerIlw) = LW_TRUE;

            // Break out and update for next client specified variable if any.
            break;
        }
        BOARDOBJGRP_ITERATOR_END;
    }

vfeVarSingleGloballySpecifiedValueSet_exit:
    return status;
}

/*!
 * @copydoc VfeVarEval()
 * @memberof VFE_VAR_SINGLE_GLOBALLY_SPECIFIED
 * @public
 */
FLCN_STATUS
vfeVarEval_SINGLE_GLOBALLY_SPECIFIED
(
    VFE_CONTEXT    *pContext,
    VFE_VAR        *pVfeVar,
    LwF32          *pResult
)
{
    VFE_VAR_SINGLE_GLOBALLY_SPECIFIED  *pVar;
    FLCN_STATUS                         status;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVar, &pVfeVar->super, PERF, VFE_VAR, SINGLE_GLOBALLY_SPECIFIED,
                                  &status, vfeVarEval_SINGLE_GLOBALLY_SPECIFIED_exit);

    // Value in PMU.
    *pResult = LW_TYPES_SFXP_X_Y_TO_F32((32 - pVar->numFracBits), pVar->numFracBits, pVar->valLwrr);

    // Finally at the end call parent implementation.
    status = vfeVarEval_SINGLE(pContext, pVfeVar, pResult);

vfeVarEval_SINGLE_GLOBALLY_SPECIFIED_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @copydoc VfeVarSingleGloballySpecifiedValueSet
 */
static FLCN_STATUS
s_vfeVarSingleGloballySpecifiedValueSet
(
    VFE_VAR_SINGLE_GLOBALLY_SPECIFIED  *pVarGloballySpecified,
    RM_PMU_PERF_VFE_VAR_VALUE          *pValues
)
{
    FLCN_STATUS     status;

    // Sanity check
    if (pValues == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_vfeVarSingleGloballySpecifiedValueSet_exit;
    }

    // Find the variable with matching unique id
    if (pValues->globallySpecified.uniqueId != pVarGloballySpecified->uniqueId)
    {
        // This is expected and not an error.
        status = FLCN_ERR_OBJECT_NOT_FOUND;
        goto s_vfeVarSingleGloballySpecifiedValueSet_exit;
    }

    // Update the globally variable value.
    pVarGloballySpecified->valSpecified = pValues->globallySpecified.varValue;

    // Trigger update for active value.
    PMU_HALT_OK_OR_GOTO(status,
        s_vfeVarSingleGloballySpecifiedValueLwrrSet(pVarGloballySpecified),
        s_vfeVarSingleGloballySpecifiedValueSet_exit);

s_vfeVarSingleGloballySpecifiedValueSet_exit:
    return status;
}

/*!
 * @brief   Helper interface to update the @ref valLwrr using the current set
 *          value of the @ref valOverride and @ref valSpecified
 *
 * @copydoc BoardObjVirtualTableDynamicCast()
 */
static FLCN_STATUS
s_vfeVarSingleGloballySpecifiedValueLwrrSet
(
    VFE_VAR_SINGLE_GLOBALLY_SPECIFIED *pVarGloballySpecified
)
{
    pVarGloballySpecified->valLwrr =
        ((pVarGloballySpecified->valOverride != LW2080_CTRL_PERF_VFE_VAR_SINGLE_GLOBALLY_SPECIFIED_VAL_OVERRIDE_ILWALID) ?
            pVarGloballySpecified->valOverride :
            pVarGloballySpecified->valSpecified);

    return FLCN_OK;
}

/*!
 * @brief   VFE_VAR_SINGLE_GLOBALLY_SPECIFIED implementation of
 *          @ref BoardObjVirtualTableDynamicCast()
 *
 * @copydoc BoardObjVirtualTableDynamicCast()
 */
static void *
s_vfeVarDynamicCast_SINGLE_GLOBALLY_SPECIFIED
(
    BOARDOBJ   *pBoardObj,
    LwU8        requestedType
)
{
    void                                *pObject                  = NULL;
    VFE_VAR_SINGLE_GLOBALLY_SPECIFIED   *pVfeVarGloballySpecified =
        (VFE_VAR_SINGLE_GLOBALLY_SPECIFIED *)pBoardObj;

    if (BOARDOBJ_GET_TYPE(pBoardObj) !=
        LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, SINGLE_GLOBALLY_SPECIFIED))
    {
        PMU_BREAKPOINT();
        goto s_vfeVarDynamicCast_SINGLE_GLOBALLY_SPECIFIED_exit;
    }

    switch (requestedType)
    {
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, BASE):
        {
            VFE_VAR *pVfeVar = &pVfeVarGloballySpecified->super.super;
            pObject = (void *)pVfeVar;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, SINGLE):
        {
            VFE_VAR_SINGLE *pVfeVarSingle = &pVfeVarGloballySpecified->super;
            pObject = (void *)pVfeVarSingle;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, SINGLE_GLOBALLY_SPECIFIED):
        {
            pObject = (void *)pVfeVarGloballySpecified;
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            break;
        }
    }

s_vfeVarDynamicCast_SINGLE_GLOBALLY_SPECIFIED_exit:
    return pObject;
}
