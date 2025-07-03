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
FUNC irq_test_handler_m
save_context:
    csrrw t0, mscratch, t0
    la t0, _gpr_context

    SREG    x1,   1 * REGBYTES(t0) // ra
    SREG    x2,   2 * REGBYTES(t0) // sp
    SREG    x3,   3 * REGBYTES(t0) // gp
    SREG    x4,   4 * REGBYTES(t0) // tp
    // missing t0
    SREG    x6,   6 * REGBYTES(t0) // t1
    SREG    x7,   7 * REGBYTES(t0) // t2
    SREG    x8,   8 * REGBYTES(t0) // s0/fp
    SREG    x9,   9 * REGBYTES(t0) // s1
    SREG    x10, 10 * REGBYTES(t0) // a0
    SREG    x11, 11 * REGBYTES(t0) // a1
    SREG    x12, 12 * REGBYTES(t0) // a2
    SREG    x13, 13 * REGBYTES(t0) // a3
    SREG    x14, 14 * REGBYTES(t0) // a4
    SREG    x15, 15 * REGBYTES(t0) // a5
    SREG    x16, 16 * REGBYTES(t0) // a6
    SREG    x17, 17 * REGBYTES(t0) // a7
    SREG    x18, 18 * REGBYTES(t0) // s2
    SREG    x19, 19 * REGBYTES(t0) // s3
    SREG    x20, 20 * REGBYTES(t0) // s4
    SREG    x21, 21 * REGBYTES(t0) // s5
    SREG    x22, 22 * REGBYTES(t0) // s6
    SREG    x23, 23 * REGBYTES(t0) // s7
    SREG    x24, 24 * REGBYTES(t0) // s8
    SREG    x25, 25 * REGBYTES(t0) // s9
    SREG    x26, 26 * REGBYTES(t0) // s10
    SREG    x27, 27 * REGBYTES(t0) // s11
    SREG    x28, 28 * REGBYTES(t0) // t3
    SREG    x29, 29 * REGBYTES(t0) // t4
    SREG    x30, 30 * REGBYTES(t0) // t5
    SREG    x31, 31 * REGBYTES(t0) // t6

    add t1, t0, zero
    csrrw t0, mscratch, t0

    // finally t0
    SREG    x5,   5 * REGBYTES(t1) // t0

    // create smol stack
    la sp, _irq_stack_bottom
    addi sp, sp, -16
    sd zero, 0(sp)
    sd zero, 8(sp)

    la t0, irq_handler_fptr
    ld t0, 0(t0)
    // call irq handler
    jalr t0

restore_context:
    // now restore saved GPR
    la t0, _gpr_context
    LREG    x1,   1 * REGBYTES(t0) // ra
    LREG    x2,   2 * REGBYTES(t0) // sp
    LREG    x3,   3 * REGBYTES(t0) // gp
    LREG    x4,   4 * REGBYTES(t0) // tp
    // missing t0, we'll restore it later
    LREG    x6,   6 * REGBYTES(t0) // t1
    LREG    x7,   7 * REGBYTES(t0) // t2
    LREG    x8,   8 * REGBYTES(t0) // s0/fp
    LREG    x9,   9 * REGBYTES(t0) // s1
    LREG    x10, 10 * REGBYTES(t0) // a0
    LREG    x11, 11 * REGBYTES(t0) // a1
    LREG    x12, 12 * REGBYTES(t0) // a2
    LREG    x13, 13 * REGBYTES(t0) // a3
    LREG    x14, 14 * REGBYTES(t0) // a4
    LREG    x15, 15 * REGBYTES(t0) // a5
    LREG    x16, 16 * REGBYTES(t0) // a6
    LREG    x17, 17 * REGBYTES(t0) // a7
    LREG    x18, 18 * REGBYTES(t0) // s2
    LREG    x19, 19 * REGBYTES(t0) // s3
    LREG    x20, 20 * REGBYTES(t0) // s4
    LREG    x21, 21 * REGBYTES(t0) // s5
    LREG    x22, 22 * REGBYTES(t0) // s6
    LREG    x23, 23 * REGBYTES(t0) // s7
    LREG    x24, 24 * REGBYTES(t0) // s8
    LREG    x25, 25 * REGBYTES(t0) // s9
    LREG    x26, 26 * REGBYTES(t0) // s10
    LREG    x27, 27 * REGBYTES(t0) // s11
    LREG    x28, 28 * REGBYTES(t0) // t3
    LREG    x29, 29 * REGBYTES(t0) // t4
    LREG    x30, 30 * REGBYTES(t0) // t5
    LREG    x31, 31 * REGBYTES(t0) // t6

    // finally t0
    LREG    x5,   5 * REGBYTES(t0) // t0
    // now return
    mret
EFUNC irq_test_handler_m

.balign 16
 _gpr_context: // GPR's, 32 for easier save/restore
 .fill 32, 8, 0

.global irq_handler_fptr
.balign 16
 irq_handler_fptr:
.fill 1, 8, 0

.balign 16
_irq_stack_top:
.fill 512, 1, 0
_irq_stack_bottom:
