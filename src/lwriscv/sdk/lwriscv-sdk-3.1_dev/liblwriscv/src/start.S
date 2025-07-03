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

.section ".text.startup","ax",@progbits
.balign 4
FUNC startup
    j    do_reset

do_reset:
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

    // write mtvec and make sure it sticks
#if LWRISCV_FEATURE_EXCEPTION == 1
    la    t0, exceptionEntry
#else
    la    t0, riscvPanicTrampoline
#endif

#if (LWRISCV_CONFIG_CPU_MODE==3)
    csrw  mtvec, t0
#endif
#if (LWRISCV_HAS_S_MODE == 1)
    csrw  stvec, t0
#endif

    // write mrsp on Hopper/LS10 due to HW bug
    // See https://lwbugs/3342254
#if ((LWRISCV_IS_CHIP_ls10 == 1) || (LWRISCV_IS_CHIP_gh100==1)) && (LWRISCV_CONFIG_CPU_MODE==3)
    csrw LW_RISCV_CSR_MRSP, zero
#endif

    la    sp, _stack_bottom
    LREG  sp, 0(sp)
    addi  sp, sp, -0x10
    SREG  zero, 0(sp)
    SREG  zero, 8(sp)
    jal   main
    // halt the core one way or another
    jal   riscvShutdown
_end:
    j _end
EFUNC startup

#if LWRISCV_FEATURE_EXCEPTION == 0
.balign 4
FUNC riscvPanicTrampoline
    jal riscvPanic
EFUNC riscvPanicTrampoline
#endif

// We need that dereference to handle pre-ga10x where dmem is far away from imem
  .balign 8
_stack_bottom:
  .dword _liblwriscv_stack_bottom
