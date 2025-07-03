/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2001-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
// $Id: //sw/integ/gpu_drv/stage_rel/diag/memory/mats2/src/comnmats.cpp#87 $
//------------------------------------------------------------------------------
// 45678901234567890123456789012345678901234567890123456789012345678901234567890
#include "comnmats.h"
#include "matsver.h"
#include "core/include/framebuf.h"
#include "core/include/memerror.h"
#include "core/include/onetime.h"
#include "core/utility/ptrnclss.h"
#include "gpu/framebuf/gpufb.h"
#include "gpu/reghal/reghal.h"
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <lwtypes.h>

#include "kepler/gk104/dev_bus.h"         // LW_PBUS_BAR0_WINDOW
#include "kepler/gk104//dev_master.h"     // LW_PMC_BOOT_0

// This is just plain evil, but it is necessary because we need the defintion
// for the Mats class.  Alternatively, we could put the header in a separate
// mats.h file, but that's just a pain to maintain
#include "gpu/tests/matshelp.h"
#include "gpu/tests/matshelp.cpp"
#include "gpu/tests/mats.cpp"

/*!
 * EFI uses BAR1 CPU physical address to access FB. BAR1
 * is in physical mode during EFI. So set max EFI FB size to
 * 128MB (BAR1 size for GK107)
 */
#define EFI_MAX_FB_SIZE 128

static MatsTest s_MatsInst;
static int s_TestType = 0;
static GpuDevMgr s_GpuDevMgr;
GpuDevMgr* DevMgrMgr::d_GraphDevMgr = &s_GpuDevMgr;

FrameBuffer *g_Ptr = 0;

//------------------------------------------------------------------------------
// Global variables.
//------------------------------------------------------------------------------
bool  g_NoScanOut = false;
string g_OutFile = "report.txt";
char s_Buff[4096];
static UINT32 *s_MatsLwBase = 0;
static UINT32 *g_MatsFbBase = 0;
FILE *g_pFile = 0;

enum s_SupportedPCIECLASSCODE
{
    //Check http://wiki.osdev.org/PCI#Class_Codes
    CLASSVGACTRL = 0x30000,
    CLASS3DCTRL  = 0x30200
};

UINT32 *g_VgaSavePtr = 0;
bool g_SaveAndRestoreVga = true;
const UINT32 VGA_MEM_SIZE_UINT32S = 8192;
const UINT32 BAR0_WINDOW_SIZE = 0x100000;

UINT32 g_ChipId = 0;

bool g_UseFileOutput     = true;
bool g_UseScreenOutput   = true;

volatile UINT32 g_UserAbort = 0;

FpContext g_TheContext;
Random    g_TheRandom;

namespace Xp {};
RC Platform::DisableUserInterface
(
    UINT32 w,
    UINT32 h,
    UINT32 d,
    UINT32 r,
    UINT32 junk1,
    UINT32 junk2,
    UINT32 junk3,
    GpuDevice* junk4
)
{
    if (g_SaveAndRestoreVga)
    {
        g_VgaSavePtr = new UINT32[VGA_MEM_SIZE_UINT32S];
        for (UINT32 i = 0; i< VGA_MEM_SIZE_UINT32S; i++)
            g_VgaSavePtr[i] = g_MatsFbBase[i];
    }

    return OK;
}
void Platform::EnableUserInterface(GpuDevice *p)
{
    if (g_SaveAndRestoreVga && g_VgaSavePtr)
    {
        for (UINT32 i = 0; i< VGA_MEM_SIZE_UINT32S; i++)
            g_MatsFbBase[i] = g_VgaSavePtr[i];
        delete[] g_VgaSavePtr;
    }
    g_VgaSavePtr = 0;
}

FpContext * GetFpContext()
{
    g_TheContext.Rand = g_TheRandom;
    return &g_TheContext;
};

void ParseCommandLineArgs(int argc, char **argv);

static GpuDevice    s_GpuDevice;
static GpuSubdevice s_GpuSubdevice;
static auto_ptr<RegHal> s_pRegHal;
static string       s_GpuName;

//------------------------------------------------------------------------------
// DRF Macros
//------------------------------------------------------------------------------

// Register access functions.
UINT32 REG_RD32(UINT32 a)
{
    const volatile UINT08* const ptr =
        reinterpret_cast<UINT08*>(s_MatsLwBase) + a;
    return *reinterpret_cast<const volatile UINT32*>(ptr);
}

void REG_WR32(UINT32 a, UINT32 d)
{
    volatile UINT08* const ptr = reinterpret_cast<UINT08*>(s_MatsLwBase) + a;
    *reinterpret_cast<volatile UINT32*>(ptr) = d;
}

//------------------------------------------------------------------------------
// Macros
//------------------------------------------------------------------------------

#define LW_SIZE (16*1024*1024)
#define FB_SIZE (VGA_MEM_SIZE_UINT32S * sizeof(UINT32))

//------------------------------------------------------------------------------
// Ctrl-C interrupt (SIGINT) handler.
//------------------------------------------------------------------------------
static void CtrlCHandler
(
    int /* */
)
{
    g_UserAbort = 1;
}

//------------------------------------------------------------------------------
// Random number generator
//------------------------------------------------------------------------------
/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that: (1) source distributions retain this entire copyright
 * notice and comment, and (2) distributions including binaries display
 * the following acknowledgement:  ``This product includes software
 * developed by the University of California, Berkeley and its contributors''
 * in the documentation or other materials provided with the distribution
 * and in all advertising materials mentioning features or use of this
 * software. Neither the name of the University nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*
 * random.c:
 * An improved random number generation package.  In addition to the standard
 * rand()/srand() like interface, this package also has a special state info
 * interface.  The initstate() routine is called with a seed, an array of
 * bytes, and a count of how many bytes are being passed in; this array is then
 * initialized to contain information for random number generation with that
 * much state information.  Good sizes for the amount of state information are
 * 32, 64, 128, and 256 bytes.  The state can be switched by calling the
 * setstate() routine with the same array as was initiallized with initstate().
 * By default, the package runs with 128 bytes of state information and
 * generates far better random numbers than a linear congruential generator.
 * If the amount of state information is less than 32 bytes, a simple linear
 * congruential R.N.G. is used.
 * Internally, the state information is treated as an array of longs; the
 * zeroeth element of the array is the type of R.N.G. being used (small
 * integer); the remainder of the array is the state information for the
 * R.N.G.  Thus, 32 bytes of state information will give 7 longs worth of
 * state information, which will allow a degree seven polynomial.  (Note: the
 * zeroeth word of state information also has some other information stored
 * in it -- see setstate() for details).
 * The random number generation technique is a linear feedback shift register
 * approach, employing trinomials (since there are fewer terms to sum up that
 * way).  In this approach, the least significant bit of all the numbers in
 * the state table will act as a linear feedback shift register, and will have
 * period 2^deg - 1 (where deg is the degree of the polynomial being used,
 * assuming that the polynomial is irreducible and primitive).  The higher
 * order bits will have longer periods, since their values are also influenced
 * by pseudo-random carries out of the lower bits.  The total period of the
 * generator is approximately deg*(2**deg - 1); thus doubling the amount of
 * state information has a vast influence on the period of the generator.
 * Note: the deg*(2**deg - 1) is an approximation only good for large deg,
 * when the period of the shift register is the dominant factor.  With deg
 * equal to seven, the period is actually much longer than the 7*(2**7 - 1)
 * predicted by this formula.
 */

/*
 * For each of the lwrrently supported random number generators, we have a
 * break value on the amount of state information (you need at least this
 * many bytes of state info to support this random number generator), a degree
 * for the polynomial (actually a trinomial) that the R.N.G. is based on, and
 * the separation between the two lower order coefficients of the trinomial.
 */

#define         TYPE_0          0               /* linear congruential */
#define         BREAK_0         8
#define         DEG_0           0
#define         SEP_0           0

#define         TYPE_1          1               /* x**7 + x**3 + 1 */
#define         BREAK_1         32
#define         DEG_1           7
#define         SEP_1           3

#define         TYPE_2          2               /* x**15 + x + 1 */
#define         BREAK_2         64
#define         DEG_2           15
#define         SEP_2           1

#define         TYPE_3          3               /* x**31 + x**3 + 1 */
#define         BREAK_3         128
#define         DEG_3           31
#define         SEP_3           3

#define         TYPE_4          4               /* x**63 + x + 1 */
#define         BREAK_4         256
#define         DEG_4           63
#define         SEP_4           1

/*
 * Array versions of the above information to make code run faster -- relies
 * on fact that TYPE_i == i.
 */

// #define MAX_TYPES 5 /* max number of types above */
// static int degrees[MAX_TYPES]   = { DEG_0, DEG_1, DEG_2, DEG_3, DEG_4 };
// static int seps[MAX_TYPES] = { SEP_0, SEP_1, SEP_2, SEP_3, SEP_4 };
//

/*
 * Initially, everything is set up as if from :
 *              initstate(1, &randtbl, 128);
 * Note that this initialization takes advantage of the fact that srandom()
 * advances the front and rear pointers 10*rand_deg times, and hence the
 * rear pointer which starts at 0 will also end up at zero; thus the zeroeth
 * element of the state information, which contains info about the current
 * position of the rear pointer is just
 *      MAX_TYPES*(rptr - state) + TYPE_3 == TYPE_3.
 */

static unsigned long randtbl[DEG_3 + 1] = { TYPE_3,
                            0x9a319039U, 0x32d9c024U, 0x9b663182U, 0x5da1f342U,
                            0xde3b81e0U, 0xdf0a6fb5U, 0xf103bc02U, 0x48f340fbU,
                            0x7449e56bU, 0xbeb1dbb0U, 0xab5c5918U, 0x946554fdU,
                            0x8c2e680fU, 0xeb3d799fU, 0xb11ee0b7U, 0x2d436b86U,
                            0xda672e2aU, 0x1588ca88U, 0xe369735dU, 0x904f35f7U,
                            0xd7158fd6U, 0x6fa6f051U, 0x616e6b96U, 0xac94efdlw,
                            0x36413f93U, 0xc622c298U, 0xf5a42ab8U, 0x8a88d77bU,
                                         0xf5ad9d0eU, 0x8999220bU, 0x27fb47b9U };

/*
 * fptr and rptr are two pointers into the state info, a front and a rear
 * pointer.  These two pointers are always rand_sep places aparts, as they cycle
 * cyclically through the state information.  (Yes, this does mean we could get
 * away with just one pointer, but the code for random() is more efficient this
 * way).  The pointers are left positioned as they would be from the call
 *                      initstate(1, randtbl, 128)
 * (The position of the rear pointer, rptr, is really 0 (as explained above
 * in the initialization of randtbl) because the state table pointer is set
 * to point to randtbl[1] (as explained below).
 */

static  long            *fptr           = (long *) &randtbl[ SEP_3 + 1 ];
static  long            *rptr           = (long *) &randtbl[ 1 ];

/*
 * The following things are the pointer to the state information table,
 * the type of the current generator, the degree of the current polynomial
 * being used, and the separation between the two pointers.
 * Note that for efficiency of random(), we remember the first location of
 * the state information, not the zeroeth.  Hence it is valid to access
 * state[-1], which is used to store the type of the R.N.G.
 * Also, we remember the last location, since this is more efficient than
 * indexing every time to find the address of the last element to see if
 * the front and rear pointers have wrapped.
 */

static  long            *state          = (long *)&randtbl[ 1 ];
// static  int             rand_type       = TYPE_3;
static  int             rand_deg        = DEG_3;
static  int             rand_sep        = SEP_3;
static  long            *end_ptr        = (long *) &randtbl[ DEG_3 + 1 ];

#if defined (DEBUG)
int randomsThisSeed;
#endif
//------------------------------------------------------------------------------
// WHY MACROS INSTEAD OF CONST?
//
// GCC computes initializer expressions such as (1.0 / TwoExp32) at
// compile time.  VC++ doesn't compute them until runtime (pre-main).
//
// Unfortunately, we have code in this file that is called pre-main --
// PreparePickItems() is called by the STATIC_PI_PREP constructor.
//
// With VC++, depending on the link order, PreparePickItems() can be called
// before the constants in this file are initialized!
//
// Using a macro for TwoExp32Ilw sidesteps this problem.
//------------------------------------------------------------------------------
const double TwoExp32      = 4294967296.0;  // 0x100000000 (won't fit in UINT32)
const double TwoExp32Less1 = 4294967295.0;  //  0xffffffff
#define TwoExp32Ilw        (1.0/TwoExp32)
#define TwoExp32Less1Ilw   (1.0/TwoExp32Less1)
/*
 * srandom:
 * Initialize the random number generator based on the given seed.  If the
 * type is the trivial no-state-information type, just remember the seed.
 * Otherwise, initializes state[] based on the given "seed" via a linear
 * congruential generator.  Then, the pointers are set to known locations
 * that are exactly rand_sep places apart.  Lastly, it cycles the state
 * information a given number of times to get rid of any initial dependencies
 * introduced by the L.C.R.N.G.
 * Note that the initialization of randtbl[] for default usage relies on
 * values produced by this routine.
 */
/*
 * random:
 * If we are using the trivial TYPE_0 R.N.G., just do the old linear
 * congruential bit.  Otherwise, we do our fancy trinomial stuff, which is the
 * same in all ther other cases due to all the global variables that have been
 * set up.  The basic operation is to add the number at the rear pointer into
 * the one at the front pointer.  Then both pointers are advanced to the next
 * location cyclically in the table.  The value returned is the sum generated,
 * reduced to 31 bits by throwing away the "least random" low bit.
 * Note: the code takes advantage of the fact that both the front and
 * rear pointers can't wrap on the same call by not testing the rear
 * pointer if the front one has wrapped.
 * Returns a 31-bit random number.
 */
// 6-15-2000 Charlie Davies -- I modified this for returning 32-bit unsigned.
UINT32 Random::GetRandom()
{
    UINT32 i;

    // if (rand_type == TYPE_0)
    // {
    //   i = state[0] = (state[0]*1103515245 + 12345)&0x7fffffff;
    // }
    // else
    {
        *fptr += *rptr;
        // i = (*fptr >> 1)&0x7fffffff; /* chucking least random bit */
        i = (UINT32) *fptr;
        if (++fptr >= end_ptr)
        {
            fptr = state;
            ++rptr;
        }
        else
        {
            if (++rptr >= end_ptr)
                rptr = state;
        }
    }
#if defined(DEBUG)
    randomsThisSeed++;
#endif

    return i;
}
//------------------------------------------------------------------------------
UINT32 Random::GetRandom
(
    UINT32 milwalue,
    UINT32 maxValue
)
{
    MASSERT(milwalue <= maxValue);

    // Generate the next psuedo-random number.
    UINT32 val = GetRandom();

    // Offset and scale the random number to fit the range requested.
    return milwalue + (UINT32(val * (double(maxValue - milwalue) + 1.0) *
                              TwoExp32Ilw));
}
//------------------------------------------------------------------------------
// Shuffle the "deck" in place.
// If numSwaps == 0, goes through the deck exactly once (decksize-1 swaps).
// If (0 < numSwaps < deckSize-1), only the first numSwaps items are guaranteed
// to be random.
void Random::Shuffle
(
    UINT32   deckSize,
    UINT32 * pDeck,
    UINT32   numSwaps /* =0 */
)
{
    MASSERT( pDeck );

    if ( numSwaps == 0 )
        numSwaps = deckSize-1;

    // The outer loop lets us to skip a % operation in the inner loop,
    // and still allow double shuffling by paranoid programmers.

    while ( numSwaps )
    {
        UINT32 numSwapsThisLoop = numSwaps;

        if ( numSwapsThisLoop > deckSize-1 )
            numSwapsThisLoop = deckSize-1;

        UINT32 swap;
        for (swap = 0; swap < numSwapsThisLoop; ++swap )
        {
            UINT32 idx = GetRandom(swap, deckSize-1);

            UINT32 tmp  = pDeck[swap];
            pDeck[swap]  = pDeck[idx];
            pDeck[idx]   = tmp;
        }
        numSwaps -= numSwapsThisLoop;
    }
}

void Random::SeedRandom(UINT32 x)
{
    state[ 0 ] = (long)x;
    for (int i = 1; i < rand_deg; i++)
    {
        state[i] = 1103515245*state[i - 1] + 12345;
    }
    fptr = &state[rand_sep];
    rptr = &state[0];
    for (int i = 0; i < 10*rand_deg; i++)
        GetRandom();

#if defined (DEBUG)
    randomsThisSeed = 0;
#endif
}

//------------------------------------------------------------------------------
// PrintHelp
//------------------------------------------------------------------------------

static void PrintHelp( void )
{
    printf("\nMATS is short for Modified Algorithmic Test Sequence\n");
    printf("Based on an algorithm described in 'March tests for word-oriented memories'\n");
    printf("by A.J. van de Goor, Delft University, the Netherlands\n\n");
    printf("\nMATS Options:\n");
    printf("-e xx        Forces endpoint of test to megabyte xx\n");
    printf("-c xx        Only test xx percent of the framebuffer.\n");
    printf("-b xx        Forces start of test to megabyte xx\n");
    printf("-t xx        Use an alternate memory test algorithm (xx=1 to 7)\n");
    printf("               0 = Box Mats test (default)   4 = March test\n");
    printf("               1 = Linear Mats test          5 = 64-bit pattern test\n");
    printf("               2 = Address-On-Data           6 = MOD3 test\n");
    printf("               3 = March test                7 = Knaizuk-Hartmann\n");
    printf("-o           Suppress file output\n");
    printf("-m xx        Set the maximum number of errors to log\n");
    printf("-3d_card     Run the mats test on a 3d_card card\n");
    printf("-n xx        Run the algorithm on the card with index xx\n");
    printf("-v           Disable save & restore of VGA memory space\n");
    printf("-maxfbmb     Set the maximum amount of memory to test in MB\n");
    printf("-no_scan_out Disable acccess for VGA region while running mats.\n");
    printf("-logfile     Set the output file name for the logs\n");
    printf("-ver         Display version information\n");
    printf("-x           Don't use the box algorithm\n");
    exit(1);
}

#define BEGIN_TEST(x)                                               \
    {                                                               \
        MatsTest &lwr = x;                                          \
        jsval lwr_seq;                                              \
        lwr.m_PatternTable.clear();                                 \
        lwr.m_TestCommands.clear();

#define END_TEST                                                    \
    }

#define DECL_PATTERNS(pat)                                          \
    UINT32 pat_siz = sizeof(pat) / sizeof(pat[0]);                  \
    for (UINT32 i = 0; i < pat_siz; ++i)                            \
        lwr.m_PatternTable.push_back(jsval(pat[i]));

#define DECL_SEQUENCE(str, dir, fmt, siz)                           \
    lwr_seq.clear();                                                \
    lwr_seq.jval.push_back(jsval(str));                             \
    lwr_seq.jval.push_back(jsval(dir));                             \
    lwr_seq.jval.push_back(jsval(fmt));                             \
    lwr_seq.jval.push_back(jsval(siz));                             \
    lwr.m_TestCommands.push_back(lwr_seq);

static const MemoryDirection MemoryUp   = MEMDIR_UP;
static const MemoryDirection MemoryDown = MEMDIR_DOWN;
static const MemoryFormat LinearNormal  = MEMFMT_LINEAR;
static const MemoryFormat LinearBounded = MEMFMT_LINEAR_BOUNDED;
static const MemoryFormat BoxNormal     = MEMFMT_BOX;
static const MemoryFormat BoxBounded    = MEMFMT_BOX_BOUNDED;

// the following Setup* routines are hardcoded copies of the MATS tests
// found in gpu/js/gpuargs.js

void SetupBoxTest()
{
    UINT64 patterns[] = { 0x00000000, 0xFFFFFFFF, 0x55555555, 0xAAAAAAAA,
                          0x33333333, 0xCCCCCCCC, 0xF0F0F0F0, 0x0F0F0F0F,
                          0x00FF00FF, 0xFF00FF00, 0x0000FFFF, 0xFFFF0000 };

    s_MatsInst.m_UpperBound = 250;

    BEGIN_TEST(s_MatsInst)
        DECL_PATTERNS( patterns )
        DECL_SEQUENCE( "W0+",      MemoryUp,   BoxNormal,  32 )
        DECL_SEQUENCE( "R0+W1+",   MemoryDown, BoxNormal,  32 )
        DECL_SEQUENCE( "R1+W2+",   MemoryUp,   BoxNormal,  32 )
        DECL_SEQUENCE( "R2+W3+",   MemoryDown, BoxNormal,  32 )
        DECL_SEQUENCE( "R3+W2+",   MemoryUp,   BoxBounded, 32 )
        DECL_SEQUENCE( "R2+W4+",   MemoryDown, BoxBounded, 32 )
        DECL_SEQUENCE( "R4+W5+",   MemoryUp,   BoxBounded, 32 )
        DECL_SEQUENCE( "R5+W6+",   MemoryDown, BoxBounded, 32 )
        DECL_SEQUENCE( "R6+W7+",   MemoryUp,   BoxBounded, 32 )
        DECL_SEQUENCE( "R7+W8+",   MemoryDown, BoxBounded, 32 )
        DECL_SEQUENCE( "R8+W9+",   MemoryUp,   BoxBounded, 32 )
        DECL_SEQUENCE( "R9+W10+",  MemoryDown, BoxBounded, 32 )
        DECL_SEQUENCE( "R10+W11+", MemoryUp,   BoxBounded, 32 )
        DECL_SEQUENCE( "R11+W10+", MemoryDown, BoxBounded, 32 )
    END_TEST
}

void SetupByteTest()
{
    UINT64 patterns[] = { 0x00, 0xff, 0x55, 0xaa, 0x33, 0x08, 0xa0, 0x00,
                          0x90, 0xa2, 0x01, 0xe0, 0x23 };

    s_MatsInst.m_UpperBound  = 4096;

    BEGIN_TEST(s_MatsInst)
        DECL_PATTERNS( patterns )
        DECL_SEQUENCE( "W0+",      MemoryUp,   LinearNormal,  8 )
        DECL_SEQUENCE( "R0+W1+",   MemoryDown, LinearNormal,  8 )
        DECL_SEQUENCE( "R1+W2+",   MemoryUp,   LinearNormal,  8 )
        DECL_SEQUENCE( "R2+W3+",   MemoryDown, LinearNormal,  8 )
        DECL_SEQUENCE( "R3+W4+",   MemoryUp,   LinearBounded, 8 )
        DECL_SEQUENCE( "R4+W5+",   MemoryDown, LinearBounded, 8 )
        DECL_SEQUENCE( "R5+W6+",   MemoryUp,   LinearBounded, 8 )
        DECL_SEQUENCE( "R6+W7+",   MemoryDown, LinearBounded, 8 )
        DECL_SEQUENCE( "R7+W8+",   MemoryUp,   LinearBounded, 8 )
        DECL_SEQUENCE( "R8+W9+",   MemoryDown, LinearBounded, 8 )
        DECL_SEQUENCE( "R9+W10+",  MemoryUp,   LinearBounded, 8 )
        DECL_SEQUENCE( "R10+W11+", MemoryDown, LinearBounded, 8 )
        DECL_SEQUENCE( "R11+W12+", MemoryUp,   LinearBounded, 8 )
        DECL_SEQUENCE( "R12+",     MemoryDown, LinearBounded, 8 )
    END_TEST
}

void SetupMarchTest()
{
    UINT64 patterns[] = { 0x00000000, 0xffffffff };

    BEGIN_TEST(s_MatsInst)
        DECL_PATTERNS( patterns )
        DECL_SEQUENCE( "W0+/R0W1+R1+", MemoryUp,   LinearNormal, 32 )
        DECL_SEQUENCE( "R1W0+R0+",     MemoryDown, LinearNormal, 32 )
        DECL_SEQUENCE( "W1+/R1W0+R0+", MemoryUp,   LinearNormal, 32 )
        DECL_SEQUENCE( "R0W1+R1+",     MemoryDown, LinearNormal, 32 )
    END_TEST
}

void SetupAODTest()
{
    BEGIN_TEST(s_MatsInst)
        DECL_SEQUENCE( "WA+/RA+", MemoryUp, LinearNormal, 32 )
    END_TEST
}

void SetupPatternTest()
{
    UINT64 patterns[] = { 0xdeadbeefdeadbeefULL, 0x0000000000000000ULL,
                          0xFFFFFFFFFFFFFFFFULL, 0xAAAAAAAAAAAAAAAAULL,
                          0x5555555555555555ULL, 0x3939393939393939ULL,
                          0x5A5A5A5A5A5A5A5AULL };

    BEGIN_TEST(s_MatsInst)
        DECL_PATTERNS( patterns )
        DECL_SEQUENCE( "W0+/R0+", MemoryUp, LinearNormal, 64 )
        DECL_SEQUENCE( "W1+/R1+", MemoryUp, LinearNormal, 64 )
        DECL_SEQUENCE( "W2+/R2+", MemoryUp, LinearNormal, 64 )
        DECL_SEQUENCE( "W3+/R3+", MemoryUp, LinearNormal, 64 )
        DECL_SEQUENCE( "W4+/R4+", MemoryUp, LinearNormal, 64 )
        DECL_SEQUENCE( "W5+/R5+", MemoryUp, LinearNormal, 64 )
        DECL_SEQUENCE( "W6+/R6+", MemoryUp, LinearNormal, 64 )
    END_TEST
}

void SetupAppleMOD3Test()
{
    UINT64 patterns[] = { 0x6db6db6dULL,  0xb6db6db6ULL,  0xdb6db6dbULL,
                         ~0x6db6db6dULL, ~0xb6db6db6ULL, ~0xdb6db6dbULL };

    BEGIN_TEST(s_MatsInst)
        DECL_PATTERNS( patterns )
        DECL_SEQUENCE( "W0+W1+W2+/R0+R1+R2+", MemoryUp, LinearNormal, 32 )
        DECL_SEQUENCE( "W3+W4+W5+/R3+R4+R5+", MemoryUp, LinearNormal, 32 )
        DECL_SEQUENCE( "W1+W2+W0+/R1+R2+R0+", MemoryUp, LinearNormal, 32 )
        DECL_SEQUENCE( "W4+W5+W3+/R4+R5+R3+", MemoryUp, LinearNormal, 32 )
        DECL_SEQUENCE( "W2+W0+W1+/R2+R0+R1+", MemoryUp, LinearNormal, 32 )
        DECL_SEQUENCE( "W5+W3+W4+/R5+R3+R4+", MemoryUp, LinearNormal, 32 )
    END_TEST
}

void SetupAppleAddrTest()
{
    BEGIN_TEST(s_MatsInst)
        DECL_SEQUENCE( "WA+/RA+", MemoryUp, LinearNormal, 32 )
        DECL_SEQUENCE( "WI+/RI+", MemoryUp, LinearNormal, 32 )
    END_TEST
}

void SetupAppleKHTest()
{
    UINT64 patterns[] = { 0x00000000, 0xffffffff };

    BEGIN_TEST(s_MatsInst)
        DECL_PATTERNS( patterns )
        DECL_SEQUENCE( "WS+W0+W0+", MemoryUp, LinearNormal, 32 )
        DECL_SEQUENCE( "W1+WS+WS+", MemoryUp, LinearNormal, 32 )
        DECL_SEQUENCE( "RS+R0+RS+", MemoryUp, LinearNormal, 32 )
        DECL_SEQUENCE( "WS+W1+WS+", MemoryUp, LinearNormal, 32 )
        DECL_SEQUENCE( "RS+RS+R0+", MemoryUp, LinearNormal, 32 )
        DECL_SEQUENCE( "R1+R1+RS+", MemoryUp, LinearNormal, 32 )
        DECL_SEQUENCE( "W0+WS+WS+", MemoryUp, LinearNormal, 32 )
        DECL_SEQUENCE( "R0+RS+RS+", MemoryUp, LinearNormal, 32 )
        DECL_SEQUENCE( "WS+WS+W1+", MemoryUp, LinearNormal, 32 )
        DECL_SEQUENCE( "RS+RS+R1+", MemoryUp, LinearNormal, 32 )
    END_TEST
}

//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------

int main
(
    int argc,
    char *argv[]
)
{
    GpuSubdevice *pGpuSubdevice = GetBoundGpuSubdevice();
    UINT32 domainNumber;
    UINT32 busNumber;
    UINT32 deviceNumber;
    UINT32 functionNumber;
    UINT32 physLwBase;
    UINT32 physFbBase;
    UINT32 index;
    StickyRC rc;

    if (!OpenDriver())
        return 1;

    index = 0;
    UINT32 classCode = CLASSVGACTRL; //By default expect display
    string noticeMessage =
        "NOTICE: For secondary gpus make sure you have run ./mods gputest.js "
        "-skip_rm_state_init first.\n";
    bool isNoticeReqd = false;
    for (INT32 i = 1; i < argc; ++i)
    {
        if ( !strcmp( argv[i], "-3d_card" ))
        {
            //3d controller class is expected
            classCode = CLASS3DCTRL;
            printf( "Running mats on a 3d controller card\n");
            isNoticeReqd = true;
        }
        if ( !strcmp( argv[i], "-n" ) )
        {
            index = atoi(argv[++i]);
            printf( "Running mats on card with index %d.\n",
                    index );
            isNoticeReqd = true;
        }
    }
    if (isNoticeReqd)
    {
        printf("%s\n", noticeMessage.c_str());
    }

    //--------------------------------------------------------------------------
    // Find a video controller.
    //--------------------------------------------------------------------------

    if (!FindVideoController(index, classCode, &domainNumber, &busNumber,
                             &deviceNumber, &functionNumber))
    {
        printf( "Could not find a video controller."
                " If you are running mats on 3d controller please try"
                " ./mats -3d_card\n");
        return 1;
    }

    s_GpuName = Utility::StrPrintf("GPU %s%d [",
                                   (classCode == CLASS3DCTRL) ? "3D " : "",
                                   index);
    if (domainNumber != 0)
    {
        s_GpuName += Utility::StrPrintf("%04x:", domainNumber);
    }
    s_GpuName += Utility::StrPrintf("%02x:%02x.%x]",
                                    busNumber, deviceNumber, functionNumber);

    // Make sure that memory access cycles are enabled...or this is going
    // to be a very short trip!
    if (0 == (PciRead32(domainNumber, busNumber,
                        deviceNumber, functionNumber, 0x4) & 0x2))
    {
        printf( "Found an LW device at domain %d bus %d device %d, but its memory cycle "
                "enable bit in\nPCI configuration space is turned off.\n",
                domainNumber, busNumber, deviceNumber);
        return 1;
    }

    //--------------------------------------------------------------------------
    // Map register and frame buffer memory spaces.
    //--------------------------------------------------------------------------

    physLwBase = PciRead32(domainNumber, busNumber,
                           deviceNumber, functionNumber, 0x10) & 0xFFF00000;
    physFbBase = PciRead32(domainNumber, busNumber,
                           deviceNumber, functionNumber, 0x14) & 0xFFF00000;

    if ( !MapPhysicalMemory( physLwBase, LW_SIZE, &s_MatsLwBase) )
    {
        printf( "Failed to map register memory space.\n" );
        return 1;
    }

    if ( !MapPhysicalMemory( physFbBase, FB_SIZE, &g_MatsFbBase) )
    {
        printf( "Failed to map frame buffer memory space.\n" );
        return 1;
    }

    g_ChipId = ((REG_RD_DRF(_PMC, _BOOT_0, _ARCHITECTURE) <<
                 DRF_SIZE(LW_PMC_BOOT_0_IMPLEMENTATION)) |
                REG_RD_DRF(_PMC, _BOOT_0, _IMPLEMENTATION));

    TestDevice* pTestDevice = static_cast<TestDevice*>(pGpuSubdevice);
    s_pRegHal.reset(new RegHal(pTestDevice)); // Requires g_ChipId
    s_pRegHal->Initialize();

    //--------------------------------------------------------------------------
    // Execute command line arguments.
    //--------------------------------------------------------------------------

    void (*pOriginalCtrlCHandler)(int) = signal(SIGINT, CtrlCHandler);

    Gpu::LwDeviceId dutId = pGpuSubdevice->DeviceId();

    const char *gpuName = 0;
    RegHal &regs = pGpuSubdevice->Regs();
    UINT32 vgaBaseReg = 0;
    if (g_NoScanOut)
    {
        vgaBaseReg = regs.Read32(MODS_PDISP_VGA_BASE);
    }

    switch (dutId)
    {
#define DEFINE_NEW_GPU( DIDStart, DIDEnd, ChipId, GpuId, Constant,   \
                        Family, SubdeviceType, FrameBufferType, ...) \
        case Gpu::GpuId:                                             \
            g_Ptr = FrameBufferType(pGpuSubdevice, nullptr);         \
            gpuName = # GpuId;                                       \
            break;
#define DEFINE_OBS_GPU(...) // don't care
#define DEFINE_DUP_GPU(...) // don't care
#include "gpu/include/gpulist.h"
#undef DEFINE_DUP_GPU
#undef DEFINE_NEW_GPU
#undef DEFINE_OBS_GPU
        case Gpu::LW0:
        default:
            Printf(Tee::PriNormal, "This card is not recognized\n");
            CHECK_RC_CLEANUP(RC::CANNOT_SET_STATE);
    }

    g_Ptr->Initialize();

    s_MatsInst.m_EndLocation = g_Ptr->GetGraphicsRamAmount() >> 20;

    ParseCommandLineArgs(argc, argv);

#ifdef EFI_MATS
    s_MatsInst.m_EndLocation =
        s_MatsInst.m_EndLocation > EFI_MAX_FB_SIZE ?
        EFI_MAX_FB_SIZE : s_MatsInst.m_EndLocation;
#endif

    if (g_NoScanOut)
    {
        regs.Write32(MODS_PDISP_VGA_BASE_STATUS_ILWALID);
    }

    if (g_UseFileOutput)
        g_pFile = fopen(g_OutFile.c_str(), "w");

    Printf(Tee::PriNormal,
           "%s version %s.  Testing %s with %d MB of memory starting with %d MB.\n",
           PRODUCT,
           VERSION,
           gpuName, s_MatsInst.m_EndLocation - s_MatsInst.m_StartLocation,
           s_MatsInst.m_StartLocation);

    GetMemError(0).SetMaxErrors(s_MatsInst.m_MaxErrors);
    GetMemError(0).SetupTest(pGpuSubdevice->GetFB());
    GetMemError(0).SuppressHelpMessage();

    switch (s_TestType)
    {
        case 0: SetupBoxTest();       break;
        case 1: SetupByteTest();      break;
        case 2: SetupAODTest();       break;
        case 3: SetupMarchTest();     break;
        case 4: SetupPatternTest();   break;
        case 5: SetupAppleAddrTest(); break;
        case 6: SetupAppleMOD3Test(); break;
        case 7: SetupAppleKHTest();   break;
        default:
            Printf(Tee::PriError, "Invalid test type (%d) specified.\n",
                   s_TestType);
            break;
    }

    // disable the status bar because it does wonky things in VGA mode
    s_MatsInst.m_ShowStatus = false;

    CHECK_RC_CLEANUP(s_MatsInst.Setup());
    rc = s_MatsInst.Run();
    rc = GetMemError(0).ResolveMemErrorResult();

    if (rc <= RC::BAD_MEMORY)
    {
        Printf(Tee::PriNormal, "\n");
        GetMemError(0).PrintStatus();
        Printf(Tee::PriNormal, "\n");
        GetMemError(0).ErrorBitsDetailed();
        Printf(Tee::PriNormal, "\n");
        GetMemError(0).PrintErrorLog(s_MatsInst.m_MaxErrors);
        if (rc == RC::BAD_MEMORY)
        {
            Printf(Tee::PriWarn, "If you are getting failure for first MB"
                   " of FB then try option -no_scan_out");
        }
    }
    else
    {
        Printf(Tee::PriNormal, "An error oclwrred while running mats.  "
               "See output file for details.\n");
    }

Cleanup:
    s_MatsInst.Cleanup();
    if (g_NoScanOut)
    {
        regs.Write32(MODS_PDISP_VGA_BASE, vgaBaseReg);
    }

    if (rc == OK)
    {
        Printf(Tee::PriNormal, "\nError Code = %08d (%s)\n\n", rc.Get(), "OK");
        Printf(Tee::PriNormal, Tee::ModuleNone, Tee::SPS_PASS,
                "                                        \n"
                " #######     ####     ######    ######  \n"
                " ########   ######   ########  ######## \n"
                " ##    ##  ##    ##  ##     #  ##     # \n"
                " ##    ##  ##    ##   ###       ###     \n"
                " ########  ########    ####      ####   \n"
                " #######   ########      ###       ###  \n"
                " ##        ##    ##  #     ##  #     ## \n"
                " ##        ##    ##  ########  ######## \n"
                " ##        ##    ##   ######    ######  \n"
                "                                        \n"
              );

    }
    else
    {
        Printf(Tee::PriNormal, "\nError Code = %08d \n\n", rc.Get());
        Printf(Tee::PriNormal, Tee::ModuleNone, Tee::SPS_FAIL,
                "                                        \n"
                " #######     ####    ########  ###      \n"
                " #######    ######   ########  ###      \n"
                " ##        ##    ##     ##     ###      \n"
                " ##        ##    ##     ##     ###      \n"
                " #######   ########     ##     ###      \n"
                " #######   ########     ##     ###      \n"
                " ##        ##    ##     ##     ###      \n"
                " ##        ##    ##  ########  ######## \n"
                " ##        ##    ##  ########  ######## \n"
                "                                        \n"
              );
    }

    // restore the original handler
    signal(SIGINT, pOriginalCtrlCHandler);

    if (g_UseFileOutput)
    {
        fclose(g_pFile);
        g_pFile = 0;
    }

    // This function restores the VGA text display.  Even though it
    // should have already been called in mats.cpp, we want to call it
    // again here in case the user hit control-C.
    Platform::EnableUserInterface(0);

    CloseDriver();

    if (g_Ptr)
        delete g_Ptr;

    return g_UserAbort ? RC::USER_ABORT : rc.Get();
}

UINT32 Printf
(
    INT32        priority,
    const char * format,
    ... //       arguments
)
{
    va_list arguments;
    va_start(arguments, format);
    UINT32 charactersWritten = vsprintf(s_Buff, format, arguments);
    va_end(arguments);

    if (priority >= Tee::PriNormal)
    {
        if (g_pFile)
            fprintf(g_pFile, "%s", s_Buff);
        if (g_UseScreenOutput)
            printf("%s", s_Buff);
    }
    return charactersWritten;
}

UINT32 Printf
(
    INT32        priority,
    UINT32       module,
    UINT32       sps,
    const char * format,
    ... //       Arguments
)
{
    va_list arguments;
    va_start(arguments, format);
    UINT32 charactersWritten = vsprintf(s_Buff, format, arguments);
    va_end(arguments);

    if (priority < Tee::PriNormal)
    {
        return charactersWritten;
    }
    if (g_pFile)
        fprintf(g_pFile, "%s", s_Buff);

    string fmtBuff;
    const char* pBuff = s_Buff;
    const char* colorEscape = "";
    const char* const colorOff = "\033[0m";

    if (g_UseScreenOutput)
    {
        if (sps == Tee::SPS_PASS)
            colorEscape = "\033[1;39;42m";
        else if (sps == Tee::SPS_FAIL)
            colorEscape = "\033[1;39;41m";

        while (pBuff)
        {
            fmtBuff.resize(0);
            fmtBuff += colorEscape;
            const char* const eol = strchr(pBuff, '\n');
            if (eol)
            {
                fmtBuff.append(pBuff, eol-pBuff);
                pBuff = eol + 1;
            }
            else
            {
                fmtBuff += pBuff;
                pBuff = nullptr;
            }
            if (*colorEscape)
            {
                fmtBuff += colorOff;
            }
            if (eol)
            {
                fmtBuff += "\n";
            }

            if (fmtBuff.size() !=
                fwrite(fmtBuff.c_str(), 1, fmtBuff.size(), stdout))
            {
                MASSERT(!"Unable to write to stdout!");
                break;
            }
        }
    }
    return charactersWritten;
}
//------------------------------------------------------------------------------
// Utility namespace
//------------------------------------------------------------------------------
RC GpuUtility::AllocateEntireFramebuffer
(
    UINT64 minChunkSize,
    UINT64 maxChunkSize,
    UINT64 maxSize,
    MemoryChunks *pChunks,
    bool blockLinear,
    UINT64 minPageSize,
    UINT64 maxPageSize,
    UINT32 hChannel,
    GpuDevice *pGpuDev,
    UINT32 minPercentReqd,
    bool contiguous)
{
    MASSERT(pChunks);
    FreeEntireFramebuffer(pChunks);
    RC  rc = OK;
    FbRanges fbRange;
    UINT64   allocatedSoFar = 0;

    MemoryChunkDesc desc;
    desc.pteKind      = 0;  // ???
    desc.partStride   = 256;// ???
    desc.pageSizeKB   = 4;    //Physical accesses are always 4k, says Elaine Tam
    desc.size         = 0;
    desc.fbOffset     = 0;

    CHECK_RC(g_Ptr->GetFbRanges(&fbRange));
    for (size_t rangesIdx = 0; rangesIdx < fbRange.size(); rangesIdx++)
    {
        bool bTerminateLoop = false;
        desc.fbOffset = fbRange[rangesIdx].start;
        desc.size = fbRange[rangesIdx].size;
        if ((allocatedSoFar + desc.size) > maxSize)
        {
            desc.size = maxSize - allocatedSoFar;
            bTerminateLoop = true;
        }

        allocatedSoFar += desc.size;
        pChunks->push_back(desc);
        if (bTerminateLoop)
            break;
    }

    MASSERT(desc.size >= minChunkSize);

    Printf(Tee::PriLow,
           "AllocateEntireFramebuffer allocated %u bytes of memory\n",
           allocatedSoFar);

    return OK;
}

RC GpuUtility::FreeEntireFramebuffer(MemoryChunks *pChunks)
{
    MASSERT(pChunks);

    pChunks->clear();

    return OK;
}

INT32 Utility::CountBits ( UINT32 value )
{
    INT32 count;
    for (count = 0; value != 0; ++count)
    {
        value &= (value - 1U);      // Chop of the least significant bit.
    }
    return count;
}

INT32 Utility::BitScanForward
(
    UINT32 value,
    INT32  start // = 0
)
{
    if ((value == 0) || (start < 0) || (start > 31))
        return -1;

    INT32 count;
    for (count = start; count < 32; ++count)
    {
        if (value & (1 << count))
            return count;
    }

    MASSERT(false);
    return -1;
}

INT32 Utility::BitScanForward64(UINT64 value, INT32 start)
{
    static const INT32 deBruijnBitPosition[64] =
    {
         0,  1, 45,  2, 36, 46, 59,  3, 56, 37, 40, 47, 60, 11,  4, 28,
        43, 57, 38,  9, 41, 15, 48, 17, 61, 53, 12, 50, 24,  5, 19, 29,
        63, 44, 35, 58, 55, 39, 10, 27, 42,  8, 14, 16, 52, 49, 23, 18,
        62, 34, 54, 26,  7, 13, 51, 22, 33, 25,  6, 21, 32, 20, 31, 30
    };

    if (start >= 0 && start <= 63)
    {
        value &= ~0ULL << start;
        if (value != 0)
        {
            // Multiplicative hash function to map the pattern to 0..63
            return deBruijnBitPosition[static_cast<UINT64>(
                    (value & ~(value - 1)) * 0x03a6af73f1285b23ULL) >> 58];
        }
    }

    return -1;
}

INT32 Utility::BitScanReverse(UINT32 x)
{
    static const INT32 mapNumBits[32] =
    {
        21, 27, 12, 25, 16, 18,  8, 20, 11, 15,  7, 10,  6,  5,  4, 31,
         0, 22,  1, 28, 23, 13,  2, 29, 26, 24, 17, 19, 14,  9,  3, 30
    };

    if (0 != x)
    {
        // first round up to the next 'power of 2 - 1'
        x |= x >> 1;
        x |= x >> 2;
        x |= x >> 4;
        x |= x >> 8;
        x |= x >> 16;

        // Use a multiplicative hash function to map the pattern to 0..31, then
        // use this number to find the first bit set by means of a lookup table.
        // The multiplicative constant was found empirically. Among all
        // constants the one that groups small indices closer together was
        // chosen.
        return mapNumBits[
            static_cast<UINT32>(x * 0x87dcd629U) >> 27
        ];
    }

    return -1;
}

INT32 Utility::FindNthSetBit(UINT32 value, INT32 n)
{
    if (n < 0)
        return -1;
    for (INT32 ii = 0; ii < n; ++ii)
        value &= value - 1;
    if (value == 0)
        return -1;
    return BitScanForward(value);
}

UINT32 Utility::Log2i(UINT32 num)
{
    if (num == 0 || (num & (num - 1)) != 0)
        return 0xffffffff;
    return static_cast<UINT32>(BitScanForward(num));
}

void Utility::CheckMaxLimit(UINT64 value, UINT64 maxLimit, const char *file, int line)
{
    if (value > maxLimit)
    {
        Printf(Tee::PriError, "Value too large to fit in variable at %s line %d\n", file, line);
        MASSERT(!"Variable overload");
    }
}

string Utility::StrPrintf(const char * format, ...)
{
    // Hopefully no one wants to use StrPrintf with a bigger size...
    // With a properly implemented vsnprintf implementation we could actually
    // ask how much space is needed, then dynamically allocate exactly that
    // much.  However, we don't have a real vsnprintf implementation on all
    // platforms, so we're out of luck.
    const size_t MAX_SIZE = 4096;
    char buf[MAX_SIZE];

    va_list arguments;
    va_start(arguments, format);

    vsnprintf(buf, MAX_SIZE, format, arguments);

    va_end(arguments);

    return buf;
}

//------------------------------------------------------------------------------
// Memory namespace
//------------------------------------------------------------------------------
RC Memory::FillRgb
(
    void * address,
    UINT32 width,
    UINT32 height,
    UINT32 depth,
    UINT32 pitch
)
{
    return OK;
}

//----------------------------------------------------------------------------
// Tasker namespace
//----------------------------------------------------------------------------
bool Tasker::IsInitialized() { return true; }
RC   Tasker::Yield()         { return OK; }

//------------------------------------------------------------------------------
// Misc functions needed by stand-alone MATS
//------------------------------------------------------------------------------
void ParseCommandLineArgs(int argc, char **argv)
{
    int i = 1;
    while (i < argc)
    {
        if (argv[i][0] == '/')
            argv[i][0] = '-';

        if      (0 == strcmp (argv[i], "-?"))
            PrintHelp();
        else if (0 == strcmp (argv[i], "?" ))
            PrintHelp();
        else if (0 == strcmp (argv[i], "-h"))
            PrintHelp();

        else if (0 == strcmp (argv[i], "-n"))
            ++i; //we already handled this
        else if (0 == strcmp (argv[i], "-3d_card"))
            ; //we already handled this
        else if (0 == strcmp (argv[i], "-v"))
            g_SaveAndRestoreVga = false;
        else if (0 == strcmp (argv[i], "-o"))
            g_UseFileOutput = false;
        else if (0 == strcmp (argv[i], "-t"))
        {
            if (++i >= argc)
            {
                Printf(3, "-t requires a parameter\n");
                exit(1);
            }
            s_TestType = atoi(argv[i]);

            if ((s_TestType == 0) || (s_TestType > 7 ))
            {
                Printf(3, "-t requires a value from 1 to 7\n");
                exit(1);
            }
        }
        else if (0 == strcmp (argv[i], "-e"))
        {
            if (++i >= argc)
            {
                Printf(3, "-e requires a parameter\n");
                exit(1);
            }
            s_MatsInst.m_EndLocation = atoi(argv[i]);
        }
        else if (0 == strcmp (argv[i], "-b"))
        {
            if (++i >= argc)
            {
                Printf(3, "-b requires a parameter\n");
                exit(1);
            }
            s_MatsInst.m_StartLocation = atoi(argv[i]);
        }
        else if (0 == strcmp (argv[i], "-m"))
        {
            if (++i >= argc)
            {
                Printf(3, "-m requires a parameter\n");
                exit(1);
            }
            s_MatsInst.m_MaxErrors=atoi(argv[i]);
        }
        else if (0 == strcmp (argv[i], "-c"))
        {
            if (++i >= argc)
            {
                Printf(3, "-c requires a parameter\n");
                exit(1);
            }
            s_MatsInst.m_Coverage = atof(argv[i]);
        }

        else if (0 == strcmp (argv[i], "-maxfbmb"))
        {
            if (++i >= argc)
            {
                Printf(3, "-maxfbmb requires a parameter\n");
                exit(1);
            }
            s_MatsInst.m_MaxFbMb = atoi(argv[i]);
        }
        else if (0 == strcmp (argv[i], "-no_scan_out"))
        {
            g_NoScanOut = true;
        }
        else if (0 == strcmp (argv[i], "-logfile"))
        {
            if (++i >= argc)
            {
                Printf(3, "-logfile name requires a parameter\n");
                exit(1);
            }
            g_OutFile = argv[i];
        }
        else if (0 == strcmp (argv[i], "-ver"))
        {
            printf("%s version %s\n", PRODUCT, VERSION);
            exit(1);
        }

        else
        {
            printf("ERROR:  Could not parse argument %s\n", argv[i]);
            exit(1);
        }
        i++;
    }
}

UINT32 FloorsweepImpl::FbMask() const
{
    return FbpMask();
}

UINT32 FloorsweepImpl::FbpMask() const
{
    const RegHal &regs = GetBoundGpuSubdevice()->Regs();
    const UINT32 disableMask =
        regs.IsSupported(MODS_FUSE_STATUS_OPT_FBP) ?
        regs.Read32(MODS_FUSE_STATUS_OPT_FBP) :
        regs.Read32(MODS_PTOP_FS_STATUS_FBP);
    const UINT32 maxFbps     = regs.Read32(MODS_PTOP_SCAL_NUM_FBPS);
    return ~disableMask & ((1 << maxFbps) - 1);
}

UINT32 FloorsweepImpl::FbioMask() const
{
    const RegHal &regs = GetBoundGpuSubdevice()->Regs();
    const UINT32 disableMask =
        regs.IsSupported(MODS_FUSE_STATUS_OPT_FBIO) ?
        regs.Read32(MODS_FUSE_STATUS_OPT_FBIO) :       // Maxwell+
        regs.Read32(MODS_PTOP_FS_STATUS_FBIO);         // Fermi-Kepler
    const UINT32 maxFbios =
        regs.IsSupported(MODS_PTOP_SCAL_NUM_FBPAS) ?
        regs.Read32(MODS_PTOP_SCAL_NUM_FBPAS) :
        regs.Read32(MODS_PTOP_SCAL_NUM_FBPS);
    return ~disableMask & ((1 << maxFbios) - 1);
}

UINT32 FloorsweepImpl::FbioshiftMask() const
{
    const RegHal &regs = GetBoundGpuSubdevice()->Regs();
    if (regs.IsSupported(MODS_FUSE_STATUS_OPT_FBIO_SHIFT))
        return regs.Read32(MODS_FUSE_STATUS_OPT_FBIO_SHIFT);
    else if (regs.IsSupported(MODS_PTOP_FS_STATUS_FBIO_SHIFT))
        return regs.Read32(MODS_PTOP_FS_STATUS_FBIO_SHIFT);
    else
        return 0;
}

UINT32 FloorsweepImpl::FbpaMask() const
{
    const RegHal &regs = GetBoundGpuSubdevice()->Regs();
    const UINT32 disableMask =
        regs.IsSupported(MODS_FUSE_STATUS_OPT_FBIO) ?
        regs.Read32(MODS_FUSE_STATUS_OPT_FBPA) :       // Maxwell+
        regs.Read32(MODS_PTOP_FS_STATUS_FBPA);         // Fermi-Kepler
    const UINT32 maxFbpas =
        regs.IsSupported(MODS_PTOP_SCAL_NUM_FBPAS) ?
        regs.Read32(MODS_PTOP_SCAL_NUM_FBPAS) :
        regs.Read32(MODS_PTOP_SCAL_NUM_FBPS);
    return ~disableMask & ((1 << maxFbpas) - 1);
}

UINT32 FloorsweepImpl::L2Mask(UINT32 hwFbpNum) const
{
    UINT32 mask = 0;
    if ((1 << hwFbpNum) & FbpMask())
    {
        const RegHal &regs = GetBoundGpuSubdevice()->Regs();

        UINT32 disableMask = 0;
        if (regs.IsSupported(MODS_FUSE_STATUS_OPT_LTC_FBP, hwFbpNum))
        {
            disableMask = regs.Read32(MODS_FUSE_STATUS_OPT_LTC_FBP, hwFbpNum);
        }
        else if (regs.IsSupported(MODS_FUSE_STATUS_OPT_ROP_L2_FBP, hwFbpNum))
        {
            disableMask = regs.Read32(MODS_FUSE_STATUS_OPT_ROP_L2_FBP, hwFbpNum);
        }
        const UINT32 maxLtcs =
            regs.IsSupported(MODS_PTOP_SCAL_NUM_LTC_PER_FBP) ?
            regs.Read32(MODS_PTOP_SCAL_NUM_LTC_PER_FBP) : 0;
        mask = ~disableMask & ((1 << maxLtcs) - 1);
    }
    if (mask == 0)
    {
        Printf(Tee::PriWarn, "L2Mask() returns 0 for Physical Fbp Num %u!",
                hwFbpNum);
    }
    return mask;
}

bool FloorsweepImpl::SupportsL2SliceMask(UINT32 hwFbpNum) const
{
    const RegHal &regs = GetBoundGpuSubdevice()->Regs();
    return regs.IsSupported(MODS_FUSE_STATUS_OPT_L2SLICE_FBP, hwFbpNum);
}

UINT32 FloorsweepImpl::L2SliceMask(UINT32 hwFbpNum) const
{
    UINT32 mask = 0;
    if ((1 << hwFbpNum) & FbpMask())
    {
        const RegHal &regs = GetBoundGpuSubdevice()->Regs();
        if (regs.IsSupported(MODS_FUSE_STATUS_OPT_L2SLICE_FBP, hwFbpNum))
        {
            const UINT32 disableMask     = regs.Read32(MODS_FUSE_STATUS_OPT_L2SLICE_FBP, hwFbpNum);
            const UINT32 maxSlicesPerLtc = regs.LookupAddress(MODS_SCAL_LITTER_NUM_SLICES_PER_LTC);
            const UINT32 maxLtcsPerFbp =
                regs.IsSupported(MODS_PTOP_SCAL_NUM_LTC_PER_FBP) ?
                regs.Read32(MODS_PTOP_SCAL_NUM_LTC_PER_FBP) : 0;
            const UINT32 maxSlicesPerFbp = maxSlicesPerLtc * maxLtcsPerFbp;
            mask = ~disableMask & ((1 << maxSlicesPerFbp) - 1);
        }
    }
    if (mask == 0)
    {
        Printf(Tee::PriWarn, "L2SliceMask() returns 0 for Physical Fbp Num %u!",
                hwFbpNum);
    }
    return mask;
}

UINT32 FloorsweepImpl::HalfFbpaMask() const
{
    const RegHal &regs = GetBoundGpuSubdevice()->Regs();
    return (regs.IsSupported(MODS_FUSE_CTRL_OPT_HALF_FBPA_DATA) ?
            regs.Read32(MODS_FUSE_CTRL_OPT_HALF_FBPA_DATA) : 0);
}

UINT32 FloorsweepImpl::GetHalfFbpaWidth() const
{
    const RegHal &regs = GetBoundGpuSubdevice()->Regs();
    const UINT32 maxFbps  = regs.Read32(MODS_PTOP_SCAL_NUM_FBPS);
    const UINT32 maxFbpas = regs.Read32(MODS_PTOP_SCAL_NUM_FBPAS);
    return (maxFbps != 0 ? (maxFbpas / maxFbps) : 0);
}

GpuSubdevice::GpuSubdevice() :
    m_FsImpl(this)
{
}

void * GpuSubdevice::EnsureFbRegionMapped(UINT64 fbOffset, UINT64 size)
{
    MASSERT(size <= BAR0_WINDOW_SIZE);

    UINT32 offset64k = fbOffset & 0xFFFF;

    if ( (offset64k + size) > BAR0_WINDOW_SIZE)
    {
        MASSERT(!"BAR0 window is only 1 MB wide");
        return NULL;
    }

    REG_WR32(LW_PBUS_BAR0_WINDOW, static_cast<UINT32>(fbOffset >> 16));

    // LW_PRAMIN                             0x007FFFFF:0x00700000 /* RW--M */
    return ((UINT08*)s_MatsLwBase) + 0x700000 + offset64k;
}

bool GpuSubdevice::IsSOC() const
{
    return false;
}

RegHal &GpuSubdevice::Regs()
{
    return *s_pRegHal.get();
}

const RegHal &GpuSubdevice::Regs() const
{
    return *s_pRegHal.get();
}

FrameBuffer *GpuSubdevice::GetFB()
{
    return g_Ptr;
}

bool GpuSubdevice::HasFeature(Feature feature) const
{
    return false;
}

bool GpuSubdevice::IsFeatureEnabled(Feature feature)
{
    return false;
}

string GpuSubdevice::GetName() const
{
    return s_GpuName;
}

RC ValidateSoftwareTree() { return RC::OK; }
GpuDevice *GetBoundGpuDevice() { return &s_GpuDevice; }
GpuSubdevice *GetBoundGpuSubdevice() { return &s_GpuSubdevice; }

static MemError s_MemErrorObj;
MemError &GetMemError(UINT32 x)
{
    return s_MemErrorObj;
}

// we have to give m_RefAllPatternsList something
static vector<PatternClass::PTTRN*> dummy;

PatternClass::PatternClass():m_RefAllPatternsList(dummy){}
PatternClass::~PatternClass() {}
void PatternClass::FillRandomPattern(UINT32 seed) {}

RC PatternClass::FillMemoryWithPattern
(
    volatile void *p,
    UINT32 pitchInBytes,
    UINT32 lines,
    UINT32 widthInBytes,
    UINT32 depth,
    UINT32 patternNum,
    UINT32 *pRepeatInterval
)
{
    static UINT32 s_ShortMats[]=
    {
        0x00000000, 0xFFFFFFFF, 0x55555555, 0xAAAAAAAA,
        PatternClass::END_PATTERN
    };

    volatile UINT08 * pRow = static_cast<volatile UINT08*>(p);
    UINT32 x;

    volatile UINT08 * pColumn = pRow;
    UINT32 iterations = widthInBytes/4;
    UINT32 cnt = 0;
    UINT32 *pPattern = s_ShortMats;

    for (x = 0, cnt = 0; cnt < iterations; x += 4, cnt++)
    {
        MEM_WR32((volatile void *)(pColumn + x), *pPattern);
        if ( *(++pPattern) == PatternClass::END_PATTERN)
            pPattern = s_ShortMats;
    }

    return OK;
}

void Platform::DelayNS(UINT32 ns)
{
    return;
};

UINT64 Platform::GetTimeMS()
{
    return 0;
}

RC GpuTest::AllocDisplay()
{
    return OK;
}

GpuMemTest::GpuMemTest() :
    m_EndLocation(0),
    m_StartLocation(0),
    m_MaxErrors(500),
    m_MinFbMemPercent(0),
    m_MaxFbMb(0xffffffff)
{
}

RC GpuMemTest::Setup()
{
    return Platform::DisableUserInterface(0, 0, 0, 0,
                                          0, 0, 0, NULL);
}

RC GpuMemTest::Cleanup()
{
    Platform::EnableUserInterface(NULL);
    return OK;
}

RC GpuMemTest::AllocateEntireFramebuffer
(
    bool blockLinear,
    UINT32 hChannel
)
{
    RC rc;
    UINT64 maxSize = GetEndLocation() * ONE_MB;

    if (maxSize > GetMaxFbMb() * ONE_MB)
        maxSize = GetMaxFbMb() * ONE_MB;

    CHECK_RC(GpuUtility::AllocateEntireFramebuffer(
                      ONE_MB,      // MinChunkSize
                      0,           // No maxChunkSize
                      maxSize,
                      GetChunks(),
                      blockLinear,
                      4096,        // minPageSize: use 4K pages for phys access
                      4096,        // maxPageSize: use 4K pages for phys access
                      hChannel,
                      GetBoundGpuDevice(),
                      GetMinFbMemPercent(),
                      true));

    if (GetChunks()->size() == 0)
    {
        Printf(Tee::PriNormal, "ERROR:  Memory was not allocated.\n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    return rc;
}

UINT32 Tee::GetTeeModuleCoreCode()
{
    return static_cast<UINT32>(Tee::ModuleNone);
}

OneTimeInit::OneTimeInit()
{
}

OneTimeInit::~OneTimeInit()
{
}
