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

#ifndef CLK3_FS_READONLY_H
#define CLK3_FS_READONLY_H


/* ------------------------ Includes --------------------------------------- */

#include "clk3/fs/clkwire.h"


/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */

typedef struct ClkReadOnly          ClkReadOnly;
typedef        ClkWire_Virtual      ClkReadOnly_Virtual;

/*!
 * @extends     ClkWire
 * @brief       Cache Updater
 * @protected
 *
 * @details     Objects of this class are inserted into the schematic dag to
 *              block any attempt to program the input.  Furthermore, any
 *              reading the input is cached to prevent superfluous register I/O.
 *
 *              Objects of this class read the input only once, and remember
 *              the signal ad infinitum.  This is useful when the input is
 *              programmed by hardware and/or devinit and never changes after that.
 *
 *              This class in NOT appropriate for the case where the hardware
 *              dynamically changes the frequency.  That's typically handled
 *              by the domain.
 *
 *              A prior version of this class was useful also when the input is
 *              programmable on one signal path, but not others.  For example,
 *              'syspll' prior to Volta is programmable from the 'sysclk' domain,
 *              but read-only from all other domains.  In this case, a
 *              'ClkCacheUpdater' object would have been put in the 'sysclk' path
 *              to ilwalidate the cache in this 'ClkReadOnly' object.
 */
struct ClkReadOnly
{
/*!
 * @brief       Inherited state
 *
 * @ilwariant   Inherited state must always be first.
 */
    ClkWire     super;

/*!
 * @brief       Cached clock signal
 */
    ClkSignal   cache;
};


/* ------------------------ External Definitions --------------------------- */

/*!
 * @brief       Virtual table
 * @memberof    ClkReadOnly
 * @protected
 */
extern const ClkReadOnly_Virtual clkVirtual_ReadOnly;


/* ------------------------ Function Prototypes ---------------------------- */

/*!
 * @brief       Implementations of virtual functions.
 * @memberof    ClkReadOnly
 * @protected
 */
/**@{*/
FLCN_STATUS clkRead_ReadOnly(ClkReadOnly *this, ClkSignal *pOutput, LwBool bActive)
    GCC_ATTRIB_SECTION("imem_libClkReadPerfDaemon", "clkRead_ReadOnly");
FLCN_STATUS clkConfig_ReadOnly(ClkReadOnly *this, ClkSignal *pOutput, ClkPhaseIndex phase,
    const ClkTargetSignal *pTarget, LwBool bHotSwitch)
    GCC_ATTRIB_SECTION("imem_libClkConfig", "clkConfig_ReadOnly");
FLCN_STATUS clkProgram_ReadOnly(ClkReadOnly *this, ClkPhaseIndex phase)
    GCC_ATTRIB_SECTION("imem_libClkProgram", "clkProgram_ReadOnly");
FLCN_STATUS clkCleanup_ReadOnly(ClkReadOnly *this, LwBool bActive)
    GCC_ATTRIB_SECTION("imem_libClkProgram", "clkCleanup_ReadOnly");
#if CLK_PRINT_ENABLED
void clkPrint_ReadOnly(ClkReadOnly *this, ClkPhaseIndex phaseCount)
    GCC_ATTRIB_SECTION("imem_libClkPrint", "clkPrint_ReadOnly");
#endif // CLK_PRINT_ENABLED
/**@}*/

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK3_FS_READONLY_H
