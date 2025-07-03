/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <lwriscv/asm-helpers.h>
#include <dev_riscv_csr_64.h>

/* This code runs in M mode unlike other files */
.balign 16
FUNC fmc_entry
    li    x1, 0
    li    x2, 0
    li    x3, 0
    li    x4, 0
    li    x5, 0
    li    x6, 0
    li    x7, 0
    li    x8, 0
    li    x9, 0
    li    x10, 0
    li    x11, 0
    li    x12, 0
    li    x13, 0
    li    x14, 0
    li    x15, 0
    li    x16, 0
    li    x17, 0
    li    x18, 0
    li    x19, 0
    li    x20, 0
    li    x21, 0
    li    x22, 0
    li    x23, 0
    li    x24, 0
    li    x25, 0
    li    x26, 0
    li    x27, 0
    li    x28, 0
    li    x29, 0
    li    x30, 0
    li    x31, 0

    // write mtvec
    la    t0, fmc_halt
    csrw  mtvec, t0

    // We need stack for init only, just take first 4k of dmem
    la    sp, 0x0000000000181000
    sd    zero, 0(sp)
    sd    zero, 8(sp)

    jal  fmc_main

1:
    jal fmc_icd
    j 1b
EFUNC fmc_entry

.balign 16
FUNC fmc_icd
    /* ICD stop */
    li t0, 0x1
    slli t0, t0, 61
    lui t1, 0x111       /* 0x111... -> which RISCV */
    addi t1, t1, 0x3d0 /* 0x...3d0 -> GA10x GSP ICD */
    add t0, t0, t1
    sw zero, 0(t0)
    fence
    j fmc_panic
EFUNC fmc_icd

.balign 16
FUNC fmc_halt
/* If we are effed we should loop in panic. */
    la t0, fmc_panic
    csrw  mtvec, t0

/* store crash CSRs in DMEM */
    lui t1, 0x00180
    addi t1, t1, 0x10
    csrr t0, mcause
    sd t0, 0(t1)
    csrr t0, 0x7d4 /*mcause2 on GA10x*/
    sd t0, 8(t1)
    csrr t0, mepc
    sd t0, 16(t1)
    csrr t0, mtval
    sd t0, 24(t1)
EFUNC fmc_halt

  .balign 16
FUNC fmc_trap_handler
trap_entry:
  csrw LW_RISCV_CSR_MSCRATCH, t0
  csrw LW_RISCV_CSR_MSCRATCH2, t1
// check if it's ECALL - if it is, just resume, it's uToM or sToM
  csrr t0, mcause
  bltz t0, fmc_halt // interrupt
  addi  t0, t0, -8 // ECALL from U
  beqz t0, skip_trap
  addi t0, t0, -1 // ECALL from S
  beqz t0, skip_trap
  addi t0, t0, -2 // ECALL from M
  beqz t0, skip_trap
  j fmc_halt // must be other trap if not ecall, DIE
skip_trap:
  csrr t0, mepc
  addi t0, t0, 4
  csrw mepc, t0
  csrr t0, mstatus
  addi t1, zero, 3
  slli t1, t1, 11
  or   t0, t0, t1
  csrw mstatus, t0
  csrr t0, LW_RISCV_CSR_MSCRATCH
  csrr t1, LW_RISCV_CSR_MSCRATCH2
  mret
EFUNC fmc_trap_handler

// Panic
.balign 16
FUNC fmc_panic
1:
    j 1b
EFUNC fmc_panic
