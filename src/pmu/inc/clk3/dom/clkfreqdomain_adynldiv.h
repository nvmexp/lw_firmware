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
 * @author  Chandrabhanu Mahapatra
 */

#ifndef CLK3_DOM_FREQDOMAIN_ADYNLDIV_H
#define CLK3_DOM_FREQDOMAIN_ADYNLDIV_H

/* ------------------------ Includes --------------------------------------- */

// Superclass
#include "clk3/dom/clkfreqdomain.h"

// Frequency Source Objects
#include "clk3/fs/clkadynramp.h"
#include "clk3/fs/clkldiv.h"
#include "clk3/fs/clkosm.h"


/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */

typedef struct ClkADynLdivFreqDomain                ClkADynLdivFreqDomain;
typedef        ClkProgrammableFreqDomain_Virtual    ClkADynLdivFreqDomain_Virtual;


/*!
 * @class       ClkADynLdivFreqDomain
 * @extends     ClkProgrammableFreqDomain
 * @protected
 *
 * @details     This class represents domains with a PLL, LDIV, and two OSMs,
 *              for example, DISPCLK on Turing.
 */
struct ClkADynLdivFreqDomain
{
    ClkProgrammableFreqDomain   super;          // domain
    ClkLdiv                     ldiv;           // root ldiv
    ClkADynRamp                 dyn;            // e.g. DYNRAMP for DISPPLL
    ClkOsm                      refosm;         // e.g. DISPCLK_REF
    ClkOsm                      altosm;         // e.g. DISPCLK_ALT
};


/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */

/*!
 * @brief       Implementations of virtual functions.
 * @memberof    ClkADynLdivFreqDomain
 * @protected
 */
/**@{*/
#define     clkRead_ADynLdivFreqDomain       clkRead_ProgrammableFreqDomain      // Inherit
#define     clkConfig_ADynLdivFreqDomain     clkConfig_ProgrammableFreqDomain    // Inherit
#define     clkProgram_ADynLdivFreqDomain    clkProgram_ProgrammableFreqDomain   // Inherit
#define     clkCleanup_ADynLdivFreqDomain    clkCleanup_ProgrammableFreqDomain   // Inherit
/**@}*/


/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK3_DOM_FREQDOMAIN_ADYNLDIV_H

