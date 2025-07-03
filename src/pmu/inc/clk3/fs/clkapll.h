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
 * @see     //hw/libs/common/analog/lwpu/doc/
 * @see     //hw/doc/gpu/volta/volta/physical/clocks/Volta_clocks_block_diagram.vsd
 * @author  Eric Colter
 * @author  Antone Vogt-Varvak
 * @author  Ming Zhong
 * @author  Prafull Gupta
 */


#ifndef CLK3_FS_APLL_H
#define CLK3_FS_APLL_H

/* ------------------------ Includes --------------------------------------- */

#include "clk3/fs/clkwire.h"
#include "pmu/pmuifclk.h"


/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */

typedef ClkWire_Virtual ClkAPll_Virtual;


/*!
 * @brief       Per-Phase Status for Phase Locked Loop
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
 * @brief       Value to program the MDIV.
 *
 * @details     The value of the MDIV is computed by clkRead/clkConfig.
 *              LW_PTRIM_SYS_PLL_COEFF_MDIV is the generic name for this field.
 *
 *              This member is zero it indicate a disabled PLL.
 */
    LwU8        mdiv;

/*!
 * @brief       Value to program the NDIV.
 *
 * @details     The value of the NDIV is computed by clkRead/clkConfig.
 *              LW_PTRIM_SYS_PLL_COEFF_NDIV is the generic name for this field.
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

/*!
 * @brief       Whether or not the pll is in use this phase
 *
 * @details     bActive is true if and only if this phase is configured so that
 *              at the end of programming it, this pll is part of the active path
 *              of the clock signal for its domain.
 *
 *              It is NOT set if the pll is not part of the active path at
 *              the end of programming this phase.  For example imagine a
 *              scenario in which during phase 0, the pll is part of the
 *              active path, and during phase 1 a multiplexer is switched so
 *              the pll is no longer a part of the active path.  In the pll's
 *              phase array, bActive will be set for phase 0.  Although at
 *              the beginning of phase 1, the pll is still part of the active
 *              path, it is no longer part of the active path at the end of
 *              phase 1, so bActive will be false for phase 1.
 *
 *              This property is also unique among phase arrays because when
 *              it is set, it does not cascade backwards to following phases.
 *              For example if bActive is set to true for phase n, then it is
 *              NOT automatically set to true for phases greater than n.
 *              This is necessary because clkConfig is only called for
 *              objects in the active path, so if this pll is no longer part
 *              of the active path, there won't be a config call to set
 *              bActive to false.
 *
 *              This property also behaves differently when it comes to
 *              clkRead and clkCleanup.  ClkRead normally sets all phases of
 *              the phase array to match the hardware state.  For this
 *              property clkRead sets only the first phase to match the
 *              hardware state and all of the others are set to false.
 *              Similaryly, clkCleanup sets this property to false for all
 *              phases except phase 0 which retains its value.
 */
    LwBool      bActive;

/*!
* @brief        Input frequency of this phase
*
* @details      The input frequency needs to be stored so that ClkDynRamp
*               can do its callwlations without rounding error.
*/
    LwU32       inputFreqKHz;
} ClkAPllPhase;


/*!
 * @extends     ClkWire
 * @brief       Phase Locked Loop
 * @version     Clocks 3.1 and after
 * @protected
 *
 * @details     Objects of this class represtent a phase-locked loop in the hardware.
 */
typedef struct
{
/*!
 * @brief       Inherited state
 *
 * @details     ClkAPll inherits from ClkWire since it is the class that handles
 *              objects with exactly one potential input, which is useful to
 *              separate out the lone-potential-input logic (e.g. daisy-chaining
 *              along the schematic dag) from the PLL-specific logic.
 */
    ClkWire     super;

/*!
 * @brief       Number of retries for PLL to become ready/locked
 * @see         Bug 1338203
 *
 * @details     Counts the number of times the PLL was power-cycled due to timeout
 *              in this phase.  'clkProgram' does not return an error until this
 *              count reaches a predefined maximum.
 */
    LwU8        retryCount;

/*!
 * @brief       Per-phase Dynamic State
 */
    ClkAPllPhase
                phase[CLK_MAX_PHASE_COUNT];

/*!
 * @brief       VBIOS PLL Info Table entry
 * @see         clkSchematicDag
 * @see         http://www.open-std.org/JTC1/SC22/wg14/www/docs/n1124.pdf
 *
 * @details     This member may be NULL for read-only PLLs since it is used
 *              only in clkConfig.
 *
 *              As of Turing, there is no support for the VBIOS copy of the PLL
 *              Info Table.  The values are hard-coded into the schematic.
 */
    const RM_PMU_CLK_PLL_DEVICE_BOARDOBJ_SET * const pPllInfoTableEntry;

/*!
 * @brief       Configuration register
 *
 * @details     The name of this register in the manuals is LW_PVTRIM_SYS_xxx_CFG.
 *              where xxx is the name of the PLL (with some exceptions).
 *
 *              This member can not be 'const' because there is floorsweeping
 *              logic for GA100 in 'clkConstruct_SchematicDag'.  However, its
 *              values does not change after construction.
 *
 *              The generic name for the register is LW_PTRIM_SYS_PLL_CFG.
 */
    LwU32       cfgRegAddr;

/*!
 * @brief       Coefficient register
 *
 * @details     The name of this register in the manuals is LW_PVTRIM_SYS_xxx_COEFF.
 *              where xxx is the name of the PLL (with some exceptions).
 *
 *              This member can not be 'const' because there is floorsweeping
 *              logic for GA100 in 'clkConstruct_SchematicDag'.  However, its
 *              values does not change after construction.
 *
 *              The generic name for the register is LW_PTRIM_SYS_PLL_COEFF.
 */
    LwU32       coeffRegAddr;

/*!
 * @brief       PLL settle time in nanoseconds
 *
 * @details     This value is typically 64000ns.  Software waits this long for
 *              the PLL to become locked and times out if it doesn't.
 */
   LwU32 settleNs;

/*!
 * @brief       PLL time between steps of ndiv slides
 *
 * @details     When being programmed hot, the pll holds the mdiv and pldiv
 *              constant while the ndiv changes one step at a time.  This is
 *              the amount of time to wait after incrementing or decrementing
 *              the ndiv before doing it again.
 */
    const LwU32 slideStepDelayNs;

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
 * @brief       PLL contains a PLDIV
 *
 * @details     When false, there is no PLDIV, which is equivalent to a PLDIV
 *              programmed to 1.
 *
 * @note        bPldivEnable and bDiv2Enable are mutually exclusive and shoud
 *              not both be set to true.
 */
    const LwBool bPldivEnable : 1;

/*!
 * @brief       PLL contains a DIV2
 *
 * @details     DRAMPLL contains a glitchy DIV2 divider in place of the more
 *              common PLDIV.
 *
 * @note        bPldivEnable and bDiv2Enable are mutually exclusive and should
 *              not both be set to true.
 */
    const LwBool bDiv2Enable : 1;
} ClkAPll;


/* ------------------------ External Definitions --------------------------- */
/*!
 * @brief       Virtual table
 * @memberof    ClkAPll
 * @protected
 */
extern const ClkAPll_Virtual clkVirtual_APll;


/* ------------------------ Function Prototypes ---------------------------- */

/*!
 * @brief       Implementations of virtual functions.
 * @memberof    ClkAPll
 * @protected
 */
/**@{*/
FLCN_STATUS clkRead_APll(ClkAPll *this, ClkSignal *pOutput, LwBool bActive)
    GCC_ATTRIB_SECTION("imem_libClkReadPerfDaemon", "clkRead_APll");
FLCN_STATUS clkConfig_APll(ClkAPll *this, ClkSignal *pOutput, ClkPhaseIndex phase,
    const ClkTargetSignal *pTarget, LwBool bHotSwitch)
    GCC_ATTRIB_SECTION("imem_libClkConfig", "clkConfig_APll");
FLCN_STATUS clkProgram_APll(ClkAPll *this, ClkPhaseIndex phase)
    GCC_ATTRIB_SECTION("imem_libClkProgram", "clkProgram_APll");
FLCN_STATUS clkCleanup_APll(ClkAPll *this, LwBool bActive)
    GCC_ATTRIB_SECTION("imem_libClkProgram", "clkCleanup_APll");
#if CLK_PRINT_ENABLED
void clkPrint_APll(ClkAPll *this, ClkPhaseIndex phaseCount)
    GCC_ATTRIB_SECTION("imem_libClkPrint", "clkPrint_APll");
#endif // CLK_PRINT_ENABLED
/**@}*/


/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK3_FS_APLL_H

