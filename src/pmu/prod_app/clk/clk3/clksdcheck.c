/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*
    === Clocks 3.x Schematic DAG Checker ===

    === Why use this? ===
    This tool prints the schematic DAG as expressed in PMU source code.
    In short, run this when you want to check to make sure everything
    in ClkSchematicDag has been initialized properly.

    Since it is stand-alone, you don't need a PMU.  You don't even need
    a GPU, RM, MODS.  You just need the GCC tool chain in Linux.

    === To compile and run under Linux ===
    Set the following environment variables (or substitute manually):
    WORKSPACE_PATH  The workspace path (e.g. /path/to/sw/dev/gpu_drv/chips_a)
    CHIP_FAMILY     The chip family for finding the manuals (e.g. ampere or turing)
    CHIP            The base chip for the target binary (e.g. ga102 or tu102)

    Enter these commands:
        cd "${WORKSPACE_PATH}/pmu_sw/prod_app/clk/clk3/" &&
        gcc -DSD_${CHIP} -g -I "${WORKSPACE_PATH}/pmu_sw/inc" -I "${WORKSPACE_PATH}/drivers/resman/arch/lwalloc/common/inc" -I "${WORKSPACE_PATH}/drivers/common/inc/hwref/${CHIP_FAMILY}/${CHIP}" -I "${WORKSPACE_PATH}/sdk/lwpu/inc" clksdcheck.c &&
        ./a.out |less

    === To add a Frequency Domain ===
    It's not always clear which classes need which macro call.  Rather than
    doing step 2 p front, it's probably easiest to first compile this tool
    and look at the errors generated.  Specific messages are dislwssed in
    the following sections.

    1.  clkSdCheckFreqDomainNameArray must have an entry for the domain object.
    2.  CLK_SD_CHECK_DEFINE_VTABLE__DOMAIN must be called for its class.

    === To add a Frequency Source ===
    Call exactly one of the following macros based on the super class.
    1.  CLK_SD_CHECK_DEFINE_VTABLE__MUX if it is derived from ClkMux.
    2.  CLK_SD_CHECK_DEFINE_VTABLE__WIRE if it is derived from ClkWire.
    3.  CLK_SD_CHECK_DEFINE_VTABLE__XTAL if it is derived from ClkXtal.
    4.  CLK_SD_CHECK_DEFINE_VTABLE__LEAF otherwise.

    CAUTION: If you get this wrong, it will be diffilwlt to debug since the
    implementations depend on type casting.

    Optionally, if you want additional information printed by this tool for
    your class, you may create your own CLK_SD_CHECK_DEFINE_VTABLE__xxx
    macro and call it.

    === Undefined References to clkVirtual_XXXFreqDomain ===
    If you get an error like this for a ClkFreqDomain subclass:
        undefined reference to `clkVirtual_XXXFreqDomain'
    add a CLK_SD_CHECK_DEFINE_VTABLE__DOMAIN macro call to this file.
    See below.

    === Undefined References to Other clkVirtual ===
    If you get an error like this for a ClkFreqSrc subclass:
        undefined reference to `clkVirtual_'
    add a CLK_SD_CHECK_DEFINE_VTABLE__XXX macro call to this file.
    There are several such macros based on the intermediate class
    in the inheritance tree.  Be sure to use the correct one because
    the casting will not work correctly and you'll get weird results.
    See below.

    === Other Undefined References ===
    If you get some other undefined-reference error, this indicates that
    some object has been added to the schematic DAG initializer.  There
    are two ways to resolve this error.

    a)  #include the .c file that defines the missing object as has been
        with clksignal.c and fs/clkosm.c.  You may need to edit the source
        file so that it doesn't #include any PMU headers such as pmusw.h
        when CLK_SD_CHECK is defined.  See one of those examples.

    b)  Define the object here in this C file.  This is less prefereable since
        it tends to suffer from bitrot.

    === "***" in Report ===
    Problematic issues are bracketed by three asterisks on each side.

    === Missing Domain in Report ===
    clkFreqDomainArray is initialized in the same source file as the schematic
    DAG (e.g. clksdtu10x.c).  If this tool does not report a particular domain,
    then it was probably overlooked in clkFreqDomainArray.

    === "unknown" for a Domain ===
    clkFreqDomainArray contains an incorrect LW2080_CTRL_CLK_DOMAIN_ index.

    === Maximum Per-Path Mux Count ===
    The amount or relwrsion for virtual implementations ClkMux (and subclasses
    of ClkMux) is equal to the largest number of multiplexers of any path in
    the schematic DAG.  Update 'cycleLimits' in pmu_sw/build/Analyze.pm with
    that number for these implementations:
        'clkRead_Mux'
        'clkConfig_Mux'
        'clkProgram_Mux'
        'clkCleanup_Mux'

    Note that the values may vary for each build (i.e. chip family).  All other
    implementations of these virtual functions should be zero. See instructions
    in Analyze.pm for the correct syntax.

    The value of STACK_PERF_DAEMON in pmu_sw/build/OverlaysDmem.pm is a function
    of the deepest path.  If it is too small, the build process issues an error.

    See 'About clkProgram' below for implementation details.

    === Single-Source Compilation ===
    So that we don't have to compile and link several files, C source code files
    are #included in this one.

    === About clkRead ===
    This file contains virtual tables with implementations of clkRead.  Unlike
    the real Clocks 3.x, these implementations crawl the schematic DAG and print
    useful information.  There is no register I/O.

    Key points:
    The clkRead interface has been usurped in each vtable (ClkFreqDomain/ClkFreqSrc).
    This has nothing to do with the actual clkRead contract.
    Most notably, 'pData' replaces the 'pOutput' parameter and is an input (not an output).
    'pData->path' contains the path from the domain to the frequency source object.
    'pData->freqKHz' contains the indentation factor.
    'bIncludePath' replaces 'bActive' to indicate that the path should be printed.
    The return value is the max multiplexer count for ClkFreqDomain sublcasses
    and is meaningless for ClkFreqSrc subclasses.

    === About clkProgram ===
    This file contains virtual tables with implementations of clkProgram for
    subclasses of ClkFreqSrc.  Unlike the real Clocks 3.x, these implementations
    crawl the schematic DAG to determine the maximum per-path mux count.
    There is no register I/O.

    Key points:
    The clkProgram interface has been usurped in each ClkFreqSrc-derived vtable.
    This has nothing to do with the actual clkProgram contract.
    The 'muxCount' parameter replaces 'phase' to contain the count from the domain to the frequency source object.
    The return value indicates the maximum mux count under that frequency source object.
*/

#include <stdio.h>

// Tell source files not to include PMU headers.
#define CLK_SD_CHECK 1

// Fake out BOARDOBJ infrastructure that we do not need
typedef unsigned        BOARDOBJGRP_E32;
typedef unsigned        BOARDOBJ;
typedef unsigned        BoardObjGrpIfaceModel10CmdHandlerFunc;

// Discard GCC attributes
#define GCC_ATTRIB_SECTION(...)

// Halts
#define PMU_HALT        __builtin_trap

// Chip and feature check macros from generated pmu-config.h
#define PMUCFG_CHIP_ENABLED(_C)    PMUCFG_CHIP_##_C
#define PMUCFG_FEATURE_ENABLED(_X) PMUCFG_FEATURE_ENABLED__##_X

// Some features are on a per-chip basis, but others are not.
#define PMUCFG_FEATURE_ENABLED__PMU_CLK_MCLK_SWITCH_SUPPORTED       1

// Common debugging hacks
#define PMU_FAILURE_EX(status, data)    __builtin_trap
#define PMU_FAILURE(status)             __builtin_trap

// No need for asertions
#define ct_assert(...)
#define PMU_ASSERT(...)

// Discard register I/O (sometimes used in debugging)
#define REG_WR32(...)
#define REG_RD32(...)

// The vtables are defined in this source, so discard others.
#define CLK_DEFINE_VTABLE_CLK3__DOMAIN(...)

// From lwmisc.h
#define DRF_SIZE(drf)       (DRF_EXTENT(drf)-DRF_BASE(drf)+1)
#define DRF_EXTENT(drf)     (1?drf)
#define LW_MIN(a, b)        (((a) < (b)) ? (a) : (b))
#define LW_MAX(a, b)        (((a) > (b)) ? (a) : (b))

// From flcntypes.h
#define LW_DELTA(a, b)      (LW_MAX((a), (b)) - LW_MIN((a), (b)))

// Useful
#include "lwtypes.h"


// Also useful
#ifndef LW_OK
#define LW_OK 0
#endif


//
// The chip to be checked determines the features, ClkFreqSrc, and Schematic DAG.
//
#if defined(SD_GV100) ||  defined(SD_gv100)
#define PMUCFG_FEATURE_ENABLED__PMU_DISPLAYLESS 0
#define PMUCFG_FEATURE_ENABLED__PMU_CLK_MCLK_HBM_SUPPORTED 0
#define PMUCFG_FEATURE_ENABLED__PMU_CLK_MCLK_DDR_SUPPORTED 1
#define CLK_SD_CHIP "gv100"
#define PMUCFG_CHIP_VOLTA    1
#define PMUCFG_CHIP_TURING   0
#define PMUCFG_CHIP_AMPERE   0
#include "fs/clkosm.c"
#include "sd/clksdgv100.c"

#elif defined(SD_TU102) || defined(SD_tu102)
#define PMUCFG_FEATURE_ENABLED__PMU_DISPLAYLESS 0
#define PMUCFG_FEATURE_ENABLED__PMU_CLK_MCLK_HBM_SUPPORTED 0
#define PMUCFG_FEATURE_ENABLED__PMU_CLK_MCLK_DDR_SUPPORTED 1
#define CLK_SD_CHIP "tu102"
#define PMUCFG_CHIP_VOLTA    0
#define PMUCFG_CHIP_TURING   1
#define PMUCFG_CHIP_AMPERE   0
#include "fs/clkosm.c"
#include "sd/clksdtu10x.c"

#elif defined(SD_GA100) || defined(SD_ga100)
#define PMUCFG_FEATURE_ENABLED__PMU_DISPLAYLESS 1
#define PMUCFG_FEATURE_ENABLED__PMU_CLK_MCLK_HBM_SUPPORTED 1
#define PMUCFG_FEATURE_ENABLED__PMU_CLK_MCLK_DDR_SUPPORTED 0
#define CLK_SD_CHIP "ga100"
#define PMUCFG_CHIP_VOLTA    0
#define PMUCFG_CHIP_TURING   0
#define PMUCFG_CHIP_AMPERE   1
#include "fs/clkswdiv.c"
#include "dom/clkfreqdomain_mclk.c"     // Needed for clkMuxValueMap_xxx
#include "sd/clksdga100.c"

#elif defined(SD_GA102) || defined(SD_ga102)
#define PMUCFG_FEATURE_ENABLED__PMU_DISPLAYLESS 0
#define PMUCFG_FEATURE_ENABLED__PMU_CLK_MCLK_HBM_SUPPORTED 0
#define PMUCFG_FEATURE_ENABLED__PMU_CLK_MCLK_DDR_SUPPORTED 1
#define CLK_SD_CHIP "ga102"
#define PMUCFG_CHIP_VOLTA    0
#define PMUCFG_CHIP_TURING   0
#define PMUCFG_CHIP_AMPERE   1
#include "fs/clkswdiv.c"
#include "dom/clkfreqdomain_mclk.c"      // Needed for clkMuxValueMap_xxx
#include "dom/clkfreqdomain_swdivhpll.c" // Needed for clkMuxValueMap_xxx
#include "dom/clkfreqdomain_swdivapll.c" // Needed for clkMuxValueMap_xxx
#include "sd/clksdga10x.c"

#else
#error Add '-DSD_chip' to the 'gcc' command line.  'chip' is the schematic to check.
#include "fatal_error"
#endif

// Include source so that we don't have to link other object files.
#include "clksignal.c"
#include "fs/clksppll.c"


// ================ General ====================================================

void clkSdCheckPrintHeader(const char *className, LwU32 indent, ClkSignalPath path, LwBool bIncludePath)
{

    printf("%*s   ", 2 * indent, "");       // Indent

    if (bIncludePath)
    {
        printf("0x%08x:", path);            // Path from the domain to this object
    }
    else
    {
        printf("           ");              // Omit the path, but line everything up
    }

    printf("  class=%s", className);        // Class name
}


// ================ ClkFreqDomain =============================================

const char *clkSdCheckFreqDomainNameArray[CLK_DOMAIN_ARRAY_SIZE] =
{
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_GPCCLK    )]    = "GPCCLK",
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_XBARCLK   )]    = "XBARCLK",
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_SYSCLK    )]    = "SYSCLK",
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_HUBCLK    )]    = "HUBCLK",
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_MCLK      )]    = "MCLK",
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_HOSTCLK   )]    = "HOSTCLK",
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_DISPCLK   )]    = "DISPCLK",
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_PCLK0     )]    = "PCLK0",
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_PCLK1     )]    = "PCLK1",
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_PCLK2     )]    = "PCLK2",
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_PCLK3     )]    = "PCLK3",
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_XCLK      )]    = "XCLK",
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_GPC2CLK   )]    = "GPC2CLK",
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_LTC2CLK   )]    = "LTC2CLK",
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_XBAR2CLK  )]    = "XBAR2CLK",
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_SYS2CLK   )]    = "SYS2CLK",
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_HUB2CLK   )]    = "HUB2CLK",
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_LEGCLK    )]    = "LEGCLK",
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_UTILSCLK  )]    = "UTILSCLK",
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_PWRCLK    )]    = "PWRCLK",
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_LWDCLK    )]    = "LWDCLK",
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_PCIEGENCLK)]    = "PCIEGENCLK",
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_VCLK0     )]    = "VCLK0",
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_VCLK1     )]    = "VCLK1",
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_VCLK2     )]    = "VCLK2",
    [BIT_IDX_32(LW2080_CTRL_CLK_DOMAIN_VCLK3     )]    = "VCLK3",
};


//
// Print useful information about a domain and it's part of the schematic dag.
// The return value is meaningless.
//
FLCN_STATUS clkSdCheckPrint_FreqDomain(const ClkFreqDomain *this, const char *className)
{
    //
    // Print the maximum mux count of in this domain to help in 'cycleLimits'.
    // It would be weird to have a path map but no multiplexers.
    //
    FLCN_STATUS maxMuxCount = 0;
    if (this->pRoot && this->pRoot->pVirtual)
    {
        maxMuxCount = clkProgram_FreqSrc_VIP(this->pRoot, 0);
        if (maxMuxCount)
        {
            printf( "maxMuxCount=%u   ", maxMuxCount);
        }
        else
        {
            printf( "maxMuxCount=zero   ");
        }
    }

    // Check for NULLs before using the vtable to call.
    if (!this->pRoot)
    {
        printf( "  pRoot=***null***");
    }
    else if (!this->pRoot->pVirtual)
    {
        printf( "  pRoot->pVirtual=***null***");
    }

    // Flush in case we get a fault below.
    putchar('\n');
    fflush(stdout);

    // Get info from the root frequency source object.
    if (this->pRoot)
    {
        ClkSignal output = clkReset_Signal;
        clkRead_FreqSrc_VIP(this->pRoot, &output, LW_TRUE);
    }

    // Done
    putchar('\n');
    return maxMuxCount;
}

//
// Use the clkRead inferface to usurp the vtable for ClkFreqDomain subclasses.
//
#define CLK_SD_CHECK_DEFINE_VTABLE__DOMAIN(_CLASS)                             \
FLCN_STATUS clkSdCheck_##_CLASS(const ClkFreqDomain *this)                     \
{                                                                              \
    return clkSdCheckPrint_FreqDomain(this, "Clk" #_CLASS);                    \
}                                                                              \
                                                                               \
const ClkFreqDomain_Virtual clkVirtual_##_CLASS =                              \
{                                                                              \
    .clkRead = clkSdCheck_##_CLASS,                                            \
}

//
// Solve undefined reference to `clkVirtual_XXXFreqDomain' for ClkFreqDomain subclasses
// by adding macro calls for every such concrete class.
//
CLK_SD_CHECK_DEFINE_VTABLE__DOMAIN(SingleNafllFreqDomain);      // clkVirtual_SingleNafllFreqDomain
CLK_SD_CHECK_DEFINE_VTABLE__DOMAIN(MultiNafllFreqDomain);       // clkVirtual_MultiNafllFreqDomain
CLK_SD_CHECK_DEFINE_VTABLE__DOMAIN(APllLdivDispFreqDomain);     // clkVirtual_APllLdivDispFreqDomain
CLK_SD_CHECK_DEFINE_VTABLE__DOMAIN(BifFreqDomain);              // clkVirtual_BifFreqDomain
CLK_SD_CHECK_DEFINE_VTABLE__DOMAIN(MclkFreqDomain);             // clkVirtual_MclkFreqDomain
CLK_SD_CHECK_DEFINE_VTABLE__DOMAIN(OsmFreqDomain);              // clkVirtual_OsmFreqDomain         Turing and before
CLK_SD_CHECK_DEFINE_VTABLE__DOMAIN(SwDivFreqDomain);            // clkVirtual_SwDivFreqDomain       Ampere and after
CLK_SD_CHECK_DEFINE_VTABLE__DOMAIN(SwDivHPllFreqDomain);        // clkVirtual_SwDivHPllFreqDomain   Ampere and after (except Ada)
CLK_SD_CHECK_DEFINE_VTABLE__DOMAIN(SwDivAPllFreqDomain);        // clkVirtual_SwDivAPllFreqDomain   Ada
CLK_SD_CHECK_DEFINE_VTABLE__DOMAIN(StubFreqDomain);             // clkVirtual_StubFreqDomain        Temporary


// ================ ClkMux ====================================================

//
// Print useful information about a multiplexer and its inputs.
//
void clkSdCheckPrint_Mux(const ClkMux *this, LwU32 indent, ClkSignalPath path)
{
    // Compute the union of all masks.
    LwU32 muxValueMapMaskUnion = 0x00000000;
    LwU32 muxValueMapValueUnion = 0x00000000;
    if (this->muxValueMap)
    {
        unsigned i;
        for (i = 0; i < this->count; ++i)
        {
            //
            // SIGFAULT here usually means that 'this->count' doesn't agree
            // with this->muxValueMap.
            //
            muxValueMapMaskUnion  |= this->muxValueMap[i].mask;
            muxValueMapValueUnion |= this->muxValueMap[i].value;
        }
    }

    // Required mux register
    if (this->muxRegAddr)
    {
        printf("  muxReg=0x%08x", this->muxRegAddr);
    }
    else
    {
        printf("  muxReg=***none***");
    }

    // Optional status register
    if (this->statusRegAddr)
    {
        printf("  statusReg=0x%08x", this->statusRegAddr);
    }
    else
    {
        printf("  statusReg=none");
    }

    // Required mux value map with nonzero mask
    if (!this->muxValueMap)
    {
        printf("  muxValueMap=***null***");
    }

    else
    {
        // Zero is bad.
        if (!muxValueMapMaskUnion)
        {
            printf("  muxValueMap.mask=***zero***");
        }
        if (!muxValueMapValueUnion)
        {
            printf("  muxValueMap.value=***zero***");
        }

        // More detail if either is nonzero
        if (muxValueMapMaskUnion || muxValueMapValueUnion)
        {
            // Superfluous bits are in the valu but not the mask.
            if (~muxValueMapMaskUnion & muxValueMapValueUnion)
            {
                printf("  muxValueMap.mask=***0x%08x***  muxValueMap.value=***0x%08x***  superfluous=0x$08x",
                            muxValueMapMaskUnion,
                            muxValueMapValueUnion,
                            ~muxValueMapMaskUnion & muxValueMapValueUnion);
            }

            // The unions are usually equal, but don't ahve to be.
            else if (muxValueMapMaskUnion == muxValueMapValueUnion)
            {
                printf("  muxValueMap.mask=value=0x%08x", muxValueMapMaskUnion);
            }

            // The nonzero values are probably good.
            else
            {
                if (muxValueMapMaskUnion)
                {
                    printf("  muxValueMap.mask=0x%08x", muxValueMapMaskUnion);
                }
                if (muxValueMapValueUnion)
                {
                    printf("  muxValueMap.value=0x%08x", muxValueMapValueUnion);
                }
            }
        }
    }

    // Optional gating
    if (this->muxGateValue.mask || this->muxGateValue.value)
    {
        printf("  gateMask=0x%08x  gateValue=0x%08x",
            this->muxGateValue.mask,
            this->muxGateValue.value);
    }
    else
    {
        printf("  gate=no");
    }

    // Glitchy and input array size
    printf("  glitchy=%s  inputCount=%u",
        (this->glitchy? "yes" : "no"),
        this->count);

    // Missing the input array
    if (!this->input)
    {
        printf("  input=***null***");
    }

    // End mux info
    putchar('\n');

    // Get info from each the input frequency source object
    if (this->input)
    {
        unsigned i;

        // flush the line in case there is a fault later on.
        fflush(stdout);

        // Each input into the mux from the schematic dag
        for (i = 0; i < this->count; ++i)
        {
            //
            // Skip NULLs.
            // SIGFAULT here usually means that 'this->count' doesn't agree
            // with this->input.
            //
            if (this->input[i])
            {
                //
                // Build pData for the clKRead interface.
                // Increase the indentation level for each input object.
                // Tell the input the path from the domain to it.
                //
                ClkSignal data = clkReset_Signal;
                data.freqKHz  = indent + 1;
                data.path   = CLK_PUSH__SIGNAL_PATH(path, i);

                //
                // If the virtual table is bad, say so instead of calling.
                // SIGFAULT here usually means that 'this->count' doesn't agree
                // with this->input or this->input is not legal (and not NULL).
                //
                if (!this->input[i]->pVirtual)
                {
                    printf("%*s   0x%08x:  input[%2u]->pVirtual=***null***\n",
                        2 * data.freqKHz, "",       // Indent
                        data.path,                  // Path to the input
                        i);                         // Array index (head of path)
                }

                // We can safely call the input.
                else
                {
                    clkRead_FreqSrc_VIP(this->input[i], &data, LW_TRUE);
                }
            }
        }
    }
}

//
// Use the clkRead and clkProgram inferfaces to usurp the vtable for ClkMux and its subclasses.
// clkSdCheck_xxx uses clkRead to print useful information.
// clkSdMuxCount_xxx uses clkProgram to determine the maximum mux count.
//
#define CLK_SD_CHECK_DEFINE_VTABLE__MUX(_CLASS)                                \
FLCN_STATUS clkSdCheck_##_CLASS(const ClkFreqSrc *this, ClkSignal *pData, LwBool bIncludePath) \
{                                                                              \
    clkSdCheckPrintHeader("Clk" #_CLASS, pData->freqKHz, pData->path, bIncludePath); \
    clkSdCheckPrint_Mux((const ClkMux *)this, pData->freqKHz, pData->path);    \
    return 0;                                                                  \
}                                                                              \
                                                                               \
FLCN_STATUS clkSdMuxCount_##_CLASS(const ClkFreqSrc *freqsrc, ClkPhaseIndex muxCount) \
{                                                                              \
    const ClkMux *this = (const ClkMux *)freqsrc;                              \
    FLCN_STATUS childCount;                                                    \
    FLCN_STATUS maxCount = 0;                                                  \
    unsigned i;                                                                \
                                                                               \
    /* Add one for this multiplexer. */                                        \
    ++muxCount;                                                                \
                                                                               \
    /* Iterate over each child ignoring those that are not callable. */        \
    for (i = 0; i < this->count; ++i)                                          \
    {                                                                          \
        if (this->input[i] && this->input[i]->pVirtual)                        \
        {                                                                      \
                                                                               \
            /* Get the maximum for each branch. */                             \
            childCount = clkProgram_FreqSrc_VIP(this->input[i], muxCount);     \
            if (maxCount < childCount)                                         \
            {                                                                  \
                maxCount = childCount;                                         \
            }                                                                  \
        }                                                                      \
    }                                                                          \
                                                                               \
    return maxCount;                                                           \
}                                                                              \
                                                                               \
const ClkFreqSrc_Virtual clkVirtual_##_CLASS =                                 \
{                                                                              \
    .clkRead    = clkSdCheck_##_CLASS,                                         \
    .clkProgram = clkSdMuxCount_##_CLASS,                                      \
}

//
// Solve undefined reference to `clkVirtual_XXX' for ClkMux and its subclasses
// by adding macro calls for ClkMux and any concrete subclass.
// ClkMux may be the only one since it typically does not have subclasses.
//
CLK_SD_CHECK_DEFINE_VTABLE__MUX(Mux);                           // clkVirtual_Mux


// ================ ClkWire ===================================================

//
// Print useful information about a frequency source object with one input.
//
void clkSdCheckPrint_Wire(const ClkWire *this, LwU32 indent, ClkSignalPath path)
{
    // Check for invalid vtable on the input.
    if (!this->pInput)
    {
        puts("  pInput=***null***");
    }
    else if (!this->pInput->pVirtual)
    {
        puts( "  pInput->pVirtual=***null***");
    }

    // Print info from the input frequency source object.
    else
    {
        //
        // Build pData for the clKRead interface.
        // Include the indentationdepth ath.
        //
        ClkSignal data  = clkReset_Signal;
        data.freqKHz    = indent;
        data.path       = path;

        // Terminate the object info line
        putchar('\n');

        // Flush in case there is a fault below.
        fflush(stdout);

        // Print info about the input
        clkRead_FreqSrc_VIP(this->pInput, &data, LW_FALSE);
    }
}


//
// Use the clkRead inferface to usurp the vtable for ClkWire subclasses.
//
#define CLK_SD_CHECK_DEFINE_VTABLE__WIRE(_CLASS)                               \
FLCN_STATUS clkSdCheck_##_CLASS(const ClkFreqSrc *this, ClkSignal *pData, LwBool bIncludePath) \
{                                                                              \
    clkSdCheckPrintHeader("Clk" #_CLASS, pData->freqKHz, pData->path, bIncludePath); \
    clkSdCheckPrint_Wire((const ClkWire *)this, pData->freqKHz, pData->path);  \
    return 0;                                                                  \
}                                                                              \
                                                                               \
FLCN_STATUS clkSdMuxCount_##_CLASS(const ClkFreqSrc *freqsrc, ClkPhaseIndex muxCount) \
{                                                                              \
    const ClkWire *this = (const ClkWire *)freqsrc;                            \
    if (this->pInput && this->pInput->pVirtual)                                \
    {                                                                          \
        return clkProgram_FreqSrc_VIP(this->pInput, muxCount);                 \
    }                                                                          \
    else                                                                       \
    {                                                                          \
        return muxCount;                                                       \
    }                                                                          \
}                                                                              \
                                                                               \
const ClkFreqSrc_Virtual clkVirtual_##_CLASS =                                 \
{                                                                              \
    .clkRead    = clkSdCheck_##_CLASS,                                         \
    .clkProgram = clkSdMuxCount_##_CLASS,                                      \
}

//
// Solve undefined reference to `clkVirtual_XXX' for ClkWire subclasses
// by adding macro calls for every such concrete class.
// ClkMux may be the only one since it typically does not have subclasses.
//
CLK_SD_CHECK_DEFINE_VTABLE__WIRE(Pll);                          // clkVirtual_Pll
CLK_SD_CHECK_DEFINE_VTABLE__WIRE(ReadOnly);                     // clkVirtual_ReadOnly
CLK_SD_CHECK_DEFINE_VTABLE__WIRE(LdivUnit);                     // clkVirtual_LdivUnit
CLK_SD_CHECK_DEFINE_VTABLE__WIRE(Sdm);                          // clkVirtual_Sdm


// ================ ClkXtal ====================================================

//
// Use the clkRead inferface to usurp the vtable for ClkXtal subclasses.
//
#define CLK_SD_CHECK_DEFINE_VTABLE__XTAL(_CLASS)                               \
FLCN_STATUS clkSdCheck_##_CLASS(const ClkFreqSrc *this, ClkSignal *pData, LwBool bIncludePath) \
{                                                                              \
    clkSdCheckPrintHeader("Clk" #_CLASS, pData->freqKHz, pData->path, bIncludePath); \
    printf("  freqKHz=%u\n", ((const ClkXtal *)this)->freqKHz);                \
    return 0;                                                                  \
}                                                                              \
                                                                               \
FLCN_STATUS clkSdMuxCount_##_CLASS(const ClkFreqSrc *this, ClkPhaseIndex muxCount) \
{                                                                              \
    return muxCount;                                                           \
}                                                                              \
                                                                               \
const ClkFreqSrc_Virtual clkVirtual_##_CLASS =                                 \
{                                                                              \
    .clkRead = clkSdCheck_##_CLASS,                                            \
    .clkProgram = clkSdMuxCount_##_CLASS,                                      \
}

//
// Solve undefined reference to `clkVirtual_XXX' for other ClkFreqSrc derivatives
// Essentially, this includes ClkXtal derivatives and any others that do not
// have an input and terminate the schematic dag.
//
CLK_SD_CHECK_DEFINE_VTABLE__XTAL(Xtal);                         // clkVirtual_Xtal


// ================ Other ClkFreqSrc Leaf Objects ==============================

//
// Use the clkRead inferface to usurp the vtable.
// This has nothing to do with the actual clkRead contract.
//
#define CLK_SD_CHECK_DEFINE_VTABLE__LEAF(_CLASS)                               \
FLCN_STATUS clkSdCheck_##_CLASS(const ClkFreqSrc *this, ClkSignal *pData, LwBool bActive) \
{                                                                              \
    clkSdCheckPrintHeader("Clk" #_CLASS, pData->freqKHz, pData->path, bActive);\
    putchar('\n');                                                             \
    return 0;                                                                  \
}                                                                              \
                                                                               \
FLCN_STATUS clkSdMuxCount_##_CLASS(const ClkFreqSrc *this, ClkPhaseIndex muxCount) \
{                                                                              \
    return muxCount;                                                           \
}                                                                              \
                                                                               \
const ClkFreqSrc_Virtual clkVirtual_##_CLASS =                                 \
{                                                                              \
    .clkRead = clkSdCheck_##_CLASS,                                            \
    .clkProgram = clkSdMuxCount_##_CLASS,                                      \
}

//
// Solve undefined reference to `clkVirtual_XXX' for other ClkFreqSrc derivatives
// by adding macro calls for any such class that is not defined above.
//
CLK_SD_CHECK_DEFINE_VTABLE__LEAF(Nafll);                        // clkVirtual_Nafll
CLK_SD_CHECK_DEFINE_VTABLE__LEAF(Bif);                          // clkVirtual_Bif



// ================ Main ======================================================

int main()
{
    unsigned i;
    FLCN_STATUS maxMuxCount = 0;

    puts( "Schematic DAG for " CLK_SD_CHIP "\n");

    // Check every domain in the array
    for (i = 0; i < CLK_DOMAIN_ARRAY_SIZE; ++i)
    {
        //
        // clkFreqDomainArray is initialized in the same source file as the
        // schematic dag (e.g. clksdgv100.c).  if an entry is missing, it
        // is skipped here.
        //
        if (clkFreqDomainArray[i])
        {
            //
            // Get the domain name.  If not known, then clkFreqDomainArray
            // probably contains an incorrect LW2080_CTRL_CLK_DOMAIN_XXX index.
            //
            const char *domainName = clkSdCheckFreqDomainNameArray[i];
            if (!domainName)
            {
                 domainName = "***unknown***";
            }

            // Index, LW2080_CTRL_CLK_DOMAIN_XXX, and name.
            printf("\n%s:  idx=%u  id=0x%08x  ", domainName, i, 1UL << i);

            // Just in case we hit a trap later
            fflush(stdout);

            // Call the virtual function unless the vtable is bad.
            if (!clkFreqDomainArray[i]->pVirtual)
            {
                puts("pVirtual=***null***");
            }

            // Accumulate the maximum multiplexer count fo all domains.
            else
            {
                FLCN_STATUS muxCount = clkRead_FreqDomain_VIP(clkFreqDomainArray[i]);
                if (maxMuxCount < muxCount)
                {
                    maxMuxCount = muxCount;
                }
            }
        }
    }

    // Report the maximum multiplexer count.
    printf("\nmaxMuxCount of All Domains:  %u\n", maxMuxCount);
    puts( "Set 'cycleLimits' in pmu_sw/build/Analyze.pm with this amount.");
    puts( "See instructions in pmu_sw/prod_app/clk/clk3/clksdcheck.c.\n");

    // Done
    return 0;
}
