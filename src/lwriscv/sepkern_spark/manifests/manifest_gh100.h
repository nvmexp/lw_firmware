/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef __MANIFEST_GH100_H_
#define __MANIFEST_GH100_H_

////////////////////////////////////////////////////////////////////////
// BEGIN of paste of manifest.h
// We can't use manifest.h as some structures has changed.
////////////////////////////////////////////////////////////////////////


#include <lwmisc.h>
#include <lwtypes.h>
#include <lwctassert.h>
#include "riscv_manuals.h"
// For PLM engine ID's
#include <dev_pri_ringstation_sys.h>

//
// Synced on 2020-01-30 to
// https://confluence.lwpu.com/display/Peregrine/GA10x+LWRISCV+BootROM+Design+Spec
//

#define MANIFEST_SIZE                           (2560)

#define MAX_NUM_OF_REGISTERPAIR_ENTRY           32
#define MAX_NUM_OF_DEVICEMAP_ENTRY              32
#define MAX_NUM_OF_COREPMP_ENTRY_IN_MANIFEST    32
#define MAX_NUM_OF_IOPMP_ENTRY_IN_MANIFEST      32

typedef struct
{
    LwU64 scpSecretMask;         // LW_PRGNLCL_RISCV_SCP_SECRET_MASK(0...1)
    LwU64 scpSecretMaskLock;     // LW_PRGNLCL_RISCV_SCP_SECRET_MASK_LOCK(0...1)
} MANIFEST_SECRET;
ct_assert(sizeof(MANIFEST_SECRET) == 16);

typedef struct
{
    LwU32 dbgctl;        // LW_PRGNLCL_RISCV_DBGCTL, allowed ICD commands
    LwU32 dbgctlLock;    // LW_PRGNLCL_RISCV_DBGCTL_LOCK, locks out write access to corresponding bits in LW_PRGNLCL_RISCV_DBGCTL
} MANIFEST_DEBUG;
ct_assert(sizeof(MANIFEST_DEBUG) == 8);

typedef struct
{
    LwU8 mplm;   // M mode PL mask. Only the lowest 4 bits can be non-zero
                    // bit 0 - indicates if PL0 is enabled (1) or disabled (0) in M mode. It is a read-only bit in HW so it has to be set here in the manifest
                    // bit 1 - indicates if PL1 is enabled (1) or disabled (0) in M mode.
                    // bit 2 - indicates if PL2 is enabled (1) or disabled (0) in M mode.
                    // bit 3 - indicates if PL3 is enabled (1) or disabled (0) in M mode.
    LwU8 msecm;  // M mode SCP transaction indicator. Only the lowest bit can be non-zero
                    // bit 0 - indicates if secure transaction is enabled (1) or disabled (0) in M mode.
} MANIFEST_MSPM;
ct_assert(sizeof(MANIFEST_MSPM) == 2);

typedef struct
{
    LwU32 deviceMap[MAX_NUM_OF_DEVICEMAP_ENTRY/8];
} MANIFEST_DEIVCEMAP;
ct_assert(sizeof(MANIFEST_DEIVCEMAP) == 16);

typedef struct
{
    LwU64 cfg[MAX_NUM_OF_COREPMP_ENTRY_IN_MANIFEST / 8];   // pmpcfg0 and pmpcfg2
    LwU64 addr[MAX_NUM_OF_COREPMP_ENTRY_IN_MANIFEST]; // pmpaddr0 to pmpaddr15
} MANIFEST_COREPMP;
ct_assert(sizeof(MANIFEST_COREPMP) == 288);

typedef struct
{
    LwU32 cfg;
    LwU32 usercfg;
    LwU32 addrLo;
    LwU32 addrHi;
} MANIFEST_IOPMP_ENTRY;

typedef struct
{
    MANIFEST_IOPMP_ENTRY entry[MAX_NUM_OF_IOPMP_ENTRY_IN_MANIFEST];
} MANIFEST_IOPMP;
ct_assert(sizeof(MANIFEST_IOPMP) == 512);

typedef struct
{
    LwU64 addr;
    LwU32 andMask;
    LwU32 orMask;
} MANIFEST_REGISTERPAIR_ENTRY;

typedef struct
{
    MANIFEST_REGISTERPAIR_ENTRY entries[MAX_NUM_OF_REGISTERPAIR_ENTRY];
} MANIFEST_REGISTERPAIR;
ct_assert(sizeof(MANIFEST_REGISTERPAIR) == 512);

//
// Helper macros
//

#define TRUE            0xAA
#define FALSE           0x55

// Colwert engine id to engineIdMask field, use with engine name (PWR_PMU, GSP, ..)
#define ENGINE_TO_ENGINE_ID_MASK(X) (1 << ( (LW_PRGNLCL_FALCON_ENGID_FAMILYID_ ##X) - 1))

// Generate device map entry
#define DEVICEMAP(group, read, write, lock) \
    DRF_IDX_DEF(_PRGNLCL, _RISCV_DEVICEMAP_RISCVMMODE, _READ , LW_PRGNLCL_DEVICE_MAP_GROUP ##group,  read) | \
    DRF_IDX_DEF(_PRGNLCL, _RISCV_DEVICEMAP_RISCVMMODE, _WRITE, LW_PRGNLCL_DEVICE_MAP_GROUP ##group, write) | \
    DRF_IDX_DEF(_PRGNLCL, _RISCV_DEVICEMAP_RISCVMMODE, _LOCK , LW_PRGNLCL_DEVICE_MAP_GROUP ##group,  lock)

// Generate PMPCFG
#define PMP_ENTRY(R, W, X, ADDRMODE, LOCK, IDX) \
    ((DRF_DEF64(_RISCV, _CSR_PMPCFG0, _PMP0R, R) | \
    DRF_DEF64(_RISCV, _CSR_PMPCFG0, _PMP0W, W) | \
    DRF_DEF64(_RISCV, _CSR_PMPCFG0, _PMP0X, X) | \
    DRF_DEF64(_RISCV, _CSR_PMPCFG0, _PMP0A, ADDRMODE) | \
    DRF_DEF64(_RISCV, _CSR_PMPCFG0, _PMP0L, LOCK)) << (IDX) * 8) // MK TODO: this is not very pretty :(

#define PMP_ENTRY_OFF(IDX) \
    PMP_ENTRY(_DENIED, _DENIED, _DENIED, _OFF, _LOCK, IDX)

// Generate iopmp config entry
#define IOPMP_CFG_ALL_PMB(read, write, lock) \
    DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _READ, read) | \
    DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _WRITE, write) | \
    DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _LOCK, lock) | \
    DRF_IDX_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_PMB, 0 , _ENABLE) | \
    DRF_IDX_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_PMB, 1 , _ENABLE) | \
    DRF_IDX_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_PMB, 2 , _ENABLE) | \
    DRF_IDX_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_PMB, 3 , _ENABLE)

// IOPMP ADDR_LO entry - config bits
// This should just be 1 macro? And also needs documentation on what its doing
#define IOPMPADDR_LO_NAPOT(START, NAPOT) (((START) >> 2) | ((NAPOT) >> 3))
#define IOPMPADDR_LO_IMEM(NAPOT) IOPMPADDR_LO_NAPOT(LW_RISCV_AMAP_IMEM_START, (NAPOT))
#define IOPMPADDR_LO_DMEM(NAPOT) IOPMPADDR_LO_NAPOT(LW_RISCV_AMAP_DMEM_START, (NAPOT))

#define IOPMPADDR_LO_ALL_IMEM IOPMPADDR_LO_IMEM(LW_RISCV_AMAP_IMEM_SIZE - 1)
#define IOPMPADDR_LO_ALL_DMEM IOPMPADDR_LO_DMEM(LW_RISCV_AMAP_DMEM_SIZE - 1)

// Aperture sizes must be power of 2 or NAPOT "generation" will fail
ct_assert((LW_RISCV_AMAP_IMEM_SIZE != 0) && ((LW_RISCV_AMAP_IMEM_SIZE & (LW_RISCV_AMAP_IMEM_SIZE - 1)) == 0));
ct_assert((LW_RISCV_AMAP_DMEM_SIZE != 0) && ((LW_RISCV_AMAP_DMEM_SIZE & (LW_RISCV_AMAP_DMEM_SIZE - 1)) == 0));

// Macro for LEVEL2+ ENABLE PLM (using PRGNLCL AMAP PLM as proxy for the field values)
#define PLM_ALL_LEVELS \
    DRF_DEF(_PRGNLCL_FALCON, _AMAP_PRIV_LEVEL_MASK, _READ_PROTECTION, _ALL_LEVELS_ENABLED)

#define PLM_LEVEL2_AND_ABOVE \
    DRF_DEF(_PRGNLCL_FALCON, _AMAP_PRIV_LEVEL_MASK, _READ_PROTECTION_LEVEL2, _ENABLE)      | \
    DRF_DEF(_PRGNLCL_FALCON, _AMAP_PRIV_LEVEL_MASK, _READ_PROTECTION_LEVEL3, _ENABLE)

#define PLM_LEVEL3 \
    DRF_DEF(_PRGNLCL_FALCON, _AMAP_PRIV_LEVEL_MASK, _READ_PROTECTION, _ONLY_LEVEL3_ENABLED)

// No one can access given PLM group (except for RISC-V)
#define PLM_SOURCE_ID_NONE      0
#define PLM_SOURCE_ID(ENGINE)   (LW_PPRIV_SYS_PRI_SOURCE_ID_ ## ENGINE)


// For PLMs, the fully disabled PLMs still have some defaults
#define PLM_ALL_DISABLE_WITH_REPORT_ERROR(name) \
    DRF_DEF(_PRGNLCL, _##name##_PRIV_LEVEL_MASK, _READ_PROTECTION, _ALL_LEVELS_DISABLED)     | \
    DRF_DEF(_PRGNLCL, _##name##_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _ALL_LEVELS_DISABLED)    | \
    DRF_DEF(_PRGNLCL, _##name##_PRIV_LEVEL_MASK, _READ_VIOLATION, _REPORT_ERROR)             | \
    DRF_DEF(_PRGNLCL, _##name##_PRIV_LEVEL_MASK, _WRITE_VIOLATION, _REPORT_ERROR)            | \
    DRF_DEF(_PRGNLCL, _##name##_PRIV_LEVEL_MASK, _SOURCE_READ_CONTROL, _BLOCKED)             | \
    DRF_DEF(_PRGNLCL, _##name##_PRIV_LEVEL_MASK, _SOURCE_WRITE_CONTROL, _BLOCKED)            | \
    DRF_NUM(_PRGNLCL, _##name##_PRIV_LEVEL_MASK, _SOURCE_ENABLE, PLM_SOURCE_ID_NONE)

// Programming PLM
#define PLM_SET_ALL_SOURCES_WITH_REPORT_ERROR(name, read, write) \
    DRF_NUM(_PRGNLCL, _##name##_PRIV_LEVEL_MASK, _READ_PROTECTION, read)             | \
    DRF_NUM(_PRGNLCL, _##name##_PRIV_LEVEL_MASK, _WRITE_PROTECTION, write)           | \
    DRF_DEF(_PRGNLCL, _##name##_PRIV_LEVEL_MASK, _READ_VIOLATION, _REPORT_ERROR)     | \
    DRF_DEF(_PRGNLCL, _##name##_PRIV_LEVEL_MASK, _WRITE_VIOLATION, _REPORT_ERROR)    | \
    DRF_DEF(_PRGNLCL, _##name##_PRIV_LEVEL_MASK, _SOURCE_READ_CONTROL, _BLOCKED)     | \
    DRF_DEF(_PRGNLCL, _##name##_PRIV_LEVEL_MASK, _SOURCE_WRITE_CONTROL, _BLOCKED)    | \
    DRF_DEF(_PRGNLCL, _##name##_PRIV_LEVEL_MASK, _SOURCE_ENABLE, _ALL_SOURCES_ENABLED)

#define PLM_SET_SOURCE_WITH_REPORT_ERROR(name, read, write, source) \
    DRF_NUM(_PRGNLCL, _##name##_PRIV_LEVEL_MASK, _READ_PROTECTION, read)             | \
    DRF_NUM(_PRGNLCL, _##name##_PRIV_LEVEL_MASK, _WRITE_PROTECTION, write)           | \
    DRF_DEF(_PRGNLCL, _##name##_PRIV_LEVEL_MASK, _READ_VIOLATION, _REPORT_ERROR)     | \
    DRF_DEF(_PRGNLCL, _##name##_PRIV_LEVEL_MASK, _WRITE_VIOLATION, _REPORT_ERROR)    | \
    DRF_DEF(_PRGNLCL, _##name##_PRIV_LEVEL_MASK, _SOURCE_READ_CONTROL, _BLOCKED)     | \
    DRF_DEF(_PRGNLCL, _##name##_PRIV_LEVEL_MASK, _SOURCE_WRITE_CONTROL, _BLOCKED)    | \
    DRF_NUM(_PRGNLCL, _##name##_PRIV_LEVEL_MASK, _SOURCE_ENABLE, source)

//
// Sanity compile-time checks
//

// Check if device map groupings are sane
ct_assert(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_READ__SIZE_1  == 8);
ct_assert(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_WRITE__SIZE_1 == 8);
ct_assert(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_LOCK__SIZE_1  == 8);

// Check that we have 8 PMB masters
ct_assert(LW_PRGNLCL_RISCV_IOPMP_CFG_MASTER_PMB__SIZE_1 == 4);

// Check if FMC sizes are aligned to 256
ct_assert(((IMEM_LIMIT) & 0xFF) == 0);
ct_assert(((DMEM_LIMIT) & 0xFF) == 0);

// Check if FMC sizes are > 0
ct_assert((DMEM_LIMIT) >= 0);
ct_assert((IMEM_LIMIT) >= 0);

// Check if FMC sizes are power of 2
ct_assert(((IMEM_LIMIT) & ((IMEM_LIMIT) - 1)) == 0);
ct_assert(((DMEM_LIMIT) & ((DMEM_LIMIT) - 1)) == 0);

/////////////////////////////////////
// End of paste of manifest.h
////////////////////////////////////
//
// Synced on 2020-10-28 to
// https://confluence.lwpu.com/display/FalconSelwrityIPUserSpace/GH100+RISCV+BootROM+Design+Spec#GH100RISCVBootROMDesignSpec-Stage1PKCBOOTParameters1
//

typedef struct
{
    LwU8 encryptionDerivationString[8];  // Part of KDF label to derive decryption key
    LwU8 iv[16];                         // IV for decryption (only 96 bits used for AES-256-GCM)
    LwU8 authTag[16];                    // GCM authentication tag (unused in non-GCM modes)
} MANIFEST_CRYPTO_ENCRYPTION_PARAMS;
ct_assert(sizeof(MANIFEST_CRYPTO_ENCRYPTION_PARAMS) == 40);

typedef struct
{
    // Stage 1 part of manifest, unencrypted
    LwU32 stage1RSA3KSigProd[96];        // patched by siggen, production signed RSASSA3k-PSS signature
    LwU32 stage1RSA3KSigDebug[96];       // patched by siggen, debug signed RSASSA3k-PSS signature


    // stage1 signed unencrypted section, 128 bytes
    LwU32 magicNumberStage1;             // Can be used by SW for anything (ignored by bootrom)
    LwU8 bUseDevKey;                     // Switch to use debug or prod key (on debug boards)
    LwU8 reservedBytes0[19];
    MANIFEST_CRYPTO_ENCRYPTION_PARAMS manifestEncParams;
    LwU8 reservedBytes1[64];

    // Stage 1, encrypted section
    LwU8 version;                        // [MISC] version of the manifest; it needs to be 0x1 in GA10x/T234
    LwU8 ucodeId;                        // [ucode revocation] ucode ID
    LwU8 ucodeVersion;                   // [ucode revocation] ucode version
    LwU8 bRelaxedVersionCheck;           // [ucode revocation] if true, allows a greater-or-equal-to check on ucodeVersion (false is defined as 0x55; true is defined as 0xAA; other values will be reported as an error)
    LwU32 engineIdMask;                  // [ucode revocation] engine ID mask
    LwU16 itcmSizeIn256Bytes;            // [FMC related] FMC's code starts at offset 0 inside IMEM; the sizes need to be a multiple of 256 bytes; the blobs need to be 0-padded.
    LwU16 dtcmSizeIn256Bytes;            // [FMC related] FMC's data starts at offset 0 inside DMEM; the sizes need to be a multiple of 256 bytes; the blobs need to be 0-padded.
    LwU32 fmcHashPadInfoBitMask;         // [FMC related] in GA10x the LSb indicates whether the 64-bit PDI is prepended to FMC code/data when callwlating the digest; all other bits must be 0s
    LwU8 swEncryptionDeviationString[8]; // [exelwtion environment] SW decryption key KDK's derivation string
    MANIFEST_CRYPTO_ENCRYPTION_PARAMS fmcEncParams;  // [FMC related] crypto inputs required to decrypt FMC's code and data, 40 bytes
    LwU32 digest[12];                    // [FMC related] hash of the FMC's code and data
    LwU8 reservedBytes2[16];             // [reserved for SHA512 extension] all 0s
    MANIFEST_SECRET secretMask;          // [exelwtion environment] SCP HW secret mask
    MANIFEST_DEBUG debugAccessControl;   // [exelwtion environment] ICD and debug control
    LwU64 ioPmpMode;                     // [exelwtion environment] IO-PMP configuration, part I
    LwU8 bDICE;                          // [DICE flag] if true, DECFUSEKEY and DICE subroutine is exelwted (false is defined as 0x55; true is defined as 0xAA; other values will be reported as an error)
    LwU8 bKDF;                           // [KDF flag] if true,  DECFUSEKEY and KDF  subroutine is exelwted (false is defined as 0x55; true is defined as 0xAA; other values will be reported as an error)
    MANIFEST_MSPM mspm;                  // [exelwtion environment] max PRIV level and SCP secure indicator
    LwU8 numberOfValidPairs;             // [exelwtion environment] first numberOfValidPairs of registerPair entries get configured by BR
    LwU8 bAttester;                      // [Attestation] if true, BR DECFUSEKEY and AK subroutines are exelwted (false is defined as 0x55; true is defined as 0xAA; other values will be reported as an error)
    LwU8 bCertCA;                        // [Attestation] only applicable when bAttester == true;
    LwU8 fmcMeasIdx;                     // [Measurement] MSR index to store FMC measurement
    LwU8 reservedBytes3[8];              // [reserved space] all 0s; 4 bytes for MAX_DELAY_USEC in the furture
    LwU32 kdfConstant[8];                // [KDF related] 256-bit constant used in KDF
    MANIFEST_DEIVCEMAP deviceMap;        // [exelwtion environment] device map
    MANIFEST_COREPMP corePmp;            // [exelwtion environment] core-PMP
    MANIFEST_IOPMP ioPmp;                // [exelwtion environment] IO-PMP configuration, part II
    MANIFEST_REGISTERPAIR registerPair;  // [exelwtion environment] register address/data pairs
    LwU8 reservedBytes4[128];    // [reserved space] all 0s;
} PKC_VERIFICATION_MANIFEST;
ct_assert(sizeof(PKC_VERIFICATION_MANIFEST) == MANIFEST_SIZE);

#endif // __MANIFEST_GH100_H_
