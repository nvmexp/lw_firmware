/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "lwriscv/csr.h"
#include "lwriscv/asm-helpers.h"
#include "partasmmacros.h"

.section ".text.partition_acr_entry","ax",@progbits
.balign 8
FUNC partition_acr_entry

    // Set trap handler
    la    t0, partition_acr_trap_handler
    csrw  stvec, t0

    // Setup stack
    // MK TODO: we can't set VA for stack, because CCC RNG driver uses it for HW operations
    // https://jirasw.lwpu.com/browse/COREUCODES-2067
    la sp, _partition_acr_stack_top
    addi sp, sp, -0x10
    sd zero, 0(sp)
    sd zero, 8(sp)

    jal partition_acr_main

_end:
    j _end
EFUNC partition_acr_entry

.section ".text.partition_acr_scrub_dmem","ax",@progbits
.balign 4
FUNC partition_acr_scrub_dmem
    la t2, _partition_acr_stack_bottom 
    la t3, _partition_acr_stack_top

    // Wipe Stack Section
1:
    beq t2, t3, end_stack_scrubbing
    sd zero, 0(t2)
    addi t2, t2, 8
    j 1b
end_stack_scrubbing:

    la t2, _partition_acr_bss_start 
    la t3, _partition_acr_bss_end

    // Wipe BSS Section
1:
    beq t2, t3, end_bss_scrubbing
    sd zero, 0(t2)
    addi t2, t2, 8
    j 1b
end_bss_scrubbing:
    ret
EFUNC partition_acr_scrub_dmem
