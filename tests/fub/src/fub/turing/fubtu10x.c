/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017 - 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: fubtu10x.c
 */

/* ------------------------- System Includes -------------------------------- */
#include "fub.h"
#include "dev_lwdec_csb.h"
#include "dev_sec_csb.h"
#include "dev_therm.h"
#include "dev_fpf.h"
#include "dev_master.h"
#include "dev_therm_vmacro.h"
#include "dev_fuse.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"
#include "fub.h"
#include <falcon-intrinsics.h>
#include "config/g_fub_private.h"
#include "config/fub-config.h"
#include "fubreg.h"
#include "lwRmReg.h"
#include "fub_inline_static_funcs_tu10x.h"
#if (FUBCFG_FEATURE_ENABLED(FUBCORE_TU10X) || FUBCFG_FEATURE_ENABLED(FUBCORE_TU116))
#include "fub_inline_static_funcs_fpf_tu10x.h"
#include "fub_inline_static_funcs_fpf_tu116.h"
#include "fub_inline_static_funcs_tu116.h"
#endif
#if (FUBCFG_FEATURE_ENABLED(FUBCORE_GA10X))
#include "fub_inline_static_funcs_ga100.h"
#endif
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
extern RM_FLCN_FUB_DESC  g_desc;
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief Create structure with all the information required for fuse burning
 *        FUB will burn fuse based on Devid.
 */
FUB_STATUS
fubFillFuseInformationStructure_TU10X(FPF_FUSE_INFO *pFuseInfo, LwU32 selectUseCase)
{
    LwU32      fuseBurlwalue = 0;
    FUB_STATUS fubStatus     = FUB_OK;

    // Return early if pointer passed is NULL
    if (pFuseInfo == NULL)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }

    // For details of fuse allocation and values to be burnt in each use case refer BUG 200450823
    switch (selectUseCase)
    {
#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_DEVID_LICENSE_REVOCATION_FUSE))
        case LW_FUB_USE_CASE_MASK_REVOKE_DEVID_BASED_HULK: // Devid based license Revocation

            // Bit 1 from LW_FPF_OPT_FIELD_SPARE_FUSES_0 will be burnt to 0x1 for this use case
            pFuseInfo->fuseBar0Address = LW_FPF_OPT_FIELD_SPARE_FUSES_0;

            // Check to make sure Fuse burn value is as expected
            ct_assert(LW_FPF_OPT_FIELD_SPARE_FUSE_0_REVOKE_DEVID_HULK_LICENSE_REVOKE_VALUE == BIT(1));

            pFuseInfo->fuseIntendedVal = LW_FPF_OPT_FIELD_SPARE_FUSE_0_REVOKE_DEVID_HULK_LICENSE_REVOKE_VALUE;

            pFuseInfo->fusePriAddress  = LW_FPF_OPT_FIELD_SPARE_FUSES_0__PRI_ALIAS_0;
            pFuseInfo->fuseRedAddress  = LW_FPF_OPT_FIELD_SPARE_FUSES_0__RED_ALIAS_0;

            // Zero out already Burnt Fuse bits. So FUB does not end up burning already burnt fuse.
            CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, pFuseInfo->fuseBar0Address, &fuseBurlwalue));
            fuseBurlwalue = (pFuseInfo->fuseIntendedVal) & (~fuseBurlwalue);
            // Not all fuses start at 0th bit of row. So adjusting fuse value as per location of fuse in row.
            pFuseInfo->fuseAdjustedBurlwalue = DRF_NUM(_FPF, _OPT_FIELD_SPARE_FUSES_0, __PRI_ALIAS_0_DATA , fuseBurlwalue);
            break;
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_GSYNC_ENABLING_FUSE))
        case LW_FUB_USE_CASE_MASK_ENABLE_GSYNC:    // Denote that a particular unit is shipping with G-Sync panel.

            // Bit 0 from LW_FPF_OPT_FIELD_SPARE_FUSES_2 will be burnt to 0x1 for this use case
            pFuseInfo->fuseBar0Address = LW_FPF_OPT_FIELD_SPARE_FUSES_2;

            // Check to make sure Fuse burn value is as expected
            ct_assert(LW_FPF_OPT_FIELD_SPARE_FUSE_2_ENABLE_GSYNC_VALUE == BIT(0));

            pFuseInfo->fuseIntendedVal = LW_FPF_OPT_FIELD_SPARE_FUSE_2_ENABLE_GSYNC_VALUE;

            pFuseInfo->fusePriAddress  = LW_FPF_OPT_FIELD_SPARE_FUSES_2__PRI_ALIAS_0;
            pFuseInfo->fuseRedAddress  = LW_FPF_OPT_FIELD_SPARE_FUSES_2__RED_ALIAS_0;

            // Zero out already Burnt Fuse bits.
            CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, pFuseInfo->fuseBar0Address, &fuseBurlwalue));
            fuseBurlwalue = (pFuseInfo->fuseIntendedVal) & (~fuseBurlwalue);
            // Not all fuses start at 0th bit of row. So adjusting fuse value as per location of fuse in row.
            pFuseInfo->fuseAdjustedBurlwalue = DRF_NUM(_FPF, _OPT_FIELD_SPARE_FUSES_2, __PRI_ALIAS_0_DATA , fuseBurlwalue);
            break;
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_ALLOW_DRAM_CHIPID_READ_HULK_LICENSE))
        case LW_FUB_USE_CASE_MASK_ALLOW_DRAM_CHIPID_READ:    // Burn fuse to enable Hulk license to read DRAM CHIPID.

            // Bit 0 from LW_FPF_OPT_FIELD_SPARE_FUSES_3 will be burnt to 0x1 for this use case
            pFuseInfo->fuseBar0Address = LW_FPF_OPT_FIELD_SPARE_FUSES_3;

            // Check to make sure Fuse burn value is as expected
            ct_assert(LW_FPF_OPT_FIELD_SPARE_FUSE_3_ALLOW_HULK_DRAM_CHIPID_READ == BIT(0));

            pFuseInfo->fuseIntendedVal = LW_FPF_OPT_FIELD_SPARE_FUSE_3_ALLOW_HULK_DRAM_CHIPID_READ;

            pFuseInfo->fusePriAddress  = LW_FPF_OPT_FIELD_SPARE_FUSES_3__PRI_ALIAS_0;
            pFuseInfo->fuseRedAddress  = LW_FPF_OPT_FIELD_SPARE_FUSES_3__RED_ALIAS_0;

            // Zero out already Burnt Fuse bits.
            CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, pFuseInfo->fuseBar0Address, &fuseBurlwalue));
            fuseBurlwalue = (pFuseInfo->fuseIntendedVal) & (~fuseBurlwalue);
            // Not all fuses start at 0th bit of row. So adjusting fuse value as per location of fuse in row.
            pFuseInfo->fuseAdjustedBurlwalue = DRF_NUM(_FPF, _OPT_FIELD_SPARE_FUSES_3, __PRI_ALIAS_0_DATA , fuseBurlwalue);
            break;
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_REVOKE_DRAM_CHIPID_READ_HULK_LICENSE))
        case LW_FUB_USE_CASE_MASK_BLOCK_DRAM_CHIPID_READ:    // Burn fuse to block Hulk license to read DRAM CHIPID.

            // Bit 1 from LW_FPF_OPT_FIELD_SPARE_FUSES_3 will be burnt to 0x1 for this use case
            pFuseInfo->fuseBar0Address = LW_FPF_OPT_FIELD_SPARE_FUSES_3;

            // Check to make sure Fuse burn value is as expected
            ct_assert(LW_FPF_OPT_FIELD_SPARE_FUSE_3_REVOKE_HULK_DRAM_CHIPID_READ == BIT(1));

            pFuseInfo->fuseIntendedVal = LW_FPF_OPT_FIELD_SPARE_FUSE_3_REVOKE_HULK_DRAM_CHIPID_READ;

            pFuseInfo->fusePriAddress  = LW_FPF_OPT_FIELD_SPARE_FUSES_3__PRI_ALIAS_0;
            pFuseInfo->fuseRedAddress  = LW_FPF_OPT_FIELD_SPARE_FUSES_3__RED_ALIAS_0;

            // Zero out already Burnt Fuse bits.
            CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, pFuseInfo->fuseBar0Address, &fuseBurlwalue));
            fuseBurlwalue = (pFuseInfo->fuseIntendedVal) & (~fuseBurlwalue);
            // Not all fuses start at 0th bit of row. So adjusting fuse value as per location of fuse in row.
            pFuseInfo->fuseAdjustedBurlwalue = DRF_NUM(_FPF, _OPT_FIELD_SPARE_FUSES_3, __PRI_ALIAS_0_DATA , fuseBurlwalue);
            break;
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_GEFORCE_SKU_RECOVERY))
        case LW_FUB_USE_CASE_MASK_GEFORCE_SKU_RECOVERY:     // Burn fuse for VdChip SKU recovery.

            // Bit 4 from LW_FPF_OPT_FIELD_SPARE_FUSES_3 will be burnt to 0x1 for this use case
            pFuseInfo->fuseBar0Address = LW_FPF_OPT_FIELD_SPARE_FUSES_3;

            // Check to make sure Fuse burn value is as expected
            ct_assert(LW_FPF_OPT_FIELD_SPARE_FUSE_3_GEFORCE_SKU_RECOVERY == BIT(4));

            pFuseInfo->fuseIntendedVal = LW_FPF_OPT_FIELD_SPARE_FUSE_3_GEFORCE_SKU_RECOVERY;

            pFuseInfo->fusePriAddress  = LW_FPF_OPT_FIELD_SPARE_FUSES_3__PRI_ALIAS_0;
            pFuseInfo->fuseRedAddress  = LW_FPF_OPT_FIELD_SPARE_FUSES_3__RED_ALIAS_0;

            // Zero out already Burnt Fuse bits.
            CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, pFuseInfo->fuseBar0Address, &fuseBurlwalue));
            fuseBurlwalue = (pFuseInfo->fuseIntendedVal) & (~fuseBurlwalue);
            // Not all fuses start at 0th bit of row. So adjusting fuse value as per location of fuse in row.
            pFuseInfo->fuseAdjustedBurlwalue = DRF_NUM(_FPF, _OPT_FIELD_SPARE_FUSES_3, __PRI_ALIAS_0_DATA , fuseBurlwalue);
            break;
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_GEFORCE_SKU_RECOVERY_GFW_REV))
        case LW_FUB_USE_CASE_MASK_GEFORCE_SKU_RECOVERY_GFW_REV:     // Burn fuse for VdChip SKU recovery GFW_REV.

            // Bit 2 from LW_FPF_OPT_FUSE_UCODE_GFW_REV  will be burnt to 0x1 for this use case
            pFuseInfo->fuseBar0Address = LW_FPF_OPT_FUSE_UCODE_GFW_REV;

            // Check to make sure Fuse burn value is as expected
            ct_assert(LW_FPF_OPT_FUSE_UCODE_GFW_REV_GEFORCE_SKU_RECOVERY == BIT(2));

            pFuseInfo->fuseIntendedVal = LW_FPF_OPT_FUSE_UCODE_GFW_REV_GEFORCE_SKU_RECOVERY;

            pFuseInfo->fusePriAddress  = LW_FPF_OPT_FUSE_UCODE_GFW_REV__PRI_ALIAS_0;
            pFuseInfo->fuseRedAddress  = LW_FPF_OPT_FUSE_UCODE_GFW_REV__RED_ALIAS_0;

            // Zero out already Burnt Fuse bits.
            CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, pFuseInfo->fuseBar0Address, &fuseBurlwalue));
            fuseBurlwalue = (pFuseInfo->fuseIntendedVal) & (~fuseBurlwalue);
            // Not all fuses start at 0th bit of row. So adjusting fuse value as per location of fuse in row.
            pFuseInfo->fuseAdjustedBurlwalue = DRF_NUM(_FPF, _OPT_FUSE_UCODE_GFW_REV, __PRI_ALIAS_0_DATA , fuseBurlwalue);
            break;
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_GFW_REV_FOR_WP_BYPASS))
        case LW_FUB_USE_CASE_MASK_GFW_REV_FOR_WP_BYPASS:     // Burn fuse for VdChip SKU recovery GFW_REV.

            // Bit 3 from LW_FPF_OPT_FUSE_UCODE_GFW_REV  will be burnt to 0x1 for this use case
            pFuseInfo->fuseBar0Address = LW_FPF_OPT_FUSE_UCODE_GFW_REV;

            // Check to make sure Fuse burn value is as expected
            ct_assert(LW_FPF_OPT_FUSE_UCODE_GFW_REV_FOR_WP_BYPASS_BUG_2623776 == BIT(3));

            pFuseInfo->fuseIntendedVal = LW_FPF_OPT_FUSE_UCODE_GFW_REV_FOR_WP_BYPASS_BUG_2623776;

            pFuseInfo->fusePriAddress  = LW_FPF_OPT_FUSE_UCODE_GFW_REV__PRI_ALIAS_0;
            pFuseInfo->fuseRedAddress  = LW_FPF_OPT_FUSE_UCODE_GFW_REV__RED_ALIAS_0;

            // Zero out already Burnt Fuse bits.
            CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, pFuseInfo->fuseBar0Address, &fuseBurlwalue));
            fuseBurlwalue = (pFuseInfo->fuseIntendedVal) & (~fuseBurlwalue);
            // Not all fuses start at 0th bit of row. So adjusting fuse value as per location of fuse in row.
            pFuseInfo->fuseAdjustedBurlwalue = DRF_NUM(_FPF, _OPT_FUSE_UCODE_GFW_REV, __PRI_ALIAS_0_DATA , fuseBurlwalue);
            break;
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_LWFLASH_REV_FOR_WP_BYPASS))
        case LW_FUB_USE_CASE_MASK_LWFLASH_REV_FOR_WP_BYPASS:     // Burn fuse for WP_BYPASS.

            // Bit 4 from LW_FPF_OPT_FUSE_UCODE_ROM_FLASH_REV will be burnt to 0x1 for this use case
            pFuseInfo->fuseBar0Address = LW_FPF_OPT_FUSE_UCODE_ROM_FLASH_REV;

            // Check to make sure Fuse burn value is as expected
            ct_assert(LW_FPF_OPT_FUSE_UCODE_ROM_FLASH_REV_FOR_WP_BYPASS_BUG_2623776 == BIT(4));

            pFuseInfo->fuseIntendedVal = LW_FPF_OPT_FUSE_UCODE_ROM_FLASH_REV_FOR_WP_BYPASS_BUG_2623776;

            pFuseInfo->fusePriAddress  = LW_FPF_OPT_FUSE_UCODE_ROM_FLASH_REV__PRI_ALIAS_0;
            pFuseInfo->fuseRedAddress  = LW_FPF_OPT_FUSE_UCODE_ROM_FLASH_REV__RED_ALIAS_0;

            // Zero out already Burnt Fuse bits.
            CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, pFuseInfo->fuseBar0Address, &fuseBurlwalue));
            fuseBurlwalue = (pFuseInfo->fuseIntendedVal) & (~fuseBurlwalue);
            // Not all fuses start at 0th bit of row. So adjusting fuse value as per location of fuse in row.
            pFuseInfo->fuseAdjustedBurlwalue = DRF_NUM(_FPF, _OPT_FUSE_UCODE_ROM_FLASH_REV, __PRI_ALIAS_0_DATA , fuseBurlwalue);
            break;
#endif
#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_AHESASC_FUSE))
        case LW_FUB_USE_CASE_MASK_AUTO_QS2_AHESASC_FUSE:         // Burn fuse for Ahesasc Qs2 bin

            // Bit 0 from LW_FPF_OPT_SEC2_UCODE1_VERSION will be burnt to 0x1 for this use case
            pFuseInfo->fuseBar0Address = LW_FPF_OPT_SEC2_UCODE1_VERSION; 
              
            // Check to make sure Fuse burn value is as expected
            ct_assert(LW_FPF_OPT_SEC2_UCODE1_VERSION_FOR_AUTO_QS2_AHESASC == BIT(0));

            pFuseInfo->fuseIntendedVal = LW_FPF_OPT_SEC2_UCODE1_VERSION_FOR_AUTO_QS2_AHESASC;

            pFuseInfo->fusePriAddress  = LW_FPF_OPT_SEC2_UCODE1_VERSION__PRI_ALIAS_0;
            pFuseInfo->fuseRedAddress  = LW_FPF_OPT_SEC2_UCODE1_VERSION__RED_ALIAS_0;
            // Zero out already Burnt Fuse bits.
            CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, pFuseInfo->fuseBar0Address, &fuseBurlwalue));
            fuseBurlwalue = (pFuseInfo->fuseIntendedVal) & (~fuseBurlwalue);
            // Not all fuses start at 0th bit of row. So adjusting fuse value as per location of fuse in row.
            pFuseInfo->fuseAdjustedBurlwalue = DRF_NUM(_FPF, _OPT_SEC2_UCODE1_VERSION, __PRI_ALIAS_0_DATA , fuseBurlwalue);
            break;
#endif
#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_ASB_FUSE))
        case LW_FUB_USE_CASE_MASK_AUTO_QS2_ASB_FUSE:             // Burn fuse for ASB Qs2 bin 
            
            // Bit 0 from LW_FPF_OPT_GSP_UCODE1_VERSION  will be burnt to 0x1 for this use case
            pFuseInfo->fuseBar0Address = LW_FPF_OPT_GSP_UCODE1_VERSION;
              
            // Check to make sure Fuse burn value is as expected
            ct_assert(LW_FPF_OPT_GSP_UCODE1_VERSION_FOR_AUTO_QS2_ASB == BIT(0));

            pFuseInfo->fuseIntendedVal = LW_FPF_OPT_GSP_UCODE1_VERSION_FOR_AUTO_QS2_ASB;

            pFuseInfo->fusePriAddress  = LW_FPF_OPT_GSP_UCODE1_VERSION__PRI_ALIAS_0;
            pFuseInfo->fuseRedAddress  = LW_FPF_OPT_GSP_UCODE1_VERSION__RED_ALIAS_0;
            // Zero out already Burnt Fuse bits.
            CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, pFuseInfo->fuseBar0Address, &fuseBurlwalue));
            fuseBurlwalue = (pFuseInfo->fuseIntendedVal) & (~fuseBurlwalue);
            // Not all fuses start at 0th bit of row. So adjusting fuse value as per location of fuse in row.
            pFuseInfo->fuseAdjustedBurlwalue = DRF_NUM(_FPF, _OPT_GSP_UCODE1_VERSION, __PRI_ALIAS_0_DATA , fuseBurlwalue);
            break;
#endif
#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_LWFLASH_FUSE))
        case LW_FUB_USE_CASE_MASK_AUTO_QS2_LWFLASH_FUSE:         // Burn fuse for LwFlash Qs2 bin
            
            // Bit 16 from LW_FPF_OPT_PMU_UCODE8_VERSION  will be burnt to 0x1 for this use case
            pFuseInfo->fuseBar0Address = LW_FPF_OPT_PMU_UCODE8_VERSION;
              
            // Check to make sure Fuse burn value is as expected
            ct_assert(LW_FPF_OPT_PMU_UCODE8_VERSION_FOR_AUTO_QS2_LWFLASH == BIT(0));

            pFuseInfo->fuseIntendedVal = LW_FPF_OPT_PMU_UCODE8_VERSION_FOR_AUTO_QS2_LWFLASH;

            pFuseInfo->fusePriAddress  = LW_FPF_OPT_PMU_UCODE8_VERSION__PRI_ALIAS_0;
            pFuseInfo->fuseRedAddress  = LW_FPF_OPT_PMU_UCODE8_VERSION__RED_ALIAS_0;
            // Zero out already Burnt Fuse bits.
            CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, pFuseInfo->fuseBar0Address, &fuseBurlwalue));
            fuseBurlwalue = (pFuseInfo->fuseIntendedVal) & (~fuseBurlwalue);
            // Not all fuses start at 0th bit of row. So adjusting fuse value as per location of fuse in row.
            pFuseInfo->fuseAdjustedBurlwalue = DRF_NUM(_FPF, _OPT_PMU_UCODE8_VERSION, __PRI_ALIAS_0_DATA , fuseBurlwalue);
            break;
#endif
#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_IMAGESELECT_FUSE))
        case LW_FUB_USE_CASE_MASK_AUTO_QS2_IMAGESELECT_FUSE:     // Burn fuse forImageSelect Qs2 bin

            // Bit 0 from LW_FPF_OPT_PMU_UCODE11_VERSION  will be burnt to 0x1 for this use case
            pFuseInfo->fuseBar0Address = LW_FPF_OPT_PMU_UCODE11_VERSION;
              
            // Check to make sure Fuse burn value is as expected
            ct_assert(LW_FPF_OPT_PMU_UCODE11_VERSION_FOR_AUTO_QS2_IMAGESELECT == BIT(0));

            pFuseInfo->fuseIntendedVal = LW_FPF_OPT_PMU_UCODE11_VERSION_FOR_AUTO_QS2_IMAGESELECT;

            pFuseInfo->fusePriAddress  = LW_FPF_OPT_PMU_UCODE11_VERSION__PRI_ALIAS_0;
            pFuseInfo->fuseRedAddress  = LW_FPF_OPT_PMU_UCODE11_VERSION__RED_ALIAS_0;
            // Zero out already Burnt Fuse bits.
            CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, pFuseInfo->fuseBar0Address, &fuseBurlwalue));
            fuseBurlwalue = (pFuseInfo->fuseIntendedVal) & (~fuseBurlwalue);
            // Not all fuses start at 0th bit of row. So adjusting fuse value as per location of fuse in row.
            pFuseInfo->fuseAdjustedBurlwalue = DRF_NUM(_FPF, _OPT_PMU_UCODE11_VERSION, __PRI_ALIAS_0_DATA , fuseBurlwalue);
            break;
#endif
#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_HULK_FUSE))
        case LW_FUB_USE_CASE_MASK_AUTO_QS2_HULK_FUSE:            // Burn fuse forImageSelect Qs2 bin

            // Bit 16 from LW_FPF_OPT_SEC2_UCODE10_VERSION  will be burnt to 0x1 for this use case
            pFuseInfo->fuseBar0Address = LW_FPF_OPT_SEC2_UCODE10_VERSION;
              
            // Check to make sure Fuse burn value is as expected
            ct_assert(LW_FPF_OPT_SEC2_UCODE10_VERSION_FOR_AUTO_QS2_HULK == BIT(0));

            pFuseInfo->fuseIntendedVal = LW_FPF_OPT_SEC2_UCODE10_VERSION_FOR_AUTO_QS2_HULK;

            pFuseInfo->fusePriAddress  = LW_FPF_OPT_SEC2_UCODE10_VERSION__PRI_ALIAS_0;
            pFuseInfo->fuseRedAddress  = LW_FPF_OPT_SEC2_UCODE10_VERSION__RED_ALIAS_0;
            // Zero out already Burnt Fuse bits.
            CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, pFuseInfo->fuseBar0Address, &fuseBurlwalue));
            fuseBurlwalue = (pFuseInfo->fuseIntendedVal) & (~fuseBurlwalue);
            // Not all fuses start at 0th bit of row. So adjusting fuse value as per location of fuse in row.
            pFuseInfo->fuseAdjustedBurlwalue = DRF_NUM(_FPF, _OPT_SEC2_UCODE10_VERSION, __PRI_ALIAS_0_DATA , fuseBurlwalue);
            break;
#endif
#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_FWSEC_FUSE))
        case LW_FUB_USE_CASE_MASK_AUTO_QS2_FWSEC_FUSE:           // Burn fuse for FwSec Qs2 bin

            // Bit 0 from LW_FPF_OPT_GSP_UCODE9_VERSION will be burnt to 0x1 for this use case
            pFuseInfo->fuseBar0Address = LW_FPF_OPT_GSP_UCODE9_VERSION;
              
            // Check to make sure Fuse burn value is as expected
            ct_assert(LW_FPF_OPT_GSP_UCODE9_VERSION_FOR_AUTO_QS2_FWSEC == BIT(0));

            pFuseInfo->fuseIntendedVal = LW_FPF_OPT_GSP_UCODE9_VERSION_FOR_AUTO_QS2_FWSEC;

            pFuseInfo->fusePriAddress  = LW_FPF_OPT_GSP_UCODE9_VERSION__PRI_ALIAS_0;
            pFuseInfo->fuseRedAddress  = LW_FPF_OPT_GSP_UCODE9_VERSION__RED_ALIAS_0;
            // Zero out already Burnt Fuse bits.
            CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, pFuseInfo->fuseBar0Address, &fuseBurlwalue));
            fuseBurlwalue = (pFuseInfo->fuseIntendedVal) & (~fuseBurlwalue);
            // Not all fuses start at 0th bit of row. So adjusting fuse value as per location of fuse in row.
            pFuseInfo->fuseAdjustedBurlwalue = DRF_NUM(_FPF, _OPT_GSP_UCODE9_VERSION, __PRI_ALIAS_0_DATA , fuseBurlwalue);
            break;
#endif
        default:
            CHECK_FUB_STATUS(FUB_ERR_DEVID_NOT_SUPPORTED);
    }

ErrorExit:
    return fubStatus;
}

/*!
 * @brief Check if Fuse burn was successful.
 */
FUB_STATUS
fubFuseBurnCheck_TU10X(LwU32 fubUseCaseMask)
{
    FUB_STATUS    fubStatus                = FUB_ERR_NEW_FUSE_VALUE_NOT_REFLECTED;
    LwBool        bIsFuseBurningApplicable = LW_FALSE;
    LwU32         fuseBar0Value            = 0;
    FPF_FUSE_INFO fuseInfo;

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_GSYNC_ENABLING_FUSE))
    bIsFuseBurningApplicable = LW_FALSE;
    // Check first if Gsync Enabling fuse is allowed to be burnt on current GPU
    CHECK_FUB_STATUS(fubCheckIfBurningGsyncEnablingIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));

    if(bIsFuseBurningApplicable)
    {
        //
        // After fuse burn is complete, FUB sets bit in use case mask specific to that use case.
        // First check if bit is set in use case mask. Return early if not set.
        //
        if (!(fubUseCaseMask & LW_FUB_USE_CASE_MASK_ENABLE_GSYNC))
        {
           CHECK_FUB_STATUS(FUB_ERR_ENABLING_GSYNC_BIT_NOT_SET_IN_MASK);
        }

        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_ENABLE_GSYNC));

        CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, fuseInfo.fuseBar0Address, &fuseBar0Value));
        if (fuseBar0Value != fuseInfo.fuseIntendedVal)
        {
            CHECK_FUB_STATUS(FUB_ERR_NEW_FUSE_VALUE_NOT_REFLECTED);
        }
        else
        {
            fubStatus = FUB_OK;
        }
    }
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_DEVID_LICENSE_REVOCATION_FUSE))
    // Now Check if Devid License Revocation fuse is allowed to be burnt on current GPU
    bIsFuseBurningApplicable = LW_FALSE;
    CHECK_FUB_STATUS(fubCheckIfBurningDevidRevocationIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));

    if(bIsFuseBurningApplicable)
    {
        //
        // After fuse burn is complete, FUB sets bit in use case mask specific to that use case.
        // First check if bit is set in use case mask. Return early if not set.
        //
        if (!(fubUseCaseMask & LW_FUB_USE_CASE_MASK_REVOKE_DEVID_BASED_HULK))
        {
           CHECK_FUB_STATUS(FUB_ERR_DEVID_BASED_HULK_REVOKE_BIT_NOT_SET_IN_MASK);
        }

        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_REVOKE_DEVID_BASED_HULK));

        CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, fuseInfo.fuseBar0Address, &fuseBar0Value));
        if (fuseBar0Value != fuseInfo.fuseIntendedVal)
        {
            CHECK_FUB_STATUS(FUB_ERR_NEW_FUSE_VALUE_NOT_REFLECTED);
        }
        else
        {
            fubStatus = FUB_OK;
        }
    }
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_ALLOW_DRAM_CHIPID_READ_HULK_LICENSE))
    bIsFuseBurningApplicable = LW_FALSE;
    // Check first if fuse enabling HULK to read DRAM CHIPID read is allowed to be burnt on current GPU
    CHECK_FUB_STATUS(fubCheckIfAllowingDramChipidReadIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));

    if(bIsFuseBurningApplicable)
    {
        //
        // After fuse burn is complete, FUB sets bit in use case mask specific to that use case.
        // First check if bit is set in use case mask. Return early if not set.
        //
        if (!(fubUseCaseMask & LW_FUB_USE_CASE_MASK_ALLOW_DRAM_CHIPID_READ))
        {
           CHECK_FUB_STATUS(FUB_ERR_HULK_ALLOWING_DRAM_CHIPID_READ_NOT_SET_IN_MASK);
        }

        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_ALLOW_DRAM_CHIPID_READ));
        CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, fuseInfo.fuseBar0Address, &fuseBar0Value));
        if (fuseBar0Value != fuseInfo.fuseIntendedVal)
        {

            CHECK_FUB_STATUS(FUB_ERR_NEW_FUSE_VALUE_NOT_REFLECTED);
        }
        else
        {
            fubStatus = FUB_OK;
        }
    }
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_REVOKE_DRAM_CHIPID_READ_HULK_LICENSE))
    bIsFuseBurningApplicable = LW_FALSE;
    // Check first if fuse blocking HULK to read DRAM CHIPID read is allowed to be burnt on current GPU
    CHECK_FUB_STATUS(fubCheckIfBlockingDramChipidReadIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));

    if(bIsFuseBurningApplicable)
    {
        //
        // After fuse burn is complete, FUB sets bit in use case mask specific to that use case.
        // First check if bit is set in use case mask. Return early if not set.
        //
        if (!(fubUseCaseMask & LW_FUB_USE_CASE_MASK_BLOCK_DRAM_CHIPID_READ))
        {
           CHECK_FUB_STATUS(FUB_ERR_HULK_REVOKING_DRAM_CHIPID_READ_NOT_SET_IN_MASK);
        }

        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_BLOCK_DRAM_CHIPID_READ));
        CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, fuseInfo.fuseBar0Address, &fuseBar0Value));
        //
        // 0th bit of fuse will already be burnt to enable HULK usage, So check here should
        // be if 2nd bit which is alocated for Revoking of fuse is set.
        //
        if (((fuseBar0Value & fuseInfo.fuseIntendedVal) != fuseInfo.fuseIntendedVal))
        {
            CHECK_FUB_STATUS(FUB_ERR_NEW_FUSE_VALUE_NOT_REFLECTED);
        }
        else
        {
            fubStatus = FUB_OK;
        }
    }
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_GEFORCE_SKU_RECOVERY_GFW_REV))
    bIsFuseBurningApplicable = LW_FALSE;
    // Check first if fuse for VdChip SKU Recovery GFW_REV is allowed to be burnt on current GPU
    CHECK_FUB_STATUS(fubCheckIfGeforceSkuRecoveryGfwRevIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));

    if(bIsFuseBurningApplicable)
    {
        //
        // After fuse burn is complete, FUB sets bit in use case mask specific to that use case.
        // First check if bit is set in use case mask. Return early if not set.
        //
        if (!(fubUseCaseMask & LW_FUB_USE_CASE_MASK_GEFORCE_SKU_RECOVERY_GFW_REV))
        {
           CHECK_FUB_STATUS(FUB_ERR_GEFORCE_SKU_RECOVERY_GFW_REV_BIT_NOT_SET_IN_MASK);
        }

        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_GEFORCE_SKU_RECOVERY_GFW_REV));
        CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, fuseInfo.fuseBar0Address, &fuseBar0Value));
        //
        // Check only intended bits since others are allocated for other purposes
        //
        if (((fuseBar0Value & fuseInfo.fuseIntendedVal) != fuseInfo.fuseIntendedVal))
        {
            CHECK_FUB_STATUS(FUB_ERR_NEW_FUSE_VALUE_NOT_REFLECTED);
        }
        else
        {
            fubStatus = FUB_OK;
        }
    }
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_GEFORCE_SKU_RECOVERY))
    bIsFuseBurningApplicable = LW_FALSE;
    // Check first if fuse for VdChip SKU Recovery is allowed to be burnt on current GPU
    CHECK_FUB_STATUS(fubCheckIfGeforceSkuRecoveryIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));

    if(bIsFuseBurningApplicable)
    {
        //
        // After fuse burn is complete, FUB sets bit in use case mask specific to that use case.
        // First check if bit is set in use case mask. Return early if not set.
        //
        if (!(fubUseCaseMask & LW_FUB_USE_CASE_MASK_GEFORCE_SKU_RECOVERY))
        {
           CHECK_FUB_STATUS(FUB_ERR_GEFORCE_SKU_RECOVERY_SPARE_BIT_NOT_SET_IN_MASK);
        }

        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_GEFORCE_SKU_RECOVERY));
        CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, fuseInfo.fuseBar0Address, &fuseBar0Value));
        //
        // Check only intended bits since others are allocated for other purposes
        //
        if (((fuseBar0Value & fuseInfo.fuseIntendedVal) != fuseInfo.fuseIntendedVal))
        {
            CHECK_FUB_STATUS(FUB_ERR_NEW_FUSE_VALUE_NOT_REFLECTED);
        }
        else
        {
            fubStatus = FUB_OK;
        }
    }
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_LWFLASH_REV_FOR_WP_BYPASS))
    bIsFuseBurningApplicable = LW_FALSE;
    // Check first if ROM_FLASH_REV fuse for WP_BYPASS mitigation is allowed to be burnt on current GPU
    CHECK_FUB_STATUS(fubCheckIfLwflashRevForWpBypassIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));

    if(bIsFuseBurningApplicable)
    {
        //
        // After fuse burn is complete, FUB sets bit in use case mask specific to that use case.
        // First check if bit is set in use case mask. Return early if not set.
        //
        if (!(fubUseCaseMask & LW_FUB_USE_CASE_MASK_LWFLASH_REV_FOR_WP_BYPASS))
        {
           CHECK_FUB_STATUS(FUB_ERR_LWFLASH_REV_FOR_WP_BYPASS_SPARE_BIT_NOT_SET_IN_MASK);
        }

        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_LWFLASH_REV_FOR_WP_BYPASS));
        CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, fuseInfo.fuseBar0Address, &fuseBar0Value));
        //
        // Check only intended bits since others are allocated for other purposes
        //
        if (((fuseBar0Value & fuseInfo.fuseIntendedVal) != fuseInfo.fuseIntendedVal))
        {
            CHECK_FUB_STATUS(FUB_ERR_NEW_FUSE_VALUE_NOT_REFLECTED);
        }
        else
        {
            fubStatus = FUB_OK;
        }
    }
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_GFW_REV_FOR_WP_BYPASS))
    bIsFuseBurningApplicable = LW_FALSE;
    // Check first if fuse for GFW_REV for WP_BYPASS mitigation is allowed to be burnt on current GPU
    CHECK_FUB_STATUS(fubCheckIfGfwRevForWpBypassIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));

    if(bIsFuseBurningApplicable)
    {
        //
        // After fuse burn is complete, FUB sets bit in use case mask specific to that use case.
        // First check if bit is set in use case mask. Return early if not set.
        //
        if (!(fubUseCaseMask & LW_FUB_USE_CASE_MASK_GFW_REV_FOR_WP_BYPASS))
        {
           CHECK_FUB_STATUS(FUB_ERR_GFW_REV_FOR_WP_BYPASS_NOT_SET_IN_MASK);
        }

        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_GFW_REV_FOR_WP_BYPASS)); 

        CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, fuseInfo.fuseBar0Address, &fuseBar0Value));
        //
        // Check only intended bits since others are allocated for other purposes
        //
        if (((fuseBar0Value & fuseInfo.fuseIntendedVal) != fuseInfo.fuseIntendedVal))
        {
            CHECK_FUB_STATUS(FUB_ERR_NEW_FUSE_VALUE_NOT_REFLECTED);
        }
        else
        {
            fubStatus = FUB_OK;
        }
    }
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_AHESASC_FUSE))
    bIsFuseBurningApplicable = LW_FALSE;
    // Check first if fuse SEC2_UCODE1 for auto QS1_TO_QS2 mitigation is allowed to be burnt on current GPU
    CHECK_FUB_STATUS(fubCheckIfAutoQs2AhesascIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));
    if(bIsFuseBurningApplicable)
    {
        //
        // After fuse burn is complete, FUB sets bit in use case mask specific to that use case.
        // First check if bit is set in use case mask. Return early if not set.
        //
        if (!(fubUseCaseMask & LW_FUB_USE_CASE_MASK_AUTO_QS2_AHESASC_FUSE))
        {
           CHECK_FUB_STATUS(FUB_ERR_AUTO_QS2_AHESASC_NOT_SET_IN_MASK);
        }

        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_AUTO_QS2_AHESASC_FUSE)); 

        CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, fuseInfo.fuseBar0Address, &fuseBar0Value));
        //
        // Check only intended bits since others are allocated for other purposes
        //
        if (((fuseBar0Value & fuseInfo.fuseIntendedVal) != fuseInfo.fuseIntendedVal))
        {
            CHECK_FUB_STATUS(FUB_ERR_NEW_FUSE_VALUE_NOT_REFLECTED);
        }
        else
        {
            fubStatus = FUB_OK;
        }
    }
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_ASB_FUSE))
    bIsFuseBurningApplicable = LW_FALSE;
    // Check first if fuse GSP_UCODE1 for auto QS1_TO_QS2 mitigation is allowed to be burnt on current GPU
    CHECK_FUB_STATUS(fubCheckIfAutoQs2AsbIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));
    if(bIsFuseBurningApplicable)
    {
        //
        // After fuse burn is complete, FUB sets bit in use case mask specific to that use case.
        // First check if bit is set in use case mask. Return early if not set.
        //
        if (!(fubUseCaseMask & LW_FUB_USE_CASE_MASK_AUTO_QS2_ASB_FUSE))
        {
           CHECK_FUB_STATUS(FUB_ERR_AUTO_QS2_ASB_NOT_SET_IN_MASK);
        }

        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_AUTO_QS2_ASB_FUSE)); 

        CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, fuseInfo.fuseBar0Address, &fuseBar0Value));
        //
        // Check only intended bits since others are allocated for other purposes
        //
        if (((fuseBar0Value & fuseInfo.fuseIntendedVal) != fuseInfo.fuseIntendedVal))
        {
            CHECK_FUB_STATUS(FUB_ERR_NEW_FUSE_VALUE_NOT_REFLECTED);
        }
        else
        {
            fubStatus = FUB_OK;
        }
    }
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_LWFLASH_FUSE))
    bIsFuseBurningApplicable = LW_FALSE;
    // Check first if fuse PMU_UCODE8 for auto QS1_TO_QS2 mitigation is allowed to be burnt on current GPU
    CHECK_FUB_STATUS(fubCheckIfAutoQs2LwflashIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));
    if(bIsFuseBurningApplicable)
    {
        //
        // After fuse burn is complete, FUB sets bit in use case mask specific to that use case.
        // First check if bit is set in use case mask. Return early if not set.
        //
        if (!(fubUseCaseMask & LW_FUB_USE_CASE_MASK_AUTO_QS2_LWFLASH_FUSE))
        {
           CHECK_FUB_STATUS(FUB_ERR_AUTO_QS2_LWFLASH_NOT_SET_IN_MASK);
        }

        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_AUTO_QS2_LWFLASH_FUSE)); 

        CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, fuseInfo.fuseBar0Address, &fuseBar0Value));
        //
        // Check only intended bits since others are allocated for other purposes
        //
        if (((fuseBar0Value & fuseInfo.fuseIntendedVal) != fuseInfo.fuseIntendedVal))
        {
            CHECK_FUB_STATUS(FUB_ERR_NEW_FUSE_VALUE_NOT_REFLECTED);
        }
        else
        {
            fubStatus = FUB_OK;
        }
    }
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_IMAGESELECT_FUSE))
    bIsFuseBurningApplicable = LW_FALSE;
    // Check first if fuse PMU_UCODE11 for auto QS1_TO_QS2 mitigation is allowed to be burnt on current GPU
    CHECK_FUB_STATUS(fubCheckIfAutoQs2ImageSelectIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));
    if(bIsFuseBurningApplicable)
    {
        //
        // After fuse burn is complete, FUB sets bit in use case mask specific to that use case.
        // First check if bit is set in use case mask. Return early if not set.
        //
        if (!(fubUseCaseMask & LW_FUB_USE_CASE_MASK_AUTO_QS2_IMAGESELECT_FUSE))
        {
           CHECK_FUB_STATUS(FUB_ERR_AUTO_QS2_IMAGESELECT_NOT_SET_IN_MASK);
        }

        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_AUTO_QS2_IMAGESELECT_FUSE)); 

        CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, fuseInfo.fuseBar0Address, &fuseBar0Value));
        //
        // Check only intended bits since others are allocated for other purposes
        //
        if (((fuseBar0Value & fuseInfo.fuseIntendedVal) != fuseInfo.fuseIntendedVal))
        {
            CHECK_FUB_STATUS(FUB_ERR_NEW_FUSE_VALUE_NOT_REFLECTED);
        }
        else
        {
            fubStatus = FUB_OK;
        }
    }
#endif
#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_HULK_FUSE))
    bIsFuseBurningApplicable = LW_FALSE;
    // Check first if fuse SEC2_UCODE10 for auto QS1_TO_QS2 mitigation is allowed to be burnt on current GPU
    CHECK_FUB_STATUS(fubCheckIfAutoQs2HulkIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));
    if(bIsFuseBurningApplicable)
    {
        //
        // After fuse burn is complete, FUB sets bit in use case mask specific to that use case.
        // First check if bit is set in use case mask. Return early if not set.
        //
        if (!(fubUseCaseMask & LW_FUB_USE_CASE_MASK_AUTO_QS2_HULK_FUSE))
        {
           CHECK_FUB_STATUS(FUB_ERR_AUTO_QS2_HULK_NOT_SET_IN_MASK);
        }

        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_AUTO_QS2_HULK_FUSE)); 

        CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, fuseInfo.fuseBar0Address, &fuseBar0Value));
        //
        // Check only intended bits since others are allocated for other purposes
        //
        if (((fuseBar0Value & fuseInfo.fuseIntendedVal) != fuseInfo.fuseIntendedVal))
        {
            CHECK_FUB_STATUS(FUB_ERR_NEW_FUSE_VALUE_NOT_REFLECTED);
        }
        else
        {
            fubStatus = FUB_OK;
        }
    }
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_FWSEC_FUSE))
    bIsFuseBurningApplicable = LW_FALSE;
    // Check first if fuse GSP_UCODE9 for auto QS1_TO_QS2 mitigation is allowed to be burnt on current GPU
    CHECK_FUB_STATUS(fubCheckIfAutoQs2FwSecIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));
    if(bIsFuseBurningApplicable)
    {
        //
        // After fuse burn is complete, FUB sets bit in use case mask specific to that use case.
        // First check if bit is set in use case mask. Return early if not set.
        //
        if (!(fubUseCaseMask & LW_FUB_USE_CASE_MASK_AUTO_QS2_FWSEC_FUSE))
        {
           CHECK_FUB_STATUS(FUB_ERR_AUTO_QS2_FWSEC_NOT_SET_IN_MASK);
        }

        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_AUTO_QS2_FWSEC_FUSE)); 

        CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, fuseInfo.fuseBar0Address, &fuseBar0Value));
        //
        // Check only intended bits since others are allocated for other purposes
        //
        if (((fuseBar0Value & fuseInfo.fuseIntendedVal) != fuseInfo.fuseIntendedVal))
        {
            CHECK_FUB_STATUS(FUB_ERR_NEW_FUSE_VALUE_NOT_REFLECTED);
        }
        else
        {
            fubStatus = FUB_OK;
        }
    }
#endif

ErrorExit:
    return fubStatus;
}

/*!
 * @brief Check if FUB nneds to run else return.
 */
FUB_STATUS
fubCheckIfFubNeedsToRun_TU10X(void)
{
    LwBool        bIsFuseBurningApplicable = LW_FALSE;
    FUB_STATUS    fubStatus                = FUB_ERR_FUSE_BURN_NOT_REQUIRED;
    FPF_FUSE_INFO fuseInfo;

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_GSYNC_ENABLING_FUSE))
    // Check first if Gsync Enabling fuse is allowed to be burnt on current GPU
    bIsFuseBurningApplicable = LW_FALSE;
    CHECK_FUB_STATUS(fubCheckIfBurningGsyncEnablingIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));

    if(bIsFuseBurningApplicable)
    {
        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_ENABLE_GSYNC));

        // Proceed to Burning only if New fuseValue bits are not already burnt
        if (fuseInfo.fuseAdjustedBurlwalue)
        {
            // FUB will run even if 1 use case is valid
            fubStatus = FUB_OK;
            goto ErrorExit;
        }
        else
        {
            fubStatus = FUB_ERR_FUSE_BURN_NOT_REQUIRED;
        }
    }
    else
    {
        fubStatus = FUB_ERR_FUSE_BURN_NOT_REQUIRED;
    }
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_DEVID_LICENSE_REVOCATION_FUSE))
    // Now Check if Devid License Revocation fuse is allowed to be burnt on current GPU
    bIsFuseBurningApplicable = LW_FALSE;
    CHECK_FUB_STATUS(fubCheckIfBurningDevidRevocationIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));

    if(bIsFuseBurningApplicable)
    {
        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_REVOKE_DEVID_BASED_HULK));

        // Proceed to Burning only if New fuseValue bits are not already burnt
        if (fuseInfo.fuseAdjustedBurlwalue)
        {
            // FUB will run even if only 1 use case is valid
            fubStatus = FUB_OK;
            goto ErrorExit;
        }
        else
        {
            fubStatus = FUB_ERR_FUSE_BURN_NOT_REQUIRED;
        }
     }
    else
    {
        fubStatus = FUB_ERR_FUSE_BURN_NOT_REQUIRED;
    }
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_ALLOW_DRAM_CHIPID_READ_HULK_LICENSE))
    // Check first if Allowing DRAM CHIPID read fuse is allowed to be burnt on current GPU
    bIsFuseBurningApplicable = LW_FALSE;
    CHECK_FUB_STATUS(fubCheckIfAllowingDramChipidReadIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));

    if(bIsFuseBurningApplicable)
    {
        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_ALLOW_DRAM_CHIPID_READ));
        // Proceed to Burning only if New fuseValue bits are not already burnt
        if (fuseInfo.fuseAdjustedBurlwalue)
        {
            // FUB will run even if 1 use case is valid
            fubStatus = FUB_OK;
            goto ErrorExit;
        }
        else
        {
            fubStatus = FUB_ERR_FUSE_BURN_NOT_REQUIRED;
        }
    }
    else
    {
        fubStatus = FUB_ERR_FUSE_BURN_NOT_REQUIRED;
    }
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_REVOKE_DRAM_CHIPID_READ_HULK_LICENSE))
    // Check first if blockiong DRAM CHIPID read fuse is allowed to be burnt on current GPU
    bIsFuseBurningApplicable = LW_FALSE;
    CHECK_FUB_STATUS(fubCheckIfBlockingDramChipidReadIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));

    if(bIsFuseBurningApplicable)
    {
        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_BLOCK_DRAM_CHIPID_READ));
        // Proceed to Burning only if New fuseValue bits are not already burnt
        if (fuseInfo.fuseAdjustedBurlwalue)
        {
            // FUB will run even if 1 use case is valid
            fubStatus = FUB_OK;
            goto ErrorExit;
        }
        else
        {
            fubStatus = FUB_ERR_FUSE_BURN_NOT_REQUIRED;
        }
    }
    else
    {
        fubStatus = FUB_ERR_FUSE_BURN_NOT_REQUIRED;
    }
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_GEFORCE_SKU_RECOVERY_GFW_REV))
    // Check first if VdChip SKU recovery GFW_REV fuse is allowed to be burnt on current GPU
    bIsFuseBurningApplicable = LW_FALSE;
    CHECK_FUB_STATUS(fubCheckIfGeforceSkuRecoveryGfwRevIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));

    if(bIsFuseBurningApplicable)
    {
        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_GEFORCE_SKU_RECOVERY_GFW_REV));
        // Proceed to Burning only if New fuseValue bits are not already burnt
        if (fuseInfo.fuseAdjustedBurlwalue)
        {
            // FUB will run even if 1 use case is valid
            fubStatus = FUB_OK;
            goto ErrorExit;
        }
        else
        {
            fubStatus = FUB_ERR_FUSE_BURN_NOT_REQUIRED;
        }
    }
    else
    {
        fubStatus = FUB_ERR_FUSE_BURN_NOT_REQUIRED;
    }
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_GEFORCE_SKU_RECOVERY))
    // Check first if VdChip SKU recovery fuse is allowed to be burnt on current GPU
    bIsFuseBurningApplicable = LW_FALSE;
    CHECK_FUB_STATUS(fubCheckIfGeforceSkuRecoveryIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));

    if(bIsFuseBurningApplicable)
    {
        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_GEFORCE_SKU_RECOVERY));
        // Proceed to Burning only if New fuseValue bits are not already burnt
        if (fuseInfo.fuseAdjustedBurlwalue)
        {
            // FUB will run even if 1 use case is valid
            fubStatus = FUB_OK;
            goto ErrorExit;
        }
        else
        {
            fubStatus = FUB_ERR_FUSE_BURN_NOT_REQUIRED;
        }
    }
    else
    {
        fubStatus = FUB_ERR_FUSE_BURN_NOT_REQUIRED;
    }
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_LWFLASH_REV_FOR_WP_BYPASS))
    // Check first if ROM_FLASH_REV fuse for WP_BYPASS mitigation is allowed to be burnt on current GPU
    bIsFuseBurningApplicable = LW_FALSE;
    CHECK_FUB_STATUS(fubCheckIfLwflashRevForWpBypassIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));

    if(bIsFuseBurningApplicable)
    {
        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_LWFLASH_REV_FOR_WP_BYPASS));
        // Proceed to Burning only if New fuseValue bits are not already burnt
        if (fuseInfo.fuseAdjustedBurlwalue)
        {
            // FUB will run even if 1 use case is valid
            fubStatus = FUB_OK;
            //goto ErrorExit;
        }
        else
        {
            fubStatus = FUB_ERR_FUSE_BURN_NOT_REQUIRED;
        }
    }
    else
    {
        fubStatus = FUB_ERR_FUSE_BURN_NOT_REQUIRED;
    }
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_GFW_REV_FOR_WP_BYPASS))
    // Check first if GFW_REV fuse for WP_BYPASS mitigation is allowed to be burnt on current GPU
    bIsFuseBurningApplicable = LW_FALSE;
    CHECK_FUB_STATUS(fubCheckIfGfwRevForWpBypassIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));

    if(bIsFuseBurningApplicable)
    {
        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_GFW_REV_FOR_WP_BYPASS)); 
        // Proceed to Burning only if New fuseValue bits are not already burnt
        if (fuseInfo.fuseAdjustedBurlwalue)
        {
            // FUB will run even if 1 use case is valid
            fubStatus = FUB_OK;
            goto ErrorExit;
        }
        else
        {
            fubStatus = FUB_ERR_FUSE_BURN_NOT_REQUIRED;
        }
    }
    else
    {
        fubStatus = FUB_ERR_FUSE_BURN_NOT_REQUIRED;
    }
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_AHESASC_FUSE))
    // Check first if AUTO_QS2_AHESASC  fuse  is allowed to be burnt on current GPU
    bIsFuseBurningApplicable = LW_FALSE;
    CHECK_FUB_STATUS(fubCheckIfAutoQs2AhesascIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));
    if (bIsFuseBurningApplicable)
    {
        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_AUTO_QS2_AHESASC_FUSE)); 
        // Proceed to Burning only if New fuseValue bits are not already burnt
        if (fuseInfo.fuseAdjustedBurlwalue)
        {
            // FUB will run even if 1 use case is valid
            fubStatus = FUB_OK;
            goto ErrorExit;
        }
        else
        {
            fubStatus = FUB_ERR_FUSE_BURN_NOT_REQUIRED;
        }
    }
    else
    {
        fubStatus = FUB_ERR_FUSE_BURN_NOT_REQUIRED;
    }
        
#endif
#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_ASB_FUSE))
    // Check first if AUTO_QS2_ASB  fuse  is allowed to be burnt on current GPU
    bIsFuseBurningApplicable = LW_FALSE;
    CHECK_FUB_STATUS(fubCheckIfAutoQs2AsbIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));
    if (bIsFuseBurningApplicable)
    {
        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_AUTO_QS2_ASB_FUSE)); 
        // Proceed to Burning only if New fuseValue bits are not already burnt
        if (fuseInfo.fuseAdjustedBurlwalue)
        {
            // FUB will run even if 1 use case is valid
            fubStatus = FUB_OK;
            goto ErrorExit;
        }
        else
        {
            fubStatus = FUB_ERR_FUSE_BURN_NOT_REQUIRED;
        }
    }
    else
    {
        fubStatus = FUB_ERR_FUSE_BURN_NOT_REQUIRED;
    }
        
#endif
#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_LWFLASH_FUSE))
    // Check first if AUTO_QS2_LWFLASH  fuse  is allowed to be burnt on current GPU
    bIsFuseBurningApplicable = LW_FALSE;
    CHECK_FUB_STATUS(fubCheckIfAutoQs2LwflashIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));
    if (bIsFuseBurningApplicable)
    {
        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_AUTO_QS2_LWFLASH_FUSE)); 
        // Proceed to Burning only if New fuseValue bits are not already burnt
        if (fuseInfo.fuseAdjustedBurlwalue)
        {
            // FUB will run even if 1 use case is valid
            fubStatus = FUB_OK;
            goto ErrorExit;
        }
        else
        {
            fubStatus = FUB_ERR_FUSE_BURN_NOT_REQUIRED;
        }
    }
    else
    {
        fubStatus = FUB_ERR_FUSE_BURN_NOT_REQUIRED;
    }
#endif
#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_IMAGESELECT_FUSE))
    // Check first if AUTO_QS2_IMAGESELECT  fuse  is allowed to be burnt on current GPU
    bIsFuseBurningApplicable = LW_FALSE;
    CHECK_FUB_STATUS(fubCheckIfAutoQs2ImageSelectIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));
    if (bIsFuseBurningApplicable)
    {
        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_AUTO_QS2_IMAGESELECT_FUSE)); 
        // Proceed to Burning only if New fuseValue bits are not already burnt
        if (fuseInfo.fuseAdjustedBurlwalue)
        {
            // FUB will run even if 1 use case is valid
            fubStatus = FUB_OK;
            goto ErrorExit;
        }
        else
        {
            fubStatus = FUB_ERR_FUSE_BURN_NOT_REQUIRED;
        }
    }
    else
    {
        fubStatus = FUB_ERR_FUSE_BURN_NOT_REQUIRED;
    }
#endif
#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_HULK_FUSE))
    // Check first if AUTO_QS2_HULK  fuse  is allowed to be burnt on current GPU
    bIsFuseBurningApplicable = LW_FALSE;
    CHECK_FUB_STATUS(fubCheckIfAutoQs2HulkIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));
    if (bIsFuseBurningApplicable)
    {
        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_AUTO_QS2_HULK_FUSE)); 
        // Proceed to Burning only if New fuseValue bits are not already burnt
        if (fuseInfo.fuseAdjustedBurlwalue)
        {
            // FUB will run even if 1 use case is valid
            fubStatus = FUB_OK;
            goto ErrorExit;
        }
        else
        {
            fubStatus = FUB_ERR_FUSE_BURN_NOT_REQUIRED;
        }
    }
    else
    {
        fubStatus = FUB_ERR_FUSE_BURN_NOT_REQUIRED;
    }
        
#endif
#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_FWSEC_FUSE))
    // Check first if AUTO_QS2_FWSEC  fuse  is allowed to be burnt on current GPU
    bIsFuseBurningApplicable = LW_FALSE;
    CHECK_FUB_STATUS(fubCheckIfAutoQs2FwSecIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));
    if (bIsFuseBurningApplicable)
    {
        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_AUTO_QS2_FWSEC_FUSE)); 
        // Proceed to Burning only if New fuseValue bits are not already burnt
        if (fuseInfo.fuseAdjustedBurlwalue)
        {
            // FUB will run even if 1 use case is valid
            fubStatus = FUB_OK;
            goto ErrorExit;
        }
        else
        {
            fubStatus = FUB_ERR_FUSE_BURN_NOT_REQUIRED;
        }
    }
    else
    {
        fubStatus = FUB_ERR_FUSE_BURN_NOT_REQUIRED;
    }
        
#endif
ErrorExit:
    return fubStatus;
}

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_GSYNC_ENABLING_FUSE))
/*
 * @brief: Check if GSYNC enabling fuse is to be burnt and Burn if check passes
 */
FUB_STATUS
fubCheckAndBurnGsyncEnablingFuse_TU10X(LwU32 *pfubUseCasMask)
{
    LwBool        bIsFuseBurningApplicable = LW_FALSE;
    FUB_STATUS    fubStatus                = FUB_ERR_GENERIC;
    FPF_FUSE_INFO fuseInfo;

    // Return early if pointer passed as argument is NULL.
    if (pfubUseCasMask == NULL)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }

    // Return early if current use case specific bit is set in use case mask
    if (*pfubUseCasMask & LW_FUB_USE_CASE_MASK_ENABLE_GSYNC)
    {
        CHECK_FUB_STATUS(FUB_ERR_ENABLE_GSYNC_BIT_ALREADY_SET_IN_MASK);
    }

    // Before Burning Fuse Verify DEVID check passes.
    CHECK_FUB_STATUS(fubCheckIfBurningGsyncEnablingIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));

    if(bIsFuseBurningApplicable)
    {
        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_ENABLE_GSYNC));

        // Proceed to Burning only if New fuseValue bits are not already burnt
        if (fuseInfo.fuseAdjustedBurlwalue)
        {
            // Burn Primary Fuse
            CHECK_FUB_STATUS(fubWriteFieldProgFuse_HAL(pFub, fuseInfo.fusePriAddress, fuseInfo.fuseAdjustedBurlwalue));

            // Burn Redundant Fuse
            CHECK_FUB_STATUS(fubWriteFieldProgFuse_HAL(pFub, fuseInfo.fuseRedAddress, fuseInfo.fuseAdjustedBurlwalue));

            //
            // If we reach this point then, fuse burn is successful,
            // Set use case specific bit in mask variable, for further reference.
            //
            *pfubUseCasMask = (*pfubUseCasMask | LW_FUB_USE_CASE_MASK_ENABLE_GSYNC);
        }
        else
        {
            // Ideally FUB should not reach here, since if condition is already checked in fubCheckIfFubNeedsToRun_* function.
            CHECK_FUB_STATUS(FUB_ERR_FUSE_BURN_NOT_REQUIRED);
        }
    }
    else
    {
        // Do not return error as FUB may be running for another use case.
        fubStatus = FUB_OK;
    }
ErrorExit:
    return fubStatus;
}
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_DEVID_LICENSE_REVOCATION_FUSE))
/*
 * @brief: First Check if Devid License Revocation fuse is to be burnt and Burn if check passes
 */
FUB_STATUS
fubCheckAndBurnDevidLicenseRevocationFuse_TU10X(LwU32 *pfubUseCasMask)
{
    LwBool        bIsFuseBurningApplicable = LW_FALSE;
    FUB_STATUS    fubStatus                = FUB_ERR_GENERIC;
    FPF_FUSE_INFO fuseInfo;

    // Return early if pointer passed as argument is NULL.
    if (pfubUseCasMask == NULL)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }

    // Return early if current use case specific bit is set in use case mask
    if (*pfubUseCasMask & LW_FUB_USE_CASE_MASK_REVOKE_DEVID_BASED_HULK)
    {
        CHECK_FUB_STATUS(FUB_ERR_DEVID_BASED_HULK_REVOKE_BIT_ALREADY_SET_IN_MASK);
    }

    // Before Burning Fuse Verify DEVID check passes.
    CHECK_FUB_STATUS(fubCheckIfBurningDevidRevocationIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));

    if(bIsFuseBurningApplicable)
    {
        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_REVOKE_DEVID_BASED_HULK));
        // Proceed to Burning only if New fuseValue bits are not already burnt
        if (fuseInfo.fuseAdjustedBurlwalue)
        {
            // Burn Primary Fuse
            CHECK_FUB_STATUS(fubWriteFieldProgFuse_HAL(pFub, fuseInfo.fusePriAddress, fuseInfo.fuseAdjustedBurlwalue));

            // Burn Redundant Fuse
            CHECK_FUB_STATUS(fubWriteFieldProgFuse_HAL(pFub, fuseInfo.fuseRedAddress, fuseInfo.fuseAdjustedBurlwalue));

            //
            // If we reach this point then, fuse burn is successful,
            // Set use case specific bit in mask variable, for further reference.
            //
            *pfubUseCasMask = (*pfubUseCasMask | LW_FUB_USE_CASE_MASK_REVOKE_DEVID_BASED_HULK);
        }
        else
        {
            CHECK_FUB_STATUS(FUB_ERR_FUSE_BURN_NOT_REQUIRED);
        }
    }
    else
    {
        // Do not return error as FUB may be running for another use case.
        fubStatus = FUB_OK;
    }
ErrorExit:
    return fubStatus;
}
#endif


#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_REVOKE_DRAM_CHIPID_READ_HULK_LICENSE))
/*
 * @brief: Check if fuse enabling HULK license to read DRAM CHIPID is to be burnt and Burn if check passes
 */
FUB_STATUS
fubCheckAndBurnAllowingDramChipidReadFuse_TU10X(LwU32 *pfubUseCasMask)
{
    LwBool        bIsFuseBurningApplicable = LW_FALSE;
    FUB_STATUS    fubStatus                = FUB_ERR_GENERIC;
    FPF_FUSE_INFO fuseInfo;

    // Return early if pointer passed as argument is NULL.
    if (pfubUseCasMask == NULL)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }

    // Return early if current use case specific bit is set in use case mask
    if (*pfubUseCasMask & LW_FUB_USE_CASE_MASK_ALLOW_DRAM_CHIPID_READ)
    {
        CHECK_FUB_STATUS(FUB_ERR_HULK_ALLOW_DRAM_CHIPID_READ_BIT_ALREADY_SET_IN_MASK);
    }

    // Before Burning Fuse Verify DEVID check passes.
    CHECK_FUB_STATUS(fubCheckIfAllowingDramChipidReadIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));

    if(bIsFuseBurningApplicable)
    {
        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_ALLOW_DRAM_CHIPID_READ));
        // Proceed to Burning only if New fuseValue bits are not already burnt
        if (fuseInfo.fuseAdjustedBurlwalue)
        {
            // Burn Primary Fuse
            CHECK_FUB_STATUS(fubWriteFieldProgFuse_HAL(pFub, fuseInfo.fusePriAddress, fuseInfo.fuseAdjustedBurlwalue));

            // Burn Redundant Fuse
            CHECK_FUB_STATUS(fubWriteFieldProgFuse_HAL(pFub, fuseInfo.fuseRedAddress, fuseInfo.fuseAdjustedBurlwalue));

            //
            // If we reach this point then, fuse burn is successful,
            // Set use case specific bit in mask variable, for further reference.
            //
            *pfubUseCasMask = (*pfubUseCasMask | LW_FUB_USE_CASE_MASK_ALLOW_DRAM_CHIPID_READ);
        }
        else
        {
            // Ideally FUB should not reach here, since if condition is already checked in fubCheckIfFubNeedsToRun_* function.
            CHECK_FUB_STATUS(FUB_ERR_FUSE_BURN_NOT_REQUIRED);
        }
    }
    else
    {
        // Do not return error as FUB may be running for another use case.
        fubStatus = FUB_OK;
    }
ErrorExit:
    return fubStatus;
}
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_REVOKE_DRAM_CHIPID_READ_HULK_LICENSE))
/*
 * @brief: Check if fuse blocking HULK license to read DRAM CHIPID is to be burnt and Burn if check passes
 */
FUB_STATUS
fubCheckAndBurnBlockingDramChipidReadFuse_TU10X(LwU32 *pfubUseCasMask)
{
    LwBool        bIsFuseBurningApplicable = LW_FALSE;
    FUB_STATUS    fubStatus                = FUB_ERR_GENERIC;
    FPF_FUSE_INFO fuseInfo;

    // Return early if pointer passed as argument is NULL.
    if (pfubUseCasMask == NULL)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }

    // Return early if current use case specific bit is set in use case mask
    if (*pfubUseCasMask & LW_FUB_USE_CASE_MASK_BLOCK_DRAM_CHIPID_READ)
    {
        CHECK_FUB_STATUS(FUB_ERR_HULK_REVOKE_DRAM_CHIPID_READ_BIT_ALREADY_SET_IN_MASK);
    }

    // Before Burning Fuse Verify DEVID check passes.
    CHECK_FUB_STATUS(fubCheckIfBlockingDramChipidReadIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));

    if(bIsFuseBurningApplicable)
    {
        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_BLOCK_DRAM_CHIPID_READ));
        // Proceed to Burning only if New fuseValue bits are not already burnt
        if (fuseInfo.fuseAdjustedBurlwalue)
        {
            // Burn Primary Fuse
            CHECK_FUB_STATUS(fubWriteFieldProgFuse_HAL(pFub, fuseInfo.fusePriAddress, fuseInfo.fuseAdjustedBurlwalue));

            // Burn Redundant Fuse
            CHECK_FUB_STATUS(fubWriteFieldProgFuse_HAL(pFub, fuseInfo.fuseRedAddress, fuseInfo.fuseAdjustedBurlwalue));

            //
            // If we reach this point then, fuse burn is successful,
            // Set use case specific bit in mask variable, for further reference.
            //
            *pfubUseCasMask = (*pfubUseCasMask | LW_FUB_USE_CASE_MASK_BLOCK_DRAM_CHIPID_READ);
        }
        else
        {
            // Ideally FUB should not reach here, since if condition is already checked in fubCheckIfFubNeedsToRun_* function.
            CHECK_FUB_STATUS(FUB_ERR_FUSE_BURN_NOT_REQUIRED);
        }
    }
    else
    {
        // Do not return error as FUB may be running for another use case.
        fubStatus = FUB_OK;
    }
ErrorExit:
    return fubStatus;
}
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_ALLOW_DRAM_CHIPID_READ_HULK_LICENSE))
/*
 * @brief: FUB decides to burn fuse to allow DRAM CHIPID read based on DEVID of GPU and
 *         Use cases passed to FUB through REG Key
 *
 * @param[in] pbIsFuseBurningApplicable Set to true if check passes
 */
FUB_STATUS
fubCheckIfAllowingDramChipidReadIsApplicable_TU10X(LwBool *pbIsFuseBurningApplicable)
{
    FUB_STATUS    fubStatus       = FUB_OK;
    LwU32         fuseBar0Value   = 0;
    FPF_FUSE_INFO fuseInfo;

    if (pbIsFuseBurningApplicable == NULL)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }

    *pbIsFuseBurningApplicable = LW_FALSE;

    //
    // Check if use cases passed by RegKey includes burning fuse to allow DRAM CHIPID read
    // In Case of failure set *pbIsFuseBurningApplicable to LW_FALSE but do not return error status
    // as fuse burn decision is based on *pbIsFuseBurningApplicable and not on returned Error status.
    //
    if (!(g_desc.useCaseMask & LW_REG_STR_RM_FUB_USE_CASE_MASK_ALLOW_DRAM_CHIPID_READ_HULK))
    {
        *pbIsFuseBurningApplicable = LW_FALSE;
        fubStatus                  = FUB_OK;
        goto ErrorExit;
    }

    // Now Check if DEVID of GPU is in Approved list of this use case
    CHECK_FUB_STATUS(fubCheckDevIdForHulkToReadDramChipid_HAL(pFub, pbIsFuseBurningApplicable));

    if (*pbIsFuseBurningApplicable)
    {
        //
        // Sequence of FPF fuse burn and HULK usage will be below
        // 1.  Use FPF to set LW_FPF_OPT_FIELD_SPARE_FUSES_XX[0] to 1
        // 2.  Use HULK to read DRAM Chip ID. HULK will verify LW_FPF_OPT_FIELD_SPARE_FUSES_XX == 0x1 before changing any PLMs
        // 3.  Use FPF to set LW_FPF_OPT_FIELD_SPARE_FUSES_XX[1] to 1 . This will prevent HULK in step 2 from running again
        //
        // When FUB gets request to burn FPF fuse to Allow HULK license (In Step 1), FPF fuse Revoking HULK license
        // must not be be burnt (From Step 3). Return error if this condition is not met.
        //
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_BLOCK_DRAM_CHIPID_READ));
        CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, fuseInfo.fuseBar0Address, &fuseBar0Value));
        // Read Fuse value from PRIV address and return error if Fuse bit to block HULK is already burnt
        if ((fuseBar0Value & fuseInfo.fuseIntendedVal) == fuseInfo.fuseIntendedVal)
        {
            *pbIsFuseBurningApplicable = LW_FALSE;
            CHECK_FUB_STATUS(FUB_ERR_HULK_REVOKE_DRAM_CHIPID_READ_FUSE_ALREADY_BURNED);
        }
    }

ErrorExit:
    return fubStatus;
}
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_REVOKE_DRAM_CHIPID_READ_HULK_LICENSE))
/*
 * @brief: FUB decides to burn fuse to block DRAM CHIPID read based on DEVID of GPU and
 *         Use cases passed to FUB through REG Key
 *
 * @param[in] pbIsFuseBurningApplicable Set to true if check passes
 */
FUB_STATUS
fubCheckIfBlockingDramChipidReadIsApplicable_TU10X(LwBool *pbIsFuseBurningApplicable)
{
    FUB_STATUS    fubStatus       = FUB_OK;
    LwU32         fuseBar0Value   = 0;
    FPF_FUSE_INFO fuseInfo;

    if (pbIsFuseBurningApplicable == NULL)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }

    *pbIsFuseBurningApplicable = LW_FALSE;

    //
    // Check if use cases passed by RegKey includes burning fuse to block DRAM CHIPID read.
    // In Case of failure set *pbIsFuseBurningApplicable to LW_FALSE but do not return error status
    // as fuse burn decision is based on *pbIsFuseBurningApplicable and not on returned Error status.
    //
    if (!(g_desc.useCaseMask & LW_REG_STR_RM_FUB_USE_CASE_MASK_REVOKE_DRAM_CHIPID_READ_HULK))
    {
        *pbIsFuseBurningApplicable = LW_FALSE;
        fubStatus                  = FUB_OK;
        goto ErrorExit;
    }

    // Now Check if DEVID of GPU is in Approved list of this use case
    CHECK_FUB_STATUS(fubCheckDevIdForHulkToReadDramChipid_HAL(pFub, pbIsFuseBurningApplicable));

    if (*pbIsFuseBurningApplicable)
    {
        //
        // Sequence of FPF fuse burn and HULK usage will be below
        // 1.  Use FPF to set LW_FPF_OPT_FIELD_SPARE_FUSES_XX[0] to 1
        // 2.  Use HULK to read DRAM Chip ID. HULK will verify LW_FPF_OPT_FIELD_SPARE_FUSES_XX == 0x1 before changing any PLMs
        // 3.  Use FPF to set LW_FPF_OPT_FIELD_SPARE_FUSES_XX[1] to 1 . This will prevent HULK in step 2 from running again

        //
        // When FUB gets request to burn FPF fuse to Revoke HULK (In Step 3), FPF fuse Allowing HULK license must
        // be already be burnt (From Step 1). Return error if this condition is not met.
        //
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_ALLOW_DRAM_CHIPID_READ));
        CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, fuseInfo.fuseBar0Address, &fuseBar0Value));
        // Read Fuse value from PRIV address and return error if Fuse bit to allow HULK is not burnt
        if (!((fuseBar0Value & fuseInfo.fuseIntendedVal) == fuseInfo.fuseIntendedVal))
        {
            *pbIsFuseBurningApplicable = LW_FALSE;
             CHECK_FUB_STATUS(FUB_ERR_HULK_ALLOW_DRAM_CHIPID_READ_FUSE_NOT_BURNED_BEFORE);
        }
    }

ErrorExit:
    return fubStatus;
}
#endif

