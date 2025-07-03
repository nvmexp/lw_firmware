/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2014 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef SPLITSERIALIZATION_H
#define SPLITSERIALIZATION_H

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

namespace BitsIO
{

namespace Detail
{
    template <class Archive, class T>
    struct Saver
    {
        static void Ilwoke(Archive &st, const T &t)
        {
            t.Save(st);
        }
    };

    template <class Archive, class T>
    struct Loader
    {
        static void Ilwoke(Archive &st, T &t)
        {
            t.Load(st);
        }
    };

    template <bool isSaving, class Archive, class T>
    struct SaverLoaderSelector {};

    template <class Archive, class T>
    struct SaverLoaderSelector<true, Archive, T>
    {
        typedef Saver<Archive, T> Type;
    };

    template <class Archive, class T>
    struct SaverLoaderSelector<false, Archive, T>
    {
        typedef Loader<Archive, T> Type;
    };
} // namespace Detail

//! Ilwoke either Load or Save method depending on stream properties.
template <class Archive, class T>
inline void SplitSerialization(Archive &st, T &t)
{
    typedef typename Detail::SaverLoaderSelector<
        Archive::isSaving
      , Archive
      , T
      >::Type Ser;
    Ser::Ilwoke(st, t);
}

} // namespace BitsIO

#endif // SPLITSERIALIZATION_H
