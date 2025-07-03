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
 * @see     Perforce: //hw/libs/common/analog/lwpu/doc/HPLL16G_SSD_DYN_B1.doc
 * @see     Jira: RMGAX-963 HUBPLL/DISPPLL Dynamic Ramp Support GA10x
 * @author  Antone Vogt-Varvak
 */


#ifndef CLK3_FS_HPLL_H
#define CLK3_FS_HPLL_H

/* ------------------------ Includes --------------------------------------- */

#include "clk3/fs/clkwire.h"
#include "pmu/pmuifclk.h"


/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */

typedef ClkWire_Virtual ClkHPll_Virtual;


/*!
 * @brief       Per-Phase Status for Hybrid Phase Locked Loop
 * @version     Clocks 3.1 and after
 *
 * @details     Dynamic state for PLLs that differs for each phase.
 *
 *              Only functions belonging to the class and subclasses may access
 *              or modify anything in this struct.
 *
 *              This class models the simple PLL with MDIV, NDIV, and PLDIV
 *              coefficients.
 *
 *              The coefficients are each zero if and only if the PLL is
 *              disabled. The caveat is, however, that the software object in
 *              phase zero shows the PLL as disabled if the hardware is in an
 *              invalid state. The software object never has an invalid
 *              state, per se.
 */
typedef struct
{
/*!
 * @brief       Is the PLL running during programming?
 */
    LwBool      bHotSwitch;

/*!
 * @brief       Value to program the MDIV.
 *
 * @details     The value of the MDIV is computed by clkRead/clkConfig.
 *              LW_PTRIM_SYS_PLL_COEFF_MDIV is the generic name for this field.
 *
 *              This member is zero to indicate a disabled PLL.
 */
    LwU8        mdiv;

/*!
 * @brief       Integer portion of value to program the NDIV.
 *
 * @details     The value of the integer portion of NDIV is computed by clkRead/clkConfig.
 *              LW_PTRIM_SYS_HPLL_CFG4_NDIV is the generic name for this field.
 *
 *              This member is zero if the PLL is disabled.
 */
    LwU8        ndiv;

/*!
 * @brief       Value to program the PLDIV.
 *
 * @details     The value of the PLDIV is computed by clkRead/clkConfig.
 *              LW_PTRIM_SYS_PLL_COEFF_PLDIV is the generic name for this field.
 *
 *              This member is zero if the PLL or the PLDIV is disabled.
 */
    LwU8        pldiv;

} ClkHPllPhase;


/*!
 * @extends     ClkWire
 * @brief       Hybrid Phase Locked Loop
 * @version     Clocks 3.1 and after
 * @protected
 *
 * @details     Objects of this class represent a phase-locked loop in the hardware.
 */
typedef struct
{
/*!
 * @brief       Inherited state
 *
 * @details     ClkHPll inherits from ClkWire since it is the class that handles
 *              objects with exactly one potential input, which is useful to
 *              separate out the lone-potential-input logic (e.g. daisy-chaining
 *              along the schematic dag) from the PLL-specific logic.
 */
    ClkWire     super;

/*!
 * @brief       Signal source per @ref LW2080_CTRL_CLK_PROG_1X_SOURCE_<XYZ>
 *
 * @details     ClkSignal::source is set to this value to indicate which PLL
 *              this is.  Typical values for this class are:
 *              LW2080_CTRL_CLK_PROG_1X_SOURCE_PLL for the domain-specific PLL
 *              LW2080_CTRL_CLK_PROG_1X_SOURCE_SPPLL0
 *              LW2080_CTRL_CLK_PROG_1X_SOURCE_SPPLL1
 */
    const LwU8  source;

/*!
 * @brief       VBIOS PLL Info Table entry
 * @see         Bug 2557340/48
 * @see         Sec 5 in hw/libs/common/analog/lwpu/doc/HPLL16G_SSD_DYN_B1.doc
 *
 * @details     The PLDIV is programmed to this value.
 */
    const LwU8 pdivTarget;

/*!
 * @brief       Configuration register
 *
 * @details     The name of this register in the manuals is LW_PVTRIM_SYS_xxx_CFG.
 *              where xxx is the name of the PLL (with some exceptions).
 *
 *              The generic name for the register is LW_PTRIM_SYS_PLL_CFG.
 */
    const LwU32 cfgRegAddr;

/*!
 * @brief       Coefficient register
 *
 * @details     The name of this register in the manuals is LW_PVTRIM_SYS_xxx_COEFF.
 *              where xxx is the name of the PLL (with some exceptions).
 *
 *              The generic name for the register is LW_PTRIM_SYS_PLL_COEFF.
 */
    const LwU32 coeffRegAddr;

/*!
 * @brief       Second coefficient register
 *
 * @details     The name of this register in the manuals is LW_PVTRIM_SYS_xxx_COEFF2.
 *              where xxx is the name of the PLL (with some exceptions).
 *
 *              The generic name for the register is LW_PTRIM_SYS_HPLL_CFG2.
 */
    const LwU32 cfg2RegAddr;

/*!
 * @brief       CFG4 register for Hybrid PLLs (HPLL)
 * @see         Bug 2440060: GA10X: Moving PLLs from tsmc16ffb to tsmc7 cells
 * @see         SW Bug 2538373: Moving NDIV_INT controls into CFG4 register for DISPPLL, HUBPLL & VPLL0-3
 * @see         HW Bug 200497311: Moving NDIV_INT controls into CFG4 register for DISPPLL, HUBPLL & VPLL0-3
 *
 * @details     Starting with Ampere, many PLLs are hybrid (e.g.tsmc7), which
 *              do not have all of the same registers and fields.  In particular
 *
 *              The generic name for the register is LW_PTRIM_SYS_HPLL_CFG4.
 *              the _NDIV fields has moved and works differently.
 */
    const LwU32 cfg4RegAddr;

/*!
 * @brief       Per-phase Dynamic State
 */
    ClkHPllPhase phase[CLK_MAX_PHASE_COUNT];

} ClkHPll;


/* ------------------------ External Definitions --------------------------- */
/*!
 * @brief       Virtual table
 * @memberof    ClkHPll
 * @protected
 */
extern const ClkHPll_Virtual clkVirtual_HPll;


/* ------------------------ Function Prototypes ---------------------------- */

/*!
 * @brief       Implementations of virtual functions.
 * @memberof    ClkHPll
 * @protected
 */
/**@{*/
FLCN_STATUS clkRead_HPll(ClkHPll *this, ClkSignal *pOutput, LwBool bActive)
    GCC_ATTRIB_SECTION("imem_libClkReadPerfDaemon", "clkRead_HPll");
FLCN_STATUS clkConfig_HPll(ClkHPll *this, ClkSignal *pOutput, ClkPhaseIndex phase,
    const ClkTargetSignal *pTarget, LwBool bHotSwitch)
    GCC_ATTRIB_SECTION("imem_libClkConfig", "clkConfig_HPll");
FLCN_STATUS clkProgram_HPll(ClkHPll *this, ClkPhaseIndex phase)
    GCC_ATTRIB_SECTION("imem_libClkProgram", "clkProgram_HPll");
FLCN_STATUS clkCleanup_HPll(ClkHPll *this, LwBool bActive)
    GCC_ATTRIB_SECTION("imem_libClkProgram", "clkCleanup_HPll");
#if CLK_PRINT_ENABLED
void clkPrint_HPll(ClkHPll *this, ClkPhaseIndex phaseCount)
    GCC_ATTRIB_SECTION("imem_libClkPrint", "clkPrint_HPll");
#endif // CLK_PRINT_ENABLED
/**@}*/


/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK3_FS_HPLL_H

