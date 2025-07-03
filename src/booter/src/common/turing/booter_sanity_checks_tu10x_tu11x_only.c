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
 * @file: booter_sanity_checks_tu10x_tu11x_only.c
 */

//
// Includes
//
#include "booter.h"
#include "dev_fpf.h"
#include "dev_fpf_addendum.h"

/*!
 * @brief Get the HW fuse version
 */
BOOTER_STATUS
booterGetUcodeFpfFuseVersion_TU10X(LwU32* pUcodeFpfFuseVersion)
{
    LwU32 count = 0;
    LwU32 fpfFuse = BOOTER_REG_RD32(BAR0, LW_FPF_OPT_FUSE_UCODE_BOOTER_HS_REV);

    fpfFuse = DRF_VAL( _FPF, _OPT_FUSE_UCODE_BOOTER_HS_REV, _DATA, fpfFuse);

    /*
     * FPF Fuse and SW Ucode version has below binding
     * FPF FUSE      SW Ucode Version
     *   0              0
     *   1              1
     *   3              2
     *   7              3
     * and so on. */
    while (fpfFuse != 0)
    {
        count++;
        fpfFuse >>= 1;
    }

    *pUcodeFpfFuseVersion = count;

    return BOOTER_OK;
}
