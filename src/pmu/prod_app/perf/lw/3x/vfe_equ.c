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
 * @file    vfe_equ.c
 * @brief   VFE_EQU and VFE_EQUS class implementations.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

#include "dmemovl.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/3x/vfe_equ.h"
#include "pmu_objperf.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"
#include "boardobj/boardobjgrp_e1024.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static FLCN_STATUS  s_vfeEquBoardObjGrpSetHelper(PMU_DMEM_BUFFER *pBuffer, LwBool *pbFirstConstruct)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "s_vfeEquBoardObjGrpSetHelper")
    GCC_ATTRIB_NOINLINE();
static BoardObjGrpIfaceModel10SetEntry    (s_vfeEquIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "s_vfeEquIfaceModel10SetEntry");
static BoardObjGrpIfaceModel10SetHeader   (s_vfeEquIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "s_vfeEquIfaceModel10SetHeader");
static VfeEquEvalNode               (s_vfeEquEvalNode);

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief The maximum depth of a VFE equation chain.
 * @memberof VFE_EQU
 * @private
 */
#define VFE_EQU_MAX_DEPTH                                                    10U

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/*!
 * @brief   Array of all vtables pertaining to different PSTATE object types.
 */
BOARDOBJ_VIRTUAL_TABLE *VfeEquVirtualTables[LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_EQU, MAX)] =
{
    [LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_EQU, BASE)           ] = NULL,
    [LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_EQU, COMPARE)        ] = PERF_VFE_EQU_COMPARE_VTABLE(),
    [LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_EQU, EQUATION_SCALAR)] = PERF_VFE_EQU_EQUATION_SCALAR_VTABLE(),
    [LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_EQU, MINMAX)         ] = PERF_VFE_EQU_MINMAX_VTABLE(),
    [LW2080_CTRL_BOARDOBJ_TYPE(PERF, VFE_EQU, QUADRATIC)      ] = PERF_VFE_EQU_QUADRATIC_VTABLE(),
};

/* ------------------------ Inline Functions -------------------------------- */
/*!
 * @brief Constructor validator helper.
 *
 * @memberof VFE_EQU_SUPER
 * @private
 *
 * Performs validation of the parameters passed to the object constructor.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
s_vfeEquConstructValidateHelper_SUPER
(
    BOARDOBJGRP    *pBObjGrp,
    VFE_EQU        *pVfeEqu,
    RM_PMU_VFE_EQU *pDescEqu
)
{
    FLCN_STATUS status = FLCN_OK;

    LwF32 outRangeMin = lwF32MapFromU32(&pDescEqu->outRangeMin);
    LwF32 outRangeMax = lwF32MapFromU32(&pDescEqu->outRangeMax);

    // Sanity check limits.
    if (lwF32CmpGT(outRangeMin, outRangeMax))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_TRUE_BP();
        goto s_vfeEquConstructValidateHelper_SUPER_exit;
    }

    // Placeholder for more input parameter checks.

s_vfeEquConstructValidateHelper_SUPER_exit:
    return status;
}

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief VFE_VARS implementation of @ref BoardObjGrpIfaceModel10CmdHandler().
 * @copydetails BoardObjGrpIfaceModel10CmdHandler()
 * @memberof VFE_EQUS
 * @public
 */
FLCN_STATUS
vfeEquBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS     status        = FLCN_OK;
    LwBool          bFirstConstruct;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_VFE_BOARD_OBJ
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = s_vfeEquBoardObjGrpSetHelper(pBuffer, &bFirstConstruct);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    PMU_ASSERT_OK_OR_GOTO(status, status, vfeEquBoardObjGrpIfaceModel10Set_exit);

    // If successfully set, ilwalidate VFE and all dependnent object groups.
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_35) &&
        !bFirstConstruct)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            vfeBoardObjIlwalidate(),
            vfeEquBoardObjGrpIfaceModel10Set_exit);
    }

vfeEquBoardObjGrpIfaceModel10Set_exit:
    return status;
}

/*!
 * @brief Constructor.
 *
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 * @memberof VFE_EQU
 * @protected
 */
FLCN_STATUS
vfeEquGrpIfaceModel10ObjSetImpl_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_VFE_EQU *pDescEqu =
        (RM_PMU_VFE_EQU *)pBoardObjDesc;
    VFE_EQU        *pVfeEqu;
    FLCN_STATUS     status;

    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc),
        vfeEquGrpIfaceModel10ObjSetImpl_SUPER_exit);

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(
        pVfeEqu, *ppBoardObj, PERF, VFE_EQU, BASE, &status,
        vfeEquGrpIfaceModel10ObjSetImpl_SUPER_exit);

    if (!PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJ_CONSTRUCT_VALIDATE))
    {
        PMU_ASSERT_OK_OR_GOTO(status, 
            s_vfeEquConstructValidateHelper_SUPER(pBObjGrp, pVfeEqu, pDescEqu),
            vfeEquGrpIfaceModel10ObjSetImpl_SUPER_exit);
    }

    // Set member variables.
    pVfeEqu->varIdx      = pDescEqu->varIdx;
    VFE_EQU_IDX_COPY_RM_TO_PMU(pVfeEqu->equIdxNext, pDescEqu->equIdxNext);
    pVfeEqu->outputType  = pDescEqu->outputType;
    pVfeEqu->outRangeMin = lwF32MapFromU32(&pDescEqu->outRangeMin);
    pVfeEqu->outRangeMax = lwF32MapFromU32(&pDescEqu->outRangeMax);

vfeEquGrpIfaceModel10ObjSetImpl_SUPER_exit:
    return status;
}

/*!
 * @brief Constructor validator.
 *
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 * @memberof VFE_EQU
 * @protected
 */
FLCN_STATUS
vfeEquGrpIfaceModel10ObjSetValidate_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_VFE_EQU *pDescEqu =
        (RM_PMU_VFE_EQU *)pBoardObjDesc;
    VFE_EQU        *pVfeEqu;
    FLCN_STATUS     status;

    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpIfaceModel10ObjSetValidate(pModel10, ppBoardObj, size, pBoardObjDesc),
        vfeEquGrpIfaceModel10ObjSetValidate_SUPER_exit);

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(
        pVfeEqu, *ppBoardObj, PERF, VFE_EQU, BASE, &status,
        vfeEquGrpIfaceModel10ObjSetValidate_SUPER_exit);

    PMU_ASSERT_OK_OR_GOTO(status, 
        s_vfeEquConstructValidateHelper_SUPER(pBObjGrp, pVfeEqu, pDescEqu),
        vfeEquGrpIfaceModel10ObjSetValidate_SUPER_exit);

vfeEquGrpIfaceModel10ObjSetValidate_SUPER_exit:
    return status;
}

/*!
 * @copydoc VfeEquEvalList()
 * @memberof VFE_EQU
 * @public
 */
FLCN_STATUS
vfeEquEvalList_IMPL
(
    VFE_CONTEXT           *pContext,
    LwVfeEquIdx            vfeEquIdx,
    LwF32                 *pResult
)
{
    FLCN_STATUS status = FLCN_OK;
    LwF32       result = 0.0f;

    if (VFE_EQU_MAX_DEPTH < (++(pContext->rcVfeEquEvalList)))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_RELWRSION_LIMIT_EXCEEDED;
        goto vfeEquEvalList_exit;
    }

    while (PMU_PERF_VFE_EQU_INDEX_ILWALID != vfeEquIdx)
    {
        VFE_EQU    *pVfeEqu = BOARDOBJGRP_OBJ_GET(VFE_EQU, vfeEquIdx);
        LwF32       nodeResult;

        // Check whether we got valid object.
        if (pVfeEqu == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto vfeEquEvalList_exit;
        }

        PMU_HALT_OK_OR_GOTO(status,
            s_vfeEquEvalNode(pContext, pVfeEqu, &nodeResult),
            vfeEquEvalList_exit);

        result = lwF32Add(result, nodeResult);

        vfeEquIdx = pVfeEqu->equIdxNext;
    }

    // Copy out only on success.
    *pResult = result;

vfeEquEvalList_exit:
    pContext->rcVfeEquEvalList--;
    return status;
}

/*!
 * @brief Evaluates the base class equation node.
 * @copydetails VfeEquEvalNode()
 * @memberof VFE_EQU
 * @protected
 */
FLCN_STATUS
vfeEquEvalNode_SUPER_IMPL
(
    VFE_CONTEXT    *pContext,
    VFE_EQU        *pVfeEqu,
    LwF32          *pResult
)
{
    // *pResult already holds value, just apply range limits.

    if (lwF32CmpGT(*pResult, pVfeEqu->outRangeMax))
    {
        *pResult = pVfeEqu->outRangeMax;
    }
    else if (lwF32CmpLT(*pResult, pVfeEqu->outRangeMin))
    {
        *pResult = pVfeEqu->outRangeMin;
    }
    else
    {
        lwosNOP(); // Explicitly spell out NOP for MISRA 15.2 compliance
    }

    boardObjGrpMaskBitSet(&(pContext->maskCacheValidEqus), pVfeEqu->super.grpIdx);
    pContext->pOutCacheEqus[pVfeEqu->super.grpIdx] = *pResult;

    return FLCN_OK;
}

/*!
 * @brief Evaluates an equation node by calling the type-specific
 * implementation.
 * @copydetails VfeEquEvalNode()
 * @memberof VFE_EQU
 * @private
 */
static FLCN_STATUS
s_vfeEquEvalNode
(
    VFE_CONTEXT    *pContext,
    VFE_EQU        *pVfeEqu,
    LwF32          *pResult
)
{
    FLCN_STATUS status = FLCN_ERR_ILWALID_INDEX;

    if (boardObjGrpMaskBitGet(&(pContext->maskCacheValidEqus), pVfeEqu->super.grpIdx))
    {
        *pResult = pContext->pOutCacheEqus[pVfeEqu->super.grpIdx];
        return FLCN_OK;
    }

    switch (BOARDOBJ_GET_TYPE(pVfeEqu))
    {
        case LW2080_CTRL_PERF_VFE_EQU_TYPE_COMPARE:
            status = vfeEquEvalNode_COMPARE(pContext, pVfeEqu, pResult);
            break;
        case LW2080_CTRL_PERF_VFE_EQU_TYPE_MINMAX:
            status = vfeEquEvalNode_MINMAX(pContext, pVfeEqu, pResult);
            break;
        case LW2080_CTRL_PERF_VFE_EQU_TYPE_QUADRATIC:
            status = vfeEquEvalNode_QUADRATIC(pContext, pVfeEqu, pResult);
            break;
        case LW2080_CTRL_PERF_VFE_EQU_TYPE_EQUATION_SCALAR:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_EQU_35))
            {
                status = vfeEquEvalNode_EQUATION_SCALAR(pContext, pVfeEqu, pResult);
            }
            else
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_NOT_SUPPORTED;
            }
            break;
        default:
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            break;
    }

    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @copydetails vfeEquBoardObjGrpIfaceModel10Set()
 *
 * @note    Helper introduced to extract VFE code to VFE dedicated sections.
 */
static FLCN_STATUS
s_vfeEquBoardObjGrpSetHelper
(
    PMU_DMEM_BUFFER *pBuffer,
    LwBool          *pbFirstConstruct
)
{
    BOARDOBJGRP *pGrp   = &(PERF_GET_VFE_EQUS(&Perf)->super.super);
    FLCN_STATUS  status = FLCN_OK;
    LwU32        contextID;

    *pbFirstConstruct = !pGrp->bConstructed;

    // Use the PERF DMEM write semaphore.
    perfWriteSemaphoreTake();
    {
        status = VFE_EQU_BOARDOBJGRP_CONSTRUCT_AUTO_DMA(
            VFE_EQU,                                    // _class
            pBuffer,                                    // _pBuffer
            s_vfeEquIfaceModel10SetHeader,                    // _hdrFunc
            s_vfeEquIfaceModel10SetEntry,                     // _entryFunc
            pascalAndLater.perf.vfeEquGrpSet,           // _ssElement
            OVL_INDEX_DMEM(perfVfe),                    // _ovlIdxDmem
            VfeEquVirtualTables);                       // _ppObjectVtables
        if (status != FLCN_OK)
        {
            goto s_vfeEquBoardObjGrpSetHelper_exit;
        }

        //
        // VFE Equations has changed:
        //  => first time allocate space for all VFE Equation caches
        //  => clear all VFE Equation caches
        //  => force reevaluation in the next VFE timer callback
        //     (outside of this helper)
        //

        for (contextID = 0; contextID < (LwU32)VFE_CONTEXT_ID__COUNT; contextID++)
        {
            VFE_CONTEXT *pContext = VFE_CONTEXT_GET(&Perf, contextID);

            if (*pbFirstConstruct)
            {
                pContext->rcVfeEquEvalList = 0;

                if (pGrp->objSlots != 0)
                {
                    pContext->pOutCacheEqus =
                        lwosCallocType(pGrp->ovlIdxDmem, pGrp->objSlots, LwF32);
                    if (pContext->pOutCacheEqus == NULL)
                    {
                        PMU_TRUE_BP();
                        status = FLCN_ERR_NO_FREE_MEM;
                        goto s_vfeEquBoardObjGrpSetHelper_exit;
                    }
                }
            }

            vfeEquBoardObjMaskInit(&(pContext->maskCacheValidEqus));
        }

        PERF_GET_VFE(&Perf)->bObjectsUpdated = LW_TRUE;

s_vfeEquBoardObjGrpSetHelper_exit:
        lwosNOP();
    }
    perfWriteSemaphoreGive();

    return status;
}

/*!
 * @brief Factory function to construct the VFE_EQUS header.
 * @note The intention is to do an appropriate transition from
 *      always 1024 VFE_EQU RM-PMU structure to the (chip-dependent)
 *      255 VFE_EQU PMU (older chip) structure. Most of the
 *      work is already done in the BOARDOBJGRP(MASK) code,
 *      here our job is just to change the type if the chip
 *      is older.
 * @memberof VFE_EQUS
 * @private
 * @copydetails BoardObjGrpIfaceModel10SetHeader()
 */
static FLCN_STATUS
s_vfeEquIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    //
    // If on older chips (i.e. chips who's PMU does not support
    // 1024 VFE EQU entries), set the RM-PMU type to E255 (it
    // would be set by RM to E1024, since RM is always E1024
    // regardless of the underlying PMU). This is done to ensure
    // the creation of the PMU's BOARDOBJGRP goes without a hitch
    // and the right things are done to keep it at E255.
    //
    // For newer chips (that have PMU that do support 1024 VFE EQU
    // entries), this function basically is a wrapper around
    // the BOARDOBJGRP implementation @ref boardObjGrpIfaceModel10SetHeader.
    //
    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_EQU_BOARDOBJGRP_1024))
    {
        //
        // Sanity check that @ref BOARDOBJGRP::objslots is less than
        // 255, which it should, since older chips (defined above)
        // can only support upto 255
        //
        PMU_ASSERT_TRUE_OR_GOTO(
            status,
            pBObjGrp->objSlots <= LW2080_CTRL_BOARDOBJGRP_E255_MAX_OBJECTS,
            FLCN_ERR_ILWALID_STATE,
            s_vfeEquIfaceModel10SetHeader_exit);
        //
        // Actually do the setting to E255
        // This line should always happen, regardless of whether
        // the BOARDOBJGRP is being constructed for the first time
        // or not, since RM will always set to E1024 when calling
        // the SET interface.
        //
        // Also check if type being overriden is 1024.
        //
        PMU_ASSERT_TRUE_OR_GOTO(
            status,
            pHdrDesc->type == LW2080_CTRL_BOARDOBJGRP_TYPE_E1024,
            FLCN_ERR_ILWALID_STATE,
            s_vfeEquIfaceModel10SetHeader_exit);
        pHdrDesc->type = LW2080_CTRL_BOARDOBJGRP_TYPE_E255;
    }

    // Call into the base class implementation
    PMU_ASSERT_OK_OR_GOTO(
        status,
        boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc),
        s_vfeEquIfaceModel10SetHeader_exit);

s_vfeEquIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * @brief Factory function to construct specific VFE_EQU instances.
 * @memberof VFE_EQUS
 * @private
 * @copydetails BoardObjGrpIfaceModel10SetEntry()
 */
static FLCN_STATUS
s_vfeEquIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (pBuf->type)
    {
        case LW2080_CTRL_PERF_VFE_EQU_TYPE_COMPARE:
            status = vfeEquGrpIfaceModel10ObjSetImpl_COMPARE(pModel10, ppBoardObj,
                        sizeof(VFE_EQU_COMPARE), pBuf);
            break;
        case LW2080_CTRL_PERF_VFE_EQU_TYPE_MINMAX:
            status = vfeEquGrpIfaceModel10ObjSetImpl_MINMAX(pModel10, ppBoardObj,
                        sizeof(VFE_EQU_MINMAX), pBuf);
            break;
        case LW2080_CTRL_PERF_VFE_EQU_TYPE_QUADRATIC:
            status = vfeEquGrpIfaceModel10ObjSetImpl_QUADRATIC(pModel10, ppBoardObj,
                        sizeof(VFE_EQU_QUADRATIC), pBuf);
            break;
        case LW2080_CTRL_PERF_VFE_EQU_TYPE_EQUATION_SCALAR:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_EQU_35))
            {
                status = vfeEquGrpIfaceModel10ObjSetImpl_EQUATION_SCALAR(pModel10, ppBoardObj,
                        sizeof(VFE_EQU_EQUATION_SCALAR), pBuf);
            }
            break;
        default:
            lwosNOP();
            break;
    }

    PMU_ASSERT_OK_OR_GOTO(status, status, s_vfeEquIfaceModel10SetEntry_exit);

s_vfeEquIfaceModel10SetEntry_exit:
    return status;
}

/*!
 * @brief Factory function to construct-validate specific VFE_EQU instances.
 * @memberof VFE_EQUS
 * @private
 * @copydetails BoardObjGrpIfaceModel10SetEntry()
 */
FLCN_STATUS
s_vfeEquConstructValidateEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (pBuf->type)
    {
        case LW2080_CTRL_PERF_VFE_EQU_TYPE_COMPARE:
            status = vfeEquGrpIfaceModel10ObjSetValidate_COMPARE(pModel10, ppBoardObj,
                        sizeof(VFE_EQU_COMPARE), pBuf);
            break;
        case LW2080_CTRL_PERF_VFE_EQU_TYPE_MINMAX:
            status = vfeEquGrpIfaceModel10ObjSetValidate_MINMAX(pModel10, ppBoardObj,
                        sizeof(VFE_EQU_MINMAX), pBuf);
            break;
        case LW2080_CTRL_PERF_VFE_EQU_TYPE_QUADRATIC:
            status = vfeEquGrpIfaceModel10ObjSetValidate_QUADRATIC(pModel10, ppBoardObj,
                        sizeof(VFE_EQU_QUADRATIC), pBuf);
            break;
        case LW2080_CTRL_PERF_VFE_EQU_TYPE_EQUATION_SCALAR:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_EQU_35))
            {
                status = vfeEquGrpIfaceModel10ObjSetValidate_EQUATION_SCALAR(pModel10, ppBoardObj,
                        sizeof(VFE_EQU_EQUATION_SCALAR), pBuf);
            }
            break;
        default:
            lwosNOP();
            break;
    }

    PMU_ASSERT_OK_OR_GOTO(status, status, s_vfeEquConstructValidateEntry_exit);

s_vfeEquConstructValidateEntry_exit:
    return status;
}
