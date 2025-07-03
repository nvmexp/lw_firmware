/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: booter_sanity_checks_tu116_only.c 
 */
#include "booter.h"

/*!
 * Verify is this build should be allowed to run on particular chip
 */
BOOTER_STATUS
booterCheckIfBuildIsSupported_TU116(void)
{
    LwU32 chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, BOOTER_REG_RD32(BAR0, LW_PMC_BOOT_42));

    if ((chip == LW_PMC_BOOT_42_CHIP_ID_TU116) ||
        (chip == LW_PMC_BOOT_42_CHIP_ID_TU117))
    {
        return BOOTER_OK;
    }
    else
    {
        return BOOTER_ERROR_ILWALID_CHIP_ID;
    }
}

#define BOOTER_TU11X_DEFAULT_UCODE_BUILD_VERSION       0x0    // For chips TU116, TU117
/*!
 * @brief Get the ucode build version
 */
BOOTER_STATUS
booterGetUcodeBuildVersion_TU116(LwU32* pUcodeBuildVersion)
{
    if (!pUcodeBuildVersion)
    {
        return BOOTER_ERROR_ILWALID_ARGUMENT;
    }

    //
    // Setup a ct_assert to warn in advance when we are close to running out of #bits for Booter version
    // Keeping a buffer of 2 version increments
    //
    ct_assert((BOOTER_TU116_UCODE_BUILD_VERSION         + 3) < DRF_MASK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_BOOTER_BINARY_VERSION));
    ct_assert((BOOTER_TU117_UCODE_BUILD_VERSION         + 3) < DRF_MASK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_BOOTER_BINARY_VERSION));
    ct_assert((BOOTER_TU11X_DEFAULT_UCODE_BUILD_VERSION + 3) < DRF_MASK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_BOOTER_BINARY_VERSION));

    LwU32 chip = DRF_VAL( _PMC, _BOOT_42, _CHIP_ID, BOOTER_REG_RD32(BAR0, LW_PMC_BOOT_42));

    if (chip == LW_PMC_BOOT_42_CHIP_ID_TU116)
    {
        *pUcodeBuildVersion = BOOTER_TU116_UCODE_BUILD_VERSION;
    }
    else if (chip == LW_PMC_BOOT_42_CHIP_ID_TU117)
    {
        *pUcodeBuildVersion = BOOTER_TU117_UCODE_BUILD_VERSION;
    }
    else
    {
        *pUcodeBuildVersion = BOOTER_TU11X_DEFAULT_UCODE_BUILD_VERSION;
    }

    return BOOTER_OK;
}
