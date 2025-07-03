/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <falcon-intrinsics.h>
#include <lwmisc.h>

// include manuals
#include "dev_fbfalcon_csb.h"
#include "dev_top.h"
#include "dev_fuse.h"
#include "dev_top.h"
#include "dev_master.h"     // PMC

// project headers
#include "priv.h"
#include "fbflcn_defines.h"
#include "fbflcn_helpers.h"
#include "fbflcn_access.h"
#include "revocation.h"

#include "fbflcn_objfbflcn.h"
#include "config/g_fbfalcon_private.h"

#include "lwuproc.h"


extern LwU8 gPlatformType;

/*
 * CheckFusing
 * check for bad fusing as a replacement for checking the chain of trust from the code that bootloads
 * and starts the fbfalcon.
 */
void
fbfalconCheckFusing_GA102
(
        void
)
{
     if ((gPlatformType != LW_PTOP_PLATFORM_TYPE_FMODEL) && (gPlatformType != LW_PTOP_PLATFORM_TYPE_EMU))
     {
         LwU32 fuseOptSltDone = REG_RD32(BAR0, LW_FUSE_OPT_SLT_DONE);
         if( DRF_VAL(_FUSE_OPT,_SLT_DONE, _DATA, fuseOptSltDone) ==  LW_FUSE_OPT_SLT_DONE_DATA_ENABLE)
         {
             LwU32 priv_sec = REG_RD32(BAR0, LW_FUSE_OPT_PRIV_SEC_EN);
             if (FLD_TEST_DRF(_FUSE, _OPT_PRIV_SEC_EN, _DATA, _NO, priv_sec))
             {
                 FW_MBOX_WR32(12, fuseOptSltDone);
                 FW_MBOX_WR32(13, priv_sec);
                 FBFLCN_HALT(FBFLCN_ERROR_CODE_PRIV_SEC_NOT_ENABLED);
             }
         }
     }
}


void
fbfalconCheckRevocation_GA102
(
        void
)
{

#if (FBFALCONCFG_FEATURE_ENABLED(NO_RELEASE_REVOCATION_DISABLED))
    // disable revocation for ampere sim bringup
    return;
#endif

    LwU32 chipId;
    LwBool bUnsupportedChip = LW_TRUE;

    chipId = REG_RD32(BAR0, LW_PMC_BOOT_42);    // @0xa00
    chipId = DRF_VAL(_PMC,_BOOT_42,_CHIP_ID, chipId);

    switch (chipId)
    {
  #if (FBFALCONCFG_CHIP_ENABLED(GA10X) && FBFALCONCFG_FEATURE_ENABLED(SUPPORT_GDDR))
        case LW_PMC_BOOT_42_CHIP_ID_GA102:      // 0x172
        case LW_PMC_BOOT_42_CHIP_ID_GA103:      // 0x173
        case LW_PMC_BOOT_42_CHIP_ID_GA104:      // 0x174
        case LW_PMC_BOOT_42_CHIP_ID_GA106:      // 0x176
        case LW_PMC_BOOT_42_CHIP_ID_GA107:      // 0x177
        case LW_PMC_BOOT_42_CHIP_ID_GA114:      // 0x179
  #elif (FBFALCONCFG_CHIP_ENABLED(AD10X))
        case LW_PMC_BOOT_42_CHIP_ID_AD102:      // 0x192
        case LW_PMC_BOOT_42_CHIP_ID_AD103:      // 0x193
        case LW_PMC_BOOT_42_CHIP_ID_AD104:      // 0x194
        case LW_PMC_BOOT_42_CHIP_ID_AD106:      // 0x196
        case LW_PMC_BOOT_42_CHIP_ID_AD107:      // 0x197
  #endif

            bUnsupportedChip = LW_FALSE;
            break;
        default:
            bUnsupportedChip = LW_TRUE;
    }

    if (bUnsupportedChip)
    {
        FW_MBOX_WR32(9, chipId);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_CODE_REVOCATION_DEVID_ILWALID);
    }

    LwU32 device = chipId;



    //
    // REVOCATION for boottime training binary
    //
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_BOOT_TRAINING))

    // get revocation rev from SPARE 14-16
    LwU8 spare14 = REG_RD32(BAR0, LW_FUSE_SPARE_BIT_14) & 0x1;
    LwU8 spare15 = REG_RD32(BAR0, LW_FUSE_SPARE_BIT_15) & 0x1;
    LwU8 spare16 = REG_RD32(BAR0, LW_FUSE_SPARE_BIT_16) & 0x1;
    LwU8 bootFuseRevData = ((spare16 << 2) | (spare15 << 1) | (spare14));

    // check against BOOT_UCODE_BUILD_VERSION
    switch (device)
    {
  #if (FBFALCONCFG_CHIP_ENABLED(GA10X) && FBFALCONCFG_FEATURE_ENABLED(SUPPORT_GDDR))

        case LW_PMC_BOOT_42_CHIP_ID_GA102:
            if (FBFLCN_BOOT_UCODE_BUILD_VERSION_GA102 >= bootFuseRevData) { return; }
            break;
        case LW_PMC_BOOT_42_CHIP_ID_GA103:
            if (FBFLCN_BOOT_UCODE_BUILD_VERSION_GA103 >= bootFuseRevData) { return; }
            break;
        case LW_PMC_BOOT_42_CHIP_ID_GA104:
            if (FBFLCN_BOOT_UCODE_BUILD_VERSION_GA104 >= bootFuseRevData) { return; }
            break;
        case LW_PMC_BOOT_42_CHIP_ID_GA106:
            if (FBFLCN_BOOT_UCODE_BUILD_VERSION_GA106 >= bootFuseRevData) { return; }
            break;
        case LW_PMC_BOOT_42_CHIP_ID_GA107:
            if (FBFLCN_BOOT_UCODE_BUILD_VERSION_GA107 >= bootFuseRevData) { return; }
            break;
        case LW_PMC_BOOT_42_CHIP_ID_GA114:
            if (FBFLCN_BOOT_UCODE_BUILD_VERSION_GA114 >= bootFuseRevData) { return; }
            break;
  #elif (FBFALCONCFG_CHIP_ENABLED(AD10X))
        case LW_PMC_BOOT_42_CHIP_ID_AD102:
            if (FBFLCN_BOOT_UCODE_BUILD_VERSION_AD102 >= bootFuseRevData) { return; }
            break;
        case LW_PMC_BOOT_42_CHIP_ID_AD103:
            if (FBFLCN_BOOT_UCODE_BUILD_VERSION_AD103 >= bootFuseRevData) { return; }
            break;
        case LW_PMC_BOOT_42_CHIP_ID_AD104:
            if (FBFLCN_BOOT_UCODE_BUILD_VERSION_AD104 >= bootFuseRevData) { return; }
            break;
        case LW_PMC_BOOT_42_CHIP_ID_AD106:
            if (FBFLCN_BOOT_UCODE_BUILD_VERSION_AD106 >= bootFuseRevData) { return; }
            break;
        case LW_PMC_BOOT_42_CHIP_ID_AD107:
            if (FBFLCN_BOOT_UCODE_BUILD_VERSION_AD107 >= bootFuseRevData) { return; }
            break;
  #endif
    }

    // halt if no valid fuse/id pair was found or checked out positively
    FW_MBOX_WR32(10, device);
    FW_MBOX_WR32(13, bootFuseRevData);
    FBFLCN_HALT(FBFLCN_ERROR_CODE_CODE_REVOCATION_BOOT_FUSEREV_MISSMATCH);

#else

  //#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X) && FBFALCONCFG_CHIP_ENABLED(GA10X))
  #if FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)
    LwU8 runFPFRevData = REF_VAL(LW_FUSE_OPT_FPF_UCODE_FBFALCON_REV_DATA,
            REG_RD32(BAR0, LW_FUSE_OPT_FPF_UCODE_FBFALCON_REV));    // @0x824264

    LwU32 ucodeBuildVer = 0;
    // check against BOOT_UCODE_BUILD_VERSION
    switch (device)
    {
    #if (FBFALCONCFG_CHIP_ENABLED(GA10X) && FBFALCONCFG_FEATURE_ENABLED(SUPPORT_GDDR))
        case LW_PMC_BOOT_42_CHIP_ID_GA102:
            ucodeBuildVer = FBFLCN_BOOT_UCODE_BUILD_VERSION_GA102_FPF;
            break;
        case LW_PMC_BOOT_42_CHIP_ID_GA103:
            ucodeBuildVer = FBFLCN_BOOT_UCODE_BUILD_VERSION_GA103_FPF;
            break;
        case LW_PMC_BOOT_42_CHIP_ID_GA104:
            ucodeBuildVer = FBFLCN_BOOT_UCODE_BUILD_VERSION_GA104_FPF;
            break;
        case LW_PMC_BOOT_42_CHIP_ID_GA106:
            ucodeBuildVer = FBFLCN_BOOT_UCODE_BUILD_VERSION_GA106_FPF;
            break;
        case LW_PMC_BOOT_42_CHIP_ID_GA107:
            ucodeBuildVer = FBFLCN_BOOT_UCODE_BUILD_VERSION_GA107_FPF;
            break;
        case LW_PMC_BOOT_42_CHIP_ID_GA114:
            ucodeBuildVer = FBFLCN_BOOT_UCODE_BUILD_VERSION_GA114_FPF;
            break;
    #elif (FBFALCONCFG_CHIP_ENABLED(AD10X))
        case LW_PMC_BOOT_42_CHIP_ID_AD102:
            ucodeBuildVer = FBFLCN_BOOT_UCODE_BUILD_VERSION_AD102_FPF;
            break;
        case LW_PMC_BOOT_42_CHIP_ID_AD103:
            ucodeBuildVer = FBFLCN_BOOT_UCODE_BUILD_VERSION_AD103_FPF;
            break;
        case LW_PMC_BOOT_42_CHIP_ID_AD104:
            ucodeBuildVer = FBFLCN_BOOT_UCODE_BUILD_VERSION_AD104_FPF;
            break;
        case LW_PMC_BOOT_42_CHIP_ID_AD106:
            ucodeBuildVer = FBFLCN_BOOT_UCODE_BUILD_VERSION_AD106_FPF;
            break;
        case LW_PMC_BOOT_42_CHIP_ID_AD107:
            ucodeBuildVer = FBFLCN_BOOT_UCODE_BUILD_VERSION_AD107_FPF;
            break;
    #endif
        default:
            FBFLCN_HALT(FBFLCN_ERROR_CODE_CODE_REVOCATION_DEVID_ILWALID);
    }

    if (ucodeBuildVer < runFPFRevData)
    {
        FW_MBOX_WR32(12, ucodeBuildVer);
        FW_MBOX_WR32(13, runFPFRevData);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_CODE_REVOCATION_FPFFUSEREV_MISSMATCH);
    }
  #endif

    //
    // REVOCATION for runtime mclk switch binary
    //
    LwU32 optFuseRevData = REF_VAL(LW_FUSE_OPT_FUSE_UCODE_FBFALCON_REV_DATA,
            REG_RD32(BAR0, LW_FUSE_OPT_FUSE_UCODE_FBFALCON_REV));

    // check against UCODE_BUILD_VERSION
    switch (device)
    {

  #if (FBFALCONCFG_CHIP_ENABLED(GA10X) && FBFALCONCFG_FEATURE_ENABLED(SUPPORT_GDDR))

        case LW_PMC_BOOT_42_CHIP_ID_GA102:
            if (FBFLCN_UCODE_BUILD_VERSION_GA102 >= optFuseRevData) { return; }
            break;
        case LW_PMC_BOOT_42_CHIP_ID_GA103:
            if (FBFLCN_UCODE_BUILD_VERSION_GA103 >= optFuseRevData) { return; }
            break;
        case LW_PMC_BOOT_42_CHIP_ID_GA104:
            if (FBFLCN_UCODE_BUILD_VERSION_GA104 >= optFuseRevData) { return; }
            break;
        case LW_PMC_BOOT_42_CHIP_ID_GA106:
            if (FBFLCN_UCODE_BUILD_VERSION_GA106 >= optFuseRevData) { return; }
            break;
        case LW_PMC_BOOT_42_CHIP_ID_GA107:
            if (FBFLCN_UCODE_BUILD_VERSION_GA107 >= optFuseRevData) { return; }
            break;
        case LW_PMC_BOOT_42_CHIP_ID_GA114:
            if (FBFLCN_UCODE_BUILD_VERSION_GA114 >= optFuseRevData) { return; }
            break;
  #elif (FBFALCONCFG_CHIP_ENABLED(AD10X))
        case LW_PMC_BOOT_42_CHIP_ID_AD102:
            if (FBFLCN_UCODE_BUILD_VERSION_AD102 >= optFuseRevData) { return; }
            break;
        case LW_PMC_BOOT_42_CHIP_ID_AD103:
            if (FBFLCN_UCODE_BUILD_VERSION_AD103 >= optFuseRevData) { return; }
            break;
        case LW_PMC_BOOT_42_CHIP_ID_AD104:
            if (FBFLCN_UCODE_BUILD_VERSION_AD104 >= optFuseRevData) { return; }
            break;
        case LW_PMC_BOOT_42_CHIP_ID_AD106:
            if (FBFLCN_UCODE_BUILD_VERSION_AD106 >= optFuseRevData) { return; }
            break;
        case LW_PMC_BOOT_42_CHIP_ID_AD107:
            if (FBFLCN_UCODE_BUILD_VERSION_AD107 >= optFuseRevData) { return; }
            break;
  #endif

            // halt if no valid fuse/id pair was found or checked out positively
    }

    FW_MBOX_WR32(10, device);
    FW_MBOX_WR32(13, optFuseRevData);
    FBFLCN_HALT(FBFLCN_ERROR_CODE_CODE_REVOCATION_FUSEREV_MISSMATCH);
#endif

}
