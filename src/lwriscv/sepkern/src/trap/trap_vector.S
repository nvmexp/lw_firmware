/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "config.h"
#include "reg_access.h"

    .section .text.trapHandler
///////////////////////////////////////////////////////////////////////////////////////////////////
    .balign 4
    .global trapEntry
trapEntry:
    // Save t0 to mscratch so we have a register to use
    csrw    mscratch, t0

    // MSb indicates interrupt if set, exception if cleared 
    csrr    t0, mcause
    bgez    t0, trapHandleException

///////////////////////////////////////////////////////////////////////////////////////////////////
trapHandleInterrupt:
    // Shift away top bit (we already parsed it)
    slli    t0, t0, 1

    // Check the causes we care about
    //// CAUSE==7: Machine Timer Interrupt
    addi    t0, t0, -(LW_RISCV_CSR_MCAUSE_EXCODE_M_TINT << 1)
    beqz    t0, trapHandleTimerInterrupt

    //// Unhandled:
trapUnhandledInterrupt:
    j       halt

///////////////////////////////////////////////////////////////////////////////////////////////////
trapHandleTimerInterrupt:
    // Set Supervisor Timer pending
    li      t0, LW_RISCV_CSR_MIP_STIP__SHIFTMASK
    csrs    mip, t0
    // Disable the interrupt for machine mode, since we can't ack the timer ourselves
    li      t0, LW_RISCV_CSR_MIE_MTIE__SHIFTMASK
    csrc    mie, t0

    // Restore temporary register
    csrr    t0, mscratch
    mret

///////////////////////////////////////////////////////////////////////////////////////////////////
trapHandleException:
    // Check the causes we care about
    //// CAUSE==9: Environment call from S-mode
    addi    t0, t0, -LW_RISCV_CSR_MCAUSE_EXCODE_SCALL
    beqz    t0, trapHandleScallException

    //// Unhandled:
trapUnhandledException:
    j       halt

///////////////////////////////////////////////////////////////////////////////////////////////////
trapHandleScallException:
    // TODO: This is a hack to support our misbehaving SafeRTOS.
    // We MUST NOT use ECALL from S-mode, unless we are making SBI calls. However, we lwrrently do
    // and the rework is significant. Until then, we have this hack that allows us to simulate
    // an ECALL from S-mode to S-mode.
    // We cannot write EXCODE_SCALL to scause, so we must also fake it as an ECALL from
    // U-mode to S-mode.
    csrwi   mcause, LW_RISCV_CSR_MCAUSE_EXCODE_UCALL
    // High bit in SBI extension ID indicates a pass-through ECALL.
    bltz    a7, trapPassThrough

    // Do not allow nested traps
    la      t0, halt
    csrw    mtvec, t0

    // Perform saving as according to ABI, and switch to SK stack
    mv      t0, sp
    csrr    sp, LW_RISCV_CSR_MSCRATCH2
    addi    sp, sp, -15 * XREGSIZE
    sd      s0,  0 * XREGSIZE(sp)
    sd      s1,  1 * XREGSIZE(sp)
    sd      s2,  2 * XREGSIZE(sp)
    sd      s3,  3 * XREGSIZE(sp)
    sd      s4,  4 * XREGSIZE(sp)
    sd      s5,  5 * XREGSIZE(sp)
    sd      s6,  6 * XREGSIZE(sp)
    sd      s7,  7 * XREGSIZE(sp)
    sd      s8,  8 * XREGSIZE(sp)
    sd      s9,  9 * XREGSIZE(sp)
    sd      s10,10 * XREGSIZE(sp)
    sd      s11,11 * XREGSIZE(sp)
    sd      gp, 12 * XREGSIZE(sp)
    sd      tp, 13 * XREGSIZE(sp)
    sd      t0, 14 * XREGSIZE(sp) // old SP
    // Zap registers we don't want ever used
    mv      ra, x0

    jal     sbiDispatch
    // The SBI interface defines a1 as the return value, and a0 as the error code.
    // sbiDispatch returns them in the register pair as expected.

    // Skip over the ecall instruction that took us here
    csrr    t0, mepc
    add     t0, t0, 4
    csrw    mepc, t0

    // Restore saved registers
    ld      s0,  0 * XREGSIZE(sp)
    ld      s1,  1 * XREGSIZE(sp)
    ld      s2,  2 * XREGSIZE(sp)
    ld      s3,  3 * XREGSIZE(sp)
    ld      s4,  4 * XREGSIZE(sp)
    ld      s5,  5 * XREGSIZE(sp)
    ld      s6,  6 * XREGSIZE(sp)
    ld      s7,  7 * XREGSIZE(sp)
    ld      s8,  8 * XREGSIZE(sp)
    ld      s9,  9 * XREGSIZE(sp)
    ld      s10,10 * XREGSIZE(sp)
    ld      s11,11 * XREGSIZE(sp)
    ld      gp, 12 * XREGSIZE(sp)
    ld      tp, 13 * XREGSIZE(sp)
    ld      sp, 14 * XREGSIZE(sp) // old SP
    // Clear temporary registers, to avoid leaking state
    mv      t0, x0
    mv      t1, x0
    mv      t2, x0
    mv      t3, x0
    mv      t4, x0
    mv      t5, x0
    mv      t6, x0
    // Clear argument registers, to avoid leaking state (returns are in a0 and a1)
    mv      a2, x0
    mv      a3, x0
    mv      a4, x0
    mv      a5, x0
    mv      a6, x0
    mv      a7, x0

    // Restore trap vector
    la      ra, trapEntry
    csrw    mtvec, ra
    mv      ra, x0

    mret

///////////////////////////////////////////////////////////////////////////////////////////////////
trapPassThrough:
    // Copy trap information to S-mode
    csrr    t0, mtval
    csrw    stval, t0
    csrr    t0, mepc
    csrw    sepc, t0
    csrr    t0, mcause
    csrw    scause, t0
    csrr    t0, LW_RISCV_CSR_MCAUSE2
    csrw    LW_RISCV_CSR_SCAUSE2, t0
    // Return to the S-mode trap vector
    csrr    t0, stvec
    csrw    mepc, t0

    // Simulate trap entry to S-mode:
    // 1) Clear MSTATUS.SIE
    li      t0, LW_RISCV_CSR_MSTATUS_SIE__SHIFTMASK | LW_RISCV_CSR_MSTATUS_SPIE__SHIFTMASK
    csrrc   t0, mstatus, t0
    // 2) Copy old MSTATUS.SIE to MSTATUS.SPIE
    andi    t0, t0, LW_RISCV_CSR_MSTATUS_SIE__SHIFTMASK
    beqz    t0, 1f
    li      t0, LW_RISCV_CSR_MSTATUS_SPIE__SHIFTMASK
    csrs    mstatus, t0
1:
    // 3) Move MSTATUS.MPP to MSTATUS.SPP
    li      t0, LW_RISCV_CSR_MSTATUS_SPP__SHIFTMASK
    csrrc   t0, mstatus, t0
    srli    t0, t0, LW_RISCV_CSR_MSTATUS_MPP__SHIFT
    andi    t0, t0, 1
    beqz    t0, 1f
    li      t0, LW_RISCV_CSR_MSTATUS_SPP__SHIFTMASK
    csrs    mstatus, t0
1:
    // 4) Set MSTATUS.MPP to S mode
    li      t0, LW_RISCV_CSR_MSTATUS_MPP__SHIFTMASK_B0
    csrs    mstatus, t0
    slli    t0, t0, 1
    csrc    mstatus, t0

    // Restore temporary register
    csrr    t0, mscratch
    mret
