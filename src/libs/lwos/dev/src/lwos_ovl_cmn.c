/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    lwos_ovl_cmn.c
 * @brief   Common overlay handling interfaces.
 */

/* ------------------------ System Includes --------------------------------- */
#include "lwoslayer.h"
#include "lwos_ovl.h"
#include "lwos_ovl_priv.h"

/* ------------------------ External Definitions ---------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * Checks if the IMEM overlay specified with overlay index ovlIdx is resident.
 */
#define LWOS_OVL_IMEM_IS_RESIDENT(ovlIdx)                                      \
    LWOS_BM_GET(_imem_ovl_attr_resident, ovlIdx, 8)

/*!
 * Checks if the DMEM overlay specified with overlay index ovlIdx is resident.
 */
#define LWOS_OVL_DMEM_IS_RESIDENT(ovlIdx)                                      \
    LWOS_BM_GET(_dmem_ovl_attr_resident, ovlIdx, 8)

/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Public Variables -------------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * Initialize LWOS overlay related data.
 *
 * @return  FLCN_OK     On success
 * @return  Others      Bubbles up errors
 */
FLCN_STATUS
lwosInitOvl(void)
{
    FLCN_STATUS status = FLCN_OK;
#ifdef DMEM_VA_SUPPORTED
    status = lwosInitOvlDmem();
    if (status != FLCN_OK)
    {
        goto lwosInitOvl_exit;
    }

lwosInitOvl_exit:
#endif // DMEM_VA_SUPPORTED
    return status;
}

/*!
 * Attach an IMEM / DMEM overlay to a task by inserting its index in the task's
 * overlay list (in the first free slot found). To simplify error-handling and
 * reduce code-size, this function simply results in a halt when there are no
 * free slots remaining to attach the requested overlay.
 *
 * @param[in]   pxTask
 *      Handle of the task to attach the overlay to.
 * @param[in]   ovlIdx
 *      Index of the overlay to attach.
 * @param[in]   bDmem
 *      Specifying if @ref ovlIdx is an DMEM or IMEM.
 * @param[in]   bHS
 *      Until sharing support is completed, bHS is not used.
 *      Once sharing support is completed:
 *      bHS specifies if the overlay with index @ref ovlIdx
 *      is to be attached in HS or LS.
 *      This parameter is only valid for IMEM overlays since
 *      only IMEM overlays can be HS signed.
 *
 * @return  LW_TRUE     On successful attachment of requested overlay.
 * @return  LW_FALSE    When requested overlay was already attached.
 *
 * @note
 *      This function is not thread-safe! Tasks may freely make calls to it
 *      without additional synchronization provided that no other tasks or ISRs
 *      attach overlays to the task. If overlays may be attached to a task from
 *      a separate task or from an ISR, special care must be taken to attach
 *      the overlays from within critical-sections (when outside an ISR).
 *
 * @note
 *      It is not required to detach each overlay that gets attached to a task.
 *      It is perfectly legal to attach an overlay once and leave it attached
 *      permenantly.
 */
LwBool
lwosTaskOverlayAttach
(
    LwrtosTaskHandle pxTask,
    LwU8             ovlIdx,
    LwBool           bDmem,
    LwBool           bHS
)
{
    PRM_RTOS_TCB_PVT    pTcb;
    LwBool              bFound = LW_FALSE;
    LwU32               ovlCntImem;
    LwU8               *pOvlList;
    LwU8                slot;
    LwU8                i;
    LwU8                listStart = 0;
    LwBool              bResult = LW_FALSE;

    lwrtosTaskGetLwTcbExt(pxTask, (void**)&pTcb);
    ovlCntImem = pTcb->ovlCntImemLS;

    //
    // Callers should never attempt attaching invalid overlay. Valid overlays
    // have IDs in the range [1 .. COUNT].
    //
    if (ovlIdx == OVL_INDEX_ILWALID)
    {
        OS_HALT();
    }

#ifdef HS_OVERLAYS_ENABLED
    ovlCntImem += pTcb->ovlCntImemHS;
#endif

#ifdef DMEM_VA_SUPPORTED
    if (bDmem)
    {
        if (LWOS_OVL_DMEM_IS_RESIDENT(ovlIdx))
        {
            // On the attempt to attach a resident overlay
            goto lwosTaskOverlayAttach;
        }

        //
        // NJ-TODO: DMEM overlay count does not match IMEM overlay count. IMEM
        //          one is equal to ID of the last overlay, while DMEM is last
        //          overlay + 1. We shoud fix this ambigutiy (for IMEM) and till
        //          then correcting DMEM code to it's final form.
        //
        if (ovlIdx >= OVL_COUNT_DMEM())
        {
            OS_HALT();
        }

        pOvlList = &(pTcb->ovlList[ovlCntImem]);
        slot = i = pTcb->ovlCntDmem;
    }
    else
#endif
    {
#ifdef HS_OVERLAYS_ENABLED
        if (LWOS_OVL_IMEM_IS_RESIDENT(ovlIdx))
        {
            // On the attempt to attach a resident overlay
            goto lwosTaskOverlayAttach;
        }
        if (bHS)
        {
            listStart = pTcb->ovlCntImemLS;
        }
#endif

        if (ovlIdx > OVL_COUNT_IMEM())
        {
            OS_HALT();
        }

        pOvlList = &(pTcb->ovlList[0]);
        slot = i = ovlCntImem;
    }

    while (i != listStart)
    {
        i--;

        if (pOvlList[i] == ovlIdx)
        {
            // On the attempt to attach already attached overlay.
            goto lwosTaskOverlayAttach;
        }
        else if (pOvlList[i] == OVL_INDEX_ILWALID)
        {
            slot   = i;
            bFound = LW_TRUE;
        }
    }

    if (bFound)
    {
        pOvlList[slot] = ovlIdx;
    }
    else
    {
        // No free slots remaining.
        OS_HALT();
    }

    //
    // Make sure that the task loader reevaluates this task the next time
    // it is selected to run (even if it is the lwrrently running task).
    //
    pTcb->reloadMask |= (
#ifdef DMEM_VA_SUPPORTED
        bDmem ? RM_RTOS_TCB_PVT_RELOAD_DMEM :
#endif
                RM_RTOS_TCB_PVT_RELOAD_IMEM);

    bResult = LW_TRUE;

lwosTaskOverlayAttach:
        ;

    return bResult;
}

/*!
 * Attach an IMEM / DMEM overlay to a task by inserting its index in the task's
 * overlay list (in the first free slot found). It halts when an attempt is made
 * to attach an already attached overlay (see @ref lwosTaskOverlayAttach()).
 */
void
lwosTaskOverlayAttachAndFail
(
    LwrtosTaskHandle pxTask,
    LwU8             ovlIdx,
    LwBool           bDmem,
    LwBool           bHS
)
{
    if (!lwosTaskOverlayAttach(pxTask, ovlIdx, bDmem, bHS))
    {
        // On attempt to attach an already attached overlay.
        OS_HALT();
    }
}

/*!
 * Detach an IMEM / DMEM overlay from a task. Reverses the actions taken in
 * @ref lwosTaskOverlayAttach. To simplify error-handling and reduce code-size,
 * this function simply results in a halt when the overlay being detached is
 * not yet attached.
 *
 * @param[in]   pxTask
 *      Handle of the task to attach the overlay to.
 * @param[in]   ovlIdx
 *      Index of the overlay to attach.
 * @param[in]   bDmem
 *      Specifying if @ref ovlIdx is an DMEM or IMEM.
 * @param[in]   bHS
 *      Until sharing support is completed, bHS is not used.
 *      Once sharing support is completed:
 *      If bDmem is true, bHS is ignored.
 *      If bDmem is false, bHS specifies if the overlay with index @ref ovlIdx
 *      was attached in HS or LS.
 * @note
 *      This function is not thread-safe. See notes for @ref
 *      lwosTaskOverlayAttach for additional information.
 */
LwBool
lwosTaskOverlayDetach
(
    LwrtosTaskHandle pxTask,
    LwU8             ovlIdx,
    LwBool           bDmem,
    LwBool           bHS
)
{
    PRM_RTOS_TCB_PVT    pTcb;
    LwU32               ovlCntImem;
    LwU8               *pOvlList;
    LwU8                cnt;
    LwU8                i;
    LwBool              bResult = LW_FALSE;

    lwrtosENTER_CRITICAL();
    {
        lwrtosTaskGetLwTcbExt(pxTask, (void**)&pTcb);
        ovlCntImem = pTcb->ovlCntImemLS;

#ifdef HS_OVERLAYS_ENABLED
        ovlCntImem += pTcb->ovlCntImemHS;
#endif

#ifdef DMEM_VA_SUPPORTED
        if (bDmem)
        {
            pOvlList = &(pTcb->ovlList[ovlCntImem]);
            cnt      = pTcb->ovlCntDmem;
        }
        else
#endif
        {
            pOvlList = &(pTcb->ovlList[0]);
            cnt      = ovlCntImem;
        }

        for (i = 0; i < cnt; i++)
        {
            if (pOvlList[i] == ovlIdx)
            {
                pOvlList[i] = OVL_INDEX_ILWALID;
                if (!bDmem)
                {
#ifdef DMEM_VA_SUPPORTED
#ifndef ON_DEMAND_PAGING_BLK
                    LWOS_BM_CLR(LwosOvlImemLoaded, ovlIdx, 32U);
#endif // ON_DEMAND_PAGING_BLK
#endif // DMEM_VA_SUPPORTED
                }
                bResult = LW_TRUE;
                goto lwosTaskOverlayDetach_exit;
            }
        }

lwosTaskOverlayDetach_exit:
        ;
    }
    lwrtosEXIT_CRITICAL();

    return bResult;
}

/*!
 * Detach an IMEM / DMEM overlay from a task. Reverses the actions taken in
 * @ref lwosTaskOverlayAttach. It halts when an attempt is made to detach an
 * overlay which is not yet attached.
 */
void
lwosTaskOverlayDetachAndFail
(
    LwrtosTaskHandle pxTask,
    LwU8             ovlIdx,
    LwBool           bDmem,
    LwBool           bHS
)
{
    if (!lwosTaskOverlayDetach(pxTask, ovlIdx, bDmem, bHS))
    {
        // Halt on attempt to detach an overlay which is not yet attached.
        OS_HALT();
    }
}

/*!
 * @brief   Processes the overlay descriptor list performing requested action.
 *
 * Iterates the overlay descriptor list in predefined direction and performs
 * desired action while ilwerting it for the reverse pass. Supports conditional
 * attachment/detachment and alters list to prevent detachment if attachment was
 * not done.
 *
 * This interface is intended to be used only through macro wrappers:
 *  @ref OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER()
 *  @ref OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT()
 * For all atypical use-cases add new macro wrappers (i.e. to explicitely
 * provide list size, etc...).
 *
 * @param[in/out]   pOvlDescList    Overlay descriptor list.
 * @param[in]       ovlDescCnt      Size of the @ref pOvlDescList.
 * @param[in]       bReverse        Defines the direction of the iteration.
 *
 * @note    Direct processing alters overlay list so that it can be used during
 *          reverse pass and it has to be reinitialized before new use-case and
 *          therefore cannot be statically allocated.
 *
 * @pre     Caller must provide non-static list of overlay descriptors.
 */
void
lwosTaskOverlayDescListExec_IMPL
(
    OSTASK_OVL_DESC    *pOvlDescList,
    LwU32               ovlDescCnt,
    LwBool              bReverse
)
{
    LwBool bAttached = LW_FALSE;

    if (bReverse)
    {
        // In reverse mode we process entries from the end.
        pOvlDescList += (ovlDescCnt - 1U);
    }

    // Process each descriptor in the list.
    while (ovlDescCnt > 0U)
    {
        OSTASK_OVL_DESC ovlDesc = *pOvlDescList;
        LwU8            ovlIdx  =
            DRF_VAL(_OSTASK, _OVL_DESC, _INDEX, ovlDesc);
        LwBool          bDmem   =
            FLD_TEST_DRF(_OSTASK, _OVL_DESC, _TYPE, _DMEM, ovlDesc);
#if HS_OVERLAYS_ENABLED
        LwBool          bHs     =
            FLD_TEST_DRF(_OSTASK, _OVL_DESC, _MODE, _HS, ovlDesc);
#else
        LwBool bHs              = LW_FALSE;
#endif

        //
        // This can be true in two cases:
        //  - in direct pass when client explicitely skips an overlay (@see
        //      OSTASK_OVL_DESC_NULL)
        //  - in reverse pass when overlay was not attached/detached during direct pass
        //
        if (ovlIdx != OVL_INDEX_ILWALID)
        {
            if (FLD_TEST_DRF(_OSTASK, _OVL_DESC, _ACTION, _ATTACH, ovlDesc))
            {
                if (lwosTaskOverlayAttach(NULL, ovlIdx, bDmem, bHs))
                {
                    bAttached = LW_TRUE;
                }
                else
                {
                    // We modify entry to skip it during second pass.
                    *pOvlDescList = OSTASK_OVL_DESC_ILWALID_VAL;
                }
            }
            else
            {
                if (!lwosTaskOverlayDetach(NULL, ovlIdx, bDmem, bHs))
                {
                    // We modify entry to skip it during second pass.
                    *pOvlDescList = OSTASK_OVL_DESC_ILWALID_VAL;
                }
            }
            // Flip _ACTION for the second pass.
            *pOvlDescList ^= DRF_NUM(_OSTASK, _OVL_DESC, _ACTION, 1);
        }

        if (bReverse)
        {
            // In reverse mode we process entries from the end.
            pOvlDescList--;
        }
        else
        {
            pOvlDescList++;
        }
        ovlDescCnt--;
    }

    if (bAttached)
    {
        lwrtosRELOAD();
    }
}

#ifdef MRU_OVERLAYS

/*!
 * @brief   Removes overlay from the MRU list.
 *
 * @param[in]   ovlIdx  Index of an overlay to remove.
 * @param[in]   bDmem   Specifying if @ref ovlIdx is an DMEM or IMEM.
 *
 * @pre     Implementation assumes that @ref ovlIdx points to a valid overlay
 *          existing within the MRU list (hence not error checking).
 */
void
lwosOvlMruListRemove
(
    LwU8    ovlIdx,
    LwBool  bDmem
)
{
    LwU8   *pPrev;
    LwU8   *pNext;
    LwU8    prev;
    LwU8    next;

#ifdef DMEM_VA_SUPPORTED
    if (bDmem)
    {
        pPrev = _dmem_ovl_mru_prev;
        pNext = _dmem_ovl_mru_next;
    }
    else
#endif
    {
        pPrev = _imem_ovl_mru_prev;
        pNext = _imem_ovl_mru_next;
    }

    prev = pPrev[ovlIdx];
    next = pNext[ovlIdx];

    pNext[prev] = next;
    pPrev[next] = prev;

    //
    // Avoiding writing previous index to reduce IMEM size. Therefore check
    // if an overlay is within the MRU list validates only next index.
    //
    // pPrev[ovlIdx] = OVL_INDEX_ILWALID;
    pNext[ovlIdx] = OVL_INDEX_ILWALID;
}

/*!
 * @brief   Inserts an overlay right after 'head' element.
 *
 * @param[in]   ovlIdx  Index of an overlay to insert.
 * @param[in]   bDmem   Specifying if @ref ovlIdx is an DMEM or IMEM.
 *
 * @pre     Implementation assumes that @ref ovlIdx points to a valid overlay
 *          not existing within the MRU list (hence not error checking).
 */
void
lwosOvlMruListMoveToFront
(
    LwU8    ovlIdx,
    LwBool  bDmem
)
{
#ifdef ON_DEMAND_PAGING_BLK
    return;
#else //ON_DEMAND_PAGING_BLK
    OS_TASK_MRU_RECYCLE_TRACKING   *pTracking;
    LwU8                           *pPrev;
    LwU8                           *pNext;
    LwU8                            oldFirst;

#ifdef DMEM_VA_SUPPORTED
    if (bDmem)
    {
extern OS_TASK_MRU_RECYCLE_TRACKING LwosOvlDmemMruRecycle;
        pTracking = &LwosOvlDmemMruRecycle;
        pPrev     = _dmem_ovl_mru_prev;
        pNext     = _dmem_ovl_mru_next;
    }
    else
#endif // DMEM_VA_SUPPORTED
    {
extern OS_TASK_MRU_RECYCLE_TRACKING LwosOvlImemMruRecycle;
        pTracking = &LwosOvlImemMruRecycle;
        pPrev     = _imem_ovl_mru_prev;
        pNext     = _imem_ovl_mru_next;
    }

    //
    // Overlay is not within MRU list if its next index is OVL_INDEX_ILWALID.
    // Previous index is not explicitely cleared, @see _osTaskOvlListRemove().
    //
    if (pNext[ovlIdx] != OVL_INDEX_ILWALID)
    {
        lwosOvlMruListRemove(ovlIdx, bDmem);
    }
    else if (ovlIdx == pTracking->ovlIdxRecycled)
    {
        //
        // Adding an overlay to the beginning of the MRU list stops recycling
        // its blocks.
        //
        pTracking->ovlIdxRecycled = OVL_INDEX_ILWALID;
        pTracking->tagStart       = 0;
        pTracking->tagEnd         = 0;
    }

    oldFirst = pNext[OVL_INDEX_ILWALID];

    pNext[OVL_INDEX_ILWALID] = ovlIdx;
    pPrev[ovlIdx] = OVL_INDEX_ILWALID;

    pNext[ovlIdx] = oldFirst;
    pPrev[oldFirst] = ovlIdx;
#endif // ON_DEMAND_PAGING_BLK
}

/*!
 * @brief   Inserts multiple overlays at the beginning of the MRU list.
 *
 * @param[in]   pOvlList    List of overlays to be inserted into the MRU list.
 * @param[in]   ovlCnt      Size of overlay list (including invalid entries).
 * @param[in]   bDmem       Specifying if @ref ovlIdx is an DMEM or IMEM.
 */
void
lwosOvlMruListMoveToFrontAll
(
    LwU8   *pOvlList,
    LwU8    ovlCnt,
    LwBool  bDmem
)
{
#ifdef ON_DEMAND_PAGING_BLK
    return;
#else // ON_DEMAND_PAGING_BLK
    OS_TASK_MRU_RECYCLE_TRACKING   *pTracking;
    LwU8                            i;

#ifdef DMEM_VA_SUPPORTED
    if (bDmem)
    {
extern OS_TASK_MRU_RECYCLE_TRACKING LwosOvlDmemMruRecycle;
        pTracking = &LwosOvlDmemMruRecycle;
    }
    else
#endif // DMEM_VA_SUPPORTED
    {
extern OS_TASK_MRU_RECYCLE_TRACKING LwosOvlImemMruRecycle;
        pTracking = &LwosOvlImemMruRecycle;
    }

    pTracking->ovlIdxDoNotRecycle = OVL_INDEX_ILWALID;

    for (i = 0; i < ovlCnt; i++)
    {
        LwU8 ovlIdx = pOvlList[i];

        if (ovlIdx != OVL_INDEX_ILWALID)
        {
            lwosOvlMruListMoveToFront(ovlIdx, bDmem);

            if (pTracking->ovlIdxDoNotRecycle == OVL_INDEX_ILWALID)
            {
                pTracking->ovlIdxDoNotRecycle = ovlIdx;
            }
        }
    }
#endif // ON_DEMAND_PAGING_BLK
}

#endif // MRU_OVERLAYS

/* ------------------------ Private Functions ------------------------------- */
