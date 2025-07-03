/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#include "../tools/tools.h"

enum _Maximums
{
    maxBlocks = 4096,
    maxChunks = 256,
    maxBurstLengthTimesPatterns = 4096,
    maxPartTimesChannels = 16
};

struct BadValue
{
    device_ptr address;
    UINT32 actual;
    UINT32 expected;
    UINT32 reread1;
    UINT32 reread2;
    UINT64 iteration;
};

struct RangeErrors
{
    UINT64 numReadErrors[maxPartTimesChannels];
    UINT64 numWriteErrors[maxPartTimesChannels];
    UINT64 numBitReadErrors[maxPartTimesChannels][32];
    UINT64 numBitWriteErrors[maxPartTimesChannels][32];
    UINT64 numEncounteredErrors;
    UINT32 numReportedErrors;
    UINT32 _dummy; // alignment
    BadValue reportedValues[1];
};

struct FunctionParam
{
    device_ptr mem;         // Device pointer (virtual) to the beginning of the tested memory chunk
    device_ptr blocks;      // Device pointer to an array of block indices (unsigned16)
    device_ptr chunks;      // Device pointer to an array of partition*channel identifiers for each chunk (unsigned8)
    device_ptr patterns;    // Device pointer to an array of patterns for each partition*channel (UINT32)
    device_ptr results;     // Results storage, equally divided between threads
    UINT32 numBlocks;       // Number of testable blocks in the tested memory region
    UINT32 blockSize;       // Size of each block in bytes
    UINT32 chunksPerBlock;  // Number of chunks per block (each chunk belongs to one partition*channel)
    UINT32 dwordsPerChunk;  // Number of 32-bit words in each chunk (corresponds to partition*channel stride)
    UINT32 burstLength;     // Number of 32-bit words in each burst
    UINT32 numPatterns;     // Number of patterns for each partition*channel
    UINT32 resultsSize;     // Total size of results storage, in bytes
    UINT32 iteration;       // Iteration index for storing in results
    UINT32 partChan;        // Partition/channel override if there is only one block
    UINT32 verif;           // Verification mode if nonzero
};
