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

void
fbfalconCheckRevocation_GH100
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
    case LW_PMC_BOOT_42_CHIP_ID_GH100:      // 0x180
    case LW_PMC_BOOT_42_CHIP_ID_GH100_SOC:  // 0x181
    case LW_PMC_BOOT_42_CHIP_ID_GH202:      // 0x182
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

    //
    // REVOCATION for boottime training binary
    //
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_BOOT_TRAINING))

    LwU32 device = chipId;

    LwU8 bootFuseRevData = REF_VAL(LW_FUSE_OPT_FPF_UCODE_FBFALCON_BOOT_REV_DATA,
                  REG_RD32(BAR0, LW_FUSE_OPT_FPF_UCODE_FBFALCON_BOOT_REV));

    // check against BOOT_UCODE_BUILD_VERSION
    switch (device)
    {
#if (FBFALCONCFG_CHIP_ENABLED(GH20X) && FBFALCONCFG_FEATURE_ENABLED(SUPPORT_GDDR))
    case LW_PMC_BOOT_42_CHIP_ID_GH202:
        if (FBFLCN_BOOT_UCODE_BUILD_VERSION_GH202 >= bootFuseRevData) { return; }
        break;
#elif (FBFALCONCFG_CHIP_ENABLED(GH10X) && FBFALCONCFG_FEATURE_ENABLED(SUPPORT_HBM))
    case LW_PMC_BOOT_42_CHIP_ID_GH100:
        if (FBFLCN_BOOT_UCODE_BUILD_VERSION_GH100 >= bootFuseRevData) { return; }
        break;
#endif
    }

    // halt if no valid fuse/id pair was found or checked out positively
    FW_MBOX_WR32(10, device);
    FW_MBOX_WR32(13, bootFuseRevData);
    FBFLCN_HALT(FBFLCN_ERROR_CODE_CODE_REVOCATION_BOOT_FUSEREV_MISSMATCH);

#else 
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_CORE_RUNTIME)) // runtime below

    LwU32 device = chipId;
    LwU8 runFPFRevData = REF_VAL(LW_FUSE_OPT_FPF_UCODE_FBFALCON_REV_DATA,
            REG_RD32(BAR0, LW_FUSE_OPT_FPF_UCODE_FBFALCON_REV));    // @0x824264

    LwU32 ucodeBuildVer = 0;
    // check against BOOT_UCODE_BUILD_VERSION
    switch (device)
    {
#if (FBFALCONCFG_CHIP_ENABLED(GH20X) && FBFALCONCFG_FEATURE_ENABLED(SUPPORT_GDDR))
    case LW_PMC_BOOT_42_CHIP_ID_GH202:
        ucodeBuildVer = FBFLCN_BOOT_UCODE_BUILD_VERSION_GH202_FPF;
        break;
#elif (FBFALCONCFG_CHIP_ENABLED(GH10X) && FBFALCONCFG_FEATURE_ENABLED(SUPPORT_HBM))
    case LW_PMC_BOOT_42_CHIP_ID_GH100:
        ucodeBuildVer = FBFLCN_BOOT_UCODE_BUILD_VERSION_GH100_FPF;
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
#endif

}
