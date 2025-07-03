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
 * @file: selwrescrubga10x.c
 * This file covers HAL for chips GA10X.
 */
/* ------------------------- System Includes -------------------------------- */
#include "selwrescrub.h"
#include "selwrescrubreg.h"
#include "dev_fuse.h"
#include "dev_master.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"
#include "dev_gc6_island.h"
#include "dev_fb.h"
#include "dev_pri_ringstation_sys_addendum.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

//
// Default Ucode version of Scrubber for chips of GA10X family(except for GA102)
// TODO : Remove its usage after Prod Signing and make changes accordingly
//
#define  SELWRESCRUB_GA10X_DEFAULT_UCODE_BUILD_VERSION     0x0

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */


/*!
 * @brief selwrescrubRevokeHsBin_GA10X: 
 * Revoke scrubber if necessary, based on HW & SW fuse version, and chip ID in PMC_BOOT_42
 * 
 * @return SELWRESCRUB_RC_OK on success
 */
SELWRESCRUB_RC
selwrescrubRevokeHsBin_GA10X(void)
{
    LwU32           fuseVersionHW    = 0;
    LwU32           fpfFuseVersionHW = 0;
    LwU32           ucodeVersion     = 0;
    SELWRESCRUB_RC  status           = SELWRESCRUB_RC_OK;


    LwBool bSupportedChip   = selwrescrubIsChipSupported_HAL(pSelwrescrub);

    if (!bSupportedChip)
    {
        return SELWRESCRUB_RC_UNSUPPORTED_CHIP;
    }

    // Read the version number from FUSE block
    if (SELWRESCRUB_RC_OK != (status = selwrescrubGetHwFuseVersion_HAL(pSelwrescrub, &fuseVersionHW)))
    {
        return status;
    }

    // Read the version number from FPF FUSE block
    if (SELWRESCRUB_RC_OK != (status = selwrescrubGetHwFpfFuseVersion_HAL(pSelwrescrub, &fpfFuseVersionHW)))
    {
        return status;
    }

    // Get SW defined Ucode build version
    if (SELWRESCRUB_RC_OK != (status = selwrescrubGetSwUcodeVersion_HAL(pSelwrescrub, &ucodeVersion)))
    {
        return status;
    }

    if ((ucodeVersion < fuseVersionHW) || (ucodeVersion < fpfFuseVersionHW))
    {
        return SELWRESCRUB_RC_BIN_REVOKED;
    }

    return SELWRESCRUB_RC_OK;
}


/*
 * @brief selwrescrubIsChipSupported_GA10X:
 * Check if UCODE is running on valid chip
 *
 * @return LW_TRUE if Chip is supported
 */
LwBool
selwrescrubIsChipSupported_GA10X(void)
{
    LwBool bSupportedChip = LW_FALSE;
    LwU32  chipId         = FALC_REG_RD32(BAR0, LW_PMC_BOOT_42);

    // Fetch the chip ID
    chipId  = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, chipId);

    switch (chipId)
    {
        case LW_PMC_BOOT_42_CHIP_ID_GA102:
        case LW_PMC_BOOT_42_CHIP_ID_GA103:
        case LW_PMC_BOOT_42_CHIP_ID_GA104:
        case LW_PMC_BOOT_42_CHIP_ID_GA106:
        case LW_PMC_BOOT_42_CHIP_ID_GA107:
            bSupportedChip = LW_TRUE;
            break;
        default:
            bSupportedChip = LW_FALSE;            
    }

    return bSupportedChip;
}

/*
 * @brief selwrescrubGetSwUcodeVersion_GA10X:
 * Get SW ucode version, the define comes from selwrescrub-profiles.lwmk -> make-profile.lwmk
 *
 * @param [out] pVersionSw   Ucode Version of Scrubber
 *
 * @return SELWRESCRUB_RC_OK on success
 */
SELWRESCRUB_RC
selwrescrubGetSwUcodeVersion_GA10X(LwU32 *pVersionSw)
{
    LwU32 chip = DRF_VAL( _PMC, _BOOT_42, _CHIP_ID,
                        FALC_REG_RD32(BAR0, LW_PMC_BOOT_42));
    if (!pVersionSw)
    {
        return SELWRESCRUB_RC_ILWALID_POINTER;
    }

    //
    // Setup a ct_assert to warn in advance when we're close to running out #bits for scrubber version in the secure scratch
    //
    // WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
    // WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
    //
    // Never use "SELWRESCRUB_GA102_UCODE_BUILD_VERSION" directly, always use the function selwrescrubGetSwUcodeVersion() to get this
    // The define is used directly here in special case where we need compile time assert instead of fetching this at run time
    //
    CT_ASSERT(SELWRESCRUB_GA102_UCODE_BUILD_VERSION + 3 < DRF_MASK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_15_SCRUBBER_BINARY_VERSION));
    //
    // WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
    // WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
    //

    switch(chip)
    {
        case LW_PMC_BOOT_42_CHIP_ID_GA102:
        case LW_PMC_BOOT_42_CHIP_ID_GA103:
        case LW_PMC_BOOT_42_CHIP_ID_GA104:
        case LW_PMC_BOOT_42_CHIP_ID_GA106:
        case LW_PMC_BOOT_42_CHIP_ID_GA107:
            *pVersionSw = SELWRESCRUB_GA102_UCODE_BUILD_VERSION;
            break;
     
        default:
            *pVersionSw = SELWRESCRUB_GA10X_DEFAULT_UCODE_BUILD_VERSION;
            break;
    }
    return SELWRESCRUB_RC_OK;
}

/*
 * @brief selwrescrubGetHwFuseVersion_GA10X: 
 * Returns supported binary version blowned in fuses.
 *
 * @param [out] pHwFuseVersion   Binary Version blown in Fuses
 *
 * @return SELWRESCRUB_RC_OK on success
 */
SELWRESCRUB_RC
selwrescrubGetHwFuseVersion_GA10X(LwU32 *pHwFuseVersion)
{
    if (!pHwFuseVersion)
    {
        return SELWRESCRUB_RC_ILWALID_POINTER;
    }

    *pHwFuseVersion  = FALC_REG_RD32(BAR0, LW_FUSE_OPT_FUSE_UCODE_SCRUBBER_BIN_REV);

    return SELWRESCRUB_RC_OK;
}

/*
 * @brief selwrescrubGetHwFpfFuseVersion_GA10X: 
 * Returns supported binary version blown in fuses of FPF Block
 *
 * @param [out] pHwFpfFuseVersion   Binary Version value blown in Fuses of FPF Block
 *
 * @return SELWRESCRUB_RC_OK on success
 */
SELWRESCRUB_RC
selwrescrubGetHwFpfFuseVersion_GA10X(LwU32 *pHwFpfFuseVersion)
{
    LwU32 count        = 0;
    LwU32 fpfFuseValue = 0;

    if (!pHwFpfFuseVersion)
    {
        return SELWRESCRUB_RC_ILWALID_POINTER;
    }

    // Get the current FPF Fuse Version for Scrubber
    fpfFuseValue = FALC_REG_RD32(BAR0, LW_FUSE_OPT_FPF_UCODE_SCRUBBER_BIN_REV);
    fpfFuseValue = DRF_VAL(_FUSE, _OPT_FPF_UCODE_SCRUBBER_BIN_REV, _DATA, fpfFuseValue);

    /*
     * FPF Fuse and SW Ucode version has below binding
     * FPF FUSE      SW Ucode Version
     *   0              0
     *   1              1
     *   3              2
     *   7              3
     * and so on. 
     */
    while (fpfFuseValue != 0)
    {
        count++;
        fpfFuseValue >>= 1;
    }

    // Assign the final count value
    *pHwFpfFuseVersion = count;

    return SELWRESCRUB_RC_OK;
}

/*!
 * @brief Check whether Bootstrap and FWSEC have run successfully to ensure Chain of Trust.
 * See https://confluence.lwpu.com/display/GFWGA10X/GA10x%3A+CoT+Enforcement+v1.0 for more information.
 */
SELWRESCRUB_RC
selwrescrubCheckChainOfTrust_GA10X()
{
    LwU32 bootstrapCOTValue = DRF_VAL(_PGC6, _AON_SELWRE_SCRATCH_GROUP_40_GFW_COT_BOOTSTRAP, _PROG,
                                        FALC_REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_40_GFW_COT_BOOTSTRAP));
    LwU32 fwsecCOTValue     = DRF_VAL(_PGC6, _AON_SELWRE_SCRATCH_GROUP_41_GFW_COT_FWSEC, _PROG,
                                        FALC_REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_41_GFW_COT_FWSEC));

    if (!FLD_TEST_DRF(_PGC6, _AON_SELWRE_SCRATCH_GROUP_40, _GFW_COT_BOOTSTRAP_PROG, _COMPLETED, bootstrapCOTValue) ||
        !FLD_TEST_DRF(_PGC6, _AON_SELWRE_SCRATCH_GROUP_41, _GFW_COT_FWSEC_PROG,     _COMPLETED, fwsecCOTValue))
    {
        return SELWRESCRUB_RC_ERROR_GFW_CHAIN_OF_TRUST_BROKEN;
    }

    return SELWRESCRUB_RC_OK;
}

SELWRESCRUB_RC
selwrescrubGetUsableFbSizeInMb_GA10X(LwU64 *pUsableFbSizeMB)
{
    LwU32 data = 0;
    LwU32 plm  = 0;  

    if (pUsableFbSizeMB == NULL)
    {
        return SELWRESCRUB_RC_ILWALID_POINTER;
    }

    data = FALC_REG_RD32( BAR0, LW_USABLE_FB_SIZE_IN_MB);
    *pUsableFbSizeMB =(LwU64) DRF_VAL(_USABLE, _FB_SIZE_IN_MB, _VALUE, data);

    //ALL_LEVEL_DISABLED for write permissions of LW_PGC6_AON_SELWRE_SCRATCH_GROUP_42
    plm = FALC_REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_42_PRIV_LEVEL_MASK);
    plm = FLD_SET_DRF( _PGC6, _AON_SELWRE_SCRATCH_GROUP_42_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _ALL_LEVELS_DISABLED,plm );
    FALC_REG_WR32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_42_PRIV_LEVEL_MASK, plm);

    return SELWRESCRUB_RC_OK;
}


