/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef _FUB_INLINE_STATIC_FUNCS_FPF_TU116_H_
#define _FUB_INLINE_STATIC_FUNCS_FPF_TU116_H_

/*!
 * @file: fub_inline_static_funcs_fpf_tu116.h
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_lwdec_csb.h"
#include "dev_sec_csb.h"
#include "dev_therm.h"
#if (FUBCFG_FEATURE_ENABLED(FUBCORE_TU10X) || FUBCFG_FEATURE_ENABLED(FUBCORE_TU116))
#include "dev_fpf.h"
#endif
#include "dev_therm_vmacro.h"
#include "dev_fuse.h"
#include "dev_gc6_island_addendum.h"
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

/*!
 * @brief Get SW ucode version, the define comes from fub-profiles.lwmk -> make-profile.lwmk
 *    
 * @param[in] pVersionSw pointer to parameter containg SW Version
 */

GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubGetSwUcodeVersion_TU116(LwU32 *pVersionSw)
{
    FUB_STATUS fubStatus = FUB_OK;
    LwU32      chip      = 0;

    if (!pVersionSw)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }

    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_PMC_BOOT_42, &chip));
    chip = DRF_VAL( _PMC, _BOOT_42, _CHIP_ID, chip);
    //
    // Added compile time assert to warn in advance when we are close to running out of #bits 
    // for FUB version. For fuse allocation refer BUG 200450823.  
    //
    CT_ASSERT(FUB_TU116_UCODE_BUILD_VERSION + 3 < DRF_MASK(LW_FPF_OPT_FIELD_SPARE_FUSES_1_DATA));
    CT_ASSERT(FUB_TU117_UCODE_BUILD_VERSION + 3 < DRF_MASK(LW_FPF_OPT_FIELD_SPARE_FUSES_1_DATA));
    switch(chip)
    {
        case LW_PMC_BOOT_42_CHIP_ID_TU116:
            *pVersionSw = FUB_TU116_UCODE_BUILD_VERSION;
            break;
        case LW_PMC_BOOT_42_CHIP_ID_TU117:
            *pVersionSw = FUB_TU117_UCODE_BUILD_VERSION;
            break;
        default:
            CHECK_FUB_STATUS(FUB_ERR_UNSUPPORTED_CHIP);
    }

ErrorExit: 
    return fubStatus;
}

#endif // _FUB_INLINE_STATIC_FUNCS_FPF_TU116_H_