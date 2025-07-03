/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: acr_lock_wpr_gh100.c
 */
/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "acr.h"
#include "dev_gc6_island.h"
#include "dev_pri_ringstation_sys_addendum.h"
#include "dev_fb.h"
#include "dev_gc6_island_addendum.h"
#include "mmu/mmucmn.h"
#include "sec2/sec2ifcmn.h"
#include "hwproject.h"
#include "config/g_acr_private.h"

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ External Definitions --------------------------- */
extern RM_FLCN_ACR_DESC g_desc;
extern ACR_DMA_PROP     g_dmaProp;


/*
 * @brief Allow or drop ACR access to WPR1 region
 *
 * @param[in] bGrantSubWprAccess Grant or drop ACR access to WPR1 region.
 *
 * @return    ACR_OK             If subwpr setup/removal is successful.
 */
ACR_STATUS
acrConfigureSubWprForAcr_GH100(LwBool bGrantSubWprAccess)
{
    ACR_STATUS status    = ACR_OK;
    LwU32 falconId       = LSF_FALCON_ID_GSP_RISCV;
    LwU32 wprStartAddr4K = ILWALID_WPR_ADDR_LO;
    LwU32 wprEndAddr4K   = ILWALID_WPR_ADDR_HI;

    if (bGrantSubWprAccess == LW_TRUE)
    {

        wprStartAddr4K = DRF_VAL(_PFB, _PRI_MMU_WPR1_ADDR_LO, _VAL,
                                ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR1_ADDR_LO));

        wprEndAddr4K   = DRF_VAL(_PFB, _PRI_MMU_WPR1_ADDR_HI, _VAL,
                                ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR1_ADDR_HI));
        wprEndAddr4K  += (ACR_REGION_ALIGN >> LW_PFB_PRI_MMU_WPR1_ADDR_HI_ALIGNMENT);
        wprEndAddr4K  -= 1;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, falconId, LSF_FALCON_INSTANCE_DEFAULT_0, GSP_SUB_WPR_ID_2_ASB_FULL_WPR1,
                                      wprStartAddr4K, wprEndAddr4K, ACR_SUB_WPR_MMU_RMASK_L3, ACR_SUB_WPR_MMU_WMASK_L3, LW_TRUE));
 
    }
    else
    {
    // Drop ACR access to WPR region
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, falconId, LSF_FALCON_INSTANCE_DEFAULT_0, GSP_SUB_WPR_ID_2_ASB_FULL_WPR1,
                                     ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));            
    }

    return status;
}

/*!
 * @brief  Copies ucodes from non-WPR region to WPR region using non-WPR location passed from host
 *
 * @return  ACR_OK                       If copy is successful
 * @return  ACR_ERROR_FLCN_ID_NOT_FOUND  If the falcon config failed
 * @return  ACR_ERROR_DMA_FAILURE        If copy failed
 */
ACR_STATUS acrShadowCopyOfWpr_GH100(LwU32 wprIndex)
{
    LwU32             totalCopySizeInBytes;
    LwU32             bytesCopied   = 0;
    ACR_DMA_PROP      dmaPropRead;
    ACR_DMA_PROP      dmaPropWrite;
    ACR_STATUS        status        = ACR_OK;
    LwU32             bytesXfered = 0;

    // If a shadow copy of the wpr is present copy that.
    if (g_desc.regions.regionProps[wprIndex].shadowMemStartAddress == 0)
    {
        // nothing to do. Return now.
        return ACR_OK;
    }

    //
    // regionProps[wprIndex].endAddress and regionProps[wprIndex].startAddress are both 256 byte aligned.
    //
    totalCopySizeInBytes = ((g_desc.regions.regionProps[wprIndex].endAddress -
                             g_desc.regions.regionProps[wprIndex].startAddress + 1) << 8);

    //
    // Set up the read ctxDma to be a different aperture from the one used to
    // write into WPR. It has to be different since the code doing the shadow
    // copy below will ping pong DMA reads and writes between these two
    // apertures, and it is never safe to change aperture REGIONCFG settings
    // while DMA requests are still in the pipeline.
    //

    // This region is non-WPR region
    dmaPropRead.wprBase  = g_desc.regions.regionProps[wprIndex].shadowMemStartAddress;
    dmaPropRead.regionID = 0;
    ct_assert(LSF_BOOTSTRAP_CTX_DMA_BOOTSTRAP_OWNER != RM_SEC2_DMAIDX_PHYS_VID_FN0);
    dmaPropRead.ctxDma   = RM_GSP_DMAIDX_PHYS_VID_FN0;

    // The DMA PROP is configured and used to write into the WPR Region
    dmaPropWrite.wprBase  = g_dmaProp.wprBase;
    dmaPropWrite.ctxDma   = RM_GSP_DMAIDX_UCODE;
    dmaPropWrite.regionID = g_dmaProp.regionID;

    // Clear the buffer before performing any operations
    acrMemset_HAL(pAcr, g_tmpGenericBuffer, 0x0, ACR_GENERIC_BUF_SIZE_IN_BYTES);

    // Copy the entire contents of Shadow memory to WPR
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

        // Fetch the content from Shadow Copy address to Buffer
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrIssueDma_HAL(pAcr, (void *)g_tmpGenericBuffer , LW_FALSE, 0,
                            bytesXfered, ACR_DMA_FROM_FB, ACR_DMA_SYNC_AT_END, &dmaPropRead));

        // Write the contents of the buffer to WPR
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrIssueDma_HAL(pAcr, (void *)g_tmpGenericBuffer , LW_FALSE, 0,
                            bytesXfered, ACR_DMA_TO_FB, ACR_DMA_SYNC_AT_END, &dmaPropWrite));


        // Increment the offsets of ShadowMem and WPR
        bytesCopied         += bytesXfered;
        dmaPropRead.wprBase  = dmaPropRead.wprBase + (bytesXfered >> 8);
        dmaPropWrite.wprBase = dmaPropWrite.wprBase + (bytesXfered >> 8);
    }

    // Clear the buffer post operation
    acrMemset_HAL(pAcr, g_tmpGenericBuffer, 0x0, ACR_GENERIC_BUF_SIZE_IN_BYTES);

    return status;
}

/*
 * Lock WPR1 region in MMU.
 */
ACR_STATUS
acrLockWpr1_GH100
(
    LwU32 *pWprIndex
)
{
    ACR_STATUS               status         = ACR_OK;
    // StartAddress and EndAddress are already right shifted by 8 to make it fit into 32 bit int
    LwU32                    defAlign       = (LW_PFB_PRI_MMU_WPR1_ADDR_LO_ALIGNMENT - 8);
    LwU32                    data           = 0;
    RM_FLCN_ACR_REGION_PROP *pRegionProp;
    LwU32                    wprStartAddr4K = ILWALID_WPR_ADDR_LO;
    LwU32                    wprEndAddr4K   = ILWALID_WPR_ADDR_HI;

    // Updated pWprIndex to WPR1 for further setup of DMA for shadow WPR and LS falcon bootstrap
    if (pWprIndex == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }
    *pWprIndex = ACR_WPR1_REGION_IDX;

    pRegionProp = &(g_desc.regions.regionProps[ACR_WPR1_REGION_IDX]);

    if (pRegionProp->regionID != LSF_WPR_EXPECTED_REGION_ID)
    {
        return ACR_ERROR_ILWALID_REGION_ID;
    }

    // If both the address are same, please ignore this region and continue
    if (pRegionProp->startAddress >= pRegionProp->endAddress)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    //
    //  Program the MMU registers with start/end address
    //  WPR MMU register accepts address in 4K. In RM we already make startAddress and endAddress aligned to 256.
    //  So we just need to shift right 4 bits to get 4K aligned address.
    //
    wprStartAddr4K = (pRegionProp->startAddress)   >> defAlign;
    wprEndAddr4K   = (pRegionProp->endAddress - 1) >> defAlign;

    data = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR1_ADDR_LO);
    data = FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR1_ADDR_LO, _VAL, wprStartAddr4K, data);
    ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR1_ADDR_LO, data);

    data = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR1_ADDR_HI);
    data= FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR1_ADDR_HI, _VAL, wprEndAddr4K, data);
    ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR1_ADDR_HI, data);

    data = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_READ);
    data = FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR_ALLOW_READ, _WPR1, LSF_WPR_REGION_RMASK_SUB_WPR_ENABLED, data);
    data = FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR_ALLOW_READ, _MIS_MATCH, LSF_WPR_REGION_ALLOW_READ_MISMATCH_NO, data);
    ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_READ, data);

    data = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_WRITE);
    data = FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR_ALLOW_WRITE, _WPR1, LSF_WPR_REGION_WMASK_SUB_WPR_ENABLED, data);
    data = FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR_ALLOW_WRITE, _MIS_MATCH, LSF_WPR_REGION_ALLOW_WRITE_MISMATCH_NO, data);
    ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_WRITE, data);

    //
    // In Fmodel fuse opt_sub_region_no_override is set to false, so atleast one bit of sub-wpr index 0 allow_read or allow_write 
    // must be set to enable sub-wpr region check for that falcon, otherwise WPR permissions apply to programmed subwpr region.
    // Tracked in Bug 200564246.  
    //
#ifdef ACR_FMODEL_BUILD
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_SEC2, LSF_FALCON_INSTANCE_DEFAULT_0, SEC2_SUB_WPR_ID_0_ACRLIB_FULL_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_SUB_WPR_MMU_RMASK_L3, ACR_SUB_WPR_MMU_WMASK_ALL_LEVELS_DISABLED, LW_TRUE));
#endif

    //
    // ASB runs on GSP and needs read only access to WPR1 to load SEC2 RTOS.
    // ASB self clears its access to SEC2 RTOS at the end of ASB binary.
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_GSP_RISCV, LSF_FALCON_INSTANCE_DEFAULT_0, GSP_SUB_WPR_ID_2_ASB_FULL_WPR1,
                                        wprStartAddr4K, wprEndAddr4K, ACR_SUB_WPR_MMU_RMASK_L3, ACR_SUB_WPR_MMU_WMASK_L3, LW_TRUE));

    return status;
}

/*
 * Lock WPR2 region in MMU.
 */
ACR_STATUS
acrLockWpr2_GH100
(
    LwU32 *pWprIndex
)
{
    ACR_STATUS              status           = ACR_OK;
    LwU32                   data             = 0;
    // StartAddress and EndAddress are already right shifted by 8 to make it fit into 32 bit int
    LwU32                   defAlign         = (LW_PFB_PRI_MMU_WPR2_ADDR_LO_ALIGNMENT - 8);
    RM_FLCN_ACR_REGION_PROP *pRegionProp;
    LwU32                   wprStartAddr4K   = ILWALID_WPR_ADDR_LO;
    LwU32                   wprEndAddr4K     = ILWALID_WPR_ADDR_HI;

    if (pWprIndex == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }
    *pWprIndex = ACR_WPR2_REGION_IDX;

    pRegionProp = &(g_desc.regions.regionProps[ACR_WPR2_REGION_IDX]);

    if (pRegionProp->regionID != ACR_WPR2_MMU_REGION_ID)
    {
        return ACR_ERROR_ILWALID_REGION_ID;
    }

    // If both the address are same, please ignore this region and continue
    if (pRegionProp->startAddress >= pRegionProp->endAddress)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    //
    // Program the MMU registers with start/end address
    // WPR MMU register accepts address in 4K.We have startAddress and endAddress aligned to 256.
    // So we just need to shift right 4 bits to get 4K aligned address.
    //
    wprStartAddr4K = (pRegionProp->startAddress)   >> defAlign;
    wprEndAddr4K   = (pRegionProp->endAddress - 1) >> defAlign;

    data = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_LO);
    data = FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR2_ADDR_LO, _VAL, wprStartAddr4K, data);
    ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_LO, data);

    data = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_HI);
    data = FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR2_ADDR_HI, _VAL, wprEndAddr4K, data);
    ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_HI, data);

    data = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_READ);
    data = FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR_ALLOW_READ, _WPR2, LSF_WPR_REGION_RMASK_SUB_WPR_ENABLED, data);
    data = FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR_ALLOW_READ, _MIS_MATCH, LSF_WPR_REGION_ALLOW_READ_MISMATCH_NO, data);
    ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_READ, data);

    data = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_WRITE);
    data = FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR_ALLOW_WRITE, _WPR2, LSF_WPR_REGION_WMASK_SUB_WPR_ENABLED, data);
    data = FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR_ALLOW_WRITE, _MIS_MATCH, LSF_WPR_REGION_ALLOW_WRITE_MISMATCH_NO, data);
    ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_WRITE, data);

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_GSP_RISCV, LSF_FALCON_INSTANCE_DEFAULT_0, GSP_SUB_WPR_ID_3_ACR_FULL_WPR2,
                                        wprStartAddr4K, wprEndAddr4K, ACR_SUB_WPR_MMU_RMASK_L3, ACR_SUB_WPR_MMU_WMASK_L3, LW_TRUE));

    return status;
}

