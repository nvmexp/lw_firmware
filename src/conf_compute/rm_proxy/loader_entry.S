/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <lwriscv/csr.h>
#include <lwriscv/asm-helpers.h>
#include "partasmmacros.h"

#include "cc_rmproxy.h"

/*
 * This code is exelwted only once, it's goal is to copy partition entry code
 * into TCM and reconfigure MPU. It is jumped to from FMC.
 * MPU is configured and enabled by this point (last several entries).
 * After successful completion, it jumps to GSP-RM-Proxy entry point.
 */

.section ".loader.entry","ax",@progbits
.balign 8
FUNC loader_entry
    // Setup (temporary) trap
    la   t0, loader_trap
    csrw stvec, t0

    // Wipe TCM carveouts - IMEM
    la  t0, __imem_rm_start
    la  t1, __imem_rm_size
    add t1, t0, t1
1:
    sd   zero, 0(t0)
    sd   zero, 8(t0)
    addi t0, t0, 0x10
    bne  t0, t1, 1b

    // Wipe TCM carveouts - DMEM
    la  t0, __dmem_rm_start
    la  t1, __dmem_rm_size
    add t1, t0, t1
1:
    sd   zero, 0(t0)
    sd   zero, 8(t0)
    addi t0, t0, 0x10
    bne  t0, t1, 1b

    // setup temporary stack for loader init code, it must run in identity
    // region, because we reprogram MPU
    la sp, _partition_rm_stack_top
    addi sp, sp, -0x10
    sd zero, 0(sp)
    sd zero, 8(sp)
    // Do basic init, copy code.
    jal loader_init_rm_proxy

    // Set stack pointer to proper value (virtual)
    li   sp, PARTITION_STACK_VA_START
    addi sp, sp, -0x10
    sd   zero, 0(sp)
    sd   zero, 8(sp)

    // Resume normal exelwtion
    j partition_rm_entry
_end_loader:
    j _end_loader
EFUNC loader_entry

.balign 8
FUNC loader_trap
1:
    wfi
    j 1b
EFUNC loader_trap
