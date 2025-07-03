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
 * @file: acr_sanity_checks_gh20x.c
 * This file is for chips GH202 and above
 */
//
// Includes
//
#include "acr.h"

#include "acr_objacr.h"
#include "acr_objacrlib.h"

#include "acrdrfmacros.h"
#include "dev_master.h"
#include "config/g_acr_private.h"

/*!
 * Verify is this build should be allowed to run on particular chip
 */
ACR_STATUS
acrCheckIfBuildIsSupported_GH20X(void)
{
    LwU32 chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, ACR_REG_RD32(BAR0, LW_PMC_BOOT_42));

   if ((chip == LW_PMC_BOOT_42_CHIP_ID_GH202)     ||
       (chip == LW_PMC_BOOT_42_CHIP_ID_G000_SOC)  ||
       (chip == LW_PMC_BOOT_42_CHIP_ID_G000))
   {
       if (acrIsDebugModeEnabled_HAL(pAcr) == LW_FALSE)
       {
           return ACR_ERROR_PROD_MODE_NOT_SUPPORTED;
       }

       return ACR_OK;
   }
    return ACR_ERROR_ILWALID_CHIP_ID;
}

#define ACR_GH20X_DEFAULT_UCODE_BUILD_VERSION   0x0   // Only for Chips GH20X,

/*!
 * @brief Get the ucode build version
 */
ACR_STATUS
acrGetUcodeBuildVersion_GH20X(LwU32* pUcodeBuildVersion)
{
    if (!pUcodeBuildVersion)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    LwU32 chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, ACR_REG_RD32(BAR0, LW_PMC_BOOT_42));

    switch (chip)
    {
        case LW_PMC_BOOT_42_CHIP_ID_GH202 :
            *pUcodeBuildVersion = ACR_GH202_UCODE_BUILD_VERSION;
            break;
        // TODO Return invalid chip before prod signing
        default :
            *pUcodeBuildVersion = ACR_GH20X_DEFAULT_UCODE_BUILD_VERSION;
            break;
    }

    return ACR_OK;
}


