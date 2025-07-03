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

#ifndef CLK3_DOM_FREQDOMAIN_MULTI_NAFLL_H
#define CLK3_DOM_FREQDOMAIN_MULTI_NAFLL_H

/* ------------------------ Includes --------------------------------------- */

#include "dev_trim.h"                   // Hardware manual
#include "clk3/generic_dev_trim.h"      // Addendum to hardware manual
#include "clk3/dom/clkfreqdomain.h"     // Superclass
#include "clk3/fs/clknafll.h"           // Frequency Source Objects

/* ------------------------ Macros ----------------------------------------- */

/*!
 * Size of ClkMultiNafllFreqDomain::nafll
 */
#define NAFLL_COUNT_MAX__MULTINAFLLFREQDOMAIN LW_PTRIM_GPC_GPCNAFLL_CFG1__SIZE_1

/* ------------------------ Datatypes -------------------------------------- */

typedef struct ClkMultiNafllFreqDomain              ClkMultiNafllFreqDomain;
typedef        ClkProgrammableFreqDomain_Virtual    ClkMultiNafllFreqDomain_Virtual;


/*!
 * @class       ClkNafllDomain
 * @extends     ClkProgrammableFreqDomain
 * @version     Clocks 3.1 and after
 * @protected
 *
 * @details     This class represents domains with a NAFLL object interface for
 *              NAFLL based clocks on Turing (GPC, XBAR, SYS, LWD).
 */
struct ClkMultiNafllFreqDomain
{
    /*!
     * Super class. Must always be first param.
     */
    ClkProgrammableFreqDomain super;

    /*!
     * Array of Nafll freqsrc objects
     */
    ClkNafll nafll[NAFLL_COUNT_MAX__MULTINAFLLFREQDOMAIN];
};

/* ------------------------ External Definitions --------------------------- */

//! Per-class Virtual Table
extern const ClkMultiNafllFreqDomain_Virtual clkVirtual_MultiNafllFreqDomain;


/* ------------------------ Function Prototypes ---------------------------- */

/*!
 * @brief       Implementations of virtual functions.
 * @memberof    ClkMultiNafllFreqDomain
 * @protected
 */
/**@{*/
#define     clkRead_MultiNafllFreqDomain         clkRead_ProgrammableFreqDomain     // Inherit
FLCN_STATUS clkConfig_MultiNafllFreqDomain(ClkMultiNafllFreqDomain *this, const ClkTargetSignal *pTarget)
    GCC_ATTRIB_SECTION("imem_libClkConfig", "clkConfig_MultiNafllFreqDomain");
FLCN_STATUS clkProgram_MultiNafllFreqDomain(ClkMultiNafllFreqDomain *this)
    GCC_ATTRIB_SECTION("imem_libClkProgram", "clkProgram_MultiNafllFreqDomain");
#define     clkCleanup_MultiNafllFreqDomain      clkCleanup_ProgrammableFreqDomain  // Inherit
/**@}*/

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK3_DOM_FREQDOMAIN_MULTI_NAFLL_H
