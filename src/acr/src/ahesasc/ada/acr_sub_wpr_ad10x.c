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
 * @file: acr_sub_wpr_ad10x.c
 */

//
// Includes
//
#include "acr.h"

#include "acr.h"
#include "acrdrfmacros.h"
#include "acr_objacr.h"
#include "acr_objacrlib.h"
#include "mmu/mmucmn.h"
#include "dev_fb.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"

#include "config/g_acr_private.h"
#include "config/g_acrlib_private.h"

// Extern Variables
extern ACR_DMA_PROP     g_dmaProp;

/*
 * @brief Setup all supported shared falcon subWprs with new WPR blob defines
 */

#ifdef NEW_WPR_BLOBS


/*
 * @brief Setup SubWpr for code and data part of falcon's ucode in FB-WPR with new wrp defines
 */
ACR_STATUS
acrSetupFalconCodeAndDataSubWprsExt_AD10X
(
    LwU32 falconId,
    PLSF_LSB_HEADER_WRAPPER pLsfHeaderWrapper
)
{
    ACR_STATUS      status               = ACR_OK;
    LwU64           codeStartAddr        = 0;
    LwU64           dataStartAddr        = 0;
    LwU64           codeEndAddr          = 0;
    LwU64           dataEndAddr          = 0;
    LwU32           codeSubWprId         = 0;
    LwU32           dataSubWprId         = 0;
    LwU32           ucodeOffset          = 0;
    LwU32           ucodeSize            = 0;
    LwU32           dataSize             = 0;
    LwU32           blDataSize           = 0;
    LwBool          bSaveSubWprToScratch = LW_TRUE;
    LwU32           codeReadPlmMask      = ACR_SUB_WPR_MMU_RMASK_L2_AND_L3;
    LwU32           codeWritePlmMask     = ACR_SUB_WPR_MMU_WMASK_ALL_LEVELS_DISABLED;
    LwU32           dataReadPlmMask      = ACR_SUB_WPR_MMU_RMASK_L2_AND_L3;
    LwU32           dataWritePlmMask     = ACR_SUB_WPR_MMU_WMASK_L2_AND_L3;
    ACR_FLCN_CONFIG flcnCfg;
    LwU32           falconInstance       = LSF_FALCON_INSTANCE_DEFAULT_0;

    //
    // We are not programming GPCCS data & code subWpr's since GPCCS is priv-loaded and
    // niether does it use demand paging, thus it does not require access to its subWpr
    //
    if (falconId == LSF_FALCON_ID_GPCCS)
    {
        return ACR_OK;
    }

    // LWDEC1 starts with default instance 1, instance 0 is meant for LWDEC0.
    if (falconId == LSF_FALCON_ID_LWDEC1)
    {
        falconInstance = LSF_FALCON_INSTANCE_COUNT_DEFAULT_1;
    }

    if (pLsfHeaderWrapper == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

#ifdef ACR_RISCV_LS
    // Get falcon config to determine architecture (Falcon/RISC-V).
    LwU32           appCodeOffset;
    LwU32           appCodeSize;
    LwU32           appDataOffset;
    LwU32           appDataSize;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetFalconConfig_HAL(pAcrlib, falconId, falconInstance, &flcnCfg));
    //
    // RISC-V cores use the re-purposed app*Size fields to specify mWPRs.
    // See the code sample below for details:
    // https://confluence.lwpu.com/pages/viewpage.action?pageId=189948067#RISC-VLSBoot(GA100)-LSF_LSB_HEADER
    //
    if (flcnCfg.uprocType == ACR_TARGET_ENGINE_CORE_RISCV ||
        flcnCfg.uprocType == ACR_TARGET_ENGINE_CORE_RISCV_EB)
    {
        //
        // RISC-V mWPR layout:
        // 0 - Golden image (read-only).
        // 1 - Runtime image (read-write).
        //
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_APP_CODE_OFFSET,
                                                                            pLsfHeaderWrapper, (void *)&appCodeOffset));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_APP_CODE_SIZE,
                                                                            pLsfHeaderWrapper, (void *)&appCodeSize));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_APP_DATA_OFFSET,
                                                                            pLsfHeaderWrapper, (void *)&appDataOffset));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_APP_DATA_SIZE,
                                                                            pLsfHeaderWrapper, (void *)&appDataSize));

        codeStartAddr = (g_dmaProp.wprBase << 8) + appCodeOffset;
        codeEndAddr = (codeStartAddr + appCodeSize - 1);

        dataStartAddr = (g_dmaProp.wprBase << 8) + appDataOffset;
        dataEndAddr = dataStartAddr + appDataSize - 1;
    }

    else
#endif
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_UCODE_OFFSET,
                                                                            pLsfHeaderWrapper, (void *)&ucodeOffset));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_UCODE_SIZE,
                                                                            pLsfHeaderWrapper, (void *)&ucodeSize));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_DATA_SIZE,
                                                                            pLsfHeaderWrapper, (void *)&dataSize));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_BL_DATA_SIZE,
                                                                            pLsfHeaderWrapper, (void *)&blDataSize));

        codeStartAddr = (g_dmaProp.wprBase << 8) + ucodeOffset;
        codeEndAddr   = (codeStartAddr + ucodeSize - 1);

        dataStartAddr = codeStartAddr + ucodeSize;
        dataEndAddr   = dataStartAddr + dataSize + blDataSize - 1;

        LwBool     bHsOvlSigBlobPresent = LW_FALSE;

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_HS_OVL_SIG_BLOB_PRESENT,
                                                                            pLsfHeaderWrapper, (void *)&bHsOvlSigBlobPresent));

        // If RM Runtime Patching of HS OVL Sig Blob is present we add hsOvlSigBlobSize to accomodate HS Ovl Sig Blob during subWPR creation
        if (bHsOvlSigBlobPresent)
        {
            LwU32      blDataOffset         = 0;
            LwU32      hsOvlSigBlobOffset   = 0;
            LwU32      hsOvlSigBlobSize     = 0;

            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_BL_DATA_OFFSET,
                                                                                pLsfHeaderWrapper, (void *)&blDataOffset));
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_HS_OVL_SIG_BLOB_SIZE,
                                                                                pLsfHeaderWrapper, (void *)&hsOvlSigBlobSize));
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_HS_OVL_SIG_BLOB_OFFSET,
                                                                                pLsfHeaderWrapper, (void *)&hsOvlSigBlobOffset));

            dataEndAddr += (hsOvlSigBlobOffset - (blDataOffset + blDataSize) + 1) + hsOvlSigBlobSize;
        }
    }

    // Sanitize above callwlation for size overflow
    if ((codeStartAddr < g_dmaProp.wprBase) || (codeEndAddr < codeStartAddr) ||
        (dataStartAddr < codeStartAddr) || (dataEndAddr < dataStartAddr))
    {
        return ACR_ERROR_VARIABLE_SIZE_OVERFLOW;
    }

    switch (falconId)
    {
        case LSF_FALCON_ID_PMU:
#ifdef ACR_RISCV_LS
        case LSF_FALCON_ID_PMU_RISCV:
#endif
            codeSubWprId = PMU_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1;
            dataSubWprId = PMU_SUB_WPR_ID_2_UCODE_DATA_SECTION_WPR1;
            break;
        case LSF_FALCON_ID_FECS:
            codeSubWprId = FECS_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1;
            dataSubWprId = FECS_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1;
            break;
        case LSF_FALCON_ID_LWDEC:
            codeSubWprId = LWDEC0_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1;
            dataSubWprId = LWDEC0_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1;
            break;
        case LSF_FALCON_ID_FBFALCON:
            codeSubWprId = FBFALCON_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1;
            dataSubWprId = FBFALCON_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1;
            break;
        case LSF_FALCON_ID_GSPLITE:
#ifdef ACR_RISCV_LS
        case LSF_FALCON_ID_GSP_RISCV:
#endif
            codeSubWprId = GSP_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1;
            dataSubWprId = GSP_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1;
            break;
        case LSF_FALCON_ID_SEC2:
            codeSubWprId = SEC2_SUB_WPR_ID_3_UCODE_CODE_SECTION_WPR1;
            dataSubWprId = SEC2_SUB_WPR_ID_4_UCODE_DATA_SECTION_WPR1;
            break;

#ifdef LWDEC1_PRESENT
        case LSF_FALCON_ID_LWDEC1:
            codeSubWprId         = LWDEC1_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1;
            dataSubWprId         = LWDEC1_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1;
            codeReadPlmMask      = ACR_SUB_WPR_MMU_RMASK_ALL_LEVELS_ENABLED;
            dataReadPlmMask      = ACR_SUB_WPR_MMU_RMASK_ALL_LEVELS_ENABLED;
            dataWritePlmMask     = ACR_SUB_WPR_MMU_WMASK_ALL_LEVELS_ENABLED;
            bSaveSubWprToScratch = LW_FALSE;
            break;
#endif

#ifdef LWDEC_RISCV_PRESENT
        case LSF_FALCON_ID_LWDEC_RISCV:
            codeSubWprId = LWDEC_RISCV_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1;
            dataSubWprId = LWDEC_RISCV_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1;
            codeReadPlmMask      = ACR_SUB_WPR_MMU_RMASK_ALL_LEVELS_ENABLED;
            dataReadPlmMask      = ACR_SUB_WPR_MMU_RMASK_ALL_LEVELS_ENABLED;
            dataWritePlmMask     = ACR_SUB_WPR_MMU_WMASK_ALL_LEVELS_ENABLED;
        break;
#endif

#ifdef LWDEC_RISCV_EB_PRESENT
        case LSF_FALCON_ID_LWDEC_RISCV_EB:
            codeSubWprId         = LWDEC_RISCV_EB_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1;
            dataSubWprId         = LWDEC_RISCV_EB_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1;
            codeReadPlmMask      = ACR_SUB_WPR_MMU_RMASK_ALL_LEVELS_ENABLED;
            dataReadPlmMask      = ACR_SUB_WPR_MMU_RMASK_ALL_LEVELS_ENABLED;
            dataWritePlmMask     = ACR_SUB_WPR_MMU_WMASK_ALL_LEVELS_ENABLED;
            bSaveSubWprToScratch = LW_FALSE;
            break;
#endif

#ifdef LWJPG_PRESENT
        case LSF_FALCON_ID_LWJPG:
            codeSubWprId         = LWJPG_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1;
            dataSubWprId         = LWJPG_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1;
            codeReadPlmMask      = ACR_SUB_WPR_MMU_RMASK_ALL_LEVELS_ENABLED;
            dataReadPlmMask      = ACR_SUB_WPR_MMU_RMASK_ALL_LEVELS_ENABLED;
            dataWritePlmMask     = ACR_SUB_WPR_MMU_WMASK_ALL_LEVELS_ENABLED;
            bSaveSubWprToScratch = LW_FALSE;
            break;
#endif

#ifdef LWENC_PRESENT
        case LSF_FALCON_ID_LWENC:
            codeSubWprId         = LWENC0_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1;
            dataSubWprId         = LWENC0_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1;
            codeReadPlmMask      = ACR_SUB_WPR_MMU_RMASK_ALL_LEVELS_ENABLED;
            dataReadPlmMask      = ACR_SUB_WPR_MMU_RMASK_ALL_LEVELS_ENABLED;
            dataWritePlmMask     = ACR_SUB_WPR_MMU_WMASK_ALL_LEVELS_ENABLED;
            bSaveSubWprToScratch = LW_FALSE;
            break;
#endif

#ifdef OFA_PRESENT
        case LSF_FALCON_ID_OFA:
            codeSubWprId         = OFA0_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1;
            dataSubWprId         = OFA0_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1;
            codeReadPlmMask      = ACR_SUB_WPR_MMU_RMASK_ALL_LEVELS_ENABLED;
            dataReadPlmMask      = ACR_SUB_WPR_MMU_RMASK_ALL_LEVELS_ENABLED;
            dataWritePlmMask     = ACR_SUB_WPR_MMU_WMASK_ALL_LEVELS_ENABLED;
            bSaveSubWprToScratch = LW_FALSE;
            break;
#endif
        default:
            return ACR_ERROR_FLCN_ID_NOT_FOUND;
    }

#if defined (LWJPG_PRESENT) || defined (LWENC_PRESENT) || defined (LWDEC1_PRESENT)
    LwU32   instanceCount        = LSF_FALCON_INSTANCE_COUNT_DEFAULT_1;
    LwU32   instanceStartIndex   = 0;
    LwBool  bInstanceEnabled;
    LwU32   i;

    // LWDEC1 starts with default instance 1, instance 0 is meant for LWDEC0.
    if (falconId == LSF_FALCON_ID_LWDEC1)
    {
        instanceStartIndex = LSF_FALCON_INSTANCE_COUNT_DEFAULT_1;
    }

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetFalconInstanceCount_HAL(pAcrlib, falconId, &instanceCount));

    for (i = instanceStartIndex; i < instanceCount; i++)
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetFalconInstanceStatus_HAL(pAcrlib, falconId, i, &bInstanceEnabled));

        if (bInstanceEnabled)      
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetFalconConfig_HAL(pAcrlib, falconId, i, &flcnCfg));
		    
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWprExt_HAL(pAcr, &flcnCfg, codeSubWprId, (codeStartAddr >> SHIFT_4KB),
                (codeEndAddr >> SHIFT_4KB), codeReadPlmMask, codeWritePlmMask, bSaveSubWprToScratch));
		    
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWprExt_HAL(pAcr, &flcnCfg, dataSubWprId, (dataStartAddr >> SHIFT_4KB),
                (dataEndAddr >> SHIFT_4KB), dataReadPlmMask, dataWritePlmMask, bSaveSubWprToScratch));
        }
    }
#else
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetFalconConfig_HAL(pAcrlib, falconId, LSF_FALCON_INSTANCE_DEFAULT_0, &flcnCfg));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWprExt_HAL(pAcr, &flcnCfg, codeSubWprId, (codeStartAddr >> SHIFT_4KB),
        (codeEndAddr >> SHIFT_4KB), codeReadPlmMask, codeWritePlmMask, bSaveSubWprToScratch));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWprExt_HAL(pAcr, &flcnCfg, dataSubWprId, (dataStartAddr >> SHIFT_4KB),
        (dataEndAddr >> SHIFT_4KB), dataReadPlmMask, dataWritePlmMask, bSaveSubWprToScratch));

#endif

    return status;
}

/*!
 * Disable all falcon subWprs because HW init value of subWpr is not disabled on Turing, Bug 200444247 has details
 * Note that this function should be called before programming any valid(known use case) subWpr
 */
ACR_STATUS
acrProgramFalconSubWprExt_AD10X
(
   PACR_FLCN_CONFIG pFlcnCfg,
   LwU8   flcnSubWprId,
   LwU32  startAddr,
   LwU32  endAddr,
   LwU8   readMask,
   LwU8   writeMask,
   LwBool bSave
)
{
    ACR_STATUS      status  = ACR_OK;
    LwU32           regCfga = 0;
    LwU32           regCfgb = 0;
    LwU32           regPlm  = 0;
    LwU32           scratchCfga = ILWALID_REG_ADDR;
    LwU32           scratchCfgb = ILWALID_REG_ADDR;
    LwU32           scratchPlm  = ILWALID_REG_ADDR;
    LwU32           falconId    = pFlcnCfg->falconId;

    // Get secure scratch allocation for save-restore of subWPR
    if (bSave)
    {
        // TODO: Need to support multi instance Falcon ?
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetSelwreScratchAllocationForSubWpr_HAL(pAcr, falconId, flcnSubWprId, &scratchCfga, &scratchCfgb, &scratchPlm));
    }

    //
    // Update subWpr in MMU
    // SubWpr registers CFGA and CFGB for single WPR are of 4 bytes and conselwtive
    // Registers for next subWpr of same falcon are 8 bytes separated (4 bytes of CFGA + 4 bytes of CFGB)
    //
    regCfga = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_FALCON_PMU_CFGA, _ALLOW_READ, readMask, regCfga);
    regCfga = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_FALCON_PMU_CFGA, _ADDR_LO, startAddr, regCfga);
    ACR_REG_WR32(BAR0, (pFlcnCfg->subWprRangeAddrBase + (FALCON_SUB_WPR_INDEX_REGISTER_STRIDE * flcnSubWprId)), regCfga);

    regCfgb = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_FALCON_PMU_CFGB, _ALLOW_WRITE, writeMask, regCfgb);
    regCfgb = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_FALCON_PMU_CFGB, _ADDR_HI, endAddr, regCfgb);
    ACR_REG_WR32(BAR0, (pFlcnCfg->subWprRangeAddrBase + (FALCON_SUB_WPR_INDEX_REGISTER_STRIDE * flcnSubWprId)
                                                      + FALCON_SUB_WPR_CONFIG_REGISTER_OFFSET_DIFF), regCfgb);

    // Update SubWpr settings in secure scratch
    if (bSave)
    {
        ACR_REG_WR32(BAR0, scratchCfga, regCfga);
        ACR_REG_WR32(BAR0, scratchCfgb, regCfgb);

        // The secure scratch PLM is read protected by default, so need to allow read
        regPlm = ACR_REG_RD32(BAR0, scratchPlm);
        regPlm = FLD_SET_DRF( _PGC6, _AON_SELWRE_SCRATCH_GROUP_00_PRIV_LEVEL_MASK, _READ_PROTECTION, _ALL_LEVELS_ENABLED, regPlm);
        ACR_REG_WR32(BAR0, scratchPlm, regPlm);
    }

    return ACR_OK;
}
#endif //NEW_WPR_BLOBS
