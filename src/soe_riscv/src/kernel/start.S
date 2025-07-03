/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file start.S
 *
 * Hand-written start-section for the SOE application. Responsible for very
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
        //
        // this ensures that _start is at the start of kernelCode. This enabled us to safely
        // pass --retSection=.kernelCode to rvmkfmc.pl (we only have 1 partition).
        // Technically since the return flag would never be set, we could pass any section there,
        // but having this as backup is cleaner.
        //
        .section .kernelCode
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

// The section below gets parsed by the bootstub, which uses this data
// to initialize the MPU.
        .section .LWPU.mpu_info, "a", @progbits
// Manual MPU config
        .word 1                                 // LwU32 mpuSettingType = LW_RISCV_MPU_SETTING_TYPE_MANUAL;
        .word UPROC_SECTION_MPU_INIT_COUNT + 4 + LWRISCV_MPU_EXTRA_COUNT // LwU32 mpuRegionCount;

// MPU has 64 entries:
//  - build script defined entries
//  - 4 by additional fixed mappings
//  - entries at the top are used by the bootstub

///////////////////////////////////// CODE //////////////////////////
        UPROC_SECTION_MPUCFG_ALL

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

// INTIO
MPU_RGN \
        LW_RISCV_AMAP_INTIO_START + portmpuREGION_VALID, \
        LW_RISCV_AMAP_INTIO_START, \
        LW_RISCV_AMAP_INTIO_SIZE, \
        portmpuREGION_READ_WRITE

// EXTIO mapping - PRI on dgpu / CBB on cheetah
MPU_RGN \
        enginePRIV_VA_BASE + portmpuREGION_VALID, \
        LW_RISCV_AMAP_PRIV_START, \
        enginePRIV_SIZE, \
        portmpuREGION_READ_WRITE

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
