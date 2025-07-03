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

.balign 4
FUNC try_fetch_int // void try_fetch_int(uintptr_t addr)
  sd	t0, -8(sp)
  sd	t1, -16(sp)
  la	t0, mask_mepc
  la    t1, finalize
  sd    t1, 0(t0)
  fence.i
  fence rw,io
  jr a0
  fence.i
  fence rw,io
  sd    zero, 0(t0)
  finalize:
  ld	t1, -16(sp)
  ld	t0, -8(sp)
  ret
EFUNC try_fetch_int

.balign 4
FUNC trap_entry
    sd t0, -8(sp)
    sd t1, -16(sp)
// check if it's ECALL - if it is, just resume, it's uToM or sToM
    csrr t0, mcause
    bltz t0, other_trap // interrupt
    addi t0, t0, -8 // ECALL from U
    beqz t0, skip_trap
    addi t0, t0, -1 // ECALL from S
    beqz t0, skip_trap
    addi t0, t0, -2 // ECALL from M
    beqz t0, skip_trap
    j other_trap // must be other trap if not ecall
skip_trap:
    csrr t0, mepc
    addi t0, t0, 4
    csrw mepc, t0
    csrr t0, mstatus
    addi t1, zero, 3
    slli t1, t1, 11
    or   t0, t0, t1
    csrw mstatus, t0
    j epilog
other_trap:
    la	t0, trap_mepc
    // save mepc
    csrr	t1, mepc
    sd	t1, 0(t0)

    // Check if we need to skip
    ld  t1, 24(t0)
    // Just patch mepc if nonzero
    bnez t1, resume
skip_inst:
    csrr	t1, mepc
        // skip over offending instruction
        addi	t1, t1, 4
resume:
    csrw mepc, t1

    la   t0, trap_mcause
    csrr t1, mcause
    sd   t1, 0(t0)

    la   t0, trap_mtval
    csrr t1, mtval
    sd   t1, 0(t0)
epilog:
    ld	t1, -16(sp)
    ld	t0, -8(sp)
    mret
EFUNC trap_entry

.section	.data
    .global trap_mepc
trap_mepc:	.space	8
    .global trap_mcause
trap_mcause:	.space	8
    .global trap_mtval
trap_mtval:	.space	8
    .global mask_mepc
mask_mepc:	.space	8
