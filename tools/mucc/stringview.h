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
#ifndef INCLUDED_STRINGVIEW_H
#define INCLUDED_STRINGVIEW_H

// This file allows the caller to use the C++17 std::string_view class.
//
// Ideally, we would simply say "#include <string_view>", but not all
// of our compilers support C++17 yet.  So this file uses three
// different techniques to get the string_view class:
//
// * If the compiler supports C++17, then simply include <string_view>.
// * If the compiler supports std::experimental::string_view, then
//   include that and alias it to std::string_view.
// * If the compiler doesn't support either, then create our own
//   std::string_view class that mimics the most useful 95% of
//   string_view.
//
// We should discard this file once all of our compilers support C++17.
//
#include <string>

#if __cplusplus >= 201703L || defined(__clang__)

// In C++17, string_view is fully supported.
// Use the official implementation.
//
#include <string_view>

#elif defined(__GNUC__)

// Before C++17, GCC supported an "experimental" string_view, which is
// almost identical to the C++17 string_view.  (See ToString() below
// for the major difference.)
//
// Import the experimental string_view, and put it in the std::
// namespace so that it can be used interchangeably with the C++17
// string_view.
//
#include <experimental/string_view>

namespace std
{
    using string_view = std::experimental::string_view;
}

#else

// For compilers that don't support string_view (i.e. MSVC before
// version 19), implement our own subset of string_view in the std::
// namespace.
//
#include <algorithm>
#include <cstring>

namespace std
{
    class string_view
    {
    public:
        using size_type = string::size_type;
        static const size_type npos = static_cast<size_type>(-1);

        constexpr string_view() : m_Data(nullptr), m_Size(0) {}
        constexpr string_view(const string_view&) = default;
        constexpr string_view(const char* data, size_type size) :
            m_Data(data), m_Size(size) {}
        string_view(const char* rhs) : string_view(rhs, strlen(rhs)) {}
        string_view(const string& rhs) : string_view(rhs.data(), rhs.size()) {}

        using const_iterator = const char*;
        const_iterator begin() const { return m_Data; }
        const_iterator end()   const { return m_Data + m_Size; }

        constexpr char operator[](size_type idx) const { return m_Data[idx]; }
        constexpr char        front()  const  { return m_Data[0]; }
        constexpr char        back()   const  { return m_Data[m_Size - 1]; }
        constexpr const char* data()   const { return m_Data; }
        constexpr size_type   size()   const { return m_Size; }
        constexpr size_type   length() const { return m_Size; }
        constexpr bool        empty()  const { return m_Size == 0; }

        void remove_prefix(size_type n) { m_Data += n; m_Size -= n; }
        void remove_suffix(size_type n) { m_Size -= n; }
        size_type copy(char* pDest, size_type count, size_type pos = 0)
        {
            string_view source = substr(pos, count);
            memcpy(pDest, source.data(), source.size());
            return source.size();
        }
        string_view substr(size_type pos = 0, size_type count = npos) const
        {
            pos = min(pos, m_Size);
            count = (count == npos) ? (m_Size - pos) : min(m_Size - pos, count);
            return count ? string_view(m_Data + pos, count) : string_view();
        }

        int compare(const string_view& rhs) const
        {
            int ncmp = strncmp(m_Data, rhs.m_Data, min(m_Size, rhs.m_Size));
            return (ncmp == 0
                    ? static_cast<int>(m_Size) - static_cast<int>(rhs.m_Size)
                    : ncmp);
        }
        int compare(size_type pos1, size_type count1,
                    const string_view& rhs) const
        {
            return substr(pos1, count1).compare(rhs);
        }
        int compare(size_type pos1, size_type count1, const string_view& rhs,
                    size_type pos2, size_type count2) const
        {
            return substr(pos1, count1).compare(rhs.substr(pos2, count2));
        }
        int compare(const string& rhs) const
        {
            return compare(string_view(rhs));
        }
        int compare(size_type pos1, size_type count1, const string& rhs) const
        {
            return substr(pos1, count1).compare(rhs);
        }
        int compare(size_type pos1, size_type count1, const string& rhs,
                    size_type pos2, size_type count2) const
        {
            string_view rhs2(rhs);
            return substr(pos1, count1).compare(rhs2.substr(pos2, count2));
        }
        int compare(const char* rhs) const
        {
            return compare(string_view(rhs, strlen(rhs)));
        }
        int compare(size_type pos1, size_type count1, const char* rhs) const
        {
            return substr(pos1, count1).compare(rhs);
        }
        int compare(size_type pos1, size_type count1, const char* rhs,
                    size_type count2) const
        {
            return substr(pos1, count1).compare(string_view(rhs, count2));
        }

        template<class T> bool operator==(const T& rhs) const
        {
            return compare(rhs) == 0;
        }
        template<class T> bool operator!=(const T& rhs) const
        {
            return compare(rhs) != 0;
        }
        template<class T> bool operator>(const T& rhs) const
        {
            return compare(rhs) >  0;
        }
        template<class T> bool operator>=(const T& rhs) const
        {
            return compare(rhs) >= 0;
        }
        template<class T> bool operator<(const T& rhs) const
        {
            return compare(rhs) <  0;
        }
        template<class T> bool operator<=(const T& rhs) const
        {
            return compare(rhs) <= 0;
        }

        size_type find(const string_view& str, size_type pos = 0) const
        {
            for (; pos + str.size() < m_Size; ++pos)
            {
                if (compare(pos, str.size(), str) == 0)
                {
                    return pos;
                }
            }
            return npos;
        }
        size_type find(char ch, size_type pos = 0) const
        {
            for (; pos < m_Size; ++pos)
            {
                if (ch == m_Data[pos])
                {
                    return pos;
                }
            }
            return npos;
        }
        size_type find(const string& str, size_type pos = 0) const
        {
            return find(string_view(str), pos);
        }
        size_type find(const char* str, size_type pos, size_type count) const
        {
            return find(string_view(str, count), pos);
        }
        size_type find(const char* str, size_type pos = 0) const
        {
            return find(string_view(str), pos);
        }

    private:
        const char* m_Data;
        size_type   m_Size;
    };

    template<typename T> struct hash;
    template<> struct hash<string_view>
    {
        size_t operator()(const string_view& str) const noexcept
        {
            const unsigned char* pData =
                reinterpret_cast<const unsigned char*>(str.data());
            const unsigned char* pEnd = pData + str.size();
            unsigned long long flwHash = 0xcbf29ce484222325ULL;
            while (pData != pEnd)
            {
                flwHash ^= *pData;
                flwHash *= 0x100000001b3ULL;
                pData++;
            }
            return static_cast<size_t>(flwHash);
        }
    };
}

#endif

//--------------------------------------------------------------------
//! \brief Colwert a string_view to a string
//!
//! The main difference between the "real" C++17 string_view and the
//! "experimental" pre-C++17 string_view is how to colwert a
//! string_view to a string:
//! * C++17 uses a string(const string_view&) constructor
//! * pre-C++17 uses a string_view::to_string() method
//!
//! Until we upgrade all of our compilers to C++17, the following
//! function can be used with either implementation.
//!
inline std::string ToString(const std::string_view& stringView)
{
    return std::string(stringView.data(), stringView.size());
}

#endif /// INCLUDED_STRINGVIEW_H
