/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <dev_pri_ringstation_sys.h>
#include "lwriscv/manifest_t23x.h"

// PMP-related defines

#define PMPADDR_NAPOT(START, NAPOT) (((START) >> 2) | ((NAPOT) >> 3))

#define PMPADDR_IMEM(NAPOT) PMPADDR_NAPOT(LW_RISCV_AMAP_IMEM_START, (NAPOT))
#define PMPADDR_DMEM(NAPOT) PMPADDR_NAPOT(LW_RISCV_AMAP_DMEM_START, (NAPOT))

// IOPMP-related defines

#define IOPMPADDR_LO_ALL_IMEM IOPMPADDR_LO_IMEM(LW_RISCV_AMAP_IMEM_SIZE - 1)
#define IOPMPADDR_LO_ALL_DMEM IOPMPADDR_LO_DMEM(LW_RISCV_AMAP_DMEM_SIZE - 1)

#define IOPMP_CFG_DISABLE_FOR_ALL_SOURCES \
    DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _READ,   _DISABLE)             | \
    DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _WRITE,  _DISABLE)             | \
    DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER, _ALL_MASTERS_ENABLED) | \
    DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _LOCK,   _LOCKED)

#define IS_IOPMP_NAPOT(a) (LW_IS_ALIGNED(a, a) && LW_IS_ALIGNED(a, 0x400)) // check min-1K-aligned NAPOT
#define IS_IOPMP_VAL(start, size)  (IS_IOPMP_NAPOT(size) && LW_IS_ALIGNED(start, size))

#define IOPMP_ALL_DISABLE_IMEM_START_REGION (SEPKERN_IMEM_LIMIT + LIBLWRISCV_IMEM_SIZE + TEGRA_ACR_IMEM_SIZE)
#define IOPMP_ALL_DISABLE_DMEM_START_REGION (SEPKERN_DMEM_LIMIT)

//
// ACR DMEM region, combined from RO and RW. RO goes before RW, RO must be
// NAPOT, and the sum must be NAPOT, this way they can be covered with NAPOT PMP
// entries (mostly needed for IOPMP)
//
#define IOPMP_DMEM_TEGRA_ACR_REGION_START   (LW_RISCV_AMAP_DMEM_START + SEPKERN_DMEM_LIMIT)
#define IOPMP_DMEM_TEGRA_ACR_REGION_SIZE    (TEGRA_ACR_DMEM_RO_SIZE + TEGRA_ACR_DMEM_SIZE)

//
// Liblwriscv is located after CheetAh-ACR.
// For DMEM, this can be useful if we expect it size to be less than the ACR
// data, since then we can map it with a separate IOPMP entry. This isn't used
// now though.
//
#define LIBLWRISCV_IMEM_START    (LW_RISCV_AMAP_IMEM_START + SEPKERN_IMEM_LIMIT + TEGRA_ACR_IMEM_SIZE)
#define LIBLWRISCV_DMEM_RO_START (LW_RISCV_AMAP_DMEM_START + SEPKERN_DMEM_LIMIT + TEGRA_ACR_DMEM_SIZE + TEGRA_ACR_DMEM_RO_SIZE)

const PKC_VERIFICATION_MANIFEST manifest = {
    .version               = 0x01,  // This always has to be 1
    .ucodeId               = 0x04,  // See https://confluence.lwpu.com/display/GFS/Ucode+ID+Assignment
    .ucodeVersion          = 0x02,  // Revoke initial prod signing
    .bRelaxedVersionCheck  = TRUE,  // Allow >= check for ucode version in BR revocation
    .engineIdMask          = ENGINE_TO_ENGINE_ID_MASK(PWR_PMU),
    .itcmSizeIn256Bytes    = (SEPKERN_IMEM_LIMIT) / 256,         // patched by siggen (value shown for clarity)
    .dtcmSizeIn256Bytes    = (SEPKERN_DMEM_LIMIT) / 256,         // patched by siggen (value shown for clarity)
    .fmcHashPadInfoBitMask = 0x00000000,                         // LSB determines if PDI is pre-pended to the FMC before hashing
    .CbcIv                 = { 0x00000000, 0x00000000, 0x00000000, 0x00000000, },   // patched by siggen
    .digest                = { 0x00000000, 0x00000000, 0x00000000, 0x00000000,      // patched by siggen
                               0x00000000, 0x00000000, 0x00000000, 0x00000000, },
    .secretMask            = {
        .scpSecretMask     = 0x0000000000000001ULL, // SCP 'secret' instruction is allowed (this is necessary to clear SCP regs in partitions which are SEC to have SCP lockdown)
        .scpSecretMaskLock = 0xFFFFFFFFFFFFFFFFULL, // This mask is locked and cannot be changed
    },
    .debugAccessControl    = {
        .dbgctl =
            //
            // Disable everything except for RSTAT and RPC, which can be left
            // enabled (via non-ICD direct RO register mappings) for the
            // RTOS partition.
            //
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_STOP,   _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_RUN,    _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_STEP,   _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_J,      _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_EMASK,  _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_RREG,   _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_WREG,   _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_RDM,    _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_WDM,    _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_RSTAT,  _ENABLE ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_IBRKPT, _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_RCSR,   _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_WCSR,   _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_RPC,    _ENABLE ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_RFREG,  _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _ICD_CMDWL_WFREG,  _DISABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _START_IN_ICD,     _FALSE  ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL, _SINGLE_STEP_MODE, _DISABLE),
        .dbgctlLock =
            //
            // Lock all the disabled controls.
            // These settings are the baseline, partitions can only be
            // more strict than this, not less.
            //
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _ICD_CMDWL_STOP,   _LOCKED  ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _ICD_CMDWL_RUN,    _LOCKED  ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _ICD_CMDWL_STEP,   _LOCKED  ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _ICD_CMDWL_J,      _LOCKED  ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _ICD_CMDWL_EMASK,  _LOCKED  ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _ICD_CMDWL_RREG,   _LOCKED  ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _ICD_CMDWL_WREG,   _LOCKED  ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _ICD_CMDWL_RDM,    _LOCKED  ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _ICD_CMDWL_WDM,    _LOCKED  ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _ICD_CMDWL_RSTAT,  _UNLOCKED) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _ICD_CMDWL_IBRKPT, _LOCKED  ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _ICD_CMDWL_RCSR,   _LOCKED  ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _ICD_CMDWL_WCSR,   _LOCKED  ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _ICD_CMDWL_RPC,    _UNLOCKED) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _ICD_CMDWL_RFREG,  _LOCKED  ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _ICD_CMDWL_WFREG,  _LOCKED  ) |
            DRF_DEF(_PRGNLCL, _RISCV_DBGCTL_LOCK, _SINGLE_STEP_MODE, _LOCKED  ),
    },
    .bDICE = FALSE,
    .bKDF  = FALSE, // PMU does not use fuse keys
    .mspm  = {
#if USE_PL0_CONFIG
        .mplm  = LW_RISCV_CSR_MSPM_MPLM_LEVEL0,
#else // USE_PL0_CONFIG
        .mplm  = LW_RISCV_CSR_MSPM_MPLM_LEVEL3, // PMU supports L3; main RTOS app runs at L2, but CheetAh-ACR partition runs at L3
#endif // USE_PL0_CONFIG
        .msecm = LW_RISCV_CSR_MSPM_MSECM_SEC,   // This allows secure SCPDMATRANSFER etc. writes. Needed due to SCP lockdown!
    },

    .kdfConstant = { 0 }, // PMU does not use KDF (no need for decrypting fuse key)

    .deviceMap.deviceMap = {
        //
        // Device map settings in manifest directly apply to M mode; since we don't expect to change these for M-mode, they all are locked.
        // This lock doesn't affect sub-m-mode.
        //
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
        DEVICEMAP(_HUB_DIO,        _DISABLE, _DISABLE, _LOCKED),      // PMU doesn't use DIO
    },

    .corePmp = {
        .pmpcfg = {
            // PMPCFG0
            PMP_ENTRY(_DENIED, _DENIED, _DENIED, _NAPOT, _UNLOCK), // IMEM FMC, access block for S/U *MUST BE UNLOCKED as unlock enables M access*
            PMP_ENTRY(_DENIED, _DENIED, _DENIED, _NAPOT, _UNLOCK), // DMEM FMC, access blocks for S/U *MUST BE UNLOCKED as unlock enables M access*
            PMP_ENTRY(_PERMITTED, _PERMITTED, _DENIED, _NAPOT, _LOCK), // LOCALIO, RW
            PMP_ENTRY(_PERMITTED, _PERMITTED, _DENIED, _NAPOT, _LOCK), // PRI, RW
            PMP_ENTRY_OFF, // TOR start for shared liblwriscv IMEM (see next)
            PMP_ENTRY(_DENIED, _DENIED, _PERMITTED, _TOR, _LOCK), // Shared liblwriscv IMEM, X only
#if (LIBLWRISCV_DMEM_RO_SIZE > 0)
            PMP_ENTRY(_PERMITTED, _DENIED, _DENIED, _NAPOT, _LOCK),
#else // (LIBLWRISCV_DMEM_RO_SIZE > 0)
            0x0, // 6
#endif // (LIBLWRISCV_DMEM_RO_SIZE > 0)
            0x0, // 7

            // Reserved for per-partition policy corepmp settings (see policy_pmu_ga10b_riscv.ads.in)
            // PMPCFG2
            0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, // 8-15
            // EXTPMPCFG0
            0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, // 16-23
            // End reserved (COREPMP_NUM_OF_COREPMP_CFG_ENTRY = 8, COREPMP_NUM_OF_EXT_COREPMP_CFG_ENTRY = 8, policy uses PMPCFG2+EXTPMPCFG0)

            // EXTPMPCFG2
            0x0, 0x0, 0x0, 0x0, 0x0, 0x0, // 24-29
            0x0, // Reserved and set by bootrom to cover its addrs
            0x0, // Reserved and set by bootrom to a deny-all entry
        },
        .pmpaddr = {
            // begin PMPADDR
            PMPADDR_IMEM(SEPKERN_IMEM_LIMIT - 1), // PMPADDR(0) - SK IMEM
            PMPADDR_DMEM(SEPKERN_DMEM_LIMIT - 1), // PMPADDR(1) - SK DMEM
            PMPADDR_NAPOT(LW_RISCV_AMAP_INTIO_START, (LW_RISCV_AMAP_INTIO_SIZE - 1)), // PMPADDR(2) - LOCALIO
            PMPADDR_NAPOT(LW_RISCV_AMAP_PRIV_START, (LW_RISCV_AMAP_PRIV_SIZE - 1)),   // PMPADDR(3) - PRI
            (LIBLWRISCV_IMEM_START >> 2),                             // PMPADDR(4) - Shared liblwriscv IMEM, TOR start
            ((LIBLWRISCV_IMEM_START + LIBLWRISCV_IMEM_SIZE) >> 2),    // PMPADDR(5) - Shared liblwriscv IMEM, TOR end
#if (LIBLWRISCV_DMEM_RO_SIZE > 0)
            PMPADDR_NAPOT(LIBLWRISCV_DMEM_RO_START, (LIBLWRISCV_DMEM_RO_SIZE - 1)), // PMPADDR(6) - Shared liblwriscv RO DMEM
#else // (LIBLWRISCV_DMEM_RO_SIZE > 0)
            0x0, // 6
#endif // (LIBLWRISCV_DMEM_RO_SIZE > 0)
            0x0, // 7

            // Reserved for per-partition policy corepmp settings (see policy_pmu_ga10b_riscv.ads.in)
            0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, // 8-15
            // begin EXTPMPADDR
            0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, // 16-23
            // End reserved (COREPMP_NUM_OF_COREPMP_CFG_ENTRY = 8, COREPMP_NUM_OF_EXT_COREPMP_CFG_ENTRY = 8, policy uses PMPCFG2+EXTPMPCFG0)

            0x0, 0x0, 0x0, 0x0, 0x0, 0x0, // 24-29
            0x0, // 30 - Reserved and set by bootrom to cover its addrs
            0x0, // 31 - Reserved and set by bootrom to a deny-all entry
        },
    },

    // IO_PMP_MODE. IOPMP is NAPOT for all entries that we use (only mode we support)
    .ioPmpMode =
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x00, _NAPOT) |
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x01, _NAPOT) |
#if (LIBLWRISCV_DMEM_RO_SIZE > 0)
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x02, _NAPOT) |
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x03, _NAPOT) |
#else // (LIBLWRISCV_DMEM_RO_SIZE > 0)
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x02, _OFF)   |
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x03, _OFF)   |
#endif // (LIBLWRISCV_DMEM_RO_SIZE > 0)
        DRF_IDX_DEF64(_PRGNLCL, _RISCV_IOPMP_MODE, _VAL_ENTRY, 0x04, _OFF)   |
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

    .ioPmp = {
        .iopmpcfg = {
            // IOPMPCFG(0) - block access to SK's IMEM, shared and CheetAh-ACR partition IMEM - All sources
            IOPMP_CFG_DISABLE_FOR_ALL_SOURCES,
            // IOPMPCFG(1) - block access to SK's DMEM - All sources
            IOPMP_CFG_DISABLE_FOR_ALL_SOURCES,
#if (LIBLWRISCV_DMEM_RO_SIZE > 0)
            // IOPMPCFG(2) - block shared liblwriscv DMEM R/O - PMB (must go before the entry that allows PMB read from all DMEM!)
            IOPMP_CFG_ALL_PMB(_DISABLE, _DISABLE, _LOCKED),
            // IOPMPCFG(3) - permit FBDMA and SCPDMA read and block write to shared liblwriscv DMEM R/O.
            // Must go before any permissive DMA entries that include this range!
            // (for example, RTOS needs an entry that allows DMA to whole DMEM range, so this has to be set before that).
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _READ, _ENABLE)         |
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _WRITE, _DISABLE)       |
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _LOCK, _LOCKED)         |
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_FBDMA, _ENABLE) |
            DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_CPDMA, _ENABLE),
#else // (LIBLWRISCV_DMEM_RO_SIZE > 0)
            0x0, 0x0, // 2-3
#endif // (LIBLWRISCV_DMEM_RO_SIZE > 0)
            0x0, 0x0, 0x0, 0x0, // 4-7

            // Reserved for per-partition policy iopmp settings (see policy_pmu_ga10b_riscv.ads.in)
            0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, // 8-15
            // End reserved (IOPMP_START_ENTRY = 8, IOPMP_NUM_OF_IO_PMP_CFG_ENTRY = 8)

            0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, // 16-23
            0x0, 0x0, 0x0, 0x0, 0x0, 0x0, // 24-29

            //
            // Next, default block access to IMEM and DMEM from everywhere;
            // the idea is that partitions will enable FBDMA, (S)CPDMA and PMB - accessible
            // regions of IMEM and DMEM in the partition policies.
            // For the other regions of memory, the IO sources can rely on their
            // built-in restrictions (e.g. PMB can only access TCM in the first
            // place).
            //

            // IOPMPCFG(30) - default block all IMEM - All sources
            IOPMP_CFG_DISABLE_FOR_ALL_SOURCES,
            // IOPMPCFG(31) - default block all DMEM - All sources
            IOPMP_CFG_DISABLE_FOR_ALL_SOURCES,
        },
        .iopmpaddrlo = {
            IOPMPADDR_LO_IMEM(IOPMP_ALL_DISABLE_IMEM_START_REGION - 1), // IOPMPADDR(0) - SK IMEM, CheetAh-ACR and shared partition IMEM
            IOPMPADDR_LO_DMEM(IOPMP_ALL_DISABLE_DMEM_START_REGION - 1), // IOPMPADDR(1) - SK DMEM
#if (LIBLWRISCV_DMEM_RO_SIZE > 0)
            PMPADDR_NAPOT(LIBLWRISCV_DMEM_RO_START, LIBLWRISCV_DMEM_RO_SIZE), // IOPMPADDR(2) - Shared liblwriscv RO DMEM
            PMPADDR_NAPOT(LIBLWRISCV_DMEM_RO_START, LIBLWRISCV_DMEM_RO_SIZE), // IOPMPADDR(3) - Shared liblwriscv RO DMEM
#else // (LIBLWRISCV_DMEM_RO_SIZE > 0)
            0x0, 0x0, // 2-3
#endif // (LIBLWRISCV_DMEM_RO_SIZE > 0)
            0x0, 0x0, 0x0, 0x0, // 4-7

            // Reserved for per-partition policy iopmp settings (see policy_pmu_ga10b_riscv.ads.in)
            0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, // 8-15
            // End reserved (IOPMP_START_ENTRY = 8, IOPMP_NUM_OF_IO_PMP_CFG_ENTRY = 8)

            0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, // 16-23
            0x0, 0x0, 0x0, 0x0, 0x0, 0x0, // 24-29
            IOPMPADDR_LO_ALL_IMEM,  // IOPMPADDR(30) - Map whole IMEM aperture
            IOPMPADDR_LO_ALL_DMEM,  // IOPMPADDR(31) - Map whole DMEM aperture
        },
        .iopmpaddrhi = { // Must be always 0 for IMEM and DMEM
            0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, // 0-7

            // Reserved for per-partition policy iopmp settings (see policy_pmu_ga10b_riscv.ads.in)
            0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, // 8-15
            // End reserved (IOPMP_START_ENTRY = 8, IOPMP_NUM_OF_IO_PMP_CFG_ENTRY = 8)

            0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, // 16-23
            0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, // 24-31
        },
    },

    .numberOfValidPairs = 0,  // no registers here, everything is configured by SK
    .registerPair       = {
        .addr    = { 0 },
        .andMask = { 0 },
        .orMask  = { 0 },
    },
};

_Static_assert(IS_IOPMP_NAPOT(IOPMP_ALL_DISABLE_IMEM_START_REGION), "IMEM start region total size must be 1K+ NAPOT to use in IOPMP!");
_Static_assert(IS_IOPMP_NAPOT(IOPMP_ALL_DISABLE_DMEM_START_REGION), "DMEM start region total size must be 1K+ NAPOT to use in IOPMP!");
_Static_assert(IS_IOPMP_VAL(IOPMP_DMEM_TEGRA_ACR_REGION_START, TEGRA_ACR_DMEM_RO_SIZE), "RO part of ACR DMEM region must be valid IOPMP!");
_Static_assert(IS_IOPMP_VAL(IOPMP_DMEM_TEGRA_ACR_REGION_START, IOPMP_DMEM_TEGRA_ACR_REGION_SIZE), "ACR DMEM region must be valid IOPMP!");
#if (LIBLWRISCV_DMEM_RO_SIZE > 0)
_Static_assert(IS_IOPMP_VAL(LIBLWRISCV_DMEM_RO_START, LIBLWRISCV_DMEM_RO_SIZE), "RO liblwriscv shared region must be valid IOPMP!");
#endif // (LIBLWRISCV_DMEM_RO_SIZE > 0)

// We set PLM for single PMB PLM
_Static_assert(LW_PRGNLCL_FALCON_PMB_IMEM_PRIV_LEVEL_MASK__SIZE_1 == 1, "LW_PRGNLCL_FALCON_PMB_IMEM_PRIV_LEVEL_MASK__SIZE_1 must be equal to 1!");

