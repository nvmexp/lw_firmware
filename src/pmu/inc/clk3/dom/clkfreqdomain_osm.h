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

#ifndef CLK3_DOM_FREQDOMAIN_OSM_H
#define CLK3_DOM_FREQDOMAIN_OSM_H

/* ------------------------ Includes --------------------------------------- */

// Superclass
#include "clk3/dom/clkfreqdomain.h"

// Frequency Source Objects
#include "clk3/fs/clkosm.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */

typedef struct ClkOsmFreqDomain                     ClkOsmFreqDomain;
typedef        ClkProgrammableFreqDomain_Virtual    ClkOsmFreqDomain_Virtual;

/*!
 * @class       ClkOsmDomain
 * @extends     ClkProgrammableFreqDomain
 * @protected
 *
 * @details     This class represents domains as HUBCLK, PWRCLK and UTILSCLK
 */
struct ClkOsmFreqDomain
{
    ClkProgrammableFreqDomain   super;          // domain
    ClkOsm                      osm;            // osm freqsrc object
};


/* ------------------------ External Definitions --------------------------- */

//! Per-class Virtual Table
extern const ClkOsmFreqDomain_Virtual           clkVirtual_OsmFreqDomain;

/* ------------------------ Function Prototypes ---------------------------- */

/*!
 * @brief       Implementations of virtual functions.
 * @memberof    ClkOsmFreqDomain
 * @protected
 */
/**@{*/
#define     clkRead_OsmFreqDomain       clkRead_ProgrammableFreqDomain      // Inherit
FLCN_STATUS clkConfig_OsmFreqDomain(ClkOsmFreqDomain *this, const ClkTargetSignal *pTarget)
    GCC_ATTRIB_SECTION("imem_libClkConfig", "clkConfig_OsmFreqDomain");
#define     clkProgram_OsmFreqDomain    clkProgram_ProgrammableFreqDomain   // Inherit
#define     clkCleanup_OsmFreqDomain    clkCleanup_ProgrammableFreqDomain   // Inherit
/**@}*/

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK3_DOM_FREQDOMAIN_OSM_H
