/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: selwrescrubtu102only.c
 * This file is specific to chips TU102, TU104, TU106.
 */
/* ------------------------- System Includes -------------------------------- */
#include "selwrescrub.h"
#include "selwrescrubreg.h"
#include "dev_fuse.h"
#include "dev_master.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"
#include "config/g_selwrescrub_private.h"

/*
 * @brief: Get SW ucode version, the define comes from selwrescrub-profiles.lwmk -> make-profile.lwmk
 */
SELWRESCRUB_RC 
selwrescrubGetSwUcodeVersion_TU10X(LwU32 *pVersionSw)
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
    // Never use "SELWRESCRUB_TU10X_UCODE_BUILD_VERSION" directly, always use the function selwrescrubGetSwUcodeVersion() to get this
    // The define is used directly here in special case where we need compile time assert instead of fetching this at run time
    //
    CT_ASSERT(SELWRESCRUB_TU102_UCODE_BUILD_VERSION + 3 < DRF_MASK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_15_SCRUBBER_BINARY_VERSION));
    CT_ASSERT(SELWRESCRUB_TU104_UCODE_BUILD_VERSION + 3 < DRF_MASK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_15_SCRUBBER_BINARY_VERSION));
    CT_ASSERT(SELWRESCRUB_TU106_UCODE_BUILD_VERSION + 3 < DRF_MASK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_15_SCRUBBER_BINARY_VERSION));
    //
    // WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
    // WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
    //

    switch(chip)
    {
        case LW_PMC_BOOT_42_CHIP_ID_TU102:
            *pVersionSw = SELWRESCRUB_TU102_UCODE_BUILD_VERSION;
            break;
        case LW_PMC_BOOT_42_CHIP_ID_TU104:
            *pVersionSw = SELWRESCRUB_TU104_UCODE_BUILD_VERSION;
            break;
        case LW_PMC_BOOT_42_CHIP_ID_TU106:
            *pVersionSw = SELWRESCRUB_TU106_UCODE_BUILD_VERSION;
            break;
        default:
            return SELWRESCRUB_RC_NOT_SUPPORTED;
    }
    return SELWRESCRUB_RC_OK;
}
