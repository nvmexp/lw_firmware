/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: acr_lock_wpr_tu10x.c
 */
#include "acr.h"
#include "dev_gc6_island.h"
#include "dev_pri_ringstation_sys_addendum.h"
#include "dev_fb.h"
#include "dev_gc6_island_addendum.h"
#include "dev_pwr_csb.h"
#include "mmu/mmucmn.h"
#include "sec2/sec2ifcmn.h"
#include "hwproject.h"

#if defined(AHESASC) || defined(ASB) || defined(BSI_LOCK)
#include "dev_sec_csb.h"
#endif

#include "config/g_acr_private.h"
#include "config/g_acrlib_private.h"

#ifdef AHESASC
LwBool            g_bIsDebug           ATTR_OVLY(".data") = LW_FALSE;
#endif
// DMA_SIZE is in bytes. These buffers are LwU32.
LwU32 g_copyBufferA[COPY_BUFFER_A_SIZE_DWORD]        ATTR_ALIGNED(0x100);
LwU32 g_copyBufferB[COPY_BUFFER_B_SIZE_DWORD]        ATTR_ALIGNED(0x100);

//
// Extern Variables
//
extern RM_FLCN_ACR_DESC g_desc;
extern ACR_DMA_PROP     g_dmaProp;

#ifdef AHESASC
static
ACR_STATUS _acrShadowCopyOfWpr(LwU32 wprIndex);
#endif

#ifdef AHESASC
/*!
 * @brief ACR routine to check if LOCAL_MEM_RANGE is setup or not
 * It is expected that UDE will program LOCAL_MEMORY_RANGE GP10X onwards. This function helps ensure that it is not possible to run ACR load without running UDE
 */
ACR_STATUS
acrCheckIfMemoryRangeIsSet_TU10X(void)
{
    if (ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_LOCAL_MEMORY_RANGE) == 0)
    {
        return ACR_ERROR_LOCAL_MEM_RANGE_IS_NOT_SET;
    }
    return ACR_OK;
}
#endif //AHESASC

/*
 * @brief Copy Frts data from WPR2(setup by FWSEC) to WPR1
 */
ACR_STATUS
acrCopyFrtsData_TU10X(void)
{
    ACR_STATUS      status = ACR_OK;
    ACR_FLCN_CONFIG flcnCfg;
    ACR_DMA_PROP    dmaPropRead;
    ACR_DMA_PROP    dmaPropWrite;
    LwU32           falconId = LSF_FALCON_ID_SEC2;

    LwU32 totalCopySize;
    LwU32 bytesCopied;
    LwU32 *pBuffer1 = g_copyBufferA;
    LwU32 *pBuffer2 = g_copyBufferB;
    LwU32 *pTemp;
    // FRTS start address in WPR2
    LwU32 frtsStartAddress = 0;

    // Check if ACR can start copying FRTS data
    LwU32 regFrtsStatus = ACR_REG_RD32(BAR0, LW_PGC6_AON_FRTS_OUTPUT_FLAG_SELWRE_SCRATCH_GROUP_03_2);
    if (!FLD_TEST_DRF( _PGC6, _AON_FRTS_OUTPUT_FLAG_SELWRE_SCRATCH_GROUP_03_2, _ACR_READY, _YES, regFrtsStatus))
    {
        // FWSEC has not setup FRTS data, so no action on ACR
        return ACR_OK;
    }

    // Get FRTS data source address (WPR2 setup by FWSEC)
    LwU64 wpr2End   = DRF_VAL( _PFB, _PRI_MMU_WPR2_ADDR_HI, _VAL, ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_HI));
    wpr2End = ((wpr2End << LW_PFB_PRI_MMU_WPR2_ADDR_LO_ALIGNMENT) + ACR_REGION_ALIGN - 1) >> LW_PFB_PRI_MMU_WPR2_ADDR_LO_ALIGNMENT;

#if ACR_GSP_RM_BUILD
    // Update start address to copy only FRTS data from WPR2 to WPR1 since WPR2 = (GSP RM Image + FRTS data) for pre-hopper authenticated GSP RM build.
    LwU32 frtsSize   = LW_PGC6_AON_FRTS_INPUT_WPR_SIZE_SELWRE_SCRATCH_GROUP_03_0_WPR_SIZE_1MB_IN_4K;
    if(wpr2End >= frtsSize)
    {
        frtsStartAddress = ((LwU32)wpr2End - frtsSize + 1);
    }
    else
    {
        return ACR_ERROR_VARIABLE_SIZE_OVERFLOW;
    }
#else
    LwU32 wpr2Start  = DRF_VAL( _PFB, _PRI_MMU_WPR2_ADDR_LO, _VAL, ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_LO));
    frtsStartAddress = wpr2Start;
#endif

    // Setup ACR falcon(SEC2) subWPR corresponding to WPR2, in order to access it
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, falconId, SEC2_SUB_WPR_ID_2_FRTS_DATA_WPR2,
                                        frtsStartAddress, wpr2End, ACR_SUB_WPR_MMU_RMASK_L3, ACR_SUB_WPR_MMU_WMASK_ALL_LEVELS_DISABLED, LW_FALSE));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetFalconConfig_HAL(pAcrlib, falconId, LSF_FALCON_INSTANCE_DEFAULT_0, &flcnCfg));

    //
    // Populate DMA params for read from WPR2
    // wprBase  = Get the offset from FRTS info secure scratch
    // regionID = WPR2
    // ctxDma   = Different than WPR1, so using index RM_SEC2_DMAIDX_PHYS_VID_FN0
    // Note that this index is also used during _acrShadowCopyOfWpr, but can be reused here as they are sequential
    //
    dmaPropRead.wprBase  = (frtsStartAddress << 4);
    dmaPropRead.regionID = ACR_WPR2_MMU_REGION_ID;
    dmaPropRead.ctxDma   = RM_SEC2_DMAIDX_PHYS_VID_FN0;
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibSetupCtxDma_HAL(pAcrlib, &flcnCfg, dmaPropRead.ctxDma, LW_TRUE));

    //
    // Populate DMA params for write to WPR1 FRTS subWpr
    // wprBase  = Get the offset from FRTS falcon FbFlcn subWpr startAddr, instead of parsing subWprHeader again
    //
    LwU32 frtsWpr1Start = DRF_VAL(_PFB, _PRI_MMU_FALCON_FBFALCON_CFGA, _ADDR_LO,
                                    ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_FALCON_FBFALCON_CFGA(FBFALCON_SUB_WPR_ID_2_FRTS_DATA_WPR1)));
    dmaPropWrite.wprBase   = (frtsWpr1Start << 4);
    dmaPropWrite.regionID = g_dmaProp.regionID;
    dmaPropWrite.ctxDma   = g_dmaProp.ctxDma;

    // Set DMACTL_REQUIRE_CTX_FALSE
    acrlibSetupDmaCtl_HAL(pAcrlib, &flcnCfg, LW_FALSE);

    // Set FBIF_CTL_ALLOW_PHYS_NO_CTX_ALLOW
    LwU32 fbifCtl = ACR_REG_RD32_STALL(CSB, LW_CSEC_FBIF_CTL);
    fbifCtl = FLD_SET_DRF(_CSEC, _FBIF_CTL, _ALLOW_PHYS_NO_CTX, _ALLOW, fbifCtl);
    ACR_REG_WR32_STALL(CSB, LW_CSEC_FBIF_CTL, fbifCtl);

    // Start DMA from WPR2 to WPR1
    bytesCopied = 0;
    ct_assert((LW_PGC6_AON_FRTS_INPUT_WPR_SIZE_SELWRE_SCRATCH_GROUP_03_0_WPR_SIZE_1MB_IN_4K << SHIFT_4KB) > DMA_SIZE);
    ct_assert(((LW_PGC6_AON_FRTS_INPUT_WPR_SIZE_SELWRE_SCRATCH_GROUP_03_0_WPR_SIZE_1MB_IN_4K << SHIFT_4KB) % DMA_SIZE) == 0);
    totalCopySize = LW_PGC6_AON_FRTS_INPUT_WPR_SIZE_SELWRE_SCRATCH_GROUP_03_0_WPR_SIZE_1MB_IN_4K << SHIFT_4KB;

    if (acrIssueDma_HAL(pAcr, pBuffer1, LW_FALSE, 0,
            DMA_SIZE, ACR_DMA_FROM_FB, ACR_DMA_SYNC_AT_END, &dmaPropRead) != DMA_SIZE)
    {
        return ACR_ERROR_DMA_FAILURE;
    }
    while (bytesCopied < (totalCopySize - DMA_SIZE))
    {
        if (acrIssueDma_HAL(pAcr, pBuffer1, LW_FALSE, 0, DMA_SIZE,
                ACR_DMA_TO_FB, ACR_DMA_SYNC_NO, &dmaPropWrite) != DMA_SIZE)
        {
            return ACR_ERROR_DMA_FAILURE;
        }

        bytesCopied += DMA_SIZE;
        dmaPropRead.wprBase = dmaPropRead.wprBase + (DMA_SIZE >> 8);
        dmaPropWrite.wprBase = dmaPropWrite.wprBase + (DMA_SIZE >> 8);

        if ((acrIssueDma_HAL(pAcr, pBuffer2, LW_FALSE, 0, DMA_SIZE,
                ACR_DMA_FROM_FB, ACR_DMA_SYNC_NO, &dmaPropRead)) != DMA_SIZE)
        {
            return ACR_ERROR_DMA_FAILURE;
        }

        falc_dmwait();

        pTemp = pBuffer1;
        pBuffer1 = pBuffer2;
        pBuffer2 = pTemp;
    }
    if (acrIssueDma_HAL(pAcr, pBuffer1, LW_FALSE, 0, DMA_SIZE,
                ACR_DMA_TO_FB, ACR_DMA_SYNC_AT_END, &dmaPropWrite) != DMA_SIZE)
    {
        return ACR_ERROR_DMA_FAILURE;
    }

    // Clear ACR falcon(SEC2) subWPR corresponding to WPR2 for FRTS
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, falconId, SEC2_SUB_WPR_ID_2_FRTS_DATA_WPR2,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_FALSE));

// Skip tearing down WPR2 since WPR2 = (GSP RM Image + FRTS data) for pre-hopper authenticated GSP RM build.
#ifndef ACR_GSP_RM_BUILD
    // Clear WPR2 setup by FWSEC FRTS command
    wpr2Start = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_LO);
    wpr2Start = FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR2_ADDR_LO, _VAL, ILWALID_WPR_ADDR_LO, wpr2Start);
    ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_LO, wpr2Start);

    wpr2End =  ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_HI);
    wpr2End = FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR2_ADDR_HI, _VAL, ILWALID_WPR_ADDR_HI, wpr2End);
    ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_HI, wpr2End);

    LwU32 wpr2AllowRead = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_READ);
    wpr2AllowRead =  FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR_ALLOW_READ, _WPR2, ACR_UNLOCK_READ_MASK, wpr2AllowRead);
    ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_READ, wpr2AllowRead);

    LwU32 wpr2AllowWrite = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_WRITE);
    wpr2AllowWrite =  FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR_ALLOW_WRITE, _WPR2, ACR_UNLOCK_WRITE_MASK, wpr2AllowWrite);
    ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_WRITE, wpr2AllowWrite);
#endif // ACR_GSP_RM_BUILD

    // Update FRTS status to clients
    LwU32 reg0301 = ACR_REG_RD32(BAR0, LW_PGC6_AON_FRTS_INPUT_WPR_OFFSET_SELWRE_SCRATCH_GROUP_03_1);
    reg0301 = FLD_SET_DRF_NUM( _PGC6, _AON_FRTS_INPUT_WPR_OFFSET_SELWRE_SCRATCH_GROUP_03_1, _WPR_OFFSET, frtsWpr1Start, reg0301);
    ACR_REG_WR32(BAR0, LW_PGC6_AON_FRTS_INPUT_WPR_OFFSET_SELWRE_SCRATCH_GROUP_03_1, reg0301);

    LwU32 reg0302 = ACR_REG_RD32(BAR0, LW_PGC6_AON_FRTS_INPUT_CTRL_SELWRE_SCRATCH_GROUP_03_2);
    reg0302 = FLD_SET_DRF( _PGC6, _AON_FRTS_INPUT_CTRL_SELWRE_SCRATCH_GROUP_03_2, _MEDIA_TYPE, _FB , reg0302);
    reg0302 = FLD_SET_DRF_NUM( _PGC6, _AON_FRTS_INPUT_CTRL_SELWRE_SCRATCH_GROUP_03_2, _WPR_ID, 0x1, reg0302);
    ACR_REG_WR32(BAR0, LW_PGC6_AON_FRTS_INPUT_CTRL_SELWRE_SCRATCH_GROUP_03_2, reg0302);

    reg0302 = ACR_REG_RD32(BAR0, LW_PGC6_AON_FRTS_OUTPUT_FLAG_SELWRE_SCRATCH_GROUP_03_2);
    reg0302 = FLD_SET_DRF( _PGC6, _AON_FRTS_OUTPUT_FLAG_SELWRE_SCRATCH_GROUP_03_2, _CMD_SUCCESSFUL, _YES, reg0302);
    reg0302 = FLD_SET_DRF( _PGC6, _AON_FRTS_OUTPUT_FLAG_SELWRE_SCRATCH_GROUP_03_2, _ACR_READY, _NO, reg0302);
    ACR_REG_WR32(BAR0, LW_PGC6_AON_FRTS_OUTPUT_FLAG_SELWRE_SCRATCH_GROUP_03_2, reg0302);

    return status;
}

#ifdef AHESASC
/*
 * @brief Function restricts the access of SEC2 falcon from complete WPR1 to
 * resized WPR1, since SEC2 falcon is the ACRlib hosting falcon.
 *
 * Resized WPR1: start of WPR1 - Ucode address of SEC2 RTOS.
 * RM has been modified to add SEC2 RTOS Ucode image at the end after all the
 * ucode images and before the shared subwpr.
 */
ACR_STATUS
acrResizeAcrlibSubwpr_TU10X(void)
{
    ACR_STATUS status    = ACR_OK;
    LwU32 falconId       = LSF_FALCON_ID_SEC2;
    LwU32 wprStartAddr4K = ILWALID_WPR_ADDR_LO;
    LwU32 wprEndAddr4K   = ILWALID_WPR_ADDR_HI;
    LwU32 flcnSubWprId;
    LwU32 acrlibFalconCodeStartAddress;
    ACR_FLCN_CONFIG flcnCfg;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetFalconConfig_HAL(pAcrlib, falconId, LSF_FALCON_INSTANCE_DEFAULT_0, &flcnCfg));

    //
    // Find reduced WPR end address.
    // Get the address from the register directly, programmed during subwpr access programming.
    //
    flcnSubWprId = SEC2_SUB_WPR_ID_3_UCODE_CODE_SECTION_WPR1;
    acrlibFalconCodeStartAddress = ACR_REG_RD32(BAR0, (flcnCfg.subWprRangeAddrBase + (FALCON_SUB_WPR_INDEX_REGISTER_STRIDE * flcnSubWprId)));

    acrlibFalconCodeStartAddress = DRF_VAL(_PFB, _PRI_MMU_FALCON_SEC_CFGA, _ADDR_LO, acrlibFalconCodeStartAddress);
    wprEndAddr4K = (acrlibFalconCodeStartAddress - 1);

    // Find  reduced WPR start address
    flcnSubWprId = SEC2_SUB_WPR_ID_0_ACRLIB_FULL_WPR1;
    wprStartAddr4K = ACR_REG_RD32(BAR0, (flcnCfg.subWprRangeAddrBase + (FALCON_SUB_WPR_INDEX_REGISTER_STRIDE * flcnSubWprId)));

    wprStartAddr4K = DRF_VAL(_PFB, _PRI_MMU_FALCON_SEC_CFGA, _ADDR_LO, wprStartAddr4K);

    // Program SEC2 falocn with new reduced WPR1 blob access.
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, falconId, SEC2_SUB_WPR_ID_0_ACRLIB_FULL_WPR1,
                               wprStartAddr4K, wprEndAddr4K, ACR_SUB_WPR_MMU_RMASK_L3, ACR_SUB_WPR_MMU_WMASK_ALL_LEVELS_DISABLED , LW_TRUE));

    return status;
}

/*!
 * @brief ACR initialization routine
 */
ACR_STATUS acrInit_TU10X(void)
{
    ACR_STATUS   status     = ACR_OK;
    LwU32        wprIndex   = 0xFFFFFFFF;

    //
    // Setup HUB Encryption for ACR regions
    // TODO: Enable once Bug 1718683 is fixed
    //
    if ((status = acrProgramHubEncryption_HAL(pAcr)) != ACR_OK)
    {
        goto Cleanup;
    }

    // Program Decode traps for BUG 2823165
    if ((status = acrProgramDecodeTrapToIsolateTSTGRegisterToFECSWARBug2823165_HAL(pAcr)) != ACR_OK)
    {
        goto Cleanup;
    }

    // Set PLM of Timer registers, no access to L0/1/2
    if ((status = acrProtectHostTimerRegisters_HAL(pAcr, LW_TRUE)) != ACR_OK)
    {
        goto Cleanup;
    }

    // Get the falcon debug status
    g_bIsDebug = acrIsDebugModeEnabled_HAL(pAcr);

    if ((status = acrLockAcrRegions_HAL(pAcr, &wprIndex)) != ACR_OK)
    {
        goto Cleanup;
    }

    //
    // Bootstrap the falcon only if it has valid WPR region.
    // A valid region should have a index less than the maximum supported regions.
    //
    if (wprIndex >= LW_MMU_WRITE_PROTECTION_REGIONS)
    {
        status = ACR_ERROR_REGION_PROP_ENTRY_NOT_FOUND;
        goto Cleanup;
    }

    if ((status = acrPopulateDMAParameters_HAL(pAcr, wprIndex)) != ACR_OK)
    {
        goto Cleanup;
    }

    if((status = _acrShadowCopyOfWpr(wprIndex)) != ACR_OK)
    {
        goto Cleanup;
    }

    // Setup supported shared subWprs in MMU
    if ((status = acrSetupSharedSubWprs_HAL(pAcr)) != ACR_OK)
    {
        goto Cleanup;
    }

#ifdef AMPERE_PROTECTED_MEMORY
    // APM can only run on APM/debug boards
    if ((status = acrCheckIfApmEnabled_HAL()) != ACR_OK)
    {
        goto Cleanup;
    }

    // Initialize APM RTS
    if ((status = acrMsrResetAll()) != ACR_OK)
    {
        goto Cleanup;
    }

    // Measure the fuses and MMU state
    if ((status = acrMeasureStaticState_HAL(pAcr)) != ACR_OK)
    {
        goto Cleanup;
    }
#endif // AMPERE_PROTECTED_MEMORY

    // Copy FRTS data from WPR2 to WPR1
    if ((status = acrCopyFrtsData_HAL(pAcr)) != ACR_OK)
    {
        goto Cleanup;
    }

#ifdef NEW_WPR_BLOBS
    if ((status = acrValidateSignatureAndScrubUnusedWprExt_HAL(pAcr)) != ACR_OK)
    {
        goto Cleanup;
    }
#else
    if ((status = acrValidateSignatureAndScrubUnusedWpr_HAL(pAcr)) != ACR_OK)
    {
        goto Cleanup;
    }
#endif

#ifdef NEW_WPR_BLOBS
    // Decrypt the LS Ucodes
    if ((status = acrDecryptAndWriteToWpr_HAL(pAcr)) != ACR_OK)
    {
        goto Cleanup;
    }
#endif

    if ((status = acrResizeAcrlibSubwpr_HAL()) != ACR_OK)
    {
        goto Cleanup;
    }

    if ((status = acrDisableMemoryLockRangeRowRemapWARBug2968134_HAL()) != ACR_OK)
    {
        goto Cleanup;
    }

Cleanup:
    return status;
}

/*!
 * @brief  Copies ucodes from non-WPR region to WPR region using non-WPR location passed from host
 *
 * @return  ACR_OK                       If copy is successful
 * @return  ACR_ERROR_FLCN_ID_NOT_FOUND  If the falcon config failed
 * @return  ACR_ERROR_DMA_FAILURE        If copy failed
 */
static
ACR_STATUS _acrShadowCopyOfWpr(LwU32 wprIndex)
{
    LwU32             totalCopySizeInBytes;
    LwU32             bytesCopied   = 0;
    LwU32            *pBuffer1      = g_copyBufferA;
    LwU32            *pBuffer2      = g_copyBufferB;
    LwU32            *pTemp;
    ACR_DMA_PROP      dmaPropRead;
    ACR_DMA_PROP      dmaPropWrite;
    ACR_FLCN_CONFIG   flcnCfg;
    ACR_STATUS        status;

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

    dmaPropRead.wprBase  = LwU64_LO32(g_desc.regions.regionProps[wprIndex].shadowMemStartAddress);
    dmaPropRead.regionID = 0; // This region is non-WPR

    //
    // Set up the read ctxDma to be a different aperture from the one used to
    // write into WPR. It has to be different since the code doing the shadow
    // copy below will ping pong DMA reads and writes between these two
    // apertures, and it is never safe to change aperture REGIONCFG settings
    // while DMA requests are still in the pipeline.
    //
    ct_assert(LSF_BOOTSTRAP_CTX_DMA_BOOTSTRAP_OWNER != RM_SEC2_DMAIDX_PHYS_VID_FN0);
    dmaPropRead.ctxDma   = RM_SEC2_DMAIDX_PHYS_VID_FN0;

    status = acrlibGetFalconConfig_HAL(pAcrlib, ACR_LSF_LWRRENT_BOOTSTRAP_OWNER,
                                       LSF_FALCON_INSTANCE_DEFAULT_0, &flcnCfg);
    if (status != ACR_OK)
    {
        return status;
    }

    status = acrlibSetupCtxDma_HAL(pAcrlib, &flcnCfg, dmaPropRead.ctxDma, LW_TRUE);
    if (status != ACR_OK)
    {
        return status;
    }

    dmaPropWrite.wprBase  = LwU64_LO32(g_dmaProp.wprBase);
    dmaPropWrite.ctxDma   = g_dmaProp.ctxDma;
    dmaPropWrite.regionID = g_dmaProp.regionID;

    if (acrIssueDma_HAL(pAcr, pBuffer1, LW_FALSE, 0,
        DMA_SIZE, ACR_DMA_FROM_FB, ACR_DMA_SYNC_AT_END, &dmaPropRead) != DMA_SIZE)
    {
        return ACR_ERROR_DMA_FAILURE;
    }

    while (bytesCopied < (totalCopySizeInBytes - DMA_SIZE))
    {
        if (acrIssueDma_HAL(pAcr, pBuffer1, LW_FALSE, 0,
            DMA_SIZE, ACR_DMA_TO_FB, ACR_DMA_SYNC_NO, &dmaPropWrite) != DMA_SIZE)
        {
            return ACR_ERROR_DMA_FAILURE;
        }

        bytesCopied += DMA_SIZE;
        dmaPropRead.wprBase  = dmaPropRead.wprBase + (DMA_SIZE >> 8);
        dmaPropWrite.wprBase  = dmaPropWrite.wprBase + (DMA_SIZE >> 8);

        if (acrIssueDma_HAL(pAcr, pBuffer2, LW_FALSE, 0,
            DMA_SIZE, ACR_DMA_FROM_FB, ACR_DMA_SYNC_NO, &dmaPropRead) != DMA_SIZE)
        {
            return ACR_ERROR_DMA_FAILURE;
        }

        falc_dmwait();

        pTemp = pBuffer1;
        pBuffer1 = pBuffer2;
        pBuffer2 = pTemp;
    }

    if (acrIssueDma_HAL(pAcr, pBuffer1, LW_FALSE, 0,
        DMA_SIZE, ACR_DMA_TO_FB, ACR_DMA_SYNC_AT_END, &dmaPropWrite) != DMA_SIZE)
    {
        return ACR_ERROR_DMA_FAILURE;
    }

    return ACR_OK;
}
#endif


/*!
 * @brief  Reads the ACR details by parsing the ACR_DESC header and programs
 *         MMU registers to lock the regions.
 *
 * @param[out] pWprIndex  Index into g_desc region properties holding wpr region
 *
 * @return     ACR_OK     If programming MMU registers are successful
 */
ACR_STATUS
acrLockAcrRegions_TU10X
(
    LwU32 *pWprIndex
)
{
    ACR_STATUS status = ACR_OK;

    //
    // Disable all falcon subWprs because HW init value of subWpr is not disabled on Turing
    // Make sure that subWprs are not programmed before this function call
    //
    if (ACR_OK != (status = acrDisableAllFalconSubWprs_HAL(pAcr)))
    {
        return status;
    }

#ifdef BSI_LOCK
    // Restore VPR range
    if (ACR_OK != (status = acrRestoreVPROnResumeFromGC6_HAL()))
    {
        return status;
    }

    // Restore WPR regions
    return acrRestoreWPRsOnResumeFromGC6_HAL(pAcr, pWprIndex);

#else   // if not BSI_LOCK

    return acrLockWprRegionsDuringACRLoad_HAL(pAcr, pWprIndex);

#endif //BSI_LOCK
}

/*
 * Lock WPR1 region in MMU during ACR_LOAD
 */
ACR_STATUS
acrLockWpr1DuringACRLoad_TU10X
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

    // Avoid the range check on fmodel since VBIOS can't provide the usable FB size on fmodel. See lwbugs/200652482
#ifndef ACR_FMODEL_BUILD
    // pRegionProp has addresses in 256B aligned form from RM, and that acrCheckWprRangeWithRowRemapperReserveFB_HAL function accepts addresses in bytes, that's why left shifted by 8
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrCheckWprRangeWithRowRemapperReserveFB_HAL(pAcr, (((LwU64)pRegionProp->startAddress) << 8), ((((LwU64)pRegionProp->endAddress) << 8) -1)));
#endif // ACR_FMODEL_BUILD

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

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrSaveWprInfoToBSISelwreScratch_HAL());

    //
    // Program subWpr for ACRlib running falcon which required full WPR access
    // This complete WPR1 access will be changed to reduced WPR1 access at the
    // end of AHESASC binary.
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_SEC2, SEC2_SUB_WPR_ID_0_ACRLIB_FULL_WPR1,
                                        wprStartAddr4K, wprEndAddr4K, ACR_SUB_WPR_MMU_RMASK_L3, ACR_SUB_WPR_MMU_WMASK_L3, LW_TRUE));

    //
    // ASB runs on GSP and needs read only access to WPR1 to load SEC2 RTOS.
    // ASB self clears its access to SEC2 RTOS at the end of ASB binary.
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_GSPLITE, GSP_SUB_WPR_ID_2_ASB_FULL_WPR1,
                                        wprStartAddr4K, wprEndAddr4K, ACR_SUB_WPR_MMU_RMASK_L3, ACR_SUB_WPR_MMU_WMASK_ALL_LEVELS_DISABLED, LW_TRUE));

    return status;
}

/*
 * Lock WPR regions during ACR_LOAD
 */
ACR_STATUS
acrLockWprRegionsDuringACRLoad_TU10X
(
    LwU32 *pWprIndex
)
{
    ACR_STATUS status = ACR_OK;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLockWpr1DuringACRLoad_HAL(pAcr, pWprIndex));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLockWpr2DuringACRLoad_HAL(pAcr, pWprIndex));

    return status;
}

/*
 * Lock WPR2 region in MMU during ACR_LOAD
 */
ACR_STATUS
acrLockWpr2DuringACRLoad_TU10X
(
    LwU32 *pWprIndex
)
{
#ifdef ACR_FMODEL_BUILD
    return acrLockWpr2DuringACRLoadForFmodel_HAL(pAcr, pWprIndex);
#else
    // We don't support WPR2 for non-fmodel cases
    return ACR_OK;
#endif
}


#ifdef ACR_FMODEL_BUILD
/*
 * Lock WPR2 region in MMU during ACR_LOAD, supported for fmodel only
 * WARINING : Please do not refer/reuse this implementation. This function is not
 *            HS production quality as it has not been reviewed and it does not have sanity checks
 */
ACR_STATUS
acrLockWpr2DuringACRLoadForFmodel_TU10X
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

    pRegionProp = &(g_desc.regions.regionProps[ACR_WPR2_REGION_IDX]);

    if (pRegionProp->regionID > g_desc.regions.noOfRegions)
    {
        return ACR_ERROR_ILWALID_REGION_ID;
    }

    // If both the address are same, please ignore this region and continue
    if (pRegionProp->startAddress == pRegionProp->endAddress)
    {
        // return safely, WPR2 setup is not required
        return ACR_OK;
    }

    //
    // Program the MMU registers with start/end address
    // WPR MMU register accepts address in 4K. In RM we already make startAddress and endAddress aligned to 256.
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
    data = FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR_ALLOW_READ, _WPR2, pRegionProp->readMask, data);
    data = FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR_ALLOW_READ, _MIS_MATCH, LSF_WPR_REGION_ALLOW_READ_MISMATCH_NO, data);
    ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_READ, data);

    data = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_WRITE);
    data = FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR_ALLOW_WRITE, _WPR2, pRegionProp->writeMask, data);
    data = FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR_ALLOW_WRITE, _MIS_MATCH, LSF_WPR_REGION_ALLOW_WRITE_MISMATCH_NO, data);
    ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_WRITE, data);

    return status;
}
#endif

