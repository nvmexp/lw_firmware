/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: acr_boot_gsprm_gh100.c
 */
#include "acr.h"
#include "dev_fb.h"
#include "mmu/mmucmn.h"
#include "hwproject.h"
#include "acr_objacr.h"
#include <partitions.h>
#include "hwproject.h"
#include <lwriscv/csr.h>
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"
#include "gsp_fw_wpr_meta.h"

static GspFwWprMeta g_gspFwWprMeta ATTR_OVLY(".data")  ATTR_ALIGNED(ACR_DESC_ALIGN);

extern RM_FLCN_ACR_DESC g_desc;
extern ACR_DMA_PROP     g_dmaProp;
extern ACR_GSP_RM_DESC  g_gspRmDesc;
extern LwU8             g_wprHeaderWrappers[LSF_WPR_HEADER_WRAPPER_SIZE_ALIGNED_BYTE];
extern LwU8             g_lsbHeaderWrapper[LSF_LSB_HEADER_WRAPPER_SIZE_ALIGNED_BYTE];

#define ADDR_IN_SYSMEM(addr)    (((addr) >= LW_RISCV_AMAP_SYSGPA_START) && \
                                 ((addr) < LW_RISCV_AMAP_SYSGPA_END))

static ACR_STATUS _acrDmaGspRmDataFromSysmem
(
    void *pDest,
    LwU64 sysmemPa,
    LwU32 size
)
{
    ACR_DMA_PROP dmaPropRead;

    if (!ADDR_IN_SYSMEM(sysmemPa))
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    switch (g_gspRmDesc.target)
    {
        case RM_FBIF_TRANSCFG_TARGET_COHERENT_SYSTEM:
            dmaPropRead.ctxDma = RM_GSP_DMAIDX_PHYS_SYS_COH_FN0;
            break;
        case RM_FBIF_TRANSCFG_TARGET_NONCOHERENT_SYSTEM:
            dmaPropRead.ctxDma = RM_GSP_DMAIDX_PHYS_SYS_NCOH_FN0;
            break;
        default:
            return ACR_ERROR_ILWALID_ARGUMENT;
    }

    dmaPropRead.regionID = 0;
    dmaPropRead.wprBase = (sysmemPa - LW_RISCV_AMAP_SYSGPA_START) >> 8;

    return acrIssueDma_HAL(pAcr, pDest, LW_FALSE, 0, size,
                           ACR_DMA_FROM_SYS, ACR_DMA_SYNC_AT_END,
                           &dmaPropRead);
}

ACR_STATUS acrLoadGspRmMetaForWpr2_GH100(void)
{
    if (g_gspRmDesc.wprMetaSize != sizeof(GspFwWprMeta))
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    return _acrDmaGspRmDataFromSysmem(&g_gspFwWprMeta,
                                      g_gspRmDesc.wprMetaPa,
                                      g_gspRmDesc.wprMetaSize);
}

/*
 * @brief Allocate WPR2 at the end of FB
 */
ACR_STATUS
acrAllocateWpr2ForGspRm_GH100(void)
{
    ACR_STATUS               status       = ACR_OK;
    LwU64                    sizeInBytes  = 0;
    LwU32                    fbSizeMB     = ACR_REG_RD32(BAR0, LW_USABLE_FB_SIZE_IN_MB);
    LwU64                    wpr2Start;
    LwU64                    wpr2End;
    RM_FLCN_ACR_REGION_PROP *pRegionProp;

    if ((g_gspFwWprMeta.magic != GSP_FW_WPR_META_MAGIC) ||
        (g_gspFwWprMeta.revision != GSP_FW_WPR_META_REVISION))
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    // Adjust the sysmem addresses to be inside LW_RISCV_AMAP_SYSGPA
    CHECK_UNSIGNED_INTEGER_ADDITION_AND_RETURN_IF_OUT_OF_BOUNDS(g_gspFwWprMeta.sysmemAddrOfRadix3Elf,
                                                                LW_RISCV_AMAP_SYSGPA_START,
                                                                LW_U64_MAX);
    g_gspFwWprMeta.sysmemAddrOfRadix3Elf += LW_RISCV_AMAP_SYSGPA_START;
    if (!ADDR_IN_SYSMEM(g_gspFwWprMeta.sysmemAddrOfRadix3Elf))
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    CHECK_UNSIGNED_INTEGER_ADDITION_AND_RETURN_IF_OUT_OF_BOUNDS(g_gspFwWprMeta.sysmemAddrOfSignature,
                                                                LW_RISCV_AMAP_SYSGPA_START,
                                                                LW_U64_MAX);
    g_gspFwWprMeta.sysmemAddrOfSignature += LW_RISCV_AMAP_SYSGPA_START;
    if (!ADDR_IN_SYSMEM(g_gspFwWprMeta.sysmemAddrOfSignature))
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    g_gspFwWprMeta.fbSize = ((LwU64)fbSizeMB) << 20;

    //
    // Start at the end of WPR2 and work down. Hopper is different than prior
    // architectures, in that gspFwWprMeta.gspFwWpr(Start|End) actually identify
    // the data sub-WPR, instead of the full WPR2 range, since the latter could
    // be shared with other data.
    //
#ifdef FB_FRTS
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetWpr2Bounds_HAL(pAcr, &g_gspFwWprMeta.frtsOffset, &wpr2End));
    g_gspFwWprMeta.frtsSize = wpr2End - g_gspFwWprMeta.frtsOffset;
#else
    if (g_gspFwWprMeta.fbSize < (64 << 20))
    {
        return ACR_ERROR_LOCAL_MEM_RANGE_IS_NOT_SET;
    }

    // HACK: Pad 64MB from the end of FB for the end of WPR2.
    g_gspFwWprMeta.frtsOffset = wpr2End = g_gspFwWprMeta.fbSize - (64 << 20);
    g_gspFwWprMeta.frtsSize = 0;
#endif
    g_gspFwWprMeta.vgaWorkspaceOffset = wpr2End;
    g_gspFwWprMeta.gspFwWprEnd = g_gspFwWprMeta.frtsOffset;

    // No bootloader needed for Hopper GSP-RM
    g_gspFwWprMeta.sizeOfBootloader = 0;
    g_gspFwWprMeta.bootBinOffset = LW_U64_MAX;

    if (g_gspFwWprMeta.gspFwWprEnd < (g_gspFwWprMeta.sizeOfRadix3Elf + 0x10000))
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    //
    // Align the start of the ELF to 64KB to avoid issues with inherent alignment
    // constraints (e.g., GC6 buffers which are allocated just below this offset).
    //
    g_gspFwWprMeta.gspFwOffset = LW_ALIGN_DOWN64(g_gspFwWprMeta.gspFwWprEnd - g_gspFwWprMeta.sizeOfRadix3Elf, 0x10000);

    //
    // The WPR heap size consumes the padding from aligning the WPR2 start to
    // 128KB granularity, so callwlate the rest of what is needed at the start
    // of WPR. The WPR/LSB headers are "hidden" in that they sit between the
    // non-WPR heap and the WPR metadata structure and are not covered by
    // either.
    //
    sizeInBytes = LW_ALIGN_UP64(sizeof(LSF_WPR_HEADER_WRAPPER), 0x1000) +
                  LW_ALIGN_UP64(sizeof(LSF_LSB_HEADER_WRAPPER), 0x1000) +
                  LW_ALIGN_UP64(sizeof(g_gspFwWprMeta), 0x1000) +
                  g_gspFwWprMeta.gspFwHeapSize;
    if (g_gspFwWprMeta.gspFwOffset < (sizeInBytes + 0x20000))
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    wpr2Start = LW_ALIGN_DOWN64(g_gspFwWprMeta.gspFwOffset - sizeInBytes, 0x20000);

    // This is actually the "data sub-WPR start for GSP-RM"
    g_gspFwWprMeta.gspFwWprStart = wpr2Start +
                                   LW_ALIGN_UP64(sizeof(LSF_WPR_HEADER_WRAPPER), 0x1000) +
                                   LW_ALIGN_UP64(sizeof(LSF_LSB_HEADER_WRAPPER), 0x1000);
    g_gspFwWprMeta.gspFwHeapOffset = g_gspFwWprMeta.gspFwWprStart +
                                     LW_ALIGN_UP64(sizeof(g_gspFwWprMeta), 0x1000);

    if (wpr2Start < g_gspFwWprMeta.nonWprHeapSize)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    //
    // These fields depend on the final WPR2 location, so we need to populate them,
    // even though they are outside WPR.
    //
    g_gspFwWprMeta.nonWprHeapOffset = wpr2Start - g_gspFwWprMeta.nonWprHeapSize;
    g_gspFwWprMeta.gspFwRsvdStart = g_gspFwWprMeta.nonWprHeapOffset;

    pRegionProp = &(g_desc.regions.regionProps[ACR_WPR2_REGION_IDX]);

    // Make startAddr and EndAddr 256B aligned
    pRegionProp->startAddress = LwU64_LO32(wpr2Start >> 8); 
    pRegionProp->endAddress   = LwU64_LO32(wpr2End >> 8);
    pRegionProp->regionID     = ACR_WPR2_MMU_REGION_ID;

    // Populate DMA Parameters for WPR2
    g_dmaProp.wprBase  = pRegionProp->startAddress;
    g_dmaProp.regionID = ACR_WPR2_MMU_REGION_ID;
    g_dmaProp.ctxDma   = RM_GSP_DMAIDX_UCODE;

    return status; 
}

/*
 * @brief Set up WPR headers and data prior to GSP-RM copy-in.
 */
ACR_STATUS
acrSetupWpr2ForGspRm_GH100(void)
{
    ACR_STATUS status = ACR_OK;
    LSF_WPR_HEADER_WRAPPER *pWprHeaderWrapper = GET_WPR_HEADER_WRAPPER(0);
    LSF_LSB_HEADER_WRAPPER *pLsbHeaderWrapper = (LSF_LSB_HEADER_WRAPPER *)g_lsbHeaderWrapper;
    LwU64 wpr2BaseAddr = g_dmaProp.wprBase << 8;

    acrMemset_HAL(pAcr, pWprHeaderWrapper, 0, sizeof(LSF_WPR_HEADER_WRAPPER));

    pWprHeaderWrapper->genericHdr.identifier = WPR_GENERIC_HEADER_ID_LSF_WPR_HEADER;
    pWprHeaderWrapper->genericHdr.version    = LSF_WPR_HEADER_VERSION_2;
    pWprHeaderWrapper->genericHdr.size       = LSF_WPR_HEADER_WRAPPER_V2_SIZE_BYTE;

    {
        LSF_WPR_HEADER_V2 *pWprHeader = &pWprHeaderWrapper->u.lsfWprHdrV2;
        pWprHeader->falconId   = LSF_FALCON_ID_GSP_RISCV;
        // WD-TODO: use lsUcodeVersion from files generated from signing.
        //          This will need to be passed in via the command arguments
        //          somehow.
        pWprHeader->bilwersion = 1;
        pWprHeader->lsbOffset  = LW_ALIGN_UP(sizeof(*pWprHeaderWrapper), 0x1000);
    }

    // DMA the WPR header structure into the start of WPR
    acrWriteAllWprHeaderWrappers_HAL(pAcr, ACR_WPR2_REGION_IDX);

    acrMemset_HAL(pAcr, pLsbHeaderWrapper, 0, sizeof(LSF_LSB_HEADER_WRAPPER));    
    pLsbHeaderWrapper->genericHdr.identifier = WPR_GENERIC_HEADER_ID_LSF_UCODE_DESC;
    pLsbHeaderWrapper->genericHdr.version    = LSF_LSB_HEADER_VERSION_2;
    pLsbHeaderWrapper->genericHdr.size       = LSF_LSB_HEADER_WRAPPER_V2_SIZE_BYTE;

    {
        LSF_LSB_HEADER_V2 *pLsbHeader = &pLsbHeaderWrapper->u.lsfLsbHdrV2;
    
        // We can DMA the signature directly to the LSB header in DMEM
        if (g_gspFwWprMeta.sysmemAddrOfSignature > 0)
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrDmaGspRmDataFromSysmem(
                                              &pLsbHeader->signature,
                                              g_gspFwWprMeta.sysmemAddrOfSignature,
                                              (LwU32)g_gspFwWprMeta.sizeOfSignature));
        }
        // WD-TODO: enable when signature support is added
        // else
        // {
        //     return ACR_ERROR_ILWALID_ARGUMENT;
        // }
        
        // "ucode" is all the ELF data (mixed code and data, so needs to be R/W)
        pLsbHeader->ucodeOffset = LwU64_LO32(g_gspFwWprMeta.gspFwOffset - wpr2BaseAddr);
        pLsbHeader->dataSize = LwU64_LO32(g_gspFwWprMeta.sizeOfRadix3Elf);

        // Everything else in the header is 0/unused
    }

    // DMA the LSB header structure into WPR
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrIssueDma_HAL(pAcr, pLsbHeaderWrapper, LW_FALSE,
                                                      pWprHeaderWrapper->u.lsfWprHdrV2.lsbOffset,
                                                      LW_ALIGN_UP((sizeof(LSF_LSB_HEADER_WRAPPER)),
                                                                  LSF_LSB_HEADER_ALIGNMENT),
                                                      ACR_DMA_TO_FB, ACR_DMA_SYNC_AT_END, &g_dmaProp));

    return ACR_OK;
}

/*
 * @brief Get WPR2 region bounds
 * @param[in] strtAddr    Pointer to startAddress of WPR2 region
 * @param[in] endAddr     Pointer to endAddress of WPR2 region
 */
ACR_STATUS
acrGetWpr2Bounds_GH100
(
    LwU64 *startAddr,
    LwU64 *endAddr
)
{
    if ((startAddr == NULL) || (endAddr == NULL))
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    *startAddr = (((LwU64)DRF_VAL(_PFB, _PRI_MMU_WPR2_ADDR_LO, _VAL,
                                  ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_LO)))
                    << LW_PFB_PRI_MMU_WPR2_ADDR_LO_ALIGNMENT);
    *endAddr   = (((LwU64)DRF_VAL(_PFB, _PRI_MMU_WPR2_ADDR_HI, _VAL,
                                  ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_HI)))
                    << LW_PFB_PRI_MMU_WPR2_ADDR_HI_ALIGNMENT)
                    + ACR_REGION_ALIGN;

    if (*endAddr <= (*startAddr + ACR_REGION_ALIGN))
    {
        return ACR_ERROR_NO_WPR;
    }
 
    return ACR_OK;
}

#define RADIX3_PAGE_SHIFT   12
#define RADIX3_PAGE_SIZE    (1 << RADIX3_PAGE_SHIFT)
#define RADIX3_ENTRIES_LOG2 (RADIX3_PAGE_SHIFT - 3)
#define RADIX3_PAGE_MASK    (RADIX3_PAGE_SIZE - 1)
#define RADIX3_IDX_MASK     ((RADIX3_PAGE_SIZE / sizeof(LwU64)) - 1)

LwU64 radix3AddrFromOffset(LwU64 ***pde2, LwU64 offset)
{
    if (offset >= ((LwU64)RADIX3_PAGE_SIZE << (3 * RADIX3_ENTRIES_LOG2)))
        return LW_U64_MAX;

    LwU64 **pde1 = pde2[(offset >> (2 * RADIX3_ENTRIES_LOG2 + RADIX3_PAGE_SHIFT)) & RADIX3_IDX_MASK];
    if (!ADDR_IN_SYSMEM((LwU64)pde1))
        return LW_U64_MAX;

    LwU64 *pde0 = pde1[(offset >> (1 * RADIX3_ENTRIES_LOG2 + RADIX3_PAGE_SHIFT)) & RADIX3_IDX_MASK];
    if (!ADDR_IN_SYSMEM((LwU64)pde0))
        return LW_U64_MAX;

    LwU64 pte  = pde0[(offset >> (0 * RADIX3_ENTRIES_LOG2 + RADIX3_PAGE_SHIFT)) & RADIX3_IDX_MASK];
    if (!ADDR_IN_SYSMEM(pte))
        return LW_U64_MAX;

    // Recompute offset
    return pte + (offset & RADIX3_PAGE_MASK);
}

/*
 * @brief Copy GSP-RM image from sysmem to WPR2
 *
 * @param[in] wprIndex          Index of the WPR region
 *
 * @return  ACR_OK                  If copy is successful
 * @return  ACR_ERROR_DMA_FAILURE   If copy failed
 */
ACR_STATUS
acrCopyGspRmToWpr2_GH100(void)
{
    ACR_DMA_PROP   dmaPropWrite;
    ACR_STATUS     status       = ACR_OK;
    LwU64       ***pRadix3Table = (LwU64 ***)(g_gspFwWprMeta.sysmemAddrOfRadix3Elf);

    ct_assert(RADIX3_PAGE_SIZE <= ACR_GENERIC_BUF_SIZE_IN_BYTES);

    // This region is the write buffer in WPR2
    dmaPropWrite.wprBase  = g_gspFwWprMeta.gspFwOffset >> 8;
    dmaPropWrite.ctxDma   = RM_GSP_DMAIDX_UCODE;
    dmaPropWrite.regionID = g_dmaProp.regionID;

    //
    // Due to its size, the GSP-RM ELF is stored in a discontiguous buffer in system memory.
    // The GSP client RM passes a "radix3" page table to map "virtual" addresses
    // (offsets in the ELF) to physical DMA addresses for the ELF data. We need to decode
    // this radix3 format to copy the image into WPR.
    //
    for (LwU64 offset = 0; offset < g_gspFwWprMeta.sizeOfRadix3Elf; offset += RADIX3_PAGE_SIZE)
    {
        LwU64 sourcePa = radix3AddrFromOffset(pRadix3Table, offset);

        // Fail if the radix3 table wasn't formatted properly
        if (sourcePa == LW_U64_MAX)
            return ACR_ERROR_DMA_FAILURE;

        // @note: radix3AddrFromOffset ensures that the final page is within syscoh
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrDmaGspRmDataFromSysmem(g_tmpGenericBuffer,
                                                                     sourcePa,
                                                                     RADIX3_PAGE_SIZE));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrIssueDma_HAL(pAcr, g_tmpGenericBuffer, LW_FALSE,
                                                          LwU64_LO32(offset),
                                                          RADIX3_PAGE_SIZE, ACR_DMA_TO_FB,
                                                          ACR_DMA_SYNC_AT_END, &dmaPropWrite));
    }
  
    return ACR_OK;
}

/*
 * @brief Setup SubWpr for GSP-RM ucode data.
 */
ACR_STATUS acrSetupSubWprForWpr2_GH100
(
    LwU32 *failingEngine,
    LwU32  wprIndex
)
{
    ACR_STATUS      status = ACR_OK;
    ACR_FLCN_CONFIG flcnCfg;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetFalconConfig_HAL(pAcr, LSF_FALCON_ID_GSP_RISCV, LSF_FALCON_INSTANCE_DEFAULT_0, &flcnCfg));

    //
    // GSP-RM uses a single sub-WPR, which is all PL0 "data", including:
    //  1. the WPR metadata structure,
    //  2. the WPR heap, and
    //  3. the signed GSP-RM ELF image
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWprExt_HAL(pAcr, &flcnCfg,
        GSP_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR2,
        (LwU32)(g_gspFwWprMeta.gspFwWprStart >> LW_PFB_PRI_MMU_WPR2_ADDR_LO_ALIGNMENT),
        (LwU32)((g_gspFwWprMeta.gspFwWprEnd - 1) >> LW_PFB_PRI_MMU_WPR2_ADDR_HI_ALIGNMENT),
        ACR_SUB_WPR_MMU_RMASK_ALL_LEVELS_ENABLED,   // Read PLM mask
        ACR_SUB_WPR_MMU_RMASK_ALL_LEVELS_ENABLED)); // Write PLM mask

    return status;
}

/*
 * @brief Copy memory map (GspFwWprMeta) into WPR2 after scrubbing and
 *        signature verif.
 */
ACR_STATUS acrSetupWpr2MemMapForGspRm_GH100(void)
{
    ACR_STATUS status;
    LwU32 wprMetaOffset = (LwU32)(g_gspFwWprMeta.gspFwWprStart - (g_dmaProp.wprBase << 8));

    g_gspFwWprMeta.verified = GSP_FW_WPR_META_VERIFIED;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrIssueDma_HAL(pAcr, &g_gspFwWprMeta, LW_FALSE, wprMetaOffset,
                                                      sizeof(g_gspFwWprMeta), ACR_DMA_TO_FB,
                                                      ACR_DMA_SYNC_AT_END, &g_dmaProp));

    return ACR_OK;
}
