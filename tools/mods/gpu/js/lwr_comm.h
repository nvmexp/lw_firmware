//#ifndef LWR_COMM_H
//#define LWR_COMM_H
/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2013, 2016, 2018-2021 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file lwr_comm.h
 * @brief Common C++ and JavaScript declarations for Lwca Random
 *        tests.
 */

#if !defined(__cplusplus)
var lwr_commId = "$Id$";
#endif

/** <a name="Indexes"> </a> */
/*
 * Picker Indexes  (JavaScript)
 *
 *
 * These are the legal picker indexes for LwdaRandom.SetPicker(), .GetPicker(),
 * and .SetDefaultPicker().
 * Use them to control the way LwdaRandom does its random picking.
 *
 * EXAMPLES:
 *
 *
 *
 * FancyPicker configuration syntax:  (JavaScript)
 *
 *    [ MODE, OPTIONS, ITEMS ]
 *
 * MODE must be a quoted string, from this list:
 *
 *    "const" "constant"         (always returns same value)
 *    "rand"  "random"           (return a random value)
 *    "list"                     (return next item in a list)
 *    "step"                     (schmoo mode, i.e. begin/step/end )
 *    "js"                       (call a JavaScript function, flexible but slow)
 *
 * OPTIONS must be zero or more quoted strings, from this list:
 *
 *    "PickOnRestart"            (pick on Restart loops, re-use that value for
 *                                all other loops.  Otherwise, we pick a new
 *                                value on each loop.)
 *    "clamp" "wrap"             (for LIST or STEP mode, what to do at the end
 *                                of the sequence: stay at end, or go back to
 *                                the beginning)
 *    "int" "float"              (whether to interpret the numbers as floats or
 *                                as integers.  If you don't specify, uses the
 *                                default value from C++, which is usually best)
 *
 *    1) Capitalization is ignored.
 *    2) If you specify more than one mode, the last one "wins".
 *    3) If you fail to specify any mode at all, the program tries to guess from
 *       the format of ITEMS.
 *    4) You are actually allowed to specify MODE and OPTIONS strings
 *       in any order, as long as they all preceed the ITEMS.
 *
 * ITEMS depends on what mode you are in:
 *
 *    CONST ITEM     must be a single number.
 *
 *    RANDOM ITEMS   must be a sequence of one or more arrays, each like this:
 *                      [ relative_probability, value ]     or
 *                      [ relative_probability, min, max ]
 *
 *    LIST ITEMS     must be a sequence of one or more numbers or arrays:
 *                      value  or  [ value ]  or  [ repeat_count, value ]
 *
 *    STEP ITEMS     must be three numbers:  begin, step, end
 *
 *    JS ITEMS       must be one or two javascript function names:
 *                      pick_func  or  pick_func, restart_func
 *
 * Picker Indexes  (C++)
 *
 *
 * We keep a separate array of Utility::FancyPicker objects in each
 * randomizer module.
 *
 * In C++, strip away the base offset, i.e. (index % RND_BASE_OFFSET).
 *
 * We set default values from C++, but the JavaScript settings override them.
 */
#define CRTST_CTRL_KERNEL                       0   // which kernel to launch for a given tile
#define CRTST_CTRL_KERNEL_SeqMatrix             0   // kernel that performs a series of arithmetic operations on matrices
#define CRTST_CTRL_KERNEL_SeqMatrixDbl          1   // kernel that performs a series of double precision arithmetic operations on matrices
#define CRTST_CTRL_KERNEL_SeqMatrixStress       2   // a more stressful kernel that performs a series of arithmetic operations on matrices
#define CRTST_CTRL_KERNEL_SeqMatrixDblStress    3   // a more stressful kernel that performs a series of double precision arithmetic operations on matrices
#define CRTST_CTRL_KERNEL_SeqMatrixInt          4   // Tests integer operatons that don't mix well with floating point operations
#define CRTST_CTRL_KERNEL_Stress                5   // a more stressful kernel that mimics the workload of LwdaStress2
#define CRTST_CTRL_KERNEL_SmallDebug            6   // very small kernel for debugging
#define CRTST_CTRL_KERNEL_Branch                7   // Test divergent branching
#define CRTST_CTRL_KERNEL_Texture               8   // Test texture operations
#define CRTST_CTRL_KERNEL_Surface               9   // Test surface operations
#define CRTST_CTRL_KERNEL_MatrixInit            10  // Test matrix init on GPU
#define CRTST_CTRL_KERNEL_Atom                  11  // Test atom and red.
#define CRTST_CTRL_KERNEL_GpuWorkCreation       12  // Test kernel that launches other kernels
#define CRTST_CTRL_KERNEL_NUM_KERNELS           13  // number of kernels available

#define CRTST_CTRL_SHUFFLE_TILES                1

#define CRTST_CTRL_TILE_WIDTH                   2   // output tile C & input tile B width for a given matrix operation
#define CRTST_CTRL_TILE_HEIGHT                  3   // output tile C & input tile A height for a given matrix operation
#define CRTST_CTRL_THREADS_PER_LAUNCH           4

// Kernel that performs a series of arithmetic operations using two input matrices and one output matrix.
#define CRTST_KRNL_SEQ_MTX_NUM_OPS              5       // number of arithmetic operation to perform on this loop
#define CRTST_KRNL_SEQ_MTX_OP_IDX               6
#define CRTST_KRNL_SEQ_MTX_OPCODE               7
#define SEQ_MTX_OPCODE_BASE                 100
#define CRTST_KRNL_SEQ_MTX_OPCODE_None          (SEQ_MTX_OPCODE_BASE+ 0)    // debug:do nothing
#define CRTST_KRNL_SEQ_MTX_OPCODE_Copy1         (SEQ_MTX_OPCODE_BASE+ 1)    // debug:copy operand1 to output matrix
#define CRTST_KRNL_SEQ_MTX_OPCODE_Copy2         (SEQ_MTX_OPCODE_BASE+ 2)    // debug:copy operand2 to optput matrix
// Compute capability 1.0 intrinsics
#define CRTST_KRNL_SEQ_MTX_OPCODE_Add           (SEQ_MTX_OPCODE_BASE+ 3)
#define CRTST_KRNL_SEQ_MTX_OPCODE_Sub           (SEQ_MTX_OPCODE_BASE+ 4)
#define CRTST_KRNL_SEQ_MTX_OPCODE_Mul           (SEQ_MTX_OPCODE_BASE+ 5)
#define CRTST_KRNL_SEQ_MTX_OPCODE_Mul24         (SEQ_MTX_OPCODE_BASE+ 6)
#define CRTST_KRNL_SEQ_MTX_OPCODE_Sad           (SEQ_MTX_OPCODE_BASE+ 7)
#define CRTST_KRNL_SEQ_MTX_OPCODE_uSad          (SEQ_MTX_OPCODE_BASE+ 8)
#define CRTST_KRNL_SEQ_MTX_OPCODE_Div           (SEQ_MTX_OPCODE_BASE+ 9)
#define CRTST_KRNL_SEQ_MTX_OPCODE_Abs           (SEQ_MTX_OPCODE_BASE+ 10)
#define CRTST_KRNL_SEQ_MTX_OPCODE_Min           (SEQ_MTX_OPCODE_BASE+ 11)
#define CRTST_KRNL_SEQ_MTX_OPCODE_Max           (SEQ_MTX_OPCODE_BASE+ 12)
#define CRTST_KRNL_SEQ_MTX_OPCODE_Sin           (SEQ_MTX_OPCODE_BASE+ 13)
#define CRTST_KRNL_SEQ_MTX_OPCODE_Cos           (SEQ_MTX_OPCODE_BASE+ 14)
#define CRTST_KRNL_SEQ_MTX_OPCODE_Tan           (SEQ_MTX_OPCODE_BASE+ 15)
#define CRTST_KRNL_SEQ_MTX_OPCODE_Log           (SEQ_MTX_OPCODE_BASE+ 16)
#define CRTST_KRNL_SEQ_MTX_OPCODE_Log2          (SEQ_MTX_OPCODE_BASE+ 17)
#define CRTST_KRNL_SEQ_MTX_OPCODE_Log10         (SEQ_MTX_OPCODE_BASE+ 18)
#define CRTST_KRNL_SEQ_MTX_OPCODE_Pow           (SEQ_MTX_OPCODE_BASE+ 19)
#define CRTST_KRNL_SEQ_MTX_OPCODE_Int2Float     (SEQ_MTX_OPCODE_BASE+ 20)
#define CRTST_KRNL_SEQ_MTX_OPCODE_Iadd          (SEQ_MTX_OPCODE_BASE+ 21)
#define CRTST_KRNL_SEQ_MTX_OPCODE_Isub          (SEQ_MTX_OPCODE_BASE+ 22)
#define CRTST_KRNL_SEQ_MTX_OPCODE_Imul          (SEQ_MTX_OPCODE_BASE+ 23)
#define CRTST_KRNL_SEQ_MTX_OPCODE_Idiv          (SEQ_MTX_OPCODE_BASE+ 24)
#define CRTST_KRNL_SEQ_MTX_OPCODE_Imul32i       (SEQ_MTX_OPCODE_BASE+ 25)
#define CRTST_KRNL_SEQ_MTX_OPCODE_Fadd32i       (SEQ_MTX_OPCODE_BASE+ 26)
#define CRTST_KRNL_SEQ_MTX_OPCODE_Fmul32i       (SEQ_MTX_OPCODE_BASE+ 27)
// Compute capability 1.1 intrinsics

// Compute capability 1.2 intrinsics
#define CRTST_KRNL_SEQ_MTX_OPCODE_AtomicAdd     (SEQ_MTX_OPCODE_BASE+ 40)
#define CRTST_KRNL_SEQ_MTX_OPCODE_AtomicSub     (SEQ_MTX_OPCODE_BASE+ 41)
#define CRTST_KRNL_SEQ_MTX_OPCODE_AtomicExch    (SEQ_MTX_OPCODE_BASE+ 42)
#define CRTST_KRNL_SEQ_MTX_OPCODE_AtomicMin     (SEQ_MTX_OPCODE_BASE+ 43)
#define CRTST_KRNL_SEQ_MTX_OPCODE_AtomicMax     (SEQ_MTX_OPCODE_BASE+ 44)
#define CRTST_KRNL_SEQ_MTX_OPCODE_AtomicInc     (SEQ_MTX_OPCODE_BASE+ 45)
#define CRTST_KRNL_SEQ_MTX_OPCODE_AtomicDec     (SEQ_MTX_OPCODE_BASE+ 46)
#define CRTST_KRNL_SEQ_MTX_OPCODE_AtomicCas     (SEQ_MTX_OPCODE_BASE+ 47)
#define CRTST_KRNL_SEQ_MTX_OPCODE_AtomicAnd     (SEQ_MTX_OPCODE_BASE+ 48)
#define CRTST_KRNL_SEQ_MTX_OPCODE_AtomicOr      (SEQ_MTX_OPCODE_BASE+ 49)
#define CRTST_KRNL_SEQ_MTX_OPCODE_AtomicXor     (SEQ_MTX_OPCODE_BASE+ 50)

// Compute capability 1.3 intrinsics
// Opcodes that work on double precision data
#define CRTST_KRNL_SEQ_MTX_OPCODE_fmarn         (SEQ_MTX_OPCODE_BASE+ 60)   // fused multiply-add round to nearest
#define CRTST_KRNL_SEQ_MTX_OPCODE_fmarz         (SEQ_MTX_OPCODE_BASE+ 61)   // fused multiply-add round to zero
#define CRTST_KRNL_SEQ_MTX_OPCODE_fmaru         (SEQ_MTX_OPCODE_BASE+ 62)   // fused multiply-add round up
#define CRTST_KRNL_SEQ_MTX_OPCODE_fmard         (SEQ_MTX_OPCODE_BASE+ 63)   // fused multiply-add round down
#define CRTST_KRNL_SEQ_MTX_OPCODE_addrn         (SEQ_MTX_OPCODE_BASE+ 64)   // add round to nearest
#define CRTST_KRNL_SEQ_MTX_OPCODE_addrz         (SEQ_MTX_OPCODE_BASE+ 65)   // add round to zero
#define CRTST_KRNL_SEQ_MTX_OPCODE_addru         (SEQ_MTX_OPCODE_BASE+ 66)   // add round up
#define CRTST_KRNL_SEQ_MTX_OPCODE_addrd         (SEQ_MTX_OPCODE_BASE+ 67)   // add round down
#define CRTST_KRNL_SEQ_MTX_OPCODE_mulrn         (SEQ_MTX_OPCODE_BASE+ 68)   // multiply round to nearest
#define CRTST_KRNL_SEQ_MTX_OPCODE_mulrz         (SEQ_MTX_OPCODE_BASE+ 69)   // multiply round to zero
#define CRTST_KRNL_SEQ_MTX_OPCODE_mulru         (SEQ_MTX_OPCODE_BASE+ 70)   // multiply round up
#define CRTST_KRNL_SEQ_MTX_OPCODE_mulrd         (SEQ_MTX_OPCODE_BASE+ 71)   // multiply round down

// Compute capability 2.0 intrinsics
// Opcodes that work on double precision data
#define CRTST_KRNL_SEQ_MTX_OPCODE_divrn         (SEQ_MTX_OPCODE_BASE+ 72)   // divide round to nearest
#define CRTST_KRNL_SEQ_MTX_OPCODE_divrz         (SEQ_MTX_OPCODE_BASE+ 73)   // divide round to zero
#define CRTST_KRNL_SEQ_MTX_OPCODE_divru         (SEQ_MTX_OPCODE_BASE+ 74)   // divide round up
#define CRTST_KRNL_SEQ_MTX_OPCODE_divrd         (SEQ_MTX_OPCODE_BASE+ 75)   // divide round down
#define CRTST_KRNL_SEQ_MTX_OPCODE_rcprn         (SEQ_MTX_OPCODE_BASE+ 76)   // recipical round to nearest
#define CRTST_KRNL_SEQ_MTX_OPCODE_rcprz         (SEQ_MTX_OPCODE_BASE+ 77)   // recipical round to zero
#define CRTST_KRNL_SEQ_MTX_OPCODE_rcpru         (SEQ_MTX_OPCODE_BASE+ 78)   // recipical round up
#define CRTST_KRNL_SEQ_MTX_OPCODE_rcprd         (SEQ_MTX_OPCODE_BASE+ 79)   // recipical round down
#define CRTST_KRNL_SEQ_MTX_OPCODE_sqrtrn        (SEQ_MTX_OPCODE_BASE+ 80)   // square-root round to nearest
#define CRTST_KRNL_SEQ_MTX_OPCODE_sqrtrz        (SEQ_MTX_OPCODE_BASE+ 81)   // square-root round to zero
#define CRTST_KRNL_SEQ_MTX_OPCODE_sqrtru        (SEQ_MTX_OPCODE_BASE+ 82)   // square-root round up
#define CRTST_KRNL_SEQ_MTX_OPCODE_sqrtrd        (SEQ_MTX_OPCODE_BASE+ 83)   // square-root round down

//Opcodes that exercise data movement
#define CRTST_KRNL_SEQ_MTX_OPCODE_ldsldu        (SEQ_MTX_OPCODE_BASE+ 84)   // load shared, load uniform
#define CRTST_KRNL_SEQ_MTX_OPCODE_ldu           (SEQ_MTX_OPCODE_BASE+ 85)   // load uniform
#define CRTST_KRNL_SEQ_MTX_OPCODE_ldg           (SEQ_MTX_OPCODE_BASE+ 86)   // load from global memory
#define CRTST_KRNL_SEQ_MTX_OPCODE_shfl          (SEQ_MTX_OPCODE_BASE+ 87)   // data shuffle within threads of a warp

//Opcodes that exercise video instructions
#define CRTST_KRNL_SEQ_MTX_OPCODE_vadd            (SEQ_MTX_OPCODE_BASE+ 90)
#define CRTST_KRNL_SEQ_MTX_OPCODE_vsub            (SEQ_MTX_OPCODE_BASE+ 91)
#define CRTST_KRNL_SEQ_MTX_OPCODE_vabsdiff        (SEQ_MTX_OPCODE_BASE+ 92)
#define CRTST_KRNL_SEQ_MTX_OPCODE_vmin            (SEQ_MTX_OPCODE_BASE+ 93)
#define CRTST_KRNL_SEQ_MTX_OPCODE_vmax            (SEQ_MTX_OPCODE_BASE+ 94)
#define CRTST_KRNL_SEQ_MTX_OPCODE_vshl            (SEQ_MTX_OPCODE_BASE+ 95)
#define CRTST_KRNL_SEQ_MTX_OPCODE_vshr            (SEQ_MTX_OPCODE_BASE+ 96)
#define CRTST_KRNL_SEQ_MTX_OPCODE_vmad            (SEQ_MTX_OPCODE_BASE+ 97)
#define CRTST_KRNL_SEQ_MTX_OPCODE_vset            (SEQ_MTX_OPCODE_BASE+ 98)
#define CRTST_KRNL_SEQ_MTX_OPCODE_vadd2           (SEQ_MTX_OPCODE_BASE+ 99)
#define CRTST_KRNL_SEQ_MTX_OPCODE_vsub2           (SEQ_MTX_OPCODE_BASE+ 100)
#define CRTST_KRNL_SEQ_MTX_OPCODE_vavrg2          (SEQ_MTX_OPCODE_BASE+ 101)
#define CRTST_KRNL_SEQ_MTX_OPCODE_vabsdiff2       (SEQ_MTX_OPCODE_BASE+ 102)
#define CRTST_KRNL_SEQ_MTX_OPCODE_vmin2           (SEQ_MTX_OPCODE_BASE+ 103)
#define CRTST_KRNL_SEQ_MTX_OPCODE_vmax2           (SEQ_MTX_OPCODE_BASE+ 104)
#define CRTST_KRNL_SEQ_MTX_OPCODE_vset2           (SEQ_MTX_OPCODE_BASE+ 105)
#define CRTST_KRNL_SEQ_MTX_OPCODE_vadd4           (SEQ_MTX_OPCODE_BASE+ 106)
#define CRTST_KRNL_SEQ_MTX_OPCODE_vsub4           (SEQ_MTX_OPCODE_BASE+ 107)
#define CRTST_KRNL_SEQ_MTX_OPCODE_vavrg4          (SEQ_MTX_OPCODE_BASE+ 108)
#define CRTST_KRNL_SEQ_MTX_OPCODE_vabsdiff4       (SEQ_MTX_OPCODE_BASE+ 109)
#define CRTST_KRNL_SEQ_MTX_OPCODE_vmin4           (SEQ_MTX_OPCODE_BASE+ 110)
#define CRTST_KRNL_SEQ_MTX_OPCODE_vmax4           (SEQ_MTX_OPCODE_BASE+ 111)
#define CRTST_KRNL_SEQ_MTX_OPCODE_vset4           (SEQ_MTX_OPCODE_BASE+ 112)
#define CRTST_KRNL_SEQ_MTX_OPCODE_prmt            (SEQ_MTX_OPCODE_BASE+ 113)
#define CRTST_KRNL_SEQ_MTX_OPCODE_dp4a            (SEQ_MTX_OPCODE_BASE+ 114)

//Opcodes that exercise texture and surface instructions
#define CRTST_KRNL_TEX_OPCODE_tex1D               (SEQ_MTX_OPCODE_BASE+ 121)
#define CRTST_KRNL_TEX_OPCODE_FIRST               CRTST_KRNL_TEX_OPCODE_tex1D
#define CRTST_KRNL_TEX_OPCODE_tex2D               (SEQ_MTX_OPCODE_BASE+ 122)
#define CRTST_KRNL_TEX_OPCODE_tex3D               (SEQ_MTX_OPCODE_BASE+ 123)
#define CRTST_KRNL_TEX_OPCODE_tex1DL              (SEQ_MTX_OPCODE_BASE+ 124)
#define CRTST_KRNL_TEX_OPCODE_tex2DL              (SEQ_MTX_OPCODE_BASE+ 125)
#define CRTST_KRNL_TEX_OPCODE_tld4                (SEQ_MTX_OPCODE_BASE+ 126)
#define CRTST_KRNL_TEX_OPCODE_texclamphi          (SEQ_MTX_OPCODE_BASE+ 127)
#define CRTST_KRNL_TEX_OPCODE_texclamplo          (SEQ_MTX_OPCODE_BASE+ 128)
#define CRTST_KRNL_TEX_OPCODE_texwraphi           (SEQ_MTX_OPCODE_BASE+ 129)
#define CRTST_KRNL_TEX_OPCODE_texwraplo           (SEQ_MTX_OPCODE_BASE+ 130)
#define CRTST_KRNL_TEX_OPCODE_LAST                CRTST_KRNL_TEX_OPCODE_texwraplo

#define CRTST_KRNL_SURF_OPCODE_suld1DA            (SEQ_MTX_OPCODE_BASE+ 131)
#define CRTST_KRNL_SURF_OPCODE_FIRST              CRTST_KRNL_SURF_OPCODE_suld1DA
#define CRTST_KRNL_SURF_OPCODE_suld2DA            (SEQ_MTX_OPCODE_BASE+ 133)
#define CRTST_KRNL_SURF_OPCODE_LAST               CRTST_KRNL_SURF_OPCODE_suld2DA

//Opcodes that exercise extended precision arithmetic
#define CRTST_KRNL_SEQ_MTX_OPCODE_addcc           (SEQ_MTX_OPCODE_BASE+ 144)
#define CRTST_KRNL_SEQ_MTX_OPCODE_subcc           (SEQ_MTX_OPCODE_BASE+ 145)
#define CRTST_KRNL_SEQ_MTX_OPCODE_madcchi         (SEQ_MTX_OPCODE_BASE+ 146)
#define CRTST_KRNL_SEQ_MTX_OPCODE_madcclo         (SEQ_MTX_OPCODE_BASE+ 147)

//Opcodes that exercise shift instructions
#define CRTST_KRNL_SEQ_MTX_OPCODE_shf             (SEQ_MTX_OPCODE_BASE+ 151)   // funnel shift

// Compute capability 7.3/7.5
#define CRTST_KRNL_SEQ_MTX_OPCODE_Tanhf           (SEQ_MTX_OPCODE_BASE+ 152) // tanh FP32
#define CRTST_KRNL_SEQ_MTX_OPCODE_hTanh           (SEQ_MTX_OPCODE_BASE+ 153) // tanh FP16
#define CRTST_KRNL_SEQ_MTX_OPCODE_h2Tanh          (SEQ_MTX_OPCODE_BASE+ 154) // tanh SIMD FP16
#define CRTST_KRNL_SEQ_MTX_OPCODE_hEx2            (SEQ_MTX_OPCODE_BASE+ 155) // ex2 FP16
#define CRTST_KRNL_SEQ_MTX_OPCODE_h2Ex2           (SEQ_MTX_OPCODE_BASE+ 156) // ex2 SIMD FP16
#define CRTST_KRNL_SEQ_MTX_OPCODE_hSin            (SEQ_MTX_OPCODE_BASE+ 157) // sin FP16
#define CRTST_KRNL_SEQ_MTX_OPCODE_h2Sin           (SEQ_MTX_OPCODE_BASE+ 158) // sin SIMD FP16
#define CRTST_KRNL_SEQ_MTX_OPCODE_hCos            (SEQ_MTX_OPCODE_BASE+ 159) // cos FP16
#define CRTST_KRNL_SEQ_MTX_OPCODE_h2Cos           (SEQ_MTX_OPCODE_BASE+ 160) // cos SIMD FP16
#define CRTST_KRNL_SEQ_MTX_OPCODE_hLg2            (SEQ_MTX_OPCODE_BASE+ 161) // lg2 FP16
#define CRTST_KRNL_SEQ_MTX_OPCODE_h2Lg2           (SEQ_MTX_OPCODE_BASE+ 162) // lg2 SIMD FP16
#define CRTST_KRNL_SEQ_MTX_OPCODE_hSqrt           (SEQ_MTX_OPCODE_BASE+ 163) // sqrt FP16
#define CRTST_KRNL_SEQ_MTX_OPCODE_h2Sqrt          (SEQ_MTX_OPCODE_BASE+ 164) // sqrt SIMD FP16
#define CRTST_KRNL_SEQ_MTX_OPCODE_hRsqrt          (SEQ_MTX_OPCODE_BASE+ 165) // rsqrt FP16
#define CRTST_KRNL_SEQ_MTX_OPCODE_h2Rsqrt         (SEQ_MTX_OPCODE_BASE+ 166) // rsqrt SIMD FP16
#define CRTST_KRNL_SEQ_MTX_OPCODE_hRcp            (SEQ_MTX_OPCODE_BASE+ 167) // rcp FP16
#define CRTST_KRNL_SEQ_MTX_OPCODE_h2Rcp           (SEQ_MTX_OPCODE_BASE+ 168) // rcp SIMD FP16

// Compute capability 8.0
#define CRTST_KRNL_SEQ_MTX_OPCODE_hfmaE8M7        (SEQ_MTX_OPCODE_BASE+ 169) // fused multiply-add for e8m7
#define CRTST_KRNL_SEQ_MTX_OPCODE_h2fmaE8M7       (SEQ_MTX_OPCODE_BASE+ 170) // fused multiply-add for e8m7
#define CRTST_KRNL_SEQ_MTX_OPCODE_hmaxE8M7        (SEQ_MTX_OPCODE_BASE+ 171) // max for e8m7
#define CRTST_KRNL_SEQ_MTX_OPCODE_hminE8M7        (SEQ_MTX_OPCODE_BASE+ 172) // min for e8m7
#define CRTST_KRNL_SEQ_MTX_OPCODE_hmax            (SEQ_MTX_OPCODE_BASE+ 173) // max for half
#define CRTST_KRNL_SEQ_MTX_OPCODE_hmin            (SEQ_MTX_OPCODE_BASE+ 174) // min for half
#define CRTST_KRNL_SEQ_MTX_OPCODE_h2max           (SEQ_MTX_OPCODE_BASE+ 175) // max for half x2
#define CRTST_KRNL_SEQ_MTX_OPCODE_h2min           (SEQ_MTX_OPCODE_BASE+ 176) // min for half x2
#define CRTST_KRNL_SEQ_MTX_OPCODE_hfmaMMA         (SEQ_MTX_OPCODE_BASE+ 177) // fp16x2 fused multiply-add with MMA pipe
#define CRTST_KRNL_SEQ_MTX_OPCODE_hfmaRELU        (SEQ_MTX_OPCODE_BASE+ 178) // fp16 fused multiply-add with ReLU
#define CRTST_KRNL_SEQ_MTX_OPCODE_fmaxNaN         (SEQ_MTX_OPCODE_BASE+ 179) // float max with NaN support
#define CRTST_KRNL_SEQ_MTX_OPCODE_fminNaN         (SEQ_MTX_OPCODE_BASE+ 180) // float min with NaN support
#define CRTST_KRNL_SEQ_MTX_OPCODE_bmmaAnd         (SEQ_MTX_OPCODE_BASE+ 181) // boolean mma and
#define CRTST_KRNL_SEQ_MTX_OPCODE_bmmaXor         (SEQ_MTX_OPCODE_BASE+ 182) // boolean mma xor
#define CRTST_KRNL_SEQ_MTX_OPCODE_clmad           (SEQ_MTX_OPCODE_BASE+ 183) // carryless mult-add (double precision)
#define CRTST_KRNL_SEQ_MTX_OPCODE_scatter         (SEQ_MTX_OPCODE_BASE+ 184) // unpack elements from compressed to sparse vector
#define CRTST_KRNL_SEQ_MTX_OPCODE_gather          (SEQ_MTX_OPCODE_BASE+ 185) // pack elements from sparse to compressed vector
#define CRTST_KRNL_SEQ_MTX_OPCODE_genMetadata     (SEQ_MTX_OPCODE_BASE+ 186) // generate metadata to use with gather/scatter

// Compute capability 8.6
#define CRTST_KRNL_SEQ_MTX_OPCODE_fmaxXorSign     (SEQ_MTX_OPCODE_BASE+ 187) // float max with xor sign
#define CRTST_KRNL_SEQ_MTX_OPCODE_fminXorSign     (SEQ_MTX_OPCODE_BASE+ 188) // float min with xor sign
#define CRTST_KRNL_SEQ_MTX_OPCODE_hmaxXorSign     (SEQ_MTX_OPCODE_BASE+ 189) // max for half with xor sign
#define CRTST_KRNL_SEQ_MTX_OPCODE_hminXorSign     (SEQ_MTX_OPCODE_BASE+ 190) // min for half with xor sign

// compute capability 9.0
#define CRTST_KRNL_SEQ_MTX_OPCODE_h2maxBF16       (SEQ_MTX_OPCODE_BASE+ 191) // max for BF16 x2
#define CRTST_KRNL_SEQ_MTX_OPCODE_h2minBF16       (SEQ_MTX_OPCODE_BASE+ 192) // min for BF16 x2
#define CRTST_KRNL_SEQ_MTX_OPCODE_hfmaRELUBF16    (SEQ_MTX_OPCODE_BASE+ 193) // BF16 fused multiply-add with ReLU
#define CRTST_KRNL_SEQ_MTX_OPCODE_hmaxXorSignBF16 (SEQ_MTX_OPCODE_BASE+ 194) // max for BF16 with xor sign
#define CRTST_KRNL_SEQ_MTX_OPCODE_hminXorSignBF16 (SEQ_MTX_OPCODE_BASE+ 195) // min for BF16 with xor sign
#define CRTST_KRNL_SEQ_MTX_OPCODE_hTanhBF16       (SEQ_MTX_OPCODE_BASE+ 196) // tanh BF16
#define CRTST_KRNL_SEQ_MTX_OPCODE_h2TanhBF16      (SEQ_MTX_OPCODE_BASE+ 197) // tanh SIMD BF16
#define CRTST_KRNL_SEQ_MTX_OPCODE_hEx2BF16        (SEQ_MTX_OPCODE_BASE+ 198) // ex2 BF16
#define CRTST_KRNL_SEQ_MTX_OPCODE_h2Ex2BF16       (SEQ_MTX_OPCODE_BASE+ 199) // ex2 SIMD BF16
#define CRTST_KRNL_SEQ_MTX_OPCODE_hSinBF16        (SEQ_MTX_OPCODE_BASE+ 200) // sin BF16
#define CRTST_KRNL_SEQ_MTX_OPCODE_h2SinBF16       (SEQ_MTX_OPCODE_BASE+ 201) // sin SIMD BF16
#define CRTST_KRNL_SEQ_MTX_OPCODE_hCosBF16        (SEQ_MTX_OPCODE_BASE+ 202) // cos BF16
#define CRTST_KRNL_SEQ_MTX_OPCODE_h2CosBF16       (SEQ_MTX_OPCODE_BASE+ 203) // cos SIMD BF16
#define CRTST_KRNL_SEQ_MTX_OPCODE_hLg2BF16        (SEQ_MTX_OPCODE_BASE+ 204) // lg2 BF16
#define CRTST_KRNL_SEQ_MTX_OPCODE_h2Lg2BF16       (SEQ_MTX_OPCODE_BASE+ 205) // lg2 SIMD BF16
#define CRTST_KRNL_SEQ_MTX_OPCODE_hSqrtBF16       (SEQ_MTX_OPCODE_BASE+ 206) // sqrt BF16
#define CRTST_KRNL_SEQ_MTX_OPCODE_h2SqrtBF16      (SEQ_MTX_OPCODE_BASE+ 207) // sqrt SIMD BF16
#define CRTST_KRNL_SEQ_MTX_OPCODE_hRsqrtBF16      (SEQ_MTX_OPCODE_BASE+ 208) // rsqrt BF16
#define CRTST_KRNL_SEQ_MTX_OPCODE_h2RsqrtBF16     (SEQ_MTX_OPCODE_BASE+ 209) // rsqrt SIMD BF16
#define CRTST_KRNL_SEQ_MTX_OPCODE_hRcpBF16        (SEQ_MTX_OPCODE_BASE+ 210) // rcp BF16
#define CRTST_KRNL_SEQ_MTX_OPCODE_h2RcpBF16       (SEQ_MTX_OPCODE_BASE+ 211) // rcp SIMD BF16

// this list will get much bigger
#define CRTST_KRNL_SEQ_MTX_OPERAND              8
#define CRTST_KRNL_SEQ_MTX_OPERAND_None     -1
#define CRTST_KRNL_SEQ_MTX_OPERAND_MatrixA  0
#define CRTST_KRNL_SEQ_MTX_OPERAND_MatrixB  1
#define CRTST_KRNL_SEQ_MTX_OPERAND_TextureA 2
#define CRTST_KRNL_SEQ_MTX_OPERAND_TextureB 3
#define CRTST_KRNL_SEQ_MTX_OPERAND_Max      4

#define CRTST_INPUT_DATA_FILL_STYLE             9
#define CRTST_INPUT_DATA_FILL_STYLE_range   0
#define CRTST_INPUT_DATA_FILL_STYLE_exp     1
#define CRTST_INPUT_DATA_RANGE_MIN              10
#define CRTST_INPUT_DATA_RANGE_MAX              11
#define CRTST_INPUT_DATA_FRAMEBASE_POW2         12
#define CRTST_INPUT_DATA_FRAMEBASE_POW2_DBL     13
#define CRTST_KRNL_SEQ_MTX_DBL_NUM_OPS          14
#define CRTST_KRNL_SEQ_MTX_DBL_OP_IDX           15
#define CRTST_KRNL_SEQ_MTX_DBL_OPCODE           16

#define CRTST_KRNL_STRESS_NUM_OPS               17
#define CRTST_KRNL_STRESS_MAT_C_HIGH_BOUND      18
#define CRTST_KRNL_STRESS_MAT_C_LOW_BOUND       19

// Debug pickers, useful for debugging the kernels
#define CRTST_KRNL_SEQ_MTX_DEBUG_IDX_INC        20 // Increment the kernels idx variable by this amount.
#define CRTST_PRINT                             21 // Print this launch
#define CRTST_PRINT_TILE_PRIORITY               22 // Priority for printing raw tile data 0=none, 1=Debug, 2=Low, 3,Normal, 4=Warn, 5=High, 6=Error
#define CRTST_SYNC                              23 // Sync after this launch
#define CRTST_LAUNCH_STREAM                     24 // Which stream to launch on.
#define CRTST_LAUNCH_STREAM_Num 4                  // How many streams there are.
#define CRTST_BRANCH_INPUT_MIN                  25 // randomize Branching kernel input tile
#define CRTST_BRANCH_INPUT_MAX                  26 // randomize Branching kernel input tile
#define CRTST_BRANCH_OUTPUT_MIN                 27 // randomize Branching kernel output tile
#define CRTST_BRANCH_OUTPUT_MAX                 28 // randomize Branching kernel output tile

#define CRTST_KRNL_SEQ_MTX_INT_NUM_OPS          29
#define CRTST_KRNL_SEQ_MTX_INT_OP_IDX           30
#define CRTST_KRNL_SEQ_MTX_INT_OPCODE           31
#define CRTST_KRNL_SEQ_MTX_INT_MAT_A_MIN        32
#define CRTST_KRNL_SEQ_MTX_INT_MAT_A_MAX        33
#define CRTST_KRNL_SEQ_MTX_INT_MAT_B_MIN        34
#define CRTST_KRNL_SEQ_MTX_INT_MAT_B_MAX        35
#define CRTST_KRNL_SEQ_MTX_INT_MAT_C_MIN        36
#define CRTST_KRNL_SEQ_MTX_INT_MAT_C_MAX        37

#define CRTST_DEBUG_INPUT                       38
#define CRTST_DEBUG_OUTPUT                      39

#define CRTST_KRNL_TEX_NUM_OPS                  40
#define CRTST_KRNL_TEX_OP_IDX                   41
#define CRTST_KRNL_TEX_OPCODE                   42

#define CRTST_KRNL_SURF_NUM_OPS                 43
#define CRTST_KRNL_SURF_OP_IDX                  44
#define CRTST_KRNL_SURF_OPCODE                  45
#define CRTST_KRNL_SURF_RND_BLOCK               46
#define CRTST_INPUT_DATA_TEXSURF                47
#define CRTST_INPUT_NUM_SUBSURF                 48

#define CRTST_MAT_INIT_INT_MIN                  49
#define CRTST_MAT_INIT_INT_MAX                  50
#define CRTST_MAT_INIT_FLOAT_MIN                51
#define CRTST_MAT_INIT_FLOAT_MAX                52
#define CRTST_KRNL_ATOM_OPCODE                  53  // opcodes for AtomKernelOps
#define CRTST_KRNL_ATOM_ARG0                    54  // integer operand
#define CRTST_KRNL_ATOM_ARG1                    55  // float operand
#define CRTST_KRNL_ATOM_SAME_WARP               56  // if true, same warps collide
#define CRTST_KRNL_ATOM_NUM_OPS                 57  // ops to execute each launch
#define CRTST_KRNL_ATOM_OP_IDX                  58  // starting offset in AtomKernelOps, each launch
#define CRTST_KRNL_ATOM_OUTPUT_MIN              59  // min output-surf pre-fill val (0)
#define CRTST_KRNL_ATOM_OUTPUT_MAX              60  // max output-surf pre-fill val (0xffffffff)
#define CRTST_NUM_PICKERS                       61

// List of ops for atomics subtest
#define CRTST_KRNL_ATOM_noop                   0
#define CRTST_KRNL_ATOM_atom_global_and_b32    1
#define CRTST_KRNL_ATOM_atom_global_or_b32     2
#define CRTST_KRNL_ATOM_atom_global_xor_b32    3
#define CRTST_KRNL_ATOM_atom_global_cas_b32    4
#define CRTST_KRNL_ATOM_atom_global_cas_b64    5
#define CRTST_KRNL_ATOM_atom_global_exch_b32   6
#define CRTST_KRNL_ATOM_atom_global_exch_b64   7
#define CRTST_KRNL_ATOM_atom_global_add_u32    8
#define CRTST_KRNL_ATOM_atom_global_add_s32    9
#define CRTST_KRNL_ATOM_atom_global_add_f32   10
#define CRTST_KRNL_ATOM_atom_global_add_u64   11
#define CRTST_KRNL_ATOM_atom_global_inc_u32   12
#define CRTST_KRNL_ATOM_atom_global_dec_u32   13
#define CRTST_KRNL_ATOM_atom_global_min_u32   14
#define CRTST_KRNL_ATOM_atom_global_min_s32   15
#define CRTST_KRNL_ATOM_atom_global_max_u32   16
#define CRTST_KRNL_ATOM_atom_global_max_s32   17
#define CRTST_KRNL_ATOM_atom_shared_and_b32   18
#define CRTST_KRNL_ATOM_atom_shared_or_b32    19
#define CRTST_KRNL_ATOM_atom_shared_xor_b32   20
#define CRTST_KRNL_ATOM_atom_shared_cas_b32   21
#define CRTST_KRNL_ATOM_atom_shared_cas_b64   22
#define CRTST_KRNL_ATOM_atom_shared_exch_b32  23
#define CRTST_KRNL_ATOM_atom_shared_exch_b64  24
#define CRTST_KRNL_ATOM_atom_shared_add_u32   25
#define CRTST_KRNL_ATOM_atom_shared_add_u64   26
#define CRTST_KRNL_ATOM_atom_shared_inc_u32   27
#define CRTST_KRNL_ATOM_atom_shared_dec_u32   28
#define CRTST_KRNL_ATOM_atom_shared_min_u32   29
#define CRTST_KRNL_ATOM_atom_shared_max_u32   30
#define CRTST_KRNL_ATOM_red_global_and_b32    31
#define CRTST_KRNL_ATOM_red_global_or_b32     32
#define CRTST_KRNL_ATOM_red_global_xor_b32    33
#define CRTST_KRNL_ATOM_red_global_add_u32    34
#define CRTST_KRNL_ATOM_red_global_add_s32    35
#define CRTST_KRNL_ATOM_red_global_add_f32    36
#define CRTST_KRNL_ATOM_red_global_add_u64    37
#define CRTST_KRNL_ATOM_red_global_inc_u32    38
#define CRTST_KRNL_ATOM_red_global_dec_u32    39
#define CRTST_KRNL_ATOM_red_global_min_u32    40
#define CRTST_KRNL_ATOM_red_global_min_s32    41
#define CRTST_KRNL_ATOM_red_global_max_u32    42
#define CRTST_KRNL_ATOM_red_global_max_s32    43
#define CRTST_KRNL_ATOM_red_shared_and_b32    44
#define CRTST_KRNL_ATOM_red_shared_or_b32     45
#define CRTST_KRNL_ATOM_red_shared_xor_b32    46
#define CRTST_KRNL_ATOM_red_shared_cas_b32    47
#define CRTST_KRNL_ATOM_red_shared_cas_b64    48
#define CRTST_KRNL_ATOM_red_shared_add_u32    49
#define CRTST_KRNL_ATOM_red_shared_add_u64    50
#define CRTST_KRNL_ATOM_red_shared_inc_u32    51
#define CRTST_KRNL_ATOM_red_shared_dec_u32    52
#define CRTST_KRNL_ATOM_red_shared_min_u32    53
#define CRTST_KRNL_ATOM_red_shared_max_u32    54
#define CRTST_KRNL_ATOM_atom_global_and_b64   55
#define CRTST_KRNL_ATOM_atom_global_or_b64    56
#define CRTST_KRNL_ATOM_atom_global_xor_b64   57
#define CRTST_KRNL_ATOM_atom_global_min_u64   58
#define CRTST_KRNL_ATOM_atom_global_max_u64   59
#define CRTST_KRNL_ATOM_atom_shared_and_b64   60
#define CRTST_KRNL_ATOM_atom_shared_or_b64    61
#define CRTST_KRNL_ATOM_atom_shared_xor_b64   62
#define CRTST_KRNL_ATOM_atom_shared_min_u64   63
#define CRTST_KRNL_ATOM_atom_shared_max_u64   64

// SM_82 and later
#define CRTST_KRNL_ATOM_redux_sync_add_u32    65
#define CRTST_KRNL_ATOM_redux_sync_min_u32    66
#define CRTST_KRNL_ATOM_redux_sync_max_u32    67
#define CRTST_KRNL_ATOM_redux_sync_and_b32    68
#define CRTST_KRNL_ATOM_redux_sync_or_b32     69
#define CRTST_KRNL_ATOM_redux_sync_xor_b32    70

#define CRTST_KRNL_ATOM_debug                 71
#define CRTST_KRNL_ATOM_NUM_OPCODES           72

//#endif
