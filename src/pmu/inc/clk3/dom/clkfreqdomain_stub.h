/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file
 * @see     https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @author  Antone Vogt-Varvak
 */

#ifndef CLK3_DOM_FREQDOMAIN_STUB_H
#define CLK3_DOM_FREQDOMAIN_STUB_H

/* ------------------------ Includes --------------------------------------- */

// Superclass
#include "clk3/dom/clkfreqdomain.h"

// Frequency Source Objects
#include "clk3/fs/clkxtal.h"

/* ------------------------ Macros ----------------------------------------- */

/*!
 * @brief       Initialize clkOsm
 * @see         clkSchematicDag
 * @see         ClkOsm::input
 * @see         http://www.open-std.org/JTC1/SC22/wg14/www/docs/n1124.pdf
 *
 * @details     This macro produces the static initializer list to initialize a
 *              frequency domain that has been stubbed out.  This macro handles
 *              all fields, so no other initialization is required.
 *
 * @note        This macro functions properly only on the PMU.  Specifically,
 *              designated initializers are not supported by some MSVC versions.
 *
 * @param[in]   _NAME               Name of the domain object (e.g. pwrclk)
 * @param[in]   _FREQKHZ            Initiial frequency in KHz
 */
#define CLK_STATIC_INIT__STUB_FREQ_DOMAIN(_NAME, _FREQKHZ)                     \
    ._NAME.super.pVirtual       = &clkVirtual_StubFreqDomain,                  \
    ._NAME.target.freqKHz       = _FREQKHZ,                                    \
    ._NAME.target.path          = CLK_SIGNAL_PATH_EMPTY,                       \
    ._NAME.target.source        = LW2080_CTRL_CLK_PROG_1X_SOURCE_ILWALID,      \
    ._NAME.target.fracdiv       = LW_TRISTATE_FALSE,                           \
    ._NAME.target.regimeId      = LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID,


/* ------------------------ Datatypes -------------------------------------- */

typedef struct ClkStubFreqDomain                    ClkStubFreqDomain;
typedef        ClkProgrammableFreqDomain_Virtual    ClkStubFreqDomain_Virtual;

/*!
 * @class       ClkStubDomain
 * @extends     ClkProgrammableFreqDomain
 * @version     Clocks 3.1 and after
 * @protected
 *
 * @details     To support changes in hardware while in progress, a domain
 *              can be stubbed out using this structure.  The initial frequency
 *              is hard-coded into 'target' in the schematic dag (ClkSchematicDag).
 *              Use CLK_STATIC_INIT__STUB_FREQ_DOMAIN to set it.
 *
 *              Call to clkConfig and clkProgram never fail, but are remembered
 *              so that the signal values (frequency, etc.) are returned from
 *              subsequent clkRead calls.
 *
 * @note        The PMU's linker discards unused (i.e. unreferenced) function
 *              implementations, so code and data for this class does not
 *              consume IMEM or DMEM on builds (i.e. chips) which do not use it.
 *
 * @note        super.pRoot is not used, so it does not need to be initialzied.
 */
struct ClkStubFreqDomain
{
    ClkProgrammableFreqDomain   super;          // Result of clkProgram in status structure
    ClkSignal                   target;         // Result of clkConfig and initialization
};


/* ------------------------ External Definitions --------------------------- */

//! Per-class Virtual Table
extern const ClkStubFreqDomain_Virtual          clkVirtual_StubFreqDomain;

/* ------------------------ Function Prototypes ---------------------------- */

/*!
 * @brief       Implementations of virtual functions.
 * @memberof    ClkStubFreqDomain
 * @protected
 */
/**@{*/
FLCN_STATUS clkRead_StubFreqDomain(ClkStubFreqDomain *this)
    GCC_ATTRIB_SECTION("imem_libClkReadPerfDaemon", "clkRead_StubFreqDomain");
FLCN_STATUS clkConfig_StubFreqDomain(ClkStubFreqDomain *this, const ClkTargetSignal *pTarget)
    GCC_ATTRIB_SECTION("imem_libClkConfig", "clkConfig_StubFreqDomain");
FLCN_STATUS clkProgram_StubFreqDomain(ClkStubFreqDomain *this)
    GCC_ATTRIB_SECTION("imem_libClkProgram", "clkProgram_StubFreqDomain");
/**@}*/

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK3_DOM_FREQDOMAIN_STUB_H
