/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_sanity_checks_ad10x.c
 * This file is for chips AD102 and above
 */

/* ------------------------ System includes -------------------------------- */
#include "acr.h"
#include "acr_objacr.h"
#include "acr_objacrlib.h"

#include "dev_top.h"
#include "dev_master.h"

/*!
 * Verify is this build should be allowed to run on particular chip
 */
ACR_STATUS
acrCheckIfBuildIsSupported_AD10X(void)
{
    LwU32 chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, ACR_REG_RD32(BAR0, LW_PMC_BOOT_42));

   if (chip == LW_PMC_BOOT_42_CHIP_ID_AD102)
   {
       return ACR_OK;
   }
   else
   {
       LwU32 platformType = DRF_VAL(_PTOP, _PLATFORM, _TYPE, ACR_REG_RD32(BAR0, LW_PTOP_PLATFORM));
       if (platformType != LW_PTOP_PLATFORM_TYPE_SILICON)
       {
           if (acrIsDebugModeEnabled_HAL(pAcr) == LW_FALSE)
           {
               return ACR_ERROR_PROD_MODE_NOT_SUPPORTED;
           }

           return ACR_OK;
       }
   }

   return ACR_ERROR_ILWALID_CHIP_ID;
}

/*!
 * @brief Check whether Bootstrap and FWSEC have run successfully to ensure Chain of Trust.
 * See https://confluence.lwpu.com/display/GFWGA10X/GA10x%3A+CoT+Enforcement+v1.0 for more information.
 */
ACR_STATUS
acrCheckChainOfTrust_AD10X()
{
    return ACR_OK;

// TODO: Uncomment below code once COT is enabled from vbios.
#if 0
    LwU32 bootstrapCOTValue = DRF_VAL(_PGC6, _AON_SELWRE_SCRATCH_GROUP_40_GFW_COT_BOOTSTRAP, _PROG,
                                        ACR_REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_40_GFW_COT_BOOTSTRAP));
    LwU32 fwsecCOTValue     = DRF_VAL(_PGC6, _AON_SELWRE_SCRATCH_GROUP_41_GFW_COT_FWSEC, _PROG,
                                        ACR_REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_41_GFW_COT_FWSEC));

    if (!FLD_TEST_DRF(_PGC6, _AON_SELWRE_SCRATCH_GROUP_40, _GFW_COT_BOOTSTRAP_PROG, _COMPLETED, bootstrapCOTValue) ||
        !FLD_TEST_DRF(_PGC6, _AON_SELWRE_SCRATCH_GROUP_41, _GFW_COT_FWSEC_PROG,     _COMPLETED, fwsecCOTValue))
    {
        return ACR_ERROR_GFW_CHAIN_OF_TRUST_BROKEN;
    }

    return ACR_OK;
#endif
}

#define ACR_AD10X_DEFAULT_UCODE_BUILD_VERSION   0x0   // Only for Chips AD102,

/*!
 * @brief Get the ucode build version
 */
ACR_STATUS
acrGetUcodeBuildVersion_AD10X(LwU32* pUcodeBuildVersion)
{
    if (!pUcodeBuildVersion)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    LwU32 chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, ACR_REG_RD32(BAR0, LW_PMC_BOOT_42));

    switch (chip)
    {
        case LW_PMC_BOOT_42_CHIP_ID_AD102 :
        case LW_PMC_BOOT_42_CHIP_ID_AD103 :
        case LW_PMC_BOOT_42_CHIP_ID_AD104 :
        case LW_PMC_BOOT_42_CHIP_ID_AD106 :
        case LW_PMC_BOOT_42_CHIP_ID_AD107 :
            *pUcodeBuildVersion = ACR_AD102_UCODE_BUILD_VERSION;
            break;
        // TODO Return invalid chip before prod signing
        default :
            *pUcodeBuildVersion = ACR_AD10X_DEFAULT_UCODE_BUILD_VERSION;
            break;
    }

    return ACR_OK;
}

