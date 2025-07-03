/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

 /*!
 * @file        start.S
 * @brief       Startup file.
 */

// Exclude header portions only meant for C code.
#define AS_ILWOKED


// LWRISCV includes.
#include <lwriscv/asm-helpers.h>


// Program entry-point.
.section ".text.startup","ax",@progbits
.balign 4
FUNC startup
    // Clear all GPRs.
    ZREG(x1)
    ZREG(x2)
    ZREG(x3)
    ZREG(x4)
    ZREG(x5)
    ZREG(x6)
    ZREG(x7)
    ZREG(x8)
    ZREG(x9)
    ZREG(x10)
    ZREG(x11)
    ZREG(x12)
    ZREG(x13)
    ZREG(x14)
    ZREG(x15)
    ZREG(x16)
    ZREG(x17)
    ZREG(x18)
    ZREG(x19)
    ZREG(x20)
    ZREG(x21)
    ZREG(x22)
    ZREG(x23)
    ZREG(x24)
    ZREG(x25)
    ZREG(x26)
    ZREG(x27)
    ZREG(x28)
    ZREG(x29)
    ZREG(x30)
    ZREG(x31)

    // Install exception-handler.
    la    t0, exceptionWrapper
    csrw  mtvec, t0
    csrw  stvec, t0

    // Set up stack pointer.
    la    sp, _liblwriscv_stack_bottom

    // Call main().
    addi  sp, sp, -0x10
    sd    zero, 0(sp)
    sd    zero, 8(sp)
    jal   main

    // Halt the core.
    jal   riscvShutdown

    // Infinite loop in case shutdown fails.
_end:
    j _end
EFUNC startup
