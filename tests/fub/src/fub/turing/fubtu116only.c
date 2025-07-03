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
 * @file: fubtu11xonly.c
 */

/* ------------------------- System Includes -------------------------------- */
#include "fub.h"
#include "fubreg.h"
#include "lwRmReg.h"
#include "dev_fuse.h"
#include "dev_master.h"
#include "dev_fpf.h"
#include "config/g_fub_private.h"
#include "config/fub-config.h"

extern RM_FLCN_FUB_DESC  g_desc;

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_GSYNC_ENABLING_FUSE))
/*
 * @brief: FUB deccides to burn GSYNC enabling fuse based on DEVID of GPU 
 *
 * @param[in] pbIsFuseBurningApplicable Set to true if check passes 
 */
FUB_STATUS
fubCheckIfBurningGsyncEnablingIsApplicable_TU116(LwBool *pbIsFuseBurningApplicable)
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
    // Check if use cases passed by RegKey includes burning GSYNC enabling fuse
    // In Case of failure set *pbIsFuseBurningApplicable to LW_FALSE but do not return error status 
    // as fuse burn decision is based on *pbIsFuseBurningApplicable and not returned on Error status.
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
        case LW_PMC_BOOT_42_CHIP_ID_TU116:
            if((devIdB == TU116_DEVIDB_TO_ENABLE_GSYNC_SKU1) && (devIdA == TU116_DEVIDA_TO_ENABLE_GSYNC_SKU1))
            {
                *pbIsFuseBurningApplicable = LW_TRUE;
            }
            else
            {
                *pbIsFuseBurningApplicable = LW_FALSE;
            }
            break;
        case LW_PMC_BOOT_42_CHIP_ID_TU117:
            if((devIdB == TU117_DEVIDB_TO_ENABLE_GSYNC_SKU1) && (devIdA == TU117_DEVIDA_TO_ENABLE_GSYNC_SKU1))
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
 * @brief: FUB deccides to burn Devid License Revocation fuse based on DEVID of GPU 
 *
 * @param[in] pbIsFuseBurningApplicable Set to true if check passes 
 */
FUB_STATUS
fubCheckIfBurningDevidRevocationIsApplicable_TU116(LwBool *pbIsFuseBurningApplicable)
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
    // as fuse burn decision is based on *pbIsFuseBurningApplicable and not  on returned Error status.
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

    // Devid based HULK Revocation use case
    switch(chip)
    {
        case LW_PMC_BOOT_42_CHIP_ID_TU116:
            if((devIdB == TU116_DEVIDB_TO_BLOCK_DEVID_LICENSE_SKU1) && (devIdA == TU116_DEVIDA_TO_BLOCK_DEVID_LICENSE_SKU1))
            {
                *pbIsFuseBurningApplicable = LW_TRUE;
            }
            else
            {
                *pbIsFuseBurningApplicable = LW_FALSE;
            }
            break;
        case LW_PMC_BOOT_42_CHIP_ID_TU117:
            if((devIdB == TU117_DEVIDB_TO_BLOCK_DEVID_LICENSE_SKU1) && (devIdA == TU117_DEVIDA_TO_BLOCK_DEVID_LICENSE_SKU1)) 
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

/*
 * @brief: DEVID check is same for both fuse burns of Enabling/Revoking HULK to read DRAM chipid 
 *         So forking new fuction to avoid code duplication
 *
 * @param[in] pbIsFuseBurningApplicable Set to true if check passes 
 */
FUB_STATUS fubCheckDevIdForHulkToReadDramChipid_TU116(LwBool *pbIsFuseBurningApplicable)
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

    // Blocking HULK license to Read DRAM CHIPID
    switch(chip)
    {
        case LW_PMC_BOOT_42_CHIP_ID_TU116:
            if (((devIdA == TU116_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_2184) && (devIdB == TU116_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_21C4)) ||
                ((devIdA == TU116_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_2182) && (devIdB == TU116_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_21C2)) ||
                ((devIdA == TU116_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_2191) && (devIdB == TU116_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_21D1)) ||
                ((devIdA == TU116_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_21AE) && (devIdB == TU116_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_21BE)) ||
                ((devIdA == TU116_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_21AF) && (devIdB == TU116_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_21BF)) ||
                ((devIdA == TU116_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_2186) && (devIdB == TU116_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_21C6)))

            {
                *pbIsFuseBurningApplicable = LW_TRUE;
            }
            else
            {
                *pbIsFuseBurningApplicable = LW_FALSE;
            }
            break;
        case LW_PMC_BOOT_42_CHIP_ID_TU117:
            if (((devIdA == TU117_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1FB0) && (devIdB == TU117_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1FF0)) ||
                ((devIdA == TU117_DEVIDA_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1F82) && (devIdB == TU117_DEVIDB_FOR_DRAM_CHIPID_READ_HULK_LICENSE_1FC2)))
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

