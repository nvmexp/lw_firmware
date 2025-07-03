/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2015, 2017 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef INARCHIVE_H
#define INARCHIVE_H

#include <type_traits>

namespace BitsIO
{
class LoadAccess
{
public:
    template <class Archive, class T>
    static void LoadPrimitive(Archive &ar, T &t)
    {
        ar.Load(t);
    }
};

// Interface of any input archive that defines operators >> and &.
template <class Archive>
class IInArchive
{
public:
    static const bool isSaving = false;
    static const bool isLoading = true;

    template <class T>
    Archive& operator>>(T & t)
    {
        this->This()->LoadOverride(t);
        return *this->This();
    }

    template <class T>
    Archive& operator&(T & t)
    {
        return *(this->This()) >> t;
    }

    const Archive* This() const
    {
        return static_cast<const Archive *>(this);
    }

    Archive* This()
    {
        return static_cast<Archive *>(this);
    }
};

// This class is used to indicate whether a type is primitive. Primitive means
// that the type doesn't have Serialize method and its serialization is defined
// not by its internal structure, but the stream itself. Ideally we would have
// to determine automatically whether a type has a method Serialize, but C++
// doesn't have this level of reflection in order to do it.
template <class T, class Enable = void>
struct SerializationTraits
{
    static constexpr bool isPrimitive = false;
};

#if defined(SUPPORT_TEXT_OUTPUT)
template <class T>
struct SerializationTraits<T, enable_if_t<is_arithmetic<T>::value>>
{
    static constexpr bool isPrimitive = true;
};
#endif

template <class Archive>
struct LoadPrimitive
{
    template <class T>
    static void Load(Archive &ar, T &t)
    {
        LoadAccess::LoadPrimitive(ar, t);
    }
};

template <class Archive>
struct LoadComplex
{
    template <class T>
    static void Load(Archive &ar, const T &t)
    {
        const_cast<T&>(t).Serialize(ar);
    }
};

// This function dispatches serialization between primitive and not primitive
// types.
template <class Archive, class T>
void Load(Archive &ar, T &t)
{
    typedef conditional_t<
        SerializationTraits<T>::isPrimitive
      , LoadPrimitive<Archive>
      , LoadComplex<Archive>
      > Loader;
    Loader::Load(ar, t);
}

// This class Load method is called in response to operator >> call.
template <class Archive>
class CommonIArchive : public IInArchive<Archive>
{
    friend class IInArchive<Archive>;
protected:
    template <class T>
    void LoadOverride(T &t)
    {
        Load(*this->This(), t);
    }
};

} // namespace BitsIO

#endif
