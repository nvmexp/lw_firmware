/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LWRISCV_MANIFEST_GH100_H
#define LWRISCV_MANIFEST_GH100_H

////////////////////////////////////////////////////////////////////////
// BEGIN of paste of manifest.h
// We can't use manifest.h as some structures has changed.
////////////////////////////////////////////////////////////////////////

//
// Synced on 2020-01-30 to
// https://confluence.lwpu.com/display/Peregrine/GA10x+LWRISCV+BootROM+Design+Spec
//

#include <stdint.h>
#include <lwmisc.h>
#include <lwriscv/peregrine.h>
#include LWRISCV64_MANUAL_ADDRESS_MAP
#include LWRISCV64_MANUAL_CSR

#define MANIFEST_SIZE                           (2560)

#define MAX_NUM_OF_REGISTERPAIR_ENTRY           32
#define MAX_NUM_OF_DEVICEMAP_ENTRY              32
#define MAX_NUM_OF_COREPMP_ENTRY_IN_MANIFEST    32
#define MAX_NUM_OF_IOPMP_ENTRY_IN_MANIFEST      32

typedef struct
{
    uint64_t scpSecretMask;         // LW_PRGNLCL_RISCV_SCP_SECRET_MASK(0...1)
    uint64_t scpSecretMaskLock;     // LW_PRGNLCL_RISCV_SCP_SECRET_MASK_LOCK(0...1)
} MANIFEST_SECRET;
_Static_assert(sizeof(MANIFEST_SECRET) == 16, "MANIFEST_SECRET size must be 16 bytes.");

typedef struct
{
    uint32_t dbgctl;        // LW_PRGNLCL_RISCV_DBGCTL, allowed ICD commands
    uint32_t dbgctlLock;    // LW_PRGNLCL_RISCV_DBGCTL_LOCK, locks out write access to corresponding bits in LW_PRGNLCL_RISCV_DBGCTL
} MANIFEST_DEBUG;
_Static_assert(sizeof(MANIFEST_DEBUG) == 8, "MANIFEST_DEBUG size must be 8 bytes.");

typedef struct
{
    uint8_t mplm;   // M mode PL mask. Only the lowest 4 bits can be non-zero
                    // bit 0 - indicates if PL0 is enabled (1) or disabled (0) in M mode. It is a read-only bit in HW so it has to be set here in the manifest
                    // bit 1 - indicates if PL1 is enabled (1) or disabled (0) in M mode.
                    // bit 2 - indicates if PL2 is enabled (1) or disabled (0) in M mode.
                    // bit 3 - indicates if PL3 is enabled (1) or disabled (0) in M mode.
    uint8_t msecm;  // M mode SCP transaction indicator. Only the lowest bit can be non-zero
                    // bit 0 - indicates if secure transaction is enabled (1) or disabled (0) in M mode.
} MANIFEST_MSPM;
_Static_assert(sizeof(MANIFEST_MSPM) == 2, "MANIFEST_MSPM size must be 8 bytes.");

typedef struct
{
    uint32_t deviceMap[MAX_NUM_OF_DEVICEMAP_ENTRY/8];
} MANIFEST_DEVICEMAP;
_Static_assert(sizeof(MANIFEST_DEVICEMAP) == 16, "MANIFEST_DEVICEMAP size must be 16 bytes.");

typedef struct
{
    uint64_t cfg[MAX_NUM_OF_COREPMP_ENTRY_IN_MANIFEST / 8];   // pmpcfg0 and pmpcfg2
    uint64_t addr[MAX_NUM_OF_COREPMP_ENTRY_IN_MANIFEST];      // pmpaddr0 to pmpaddr15
} MANIFEST_COREPMP;
_Static_assert(sizeof(MANIFEST_COREPMP) == 288, "MANIFEST_COREPMP size must be 288 bytes.");

typedef struct
{
    uint32_t cfg;
    uint32_t usercfg;
    uint32_t addrLo;
    uint32_t addrHi;
} MANIFEST_IOPMP_ENTRY;

typedef struct
{
    MANIFEST_IOPMP_ENTRY entry[MAX_NUM_OF_IOPMP_ENTRY_IN_MANIFEST];
} MANIFEST_IOPMP;
_Static_assert(sizeof(MANIFEST_IOPMP) == 512, "MANIFEST_IOPMP size must be 512 bytes.");

typedef struct
{
    uint64_t addr;
    uint32_t andMask;
    uint32_t orMask;
} MANIFEST_REGISTERPAIR_ENTRY;

typedef struct
{
    MANIFEST_REGISTERPAIR_ENTRY entries[MAX_NUM_OF_REGISTERPAIR_ENTRY];
} MANIFEST_REGISTERPAIR;
_Static_assert(sizeof(MANIFEST_REGISTERPAIR) == 512,  "MANIFEST_REGISTERPAIR size must be 512 bytes.");

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
_Static_assert((LW_RISCV_AMAP_IMEM_SIZE != 0) && ((LW_RISCV_AMAP_IMEM_SIZE & (LW_RISCV_AMAP_IMEM_SIZE - 1)) == 0), "Incorrect IMEM size.");
_Static_assert((LW_RISCV_AMAP_DMEM_SIZE != 0) && ((LW_RISCV_AMAP_DMEM_SIZE & (LW_RISCV_AMAP_DMEM_SIZE - 1)) == 0), "Incorrect DMEM size.");


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
_Static_assert(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_READ__SIZE_1  == 8,
               "LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_READ__SIZE_1 must be 8.");
_Static_assert(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_WRITE__SIZE_1 == 8,
               "LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_WRITE__SIZE_1 must be 8.");
_Static_assert(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_LOCK__SIZE_1  == 8,
               "LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_LOCK__SIZE_1 must be 8.");

// Check that we have 8 PMB masters
_Static_assert(LW_PRGNLCL_RISCV_IOPMP_CFG_MASTER_PMB__SIZE_1 == 4,
               "LW_PRGNLCL_RISCV_IOPMP_CFG_MASTER_PMB__SIZE_1 must be 4.");

// Check if FMC sizes are aligned to 256
_Static_assert(((LWRISCV_APP_IMEM_LIMIT) & 0xFF) == 0,
               "LWRISCV_APP_IMEM_LIMIT must be aligned to 256 bytes.");
_Static_assert(((LWRISCV_APP_DMEM_LIMIT) & 0xFF) == 0,
               "LWRISCV_APP_DMEM_LIMIT must be aligned to 256 bytes.");

// Check if FMC sizes are > 0
_Static_assert((LWRISCV_APP_IMEM_LIMIT) >= 0,
               "LWRISCV_APP_IMEM_LIMIT must be greater than 0.");
_Static_assert((LWRISCV_APP_DMEM_LIMIT) >= 0,
               "LWRISCV_APP_DMEM_LIMIT must be greater than 0.");

/////////////////////////////////////
// End of paste of manifest.h
////////////////////////////////////
//
// Synced on 2020-10-28 to
// https://confluence.lwpu.com/display/FalconSelwrityIPUserSpace/GH100+RISCV+BootROM+Design+Spec#GH100RISCVBootROMDesignSpec-Stage1PKCBOOTParameters1
//

typedef struct
{
    uint8_t encryptionDerivationString[8];  // Part of KDF label to derive decryption key
    uint8_t iv[16];                         // IV for decryption (only 96 bits used for AES-256-GCM)
    uint8_t authTag[16];                    // GCM authentication tag (unused in non-GCM modes)
} MANIFEST_CRYPTO_ENCRYPTION_PARAMS;
_Static_assert(sizeof(MANIFEST_CRYPTO_ENCRYPTION_PARAMS) == 40, "Incorrect MANIFEST_CRYPTO_ENCRYPTION_PARAMS size.");

typedef struct
{
    // Stage 1 part of manifest, unencrypted
    uint32_t stage1RSA3KSigProd[96];        // patched by siggen, production signed RSASSA3k-PSS signature
    uint32_t stage1RSA3KSigDebug[96];       // patched by siggen, debug signed RSASSA3k-PSS signature


    // stage1 signed unencrypted section, 128 bytes
    uint32_t magicNumberStage1;             // Can be used by SW for anything (ignored by bootrom)
    uint8_t bUseDevKey;                     // Switch to use debug or prod key (on debug boards)
    uint8_t reservedBytes0[19];
    MANIFEST_CRYPTO_ENCRYPTION_PARAMS manifestEncParams;
    uint8_t reservedBytes1[64];

    // Stage 1, encrypted section
    uint8_t version;                                 // [MISC] version of the manifest; it needs to be 0x1 in GA10x/T234
    uint8_t ucodeId;                                 // [ucode revocation] ucode ID
    uint8_t ucodeVersion;                            // [ucode revocation] ucode version
    uint8_t bRelaxedVersionCheck;                    // [ucode revocation] if true, allows a greater-or-equal-to check on ucodeVersion (false is defined as 0x55; true is defined as 0xAA; other values will be reported as an error)
    uint32_t engineIdMask;                           // [ucode revocation] engine ID mask
    uint16_t itcmSizeIn256Bytes;                     // [FMC related] FMC's code starts at offset 0 inside IMEM; the sizes need to be a multiple of 256 bytes; the blobs need to be 0-padded.
    uint16_t dtcmSizeIn256Bytes;                     // [FMC related] FMC's data starts at offset 0 inside DMEM; the sizes need to be a multiple of 256 bytes; the blobs need to be 0-padded.
    uint32_t fmcHashPadInfoBitMask;                  // [FMC related] in GA10x the LSb indicates whether the 64-bit PDI is prepended to FMC code/data when callwlating the digest; all other bits must be 0s
    uint8_t swEncryptionDeviationString[8];          // [exelwtion environment] SW decryption key KDK's derivation string
    MANIFEST_CRYPTO_ENCRYPTION_PARAMS fmcEncParams;  // [FMC related] crypto inputs required to decrypt FMC's code and data, 40 bytes
    uint32_t digest[12];                             // [FMC related] hash of the FMC's code and data
    uint8_t reservedBytes2[16];                      // [reserved for SHA512 extension] all 0s
    MANIFEST_SECRET secretMask;                      // [exelwtion environment] SCP HW secret mask
    MANIFEST_DEBUG debugAccessControl;               // [exelwtion environment] ICD and debug control
    uint64_t ioPmpMode;                              // [exelwtion environment] IO-PMP configuration, part I
    uint8_t bDICE;                                   // [DICE flag] if true, DECFUSEKEY and DICE subroutine is exelwted (false is defined as 0x55; true is defined as 0xAA; other values will be reported as an error)
    uint8_t bKDF;                                    // [KDF flag] if true,  DECFUSEKEY and KDF  subroutine is exelwted (false is defined as 0x55; true is defined as 0xAA; other values will be reported as an error)
    MANIFEST_MSPM mspm;                              // [exelwtion environment] max PRIV level and SCP secure indicator
    uint8_t numberOfValidPairs;                      // [exelwtion environment] first numberOfValidPairs of registerPair entries get configured by BR
    uint8_t bAttester;                               // [Attestation] if true, BR DECFUSEKEY and AK subroutines are exelwted (false is defined as 0x55; true is defined as 0xAA; other values will be reported as an error)
    uint8_t bCertCA;                                 // [Attestation] only applicable when bAttester == true;
    uint8_t fmcMeasIdx;                              // [Measurement] MSR index to store FMC measurement
    uint8_t reservedBytes3[8];                       // [reserved space] all 0s; 4 bytes for MAX_DELAY_USEC in the furture
    uint32_t kdfConstant[8];                         // [KDF related] 256-bit constant used in KDF
    MANIFEST_DEVICEMAP deviceMap;                    // [exelwtion environment] device map
    MANIFEST_COREPMP corePmp;                        // [exelwtion environment] core-PMP
    MANIFEST_IOPMP ioPmp;                            // [exelwtion environment] IO-PMP configuration, part II
    MANIFEST_REGISTERPAIR registerPair;              // [exelwtion environment] register address/data pairs
    uint8_t reservedBytes4[128];                     // [reserved space] all 0s;
} PKC_VERIFICATION_MANIFEST;
_Static_assert(sizeof(PKC_VERIFICATION_MANIFEST) == MANIFEST_SIZE, "Incorrect PKC_VERIFICATION_MANIFEST size.");

#endif // LWRISCV_MANIFEST_GH100_H
