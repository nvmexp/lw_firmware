/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   load.S
 * @brief  Transfers control to new exelwtable.
 */

#include "dmem_addrs.h"
#include "riscv_asm_helpers.h"

        .section        .text
FUNC bootLoad
        /* Save the entrypoint to ra */
        mv      ra, a0

        /* Clean stack (assumed to be 16-byte aligned) */
        li      t0, DMEM_STACK_BASE
        addi    t1, t0, STACK_SIZE
_scrubStart:
        beq     t0, t1, _scrubEnd
        sd      zero, 0(t0)
        sd      zero, 8(t0)
        addi    t0, t0, 16
        j       _scrubStart
_scrubEnd:

        /* Set up argument registers. */
        ZREG(a0)    /* bootargsPa = NULL */
        ZREG(a1)    /* bootargsVa = NULL */
        ZREG(a2)    /* elfPa      = NULL */
        ZREG(a3)    /* elfSize    = 0    */
        ZREG(a4)    /* loadBase   = NULL */
        ZREG(a5)    /* wprId      = 0    */

        /* Clear all other registers. */
        ZREG(a6)
        ZREG(a7)
        ZREG(sp)
        ZREG(gp)
        ZREG(tp)
        ZREG(t0)
        ZREG(t1)
        ZREG(t2)
        ZREG(t3)
        ZREG(t4)
        ZREG(t5)
        ZREG(t6)
        ZREG(s0)
        ZREG(s1)
        ZREG(s2)
        ZREG(s3)
        ZREG(s4)
        ZREG(s5)
        ZREG(s6)
        ZREG(s7)
        ZREG(s8)
        ZREG(s9)
        ZREG(s10)
        ZREG(s11)

/* Ensure memory operations all finished */
        fence
/* Flush the just written program for instruction fetch */
        fence.i

/* Jump into new program */
        jr      ra
EFUNC bootLoad
