/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2021 by LWPU Corporation.  All rights reserved.  All
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


#ifndef CLK3_FS_WIRE_H
#define CLK3_FS_WIRE_H


/* ------------------------ Includes --------------------------------------- */

#include "clk3/fs/clkfreqsrc.h"


/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */

typedef struct ClkWire              ClkWire;
typedef        ClkFreqSrc_Virtual   ClkWire_Virtual;


/*!
 * @extends     ClkFreqSrc
 * @brief       Single-Input Frequency Source
 * @protected
 *
 * @details     Objects of this class do nothing except daisy-chain.  The output
 *              signal always equals the output of its input.  It is called
 *              ClkWire as a metaphor because wires don't do anything to the
 *              signal in hardware (we hope).
 *
 * @note        abstract:  This class does not have a vtable if its own.
 */
struct ClkWire
{
/*!
 * @brief       Inherited state
 *
 * @ilwariant   Inherited state must always be first.
 */
    ClkFreqSrc  super;

/*!
 * @brief       Input node per the schematic dag
 *
 * @note        init state: The value for this member is set during initialization
 *              and does not change after that.
 */
    ClkFreqSrc * const pInput;
};


/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */

/*!
 * @brief       Implementations of virtual functions.
 * @memberof    ClkWire
 * @protected

 */
/**@{*/
FLCN_STATUS clkRead_Wire(ClkWire *this, ClkSignal *pOutput, LwBool bActive)
    GCC_ATTRIB_SECTION("imem_libClkReadPerfDaemon", "clkRead_Wire");
FLCN_STATUS clkConfig_Wire(ClkWire *this, ClkSignal *pOutput, ClkPhaseIndex phase,
    const ClkTargetSignal *pTarget, LwBool bHotSwitch)
    GCC_ATTRIB_SECTION("imem_libClkConfig", "clkConfig_Wire");
FLCN_STATUS clkProgram_Wire(ClkWire *this, ClkPhaseIndex phase)
    GCC_ATTRIB_SECTION("imem_libClkProgram", "clkProgram_Wire");
FLCN_STATUS clkCleanup_Wire(ClkWire *this, LwBool bActive)
    GCC_ATTRIB_SECTION("imem_libClkProgram", "clkCleanup_Wire");
#if CLK_PRINT_ENABLED
void clkPrint_Wire(ClkWire *this, ClkPhaseIndex phaseCount)
    GCC_ATTRIB_SECTION("imem_libClkPrint", "clkPrint_Wire");
#endif // CLK_PRINT_ENABLED
/**@}*/


/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK3_FS_WIRE_H

