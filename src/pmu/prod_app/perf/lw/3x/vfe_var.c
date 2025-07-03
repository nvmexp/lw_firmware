
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
 * @file    vfe_var.c
 * @brief   VFE_VAR class implementation
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

#include "dmemovl.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/3x/vfe_var.h"
#include "pmu_objperf.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static FLCN_STATUS  s_vfeVarBoardObjGrpSetHelper(PMU_DMEM_BUFFER *pBuffer, LwBool *pbFirstConstruct)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "s_vfeVarBoardObjGrpSetHelper")
    GCC_ATTRIB_NOINLINE();

static BoardObjGrpIfaceModel10SetHeader              (s_vfeVarIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "s_vfeVarIfaceModel10SetHeader");
static BoardObjGrpIfaceModel10SetEntry               (s_vfeVarIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "s_vfeVarIfaceModel10SetEntry");
static BoardObjIfaceModel10GetStatus                           (s_vfeVarIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "s_vfeVarIfaceModel10GetStatus");

static VfeVarSample (s_vfeVarSample);
static VfeVarSample (s_vfeVarSample_SUPER);
static VfeVarEval   (s_vfeVarEval);

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/*!
 * @brief   Array of all vtables pertaining to different PSTATE object types.
 */
BOARDOBJ_VIRTUAL_TABLE *VfeVarVirtualTables[LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, MAX)] =
{
    [LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, BASE)                   ] = NULL,
    [LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, DERIVED)                ] = NULL,
    [LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, DERIVED_PRODUCT)        ] = PERF_VFE_VAR_DERIVED_PRODUCT_VTABLE(),
    [LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, DERIVED_SUM)            ] = PERF_VFE_VAR_DERIVED_SUM_VTABLE(),
    [LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, SINGLE)                 ] = NULL,
    [LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, SINGLE_FREQUENCY)       ] = PERF_VFE_VAR_SINGLE_FREQUENCY_VTABLE(),
    [LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, SINGLE_SENSED)          ] = NULL,
    [LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, SINGLE_SENSED_FUSE)     ] = PERF_VFE_VAR_SINGLE_SENSED_FUSE_VTABLE(),
    [LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, SINGLE_SENSED_TEMP)     ] = PERF_VFE_VAR_SINGLE_SENSED_TEMP_VTABLE(),
    [LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, SINGLE_VOLTAGE)         ] = PERF_VFE_VAR_SINGLE_VOLTAGE_VTABLE(),
    [LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_VAR, SINGLE_CALLER_SPECIFIED)] = PERF_VFE_VAR_SINGLE_CALLER_SPECIFIED_VTABLE(),
};

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief VFE_VARS implementation of BoardObjGrpIfaceModel10CmdHandler().
 * @memberof VFE_VARS
 * @public
 * @copydetails BoardObjGrpIfaceModel10CmdHandler()
 */
FLCN_STATUS
vfeVarBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS     status           = FLCN_OK;
    LwBool          bFirstConstruct;
    OSTASK_OVL_DESC ovlDescList[]    = {
        OSTASK_OVL_DESC_DEFINE_VFE_BOARD_OBJ
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = s_vfeVarBoardObjGrpSetHelper(pBuffer, &bFirstConstruct);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    PMU_ASSERT_OK_OR_GOTO(status, status, vfeVarBoardObjGrpIfaceModel10Set_exit);

    // If successfully set, ilwalidate VFE and all dependnent object groups.
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_35) &&
        !bFirstConstruct)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            vfeBoardObjIlwalidate(),
            vfeVarBoardObjGrpIfaceModel10Set_exit);
    }

vfeVarBoardObjGrpIfaceModel10Set_exit:
    return status;
}

/*!
 * @brief Constructor.
 * @memberof VFE_VAR
 * @protected
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
vfeVarGrpIfaceModel10ObjSetImpl_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_VFE_VAR *pDescVar = (RM_PMU_VFE_VAR *)pBoardObjDesc;
    VFE_VAR        *pVfeVar;
    FLCN_STATUS     status;

    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc),
        vfeVarGrpIfaceModel10ObjSetImpl_SUPER_exit);

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVfeVar, *ppBoardObj, PERF, VFE_VAR, BASE,
                                  &status, vfeVarGrpIfaceModel10ObjSetImpl_SUPER_exit);

    // Set member variables.
    pVfeVar->outRangeMin = lwF32MapFromU32(&pDescVar->outRangeMin);
    pVfeVar->outRangeMax = lwF32MapFromU32(&pDescVar->outRangeMax);

    // Import dependency masks.

    boardObjGrpMaskInit_E255(&(pVfeVar->maskNotDependentVars));

    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpMaskImport_E255(&(pVfeVar->maskNotDependentVars),
                                   &(pDescVar->maskDependentVars)),
        vfeVarGrpIfaceModel10ObjSetImpl_SUPER_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpMaskIlw(&(pVfeVar->maskNotDependentVars)),
        vfeVarGrpIfaceModel10ObjSetImpl_SUPER_exit);

    vfeEquBoardObjMaskInit(&(pVfeVar->maskNotDependentEqus));

    PMU_ASSERT_OK_OR_GOTO(status,
        vfeEquBoardObjGrpMaskImport(&(pVfeVar->maskNotDependentEqus),
                                   &(pDescVar->maskDependentEqus)),
        vfeVarGrpIfaceModel10ObjSetImpl_SUPER_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpMaskIlw(&(pVfeVar->maskNotDependentEqus)),
        vfeVarGrpIfaceModel10ObjSetImpl_SUPER_exit);

    // Sanity check limits.
    if (lwF32CmpGT(pVfeVar->outRangeMin, pVfeVar->outRangeMax))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_TRUE_BP();
        goto vfeVarGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }

vfeVarGrpIfaceModel10ObjSetImpl_SUPER_exit:
    return status;
}

/*!
 * @brief VFE_VAR implementation of BoardObjGrpIfaceModel10CmdHandler().
 * @memberof VFE_VARS
 * @public
 * @copydetails BoardObjGrpIfaceModel10CmdHandler()
 */
FLCN_STATUS
vfeVarBoardObjGrpIfaceModel10GetStatus
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS     status        = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_VFE_BOARD_OBJ
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS(E255,           // _grpType
            VFE_VAR,                                    // _class
            pBuffer,                                    // _pBuffer
            NULL,                                       // _hdrFunc
            s_vfeVarIfaceModel10GetStatus,                          // _entryFunc
            pascalAndLater.perf.vfeVarGrpGetStatus);    // _ssElement
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    // NJ-TODO

    return status;
}

/*!
 * @copydoc VfeVarEval()
 * @memberof VFE_VAR
 * @protected
 */
FLCN_STATUS
vfeVarEval_SUPER
(
    VFE_CONTEXT    *pContext,
    VFE_VAR        *pVfeVar,
    LwF32          *pResult
)
{
    // *pResult already holds value, just apply range limits.

    if (lwF32CmpGT(*pResult, pVfeVar->outRangeMax))
    {
        *pResult = pVfeVar->outRangeMax;
    }
    else if (lwF32CmpLT(*pResult, pVfeVar->outRangeMin))
    {
        *pResult = pVfeVar->outRangeMin;
    }
    else
    {
        lwosNOP(); // Explicitly spell out NOP for MISRA 15.2 compliance
    }

    boardObjGrpMaskBitSet(&(pContext->maskCacheValidVars), pVfeVar->super.grpIdx);
    pContext->pOutCacheVars[pVfeVar->super.grpIdx] = *pResult;

    return FLCN_OK;
}

/*!
 * @brief VFE_VAR implemetation.
 * @memberof VFE_VAR
 * @private
 * @copydoc VfeVarEval()
 */
static FLCN_STATUS
s_vfeVarEval
(
    VFE_CONTEXT    *pContext,
    VFE_VAR        *pVfeVar,
    LwF32          *pResult
)
{
    FLCN_STATUS status = FLCN_OK;

    if (boardObjGrpMaskBitGet(&(pContext->maskCacheValidVars), pVfeVar->super.grpIdx))
    {
        *pResult = pContext->pOutCacheVars[pVfeVar->super.grpIdx];
    }
    else
    {
        switch (BOARDOBJ_GET_TYPE(pVfeVar))
        {
            case LW2080_CTRL_PERF_VFE_VAR_TYPE_DERIVED_PRODUCT:
                status = vfeVarEval_DERIVED_PRODUCT(pContext, pVfeVar, pResult);
                break;
            case LW2080_CTRL_PERF_VFE_VAR_TYPE_DERIVED_SUM:
                status = vfeVarEval_DERIVED_SUM(pContext, pVfeVar, pResult);
                break;
            case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_FREQUENCY:
                status = vfeVarEval_SINGLE_CALLER(pContext, pVfeVar, pResult);
                break;
            case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_SENSED_FUSE:
                if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_VAR_SINGLE_SENSED_FUSE_LEGACY))
                {
                    status = vfeVarEval_SINGLE_SENSED_FUSE(pContext, pVfeVar, pResult);
                }
                break;
            case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_SENSED_FUSE_20:
                if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_VAR_SINGLE_SENSED_FUSE_20))
                {
                    status = vfeVarEval_SINGLE_SENSED_FUSE_20(pContext, pVfeVar, pResult);
                }
                break;
            case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_SENSED_TEMP:
                status = vfeVarEval_SINGLE_SENSED_TEMP(pContext, pVfeVar, pResult);
                break;
            case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_VOLTAGE:
                status = vfeVarEval_SINGLE_CALLER(pContext, pVfeVar, pResult);
                break;
            case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_CALLER_SPECIFIED:
                status = vfeVarEval_SINGLE_CALLER(pContext, pVfeVar, pResult);
                break;
            case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_GLOBALLY_SPECIFIED:
                if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_VAR_SINGLE_GLOBALLY_SPECIFIED))
                {
                    status = vfeVarEval_SINGLE_GLOBALLY_SPECIFIED(pContext, pVfeVar, pResult);
                }
                break;
            default:
                status = FLCN_ERR_ILWALID_INDEX;
                PMU_BREAKPOINT();
                break;
        }
    }

    return status;
}

/*!
 * @copydoc VfeVarEvalByIdx()
 * @memberof VFE_VAR
 * @public
 */
FLCN_STATUS
vfeVarEvalByIdx
(
    VFE_CONTEXT    *pContext,
    LwU8            vfeVarIdx,
    LwF32          *pResult
)
{
    FLCN_STATUS status = FLCN_ERR_ILWALID_INDEX;
    VFE_VAR    *pVfeVar;

    // NJ-TODO: Generate meaningfull limit value.
    if (10U < (++(pContext->rcVfeVarEvalByIdx)))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_RELWRSION_LIMIT_EXCEEDED;
        goto vfeVarEvalByIdx_exit;
    }

    pVfeVar = BOARDOBJGRP_OBJ_GET(VFE_VAR, vfeVarIdx);
    if (pVfeVar == NULL)
    {
        PMU_BREAKPOINT();
        goto vfeVarEvalByIdx_exit;
    }

    status = s_vfeVarEval(pContext, pVfeVar, pResult);

vfeVarEvalByIdx_exit:
    pContext->rcVfeVarEvalByIdx--;
    return status;
}

/*!
 * @brief Evaluates VFE Variable by returning cached value or fails.
 * @memberof VFE_VAR
 * @protected
 * @copydetails VfeVarEvalByIdx()
 */
FLCN_STATUS
vfeVarEvalByIdxFromCache_IMPL
(
    VFE_CONTEXT    *pContext,
    LwU8            vfeVarIdx,
    LwF32          *pResult
)
{
    FLCN_STATUS status = FLCN_OK;

    if (!VFE_VAR_IS_VALID(vfeVarIdx))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto vfeVarEvalByIdxFromCache_exit;
    }

    if (!boardObjGrpMaskBitGet(&(pContext->maskCacheValidVars), vfeVarIdx))
    {
        //
        // Failing here means that not all required VFE VARs were cached, and
        // that happens when client has failed to provide values for SINGLE
        // non-SENSED VFE VARs required byt the evaluated VFE EQU.
        //
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto vfeVarEvalByIdxFromCache_exit;
    }

    // Fetch strait from the cache.
    *pResult = pContext->pOutCacheVars[vfeVarIdx];

vfeVarEvalByIdxFromCache_exit:
    return status;
}

/*!
 * @copydoc VfeVarSampleAll()
 * @memberof VFE_VARS
 * @public
 */
FLCN_STATUS
vfeVarSampleAll
(
    VFE_VARS   *pVfeVars,
    LwBool     *pBChanged
)
{
    VFE_VAR      *pVar;
    LwBoardObjIdx i;
    FLCN_STATUS   status   = FLCN_OK;

    BOARDOBJGRP_ITERATOR_PTR_BEGIN(VFE_VAR, (&pVfeVars->super.super), pVar, i, NULL)
    {
        LwBool bChangedTmp = LW_FALSE;

        status = s_vfeVarSample(pVar, &bChangedTmp);
        if (status != FLCN_OK)
        {
            goto vfeVarSampleAll_exit;
        }
        if (bChangedTmp)
        {
            LwU32 contextID;

            for (contextID = 0; contextID < (LwU32)VFE_CONTEXT_ID__COUNT; contextID++)
            {
                VFE_CONTEXT *pContext = VFE_CONTEXT_GET(&Perf, contextID);

                status = vfeVarDependentsCacheIlw(pContext, pVar);
                if (status != FLCN_OK)
                {
                    goto vfeVarSampleAll_exit;
                }
            }

            *pBChanged = bChangedTmp;
        }
    }
    BOARDOBJGRP_ITERATOR_PTR_END;

vfeVarSampleAll_exit:
    return status;
}

/*!
 * @copydoc VfeVarDependentsCacheIlw()
 * @memberof VFE_VAR
 * @protected
 */
FLCN_STATUS
vfeVarDependentsCacheIlw
(
    VFE_CONTEXT    *pContext,
    VFE_VAR        *pVfeVar
)
{
    FLCN_STATUS status;

    PMU_HALT_OK_OR_GOTO(status,
        boardObjGrpMaskAnd(&(pContext->maskCacheValidVars),
                           &(pContext->maskCacheValidVars),
                           &(pVfeVar->maskNotDependentVars)),
        vfeVarDependentsCacheIlw_exit);

    PMU_HALT_OK_OR_GOTO(status,
        boardObjGrpMaskAnd(&(pContext->maskCacheValidEqus),
                           &(pContext->maskCacheValidEqus),
                           &(pVfeVar->maskNotDependentEqus)),
        vfeVarDependentsCacheIlw_exit);

vfeVarDependentsCacheIlw_exit:
    return status;
}

/*!
 * @copydoc VfeVarsCacheAll()
 * @memberof VFE_VARS
 * @protected
 */
FLCN_STATUS
vfeVarsCacheAll
(
    VFE_CONTEXT *pContext
)
{
    FLCN_STATUS   status = FLCN_OK;
    VFE_VAR      *pVar   = NULL;
    LwF32         dummy;
    LwBoardObjIdx i;

    pContext->bCachingInProgress = LW_TRUE;

    BOARDOBJGRP_ITERATOR_BEGIN(VFE_VAR, pVar, i, NULL)
    {
        status = vfeVarEvalByIdx(pContext, BOARDOBJ_GRP_IDX_TO_8BIT(i), &dummy);
        if ((status != FLCN_OK) &&
            (status != FLCN_ERR_OBJECT_NOT_FOUND))
        {
            goto vfeVarsCacheAll_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    // Just in case last one was FLCN_ERR_OBJECT_NOT_FOUND.
    status = FLCN_OK;

vfeVarsCacheAll_exit:
    pContext->bCachingInProgress = LW_FALSE;

    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @copydetails vfeVarBoardObjGrpIfaceModel10Set()
 *
 * @note    Helper introduced to extract VFE code to VFE dedicated sections.
 */
static FLCN_STATUS
s_vfeVarBoardObjGrpSetHelper
(
    PMU_DMEM_BUFFER *pBuffer,
    LwBool          *pbFirstConstruct
)
{
    BOARDOBJGRP *pGrp   = &(PERF_GET_VFE_VARS(&Perf)->super.super);
    FLCN_STATUS  status = FLCN_OK;
    LwU32        contextID;

    *pbFirstConstruct = !pGrp->bConstructed;

    // Use the PERF DMEM write semaphore.
    perfWriteSemaphoreTake();
    {
        status = BOARDOBJGRP_IFACE_MODEL_10_SET(E255,            // _grpType
            VFE_VAR,                                    // _class
            pBuffer,                                    // _pBuffer
            s_vfeVarIfaceModel10SetHeader,                    // _hdrFunc
            s_vfeVarIfaceModel10SetEntry,                     // _entryFunc
            pascalAndLater.perf.vfeVarGrpSet,           // _ssElement
            OVL_INDEX_DMEM(perfVfe),                    // _ovlIdxDmem
            VfeVarVirtualTables);                       // _ppObjectVtables
        if (status != FLCN_OK)
        {
            goto s_vfeVarBoardObjGrpSetHelper_exit;
        }

        //
        // VFE Variables has changed:
        //  => first time allocate space for all VFE Variable caches
        //  => clear all VFE Variable and VFE Equation caches
        //  => force reevaluation in the next VFE timer callback
        //     (outside of this helper)
        //

        for (contextID = 0; contextID < (LwU32)VFE_CONTEXT_ID__COUNT; contextID++)
        {
            VFE_CONTEXT *pContext = VFE_CONTEXT_GET(&Perf, contextID);

            if (*pbFirstConstruct)
            {
                pContext->rcVfeVarEvalByIdx = 0;

                if (pGrp->objSlots != 0)
                {
                    pContext->pOutCacheVars =
                        lwosCallocType(pGrp->ovlIdxDmem, pGrp->objSlots, LwF32);
                    if (pContext->pOutCacheVars == NULL)
                    {
                        PMU_TRUE_BP();
                        status = FLCN_ERR_NO_FREE_MEM;
                        goto s_vfeVarBoardObjGrpSetHelper_exit;
                    }
                }

                pContext->bCachingInProgress = LW_FALSE;
            }

            boardObjGrpMaskInit_E255(&(pContext->maskCacheValidVars));
            vfeEquBoardObjMaskInit(&(pContext->maskCacheValidEqus));
        }

        PERF_GET_VFE(&Perf)->bObjectsUpdated = LW_TRUE;

s_vfeVarBoardObjGrpSetHelper_exit:
        lwosNOP();
    }
    perfWriteSemaphoreGive();

    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10SetHeader()
 * @memberof VFE_VARS
 * @private
 */
static FLCN_STATUS
s_vfeVarIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    RM_PMU_PERF_VFE_VAR_BOARDOBJGRP_SET_HEADER *pSet =
        (RM_PMU_PERF_VFE_VAR_BOARDOBJGRP_SET_HEADER *)pHdrDesc;
    FLCN_STATUS status;

    PMU_HALT_OK_OR_GOTO(status,
        boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc),
        s_vfeVarIfaceModel10SetHeader_exit);

    // Copy global VFE_VARS state.
    PERF_GET_VFE(&Perf)->pollingPeriodms = pSet->pollingPeriodms;

s_vfeVarIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry()
 * @memberof VFE_VARS
 * @private
 */
static FLCN_STATUS
s_vfeVarIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (pBuf->type)
    {
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_DERIVED_PRODUCT:
            status = vfeVarGrpIfaceModel10ObjSetImpl_DERIVED_PRODUCT(pModel10, ppBoardObj,
                        sizeof(VFE_VAR_DERIVED_PRODUCT), pBuf);
            break;
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_DERIVED_SUM:
            status = vfeVarGrpIfaceModel10ObjSetImpl_DERIVED_SUM(pModel10, ppBoardObj,
                        sizeof(VFE_VAR_DERIVED_SUM), pBuf);
            break;
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_FREQUENCY:
            status = vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_FREQUENCY(pModel10, ppBoardObj,
                        sizeof(VFE_VAR_SINGLE_FREQUENCY), pBuf);
            break;
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_SENSED_FUSE:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_VAR_SINGLE_SENSED_FUSE_LEGACY))
            {
                status = vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_FUSE(pModel10, ppBoardObj,
                        sizeof(VFE_VAR_SINGLE_SENSED_FUSE), pBuf);
            }
            break;
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_SENSED_FUSE_20:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_VAR_SINGLE_SENSED_FUSE_20))
            {
                status = vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_FUSE_20(pModel10, ppBoardObj,
                        sizeof(VFE_VAR_SINGLE_SENSED_FUSE_20), pBuf);
            }
            break;
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_SENSED_TEMP:
            status = vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_TEMP(pModel10, ppBoardObj,
                        sizeof(VFE_VAR_SINGLE_SENSED_TEMP), pBuf);
            break;
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_VOLTAGE:
            status = vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_VOLTAGE(pModel10, ppBoardObj,
                        sizeof(VFE_VAR_SINGLE_VOLTAGE), pBuf);
            break;
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_CALLER_SPECIFIED:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_VAR_35))
            {
                status = vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_CALLER_SPECIFIED(pModel10, ppBoardObj,
                        sizeof(VFE_VAR_SINGLE_CALLER_SPECIFIED), pBuf);
            }
            break;
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_GLOBALLY_SPECIFIED:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_VAR_SINGLE_GLOBALLY_SPECIFIED))
            {
                status = vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_GLOBALLY_SPECIFIED(pModel10, ppBoardObj,
                        sizeof(VFE_VAR_SINGLE_GLOBALLY_SPECIFIED), pBuf);
            }
            break;
        default:
            lwosNOP();
            break;
    }

    PMU_ASSERT_OK_OR_GOTO(status, status, s_vfeVarIfaceModel10SetEntry_exit);

s_vfeVarIfaceModel10SetEntry_exit:
    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus()
 * @memberof VFE_VARS
 * @private
 */
static FLCN_STATUS
s_vfeVarIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pBuf
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pBoardObj))
    {
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_DERIVED_PRODUCT:
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_DERIVED_SUM:
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_FREQUENCY:
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_VOLTAGE:
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_SENSED_TEMP:
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_CALLER_SPECIFIED:
            status = vfeVarIfaceModel10GetStatus_SUPER(pModel10, pBuf);
            break;
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_SENSED_FUSE:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_VAR_SINGLE_SENSED_FUSE_LEGACY))
            {
                status = vfeVarIfaceModel10GetStatus_SINGLE_SENSED_FUSE(pModel10, pBuf);
            }
            break;
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_SENSED_FUSE_20:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_VAR_SINGLE_SENSED_FUSE_20))
            {
                status = vfeVarIfaceModel10GetStatus_SINGLE_SENSED_FUSE_20(pModel10, pBuf);
            }
            break;
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_GLOBALLY_SPECIFIED:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_VAR_SINGLE_GLOBALLY_SPECIFIED))
            {
                status = vfeVarIfaceModel10GetStatus_SINGLE_GLOBALLY_SPECIFIED(pModel10, pBuf);
            }
            break;
        default:
            status = FLCN_ERR_NOT_SUPPORTED;
            break;
    }

    PMU_ASSERT_OK_OR_GOTO(status, status, s_vfeVarIfaceModel10GetStatus_exit);

s_vfeVarIfaceModel10GetStatus_exit:
    return status;
}

/*!
 * @copydoc VfeVarSample()
 * @memberof VFE_VAR
 * @private
 */
static FLCN_STATUS
s_vfeVarSample
(
    VFE_VAR    *pVfeVar,
    LwBool     *pBChanged
)
{
    FLCN_STATUS status = FLCN_ERR_ILWALID_INDEX;

    switch (BOARDOBJ_GET_TYPE(pVfeVar))
    {
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_DERIVED_PRODUCT:
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_DERIVED_SUM:
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_FREQUENCY:
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_SENSED_FUSE:
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_SENSED_FUSE_20:
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_VOLTAGE:
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_CALLER_SPECIFIED:
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_GLOBALLY_SPECIFIED:
            status = s_vfeVarSample_SUPER(pVfeVar, pBChanged);
            break;
        case LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_SENSED_TEMP:
            status = vfeVarSample_SINGLE_SENSED_TEMP(pVfeVar, pBChanged);
            break;
        default:
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            break;
    }

    return status;
}

/*!
 * @copydoc VfeVarSample()
 * @memberof VFE_VAR
 * @private
 */
static FLCN_STATUS
s_vfeVarSample_SUPER
(
    VFE_VAR    *pVfeVar,
    LwBool     *pBChanged
)
{
    *pBChanged = LW_FALSE;

    return FLCN_OK;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus()
 * @memberof VFE_VAR
 * @protected
 */
FLCN_STATUS
vfeVarIfaceModel10GetStatus_SUPER
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    return boardObjIfaceModel10GetStatus(pModel10, pPayload);
}
