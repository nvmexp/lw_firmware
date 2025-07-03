/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  jsvp.h
 * @brief Relevant stuff from vp.h
 *
 *
 */

#ifndef INCLUDED_JSVP_H
#define INCLUDED_JSVP_H

#define  VREGNUM        32 // number of vector registers
#define  HREGNUM        32 // number of host interface registers
#define  UREGNUM        32 // number of ucode controller registers
#define  BREGNUM        4  // number of branch registers
#define  AREGNUM        32 // number of address registers
#define  SREGNUM        32 // number of scalar registers
#define  MREGNUM        32 // number of memory interface registers
#define  CREGNUM        4  // number of condition code registers
#define  CCNUM       16 // number of condition codes per cc register

// enumerated
#define  RND_N       0x0   // round to nearest
#define  RND_NI         0x1   // round to negative infinity

#define  HI0L        0x0   // host interface context 0 registers 0-31
#define  HI0H        0x1   // host interface context 0 registers 32-63
#define  HI1L        0x2   // host interface context 1 registers 0-31
#define  HI1H        0x3   // host interface context 1 registers 32-63

// vp units (32 register address spaces)
#define  S_UNIT_VR0     0x0   // 5 bits: vector registers 0
#define  S_UNIT_VR1     0x1   // 5 bits: vector registers 1
#define  S_UNIT_VR2     0x2   // 5 bits: vector registers 2
#define  S_UNIT_VR3     0x3   // 5 bits: vector registers 3
#define  S_UNIT_HI0L    0x4   // 5 bits: host interface context 0 registers 0-31
#define  S_UNIT_HIL     0x14  // 5 bits: host interface current context regs 0-31
#define  S_UNIT_HI0H    0x5   // 5 bits: host interface context 0 registers 32-63
#define  S_UNIT_HIH     0x15  // 5 bits: host interface current context regs 32-63
#define  S_UNIT_HI1L    0x6   // 5 bits: host interface context 1 registers 0-31
#define  S_UNIT_HI1H    0x7   // 5 bits: host interface context 1 registers 32-63
#define  S_UNIT_HI      0x8   // 5 bits: host interface priv registers
#define  S_UNIT_MI      0x9   // 5 bits: memory interface registers
#define  S_UNIT_UC      0xa   // 5 bits: ucode controller registers
#define  S_UNIT_BR      0xb   // 5 bits: branch registers
#define  S_UNIT_AR      0xc   // 5 bits: address registers
#define  S_UNIT_CR      0xd   // 5 bits: address/scalar condition code registers
#define  S_UNIT_VCR     0xe   // 5 bits: address/scalar condition code registers
#define  S_UNIT_SR      0xf   // 5 bits: scalar registers
#define S_UNIT_IS    0x10
#define S_UNIT_DS    0x11
#define S_UNIT_VR    0x12

#define I_CACHE_LINE_BITS  10
#define I_CACHE_LINE_SIZE  (1 << I_CACHE_LINE_BITS)
#define I_CACHE_LINE_WORDS (I_CACHE_LINE_SIZE / 4)
#define I_TAG_MASK      ((-1) << I_CACHE_LINE_BITS)

//////////////////////////////////
// register bit fields
//////////////////////////////////

// branch registers
#define  B_START        1  // starting count value (byte field 1, ie. bits 15:8)
#define  B_CNT       0  // current count (byte field 0, ie. bits 7:0)

// scalar registers
#define S_FIX_FRACMSB      19,19 // fixed point fraction msb
#define S_FIX_FRAC      19,12 // fixed point fraction
#define S_FIX_INTLSB    20 // fixed point integer lsb
#define S_FIX_SINT           21,20
#define S_FIX_INT    32,20 // fixed point integer

#define S_FIX_MSB_MASK          (1<<20) // more than this is COUT

#define S_TAP_BOB_0     0  // bob filter kernel tap slope (byte field 3, ie. bits 31:24)
#define S_TAP_BOB_1     2  // bob filter kernel tap offset (byte field 2, ie. bits 23:16)
#define S_TAP_WVE_0     1  // weave filter kernel tap slope (byte field 1, ie. bits 15:8)
#define S_TAP_WVE_1     3  // weave filter kernel tap offset (byte field 0, ie. bits 7:0)

// address/scalar condition code registers
#define  C_SN        0x0   // 4 bits: scalar unit output negative
#define  C_SZ        0x1   // 4 bits: scalar unit output zero
#define  C_SFMSB        0x2   // 4 bits: scalar unit fixed point fraction msb
#define  C_SFCOUT    0x3   // 4 bits: scalar unit fixed point fraction carry out
#define  C_SINT         0x4   // 4 bits: scalar unit integer lsbs
//          0x5   // 4 bits: 0x9 cannot be used because C_SINT is 2 bits wide
//          0x6
//          0x7
#define  C_AN        0x8   // 4 bits: address unit ouput negative
#define  C_AZ        0x9   // 4 bits: address unit ouput zero
#define  C_AEND         0xa   // 4 bits: address unit output crosses buffer marker
//          0xb
//          0xc
#define  C_BZ        0xd   // 4 bits: branch unit output zero
#define C_0       0xe   // 4 bits: never
#define C_1       0xf   // 4 bits: always

#define C_SCC_MASK      0x000F   // mask for scalar condition codes
#define C_SCC_SH     0  // shift for scalar cc's
#define C_ACC_MASK      0x01F0   // mask for address condition codes
#define C_ACC_SH     4  // shift for addr cc's
#define C_BCC_MASK      0x2000   // mask for branch condition codes
#define C_BCC_SH     13 // shift for branch cc's

// condition code registers
#define CC_GEN(reg,bit)    (((reg)&0x3)|(((bit)&0xF)<<2))

#define C0_AEND         ((0x0)|((C_AEND)<<0x2))
#define C0_SINT         ((0x0)|((C_SINT)<<0x2))
#define C0_SN        ((0x0)|((C_SN)<<0x2))
#define C0_SZ        ((0x0)|((C_SZ)<<0x2))
#define C0_BZ        ((0x0)|((C_BZ)<<0x2))
#define C0_0         ((0x0)|((C_0)<<0x2))
#define C0_1         ((0x0)|((C_1)<<0x2))
#define  C0_AN       ((0x0)|((C_AN)<<0x2))
#define  C0_AZ       ((0x0)|((C_AZ)<<0x2))
#define  C0_SFCOUT      ((0x0)|((C_SFCOUT)<<0x2))
#define  C0_SFMSB    ((0x0)|((C_SFMSB)<<0x2))
#define  C0_VN       ((0x0)|((C_VN)<<0x2))
#define  C0_VZ       ((0x0)|((C_VZ)<<0x2))
#define  C0_VTRUE    ((0x0)|((C_VTRUE)<<0x2))
#define  C0_VCARRY      ((0x0)|((C_VCARRY)<<0x2))

#define C1_AEND         ((0x1)|((C_AEND)<<0x2))
#define C1_SINT         ((0x1)|((C_SINT)<<0x2))
#define C1_SN        ((0x1)|((C_SN)<<0x2))
#define C1_SZ        ((0x1)|((C_SZ)<<0x2))
#define C1_BZ        ((0x1)|((C_BZ)<<0x2))
#define C1_0         ((0x1)|((C_0)<<0x2))
#define C1_1         ((0x1)|((C_1)<<0x2))
#define  C1_AN       ((0x1)|((C_AN)<<0x2))
#define  C1_AZ       ((0x1)|((C_AZ)<<0x2))
#define  C1_SFCOUT      ((0x1)|((C_SFCOUT)<<0x2))
#define  C1_SFMSB    ((0x1)|((C_SFMSB)<<0x2))
#define  C1_VN       ((0x1)|((C_VN)<<0x2))
#define  C1_VZ       ((0x1)|((C_VZ)<<0x2))
#define  C1_VTRUE    ((0x1)|((C_VTRUE)<<0x2))
#define  C1_VCARRY      ((0x1)|((C_VCARRY)<<0x2))

#define C2_AEND         ((0x2)|((C_AEND)<<0x2))
#define C2_SINT         ((0x2)|((C_SINT)<<0x2))
#define C2_SN        ((0x2)|((C_SN)<<0x2))
#define C2_SZ        ((0x2)|((C_SZ)<<0x2))
#define C2_BZ        ((0x2)|((C_BZ)<<0x2))
#define C2_0         ((0x2)|((C_0)<<0x2))
#define C2_1         ((0x2)|((C_1)<<0x2))
#define  C2_AN       ((0x2)|((C_AN)<<0x2))
#define  C2_AZ       ((0x2)|((C_AZ)<<0x2))
#define  C2_SFCOUT      ((0x2)|((C_SFCOUT)<<0x2))
#define  C2_SFMSB    ((0x2)|((C_SFMSB)<<0x2))
#define  C2_VN       ((0x2)|((C_VN)<<0x2))
#define  C2_VZ       ((0x2)|((C_VZ)<<0x2))
#define  C2_VTRUE    ((0x2)|((C_VTRUE)<<0x2))
#define  C2_VCARRY      ((0x2)|((C_VCARRY)<<0x2))

#define C3_AEND         ((0x3)|((C_AEND)<<0x2))
#define C3_SINT         ((0x3)|((C_SINT)<<0x2))
#define C3_SN        ((0x3)|((C_SN)<<0x2))
#define C3_SZ        ((0x3)|((C_SZ)<<0x2))
#define C3_BZ        ((0x3)|((C_BZ)<<0x2))
#define C3_0         ((0x3)|((C_0)<<0x2))
#define C3_1         ((0x3)|((C_1)<<0x2))
#define  C3_AN       ((0x3)|((C_AN)<<0x2))
#define  C3_AZ       ((0x3)|((C_AZ)<<0x2))
#define  C3_SFCOUT      ((0x3)|((C_SFCOUT)<<0x2))
#define  C3_SFMSB    ((0x3)|((C_SFMSB)<<0x2))
#define  C3_VN       ((0x3)|((C_VN)<<0x2))
#define  C3_VZ       ((0x3)|((C_VZ)<<0x2))
#define  C3_VTRUE    ((0x3)|((C_VTRUE)<<0x2))
#define  C3_VCARRY      ((0x3)|((C_VCARRY)<<0x2))

#define OLD(R) ((R)|0x100)

#define OLD_C0_AEND (C0_AEND|0x100)
#define OLD_C0_SINT (C0_SINT|0x100)
#define OLD_C0_SN (C0_SN|0x100)
#define OLD_C0_SZ (C0_SZ|0x100)
#define OLD_C0_BZ (C0_BZ|0x100)
#define OLD_C0_AN (C0_AN|0x100)
#define OLD_C0_AZ (C0_AZ|0x100)
#define OLD_C0_SFCOUT (C0_SFCOUT|0x100)
#define OLD_C0_SFMSB (C0_SFMSB|0x100)
#define OLD_C0_VN (C0_VN|0x100)
#define OLD_C0_VZ (C0_VZ|0x100)
#define OLD_C0_VTRUE (C0_VTRUE|0x100)
#define OLD_C0_VCARRY (C0_VCARRY|0x100)

#define OLD_C1_AEND (C1_AEND|0x100)
#define OLD_C1_SINT (C1_SINT|0x100)
#define OLD_C1_SN (C1_SN|0x100)
#define OLD_C1_SZ (C1_SZ|0x100)
#define OLD_C1_BZ (C1_BZ|0x100)
#define OLD_C1_AN (C1_AN|0x100)
#define OLD_C1_AZ (C1_AZ|0x100)
#define OLD_C1_SFCOUT (C1_SFCOUT|0x100)
#define OLD_C1_SFMSB (C1_SFMSB|0x100)
#define OLD_C1_VN (C1_VN|0x100)
#define OLD_C1_VZ (C1_VZ|0x100)
#define OLD_C1_VTRUE (C1_VTRUE|0x100)
#define OLD_C1_VCARRY (C1_VCARRY|0x100)

#define OLD_C2_AEND (C2_AEND|0x100)
#define OLD_C2_SINT (C2_SINT|0x100)
#define OLD_C2_SN (C2_SN|0x100)
#define OLD_C2_SZ (C2_SZ|0x100)
#define OLD_C2_BZ (C2_BZ|0x100)
#define OLD_C2_AN (C2_AN|0x100)
#define OLD_C2_AZ (C2_AZ|0x100)
#define OLD_C2_SFCOUT (C2_SFCOUT|0x100)
#define OLD_C2_SFMSB (C2_SFMSB|0x100)
#define OLD_C2_VN (C2_VN|0x100)
#define OLD_C2_VZ (C2_VZ|0x100)
#define OLD_C2_VTRUE (C2_VTRUE|0x100)
#define OLD_C2_VCARRY (C2_VCARRY|0x100)

#define OLD_C3_AEND (C3_AEND|0x100)
#define OLD_C3_SINT (C3_SINT|0x100)
#define OLD_C3_SN (C3_SN|0x100)
#define OLD_C3_SZ (C3_SZ|0x100)
#define OLD_C3_BZ (C3_BZ|0x100)
#define OLD_C3_AN (C3_AN|0x100)
#define OLD_C3_AZ (C3_AZ|0x100)
#define OLD_C3_SFCOUT (C3_SFCOUT|0x100)
#define OLD_C3_SFMSB (C3_SFMSB|0x100)
#define OLD_C3_VN (C3_VN|0x100)
#define OLD_C3_VZ (C3_VZ|0x100)
#define OLD_C3_VTRUE (C3_VTRUE|0x100)
#define OLD_C3_VCARRY (C3_VCARRY|0x100)

#define C_NULL                  0x7

// vector condition code register fields
#define  C_VN        0x0   // vector unit output negative
#define  C_VTRUE        0x0   // vector unit output true (overloaded to true for v8Clamp instr only)
#define  C_VCARRY    0x0   // vector unit carry out
#define  C_VZ        0x1   // vector unit output zero
#define C_VN_BITS(reg)     (vcr(reg)&0xFFFF)
#define C_VZ_BITS(reg)     ((vcr(reg)>>16)&0xFFFF)
#define C_VBITS(cc,reg)    (((cc)==C_VN)?C_VN_BITS(reg): \
               (((cc)==C_VZ)?C_VZ_BITS(reg): \
                (0)))

// s8Flt* and v8Flt* immediate values
#define S8FLT_VCMOD(x)  (((x)&7)<<S8FLT_VCMOD_SH)
#define S8FLT_VCMOD_SH  3
#define S8FLT_VCCR(v)   ((v) & 0x7)
#define S8FLT_VCCR_HI   2
#define S8FLT_VCCR_LO   0

#define VCMOD_NONE   0
#define VCMOD_P3  1
#define VCMOD_P3C 2
#define VCMOD_YUYV   3
#define VCMOD_YUY2   VCMOD_YUYV
#define VCMOD_UYVY   4
#define VCMOD_LW12   5
#define VCMOD_AYUV   6
#define VCMOD_YV12   7

// instruction register
#define I_MASK(sz)      ((1<<(sz))-1)

#define  I_OP_HI        31 // opcode high bit
#define  I_OP_LO        24 // opcode low bit
#define I_OP_SZ         (I_OP_HI-I_OP_LO+1)
#define  I_OP        I_OP_HI,I_OP_LO

#define  I_D_HI         23 // destination high bit
#define  I_D_LO         19 // destination low bit
#define  I_D_SZ         (I_D_HI-I_D_LO+1)
#define  I_D         I_D_HI,I_D_LO // destination

#define  I_A_HI         18 // source A high bit
#define  I_A_LO         14 // source A low bit
#define  I_A_SZ         (I_A_HI-I_A_LO+1)
#define  I_A         I_A_HI,I_A_LO  // source A

#define  I_B_HI         13 // source B high bit
#define  I_B_LO         9  // source B low bit
#define  I_B_SZ         (I_B_HI-I_B_LO+1)
#define  I_B         I_B_HI,I_B_LO  // source B

#define  I_C_HI         8  // source C high bit
#define  I_C_LO         4  // source C low bit
#define  I_C_SZ         (I_C_HI-I_C_LO+1)
#define  I_C         I_C_HI,I_C_LO  // source C

#define  I_CCR_HI    8  // condition code read high bit
#define  I_CCR_LO    3  // condition code read low bit
#define  I_CCR_SZ    (I_CCR_HI-I_CCR_LO+1)
#define  I_CCR       I_CCR_HI,I_CCR_LO // condition code read

// within the CCR register field:
#define I_CCR_REG_POS      I_CCR_LO+1, I_CCR_LO // condition code register position
#define  I_CCR_REG      1,0   // condition code register
#define  I_CCR_CC    5,2   // condition code

#define  I_CCW_HI    2  // condition code write high bit
#define  I_CCW_LO    0  // condition code write low bit
#define  I_CCW_SZ    (I_CCW_HI-I_CCW_LO+1)
#define  I_CCW       I_CCW_HI,I_CCW_LO // condition code write

// within the CCW register field:
#define  I_CCW_REG      1,0   // condition code write register
#define  I_CCW_CC    2  // condition code write enable

#define  I_VCR       2,0   // vector condition code read
#define  I_VCR_REG      2,1   // vector condition code register
#define  I_VCR_CC    0,0   // vector condition code
#define  I_READ_LINES      8,5   // # of lines to read/write (dram read/write instructions only)
#define  I_MOV_UNIT     3,0   // unit specifier (3:0 of imm)
#define  I_IMM_EXP      7,5   // exponent
#define  I_IMM_BIT      1  // scalar broadcast is in bit format
#define  I_IMM_RND      I_MTYPE_RND // round nearest

// defines for v8Flt3
#define I_IMM_DSIGN     12
#define I_IMM_ACC_WE    11
#define I_IMM_FLIP      10
#define I_IMM_SEXT      9

#define I_IMM4_HI    8  // 4-bit immediate field high bit
#define I_IMM4_LO    5  // 4-bit immediate field low bit
#define I_IMM4_SZ    (I_IMM4_HI-I_IMM4_LO+1)
#define I_IMM4       I_IMM4_HI,I_IMM4_LO // 5-bit immediate field

#define I_IMM5_HI    13 // 5-bit immediate field high bit
#define I_IMM5_LO    9  // 5-bit immediate field low bit
#define I_IMM5_SZ    (I_IMM5_HI-I_IMM5_LO+1)
#define I_IMM5       I_IMM5_HI,I_IMM5_LO // 5-bit immediate field

#define I_IMM5_32_HI    23 // alt 5-bit immediate field high bit
#define I_IMM5_32_LO    19 // alt 5-bit immediate field low bit
#define I_IMM5_32_SZ    (I_IMM5_32_HI-I_IMM5_32_LO+1)
#define I_IMM5_32    I_IMM5_32_HI,I_IMM5_32_LO // alt 5-bit immediate field

#define I_IMM6_HI    8  // 5-bit immediate field high bit
#define I_IMM6_LO    3  // 5-bit immediate field low bit
#define I_IMM6_SZ    (I_IMM6_HI-I_IMM6_LO+1)
#define I_IMM6       I_IMM6_HI,I_IMM6_LO // 5-bit immediate field

#define I_IMM9_HI    8  // 9-bit immediate field high bit
#define I_IMM9_LO    0  // 9-bit immediate field low bit
#define I_IMM9_SZ    (I_IMM9_HI-I_IMM9_LO+1)
#define I_IMM9       I_IMM9_HI,I_IMM9_LO // 9-bit immediate field

#define I_IMM11_HI      13 // 11-bit immediate field high bit
#define I_IMM11_LO      3  // 11-bit immediate field low bit
#define I_IMM11_SZ      (I_IMM11_HI-I_IMM11_LO+1)
#define I_IMM11         I_IMM11_HI,I_IMM11_LO // 11-bit immediate field

#define I_IMM14_HI      13 // 14-bit immediate field high bit
#define I_IMM14_LO      0  // 14-bit immediate field low bit
#define I_IMM14_SZ      (I_IMM14_HI-I_IMM14_LO+1)
#define I_IMM14         I_IMM14_HI,I_IMM14_LO // 14-bit immediate field

#define I_IMM15_HI      23 // 15-bit immediate field high bit
#define I_IMM15_LO      9  // 15-bit immediate field low bit
#define I_IMM15_SZ      (I_IMM15_HI-I_IMM15_LO+1)
#define I_IMM15         I_IMM15_HI,I_IMM15_LO // 15-bit immediate field

#define I_IMM19_HI      18 // 19-bit immediate field high bit
#define I_IMM19_LO      0  // 19-bit immediate field low bit
#define I_IMM19_SZ      (I_IMM19_HI-I_IMM19_LO+1)
#define I_IMM19         I_IMM19_HI,I_IMM19_LO // 19-bit immediate field

#define I_IMM40_HI      3  // 1-bit immediate field high bit
#define I_IMM40_LO      3  // 1-bit immediate field low bit
#define I_IMM40_SZ      (I_IMM40_HI-I_IMM40_LO+1)
#define I_IMM40         I_IMM40_HI,I_IMM40_LO // 1-bit immediate field

#define I_DSTRIDE_HI    8  // data store stride field high bit
#define I_DSTRIDE_LO    8  // data store stride field low bit
#define I_DSTRIDE_SZ    (I_DSTRIDE_HI-I_DSTRIDE_LO+1)
#define I_DSTRIDE    I_DSTRIDE_HI,I_DSTRIDE_LO // data store stride field

#define I_LINES_HI      7  // lines field high bit
#define I_LINES_LO      3  // lines field low bit
#define I_LINES_SZ      (I_LINES_HI-I_LINES_LO+1)
#define I_LINES         I_LINES_HI,I_LINES_LO // lines field

#define I_BURST_HI      2  // burst size (in 32B) field high bit
#define I_BURST_LO      0  // burst size (in 32B) field low bit
#define I_BURST_SZ      (I_BURST_HI-I_BURST_LO+1)
#define I_BURST         I_BURST_HI,I_BURST_LO // burst size (in 32B) field

#define I_MTYPE_RND  8
#define I_MTYPE_EXP_HI  7
#define I_MTYPE_EXP_LO  5
#define I_MTYPE_LO   4
#define I_MTYPE_DP   3
#define I_MTYPE_AS   2
#define I_MTYPE_BS   1
#define I_MTYPE_BIT  0
// for v8Flt3
#define I_FTYPE_DSIGN     3
#define I_FTYPE_ACC_WE    2
#define I_FTYPE_FLIP      1
#define I_FTYPE_SEXT      0

#define I_MITYPE_IMM0_LO   9
#define I_MITYPE_IMM0_HI   13
#define I_MITYPE_IMM0_SH   2
#define I_MITYPE_IMM1_LO   0
#define I_MITYPE_IMM1_HI   0
#define I_MITYPE_IMM1_SH   7
#define I_MITYPE_IMM(i) ((ExBits((i),I_MITYPE_IMM1_HI, \
            I_MITYPE_IMM1_LO)<<I_MITYPE_IMM1_SH)| \
            ExBits((i),I_MITYPE_IMM0_HI, \
            I_MITYPE_IMM0_LO)<<I_MITYPE_IMM0_SH)

#define ACC_ONLY  0x100
#define ACC    ACC_ONLY

#define V_LO      (1<<I_MTYPE_LO)
#define V_HI      (0<<I_MTYPE_LO)
#define V_RND     (1<<I_MTYPE_RND)
#define V_TRUNC      (0<<I_MTYPE_RND)
#define V_EXP(x)  (((x) & 0x7)<<I_MTYPE_EXP_LO)
#define V_DP      (1<<I_MTYPE_DP)
#define V_SP      (0<<I_MTYPE_DP)
#define V_AS      (1<<I_MTYPE_AS)
#define V_AU      (0<<I_MTYPE_AS)
#define V_BS      (1<<I_MTYPE_BS)
#define V_BU      (0<<I_MTYPE_BS)
#define V_BIT     (1<<I_MTYPE_BIT)
#define V_BYTE    (0<<I_MTYPE_BIT)
// for v8Flt3
#define V_DS      (1<<I_FTYPE_DSIGN)
#define V_DU      (0<<I_FTYPE_DSIGN)
#define V_ACC_WRITE  (1<<I_FTYPE_ACC_WE)
#define V_ACC_NOWRITE   (0<<I_FTYPE_ACC_WE)
#define V_FLIP    (1<<I_FTYPE_FLIP)
#define V_NOFLIP  (0<<I_FTYPE_FLIP)
#define V_SEXT    (1<<I_FTYPE_SEXT)
#define V_NOSEXT  (0<<I_FTYPE_SEXT)

// pack 6-bit number into immediate: imm[13:9] = n[4:0], imm[0] = n[5]
#define V_IMM(n)  ((((n) & 0x1F) << 9) | (((n) & 0x20) >> 5))

// pseudo-flag for preclearing vacc. on v8Mul,etc
#define I_MTYPE_PRECLR  30
#define V_PRECLR  (0<<I_MTYPE_PRECLR)
#define V_NOCLR      (1<<I_MTYPE_PRECLR)

#define I_FLT1_SH_HI     4
#define I_FLT1_SH_LO     2
#define I_FLT1_RND_HI       0
#define I_FLT1_RND_LO       0

#define I_FHV_RND     3
#define I_FHV_EXP_HI     2
#define I_FHV_EXP_LO     0

#define I_ARD_TARG_HI      31
#define I_ARD_TARG_LO      30
#define I_ARD_LINES_HI     28
#define I_ARD_LINES_LO     24
#define I_ARD_BURST_HI     23
#define I_ARD_BURST_LO     16
#define I_ARD_STRIDE_HI    15
#define I_ARD_STRIDE_LO     0

#define I_ARDI_LINES_HI    12
#define I_ARDI_LINES_LO     8
#define I_ARDI_BURST_HI     7
#define I_ARDI_BURST_LO     0

#define I_CCR_CC_HI      5
#define I_CCR_CC_LO      2
#define I_CCR_REG_HI     1
#define I_CCR_REG_LO     0

#define I_VCCR_CC_HI     2
#define I_VCCR_CC_LO     2
#define I_VCCR_REG_HI       1
#define I_VCCR_REG_LO       0

// Took out some stuff

#define NOREG -1

#define C0  0
#define C1  1
#define C2  2
#define C3  3
#define C4  4
#define C5  5
#define C6  6
#define C7  7

#define R0  0
#define R1  1
#define R2  2
#define R3  3
#define R4  4
#define R5  5
#define R6  6
#define R7  7
#define R8  8
#define R9  9
#define R10 10
#define R11 11
#define R12 12
#define R13 13
#define R14 14
#define R15 15
#define R16 16
#define R17 17
#define R18 18
#define R19 19
#define R20 20
#define R21 21
#define R22 22
#define R23 23
#define R24 24
#define R25 25
#define R26 26
#define R27 27
#define R28 28
#define R29 29
#define R30 30
#define R31 31

#define OLD_REG      0x100

#define OLD_R0    (0|OLD_REG)
#define OLD_R1    (1|OLD_REG)
#define OLD_R2    (2|OLD_REG)
#define OLD_R3    (3|OLD_REG)
#define OLD_R4    (4|OLD_REG)
#define OLD_R5    (5|OLD_REG)
#define OLD_R6    (6|OLD_REG)
#define OLD_R7    (7|OLD_REG)
#define OLD_R8    (8|OLD_REG)
#define OLD_R9    (9|OLD_REG)
#define OLD_R10      (10|OLD_REG)
#define OLD_R11      (11|OLD_REG)
#define OLD_R12      (12|OLD_REG)
#define OLD_R13      (13|OLD_REG)
#define OLD_R14      (14|OLD_REG)
#define OLD_R15      (15|OLD_REG)
#define OLD_R16      (16|OLD_REG)
#define OLD_R17      (17|OLD_REG)
#define OLD_R18      (18|OLD_REG)
#define OLD_R19      (19|OLD_REG)
#define OLD_R20      (20|OLD_REG)
#define OLD_R21      (21|OLD_REG)
#define OLD_R22      (22|OLD_REG)
#define OLD_R23      (23|OLD_REG)
#define OLD_R24      (24|OLD_REG)
#define OLD_R25      (25|OLD_REG)
#define OLD_R26      (26|OLD_REG)
#define OLD_R27      (27|OLD_REG)
#define OLD_R28      (28|OLD_REG)
#define OLD_R29      (29|OLD_REG)
#define OLD_R30      (30|OLD_REG)
#define OLD_R31      (31|OLD_REG)

#endif // INCLUDED_JSVP_H
