/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
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
/* Shift registers around and clear ones that we won't use. */

        /* Save the entrypoint to ra */
        mv      ra, a0

        /* Too many arguments so get WPR ID off the stack before we scrub it */
        ld      t0, 0(sp)

        /* Clean stack */
        beq     a3, a4, skipStackScrubbing
1:
        /* Stack should be at least 16B aligned */
        sd      zero, 0(a3)
        sd      zero, 8(a3)
        addi    a3, a3, 16
        bne     a3, a4, 1b
skipStackScrubbing:

        /* Shift a1 to a0 (this is bootargsPa pointer) */
        mv      a0, a1
        /* Shift a2 to a1 (this is bootargsVa pointer) */
        mv      a1, a2
        /* Shift a5 to a2 (this is ELF pointer) */
        mv      a2, a5
        /* Shift a6 to a3 (this is ELF size) */
        mv      a3, a6
        /* Shift a7 to a4 (this is load base) */
        mv      a4, a7
        /* Shift t0 to a5 (this is wprId) */
        mv      a5, t0

        /* Clean remaining argument registers. */
        ZREG(a6)
        ZREG(a7)
        /* Clear all other registers */
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

