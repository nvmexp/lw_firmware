/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"
/* ------------------------- Application Includes --------------------------- */
#include "sec2_objgfe.h"

#include "sha256.h"
#include "tsec_drv.h"

#include "dev_sec_csb.h"
#include "dev_fuse.h"
#include "dev_lw_xve.h"
#include "dev_master.h"
#include "sec2_objsec2.h"
#include "sec2_bar0.h"

typedef struct _def_ecid_content
{
    LwU32 lotCode0;
    LwU32 lotCode1;
    LwU32 fabCode;
    LwU32 xCoord;
    LwU32 yCoord;
    LwU32 waferId;
    LwU32 vendorCode;
} GFE_ECID_CONTENT;

/*!
 * @brief Reads ECID
 */
void
gfeReadDeviceInfo_GP10X
(
    gfe_devInfo *pDevinfo
)
{
    GFE_ECID_CONTENT ecid;
    LwU64            serialNum[2];
    LwU32            offset = 0;
    LwU32            val32;

    val32 = REG_RD32(BAR0, LW_FUSE_OPT_LOT_CODE_0);
    ecid.lotCode0   = DRF_VAL(_FUSE, _OPT_LOT_CODE_0, _DATA, val32);

    val32 = REG_RD32(BAR0, LW_FUSE_OPT_LOT_CODE_1);
    ecid.lotCode1   = DRF_VAL(_FUSE, _OPT_LOT_CODE_1, _DATA, val32);

    val32 = REG_RD32(BAR0, LW_FUSE_OPT_FAB_CODE);
    ecid.fabCode    = DRF_VAL(_FUSE, _OPT_FAB_CODE, _DATA, val32);

    val32 = REG_RD32(BAR0, LW_FUSE_OPT_X_COORDINATE);
    ecid.xCoord     = DRF_VAL(_FUSE, _OPT_X_COORDINATE, _DATA, val32);

    val32 = REG_RD32(BAR0, LW_FUSE_OPT_Y_COORDINATE);
    ecid.yCoord     = DRF_VAL(_FUSE, _OPT_Y_COORDINATE, _DATA, val32);

    val32 = REG_RD32(BAR0, LW_FUSE_OPT_WAFER_ID);
    ecid.waferId    = DRF_VAL(_FUSE, _OPT_WAFER_ID, _DATA, val32);

    val32 = REG_RD32(BAR0, LW_FUSE_OPT_VENDOR_CODE);
    ecid.vendorCode = DRF_VAL(_FUSE, _OPT_VENDOR_CODE, _DATA, val32);
    serialNum[0] = (LwU64) (ecid.vendorCode);
    offset = DRF_SIZE(LW_FUSE_OPT_VENDOR_CODE_DATA);        // +4 = 4

    serialNum[0] |= (LwU64) (ecid.fabCode) << offset;
    offset += DRF_SIZE(LW_FUSE_OPT_FAB_CODE_DATA);          // +6 = 10

    serialNum[0] |= (LwU64) (ecid.lotCode0) << offset;
    offset += DRF_SIZE(LW_FUSE_OPT_LOT_CODE_0_DATA);        // +32 = 42

    serialNum[0] |= (LwU64) (ecid.lotCode1) << offset;
    offset += DRF_SIZE(LW_FUSE_OPT_LOT_CODE_1_DATA);        // +28 = 70!
    offset -= 64;                                           // 6

    serialNum[1] = (LwU64) (ecid.lotCode1) >> (DRF_SIZE(LW_FUSE_OPT_LOT_CODE_1_DATA) - offset);

    serialNum[1] |= (LwU64) (ecid.waferId) << offset;
    offset += DRF_SIZE(LW_FUSE_OPT_WAFER_ID_DATA);          // +6 = 12

    serialNum[1] |= (LwU64) (ecid.xCoord) << offset;
    offset += DRF_SIZE(LW_FUSE_OPT_X_COORDINATE_DATA);      // +9 = 21

    serialNum[1] |= (LwU64) (ecid.yCoord) << offset;
    offset += DRF_SIZE(LW_FUSE_OPT_Y_COORDINATE_DATA);      // +9 = 30

    offset = LW_ALIGN_UP((offset + 64), 8); //make it byte aligned

    sha256((LwU8*)(pDevinfo->ecidSha2Hash), (LwU8*)serialNum, (offset / 8));

    val32 = REG_RD32(BAR0, DRF_BASE(LW_PCFG) + LW_XVE_ID);
    pDevinfo->vendorId     =  DRF_VAL(_XVE, _ID, _VENDOR, val32);

    val32 = REG_RD32(BAR0, DRF_BASE(LW_PCFG) + LW_XVE_SUBSYSTEM_ALIAS);
    pDevinfo->subSystemId  =  DRF_VAL(_XVE, _SUBSYSTEM_ALIAS, _ID, val32);

    val32 = REG_RD32(BAR0, DRF_BASE(LW_PCFG) + LW_XVE_SUBSYSTEM);
    pDevinfo->subVendorId  =  DRF_VAL(_XVE, _SUBSYSTEM, _VENDOR_ID, val32);

    val32 = REG_RD32(BAR0, DRF_BASE(LW_PCFG) + LW_XVE_ID);
    pDevinfo->deviceId     =  DRF_VAL(_XVE, _ID, _DEVICE_CHIP, val32);

    val32 = REG_RD32(BAR0, DRF_BASE(LW_PCFG) + LW_XVE_REV_ID);

    // revId is a combination of _FIB and _MASK fields
    pDevinfo->revId        =  DRF_VAL(_XVE, _REV_ID, _FIB, val32) |
                              (DRF_VAL(_XVE, _REV_ID, _MASK, val32) <<
                               DRF_SIZE(LW_XVE_REV_ID_FIB));

    val32 = REG_RD32(BAR0, LW_PMC_BOOT_42);
    pDevinfo->chipId       =  DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, val32);
}

