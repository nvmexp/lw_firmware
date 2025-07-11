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
 * @brief       4-Input Switch Divider with Automatic Selection
 * @version     Clocks 3.1 and after
 * @see         https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @see         Bug 2660988
 * @author      Antone Vogt-Varvak
 *
 * @details     Switch Dividers (SWDIVs) are composites of a multiplexer and
 *              an LDIV unit.  They use less area than the OSMs they replace.
 *              SWDIVs were introduced in Ampere.
 *
 *              There are several varieties of switch dividers.  This class
 *              handles the 4-input SWDIVs with an automatic selection feature.
 *
 *              Automatic selection is enabled by default, which means that the
 *              hardware ignores the CLOCK_SOURCE_SEL field, but switches the
 *              input based on readiness of the input clocks.  CLOCK_SEL_OVERRIDE
 *              controls enablement.
 *
 *              This variety was introduced with GA10x for UTILSCLK.
 *
 *                        |\
 *              ----------|0\
 *                        |  \          ______
 *              ----------|1  |        |      |
 *                        |   |--------| LDIV |---- OUTPUT
 *              ----------|2  |        |______|
 *                        |  /
 *              ----------|3/|
 *                        |/ |
 *                           |
 *              auto ------4-+
 */

#ifndef CLK3_FS_SWDIV4AUTOMATIC_H
#define CLK3_FS_SWDIV4AUTOMATIC_H


/* ------------------------ Includes --------------------------------------- */

#include "clk3/fs/clkldivv2.h"
#include "clk3/fs/clkmux.h"
#include "clk3/clkfieldvalue.h"


/* ------------------------ Macros ----------------------------------------- */

/*!
 * @brief       Number of inputs for the multiplexer
 * @memberof    SwDiv4Automatic
 * @see         SwDiv4Automatic::input
 * @see         clkMuxValueMap_SwDiv4Automatic
 * @protected
 */
#define CLK_INPUT_COUNT__SWDIV4AUTOMATIC  5U


/*!
 * @brief       Multiplexer Index using Manuals Names
 * @memberof    SwDiv4Automatic
 * @see         SwDiv4Automatic::input
 * @see         clkMuxValueMap_SwDiv4Automatic
 * @public
 *
 * @details     This macro produces a compile-time constant which is the value
 *              from 'dev_trim.h' or 'generic_dev_trim.h' corresponding to one
 *              of the multiplexer inputs.
 *
 *              These indices are used to index both 'SwDiv4Automatic::input'
 *              and 'clkMuxValueMap_SwDiv4Automatic'.
 *
 *              Unlike OSMs, the function varies by the clock domain.  As such,
 *              it is generally best to use the field value whose name reflects
 *              its function as defined in 'dev_trim.h'.
 *
 *              However, this is not always possible, so the generic name
 *              defined in 'generic_dev_trim.h' must be used.  For input 'i',
 *              use this expression:
 *              CLK_INPUT_INDEX__SWDIV4AUTOMATIC(_PTRIM, _SYS_SWITCH_DIVIDER, _SOURCE(i))
 *
 *              The macro uses BIT_IDX_32 since the multiplexer values in
 *              'dev_trim.h' are usually bitmapped.
 *
 *              This macro can not be used for the automatic mode since it is in a
 *              different register field.  Use CLK_INPUT_AUTO_INDEX__SWDIV4AUTOMATIC.
 *
 * @param[in]   _SWDIVDEV           Either _PTRIM or _PVTRIM
 * @param[in]   _SWDIVREG           Base register name
 * @param[in]   _SWDIVVAL           Value name
 */
#define CLK_INPUT_INDEX__SWDIV4AUTOMATIC(_SWDIVDEV, _SWDIVREG, _SWDIVVAL)      \
    BIT_IDX_32(LW ## _SWDIVDEV ## _SWDIVREG ## _CLOCK_SOURCE_SEL ## _SWDIVVAL)


/*!
 * @brief       Multiplexer Index for Automatic Feature
 * @memberof    SwDiv4Automatic
 * @see         SwDiv4Automatic::input
 * @see         clkMuxValueMap_SwDiv4Automatic
 * @public
 *
 * @details     This macro produces a compile-time constant which is the index
 *              corresponding to the CLOCK_SEL_OVERRIDE field (i.e automatic
 *              mode).
 *
 *              CLK_INPUT_INDEX__SWDIV4AUTOMATIC can not be used for this purpose
 *              since that macro assumes an actual register field.
 *
 *              This index is used to index both 'SwDiv4Automatic::input' and
 *              'clkMuxValueMap_SwDiv4Automatic'.
 *
 * @param[in]   _SWDIVDEV           Either _PTRIM or _PVTRIM
 * @param[in]   _SWDIVREG           Base register name
 * @param[in]   _SWDIVVAL           Value name
 */
#define CLK_INPUT_AUTO_INDEX__SWDIV4AUTOMATIC 4U


/*!
 * @brief       Initialize SwDiv4Automatic
 * @see         clkSchematicDag
 * @see         SwDiv4Automatic::input
 * @see         CLK_INPUT_INDEX__SWDIV4AUTOMATIC
 * @see         http://www.open-std.org/JTC1/SC22/wg14/www/docs/n1124.pdf
 * @see         clkldivunit.c
 *
 * @details     This macro produces the static initializer list to initialize a
 *              'SwDiv4Automatic' object.  It is assumed that the macro is used
 *              in the litter-specific source (e.g. 'clksdga100.c') to initialize
 *              'SwDiv4Automatic' objects in 'clkSchematicDag'.
 *
 *              The following fields are *not* initialized by this macro and
 *              must by initialized by the schematic dag or runtime constructor:
 *              .div.maxFracDivValue    LwU8 used as a fixed-point 5.1 binary
 *              .input[0]               ClkFreqSrc *
 *              .input[1]               ClkFreqSrc *
 *              .input[2]               ClkFreqSrc *
 *              .input[3]               ClkFreqSrc *
 *              .input[4]               ClkFreqSrc *
 *
 *              The purpose of 'SwDiv4Automatic::input' is to map values for the
 *              _CLOCK_SOURCE_SEL field to frequency source objects.
 *              CLK_INPUT_INDEX__SWDIV4AUTOMATIC is useful for specifying the index.
 *              Unlike OSMs, the input mapping varies by the clock domain.
 *
 *              _INIT should not be used since its name does not reflect the
 *              input mapping.
 *
 *              All elements of 'SwDiv4Automatic::input' should be specified
 *              in the schematic dag.  GCC and other modern compilers generally
 *              issue diagnostics if the same element in an array (such as
 *              'SwDiv4Automatic::input') is initialized more than once.  As such,
 *              if all four elements of this array are initialized, the compiler
 *              will complain if one of the 'dev_trim.h' field value macros
 *              differs from the expected.
 *
 *              There is often a connection in the hardware for which 'dev_trim.h'
 *              does not contain a definition, and in some cases, they are
 *              hardware values which may be applicable when clkRead is called.
 *              In these cases, use generic values defined in 'generic_dev_trim.h'.
 *
 *              If an input is not connected to anything, then the
 *              'ClkSwDiv4Automatic::input' element should be initialized to NULL.
 *              'ClkMux' contains logic to deal with that.
 *
 *              No comma should follow this macro.
 *
 * @note        Logic in 'clkldivunit.c' depends on the fact that the SWDIVs
 *              register field definitions that are the same as divider zero
 *              in the older LDIVs and OSMs.  As such, 'div.divider' is set to
 *              zero.  See 'clkldivunit.c' for details.
 *
 * @note        This macro functions properly only on the PMU.  Specifically,
 *              designated initializers are not supported by some MSVC versions.
 *
 * @param[in]   _SWDIVNAME          Name of the SWDIV object (e.g. utilsclk.swdiv)
 * @param[in]   _SWDIVREG           Address of the control register
 */
#define CLK_STATIC_INIT__SWDIV4AUTOMATIC(_SWDIVNAME, _SWDIVREG)                                             \
    ._SWDIVNAME.div.super.super.pVirtual    = &clkVirtual_LdivV2,                                           \
    ._SWDIVNAME.div.super.pInput            = &clkSchematicDag._SWDIVNAME.mux.super,    /* Internal */      \
    ._SWDIVNAME.div.ldivRegAddr             = (_SWDIVREG),                                                  \
                                                                                                            \
    ._SWDIVNAME.mux.super.pVirtual          = &clkVirtual_Mux,                                              \
    ._SWDIVNAME.mux.muxRegAddr              = (_SWDIVREG),                                                  \
    ._SWDIVNAME.mux.muxValueMap             = clkMuxValueMap_SwDiv4Automatic,                               \
    ._SWDIVNAME.mux.input                   = clkSchematicDag._SWDIVNAME.input,                             \
    ._SWDIVNAME.mux.count                   = CLK_INPUT_COUNT__SWDIV4AUTOMATIC,                             \
    ._SWDIVNAME.mux.glitchy                 = LW_FALSE,


/* ------------------------ Datatypes -------------------------------------- */

typedef struct ClkSwDiv4Automatic ClkSwDiv4Automatic;

/*!
 * @brief       4-Input Switch Divider
 * @version     Clocks 3.1 and after
 * @version     Ampere and after
 * @protected
 *
 * @details     This struct is not a class, but merely a composition of ClkFreqSrc-
 *              derived objects, specifically, two mulitplexers and two LDIV units.
 *
 *              It does not extend any other class nor does it contain member
 *              functions nor vtable.  It merely contains 'ClkFreqSrc' objects
 *              and a macro to properly initialize them.
 */
struct ClkSwDiv4Automatic
{
/*!
 * @brief       The LDIV unit that is the output of the SWDIV
 */
    ClkLdivV2   div;

/*!
 * @brief       The mux that provides the input to 'div'.
 */
    ClkMux      mux;

/*!
 * @brief       Inputs to each multiplixer
 * @see         ClkMux::muxValueMap
 * @see         clkMuxValueMap_SwDiv4Automatic
 * @see         CLK_INPUT_MUX_COUNT__SWDIV4AUTOMATIC
 * @see         CLK_STATIC_INIT__SWDIV4AUTOMATIC
 * @see         ClkSignalPath
 *
 * @details     This array contains pointers to the frequency sources for
 *              each input of each of the multiplexers.  This array and
 *              'clkMuxValueMap_SwDiv4Automatic' are indexed by the CLK_INPUT_xxx
 *              constant macros.
 */
    ClkFreqSrc * const input[CLK_INPUT_COUNT__SWDIV4AUTOMATIC];
};


/* ------------------------ External Definitions --------------------------- */

/*!
 * @brief       Map of mux inputs to register values
 * @memberof    SwDiv4Automatic
 * @see         SwDiv4Automatic::input
 * @see         ClkMux::muxValueMap
 * @protected
 *
 * @details     This ine array is shared by both multiplexers in the SWDIV.
 *              See 'SwDiv4Automatic::input' for a complete explanation.
 */
extern const ClkFieldValue clkMuxValueMap_SwDiv4Automatic[CLK_INPUT_COUNT__SWDIV4AUTOMATIC];


/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK3_FS_SWDIV4AUTOMATIC_H

