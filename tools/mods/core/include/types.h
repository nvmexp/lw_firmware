/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Type definitions.

#pragma once

#include "lwtypes.h"

typedef LwS8   INT08;
typedef LwU8  UINT08;

#if !defined(MODS_NO_DEFINE_STANDARD_INT)
// 8/16/32/64 bit integers
typedef LwS16  INT16;
typedef LwU16 UINT16;
typedef LwS32  INT32;
typedef LwU32 UINT32;
typedef LwS64  INT64;
typedef LwU64 UINT64;
#endif

#define  _INT08_MIN static_cast< INT08>(0x80)
#define  _INT08_MAX static_cast< INT08>(0x7F)
#define _UINT08_MAX static_cast<UINT08>(0xFF)
#define  _INT16_MIN static_cast< INT16>(0x8000)
#define  _INT16_MAX static_cast< INT16>(0x7FFF)
#define _UINT16_MAX static_cast<UINT16>(0xFFFF)
#define  _INT32_MIN static_cast< INT32>(0x80000000)
#define  _INT32_MAX static_cast< INT32>(0x7FFFFFFF)
#define _UINT32_MAX static_cast<UINT32>(0xFFFFFFFF)
#define  _INT64_MIN static_cast< INT64>(0x8000000000000000ULL)
#define  _INT64_MAX static_cast< INT64>(0x7FFFFFFFFFFFFFFFLL)
#define _UINT64_MAX static_cast<UINT64>(0xFFFFFFFFFFFFFFFFULL)

// Float numbers
typedef LwF32 FLOAT32;
typedef LwF64 FLOAT64;

// Physical address -- always defined as 64 bits for now, but could be reduced
// in some builds if we want.
typedef UINT64 PHYSADDR;

//! Form of a function used as an ISR.
typedef long (*ISR)(void*);

typedef enum { MODS_FALSE, MODS_TRUE } MODS_BOOL;

// For now, define MODS_RC to be UINT32
typedef UINT32 MODS_RC;

#ifdef _WIN32
#define DIR_SEPARATOR_CHAR '\\'
#define DIR_SEPARATOR_CHAR2 '/'
#else
#define DIR_SEPARATOR_CHAR '/'
#define DIR_SEPARATOR_CHAR2 '\\'
#endif

#if defined(__cplusplus)

#if defined(INCLUDE_STD_NAMESPACE)
namespace std { }
using namespace std;
#endif  // INCLUDE_STD_NAMESPACE

// types.h is sometimes included inside extern "C" clause, so cancel that,
// because we need C++ linkage for these operators.

extern "C++"
{
    constexpr unsigned long long operator "" _KB(unsigned long long size)
    {
        return size << 10;
    }

    constexpr unsigned long long operator "" _MB(unsigned long long size)
    {
        return size << 20;
    }

    constexpr unsigned long long operator "" _GB(unsigned long long size)
    {
        return size << 30;
    }
}

// Ensure C++ standard feature flag for exception support is defined
//
// __cpp_exceptions is 1 if exceptions are enabled, 0 otherwise
#ifndef __cpp_exceptions
#   if defined(LW_WINDOWS)
#       if defined(_MSC_VER)
            // MSVC defines _CPPUNWIND with 1 if exceptions, otherwise undefined
#           if defined(_CPPUNWIND)
#               define __cpp_exceptions 1
#           else
#               define __cpp_exceptions 0
#           endif
#       else
#           error Unable to translate Windows exception feature definition to __cpp_exceptions.
#       endif
#   elif defined(__GNUC__)
        // GCC 4.9 defines __EXCEPTIONS with 1 if exceptions, otherwise undefined
#       ifdef __EXCEPTIONS
#           define __cpp_exceptions 1
#       else
#           define __cpp_exceptions 0
#       endif
#   elif defined(__clang__)
        // Clang has some confusion up to 3.6. Need to test both feature flags
        // to confirm enablement on all versions.
#       if defined(__EXCEPTIONS && __has_feature(cxx_exceptions))
#           define __cpp_exceptions 1
#       else
#           define __cpp_exceptions 0
#       endif
#   else
#       error __cpp_exceptions not defined. Unable to derive its value.
#   endif
#endif

#if defined(__GNUC__) && __GNUC__ <= 4
    // Full C++14 constexpr support available in gcc 7.3, but not gcc 4.9
#   define MODS_CONSTEXPR
#else
#   define MODS_CONSTEXPR constexpr
#endif

#endif  // __cplusplus

#if defined(__GNUC__) || defined(__RESHARPER__)
// GCC can type-check printf like 'Arguments' against the 'Format' string.
#define GNU_FORMAT_PRINTF(f, a) [[gnu::format(printf, f, a)]]
#else
#define GNU_FORMAT_PRINTF(f, a)
#endif
