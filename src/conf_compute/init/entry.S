/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "lwriscv/csr.h"
#include "lwriscv/asm-helpers.h"
#include "partasmmacros.h"
#include "partitions.h"

.section ".text.partition_init_entry","ax",@progbits
.balign 8
FUNC partition_init_entry

    // Set Partition 0 trap handler
    la    t0, partition_init_trap_entry
    csrw  stvec, t0

    partitionInitSpAndMpu _partition_init_stack_bottom, _partition_init_stack_top

    jal partition_init_main

_end:
    j _end
EFUNC partition_init_entry

.section ".text.partition_init_wipe_and_switch","ax",@progbits
.balign 8
FUNC partition_init_wipe_and_switch
    // Load ACR switch data (later it may have been wiped already)
    li a7, SBI_EXTENSION_LWIDIA
    li a6, SBI_LWFUNC_FIRST
    li a4, PARTITION_ID_INIT
    li a5, PARTITION_ID_ACR

    la t2, __partition_init_data_wipe_start
    la t3, __partition_init_data_wipe_end

    // Wipe DMEM
1:
    beq t2, t3, end_dmem
    sd zero, 0(t2)
    addi t2, t2, 8
    j 1b
end_dmem:

    ecall // Switch to ACR
EFUNC partition_init_wipe_and_switch
// Trap in init is fatal
.balign 8
FUNC partition_init_trap_entry
    // Save ra and sp
    csrw sscratch, ra
    csrw LW_RISCV_CSR_SSCRATCH2, sp
    // Set "fatal" exception handler
    la sp, 1f
    csrw stvec, sp
    // Reset stack
    la sp, _partition_init_stack_top
    addi sp, sp, -16
    sd zero, 0(sp)
    sd zero, 8(sp)

    // Jump to trap code
    jal partition_init_trap_handler

// not much can be done at this point
1:
    wfi
    j 1b
EFUNC partition_init_trap_entry
