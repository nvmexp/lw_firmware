/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <dev_riscv_csr_64.h>

.align 6
.section ".text.startup","ax",@progbits
.global startup
startup:
	j    do_reset

do_reset:
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

	// write mtvec and make sure it sticks
	la    t0, trap_entry
	csrw  mtvec, t0
    csrw  stvec, t0

	la    sp, _monitor_stack_top - 0x10
	sd    zero, 0(sp)
	sd    zero, 8(sp)
	jal   main
	// Don't halt the core, Reset is broken on Orin NET < 28
	// csrwi LW_RISCV_CSR_MOPT, LW_RISCV_CSR_MOPT_CMD_HALT
_end:
	j _end

# Panic
_panic:
	j _panic

.global try_fetch_int // void try_fetch_int(uintptr_t addr)
try_fetch_int:
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

trap_entry:
	sd	t0, -8(sp)
	sd	t1, -16(sp)
// check if it's ECALL - if it is, just resume, it's uToM or sToM
    csrr t0, mcause
    bltz t0, other_trap // interrupt
    addi  t0, t0, -8 // ECALL from U
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
    csrw	mepc, t1

	csrr	t1, mcause
	sd	t1, 8(t0)
	csrr	t1, mtval
	sd	t1, 16(t0)
epilog:
	ld	t1, -16(sp)
	ld	t0, -8(sp)
	mret

	.section	.data
    .global trap_mepc
trap_mepc:	.space	8
	.global trap_mcause
trap_mcause:	.space	8
	.global trap_mtval
trap_mtval:	.space	8
  .global mask_mepc
mask_mepc:	.space	8
