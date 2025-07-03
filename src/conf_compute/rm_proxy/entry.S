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

// Entry to GSP-RM-PROXY.
// This code will be written into partition's IMEM entry point, so must be
// position independent.

// It also has to turn on MPU to properly access WPR

.section ".text.partition_rm_entry","ax",@progbits
.balign 8
FUNC partition_rm_entry
    // Temporary "panic" trap until we reset MPU and PLM
    la   t0, rm_proxy_entry_trap
    csrw stvec, t0

#if CC_IS_RESIDENT
    partitionInitSpAndMpu _partition_rm_stack_bottom, _partition_rm_stack_top
#else // CC_IS_RESIDENT
    // Reenable MPU, code will not work without that
    li   t0, 0xf
    slli t0, t0, 60
    csrw LW_RISCV_CSR_SATP, t0
    sfence.vma

// switch to virtual memory / FB
    la t0, running_from_va_addr
    ld t0, 0(t0)
    jr t0

    .balign 8
running_from_va_addr:
    .dword running_from_va
running_from_va:
    sfence.vma

    // Now load actual trap, it'll be overwritten by partition return (possibly)
    la t0, partition_rm_trap_entry
    csrw stvec, t0
#endif // CC_IS_RESIDENT

     // This macro does not return if context switch is needed
    // Otherwise fallthrough
    partitionReturnIfNeeded

#if CC_IS_RESIDENT
     // Setup stack
     la sp, _partition_rm_stack_top
#endif

    jal partition_rm
_end:
     wfi
    j _end
EFUNC partition_rm_entry

.balign 8
FUNC rm_proxy_entry_trap
    wfi
1:
    j 1b
EFUNC rm_proxy_entry_trap

.balign 8
FUNC partition_rm_trap_entry
    // Save ra and sp
    csrw sscratch, ra
    csrw LW_RISCV_CSR_SSCRATCH2, sp
    // Set "fatal" exception handler
    la sp, 1f
    csrw stvec, sp
    // Reset stack
    la sp, _rm_proxy_trap_stack_top
    addi sp, sp, -16
    sd zero, 0(sp)
    sd zero, 8(sp)

    // Jump to trap code
    jal rm_proxy_trap

// not much can be done at this point
1:
    wfi
    j 1b
EFUNC partition_rm_trap_entry

// Generate partition switch code
#define PARTCTX_PRESERVE_CSR 1
#define PARTCTX_REENABLE_MPU 1
#include "partctxasm.h"

.section ".bss.partition_rm_entry","a",@nobits
    _rm_proxy_trap_stack_bottom:
    .fill 8, 1024, 0 // 1kb emergency stack
    .balign 8
    _rm_proxy_trap_stack_top:
