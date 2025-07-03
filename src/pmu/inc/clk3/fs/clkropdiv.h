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

#ifndef CLK3_FS_ROPDIV_H
#define CLK3_FS_ROPDIV_H


/* ------------------------ Includes --------------------------------------- */

#include "clk3/fs/clkwire.h"


/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */

// Same vtable as base class
typedef         ClkWire_Virtual ClkRoPDiv_Virtual;


/*!
 * @extends     ClkWire
 * @version     Clocks 3.1
 * @brief       Simple Read-Only Divider
 * @protected
 *
 * @details     Objects if this class represent a simple divider that is not
 *              programmed.  The clock signal passing through this divider is
 *              divided by the numerical value in the register field.  (This
 *              differs from LDIVs, for example.)
 *
 * @note        clkConfig/clkProgram/clkCleanup are not supported.  Calls to
 *              any of these functions results in a NULL pointer exception.
 *              This is okay for 'ClkMclkFreqDomain' (and 'ClkReadOnly') since
 *              they never call these functions.
 *
 * @note        Do not use this class in a build (e.g. GA10x) which also uses
 *              ClkPDiv (clkpdiv.c).  You save IMEM/DMEM if you use choose
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
 *              LW_PVTRIM_SYS_SPPLLn_COEFF2 where 'n' is the SPPLL number.
 *
 *              For REFPLL, the name in dev_fbpa is LW_PFB_FBPA_REFMPLL_COEFF.
 *
 *              For HBMPLL, the name in dev_fbpa is LW_PFB_FBPA_FBIO_COEFF_CFG(i)
 *              for some 'i' based on floorsweeping.
 *
 *              The generic name for the register is LW_PTRIM_SYS_PLL_COEFF.
 *
 *              This member can not be marked 'const' because the GH100 MCLK
 *              constructor (for example) assigns this value at runtime based
 *              on floorsweeping.  Nonetheless, it is effectively 'const'
 *              after construction.
 */
    LwU32       regAddr;

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
} ClkRoPDiv;


/* ------------------------ External Definitions --------------------------- */

/*!
 * @brief       Virtual table
 * @memberof    ClkRoPDiv
 * @protected
 */
extern const ClkRoPDiv_Virtual clkVirtual_RoPDiv;


/* ------------------------ Function Prototypes ---------------------------- */

/*!
 * @brief       Implementations of virtual functions.
 * @memberof    ClkRoPDiv
 * @protected
 */
/**@{*/
FLCN_STATUS clkRead_RoPDiv(ClkRoPDiv *this, ClkSignal *pOutput, LwBool bActive)
    GCC_ATTRIB_SECTION("imem_libClkReadPerfDaemon", "clkRead_RoPDiv");
#define     clkConfig_RoPDiv    NULL
#define     clkProgram_RoPDiv   NULL
#define     clkCleanup_RoPDiv   NULL
#if CLK_PRINT_ENABLED
void clkPrint_RoPDiv(ClkRoPDiv *this, ClkPhaseIndex phaseCount)
    GCC_ATTRIB_SECTION("imem_libClkPrint", "clkPrint_RoPDiv");
#endif // CLK_PRINT_ENABLED
/**@}*/

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK3_FS_PDIV_H

