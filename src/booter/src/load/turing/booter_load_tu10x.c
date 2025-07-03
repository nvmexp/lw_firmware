/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: booter_load_tu10x.c
 */
//
// Includes
//
#include "booter.h"

extern BOOTER_DMA_PROP g_dmaProp[BOOTER_DMA_CONTEXT_COUNT];
extern GspFwWprMeta g_gspFwWprMeta;

LwU8   g_bounceBuffer[256]   ATTR_OVLY(".data") ATTR_ALIGNED(0x100);
LwU8   g_bounceBufferB[4096] ATTR_OVLY(".data") ATTR_ALIGNED(0x100);
LwBool g_bIsDebug            ATTR_OVLY(".data") = LW_FALSE;

#define LIBOS_MEMORY_REGION_RADIX_PAGE_SIZE 4096
#define LIBOS_MEMORY_REGION_RADIX_PAGE_LOG2 12

static LwU64 _pageByAddress(LwU64 pde2, LwU64 va) ATTR_OVLY(OVL_NAME);

static LwU64
_pageByAddress(LwU64 pde2, LwU64 va)
{
    LwU64 entriesLog2 = LIBOS_MEMORY_REGION_RADIX_PAGE_LOG2 - 3;
    LwU64 pageMask = LIBOS_MEMORY_REGION_RADIX_PAGE_SIZE - 1;
    LwU64 idxMask = (LIBOS_MEMORY_REGION_RADIX_PAGE_SIZE  / sizeof(LwU64)) - 1;
    LwU64 tmpOff;
    LwU32 dmaOff;
    LwU64 val;

    tmpOff = pde2 + (sizeof(LwU64)*((va >> (2 * entriesLog2 + LIBOS_MEMORY_REGION_RADIX_PAGE_LOG2)) & idxMask));
    dmaOff = (LwU64_LO32(tmpOff) & 0xFF);
    
    g_dmaProp[BOOTER_DMA_SYSMEM_CONTEXT].baseAddr = (tmpOff >> 8);
    if ((booterIssueDma_HAL(pBooter, &val, LW_FALSE, dmaOff, sizeof(LwU64),
       BOOTER_DMA_FROM_FB, BOOTER_DMA_SYNC_AT_END, &g_dmaProp[BOOTER_DMA_SYSMEM_CONTEXT])) != sizeof(LwU64))
    {
        return BOOTER_ERROR_DMA_FAILURE;
    }

    LwU64 pde1 = val;
    tmpOff = pde1 + (sizeof(LwU64)*((va >> (1 * entriesLog2 + LIBOS_MEMORY_REGION_RADIX_PAGE_LOG2)) & idxMask));
    dmaOff = (LwU64_LO32(tmpOff) & 0xFF);

    g_dmaProp[BOOTER_DMA_SYSMEM_CONTEXT].baseAddr = (tmpOff >> 8);
    if ((booterIssueDma_HAL(pBooter, &val, LW_FALSE, dmaOff, sizeof(LwU64),
       BOOTER_DMA_FROM_FB, BOOTER_DMA_SYNC_AT_END, &g_dmaProp[BOOTER_DMA_SYSMEM_CONTEXT])) != sizeof(LwU64))
    {
        return BOOTER_ERROR_DMA_FAILURE;
    }

    LwU64 pde0 = val;
    LwU64 pte = pde0 + (sizeof(LwU64)*((va >> (0 * entriesLog2 + LIBOS_MEMORY_REGION_RADIX_PAGE_LOG2)) & idxMask));
    dmaOff = (LwU64_LO32(pte) & 0xFF);

    g_dmaProp[BOOTER_DMA_SYSMEM_CONTEXT].baseAddr = (pte >> 8);
    if ((booterIssueDma_HAL(pBooter, &val, LW_FALSE, dmaOff, sizeof(LwU64),
       BOOTER_DMA_FROM_FB, BOOTER_DMA_SYNC_AT_END, &g_dmaProp[BOOTER_DMA_SYSMEM_CONTEXT])) != sizeof(LwU64))
    {
        return BOOTER_ERROR_DMA_FAILURE;
    }
 
    // Recompute offset.
    // Returns offset in aperture (without adding LW_RISCV_AMAP_SYSGPA_START).
    return val + (va & pageMask);
}


/*!
 * @brief
 */
BOOTER_STATUS
booterLoadGspRmFromSysmemToFbRadix3_TU10X(GspFwWprMeta *pWprMeta)
{
    LwU64 offset      = 0;
    LwU64 imagePa     = pWprMeta->sysmemAddrOfRadix3Elf;
    LwU64 alignedSize = LW_ALIGN_UP(pWprMeta->sizeOfRadix3Elf, LIBOS_MEMORY_REGION_RADIX_PAGE_SIZE);

    g_dmaProp[BOOTER_DMA_FB_WPR_CONTEXT].baseAddr = (pWprMeta->gspFwOffset >> 8); // baseAddr is 256 byte aligned

    for(offset = 0; offset < alignedSize; offset += LIBOS_MEMORY_REGION_RADIX_PAGE_SIZE)
    {
        // Translate the address
        LwU64 sourceOffset = _pageByAddress(imagePa, offset);

        if(sourceOffset == BOOTER_ERROR_DMA_FAILURE)
        {
            falc_halt();
        }

        LwU32 dmaOff = 0;

        g_dmaProp[BOOTER_DMA_SYSMEM_CONTEXT].baseAddr = (sourceOffset >> 8); // baseAddr is 256 byte aligned
        dmaOff = sourceOffset & 0xFF;

        // Read data into TCM
        if ((booterIssueDma_HAL(pBooter, g_bounceBufferB, LW_FALSE, dmaOff, LIBOS_MEMORY_REGION_RADIX_PAGE_SIZE,
           BOOTER_DMA_FROM_FB, BOOTER_DMA_SYNC_AT_END, &g_dmaProp[BOOTER_DMA_SYSMEM_CONTEXT])) != LIBOS_MEMORY_REGION_RADIX_PAGE_SIZE)
        {
            return BOOTER_ERROR_DMA_FAILURE;
        }

        // dma_flush();

        if ((booterIssueDma_HAL(pBooter, g_bounceBufferB, LW_FALSE, offset, LIBOS_MEMORY_REGION_RADIX_PAGE_SIZE,
           BOOTER_DMA_TO_FB, BOOTER_DMA_SYNC_AT_END, &g_dmaProp[BOOTER_DMA_FB_WPR_CONTEXT])) != LIBOS_MEMORY_REGION_RADIX_PAGE_SIZE)
        {
            return BOOTER_ERROR_DMA_FAILURE;
        }

        // fence rw, rw
    }


    return BOOTER_OK; 
}

/*!
 * @brief
 */
BOOTER_STATUS
booterLoadFmcFromSysmemToFb_TU10X(GspFwWprMeta *pWprMeta)
{
#define DMA_CHUNK_SIZE   256

    LwU32 fbOff = 0;

    g_dmaProp[BOOTER_DMA_SYSMEM_CONTEXT].baseAddr = (pWprMeta->sysmemAddrOfBootloader >> 8); // baseAddr is 256 byte aligned
    g_dmaProp[BOOTER_DMA_FB_WPR_CONTEXT].baseAddr = (pWprMeta->bootBinOffset >> 8);          // baseAddr is 256 byte aligned

    LwU32 size = pWprMeta->sizeOfBootloader;
    while (size)
    {
        LwU64 blockSize = size > DMA_CHUNK_SIZE ? DMA_CHUNK_SIZE : size;

        if ((booterIssueDma_HAL(pBooter, g_bounceBuffer, LW_FALSE, fbOff, blockSize,
           BOOTER_DMA_FROM_FB, BOOTER_DMA_SYNC_AT_END, &g_dmaProp[BOOTER_DMA_SYSMEM_CONTEXT])) != blockSize)
        {
            return BOOTER_ERROR_DMA_FAILURE;
        }

        if ((booterIssueDma_HAL(pBooter, g_bounceBuffer, LW_FALSE, fbOff, blockSize,
           BOOTER_DMA_TO_FB, BOOTER_DMA_SYNC_AT_END, &g_dmaProp[BOOTER_DMA_FB_WPR_CONTEXT])) != blockSize)
        {
            return BOOTER_ERROR_DMA_FAILURE;
        }

        size -= blockSize;
        fbOff += blockSize;
    }

    return BOOTER_OK; 
}

/*!
 * @brief Writes GspFwWprMeta header wrappers to FB (now WPR)
 */
BOOTER_STATUS
booterWriteGspRmWprHeaderToWpr_TU10X(GspFwWprMeta *pWprMeta, LwU64 fbAddr)
{
    LwU32 fbOff = 0;

    //
    // Booter makes assumptions regarind GspFwWprMeta alignment (for DMA, etc.)
    // Fail loudly at compile-time if size ever changes so we can revisit.
    //
    ct_assert(sizeof(*pWprMeta) == 256);

    g_dmaProp[BOOTER_DMA_FB_WPR_CONTEXT].baseAddr = (fbAddr >> 8); // baseAddr is 256 byte aligned

    if ((booterIssueDma_HAL(pBooter, pWprMeta, LW_FALSE, fbOff, sizeof(*pWprMeta),
                            BOOTER_DMA_TO_FB, BOOTER_DMA_SYNC_AT_END, &g_dmaProp[BOOTER_DMA_FB_WPR_CONTEXT])) != sizeof(*pWprMeta))
    {
        return BOOTER_ERROR_DMA_FAILURE;
    }

    return BOOTER_OK;
}

/*!
 * @brief Main function to do Booter load process
 */
BOOTER_STATUS booterLoad_TU10X(void)
{
#define MAILBOX1_SYSMEM_ALIGNMENT_FROM_CPU_RM 12  // 4K aligned

    BOOTER_STATUS status;

    // Check handoffs with other Booters / ACR
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterCheckBooterHandoffsInSelwreScratch_HAL(pBooter));

    // Ensure WPR1 is down (no ACR should have run yet)
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterCheckIfWpr1IsTornDown_HAL(pBooter));

    booterInitializeDma_HAL(pBooter);

    // Retrieve initial GspFwWprMeta via SYSMEM address given by CPU-RM
    LwU64 mailbox1 = BOOTER_REG_RD32(CSB, LW_CSEC_FALCON_MAILBOX1);
    LwU64 wprMetaSysmemAddr = (mailbox1 << MAILBOX1_SYSMEM_ALIGNMENT_FROM_CPU_RM);
    // TODO-derekw: sanitize sysmem addr?
    GspFwWprMeta *pWprMeta = &g_gspFwWprMeta;
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterReadGspRmWprHeader_HAL(pBooter, pWprMeta, wprMetaSysmemAddr, &g_dmaProp[BOOTER_DMA_SYSMEM_CONTEXT]));

    LwU64 actualFrtsStart;
    LwU64 actualFrtsEnd;
    LwBool bFrtsIsSetUp;

    status = booterCheckIfFrtsIsSetUp_HAL(pBooter, &actualFrtsStart, &actualFrtsEnd);
    if (status == BOOTER_ERROR_BIN_NOT_SUPPORTED)
    {
        // FRTS is not supported on this chip.
        // We need to setup WPR2 from scratch.
        bFrtsIsSetUp = LW_FALSE;
    }
    else if (status == BOOTER_OK)
    {
        // FRTS is supported on this chip, and FRTS has setup WPR2.
        // We need to extend WPR2.
        bFrtsIsSetUp = LW_TRUE;
    }
    else
    {
        // FRTS is supposed to be on this chip, but it has not setup WPR2.
        // We need to bail.
        return status;
    }

    // Sanity check GspFwWprMeta received from CPU-RM
    if (pWprMeta->magic != GSP_FW_WPR_META_MAGIC ||
        pWprMeta->revision != GSP_FW_WPR_META_REVISION)
    {
        return BOOTER_ERROR_ILWALID_WPRMETA_MAGIC_OR_REVISION;
    }

    //
    // Verify CPU-RM-proposed WPR2 regions
    //

    // Ensure WPR2 will actually be up
    if (pWprMeta->gspFwWprEnd < pWprMeta->gspFwWprStart)
    {
        return BOOTER_ERROR_ILWALID_WPRMETA_WPR2_REGION;
    }

    // Ensure WPR2 start and end are well-aligned for WPR
    if (!(LW_IS_ALIGNED(pWprMeta->gspFwWprStart, BOOTER_WPR_ALIGNMENT) &&
          LW_IS_ALIGNED(pWprMeta->gspFwWprEnd, BOOTER_WPR_ALIGNMENT)))
    {
        return BOOTER_ERROR_ILWALID_WPRMETA_WPR2_ALIGNMENT;
    }

    // Ensure FRTS offset is well-aligned for sub-WPR
    // (if it exists, otherwise force it to be zero-sized at WPR2 end)
    if (bFrtsIsSetUp)
    {
        if (!LW_IS_ALIGNED(pWprMeta->frtsOffset, BOOTER_SUB_WPR_ALIGNMENT))
        {
            return BOOTER_ERROR_ILWALID_WPRMETA_FRTS_ALIGNMENT;
        }
    }
    else
    {
        pWprMeta->frtsOffset = pWprMeta->gspFwWprEnd;
        pWprMeta->frtsSize = 0;
    }

    //
    // Ensure GSP-RM ELF, GSP-RM BL, and FRTS all end up in WPR2
    // Note: checking order:
    //   1. check start offsets against WPR2 start
    //   2. check start offsets against WPR2 end
    //   3. check size against (WPR2 end - start offset)
    //      (note: this can't overflow/underflow after checking 2.)
    //
    {
        if ((pWprMeta->gspFwOffset <= pWprMeta->gspFwWprStart) || 
            (pWprMeta->gspFwOffset >= pWprMeta->gspFwWprEnd) ||
            ((pWprMeta->gspFwWprEnd - pWprMeta->gspFwOffset) < pWprMeta->sizeOfRadix3Elf))
        {
            return BOOTER_ERROR_ILWALID_WPRMETA_ELF_REGION;
        }

        if ((pWprMeta->bootBinOffset <= pWprMeta->gspFwWprStart) || 
            (pWprMeta->bootBinOffset >= pWprMeta->gspFwWprEnd) ||
            ((pWprMeta->gspFwWprEnd - pWprMeta->bootBinOffset) < pWprMeta->sizeOfBootloader))
        {
            return BOOTER_ERROR_ILWALID_WPRMETA_BL_REGION;
        }

        if (bFrtsIsSetUp)
        {
            if ((pWprMeta->frtsOffset <= pWprMeta->gspFwWprStart) || 
                (pWprMeta->frtsOffset >= pWprMeta->gspFwWprEnd) ||
                ((pWprMeta->gspFwWprEnd - pWprMeta->frtsOffset) < pWprMeta->frtsSize))
            {
                return BOOTER_ERROR_ILWALID_WPRMETA_FRTS_REGION;
            }
        }
    }

    // Verify proposed FRTS region (if it exists)
    if (bFrtsIsSetUp)
    {
        // Ensure proposed FRTS region agrees with what FRTS actually setup
        if ((pWprMeta->frtsOffset != actualFrtsStart) ||
            (pWprMeta->frtsSize != (actualFrtsEnd - actualFrtsStart)))
        {
            return BOOTER_ERROR_ILWALID_WPRMETA_FRTS_REGION;
        }

        // Ensure proposed FRTS region is at the end of WPR2
        if (pWprMeta->frtsOffset != (pWprMeta->gspFwWprEnd - pWprMeta->frtsSize))
        {
            return BOOTER_ERROR_ILWALID_WPRMETA_FRTS_REGION;
        }
    }

    // Verify no overlaps caused by GSP-RM ELF and BL placement
    if (pWprMeta->gspFwOffset < pWprMeta->bootBinOffset)
    {
        // Case: GSP-RM ELF is placed lower than GSP-RM BL

        // Ensure GSP-RM ELF placement leaves enough room for WprMeta at the start
        if ((pWprMeta->gspFwOffset - pWprMeta->gspFwWprStart) < sizeof(GspFwWprMeta))
        {
            return BOOTER_ERROR_ILWALID_WPRMETA_ELF_REGION;
        }

        // Ensure GSP-RM BL placement leaves enough room for FRTS at the end
        if (((pWprMeta->gspFwWprEnd - pWprMeta->bootBinOffset) - pWprMeta->sizeOfBootloader) < pWprMeta->frtsSize)
        {
            return BOOTER_ERROR_ILWALID_WPRMETA_BL_REGION;
        }

        // Ensure no overlap from GSP-RM ELF to GSP-RM BL
        if ((pWprMeta->bootBinOffset - pWprMeta->gspFwOffset) < pWprMeta->sizeOfRadix3Elf)
        {
            return BOOTER_ERROR_ILWALID_WPRMETA_BL_REGION;
        }
    }
    else
    {
        // Case: GSP-RM BL is placed lower than GSP-RM ELF

        // Ensure GSP-RM BL placement leaves enough room for WprMeta at the start
        if ((pWprMeta->bootBinOffset - pWprMeta->gspFwWprStart) < sizeof(GspFwWprMeta))
        {
            return BOOTER_ERROR_ILWALID_WPRMETA_BL_REGION;
        }

        // Ensure GSP-RM ELF placement leaves enough room for FRTS at the end
        if (((pWprMeta->gspFwWprEnd - pWprMeta->gspFwOffset) - pWprMeta->sizeOfRadix3Elf) < pWprMeta->frtsSize)
        {
            return BOOTER_ERROR_ILWALID_WPRMETA_ELF_REGION;
        }

        // Ensure no overlap from GSP-RM BL to GSP-RM ELF
        if ((pWprMeta->gspFwOffset - pWprMeta->bootBinOffset) < pWprMeta->sizeOfBootloader)
        {
            return BOOTER_ERROR_ILWALID_WPRMETA_ELF_REGION;
        }
    }

    LwU32 i;

    // Ilwalidate SEC sub-WPRs
    for (i = 0; i < LW_PFB_PRI_MMU_FALCON_SEC_CFGA__SIZE_1; i++)
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterProgramSubWprSec_HAL(pBooter,
            i, LW_FALSE, 0, 0, BOOTER_UNLOCK_READ_MASK, BOOTER_UNLOCK_WRITE_MASK));
    }

    // Ilwalidate GSP sub-WPRs
    for (i = 0; i < LW_PFB_PRI_MMU_FALCON_GSP_CFGA__SIZE_1; i++)
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterProgramSubWprGsp_HAL(pBooter,
            i, LW_FALSE, 0, 0, BOOTER_UNLOCK_READ_MASK, BOOTER_UNLOCK_WRITE_MASK));
    }

    // Extend or setup WPR2, but do NOT setup sub-WPRs
    if (bFrtsIsSetUp)
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterExtendWpr2_HAL(pBooter, pWprMeta));
    }
    else
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterSetUpWpr2_HAL(pBooter, pWprMeta));
    }

    // Program SEC sub-WPR for ourself (Booter Load) to use (L3 R/W covering all of WPR2 except FRTS)
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterProgramSubWprSec_HAL(pBooter,
        SEC2_SUB_WPR_ID_6_GSPRM_BOOTER_WPR2, LW_TRUE,
        pWprMeta->gspFwWprStart, pWprMeta->frtsOffset,
        BOOTER_SUB_WPR_MMU_RMASK_L3, BOOTER_SUB_WPR_MMU_WMASK_L3));

    // Write WprMeta (without verified marked) to FB at the start of WPR2
    pWprMeta->verified = 0;
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterWriteGspRmWprHeaderToWpr_HAL(pBooter, pWprMeta, pWprMeta->gspFwWprStart));

    // DMA BL to FB from SYSMEM
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterLoadFmcFromSysmemToFb_HAL(pBooter, pWprMeta));

    // DMA GSP-RM ELF to FB from SYSMEM following the radix3 implementation
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterLoadGspRmFromSysmemToFbRadix3_HAL(pBooter, pWprMeta));

    // Verify signature of GSP-RM ELF and BL
    g_bIsDebug = booterIsDebugModeEnabled_HAL(pBooter);
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterVerifyLsSignatures_HAL(pBooter, pWprMeta));

    // Write WprMeta (with verified marked now that sig verif has passed) to FB at the start of WPR2
    pWprMeta->verified = GSP_FW_WPR_META_VERIFIED;
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterWriteGspRmWprHeaderToWpr_HAL(pBooter, pWprMeta, pWprMeta->gspFwWprStart));

    // Prepare GSP RISC-V for GSP-RM (set sub-WPR, PLMs, etc.) 
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterPrepareGspRm_HAL(pBooter, pWprMeta));

    // Program sub-WPRs for Booter Reload and Booter Unload
    {
        LwU64 wprMetaStart = pWprMeta->gspFwWprStart;
        LwU64 wprMetaEnd = LW_ALIGN_UP(((LwU64) (pWprMeta->gspFwWprStart)) + sizeof(GspFwWprMeta),
                                       BOOTER_SUB_WPR_ALIGNMENT);

        // Re-program SEC sub-WPR for Booter Unload to use (L3 R-only covering WprMeta only)
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterProgramSubWprSec_HAL(pBooter,
            SEC2_SUB_WPR_ID_6_GSPRM_BOOTER_WPR2, LW_TRUE,
            wprMetaStart, wprMetaEnd,
            BOOTER_SUB_WPR_MMU_RMASK_L3, BOOTER_SUB_WPR_MMU_WMASK_ALL_LEVELS_DISABLED));
        
        // Program LWDEC sub-WPR for Booter Reload to use (L3 R-only covering WprMeta only)
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterProgramSubWprLwdec_HAL(pBooter,
            LWDEC0_SUB_WPR_ID_3_GSPRM_BOOTER_WPR2, LW_TRUE,
            wprMetaStart, wprMetaEnd,
            BOOTER_SUB_WPR_MMU_RMASK_L3, BOOTER_SUB_WPR_MMU_WMASK_ALL_LEVELS_DISABLED));
    }

    //
    // Set handoff bit for indicating Booter Load has completed successfully
    // (checked by GSP-RM BL in addition to other Booter / ACR ucodes)
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterWriteBooterHandoffsToSelwreScratch_HAL(pBooter));

    // Now, start GSP-RM from WPR2
    booterStartGspRm_HAL(pBooter);

    return status;
}

