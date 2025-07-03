/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: acr_boot_gsprm_proxy_gh100.c
 */
#include "acr.h"
#include "dev_fb.h"
#include "mmu/mmucmn.h"
#include "hwproject.h"
#include "acr_objacr.h"
#include <partitions.h>
#include <lwriscv/csr.h>


extern RM_FLCN_ACR_DESC       g_desc;
extern ACR_DMA_PROP           g_dmaProp;
extern ACR_GSP_RM_PROXY_DESC  g_gspRmProxyDesc;
extern LwU8                   g_wprHeaderWrappers[LSF_WPR_HEADERS_WRAPPER_TOTAL_SIZE_MAX];
extern LwU8                   g_lsbHeaderWrapper[LSF_LSB_HEADER_WRAPPER_SIZE_ALIGNED_BYTE];

/*
 * @brief Allocate WPR2 at the end of FB
 * @param[in] baseAddressPa    StartAddress of WPR2 region in Riscv Pa
 * @param[in] sizeInBytes      Size of allocated region in FB in bytes
 */
ACR_STATUS
acrAllocateWpr2ForGspRm_GH100()
{
    ACR_STATUS               status       = ACR_OK;
    LwU32                    sizeInBytes;
    LwU64                    startAddr;
    LwU64                    endAddr;
    RM_FLCN_ACR_REGION_PROP *pRegionProp; 

    if (g_gspRmProxyDesc.gspRmProxyBlobSize == 0)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    } 

    pRegionProp = &(g_desc.regions.regionProps[ACR_WPR2_REGION_IDX]);

#ifdef FB_FRTS
    sizeInBytes = g_gspRmProxyDesc.gspRmProxyBlobSize;

    // Get WPR2 start and end Address
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetWpr2Bounds_HAL(pAcr, &startAddr, &endAddr));
    if (startAddr >= sizeInBytes)
    {
        startAddr -= sizeInBytes;
    }
    else
    {
        return ACR_ERROR_VARIABLE_SIZE_OVERFLOW;
    }

    pRegionProp->startAddress = startAddr >> 8;
    pRegionProp->endAddress   = endAddr   >> 8;
    pRegionProp->regionID     = ACR_WPR2_MMU_REGION_ID;
#else
    LwU64 baseAddressPa;
    baseAddressPa = g_gspRmProxyDesc.wprCarveoutBasePa; 
    sizeInBytes   = g_gspRmProxyDesc.wprCarveoutSize;

    // Make startAddr and EndAddr 256B aligned
    if (baseAddressPa >= LW_RISCV_AMAP_FBGPA_START)
    {
        startAddr = (baseAddressPa - LW_RISCV_AMAP_FBGPA_START) >> 8;
    }
    else
    {
        return ACR_ERROR_VARIABLE_SIZE_OVERFLOW;
    }

    if (startAddr > 0xFFFFFFFFU)
    {
        return ACR_ERROR_VARIABLE_SIZE_OVERFLOW;
    }

    endAddr = (startAddr + (sizeInBytes >> 8));

    if ((endAddr > 0xFFFFFFFFU) || (endAddr < startAddr))
    {
        return ACR_ERROR_VARIABLE_SIZE_OVERFLOW;
    }

    pRegionProp->startAddress = LwU64_LO32(startAddr);
    pRegionProp->endAddress   = LwU64_LO32(endAddr);
    pRegionProp->regionID     = ACR_WPR2_MMU_REGION_ID;
#endif // FB_FRTS

    // Populate DMA Parameters for WPR2
    g_dmaProp.wprBase  = pRegionProp->startAddress;
    g_dmaProp.regionID = ACR_WPR2_MMU_REGION_ID;
    g_dmaProp.ctxDma   = RM_GSP_DMAIDX_UCODE;

    return status; 
}

/*
 * @brief Get WPR2 region Address
 * @param[in] strtAddr    Pointer to startAddress of WPR2 region
 * @param[in] endAddr     Pointer to endAddress of WPR2 region
 * @param[in] sizeInBytes Size of the region to be allocated in bytes,
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

/*
 * @brief Copy Gsp Rm Proxy Image from sysmem to WPR2

 * @return  ACR_OK                  If copy is successful
 * @return  ACR_ERROR_DMA_FAILURE   If copy failed
 */
ACR_STATUS
acrCopyGspRmToWpr2_GH100()
{
    ACR_STATUS     status                = ACR_OK;
    LwU32          totalCopySizeInBytes;
    LwU64          gspProxyStartAddr;
    LwU32          bytesCopied           = 0;
    ACR_DMA_PROP   dmaPropRead;
    ACR_DMA_PROP   dmaPropWrite;
    LwU32          bytesXfered           = 0;

    // Get total size of GSP-RM region in sysmem in bytes
    totalCopySizeInBytes = g_gspRmProxyDesc.gspRmProxyBlobSize;

    //
    // Get GSP-RM Blob Start Address in Sysmem from GSP-RM Blob descriptor
    // Align Start Address to 256B 
    //
    if (g_gspRmProxyDesc.gspRmProxyBlobPa >= LW_RISCV_AMAP_SYSGPA_START)
    {
        gspProxyStartAddr    = (g_gspRmProxyDesc.gspRmProxyBlobPa - LW_RISCV_AMAP_SYSGPA_START) >> 8;
    }
    else
    {
        return ACR_ERROR_VARIABLE_SIZE_OVERFLOW;
    }
    // This region is gspRmProxy region in sysmem
    dmaPropRead.wprBase  = gspProxyStartAddr;    
    dmaPropRead.regionID = 0;                   
    dmaPropRead.ctxDma   = RM_GSP_DMAIDX_PHYS_SYS_COH_FN0;

    if (g_gspRmProxyDesc.target == RM_FBIF_TRANSCFG_TARGET_NONCOHERENT_SYSTEM)
    {
        dmaPropRead.ctxDma = RM_GSP_DMAIDX_PHYS_SYS_NCOH_FN0;        
    }

    // The DMA PROP is configured and used to write into the WPR2 Region
    dmaPropWrite.wprBase  = g_dmaProp.wprBase;
    dmaPropWrite.ctxDma   = g_dmaProp.ctxDma;
    dmaPropWrite.regionID = g_dmaProp.regionID;

    // Clear the buffer before performing any operations
    acrMemset_HAL(pAcr, g_tmpGenericBuffer, 0x0, ACR_GENERIC_BUF_SIZE_IN_BYTES);

    while (bytesCopied < totalCopySizeInBytes)
    {
       // Set the number of bytes to be copied
        if ((totalCopySizeInBytes - bytesCopied) >= ACR_GENERIC_BUF_SIZE_IN_BYTES)
        {
            bytesXfered = ACR_GENERIC_BUF_SIZE_IN_BYTES;
        }
        else
        {
            bytesXfered = totalCopySizeInBytes - bytesCopied;
        }

        // Fetch the content from the SysMem address to the buffer
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrIssueDma_HAL(pAcr, (void *)g_tmpGenericBuffer , LW_FALSE, 0,
                            bytesXfered, ACR_DMA_FROM_SYS, ACR_DMA_SYNC_AT_END, &dmaPropRead));
 
        // Write the Buffer content into the WPR2 region
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrIssueDma_HAL(pAcr, (void *)g_tmpGenericBuffer , LW_FALSE, 0,
                            bytesXfered, ACR_DMA_TO_FB, ACR_DMA_SYNC_AT_END, &dmaPropWrite));
  
        // Increment the offsets of SysMem and WPR
        bytesCopied         += bytesXfered;
        dmaPropRead.wprBase  = dmaPropRead.wprBase  + (bytesXfered >> 8);
        dmaPropWrite.wprBase = dmaPropWrite.wprBase + (bytesXfered >> 8);
    }

    // Clear the buffer post operation
    acrMemset_HAL(pAcr, g_tmpGenericBuffer, 0x0, ACR_GENERIC_BUF_SIZE_IN_BYTES);
  
    return status;
}

/*
 * @brief Setup SubWpr for code and data part of GSP-RM Ucode.
 *
 * @param[in] failingEngine falconId if Setup Subwpr fails.
 * @param[in] wprIndex      WPR region Index
 */
ACR_STATUS
acrSetupSubWprForWpr2_GH100
(
    LwU32 *failingEngine,
    LwU32  wprIndex
)
{
    ACR_STATUS               status            = ACR_OK;
    LwU32                    falconId;
    PLSF_WPR_HEADER_WRAPPER  pWprHeaderWrapper = NULL;
    PLSF_LSB_HEADER_WRAPPER  pLsbHeaderWrapper = (PLSF_LSB_HEADER_WRAPPER)&g_lsbHeaderWrapper;

    // Read the WPR2 header into heap first
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrReadAllWprHeaderWrappers_HAL(pAcr, wprIndex));

    // Get WPR2 header,assuming we have only GSP-RM Image in WPR2.
    pWprHeaderWrapper = GET_WPR_HEADER_WRAPPER(0);
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfWprHeaderWrapperCtrl_HAL(pAcr, LSF_WPR_HEADER_COMMAND_GET_FALCON_ID,
                                                                            pWprHeaderWrapper, (void *)&falconId));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrReadLsbHeaderWrapper_HAL(pAcr, pWprHeaderWrapper, pLsbHeaderWrapper));

    if ((status = acrSetupFalconCodeAndDataSubWprsExt_HAL(pAcr, falconId, pLsbHeaderWrapper)) != ACR_OK)
    {
       *failingEngine = falconId;
        return status;
    }
    return status;
}
