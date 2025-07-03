/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "lwriscv/csr.h"
#include "lwriscv/asm-helpers.h"

.section ".text.partition_rm_entry","ax",@progbits
.balign 8
FUNC partition_rm_entry

    // Set trap handler
    la    t0, partition_rm_trap_handler
    csrw  stvec, t0

    // Setup stack
    la sp, _partition_rm_stack_top
    addi sp, sp, -16
    sd zero, 0(sp)
    sd zero, 8(sp)

    // Jump to bootstrapper, which will jump to GSP-RM
    jal partition_rm_main

    // Should never get here
    jal partition_rm_trap_handler

_end:
    wfi
    j _end
EFUNC partition_rm_entry

// Trap in bootstrap is fatal
.balign 8
FUNC partition_rm_trap_handler
    // Save ra and sp
    csrw sscratch, ra
    csrw LW_RISCV_CSR_SSCRATCH2, sp
    // Set "fatal" exception handler
    la sp, 1f
    csrw stvec, sp
    // Reset stack
    la sp, _partition_rm_stack_top
    addi sp, sp, -16
    sd zero, 0(sp)
    sd zero, 8(sp)

    // Jump to trap code
    jal partition_rm_trap

// not much can be done at this point
1:
    wfi
    j 1b
EFUNC partition_rm_trap_handler
