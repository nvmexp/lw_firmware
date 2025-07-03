/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file glr_comm.h
 * @brief Common C++ and JavaScript declarations for GL Random tests.
 */

//-----------------------------------------------------------------------------
// Define a bunch of constants shared between JS and C++, with the
// C++ side declared as enum values for better type-checking.
//
// When all mods platforms have upgraded to JavaScript 1.7 or later, we can
// use const global variables instead of non-const variables.
//
#if defined(__cplusplus)

    #define ENUM_BEGIN(e) enum e {
    #define ENUM_CONST(n, val) n = (val),
    #define ENUM_END     };

#else

    #define ENUM_BEGIN(n)
    #define ENUM_CONST(n, val) var n = (val);
    #define ENUM_END

    var glr_commId = "$Id$";
#endif

/** <a name="Dimensions"> </a> */
/*
 * Dimensions of our world, before the view projection into eye-space.
 *
 * We use a large world, so that we get sub-pixel resolution even when
 * using 16-bit integer vertex coordinates.
 * Lwrrently, we keep model-space the same as world-space.
 *
 * The center of the world is:   0,0,0
 *
 * The eye is at world location: 0,0,16000
 */
#define RND_GEOM_WorldWidth   (20 * 1024)
#define RND_GEOM_WorldHeight  (15 * 1024)
#define RND_GEOM_WorldDepth   (15 * 1024)
#define RND_GEOM_WorldLeft    (-RND_GEOM_WorldWidth/2)
#define RND_GEOM_WorldRight   ( RND_GEOM_WorldWidth/2)
#define RND_GEOM_WorldBottom  (-RND_GEOM_WorldHeight/2)
#define RND_GEOM_WorldTop     ( RND_GEOM_WorldHeight/2)
#define RND_GEOM_WorldFar     (3*RND_GEOM_WorldDepth/2)  // absolute eye distance for glOrtho() or fog
#define RND_GEOM_WorldNear    (RND_GEOM_WorldDepth/2)    // absolute eye distance for glOrtho() or fog

/** <a name="Modules"> </a> */
/*
 * Codes for the different randomizer modules.
 */
#define RND_TSTCTRL          0   // RndMisc
#define RND_GPU_PROG         1   // RndGpuPrograms
#define RND_TX               2   // RndTexturing
#define RND_GEOM             3   // RndGeometry
#define RND_LIGHT            4   // RndLighting
#define RND_FOG              5   // RndFog
#define RND_FRAG             6   // RndFragment
#define RND_PRIM             7   // RndPrimitiveModes
#define RND_VXFMT            8   // RndVertexFormat
#define RND_PXL              9   // RndPixels
#define RND_VX              10   // RndVertexes
#define RND_NUM_RANDOMIZERS 11
#define RND_NO_RANDOMIZER   -1

/** <a name="Indexes"> </a> */
/*
 * Picker Indexes  (JavaScript)
 *
 *
 * These are the legal picker indexes for GLRandom.SetPicker(), .GetPicker(),
 * and .SetDefaultPicker().
 * Use them to control the way GLRandom does its random picking.
 *
 * EXAMPLES:
 *
 *    To always use both texture units 0 and 3:
 *
 *       GLRandom.SetPicker(RND_TXU_ENABLE_MASK, [  0x9 ]);
 *
 *    To enable fog 75% of the time:
 *
 *       GLRandom.SetPicker(RND_FOG_ENABLE, [ [3, GL_TRUE], [1, GL_FALSE] ]);
 *
 *    To force vertex primary/material color to r,g,b 0,0,255:
 *
 *       GLRandom.SetPicker(RND_VX_COLOR_BYTE, [ "list", 0,0,255 ]);
 *
 *    To allow any light spelwlar exponent (shininess) between 0 and 256,
 *    but make higher numbers more probable than lower numbers:
 *
 *       GLRandom.SetPicker(RND_LIGHT_SHININESS, [ [3, 0,128], [97, 129,256] ]);
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
 *    "shuffle"                  (return items from list in random order)
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
 *    "int" "float" "int64"       (whether to interpret the numbers as floats
 *                                 32 bit integers, or 64 bit integers.
 *                                 DEFAULT is set in C++ for each
 *                                 picker,)
 *
 *    1) Capitalization is ignored.
 *    2) If you specify more than one mode, the last one "wins".
 *    3) If you fail to specify any mode at all, the program tries to guess from
 *       the format of ITEMS.
 *    4) You are actually allowed to specify MODE and OPTIONS strings
 *       in any order, as long as they all preceed the ITEMS.
 *    5) Javascript cannot pass the full range of 64 bit
 *       integers.  In order to work around this limitation, 64
 *       bit integers may be specified with strings
 *
 * ITEMS depends on what mode you are in:
 *
 *    CONST ITEM     must be a single number.
 *
 *    RANDOM ITEMS   must be a sequence of one or more arrays, each like this:
 *                      [ relative_probability, value ]     or
 *                      [ relative_probability, min, max ]
 *
 *    SHUFFLE ITEMS  must be a sequence of one or more arrays, each like this:
 *                      [ copies_in_deck, value ]
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

#define RND_BASE_OFFSET    1000
#if defined(__cplusplus)
   // C++
   #define IDX(i)  ((i) % RND_BASE_OFFSET)
#else
   // JavaScript
   #define IDX(i)  (i)
#endif

#define RND_TST_BASE                     (RND_TSTCTRL*RND_BASE_OFFSET)
#define RND_TST
#define RND_TSTCTRL_STOP                 IDX(RND_TST_BASE+ 0)   // (picked before all other pickers in a loop) stop testing, return current pass/fail code
#define RND_TSTCTRL_LOGMODE              IDX(RND_TST_BASE+ 1)   // (picked just after STOP) self-check logging/checking of generated textures and random picks
#define RND_TSTCTRL_LOGMODE_Skip                0    //   value for RND_TSTCTRL_LOGMODE
#define RND_TSTCTRL_LOGMODE_Store               1    //   value for RND_TSTCTRL_LOGMODE
#define RND_TSTCTRL_LOGMODE_Check               2    //   value for RND_TSTCTRL_LOGMODE
#define RND_TSTCTRL_SUPERVERBOSE         IDX(RND_TST_BASE+ 2)   // (picked after all other pickers in a loop) do/don't print picks just before sending them
#define RND_TSTCTRL_SKIP                 IDX(RND_TST_BASE+ 3)   // (picked after SUPERVERBOSE) mask of blocks that should skip sending to GL this loop
#define RND_TSTCTRL_SKIP_none               0x00                //$
#define RND_TSTCTRL_SKIP_TSTCTRL           (1 << RND_TSTCTRL)
#define RND_TSTCTRL_SKIP_TX                (1 << RND_TX     )
#define RND_TSTCTRL_SKIP_GEOM              (1 << RND_GEOM   )
#define RND_TSTCTRL_SKIP_LIGHT             (1 << RND_LIGHT  )
#define RND_TSTCTRL_SKIP_FOG               (1 << RND_FOG    )
#define RND_TSTCTRL_SKIP_FRAG              (1 << RND_FRAG   )
#define RND_TSTCTRL_SKIP_PRIM              (1 << RND_PRIM   )
#define RND_TSTCTRL_SKIP_VXFMT             (1 << RND_VXFMT  )
#define RND_TSTCTRL_SKIP_VX                (1 << RND_VX     )
#define RND_TSTCTRL_SKIP_PXL               (1 << RND_PXL    )
#define RND_TSTCTRL_SKIP_all               ((1 << RND_NUM_RANDOMIZERS)-1)
#define RND_TSTCTRL_RESTART_SKIP         IDX(RND_TST_BASE+ 4)
#define RND_TSTCTRL_RESTART_SKIP_clear      0x01    // skip glClear and rect clear during Restart()
#define RND_TSTCTRL_RESTART_SKIP_txload     0x02    // skip downloading all but 1 texel of textures during Restart()
#define RND_TSTCTRL_RESTART_SKIP_txblack    0x04    // force all textures to be black during Restart()
#define RND_TSTCTRL_RESTART_SKIP_rectclear  0x08    // after the glClear, draw a big rect over the screen during Restart()
#define RND_TSTCTRL_COUNT_PIXELS         IDX(RND_TST_BASE+ 5)   // do/don't enable zpass-pixel-count mode on this draw
#define RND_TSTCTRL_FINISH               IDX(RND_TST_BASE+ 6)   // (picked after the send on each loop) do/don't call glFinish() after the draw.
#define RND_TSTCTRL_CLEAR_ALPHA_F        IDX(RND_TST_BASE+ 7)   // (clear color: alpha, float32)
#define RND_TSTCTRL_CLEAR_RED_F          IDX(RND_TST_BASE+ 8)   // (clear color: red, float32)
#define RND_TSTCTRL_CLEAR_GREEN_F        IDX(RND_TST_BASE+ 9)   // (clear color: green, float32)
#define RND_TSTCTRL_CLEAR_BLUE_F         IDX(RND_TST_BASE+10)   // (clear color: blue, float32)
#define RND_TSTCTRL_DRAW_ACTION          IDX(RND_TST_BASE+11)   // draw vertexes or pixels
#define RND_TSTCTRL_DRAW_ACTION_vertices    0
#define RND_TSTCTRL_DRAW_ACTION_pixels      1
#define RND_TSTCTRL_DRAW_ACTION_3dpixels    2
#define RND_TSTCTRL_ENABLE_XFB           IDX(RND_TST_BASE+12)   // enable/disable Transform Feedback
#define RND_TSTCTRL_ENABLE_GL_DEBUG      IDX(RND_TST_BASE+13)   // enable/disable GL Debug messages
#define RND_TSTCTRL_LWTRACE              IDX(RND_TST_BASE+14)   // enable/disable GL driver tracing of loop
#define RND_TSTCTRL_LWTRACE_registry   0 // leave lwtrace settings at registry values
#define RND_TSTCTRL_LWTRACE_apply_off  1 // override lwtrace settings, disable tracing
#define RND_TSTCTRL_LWTRACE_apply_on   2 // override lwtrace settings, enable tracing
#define RND_TSTCTRL_RESTART_LWTRACE      IDX(RND_TST_BASE+15)   // enable/disable GL driver tracing of restart
#define RND_TSTCTRL_PWRWAIT_DELAY_MS     IDX(RND_TST_BASE+16)   // how long to wait for a GC5/GC6 entry
#define RND_TSTCTRL_NUM_PICKERS                17

#define RND_GPU_PROG_BASE                     (RND_GPU_PROG*RND_BASE_OFFSET)
// common vertex,geometry, & fragment opcodes                     Modifiers
//                                                                F I C S H D  Out Inputs    Description
//                                                                - - - - - -  --- --------  --------------------------------
#define RND_GPU_PROG_OPCODE_abs                           0 // "ABS"    X X X X X F  v   v         absolute value
#define RND_GPU_PROG_OPCODE_add                           1 // "ADD"    X X X X X F  v   v,v       add
#define RND_GPU_PROG_OPCODE_and                           2 // "AND"    - X X - - S  v   v,v       bitwise and
#define RND_GPU_PROG_OPCODE_brk                           3 // "BRK"    - - - - - -  -   c         break out of loop instruction
#define RND_GPU_PROG_OPCODE_cal                           4 // "CAL"    - - - - - -  -   c         subroutine call                                 )
#define RND_GPU_PROG_OPCODE_cei                           5 // "CEIL"   X X X X X F  v   vf        ceiling
#define RND_GPU_PROG_OPCODE_cmp                           6 // "CMP"    X X X X X F  v   v,v,v     compare
#define RND_GPU_PROG_OPCODE_con                           7 // "CONT"   - - - - - -  -   c         continue with next loop interation
#define RND_GPU_PROG_OPCODE_cos                           8 // "COS"    X - X X X F  s   s         cosine with reduction to [-PI,PI]
#define RND_GPU_PROG_OPCODE_div                           9 // "DIV"    X X X X X F  v   v,s       divide vector components by scalar
#define RND_GPU_PROG_OPCODE_dp2                          10 // "DP2"    X - X X X F  s   v,v       2-component dot product
#define RND_GPU_PROG_OPCODE_dp2a                         11 // "DP2A"   X - X X X F  s   v,v,v     2-comp. dot product w/scalar add
#define RND_GPU_PROG_OPCODE_dp3                          12 // "DP3"    X - X X X F  s   v,v       3-component dot product
#define RND_GPU_PROG_OPCODE_dp4                          13 // "DP4"    X - X X X F  s   v,v       4-component dot product
#define RND_GPU_PROG_OPCODE_dph                          14 // "DPH"    X - X X X F  s   v,v       homogeneous dot product
#define RND_GPU_PROG_OPCODE_dst                          15 // "DST"    X - X X X F  v   v,v       distance vector
#define RND_GPU_PROG_OPCODE_else                         16 // "ELSE"   - - - - - -  -   -         start if test else block
#define RND_GPU_PROG_OPCODE_endif                        17 // "ENDIF"  - - - - - -  -   -         end if test block
#define RND_GPU_PROG_OPCODE_endrep                       18 // "ENDREP  - - - - - -  -   -         end of repeat block
#define RND_GPU_PROG_OPCODE_ex2                          19 // "EX2"    X - X X X F  s   s         exponential base 2
#define RND_GPU_PROG_OPCODE_flr                          20 // "FLR"    X X X X X F  v   vf        floor
#define RND_GPU_PROG_OPCODE_frc                          21 // "FRC"    X - X X X F  v   v         fraction
#define RND_GPU_PROG_OPCODE_i2f                          22 // "I2F"    - X X - - S  vf  v         integer to float
#define RND_GPU_PROG_OPCODE_if                           23 // "IF"     - - - - - -  -   c         start of if test block
#define RND_GPU_PROG_OPCODE_lg2                          24 // "LG2"    X - X X X F  s   s         logarithm base 2
#define RND_GPU_PROG_OPCODE_lit                          25 // "LIT"    X - X X X F  v   v         compute lighting coefficients
#define RND_GPU_PROG_OPCODE_lrp                          26 // "LRP"    X - X X X F  v   v,v,v     linear interpolation
#define RND_GPU_PROG_OPCODE_mad                          27 // "MAD"    X X X X X F  v   v,v,v     multiply and add
#define RND_GPU_PROG_OPCODE_max                          28 // "MAX"    X X X X X F  v   v,v       maximum
#define RND_GPU_PROG_OPCODE_min                          29 // "MIN"    X X X X X F  v   v,v       minimum
#define RND_GPU_PROG_OPCODE_mod                          30 // "MOD"    - X X - - S  v   v,s       modulus vector components by scalar
#define RND_GPU_PROG_OPCODE_mov                          31 // "MOV"    X X X X X F  v   v         move
#define RND_GPU_PROG_OPCODE_mul                          32 // "MUL"    X X X X X F  v   v,v       multiply
#define RND_GPU_PROG_OPCODE_not                          33 // "NOT"    - X X - - S  v   v         bitwise not
#define RND_GPU_PROG_OPCODE_nrm                          34 // "NRM"    X - X X X F  v   v         normalize 3-component vector
#define RND_GPU_PROG_OPCODE_or                           35 // "OR"     - X X - - S  v   v,v       bitwise or
#define RND_GPU_PROG_OPCODE_pk2h                         36 // "PK2H"   X X - - - F  s   vf        pack two 16-bit floats
#define RND_GPU_PROG_OPCODE_pk2us                        37 // "PK2US"  X X - - - F  s   vf        pack two floats as unsigned 16-bit
#define RND_GPU_PROG_OPCODE_pk4b                         38 // "PK4B"   X X - - - F  s   vf        pack four floats as signed 8-bit
#define RND_GPU_PROG_OPCODE_pk4ub                        39 // "PK4UB"  X X - - - F  s   vf        pack four floats as unsigned 8-bit
#define RND_GPU_PROG_OPCODE_pow                          40 // "POW"    X - X X X F  s   s,s       exponentiate
#define RND_GPU_PROG_OPCODE_rcc                          41 // "RCC"    X - X X X F  s   s         reciprocal (clamped)
#define RND_GPU_PROG_OPCODE_rcp                          42 // "RCP"    X - X X X F  s   s         reciprocal
#define RND_GPU_PROG_OPCODE_rep                          43 // "REP"    X X - - X F  -   v         start of repeat block
#define RND_GPU_PROG_OPCODE_ret                          44 // "RET"    - - - - - -  -   c         subroutine return
#define RND_GPU_PROG_OPCODE_rfl                          45 // "RFL"    X - X X X F  v   v,v       reflection vector
#define RND_GPU_PROG_OPCODE_round                        46 // "ROUND"  X X X X X F  v   vf        round to nearest integer
#define RND_GPU_PROG_OPCODE_rsq                          47 // "RSQ"    X - X X X F  s   s         reciprocal square root
#define RND_GPU_PROG_OPCODE_sad                          48 // "SAD"    - X X - - S  vu  v,v,vu    sum of absolute differences
#define RND_GPU_PROG_OPCODE_scs                          49 // "SCS"    X - X X X F  v   s         sine/cosine without reduction
#define RND_GPU_PROG_OPCODE_seq                          50 // "SEQ"    X X X X X F  v   v,v       set on equal
#define RND_GPU_PROG_OPCODE_sfl                          51 // "SFL"    X X X X X F  v   v,v       set on false
#define RND_GPU_PROG_OPCODE_sge                          52 // "SGE"    X X X X X F  v   v,v       set on greater than or equal
#define RND_GPU_PROG_OPCODE_sgt                          53 // "SGT"    X X X X X F  v   v,v       set on greater than
#define RND_GPU_PROG_OPCODE_shl                          54 // "SHL"    - X X - - S  v   v,s       shift left
#define RND_GPU_PROG_OPCODE_shr                          55 // "SHR"    - X X - - S  v   v,s       shift right
#define RND_GPU_PROG_OPCODE_sin                          56 // "SIN"    X - X X X F  s   s         sine with reduction to [-PI,PI]
#define RND_GPU_PROG_OPCODE_sle                          57 // "SLE"    X X X X X F  v   v,v       set on less than or equal
#define RND_GPU_PROG_OPCODE_slt                          58 // "SLT"    X X X X X F  v   v,v       set on less than
#define RND_GPU_PROG_OPCODE_sne                          59 // "SNE"    X X X X X F  v   v,v       set on not equal
#define RND_GPU_PROG_OPCODE_ssg                          60 // "SSG"    X - X X X F  v   v         set sign
#define RND_GPU_PROG_OPCODE_str                          61 // "STR"    X X X X X F  v   v,v       set on true
#define RND_GPU_PROG_OPCODE_sub                          62 // "SUB"    X X X X X F  v   v,v       subtract
#define RND_GPU_PROG_OPCODE_swz                          63 // "SWZ"    X - X X X F  v   v         extended swizzle
#define RND_GPU_PROG_OPCODE_tex                          64 // "TEX"    X X X X - F  v   vf        texture sample
#define RND_GPU_PROG_OPCODE_trunc                        65 // "TRUNC"  X X X X X F  v   vf        truncate (round toward zero)
#define RND_GPU_PROG_OPCODE_txb                          66 // "TXB"    X X X X - F  v   vf        texture sample with bias
#define RND_GPU_PROG_OPCODE_txd                          67 // "TXD"    X X X X - F  v   vf,vf,vf  texture sample w/partials
#define RND_GPU_PROG_OPCODE_txf                          68 // "TXF"    X X X X - F  v   vs        texel fetch
#define RND_GPU_PROG_OPCODE_txl                          69 // "TXL"    X X X X - F  v   vf        texture sample w/LOD
#define RND_GPU_PROG_OPCODE_txp                          70 // "TXP"    X X X X - F  v   vf        texture sample w/projection
#define RND_GPU_PROG_OPCODE_txq                          71 // "TXQ"    - - - - - S  vs  vs        texture info query
#define RND_GPU_PROG_OPCODE_up2h                         72 // "UP2H"   X X X X - F  vf  s         unpack two 16-bit floats
#define RND_GPU_PROG_OPCODE_up2us                        73 // "UP2US"  X X X X - F  vf  s         unpack two unsigned 16-bit ints
#define RND_GPU_PROG_OPCODE_up4b                         74 // "UP4B"   X X X X - F  vf  s         unpack four signed 8-bit ints
#define RND_GPU_PROG_OPCODE_up4ub                        75 // "UP4UB"  X X X X - F  vf  s         unpack four unsigned 8-bit ints
#define RND_GPU_PROG_OPCODE_x2d                          76 // "X2D"    X - X X X F  v   v,v,v     2D coordinate transformation
#define RND_GPU_PROG_OPCODE_xor                          77 // "XOR"    - X X - - S  v   v,v       exclusive or
#define RND_GPU_PROG_OPCODE_xpd                          78 // "XPD"    X - X X X F  v   v,v       cross product
#define RND_GPU_PROG_OPCODE_txg                          79 // "TXG"    X X X X X F  v   v         texture gather (LW_gpu_program4_1)
#define RND_GPU_PROG_OPCODE_tex2                         80 // "TEX"    X X X X - F  v   vf,vf     texture sample (LW_gpu_program4_1)
#define RND_GPU_PROG_OPCODE_txb2                         81 // "TXB"    X X X X - F  v   vf,vf     texture sample with bias (LW_gpu_program4_1)
#define RND_GPU_PROG_OPCODE_txl2                         82 // "TXL"    X X X X - F  v   vf,vf     texture sample w/LOD (LW_gpu_program4_1)
#define RND_GPU_PROG_OPCODE_txfms                        83 // "TXFMS"  X X X X - F  v   vs        texture fetch from multisampled texture.
// lw_gpu_program5 opcodes
#define RND_GPU_PROG_OPCODE_bfe                          84 // "BFE"    - X X - - I  v   v,v       bitfield extract
#define RND_GPU_PROG_OPCODE_bfi                          85 // "BFI"    - X X - - I  v   v,v,v     bitfield insert
#define RND_GPU_PROG_OPCODE_bfr                          86 // "BFR"    - X X - - I  v   v         bitfield reverse
#define RND_GPU_PROG_OPCODE_btc                          87 // "BTC"    - X X - - I  v   v         bitcount
#define RND_GPU_PROG_OPCODE_btfl                         88 // "BTFL"   - X X - - I  v   v         find least significant bit
#define RND_GPU_PROG_OPCODE_btfm                         89 // "BTFM"   - X X - - I  v   v         find most significant bit
#define RND_GPU_PROG_OPCODE_txgo                         90 // "TXGO"   X X X X - F  v   vf,vs,vs  texture gather w/per-texel offsets
#define RND_GPU_PROG_OPCODE_cvt                          91 // "CVT"    - - X X - I  v   v         colwert data type
#define RND_GPU_PROG_OPCODE_pk64                         92 // "PK64"   6 6 - - - F  v   vf        pack 2 32-bit values to 64-bit
#define RND_GPU_PROG_OPCODE_up64                         93 // "UP64"   6 6 X X - F  vf  v         unpack 64-bit component to 2x32
#define RND_GPU_PROG_OPCODE_ldc                          94 // "LDC"    X X X X - F  v   v         load from constant buffer
#define RND_GPU_PROG_OPCODE_cali                         95 // "CALI"   - - - - - -  -   s         call fn-ptr or indexed fn-ptr array
#define RND_GPU_PROG_OPCODE_loadim                       96 // "LOADIM" - - X X - F  v   vs,i      image load
#define RND_GPU_PROG_OPCODE_storeim                      97 // "STOREIM"X X - - - F  -   i,v,vs    image store
#define RND_GPU_PROG_OPCODE_membar                       98 // "MEMBAR" - - - - - -  -   -         memory barrier
#define RND_GPU_PROG_OPCODE_NUM_OPCODES                  99
// common vertex, geometry, & fragment templates
#define RND_GPU_PROG_TEMPLATE_Simple                0    // no flow control
#define RND_GPU_PROG_TEMPLATE_Call                  1    // subroutines with conditional CALs (vp2+)
#define RND_GPU_PROG_TEMPLATE_Flow                  2    // if/else or repeat inst.
#define RND_GPU_PROG_TEMPLATE_CallIndexed           3    // use CALI (indexed array of function pointers)
#define RND_GPU_PROG_TEMPLATE_NUM_TEMPLATES         4

#define RND_GPU_PROG_REG_none                       -1   // initial state of input/output registers

// common vertex, geometry, & fragment clamping (saturation) flags
#define RND_GPU_PROG_OP_SAT_signed                  0   // clamped to [-1,1]
#define RND_GPU_PROG_OP_SAT_unsigned                1   // clamped to [0,1]
#define RND_GPU_PROG_OP_SAT_none                    2   // no clamping

// common vertex, geometry, & fragment test flags
#define RND_GPU_PROG_OUT_TEST_eq                 0  // equal
#define RND_GPU_PROG_OUT_TEST_ge                 1  // greater than or equal
#define RND_GPU_PROG_OUT_TEST_gt                 2  // greater than
#define RND_GPU_PROG_OUT_TEST_le                 3  // less than or equal
#define RND_GPU_PROG_OUT_TEST_lt                 4  // less than
#define RND_GPU_PROG_OUT_TEST_ne                 5  // not equal
#define RND_GPU_PROG_OUT_TEST_tr                 6  // true
#define RND_GPU_PROG_OUT_TEST_fl                 7  // false
#define RND_GPU_PROG_OUT_TEST_nan                8  // not a number
#define RND_GPU_PROG_OUT_TEST_leg                9  // less than, equal, or greater than
#define RND_GPU_PROG_OUT_TEST_cf                10  // carry flag
#define RND_GPU_PROG_OUT_TEST_ncf               11  // no carry flag
#define RND_GPU_PROG_OUT_TEST_of                12  // overflow flag
#define RND_GPU_PROG_OUT_TEST_nof               13  // no overflow flag
#define RND_GPU_PROG_OUT_TEST_ab                14  // above
#define RND_GPU_PROG_OUT_TEST_ble               15  // below or equal
#define RND_GPU_PROG_OUT_TEST_sf                16  // sign flag
#define RND_GPU_PROG_OUT_TEST_nsf               17  // no sign flag
#define RND_GPU_PROG_OUT_TEST_NUM               18
#define RND_GPU_PROG_OUT_TEST_none              RND_GPU_PROG_OUT_TEST_NUM
// common vertex, geometry, & fragment CC masks (used for opcode modifier)
#define RND_GPU_PROG_CC0                         0    //   CC0
#define RND_GPU_PROG_CC1                         1    //   CC1
#define RND_GPU_PROG_CCnone                      2    //   don't update/test CC
// common vertex, geometry & fragment swizzle ops
#define RND_GPU_PROG_SWIZZLE_xyzw                1    //   value for RND_TX_FRAG_PROG_SWIZZLE
#define RND_GPU_PROG_SWIZZLE_shuffle             2    //   value for RND_TX_FRAG_PROG_SWIZZLE
#define RND_GPU_PROG_SWIZZLE_random              3    //   value for RND_TX_FRAG_PROG_SWIZZLE
// common vertex, geometry & fragment op sign
#define RND_GPU_PROG_OP_SIGN_none                0
#define RND_GPU_PROG_OP_SIGN_plus                1
#define RND_GPU_PROG_OP_SIGN_neg                 2
// common vertex, geometry & fragment swizzle order types
#define RND_GPU_PROG_SWIZZLE_ORDER_xyzw          0
#define RND_GPU_PROG_SWIZZLE_ORDER_shuffle       1
#define RND_GPU_PROG_SWIZZLE_ORDER_random        2
// common vertex, geometry & fragment swizzle suffix types
#define RND_GPU_PROG_SWIZZLE_SUFFIX_none         0
#define RND_GPU_PROG_SWIZZLE_SUFFIX_component    1 // one 1 component
#define RND_GPU_PROG_SWIZZLE_SUFFIX_all          2 // all 4 components are used

//---------------------------
// RndGpu program specific pickers
#define RND_GPU_PROG_LP_PICK_PGMS               IDX(RND_GPU_PROG_BASE+0)   // pick new loaded pgm on this loop?
#define RND_GPU_PROG_LOG_RND_PGMS               IDX(RND_GPU_PROG_BASE+1)   // If non-zero log each random program to a separate file.
#define RND_GPU_PROG_LOG_TO_JS_FILE             IDX(RND_GPU_PROG_BASE+2)   // If non-zero log each random program to a seperate js file.
#define RND_GPU_PROG_OPCODE_DATA_TYPE           IDX(RND_GPU_PROG_BASE+3)   // data type for opmodifier, see opDataType below
#define RND_GPU_PROG_VERBOSE_OUTPUT             IDX(RND_GPU_PROG_BASE+4)   // show more output to help debug OCG issues.
#define RND_GPU_PROG_STRICT_PGM_LINKING         IDX(RND_GPU_PROG_BASE+5)   // (restart) Generate programs and enables in matched sets.
#define RND_GPU_PROG_ATOMIC_MODIFIER            IDX(RND_GPU_PROG_BASE+6)   // mandatory modifier for atomic operations
#define RND_GPU_PROG_USE_BINDLESS_TEXTURES      IDX(RND_GPU_PROG_BASE+7)   // if true, texture instructions use bindless syntax.
#define RND_GPU_PROG_LAYER_OUT                  IDX(RND_GPU_PROG_BASE+8)   // In case of layered FBO, select the layer to which we will draw.
#define RND_GPU_PROG_VPORT_IDX                  IDX(RND_GPU_PROG_BASE+9)   // In case of layered FBO, select the single viewport to which we will draw.
#define RND_GPU_PROG_VPORT_MASK                 IDX(RND_GPU_PROG_BASE+10)  // In case of layered FBO, select multiple viewports (mask) to which we will draw.
#define RND_GPU_PROG_IS_VPORT_SELECT_AS_INDEXED IDX(RND_GPU_PROG_BASE+11) // Select whether we are specifying final viewport to render to, using index (or mask) method.

//--------------------------
// vertex specific pickers
#define RND_GPU_PROG_VX_BASE                  (RND_GPU_PROG_BASE+12)
#define RND_GPU_PROG_VX_ENABLE                IDX(RND_GPU_PROG_VX_BASE+ 0)   // enable vertex program
#define RND_GPU_PROG_VX_INDEX                 IDX(RND_GPU_PROG_VX_BASE+ 1)   // which vertex program to use
#define RND_GPU_PROG_VX_INDEX_UseAttr1              0    //   value for RND_PROG_VX_INDEX
#define RND_GPU_PROG_VX_INDEX_UseAttrNRML           1    //   value for RND_PROG_VX_INDEX
#define RND_GPU_PROG_VX_INDEX_UseAttrCOL0           2    //   value for RND_PROG_VX_INDEX
#define RND_GPU_PROG_VX_INDEX_UseAttrFOGC           3    //   value for RND_PROG_VX_INDEX
#define RND_GPU_PROG_VX_INDEX_UseAttr6              4    //   value for RND_PROG_VX_INDEX
#define RND_GPU_PROG_VX_INDEX_UseAttr7              5    //   value for RND_PROG_VX_INDEX
#define RND_GPU_PROG_VX_INDEX_UseAttrTEX4           6    //   value for RND_PROG_VX_INDEX
#define RND_GPU_PROG_VX_INDEX_UseAttrTEX5           7    //   value for RND_PROG_VX_INDEX
#define RND_GPU_PROG_VX_INDEX_UseAttrTEX6           8    //   value for RND_PROG_VX_INDEX
#define RND_GPU_PROG_VX_INDEX_UseAttrTEX7           9    //   value for RND_PROG_VX_INDEX
#define RND_GPU_PROG_VX_INDEX_IdxLight012          10    //   value for RND_PROG_VX_INDEX
#define RND_GPU_PROG_VX_INDEX_IdxLight345          11    //   value for RND_PROG_VX_INDEX
#define RND_GPU_PROG_VX_INDEX_IdxLight678          12    //   value for RND_PROG_VX_INDEX
#define RND_GPU_PROG_VX_INDEX_IdxLight91011        13    //   value for RND_PROG_VX_INDEX
#define RND_GPU_PROG_VX_INDEX_UseADD               14    //   value for RND_PROG_VX_INDEX
#define RND_GPU_PROG_VX_INDEX_UseRCP               15    //   value for RND_PROG_VX_INDEX
#define RND_GPU_PROG_VX_INDEX_UseDST               16    //   value for RND_PROG_VX_INDEX
#define RND_GPU_PROG_VX_INDEX_UseMIN               17    //   value for RND_PROG_VX_INDEX
#define RND_GPU_PROG_VX_INDEX_UseMAX               18    //   value for RND_PROG_VX_INDEX
#define RND_GPU_PROG_VX_INDEX_UseSLT               19    //   value for RND_PROG_VX_INDEX
#define RND_GPU_PROG_VX_INDEX_UseSGE               20    //   value for RND_PROG_VX_INDEX
#define RND_GPU_PROG_VX_INDEX_UseEXP               21    //   value for RND_PROG_VX_INDEX
#define RND_GPU_PROG_VX_INDEX_UseLOG               22    //   value for RND_PROG_VX_INDEX
#define RND_GPU_PROG_VX_INDEX_UseDPH               23    //   value for RND_PROG_VX_INDEX
#define RND_GPU_PROG_VX_INDEX_UseRCC               24    //   value for RND_PROG_VX_INDEX
#define RND_GPU_PROG_VX_INDEX_PosIlwar             25    //   value for RND_PROG_VX_INDEX
#define RND_GPU_PROG_VX_INDEX_NumFixedProgs        26
#define RND_GPU_PROG_VX_NUMRANDOM             IDX(RND_GPU_PROG_VX_BASE+ 2)   // how many random vertex programs to generate per frame
#define RND_GPU_PROG_VX_NUMRANDOM_Max             100    //   max value allowed
#define RND_GPU_PROG_VX_TEMPLATE              IDX(RND_GPU_PROG_VX_BASE+ 3)   // which template to use for randomly generated vertex programs?
#define RND_GPU_PROG_VX_NUM_OPS               IDX(RND_GPU_PROG_VX_BASE+ 4)   // how many instructions long should a random vxpgm be?
#define RND_GPU_PROG_VX_NUM_OPS_RandomOneToMax     -1    //   value for VX_NUM_OPS: use a random number in [1,max supported]
#define RND_GPU_PROG_VX_OPCODE                IDX(RND_GPU_PROG_VX_BASE+ 5)   // random vertex program instruction
#define RND_GPU_PROG_VX_OPCODE_NUM_OPCODES    RND_GPU_PROG_OPCODE_NUM_OPCODES // no extra opcode for vertex programs
#define RND_GPU_PROG_VX_OUT_TEST              IDX(RND_GPU_PROG_VX_BASE+ 6)   // CC test for conditional result writes.
#define RND_GPU_PROG_VX_WHICH_CC              IDX(RND_GPU_PROG_VX_BASE+ 7)   // which condition-code register to update or test.
#define RND_GPU_PROG_VX_SWIZZLE_ORDER         IDX(RND_GPU_PROG_VX_BASE+ 8)   // input or CC swizzle
#define RND_GPU_PROG_VX_SWIZZLE_SUFFIX        IDX(RND_GPU_PROG_VX_BASE+ 9)   // input or CC swizzle
#define RND_GPU_PROG_VX_OUT_REG               IDX(RND_GPU_PROG_VX_BASE+10)   // output register for random vertex program instruction

#define RND_GPU_PROG_VX_REG_vOPOS                   0    // object-space position
#define RND_GPU_PROG_VX_REG_vWGHT                   1    // skinning weight
#define RND_GPU_PROG_VX_REG_vNRML                   2    // object-space normal
#define RND_GPU_PROG_VX_REG_vCOL0                   3    // primary color
#define RND_GPU_PROG_VX_REG_vCOL1                   4    // secondary color
#define RND_GPU_PROG_VX_REG_vFOGC                   5    // fog coord
#define RND_GPU_PROG_VX_REG_v6                      6    // extra
#define RND_GPU_PROG_VX_REG_v7                      7    // extra
#define RND_GPU_PROG_VX_REG_vTEX0                   8    // texture coord
#define RND_GPU_PROG_VX_REG_vTEX1                   9    //
#define RND_GPU_PROG_VX_REG_vTEX2                  10    //
#define RND_GPU_PROG_VX_REG_vTEX3                  11    //
#define RND_GPU_PROG_VX_REG_vTEX4                  12    //
#define RND_GPU_PROG_VX_REG_vTEX5                  13    //
#define RND_GPU_PROG_VX_REG_vTEX6                  14    //
#define RND_GPU_PROG_VX_REG_vTEX7                  15    //
#define RND_GPU_PROG_VX_REG_vINST                  16    //$ vertex.instance (ARB_draw_instanced)
#define RND_GPU_PROG_VX_REG_oHPOS                  17    //$ Homogeneous clip space position    (x,y,z,w)
#define RND_GPU_PROG_VX_REG_oCOL0                  18    //$ Primary color (front-facing)       (r,g,b,a)
#define RND_GPU_PROG_VX_REG_oCOL1                  19    //$ Secondary color (front-facing)     (r,g,b,a)
#define RND_GPU_PROG_VX_REG_oBFC0                  20    //$ Back-facing primary color          (r,g,b,a)
#define RND_GPU_PROG_VX_REG_oBFC1                  21    //$ Back-facing secondary color        (r,g,b,a)
#define RND_GPU_PROG_VX_REG_oFOGC                  22    //$ Fog coordinate                     (f,*,*,*)
#define RND_GPU_PROG_VX_REG_oPSIZ                  23    //$ Point size                         (p,*,*,*)
#define RND_GPU_PROG_VX_REG_oCLP0                  24    //$ clip coord (vp2 only)
#define RND_GPU_PROG_VX_REG_oCLP1                  25    //
#define RND_GPU_PROG_VX_REG_oCLP2                  26    //
#define RND_GPU_PROG_VX_REG_oCLP3                  27    //
#define RND_GPU_PROG_VX_REG_oCLP4                  28    //
#define RND_GPU_PROG_VX_REG_oCLP5                  29    //
#define RND_GPU_PROG_VX_REG_oTEX0                  30    //$ Texture coordinate set 0           (s,t,r,q)
#define RND_GPU_PROG_VX_REG_oTEX1                  31    //$ Texture coordinate set 1           (s,t,r,q)
#define RND_GPU_PROG_VX_REG_oTEX2                  32    //$ Texture coordinate set 2           (s,t,r,q)
#define RND_GPU_PROG_VX_REG_oTEX3                  33    //$ Texture coordinate set 3           (s,t,r,q)
#define RND_GPU_PROG_VX_REG_oTEX4                  34    //$ Texture coordinate set 4           (s,t,r,q)
#define RND_GPU_PROG_VX_REG_oTEX5                  35    //$ Texture coordinate set 5           (s,t,r,q)
#define RND_GPU_PROG_VX_REG_oTEX6                  36    //$ Texture coordinate set 6           (s,t,r,q)
#define RND_GPU_PROG_VX_REG_oTEX7                  37    //$ Texture coordinate set 7           (s,t,r,q)
#define RND_GPU_PROG_VX_REG_oNRML                  38    //$ surface normal direction
#define RND_GPU_PROG_VX_REG_oID                    39
#define RND_GPU_PROG_VX_REG_oLAYR                  40    //$ layer for lwbe/array/3D FBOs       (l,*,*,*)
#define RND_GPU_PROG_VX_REG_oVPORTIDX              41    //$ output viewport index              (l,*,*,*)
#define RND_GPU_PROG_VX_REG_oVPORTMASK             42    //$ output viewport mask               (l,*,*,*)
#define RND_GPU_PROG_VX_REG_R0                     43    //$ temp register
#define RND_GPU_PROG_VX_REG_R1                     44    //
#define RND_GPU_PROG_VX_REG_R2                     45    //
#define RND_GPU_PROG_VX_REG_R3                     46    //
#define RND_GPU_PROG_VX_REG_R4                     47    //
#define RND_GPU_PROG_VX_REG_R5                     48    //
#define RND_GPU_PROG_VX_REG_R6                     49    //
#define RND_GPU_PROG_VX_REG_R7                     50    //
#define RND_GPU_PROG_VX_REG_R8                     51    //
#define RND_GPU_PROG_VX_REG_R9                     52    //
#define RND_GPU_PROG_VX_REG_R10                    53    //
#define RND_GPU_PROG_VX_REG_R11                    54    //
#define RND_GPU_PROG_VX_REG_R12                    55    // temp register (vp2 only)
#define RND_GPU_PROG_VX_REG_R13                    56    //
#define RND_GPU_PROG_VX_REG_R14                    57    //
#define RND_GPU_PROG_VX_REG_R15                    58    //
#define RND_GPU_PROG_VX_REG_A128                   59    //$ array-index reg (holds random 0..128 for use with pgmElw and cbuf)
#define RND_GPU_PROG_VX_REG_Const                  60    //$ place holder for the 96 (or 256) const regs
#define RND_GPU_PROG_VX_REG_LocalElw               61    //$ place holder for the local program elw[]
#define RND_GPU_PROG_VX_REG_ConstVect              62    //$ place holder for when a const vector is being used
#define RND_GPU_PROG_VX_REG_Literal                63    //$ place holder where a literal constant is being used as 1st arg
#define RND_GPU_PROG_VX_REG_PaboElw                64    //$ place holder for const buffer object elw, ie. program.buffer[a][b]
#define RND_GPU_PROG_VX_REG_TempArray              65    //$ temp array
#define RND_GPU_PROG_VX_REG_Subs                   66    //$ place holder for "Subs" array of fn-ptrs
#define RND_GPU_PROG_VX_REG_A1                     67    //$ array-index reg (holds random 0..1 for use with tmpArray)
#define RND_GPU_PROG_VX_REG_ASubs                  68    //$ array-index reg (holds random 0..NumSubs-1 for use with Subs)
#define RND_GPU_PROG_VX_REG_A3                     69    //$ array-index reg (holds random 0..NumTxHandles-1 for use with bindless textures)
#define RND_GPU_PROG_VX_REG_Garbage                70    //$ special temp register that stores garbage results
#define RND_GPU_PROG_VX_REG_NUM_REGS               71
#define RND_GPU_PROG_VX_OUT_MASK              IDX(RND_GPU_PROG_VX_BASE+11)   //$ output register component mask
#define RND_GPU_PROG_VX_SATURATE              IDX(RND_GPU_PROG_VX_BASE+12)   //$ do/don't  saturate output (clamp to [0,1]).
#define RND_GPU_PROG_VX_RELADDR               IDX(RND_GPU_PROG_VX_BASE+13)   //$ do/don't use addr-reg  addressing for reg reads
#define RND_GPU_PROG_VX_IN_ABS                IDX(RND_GPU_PROG_VX_BASE+14)   //$ do/don't take abs. value of input argument
#define RND_GPU_PROG_VX_IN_NEG                IDX(RND_GPU_PROG_VX_BASE+15)   //$ do/don't negate input argument
#define RND_GPU_PROG_VX_TX_DIM                IDX(RND_GPU_PROG_VX_BASE+16)   //$ Which texture dim (1d/2d/rect/lwbe/3d) to fetch from.
#define RND_GPU_PROG_VX_TX_OFFSET             IDX(RND_GPU_PROG_VX_BASE+17)   //$ Which form of texture offset to use.
#define RND_GPU_PROG_VX_LIGHT0                IDX(RND_GPU_PROG_VX_BASE+18)   //$ which "light" to use in certain vertex programs
#define RND_GPU_PROG_VX_LIGHT1                IDX(RND_GPU_PROG_VX_BASE+19)   //$ which "light" to use in certain vertex programs
#define RND_GPU_PROG_VX_POINTSZ               IDX(RND_GPU_PROG_VX_BASE+20)   //$ do/don't generate point size from vertex program
#define RND_GPU_PROG_VX_TWOSIDE               IDX(RND_GPU_PROG_VX_BASE+21)   //$ do/don't callwlate two-sided lighting in vertex programs
#define RND_GPU_PROG_VX_POINTSZ_VAL           IDX(RND_GPU_PROG_VX_BASE+22)   //$ (float) point size used inside vx progs
#define RND_GPU_PROG_VX_COL0_ALIAS            IDX(RND_GPU_PROG_VX_BASE+23)   //$ which vertex attrib will contain input primary color?
#define RND_GPU_PROG_VX_POSILWAR              IDX(RND_GPU_PROG_VX_BASE+24)   //$ use OPTION LW_position_ilwariant (vp1.1 and up)
#define RND_GPU_PROG_VX_MULTISAMPLE           IDX(RND_GPU_PROG_VX_BASE+25)   //$ use OPTION LW_explicit_multisample (gpu4.1 and up)
#define RND_GPU_PROG_VX_CLAMP_COLOR_ENABLE    IDX(RND_GPU_PROG_VX_BASE+26)   //$ enable/disable vertex color clamping
#define RND_GPU_PROG_VX_INDEX_DEBUG           IDX(RND_GPU_PROG_VX_BASE+27)   //$ which vertex program to use (debug override)
#define RND_GPU_PROG_VX_WRITES_TO_LAYR_VPORT  IDX(RND_GPU_PROG_VX_BASE+28)   //$ Does vertex program write to a different layer/viewport?
#define RND_GPU_PROG_INDEX_DEBUG_NO_OVERRIDE       0xffffffff                //$ default value, don't override
#define RND_GPU_PROG_INDEX_DEBUG_FORCE_DISABLE     0xfffffffe                //$ override -- force disable

//---------------------------
// Tesselation control specific pickers
#define RND_GPU_PROG_TC_BASE                    (RND_GPU_PROG_VX_BASE+29)
#define RND_GPU_PROG_TC_ENABLE                IDX(RND_GPU_PROG_TC_BASE+0)
#define RND_GPU_PROG_TC_INDEX                 IDX(RND_GPU_PROG_TC_BASE+1)
#define RND_GPU_PROG_TC_INDEX_PassThruPatch2 0
#define RND_GPU_PROG_TC_INDEX_PassThruPatch3 1
#define RND_GPU_PROG_TC_INDEX_PassThruPatch4 2
#define RND_GPU_PROG_TC_INDEX_PassThruPatch5 3
#define RND_GPU_PROG_TC_INDEX_PassThruPatch6 4
#define RND_GPU_PROG_TC_INDEX_PassThruPatch7 5
#define RND_GPU_PROG_TC_INDEX_PassThruPatch8 6
#define RND_GPU_PROG_TC_INDEX_NumFixedProgs  7
#define RND_GPU_PROG_TC_TESSRATE_OUTER        IDX(RND_GPU_PROG_TC_BASE+2)
#define RND_GPU_PROG_TC_TESSRATE_INNER        IDX(RND_GPU_PROG_TC_BASE+3)
#define RND_GPU_PROG_TC_NUMRANDOM             IDX(RND_GPU_PROG_TC_BASE+4)
#define RND_GPU_PROG_TC_INDEX_DEBUG           IDX(RND_GPU_PROG_TC_BASE+5)   // which program to use (debug override)

//---------------------------
// Tesselation evaluation specific pickers
#define RND_GPU_PROG_TE_BASE                    (RND_GPU_PROG_TC_BASE+6)
#define RND_GPU_PROG_TE_ENABLE                IDX(RND_GPU_PROG_TE_BASE+0)
#define RND_GPU_PROG_TE_INDEX                 IDX(RND_GPU_PROG_TE_BASE+1)
#define RND_GPU_PROG_TE_INDEX_Quad          0
#define RND_GPU_PROG_TE_INDEX_Tri           1
#define RND_GPU_PROG_TE_INDEX_Line          2
#define RND_GPU_PROG_TE_INDEX_Point         3
#define RND_GPU_PROG_TE_INDEX_NumFixedProgs 4
#define RND_GPU_PROG_TE_BULGE                 IDX(RND_GPU_PROG_TE_BASE+2)
#define RND_GPU_PROG_TE_TESS_MODE             IDX(RND_GPU_PROG_TE_BASE+3)   // GL_TRIANGLES, QUADS, ISOLINES
#define RND_GPU_PROG_TE_TESS_POINT_MODE       IDX(RND_GPU_PROG_TE_BASE+4)
#define RND_GPU_PROG_TE_TESS_SPACING          IDX(RND_GPU_PROG_TE_BASE+5)   // equal, frac_odd, frac_even
#define RND_GPU_PROG_TE_CW                    IDX(RND_GPU_PROG_TE_BASE+6)   // use clockwise triangles (vs. counter-clockwise)
#define RND_GPU_PROG_TE_WRITES_TO_LAYR_VPORT  IDX(RND_GPU_PROG_TE_BASE+7)   // Does TE program write to a different layer/viewport?
#define RND_GPU_PROG_TE_NUMRANDOM             IDX(RND_GPU_PROG_TE_BASE+8)
#define RND_GPU_PROG_TE_INDEX_DEBUG           IDX(RND_GPU_PROG_TE_BASE+9)   // which program to use (debug override)

//---------------------------
// Geometry specific pickers
#define RND_GPU_PROG_GM_BASE                    (RND_GPU_PROG_TE_BASE+10)
#define RND_GPU_PROG_GM_ENABLE                IDX(RND_GPU_PROG_GM_BASE+ 0)   // enable geometry program
#define RND_GPU_PROG_GM_INDEX                 IDX(RND_GPU_PROG_GM_BASE+ 1)   // which geometry program to use
#define RND_GPU_PROG_GM_INDEX_UseTri            0   // value for RND_GPU_PROG_GM_INDEX
#define RND_GPU_PROG_GM_INDEX_UseTriAdj         1   // value for RND_GPU_PROG_GM_INDEX
#define RND_GPU_PROG_GM_INDEX_UseLine           2   // value for RND_GPU_PROG_GM_INDEX
#define RND_GPU_PROG_GM_INDEX_UseLineAdj        3   // value for RND_GPU_PROG_GM_INDEX
#define RND_GPU_PROG_GM_INDEX_UseLineAdj2       4   // value for RND_GPU_PROG_GM_INDEX
#define RND_GPU_PROG_GM_INDEX_UsePoint          5   // value for RND_GPU_PROG_GM_INDEX
#define RND_GPU_PROG_GM_INDEX_UsePointSprite    6   // value for RND_GPU_PROG_GM_INDEX
#define RND_GPU_PROG_GM_INDEX_UseBezierLwrve    7   // not ready for prime time just yet.
#define RND_GPU_PROG_GM_INDEX_NumFixedProgs     7
#define RND_GPU_PROG_GM_NUMRANDOM             IDX(RND_GPU_PROG_GM_BASE+ 2)   //$ how many random geometry programs to generate per frame
#define RND_GPU_PROG_GM_NUMRANDOM_Max             100                        //$   max value allowed
#define RND_GPU_PROG_GM_ISPASSTHRU            IDX(RND_GPU_PROG_GM_BASE+ 3)   //$ is this a pass through geometyr program?
#define RND_GPU_PROG_GM_TEMPLATE              IDX(RND_GPU_PROG_GM_BASE+ 4)   //$ which template to use for randomly generated vertex programs?
#define RND_GPU_PROG_GM_NUM_OPS_PER_VERTEX    IDX(RND_GPU_PROG_GM_BASE+ 5)   //$ min number of opcodes per emitted vertex
#define RND_GPU_PROG_GM_NUM_OPS_RandomOneToMax     -1                        //$   value for VX_NUM_OPS: use a random number in [1,max supported]
#define RND_GPU_PROG_GM_OPCODE                IDX(RND_GPU_PROG_GM_BASE+ 6)   //$ random vertex program instruction
#define RND_GPU_PROG_GM_OPCODE_NUM_OPCODES    RND_GPU_PROG_OPCODE_NUM_OPCODES//$ no extra opcode for geometry programs
#define RND_GPU_PROG_GM_OUT_TEST              IDX(RND_GPU_PROG_GM_BASE+ 7)   //$ CC test for conditional result writes.
#define RND_GPU_PROG_GM_WHICH_CC              IDX(RND_GPU_PROG_GM_BASE+ 8)   //$ which condition-code register to update or test.
#define RND_GPU_PROG_GM_SWIZZLE_ORDER         IDX(RND_GPU_PROG_GM_BASE+ 9)   //$ input or CC swizzle
#define RND_GPU_PROG_GM_SWIZZLE_SUFFIX        IDX(RND_GPU_PROG_GM_BASE+10)   //$ input or CC swizzle
#define RND_GPU_PROG_GM_OUT_REG               IDX(RND_GPU_PROG_GM_BASE+11)   //$ output register for random geometry program instruction
#define RND_GPU_PROG_GM_REG_vHPOS                   0    //$ Homogeneous clip space position    (x,y,z,w)
#define RND_GPU_PROG_GM_REG_vCOL0                   1    //$ primary color                      (r,g,b,a)
#define RND_GPU_PROG_GM_REG_vCOL1                   2    //$ secondary color                    (r,g,b,a)
#define RND_GPU_PROG_GM_REG_vBFC0                   3    //$ back facing primary color          (r,g,b,a)
#define RND_GPU_PROG_GM_REG_vBFC1                   4    //$ back facing secondary color        (r,g,b,a)
#define RND_GPU_PROG_GM_REG_vFOGC                   5    //$ fog coord                          (f,*,*,*)
#define RND_GPU_PROG_GM_REG_vPSIZ                   6    //$ point size                         (p,*,*,*)
#define RND_GPU_PROG_GM_REG_vTEX0                   7    //$ Texture coordinate set 0           (s,t,r,q)
#define RND_GPU_PROG_GM_REG_vTEX1                   8    //$ Texture coordinate set 1           (s,t,r,q)
#define RND_GPU_PROG_GM_REG_vTEX2                   9    //$ Texture coordinate set 2           (s,t,r,q)
#define RND_GPU_PROG_GM_REG_vTEX3                  10    //$ Texture coordinate set 3           (s,t,r,q)
#define RND_GPU_PROG_GM_REG_vTEX4                  11    //$ Texture coordinate set 4           (s,t,r,q)
#define RND_GPU_PROG_GM_REG_vTEX5                  12    //$ Texture coordinate set 5           (s,t,r,q)
#define RND_GPU_PROG_GM_REG_vTEX6                  13    //$ Texture coordinate set 6           (s,t,r,q)
#define RND_GPU_PROG_GM_REG_vTEX7                  14    //$ Texture coordinate set 7           (s,t,r,q)
#define RND_GPU_PROG_GM_REG_vNRML                  15    //$ surface normal
#define RND_GPU_PROG_GM_REG_vCLP0                  16    //$ Clip plane 0 distance              (d,*,*,*)
#define RND_GPU_PROG_GM_REG_vCLP1                  17    //$ Clip plane 1 distance              (d,*,*,*)
#define RND_GPU_PROG_GM_REG_vCLP2                  18    //$ Clip plane 2 distance              (d,*,*,*)
#define RND_GPU_PROG_GM_REG_vCLP3                  19    //$ Clip plane 3 distance              (d,*,*,*)
#define RND_GPU_PROG_GM_REG_vCLP4                  20    //$ Clip plane 4 distance              (d,*,*,*)
#define RND_GPU_PROG_GM_REG_vCLP5                  21    //$ Clip plane 5 distance              (d,*,*,*)
#define RND_GPU_PROG_GM_REG_vID                    22    //$ vertex id                          (x,*,*,*)
#define RND_GPU_PROG_GM_REG_pID                    23    //$ primitive id                       (x,*,*,*)
#define RND_GPU_PROG_GM_REG_pILWO                  24    //$ primitive invocation               (x,*,*,*)
#define RND_GPU_PROG_GM_REG_oHPOS                  25    //$ Homogeneous clip space position    (x,y,z,w)
#define RND_GPU_PROG_GM_REG_oCOL0                  26    //$ Primary color (front-facing)       (r,g,b,a)
#define RND_GPU_PROG_GM_REG_oCOL1                  27    //$ Secondary color (front-facing)     (r,g,b,a)
#define RND_GPU_PROG_GM_REG_oBFC0                  28    //$ Back-facing primary color          (r,g,b,a)
#define RND_GPU_PROG_GM_REG_oBFC1                  29    //$ Back-facing secondary color        (r,g,b,a)
#define RND_GPU_PROG_GM_REG_oFOGC                  30    //$ Fog coordinate                     (f,*,*,*)
#define RND_GPU_PROG_GM_REG_oPSIZ                  31    //$ Point size                         (p,*,*,*)
#define RND_GPU_PROG_GM_REG_oTEX0                  32    //$ Texture coordinate set 0           (s,t,r,q)
#define RND_GPU_PROG_GM_REG_oTEX1                  33    //$ Texture coordinate set 1           (s,t,r,q)
#define RND_GPU_PROG_GM_REG_oTEX2                  34    //$ Texture coordinate set 2           (s,t,r,q)
#define RND_GPU_PROG_GM_REG_oTEX3                  35    //$ Texture coordinate set 3           (s,t,r,q)
#define RND_GPU_PROG_GM_REG_oTEX4                  36    //$ Texture coordinate set 4           (s,t,r,q)
#define RND_GPU_PROG_GM_REG_oTEX5                  37    //$ Texture coordinate set 5           (s,t,r,q)
#define RND_GPU_PROG_GM_REG_oTEX6                  38    //$ Texture coordinate set 6           (s,t,r,q)
#define RND_GPU_PROG_GM_REG_oTEX7                  39    //$ Texture coordinate set 7           (s,t,r,q)
#define RND_GPU_PROG_GM_REG_oNRML                  40    //$ surface normal
#define RND_GPU_PROG_GM_REG_oCLP0                  41    //$ Clip plane 0 distance              (d,*,*,*)
#define RND_GPU_PROG_GM_REG_oCLP1                  42    //$ Clip plane 1 distance              (d,*,*,*)
#define RND_GPU_PROG_GM_REG_oCLP2                  43    //$ Clip plane 2 distance              (d,*,*,*)
#define RND_GPU_PROG_GM_REG_oCLP3                  44    //$ Clip plane 3 distance              (d,*,*,*)
#define RND_GPU_PROG_GM_REG_oCLP4                  45    //$ Clip plane 4 distance              (d,*,*,*)
#define RND_GPU_PROG_GM_REG_oCLP5                  46    //$ Clip plane 5 distance              (d,*,*,*)
#define RND_GPU_PROG_GM_REG_oPID                   47    //$ Primitive ID                       (id,*,*,*)
#define RND_GPU_PROG_GM_REG_oLAYR                  48    //$ layer for lwbe/array/3D FBOs       (l,*,*,*)
#define RND_GPU_PROG_GM_REG_oVPORTIDX              49    //$ output viewport index              (l,*,*,*)
#define RND_GPU_PROG_GM_REG_oVPORTMASK             50    //$ output viewport mask               (l,*,*,*)
#define RND_GPU_PROG_GM_REG_R0                     51    //$ temp registers
#define RND_GPU_PROG_GM_REG_R1                     52    //
#define RND_GPU_PROG_GM_REG_R2                     53    //
#define RND_GPU_PROG_GM_REG_R3                     54    //
#define RND_GPU_PROG_GM_REG_R4                     55    //
#define RND_GPU_PROG_GM_REG_R5                     56    //
#define RND_GPU_PROG_GM_REG_R6                     57    //
#define RND_GPU_PROG_GM_REG_R7                     58    //
#define RND_GPU_PROG_GM_REG_R8                     59    //
#define RND_GPU_PROG_GM_REG_R9                     60    //
#define RND_GPU_PROG_GM_REG_R10                    61    //
#define RND_GPU_PROG_GM_REG_R11                    62    //
#define RND_GPU_PROG_GM_REG_R12                    63    // temp register (vp2 only)
#define RND_GPU_PROG_GM_REG_R13                    64    //
#define RND_GPU_PROG_GM_REG_R14                    65    //
#define RND_GPU_PROG_GM_REG_R15                    66    //
#define RND_GPU_PROG_GM_REG_A128                   67    //$ array-index reg (holds random 0..128 for use with pgmElw and cbuf)
#define RND_GPU_PROG_GM_REG_Const                  68    //$ place holder for the 96 (or 256) const regs
#define RND_GPU_PROG_GM_REG_LocalElw               69    //$ place holder for the local program elw[]
#define RND_GPU_PROG_GM_REG_ConstVect              70    //$ place holder when a const vector is being used
#define RND_GPU_PROG_GM_REG_Literal                71    //$ place holder where a literal constant is being used as 1st arg
#define RND_GPU_PROG_GM_REG_PaboElw                72    //$ place holder for const buffer object elw[], ie. program.buffer[a][b]
#define RND_GPU_PROG_GM_REG_TempArray              73    //$ temp array (for STL/LDL coverage)
#define RND_GPU_PROG_GM_REG_Subs                   74    //$ place holder for "Subs" array of fn-ptrs
#define RND_GPU_PROG_GM_REG_A1                     75    //$ array-index reg (holds random 0..1 for use with tmpArray)
#define RND_GPU_PROG_GM_REG_ASubs                  76    //$ array-index reg (holds random 0..NumSubs-1 for use with Subs)
#define RND_GPU_PROG_GM_REG_A3                     77    //$ array-index reg (holds random 0..NumTxHandles-1 for use with bindless textures)
#define RND_GPU_PROG_GM_REG_Garbage                78    //$ special temp register that stores garbage results    
#define RND_GPU_PROG_GM_REG_NUM_REGS               79
#define RND_GPU_PROG_GM_OUT_MASK              IDX(RND_GPU_PROG_GM_BASE+12)   //$ output register component mask
#define RND_GPU_PROG_GM_SATURATE              IDX(RND_GPU_PROG_GM_BASE+13)   //$ do/don't  saturate output (clamp to [0,1]).
#define RND_GPU_PROG_GM_RELADDR               IDX(RND_GPU_PROG_GM_BASE+14)   //$ do/don't use addr-reg  addressing for reg reads
#define RND_GPU_PROG_GM_IN_ABS                IDX(RND_GPU_PROG_GM_BASE+15)   //$ do/don't take abs. value of input argument
#define RND_GPU_PROG_GM_IN_NEG                IDX(RND_GPU_PROG_GM_BASE+16)   //$ do/don't negate input argument
#define RND_GPU_PROG_GM_TX_DIM                IDX(RND_GPU_PROG_GM_BASE+17)   //$ Which texture dim (1d/2d/rect/lwbe/3d) to fetch from.
#define RND_GPU_PROG_GM_TX_OFFSET             IDX(RND_GPU_PROG_GM_BASE+18)   //$ Which form of texture offset to use.
#define RND_GPU_PROG_GM_LIGHT0                IDX(RND_GPU_PROG_GM_BASE+19)   //$ which "light" to use in certain vertex programs
#define RND_GPU_PROG_GM_LIGHT1                IDX(RND_GPU_PROG_GM_BASE+20)   //$ which "light" to use in certain vertex programs
#define RND_GPU_PROG_GM_PRIM_IN               IDX(RND_GPU_PROG_GM_BASE+21)   //$ which type of input primitives this pgm requires
#define RND_GPU_PROG_GM_PRIM_OUT              IDX(RND_GPU_PROG_GM_BASE+22)   //$ what tppe of output primitives this gpm will create
#define RND_GPU_PROG_GM_VERTICES_OUT          IDX(RND_GPU_PROG_GM_BASE+23)   //$ number of output vertices per primitive this program will emit.
#define RND_GPU_PROG_GM_PRIMITIVES_OUT        IDX(RND_GPU_PROG_GM_BASE+24)   //$ number of primitives to output for this pgm.
#define RND_GPU_PROG_GM_WRITES_TO_LAYR_VPORT  IDX(RND_GPU_PROG_GM_BASE+25)   //$ Does Geometry program write to a different layer/viewport?
#define RND_GPU_PROG_GM_INDEX_DEBUG           IDX(RND_GPU_PROG_GM_BASE+26)   //$ which program to use (debug override)

//---------------------------
// Fragment specific pickers
#define RND_GPU_PROG_FR_BASE                  (RND_GPU_PROG_GM_BASE+27)
#define RND_GPU_PROG_FR_ENABLE                IDX(RND_GPU_PROG_FR_BASE+0)   // Do/don't enable fragment programs.
#define RND_GPU_PROG_FR_INDEX                 IDX(RND_GPU_PROG_FR_BASE+1)   // Which fragment program to use
#define RND_GPU_PROG_FR_INDEX_NumFixedProgs         1
#define RND_GPU_PROG_FR_NUMRANDOM             IDX(RND_GPU_PROG_FR_BASE+2)   // how many random fragment programs to generate per frame
#define RND_GPU_PROG_FR_NUMRANDOM_Max             100    //   max value allowed
#define RND_GPU_PROG_FR_TEMPLATE              IDX(RND_GPU_PROG_FR_BASE+3)   // which template to use for randomly generated fragment programs?
#define RND_GPU_PROG_FR_NUM_OPS               IDX(RND_GPU_PROG_FR_BASE+4)   // number of opcodes for each fragment pgm
#define RND_GPU_PROG_FR_OPCODE                IDX(RND_GPU_PROG_FR_BASE+5)   // which fragment program opcode to use next when generating programs
                                                                                        //  Modifiers
                                                                                        // F I C S H D  Out Inputs    Description
                                                                                        // - - - - - -  --- --------  --------------------------------
#define RND_GPU_PROG_FR_OPCODE_ddx                 (RND_GPU_PROG_OPCODE_NUM_OPCODES + 0)// X - X X X F  v   v         partial dericative relative to X
#define RND_GPU_PROG_FR_OPCODE_ddy                 (RND_GPU_PROG_OPCODE_NUM_OPCODES + 1)// X - X X X F  v   v         partial derviative relative to Y
#define RND_GPU_PROG_FR_OPCODE_kil                 (RND_GPU_PROG_OPCODE_NUM_OPCODES + 2)// X X - - X F  -   vc        kill fragment
#define RND_GPU_PROG_FR_OPCODE_lod                 (RND_GPU_PROG_OPCODE_NUM_OPCODES + 3)// X - X X - F  v   v         Texture Level Of Detail (LOD)
#define RND_GPU_PROG_FR_OPCODE_ipac                (RND_GPU_PROG_OPCODE_NUM_OPCODES + 4)// X - X X X F  v   v         Interpolate at Centroid
#define RND_GPU_PROG_FR_OPCODE_ipao                (RND_GPU_PROG_OPCODE_NUM_OPCODES + 5)// X - X X X F  v   v         Interpolate with offset
#define RND_GPU_PROG_FR_OPCODE_ipas                (RND_GPU_PROG_OPCODE_NUM_OPCODES + 6)// X - X X X F  v   v         Interpolate at sample
#define RND_GPU_PROG_FR_OPCODE_atom                (RND_GPU_PROG_OPCODE_NUM_OPCODES + 7)// - - X - - -  s   v,su      atomic memory transaction
#define RND_GPU_PROG_FR_OPCODE_load                (RND_GPU_PROG_OPCODE_NUM_OPCODES + 8)// - - X X - F  v   su        global load
#define RND_GPU_PROG_FR_OPCODE_store               (RND_GPU_PROG_OPCODE_NUM_OPCODES + 9)// - - - - - -  -   v,su      global store
#define RND_GPU_PROG_FR_OPCODE_atomim              (RND_GPU_PROG_OPCODE_NUM_OPCODES +10)// - - X - - -  s   v,vs,i    atomic image modify
#define RND_GPU_PROG_FR_OPCODE_NUM_OPCODES         (RND_GPU_PROG_OPCODE_NUM_OPCODES +11)
#define RND_GPU_PROG_FR_OUT_TEST              IDX(RND_GPU_PROG_FR_BASE+6)
#define RND_GPU_PROG_FR_WHICH_CC              IDX(RND_GPU_PROG_FR_BASE+7)
#define RND_GPU_PROG_FR_SWIZZLE_ORDER         IDX(RND_GPU_PROG_FR_BASE+8)
#define RND_GPU_PROG_FR_SWIZZLE_SUFFIX        IDX(RND_GPU_PROG_FR_BASE+9)
#define RND_GPU_PROG_FR_OUT_REG               IDX(RND_GPU_PROG_FR_BASE+10)   // Which register to use for frag prog opcode result?
#define RND_GPU_PROG_FR_REG_fWPOS                   0    //$ input registers
#define RND_GPU_PROG_FR_REG_fCOL0                   1    //$
#define RND_GPU_PROG_FR_REG_fCOL1                   2    //$
#define RND_GPU_PROG_FR_REG_fFOGC                   3    //$
#define RND_GPU_PROG_FR_REG_fTEX0                   4    //$
#define RND_GPU_PROG_FR_REG_fTEX1                   5    //$
#define RND_GPU_PROG_FR_REG_fTEX2                   6    //$
#define RND_GPU_PROG_FR_REG_fTEX3                   7    //$
#define RND_GPU_PROG_FR_REG_fTEX4                   8    //$
#define RND_GPU_PROG_FR_REG_fTEX5                   9    //$
#define RND_GPU_PROG_FR_REG_fTEX6                  10    //$
#define RND_GPU_PROG_FR_REG_fTEX7                  11    //$
#define RND_GPU_PROG_FR_REG_fNRML                  12    //$
#define RND_GPU_PROG_FR_REG_fCLP0                  13    //$
#define RND_GPU_PROG_FR_REG_fCLP1                  14    //$
#define RND_GPU_PROG_FR_REG_fCLP2                  15    //$
#define RND_GPU_PROG_FR_REG_fCLP3                  16    //$
#define RND_GPU_PROG_FR_REG_fCLP4                  17    //$
#define RND_GPU_PROG_FR_REG_fCLP5                  18    //$
#define RND_GPU_PROG_FR_REG_fFACE                  19    //$
#define RND_GPU_PROG_FR_REG_fLAYR                  20    //$
#define RND_GPU_PROG_FR_REG_fVPORT                 21    //$
#define RND_GPU_PROG_FR_REG_fFULLYCOVERED          22    //$
#define RND_GPU_PROG_FR_REG_fBARYCOORD             23    //$ barycentric coordinate
#define RND_GPU_PROG_FR_REG_fBARYNOPERSP           24    //$ barycoord w/o perspective scaling
#define RND_GPU_PROG_RF_REG_fSHADINGRATE           25    //$ shading rate for coarse shading.
#define RND_GPU_PROG_FR_REG_oCOL0                  26    //$ output color buffer 0
#define RND_GPU_PROG_FR_REG_oCOL1                  27    //$
#define RND_GPU_PROG_FR_REG_oCOL2                  28    //$
#define RND_GPU_PROG_FR_REG_oCOL3                  29    //$
#define RND_GPU_PROG_FR_REG_oCOL4                  30    //$
#define RND_GPU_PROG_FR_REG_oCOL5                  31    //$
#define RND_GPU_PROG_FR_REG_oCOL6                  32    //$
#define RND_GPU_PROG_FR_REG_oCOL7                  33    //$
#define RND_GPU_PROG_FR_REG_oCOL8                  34    //$
#define RND_GPU_PROG_FR_REG_oCOL9                  35    //$
#define RND_GPU_PROG_FR_REG_oCOL10                 36    //$
#define RND_GPU_PROG_FR_REG_oCOL11                 37    //$
#define RND_GPU_PROG_FR_REG_oCOL12                 38    //$
#define RND_GPU_PROG_FR_REG_oCOL13                 39    //$
#define RND_GPU_PROG_FR_REG_oCOL14                 40    //$
#define RND_GPU_PROG_FR_REG_oCOL15                 41    //$
#define RND_GPU_PROG_FR_REG_oDEP                   42    //$ output depth replace
#define RND_GPU_PROG_FR_REG_R0                     43    //$ temp registers
#define RND_GPU_PROG_FR_REG_R1                     44    //$
#define RND_GPU_PROG_FR_REG_R2                     45    //$
#define RND_GPU_PROG_FR_REG_R3                     46    //$
#define RND_GPU_PROG_FR_REG_R4                     47    //$
#define RND_GPU_PROG_FR_REG_R5                     48    //$
#define RND_GPU_PROG_FR_REG_R6                     49    //$
#define RND_GPU_PROG_FR_REG_R7                     50    //$
#define RND_GPU_PROG_FR_REG_R8                     51    //$
#define RND_GPU_PROG_FR_REG_R9                     52    //$
#define RND_GPU_PROG_FR_REG_R10                    53    //$
#define RND_GPU_PROG_FR_REG_R11                    54    //$
#define RND_GPU_PROG_FR_REG_R12                    55    //$
#define RND_GPU_PROG_FR_REG_R13                    56    //$
#define RND_GPU_PROG_FR_REG_R14                    57    //$
#define RND_GPU_PROG_FR_REG_R15                    58    //$
#define RND_GPU_PROG_FR_REG_A128                   59    //$ array-index reg (holds random 0..128 for use with pgmElw and cbuf)
#define RND_GPU_PROG_FR_REG_Const                  60    //$ place holder for the 96 (or 256) const regs
#define RND_GPU_PROG_FR_REG_LocalElw               61    //$ place holder for the local program elw[]
#define RND_GPU_PROG_FR_REG_ConstVect              62    //$ place holder where a const vector is being used
#define RND_GPU_PROG_FR_REG_Literal                63    //$ place holder where a literal constant is being used as 1st arg
#define RND_GPU_PROG_FR_REG_SMID                   64    //$
#define RND_GPU_PROG_FR_REG_SBO                    65    //$ special register that holds the ShaderBufferObject gpu address's
#define RND_GPU_PROG_FR_REG_Garbage                66    //$ special temp register that stores garbage results
#define RND_GPU_PROG_FR_REG_PaboElw                67    //$ place holder for const buffer object elw[] ie. program.buffer[a][b]
#define RND_GPU_PROG_FR_REG_TempArray              68    //$ temp array (for STL/LDL coverage)
#define RND_GPU_PROG_FR_REG_Subs                   69    //$ place holder for "Subs" array of fn-ptrs
#define RND_GPU_PROG_FR_REG_A1                     70    //$ array-index reg (holds random 0..1 for use with tmpArray)
#define RND_GPU_PROG_FR_REG_ASubs                  71    //$ array-index reg (holds random 0..NumSubs-1 for use with Subs)
#define RND_GPU_PROG_FR_REG_A3                     72    //$ array-index reg (holds random 0..NumTxHandles-1 for use with bindless textures)
#define RND_GPU_PROG_FR_REG_Atom64                 73    //$ special temp register to store 64 bit atomic masks
#define RND_GPU_PROG_FR_REG_NUM_REGS               74
#define RND_GPU_PROG_FR_OUT_MASK              IDX(RND_GPU_PROG_FR_BASE+11)   // Output register mask
#define RND_GPU_PROG_FR_SATURATE              IDX(RND_GPU_PROG_FR_BASE+12)
#define RND_GPU_PROG_FR_RELADDR               IDX(RND_GPU_PROG_FR_BASE+13)
#define RND_GPU_PROG_FR_IN_ABS                IDX(RND_GPU_PROG_FR_BASE+14)   // do/don't take absolute value of frag prog opcode input
#define RND_GPU_PROG_FR_IN_NEG                IDX(RND_GPU_PROG_FR_BASE+15)   // do/don't negate frag prog opcode input
#define RND_GPU_PROG_FR_TX_DIM                IDX(RND_GPU_PROG_FR_BASE+16)   // Which texture dim (1d/2d/rect/lwbe/3d) to fetch from.
#define RND_GPU_PROG_FR_TX_OFFSET             IDX(RND_GPU_PROG_FR_BASE+17)   // Which form of texture offset to use.
#define RND_GPU_PROG_FR_DEPTH_RQD             IDX(RND_GPU_PROG_FR_BASE+18)   // do/don't require the depth register to be written.
#define RND_GPU_PROG_FR_DRAW_BUFFERS          IDX(RND_GPU_PROG_FR_BASE+19)   // OPTION=ARB_draw_buffers in fragment programs
#define RND_GPU_PROG_FR_MULTISAMPLE           IDX(RND_GPU_PROG_FR_BASE+20)   // OPTION=LW_explicit_multisample in fragment programs
#define RND_GPU_PROG_FR_END_STRATEGY          IDX(RND_GPU_PROG_FR_BASE+21)   // which strategy to use for the fragment.color0 write
#define RND_GPU_PROG_FR_END_STRATEGY_frc    0       //$ write fractional value of interesting tmp reg
#define RND_GPU_PROG_FR_END_STRATEGY_mod    1       //$ treat input as UINT32 mod 257, MUL by 1/255.0
#define RND_GPU_PROG_FR_END_STRATEGY_coarse 2       //$ use coarse shading technique
#define RND_GPU_PROG_FR_END_STRATEGY_mov    3       //$ just copy the tmp reg to output (always used for float bufs)
#define RND_GPU_PROG_FR_END_STRATEGY_mrt_fs_pred 4  //$ write one of 8 color buffers based on SMID for floorsweeping (predicated writes)
#define RND_GPU_PROG_FR_END_STRATEGY_mrt_fs_cali 5  //$ write one of 8 color buffers based on SMID for floorsweeping (indexed jump)
#define RND_GPU_PROG_FR_END_STRATEGY_per_vtx_attr 6 //$ use an interpolated per-vertex attribute
#define RND_GPU_PROG_FR_END_STRATEGY_unknown -1     //$
#define RND_GPU_PROG_FR_DISABLE_CENTROID_SAMPLING IDX(RND_GPU_PROG_FR_BASE+22) //
#define RND_GPU_PROG_FR_INDEX_DEBUG           IDX(RND_GPU_PROG_FR_BASE+23)   // which program to use (debug override)

#define RND_GPU_PROG_NUM_PICKERS           IDX(RND_GPU_PROG_FR_BASE+24)

#define RND_TX_BASE                       (RND_TX*RND_BASE_OFFSET)
#define RND_TXU_ENABLE_MASK               IDX(RND_TX_BASE+ 0)   // bit mask of which textures are enabled
#define RND_TXU_WRAPMODE                  IDX(RND_TX_BASE+ 1)   // how to handle out-of-range S,T coordinates
#define RND_TXU_MIN_FILTER                IDX(RND_TX_BASE+ 2)   // how to map N texels down to N-M pixels
#define RND_TXU_MAG_FILTER                IDX(RND_TX_BASE+ 3)   // how to map N texels up to N+M pixels
#define RND_TXU_MIN_LAMBDA                IDX(RND_TX_BASE+ 4)   // min lambda (mipmap factor) (float)
#define RND_TXU_MAX_LAMBDA                IDX(RND_TX_BASE+ 5)   // max lambda (mipmap factor) (float)
#define RND_TXU_BIAS_LAMBDA               IDX(RND_TX_BASE+ 6)   // bias lambda (mipmap factor) (float)
#define RND_TXU_MAX_ANISOTROPY            IDX(RND_TX_BASE+ 7)   // max change in lambda across polygon
#define RND_TXU_BORDER_COLOR              IDX(RND_TX_BASE+ 8)   // border color r,g,b (float)
#define RND_TXU_BORDER_ALPHA              IDX(RND_TX_BASE+ 9)   // border alpha (float)
#define RND_TX_DEPTH_TEX_MODE             IDX(RND_TX_BASE+10)   // use depth as luminance/alpha/intensity
#define RND_TXU_COMPARE_MODE              IDX(RND_TX_BASE+11)   // perform shadow comparison
#define RND_TXU_SHADOW_FUNC               IDX(RND_TX_BASE+12)   // shadow comparison function
#define RND_TX_DIM                        IDX(RND_TX_BASE+13)   // texture dimensionality: 2D, 3D, lwbe-map, etc.
#define RND_TX_WIDTH                      IDX(RND_TX_BASE+14)   // texture width in texels at base mipmap level
#define RND_TX_HEIGHT                     IDX(RND_TX_BASE+15)   // texture height in texels at base mipmap level
#define RND_TX_BASE_MM                    IDX(RND_TX_BASE+16)   // texture base mipmap level (probably 0, for no minification)
#define RND_TX_MAX_MM                     IDX(RND_TX_BASE+17)   // texture max mipmap level (probably 100, for go all the way down to 1x1)
#define RND_TXU_REDUCTION_MODE            IDX(RND_TX_BASE+18)   // how to combine filtered texels
#define RND_TX_NAME                       IDX(RND_TX_BASE+19)   // which pretty image should we load into the texture memory?
#define RND_TX_NAME_Checkers                    0    //   value for RND_TX_NAME
#define RND_TX_NAME_Random                      1    //   value for RND_TX_NAME
#define RND_TX_NAME_SolidColor                  2    //   value for RND_TX_NAME
#define RND_TX_NAME_Tga                         3    //   value for RND_TX_NAME
#define RND_TX_NAME_MsCircles                   4    //   value for RND_TX_MS_NAME
#define RND_TX_NAME_MsSquares                   5    //   value for RND_TX_MS_NAME
#define RND_TX_NAME_MsStripes                   6    //   value for RND_TX_MS_NAME
#define RND_TX_NAME_MsRandom                    7    //   value for RND_TX_MS_NAME
#define RND_TX_NAME_FIRST_MATS_PATTERN          8
#define RND_TX_NAME_ShortMats                  0 + RND_TX_NAME_FIRST_MATS_PATTERN
#define RND_TX_NAME_LongMats                   1 + RND_TX_NAME_FIRST_MATS_PATTERN
#define RND_TX_NAME_WalkingOnes                2 + RND_TX_NAME_FIRST_MATS_PATTERN
#define RND_TX_NAME_WalkingZeros               3 + RND_TX_NAME_FIRST_MATS_PATTERN
#define RND_TX_NAME_SuperZeroOne               4 + RND_TX_NAME_FIRST_MATS_PATTERN
#define RND_TX_NAME_ByteSuper                  5 + RND_TX_NAME_FIRST_MATS_PATTERN
#define RND_TX_NAME_CheckerBoard               6 + RND_TX_NAME_FIRST_MATS_PATTERN
#define RND_TX_NAME_IlwCheckerBd               7 + RND_TX_NAME_FIRST_MATS_PATTERN
#define RND_TX_NAME_WalkSwizzle                8 + RND_TX_NAME_FIRST_MATS_PATTERN
#define RND_TX_NAME_WalkNormal                 9 + RND_TX_NAME_FIRST_MATS_PATTERN
#define RND_TX_NAME_DoubleZeroOne             10 + RND_TX_NAME_FIRST_MATS_PATTERN
#define RND_TX_NAME_TripleSuper               11 + RND_TX_NAME_FIRST_MATS_PATTERN
#define RND_TX_NAME_QuadZeroOne               12 + RND_TX_NAME_FIRST_MATS_PATTERN
#define RND_TX_NAME_TripleWhammy              13 + RND_TX_NAME_FIRST_MATS_PATTERN
#define RND_TX_NAME_IsolatedOnes              14 + RND_TX_NAME_FIRST_MATS_PATTERN
#define RND_TX_NAME_IsolatedZeros             15 + RND_TX_NAME_FIRST_MATS_PATTERN
#define RND_TX_NAME_SlowFalling               16 + RND_TX_NAME_FIRST_MATS_PATTERN
#define RND_TX_NAME_SlowRising                17 + RND_TX_NAME_FIRST_MATS_PATTERN
#define RND_TX_NAME_SolidZero                 18 + RND_TX_NAME_FIRST_MATS_PATTERN
#define RND_TX_NAME_SolidOne                  19 + RND_TX_NAME_FIRST_MATS_PATTERN
#define RND_TX_NAME_SolidAlpha                20 + RND_TX_NAME_FIRST_MATS_PATTERN
#define RND_TX_NAME_SolidRed                  21 + RND_TX_NAME_FIRST_MATS_PATTERN
#define RND_TX_NAME_SolidGreen                22 + RND_TX_NAME_FIRST_MATS_PATTERN
#define RND_TX_NAME_SolidBlue                 23 + RND_TX_NAME_FIRST_MATS_PATTERN
#define RND_TX_NAME_SolidWhite                24 + RND_TX_NAME_FIRST_MATS_PATTERN
#define RND_TX_NAME_SolidCyan                 25 + RND_TX_NAME_FIRST_MATS_PATTERN
#define RND_TX_NAME_SolidMagenta              26 + RND_TX_NAME_FIRST_MATS_PATTERN
#define RND_TX_NAME_SolidYellow               27 + RND_TX_NAME_FIRST_MATS_PATTERN
#define RND_TX_NAME_MarchingZeroes            28 + RND_TX_NAME_FIRST_MATS_PATTERN
#define RND_TX_NAME_HoppingZeroes             29 + RND_TX_NAME_FIRST_MATS_PATTERN
#define RND_TX_NAME_RunSwizzle                30 + RND_TX_NAME_FIRST_MATS_PATTERN
#define RND_TX_NAME_NUM_MATS_PATTERNS         31
#define RND_TX_NAME_NUM                       RND_TX_NAME_FIRST_MATS_PATTERN + RND_TX_NAME_NUM_MATS_PATTERNS
#define RND_TX_INTERNAL_FORMAT            IDX(RND_TX_BASE+20)   // how the driver should store the texture in memory: RGB, A, RGBA, compression, bits per color, etc.
#define RND_TX_DEPTH                      IDX(RND_TX_BASE+21)   // depth in pixels of 3D textures
#define RND_TX_NUM_OBJS                   IDX(RND_TX_BASE+22)   // Number of texture objects to load at each restart point
#define RND_TX_MAX_TOTAL_BYTES            IDX(RND_TX_BASE+23)   // Max total bytes of texture memory to consume when loading textures at a restart point.
#define RND_TX_CONST_COLOR                IDX(RND_TX_BASE+24)   // Color for use by RND_TX_NAME_SolidColor.
#define RND_TX_IS_NP2                     IDX(RND_TX_BASE+25)   // Should the texture have non-power-of-two dimensions?
#define RND_TX_MS_INTERNAL_FORMAT         IDX(RND_TX_BASE+26)   // internal format for multisampled textures
#define RND_TX_MS_MAX_SAMPLES             IDX(RND_TX_BASE+27)   // max samples for any given multisampled texture
#define RND_TX_MS_NAME                    IDX(RND_TX_BASE+28)   // kind of pattern to render into a multisampled texture
#define RND_TX_HAS_BORDER                 IDX(RND_TX_BASE+29)   // texture image has 1 texel border
#define RND_TX_LWBEMAP_SEAMLESS           IDX(RND_TX_BASE+30)   // seamless lwbemap texture filtering on/off
#define RND_TX_MSTEX_DEBUG_FILL           IDX(RND_TX_BASE+31)   // (debug) overwrite ms tex with debug data w/o changing random sequence
#define RND_TX_NUM_SPARSE_TEX                   2
#define RND_TX_SPARSE_ENABLE              IDX(RND_TX_BASE+32)   // enable sparse textures for this test?
#define RND_TX_SPARSE_TEX_INT_FMT_CLASS   IDX(RND_TX_BASE+33)   // select the internal format of the sparse texture
#define RND_TX_SPARSE_TEX_NUM_MMLEVELS    IDX(RND_TX_BASE+34)   // select the number of mipmap levels in sparse texture
#define RND_TX_SPARSE_TEX_NUM_LAYERS      IDX(RND_TX_BASE+35)   // select the number of layers in sparse texture
#define RND_TX_VIEW_ENABLE                IDX(RND_TX_BASE+36)   // enable texture view for this frame?
#define RND_TX_VIEW_BASE_MMLEVEL          IDX(RND_TX_BASE+37)   // select the base mipmap level of the texture view
#define RND_TX_VIEW_NUM_LAYERS            IDX(RND_TX_BASE+38)   // select the number of layers in the view
#define RND_TX_NUM_PICKERS                     39

#define RND_GEOM_BASE                     (RND_GEOM*RND_BASE_OFFSET)
#define RND_GEOM_BBOX_WIDTH               IDX(RND_GEOM_BASE+ 0)   // X width of bounding-box for each primitive
#define RND_GEOM_BBOX_HEIGHT              IDX(RND_GEOM_BASE+ 1)   // Y height of bounding-box for each primitive
#define RND_GEOM_BBOX_XOFFSET             IDX(RND_GEOM_BASE+ 2)   // X offset of bbox from previous bbox within row
#define RND_GEOM_BBOX_YOFFSET             IDX(RND_GEOM_BASE+ 3)   // Y offset of new bbox row from previous bbox row
#define RND_GEOM_BBOX_Z_BASE              IDX(RND_GEOM_BASE+ 4)   // Z center of BBOX (offset from max depth)
#define RND_GEOM_BBOX_DEPTH               IDX(RND_GEOM_BASE+ 5)   // Z depth of bounding box for each primitive
#define RND_GEOM_FRONT_FACE               IDX(RND_GEOM_BASE+ 6)   // is a clockwise poly front, or CCWise?
#define RND_GEOM_VTX_NORMAL_SCALING       IDX(RND_GEOM_BASE+ 7)   // how should vertex normals be forced to length 1.0?
#define RND_GEOM_VTX_NORMAL_SCALING_Unscaled    0    // GL will use my vertex normals as-is
#define RND_GEOM_VTX_NORMAL_SCALING_Scaled      1    // GL will scale my normals based on my modelview matrix
#define RND_GEOM_VTX_NORMAL_SCALING_Corrected   2    // GL will correct my normals to length 1.0
#define RND_GEOM_LWLL_FACE                IDX(RND_GEOM_BASE+ 8)   // what faces to lwll? (none, front, back, both)
#define RND_GEOM_BBOX_FACES_FRONT         IDX(RND_GEOM_BASE+ 9)   // should the polygons drawn in this bounding box face front (else back).
#define RND_GEOM_FORCE_BIG_SIMPLE         IDX(RND_GEOM_BASE+10)   // Force polygons to be big and simple (for simplifying debugging)
#define RND_GEOM_TXU_GEN_ENABLED          IDX(RND_GEOM_BASE+11)   // should texture coordinate generation be enabled for this texture unit?
#define RND_GEOM_TXU_GEN_MODE             IDX(RND_GEOM_BASE+12)   // what texture coord. gen. mode?
#define RND_GEOM_TXMX_OPERATION           IDX(RND_GEOM_BASE+13)   // what should we do with the texture matrix?
#define RND_GEOM_TXMX_OPERATION_Identity                     0    //   value for RND_GEOM_TXMX_OPERATION
#define RND_GEOM_TXMX_OPERATION_2dRotate                     1    //   value for RND_GEOM_TXMX_OPERATION
#define RND_GEOM_TXMX_OPERATION_2dTranslate                  2    //   value for RND_GEOM_TXMX_OPERATION
#define RND_GEOM_TXMX_OPERATION_2dScale                      3    //   value for RND_GEOM_TXMX_OPERATION
#define RND_GEOM_TXMX_OPERATION_2dAll                        4    //   value for RND_GEOM_TXMX_OPERATION
#define RND_GEOM_TXMX_OPERATION_Project                      5    //   value for RND_GEOM_TXMX_OPERATION
#define RND_GEOM_TXU_SPRITE_ENABLE_MASK   IDX(RND_GEOM_BASE+14)   // mask of texture units for which point sprite coord replacement should be enabled
#define RND_GEOM_USE_PERSPECTIVE          IDX(RND_GEOM_BASE+15)   // Use perspective projection (else orthographic).
#define RND_GEOM_USER_CLIP_PLANE_MASK     IDX(RND_GEOM_BASE+16)   // which user clip planes to enable? (bit mask)
#define RND_GEOM_MODELVIEW_OPERATION      IDX(RND_GEOM_BASE+17)   // what should we do with the modelview matrix?
#define RND_GEOM_MODELVIEW_OPERATION_Identity                0
#define RND_GEOM_MODELVIEW_OPERATION_Translate               1
#define RND_GEOM_MODELVIEW_OPERATION_Scale                   2
#define RND_GEOM_MODELVIEW_OPERATION_Rotate                  3
#define RND_GEOM_NUM_PICKERS                                18

#define RND_LIGHT_BASE                    (RND_LIGHT*RND_BASE_OFFSET)
#define RND_LIGHT_ENABLE                  IDX(RND_LIGHT_BASE+ 0)   // enable/not lighting
#define RND_LIGHT_SHADE_MODEL             IDX(RND_LIGHT_BASE+ 1)   // flat or smooth
#define RND_LIGHT_TYPE                    IDX(RND_LIGHT_BASE+ 2)   // directional, point-source, spotlight
#define RND_LIGHT_TYPE_Directional                            0    // light with no local position, just a direction (like sunlight)
#define RND_LIGHT_TYPE_PointSource                            1    // light with local position, radiating in all directions equally
#define RND_LIGHT_TYPE_SpotLight                              2    // local positioned light, radiating in a cone
#define RND_LIGHT_SPOT_LWTOFF             IDX(RND_LIGHT_BASE+ 3)   // angle (in degrees) at which spotlight lwts off
#define RND_LIGHT_NUM_ON                  IDX(RND_LIGHT_BASE+ 4)   // how many light sources should be enabled?
#define RND_LIGHT_BACK                    IDX(RND_LIGHT_BASE+ 5)   // does this light shine on the front or back of polygons?
#define RND_LIGHT_MODEL_LOCAL_VIEWER      IDX(RND_LIGHT_BASE+ 6)   // should we callwlate spelwlar highlights with a custom eye angle for each vertex?
#define RND_LIGHT_MODEL_TWO_SIDE          IDX(RND_LIGHT_BASE+ 7)   // should we light only front, or both front and back sides of polygons?
#define RND_LIGHT_MODEL_COLOR_CONTROL     IDX(RND_LIGHT_BASE+ 8)   // should spelwlar highlight color be callwlated independently of texturing?
#define RND_LIGHT_MAT_IS_EMISSIVE         IDX(RND_LIGHT_BASE+ 9)   // should this material "glow"?
#define RND_LIGHT_MAT                     IDX(RND_LIGHT_BASE+10)   // which material color property should glColor() control?
#define RND_LIGHT_SHININESS               IDX(RND_LIGHT_BASE+11)   // shininess (spelwlar exponent) for lighting
#define RND_LIGHT_COLOR                   IDX(RND_LIGHT_BASE+12)   // light color (spelwlar & diffuse) float, 3 calls per light
#define RND_LIGHT_AMBIENT                 IDX(RND_LIGHT_BASE+13)   // light color (ambient) float, 3 calls per light
#define RND_LIGHT_ALPHA                   IDX(RND_LIGHT_BASE+14)   // light alpha value (float)
#define RND_LIGHT_SPOT_EXPONENT           IDX(RND_LIGHT_BASE+15)   // spotlight falloff exponent
#define RND_LIGHT_NUM_PICKERS                  16

#define RND_FOG_BASE                      (RND_FOG*RND_BASE_OFFSET)
#define RND_FOG_ENABLE                    IDX(RND_FOG_BASE+ 0)   // enable/disable depth fog
#define RND_FOG_DIST_MODE                 IDX(RND_FOG_BASE+ 1)   // fog distance mode (eye-plane, eye-plane-abs, eye-radial)
#define RND_FOG_MODE                      IDX(RND_FOG_BASE+ 2)   // fog intensity callwlation mode: linear/exp/exp2
#define RND_FOG_DENSITY                   IDX(RND_FOG_BASE+ 3)   // coefficient for exp/exp2 modes
#define RND_FOG_START                     IDX(RND_FOG_BASE+ 4)   // starting eye-space Z depth for linear mode
#define RND_FOG_END                       IDX(RND_FOG_BASE+ 5)   // ending eye-space Z depth for linear mode
#define RND_FOG_COORD                     IDX(RND_FOG_BASE+ 6)   // where does fog distance come from? explicit per-vertex fog coord or fragment Z?
#define RND_FOG_COLOR                     IDX(RND_FOG_BASE+ 7)   // Fog color (float, called 3 times per loop)
#define RND_FOG_NUM_PICKERS                     8

#define RND_FRAG_BASE                     (RND_FRAG*RND_BASE_OFFSET)
#define RND_FRAG_ALPHA_FUNC                  IDX(RND_FRAG_BASE+ 0)   //$ fragment alpha test function
#define RND_FRAG_BLEND_ENABLE                IDX(RND_FRAG_BASE+ 1)   //$ do/don't enable blending
#define RND_FRAG_BLEND_FUNC                  IDX(RND_FRAG_BASE+ 2)   //$ scaling function for color in blending
#define RND_FRAG_BLEND_EQN                   IDX(RND_FRAG_BASE+ 3)   //$ what op to do with the scaled source and dest (normally addition)
#define RND_FRAG_COLOR_MASK                  IDX(RND_FRAG_BASE+ 4)   //$ do/don't write each color channel (4 bit mask)
#define RND_FRAG_DEPTH_FUNC                  IDX(RND_FRAG_BASE+ 5)   //$ fragment Z test function
#define RND_FRAG_DEPTH_MASK                  IDX(RND_FRAG_BASE+ 6)   //$ do/don't write Z if depth test passes
#define RND_FRAG_DITHER_ENABLE               IDX(RND_FRAG_BASE+ 7)   //$ do/don't enable dithering
#define RND_FRAG_LOGIC_OP                    IDX(RND_FRAG_BASE+ 8)   //$ fragment logic op
#define RND_FRAG_STENCIL_FUNC                IDX(RND_FRAG_BASE+ 9)   //$ comparison with stencil buffer for fragment testing
#define RND_FRAG_STENCIL_OP                  IDX(RND_FRAG_BASE+10)   //$ what to do to the stencil buffer when op passes/fail
#define RND_FRAG_SCISSOR_ENABLE              IDX(RND_FRAG_BASE+11)   //$ enable scissor box that prevents drawing to the outer 5 pixels of the screen.
#define RND_FRAG_BLEND_COLOR                 IDX(RND_FRAG_BASE+12)   //$ blend color r, g, or b (float)
#define RND_FRAG_BLEND_ALPHA                 IDX(RND_FRAG_BASE+13)   //$ blend color alpha value (float)
#define RND_FRAG_STENCIL_2SIDE               IDX(RND_FRAG_BASE+14)   //$ use the LW_stencil_two_side extension (if present)
#define RND_FRAG_DEPTH_CLAMP                 IDX(RND_FRAG_BASE+15)   //$ use the LW_depth_clamp extension to clamp at the near/far planes instead of clipping.
#define RND_FRAG_VIEWPORT_OFFSET_X           IDX(RND_FRAG_BASE+16)   //$ Offset and reduce the viewport by this many pixels in x
#define RND_FRAG_VIEWPORT_OFFSET_Y           IDX(RND_FRAG_BASE+17)   //$ Offset and reduce the viewport by this many pixels in y
#define RND_FRAG_DEPTH_BOUNDS_TEST           IDX(RND_FRAG_BASE+18)   //$ do/don't enable GL_LW_depth_bounds_test
#define RND_FRAG_MULTISAMPLE_ENABLE          IDX(RND_FRAG_BASE+19)   //$ do/don't enable multisampling
#define RND_FRAG_ALPHA_TO_COV_ENABLE         IDX(RND_FRAG_BASE+20)   //$ do/don't enable alpha-to-coverage
#define RND_FRAG_ALPHA_TO_ONE_ENABLE         IDX(RND_FRAG_BASE+21)   //$ do/don't enable alpha-to-one
#define RND_FRAG_SAMPLE_COVERAGE_ENABLE      IDX(RND_FRAG_BASE+22)   //$ do/don't enable sample-to-coverage
#define RND_FRAG_SAMPLE_COVERAGE             IDX(RND_FRAG_BASE+23)   //$ sample coverage fraction
#define RND_FRAG_SAMPLE_COVERAGE_ILWERT      IDX(RND_FRAG_BASE+24)   //$ ilwert sample coverage
#define RND_FRAG_STENCIL_WRITE_MASK          IDX(RND_FRAG_BASE+25)   //$ glStencilMask, write mask
#define RND_FRAG_STENCIL_READ_MASK           IDX(RND_FRAG_BASE+26)   //$ glStencilFunc, read/compare mask
#define RND_FRAG_STENCIL_REF                 IDX(RND_FRAG_BASE+27)   //$ glStencilFunc, compare value
#define RND_FRAG_RASTER_DISCARD              IDX(RND_FRAG_BASE+28)   //$ glEnable(GL_RASTERIZER_DISCARD_LW) not used by default
#define RND_FRAG_ADV_BLEND_EQN               IDX(RND_FRAG_BASE+29)   //$ what advanced color blending equations to use
#define RND_FRAG_ADV_BLEND_PREMULTIPLIED_SRC IDX(RND_FRAG_BASE+30)   //$ enable/disable pre-multiplied RGB src
#define RND_FRAG_ADV_BLEND_OVERLAP_MODE      IDX(RND_FRAG_BASE+31)   //$ type of overlap mode to assume
#define RND_FRAG_ADV_BLEND_ENABLE            IDX(RND_FRAG_BASE+32)   //$ enable/disable advanced blending modes(requires OpenGL 4.1).
#define RND_FRAG_VPORT_X                     IDX(RND_FRAG_BASE+33)   //$ X coordinate of the pixel at which the 4 viewports intersect.
#define RND_FRAG_VPORT_Y                     IDX(RND_FRAG_BASE+34)   //$ Y coordinate of the pixel at which the 4 viewports intersect.
#define RND_FRAG_VPORT_SEP                   IDX(RND_FRAG_BASE+35)   //$ Separation between the 4 viewports
#define RND_FRAG_VPORT_SWIZZLE_X             IDX(RND_FRAG_BASE+36)   //$ Swizzle value for vport along X
#define RND_FRAG_VPORT_SWIZZLE_Y             IDX(RND_FRAG_BASE+37)   //$ Swizzle value for vport along Y
#define RND_FRAG_VPORT_SWIZZLE_Z             IDX(RND_FRAG_BASE+38)   //$ Swizzle value for vport along Z
#define RND_FRAG_VPORT_SWIZZLE_W             IDX(RND_FRAG_BASE+39)   //$ Swizzle value for vport along W
#define RND_FRAG_CONS_RASTER_ENABLE          IDX(RND_FRAG_BASE+40)   //$ Enable/disable conservative raster
#define RND_FRAG_CONS_RASTER_DILATE_VALUE    IDX(RND_FRAG_BASE+41)   //$ Conservative raster dilation value
#define RND_FRAG_CONS_RASTER_UNDERESTIMATE   IDX(RND_FRAG_BASE+42)   //$ Enable conservative raster underestimation
#define RND_FRAG_CONS_RASTER_SNAP_MODE       IDX(RND_FRAG_BASE+43)   //$ Set conservative raster pre-snapping modes
#define RND_FRAG_CONS_RASTER_SUBPIXEL_BIAS_X IDX(RND_FRAG_BASE+44)   //$ Set subpixel bias in conservative raster along X
#define RND_FRAG_CONS_RASTER_SUBPIXEL_BIAS_Y IDX(RND_FRAG_BASE+45)   //$ Set subpixel bias in conservative raster along Y
#define RND_FRAG_SHADING_RATE_CONTROL_ENABLE IDX(RND_FRAG_BASE+46)   //$ do/don't enable coarse/fine shading
#define RND_FRAG_SHADING_RATE_IMAGE_ENABLE   IDX(RND_FRAG_BASE+47)   //$ do/don't bind the shading rate image texture.
#define RND_FRAG_NUM_PICKERS                 48

#define RND_PRIM_BASE                     (RND_PRIM*RND_BASE_OFFSET)
#define RND_PRIM_POINT_SPRITE_ENABLE      IDX(RND_PRIM_BASE+ 0)   //$ enable/disable point sprites
#define RND_PRIM_POINT_SPRITE_R_MODE      IDX(RND_PRIM_BASE+ 1)   //$ R texture coordinate mode for point sprites
#define RND_PRIM_POINT_SIZE               IDX(RND_PRIM_BASE+ 2)   //$ point size (float)
#define RND_PRIM_POINT_ATTEN_ENABLE       IDX(RND_PRIM_BASE+ 3)   //$ enable the point parameters extension for distance attenuation of points
#define RND_PRIM_POINT_SIZE_MIN           IDX(RND_PRIM_BASE+ 4)   //$ min size of point that is distance-shrunk (EXT_point_parameters)
#define RND_PRIM_POINT_THRESHOLD          IDX(RND_PRIM_BASE+ 5)   //$ size of point at which it starts alpha fading (EXT_point_parameters)
#define RND_PRIM_LINE_SMOOTH_ENABLE       IDX(RND_PRIM_BASE+ 6)   //$ enable/disable line anti-aliasing
#define RND_PRIM_LINE_WIDTH               IDX(RND_PRIM_BASE+ 7)   //$ line width in pixels (float)
#define RND_PRIM_LINE_STIPPLE_ENABLE      IDX(RND_PRIM_BASE+ 8)   //$ enable/disable line stippling
#define RND_PRIM_LINE_STIPPLE_REPEAT      IDX(RND_PRIM_BASE+ 9)   //$ line stipple repeat factor
#define RND_PRIM_POLY_MODE                IDX(RND_PRIM_BASE+10)   //$ one of point, line, or fill
#define RND_PRIM_POLY_STIPPLE_ENABLE      IDX(RND_PRIM_BASE+11)   //$ enable/disable polygon stippling
#define RND_PRIM_POLY_SMOOTH_ENABLE       IDX(RND_PRIM_BASE+12)   //$ do/don't alpha smooth polygon edges
#define RND_PRIM_POLY_OFFSET_ENABLE       IDX(RND_PRIM_BASE+13)   //$ enable/disable polygon Z offset
#define RND_PRIM_POLY_OFFSET_FACTOR       IDX(RND_PRIM_BASE+14)   //$ slope term of polygon offset
#define RND_PRIM_POLY_OFFSET_UNITS        IDX(RND_PRIM_BASE+15)   //$ constant term of polygon offset
#define RND_PRIM_STIPPLE_BYTE             IDX(RND_PRIM_BASE+16)   //$ called for each byte of the poly and line stipple patterns on restart points.
#define RND_PRIM_NUM_PICKERS                   17

#define RND_VXFMT_BASE                    (RND_VXFMT*RND_BASE_OFFSET)
#define RND_VXFMT_COORD_SIZE              IDX(RND_VXFMT_BASE+ 0)   //$ 2 to 4 for xy, xyz, or xyzw coordinates
#define RND_VXFMT_COORD_DATA_TYPE         IDX(RND_VXFMT_BASE+ 1)   //$ GL_FLOAT or GL_SHORT.
#define RND_VXFMT_HAS_NORMAL              IDX(RND_VXFMT_BASE+ 2)   //$ has/not got a normal vector per vertex
#define RND_VXFMT_NORMAL_DATA_TYPE        IDX(RND_VXFMT_BASE+ 3)   //$ double, float, short, etc.
#define RND_VXFMT_HAS_PRICOLOR            IDX(RND_VXFMT_BASE+ 4)   //$ has/not got a color per vertex
#define RND_VXFMT_HAS_EDGE_FLAG           IDX(RND_VXFMT_BASE+ 5)   //$ has/not got edge flag per vertex
#define RND_VXFMT_TXU_COORD_SIZE          IDX(RND_VXFMT_BASE+ 6)   //$ number of texture coordinates: 2, 3 == st, str
#define RND_VXFMT_HAS_SECCOLOR            IDX(RND_VXFMT_BASE+ 7)   //$ do/don't send per-vertex attributes that don't have their own pickers: WGHT, COL1, FOGC, 6, 7
#define RND_VXFMT_HAS_FOG_COORD           IDX(RND_VXFMT_BASE+ 8)   //$ do/don't send per-vertex fog coords
#define RND_VXFMT_INDEX_TYPE              IDX(RND_VXFMT_BASE+ 9)   //$ data type for vertex indexes
#define RND_VXFMT_NUM_PICKERS                  10

#define RND_VX_BASE                       (RND_VX*RND_BASE_OFFSET)
#define RND_VX_PRIMITIVE                  IDX(RND_VX_BASE+ 0)   //$ which primitive to draw: GL_POINTS, GL_TRIANGLES, etc.
#define RND_VX_PRIMITIVE_Solid                           15     //$   (instead of GL_POINT .. GL_POLYGON, draws a "solid" closed colwex mesh as tri-strips)
#define RND_VX_SEND_METHOD                IDX(RND_VX_BASE+ 1)   //$ how should vertexes be sent?
#define RND_VX_SEND_METHOD_Immediate                       0    //$ use glBegin() glEnd(), with data passed on the stack.
#define RND_VX_SEND_METHOD_Vector                          1    //$ use glBegin() glEnd(), with data passed as pointers.
#define RND_VX_SEND_METHOD_Array                           2    //$ use glBegin() glEnd(), with vertex arrays and glArrayElement()
#define RND_VX_SEND_METHOD_DrawArrays                      3    //$ use glDrawArray() with vertex arrays and sequential indexing.
#define RND_VX_SEND_METHOD_DrawArraysInstanced             4    //$ use glDrawArraysInstanced() with vertex arrays and sequential indexing.
#define RND_VX_SEND_METHOD_DrawElements                    5    //$ use glDrawElements() with vertex arrays and an index array
#define RND_VX_SEND_METHOD_DrawElementsInstanced           6    //$ use glDrawElementsInstanced() with vertex arrays and an index array
#define RND_VX_SEND_METHOD_DrawRangeElements               7    //$ use glDrawRangeElements() with vertex arrays and an index array
#define RND_VX_SEND_METHOD_DrawElementArray                8    //$ use glDrawElementArrayLW() with vertex arrays and vertex index arrays.
#define RND_VX_SEND_METHOD_NUM_METHODS                     9
#define RND_VX_PER_LOOP                   IDX(RND_VX_BASE+ 2)   //$ number of random vertexes to generate within each bbox
#define RND_VX_W                          IDX(RND_VX_BASE+ 3)   //$ vertex W (float)
#define RND_VX_EDGE_FLAG                  IDX(RND_VX_BASE+ 4)   //$ is this an edge or not (for wireframe mode)
#define RND_VX_VBO_ENABLE                 IDX(RND_VX_BASE+ 5)   //$ enable vertex buffer objects with vertex arrays
#define RND_VX_SECCOLOR_BYTE              IDX(RND_VX_BASE+ 6)   //$ secondary (spelwlar) color r, g, or b
#define RND_VX_COLOR_BYTE                 IDX(RND_VX_BASE+ 7)   //$ primary color (material color) r, g, b
#define RND_VX_ALPHA_BYTE                 IDX(RND_VX_BASE+ 8)   //$ primary color (material color) transparency
#define RND_VX_NEW_GROUP                  IDX(RND_VX_BASE+ 9)   //$ called 17 times for laying out vertex array data: if always 0, all vertexes in one struct, max stride.
#define RND_VX_DO_SHUFFLE_STRUCT          IDX(RND_VX_BASE+10)   //$ if false, (if if not vxarray), don't shuffle the order of vx attribs in vertex struct.
#define RND_VX_PRIM_RESTART               IDX(RND_VX_BASE+11)   //$ do/don't use LW_primitive_restart with vertex arrays.
#define RND_VX_COLOR_WHITE                IDX(RND_VX_BASE+12)   //$ do/don't force color==white
#define RND_VX_MAXTOSEND                  IDX(RND_VX_BASE+13)   //$ (debugging) if < VX_PER_LOOP, rest of vertexes are not sent.
#define RND_VX_INSTANCE_COUNT             IDX(RND_VX_BASE+14)   //$ used with DrawArraysInstanced and DrawElementsInstanced
#define RND_VX_ACTIVATE_BUBBLE            IDX(RND_VX_BASE+15)   //$ activate a pause bubble
#define RND_VX_NUM_PICKERS                                16

#define RND_PXL_BASE                     (RND_PXL*RND_BASE_OFFSET)
#define RND_PIXEL_FMT                    IDX(RND_PXL_BASE+ 0)   //$ specific pixel format to encode pixel data.
#define RND_PIXEL_DATA_TYPE              IDX(RND_PXL_BASE+ 1)   //$ data type for the data.
#define RND_PIXEL_WIDTH                  IDX(RND_PXL_BASE+ 2)   //$ width of the pixel rectangle.
#define RND_PIXEL_HEIGHT                 IDX(RND_PXL_BASE+ 3)   //$ height of the pixel rectangle.
#define RND_PIXEL_POS_X                  IDX(RND_PXL_BASE+ 4)   //$ x coordinate of the pixel rectangle.
#define RND_PIXEL_POS_Y                  IDX(RND_PXL_BASE+ 5)   //$ y coordinate of the pixel rectangle.
#define RND_PIXEL_POS_Z                  IDX(RND_PXL_BASE+ 6)   //$ y coordinate of the pixel rectangle.
#define RND_PIXEL_DATA_BYTE              IDX(RND_PXL_BASE+ 7)   //$ used when data type is a byte for unit of the pixel data.
#define RND_PIXEL_DATA_SHORT             IDX(RND_PXL_BASE+ 8)   //$ used when data type is a byte for unit of the pixel data.
#define RND_PIXEL_DATA_INT               IDX(RND_PXL_BASE+ 9)   //$ used when data type is a byte for unit of the pixel data.
#define RND_PIXEL_DATA_FLOAT             IDX(RND_PXL_BASE+10)   //$ used when data type is a float for unit of the pixel data.
#define RND_PIXEL_FMT_RGBA               IDX(RND_PXL_BASE+11)   //$ special data type condition, picks GL_RGBA or GL_BGRA format
#define RND_PIXEL_FMT_STENCIL            IDX(RND_PXL_BASE+12)   //$ special data type condition, picks GL_STENCIL_INDEX or GL_COLOR_INDEX
#define RND_PIXEL_DATA_ALIGNMENT         IDX(RND_PXL_BASE+13)   //$ data alignment for pixel rows
#define RND_PIXEL_FILL_MODE              IDX(RND_PXL_BASE+14)   //$ fill rects with solid, stripes, or random colors.
#define RND_PIXEL_FILL_MODE_solid                          0
#define RND_PIXEL_FILL_MODE_stripes                        1
#define RND_PIXEL_FILL_MODE_random                         2
#define RND_PIXEL_3D_NUM_TEXTURES        IDX(RND_PXL_BASE+15)   //$ Number of textures to allocate for 3D pixels
#define RND_PIXEL_3D_TEX_FORMAT          IDX(RND_PXL_BASE+16)   //$ Texture format to use for 3D pixels
#define RND_PIXEL_3D_S0                  IDX(RND_PXL_BASE+17)   //$ Texture sampling s coordinate (rectangle corner 0) for 3D pixels (normalized)
#define RND_PIXEL_3D_T0                  IDX(RND_PXL_BASE+18)   //$ Texture sampling t coordinate (rectangle corner 0) for 3D pixels (normalized)
#define RND_PIXEL_3D_S1                  IDX(RND_PXL_BASE+19)   //$ Texture sampling s coordinate (rectangle corner 1) for 3D pixels (normalized)
#define RND_PIXEL_3D_T1                  IDX(RND_PXL_BASE+20)   //$ Texture sampling t coordinate (rectangle corner 1) for 3D pixels (normalized)
#define RND_PIXEL_NUM_PICKERS                   21

//
// Texture attrib bits & masks, for matching our frag programs
// and vertex programs with the correct bound textures.
//
// The textures have a mask of all the attrib bits that are TRUE for that
// texture.
//
// The programs have a mask of all the bits they REQUIRE from a
// texture.
//
ENUM_BEGIN(TexAttribBits)
ENUM_CONST(IsLegacy,           0x00000002)
ENUM_CONST(IsUnSigned,         0x00000008)
ENUM_CONST(IsSigned,           0x00000040)
ENUM_CONST(IsFloat,            0x00000080)
ENUM_CONST(IsDepth,            0x00000200)
ENUM_CONST(Is1d,               0x00000400)
ENUM_CONST(Is2d,               0x00000800)
ENUM_CONST(Is3d,               0x00001000)
ENUM_CONST(IsLwbe,             0x00002000)
ENUM_CONST(IsArray1d,          0x00010000)
ENUM_CONST(IsArray2d,          0x00020000)
ENUM_CONST(IsArrayLwbe,        0x00040000)
ENUM_CONST(IsShadow1d,         0x00080000)
ENUM_CONST(IsShadow2d,         0x00100000)
ENUM_CONST(IsShadowLwbe,       0x00400000)
ENUM_CONST(IsShadowArray1d,    0x00800000)
ENUM_CONST(IsShadowArray2d,    0x01000000)
ENUM_CONST(IsShadowArrayLwbe,  0x02000000)
ENUM_CONST(Is2dMultisample,    0x04000000)
ENUM_CONST(IsNoFilter,         0x20000000)
ENUM_CONST(IsNoBorder,         0x40000000)
ENUM_CONST(IsShadowMask,       0x03f80000)
ENUM_CONST(IsDimMask,          0x07fffc00)
ENUM_CONST(IsNoClamp,          0x80000000)
ENUM_CONST(SDimFlags,          Is1d | Is2d | Is3d | IsLwbe)
ENUM_CONST(ADimFlags,          IsArray1d | IsArray2d | IsArrayLwbe |IsShadowArray1d |IsShadowArray2d |IsShadowArrayLwbe) //$
ENUM_CONST(SADimFlags,         SDimFlags | ADimFlags)
ENUM_CONST(SAMDimFlags,        SADimFlags | Is2dMultisample)
ENUM_CONST(SMDimFlags,         SDimFlags | Is2dMultisample)
ENUM_CONST(SASMDimFlags,       SAMDimFlags | IsShadowMask)
ENUM_CONST(SASMNbDimFlags,     SASMDimFlags | IsNoBorder)
ENUM_END

//
// Indicies into the SBO for bindless texture object handles. These indices
// coorespond to the bit positions of the target types above, Is1d..IsArrayLwbe
// Note1:There are too many restrictions for shadow & multisample targets so we
//       won't implement bindless textures for those targets.
// Note2:To use image units with bindless textures and dynamically query the
//       Level Of Detail (LOD) for the bound image we must bind a tex. fetcher
//       to each texture object. Even though it wont get used for the actual
//       texture instruction.
ENUM_BEGIN(TexIndex)
ENUM_CONST(Ti1d,               0)
ENUM_CONST(Ti2d,               1)
ENUM_CONST(Ti3d,               2)
ENUM_CONST(TiLwbe,             3)
ENUM_CONST(TiArray1d,          4)
ENUM_CONST(TiArray2d,          5)
ENUM_CONST(TiArrayLwbe,        6)
ENUM_CONST(TiNUM_Indicies,     7)
ENUM_CONST(NumTxHandles,       4) // 4 texture object handles per texture target.
ENUM_END
// Instruction's internal texel offset form to use for texture instructions.
//
ENUM_BEGIN(TexOffset)
ENUM_CONST(toNone,         0x00)   //!< No texel offset
ENUM_CONST(toConstant,     0x01)   //!< Use constant form of texel offset
ENUM_CONST(toProgrammable, 0x02)   //!< Use programmable form of texel offset
ENUM_END

//
// Instruction's internal output mask (some instr. don't write to all 4 components)
// For instruction-table declarations.
//
ENUM_BEGIN(OutputMask)
ENUM_CONST(omNone,     0x00)  //!< No components are written
ENUM_CONST(omX,        0x01)  //!< Only X component is written
ENUM_CONST(omXY,       0x03)  //!< Only X, Y components are written
ENUM_CONST(omXYZ,      0x07)  //!< only X, Y, Z components are written
ENUM_CONST(omXYZW,     0x0f)  //!< all components are written.
ENUM_CONST(omPassThru, 0x80)  //!< these components are simply passed-thru the program
ENUM_END

//
// Runtime input/output mask constants -- all combinations of xyzw are possible.
//
ENUM_BEGIN(ComponentsUsed)
ENUM_CONST(compNone,   0x00)
ENUM_CONST(compX,      0x01)
ENUM_CONST(compY,      0x02)
ENUM_CONST(compXY,     0x03)
ENUM_CONST(compZ,      0x04)
ENUM_CONST(compXZ,     0x05)
ENUM_CONST(compYZ,     0x06)
ENUM_CONST(compXYZ,    0x07)
ENUM_CONST(compW,      0x08)
ENUM_CONST(compXW,     0x09)
ENUM_CONST(compYW,     0x0a)
ENUM_CONST(compXYW,    0x0b)
ENUM_CONST(compZW,     0x0c)
ENUM_CONST(compXZW,    0x0d)
ENUM_CONST(compYZW,    0x0e)
ENUM_CONST(compXYZW,   0x0f)
ENUM_END

// Individual component identifiers used in the <swizzleSuffix> grammar
ENUM_BEGIN(SwizzleComponent)
ENUM_CONST(scX,        0x00)
ENUM_CONST(scY,        0x01)
ENUM_CONST(scZ,        0x02)
ENUM_CONST(scW,        0x03)
ENUM_END

//
// ProgProperty -- runtime input/output data requirements for programs.
//
// Used to declare the required inputs or available outputs from vertex,
// geometry, fragment, etc. programs.
//
                                      // input/output requirements
                                      //   Vertex   Tess   Geometry   Fragment
ENUM_BEGIN(ProgProperty)              //    - -     - -     - -        - -
ENUM_CONST(ppIlwalid,          -1)    //!<  - -     - -     - -        - -
ENUM_CONST(ppPos,               0)    //!<  I O     I O     I O        I -
ENUM_CONST(ppNrml,              1)    //!<  I -     - -     - -        - -
ENUM_CONST(ppPriColor,          2)    //!<  I O     I O     I O        I O
ENUM_CONST(ppSecColor,          3)    //!<  I O     I O     I O        I -
ENUM_CONST(ppBkPriColor,        4)    //!<  - O     I O     I O        - -
ENUM_CONST(ppBkSecColor,        5)    //!<  - O     I O     I O        - -
ENUM_CONST(ppFogCoord,          6)    //!<  I O     I O     I O        I -
ENUM_CONST(ppPtSize,            7)    //!<  - O     I O     I O        - -
ENUM_CONST(ppVtxId,             8)    //!<  I O     I O     I -        - -
ENUM_CONST(ppInstance,          9)    //!<  I -     - -     I -        - -
ENUM_CONST(ppFacing,           10)    //!<  - -     - -     - -        I -
ENUM_CONST(ppDepth,            11)    //!<  - -     - -     - -        - O
ENUM_CONST(ppPrimId,           12)    //!<  - -     - O     I O        I -
ENUM_CONST(ppLayer,            13)    //!<  - O     - O     - O        I -
ENUM_CONST(ppViewport,         14)    //!<  - O     - O     - O        I -
ENUM_CONST(ppPrimitive,        15)    //!<  - -     I O     I -        - -
ENUM_CONST(ppFullyCovered,     16)    //!<  - -     - -     - -        I -
ENUM_CONST(ppBaryCoord,        17)    //!<  - -     - -     - -        I -
ENUM_CONST(ppBaryNoPersp,      18)    //!<  - -     - -     - -        I -
ENUM_CONST(ppShadingRate,      19)    //!<  - -     - -     - -        I -
ENUM_CONST(ppClip0,            20)    //!<  - O     I O     I O        I -
ENUM_CONST(ppClip5,            25)    //!<  - O     I O     I O        I -
ENUM_CONST(ppGenAttr0,         26)    //!<  I O     I O     I O        I -
ENUM_CONST(ppGenAttr31,        57)    //!<  I O     I O     I O        I -
ENUM_CONST(ppClrBuf0,          58)    //!<  - -     - -     - -        - O
ENUM_CONST(ppClrBuf15,         73)    //!<  - -     - -     - -        - O
ENUM_CONST(ppTxCd0,            74)    //!<  I O     I O     I O        I -
ENUM_CONST(ppTxCd7,            81)    //!<  I O     I O     I O        I -
ENUM_CONST(ppNUM_ProgProperty, 82)
ENUM_END

ENUM_BEGIN(ppIOAttr)
ENUM_CONST(ppIONone,            0)    //!< property is not accessable
ENUM_CONST(ppIORd,              1)    //!< property can be read
ENUM_CONST(ppIOWr,              2)    //!< property can be written
ENUM_CONST(ppIORw,              3)    //!< property can be read or written
ENUM_END

// Instruction data type -- controls the instruction suffix.
//
// Note that glrandom programs use untyped temp vars to hold a random mix
// of data types.  We obey only the unavoidable data-size restrictions.
//
ENUM_BEGIN(opDataType)
ENUM_CONST(dtNA,           0)
ENUM_CONST(dtNone,         0)
        // base types
ENUM_CONST(dtF,            1)   //!< Float of some size
ENUM_CONST(dtS,            2)   //!< Signed int of some size
ENUM_CONST(dtU,            4)   //!< Unsigned int of some size

        // size/precision
ENUM_CONST(dt8,            8)   //!< 8-bit int (LOAD, STORE, CVT)
ENUM_CONST(dt16,        0x10)   //!< 16-bit int/float (LOAD, STORE, CVT)
ENUM_CONST(dt24,        0x20)   //!< 24-bit precision int (MUL)
ENUM_CONST(dt32,        0x40)   //!< 32-bit float or int implicitly declared
ENUM_CONST(dt32e,       0x80)   //!< 32-bit float or int explicitly declared
ENUM_CONST(dt64,       0x100)   //!< 64-bit float or int (fermi+)
ENUM_CONST(dtHi32,     0x200)   //!< hi32 of 32x32->64 int (MUL)
ENUM_CONST(dtSizeMask, 0x3f8)   //!< mask of all possible sizes.

        // Unique type masks
ENUM_CONST(dt16f,      0x400)   //!< 16-bit float only (no int or uint)
        // Optional component count storage modifiers
ENUM_CONST(dtX2,       0x800)  //!< access 2 values
ENUM_CONST(dtX4,      0x1000)  //!< access 4 values
        // special purpose modifiers
ENUM_CONST(dta,       0x2000)  //!< all bits must match,

ENUM_CONST(dtRun,     0x8000)  //!< Data type is determined at runtime.
        // Combos for OpData::TypeMask
ENUM_CONST(dtI,        dtS|dtU)
ENUM_CONST(dtFI,       dtF|dtI)
ENUM_CONST(dtFx,       dtF |dt32)
ENUM_CONST(dtF6,       dtF |dt32|dt64)
ENUM_CONST(dtF6f,      dtF6|dt16f)
ENUM_CONST(dtFIx,      dtFI|dt32)
ENUM_CONST(dtFIxf,     dtFIx|dt16f)
ENUM_CONST(dtFSx,      dtF|dtS|dt32)  // KIL doesn't accept unsigned
ENUM_CONST(dtFI6,      dtFI|dt32|dt64)
ENUM_CONST(dtFI6e,     dtFI|dt32e|dt64)
ENUM_CONST(dtFI6f,     dtFI6 | dt16f)
ENUM_CONST(dtI6,       dtI |dt32|dt64)
ENUM_CONST(dtIx,       dtI |dt32)
ENUM_CONST(dtFIe,      dtFI|dt64|dt32e|dt16|dt8)       //$ CVT requires explicit .F32 declaration
ENUM_CONST(dtFe,       dtF |dt64|dt32e|dt16|dt8)       //$ CVT requires explicit .F32 declaration
ENUM_CONST(dtMUL,      dtFI|dt24|dt32|dt64|dtHi32)     //$ special case for MUL
ENUM_CONST(dtMULf,     dtMUL|dt16f)                    //$ special case for MUL
ENUM_CONST(dtFI6X4,    dtFI6e|dtX4|dtX2)               //$ special case for memory transactions, LOAD/STORE
ENUM_CONST(dtFI32e,    dtFI|dt32e|dt24|dt16|dt8)       //$ special case for STORE using current implementation
ENUM_CONST(dtFI32eX4,  dtFI|dt32e|dtX4|dtX2)           //$ special case for LOADIM/STOREIM/LDC

        // Combos for RND_GPU_PROG_OPCODE_DATA_TYPE and OpData::DefaultType
ENUM_CONST(dtF16,      dtF|dt16)
ENUM_CONST(dtF32,      dtF|dt32)
ENUM_CONST(dtF32e,     dtF|dt32e)
ENUM_CONST(dtF64,      dtF|dt64)
ENUM_CONST(dtF16X2a,   dtF16|dtX2|dta)
ENUM_CONST(dtF16X4a,   dtF16|dtX4|dta)
ENUM_CONST(dtF32X2,    dtF32e|dtX2)
ENUM_CONST(dtF32X4,    dtF32e|dtX4)
ENUM_CONST(dtF64X2,    dtF64|dtX2)
ENUM_CONST(dtF64X4,    dtF64|dtX4)
ENUM_CONST(dtS8,       dtS|dt8)
ENUM_CONST(dtS16,      dtS|dt16)
ENUM_CONST(dtS24,      dtS|dt24)
ENUM_CONST(dtS32,      dtS|dt32)
ENUM_CONST(dtS32e,     dtS|dt32e)
ENUM_CONST(dtS32_HI,   dtS|dt32|dtHi32)
ENUM_CONST(dtS64,      dtS|dt64)
ENUM_CONST(dtS32X2,    dtS32e|dtX2)
ENUM_CONST(dtS32X4,    dtS32e|dtX4)
ENUM_CONST(dtS64X2,    dtS64|dtX2)
ENUM_CONST(dtS64X4,    dtS64|dtX4)
ENUM_CONST(dtU8,       dtU|dt8)
ENUM_CONST(dtU16,      dtU|dt16)
ENUM_CONST(dtU24,      dtU|dt24)
ENUM_CONST(dtU32,      dtU|dt32)
ENUM_CONST(dtU32e,     dtU|dt32e)
ENUM_CONST(dtU32_HI,   dtU|dt32|dtHi32)
ENUM_CONST(dtU64,      dtU|dt64)
ENUM_CONST(dtU32X2,    dtU32e|dtX2)
ENUM_CONST(dtU32X4,    dtU32e|dtX4)
ENUM_CONST(dtU64X2,    dtU64|dtX2)
ENUM_CONST(dtU64X4,    dtU64|dtX4)
ENUM_END

// Rounding operations when colwerting from non-integral floating-point to integer
ENUM_BEGIN(opRoundModifier)
ENUM_CONST(rmNone,     0)  //!< use default (ROUND)
ENUM_CONST(rmCeil,     1)  //!< round to larger value
ENUM_CONST(rmFlr,      2)  //!< round to smaller value
ENUM_CONST(rmTrunc,    3)  //!< round to value nearest to zero
ENUM_CONST(rmRound,    4)  //!< if one integer is nearing in value to the original float-point
                           //!< value, it is chosen; otherwise the even integer is chosen.
ENUM_END

    // Special LOAD/STORE opModifiers
ENUM_BEGIN(opMemModifier)
ENUM_CONST(mmNone,     0)  //!< use default (none)
ENUM_CONST(mmCoh,      1)  //!< use coherent caching on LOAD/STORE
ENUM_CONST(mmVol,      2)  //!< treat memory as volatile on LOAD/STORE
ENUM_END

    // Special ATOM opModifiers
ENUM_BEGIN(opAtomicModifier)
ENUM_CONST(amNone,     0)  //!< default none
ENUM_CONST(amAdd,      1)  //!< perform ADD operation for ATOM
ENUM_CONST(amMin,      2)  //!< perform MIN operation for ATOM
ENUM_CONST(amMax,      3)  //!< perform MAX operation for ATOM
ENUM_CONST(amIwrap,    4)  //!< perform wrapping increment operation for ATOM
ENUM_CONST(amDwrap,    5)  //!< perform wrapping decrement operation for ATOM
ENUM_CONST(amAnd,      6)  //!< perform logical AND operation for ATOM
ENUM_CONST(amOr,       7)  //!< perform logical OR operation for ATOM
ENUM_CONST(amXor,      8)  //!< perform logical XOR operation for ATOM
ENUM_CONST(amExch,     9)  //!< perform exchange operation for ATOM
ENUM_CONST(amCSwap,   10)  //!< perform compare and swap operation for ATOM
ENUM_CONST(amNUM,     10)
ENUM_END

    // Special atomic modifier mask ids
    // These IDs are used to create an array of masks/values that get written
    // into the pgmElw[] for each shader.
    // refer to RndGpuPgms::PickAtomicModifierMasks()

ENUM_BEGIN(opAtomicModifierMask)
ENUM_CONST(ammAnd,    0)  // bits 7-0 are random all other "don't touch" bits=1
ENUM_CONST(ammOr,     1)  // bits 15-8 are random all other "don't touch" bits=0
ENUM_CONST(ammXor,    2)  // bits 23-16 are random all other "don't touch" bits=0
ENUM_CONST(ammMinMax, 3)  // used for .MIN & .MAX
ENUM_CONST(ammAdd,    4)  // used for .ADD
ENUM_CONST(ammWrap,   5)  // used for .IWRAP & .DWRAP
ENUM_CONST(ammExch,   6)  // just a random number.
ENUM_CONST(ammCSwap,  7)  // just another random number
ENUM_CONST(ammNUM,    8)
ENUM_END

    // Special ops that must be constrained
ENUM_BEGIN(opConstraint)
ENUM_CONST(ocNone,     amNone)   //!< default none
ENUM_CONST(ocAdd,      amAdd)    //!< perform ADD operation for ATOM
ENUM_CONST(ocMin,      amMin)    //!< perform MIN operation for ATOM
ENUM_CONST(ocMax,      amMax)    //!< perform MAX operation for ATOM
ENUM_CONST(ocIwrap,    amIwrap)  //!< perform wrapping increment operation for ATOM
ENUM_CONST(ocDwrap,    amDwrap)  //!< perform wrapping decrement operation for ATOM
ENUM_CONST(ocAnd,      amAnd)    //!< perform logical AND operation for ATOM
ENUM_CONST(ocOr,       amOr)     //!< perform logical OR operation for ATOM
ENUM_CONST(ocXor,      amXor)    //!< perform logical XOR operation for ATOM
ENUM_CONST(ocExch,     amExch)   //!< perform exchange operation for ATOM
ENUM_CONST(ocCSwap,    amCSwap)  //!< perform compare and swap operation for ATOM
ENUM_CONST(ocStore,    amCSwap+1)
ENUM_CONST(ocNUM,      ocStore+1)
ENUM_END

ENUM_BEGIN(pgmSource)
ENUM_CONST(psFixed,      0)   //!< program indices for non-random programs start at 0
ENUM_CONST(psRandom,  1000)   //!< program indices for random programs start here
ENUM_CONST(psUser,    2000)   //!< program indices for user-loaded programs start here
ENUM_CONST(psSpecial, 3000)   //!< program indices for special internal fixed programs
                              //!< not avail to the picking logic.
ENUM_CONST(pgmSource_NUM, 4)
ENUM_END

ENUM_BEGIN(opScalingMethod)//!< How to scale register values used in texture coordinates
ENUM_CONST(smLinear, 0)    //!< scale normalized values linearly to tex image size.
ENUM_CONST(smWrap, 1)      //!< wrap non-normalized values to tex image size.
ENUM_END

ENUM_BEGIN(lwTrace)
ENUM_CONST(trOption,   3)  // output to console window & file
ENUM_CONST(trLevelUndefined, -1)
ENUM_CONST(trLevelOff, 0)  // all tracing disabled
ENUM_CONST(trLevelOn,  50) // normal tracing level
ENUM_CONST(trDfltMask0,
                    0
                    //|  (1<<0)    // CLEAR
                    //| (1<<1)    // BEGIN
                    //| (1<<2)    // VERTEX
                    //| (1<<3)    // XFROM
                    //| (1<<4)    // TEXTURE
                    //| (1<<5)    // STATE
                    //| (1<<6)    // RASTER
                    //| (1<<7)    // LIGHT
                    //| (1<<8)    // DLIST
                    //| (1<<9)    // VTXARRAY
                    //| (1<<10)   // ICD_CMDS
                    //| (1<<11)   // EVAL
                    //| (1<<12)   // PIXEL
                    | (1<<13)   // ERRORS
                    //| (1<<14)   // PUSHBUFFER
                    | (1<<15)   // WGL (and DGL)
                    //| (1<<16)   // DRAWABLE
                    //| (<<17)    //USER_1 Reserved for hacking.  Don't check in
                    //| (1<<18)   // GLS
                    //| (1<<20)   // THREAD
                    | (1<<22)   // PROGRAM
                    //| (1<<25)   // FSAA tracking
                    //| (1<<26)   // any GET routine
                    | (1<<27)   // VP_CREATE
                    //| (1<<28)   // VALIDATE
                    //| (1<<29)   // TEXMGR
                    //| (1<<30)   // MUTEX
                    )
ENUM_CONST(trDfltMask1,
                     0
                    | (1<<(32-32)) // VP_INTERNAL
                    | (1<<(33-32)) // STATS
                    | (1<<(34-32)) // PM_STATS
                    | (1<<(35-32)) // FP_CREATE
                    | (1<<(36-32)) // COP
                    //| (1<<(37-32)) // FBO
                    //| (1<<(38-32)) // MULTIGPU
                    | (1<<(42-32)) // UTIL (zlwll alloc)
                    )

ENUM_CONST(trMask0cop,
                    0
                    | (1<<22)   // PROGRAM
                    | (1<<27)   // VP_CREATE
                    | (1<<28)   // VALIDATE
                    //| (1<<29)   // TEXMGR
                    )
ENUM_CONST(trMask1cop,
                    0
                    | (1<<(32-32)) // VP_INTERNAL
                    //| (1<<(33-32)) // STATS
                    //| (1<<(34-32)) // PM_STATS
                    | (1<<(35-32)) // FP_CREATE
                    | (1<<(36-32)) // COP
                    )
ENUM_END

#define RND_GPU_PROG_VX_INDEX_Random  psRandom
#define RND_GPU_PROG_VX_INDEX_User    psUser
#define RND_GPU_PROG_GM_INDEX_Random  psRandom
#define RND_GPU_PROG_GM_INDEX_User    psUser
#define RND_GPU_PROG_FR_INDEX_Random  psRandom
#define RND_GPU_PROG_FR_INDEX_User    psUser

ENUM_BEGIN(pgmKind)
ENUM_CONST(pkVertex,    0)
ENUM_CONST(pkTessCtrl,  1)
ENUM_CONST(pkTessEval,  2)
ENUM_CONST(pkGeometry,  3)
ENUM_CONST(pkFragment,  4)
ENUM_CONST(PgmKind_NUM, 5)
ENUM_END

ENUM_BEGIN(xfbMode)
ENUM_CONST(xfbModeDisable,             0)
ENUM_CONST(xfbModeCapture,             1)
ENUM_CONST(xfbModePlayback,            2)
ENUM_CONST(xfbModeCaptureAndPlayback,  3)
ENUM_END

ENUM_BEGIN(gcxMode)
ENUM_CONST(gcxModeDisabled,            0)
ENUM_CONST(gcxModeGC5,                 1)
ENUM_CONST(gcxModeGC6,                 2)
ENUM_CONST(gcxModeRTD3,                3)
ENUM_END

// Based on LowPowerPgBubble::PgFeature in pwrwait.h
ENUM_BEGIN(PgFeature)
ENUM_CONST(PgGrRpg,                    0)
ENUM_CONST(PgGpcRg,                    1)
ENUM_CONST(PgMsRpg,                    3)
ENUM_CONST(PgMsRppg,                   4)
ENUM_CONST(PgDisable,               0xFF)
ENUM_END

#if !defined(__cplusplus)
//
// Stuff copied from <gl/gl.h>
//
// It would be nice if we could just use that file directly...
//
#define GL_FLAT                           0x1D00
#define GL_SMOOTH                         0x1D01
#define GL_POINTS                         0
#define GL_LINES                          1
#define GL_LINE_LOOP                      2
#define GL_LINE_STRIP                     3
#define GL_TRIANGLES                      4
#define GL_TRIANGLE_STRIP                 5
#define GL_TRIANGLE_FAN                   6
#define GL_QUADS                          7
#define GL_QUAD_STRIP                     8
#define GL_POLYGON                        9
#define GL_LINES_ADJACENCY_EXT            10
#define GL_LINE_STRIP_ADJACENCY_EXT       11
#define GL_TRIANGLES_ADJACENCY_EXT        12
#define GL_TRIANGLE_STRIP_ADJACENCY_EXT   13
#define GL_PATCHES                        14
#define GL_BGR                            0x80E0
#define GL_BGRA                           0x80E1
#define GL_COLOR_INDEX                    0x1900
#define GL_STENCIL_INDEX                  0x1901
#define GL_DEPTH_COMPONENT                0x1902
#define GL_RED                            0x1903
#define GL_GREEN                          0x1904
#define GL_BLUE                           0x1905
#define GL_ALPHA                          0x1906
#define GL_RGB                            0x1907
#define GL_RGBA                           0x1908
#define GL_LUMINANCE                      0x1909
#define GL_LUMINANCE_ALPHA                0x190A
#define GL_BITMAP                         0x1A00
#define GL_BYTE                           0x1400
#define GL_UNSIGNED_BYTE                  0x1401
#define GL_SHORT                          0x1402
#define GL_UNSIGNED_SHORT                 0x1403
#define GL_INT                            0x1404
#define GL_UNSIGNED_INT                   0x1405
#define GL_FLOAT                          0x1406
#define GL_UNSIGNED_BYTE_3_3_2            0x8032
#define GL_UNSIGNED_BYTE_2_3_3_REV        0x8362
#define GL_UNSIGNED_SHORT_5_6_5           0x8363
#define GL_UNSIGNED_SHORT_5_6_5_REV       0x8364
#define GL_UNSIGNED_SHORT_4_4_4_4         0x8033
#define GL_UNSIGNED_SHORT_4_4_4_4_REV     0x8365
#define GL_UNSIGNED_SHORT_5_5_5_1         0x8034
#define GL_UNSIGNED_SHORT_1_5_5_5_REV     0x8366
#define GL_UNSIGNED_INT_8_8_8_8           0x8035
#define GL_UNSIGNED_INT_8_8_8_8_REV       0x8367
#define GL_UNSIGNED_INT_10_10_10_2        0x8036
#define GL_UNSIGNED_INT_2_10_10_10_REV    0x8368
#define GL_FALSE                          0
#define GL_TRUE                           1
#define GL_POINT                          0x1B00
#define GL_LINE                           0x1B01
#define GL_FILL                           0x1B02
#define GL_CW                             0x0900
#define GL_CCW                            0x0901
#define GL_FRONT                          0x0404
#define GL_BACK                           0x0405
#define GL_FRONT_AND_BACK                 0x0408
#define GL_SINGLE_COLOR                   0x81F9
#define GL_SEPARATE_SPELWLAR_COLOR        0x81FA
#define GL_EMISSION                       0x1600
#define GL_SHININESS                      0x1601
#define GL_AMBIENT_AND_DIFFUSE            0x1602
#define GL_COLOR_INDEXES                  0x1603
#define GL_DIFFUSE                        0x1201
#define GL_TEXTURE_1D                     0x0DE0
#define GL_TEXTURE_2D                     0x0DE1
#define GL_TEXTURE_3D                     0x806F
#define GL_TEXTURE_LWBE_MAP_EXT           0x8513
#define GL_TEXTURE_1D_ARRAY_EXT           0x8C18
#define GL_TEXTURE_2D_ARRAY_EXT           0x8C1A
#define GL_TEXTURE_LWBE_MAP_ARRAY_ARB     0x9009
#define GL_TEXTURE_RECTANGLE_LW           0x84F5
#define GL_EYE_RADIAL_LW                  0x855B
#define GL_EYE_PLANE_ABSOLUTE_LW          0x855C
#define GL_EYE_PLANE                      0x2502
#define GL_CLAMP                          0x2900
#define GL_CLAMP_TO_EDGE                  0x812F
#define GL_CLAMP_TO_BORDER_ARB            0x812D
#define GL_REPEAT                         0x2901
#define GL_MIRRORED_REPEAT_IBM            0x8370
#define GL_NEAREST                        0x2600
#define GL_LINEAR                         0x2601
#define GL_NEAREST_MIPMAP_NEAREST         0x2700
#define GL_LINEAR_MIPMAP_NEAREST          0x2701
#define GL_NEAREST_MIPMAP_LINEAR          0x2702
#define GL_LINEAR_MIPMAP_LINEAR           0x2703
#define GL_EYE_LINEAR                     0x2400
#define GL_OBJECT_LINEAR                  0x2401
#define GL_SPHERE_MAP                     0x2402
#define GL_NORMAL_MAP_EXT                 0x8511
#define GL_REFLECTION_MAP_EXT             0x8512
#define GL_EMBOSS_MAP_LW                  0x855F
#define GL_ALPHA                          0x1906
#define GL_RGB                            0x1907
#define GL_RGBA                           0x1908
#define GL_LUMINANCE                      0x1909
#define GL_LUMINANCE_ALPHA                0x190A
#define GL_INTENSITY                      0x8049
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT   0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT  0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT  0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT  0x83F3
#define GL_COLOR_INDEX8_EXT               0x80E5
#define GL_SIGNED_RGBA_LW                 0x86FB
#define GL_SIGNED_RGB_LW                  0x86FE
#define GL_SIGNED_LUMINANCE_LW            0x8701
#define GL_SIGNED_LUMINANCE_ALPHA_LW      0x8703
#define GL_SIGNED_ALPHA_LW                0x8705
#define GL_SIGNED_INTENSITY_LW            0x8707
#define GL_SIGNED_RGB_UNSIGNED_ALPHA_LW   0x870C
#define GL_HILO_LW                        0x86F4
#define GL_SIGNED_HILO_LW                 0x86F9
#define GL_DSDT_LW                        0x86F5
#define GL_DSDT_MAG_LW                    0x86F6
#define GL_DSDT_MAG_INTENSITY_LW          0x86DC
#define GL_LINEAR                         0x2601
#define GL_EXP                            0x0800
#define GL_EXP2                           0x0801
#define GL_FOG_COORDINATE_EXT             0x8451
#define GL_FRAGMENT_DEPTH_EXT             0x8452
#define GL_NEVER                          0x0200
#define GL_LESS                           0x0201
#define GL_EQUAL                          0x0202
#define GL_LEQUAL                         0x0203
#define GL_GREATER                        0x0204
#define GL_NOTEQUAL                       0x0205
#define GL_GEQUAL                         0x0206
#define GL_ALWAYS                         0x0207
#define GL_ZERO                           0
#define GL_ONE                            1
#define GL_SRC_COLOR                      0x0300
#define GL_ONE_MINUS_SRC_COLOR            0x0301
#define GL_SRC_ALPHA                      0x0302
#define GL_ONE_MINUS_SRC_ALPHA            0x0303
#define GL_DST_ALPHA                      0x0304
#define GL_ONE_MINUS_DST_ALPHA            0x0305
#define GL_DST_COLOR                      0x0306
#define GL_ONE_MINUS_DST_COLOR            0x0307
#define GL_SRC_ALPHA_SATURATE             0x0308
#define GL_CONSTANT_COLOR_EXT             0x8001
#define GL_ONE_MINUS_CONSTANT_COLOR_EXT   0x8002
#define GL_CONSTANT_ALPHA_EXT             0x8003
#define GL_ONE_MINUS_CONSTANT_ALPHA_EXT   0x8004
#define GL_FUNC_ADD_EXT                   0x8006
#define GL_MIN_EXT                        0x8007
#define GL_MAX_EXT                        0x8008
#define GL_BLEND_EQUATION_EXT             0x8009
#define GL_FUNC_SUBTRACT_EXT              0x800A
#define GL_FUNC_REVERSE_SUBTRACT_EXT      0x800B
#define GL_CLEAR                          0x1500
#define GL_AND                            0x1501
#define GL_AND_REVERSE                    0x1502
#define GL_COPY                           0x1503
#define GL_AND_ILWERTED                   0x1504
#define GL_NOOP                           0x1505
#define GL_XOR                            0x1506
#define GL_OR                             0x1507
#define GL_NOR                            0x1508
#define GL_EQUIV                          0x1509
#define GL_ILWERT                         0x150A
#define GL_OR_REVERSE                     0x150B
#define GL_COPY_ILWERTED                  0x150C
#define GL_OR_ILWERTED                    0x150D
#define GL_NAND                           0x150E
#define GL_SET                            0x150F
#define GL_ZERO                           0
#define GL_KEEP                           0x1E00
#define GL_REPLACE                        0x1E01
#define GL_INCR                           0x1E02
#define GL_DECR                           0x1E03
#define GL_ILWERT                         0x150A
#define GL_INCR_WRAP_EXT                  0x8507
#define GL_DECR_WRAP_EXT                  0x8508
#define GL_PRIMARY_COLOR_LW               0x852C
#define GL_SECONDARY_COLOR_LW             0x852D
#define GL_TEXTURE0_ARB                   0x84C0
#define GL_TEXTURE1_ARB                   0x84C1
#define GL_TEXTURE2_ARB                   0x84C2
#define GL_TEXTURE3_ARB                   0x84C3
#define GL_TEXTURE4_ARB                   0x84C4
#define GL_SPARE0_LW                      0x852E
#define GL_SPARE1_LW                      0x852F
#define GL_FOG                            0x0B60
#define GL_CONSTANT_COLOR0_LW             0x852A
#define GL_CONSTANT_COLOR1_LW             0x852B
#define GL_DISCARD_LW                     0x8530
#define GL_HILO8_LW                       0x885E
#define GL_SIGNED_HILO8_LW                0x885F
#define GL_FORCE_BLUE_TO_ONE_LW           0x8860
#define GL_EXPAND_NORMAL_LW               0x8538
#define GL_UNSIGNED_IDENTITY_LW           0x8536
#define GL_RGBA32F_ARB                    0x8814
#define GL_RGB32F_ARB                     0x8815
#define GL_ALPHA32F_ARB                   0x8816
#define GL_INTENSITY32F_ARB               0x8817
#define GL_LUMINANCE32F_ARB               0x8818
#define GL_LUMINANCE_ALPHA32F_ARB         0x8819
#define GL_RGBA16F_ARB                    0x881A
#define GL_RGB16F_ARB                     0x881B
#define GL_ALPHA16F_ARB                   0x881C
#define GL_INTENSITY16F_ARB               0x881D
#define GL_LUMINANCE16F_ARB               0x881E
#define GL_LUMINANCE_ALPHA16F_ARB         0x881F
#define GL_RGB5                           0x8050
#define GL_RGB8                           0x8051
#define GL_RGBA4                          0x8056
#define GL_RGB5_A1                        0x8057
#define GL_RGBA8                          0x8058
#define GL_ALPHA8                         0x803C
#define GL_ALPHA16                        0x803E
#define GL_LUMINANCE8                     0x8040
#define GL_LUMINANCE16                    0x8042
#define GL_LUMINANCE8_ALPHA8              0x8045
#define GL_LUMINANCE16_ALPHA16            0x8048
#define GL_INTENSITY8                     0x804B
#define GL_INTENSITY16                    0x804D
#define GL_HILO16_LW                      0x86F8
#define GL_SIGNED_HILO16_LW               0x86FA
#define GL_SIGNED_RGBA8_LW                0x86FC
#define GL_SIGNED_RGB8_LW                 0x86FF
#define GL_SIGNED_LUMINANCE8_LW           0x8702
#define GL_SIGNED_LUMINANCE8_ALPHA8_LW    0x8704
#define GL_SIGNED_ALPHA8_LW               0x8706
#define GL_SIGNED_INTENSITY8_LW           0x8708
#define GL_DSDT8_LW                       0x8709
#define GL_DSDT8_MAG8_LW                  0x870A
#define GL_DSDT8_MAG8_INTENSITY8_LW       0x870B
#define GL_SIGNED_RGB8_UNSIGNED_ALPHA8_LW 0x870D
#define GL_DEPTH_COMPONENT16_SGIX         0x81A5
#define GL_DEPTH_COMPONENT24_SGIX         0x81A6
#define GL_DEPTH_COMPONENT16              0x81A5
#define GL_DEPTH_COMPONENT24              0x81A6
#define GL_HALF_FLOAT_LW                  0x140B
#define GL_NONE                           0
#define GL_RGB10_A2                       0x8059
#define GL_STENCIL_INDEX1                                   0x8D46
#define GL_STENCIL_INDEX4                                   0x8D47
#define GL_STENCIL_INDEX8                                   0x8D48
#define GL_STENCIL_INDEX16                                  0x8D49

#define GL_RGB9_E5_EXT                                      0x8C3D
#define GL_R3_G3_B2                                         0x2A10
#define GL_RGB4                                             0x804F
#define GL_RGB5                                             0x8050
#define GL_RGB8                                             0x8051
#define GL_RGB10                                            0x8052
#define GL_RGB12                                            0x8053
#define GL_RGB16                                            0x8054
#define GL_RGBA2                                            0x8055
#define GL_RGBA4                                            0x8056
#define GL_RGB5_A1                                          0x8057
#define GL_RGBA8                                            0x8058
#define GL_RGB10_A2                                         0x8059
#define GL_RGBA12                                           0x805A
#define GL_RGBA16                                           0x805B

#define GL_RGBA32UI_EXT                                     0x8D70
#define GL_RGB32UI_EXT                                      0x8D71
#define GL_RGBA16UI_EXT                                     0x8D76
#define GL_RGB16UI_EXT                                      0x8D77
#define GL_RGBA8UI_EXT                                      0x8D7C
#define GL_RGB8UI_EXT                                       0x8D7D
#define GL_RGBA32I_EXT                                      0x8D82
#define GL_RGB32I_EXT                                       0x8D83
#define GL_RGBA16I_EXT                                      0x8D88
#define GL_RGB16I_EXT                                       0x8D89
#define GL_RGBA8I_EXT                                       0x8D8E
#define GL_RGB8I_EXT                                        0x8D8F
#define GL_RGB_INTEGER_EXT                                  0x8D98
#define GL_RGBA_INTEGER_EXT                                 0x8D99
#define GL_RGBA_INTEGER_MODE_EXT                            0x8D9E
#define GL_RGBA_FLOAT32_ATI                                 0x8814
#define GL_RGB_FLOAT32_ATI                                  0x8815
#define GL_RGBA_FLOAT16_ATI                                 0x881A
#define GL_RGB_FLOAT16_ATI                                  0x881B

/* KHR_texture_compression_astc_ldr */
#define GL_COMPRESSED_RGBA_ASTC_4x4_KHR                     0x93B0
#define GL_COMPRESSED_RGBA_ASTC_5x4_KHR                     0x93B1
#define GL_COMPRESSED_RGBA_ASTC_5x5_KHR                     0x93B2
#define GL_COMPRESSED_RGBA_ASTC_6x5_KHR                     0x93B3
#define GL_COMPRESSED_RGBA_ASTC_6x6_KHR                     0x93B4
#define GL_COMPRESSED_RGBA_ASTC_8x5_KHR                     0x93B5
#define GL_COMPRESSED_RGBA_ASTC_8x6_KHR                     0x93B6
#define GL_COMPRESSED_RGBA_ASTC_8x8_KHR                     0x93B7
#define GL_COMPRESSED_RGBA_ASTC_10x5_KHR                    0x93B8
#define GL_COMPRESSED_RGBA_ASTC_10x6_KHR                    0x93B9
#define GL_COMPRESSED_RGBA_ASTC_10x8_KHR                    0x93BA
#define GL_COMPRESSED_RGBA_ASTC_10x10_KHR                   0x93BB
#define GL_COMPRESSED_RGBA_ASTC_12x10_KHR                   0x93BC
#define GL_COMPRESSED_RGBA_ASTC_12x12_KHR                   0x93BD
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR             0x93D0
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR             0x93D1
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR             0x93D2
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR             0x93D3
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR             0x93D4
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR             0x93D5
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR             0x93D6
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR             0x93D7
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR            0x93D8
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR            0x93D9
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR            0x93DA
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR           0x93DB
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR           0x93DC
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR           0x93DD

#define GL_TEXTURE_RENDERBUFFER_LW                          0x8E55
// Defines needed to support WAR for bug482333
#define GL_FLOAT_R_LW                                       0x8880
#define GL_FLOAT_RG_LW                                      0x8881
#define GL_FLOAT_RGB_LW                                     0x8882
#define GL_FLOAT_RGBA_LW                                    0x8883
#define GL_FLOAT_R16_LW                                     0x8884
#define GL_FLOAT_R32_LW                                     0x8885
#define GL_FLOAT_RG16_LW                                    0x8886
#define GL_FLOAT_RG32_LW                                    0x8887
#define GL_FLOAT_RGB16_LW                                   0x8888
#define GL_FLOAT_RGB32_LW                                   0x8889
#define GL_FLOAT_RGBA16_LW                                  0x888A
#define GL_FLOAT_RGBA32_LW                                  0x888B
#define GL_TEXTURE_FLOAT_COMPONENTS_LW                      0x888C
#define GL_FLOAT_CLEAR_COLOR_VALUE_LW                       0x888D
#define GL_FLOAT_RGBA_MODE_LW                               0x888E

#define GL_RGBA_FLOAT32_ATI                                 0x8814
#define GL_RGB_FLOAT32_ATI                                  0x8815
#define GL_ALPHA_FLOAT32_ATI                                0x8816
#define GL_INTENSITY_FLOAT32_ATI                            0x8817
#define GL_LUMINANCE_FLOAT32_ATI                            0x8818
#define GL_LUMINANCE_ALPHA_FLOAT32_ATI                      0x8819
#define GL_RGBA_FLOAT16_ATI                                 0x881A
#define GL_RGB_FLOAT16_ATI                                  0x881B
#define GL_ALPHA_FLOAT16_ATI                                0x881C
#define GL_INTENSITY_FLOAT16_ATI                            0x881D
#define GL_LUMINANCE_FLOAT16_ATI                            0x881E
#define GL_LUMINANCE_ALPHA_FLOAT16_ATI                      0x881F

#define GL_COMPRESSED_LUMINANCE_LATC1_EXT                   0x8C70
#define GL_COMPRESSED_SIGNED_LUMINANCE_LATC1_EXT            0x8C71
#define GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT             0x8C72
#define GL_COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2_EXT      0x8C73

#define GL_R11F_G11F_B10F_EXT                               0x8C3A
#define GL_UNSIGNED_INT_10F_11F_11F_REV_EXT                 0x8C3B
#define GL_RGBA_SIGNED_COMPONENTS_EXT                       0x8C3C

#define GL_R16_SNORM                                        0x8F98
#define GL_RG16_SNORM                                       0x8F99
#define GL_RGB16_SNORM                                      0x8F9A
#define GL_RGBA16_SNORM                                     0x8F9B

#define GL_COMPARE_R_TO_TEXTURE                             0x884E
#define GL_LUMINANCE_ALPHA8I_EXT                            0x8D93
#define GL_LUMINANCE8I_EXT                                  0x8D92
#define GL_LUMINANCE_ALPHA16I_EXT                           0x8D8D
#define GL_LUMINANCE16I_EXT                                 0x8D8C
#define GL_LUMINANCE_ALPHA32I_EXT                           0x8D87
#define GL_LUMINANCE32I_EXT                                 0x8D86
#define GL_LUMINANCE_ALPHA8UI_EXT                           0x8D81
#define GL_LUMINANCE8UI_EXT                                 0x8D80
#define GL_LUMINANCE_ALPHA16UI_EXT                          0x8D7B
#define GL_LUMINANCE16UI_EXT                                0x8D7A
#define GL_LUMINANCE_ALPHA32UI_EXT                          0x8D75
#define GL_LUMINANCE32UI_EXT                                0x8D74

#define GL_COMPRESSED_R11_EAC                               0x9270
#define GL_COMPRESSED_SIGNED_R11_EAC                        0x9271
#define GL_COMPRESSED_RG11_EAC                              0x9272
#define GL_COMPRESSED_SIGNED_RG11_EAC                       0x9273
#define GL_COMPRESSED_RGB8_ETC2                             0x9274
#define GL_COMPRESSED_SRGB8_ETC2                            0x9275
#define GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2         0x9276
#define GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2        0x9277
#define GL_COMPRESSED_RGBA8_ETC2_EAC                        0x9278
#define GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC                 0x9279

#define GL_VIEWPORT_SWIZZLE_POSITIVE_X_LW                   0x9350
#define GL_VIEWPORT_SWIZZLE_NEGATIVE_X_LW                   0x9351
#define GL_VIEWPORT_SWIZZLE_POSITIVE_Y_LW                   0x9352
#define GL_VIEWPORT_SWIZZLE_NEGATIVE_Y_LW                   0x9353
#define GL_VIEWPORT_SWIZZLE_POSITIVE_Z_LW                   0x9354
#define GL_VIEWPORT_SWIZZLE_NEGATIVE_Z_LW                   0x9355
#define GL_VIEWPORT_SWIZZLE_POSITIVE_W_LW                   0x9356

/* LW_conservative_raster_pre_snap_triangles */
#define GL_CONSERVATIVE_RASTER_MODE_LW                      0x954D
#define GL_CONSERVATIVE_RASTER_MODE_POST_SNAP_LW            0x954E
#define GL_CONSERVATIVE_RASTER_MODE_PRE_SNAP_TRIANGLES_LW   0x954F

/* LW_conservative_raster_pre_snap */
#define GL_CONSERVATIVE_RASTER_MODE_PRE_SNAP_LW             0x9550

#endif

