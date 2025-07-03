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
 * @file: acr_sub_wpr_gh100.c
 */

//
// Includes
//
#include "acr.h"
#include "acrdrfmacros.h"
#include "acr_objacr.h"
#include "mmu/mmucmn.h"
#include "dev_fb.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"

#include "config/g_acr_private.h"

// Extern Variables
extern ACR_DMA_PROP     g_dmaProp;
extern RM_FLCN_ACR_DESC g_desc;

/*
 * @brief Read SubWpr Header from FB with new wpr blob defines
 */
ACR_STATUS
acrReadAllSubWprHeaderWrappers_GH100
(
    LwU8 *pSubWprHeaderWrapper
)
{
    LwU32      size   = LSF_SUB_WPR_HEADERS_WRAPPER_TOTAL_SIZE_MAX;
    ACR_STATUS status = ACR_OK;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrIssueDma_HAL(pAcr, pSubWprHeaderWrapper, LW_FALSE, LSF_WPR_HEADERS_WRAPPER_TOTAL_SIZE_MAX, size,
            ACR_DMA_FROM_FB, ACR_DMA_SYNC_AT_END, &g_dmaProp));

    return status;
}

/*
 * @brief Setup SubWpr for code and data part of falcon's ucode in FB-WPR with new wrp defines
 */
ACR_STATUS
acrSetupFalconCodeAndDataSubWprsExt_GH100
(
    LwU32 falconId,
    PLSF_LSB_HEADER_WRAPPER pLsfHeaderWrapper
)
{
    ACR_STATUS status               = ACR_OK;
    LwU64      codeStartAddr        = 0;
    LwU64      dataStartAddr        = 0;
    LwU64      codeEndAddr          = 0;
    LwU64      dataEndAddr          = 0;
    LwU8       codeSubWprId         = 0;
    LwU8       dataSubWprId         = 0;
    LwU32      ucodeOffset          = 0;
    LwU32      ucodeSize            = 0;
    LwU32      dataSize             = 0;
    LwU32      blDataSize           = 0;
    LwU8       codeReadPlmMask      = ACR_SUB_WPR_MMU_RMASK_L2_AND_L3;
    LwU8       codeWritePlmMask     = ACR_SUB_WPR_MMU_WMASK_ALL_LEVELS_DISABLED;
    LwU8       dataReadPlmMask      = ACR_SUB_WPR_MMU_RMASK_L2_AND_L3;
    LwU8       dataWritePlmMask     = ACR_SUB_WPR_MMU_WMASK_L2_AND_L3;
    ACR_FLCN_CONFIG flcnCfg;

    //
    // We are not programming GPCCS data & code subWpr's since GPCCS is priv-loaded and
    // niether does it use demand paging, thus it does not require access to its subWpr
    //
    if (falconId == LSF_FALCON_ID_GPCCS)
    {
        return ACR_OK;
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

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetFalconConfig_HAL(pAcr, falconId, LSF_FALCON_INSTANCE_DEFAULT_0, &flcnCfg));

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
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_APP_CODE_OFFSET,
                                                                            pLsfHeaderWrapper, (void *)&appCodeOffset));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_APP_CODE_SIZE,
                                                                            pLsfHeaderWrapper, (void *)&appCodeSize));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_APP_DATA_OFFSET,
                                                                            pLsfHeaderWrapper, (void *)&appDataOffset));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_APP_DATA_SIZE,
                                                                            pLsfHeaderWrapper, (void *)&appDataSize));

        // The following sanity check macros are added to fix coverity / CERT - C warnings - Unsigned integer operation may wrap
        CHECK_UNSIGNED_INTEGER_ADDITION_AND_RETURN_IF_OUT_OF_BOUNDS(appCodeOffset, (g_dmaProp.wprBase << 8), LW_U64_MAX);
        
        codeStartAddr = (g_dmaProp.wprBase << 8) + appCodeOffset;

        CHECK_UNSIGNED_INTEGER_ADDITION_AND_RETURN_IF_OUT_OF_BOUNDS(appCodeSize, codeStartAddr, LW_U64_MAX);

        if ((codeStartAddr == 0) && (appCodeSize == 0))
        {
            return ACR_ERROR_ILWALID_ARGUMENT;
        }
        else
        {
            codeEndAddr = (codeStartAddr + appCodeSize - 1);
        }

        CHECK_UNSIGNED_INTEGER_ADDITION_AND_RETURN_IF_OUT_OF_BOUNDS(appDataOffset, (g_dmaProp.wprBase << 8), LW_U64_MAX);

        dataStartAddr = (g_dmaProp.wprBase << 8) + appDataOffset;

        CHECK_UNSIGNED_INTEGER_ADDITION_AND_RETURN_IF_OUT_OF_BOUNDS(appDataSize, dataStartAddr, LW_U64_MAX);

        if ((dataStartAddr == 0) && (appDataSize == 0))
        {
            return ACR_ERROR_ILWALID_ARGUMENT;
        }
        else
        {
            dataEndAddr = dataStartAddr + appDataSize - 1;
        }
    }

    else
#endif
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_UCODE_OFFSET,
                                                                            pLsfHeaderWrapper, (void *)&ucodeOffset));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_UCODE_SIZE,
                                                                            pLsfHeaderWrapper, (void *)&ucodeSize));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_DATA_SIZE,
                                                                            pLsfHeaderWrapper, (void *)&dataSize));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_BL_DATA_SIZE,
                                                                            pLsfHeaderWrapper, (void *)&blDataSize));

        //The following sanity checks are added to fix coverity / CERT - C warnings - Unsigned integer operation may wrap
        CHECK_UNSIGNED_INTEGER_ADDITION_AND_RETURN_IF_OUT_OF_BOUNDS(ucodeOffset, (g_dmaProp.wprBase << 8), LW_U64_MAX);

        codeStartAddr = (g_dmaProp.wprBase << 8) + ucodeOffset;

        CHECK_UNSIGNED_INTEGER_ADDITION_AND_RETURN_IF_OUT_OF_BOUNDS(ucodeSize, codeStartAddr, LW_U64_MAX);

        if ((codeStartAddr == 0) && (ucodeSize == 0))
        {
            return ACR_ERROR_ILWALID_ARGUMENT;
        }
        else
        {
            codeEndAddr   = (codeStartAddr + ucodeSize - 1);
        }

        CHECK_UNSIGNED_INTEGER_ADDITION_AND_RETURN_IF_OUT_OF_BOUNDS(codeStartAddr, ucodeSize, LW_U64_MAX);

        dataStartAddr = codeStartAddr + ucodeSize;

        if ((LW_U64_MAX - dataStartAddr) >= dataSize)
        {
            LwU64   temp = dataStartAddr + dataSize;

            if (((LW_U64_MAX - temp) >= blDataSize) && ((dataStartAddr > 0) || (dataSize > 0) || (blDataSize > 0)))
            {
                dataEndAddr   = dataStartAddr + dataSize + blDataSize - 1;
            }
            else
            {
                return ACR_ERROR_ILWALID_ARGUMENT;
            }
        }
        else
        {
            return ACR_ERROR_ILWALID_ARGUMENT;
        }

        LwBool     bHsOvlSigBlobPresent = LW_FALSE;

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_HS_OVL_SIG_BLOB_PRESENT,
                                                                            pLsfHeaderWrapper, (void *)&bHsOvlSigBlobPresent));

        // If RM Runtime Patching of HS OVL Sig Blob is present we add hsOvlSigBlobSize to accomodate HS Ovl Sig Blob during subWPR creation
        if (bHsOvlSigBlobPresent)
        {
            LwU32      blDataOffset         = 0;
            LwU32      hsOvlSigBlobOffset   = 0;
            LwU32      hsOvlSigBlobSize     = 0;

            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_BL_DATA_OFFSET,
                                                                                pLsfHeaderWrapper, (void *)&blDataOffset));
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_HS_OVL_SIG_BLOB_SIZE,
                                                                                pLsfHeaderWrapper, (void *)&hsOvlSigBlobSize));
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_HS_OVL_SIG_BLOB_OFFSET,
                                                                                pLsfHeaderWrapper, (void *)&hsOvlSigBlobOffset));

            dataEndAddr += (hsOvlSigBlobOffset - (blDataOffset + blDataSize) + 1) + hsOvlSigBlobSize;
        }
    }

    // Sanitize above callwlation for size overflow
    if ((codeStartAddr < (g_dmaProp.wprBase << 8)) || (codeEndAddr < codeStartAddr) ||
        (dataStartAddr < codeStartAddr) || (dataEndAddr < dataStartAddr))
    {
        return ACR_ERROR_VARIABLE_SIZE_OVERFLOW;
    }

    switch (falconId)
    {
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
#ifdef ACR_RISCV_LS
        case LSF_FALCON_ID_GSP_RISCV:
#endif
            codeSubWprId     = GSP_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR2;
            dataSubWprId     = GSP_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR2;
            codeReadPlmMask  = ACR_SUB_WPR_MMU_RMASK_ALL_LEVELS_ENABLED; 
            codeWritePlmMask = ACR_SUB_WPR_MMU_WMASK_ALL_LEVELS_DISABLED;
            dataReadPlmMask  = ACR_SUB_WPR_MMU_RMASK_ALL_LEVELS_ENABLED; 
            dataWritePlmMask = ACR_SUB_WPR_MMU_RMASK_ALL_LEVELS_ENABLED;
            break;
        case LSF_FALCON_ID_SEC2:
#ifdef ACR_RISCV_LS
        case LSF_FALCON_ID_SEC2_RISCV:
#endif
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
            break;
#endif

#ifdef LWDEC_RISCV_PRESENT
        case LSF_FALCON_ID_LWDEC_RISCV:
            codeSubWprId         = LWDEC_RISCV_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1;
            dataSubWprId         = LWDEC_RISCV_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1;
            codeReadPlmMask      = ACR_SUB_WPR_MMU_RMASK_L2_AND_L3;
            dataReadPlmMask      = ACR_SUB_WPR_MMU_RMASK_L2_AND_L3;
            dataWritePlmMask     = ACR_SUB_WPR_MMU_WMASK_L2_AND_L3;
        break;
#endif

#ifdef LWDEC_RISCV_EB_PRESENT
        case LSF_FALCON_ID_LWDEC_RISCV_EB:
            codeSubWprId         = LWDEC_RISCV_EB_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1;
            dataSubWprId         = LWDEC_RISCV_EB_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1;
            codeReadPlmMask      = ACR_SUB_WPR_MMU_RMASK_ALL_LEVELS_ENABLED;
            dataReadPlmMask      = ACR_SUB_WPR_MMU_RMASK_ALL_LEVELS_ENABLED;
            dataWritePlmMask     = ACR_SUB_WPR_MMU_WMASK_ALL_LEVELS_ENABLED;
            break;
#endif

#ifdef LWJPG_PRESENT
        case LSF_FALCON_ID_LWJPG:
            codeSubWprId         = LWJPG_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1;
            dataSubWprId         = LWJPG_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1;
            codeReadPlmMask      = ACR_SUB_WPR_MMU_RMASK_ALL_LEVELS_ENABLED;
            dataReadPlmMask      = ACR_SUB_WPR_MMU_RMASK_ALL_LEVELS_ENABLED;
            dataWritePlmMask     = ACR_SUB_WPR_MMU_WMASK_ALL_LEVELS_ENABLED;
            break;
#endif

#ifdef LWENC_PRESENT
        case LSF_FALCON_ID_LWENC:
            codeSubWprId         = LWENC0_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1;
            dataSubWprId         = LWENC0_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1;
            codeReadPlmMask      = ACR_SUB_WPR_MMU_RMASK_ALL_LEVELS_ENABLED;
            dataReadPlmMask      = ACR_SUB_WPR_MMU_RMASK_ALL_LEVELS_ENABLED;
            dataWritePlmMask     = ACR_SUB_WPR_MMU_WMASK_ALL_LEVELS_ENABLED;
            break;
#endif

#ifdef OFA_PRESENT
        case LSF_FALCON_ID_OFA:
            codeSubWprId         = OFA0_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1;
            dataSubWprId         = OFA0_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1;
            codeReadPlmMask      = ACR_SUB_WPR_MMU_RMASK_ALL_LEVELS_ENABLED;
            dataReadPlmMask      = ACR_SUB_WPR_MMU_RMASK_ALL_LEVELS_ENABLED;
            dataWritePlmMask     = ACR_SUB_WPR_MMU_WMASK_ALL_LEVELS_ENABLED;
            break;
#endif
        default:
            return ACR_ERROR_FLCN_ID_NOT_FOUND;
    }
#if defined(LWDEC_RISCV_EB_PRESENT) || defined (LWJPG_PRESENT)
    LwU32   instanceCount        = LSF_FALCON_INSTANCE_COUNT_DEFAULT_1;
    LwBool  bInstanceEnabled;
    LwU32   instance;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetFalconInstanceCount_HAL(pAcr, falconId, &instanceCount));

    for (instance = 0; instance < instanceCount; instance++)
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetFalconInstanceStatus_HAL(pAcr, falconId, instance, &bInstanceEnabled));

        if (bInstanceEnabled)
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetFalconConfig_HAL(pAcr, falconId, instance, &flcnCfg));

            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWprExt_HAL(pAcr, &flcnCfg, codeSubWprId, (LwU32)(codeStartAddr >> SHIFT_4KB),
                (LwU32)(codeEndAddr >> SHIFT_4KB), codeReadPlmMask, codeWritePlmMask));

            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWprExt_HAL(pAcr, &flcnCfg, dataSubWprId, (LwU32)(dataStartAddr >> SHIFT_4KB),
                (LwU32)(dataEndAddr >> SHIFT_4KB), dataReadPlmMask, dataWritePlmMask));
        }
    }
#else
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetFalconConfig_HAL(pAcr, falconId, LSF_FALCON_INSTANCE_DEFAULT_0, &flcnCfg));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWprExt_HAL(pAcr, &flcnCfg, codeSubWprId, (LwU32)(codeStartAddr >> SHIFT_4KB),
        (LwU32)(codeEndAddr >> SHIFT_4KB), codeReadPlmMask, codeWritePlmMask));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWprExt_HAL(pAcr, &flcnCfg, dataSubWprId, (LwU32)(dataStartAddr >> SHIFT_4KB),
        (LwU32)(dataEndAddr >> SHIFT_4KB), dataReadPlmMask, dataWritePlmMask));

#endif // defined(LWDEC_RISCV_EB_PRESENT) || defined (LWJPG_PRESENT)

    return status;
}

/*
 * @brief Setup FRTS shared falcon subWprs
 */
ACR_STATUS
acrSetupFrtsSubWprs_GH100(void)
{
    ACR_STATUS   status         = ACR_OK;
    LwU32        wpr2EndAddr;
    LwU32        wpr2StartAddr;
    LwU32        regVal;

    // GET WPR2 address
    wpr2StartAddr = DRF_VAL( _PFB, _PRI_MMU_WPR2_ADDR_LO, _VAL, ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_LO));
    wpr2EndAddr   = DRF_VAL( _PFB, _PRI_MMU_WPR2_ADDR_HI, _VAL, ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_HI));

    CHECK_UNSIGNED_INTEGER_ADDITION_AND_RETURN_IF_OUT_OF_BOUNDS((wpr2EndAddr << LW_PFB_PRI_MMU_WPR2_ADDR_LO_ALIGNMENT), ACR_REGION_ALIGN, LW_U32_MAX);
    wpr2EndAddr   = ((wpr2EndAddr << LW_PFB_PRI_MMU_WPR2_ADDR_LO_ALIGNMENT) + ACR_REGION_ALIGN - 1) >> LW_PFB_PRI_MMU_WPR2_ADDR_LO_ALIGNMENT;

    // Sanity Checks
    if (wpr2StartAddr >= wpr2EndAddr)
    {
         return ACR_ERROR_FRTS_CHECK_FAILED;
    }

    // Configure SubWPR Registers
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_FBFALCON, LSF_FALCON_INSTANCE_DEFAULT_0,
                        FBFALCON_SUB_WPR_ID_2_FRTS_DATA_WPR2, (LwU32)(wpr2StartAddr), (LwU32)(wpr2EndAddr),
                        ACR_SUB_WPR_MMU_RMASK_L2_AND_L3, ACR_SUB_WPR_MMU_WMASK_ALL_LEVELS_DISABLED, LW_TRUE));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_PMU_RISCV, LSF_FALCON_INSTANCE_DEFAULT_0,
                        PMU_SUB_WPR_ID_1_FRTS_DATA_WPR2, (LwU32)(wpr2StartAddr), (LwU32)(wpr2EndAddr),
                        ACR_SUB_WPR_MMU_RMASK_L2_AND_L3, ACR_SUB_WPR_MMU_WMASK_ALL_LEVELS_DISABLED, LW_TRUE));

    // Update FRTS status to clients
    regVal = ACR_REG_RD32(BAR0, LW_PGC6_AON_FRTS_INPUT_WPR_OFFSET_SELWRE_SCRATCH_GROUP_03_1);
    regVal = FLD_SET_DRF_NUM( _PGC6, _AON_FRTS_INPUT_WPR_OFFSET_SELWRE_SCRATCH_GROUP_03_1, _WPR_OFFSET, wpr2StartAddr, regVal);
    ACR_REG_WR32(BAR0, LW_PGC6_AON_FRTS_INPUT_WPR_OFFSET_SELWRE_SCRATCH_GROUP_03_1, regVal);

    regVal = ACR_REG_RD32(BAR0, LW_PGC6_AON_FRTS_INPUT_CTRL_SELWRE_SCRATCH_GROUP_03_2);
    regVal = FLD_SET_DRF( _PGC6, _AON_FRTS_INPUT_CTRL_SELWRE_SCRATCH_GROUP_03_2, _MEDIA_TYPE, _FB , regVal);
    regVal = FLD_SET_DRF_NUM( _PGC6, _AON_FRTS_INPUT_CTRL_SELWRE_SCRATCH_GROUP_03_2, _WPR_ID, ACR_WPR2_MMU_REGION_ID, regVal);
    ACR_REG_WR32(BAR0, LW_PGC6_AON_FRTS_INPUT_CTRL_SELWRE_SCRATCH_GROUP_03_2, regVal);

    regVal = ACR_REG_RD32(BAR0, LW_PGC6_AON_FRTS_OUTPUT_FLAG_SELWRE_SCRATCH_GROUP_03_2);
    regVal = FLD_SET_DRF( _PGC6, _AON_FRTS_OUTPUT_FLAG_SELWRE_SCRATCH_GROUP_03_2, _CMD_SUCCESSFUL, _YES, regVal);
    regVal = FLD_SET_DRF( _PGC6, _AON_FRTS_OUTPUT_FLAG_SELWRE_SCRATCH_GROUP_03_2, _ACR_READY, _NO, regVal);
    ACR_REG_WR32(BAR0, LW_PGC6_AON_FRTS_OUTPUT_FLAG_SELWRE_SCRATCH_GROUP_03_2, regVal);

    return status;
}

/*!
 * @brief Program particular falcon SubWpr in MMU and secure scratch
 * TODO : Add parameter checks for allowed values
 */
ACR_STATUS
acrProgramFalconSubWpr_GH100(LwU32 falconId, LwU32 falconInstance, LwU8 flcnSubWprId, LwU32 startAddr, LwU32 endAddr, LwU8 readMask, LwU8 writeMask, LwBool bSave)
{
    ACR_STATUS      status  = ACR_OK;
    ACR_FLCN_CONFIG flcnCfg;
    LwU32           regCfga = 0;
    LwU32           regCfgb = 0;

    // Get falcon config
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetFalconConfig_HAL(pAcr, falconId, falconInstance, &flcnCfg));

    //
    // Update subWpr in MMU
    // SubWpr registers CFGA and CFGB for single WPR are of 4 bytes and conselwtive
    // Registers for next subWpr of same falcon are 8 bytes seperated (4 bytes of CFGA + 4 bytes of CFGB)
    //
    regCfga = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_FALCON_PMU_CFGA, _ALLOW_READ, readMask, regCfga);
    regCfga = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_FALCON_PMU_CFGA, _ADDR_LO, startAddr, regCfga);
    ACR_REG_WR32(BAR0, (flcnCfg.subWprRangeAddrBase + (LwU32)(FALCON_SUB_WPR_INDEX_REGISTER_STRIDE * flcnSubWprId)), regCfga);

    regCfgb = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_FALCON_PMU_CFGB, _ALLOW_WRITE, writeMask, regCfgb);
    regCfgb = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_FALCON_PMU_CFGB, _ADDR_HI, endAddr, regCfgb);
    ACR_REG_WR32(BAR0, (flcnCfg.subWprRangeAddrBase + (LwU32)(FALCON_SUB_WPR_INDEX_REGISTER_STRIDE * flcnSubWprId)
                                                    + FALCON_SUB_WPR_CONFIG_REGISTER_OFFSET_DIFF), regCfgb);

    return status;
}


/*
 * @brief Disable all falcon subWprs because HW init value of subWpr is not disabled
 */
ACR_STATUS
acrProgramFalconSubWprExt_GH100
(
   PACR_FLCN_CONFIG pFlcnCfg,
   LwU8   flcnSubWprId,
   LwU32  startAddr,
   LwU32  endAddr,
   LwU8   readMask,
   LwU8   writeMask
)
{
    LwU32 regCfga = 0;
    LwU32 regCfgb = 0;

    //
    // Update subWpr in MMU
    // SubWpr registers CFGA and CFGB for single WPR are of 4 bytes and conselwtive
    // Registers for next subWpr of same falcon are 8 bytes separated (4 bytes of CFGA + 4 bytes of CFGB)
    //
    regCfga = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_FALCON_PMU_CFGA, _ALLOW_READ, readMask, regCfga);
    regCfga = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_FALCON_PMU_CFGA, _ADDR_LO, startAddr, regCfga);
    ACR_REG_WR32(BAR0, (pFlcnCfg->subWprRangeAddrBase + (LwU32)(FALCON_SUB_WPR_INDEX_REGISTER_STRIDE * flcnSubWprId)), regCfga);

    regCfgb = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_FALCON_PMU_CFGB, _ALLOW_WRITE, writeMask, regCfgb);
    regCfgb = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_FALCON_PMU_CFGB, _ADDR_HI, endAddr, regCfgb);
    ACR_REG_WR32(BAR0, (pFlcnCfg->subWprRangeAddrBase + (LwU32)(FALCON_SUB_WPR_INDEX_REGISTER_STRIDE * flcnSubWprId)
                                                      + FALCON_SUB_WPR_CONFIG_REGISTER_OFFSET_DIFF), regCfgb);

    return ACR_OK;
}

