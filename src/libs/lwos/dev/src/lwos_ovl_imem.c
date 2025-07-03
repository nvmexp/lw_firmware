/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    lwos_ovl_imem.c
 * @brief   IMEM specific overlay handling interfaces.
 */

/* ------------------------ System Includes --------------------------------- */
#include "lwoslayer.h"
#include "lwos_ovl.h"
#include "lwos_ovl_priv.h"
#ifdef HS_OVERLAYS_ENABLED
#include "lwosselwreovly.h"
#endif // HS_OVERLAYS_ENABLED
#include "linkersymbols.h"

/* ------------------------ External Definitions ---------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Public Variables -------------------------------- */
/*!
 * Global state of the MRU overlay approach shared between multiple routines.
 */
#ifdef MRU_OVERLAYS
OS_TASK_MRU_RECYCLE_TRACKING LwosOvlImemMruRecycle = { 0 };
#endif // MRU_OVERLAYS

/*!
 * Tracking lwrrently loaded IMEM overlays.
 *
 * NJ-TODO: Change the name of the feature.
 */
#ifdef DMEM_VA_SUPPORTED
LwU32 LwosOvlImemLoaded[8] = { 0 };
#endif

/* ------------------------ Static Variables -------------------------------- */
#ifdef MRU_OVERLAYS
static LwU32 LwosOvlImemFreeBlock = (LwU32)_overlay_base_imem;
#endif // MRU_OVERLAYS

/* ------------------------ Static Function Prototypes ---------------------- */
static LwU16  _lwosOvlImemFindBestBlock(LwU8 *pOvlList, LwU8 ovlCnt);
static LwBool _lwosOvlImemBlockIsInKeepList(LwU8 *pOvlList, LwU8 ovlCnt, LwU16 blockTag);

#ifdef HS_UCODE_ENCRYPTION
static void _lwosImemBaseRead(LwU32 *pImb, LwU32 *pImb1);
static void _lwosImemBaseWrite(LwU32 imb, LwU32 imb1);
static void _lwosImemBaseSetEncrypted(LwU32 imb, LwU32 imb1, LwU8 ovlIdx);
#else // HS_UCODE_ENCRYPTION
#define _lwosImemBaseRead(pImb, pImb1)                      lwosNOP()
#define _lwosImemBaseWrite(imb, imb1)                       lwosNOP()
#define _lwosImemBaseSetEncrypted(imb, imb1, ovlIdx)        lwosNOP()
#endif // HS_UCODE_ENCRYPTION

#ifdef ON_DEMAND_PAGING_OVL_IMEM
static void _lwosOvlImemDetachAll(PRM_RTOS_TCB_PVT pTcb);
#endif // ON_DEMAND_PAGING_OVL_IMEM

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief   Load an overlay into IMEM.
 *
 * @param[in]   ovlIdx
 *      Index of the overlay to attach.
 * @param[in]   pOvlList
 *      List of overlays that cannot be evicted from IMEM, that matches the list
 *      of overlays lwrrently being loaded by @ref lwosOvlImemLoadList.
 *      Note that some list entries may point to OVL_INDEX_ILWALID.
 * @param[in]   ovlCnt
 *      Size of overlay list (including invalid entries).
 * @param[in]   bHS
 *      Indicates whether the overlay to be loaded in HS or in LS.
 * @note
 *      It's the user's responsibility to assert secure bit as needed
 *      before calling this function.
 * @return
 *      A strictly-negative integer if the overlay could not be loaded due to
 *      suspended DMA operations; zero if the overlay was previously loaded
 *      (100%); a strictly-positive integer if the overlay or a portion of the
 *      overlay was loaded from FB/SYSMEM (over DMA).
 *
 *      In the latter two cases it may be assumed that the entire overlay is
 *      intact and may be switched by the scheduler as the "RUNNING" task.
 */
LwS32
lwosOvlImemLoad
(
    LwU8    ovlIdx,
    LwU8   *pOvlList,
    LwU8    ovlCnt,
    LwBool  bHS
)
{
    LwU16   tagStart      = _imem_ovl_tag[ovlIdx];
    LwU16   tagEnd        = tagStart + _imem_ovl_size[ovlIdx];
    LwS32   blocksLoaded  = 0;
    LwBool  bHSOvlEnabled = LW_FALSE;
#ifdef HS_UCODE_ENCRYPTION
    LwU32   imb           = 0;
    LwU32   imb1          = 0;
#endif

    // Just making subsequent code more readable.
#ifdef HS_OVERLAYS_ENABLED
    bHSOvlEnabled = LW_TRUE;

    // Note that overlay IDs 1..._num_imem_hs_overlays are heavy secure
    if (bHSOvlEnabled && bHS)
    {
#ifdef HS_UCODE_ENCRYPTION
        _lwosImemBaseRead(&imb, &imb1);
        _lwosImemBaseSetEncrypted(imb, imb1, ovlIdx);
#endif // HS_UCODE_ENCRYPTION

        //
        // Setup SVEC register to assert secure bit
        // (no need to set up the encryption bit now, we'll do it when we load
        // the signature)
        //
        falc_wspr(SEC, SVEC_REG(0, 0, 1, 0));
    }
#endif // HS_OVERLAYS_ENABLED

    // Attempt to page-in each block of the requested overlay (if required).
    while (tagEnd > tagStart)
    {
        LwU32   vAddr = IMEM_IDX_TO_ADDR(--tagEnd);
        LwU32   imtag;
        LwU16   blockIdx;

        // Query the status of a fully tagged code address.
        falc_imtag(&imtag, vAddr);

        //
        // If the code address is not in IMEM, load it. If it is in IMEM, if it
        // is a secure block, don't load it since it has not yet been validated.
        // In other cases, check if the block is still valid. If it is invalid,
        // it may be bootloader code blocks whose tags match the code we're
        // trying to load. In that case, load it.
        //
        if (IMTAG_IS_MISS(imtag) ||
            (IMTAG_IS_ILWALID(imtag) &&
             !(bHSOvlEnabled && IMTAG_IS_SELWRE(imtag))))
        {
            {
                if (lwosDmaIsSuspended())
                {
                    return -1;
                }
            }

            // Find a suitable physical IMEM block.
            blockIdx = _lwosOvlImemFindBestBlock(pOvlList, ovlCnt);

            falc_imread(vAddr, IMEM_IDX_TO_ADDR(blockIdx));
            blocksLoaded++;
        }
#ifdef HS_OVERLAYS_ENABLED
        else if (!IMTAG_IS_MISS(imtag) && (bHS != IMTAG_IS_SELWRE(imtag)))
        {
            //
            // The block is loaded in physical memory but it's secure bit is
            // not the one we want, so refresh it.
            //
            blockIdx = IMTAG_IDX_GET(imtag);
            falc_imread(vAddr, IMEM_IDX_TO_ADDR(blockIdx));
        }
#ifdef HS_UCODE_ENCRYPTION
        else if (!IMTAG_IS_MISS(imtag) && IMTAG_IS_SELWRE(imtag))
        {
            //
            // The block is loaded in physical memory and is tagged as secure,
            // however, it is sitting in memory decrypted - so force reload it.
            //
            blockIdx = IMTAG_IDX_GET(imtag);
            falc_imread(vAddr, IMEM_IDX_TO_ADDR(blockIdx));
        }
#endif // HS_UCODE_ENCRYPTION
#endif // HS_OVERLAYS_ENABLED
    }

    falc_imwait();

#ifdef HS_OVERLAYS_ENABLED
    // Note that overlay IDs 1..._num_imem_hs_overlays are heavy secure
    if (bHSOvlEnabled && bHS)
    {
#ifdef HS_UCODE_ENCRYPTION
        _lwosImemBaseWrite(imb, imb1);
#endif

        // De-assert secure bit if it was set
        falc_wspr(SEC, SVEC_REG(0, 0, 0, 0));
    }
#endif

    return blocksLoaded;
}

/*!
 * @brief   Loads multiple overlays into IMEM.
 *
 * @param[in]   pOvlList
 *      List of overlays to be loaded into IMEM. Note that some list entries may
 *      point to OVL_INDEX_ILWALID.
 * @param[in]   ovlTotalCntImem
 *      Size of overlay list (including invalid entries).
 * @param[in]   bHS
 *      bHS specifies if the overlays are to be loaded in HS or LS.
 *
 * @return  A strictly-negative integer if the overlay could not be loaded.
 *          In this case, the scheduler must select a new task to run.
 * @return  Zero if the overlay was previously loaded (100%).
 * @return  A strictly-postive integer if the task was successfully loaded.
 *
 * @note    This function assures that all valid overlays from pOvlList will
 *          reside in the physical memory, and it does not guaranteee that
 *          anything else (beside resident code) will remain paged in.
 */
LwS32
lwosOvlImemLoadList
(
    LwU8   *pOvlList,
    LwU8    ovlTotalCntImem,
    LwBool  bHS
)
{
    LwS32   blocksLoadedTotal = 0;
    LwU8    i;

    // Sanity check input.
    if ((NULL == pOvlList) ||
        (0U == ovlTotalCntImem))
    {
        OS_HALT();
        return -1; // Coverity
    }

    // In first pass target overlays are moved to the beginning of the MRU list.
#ifdef MRU_OVERLAYS
    lwosOvlMruListMoveToFrontAll(pOvlList, ovlTotalCntImem, LW_FALSE);
#endif

    // In second pass target overlays are loaded into the physical memory.
    for (i = 0; i < ovlTotalCntImem; i++)
    {
        LwU8 ovlIdx = pOvlList[i];

        if (ovlIdx != OVL_INDEX_ILWALID)
        {
#ifdef DMEM_VA_SUPPORTED
            if (!LWOS_BM_GET(LwosOvlImemLoaded, ovlIdx, 32U))
#endif
            {
                LwS32 blocksLoaded = lwosOvlImemLoad(ovlIdx, pOvlList, ovlTotalCntImem, bHS);

                if (blocksLoaded < 0)
                {
                    return blocksLoaded;
                }
                blocksLoadedTotal += blocksLoaded;

#ifdef DMEM_VA_SUPPORTED
                LWOS_BM_SET(LwosOvlImemLoaded, ovlIdx, 32U);
#endif
            }
        }
    }

    return blocksLoadedTotal;
}

#ifdef DMEM_VA_SUPPORTED
#ifdef HS_OVERLAYS_ENABLED
/*!
 * @brief Load all resident HS overlays into memory
 * @note  This function should be called before any other overlays are loaded.
 */
void
lwosOvlImemLoadAllResidentHS(void)
{
    LwU32           i;
    LwU16           lwrrBlockIdx;
    static LwU32    startAddrPhys = (LwU32)_ls_resident_end_imem;
#ifdef HS_UCODE_ENCRYPTION
    LwU32           imb;
    LwU32           imb1;
    _lwosImemBaseRead(&imb, &imb1);
#endif

    lwrrBlockIdx  = IMEM_ADDR_TO_IDX(startAddrPhys);

    //
    // Set the secure bit
    // (no need to set up the encryption bit now, we'll do it when we load the
    // signature)
    //
    falc_wspr(SEC, SVEC_REG(0, 0, 1, 0));

    // Load all resident HS overlays
    for (i = (OVL_INDEX_ILWALID + 1); i < OVL_COUNT_IMEM_HS(); i++)
    {
        if (OVL_IMEM_IS_RESIDENT(i))
        {
#ifdef HS_UCODE_ENCRYPTION
            _lwosImemBaseSetEncrypted(imb, imb1, i);
#endif // HS_UCODE_ENCRYPTION

            LwU16   tagStart = _imem_ovl_tag[i];
            LwU16   tagEnd   = tagStart + _imem_ovl_size[i];

            while (tagEnd > tagStart)
            {
                LwU32 vAddr = IMEM_IDX_TO_ADDR(--tagEnd);
                falc_imread(vAddr, IMEM_IDX_TO_ADDR(lwrrBlockIdx++));
            }
        }
    }
    falc_imwait();

    if (lwrrBlockIdx != IMEM_ADDR_TO_IDX((LwU32)_overlay_base_imem))
    {

        //
        // This should never happen assuming our linker script has been
        // correctly set up to callwlate the overlay IMEM base and the size of
        // resident HS overlays. Try to catch cases where someone potentially
        // brings it out of sync.
        //
        OS_HALT();
    }

#ifdef HS_UCODE_ENCRYPTION
    _lwosImemBaseWrite(imb, imb1);
#endif

    // Clear the secure bit
    falc_wspr(SEC, SVEC_REG(0, 0, 0, 0));
}
#endif // HS_OVERLAYS_ENABLED
#endif // DMEM_VA_SUPPORTED

#ifdef ON_DEMAND_PAGING_OVL_IMEM
/*!
 * @brief   Implements on-demand paging for IMEM overlays.
 *
 * On finding the missing overlay, it detaches all overlays from current task
 * and attaches the newly found missing overlay. This function expects a context
 * switch after its done so that the new overlay added to pOvlList gets loaded
 * into IMEM.
 *
 * @param[in]   exPc    PC causing an IMEM miss exception.
 */
void
lwosOvlImemOnDemandPaging
(
    LwU32 exPc
)
{
    PRM_RTOS_TCB_PVT pTcb;
    LwU32 ovlIdx   = 0;
    LwU32 index    = 0;
    LwU32 exPcTag  = 0;

    lwrtosTaskGetLwTcbExt(NULL, (void **)&pTcb);

    // Check if IMEM overlay on-demand paging is enabled for the current task.
    if (!(pTcb->bPagingOnDemandImem))
    {
        OS_HALT();
    }

    // Get 256B aligned PC.
    exPcTag = IMEM_ADDR_TO_IDX(exPc);

    //
    // Find the IMEM overlay index of this PC.
    // TODO: Optimize with binary-search.
    //
    for (index = 0; index <= ((LwU32)_num_imem_overlays); index++)
    {
        //
        // Compare tag to starting block of overlay and PC to end of overlay,
        // to see if there's a match. We must compare against end address,
        // not tag, since we may share last block with another overlay.
        // 
        if ((exPcTag >= ((LwU32)_imem_ovl_tag[index])) &&
            (exPc < _imem_ovl_end_addr[index]))
        {
            ovlIdx = index;
            break;
        }
    }

    // HALT in case overlay is not found.
    if (index > ((LwU32)_num_imem_overlays))
    {
        OS_HALT();
    }

    //
    // Detach all lwrrently attached overlays.
    // TODO: Find out optimal list of overlays to be detached.
    //
    _lwosOvlImemDetachAll(pTcb);

    // Now attach the overlay corresponding to the missing PC.
    OSTASK_ATTACH_IMEM_OVERLAY(ovlIdx);
}
#endif // ON_DEMAND_PAGING_OVL_IMEM

#ifdef DMEM_VA_SUPPORTED
#ifdef HS_OVERLAYS_ENABLED
/*!
 * @brief   Refresh IMEM overlay(s) with a change in security level.
 *
 * @param[in]   pOvlList  List of index(es) of the IMEM overlay(s) to be refreshed.
 * @param[in]   ovlCnt    Number of the IMEM overlay(s) to be refreshed
 * @param[in]   bIntoHs   Specifies if the security level is to be changed to HS.
 *
 * @note    This function should only be called inside a critical section.
 */
void
lwosOvlImemRefresh
(
    LwU8   *pOvlList,
    LwU8    ovlCnt,
    LwBool  bIntoHs
)
{
    LwU8 i = 0;

    // Sanity check input.
    if ((NULL == pOvlList) ||
        (0 == ovlCnt))
    {
        OS_HALT();
        return; // Pacify Coverity.
    }

    if (bIntoHs)
    {
        falc_wspr(SEC, SVEC_REG(0, 0, 1, 0));
    }

    // Refresh IMEM overlays
    for (i = 0; i < ovlCnt; ++i)
    {
        LwU8  ovlIdx = pOvlList[i];
        LwU16 tagStart;
        LwU16 tagEnd;

        if (ovlIdx == OVL_INDEX_ILWALID)
        {
            OS_HALT();
        }

        // TODO-ssapre: Check if the IMEM overlay is resident.
        tagStart = _imem_ovl_tag[ovlIdx];
        tagEnd = tagStart + _imem_ovl_size[ovlIdx];

        while (tagEnd > tagStart)
        {
            LwU32   vAddr = IMEM_IDX_TO_ADDR(--tagEnd);
            LwU32   imtag;
            LwU16   blockIdx;

            // Query the status of a fully tagged code address.
            falc_imtag(&imtag, vAddr);

            if (!(IMTAG_IS_MISS(imtag)))
            {
                blockIdx = IMTAG_IDX_GET(imtag);
                falc_imread(vAddr, IMEM_IDX_TO_ADDR(blockIdx));
            }
            else
            {
                //
                // We should NEVER hit miss since the overlay is resident,
                // but adding this as a precaution
                //
                OS_HALT();
            }
        }
    }

    if (bIntoHs)
    {
        falc_wspr(SEC, SVEC_REG(0, 0, 0, 0));
    }
}
#endif // HS_OVERLAYS_ENABLED
#endif // DMEM_VA_SUPPORTED

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @brief   Find next 'available' block that can be used to load overlay code.
 *
 * #ifdef MRU_OVERLAYS
 *
 * An 'available' block is defined as a block present in the physical memory
 * belonging to an oldest overlay (by the order of an overlay attachment),
 * while not containing code belonging to the overlays trying to be loaded.
 *
 * @param[in]   pOvlList    Not used.
 * @param[in]   ovlCnt      Not used.
 *
 * #else // MRU_OVERLAYS
 *
 * An 'available' block is defined as a block that:
 *   - Is invalid (i.e. not mapped to a tag)
 *   - Does not contain code part of the overlays trying to be loaded.
 *
 * Blocks are traversed in a round-robin fashion. The next block searched will
 * be the block logically after the previous block returned by this function
 * (taking into account wrap-around).
 *
 * @param[in]   pOvlList
 *      List of overlays that cannot be evicted from IMEM, that matches the list
 *      of overlays lwrrently being loaded by @ref lwosOvlImemLoadList.
 *      Note that some list entries may point to OVL_INDEX_ILWALID.
 * @param[in]   ovlCnt
 *      Size of overlay list (including invalid entries).
 *
 * #endif // MRU_OVERLAYS
 *
 * @return  Physical memory block index of an 'available' block.
 */
static LwU16
_lwosOvlImemFindBestBlock
(
    LwU8   *pOvlList,
    LwU8    ovlCnt
)
#ifdef MRU_OVERLAYS
{
    LwU16 result = IMEM_ADDR_TO_IDX(LwosOvlImemFreeBlock);

    if (result < (LwU32)_num_imem_blocks)
    {
        // In initial pass we give away all IMEM blocks dedicated for paging.
        LwosOvlImemFreeBlock = IMEM_IDX_TO_ADDR(result + 1U);
    }
    else
    {
        //
        // All initially free blocks were allocated so blocks belonging to the
        // least recently used overlay has to be recycled.
        //
        LwU32   vAddr;
        LwU32   imtag;
        LwU16   tag;

        while (LW_TRUE)
        {
            if (LwosOvlImemMruRecycle.tagStart == LwosOvlImemMruRecycle.tagEnd)
            {
                LwU8 lastOvl = _imem_ovl_mru_prev[OVL_COUNT_IMEM() + 1U];

                OS_ASSERT(lastOvl != LwosOvlImemMruRecycle.ovlIdxDoNotRecycle);

                lwosOvlMruListRemove(lastOvl, LW_FALSE);

                LwosOvlImemMruRecycle.ovlIdxRecycled = lastOvl;
                LwosOvlImemMruRecycle.tagStart       = _imem_ovl_tag[lastOvl];
                LwosOvlImemMruRecycle.tagEnd         =
                    LwosOvlImemMruRecycle.tagStart + _imem_ovl_size[lastOvl];
            }

            tag = --LwosOvlImemMruRecycle.tagEnd;

            vAddr = IMEM_IDX_TO_ADDR(tag);

            falc_imtag(&imtag, vAddr);

            if (!(IMTAG_IS_MISS(imtag) ||
                _lwosOvlImemBlockIsInKeepList(pOvlList, ovlCnt, tag)))
            {
                result = IMTAG_IDX_GET(imtag);
                break;
            }
        }
    }

    return result;
}
#else // MRU_OVERLAYS
{
    //
    // Resident code oclwpies some contiguous number of low blocks. All other
    // blocks are free for overlay code. Thus, blocks [N, COUNT-1] can be
    // searched and returned by this function. This function keeps track of the
    // start of the overlay blocks and a pointer to the next block to search.
    // These values are stored as physical IMEM addresses.
    //
    static LwU32 rrStartAddr   = (LwU32)_overlay_base_imem;
    static LwU32 rrPointerAddr = (LwU32)_overlay_base_imem;
    LwU16        lwrrBlockIdx  = IMEM_ADDR_TO_IDX(rrPointerAddr);
    LwU16        nextBlockIdx;
    LwBool       bFound = LW_FALSE;

    do
    {
        LwU32   blockInfo;
        LwU16   tagIdx;

        // Get the block information for the next block to consider.
        nextBlockIdx = lwrrBlockIdx + 1;
        if (nextBlockIdx == (LwU32)_num_imem_blocks)
        {
            nextBlockIdx = IMEM_ADDR_TO_IDX(rrStartAddr);
        }

        falc_imblk(&blockInfo, lwrrBlockIdx);

        // If the block is invalid, use it without further consideration.
        if (IMBLK_IS_ILWALID(blockInfo))
        {
            bFound = LW_TRUE;
            break;
        }

        tagIdx = (LwU16)IMEM_ADDR_TO_IDX(IMBLK_TAG_ADDR(blockInfo));

        // Check if the code in the current block can be evicted.
        if (!_lwosOvlImemBlockIsInKeepList(pOvlList, ovlCnt, tagIdx))
        {
            bFound = LW_TRUE;
            break;
        }

        // Current block is not available, try next one.
        lwrrBlockIdx = nextBlockIdx;
    } while (lwrrBlockIdx != IMEM_ADDR_TO_IDX(rrPointerAddr));

    if (!bFound)
    {
        // We cannot find free physical memory block -> unrecoverable error.
        OS_HALT();
    }

    rrPointerAddr = IMEM_IDX_TO_ADDR(nextBlockIdx);
    return lwrrBlockIdx;
}
#endif // MRU_OVERLAYS

/*!
 * @brief   Determine if the code in the IMEM block is in the keep list.
 *
 * Check the bounds of each valid overlay in the list to see if the code in the
 * current block resides in that overlay. If the code exists in any overlay in
 * the keep-list, the block is not available for eviction.
 *
 * @param[in]   pOvlList
 *      List of overlays that cannot be evicted from IMEM, that matches the list
 *      of overlays lwrrently being loaded by @ref lwosOvlImemLoadList.
 *      Note that some list entries may point to OVL_INDEX_ILWALID.
 * @param[in]   ovlCnt
 *      Size of overlay list (including invalid entries).
 * @param[in]   blockTag
 *      Tag of the IMEM block to be checked.
 *
 * @return  LW_TRUE if the block is in the keep list, LW_FALSE otherwise.
 */
static LwBool
_lwosOvlImemBlockIsInKeepList
(
    LwU8   *pOvlList,
    LwU8    ovlCnt,
    LwU16   blockTag
)
{
    LwU8 i;

    for (i = 0; i < ovlCnt; i++)
    {
        LwU8 ovlIdx = pOvlList[i];

        if (ovlIdx != OVL_INDEX_ILWALID)
        {
            LwU16 tagStart = _imem_ovl_tag[ovlIdx];
            LwU16 tagEnd   = tagStart + _imem_ovl_size[ovlIdx];

            if ((blockTag >= tagStart) &&
                (blockTag <  tagEnd))
            {
                return LW_TRUE;
            }
        }
    }

    return LW_FALSE;
}

#ifdef HS_UCODE_ENCRYPTION

static void
_lwosImemBaseRead
(
    LwU32 *pImb,
    LwU32 *pImb1
)
{
    falc_rspr(*pImb,  IMB);
    falc_rspr(*pImb1, IMB1);
}

static void
_lwosImemBaseWrite
(
    LwU32 imb,
    LwU32 imb1
)
{
    falc_wspr(IMB,  imb);
    falc_wspr(IMB1, imb1);
}

static void
_lwosImemBaseSetEncrypted
(
    LwU32 imb,
    LwU32 imb1,
    LwU8  ovlIdx
)
{
    LwU64 fbAddrAligned = imb | ((LwU64)imb1 << 32);

    fbAddrAligned -= _imem_ovl_tag[ovlIdx];
#ifdef BOOT_FROM_HS
    fbAddrAligned += _patched_imem_ovl_tag[ovlIdx];
#else
    if (OS_SEC_FALC_IS_DBG_MODE())
    {
        // Offset by the dbg key encrypted tag
        fbAddrAligned += _patched_dbg_imem_ovl_tag[ovlIdx];
    }
    else
    {
        // Offset by the prod key encrypted tag
        fbAddrAligned += _patched_prd_imem_ovl_tag[ovlIdx];
    }

#endif
    _lwosImemBaseWrite((LwU32)LwU64_LO32(fbAddrAligned),
                       (LwU32)LwU64_HI32(fbAddrAligned));
}

#endif // HS_UCODE_ENCRYPTION

#ifdef ON_DEMAND_PAGING_OVL_IMEM
/*!
 * @brief   Detach all IMEM overlays from a specified task.
 */
static void
_lwosOvlImemDetachAll
(
    PRM_RTOS_TCB_PVT pTcb
)
{
    LwU32   ovlCntImem;
    LwU8   *pOvlList;
    LwU8    i;
    LwU8    ovlIdx;

    ovlCntImem = pTcb->ovlCntImemLS;

#ifdef HS_OVERLAYS_ENABLED
    ovlCntImem += pTcb->ovlCntImemHS;
#endif // HS_OVERLAYS_ENABLED

    pOvlList = &(pTcb->ovlList[0]);

    for (i = 0; i < ovlCntImem; i++)
    {
        ovlIdx      = pOvlList[i];
        pOvlList[i] = OVL_INDEX_ILWALID;
#ifdef DMEM_VA_SUPPORTED
        if (ovlIdx != OVL_INDEX_ILWALID)
        {
            LWOS_BM_CLR(LwosOvlImemLoaded, ovlIdx, 32);
        }
#endif // DMEM_VA_SUPPORTED
    }
}
#endif // ON_DEMAND_PAGING_OVL_IMEM
