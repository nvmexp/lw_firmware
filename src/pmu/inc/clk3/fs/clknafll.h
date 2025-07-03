/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
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


#ifndef CLK3_FS_NAFLL_H
#define CLK3_FS_NAFLL_H


/* ------------------------ Includes --------------------------------------- */

#include "clk3/fs/clkfreqsrc.h"
#include "pmu/pmuifclk.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */
typedef struct ClkNafll                 ClkNafll;
typedef        ClkFreqSrc_Virtual       ClkNafll_Virtual;


/*!
 * @class       ClkNafll
 * @extends     ClkFreqSrc
 * @version     Clocks 3.1 and after
 * @brief       NAFLL frequency source programming.
 * @protected
 *
 * @details     This class calls into NAFLL routines for programming NAFLL
 *              clock domains.
 *              https://wiki.lwpu.com/engwiki/index.php/Clocks/AVFS
 */
struct ClkNafll
{
    /*!
     * @brief       Inherited state
     *
     * @ilwariant   Inherited state must always be first.
     */
    ClkFreqSrc                                  super;

    /*!
     * The global ID @ref LW2080_CTRL_CLK_NAFLL_ID_<xyz> of this
     * NAFLL device.
     *
     * NAFLL source uses this @ref id to get the pointer to
     * @ref CLK_NAFLL_DEVICE struct containing static params
     * of NAFLL device.
     */
    const LwU8                                  id;
};



/* ------------------------ External Definitions --------------------------- */

/*!
 * @brief       Virtual table
 * @memberof    ClkNafll
 * @protected
 */
extern const ClkNafll_Virtual clkVirtual_Nafll;


/* ------------------------ Function Prototypes ---------------------------- */

/*!
 * @brief       Implementations of virtual functions.
 * @memberof    ClkNafll
 * @protected
 */
/**@{*/
FLCN_STATUS clkRead_Nafll(ClkNafll *this, ClkSignal *pOutput, LwBool bActive)
    GCC_ATTRIB_SECTION("imem_libClkReadPerfDaemon", "clkRead_Nafll");
FLCN_STATUS clkConfig_Nafll(ClkNafll *this, ClkSignal *pOutput, ClkPhaseIndex phase,
    const ClkTargetSignal *pTarget, LwBool bHotSwitch)
    GCC_ATTRIB_SECTION("imem_libClkConfig", "clkConfig_Nafll");
FLCN_STATUS clkProgram_Nafll(ClkNafll *this, ClkPhaseIndex phase)
    GCC_ATTRIB_SECTION("imem_libClkProgram", "clkProgram_Nafll");
FLCN_STATUS clkCleanup_Nafll(ClkNafll *this, LwBool bActive)
    GCC_ATTRIB_SECTION("imem_libClkProgram", "clkCleanup_Nafll");
#if CLK_PRINT_ENABLED
void clkPrint_Nafll(ClkNafll *this, ClkPhaseIndex phaseCount)
    GCC_ATTRIB_SECTION("imem_libClkPrint", "clkPrint_Nafll");
#endif // CLK_PRINT_ENABLED
/**@}*/


/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK3_FS_NAFLL_H

