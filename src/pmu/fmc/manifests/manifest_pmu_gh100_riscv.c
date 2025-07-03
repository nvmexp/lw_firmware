/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <dev_pri_ringstation_sys.h>
#include "lwriscv/manifest_gh100.h"

#define DEBUG_BUFFER_START (LW_RISCV_AMAP_DMEM_START + PMU_DMEM_SIZE - DEBUG_BUFFER_SIZE)

// PMP-related defines

#define PMPADDR_NAPOT(START, NAPOT) (((START) >> 2) | ((NAPOT) >> 3))

#define PMPADDR_IMEM(NAPOT) PMPADDR_NAPOT(LW_RISCV_AMAP_IMEM_START, (NAPOT))
#define PMPADDR_DMEM(NAPOT) PMPADDR_NAPOT(LW_RISCV_AMAP_DMEM_START, (NAPOT))

// IOPMP-related defines

#define IOPMP_USERCFG_NO_RULE_CHECK                              \
    DRF_DEF(_PRGNLCL, _RISCV_IOPMP_USER_CFG, _MATCH, _INT)     | \
    DRF_DEF(_PRGNLCL, _RISCV_IOPMP_USER_CFG, _MASK,  _DISABLE)

#define IOPMPADDR_LO_ALL_IMEM IOPMPADDR_LO_IMEM(LW_RISCV_AMAP_IMEM_SIZE - 1)
#define IOPMPADDR_LO_ALL_DMEM IOPMPADDR_LO_DMEM(LW_RISCV_AMAP_DMEM_SIZE - 1)

#define IOPMP_CFG_DISABLE_FOR_ALL_SOURCES                                \
    DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _READ,   _DISABLE)             | \
    DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _WRITE,  _DISABLE)             | \
    DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER, _ALL_MASTERS_ENABLED) | \
    DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _LOCK,   _LOCKED)

#define IS_POW2(a)  LW_IS_ALIGNED(a, a)

#define IS_IOPMP_NAPOT(a)          (IS_POW2(a) && LW_IS_ALIGNED(a, 0x400)) // check min-1K-aligned NAPOT
#define IS_IOPMP_VAL(start, size)  (IS_IOPMP_NAPOT(size) && LW_IS_ALIGNED(start, size))

#define IS_COREPMP_NAPOT_VAL(start, size)  (IS_POW2(size) && LW_IS_ALIGNED(start, size))

const PKC_VERIFICATION_MANIFEST manifest = {
    // Decrytpted part of manifest
    .stage1RSA3KSigProd     = { 0 },
    .stage1RSA3KSigDebug    = { 0 },
    .magicNumberStage1      = 0x42,
    .bUseDevKey             = TRUE,
    .manifestEncParams      = {
        .encryptionDerivationString = "",
        .iv                 = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     // patched by siggen
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, },
        .authTag            = { 0 },
    },

    // Encrypted part of manifest (stage 1)
    .version               = 0x01,  // This always has to be 1
#if USE_PL0_CONFIG                  // See https://confluence.lwpu.com/display/GFS/Ucode+ID+Assignment#tab-GH100
    .ucodeId               = 0x03,  // inst-in-sys PMU-RTOS ucode ID
#else // USE_PL0_CONFIG
    .ucodeId               = 0x04,  // primary PMU-RTOS ucode ID
#endif // USE_PL0_CONFIG
    .ucodeVersion          = 0x01,  // ES signing
    .bRelaxedVersionCheck  = TRUE,  // Allow >= check for ucode version in BR revocation
    .engineIdMask          = ENGINE_TO_ENGINE_ID_MASK(PWR_PMU),
    .itcmSizeIn256Bytes    = (SEPKERN_IMEM_LIMIT) / 256,         // patched by siggen (value shown for clarity)
    .dtcmSizeIn256Bytes    = (SEPKERN_DMEM_LIMIT) / 256,         // patched by siggen (value shown for clarity)
    .fmcHashPadInfoBitMask = 0x00000000,                         // LSB determines if PDI is pre-pended to the FMC before hashing
    .fmcEncParams          = {
        .encryptionDerivationString = "",
        .iv                 = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     // patched by siggen
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, },
        .authTag            = { 0 },
    },
    .digest                = { 0x00000000, 0x00000000, 0x00000000, 0x00000000,      // patched by siggen
                               0x00000000, 0x00000000, 0x00000000, 0x00000000,
                               0x00000000, 0x00000000, 0x00000000, 0x00000000, },
    .secretMask            = {
        .scpSecretMask     = 0x0000000000000001ULL, // SCP 'secret' instruction is allowed (this is necessary to clear SCP regs in partitions which are SEC to have SCP lockdown)
        .scpSecretMaskLock = 0xFFFFFFFFFFFFFFFFULL, // This mask is locked and cannot be changed
    },
    .debugAccessControl    = {
        // See https://confluence.lwpu.com/display/LW/LWWATCH+Debugging+and+Security+-+GA10X+POR for reviewed POR debugging capabilities
        .dbgctl =
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_STOP,   _ENABLE ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_RUN,    _ENABLE ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_STEP,   _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_J,      _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_EMASK,  _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_RREG,   _ENABLE ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_WREG,   _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_RDM,    _ENABLE ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_WDM,    _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_RSTAT,  _ENABLE ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_IBRKPT, _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_RCSR,   _ENABLE ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_WCSR,   _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_RPC,    _ENABLE ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_RFREG,  _ENABLE ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_WFREG,  _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _START_IN_ICD,     _FALSE  ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _SINGLE_STEP_MODE, _DISABLE),
        .dbgctlLock =
            //
            // Lock all the disabled controls.
            // These settings are the baseline, partitions can only be
            // more strict than this, not less.
            //
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _ICD_CMDWL_STOP,   _UNLOCKED) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _ICD_CMDWL_RUN,    _UNLOCKED) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _ICD_CMDWL_STEP,   _LOCKED  ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _ICD_CMDWL_J,      _LOCKED  ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _ICD_CMDWL_EMASK,  _LOCKED  ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _ICD_CMDWL_RREG,   _UNLOCKED) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _ICD_CMDWL_WREG,   _LOCKED  ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _ICD_CMDWL_RDM,    _UNLOCKED) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _ICD_CMDWL_WDM,    _LOCKED  ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _ICD_CMDWL_RSTAT,  _UNLOCKED) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _ICD_CMDWL_IBRKPT, _LOCKED  ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _ICD_CMDWL_RCSR,   _UNLOCKED) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _ICD_CMDWL_WCSR,   _LOCKED  ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _ICD_CMDWL_RPC,    _UNLOCKED) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _ICD_CMDWL_RFREG,  _UNLOCKED) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _ICD_CMDWL_WFREG,  _LOCKED  ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _SINGLE_STEP_MODE, _LOCKED  ),
    },
    .bDICE = FALSE,
    .bKDF  = FALSE, // PMU does not use fuse keys
    .bCertCA = FALSE,
    .bAttester = FALSE,

    .mspm  = {
#if USE_PL0_CONFIG
        .mplm  = LW_RISCV_CSR_MSPM_MPLM_LEVEL0,
#else // USE_PL0_CONFIG
        .mplm  = LW_RISCV_CSR_MSPM_MPLM_LEVEL2, // PMU RTOS runs at L2.
#endif // USE_PL0_CONFIG
        .msecm = LW_RISCV_CSR_MSPM_MSECM_SEC,   // This allows secure SCPDMATRANSFER etc. writes. Needed due to SCP lockdown!
    },

    .kdfConstant = { 0 }, // PMU does not use KDF (no need for decrypting fuse key)

    .deviceMap.deviceMap = {
        DEVICEMAP(_MMODE,          _ENABLE,  _ENABLE,  _LOCKED) |     // Needed for SK but write-blocked for partitions
        DEVICEMAP(_RISCV_CTL,      _DISABLE, _DISABLE, _LOCKED) |     // Unused
        DEVICEMAP(_PIC,            _ENABLE,  _ENABLE,  _LOCKED) |
        DEVICEMAP(_TIMER,          _ENABLE,  _ENABLE,  _LOCKED) |
        DEVICEMAP(_HOSTIF,         _ENABLE,  _ENABLE,  _LOCKED) |
        DEVICEMAP(_DMA,            _ENABLE,  _ENABLE,  _LOCKED) |
        DEVICEMAP(_PMB,            _ENABLE,  _ENABLE,  _LOCKED) |     // PMB R/W must be enabled so someone can write LOCKPMB, b.c. PMU has an open DMEM aperture
        DEVICEMAP(_DIO,            _DISABLE, _DISABLE, _LOCKED),      // PMU doesn't use DIO

        DEVICEMAP(_KEY,            _DISABLE, _ENABLE,  _LOCKED) |     // PMU has no key access, but the SK needs this (write only)
        DEVICEMAP(_DEBUG,          _ENABLE,  _ENABLE,  _LOCKED) |
        DEVICEMAP(_SHA,            _DISABLE, _DISABLE, _LOCKED) |     // PMU does not use SHA engine
        DEVICEMAP(_KMEM,           _DISABLE, _DISABLE, _LOCKED) |     // PMU does not use KMEM
        DEVICEMAP(_BROM,           _ENABLE,  _DISABLE, _LOCKED) |     // Need read available to get BCR* DMA config (WPR ID, base addrs etc)
        DEVICEMAP(_ROM_PATCH,      _DISABLE, _DISABLE, _LOCKED) |     // Not used by PMU
        DEVICEMAP(_IOPMP,          _ENABLE,  _ENABLE,  _LOCKED) |     // Only used by SK to do IOPMP config, write must be disabled in partition policies
        DEVICEMAP(_NOACCESS,       _DISABLE, _DISABLE, _LOCKED),      // Empty Group

        DEVICEMAP(_SCP,            _ENABLE,  _ENABLE,  _LOCKED) |
        DEVICEMAP(_FBIF,           _ENABLE,  _ENABLE,  _LOCKED) |
        DEVICEMAP(_FALCON_ONLY,    _DISABLE, _DISABLE, _LOCKED) |     // Registers which only apply to Falcon part of Peregrine. These must be disabled!
        DEVICEMAP(_PRGN_CTL,       _ENABLE,  _ENABLE,  _LOCKED) |
        DEVICEMAP(_SCRATCH_GROUP0, _ENABLE,  _ENABLE,  _LOCKED) |
        DEVICEMAP(_SCRATCH_GROUP1, _ENABLE,  _ENABLE,  _LOCKED) |
        DEVICEMAP(_SCRATCH_GROUP2, _ENABLE,  _ENABLE,  _LOCKED) |
        DEVICEMAP(_SCRATCH_GROUP3, _ENABLE,  _ENABLE,  _LOCKED),

        DEVICEMAP(_PLM,            _ENABLE,  _ENABLE,  _LOCKED) |
        DEVICEMAP(_HUB_DIO,        _DISABLE, _DISABLE, _LOCKED) |     // PMU doesn't use DIO
        DEVICEMAP(_RESET,          _ENABLE,  _ENABLE,  _LOCKED),
    },

    .corePmp = {
        .cfg = {
            [0] = // PMP entry 0..7 -> PMPCFG0
            PMP_ENTRY(_DENIED,    _DENIED,    _DENIED,    _NAPOT, _UNLOCK, 0) | // IMEM FMC, access blocked for S/U *MUST BE UNLOCKED as unlock enables M access*
            PMP_ENTRY(_DENIED,    _DENIED,    _DENIED,    _NAPOT, _UNLOCK, 1) | // DMEM FMC, access blocked for S/U *MUST BE UNLOCKED as unlock enables M access*
            PMP_ENTRY(_PERMITTED, _PERMITTED, _DENIED,    _NAPOT, _LOCK,   2) | // LOCALIO, RW
            PMP_ENTRY(_PERMITTED, _PERMITTED, _DENIED,    _NAPOT, _LOCK,   3) | // PRI, RW
            PMP_ENTRY(_DENIED,    _DENIED,    _PERMITTED, _NAPOT, _LOCK,   4) | // IMEM, X
            PMP_ENTRY(_PERMITTED, _PERMITTED, _DENIED,    _NAPOT, _LOCK,   5) | // DMEM, RW
            PMP_ENTRY(_PERMITTED, _PERMITTED, _PERMITTED, _NAPOT, _LOCK,   6) | // FBGPA - RWX (due to bootloader)
#if USE_PL0_CONFIG
            PMP_ENTRY(_PERMITTED, _PERMITTED, _PERMITTED, _NAPOT, _LOCK,   7),  // SYSGPA - RWX (PL0 supports inst-in-sys mode, where bootloader will run from SYSGPA)
#else // USE_PL0_CONFIG
            PMP_ENTRY(_PERMITTED, _DENIED,    _DENIED,    _NAPOT, _LOCK,   7),  // SYSGPA - R (for non-PL0, bootloader still need R access to SYSGPA for bootargs)
#endif // USE_PL0_CONFIG

            // Reserved for per-partition policy corepmp settings (see policy_pmu_gh100_riscv.ads.in)
            [1] = 0, // PMP entry 8..15 -> PMPCFG2
            [2] = 0, // PMP entry 16..23 -> EXTPMPCFG0
            // End reserved (COREPMP_NUM_OF_COREPMP_CFG_ENTRY = 8, COREPMP_NUM_OF_EXT_COREPMP_CFG_ENTRY = 8, policy uses PMPCFG2+EXTPMPCFG0)

            [3] = 0, // PMP entry 24..31 -> EXTPMPCFG2
        },
        .addr = {
            // begin PMPADDR
            PMPADDR_IMEM(SEPKERN_IMEM_LIMIT - 1), // PMPADDR(0) - SK IMEM
            PMPADDR_DMEM(SEPKERN_DMEM_LIMIT - 1), // PMPADDR(1) - SK DMEM
            PMPADDR_NAPOT(LW_RISCV_AMAP_INTIO_START, (LW_RISCV_AMAP_INTIO_SIZE - 1)), // PMPADDR(2) - LOCALIO
            PMPADDR_NAPOT(LW_RISCV_AMAP_PRIV_START, (LW_RISCV_AMAP_PRIV_SIZE - 1)),   // PMPADDR(3) - PRI
            PMPADDR_IMEM(LW_RISCV_AMAP_IMEM_SIZE - 1), // PMPADDR(4) - PMU IMEM
            PMPADDR_DMEM(LW_RISCV_AMAP_DMEM_SIZE - 1), // PMPADDR(5) - PMU DMEM
            PMPADDR_NAPOT(LW_RISCV_AMAP_FBGPA_START, (LW_RISCV_AMAP_FBGPA_SIZE - 1)), // PMPADDR(6) - FBGPA
            PMPADDR_NAPOT(LW_RISCV_AMAP_SYSGPA_START, (LW_RISCV_AMAP_SYSGPA_SIZE - 1)), // PMPADDR(7) - SYSGPA

            // Reserved for per-partition policy corepmp settings (see policy_pmu_ga10b_riscv.ads.in)
            0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, // 8-15
            // begin EXTPMPADDR
            0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, // 16-23
            // End reserved (COREPMP_NUM_OF_COREPMP_CFG_ENTRY = 8, COREPMP_NUM_OF_EXT_COREPMP_CFG_ENTRY = 8, policy uses PMPCFG2+EXTPMPCFG0)

            0x0, 0x0, 0x0, 0x0, 0x0, 0x0, // 24-29
            0x0, // PMPADDR(30) - Reserved and set by bootrom to cover its addrs
            0x0, // PMPADDR(31) - Reserved and set by bootrom to a deny-all entry
        },
    },

    // IO_PMP_MODE. IOPMP is NAPOT for all entries that we use (only mode we support)
    .ioPmpMode =
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x00, _NAPOT) |
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x01, _NAPOT) |
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x02, _NAPOT) |
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x03, _NAPOT) |
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x04, _NAPOT) |
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x05, _OFF)   |
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x06, _OFF)   |
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x07, _OFF)   |
        // Begin reserved for per-partition policy iopmp settings (see policy_pmu_ga10b_riscv.ads.in)
        // Entries are disabled by default, each partition enables the entries that it needs.
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x08, _OFF)   |
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x09, _OFF)   |
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x0A, _OFF)   |
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x0B, _OFF)   |
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x0C, _OFF)   |
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x0D, _OFF)   |
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x0E, _OFF)   |
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x0F, _OFF)   |
        // End reserved (IOPMP_START_ENTRY = 8, IOPMP_NUM_OF_IO_PMP_CFG_ENTRY = 8)
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x10, _OFF)   |
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x11, _OFF)   |
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x12, _OFF)   |
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x13, _OFF)   |
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x14, _OFF)   |
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x15, _OFF)   |
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x16, _OFF)   |
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x17, _OFF)   |
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x18, _OFF)   |
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x19, _OFF)   |
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x1A, _OFF)   |
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x1B, _OFF)   |
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x1C, _OFF)   |
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x1D, _OFF)   |
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x1E, _NAPOT) |
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x1F, _NAPOT),

    .ioPmp.entry = {
        { // IOPMPCFG(0) - block access to SK IMEM - All sources
            .cfg     = IOPMP_CFG_DISABLE_FOR_ALL_SOURCES,
            .usercfg = IOPMP_USERCFG_NO_RULE_CHECK,
            .addrLo  = IOPMPADDR_LO_IMEM(SEPKERN_IMEM_LIMIT - 1),
            .addrHi  = LW_PRGNLCL_RISCV_IOPMP_ADDR_HI_VAL_INIT,
        },
        { // IOPMPCFG(1) - block access to SK DMEM - All sources
            .cfg     = IOPMP_CFG_DISABLE_FOR_ALL_SOURCES,
            .usercfg = IOPMP_USERCFG_NO_RULE_CHECK,
            .addrLo  = IOPMPADDR_LO_DMEM(SEPKERN_DMEM_LIMIT - 1),
            .addrHi  = LW_PRGNLCL_RISCV_IOPMP_ADDR_HI_VAL_INIT,
        },
        { // IOPMPCFG(2) - block whole IMEM R, allow W (minus SK IMEM above) - FBDMA (needed for BL and ODP)
            .cfg     = DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _READ,              _DISABLE) |
                       DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _WRITE,             _ENABLE)  |
                       DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_FBDMA_IMEM, _ENABLE)  |
                       DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _LOCK,              _LOCKED),
            .usercfg = IOPMP_USERCFG_NO_RULE_CHECK,
            .addrLo  = IOPMPADDR_LO_ALL_IMEM,
            .addrHi  = LW_PRGNLCL_RISCV_IOPMP_ADDR_HI_VAL_INIT,
        },
        { // IOPMPCFG(3) - allow whole DMEM R/W (minus SK DMEM above) - FBDMA, CPDMA
            .cfg     = DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _READ,              _ENABLE)  |
                       DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _WRITE,             _ENABLE)  |
                       DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_FBDMA_DMEM, _ENABLE)  |
                       DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_CPDMA,      _ENABLE)  |
                       DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _LOCK,              _LOCKED),
            .usercfg = IOPMP_USERCFG_NO_RULE_CHECK,
            .addrLo  = IOPMPADDR_LO_ALL_DMEM,
            .addrHi  = LW_PRGNLCL_RISCV_IOPMP_ADDR_HI_VAL_INIT,
        },
        { // IOPMPCFG(4) - allow debug buffer DMEM R - PMB
            .cfg     = IOPMP_CFG_ALL_PMB(_ENABLE, _DISABLE, _LOCKED),
            .usercfg = IOPMP_USERCFG_NO_RULE_CHECK,
            .addrLo  = PMPADDR_NAPOT(DEBUG_BUFFER_START, DEBUG_BUFFER_SIZE - 1),
            .addrHi  = LW_PRGNLCL_RISCV_IOPMP_ADDR_HI_VAL_INIT,
        },
        { 0 }, { 0 }, { 0 }, // 5-7

        // Reserved for per-partition policy iopmp settings (see policy_pmu_ga10b_riscv.ads.in)
        { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, // 8-15
        // End reserved (IOPMP_START_ENTRY = 8, IOPMP_NUM_OF_IO_PMP_CFG_ENTRY = 8)

        { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, // 16-23
        { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, // 24-29

        { // IOPMPCFG(30) - default block all IMEM - All sources
            .cfg     = IOPMP_CFG_DISABLE_FOR_ALL_SOURCES,
            .usercfg = IOPMP_USERCFG_NO_RULE_CHECK,
            .addrLo  = IOPMPADDR_LO_ALL_IMEM,
            .addrHi  = LW_PRGNLCL_RISCV_IOPMP_ADDR_HI_VAL_INIT,
        },
        { // IOPMPCFG(31) - default block all DMEM - All sources
            .cfg     = IOPMP_CFG_DISABLE_FOR_ALL_SOURCES,
            .usercfg = IOPMP_USERCFG_NO_RULE_CHECK,
            .addrLo  = IOPMPADDR_LO_ALL_IMEM,
            .addrHi  = LW_PRGNLCL_RISCV_IOPMP_ADDR_HI_VAL_INIT,
        },
    },

#if USE_PL0_CONFIG
    .numberOfValidPairs      = 1,  // 1 register (no PLMs)
    .registerPair.entries    = {
        [0] = {
            .addr    = LW_PRGNLCL_FBIF_CTL2,
            .andMask = 0,
            .orMask  = DRF_DEF(_PRGNLCL, _FBIF_CTL2, _NACK_MODE, _NACK_AS_ACK),
        },
    },
#else // USE_PL0_CONFIG
    .numberOfValidPairs      = 27,  // 27 registers
    .registerPair.entries    = {
        [0] = {
            .addr    = LW_PRGNLCL_FBIF_CTL2,
            .andMask = 0,
            .orMask  = DRF_DEF(_PRGNLCL, _FBIF_CTL2, _NACK_MODE, _NACK_AS_ACK),
        },
        // PLMs, see https://confluence.lwpu.com/pages/viewpage.action?pageId=481960351
        // PLMs that are fully disabled
        [1] = {
            .addr    = LW_PRGNLCL_FALCON_AMAP_PRIV_LEVEL_MASK,
            .andMask = 0,
            .orMask  = PLM_ALL_DISABLE_WITH_REPORT_ERROR(FALCON_AMAP       ),
        },
        [2] = {
            .addr    = LW_PRGNLCL_FALCON_BOOTVEC_PRIV_LEVEL_MASK,
            .andMask = 0,
            .orMask  = PLM_ALL_DISABLE_WITH_REPORT_ERROR(FALCON_BOOTVEC    ),
        },
        [3] = {
            .addr    = LW_PRGNLCL_FALCON_CPUCTL_PRIV_LEVEL_MASK,
            .andMask = 0,
            .orMask  = PLM_ALL_DISABLE_WITH_REPORT_ERROR(FALCON_CPUCTL     ),
        },
        [4] = {
            .addr    = LW_PRGNLCL_FALCON_DBGCTL_PRIV_LEVEL_MASK,
            .andMask = 0,
            .orMask  = PLM_ALL_DISABLE_WITH_REPORT_ERROR(FALCON_DBGCTL     ),
        },
        [5] = {
            .addr    = LW_PRGNLCL_FALCON_PMB_IMEM_PRIV_LEVEL_MASK(0), // This is indexed, but one element, ct_assert at the end
            .andMask = 0,
            .orMask  = PLM_ALL_DISABLE_WITH_REPORT_ERROR(FALCON_PMB_IMEM   ),
        },
        [6] = {
            .addr    = LW_PRGNLCL_FALCON_PRIVSTATE_PRIV_LEVEL_MASK,
            .andMask = 0,
            .orMask  = PLM_ALL_DISABLE_WITH_REPORT_ERROR(FALCON_PRIVSTATE  ),
        },
        [7] = {
            .addr    = LW_PRGNLCL_FALCON_SAFETY_CTRL_PRIV_LEVEL_MASK,
            .andMask = 0,
            .orMask  = PLM_ALL_DISABLE_WITH_REPORT_ERROR(FALCON_SAFETY_CTRL),
        },
        [8] = {
            .addr    = LW_PRGNLCL_FALCON_SCTL_PRIV_LEVEL_MASK,
            .andMask = 0,
            .orMask  = PLM_ALL_DISABLE_WITH_REPORT_ERROR(FALCON_SCTL       ),
        },
        [9] = {
            .addr    = LW_PRGNLCL_FALCON_TRACEBUF_PRIV_LEVEL_MASK,
            .andMask = 0,
            .orMask  = PLM_ALL_DISABLE_WITH_REPORT_ERROR(FALCON_TRACEBUF   ),
        },
        [10] = {
            .addr    = LW_PRGNLCL_RISCV_BOOTVEC_PRIV_LEVEL_MASK,
            .andMask = 0,
            .orMask  = PLM_ALL_DISABLE_WITH_REPORT_ERROR(RISCV_BOOTVEC     ),
        },
        //
        // PLMs that are accessed by RISC-V but can be fully disabled, because
        // RISC-V bypassess PLM checks
        //
        [11] = {
            .addr    = LW_PRGNLCL_FALCON_DIODT_PRIV_LEVEL_MASK,
            .andMask = 0,
            .orMask  = PLM_ALL_DISABLE_WITH_REPORT_ERROR(FALCON_DIODT  ),
        },
        [12] = {
            .addr    = LW_PRGNLCL_FALCON_SHA_RAL_PRIV_LEVEL_MASK,
            .andMask = 0,
            .orMask  = PLM_ALL_DISABLE_WITH_REPORT_ERROR(FALCON_SHA_RAL),
        },
        [13] = {
            .addr    = LW_PRGNLCL_FALCON_TMR_PRIV_LEVEL_MASK,
            .andMask = 0,
            .orMask  = PLM_ALL_DISABLE_WITH_REPORT_ERROR(FALCON_TMR    ),
        },
        [14] = {
            .addr    = LW_PRGNLCL_FALCON_WDTMR_PRIV_LEVEL_MASK,
            .andMask = 0,
            .orMask  = PLM_ALL_DISABLE_WITH_REPORT_ERROR(FALCON_WDTMR  ),
        },
        [15] = {
            .addr    = LW_PRGNLCL_RISCV_MSIP_PRIV_LEVEL_MASK,
            .andMask = 0,
            .orMask  = PLM_ALL_DISABLE_WITH_REPORT_ERROR(RISCV_MSIP    ),
        },
        [16] = {
            .addr    = LW_PRGNLCL_RISCV_LWCONFIG_PRIV_LEVEL_MASK,
            .andMask = 0,
            .orMask  = PLM_ALL_DISABLE_WITH_REPORT_ERROR(RISCV_LWCONFIG),
        },
        // Those have SELF L0 access because DMA does post level check
        [17] = {
            .addr    = LW_PRGNLCL_FALCON_IMEM_PRIV_LEVEL_MASK,
            .andMask = 0,
            .orMask  = PLM_SET_SOURCE_WITH_REPORT_ERROR(FALCON_IMEM,       PLM_ALL_LEVELS, PLM_ALL_LEVELS, PLM_SOURCE_ID(PMU)),
        },
        [18] = {
            .addr    = LW_PRGNLCL_FALCON_DMEM_PRIV_LEVEL_MASK,
            .andMask = 0,
            .orMask  = PLM_SET_SOURCE_WITH_REPORT_ERROR(FALCON_DMEM,       PLM_ALL_LEVELS, PLM_ALL_LEVELS, PLM_SOURCE_ID(PMU)),
        },
        // PLMs that are programmed to values
        [19] = {
            .addr    = LW_PRGNLCL_FBIF_REGIONCFG_PRIV_LEVEL_MASK,
            .andMask = 0,
            .orMask  = PLM_SET_ALL_SOURCES_WITH_REPORT_ERROR(FBIF_REGIONCFG,   PLM_LEVEL3,     PLM_LEVEL3          ), // ACR needs it
        },
        [20] = {
            .addr    = LW_PRGNLCL_FALCON_DMA_PRIV_LEVEL_MASK,
            .andMask = 0,
            .orMask  = PLM_SET_ALL_SOURCES_WITH_REPORT_ERROR(FALCON_DMA,       PLM_ALL_LEVELS, PLM_LEVEL2_AND_ABOVE),
        },
        [21] = {
            .addr    = LW_PRGNLCL_FALCON_EXE_PRIV_LEVEL_MASK,
            .andMask = 0,
            .orMask  = PLM_SET_ALL_SOURCES_WITH_REPORT_ERROR(FALCON_EXE,       PLM_ALL_LEVELS, PLM_LEVEL2_AND_ABOVE),
        },
        [22] = {
            .addr    = LW_PRGNLCL_FALCON_IRQSCMASK_PRIV_LEVEL_MASK,
            .andMask = 0,
            .orMask  = PLM_SET_ALL_SOURCES_WITH_REPORT_ERROR(FALCON_IRQSCMASK, PLM_ALL_LEVELS, PLM_LEVEL2_AND_ABOVE),
        },
        [23] = {
            .addr    = LW_PRGNLCL_FALCON_IRQTMR_PRIV_LEVEL_MASK,
            .andMask = 0,
            .orMask  = PLM_SET_ALL_SOURCES_WITH_REPORT_ERROR(FALCON_IRQTMR,    PLM_ALL_LEVELS, PLM_LEVEL2_AND_ABOVE),
        },
        [24] = {
            .addr    = LW_PRGNLCL_FBIF_CTL2_PRIV_LEVEL_MASK,
            .andMask = 0,
            .orMask  = PLM_SET_ALL_SOURCES_WITH_REPORT_ERROR(FBIF_CTL2,        PLM_ALL_LEVELS, PLM_LEVEL2_AND_ABOVE),
        },
        [25] = {
            .addr    = LW_PRGNLCL_RISCV_DBGCTL_PRIV_LEVEL_MASK,
            .andMask = 0,
            .orMask  = PLM_SET_ALL_SOURCES_WITH_REPORT_ERROR(RISCV_DBGCTL,     PLM_ALL_LEVELS, PLM_LEVEL3          ),
        },
        [26] = {
            .addr    = LW_PRGNLCL_RISCV_IRQ_PRIV_LEVEL_MASK,
            .andMask = 0,
            .orMask  = PLM_SET_ALL_SOURCES_WITH_REPORT_ERROR(RISCV_IRQ,        PLM_ALL_LEVELS, PLM_LEVEL3          ),
        },
    },
#endif // USE_PL0_CONFIG
};

_Static_assert(IS_COREPMP_NAPOT_VAL(LW_RISCV_AMAP_INTIO_START, LW_RISCV_AMAP_INTIO_SIZE), "LOCALIO must be valid NAPOT!");
_Static_assert(IS_COREPMP_NAPOT_VAL(LW_RISCV_AMAP_PRIV_START, LW_RISCV_AMAP_PRIV_SIZE), "PRI must be valid NAPOT!");
_Static_assert(IS_COREPMP_NAPOT_VAL(LW_RISCV_AMAP_IMEM_START, LW_RISCV_AMAP_IMEM_SIZE), "PMU IMEM must be valid NAPOT!");
_Static_assert(IS_COREPMP_NAPOT_VAL(LW_RISCV_AMAP_DMEM_START, LW_RISCV_AMAP_DMEM_SIZE), "PMU DMEM must be valid NAPOT!");
_Static_assert(IS_COREPMP_NAPOT_VAL(LW_RISCV_AMAP_SYSGPA_START, LW_RISCV_AMAP_SYSGPA_SIZE), "SYSGPA must be valid NAPOT!");
_Static_assert(IS_COREPMP_NAPOT_VAL(LW_RISCV_AMAP_FBGPA_START, LW_RISCV_AMAP_FBGPA_SIZE), "FBGPA must be valid NAPOT!");
_Static_assert(IS_IOPMP_VAL(DEBUG_BUFFER_START, DEBUG_BUFFER_SIZE), "Debug buffer must be valid IOPMP!");

// We set PLM for single PMB PLM
_Static_assert(LW_PRGNLCL_FALCON_PMB_IMEM_PRIV_LEVEL_MASK__SIZE_1 == 1, "LW_PRGNLCL_FALCON_PMB_IMEM_PRIV_LEVEL_MASK__SIZE_1 must be equal to 1!");
