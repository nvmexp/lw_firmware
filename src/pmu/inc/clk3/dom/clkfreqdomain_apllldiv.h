/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file
 * @see     https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @author  Eric Colter
 * @author  Antone Vogt-Varvak
 */

#ifndef CLK3_DOM_FREQDOMAIN_APLLLDIV_H
#define CLK3_DOM_FREQDOMAIN_APLLLDIV_H

/* ------------------------ Includes --------------------------------------- */

// Superclass
#include "clk3/dom/clkfreqdomain.h"

// Frequency Source Objects
#include "clk3/fs/clkldiv.h"
#include "clk3/fs/clkapll.h"
#include "clk3/fs/clkosm.h"


/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */

typedef struct ClkAPllLdivFreqDomain                ClkAPllLdivFreqDomain;
typedef        ClkProgrammableFreqDomain_Virtual    ClkAPllLdivFreqDomain_Virtual;


/*!
 * @class       ClkAPllLdivFreqDomain
 * @extends     ClkProgrammableFreqDomain
 * @version     Clocks 3.1 and after
 * @protected
 *
 * @details     This class represents domains with a PLL, LDIV, and two OSMs,
 *              for example, DISPCLK on Turing.
 */
struct ClkAPllLdivFreqDomain
{
    ClkProgrammableFreqDomain   super;          // domain
    ClkLdiv                     ldiv;           // root ldiv
    ClkAPll                     pll;            // e.g. DISPPLL or VPLL
    ClkOsm                      refosm;         // e.g. DISPCLK_REF
    ClkOsm                      altosm;         // e.g. DISPCLK_ALT
};


/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */

/*!
 * @brief       Implementations of virtual functions.
 * @memberof    ClkAPllLdivFreqDomain
 * @protected
 */
/**@{*/
#define     clkRead_APllLdivFreqDomain       clkRead_ProgrammableFreqDomain      // Inherit
#define     clkConfig_APllLdivFreqDomain     clkConfig_ProgrammableFreqDomain    // Inherit
#define     clkProgram_APllLdivFreqDomain    clkProgram_ProgrammableFreqDomain   // Inherit
#define     clkCleanup_APllLdivFreqDomain    clkCleanup_ProgrammableFreqDomain   // Inherit
/**@}*/


/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK3_DOM_FREQDOMAIN_APLLLDIV_H

