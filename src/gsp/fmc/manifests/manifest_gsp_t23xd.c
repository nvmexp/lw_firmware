/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "lwriscv/manifest.h"
#include "lwriscv/manifest_t23x.h"

// IOPMP ADDR_LO entry - config bits
// This should just be 1 macro? And also needs documentation on what its doing
#define IOPMPADDR_LO_ALL_IMEM IOPMPADDR_LO_IMEM(LW_RISCV_AMAP_IMEM_SIZE - 1)
#define IOPMPADDR_LO_ALL_DMEM IOPMPADDR_LO_DMEM(LW_RISCV_AMAP_DMEM_SIZE - 1)

const PKC_VERIFICATION_MANIFEST manifest = {
    .version               = 0x07,
    .ucodeId               = 0x04,
    .ucodeVersion          = 0x07,
    .bRelaxedVersionCheck  = TRUE,
    .engineIdMask          = 0xFFFFFFFF,
    // This field will be patched during build
    .itcmSizeIn256Bytes    = (LWRISCV_APP_IMEM_LIMIT) / 256,
    // This field will be patched during build
    .dtcmSizeIn256Bytes    = (LWRISCV_APP_DMEM_LIMIT) / 256,
    .fmcHashPadInfoBitMask = 0x00000000,
    // NOT AUTOGENERATED! also must be !=0
    .CbcIv                 = { 0x27d0ef69, 0xc4bbca8a, 0x3437c8cb, 0x8cb8fc04 },
    // This field will be patched by siggen
    .digest                = { 0x00000000, 0x00000000, 0x00000000, 0x00000000,
                               0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    .secretMask            = {
        .scpSecretMask     = 0x0000000000000000ULL,
        .scpSecretMaskLock = 0xFFFFFFFFFFFFFFFFULL,
    },
    .debugAccessControl    = {
        // Enable debugging for now, let Separation Kernel disable it if needed.
        .dbgctl =
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_STOP,   _ENABLE ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_RUN,    _ENABLE ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_STEP,   _ENABLE ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_J,      _ENABLE ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_EMASK,  _ENABLE ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_RREG,   _ENABLE ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_WREG,   _ENABLE ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_RDM,    _ENABLE ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_WDM,    _ENABLE ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_RSTAT,  _ENABLE ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_IBRKPT, _ENABLE ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_RCSR,   _ENABLE ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_WCSR,   _ENABLE ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_RPC,    _ENABLE ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_RFREG,  _ENABLE ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_WFREG,  _ENABLE ) |

            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _START_IN_ICD,     _FALSE  ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _SINGLE_STEP_MODE, _DISABLE),

        .dbgctlLock = 0x0,
    },
    .bDICE = FALSE,
    .bKDF  = FALSE,
    .mspm  = {
        .mplm  = (LW_RISCV_CSR_MSPM_MPLM_LEVEL3),
        .msecm = 0x0
    },

    .kdfConstant = { 0 },

    .deviceMap.deviceMap = {
        DEVICEMAP(_MMODE,          _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_RISCV_CTL,      _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_PIC,            _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_TIMER,          _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_HOSTIF,         _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_DMA,            _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        // PMB must be unlocked so someone can write LOCKPMB (and then disable/lock it)
        DEVICEMAP(_PMB,            _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_DIO,            _ENABLE,  _ENABLE,  _UNLOCKED  ),

        DEVICEMAP(_KEY,            _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_DEBUG,          _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_SHA,            _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_KMEM,           _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_BROM,           _ENABLE,  _DISABLE, _LOCKED  ) | // BROM enforced
        DEVICEMAP(_ROM_PATCH,      _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_IOPMP,          _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_NOACCESS,       _DISABLE, _DISABLE, _UNLOCKED  ), // Empty Group

        DEVICEMAP(_SCP,            _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        // @change_based_on_gsp  tsec does not have FBIF, it has TFBIF instead
        DEVICEMAP(_TFBIF,          _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_FALCON_ONLY,    _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_PRGN_CTL,       _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_SCRATCH_GROUP0, _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_SCRATCH_GROUP1, _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_SCRATCH_GROUP2, _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_SCRATCH_GROUP3, _ENABLE,  _ENABLE,  _UNLOCKED  ),

        DEVICEMAP(_PLM,            _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_HUB_DIO,        _ENABLE,  _ENABLE,  _UNLOCKED  ),  // Secure Bus - needed for HDCP
    },

    .corePmp = {
        .pmpcfg = {
            // PMPCFG
            PMP_ENTRY(_PERMITTED, _PERMITTED, _PERMITTED, _NAPOT, _LOCK), // IMEM - RWX
            PMP_ENTRY(_PERMITTED, _PERMITTED, _DENIED,    _NAPOT, _LOCK), // DMEM - RW
            PMP_ENTRY(_PERMITTED, _PERMITTED, _DENIED,    _NAPOT, _LOCK), // EMEM - RW
            PMP_ENTRY(_PERMITTED, _PERMITTED, _DENIED,    _NAPOT, _LOCK), // LOCALIO - RW
            PMP_ENTRY_OFF,                                                // TOR start for FBGPA (see next)
            PMP_ENTRY(_PERMITTED, _PERMITTED, _PERMITTED, _TOR,   _LOCK), // TOR - FBGPA - RWX
            0x0, 0x0,                               // 6-7
            0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, // 8-15
            // MEXTPMPCFG
            0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, // 16-23
            0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, // 24-31
        },
        .pmpaddr = {
            // PMPADDR
            0x000000000004FFFF, // IMEM
            0x000000000006FFFF, // DMEM
            0x000000000048FFFF, // EMEM
            ((LW_RISCV_AMAP_INTIO_START >> 2) | ((LW_RISCV_AMAP_INTIO_SIZE - 1) >> 3)), // LOCALIO
            (LW_RISCV_AMAP_EXTMEM1_START >> 2),     // FBGPA START
            (LW_RISCV_AMAP_EXTMEM1_END >> 2),       // FBGPA END
            0x0, 0x0,                               // 6-7
            0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, // 8-15
            // MEXTPMPADDR
            0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, // 16-23
            0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, // 24-31
        },
    },

    // IO_PMP_MODE - NAPOT for enabled entries
    .ioPmpMode = 0x3FFF,

    .ioPmp = {
        .iopmpcfg = {
            // Protect IMEM FMC
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _READ,   _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _WRITE,  _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER, _ALL_MASTERS_ENABLED) |
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _LOCK,   _LOCKED),

            // Protect DMEM FMC
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _READ,   _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _WRITE,  _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER, _ALL_MASTERS_ENABLED) |
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _LOCK,   _LOCKED),

            // Protect "Share" + "Secure" Partition IMEM ( 0x100000 ~ 0x106000 )
            // *WARNING* this must be updated when linker script layout changes
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _READ,   _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _WRITE,  _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER, _ALL_MASTERS_ENABLED) |
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _LOCK,   _LOCKED),

            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _READ,   _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _WRITE,  _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER, _ALL_MASTERS_ENABLED) |
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _LOCK,   _LOCKED),

            // Protect "Secure" Partition DMEM,
            // *WARNING* this must be updated when linker script layout changes
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _READ,   _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _WRITE,  _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER, _ALL_MASTERS_ENABLED) |
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _LOCK,   _LOCKED),

            // Protect "Shared code" IMEM - because it's used by secure partition,
            // It should be never touched by other masters
            // *WARNING* this must be updated when linker script layout changes
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _READ,   _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _WRITE,  _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER, _ALL_MASTERS_ENABLED) |
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _LOCK,   _LOCKED),

            // Protect "Shared data" DMEM - because it's used by secure partition,
            // It should be never touched by other masters
            // *WARNING* this must be updated when linker script layout changes
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _READ,   _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _WRITE,  _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER, _ALL_MASTERS_ENABLED) |
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _LOCK,   _LOCKED),

            // Permit read of DMESG buffer
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _READ,   _ENABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _WRITE,  _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER, _ALL_MASTERS_ENABLED) |
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _LOCK,   _LOCKED),
        },
        .iopmpaddrlo = {
            IOPMPADDR_LO_IMEM(LWRISCV_APP_IMEM_LIMIT - 1),
            IOPMPADDR_LO_DMEM(LWRISCV_APP_DMEM_LIMIT - 1),
            ((0x100000 >> 2) | ((0x4000 - 1) >> 3)), // partition_share_code, partition_hs_code. 0x106000 is not 0x4000 aligned(NAPOT) thus split to 2 entries.
            ((0x104000 >> 2) | ((0x2000 - 1) >> 3)),
            ((0x182000 >> 2) | ((0x2000 - 1) >> 3)),
            ((0x101000 >> 2) | ((0x1000 - 1) >> 3)),
            ((0x181000 >> 2) | ((0x1000 - 1) >> 3)),
            ((0x18F000 >> 2) | ((0x1000 - 1) >> 3))
        },
        .iopmpaddrhi = {
        },
    },

    .numberOfValidPairs = 0x0,
    .registerPair       = {
        .addr    = { },
        .andMask = { },
        .orMask  = { },
    },
};
