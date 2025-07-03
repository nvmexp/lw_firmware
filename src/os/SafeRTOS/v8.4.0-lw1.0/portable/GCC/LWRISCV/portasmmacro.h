/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PORTASMMACRO_H
#define PORTASMMACRO_H

#include "portcommon.h"

#define FPREGBYTES      4

/* There are 2 types of context switches:
 * - Full - every register is saved / restored
 * - Partial - only callee saved registers are stored (remaining are discarded on return)
 *
 * Both macros expect that mscratch contains pointer to xPortTaskContext.
 * For kernel-side that may be on stack, for tasks should be part of TCB.
 *
 * Type of context switch is stored in flags
 * For stack that is stored at top address so we may use less stack for context.
 * For TCB it doesn't matter as we have that space preallocated anyway. */

/* Set all FP Registers to ZERO */
.macro fpContextInit
    fmv.w.x f0,  zero
    fmv.w.x f1,  zero
    fmv.w.x f2,  zero
    fmv.w.x f3,  zero
    fmv.w.x f4,  zero
    fmv.w.x f5,  zero
    fmv.w.x f6,  zero
    fmv.w.x f7,  zero
    fmv.w.x f8,  zero
    fmv.w.x f9,  zero
    fmv.w.x f10,  zero
    fmv.w.x f11,  zero
    fmv.w.x f12,  zero
    fmv.w.x f13,  zero
    fmv.w.x f14,  zero
    fmv.w.x f15,  zero
    fmv.w.x f16,  zero
    fmv.w.x f17,  zero
    fmv.w.x f18,  zero
    fmv.w.x f19,  zero
    fmv.w.x f20,  zero
    fmv.w.x f21,  zero
    fmv.w.x f22,  zero
    fmv.w.x f23,  zero
    fmv.w.x f24,  zero
    fmv.w.x f25,  zero
    fmv.w.x f26,  zero
    fmv.w.x f27,  zero
    fmv.w.x f28,  zero
    fmv.w.x f29,  zero
    fmv.w.x f30,  zero
    fmv.w.x f31,  zero
    fscsr zero
.endm

.macro fpContextSaveCaller
    fsw     f0,   FPREGBYTES * 0(s1)
    fsw     f1,   FPREGBYTES * 1(s1)
    fsw     f2,   FPREGBYTES * 2(s1)
    fsw     f3,   FPREGBYTES * 3(s1)
    fsw     f4,   FPREGBYTES * 4(s1)
    fsw     f5,   FPREGBYTES * 5(s1)
    fsw     f6,   FPREGBYTES * 6(s1)
    fsw     f7,   FPREGBYTES * 7(s1)
    fsw     f10,  FPREGBYTES * 10(s1)
    fsw     f11,  FPREGBYTES * 11(s1)
    fsw     f12,  FPREGBYTES * 12(s1)
    fsw     f13,  FPREGBYTES * 13(s1)
    fsw     f14,  FPREGBYTES * 14(s1)
    fsw     f15,  FPREGBYTES * 15(s1)
    fsw     f16,  FPREGBYTES * 16(s1)
    fsw     f17,  FPREGBYTES * 17(s1)
    fsw     f28,  FPREGBYTES * 28(s1)
    fsw     f29,  FPREGBYTES * 29(s1)
    fsw     f30,  FPREGBYTES * 30(s1)
    fsw     f31,  FPREGBYTES * 31(s1)
.endm

.macro fpContextRestoreCaller
    flw     f0,   FPREGBYTES * 0(s1)
    flw     f1,   FPREGBYTES * 1(s1)
    flw     f2,   FPREGBYTES * 2(s1)
    flw     f3,   FPREGBYTES * 3(s1)
    flw     f4,   FPREGBYTES * 4(s1)
    flw     f5,   FPREGBYTES * 5(s1)
    flw     f6,   FPREGBYTES * 6(s1)
    flw     f7,   FPREGBYTES * 7(s1)
    flw     f10,  FPREGBYTES * 10(s1)
    flw     f11,  FPREGBYTES * 11(s1)
    flw     f12,  FPREGBYTES * 12(s1)
    flw     f13,  FPREGBYTES * 13(s1)
    flw     f14,  FPREGBYTES * 14(s1)
    flw     f15,  FPREGBYTES * 15(s1)
    flw     f16,  FPREGBYTES * 16(s1)
    flw     f17,  FPREGBYTES * 17(s1)
    flw     f28,  FPREGBYTES * 28(s1)
    flw     f29,  FPREGBYTES * 29(s1)
    flw     f30,  FPREGBYTES * 30(s1)
    flw     f31,  FPREGBYTES * 31(s1)
.endm

.macro fpContextSaveCallee
    fsw     f8,    FPREGBYTES * 8(s1)
    fsw     f9,    FPREGBYTES * 9(s1)
    fsw     f18,   FPREGBYTES * 18(s1)
    fsw     f19,   FPREGBYTES * 19(s1)
    fsw     f20,   FPREGBYTES * 20(s1)
    fsw     f21,   FPREGBYTES * 21(s1)
    fsw     f22,   FPREGBYTES * 22(s1)
    fsw     f23,   FPREGBYTES * 23(s1)
    fsw     f24,   FPREGBYTES * 24(s1)
    fsw     f25,   FPREGBYTES * 25(s1)
    fsw     f26,   FPREGBYTES * 26(s1)
    fsw     f27,   FPREGBYTES * 27(s1)
.endm

.macro fpContextRestoreCallee
    flw     f8,    FPREGBYTES * 8(s1)
    flw     f9,    FPREGBYTES * 9(s1)
    flw     f18,   FPREGBYTES * 18(s1)
    flw     f19,   FPREGBYTES * 19(s1)
    flw     f20,   FPREGBYTES * 20(s1)
    flw     f21,   FPREGBYTES * 21(s1)
    flw     f22,   FPREGBYTES * 22(s1)
    flw     f23,   FPREGBYTES * 23(s1)
    flw     f24,   FPREGBYTES * 24(s1)
    flw     f25,   FPREGBYTES * 25(s1)
    flw     f26,   FPREGBYTES * 26(s1)
    flw     f27,   FPREGBYTES * 27(s1)
.endm

/* We need to save RA and A0 always, even if they're caller saved because:
 * A0: it is return value
 * RA: Because we call into C from assembly (so must preserve it) */
.macro contextSaveRaA0
    SREG    x1,  0 * REGBYTES(gp)   // ra
    SREG    x10, 9 * REGBYTES(gp)  // a0
.endm

.macro contextRestoreRaA0
    LREG    x1,  0 * REGBYTES(gp)   // ra
    LREG    x10, 9 * REGBYTES(gp)  // a0
.endm

/* Save caller registers: t0-t6, a1-a7
 * a0 and ra are saved separately. */
.macro contextSaveCaller
    SREG    x5,   4 * REGBYTES(gp) // t0
    SREG    x6,   5 * REGBYTES(gp) // t1
    SREG    x7,   6 * REGBYTES(gp) // t2
    SREG    x11, 10 * REGBYTES(gp) // a1
    SREG    x12, 11 * REGBYTES(gp) // a2
    SREG    x13, 12 * REGBYTES(gp) // a3
    SREG    x14, 13 * REGBYTES(gp) // a4
    SREG    x15, 14 * REGBYTES(gp) // a5
    SREG    x16, 15 * REGBYTES(gp) // a6
    SREG    x17, 16 * REGBYTES(gp) // a7
    SREG    x28, 27 * REGBYTES(gp) // t3
    SREG    x29, 28 * REGBYTES(gp) // t4
    SREG    x30, 29 * REGBYTES(gp) // t5
    SREG    x31, 30 * REGBYTES(gp) // t6
.endm

/* Restore caller registers: t0-t6, a1-a7
 * a0 and ra are restored separately. */
.macro contextRestoreCaller
    LREG    x5,   4 * REGBYTES(gp) // t0
    LREG    x6,   5 * REGBYTES(gp) // t1
    LREG    x7,   6 * REGBYTES(gp) // t2
    LREG    x11, 10 * REGBYTES(gp) // a1
    LREG    x12, 11 * REGBYTES(gp) // a2
    LREG    x13, 12 * REGBYTES(gp) // a3
    LREG    x14, 13 * REGBYTES(gp) // a4
    LREG    x15, 14 * REGBYTES(gp) // a5
    LREG    x16, 15 * REGBYTES(gp) // a6
    LREG    x17, 16 * REGBYTES(gp) // a7
    LREG    x28, 27 * REGBYTES(gp) // t3
    LREG    x29, 28 * REGBYTES(gp) // t4
    LREG    x30, 29 * REGBYTES(gp) // t5
    LREG    x31, 30 * REGBYTES(gp) // t6
.endm

/* Zero caller registers: t0-t6, a1-a7
 * a0 and ra are not cleared. */
.macro contextZeroCaller
    ZREG(x5)
    ZREG(x6)
    ZREG(x7)
    ZREG(x11)
    ZREG(x12)
    ZREG(x13)
    ZREG(x14)
    ZREG(x15)
    ZREG(x16)
    ZREG(x17)
    ZREG(x28)
    ZREG(x29)
    ZREG(x30)
    ZREG(x31)
.endm

/* Save callee registers. Assume old gp is in sscratch.
 * Regs saved: sp, tp, gp, s0-s11, mepc, mstatus, mie */
.macro contextSaveCallee
    SREG    x2,   1 * REGBYTES(gp) // sp
    SREG    x4,   3 * REGBYTES(gp) // tp
    SREG    x8,   7 * REGBYTES(gp) // s0/fp
    SREG    x9,   8 * REGBYTES(gp) // s1
    SREG    x18, 17 * REGBYTES(gp) // s2
    SREG    x19, 18 * REGBYTES(gp) // s3
    SREG    x20, 19 * REGBYTES(gp) // s4
    SREG    x21, 20 * REGBYTES(gp) // s5
    SREG    x22, 21 * REGBYTES(gp) // s6
    SREG    x23, 22 * REGBYTES(gp) // s7
    SREG    x24, 23 * REGBYTES(gp) // s8
    SREG    x25, 24 * REGBYTES(gp) // s9
    SREG    x26, 25 * REGBYTES(gp) // s10
    SREG    x27, 26 * REGBYTES(gp) // s11

#if ( configLW_FPU_SUPPORTED == 1U )
    // if dirty, save
    csrr   s0, sstatus
    bge    s0, zero, 1f

    lh     s0, REGBYTES * TCB_OFFSET_FPU_CTX_OFFSET(gp)
    add    s1, s0, gp

    frcsr  s0
    SREG   s0, FPREGBYTES * FPU_CTX_OFFSET_FP_STATUS (s1)
    fpContextSaveCallee
    fpContextSaveCaller

    // clear the 13th  bit (dirty 3 - 1 = 2)
    li     s1, 1 << 13
    csrrc  zero, sstatus, s1

1: // Skip FP save, normal save-callee sequence
#endif /* ( configLW_FPU_SUPPORTED == 1U ) */

    /* Use s0 as temp for CSR */
    csrr    s0, sscratch
    SREG    s0, 2 * REGBYTES(gp) // save gp

    csrr    s0, sepc
    SREG    s0, TCB_OFFSET_PC * REGBYTES(gp)
    csrr    s0, sstatus
    SREG    s0, TCB_OFFSET_SSTATUS * REGBYTES(gp)
    csrr    s0, sie
    SREG    s0, TCB_OFFSET_SIE * REGBYTES(gp)

#if ( configLW_FPU_SUPPORTED == 1U )
    //
    // Clear the 13th and 14th bit when running in kernel mode
    // to always set FPU state to off.
    //
    li     s1, 3 << 13
    csrrc  zero, sstatus, s1
#endif /* ( configLW_FPU_SUPPORTED == 1U ) */
.endm

/* Restore callee registers. gp *is* overwritten. Assume gp points to context.
 * Important: assume that xPortExceptionNesting has been decremented already!
 * After that macro completes, we may do ERET safely
 * Regs restored: sp, tp, gp, s0-s11, mepc, sstatus (part), mie
 * Uses restored (s0-) regs so should have no side effects */
.macro contextRestoreCallee
    /* Restore CSRs (mepc, mstatus, mie / sepc. sstatus, sie) first, use s0-s1 as scratch if necessary. */
    LREG    s0,   TCB_OFFSET_PC * REGBYTES(gp)      // xepc
    csrw    sepc, s0

    LREG    s0, TCB_OFFSET_SSTATUS * REGBYTES(gp)   // sstatus
    li      s1, sstatusReturnBits                   // bits to keep from TCB's sstatus
    and     s0, s0, s1

    not     s1, s1                                  // ilwert the bitmask
    csrr    s2, sstatus
    and     s1, s1, s2                              // keep all the other bits from current sstatus

    or      s0, s0, s1                              // merge

    ld      s1, xPortExceptionNesting               // Start sstatus interrupt bit (xPIE) check
    beqz    s1, 1f                                  // If we're returning and nesting == 0, skip check
    and     s1, s0, sstatusInterruptCheckBits
    bnez    s1, vPortInfiniteLoop                   // If the bits we check (xPIE) are non-zero, we need to crash!
                                                    // If we get to sret with xPIE == 1, we end up with s-mode intr
                                                    // enabled on xret, which is fatal if we're in a nested trap handler!
1:
    csrw    sstatus, s0                             // write sstatus back

    /* Restore SIE */
    ld      s1, TCB_OFFSET_SIE * REGBYTES(gp)
    csrw    sie, s1

#if ( configLW_FPU_SUPPORTED == 1U )
    la      s2, xPortExceptionNesting
    ld      s1, 0(s2)
    bnez    s1, 2f                   // skip FP restore on nested exceptions

    srli    s2, s0, 13               // shift 13 times to get fs bits
    andi    s2, s2, 0x3              // get just the mask
    beq     s2, zero, 2f             // Skip FP restore

    li      s1, LW_RISCV_CSR_SSTATUS_FS_CLEAN
    bltu    s2, s1, 1f // if its not clean, then its initial

    lh      s2, REGBYTES * TCB_OFFSET_FPU_CTX_OFFSET(gp)
    add     s1, s2, gp

    LREG    s2, FPREGBYTES * FPU_CTX_OFFSET_FP_STATUS (s1)
    fscsr   s2
    fpContextRestoreCallee
    fpContextRestoreCaller
    csrw    sstatus, s0

    j 2f                             // Avoid initialization

1: // FpRestoreInitial
    fpContextInit                    // set all values to zero
    csrw    sstatus, s0

2: // Skip FP restore
#endif /* ( configLW_FPU_SUPPORTED == 1U ) */

    /* Restore GPR */
    LREG    x2,   1 * REGBYTES(gp) // sp
    LREG    x4,   3 * REGBYTES(gp) // tp
    LREG    x8,   7 * REGBYTES(gp) // s0/fp
    LREG    x9,   8 * REGBYTES(gp) // s1
    LREG    x18, 17 * REGBYTES(gp) // s2
    LREG    x19, 18 * REGBYTES(gp) // s3
    LREG    x20, 19 * REGBYTES(gp) // s4
    LREG    x21, 20 * REGBYTES(gp) // s5
    LREG    x22, 21 * REGBYTES(gp) // s6
    LREG    x23, 22 * REGBYTES(gp) // s7
    LREG    x24, 23 * REGBYTES(gp) // s8
    LREG    x25, 24 * REGBYTES(gp) // s9
    LREG    x26, 25 * REGBYTES(gp) // s10
    LREG    x27, 26 * REGBYTES(gp) // s11

    /* Restore gp */
    LREG    gp, 2 * REGBYTES(gp)
    /* At that point we're good to eret */
.endm


.macro inlineIcd reg
    li      \reg, 0xc0111300
    sw      zero, 0(\reg)
    fence   io,io
.endm


.macro optimizedSyscallFuncContextSave
    /*
     * An optimized syscall might want to call into a function.
     * This function needs to have a proper stack, as well as valid tp and gp.
     * We load the kernel stack and kernel TLS while saving the original values
     * in sscratch and sscratch2. We also restore TCB just in case the function call
     * might need it.
     */

    li      gp, 0 // zero out gp to make sure that the syscall handler will fail if it ever tries to access it

    csrw    sscratch, sp // save user sp. For an optimized syscall we know we must have come from user
    ld      sp, pxPortKernelStackTop // We came from user, switch to (fresh) kernel stack

    csrw    LW_RISCV_CSR_SSCRATCH2, tp // save user tp
    la      tp, kernelTls // Load kernel-level TLS into tp
.endm

.macro optimizedSyscallFuncContextRestore
    csrr    tp, LW_RISCV_CSR_SSCRATCH2 // restore user tp
    csrr    sp, sscratch // restore user sp

    // Zero out caller-saved registers used by the function just to be safe, except for a0.
    // The syscall callsite will have saved all caller-saved registers that it needs.
    contextZeroCaller
    li      ra, 0 // contextZeroCaller doesn't clear ra or a0. a0 has retval, but ra we can clear just in case

    ld      gp, pxLwrrentTCB // We know we came from a task, so this should always be valid
    csrw    sscratch, gp // save gp to sscratch again for further use, some of our code expects sscratch to hold gp
.endm

// This goes before an optimized syscall body.
// Note: the optimized syscall case macros must follow each other.
.macro optimizedSyscallCasePre idbase, id
    addi    t0, \idbase, \id
    bne     t0, a7, 1f // try next optimized syscall check
.endm

// This goes after an optimized syscall body.
// Note: the optimized syscall case macros must follow each other.
.macro optimizedSyscallCasePost
    j       __PortOptimizedSyscallsDone

1: // Marker for next optimized syscall check
.endm

#ifndef SAFE_RTOS_BUILD
#error This header file must not be included outside SafeRTOS build.
#endif

#endif /* PORTASMMACRO_H */
