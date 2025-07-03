/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file
 * @see     https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @author  Chandrabhanu Mahapatra
 */

#ifndef CLK3_DOM_FREQDOMAIN_BIF_H
#define CLK3_DOM_FREQDOMAIN_BIF_H

/* ------------------------ Includes --------------------------------------- */

// Superclass
#include "clk3/dom/clkfreqdomain.h"

// Frequency Source Objects
#include "clk3/fs/clkbif.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */

typedef struct ClkBifFreqDomain                     ClkBifFreqDomain;
typedef        ClkProgrammableFreqDomain_Virtual    ClkBifFreqDomain_Virtual;


/*!
 * @class       ClkBifDomain
 * @extends     ClkProgrammableFreqDomain
 * @protected
 *
 * @details     This class represents domains with a BIF object interface for
 *              PCIEGENCLK on Turing.
 */
struct ClkBifFreqDomain
{
    ClkProgrammableFreqDomain   super;          // domain
    ClkBif                      bif;            // bif freqsrc object
};


/* ------------------------ External Definitions --------------------------- */

//! Per-class Virtual Table
extern const ClkBifFreqDomain_Virtual clkVirtual_BifFreqDomain;


/* ------------------------ Function Prototypes ---------------------------- */

/*!
 * @brief       Implementations of virtual functions.
 * @memberof    ClkBifFreqDomain
 * @protected
 */
/**@{*/
#define     clkRead_BifFreqDomain       clkRead_ProgrammableFreqDomain      // Inherit
FLCN_STATUS clkConfig_BifFreqDomain(ClkBifFreqDomain *this, const ClkTargetSignal *pTarget)
    GCC_ATTRIB_SECTION("imem_libClkConfig", "clkConfig_BifFreqDomain");
#define     clkProgram_BifFreqDomain    clkProgram_ProgrammableFreqDomain   // Inherit
#define     clkCleanup_BifFreqDomain    clkCleanup_ProgrammableFreqDomain   // Inherit
/**@}*/


/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK3_DOM_FREQDOMAIN_BIF_H
