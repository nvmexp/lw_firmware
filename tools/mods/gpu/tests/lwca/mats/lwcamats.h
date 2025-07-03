/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights
 * reserved.  All information contained herein is proprietary and confidential
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#include "../tools/tools.h"

struct BadValue
{
    device_ptr address;
    UINT64 actual;
    UINT64 expected;
    UINT64 reread1;
    UINT64 reread2;
    UINT64 timestamp;
};

struct RangeErrors
{
    UINT64 numErrors;
    UINT32 numReportedErrors;
    UINT64 badBits;
    BadValue reportedValues[1];
};

struct LwdaMatsInput
{
    // Tested memory
    device_ptr mem;       // Device pointer (virtual) to the tested memory chunk
    device_ptr memEnd;    // Device pointer to the first byte beyond the tested memory chunk

    // Test configuration
    UINT32 numIterations; // Number of test iterations to perform on the memory
    UINT32 pattern1;      // 32-bit pattern to write for even iterations
    UINT32 pattern2;      // 32-bit pattern to write for odd iterations
    UINT32 direction;     // Direction of the test (0=forward, 1=backward)
    UINT32 step;          // Size in MemAccessSize steps (period) between mem being read and written
    UINT32 ilwstep;       // Size in MemAccessSize steps (period) how often ilwerted values of patterns should be used
    UINT32 offset;        // Index offset within "step" where current kernel operates on
    UINT32 memAccessSize; // Size of each memory access (ex. 32/64-bit accesses)

    // Output results
    UINT32 resultsSize;   // Total size of results storage, in bytes
    device_ptr results;   // Results storage, equally divided between threads
};
