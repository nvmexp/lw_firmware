/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2019 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef ALIGN_H
#define ALIGN_H

#include <type_traits>

template <class T, class U>
constexpr
typename std::remove_cv<T>::type
AlignDown(
    T t,
    U  alignment,
    typename std::enable_if<std::is_unsigned<T>::value, T>::type* dummy1 = 0,
    typename std::enable_if<std::is_unsigned<U>::value, U>::type* dummy2 = 0
)
{
    typedef typename std::remove_cv<T>::type CleanType;
    return static_cast<CleanType>((t / alignment) * alignment);
}

template <class T, class U>
constexpr
typename std::remove_cv<T>::type
AlignUp(
    T t,
    U  alignment,
    typename std::enable_if<std::is_unsigned<T>::value, T>::type* dummy1 = 0,
    typename std::enable_if<std::is_unsigned<U>::value, U>::type* dummy2 = 0
)
{
    typedef typename std::remove_cv<T>::type CleanType;
    return static_cast<CleanType>(((t + alignment - 1) / alignment) * alignment);
}

template <unsigned int alignment, class T>
constexpr
typename std::remove_cv<T>::type
AlignDown(T t, typename std::enable_if<std::is_unsigned<T>::value, T>::type* dummy = 0)
{
    typedef typename std::remove_cv<T>::type CleanType;
    return static_cast<CleanType>((t / alignment) * alignment);
}

template <unsigned int alignment, class T>
constexpr
typename std::remove_cv<T>::type
AlignUp(T t, typename std::enable_if<std::is_unsigned<T>::value, T>::type* dummy = 0)
{
    typedef typename std::remove_cv<T>::type CleanType;
    return static_cast<CleanType>(((t + alignment - 1) / alignment) * alignment);
}

#endif
