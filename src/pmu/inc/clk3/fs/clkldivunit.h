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
 * @author  Eric Colter
 * @author  Lawrence Chang (ClkEmbeddedLdiv)
 * @author  Antone Vogt-Varvak
 */

#ifndef CLK3_FS_LDIVUNIT_H
#define CLK3_FS_LDIVUNIT_H


/* ------------------------ Includes --------------------------------------- */

#include "clk3/fs/clkwire.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */

typedef ClkWire_Virtual ClkLdivUnit_Virtual;


/*!
 * @brief       Linear Divider Per-Phase Status
 * @version     Clocks 3.1 and after
 * @see         LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_LDIV_UNIT
 */
typedef struct
{
/*!
 * @brief       Input frequency of this phase
 *
 * @details     The input frequency needs to be stored, because during the
 *              configuration stage, the input frequency of the previous phase
 *              is required.
 */
    LwU32       inputFreqKHz;

/*!
 * @brief       First Divider Value Times Two
 * @memberof    ClkLdivUnit
 * @protected
 *
 * @details     This value can also be an intermediate value when the LDIV
 *              is programmed hot.
 *
 *              In most cases, ldivBefore == ldivAfter. The exceptional case
 *              is dolwmented in the doxygen comments for ldivAfter.
 */
    LwU8        ldivBefore;

/*!
 * @brief       Final Divider Value Time Two
 *
 * @details     In nonzero phases, the value is written to the _LDIV after
 *              the input is switched. In most cases though, ldivAfter
 *              == ldivBefore so this value is not actually written.
 *
 *              However, in cases when the LDIV is programmed hot,
 *              ldivBefore actually holds an intermediate divider value
 *              while ldivAfter holds the final divider value.
 *              See clkConfig_CheckHotSwitch_LdivUnit for details.
 *
 * @note        protected:  Variable may be used directly from a subclass.
 */
    LwU8        ldivAfter;
} ClkLdivUnitPhase;


/*!
 * @brief       Linear Divider Version 1
 * @extends     ClkWire
 * @see         ClkLdivUnit_Virtual
 * @protected
 *
 * @details     Objects of this class divide the input signal.  The divisor can
 *              be fractional, which means that given a positive integer 'n',
 *              it is experssed in the register as 'n/2'.
 *
 *              Version 1 were used in Turng and prior chips.  For version 2, use
 *              'ClkLdivV2'.  The main differences are that version 1 supports:
 *              - Programmability,
 *              - Two LDIV units per register, and
 *              - Fractional divide.
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
 *              LW_PTRIM_SYS_xxxCLK_OUT or LW_PTRIM_SYS_xxxCLK_yyy_LDIV
 *              where xxx is the name of the clock domain and yyy represents
 *              the path. The generic name for the register is
 *              LW_PTRIM_SYS_CLK_LDIV.
 *
 *              Re LW_PTRIM_GPC_BCAST_GPC2CLK_OUT vs LW_PTRIM_SYS_GPC2CLK_OUT:
 *              LW_PTRIM_SYS_GPC2CLK_OUT is the one we should generally use.
 *              LW_PTRIM_GPC_BCAST_GPC2CLK_OUT is a CYA register that does
 *              essentially the same thing.  LW_PTRIM_SYS_USE_GPC_CONTROLS
 *              selects between these two registers.  Bug 1345962 is an
 *              example in which we considered using both of registers so
 *              that hardware could slowdown GPC2CLK by switching between
 *              them during a context switch.
 */
    const LwU32 ldivRegAddr;

/*!
 * @brief       Maximum Fractional LDIV Value Times Two
 * @protected
 *
 * @detail      Fractional divide is disabled for any divider value above
 *              this quantity.  Since 2 is the implicit denominator, the
 *              value of this member must be double the LDIV value.
 *
 *              2 (i.e. 1.0) disables fractional divide altogether.  Values
 *              less than 2 or in excess of 'maxLdivValue2x' are not permitted.
 *              This member colwentionally contains an even value.
 */
    const LwU8  maxFracDivValue;

/*!
 * @brief       Maximum LDIV Value Times Two
 * @protected
 *
 * @detail      clkConfig uses only divider values less than or equal to
 *              this quantity.  Since 2 is the implicit denominator, the
 *              value of this member must be double the LDIV value.  31.5
 *              (i.e. 63/2) is the typical maximum since the _LDIV register
 *              field is 6 bits wide.  Since the minimum is 1.0 (i.e. 2/2),
 *              2 effectively disables the linear divider unit.  Values
 *              less than that are not permitted.
 */
    const LwU8 maxLdivValue;

/*!
 * @brief       Divider Select
 * @protected
 *
 * @detail      Linear dividers are typically used in pairs, both of which
 *              are controlled by the same register (ldivReg).  This value,
 *              which is always zero or one, is used to specify which ldiv
 *              is represented by this object.  It is used to index
 *              LW_PTRIM_SYS_CLK_LDIV_DIV(i).  Zero generally corresponds
 *              to _ONESRC0DIV or _BYPDIV in the manuals while one corresponds
 *              to _ONESRC1DIV or _VCODIV.
 *
 *              Note that this class does not "think" in terms of bypass
 *              vs VCO; that is a matter for the constructor.
 *              Divider zero is _BYPDIV in LDIVs and _ONESRC0DIV in OSMs.
 *              Divider one is _VCODIV in LDIVs and _ONESRC1DIV in OSMs.
 */
    const LwU8 divider;

/*!
 * @brief       Cleanup Divider WAR Value Times Two
 * @see         http://lwbugs/1424323 [GM20x] Set bypass linear divider to BY2 during clkCleanup.
 * @see         http://lwbugs/1393532 [GM10x] 1620MHz with MSCG/GC5
 * @protected
 *
 * @detail      When this linear divider is no longer in use, this value
 *              is written to the register.  Zero disables this WAR.
 *
 *              4/2 would be the typical value for the bypass path on
 *              clock domains subject to MSCG/GC5 (and similar).
 *              Failure to apply this war causes MSCG/GC5 to resume to
 *              the prior frequency, which may be too fast.
 *
 *              If PMU function _msClkSwitchToBypass_xxx (or msGateClocks_xxx)
 *              were to write a proper divider value, then this WAR would
 *              not be needed.
 *
 * @todo        Verify if this WAR is needed for chips supported by Clocks 3.x.
 */
    const LwU8 ldivCleanup;

/*!
 * @brief       Phase-specific information
 */
    ClkLdivUnitPhase phase[CLK_MAX_PHASE_COUNT];
} ClkLdivUnit;


/* ------------------------ External Definitions --------------------------- */

/*!
 * @brief       Virtual table
 * @memberof    ClkLdivUnit
 * @protected
 */
extern const ClkLdivUnit_Virtual clkVirtual_LdivUnit;


/* ------------------------ Function Prototypes ---------------------------- */

/*!
 * @brief       Implementations of virtual functions.
 * @memberof    ClkLdivUnit
 * @protected
 */
/**@{*/
FLCN_STATUS clkRead_LdivUnit(ClkLdivUnit *this, ClkSignal *pOutput, LwBool bActive)
    GCC_ATTRIB_SECTION("imem_libClkReadPerfDaemon", "clkRead_LdivUnit");
FLCN_STATUS clkConfig_LdivUnit(ClkLdivUnit *this, ClkSignal *pOutput, ClkPhaseIndex phase,
    const ClkTargetSignal *pTarget, LwBool bHotSwitch)
    GCC_ATTRIB_SECTION("imem_libClkConfig", "clkConfig_LdivUnit");
FLCN_STATUS clkProgram_LdivUnit(ClkLdivUnit *this, ClkPhaseIndex phase)
    GCC_ATTRIB_SECTION("imem_libClkProgram", "clkProgram_LdivUnit");
FLCN_STATUS clkCleanup_LdivUnit(ClkLdivUnit *this, LwBool bActive)
    GCC_ATTRIB_SECTION("imem_libClkProgram", "clkCleanup_LdivUnit");
#if CLK_PRINT_ENABLED
void clkPrint_LdivUnit(ClkLdivUnit *this, ClkPhaseIndex phaseCount)
    GCC_ATTRIB_SECTION("imem_libClkPrint", "clkPrint_LdivUnit");
#endif // CLK_PRINT_ENABLED
/**@}*/


/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK3_FS_LDIVUNIT_H

