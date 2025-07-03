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
 * @file    Manages handling of MclkFreqDomain
 * @see     https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @author  Chandrabhanu Mahapatra
 */
#ifndef CLK3_DOM_FREQDOMAIN_MCLK_H
#define CLK3_DOM_FREQDOMAIN_MCLK_H


#if PMUCFG_FEATURE_ENABLED(PMU_CLK_MCLK_DDR_SUPPORTED) && PMUCFG_FEATURE_ENABLED(PMU_CLK_MCLK_HBM_SUPPORTED)
#error This build does not support more than one memory type.
#endif

#if ! PMUCFG_FEATURE_ENABLED(PMU_CLK_MCLK_DDR_SUPPORTED) && ! PMUCFG_FEATURE_ENABLED(PMU_CLK_MCLK_HBM_SUPPORTED)
#error Required: At least one PMUCFG feature to specify the supported memory type(s)
#endif

/* ------------------------ Includes --------------------------------------- */

// Superclass
#include "clk3/dom/clkfreqdomain.h"

// Frequency Source Objects
#include "clk3/fs/clkmux.h"


/* ------------------------ Macros ----------------------------------------- */

/*!
 * @brief       Number of inputs for the main multiplexer
 * @memberof    ClkMclkFreqDomain
 * @see         ClkMclkFreqDomain::input
 * @protected
 *
 * @details     See 'ClkMclkFreqDomain::input' for a complete explanation.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_MCLK_DDR_SUPPORTED)
#define CLK_INPUT_COUNT__MCLK   3
#elif PMUCFG_FEATURE_ENABLED(PMU_CLK_MCLK_HBM_SUPPORTED)
#define CLK_INPUT_COUNT__MCLK   2
#endif


/*!
 * @brief       Index using generic names
 * @memberof    ClkMclkFreqDomain
 * @see         ClkMclkFreqDomain::input
 * @see         clkMuxValueMap_MclkFreqDomain
 * @public
 *
 * @details     These indices are used to index both the input array
 *              'ClkMclkFreqDomain::input' and 'clkMuxValueMap_MclkFreqDomain'.
 *
 *              The names of these symbols reflect the generic name of the
 *              input into the OSM rather than the functional name.
 *              These names are more-or-less based on the register field
 *              names as defined in the manuals.
 */
/**@{*/
#define CLK_INPUT_BYPASS__MCLK          0x0U    // Bypass all PLLs to either OSM or SWDIV
#define CLK_INPUT_HBMPLL__MCLK          0x1U    // HBM only
#define CLK_INPUT_REFMPLL__MCLK         0x1U    // DDR only
#define CLK_INPUT_DRAMPLL__MCLK         0x2U    // DDR only
/**@}*/


/* ------------------------ Datatypes -------------------------------------- */

typedef struct ClkMclkFreqDomain                    ClkMclkFreqDomain;

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_PROGRAMMABLE_MCLK_SUPPORTED))
typedef        ClkProgrammableFreqDomain_Virtual    ClkMclkFreqDomain_Virtual;
#else
typedef        ClkFixedFreqDomain_Virtual           ClkMclkFreqDomain_Virtual;
#endif

/*!
 * @class       ClkMclkFreqDomain
 * @extends     ClkProgrammableFreqDomain or ClkFixedFreqDomain
 * @protected
 *
 * @details     This class represents MClk domain.
 */
struct ClkMclkFreqDomain
{
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_PROGRAMMABLE_MCLK_SUPPORTED)
    ClkProgrammableFreqDomain       super;                          // domain
#else
    ClkFixedFreqDomain              super;                          // domain
#endif

    ClkMux                          mux;                            // Main multiplexer
    ClkFreqSrc                     *input[CLK_INPUT_COUNT__MCLK];   // Main mux inputs

    LwU32                           targetFreqKHz;                  // Target Frequency in KHz
    LwU32                           fbStopTimeUs;                   // FB STOP time during MClk switch
};


/* ------------------------ External Definitions --------------------------- */

//! Per-class Virtual Table
extern const ClkMclkFreqDomain_Virtual clkVirtual_MclkFreqDomain;

/*!
 * @brief       Map of main mux inputs to register values
 * @memberof    ClkMclkFreqDomain
 * @see         ClkMclkFreqDomain::input
 * @see         ClkMux::muxValueMap
 * @protected
 */
extern const ClkFieldValue clkMuxValueMap_MclkFreqDomain[CLK_INPUT_COUNT__MCLK];


/* ------------------------ Function Prototypes ---------------------------- */

/*!
 * @brief       Implementations of virtual functions.
 * @memberof    ClkMclkFreqDomain
 * @protected
 * @details     MclkFreqDomain class is used for reading back register coefficients
 *              and sanity checking target MCLK frequency before sending it to
 *              fbfalcon for programming. As such cleanup is never called here
 *              and thus been assgined to NULL.
 */
/**@{*/
#ifndef CLK_SD_CHECK        // See pmu_sw/prod_app/clk/clk3/clksdcheck.c

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_PROGRAMMABLE_MCLK_SUPPORTED))
#define     clkRead_MclkFreqDomain         clkRead_ProgrammableFreqDomain       // Inherit
FLCN_STATUS clkConfig_MclkFreqDomain(ClkMclkFreqDomain *this, const ClkTargetSignal *pTarget)
    GCC_ATTRIB_SECTION("imem_libClkConfig", "clkConfig_MclkFreqDomain");
FLCN_STATUS clkProgram_MclkFreqDomain(ClkMclkFreqDomain *this)
    GCC_ATTRIB_SECTION("imem_libClkProgram", "clkProgram_MclkFreqDomain");

#else                       // PMU_CLK_PROGRAMMABLE_MCLK_SUPPORTED
#define     clkRead_MclkFreqDomain         clkRead_FixedFreqDomain      // Inherit
#define     clkConfig_MclkFreqDomain       clkConfig_FixedFreqDomain    // Inherit
#define     clkProgram_MclkFreqDomain      clkProgram_FixedFreqDomain   // Inherit

#endif                      // PMU_CLK_PROGRAMMABLE_MCLK_SUPPORTED
#endif                      // CLK_SD_CHECK
/**@}*/


/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK3_DOM_FREQDOMAIN_MCLK_H

