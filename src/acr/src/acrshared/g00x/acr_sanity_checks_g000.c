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
 * @file: acr_sanity_checks_g000.c
 */
//
// Includes
//
#include "acr.h"
#include "acr_objacr.h"
#include "acr_objacrlib.h"
#include "dev_master.h"
#include "config/g_acr_private.h"


/*!
 * Verify is this build should be allowed to run on particular chip
 */
ACR_STATUS
acrCheckIfBuildIsSupported_G000(void)
{
    LwU32 chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, ACR_REG_RD32(BAR0, LW_PMC_BOOT_42));
    
    if ((chip == LW_PMC_BOOT_42_CHIP_ID_G000)    ||
       (chip == LW_PMC_BOOT_42_CHIP_ID_G000_SOC))
    {
        return ACR_OK;
    }

    return  ACR_ERROR_ILWALID_CHIP_ID;
}

/*!
 * @brief Get the ucode build version
 */
ACR_STATUS
acrGetUcodeBuildVersion_G000(LwU32* pUcodeBuildVersion)
{
    if (!pUcodeBuildVersion)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    *pUcodeBuildVersion = ACR_G000_UCODE_BUILD_VERSION;

    return ACR_OK;
}

