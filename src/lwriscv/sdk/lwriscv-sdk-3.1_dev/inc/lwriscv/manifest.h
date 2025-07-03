/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LWRISCV_MANIFEST_H
#define LWRISCV_MANIFEST_H

#include <stdint.h>
#include <lwmisc.h>

#include LWRISCV64_MANUAL_CSR
#include LWRISCV64_MANUAL_ADDRESS_MAP

#include "lwriscv/peregrine.h"

#define RSA_SIGNATURE_SIZE                      384
#define MANIFEST_SIZE                           (2048 - RSA_SIGNATURE_SIZE)

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
    uint8_t pmpcfg[MAX_NUM_OF_COREPMP_ENTRY_IN_MANIFEST];   // pmpcfg0 and pmpcfg2
    uint64_t pmpaddr[MAX_NUM_OF_COREPMP_ENTRY_IN_MANIFEST]; // pmpaddr0 to pmpaddr15
} MANIFEST_COREPMP;
_Static_assert(sizeof(MANIFEST_COREPMP) == 288, "MANIFEST_COREPMP size must be 288 bytes.");

typedef struct
{
    uint32_t iopmpcfg[MAX_NUM_OF_IOPMP_ENTRY_IN_MANIFEST];
    uint32_t iopmpaddrlo[MAX_NUM_OF_IOPMP_ENTRY_IN_MANIFEST];
    uint32_t iopmpaddrhi[MAX_NUM_OF_IOPMP_ENTRY_IN_MANIFEST];
} MANIFEST_IOPMP;
_Static_assert(sizeof(MANIFEST_IOPMP) == 384, "MANIFEST_IOPMP size must be 384 bytes.");

typedef struct
{
    uint64_t addr[MAX_NUM_OF_REGISTERPAIR_ENTRY];
    uint32_t andMask[MAX_NUM_OF_REGISTERPAIR_ENTRY];
    uint32_t orMask[MAX_NUM_OF_REGISTERPAIR_ENTRY];
} MANIFEST_REGISTERPAIR;
_Static_assert(sizeof(MANIFEST_REGISTERPAIR) == 512, "MANIFEST_REGISTERPAIR size must be 512 bytes.");

//
// Helper macros
//

// Colwert engine id to engineIdMask field, use with engine name (PWR_PMU, GSP, ..)
#define ENGINE_TO_ENGINE_ID_MASK(X) (1 << ( (LW_PRGNLCL_FALCON_ENGID_FAMILYID_ ##X) - 1))

#define TRUE            0xAA
#define FALSE           0x55

// Generate device map entry
#define DEVICEMAP(group, read, write, lock) \
    ((unsigned)DRF_IDX_DEF( _PRGNLCL, _RISCV_DEVICEMAP_RISCVMMODE, _READ , LW_PRGNLCL_DEVICE_MAP_GROUP##group,  read)) | \
    ((unsigned)DRF_IDX_DEF( _PRGNLCL, _RISCV_DEVICEMAP_RISCVMMODE, _WRITE, LW_PRGNLCL_DEVICE_MAP_GROUP##group, write)) | \
    ((unsigned)DRF_IDX_DEF( _PRGNLCL, _RISCV_DEVICEMAP_RISCVMMODE, _LOCK , LW_PRGNLCL_DEVICE_MAP_GROUP##group,  lock))

// Generate PMPCFG
#define PMP_ENTRY(R, W, X, ADDRMODE, LOCK) \
    DRF_DEF64(_RISCV, _CSR_PMPCFG0, _PMP0R, R) | \
    DRF_DEF64(_RISCV, _CSR_PMPCFG0, _PMP0W, W) | \
    DRF_DEF64(_RISCV, _CSR_PMPCFG0, _PMP0X, X) | \
    DRF_DEF64(_RISCV, _CSR_PMPCFG0, _PMP0A, ADDRMODE) | \
    DRF_DEF64(_RISCV, _CSR_PMPCFG0, _PMP0L, LOCK)

#define PMP_ENTRY_OFF \
    PMP_ENTRY(_DENIED, _DENIED, _DENIED, _OFF, _LOCK)

// Generate iopmp config entry
#define IOPMP_CFG_ALL_PMB(read, write, lock) \
    DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _READ, read) | \
    DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _WRITE, write) | \
    DRF_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _LOCK, lock) | \
    DRF_IDX_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_PMB, 0 , _ENABLE) | \
    DRF_IDX_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_PMB, 1 , _ENABLE) | \
    DRF_IDX_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_PMB, 2 , _ENABLE) | \
    DRF_IDX_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_PMB, 3 , _ENABLE) | \
    DRF_IDX_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_PMB, 4 , _ENABLE) | \
    DRF_IDX_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_PMB, 5 , _ENABLE) | \
    DRF_IDX_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_PMB, 6 , _ENABLE) | \
    DRF_IDX_DEF(_PRGNLCL, _RISCV_IOPMP_CFG, _MASTER_PMB, 7 , _ENABLE)

// IOPMP ADDR_LO entry - config bits
#define IOPMPADDR_LO_IMEM(NAPOT) ((LW_RISCV_AMAP_IMEM_START >> 2) | ((NAPOT) >> 3))
#define IOPMPADDR_LO_DMEM(NAPOT) ((LW_RISCV_AMAP_DMEM_START >> 2) | ((NAPOT) >> 3))

// Max TCM window size
#define SZ_256KIB (1 << 18)

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
_Static_assert((LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_READ__SIZE_1)  == 8,
               "LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_READ__SIZE_1 must be 8.");
_Static_assert((LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_WRITE__SIZE_1) == 8,
               "LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_WRITE__SIZE_1 must be 8.");

// Check that we have 8 PMB masters
_Static_assert((LW_PRGNLCL_RISCV_IOPMP_CFG_MASTER_PMB__SIZE_1) == 8,
               "LW_PRGNLCL_RISCV_IOPMP_CFG_MASTER_PMB__SIZE_1 must be 8.");

// Check if FMC sizes are aligned to 256, this is bootrom requirement.
_Static_assert(((LWRISCV_APP_IMEM_LIMIT) & 0xFF) == 0,
               "LWRISCV_APP_IMEM_LIMIT must be aligned to 256 bytes.");
_Static_assert(((LWRISCV_APP_DMEM_LIMIT) & 0xFF) == 0,
               "LWRISCV_APP_DMEM_LIMIT must be aligned to 256 bytes.");

// Check if FMC sizes are > 0
_Static_assert((LWRISCV_APP_DMEM_LIMIT) >= 0,
               "LWRISCV_APP_IMEM_LIMIT must be greater than 0.");
_Static_assert((LWRISCV_APP_IMEM_LIMIT) >= 0,
               "LWRISCV_APP_DMEM_LIMIT must be greater than 0.");
#endif // LWRISCV_MANIFEST_H
