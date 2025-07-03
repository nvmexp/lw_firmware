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

/* ------------------------ External Definitions ---------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
// Length of bitmask in LwU32 words
#define LWOS_ODP_IMEM_PINNED_BM_LENGTH  8

/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Public Variables -------------------------------- */
/*!
 * Mapping to track IMEM blocks allocated to pinned overlays and those that
 * are available for IMEM on-demand paging.
 *
 * The mapping is as follows:
 *      0: IMEM block is available for on-demand paging.
 *      1: IMEM block is allocated to a lwrrently loaded pinned overlay.
 */
LwU32 LwosOdpImemPinnedMap[LWOS_ODP_IMEM_PINNED_BM_LENGTH] = { 0 };

/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Static Function Prototypes ---------------------- */
static LwU32  _lwosOdpImemBlkLoad(LwU32 vAddr);
static LwS32  _lwosOdpImemPinnedOvlLoad(LwU8 ovlIdx);
static LwBool _lwosOdpImemBlkVaIsValid(LwU32 vAddr);
static LwU32  _lwosOdpImemBlkNextGet(LwU32 blockIdx);

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Load a list of overlays info IMEM and mark them as pinned.
 *
 * @param[in]   pOvlList
 *      List of overlays to be loaded into IMEM. Note that some list entries may
 *      point to OVL_INDEX_ILWALID.
 * @param[in]   ovlCnt
 *      Size of overlay list (including invalid entries).
 */
LwS32
lwosOvlImemLoadPinnedOverlayList
(
    LwU8 *pOvlList,
    LwU8 ovlCnt
)
{
    LwU32 i;
    LwS32 blocksLoadedTotal = 0;

    // Clear pinned block mapping
    (void)memset(LwosOdpImemPinnedMap, 0, sizeof(LwosOdpImemPinnedMap));

    // Load and add the mapping for all overlays.
    for (i = 0; i < ovlCnt; i++)
    {
        LwS32 blocksLoaded = _lwosOdpImemPinnedOvlLoad(pOvlList[i]);
        if (blocksLoaded < 0)
        {
            return blocksLoaded;
        }
        blocksLoadedTotal += blocksLoaded;
    }

    return blocksLoadedTotal;
}

/*!
 * @brief      Handler for IMEM misses to load the missing data.
 *
 * @details    It is expected for IMEM misses to occur when IMEM ODP is enabled.
 *             If LPWR has turned off FB, a miss indicates that a non-preloaded
 *             task before turning off FB needs to execute, so LPWR must be
 *             notified and selected to run to re-enable FB.
 *
 *             Otherwise, the handler records the IMEM address that missed, and
 *             attempts to load the correct, missing block.
 */
void
lwrtosHookImemMiss(void)
{
    if (lwosDmaSuspensionNoticeISR())
    {
        return;
    }
    else
    {
        LwU32 uxMissAddr;
        LwU32 iTag;

        uxMissAddr = DRF_VAL(_CMSDEC, _FALCON_EXCI2, _ADDRESS, falc_ldx_i(CSB_REG(_EXCI2), 0));

        falc_imtag(&iTag, uxMissAddr);

        if (!IMTAG_IS_MISS(iTag))
        {
            // Instruction spanned block boundary, so point to the next block.
            uxMissAddr = IMEM_IDX_TO_ADDR(IMEM_ADDR_TO_IDX(uxMissAddr) + 1);
        }

        if (_lwosOdpImemBlkVaIsValid(uxMissAddr))
        {
            (void)_lwosOdpImemBlkLoad(uxMissAddr);
        }
        else
        {
            OS_HALT();
        }
    }
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @brief   Load and pin an overlay into IMEM.
 *
 * @param[in]   ovlIdx      Index of IMEM overlay to be loaded.
 */
static LwS32
_lwosOdpImemPinnedOvlLoad
(
    LwU8 ovlIdx
)
{
    LwU32 vAddr;
    LwU32 imtag;
    LwU32 pIdx;
    LwS32 blocksLoaded = 0;

    if (ovlIdx == OVL_INDEX_ILWALID)
    {
        goto _lwosOdpImemPinnedOvlLoad_exit;
    }

    LwU32 start = _imem_ovl_tag[ovlIdx];
    LwU32 end   = start + _imem_ovl_size[ovlIdx];

    // Load all blocks and update bitmap.
    while (start < end)
    {
        vAddr = IMEM_IDX_TO_ADDR(start);
        falc_imtag(&imtag, vAddr);
        if (IMTAG_IS_MISS(imtag) || IMTAG_IS_ILWALID(imtag))
        {
            {
                if (lwosDmaIsSuspended())
                {
                    return -1;
                }
            }
            pIdx = _lwosOdpImemBlkLoad(vAddr);
            blocksLoaded++;
        }
        else
        {
            pIdx = IMTAG_IDX_GET(imtag);
        }

        LWOS_BM_SET(LwosOdpImemPinnedMap, pIdx, 32);
        start++;
    }
    falc_imwait();

_lwosOdpImemPinnedOvlLoad_exit:
    return blocksLoaded;
}

/*!
 * @brief   Load an IMEM address into physical memory.
 *
 * @param[in]   vAddr   Address to be loaded.
 *
 * @return      The physical IMEM block index.
 */
static LwU32
_lwosOdpImemBlkLoad
(
    LwU32 vAddr
)
{
    static LwU32 uxPointerAddrIdx = (LwU32)_overlay_base_imem_idx;

    uxPointerAddrIdx = _lwosOdpImemBlkNextGet(uxPointerAddrIdx);

    falc_imread(vAddr, IMEM_IDX_TO_ADDR(uxPointerAddrIdx));

    return uxPointerAddrIdx;
}

/*!
 * @brief   Checks if referenced VA is valid code address.
 *
 * @param[in]   vAddr   Address that triggered miss.
 *
 * @return  Boolean if VA is valid or not.
 */
static LwBool
_lwosOdpImemBlkVaIsValid
(
    LwU32 vAddr
)
{
    if (vAddr < (LwU32)_overlay_base_imem)
    {
        return LW_FALSE;
    }

    if (vAddr >= (LwU32)_overlay_end_imem)
    {
        return LW_FALSE;
    }

    return LW_TRUE;
}

/*!
 * @brief   Lookup next IMEM block availabe for ODP.
 *
 * @param[in]   blockIdx    Previous block used for ODP.
 *
 * @return  The physical DMEM block index.
 */
static LwU32
_lwosOdpImemBlkNextGet
(
    LwU32 blockIdx
)
{
    do
    {
        blockIdx++;
        if (blockIdx == (LwU32)_num_imem_blocks)
        {
            blockIdx = (LwU32)_overlay_base_imem_idx;
        }
    }
    while (LWOS_BM_GET(LwosOdpImemPinnedMap, blockIdx, 32));

    return blockIdx;
}
