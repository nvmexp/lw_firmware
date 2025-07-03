/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2010,2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#include "../tools/lwda_vectors.h"
#include "../tools/tools.h"

#define DEFAULT_PATTERN_ITERATIONS 4

namespace NewLwdaMats {

struct BadValue
{
    device_ptr address;
    UINT32 actual;
    UINT32 expected;

    // We encode information into 'encFailData' as BTTTOOOIIIDPP:
    //   B: BlockID, max 255
    //   T: ThreadID, max 4095
    //   O: Outer iteration, max 4095
    //   I: Inner iteration, max 4095
    //   D: Direction, can only be 0 (forward) or 1 (reverse)
    //   P: Pattern, max 255 (due to maxLwdaMatsPatterns = 256)
    UINT64 encFailData;

    // NOTE: Changes made to either EncodeFailData or DecodeFailData functions
    //       neeed to be synchronized between the two, and likely require all
    //       nwlwdamats lwbins to be recompiled!!

    __device__
    static UINT64 EncodeFailData(
        UINT32 blockIdx,
        UINT32 threadIdx,
        UINT32 outerIteration,
        UINT32 innerIteration,
        UINT32 patternIdx,
        bool forward)
    {
        UINT64 encFailData = (((blockIdx        & 0xFFULL)  << 45) |
                             ((threadIdx        & 0xFFFULL) << 33) |
                             ((outerIteration   & 0xFFFULL) << 21) |
                             ((innerIteration   & 0xFFFULL) <<  9) |
                             ((forward ? 0x0ULL : 0x1ULL)   <<  8) |
                             ((patternIdx       & 0xFFULL)  <<  0));
        return encFailData;
    }

    __device__
    static bool DecodeFailData(
        UINT64 encFailData,
        UINT32 *blockIdx,
        UINT32 *threadIdx,
        UINT32 *outerIteration,
        UINT32 *innerIteration,
        UINT32 *patternIdx,
        bool *forward)
    {
        if (!(blockIdx && threadIdx && outerIteration && innerIteration && patternIdx && forward))
        {
            return false;
        }

        *blockIdx       = (encFailData  >> 45) & 0xFFU;
        *threadIdx      = (encFailData  >> 33) & 0xFFFU;
        *outerIteration = (encFailData  >> 21) & 0xFFFU;
        *innerIteration = (encFailData  >>  9) & 0xFFFU;
        *forward        = ((encFailData >>  8) & 0x1) ? false : true;
        *patternIdx     = (encFailData  >>  0) & 0xFU;

        return true;
    }
};

struct RangeErrors
{
    UINT64 numErrors;
    UINT32 numReportedErrors;
    BadValue reportedValues[1];
};

struct BitError
{
    UINT64 address;
    UINT32 badBits;
};

struct RangeBitErrors
{
    UINT64 numErrors;
    BitError overflowBuffer[1];
};

struct SaveState
{
    UINT64 prevPass;        // Previous number of passes through the memory
    UINT64 loop;            // Current stride offset of thread within a memory pass
    UINT32 pattern;
    UINT32 dirReverse;
    UINT32 innerIteration;
};

/*  Buffer state transition (*) = who's allowed to write to global host memory:
 *
 *        ------------------(device)--------------------
 *       |                                              \
 *       V                                               \
 *  Available ---(device)---> Full ---(host)---> Flushed /
 *
 *  Synchronicity is maintained if only one machine is allowed to write to buffer at each state
 */

using BufferStatus = UINT32;
enum : UINT32
{
    BUFFER_AVAILABLE = 0,   // Buffer is available to write
    BUFFER_FULL      = 1,   // Last thread to be able to write to buffer marks it FULL
    BUFFER_FLUSHED   = 2,   // Host has marked buffer flushed. First device thread to notice this
                            // should mark the buffer AVAILABLE
};

// We shouldn't need to have index pointer to host-mapped memory
// but doing so avoids register spillage
// Host may read index but does not write (except when flushing)
struct OverflowHandler
{
    device_ptr indexPtr;        // Index of overflow buffer in which to place excess error
    device_ptr blkBufStatusPtr; // Keeps track of buffer status
    UINT32 maxBlockErrors;      // Max number of overflow errors on overflow buffer at a given time
};

struct LwdaMatsInput
{
    SaveState  state;       // Resume state when running with PulseTicks
    device_ptr mem;         // Device pointer (virtual) to the tested memory chunk
    device_ptr memEnd;      // Device pointer to the first byte beyond the tested memory chunk
    device_ptr patterns;    // Device pointer to the MATS patterns
    device_ptr results;     // Results storage, equally divided between threads
    device_ptr debugPtr;    // Used to dump initialized data for debugging
    UINT64 randSeed;        // Used by Random mode to seed the RNG during init, not always used
    UINT64 numTicks;        // Number of mem accesses per thread (0 is max accesses)
    UINT64 delayNs;         // Time to delay at the end of the kernel in Nanoseconds
    UINT32 numIterations;   // Number of test iterations to perform on the memory
    UINT32 numPatterns;     // Number of MATS patterns
    UINT32 maxGlobalErrors; // Max number of errors that can be written across kernels
    UINT32 outerIter;       // Current value of outer iteration, used for reporting
    UINT32 maxLocalErrors;  // Max number of errors that can be stored in shared memory
    UINT32 randAccessBytes; // Number of bytes read/written in random offset
    UINT32 readOnly;        // Only read and check results
    UINT32 busPatBytes;     // Length of bus pattern in BusTest mode
    UINT32 levelMask;       // Mask to select which PAM4 levels to test in PAM4 mode
    UINT32 useSafeLevels;   // Forbid the first PAM4 level in a burst from being -3
    UINT32 reportBitErrors; // Report excess errors that don't fit in shared memory
    OverflowHandler ovfHandler; // Contains auxiliary variables for bitError reporting
    UINT32 skipErrReport;   // Skip error reporting when callwlating pulse ticks
};

enum MaxPatterns
{
    // If this is increased, BadValue->encFailData will need to be updated
    maxLwdaMatsPatterns = 256
};

enum class Variant : UINT32
{
    Standard = 0,
    Random   = 1,
    BusTest  = 2,
    PAM4     = 3,
};

// PAM4 mode requires additional random state
struct PAM4Rand
{
    UINT32 x;
    UINT32 y;
    UINT32 z;
    UINT32 w;
    UINT32 data[2];
};

} // namespace NewLwdaMats
