/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "lwriscv/manifest_gh100.h"

const PKC_VERIFICATION_MANIFEST manifest = {
    // Decrytpted part of manifest
    .stage1RSA3KSigProd     = {0, },
    .stage1RSA3KSigDebug    = {0, },
    .magicNumberStage1      = 0x42,
    .bUseDevKey             = TRUE,
    .manifestEncParams      = {
        // MK TODO: check endiannes and if we care about that field on non-fsp
        .encryptionDerivationString = "",
        .iv                 = {0x69, 0xef, 0xd0, 0x27, 0x8a, 0xca, 0xbb, 0xc4,
                               0xcb, 0xc8, 0x37, 0x34, 0x04, 0xfc, 0xb8,0x8c},
        .authTag            = {0, },
    },

    .version               = 0x7,
    .ucodeId               = 0x4,
    .ucodeVersion          = 0x7,
    .bRelaxedVersionCheck  = TRUE,
    .engineIdMask          = ENGINE_TO_ENGINE_ID_MASK(FSP),
    // This field will be patched during build
    .itcmSizeIn256Bytes    = (LWRISCV_APP_IMEM_LIMIT) / 256,
    // This field will be patched during build
    .dtcmSizeIn256Bytes    = (LWRISCV_APP_DMEM_LIMIT) / 256,
    .fmcHashPadInfoBitMask = 0x00000000,
    .fmcEncParams          = {
        // NOT AUTOGENERATED! also must be !=0
        // MK TODO: check endiannes
        .encryptionDerivationString = "",
        .iv                 = {0x69, 0xef, 0xd0, 0x27, 0x8a, 0xca, 0xbb, 0xc4,
                               0xcb, 0xc8, 0x37, 0x34, 0x04, 0xfc, 0xb8,0x8c},

        .authTag            = {0, },
    },
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
    .bCertCA = FALSE,
    .bAttester = FALSE,
    .mspm  = {
        .mplm  = (LW_RISCV_CSR_MSPM_MPLM_LEVEL2 | LW_RISCV_CSR_MSPM_MPLM_LEVEL1),
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
        DEVICEMAP(_PMB,            _ENABLE,  _ENABLE,  _UNLOCKED) |
        DEVICEMAP(_DIO,            _ENABLE,  _ENABLE,  _UNLOCKED  ),

        DEVICEMAP(_KEY,            _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_DEBUG,          _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_SHA,            _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_KMEM,           _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_BROM,           _ENABLE,  _DISABLE, _LOCKED    ) | // BROM enforced
        DEVICEMAP(_ROM_PATCH,      _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_IOPMP,          _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_NOACCESS,       _DISABLE, _DISABLE, _UNLOCKED  ), // Empty Group

        DEVICEMAP(_SCP,            _ENABLE,  _ENABLE,  _UNLOCKED  ) |
        DEVICEMAP(_FBIF,           _ENABLE,  _ENABLE,  _UNLOCKED  ) |
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
        .cfg = {
            [0]=
            // PMPCFG
            PMP_ENTRY_OFF(0) | // TOR starting at 0
            PMP_ENTRY(_PERMITTED, _PERMITTED, _PERMITTED, _TOR,   _LOCK, 0), // TOR - everything - RWX
        },
        .addr = {
            // PMPADDR
            0x0,
            0xFFFFFFFFFFFFFFFF, // Entire PA
        },
    },

    // IO_PMP_MODE - NAPOT for all entries as that's the only mode we support
    .ioPmpMode = 0xffffffffffffffff,

    .ioPmp.entry = {
        [0] = { // IOPMPCFG(0) - explicit zero as and example
                .cfg     = 0,
                .usercfg = 0,
                .addrLo  = 0,
                .addrHi  = 0,
        },
        // Rest is unconfigured/zero
    },

    .numberOfValidPairs = 0x0,
    .registerPair.entries       = {
    },
};
