/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   core_utility.cpp
 * @brief  Implementation of the Utility namespace (core_utility.h) that does not depend
 *         on any other header files.
 *         Do not add any functions or code that requires other header files to be included.
 *
 */

// Utility functions.
#include "core/include/coreutility.h"

INT32 Utility::CountBits
(
    UINT64 Value
)
{
    //
    // The algorithm is optimized for values with only a few bits set.
    //
    INT32 Count;
    for (Count = 0; Value != 0; ++Count)
    {
        Value &= (Value - 1ULL);    // Chop off the least significant bit.
    }
    return Count;
}

INT32 Utility::FindNthSetBit(UINT32 val, INT32 n)
{
    if ((val == 0) || (n < 0) || (n > 31))
      return -1;
    for (INT32 i = 0; i < n; i++)
    {
       val &= (val - 1U);
       if (!val)
       {
           return -1;
       }
    }
    return Utility::BitScanForward(val);
}

INT32 Utility::BitScanForward(UINT64 val)
{
    static const INT32 deBruijnBitPosition[32] =
    {
        31,  0,  1,  5,  2, 10,  6, 15,  3, 13, 11, 20,  7, 22, 16, 25,
        30,  4,  9, 14, 12, 19, 21, 24, 29,  8, 18, 23, 28, 17, 27, 26
    };

    if (val > _UINT32_MAX)
        return BitScanForward64(val);

    UINT32 x = static_cast<UINT32>(val);

    if (0 != x)
    {
        // Use a multiplicative hash function to map the pattern to 0..31, then
        // use this number to find the bit position by means of a lookup table.
        // The multiplicative constant was found empirically. This number is a
        // de Bruijn sequence for k = 2 and n = 5. Among all suitable de Bruijn
        // sequences the one that groups small indices closer together was
        // chosen.
        return deBruijnBitPosition[
            static_cast<UINT32>((x & ~(x - 1)) * 0x08ca75beU) >> 27
        ];
    }

    return -1;
}

INT32 Utility::BitScanForward(UINT64 value, INT32  start)
{
    if (value > _UINT32_MAX)
        return BitScanForward64(value, start);

    if ((start < 0) || (start > 31))
        return -1;

    UINT32 x = static_cast<UINT32>(value);

    return BitScanForward(x & (static_cast<UINT32>(-1) << start));
}

INT32 Utility::BitScanForward64(UINT64 x)
{
    static const INT32 deBruijnBitPosition[64] =
    {
         0,  1, 45,  2, 36, 46, 59,  3, 56, 37, 40, 47, 60, 11,  4, 28,
        43, 57, 38,  9, 41, 15, 48, 17, 61, 53, 12, 50, 24,  5, 19, 29,
        63, 44, 35, 58, 55, 39, 10, 27, 42,  8, 14, 16, 52, 49, 23, 18,
        62, 34, 54, 26,  7, 13, 51, 22, 33, 25,  6, 21, 32, 20, 31, 30
    };

    if (0 != x)
    {
        // Multiplicative hash function to map the pattern to 0..63
        return deBruijnBitPosition[
            static_cast<UINT64>((x & ~(x - 1)) * 0x03a6af73f1285b23ULL) >> 58
        ];
    }

    return -1;
}

INT32 Utility::BitScanForward64(UINT64 value, INT32  start)
{
    if ((start < 0) || (start > 63))
        return -1;

    return BitScanForward64(value & (~0ULL << start));
}

INT32 Utility::BitScanReverse(UINT64 val)
{
    static const INT32 mapNumBits[32] =
    {
        21, 27, 12, 25, 16, 18,  8, 20, 11, 15,  7, 10,  6,  5,  4, 31,
         0, 22,  1, 28, 23, 13,  2, 29, 26, 24, 17, 19, 14,  9,  3, 30
    };

    if (val > _UINT32_MAX)
        return BitScanReverse64(val);

    UINT32 x = static_cast<UINT32>(val);

    if (0 != x)
    {
        // first round up to the next 'power of 2 - 1'
        x |= x >> 1;
        x |= x >> 2;
        x |= x >> 4;
        x |= x >> 8;
        x |= x >> 16;

        // Use a multiplicative hash function to map the pattern to 0..31, then
        // use this number to find the first bit set by means of a lookup table.
        // The multiplicative constant was found empirically. Among all
        // constants the one that groups small indices closer together was
        // chosen.
        return mapNumBits[
            static_cast<UINT32>(x * 0x87dcd629U) >> 27
        ];
    }

    return -1;
}

INT32 Utility::BitScanReverse(UINT64 value, INT32  end)
{
    if (value > _UINT32_MAX)
        return BitScanReverse64(value, end);

    if ((end < 0) || (end > 31))
        return -1;

    UINT32 x = static_cast<UINT32>(value);

    return BitScanReverse(x & (static_cast<UINT32>(-1) >> (31 - end)));
}

INT32 Utility::BitScanReverse64(UINT64 x)
{
    static const INT32 mapNumBits[64] =
    {
         0, 58,  1, 59, 45, 29,  2, 60, 11, 21, 46, 30, 34, 14,  3, 61,
        43, 19, 12, 41, 22, 24, 47, 26, 31, 38, 35, 54, 15, 49,  4, 62,
        57, 44, 28, 10, 20, 33, 13, 42, 18, 40, 23, 25, 37, 53, 48, 56,
        27,  9, 32, 17, 39, 36, 52, 55,  8, 16, 51,  7, 50,  6,  5, 63
    };

    if (0 != x)
    {
        // first round up to the next 'power of 2 - 1'
        x |= x >> 1;
        x |= x >> 2;
        x |= x >> 4;
        x |= x >> 8;
        x |= x >> 16;
        x |= x >> 32;

        // Multiplicative hash function to map the pattern to 0..63
        return mapNumBits[
            static_cast<UINT64>(x * 0x03f274ac66d45ee1ULL) >> 58
        ];
    }

    return -1;
}

INT32 Utility::BitScanReverse64(UINT64 value, INT32  end)
{
    if ((end < 0) || (end > 63))
        return -1;

    return BitScanReverse64(value & (~0ULL >> (63 - end)));
}

const string Utility::CleanseFormatWAR(const string &in)
{
    string work;
    size_t last = 0, next = 0, length = in.length();
    bool replaced = false;

    while (string::npos != (next = in.find('%', next)))
    {
        // skip over any (valid) formatting fields
        next = in.find_first_not_of("-+0 #.123456789", next + 1);
        if ((string::npos == next) || ((next + 1) >= length))
            break;

        if (in[next] == 'l' && in[next + 1] == 'l')
        {
            // replace %ll with %I64
            work.append(in.substr(last, next - last));
            work.append("I64");

            replaced = true;
            last = next = next + 2;
        }
    }

    if (replaced)
    {
        // attach the last part and return the cleansed string
        work.append(in.substr(last, length - last));
        return work;
    }

    // return the original string if we didn't do anything
    return in;
}

// In sim builds va_copy is not defined, so do a simple copy in that case
#ifndef va_copy
#define va_copy(dst, src) (dst)=(src)
#endif
INT32 Utility::VsnPrintf(char* Str, size_t Size, const char* Format, va_list Args)
{
    va_list localArgs;
    va_copy(localArgs, Args);
    const INT32 ret = vsnprintf(Str, Size, Format, localArgs);
    va_end(localArgs);
    return ret;
}

string Utility::StrPrintf(const char * Format, ...)
{
    // The common case for the function assumes a maximum string length of
    // 256 bytes (to avoid time ilwolved in dynamic memory allocation and two
    // calls to vsnprintf). In case the buffer size bytes is insufficient,
    // a new buffer is allocated with the obtained length and vsnprintf is rerun.

    const size_t initialBufSize = 256;

    char buf[initialBufSize];
    string resBuf;

    va_list arguments;
    va_start(arguments, Format);
    const size_t reqdSize = vsnprintf(buf, initialBufSize, Format, arguments);
    va_end(arguments);

    if (reqdSize >= initialBufSize)
    {
        // Addition of 1 for the terminal null character
        resBuf.resize(reqdSize + 1);
        va_start(arguments, Format);
        vsnprintf(&resBuf[0], reqdSize + 1, Format, arguments);
        va_end(arguments);

        resBuf.resize(reqdSize);
    }
    else
    {
        resBuf.assign(buf, reqdSize);
    }

    return resBuf;
}

string Utility::ToString(bool value)
{
    return value ? "true" : "false";
}

string Utility::ToString(INT32 value)
{
    return StrPrintf("%d", value);
}

string Utility::ToString(UINT32 value)
{
    return StrPrintf("%u", value);
}

string Utility::ToString(INT64 value)
{
    return StrPrintf("%lld", value);
}

string Utility::ToString(UINT64 value)
{
    return StrPrintf("%llu", value);
}

string Utility::ToString(double value)
{
    return StrPrintf("%f", value);
}

string Utility::ToString(const string& value)
{
    return value;
}

#if defined(QNX) || defined(_WIN32)
void* memmem(const void* haystack, size_t haystackLen,
             const void* needle, size_t needleLen)
{
    const char  first     = *static_cast<const char*>(needle);
    const char* sHaystack = static_cast<const char*>(haystack);
    void*       found     = nullptr;

    for (;;)
    {
        const char* possible = static_cast<const char*>(memchr(sHaystack, first, haystackLen));

        if (!possible)
        {
            break;
        }

        haystackLen -= (possible - sHaystack);

        if (haystackLen < needleLen)
        {
            break;
        }

        sHaystack = possible;

        if (memcmp(sHaystack, needle, needleLen) == 0)
        {
            found = const_cast<char*>(sHaystack);
            break;
        }

        ++sHaystack;
        --haystackLen;
    }

    return found;
}
#endif
