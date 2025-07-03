/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   utility.h
 * @brief  Contains the Utility namespace declaration.
 *
 * The Utility functions are miscellaneous functions that could be used
 * in a lot of areas.  Things like random number routines and type
 * colwersions that don't require any other mods header files go here.
 * Including stdlib headers is ok.
 */
#pragma once

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <limits>
#include <set>
#include <vector>
#include <type_traits>
#include <string>
#include <stdarg.h>

#include <boost/numeric/colwersion/cast.hpp>

#include "types.h"

#if defined(BitScanForward)
#   undef BitScanForward
#endif

#if defined(BitScanReverse)
#   undef BitScanReverse
#endif

#if defined(BitScanForward64)
#   undef BitScanForward64
#endif

#if defined(BitScanReverse64)
#   undef BitScanReverse64
#endif

using std::string;

#define NUMELEMS(arr) Utility::ArraySize(arr)

namespace Utility
{
    template <typename T>
    constexpr std::size_t ArraySize(const T& arr)
    {
        return std::end(arr) - std::begin(arr);
    }

    template<typename T, std::size_t N>
    constexpr std::size_t ArraySize(T (&arr)[N])
    {
        return N;
    }

    //! Count the number of bits set in the passed in value.
    INT32 CountBits(UINT64 Value);

    //! \brief finds the nth bit set in word val.
    //!
    //! Returns index of nth bit setin a word, starting from idx 0.
    //! If word has less set bits than n, returns -1
    INT32 FindNthSetBit(UINT32 Value, INT32 n);

    //! Find the index of the lowest set bit. If no bits are set return -1.
    INT32 BitScanForward(UINT64 value);

    //! Find the index of the lowest set bit starting at or after the specified index.
    //! If no bits are set return -1.
    INT32 BitScanForward(UINT64 Value, INT32 Start);

    //! Find the index of the lowest set bit. If no bits are set return -1.
    INT32 BitScanForward64(UINT64 value);

    //! Find the index of the lowest set bit starting at or after the specified index.
    //! If no bits are set return -1.
    INT32 BitScanForward64(UINT64 Value, INT32 Start);

    //! Find the index of the highest set bit. If no bits are set return -1.
    INT32 BitScanReverse(UINT64 value);

    //! Find the index of the highest set bit starting at or before the specified index.
    //! If no bits are set return -1.
    INT32 BitScanReverse(UINT64 Value, INT32 End);

    //! Find the index of the highest set bit. If no bits are set return -1.
    INT32 BitScanReverse64(UINT64 value);

    //! Find the index of the highest set bit starting at or before the specified index.
    //! If no bits are set return -1.
    INT32 BitScanReverse64(UINT64 Value, INT32 End);

    // XXX (FORMAT_FOR_64BITS) - workaround until MSVC71 is phased out
    const string CleanseFormatWAR(const string &in);

    //! Like library version of vsnprintf, but doesn't destroy the va list passed to it
    INT32 VsnPrintf(char* Str, size_t Size, const char* Format, va_list Args);

    //! Like sprintf, but returns the result as a string object.
    GNU_FORMAT_PRINTF(1, 2)
    string StrPrintf(const char * Format, ...);

    string ToString(bool value);
    string ToString(INT32 value);
    string ToString(UINT32 value);
    string ToString(INT64 value);
    string ToString(UINT64 value);
    string ToString(double value);
    string ToString(const string& value);

    template <typename Target, typename Source> inline
    Target RoundCast(Source arg) {
      typedef boost::numeric::colwerter<
          Target
        , Source
        , boost::numeric::colwersion_traits<Target, Source>
        , boost::numeric::silent_overflow_handler
        , boost::numeric::RoundEven<
              typename boost::numeric::colwersion_traits<Target, Source>::source_type
            >
        > Colwerter;

      return Colwerter::colwert(arg);
    }
}
