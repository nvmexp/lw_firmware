/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "../tools/tools.h"

#pragma once
// Include some vector types for use by MODS C++ code
#if !defined(LWDACC)
struct uint2
{
    UINT32 x;
    UINT32 y;
};
inline uint2 make_uint2(UINT32 x, UINT32 y)
{
    return uint2{x, y};
}

struct uint4
{
    UINT32 x;
    UINT32 y;
    UINT32 z;
    UINT32 w;
};
inline uint4 make_uint4(UINT32 x, UINT32 y, UINT32 z, UINT32 w)
{
    return uint4{x, y, z, w};
}
struct EnsureUint2 { EnsureUint<sizeof(uint2)> ensureUint2; };
struct EnsureUint4 { EnsureUint<sizeof(uint4)> ensureUint4; };
#endif

struct uint4x2
{
    uint4 a;
    uint4 b;
};
struct EnsureUint4x2 { EnsureUint<sizeof(uint4x2)> ensureUint4x2; };

// Packs pattern into LWCA vector with Template Specialization
template<typename T>
__device__ T make_vec(UINT32 pattern);

template<> __device__ inline UINT32 make_vec(UINT32 pattern)
{
    return pattern;
}
template<> __device__ inline uint2 make_vec(UINT32 pattern)
{
    return make_uint2(pattern, pattern);
}
template<> __device__ inline uint4 make_vec(UINT32 pattern)
{
    return make_uint4(pattern, pattern, pattern, pattern);
}
template<> __device__ inline uint4x2 make_vec(UINT32 pattern)
{
    return uint4x2{make_vec<uint4>(pattern), make_vec<uint4>(pattern)};
}