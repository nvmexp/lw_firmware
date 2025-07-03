/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  instrdef.h
 * @brief From //hw/gpu/cmod/mpeg_vp/vp_instr_defs.h
 *
 *
 */

#ifndef VP_INSTR_DEFS_H
#define VP_INSTR_DEFS_H

// put defines that replace instructions here

#define s32Not(d,a,ccw)         (s32XorI((d),(a),-1,(ccw)))
#define s32Neg(d,a,ccw)         (s32Sub((d),R31,(a),C0_0,(ccw)))
#define s32SubI(d,a,imm,ccw)    (s32AddI((d),(a),-(imm),(ccw)))
#define s32Or(d,a,b,ccw)        (s32LOp((d),(a),(b),LOP_OR,(ccw)))
#define s32And(d,a,b,ccw)       (s32LOp((d),(a),(b),LOP_AND,(ccw)))
#define s32Xor(d,a,b,ccw)       (s32LOp((d),(a),(b),LOP_XOR,(ccw)))
#define s32Nor(d,a,b,ccw)       (s32LOp((d),(a),(b),LOP_NOR,(ccw)))
#define s32Nand(d,a,b,ccw)      (s32LOp((d),(a),(b),LOP_NAND,(ccw)))
#define s32Xnor(d,a,b,ccw)      (s32LOp((d),(a),(b),LOP_XNOR,(ccw)))

#define s8SubI(d,a,imm)         (s8AddI((d),(a),-(imm)))
#define s8Not(d,a)              (s8XorI((d),(a),0xFF))
#define s8Or(d,a,b)             (s32LOp((d),(a),(b),LOP_OR,C_NULL))
#define s8And(d,a,b)            (s32LOp((d),(a),(b),LOP_AND,C_NULL))
#define s8Xor(d,a,b)            (s32LOp((d),(a),(b),LOP_XOR,C_NULL))
#define s8Nor(d,a,b)            (s32LOp((d),(a),(b),LOP_NOR,C_NULL))
#define s8Nand(d,a,b)           (s32LOp((d),(a),(b),LOP_NAND,C_NULL))
#define s8Xnor(d,a,b)           (s32LOp((d),(a),(b),LOP_XNOR,C_NULL))

#define v8XBar(d,a,b)           (v8XBar2((d),(b),(b),(a),0))
#define v8Not(d,a,ccw)          (v8XorI((d),(a),0xFF,(ccw)))
#define v8SubI(d,a,imm,ccw)     (v8AddI((d),(a),-(imm),(ccw)))
#define v8Flt(d,a,b,imm)        (v8Fltul((d),(a),(b),(imm)))
#define v8FltU(d,a,b,imm)       (v8FltulU((d),(a),(b),(imm)))
#define v8Flt3(d,a,immf3,imm,ccr,vccr) \
                (v8Flt3U((d),(a),(immf3)|V_DS,(imm),(ccr),(vccr)))
#define v8Or(d,a,b,ccw)         (v8LOp((d),(a),(b),LOP_OR,(ccw)))
#define v8And(d,a,b,ccw)        (v8LOp((d),(a),(b),LOP_AND,(ccw)))
#define v8Xor(d,a,b,ccw)        (v8LOp((d),(a),(b),LOP_XOR,(ccw)))
#define v8Nor(d,a,b,ccw)        (v8LOp((d),(a),(b),LOP_NOR,(ccw)))
#define v8Nand(d,a,b,ccw)       (v8LOp((d),(a),(b),LOP_NAND,(ccw)))
#define v8Xnor(d,a,b,ccw)       (v8LOp((d),(a),(b),LOP_XNOR,(ccw)))

#define aRd(d,a)                (aRdI((d),(a),0x2000))
#define aWr(d,a)                (aWrI((d),(a),0x2000))
#define aStVLUT(d,a,b,ccr)      (aLUT((d),(a),(b),(ccr),1))
#define aLdVLUT(d,a,b)          (aLUT((d),(a),(b),C0_0,0))
#define aOr(d,a,b,ccw)          (aLOp((d),(a),(b),LOP_OR,(ccw)))
#define aAnd(d,a,b,ccw)         (aLOp((d),(a),(b),LOP_AND,(ccw)))
#define aXor(d,a,b,ccw)         (aLOp((d),(a),(b),LOP_XOR,(ccw)))
#define aNor(d,a,b,ccw)         (aLOp((d),(a),(b),LOP_NOR,(ccw)))
#define aNand(d,a,b,ccw)        (aLOp((d),(a),(b),LOP_NAND,(ccw)))
#define aXnor(d,a,b,ccw)        (aLOp((d),(a),(b),LOP_XNOR,(ccw)))

#define aCsimInitTiles(d,count) (aNop((d),0,count,7))
#define aCsimInitTile(d) (aCsimInitTiles((d),1))

#define bError(imm16)           (bHalt((imm16) | (0x10000)))

// aLOp, s32LOp, and v8LOp common operations
#define LOP_AND  0x8
#define LOP_OR   0xE
#define LOP_XOR  0x6
#define LOP_NOR  0x1
#define LOP_XNOR 0x9
#define LOP_NAND 0x7
#define LOP_0    0x0
#define LOP_1    0xF
#define LOP_MOVE 0xC

#endif /* VP_INSTR_DEFS_H */

