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
 * @file: booter_sanity_checks_tu10x_only.c
 */
//
// Includes
//
#include "booter.h"

/*!
 * Verify is this build should be allowed to run on particular chip
 */
BOOTER_STATUS
booterCheckIfBuildIsSupported_TU10X(void)
{
    LwU32 chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, BOOTER_REG_RD32(BAR0, LW_PMC_BOOT_42));

    if ((chip == LW_PMC_BOOT_42_CHIP_ID_TU102) ||
        (chip == LW_PMC_BOOT_42_CHIP_ID_TU104) ||
        (chip == LW_PMC_BOOT_42_CHIP_ID_TU106))
    {
        return BOOTER_OK;
    }
    else
    {
        return BOOTER_ERROR_ILWALID_CHIP_ID;
    }
}

#define BOOTER_TU10X_DEFAULT_UCODE_BUILD_VERSION       0x0     // Only for chips TU102, TU104, TU106
/*!
 * @brief Get the ucode build version
 */
BOOTER_STATUS
booterGetUcodeBuildVersion_TU10X(LwU32* pUcodeBuildVersion)
{
    if (!pUcodeBuildVersion)
    {
        return BOOTER_ERROR_ILWALID_ARGUMENT;
    }

    //
    // Setup a ct_assert to warn in advance when we are close to running out of #bits for Booter version
    // Keeping a buffer of 2 version increments
    //
    ct_assert((BOOTER_TU102_UCODE_BUILD_VERSION         + 3) < DRF_MASK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_BOOTER_BINARY_VERSION));
    ct_assert((BOOTER_TU104_UCODE_BUILD_VERSION         + 3) < DRF_MASK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_BOOTER_BINARY_VERSION));
    ct_assert((BOOTER_TU106_UCODE_BUILD_VERSION         + 3) < DRF_MASK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_BOOTER_BINARY_VERSION));
    ct_assert((BOOTER_TU10X_DEFAULT_UCODE_BUILD_VERSION + 3) < DRF_MASK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_BOOTER_BINARY_VERSION));

    LwU32 chip = DRF_VAL( _PMC, _BOOT_42, _CHIP_ID, BOOTER_REG_RD32(BAR0, LW_PMC_BOOT_42));

    if (chip == LW_PMC_BOOT_42_CHIP_ID_TU102)
    {
        *pUcodeBuildVersion = BOOTER_TU102_UCODE_BUILD_VERSION;
    }
    else if (chip == LW_PMC_BOOT_42_CHIP_ID_TU104)
    {
        *pUcodeBuildVersion = BOOTER_TU104_UCODE_BUILD_VERSION;
    }
    else if (chip == LW_PMC_BOOT_42_CHIP_ID_TU106)
    {
        *pUcodeBuildVersion = BOOTER_TU106_UCODE_BUILD_VERSION;
    }
    else
    {
        *pUcodeBuildVersion = BOOTER_TU10X_DEFAULT_UCODE_BUILD_VERSION;
    }

    return BOOTER_OK;
}
