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
 * @file: booter_sanity_checks_ga10x.c
 * This file is for chips GA102 and above
 */
//
// Includes
//
#include "booter.h"

/*!
 * Verify is this build should be allowed to run on particular chip
 */
BOOTER_STATUS
booterCheckIfBuildIsSupported_GA10X(void)
{
    LwU32 chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, BOOTER_REG_RD32(BAR0, LW_PMC_BOOT_42));

   if ((chip == LW_PMC_BOOT_42_CHIP_ID_GA102)     ||
       (chip == LW_PMC_BOOT_42_CHIP_ID_GA103)     ||
       (chip == LW_PMC_BOOT_42_CHIP_ID_GA104)     ||
       (chip == LW_PMC_BOOT_42_CHIP_ID_GA106)     ||
       (chip == LW_PMC_BOOT_42_CHIP_ID_GA107))
   {
       return BOOTER_OK;
   }
   else
   {
       LwU32 platformType = DRF_VAL(_PTOP, _PLATFORM, _TYPE, BOOTER_REG_RD32(BAR0, LW_PTOP_PLATFORM));
       if (platformType != LW_PTOP_PLATFORM_TYPE_SILICON)
       {
           if (booterIsDebugModeEnabled_HAL(pBooter) == LW_FALSE)
           {
               return BOOTER_ERROR_PROD_MODE_NOT_SUPPORTED;
           }

           return BOOTER_OK;
       }
   }

   return BOOTER_ERROR_ILWALID_CHIP_ID;
}

#define BOOTER_GA10X_DEFAULT_UCODE_BUILD_VERSION   0x0   // Only for Chips GA102,

/*!
 * @brief Get the ucode build version
 */
BOOTER_STATUS
booterGetUcodeBuildVersion_GA10X(LwU32* pUcodeBuildVersion)
{
    if (!pUcodeBuildVersion)
    {
        return BOOTER_ERROR_ILWALID_ARGUMENT;
    }

    LwU32 chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, BOOTER_REG_RD32(BAR0, LW_PMC_BOOT_42));

    switch (chip)
    {
        case LW_PMC_BOOT_42_CHIP_ID_GA102 :
        case LW_PMC_BOOT_42_CHIP_ID_GA103 :
        case LW_PMC_BOOT_42_CHIP_ID_GA104 :
        case LW_PMC_BOOT_42_CHIP_ID_GA106 :
        case LW_PMC_BOOT_42_CHIP_ID_GA107 :
            *pUcodeBuildVersion = BOOTER_GA102_UCODE_BUILD_VERSION;
            break;
        // TODO Return invalid chip before prod signing
        default :
            *pUcodeBuildVersion = BOOTER_GA10X_DEFAULT_UCODE_BUILD_VERSION;
            break;
    }

    return BOOTER_OK;
}

/*!
 * @brief Check whether Bootstrap and FWSEC have run successfully to ensure Chain of Trust.
 * See https://confluence.lwpu.com/display/GFWGA10X/GA10x%3A+CoT+Enforcement+v1.0 for more information.
 */
BOOTER_STATUS
booterCheckChainOfTrust_GA10X()
{
    LwU32 bootstrapCOTValue = DRF_VAL(_PGC6, _AON_SELWRE_SCRATCH_GROUP_40_GFW_COT_BOOTSTRAP, _PROG,
                                        BOOTER_REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_40_GFW_COT_BOOTSTRAP));
    LwU32 fwsecCOTValue     = DRF_VAL(_PGC6, _AON_SELWRE_SCRATCH_GROUP_41_GFW_COT_FWSEC, _PROG,
                                        BOOTER_REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_41_GFW_COT_FWSEC));

    if (!FLD_TEST_DRF(_PGC6, _AON_SELWRE_SCRATCH_GROUP_40, _GFW_COT_BOOTSTRAP_PROG, _COMPLETED, bootstrapCOTValue) ||
        !FLD_TEST_DRF(_PGC6, _AON_SELWRE_SCRATCH_GROUP_41, _GFW_COT_FWSEC_PROG,     _COMPLETED, fwsecCOTValue))
    {
        return BOOTER_ERROR_GFW_CHAIN_OF_TRUST_BROKEN;
    }

    return BOOTER_OK;
}

