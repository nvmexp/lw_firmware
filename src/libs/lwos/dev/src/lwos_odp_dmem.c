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
// Length of bitmask in LwU32 words.
#define LWOS_ODP_DMEM_PINNED_BM_LENGTH  8

/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Public Variables -------------------------------- */
/*!
 * Mapping to track DMEM blocks allocated to pinned overlays an those that
 * are available for DMEM on-demand paging.
 *
 * The mapping is as follows:
 *      0: DMEM block is available for on-demand paging.
 *      1: DMEM block is allocated to a lwrrently loaded pinned overlay.
 */
LwU32 LwosOdpDmemPinnedMap[LWOS_ODP_DMEM_PINNED_BM_LENGTH] = { 0 };

/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Static Function Prototypes ---------------------- */
static LwS32  _lwosOvlDmemPinnedOvlLoad(LwU8 ovlIdx);
static LwU32  _lwosOdpDmemBlkLoad(LwU32 vAddr);
static LwBool _lwosOdpDmemBlkVaIsValid(LwU32 vAddr);
static LwU32  _lwosOdpDmemBlkNextGet(LwU32 blockIdx);
static void   _lwosOdpDmemBlkClean(LwU32 blockIdx);

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
#ifdef DMTAG_WAR_1845883
    ct_assert(0);
#endif // DMTAG_WAR_1845883

    return FLCN_OK;
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

    lwosOvlDmemLoadPinnedOverlayList(&ovlIdxStack, 1);

    return status;
}

LwS32
lwosOvlDmemLoadPinnedOverlayList
(
    LwU8 *pOvlList,
    LwU8 ovlCnt
)
{
    LwU32 i;
    LwS32 blocksLoadedTotal = 0;

    // Clear pinned block mapping.
    (void)memset(LwosOdpDmemPinnedMap, 0, sizeof(LwosOdpDmemPinnedMap));

    for (i = 0; i < ovlCnt; i++)
    {
        LwS32 blocksLoaded = _lwosOvlDmemPinnedOvlLoad(pOvlList[i]);
        if (blocksLoaded < 0)
        {
            return blocksLoaded;
        }
        blocksLoadedTotal += blocksLoaded;
    }

    return blocksLoadedTotal;
}

/*!
 * @brief      Handler for DMEM misses to load the missing data.
 *
 * @details    It is expected for DMEM misses to occur when DMEM ODP is enabled.
 *             If LPWR has turned off FB, a miss indicates that a non-preloaded
 *             task before turning off FB needs to execute, so LPWR must be
 *             notified and selected to run to re-enable FB.
 *
 *             Otherwise, the handler records the DMEM address that missed, and
 *             attempts to load it.
 */
void
lwrtosHookDmemMiss(void)
{

    if (lwosDmaSuspensionNoticeISR())
    {
        return;
    }
    else
    {
        LwU32 uxMissAddr;

        uxMissAddr = DRF_VAL(_CMSDEC, _FALCON_EXCI2, _ADDRESS, falc_ldx_i(CSB_REG(_EXCI2), 0));

        if (_lwosOdpDmemBlkVaIsValid(uxMissAddr))
        {
            (void)_lwosOdpDmemBlkLoad(uxMissAddr);
        }
        else
        {
            OS_HALT();
        }
    }
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @brief   Load and pin and overlay into DMEM.
 *
 * @param[in]   ovlIdx  Index of DMEM overlay to be loaded.
 */
static LwS32
_lwosOvlDmemPinnedOvlLoad
(
    LwU8 ovlIdx
)
{
    LwU32 vAddr;
    LwU32 dmtag;
    LwU32 pIdx;
    LwS32 blocksLoaded = 0;

    if (ovlIdx == OVL_INDEX_ILWALID)
    {
        goto _lwosOvlDmemPinnedOvlLoad_exit;
    }

    LwU16 start = DMEM_ADDR_TO_IDX(_dmem_ovl_start_address[ovlIdx]);
    LwU16 end   = start + DMEM_ADDR_TO_IDX(_dmem_ovl_size_lwrrent[ovlIdx] +
                                           (DMEM_BLOCK_SIZE - 1));
    // Load all blocks and update bitmap.
    while (start < end)
    {
        vAddr = DMEM_IDX_TO_ADDR(start);
        LWOS_DMTAG(dmtag, vAddr);
        if (DMTAG_IS_MISS(dmtag) || DMTAG_IS_ILWALID(dmtag))
        {
            {
                if (lwosDmaIsSuspended())
                {
                    return -1;
                }
            }
            pIdx = _lwosOdpDmemBlkLoad(vAddr);

            blocksLoaded++;
        }
        else
        {
            pIdx = DMTAG_IDX_GET(dmtag);
        }

        LWOS_BM_SET(LwosOdpDmemPinnedMap, pIdx, 32);
        start++;
    }
    falc_dmwait();

_lwosOvlDmemPinnedOvlLoad_exit:
    return blocksLoaded;
}

/*!
 * @brief   Load a DMEM address into physical memory.
 *
 * @param[in]   vAddr   Address to be loaded.
 *
 * @return  The physical DMEM block index.
 */
static LwU32
_lwosOdpDmemBlkLoad
(
    LwU32 vAddr
)
{
    static LwU32 uxLwrrentBlockIdxPrev = (LwU32)_dmem_va_bound;
    static LwU32 uxLwrrentBlockIdxNext = (LwU32)_dmem_va_bound + 1;

    uxLwrrentBlockIdxPrev = uxLwrrentBlockIdxNext;
    uxLwrrentBlockIdxNext = _lwosOdpDmemBlkNextGet(uxLwrrentBlockIdxNext);

    // Read the desired address into DMEM.
    falc_dmread(vAddr,
                DRF_NUM(_FLCN, _DMREAD, _PHYSICAL_ADDRESS,
                        DMEM_IDX_TO_ADDR(uxLwrrentBlockIdxPrev)) |
                DRF_DEF(_FLCN, _DMREAD, _ENC_SIZE, _256)         |
                DRF_DEF(_FLCN, _DMREAD, _SET_TAG, _YES));

    _lwosOdpDmemBlkClean(uxLwrrentBlockIdxNext);

    return uxLwrrentBlockIdxPrev;
}

/*!
 * @brief   Checks if referenced VA is valid data/stack address.
 *
 * @param[in]   vAddr   Address that triggered miss.
 *
 * @return  Boolean if VA is valid or not.
 */
static LwBool
_lwosOdpDmemBlkVaIsValid
(
    LwU32 vAddr
)
{
    LwU32 ovlIdxMin = 0;
    LwU32 ovlIdxMax = OVL_COUNT_DMEM() - 1;
    LwU32 ovlIdxMid;

    while (ovlIdxMin != ovlIdxMax)
    {
        ovlIdxMid = (ovlIdxMin + ovlIdxMax + 1) / 2;

        if (vAddr < _dmem_ovl_start_address[ovlIdxMid])
        {
            ovlIdxMax = ovlIdxMid - 1;
        }
        else
        {
            ovlIdxMin = ovlIdxMid;
        }
    }

    if (ovlIdxMin == OVL_INDEX_OS_HEAP)
    {
        return LW_FALSE;
    }

    if ((vAddr - _dmem_ovl_start_address[ovlIdxMin]) >=
        _dmem_ovl_size_lwrrent[ovlIdxMin])
    {
        return LW_FALSE;
    }

    return LW_TRUE;
}

/*!
 * @brief   Lookup next DMEM block availabe for ODP.
 *
 * @param[in]   blockIdx    Previous block used for ODP.
 *
 * @return  The physical DMEM block index.
 */
static LwU32
_lwosOdpDmemBlkNextGet
(
    LwU32 blockIdx
)
{
    do
    {
        blockIdx++;
        if ((blockIdx + 1) >= (LwU32)_num_dmem_blocks)
        {
            blockIdx = (LwU32)_dmem_va_bound;
        }
    }
    while (LWOS_BM_GET(LwosOdpDmemPinnedMap, blockIdx, 32));

    return blockIdx;
}

/*!
 * @brief   Write back dirty data from a DMEM block.
 *
 * @param[in]   blockIdx    Index of physical DMEM block to be cleaned.
 */
static void
_lwosOdpDmemBlkClean
(
    LwU32 blockIdx
)
{
    LwU32 uxDmBlk;

    // Check status of current block.
    falc_dmblk(&uxDmBlk, blockIdx);

    if (!DMBLK_IS_ILWALID(uxDmBlk))
    {
        if (DMBLK_IS_DIRTY(uxDmBlk))
        {
            falc_dmwrite(DMBLK_TAG_ADDR(uxDmBlk),
                         DRF_NUM(_FLCN, _DMWRITE, _PHYSICAL_ADDRESS,
                                 DMEM_IDX_TO_ADDR(blockIdx))       |
                         DRF_DEF(_FLCN, _DMWRITE, _ENC_SIZE, _256));
        }
        falc_dmilw(blockIdx);
    }
}
