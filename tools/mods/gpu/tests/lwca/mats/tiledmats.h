/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#include "../tools/tools.h"
#include "../tools/lwda_vectors.h"
#include "../tma_types.h"
#include "../tma_utils.h"

// Max constant memory size is 64 KB
#define CONST_MEM_MAX_SIZE (64 << 10)
// L2 cache line size is 32B
#define L2_LINE_SIZE_BYTES 32
// Not very robust to define size like this, but can't use sizeof() in
// preprocessor directives
#define TENSOR_DESC_SIZE (64)

// Divide available mem size by 16B to get max number of pattern vectors
#define CONST_MAX_PATTERN_VECS (CONST_MEM_MAX_SIZE - TENSOR_DESC_SIZE) >> 4

// 128B offsets that indicate what the smem region should be used for
#define MBAR_OFFSET 0
#define SMEM_OFFSET 1

// Struct to ensure dsmem address for tma instructions is 128B aligned
struct alignas(128) AlignStruct128B
{
    uint4 dummy[8];
};

struct BadValue
{
    device_ptr address;
    UINT32 actual;
    UINT32 expected;
};

struct RangeErrors
{
    UINT64 numErrors;
    UINT32 numReportedErrors;
    BadValue reportedValues[1];
};

struct Array5
{
    UINT64 vals[5];
};

typedef Array5 Dimensions;
typedef Array5 Coords;

enum class BoxAction
{
    CHECK_PAT,
    FILL_PAT
};

// We'll use two separate structs, one for the main mats kernel, and
// one for check and fill kernels to minimize register usage, which helps increase
// theoretical oclwpancy in each SM (although this shouldn't matter if we only
// need 1 thread per block for the mats kernel).
struct CheckFillInput
{
    device_ptr mem;             // Device pointer (virtual) to the tested memory chunk
    device_ptr memEnd;          // Device pointer to the first byte beyond the tested memory chunk
    device_ptr results;         // Results storage, equally divided between threads
    UINT32 useTMA;              // Use TMA unit to perform memory access
    UINT64 randSeed;            // Used by Random mode to seed the RNG during init, not always used
    UINT32 outerIter;           // Current value of outer iteration, used for reporting
    UINT32 maxGlobalErrors;     // Max number of errors able to be stored in host buffer
    UINT32 maxLocalErrors;      // Max number of errors able to be stored in shared mem
    UINT32 numPatSubsetsInBox;  // Number of pattern subsets in a single "box"
    UINT32 numPatternSubsets;   // Number of pattern subsets
    UINT32 numPatVecsInSubset;  // Number of pattern vectors in a pattern subset
    UINT32 numDims;             // Number of dimensions we're working in (tensors and boxes)
    Dimensions boxDims;         // Box dimensions
    Dimensions tensorDims;      // Tensor dimensions
    UINT64 numBoxesInTensor;    // Number of boxes that fit in the tensor
};

struct TiledMatsInput
{
    device_ptr mem;             // Device pointer (virtual) to the tested memory chunk
    device_ptr memEnd;          // Device pointer to the first byte beyond the tested memory chunk
    UINT32 useTMA;              // Use TMA unit to perform memory access
    UINT32 prefetchMode;        // Use prefetch tma instructions instead of load/store
    UINT32 numIterations;       // Number of test iterations to perform on memory
    UINT32 numDims;             // Number of dimensions we're working in (tensors and boxes)
    Dimensions boxDims;         // Box dimensions
    Dimensions tensorDims;      // Tensor dimensions
	UINT64 numBoxesInTensor;    // Number of boxes that fit in the tensor
	UINT32 numBoxesInSMem;      // Number of boxes that fit into allocated shared memory
    UINT32 boxMemSize;          // Box memory size in bytes
    UINT64 startingBoxIdx;      // Starting box index. Only relevant in pulse mode.
    UINT64 numBoxesInBatch;     // Number of boxes to iterate over per kernel launch.
    UINT64 delayNs;             // Delay duration in nanoseconds
    UINT32 dummyVar;            // Dummy variable used to prevent compiler optimizing away memory accesses
};
