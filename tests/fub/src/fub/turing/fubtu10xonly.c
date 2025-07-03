/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018 - 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: fubtu10xonly.c
 */

/* ------------------------- System Includes -------------------------------- */
#include "fub.h"
#include "fubreg.h"
#include "config/g_fub_private.h"
#include "config/fub-config.h"
#include "lwRmReg.h"
#include "dev_fuse.h"
#include "dev_master.h"
#include "dev_fpf.h"
#include "lwdevid.h"
#include "dev_top.h"
#include "dev_pri_ringstation_sys.h"
#include "dev_gc6_island.h"
#include "fub_inline_static_funcs_tu10x.h"
#include "fub_inline_static_funcs_fpf_tu10x.h"
#include "fub_inline_static_funcs_fpf_tu116.h"
#include "fub_inline_static_funcs_tu116.h"
#include "dev_gc6_island_addendum.h"


extern RM_FLCN_FUB_DESC  g_desc;

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_GSYNC_ENABLING_FUSE))
/*
 * @brief: FUB decides to burn GSYNC enabling fuse based on DEVID of GPU and
 *         Use cases passed to FUB through REG Key
 *
 * @param[in] pbIsFuseBurningApplicable Set to true if check passes
 */
FUB_STATUS
fubCheckIfBurningGsyncEnablingIsApplicable_TU10X(LwBool *pbIsFuseBurningApplicable)
{
    LwU32      devIdB          = 0;
    LwU32      devIdA          = 0;
    LwU32      chip            = 0;
    FUB_STATUS fubStatus       = FUB_OK;

    if (pbIsFuseBurningApplicable == NULL)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }

    *pbIsFuseBurningApplicable = LW_FALSE;

    //
    // Check if use cases passed by RegKey includes burning GSYNC enabling fuse
    // In Case of failure set *pbIsFuseBurningApplicable to LW_FALSE but do not return error status
    // as fuse burn decision is based on *pbIsFuseBurningApplicable and not on returned Error status.
    //
    if (!(g_desc.useCaseMask & LW_REG_STR_RM_FUB_USE_CASE_MASK_ENABLE_GSYNC))
    {
        *pbIsFuseBurningApplicable = LW_FALSE;
        fubStatus                  = FUB_OK;
        goto ErrorExit;
    }

    //
    // For GSYNC panels DEVIDB is used, but in case if strap to select DEVID is messed up with
    // FUB should check for both DEVIDS.
    //
    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_FUSE_OPT_PCIE_DEVIDB, &devIdB));
    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_FUSE_OPT_PCIE_DEVIDA, &devIdA));

    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_PMC_BOOT_42, &chip));
    chip = DRF_VAL( _PMC, _BOOT_42, _CHIP_ID, chip);

    // Denote that a particular unit is shipping with G-Sync panel.
    switch(chip)
    {
        case LW_PMC_BOOT_42_CHIP_ID_TU102:
            if((devIdB == TU102_DEVIDB_TO_ENABLE_GSYNC_SKU_700) && (devIdA == TU102_DEVIDA_TO_ENABLE_GSYNC_SKU_700))
            {
                *pbIsFuseBurningApplicable = LW_TRUE;
            }
            else
            {
                *pbIsFuseBurningApplicable = LW_FALSE;
            }
            break;
        case LW_PMC_BOOT_42_CHIP_ID_TU104:
            if (((devIdB == TU104_DEVIDB_TO_ENABLE_GSYNC_SKU_N18E_G3_ES_A1) && (devIdA == TU104_DEVIDA_TO_ENABLE_GSYNC_SKU_N18E_G3_ES_A1)) ||
                ((devIdB == TU104_DEVIDB_TO_ENABLE_GSYNC_SKU_750) && (devIdA == TU104_DEVIDA_TO_ENABLE_GSYNC_SKU_750)))
            {
                *pbIsFuseBurningApplicable = LW_TRUE;
            }
            else
            {
                *pbIsFuseBurningApplicable = LW_FALSE;
            }
            break;
        case LW_PMC_BOOT_42_CHIP_ID_TU106:
            if (((devIdB == TU106_DEVIDB_TO_ENABLE_GSYNC_SKU_725) && (devIdA == TU106_DEVIDA_TO_ENABLE_GSYNC_SKU_725)) ||
             ((devIdB == TU106_DEVIDB_TO_ENABLE_GSYNC_SKU_750) && (devIdA == TU106_DEVIDA_TO_ENABLE_GSYNC_SKU_750)))
            {
                *pbIsFuseBurningApplicable = LW_TRUE;
            }
            else
            {
                *pbIsFuseBurningApplicable = LW_FALSE;
            }
            break;
        default:
            CHECK_FUB_STATUS(FUB_ERR_UNSUPPORTED_CHIP);
    }

ErrorExit:
    return fubStatus;
}
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_DEVID_LICENSE_REVOCATION_FUSE))
/*
 * @brief: FUB decides to burn Devid License Revocation fuse based on DEVID of GPU and
 *         Use cases passed to FUB through REG Key
 *
 * @param[in] pbIsFuseBurningApplicable Set to true if check passes
 */
FUB_STATUS
fubCheckIfBurningDevidRevocationIsApplicable_TU10X(LwBool *pbIsFuseBurningApplicable)
{
    LwU32      devIdA          = 0;
    LwU32      devIdB          = 0;
    LwU32      chip            = 0;
    FUB_STATUS fubStatus       = FUB_OK;

    if (pbIsFuseBurningApplicable == NULL)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }

    *pbIsFuseBurningApplicable = LW_FALSE;

    //
    // Check if use cases passed by RegKey includes burning Revoking DEVID based HULK license fuse
    // In Case of failure set *pbIsFuseBurningApplicable to LW_FALSE but do not return error status
    // as fuse burn decision is based on *pbIsFuseBurningApplicable and not on returned Error status.
    //
    if (!(g_desc.useCaseMask & LW_REG_STR_RM_FUB_USE_CASE_MASK_REVOKE_DEVID_BASED_HULK))
    {
        *pbIsFuseBurningApplicable = LW_FALSE;
        fubStatus                  = FUB_OK;
        goto ErrorExit;
    }

    //
    // For NON GSYNC use cases DEVIDA is used, but in case if strap to select DEVID is messed up with
    // FUB should check for both DEVIDS.
    //
    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_FUSE_OPT_PCIE_DEVIDA, &devIdA));
    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_FUSE_OPT_PCIE_DEVIDB, &devIdB));

    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_PMC_BOOT_42, &chip));
    chip = DRF_VAL( _PMC, _BOOT_42, _CHIP_ID, chip);

    // Devid based license Revocation use case
    switch(chip)
    {
        case LW_PMC_BOOT_42_CHIP_ID_TU102:
            if(((devIdB == TU102_DEVIDB_TO_BLOCK_DEVID_LICENSE_SKU_895) && (devIdA == TU102_DEVIDA_TO_BLOCK_DEVID_LICENSE_SKU_895)) ||
               ((devIdB == TU102_DEVIDB_TO_BLOCK_DEVID_LICENSE_SKU_890) && (devIdA == TU102_DEVIDA_TO_BLOCK_DEVID_LICENSE_SKU_890)))
            {
                *pbIsFuseBurningApplicable = LW_TRUE;
            }
            else
            {
                *pbIsFuseBurningApplicable = LW_FALSE;
            }
            break;
        case LW_PMC_BOOT_42_CHIP_ID_TU104:
            if (((devIdB == TU104_DEVIDB_TO_BLOCK_DEVID_LICENSE_SKU_895) && (devIdA == TU104_DEVIDA_TO_BLOCK_DEVID_LICENSE_SKU_895)) ||
                ((devIdB == TU104_DEVIDB_TO_BLOCK_DEVID_LICENSE_SKU_896) && (devIdA == TU104_DEVIDA_TO_BLOCK_DEVID_LICENSE_SKU_896)))
            {
                *pbIsFuseBurningApplicable = LW_TRUE;
            }
            else
            {
                *pbIsFuseBurningApplicable = LW_FALSE;
            }
            break;
        default:
            CHECK_FUB_STATUS(FUB_ERR_UNSUPPORTED_CHIP);
    }
ErrorExit:
    return fubStatus;
}
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_ALLOW_DRAM_CHIPID_READ_HULK_LICENSE) || \
     FUBCFG_FEATURE_ENABLED(FUB_BURN_REVOKE_DRAM_CHIPID_READ_HULK_LICENSE))
/*
 * @brief: DEVID check is same for both fuse burns of Enabling/Revoking HULK to read DRAM chipid
 *         So forking new fuction to avoid code duplication
 *
 * @param[in] pbIsFuseBurningApplicable Set to true if check passes
 */
FUB_STATUS fubCheckDevIdForHulkToReadDramChipid_TU10X(LwBool *pbIsFuseBurningApplicable)
{
    LwU32      devIdA          = 0;
    LwU32      devIdB          = 0;
    LwU32      chip            = 0;
    FUB_STATUS fubStatus       = FUB_ERR_GENERIC;

    if (pbIsFuseBurningApplicable == NULL)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }

    *pbIsFuseBurningApplicable = LW_FALSE;

    // In case if strap to select DEVID out of DEVIDA and DEVIDB is messed up with, FUB should check for both DEVIDS.
    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_FUSE_OPT_PCIE_DEVIDA, &devIdA));
    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_FUSE_OPT_PCIE_DEVIDB, &devIdB));

    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_PMC_BOOT_42, &chip));
    chip = DRF_VAL( _PMC, _BOOT_42, _CHIP_ID, chip);

    // Allowing DRAM CHIPID read use case
    switch(chip)
    {
        case LW_PMC_BOOT_42_CHIP_ID_TU102:
            if(((devIdA == TU102_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1E07) && (devIdB == TU102_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1E47)) ||
               ((devIdA == TU102_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1E04) && (devIdB == TU102_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1E44)) ||
               ((devIdA == TU102_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1E02) && (devIdB == TU102_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1E42)) ||
               ((devIdA == TU102_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1E36) && (devIdB == TU102_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1E76)) ||
               ((devIdA == TU102_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1E30) && (devIdB == TU102_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1E70)) ||
               ((devIdA == TU102_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1E37) && (devIdB == TU102_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1E77)) ||
               ((devIdA == TU102_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1E38) && (devIdB == TU102_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1E78)) ||
               ((devIdA == TU102_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1E3D) && (devIdB == TU102_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1E7D)) ||
               ((devIdA == TU102_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1E3C) && (devIdB == TU102_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1E7C)) ||
               ((devIdA == TU102_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1E2F) && (devIdB == TU102_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1E3F)) ||
               ((devIdA == TU102_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1E2E) && (devIdB == TU102_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1E3E)))
            {
                *pbIsFuseBurningApplicable = LW_TRUE;
            }
            else
            {
                *pbIsFuseBurningApplicable = LW_FALSE;
            }
            break;
        case LW_PMC_BOOT_42_CHIP_ID_TU104:
            if(((devIdA == TU104_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1E89) && (devIdB == TU104_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1EC9)) ||
               ((devIdA == TU104_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1E82) && (devIdB == TU104_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1EC2)) ||
               ((devIdA == TU104_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1E87) && (devIdB == TU104_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1EC7)) ||
               ((devIdA == TU104_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1EBA) && (devIdB == TU104_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1EFA)) ||
               ((devIdA == TU104_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1EBB) && (devIdB == TU104_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1EFB)) ||
               ((devIdA == TU104_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1E90) && (devIdB == TU104_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1ED0)) ||
               ((devIdA == TU104_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1EB1) && (devIdB == TU104_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1EF1)) ||
               ((devIdA == TU104_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1EB0) && (devIdB == TU104_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1EF0)) ||
               ((devIdA == TU104_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1EB8) && (devIdB == TU104_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1EF8)) ||
               ((devIdA == TU104_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1EB9) && (devIdB == TU104_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1EF9)) ||
               ((devIdA == TU104_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1EB6) && (devIdB == TU104_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1EF6)) ||
               ((devIdA == TU104_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1EB5) && (devIdB == TU104_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1EF5)) ||
               ((devIdA == TU104_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1EAB) && (devIdB == TU104_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1EEB)) ||
               ((devIdA == TU104_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1EAF) && (devIdB == TU104_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1EBF)) ||
               ((devIdA == TU104_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1EAE) && (devIdB == TU104_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1EBE)))
            {
                *pbIsFuseBurningApplicable = LW_TRUE;
            }
            else
            {
                *pbIsFuseBurningApplicable = LW_FALSE;
            }
            break;

        case LW_PMC_BOOT_42_CHIP_ID_TU106:
            if(((devIdA == TU106_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1F04) && (devIdB == TU106_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1F44)) ||
               ((devIdA == TU106_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1F08) && (devIdB == TU106_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1F48)) ||
               ((devIdA == TU106_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1F02) && (devIdB == TU106_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1F42)) ||
               ((devIdA == TU106_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1F07) && (devIdB == TU106_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1F47)) ||
               ((devIdA == TU106_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1F11) && (devIdB == TU106_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1F51)) ||
               ((devIdA == TU106_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1F10) && (devIdB == TU106_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1F50)) ||
               ((devIdA == TU106_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1F38) && (devIdB == TU106_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1F78)) ||
               ((devIdA == TU106_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1F36) && (devIdB == TU106_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1F76)) ||
               ((devIdA == TU106_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1F2F) && (devIdB == TU106_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1F3F)) ||
               ((devIdA == TU106_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1F2E) && (devIdB == TU106_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1F3E)))
            {
                *pbIsFuseBurningApplicable = LW_TRUE;
            }
            else
            {
                *pbIsFuseBurningApplicable = LW_FALSE;
            }
            break;
        default:
            CHECK_FUB_STATUS(FUB_ERR_UNSUPPORTED_CHIP);
    }

ErrorExit:
    return fubStatus;
}
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_GEFORCE_SKU_RECOVERY))
/*
 * @brief: First Check if VdChip SKU recovery fuse is to be burnt and Burn if check passes
 */
FUB_STATUS
fubCheckAndBurnGeforceSkuRecoveryFuse_TU10X(LwU32 *pfubUseCasMask)
{
    LwBool        bIsFuseBurningApplicable = LW_FALSE;
    FUB_STATUS    fubStatus                = FUB_ERR_GENERIC;
    FPF_FUSE_INFO fuseInfo;

    // Return early if pointer passed as argument is NULL.
    if (pfubUseCasMask == NULL)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }

    // Return early if current use case specific bit is set in use case mask
    if (*pfubUseCasMask & LW_FUB_USE_CASE_MASK_GEFORCE_SKU_RECOVERY)
    {
        CHECK_FUB_STATUS(FUB_ERR_GEFORCE_SKU_RECOVERY_SPARE_BIT_ALREADY_SET_IN_MASK);
    }

    // Before Burning Fuse Verify DEVID check passes.
    CHECK_FUB_STATUS(fubCheckIfGeforceSkuRecoveryIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));

    if(bIsFuseBurningApplicable)
    {
        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_GEFORCE_SKU_RECOVERY));
        // Proceed to Burning only if New fuseValue bits are not already burnt
        if (fuseInfo.fuseAdjustedBurlwalue)
        {
            // Burn Primary Fuse
            CHECK_FUB_STATUS(fubWriteFieldProgFuse_HAL(pFub, fuseInfo.fusePriAddress, fuseInfo.fuseAdjustedBurlwalue));

            // Burn Redundant Fuse
            CHECK_FUB_STATUS(fubWriteFieldProgFuse_HAL(pFub, fuseInfo.fuseRedAddress, fuseInfo.fuseAdjustedBurlwalue));

            //
            // If we reach this point then, fuse burn is successful,
            // Set use case specific bit in mask variable, for further reference.
            //
            *pfubUseCasMask = (*pfubUseCasMask | LW_FUB_USE_CASE_MASK_GEFORCE_SKU_RECOVERY);
        }
        else
        {
            CHECK_FUB_STATUS(FUB_ERR_FUSE_BURN_NOT_REQUIRED);
        }
    }
    else
    {
        // Do not return error as FUB may be running for another use case.
        fubStatus = FUB_OK;
    }
ErrorExit:
    return fubStatus;
}
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_GEFORCE_SKU_RECOVERY_GFW_REV))
/*
 * @brief: First Check if VdChip SKU recovery GFW_REV fuse is to be burnt and Burn if check passes
 */
FUB_STATUS
fubCheckAndBurnGeforceSkuRecoveryGfwRevFuse_TU10X(LwU32 *pfubUseCasMask)
{
    LwBool        bIsFuseBurningApplicable = LW_FALSE;
    FUB_STATUS    fubStatus                = FUB_ERR_GENERIC;
    FPF_FUSE_INFO fuseInfo;

    // Return early if pointer passed as argument is NULL.
    if (pfubUseCasMask == NULL)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }

    // Return early if current use case specific bit is set in use case mask
    if (*pfubUseCasMask & LW_FUB_USE_CASE_MASK_GEFORCE_SKU_RECOVERY_GFW_REV)
    {
        CHECK_FUB_STATUS(FUB_ERR_GEFORCE_SKU_RECOVERY_GFW_REV_BIT_ALREADY_SET_IN_MASK);
    }

    // Before Burning Fuse Verify DEVID check passes.
    CHECK_FUB_STATUS(fubCheckIfGeforceSkuRecoveryGfwRevIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));

    if(bIsFuseBurningApplicable)
    {
        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_GEFORCE_SKU_RECOVERY_GFW_REV));
        // Proceed to Burning only if New fuseValue bits are not already burnt
        if (fuseInfo.fuseAdjustedBurlwalue)
        {
            // Burn Primary Fuse
            CHECK_FUB_STATUS(fubWriteFieldProgFuse_HAL(pFub, fuseInfo.fusePriAddress, fuseInfo.fuseAdjustedBurlwalue));

            // Burn Redundant Fuse
            CHECK_FUB_STATUS(fubWriteFieldProgFuse_HAL(pFub, fuseInfo.fuseRedAddress, fuseInfo.fuseAdjustedBurlwalue));

            //
            // If we reach this point then, fuse burn is successful,
            // Set use case specific bit in mask variable, for further reference.
            //
            *pfubUseCasMask = (*pfubUseCasMask | LW_FUB_USE_CASE_MASK_GEFORCE_SKU_RECOVERY_GFW_REV);
        }
        else
        {
            CHECK_FUB_STATUS(FUB_ERR_FUSE_BURN_NOT_REQUIRED);
        }
    }
    else
    {
        // Do not return error as FUB may be running for another use case.
        fubStatus = FUB_OK;
    }
ErrorExit:
    return fubStatus;
}
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_GEFORCE_SKU_RECOVERY))
/*
 * @brief: FUB decides to burn fuse for VdChip SKU Recovery based on
 *         DEVID+STRAP of GPU and Use cases passed to FUB through REG Key
 *
 * @param[in] pbIsFuseBurningApplicable Set to true if check passes
 */
FUB_STATUS
fubCheckIfGeforceSkuRecoveryIsApplicable_TU10X(LwBool *pbIsFuseBurningApplicable)
{
    FUB_STATUS    fubStatus       = FUB_OK;
    FPF_FUSE_INFO fuseInfo;
    LwU32         fuseBar0Value   = 0;

    if (pbIsFuseBurningApplicable == NULL)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }

    *pbIsFuseBurningApplicable = LW_FALSE;

    //
    // Check if use cases passed by RegKey includes burning fuse for VdChip SKU recovery.
    // In Case of failure set *pbIsFuseBurningApplicable to LW_FALSE but do not return error status
    // as fuse burn decision is based on *pbIsFuseBurningApplicable and not on returned Error status.
    //
    if (!(g_desc.useCaseMask & LW_REG_STR_RM_FUB_USE_CASE_MASK_GEFORCE_SKU_RECOVERY))
    {
        *pbIsFuseBurningApplicable = LW_FALSE;
        fubStatus                  = FUB_OK;
        goto ErrorExit;
    }

    // Now Check if DEVID and Strap of GPU is in Approved list of this use case
    CHECK_FUB_STATUS(fubCheckDevIdAndStrapForGeforceSkuRecovery_HAL(pFub, pbIsFuseBurningApplicable));

    if (*pbIsFuseBurningApplicable)
    {
        *pbIsFuseBurningApplicable = LW_FALSE;

        //
        // Goal is to blow GFW_REV[2:2] and and SPARE_FUSES_3[4:4] with one FUB
        // invocation.
        // We should blow GFW_REV[2:2] first to avoid the scenario
        // of shipping old GFW code that doesn't understand SPARE_FUSES_3[4:4].
        // Therefore check here whether GFW_REV[2:2] is blown.
        //
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_GEFORCE_SKU_RECOVERY_GFW_REV));
        CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, fuseInfo.fuseBar0Address, &fuseBar0Value));
        if ((fuseBar0Value & fuseInfo.fuseIntendedVal) == fuseInfo.fuseIntendedVal)
        {
            *pbIsFuseBurningApplicable = LW_TRUE;
        }
    }

    if (!*pbIsFuseBurningApplicable)
    {
        fubReportApplicabilityFailure_HAL(pFub, LW_FUB_USE_CASE_MASK_GEFORCE_SKU_RECOVERY);
    }

ErrorExit:
    return fubStatus;
}
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_GEFORCE_SKU_RECOVERY_GFW_REV))
/*
 * @brief: FUB decides to burn fuse for VdChip SKU Recovery GFW_REV based on
 *         DEVID+STRAP of GPU and Use cases passed to FUB through REG Key
 *
 * @param[in] pbIsFuseBurningApplicable Set to true if check passes
 */
FUB_STATUS
fubCheckIfGeforceSkuRecoveryGfwRevIsApplicable_TU10X(LwBool *pbIsFuseBurningApplicable)
{
    FUB_STATUS    fubStatus       = FUB_OK;

    if (pbIsFuseBurningApplicable == NULL)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }

    *pbIsFuseBurningApplicable = LW_FALSE;

    //
    // Check if use cases passed by RegKey includes burning fuse for VdChip SKU recovery GFW_REV.
    // In Case of failure set *pbIsFuseBurningApplicable to LW_FALSE but do not return error status
    // as fuse burn decision is based on *pbIsFuseBurningApplicable and not on returned Error status.
    //
    if (!(g_desc.useCaseMask & LW_REG_STR_RM_FUB_USE_CASE_MASK_GEFORCE_SKU_RECOVERY_GFW_REV))
    {
        *pbIsFuseBurningApplicable = LW_FALSE;
        fubStatus                  = FUB_OK;
        goto ErrorExit;
    }

    // Now Check if DEVID and Strap of GPU is in Approved list of this use case
    CHECK_FUB_STATUS(fubCheckDevIdAndStrapForGeforceSkuRecovery_HAL(pFub, pbIsFuseBurningApplicable));

    if (!*pbIsFuseBurningApplicable)
    {
        fubReportApplicabilityFailure_HAL(pFub, LW_FUB_USE_CASE_MASK_GEFORCE_SKU_RECOVERY_GFW_REV);
    }

ErrorExit:
    return fubStatus;
}
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_GEFORCE_SKU_RECOVERY) ||\
     FUBCFG_FEATURE_ENABLED(FUB_BURN_GEFORCE_SKU_RECOVERY_GFW_REV))
/*
 * @brief: Check DEVID+STRAP for VdChip SKU Recovery
 *
 * @param[in] pbIsFuseBurningApplicable Set to true if check passes
 */
FUB_STATUS fubCheckDevIdAndStrapForGeforceSkuRecovery_TU10X(LwBool *pbIsFuseBurningApplicable)
{
    LwU32      devIdA          = 0;
    LwU32      devIdB          = 0;
    LwU32      chip            = 0;
    LwU32      strap           = 0;
    FUB_STATUS fubStatus       = FUB_ERR_GENERIC;

    if (pbIsFuseBurningApplicable == NULL)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }

    *pbIsFuseBurningApplicable = LW_FALSE;

    // In case if strap to select DEVID out of DEVIDA and DEVIDB is messed up with, FUB should check for both DEVIDS.
    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_FUSE_OPT_PCIE_DEVIDA, &devIdA));
    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_FUSE_OPT_PCIE_DEVIDB, &devIdB));

    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_PMC_BOOT_42, &chip));
    chip = DRF_VAL( _PMC, _BOOT_42, _CHIP_ID, chip);

    // VdChip SKU recovery use case
    switch(chip)
    {
        case LW_PMC_BOOT_42_CHIP_ID_TU104:
            if(((devIdA == TU104_DEVIDA_FOR_GEFORCE_SKU_RECOVERY_SKU_1E87) && (devIdB == TU104_DEVIDB_FOR_GEFORCE_SKU_RECOVERY_SKU_1EC7)) ||
               ((devIdA == TU104_DEVIDA_FOR_GEFORCE_SKU_RECOVERY_SKU_1E82) && (devIdB == TU104_DEVIDB_FOR_GEFORCE_SKU_RECOVERY_SKU_1EC2)))
            {
                *pbIsFuseBurningApplicable = LW_TRUE;
            }
            else
            {
                *pbIsFuseBurningApplicable = LW_FALSE;
            }
            break;

        case LW_PMC_BOOT_42_CHIP_ID_TU106:
            if(((devIdA == TU106_DEVIDA_FOR_GEFORCE_SKU_RECOVERY_SKU_1F07) && (devIdB == TU106_DEVIDB_FOR_GEFORCE_SKU_RECOVERY_SKU_1F47)) ||
               ((devIdA == TU106_DEVIDA_FOR_GEFORCE_SKU_RECOVERY_SKU_1F02)  && (devIdB == TU106_DEVIDB_FOR_GEFORCE_SKU_RECOVERY_SKU_1F42)))
            {
                *pbIsFuseBurningApplicable = LW_TRUE;
            }
            else
            {
                *pbIsFuseBurningApplicable = LW_FALSE;
            }
            break;
        default:
            CHECK_FUB_STATUS(FUB_ERR_UNSUPPORTED_CHIP);
    }

    //
    // Check if DEVID strap is set to REBRAND mode in HW
    // Per PG160 SKU 50 (TU106) and PG180 SKU 0 (TU104) schematics, this is true if STRAP5 is non-zero
    // Confirmed with board team
    //
    if (*pbIsFuseBurningApplicable)
    {
        *pbIsFuseBurningApplicable = LW_FALSE;

        CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_PGC6_SCI_STRAP_DEBUG, &strap));

        if (!FLD_TEST_DRF_NUM(_PGC6, _SCI_STRAP_DEBUG, _STRAP5, 0, strap))
        {
            *pbIsFuseBurningApplicable = LW_TRUE;
        }
    }

ErrorExit:
    return fubStatus;
}
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_GFW_REV_FOR_WP_BYPASS))
/*
 * @brief: First Check if GFW_REV fuse for WP_BYPASS mitigation is to be burnt and Burn if check passes
 */
FUB_STATUS
fubCheckAndBurnGfwRevForWpBypassFuse_TU10X(LwU32 *pfubUseCasMask)
{
    LwBool        bIsFuseBurningApplicable = LW_FALSE;
    FUB_STATUS    fubStatus                = FUB_ERR_GENERIC;
    FPF_FUSE_INFO fuseInfo;

    // Return early if pointer passed as argument is NULL.
    if (pfubUseCasMask == NULL)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }

    // Return early if current use case specific bit is set in use case mask
    if (*pfubUseCasMask & LW_FUB_USE_CASE_MASK_GFW_REV_FOR_WP_BYPASS)
    {
        CHECK_FUB_STATUS(FUB_ERR_GFW_REV_FOR_WP_BYPASS_BIT_ALREADY_SET_IN_MASK);
    }

    // Before Burning Fuse Verify DEVID check passes.
    CHECK_FUB_STATUS(fubCheckIfGfwRevForWpBypassIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));
    if(bIsFuseBurningApplicable)
    {
        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_GFW_REV_FOR_WP_BYPASS));

        // Proceed to Burning only if New fuseValue bits are not already burnt
        if (fuseInfo.fuseAdjustedBurlwalue)
        {
            // Burn Primary Fuse
            CHECK_FUB_STATUS(fubWriteFieldProgFuse_HAL(pFub, fuseInfo.fusePriAddress, fuseInfo.fuseAdjustedBurlwalue));

            // Burn Redundant Fuse
            CHECK_FUB_STATUS(fubWriteFieldProgFuse_HAL(pFub, fuseInfo.fuseRedAddress, fuseInfo.fuseAdjustedBurlwalue));

            //
            // If we reach this point then, fuse burn is successful,
            // Set use case specific bit in mask variable, for further reference.
            //
            *pfubUseCasMask = (*pfubUseCasMask | LW_FUB_USE_CASE_MASK_GFW_REV_FOR_WP_BYPASS);
        }
        else
        {
            CHECK_FUB_STATUS(FUB_ERR_FUSE_BURN_NOT_REQUIRED);
        }
    }
    else
    {
        // Do not return error as FUB may be running for another use case.
        fubStatus = FUB_OK;
    }
ErrorExit:
    return fubStatus;
}

/*
 * @brief: FUB decides to burn fuse for LWFLASH Revoke GFW_REV based on
 *         DEVID of GPU and Use cases passed to FUB through REG Key
 *
 * @param[in] pbIsFuseBurningApplicable Set to true if check passes
 */
FUB_STATUS
fubCheckIfGfwRevForWpBypassIsApplicable_TU10X(LwBool *pbIsFuseBurningApplicable)
{
    FUB_STATUS    fubStatus       = FUB_OK;

    if (pbIsFuseBurningApplicable == NULL)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }

    *pbIsFuseBurningApplicable = LW_FALSE;

    //
    // Check if use cases passed by RegKey includes burning fuse for GFW_REV for WP_BYPASS mitigation.
    // In Case of failure set *pbIsFuseBurningApplicable to LW_FALSE but do not return error status
    // as fuse burn decision is based on *pbIsFuseBurningApplicable and not on returned Error status.
    //
    if (!(g_desc.useCaseMask & LW_REG_STR_RM_FUB_USE_CASE_MASK_GFW_REV_FOR_WP_BYPASS))
    {
        *pbIsFuseBurningApplicable = LW_FALSE;
        fubStatus                  = FUB_OK;
        goto ErrorExit;
    }


    CHECK_FUB_STATUS(fubCheckIfWpBypassIsApplicableCommonChecks_HAL(pFub, pbIsFuseBurningApplicable));

    if (!*pbIsFuseBurningApplicable)
    {
        fubReportApplicabilityFailure_HAL(pFub, LW_FUB_USE_CASE_MASK_GFW_REV_FOR_WP_BYPASS);
    }

ErrorExit:
    return fubStatus;
}
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_LWFLASH_REV_FOR_WP_BYPASS))
/*
 * @brief: First Check if ROM_FLASH__REV fuse for WP_BYPASS mitigation is to be burnt and Burn if check passes
 */
FUB_STATUS
fubCheckAndBurnLwflashRevForWpBypassFuse_TU10X(LwU32 *pfubUseCasMask)
{
    LwBool        bIsFuseBurningApplicable = LW_FALSE;
    FUB_STATUS    fubStatus                = FUB_ERR_GENERIC;
    FPF_FUSE_INFO fuseInfo;

    // Return early if pointer passed as argument is NULL.
    if (pfubUseCasMask == NULL)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }

    // Return early if current use case specific bit is set in use case mask
    if (*pfubUseCasMask & LW_FUB_USE_CASE_MASK_LWFLASH_REV_FOR_WP_BYPASS)
    {
        CHECK_FUB_STATUS(FUB_ERR_LWFLASH_REV_FOR_WP_BYPASS_SPARE_BIT_ALREADY_SET_IN_MASK);
    }

    // Before Burning Fuse Verify DEVID check passes.
    CHECK_FUB_STATUS(fubCheckIfLwflashRevForWpBypassIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));

    if(bIsFuseBurningApplicable)
    {
        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_LWFLASH_REV_FOR_WP_BYPASS));
        // Proceed to Burning only if New fuseValue bits are not already burnt
        if (fuseInfo.fuseAdjustedBurlwalue)
        {
            // Burn Primary Fuse
            CHECK_FUB_STATUS(fubWriteFieldProgFuse_HAL(pFub, fuseInfo.fusePriAddress, fuseInfo.fuseAdjustedBurlwalue));

            // Burn Redundant Fuse
            CHECK_FUB_STATUS(fubWriteFieldProgFuse_HAL(pFub, fuseInfo.fuseRedAddress, fuseInfo.fuseAdjustedBurlwalue));

            //
            // If we reach this point then, fuse burn is successful,
            // Set use case specific bit in mask variable, for further reference.
            //
            *pfubUseCasMask = (*pfubUseCasMask | LW_FUB_USE_CASE_MASK_LWFLASH_REV_FOR_WP_BYPASS);
        }
        else
        {
            CHECK_FUB_STATUS(FUB_ERR_FUSE_BURN_NOT_REQUIRED);
        }
    }
    else
    {
        // Do not return error as FUB may be running for another use case.
        fubStatus = FUB_OK;
    }
ErrorExit:
    return fubStatus;
}

/*
 * @brief: FUB decides to burn fuse for WP_BYPASS mitigation for all SKUs
 *
 * @param[in] pbIsFuseBurningApplicable Set to true if check passes
 */
FUB_STATUS
fubCheckIfLwflashRevForWpBypassIsApplicable_TU10X(LwBool *pbIsFuseBurningApplicable)
{
    FUB_STATUS    fubStatus       = FUB_OK;

    if (pbIsFuseBurningApplicable == NULL)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }

    *pbIsFuseBurningApplicable = LW_FALSE;

    //
    // Check if use cases passed by RegKey includes burning fuse for WP_BYPASS.
    // In Case of failure set *pbIsFuseBurningApplicable to LW_FALSE but do not return error status
    // as fuse burn decision is based on *pbIsFuseBurningApplicable and not on returned Error status.
    //
    if (!(g_desc.useCaseMask & LW_REG_STR_RM_FUB_USE_CASE_MASK_LWFLASH_REV_FOR_WP_BYPASS))
    {
        *pbIsFuseBurningApplicable = LW_FALSE;
        fubStatus                  = FUB_OK;
        goto ErrorExit;
    }

    CHECK_FUB_STATUS(fubCheckIfWpBypassIsApplicableCommonChecks_HAL(pFub, pbIsFuseBurningApplicable));

    if (!*pbIsFuseBurningApplicable)
    {
        fubReportApplicabilityFailure_HAL(pFub, LW_FUB_USE_CASE_MASK_LWFLASH_REV_FOR_WP_BYPASS);
    }

ErrorExit:
    return fubStatus;
}
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_LWFLASH_REV_FOR_WP_BYPASS) || \
     FUBCFG_FEATURE_ENABLED(FUB_BURN_GFW_REV_FOR_WP_BYPASS))
/*
 * @brief:Common Check for 2 Fuse burns of LWFLASH Revoke use case
 *
 * @param[in] pbIsFuseBurningApplicable Set to true if check passes
 */
FUB_STATUS
fubCheckIfWpBypassIsApplicableCommonChecks_TU10X(LwBool *pbIsFuseBurningApplicable)
{

    FUB_STATUS    fubStatus          = FUB_ERR_GENERIC;
    LwU32         devIdA             = 0;
    LwU32         devIdB             = 0;
    LwU32         chip               = 0;
    LwBool        bHypervisorBlessed = LW_FALSE;

    if(pbIsFuseBurningApplicable == NULL)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }

    *pbIsFuseBurningApplicable = LW_FALSE;

    CHECK_FUB_STATUS(fubIsHypervisorBlessed_HAL(pFub, &bHypervisorBlessed));

    if(bHypervisorBlessed)
    {
        CHECK_FUB_STATUS(fubSmbpbiWriteProtectionDisabled_HAL(pFub));
        CHECK_FUB_STATUS(fubAwsUdeVersionCheck_HAL(pFub));
    }
    else
    {
        CHECK_FUB_STATUS(FUB_ERR_HYPERVISOR_WRITE_CHECK_FAILED);
    }

    //
    // In case if strap to select DEVID is messed up with
    // FUB should check for both DEVIDS.
    //
    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_FUSE_OPT_PCIE_DEVIDB, &devIdB));
    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_FUSE_OPT_PCIE_DEVIDA, &devIdA));

    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_PMC_BOOT_42, &chip));
    chip = DRF_VAL( _PMC, _BOOT_42, _CHIP_ID, chip);

    // These defines are coming from lwdevid.h, so make sure they are not tampered with
    ct_assert(LW_PCI_DEVID_DEVICE_PG183_SKU200_DEVIDB == (0x1EF8));
    ct_assert(LW_PCI_DEVID_DEVICE_PG183_SKU200 == (0x1EB8));

    switch(chip)
    {
        case LW_PMC_BOOT_42_CHIP_ID_TU104:
            if((devIdB == LW_PCI_DEVID_DEVICE_PG183_SKU200_DEVIDB) && (devIdA == LW_PCI_DEVID_DEVICE_PG183_SKU200))
            {
                *pbIsFuseBurningApplicable = LW_TRUE;
            }
            else
            {
                 *pbIsFuseBurningApplicable = LW_FALSE;
            }
            break;
        default:
            CHECK_FUB_STATUS(FUB_ERR_UNSUPPORTED_CHIP);
    }

ErrorExit:
    return fubStatus;
}
#endif
/*!
 * @brief:  LWFLASH provides write protection mechanism for the GPU firmware based on selective I/O passthruough, which is implemented by the hypervisor to prevent using LWFLASH from guest VM.
 *          LWFLASH uses LW_PTOP_SCRATCH0 to determine if erase and program operations are allowed based on whether LWFLASH code is able to write to scratch register.
 *          This function checks whether scratch register is set
 *
 * @param[in]: pbIsHypervisorBlessed : Sets to true if hypervisor write protection is enabled (value of LW_PTOP_SCRATCH0 register value)
 *
 * @return   : FUB_STATUS
 *
 */

// Decode Trap 17 programmed by FWSEC to protect LW_PTOP_SCRATCH0
#define DECODE_TRAP_17_PROGRAMMED_BY_FWSEC              (17)
FUB_STATUS
fubIsHypervisorBlessed_TU10X
(
    LwBool *pbIsHypervisorBlessed
)
{
    FUB_STATUS fubStatus = FUB_ERR_GENERIC;

    if (pbIsHypervisorBlessed == NULL)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }

    *pbIsHypervisorBlessed = LW_FALSE;

    LwU32 decodeTrap17MatchReg = 0;
    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0,LW_PPRIV_SYS_PRI_DECODE_TRAP_MATCH(DECODE_TRAP_17_PROGRAMMED_BY_FWSEC),
                &decodeTrap17MatchReg));

    //
    // FLD_TEST_DRF_NUM(d,r,f,n,v)             ((DRF_VAL(d, r, f, (v)) == (LwU32)(n)))
    // Check if LW_PTOP_SCRATCH0 is programmed in decode trap 17 by FwSec
    //
    if(FLD_TEST_DRF_NUM(_PPRIV, _SYS_PRI_DECODE_TRAP_MATCH, _ADDR,
                LW_PTOP_SCRATCH0, decodeTrap17MatchReg))
    {
        // Make sure that write protection is enabled on decode trap 17;
        LwU32 decodeTrap17Action = 0;
        CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP_ACTION(DECODE_TRAP_17_PROGRAMMED_BY_FWSEC),
                    &decodeTrap17Action));

        if(FLD_TEST_DRF(_PPRIV, _SYS_PRI_DECODE_TRAP_ACTION,
                        _DROP_TRANSACTION, _ENABLE, decodeTrap17Action))
        {

            // Program decode trap to read actual scratch register information
            decodeTrap17Action = FLD_SET_DRF(_PPRIV, _SYS_PRI_DECODE_TRAP_ACTION,
                                _RETURN_SPECIFIED_DATA, _DISABLE, decodeTrap17Action);
            CHECK_FUB_STATUS(FALC_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP_ACTION(DECODE_TRAP_17_PROGRAMMED_BY_FWSEC),
                    decodeTrap17Action));

            LwU32 scratch = 0;
            CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_PTOP_SCRATCH0, &scratch));

            // Reset read protection
            decodeTrap17Action = FLD_SET_DRF(_PPRIV, _SYS_PRI_DECODE_TRAP_ACTION,
                                        _RETURN_SPECIFIED_DATA, _ENABLE, decodeTrap17Action);
            CHECK_FUB_STATUS(FALC_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP_ACTION(DECODE_TRAP_17_PROGRAMMED_BY_FWSEC),
                            decodeTrap17Action));

            if(scratch == 2)
            {
                *pbIsHypervisorBlessed = LW_TRUE;
            }
        }
        else
        {
            CHECK_FUB_STATUS(FUB_ERR_HYPERVISOR_DECODE_TRAP_WRITE_PROTECTION_UNSET);
        }
    }

ErrorExit:
    return fubStatus;
}

/*!
 * @brief: Checks if SMBPBI write protection is enabled or not
 *
 * @param[in]: @None
 *
 * @return   : FUB_STATUS
 *
 */
FUB_STATUS
fubSmbpbiWriteProtectionDisabled_TU10X
(
    void
)
{
    FUB_STATUS  fubStatus       = FUB_ERR_GENERIC;
    // Setting it 0xffffffff as part of FI countermeasure
    LwU32       scratch_05_01   = 0xffffffff;
    LwU32       scratchGroup17  = 0xffffffff;

    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0,
        LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_1,
        &scratch_05_01));

    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0,
        LW_PGC6_AON_SELWRE_SCRATCH_GROUP_17,
        &scratchGroup17));

    if(FLD_TEST_DRF(_PGC6, _AON_SELWRE_SCRATCH_GROUP_05_1,
        _WRITE_PROTECT_MODE, _DISABLED, scratch_05_01) &&
        FLD_TEST_DRF(_PGC6, _AON_SELWRE_SCRATCH_GROUP_17,
        _WRITE_PROTECT_MODE, _DISABLED, scratchGroup17)
      )
    {
        fubStatus = FUB_OK;
    }
    else
    {
        CHECK_FUB_STATUS(FUB_ERR_SMBPBI_WRITE_PROTECTION_ENABLED);
    }
ErrorExit:
    return fubStatus;
}

/*!
 * @brief: Checks AWS VBIOS version
 *
 * @param[in]: @None
 *
 * @return   : FUB_STATUS
 *
 */

// Check http://lwbugs/2623776/21 for the details
#define AWS_UDE_VERSION_JULY_25_2019             (4)

FUB_STATUS
fubAwsUdeVersionCheck_TU10X
(
    void
)
{
    FUB_STATUS  fubStatus = FUB_ERR_GENERIC;
    LwU32       scratch15 = 0;

    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_PGC6_BSI_VPR_SELWRE_SCRATCH_15,
                    &scratch15));

    if(DRF_VAL(_PGC6, _BSI_VPR_SELWRE_SCRATCH_15,
        _VBIOS_UDE_VERSION, scratch15) == AWS_UDE_VERSION_JULY_25_2019)
    {
        fubStatus = FUB_OK;
    }
    else
    {
        CHECK_FUB_STATUS(FUB_ERR_AWS_UDE_VERSION_CHECK_FAILED);
    }
ErrorExit:
    return fubStatus;
}

