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
 * @author  Antone Vogt-Varvak
 * @author  Lawrence Chang
 * @author  Manisha Choudhury
 */

#ifndef CLK3_FS_MUX_H
#define CLK3_FS_MUX_H


/* ------------------------ Includes --------------------------------------- */

#include "clk3/fs/clkfreqsrc.h"
#include "clk3/clkfieldvalue.h"


/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */

typedef ClkFreqSrc_Virtual ClkMux_Virtual;


/*!
 * @brief       Per-Phase Status for Multiplexer
 * @version     Clocks 3.1 and after
 */
typedef struct
{
/*!
 * @brief       Which of the inputs is the active input to the mux
 */
    LwU8 inputIndex;

} ClkMuxPhase;


/*!
 * @extends     ClkFreqSrc
 * @brief       Multi-Input Frequency Source
 *
 * @details     These classes contain the logic for frequency source objects that
 *              have more than one input.  This may mean a simple multiplexer or
 *              an array of multiplexers (e.g. OSM) provided they are controlled
 *              by the same register.
 *
 *              The multiplexers may be glitchy or glitchless (but not a mix).
 *              There is also support for gating the multiplexer when not in use.
 *
 *              What is not supported is an array of multiplexers with more than
 *              one path to select a single input.  Multiple software objects of
 *              this class would be needed.  Input 1 has more than one path in
 *              this example:
 *                  input 0 --------------------------+
 *                  input 1 ----------+---------+     |
 *                  input 2 ----+     |         |     |
 *                              |     |         |     |
 *                           ---+-----+---   ---+-----+---
 *                           \  2x1 mux  /   \  2x1 mux  /
 *                            -----+-----     -----+-----
 *                                 |               |
 *                             ----+---------------+----
 *                             \        2x1 mux        /
 *                              -----------+-----------
 *                                         |
 *                                        out
 */
typedef struct
{
/*!
 * @brief       Inherited state
 * @ilwariant   Inherited state must always be first.
 */
    ClkFreqSrc  super;

/*!
 * @brief       Map between switch register values and node numbers of potential inputs
 * @see         muxGateValue
 * @protected
 *
 * @details     This final member maps a potential input to the field mask and
 *              value required to program the multiplexer to that input.
 *              These values apply to both 'muxReg' and 'statusReg'.
 *
 *              If inputs of this object can be gated with a field in 'muxReg',
 *              then each element of this array must include the field value
 *              to ungate the inputs.  This is smart even if we don't use the
 *              gating feature, but essential if we do (i.e. if 'muxGateValue'
 *              is used).  OSMs are an example.
 *
 * @ilwariant   The number of elements in this array must equal ClkMux::count.
 *
 * @note        final:          The pointer is initialized by the constructor and never subsequently changed.
 *                              Moreover, the data in the array is never changed after construction.
 */
    const ClkFieldValueMap muxValueMap;

/*!
 * @brief       Potential input array
 * @protected
 *
 * @details     This array contains pointers to the frequency source objecs that
 *              that object may use as inputs.  Unconnected inputs are set to
 *              the NULL pointer.
 *
 * @see         ClkMux::count
 *
 * @note        final:          Value does not change after construction.
 */
    ClkFreqSrc * const * const input;

/*!
 * @brief       Register to switch multiplexer among the inputs
 * @protected
 *
 * @details     For Turing One-Source Modules (OSMs), the name of this register
 *              in the manuals is generally LW_PTRIM_SYS_xxxCLK_yyy_SWITCH where
 *              xxx is the name of the clock domain and yyy represents the path.
 *              The generic name for the register is LW_PTRIM_SYS_CLK_SWITCH.
 *              OSMs generally do not share this register with other objects.
 *
 *              For Turing linear dividers, LW_PTRIM_SYS_SEL_VCO is the name
 *              given in the manuals.  In contrast to OSMs, this register is
 *              generally shared with other linear dividers.
 *
 *              For Ampere and later Switch Dividers, this register is usually
 *              named LW_PVTRIM_SYS_xxx_SWITCH_DIVIDER where xxx is the name of
 *              the clock domains.
 *
 *              This member can not be 'const' because there is floorsweeping
 *              logic for GA100 in 'clkConstruct_SchematicDag'.  However, its
 *              values does not change after construction.
 */
    LwU32 muxRegAddr;

#if PMUCFG_FEATURE_ENABLED(PMU_CLK_SWDIV_SUPPORTED)
/*!
 * @brief       Field to check if the multiplexer is done switching
 * @see         Bug 2664354
 * @protected
 *
 * @details     Glitchless multiplexers can take time to switch.  For example,
 *              Ampere switch dividers (SWDIVs) LDIVs may several cycles.
 *              As such, it is necessary to poll until the switch completes.
 *
 *              Logic in this class assumes that this field is part of the
 *              'muxRegAddr' register.
 *
 *              Starting with GA10x, this is the _SWITCH_DIVIDER_DONE bit
 *              serves this purpose in switch dividers.
 *
 *              Leave this field zero (uninitialized) when not applicable.
 *
 *              GA10x is the first chip family to use a _DONE field.  This
 *              feature comes with the switch dividers (SWDIVs).
 *
 *              Due to a bug in the GA10x emulation model, this field must
 *              be set to zero in emulation to disable the feature.  However,
 *              it should be considered final and does not change after
 *              construction.  See bug 2664354/181.
 */
    ClkFieldValue muxDoneField;

#else
/*!
 * @brief       Register to test if the multiplexer has switched
 * @see         Bug 1625463
 * @protected
 *
 * @details     Glitchless multiplexers can take time to switch.  For example,
 *              Pascal LDIVs may take 170 cycles, which is significant at low
 *              clock speeds.  As such, it is necessary to poll until the switch
 *              completes.
 *
 *              It is assumed that the field definitions are the same for this
 *              register as for 'muxReg'.
 *
 *              If this pointer is NULL, then no polling is required.
 *
 *              For some linear dividers, LW_PTRIM_SYS_STATUS_SEL_VCO is the
 *              name given in the manuals.  In contrast to OSMs, this register
 *              is generally shared with other linear dividers.
 *
 *              For One-Source Modules (OSMs), the status register is not
 *              usually required.
 *
 *              Status registers have fallen out of style with the advent of
 *              switch dividers (SWDIVs) for GA10x and later.
 */
    const LwU32 statusRegAddr;
#endif

/*!
 * @brief       Switch register value to disable all potential inputs
 * @see         CLK_DRF__FIELDVALUE (for initialization)
 * @see         muxValueMap
 * @protected
 *
 * @details     This value is applied to 'muxReg' during 'clkCleanup' (after
 *              'clkProgram' of the last phase) if the multiplexer is not hot.
 *              This feature may save power.
 *
 *              For OSM3s, this is LW_PTRIM_SYS_CLK_SWITCH_STOPCLK_YES.
 *
 *              Leave this field zero to disable gating (or if this object can
 *              not be gated).
 *
 *              If this object can be gated, but not with 'muxReg', then use a
 *              'ClkGate' object instead of this field.
 *
 *              If this member contains something other than zeroes, then the
 *              ungating field value must be set in all entries of 'muxValueMap'.
 */
    const ClkFieldValue muxGateField;

/*!
 * @brief       Length of potential input array
 * @protected
 *
 * @details     This is how long the array pointed to by ClkMux::input is.
 *
 * @see         ClkMux::input
 */
    const LwU8 count;

/*!
 * @brief       Glitchy flag
 * @protected
 *
 * @details     Multiplexers can be glitchy or glitchless.  When the clock
 *              signal is passing through a glitchy multiplexer, it is illegal
 *              to switch it.  This member is LW_TRUE iff this multiplexer (or
 *              multiplexer array) is glitchy.
 *
 * @note        protected:      Only functions belonging to the class and subclasses may read its value.
 * @note        final:          Value does not change after construction.
 */
    const LwBool glitchy;

/*!
 * @brief       Phase-specific information
 */
    ClkMuxPhase phase[CLK_MAX_PHASE_COUNT];
} ClkMux;


/* ------------------------ External Definitions --------------------------- */

extern const ClkFreqSrc_Virtual clkVirtual_Mux;


/* ------------------------ Function Prototypes ---------------------------- */

/*!
 * @brief       Implementations of virtual functions.
 * @memberof    ClkMux
 * @protected

 */
/**@{*/
FLCN_STATUS clkRead_Mux(ClkMux *this, ClkSignal *pOutput, LwBool bActive)
    GCC_ATTRIB_SECTION("imem_libClkReadPerfDaemon", "clkRead_Mux");
FLCN_STATUS clkConfig_Mux(ClkMux *this, ClkSignal *pOutput, ClkPhaseIndex phase,
    const ClkTargetSignal *pTarget, LwBool bHotSwitch)
    GCC_ATTRIB_SECTION("imem_libClkConfig", "clkConfig_Mux");
FLCN_STATUS clkProgram_Mux(ClkMux *this, ClkPhaseIndex phase)
    GCC_ATTRIB_SECTION("imem_libClkProgram", "clkProgram_Mux");
FLCN_STATUS clkCleanup_Mux(ClkMux *this, LwBool bActive)
    GCC_ATTRIB_SECTION("imem_libClkProgram", "clkCleanup_Mux");
#if CLK_PRINT_ENABLED
void clkPrint_Mux(ClkMux *this, ClkPhaseIndex phaseCount)
    GCC_ATTRIB_SECTION("imem_libClkPrint", "clkPrint_Mux");
#endif // CLK_PRINT_ENABLED
/**@}*/

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK3_FS_MUX_H

