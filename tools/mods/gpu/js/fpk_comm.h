/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file fpk_comm.h
 * @brief Common C++ and JavaScript declarations for tests that use FancyPicker.
 *
 * Several MODS tests use FancyPickers to control random numbers.
 *
 * This file dolwments the syntax for overriding FancyPicker probability
 * tables at runtime.  This can be helpful in making more directed tests
 * out of random tests when debugging a hardware problem.
 *
 * Also, the indexes for the various FancyPickers used by most tests
 * are #defined here.
 *
 * The GLRandom 3d-engine test uses so many FancyPickers that it gets its
 * own javascript include file called glr_comm.h.
 */

/**
 * Examples:
 *
 * Do only small blits in the blit test:
 *
 *    Class05f.Rnd[FPK_05F_BLIT_SHAPE] = [FPK_05F_WIDTH_HEIGHT_Small_Small];
 *
 * Make the random fill for the source surface use 95% white and 5% black pixels
 * in the blit test:
 *
 *    Class05f.Rnd[FPK_05F_SOURCE_FILL_COLORS] = ["rand", [19, 0xffffffff], [1, 0]];
 *
 * Wait for a notification after every 10th loop in the 2d triangle test:
 *
 *    Class05d.Rnd[FPK_05D_NOTIFY] = [ "list", [9, 0], [1, 1], "wrap" ];
 */

/**
 * Syntax for overriding FancyPickers from JavaScript:
 *
 *    [ MODE, OPTIONS, ITEMS ]
 *
 * MODE must be a quoted string, from this list:
 *
 *    "const" "constant"         (always returns same value)
 *    "rand"  "random"           (return a random value)
 *    "shuffle"                  (return items from list in random order)
 *    "list"                     (return next item in a list)
 *    "step"                     (schmoo mode, i.e. begin/step/end )
 *    "js"                       (call a JavaScript function, flexible but slow)
 *
 * OPTIONS must be zero or more quoted strings, from this list:
 *
 *    "PickOnRestart"            (pick only on Restart loops, re-use that value
 *                                for all other loops.  DEFAULT is to pick a
 *                                new value on each loop.)
 *    "clamp" "wrap"             (for LIST or STEP mode, what to do at the end
 *                                of the sequence: stay at end, or go back to
 *                                the beginning.  LIST and STEP mode reset to
 *                                the beginning on Restart loops unless they
 *                                have the PickOnRestart option enabled.
 *                                DEFAULT is wrap.)
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
 *                   WARNING: "int" "step" mode defaults to *signed* (SI32).
 *                   If (1.0 > (end-begin) / step), it will assume *unsigned*.
 *                   This is *probably* the behavior you intended...
 *
 *    JS ITEMS       must be one or two javascript function names:
 *                      pick_func  or  pick_func, restart_func
 */

//
// Random2d
//

/** <a name="Random2d Modules"> </a> */
/*
 * Codes for the different randomizer modules within Random2d:
 */
#define RND2D_MISC             0 //!< (context) miscellanious stuff
#define RND2D_TWOD             1 //!< (draw) LW50_TWOD_PRIMITIVE
#define RND2D_NUM_RANDOMIZERS  2

#define RND2D_NO_RANDOMIZER   -1
#define RND2D_FIRST_DRAW_CLASS RND2D_TWOD

/*
 * We keep a separate array of Utility::FancyPicker objects in each
 * randomizer module.
 *
 * In C++, strip away the base offset, i.e. (index % RND_BASE_OFFSET).
 *
 * We set default values from C++, but the JavaScript settings override them.
 *
 * WARNING: keep the RND2D_*_NUM_PICKERS defines up-to-date, or you will
 *          corrupt memory at runtime (asserts in the debug build).
 */

#define RND2D_BASE_OFFSET    100
#if defined(__cplusplus)
// C++
   #define TWODIDX(i)  ((i) % RND2D_BASE_OFFSET)
#else
// JavaScript
   #define TWODIDX(i)  (i)
#endif

#define RND2D_MISC_SWAP_SRC_DST         TWODIDX(  0) //!< (pick once per frame) do/don't swap src and dest (to allow rendering to either tiled or untiled)
#define RND2D_MISC_STOP                 TWODIDX(  1) //!< (PickFirst) stop testing, return current pass/fail code
#define RND2D_MISC_DRAW_CLASS           TWODIDX(  2) //!< (PickFirst) which draw object (blit/line/etc) to use this loop
#define RND2D_MISC_NOTIFY               TWODIDX(  3) //!< (PickFirst) do/don't set and wait for a notifier at the end of the draw
#define RND2D_MISC_FORCE_BIG_SIMPLE     TWODIDX(  4) //!< (PickFirst) do/don't force non-overlapping draws (for debugging).
#define RND2D_MISC_SUPERVERBOSE         TWODIDX(  5) //!< (PickLast) do/don't print picks just before sending them
#define RND2D_MISC_SKIP                 TWODIDX(  6) //!< (PickLast) do/don't draw (if returns nonzero/true, nothing is sent to HW this loop)
#define RND2D_MISC_DST_FILL_COLOR       TWODIDX(  7) //!< (Restart) random table for the once-per-restart single-color fill of the destination surface
#define RND2D_MISC_SLEEPMS              TWODIDX(  8) //!< (PickFirst) milliseconds to sleep after each channel flush.
#define RND2D_MISC_FLUSH                TWODIDX(  9) //!< (PickLast) do/don't flush even if not notifying this loop.
#define RND2D_MISC_EXTRA_NOOPS          TWODIDX( 10) //!< (PickLast) number of NOOP commands to add after each loop.
#define RND2D_MISC_NUM_PICKERS                11

#define RND2D_TWOD_PRIM                TWODIDX(100) //!< Which 2d primitive to draw.
#define RND2D_TWOD_PRIM_Points            0     //!< draw points
#define RND2D_TWOD_PRIM_Lines             1     //!< draw lines
#define RND2D_TWOD_PRIM_PolyLine          2     //!< draw a multisegment line
#define RND2D_TWOD_PRIM_Triangles         3     //!< draw 2d triangles
#define RND2D_TWOD_PRIM_Rects             4     //!< draw a rectangle-strip
#define RND2D_TWOD_PRIM_Imgcpu            5     //!< draw pixels, color or index data from pushbuffer
#define RND2D_TWOD_PRIM_Imgmem            6     //!< draw pixels, color data from memory (like scimg)
#define RND2D_TWOD_SURF_FORMAT         TWODIDX(101) //!< LW502D_SET_DST_FORMAT and LW502D_SET_SRC_FORMAT
#define RND2D_TWOD_SURF_OFFSET         TWODIDX(102) //!< Surface2d-relative byte offset for src and dst surfaces
#define RND2D_TWOD_SURF_PITCH          TWODIDX(103) //!< byte-pitch (2 picks per loop: dst, src), 0 == modeset pitch.
#define RND2D_TWOD_PCPU_INDEX_WRAP     TWODIDX(104) //!< LW502D_SET_PIXELS_FROM_CPU_INDEX_WRAP
#define RND2D_TWOD_NUM_TPCS            TWODIDX(105) //!< LW502D_SET_NUM_TPCS
#define RND2D_TWOD_CLIP_ENABLE         TWODIDX(106)
#define RND2D_TWOD_CLIP_LEFT           TWODIDX(107)
#define RND2D_TWOD_CLIP_TOP            TWODIDX(108)
#define RND2D_TWOD_CLIP_RIGHT          TWODIDX(109)
#define RND2D_TWOD_CLIP_BOTTOM         TWODIDX(110)
#define RND2D_TWOD_SOURCE              TWODIDX(111) //!< for ops that have a source, use src->dst vs. dst->dst
#define RND2D_TWOD_SOURCE_SrcToDst 0
#define RND2D_TWOD_SOURCE_DstToDst 1
#define RND2D_TWOD_SOURCE_SysToDst 2
#define RND2D_TWOD_RENDER_ENABLE_MODE  TWODIDX(112) //!< LW_502D_SET_RENDER_ENABLE_C_MODE
#define RND2D_TWOD_BIG_ENDIAN_CTRL     TWODIDX(113) //!< LW_502D_SET_BIG_ENDIAN_CONTROL
#define RND2D_TWOD_CKEY_ENABLE         TWODIDX(114) //!< LW502D_SET_COLOR_KEY_ENABLE
#define RND2D_TWOD_CKEY_FORMAT         TWODIDX(115) //!< LW502D_SET_COLOR_KEY_FORMAT
#define RND2D_TWOD_CKEY_COLOR          TWODIDX(116) //!< LW502D_SET_COLOR_KEY
#define RND2D_TWOD_ROP                 TWODIDX(117) //!< LW502D_SET_ROP
#define RND2D_TWOD_BETA1               TWODIDX(118) //!< LW502D_SET_BETA1
#define RND2D_TWOD_BETA4               TWODIDX(119) //!< LW502D_SET_BETA4
#define RND2D_TWOD_OP                  TWODIDX(120) //!< LW502D_SET_OPERATION
#define RND2D_TWOD_PRIM_COLOR_FORMAT   TWODIDX(121) //!< LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT
#define RND2D_TWOD_LINE_TIE_BREAK_BITS TWODIDX(122) //!< LW502D_SET_RENDER_SOLID_LINE_TIE_BREAK_BITS
#define RND2D_TWOD_PCPU_DATA_TYPE      TWODIDX(123) //!< LW502D_SET_PIXELS_FROM_CPU_DATA_TYPE
#define RND2D_TWOD_PATT_OFFSET         TWODIDX(124) //!< LW502D_SET_PATTERN_OFFSET
#define RND2D_TWOD_PATT_SELECT         TWODIDX(125) //!< LW502D_SET_PATTERN_SELECT
#define RND2D_TWOD_PATT_MONO_COLOR_FORMAT   TWODIDX(126) //!< LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT
#define RND2D_TWOD_PATT_MONO_FORMAT    TWODIDX(127) //!< LW502D_SET_MONOCHROME_PATTERN_FORMAT
#define RND2D_TWOD_PATT_COLOR          TWODIDX(128) //!< LW502D_SET_MONOCHROME_PATTERN_COLOR0,1
#define RND2D_TWOD_PATT_MONO_PATT      TWODIDX(129) //!< LW502D_SET_MONOCHROME_PATTERN0,1
#define RND2D_TWOD_SENDXY_16           TWODIDX(130) //!< if true, use _PRIM_POINT_X_Y, else _PRIM_POINT_Y.
#define RND2D_TWOD_NUM_VERTICES        TWODIDX(131) //!< number of solid-prim vertices to send
#define RND2D_TWOD_PRIM_WIDTH          TWODIDX(132) //!< width of a solid primitive in pixels
#define RND2D_TWOD_PRIM_HEIGHT         TWODIDX(133) //!< height of a solid primitive, in pixels
#define RND2D_TWOD_PCPU_CFMT           TWODIDX(134) //!< LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT
#define RND2D_TWOD_PCPU_IFMT           TWODIDX(135) //!< LW502D_SET_PIXELS_FROM_CPU_INDEX_FORMAT
#define RND2D_TWOD_PCPU_MFMT           TWODIDX(136) //!< LW502D_SET_PIXELS_FROM_CPU_MONO_FORMAT
#define RND2D_TWOD_PCPU_COLOR          TWODIDX(137) //!< LW502D_SET_PIXELS_FROM_CPU_COLOR0,1 and DATA
#define RND2D_TWOD_PCPU_MONO_OPAQUE    TWODIDX(138) //!< LW502D_SET_PIXELS_FROM_CPU_MONO_OPACITY
#define RND2D_TWOD_PCPU_SRC_W          TWODIDX(139) //!< LW502D_SET_PIXELS_FROM_CPU_SRC_WIDTH
#define RND2D_TWOD_PCPU_SRC_H          TWODIDX(140) //!< LW502D_SET_PIXELS_FROM_CPU_SRC_HEIGHT
#define RND2D_TWOD_PCPU_SRC_DXDU_INT   TWODIDX(141) //!< LW502D_SET_PIXELS_FROM_CPU_DX_DU_INT
#define RND2D_TWOD_PCPU_SRC_DXDU_FRAC  TWODIDX(142) //!< LW502D_SET_PIXELS_FROM_CPU_DX_DU_FRAC
#define RND2D_TWOD_PCPU_SRC_DYDV_INT   TWODIDX(143) //!< LW502D_SET_PIXELS_FROM_CPU_DY_DV_INT
#define RND2D_TWOD_PCPU_SRC_DYDV_FRAC  TWODIDX(144) //!< LW502D_SET_PIXELS_FROM_CPU_DY_DV_FRAC
#define RND2D_TWOD_PCPU_DST_X_INT      TWODIDX(145) //!< LW502D_SET_PIXELS_FROM_CPU_DST_Y0_INT
#define RND2D_TWOD_PCPU_DST_X_FRAC     TWODIDX(146) //!< LW502D_SET_PIXELS_FROM_CPU_DST_Y0_FRAC
#define RND2D_TWOD_PCPU_DST_Y_INT      TWODIDX(147) //!< LW502D_SET_PIXELS_FROM_CPU_DST_Y0_INT
#define RND2D_TWOD_PCPU_DST_Y_FRAC     TWODIDX(148) //!< LW502D_SET_PIXELS_FROM_CPU_DST_Y0_FRAC
#define RND2D_TWOD_PMEM_BLOCK_SHAPE    TWODIDX(149) //!< LW502D_SET_PIXELS_FROM_MEMORY_BLOCK_SHAPE
#define RND2D_TWOD_PMEM_SAFE_OVERLAP   TWODIDX(150) //!< LW502D_SET_PIXELS_FROM_MEMORY_SAFE_OVERLAP
#define RND2D_TWOD_PMEM_ORIGIN         TWODIDX(151) //!< LW502D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE_ORIGIN
#define RND2D_TWOD_PMEM_FILTER         TWODIDX(152) //!< LW502D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE_FILTER
#define RND2D_TWOD_PMEM_DST_X0         TWODIDX(153) //!< LW502D_SET_PIXELS_FROM_MEMORY_DST_X0
#define RND2D_TWOD_PMEM_DST_Y0         TWODIDX(154) //!< LW502D_SET_PIXELS_FROM_MEMORY_DST_Y0
#define RND2D_TWOD_PMEM_DST_W          TWODIDX(155) //!< LW502D_SET_PIXELS_FROM_MEMORY_DST_WIDTH
#define RND2D_TWOD_PMEM_DST_H          TWODIDX(156) //!< LW502D_SET_PIXELS_FROM_MEMORY_DST_HEIGHT
#define RND2D_TWOD_PMEM_DUDX_FRAC      TWODIDX(157) //!< LW502D_SET_PIXELS_FROM_MEMORY_DU_DX_FRAC
#define RND2D_TWOD_PMEM_DUDX_INT       TWODIDX(158) //!< LW502D_SET_PIXELS_FROM_MEMORY_DU_DX_INT
#define RND2D_TWOD_PMEM_DVDY_FRAC      TWODIDX(159) //!< LW502D_SET_PIXELS_FROM_MEMORY_DV_DY_FRAC
#define RND2D_TWOD_PMEM_DVDY_INT       TWODIDX(160) //!< LW502D_SET_PIXELS_FROM_MEMORY_DV_DY_INT
#define RND2D_TWOD_PMEM_SRC_X0_FRAC    TWODIDX(161) //!< LW502D_SET_PIXELS_FROM_MEMORY_SRC_X0_FRAC
#define RND2D_TWOD_PMEM_SRC_X0_INT     TWODIDX(162) //!< LW502D_SET_PIXELS_FROM_MEMORY_SRC_X0_INT
#define RND2D_TWOD_PMEM_SRC_Y0_FRAC    TWODIDX(163) //!< LW502D_SET_PIXELS_FROM_MEMORY_SRC_Y0_FRAC
#define RND2D_TWOD_PMEM_SRC_Y0_INT     TWODIDX(164) //!< LW502D_PIXELS_FROM_MEMORY_SRC_X0_INT
#define RND2D_TWOD_PATT_LOAD_METHOD    TWODIDX(165) //!< which pattern download method to use
#define RND2D_TWOD_FERMI_SURF_FORMAT   TWODIDX(166) //!< LW902D_SET_DST_FORMAT and LW902D_SET_SRC_FORMAT for Fermi
#define RND2D_TWOD_FERMI_PRIM_COLOR_FORMAT      TWODIDX(167) //!< LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT
#define RND2D_TWOD_FERMI_PATT_MONO_COLOR_FORMAT TWODIDX(168) //!< LW902D_SET_MONOCHROME_PATTERN_COLOR_FORMAT
#define RND2D_TWOD_PMEM_DIRECTION      TWODIDX(169) //!< LW902D_SET_PIXELS_FROM_MEMORY_DIRECTION_HORIZONTAL and LW902D_SET_PIXELS_FROM_MEMORY_DIRECTION_VERTICAL
#define RND2D_TWOD_NUM_PICKERS               70

//-----------------------------------------------------------------------------
//@{
//! Copy Engine Test FancyPicker Indices
#define FPK_COPYENG_SRC_SURF     0
#define FPK_COPYENG_DST_SURF     1
#define FPK_COPYENG_FORMAT_IN    2
#define FPK_COPYENG_FORMAT_OUT   3
#define FPK_COPYENG_OFFSET_IN    4
#define FPK_COPYENG_OFFSET_OUT   5
#define FPK_COPYENG_PITCH_IN     6
#define FPK_COPYENG_PITCH_OUT    7
#define FPK_COPYENG_LINE_BYTES   8
#define FPK_COPYENG_LINE_COUNT   9
#define FPK_COPYENG_SRC_BLOCK_SIZE_WIDTH 10
#define FPK_COPYENG_DST_BLOCK_SIZE_WIDTH 11
#define FPK_COPYENG_SRC_BLOCK_SIZE_HEIGHT 12
#define FPK_COPYENG_DST_BLOCK_SIZE_HEIGHT 13
#define FPK_COPYENG_SRC_BLOCK_SIZE_DEPTH 14
#define FPK_COPYENG_DST_BLOCK_SIZE_DEPTH 15
#define FPK_COPYENG_SRC_ORIGIN_X 16
#define FPK_COPYENG_SRC_ORIGIN_Y 17
#define FPK_COPYENG_DST_ORIGIN_X 18
#define FPK_COPYENG_DST_ORIGIN_Y 19
#define FPK_COPYENG_SRC_WIDTH    20
#define FPK_COPYENG_DST_WIDTH    21
#define FPK_COPYENG_SRC_HEIGHT   22
#define FPK_COPYENG_DST_HEIGHT   23
#define FPK_COPYENG_GOB_HEIGHT   24
#define FPK_COPYENG_NUM_PICKERS  25
//@}

//------------------------------------------------------------------------------
//@{
//! GpuRamTest (utility) FancyPicker Indexes
#define FPK_GPURAM_READ_PRIORITY   0
#define FPK_GPURAM_WRITE_PRIORITY  1
//@}

#define CHECKDMA_MAX_GPUS 32 // LW0000_CTRL_GPU_MAX_ATTACHED_GPUS

//------------------------------------------------------------------------------
//@{
//! CheckDma FancyPicker Indexes
#define FPK_CHECKDMA_SKIP                  0       //!< If !0, don't send/check this copy
#define FPK_CHECKDMA_SRC                   1       //!< Source surface
#define FPK_CHECKDMA_DST                   2       //!< Destination surface
#define FPK_CHECKDMA_SURF_HostC        0           //!<  src/dest Coherent (cached) host memory
#define FPK_CHECKDMA_SURF_HostNC       1           //!<  src/dest NonCoherent (write-combined, unsnooped) host memory
#define FPK_CHECKDMA_SURF_FbGpu0       2           //!<  src/dest frame buffer of GPU 0
#define FPK_CHECKDMA_SURF_FbGpu1       3           //!<  src/dest frame buffer of GPU 1 (if present)
#define FPK_CHECKDMA_SURF_NUM_SURFS    (FPK_CHECKDMA_SURF_FbGpu0 + CHECKDMA_MAX_GPUS)
#define FPK_CHECKDMA_ACTION                3       //!< Which type of copy action:
#define FPK_CHECKDMA_ACTION_Noop       0           //!<  type no-op (free block)
#define FPK_CHECKDMA_ACTION_CpuCpy     1           //!<  type CPU reads and writes
#define FPK_CHECKDMA_ACTION_Twod       2           //!<  type TWOD (GPU)
#define FPK_CHECKDMA_ACTION_CopyEng0   3           //!<  type CE0 (GPU)
#define FPK_CHECKDMA_ACTION_CopyEng1   4           //!<  type CE1 (GPU)
#define FPK_CHECKDMA_ACTION_CopyEng2   5           //!<  type CE2 (GPU)
#define FPK_CHECKDMA_ACTION_CopyEng3   6           //!<  type CE3 (GPU)
#define FPK_CHECKDMA_ACTION_CopyEng4   7           //!<  type CE4 (GPU)
#define FPK_CHECKDMA_ACTION_CopyEng5   8           //!<  type CE5 (GPU)
#define FPK_CHECKDMA_ACTION_CopyEng6   9           //!<  type CE6 (GPU)
#define FPK_CHECKDMA_ACTION_CopyEng7  10           //!<  type CE7 (GPU)
#define FPK_CHECKDMA_ACTION_CopyEng8  11           //!<  type CE8 (GPU)
#define FPK_CHECKDMA_ACTION_CopyEng9  12           //!<  type CE9 (GPU)
#define FPK_CHECKDMA_ACTION_NUM_ACTIONS 13
#define FPK_CHECKDMA_SIZE_BYTES            4       //!< Number of bytes in a copy
#define FPK_CHECKDMA_WIDTH_BYTES           5       //!< Width in bytes of the copied rect (if 0, screen width)
#define FPK_CHECKDMA_SRC_OFFSET            6       //!< Offset in bytes from start of source
#define FPK_CHECKDMA_SHUFFLE_ACTIONS       7       //!< If !0, do copies in random order, else in order of starting offset in each dest surface
#define FPK_CHECKDMA_FORCE_PEER2PEER       8       //!< If !0, force actions to use 2 different GPU fb surfaces as source and dest
#define FPK_CHECKDMA_VERBOSE               9       //!< If !0, print details of this loop before sending to HW
#define FPK_CHECKDMA_FB_MAPPING           10       //!< Which FB surface to map this loop.
#define FPK_CHECKDMA_FB_MAPPING_FbGpu0  0          // HW restrictions on BR04-based dagwood boards.
#define FPK_CHECKDMA_FB_MAPPING_FbGpu1  1          // HW restrictions on BR04-based dagwood boards.
#define FPK_CHECKDMA_FB_MAPPING_NUM     (FPK_CHECKDMA_FB_MAPPING_FbGpu0 + CHECKDMA_MAX_GPUS)
#define FPK_CHECKDMA_PEER_DIRECTION       11       //!< Does this loop test peer reads, or writes?
#define FPK_CHECKDMA_PEER_DIRECTION_Read  0        //!< GPU reads peer's memory over bus
#define FPK_CHECKDMA_PEER_DIRECTION_Write 1        //!< GPU writes peer's memory over bus (not supported by ROP, so must be mem2mem)
#define FPK_CHECKDMA_CPU_WRITE_DIRECTION  12       //!< CPU writes -- incrementing addresses vs. decrementing
#define FPK_CHECKDMA_CPU_WRITE_DIRECTION_incr 0
#define FPK_CHECKDMA_CPU_WRITE_DIRECTION_decr 1
#define FPK_CHECKDMA_FORCE_NOT_PEER2PEER  13       //!< If !0, force actions to use only one GPU surface.
#define FPK_CHECKDMA_SRC_ADDR_MODE           14
#define FPK_CHECKDMA_SRC_ADDR_MODE_virtual    0
#define FPK_CHECKDMA_SRC_ADDR_MODE_physical   1
#define FPK_CHECKDMA_DST_ADDR_MODE           15
#define FPK_CHECKDMA_DST_ADDR_MODE_virtual    0
#define FPK_CHECKDMA_DST_ADDR_MODE_physical   1
#define FPK_CHECKDMA_NUM_PICKERS             16
//@}

//------------------------------------------------------------------------------
//@{
//! EvoLwrs FancyPicker Indexes
#define FPK_EVOLWRS_LWRSOR_POSITION                   0 //!< random, top, bottom, ...
#define FPK_EVOLWRS_LWRSOR_SIZE                       1 //!< cursor size: 32x32 or 64x64 pixels
#define FPK_EVOLWRS_LWRSOR_HOT_SPOT_X                 2 //!< <0;31> or <0;63> based on size
#define FPK_EVOLWRS_LWRSOR_HOT_SPOT_Y                 3 //!< <0;31> or <0;63> based on size
#define FPK_EVOLWRS_LWRSOR_FORMAT                     4 //!< cursor color format: A1R5G5B5 or A8R8G8B8
#define FPK_EVOLWRS_LWRSOR_COMPOSITION                5 //!< cursor composition: Alpha, PremultAlpha, Xor
#define FPK_EVOLWRS_USE_GAIN_OFFSET                   6
#define FPK_EVOLWRS_USE_GAMUT_COLOR_SPACE_COLWERSION  7
#define FPK_EVOLWRS_CSC_CHANNEL                       8
#define FPK_EVOLWRS_LUT_MODE                          9
#define FPK_EVOLWRS_DITHER_BITS                      10
#define FPK_EVOLWRS_DITHER_MODE                      11
#define FPK_EVOLWRS_DITHER_PHASE                     12
#define FPK_EVOLWRS_BACKGROUND_FORMAT                13
#define FPK_EVOLWRS_BACKGROUND_SCALER_H              14
#define FPK_EVOLWRS_BACKGROUND_SCALER_V              15
#define FPK_EVOLWRS_USE_STEREO_LWRSOR                16
#define FPK_EVOLWRS_USE_STEREO_BACKGROUND            17
#define FPK_EVOLWRS_USE_DP_FRAME_PACKING             18
#define FPK_EVOLWRS_DYNAMIC_RANGE                    19
#define FPK_EVOLWRS_RANGE_COMPRESSION                20
#define FPK_EVOLWRS_COLOR_SPACE                      21
#define FPK_EVOLWRS_CHROMA_LPF                       22
#define FPK_EVOLWRS_SAT_COS                          23
#define FPK_EVOLWRS_SAT_SINE                         24
#define FPK_EVOLWRS_OR_PIXEL_BPP                     25
#define FPK_EVOLWRS_NUM_PICKERS                      26
//@}

//------------------------------------------------------------------------------
//@{
//! EvoOvrl FancyPicker Indexes
#define FPK_EVOOVRL_SURFACE_WIDTH                     0 //!< Overlay surface width in pixels
#define FPK_EVOOVRL_SURFACE_HEIGHT                    1 //!< Overlay surface height in pixels
#define FPK_EVOOVRL_SURFACE_FORMAT                    2 //!< Overlay surface format
#define FPK_EVOOVRL_SURFACE_LAYOUT                    3 //!< Pitch or Block Linear
#define FPK_EVOOVRL_USE_WHOLE_SURFACE                 4 //!< If !0: PointInX, Y = 0, 0 and SizeInW, H = SurfaceW, H
#define FPK_EVOOVRL_SCALE_OUTPUT                      5 //!< If !0: SizeOutW, H != SizeInW, H
#define FPK_EVOOVRL_OVERLAY_POSITION                  6 //!< Force top left corner of overlay image to particular region
#define FPK_EVOOVRL_OVERLAY_POSITION_ScreenRandom         1
#define FPK_EVOOVRL_OVERLAY_POSITION_Top                  2
#define FPK_EVOOVRL_OVERLAY_POSITION_Bottom               3
#define FPK_EVOOVRL_OVERLAY_POSITION_Left                 4
#define FPK_EVOOVRL_OVERLAY_POSITION_Right                5
#define FPK_EVOOVRL_OVERLAY_POSITION_TopLeft              6
#define FPK_EVOOVRL_OVERLAY_POSITION_TopRight             7
#define FPK_EVOOVRL_OVERLAY_POSITION_BottomLeft           8
#define FPK_EVOOVRL_OVERLAY_POSITION_BottomRight          9
#define FPK_EVOOVRL_OVERLAY_POINT_IN_X                7 //!< When displaying subregion of overlay surface, start at this point, depends on Surface Width
#define FPK_EVOOVRL_OVERLAY_POINT_IN_Y                8 //!< When displaying subregion of overlay surface, start at this point, depends on Surface Height
#define FPK_EVOOVRL_OVERLAY_SIZE_IN_WIDTH             9 //!< When displaying subregion of overlay surface, use this size, depends on PointInX and Surface Width
#define FPK_EVOOVRL_OVERLAY_SIZE_IN_HEIGHT           10 //!< When displaying subregion of overlay surface, use this size, depends on PointInY and Surface Height
#define FPK_EVOOVRL_OVERLAY_SIZE_OUT_WIDTH           11
#define FPK_EVOOVRL_OVERLAY_COMPOSITION_CONTROL_MODE 12
#define FPK_EVOOVRL_OVERLAY_KEY_COLOR                13
#define FPK_EVOOVRL_OVERLAY_KEY_MASK                 14
#define FPK_EVOOVRL_OVERLAY_LUT_MODE                 15
#define FPK_EVOOVRL_USE_GAIN_OFFSET                  16
#define FPK_EVOOVRL_USE_GAMUT_COLOR_SPACE_COLWERSION 17
#define FPK_EVOOVRL_USE_STEREO                       18
#define FPK_EVOOVRL_NUM_PICKERS                      19
//@}

//------------------------------------------------------------------------------
//@{
//! LwDisplayRandom FancyPicker Indexes
#define FPK_LWDRNDM_VIEWPORT_POINT_IN_X                     0
#define FPK_LWDRNDM_VIEWPORT_POINT_IN_Y                     1
#define FPK_LWDRNDM_VIEWPORT_SIZE_IN_WIDTH                  2
#define FPK_LWDRNDM_VIEWPORT_SIZE_IN_HEIGHT                 3
#define FPK_LWDRNDM_VIEWPORT_VALID_SIZE_IN_WIDTH            4
#define FPK_LWDRNDM_VIEWPORT_VALID_SIZE_IN_HEIGHT           5
#define FPK_LWDRNDM_VIEWPORT_VALID_POINT_IN_X               6
#define FPK_LWDRNDM_VIEWPORT_VALID_POINT_IN_Y               7
#define FPK_LWDRNDM_VIEWPORT_SIZE_OUT_WIDTH                 8
#define FPK_LWDRNDM_VIEWPORT_SIZE_OUT_HEIGHT                9
#define FPK_LWDRNDM_VIEWPORT_POINT_OUT_ADJUST_X            10
#define FPK_LWDRNDM_VIEWPORT_POINT_OUT_ADJUST_Y            11
#define FPK_LWDRNDM_WINDOW_FORMAT                          12
#define FPK_LWDRNDM_RASTER_OR_BPP                          13
#define FPK_LWDRNDM_DE_GAMMA                               14
#define FPK_LWDRNDM_SWAP_UV                                15
#define FPK_LWDRNDM_CLAMP_BEFORE_BLEND                     16
#define FPK_LWDRNDM_INPUT_RANGE                            17
#define FPK_LWDRNDM_ENABLE_GAIN_OFFSET                     18
#define FPK_LWDRNDM_WIN_POSITION                           19
#define FPK_LWDRNDM_WIN_SIZE                               20
#define FPK_LWDRNDM_ENABLE_CSC                             21
#define FPK_LWDRNDM_DITHER_BITS                            22
#define FPK_LWDRNDM_DITHER_MODE                            23
#define FPK_LWDRNDM_DITHER_PHASE                           24
#define FPK_LWDRNDM_MAX_OUTPUT_SCALE_FACTOR_VERTICAL       25
#define FPK_LWDRNDM_MAX_OUTPUT_SCALE_FACTOR_HORIZONTAL     26
#define FPK_LWDRNDM_COMPOSITION_PRESET                     27
#define FPK_LWDRNDM_COMPOSITION_PRESET_opaque                 0
#define FPK_LWDRNDM_COMPOSITION_PRESET_transparent            1
#define FPK_LWDRNDM_COMPOSITION_PRESET_random                 2
#define FPK_LWDRNDM_TIMESTAMP_MODE_ENABLE                  28
#define FPK_LWDRNDM_OR_PROTOCOL                            29
#define FPK_LWDRNDM_OR_PROTOCOL_single_tmds_a                 0
#define FPK_LWDRNDM_OR_PROTOCOL_single_tmds_b                 1
#define FPK_LWDRNDM_OR_PROTOCOL_dual_tmds                     2
#define FPK_LWDRNDM_OR_PROTOCOL_dp_sst                        3
#define FPK_LWDRNDM_OR_PROTOCOL_dp_dual_sst                   4
#define FPK_LWDRNDM_OR_PROTOCOL_dp_mst                        5
#define FPK_LWDRNDM_OR_PROTOCOL_dp_dual_mst                   6
#define FPK_LWDRNDM_OR_PROTOCOL_dp_dual_head_one_or           7
#define FPK_LWDRNDM_OR_PROTOCOL_tmds_frl                      8
#define FPK_LWDRNDM_OR_PROTOCOL_tmds_frl_dual_head_one_or     9
#define FPK_LWDRNDM_WINDOW_LAYOUT                          30
#define FPK_LWDRNDM_LWRSOR_ENABLE                          31
#define FPK_LWDRNDM_LWRSOR_FORMAT                          32
#define FPK_LWDRNDM_LWRSOR_SIZE                            33
#define FPK_LWDRNDM_SOR_INDEX                              34
#define FPK_LWDRNDM_LWRSOR_HOTSPOT_X                       35
#define FPK_LWDRNDM_LWRSOR_HOTSPOT_Y                       36
#define FPK_LWDRNDM_LWRSOR_DEGAMMA                         37
#define FPK_LWDRNDM_LWRSOR_COMPOSITION_MODE                38
#define FPK_LWDRNDM_LWRSOR_FACTOR                          39
#define FPK_LWDRNDM_VIEWPORT_FACTOR                        40
#define FPK_LWDRNDM_OUTPUT_LUT                             41
#define FPK_LWDRNDM_INPUT_LUT                              42
#define FPK_LWDRNDM_PIXEL_CLOCK                            43
#define FPK_LWDRNDM_PIXEL_CLOCK_ilwalid                       0
#define FPK_LWDRNDM_PIXEL_CLOCK_min                           1
#define FPK_LWDRNDM_PIXEL_CLOCK_max                           2
#define FPK_LWDRNDM_PIXEL_CLOCK_random                        3
#define FPK_LWDRNDM_PIXEL_CLOCK_skip_change                   4
#define FPK_LWDRNDM_RASTER_SIZE                            44
#define FPK_LWDRNDM_RASTER_SIZE_min_depreciated               0
#define FPK_LWDRNDM_RASTER_SIZE_max_width                     1
#define FPK_LWDRNDM_RASTER_SIZE_max_height                    2
#define FPK_LWDRNDM_RASTER_SIZE_random                        3
#define FPK_LWDRNDM_MIN_PRESENT_INT                        45
#define FPK_LWDRNDM_MIN_PRESENT_INT_min                       0
#define FPK_LWDRNDM_MIN_PRESENT_INT_random                    1
#define FPK_LWDRNDM_WINDOW_SURFACE_TYPE                    46
#define FPK_LWDRNDM_WINDOW_SURFACE_TYPE_tall                  0
#define FPK_LWDRNDM_WINDOW_SURFACE_TYPE_wide                  1
#define FPK_LWDRNDM_WINDOW_SURFACE_TYPE_rand                  2
#define FPK_LWDRNDM_WINDOW_SURFACE_TYPE_max                   3
#define FPK_LWDRNDM_WINDOW_COLOR_SPACE                     47
#define FPK_LWDRNDM_PROCAMP_COLOR_SPACE                    48
#define FPK_LWDRNDM_PICK_STEREOMODE                        49
#define FPK_LWDRNDM_ONE_SHOT_MODE_ENABLE                   50
#define FPK_LWDRNDM_ILUT_LWRVE                             51
#define FPK_LWDRNDM_OLUT_LWRVE                             52
#define FPK_LWDRNDM_DSC_ENABLE                             53
#define FPK_LWDRNDM_MIN_DUAL_HEAD_ONE_OR_PAIRS             54
#define FPK_LWDRNDM_MIN_DUAL_HEAD_ONE_OR_PAIRS_PAIR_M0_S1     1
#define FPK_LWDRNDM_MIN_DUAL_HEAD_ONE_OR_PAIRS_PAIR_M2_S3     2
#define FPK_LWDRNDM_MIN_DUAL_HEAD_ONE_OR_PAIRS_ALL            3
#define FPK_LWDRNDM_RASTER_LOCK_MASTER_HEAD                55
#define FPK_LWDRNDM_RASTER_LOCK_MASTER_HEAD_0                 0
#define FPK_LWDRNDM_RASTER_LOCK_MASTER_HEAD_2                 2
#define FPK_LWDRNDM_TMO_LUT                                56
#define FPK_LWDRNDM_TMO_LUT_LWRVE                          57
#define FPK_LWDRNDM_V_UPSCALING_ALLOWED                    58
#define FPK_LWDRNDM_INPUT_SCALER_H                         59
#define FPK_LWDRNDM_INPUT_SCALER_V                         60
#define FPK_LWDRNDM_INPUT_SCALER_TAPS_H                    61
#define FPK_LWDRNDM_INPUT_SCALER_TAPS_V                    62
#define FPK_LWDRNDM_WIN_USAGE_BOUNDS_INPUT_SCALER_TAPS     63
#define FPK_LWDRNDM_OUTPUT_SCALER_TAPS_H                   64
#define FPK_LWDRNDM_OUTPUT_SCALER_TAPS_V                   65
#define FPK_LWDRNDM_DSC_REF_MODE                           66
#define FPK_LWDRNDM_POLARITY_X                             67
#define FPK_LWDRNDM_POLARITY_Y                             68
#define FPK_LWDRNDM_WIN_CLAMP_RANGE_MIN                    69
#define FPK_LWDRNDM_WIN_BEGIN_MODE                         70
#define FPK_LWDRNDM_WIN_BEGIN_MODE_NON_TEARING                0
#define FPK_LWDRNDM_WIN_BEGIN_MODE_IMMEDIATE                  1
#define FPK_LWDRNDM_PRECOMP_H_SCALING                      71
#define FPK_LWDRNDM_PRECOMP_V_SCALING                      72
#define FPK_LWDRNDM_SCALER_TAPS                            73
#define FPK_LWDRNDM_SCALER_TAPS_2                             0
#define FPK_LWDRNDM_SCALER_TAPS_5                             1
#define FPK_LWDRNDM_PRECOMP_SCALING                        74
// Pickers for HW rotation and flipping of window surfaces - start
#define FPK_LWDRNDM_HORIZ_SCAN_DIR                         75 
#define FPK_LWDRNDM_HORIZ_SCAN_DIR_LTR                        0
#define FPK_LWDRNDM_HORIZ_SCAN_DIR_RTL                        1
#define FPK_LWDRNDM_VER_SCAN_DIR                           76
#define FPK_LWDRNDM_VER_SCAN_DIR_TTB                          0 
#define FPK_LWDRNDM_VER_SCAN_DIR_BTT                          1
// Pickers for HW rotation and flipping of window surfaces - end
#define FPK_LWDRNDM_NUM_PICKERS                            77
//@}

//------------------------------------------------------------------------------
//@{
//! SecTest FancyPicker Indexes
#define FPK_SEC_BLOCK_COUNT        0           //!< Size of the transfer in 16 byte blocks
#define FPK_SEC_SRC_BLOCK_OFFSET   1           //!< Block-offset into source surface
#define FPK_SEC_DST_BLOCK_OFFSET   2           //!< Block-offset into destination surface
#define FPK_SEC_SRC_IS_FB          3           //!< If !0, use FB surface for source (else NC host memory)
#define FPK_SEC_DST_IS_FB          4           //!< If !0, use FB surface for dest (else NC host memory)
#define FPK_SEC_COPY_ACTION        5           //!< Plain copy / encrypt / decrypt
#define FPK_SEC_COPY_ACTION_encrypt    0       //!< do an encrypting copy, and a decrypting copy, check each worked
#define FPK_SEC_COPY_ACTION_decrypt    1       //!< do a decrypting copy, and an encrypting copy, check each worked
#define FPK_SEC_SEPARATE_SEMA      6           //!< send a separate semaphore-release method rather than releasing the semaphore with the dma-trigger method
#define FPK_SEC_AWAKEN_SEMA        7           //!< send a GPU interrupt along with the semaphore release
#define FPK_SEC_SUPERVERBOSE       8           //!< if !0, print details just before sending the methods
#define FPK_SEC_APP_ID             9           //!< LWSEC_SET_APPLICATION_ID
#define FPK_SEC_APP_ID_ctr64           0
#define FPK_SEC_NUM_PICKERS       10
//@}

//------------------------------------------------------------------------------
//@{
//! Elpg Test FancyPicker Indexes
#define FPK_ELPGTEST_SUBTEST            0           //!< select one of the subtests
#define FPK_ELPGTEST_SUBTEST_grmethods      0       //!< send Graphics methods to ilwoke PG_ON
#define FPK_ELPGTEST_SUBTEST_grprivreg      1       //!< issue Graphics reg read to ilwoke PG_ON
#define FPK_ELPGTEST_SUBTEST_vicmethods     4       //!< send Video Compositor method read to ilwoke PG_ON
#define FPK_ELPGTEST_SUBTEST_vicprivreg     5       //!< send Video Compositor reg read to ilwoke PG_ON
#define FPK_ELPGTEST_SUBTEST_pwrrailreg     6       //!< send reg read to ilwoke power rail gate
#define FPK_ELPGTEST_SUBTEST_pwrrailmethods 7       //!< send methods to ilwoke power rail gate
#define FPK_ELPGTEST_VIDEOENGINE        1
#define FPK_ELPGTEST_DEEPIDLE           2           //!< Whether the GPU should go into deep idle
#define FPK_ELPGTEST_DEEPIDLE_off           0       //!< Do not go into deep idle
#define FPK_ELPGTEST_DEEPIDLE_on            1       //!< Go into deep idle
#define FPK_ELPGTEST_ENGINE             3           //!< engine used to ilwoke power gate
#define FPK_ELPGTEST_ENGINE_gr              0       //!< use gr to wake up from pwr rail
#define FPK_ELPGTEST_ENGINE_vic             2       //!< use vic to wake up from pwr rail
#define FPK_ELPGTEST_NUM_PICKERS        4
//@}

//------------------------------------------------------------------------------
//@{
//! PcieSpeedChange FancyPicker Indexes
#define FPK_PEXSPEEDTST_LINKSPEED          0        //!< Subtest to run
#define FPK_PEXSPEEDTST_LINKSPEED_gen1     0
#define FPK_PEXSPEEDTST_LINKSPEED_gen2     1
#define FPK_PEXSPEEDTST_LINKSPEED_gen3     2
#define FPK_PEXSPEEDTST_LINKSPEED_gen4     3
#define FPK_PEXSPEEDTST_LINKSPEED_gen5     4
#define FPK_PEXSPEEDTST_NUM_PICKERS        1
//@}

//------------------------------------------------------------------------------
//@{
//! Elpg Test FancyPicker Indexes
#define FPK_GLSTRESSTEST_POWER_WAIT        0  //!< Perform a power wait after the
                                              //!< specified quad strip or frame
#define FPK_GLSTRESSTEST_DELAYMS_AFTER_PG  1  //!< Time in ms to delay after PG
#define FPK_GLSTRESSTEST_DEEPIDLE_PSTATE   2   //!< DeepIdle Pstates that are needed to be tested
#define FPK_GLSTRESSTEST_NUM_PICKERS       3

//@}

//------------------------------------------------------------------------------
//@{
//! ClockPulseTest FancyPicker Indexes
#define FPK_CPULSE_DWELL_MSEC       0   //!< Dwell time in milliseconds at each loop.
#define FPK_CPULSE_FREQ_HZ          1   //!< Pulse frequency in Hertz.
#define FPK_CPULSE_DUTY_PCT         2   //!< Duty cycle as pct 1..100 at high clock.
#define FPK_CPULSE_SLOW_RATIO       3   //!< low/high clock speed as float: 1.0 for no slowdown, 0.5 for 2x slowdown, etc.
#define FPK_CPULSE_GRAD_STEP_DUR    4   //!< GRAD_STEP_DURATION in clocks, 0 for square transitions
#define FPK_CPULSE_VERBOSE          5   //!< Print details at Normal priority (vs debug)
#define FPK_CPULSE_NUM_PICKERS      6
//@}

//------------------------------------------------------------------------------
//@{
//! MME Random test FancyPicker Indexes
#define FPK_MMERANDOM_MACRO_SIZE           0  //!< Size of the Macro
#define FPK_MMERANDOM_MACRO_OVERSIZE       1  //!< Adustment for macro size when it runs over
#define FPK_MMERANDOM_MACRO_OVERSIZE_Fill        0
#define FPK_MMERANDOM_MACRO_OVERSIZE_Truncate    1

#define FPK_MMERANDOM_MACRO_BLOCK_TYPE     2
#define FPK_MMERANDOM_MACRO_BLOCK_TYPE_NoBranch        0
#define FPK_MMERANDOM_MACRO_BLOCK_TYPE_ForwardBranch   1
#define FPK_MMERANDOM_MACRO_BLOCK_TYPE_BackwardBranch  2

#define FPK_MMERANDOM_MACRO_BRA_SRC0       3

#define FPK_MMERANDOM_MACRO_BRA_NZ         4
#define FPK_MMERANDOM_MACRO_BRA_NZ_False     0
#define FPK_MMERANDOM_MACRO_BRA_NZ_True      1

#define FPK_MMERANDOM_MACRO_BRA_NDS        5
#define FPK_MMERANDOM_MACRO_BRA_NDS_False     0
#define FPK_MMERANDOM_MACRO_BRA_NDS_True      1

#define FPK_MMERANDOM_MACRO_BRA_SRC0_TO_BRA 6

#define FPK_MMERANDOM_MACRO_SPACING         7

#define FPK_MMERANDOM_MACRO_RELEASE_ONLY    8
#define FPK_MMERANDOM_MACRO_RELEASE_ONLY_Disable     0
#define FPK_MMERANDOM_MACRO_RELEASE_ONLY_Enable      1

#define FPK_MMERANDOM_MACRO_BLOCK_LOAD            9
#define FPK_MMERANDOM_MACRO_BLOCK_LOAD_Disable     0
#define FPK_MMERANDOM_MACRO_BLOCK_LOAD_Enable      1

#define FPK_MMERANDOM_MACRO_BLOCK_CALL            10
#define FPK_MMERANDOM_MACRO_BLOCK_CALL_Disable     0
#define FPK_MMERANDOM_MACRO_BLOCK_CALL_Enable      1

#define FPK_MMERANDOM_NUM_PICKERS          11

#define FPK_B1REMAP_WIDTH                       0
#define FPK_B1REMAP_HEIGHT                      1
#define FPK_B1REMAP_BLOCK_HEIGHT_LOG2_GOBS      2
#define FPK_B1REMAP_COLOR_FORMAT                3
#define FPK_B1REMAP_NUM_PICKERS                 4
//@}

//------------------------------------------------------------------------------
//@{
//! Line Test FancyPicker Indices
#define FPK_LINETEST_COLOR          0
#define FPK_LINETEST_ROP            1
#define FPK_LINETEST_BETA1          2
#define FPK_LINETEST_BETA4          3
#define FPK_LINETEST_OP             4
#define FPK_LINETEST_XPOS           5
#define FPK_LINETEST_YPOS           6
#define FPK_LINETEST_NUM_PICKERS    7
//@}

//------------------------------------------------------------------------------
//@{
//! CheetAh Window Test FancyPicker Indices
#define FPK_TEGWIN_COLOR_FORMAT           0
#define FPK_TEGWIN_BYTE_SWAP              1
#define FPK_TEGWIN_CHROMA_MODE            2
#define FPK_TEGWIN_420                    3
#define FPK_TEGWIN_LWRSOR_REL_WINDOW      4
#define FPK_TEGWIN_DRAW_TYPE              5
#define FPK_TEGWIN_BLEND_WEIGHT           6
#define FPK_TEGWIN_COLOR_KEY              7
#define FPK_TEGWIN_LWRSOR_SIZE            8
#define FPK_TEGWIN_CMU_CSC_ENTRY          9
#define FPK_TEGWIN_SURFACE_FORMAT         10
#define FPK_TEGWIN_BLEND_SRC_COLOR_MATCH  11
#define FPK_TEGWIN_BLEND_DST_COLOR_MATCH  12
#define FPK_TEGWIN_BLEND_SRC_ALPHA_MATCH  13
#define FPK_TEGWIN_BLEND_DST_ALPHA_MATCH  14
#define FPK_TEGWIN_BLEND_COLOR_KEY_SELECT 15
#define FPK_TEGWIN_BLEND_LAYER_MODE       16
#define FPK_TEGWIN_TEST_MODE              17
#define FPK_TEGWIN_SIMPLE_COLOR_FORMAT    18
#define FPK_TEGWIN_PLANAR_CHROMA0_FORMAT  19
#define FPK_TEGWIN_OUTPUT_COLOR_MODE      20
#define FPK_TEGWIN_LWRSOR_SRC_BLEND       21
#define FPK_TEGWIN_LWRSOR_DST_BLEND       22
#define FPK_TEGWIN_LWRSOR_COLOR_FORMAT    23
#define FPK_TEGWIN_PREDEF_BLEND_MODE      24
#define FPK_TEGWIN_SRC_TYPE               25
#define FPK_TEGWIN_SRC_TYPE_normal         0
#define FPK_TEGWIN_SRC_TYPE_compressed     1
#define FPK_TEGWIN_WINDOW_IDX             26
#define FPK_TEGWIN_WINDOW_COUNT           27
#define FPK_TEGWIN_DEGAMMA                28
#define FPK_TEGWIN_RASTER_SIZE            29
#define FPK_TEGWIN_OPE_FMT                30
#define FPK_TEGWIN_OPE_YUV_BPP            31
#define FPK_TEGWIN_SCAN_METHOD            32
#define FPK_TEGWIN_SUBTEST                33
#define FPK_TEGWIN_SUBTEST_win_features        0 // focus on testing features
#define FPK_TEGWIN_SUBTEST_max_pclk            1 // focus on testing at max pclk
#define FPK_TEGWIN_NUM_PICKERS            34
//@}

//------------------------------------------------------------------------------
//@{
//! FancyPicker sanity-test FancyPicker Indices
#define FPK_FPK_RANDOM          0
#define FPK_FPK_RANDOM_F        1
#define FPK_FPK_SHUFFLE         2
#define FPK_FPK_STEP            3
#define FPK_FPK_LIST            4
#define FPK_FPK_JS              5
#define FPK_FPK_NUM_PICKERS     6
//@}

//------------------------------------------------------------------------------
//@{
//! FancyPicker GC5/GC6 test indices
#define GC_CMN_BASE                               0
#define GC_CMN_WAKEUP_EVENT         (GC_CMN_BASE+ 0)   // Common wakeup events for GC5/GC6
#define GC_CMN_WAKEUP_EVENT_none                  0
// Bits 7:0 reserved for GC6, 15:8 reserved for GC5
// These bits are mutually exclusive.
#define GC_CMN_WAKEUP_EVENT_gc6gpu              (1<<0) //!< ask the EC to generate a gpu event
#define GC_CMN_WAKEUP_EVENT_gc6i2c              (1<<1) //!< read the therm sensor via I2C
#define GC_CMN_WAKEUP_EVENT_gc6timer            (1<<2) //!< program the SCI timer to wakeup the gpu
#define GC_CMN_WAKEUP_EVENT_gc6hotplug          (1<<3) //!< ask the EC to simulate a hot plug detect
#define GC_CMN_WAKEUP_EVENT_gc6hotplugirq       (1<<4) //!< ask the EC to simulate a hot plug detect IRQ
#define GC_CMN_WAKEUP_EVENT_gc6hotunplug        (1<<5) //!< ask the EC to simulate a hot unplug detect
#define GC_CMN_WAKEUP_EVENT_gc6i2c_bypass       (1<<6) //!< ask the EC to generate fake i2c event
#define GC_CMN_WAKEUP_EVENT_gc6rtd3_cpu         (1<<7) //!< simulate RTD3 CPU wake event
#define GC_CMN_WAKEUP_EVENT_gc6rtd3_gputimer    (1<<8) //!< simulate RTD3 GPU Timer wake event
#define GC_CMN_WAKEUP_EVENT_gc6all_mfg          0x47   //!< bitmaks of all mfg wakeup events to test
#define GC_CMN_WAKEUP_EVENT_rtd3all             0x180   //!< bitmask of all RTD3 events
#define GC_CMN_WAKEUP_EVENT_gc6all              0x1ff   //!< bitmask of all wakeup events to test
#define GC_CMN_WAKEUP_EVENT_gc5rm               (1<<16) //!< force wakeup via RM ctrl call.
#define GC_CMN_WAKEUP_EVENT_gc5pex              (1<<17) //!< read a GPU register to generate PEX traffic
#define GC_CMN_WAKEUP_EVENT_gc5i2c              (1<<18) //!< read the therm sensor via I2C
#define GC_CMN_WAKEUP_EVENT_gc5timer            (1<<19) //!< program the SCI timer to wakeup the gpu
#define GC_CMN_WAKEUP_EVENT_gc5alarm            (1<<20) //!< schedule a PTIMER event
#define GC_CMN_WAKEUP_EVENT_gc5all              (0x1f<<16)//!< bit mask of all wakeup events
#define GC_CMN_WAKEUP_EVENT_gc5first            GC_CMN_WAKEUP_EVENT_gc5rm
#define GC_CMN_WAKEUP_EVENT_NUM_EVENTS_gc5      5 //!< total number of gc5 wakeup events
#define GC_CMN_WAKEUP_EVENT_NUM_EVENTS_gc6      9 //!< total number of gc6 wakeup events
#define GC_CMN_WAKEUP_EVENT_NUM_EVENTS          (GC_CMN_WAKEUP_EVENT_NUM_EVENTS_gc5 + \
                                                GC_CMN_WAKEUP_EVENT_NUM_EVENTS_gc6) //!< total number of wakeup events.
#define GC_CMN_BASE_NUM_PICKERS                 1 //!< The one and only common picker.
//@}
//
//------------------------------------------------------------------------------
//@{
//! FancyPicker GC6 test Indices
// All of these DELAY settings are in usecs
#define GC6_BASE                    GC_CMN_BASE_NUM_PICKERS
#define GC6_PWR_DN_POST_DELAY       (GC6_BASE+ 0) // how long to wait after entering GC6 pwr state
#define GC6_PWR_UP_POST_DELAY       (GC6_BASE+ 1) // how long to wait after exiting GC6 pwr state.
#define GC6_PWR_DELAY_MAX           (GC6_BASE+ 2) // max value for the above pickers.
#define GC6_WAKEUP_EVENT_TIMER_VAL  (GC6_BASE+ 3) // SCI's internal wakeup timer value (in usec)
// insert new pickers above this and adjust this define.
#define GC6_NUM_PICKERS        (GC6_BASE+4)

//@}

//------------------------------------------------------------------------------
//@{
//! FancyPicker GC5 test Indices
// All of these DELAY settings are in usecs
#define GC5_BASE                         GC_CMN_BASE_NUM_PICKERS
#define GC5_PWR_DN_POST_DELAY            (GC5_BASE+ 0) // how long to wait after entering GC5 pwr state
#define GC5_PWR_UP_POST_DELAY            (GC5_BASE+ 1) // how long to wait after exiting GC5 pwr state.
#define GC5_PWR_DELAY_MAX                (GC5_BASE+ 2) // max value for the above pickers.
#define GC5_WAKEUP_EVENT_TIMER_VAL       (GC5_BASE+ 3)// SCI's internal wakeup timer value (in usec)
#define GC5_WAKEUP_EVENT_ALARM_TIMER_VAL (GC5_BASE+ 4)// SCI's special purpose alarm timer value (in usec)
// insert new pickers above this and adjust this define.
#define GC5_NUM_PICKERS        (GC5_BASE+5)

#define GCX_BASE                         GC_CMN_BASE_NUM_PICKERS
#define GCX_PWR_DN_POST_DELAY            (GCX_BASE+ 0) // how long to wait after entering GC5 pwr state
#define GCX_PWR_UP_POST_DELAY            (GCX_BASE+ 1) // how long to wait after exiting GC5 pwr state.
#define GCX_PWR_DELAY_MAX                (GCX_BASE+ 2) // max value for the above pickers.
#define GCX_WAKEUP_EVENT_TIMER_VAL       (GCX_BASE+ 3) // SCI's internal wakeup timer value (in usec)
#define GCX_WAKEUP_EVENT_ALARM_TIMER_VAL (GCX_BASE+ 4) // SCI's special purpose alarm timer value (in usec)
#define GCX_GC5_PSTATE                   (GCX_BASE+ 5) // What pstates to use when doing GC5 cycles
// insert new pickers above this and adjust this define.
#define GCX_NUM_PICKERS        (GCX_BASE+6)

#define GCX_TEST_ALGORITHM_default       0   // use new GCxTest algorithm
#define GCX_TEST_ALGORITHM_gc6           1   // fallback to test 147 algorithm
#define GCX_TEST_ALGORITHM_gc5           2   // fallback to test 247 algorithm
#define GCX_TEST_ALGORITHM_rtd3          4
#define GCX_TEST_ALGORITHM_fgc6          8

//@}
//
// Random2d
//

//------------------------------------------------------------------------------
//@{
//! CheetAh CEC Test FancyPicker Indices
#define FPK_TEGCEC_NUM_ADDRS                 0
#define FPK_TEGCEC_ADDR                      1
#define FPK_TEGCEC_BROADCAST                 2
#define FPK_TEGCEC_NUM_DATA                  3
#define FPK_TEGCEC_DATA                      4
#define FPK_TEGCEC_NUM_PICKERS               5
//@}

//------------------------------------------------------------------------------
//@{
//! TegraVic FancyPicker Indices
#define FPK_TEGRAVIC_PERCENT_SLOTS           0
#define FPK_TEGRAVIC_FRAME_FORMAT            1
#define FPK_TEGRAVIC_DEINTERLACE_MODE        2
#define FPK_TEGRAVIC_PIXEL_FORMAT            3
#define FPK_TEGRAVIC_BLOCK_KIND              4
#define FPK_TEGRAVIC_FLIPX                   5
#define FPK_TEGRAVIC_FLIPY                   6
#define FPK_TEGRAVIC_TRANSPOSE               7
#define FPK_TEGRAVIC_IMAGE_WIDTH             8
#define FPK_TEGRAVIC_IMAGE_HEIGHT            9
#define FPK_TEGRAVIC_DENOISE                10
#define FPK_TEGRAVIC_CADENCE_DETECT         11
#define FPK_TEGRAVIC_RECTX                  12
#define FPK_TEGRAVIC_RECTY                  13
#define FPK_TEGRAVIC_LIGHT_LEVEL            14
#define FPK_TEGRAVIC_RGB_DEGAMMAMODE        15
#define FPK_TEGRAVIC_YUV_DEGAMMAMODE        16
#define FPK_TEGRAVIC_GAMUT_MATRIX           17
#define FPK_TEGRAVIC_NUM_PICKERS            18

//------------------------------------------------------------------------------
//@{
//! VicLDCTNR FancyPicker Indices
#define FPK_VICLDCTNR_IMAGE_WIDTH             0
#define FPK_VICLDCTNR_IMAGE_HEIGHT            1
#define FPK_VICLDCTNR_RECTX                   2
#define FPK_VICLDCTNR_RECTY                   3
#define FPK_VICLDCTNR_PIXEL_FORMAT            4
#define FPK_VICLDCTNR_BLOCK_KIND              5
#define FPK_VICLDCTNR_FRAME_FORMAT            6
#define FPK_VICLDCTNR_PIXEL_FILTERTYPE        7
#define FPK_VICLDCTNR_TESTMODE                8
#define FPK_VICLDCTNR_NUM_PICKERS             9

//@}
//------------------------------------------------------------------------------
//@{
//! LwjpgTest FancyPicker Indices
#define FPK_LWJPG_TEST_TYPE         0
#define FPK_LWJPG_IMAGE_WIDTH       1
#define FPK_LWJPG_IMAGE_HEIGHT      2
#define FPK_LWJPG_PIXEL_FORMAT      3
#define FPK_LWJPG_IMG_CHROMA_MODE   4
#define FPK_LWJPG_JPG_CHROMA_MODE   5
#define FPK_LWJPG_YUV_MEMORY_MODE   6
#define FPK_LWJPG_RESTART_INTERVAL  7
#define FPK_LWJPG_USE_BYTE_STUFFING 8
#define FPK_LWJPG_USE_RANDOM_IMAGES 9
#define FPK_LWJPG_USE_RANDOM_TABLES 10
#define FPK_LWJPG_NUM_PICKERS       11

//@}
//------------------------------------------------------------------------------
//@{
//! GLLWPR test Fancypicker Indices

// Magic Numbers(path ids) 1-7 are chosen by me and are not suggested by any document.
// The same values will be used as pathid(path name) for paths.
// The only condition is that numbers(path ids) should be unique.
#define FPK_FIG_SIMPLEPATH                      1
#define FPK_FIG_CONTOUR1                        2
#define FPK_FIG_CONTOUR2                        3
#define FPK_FIG_CONTOUR3                        4
#define FPK_FIG_FILLPOINTS                      5
#define FPK_FIG_MIXED_CONTOURS                  6
#define FPK_FIG_SHARED_EDGE                     7

#define FPK_FIG_GLYPH0                          8
#define FPK_FIG_GLYPH1                          9

#define FPK_FIG_MISC0                           10
#define FPK_FIG_MISC1                           11 //Non path rendering simple triangle

#define FPK_FIRST_PATH                          FPK_FIG_SIMPLEPATH
#define FPK_LAST_PATH                           FPK_FIG_SHARED_EDGE
#define FPK_FIRST_GLYPH                         FPK_FIG_GLYPH0
#define FPK_LAST_GLYPH                          FPK_FIG_GLYPH1
#define FPK_FIRST_MISCFIG                       FPK_FIG_MISC0
#define FPK_LAST_MISCFIG                        FPK_FIG_MISC1

// Following macro represents total number of Glyphs.
#define FPK_FIG_TOTALGLYPHS                     (FPK_LAST_GLYPH - FPK_FIRST_GLYPH) + 1
//Following macro represents number Lwrves to draw.
#define FPK_FIG_TOTALPATHS                      (FPK_LAST_PATH - FPK_FIRST_PATH) + 1
//Following macro represents number of misc diagrams
#define FPK_FIG_TOTALMISCFIG                    (FPK_LAST_MISCFIG - FPK_FIRST_MISCFIG) + 1
//Following macro is the total number of diagrams named above.
#define FPK_FIG_TOTAL                           FPK_FIG_TOTALGLYPHS + FPK_FIG_TOTALPATHS + FPK_FIG_TOTALMISCFIG

//Following macro represent exclusive range of pathids for shared edge problem
#define FPK_FIG_SHARED_ROWCOL_MAX               60
#define FPK_FIG_SHARED_MIN_PATH                 50
#define FPK_FIG_SHARED_MAX_PATH                 FPK_FIG_SHARED_MIN_PATH +\
                    (2 * FPK_FIG_SHARED_ROWCOL_MAX * FPK_FIG_SHARED_ROWCOL_MAX)

//Combination of colour samples vs raster or depth samples
#define FPK_MSAA_2x2                            0
#define FPK_MSAA_4x4                            1
#define FPK_MSAA_8x8                            2
#define FPK_MSAA_16x16                          3
#define FPK_MSAA_2x4                            4
#define FPK_MSAA_2x8                            5
#define FPK_MSAA_2x16                           6
#define FPK_MSAA_4x8                            7
#define FPK_MSAA_4x16                           8
#define FPK_MSAA_8x16                           9

#define FPK_LOG_MODE_Skip                       0    //   value for LWPR_LOG_MODE selfcheck
#define FPK_LOG_MODE_Store                      1    //   value for LWPR_LOG_MODE selfcheck
#define FPK_LOG_MODE_Check                      2    //   value for LWPR_LOG_MODE selfcheck

#define MSAA_SET_BEGIN                          FPK_MSAA_8x8
#define MSAA_SET_END                            FPK_MSAA_16x16
#define NUM_MSAA_SETS                           MSAA_SET_END - MSAA_SET_BEGIN +1
#define TIR_SET_BEGIN                           FPK_MSAA_4x16
#define TIR_SET_END                             FPK_MSAA_8x16
#define NUM_TIR_SAMPLES                         TIR_SET_END - TIR_SET_BEGIN + 1
#define NUM_TEST_SAMPLES                        NUM_MSAA_SETS + NUM_TIR_SAMPLES

#define LWPR_PATH_INLOGFILE                     0
#define LWPR_PATH_ONSCREEN                      1
#define LWPR_PATH_DONOTLOG                      2

#define LWPR_TEST_FIGURE                        0
#define LWPR_SHARED_PATHID                      1
#define LWPR_TEST_SAMPLES                       2
#define LWPR_TEST_VERBOSITY                     3
#define LWPR_LOG_MODE                           4
#define LWPR_FIG_NUMPICKERS                     5
//@}

//------------------------------------------------------------------------------
//@{
//! GLCompute FancyPicker indices

//! Number of time steps to evolve each simulation world.
//! The test runs each simulation world this many steps forwards and then
//! backwards in "time" before checking that it returned to initial state.
#define FPK_GLCOMPUTE_SIM_STEPS       0

//! Size of the simulation world, in cells.
//! The world will actually be set to a square containing about this many cells.
//! This picker controls the workload for each run of the compute shader.
#define FPK_GLCOMPUTE_SIM_CELLS       1

//! Size to draw each simulation cell, in pixels.
//! The cells will actually be drawn as roughly square blocks on-screen, with
//! about this many pixels.  This is a FLOAT32 picker, accepting fractional values.
//! This picker sort of indirectly controls the workload for graphics.
#define FPK_GLCOMPUTE_CELL_PIXELS     2

//! Verify the simultaneous compute and graphics are actually oclwring after every nth simulation
#define FPK_GLCOMPUTE_NUM_PICKERS     3
//@}

//------------------------------------------------------------------------------
//@{
//! LWDEC GCx test pickers

//! This picker control if GCx should be activated/not
#define FPK_LWDEC_GCX_ACTIVATE_BUBBLE           0

//! This picker will tell how long to wait for a GC5/GC6 entry
#define FPK_LWDEC_GCX_PWRWAIT_DELAY_MS          1

//! Total number of pickers in LWDEC GCX test
#define FPK_LWDEC_GCX_NUM_PICKERS               2
//@}

//------------------------------------------------------------------------------
//@{
//! PerfPunish FancyPickers

#define FPK_PERFPUNISH_VF_SWITCHES_PER_PSTATE 0
#define FPK_PERFPUNISH_PSTATE_NUM             1
#define FPK_PERFPUNISH_INTERSECT              2
#define FPK_PERFPUNISH_INTERSECT_disabled         0
#define FPK_PERFPUNISH_INTERSECT_lwvdd            1
#define FPK_PERFPUNISH_INTERSECT_lwvdds           2
#define FPK_PERFPUNISH_VMIN                   3
#define FPK_PERFPUNISH_VMIN_disabled              0
#define FPK_PERFPUNISH_VMIN_below                 1
#define FPK_PERFPUNISH_VMIN_above_or_equal        2
#define FPK_PERFPUNISH_GPCCLK                 4
#define FPK_PERFPUNISH_TIMING_US              5
#define FPK_PERFPUNISH_CLK_TOPOLOGY           6
#define FPK_PERFPUNISH_RANGE                  7
#define FPK_PERFPUNISH_RANGE_IN                   3
#define FPK_PERFPUNISH_RANGE_OUT                  4
#define FPK_PERFPUNISH_NUM_PICKERS            8

//@}
//------------------------------------------------------------------------------
//@{
//! LwvidLwOfa FancyPicker Indices
#define FPK_LWVIDLWOFA_OFAMODE                      0
#define FPK_LWVIDLWOFA_IMAGE_WIDTH                  1
#define FPK_LWVIDLWOFA_IMAGE_HEIGHT                 2
#define FPK_LWVIDLWOFA_IMAGE_BITDEPTH               3
#define FPK_LWVIDLWOFA_GRID_SIZE_LOG2               4
#define FPK_LWVIDLWOFA_PYD_NUM                      5
#define FPK_LWVIDLWOFA_PASS_NUM                     6
#define FPK_LWVIDLWOFA_ADAPTIVEP2                   7
#define FPK_LWVIDLWOFA_ROI_MODE                     8
#define FPK_LWVIDLWOFA_ENDIAGONAL                   9
#define FPK_LWVIDLWOFA_SUBFRAMEMODE                 10
#define FPK_LWVIDLWOFA_PYD_LEVEL_FILTER_EN          11
#define FPK_LWVIDLWOFA_PYD_FILTER_TYPE              12
#define FPK_LWVIDLWOFA_PYD_FILTER_K_SIZE            13
#define FPK_LWVIDLWOFA_PYD_FILTER_SIGMA             14
#define FPK_LWVIDLWOFA_PYD_HIGH_PERF_MODE           15
#define FPK_LWVIDLWOFA_NUM_PICKERS                  16

//@}
//------------------------------------------------------------------------------
//@{
//! Lwvid JPG Test FancyPicker Indices
#define FPK_LWVID_JPG_COLOR_FORMAT         0
#define FPK_LWVID_JPG_YUV_TO_RGB_PARAMS    1
#define FPK_LWVID_JPG_ALPHA                2
#define FPK_LWVID_JPG_DOWNSCALE_FACTOR     3
#define FPK_LWVID_JPG_NUM_PICKERS          4

//@}
//------------------------------------------------------------------------------
//@{
//! LwvidLWENC FancyPicker Indices
#define FPK_LWVIDLWENC_CODEC                        0
#define FPK_LWVIDLWENC_NUM_FRAMES                   1
#define FPK_LWVIDLWENC_CONTENT_ACTION               2
#define FPK_LWVIDLWENC_CONTENT_ACTION_FILL_RANDOM       0
#define FPK_LWVIDLWENC_CONTENT_ACTION_FILL_PATTERN      1
#define FPK_LWVIDLWENC_CONTENT_ACTION_MOVE              2
#define FPK_LWVIDLWENC_CONTENT_ACTION_SPRINKLE          3
#define FPK_LWVIDLWENC_CONTENT_PATTERN_IDX          3
#define FPK_LWVIDLWENC_CONTENT_MOVE_NUM_BLOCKS      4
#define FPK_LWVIDLWENC_CONTENT_MOVE_X               5
#define FPK_LWVIDLWENC_CONTENT_MOVE_Y               6
#define FPK_LWVIDLWENC_CONTENT_MOVE_WIDTH           7
#define FPK_LWVIDLWENC_CONTENT_MOVE_HEIGHT          8
#define FPK_LWVIDLWENC_CONTENT_MOVE_LOCATION        9
#define FPK_LWVIDLWENC_CONTENT_SPRINKLE_OFFSET     10
#define FPK_LWVIDLWENC_CONTENT_SPRINKLE_AND_VALUE  11
#define FPK_LWVIDLWENC_CONTENT_SPRINKLE_XOR_VALUE  12
#define FPK_LWVIDLWENC_NUM_PICKERS                 13

//@}
//------------------------------------------------------------------------------
//@{
//! TegraLwOfa FancyPicker Indices
#define FPK_TEGRALWOFA_OFAMODE          0
#define FPK_TEGRALWOFA_IMAGE_WIDTH      1
#define FPK_TEGRALWOFA_IMAGE_HEIGHT     2
#define FPK_TEGRALWOFA_IMAGE_BITDEPTH   3
#define FPK_TEGRALWOFA_GRID_SIZE_LOG2   4
#define FPK_TEGRALWOFA_PYD_NUM          5
#define FPK_TEGRALWOFA_ROI_MODE         6
#define FPK_TEGRALWOFA_SUBFRAMEMODE     7
#define FPK_TEGRALWOFA_ENDIAGONAL       8
#define FPK_TEGRALWOFA_ADAPTIVEP2       9
#define FPK_TEGRALWOFA_PASS_NUM         10
#define FPK_TEGRALWOFA_NUM_PICKERS      11

//@}
//------------------------------------------------------------------------------
//@{
//! TegraLwJpg FancyPicker Indices
#define FPK_TEGRALWJPG_ENABLE_RATE_CONTROL  0
#define FPK_TEGRALWJPG_ENCODE_QUALITY       1
#define FPK_TEGRALWJPG_RESTART_INTERVAL     2
#define FPK_TEGRALWJPG_COLOR_FORMAT         3
#define FPK_TEGRALWJPG_YUV_TO_RGB_PARAMS    4
#define FPK_TEGRALWJPG_ALPHA                5
#define FPK_TEGRALWJPG_DOWNSCALE_FACTOR     6
#define FPK_TEGRALWJPG_PIXEL_PACK_FMT       7
#define FPK_TEGRALWJPG_NUM_PICKERS          8

//@}
//------------------------------------------------------------------------------
//@{
//! TegraLwEnc FancyPicker Indices
#define FPK_TEGRALWENC_NUM_FRAMES   0
#define FPK_TEGRALWENC_NUM_PICKERS  1
//@}
