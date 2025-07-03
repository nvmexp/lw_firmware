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
 */

#ifndef CLK3_DOM_FREQDOMAIN_SINGLE_NAFLL_H
#define CLK3_DOM_FREQDOMAIN_SINGLE_NAFLL_H

/* ------------------------ Includes --------------------------------------- */

// Superclass
#include "clk3/dom/clkfreqdomain.h"

// Frequency Source Objects
#include "clk3/fs/clknafll.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */

typedef struct ClkSingleNafllFreqDomain             ClkSingleNafllFreqDomain;
typedef        ClkProgrammableFreqDomain_Virtual    ClkSingleNafllFreqDomain_Virtual;


/*!
 * @class       ClkNafllDomain
 * @extends     ClkProgrammableFreqDomain
 * @version     Clocks 3.1 and after
 * @protected
 *
 * @details     This class represents domains with a NAFLL object interface for
 *              NAFLL based clocks on Turing (GPC, XBAR, SYS, LWD).
 */
struct ClkSingleNafllFreqDomain
{
    /*!
     * Super class. Must always be first param.
     */
    ClkProgrammableFreqDomain   super;

    /*!
     *  Nafll freqsrc object
     */
    ClkNafll                    nafll;
};

/* ------------------------ External Definitions --------------------------- */

//! Per-class Virtual Table
extern const ClkSingleNafllFreqDomain_Virtual clkVirtual_SingleNafllFreqDomain;


/* ------------------------ Function Prototypes ---------------------------- */

/*!
 * @brief       Implementations of virtual functions.
 * @memberof    ClkSingleNafllFreqDomain
 * @protected
 */
/**@{*/
#define     clkRead_SingleNafllFreqDomain         clkRead_ProgrammableFreqDomain    // Inherit
FLCN_STATUS clkConfig_SingleNafllFreqDomain(ClkSingleNafllFreqDomain *this, const ClkTargetSignal *pTarget)
    GCC_ATTRIB_SECTION("imem_libClkConfig", "clkConfig_SingleNafllFreqDomain");
FLCN_STATUS clkProgram_SingleNafllFreqDomain(ClkSingleNafllFreqDomain *this)
    GCC_ATTRIB_SECTION("imem_libClkProgram", "clkProgram_SingleNafllFreqDomain");
#define     clkCleanup_SingleNafllFreqDomain      clkCleanup_ProgrammableFreqDomain // Inherit
/**@}*/


/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK3_DOM_FREQDOMAIN_SINGLE_NAFLL_H
