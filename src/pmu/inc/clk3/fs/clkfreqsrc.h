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

#ifndef CLK3_FS_FREQSRC_H
#define CLK3_FS_FREQSRC_H


/* ------------------------ Includes --------------------------------------- */

#ifndef CLK_SD_CHECK        // See pmu_sw/prod_app/clk/clk3/clksdcheck.c
#include "pmusw.h"
#endif                      // CLK_SD_CHECK

#include "clk3/clkprint.h"
#include "clk3/clksignal.h"


/* ------------------------ Macros ----------------------------------------- */

/*!
 * @brief       Is the ClkFreqSrc read only?
 * @memberof    ClkFreqSrc
 *
 * @details     If a ClkFreqSrc is read only, it is expected to always output
 *              the same frequency when being configured, regardless of the
 *              target signal parameter passed in.  It is important to be able
 *              to identify these "locked objects" because many 'clkConfig'
 *              algorithms can be optimized if the input is locked.
 *
 * @todo        Consider adding a 'bIsReadOnly' flag to 'ClkFreqSrc_Virtual' to
 *              replace this macro.  This would eliminate the need for forward
 *              declarations of 'clkVirtual_Xtal' and 'clkVirtual_ReadOnly' in
 *              this header file.
 */
#define CLK_IS_READ_ONLY__FREQSRC(_FREQSRC)                    \
    ((_FREQSRC)->pVirtual == &clkVirtual_Xtal           ||     \
     (_FREQSRC)->pVirtual == &clkVirtual_ReadOnly)


/*!
 * @brief       Virtual Interface Point Thunks
 * @memberof    ClkFreqSrc
 * @see         ClkFreqSrc_Virtual
 * @see         https://wiki.lwpu.com/engwiki/index.php/Resman/RM_Foundations/Lwrrent_Projects/RTOS-FLCN-Scripts#rtos-flcn-script_-_Static_UCode_Analysis_on_objdump
 * @public
 *
 * @details     Thunks (thin wrappers) used to call virtual interface points.
 *
 *              In general, virtual implementations should be called via these
 *              '_FreqSrc_VIP' thunks.  In effect, they define the virtual
 *              interface for 'ClkFreqSrc'.  See comments in 'ClkFreqSrc_Virtual'
 *              for each function for detail information.
 *
 *              Any functions that calls one of these VIP functions must be
 *              listed in pmu_sw/build/Analyze.pm to prevent a "Missing mapping
 *              for function pointer" from 'rtos-flcn-script.pl' during the
 *              build process.  For example:
 *
 *                  'clkConfig_PllLdivDispFreqDomain' => {
 *                      'ALL' => [
 *                          'clkConfig_LdivUnit',
 *                          'clkConfig_Multiplier',
 *                          'clkConfig_Mux',
 *                          'clkConfig_Pll',
 *                          'clkConfig_ReadOnly',
 *                          'clkConfig_Wire',
 *                          'clkConfig_Xtal',
 *                      ],
 *                  },
 *
 *
 * @note        Although these thunks are public, the implementations themselves
 *              are protected, which means that subclasses may call them directly
 *              for the purpose of daisy-chaining up the inheritance tree.
 */
/**@{*/
#define clkRead_FreqSrc_VIP(this, pOutput, bActive)                            \
    ((this)->pVirtual->clkRead((this), (pOutput), (bActive)))

#define clkConfig_FreqSrc_VIP(this, pOutput, phase, pTarget, bHotSwitch)       \
    ((this)->pVirtual->clkConfig((this), (pOutput), (phase), (pTarget), (bHotSwitch)))

#define clkProgram_FreqSrc_VIP(this, phase)                                    \
    ((this)->pVirtual->clkProgram((this), (phase)))

#define clkCleanup_FreqSrc_VIP(this, bActive)                                  \
    ((this)->pVirtual->clkCleanup((this), (bActive)))

#if CLK_PRINT_ENABLED
#define clkPrint_FreqSrc_VIP(this, phaseCount)                                 \
    do                                                                         \
    {   if (this) (this)->pVirtual->clkPrint(this, phaseCount);                \
    } while(0)
#endif
/**@}*/


/*!
 * @brief       Define virtual table
 * @memberof    ClkFreqSrc
 * @see         ClkFreqSrc_Virtual
 * @see         drivers/resman/arch/lwalloc/common/inc/pmu/pmuifclk.h
 *
 * @details     This macros defines the content (using initializers) of the
 *              table of virtual function pointers for the specified class.
 *
 *              The class is named 'Clk[class]' where '[class]' is specified
 *              by '_CLASS'
 *
 *              The class must declare all the members of 'ClkFreqSrc_Virtual'
 *              in the form of '[fun]_[class]'. For example: 'clkRead_Pll' or
 *              'clkProgram_Multiplier'.
 *
 *              In the case where the class inherits an implementation from its
 *              super class, a macro should be used to define an alias.  For
 *              example, 'CLkMultiplier' inherits 'clkProgram' from 'ClkWire',
 *              its super class, rather than define its own implementation.
 *              As such, its header file contains this macro:
 *                  #define clkProgram_Multiplier clkProgram_Wire
 *
 *              Usage of this macro should be followed with a semicolon.
 *
 * @param[in]   _CLASS      Bare name of Clocks 3.x class (e.g. 'Pll')
 */
#if CLK_PRINT_ENABLED
#define CLK_DEFINE_VTABLE__FREQSRC(_CLASS)                                     \
const ClkFreqSrc_Virtual clkVirtual_##_CLASS                                   \
    GCC_ATTRIB_SECTION("dmem_clk3x", "clkVirtual_" #_CLASS) =                  \
{                                                                              \
    .clkRead        = (ClkRead_FreqSrc_VIP(*))    clkRead_##_CLASS,            \
    .clkConfig      = (ClkConfig_FreqSrc_VIP(*))  clkConfig_##_CLASS,          \
    .clkProgram     = (ClkProgram_FreqSrc_VIP(*)) clkProgram_##_CLASS,         \
    .clkCleanup     = (ClkCleanup_FreqSrc_VIP(*)) clkCleanup_##_CLASS,         \
    .clkPrint       = (ClkPrint_FreqSrc_VIP(*))   clkPrint_##_CLASS,           \
}
#else
#define CLK_DEFINE_VTABLE__FREQSRC(_CLASS)                                     \
const ClkFreqSrc_Virtual clkVirtual_##_CLASS                                   \
    GCC_ATTRIB_SECTION("dmem_clk3x", "clkVirtual_" #_CLASS) =                  \
{                                                                              \
    .clkRead        = (ClkRead_FreqSrc_VIP(*))    clkRead_##_CLASS,            \
    .clkConfig      = (ClkConfig_FreqSrc_VIP(*))  clkConfig_##_CLASS,          \
    .clkProgram     = (ClkProgram_FreqSrc_VIP(*)) clkProgram_##_CLASS,         \
    .clkCleanup     = (ClkCleanup_FreqSrc_VIP(*)) clkCleanup_##_CLASS,         \
}
#endif


/*!
 * @brief       Maximum number of phases
 *
 * @note        This value may vary by build (i.e. litter) if there is a future
 *              need for more or fewer phases.
 */
#define CLK_MAX_PHASE_COUNT 3U


/* ------------------------ Datatypes -------------------------------------- */

/*!
 * @brief       Index into phase arrays
 *
 * @details     This data type is used to index the various phase arrays and
 *              to contain phase counts.
 */
typedef LwU8 ClkPhaseIndex;

typedef struct ClkFreqSrc           ClkFreqSrc;         //!< One per object -- Statically allocated
typedef struct ClkFreqSrc_Virtual   ClkFreqSrc_Virtual; //!< One per class -- Statically allocatied


/*!
 * @brief       Function Pointer Types for 'ClkFreqSrc_Virtual'
 * @memberof    ClkFreqSrc
 * @see         ClkFreqSrc_Virtual
 */
/**@{*/
#define ClkRead_FreqSrc_VIP(fname) FLCN_STATUS (fname)(ClkFreqSrc *this, ClkSignal *pOutput, LwBool bActive)
typedef FLCN_STATUS ClkConfig_FreqSrc_VIP (ClkFreqSrc *this, ClkSignal *pOutput, ClkPhaseIndex phase, const ClkTargetSignal *pTarget, LwBool bHotSwitch);
typedef FLCN_STATUS ClkProgram_FreqSrc_VIP(ClkFreqSrc *this, ClkPhaseIndex phase);
typedef FLCN_STATUS ClkCleanup_FreqSrc_VIP(ClkFreqSrc *this, LwBool bActive);
#if CLK_PRINT_ENABLED
typedef void ClkPrint_FreqSrc_VIP(ClkFreqSrc *this, ClkPhaseIndex phaseCount);
#endif
/**@}*/


//
// Virtual tables of "Locked frequency source objects".  These objects are
// expected to always output the same frequency while programming a
// clock domain, and can be used to improve the speed of certain clkConfig
// algorthims.  The virtual table is needed to uniquely identify a ClkFreqSrc
// pointer and determine which derived class it points too.
//
// These declarations are needed only for the CLK_IS_READ_ONLY__FREQSRC macro.
// Should we ever retied that macro, these should be deleted.
//
extern const ClkFreqSrc_Virtual clkVirtual_Xtal;
extern const ClkFreqSrc_Virtual clkVirtual_ReadOnly;


/*!
 * @brief       Table of virtual interface points
 * @memberof    ClkFreqSrc
 *
 * @details     Each function pointer represents a virtual interface point (VIP).
 *              There is a thunk (thin wrapper) for each virtual interface point
 *              to simplify the calling syntax.  By convention, each of these
 *              thunks has the same name as the corresponding pointer with the
 *              suffix _FreqSrc_VIP attached.  Conceptually, these xxx_VIP thunks
 *              are roughly equivalent to the xxx_HAL wrappers used by the
 *              rmconfig/LWOC code generators.
 *
 *              Clocks 3.x differs from Clocks 2.x in that Clocks 2.x used a
 *              dynamic vtable in which the pointer value would change.  In
 *              costrast, the member function pointers in Clocks 3.x are 'const'
 *              and do not change.
 *
 *              The implementations referenced by each pointer has a slightly
 *              different parameter list v. the pointer itself in that the
 *              first parameter is the specific sublcass in the implementation,
 *              but is 'ClkFreqSrc' in the pointer.  For example, since
 *              'ClkPll <: ClkFreqSrc', the 'ClkPll' implementation of 'clkRead' is:
 *                  FLCN_STATUS clkRead_Pll(ClkPll *this, ClkSignal *pOutput, LwBool bActive)
 *              even though the pointer typedef is:
 *                  FLCN_STATUS ClkRead_FreqSrc_VIP(ClkFreqSrc *this, ClkSignal *pOutput, LwBool bActive)
 *
 * @note        const:  All structures of this type should be 'const'.
 */
struct ClkFreqSrc_Virtual
{
/*!
 * @brief       Read hardware.
 * @see         clkConfig
 * @see         clkCleanup
 * @see         ClkFreqSrc::cycle
 *
 * @details     The software state (i.e. configuration) for all phases is
 *              computed based on the state of the hardware.  In other words,
 *              the hardware is read and the results are placed in all elements
 *              of the phase array.
 *
 *              In general, this function is called after initialization.
 *              Assuming that (1) 'clkCleanup' sets the phase array according
 *              to the hardware state, (2) that the hardware state does not
 *              change, and (3) there was no error in programming, there is no
 *              need to call this function again.
 *
 *              This function (and 'clkCleanup') assigns the state for all
 *              phases of this object's phase array so that, if 'clkConfig'
 *              is not called on some phases, the configuration will be the
 *              same as the prior phase.
 *
 *              Unlike Clocks 2.x, implementations for classes that have more
 *              that one potential input (e.g. 'ClkMux') call all inputs, not
 *              just the active one.  For inputs that are not active, 'bActive'
 *              is set to LW_FALSE which indicates that the result in 'pOutput'
 *              will not be used.
 *
 *              All fields of 'pOutput' are assigned appropriate values upon
 *              successful completion.
 *
 *              Cycles must be detected along the active signal path using
 *              the 'ClkFreqSrc::cycle' flag.  The documentation for this
 *              flag contains details.
 *
 * @pre         'this' may not be NULL.
 *
 * @pre         'pOutput' may not be NULL.
 *
 * @post        On successful return (FLCN_OK), this software object's state
 *              reflects the corresponding hardware in all phases of its phase
 *              array.
 *
 * @post        On successful return, all elements of the phase array are equal
 *              to each other.
 *
 *
 * @ilwariant   This function does not write any registers.
 *
 * @ilwariant   This function does not throw a DBG_BREAKPOINT (or assertion) due
 *              to an invalid hardware state.  Instead, returns a descriptive
 *              nonzero error code.
 *
 * @param[in]   this                Pointer to the ClkFreqSrc object
 * @param[out]  pOutput             Clock signal produced by the hardware
 * @param[in]   bActive             Nonzero if this the clock signal generated
 *                                  by this object is being used
 */
    ClkRead_FreqSrc_VIP            (*clkRead);

/*!
 * @brief       Configure for the specified target and phase.
 *
 * @details     This function configures the frequency source object for the
 *              specified target in the specified phase of the programming process.
 *              In other words, it sets the software state with a plan for
 *              programming the hardware according to the 'pTarget' parameter.
 *
 *              If the exact target frequency (pTagret->freqKHz) cannot be
 *              reached, this function configures this object for the nearest
 *              possible frequency it can achieve.  No check is made for
 *              tolerance; it is the responsibility of the 'ClkDomain' object
 *              to make sure the output frequency is close enough.
 *
 *              If the target path is specified in 'pTarget->path' (not empty),
 *              objects that have more than one potential input (e.g. 'ClkMux')
 *              use the head node for the active input.  The remainder of
 *              path is passed to the input node.
 *
 *              'pTarget->source' is ignored.  However, 'pOutput->source' is
 *              set by objects that have corresponding values defined as
 *              LW2080_CTRL_CLK_PROG_1X_SOURCE_xxx constants in the same way
 *              as 'clkRead'.
 *
 *              This function daisy-chains along the schematic dag to configure
 *              the input it will use.  When this function successfully returns
 *              (FLCN_OK), all nodes along the output path have been correctly
 *              configured.
 *
 *              The 'phase' parameter, which must be nonzero, indicates which
 *              programming phase is to be configured.  This function assumes
 *              that the state at 'phase-1' represents the hardware state when
 *              programming 'phase' begins.
 *
 *              To make this assumption work, this function sets the state in
 *              all elements of the phase array at 'phase' and after (greater).
 *              It's important because this function is not necessarily called
 *              on each phase for any particular frequency-source object.
 *
 *              Specifically, this function is called only on objects that are
 *              active.  It is not called on objects that are inactive.
 *
 *              Cycles must be rejected along the active signal path specified
 *              by 'pTarget->path' using the 'ClkFreqSrc::cycle' flag.
 *              The documentation for this flag contains details.
 *
 *              Unlike Clocks 2.x, this function is not spelwlative.  That is,
 *              it should be called only once for a particular object ('this')
 *              for a particular phase ('phase').
 *
 * @pre         'this', 'pOutput' and 'pTarget' may not be NULL.
 *
 * @pre         The 'phase' parameter must not be zero.
 *
 * @post        On successful return (FLCN_OK), the software state reflects
 *              the configuration required to be as near as possible to the
 *              requested output signal.  This implies that 'clkConfig' has
 *              been successfully called (daisy-chain) on the active input
 *              object (if any).
 *
 * @post        On successful return, all elements of the phase array with an
 *              index greater than or equal to the 'phase' parameter are equal
 *              to each other.
 *
 * @post        This function does not change elements of the phase array with
 *              an index less than the 'phase' parameter.
 *
 * @post        If this function returns an error (not FLCN_OK) then elments of
 *              the phase array with an index greater than or equal to the
 *              'phase' parameter are undefined and should not be relied upon.
 *
 * @ilwariant   This function neither reads not writes any registers.
 *
 * @ilwariant   This function does not throw a DBG_BREAKPOINT (or assertion) due
 *              to an invalid 'pTarget' parameter.  Nor does it throw a breakpoint
 *              due to a problem of the software state of the previous phase.
 *
 * @ilwariant   This function does not throw a DBG_BREAKPOINT (or assertion) due
 *              to an invalid hardware state.  Instead, it returns a descriptive
 *              nonzero error code.
 *
 * @param[in]   this                Pointer to the ClkFreqSrc object
 * @param[out]  pOutput             Resultant clock signal
 * @param[in]   pTarget             Target frequency, path, etc.
 * @param[in]   phase               Index into the phase array of this object
 * @param[in]   bHotSwitch          Nonzero iff this object will be programmed
 *                                  while it is in the path that is lwrrently
 *                                  generating the clock singal during this phase
*/
    ClkConfig_FreqSrc_VIP          (*clkConfig);

/*!
 * @brief       Program the hardware for the specified phase.
 * @see         clkConfig
 * @see         clkRead
 * @see         clkCleanup
 *
 * @details     This function programs the hardware object according to the
 *              software state in the specified phase.  This object's phase
 *              array must have a valid configuration (via 'clkConfig',
 *              'clkRead', or 'clkCleanup') before this function is called.
 *
 *              The 'phase' parameter, which must not be zero, is the index into
 *              this object's phase array which specifies the state to be
 *              programmed into the hardware.  'phase-1' contains the hardware
 *              state before programming begins so that this function can do
 *              what is necessary to efficiently transition from 'phase-1' to
 *              'phase'.  In particular, this function should skip register
 *              writes if the hardware state would be unchanged.  (In contrast,
 *              Clocks 1.0 and Clocks 2.x has a register file cache to do this.)
 *
 *              This function is not called on an object that is not in use in
 *              the specified phase.
 *
 *              If programming is complete (FLCN_OK), the caller can assume that
 *              the hardware has been programmed to agree with the 'output' member.
 *
 *              If programming fails, this function returns a descriptive nonzero
 *              error code.  The caller would typically return the same error code.
 *              Any recovery attempts (e.g. resetting a PLL) are made before this
 *              function returns an error.  This function does not issue a break
 *              or assertion due to failed programming or an invalid hardware
 *              state.
 *
 *              This function is supported only in nonzero phases.
 *
 * @pre         'this' may not be NULL.
 *
 * @pre         Before calling, this object must have a valid configuration.
 *
 * @post        If the function returns FLCN_ERR_MORE_PROCESSING_REQUIRED, then
 *              it must be called again.
 *
 * @post        No implementation of this function waits or polls the hardware.
 *              The software state of this object is used to keep track of what
 *              steps are still pending.
 *
 * @ilwariant   This function does not call virtual functions other than to
 *              daisy-chain to 'clkProgram'.
 *
 * @ilwariant   This function does not throw a DBG_BREAKPOINT (or assertion) due
 *              to an invalid hardware state.  Instead, it returns a descriptive
 *              nonzero error code.
 *
 * @param[in]   this                Pointer to the ClkFreqSrc object
 * @param[in]   phase               Index into the phase array of this object
 */
    ClkProgram_FreqSrc_VIP         (*clkProgram);

/*!
 * @brief       Clean up after programming.
 * @see         clkRead
 * @see         clkProgram
 *
 * @details     This function cleans up the hardware after programming and
 *              initializes all elements of the phase array to reflect the
 *              current hardware state.
 *
 *              This function is called only after 'clkProgram' has been called
 *              for all active phases.  'clkConfig' and 'clkRead' always update
 *              the last phase in the phase array (in addition to the phase
 *              specified in the 'phase' parameter and all in between);
 *              therefore the last phase reflects the hardware state.
 *
 *              The 'bActive' parameter indicates if the hardware object is is
 *              use.  Generally, if this object is not in use, then this function
 *              changes the hardware state to save power, etc.  This includes
 *              such things as gating clock signals or powering down PLLs.
 *              It may also be used to disabling WARs.  ('bActive' is more-or-
 *              less equivalent to ClkFreqSrc::hot in Clocks 2.x.)
 *
 *              Also, this function copies the state of the last phase
 *              to the other elements in the phase array.  Since the last phase
 *              is assumed to reflect the current hardware state when the
 *              function is called, then all phases reflect the hardware state
 *              when the function exits.  This post-condition mimics the effect
 *              of calling 'clkRead' but without reading any registers.
 *
 *              This function should daisy-chain to all inputs (if any).  In
 *              the case of an object with multiple inputs (i.e. ClkMux), the
 *              'bActive' parameter is set to false except on the active input.
 *              Other than that, 'bActive' should be preserved.
 *
 * @pre         Before calling this function, the software state of the last
 *              element in the phase array must reflect the hardware state
 *              (by calling 'clkProgram').
 *
 * @post        If the function successfully returns (FLCN_OK), then the
 *              hardware object is in a proper state for normal operation.
 *
 * @post        All elements of the phase array are equal to the value that the
 *              last element had when this function was called.
 *
 * @ilwariant   This function does not throw a DBG_BREAKPOINT (or assertion)
 *              due to an invalid hardware state.  Instead, it returns a
 *              descriptive nonzero error code.
 *
 * @param[in]   this                Pointer to the ClkFreqSrc object
 * @param[in]   bActive             Nonzero unless this object isn't being used
 */
    ClkCleanup_FreqSrc_VIP         (*clkCleanup);

/*!
 * @brief       Print dynamic state.
 * @see         clkPrintAll_FreqDomain
 *
 * @details     In builds for which it is enabled, this function prints available
 *              dynamic state for both software and hardware,  This means field
 *              values from the software object other than those which are either
 *              const (initialized at comiler time) or are not changed after
 *              construction.  Additionally, applicable registers are read and
 *              their values printed.
 *
 *              This function traverses the schematic dag by calling itself
 *              on the input (ClkWire) or inputs (ClkMux).
 *
 *              This function should be called from `clkPrintAll_FreqDomain`
 *              just before a breakpoint is fired.  It prints too much info
 *              to be useful for any other scenario.
 *
 * @param[in]   this                Pointer to the ClkFreqSrc object
 * @param[in]   phaseCount          Number of phases for config/program
 */
#if CLK_PRINT_ENABLED
    ClkPrint_FreqSrc_VIP           (*clkPrint);
#endif
};


/*!
 * @class       ClkFreqSrc
 * @brief       Frequency Source Class
 * @see         ClkFreqSrc_Virtual
 *
 * @details     For the most part, the data in objects of subclasses does not
 *              change after initialization.  Specifically, the data in these
 *              objects are not epxosed to through the RM_PMU_ data structures.
 *
 *              As such, each concrete subclass contains a pointer to the
 *              subclass-specific RM_PMU_ data structure.  The pointer is not
 *              placed here in the base class since such a pointer would have to
 *              be down-casted in the subclass logic, something we'd like to
 *              avoid.  However, this means that we do no support any sort of
 *              inheritance in the RM_PMU_ data structures.
 *
 *              By convention, these structures are named in the form of
 *              'RM_PMU_CLK_FREQ_DOMAIN_GET_STATUS_[iftag]', where '[iftag]'
 *              is the all-uppercase base name of the concrete subclass (e.g.
 *              'PLL').  These structures are defined in 'pmuifclk.h'.
 *
 *              For each concrete subclass, there is exactly one instance of a
 *              virtual table and all objects of that subclass point to it via
 *              the 'pVirtual' member.
 *
 * @note        In the case where all the members of a struct are inherited from
 *              the superclass, then they are aliased using 'typedef'.
 *
 * @protected   Only functions belonging to the class and subclasses may access
 *              or modify anything in this struct.
 *
 * @note        abstract:  This class does not have a vtable if its own.
 */
struct ClkFreqSrc
{
/*!
 * @brief       Table of Virtual Interface Points (Vtable)
 */
    const ClkFreqSrc_Virtual * const pVirtual;

/*!
 * @brief       Cycle Detector
 * @see         ClkFreqSrc_Virtual::clkRead
 * @see         ClkFreqSrc_Virtual::clkConfig
 *
 * @details     This member is used to detect cycles within the schematic dag.
 *
 *              In general, any function which daisy-chains along the schematic
 *              dag should return on entry if this flag is nonzero.  If it
 *              is daisy-chaining along the active signal path, it should return
 *              FLCN_ERR_CYCLE_DETECTED.  Otherwise, it should return FLCN_OK
 *              (or similar).
 *
 *              If the flag is zero on entry, it should set the flag to nonzero
 *              before daisy-chaining to an input, then reset it to zero upon
 *              return from the daisy-chain.
 *
 *              In theory, such cycles should not happen.  However, it is
 *              possible for the 'pTarget->path' value passed to 'clkConfig' to
 *              request a cycle, which would put the hardware into a bad state
 *              if not rejected.  Since 'clkConfig' daisy-chains only along
 *              the active path, it should return FLCN_ERR_CYCLE_DETECTED any
 *              time it detects a cycle.
 *
 *              Similar logic could be put into 'clkProgram', but if 'clkConfig'
 *              does its job properly, then there would never be a cycle for
 *              'clkProgram' to trace.  However, it doesn't hurt to check.
 *
 *              'clkRead' differs from 'clkConfig' in that it daisy-chains to
 *              all potential inputs, not just the active one.  Therefore,
 *              it checks to see if it is active before deciding between FLCN_OK
 *              v. FLCN_ERR_CYCLE_DETECTED.  FLCN_ERR_CYCLE_DETECTED means that
 *              the hardware is lwrrently in a bad state.  (Don't laugh; this
 *              has happened in the past.)
 *
 *              Since 'clkCleanup' also daisy-chains to all potential inputs,
 *              it too would return FLCN_ERR_CYCLE_DETECTED only if it is active,
 *              like 'cllRead'.  However, like 'clkProgram', it should never
 *              see a cycle in the active path if 'clkConfig' works properly,
 *              but it doesn't hurt to check.
 */
    LwBool      bCycle;

};


/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK3_FS_FREQSRC_H

