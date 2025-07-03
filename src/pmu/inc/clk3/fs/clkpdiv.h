/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file
 * @see     https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @see     Bug 2557340: Turn off VPLL, HUBPLL and DISPPLL based on usecase
 * @see     Perforce:  hw/doc/gpu/SOC/Clocks/Documentation/Display/Unified_Display_Clocking_Structure.vsdx
 * @author  Antone Vogt-Varvak
 */

#ifndef CLK3_FS_PDIV_H
#define CLK3_FS_PDIV_H


/* ------------------------ Includes --------------------------------------- */

#include "clk3/fs/clkwire.h"


/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */

// Same vtable as base class
typedef         ClkWire_Virtual ClkPDiv_Virtual;


/*!
 * @brief       Dynamic phase-specific information
 */
typedef struct
{
/*!
 * @brief       Divider value
 *
 * @details     LW_PVTRIM_SPPLL_PDIV_POWERDOWN indicates that the NDIV is
 *              powered down.
 *
 * @ilwariant   ( divValue => LW_PVTRIM_SPPLL_PDIV_MIN   &&
 *                divValue <= LW_PVTRIM_SPPLL_PDIV_MAX ) ||
 *                divValue == LW_PVTRIM_SPPLL_PDIV_POWERDOWN

 * @ilwariant   LW_PVTRIM_SPPLL_PDIV_POWERDOWN < LW_PVTRIM_SPPLL_PDIV_MIN ||
 *              LW_PVTRIM_SPPLL_PDIV_POWERDOWN > LW_PVTRIM_SPPLL_PDIV_MAX
 */
    LwU8        divValue;
} ClkPDivPhase;


/*!
 * @extends     ClkWire
 * @version     Clocks 3.1
 * @brief       Simple Divider
 * @protected
 *
 * @details     Starting with GA10x, PDIVs are inserted between the SPPLLs and
 *              DISPCLK/HUBCLK.  These are in addition to the PDIVs built
 *              into PLLs (which are handled by ClkPll in Clocks 3.1).
 *
 *              The clock signal passing through this divider is divided by
 *              the numerical value in the register field.  (This differs
 *              from LDIVs, for example.)
 *
 * @note        Do not use this class in a build (e.g. GH100) which also uses
 *              ClkRODiv (clkropdiv.c).  You save IMEM/DMEM if you use choose
 *              either clkpdiv.c (programmable) or clkropdiv.c (read-only),
 *              but not both.  They are mutually exclusive.
 */
typedef struct
{
/*!
 * @brief       Inherited state
 *
 * @ilwariant   Inherited state must always be first.
 */
    ClkWire     super;

/*!
 * @brief       Configuration register address
 *
 * @details     For SPPLLs, the name of this register in dev_trim is
 *              LW_PVTRIM_SYS_SPPLLn_COEFF2 where n is SPPLL number.
 */
    const LwU32 regAddr;

/*!
 * @brief       Position of divider field
 *
 * @details     For SPPLLs, the name of this field in dev_trim is
 *              LW_PVTRIM_SYS_SPPLL0_COEFF2_PDIVA or _PDIVB.
 */
    const LwU8  base;

/*!
 * @brief       Size of divider field
 *
 * @details     For SPPLLs, the name of this field in dev_trim is
 *              LW_PVTRIM_SYS_SPPLL0_COEFF2_PDIVA or _PDIVB.
 */
    const LwU8  size;

/*!
 * @brief       Dynamic phase-specific information
 */
    ClkPDivPhase phase[CLK_MAX_PHASE_COUNT];
} ClkPDiv;


/* ------------------------ External Definitions --------------------------- */

/*!
 * @brief       Virtual table
 * @memberof    ClkPDiv
 * @protected
 */
extern const ClkPDiv_Virtual clkVirtual_PDiv;


/* ------------------------ Function Prototypes ---------------------------- */

/*!
 * @brief       Implementations of virtual functions.
 * @memberof    ClkPDiv
 * @protected
 */
/**@{*/
FLCN_STATUS clkRead_PDiv(ClkPDiv *this, ClkSignal *pOutput, LwBool bActive)
    GCC_ATTRIB_SECTION("imem_libClkReadPerfDaemon", "clkRead_PDiv");
FLCN_STATUS clkConfig_PDiv(ClkPDiv *this, ClkSignal *pOutput, ClkPhaseIndex phase,
    const ClkTargetSignal *pTarget, LwBool bHotSwitch)
    GCC_ATTRIB_SECTION("imem_libClkConfig", "clkConfig_PDiv");
FLCN_STATUS clkProgram_PDiv(ClkPDiv *this, ClkPhaseIndex phase)
    GCC_ATTRIB_SECTION("imem_libClkProgram", "clkProgram_PDiv");
FLCN_STATUS clkCleanup_PDiv(ClkPDiv *this, LwBool bActive)
    GCC_ATTRIB_SECTION("imem_libClkProgram", "clkCleanup_PDiv");
#if CLK_PRINT_ENABLED
void clkPrint_PDiv(ClkPDiv *this, ClkPhaseIndex phaseCount)
    GCC_ATTRIB_SECTION("imem_libClkPrint", "clkPrint_PDiv");
#endif // CLK_PRINT_ENABLED
/**@}*/

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK3_FS_PDIV_H

