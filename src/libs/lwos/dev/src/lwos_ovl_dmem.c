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
 * @file    lwos_ovl_dmem.c
 * @brief   DMEM specific overlay handling interfaces.
 */

/* ------------------------ System Includes --------------------------------- */
#include "lwoslayer.h"
#include "lwos_ovl.h"
#include "lwos_ovl_priv.h"
#include "linkersymbols.h"
#include "dmemovl.h"

/* ------------------------ External Definitions ---------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Public Variables -------------------------------- */
/*!
 * Global state of the MRU overlay approach shared between multiple routines.
 */
OS_TASK_MRU_RECYCLE_TRACKING LwosOvlDmemMruRecycle = { 0 };

/*!
 * Allocate the memory for the DMEM tag state mask.
 */
LwU32 *pOsDmtagStateMask = NULL;

/* ------------------------ Static Variables -------------------------------- */
static LwU32 LwosOvlDmemFreeBlock = (LwU32)_dmem_va_bound;

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * Initialize LWOS DMEM overlay related data.
 *
 * @return  FLCN_OK                 On success
 * @return  FLCN_ERR_NO_FREE_MEM    If malloc fails
 */
FLCN_STATUS
lwosInitOvlDmem(void)
{
    FLCN_STATUS status = FLCN_OK;

    // Initialize DMEM tag status memory (see bug 1845883).
#ifdef DMTAG_WAR_1845883
    {
        LwU32 maskSize = (((LwU32)_dmem_va_blocks_max + 31) / 32);

        pOsDmtagStateMask = lwosCallocResidentType(maskSize, LwU32);
        if (pOsDmtagStateMask == NULL)
        {
            status = FLCN_ERR_NO_FREE_MEM;
            OS_BREAKPOINT();
            goto lwosInitOvlDmem_exit;
        }
    }

lwosInitOvlDmem_exit:
#endif // DMTAG_WAR_1845883
    return status;
}

/*!
 * @brief   Find next 'available' block that can be used to load overlay data.
 *
 * An 'available' block is defined as a block present in the physical memory
 * belonging to an oldest overlay (by the order of an overlay attachment),
 * while not containing code belonging to the overlays trying to be loaded.
 *
 * @return  Physical DMEM block index of an 'available' block.
 */
LwU16
lwosOvlDmemFindBestBlock(void)
{
    LwU32 result = LwosOvlDmemFreeBlock;

    if (result < (LwU32)_num_dmem_blocks)
    {
        // In initial pass we give away all DMEM blocks dedicated for paging.
        LwosOvlDmemFreeBlock = result + 1U;
    }
    else
    {
        //
        // All initially free blocks were allocated so blocks belonging to the
        // least recently used overlay has to be recycled.
        //
        LwU32   vAddr;
        LwU32   dmtag;
        LwU16   tag;

        while (LW_TRUE)
        {
            while (LwosOvlDmemMruRecycle.tagStart == LwosOvlDmemMruRecycle.tagEnd)
            {
                //
                // NJ-TODO: DMEM overlay count does not match IMEM overlay count. IMEM
                //          one is equal to ID of the last overlay, while DMEM is last
                //          overlay + 1. We should fix this ambiguity (for IMEM) and till
                //          then correcting DMEM code to it's final form.
                //
                LwU8 lastOvl = _dmem_ovl_mru_prev[OVL_COUNT_DMEM()];

                OS_ASSERT(lastOvl != LwosOvlDmemMruRecycle.ovlIdxDoNotRecycle);

                lwosOvlMruListRemove(lastOvl, LW_TRUE);

                LwosOvlDmemMruRecycle.ovlIdxRecycled = lastOvl;
                LwosOvlDmemMruRecycle.tagStart       =
                    DMEM_ADDR_TO_IDX(_dmem_ovl_start_address[lastOvl]);
                LwosOvlDmemMruRecycle.tagEnd         =
                    LwosOvlDmemMruRecycle.tagStart +
                    DMEM_ADDR_TO_IDX(_dmem_ovl_size_lwrrent[lastOvl] +
                                     (DMEM_BLOCK_SIZE - 1));
            }

            tag = --LwosOvlDmemMruRecycle.tagEnd;

            vAddr = DMEM_IDX_TO_ADDR(tag);

            LWOS_DMTAG(dmtag, vAddr);
            if (!DMTAG_IS_MISS(dmtag))
            {
                result = DMTAG_IDX_GET(dmtag);

                //
                // We are passing back a block that could potentially be dirty
                // so it needs to be paged out to maintain consistency
                //
                if (DMTAG_IS_DIRTY(dmtag))
                {
                    //
                    // TODO-AA: Need to talk to HW to see how exactly they
                    // avoided in HW conflicts when simultaneously performing
                    // a read and write to the same block since having
                    // falc_dmwait() here is wasteful
                    // After clarification we should optimize this
                    //
                    falc_dmwrite(vAddr,
                                 DRF_NUM(_FLCN, _DMWRITE, _PHYSICAL_ADDRESS,
                                         DMEM_IDX_TO_ADDR(result))         |
                                 DRF_DEF(_FLCN, _DMWRITE, _ENC_SIZE, _256));
                    falc_dmwait();
                }

                // Mark the miss in array
                DMTAG_WAR_1845883_VALID_CLR(DMEM_ADDR_TO_IDX(vAddr));
                break;
            }
        }
    }

    return (LwU16)result;
}

/*!
 * @brief   Load an overlay into DMEM.
 *
 * @param[in]   ovlIdx  Index of the overlay to load.
 * @param[in]   bDMA    If not set DMAs are skipped and only tags are set.
 *
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
lwosOvlDmemLoad
(
    LwU8    ovlIdx,
    LwBool  bDMA
)
{
    LwU16   tagStart      = DMEM_ADDR_TO_IDX(_dmem_ovl_start_address[ovlIdx]);
    LwU16   tagEnd        = tagStart +
        DMEM_ADDR_TO_IDX(_dmem_ovl_size_lwrrent[ovlIdx] + (DMEM_BLOCK_SIZE - 1U));
    LwS32   blocksLoaded  = 0;

    // Attempt to page-in each block of the requested overlay (if required).
    while (tagEnd > tagStart)
    {
        LwU16   tag   = --tagEnd;
        LwU32   vAddr = DMEM_IDX_TO_ADDR(tag);
        LwU32   dmtag = 0;
        LwU16   blockIdx;

        //
        // If the code address is not in DMEM, load it. If it is in DMEM, if it
        // is a secure block, don't load it since it has not yet been validated.
        // In other cases, check if the block is still valid. If it is invalid,
        // it may be bootloader code blocks whose tags match the code we're
        // trying to load. In that case, load it.
        //
        // TODO-AA: This can be optimized since we always recycle blocks from
        // the end of overlays. If block i is in memory, then blocks 0 to i-1
        // will also be in memory. Thus we can exit early on the first hit.
        // Leaving this a seperate CL to debug less code.
        //

        // Query the status of a fully tagged code address.
        LWOS_DMTAG(dmtag, vAddr);

        if (DMTAG_IS_MISS(dmtag) ||
            (DMTAG_IS_ILWALID(dmtag)))
        {
            {
                if (lwosDmaIsSuspended())
                {
                    return -1;
                }
            }

            // Find a suitable physical DMEM block.
            blockIdx = lwosOvlDmemFindBestBlock();

            if (bDMA)
            {
                LWOS_DMREAD_LINE(vAddr, blockIdx);
            }
            else
            {
                falc_setdtag(vAddr, blockIdx);
            }

            // Mark the tag valid in array
            DMTAG_WAR_1845883_VALID_SET(DMEM_ADDR_TO_IDX(vAddr));
            blocksLoaded++;
        }
    }

    if (bDMA)
    {
        falc_dmwait();
    }
    return blocksLoaded;
}

/*!
 * @brief   Loads multiple overlays into DMEM.
 *
 * @param[in]   pOvlList
 *      List of overlays to be loaded into DMEM. Note that some list entries may
 *      point to OVL_INDEX_ILWALID.
 * @param[in]   ovlCnt
 *      Size of overlay list (including invalid entries).
 *
 * @return  A strictly-negative integer if the overlay could not be loaded.
 *          In this case, the scheduler must select a new task to run.
 * @return  Zero if the overlay was previously loaded (100%).
 * @return  A strictly-postive integer if the task was successfully loaded.
 *
 * @note    This function assures that all valid overlays from pOvlList will
 *          reside in the physical memory, and it does not guaranteee that
 *          anything else (beside resident code) will remain paged in.
 * @note    DMEM overlays always use MRU_LIST so code is not conditional.
 */
LwS32
lwosOvlDmemLoadList
(
    LwU8   *pOvlList,
    LwU8    ovlCnt
)
{
    LwS32   blocksLoadedTotal = 0;
    LwU8    i;

    // Sanity check input.
    if ((NULL == pOvlList) ||
        (0U == ovlCnt))
    {
        OS_HALT();
        return -1; // Coverity
    }

    // In first pass target overlays are moved to the beginning of the MRU list.
    lwosOvlMruListMoveToFrontAll(pOvlList, ovlCnt, LW_TRUE);

    // In second pass target overlays are loaded into the physical memory.
    for (i = 0; i < ovlCnt; i++)
    {
        LwU8 ovlIdx = pOvlList[i];

        if (ovlIdx != OVL_INDEX_ILWALID)
        {
            LwS32 blocksLoaded = lwosOvlDmemLoad(ovlIdx, LW_TRUE);

            if (blocksLoaded < 0)
            {
                return blocksLoaded;
            }
            blocksLoadedTotal += blocksLoaded;
        }
    }

    return blocksLoadedTotal;
}

/*!
 * NJ-TODO: Document
 */
FLCN_STATUS
lwosOvlDmemStackSetup
(
    LwU8        ovlIdxStack,
    LwU16      *pStackDepth,
    LwUPtr    **pStack
)
{
    FLCN_STATUS status = FLCN_OK;
    DmemOvlSize stackDepth;

    // Extract stack info from linker-script initialized data.
    *pStack      = (LwUPtr *)_dmem_ovl_start_address[ovlIdxStack];

    //
    // Depending on build, DMEM overlay sizes may be greater than 16-bits. The
    // stack should never be, however, so catch an error case where we'd
    // truncate to 16-bits before assigning to *pStackDepth.
    //
    stackDepth = _dmem_ovl_size_max[ovlIdxStack] / sizeof(LwUPtr);
    if (stackDepth > LW_U16_MAX)
    {
        OS_BREAKPOINT();
        return FLCN_ERR_ILWALID_STATE;
    }

    *pStackDepth = (LwU16)stackDepth;

    //
    // Declare an overlay 100% used (required by @ref lwosOvlDmemLoad()).
    // TODO-Sahil: Remove once stack overlays are auto-generated.
    //
    _dmem_ovl_size_lwrrent[ovlIdxStack] = _dmem_ovl_size_max[ovlIdxStack];

    //
    // Stack will get initialized so assure that the respective stack DMEM
    // overlay is paged into the physical memory before accessing it.
    //

    lwosOvlMruListMoveToFront(ovlIdxStack, LW_TRUE);

    LwosOvlDmemMruRecycle.ovlIdxDoNotRecycle = ovlIdxStack;

    if (0 > lwosOvlDmemLoad(ovlIdxStack, LW_FALSE))
    {
        // At init time this should never happen (carefull on task restart).
        status = FLCN_ERR_ILWALID_STATE;
    }

    return status;
}
/* ------------------------ Private Functions ------------------------------- */
