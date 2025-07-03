/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file start.S
 */
/* ------------------------ SafeRTOS Includes ------------------------------ */
#include <portmpumacros.h>

/* ------------------------ RM Includes -------------------------------- */
#include <memmap.h>
#include <engine.h>
#include <riscv_asm_helpers.h>
#include "config/g_sections_riscv.h"

        .section  .kernel_code
        .global   _start
        .global   __stack_end
        .global   pmuMain

/*
 * WARNING
 *
 * We pass a number of arguments from the bootloader via the a* registers.
 * That means we can't touch them until we call pmuMain.
 *
 * Mapping:
 * a0 - bootargs, PA (may be NULL if not passed from RM)
 * a1 - bootargs, VA (may be NULL if not mapped)
 * a2 - ELF address, PA
 * a3 - ELF size in bytes
 * a4 - Load base, PA
 * a5 - Write-Protected Region id
 */

_start:
        // Write exception vector as early as possible.
        la      t0, vPortPanic
        csrw    stvec, t0

        // Initialize SP and call C entry point routine
        la      sp, __stack_end
        // Push two dwords of 0, to assist debugger backtracing.
        addi    sp, sp, -0x10
        sd      zero, 0(sp)
        sd      zero, 8(sp)

        jal     pmuMain
        j       vPortPanic

.macro  MPU_RGN va, pa, range, attr
        .dword  \va
        .dword  \pa
        .dword  \range
        .dword  \attr
.endm

.macro  MPU_RGN_NOTSET
        MPU_RGN 0,0,0,0
.endm

// The section below gets parsed by the bootloader, which uses this data
// to initialize the MPU.
        .section .LWPU.mpu_info, "a", @progbits
// Manual MPU config
        .word 1                                 // LwU32 mpuSettingType = LW_RISCV_MPU_SETTING_TYPE_MANUAL;
#if LWRISCV_PARTITION_SWITCH
#define LWRISCV_MPU_EXTRA_COUNT 2
#else
#define LWRISCV_MPU_EXTRA_COUNT 0
#endif

        .word UPROC_SECTION_MPU_INIT_COUNT + 4 + LWRISCV_MPU_EXTRA_COUNT // LwU32 mpuRegionCount;

// MPU entries:
//  - build script defined entries
//  - 4 by additional fixed mappings
//  - entries at the top are used by bootloader

// MPU entries that are mapped at init
        UPROC_SECTION_MPUCFG_ALL

// EXTIO mapping - PRI on dgpu / CBB on cheetah
MPU_RGN \
        enginePRIV_VA_BASE + portmpuREGION_VALID, \
        LW_RISCV_AMAP_PRIV_START, \
        LW_RISCV_AMAP_PRIV_SIZE, \
        portmpuREGION_READ_WRITE

// INTIO
MPU_RGN \
        LW_RISCV_AMAP_INTIO_START + portmpuREGION_VALID, \
        LW_RISCV_AMAP_INTIO_START, \
        LW_RISCV_AMAP_INTIO_SIZE, \
        portmpuREGION_READ_WRITE

// DMEM-end carveout
MPU_RGN \
        _dmem_end_carveout_start + portmpuREGION_VALID, \
        __dmem_end_carveout_physaddr, \
        _dmem_end_carveout_size, \
        portmpuREGION_KERNEL_MODE_READ_WRITE | portmpuREGION_CACHEABLE

#if LWRISCV_PARTITION_SWITCH
// Imem identity mapping
MPU_RGN \
        __section_imem_identity_start + portmpuREGION_VALID, \
        __section_imem_identity_physaddr, \
        __section_imem_identity_max_size, \
        portmpuREGION_KERNEL_MODE_READ_EXELWTE | portmpuREGION_CACHEABLE
// Dmem identity mapping
MPU_RGN \
        __section_dmem_identity_start + portmpuREGION_VALID, \
        __section_dmem_identity_physaddr, \
        __section_dmem_identity_max_size, \
        portmpuREGION_KERNEL_MODE_READ_WRITE | portmpuREGION_CACHEABLE
#endif // LWRISCV_PARTITION_SWITCH

// System stack
MPU_RGN \
        __section_sysStack_start + portmpuREGION_VALID, \
        __section_sysStack_physaddr, \
        __section_sysStack_max_size, \
        portmpuREGION_KERNEL_MODE_READ_WRITE | portmpuREGION_CACHEABLE

