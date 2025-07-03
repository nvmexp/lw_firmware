/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#include "../tools/tools.h"

enum C2CBandwidthTestMemory
{
    SRC_MEM_A,
    SRC_MEM_B,
    DST_MEM,
    MEM_COUNT
};
struct C2CBandwidthParams
{
    device_ptr             srcAMemory;
    device_ptr             srcBMemory;
    device_ptr             dstMemory;
    UINT32                 uint4PerThread;
    UINT32                 iterations;
    C2CBandwidthTestMemory firstSrcMem;
};
