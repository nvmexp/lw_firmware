/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include <stdint.h>
#include "../tools/tools.h"

#define L1_LINE_SIZE_BYTES 128
struct L1TagParams
{
    device_ptr data;
    device_ptr errorCountPtr;
    device_ptr errorLogPtr;
    UINT64   sizeBytes;
    UINT64   iterations;
    UINT32   errorLogLen;
    UINT32   randSeed;
};

enum class TestStage : UINT32
{
    PreLoad = 0,
    RandomLoad = 1
};

struct L1TagError
{
    TestStage testStage;
    UINT16 decodedOff;
    UINT16 expectedOff;
    UINT64 iteration;
    UINT32 innerLoop;
    UINT32 smid;
    UINT32 warpid;
    UINT32 laneid;
};
