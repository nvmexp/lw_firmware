/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//  GpuPgmGen
//      Generic program generator to support the creation of vertex, geometry
//      and fragment programs. This class strickly follows the Bakus-Nuar form
//      to specify the grammar.
//
// 45678901234567890123456789012345678901234567890123456789012345678901234567890

#include "glrandom.h"  // declaration of our namespace
#include "pgmgen.h"
#include "core/include/massert.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/utility.h"
#include "core/include/coverage.h"
#include "opengl/mglcoverage.h"
#include "core/include/version.h"
#include <math.h>
#include <algorithm>   // for the "find" template function

#define MIN_OPS_FOR_MS  2   // min number of ops to use the TXFMS instr consistently.

using namespace GLRandom;
using namespace TestCoverage;
// Note that we ignore the "H" column from the extension table, i.e. we never
// the legacy "H" suffix (half-precision-float).
// We will use the ".F16" suffix with CVT though.

// This list must be ordered sequentially using RND_GPU_PROG_OPCODE_??? values
const GpuPgmGen::OpData GpuPgmGen::s_GpuOpData4[ RND_GPU_PROG_OPCODE_NUM_OPCODES ] =
{                                                                                                                             //$  Modifiers
                                                                                                                              //$ F I C S H D  Out Inputs    Description
//     Name      OpInstType   ExtVer  nIn inType1    inType2   inType3    outType    outMask opMod Types   defType IsTx   mul add  //$ - - - - - -  --- --------  --------------------------------
     { "ABS",    oitVector,   vp3fp2, 1,  {atVector, atNone,   atNone},   atVector,  omXYZW, smCS, dtFI6,  dtF32,  false,  10,  8} //$ 6 6 X X X F  v   v         absolute value
    ,{ "ADD",    oitBin,      vp3fp2, 2,  {atVector, atVector, atNone},   atVector,  omXYZW, smCS, dtFI6f, dtF32,  false,   7,  8} //$ 6 6 X X X F  v   v,v       add
    ,{ "AND",    oitBin,      gpu4,   2,  {atVector, atVector, atNone},   atVector,  omXYZW, smC,  dtI6,   dtS32,  false,   7,  8} //$ - 6 X - - S  v   v,v       bitwise and
    ,{ "BRK",    oitFlowCC,   gpu4,   1,  {atCC,     atNone,   atNone},   atNone,    omNone, smNA, dtNA,   dtNA,   false,   5,  8} //$ - - - - - -  -   c         break out of loop instruction
    ,{ "CAL",    oitBra,      gpu4,   1,  {atCC,     atNone,   atNone},   atNone,    omNone, smNA, dtNA,   dtNA,   false,  10,  8} //$ - - - - - -  -   c         subroutine call
    ,{ "CEIL",   oitVector,   gpu4,   1,  {atVectF,  atNone,   atNone},   atVector,  omXYZW, smCS, dtFI6,  dtF32,  false,  10,  4} //$ 6 6 X X X F  v   vf        ceiling
    ,{ "CMP",    oitTri,      vnafp2, 3,  {atVector, atVector, atVector}, atVector,  omXYZW, smCS, dtFI6,  dtF32,  false,  10,  4} //$ 6 6 X X X F  v   v,v,v     compare
    ,{ "CONT",   oitFlowCC,   gpu4,   1,  {atCC,     atNone,   atNone},   atNone,    omNone, smNA, dtNA,   dtNA,   false,   5,  8} //$ - - - - - -  -   c         continue with next loop interation
    ,{ "COS",    oitScalar,   vp3fp2, 1,  {atScalar, atNone,   atNone},   atScalar,  omXYZW, smCS, dtFx,   dtF32,  false,  10,  8} //$ X - X X X F  s   s         cosine with reduction to [-PI,PI]
    ,{ "DIV",    oitVecSca,   vnafp2, 2,  {atVector, atScalar, atNone},   atVector,  omXYZW, smCS, dtFI6f, dtF32,  false,   5,  8} //$ 6 6 X X X F  v   v,s       divide vector components by scalar
    ,{ "DP2",    oitBin,      vnafp2, 2,  {atVector, atVector, atNone},   atScalar,  omXYZW, smCS, dtFx,   dtF32,  false,   6,  8} //$ X - X X X F  s   v,v       2-component dot product
    ,{ "DP2A",   oitTri,      vnafp2, 3,  {atVector, atVector, atVector}, atScalar,  omXYZW, smCS, dtFx,   dtF32,  false,   6,  8} //$ X - X X X F  s   v,v,v     2-comp. dot product w/scalar add
    ,{ "DP3",    oitBin,      vp3fp2, 2,  {atVector, atVector, atNone},   atScalar,  omXYZW, smCS, dtFx,   dtF32,  false,   6,  8} //$ X - X X X F  s   v,v       3-component dot product
    ,{ "DP4",    oitBin,      vp3fp2, 2,  {atVector, atVector, atNone},   atScalar,  omXYZW, smCS, dtFx,   dtF32,  false,   6,  8} //$ X - X X X F  s   v,v       4-component dot product
    ,{ "DPH",    oitBin,      vp3fp2, 2,  {atVector, atVector, atNone},   atScalar,  omXYZW, smCS, dtFx,   dtF32,  false,   6,  8} //$ X - X X X F  s   v,v       homogeneous dot product
    ,{ "DST",    oitBin,      vp3fp2, 2,  {atVector, atVector, atNone},   atVector,  omXYZW, smCS, dtFx,   dtF32,  false,   6,  8} //$ X - X X X F  v   v,v       distance vector
    ,{ "ELSE",   oitEndFlow,  gpu4,   0,  {atNone,   atNone,   atNone},   atNone,    omNone, smNA, dtNA,   dtNA,   false,   6,  8} //$ - - - - - -  -   -         start if test else block
    ,{ "ENDIF",  oitEndFlow,  gpu4,   0,  {atNone,   atNone,   atNone},   atNone,    omNone, smNA, dtNA,   dtNA,   false,   5,  8} //$ - - - - - -  -   -         end if test block
    ,{ "ENDREP", oitEndFlow,  gpu4,   0,  {atNone,   atNone,   atNone},   atNone,    omNone, smNA, dtNA,   dtNA,   false,   5,  8} //$ - - - - - -  -   -         end of repeat block
    ,{ "EX2",    oitScalar,   vp3fp2, 1,  {atScalar, atNone,   atNone},   atScalar,  omXYZW, smCS, dtFx,   dtF32,  false,  10,  4} //$ X - X X X F  s   s         exponential base 2
    ,{ "FLR",    oitVector,   vp3fp2, 1,  {atVector, atNone,   atNone},   atVector,  omXYZW, smCS, dtFI6,  dtF32,  false,  10,  4} //$ 6 6 X X X F  v   vf        floor
    ,{ "FRC",    oitVector,   vp3fp2, 1,  {atVector, atNone,   atNone},   atVector,  omXYZW, smCS, dtF6f,  dtF32,  false,  10, 10} //$ 6 - X X X F  v   v         fraction
    ,{ "I2F",    oitVector,   gpu4,   1,  {atVectS,  atNone,   atNone},   atVectF,   omXYZW, smC,  dtI6,   dtS32,  false,   5,  8} //$ - 6 X - - S  vf  v         integer to float
    ,{ "IF",     oitIf,       gpu4,   1,  {atCC,     atNone,   atNone},   atNone,    omNone, smNA, dtNA,   dtNA,   false,   5,  8} //$ - - - - - -  -   c         start of if test block
    ,{ "LG2",    oitScalar,   vp3fp2, 1,  {atScalar, atNone,   atNone},   atScalar,  omXYZW, smCS, dtFx,   dtF32,  false,  10,  4} //$ X - X X X F  s   s         logarithm base 2
    ,{ "LIT",    oitVector,   vp3fp2, 1,  {atVector, atNone,   atNone},   atVector,  omXYZW, smCS, dtFx,   dtF32,  false,  10,  4} //$ X - X X X F  v   v         compute lighting coefficients
    ,{ "LRP",    oitTri,      vnafp2, 3,  {atVector, atVector, atVector}, atVector,  omXYZW, smCS, dtFx,   dtF32,  false,   5,  8} //$ X - X X X F  v   v,v,v     linear interpolation
    ,{ "MAD",    oitTri,      vp3fp2, 3,  {atVector, atVector, atVector}, atVector,  omXYZW, smCS, dtFI6,  dtF32,  false,   5,  4} //$ 6 6 X X X F  v   v,v,v     multiply and add
    ,{ "MAX",    oitBin,      vp3fp2, 2,  {atVector, atVector, atNone},   atVector,  omXYZW, smCS, dtFI6f, dtF32,  false,   6,  4} //$ 6 6 X X X F  v   v,v       maximum
    ,{ "MIN",    oitBin,      vp3fp2, 2,  {atVector, atVector, atNone},   atVector,  omXYZW, smCS, dtFI6f, dtF32,  false,   6,  4} //$ 6 6 X X X F  v   v,v       minimum
    // WAR 697262 -- disallow MOD.U64 and MOD.S64
    //,{ "MOD",    oitVecSca, gpu4,    2,  {atVector, atScalar, atNone}, atVector,  omXYZW, smC,  dtI6,      dtS3 2, false,   5,  8} //$ - 6 X - - S  v   v,s       modulus vector components by scalar
    ,{ "MOD",    oitVecSca,   gpu4,   2,  {atVector, atScalar, atNone},   atVector,  omXYZW, smC,  dtI,      dtS32,  false,   5,  8} //$ - 6 X - - S  v   v,s       modulus vector components by scalar
    ,{ "MOV",    oitVector,   vp3fp2, 1,  {atVector, atNone,   atNone},   atVector,  omXYZW, smCS, dtFI6,    dtF32,  false,  11,  4} //$ 6 6 X X X F  v   v         move
    ,{ "MUL",    oitBin,      vp3fp2, 2,  {atVector, atVector, atNone},   atVector,  omXYZW, smCS, dtMULf,   dtF32,  false,   6,  8} //$ 6 6 X X X F  v   v,v       multiply
    ,{ "NOT",    oitVector,   gpu4,   1,  {atVector, atNone,   atNone},   atVector,  omXYZW, smC,  dtI6,     dtS32,  false,   7,  8} //$ - 6 X - - S  v   v         bitwise not
    ,{ "NRM",    oitVector,   vnafp2, 1,  {atVector, atNone,   atNone},   atVector,  omXYZ,  smCS, dtFx,     dtF32,  false,   5,  8} //$ X - X X X F  v   v         normalize 3-component vector
    ,{ "OR",     oitBin,      gpu4,   2,  {atVector, atVector, atNone},   atVector,  omXYZW, smC,  dtI6,     dtS32,  false,   7,  8} //$ - 6 X - - S  v   v,v       bitwise or
    ,{ "PK2H",   oitVector,   gpu4,   1,  {atVectF,  atNone,   atNone},   atScalar,  omXYZW, smNA, dtFIx,    dtF32,  false,   7,  8} //$ X X - - - F  s   vf        pack two 16-bit floats
    ,{ "PK2US",  oitVector,   gpu4,   1,  {atVectF,  atNone,   atNone},   atScalar,  omXYZW, smNA, dtFIx,    dtF32,  false,   7,  8} //$ X X - - - F  s   vf        pack two floats as unsigned 16-bit
    ,{ "PK4B",   oitVector,   gpu4,   1,  {atVectF,  atNone,   atNone},   atScalar,  omXYZW, smNA, dtFIx,    dtF32,  false,   7,  8} //$ X X - - - F  s   vf        pack four floats as signed 8-bit
    ,{ "PK4UB",  oitVector,   gpu4,   1,  {atVectF,  atNone,   atNone},   atScalar,  omXYZW, smNA, dtFIx,    dtF32,  false,   7,  8} //$ X X - - - F  s   vf        pack four floats as unsigned 8-bit
    ,{ "POW",    oitBinSC,    vp3fp2, 2,  {atScalar, atScalar, atNone},   atScalar,  omXYZW, smCS, dtFx,     dtF32,  false,  11,  8} //$ X - X X X F  s   s,s       exponentiate
    ,{ "RCC",    oitScalar,   gpu4,   1,  {atScalar, atNone,   atNone},   atScalar,  omXYZW, smCS, dtFx,     dtF32,  false,  10,  8} //$ X - X X X F  s   s         reciprocal (clamped)
    ,{ "RCP",    oitScalar,   vp3fp2, 1,  {atScalar, atNone,   atNone},   atScalar,  omXYZW, smCS, dtF6,     dtF32,  false,  10,  8} //$ 6 - X X X F  s   s         reciprocal
    ,{ "REP",    oitRep,      gpu4,   1,  {atVector, atNone,   atNone},   atNone,    omNone, smNA, dtFI6,    dtNA,   false,  10,  0} //$ 6 6 - - X F  -   v         start of repeat block
    ,{ "RET",    oitFlowCC,   gpu4,   1,  {atCC,     atNone,   atNone},   atNone,    omNone, smNA, dtNA,     dtNA,   false,  10,  0} //$ - - - - - -  -   c         subroutine return
    ,{ "RFL",    oitBin,      vnafp2, 2,  {atVector, atVector, atNone},   atVector,  omXYZ,  smCS, dtFx,     dtF32,  false,   5,  8} //$ X - X X X F  v   v,v       reflection vector
    ,{ "ROUND",  oitVector,   gpu4,   1,  {atVectF,  atNone,   atNone},   atVector,  omXYZW, smCS, dtFI6,    dtF32,  false,   5,  8} //$ 6 6 X X X F  v   vf        round to nearest integer
    ,{ "RSQ",    oitSpecial,  vp3fp2, 1,  {atScalar, atNone,   atNone},   atScalar,  omXYZW, smCS, dtF6,     dtF32,  false,  10,  8} //$ 6 - X X X F  s   s         reciprocal square root
    ,{ "SAD",    oitTri,      gpu4,   3,  {atVector, atVector, atVectU},  atVectU,   omXYZW, smC,  dtI6,     dtS32,  false,   5,  8} //$ - 6 X - - S  vu  v,v,vu    sum of absolute differences
    ,{ "SCS",    oitScalar,   vnafp2, 1,  {atScalar, atNone,   atNone},   atVector,  omXY,   smCS, dtFx,     dtF32,  false,  11,  8} //$ X - X X X F  v   s         sine/cosine without reduction
    ,{ "SEQ",    oitBin,      vp3fp2, 2,  {atVector, atVector, atNone},   atVector,  omXYZW, smCS, dtFI6,    dtF32,  false,   6,  4} //$ 6 6 X X X F  v   v,v       set on equal
    ,{ "SFL",    oitBin,      vp3fp2, 2,  {atVector, atVector, atNone},   atVector,  omXYZW, smCS, dtFI6,    dtF32,  false,   6,  4} //$ 6 6 X X X F  v   v,v       set on false
    ,{ "SGE",    oitBin,      vp3fp2, 2,  {atVector, atVector, atNone},   atVector,  omXYZW, smCS, dtFI6,    dtF32,  false,   6,  4} //$ 6 6 X X X F  v   v,v       set on greater than or equal
    ,{ "SGT",    oitBin,      vp3fp2, 2,  {atVector, atVector, atNone},   atVector,  omXYZW, smCS, dtFI6,    dtF32,  false,   6,  4} //$ 6 6 X X X F  v   v,v       set on greater than
    ,{ "SHL",    oitVecSca,   gpu4,   2,  {atVector, atScalar, atNone},   atVector,  omXYZW, smC,  dtI6,     dtS32,  false,   6,  4} //$ - 6 X - - S  v   v,s       shift left
    ,{ "SHR",    oitVecSca,   gpu4,   2,  {atVector, atScalar, atNone},   atVector,  omXYZW, smC,  dtI6,     dtS32,  false,   6,  4} //$ - 6 X - - S  v   v,s       shift right
    ,{ "SIN",    oitScalar,   vp3fp2, 1,  {atScalar, atNone,   atNone},   atScalar,  omXYZW, smCS, dtFx,     dtF32,  false,  11,  8} //$ X - X X X F  s   s         sine with reduction to [-PI,PI]
    ,{ "SLE",    oitBin,      vp3fp2, 2,  {atVector, atVector, atNone},   atVector,  omXYZW, smCS, dtFI6,    dtF32,  false,   6,  4} //$ 6 6 X X X F  v   v,v       set on less than or equal
    ,{ "SLT",    oitBin,      vp3fp2, 2,  {atVector, atVector, atNone},   atVector,  omXYZW, smCS, dtFI6,    dtF32,  false,   6,  4} //$ 6 6 X X X F  v   v,v       set on less than
    ,{ "SNE",    oitBin,      vp3fp2, 2,  {atVector, atVector, atNone},   atVector,  omXYZW, smCS, dtFI6,    dtF32,  false,   6,  4} //$ 6 6 X X X F  v   v,v       set on not equal
    ,{ "SSG",    oitVector,   gpu4,   1,  {atVector, atNone,   atNone},   atVector,  omXYZW, smCS, dtF6,     dtF32,  false,  11,  4} //$ 6 - X X X F  v   v         set sign
    ,{ "STR",    oitBin,      vp3fp2, 2,  {atVector, atVector, atNone},   atVector,  omXYZW, smCS, dtFI6,    dtF32,  false,   5,  4} //$ 6 6 X X X F  v   v,v       set on true
    ,{ "SUB",    oitBin,      vp3fp2, 2,  {atVector, atVector, atNone},   atVector,  omXYZW, smCS, dtFI6,    dtF32,  false,   6,  8} //$ 6 6 X X X F  v   v,v       subtract
    ,{ "SWZ",    oitSwz,      vp3fp2, 1,  {atVectNS, atNone,   atNone},   atVector,  omXYZW, smCS, dtFx,     dtF32,  false,  11,  8} //$ X - X X X F  v   v         extended swizzle
    ,{ "TEX",    oitTex,      vp4fp2, 1,  {atVectF,  atNone,   atNone},   atVector,  omXYZW, smCS, dtFIxf,   dtF32,  true,   10, 20} //$ X X X X - F  v   vf        texture sample
    ,{ "TRUNC",  oitVector,   gpu4,   1,  {atVectF,  atNone,   atNone},   atVector,  omXYZW, smCS, dtFI6,    dtF32,  false,   5,  8} //$ 6 6 X X X F  v   vf        truncate (round toward zero)
    ,{ "TXB",    oitTex,      vp4fp2, 1,  {atVectF,  atNone,   atNone},   atVector,  omXYZW, smCS, dtFIxf,   dtF32,  true,   11, 20} //$ X X X X - F  v   vf        texture sample with bias
    ,{ "TXD",    oitTexD,     gpu4,   3,  {atVectF,  atVectF,  atVectF},  atVector,  omXYZW, smCS, dtFIxf,   dtF32,  true,   11, 20} //$ X X X X - F  v   vf,vf,vf  texture sample w/partials
    ,{ "TXF",    oitTex,      gpu4,   1,  {atVectS,  atNone,   atNone},   atVector,  omXYZW, smCS, dtFIxf,   dtF32,  true,   11, 20} //$ X X X X - F  v   vs        texel fetch
    ,{ "TXL",    oitTex,      vp4fp2, 1,  {atVectF,  atNone,   atNone},   atVector,  omXYZW, smCS, dtFIxf,   dtF32,  true,   11, 20} //$ X X X X - F  v   vf        texture sample w/LOD
    ,{ "TXP",    oitTex,      vp4fp2, 1,  {atVectF,  atNone,   atNone},   atVector,  omXYZW, smCS, dtFIxf,   dtF32,  true,   12, 20} //$ X X X X - F  v   vf        texture sample w/projection
    ,{ "TXQ",    oitTxq,      gpu4,   1,  {atVectS,  atNone,   atNone},   atVectS,   omX,    smNA, dtNA,     dtNA,   true,   11, 20} //$ - - - - - S  vs  vs        texture info query
    ,{ "UP2H",   oitScalar,   gpu4,   1,  {atScalar, atNone,   atNone},   atVectF,   omXYZW, smCS, dtFIx,    dtF32,  false,   7,  8} //$ X X X X - F  vf  s         unpack two 16-bit floats
    ,{ "UP2US",  oitScalar,   gpu4,   1,  {atScalar, atNone,   atNone},   atVectF,   omXYZW, smCS, dtFIx,    dtF32,  false,   7,  8} //$ X X X X - F  vf  s         unpack two unsigned 16-bit ints
    ,{ "UP4B",   oitScalar,   gpu4,   1,  {atScalar, atNone,   atNone},   atVectF,   omXYZW, smCS, dtFIx,    dtF32,  false,   7,  8} //$ X X X X - F  vf  s         unpack four signed 8-bit ints
    ,{ "UP4UB",  oitScalar,   gpu4,   1,  {atScalar, atNone,   atNone},   atVectF,   omXYZW, smCS, dtFIx,    dtF32,  false,   7,  8} //$ X X X X - F  vf  s         unpack four unsigned 8-bit ints
    ,{ "X2D",    oitTri,      vnafp2, 3,  {atVector, atVector, atVector}, atVector,  omXYZW, smCS, dtFx,     dtF32,  false,   7,  8} //$ X - X X X F  v   v,v,v     2D coordinate transformation
    ,{ "XOR",    oitBin,      gpu4,   2,  {atVector, atVector, atNone},   atVector,  omXYZW, smC,  dtI6,     dtS32,  false,   7,  8} //$ - 6 X - - S  v   v,v       exclusive or
    ,{ "XPD",    oitBin,      vp3fp2, 2,  {atVector, atVector, atNone},   atVector,  omXYZ,  smCS, dtFx,     dtF32,  false,  12,  8} //$ X - X X X F  v   v,v       cross product
    // LW_gpu_program4_1 opcodes
    ,{ "TXG",     oitTex,     gpu4_1, 1,  {atVector, atNone,   atNone},   atVector,  omXYZW, smCS, dtFIxf,   dtF32,  true,   10, 20} //$ X X X X X F  v   v         Texture Gather
    ,{ "TEX",     oitTex2,    gpu4_1, 2,  {atVectF,  atVectF,  atNone},   atVector,  omXYZW, smCS, dtFIxf,   dtF32,  true,   10, 20} //$ X X X X - F  v   vf        texture sample
    ,{ "TXB",     oitTex2,    gpu4_1, 2,  {atVectF,  atVectF,  atNone},   atVector,  omXYZW, smCS, dtFIxf,   dtF32,  true,   11, 20} //$ X X X X - F  v   vf        texture sample with bias
    ,{ "TXL",     oitTex2,    gpu4_1, 2,  {atVectF,  atVectF,  atNone},   atVector,  omXYZW, smCS, dtFIxf,   dtF32,  true,   11, 20} //$ X X X X - F  v   vf        texture sample w/LOD
    // LW_explicit_multisample opcode
    ,{ "TXFMS",   oitTexMs,   expms,  1,  {atVectF,  atNone,   atNone},   atVector,  omXYZW, smC,  dtFIxf,   dtF32,  true,   11, 20} //$ X X X X - F  v   vs        texture fetch, multisample
    // LW_gpu_program5 opcodes
    ,{ "BFE",     oitBitFld, gpu5,    2,  {atVector, atVector, atNone},   atVector, omXYZW, smC,  dtIx,      dtS32,   false,   1,  1} //$ - X X - - I  v   v,v       bitfield extract
    ,{ "BFI",     oitBitFld, gpu5,    3,  {atVector, atVector, atVector}, atVector, omXYZW, smC,  dtIx,      dtS32,   false,   1,  1} //$ - X X - - I  v   v,v,v     bitfield insert
    ,{ "BFR",     oitVector, gpu5,    1,  {atVector, atNone,   atNone},   atVector, omXYZW, smC,  dtIx,      dtS32,   false,   1,  1} //$ - X X - - I  v   v         bitfield reverse
    ,{ "BTC",     oitVector, gpu5,    1,  {atVector, atNone,   atNone},   atVector, omXYZW, smC,  dtIx,      dtS32,   false,   1,  1} //$ - X X - - I  v   v         bitcount
    ,{ "BTFL",    oitVector, gpu5,    1,  {atVector, atNone,   atNone},   atVector, omXYZW, smC,  dtIx,      dtS32,   false,   1,  1} //$ - X X - - I  v   v         find least significant bit
    ,{ "BTFM",    oitVector, gpu5,    1,  {atVector, atNone,   atNone},   atVector, omXYZW, smC,  dtIx,      dtS32,   false,   1,  1} //$ - X X - - I  v   v         find most significant bit
    ,{ "TXGO",    oitTxgo,   gpu5,    3,  {atVectF,  atVectS,  atVectS},  atVector, omXYZW, smCS, dtFIxf,    dtF32,   true,    1,  1} //$ X X X X - F  v   vf,vs,vs  texture gather w/per-texel offsets
    ,{ "CVT",     oitCvt,    gpu5,    1,  {atVector, atNone,   atNone},   atVector, omXYZW, smCS, dtFIe,     dtF32e,  false,   1,  1} //$ - - X X - I  v   v         find most significant bit
    ,{ "PK64",    oitPk64,   gpu5,    1,  {atVector, atNone,   atNone},   atVector, omXYZW, smNA, dtFIx,     dtF64,   false,   1,  1} //$ x x - - - F  v   vf        pack 2 32-bit values to 64-bit
    ,{ "UP64",    oitUp64,   gpu5,    1,  {atVector, atNone,   atNone},   atVector, omXYZW, smCS, dtFIx,     dtF32,   false,   1,  1} //$ 6 6 X X - F  vf  v         unpack 64-bit component to 2x32
    ,{ "LDC",     oitMem,    gpu5,    1,  {atVector, atNone,   atNone},   atVector, omXYZW, smCS, dtFI32eX4, dtF32e,  false,   1,  1} //$ X X X X - F  v   v         load from constant buffer //$
    ,{ "CALI",    oitCali,   gpu5,    1,  {atScalar, atCC,     atNone},   atNone,   omNone, smNA, dtNA,      dtNA,    false,   1,  1} //$ - - - - - -  -   s         call fn-ptr or indexed fn-ptr array
    ,{ "LOADIM",  oitImage,  gpu5,    1,  {atVectNS, atNone,   atNone},   atVector, omXYZW, smCS, dtFI32eX4, dtF32X4, true,    1,  1} //$ - - X X - F  v   vs,i      image load
    ,{ "STOREIM", oitImage,  gpu5,    2,  {atVectNS, atVectNS, atNone},   atNone,   omNone, smNA, dtFI32eX4, dtF32X4, true,    1,  1} //$ X X - - - F  -   i,v,vs    image store
    ,{ "MEMBAR",  oitMembar, gpu5,    0,  {atNone,   atNone,   atNone},   atNone,   omNone, smNA, dtNA,      dtF32,   false,   1,  1} //$ - - - - - F  -   -         memory barrier
//     Name    OpInstType  ExtVer nIn inType1   inType2  inType3   outType    outMask opMod Types de  fType     IsTx   mul add//$ - - - - - -  --- --------  --------------------------------
                                                                                                                              //$ F I C S H D  Out Inputs    Description
};
// See PickAtomicOpModifier()
const UINT32 GpuPgmGen::s_OpConstraints[ocNUM] =
{
        1<<ocNone,              //None
        1<<ocNone | 1<<ocAdd,   //Add
        1<<ocNone | 1<<ocMin | 1<<ocMax,   //Min
        1<<ocNone | 1<<ocMin | 1<<ocMax,   //Max
        1<<ocNone | 1<<ocIwrap | 1<<ocDwrap, //Iwrap & Dwrap use same values.
        1<<ocNone | 1<<ocIwrap | 1<<ocDwrap,//Dwrap & Iwrap use same values.
        1<<ocNone | 1<<ocAnd | 1<<ocOr | 1<<ocXor, //And,Or,Xor use complementry masks
        1<<ocNone | 1<<ocAnd | 1<<ocOr | 1<<ocXor, //Or,And,Xor use complementry masks
        1<<ocNone | 1<<ocAnd | 1<<ocOr | 1<<ocXor, //Xor,And,Or use complementry masks
        1<<ocNone | 1<<ocExch,  //Exch
        1<<ocNone | 1<<ocCSwap, //CSwap
        1<<ocNone | 1<<ocStore, //STORE
};
//------------------------------------------------------------------------------
GpuPgmGen::GpuPgmGen
(
    GLRandomTest * pGLRandom
    ,FancyPickerArray *pPicker
    ,pgmKind           kind
)
  : m_pFpCtx(NULL)
    ,m_Template(0)
    ,m_pPickers(pPicker)
    ,m_pGLRandom(pGLRandom)
    ,m_ElwSize(256)
    ,m_ThisProgWritesLayerVport(false)
    ,m_PgmKind(kind)
{
    m_GS.NumOps = 0;
    m_GS.Ops    = NULL;

    // This list gets shuffled at the start of each frame.
    m_AtomicModifiers.resize(amNUM);
    m_AmIdx = 0;
    m_BnfExt = GLRandomTest::ExtNO_SUCH_EXTENSION;
}

//------------------------------------------------------------------------------
GpuPgmGen::~GpuPgmGen()
{
    ResizeOps(0);
    ResetBlocks();
}

//------------------------------------------------------------------------------
void GpuPgmGen::ResetBlocks()
{
    // Discard our RegState map and old statements strings.
    m_GS.MainBlock = Block(NULL);

    // Discard old Subroutines.
    while (!m_GS.Subroutines.empty())
    {
        Subroutine * pSub = m_GS.Subroutines.back();
        delete pSub;
        m_GS.Subroutines.pop_back();
    }

    // Discard our stack of Block pointers.
    while (!m_GS.Blocks.empty())
    {
        m_GS.Blocks.pop();
    }
}

//------------------------------------------------------------------------------
bool GpuPgmGen::ResizeOps(int numOps)
{
    if (m_GS.NumOps != numOps)
    {
        delete [] m_GS.Ops;
        m_GS.Ops = 0;

        if (numOps > 0)
        {
            m_GS.Ops = new OpData [numOps];
            memset(m_GS.Ops, 0, m_GS.NumOps * sizeof(OpData));
        }
        m_GS.NumOps = numOps;

        return true; // did resize Ops.
    }
    return false; // did NOT resize Ops.
}

//------------------------------------------------------------------------------
void GpuPgmGen::InitGenState
(
    int numOps                  // total number of opcodes
    ,const OpData *pOpData      // additional opcodes to append
    ,PgmRequirements *pRqmts    // optional program requirements
    ,const PassthruRegs * pPTRegs     // pointer to a lookup table of pass-thru registers
    ,bool bAllowLiterals        //
)
{
    // Initialize the Program struct.
    InitProg(&m_GS.Prog, PgmGen::pkTargets[m_PgmKind], pRqmts);
    m_GS.PassthruRegs    = pPTRegs;
    m_GS.PgmStyle        = RND_GPU_PROG_TEMPLATE_Simple;
    m_GS.UsingRelAddr    = false;
    m_GS.LwrSub          = 0;
    m_GS.BranchOpsLeft   = 0;
    m_GS.MsOpsLeft       = 0;
    m_GS.MaxTxgoOffset   = 0;
    m_GS.MinTxgoOffset   = 0;
    m_GS.MinTexOffset    = 0;
    m_GS.MaxTexOffset    = 0;
    m_GS.MinTexOffset    = 0;
    m_GS.MaxTexOffset    = 0;
    m_GS.ConstrainedOpsUsed= 1 << ocNone;
    m_GS.AllowLiterals   = bAllowLiterals;

    // Get the GPU specific limits for texture offsets
    if (m_pGLRandom->HasExt(extGpu4))
    {
        glGetIntegerv(GL_MIN_PROGRAM_TEXEL_OFFSET_EXT, &m_GS.MinTexOffset);
        glGetIntegerv(GL_MAX_PROGRAM_TEXEL_OFFSET_EXT, &m_GS.MaxTexOffset);
    }
    if ( m_pGLRandom->HasExt(extGpu5) )
    {
        glGetIntegerv (GL_MIN_PROGRAM_TEXTURE_GATHER_OFFSET_LW, &m_GS.MinTxgoOffset);
        glGetIntegerv (GL_MAX_PROGRAM_TEXTURE_GATHER_OFFSET_LW, &m_GS.MaxTxgoOffset);
    }

    if (ResizeOps(numOps))
    {
        // Copy in the common set of instructions.
        memcpy(m_GS.Ops, s_GpuOpData4, sizeof(OpData)*RND_GPU_PROG_OPCODE_NUM_OPCODES);

        if (pOpData)
        {
            // now append any additional instructions
            memcpy(&m_GS.Ops[RND_GPU_PROG_OPCODE_NUM_OPCODES],
                   pOpData,
                   sizeof(OpData)*(numOps - RND_GPU_PROG_OPCODE_NUM_OPCODES));
        }
    }

    // Set initial MainBlock state, including initial VarState.
    ResetBlocks();
    m_GS.Blocks.push(&m_GS.MainBlock);
    InitMailwars (&m_GS.MainBlock);

    if (pRqmts)
        InitVarRqdFromPgmRequirements();
}

//------------------------------------------------------------------------------
// Initialize the Program data structure
void GpuPgmGen::InitProg
(
    Program *pProg,
    unsigned int PgmTarget,
    PgmRequirements *pRqmts
)
{
    pProg->Target        = PgmTarget;
    pProg->Pgm           = "";
    pProg->Name          = "";
    pProg->Id            = 0;
    pProg->NumInstr      = 0;
    pProg->SubArrayLen   = 0;
    pProg->Col0Alias     = att_COL0;
    pProg->XfbStdAttrib  = 0;
    pProg->XfbUsrAttrib  = 0;
    pProg->UsingGLFeature.reset();
    // initialize the input/output requirements.
    if (pRqmts)
    {
        pProg->PgmRqmt = *pRqmts;
    }
    else
    {
        pProg->PgmRqmt = PgmRequirements();
    }
}

//------------------------------------------------------------------------------
// Scoring:
//
// Callwlate the "interestingness" score for a given input register,
// based on the swizzle, the register type, and our rough knowlege of
// what has been stored in it so far.
//
// This "interestingness score" scales the likelihood of this reg being
// chosen as input to a given random instruction.
//
// Of course, this entire scheme is a crude approximation.  To really do
// things right, we would have an actual simulator running here, and track
// actual opcode results, etc.
//
UINT32 GpuPgmGen::CalcScore(PgmInstruction *pOp, int reg,int argIdx)
{
    MASSERT(reg < m_GS.MainBlock.NumVars());

    const RegState & v = GetVar(reg);

    if (0 == (v.ReadableMask & v.WrittenMask))
    {
        // Not a readable register, or not written yet.
        return 0;
    }

    UINT32 Score = 0;

    const int numComp = (pOp->GetArgType(argIdx) & atScalar) ? 1 : 4;
    for (int cii = 0; cii < numComp; cii++)
    {
        const int ci = pOp->InReg[argIdx].Swiz[cii];
        Score += v.Score[ci];

        // Unwritten components should have Score of 0.
        MASSERT((v.WrittenMask & (1<<ci)) || (v.Score[ci] == 0));
    }

    return Score;
}

//------------------------------------------------------------------------------
void GpuPgmGen::SetPgmCol0Alias(VxAttribute col0Alias)
{
    m_GS.Prog.Col0Alias = col0Alias;
}

//------------------------------------------------------------------------------
VxAttribute GpuPgmGen::GetPgmCol0Alias()
{
    return m_GS.Prog.Col0Alias;
}

//------------------------------------------------------------------------------
int GpuPgmGen::GetPgmElwSize()
{
    return m_ElwSize;
}

//------------------------------------------------------------------------------
int GpuPgmGen::GetNumTxFetchers()
{
    return m_pGLRandom->NumTxFetchers();
}

//------------------------------------------------------------------------------
void GpuPgmGen::SetContext(FancyPicker::FpContext * pFpCtx)
{
    m_pFpCtx = pFpCtx;
}

//------------------------------------------------------------------------------
void GpuPgmGen::SetProgramTemplate(int templateType)
{
    switch (templateType)
    {
        default:
            m_GS.PgmStyle = RND_GPU_PROG_TEMPLATE_Simple;
            break;
        case RND_GPU_PROG_TEMPLATE_Simple:  // no CAL,IF,or REP instr.
        case RND_GPU_PROG_TEMPLATE_Flow:    // no CAL instr
        case RND_GPU_PROG_TEMPLATE_Call:    // everything goes
        case RND_GPU_PROG_TEMPLATE_CallIndexed:  // use CALI
            m_GS.PgmStyle = templateType;
            break;
    }
}

//------------------------------------------------------------------------------
void GpuPgmGen::SetProgramXfbAttributes( UINT32 stdAttr, UINT32 usrAttr)
{
    m_GS.Prog.XfbStdAttrib = stdAttr;
    m_GS.Prog.XfbUsrAttrib = usrAttr;
}

//------------------------------------------------------------------------------
void GpuPgmGen::MarkRegUsed
(
    int reg
)
{
    RegState v = GetVar(reg);
    v.Attrs |= regUsed;
    m_GS.Blocks.top()->UpdateVar(v);
}

//------------------------------------------------------------------------------
void GpuPgmGen::UpdateRegScore
(
    int reg
    ,int mask
    ,int newScore
)
{
    RegState v = GetVar(reg);

    if (v.Attrs & regIndex)
    {
        // Index regs are declared as integer, keep score at 0 to avoid
        // type errors when used as inputs.
        newScore = 0;
    }
    v.Attrs       |= regUsed;
    v.WrittenMask |= (mask & v.WriteableMask);

    for (int ci = 0; ci < 4; ci++)
    {
        if (mask & (1<<ci))
            v.Score[ci] = newScore;
    }

    m_GS.Blocks.top()->UpdateVar(v);
}

//--------------------------------------------------------------------------------------------
// Report test coverage by decoding the instructions in the program string.
// This is the highest level API so check the following before reporting ISA instruction coverage.
// - We are running gputest.js
// - The user has specified a coverage level other than zero (basic or detailed)
// - We have a coverage database
RC GpuPgmGen::ReportTestCoverage(const string & pgmStr)
{
    if (m_pGLRandom->GetCoverageLevel() != 0)
    {
        CoverageDatabase * pDB = CoverageDatabase::Instance();
        if (pDB)
        {
            RC rc;
            std::size_t start = 0;
            while (start < pgmStr.length())
            {
                std::size_t end = pgmStr.find('\n',start);
                if (end != std::string::npos)
                {
                    CHECK_RC(ReportISATestCoverage(pgmStr.substr(start,end-start)));
                    start = end+1;
                }
            }
        }
    }
    return OK;
}

//--------------------------------------------------------------------------------------------
// Helper function for ReportTestCoverage() to report test coverage by decoding a single instr
// string.
RC GpuPgmGen::ReportISATestCoverage(const string & line)
{
    RC rc;
    if (line.empty() || (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK))
    {
        return rc;
    }

    // We need to extract the ISA instruction from the complete line of text.
    // So try to determine if this is a comment, a declaration, or an instruction.
    unsigned start = 0;
    while (line[start] == ' ' && start < line.length())
    {
        start++;
    }
    if (line[start] == '#')
    {
        return OK; // comment statement
    }
    unsigned  end = start;
    while (end < line.length() &&
           line[end] != ' ' &&
           line[end] != '.' &&
           line[end] != ';' &&
           line[end] != '#' )
    {
        end++;
    }

    string instr = line.substr(start, end-start);
    // Note: The OpData table usually has a single entry for each instruction. However there are a
    //       few special instructions (TEX,TXL,TXB) that have a second entry to describe the special
    //       two source register variant requirements. The code below will check for that.
    // TEX tmpR3.yw, fragment.texcoord[3].xyzw, tmpR6.wywy, texture[17], SHADOWARRAYLWBE;
    // TXB.CC1 tmpR8, fragment.texcoord[6].xyzw, fragment.texcoord[6].xwzy, texture[24], ARRAYLWBE;
    // TXL.CC1.SSAT tmpArray[tmpA1.w + 1], fragment.texcoord[0].xyzw, |tmpR10.xxyw|, texture[14], ARRAYLWBE;
    bool bRequiresTwoSourceVariant = false;
    if (((instr.compare("TXL") == 0) || (instr.compare("TXB") == 0)) &&
        (line.find("ARRAYLWBE") != string::npos))
    {
        bRequiresTwoSourceVariant = true;
    }
    if ((instr.compare("TEX") == 0) && (line.find("SHADOWARRAYLWBE") != string::npos))
    {
        bRequiresTwoSourceVariant = true;
    }

    // try to find this instruction in the op table.
    for (int ii = 0; ii < m_GS.NumOps; ii++)
    {
        if (instr.compare(m_GS.Ops[ii].Name) == 0)
        {
            if (bRequiresTwoSourceVariant && m_GS.Ops[ii].NumArgs != 2)
            {
                continue;
            }
            // The index into this table is the OpId
            CHECK_RC(ReportISATestCoverage(ii));
            return rc;
        }
    }
    return rc;
}

//------------------------------------------------------------------------------------------
// Helper function for ReportISATestCoverage() to report test coverage using the program's OpId.
RC GpuPgmGen::ReportISATestCoverage(UINT32 opId)
{
    if (m_pGLRandom->GetCoverageLevel() != 0)
    {
        CoverageDatabase * pDB = CoverageDatabase::Instance();
        if (pDB)
        {
            RC rc;
            CoverageObject *pCov = nullptr;
            pDB->GetCoverageObject(Hardware,hwIsa, &pCov);
            if (!pCov)
            {
                pCov = new HwIsaCoverageObject;
                pDB->RegisterObject(Hardware,hwIsa,pCov);
            }
            pCov->AddCoverage(opId, m_pGLRandom->GetName());
        }
    }
    return OK;
}

//------------------------------------------------------------------------------
// last chance to validate proper form
int GpuPgmGen::CommitInstruction(PgmInstruction * pOp, bool bUpdateVarMaybe, bool bApplyScoring)
{
    const bool bDontApplyScoring = !bApplyScoring;
    if ((pOp->GetResultType() != atNone) && (pOp->OutReg.Reg == RND_GPU_PROG_REG_none))
    {
        // This op requires an output register but for some reason we couldn't
        // find one that meets all the requirements.
        return GpuPgmGen::instrIlwalid;
    }

    if (pOp->ForceRqdWrite)
    {
        // We're supposed to force a write to a required register.
        // Abort the instruction if we didn't.

        if (pOp->GetResultType() == atNone)
        {
            // This op doesn't write any results -- rejected!
            // Examples: KIL, CAL, etc.
            return GpuPgmGen::instrIlwalid;
        }

        const RegState & v = GetVar(pOp->OutReg.Reg);

        const int needsWrittenMask = v.UnWrittenMask();

        if (needsWrittenMask != (pOp->OutMask & needsWrittenMask))
        {
            // For some other reason (catch-all), it doesn't write all
            // writable & unwritten components -- rejected!
            return GpuPgmGen::instrIlwalid;
        }
    }
    if ((pOp->OutMask == 0) && !(pOp->GetResultType() == atNone))
    {
        // This op's outMask, after being ANDed with the output var's
        // writable mask, left nothing.
        // Example: SCS can only write XY, but result.depth requires Z.
        return GpuPgmGen::instrIlwalid;
    }

    // Instruction is valid. Update output RegState.
    if (pOp->GetResultType() != atNone)
    {
        RegState tmpv = GetVar(pOp->OutReg.Reg);
        if ((tmpv.Attrs & regTemp) &&
           !(tmpv.Attrs & regUsed))
        {
            // First write to a temp reg.
            // Pick a size (16/32/64) based on the operation data type.
            // But don't declare any temps SHORT, we haven't had any HW support
            // for 16-bit registers since LW40.
            if (pOp->dstDT & dt64)
                tmpv.Bits = 64;             // Will be declared "LONG".
            else
                tmpv.Bits = 32;

            // Disable condition-code masking on first write.
            pOp->CCtoRead = RND_GPU_PROG_CCnone;
            pOp->OutTest = RND_GPU_PROG_OUT_TEST_none;
        }

        if (IsIndexReg(pOp->OutReg.IndexReg))
        {
            // since we are going to use relative indexing we better declare
            // and initialize our index register
            MarkRegUsed(pOp->OutReg.IndexReg);
        }

        const bool ccFalse = pOp->OutTest == RND_GPU_PROG_OUT_TEST_fl;
        const bool ccTrue  = (pOp->OutTest == RND_GPU_PROG_OUT_TEST_tr ||
                              pOp->OutTest == RND_GPU_PROG_OUT_TEST_none);
        const bool ccMaybe = !(ccFalse || ccTrue);

        // Update Score of output reg.
        if (IsIndexReg(tmpv.Id) || (tmpv.Attrs & (regSBO|regGarbage) || bDontApplyScoring))
        {
            // Except, leave Score of index, SBO & Garbage regs alone (at 0 as initialized),
            // so we won't try to read from them except as indexes.
            // They are declared as INT, and mods doesn't track this -- causes
            // syntax errors when a float is expected.
        }
        else
        {
            // Calc and update new Score.
            UINT32 newScore = 0;
            for (int argIdx = 0; argIdx < pOp->GetNumArgs(); argIdx++)
            {
                newScore += CalcScore(pOp, pOp->InReg[argIdx].Reg, argIdx);
            }
            newScore = ((newScore * pOp->GetScoreMult()) / OpData::ScoreMultDivisor
                        + pOp->GetScoreAdd()) / 4;

            newScore = max<UINT32>(1, newScore);

            for (UINT32 ic = 0; ic < 4; ic++)
            {
                const int comp = (1<<ic);
                if (pOp->OutMask & comp)
                {
                    MASSERT(tmpv.WriteableMask & comp);
                    if (ccTrue)
                    {
                        tmpv.WrittenMask |= comp;
                        tmpv.Score[ic] = newScore;
                    }
                    else if (ccMaybe && (tmpv.WrittenMask & comp))
                    {
                        // For conditional overwrite, blend old/new.
                        tmpv.Score[ic] = (tmpv.Score[ic] + newScore + 1) / 2;
                    }
                }
            }
        }

        // Update the current Block with the new RegState for the output var.
        if (bUpdateVarMaybe)
        {
            m_GS.Blocks.top()->UpdateVarMaybe(tmpv);
        }
        else
        {
            tmpv.Attrs |= regUsed;
            m_GS.Blocks.top()->UpdateVar(tmpv);
        }

        // We will defer updating PgmRequirements for outputs until
        // the end (in UpdatePgmRequirements).
    }

    // Update each input RegState.
    for (int argIdx = 0; argIdx < pOp->GetNumArgs(); argIdx++)
    {
        if ((pOp->GetArgType(argIdx) == atCC) ||
            (pOp->GetArgType(argIdx) == atLabel) ||
            (pOp->GetArgType(argIdx) == atNone))
        {
            // Not a real input reg, skip updating.
            continue;
        }

        const int inRegId = pOp->InReg[argIdx].Reg;
        RegState tmpv     = GetVar(inRegId);
        if (pOp->ForceRqdWrite && tmpv.WriteableMask && bApplyScoring)
        {
            // Now that we've read from this temp reg to write a required
            // output, reduce its score to so we're less likely to spam the
            // same high-score temp reg to all outputs in later statements.
            for (int iic = 0; iic < 4; iic++)
            {
                const int ic = pOp->InReg[argIdx].Swiz[iic];
                //Don't update special purpose SBO registers.
                if (tmpv.WrittenMask & (1<<ic) && !(tmpv.Attrs & regSBO))
                    tmpv.Score[ic] = max(1, (tmpv.Score[ic] * 2 / 3));
            }
        }

        if (IsIndexReg(pOp->InReg[argIdx].IndexReg))
        {
            // since we are going to use relative indexing we better declare
            // and initialize our index register
            MarkRegUsed(pOp->InReg[argIdx].IndexReg);
        }

        // Update the current Block with the new RegState for the input var.
        tmpv.Attrs |= regUsed;
        m_GS.Blocks.top()->UpdateVar(tmpv);

        if (tmpv.Attrs & regIn)
        {
            // Update our PgmRequirements to show which components of
            // all regIn variables we read.
            ProgProperty prop = GetProp (inRegId);
            MASSERT(prop != ppIlwalid);

            // Some registers are built-in and provided by some fixed functionality and  not
            // written by the upstream shaders. For example the ppBaryCoord & ppBaryNoPersp.
            // If so don't include them in the program requirements.
            if (!IsPropertyBuiltIn(prop))
            {
                for (int iic = 0; iic < 4; iic++)
                {
                    const int ic = pOp->InReg[argIdx].Swiz[iic];
                    m_GS.Prog.PgmRqmt.Inputs[prop] |= (1<<ic);
                }
            }
        }
    }

    if (pOp->GetIsTx())
    {
        // Texture operation -- update texture-binding requirements for this
        // program.  Texture-lookup coordinate is always input arg 0.
        //
        // Add a new record, note it is possible to use the same texture coordinate
        // property to access different texture images via different fetchers.
        // However all of the fetchers using this texture coordinate must be bound to
        // a texture image with the same attribute.
        //
        // Update the texture attributes array
        int rqdCompMask = GetRqdTxCdMask(pOp->TxAttr);

        ProgProperty prop = GetProp(pOp->InReg[0].Reg);

        if (prop != ppIlwalid)
        {
            if (!IsPropertyBuiltIn(prop))
            {
                // Multisampled and Array based texture targets need to have the
                // layer components intact. So pass them through upstream programs.
                if (pOp->TxAttr & (IsArray1d | IsShadowArray1d |
                                   IsArray2d | IsShadowArray2d |
                                   IsArrayLwbe | IsShadowArrayLwbe |
                                   Is2dMultisample))
                {
                    m_GS.Prog.PgmRqmt.Inputs[prop] |= omPassThru;
                }
                m_GS.Prog.PgmRqmt.Inputs[prop] |= rqdCompMask;
            }

            if (!IsBindlessTextures())
            {
                TxcReq txcReq(ppIlwalid,0);
                if( m_GS.Prog.PgmRqmt.InTxc.count(prop))
                {
                    txcReq = m_GS.Prog.PgmRqmt.InTxc[prop];
                }
                txcReq.Prop = prop;
                txcReq.TxfMask |= (1 << pOp->TxFetcher);

                m_GS.Prog.PgmRqmt.InTxc[prop] = txcReq;
            }
        }

        // Always update requirements for how to fill the bindless-texture
        // handle SBO correctly, regardless of what tex-coord input we use.
        if (IsBindlessTextures())
        {
            // setup the bindless texture requirement.
            m_GS.Prog.PgmRqmt.BindlessTxAttr |= (pOp->TxAttr & SASMNbDimFlags);
        }
    }

    // Update the texture bindings used
    if (pOp->TxFetcher >= 0 && pOp->TxFetcher < GetNumTxFetchers())
    {
        m_GS.Prog.PgmRqmt.UsedTxf[pOp->TxFetcher].Attrs |= pOp->TxAttr;
        // TXGO instructions will produce undefined results if
        // TEXTURE_WRAP_S/TEXTURE_WRAP_T are set to CLAMP or MIRROR_CLAMP_EXT
        if (pOp->OpId == RND_GPU_PROG_OPCODE_txgo)
        {
            m_GS.Prog.PgmRqmt.UsedTxf[pOp->TxFetcher].Attrs |= IsNoClamp;
        }

        // update Image Unit bindings for LOADIM/STOREIM/ATOMIM inst.
        if (pOp->IU.unit >= 0)
        {
            if (m_GS.Prog.PgmRqmt.UsedTxf.count(pOp->TxFetcher) &&
                m_GS.Prog.PgmRqmt.UsedTxf[pOp->TxFetcher].IU.unit >= 0)
            {
                MASSERT(m_GS.Prog.PgmRqmt.UsedTxf[pOp->TxFetcher].IU == pOp->IU);
            }
            else
            {
                m_GS.Prog.PgmRqmt.UsedTxf[pOp->TxFetcher].IU = pOp->IU;
            }
            // Note: When using Image Units the bound texture can not have a
            // border pixel.
            m_GS.Prog.PgmRqmt.UsedTxf[pOp->TxFetcher].Attrs |= IsNoBorder;
        }
    }

    if (RND_GPU_PROG_CCnone == pOp->CCtoRead)
    {
        // If we aren't reading a CC register, we might be writing one.
        if (pOp->OpModifier & smCC0)
        {
            m_GS.Blocks.top()->UpdateCCValidMask(pOp->OutMask);
        }
        else if (pOp->OpModifier & smCC1)
        {
            m_GS.Blocks.top()->UpdateCCValidMask(pOp->OutMask << 4);
        }
    }
    else if (RND_GPU_PROG_OUT_TEST_none != pOp->OutTest)
    {
        int validCCMask = m_GS.Blocks.top()->CCValidMask();

        if (pOp->CCtoRead == RND_GPU_PROG_CC0)
            validCCMask &= 0x0f;
        else
            validCCMask >>= 4;

        int swizCount = pOp->CCSwizCount;
        if (!swizCount)
            swizCount = 4;
        for (int i = 0; i < swizCount; i++)
        {
            if (0 == ((1<<i) & pOp->OutMask))
                continue;

            if (0 == (validCCMask & (1 << pOp->CCSwiz[i])))
            {
                Printf(Tee::PriHigh, "Reading Invalid CC bit:\n");
                Printf(Tee::PriHigh, "  %s\n", FormatInstruction(pOp).c_str());
                return GpuPgmGen::instrIlwalid;
            }
        }
    }

    // Set runtime OpenGL features used.
    SetOpenGLFeatures(pOp);
    return GpuPgmGen::instrValid;
}

//-------------------------------------------------------------------------------
// The implementation of some OpenGL features are based on special forms of specific
// instructions. This routine will evaluate each valid instruction to determine if
// any of these features are being used. If so the proper flag will be set which will
// trigger the required OPTION string to be added to the program.
void GpuPgmGen::SetOpenGLFeatures(PgmInstruction * pOp)
{
    switch(pOp->OpId)
    {
        case RND_GPU_PROG_FR_OPCODE_atom:
            if (((pOp->srcDT == dtF64) || pOp->dstDT == dtF64))
                m_GS.Prog.UsingGLFeature[Program::Fp64Atomics] = true;
            //intentional fallthrough
        case RND_GPU_PROG_FR_OPCODE_atomim:
            if ((pOp->srcDT == dtS64) || (pOp->dstDT == dtS64) ||
                (pOp->srcDT == dtU64) || (pOp->dstDT == dtU64) )
                m_GS.Prog.UsingGLFeature[Program::I64Atomics] = true;
            if ((pOp->srcDT == dtF32) || (pOp->dstDT == dtF32) ||
                (pOp->srcDT == dtF32e)|| (pOp->dstDT == dtF32e))
                m_GS.Prog.UsingGLFeature[Program::Fp32Atomics] = true;
            if ((pOp->dstDT == dtF16X2a) || (pOp->dstDT == dtF16X4a))
                m_GS.Prog.UsingGLFeature[Program::Fp16Atomics] = true;
            break;
        default:
            break;
    }
    if ((pOp->srcDT == dtF64) || (pOp->dstDT == dtF64))
    {
        m_GS.Prog.UsingGLFeature[Program::Fp64] = true;
    }
    for (UINT32 i = 0; i < GpuPgmGen::MaxIn; i++)
    {
        switch(pOp->InReg[i].Reg)
        {
            case RND_GPU_PROG_FR_REG_fFULLYCOVERED:
                m_GS.Prog.UsingGLFeature[Program::ConservativeRaster] = true;
                break;
        }
    }
}
//------------------------------------------------------------------------------
//! Called once at the beginning of random-program generation, to mark
//! variables "regRqd" to match pre-generated PgmRequirements.
//!
//! We generate random programs in downstream-first order, i.e. this program
//! can read whatever inputs it likes, but must match the already-generated
//! downstream program's requested outputs.
//!
void GpuPgmGen::InitVarRqdFromPgmRequirements ()
{
    const int numVars = m_GS.MainBlock.NumVars();

    for (int vi = 0; vi < numVars; vi++)
    {
        RegState v = m_GS.MainBlock.GetVar(vi);

        if (v.Attrs & regOut)
        {
            // We only "tune down" the output mask & required flag for some of
            // the possible outputs.  Others are left alone, as set up by
            // InitMailwars.
            bool shouldForceToMatch = false;

            const ProgProperty prop = GetProp (vi);
            switch (prop)
            {
                case ppPriColor:
                case ppSecColor:
                case ppBkPriColor:
                case ppBkSecColor:
                case ppFogCoord:
                case ppPtSize:
                case ppVtxId:
                case ppNrml:
                    shouldForceToMatch = true;
                    break;

                default:
                    if (prop >= ppTxCd0 && prop <= ppTxCd7)
                        shouldForceToMatch = true;
                    else if (prop >= ppClip0 && prop <= ppClip5)
                        shouldForceToMatch = true;
                    break;
            }

            if (shouldForceToMatch)
            {
                if (m_GS.Prog.PgmRqmt.Outputs.count(prop))
                    v.WriteableMask = m_GS.Prog.PgmRqmt.Outputs[prop];
                else
                    v.WriteableMask = omNone;

                if (v.WriteableMask)
                    v.Attrs |= regRqd;
                else
                    v.Attrs &= ~regRqd;

                m_GS.MainBlock.UpdateVar(v);
            }
        }
    }
}

//-----------------------------------------------------------------------------
// Built-in properties don't come from upstream shaders and therefor should not
// be included in the program requirements, because they are always available by
// some fixed functionality.
bool GpuPgmGen::IsPropertyBuiltIn(GLRandom::ProgProperty pp)
{
    switch (pp)
    {
        case GLRandom::ppBaryCoord:
        case GLRandom::ppBaryNoPersp:
        case GLRandom::ppFullyCovered:
        case GLRandom::ppFacing:
            return true;
        default:
            return false;
    }
    return (false);
}
//------------------------------------------------------------------------------
//! Called once at the end of random-program generation, to update the
//! PgmRqmt struct for m_GS.Prog based on the outputs written.
//!
void GpuPgmGen::UpdatePgmRequirements ()
{
    const int numVars = m_GS.MainBlock.NumVars();

    for (int vi = 0; vi < numVars; vi++)
    {
        const RegState & v = m_GS.MainBlock.GetVar(vi);
        const ProgProperty prop = GetProp (vi);

        if ((v.Attrs & regOut) && v.WrittenMask)
        {
            // Mark outputs as available for downstram pipeline stages to
            // read if they are known to have been written.

            MASSERT (ppIlwalid != prop);
            m_GS.Prog.PgmRqmt.Outputs[prop] |= v.WrittenMask;
        }
    }
}

///-----------------------------------------------------------------------------
// Return the required component mask for this texture target.
int GpuPgmGen::GetRqdTxCdMask(int Attr)
{
    int compMask = compNone;
    Attr = Attr & SASMDimFlags;
    // make sure only 1 texture dimension is referenced
    if ( (Attr & (Attr-1)) == 0 )
    {                               // Coordinates used
                                    // s t r  layer  shadow  sample
        switch (Attr)               // ------ -----  ------  ------
        {
            default:  MASSERT(!"Unrecognized tx Attr.");
            // fall through

            case Is2d:              // x y -    -      -
            case IsArray1d:         // x - -    y      -
                compMask = compXY;
                break;

            case Is1d:              // x - -    -      -
                compMask = compX;
                break;

            case IsArray2d:         // x y -    z      -
            case IsShadowArray1d:   // x - -    y      z
            case IsShadow1d:        // x - -    -      z
            case IsShadow2d:        // x y -    -      z
            case Is3d:              // x y z    -      -
            case IsLwbe:            // x y z    -      -
                compMask = compXYZ;
                break;

            case IsShadowArray2d:   // x y -    z      w
            case IsShadowLwbe:      // x y z    -      w
            case IsArrayLwbe:       // x y z    w      -
                compMask = compXYZW;
                break;

            case Is2dMultisample:   // x y -    -      -       w
                compMask = compXYW;
                break;

            //this case actually requires a second vector(or texture coordinate)
            //to specify the shadow value. For now we are just getting it from
            //a random register. So return 4 for now.
            case IsShadowArrayLwbe: // x y z    w      * (requires a 2nd vector)
                compMask = compXYZW;
                break;
        }
    }
    else
        MASSERT(!"More than 1 texture target specified!");

    return compMask;
}

//------------------------------------------------------------------------------
// Grammar specific implementation
void GpuPgmGen::ProgramSequence(const char *pHeader)
{
    MASSERT(m_pFpCtx);

    MASSERT(m_BnfExt); // should be set before calling this fuction.

    static int ms_PgmCount = 0; // debug variable
    int minOps  = GetMinOps();
    int maxOps  = GetMaxOps();

    // Are we using the LW_explicit_multisample feature?
    m_GS.Prog.UsingGLFeature[Program::Multisample] = PickMultisample() != 0;
    if (m_GS.Prog.UsingGLFeature[Program::Multisample])
    {
        m_GS.MsOpsLeft = MIN_OPS_FOR_MS; // force atleast one multisample instr.
        minOps += MIN_OPS_FOR_MS;
    }

    // How many opcodes long shall this program be?
    int ops = PickNumOps();

    // range protect
    ops = MINMAX(minOps, ops, maxOps);

    // How many subroutines to generate?
    // each branch will cost a min of 3 instr.
    GLRandomTest::ExtensionId extNeeded = (m_PgmKind == GLRandom::pkVertex) ?
        (GLRandomTest::ExtensionId)m_GS.Ops[RND_GPU_PROG_OPCODE_cal].ExtNeeded[0] :
        (GLRandomTest::ExtensionId)m_GS.Ops[RND_GPU_PROG_OPCODE_cal].ExtNeeded[1] ;

    int mx = 0;
    if (m_pGLRandom->HasExt(extNeeded))
    {
        mx = min(static_cast<int>(MaxSubs), (ops - minOps)/3);
    }
    int numSubs = (mx == 0 || IsLwrrGMProgPassThru()) ?
                         0 : m_pFpCtx->Rand.GetRandom(1, mx);

    m_GS.PgmStyle = PickPgmTemplate();

    switch (m_GS.PgmStyle)
    {
        default:
            m_GS.PgmStyle = RND_GPU_PROG_TEMPLATE_Simple;
            // fall through
        case RND_GPU_PROG_TEMPLATE_Simple:
        case RND_GPU_PROG_TEMPLATE_Flow:
            numSubs = 0;
            break;

        case RND_GPU_PROG_TEMPLATE_CallIndexed:
            if (! m_pGLRandom->HasExt(extGpu5))
                m_GS.PgmStyle = RND_GPU_PROG_TEMPLATE_Call;
            if (numSubs < 2)
                m_GS.PgmStyle = RND_GPU_PROG_TEMPLATE_Call;
            // fall through

        case RND_GPU_PROG_TEMPLATE_Call:
            if (numSubs == 0)
                m_GS.PgmStyle = RND_GPU_PROG_TEMPLATE_Simple;
            break;
    }

    if (numSubs > 0)
    {
        m_GS.BranchOpsLeft = numSubs;

        if (m_GS.PgmStyle == RND_GPU_PROG_TEMPLATE_CallIndexed)
            m_GS.Prog.SubArrayLen = numSubs;

        for (int si = 0; si < numSubs; si++)
        {
            m_GS.Subroutines.push_back(new Subroutine(&m_GS.MainBlock, si));

            // Choose approximate length for this subroutine.
            // Allow up to 10 of the remaining statements.
            const int maxSubOps = max(1, min(10, ops - minOps - numSubs*3));
            const int subOps = m_pFpCtx->Rand.GetRandom(0, maxSubOps);

            m_GS.Subroutines[si]->m_Block.SetTargetStatementCount (subOps);
            ops -= subOps;
        }
    }

    // Main gets to use up all statements not already reserved for subroutines.
    m_GS.MainBlock.SetTargetStatementCount (ops);

    PgmAppend (pHeader);

    //Add this program's name
    if (m_GS.Prog.Name.size())
    {
        AddPgmComment(Utility::StrPrintf("PgmName:%s", m_GS.Prog.Name.c_str()).c_str());
    }

    // Now add any User comments
    for ( UINT32 i = 0; i < m_GS.Prog.PgmRqmt.UsrComments.size(); i++)
        AddPgmComment( m_GS.Prog.PgmRqmt.UsrComments[i].c_str());

    const char * templateNames[RND_GPU_PROG_TEMPLATE_NUM_TEMPLATES] =
    {
        "Simple_template"
        ,"Call_template"
        ,"Flow_template"
        ,"CallIndexed_template"
    };
    AddPgmComment(templateNames[m_GS.PgmStyle]);
    // Generate main first, it will generate each sub's statements
    // at the point of the first CAL to that Subroutine.

    MainSequenceStart();
    StatementSequence();
    MainSequenceEnd();

    // this is out of order because we dynamically choose what options to enable.
    OptionSequence();

    // Walk the list of used variables, and update program-linking requirements
    // to match the list of output variables the program writes.
    //
    // Texture-binding reqs and inputs were updated during CommitInstruction.

    UpdatePgmRequirements();

    // Now, assemble the program string:
    //   temp & array-idx variable declarations
    //   subroutines
    //   main

    DeclSequence();

    for (int si = 0; si < (int) m_GS.Subroutines.size(); si++)
    {
        const Subroutine & sub = *m_GS.Subroutines[si];

        if (sub.m_IsCalled)
        {
            if ((m_GS.PgmStyle == RND_GPU_PROG_TEMPLATE_CallIndexed) &&
                (si < m_GS.Prog.SubArrayLen))
            {
                PgmAppend (Utility::StrPrintf(
                        "%s SUBROUTINENUM(%d):\n",
                        sub.m_Name.c_str(),
                        si));
            }
            else
            {
                PgmAppend (Utility::StrPrintf(
                        "%s:\n",
                        sub.m_Name.c_str()));
            }
            PgmAppend (sub.m_Block.ProgStr());
            m_GS.Prog.NumInstr += sub.m_Block.StatementCount();
        }
    }

    PgmAppend ("main:\n");
    ArrayVarInitSequence();
    PgmAppend (m_GS.MainBlock.ProgStr());
    m_GS.Prog.NumInstr += m_GS.MainBlock.StatementCount();
    PgmAppend("END\n");

    //------------------------------------------------------------------------
    // make sure all the output registers were written and subroutines have
    // been called.
    if (m_GS.MainBlock.NumRqdUnwritten())
    {
        const int numvars = m_GS.MainBlock.NumVars();
        for (int iv = 0; iv < numvars; ++iv)
        {
            const RegState & v = m_GS.MainBlock.GetVar(iv);
            if ((v.Attrs & regRqd) && v.UnWrittenMask())
            {
                MASSERT(!"required var not written");
            }
        }
    }
    MASSERT( 0 == m_GS.BranchOpsLeft);
    MASSERT( 0 == m_GS.MsOpsLeft);

    ms_PgmCount++;
}

//-----------------------------------------------------------------------------
void GpuPgmGen::DeclSequence()
{
    // Grammar spec:
    // <declSequence> ::=   /* empty */
    // Declare program-elw "constants":
    PgmAppend (Utility::StrPrintf (
            "PARAM %s[%d] = { program.elw[0..%d] };\n",
            GetRegName(GetConstReg()).c_str(),
            m_ElwSize,
            m_ElwSize-1));

    if (IsLwrrGMProgPassThru())
    {
        // Pass through GM does not need any further declarations
        return;
    }

    if (IsBindlessTextures())
    {
        PgmAppend (Utility::StrPrintf(
                "PARAM sboTxAddrReg = program.local[%d];\n",
                SboTxAddrReg));
    }
    const RegState & rs = GetVar(GetPaboReg());

    if (rs.Attrs & regUsed)
    {
        PgmAppend (Utility::StrPrintf(
                "CBUFFER %s[] = { program.buffer[0] };\n",
                GetRegName(GetPaboReg()).c_str()));
    }

    if (m_GS.PgmStyle == RND_GPU_PROG_TEMPLATE_CallIndexed)
    {
        // Declare the "function pointer data type" for the subroutine array
        // and for each indexable subroutine.
        const int numCaliSubs = m_GS.Prog.SubArrayLen;

        PgmAppend ("SUBROUTINETYPE tSub {");
        char comma = ' ';
        for (int si = 0; si < numCaliSubs; si++)
        {
            const Subroutine & sub = *m_GS.Subroutines[si];
            PgmAppend (Utility::StrPrintf(
                    "%c%s", comma, sub.m_Name.c_str()));
            comma = ',';
        }
        PgmAppend (" };\n");

        // Declare an array of "function pointers" that we can use with CALI.
        PgmAppend (Utility::StrPrintf(
                "SUBROUTINE tSub Subs[%d] = { program.subroutine[0..%d] };\n",
                numCaliSubs,
                numCaliSubs-1));
    }

    // Declare any TEMP vars that were used, including array-index temps.
    const int lwar = m_GS.MainBlock.NumVars();

    for (int vi = 0; vi < lwar; ++vi)
    {
        const RegState & v = GetVar(vi);

        if ((v.Attrs & regUsed) && (v.Attrs & (regTemp|regIndex|regGarbage)))
        {
            const char * size_prefix;
            switch (v.Bits)
            {
                case 16:
                    size_prefix = "SHORT ";
                    break;
                case 64:
                    size_prefix = "LONG ";
                    break;

                default:
                    MASSERT(!"Unexpected temp var Bits");
                    // fall through

                case 32:
                    size_prefix = "";
                    break;
            }

            const char* type_prefix =(v.Attrs & regIndex) ? "INT TEMP" : "TEMP";

            PgmAppend (Utility::StrPrintf (
                    "%s%s %s",
                    size_prefix,
                    type_prefix,
                    GetRegName(vi).c_str()));
            if (v.ArrayLen)
                PgmAppend (Utility::StrPrintf ("[%d]", v.ArrayLen));
            PgmAppend (";\n");
        }
    }
    if (IsBindlessTextures())
    {
        PgmAppend (Utility::StrPrintf(
            "LONG TEMP Tx; # .x=SboGpuAddr, .y=TxHandleGpuAddr, .zw=unused\n"));
        PgmAppend (Utility::StrPrintf(
            "LONG TEMP hTx; # .x=Handle, .yzw=unused\n"));
    }

}

//-----------------------------------------------------------------------------
int GpuPgmGen::UtilMapTxAttrToTxHandleIdx( UINT32 txAttr)
{
    static int hLookup[] = {
        -1,-1,-1,-1,                // bits 0-3
        -1,-1,-1,-1,                // bits 4-7
        -1,-1,Ti1d,Ti2d,            // bits 8-11
        Ti3d,TiLwbe,-1,-1,          // bits 12-15
        TiArray1d,TiArray2d,TiArrayLwbe,-1, //bits 16-19
        -1,-1,-1,-1, //bits 20-23
        -1,-1,-1,-1, //bits 24-27
        -1,-1,-1,-1, // bits 28-31
    };
    int bit = Utility::FindNthSetBit(txAttr & SASMDimFlags, 0) & 0x1f;
    MASSERT(hLookup[bit] >= 0);
    return (hLookup[bit]);
}

//----------------------------------------------------------------------------
// Bindless textures need to retrieve one of the appropriate texture object
// handles stored in the TxSbo using the folling sequence of instructions.
// ADD.U64 Tx.y, Tx.x 0x??;                 # get initial offset in TxHandles[]
// MAD.U64 Tx.y, {8,8,8,8}, tmpA3.x, Tx.y;  # add random 0..3 value to offset
// LOAD.U64 hTx.x, Tx.y;                    # load handle from TxHandles[0..?]
void GpuPgmGen::LoadTxHandle(PgmInstruction * pOp)
{
    // Mark it as used so it get declared.
    MarkRegUsed(GetIndexReg(tmpA3));

    int offset = NumTxHandles * sizeof(GLuint64) *
        UtilMapTxAttrToTxHandleIdx(pOp->TxAttr);

    m_GS.Blocks.top()->AddStatement(
        Utility::StrPrintf("ADD.U64 Tx.y, Tx.x, 0x%x; #get initial offset of TxHandles[]",offset));

    m_GS.Blocks.top()->AddStatement(
        Utility::StrPrintf( "MAD.U64 Tx.y, {8,8,8,8}, %s.%c, Tx.y; #add random 0..3 value to offset",
        GetRegName(GetIndexReg(tmpA3)).c_str(),
        "xyzw"[m_pFpCtx->Rand.GetRandom(0,3)]));

    m_GS.Blocks.top()->AddStatement(
        Utility::StrPrintf("LOAD.U64 hTx.x, Tx.y; #load handle from TxHandles[0..3]"));

}

//----------------------------------------------------------------------------

void GpuPgmGen::ArrayVarInitSequence ()
{
    if (IsLwrrGMProgPassThru())
        return;

    if (IsBindlessTextures())
    {
        PgmAppend (" PK64.U Tx, sboTxAddrReg;\n");
    }

    // Set safe default values for registers that need them.
    const int lwar = m_GS.MainBlock.NumVars();
    for (int vi = 0; vi < lwar; ++vi)
    {
        const RegState & v = GetVar(vi);

        if (!(v.Attrs & regUsed))
            continue;

        if (v.Attrs & regIndex)
        {
            // Note: It is expected that the contents of these pgmElw[] are
            // holding integer values NOT floats.
            // MOV.U tmpA128, pgmElw[14]; # random whole numbers 0..128
            // MOV.U tmpA3, pgmElw[13]; # random 0..3
            // MOV.U tmpA1, pgmElw[19]; # random 0 or 1
            // MOV.U tmpASubs, pgmElw[20]; # random indices into Subs

            int pgmElwIdx = 0;
            const char * comment = "";
            if (v.Id == GetIndexReg(tmpA128))
            {
                pgmElwIdx = RelativePgmElwReg;
                comment   = "# random whole numbers 0..128";
            }
            else if (v.Id == GetIndexReg(tmpA3))
            {
                pgmElwIdx = RelativeTxHndlReg;
                comment   = "# random 0..3";
            }
            else if (v.Id == GetIndexReg(tmpA1))
            {
                pgmElwIdx = TmpArrayIdxReg;
                comment   = "# random 0 or 1";
            }
            else if (v.Id == GetIndexReg(tmpASubs))
            {
                pgmElwIdx = SubIdxReg;
                comment   = "# random indices into Subs";
            }
            PgmAppend (Utility::StrPrintf(
                    " MOV.U %s, %s[%d]; %s\n",
                    GetRegName(vi).c_str(),
                    GetRegName(GetConstReg()).c_str(),
                    pgmElwIdx,
                    comment));
        }
        if ((v.Attrs & regTemp) && v.ArrayLen)
        {
            // Preload tmpArray with repeatable data from pgmElw.
            if (m_BnfExt >= extGpu4)
            {
                PgmAppend (Utility::StrPrintf(
                        " INT TEMP i;\n"
                        " MOV.S i.x, %d;\n"
                        " REP;\n"
                        "  MOV %s[i.x], %s[i.x + %d];\n"
                        "  ADD.S.CC0 i.x, i.x, -1;\n"
                        "  BRK (LT0.x);\n"
                        " ENDREP;\n",
                        v.ArrayLen-1,
                        GetRegName(vi).c_str(),
                        GetRegName(GetConstReg()).c_str(),
                        FirstLightReg));
            }
            else // indirect addressing not supported, unroll the loop.
            {
                for (int i=0; i<v.ArrayLen-1; i++)
                {
                    PgmAppend (Utility::StrPrintf(
                            "  MOV %s[%d], %s[%d];\n",
                            GetRegName(vi).c_str(),i,
                            GetRegName(GetConstReg()).c_str(),FirstLightReg+i));
                }
            }
        }
    }
}

//---------------------------------------------------------------------------------
void GpuPgmGen::OptionSequence()
{
    // Add the options that were enabled dynamically
    //-------------------------------------------------------------------------
    // Grammar spec:
    // <optionSequence> ::= <option> <optionSequence>
    //                  | /* empty */
    //
    // Run through all the options we want to declare
    if (m_GS.Prog.UsingGLFeature[Program::Multisample])
        PgmOptionAppend("OPTION LW_explicit_multisample;\n");
    if (m_GS.Prog.UsingGLFeature[Program::Fp64])
        PgmOptionAppend("OPTION LW_gpu_program_fp64;\n");
    if (m_GS.Prog.UsingGLFeature[Program::Fp16Atomics])
        PgmOptionAppend("OPTION LW_shader_atomic_fp16_vector;\n");
    if (m_GS.Prog.UsingGLFeature[Program::Fp64Atomics])
        PgmOptionAppend("OPTION LW_shader_atomic_float64;\n");
    if (m_GS.Prog.UsingGLFeature[Program::I64Atomics])
        PgmOptionAppend("OPTION LW_shader_atomic_int64;\n");
    if (m_GS.Prog.UsingGLFeature[Program::Fp32Atomics])
        PgmOptionAppend("OPTION LW_shader_atomic_float;\n");
    if (m_pGLRandom->m_GpuPrograms.UseBindlessTextures())
        PgmOptionAppend("OPTION LW_bindless_texture;\n");
}
//------------------------------------------------------------------------------
void GpuPgmGen::MainSequenceStart()
{
}

//-----------------------------------------------------------------------------
void GpuPgmGen::MainSequenceEnd()
{
}

//-----------------------------------------------------------------------------
void GpuPgmGen::StatementSequence()
{
    //------------------------------------------------------------------------
    // Grammar spec:
    // <statementSequence> ::=  <statement> <statementSequence>
    //                      | /* empty */
    // <statement> ::= <instruction> ";"
    //              |  <namingStatement> ";"  (not implemented)
    //              |  <instLabel> ";" (handled in <declSequence>)
    while (m_GS.Blocks.top()->StatementsLeft())
    {
        Instruction();
    }

    const bool IsMainBlock = (m_GS.Blocks.top() == &m_GS.MainBlock);

    if (IsMainBlock)
    {
        // Keep adding instructions to the main Block until all the required
        // statments have been done: required outputs written, CAL, etc.

        int numUnwritten = m_GS.MainBlock.NumRqdUnwritten();

        while (numUnwritten + m_GS.BranchOpsLeft + m_GS.MsOpsLeft)
        {
            Instruction();
            numUnwritten = m_GS.MainBlock.NumRqdUnwritten();
        }
    }
}

//-----------------------------------------------------------------------------
void GpuPgmGen::Instruction()
{
    //------------------------------------------------------------------------
    // Grammar spec:
    // <instruction> ::= <ALUinstruction>
    //               | <TexInstruction>
    //               | <FlowInstruction>
    // <ALUInstruction ::=  <VECTORop_instruction>
    //                  |   <SCALARop_instruction>
    //                  |   <BINSCop_instruction>
    //                  |   <BINop_instruction>
    //                  |   <VECSCAop_instruction>
    //                  |   <TRIop_instruction>
    //                  |   <SWZop_instruction>
    // <TexInstruction> ::= <TEXop_instruction>
    //                  |   <TXDop_instruction>
    //                  |   <TEX2op_instruction>  (LW_gpu_program4_1)
    // <FlowInstruction> ::= <BRAop_instruction>
    //                   |  <FLOWCCop_instruction>
    //                   |  <IFop_instruction>
    //                   |  <REPop_instruction>
    //                   |  <ENDFLOWop_instruction>
    PgmInstruction Op;

    // Pick an instr and validate it against the required extension
    int retry;
    for (retry = 0; retry < 50; retry++)
    {
        int opId = PickOp() % m_GS.NumOps;
        Op.Init(&m_GS,opId);
        if (m_pGLRandom->HasExt((GLRandomTest::ExtensionId)Op.GetExtNeeded(m_PgmKind)))
            break;
    }
    MASSERT(retry < 50);// If we assert here we are probably going to create
                        // an invalid program instruction.

    const bool isMainBlock = (m_GS.Blocks.top() == &m_GS.MainBlock);

    if (isMainBlock)
    {
        const int tmp = m_pFpCtx->Rand.GetRandom(0,
                                        m_GS.MainBlock.StatementsLeft());
        const int numUnwritten = m_GS.MainBlock.NumRqdUnwritten();

        if (m_GS.MsOpsLeft > tmp)
        {
            // force a multisample instruction
            Op.Init(&m_GS,RND_GPU_PROG_OPCODE_txfms);
        }
        else if (m_GS.MsOpsLeft + m_GS.BranchOpsLeft > tmp)
        {   // force a branch operation
            if (m_GS.PgmStyle == RND_GPU_PROG_TEMPLATE_CallIndexed)
                Op.Init(&m_GS, RND_GPU_PROG_OPCODE_cali);
            else
                Op.Init(&m_GS, RND_GPU_PROG_OPCODE_cal);
        }
        else if (m_GS.MsOpsLeft + m_GS.BranchOpsLeft + numUnwritten > tmp)
        {
            // Use this op to do a required write to a result register.
            Op.ForceRqdWrite = true;
        }
    }
    else
    {
        // For now, allow only MainBlock to call subroutines.
        if (Op.GetOpInstType() == oitBra)
            return;
    }

    bool bHandled = false;
    switch (Op.GetOpInstType())
    {
        default:
            MASSERT(!"Undefined OpInstType!");
            bHandled = false;                                       break;
        //                ALUInstructions
        case oitVector:     bHandled = VECTORop_instruction(&Op);      break;
        case oitScalar:     bHandled = SCALARop_instruction(&Op);      break;
        case oitBinSC:      bHandled = BINSCop_instruction(&Op);       break;
        case oitBin:        bHandled = BINop_instruction(&Op);         break;
        case oitVecSca:     bHandled = VECSCAop_instruction(&Op);      break;
        case oitTri:        bHandled = TRIop_instruction(&Op);         break;
        case oitSwz:        bHandled = SWZop_instruction(&Op);         break;
        //                TexInstructions
        case oitTex:        bHandled = TEXop_instruction(&Op);         break;
        case oitTxq:        bHandled = TXQop_instruction(&Op);         break;
        case oitTexD:       bHandled = TXDop_instruction(&Op);         break;
        case oitTex2:       bHandled = TEX2op_instruction(&Op);        break;
        case oitTexMs:      bHandled = TXFMSop_instruction(&Op);       break;
        case oitTxgo:       bHandled = TXGOop_instruction(&Op);        break;
        //                SpecialInstructions
        case oitSpecial:    bHandled = SpecialInstruction(&Op);        break;
        case oitPk64:       bHandled = PK64op_instruction(&Op);        break;
        case oitUp64:       bHandled = UP64op_instruction(&Op);        break;
        case oitCvt:        bHandled = CVTop_instruction(&Op);         break;

        //                FlowInstructions
        case oitBra:        bHandled = BRAop_instruction(&Op);         break;
        case oitCali:       bHandled = CALIop_instruction(&Op);        break;
        case oitIf:         bHandled = IFop_instruction(&Op);          break;
        case oitRep:        bHandled = REPop_instruction(&Op);         break;
        //                  Special Bit Field instructions
        case oitBitFld:     bHandled = BITFLDop_instruction(&Op);      break;

        case oitFlowCC:     // handled by oitIf & oitRep
        case oitEndFlow:    // handled by oitIf
            bHandled = false;                                          break;
        case oitMem:        bHandled = MEMop_instruction(&Op);         break;

        case oitImage:      bHandled = IMAGEop_instruction(&Op);       break;
        case oitMembar:     bHandled = MEMBARop_instruction(&Op);      break;
    }

    if (bHandled)
    {
        // Instruction should be complete and ready for final validation
        if (CommitInstruction(&Op) == GpuPgmGen::instrValid)
        {
            CommitInstText(&Op);
        }
    }
}

//-----------------------------------------------------------------------------
//! Add this statement with use comments to the current Block.
void GpuPgmGen::CommitInstText(PgmInstruction *pOp, const string &str, bool bCountStatement)
{
    pOp->SetUsrComment(str);
    CommitInstText(pOp, bCountStatement);
}

void GpuPgmGen::CommitInstText(PgmInstruction *pOp, const char *szComment, bool bCountStatement)
{
    if (szComment)
        pOp->SetUsrComment(szComment);
    CommitInstText(pOp, bCountStatement);
}
//-----------------------------------------------------------------------------
//! Add this statement to the current Block.
void GpuPgmGen::CommitInstText(PgmInstruction *pOp, bool bCountStatement)
{
    m_GS.Blocks.top()->AddStatement(FormatInstruction(pOp), bCountStatement);
}

//-----------------------------------------------------------------------------
//! Append text to the current program.
void GpuPgmGen::PgmAppend (const string& s)
{
    m_GS.Prog.Pgm.append(s);
}
//! Append text to the current program.
void GpuPgmGen::PgmAppend (const char * s)
{
    m_GS.Prog.Pgm.append(s);
}

//-----------------------------------------------------------------------------
//! Insert a new OPTION string just after the program header.
void GpuPgmGen::PgmOptionAppend(const string& s)
{
    size_t pos = m_GS.Prog.Pgm.find(m_PgmHeader);
    if (pos < m_GS.Prog.Pgm.length())
    {
        pos += m_PgmHeader.length();
        m_GS.Prog.Pgm.insert(pos,s);
        return;
    }
    MASSERT(!"Missing required program header when appending OPTION statement!");
}
void GpuPgmGen::PgmOptionAppend(const char * s)
{
    PgmOptionAppend((string)s);
}

//-----------------------------------------------------------------------------
bool GpuPgmGen::VECTORop_instruction(PgmInstruction *pOp)
{
    // Grammar spec:
    // <VECTORop_instruction> ::= <VECTORop> <opModifiers> <instResult> ","
    //                            <instOperandV>
    // <VECTORop> ::= "ABS" | "CEIL" | "FLR" | "FRC" | "I2F" | "LIT" | "MOV" |
    //                "NOT" | "NRM" | "PK2H" | "PK2US" | "PK4B" | "PK4UB" |
    //                "ROUND" | "SSG" | "TRUNC"

    OpModifiers(pOp);
    InstResult(pOp);
    InstOperandV(pOp);

    return true;
}
//-----------------------------------------------------------------------------
bool GpuPgmGen::SCALARop_instruction(PgmInstruction *pOp)
{
    // Grammar spec:
    // <SCALARop_instruction> ::= <SCALARop> <opModifiers> <instResult> ","
    //                            <instOperandS>
    // <SCALARop> ::= "COS" | "EX2" | "LG2" | "RCC" | "RCP" | "RSQ" | "SCS"
    //                      | "SIN" | "UP2H" | "UP2US" | "UP4B" | "UP4UB"

    OpModifiers(pOp);
    InstResult(pOp);
    InstOperandS(pOp);
    return true;
}

//-----------------------------------------------------------------------------
bool GpuPgmGen::BINSCop_instruction(PgmInstruction *pOp)
{
    // Grammar spec:
    // <BINSCop_instruction> ::= <BINSCop> <opModifiers> <instResult> ","
    //                           <instOperandS> "," <instOperandS>
    // <BINSCop> ::= "POW"
    OpModifiers(pOp);
    InstResult(pOp);
    InstOperandS(pOp);
    InstOperandS(pOp);
    return true;
}

//-----------------------------------------------------------------------------
bool GpuPgmGen::VECSCAop_instruction(PgmInstruction *pOp)
{
    // Grammar spec:
    // <VECSCAop_instruction> ::= <VECSCAop> <opModifiers> <instReslt> ","
    //                            <instOperandV> "," <instOperandS>
    // <VECSCAop> ::= "DIV"
    //              | "SHL"
    //              | "SHR"
    //              | "MOD"
    OpModifiers(pOp);
    InstResult(pOp);
    InstOperandV(pOp);
    InstOperandS(pOp);
    return true;
}

//-----------------------------------------------------------------------------
bool GpuPgmGen::BINop_instruction(PgmInstruction *pOp)
{
    // Grammar spec:
    // <BINop_instruction> ::= <BINop> <opModifiers> <instResult> ","
    //                         <instOperandV> "," <instOperandV>
    // <BINop> ::= "ADD" |"AND" |"DP2" |"DP3" |"DP4" |"DPH" |"DST" |"MAX" |
    //             "MIN" |"MUL" |"OR"  |"RFL" |"SEQ" |"SFL" |"SGE" |"SGT" |
    //             "SLE" |"SLT" |"SNE" |"STR" |"SUB" |"XPD" |"XOR" |"BFE"
    OpModifiers(pOp);
    InstResult(pOp);
    InstOperandV(pOp);
    InstOperandV(pOp);
    return true;
}

//-----------------------------------------------------------------------------
bool GpuPgmGen::TRIop_instruction(PgmInstruction *pOp)
{
    // Grammar spec:
    // <TRIop_instruction> ::= <TRIop> <opModifiers> <instReslt> ","
    //                         <instOperandV> "," <instOperandV> ","
    //                         <instOperandV>
    // <TRIop> ::= "CMP"
    //          |  "DP2A"
    //          |  "LRP"
    //          |  "MAD"
    //          |  "SAD"
    //          |  "X2D"
    OpModifiers(pOp);
    InstResult(pOp);
    InstOperandV(pOp);
    InstOperandV(pOp);
    InstOperandV(pOp);
    return true;
}

//-----------------------------------------------------------------------------
bool GpuPgmGen::SpecialInstruction(PgmInstruction *pOp)
{
    bool bHandled = false;
    switch (pOp->OpId)
    {
        // This instruction will produce undefined results if the input scalar is <= 0
        case RND_GPU_PROG_OPCODE_rsq:
        {
            SCALARop_instruction(pOp);
            pOp->InReg[0].Abs = true;
            pOp->InReg[0].Neg = false;
            if (m_pGLRandom->GetUseSanitizeRSQ())
            {
                //We are going to apply a sanitization of the iput register without affecting
                // the random sequence.
                if (CommitInstruction(pOp) == GpuPgmGen::instrValid)
                {
                    m_GS.Blocks.top()->AddStatement("#sanitize RSQ inputs against <= 0.0", false);
                    // now switch out all of the input registers with sanitized temp registers
                    // We just did a SCALARop_instruction so we are accessing a single component.
                    UINT32 writeMask = 1 << pOp->InReg[0].Swiz[0];
                    int garbage = GetGarbageReg();
                    UtilSanitizeInputAgainstZero(pOp, garbage, 0, writeMask);
        
                    // change the input registers to use the new temp registers
                    pOp->InReg[0].Reg = garbage;

                    // update the modified instruction but don't apply scoring again
                    if (CommitInstruction(pOp, false, false) == GpuPgmGen::instrValid)
                        CommitInstText(pOp, true);
                    // return false because we are handling the text insertion
                    bHandled = false;
                }
            }
            else
                bHandled = true;
            break;
        }
        default:
            break;
    }
    return bHandled;
}

//-----------------------------------------------------------------------------
bool GpuPgmGen::SWZop_instruction(PgmInstruction *pOp)
{
    // Grammar spec:
    // <SWZop_instruction> ::= <SWZop> <opModifiers> <instResult> ","
    //                         <instOperandVNS> "," <extendedSwizzle>
    // <SWZop> ::= "SWZ"
    OpModifiers(pOp);
    InstResult(pOp);
    InstOperandVNS(pOp);
    ExtendedSwizzle(pOp);
    return true;
}

//-----------------------------------------------------------------------------
bool GpuPgmGen::TEXop_instruction(PgmInstruction *pOp)
{
    // Grammar spec
    // <TEXop_instruction> ::= <TEXop> <opModifiers> <instResult> ","
    //                         <instOperandV> "," <texAccess>
    GpuSubdevice * pSubdev = m_pGLRandom->GetBoundGpuSubdevice();
    OpModifiers(pOp);
    InstResult(pOp);
    bool bHandled = TexAccess(pOp);
    if (bHandled)
    {
        bHandled = InstOperandTxCd(pOp);
    }
    if (bHandled)
    {
        // On Tesla chips we need to purge +/-INF & NaN LODs and biases. ie.
        // Change:  TXL dest, src, texture[x], 2D
        // into:    MOV temp, src;
        //          #x-x == 0 for all <x> except +/-INF and NAN. Use NE to slam
        //          #all three to zero
        //          ADD.F.CC0 garbage.w, -temp.w, temp.w;
        //          MOV.F temp.w (NE), 0;
        //          TXL dest,temp, texture[x], 2D
        if (pSubdev->HasBug(616873) &&
            (pOp->OpId == RND_GPU_PROG_OPCODE_txl ||
             pOp->OpId == RND_GPU_PROG_OPCODE_txb))
        {
            // we must call commitInstruction so the texture coordinate
            // requirements get properly updated.
            if(CommitInstruction(pOp) == GpuPgmGen::instrValid)
            {
                m_GS.Blocks.top()->AddStatement(string("#Applying WAR616873"));

                // now switch out the src register for a temp register and
                // filter out +/-INF & NAN
                const int tempReg = PickTempReg();
                const int garbageReg =
                    tempReg > GetFirstTempReg() ? (tempReg-1) : (tempReg+1);
                UtilSanitizeInput( pOp, garbageReg, tempReg, 0, compW);
                pOp->InReg[0].Reg = tempReg;
            }
            else
                bHandled = false;
        }

        // TXG and TXGO can have single component qualifiers on the texture
        // TXG only supports component qualifiers for gpu 5 or above
        if (m_pGLRandom->HasExt(extGpu5) &&
            (pOp->OpId == RND_GPU_PROG_OPCODE_txg))
        {
            pOp->TxgTxComponent = m_pFpCtx->Rand.GetRandom(0,3);
        }

        if (IsBindlessTextures())
        {
            LoadTxHandle(pOp);
        }
    }
    return bHandled;
}

//-----------------------------------------------------------------------------
bool GpuPgmGen::TXFMSop_instruction(PgmInstruction *pOp)
{
    // Grammar spec
    // <TEXop_instruction> ::= <TEXop> <opModifiers> <instResult> ","
    //                         <instOperandV> "," <texAccess>
    // The grammar spec. is the same as the standard <TEXop_instruction>,
    // however there is some special processing that we must do to insure that
    // sample value is within range.
    // There are several limitations when using this inst.
    // 1. You can't set the standard texture parameters, so wrapping and
    //    clamping don't work.
    // 2. Texture access's that are out or range are undefined.
    // 3. Texture coordinates need to be in texel coordinates so the coordinates
    //    need to be scaled the the width/height of the texture image.
    // 4. The sample index value must be < number of samples in the texture
    //    image or we get undefined results.
    // 5. The data type for the texel fetch must be an int (not a float).
    //
    // There are also several GLRandom architecture constraints that must be
    // considered.
    // 6. Canned vertex programs are guarenteed to pass through the texture
    //    coordinates 4..7, 0..3 get munged by special instruction testing.
    //    So we can only use coordinate sets 4..7.
    // 7. RndTexturing is assuming that texCoordSetN is used with texFetcherN,
    //    so this limits use to use fetchers 4..7.
    // 8. When multisample is enabled we need to insure that we have atleast 1
    //    texture image that is bound to a TEXTURE_RENDERBUFFER_LW.
    //
    // So here is what we are going to do:
    // RndTexturing will properly scale the x,y,&w values for the texture
    // coordinate, however the format of these values will be a float. So we
    // need to colwert them to an int.

    // 1. We will colwert the xyw components of coordinate set 4-7 to an int and
    //    load it into a random temp register.
    // 2. We will do a texture fetch using the temp register and the appropriate
    //    fetcher 4-7.
    // We should end up with a set of instructions like the following(assuming
    // we will be using texture fetcher #4 and a randomly picked tmpR1 register)
    // "FLR.U tmpR1.xyzw, fragment.texcoord[4].xyzw;"
    // "TXFMS result.color.xyz, tmpR1, texture[4], RENDERBUFFER;"

    const bool IsMainBlock = (m_GS.Blocks.top() == &m_GS.MainBlock);

    if (!m_GS.Prog.UsingGLFeature[Program::Multisample] || !IsMainBlock )
    {
        return false;
    }

    const int tempReg  = PickTempReg();

    // We have to build the TXFMS before the "FLR" instruction because we need
    // to know which texture fetcher will be used.
    // "TXFMS result.color.xyz, tmpR?, texture[?], RENDERBUFFER;"
    InstResult(pOp); // not guarenteed to produce a result reg.

    if (pOp->OutReg.Reg != RND_GPU_PROG_REG_none)
    {
        bool bHandled = TexAccess(pOp);
        if (bHandled)
        {
            // Add the instructions in the correct order, and do some
            // housekeeping. Build and add the FLR inst.
            PgmInstruction flr;
            flr.Init(&m_GS, RND_GPU_PROG_OPCODE_flr);
            flr.dstDT         = dtU32;
            flr.OutReg.Reg    = tempReg;
            flr.InReg[0].Reg  = GetFirstTxCdReg() + pOp->TxFetcher;
            flr.LwrInReg++;
            if (CommitInstruction(&flr) == GpuPgmGen::instrValid)
            {
                CommitInstText(&flr);
            }
            else
            {
                MASSERT(!"Malformed FLR instr!");
            }

            // Setup the TXFMS instr as though the texture coordinate register
            // was actually being used and then call CommitInstruction() to
            // update the proper input requirements. ie. setup the m_GS state to
            // make the instruction look like this:
            // "TXFMS result.???,fragment.texcoord[x].xyzw,texture[x],RENDERBUFFER"
            pOp->InReg[0].Reg = GetFirstTxCdReg() + pOp->TxFetcher;
            pOp->InReg[0].SwizCount = 4;// we need 4 to access the w component.
            pOp->InReg[0].Swiz[siX] = scX;
            pOp->InReg[0].Swiz[siY] = scY;
            pOp->InReg[0].Swiz[siZ] = scZ;
            pOp->InReg[0].Swiz[siW] = scW;
            pOp->LwrInReg = 1;
            if (CommitInstruction(pOp) == instrValid)
            {
                pOp->InReg[0].Reg = tempReg;
            }
            else
            {
                bHandled = false;
                MASSERT(!"Malformed TXFMS instruction!");
            }
            if (bHandled)
            {
                if (IsBindlessTextures())
                    LoadTxHandle(pOp);

                // Mark the input texcoord as must-be-CENTROID.
                RegState v = m_GS.MainBlock.GetVar(flr.InReg[0].Reg);
                v.Attrs |= regCentroid;
                m_GS.MainBlock.UpdateVar(v);
            }
        }

        // If TexAccess() fails its more than likely the fancy picker has been
        // configured to not pick Is2dMultisample. So reset the MsOpsLeft
        // counter to prevent infinite loops.
        m_GS.MsOpsLeft = 0;
        return bHandled;
    }
    return false;
}

//-----------------------------------------------------------------------------
bool GpuPgmGen::TXDop_instruction(PgmInstruction *pOp)
{
    // Grammar spec
    // <TXDop_instruction> ::= <TXDop> <opModifiers> <instResult> ","
    //                         <instOperandV> "," <instOperandV> ","
    //                         <instOperandV> "," <texAccess>
    // <TXDop> ::=  "TXD"
    //          |   "TXGO"
    // <texAccess> ::= <texImageUnit> "," <texTarget>
    //              |  <texImageUnit> "," <texTarget> "," <texOffset>
    GpuSubdevice * pSubdev = m_pGLRandom->GetBoundGpuSubdevice();

    OpModifiers(pOp);
    InstResult(pOp);
    bool bHandled = TexAccess(pOp);
    if (bHandled)
    {
        bHandled = InstOperandTxCd(pOp);
    }
    if (bHandled)
    {
        InstOperandV(pOp);
        InstOperandV(pOp);
        // For bug 489105 workaround.
        m_GS.Blocks.top()->SetUsesTXD();

        // On Tesla & Fermi chips we need to sanitize the input partial
        // derivitives so there aren't any +/-INF or NAN values to prevent
        // non-deterministic results.
        if (pSubdev->HasBug(630560) &&
            (m_GS.Prog.Target != GL_FRAGMENT_PROGRAM_LW) &&
            !(pOp->TxAttr &
                (IsLwbe |IsArrayLwbe | IsShadowLwbe | IsShadowArrayLwbe))
           )
        {
            m_GS.Blocks.top()->AddStatement(string("#Applying WAR630560 (sanitize TXD inputs)"));
            // now switch out all of the input registers with sanitized temp
            // registers
            const int tmpR1 = GetFirstTempReg()+1;
            const int tmpR2 = tmpR1+1;
            const int garbageR = tmpR1+2;
            UtilSanitizeInput(pOp, garbageR, tmpR1, 1, compXYZW);
            UtilSanitizeInput(pOp, garbageR, tmpR2, 2, compXYZW);
            // change the input registers to use the new temp registers
            pOp->InReg[1].Reg = tmpR1;
            pOp->InReg[2].Reg = tmpR2;
        }
        if (IsBindlessTextures())
            LoadTxHandle(pOp);
    }
    return bHandled;
}

//-----------------------------------------------------------------------------
bool GpuPgmGen::TXQop_instruction(PgmInstruction *pOp)
{
    // TXQ is a TEXop, except that for repeatability we need to force a zero
    // for the input operand, and the output mask depends on the texture dim.

    OpModifiers(pOp);

    // Do the TexAccess first, because InstResult will indirectly call
    // PgmInstruction::GetResultMask, which needs to know the texture dimension.

    bool bHandled = TexAccess(pOp);

    // Now we can choose a result register and write-mask.
    InstResult(pOp);

    // Force first arg to be constant 0.
    // We always query the size of mipmap level 0, since we don't know how big a
    // texture will be bound at runtime.
    UtilOpInConstU32(pOp, UINT32(0));

    if (bHandled && IsBindlessTextures())
        LoadTxHandle(pOp);

    return bHandled;
}

//-----------------------------------------------------------------------------
// Grammar is the same as the <TXDop_instruction> but there must be some range
// checking done to the 2nd & 3rd operands which contain the x/y offsets.
bool GpuPgmGen::TXGOop_instruction(PgmInstruction *pOp)
{
    // Grammar spec
    // <TXDop_instruction> ::= <TXDop> <opModifiers> <instResult> ","
    //                         <instOperandV> "," <instOperandV> ","
    //                         <instOperandV> "," <texAccess>
    // <TXDop> ::=  "TXD"
    //          |   "TXGO"
    // <texAccess> ::= <texImageUnit> "," <texTarget>
    //              |  <texImageUnit> "," <texTarget> "," <texOffset>

    // TXGO instruction require the IsNoClamp attribute to be set. Lwrrently
    // GLRandom can't accomodate this requirement with bindless textures.
    if (IsBindlessTextures())
        return false;

    bool bHandled = true;
    int   offsets[4]= {0, 0, 0, 0};
    INT32 offset    = 0;
    INT32 maxOffset = 0;
    INT32 firstTemp = GetFirstTempReg();
    INT32 lastTemp  = firstTemp + GetNumTempRegs()-1;

    OpModifiers(pOp);
    InstResult(pOp);
    bHandled = TexAccess(pOp);
    if (bHandled)
    {
        bHandled = InstOperandTxCd(pOp);
    }
    if (bHandled)
    {
        InstOperand( pOp, firstTemp, firstTemp, lastTemp);
        InstOperand( pOp, firstTemp+1, firstTemp, lastTemp);
        // range check the 2nd & 3rd vectors and set the inputs as
        // const vectors to keep the internal scoring correct.
        if (m_GS.MinTxgoOffset < 0)
            offset = abs(m_GS.MinTxgoOffset);
        maxOffset = m_GS.MaxTxgoOffset + offset;
        offsets[0] = m_pFpCtx->Rand.GetRandom(0,maxOffset) - offset;
        offsets[1] = m_pFpCtx->Rand.GetRandom(0,maxOffset) - offset;
        offsets[2] = m_pFpCtx->Rand.GetRandom(0,maxOffset) - offset;
        offsets[3] = m_pFpCtx->Rand.GetRandom(0,maxOffset) - offset;
        UtilCopyConstVect(pOp->InReg[1].Reg, offsets);

        offsets[0] = m_pFpCtx->Rand.GetRandom(0,maxOffset) - offset;
        offsets[1] = m_pFpCtx->Rand.GetRandom(0,maxOffset) - offset;
        offsets[2] = m_pFpCtx->Rand.GetRandom(0,maxOffset) - offset;
        offsets[3] = m_pFpCtx->Rand.GetRandom(0,maxOffset) - offset;
        UtilCopyConstVect(pOp->InReg[2].Reg,offsets);

        // TXG and TXGO can have single component qualifiers on the texture
        pOp->TxgTxComponent = m_pFpCtx->Rand.GetRandom(0,3);
    }

    return bHandled;
}
//-----------------------------------------------------------------------------
bool GpuPgmGen::TEX2op_instruction(PgmInstruction *pOp)
{
    // Grammar spec
    // <TEX2op_instruction> ::= <TEXop> <opModifiers> <instResult> ","
    //                         <instOperandV> "," <instOperandV> "," <texAccess>
    // <TEXop> ::= "TEX" | "TXL" | "TXB"
    // <texAccess> ::= <texImageUnit> "," <texTarget>
    //              |  <texImageUnit> "," <texTarget> "," <texOffset>
    // <texTarget> ::= "ARRAY1D"       | "ARRAY2D"       | "ARRAYLWBE"      |
    //                 "SHADOWARRAY1D" | "SHADOWARRAY2D" | "SHADOWARRAYLWBE"
    // Note: The LW_gpu_program4_1 program grammar is very vague on the
    // TEX2op_instruction. You actually have to read the instr. specific text to
    // determine the restrictions on the grammar above. I have stated the
    // restrictions to <TEXop> and <texTarget> for this instruction in the
    // text above.
    GpuSubdevice * pSubdev = m_pGLRandom->GetBoundGpuSubdevice();
    OpModifiers(pOp);
    InstResult(pOp);
    bool bHandled = TexAccess(pOp);
    if (bHandled)
    {
        bHandled = InstOperandTxCd(pOp);
    }
    if (bHandled)
    {
        InstOperandV(pOp);
        // On Tesla chips we need to purge +/-INF & NaN LODs and biases. ie.
        // Change:  TXL dest, src, texture[x], 2D
        // into:    MOV temp, src;
        //          #x-x == 0 for all <x> except +/-INF and NAN. Use NE to slam
        //          #all three to zero
        //          ADD.F.CC0 garbage.w, -temp.w, temp.w;
        //          MOV.F temp.w (NE), 0;
        //          TXL dest,temp, texture[x], 2D
        if (pSubdev->HasBug(616873) &&
            (pOp->OpId == RND_GPU_PROG_OPCODE_txl2 ||
             pOp->OpId == RND_GPU_PROG_OPCODE_txb2))
        {
            // we must call commitInstruction so the texture coordinate
            // requirements get properly updated.
            if (CommitInstruction(pOp) != GpuPgmGen::instrValid)
            {
                MASSERT(!"Malformed TEX2 instr!");
                bHandled = false;
            }
            m_GS.Blocks.top()->AddStatement(string("#Applying WAR616873"));

            // now switch out the src register for a temp register
            int tempReg = PickTempReg();
            int garbageReg = tempReg > GetFirstTempReg() ?
                (tempReg-1):(tempReg+1);
            UtilSanitizeInput(pOp, garbageReg, tempReg, 1, compX);
            pOp->InReg[1].Reg = tempReg;
        }

        if (IsBindlessTextures())
            LoadTxHandle(pOp);
    }
    return bHandled;
}

//-----------------------------------------------------------------------------
// BTFI & BTFE must have op0 range checked to prevent undefined results.
// They also only work on signed & unsigned int data types.
bool GpuPgmGen::BITFLDop_instruction(PgmInstruction *pOp)
{
    bool bHandled = false;

    if (pOp->OpId == RND_GPU_PROG_OPCODE_bfi ||
        pOp->OpId == RND_GPU_PROG_OPCODE_bfe )
    {
        bHandled = true;
        // Pick a temp register and initialize the x and y components with
        // acceptable bit offset & numBits values
        const int reg = PickTempReg();
        int vals[4];
        vals[0] = m_pFpCtx->Rand.GetRandom(0,31);
        vals[1] = m_pFpCtx->Rand.GetRandom(1, 32-vals[0]);
        vals[2] = 1; //Do not use 0, as this reg component could be used in DIV
        vals[3] = 1;
        UtilCopyConstVect(reg,vals);

        // Standard inst. processing.
        OpModifiers(pOp);
        InstResult(pOp);
        pOp->InReg[0].Reg = reg;
        pOp->LwrInReg++;
        InstOperandV(pOp);
        if (pOp->OpId == RND_GPU_PROG_OPCODE_bfi)
        {
            InstOperandV(pOp);
        }
    }
    return bHandled;
}

//-----------------------------------------------------------------------------
bool GpuPgmGen::CVTop_instruction(PgmInstruction *pOp)
{
    // Grammar spec:
    // <CVTop_instruction> ::= <CVTop>  <opCCSModifiers> <opStorageModifier>
    //                         <opStorageModifier> <tempUseW> ","
    //                         <instOperandV>
    // <CVTop>             ::= "CVT"
    // <opCCSModifiers>    ::= <opModifierItem> <opCCSModifier>
    // <opModifierItem> ::= "."
    // <opCCSModifier>     ::= "SAT"    - clamp [0,1]
    //                       | "SSAT"   - clamp [-1,1]
    //                       | "CC0"    - condition code0 update
    //                       | "CC1"    - condition code1 update
    // <opStorageModifiers>::= <opModifierItem> <opStorageModifer>
    //                          <opModifierItem> <opStorageModifier>
    // <opStorageModifier> ::= "S8"
    //                      | "S16"
    //                      | "S32"
    //                      | "S64"
    //                      | "U8"
    //                      | "U16"
    //                      | "U32"
    //                      | "U64"
    //                      | "F16"
    //                      | "F32"
    //                      | "F64"
    // This op is going to write to a Tmp register, so return not handled if
    // there is a required write.
    if (pOp->ForceRqdWrite)
        return false;

    // implement both required storage modifiers ".dst.src"
    pOp->OpModifier = pOp->GetOpModMask() & PickOpModifier();
    opDataType srcDT = dtNone;
    opDataType dstDT = dtNone;
    int retry;
    for (retry = 0; retry < 10 && srcDT == dtNone; retry++)
    {
        srcDT = PickOpDataType(pOp);
    }
    for (retry = 0; retry < 10 &&
         (dstDT == dtNone || dstDT == srcDT); retry++)
    {
        dstDT = PickOpDataType(pOp);
    }
    if( srcDT == dtNone || dstDT == dtNone || srcDT == dstDT)
    {
        return false;
    }

    pOp->dstDT = dstDT;
    pOp->srcDT = srcDT;

    // Pick a SAT modifier, only applies if we are writing a float
    if (!(dstDT & dtF))
        pOp->OpModifier &= ~smSS;

    // Pick a rounding modifier if we are colwerting from a float to int
    if ((srcDT & dtF) && (dstDT & dtI))
    {
        switch (m_pFpCtx->Rand.GetRandom(0, 5))
        {
            case rmCeil:
                pOp->OpModifier |= smCEIL;
                break;
            case rmFlr:
                pOp->OpModifier |= smFLR;
                break;
            case rmTrunc:
                pOp->OpModifier |= smTRUNC;
                break;
            case rmRound:
                pOp->OpModifier |= smROUND;
                break;
            case rmNone:
            default:        break;
        }
    }
    InstResult(pOp);
    InstOperandV(pOp);
    // now make sure the following have not been set.
    pOp->InReg[0].Abs = false;
    pOp->InReg[0].Neg = false;

    return true;
}

//-----------------------------------------------------------------------------
// Pack a 32bit register into the xy & zw components of a 64 bit register's
// x & y components.
//
bool GpuPgmGen::PK64op_instruction(PgmInstruction *pOp)
{
    // This op is going to write to a Tmp register, so return not handled if
    // there is a required write.
    if (pOp->ForceRqdWrite)
        return false;

    // pick a temp register that can be declared as 64bit
    int reg = 0;
    int firstTmp = GetFirstTempReg();
    int numTmp = GetNumTempRegs();
    int offset = m_pFpCtx->Rand.GetRandom(0,numTmp);
    for (int i = 0; i < numTmp; i++)
    {
        // PickOperand() updates m_GS.InReg[].Reg
        reg = ((i+offset)%numTmp)+firstTmp;
        // make sure it can be configured as a 64bit reg.
        const RegState & tmpv = GetVar(reg);
        if (!(tmpv.Attrs & regUsed) || (tmpv.Bits == 64))
        {
            OpModifiers(pOp);  //PK64 only supports the F/S/U datatype modifiers

            // manually implement the InstResult()
            pOp->OutReg.Reg = reg;
            OptWriteMask(pOp);
            CCMask(pOp);

            InstOperandV(pOp);
            return true;
        }
    }
    return false;
}

//-----------------------------------------------------------------------------
// Unpack the xy components of a 64bit register into a 32 bit register.
bool GpuPgmGen::UP64op_instruction(PgmInstruction *pOp)
{
    // This op is going to write to a Tmp register, so return not handled if
    // there is a required write.
    if (pOp->ForceRqdWrite)
        return false;

    int reg = GetFirstTempReg();
    const int numTmp = GetNumTempRegs();
    for (int i = 0; i < numTmp; i++, reg++)
    {
        const RegState & tmpv = GetVar(reg);
        if (((tmpv.WrittenMask & compXY) == compXY) && (tmpv.Bits == 64))
        {
            OpModifiers(pOp);  // UP64 only supports F/S/U datatype modifiers
            InstResult(pOp);
            // manually implement the InstOperandV()
            pOp->InReg[pOp->LwrInReg].Reg = reg;
            pOp->InReg[pOp->LwrInReg].Neg =  PickNeg(); // <operandNeg>
            pOp->InReg[pOp->LwrInReg].Abs = (PickAbs() != 0);
            InstOperand(pOp, rndNoPick);
            return true;
        }
    }
    return false;
}

//-----------------------------------------------------------------------------
// Memory LDC instructions
bool GpuPgmGen::MEMop_instruction(PgmInstruction *pOp)
{
    // Grammar spec:
    // <VECTORop_instruction> ::= <VECTORop> <opModifiers> <instResult> ","
    //                            <instOperandV>
    // <VECTORop> ::= "ABS" | "CEIL" | "FLR" | "FRC" | "I2F" | "LIT" | "MOV" |
    //                "NOT" | "NRM" | "PK2H" | "PK2US" | "PK4B" | "PK4UB" |
    //                "ROUND" | "SSG" | "TRUNC"

    OpModifiers(pOp);
    InstResult(pOp);
    InstOperand(pOp, GetPaboReg(), GetPaboReg(), GetPaboReg());
    // we need to do a the following fixups
    // 1. Make sure the IndexOffset is properly aligned with the data type.
    unsigned mask = ~(GetMemAccessAlignment(pOp->dstDT)-1);
    pOp->InReg[0].IndexOffset &= mask;

    // 2. If using a RelAddr register it too must be data aligned at runtime
    const int idxReg = GetIndexReg(tmpA128);
    if(pOp->InReg[0].IndexReg == idxReg)
    {
        // Add one of the following instructions:
        // AND.U TmpA128.x , TmpA128.x, ~0x07;  8byte alignment
        // AND.U TmpA128.x , TmpA128.x, ~0x03;  4byte alignment
        // AND.U TmpA128.x , TmpA128.x, ~0x01;  2byte alignment
        PgmInstruction AndOp;
        UtilOpDst(&AndOp, RND_GPU_PROG_OPCODE_and, idxReg);
        AndOp.OutMask = 1 << pOp->InReg[0].IndexRegComp;
        AndOp.dstDT = dtU32;

        UtilOpInReg(&AndOp, idxReg);
        AndOp.InReg[0].SwizCount = 1;
        AndOp.InReg[0].Swiz[siX] = pOp->InReg[0].IndexRegComp; //.x .y .z or .w

        UtilOpInConstU32(&AndOp,mask);
        UtilOpCommit(&AndOp);
     }
    return true;

}

//-----------------------------------------------------------------------------
// Instruction Vector Operand
void GpuPgmGen::InstOperandV(PgmInstruction *pOp)
{
    // Vector operands
    InstOperand(pOp);
}

//-----------------------------------------------------------------------------
// Instruction Scalar Operand
void GpuPgmGen::InstOperandS(PgmInstruction *pOp)
{
    // Scalar operands
    InstOperand(pOp);
}

//-----------------------------------------------------------------------------
// Instruction Vector Operand with No Suffix
void GpuPgmGen::InstOperandVNS(PgmInstruction *pOp)
{
    // Operands with no swizzle or scalar suffix
    InstOperand(pOp);
}

//------------------------------------------------------------------------------
// Pick an appropriate input for the texture coordinate operand.
//
bool GpuPgmGen::InstOperandTxCd( PgmInstruction *pOp)
{
    bool bFound = false;
    if (pOp->TxAttr & (IsArray1d | IsShadowArray1d |
                       IsArray2d | IsShadowArray2d |
                       IsArrayLwbe | IsShadowArrayLwbe))
    {
        GLRandom::PgmRequirements::TxfMap & rsvd = m_GS.Prog.PgmRqmt.RsvdTxf;
        GLRandom::PgmRequirements::TxfMap & used = m_GS.Prog.PgmRqmt.UsedTxf;
        // We need a texture coordinate that has the layer component properly
        // scaled
        int reg = m_pFpCtx->Rand.GetRandom(GetFirstTxCdReg(),GetLastTxCdReg());
        const int numRegs = GetLastTxCdReg() - GetFirstTxCdReg() + 1;
        for (int i = 0; (i < numRegs) && !bFound; i++)
        {
            const RegState & v = GetVar(reg);
            if (v.ReadableMask)
            {
                // We need to find a texture coordinate property that is either
                // "unused" or is already being use by another fetcher that is
                // bound to a texture image with the same attribute.
                // ie. we can't use a texture coordinate register that is being
                // used to access an ARRAY_1D target on an ARRAY_3D target,
                // because the layer value is in a different component.
                ProgProperty prop = GetProp(reg);
                if (m_GS.Prog.PgmRqmt.InTxc.count(prop))
                {
                    // this texture coordinate is already being used, lets see
                    // if its fetchers are bound to the same target.
                    TxcReq txcReq = m_GS.Prog.PgmRqmt.InTxc[prop];
                    for (int txf=0; txf < GetNumTxFetchers(); txf++)
                    {
                        if( 0 == (txcReq.TxfMask & (1<<txf)))
                            continue;

                        if ((used.count(txf) &&
                             ((used[txf].Attrs & pOp->TxAttr) == pOp->TxAttr))
                             ||
                            (rsvd.count(txf) &&
                             ((rsvd[txf].Attrs & pOp->TxAttr) == pOp->TxAttr)))
                        {
                            bFound = true;
                            break;
                        }
                    }
                }
                else    // this texture coordinate is unused.
                {
                    bFound = true;
                }
            }
            if (!bFound)
            {   // wrap if necessary.
                reg =  (reg == GetLastTxCdReg()) ? GetFirstTxCdReg() : reg+1;
            }
        }

        if (bFound)
        {
            pOp->InReg[pOp->LwrInReg].Reg = reg;
            pOp->InReg[pOp->LwrInReg].Neg =  false; // <operandNeg>
            pOp->InReg[pOp->LwrInReg].Abs = false;
            pOp->InReg[pOp->LwrInReg].SwizCount = 4;
            pOp->InReg[pOp->LwrInReg].Swiz[siX] = scX;
            pOp->InReg[pOp->LwrInReg].Swiz[siY] = scY;
            pOp->InReg[pOp->LwrInReg].Swiz[siZ] = scZ;
            pOp->InReg[pOp->LwrInReg].Swiz[siW] = scW;
            pOp->LwrInReg++;    // move on to the next operand
        }
    }
    else    // any operand will do in this case.
    {
        int idx = pOp->LwrInReg;
        int i = 0;
        const int retries = 10;

        do
        {
            InstOperandV(pOp);
            // we don't really want to be picking a const register for texture
            // coords because it would cause the pipeline to fill up with 1000s
            // of reads from the same memory address.
            if ((pOp->InReg[idx].Reg == GetConstReg()) &&
                (pOp->GetOpInstType() == oitImage))
            {
                pOp->LwrInReg--; // do over
            }
            else
            {
                bFound = true;
            }
        } while (!bFound && (++i < retries));
    }

    if (!bFound)
        Printf(Tee::PriDebug,
            "Unable to find a TxCoord Register for TxAttr:%x\n",
            pOp->TxAttr);

    return bFound;
}

//------------------------------------------------------------------------------
// Pick an operand from a range of acceptable registers.
void GpuPgmGen::InstOperand
(
    PgmInstruction *pOp,
    int dfltReg,
    int minReg,
    int maxReg
)
{
    int reg = dfltReg;
    for (int retry = 0; retry < 5; retry++)
    {
        // PickOperand() updates m_GS.InReg[].Reg
        int tmp = PickOperand(pOp);
        if (tmp >= minReg && tmp <= maxReg)
        {
            reg = tmp;
            break;
        }
    }
    pOp->InReg[pOp->LwrInReg].Reg = reg;
    pOp->InReg[pOp->LwrInReg].Neg =  PickNeg(); // <operandNeg>
    pOp->InReg[pOp->LwrInReg].Abs = (PickAbs() != 0);
    InstOperand(pOp, rndNoPick);
}

//-----------------------------------------------------------------------------
void GpuPgmGen::InstOperand(PgmInstruction *pOp, int Pick)
{
    // Grammar spec:
    // <instOperandV>   ::= <instOperandAbsV>
    //                  | <instOperandBaseV>
    // <instOperandAbsV>    ::= <operandAbsNeg> "|" <instOperandBaseV> "|"
    // <instOperandBaseV>   ::= <operandNeg> <attribUseV>
    //                      | <operandNeg> <tempUseV>
    //                      | <operandNeg> <paramUseV>
    //                      | <operandNeg> <bufferUseV>
    // <attribUseV>         ::= <attribBasic> <swizzleSuffix>
    //                      | <attribVarName> <swizzleSuffix>
    //                      | <attribVarName> <arrayMem> <swizzleSuffix>
    //                      | <attribColor> <swizzleSuffix>
    //                      | <attribColor> "." <colorType> <swizzleSuffix>
    //
    // <tempUseV>       ::= <tempVarName> <swizzleSuffix>
    // <paramUseV>      ::= <paramVarName> <optArrayMem> <swizzleSuffix>
    //                  | <stateSingleItem> <swizzleSuffix>
    //                  | <programSingleItem> <swizzleSuffix>
    //                  | <constantVector> <swizzleSuffix>
    //                  | <constantScalar>
    //
    // <bufferUseV>     ::= <bufferVarName> <optArrayMem> <swizzleSuffix>
    //
    // <instOperandS>   ::= <instOperandAbsS>
    //                  | <instOperandBaseS>
    // <instOperandAbsV>    ::= <operandAbsNeg> "|" <instOperandBaseS> "|"
    // <instOperandBaseS> ::= <operandNeg> <attribUseS>
    //                      | <operandNeg> <tempUseS>
    //                      | <operandNeg> <paramUseS>
    //                      | <operandNeg> <bufferUseS>
    // <attribUseS>       ::= <attribBasic> <scalarSuffix>
    //                      | <attribVarName> <scalarSuffix>
    //                      | <attribVarName> <arrayMem> <scalarSuffix>
    //                      | <attribColor> <scalarSuffix>
    //                      | <attribColor> "." <colorType> <scalarSuffix>
    //
    // <instOperandVNS>   ::= <attribUseVNS>
    //                      | <tempUseVNS>
    //                      | <paramUseVNS>
    //                      | <bufferUseVNS>
    //
    // <attribUseVNS>     ::= <attribBasic>
    //                      | <attribVarName>
    //                      | <attribVarName> <arrayMem>
    //                      | <attribColor>
    //                      | <attribColor> "." <colorType>
    // <tempUseVNS>       ::= <tempVarName>
    // <paramUseVNS>      ::= <paramVarName> <optArrayMem>
    //                      | <stateSingleItem>
    //                      | <programSingleItem>
    //                      | <constantVector>
    //                      | <constantScalar>
    // <bufferUseVNS>     ::= <bufferVarName> <optArrayMem>

    // <operandAbsNeg>      ::= <optSign>
    // <operandNeg>         ::= <optSign>

    // A temp variable to make sure the code doesn't exceed the 80 col.
    int idx = pOp->LwrInReg;

    if (Pick == rndPick)
    {
        // If the operand is a vector without suffix or scalar this implies
        // that we will be accessing all 4 components. So if we have picked
        // a temp register make sure all 4 components have been previously
        // written.

        int reg;
        const RegState * pReg;
        do
        {
            reg = PickOperand(pOp);
            pReg = &GetVar(reg);
        }
        while ((pOp->GetArgType(idx) & atVectNS) &&
                (pReg->Attrs & regTemp) &&
                (pReg->WrittenMask != 0xf));

        pOp->InReg[idx].Reg = reg;
        pOp->InReg[idx].Neg =  PickNeg(); // <operandNeg>
        pOp->InReg[idx].Abs = (PickAbs() != 0);
        FixRandomLiterals(pOp, idx);
   }

    const int reg = pOp->InReg[idx].Reg;
    const RegState & v = GetVar(reg);

    if (v.ArrayLen)
    {
        PickIndexing(&pOp->InReg[idx]);
    }

    const int argType = pOp->GetArgType(idx);

    // special processing for Tmp registers. We don't want to read from an
    // unwritten component, so we will pass in the WrittenMask as the
    // ReadableMask.
    UINT08 readableMask = (v.Attrs & regTemp) ? v.WrittenMask : v.ReadableMask;

    if (argType & (atVector + atVectF + atVectS + atVectU))
    {
        // Need to separate the Picking of swizzle suffix from the formatting.
        SwizzleSuffix(pOp,
                      pOp->InReg[idx].Swiz,
                      &pOp->InReg[idx].SwizCount,
                      readableMask);

    }
    else if (argType & atScalar)
    {
        ScalarSuffix(pOp, readableMask);
    }

    pOp->LwrInReg++;    // move on to the next operand
}

//-----------------------------------------------------------------------------
void GpuPgmGen::FixRandomLiterals(PgmInstruction *pOp, int idx)
{
    const int argType = pOp->GetArgType(idx);
    const int ConstVectReg = GetConstVectReg();
    const int LiteralReg   = GetLiteralReg();

    if (((pOp->InReg[idx].Reg == ConstVectReg) ||
         (pOp->InReg[idx].Reg == LiteralReg)) &&
        (pOp->InReg[idx].LiteralVal.m_Count == 0))
    {
        // This input is a literal that hasn't been filled in yet.
        // We must fill in valid random numbers for it.

        if (argType & atScalar)
        {
            pOp->InReg[idx].Reg = LiteralReg;
            pOp->InReg[idx].LiteralVal.m_Count = 1;
        }
        else
        {
            pOp->InReg[idx].Reg = ConstVectReg;
            pOp->InReg[idx].LiteralVal.m_Count = 4;
        }

        Literal * pLiteral = &pOp->InReg[idx].LiteralVal;
        int typeOfLiteral = pOp->srcDT == dtNone ? pOp->dstDT : pOp->srcDT;

        switch (argType)
        {
        default:  break; // use opcode result data type as above.

        case atVectF:  typeOfLiteral = dtF; break;
        case atVectS:  typeOfLiteral = dtS; break;
        case atVectU:  typeOfLiteral = dtU; break;
        }

        // Range of values is large enough to overflow 11-bit immediate fields
        // part of the time, so we get coverage of 32-bit immediate opcodes.

        switch (typeOfLiteral & (dtS|dtU|dtF))
        {
        default:
        case dtF:
            pLiteral->m_Type = Literal::f32;
            for (int i = 0; i < pLiteral->m_Count; i++)
                pLiteral->m_Data.m_f32[i] =
                    m_pFpCtx->Rand.GetRandomFloatExp(-8, 8);
            break;
        case dtS:
            pLiteral->m_Type = Literal::s32;
            for (int i = 0; i < pLiteral->m_Count; i++)
                pLiteral->m_Data.m_s32[i] = -2048 +
                    static_cast<INT32>(m_pFpCtx->Rand.GetRandom(0, 4095));
            break;
        case dtU:
            pLiteral->m_Type = Literal::u32;
            for (int i = 0; i < pLiteral->m_Count; i++)
                pLiteral->m_Data.m_u32[i] = m_pFpCtx->Rand.GetRandom(0, 4096);
            break;
        }
    }
}

//-----------------------------------------------------------------------------
void GpuPgmGen::PickIndexing(RegUse * pRegUse)
{
    const RegState & v = GetVar(pRegUse->Reg);

    if ((m_BnfExt >= extGpu4) && (PickRelAddr() != 0))
    {
        // Use this sort of syntax: regname[tmpA128.x + 33]
        // tmpSubs is only used with CALI, which doesn't call PickIndexing.
        pRegUse->IndexReg = (v.ArrayLen == TempArrayLen) ?
            GetIndexReg(tmpA1) : GetIndexReg(tmpA128);
        pRegUse->IndexOffset = m_pFpCtx->Rand.GetRandom(0, v.ArrayLen/2 - 1);
        pRegUse->IndexRegComp = m_pFpCtx->Rand.GetRandom(0,3);
        pRegUse->Neg = false;
    }
    else
    {
        // Use this sort of syntax: regname[3]
        pRegUse->IndexOffset = m_pFpCtx->Rand.GetRandom(0, v.ArrayLen-1);
    }
}

//-----------------------------------------------------------------------------
bool GpuPgmGen::TexAccess(PgmInstruction *pOp, int Access)
{
    // grammar spec:
    // <texAccess> ::= <texImageUnit> "," <texTarget>
    //              |  <texImageUnit> "," <texTarget> "," <texOffset>
    // <texImageUnit> ::= "texture" <optArrayMember>
    // <texTarget> ::= "1D" | "2D" | "3D" | "LWBE" | "SHADOW1D"
    //              |  "SHADOW2D"  | "ARRAY1D" | "ARRAY2D"
    //              |  "SHADOWLWBE"|SHADOWARRAY1D" | "SHADOWARRAY1D"
    //              |  "RENDERBUFFER"

    // TODO: There are lots of additional rules that should be implemented to
    // insure consistent results
    // 1. Single texel fetches (TXF) can not perform depth comparisons or access
    //    lwbemaps.
    // 2. Single-texel fetches do not support LOD clamping or any texture wrap
    //    mode, and require a mipmapped minification filter to access any level
    //    of detail other than the base level. The results of a texel fetch are
    //    undefined:
    //    -If the computed LOD is less than the textures's base level are
    //     greater than the maximum level
    //    -If the computed LOD is not the texture's base level and the texture's
    //     minification filter is NEAREST or LINEAR.
    //    -If the layer specified for array textures is negative or greater than
    //     the number of layres in the array texture.
    //    -If the texel at (i,j,k) coordinates refer to a border texel outside
    //     the defined extents of the specified LOD, where
    //      i < -b_s, j < -b_s, k < -b_s,
    //      i >= w_s - b_s, j >= H_s - b_s, or k >= d_s -b_s.(see LW_gpu_program4)
    //    -If the texture being accessed is not complete (or lwbe complete for
    //     lwbemaps).
    //
    bool TargetIsGood;
    GLint count = 0;   // prevent endless loop.
    for (TargetIsGood = false;
         !TargetIsGood && count < GetNumTxFetchers();
         count++ )
    {
        pOp->TxFetcher = PickTxFetcher(Access);
        if (pOp->TxFetcher == -1)
        {
            continue;
        }

        if (m_GS.Prog.PgmRqmt.RsvdTxf.count(pOp->TxFetcher))
        {
            // we have to use the same texture target
            pOp->TxAttr = m_GS.Prog.PgmRqmt.RsvdTxf[pOp->TxFetcher].Attrs;
        }
        else if (m_GS.Prog.PgmRqmt.UsedTxf.count(pOp->TxFetcher))
        {
            // we have to use the same texture target
            pOp->TxAttr = m_GS.Prog.PgmRqmt.UsedTxf[pOp->TxFetcher].Attrs;
        }
        else
        {   // get a new texture attribute for this TxFetcher
            pOp->TxAttr = PickTxTarget();
            // Too many restrictions for shadow targets, fall back to more
            // generic type.
            if (IsBindlessTextures())
            {
                switch (pOp->TxAttr)
                {
                    case IsShadow1d       : pOp->TxAttr = Is1d; break;
                    case IsShadow2d       : pOp->TxAttr = Is3d; break;
                    case IsShadowLwbe     : pOp->TxAttr = IsLwbe; break;
                    case IsShadowArray1d  : pOp->TxAttr = IsArray1d; break;
                    case IsShadowArray2d  : pOp->TxAttr = IsArray2d; break;
                    case IsShadowArrayLwbe: pOp->TxAttr = IsArrayLwbe; break;
                    // 2dMultisample have to be the same size, boring!
                    case Is2dMultisample  : pOp->TxAttr = Is2d; break;
                    default: break;
                }
            }
            // fallback if target is not supported
            if ((pOp->TxAttr == IsShadowArrayLwbe )  &&
              !m_pGLRandom->HasExt(GLRandomTest::ExtARB_texture_lwbe_map_array))
            {
                pOp->TxAttr = IsShadowLwbe;
            }
        }
        // make sure the current op can access this type of target.
        TargetIsGood = IsTexTargetValid(pOp->OpId,pOp->TxFetcher, pOp->TxAttr);
    }

    if (TargetIsGood)
    {
        // Ok the fetcher and target are valid, finish off the instruction.
        int txOffset = (m_BnfExt < extGpu4) ? toNone :PickTxOffset();
        TexOffset(pOp, txOffset);
    }
    else
    {
        pOp->TxFetcher = -1;
    }
    return TargetIsGood;
}

//----------------------------------------------------------------------------
// Apply the optional texture offsets to the instruction.
void GpuPgmGen::TexOffset(PgmInstruction *pOp, int toFmt)
{
    bool bHandled = true;
    INT32 offset=0, maxOffset;
    if ( pOp->OpId == RND_GPU_PROG_OPCODE_txgo ||
         pOp->OpId == RND_GPU_PROG_OPCODE_txq ||
         pOp->OpId == RND_GPU_PROG_OPCODE_loadim ||
         pOp->OpId == RND_GPU_PROG_OPCODE_storeim ||
         pOp->OpId == RND_GPU_PROG_FR_OPCODE_atomim ||
         toFmt == toNone
        )
    {
        // TXGO doesn't support texture offsets because they are already in the
        // operands.
        return;
    }
    else
    {
        if (m_GS.MinTexOffset < 0)
            offset = abs(m_GS.MinTexOffset);
        maxOffset = m_GS.MaxTexOffset + offset;
    }

    pOp->TxOffsetFmt = toFmt;
    switch (pOp->TxAttr & SASMDimFlags)
    {
        // required z component, fallthrough
        case Is3d:
            pOp->TxOffset[2] = m_pFpCtx->Rand.GetRandom(0,maxOffset) - offset;
        // requires y component, fallthrough
        case Is2d:
        case IsArray2d:
        case IsShadow2d:
        case IsShadowArray2d:
            pOp->TxOffset[1] = m_pFpCtx->Rand.GetRandom(0,maxOffset) - offset;
        // requires x component, fallthrough
        case Is1d:
        case IsArray1d:
        case IsShadow1d:
        case IsShadowArray1d:
            pOp->TxOffset[0] = m_pFpCtx->Rand.GetRandom(0,maxOffset) - offset;
            break;

        // LwbeMap and buffered targets don't support offsets.
        case IsLwbe:
        case IsShadowLwbe:
        case IsArrayLwbe:
        case IsShadowArrayLwbe:
        case Is2dMultisample:
            bHandled = false;
            break;
    }

    if (!bHandled)
    {
        pOp->TxOffsetFmt = toNone;
    }
    else
    {
        // TXD only supports the constant offset version.
        // Only LW_gpu_program5 support programmable offsets.
        if (toFmt == toProgrammable &&
           ((pOp->OpId == RND_GPU_PROG_OPCODE_txd) ||
             !m_pGLRandom->HasExt(extGpu5)))
        {
            pOp->TxOffsetFmt = toConstant; // fall back to constant version.
        }

        if (pOp->TxOffsetFmt == toProgrammable)
        {
            // Use new non-constant grammar for texture offsets.
            pOp->TxOffsetReg = PickTempReg();
            UtilCopyConstVect(pOp->TxOffsetReg, pOp->TxOffset);
        }
    }
}
//----------------------------------------------------------------------------
// Make sure the image unit's r/w access and data type match this op.
bool GpuPgmGen::CheckImageUnitRequirements
(
    PgmInstruction *pOp,
    ImageUnit& rImg,
    int Access          // desired r/w access
)
{
    bool bHandled = false;

    if ( (rImg.access != Access))
        return false;

    // check data format.
    switch (rImg.elems)
    {
        case 4:
            bHandled = ((pOp->dstDT & dtX4) == dtX4);
            break;
        case 2:
            bHandled = ((pOp->dstDT & dtX2) == dtX2);
            break;
        case 1:
            bHandled = ((pOp->dstDT & (dtX2|dtX4)) == 0);
            break;
        default:
            break;
    }

    if (bHandled)
        pOp->IU = rImg;

    return bHandled;
}

//----------------------------------------------------------------------------
// Pick the first available image unit.
bool GpuPgmGen::PickImageUnit(PgmInstruction *pOp, int Access)
{
    bool bHandled = true;
    int txf = pOp->TxFetcher;
    // find the first image unit that is available.
    GLRandom::PgmRequirements::TxfMap & rsvd = m_GS.Prog.PgmRqmt.RsvdTxf;
    GLRandom::PgmRequirements::TxfMap & used = m_GS.Prog.PgmRqmt.UsedTxf;
    
    if (IsImageUnitUsed(rsvd, txf))
    {   // check the downstream programs
        ImageUnit& rImg = m_GS.Prog.PgmRqmt.RsvdTxf[txf].IU;
        bHandled = CheckImageUnitRequirements(pOp,rImg,Access);
    }
    else if (IsImageUnitUsed(used, txf))
    {   // check this program
        ImageUnit& rImg = m_GS.Prog.PgmRqmt.UsedTxf[txf].IU;
        bHandled = CheckImageUnitRequirements(pOp,rImg,Access);
    }
    else
    {
        INT32 img = -1;
        // This fetcher doesn't have an Image Unit associated with it.
        // So find one that hasn't been used.
        PgmRequirements::TxfMapItr iter;
        for (iter = m_GS.Prog.PgmRqmt.RsvdTxf.begin();
             iter != m_GS.Prog.PgmRqmt.RsvdTxf.end();
             iter++)
        {
            img = max<INT32>(img, iter->second.IU.unit);
        }
        for (iter = m_GS.Prog.PgmRqmt.UsedTxf.begin();
             iter != m_GS.Prog.PgmRqmt.UsedTxf.end();
             iter++)
        {
            img = max<INT32>(img, iter->second.IU.unit);
        }
        img++;
        if (img < MaxImageUnits)
        {
            pOp->IU.unit = img;
            pOp->IU.access = Access;
            if (pOp->dstDT & dtX4)
                pOp->IU.elems = 4;
            else if(pOp->dstDT & dtX2)
                pOp->IU.elems = 2;
            else
                pOp->IU.elems = 1;

            // WARNING: Be conservative about this number, increasing it
            // requires RndTexturing::Pick() to consume more memory.
            pOp->IU.mmLevel = m_pFpCtx->Rand.GetRandom(0,4);
        }
        else
            bHandled = false;
    }

    return bHandled;
}
//------------------------------------------------------------------------------
// Filter texture targets to use a fallback strategy that is more balanced based
// on the initial target and the extension that is supported.
int GpuPgmGen::FilterTexTarget(int target) const
{
    if (m_BnfExt < extGpu4)
    {
        // no support for shadow or multisample targets
        if (target & (IsShadowMask | Is2dMultisample))
        {
            return(Is1d);
        }
        // no support for array targets (shadowArray covered above)
        if (target & (ADimFlags | Is3d))
        {
            return(Is2d);
        }
    }
    return (target);
}
//-----------------------------------------------------------------------------
// Here is a nice table that I got from Eric Werness. I reformated it to
// correspond to the OpId.
// numComponents is 1 for normal tex instructions (1 source vector),
// 2 for the new 2R in GPU4_1 (2 source vector) and 3 for TXD. 0 indicates an
// illegal combination
//static const int numComponents[numTexInsts][numTexTargets] = {
// Inst    1D, 2D, 3D, C, R, S1D, S2D, SC, SR, A1, A2, AC, SA1, SA2, SAC, B, RB       OpId
// LOD    { 1,  1,  1, 1, 0,   1,   1,  1,  0,  1,  1,  1,   1,   1,   1, 0,  0 },    _lod
// TEX    { 1,  1,  1, 1, 1,   1,   1,  1,  1,  1,  1,  1,   1,   1,   0, 0,  0 },    _tex
// TEX    { 0,  0,  0, 0, 0,   0,   0,  0,  0,  0,  0,  0,   0,   0,   2, 0,  0 },    _tex2
// TXB    { 1,  1,  1, 1, 1,   1,   1,  0,  1,  1,  1,  0,   1,   0,   0, 0,  0 },    _txb
// TXB    { 0,  0,  0, 0, 0,   0,   0,  0,  0,  0,  0,  2,   0,   0,   0, 0,  0 },    _txb2
// TXD    { 3,  3,  3, 3, 3,   3,   3,  3,  3,  3,  3,  3,   3,   3,   0, 0,  0 },    _txd
// TXF    { 1,  1,  1, 0, 1,   0,   0,  0,  0,  1,  1,  0,   0,   0,   0, 1,  0 },    _txf
// TXFMS  { 0,  0,  0, 0, 0,   0,   0,  0,  0,  0,  0,  0,   0,   0,   0, 0,  1 },    _txfms
// TXG_41 { 0,  1,  0, 1, 0,   0,   0,  0,  0,  0,  1,  1,   0,   0,   0, 0,  0 },    _txg
// TXG_5  { 0,  1,  0, 1, 0,   0,   1,  1,  0,  0,  1,  1,   0,   1,   2, 0,  0 },    not implemented
// TXL    { 1,  1,  1, 1, 1,   1,   1,  0,  1,  1,  1,  0,   1,   0,   0, 0,  0 },    _txl
// TXL    { 0,  0,  0, 0, 0,   0,   0,  0,  0,  0,  0,  2,   0,   0,   0, 0,  0 },    _txl2
// TXP    { 1,  1,  1, 1, 1,   1,   1,  0,  1,  1,  1,  0,   1,   0,   0, 0,  0 },    _txp
// TXQ    { 1,  1,  1, 1, 1,   1,   1,  1,  1,  1,  1,  1,   1,   1,   1, 1,  0 },    _txq
// TXGO   { 0,  3,  0, 0, 3,   0,   3,  0,  3,  0,  3,  0,   0,   3,   0, 0,  0 },    _txgo
bool GpuPgmGen::IsTexTargetValid( int OpId, int TxFetcher,int TxAttr)
{
    bool validTarget = true;

    // LW_gpu_program4_1 introduced the ability to access lwbemap arrays. So if
    // this extension is not supported ilwalidate that target for all opcodes.
    if (!m_pGLRandom->HasExt(extGpu4_1))
    {
        validTarget = (TxAttr & ~(IsShadowArrayLwbe |
                                  IsArrayLwbe)) != 0;
    }

    if (validTarget)
    {
        // Now check opcode specific targets.
        switch (OpId)
        {
            case RND_GPU_PROG_OPCODE_tex:
                validTarget = !(TxAttr & (Is2dMultisample |
                                          IsShadowArrayLwbe));
                break;
            case RND_GPU_PROG_OPCODE_txd:
                validTarget = !(TxAttr & (Is2dMultisample |
                                          IsShadowArrayLwbe));
                break;
            case RND_GPU_PROG_OPCODE_txb:
            case RND_GPU_PROG_OPCODE_txl:
                validTarget = !(TxAttr & (Is2dMultisample |
                                          IsArrayLwbe     |
                                          IsShadowLwbe    |
                                          IsShadowArray2d |
                                          IsShadowArrayLwbe));
                break;
            case RND_GPU_PROG_OPCODE_txp:
                validTarget = !(TxAttr & (Is2dMultisample |
                                          IsArrayLwbe     |
                                          IsShadowLwbe    |
                                          IsShadowArray2d |
                                          IsShadowArrayLwbe));
                break;
            case RND_GPU_PROG_OPCODE_txf:
                validTarget = (TxAttr & (Is1d |
                                         Is2d |
                                         Is3d |
                                         IsArray1d |
                                         IsArray2d )) != 0;
                break;
            case RND_GPU_PROG_OPCODE_txg:
                validTarget = (TxAttr & (Is2d |
                                          IsLwbe |
                                          IsArray2d |
                                          IsArrayLwbe)) != 0;
                break;

            case RND_GPU_PROG_OPCODE_txfms:
                validTarget = (TxAttr & Is2dMultisample) &&
                              TxFetcher > 3 && TxFetcher < 8;
                break;

            case RND_GPU_PROG_OPCODE_tex2:
                validTarget = (TxAttr & IsShadowArrayLwbe) != 0;
                break;

            case RND_GPU_PROG_OPCODE_txb2:
            case RND_GPU_PROG_OPCODE_txl2:
                validTarget = (TxAttr & IsArrayLwbe) != 0;
                break;

            case RND_GPU_PROG_OPCODE_txq:
                validTarget = !(TxAttr & Is2dMultisample);
                break;

            case RND_GPU_PROG_OPCODE_txgo:
                validTarget = (TxAttr & (Is2d |
                                         IsShadow2d |
                                         IsArray2d  |
                                         IsShadowArray2d)) != 0;
                break;

            // No shadow or multisample targets
            case RND_GPU_PROG_FR_OPCODE_atomim:
            case RND_GPU_PROG_OPCODE_loadim:
            case RND_GPU_PROG_OPCODE_storeim:
                validTarget = !(TxAttr & (IsShadowMask |Is2dMultisample));
                break;
        }
    }
    return validTarget;
}

//-----------------------------------------------------------------------------
bool GpuPgmGen::BRAop_instruction(PgmInstruction *pOp)
{
    // grammar spec.
    // <BRAop_instruction ::= <BRAop> <opModifiers> <instTarget> <optBranchCond>
    // <BRAop> ::= "CAL"
    // <optBranchCond> ::= /* empty */
    //                   | <ccMask>

    const UINT32 numSubs = static_cast<UINT32>(m_GS.Subroutines.size());

    if (0 == numSubs || pOp->ForceRqdWrite)
    {
        return false;
    }

    // Have all subroutines been called at least once already?

    bool allSubsCalled = true;
    for (UINT32 subIdx = 0; subIdx < numSubs; subIdx++)
    {
        if (! m_GS.Subroutines[subIdx]->m_IsCalled)
            allSubsCalled = false;
    }

    // Pick which subroutine to call.
    // If some subs haven't been called yet, be sure to pick one of the
    // uncalled ones this time.

    UINT32 subToCall = m_pFpCtx->Rand.GetRandom(0, numSubs-1);
    while (m_GS.Subroutines[subToCall]->m_IsCalled && !allSubsCalled)
    {
        subToCall = (subToCall + 1) % numSubs;
    }

    Subroutine * pSub = m_GS.Subroutines[subToCall];

    if (! pSub->m_IsCalled)
    {
        // On first call, generate the statements for the Subroutine's Block.

        m_GS.Blocks.push( &(pSub->m_Block) );
        StatementSequence();
        pSub->m_Block.AddStatement("RET;");
        m_GS.Blocks.pop();
        pSub->m_IsCalled = true;
    }

    // Generate the actual call statement from the main block.

    // "CAL.modifiers SubN (opt-condition);"
    OpModifiers(pOp);

    // WAR489105: backdoor data used in SwizzleSuffix called by CCMask.
    m_GS.LwrSub = subToCall;
    bool maskUsed = CCMask(pOp,rndPick);

    // Update current state of calling block (i.e. main, probably)
    // with any work done by the subroutine.
    m_GS.Blocks.top()->IncorporateInnerBlock (
            pSub->m_Block,
            maskUsed ? Block::CalledMaybe : Block::CalledForSure);

    if ( m_GS.BranchOpsLeft)
        m_GS.BranchOpsLeft--;

    return true; // handled
}

//-----------------------------------------------------------------------------
bool GpuPgmGen::CALIop_instruction(PgmInstruction *pOp)
{
    // grammar spec.
    // <CALI_instruction> ::= <CALIop> <opModifiers> <subroutineVarName>
    //                        <optArrayMem> <optBranchCond>
    // <optBranchCond> ::= /* empty */
    //                   | <ccMask>
    //
    // Dolwmented in GL_ARB_shader_subroutine.

    const int numCaliSubs = m_GS.Prog.SubArrayLen;
    MASSERT(numCaliSubs);
    MASSERT(!pOp->ForceRqdWrite);

    // On first CALI of a RND_GPU_PROG_TEMPLATE_CallIndexed program, generate
    // all subroutines and mark them all as "called".

    for (int subIdx = 0; subIdx < numCaliSubs; subIdx++)
    {
        Subroutine * pSub = m_GS.Subroutines[subIdx];
        if (!pSub->m_IsCalled)
        {
            m_GS.Blocks.push( &(pSub->m_Block) );
            StatementSequence();
            pSub->m_Block.AddStatement("RET;");
            m_GS.Blocks.pop();
            pSub->m_IsCalled = true;
        }
    }

    // Generate the actual call statement from the main block.
    // "CALI Subs[tmpASubs.x] (opt-condition);"
    pOp->InReg[0].Reg = GetSubsReg();
    pOp->InReg[0].IndexReg = GetIndexReg(tmpASubs);
    pOp->InReg[0].IndexRegComp = m_pFpCtx->Rand.GetRandom(0,3);

    pOp->LwrInReg++;

    // WAR489105: backdoor data used in SwizzleSuffix called by CCMask.
    CCMask(pOp,rndPick);

    // WAR 744350: CALI Subs[0](FL0.x)
    // Predicate-test FALSE on CALI is mishandled by GL driver.
    if (pOp->OutTest == RND_GPU_PROG_OUT_TEST_fl)
        pOp->OutTest = RND_GPU_PROG_OUT_TEST_none;

    // Update current state of calling block (i.e. main, probably)
    // with any work done by the subroutine.
    for (int subIdx = 0; subIdx < numCaliSubs; subIdx++)
    {
        Subroutine * pSub = m_GS.Subroutines[subIdx];
        m_GS.Blocks.top()->IncorporateInnerBlock (pSub->m_Block,
                Block::CalledMaybe);
    }

    if ( m_GS.BranchOpsLeft)
        m_GS.BranchOpsLeft--;

    return true; // handled
}

//-----------------------------------------------------------------------------
bool GpuPgmGen::FLOWCCop_instruction(PgmInstruction *pOp)
{
    return false;
}

//-----------------------------------------------------------------------------
bool GpuPgmGen::IFop_instruction(PgmInstruction *pOp)
{
    // Grammar spec:
    // <IFop_instruction> ::= <IFop> <opModifiers> <ccTest>
    // <IFop> ::= "IF"
    // <ccTest> ::= <ccMask> <swizzleSuffix>

    const bool isMainBlock = (m_GS.Blocks.top() == &m_GS.MainBlock);
    const int  numUnwritten = m_GS.MainBlock.NumRqdUnwritten();

    int opsLeft = m_GS.Blocks.top()->StatementsLeft() -
            (numUnwritten + m_GS.BranchOpsLeft + m_GS.MsOpsLeft);

    // Requirements for an IF block:
    //  - must be a Flow template
    //  - must be in Main
    //  - must have 3 instructions free
    //  - must have a valid CC mask to use
    if ((m_GS.PgmStyle == RND_GPU_PROG_TEMPLATE_Simple) ||
        (!isMainBlock) ||
        (pOp->ForceRqdWrite) ||
        (!m_GS.Blocks.top()->CCValidMask()))
    {
        return false;
    }

    // Finish out the IF statement:

    OpModifiers(pOp);
    CCTest(pOp, rndPickRequired, ccMaskAll);
    CommitInstText(pOp);

    // Use a temporary Block object for the IF statements:

    Block tmpIfBlock(m_GS.Blocks.top());
    tmpIfBlock.SetTargetStatementCount(m_pFpCtx->Rand.GetRandom(1, max(1, opsLeft)));

    m_GS.Blocks.push(&tmpIfBlock);
    StatementSequence();
    m_GS.Blocks.pop();

    m_GS.Blocks.top()->AddStatements (tmpIfBlock);
    opsLeft -= tmpIfBlock.StatementCount();

    // Now possibly an ELSE:
    if (m_pFpCtx->Rand.GetRandom(0, 100) > 40)
    {
        pOp->Init(&m_GS,RND_GPU_PROG_OPCODE_else);
        CommitInstText(pOp);

        Block tmpElseBlock(m_GS.Blocks.top());
        tmpElseBlock.SetTargetStatementCount(m_pFpCtx->Rand.GetRandom(1, max(1, opsLeft)));

        m_GS.Blocks.push(&tmpElseBlock);
        StatementSequence();
        m_GS.Blocks.pop();
        m_GS.Blocks.top()->AddStatements (tmpElseBlock);

        m_GS.Blocks.top()->IncorporateIfElseBlocks (tmpIfBlock, tmpElseBlock);
    }
    else
    {
        m_GS.Blocks.top()->IncorporateInnerBlock(tmpIfBlock, Block::CalledMaybe);
    }

    // Setup the return state for the ENDIF instr.

    pOp->Init(&m_GS,RND_GPU_PROG_OPCODE_endif);

    return true;
}

//-----------------------------------------------------------------------------
bool GpuPgmGen::REPop_instruction(PgmInstruction *pOp)
{
    // Grammar spec.
    // <REPop_instruction> ::= <REPop> <opModifiers> <instOperandV>
    // <REPop> ::= "REP"

    const bool isMainBlock  = (m_GS.Blocks.top() == &m_GS.MainBlock);
    const int  numUnwritten = m_GS.MainBlock.NumRqdUnwritten();

    int opsLeft = m_GS.Blocks.top()->StatementsLeft() -
            (numUnwritten + m_GS.BranchOpsLeft + m_GS.MsOpsLeft);

    // Requirements for a REP block:
    //  - must be a Flow template
    //  - must be in Main
    //  - must have 4 instructions free
    if ((m_GS.PgmStyle == RND_GPU_PROG_TEMPLATE_Simple) ||
        (!isMainBlock) ||
        (opsLeft < 4) ||
        pOp->ForceRqdWrite)
    {
        return false;
    }

    const int reg = PickTempReg();

    // Setup the loop count and find a temp variable to use and initialize
    // it with a reasonable value.

    // Add a utility MOV instruction to set up the rep-count.
    // pgmElw[relativeTxReg].x is a random whole number between 0 and
    // numTxCoords/2.
    PgmInstruction movInst;
    movInst.Init(&m_GS, RND_GPU_PROG_OPCODE_mov);
    movInst.dstDT               = dtF32;            // MOV.F
    movInst.OutReg.Reg          = reg;
    movInst.OutMask             = compX;
    movInst.InReg[0].Reg        = GetConstReg();    //pgmElw[]
    movInst.InReg[0].IndexOffset= RelativeTxReg;
    movInst.InReg[0].SwizCount  = 1;
    movInst.LwrInReg            = 1;
    if (CommitInstruction(&movInst) == GpuPgmGen::instrValid)
    {
        CommitInstText(&movInst);
    }
    else
    {
        MASSERT(!"Malformed MOV instr!");
    }

    // Add the REP instruction.
    const RegState & v = GetVar(reg);
    pOp->InReg[0].Reg = reg;
    pOp->InReg[0].SwizCount = 1;
    pOp->InReg[0].Swiz[siX] = scX;  // compX
    pOp->LwrInReg = 1;
    // WAR611403 for OCG compiler bug.
    if (v.Bits == 64)
    {
        pOp->dstDT = dtF64;
    }
    else
    {
        pOp->dstDT = dtF32;
    }

    if(CommitInstruction(pOp) == GpuPgmGen::instrValid)
    {
        CommitInstText(pOp);
    }
    else
    {
        MASSERT(!"Malformed REP instr!");
    }

    // Use a temporary Block object for the looped statements.
    // Up to 5 statements, but no more than opsLeft minus the MOV, REP, ENDREP.

    Block tmpRepBlock(m_GS.Blocks.top());
    tmpRepBlock.SetTargetStatementCount(
            m_pFpCtx->Rand.GetRandom(1, min(5, opsLeft - 3)));

    m_GS.Blocks.push(&tmpRepBlock);
    StatementSequence();
    m_GS.Blocks.pop();

    // Use CalledMaybe because the pgmElw var might be 0.
    m_GS.Blocks.top()->AddStatements (tmpRepBlock);
    m_GS.Blocks.top()->IncorporateInnerBlock (tmpRepBlock, Block::CalledMaybe);

    // Setup the return state for the ENDREP instruction.
    pOp->Init(&m_GS,RND_GPU_PROG_OPCODE_endrep);

    return true;
}

bool GpuPgmGen::MEMBARop_instruction(PgmInstruction *pOp)
{
    // no modifiers, results, or operands
    return true;
}

//-----------------------------------------------------------------------------
bool GpuPgmGen::IMAGEop_instruction(PgmInstruction *pOp)
{
    // Trying to support image units with bindless textures is not worth
    // the added complexity.
    if (IsBindlessTextures())
        return false;

    bool bHandled = true;
    // These instructions require the texture coordinate values be
    // non-normalized integer values that do not exceed the size of the texture
    // image. We have two choices
    // a) Setup a complicated mechanism to communicate to RndVertexes that we
    //    need texture coordinates that don't exceed the image size of the
    //    MipMap level we are accessing.
    // b) Create our own coordinate value that is scaled to the size of the
    //    current MIPMAP on this texture image.
    //    Like this:
    //      TXQ R0, 1, texture[x], 2D;    // query its size
    //      I2F R0.xyz, R0;               // colwert to int
    //      FRC R1, fragment.texcoord[x]; // normalize the texcoord[x] to 0..1
    //      MUL R0.xyz, R1, R0;           // scale texture coordinates
    //      FLR.U R0.xyz, R0;             // colwert scaled coordinates to int
    //      LOADIM.F32X4 result.color, R0, image[0], 2D;
    //  Going with option B.
    switch (pOp->OpId)
    {

        case RND_GPU_PROG_OPCODE_loadim:
            OpModifiers(pOp);
            InstResult(pOp);
            // we are going to use the picking logic in TexAccess() to get us a
            // fetcher and target needed for the TXQ instruction. Then try to
            // pick an Image Unit that can accomodate that target.
            bHandled = TexAccess(pOp, iuaRead);
            if (bHandled)
                bHandled = InstOperandTxCd(pOp);

            if (bHandled)
                bHandled = PickImageUnit(pOp, iuaRead);
            // need a little fine tuning.
            if (bHandled)
            {
                pOp->InReg[0].SwizCount = 0;
                pOp->InReg[0].Neg = false;
                if (CommitInstruction(pOp) == GpuPgmGen::instrValid)
                {
                    UtilScaleRegToImage(pOp);
                }
                else
                    bHandled = false;
            }
            break;

        case RND_GPU_PROG_OPCODE_storeim:
            // STOREIM.F32X4 image[0], R1, R0, 2D;
            // R1 is the data to store, R0 is the "int" holding the coordinates
            // to the bound image.
            OpModifiers(pOp);
            // Note: There is no result register!
            // Data to store.
            UtilOpInReg(pOp, PickTempReg());

            // we are going to use the picking logic in TexAccess() to get us a
            // fetcher (or handle) and target needed for the TXQ instruction.
            // Then try to pick an Image Unit that can accomodate that target.
            bHandled = TexAccess(pOp,iuaWrite);
            if (bHandled)
                bHandled = InstOperandTxCd(pOp);

            if (bHandled)
                bHandled = PickImageUnit(pOp, iuaWrite);

            if (bHandled)
            {
                // need a little fine tuning.
                pOp->InReg[1].SwizCount = 0;
                pOp->InReg[1].Neg = false;
                pOp->InReg[1].Abs = false;

                if (CommitInstruction(pOp) == GpuPgmGen::instrValid)
                {
                    UtilScaleRegToImage(pOp);
                    // At this point
                    // TmpR0 = data
                    // TmpR1 = tex coords.
                    // Make the data a function of the tex coords.
                    // MAX R0, R1, {16057.0f},R1.x;
                    const int TmpR1 = pOp->InReg[1].Reg;
                    int TmpR0 = pOp->InReg[0].Reg;
                    if( TmpR0 == TmpR1)
                    {   // Pick a new data register
                        TmpR0 = TmpR1 > GetFirstTempReg() ? (TmpR1-1):(TmpR1+1);
                        pOp->InReg[0].Reg = TmpR0;
                    }

                    PgmInstruction Mad;
                    UtilOpDst(&Mad,RND_GPU_PROG_OPCODE_mad,TmpR0,compXYZW,dtNone);
                    UtilOpInReg(&Mad,TmpR1,scX,scY,scX,scY);
                    UtilOpInConstU32(&Mad, 16057); // a large prime number
                    UtilOpInReg(&Mad,TmpR1,scX);
                    CommitInstText(&Mad,"Make data a function of texcoords");
                }
                else
                    bHandled = false;
            }
            break;

        default:
            MASSERT(!"Unknown IMAGEop_instruction!");
            return false;
    }

    return bHandled;
}

void GpuPgmGen::ExtendedSwizzle(PgmInstruction *pOp)
{
    // grammar spec.
    // <extendedSwizzle> ::= <extSwizComp> "," <extSwizComp> "," <extSwizComp>
    //                       "," <extSwizComp>
    // <extSwizComp>     ::= <optSign> <xyzwExtSwizSel>
    //                      |<optSign> <rgbaExtSwizSel>
    // <xyzwExtSwizSel>  ::= "0" | "1" | <xyzwComponent>
    // <rgbaExtSwizSel>  ::= <rgbaComponent>
    // <xyzwComponent>   ::= "x" | "y" | "z" | "w"
    for ( int i=0; i<4; i++)
    {
        pOp->ExtSwiz[i] = m_pFpCtx->Rand.GetRandom(0,5);
    }
    pOp->ExtSwizCount = 4;
}

//-----------------------------------------------------------------------------
int GpuPgmGen::PickMultisample() const
{
    int value = m_pFpCtx->Rand.GetRandom(0,100);
    if (!m_pGLRandom->HasExt(GLRandomTest::ExtLW_explicit_multisample))
        return 0;
    // return true 20% of the time
    return (value <= 20) ? 0:1;

}
//-----------------------------------------------------------------------------
int GpuPgmGen::PickTxOffset() const
{
    // return enabled 50% of the time
    if ( m_pFpCtx->Rand.GetRandom(0,1) == 1)
    {
        // pick one of two formats
        return (m_pFpCtx->Rand.GetRandom(0,1) + toConstant);
    }
    return 0;
}

//-----------------------------------------------------------------------------
bool GpuPgmGen::IsBindlessTextures() const
{
    return m_pGLRandom->m_GpuPrograms.UseBindlessTextures();
}

//-----------------------------------------------------------------------------
bool GpuPgmGen::IsLwrrGMProgPassThru() const
{
    return false;
}

//-----------------------------------------------------------------------------
// Atomic modifiers actually control the instruction type. To insure determinis-
// tic results we have to constrain the modifiers and STORE instr for each
// program.
// The ATOM instruction writes to a result register and has an implied STORE
// operation. Since we can't control the exelwtion order the result register
// must be discarded. However the table below takes into account the implied
// STORE operation.
// Here is a table with compatible modifiers that will produce deterministic
// results. The requirements here are that RndGpuPrograms will setup the
// pgmElw[] with complementry data masks for these operations.
// Instructions in a given row that have an 'x' can be used in the
// same program. If there is no 'x' for an instruction then it can only
// be used once.
// bit      11    10    9     8     7     6     5     4     3     2     1     0
//          Store CSwap Exch  Xor   Or    And   Dwrap Iwrap max   min   add  None
// add      --    --    --    --    --    --    --    --    --    --    x    x
// min      --    --    --    --    --    --    --    --    x     x     --   x
// max      --    --    --    --    --    --    --    --    x     x     --   x
// IWrap    --    --    --    --    --    --    x     x     --    --    --   x
// Dwrap    --    --    --    --    --    --    x     x     --    --    --   x
// And      --    --    --    x     x     x     --    --    --    --    --   x
// Or       --    --    --    x     x     x     --    --    --    --    --   x
// Xor      --    --    --    x     x     x     --    --    --    --    --   x
// Exch     --    --    x     --    --    --    --    --    --    --    --   x
// CSwap    --    --    --    --    --    --    --    --    --    --    --   x
// Store    x     --    --    --    --    --    --    --    --    --    --   x
// Return one of amNone-amNUM in the upper 16 bits.
int GpuPgmGen::PickAtomicOpModifier()
{
    MASSERT(m_AtomicModifiers.size() == amNUM);
    for(int i =0; i<amNUM; i++)
    {
        int opMod = m_AtomicModifiers[m_AmIdx++];
        m_AmIdx = m_AmIdx % m_AtomicModifiers.size();
        if( s_OpConstraints[opMod] & m_GS.ConstrainedOpsUsed)
        {
            m_GS.ConstrainedOpsUsed &= ~(1<<ocNone); //clear this bit once we have used a constraint
            m_GS.ConstrainedOpsUsed |= (1 << opMod); //set the specific constraint used
            return opMod << 16;
        }
    }
    // error condition!
    return amNone;
}
//-----------------------------------------------------------------------------
// Atomic ops aren't used too often and we want an even distribution of test
// coverage. Using a random picker  on a small amount of items does not yeild
// even coverage. So we put all of the atomic ops in a list and shuffle them
// up. Then use PickAtomOpModifier() to retrieve them.
void GpuPgmGen::ShuffleAtomicModifiers()
{
    int op = amAdd;
    for (unsigned i = 0; i < amNUM; i++, op++)
        m_AtomicModifiers[i] = op;
    m_AmIdx = 0;
    UINT32 size = (UINT32)m_AtomicModifiers.size();
    m_pFpCtx->Rand.Shuffle(size,&m_AtomicModifiers[0]);

}
//-----------------------------------------------------------------------------
// Get the proper pgmElw[] index for this atomic operation.

// This table maps an atomic modifier to a atomicModifierMaskId. The
// atomicModifierMaskId can be used to callwlate the proper pgmElw[] index and
// component swizzle.
struct AmmTranslate
{
    int dtSize;   // 32/64 bit
    int modifier;   //
    int maskId;     //
};
const struct AmmTranslate AmtTable[] =
{
    //start of 32 bit modifiers
    {4, amAdd,  ammAdd},
    {4, amMin,  ammMinMax},
    {4, amMax,  ammMinMax},
    {4, amIwrap,ammWrap},
    {4, amDwrap,ammWrap},
    {4, amAnd,  ammAnd},
    {4, amOr,   ammOr},
    {4, amXor,  ammXor},
    {4, amExch, ammExch},
    {4, amCSwap,ammCSwap},
    // start of 64 bit modifiers
    {8, amAdd,  0},  //.x
    {8, amMin,  3},  //.w
    {8, amMax,  3},  //.w
    {8, amAnd,  0},  //.x
    {8, amOr,   1},  //.y
    {8, amXor,  2},  //.z
    {8, amExch, 1},  //.y
    {8, amCSwap,1}   //.y
};

//-----------------------------------------------------------------------------
// Get the proper index into the pgmElw[] array for 32 bit atomics
int GpuPgmGen::GetAtomicIndex(PgmInstruction *pOp)
{
    if (pOp->dstDT & dt64)
    {
        Printf(Tee::PriHigh, "GetAtomicIndex() can't be called for 64bit instructions\n");
        //MASSERT(!"GetAtomicIndex() can't be called for 64bit instructions");
        return 0;
    }
    const int modifier = pOp->OpModifier >> 16;
    for (unsigned i=0; i < NUMELEMS(AmtTable); i++)
    {
        if (AmtTable[i].modifier == modifier)
        {
            // return the proper pgmElw[] index for this atomic instruction.
            return GpuPgmGen::AtomicMask1 + (AmtTable[i].maskId / 4);
        }
    }
    Printf(Tee::PriHigh,"Unknown atomic op modifier:0x%x\n",modifier);
    //MASSERT(!"Unknown atomic op modifier");
    return 0;
}
//----------------------------------------------------------------------------
// Get the proper component in pgmElw[] that holds this atomic operations mask
int GpuPgmGen::GetAtomicSwiz(PgmInstruction *pOp)
{
    MASSERT( (pOp->OpModifier >> 16) <= amNUM);
    const int modifier = pOp->OpModifier >> 16;

    for (unsigned i=0; i < NUMELEMS(AmtTable); i++)
    {
        if ((AmtTable[i].modifier == modifier) &&
            (GetDataTypeSize(pOp->dstDT) == AmtTable[i].dtSize))
        {
            return (AmtTable[i].maskId % 4);
        }
    }
    Printf(Tee::PriHigh,"GetAtomicSwizzle(), Unknown atomic op modifier:0x%x\n",modifier);
    //MASSERT(!"Unknown atomic op modifier");
    return 0;
}

//-----------------------------------------------------------------------------
int GpuPgmGen::GetAtomicOperand0Reg(PgmInstruction *pOp)
{
    const int modifier = pOp->OpModifier >> 16;
    if (pOp->dstDT & dt64)
    {
        switch (modifier)
        {
            case amAnd: // atom64.x
            case amOr:  // atom64.y
            case amXor: // atom64.z
            case amMin: // atom64.w
            case amMax: // atom64.w
            case amAdd: // atom64.x
            case amExch: // atom64.y
            case amCSwap: // atom64.y
                return RND_GPU_PROG_FR_REG_Atom64;

            case amIwrap: //not supported with 64 bit instr.
            case amDwrap: //not supported with 64 bit instr.
            case amNone:
                MASSERT(!"64bit atomic instr. not supported for this modifier");
                return RND_GPU_PROG_FR_REG_Const;
        }
    }
    return RND_GPU_PROG_FR_REG_Const;
}
//-----------------------------------------------------------------------------
int GpuPgmGen::PickOpModifier() const
{
    int modifier = smNone;

    switch (m_pFpCtx->Rand.GetRandom(0, 5))
    {
        case 0:
            modifier |= GpuPgmGen::smCC0;
            break;
        case 1:
            if (m_BnfExt >= extGpu4)
            {
                modifier |= GpuPgmGen::smCC1;
            }
            break;
        default:
            // 60% case: no condition-code update.
            break;
    }

    switch (m_pFpCtx->Rand.GetRandom(0, 9))
    {
        case 0:
            // clamp value to [0,1]
            modifier |= GpuPgmGen::smSS0;
            break;
        case 1:
            // clamp value to [-1,1]
            modifier |= GpuPgmGen::smSS1;
            break;
        default:
            // 90% case: no clamp
            break;
    }
    return modifier;
}

//-----------------------------------------------------------------------------
// Return the opDataType size in bytes.
int GpuPgmGen::GetDataTypeSize(opDataType dt)
{
    UINT32 size = 0;
    switch (dt & dtSizeMask)
    {
        case dt8:   size = 1; break;
        case dt16:  size = 2; break;
        case dt24:  size = 3; break;
        case dtHi32:size = 4; break;
        case dt32e: size = 4; break;
        case dt32:  size = 4; break;
        case dt64:  size = 8; break;
        default:    size = 0; break;
    }
    if (dt & dtX2)
    {
        size *= 2;
    }
    else if (dt & dtX4)
    {
        size *= 4;
    }

    return size;
}
//------------------------------------------------------------------------------
opDataType GpuPgmGen::PickOpDataType(PgmInstruction *pOp) const
{

    // Pick a random data-type instruction suffix.

    opDataType t = (opDataType)
            (*m_pPickers)[RND_GPU_PROG_OPCODE_DATA_TYPE].Pick();

    if ((t & dt64) && !m_pGLRandom->HasExt(extGpu5))
    {
        // For pre-GF100, replace 64 with 32.
        t = opDataType((t & ~dt64) | dt32);
    }
    if (t==dtF64 && !m_pGLRandom->HasExt(GLRandomTest::ExtLW_gpu_program_fp64))
    {
        // All instructions that support F64 also support F32.
        t = dtF32;
    }
    if (t & (dtS|dtU) && !m_pGLRandom->HasExt(extGpu4))
    {
        // integer data types are not supported
        t = dtF32;
    }
    // Force the random opDataType to be valid.
    // Some instruction have mutually exclusive data type masks that share the same bitfields.
    // So we need to process a list of possible types instead of a single bitmapped value.
    vector <opDataType> opMasks;
    pOp->GetTypeMasks(m_pGLRandom, &opMasks);
    for (UINT32 ii = 0; ii < opMasks.size(); ii++)
    {
        // the mask specifies that all mask bits must match data type bits
        if (opMasks[ii] & dta)
        {
            if (t == opMasks[ii])
            {
                return t;
            }
        }
        // data type if F16 and this instruction only allows F16 but no I16 or U16 types.
        else if ((t == dtF16) && (opMasks[ii] & dt16f))
        {
            return t;
        }
        // otherwise all data type bits can be a subset of the mask bits
        else if (t == (t & opMasks[ii]))
        {
            return t;
        }
    }

    // Couldn't find a mask that works with this type. So fallback.
    // Instruction datatype suffix isn't supported by this op.
    // Try to fall back intelligently.
    const opDataType fallbacks[][2] =
    {
        // unsupported  fallback
        { dtF16,        dtF32 },  // unsupported Fxx to F32
        { dtF64,        dtF32 },
        { dtF32,        dtS32 },  // unsupported F32 to S32
        { dtS8,         dtS32 },  // unsupported Sxx to S32
        { dtS16,        dtS32 },
        { dtS24,        dtS32 },
        { dtS32_HI,     dtS32 },
        { dtS64,        dtS32 },
        { dtS32,        dtF32 },  // unsupported S32 to F32
        { dtU8,         dtU32 },  // unsupported Uxx to U32
        { dtU16,        dtU32 },
        { dtU24,        dtU32 },
        { dtU32_HI,     dtU32 },
        { dtU64,        dtU32 },
        { dtU32,        dtF32 },  // unsupported U32 to F32
        { dtF16X2a,     dtF32 },
        { dtF16X4a,     dtF32 },
        { dtF32X2,      dtF32 },
        { dtF32X4,      dtF32 },
        { dtS32X2,      dtS32 },
        { dtS32X4,      dtS32 },
        { dtU32X2,      dtU32 },
        { dtU32X4,      dtU32 },
        { dtF64X2,      dtF64 },
        { dtF64X4,      dtF64 },
        { dtS64X2,      dtS64 },
        { dtS64X4,      dtS64 },
        { dtU64X2,      dtU64 },
        { dtU64X4,      dtU64 },
    };
    for (size_t i = 0; i < NUMELEMS(fallbacks); i++)
    {
        if (t == fallbacks[i][0])
        {
            t = fallbacks[i][1];
                break;  // Found a supported type.
        }
    }

    // Some ops require explicit 32bit data type declaration, but the
    // fallback algorithm will mask that out. So put it back before the
    // final check.
    for (UINT32 ii = 0; ii < opMasks.size(); ii++)
    {
        if ((opMasks[ii] & dt32e) && (t & dt32))
            t = (GLRandom::opDataType)((t & ~dt32) | dt32e);

        if (opMasks[ii] & dta)
        {
            if (t == opMasks[ii])
            {
                return t;
            }
        }
        else if (t == (t & opMasks[ii]))
        {
            return t;
        }
    }

    // Catchall case, for when fallbacks didn't work.
    // I.e. picker is misconfigured by user, or opMask is opNA, etc.
    return dtNA;
}

//-----------------------------------------------------------------------------
int GpuPgmGen::PickCCtoRead() const
{
    int cc = m_pFpCtx->Rand.GetRandom(RND_GPU_PROG_CC0,RND_GPU_PROG_CCnone);
    if ((m_BnfExt < extGpu4) && (cc == RND_GPU_PROG_CC1))
        cc = RND_GPU_PROG_CC0;

    if ( m_GS.Blocks.top()->CCValidMask() & (0xf << (cc*4)))
        return cc;
    else
        return RND_GPU_PROG_CCnone;

}

//-----------------------------------------------------------------------------
int GpuPgmGen::PickCCTest(PgmInstruction *pOp) const
{
    int ccTest = m_pFpCtx->Rand.GetRandom(RND_GPU_PROG_OUT_TEST_eq,
                                    RND_GPU_PROG_OUT_TEST_none);

    if (m_BnfExt < extGpu4 && ccTest != RND_GPU_PROG_OUT_TEST_none)
    {
        ccTest = ccTest % RND_GPU_PROG_OUT_TEST_nan;
    }
    return ccTest;
}

//-----------------------------------------------------------------------------
int GpuPgmGen::PickSwizzleSuffix() const
{
    return m_pFpCtx->Rand.GetRandom(RND_GPU_PROG_SWIZZLE_SUFFIX_none,
                                    RND_GPU_PROG_SWIZZLE_SUFFIX_all);
}

//-----------------------------------------------------------------------------
int GpuPgmGen::PickSwizzleOrder() const
{
    return m_pFpCtx->Rand.GetRandom(RND_GPU_PROG_SWIZZLE_ORDER_xyzw,
                                    RND_GPU_PROG_SWIZZLE_ORDER_random);
}

//-----------------------------------------------------------------------------
int GpuPgmGen::PickWriteMask() const
{
    int value = m_pFpCtx->Rand.GetRandom(0,80);
    // return all 4 components 80% of the time
    // and 0-4 components 20% of the time.
    return( min(15,value));
}

//-----------------------------------------------------------------------------
int GpuPgmGen::PickOperand(PgmInstruction *pOp)
{
// TODO: Add support for relative indexing and apply the rules associated
//      with using a const register with relative indexing.
//      Note: Any temp register can now be used as an index register. So we
//            need to identify how we intend to use it.
//            This logic is common to all programs.
//      Rules:
//      - Don't use with vp1.1 position-ilwariant.
//      - Don't use unless an index register has been written.
//      - Don't use if the register's value may be out of bounds.

    const int numVars = m_GS.MainBlock.NumVars();

    // Callwlate the "interestingness" score of the current register values.
    m_Scores.clear();
    if (m_Scores.size() < (UINT32)numVars)
        m_Scores.resize(numVars);

    UINT32 TotalScore = 0;

    int reg;
    for (reg = 0; reg < numVars; reg++)
    {
        UINT32 score = CalcScore(pOp, reg, pOp->LwrInReg);
        TotalScore += score;
        m_Scores[ reg ] = TotalScore;
    }

    MASSERT(TotalScore);  // There must be at least 1 valid input for this arg!

    // Pick the input reg, weighted by interest scores.
    UINT32 pick = m_pFpCtx->Rand.GetRandom(0, TotalScore-1);
    for (reg = 0; reg < m_GS.MainBlock.NumVars(); reg++)
    {
        if (pick < m_Scores[reg])
        {
            pOp->InReg[pOp->LwrInReg].Reg = reg;
            break;
        }
    }
    const RegState & v = GetVar(reg);

    // Don't return a register that has not been written.
    // If we assert here there is something wrong with our scoring!
    if(!v.WrittenMask)
    {
        MASSERT(!"Problem detected with scoring algorithm");
    }

    return reg;
}

//------------------------------------------------------------------------------
int GpuPgmGen::PickTxFetcher(int Access)
{
    int maxTxFetchers = GetNumTxFetchers();
    int txf = m_pFpCtx->Rand.GetRandom(0, maxTxFetchers - 1);
    int access;

    for ( int i = 0; i < maxTxFetchers; i++)
    {
        txf = (txf+i) % maxTxFetchers;
        if (m_GS.Prog.PgmRqmt.RsvdTxf.count(txf))
            access = m_GS.Prog.PgmRqmt.RsvdTxf[txf].IU.access;
        else if (m_GS.Prog.PgmRqmt.UsedTxf.count(txf))
            access = m_GS.Prog.PgmRqmt.UsedTxf[txf].IU.access;
        else
            access = Access;

        if( access == Access)
            return txf;
    }
    // error couldn't find a fetcher with requested access.
    return -1;
}

//-----------------------------------------------------------------------------
void GpuPgmGen::OpModifiers(PgmInstruction *pOp)
{
    // Grammar spec:
    // <opModifiers> ::= <opModifierItem> <opModifiers>
    // <opModifierItem> ::= "." <opModifier>
    // <opModifier>  ::= "F"    -data type
    //                  |"U"    -data type
    //                  |"S"    -data type
    //                  |"CC"   -condition code
    //                  |"CC0"  -condition code
    //                  |"CC1"  -condition code
    //                  |"SAT"  -saturation
    //                  |"SSAT" -saturation
    //                  |"NTC"  -special NoTypeChecking modifier
    //                  |"S24"  -special MUL modifier
    //                  |"U24"  -special MUL modifier
    //                  |"HI"   -special MUL modifier
    // We are implementing the stand-alone modifiers, not the suffix modifiers.
    // A program will fail to load if it contains an instruction that:
    // - specifies more than one modifier of any given type,
    // - specifies a clamping modifier on an instruction, unless it produces
    //     floating point reults, or
    // - specifies a modifier that is not supported by the instruction,
    //   see(s_GpuOpData4[] and similar OpData tables)
    // A program will be inconsistent if:
    // - The data type modifier is not consistent throughout the pipeline

    pOp->OpModifier = pOp->GetOpModMask() & PickOpModifier();

    if (pOp->OpId == RND_GPU_PROG_FR_OPCODE_atom ||
        pOp->OpId == RND_GPU_PROG_FR_OPCODE_atomim)
    {
        pOp->OpModifier |= PickAtomicOpModifier(); // ie. .ADD/.MIN/.IWRAP/etc
    }

    pOp->dstDT = PickOpDataType(pOp);
    if (dtNone == pOp->dstDT)
    {
        // Remember the implicit type the compiler will use w/o a suffix.
        pOp->dstDT = pOp->GetDefaultType();
    }

    // Pick a SAT modifier, only applies if we are writing a float
    if (!(pOp->dstDT & dtF))
        pOp->OpModifier &= ~smSS;

}

//-----------------------------------------------------------------------------
void GpuPgmGen::InstResult(PgmInstruction *pOp)
{
    // Grammar spec:
    // <instResult> ::= <instResultCC>
    //                | <instResultBase>
    // <instResultCC> ::= <instResultBase> <ccMask>

    InstResultBase(pOp);
    CCMask(pOp);
}

//-----------------------------------------------------------------------------
void GpuPgmGen::InstLiteral(PgmInstruction *pOp,
                    int reg,
                    int numComp,
                    INT32 v0,
                    INT32 v1,
                    INT32 v2,
                    INT32 v3)
{
    MASSERT(reg == GetConstVectReg() || reg == GetLiteralReg());
    const int idx = pOp->LwrInReg;
    pOp->InReg[idx].Reg = reg;
    pOp->InReg[idx].LiteralVal.m_Count = numComp;
    pOp->InReg[idx].LiteralVal.m_Type = GpuPgmGen::Literal::s32;
    pOp->InReg[idx].LiteralVal.m_Data.m_s32[0] = v0;
    pOp->InReg[idx].LiteralVal.m_Data.m_s32[1] = v1;
    pOp->InReg[idx].LiteralVal.m_Data.m_s32[2] = v2;
    pOp->InReg[idx].LiteralVal.m_Data.m_s32[3] = v3;
    pOp->LwrInReg++;
}

//-----------------------------------------------------------------------------
void GpuPgmGen::InstLiteral(PgmInstruction *pOp,
                    int reg,
                    int numComp,
                    UINT32 v0,
                    UINT32 v1,
                    UINT32 v2,
                    UINT32 v3)
{
    MASSERT(reg == GetConstVectReg() || reg == GetLiteralReg());
    const int idx = pOp->LwrInReg;
    pOp->InReg[idx].Reg = reg;
    pOp->InReg[idx].LiteralVal.m_Count = numComp;
    pOp->InReg[idx].LiteralVal.m_Type = GpuPgmGen::Literal::u32;
    pOp->InReg[idx].LiteralVal.m_Data.m_u32[0] = v0;
    pOp->InReg[idx].LiteralVal.m_Data.m_u32[1] = v1;
    pOp->InReg[idx].LiteralVal.m_Data.m_u32[2] = v2;
    pOp->InReg[idx].LiteralVal.m_Data.m_u32[3] = v3;
    pOp->LwrInReg++;
}

//-----------------------------------------------------------------------------
void GpuPgmGen::InstLiteral(PgmInstruction *pOp,
                    int reg,
                    int numComp,
                    float v0,
                    float v1,
                    float v2,
                    float v3)
{
    MASSERT(reg == GetConstVectReg() || reg == GetLiteralReg());
    const int idx = pOp->LwrInReg;
    pOp->InReg[idx].Reg = reg;
    pOp->InReg[idx].LiteralVal.m_Count = numComp;
    pOp->InReg[idx].LiteralVal.m_Type = GpuPgmGen::Literal::f32;
    pOp->InReg[idx].LiteralVal.m_Data.m_f32[0] = v0;
    pOp->InReg[idx].LiteralVal.m_Data.m_f32[1] = v1;
    pOp->InReg[idx].LiteralVal.m_Data.m_f32[2] = v2;
    pOp->InReg[idx].LiteralVal.m_Data.m_f32[3] = v3;
    pOp->LwrInReg++;
}

//-----------------------------------------------------------------------------
void GpuPgmGen::InstLiteral(PgmInstruction *pOp,
                    int reg,
                    int numComp,
                    double v0,
                    double v1,
                    double v2,
                    double v3)
{
    MASSERT( reg == GetConstVectReg() || reg == GetLiteralReg());
    const int idx = pOp->LwrInReg;
    pOp->InReg[idx].Reg = reg;
    pOp->InReg[idx].LiteralVal.m_Count = numComp;
    pOp->InReg[idx].LiteralVal.m_Type = GpuPgmGen::Literal::f64;
    pOp->InReg[idx].LiteralVal.m_Data.m_f64[0] = v0;
    pOp->InReg[idx].LiteralVal.m_Data.m_f64[1] = v1;
    pOp->InReg[idx].LiteralVal.m_Data.m_f64[2] = v2;
    pOp->InReg[idx].LiteralVal.m_Data.m_f64[3] = v3;
    pOp->LwrInReg++;
}

//-----------------------------------------------------------------------------
bool GpuPgmGen::CCMask(PgmInstruction *pOp, int Pick)
{
    // grammar spec:
    // <ccMask> ::= "(" <ccTest> ")"

    if (m_BnfExt < extGpu4 && pOp->OpModifier & (smCC0 | smCC1))
    {
        // From running in parseasm:
        // Bail out if the program conditionally writes a CC reg.  We
        // don't handle this case correctly, and the fix is too risky
        // for lwr55.  See bugs 100521 and 102069.
        pOp->CCtoRead = RND_GPU_PROG_CCnone;
        pOp->OutTest = RND_GPU_PROG_OUT_TEST_none;
        return false; // no mask was used
    }

    if (Pick == rndPick)
    {
        pOp->CCtoRead = pOp->ForceRqdWrite ?
                            RND_GPU_PROG_CCnone : PickCCtoRead();
        pOp->OutTest  = pOp->ForceRqdWrite ?
                            RND_GPU_PROG_OUT_TEST_none : PickCCTest(pOp);
    }

    if ((pOp->CCtoRead == RND_GPU_PROG_CC0 || pOp->CCtoRead == RND_GPU_PROG_CC1)
        && (pOp->OutTest != RND_GPU_PROG_OUT_TEST_none))
    {
        CCTest(pOp, rndNoPick);
        return true;    // a mask was used
    }
    else //insure the internal state is consistent for the rest of the generation.
    {
        pOp->OutTest = RND_GPU_PROG_OUT_TEST_none;
        pOp->CCtoRead = RND_GPU_PROG_CCnone;
    }
    return false; // no mask was used
}

//-----------------------------------------------------------------------------
// The CompMask parameter is a WAR for Bug432317. If is has a value of
// ccMaskComp then we will only allow "." <component> format, ie GT0.x, LE1.y,
// GE1.z, etc. and we will not allow the "." <xyzwSwizzle> format.
// If Pick = rndPickRequired, then we much choose CC0 or CC1 and the caller has
// already verified that there is a valid ccMask to use.
// If Pick = rndPick, then we can choose between CCnone, CC0, or CC1 provide there
// is a valid ccMask to use.

bool GpuPgmGen::CCTest(PgmInstruction *pOp, int Pick, int CompMask)
{
    // grammar spec:
    // <ccTest> ::= <ccMaskRule> <swizzleSuffix>
    // <swizzleSuffix> ::= /* empty */
    //                   | "." <component>
    //                   | "." <xyzwSwizzle>
    //                   | "." <rgbaSwizzle>
    // pick first so we don't change the random sequence.
    int ccToRead = pOp->CCtoRead;
    int ccValidMask = m_GS.Blocks.top()->CCValidMask();
    if (Pick != rndNoPick) // rndPick or rndPickRequired was specified.
    {
        ccToRead = PickCCtoRead();
        if (Pick == rndPick)
        {
            if ((ccToRead == RND_GPU_PROG_CCnone) || (ccValidMask == 0))
            {
                pOp->CCtoRead = RND_GPU_PROG_CCnone;
                return false;
            }
        }
        else if (Pick == rndPickRequired) // some OPs like IF require a <ccTest>
        {
            MASSERT(ccValidMask);
            // fallback to CC0 or CC1 if necesary.
            if (ccToRead == RND_GPU_PROG_CCnone)
            {
                ccToRead = (ccValidMask & 0x0f) ? RND_GPU_PROG_CC0 : RND_GPU_PROG_CC1;
            }
        }

        // Validate the ccValidMask against ccToRead and fallback to a valid CC register
        // if necessary.
        if ((ccToRead == RND_GPU_PROG_CC0) && (ccValidMask & 0x0f ) == 0)
        {
            ccToRead = RND_GPU_PROG_CC1;
        }
        else if ((ccToRead == RND_GPU_PROG_CC1) && (ccValidMask & 0xf0) == 0)
        {
            ccToRead = RND_GPU_PROG_CC0;
        }

        pOp->OutTest  = PickCCTest(pOp);
        if (pOp->OutTest == RND_GPU_PROG_OUT_TEST_none)
        {
            pOp->OutTest = RND_GPU_PROG_OUT_TEST_gt;
        }

    }

    if (((ccToRead == RND_GPU_PROG_CC0) || (ccToRead == RND_GPU_PROG_CC1))
        && (pOp->OutTest != RND_GPU_PROG_OUT_TEST_none))
    {
        int mask = 0;
        if (ccToRead == RND_GPU_PROG_CC0)
        {
            mask = ccValidMask & 0xf;
        }
        else if (ccToRead == RND_GPU_PROG_CC1)
        {
            mask = (ccValidMask >> 4) & 0xf;
        }

        // This block of code is a WAR for Bug432317, remove it ASAP!
        // We get a SegFault when using a swizzle mask but not a component mask
        if (CompMask == ccMaskComp)
        {
            // randomly pick a starting bit and locate the first
            // bit set.
            int bit = m_pFpCtx->Rand.GetRandom(0, 3);
            for ( int i = 0; i<4; i++, bit++)
            {
                bit = bit % 4;
                if (mask & (1<<bit)) // found a valid bit
                {
                    mask = (1<<bit);
                    break;
                }
            }
        }

        SwizzleSuffix(pOp, pOp->CCSwiz,&pOp->CCSwizCount, mask);
        pOp->CCtoRead = ccToRead;
        return true;    // a mask was used
    }
    return false; // no mask was used
}

//-----------------------------------------------------------------------------
void GpuPgmGen::SwizzleSuffix(PgmInstruction *pOp,
                              UINT08 *pswizzle,
                              int *pNumComp,
                              UINT08 ReadableMask)
{
    // Grammar spec:
    // <swizzleSuffix> ::=  </* empty */
    //                  |   "." <component>
    //                  |   "." <xyzwSwizzle>
    //                  |   "." <rgbaSwizzle>

    // Pick the xyzw/rgba swizzle order on the input or CC registers
    // make sure pswizzle[] has valid components in each index.
    if (ReadableMask == 0x0f)
    {
        switch (PickSwizzleOrder())
        {
            case RND_GPU_PROG_SWIZZLE_xyzw:
            {
                pswizzle[0] = 0;
                pswizzle[1] = 1;
                pswizzle[2] = 2;
                pswizzle[3] = 3;
                break;
            }

            case RND_GPU_PROG_SWIZZLE_shuffle:
            {
                UINT32 x[4] = { 0, 1, 2, 3};
                m_pFpCtx->Rand.Shuffle(4, x);

                pswizzle[0] = x[0];
                pswizzle[1] = x[1];
                pswizzle[2] = x[2];
                pswizzle[3] = x[3];
                break;
            }

            case RND_GPU_PROG_SWIZZLE_random:
            default:
            {
                pswizzle[0] = m_pFpCtx->Rand.GetRandom(0,3);
                pswizzle[1] = m_pFpCtx->Rand.GetRandom(0,3);
                pswizzle[2] = m_pFpCtx->Rand.GetRandom(0,3);
                pswizzle[3] = m_pFpCtx->Rand.GetRandom(0,3);
                break;
            }
        }
    } else
    {    // we are dealing with a CC register with 1 or more valid components.
        // randomly fill in the pswizzle[] with valid components
        for (int j=0; j<4; j++)
        {
            // Pick a random starting point and take the first one we find.
            int comp = m_pFpCtx->Rand.GetRandom(0,3);
            for (int i = 0; i < 4; i++,comp++)
            {
                comp %= 4;
                if ( ReadableMask & (1<<comp))
                {
                    pswizzle[j] = comp;
                    break;
                }
            }
        }
    }

    int numComp = 0;
    int swizSuffix = PickSwizzleSuffix();

    if ((pOp->GetOpInstType() == oitBra) && m_GS.Subroutines[m_GS.LwrSub]->m_Block.UsesTXD())
    {
        // WAR489105: we have used a TXD op in the subroutine we are calling.
        swizSuffix = RND_GPU_PROG_SWIZZLE_SUFFIX_component;
    }
    else if (pOp->GetOpInstType() == oitCali)
    {
        const int numCaliSubs = m_GS.Prog.SubArrayLen;
        for (int subIdx = 0; subIdx < numCaliSubs; subIdx++)
        {
            Subroutine * pSub = m_GS.Subroutines[subIdx];
            if (pSub->m_Block.UsesTXD())
            {
                // WAR489105: we have used a TXD op in one of the
                // subroutines we could be calling.
                swizSuffix = RND_GPU_PROG_SWIZZLE_SUFFIX_component;
            }
        }
    }

    switch ( swizSuffix)
    {
        default: MASSERT(!"Undefined swizzle suffix"); break;
        case RND_GPU_PROG_SWIZZLE_SUFFIX_all:       numComp = 4; break;
        case RND_GPU_PROG_SWIZZLE_SUFFIX_component: numComp = 1; break;
        case RND_GPU_PROG_SWIZZLE_SUFFIX_none:
        // no mask implies all 4 components in the xyzw order, so make sure all
        // 4 are readable, if not the pswizzle[] will specify what is readable.
        if ( ReadableMask == 0x0f)
        {
            numComp = 0;     // use implied form of the grammar.
            pswizzle[0] = 0; // make sure pswizzle[] is properly initialized
            pswizzle[1] = 1; // just incase we are referencing a texcoord[]
            pswizzle[2] = 2;
            pswizzle[3] = 3;
        }
        else
        {
            numComp = 4; //pswizzle is already initialized with valid components
        }
        break;
    }

    *pNumComp = numComp;
}

//-----------------------------------------------------------------------------
void GpuPgmGen::ScalarSuffix(PgmInstruction *pOp, UINT08 ReadableMask)
{
    UINT08 * pswizzle = pOp->InReg[pOp->LwrInReg].Swiz;
    int comp = m_pFpCtx->Rand.GetRandom(0,3);

    for (int i = 0; i < 4; i++,comp++)
    {
        comp %= 4;
        if ( ReadableMask & (1<<comp))
        {
            break;
        }
    }
    pOp->InReg[pOp->LwrInReg].SwizCount = 1;
    pswizzle[0] = pswizzle[1] = pswizzle[2] = pswizzle[3] = comp;
}

//-----------------------------------------------------------------------------
void GpuPgmGen::InstResultBase(PgmInstruction *pOp)
{
    // Grammar spec:
    // <instResultBase> ::= <tempUseW>
    //                  |   <resultUseW>

    // Sometimes we must write an output register, to get all requirements
    // written before the end of the program.
    // Otherwise, write to a temp register 2/3rds of the time.

    if (pOp->ForceRqdWrite || (m_pFpCtx->Rand.GetRandom(0,2) == 0))
    {
        ResultUseW(pOp); // write to result var with optional write mask
    }
    else
    {
        TempUseW(pOp); // write to temp var with optional write mask
    }
}

//-----------------------------------------------------------------------------
void GpuPgmGen::TempUseW(PgmInstruction *pOp)
{
    // Grammar spec:
    // <tempUseW> ::= <tempVarName> <optWriteMask>
    // The <tempVarName> rule matches identifiers that have been previously
    // established as names of temporary variables. We are going to refine this
    // to the following format:
    // <tempVarName> ::= "tmpR" <0 .. tempCount>
    // <tempCount> is the number of temporary variables previously created

    int rId = RegState::IlwalidId;
    for (int tries = 0; tries < 10; tries++)
    {
        int r = PickResultReg();
        const RegState & v = GetVar(r);

        if ((v.Attrs & regTemp) && (v.WriteableMask))
        {
            rId = r;
            break;
        }
    }
    if (rId == RegState::IlwalidId)
        rId = PickTempReg();
    pOp->OutReg.Reg = rId;

    const RegState & v = GetVar(rId);
    if (v.ArrayLen > 0)
        PickIndexing(&pOp->OutReg);

    OptWriteMask(pOp);
}

//-----------------------------------------------------------------------------
void GpuPgmGen::ResultUseW(PgmInstruction *pOp)
{
    bool bHandled = false;
    // Grammar spec:
    // <resultUseW> ::= <resultBasic> <optWriteMask>
    //                | <resultVarName> <optWriteMask>
    // flip a coin
    if (pOp->ForceRqdWrite || m_pFpCtx->Rand.GetRandom(0,255) & 0x01)
        bHandled = ResultBasic(pOp);
    else
        bHandled = ResultVarName(pOp);

    if (bHandled)
        OptWriteMask(pOp);
}

//-----------------------------------------------------------------------------
bool GpuPgmGen::ResultVarName(PgmInstruction *pOp)
{
    // Grammar spec:
    // The <resultVarName> rule match identifiers that have been previously
    // established as a name of a result variable.
    // <resultVarName> references variables created using the <OUTPUT_statement>
    // grammar rule
    // <OUTPUT_statement> ::= "OUTPUT" <establishName> "=" <resultUseD>
    // <resultUseD> ::= <resultBasic>

    // We are not implementing the <OUTPUT_statement> just yet.
    return ResultBasic(pOp);
}

//-----------------------------------------------------------------------------
bool GpuPgmGen::ResultBasic(PgmInstruction *pOp)
{
    // Grammar spec: (Vertex Programs)
    // <resultBasic> ::= <resPrefix> "position"
    //                  |<resPrefix> "fogcoord"
    //                  |<resPrefix> "pointsize"
    //                  |<resultTexCoord> <optArrayMemAbs>
    //                  |<resultClip> <arrayMemAbs>
    //                  |<resultGeneric> <arrayMemAbs>
    //                  |<resPrefix> "id"
    // Grammar spec: (Fragment Programs)
    // <resultBasic> ::= <resPrefix> "color" <resultOptColorNum>
    //                  |<resPrefix> "depth"
    // <resultOptColorNum> ::= /* empty */
    // Note: We can use the same algorithm to pick a result register as long
    //       as GsInit() properly sets up the register attributes!

    int outReg = RegState::IlwalidId;

    if (pOp->ForceRqdWrite)
    {
        // Must write to a lwrrently-not-fully-written o[] register.
        UINT32 numToWrite = m_pFpCtx->Rand.GetRandom(0,
                m_GS.MainBlock.NumRqdUnwritten() - 1);
        for (int reg = 0; reg < m_GS.MainBlock.NumVars(); reg++)
        {
            const RegState & v = GetVar(reg);
            const int rqdWriteMask = v.UnWrittenMask();

            if ((v.Attrs & regRqd) && (rqdWriteMask & pOp->GetResultMask()))
            {
                if (0 == numToWrite)
                {
                    // We'll write this one.
                    outReg = reg;
                    break;
                }
                else
                {   // find another reg to write
                    numToWrite--;
                }
            }
        }
    }
    else
    {
        // we can pick any output register, however the grammar requires that we
        // only write to a result register, not temporaries.
        for (int tries = 0; tries < 10; tries++)
        {
            int reg = PickResultReg();
            const RegState & v = GetVar(reg);

            if ((v.Attrs & regOut) && (v.WriteableMask & pOp->GetResultMask()))
            {
                outReg = reg;
                break;
            }
        }
    }

    if (outReg != RegState::IlwalidId)
    {
        pOp->OutReg.Reg = outReg;
        return true;
    }
    else
    {
        // The required output mask doesn't match this instruction's ResultMask.
        // This is not an error -- we will repick a different instruction
        // in the retry loop all the way up in StatmentSequence.
        //MASSERT(!"No valid regOut for this instruction");
        return false;
    }
}

//-----------------------------------------------------------------------------
// Note pOp->OutMask may produce a zero value, which would be invalid. This
// case will be tested in CommitInstruction(), see condition #3.
void GpuPgmGen::OptWriteMask(PgmInstruction *pOp, int Pick)
{
    // Grammar spec:
    // <optWriteMask> ::= /* empty */
    //                  | <xyzwMask>
    //                  | <rgbaMask>

    // Pick a write-mask for the output register.
    if (Pick == rndPick)
        pOp->OutMask = PickWriteMask();

    if (pOp->GetResultType() & atScalar)
    {
        // Scalar ops produce only one result, so let's usually not spam
        // that result across all the destination register components.
        if (m_pFpCtx->Rand.GetRandom(0,4))
            pOp->OutMask = (1 << m_pFpCtx->Rand.GetRandom(0,3));
    }

    const RegState & vOut = GetVar(pOp->OutReg.Reg);

    if (pOp->ForceRqdWrite)
    {
        // Make sure all unwritten components get written.
        pOp->OutMask |= vOut.UnWrittenMask();
    }

    // Some instructions (RFL, SCS,etc) don't write to all 4 components. So
    // apply the instruction's (internal) result mask to our OutMask so we
    // get consistent programs.
    pOp->OutMask &= pOp->GetResultMask();

    // Now apply this register's writtable mask.
    pOp->OutMask &= vOut.WriteableMask;

    // If we picked an outMask that is not consistent with the output
    // register just use the output register's mask
    if (0 == pOp->OutMask)
    {
        pOp->OutMask = (vOut.WriteableMask & pOp->GetResultMask());
    }
}

//-----------------------------------------------------------------------------
// Formats the data for the current instruction into lw_gpu_program4/5 syntax
string GpuPgmGen::FormatInstruction(PgmInstruction *pOp)
{
    string LwrInst = "";
    LwrInst.append(pOp->GetName());   // <op>
    // <op_modifier>
    // atomicModifier
    LwrInst.append(FormatAtomicModifier(pOp->OpModifier >> 16));
    // datatype
    LwrInst.append(FormatDataType(pOp->dstDT, pOp->GetDefaultType()));
    LwrInst.append(FormatDataType(pOp->srcDT,dtNone));

    // condition code (pre gpu4 grammar requires CC before _SAT)
    if (pOp->OpModifier & smCC0)
    {
        LwrInst.append( (m_BnfExt < extGpu4) ? "C" : ".CC0" );
    }
    else if (pOp->OpModifier & smCC1)
    {
        LwrInst.append( (m_BnfExt < extGpu4) ? "C1" : ".CC1" );
    }

    // saturation
    if (pOp->OpModifier & smSS0)
    {
        LwrInst.append( (m_BnfExt < extGpu4) ? "_SAT" : ".SAT" );
    }
    else if (pOp->OpModifier & smSS1)
    {
        LwrInst.append((m_BnfExt < extGpu4) ? "_SSAT" : ".SSAT" );
    }

    // rounding modifiers (gpu4 & greater)
    if (m_BnfExt >= extGpu4)
    {
        switch (pOp->OpModifier & smR)
        {
            case smCEIL:    LwrInst.append(".CEIL");    break;
            case smFLR:     LwrInst.append(".FLR");     break;
            case smTRUNC:   LwrInst.append(".TRUNC");   break;
            case smROUND:   LwrInst.append(".ROUND");   break;
            default: break;
        }
    }

    LwrInst.append(" "); // Pretty things up with a space.
    bool fmtCCMask = true;
    bool operandsHandledEarly = 0;

    // Deal with some special processing situations
    switch (pOp->GetOpInstType())
    {
        case oitBra:
            LwrInst.append(m_GS.Subroutines[m_GS.LwrSub]->m_Name);
            break;
        case oitCali:
            LwrInst.append(FormatOperand(pOp,0));
            operandsHandledEarly = true;
            break;
        case oitFlowCC: // BRK, CONT, RET
        case oitEndFlow: // ELSE, ENDIF, ENDREP (just a ';' at end of line)
        case oitMembar: // MEMBAR (just a ';' at end of line)
            LwrInst.resize(LwrInst.length()-1);
            break;
        case oitIf: // no output register or <ccMask>, just <ccTest> for this op.
            fmtCCMask = false;
            break;
    }

    // <instResult>
    if (pOp->GetResultType() != atNone)
    {
        LwrInst.append(GetRegName(pOp->OutReg.Reg));
        LwrInst.append(FormatArrayIndex(pOp->OutReg));

            // <optWriteMask>
        if (pOp->OutMask > 0 && pOp->OutMask < 0xf)
        {
            LwrInst.append( "." );
            for (int i = 0; i < 4; i++)
            {
                // xyzw can always be substituted for rgba
                const char * xyzw[] = {"x", "y", "z", "w"};
                if (pOp->OutMask & (1 << i))
                    LwrInst.append(xyzw[i]);
            }
        }
    }

        // <ccMask>
    if ((pOp->CCtoRead == RND_GPU_PROG_CC0 || pOp->CCtoRead == RND_GPU_PROG_CC1)
        && (pOp->OutTest != RND_GPU_PROG_OUT_TEST_none))
    {
        if (fmtCCMask)
        {
            LwrInst.append( FormatccMask(pOp) );
        }
        else
        {
            // IF uses only ccTest, not ccMask
            LwrInst.append( FormatccTest(pOp) );
        }
    }

    // special processing for the STOREIM.
    if (pOp->OpId == RND_GPU_PROG_OPCODE_storeim)
    {
        LwrInst.append(Utility::StrPrintf(" image[%d], ", pOp->IU.unit));
    }
    // <instOperandV>
    for (int idx = operandsHandledEarly; idx < pOp->LwrInReg; idx++)
    {
        // some instr. don't have a result, but do have an Operand(KILL & STORE)
        if (((idx == 0) && (pOp->GetResultType() != atNone)) || (idx > 0))
        {
            LwrInst.append(", ");
        }
        LwrInst.append(FormatOperand(pOp,idx));
    }
    // <texAccess>
    if ( pOp->IU.unit >= 0)
    {   // note: pOp->TxFetcher will be > 0 in this case.
        if(pOp->OpId == RND_GPU_PROG_OPCODE_storeim)
            LwrInst.append(Utility::StrPrintf(", "));
        else
            LwrInst.append(Utility::StrPrintf(", image[%d], ", pOp->IU.unit));

        LwrInst.append(FormatTargetName(pOp->TxAttr));
    }
    else if ( pOp->TxFetcher >= 0)
    {
        if (IsBindlessTextures())
            LwrInst.append(Utility::StrPrintf(", handle(hTx.x)"));
        else
            LwrInst.append(Utility::StrPrintf(", texture[%d]",pOp->TxFetcher));

        LwrInst.append(FormatTxgTxComponent(pOp));
        LwrInst.append(", ");
        LwrInst.append(FormatTargetName(pOp->TxAttr));
    }
    if (pOp->TxOffsetFmt != toNone)
    {
        LwrInst.append(FormatTxOffset(pOp));
    }
    // <ExtSwizzle>
    if (pOp->ExtSwizCount)
    {
        LwrInst.append(FormatExtSwizzle(pOp->ExtSwiz, pOp->ExtSwizCount));
    }
    LwrInst.append(";");
    if (pOp->UsrComment.size() > 0)
        LwrInst.append(pOp->UsrComment);

    return LwrInst;
}

//-----------------------------------------------------------------------------
//     <ccMask>                ::= "(" <ccTest> ")"
string GpuPgmGen::FormatccMask(PgmInstruction *pOp)
{
    string s = "(";
    s += FormatccTest(pOp);
    s += ")";
    return s;
}

//-----------------------------------------------------------------------------
//    <ccTest>                ::= <ccMaskRule> <swizzleSuffix>
string GpuPgmGen::FormatccTest(PgmInstruction *pOp)
{
    //<ccTest>
    static const char* ccTest[]=
    {
        "EQ", "GE" ,"GT", "LE" ,"LT","NE" ,"TR","FL" , // < extGpu4
        "NAN","LEG","CF", "NCF","OF", "NOF","AB","BLE","SF","NSF" // >= extGpu4
    };

    MASSERT(pOp->OutTest < (int)NUMELEMS(ccTest));
    string s = ccTest[pOp->OutTest];

    if ((pOp->CCtoRead == RND_GPU_PROG_CC0) && (m_BnfExt >= extGpu4))
    {
        s += "0";
    }
    else if (pOp->CCtoRead == RND_GPU_PROG_CC1)
    {
        s += "1";
    }

    //<swizzleSuffix>
    if (pOp->CCSwizCount > 0)
    {
        s += ".";
        const char * xyzw[] = { "x", "y", "z", "w" };
        for (int i = 0; i < pOp->CCSwizCount; i++)
            s += xyzw[ pOp->CCSwiz[i] ];
    }

    return s;
}

//-----------------------------------------------------------------------------
// Formats the atomic modifier to the current instruction.
string GpuPgmGen::FormatAtomicModifier(int am)
{
    const char * modifiers[] =
    {
        "",     //amNone
        ".ADD", //amAdd
        ".MIN", //amMin
        ".MAX", //amMax
        ".IWRAP", //amIwrap
        ".DWRAP", //amDwrap
        ".AND", //amAnd
        ".OR",  //amOr
        ".XOR", //amXor
        ".EXCH",//amExch
        ".CSWAP",//amCSwap
    };
    MASSERT( am < (int)NUMELEMS(modifiers));

    return (am < (int)NUMELEMS(modifiers) ? modifiers[am]: "");
}

//-----------------------------------------------------------------------------
// Formats the dataType modifier to the current instruction.
string GpuPgmGen::FormatDataType( opDataType dt, opDataType defaultType)
{
    if( m_BnfExt < extGpu4)
    {
        return("");
    }
    if( dt == defaultType && !(dt & dt32e))
    {
        return(""); // use the implicit format
    }

    switch (dt) // use explicit format
    {
        case dtF16:     return(".F16");
        case dtF32:     return(".F");
        case dtF32e:    return(".F32");
        case dtF64:     return(".F64");
        case dtS8:      return(".S8");
        case dtS16:     return(".S16");
        case dtS24:     return(".S24");
        case dtS32:     return(".S");
        case dtS32e:    return(".S32");
        case dtS32_HI:  return(".S.HI");
        case dtS64:     return(".S64");
        case dtU8:      return(".U8");
        case dtU16:     return(".U16");
        case dtU24:     return(".U24");
        case dtU32:     return(".U");
        case dtU32e:    return(".U32");
        case dtU32_HI:  return(".U.HI");
        case dtU64:     return(".U64");
        case dtF16X2a:  return(".F16X2");
        case dtF16X4a:  return(".F16X4");
        case dtF32X2:   return(".F32X2");
        case dtF32X4:   return(".F32X4");
        case dtF64X2:   return(".F64X2");
        case dtF64X4:   return(".F64X4");
        case dtS32X2:   return(".S32X2");
        case dtS32X4:   return(".S32X4");
        case dtS64X2:   return(".S64X2");
        case dtS64X4:   return(".S64X4");
        case dtU32X2:   return(".U32X2");
        case dtU32X4:   return(".U32X4");
        case dtU64X2:   return(".U64X2");
        case dtU64X4:   return(".U64X4");

        default:
            MASSERT(!"Unexpected DataType");
            // fall through
        case dtNone:    return("");
    }
}

//-----------------------------------------------------------------------------
string GpuPgmGen::FormatOperand(PgmInstruction *pOp, int idx)
{
    string str = "";
    const int reg = pOp->InReg[idx].Reg;

    if (reg == GetLiteralReg() || reg == GetConstVectReg())
    {
        return FormatLiteral(pOp,idx);
    }

    if (pOp->InReg[idx].Neg)
    {
        str.append("-");
    }

    if (pOp->InReg[idx].Abs) // <instOperandAbsV>
    {
        str.append("|");
    }

    str.append(GetRegName(reg));
    str.append(FormatArrayIndex(pOp->InReg[idx]));

    str.append(FormatSwizzleSuffix(pOp->InReg[idx].Swiz,
            pOp->InReg[idx].SwizCount));

    if (pOp->InReg[idx].Abs == true) // <instOperandAbsV>
    {
        str.append("|");
    }
    return str;
}

//-----------------------------------------------------------------------------
string GpuPgmGen::FormatArrayIndex(const RegUse & ru)
{
    const RegState & v = GetVar(ru.Reg);
    if (v.ArrayLen)
    {
        if (IsIndexReg(ru.IndexReg))
        {
            // ie. [tmpA128.x + 120]
            return Utility::StrPrintf(
                    "[%s.%c + %d]",
                    GetRegName(ru.IndexReg).c_str(),
                    "xyzw"[ru.IndexRegComp],
                    ru.IndexOffset);
        }
        else
        {
            return Utility::StrPrintf(
                    "[%3d]",
                    ru.IndexOffset);
        }
    }
    // else not indexed
    return "";
}

//-----------------------------------------------------------------------------
// Appends the ASCII text string for a constVector or upto 4 literal values.
string GpuPgmGen::FormatLiteral(PgmInstruction *pOp, int idx)
{
    string str = "";
    const Literal & L = pOp->InReg[idx].LiteralVal;

    for(int i = 0; i < L.m_Count; i++)
    {
        if (i > 0)
            str.append(",");

        switch (L.m_Type)
        {
        case GpuPgmGen::Literal::s32:
            str += Utility::StrPrintf("%d", L.m_Data.m_s32[i]);
            break;
        case GpuPgmGen::Literal::u32:
            str += Utility::StrPrintf("0x%x", L.m_Data.m_u32[i]);
            break;
        case GpuPgmGen::Literal::f32:
            str += Utility::StrPrintf("%4.3f", L.m_Data.m_f32[i]);
            break;
        //TODO format out these datatypes
        case GpuPgmGen::Literal::s64:
        case GpuPgmGen::Literal::u64:
        case GpuPgmGen::Literal::f64:
            MASSERT(!"64bit literals not implemented");
            break;
        }
    }

    if (pOp->InReg[idx].Reg == GetConstVectReg())
    {
        str = Utility::StrPrintf("{%s}", str.c_str());
    }

    return str;
}
//-----------------------------------------------------------------------------
string GpuPgmGen::FormatTxOffset(PgmInstruction *pOp)
{
    switch (pOp->TxOffsetFmt)
    {
        case toConstant:
            return Utility::StrPrintf(", (%d,%d,%d)",
                    pOp->TxOffset[0],pOp->TxOffset[1],pOp->TxOffset[2]);
        case toProgrammable:
            return Utility::StrPrintf(", offset(%s)",
                    GetRegName(pOp->TxOffsetReg).c_str());
        case toNone:
        default:
            break;
    }
    return "";
}

//-----------------------------------------------------------------------------
string GpuPgmGen::FormatTxgTxComponent(PgmInstruction *pOp)
{
    string str = "";
    if (pOp->TxgTxComponent != -1)
    {
        str.append(".");
        const char * xyzw[] = {"x", "y", "z", "w"};
        str.append( xyzw[ pOp->TxgTxComponent ]);
    }
    return str;
}

//-----------------------------------------------------------------------------
string GpuPgmGen::FormatSwizzleSuffix(UINT08 *pswizzle, int numComp)
{
    string str = "";
    if ( numComp)
    {
        str.append(".");
        const char * xyzw[] = {"x", "y", "z", "w"};
        for ( int i = 0; i<numComp; i++)
            str.append( xyzw[ pswizzle[i] ]);
    }
    return str;
}
//-----------------------------------------------------------------------------
string GpuPgmGen::FormatExtSwizzle(UINT08 *pswizzle, int numComp)
{
    string str = "";
    if ( numComp)
    {
        const char * xyzw[] = {"x", "y", "z", "w", "0", "1"};
        int i;
        for ( i = 0; i<numComp; i++)
        {
            str.append( ",");
            str.append( xyzw[ pswizzle[i] ]);
        }
    }
    return str;
}

string GpuPgmGen::FormatTargetName(int TxAttr)
{
    switch (TxAttr & SASMDimFlags)
    {
        default:  MASSERT(!"Unrecognized tx dim in texAccess().");
        // fall through
        case Is2d:              return("2D");
        case Is1d:              return("1D");
        case Is3d:              return("3D");
        case IsLwbe:            return("LWBE");
        case IsArray1d:         return("ARRAY1D");
        case IsArray2d:         return("ARRAY2D");
        case IsArrayLwbe:       return("ARRAYLWBE");
        case Is2dMultisample:   return("RENDERBUFFER");
        case IsShadow1d:        return("SHADOW1D");
        case IsShadow2d:        return("SHADOW2D");
        case IsShadowLwbe:      return("SHADOWLWBE");
        case IsShadowArray1d:   return("SHADOWARRAY1D");
        case IsShadowArray2d:   return("SHADOWARRAY2D");
        case IsShadowArrayLwbe: return("SHADOWARRAYLWBE");
    }

}
//-----------------------------------------------------------------------------
void GpuPgmGen::AddPgmComment( const char * szComment)
{
    PgmAppend("#");
    PgmAppend(szComment);
    PgmAppend("\n");
}

//-----------------------------------------------------------------------------
int GpuPgmGen::GetMaxTxCdSz(int compMask)
{
    if (compMask & compW) return 4;
    if (compMask & compZ) return 3;
    if (compMask & compY) return 2;
    if (compMask & compX) return 1;
    return 0;
}

//-----------------------------------------------------------------------------
// colwert a program propery to a string
string GpuPgmGen::PropToString(INT32 prop)
{
    static const char *szProp[] =
    {
        "ppPos",        "ppNrml",         "ppPriColor",  "ppSecColor",    "ppBkPriColor",
        "ppBkSecColor", "ppFogCoord",     "ppPtSize",    "ppVtxId",       "ppInstance",
        "ppFacing",     "ppDepth",        "ppPrimId",    "ppLayer",       "ppViewport",
        "ppPrimitive",  "ppFullyCovered", "ppBaryCoord", "ppBaryNoPersp", "ppShadingRate"
    };
    string str = "Unknown";

    if( prop < ppPos)
        str = "ppIlwalid";
    else if (prop < ppClip0)
        str = szProp[prop];
    else if (prop < ppGenAttr0)
    {
        str = "ppClip0";
        if (prop - ppClip0)
        {
            str.append(Utility::StrPrintf("+%d", prop-ppClip0));
        }
    }
    else if (prop < ppClrBuf0)
    {
        str = "ppGenAttr0";
        if (prop - ppGenAttr0)
        {
            str.append(Utility::StrPrintf("+%d", prop-ppGenAttr0));
        }
    }
    else if (prop < ppTxCd0)
    {
        str = "ppClrBuf0";
        if ( prop - ppClrBuf0)
        {
            str.append(Utility::StrPrintf("+%d", prop-ppClrBuf0));
        }
    }
    else if (prop < ppNUM_ProgProperty)
    {
        str = "ppTxCd0";
        if ( prop-ppTxCd0)
        {
            str.append(Utility::StrPrintf("+%d", prop-ppTxCd0));
        }
    }
    else
        MASSERT(!"Unknown ProgProperty value");

    return str;
}

//-----------------------------------------------------------------------------
// Colwert the XYZW component mask to a string
string GpuPgmGen::CompToString (INT32 comp)
{
    MASSERT(comp & compXYZW);
    string str = "comp";
    if (comp & compX)
        str.append("X");
    if (comp & compY)
        str.append("Y");
    if (comp & compZ)
        str.append("Z");
    if (comp & compW)
        str.append("W");
    if (comp & omPassThru)
        str.append("+omPassThru");
    return str;
}

const char* GpuPgmGen::CompMaskToString(INT32 comp)
{
    comp &= compXYZW;
    const static char *xyzwMask[16] =
    {
        " ",
        ".x",
        ".y",
        ".xy",
        ".z",
        ".xz",
        ".yz",
        ".xyz",
        ".w",
        ".xw",
        ".yw",
        ".xyw",
        ".zw",
        ".xzw",
        ".yzw",
        ".xyzw"
    };
    return xyzwMask[comp];
}

//-----------------------------------------------------------------------------
// Return the EndStrategy define as a string.
const char * GpuPgmGen::EndStrategyToString(int strategy)
{
    #define STRATEGY_ENUM_STRING(x)   case x: return #x
    switch (strategy)
    { 
        STRATEGY_ENUM_STRING(RND_GPU_PROG_FR_END_STRATEGY_frc);
        STRATEGY_ENUM_STRING(RND_GPU_PROG_FR_END_STRATEGY_mod);
        STRATEGY_ENUM_STRING(RND_GPU_PROG_FR_END_STRATEGY_coarse);
        STRATEGY_ENUM_STRING(RND_GPU_PROG_FR_END_STRATEGY_mov);
        STRATEGY_ENUM_STRING(RND_GPU_PROG_FR_END_STRATEGY_mrt_fs_pred);
        STRATEGY_ENUM_STRING(RND_GPU_PROG_FR_END_STRATEGY_mrt_fs_cali);
        STRATEGY_ENUM_STRING(RND_GPU_PROG_FR_END_STRATEGY_per_vtx_attr);
        default:
        STRATEGY_ENUM_STRING(RND_GPU_PROG_FR_END_STRATEGY_unknown);
    }
}

//-----------------------------------------------------------------------------
// Colwert a geometry or tess input/output primitive to a string
string GpuPgmGen::PrimitiveToString (int Primitive)
{
    switch (Primitive)
    {
        case GL_LINES:                      return "LINES";
        case GL_LINES_ADJACENCY_EXT:        return "LINES_ADJACENCY";
        case GL_TRIANGLES:                  return "TRIANGLES";
        case GL_TRIANGLES_ADJACENCY_EXT:    return "TRIANGLES_ADJACENCY";
        case GL_TRIANGLE_STRIP:             return "TRIANGLE_STRIP";
        case GL_LINE_STRIP:                 return "LINE_STRIP";
        case GL_POINTS:                     return "POINTS";
        case GL_PATCHES:                    return "PATCHES";

        // special case for don't-care
        case -1:                            return "";

        default:
            MASSERT(!"Unknown geometry primitive");
            return Utility::StrPrintf("%d?", Primitive);
    }
}

const GpuPgmGen::RegState & GpuPgmGen::GetVar (int id) const
{
    return m_GS.Blocks.top()->GetVar(id);
}

//-----------------------------------------------------------------------------
void GpuPgmGen::UtilOpDst
(
    GpuPgmGen::PgmInstruction * pOp,
    int opCode,
    int dstReg
)
{
    pOp->Init(&m_GS, opCode);
    pOp->OutReg.Reg   = dstReg;
    pOp->OutMask      = (GetVar(dstReg).WriteableMask & pOp->GetResultMask());
}
void GpuPgmGen::UtilOpDst
(
    GpuPgmGen::PgmInstruction *pOp,
    int opCode,
    int dstReg,
    int outMask,
    int dstDT
)
{
    pOp->Init(&m_GS, opCode);
    pOp->OutReg.Reg = dstReg;
    pOp->OutMask = outMask;
    pOp->dstDT = (GLRandom::opDataType)dstDT;
}

//-----------------------------------------------------------------------------
void GpuPgmGen::UtilOpInReg
(
    GpuPgmGen::PgmInstruction * pOp,
    const RegUse & regUseToCopy
)
{
    pOp->InReg[pOp->LwrInReg] = regUseToCopy;
    pOp->LwrInReg++;
}

//-----------------------------------------------------------------------------
void GpuPgmGen::UtilOpInReg
(
    GpuPgmGen::PgmInstruction * pOp,
    int regId
)
{
    // Must use the const RegUse& version of UtilOpInReg, or one of the
    // UtilOpInConst methods for an immediate literal!
    MASSERT(regId != GetLiteralReg() && regId != GetConstVectReg());

    pOp->InReg[pOp->LwrInReg].Reg = regId;
    pOp->LwrInReg++;
}

//-----------------------------------------------------------------------------
void GpuPgmGen::UtilOpInReg
(
    GpuPgmGen::PgmInstruction *pOp,
    int regId,
    int swizComp
)
{
    // Must use the const RegUse& version of UtilOpInReg, or one of the
    // UtilOpInConst methods for an immediate literal!
    MASSERT(regId != GetLiteralReg() && regId != GetConstVectReg());

    const int idx = pOp->LwrInReg;
    pOp->InReg[idx].Reg = regId;
    pOp->InReg[idx].Swiz[siX] = swizComp;
    pOp->InReg[idx].SwizCount = 1;
    pOp->LwrInReg++;
}

//-----------------------------------------------------------------------------
void GpuPgmGen::UtilOpInReg
(
    GpuPgmGen::PgmInstruction *pOp,
    int regId,
    int swiz0,
    int swiz1,
    int swiz2,
    int swiz3
)
{
    // Must use the const RegUse& version of UtilOpInReg, or one of the
    // UtilOpInConst methods for an immediate literal!
    MASSERT(regId != GetLiteralReg() && regId != GetConstVectReg());

    const int idx = pOp->LwrInReg;
    pOp->InReg[idx].Reg = regId;
    pOp->InReg[idx].Swiz[siX] = swiz0;
    pOp->InReg[idx].Swiz[siY] = swiz1;
    pOp->InReg[idx].Swiz[siZ] = swiz2;
    pOp->InReg[idx].Swiz[siW] = swiz3;
    pOp->InReg[idx].SwizCount = 4;
    pOp->LwrInReg++;
}

//-----------------------------------------------------------------------------
void GpuPgmGen::UtilOpInConstU32
(
    GpuPgmGen::PgmInstruction * pOp,
    UINT32 inUI
)
{
    int rIdx = pOp->LwrInReg;
    pOp->LwrInReg++;
    pOp->InReg[rIdx].Reg = GetLiteralReg();
    pOp->InReg[rIdx].LiteralVal.m_Count = 1;
    pOp->InReg[rIdx].LiteralVal.m_Type   = GpuPgmGen::Literal::u32;
    pOp->InReg[rIdx].LiteralVal.m_Data.m_u32[0] = inUI;
}

//-----------------------------------------------------------------------------
void GpuPgmGen::UtilOpInConstF32
(
    GpuPgmGen::PgmInstruction * pOp,
    FLOAT32 inF
)
{
    int rIdx = pOp->LwrInReg;
    pOp->LwrInReg++;
    pOp->InReg[rIdx].Reg = GetLiteralReg();
    pOp->InReg[rIdx].LiteralVal.m_Count = 1;
    pOp->InReg[rIdx].LiteralVal.m_Type   = GpuPgmGen::Literal::f32;
    pOp->InReg[rIdx].LiteralVal.m_Data.m_f32[0] = inF;
}

//-----------------------------------------------------------------------------
void GpuPgmGen::UtilOpInConstF32
(
    GpuPgmGen::PgmInstruction * pOp,
    FLOAT32 inFx,
    FLOAT32 inFy,
    FLOAT32 inFz,
    FLOAT32 inFw
)
{
    int rIdx = pOp->LwrInReg;
    pOp->LwrInReg++;
    pOp->InReg[rIdx].Reg = GetConstVectReg();
    pOp->InReg[rIdx].LiteralVal.m_Count = 4;
    pOp->InReg[rIdx].LiteralVal.m_Type   = GpuPgmGen::Literal::f32;
    pOp->InReg[rIdx].LiteralVal.m_Data.m_f32[0] = inFx;
    pOp->InReg[rIdx].LiteralVal.m_Data.m_f32[1] = inFy;
    pOp->InReg[rIdx].LiteralVal.m_Data.m_f32[2] = inFz;
    pOp->InReg[rIdx].LiteralVal.m_Data.m_f32[3] = inFw;
}

//-----------------------------------------------------------------------------
void GpuPgmGen::UtilOpInConstS32
(
    GpuPgmGen::PgmInstruction * pOp,
    INT32 inSx,
    INT32 inSy,
    INT32 inSz,
    INT32 inSw
)
{
    int rIdx = pOp->LwrInReg;
    pOp->LwrInReg++;
    pOp->InReg[rIdx].Reg = GetConstVectReg();
    pOp->InReg[rIdx].LiteralVal.m_Count = 4;
    pOp->InReg[rIdx].LiteralVal.m_Type   = GpuPgmGen::Literal::s32;
    pOp->InReg[rIdx].LiteralVal.m_Data.m_s32[0] = inSx;
    pOp->InReg[rIdx].LiteralVal.m_Data.m_s32[1] = inSy;
    pOp->InReg[rIdx].LiteralVal.m_Data.m_s32[2] = inSz;
    pOp->InReg[rIdx].LiteralVal.m_Data.m_s32[3] = inSw;
}

//-----------------------------------------------------------------------------
void GpuPgmGen::UtilOpCommit
(
    GpuPgmGen::PgmInstruction * pOp
    ,const char * comment           // = NULL 
    ,bool bApplyScoring
)
{
    // Fix up op before commit:
    for (int idx = 0; idx < pOp->LwrInReg; idx++)
    {
        // Generate random literals if not filled in already.
        FixRandomLiterals(pOp, idx);
    }

    if (CommitInstruction(pOp, false, bApplyScoring) == GpuPgmGen::instrValid)
        CommitInstText(pOp, comment, bApplyScoring);
    else
        MASSERT(!"Malformed Util instr!");
}

//-----------------------------------------------------------------------------
// Copies a constant vector to dstReg using the vals array.
// Issue a MOV rsltReg {val[0],val[1],val[2],val[3]}; instruction
// and then update the results score using the compMask and rsltValue
void GpuPgmGen::UtilCopyConstVect( int DstReg, float Vals[4])
{
    PgmInstruction Op;
    UtilOpDst(&Op, RND_GPU_PROG_OPCODE_mov, DstReg);
    UtilOpInConstF32(&Op, Vals[0], Vals[1], Vals[2], Vals[3]);
    UtilOpCommit(&Op);
}

//-----------------------------------------------------------------------------
// Copies a constant vector to dstReg using the vals array.
// Issue a MOV rsltReg {val[0],val[1],val[2],val[3]}; instruction
// and then marks the destination register as used and updates the results score
// using the compMask and rsltValue
void GpuPgmGen::UtilCopyConstVect( int DstReg, int Vals[4])
{
    PgmInstruction Op;
    UtilOpDst(&Op, RND_GPU_PROG_OPCODE_mov, DstReg);
    UtilOpInConstS32(&Op, Vals[0], Vals[1], Vals[2], Vals[3]);
    UtilOpCommit(&Op);
}

//-----------------------------------------------------------------------------
// Copy the srcReg to the dstReg using the MOV instruction.
// and then update the results score using the compMask and rsltValue
// Note: srcReg & dstReg are non-constant vectors, and no swizzle is performed.
void GpuPgmGen::UtilCopyReg(int DstReg, int SrcReg)
{
    PgmInstruction Op;
    UtilOpDst(&Op, RND_GPU_PROG_OPCODE_mov, DstReg);
    UtilOpInReg(&Op, SrcReg);
    UtilOpCommit(&Op);
}

// Get a temp register that has already been written so as not to disturb the 
// random sequence.
int GpuPgmGen::GetUsedTempReg(int WriteMask)
{
    int reg = GetFirstTempReg();
    for (int i = 0; i < GetNumTempRegs(); i++)
    {
        const RegState & v = GetVar(reg+i);
        if ((v.WrittenMask & WriteMask) == WriteMask)
        {
            return reg+i;
        }
    }
    return -1;
}
//--------------------------------------------------------------------------------
// Some instructions require that the input be > 0.0. So take the abs of the
// original input and conditionally add 0.01 to any component that is <= 0.0. and 
// store the result in the requested temp register.
void GpuPgmGen::UtilSanitizeInputAgainstZero
(
    PgmInstruction *pOp,
    int TmpReg,
    int InputRegIdx,
    int WriteMask
)
{
    // Should support both scalar and vector variants
    // MOV.F.CC0 tmpR1.y |vertex.texcoord[0].y|;  //scalar
    // MOV.F.CC0 tmpR1 |vertex.texcoord[0]|;      //vector
    PgmInstruction Mov; 
    UtilOpDst(&Mov, RND_GPU_PROG_OPCODE_mov, TmpReg);
    Mov.dstDT       = dtF32;
    Mov.OpModifier  = smCC0;
    Mov.OutMask     = WriteMask;
    Mov.CCSwizCount = pOp->InReg[InputRegIdx].SwizCount;
    // CCSwiz[] is initialized to .xyzw
    if (Mov.CCSwizCount == 1)
    {   //.x .y .z or .w
        Mov.CCSwiz[0] = pOp->InReg[InputRegIdx].Swiz[0];
    }
    UtilOpInReg(&Mov, pOp->InReg[InputRegIdx]);
    Mov.InReg[0].Abs = true;
    UtilOpCommit(&Mov, nullptr, false);  // don't apply scoring

    // ADD.F tmpR1.y(EQ.y), tmpR1.y, { 10.0f, 10.0f, 10.0f, 10.0f }; 
    // ADD.F tmpR1(EQ.xyzw), tmpR1, { 10.0f, 10.0f, 10.0f, 10.0f };  
    PgmInstruction Add; 
    UtilOpDst(&Add, RND_GPU_PROG_OPCODE_add, TmpReg);
    Add.dstDT       = dtF32;
    Add.OutMask     = WriteMask;
    Add.CCtoRead    = RND_GPU_PROG_CC0;
    Add.OutTest     = RND_GPU_PROG_OUT_TEST_eq;
    UtilOpInReg(&Add, TmpReg);
    //ApproxRSQRT(10) = 0.316
    UtilOpInConstF32(&Add, 10.0f, 10.0f, 10.0f, 10.0f);
    UtilOpCommit(&Add, nullptr, false); // don't apply scoring
}
//-----------------------------------------------------------------------------
// Some instructions require that the inputs be sanitized to prevent +/-INF and
// NAN values. This routine provides that functionality by moving the contents
// from the input register to a temp reg, then adding its negative value. The
// expected result will be zero for all values except +/-INF and NAN. In those
// cases the temp register's components will be explicitly set to zero. The
// sanitized results will be stored in the temp register.
void GpuPgmGen::UtilSanitizeInput
(
    PgmInstruction *pOp,
    int GarbageReg,
    int TmpReg,
    int InputRegIdx,
    int WriteMask
)
{
    MarkRegUsed(GarbageReg);
    UtilCopyOperand( pOp, TmpReg, InputRegIdx);

    PgmInstruction Add; //"ADD.F.CC0 %garbageReg.xyzw,-TmpReg.xyzw,TmpReg.xyzw;",
    UtilOpDst(&Add, RND_GPU_PROG_OPCODE_add, GarbageReg);
    Add.dstDT       = dtF32;
    Add.OpModifier  = smCC0;
    Add.CCtoRead    = RND_GPU_PROG_CCnone;
    UtilOpInReg(&Add, TmpReg);
    Add.InReg[0].Neg= true;
    UtilOpInReg(&Add, TmpReg);
    UtilOpCommit(&Add);

    PgmInstruction Mov; // "MOV.F tempReg.xyzw(NE0.xyzw), {0,0,0,0};",
    UtilOpDst(&Mov, RND_GPU_PROG_OPCODE_mov, TmpReg);
    Mov.dstDT       = dtF32;
    Mov.OutMask     = WriteMask;
    Mov.CCtoRead    = RND_GPU_PROG_CC0;
    Mov.OutTest     = RND_GPU_PROG_OUT_TEST_ne;
    Mov.CCSwizCount = 4;    // note: CCSwiz[] is initialized for xyzw
    UtilOpInConstF32(&Mov, 0.0);
    UtilOpCommit(&Mov);
}

//-----------------------------------------------------------------------------
void GpuPgmGen::UtilCopyOperand
(
    PgmInstruction *pOp,
    int DstReg,
    int InputRegIdx,
    int WriteMask
)
{
    // Create a new MOV instruction that copies the user's Instruction operandX
    // to a new destination register DstReg.
    PgmInstruction Mov;
    UtilOpDst(&Mov, RND_GPU_PROG_OPCODE_mov, DstReg);
    Mov.dstDT           = dtF32;
    Mov.InReg[0]        = pOp->InReg[InputRegIdx];
    Mov.LwrInReg        = 1;
    if (WriteMask)
    {
        Mov.OutMask     = WriteMask;
    }
    UtilOpCommit(&Mov);
}

//-----------------------------------------------------------------------------
// Perform a basic binary operand instruction using the following form:
// instr dstReg, operand1, operand2;
// No swizzle is performed, operands must be readable vectors.
void GpuPgmGen::UtilBinOp( int Instr, int DstReg, int Operand1, int Operand2)
{
    PgmInstruction Op;
    UtilOpDst(&Op, Instr, DstReg);
    UtilOpInReg(&Op, Operand1);
    UtilOpInReg(&Op, Operand2);
    UtilOpCommit(&Op);
}

//-----------------------------------------------------------------------------
// Perform a basic binary operand instruction using the following form:
// instr dstReg, operand1, {val[0], val[1], val[2], val[3]};
// No swizzle is performed, operands must be readable vectors.
void GpuPgmGen::UtilBinOp( int Instr, int DstReg, int Operand1, float Vals[4])
{
    PgmInstruction Op;
    UtilOpDst(&Op, Instr, DstReg);
    UtilOpInReg(&Op, Operand1);
    UtilOpInConstF32(&Op, Vals[0], Vals[1], Vals[2], Vals[3]);
    UtilOpCommit(&Op);
}

//-----------------------------------------------------------------------------
// Perform a basic trinary operand instruction using the following form:
// instr dstReg, operand1, operand2, operand3;
// No swizzle is performed, operands must be readable vectors.
void GpuPgmGen::UtilTriOp
(
    int Instr,
    int DstReg,
    int Operand1,
    int Operand2,
    int Operand3
)
{
    PgmInstruction Op;
    UtilOpDst(&Op, Instr, DstReg);
    UtilOpInReg(&Op, Operand1);
    UtilOpInReg(&Op, Operand2);
    UtilOpInReg(&Op, Operand3);
    UtilOpCommit(&Op);
}

//-----------------------------------------------------------------------------
// Perform the branch instruction to call a specific subroutine using this form:
// CAL subName;
void GpuPgmGen::UtilBranchOp( int si)
{
    int orgSi = m_GS.LwrSub;
    PgmInstruction Bra;
    Bra.Init( &m_GS, RND_GPU_PROG_OPCODE_cal);
    m_GS.LwrSub = si;
    UtilOpCommit(&Bra);
    m_GS.Subroutines[si]->m_IsCalled = true;
    m_GS.LwrSub = orgSi;
}

// Create a tex coordinate value that is scaled to the size of MMlevel x for the
// current texture image that is bound to ImageUnit y. The routine requires that
// the following variables be properly setup.
//  pOp->TxFetcher:   Fetcher to use to query the image size.
//  pOp->IU.unit:     Image Unit bound to the texture image
//  pOp->IU.mmLevel:  Texture Image MM level to query.
//  pOp->InReg[pOp->LwrInReg-1]: The register holding the unscaled texcoord
//                               values.
// The following variables will be updated:
//  pOp->InReg[pOp->LwrInReg-1]: The new temp register holding the scaled
//  texcoord values.
//
// Like this(if scaleMethod == smLinear):
//   TXQ R0, x, texture[x], 2D;     // query its size for ImageUnitMMLevel
//   I2F R0.xyz, R0;                // colwert to float
//   FRC R1, |fragment.texcoord[x]|;// normalize the texcoord[x] to 0-1 value.
//   MUL R0.xyz, R1, R0;            // scale texture coordinates
//   FLR.U R0.xyz, R0;              // colwert scaled coordinates to integer
//                                     0..0.9999 all map to zero
// Like this (if scaleMethod == smWrap)
//   TXQ R0, x, texture[x], 2D;     // query its size for ImageUnitMMLevel
//   FLR R1, fragment.position;     // colwert to int.
//   MOD R1.x, R1.x, R0.x;          // modulo the x coordinate by surface width
//   MOD.R1.y, R1.y, R0.y;          // modulo the y coordinate by surface height
//   MOD.R1.z, R1.y, R0.z;          // modulo the y coordinate by surface height
void GpuPgmGen::UtilScaleRegToImage(PgmInstruction * pOp)
{
    // scale the texcoord register to the image layer (see example above).
    const int TmpR0 = PickTempReg();
    const int TmpR1 = TmpR0 > GetFirstTempReg() ? (TmpR0-1) : (TmpR0+1);
    MarkRegUsed(TmpR0);
    MarkRegUsed(TmpR1);

    PgmInstruction Op;
    //TXQ R0, x, texture[y], 2D;
    UtilOpDst(&Op,RND_GPU_PROG_OPCODE_txq, TmpR0, compXYZ, dtNone);
    UtilOpInConstU32(&Op, pOp->IU.mmLevel);
    Op.IU.mmLevel = pOp->IU.mmLevel;
    Op.TxFetcher = pOp->TxFetcher;
    Op.TxAttr = pOp->TxAttr;
    CommitInstText(&Op, Utility::StrPrintf("Query texture size for LOD %d",
                                           pOp->IU.mmLevel));

    const int scaleMethod = GetTxCdScalingMethod(pOp);
    if (scaleMethod == smWrap)
    {
        // FLR tmpR1, fragment.position;
        UtilOpDst(&Op, RND_GPU_PROG_OPCODE_flr, TmpR1, compXYZW, dtU32);
        UtilOpInReg(&Op, pOp->InReg[pOp->LwrInReg-1]);
        CommitInstText(&Op, "Get integer (x,y,z) coords");

        // All types of texture access must use component .x
        // MOD tmpR1.x, tmpR1.x, tmpR0.x;
        UtilOpDst(&Op, RND_GPU_PROG_OPCODE_mod, TmpR1, compX,dtU32);
        UtilOpInReg(&Op, TmpR1, scX);
        UtilOpInReg(&Op, TmpR0, scX);
        CommitInstText(&Op,"Wrap x coord around texture size");

        //See if we need to colwert .y component
        if (pOp->TxAttr & (Is2d | Is3d | IsLwbe | IsArray2d | IsArrayLwbe |
                           Is2dMultisample))
        {
            // MOD tmpR1.y, tmpR1.y, tmpR0.y;
            UtilOpDst(&Op,RND_GPU_PROG_OPCODE_mod,TmpR1,compY,dtU32);
            UtilOpInReg(&Op,TmpR1,scY);
            UtilOpInReg(&Op,TmpR0,scY);
            CommitInstText(&Op,"Wrap y coord around texture size");
        }
        // Figure out what to do with the .z component
        if (pOp->TxAttr & Is3d)
        {
            // MOD tmpR1.z, tmpR1.z, tmpR0.z;
            UtilOpDst(&Op,RND_GPU_PROG_OPCODE_mod,TmpR1,compZ,dtU32);
            UtilOpInReg(&Op,TmpR1,scZ);
            UtilOpInReg(&Op,TmpR0,scZ);
            CommitInstText(&Op,"Wrap z coord");
        }
        else if (pOp->TxAttr & IsLwbe)
        {
            // Modulo the position.x by the number of faces in a lwbe.
            // MOD tmpR1.z, tmpR1.y, 6;
            UtilOpDst(&Op,RND_GPU_PROG_OPCODE_mod,TmpR1,compZ,dtU32);
            UtilOpInReg(&Op,TmpR1,scY);
            UtilOpInConstU32(&Op, 6);
            CommitInstText(&Op,"Mod x coord by the number of faces in a lwbe");
        }
        else if (pOp->TxAttr & IsArray1d)
        {
            //The .y compontent of TXQ contains the number of layers. Need to
            // modulo it against a random number.
            // MOD tmpR0.z, tmpR0.y, random_value;
            UtilOpDst(&Op,RND_GPU_PROG_OPCODE_mod,TmpR1,compZ,dtU32);
            UtilOpInReg(&Op,TmpR0,scY);
            UtilOpInConstU32(&Op, m_pFpCtx->Rand.GetRandom(0,100));
            CommitInstText(&Op,"Mod a random number against number of layers");
        }
        else if (pOp->TxAttr & (IsArray2d | IsArrayLwbe))
        {
            if (pOp->TxAttr & IsArrayLwbe)
            {
                // The .z component of TXQ holds the number of layers but we
                // need to multiply it by the number of faces(6) and use that
                // as the modulo value.
                // MUL TmpR1.z, TmpR1.z, 6;
                UtilOpDst(&Op,RND_GPU_PROG_OPCODE_mul, TmpR0,compZ,dtU32);
                UtilOpInReg(&Op,TmpR0,scZ);
                UtilOpInConstU32(&Op, 6);
                CommitInstText(&Op,"Compute tot. layer/face from TXQ");
            }

            //z component from TXQ holds the number of layers(or layers*faces)
            //MOD tmpR1.z, tmpR1.x, tmpR0.z;
            UtilOpDst(&Op,RND_GPU_PROG_OPCODE_mod,TmpR1,compZ,dtU32);
            UtilOpInReg(&Op,TmpR1,scY);
            UtilOpInReg(&Op,TmpR0,scZ);
            CommitInstText(&Op,"Mod x coord against tot num layers");
        }

        // Use new scaled value for the texcoord reg.
        pOp->InReg[pOp->LwrInReg-1].Reg = TmpR1;
    }
    else if(scaleMethod == smLinear)   // scale
    {
        //I2F R0, R0;
        UtilOpDst(&Op, RND_GPU_PROG_OPCODE_i2f, TmpR0, compXYZW, dtNone);
        UtilOpInReg(&Op, TmpR0);
        CommitInstText(&Op,"Colwert to float");

        //FRC R1, |texcoord[x]|;
        UtilOpDst(&Op, RND_GPU_PROG_OPCODE_frc, TmpR1, compXYZW, dtNone);
        UtilOpInReg(&Op, pOp->InReg[pOp->LwrInReg-1]);
        Op.InReg[0].Abs = true;
        CommitInstText(&Op, "Normalize tex coord to 0..1");

        if (pOp->TxAttr & IsLwbe)
        {
            // MOV R0.z, 6;
            UtilOpDst(&Op, RND_GPU_PROG_OPCODE_mov, TmpR0, compZ, dtNone);
            UtilOpInConstF32(&Op, 6.0);
            CommitInstText(&Op,"Setup number of faces.");
        }
        else if (pOp->TxAttr & IsArrayLwbe)
        {
            //MUL R0.z, R0, 6.0;
            UtilOpDst(&Op,RND_GPU_PROG_OPCODE_mul,TmpR0,compZ,dtNone);
            UtilOpInReg(&Op,TmpR0,scZ);
            UtilOpInConstF32(&Op,6.0);
            CommitInstText(&Op,"Totlayers = layers * faces");
        }
        else if (pOp->TxAttr & IsArray1d)
        {
            // The .y compontent from TXQ contains the number of layers.
            // Need to mov it to R0.z prior to scaling.
            // MOV tmpR0.z, tmpR0.y;
            UtilOpDst(&Op,RND_GPU_PROG_OPCODE_mov,TmpR0,compZ,dtNone);
            UtilOpInReg(&Op,TmpR0,scY);
            CommitInstText(&Op,"Mov layers to .z for proper scaling");
        }
        //MUL R0.xyz, R1, R0;
        UtilOpDst(&Op,RND_GPU_PROG_OPCODE_mul,TmpR0,compXYZW,dtNone);
        UtilOpInReg(&Op,TmpR1);
        UtilOpInReg(&Op,TmpR0);
        CommitInstText(&Op,"Scale coords to this texture layer");

        //FLR.U R0.xyz, R0;
        UtilOpDst(&Op,RND_GPU_PROG_OPCODE_flr,TmpR0,compXYZW,dtU32);
        UtilOpInReg(&Op,TmpR0);
        CommitInstText(&Op,"colwert to INT (0..0.9999) all map to zero");

        // Use new scaled value for the texcoord reg.
        pOp->InReg[pOp->LwrInReg-1].Reg = TmpR0;
    }
    else
        MASSERT(!"Unknown opScalingMethod!");
}

//-----------------------------------------------------------------------------
// Get the input/output registers for a property that can be passed-through this
// shader.
const GpuPgmGen::PassthruRegs & GpuPgmGen::GetPassthruReg
(
    GLRandom::ProgProperty Prop
)
{
    int i = 0;
    while(m_GS.PassthruRegs[i].Prop != Prop &&
          m_GS.PassthruRegs[i].Prop != ppIlwalid)
    {
        i++;
    }
    return m_GS.PassthruRegs[i];
}

//-----------------------------------------------------------------------------
// Run through the output registers. If any of them are being used for an
// operation that requires this register to be passed-through this shader
// ie. multisample, then issue a MOV instruction to pass this register through
// undisturbed.
void GpuPgmGen::UtilProcessPassthruRegs()
{
    for (PgmRequirements::PropMapItr iter = m_GS.Prog.PgmRqmt.Outputs.begin();
         iter != m_GS.Prog.PgmRqmt.Outputs.end();
         iter++)
    {
        if (iter->second & omPassThru)
        {
            const PassthruRegs &Reg = GetPassthruReg(iter->first);
            if(Reg.Prop != ppIlwalid)
            {
                // build a simple pass-thru instruction
                // ie. MOV result.texcoord[i], vertex[x].texcoord[i];
                UtilCopyReg(Reg.OutReg, Reg.InReg);

                if (!IsPropertyBuiltIn(static_cast<GLRandom::ProgProperty>(Reg.Prop)))
                {
                    // update the program requirements
                    m_GS.Prog.PgmRqmt.Inputs[iter->first] = compXYZW + omPassThru;
                }
            }
            else
            {
                MASSERT(!"Invalid passthru attrib. on output register");
            }
        }
    }
}

//-----------------------------------------------------------------------------
//! Generate an empty placeholder program.
//! Used for strict-linking mode to skip a pgmKind in a chain picked at
//! restart-time.
void GpuPgmGen::GenerateDummyProgram(Programs * pProgramStorage, GLuint target)
{
    Program prog;
    prog.Target = target;
    pProgramStorage->AddProg(psRandom, prog);
}

//-----------------------------------------------------------------------------
// struct RegState methods.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//! Set initial Score, Readable, Written masks.
void GpuPgmGen::RegState::InitReadOnlyReg (int score, int mask)
{
    MASSERT(WriteableMask == 0);

    WrittenMask  |= mask;
    ReadableMask |= mask;
    for (int ci = 0; ci < 4; ci++)
    {
        if (mask & (1<<ci))
            Score[ci] = score;
    }
}

//-----------------------------------------------------------------------------
void GpuPgmGen::RegState::InitIndexReg()
{
    Attrs |= (regTemp | regIndex);
    WriteableMask = compXYZW;
    ReadableMask  = compXYZW;
    // Because of the special handling in ArrayVarInitSequence,
    // we can assume this reg is pre-written.
    // Leave Scores at 0 so the reg won't be read except for indexing, because
    // we declare it as INT and will get type errors in many cases.
    WrittenMask = compXYZW;
}

//-----------------------------------------------------------------------------
// class Block methods.
//-----------------------------------------------------------------------------

/* class static data */
const GpuPgmGen::RegState GpuPgmGen::Block::s_OutOfRangeVar (GpuPgmGen::RegState::IlwalidId);

//-----------------------------------------------------------------------------
GpuPgmGen::Block::Block
(
    const GpuPgmGen::Block * parent
)
:   m_Parent(parent)
   ,m_Indent(parent ? (parent->m_Indent+1) : 0)
   ,m_TargetStatementCount(0)
   ,m_CCValidMask(parent ? parent->m_CCValidMask : 0)
   ,m_StatementCount(0)
   ,m_UsesTXD(false)
{
}

//-----------------------------------------------------------------------------
GpuPgmGen::Block::~Block()
{
}

//-----------------------------------------------------------------------------
const GpuPgmGen::Block & GpuPgmGen::Block::operator=
(
    const GpuPgmGen::Block &rhs
)
{
    if (this != &rhs)
    {
        m_Parent            = rhs.m_Parent;
        m_Vars              = rhs.m_Vars;
        // const m_Indent
        m_TargetStatementCount = rhs.m_TargetStatementCount;
        m_CCValidMask       = rhs.m_CCValidMask;
        m_ProgStr           = rhs.m_ProgStr;
        m_StatementCount    = rhs.m_StatementCount;
        m_UsesTXD           = rhs.m_UsesTXD;
    }
    return *this;
}

//-----------------------------------------------------------------------------
GpuPgmGen::Block& GpuPgmGen::Block::operator=(GpuPgmGen::Block&& rhs)
{
    m_Parent               = rhs.m_Parent;
    m_Vars                 = move(rhs.m_Vars);
    // const int m_Indent;
    m_TargetStatementCount = rhs.m_TargetStatementCount;
    m_CCValidMask          = rhs.m_CCValidMask;
    m_ProgStr              = move(rhs.m_ProgStr);
    m_StatementCount       = rhs.m_StatementCount;
    m_UsesTXD              = rhs.m_UsesTXD;
    return *this;
}

//-----------------------------------------------------------------------------
int GpuPgmGen::Block::NumVars() const
{
    if (m_Parent)
    {
        // Cascade up to the outermost Block (which is main) to get
        // the size of the definitive list containing all vars.
        return m_Parent->NumVars();
    }
    else
    {
        // This Block is main.
        return static_cast<UINT32>(m_Vars.size());
    }
}

//-----------------------------------------------------------------------------
void GpuPgmGen::Block::SetTargetStatementCount (int tgtStatementCount)
{
    m_TargetStatementCount = tgtStatementCount;
}

//-----------------------------------------------------------------------------
//! Add a single instruction line, indented and with a newline.
//! Tracks how many instructions the Block contains.
void GpuPgmGen::Block::AddStatement
(
    const string & line
    ,bool bCountStatement
)
{
    m_ProgStr.append(m_Indent+1, ' ');
    m_ProgStr.append(line);
    m_ProgStr.append(1, '\n');

    if (bCountStatement)
    {
        m_StatementCount++;
    }
}

//-----------------------------------------------------------------------------
void GpuPgmGen::Block::AddStatements (const Block & child)
{
    m_ProgStr.append(child.ProgStr());
    m_StatementCount += child.StatementCount();
}

//-----------------------------------------------------------------------------
const string & GpuPgmGen::Block::ProgStr() const
{
    return m_ProgStr;
}

//-----------------------------------------------------------------------------
int GpuPgmGen::Block::StatementCount() const
{
    return m_StatementCount;
}

//-----------------------------------------------------------------------------
int GpuPgmGen::Block::StatementsLeft() const
{
    return max(0, m_TargetStatementCount - m_StatementCount);
}

//-----------------------------------------------------------------------------
const GpuPgmGen::RegState & GpuPgmGen::Block::GetVar (int id) const
{
    ConstVarIter iter = m_Vars.find(id);

    if (iter == m_Vars.end())
    {
        if (m_Parent)
        {
            // No local version, cascade to outer block.
            return m_Parent->GetVar(id);
        }
        else
        {
            // No local version, and no parent!
            MASSERT(!"Invalid REG/VAR id!");

            return s_OutOfRangeVar;
        }
    }
    return iter->second;
}

//-----------------------------------------------------------------------------
//! Add a var (should only be called on the outermost (main) Block.
void GpuPgmGen::Block::AddVar
(
    const GpuPgmGen::RegState & v
)
{
    // AddVar should only be called on the main Block.
    MASSERT(0 == m_Parent);

    // AddVar should only be called once for a given Id.
    MASSERT(m_Vars.end() == m_Vars.find(v.Id));

    // AddVar is always called in order from Id 0 to Id max.
    MASSERT(v.Id == (int) m_Vars.size());

    m_Vars[v.Id] = v;
}

//-----------------------------------------------------------------------------
//! Update a var (only affects innermost Block).
void GpuPgmGen::Block::UpdateVar
(
    const RegState & v
)
{
    const bool oldvFromParent = m_Vars.find(v.Id) == m_Vars.end();
    const RegState & oldv = GetVar(v.Id);

#ifdef DEBUG

    // RegState::Id should always range from 0 to NumVars-1.
    MASSERT(v.Id < NumVars());

    // These parts of the RegState must not change in UpdateVar:

    const int ConstAttrs = regTemp | regIn | regOut | regIndex;

    MASSERT(oldv.ReadableMask == v.ReadableMask);
    MASSERT((oldv.Attrs & ConstAttrs) == (v.Attrs & ConstAttrs));

    // These parts may be set but must not be cleared:

    const int SetOnlyAttrs = regUsed;

    MASSERT((oldv.WrittenMask & v.WrittenMask) == oldv.WrittenMask);
    MASSERT((oldv.Attrs & v.Attrs & SetOnlyAttrs) == (oldv.Attrs & SetOnlyAttrs));

    // TBD: data-type
#endif

    // Maintain a mask of "modified bits in this RegState"
    // for this block.  We'll need it later in the Incorporate methods.
    //
    // If we are just now adding this RegState to this Block,
    // we start with a blank BlockMask.  Otherwise we start
    // with the BlockMask from previous UpdateVar calls in this Block
    // for this RegState.
    RegState tmpv = v;
    tmpv.BlockMask = oldvFromParent ? 0 : oldv.BlockMask;

    // Set block-modified based on this new change to WrittenMask.
    tmpv.BlockMask |= (compXYZW & (oldv.WrittenMask ^ v.WrittenMask));

    // Set block-modified based on this new change to Score.
    for (int ic = 0; ic < 4; ic++)
    {
        if (v.Score[ic] != oldv.Score[ic])
            tmpv.BlockMask |= (1<<ic);
    }

    m_Vars[v.Id] = tmpv;
}

//-----------------------------------------------------------------------------
// Because this variable might be used we can only update the regUsed attribute.
// We cant update the written mask or the BlockMask.
void GpuPgmGen::Block::UpdateVarMaybe(const RegState& v)
{
    // RegState::Id should always range from 0 to NumVars-1.
    MASSERT(v.Id < NumVars());

    // Start with parent's VarState, not child's.
    RegState tmpVar(this->GetVar(v.Id));

    // Since a child used this var, we'll need to declare it.
    tmpVar.Attrs |= regUsed;

    // TBD: If a data-type changed, we can't assume we know the type anymore.

    UpdateVar(tmpVar);
}

//-----------------------------------------------------------------------------
int GpuPgmGen::Block::CCValidMask() const
{
    return m_CCValidMask;
}

//-----------------------------------------------------------------------------
void GpuPgmGen::Block::UpdateCCValidMask
(
    int newBits
)
{
    m_CCValidMask |= newBits;
}

//-----------------------------------------------------------------------------
bool GpuPgmGen::Block::UsesTXD() const
{
    return m_UsesTXD;
}

//-----------------------------------------------------------------------------
void GpuPgmGen::Block::SetUsesTXD()
{
    m_UsesTXD = true;
}

//-----------------------------------------------------------------------------
//! Incorporate VarState updates from an inner block into this
//! outer block.
void GpuPgmGen::Block::IncorporateInnerBlock
(
    const Block & child
    ,Called c
)
{
    ConstVarIter it;
    for (it = child.m_Vars.begin(); it != child.m_Vars.end(); ++it)
    {
        const RegState& childVar = it->second;

        if (c == CalledMaybe)
        {
            UpdateVarMaybe (childVar);
        }
        else
        {
            RegState tmpv = GetVar(childVar.Id);
            for (int ic = 0; ic < 4; ic++)
            {
                const int comp = (1<<ic);

                if (childVar.BlockMask & comp)
                {
                    // Child modified this component.
                    tmpv.Score[ic] = childVar.Score[ic];
                    tmpv.WrittenMask = (tmpv.WrittenMask & ~comp) |
                                       (childVar.WrittenMask & comp);
                }
            }
            tmpv.Attrs |= regUsed;
            UpdateVar (tmpv);
        }
    }

    if (c == CalledForSure)
        UpdateCCValidMask (child.CCValidMask());

    if (child.UsesTXD())
        SetUsesTXD();
}

//-----------------------------------------------------------------------------
//! Incorporate VarState updates from a pair of inner blocks into this
//! outer block.
void GpuPgmGen::Block::IncorporateIfElseBlocks
(
    const Block & childA,
    const Block & childB
)
{
    ConstVarIter itA  = childA.m_Vars.begin();
    ConstVarIter endA = childA.m_Vars.end();
    ConstVarIter itB  = childB.m_Vars.begin();
    ConstVarIter endB = childB.m_Vars.end();

    while (itA != endA || itB != endB)
    {
        if (itA == endA)
        {
            // Only childB has vars left.
            UpdateVarMaybe(itB->second);
            ++itB;
        }
        else if (itB == endB)
        {
            // Only childA has vars left.
            UpdateVarMaybe(itA->second);
            ++itA;
        }
        else if (itB->first < itA->first)
        {
            // Different vars, childB's has lower Id.
            UpdateVarMaybe(itB->second);
            ++itB;
        }
        else if (itA->first < itB->first)
        {
            // Different vars, childA's has lower Id.
            UpdateVarMaybe(itA->second);
            ++itA;
        }
        else
        {
            // Same var modified in both children.
            const RegState & vA = itA->second;
            const RegState & vB = itB->second;

            // If data type changed, TBD

            RegState tmpVar = this->GetVar(vA.Id);

            const int bothWroteMask = (vA.BlockMask & vA.WrittenMask) &
                                      (vB.BlockMask & vB.BlockMask);

            for (int ci = 0; ci < 4; ci++)
            {
                const int comp = (1<<ci);
                if (bothWroteMask & comp)
                {
                    tmpVar.WrittenMask |= comp;
                    tmpVar.Score[ci] = (vA.Score[ci] + vB.Score[ci] + 1)/2;
                }
            }
            tmpVar.Attrs |= regUsed;
            UpdateVar(tmpVar);
            ++itA;
            ++itB;
        }
    }

    UpdateCCValidMask (childA.CCValidMask() & childB.CCValidMask());

    if (childA.UsesTXD() || childB.UsesTXD())
        SetUsesTXD();
}

//-----------------------------------------------------------------------------
int GpuPgmGen::Block::NumRqdUnwritten() const
{
    int rqdUnw = 0;

    const int lwar = NumVars();

    for (int iVar = 0; iVar < lwar; iVar++)
    {
        const RegState & v = GetVar(iVar);

        if ((v.Attrs & regRqd) && v.UnWrittenMask())
            rqdUnw++;
    }
    return rqdUnw;
}

//-----------------------------------------------------------------------------
void GpuPgmGen::Block::MarkAllOutputsUnwritten()
{
    const int lwar = NumVars();

    for (int iVar = 0; iVar < lwar; iVar++)
    {
        RegState v = GetVar(iVar);

        if (v.Attrs & regOut)
        {
            v.WrittenMask = 0;
            m_Vars[v.Id] = v;
        }
    }
}

//-----------------------------------------------------------------------------
// struct Subroutine methods.
//-----------------------------------------------------------------------------

GpuPgmGen::Subroutine::Subroutine
(
    const GpuPgmGen::Block * outer
    ,int subNum
)
:   m_Block(outer)
   ,m_Name(Utility::StrPrintf("Sub%d", subNum))
   ,m_IsCalled(false)
{
}

//-----------------------------------------------------------------------------
GpuPgmGen::Subroutine::Subroutine
(
    const GpuPgmGen::Block * outer
    ,const char * name
)
:   m_Block(outer)
   ,m_Name(name)
   ,m_IsCalled(false)
{
}
//-----------------------------------------------------------------------------
// struct RegUse methods.
//-----------------------------------------------------------------------------
void GpuPgmGen::RegUse::Init()
{
    Reg          = RND_GPU_PROG_REG_none;
    IndexReg     = 0;
    IndexRegComp = 0;
    IndexOffset  = 0;
    Swiz[siX]    = scX;
    Swiz[siY]    = scY;
    Swiz[siZ]    = scZ;
    Swiz[siW]    = scW;
    Abs          = false;
    Neg          = false;
    SwizCount    = 0;
    LiteralVal.Reset();
}

//-----------------------------------------------------------------------------
// class PgmInstruction methods.
//-----------------------------------------------------------------------------
void GpuPgmGen::PgmInstruction::Init(GenState * pGenState, int opId)
{
    MASSERT(pGenState != nullptr || (pGenState == nullptr && opId == 0));

    pGS         = pGenState;
    OpId        = opId;
    pDef        = pGS ? &pGS->Ops[opId] : nullptr;
    LwrInReg    = 0;
    OpModifier  = smNone;
    srcDT       = dtNone;
    dstDT       = dtNone;

    OutReg.Init();

    OutMask  = omXYZW;
    OutTest  = RND_GPU_PROG_OUT_TEST_none;
    CCtoRead = RND_GPU_PROG_CCnone;

    CCSwiz[siX] = scX;
    CCSwiz[siY] = scY;
    CCSwiz[siZ] = scZ;
    CCSwiz[siW] = scW;
    CCSwizCount = 0;

    ExtSwiz[siX] = scX;
    ExtSwiz[siY] = scY;
    ExtSwiz[siZ] = scZ;
    ExtSwiz[siW] = scW;
    ExtSwizCount = 0;

    for (unsigned i = 0; i < MaxIn; i++)
        InReg[i].Init();

    TxFetcher   = -1;
    TxAttr      = 0;
    TxOffsetFmt = toNone;
    TxOffsetReg = 0;
    TxOffset[0] = 0;
    TxOffset[1] = 0;
    TxOffset[2] = 0;
    TxOffset[3] = 0;
    TxgTxComponent = -1;
    ForceRqdWrite= false;
    IU.unit   = -1;
    IU.mmLevel= -1;
    IU.access = GLRandom::iuaRead;
    IU.elems  = 0;
    UsrComment = "";
}
//-----------------------------------------------------------------------------
// class PgmInstruction accessor methods into the s_GpuOpData4 for this Op.
//-----------------------------------------------------------------------------
const char * GpuPgmGen::PgmInstruction::GetName() const
{
    if(pDef)
    {
        return pDef->Name;
    }
    MASSERT(!"PgmInstruction not initialized before use!");
    return "Unknown";
}
//-----------------------------------------------------------------------------
int GpuPgmGen::PgmInstruction::GetOpInstType() const
{
    if(pDef)
    {
        return pDef->OpInstType;
    }
    MASSERT(!"PgmInstruction not initialized before use!");
    return oitUnknown;
}
//-----------------------------------------------------------------------------
int GpuPgmGen::PgmInstruction::GetExtNeeded(GLRandom::pgmKind kind) const
{
    if(pDef)
    {
        return ((kind == GLRandom::pkVertex) ?
                pDef->ExtNeeded[0] : pDef->ExtNeeded[1]);
    }
    MASSERT(!"PgmInstruction not initialized before use!");
    return GLRandomTest::ExtLW_gpu_program4;
}
//-----------------------------------------------------------------------------
int GpuPgmGen::PgmInstruction::GetNumArgs() const
{
    if(pDef)
    {
        return pDef->NumArgs;
    }
    MASSERT(!"PgmInstruction not initialized before use!");
    return 0;
}
//-----------------------------------------------------------------------------
int GpuPgmGen::PgmInstruction::GetArgType(int idx) const
{
    if(pDef)
    {
        return pDef->ArgType[idx];
    }
    MASSERT(!"PgmInstruction not initialized before use!");
    return atNone;
}
//-----------------------------------------------------------------------------
int GpuPgmGen::PgmInstruction::GetResultType() const
{
    if(pDef)
    {
        return pDef->ResultType;
    }
    MASSERT(!"PgmInstruction not initialized before use!");
    return atNone;
}
//-----------------------------------------------------------------------------
int GpuPgmGen::PgmInstruction::GetResultMask() const
{
    if(pDef)
    {
        int outMask = pDef->ResultMask;

        if (pDef->OpInstType == oitTxq)
        {
            if (TxAttr & (Is1d | IsShadow1d))
            {
                outMask = omX;
            }
            else if (TxAttr & (Is2d | IsLwbe | IsShadow2d | IsShadowLwbe |
                               IsArray1d | IsShadowArray1d))
            {
                outMask = omXY;
            }
            else if (TxAttr & (Is3d | IsArray2d | IsArrayLwbe |
                               IsShadowArray2d | IsShadowArrayLwbe))
            {
                outMask = omXYZ;
            }
            else
            {
                MASSERT(!"Unexpected texture dim");
            }
        }
        return outMask;
    }
    MASSERT(!"PgmInstruction not initialized before use!");
    return omNone;
}
//-----------------------------------------------------------------------------
int GpuPgmGen::PgmInstruction::GetOpModMask() const
{
    if(pDef)
    {
        return pDef->OpModMask;
    }
    MASSERT(!"PgmInstruction not initialized before use!");
    return smNA;
}

//-----------------------------------------------------------------------------
// Standard instructions have a single datatype mask contained in the
// PgmInstruction. However special instructions require the mask to be determined
// during runtime may have multiple mutually exclusive masks for valid data types.
void GpuPgmGen::PgmInstruction::GetTypeMasks
(
    GLRandomTest *pGLRandom
    ,vector <GLRandom::opDataType>* pOpMasks
)const
{
    if (pDef)
    {
        if (pDef->TypeMask & dtRun)
        {
            return(GetDynamicTypeMasks(pGLRandom,pOpMasks));
        }
        else
        {
            //LW_gpu_program5 has FP16 (not U16/I16) support, but earlier extensions don't
            if (!pGLRandom->HasExt(GLRandomTest::ExtLW_gpu_program5))
            {
                UINT32 tmp = pDef->TypeMask;
                tmp &= ~(int)(dt16f);
                pOpMasks->push_back((GLRandom::opDataType)tmp);
            }
            else
            {
                pOpMasks->push_back(pDef->TypeMask);
            }
        }
    }
    else
    {
        MASSERT(!"PgmInstruction not initialized before use!");
        pOpMasks->push_back(GLRandom::dtNA);
    }
    return;
}

//-----------------------------------------------------------------------------
//Note: This API does not take into account the requirements of the ATOMS/ATOMB
// instructions. Lwrrently it only accounts for ATOM/ATOMIM.
void GpuPgmGen::PgmInstruction::GetDynamicTypeMasks
(
    GLRandomTest *pGLRandom
    ,vector <GLRandom::opDataType>* pOpMasks
) const
{
    // Some ops will dynamically change the typemask depending on other
    // cirlwmstances. ie ATOM & ATOMIM have unique modifers that change
    // depending on the dataType modifiers.
    if (pGLRandom->HasExt(GLRandomTest::ExtLW_shader_atomic_fp16_vector))
    {
        // This mask is mutually exclusive to the mask in the lower switch statement.
        // ie. Either dtF16X2 or dtF16X4 or dtS32 or dtU32 but not
        // (dtF32 or dtF32X2 or dtF32X4)    ATOM(add/exch)    ATOMIM(add/exch)  dataType
        //       x x             x       x  yes                 yes             dtF16X2, dtF16X4
        //                 x         x x x  yes                 yes             dtS32,dtU32,dtF32
        //               x           x x x  yes                 no              dtS64,dtU64,dtF64
        // - - - -|- - - -|- - - -|- - - -
        // d d d d d d d d d d d d d d d d
        // t t t t t t t t t t t t t t t t
        // R   a X X 1 H 6 3 3 2 1 8 U S F
        //       4 2 6 i 4 2 2 4 6
        //           f 3   e
        //             2
        switch((OpModifier >> 16) & 0xf)
        {
            case amAdd:
            case amMin:
            case amMax:
            case amExch:
                pOpMasks->push_back((opDataType)(dtF16X2a));
                pOpMasks->push_back((opDataType)(dtF16X4a));
                break;
            default:
                break;
        }
    }
    // process 64bit capabilities
    switch ((OpModifier >> 16) & 0xf)
    {
        case amAdd:
        case amExch:
        case amMin:
        case amMax:
        case amAnd:
        case amOr:
        case amXor:
        case amCSwap:
            if (OpId == RND_GPU_PROG_FR_OPCODE_atom)
            {
                UINT32 dt = dtNone;    //
                if (pGLRandom->HasExt(GLRandomTest::ExtLW_shader_atomic_int64))
                {
                    dt |= (dtS64 | dtU64);
                }
                if (pGLRandom->HasExt(GLRandomTest::ExtLW_shader_atomic_float64))
                {
                    // F64 supported only for atomic ADD/EXCH
                    if (((OpModifier >> 16) & 0xf) == amAdd || ((OpModifier >> 16) & 0xf) == amExch)
                    {
                        dt |= dtF64;
                    }
                }
                if (dt != dtNone)
                {
                    pOpMasks->push_back((opDataType)dt);
                }
            }
            break;
        default:
            break;
    }

    // 32 bit capabilities
    switch ((OpModifier >> 16) & 0xf)
    {
        case amAdd:
        case amExch:
        {
            UINT32 dt = dtS32e | dtU32e;    //
            if (pGLRandom->HasExt(GLRandomTest::ExtLW_shader_atomic_float))
            {
                dt |= dtF32e;
            }
            pOpMasks->push_back((opDataType)(dt));
            break;
        }
        case amCSwap:
        case amMin:
        case amMax:
        case amAnd:
        case amOr:
        case amXor:
            pOpMasks->push_back((opDataType)(dtS32e | dtU32e));
            break;
        case amIwrap:
        case amDwrap:
            pOpMasks->push_back((opDataType)(dtU32e));
            break;
        case amNone:
        default:    // no idea so just return the default type.
            pOpMasks->push_back(pDef->DefaultType);
            break;
    }
    return;
}

//-----------------------------------------------------------------------------
GLRandom::opDataType GpuPgmGen::PgmInstruction::GetDefaultType() const
{
    if(pDef)
    {
        return pDef->DefaultType;
    }
    MASSERT(!"PgmInstruction not initialized before use!");
    return GLRandom::dtNA;

}
//-----------------------------------------------------------------------------
bool GpuPgmGen::PgmInstruction::GetIsTx() const
{
    if(pDef)
    {
        return pDef->IsTx;
    }
    MASSERT(!"PgmInstruction not initialized before use!");
    return false;
}
//-----------------------------------------------------------------------------
int GpuPgmGen::PgmInstruction::GetScoreMult() const
{
    if(pDef)
    {
        return pDef->ScoreMult;
    }
    MASSERT(!"PgmInstruction not initialized before use!");
    return 1;
}
//-----------------------------------------------------------------------------
int GpuPgmGen::PgmInstruction::GetScoreAdd()  const
{
    if(pDef)
    {
        return pDef->ScoreAdd;
    }
    MASSERT(!"PgmInstruction not initialized before use!");
    return 0;
}

//-----------------------------------------------------------------------------
// This function is handy in finding workarounds for GL and OCG bugs.
bool GpuPgmGen::PgmInstruction::HasComplexLwinstExpansion() const
{
    if (OpId == RND_GPU_PROG_OPCODE_txd)
    {
        // The TXD instruction is public enemy #1 for finding GL/OCG bugs.
        return true;
    }

    if (GetIsTx())
    {
        // Texture instructions in general are bug-prone.
        return true;
    }

    if (!(dstDT & dtF))
    {
        if (dstDT & dt64)
        {
            // 64-bit integer ops are not supported in HW (Fermi).
            return true;
        }
        if ((OpId == RND_GPU_PROG_OPCODE_rcp) ||
            (OpId == RND_GPU_PROG_OPCODE_div))
        {
            // Integer divide and reciprocal are not supported in HW (Fermi).
            return true;
        }
    }
    // Most likely this instruction translates into a single lwInst instruction.
    // If this kind of instruction asserts, we probably can't WAR the bug.
    return false;
}
//-----------------------------------------------------------------------------
void GpuPgmGen::PgmInstruction::SetUsrComment(const char * szComment)
{
    UsrComment = " #";
    UsrComment += szComment;
}
//-----------------------------------------------------------------------------
void GpuPgmGen::PgmInstruction::SetUsrComment(const string & str)
{
    UsrComment = " #";
    UsrComment += str;
}

//-----------------------------------------------------------------------------
/*static*/
GLint GpuPgmGen::LightToRegIdx(GLuint light)
{
    return light * 4 + FirstLightReg;
}

//-----------------------------------------------------------------------------
// Initialize anything that needs to be initialized at the start of a frame.
void GpuPgmGen::Restart()
{
    ShuffleAtomicModifiers();
}

//-----------------------------------------------------------------------------
RC GpuPgmGen::GenerateXfbPassthruProgram
(
    Programs * pProgramStorage
)
{
    return RC::SOFTWARE_ERROR;
}

//----------------------------------------------------------------------------
// Return the byte alignment requirements for memory access op modifiers
int GpuPgmGen::GetMemAccessAlignment(GLRandom::opDataType dt)
{
    switch (dt) // use explicit format
    {
        case dtU8:
        case dtS8:
            return 1;
        case dtF16:
        case dtS16:
        case dtU16:
            return 2;
        case dtS24: // still needs 4byte memory access alignment
        case dtU24: // still needs 4byte memory access alignment

        case dtF32:
        case dtF32e:
        case dtS32:
        case dtS32e:
        case dtS32_HI:
        case dtU32:
        case dtU32e:
        case dtU32_HI:
        case dtF32X2:
        case dtF32X4:
        case dtS32X2:
        case dtS32X4:
        case dtU32X2:
        case dtU32X4:
            return 4;
        case dtF64:
        case dtS64:
        case dtU64:
        case dtF64X2:
        case dtF64X4:
        case dtS64X2:
        case dtS64X4:
        case dtU64X2:
        case dtU64X4:
            return 8;

        default:
            MASSERT(!"Unexpected DataType");
            // fall through
        case dtNone:    return(1);
    }
    // should never get here
    return(1);
}

//-----------------------------------------------------------------------------
int GpuPgmGen::GetTxCdScalingMethod(PgmInstruction *pOp)
{
    // default implementation for vertex/geometry shaders.
    return smLinear;
}

//------------------------------------------------------------------------------
void GpuPgmGen::AddLayeredOutputRegs(int LayerReg, int VportIdxReg, int VportMaskReg)
{
    if (m_pGLRandom->GetNumLayersInFBO() > 0)  //only if we have a layered FBO attached
    {
        // ROUND.U result.layer.x, pgmElw[LayerAndVportIDReg].x;
        PgmInstruction Op;
        UtilOpDst(&Op, RND_GPU_PROG_OPCODE_round, LayerReg, compX, dtU32);
        UtilOpInReg(&Op, GetConstReg(), scX);
        Op.InReg[0].IndexOffset = GpuPgmGen::LayerAndVportIDReg;
        UtilOpCommit(&Op);

        // ROUND.U result.viewport.x/viewportmask[0].x, pgmElw[LayerAndVportIDReg].y;
        if (m_pGLRandom->m_GpuPrograms.GetIsLwrrVportIndexed())
        {
            UtilOpDst(&Op, RND_GPU_PROG_OPCODE_round, VportIdxReg, compX, dtU32);
        }
        else
        {
            UtilOpDst(&Op, RND_GPU_PROG_OPCODE_round, VportMaskReg, compX, dtU32);
        }
        UtilOpInReg(&Op, GetConstReg(), scY);
        Op.InReg[0].IndexOffset = GpuPgmGen::LayerAndVportIDReg;
        UtilOpCommit(&Op);
    }
}
