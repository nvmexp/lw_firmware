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

#ifndef OUTARCHIVE_H
#define OUTARCHIVE_H

#include <sstream>
#include <utility>
#include <type_traits>

#include "splitserialization.h"

namespace BitsIO
{

class SaveAccess
{
public:
    template <class Archive, class T>
    static void SavePrimitive(Archive &ar, const T &t)
    {
        ar.Save(t);
    }
};

#if defined(SUPPORT_TEXT_OUTPUT)
template <class T>
class Lwp : public pair<const char *, T *>
{
public:
    Lwp(const char * name, T & t)
      : pair<const char *, T *>(name, &t)
      , m_printIdx1(false)
      , m_printIdx2(false)
    {}

    Lwp(const char * name, T & t, UINT32 idx1)
      : pair<const char *, T *>(name, &t)
      , m_idx1(idx1)
      , m_printIdx1(true)
      , m_printIdx2(false)
    {}

    Lwp(const char * name, T & t, UINT32 idx1, UINT32 idx2)
      : pair<const char *, T *>(name, &t)
      , m_idx1(idx1)
      , m_idx2(idx2)
      , m_printIdx1(true)
      , m_printIdx2(true)
    {}

    const char * Name() const
    {
        return this->first;
    }

    T & Value() const
    {
        return *(this->second);
    }

    const T & ConstValue() const
    {
        return *(this->second);
    }

    template <class Archive>
    void Serialize(Archive& sm)
    {
        SplitSerialization(sm, *this);
    }

    template <class Archive>
    void Load(Archive& sm)
    {
        sm >> Value();
    }

    template <class Archive>
    void Save(Archive& sm) const
    {
        sm << ConstValue();
    }

    UINT32 GetIdx1() const
    {
        return m_idx1;
    }

    bool HasIdx1() const
    {
        return m_printIdx1;
    }

    UINT32 GetIdx2() const
    {
        return m_idx2;
    }

    bool HasIdx2() const
    {
        return m_printIdx2;
    }

private:
    UINT32 m_idx1;
    UINT32 m_idx2;
    bool   m_printIdx1;
    bool   m_printIdx2;
};

template <class T>
inline const
Lwp<T> lwp(const char * name, T & t)
{
    return Lwp<T>(name, t);
}

template <class T>
inline const
Lwp<T> lwp(const char * name, T & t, UINT32 idx)
{
    return Lwp<T>(name, t, idx);
}

template <class T>
inline const
Lwp<T> lwp(const char * name, T & t, UINT32 idx1, UINT32 idx2)
{
    return Lwp<T>(name, t, idx1, idx2);
}
#endif

// Interface of any output archive that defines operators << and &.
template <class Archive>
class IOutArchive
{
public:
    static const bool isSaving = true;
    static const bool isLoading = false;

    template <class T>
    Archive& operator<<(const T &t)
    {
        this->This()->SaveOverride(t);
        return *this->This();
    }

    template <class T>
    Archive& operator&(const T & t)
    {
        return *(this->This()) << t;
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

template <class Archive>
struct SavePrimitive
{
    template <class T>
    static void Save(Archive &ar, const T &t)
    {
        SaveAccess::SavePrimitive(ar, t);
    }
};

template <class Archive>
struct SaveComplex
{
    template <class T>
    static void Save(Archive &ar, const T &t)
    {
        const_cast<T&>(t).Serialize(ar);
    }
};

// This function dispatches serialization between primitive and not primitive
// types.
template <class Archive, class T>
void Save(Archive &ar, const T &t)
{
    typedef conditional_t<
        SerializationTraits<T>::isPrimitive
      , SavePrimitive<Archive>
      , SaveComplex<Archive>
      > Saver;
    Saver::Save(ar, t);
}

// This class Load method is called in response to operator >> call.
template <class Archive>
class CommonOArchive : public IOutArchive<Archive>
{
    friend class IOutArchive<Archive>;
protected:
    template <class T>
    void SaveOverride(const T &t)
    {
        Save(*this->This(), t);
    }
};

} // namespace BitsIO

#if defined(SUPPORT_TEXT_OUTPUT)
#define LWP(name) BitsIO::lwp(#name, name)
#define LWP1(name, idx) BitsIO::lwp(#name, name[idx], idx)
#define LWP2(name, idx1, idx2) BitsIO::lwp(#name, name[idx1][idx2], idx1, idx2)
#else
#define LWP(name) name
#define LWP1(name, idx) name[idx]
#define LWP2(name, idx1, idx2) name[idx1][idx2]
#endif

#endif
