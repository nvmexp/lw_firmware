/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#include "../tools/tools.h"

namespace LwLinkKernel
{
    enum TEST_TYPE
    {
        NOP
       ,READ
       ,WRITE
       ,MODIFY
       ,ATOMIC
       ,NONE
    };

    enum MEM_INDEX_TYPE
    {
        LINEAR
       ,RANDOM
    };

    struct TEST_RESULT
    {
        double nanoseconds;
        double bytes;
    };
    typedef TEST_RESULT CopyStats;

    struct CopyTriggers
    {
        bool bWaitStart;
        bool bWaitStop;
    };

    struct GupsInput
    {
        device_ptr     testData       = 0ULL;
        UINT32         transferWidth  = 0U;
        UINT64         testDataSize   = 0ULL;
        TEST_TYPE      testType       = NONE;
        MEM_INDEX_TYPE memIdxType     = LINEAR;
        UINT32         unroll         = 0ULL;
        device_ptr     copyStats      = 0ULL;
        device_ptr     copyTriggers   = 0ULL;
    };
};
