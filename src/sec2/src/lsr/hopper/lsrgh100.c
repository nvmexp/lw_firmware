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
 * @file   lsrgh100.c
 * @brief  Contains new functions added for RISC-V LS support on GA10X.
 */

/* --------------------------- Sanity Check --------------------------------- */
#ifndef ACR_RISCV_LS
#error "Including lsrgh100.c in build"
#endif

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
/* ------------------------- Application Includes --------------------------- */
#include "acr.h"
#include "acr_objacrlib.h"
#include "sec2_objacr.h"
#include "config/g_lsr_hal.h"
#include "dev_riscv_pri.h"
#include "dev_fbif_v4.h"
#include "g_acrlib_private.h"


/* ------------------------- Macros and Defines ----------------------------- */

/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Prepares a RISCV uproc with external boot configuration.
 *
 * TODO: Update this function per the results of bugs 2720166 and 2720167.
 *
 * @param[in] pWprBase     Base address of WPR
 * @param[in] wprRegionID  ACR region ID
 * @param[in] pFlcnCfg     Falcon configuration
 * @param[in] pLsbHeader   LSB header
 */
ACR_STATUS
lsrAcrSetupLSRiscvExternalBoot_GH100
(
    PRM_FLCN_U64             pWprBase,
    LwU32                    wprRegionID,
    PACR_FLCN_CONFIG         pFlcnCfg,
    PLSF_LSB_HEADER_WRAPPER  pLsbHeaderWrapper
)
{
    ACR_STATUS  status            = ACR_OK;
    LwU32       addrHi            = 0;
    LwU32       addrLo            = 0;
    LwU64       wprBase           = 0;
    LwU32       blCodeSize        = 0;
    LwU32       ucodeOffset       = 0;
    LwU32       blImemOffset      = 0;
    LwU32       blDataSize        = 0;
    LwU32       blDataOffset      = 0;
    RM_FLCN_U64 physAddr          = {0};
    LwU64       wprAddr           = 0;
    LwU32       dst               = 0;
    LwU32       data              = 0;
    LwU64       riscvAddrPhy      = 0;
    LwU32       bootItcmOffset    = 0;
    LwU32       i                 = 0;

    if (pFlcnCfg->uprocType != ACR_TARGET_ENGINE_CORE_RISCV_EB)
    {
        return ACR_ERROR_ILWALID_OPERATION;
    }

    // Reset RISCV and set core to RISCV
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibPutFalconInReset_HAL(pAcrlib, pFlcnCfg));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibBringFalconOutOfReset_HAL(pAcrlib, pFlcnCfg));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibCoreSelect_HAL(pAcrlib, pFlcnCfg));

    //
    // Need to program IMEM/DMEM PLMs before DMA load data to IMEM/DMEM.
    // Program the IMEM and DMEM PLMs to the final desired values (which may
    // be level 3 for certain falcons) now that acrlib is done loading the
    // code onto the falcon.
    //
    // acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_IMEM_PRIV_LEVEL_MASK, pFlcnCfg->imemPLM);
    // acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_DMEM_PRIV_LEVEL_MASK, pFlcnCfg->dmemPLM);
    // CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibUpdateBootvecPlmLevel3Writeable_HAL(pAcrlib, pFlcnCfg));
    //

    for (i = 0; i < LW_PFALCON_FBIF_TRANSCFG__SIZE_1; i++)
    {
        acrlibProgramRegionCfg_HAL(pAcrlib, pFlcnCfg, LW_FALSE, i, LSF_WPR_EXPECTED_REGION_ID);
    }

    addrHi = pWprBase->hi;
    addrLo = pWprBase->lo;

    wprBase = (addrLo >> FLCN_IMEM_BLK_ALIGN_BITS) |
              (addrHi << (32 - FLCN_IMEM_BLK_ALIGN_BITS));

    // Read sw-bootrom code size
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_BL_CODE_SIZE,
                                                                        pLsbHeaderWrapper, (void *)&blCodeSize));

    // For RISCV-EB, sw-bootrom size must NOT be zero. 
    if (!blCodeSize)
    {
        return ACR_ERROR_SW_BOOTROM_SIZE_ILWALID; 
    }

    blCodeSize = LW_ALIGN_UP(blCodeSize, FLCN_IMEM_BLK_SIZE_IN_BYTES);

    // Write back aligned BL code size
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_SET_BL_CODE_SIZE,
                                                                        pLsbHeaderWrapper, (void *)&blCodeSize));
  
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_UCODE_OFFSET,
                                                                        pLsbHeaderWrapper, (void *)&ucodeOffset));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_BL_IMEM_OFFSET,
                                                                        pLsbHeaderWrapper, (void *)&blImemOffset));

    // Load BL into the last several blcoks, finding enough space to store BL.  
    dst = (acrlibFindFarthestImemBl_HAL(pAcrlib, pFlcnCfg, blCodeSize) * FLCN_IMEM_BLK_SIZE_IN_BYTES);

    // Load sw-bootrom code to IMEM
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibIssueTargetFalconDma_HAL(pAcrlib, dst, wprBase, (ucodeOffset + blImemOffset), blCodeSize,
                                      wprRegionID, LW_TRUE, LW_TRUE, LW_FALSE, pFlcnCfg));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_BL_DATA_OFFSET,
                                                                        pLsbHeaderWrapper, (void *)&blDataOffset));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_BL_DATA_SIZE,
                                                                        pLsbHeaderWrapper, (void *)&blDataSize));

    //
    // !!! Warning !!! lwrrently swbootrom data size is always zero, we don't validate below code yet.
    //      ________________________
    //     |   RISCV-EB DMEM layout | 
    // 0   |------------------------|
    //     |                        | 
    //     |    other's app data    |   
    //     |                        | 
    //     |------------------------| 
    //     | swbootrom data blob    |
    //     | size is blDataSize     |
    //     | (or = swBromCodeSize)  | 
    //     |------------------------|
    //     |  manifest data blob    |
    //     | size is manifest size  |
    // MAX |________________________|  
    // 
    if (blDataSize > 0)
    {
        LwU32 blDmemOffset;
        LwU32 manifestSize;
        LwU32 dmemSize;

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_MANIFEST_SIZE,
                                                                            pLsbHeaderWrapper, (void *)&manifestSize));

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetDmemSize_HAL(pAcrlib, pFlcnCfg, &dmemSize));

        // callwlate BL DMEM offset per above DMEM layout.
        blDmemOffset = dmemSize - manifestSize - blDataSize;

        // Load sw-bootrom data to DMEM
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibIssueTargetFalconDma_HAL(pAcrlib, blDmemOffset, wprBase, blDataOffset, blDataSize,
                                          wprRegionID, LW_TRUE, LW_FALSE, LW_FALSE, pFlcnCfg));
    }

    // Set BCR priv protection level.
    data = ACR_REG_RD32(BAR0,  pFlcnCfg->riscvRegisterBase + LW_PRISCV_RISCV_BCR_PRIV_LEVEL_MASK);
    data = FLD_SET_DRF(_PRISCV, _RISCV_BCR_PRIV_LEVEL, _MASK_READ_PROTECTION, _ALL_LEVELS_ENABLED, data);
    data = FLD_SET_DRF(_PRISCV, _RISCV_BCR_PRIV_LEVEL, _MASK_WRITE_PROTECTION, _ONLY_LEVEL3_ENABLED, data);
    ACR_REG_WR32(BAR0, pFlcnCfg->riscvRegisterBase + LW_PRISCV_RISCV_BCR_PRIV_LEVEL_MASK, data);

#ifdef BOOT_FROM_HS_BUILD
    RM_FLCN_U64_UNPACK(&wprAddr, pWprBase);
  
    // WPR base is expected to be 256 bytes aligned
    wprAddr = wprAddr >> 8;
 
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibProgramFalconCodeDataBaseInScratchGroup_HAL(pAcrlib, wprAddr, pLsbHeaderWrapper, pFlcnCfg));
#endif

    physAddr.lo  = ucodeOffset;
    LwU64_ALIGN32_ADD(&physAddr, pWprBase, &physAddr);
    LwU64_ALIGN32_UNPACK(&wprAddr, &physAddr);

    // This function just set BCR.
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibSetupBootvecRiscvExt_HAL(pAcrlib, pFlcnCfg, wprRegionID, pLsbHeaderWrapper, wprAddr));

    // Get IMEM size
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetImemSize_HAL(pAcrlib, pFlcnCfg, &data));
    bootItcmOffset = data - blCodeSize;
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibRiscvGetPhysicalAddress_HAL(pAcrlib, LW_RISCV_MEM_IMEM, (LwU64)bootItcmOffset, &riscvAddrPhy));

    // External boot rom (aka. hw-bootrom) set LW_RISCV_MEM_IMEM address to boot vector registers.
    acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg, REG_LABEL_RISCV_BOOT_VECTOR_HI, LwU64_HI32(riscvAddrPhy));
    acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg, REG_LABEL_RISCV_BOOT_VECTOR_LO, LwU64_LO32(riscvAddrPhy));

    return status;
}
