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

///////////////////////////////////////////////////////////////////////////////////////////////////
    .section .text.startup
    .global startup
startup:
    // Validate mcause = instruction access fault
    csrr    t0, mcause
    li      t1, 1
    bne     t0, t1, halt
    csrr    t0, mtvec
    la      t1, startup
    bne     t0, t1, halt

#if BOOT_PROFILING
    // TODO
#endif

    la      t0, trapEntry
    csrw    mtvec, t0

    // mscratch2 always contains the monitor ISR stack
    la      t0, __monitor_isr_stack_top
    csrw    LW_RISCV_CSR_MSCRATCH2, t0

    la      sp, __monitor_main_stack_top
    jal     partitionBootInitial
    // Set up arguments
    // return value is in a0, and has the SBI version
    csrr    a1, misa
    csrr    a2, marchid
    csrr    a3, mimpid
    csrr    a4, mhartid
    mv      a5, x0
    mv      a6, x0
    mv      a7, x0

    // Clear branch predictors
    li      t0, LW_RISCV_CSR_MBPCFG_ALL_FLUSH
    csrw    LW_RISCV_CSR_MBPCFG, t0

    // Clear remaining registers to avoid leaking state
    mv      t0, x0
    mv      t1, x0
    mv      t2, x0
    mv      t3, x0
    mv      t4, x0
    mv      t5, x0
    mv      t6, x0
    mv      s0, x0
    mv      s1, x0
    mv      s2, x0
    mv      s3, x0
    mv      s4, x0
    mv      s5, x0
    mv      s6, x0
    mv      s7, x0
    mv      s8, x0
    mv      s9, x0
    mv      s10,x0
    mv      s11,x0
    mv      ra, x0
    mv      sp, x0
    mv      gp, x0
    mv      tp, x0

    fence
    fence.i

    // Transfer exelwtion
    mret

///////////////////////////////////////////////////////////////////////////////////////////////////
    .global halt
halt:
    // Wait loop forever
    wfi
    j       halt

///////////////////////////////////////////////////////////////////////////////////////////////////
    .section .bss.stack
// Since monitor does not actively "run", the ISR stack is the same as the main stack.
__monitor_main_stack_bottom:
__monitor_isr_stack_bottom:
    .space STACK_SIZE
__monitor_main_stack_top:
__monitor_isr_stack_top:
