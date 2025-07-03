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
 * @file: acr_sanity_checks_gh100.c
 */
//
// Includes
//
#include "acr.h"

#include "acr_objacr.h"
#include "acr_objacrlib.h"

#include "dev_master.h"

#include "config/g_acr_private.h"

//
// TODO @srathod/@adjoshi: Temporary define to enable GB100 - to be removed once
// the binaries are forked off.
//
#define LW_PMC_BOOT_42_CHIP_ID_GB100                     0x000001A0
#define LW_PMC_BOOT_42_CHIP_ID_GB102                     0x000001A2

/*!
 * Verify is this build should be allowed to run on particular chip
 */
ACR_STATUS
acrCheckIfBuildIsSupported_GH100(void)
{
    LwU32 chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, ACR_REG_RD32(BAR0, LW_PMC_BOOT_42));

    if ((chip == LW_PMC_BOOT_42_CHIP_ID_GH100) ||
            (chip == LW_PMC_BOOT_42_CHIP_ID_GB100) ||
            (chip == LW_PMC_BOOT_42_CHIP_ID_GB102))
    {
        return ACR_OK;
    }

    return  ACR_ERROR_ILWALID_CHIP_ID;
}

/*!
 * @brief Get the ucode build version
 */
ACR_STATUS
acrGetUcodeBuildVersion_GH100(LwU32* pUcodeBuildVersion)
{
    if (!pUcodeBuildVersion)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    *pUcodeBuildVersion = ACR_GH100_UCODE_BUILD_VERSION;

    return ACR_OK;
}

/*!
 * In GH100, SE TRNG is being removed (ref. bug 3078167).
 * Thus, stub the function on the Hopper and later chips
 * to return ACR_OK unconditionally.
 */
ACR_STATUS
seAcquireMutex_GH100(void)
{
    return ACR_OK;
}

/*!
 * In GH100, SE TRNG is being removed (ref. bug 3078167).
 * Thus, stub the function on the Hopper and later chips
 * to return ACR_OK unconditionally.
 */
ACR_STATUS
seReleaseMutex_GH100(void)
{
    return ACR_OK;
}
