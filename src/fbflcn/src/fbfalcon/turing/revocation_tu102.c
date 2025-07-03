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

// project headers
#include "priv.h"
#include "fbflcn_defines.h"
#include "fbflcn_helpers.h"
#include "fbflcn_access.h"
#include "revocation.h"

#include "fbflcn_objfbflcn.h"
#include "config/g_fbfalcon_private.h"

#include "lwuproc.h"

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GH10X))
// Hopper retires Gen4 PCIe and its manuals (bug: 3164274)
#else
// xve is used to detect chip ip for revocation up to Turing
#include "dev_lw_xve.h"
#endif

extern LwU8 gPlatformType;

void
fbfalconCheckRevocation_TU102
(
        void
)
{


#if (FBFALCONCFG_FEATURE_ENABLED(NO_RELEASE_REVOCATION_DISABLED))
    // disable revocation for ampere sim bringup
    return;
#endif


    // lookup devId's
    LwU32 devIdA = REG_RD32(BAR0, LW_FUSE_OPT_PCIE_DEVIDA);
    LwU32 devIdB = REG_RD32(BAR0, LW_FUSE_OPT_PCIE_DEVIDB);

    // if both devid's are zero check LW_XVE_ID   0x88000 (tu102)
    // on our bringup boards tu102 this yields 0x1e2e
    if ((devIdA == 0) && (devIdB == 0))
    {
        // LW_XVE_ID is defined in a different domain, translation with
        // device base should fix this,  (compile assert if picking up
        // untranslated adr which is 0x0)
        LwU32 xveId = REG_RD32(BAR0, DEVICE_BASE(LW_PCFG) + LW_XVE_ID);
        CASSERT(((DEVICE_BASE(LW_PCFG) + LW_XVE_ID) != 0x0),revocation_c);

        devIdA = DRF_VAL ( _XVE, _ID, _DEVICE_CHIP, xveId );
    }

    // detect device independently for each id
    LwU32 deviceA = 0;
    LwU32 deviceB = 0;

    CHECK_RANGE(A, VGA_TU102,   TU102);
    CHECK_RANGE(A, VGA_TU102_B, TU102);
    CHECK_RANGE(B, VGA_TU102,   TU102);
    CHECK_RANGE(B, VGA_TU102_B, TU102);

    CHECK_RANGE(A, VGA_TU104,   TU104);
    CHECK_RANGE(A, VGA_TU104_B, TU104);
    CHECK_RANGE(B, VGA_TU104,   TU104);
    CHECK_RANGE(B, VGA_TU104_B, TU104);

    CHECK_RANGE(A, VGA_TU106,   TU106);
    CHECK_RANGE(A, VGA_TU106_B, TU106);
    CHECK_RANGE(B, VGA_TU106,   TU106);
    CHECK_RANGE(B, VGA_TU106_B, TU106);

    CHECK_RANGE(A, VGA_TU116,   TU116);
    CHECK_RANGE(A, VGA_TU116_B, TU116);
    CHECK_RANGE(B, VGA_TU116,   TU116);
    CHECK_RANGE(B, VGA_TU116_B, TU116);

    CHECK_RANGE(A, VGA_TU117,   TU117);
    CHECK_RANGE(A, VGA_TU117_B, TU117);
    CHECK_RANGE(B, VGA_TU117,   TU117);
    CHECK_RANGE(B, VGA_TU117_B, TU117);


  #if (FBFALCONCFG_FEATURE_ENABLED(SUPPORT_GDDR))
    REG_WR32(CSB, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_0(2),0x00000005);
  #endif
    // aggregate to final device ID
    LwU32 device = 0;
    if ((deviceA == deviceB) && (deviceA != 0))
    {
        device = deviceA;
    }
    else if ((deviceA != 0) && (deviceB == 0))
    {
        device = deviceA;
    }
    else if ((deviceB != 0) && (deviceA == 0))
    {
        device = deviceB;
    }
    else
    {
        FW_MBOX_WR32(9, deviceA);
        FW_MBOX_WR32(10, deviceB);
        FW_MBOX_WR32(11, device);
        FW_MBOX_WR32(12, devIdA);
        FW_MBOX_WR32(13, devIdB);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_CODE_REVOCATION_DEVID_ILWALID);
    }


    //
    // REVOCATION for boottime training binary
    //
    //
    //
    // Note: the define for CORE_BOOT does not apply for the turing automotive binary which has CORE_AUTOMOTIVE_BOOT enabled
    //       so rather than brining this over to JOB_BOOT_TRAINING as in ga10x I left as is to avoid impact on release binary.    
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_CORE_BOOT))

    // get revocation rev from SPARE 14-16
    LwU8 spare14 = REG_RD32(BAR0, LW_FUSE_SPARE_BIT_14) & 0x1;
    LwU8 spare15 = REG_RD32(BAR0, LW_FUSE_SPARE_BIT_15) & 0x1;
    LwU8 spare16 = REG_RD32(BAR0, LW_FUSE_SPARE_BIT_16) & 0x1;
    LwU8 bootFuseRevData = ((spare16 << 2) | (spare15 << 1) | (spare14));

    // check against BOOT_UCODE_BUILD_VERSION
    switch (device)
    {
        case FBFLCN_DEVICE_ID_TU102:
            if (FBFLCN_BOOT_UCODE_BUILD_VERSION_TU102 >= bootFuseRevData) { return; }
            break;
        case FBFLCN_DEVICE_ID_TU104:
            if (FBFLCN_BOOT_UCODE_BUILD_VERSION_TU104 >= bootFuseRevData) { return; }
            break;
        case FBFLCN_DEVICE_ID_TU106:
            if (FBFLCN_BOOT_UCODE_BUILD_VERSION_TU106 >= bootFuseRevData) { return; }
            break;
        case FBFLCN_DEVICE_ID_TU116:
            if (FBFLCN_BOOT_UCODE_BUILD_VERSION_TU116 >= bootFuseRevData) { return; }
            break;
        case FBFLCN_DEVICE_ID_TU117:
            if (FBFLCN_BOOT_UCODE_BUILD_VERSION_TU117 >= bootFuseRevData) { return; }
            break;
    }

    // halt if no valid fuse/id pair was found or checked out positively

    FW_MBOX_WR32(8, deviceA);
    FW_MBOX_WR32(9, deviceB);
    FW_MBOX_WR32(10, device);
    FW_MBOX_WR32(11, devIdA);
    FW_MBOX_WR32(12, devIdB);
    FW_MBOX_WR32(13, bootFuseRevData);
    FBFLCN_HALT(FBFLCN_ERROR_CODE_CODE_REVOCATION_BOOT_FUSEREV_MISSMATCH);

#else


    //
    // REVOCATION for runtime mclk switch binary
    //
    LwU32 optFuseRevData = REF_VAL(LW_FUSE_OPT_FUSE_UCODE_FBFALCON_REV_DATA,
            REG_RD32(BAR0, LW_FUSE_OPT_FUSE_UCODE_FBFALCON_REV));

    // check against UCODE_BUILD_VERSION
    switch (device)
    {
        case FBFLCN_DEVICE_ID_TU102:
            if (FBFLCN_UCODE_BUILD_VERSION_TU102 >= optFuseRevData) { return; }
            break;
        case FBFLCN_DEVICE_ID_TU104:
            if (FBFLCN_UCODE_BUILD_VERSION_TU104 >= optFuseRevData) { return; }
            break;
        case FBFLCN_DEVICE_ID_TU106:
            if (FBFLCN_UCODE_BUILD_VERSION_TU106 >= optFuseRevData) { return; }
            break;
        case FBFLCN_DEVICE_ID_TU116:
            if (FBFLCN_UCODE_BUILD_VERSION_TU116 >= optFuseRevData) { return; }
            break;
        case FBFLCN_DEVICE_ID_TU117:
            if (FBFLCN_UCODE_BUILD_VERSION_TU117 >= optFuseRevData) { return; }
            break;

            // halt if no valid fuse/id pair was found or checked out positively
    }

    FW_MBOX_WR32(8, deviceA);
    FW_MBOX_WR32(9, deviceB);
    FW_MBOX_WR32(10, device);
    FW_MBOX_WR32(11, devIdA);
    FW_MBOX_WR32(12, devIdB);
    FW_MBOX_WR32(13, optFuseRevData);
    FBFLCN_HALT(FBFLCN_ERROR_CODE_CODE_REVOCATION_FUSEREV_MISSMATCH);
#endif

}
