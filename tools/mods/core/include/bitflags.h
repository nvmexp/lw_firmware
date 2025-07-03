/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include <climits> // For CHAR_BIT
#include <cstddef> // For size_t

// Adds operators |, &, ~, ==, and != for use as a bitflags enum
#define BITFLAGS_ENUM(EnumName)                                                             \
    inline constexpr EnumName operator|(EnumName a, EnumName b)                             \
        { return static_cast<EnumName>(static_cast<UINT64>(a) | static_cast<UINT64>(b)); }  \
    inline constexpr EnumName operator&(EnumName a, EnumName b)                             \
        { return static_cast<EnumName>(static_cast<UINT64>(a) & static_cast<UINT64>(b)); }  \
    inline EnumName& operator|=(EnumName& a, EnumName b)                                    \
        { return (a = a | b); }                                                             \
    inline EnumName& operator&=(EnumName& a, EnumName b)                                    \
        { return (a = a & b); }                                                             \
    inline constexpr EnumName operator~(EnumName e)                                         \
        { return static_cast<EnumName>(~static_cast<UINT64>(e)); }                          \
    inline constexpr bool operator==(EnumName e, UINT64 i)                                  \
        { return static_cast<UINT64>(e) == i; }                                             \
    inline constexpr bool operator!=(EnumName e, UINT64 i) { return !(e == i); }

namespace Utility
{
    //!
    //! \brief Create a bitmask of given size.
    //!
    //! \param size Size of the bitmask. If size is greater than the width of
    //! the type, all bits will be set.
    //! \return Bitmask of given size.
    //!
    template <typename ElementT>
    constexpr ElementT Bitmask(size_t size)
    {
        return (size >= (sizeof(ElementT) * CHAR_BIT)
                ? ~static_cast<ElementT>(0) // Return all ones instead of overshifting
                : ((static_cast<ElementT>(1) << size) - 1));
    }
}
