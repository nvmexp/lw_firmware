/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef _FUB_INLINE_STATIC_FUNCS_TU116_H_
#define _FUB_INLINE_STATIC_FUNCS_TU116_H_

/*!
 * @file: fub_inline_static_funcs_tu116.h
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_lwdec_csb.h"
#include "dev_sec_csb.h"
#include "dev_therm.h"
//#include "dev_fpf.h"
#include "dev_therm_vmacro.h"
#include "dev_fuse.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

/*
 * @brief: Check if UCODE is running on valid chip
 *
 */

GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubIsChipSupported_TU116(void)
{
    FUB_STATUS  fubStatus = FUB_ERR_UNSUPPORTED_CHIP;
    LwU32       chipId    = 0;

    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_PMC_BOOT_42, &chipId));
    chipId = DRF_VAL( _PMC, _BOOT_42, _CHIP_ID, chipId);

    switch (chipId)
    {
        case LW_PMC_BOOT_42_CHIP_ID_TU116:
        case LW_PMC_BOOT_42_CHIP_ID_TU117:
            fubStatus = FUB_OK;
        break;
        default:
            fubStatus = FUB_ERR_UNSUPPORTED_CHIP;
    }

ErrorExit: 
    return fubStatus;
}

#endif // _FUB_INLINE_STATIC_FUNCS_TU116_H_