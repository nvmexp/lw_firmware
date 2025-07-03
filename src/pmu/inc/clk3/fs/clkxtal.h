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


#ifndef CLK3_FS_XTAL_H
#define CLK3_FS_XTAL_H


/* ------------------------ Includes --------------------------------------- */

#include "clk3/fs/clkfreqsrc.h"


/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */

typedef struct ClkXtal              ClkXtal;
typedef        ClkFreqSrc_Virtual   ClkXtal_Virtual;


/*!
 * @class       ClkXtal
 * @extends     ClkFreqSrc
 * @brief       Zero-Input Frequency Source
 * @protected
 *
 * @details     Objects of this class represent crystals, but can also be used
 *              any place where we want a hard-coded frequency to be reported.
 *
 *              Furthermore, this class can be extended for any frequency source
 *              that takes no input clock signals.
 *
 * @todo        The xtal may have different frequencies depending on the SKU.
 *              This should be determined at runtime by checking the
 *              LW_PEXTDEV_BOOT_0_STRAP_CRYSTAL register, and then setting the
 *              ClkXtal's final pointer to point to a ClkXtal with the correct
 *              frequency.
 */
struct ClkXtal
{
/*!
 * @brief       Inherited state
 *
 * @ilwariant   Inherited state must always be first.
 */
    ClkFreqSrc  super;

/*!
* @brief        Crystal oscillation frequency in KHz
*/
    const LwU32 freqKHz;
};



/* ------------------------ External Definitions --------------------------- */

/*!
 * @brief       Virtual table
 * @memberof    ClkXtal
 * @protected
 */
extern const ClkXtal_Virtual clkVirtual_Xtal;


/* ------------------------ Function Prototypes ---------------------------- */

/*!
 * @brief       Implementations of virtual functions.
 * @memberof    ClkXtal
 * @protected
 */
/**@{*/
FLCN_STATUS clkRead_Xtal(ClkXtal *this, ClkSignal *pOutput, LwBool bActive)
    GCC_ATTRIB_SECTION("imem_libClkReadPerfDaemon", "clkRead_Xtal");
FLCN_STATUS clkConfig_Xtal(ClkXtal *this, ClkSignal *pOutput, ClkPhaseIndex phase,
    const ClkTargetSignal *pTarget, LwBool bHotSwitch)
    GCC_ATTRIB_SECTION("imem_libClkConfig", "clkConfig_Xtal");
FLCN_STATUS clkProgram_Xtal(ClkXtal *this, ClkPhaseIndex phase)
    GCC_ATTRIB_SECTION("imem_libClkProgram", "clkProgram_Xtal");
FLCN_STATUS clkCleanup_Xtal(ClkXtal *this, LwBool bActive)
    GCC_ATTRIB_SECTION("imem_libClkProgram", "clkCleanup_Xtal");
#if CLK_PRINT_ENABLED
void clkPrint_Xtal(ClkXtal *this, ClkPhaseIndex phaseCount)
    GCC_ATTRIB_SECTION("imem_libClkPrint", "clkPrint_Xtal");
#endif // CLK_PRINT_ENABLED
/**@}*/


/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK3_FS_XTAL_H

