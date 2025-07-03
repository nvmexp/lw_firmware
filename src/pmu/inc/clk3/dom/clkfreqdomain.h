/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file
 * @see         https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @author      Akshata Mahajan
 * @author      Antone Vogt-Varvak
 * @author      Prafull Gupta
 *
 * @details     This file contains the declarations and definitions for five classes:
 *                  ClkFreqDomain               Base class
 *                  ClkVolatileFreqDomain       Domains whose state cannot be cached
 *                  ClkNolwolatileFreqDomain    Domains whose state can be cached
 *                  ClkProgrammableFreqDomain   Domains whose state can be cached and programmed
 *                  ClkFixedFreqDomain          Domains whose state can be cached but not programmed
 *
 *              Each of these classes uses the same structure (ClkFreqDomain).
 *              In effect, they are really just aliases of one another, which is
 *              to facilitate macro definitions for the subclass virtual tables
 *              and the like.  As such, they are all put into this same file
 *              to simplify maintenance.
 *
 * @note        OBJPERF has a concept of a clock domain which is different than
 *              OBJCLK's concept. Historically, OBJCLK (Clocks 2.1 and prior)
 *              called it a clock domain (hence names like 'clkDomain'), but now
 *              (Clocks 3.x and after) it is called a frequency domain to
 *              distinguish it from the OBJPERF concept.
 *
 * @note        Sometimes the domain (e.g. 'clkDomain') is passed around as a
 *              bitmap and sometimes as an index.  We should use an index (and
 *              therefore LwU8) everywhere that we don't need a bitmap, but
 *              there are some exceptions.
 */

#ifndef CLK3_DOM_FREQDOMAIN_H
#define CLK3_DOM_FREQDOMAIN_H

/* ------------------------ Includes --------------------------------------- */

#ifndef CLK_SD_CHECK        // See pmu_sw/prod_app/clk/clk3/clksdcheck.c
#include "pmusw.h"
#include "boardobj/boardobjgrp_e32.h"
#endif                      // CLK_SD_CHECK

#include "clk3/clkprint.h"
#include "clk3/clksignal.h"
#include "clk3/fs/clkfreqsrc.h"
#include "lib_semaphore.h"
#include "boardobj/boardobjgrp_iface_model_10.h"


/* ------------------------ Macros ----------------------------------------- */

/*!
 * Macro to get pointer to frequency domains group. Returns NULL if the feature
 * is not enabled.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLKS_ON_PMU)
#define CLK_FREQ_DOMAINS_GET()                                                \
    &(Clk.freqDomainGrp)
#else
#define CLK_FREQ_DOMAINS_GET()                                                \
    (NULL)
#endif

/*!
 * Macro to figure out if the given clock domains are enabled/disabled at a
 * given point of time.
 *
 * @note This macro doesn't guarantee the state to be the same after this check
 * happens. If you're looking for the state of a domain/domains to not change
 * while you carry out some operation, please consider using
 * @ref CLK_DOMAIN_IS_ACCESS_ENABLED instead.
 */
#define CLK_DOMAINS_DISABLED(_clkDomainMask)                                                     \
        ((CLK_FREQ_DOMAINS_GET() != NULL)?                                                       \
            (((_clkDomainMask) & ((CLK_FREQ_DOMAINS_GET())->clkFreqDomainDisabledMask)) == 0U)   \
            : LW_FALSE)

/*!
 * @brief       Locate FREQ_DOMAIN_GRP for BOARDOBJGRP macros
 * @see         BOARDOBJGRP_OBJ_GET
 * @see         BOARDOBJGRP_GRP_GET
 * @see         CLK_DEFINE_BOARDOBJ_EXTENDED_LEAF_CLASS
 *
 * @details     'BOARDOBJGRP_GRP_GET(ClkFreqDomain)' uses this macro.
 */
#define BOARDOBJGRP_DATA_LOCATION_FREQ_DOMAIN                                 \
    (&Clk.freqDomainGrp.super.super)

/*!
 * @brief       Size of domain array
 * @see         clkFreqDomainArray
 * @see         clkFreqDomain::apiFreqDomainId
 * @see         LW2080_CTRL_CLK_DOMAIN
 * @see         LW2080_CTRL_CLK_FREQ_DOMAIN_STATUS_BASE::clkDomain
 *
 * @details     This is the size of 'clkFreqDomainArray'.
 *
 *              Since 'clkFreqDomainArray' is indexed by LW2080_CTRL_CLK_DOMAIN
 *              constants, and LW2080_CTRL_CLK_DOMAIN is a 32-bit bitmap,
 *              the value of this macro is 32.
 *
 *              Specifically, 'clkFreqDomainArray' is indexed by:
 *                  BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_[dom])
 *              where '[dom]' is the name of the domain.
 */
#define CLK_DOMAIN_ARRAY_SIZE 32

/*!
 * @brief       Define virtual table
 * @memberof    ClkFreqDomain
 * @see         ClkFreqDomain_Virtual
 *
 * @details     This macro defines two per-class items:
 *              1)  the content (using initializers) of the table of virtual
 *                  function pointers for the specified class, and
 *              2)  an inline function to get and downcast the status structure.
 *
 *              The class is named 'Clk[class]' where '[class]' is specified
 *              by '_CLASS', and its dynamic data is stored in a struct named
 *              'RM_PMU_CLK_FREQ_DOMAIN_[schema]_STATUS' where
 *              '[schema]' is specified by _SCHEMA.
 *
 *              The class must declare all the members of 'ClkFreqDomain_Virtual'
 *              in the form of '[fun]_[class]'. For example: 'clkRead_DispFreqDomain'.
 *
 *              In the case where the class inherits an implementation from its
 *              super class, a macro should be used to define an alias.
 *
 *              Usage of this macro should not be followed with a semicolon.
 *
 * @note        When adding a new schema, add it also to clkFreqDomainTypeArray
 *              in litter-specific clk3[chip].c in RM.
 *
 * @warning     After adding a new schema, please check it by running:
 *              //sw/dev/gpu_drv/chips_a/pmu_sw/prod_app/clk/clk3/clksdcheck.c
 *              See source code comments for instructions and information.
 *
 * @param[in]   _CLASS      Bare name of Clocks 3.x class (e.g. 'PllLdivDispFreqDomain')
 */
#ifndef CLK_SD_CHECK        // See pmu_sw/prod_app/clk/clk3/clksdcheck.c
#define CLK_DEFINE_VTABLE_CLK3__DOMAIN(_CLASS)                                 \
const ClkFreqDomain_Virtual clkVirtual_##_CLASS                                \
    GCC_ATTRIB_SECTION("dmem_clk3x", "clkVirtual_" #_CLASS) =                  \
{                                                                              \
    .clkRead        = (ClkRead_FreqDomain_VIP (*))      clkRead_##_CLASS,      \
    .clkConfig      = (ClkConfig_FreqDomain_VIP (*))    clkConfig_##_CLASS,    \
    .clkProgram     = (ClkProgram_FreqDomain_VIP (*))   clkProgram_##_CLASS,   \
};
#endif                      // CLK_SD_CHECK


/*!
 * @brief       Initialize ClkFreqDomain
 * @memberof    ClkFreqDomain
 * @see         clkSchematicDag
 * @see         http://www.open-std.org/JTC1/SC22/wg14/www/docs/n1124.pdf
 *
 * @details     This macro produces the static initializer list to initialize a
 *              'ClkFreqDomain' object.  It is assumed that the macro is used in
 *              the litter-specific source (e.g. 'clksdga100.c') to initialize
 *              'ClkFreqDomain' objects in 'clkSchematicDag'.
 *
 *              The following fields are *not* initialized by this macro and
 *              must by initialized by the schematic dag constructor:
 *              .pVirtual       ClkFreqDomain_Virtual *
 *              .pRoot          ClkFreqSrc *
 *
 *              Members that should are initialized to zero or null are not
 *              included in this macro unless it is defined as a symbolic
 *              constant.  The C standard does this for us.
 *
 *              No comma should follow this macro.
 *
 * @note        This macro functions properly only on the PMU.  Specifically,
 *              designated initializers are not supported by some MSVC versions.
 *
 * @param[in]   _DOMAIN_NAME    Name of the domain
 */
#define CLK_STATIC_INIT__DOMAIN(_DOMAIN_NAME)                                   \
    ._DOMAIN_NAME.output[0].path     = LW2080_CTRL_CMD_CLK_SIGNAL_PATH_EMPTY,   \
    ._DOMAIN_NAME.output[0].fracdiv  = LW_TRISTATE_INDETERMINATE,               \
    ._DOMAIN_NAME.output[0].regimeId = LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID, \
    ._DOMAIN_NAME.output[0].source   = LW2080_CTRL_CLK_PROG_1X_SOURCE_ILWALID,  \
    ._DOMAIN_NAME.output[1].path     = LW2080_CTRL_CMD_CLK_SIGNAL_PATH_EMPTY,   \
    ._DOMAIN_NAME.output[1].fracdiv  = LW_TRISTATE_INDETERMINATE,               \
    ._DOMAIN_NAME.output[1].regimeId = LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID, \
    ._DOMAIN_NAME.output[1].source   = LW2080_CTRL_CLK_PROG_1X_SOURCE_ILWALID,


/*!
 * @brief       Get the statically allocated ClkFreqDomain object
 *
 * @details     Under the assumption that 'clkFreqDomainArray' has 32 elements,
 *              it's impossible to overflow the array with 'BIT_IDX_32'.
 *
 * @param[in]   clkDomain   Domain ID per LW2080_CTRL_CLK_DOMAIN_[dom]
 */
#define CLK_FREQ_DOMAIN_GET(clkDomain)                                         \
    clkFreqDomainArray[BIT_IDX_32(clkDomain)]

/*!
 * @brief       Output buffer for writing
 *
 * @details     This is the buffer which is lwrrently used Clocks 3.1+ logic
 *              (including by clkRead) and being likely updated by that logic.
 */
#define CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(PDOMAIN)                           \
            ((PDOMAIN)->output[clkFreqDomain_DoubleBufferControl.bIsWriteIndexOne])

/*!
 * @brief       Read-only output buffer
 *
 * @details     This is the buffer which is NOT updated by Clocks 3.1+ logic.
 */
#define CLK_OUTPUT_READ_ONLY_BUFFER__FREQDOMAIN(PDOMAIN)                       \
            ((PDOMAIN)->output[!clkFreqDomain_DoubleBufferControl.bIsWriteIndexOne])

/*!
 * @brief       Standard tolerance of 1/2%
 */
/**@{*/
#define CLK_STANDARD_MARGIN__FREQDOMAIN(f)      ((f) / 200)
#define CLK_PLUS_STANDARD_MARGIN__FREQDOMAIN(f) ((f) + CLK_STANDARD_MARGIN__FREQDOMAIN(f))
#define CLK_LESS_STANDARD_MARGIN__FREQDOMAIN(f) ((f) - CLK_STANDARD_MARGIN__FREQDOMAIN(f))
#define CLK_WITHIN_STANDARD_MARGIN__FREQDOMAIN(ft, fc)                          \
    ((ft < CLK_PLUS_STANDARD_MARGIN__FREQDOMAIN(fc)) && (ft >= CLK_LESS_STANDARD_MARGIN__FREQDOMAIN(fc)))
/**@}*/

/*!
 * @brief   Macros to help check if access to the given clock domains is
 *          enabled/disabled
 *
 * All clients shall use following public macros around clock domain HW access
 * (read or program) to make sure the clock domain is not gated/disabled by
 * any client.
 *
 * Client shall pass clock domain mask with bit set for all domains that need to
 * be enabled for the HW access(read/program) to go through.
 * For e.g., we'd need GPC and XBAR clocks to be enabled for accessing GPC clock
 * HW (or in other words registers that lie in GPC domain).
 *
 * Example to depict the usage -
 *
 * LwU32 clkDomainMask =
 *   (LW2080_CTRL_CLK_DOMAIN_GPCCLK | LW2080_CTRL_CLK_DOMAIN_XBARCLK)
 *
 * CLK_DOMAIN_ACCESS_BEGIN(clkDomainMask)
 * {
 *      CLK_DOMAIN_IS_ACCESS_ENABLED(clkDomainMask)
 *      {
 *          // Access enabled
 *      }
 *      CLK_DOMAIN_IS_ACCESS_DISABLED()
 *      {
 *          //
 *          // Access disabled - return cached value for reads and skip
 *          // the operation for program.
 *          //
 *      }
 * }
 * CLK_DOMAIN_ACCESS_END()
 *
 * @prereq  Client is required to attach IMEM overlay "imem_clkLibClk3"
 *
 */
#define CLK_DOMAIN_ACCESS_BEGIN()                                              \
        do                                                                     \
        {                                                                      \
            CHECK_SCOPE_BEGIN(CLK_DOMAIN_ACCESS);

#if PMUCFG_FEATURE_ENABLED(PMU_CLK_DOMAIN_ACCESS_CONTROL)
#define CLK_DOMAIN_IS_ACCESS_ENABLED(_clkDomainMask)                           \
            if (clkFreqDomainsAcquireAccessLock(_clkDomainMask))               \
            {

#define CLK_DOMAIN_IS_ACCESS_DISABLED()                                        \
                clkFreqDomainsReleaseLock();                                   \
            }                                                                  \
            else                                                               \
            {

#else // PMU_CLK_DOMAIN_ACCESS_CONTROL feature disabled
#define CLK_DOMAIN_IS_ACCESS_ENABLED(_clkDomainMask)                           \
            if (LW_TRUE)                                                       \
            {

#define CLK_DOMAIN_IS_ACCESS_DISABLED()                                        \
            }                                                                  \
            else                                                               \
            {
#endif

#define CLK_DOMAIN_ACCESS_END()                                                \
            }                                                                  \
            CHECK_SCOPE_END(CLK_DOMAIN_ACCESS);                                \
        } while (LW_FALSE)

/*!
 * @brief   Helper macro to release read access of clock domains. Here we're really
 *          releasing the semaphore protecting the clock domains disabled mask.
 *
 * @note    Client shall never directly call this interface - should use public
 *          access macros instead.
 *          Check @ref CLK_DOMAIN_ACCESS_BEGIN, CLK_DOMAIN_ACCESS_END,
 *          CLK_DOMAIN_IS_ACCESS_ENABLED and CLK_DOMAIN_IS_ACCESS_DISABLED.
 */
#define clkFreqDomainsReleaseLock()                                                             \
    do                                                                                          \
    {                                                                                           \
        if (PMUCFG_FEATURE_ENABLED(PMU_CLK_DOMAIN_ACCESS_CONTROL))                              \
        {                                                                                       \
            FREQ_DOMAIN_GRP *pClkFreqDomainGrp = CLK_FREQ_DOMAINS_GET();                        \
            if (pClkFreqDomainGrp != NULL)                                                      \
            {                                                                                   \
                OSTASK_OVL_DESC _ovlDescList[] = {                                              \
                    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clkAccessControl)                     \
                };                                                                              \
                OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(_ovlDescList);                           \
                OS_SEMAPHORE_RW_GIVE_RD(pClkFreqDomainGrp->pClkFreqDomainDisableSemaphoreRW);   \
                OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(_ovlDescList);                            \
            }                                                                                   \
            else                                                                                \
            {                                                                                   \
                PMU_BREAKPOINT();                                                               \
            }                                                                                   \
        }                                                                                       \
    } while (LW_FALSE)


/*!
 * @brief       Print dynamic state.
 * @see         clkPrintAll_FreqDomain
 *
 * @details     In builds for which it is enabled, this macro prints available
 *              dynamic state for both software and hardware for all clock domains
 *              by calling `clkPrintAll_FreqDomain`.
 *
 *              In other builds, it has no effect.
 *
 *              `phase` is a parameter for this macros since it is a parameter
 *              in `clkConfig` and `clkProgram` and useful in debugging.  The
 *              values of other local variables should be printed by direct
 *              calls to `CLK_PRINTF` or similar.
 *
 *              This macro should be called only before a breakpoint is fired
 *              because it prints too much info to be useful for any other scenario.
 *
 * @param[in]   phase       Phase number lwrrently being configured or programmed
 */
#if CLK_PRINT_ENABLED
#define CLK_PRINT_ALL(PHASE)     clkPrintAll_FreqDomain(__FILE__, __LINE__, PHASE)
#else
#define CLK_PRINT_ALL(PHASE)     ((void) 0)
#endif


/* ------------------------ Datatypes -------------------------------------- */

typedef struct ClkFreqDomain                ClkFreqDomain;                      //!< One per object -- Statically allocated
typedef struct ClkFreqDomain_Virtual        ClkFreqDomain_Virtual;              //!< One per class -- Statically allocated

// Immediate subclasses of ClkFreqDomain which do not add members
typedef ClkFreqDomain                       ClkVolatileFreqDomain;              //!< Volatile hardware state  -- Supports read
typedef ClkFreqDomain_Virtual               ClkVolatileFreqDomain_Virtual;      //!< Volatile hardware state  -- Supports read
typedef ClkFreqDomain                       ClkNolwolatileFreqDomain;           //!< Cacheable hardware state
typedef ClkFreqDomain_Virtual               ClkNolwolatileFreqDomain_Virtual;   //!< Cacheable hardware state
typedef ClkNolwolatileFreqDomain            ClkProgrammableFreqDomain;          //!< Cacheable hardware state -- Supports read, config, program, and cleanup
typedef ClkNolwolatileFreqDomain_Virtual    ClkProgrammableFreqDomain_Virtual;  //!< Cacheable hardware state -- Supports read, config, program, and cleanup
typedef ClkNolwolatileFreqDomain            ClkFixedFreqDomain;                 //!< Cacheable hardware state -- Supports read
typedef ClkNolwolatileFreqDomain_Virtual    ClkFixedFreqDomain_Virtual;         //!< Cacheable hardware state -- Supports read


/*!
 * @brief       Function Pointer Types for 'ClkFreqDomain_Virtual'
 * @see         ClkFreqDomain_Virtual
 */
/**@{*/
#define ClkRead_FreqDomain_VIP(fname) FLCN_STATUS (fname) (ClkFreqDomain *this)
#define ClkConfig_FreqDomain_VIP(fname) FLCN_STATUS (fname) (ClkFreqDomain *this, const ClkTargetSignal *pTarget)
#define ClkProgram_FreqDomain_VIP(fname) FLCN_STATUS (fname) (ClkFreqDomain *this)
/**@}*/


/*!
 * @brief       Table of virtual interface points
 *
 * @details     For each BOARDOBJ domain class, there is one instance of this
 *              struct which is allocated at compile-time and contains information
 *              about the domain class that never changes.  For the most part,
 *              this means pointers to virtual functions, but there is also the
 *              size of each object.
 *
 *              Each function pointer represents a virtual public interface point
 *              (PIP).  There is a thunk (thin wrapper) for each interface point
 *              to simplify the calling syntax.  By convention, each of these
 *              thunks has the same name as the corresponding pointer with the
 *              suffix _FreqDomain_VIP attached.  Conceptually, these xxx_VIP thunks
 *              are roughly equivalent to the xxx_HAL wrappers used by the
 *              rmconfig code generator.
 *
 *              Clocks 3.x differs from Clocks 2.x in that Clocks 2.x used a
 *              dynamic vtable in which the pointer value would change.  In
 *              contrast, the member function pointers in Clocks 3.x are 'const'
 *              and do not change.
 *
 *              Also, in Clocks 2.x, there are public interface points (PIP) are
 *              not virtual.  However, those interface points are not required
 *              in Clocks 3.x.
 *
 *              The implementations referenced by each pointer may have a slightly
 *              different parameter list v. the pointer itself in that the first
 *              parameter is the specific subclass in the implementation, but
 *              is 'ClkFreqDomain' in the pointer.  For example, since
 *              'ClkFixedFreqDomain <: ClkFreqDomain ', the 'ClkFixedFreqDomain'
 *              implementation of 'clkRead' is:
 *                  FLCN_STATUS clkRead_Fixed(ClkFixedFreqDomain *this, ClkSignal *pOutput)
 *              even though the pointer typedef is:
 *                  FLCN_STATUS ClkRead_FreqDomain_VIP(ClkFreqDomain *this, ClkSignal *pOutput)
 *
 * @note        const:      All structures of this type should be 'const'.
 */
struct ClkFreqDomain_Virtual
{
/*!
 * @brief       Read hardware (unless cache is valid).
 *
 * @details     The software state (i.e. configuration) is computed based on the
 *              state of the hardware.  In other words, the hardware is read for
 *              this domain.
 *
 *              In general, this function is called after initialization.
 *              Assuming that (1) 'clkProgram' sets the software state according
 *              to the hardware state, (2) that the hardware state is not
 *              volatile, and (3) there was no error in programming, this
 *              function does nothing.
 *
 *              This function is pure virtual in 'ClkFreqDomain', but is
 *              implemented in both 'ClkVolatileFreqDomain' and
 *              'ClkNolwolatileFreqDomain'.
 *
 * @pre         'this', 'pOutput' and 'pTarget' may not be NULL.
 *
 * @post        On successful return (FLCN_OK) the status structures in this
 *              domain are up to date and reflect the hardware.
 *
 * @ilwariant   This function does not write any registers.
 *
 * @param[in]   this                ClkFreqDomain-derived object to be read
 */
    ClkRead_FreqDomain_VIP             (*clkRead);

/*!
 * @brief       Configure the domain to the target parameters.
 *
 * @details     This function configures the frequency source object for the
 *              specified target.  In other words, it sets the software state
 *              with a plan for programming the hardware according to the
 *              'pTarget' parameter.
 *
 *              In many cases, the hardware requires multiple phases to
 *              transition from one frequency to another.  For example, some
 *              PLLs should not be programmed while in use, so we must switch
 *              to an alternate (bypass) path in phase one so that we can
 *              reprogram the PLL in phase 2.  Similarly, glitchy multiplexers
 *              cannot by switched while in use.
 *
 *              This function determines how many phases are required and
 *              the state of the intermediate phase,  The 'pTarget' parameter
 *              applies only to the final phase.
 *
 *              If the exact target frequency (pTagret->freqKHz) cannot be
 *              reached, this function configures this object for the nearest
 *              possible frequency it can achieve.  No check is made for
 *              tolerance; it is the responsibility of the 'ClkFreqDomain' object
 *              to make sure the output frequency is close enough.
 *
 *              This function daisy-chains along the schematic dag to configure
 *              the input it will use.  When this function successfully returns
 *              (FLCN_OK or warning), all nodes along the output path have been
 *              correctly configured.
 *
 *
 * @pre         'this', 'pOutput' and 'pTarget' may not be NULL.
 *
 * @pre         minFreqKHz <= target.freqKHz <= maxFreqKHz except when these
 *              values are zero (unspecified), in which case appropriate
 *              defaults are applied.
 *
 * @post        On successful return (FLCN_OK or warning), the software state
 *              reflects the configuration required to be as near as possible
 *              to the requested output signal.  This implies that 'clkConfig'
 *              has been successfully called (daisy-chain) on the input object.
 *
 * @param[in]   this                ClkFreqDomain-derived object to be configured
 * @param[out]  pOutput             Resultant clock signal
 * @param[in]   pTarget             Target frequency, path, etc.
 *
 * @retval      FLCN_ERR_NO_VALID_PATH                Unable to determine a config that meets the target path
 */
    ClkConfig_FreqDomain_VIP       (*clkConfig);

/*!
 * @brief       Program the hardware.
 * @see         clkConfig
 * @see         clkRead
 *
 * @pre         'this' may not be NULL.
 *
 * @post        On successful return (FLCN_OK or warning) the frequency source
 *              objects along the signal path have been programmed to agree with
 *              the internal software state of those objects.
 *
 * @param[in]   this                ClkFreqDomain-derived object to be programmed
 */
    ClkProgram_FreqDomain_VIP      (*clkProgram);
};


/*!
 * @class       ClkFreqDomain
 * @brief       Clock Frequency Domain Class
 * @see         RM_PMU_CLK_FREQ_DOMAIN_BOARDOBJ_GET_STATUS_UNION
 *
 * @details     A "frequency domain" is analogous to a "clock domain" in Clocks
 *              2.x.  We now use "frequency" for two reasons:
 *
 *              1.  OBJPERF has a concept of a "clock domain" with structures
 *                  and purpose differ significantly from OBJCLK.
 *              2.  The main base classes for Clocks 3.x are 'ClkFreqSrc' and
 *                  'ClkFreqDomain'.  It's nice that they are similar in name.
 *
 *              Objects of this class (and subclasses) are instantiated at
 *              compile-time.  Much of the data are const and do not change
 *              after compilation (e.g. register addresses).  Most other data
 *              are set at initialization and do not change after that (e.g. WAR
 *              flags).  Collectively, these data are called "final" which means
 *              "data that does not change after initialization/construction".
 *              The specific requirement, however, for data in these classes is
 *              that they are not interesting for debugging.
 *
 *              Each object has a companion struct which is dynamically allocated
 *              and is defined in RM_PMU_CLK_FREQ_DOMAIN_BOARDOBJ_GET_STATUS_UNION.
 *              These dynamic data are made available to the RM via BOARDOBJ.
 *              There is generally a one-to-one correspondence between class
 *              objects and instances of these structs.  The pointer from the
 *              class object to the RM_PMU_ instance is set during construction.
 *
 *              The planned class hierarchy for this class is:
 *                  ClkFreqDomain
 *                  +-- ClkVolatileFreqDomain
 *                  +-- ClkNolwolatileFreqDomain
 *                      +-- ClkProgrammableFreqDomain
 *                      |   +-- ClkNafllFreqDomain
 *                      |   +-- ClkBifFreqDomain
 *                      |   +-- CLkMClkFreqDomain
 *                      |   +-- ClkPllLdivFreqDomain
 *                      |   +-- ClkOsmFreqDomain
 *                      +-- ClkFixedFreqDomain
 *
 *              Frequency domains can be classified into one of three categories:
 *              - Volatile      - Readable, not cacheable, not programmable
 *              - Programmable  - Readable,     cacheable,     programmable
 *              - Fixed         - Readable,     cacheable, not programmable
 *
 *              It's important to note that these categories do not reflect
 *              the VBIOS table settings or the analogous concepts in OBJPERF.
 *              They speak specifically to operations supported in Clocks 3.x.
 *              For example, a frequency domain may be programmable in Clocks
 *              3.x because there is logic to program it, but be handled as
 *              fixed by OBJPERF.
 *
 * @note        In the case where all the members of a struct are inherited from
 *              the superclass, then they are aliased using 'typedef'.  For
 *              example, 'ClkCacheUpdater' is an alias of 'ClkWire'
 *              because 'ClkCacheUpdater' inherits all its members from
 *              'ClkWire'.  This is typically the case for '_Virtual' types.
 *
 * @note        abstract:  This class neither implements nor inherits any
 *              virtual functions.  As such, it has no vtable and cannot be
 *              instantiated.
 */
struct ClkFreqDomain
{
/*!
 * @brief       Table of virtual interface points (VIPs)
 */
    const ClkFreqDomain_Virtual *pVirtual;

/*!
 * @brief       Domain API ID
 * @see         LW2080_CTRL_CLK_DOMAIN_<xyz>
 *
 * @details     As of the migration to Clocks 3.1, this member is used only by
 *              NAFLL domains to identify a clock counter.  Specicially,
 *              'clkProgram_SingleNafllFreqDomain' and 'clkProgram_MultiNafllFreqDomain'
 *              use this field to call 'clkFreqCountedAvgUpdate', which in turn
 *              calls 's_clkFreqCounterGet' to get the pointer to the relevant
 *              'CLK_FREQ_COUNTER' object in the 'Clk.freqCountedAvg.pFreqCntr'
 *              array.
 *
 * @ilwariant   Exactly one bit of this field must be set.
 */
    LwU32       clkDomain;

/*!
 * @brief       Mask of clients who have disabled the clock domain.
 * @details     When a client disables a particular clock domain, we can't read
 *              or program it until it's re-enabled by the client
 *
 *              Read/write to this mask happens along with read/write to @ref
 *              clkFreqDomainDisabledMask and access to both of them are protected
 *              using a RW semaphore @ref pClkFreqDomainDisableSemaphoreRW.
 *
 * @ref         LW2080_CTRL_CLK_CLIENT_ID_<xyz>
 */
    LwU32       clkFreqDomainDisabledClientMask;

/*!
 * @brief       Double-buffered output signal
 * @see         CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN
 * @see         CLK_OUTPUT_READ_ONLY_BUFFER__FREQDOMAIN
 *
 * @details     This array is a cache of the hardware state.
 *
 *              There are two elements to this array because it is a double-
 *              buffer.  The functions in this class and 'ClkFreqSrc' use
 *              the write half of the double buffer as indicated by the
 *              CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN macro.  As such, they must
 *              be called only from within the single-threaded PERF_DAEMON task.
 *
 *              Furthermore, when 'output.freqKHz' is zero, the cache is in a
 *              reset state and 'clkRead_FreqDomain_VIP' reads the hardware and
 *              updates the write half of this member array.
 *
 *              Otherwise, except for volatile domains, 'clkRead_FreqDomain_VIP'
 *              is a no-op and simply returns.
 *
 *              'clkProgram_FreqDomain_VIP' updates this member when programming
 *              is done.  Assuming no error, this means that 'clkRead_FreqDomain_VIP'
 *              reads hardware only after initialization (except on volatile domains).
 */
    LW2080_CTRL_CMD_CLK_SIGNAL output[2];

/*!
 * @brief       Programmable Phase Array Count
 *
 * @details     The value is assigned by 'clkConfig' and used by 'clkProgram'
 *              to iterate through the phase array.
 */
    ClkPhaseIndex phaseCount;

/*!
 * @brief       Root frequency source object
 * @see         s_clkFreqDomainGrpIfaceModel10SetEntry
 *
 * @details     The output signal for this clock domain is the output signal of
 *              the root object.
 */
    struct ClkFreqSrc *pRoot;
};


/*!
 * @brief       Low-Level Clock frequency Domain Group
 */
typedef struct
{
/*!
 * @brief       Inherited Data
 * @note        Must always be first element in structure.
 */
    BOARDOBJGRP_E32 super;

/*!
 * @brief       Initialization Flags
 *
 * @details     These flags are required to convey some of the run-time decision
 *              making information wrt clock domains from RM to PMU.
 *
 *              Each bit is used for different purposes as represented by
 *              LW_RM_PMU_CLK_INIT_FLAGS_<XXX>
 */
    LwU32       initFlags;

/*!
 * @brief       Supported clock domains by clocks 3.x for the underlying chip
 *
 * @details     This mask indicates the domains constructed by BOARDOBJ and
 *              also supported by this ucode.
 *
 *              If the corresponding bit in this mask is zero, then no memory
 *              has been allocated by BOARDOBJ, ClkFreqDomain::ClkFreqDomain
 *              is NULL, and BOARDOBJ can not be used to query the frequency
 *              domain.
 *
 *              This mask is based on LW2080_CTRL_CLK_DOMAIN_[dom] so that it
 *              can be used to validate clock get/set requests coming from
 *              outside PMU.
 */
    LwU32       clkFreqDomainSupportedMask;

/*!
 * @brief       Mask of clock domains whose access is disabled
 *
 * @details     Mask of clock domain @ref LW2080_CTRL_CLK_DOMAIN_<xyz> tracking
 *              clock domain whose HW access is disabled due to one or more
 *              clients. We also log the clients disabling a particular clock
 *              domain in@ref clkFreqDomainDisabledClientMask.
 *
 *              Read/write to this mask happens along with read/write to @ref
 *              clkFreqDomainDisabledClientMask and access to both of them are
 *              protected using a RW semaphore @ref pClkFreqDomainDisableSemaphoreRW.
 */
    LwU32       clkFreqDomainDisabledMask;

/*!
 * @brief       Read/write semaphore used for synchronizing access to clock domain
 *              mask @ref clkFreqDomainDisabledMask & the mask of clients disabling
 *              a particular clock domains - @ref clkFreqDomainDisabledClientMask.
 *
 * Writer: Client enabling / disabling a clock domain
 * Reader: Any piece of code trying to access the clock domain HW (registers)
 */
    PSEMAPHORE_RW   pClkFreqDomainDisableSemaphoreRW;

} FREQ_DOMAIN_GRP;

/*!
 * @brief       Double Buffer Control
 * @see         CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN
 * @see         CLK_OUTPUT_READ_ONLY_BUFFER__FREQDOMAIN
 * @see         ClkFreqDomain::output
 * @see         clkFlipBuffer_PerfDaemon
 *
 * @note        There is only one instance of this struct.
 */
struct ClkFreqDomain_DoubleBufferControl
{
/*!
 * @brief       Semaphore to Protect Flipping
 */
    PSEMAPHORE_RW   pSemaphore;

/*!
 * @brief       Write Buffer index Indicator
 *
 * @details     When true, the index to the write half of the double buffer is 1.
 *              When false, the index to the write half of the double buffer is 0.
 *              The read-only half, of course, is always the one that is not the
 *              write half.
 *
 *              This indicator is ilwerted by clkFlipBuffer_PerfDaemon and
 *              wrapped by CLK_OUTPUT_READ_ONLY_BUFFER__FREQDOMAIN and
 *              CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN.
 *
 * @note        Do NOT use this field directly.  Instead, be sure
 *              to use CLK_OUTPUT_READ_ONLY_BUFFER__FREQDOMAIN and
 *              CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN.
 */
    LwBool          bIsWriteIndexOne;
};


/* ------------------------ External Definitions --------------------------- */

/*!
 * @brief       Per-class Virtual Table
 *
 * @details     Only concrete classes that are actually instantiated need
 *              virtual tables.
 */
extern const ClkFixedFreqDomain_Virtual clkVirtual_FixedFreqDomain;


/*!
 * @brief       Table of all domains in the schematic
 * @see         LW2080_CTRL_CLK_FREQ_DOMAIN_STATUS_BASE::clkDomain
 * @see         CLK_DOMAIN_ARRAY_SIZE
 * @see         clkConstruct_SchematicDag
 * @see         ClkFreqDomain_Index
 * @see         clkSchematicDag
 *
 * @details     This array is initialized at compile-time.  It is indexed by the
 *              bit index of the LW2080_CTRL_CLK_DOMAIN constant associated with
 *              the domain.  Specifically:
 *                  BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_[dom])
 *              where '[dom]' is the name of the domain.
 *
 *              'LW2080_CTRL_CLK_FREQ_DOMAIN_STATUS_BASE::clkDomain' contains
 *              this value.
 *
 *              Since LW2080_CTRL_CLK_DOMAIN is a 32-bit bitmap, this array
 *              contains 32 elements.
 *
 *              This array is defined and initialized at compile-time in the
 *              litter-specific source file (e.g. 'clksdgv100.c') using C99
 *              designated initializers.
 *
 *              This array is used by 's_clkFreqDomainGrpIfaceModel10SetEntry' and other
 *              BOARDOBJ infrastructure.  In the Clocks 3.x schematic, each
 *              domain is specified by name, so functions in domain classes
 *              can reference the domains without using this array.
 *
 *              NULL indicates a domain that is not supported on this chip.
 *
 *              In the case where chips within the same litter (i.e. build)
 *              have different domains, the litter-specific constructor,
 *              'clkConstruct_SchematicDag', is responsible for applying
 *              chip-specific differences at initialization time.
 *
 *              'clkSchematicDag' contains the 'ClkFreqDomain' objects.
 */
extern ClkFreqDomain *clkFreqDomainArray[CLK_DOMAIN_ARRAY_SIZE];


/*!
 * @brief       Double Buffer Control
 */
extern struct ClkFreqDomain_DoubleBufferControl clkFreqDomain_DoubleBufferControl;


/* ------------ Function Prototypes: Virtual thunks ------------------------ */

/*!
 * @brief       Virtual Interface Point Thunks
 * @see         ClkFreqDomain_Virtual
 * @see         https://wiki.lwpu.com/engwiki/index.php/Resman/RM_Foundations/Lwrrent_Projects/RTOS-FLCN-Scripts#rtos-flcn-script_-_Static_UCode_Analysis_on_objdump
 *
 * @details     Thunks (thin wrappers) used to call public virtual interface points.
 *
 *              In general, virtual implementations should be called via these
 *              'xxx_ClkFreqDomain_VIP' thunks.  In effect, they define the public
 *              virtual interface for 'ClkFreqDomain'.  See comments in
 *              'ClkFreqDomain_Virtual' for each function for detail information.
 *
 *              Any functions that calls one of these VIP functions must be
 *              listed in pmu_sw/build/Analyze.pm to prevent a "Missing mapping
 *              for function pointer" from 'rtos-flcn-script.pl' during the
 *              build process.
 */
/**@{*/
#define clkRead_FreqDomain_VIP(this)                                           \
    ((this)->pVirtual->clkRead((this)))


static inline FLCN_STATUS clkConfig_FreqDomain_VIP(ClkFreqDomain *this, const ClkTargetSignal *pTarget)
{
    return this->pVirtual->clkConfig(this, pTarget);
}


static inline FLCN_STATUS clkProgram_FreqDomain_VIP(ClkFreqDomain *this)
{
    return this->pVirtual->clkProgram(this);
}
/**@}*/


/* ------------ Function Prototypes: Virtual implementations ---------------- */

/*!
 * @brief       Implementations of virtual functions
 * @memberof    ClkVolatileFreqDomain
 * @protected
 */
/**@{*/
// clkConstruct is not implemented (pure virtual).
FLCN_STATUS clkRead_VolatileFreqDomain(ClkVolatileFreqDomain *this)
    GCC_ATTRIB_SECTION("imem_libClkReadPerfDaemon", "clkRead_VolatileFreqDomain");
// clkConfig is not implemented (pure virtual).
// clkProgram is not implemented (pure virtual).
/**@}*/


/*!
 * @brief       Implementations of virtual functions
 * @memberof    ClkNolwolatileFreqDomain
 * @protected
 */
/**@{*/
// clkConstruct is not implemented (pure virtual).
FLCN_STATUS clkRead_NolwolatileFreqDomain(ClkNolwolatileFreqDomain *this)
    GCC_ATTRIB_SECTION("imem_libClkReadPerfDaemon", "clkRead_NolwolatileFreqDomain");
// clkConfig is not implemented (pure virtual).
// clkProgram is not implemented (pure virtual).
/**@}*/



/*!
 * @brief       Implementations of virtual functions
 * @memberof    ClkProgrammableFreqDomain
 * @protected
 */
/**@{*/
// clkConstruct is not implemented (pure virtual).
#define clkRead_ProgrammableFreqDomain      clkRead_NolwolatileFreqDomain   // Inherit
// clkConfig is not implemented (pure virtual).
FLCN_STATUS clkProgram_ProgrammableFreqDomain(ClkProgrammableFreqDomain *this)
    GCC_ATTRIB_SECTION("imem_libClkProgram", "clkProgram_ProgrammableFreqDomain");
/**@}*/



/*!
 * @brief       Implementations of virtual functions
 * @memberof    ClkFixedFreqDomain
 * @protected
 */
/**@{*/
// clkConstruct is not implemented (pure virtual).
#define clkRead_FixedFreqDomain         clkRead_NolwolatileFreqDomain       // Inherit
FLCN_STATUS clkConfig_FixedFreqDomain(ClkFixedFreqDomain *this, const ClkTargetSignal *pTarget)
    GCC_ATTRIB_SECTION("imem_libClkConfig", "clkConfig_FreqDomain");
FLCN_STATUS clkProgram_FixedFreqDomain(ClkFixedFreqDomain *this)
    GCC_ATTRIB_SECTION("imem_libClkProgram", "clkProgram_FixedFreqDomain");
/**@}*/


/* ------------------ Function Prototypes: Nolwirtual  ---------------------- */

// BOARDOBJGRP interfaces
#ifndef CLK_SD_CHECK        // See pmu_sw/prod_app/clk/clk3/clksdcheck.c
BoardObjGrpIfaceModel10CmdHandler  (clkFreqDomainBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkFreqDomainBoardObjGrpIfaceModel10Set");
BoardObjGrpIfaceModel10CmdHandler  (clkFreqDomainBoardObjGrpIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clkFreqDomainBoardObjGrpIfaceModel10GetStatus");
#endif                      // CLK_SD_CHECK

// Protected helpers
FLCN_STATUS clkConfig_CheckTolerance_FreqDomain(ClkFreqDomain *this, FLCN_STATUS status, const ClkTargetSignal *pTarget, const LwRangeU32 *pToleranceKHz)
    GCC_ATTRIB_SECTION("imem_libClkConfig", "clkConfig_CheckTolerance_FreqDomain");

/* ------------------------ Public Functions -------------------------------- */

void clkFreqDomainPreInit(void)
    GCC_ATTRIB_SECTION("imem_init", "clkFreqDomainPreInit");

// Reset output double buffer
FLCN_STATUS clkFreqDomainsResetDoubleBuffer(LwU32 clkDomainMask)
    GCC_ATTRIB_SECTION("imem_clkLibClk3", "clkFreqDomainsResetDoubleBuffer");

//
// Interfaces to enable/disable (HW) access to clock domains
// To-do akshatam: Re-evaluate if this needs an overlay of its own
//
LwBool      clkFreqDomainsAcquireAccessLock(LwU32 clkDomainMask)
    GCC_ATTRIB_SECTION("imem_clkLibClk3", "clkFreqDomainsAcquireAccessLock");
LwBool      clkFreqDomainsEnabledCheck(LwU32 clkDomainMask)
    GCC_ATTRIB_SECTION("imem_clkLibClk3", "clkFreqDomainsEnabledCheck");
FLCN_STATUS clkFreqDomainsDisable(LwU32 clkDomainMask, LwU8 clientId, LwBool bSet, LwBool bConditional)
    GCC_ATTRIB_SECTION("imem_clkLibClk3", "clkFreqDomainsDisable");

// Print dynamic state for all clock domains.
#if CLK_PRINT_ENABLED
void        clkPrintAll_FreqDomain(const char *file, unsigned line, ClkPhaseIndex phase)
    GCC_ATTRIB_SECTION("imem_libClkPrint", "clkPrintAll_FreqDomain");
#endif


/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK3_DOM_FREQDOMAIN_H

