/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or dis  closure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file
 * @brief       SPPLL
 * @version     Clocks 3.1 and after
 * @see         https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @author      Chandrabhanu Mahapatra
 */

#ifndef CLK3_FS_SPPLL_H
#define CLK3_FS_SPPLL_H


/* ------------------------ Includes --------------------------------------- */

#include "clk3/fs/clkreadonly.h"
#include "clk3/fs/clkmux.h"
#include "clk3/fs/clkapll.h"
#include "clk3/clkfieldvalue.h"


/* ------------------------ Macros ----------------------------------------- */

/*!
 * @brief       Number of inputs for the SPPLL multiplexer
 * @memberof    sppll0/sppll1
 * @see         sppll0::input/sppll1::input
 * @protected
 *
 * @details     See 'sppll0::input/sppll1::input' for a complete explanation.
 */
#define CLK_INPUT_COUNT__SPPLL      2

/*!
 * @brief       Index using generic names
 * @memberof    sppll0/sppll1
 * @see         sppll0::input/sppll1::input
 * @see         clkMuxValueMap_Sppll0/clkMuxValueMap_Sppll1
 * @public
 *
 * @details     These indices are used to index both the input array
 *              'sppll0::input/sppll1::input' and 'clkMuxValueMap_Sppll0/clkMuxValueMap_Sppll1'.
 *
 *              These names are more-or-less based on the register field
 *              names as defined in the manuals.
 */
/**@{*/
#define CLK_INPUT_VCO__SPPLL        0x0
#define CLK_INPUT_BYPASSCLK__SPPLL  0x1
/**@}*/

/* ------------------------ Datatypes -------------------------------------- */

typedef struct ClkSppll ClkSppll;


/*!
 * @brief       SPPLL
 * @version     Clocks 3.1 and after
 * @protected
 *
 * @details     This struct is not a class, but merely composites of ClkFreqSrc-
 *              derived objects, specifically, one mulitplexer and one PLL unit.
 *
 *              It does not extend any other class nor does it contain member
 *              functions nor vtable.  It merely contains 'ClkFreqSrc' objects
 *              and a macro to properly initialize them.
 */
struct ClkSppll
{
/*!
 * @brief       This serves as the root of the SPPLL
 */
    ClkReadOnly readonly;

/*!
 * @brief       This mux helps to select between BYPASS and PLL path
 */
    ClkMux      mux;

/*!
 * @brief       Inputs to each multiplixer
 * @see         ClkMux::muxValueMap
 * @see         clkMuxValueMap_Sppll
 * @see         ClkSignalPath
 *
 * @details     This array contains pointers to the frequency sources for
 *              each input of each of the multiplexers.  This array and
 *              'clkMuxValueMap_Sppll' are indexed by the CLK_INPUT_xxx
 *              constant macros.
 */
    ClkFreqSrc * const input[CLK_INPUT_COUNT__SPPLL];

/*!
 * @brief       PLL object initialized for PLL path
 */
    ClkAPll     pll;
};


/* ------------------------ External Definitions --------------------------- */

/*!
 * @brief       Map of mux inputs to register values
 * @memberof    ClkSppll
 * @see         ClkSppll::input
 * @see         ClkMux::muxValueMap
 * @protected
 */
/**@{*/
extern const ClkFieldValue clkMuxValueMap_Sppll0[CLK_INPUT_COUNT__SPPLL];
extern const ClkFieldValue clkMuxValueMap_Sppll1[CLK_INPUT_COUNT__SPPLL];
/**@}*/

/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Include Derived Types -------------------------- */

#endif //CLK3_FS_SPPLL_H

