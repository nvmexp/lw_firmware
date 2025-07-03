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
 * @file: booter_sanity_checks_ga100.c
 */
//
// Includes
//
#include "booter.h"

/*!
 * Verify is this build should be allowed to run on particular chip
 */
BOOTER_STATUS
booterCheckIfBuildIsSupported_GA100(void)
{
    LwU32 chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, BOOTER_REG_RD32(BAR0, LW_PMC_BOOT_42));

    if (chip == LW_PMC_BOOT_42_CHIP_ID_GA100)
    {
        return BOOTER_OK;
    }

    if (chip == LW_PMC_BOOT_42_CHIP_ID_GA101)
    {
        if (booterIsDebugModeEnabled_HAL(pBooter) == LW_FALSE)
        {
            return BOOTER_ERROR_PROD_MODE_NOT_SUPPORTED;
        }

        return BOOTER_OK;
    }
    return  BOOTER_ERROR_ILWALID_CHIP_ID;
}

/*!
 * @brief Get the ucode build version
 */
BOOTER_STATUS
booterGetUcodeBuildVersion_GA100(LwU32* pUcodeBuildVersion)
{
    if (!pUcodeBuildVersion)
    {
        return BOOTER_ERROR_ILWALID_ARGUMENT;
    }

    *pUcodeBuildVersion = BOOTER_GA100_UCODE_BUILD_VERSION;

    return BOOTER_OK;
}

