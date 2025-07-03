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
 * @file: acr_wprt210.c
 */

//
// Includes
//
#include "acr.h"
#include "external/pmuifcmn.h"
#include "dev_fb.h"
#include "dev_pwr_csb.h"
#include "hwproject.h"
#include "external/lwmisc.h"

#include "config/g_acr_private.h"

//
// Global Variables
//

LSF_WPR_HEADER    g_pWprHeader[LSF_WPR_HEADERS_TOTAL_SIZE_MAX/sizeof(LSF_WPR_HEADER)] ATTR_OVLY(".data")ATTR_ALIGNED(LSF_WPR_HEADER_ALIGNMENT); /*!< g_pWprHeader holds the WPR header of the ucode blob present in WPR. LSF_WPR_HEADERS_TOTAL_SIZE_MAX is sizeof(LSF_WPR_HEADER) * LSF_FALCON_ID_END. i.e 264 Bytes */
ACR_SCRUB_STATE   g_scrubState ATTR_OVLY(".data")ATTR_ALIGNED(ACR_SCRUB_ZERO_BUF_ALIGN); /*!< g_scrubState holds scrub offset of WPR */
LwU32 g_copyBuffer1[DMA_SIZE]        ATTR_ALIGNED(0x100); /*!< g_copyBuffer1 is global buffer used for copying data from non-wpr to WPR region. */
LwU32 g_copyBuffer2[DMA_SIZE]        ATTR_ALIGNED(0x100); /*!< g_copyBuffer2 is global buffer used for copying data from non-wpr to WPR region. */

//
// Function Definitions
//
#ifdef ACR_MMU_INDEXED
/*!
 * @brief Check if Locked WPR is present\n
 *        Read MMU_WPR_INFO register to know WPR start/end address.\n
 *        Make sure ucodeBlobsize is not greater than wpr size.\n
 *        Read READ/WRITE mask to ascertain WPR is created with correct permissions.\n
 *        If WPR region is already known, validate it using regionprops.\n
 *        Or else
 *        Update region properties(regionProp) in g_desc with the WPR info found i.e. start/end address, read/write mask, wprRegion ID. 
 *
 * @param[in]  pWprIndex                     Index into g_desc region properties holding wpr region
 *
 *   @global_START
 *   @global_{g_desc.regions , Region descriptors}
 *   @global_{g_desc.wprRegionID , wprRegionID specified by lwgpu-rm}
 *   @global_END
 *
 * @return     ACR_OK                        If ACR regions are locked 
 * @return     ACR_ERROR_ILWALID_REGION_ID   If ACR region details fetched doesn't match that of RM
 * @return     ACR_ERROR_NO_WPR              If no valid WPR region
 */
ACR_STATUS
acrFindWprRegions_T210
(
    LwU32 *pWprIndex
)
{
    ACR_STATUS               status          = ACR_OK;
    LwU32                    cmd             = 0;
    LwU32                    i               = 0;
    LwU64                    start           = 0;
    LwU64                    end             = 0;
    LwU32                    writeMask       = 0;
    LwU32                    readMask        = 0;
    LwBool                   bWprRegionFound = LW_FALSE;

    *pWprIndex = 0xFFFFFFFF;
    
    // Find WPR region
    for(i = 0; i < LW_MMU_WRITE_PROTECTION_REGIONS; i++)
    {
        // Read ReadMask
        cmd = FLD_SET_DRF(_PFB, _PRI_MMU_WPR, _INFO_INDEX, _ALLOW_READ, 0);
        ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR_INFO, cmd);
        readMask = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_INFO);
        readMask = DRF_IDX_VAL( _PFB, _PRI_MMU_WPR_INFO, _ALLOW_READ_WPR, i + 1U, readMask);

        // Read WriteMask
        cmd = FLD_SET_DRF(_PFB, _PRI_MMU_WPR, _INFO_INDEX, _ALLOW_WRITE, 0);
        ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR_INFO, cmd);
        writeMask = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_INFO);
        writeMask = DRF_IDX_VAL( _PFB, _PRI_MMU_WPR_INFO, _ALLOW_READ_WPR, i + 1U, writeMask);

        //
        // TODO: This is needed because of a bug in manual defines. This will be removed once the
        // the bug is fixed.
        //
        writeMask &= 0xFU;
        // TODO: Change expected RMASK to correct value after RMASK of WPR is changed in CHEETAH RM & Bootloaders.
        if ((readMask  <= LSF_WPR_REGION_RMASK_FOR_TEGRA) && (writeMask == LSF_WPR_REGION_WMASK))
        {
            // Found WPR region

            // Read start address
            cmd = FLD_SET_DRF_IDX(_PFB, _PRI_MMU_WPR, _INFO_INDEX, _WPR_ADDR_LO, i + 1U, 0U);
            ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR_INFO, cmd);
            start = (LwU64)ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_INFO);
            start = DRF_VAL(_PFB, _PRI_MMU_WPR_INFO, _DATA, start);
            start = start << LW_PFB_PRI_MMU_WPR_INFO_ADDR_ALIGNMENT;

            // Read end address
            cmd = FLD_SET_DRF_IDX(_PFB, _PRI_MMU_WPR, _INFO_INDEX, _WPR_ADDR_HI, i + 1U, 0U);
            ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR_INFO, cmd);
            end = (LwU64)ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_INFO);
            end = DRF_VAL(_PFB, _PRI_MMU_WPR_INFO, _DATA, end);
            end = end << LW_PFB_PRI_MMU_WPR_INFO_ADDR_ALIGNMENT;

            if (start < end)
            {
                // End address always point to start of the last aligned address
                if (g_desc.ucodeBlobSize > (LwU32)(end - start))
                {
                    return ACR_ERROR_NO_WPR;
                }
                end += ACR_REGION_ALIGN;
                start >>= LW_PFB_PRI_MMU_WPR_INFO_ADDR_ALIGNMENT;
                end   >>= LW_PFB_PRI_MMU_WPR_INFO_ADDR_ALIGNMENT;
                bWprRegionFound = LW_TRUE;
            }
            else
            {
                start = end = 0;
            }

            break;
        }
    }

    if (bWprRegionFound && (g_desc.regions.noOfRegions > 0U))
    {
        //Found a valid WPR region
        PRM_FLCN_ACR_REGION_PROP regionProp;

        if ((i+1U) != LSF_WPR_EXPECTED_REGION_ID)
        {
            return ACR_ERROR_ILWALID_REGION_ID;
        }

        regionProp = &(g_desc.regions.regionProps[i]);

        if (g_desc.wprRegionID > 0U)
        {
            //
            // If WPR region is already known, validate it.
            // This is the case when RM loads ACR binary.
            //
            if (i >= g_desc.regions.noOfRegions)
            {
                return ACR_ERROR_ILWALID_REGION_ID;
            }

            if (regionProp->regionID != g_desc.wprRegionID)
            {
                return ACR_ERROR_ILWALID_REGION_ID;
            }

            if ((start     != regionProp->startAddress) ||
                (end       != regionProp->endAddress)   ||
                (readMask  != regionProp->readMask)     ||
                (writeMask != regionProp->writeMask))
            {
                return ACR_ERROR_NO_WPR;
            }
        }
        else
        {
            //
            // If WPR region is unknown, populate it.
            // This is the case when Android loads ACR binary.
            // TODO check start and end addresses for overflow.
            //
            g_desc.wprRegionID       = i + 1U;
            regionProp->startAddress = (LwU32)start;
            regionProp->endAddress   = (LwU32)end;
            regionProp->readMask     = readMask;
            regionProp->writeMask    = writeMask;
        }

        *pWprIndex = i;
    }
    else
    {
        if (g_desc.wprRegionID > 0U)
        {
            status = ACR_ERROR_ILWALID_REGION_ID;
        }
        else
        {
            status = ACR_ERROR_NO_WPR;
        }
    }

    return status;
}
#endif

/*!
 * @brief Reads WPR header into global buffer(g_pWprHeader).
 *        It uses Falcon DMA controller to copy WPR headers from WPR to global buffer.\n
 *
 *   @global_START
 *   @global_{g_pWprHeader , Global buffer containing copy of WPR headers.}
 *   @global_{g_dmaProp , Global variable containing DMA properties wprBase, ctxDma, regionID populated by DMA unit.}
 *   @global_END
 *
 * @return ACR_OK                  If DMA controller successfully copies(DMAs) WPR headers from WPR to global buffer.
 * @return ACR_ERROR_DMA_FAILURE   If DMA controller fails to copy(DMA) WPR headers from WPR to global buffer.
 */
ACR_STATUS
acrReadWprHeader_GM200(void)
{
    LwU32  wprSize  = LW_ALIGN_UP((sizeof(LSF_WPR_HEADER) * LSF_FALCON_ID_END), 
                                LSF_WPR_HEADER_ALIGNMENT);

    // Read the WPR header
    if ((acrIssueDma_HAL(pAcr, (LwU32)g_pWprHeader, LW_FALSE, 0, wprSize, 
            ACR_DMA_FROM_FB, ACR_DMA_SYNC_AT_END, &g_dmaProp)) != wprSize) 
    {
        return ACR_ERROR_DMA_FAILURE; 
    }

    return ACR_OK;
}

/*!
 * @brief Writes/Updates WPR header contents from global buffer(g_pWprHeader) to headers present in WPR.
 *        It uses Falcon DMA controller to copy WPR headers from global buffer to WPR.\n
 *
 *   @global_START
 *   @global_{g_pWprHeader , Global buffer containing copy of WPR headers.}
 *   @global_{g_dmaProp , Global variable containing DMA properties wprBase, ctxDma, regionID populated by DMA unit.}
 *   @global_END
 *
 * @return ACR_OK                  If DMA controller successfully copies(DMAs) WPR headers from WPR to global buffer.
 * @return ACR_ERROR_DMA_FAILURE   If DMA controller fails to copy(DMA) WPR headers from WPR to global buffer.
 */
ACR_STATUS
acrWriteWprHeader_GM200(void)
{
    LwU32  wprSize  = LW_ALIGN_UP((sizeof(LSF_WPR_HEADER) * LSF_FALCON_ID_END), 
                                LSF_WPR_HEADER_ALIGNMENT);

    // Update WPR header
    if ((acrIssueDma_HAL(pAcr, (LwU32)g_pWprHeader, LW_FALSE, 0, wprSize, 
            ACR_DMA_TO_FB, ACR_DMA_SYNC_AT_END, &g_dmaProp)) != wprSize) 
    {
        return ACR_ERROR_DMA_FAILURE; 
    }
    
    return ACR_OK;
}

/*!
 * @brief Scrub FB with zeroes
 *
 * @param[in]  nextKnownOffset               offset upto which scrubbing needs to be done from last scrub point. 
 * @param[in]  size                          size of the valid region in WPR after nextKnownOffset.
 *
 *   @global_START
 *   @global_{g_scrubState.zeroBuf, Global buffer containing 0's used for scrubbing.It's size is 256 bytes.}
 *   @global_{g_scrubState.scrubTrack, WPR scrub tracker.}
 *   @global_END
 *
 * @return     ACR_OK                        If WPR scrubbing is successful.
 * @return     ACR_ERROR_DMA_FAILURE         If scrubbing fails due to DMA failure.
 * 
 */
ACR_STATUS
acrScrubUnusedWprWithZeroes_GM200
(
    LwU32              nextKnownOffset,
    LwU32              size
)
{
#ifdef ACR_LOAD_PATH
    LwS64         gap     = 0;

    if (g_scrubState.scrubTrack == 0U)
    {
        // Zero out the buf
        acrMemset(g_scrubState.zeroBuf, 0, ACR_SCRUB_ZERO_BUF_SIZE_BYTES);
    }

    {
        //Cast U32 to S64 to avoid loss of data (CERT-C INT30)
        //Safe as r-value cannot exceed 32 bits
        gap = ((LwS64)(nextKnownOffset) - (LwS64)(g_scrubState.scrubTrack));

        if (gap > 0)
        {
            // DMA zeroes to this gap and update scrub tracker
#ifndef ACR_SKIP_SCRUB
            if ((acrIssueDma_HAL(pAcr, (LwU32)g_scrubState.zeroBuf, LW_FALSE, g_scrubState.scrubTrack, (LwU32)gap, 
                ACR_DMA_TO_FB_SCRUB, ACR_DMA_SYNC_NO, &g_dmaProp)) != (LwU32)gap) 
            {
                return ACR_ERROR_DMA_FAILURE; 
            }
#endif

        }

        if (gap >=  0)
        {
            CHECK_WRAP_AND_ERROR((LwU32)gap > (LW_U32_MAX - g_scrubState.scrubTrack));
            (g_scrubState.scrubTrack) += (LwU32)gap;
            CHECK_WRAP_AND_ERROR(size  > (LW_U32_MAX - g_scrubState.scrubTrack));
            (g_scrubState.scrubTrack) += size;
        }
    }

#endif


    return ACR_OK;
}

/*!
 * @brief  Copies ucode blob from non-WPR region to WPR region using non-WPR location passed from host(lwgpu-rm)\n
 *         It uses Falcon DMA controller to dma data from non-WPR region to WPR memory.
 *
 *   @global_START
 *   @global_{g_desc.ucodeBlobBase , Address of non-wpr blob passed by lwgpu-rm}
 *   @global_{g_desc.ucodeBlobSize , Size of non-wpr blob required to be passed to DMA controller for copying non-wpr blob to WPR}
 *   @global_END
 *
 * @return  ACR_OK                       If copy is successful
 * @return  ACR_ERROR_DMA_FAILURE        If copy failed
 */
ACR_STATUS acrCopyUcodesToWpr_T210(void)
{
    LwU32 copyCountRead, copyCountWrite, bytesCopied = 0;
    LwU32 *pBuffer1, *pBuffer2, *pTemp;
    LwU32 wprBaseTemp = 0;
    ACR_DMA_PROP dmaPropRead, dmaPropWrite;

    // 
    // On rail-gating exit, we don't need to copy the ucodes again to the WPR region,
    // since DRAM contents as retained.
    // So RM sets the ucode blob size as zero to skip the copying.
    //
#ifndef ACR_SAFE_BUILD
    if (g_desc.ucodeBlobSize == 0U)
    {
        return ACR_OK;
    }
#endif

    wprBaseTemp = lwU64Lo32(g_desc.ucodeBlobBase >> 8);
    if (wprBaseTemp == 0xDEADBEEFU)
    {
        return ACR_ERROR_VARIABLE_SIZE_OVERFLOW;
    }
    dmaPropRead.wprBase  = wprBaseTemp; 
    dmaPropRead.ctxDma   = RM_PMU_DMAIDX_PHYS_VID;
    dmaPropRead.regionID = 0; //as this region is non-WPR

    dmaPropWrite.wprBase  = g_dmaProp.wprBase;
    dmaPropWrite.ctxDma   = g_dmaProp.ctxDma;
    dmaPropWrite.regionID = g_dmaProp.regionID;

    pBuffer1 = g_copyBuffer1;
    pBuffer2 = g_copyBuffer2;

    copyCountRead = acrIssueDma_HAL(pAcr, (LwU32)pBuffer1, LW_FALSE, bytesCopied,
        DMA_SIZE, ACR_DMA_FROM_FB, ACR_DMA_SYNC_AT_END, &dmaPropRead);
    if(copyCountRead != DMA_SIZE) {
        return ACR_ERROR_DMA_FAILURE;
    }

    while(bytesCopied < (g_desc.ucodeBlobSize - DMA_SIZE))
    {
        copyCountWrite = acrIssueDma_HAL(pAcr, (LwU32)pBuffer1, LW_FALSE, bytesCopied,
            DMA_SIZE, ACR_DMA_TO_FB, ACR_DMA_SYNC_NO, &dmaPropWrite);
        if(copyCountWrite != DMA_SIZE) {
            return ACR_ERROR_DMA_FAILURE;
        }

        bytesCopied += copyCountWrite;

        copyCountRead = acrIssueDma_HAL(pAcr, (LwU32)pBuffer2, LW_FALSE, bytesCopied,
            DMA_SIZE, ACR_DMA_FROM_FB, ACR_DMA_SYNC_NO, &dmaPropRead);
        if(copyCountRead != DMA_SIZE) {
            return ACR_ERROR_DMA_FAILURE;
        }

        falc_dmwait();

        pTemp = pBuffer1;
        pBuffer1 = pBuffer2;
        pBuffer2 = pTemp;
    }

    copyCountWrite = acrIssueDma_HAL(pAcr, (LwU32)pBuffer1, LW_FALSE, bytesCopied,
        DMA_SIZE, ACR_DMA_TO_FB, ACR_DMA_SYNC_AT_END, &dmaPropWrite);
    if(copyCountWrite != DMA_SIZE) {
        return ACR_ERROR_DMA_FAILURE;
    }

    return ACR_OK;
}
