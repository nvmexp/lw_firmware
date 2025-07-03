/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: acr_sub_wpr_tu10x.c
 */
//
// Includes
//
#include "acr.h"
#include "mmu/mmucmn.h"
#include "dev_fb.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"

#include "config/g_acr_private.h"
#include "config/g_acrlib_private.h"

// Extern Variables
extern ACR_DMA_PROP     g_dmaProp;
extern RM_FLCN_ACR_DESC g_desc;
#ifdef AMPERE_PROTECTED_MEMORY
extern LwU32            g_offsetApmRts;
#endif
/*
 * @brief Setup all supported shared falcon subWprs
 */
ACR_STATUS
acrSetupSharedSubWprs_TU10X(void)
{
    ACR_STATUS status = ACR_OK;
#ifdef NEW_WPR_BLOBS
    PLSF_SHARED_SUB_WPR_HEADER_WRAPPER pSubWprWrapper;
    LwU8  subWprHeaderWrappers[LSF_SUB_WPR_HEADERS_WRAPPER_TOTAL_SIZE_MAX];
#else
    PLSF_SHARED_SUB_WPR_HEADER pSubWpr;
    LwU8  pSubWprHeader[LSF_SUB_WPR_HEADERS_TOTAL_SIZE_MAX];
#endif
    LwU64 startAddr;
    LwU64 endAddr;
    LwU64 subWprSizeInBytes;
    LwU32 index = 0;
    LwU32 useCaseId;
    LwU32 size4K;
    LwU32 subWprStartAddr;

#ifdef NEW_WPR_BLOBS
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrReadAllSubWprHeaderWrappers_HAL(pAcr, subWprHeaderWrappers));
#else
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrReadSubWprHeader_HAL(pAcr, pSubWprHeader));
#endif

    //
    // Loop through supported shared data subWpr use cases using index
    // Where, useCaseId represents the request by RM
    //
    for (index=0; index < LSF_SHARED_DATA_SUB_WPR_USE_CASE_ID_MAX; index++)
    {
#ifdef NEW_WPR_BLOBS
        pSubWprWrapper = ((PLSF_SHARED_SUB_WPR_HEADER_WRAPPER)(subWprHeaderWrappers + (index * sizeof(LSF_SHARED_SUB_WPR_HEADER_WRAPPER))));
         // Sanitize shared subWpr request
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfSubWprHeaderWrapperCtrl_HAL(pAcrlib, LSF_SUB_WPR_HEADER_COMMAND_GET_USECASEID,
                                                                               pSubWprWrapper, (void *)&useCaseId));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfSubWprHeaderWrapperCtrl_HAL(pAcrlib, LSF_SUB_WPR_HEADER_COMMAND_GET_SIZE4K,
                                                                               pSubWprWrapper, (void *)&size4K));
#else
        pSubWpr = ((PLSF_SHARED_SUB_WPR_HEADER)(pSubWprHeader + (index * sizeof(LSF_SHARED_SUB_WPR_HEADER))));
        useCaseId = pSubWpr->useCaseId;
        size4K = pSubWpr->size4K;
#endif
        // Sanitize shared subWpr request
        if (useCaseId == LSF_SHARED_DATA_SUB_WPR_USE_CASE_ID_ILWALID)
        {
            break;
        }
        switch (useCaseId)
        {
            case LSF_SHARED_DATA_SUB_WPR_USE_CASE_ID_FRTS_VBIOS_TABLES:
                if (size4K != LSF_SHARED_DATA_SUB_WPR_FRTS_VBIOS_TABLES_SIZE_IN_4K)
                {
                    return ACR_ERROR_SUB_WPR_ILWALID_REQUEST;
                }
                break;
            case LSF_SHARED_DATA_SUB_WPR_USE_CASE_ID_PLAYREADY_SHARED_DATA:
                if (size4K != LSF_SHARED_DATA_SUB_WPR_PLAYREADY_SHARED_DATA_SIZE_IN_4K)
                {
                    return ACR_ERROR_SUB_WPR_ILWALID_REQUEST;
                }
                break;
#ifdef AMPERE_PROTECTED_MEMORY
            case LSF_SHARED_DATA_SUB_WPR_USE_CASE_ID_APM_RTS:
                if (size4K != LSF_SHARED_DATA_SUB_WPR_APM_RTS_SIZE_IN_4K)
                {
                    return ACR_ERROR_SUB_WPR_ILWALID_REQUEST;
                }
                break;
#endif // AMPERE_PROTECTED_MEMORY
            default:
                return ACR_ERROR_SUB_WPR_ID_NOT_SUPPORTED;
        }

        subWprSizeInBytes = size4K << SHIFT_4KB;
        if (subWprSizeInBytes == 0)
        {
            // RM can not be using size of 0 or passing such a big number that left shift leads to 0
            return ACR_ERROR_ILWALID_ARGUMENT;
        }

#ifdef NEW_WPR_BLOBS
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfSubWprHeaderWrapperCtrl_HAL(pAcrlib, LSF_SUB_WPR_HEADER_COMMAND_GET_START_ADDR,
                                                                               pSubWprWrapper, (void *)&subWprStartAddr));
#else
        subWprStartAddr = pSubWpr->startAddr;
#endif

#ifdef AMPERE_PROTECTED_MEMORY
        if (useCaseId == LSF_SHARED_DATA_SUB_WPR_USE_CASE_ID_APM_RTS)
        {
            g_offsetApmRts = subWprStartAddr;
        }
#endif // AMPERE_PROTECTED_MEMORY

        startAddr = ((g_dmaProp.wprBase << 8) + subWprStartAddr);

        if (startAddr < subWprStartAddr)
        {
            return ACR_ERROR_VARIABLE_SIZE_OVERFLOW;
        }

        endAddr = startAddr + subWprSizeInBytes;    // actual endAddr is (this_value - 1), done below
        if (endAddr < subWprSizeInBytes)
        {
            return ACR_ERROR_VARIABLE_SIZE_OVERFLOW;
        }
        endAddr = endAddr - 1; // Can not underflow since endAddr >= subWprSize4K due to check immediately above and subWprSize4K > 0 due to a previous check. So, endAddr > 0.

        //
        // The following check is not required even after subtracting 1 above since
        // endAddr = startAddr + subWprSize4K - 1 with subWprSize4K >= 1. So, endAddr >= startAddr at this point.
        //
        /*
        if (endAddr < startAddr)
        {
            return ACR_ERROR_ILWALID_ARGUMENT;
        }
        */

        switch (useCaseId)
        {
            case LSF_SHARED_DATA_SUB_WPR_USE_CASE_ID_FRTS_VBIOS_TABLES:
                // Program FRTS WPR1 subWprs for FbFlcn and PMU
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_FBFALCON,
                                                    FBFALCON_SUB_WPR_ID_2_FRTS_DATA_WPR1, (startAddr >> SHIFT_4KB), (endAddr >> SHIFT_4KB),
                                                    ACR_SUB_WPR_MMU_RMASK_L2_AND_L3, ACR_SUB_WPR_MMU_WMASK_ALL_LEVELS_DISABLED, LW_TRUE));
#ifndef BOOT_FROM_HS_BUILD
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_PMU,
                                                    PMU_SUB_WPR_ID_1_FRTS_DATA_WPR1, (startAddr >> SHIFT_4KB), (endAddr >> SHIFT_4KB),
                                                    ACR_SUB_WPR_MMU_RMASK_L2_AND_L3, ACR_SUB_WPR_MMU_WMASK_ALL_LEVELS_DISABLED, LW_TRUE));
#endif
#ifdef ACR_RISCV_LS
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_PMU_RISCV,
                                                    PMU_SUB_WPR_ID_1_FRTS_DATA_WPR1, (startAddr >> SHIFT_4KB), (endAddr >> SHIFT_4KB),
                                                    ACR_SUB_WPR_MMU_RMASK_L2_AND_L3, ACR_SUB_WPR_MMU_WMASK_ALL_LEVELS_DISABLED, LW_TRUE));
#endif
                break;
            case LSF_SHARED_DATA_SUB_WPR_USE_CASE_ID_PLAYREADY_SHARED_DATA:
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_SEC2,
                                                    SEC2_SUB_WPR_ID_1_PLAYREADY_SHARED_DATA_WPR1, (startAddr >> SHIFT_4KB), (endAddr >> SHIFT_4KB),
                                                    ACR_SUB_WPR_MMU_RMASK_L2_AND_L3, ACR_SUB_WPR_MMU_WMASK_L2_AND_L3, LW_TRUE));
#ifdef LWDEC_RISCV_PRESENT
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_LWDEC_RISCV,
                                                    LWDEC0_SUB_WPR_ID_2_PLAYREADY_SHARED_DATA_WPR1, (startAddr >> SHIFT_4KB), (endAddr >> SHIFT_4KB),
                                                    ACR_SUB_WPR_MMU_RMASK_L3, ACR_SUB_WPR_MMU_WMASK_L3, LW_TRUE));
#else
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_LWDEC,
                                                    LWDEC0_SUB_WPR_ID_2_PLAYREADY_SHARED_DATA_WPR1, (startAddr >> SHIFT_4KB), (endAddr >> SHIFT_4KB),
                                                    ACR_SUB_WPR_MMU_RMASK_L3, ACR_SUB_WPR_MMU_WMASK_L3, LW_TRUE));
#endif
                break;
#ifdef AMPERE_PROTECTED_MEMORY
            case LSF_SHARED_DATA_SUB_WPR_USE_CASE_ID_APM_RTS:

                // TO DO: APM PID 4 - When APM in SEC2-RTOS supports HS make this SubWpr accessible only from L3
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_SEC2,
                                                    SEC2_SUB_WPR_ID_5_APM_RTS_WPR1, (startAddr >> SHIFT_4KB), (endAddr >> SHIFT_4KB),
                                                    ACR_SUB_WPR_MMU_RMASK_L2_AND_L3, ACR_SUB_WPR_MMU_WMASK_L2_AND_L3, LW_FALSE));
                break;
#endif
        }
    }

    return status;
}

/*!
 * @brief Program particular falcon SubWpr in MMU and secure scratch
 * TODO : Add parameter checks for allowed values
 */
ACR_STATUS
acrProgramFalconSubWpr_TU10X(LwU32 falconId, LwU8 flcnSubWprId, LwU32 startAddr, LwU32 endAddr, LwU8 readMask, LwU8 writeMask, LwBool bSave)
{
    ACR_STATUS      status  = ACR_OK;
    ACR_FLCN_CONFIG flcnCfg;
    LwU32           regCfga = 0;
    LwU32           regCfgb = 0;
    LwU32           regPlm  = 0;
    LwU32           scratchCfga = ILWALID_REG_ADDR;
    LwU32           scratchCfgb = ILWALID_REG_ADDR;
    LwU32           scratchPlm  = ILWALID_REG_ADDR;

    // Get falcon config
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetFalconConfig_HAL(pAcrlib, falconId, LSF_FALCON_INSTANCE_DEFAULT_0, &flcnCfg));

    // Get secure scratch allocation for save-restore of subWPR
    if (bSave)
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetSelwreScratchAllocationForSubWpr_HAL(pAcr, falconId, flcnSubWprId, &scratchCfga, &scratchCfgb, &scratchPlm));
    }

    //
    // Update subWpr in MMU
    // SubWpr registers CFGA and CFGB for single WPR are of 4 bytes and conselwtive
    // Registers for next subWpr of same falcon are 8 bytes seperated (4 bytes of CFGA + 4 bytes of CFGB)
    //
    regCfga = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_FALCON_PMU_CFGA, _ALLOW_READ, readMask, regCfga);
    regCfga = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_FALCON_PMU_CFGA, _ADDR_LO, startAddr, regCfga);
    ACR_REG_WR32(BAR0, (flcnCfg.subWprRangeAddrBase + (FALCON_SUB_WPR_INDEX_REGISTER_STRIDE * flcnSubWprId)), regCfga);

    regCfgb = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_FALCON_PMU_CFGB, _ALLOW_WRITE, writeMask, regCfgb);
    regCfgb = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_FALCON_PMU_CFGB, _ADDR_HI, endAddr, regCfgb);
    ACR_REG_WR32(BAR0, (flcnCfg.subWprRangeAddrBase + (FALCON_SUB_WPR_INDEX_REGISTER_STRIDE * flcnSubWprId)
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

    return status;
}

/*
 * @brief Read SubWpr Header from FB
 */
ACR_STATUS
acrReadSubWprHeader_TU10X(LwU8 *pSubWprHeader)
{
    LwU32 size = LSF_SUB_WPR_HEADERS_TOTAL_SIZE_MAX;

    if ((acrIssueDma_HAL(pAcr, pSubWprHeader, LW_FALSE, LSF_WPR_HEADERS_TOTAL_SIZE_MAX, size,
            ACR_DMA_FROM_FB, ACR_DMA_SYNC_AT_END, &g_dmaProp)) != size)
    {
        return ACR_ERROR_DMA_FAILURE;
    }
    return ACR_OK;
}

/*
 * @brief Setup SubWpr for code and data part of falcon's ucode in FB-WPR
 */
ACR_STATUS
acrSetupFalconCodeAndDataSubWprs_TU10X
(
    LwU32 falconId,
    PLSF_LSB_HEADER pLsfHeader
)
{
    ACR_STATUS status = ACR_OK;
    LwU64      codeStartAddr;
    LwU64      dataStartAddr;
    LwU64      codeEndAddr;
    LwU64      dataEndAddr;
    LwU32      codeSubWprId;
    LwU32      dataSubWprId;

    //
    // We are not programming GPCCS data & code subWpr's since GPCCS is priv-loaded and
    // niether does it use demand paging, thus it does not require access to its subWpr
    //
    if (falconId == LSF_FALCON_ID_GPCCS)
    {
        return ACR_OK;
    }

    if (pLsfHeader == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

#ifdef ACR_RISCV_LS
    // Get falcon config to determine architecture (Falcon/RISC-V).
    ACR_FLCN_CONFIG flcnCfg;
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetFalconConfig_HAL(pAcrlib, falconId, LSF_FALCON_INSTANCE_DEFAULT_0, &flcnCfg));

    //
    // RISC-V cores use the re-purposed app*Size fields to specify mWPRs.
    // See the code sample below for details:
    // https://confluence.lwpu.com/pages/viewpage.action?pageId=189948067#RISC-VLSBoot(GA100)-LSF_LSB_HEADER
    //
    if (flcnCfg.uprocType == ACR_TARGET_ENGINE_CORE_RISCV)
    {
        //
        // RISC-V mWPR layout:
        // 0 - Golden image (read-only).
        // 1 - Runtime image (read-write).
        //

        codeStartAddr = (g_dmaProp.wprBase << 8) + pLsfHeader->appCodeOffset;
        codeEndAddr = (codeStartAddr + pLsfHeader->appCodeSize - 1);

        dataStartAddr = (g_dmaProp.wprBase << 8) + pLsfHeader->appDataOffset;
        dataEndAddr = dataStartAddr + pLsfHeader->appDataSize - 1;
    }

    else
#endif
    {
        codeStartAddr = (g_dmaProp.wprBase << 8) + pLsfHeader->ucodeOffset;
        codeEndAddr   = (codeStartAddr + pLsfHeader->ucodeSize - 1);

        dataStartAddr = codeStartAddr + pLsfHeader->ucodeSize;
        dataEndAddr   = dataStartAddr + pLsfHeader->dataSize + pLsfHeader->blDataSize - 1;
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
        default:
            return ACR_ERROR_FLCN_ID_NOT_FOUND;
    }

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, falconId, codeSubWprId, (codeStartAddr >> SHIFT_4KB),
        (codeEndAddr >> SHIFT_4KB), ACR_SUB_WPR_MMU_RMASK_L2_AND_L3, ACR_SUB_WPR_MMU_WMASK_ALL_LEVELS_DISABLED, LW_TRUE));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, falconId, dataSubWprId, (dataStartAddr >> SHIFT_4KB),
        (dataEndAddr >> SHIFT_4KB), ACR_SUB_WPR_MMU_RMASK_L2_AND_L3, ACR_SUB_WPR_MMU_WMASK_L2_AND_L3, LW_TRUE));

    return status;
}

/*
 * @brief Get secure scratch register allocation for particular subWpr
 */
ACR_STATUS
acrGetSelwreScratchAllocationForSubWpr_TU10X(LwU32 falconId, LwU8 flcnSubWprId, LwU32 *pScratchCfga, LwU32 *pScratchCfgb, LwU32 *pScratchPlm)
{
    if ((pScratchCfga == NULL) || (pScratchCfgb == NULL) || (pScratchPlm == NULL))
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    switch (falconId)
    {
//
// ASB binary runs on falcon GSP and boots SEC2 in LS mode. It does not need to know Sub WPR allocation of other Falcons,
// ASB needs only Sub WPR allocation of GSP for call stack (ASB : acrEntryFunction->acrBootFalcon->acrProgramSubWpr(GSPLITE, GSP_SUB_WPR_ID_2_ASB_FULL_WPR1)).
// So to reduce ASB binary size, other falcon cases are not included in ASB.
//
#ifndef ASB
        case LSF_FALCON_ID_PMU:
#ifdef ACR_RISCV_LS
        case LSF_FALCON_ID_PMU_RISCV:
#endif
            switch(flcnSubWprId)
            {
                case PMU_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1:
                    *pScratchCfga = LW_PGC6_SELWRE_SCRATCH_PMU_SUB_WPR_ID_0_CFGA;
                    *pScratchCfgb = LW_PGC6_SELWRE_SCRATCH_PMU_SUB_WPR_ID_0_CFGB;
                    *pScratchPlm  = LW_PGC6_SELWRE_SCRATCH_PMU_SUB_WPR_ID_0_PRIV_LEVEL_MASK;
                    break;
                case PMU_SUB_WPR_ID_1_FRTS_DATA_WPR1:
                    *pScratchCfga = LW_PGC6_SELWRE_SCRATCH_PMU_SUB_WPR_ID_1_CFGA;
                    *pScratchCfgb = LW_PGC6_SELWRE_SCRATCH_PMU_SUB_WPR_ID_1_CFGB;
                    *pScratchPlm  = LW_PGC6_SELWRE_SCRATCH_PMU_SUB_WPR_ID_1_PRIV_LEVEL_MASK;
                    break;
                case PMU_SUB_WPR_ID_2_UCODE_DATA_SECTION_WPR1:
                    *pScratchCfga = LW_PGC6_SELWRE_SCRATCH_PMU_SUB_WPR_ID_2_CFGA;
                    *pScratchCfgb = LW_PGC6_SELWRE_SCRATCH_PMU_SUB_WPR_ID_2_CFGB;
                    *pScratchPlm  = LW_PGC6_SELWRE_SCRATCH_PMU_SUB_WPR_ID_2_PRIV_LEVEL_MASK;
                    break;
                default:
                    return ACR_ERROR_SUB_WPR_ID_NOT_SUPPORTED;
            }
            break;
        case LSF_FALCON_ID_FECS:
            switch(flcnSubWprId)
            {
                case FECS_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1:
                    *pScratchCfga = LW_PGC6_SELWRE_SCRATCH_FECS_SUB_WPR_ID_0_CFGA;
                    *pScratchCfgb = LW_PGC6_SELWRE_SCRATCH_FECS_SUB_WPR_ID_0_CFGB;
                    *pScratchPlm  = LW_PGC6_SELWRE_SCRATCH_FECS_SUB_WPR_ID_0_PRIV_LEVEL_MASK;
                    break;
                case FECS_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1:
                    *pScratchCfga = LW_PGC6_SELWRE_SCRATCH_FECS_SUB_WPR_ID_1_CFGA;
                    *pScratchCfgb = LW_PGC6_SELWRE_SCRATCH_FECS_SUB_WPR_ID_1_CFGB;
                    *pScratchPlm  = LW_PGC6_SELWRE_SCRATCH_FECS_SUB_WPR_ID_1_PRIV_LEVEL_MASK;
                    break;
                default:
                    return ACR_ERROR_SUB_WPR_ID_NOT_SUPPORTED;
            }
            break;
        case LSF_FALCON_ID_SEC2:
            switch(flcnSubWprId)
            {
                case SEC2_SUB_WPR_ID_0_ACRLIB_FULL_WPR1:
                    *pScratchCfga = LW_PGC6_SELWRE_SCRATCH_SEC2_SUB_WPR_ID_0_CFGA;
                    *pScratchCfgb = LW_PGC6_SELWRE_SCRATCH_SEC2_SUB_WPR_ID_0_CFGB;
                    *pScratchPlm  = LW_PGC6_SELWRE_SCRATCH_SEC2_SUB_WPR_ID_0_PRIV_LEVEL_MASK;
                    break;
                case SEC2_SUB_WPR_ID_1_PLAYREADY_SHARED_DATA_WPR1:
                    *pScratchCfga = LW_PGC6_SELWRE_SCRATCH_SEC2_SUB_WPR_ID_1_CFGA;
                    *pScratchCfgb = LW_PGC6_SELWRE_SCRATCH_SEC2_SUB_WPR_ID_1_CFGB;
                    *pScratchPlm  = LW_PGC6_SELWRE_SCRATCH_SEC2_SUB_WPR_ID_1_PRIV_LEVEL_MASK;
                    break;
                case SEC2_SUB_WPR_ID_3_UCODE_CODE_SECTION_WPR1:
                    *pScratchCfga = LW_PGC6_SELWRE_SCRATCH_SEC2_SUB_WPR_ID_3_CFGA;
                    *pScratchCfgb = LW_PGC6_SELWRE_SCRATCH_SEC2_SUB_WPR_ID_3_CFGB;
                    *pScratchPlm  = LW_PGC6_SELWRE_SCRATCH_SEC2_SUB_WPR_ID_3_PRIV_LEVEL_MASK;
                    break;
                case SEC2_SUB_WPR_ID_4_UCODE_DATA_SECTION_WPR1:
                    *pScratchCfga = LW_PGC6_SELWRE_SCRATCH_SEC2_SUB_WPR_ID_4_CFGA;
                    *pScratchCfgb = LW_PGC6_SELWRE_SCRATCH_SEC2_SUB_WPR_ID_4_CFGB;
                    *pScratchPlm  = LW_PGC6_SELWRE_SCRATCH_SEC2_SUB_WPR_ID_4_PRIV_LEVEL_MASK;
                    break;
                default:
                    return ACR_ERROR_SUB_WPR_ID_NOT_SUPPORTED;
            }
            break;
        case LSF_FALCON_ID_LWDEC:
#ifdef LWDEC_RISCV_PRESENT
        case LSF_FALCON_ID_LWDEC_RISCV:
#endif
            switch(flcnSubWprId)
            {
                case LWDEC0_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1:
                    *pScratchCfga = LW_PGC6_SELWRE_SCRATCH_LWDEC0_SUB_WPR_ID_0_CFGA;
                    *pScratchCfgb = LW_PGC6_SELWRE_SCRATCH_LWDEC0_SUB_WPR_ID_0_CFGB;
                    *pScratchPlm  = LW_PGC6_SELWRE_SCRATCH_LWDEC0_SUB_WPR_ID_0_PRIV_LEVEL_MASK;
                    break;
                case LWDEC0_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1:
                    *pScratchCfga = LW_PGC6_SELWRE_SCRATCH_LWDEC0_SUB_WPR_ID_1_CFGA;
                    *pScratchCfgb = LW_PGC6_SELWRE_SCRATCH_LWDEC0_SUB_WPR_ID_1_CFGB;
                    *pScratchPlm  = LW_PGC6_SELWRE_SCRATCH_LWDEC0_SUB_WPR_ID_1_PRIV_LEVEL_MASK;
                    break;
                case LWDEC0_SUB_WPR_ID_2_PLAYREADY_SHARED_DATA_WPR1:
                    *pScratchCfga = LW_PGC6_SELWRE_SCRATCH_LWDEC0_SUB_WPR_ID_2_CFGA;
                    *pScratchCfgb = LW_PGC6_SELWRE_SCRATCH_LWDEC0_SUB_WPR_ID_2_CFGB;
                    *pScratchPlm  = LW_PGC6_SELWRE_SCRATCH_LWDEC0_SUB_WPR_ID_2_PRIV_LEVEL_MASK;
                    break;
                default:
                    return ACR_ERROR_SUB_WPR_ID_NOT_SUPPORTED;
            }
            break;
        case LSF_FALCON_ID_FBFALCON:
            switch(flcnSubWprId)
            {
                case FBFALCON_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1:
                    *pScratchCfga = LW_PGC6_SELWRE_SCRATCH_FBFALCON_SUB_WPR_ID_0_CFGA;
                    *pScratchCfgb = LW_PGC6_SELWRE_SCRATCH_FBFALCON_SUB_WPR_ID_0_CFGB;
                    *pScratchPlm  = LW_PGC6_SELWRE_SCRATCH_FBFALCON_SUB_WPR_ID_0_PRIV_LEVEL_MASK;
                    break;
                case FBFALCON_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1:
                    *pScratchCfga = LW_PGC6_SELWRE_SCRATCH_FBFALCON_SUB_WPR_ID_1_CFGA;
                    *pScratchCfgb = LW_PGC6_SELWRE_SCRATCH_FBFALCON_SUB_WPR_ID_1_CFGB;
                    *pScratchPlm  = LW_PGC6_SELWRE_SCRATCH_FBFALCON_SUB_WPR_ID_1_PRIV_LEVEL_MASK;
                    break;
                case FBFALCON_SUB_WPR_ID_2_FRTS_DATA_WPR1:
                    *pScratchCfga = LW_PGC6_SELWRE_SCRATCH_FBFALCON_SUB_WPR_ID_2_CFGA;
                    *pScratchCfgb = LW_PGC6_SELWRE_SCRATCH_FBFALCON_SUB_WPR_ID_2_CFGB;
                    *pScratchPlm  = LW_PGC6_SELWRE_SCRATCH_FBFALCON_SUB_WPR_ID_2_PRIV_LEVEL_MASK;
                    break;
                default:
                    return ACR_ERROR_SUB_WPR_ID_NOT_SUPPORTED;
            }
            break;
#endif //ASB
        case LSF_FALCON_ID_GSPLITE:
#ifdef ACR_RISCV_LS
        case LSF_FALCON_ID_GSP_RISCV:
#endif
            switch(flcnSubWprId)
            {
                case GSP_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1:
                    *pScratchCfga = LW_PGC6_SELWRE_SCRATCH_GSP_SUB_WPR_ID_0_CFGA;
                    *pScratchCfgb = LW_PGC6_SELWRE_SCRATCH_GSP_SUB_WPR_ID_0_CFGB;
                    *pScratchPlm  = LW_PGC6_SELWRE_SCRATCH_GSP_SUB_WPR_ID_0_PRIV_LEVEL_MASK;
                    break;
                case GSP_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1:
                    *pScratchCfga = LW_PGC6_SELWRE_SCRATCH_GSP_SUB_WPR_ID_1_CFGA;
                    *pScratchCfgb = LW_PGC6_SELWRE_SCRATCH_GSP_SUB_WPR_ID_1_CFGB;
                    *pScratchPlm  = LW_PGC6_SELWRE_SCRATCH_GSP_SUB_WPR_ID_1_PRIV_LEVEL_MASK;
                    break;
                case GSP_SUB_WPR_ID_2_ASB_FULL_WPR1:
                    *pScratchCfga = LW_PGC6_SELWRE_SCRATCH_GSP_SUB_WPR_ID_2_CFGA;
                    *pScratchCfgb = LW_PGC6_SELWRE_SCRATCH_GSP_SUB_WPR_ID_2_CFGB;
                    *pScratchPlm  = LW_PGC6_SELWRE_SCRATCH_GSP_SUB_WPR_ID_2_PRIV_LEVEL_MASK;
                    break;
                default:
                    return ACR_ERROR_SUB_WPR_ID_NOT_SUPPORTED;
            }
            break;
        default:
            return ACR_ERROR_FLCN_ID_NOT_FOUND;
    }

    return ACR_OK;
}


/*!
 * Disable all falcon subWprs because HW init value of subWpr is not disabled on Turing, Bug 200444247 has details
 * Note that this function should be called before programming any valid(known use case) subWpr
 */
ACR_STATUS
acrDisableAllFalconSubWprs_TU10X(void)
{
    ACR_STATUS      status       = ACR_OK;
    ACR_FLCN_CONFIG flcnCfg;
    LwU32           falconId     = LSF_FALCON_ID_ILWALID;
    LwU32           flcnSubWprId = LSF_SUB_WPR_ID_ILWALID;

    // Disable Falcon SubWprs for ACR managed falcons
    for (falconId = 0; falconId < LSF_FALCON_ID_END; falconId++)
    {
        if (acrlibGetFalconConfig_HAL(pAcrlib, falconId, LSF_FALCON_INSTANCE_DEFAULT_0, &flcnCfg) != ACR_OK)
        {
            //
            // Only cover falcons which are supported by particular ACR profile
            // Note that if falcon is not LSFM managed but supported by ACR profile,
            // we will still reset its subWpr settings as it is no harm
            // Not checking for LSFM manage here, because it requires us to read WPR header
            //
            continue;
        }

        if (flcnCfg.maxSubWprSupportedByHw == 0)
        {
            return ACR_ERROR_SUB_WPR_INFO_NOT_AVAILABLE;
        }

        for (flcnSubWprId = 0; flcnSubWprId < flcnCfg.maxSubWprSupportedByHw; flcnSubWprId++)
        {
            // This check is required because FWSEC has already setup few subWprs before ACR
            if (acrCheckIfWARRequiredForBadHwInitOfSubWPR_HAL(pAcr, &flcnCfg, flcnSubWprId))
            {
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, falconId, flcnSubWprId,
                                                    ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI,
                                                    ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_FALSE));
            }
        }
    }

    //
    // Disable falcon subWprs for non ACR managed falcons(LWDEC1, LWDEC2, and LWENC) on Turing
    // Since we do not have ACR_FLCN_CONFIG for these falcons and since they do not have their LSF_FALCON_ID,
    // we need to hardcode the subWprs for these falcons. Bug 200444247 is filed HW bug
    //
    {
        // LWENC0
        for (flcnSubWprId = 0; flcnSubWprId < LW_PFB_PRI_MMU_FALCON_LWENC0_CFGA__SIZE_1; flcnSubWprId++)
        {
            ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_FALCON_LWENC0_CFGA(flcnSubWprId), DRF_NUM(_PFB, _PRI_MMU_FALCON_LWENC0_CFGA, _ALLOW_READ, ACR_UNLOCK_READ_MASK) |
                                                                                DRF_NUM(_PFB, _PRI_MMU_FALCON_LWENC0_CFGA, _ADDR_LO, ILWALID_WPR_ADDR_LO));
            ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_FALCON_LWENC0_CFGB(flcnSubWprId), DRF_NUM(_PFB, _PRI_MMU_FALCON_LWENC0_CFGB, _ALLOW_WRITE, ACR_UNLOCK_WRITE_MASK) |
                                                                                DRF_NUM(_PFB, _PRI_MMU_FALCON_LWENC0_CFGB, _ADDR_HI, ILWALID_WPR_ADDR_HI));
        }
        // LWDEC1
        for (flcnSubWprId = 0; flcnSubWprId < LW_PFB_PRI_MMU_FALCON_LWDEC1_CFGA__SIZE_1; flcnSubWprId++)
        {
            ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_FALCON_LWDEC1_CFGA(flcnSubWprId), DRF_NUM(_PFB, _PRI_MMU_FALCON_LWDEC1_CFGA, _ALLOW_READ, ACR_UNLOCK_READ_MASK) |
                                                                                DRF_NUM(_PFB, _PRI_MMU_FALCON_LWDEC1_CFGA, _ADDR_LO, ILWALID_WPR_ADDR_LO));
            ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_FALCON_LWDEC1_CFGB(flcnSubWprId), DRF_NUM(_PFB, _PRI_MMU_FALCON_LWDEC1_CFGB, _ALLOW_WRITE, ACR_UNLOCK_WRITE_MASK) |
                                                                                DRF_NUM(_PFB, _PRI_MMU_FALCON_LWDEC1_CFGB, _ADDR_HI, ILWALID_WPR_ADDR_HI));
        }
        // LWDEC2
        for (flcnSubWprId = 0; flcnSubWprId < LW_PFB_PRI_MMU_FALCON_LWDEC2_CFGA__SIZE_1; flcnSubWprId++)
        {
            ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_FALCON_LWDEC2_CFGA(flcnSubWprId), DRF_NUM(_PFB, _PRI_MMU_FALCON_LWDEC2_CFGA, _ALLOW_READ, ACR_UNLOCK_READ_MASK) |
                                                                                DRF_NUM(_PFB, _PRI_MMU_FALCON_LWDEC2_CFGA, _ADDR_LO, ILWALID_WPR_ADDR_LO));
            ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_FALCON_LWDEC2_CFGB(flcnSubWprId), DRF_NUM(_PFB, _PRI_MMU_FALCON_LWDEC2_CFGB, _ALLOW_WRITE, ACR_UNLOCK_WRITE_MASK) |
                                                                                DRF_NUM(_PFB, _PRI_MMU_FALCON_LWDEC2_CFGB, _ADDR_HI, ILWALID_WPR_ADDR_HI));
        }
    }

    return status;
}

