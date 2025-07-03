/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PORTASMMACRO_H
#define PORTASMMACRO_H

//TODO: we might want to pull portcommon or csr includes as well
//#include "portcommon.h"

#define REGBYTES        8
#define SREG            sd
#define LREG            ld
#define ZREG(rd)        mv rd, zero
#define CONTEXT_SIZE    (31 * REGBYTES)
.macro FUNC name
   .global \name
   \name :
   .func \name, \name
    .type \name, STT_FUNC
.endm

.macro EFUNC name
    .endfunc
    .size \name, .-\name
.endm

.macro contextSave
    SREG    x1,   0 * REGBYTES(sp) // ra
    SREG    x2,   1 * REGBYTES(sp) // sp
    SREG    x3,   2 * REGBYTES(sp) // gp?
    SREG    x4,   3 * REGBYTES(sp) // tp
    SREG    x5,   4 * REGBYTES(sp) // t0
    SREG    x6,   5 * REGBYTES(sp) // t1
    SREG    x7,   6 * REGBYTES(sp) // t2
    SREG    x8,   7 * REGBYTES(sp) // s0/fp
    SREG    x9,   8 * REGBYTES(sp) // s1
    SREG    x10,  9 * REGBYTES(sp) // a0
    SREG    x11, 10 * REGBYTES(sp) // a1
    SREG    x12, 11 * REGBYTES(sp) // a2
    SREG    x13, 12 * REGBYTES(sp) // a3
    SREG    x14, 13 * REGBYTES(sp) // a4
    SREG    x15, 14 * REGBYTES(sp) // a5
    SREG    x16, 15 * REGBYTES(sp) // a6
    SREG    x17, 16 * REGBYTES(sp) // a7
    SREG    x18, 17 * REGBYTES(sp) // s2
    SREG    x19, 18 * REGBYTES(sp) // s3
    SREG    x20, 19 * REGBYTES(sp) // s4
    SREG    x21, 20 * REGBYTES(sp) // s5
    SREG    x22, 21 * REGBYTES(sp) // s6
    SREG    x23, 22 * REGBYTES(sp) // s7
    SREG    x24, 23 * REGBYTES(sp) // s8
    SREG    x25, 24 * REGBYTES(sp) // s9
    SREG    x26, 25 * REGBYTES(sp) // s10
    SREG    x27, 26 * REGBYTES(sp) // s11
    SREG    x28, 27 * REGBYTES(sp) // t3
    SREG    x29, 28 * REGBYTES(sp) // t4
    SREG    x30, 29 * REGBYTES(sp) // t5
    SREG    x31, 30 * REGBYTES(sp) // t6
.endm

.macro contextRestore
    LREG    x1,   0 * REGBYTES(sp) // ra
    LREG    x2,   1 * REGBYTES(sp) // sp
    LREG    x3,   2 * REGBYTES(sp) // gp?
    LREG    x4,   3 * REGBYTES(sp) // tp
    LREG    x5,   4 * REGBYTES(sp) // t0
    LREG    x6,   5 * REGBYTES(sp) // t1
    LREG    x7,   6 * REGBYTES(sp) // t2
    LREG    x8,   7 * REGBYTES(sp) // s0/fp
    LREG    x9,   8 * REGBYTES(sp) // s1
    LREG    x10,  9 * REGBYTES(sp) // a0
    LREG    x11, 10 * REGBYTES(sp) // a1
    LREG    x12, 11 * REGBYTES(sp) // a2
    LREG    x13, 12 * REGBYTES(sp) // a3
    LREG    x14, 13 * REGBYTES(sp) // a4
    LREG    x15, 14 * REGBYTES(sp) // a5
    LREG    x16, 15 * REGBYTES(sp) // a6
    LREG    x17, 16 * REGBYTES(sp) // a7
    LREG    x18, 17 * REGBYTES(sp) // s2
    LREG    x19, 18 * REGBYTES(sp) // s3
    LREG    x20, 19 * REGBYTES(sp) // s4
    LREG    x21, 20 * REGBYTES(sp) // s5
    LREG    x22, 21 * REGBYTES(sp) // s6
    LREG    x23, 22 * REGBYTES(sp) // s7
    LREG    x24, 23 * REGBYTES(sp) // s8
    LREG    x25, 24 * REGBYTES(sp) // s9
    LREG    x26, 25 * REGBYTES(sp) // s10
    LREG    x27, 26 * REGBYTES(sp) // s11
    LREG    x28, 27 * REGBYTES(sp) // t3
    LREG    x29, 28 * REGBYTES(sp) // t4
    LREG    x30, 29 * REGBYTES(sp) // t5
    LREG    x31, 30 * REGBYTES(sp) // t6
.endm

.macro contextZero
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
.endm

.macro inlineIcd reg
    li      \reg, 0xc0111300
    sw      zero, 0(\reg)
    fence   io,io
.endm

#endif /* ASMMACRO_H */
