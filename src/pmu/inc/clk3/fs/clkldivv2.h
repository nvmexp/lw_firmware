/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or dis  closure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file
 * @see     https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @author  Antone Vogt-Varvak
 */

#ifndef CLK3_FS_LDIVV2_H
#define CLK3_FS_LDIVV2_H


/* ------------------------ Includes --------------------------------------- */

#include "clk3/fs/clkwire.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */

typedef ClkWire_Virtual ClkLdivV2_Virtual;


/*!
 * @brief       Linear Divider
 * @extends     ClkWire
 * @see         ClkLdivV2_Virtual
 * @protected
 *
 * @details     Objects of this class divide the input signal.
 *
 *              The divisor can NOT be fractional.  This creates some confusion
 *              in GA10x because the _DIVIDER_SEL field (which contains the
 *              divider value) is defined inconsistently.
 *
 *              _DIVIDER_SEL is defined as 5:1 for GA102 here:
 *                  LW_PTRIM_SYS_XTAL4X_UNIV_SEC_CLK_CTRL
 *                  LW_PTRIM_SYS_PWRCLK_CTRL
 *              In this case, there is no fractional divide, and the divider is
 *              set to the field value + 1.
 *
 *              _DIVIDER_SEL is defined as 5:0 for GA100 everywhere.
 *              _DIVIDER_SEL is defined as 5:0 for GA102 here:
 *                  LW_PVTRIM_SYS_DISPCLK_SWITCH_DIVIDER
 *                  LW_PVTRIM_SYS_HUBCLK_OUT_SWITCH_DIVIDER
 *                  LW_PTRIM_SYS_DRAMCLK_ALT_SWITCH_DIVIDER
 *              In this case, bit 0 is the fractional divide indicator, but it
 *              always reads as zero indicating that fractional divide is
 *              disabled.  As such, using only 5:1 for these registers works
 *              out to be the same.  This was done to be consistent with prior
 *              chips which did support fractional divide.
 *
 *              This class uses 5:1 via LW_PTRIM_SYS_CLK_LDIV_V2.
 */
typedef struct
{
/*!
 * @brief       Inherited state
 * @protected
 *
 * @ilwariant   Inherited state must always be first.
 */
    ClkWire     super;

/*!
 * @brief       Register controlling the linear divider pair
 * @protected
 *
 * @details     The name of this register in the manuals is usually
 *              LW_P[V]TRIM_SYS_xx_SWITCH_DIVIDER where xxx is the name of
 *              the clock domain, although there are exceptions.
 */
    const LwU32 ldivRegAddr;
} ClkLdivV2;


/* ------------------------ External Definitions --------------------------- */

/*!
 * @brief       Virtual table
 * @memberof    ClkLdivV2
 * @protected
 */
extern const ClkLdivV2_Virtual clkVirtual_LdivV2;


/* ------------------------ Function Prototypes ---------------------------- */

/*!
 * @brief       Implementations of virtual functions.
 * @memberof    ClkLdivV2
 * @protected
 */
/**@{*/
FLCN_STATUS clkRead_LdivV2(ClkLdivV2 *this, ClkSignal *pOutput, LwBool bActive)
    GCC_ATTRIB_SECTION("imem_libClkReadPerfDaemon", "clkRead_LdivV2");
FLCN_STATUS clkConfig_LdivV2(ClkLdivV2 *this, ClkSignal *pOutput, ClkPhaseIndex phase, const ClkTargetSignal *pTarget, LwBool bHotSwitch)
    GCC_ATTRIB_SECTION("imem_libClkConfig", "clkConfig_LdivV2");
#define clkProgram_LdivV2 clkProgram_Wire       // Inherit
#define clkCleanup_LdivV2 clkCleanup_Wire       // Inherit
#if CLK_PRINT_ENABLED
void clkPrint_LdivV2(ClkLdivV2 *this, ClkPhaseIndex phaseCount)
    GCC_ATTRIB_SECTION("imem_libClkPrint", "clkPrint_LdivV2");
#endif // CLK_PRINT_ENABLED
/**@}*/


/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK3_FS_LDIVV2_H

