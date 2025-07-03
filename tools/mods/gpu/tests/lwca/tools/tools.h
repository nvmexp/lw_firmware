/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

// This file is compiled by both lwcc and gcc. When compiling LWCA lwbins,
// static and inline functions require the keyword __device__ to be used
// to indicate to lwcc that they are kernel functions. However, this keyword
// is not defined in gcc, therefore we need to define it as null here.
#if !defined(__LWDACC__) && !defined(__device__)
#define __device__
#endif

// Define MODS types in LWCA
#if defined(__LWDACC__)
using UINT08 = uint8_t;
using UINT16 = uint16_t;
using UINT32 = uint32_t;
// The LWCA built-in atomicAdd only works for unsigned long long int,
// and uint64_t is usually unsigned long int
using UINT64 = unsigned long long int;
using INT08 = int8_t;
using INT16 = int16_t;
using INT32 = int32_t;
using INT64 = int64_t;
using FLOAT32 = float;
using FLOAT64 = double;
#endif

// Ensure that types are the correct size
template< int size > struct EnsureUint;
template<> struct EnsureUint<4>  { };
template<> struct EnsureUint<8>  { };
template<> struct EnsureUint<16> { };
template<> struct EnsureUint<32> { };
struct EnsureFLOAT32 { EnsureUint<sizeof(FLOAT32)> ensureU32; };
struct EnsureFLOAT64 { EnsureUint<sizeof(FLOAT64)> ensureU64; };
struct EnsureU64 { EnsureUint<sizeof(UINT64)> ensureU64; };

// Type used for passing device pointers to LWCA
using device_ptr = UINT64;
// Get pointer type from device_ptr
template<typename T>
__device__ T GetPtr(device_ptr ptr)
{
    return reinterpret_cast<T>(static_cast<size_t>(ptr));
}
