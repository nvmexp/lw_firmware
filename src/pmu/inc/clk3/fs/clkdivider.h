/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
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
 * @author  Onkar Bimalkhedkar
 */

#ifndef CLK3_FS_DIVIDER_H
#define CLK3_FS_DIVIDER_H


/* ------------------------ Includes --------------------------------------- */

#include "clk3/fs/clkwire.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */

typedef struct ClkDivider           ClkDivider;
typedef        ClkWire_Virtual      ClkDivider_Virtual;


/*!
 * @extends     ClkWire
 * @brief       Fixed Divider
 * @version     Clocks 3.1 and after
 * @protected
 *
 * @details     Objects of this class divide the input signal by a fixed amount
 *              determined at or before initialization.
 *
 * @note        This class does not have its own implementation of clkProgram or
 *              or clkCleanup, but instead uses the implementation of the super
 *              class, 'clkWire'.
 */
struct ClkDivider
{
/*!
 * @brief       Inherited state
 * @memberof    ClkDivider
 * @protected
 *
 * @ilwariant   Inherited state must always be first.
 */
    ClkWire     super;

/*!
* @brief        The value to divide the clock frequency by.
*/
    const LwU8  divider;
};



/* ------------------------ External Definitions --------------------------- */

/*!
 * @brief       Virtual table
 * @memberof    ClkDivider
 * @protected
 */
extern const ClkDivider_Virtual clkVirtual_Divider;


/* ------------------------ Function Prototypes ---------------------------- */

/*!
 * @brief       Implementations of virtual functions.
 * @memberof    ClkDivider
 * @protected
 */
/**@{*/
FLCN_STATUS clkRead_Divider(ClkDivider *this, ClkSignal *pOutput, LwBool bActive)
    GCC_ATTRIB_SECTION("imem_libClkReadPerfDaemon", "clkRead_Divider");
FLCN_STATUS clkConfig_Divider(ClkDivider *this, ClkSignal *pOutput, ClkPhaseIndex phase,
    const ClkTargetSignal *pTarget, LwBool bHotSwitch);
#define     clkProgram_Divider clkProgram_Wire
#define     clkCleanup_Divider clkCleanup_Wire
#if CLK_PRINT_ENABLED
void clkPrint_Divider(ClkDivider *this, ClkPhaseIndex phaseCount)
    GCC_ATTRIB_SECTION("imem_libClkPrint", "clkPrint_Divider");
#endif // CLK_PRINT_ENABLED
/**@}*/


/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK3_FS_DIVIDER_H

