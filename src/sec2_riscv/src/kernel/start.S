/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file start.S
 *
 * Hand-written start-section for the SEC2 application. Responsible for very
 * basic initialization, and calling the main function.
 */

/* ------------------------ SafeRTOS Includes ------------------------------ */
#include <portmpumacros.h>

/* ------------------------ RM Includes -------------------------------- */
#include <engine.h>
#include <riscv_asm_helpers.h>
#include "config/g_sections_riscv.h"
#if LWRISCV_PARTITION_SWITCH
#include "partswitch.h"
#endif // LWRISCV_PARTITION_SWITCH

/*
 * WARNING
 *
 * We pass a number of arguments from the bootloader via the a* registers.
 * That means we can't touch them until we call appMain.
 *
 * Mapping:
 * a0 - bootargs, PA (may be NULL if not passed from RM)
 * a1 - bootargs, VA (may be NULL if not mapped)
 * a2 - ELF address, PA
 * a3 - ELF size in bytes
 * a4 - Load base, PA
 * a5 - Write-Protected Region id
 */
        .section .kernel_code
        .global  _start
_start:
        // Write exception vector, so early just panic - we can't do much more
        la      t0, vPortPanic
        csrw    stvec, t0

        // Initialize stack pointer
        la      sp, __stack_end

        // Push two dwords of 0, to assist debugger backtracing.
        addi    sp, sp, -0x10
        sd      zero, 0(sp)
        sd      zero, 8(sp)

        // The rest of setup is handled in main itself
        jal     appMain

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

#if LWRISCV_PARTITION_SWITCH
#define LWRISCV_MPU_EXTRA_COUNT 3
#else
#define LWRISCV_MPU_EXTRA_COUNT 0
#endif
        .section .LWPU.mpu_info, "a", @progbits
// Manual MPU config
        .word 1
        .word 64 - 4  // start of dynamic / end of normal

// MPU has 64 entries:
//  - top 4 are used by bootloader
//  - build script defined entries
//  - at most 10 by additional fixed mappings
// declare rest as unused so we can use them for dynamic mappings.
.rept 64 - 4 - UPROC_SECTION_MPU_INIT_COUNT - 10 - LWRISCV_MPU_EXTRA_COUNT
        MPU_RGN_NOTSET
.endr

#ifndef LW_RISCV_AMAP_SYSGPA_START
        MPU_RGN_NOTSET
#endif // LW_RISCV_AMAP_SYSGPA_START

#ifndef LW_RISCV_AMAP_FBGPA_START
        MPU_RGN_NOTSET
        MPU_RGN_NOTSET
#endif // LW_RISCV_AMAP_FBGPA_START

#ifndef LW_RISCV_AMAP_INTIO_START
        MPU_RGN_NOTSET
#endif //LW_RISCV_AMAP_INTIO_START

#ifndef LW_RISCV_AMAP_PRIV_START
        MPU_RGN_NOTSET
#endif // LW_RISCV_AMAP_PRIV_START

///////////////////////////////////// CODE //////////////////////////
        UPROC_SECTION_MPUCFG_ALL

// SYSMEM
#ifdef LW_RISCV_AMAP_SYSGPA_START
MPU_RGN \
        engineSYSGPA_VA_BASE + portmpuREGION_VALID, \
        LW_RISCV_AMAP_SYSGPA_START, \
        engineSYSGPA_SIZE, \
        portmpuREGION_KERNEL_MODE_READ_WRITE | portmpuREGION_CACHEABLE
#endif // LW_RISCV_AMAP_SYSGPA_START

// BAR2 - Full FB Mapping
#ifdef LW_RISCV_AMAP_FBGPA_START
MPU_RGN \
        engineFBGPA_FULL_VA_BASE + portmpuREGION_VALID, \
        LW_RISCV_AMAP_FBGPA_START, \
        engineFBGPA_PA_SIZE, \
        portmpuREGION_READ_WRITE | portmpuREGION_CACHEABLE

// BAR2 - Full FB Mapping (uncached)
MPU_RGN \
        engineFBGPA_FULL_VA_UC_BASE + portmpuREGION_VALID, \
        LW_RISCV_AMAP_FBGPA_START, \
        engineFBGPA_PA_SIZE, \
        portmpuREGION_READ_WRITE
#endif // LW_RISCV_AMAP_FBGPA_START

// DMESG buffer
MPU_RGN \
        _dmesg_buffer_start + portmpuREGION_VALID, \
        __dmesg_buffer_physaddr, \
        _dmesg_buffer_size, \
        portmpuREGION_KERNEL_MODE_READ_WRITE | portmpuREGION_CACHEABLE

// System stack
MPU_RGN \
        __section_sysStack_start + portmpuREGION_VALID, \
        __section_sysStack_physaddr, \
        __section_sysStack_max_size, \
        portmpuREGION_KERNEL_MODE_READ_WRITE | portmpuREGION_CACHEABLE

// Fallback .text
MPU_RGN \
        __section_text_start + portmpuREGION_VALID, \
        __section_text_physaddr, \
        __section_text_max_size, \
        portmpuREGION_READ_EXELWTE | portmpuREGION_CACHEABLE

// Fallback .rodata
MPU_RGN \
        __section_rodata_start + portmpuREGION_VALID, \
        __section_rodata_physaddr, \
        __section_rodata_max_size, \
        portmpuREGION_READ_ONLY | portmpuREGION_CACHEABLE

// Fallback .data
MPU_RGN \
        __section_data_start + portmpuREGION_VALID, \
        __section_data_physaddr, \
        __section_data_max_size, \
        portmpuREGION_READ_WRITE | portmpuREGION_CACHEABLE

#ifdef LW_RISCV_AMAP_INTIO_START
// INTIO
MPU_RGN \
        LW_RISCV_AMAP_INTIO_START + portmpuREGION_VALID, \
        LW_RISCV_AMAP_INTIO_START, \
        LW_RISCV_AMAP_INTIO_SIZE, \
        portmpuREGION_READ_WRITE
#endif //LW_RISCV_AMAP_INTIO_START

#ifdef LW_RISCV_AMAP_PRIV_START
// EXTIO mapping - PRI on dgpu / CBB on cheetah
MPU_RGN \
        enginePRIV_VA_BASE + portmpuREGION_VALID, \
        LW_RISCV_AMAP_PRIV_START, \
        enginePRIV_SIZE, \
        portmpuREGION_READ_WRITE
#endif // LW_RISCV_AMAP_PRIV_START

// Partition switch code
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
// Shared carveout mapping for partition-partition message passing
// There is no (yet) nice way to do it, as PA is defined in FMC linker script,
// and RTOS image is not linked with it (so we have no access to PA).
MPU_RGN \
        PARTSWITCH_SHARED_CARVEOUT_VA + portmpuREGION_VALID, \
        PARTSWITCH_SHARED_CARVEOUT_PA, \
        PARTSWITCH_SHARED_CARVEOUT_SIZE, \
        portmpuREGION_READ_WRITE

#endif // LWRISCV_PARTITION_SWITCH
