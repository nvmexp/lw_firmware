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

#include <stdint.h>
#include "../tools/tools.h"

#define PREEMPT_NUM_RETRIES 32

enum class QueryType : UINT32
{
    TriRange_Triangle,
    Complet_Triangle,
    Complet_TriRange
};

struct Retval
{
    UINT32 data[4];
};

struct Point
{
    float x;
    float y;
    float z;
};
using Direction = Point;

enum class HitType : UINT32
{
    Triangle,
    TriRange,
    DisplacedSubTri,
    Error,
    None,
    INVALID
};

struct TTUStressParams
{
    device_ptr triRangePtr;
    device_ptr completPtr;
    device_ptr errorCountPtr;
    device_ptr errorLogPtr;
    device_ptr debugResultPtr;
    UINT64     iterations;
    UINT32     randSeed;
    UINT32     errorLogLen;
    UINT32     raysToDump;
    QueryType  queryType;
    UINT32     numLines;
    UINT32     clusterRays;
};

struct TTUSingleRayParams
{
    QueryType  queryType;
    device_ptr dataPtr;
    device_ptr retvalPtr;
    UINT32     numLines;
    Point      rayOrigin;
    Direction  rayDir;
    float      timestamp;
};

struct TTUError
{
    UINT64 iteration;
    QueryType queryType;
    UINT32 smid;
    UINT32 rayId;
    Retval ref;
    Retval ret;
};

#if defined(__LWDACC__)
__device__ __forceinline__ HitType GetHitType(const UINT32 hitReg3)
#else
inline HitType GetHitType(const UINT32 hitReg3)
#endif
{
    // Triangle Hit if the last bit is zero
    if ((hitReg3 >> 31) == 0)
    {
        return HitType::Triangle;
    }
    switch (hitReg3 >> 28)
    {
        case 0b1001:
            if ((hitReg3 >> 27) & 0x1)
            {
                return HitType::DisplacedSubTri;
            }
            else
            {
                return HitType::TriRange;
            }
        case 0b1110:
            return HitType::Error;
        case 0b1111:
            return HitType::None;
        default:
            return HitType::INVALID;
    }
}
