/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012, 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#if defined(WINDOWS) || defined(_WIN32) || defined(_WIN64)
#include <direct.h>     // _getcwd
#else
#include <unistd.h>     // getcwd or get_lwrrent_dir_name
#endif

#include <cstdio>
#include <sstream>
#include <cassert>
#include "rmtstring.h"

// List of whitespace characters
const char * const rmt::String::whitespace = " \r\n\t\v\f";

// List of ASCII alphanumeric characters
const char * const rmt::String::alnum = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

// Concatenate into a single string
rmt::String rmt::StringList::concatenate(const char *before, const char *between, const char *after) const
{
    std::ostringstream oss;
    const_iterator it;
    bool first = true;

    for (it = begin(); it != end(); it++)
    {
        if (first && before && *before) oss << before;
        if (!first && between && *between) oss << between;
        first = false;
        oss<< *it;
    }

    if (!first && after && *after) oss << after;
    return oss.str();
}

// Extract function name from __PRETTY_FUNCTION__
rmt::String rmt::String::function(const char *function)
{
    size_t pos;
    String s = function;

    // Discard anything after an open parenthesis (argument list, etc.)
    pos = s.find('(');
    if (pos != npos)
        s.erase(pos);

    // Discard anything before the last space (return value, etc.)
    pos = s.rfind(' ');
    if (pos != npos)
        s.erase(0, pos + 1);

    return s;
}

// Get the current (present) working directory
// Empty string if we are unable to get current working directory
rmt::String rmt::String::getcwd()
{
#if defined(WINDOWS) || defined(_WIN32) || defined(_WIN64)
    char *heap = ::_getcwd(NULL, 0);         // ISO C++
    String cwd(heap);
    if (heap)
        free(heap);

#elif defined(_GNU_SOURCE)
    char *heap = ::get_lwrrent_dir_name();   // Kernel call
    String cwd(heap);
    if (heap)
        free(heap);

#elif defined(PATH_MAX)
    char stack[PATH_MAX];
    String cwd(::getcwd(stack, PATH_MAX));   // POSIX.1-2001

#else
#error Unsupported: _getcwd, getcwd, or get_lwrrent_dir_name.
#endif

    // Compile-time error ('cwd' not found) if one of the above #if isn't true.
    return cwd;
}

// Break string into shards.
void rmt::String::extractShards(rmt::StringList& shards, char delimiter) const
{
    // Should be empty
    shards.clear();

    // Colwert this string to a stream.
    std::istringstream iss(*this);
    String shard;

    // Add each shard to the list and trim the excess whitespace.
    while (getline(iss, shard, delimiter))
    {
        shard.trim();
        shards.push_back(shard);
    }
}

// Break string into shards.
rmt::String rmt::String::extractShard(char delimiter)
{
    // Search for the delimiter.
    String shard;
    size_t pos = find(delimiter);

    // No delimiter, so this string is the only shard.
    if (pos == npos)
        swap(shard);

    // Discard the shard and the delimiter from this string.
    else
    {
        shard = substr(0, pos);
        erase(0, pos + 1);
    }

    // Discard leading/trailing whitespace.
    shard.trim();
    return shard;
}

// Break string into shards.
// Empty shards between delimiters are discarded.
rmt::String rmt::String::extractShardOf(const char *shardCharSet)
{
    // Search for the delimiter.
    String shard;
    size_t pos = find_first_not_of(shardCharSet);

    // If the string starts with a delimiter, discard it so that we don't
    // return an empty shard (unless the entire string is only delimiter).
    if (pos == 0)
    {
        pos = find_first_of(shardCharSet, pos);
        erase(0, pos);
        pos = find_first_not_of(shardCharSet);
    }

    // Observation:  'pos' can not be zero at this point.

    // No delimiter, so this string is the only shard.
    if (pos == npos)
        swap(shard);

    // Discard the shard and the delimiter(s) from this string.
    else
    {
        shard = substr(0, pos);
        erase(0, pos);
    }

    // Done
    return shard;
}

// Construction using bitmapped map
rmt::StringBitmap32::StringBitmap32(const initialize &that)
{
    UINT32 b;
    StringMap<UINT32>::const_iterator p;

    // p->first is the string.
    // p->second is the bit map.
    // If there are multiple bits for a single string, use each and every bit.
    // Multiple strings for the same bit causing the assertion to fail.
    for (p = that.map.begin(); p != that.map.end(); ++p)
        for (b = 0x80000000; b ; b >>= 1)
            if (b & p->second)
            {
                assert(!map[BIT_IDX_32(b)]);
                map[BIT_IDX_32(b)] = p->first;
            }
}

// Bit associated with this name
UINT32 rmt::StringBitmap32::bit(const String &text) const
{
    // Search for the associated bit.
    unsigned short n;
    for (n = 0; n < 32; ++n)
        if (!map[n].compare(text))
            return BIT(n);

    // Zero if not found.
    return 0x00000000;
}

//
// Return all names associated with the bitmap.
// NOTE: MSVC does not permit the use of 'plus' or 'minus' for parameter names.
//
rmt::StringList rmt::StringBitmap32::textListWithSign(UINT32 mask, UINT32 value,
            const char *plus0, const char *minus0) const
{
    StringList list;
    UINT32 bit;

    // Iterate over all possible bits
    for (bit = 0x80000000; bit; bit >>= 1)
    {
        // This bit is one that is requested
        if (bit & mask)
        {
            // Create an empty stream.
            std::ostringstream oss;

            // Apply plus (i.e. true) text prefix if applicable
            if ((bit & value) && plus0 && *plus0)
                oss << plus0;

            // Apply minus (i.e. false) text prefix if applicable
            if ( !(bit & value) && minus0 && *minus0)
                oss << minus0;

            // If there is no matching text, use the number.
            if (map[BIT_IDX_32(bit)].empty())
                oss << "bit(" << bit << ')';

            // Use the matching text if available.
            else
                oss << map[BIT_IDX_32(bit)];

            // Apply the text the the list
            list.push_front(rmt::String(oss));
        }
    }

    // Done
    return list;
}

