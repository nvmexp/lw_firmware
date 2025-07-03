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
 * @brief       Linear Divider Version 1
 * @version     Clocks 3.1 and after
 * @see         https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @see         //hw/libs/common/analog/lwpu/doc/
 * @see         //hw/doc/gpu/volta/volta/physical/clocks/Volta_clocks_block_diagram.vsd
 * @author      Eric Colter
 * @author      Antone Vogt-Varvak
 *
 * @details     Turing Linear Dividers (LDIVs) are composites of one mux, and
 *              two LDIV units that feed into the mux.
 *
 *              This is the software viewpoint of an LDIV:
 *                                         |\
 *                                         | \
 *                                         |  \
 *                           _______       |   |
 *               input1 ----| LDIV1 |------| 1 |
 *                          |_______|      |   |
 *                                         |   |----- output
 *                           _______       |   |
 *               input0 ----| LDIV0 |------| 0 |
 *                          |_______|      |   |
 *                                         |  /
 *                                         | /
 *                                         |/
 *
 *              Typically, 'input1' connected to the domain-specific PLL and/or
 *              NAFLL.  This is often called the VCO path or PLL path.
 *
 *              'input0' is usually connected to the ALT OSM and is called the
 *              alternate or bypass path.
 *
 *              Version 1 Linear Dividers were retired as of Ampere.
 */

#ifndef CLK3_FS_LDIV_H
#define CLK3_FS_LDIV_H


/* ------------------------ Includes --------------------------------------- */

#include "clk3/fs/clkldivunit.h"
#include "clk3/fs/clkmux.h"
#include "clk3/clkfieldvalue.h"


/* ------------------------ Macros ----------------------------------------- */

/*!
 * @brief       Number of inputs for the multiplexer
 * @memberof    ClkLdiv
 * @see         ClkLdiv::input
 * @see         clkMuxValueMap_Ldiv
 * @protected
 */
#define CLK_INPUT_MAIN_MUX_COUNT__LDIV 2

/*!
 * @brief       Index to mux input array
 * @memberof    ClkLdiv
 * @see         ClkLdiv::input
 * @see         ClkMux::muxValueMap
 * @see         CLK_STATIC_INIT__LDIV
 * @public
 *
 * @details     These indices are used to index both the input array
 *              'ClkLdiv::input' and 'ClkLDiv::mux.muxValueMap'.
 *
 *              They are strictly internal to the LDIV itself and not exposed.
 *              As such, they are initialized by 'CLK_STATIC_INIT__LDIV'.
 */
/**@{*/
#define CLK_INPUT_DIV0__LDIV        0x0
#define CLK_INPUT_DIV1__LDIV        0x1
/**@}*/


/*!
 * @brief       Initialize a clkLdiv
 *
 * @details     This macro produces the static initializer list to initialize an
 *              'clkLdiv' object.  It is assumed that the macro is used in the
 *              litter-specific source (e.g. 'clksdtu100.c') to initialize
 *              'ClkLdiv' objects in 'clkSchematicDag'.
 *
 *              The following members must be initialized outside this macro:
 *                  .div0.super.pInput      (usually the bypass path)
 *                  .div0.maxLdivValue      (usually 2 to disable or 63)
 *                  .div0.maxFracDivValue   (usually 2 to disable or 34)
 *                  .div1.super.pInput      (usually the VCO/PLL/NAFLL path)
 *                  .div1.maxLdivValue      (usually 2 to disable or 63)
 *                  .div1.maxFracDivValue   (usually 2 to disable or 34)
 *
 *              No comma should follow this macro.
 *
 * @note        This macro functions properly only on the PMU.  Specifically,
 *              designated initializers are not supported by some MSVC versions.
 *
 * @param[in]   _LDIVNAME           Name of the linear divicer object (e.g. dispclk.ldiv)
 * @param[in]   _MUXDEV             Mulptiplexer device (e.g. _PTRIM or _PVTRIM)
 * @param[in]   _MUXREG             Mulptiplexer control register (e.g. _SYS_SEL_VCO)
 * @param[in]   _STATREG            Mulptiplexer status register (e.g. _SYS_STATUS_SEL_VCO)
 * @param[in]   _MUXFIELD           Mulptiplexer field (e.g. _DISPCLK_OUT)
 * @param[in]   _MUXZERO            Mulptiplexer value to select div0 (e.g. _BYPASS)
 * @param[in]   _LDIVREG            Linear divider control register (e.g. _SYS_DISPCLK_OUT)
 */
#define CLK_STATIC_INIT__LDIV(_LDIVNAME, _MUXDEV, _MUXREG, _STATREG, _MUXFIELD, _MUXZERO, _LDIVREG)                 \
    ._LDIVNAME.mux.super.pVirtual           = &clkVirtual_Mux,                                                      \
    ._LDIVNAME.mux.muxRegAddr               = LW##_MUXDEV##_MUXREG,                                                 \
    ._LDIVNAME.mux.statusRegAddr            = LW##_MUXDEV##_STATREG,                                                \
    ._LDIVNAME.mux.muxValueMap              = clkSchematicDag._LDIVNAME.map,                                        \
    ._LDIVNAME.mux.input                    = clkSchematicDag._LDIVNAME.input,                                      \
    ._LDIVNAME.mux.count                    = CLK_INPUT_MAIN_MUX_COUNT__LDIV,                                       \
    ._LDIVNAME.mux.glitchy                  = LW_FALSE,                                                             \
                                                                                                                    \
    ._LDIVNAME.div0.super.super.pVirtual    = &clkVirtual_LdivUnit,                                                 \
/*  ._LDIVNAME.div0.super.pInput            = Specified outside this macro */                                       \
    ._LDIVNAME.div0.ldivRegAddr             = LW##_MUXDEV##_LDIVREG,                                                \
/*  ._LDIVNAME.div0.maxLdivValue            = Specified outside this macro -- usually 63 */                         \
/*  ._LDIVNAME.div0.maxFracDivValue         = Specified outside this macro -- usually 34 */                         \
    ._LDIVNAME.div0.divider                 =  0,       /* as in div0 */                                            \
    ._LDIVNAME.div0.ldivCleanup             =  0,       /* disable WAR 1424323 */                                   \
                                                                                                                    \
    ._LDIVNAME.div1.super.super.pVirtual    = &clkVirtual_LdivUnit,                                                 \
/*  ._LDIVNAME.div1.super.pInput            = Specified outside this macro */                                       \
    ._LDIVNAME.div1.ldivRegAddr             = LW##_MUXDEV##_LDIVREG,                                                \
/*  ._LDIVNAME.div1.maxLdivValue            = Specified outside this macro -- usually 63 */                         \
/*  ._LDIVNAME.div1.maxFracDivValue         = Specified outside this macro -- usually 34 */                         \
    ._LDIVNAME.div1.divider                 =  1,       /* as in div1 */                                            \
    ._LDIVNAME.div1.ldivCleanup             =  0,       /* disable WAR 1424323 */                                   \
                                                                                                                    \
    ._LDIVNAME.map[CLK_INPUT_DIV0__LDIV]    = CLK_DRF__FIELDVALUE(_MUXDEV, _MUXREG, _MUXFIELD, _MUXZERO),           \
    ._LDIVNAME.map[CLK_INPUT_DIV1__LDIV]    = CLK_DRF_ILW__FIELDVALUE(_MUXDEV, _MUXREG, _MUXFIELD, _MUXZERO),       \
                                                                                                                    \
    ._LDIVNAME.input[CLK_INPUT_DIV0__LDIV]  = &clkSchematicDag._LDIVNAME.div0.super.super, /* Internal */           \
    ._LDIVNAME.input[CLK_INPUT_DIV1__LDIV]  = &clkSchematicDag._LDIVNAME.div1.super.super, /* Internal */


/* ------------------------ Datatypes -------------------------------------- */

typedef struct ClkLdiv ClkLdiv;


/*!
 * @brief       Linear Divider
 * @version     Clocks 3.1 and after
 * @protected
 *
 * @details     This struct is not a class, but merely composites of ClkFreqSrc-
 *              derived objects, specifically, a mulitplexer and two LDIV units.
 *
 *              It does not extend any other class nor does it contain member
 *              functions nor vtable.  It merely contains 'ClkFreqSrc' objects
 *              and a macro to properly initialize them.
 *
 */
struct ClkLdiv
{
/*!
 * @brief       Mux that selects between the two linear dividers
 */
    ClkMux      mux;

/*!
 * @brief       Ldiv unit that connects to the 0 input of the mux
 */
    ClkLdivUnit div0;

/*!
 * @brief       Ldiv unit that connects to the 1 input of the mux
 */
    ClkLdivUnit div1;

/*!
 * @brief       Map of inputs to the register values which select them
 *
 * @details     Element zero should be the field value that selects div0.
 *              Element one should be the field value that selects div1.
 */
    const ClkFieldValue map[CLK_INPUT_MAIN_MUX_COUNT__LDIV];

/*!
 * @brief       An array of input pointer to the mux
 *
 * @details     The inputs of the mux are div0 and div1, so this array is simply
 *              {address of div0 object, address of div1 object}
 */
    ClkFreqSrc * const input[CLK_INPUT_MAIN_MUX_COUNT__LDIV];
};


/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Include Derived Types -------------------------- */

#endif //CLK3_FS_LDIV_H

