/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    boardobjgrp_iface_model_10.c
 * @copydoc boardobjgrp_iface_model_10.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "pmu/ssurface.h"

/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief   Generic "super" class for any BOARDOBJGRP implementing class.
 *
 * @note    Because the _E<SIZE> group classes extend the object mask and
 *          not their BOARDOBJGP super, this is how one must represent a
 *          super class in the context of this group-size agnostic code.
 *
 * @note    This design depends on the fact that derived BOARDOBOJGRP classes
 *          will always have a BOARDOBJGRP super as their first member and a
 *          derived BOARDOBJGRPMASK field as the second member.
 */
typedef struct
{
    BOARDOBJGRP     super;
    BOARDOBJGRPMASK objMask;
} BOARDOBJGRP_SUPER;

/*!
 * @brief   Generic "super" class for any RM_PMU _E<SIZE> object.
 *
 * @note    Use of this structure comes from the same justification seen in
 *          @ref BOARDOBJGRP_SUPER.
 *
 * @note    This design depends on the fact that derived RM_PMU_BOARDOBJGRP
 *          classes will always have a RM_PMU_BOARDOBJGRP super as their first
 *          member and a derived LW2080_CTRL_BOARDOBJGRP_MASK field as the
 *          second member.
 */
typedef struct
{
    RM_PMU_BOARDOBJGRP              super;
    LW2080_CTRL_BOARDOBJGRP_MASK    objMask;
} RM_PMU_BOARDOBJGRP_SUPER;

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Static Asserts ---------------------------------- */

// Make sure the enum LW2080_CTRL_BOARDOBJGRP_CLASS_ID hasn't exceeded its size
ct_assert(LW2080_CTRL_BOARDOBJGRP_CLASS_ID__MAX < (sizeof(LW2080_CTRL_BOARDOBJGRP_CLASS_ID) << 8));

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * Board Object Group construction function.
 *
 * This is the default implementation of @ref BoardObjGrpIfaceModel10Set interface.
 * It will first calls hdrFunc to issue DMA read to fetch header data and
 * construct it, then according to the returned entryMask from hdrFunc, calls
 * multiple times of entryFunc to construct each entry.
 *
 * To use the default implementation, the boardObjGrp must:
 *   -# Be constructing only 1 entry per one DMA read. Some tables have smaller
 *      yet more entries, where issuing one DMA for each entry results in
 *      too many DMA transactions should implement its own BoardObjGrpIfaceModel10Set
 *      interface.
 *   -# Put each header and corresponding entry array adjacent to each other,
 *      so that to construct the whole boardObjGrp, a series of call into
 *      boardObjGrpIfaceModel10Set_E32() could be called, each with its header
 *      structure offset as readOffset. The hdrFunc should parse each _HEADER
 *      structure and return entryMask for subsequent entry construction to use.
 *
 * @copydetails BoardObjGrpIfaceModel10Set()
 */
FLCN_STATUS
boardObjGrpIfaceModel10Set
(
    BOARDOBJGRP_IFACE_MODEL_10                *pModel10,
    LwU8                        groupType,
    PMU_DMEM_BUFFER            *pBuffer,
    BoardObjGrpIfaceModel10SetHeader  (hdrFunc),
    LwLength                    hdrSize,
    BoardObjGrpIfaceModel10SetEntry   (entryFunc),
    LwLength                    entrySize,
    LwU32                       ssOffset,
    LwU8                        ovlIdxDmem,
    BOARDOBJGRPMASK            *pMask,
    BOARDOBJ_VIRTUAL_TABLE    **ppObjectVtables,
    LwLength                    numObjectVtables
)
{
    BOARDOBJGRP *pBoardObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    BOARDOBJGRP_SUPER              *pGrpSuper = (BOARDOBJGRP_SUPER *)(pBoardObjGrp);
    void                           *pBuf      = pBuffer->pBuf;
    RM_PMU_BOARDOBJGRP_SUPER       *pHdrSuper;
    LwBoardObjIdx                   grpIdx;
    FLCN_STATUS                     status;
    LwU32                           readOffset = ssOffset;
    LwU8                            mIndex;
    LwBoardObjIdx                   maskSize;
    LwU8                            j;

#if PMUCFG_FEATURE_ENABLED(PMU_FBQ) && PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING)
    if ((pBuffer->size < hdrSize) ||
        (pBuffer->size < entrySize))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_OUT_OF_RANGE;
        goto boardObjGrpIfaceModel10Set_done;
    }
#endif

    // Caller provides groupType since it is lost while casting first operand.
    pBoardObjGrp->type       = groupType;
    pBoardObjGrp->ovlIdxDmem = ovlIdxDmem;

    if (BOARDOBJGRP_VIRTUAL_TABLES_GET(pBoardObjGrp) != NULL)
    {
        BOARDOBJGRP_VIRTUAL_TABLES *pVtables =
            BOARDOBJGRP_VIRTUAL_TABLES_GET(pBoardObjGrp);

        if ((numObjectVtables == 0) &&
            (ppObjectVtables  != NULL))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto boardObjGrpIfaceModel10Set_done;
        }

        pVtables->numObjectVtables = numObjectVtables;
        pVtables->ppObjectVtables  = ppObjectVtables;
    }

    // 1. Fetch and parse header using hdrFunc().

    PMU_HALT_OK_OR_GOTO(status,
        ssurfaceRd(pBuf, readOffset, hdrSize),
        boardObjGrpIfaceModel10Set_done);

    readOffset += hdrSize;

    PMU_HALT_OK_OR_GOTO(status,
        hdrFunc(pModel10, (RM_PMU_BOARDOBJGRP *)pBuf),
        boardObjGrpIfaceModel10Set_done);

    //
    // 2. Create the mask to parse over, as we can be updating a subset of
    //    BOARDOBJ
    //
    PMU_HALT_OK_OR_GOTO(status,
        boardObjGrpMaskSizeGet(pBoardObjGrp, &maskSize),
        boardObjGrpIfaceModel10Set_done);

    pHdrSuper = (RM_PMU_BOARDOBJGRP_SUPER *)pBuf;
    boardObjGrpMaskInit_SUPER(pMask, maskSize);

    PMU_HALT_OK_OR_GOTO(status,
        boardObjGrpMaskImport_FUNC(pMask, maskSize, &pHdrSuper->objMask),
        boardObjGrpIfaceModel10Set_done);

    // 3. Fetch and parse entries using entryFunc().
    for (mIndex = 0; mIndex < pGrpSuper->objMask.maskDataCount; mIndex++)
    {
        FOR_EACH_INDEX_IN_MASK(32, j, pMask->pData[mIndex])
        {
            grpIdx = mIndex * LW2080_CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_BIT_SIZE + j;

            PMU_HALT_OK_OR_GOTO(status,
                ssurfaceRd(pBuf, readOffset + grpIdx * entrySize, entrySize),
                boardObjGrpIfaceModel10Set_done);

            //
            // Set grpIdx as construct parameter, consumed by
            // boardObjGrpIfaceModel10ObjSet().  Thus, implementing classes can use this
            // parameter within their construct routines.
            //
            ((RM_PMU_BOARDOBJ *)pBuf)->grpIdx = grpIdx;

            status = entryFunc(pModel10, &pBoardObjGrp->ppObjects[grpIdx],
                        (RM_PMU_BOARDOBJ *)pBuf);
            if (status != FLCN_OK)
            {
                goto boardObjGrpIfaceModel10Set_done;
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }

boardObjGrpIfaceModel10Set_done:
    return status;
}

/*!
 * @copydetails boardObjGrpIfaceModel10Set
 *
 * Constructs a boardObjGrp, under the assumption that the subsurface used has
 * already been transferred into *pBuffer with the Auto DMA feature.
 */
FLCN_STATUS
boardObjGrpIfaceModel10SetAutoDma
(
    BOARDOBJGRP_IFACE_MODEL_10                *pModel10,
    LwU8                        groupType,
    PMU_DMEM_BUFFER            *pBuffer,
    BoardObjGrpIfaceModel10SetHeader  (hdrFunc),
    LwLength                    hdrSize,
    BoardObjGrpIfaceModel10SetEntry   (entryFunc),
    LwLength                    entrySize,
    LwU32                       ssOffset,        /* Unused */
    LwU8                        ovlIdxDmem,
    BOARDOBJGRPMASK            *pMask,
    BOARDOBJ_VIRTUAL_TABLE    **ppObjectVtables,
    LwLength                    numObjectVtables
)
{
    BOARDOBJGRP *pBoardObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    BOARDOBJGRP_SUPER              *pGrpSuper = (BOARDOBJGRP_SUPER *)(pBoardObjGrp);
    LwU8                           *pBuf      = pBuffer->pBuf;
    RM_PMU_BOARDOBJGRP_SUPER       *pHdrSuper;
    LwBoardObjIdx                   grpIdx;
    FLCN_STATUS                     status;
    LwU8                            mIndex;
    LwBoardObjIdx                   maskSize;
    LwU8                            j;
    RM_PMU_BOARDOBJ                *pLwrBoardObj;

    // Caller provides groupType since it is lost while casting first operand.
    pBoardObjGrp->type       = groupType;
    pBoardObjGrp->ovlIdxDmem = ovlIdxDmem;

    if (BOARDOBJGRP_VIRTUAL_TABLES_GET(pBoardObjGrp) != NULL)
    {
        BOARDOBJGRP_VIRTUAL_TABLES *pVtables =
                BOARDOBJGRP_VIRTUAL_TABLES_GET(pBoardObjGrp);

        if ((numObjectVtables == 0) &&
            (ppObjectVtables  != NULL))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto boardObjGrpIfaceModel10Set_done;
        }

        pVtables->numObjectVtables = numObjectVtables;
        pVtables->ppObjectVtables  = ppObjectVtables;
    }

    // 1. Parse header using hdrFunc().
    PMU_HALT_OK_OR_GOTO(status,
                        hdrFunc(pModel10, (RM_PMU_BOARDOBJGRP *)pBuf),
                        boardObjGrpIfaceModel10Set_done);

    //
    // 2. Create the mask to parse over, as we can be updating a subset of
    //    BOARDOBJ
    //
    PMU_HALT_OK_OR_GOTO(status,
                        boardObjGrpMaskSizeGet(pBoardObjGrp, &maskSize),
                        boardObjGrpIfaceModel10Set_done);

    pHdrSuper = (RM_PMU_BOARDOBJGRP_SUPER *)pBuf;
    boardObjGrpMaskInit_SUPER(pMask, maskSize);

    PMU_HALT_OK_OR_GOTO(status,
                        boardObjGrpMaskImport_FUNC(pMask, maskSize, &pHdrSuper->objMask),
                        boardObjGrpIfaceModel10Set_done);

    // 3. Fetch and parse entries using entryFunc().
    pBuf += hdrSize;
    for (mIndex = 0; mIndex < pGrpSuper->objMask.maskDataCount; mIndex++)
    {
        FOR_EACH_INDEX_IN_MASK(32, j, pMask->pData[mIndex])
        {
            grpIdx = mIndex * LW2080_CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_BIT_SIZE + j;

            //
            // Since entire supersurface is in DMEM, we need to callwlate the offset
            // from within pBuf to find the right object.
            //
            pLwrBoardObj = (RM_PMU_BOARDOBJ *)(pBuf + grpIdx * entrySize);

            //
            // Set grpIdx as construct parameter, consumed by
            // boardObjGrpIfaceModel10ObjSet().  Thus, implementing classes can use this
            // parameter within their construct routines.
            //
            pLwrBoardObj->grpIdx = grpIdx;

            status = entryFunc(pModel10, &pBoardObjGrp->ppObjects[grpIdx], pLwrBoardObj);
            if (status != FLCN_OK)
            {
                goto boardObjGrpIfaceModel10Set_done;
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }

boardObjGrpIfaceModel10Set_done:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10CmdHandlerDispatch()
 */
FLCN_STATUS
boardObjGrpIfaceModel10CmdHandlerDispatch_IMPL
(
    LW2080_CTRL_BOARDOBJGRP_CLASS_ID    classId,
    PMU_DMEM_BUFFER                    *pBuffer,
    LwU8                                numEntries,
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_ENTRY      *pEntries,
    RM_PMU_BOARDOBJGRP_CMD              commandId
)
{
    FLCN_STATUS             status = FLCN_ERR_NOT_SUPPORTED;
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_DESC   cmdDesc;
    LwU8                    i;

    // Search through array for engine-specific handler for the type.
    for (i = 0; i < numEntries; i++)
    {
        if (pEntries[i].classId == classId)
        {
            if (commandId < RM_PMU_BOARDOBJGRP_CMD__COUNT)
            {
                cmdDesc = pEntries[i].cmdDesc[commandId];

                if (cmdDesc.pHandler != NULL)
                {
#if (PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_RPC_AUTO_DMA))
                    LwU32   ssOffset    = cmdDesc.subsurfLoc.offset;
                    LwU32   hdrSize     = cmdDesc.subsurfLoc.hdrSize;
                    LwU32   entrySize   = cmdDesc.subsurfLoc.entrySize;
                    LwU32   objSlots    = 0;
                    LwU32   grpSize     = 0;
                    void   *pBuf        = pBuffer->pBuf;

                    if ((ssOffset != 0) && (hdrSize != 0) && (entrySize != 0))
                    {
                        status = ssurfaceRd(pBuf, ssOffset, hdrSize);

                        // Determine how much memory to DMA
                        objSlots = ((RM_PMU_BOARDOBJGRP *)pBuf)->objSlots;
                        grpSize  = hdrSize + objSlots * entrySize;

                        if ((objSlots > cmdDesc.subsurfLoc.grpSize)||
                            (grpSize > pBuffer->size))
                        {
                            status = FLCN_ERR_ILWALID_INDEX;
                            goto boardObjGrpIfaceModel10CmdHandlerDispatch_IMPL_exit;
                        }

                        if (commandId == RM_PMU_BOARDOBJGRP_CMD_GET_STATUS)
                        {
                            if (PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJ_GET_STATUS_COPY_IN) &&
                                FLD_TEST_DRF(_RM_PMU_BOARDOBJGRP_SUPER, _FLAGS, _GET_STATUS_COPY_IN,
                                    _TRUE, ((RM_PMU_BOARDOBJGRP_SUPER *)pBuf)->super.flags))
                            {
                                // If bCopyIn is set on header, DMA whole subsurface
                               status = ssurfaceRd(pBuf, ssOffset, grpSize);
                            }
                            else
                            {
                                // Otherwise, set unused memory to 0
                                memset(pBuf + hdrSize, 0x0, objSlots * entrySize);
                            }
                        }
                        else
                        {
                            // Always perform whole DMA for BoardObjGrpSet
                            status  = ssurfaceRd(pBuf, ssOffset, grpSize);
                            // Ensure no write back on completion for GrpSet
                            grpSize = 0;
                        }
                    }
#endif // PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_RPC_AUTO_DMA)

                    status = cmdDesc.pHandler(pBuffer);

#if (PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_RPC_AUTO_DMA))
                    if (grpSize != 0)
                    {
                        status = ssurfaceWr(pBuf, ssOffset, grpSize);
                    }
#endif // PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_RPC_AUTO_DMA)
                }
            }
            else
            {
                status = FLCN_ERR_ILWALID_ARGUMENT;
            }
            break;
        }
    }

#if (PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_RPC_AUTO_DMA))
boardObjGrpIfaceModel10CmdHandlerDispatch_IMPL_exit:
#endif // PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_RPC_AUTO_DMA)
    return status;
}

/*!
 * @brief   Super implementation for @ref BoardObjGrpIfaceModel10SetHeader interface.
 *
 * @copydetails BoardObjGrpIfaceModel10SetHeader()
 */
FLCN_STATUS
boardObjGrpIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    BOARDOBJGRP_SUPER          *pGrpSuper = (BOARDOBJGRP_SUPER *)(pBObjGrp);
    RM_PMU_BOARDOBJGRP_SUPER   *pHdrSuper = (RM_PMU_BOARDOBJGRP_SUPER *)pHdrDesc;
    FLCN_STATUS                 status    = FLCN_ERR_ILWALID_STATE;

    // Assure that caller (RM) and client (PMU) use the same group type.
    if (pBObjGrp->type != pHdrDesc->type)
    {
        PMU_BREAKPOINT();
        goto boardObjGrpIfaceModel10SetHeader_exit;
    }

    if (!pBObjGrp->bConstructed)
    {
        LwBoardObjIdx maskSize;

        PMU_HALT_OK_OR_GOTO(status,
            boardObjGrpMaskSizeGet(pBObjGrp, &maskSize),
            boardObjGrpIfaceModel10SetHeader_exit);

        // Create => allocate memory for the array of board object pointers.
        pBObjGrp->classId  = pHdrDesc->classId;
        pBObjGrp->objSlots = pHdrDesc->objSlots;

        if (pBObjGrp->objSlots > maskSize)
        {
            status = FLCN_ERR_OUT_OF_RANGE;
            PMU_BREAKPOINT();
            goto boardObjGrpIfaceModel10SetHeader_exit;
        }

        if (pBObjGrp->objSlots != 0)
        {
            pBObjGrp->ppObjects = lwosCallocType(
                pBObjGrp->ovlIdxDmem, pBObjGrp->objSlots, BOARDOBJ *);
            if (pBObjGrp->ppObjects == NULL)
            {
                status = FLCN_ERR_NO_FREE_MEM;
                PMU_BREAKPOINT();
                goto boardObjGrpIfaceModel10SetHeader_exit;
            }
        }

        // Copy in mask of Board Objects.
        boardObjGrpMaskInit_SUPER(&pGrpSuper->objMask, maskSize);
        status = boardObjGrpMaskImport_FUNC(&pGrpSuper->objMask, maskSize,
                                            &pHdrSuper->objMask);
        if (status != FLCN_OK)
        {
            goto boardObjGrpIfaceModel10SetHeader_exit;
        }

        pBObjGrp->bConstructed = LW_TRUE;
    }
    else
    {
        LwU8 mIndex;

        // Update => header information must not change.
        if ((pBObjGrp->classId  != pHdrDesc->classId) ||
            (pBObjGrp->objSlots != pHdrDesc->objSlots))
        {
            PMU_BREAKPOINT();
            goto boardObjGrpIfaceModel10SetHeader_exit;
        }

        for (mIndex = 0; mIndex < pGrpSuper->objMask.maskDataCount; mIndex++)
        {
            LwU32  grpMask = pGrpSuper->objMask.pData[mIndex];
            LwU32  hdrMask = pHdrSuper->objMask.pData[mIndex];

            if (FLD_TEST_DRF(_RM_PMU_BOARDOBJGRP_SUPER, _FLAGS, _SET_TYPE,
                    _INIT, pHdrSuper->super.flags))
            {
                if (grpMask != hdrMask)
                {
                    // Masks must match with _INIT
                    PMU_BREAKPOINT();
                    goto boardObjGrpIfaceModel10SetHeader_exit;
                }

            }
            else if (FLD_TEST_DRF(_RM_PMU_BOARDOBJGRP_SUPER, _FLAGS, _SET_TYPE,
                        _UPDATE, pHdrSuper->super.flags))
            {
                if ((grpMask & hdrMask) != hdrMask)
                {
                    // hdrMask must be a subset of grpMask with _UPDATE
                    PMU_BREAKPOINT();
                    goto boardObjGrpIfaceModel10SetHeader_exit;
                }
            }
            else
            {
                // Invalid flag
                PMU_BREAKPOINT();
                goto boardObjGrpIfaceModel10SetHeader_exit;
            }
        }

        status = FLCN_OK;
    }

boardObjGrpIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * @brief   Super implementation for @ref BoardObjGrpIfaceModel10GetStatus() interface.
 *
 * @copydetails BoardObjGrpIfaceModel10GetStatus()
 */
FLCN_STATUS
boardObjGrpIfaceModel10GetStatus_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10                    *pModel10,
    PMU_DMEM_BUFFER                *pBuffer,
    BoardObjGrpIfaceModel10GetStatusHeader      (hdrFunc),
    LwLength                        hdrSize,
    BoardObjIfaceModel10GetStatus                   (entryFunc),
    LwLength                        entrySize,
    LwU32                           ssOffset,
    BOARDOBJGRPMASK                *pMask
)
{
    BOARDOBJGRP *pBoardObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    FLCN_STATUS                     status;
    void                           *pBuf   = pBuffer->pBuf;
    RM_PMU_BOARDOBJGRP_SUPER       *pHdrSuper;
    BOARDOBJ                       *pBoardObj;
    LwU32                           readOffset = ssOffset;
    LwBoardObjIdx                   i;
    LwBool                          bCopyIn;
    LwBoardObjIdx                   maskSize;

#if PMUCFG_FEATURE_ENABLED(PMU_FBQ) && PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING)
    if ((pBuffer->size < hdrSize) ||
        (pBuffer->size < entrySize))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_OUT_OF_RANGE;
        goto boardObjGrpIfaceModel10GetStatus_SUPER_done;
    }
#endif

    // Always read in the header, which will provide the mask.
    PMU_HALT_OK_OR_GOTO(status,
        ssurfaceRd(pBuf, readOffset, hdrSize),
        boardObjGrpIfaceModel10GetStatus_SUPER_done);

    // Determine the mask size.
    PMU_HALT_OK_OR_GOTO(status,
        boardObjGrpMaskSizeGet(pBoardObjGrp, &maskSize),
        boardObjGrpIfaceModel10GetStatus_SUPER_done);

    //
    // Parse the RM mask of objects from the header data.  For now, will trust
    // that RM mask is a subset of the BOARDOBJGRP mask.  @ref
    // _BOARDOBJGRP_ITERATOR_BEGIN() will only iterate over the intersection of
    // the mask with the set of objects actually in the BOARDOBJGRP.
    //
    pHdrSuper = (RM_PMU_BOARDOBJGRP_SUPER *)pBuf;
    boardObjGrpMaskInit_SUPER(pMask, maskSize);

    PMU_HALT_OK_OR_GOTO(status,
        boardObjGrpMaskImport_FUNC(pMask, maskSize, &pHdrSuper->objMask),
        boardObjGrpIfaceModel10GetStatus_SUPER_done);

    bCopyIn =
        PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJ_GET_STATUS_COPY_IN) &&
        FLD_TEST_DRF(_RM_PMU_BOARDOBJGRP_SUPER, _FLAGS, _GET_STATUS_COPY_IN,
                     _TRUE, pHdrSuper->super.flags);

    //
    // Call into hdrFunc() to query global header data and then DMA the header
    // data out to the RM (if required).
    //
    if (hdrFunc != NULL)
    {
        status = hdrFunc(pModel10, (RM_PMU_BOARDOBJGRP *)pBuf, pMask);
        if (status != FLCN_OK)
        {
            goto boardObjGrpIfaceModel10GetStatus_SUPER_done;
        }

        PMU_HALT_OK_OR_GOTO(status,
            ssurfaceWr(pBuf, readOffset, hdrSize),
            boardObjGrpIfaceModel10GetStatus_SUPER_done);
    }

    readOffset += hdrSize;

    // Parse each entry using entryFunc and DMA the entry data out to the RM.
    BOARDOBJGRP_ITERATOR_PTR_BEGIN(BOARDOBJ, pBoardObjGrp, pBoardObj, i, pMask)
    {
        BOARDOBJ_IFACE_MODEL_10 *pObjModel10 =
            boardObjIfaceModel10FromBoardObjGet(pBoardObj);

        if (bCopyIn)
        {
            PMU_HALT_OK_OR_GOTO(status,
                ssurfaceRd(pBuf, readOffset + (i * entrySize), entrySize),
                boardObjGrpIfaceModel10GetStatus_SUPER_done);
        }
        else
        {
            memset(pBuf, 0x0, entrySize);
        }

        status = entryFunc(pObjModel10, (RM_PMU_BOARDOBJ *)pBuf);
        if (status != FLCN_OK)
        {
            goto boardObjGrpIfaceModel10GetStatus_SUPER_done;
        }

        PMU_HALT_OK_OR_GOTO(status,
            ssurfaceWr(pBuf, readOffset + (i * entrySize), entrySize),
            boardObjGrpIfaceModel10GetStatus_SUPER_done);
    }
    BOARDOBJGRP_ITERATOR_PTR_END;

boardObjGrpIfaceModel10GetStatus_SUPER_done:
    return status;
}

/*!
 * @brief   Super implementation for @ref BoardObjGrpIfaceModel10GetStatus() interface
 *          using the AutoDMA feature.
 *
 * This means that all all memory transfers must happen before
 * or after this handler function, not during.
 *
 * @copydetails BoardObjGrpIfaceModel10GetStatus()
 */
FLCN_STATUS
boardObjGrpIfaceModel10GetStatusAutoDma_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10                    *pModel10,
    PMU_DMEM_BUFFER                *pBuffer,
    BoardObjGrpIfaceModel10GetStatusHeader      (hdrFunc),
    LwLength                        hdrSize,
    BoardObjIfaceModel10GetStatus                   (entryFunc),
    LwLength                        entrySize,
    LwU32                           ssOffset,
    BOARDOBJGRPMASK                *pMask
)
{
    BOARDOBJGRP *pBoardObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    FLCN_STATUS                     status;
    LwU8                           *pBuf   = pBuffer->pBuf;
    RM_PMU_BOARDOBJGRP_SUPER       *pHdrSuper;
    BOARDOBJ                       *pBoardObj;
    LwBoardObjIdx                   i;
    LwBoardObjIdx                   maskSize;
    RM_PMU_BOARDOBJ                *pLwrBoardObj;

    // Determine the mask size.
    PMU_HALT_OK_OR_GOTO(status,
                        boardObjGrpMaskSizeGet(pBoardObjGrp, &maskSize),
                        boardObjGrpIfaceModel10GetStatus_SUPER_done);

    //
    // Parse the RM mask of objects from the header data.  For now, will trust
    // that RM mask is a subset of the BOARDOBJGRP mask.  @ref
    // _BOARDOBJGRP_ITERATOR_BEGIN() will only iterate over the intersection of
    // the mask with the set of objects actually in the BOARDOBJGRP.
    //
    pHdrSuper = (RM_PMU_BOARDOBJGRP_SUPER *)pBuf;
    boardObjGrpMaskInit_SUPER(pMask, maskSize);

    PMU_HALT_OK_OR_GOTO(status,
                        boardObjGrpMaskImport_FUNC(pMask, maskSize, &pHdrSuper->objMask),
                        boardObjGrpIfaceModel10GetStatus_SUPER_done);

    //
    // Call into hdrFunc() to query global header data and then DMA the header
    // data out to the RM (if required).
    //
    if (hdrFunc != NULL)
    {
        status = hdrFunc(pModel10, (RM_PMU_BOARDOBJGRP *)pBuf, pMask);
        if (status != FLCN_OK)
        {
            goto boardObjGrpIfaceModel10GetStatus_SUPER_done;
        }
    }

    // Parse each entry using entryFunc and DMA the entry data out to the RM.
    pBuf += hdrSize;
    BOARDOBJGRP_ITERATOR_PTR_BEGIN(BOARDOBJ, pBoardObjGrp, pBoardObj, i, pMask)
    {
        BOARDOBJ_IFACE_MODEL_10 *pObjModel10 =
            boardObjIfaceModel10FromBoardObjGet(pBoardObj);

        pLwrBoardObj = (RM_PMU_BOARDOBJ *)(pBuf + (i * entrySize));
        status = entryFunc(pObjModel10, pLwrBoardObj);
        if (status != FLCN_OK)
        {
            goto boardObjGrpIfaceModel10GetStatus_SUPER_done;
        }
    }
    BOARDOBJGRP_ITERATOR_PTR_END;

boardObjGrpIfaceModel10GetStatus_SUPER_done:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10ReadHeader()
 */
FLCN_STATUS
boardObjGrpIfaceModel10ReadHeader
(
    void       *pHeader,
    LwLength    hdrSize,
    LwU32       ssOffset,
    LwU32       readOffset
)
{
    FLCN_STATUS status = FLCN_OK;

    if (pHeader == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto boardObjGrpIfaceModel10ReadHeader_exit;
    }

    readOffset += ssOffset;

    PMU_HALT_OK_OR_GOTO(status,
        ssurfaceRd(pHeader, readOffset, hdrSize),
        boardObjGrpIfaceModel10ReadHeader_exit);

boardObjGrpIfaceModel10ReadHeader_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10WriteHeader()
 */
FLCN_STATUS
boardObjGrpIfaceModel10WriteHeader
(
    void       *pHeader,
    LwLength    hdrSize,
    LwU32       ssOffset,
    LwU32       writeOffset
)
{
    FLCN_STATUS status = FLCN_OK;

    if (pHeader == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto boardObjGrpIfaceModel10WriteHeader_exit;
    }

    writeOffset += ssOffset;

    PMU_HALT_OK_OR_GOTO(status,
        ssurfaceWr(pHeader, writeOffset, hdrSize),
        boardObjGrpIfaceModel10WriteHeader_exit);

boardObjGrpIfaceModel10WriteHeader_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
