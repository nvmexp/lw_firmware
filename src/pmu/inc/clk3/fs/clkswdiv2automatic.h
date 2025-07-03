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
 * @brief       2-Input Switch Divider with Automatic Selection
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
 *              handles the 2-input SWDIVs with an automatic selection feature.
 *
 *              Automatic selection is enabled by default, which means that the
 *              hardware ignores the CLOCK_SOURCE_SEL field, but switches the
 *              input based on readiness of the input clocks.  CLOCK_SEL_OVERRIDE
 *              controls enablement.
 *
 *              This variety was introduced with GA10x for PWRCLK.
 *
 *                        |\
 *                        | \           ______
 *              ----------|0 |         |      |
 *                        |  |---------| LDIV |---- OUTPUT
 *              ----------|1 |         |______|
 *                        | /
 *                        |/|
 *                          |
 *              auto -----2-+
 */

#ifndef CLK3_FS_SWDIV2AUTOMATIC_H
#define CLK3_FS_SWDIV2AUTOMATIC_H


/* ------------------------ Includes --------------------------------------- */

#include "clk3/fs/clkldivv2.h"
#include "clk3/fs/clkmux.h"
#include "clk3/clkfieldvalue.h"


/* ------------------------ Macros ----------------------------------------- */

/*!
 * @brief       Number of inputs for the multiplexer
 * @memberof    ClkSwDiv2Automatic
 * @see         ClkSwDiv2Automatic::input
 * @see         clkMuxValueMap_SwDiv2Automatic
 * @protected
 */
#define CLK_INPUT_COUNT__SWDIV2AUTOMATIC 3U


/*!
 * @brief       Multiplexer Index for Automatic Feature
 * @memberof    ClkSwDiv2Automatic
 * @see         ClkSwDiv2Automatic::input
 * @see         clkMuxValueMap_SwDiv2Automatic
 * @public
 *
 * @details     This macro produces a compile-time constant which is the index
 *              corresponding to the CLOCK_SEL_OVERRIDE field (i.e automatic
 *              mode).
 *
 *              For other indices, use the field value defined in dev_trim.h.
 *              since that macro assumes an actual register field.
 *
 *              This index is used to index both 'ClkSwDiv2Automatic::input' and
 *              'clkMuxValueMap_SwDiv2Automatic'.
 *
 * @param[in]   _SWDIVDEV           Either _PTRIM or _PVTRIM
 * @param[in]   _SWDIVREG           Base register name
 * @param[in]   _SWDIVVAL           Value name
 */
#define CLK_INPUT_AUTO_INDEX__SWDIV2AUTOMATIC 2U


/*!
 * @brief       Initialize ClkSwDiv2Automatic
 * @see         clkSchematicDag
 * @see         ClkSwDiv2Automatic::input
 * @see         http://www.open-std.org/JTC1/SC22/wg14/www/docs/n1124.pdf
 * @see         clkldivunit.c
 *
 * @details     This macro produces the static initializer list to initialize a
 *              'clkSwDiv2Automatic' object.  It is assumed that the macro is
 *              used in the litter-specific source (e.g. 'clksdgh100.c') to
 *              initialize 'ClkSwDiv2Automatic' objects in 'clkSchematicDag'.
 *
 *              The following fields are *not* initialized by this macro and
 *              must by initialized by the schematic dag or runtime constructor:
 *              .div.maxFracDivValue    LwU8 used as a fixed-point 5.1 binary
 *              .input[0]               ClkFreqSrc *
 *              .input[1]               ClkFreqSrc *
 *              .input[2]               ClkFreqSrc *
 *
 *              The purpose of 'ClkSwDiv2Automatic::input' is to map values for
 *              the _CLOCK_SOURCE_SEL field to frequency source objects.  Unlike
 *              OSMs, the input mapping may vary by the clock domain.
 *
 *              _INIT should not be used since its name does not reflect the
 *              input mapping.
 *
 *              All elements of 'ClkSwDiv2Automatic::input' should be specified
 *              in the schematic dag.  GCC and other modern compilers generally
 *              issue diagnostics if the same element in an array (such as
 *              'ClkSwDiv2Automatic::input') is initialized more than once.
 *              As such, if both elements of this array are initialized, the
 *              compiler will complain if one of the 'dev_trim.h' field value
 *              macros differs from the expected.
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
#define CLK_STATIC_INIT__SWDIV2AUTOMATIC(_SWDIVNAME, _SWDIVREG)                                             \
    ._SWDIVNAME.div.super.super.pVirtual    = &clkVirtual_LdivV2,                                           \
    ._SWDIVNAME.div.super.pInput            = &clkSchematicDag._SWDIVNAME.mux.super,    /* Internal */      \
    ._SWDIVNAME.div.ldivRegAddr             = (_SWDIVREG),                                                  \
                                                                                                            \
    ._SWDIVNAME.mux.super.pVirtual          = &clkVirtual_Mux,                                              \
    ._SWDIVNAME.mux.muxRegAddr              = (_SWDIVREG),                                                  \
    ._SWDIVNAME.mux.muxValueMap             = clkMuxValueMap_SwDiv2Automatic,                               \
    ._SWDIVNAME.mux.input                   = clkSchematicDag._SWDIVNAME.input,                             \
    ._SWDIVNAME.mux.count                   = CLK_INPUT_COUNT__SWDIV2AUTOMATIC,                             \
    ._SWDIVNAME.mux.glitchy                 = LW_FALSE,

/*  ._SWDIVNAME.input[all]                  = Specified by schematic dag initializer */


/* ------------------------ Datatypes -------------------------------------- */

typedef struct ClkSwDiv2Automatic ClkSwDiv2Automatic;

/*!
 * @brief       2-Input Switch Divider
 * @version     Clocks 3.1 and after
 * @version     GA10x and after
 * @protected
 *
 * @details     This struct is not a class, but merely a composition of ClkFreqSrc-
 *              derived objects, specifically, two mulitplexers and two LDIV units.
 *
 *              It does not extend any other class nor does it contain member
 *              functions nor vtable.  It merely contains 'ClkFreqSrc' objects
 *              and a macro to properly initialize them.
 */
struct ClkSwDiv2Automatic
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
 * @see         clkMuxValueMap_SwDiv2Automatic
 * @see         CLK_INPUT_MUX_COUNT__SWDIV2AUTOMATIC
 * @see         CLK_STATIC_INIT__SWDIV2AUTOMATIC
 * @see         ClkSignalPath
 *
 * @details     This array contains pointers to the frequency sources for
 *              each input of each of the multiplexers.  This array and
 *              'clkMuxValueMap_SwDiv2Automatic' are indexed by the CLK_INPUT_xxx
 *              constant macros.
 */
    ClkFreqSrc * const input[CLK_INPUT_COUNT__SWDIV2AUTOMATIC];
};


/* ------------------------ External Definitions --------------------------- */

/*!
 * @brief       Map of mux inputs to register values
 * @memberof    ClkSwDiv2Automatic
 * @see         ClkSwDiv2Automatic::input
 * @see         ClkMux::muxValueMap
 * @protected
 *
 * @details     This ine array is shared by both multiplexers in the SWDIV.
 *              See 'ClkSwDiv2Automatic::input' for a complete explanation.
 */
extern const ClkFieldValue clkMuxValueMap_SwDiv2Automatic[CLK_INPUT_COUNT__SWDIV2AUTOMATIC];


/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK3_FS_SWDIV2AUTOMATIC_H

