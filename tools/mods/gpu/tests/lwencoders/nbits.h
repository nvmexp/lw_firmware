/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2015 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef NBITS_H
#define NBITS_H

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <type_traits>

#include "core/include/types.h"

#include "inarchive.h"

namespace Vid
{

//! Represents a unsigned integer number that is encoded by N bits in a bit
//! stream. Corresponds to u(n) of ITU-T H.264 (section 7.2) and to uimsbf of
//! SMPTE 421M-2006.
template <unsigned int N>
class NBits
{
public:
    typedef conditional_t<
        N <= 8
      , UINT08
      , conditional_t<
            N <= 16
          , UINT16
          , conditional_t<
                N <= 32
              , UINT32
              , UINT64
              >
          >
      > StorageType;

    NBits() : m_v(0)
    {}

    NBits(StorageType v) : m_v(v)
    {}

    operator StorageType() const
    {
        return m_v;
    }

    NBits& operator =(StorageType rhs)
    {
        m_v = rhs;

        return *this;
    }

private:
    StorageType m_v;
};

//! A version of `NBits` that can be colwerted to bool, but cannot be
//! arithmetically compared.
template <>
class NBits<1>
{
    typedef void (NBits::*SafeBool)() const;
    void SafeBoolF() const {}

public:
    typedef bool StorageType;

    NBits() : m_v(false)
    {}

    NBits(bool v) : m_v(v)
    {}

    operator SafeBool() const
    {
        return m_v ? &NBits::SafeBoolF : 0;
    }

    NBits& operator =(bool rhs)
    {
        m_v = rhs;

        return *this;
    }

private:
    bool m_v;
};

} // namespace Vid

namespace BitsIO
{
template <unsigned int N>
struct SerializationTraits<Vid::NBits<N> >
{
    static constexpr bool isPrimitive = true;
};
} // namespace BitsIO

#endif // NBITS_H
