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

#ifndef CLK3_FS_MULTIPLIER_H
#define CLK3_FS_MULTIPLIER_H


/* ------------------------ Includes --------------------------------------- */

#include "clk3/fs/clkwire.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */

typedef struct ClkMultiplier        ClkMultiplier;
typedef        ClkWire_Virtual      ClkMultiplier_Virtual;


/*!
 * @extends     ClkWire
 * @brief       Fixed Multiplier
 * @version     Clocks 3.1 and after
 * @protected
 *
 * @details     Objects of this class multiply the input signal by a fixed amount
 *              determined at or before initialization.
 *
 * @note        This class does not have its own implementation of clkProgram or
 *              or clkCleanup, but instead uses the implementation of the super
 *              class, 'clkWire'.
 */
struct ClkMultiplier
{
/*!
 * @brief       Inherited state
 * @memberof    ClkMultiplier
 * @protected
 *
 * @ilwariant   Inherited state must always be first.
 */
    ClkWire     super;

/*!
* @brief        The value to multiply the clock frequency by.
*/
    const LwU8 multiplier;
};



/* ------------------------ External Definitions --------------------------- */

/*!
 * @brief       Virtual table
 * @memberof    ClkMultiplier
 * @protected
 */
extern const ClkMultiplier_Virtual clkVirtual_Multiplier;


/* ------------------------ Function Prototypes ---------------------------- */

/*!
 * @brief       Implementations of virtual functions.
 * @memberof    ClkMultiplier
 * @protected
 */
/**@{*/
FLCN_STATUS clkRead_Multiplier(ClkMultiplier *this, ClkSignal *pOutput, LwBool bActive)
    GCC_ATTRIB_SECTION("imem_libClkReadPerfDaemon", "clkRead_Multiplier");
FLCN_STATUS clkConfig_Multiplier(ClkMultiplier *this, ClkSignal *pOutput, ClkPhaseIndex phase,
    const ClkTargetSignal *pTarget, LwBool bHotSwitch);
#define     clkProgram_Multiplier clkProgram_Wire
#define     clkCleanup_Multiplier clkCleanup_Wire
#if CLK_PRINT_ENABLED
void clkPrint_Multiplier(ClkMultiplier *this, ClkPhaseIndex phaseCount)
    GCC_ATTRIB_SECTION("imem_libClkPrint", "clkPrint_Multiplier");
#endif // CLK_PRINT_ENABLED
/**@}*/


/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK3_FS_MULTIPLIER_H

