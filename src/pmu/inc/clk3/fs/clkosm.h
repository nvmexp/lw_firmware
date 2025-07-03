/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or dis  closure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file
 * @brief       One Source Module
 * @version     Clocks 3.1 and after
 * @see         https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @see         //hw/libs/common/analog/lwpu/doc/
 * @see         //hw/doc/gpu/volta/volta/physical/clocks/Volta_clocks_block_diagram.vsd
 * @author      Eric Colter
 * @author      Antone Vogt-Varvak
 *
 * @details     Turing One Source Modules (OSMs) are merely composites of two
 *              multiplexers and two LDIV units, at least from the software
 *              viewpoint.  OSMs of this configuration were introduced in Kepler.
 *
 *              This is the software viewpoint of an OSM:
 *                                                 |\  main
 *                                                 | \ mux
 *                                                 |  \
 *                XTAL   --------------------------|8  |
 *                                                 |   |
 *                XTAL4x --------------------------|b  |
 *                                      ______     |   |-------- OSM OUTPUT
 *                SPPLL0 --------------| DIV0 |----|c  |
 *                              |\     |______|    |   |
 *                SPPLL1 -------|0\     ______     |   |
 *                              |  |---| DIV1 |----|d  |
 *                SYSPLL -------|1/    |______|    |  /
 *                              |/ div1            | /
 *                                 mux             |/
 *
 *              The hardware as shown in 'Volta_clocks_block_diagram.vsd' on
 *              the 'OSM' tab is a bit more complex than the equivalent software
 *              viewpoint shown above.
 *
 *              The main mux is actually four multiplexers, one of which is
 *              inside the LDIV box.  There are several inputs on the main mux
 *              which are not generally wired to anything in Pascal+ chips.
 *
 *              The other multiplexer is called the div1 mux because its output
 *              is the input to the DIV1 linear divider.  The VSD shows it as
 *              'Onesrcclk1 switch'.
 *
 *              The LDIV box contains two linear dividers, DIV0 and DIV1,
 *              and a multiplexer mentioned above.
 *
 *              OSMs were retired as of Ampere.
 */

#ifndef CLK3_FS_OSM_H
#define CLK3_FS_OSM_H


/* ------------------------ Includes --------------------------------------- */

#include "clk3/fs/clkldivunit.h"
#include "clk3/fs/clkmux.h"
#include "clk3/clkfieldvalue.h"


/* ------------------------ Macros ----------------------------------------- */

/*!
 * @brief       Number of inputs for the main multiplexer
 * @memberof    ClkOsm
 * @see         ClkOsm::input
 * @see         clkMuxValueMap_Osm
 * @protected
 *
 * @details     See 'ClkOsm::input' for a complete explanation.
 */
#define CLK_INPUT_MAIN_MUX_COUNT__OSM       14

/*!
 * @brief       Number of inputs for the DIV1 multiplexer
 * @memberof    ClkOsm
 * @see         ClkOsm::input
 * @see         clkMuxValueMap_Osm
 * @protected
 *
 * @details     See 'ClkOsm::input' for a complete explanation.
 */
#define CLK_INPUT_DIV1_MUX_COUNT__OSM        2


/*!
 * @brief       Number of inputs in arrays
 * @memberof    ClkOsm
 * @see         ClkOsm::input
 * @see         clkMuxValueMap_Osm
 * @protected
 *
 * @details     Number of elements in both the input arrays 'ClkOsm::input'
 *              and 'clkMuxValueMap_Osm'.
 */
#define CLK_INPUT_COUNT__OSM    (CLK_INPUT_MAIN_MUX_COUNT__OSM + CLK_INPUT_DIV1_MUX_COUNT__OSM)


/*!
 * @brief       Offset of the main multiplexer within input arrays
 * @memberof    ClkOsm
 * @see         ClkOsm::input
 * @see         clkMuxValueMap_Osm
 * @protected
 *
 * @details     See 'ClkOsm::input' for a complete explanation.
 */
#define CLK_INPUT_MAIN_MUX_OFFSET__OSM       0


/*!
 * @brief       Offset of the DIV1 multiplexer within input arrays
 * @memberof    ClkOsm
 * @see         ClkOsm::input
 * @see         clkMuxValueMap_Osm
 * @protected
 *
 * @details     See 'ClkOsm::input' for a complete explanation.
 */
#define CLK_INPUT_DIV1_MUX_OFFSET__OSM      CLK_INPUT_MAIN_MUX_COUNT__OSM


/*!
 * @brief       Index using generic names
 * @memberof    ClkOsm
 * @see         ClkOsm::input
 * @see         clkMuxValueMap_Osm
 * @public
 *
 * @details     These indices are used to index both the input array
 *              'ClkOsm::input' and 'clkMuxValueMap_Osm'.
 *
 *              The names of these symbols reflect the generic name of the
 *              input into the OSM rather than the functional name.
 *              These names are more-or-less based on the register field
 *              names as defined in the manuals.
 *
 *              CLK_INPUT_ONESRCCLK[01]__OSM are special in that they are
 *              strictly internal to the OSM itself and not exposed.
 */
/**@{*/
#define CLK_INPUT_MISCCLK0__OSM         (0x0 + CLK_INPUT_MAIN_MUX_OFFSET__OSM)
#define CLK_INPUT_MISCCLK1__OSM         (0x1 + CLK_INPUT_MAIN_MUX_OFFSET__OSM)
#define CLK_INPUT_MISCCLK2__OSM         (0x2 + CLK_INPUT_MAIN_MUX_OFFSET__OSM)
#define CLK_INPUT_MISCCLK3__OSM         (0x3 + CLK_INPUT_MAIN_MUX_OFFSET__OSM)
#define CLK_INPUT_MISCCLK4__OSM         (0x4 + CLK_INPUT_MAIN_MUX_OFFSET__OSM)
#define CLK_INPUT_MISCCLK5__OSM         (0x5 + CLK_INPUT_MAIN_MUX_OFFSET__OSM)
#define CLK_INPUT_MISCCLK6__OSM         (0x6 + CLK_INPUT_MAIN_MUX_OFFSET__OSM)
#define CLK_INPUT_MISCCLK7__OSM         (0x7 + CLK_INPUT_MAIN_MUX_OFFSET__OSM)
#define CLK_INPUT_SLOWCLK0__OSM         (0x8 + CLK_INPUT_MAIN_MUX_OFFSET__OSM)
#define CLK_INPUT_SLOWCLK1__OSM         (0x9 + CLK_INPUT_MAIN_MUX_OFFSET__OSM)
#define CLK_INPUT_SLOWCLK2__OSM         (0xa + CLK_INPUT_MAIN_MUX_OFFSET__OSM)
#define CLK_INPUT_SLOWCLK3__OSM         (0xb + CLK_INPUT_MAIN_MUX_OFFSET__OSM)

#define CLK_INPUT_ONESRCCLK0__OSM       (0xc + CLK_INPUT_MAIN_MUX_OFFSET__OSM)  /* internal */
#define CLK_INPUT_ONESRCCLK1__OSM       (0xd + CLK_INPUT_MAIN_MUX_OFFSET__OSM)  /* internal */

#define CLK_INPUT_ONESRCCLK1_0__OSM     (0x0 + CLK_INPUT_DIV1_MUX_OFFSET__OSM)
#define CLK_INPUT_ONESRCCLK1_1__OSM     (0x1 + CLK_INPUT_DIV1_MUX_OFFSET__OSM)
/**@}*/


/*!
 * @brief       Index using functional names
 * @memberof    ClkOsm
 * @see         CLK_STATIC_INIT__OSM
 * @see         //hw/doc/gpu/volta/volta/physical/clocks/Volta_clocks_block_diagram.vsd
 * @protected
 *
 * @details     These indices are used to index both the input array
 *              'ClkOsm::input' and 'clkMuxValueMap_Osm'.
 *
 *              The names of these symbols reflect the functional name.
 *
 *              If it is listed here, then CLK_STATIC_INIT__OSM automatically
 *              initializes this part of the input array 'ClkOsm::input'.
 *
 *              CLK_INPUT_SYSPLL__OSM is named for SYSPLL to be consistent with
 *              the manuals, but may actually be either the SYSPLL or the
 *              SYSNAFLL (at least in Pascal/Volta/Turing) based on the setting
 *              of 'syspll_mode'.  See Pascal_clocks_block_diagram.vsd or
 *              Volta_clocks_block_diagram.vsd.
 *
 *              CLK_INPUT_DIV[01]__OSM are special in that they are strictly
 *              internal to the OSM itself and not exposed.
 */
/**@{*/
#define CLK_INPUT_PEX_REFCLK__OSM       CLK_INPUT_MISCCLK0__OSM
#define CLK_INPUT_XTAL__OSM             CLK_INPUT_SLOWCLK0__OSM
#define CLK_INPUT_XTAL4X__OSM           CLK_INPUT_SLOWCLK3__OSM
#define CLK_INPUT_SPPLL1__OSM           CLK_INPUT_ONESRCCLK1_0__OSM
#define CLK_INPUT_SYSPLL__OSM           CLK_INPUT_ONESRCCLK1_1__OSM
#define CLK_INPUT_DIV0__OSM             CLK_INPUT_ONESRCCLK0__OSM
#define CLK_INPUT_DIV1__OSM             CLK_INPUT_ONESRCCLK1__OSM
/**@}*/


/*!
 * @brief       Initialize clkOsm
 * @see         clkSchematicDag
 * @see         ClkOsm::input
 * @see         http://www.open-std.org/JTC1/SC22/wg14/www/docs/n1124.pdf
 *
 * @details     This macro produces the static initializer list to initialize a
 *              'clkOsm' object.  It is assumed that the macro is used
 *              in the litter-specific source (e.g. 'clksdgv100.c') to
 *              initialize 'ClkOsm' objects in 'clkSchematicDag'.
 *
 *              In particular, this macro contains explicit references to objects
 *              within 'clkSchematicDag' including:
 *                  xtal
 *                  xtal4x
 *                  sppll0.readonly
 *                  sppll1.readonly
 *                  syspll.readonly or syspll.pll
 *              as well as references to the object being initialized,
 *              'clkSchematicDag.[osm]', where '[osm]' is specified by
 *              the '_OSMNAME' parameter.
 *
 *              This macro initializes only the inputs that are the same for
 *              all or most OSMs in all chips supprted by Clocks 3.1 (i.e. Volta+)
 *              so that the other inputs may be initialized outside this macro
 *              as needed.  Please use the public CLK_INPUT_ macros for indicies.
 *
 *              Most of these inputs are read-only.  The exception is SYSPLL,
 *              which is read-only except via SYSCLK.  As such, if the '_SYSCLK'
 *              parameter is LW_TRUE, the input is set to 'syspll.pll', otherwise
 *              'syspll.readonly'.
 *
 *              Any inputs that are not initialized are set to NULL according
 *              to the C99 standard (ISO/IEC 9899:TC2 WG14/N1124, May 6, 2005),
 *              Section 6.7.8, Paragraph 10, assuming that 'clkSchematicDag'
 *              has static storage duration (which it should).  These input can
 *              be initialized outside the macro per 'ClkOsm::input'.
 *
 *              No comma should follow this macro.
 *
 * @note        This macro functions properly only on the PMU.  Specifically,
 *              designated initializers are not supported by some MSVC versions.
 *
 * @param[in]   _OSMNAME            Name of the osm object (e.g. dispclk.ref or dispclk.alt)
 * @param[in]   _MUXREG             Address of the mulptiplexer control register
 * @param[in]   _LDIVREG            Address of the linear divider control register
 * @param[in]   _SYSCLK             LW_TRUE iff SYSPLL is programmable on this path
 */
#define CLK_STATIC_INIT__OSM(_OSMNAME, _MUXREG, _LDIVREG, _SYSCLK)                                                      \
    ._OSMNAME.mux.super.pVirtual            = &clkVirtual_Mux,                                                          \
    ._OSMNAME.mux.muxRegAddr                = (_MUXREG),                                                                \
    ._OSMNAME.mux.muxGateField.mask         = DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_STOPCLK),                           \
    ._OSMNAME.mux.muxGateField.value        = DRF_DEF(_PTRIM, _SYS_CLK_SWITCH, _STOPCLK, _YES),                         \
    ._OSMNAME.mux.muxValueMap               = CLK_INPUT_MAIN_MUX_OFFSET__OSM + clkMuxValueMap_Osm,                      \
    ._OSMNAME.mux.input                     = CLK_INPUT_MAIN_MUX_OFFSET__OSM + clkSchematicDag._OSMNAME.input,          \
    ._OSMNAME.mux.count                     = CLK_INPUT_MAIN_MUX_COUNT__OSM,                                            \
    ._OSMNAME.mux.glitchy                   = LW_FALSE,                                                                 \
                                                                                                                        \
    ._OSMNAME.div0.super.super.pVirtual     = &clkVirtual_LdivUnit,                                                     \
    ._OSMNAME.div0.super.pInput             = &clkSchematicDag.sppll0.readonly.super.super, /* Standard external */     \
    ._OSMNAME.div0.ldivRegAddr              = (_LDIVREG),                                                               \
    ._OSMNAME.div0.maxFracDivValue          = 34,                                                                       \
    ._OSMNAME.div0.maxLdivValue             = 63,                                                                       \
    ._OSMNAME.div0.divider                  = 0,        /* as in div0 */                                                \
    ._OSMNAME.div0.ldivCleanup              = 0,        /* disable WAR */                                               \
                                                                                                                        \
    ._OSMNAME.div1.div.super.super.pVirtual = &clkVirtual_LdivUnit,                                                     \
    ._OSMNAME.div1.div.super.pInput         = &clkSchematicDag._OSMNAME.div1.mux.super,   /* Internal mux */            \
    ._OSMNAME.div1.div.ldivRegAddr          = (_LDIVREG),                                                               \
    ._OSMNAME.div1.div.maxFracDivValue      = 34,                                                                       \
    ._OSMNAME.div1.div.maxLdivValue         = 63,                                                                       \
    ._OSMNAME.div1.div.divider              = 1,        /* as in div1 */                                                \
    ._OSMNAME.div1.div.ldivCleanup          = 0,        /* disable WAR */                                               \
                                                                                                                        \
    ._OSMNAME.div1.mux.super.pVirtual       = &clkVirtual_Mux,                                                          \
    ._OSMNAME.div1.mux.muxRegAddr           = (_MUXREG),                                                                \
    ._OSMNAME.div1.mux.muxValueMap          = CLK_INPUT_DIV1_MUX_OFFSET__OSM + clkMuxValueMap_Osm,                      \
    ._OSMNAME.div1.mux.input                = CLK_INPUT_DIV1_MUX_OFFSET__OSM + clkSchematicDag._OSMNAME.input,          \
    ._OSMNAME.div1.mux.count                = CLK_INPUT_DIV1_MUX_COUNT__OSM,                                            \
    ._OSMNAME.div1.mux.glitchy              = LW_TRUE,                                                                  \
                                                                                                                        \
    ._OSMNAME.input[CLK_INPUT_DIV0__OSM]    = &clkSchematicDag._OSMNAME.div0.super.super,       /* Internal divider to main mux */          \
    ._OSMNAME.input[CLK_INPUT_DIV1__OSM]    = &clkSchematicDag._OSMNAME.div1.div.super.super,   /* Internal divider to main mux */          \
    ._OSMNAME.input[CLK_INPUT_XTAL__OSM]    = &clkSchematicDag.xtal.super,                      /* Standard external input to main mux */   \
    ._OSMNAME.input[CLK_INPUT_XTAL4X__OSM]  = &clkSchematicDag.xtal4x.super,                    /* Standard external input to main mux */   \
    ._OSMNAME.input[CLK_INPUT_SPPLL1__OSM]  = &clkSchematicDag.sppll1.readonly.super.super,     /* Standard external input to div1.mux */   \
    ._OSMNAME.input[CLK_INPUT_SYSPLL__OSM]  = ((_SYSCLK)?                                       /* Standard external input to div1.mux */   \
                                                &clkSchematicDag.syspll.pll.super.super :       /* Programmable only from SYSCLK */         \
                                                &clkSchematicDag.syspll.readonly.super.super),

//
// This version of the macro is temporary and accommodates the pending hardware changes
// to replace OSMs in Ampere with the new SWDIVs.
// TODO_CLK3: Remove this once the OSMs are removed from all Ampere clock domains.
//
#define CLK_STATIC_INIT_WITHOUT_SPPLL1__OSM(_OSMNAME, _MUXREG, _LDIVREG, _SYSCLK)                                       \
    ._OSMNAME.mux.super.pVirtual            = &clkVirtual_Mux,                                                          \
    ._OSMNAME.mux.muxRegAddr                = (_MUXREG),                                                                \
    ._OSMNAME.mux.muxGateValue.mask         = DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_STOPCLK),                           \
    ._OSMNAME.mux.muxGateValue.value        = DRF_DEF(_PTRIM, _SYS_CLK_SWITCH, _STOPCLK, _YES),                         \
    ._OSMNAME.mux.muxValueMap               = CLK_INPUT_MAIN_MUX_OFFSET__OSM + clkMuxValueMap_Osm,                      \
    ._OSMNAME.mux.input                     = CLK_INPUT_MAIN_MUX_OFFSET__OSM + clkSchematicDag._OSMNAME.input,          \
    ._OSMNAME.mux.count                     = CLK_INPUT_MAIN_MUX_COUNT__OSM,                                            \
    ._OSMNAME.mux.glitchy                   = LW_FALSE,                                                                 \
                                                                                                                        \
    ._OSMNAME.div0.super.super.pVirtual     = &clkVirtual_LdivUnit,                                                     \
    ._OSMNAME.div0.super.pInput             = &clkSchematicDag.sppll0.readonly.super.super, /* Standard external */     \
    ._OSMNAME.div0.ldivRegAddr              = (_LDIVREG),                                                               \
    ._OSMNAME.div0.maxFracDivValue          = 34,                                                                       \
    ._OSMNAME.div0.maxLdivValue             = 63,                                                                       \
    ._OSMNAME.div0.divider                  = 0,        /* as in div0 */                                                \
    ._OSMNAME.div0.ldivCleanup              = 0,        /* disable WAR */                                               \
                                                                                                                        \
    ._OSMNAME.div1.div.super.super.pVirtual = &clkVirtual_LdivUnit,                                                     \
    ._OSMNAME.div1.div.super.pInput         = &clkSchematicDag._OSMNAME.div1.mux.super,   /* Internal mux */            \
    ._OSMNAME.div1.div.ldivRegAddr          = (_LDIVREG),                                                               \
    ._OSMNAME.div1.div.maxFracDivValue      = 34,                                                                       \
    ._OSMNAME.div1.div.maxLdivValue         = 63,                                                                       \
    ._OSMNAME.div1.div.divider              = 1,        /* as in div1 */                                                \
    ._OSMNAME.div1.div.ldivCleanup          = 0,        /* disable WAR */                                               \
                                                                                                                        \
    ._OSMNAME.div1.mux.super.pVirtual       = &clkVirtual_Mux,                                                          \
    ._OSMNAME.div1.mux.muxRegAddr           = (_MUXREG),                                                                \
    ._OSMNAME.div1.mux.muxValueMap          = CLK_INPUT_DIV1_MUX_OFFSET__OSM + clkMuxValueMap_Osm,                      \
    ._OSMNAME.div1.mux.input                = CLK_INPUT_DIV1_MUX_OFFSET__OSM + clkSchematicDag._OSMNAME.input,          \
    ._OSMNAME.div1.mux.count                = CLK_INPUT_DIV1_MUX_COUNT__OSM,                                            \
    ._OSMNAME.div1.mux.glitchy              = LW_TRUE,                                                                  \
                                                                                                                        \
    ._OSMNAME.input[CLK_INPUT_DIV0__OSM]    = &clkSchematicDag._OSMNAME.div0.super.super,       /* Internal divider to main mux */          \
    ._OSMNAME.input[CLK_INPUT_DIV1__OSM]    = &clkSchematicDag._OSMNAME.div1.div.super.super,   /* Internal divider to main mux */          \
    ._OSMNAME.input[CLK_INPUT_XTAL__OSM]    = &clkSchematicDag.xtal.super,                      /* Standard external input to main mux */   \
    ._OSMNAME.input[CLK_INPUT_XTAL4X__OSM]  = &clkSchematicDag.xtal4x.super,                    /* Standard external input to main mux */   \
/*  ._OSMNAME.input[CLK_INPUT_SPPLL1__OSM]  = SPPLL1 does not exist in GA100. However, OSMs are deprecated anyway for Ampere. */            \
    ._OSMNAME.input[CLK_INPUT_SYSPLL__OSM]  = ((_SYSCLK)?                                       /* Standard external input to div1.mux */   \
                                                &clkSchematicDag.syspll.pll.super.super :       /* Programmable only from SYSCLK */         \
                                                &clkSchematicDag.syspll.readonly.super.super),


/* ------------------------ Datatypes -------------------------------------- */

typedef struct ClkOsm ClkOsm;


/*!
 * @brief       One Source Module
 * @version     Clocks 3.1 and after
 * @protected
 *
 * @details     This struct is not a class, but merely composites of ClkFreqSrc-
 *              derived objects, specifically, two mulitplexers and two LDIV units.
 *
 *              It does not extend any other class nor does it contain member
 *              functions nor vtable.  It merely contains 'ClkFreqSrc' objects
 *              and a macro to properly initialize them.
 *
 * @todo        Add the 'syspll_mode' multiplexer to this struct.  It is a 2x1
 *              mux that is tied to the SYSPLL input of the DIV1 mux and selects
 *              between the SYSNAFLL and the SYSPLL.
 */
struct ClkOsm
{
/*!
 * @brief       The mux that serves as the root of the osm
 *
 * @details     This mux is the root of the osm, and its output of the whole osm.
 */
    ClkMux      mux;

/*!
 * @brief       The 0 unit of the two ldivunits within the osm
 */
    ClkLdivUnit div0;

/*!
 * @brief       The 1 unit of the two ldivunits within the osm and its input mux
 */
    struct
    {
/*!
 * @brief       The 1 unit of the two ldivunits wihtin the osm
 */
        ClkLdivUnit div;

/*!
 * @brief       The mux that provides the input to div1.
 */
        ClkMux      mux;
    } div1;

/*!
 * @brief       Inputs to each multiplixer
 * @see         ClkMux::muxValueMap
 * @see         clkMuxValueMap_Osm
 * @see         CLK_INPUT_MAIN_MUX_COUNT__OSM
 * @see         CLK_INPUT_MAIN_MUX_OFFSET__OSM
 * @see         CLK_INPUT_DIV1_MUX_COUNT__OSM
 * @see         CLK_INPUT_DIV1_MUX_OFFSET__OSM
 * @see         CLK_STATIC_INIT__OSM
 * @see         ClkSignalPath
 *
 * @details     This array contains pointers to the frequency sources for
 *              each input of each of the multiplexers.  This array and
 *              'clkMuxValueMap_Osm' are indexed by the CLK_INPUT_xxx
 *              constant macros, as shown numerically in 'index' in the
 *              table below.
 *
 *              index | path | mux  | int/ext  | div  | hardcoded
 *              ----- | ---- | ---- | -------- | ---- | ---------
 *                  0 |    0 | main | external |      |
 *                  1 |    1 | main | external |      |
 *                  2 |    2 | main | external |      |
 *                  3 |    3 | main | external |      |
 *                  4 |    4 | main | external |      |
 *                  5 |    5 | main | external |      |
 *                  6 |    6 | main | external |      |
 *                  7 |    7 | main | external |      |
 *                  8 |    8 | main | external |      | XTAL
 *                  9 |    9 | main | external |      | XTAL4X
 *                 10 |    a | main | external |      |
 *                 11 |    b | main | external |      |
 *                 12 |    c | main | internal | DIV0 | SPPLL0
 *                 13 |    d | main | internal | DIV1 | DIV1
 *                 14 |   0d | div1 | external | DIV1 | SPPLL1
 *                 15 |   1d | div1 | external | DIV1 | SYSPLL
 *
 *              'path' refers to the 'ClkSignalPath' used by the OSM.  The 'd'
 *              path would never be used bu itself since it is an intermediate
 *              to '0d' and '1d'.  Paths are shown in hexacedimal.
 *
 *              One array is used for both multiplexers as specified by 'mux'.
 *              To make this work, the ones marked 'main' are the indices zero
 *              through 14 (CLK_INPUT_MAIN_MUX_COUNT__OSM) plus a zero offset.
 *              (CLK_INPUT_MAIN_MUX_OFFSET__OSM).  Similarly, the inputs marked
 *              'div1' are the indicies zero through one (CLK_INPUT_DIV1_MUX_COUNT__OSM)
 *              plus offset 14 (CLK_INPUT_DIV1_MUX_OFFSET__OSM).
 *
 *              Each multiplexer has a 'ClkMux::muxValueMap' which is indexed
 *              with the same offset scheme in which one array, 'clkMuxValueMap_Osm',
 *              contains both maps.
 *
 *              'internal' indicates a main mux input which is connected to one
 *              of the dividers within the OSM as indicated by the 'div' column.
 *
 *              'div' indicates which divider (if any) is applied to the clock
 *              signal on that path.
 *
 *              For colwenience, the CLK_STATIC_INIT__OSM macro hardcodes the
 *              inputs as specified in the 'hardcoded' column.  These are the
 *              standard connections shared by all supported OSMs in all
 *              supported chips.  THose inputs shown as blank may be initialized
 *              in the litter-specific source (e.g. 'clksdgv100.c') as needed.
 *
 *              The input of DIV0 is external (not shown in table) and hardcoded
 *              to SPPLL0.  The input of DIV1 (not shown in table) is internally
 *              connected to the div1 mux, which in turn is connect externally
 *              tp SPPLL1 and SYSPLL.
 */
    ClkFreqSrc * const input[CLK_INPUT_COUNT__OSM];
};


/* ------------------------ External Definitions --------------------------- */

/*!
 * @brief       Map of mux inputs to register values
 * @memberof    ClkOsm
 * @see         ClkOsm::input
 * @see         ClkMux::muxValueMap
 * @protected
 *
 * @details     This ine array is shared by both multiplexers in the OSM.
 *              See 'ClkOsm::input' for a complete explanation.
 */
extern const ClkFieldValue clkMuxValueMap_Osm[CLK_INPUT_COUNT__OSM];


/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Include Derived Types -------------------------- */

#endif //CLK3_FS_OSM_H

