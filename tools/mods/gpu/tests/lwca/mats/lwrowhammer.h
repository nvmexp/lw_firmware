/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "../tools/tools.h"

#define NUM_THREADS   1024
#define COL_MASK_BITS 0
#define READS_PER_ROW (1<<COL_MASK_BITS)

namespace LwdaRowHammerTest {

    struct RowHammerInput {
        device_ptr errors;
        device_ptr ptrs;
        UINT32 iterations;
    };

    struct CheckMemInput
    {
        device_ptr errors;
        device_ptr mem;
        UINT64 size;
        UINT32 pattern;
    };

    struct BadValue
    {
        device_ptr addr;
        UINT32 value;
        UINT32 reread1;
        UINT32 reread2;
        UINT32 _dummy;
    };

    struct Errors
    {
        UINT64 allSum;
        UINT64 numErrors;
        UINT32 badBits;
        UINT32 numReportedErrors;
        UINT32 maxReportedErrors;
        UINT32 _dummy;
        BadValue   errors[1];
    };

} // namespace LwdaRowHammer
