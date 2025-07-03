/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: acr_sanity_checks_tu116_only.c 
 */
#include "acr.h"
#include "acrdrfmacros.h"
#include "acr_objacr.h"

#include "mmu/mmucmn.h"

#include "dev_master.h"

// For decode traps for protecting LS falcon setup

#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"

#include "config/g_acr_private.h"
#include "config/g_acrlib_private.h"

#if defined(AHESASC) || defined(ASB) || defined(BSI_LOCK) 
#include "dev_sec_csb.h"
#endif

/*!
 * Verify is this build should be allowed to run on particular chip
 */
ACR_STATUS
acrCheckIfBuildIsSupported_TU116(void)
{
    LwU32 chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, ACR_REG_RD32(BAR0, LW_PMC_BOOT_42));

    if ((chip == LW_PMC_BOOT_42_CHIP_ID_TU116) ||
        (chip == LW_PMC_BOOT_42_CHIP_ID_TU117))
    {
        return ACR_OK;
    }
    else
    {
        return ACR_ERROR_ILWALID_CHIP_ID;
    }
}

#define ACR_TU11X_DEFAULT_UCODE_BUILD_VERSION       0x0    // For chips TU116, TU117
/*!
 * @brief Get the ucode build version
 */
ACR_STATUS
acrGetUcodeBuildVersion_TU116(LwU32* pUcodeBuildVersion)
{
    if (!pUcodeBuildVersion)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    //
    // Setup a ct_assert to warn in advance when we are close to running out of #bits for ACR version
    // Keeping a buffer of 2 version increments
    //
    ct_assert((ACR_TU116_UCODE_BUILD_VERSION         + 3) < DRF_MASK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_ACR_BINARY_VERSION));
    ct_assert((ACR_TU117_UCODE_BUILD_VERSION         + 3) < DRF_MASK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_ACR_BINARY_VERSION));
    ct_assert((ACR_TU11X_DEFAULT_UCODE_BUILD_VERSION + 3) < DRF_MASK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_ACR_BINARY_VERSION));

    LwU32 chip = DRF_VAL( _PMC, _BOOT_42, _CHIP_ID, ACR_REG_RD32(BAR0, LW_PMC_BOOT_42));

    if (chip == LW_PMC_BOOT_42_CHIP_ID_TU116)
    {
        *pUcodeBuildVersion = ACR_TU116_UCODE_BUILD_VERSION;
    }
    else if (chip == LW_PMC_BOOT_42_CHIP_ID_TU117)
    {
        *pUcodeBuildVersion = ACR_TU117_UCODE_BUILD_VERSION;
    }
    else
    {
        *pUcodeBuildVersion = ACR_TU11X_DEFAULT_UCODE_BUILD_VERSION;
    }

    return ACR_OK;
}
