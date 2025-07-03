/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2014, 2017 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef VC1SYNTAX_H
#define VC1SYNTAX_H

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <cstddef>
#include <vector>

#include "core/include/types.h"
#include "core/include/utility.h"

#include "inbitstream.h"
#include "nbits.h"
#include "splitserialization.h"

namespace VC1
{
using Vid::NBits;
using Utility::ArraySize;

//! Start code suffixes that signifies types of BDUs. Described in section E.5
//! of Annex E of SMPTE 421M-2006, Table 256.
enum BDUType
{
    BDU_TYPE_END_OF_SEQ                = 0x0A,
    BDU_TYPE_SLICE                     = 0x0B,
    BDU_TYPE_FIELD                     = 0x0C,
    BDU_TYPE_FRAME                     = 0x0D,
    BDU_TYPE_ENTRY_POINT               = 0x0E,
    BDU_TYPE_SEQUENCE                  = 0x0F,
    BDU_TYPE_SLICE_LVL_USER_DATA       = 0x1B,
    BDU_TYPE_FIELD_LVL_USER_DATA       = 0x1C,
    BDU_TYPE_FRAME_LVL_USER_DATA       = 0x1D,
    BDU_TYPE_ENTRY_POINT_LVL_USER_DATA = 0x1E,
    BDU_TYPE_SEQ_LVL_USER_DATA         = 0x1F
};

enum PictureType
{
    PICTURE_TYPE_P  = 0,
    PICTURE_TYPE_B  = 1,
    PICTURE_TYPE_I  = 2,
    PICTURE_TYPE_BI = 3
};

enum FrameCodingMode
{
    FCM_PROGRESSIVE     = 0,
    FCM_FRAME_INTERLACE = 2,
    FCM_FIELD_INTERLACE = 3
};

enum BitPlaneIMode
{
    BITPLANE_IMODE_NORM2,
    BITPLANE_IMODE_NORM6,
    BITPLANE_IMODE_ROWSKIP,
    BITPLANE_IMODE_COLSKIP,
    BITPLANE_IMODE_DIFF2,
    BITPLANE_IMODE_DIFF6,
    BITPLANE_IMODE_RAW
};

enum QuantMode
{
    QUANT_FRAME_IMPLICIT,
    QUANT_FRAME_EXPLICIT,
    QUANT_NON_UNIFORM,
    QUANT_UNIFORM
};

enum OverlapSmoothing
{
    OVERLAP_SMOOTHING_NONE,
    OVERLAP_SMOOTHING_ALL,
    OVERLAP_SMOOTHING_SELECT
};

enum DQProfile
{
    DQPROFILE_ALL_FOUR_EDGES,
    DQPROFILE_DOUBLE_EDGE,
    DQPROFILE_SINGLE_EDGE,
    DQPROFILE_ALL_MACROBLOCKS
};

template <unsigned int N>
class Unary
{
    UINT32 m_v;

public:
    Unary() : m_v(0)
    {}

    Unary(UINT32 v) : m_v(v)
    {}

    operator UINT32() const
    {
        return m_v;
    }

    Unary& operator =(UINT32 rhs)
    {
        m_v = rhs;

        return *this;
    }

    template <class Stream>
    void Serialize(Stream& sm)
    {
        SplitSerialization(sm, *this);
    }

    template <class Stream>
    void Load(Stream& sm)
    {
        unsigned int i;
        for (i = 0; N > i && 0 != sm.Read(1); ++i);
        m_v = static_cast<UINT32>(i);
    }
};

template <class InputIterator>
class PrimitiveTypesOnIBitStream
{
#if defined(__GNUC__) && __GNUC__ < 3
public:
#else
    friend class BitsIO::LoadAccess;
protected:
#endif
    PrimitiveTypesOnIBitStream(InputIterator start, InputIterator finish)
        : m_inBitStream(start, finish)
    {}

    UINT64 BitsLeft() const
    {
        return m_inBitStream.BitsLeft();
    }

    UINT64 Read(size_t n)
    {
        return m_inBitStream.Read(n);
    }

    void Skip(size_t n)
    {
        m_inBitStream.Skip(n);
    }

    template <unsigned int N>
    void Load(NBits<N> &u)
    {
        typedef typename NBits<N>::StorageType StorageType;
        u = static_cast<StorageType>(m_inBitStream.Read(N));
    }

    void Load(NBits<1> &u)
    {
        u = 0 != m_inBitStream.Read(1);
    }

    template <unsigned int N>
    void Load(Unary<N> &u)
    {
        unsigned int i;
        for (i = 0; N > i && 0 != m_inBitStream.Read(1); ++i);
        u = static_cast<UINT32>(i);
    }

private:
    BitsIO::InBitStream<InputIterator> m_inBitStream;
};
} // namespace VC1

namespace BitsIO
{
    template <unsigned int N>
    struct SerializationTraits<VC1::Unary<N> >
    {
        static constexpr bool isPrimitive = true;
    };
} // namespace BitsIO

namespace VC1
{

template <class Archive, class InputIterator>
class BitIArchiveImpl :
    public PrimitiveTypesOnIBitStream<InputIterator>
  , public BitsIO::CommonIArchive<Archive>
{
public:
    BitIArchiveImpl(InputIterator start, InputIterator finish)
      : PrimitiveTypesOnIBitStream<InputIterator>(start, finish)
    {}

    UINT64 BitsLeft() const
    {
        return PrimitiveTypesOnIBitStream<InputIterator>::BitsLeft();
    }

    UINT64 Read(size_t n)
    {
        return PrimitiveTypesOnIBitStream<InputIterator>::Read(n);
    }

    void Skip(size_t n)
    {
        PrimitiveTypesOnIBitStream<InputIterator>::Skip(n);
    }
};

template <class InputIterator>
class BitIArchive :
    public BitIArchiveImpl<BitIArchive<InputIterator>, InputIterator>
{
public:
    BitIArchive(InputIterator start, InputIterator finish)
      : BitIArchiveImpl<BitIArchive<InputIterator>, InputIterator>(start, finish)
    {}
};

class BFractiolwLC
{
    UINT08 m_numerator;
    UINT08 m_denominator;

public:
    BFractiolwLC()
      : m_numerator(1)
      , m_denominator(1)
    {}

    UINT08 GetNumerator() const
    {
        return m_numerator;
    }

    UINT08 GetDenominator() const
    {
        return m_denominator;
    }

    template <class Stream>
    void Serialize(Stream& sm)
    {
        SplitSerialization(sm, *this);
    }

    template <class Stream>
    void Load(Stream& sm)
    {
        // Auto-generated from Table 40 of SMPTE 421M
        if (0 == sm.Read(1))
        {
            if (0 == sm.Read(1))
            {
                if (0 == sm.Read(1))
                {
                    m_numerator = 1;
                    m_denominator = 2;
                }
                else
                {
                    m_numerator = 1;
                    m_denominator = 3;
                }
            }
            else
            {
                if (0 == sm.Read(1))
                {
                    m_numerator = 2;
                    m_denominator = 3;
                }
                else
                {
                    m_numerator = 1;
                    m_denominator = 4;
                }
            }
        }
        else
        {
            if (0 == sm.Read(1))
            {
                if (0 == sm.Read(1))
                {
                    m_numerator = 3;
                    m_denominator = 4;
                }
                else
                {
                    m_numerator = 1;
                    m_denominator = 5;
                }
            }
            else
            {
                if (0 == sm.Read(1))
                {
                    m_numerator = 2;
                    m_denominator = 5;
                }
                else
                {
                    if (0 == sm.Read(1))
                    {
                        if (0 == sm.Read(1))
                        {
                            if (0 == sm.Read(1))
                            {
                                if (0 == sm.Read(1))
                                {
                                    m_numerator = 3;
                                    m_denominator = 5;
                                }
                                else
                                {
                                    m_numerator = 4;
                                    m_denominator = 5;
                                }
                            }
                            else
                            {
                                if (0 == sm.Read(1))
                                {
                                    m_numerator = 1;
                                    m_denominator = 6;
                                }
                                else
                                {
                                    m_numerator = 5;
                                    m_denominator = 6;
                                }
                            }
                        }
                        else
                        {
                            if (0 == sm.Read(1))
                            {
                                if (0 == sm.Read(1))
                                {
                                    m_numerator = 1;
                                    m_denominator = 7;
                                }
                                else
                                {
                                    m_numerator = 2;
                                    m_denominator = 7;
                                }
                            }
                            else
                            {
                                if (0 == sm.Read(1))
                                {
                                    m_numerator = 3;
                                    m_denominator = 7;
                                }
                                else
                                {
                                    m_numerator = 4;
                                    m_denominator = 7;
                                }
                            }
                        }
                    }
                    else
                    {
                        if (0 == sm.Read(1))
                        {
                            if (0 == sm.Read(1))
                            {
                                if (0 == sm.Read(1))
                                {
                                    m_numerator = 5;
                                    m_denominator = 7;
                                }
                                else
                                {
                                    m_numerator = 6;
                                    m_denominator = 7;
                                }
                            }
                            else
                            {
                                if (0 == sm.Read(1))
                                {
                                    m_numerator = 1;
                                    m_denominator = 8;
                                }
                                else
                                {
                                    m_numerator = 3;
                                    m_denominator = 8;
                                }
                            }
                        }
                        else
                        {
                            if (0 == sm.Read(1))
                            {
                                if (0 == sm.Read(1))
                                {
                                    m_numerator = 5;
                                    m_denominator = 8;
                                }
                                else
                                {
                                    m_numerator = 7;
                                    m_denominator = 8;
                                }
                            }
                            else
                            {
                                if (0 == sm.Read(1))
                                {
                                    // SMPTE reserved
                                }
                                else
                                {
                                    // Reserved in the advanced profile
                                }
                            }
                        }
                    }
                }
            }
        }
    }
};

class IModeVLC
{
    BitPlaneIMode m_v;
public:
    IModeVLC()
    {}

    IModeVLC(BitPlaneIMode v) : m_v(v)
    {}

    operator BitPlaneIMode() const
    {
        return m_v;
    }

    IModeVLC& operator =(BitPlaneIMode rhs)
    {
        m_v = rhs;

        return *this;
    }

    template <class Stream>
    void Serialize(Stream& sm)
    {
        SplitSerialization(sm, *this);
    }

    template <class Stream>
    void Load(Stream& sm)
    {
        // Auto-generated from Table 69 of SMPTE 421M
        if (0 == sm.Read(1))
        {
            if (0 == sm.Read(1))
            {
                if (0 == sm.Read(1))
                {
                    if (0 == sm.Read(1))
                        m_v = BITPLANE_IMODE_RAW;
                    else
                        m_v = BITPLANE_IMODE_DIFF6;
                }
                else
                    m_v = BITPLANE_IMODE_DIFF2;
            }
            else
            {
                if (0 == sm.Read(1))
                    m_v = BITPLANE_IMODE_ROWSKIP;
                else
                    m_v = BITPLANE_IMODE_COLSKIP;
            }
        }
        else
        {
            if (0 == sm.Read(1))
                m_v = BITPLANE_IMODE_NORM2;
            else
                m_v = BITPLANE_IMODE_NORM6;
        }
    }
};

class BitPlane
{
private:
    typedef vector<UINT08> Container;

public:
    void SetDim(size_t width, size_t height)
    {
        m_width = width;
        m_height = height;
        m_data.resize(width * height);
    }

    UINT08 at(size_t x, size_t y) const
    {
        return m_data[y * m_width + x];
    }

    UINT08& at(size_t x, size_t y)
    {
        return m_data[y * m_width + x];
    }

    template <class Stream>
    void Serialize(Stream& sm)
    {
        SplitSerialization(sm, *this);
    }

    template <class Stream>
    void Load(Stream& sm)
    {
        sm >> ILWERT;
        sm >> IMODE;
        switch (IMODE)
        {
            case BITPLANE_IMODE_NORM2:
            case BITPLANE_IMODE_DIFF2:
            {
                ReadNorm2AndDiff2(sm);
                break;
            }
            case BITPLANE_IMODE_NORM6:
            case BITPLANE_IMODE_DIFF6:
            {
                ReadNorm6AndDiff6(sm);
                break;
            }
            case BITPLANE_IMODE_ROWSKIP:
            {
                for (size_t y = 0; y < m_height; ++y)
                {
                    if (0 == sm.Read(1))
                    {
                        ClearRow(y);
                    }
                    else
                    {
                        ReadRow(sm, y);
                    }
                }
                break;
            }
            case BITPLANE_IMODE_COLSKIP:
            {
                for (size_t x = 0; x < m_width; ++x)
                {
                    if (0 == sm.Read(1))
                    {
                        ClearColumn(x);
                    }
                    else
                    {
                        ReadColumn(sm, x);
                    }
                }
                break;
            }
            case BITPLANE_IMODE_RAW:
            {
                break;
            }
        }

        if (BITPLANE_IMODE_DIFF2 == IMODE || BITPLANE_IMODE_DIFF6 == IMODE)
        {
            Container::value_type A = ILWERT ? 1 : 0;

            at(0, 0) ^= A;
            for (size_t i = 1; m_width > i; ++i)
            {
                at(i, 0) ^= at(i - 1, 0);
            }
            for (size_t j = 1; m_height > j; ++j)
            {
                at(0, j) ^= at(0, j - 1);
                for (size_t i = 1; m_width > i; ++i)
                {
                    if (at(i - 1, j) != at(i, j - 1))
                    {
                        at(i, j) ^= A;
                    }
                    else
                    {
                        at(i, j) ^= at(i - 1, j);
                    }
                }
            }
        } else if (ILWERT) {
            Container::iterator it;
            for (it = m_data.begin(); m_data.end() != it; ++it)
            {
                *it = !*it;
            }
        }
    }

private:
    template <class Stream>
    void ReadColumn(Stream& sm, size_t x)
    {
        Container::iterator it = m_data.begin() + x;
        for (size_t y = 0; m_height > y; ++y, it += m_width)
        {
            *it = static_cast<UINT08>(sm.Read(1));
        }
    }

    void ClearColumn(size_t x);

    template <class Stream>
    void ReadRow(Stream& sm, size_t y)
    {
        ReadRow(sm, 0, y);
    }

    template <class Stream>
    void ReadRow(Stream& sm, size_t fromCol, size_t y)
    {
        Container::iterator it = m_data.begin() + fromCol + y * m_width;
        for (size_t x = fromCol; m_width > x; ++x)
        {
            *it++ = static_cast<UINT08>(sm.Read(1));
        }
    }

    void ClearRow(size_t y)
    {
        ClearRow(0, y);
    }

    void ClearRow(size_t fromCol, size_t y);

    template <class Stream>
    void ReadNorm2AndDiff2(Stream& sm)
    {
        Container::iterator it = m_data.begin();

        if ((m_height * m_width) & 1)
        {
            *it++ = static_cast<UINT08>(sm.Read(1));
        }

        UINT08 code;
        // Auto-generated from Table 80 of SMPTE 421M
        if (0 == sm.Read(1))
            code = 0;
        else
        {
            if (0 == sm.Read(1))
            {
                if (0 == sm.Read(1))
                    code = 2;
                else
                    code = 1;
            }
            else code = 3;
        }
        for (; m_data.end() != it;)
        {
            *it++ = code >> 1;
            *it++ = code & 1;
        }
    }

    template <class Stream>
    void ReadNorm6AndDiff6(Stream& sm)
    {
        if (0 == (m_height % 3) && 0 != (m_width % 3))
        {
            for (size_t y = 0; m_height > y; y += 3)
            {
                for (size_t x = m_width % 2; m_width > x; x += 2)
                {
                    UINT08 code = ReadTileCode(sm);
                    at(x,     y)     =  code       & 1;
                    at(x + 1, y)     = (code >> 1) & 1;
                    at(x,     y + 1) = (code >> 2) & 1;
                    at(x + 1, y + 1) = (code >> 3) & 1;
                    at(x,     y + 2) = (code >> 4) & 1;
                    at(x + 1, y + 2) = (code >> 5) & 1;
                }
            }
            if (0 != m_width % 2)
            {
                if (0 == sm.Read(1))
                {
                    ClearColumn(0);
                }
                else
                {
                    ReadColumn(sm, 0);
                }
            }
        }
        else
        {
            for (size_t y = m_height % 2; m_height > y; y += 2)
            {
                for (size_t x = m_width % 3; m_width > x; x += 3)
                {
                    UINT08 code = ReadTileCode(sm);
                    at(x,     y)     =  code       & 1;
                    at(x + 1, y)     = (code >> 1) & 1;
                    at(x + 2, y)     = (code >> 2) & 1;
                    at(x,     y + 1) = (code >> 3) & 1;
                    at(x + 1, y + 1) = (code >> 4) & 1;
                    at(x + 2, y + 1) = (code >> 5) & 1;
                }

                // columns and a row not covered by tiles
                size_t firstCols = m_width % 3;
                for (size_t x = 0; firstCols > x; ++x)
                {
                    if (0 == sm.Read(1))
                    {
                        ClearColumn(x);
                    }
                    else
                    {
                        ReadColumn(sm, x);
                    }
                }
                if (0 != m_height % 2)
                {
                    if (0 == sm.Read(1))
                    {
                        ClearRow(firstCols, 0);
                    }
                    else
                    {
                        ReadRow(sm, firstCols, 0);
                    }
                }
            }
        }
    }

    template <class Stream>
    UINT08 ReadTileCode(Stream& sm)
    {
        // Auto-generated from Table 81 of SMPTE 421M
        if (0 == sm.Read(1))
        {
            if (0 == sm.Read(1))
            {
                if (0 == sm.Read(1))
                {
                    if (0 == sm.Read(1))
                    {
                        if (0 == sm.Read(1))
                        {
                            if (0 == sm.Read(1))
                            {
                                if (0 == sm.Read(1))
                                {
                                    if (0 == sm.Read(1))
                                        return 3;
                                    else
                                        return 5;
                                }
                                else
                                {
                                    if (0 == sm.Read(1))
                                        return 6;
                                    else
                                        return 9;
                                }
                            }
                            else
                            {
                                if (0 == sm.Read(1))
                                {
                                    if (0 == sm.Read(1))
                                        return 10;
                                    else
                                        return 12;
                                }
                                else
                                {
                                    if (0 == sm.Read(1))
                                        return 17;
                                    else
                                        return 18;
                                }
                            }
                        }
                        else
                        {
                            if (0 == sm.Read(1))
                            {
                                if (0 == sm.Read(1))
                                {
                                    if (0 == sm.Read(1))
                                        return 20;
                                    else
                                        return 24;
                                }
                                else
                                {
                                    if (0 == sm.Read(1))
                                        return 33;
                                    else
                                        return 34;
                                }
                            }
                            else
                            {
                                if (0 == sm.Read(1))
                                {
                                    if (0 == sm.Read(1))
                                        return 36;
                                    else
                                        return 40;
                                }
                                else
                                {
                                    sm.Skip(1);
                                    return 48;
                                }
                            }
                        }
                    }
                    else
                    {
                        if (0 == sm.Read(1))
                        {
                            if (0 == sm.Read(1))
                            {
                                if (0 == sm.Read(1))
                                {
                                    if (0 == sm.Read(1))
                                    {
                                        sm.Skip(2);
                                        return 35;
                                    }
                                    else
                                    {
                                        if (0 == sm.Read(1))
                                        {
                                            sm.Skip(1);
                                            return 37;
                                        }
                                        else
                                        {
                                            if (0 == sm.Read(1))
                                                return 38;
                                            else
                                                return 7;
                                        }
                                    }
                                }
                                else
                                {
                                    if (0 == sm.Read(1))
                                    {
                                        if (0 == sm.Read(1))
                                        {
                                            sm.Skip(1);
                                            return 41;
                                        }
                                        else
                                        {
                                            if (0 == sm.Read(1))
                                                return 42;
                                            else
                                                return 11;
                                        }
                                    }
                                    else
                                    {
                                        if (0 == sm.Read(1))
                                        {
                                            if (0 == sm.Read(1))
                                                return 44;
                                            else
                                                return 13;
                                        }
                                        else
                                        {
                                            sm.Skip(1);
                                            return 14;
                                        }
                                    }
                                }
                            }
                            else
                            {
                                if (0 == sm.Read(1))
                                {
                                    if (0 == sm.Read(1))
                                    {
                                        if (0 == sm.Read(1))
                                        {
                                            sm.Skip(1);
                                            return 49;
                                        }
                                        else
                                        {
                                            if (0 == sm.Read(1))
                                                return 50;
                                            else
                                                return 19;
                                        }
                                    }
                                    else
                                    {
                                        if (0 == sm.Read(1))
                                        {
                                            if (0 == sm.Read(1))
                                                return 52;
                                            else
                                                return 21;
                                        }
                                        else
                                        {
                                            sm.Skip(1);
                                            return 22;
                                        }
                                    }
                                }
                                else
                                {
                                    if (0 == sm.Read(1))
                                    {
                                        if (0 == sm.Read(1))
                                        {
                                            if (0 == sm.Read(1))
                                                return 56;
                                            else
                                                return 25;
                                        }
                                        else
                                        {
                                            sm.Skip(1);
                                            return 26;
                                        }
                                    }
                                    else
                                    {
                                        sm.Skip(2);
                                        return 28;
                                    }
                                }
                            }
                        }
                        else
                        {
                            if (0 == sm.Read(1))
                            {
                                if (0 == sm.Read(1))
                                {
                                    if (0 == sm.Read(1))
                                    {
                                        sm.Skip(1);
                                        if (0 == sm.Read(1))
                                        {
                                            if (0 == sm.Read(1))
                                            {
                                                if (0 == sm.Read(1))
                                                {
                                                    if (0 == sm.Read(1))
                                                        return 60;
                                                    else
                                                        return 58;
                                                }
                                                else
                                                {
                                                    if (0 == sm.Read(1))
                                                        return 57;
                                                    else
                                                        return 54;
                                                }
                                            }
                                            else
                                            {
                                                if (0 == sm.Read(1))
                                                {
                                                    if (0 == sm.Read(1))
                                                        return 53;
                                                    else
                                                        return 51;
                                                }
                                                else
                                                {
                                                    if (0 == sm.Read(1))
                                                        return 46;
                                                    else
                                                        return 45;
                                                }
                                            }
                                        }
                                        else
                                        {
                                            if (0 == sm.Read(1))
                                            {
                                                if (0 == sm.Read(1))
                                                {
                                                    if (0 == sm.Read(1))
                                                        return 43;
                                                    else
                                                        return 39;
                                                }
                                                else
                                                {
                                                    if (0 == sm.Read(1))
                                                        return 30;
                                                    else
                                                        return 29;
                                                }
                                            }
                                            else
                                            {
                                                if (0 == sm.Read(1))
                                                {
                                                    if (0 == sm.Read(1))
                                                        return 27;
                                                    else
                                                        return 23;
                                                }
                                                else
                                                {
                                                    sm.Skip(1);
                                                    return 15;
                                                }
                                            }
                                        }
                                    }
                                    else
                                    {
                                        if (0 == sm.Read(1))
                                            return 62;
                                        else
                                            return 61;
                                    }
                                }
                                else
                                {
                                    if (0 == sm.Read(1))
                                    {
                                        if (0 == sm.Read(1))
                                            return 59;
                                        else
                                            return 55;
                                    }
                                    else
                                    {
                                        if (0 == sm.Read(1))
                                            return 47;
                                        else
                                            return 31;
                                    }
                                }
                            }
                            else
                                return 63;
                        }
                    }
                }
                else
                {
                    if (0 == sm.Read(1))
                        return 1;
                    else
                        return 2;
                }
            }
            else
            {
                if (0 == sm.Read(1))
                {
                    if (0 == sm.Read(1))
                        return 4;
                    else
                        return 8;
                }
                else
                {
                    if (0 == sm.Read(1))
                        return 16;
                    else
                        return 32;
                }
            }
        }
        else
            return 0;
    }

    NBits<1> ILWERT;
    IModeVLC IMODE;

    size_t         m_width;
    size_t         m_height;
    vector<UINT08> m_data;
};

class CondoverVLC
{
    OverlapSmoothing m_v;
public:
    CondoverVLC()
    {}

    CondoverVLC(OverlapSmoothing v) : m_v(v)
    {}

    operator OverlapSmoothing() const
    {
        return m_v;
    }

    CondoverVLC& operator =(OverlapSmoothing rhs)
    {
        m_v = rhs;

        return *this;
    }

    template <class Stream>
    void Serialize(Stream& sm)
    {
        SplitSerialization(sm, *this);
    }

    template <class Stream>
    void Load(Stream& sm)
    {
        // Auto-generated from Section 7.1.1.29 of SMPTE 421M
        if (0 == sm.Read(1))
            m_v = OVERLAP_SMOOTHING_NONE;
        else
        {
            if (0 == sm.Read(1))
                m_v = OVERLAP_SMOOTHING_ALL;
            else
                m_v = OVERLAP_SMOOTHING_SELECT;
        }
    }
};

class TransacfrmVlc
{
    UINT08 m_v;

public:
    TransacfrmVlc()
    {}

    TransacfrmVlc(UINT08 v) : m_v(v)
    {}

    operator UINT08() const
    {
        return m_v;
    }

    TransacfrmVlc& operator =(UINT08 rhs)
    {
        m_v = rhs;

        return *this;
    }

    template <class Stream>
    void Serialize(Stream& sm)
    {
        SplitSerialization(sm, *this);
    }

    template <class Stream>
    void Load(Stream& sm)
    {
        // Auto-generated from Table 39 of SMPTE 421M
        if (0 == sm.Read(1))
            m_v = 0;
        else
        {
            if (0 == sm.Read(1))
                m_v = 1;
            else
                m_v = 2;
        }
    }
};

//! Bitstream Data Unit (BDU). See Annex E of SMPTE 421M-2006.
class BDU
{
public:
    typedef vector<UINT08>::const_iterator Iterator;
    BDU()
      : m_startOffset(0)
      , m_ebduDataSize(0)
    {}

    size_t GetEbduDataSize() const
    {
        return m_ebduDataSize;
    }

    BDUType GetBduType() const
    {
        return m_bduType;
    }

    Iterator RBDUBegin()
    {
        return m_rbduData.begin();
    }

    Iterator RBDUEnd()
    {
        return m_rbduData.end();
    }

    bool Read(const UINT08 *bufStart, size_t searchFromOffset, size_t bufSize);

private:
    size_t FindNextStartCode(
        const UINT08 *bufStart,
        size_t searchFromOffset,
        size_t bufSize
    );

    size_t         m_startOffset;
    size_t         m_ebduDataSize;
    vector<UINT08> m_rbduData;
    BDUType        m_bduType;
};

class HrdParam
{
public:
    template <class Stream>
    void Serialize(Stream& sm)
    {
        SplitSerialization(sm, *this);
    }

    template <class Stream>
    void Load(Stream& sm)
    {
        sm >> HRD_NUM_LEAKY_BUCKETS;
        sm >> BIT_RATE_EXPONENT;
        sm >> BUFFER_SIZE_EXPONENT;
        for (UINT32 n = 1; n <= static_cast<UINT32>(HRD_NUM_LEAKY_BUCKETS); ++n)
        {
            sm >> HRD_RATE[n - 1];
            sm >> HRD_BUFFER[n - 1];
        }
    }

    NBits<5>  HRD_NUM_LEAKY_BUCKETS;
    NBits<4>  BIT_RATE_EXPONENT;
    NBits<4>  BUFFER_SIZE_EXPONENT;
    NBits<16> HRD_RATE[31];
    NBits<16> HRD_BUFFER[31];
};

template <class Derived>
class SeqLayerParamSrc
{
public:
    bool GetPostprocessingFlag() const
    {
        return This()->GetPostprocessingFlag();
    }

    UINT32 GetMaxCodedWidth() const
    {
        return This()->GetMaxCodedWidth();
    }

    UINT32 GetMaxCodedHeight() const
    {
        return This()->GetMaxCodedHeight();
    }

    bool GetPullDownFlag() const
    {
        return This()->GetPullDownFlag();
    }

    bool GetInterlace() const
    {
        return This()->GetInterlace();
    }

    bool GetFrameCounterFlag() const
    {
        return This()->GetFrameCounterFlag();
    }

    bool GetFrameInterpolationFlag() const
    {
        return This()->GetFrameInterpolationFlag();
    }

    bool GetProgressiveSegmentedFrame() const
    {
        return This()->GetProgressiveSegmentedFrame();
    }

    bool GetHrdParamFlag() const
    {
        return This()->GetHrdParamFlag();
    }

    UINT32 GetHrdNumLeakyBuckets() const
    {
        return This()->GetHrdNumLeakyBuckets();
    }

private:
    const Derived* This() const
    {
        return static_cast<const Derived*>(this);
    }
};

template <class Derived>
class EntryPLayerParamSrc
{
public:
    bool GetPanScanPresentFlag() const
    {
        return This()->GetPanScanPresentFlag();
    }

    UINT08 GetMBQuantizationFlag() const
    {
        return This()->GetMBQuantizationFlag();
    }

    bool GetOverlap() const
    {
        return This()->GetOverlap();
    }

    UINT32 GetQuantizerSpecifier() const
    {
        return This()->GetQuantizerSpecifier();
    }

    UINT32 GetCodedWidth() const
    {
        return This()->GetCodedWidth();
    }

    UINT32 GetCodedHeight() const
    {
        return This()->GetCodedHeight();
    }

private:
    const Derived* This() const
    {
        return static_cast<const Derived*>(this);
    }
};

class SequenceLayer : public SeqLayerParamSrc<SequenceLayer>
{
public:
    bool GetPostprocessingFlag() const
    {
        return POSTPROCFLAG;
    }

    UINT32 GetMaxCodedWidth() const
    {
        return m_maxCodedWidth;
    }

    UINT32 GetMaxCodedHeight() const
    {
        return m_maxCodedHeight;
    }

    bool GetPullDownFlag() const
    {
        return PULLDOWN;
    }

    bool GetInterlace() const
    {
        return INTERLACE;
    }

    bool GetFrameCounterFlag() const
    {
        return TFCNTRFLAG;
    }

    bool GetFrameInterpolationFlag() const
    {
        return FINTERPFLAG;
    }

    bool GetProgressiveSegmentedFrame() const
    {
        return PSF;
    }

    bool GetHrdParamFlag() const
    {
        return HRD_PARAM_FLAG;
    }

    UINT32 GetHrdNumLeakyBuckets() const
    {
        return HRD_PARAM.HRD_NUM_LEAKY_BUCKETS;
    }

    template <class Stream>
    void Serialize(Stream& sm)
    {
        SplitSerialization(sm, *this);
    }

    template <class Stream>
    void Load(Stream& sm)
    {
        sm >> PROFILE;
        sm >> LEVEL;
        sm >> COLORDIFF_FORMAT;
        sm >> FRMRTQ_POSTPROC;
        sm >> BITRTQ_POSTPROC;
        sm >> POSTPROCFLAG;
        sm >> MAX_CODED_WIDTH;
        m_maxCodedWidth = (MAX_CODED_WIDTH + 1) * 2;
        sm >> MAX_CODED_HEIGHT;
        m_maxCodedHeight = (MAX_CODED_HEIGHT + 1) * 2;
        sm >> PULLDOWN;
        sm >> INTERLACE;
        sm >> TFCNTRFLAG;
        sm >> FINTERPFLAG;
        NBits<1> RESERVED;
        sm >> RESERVED;
        sm >> PSF;
        sm >> DISPLAY_EXT;
        if (DISPLAY_EXT)
        {
            sm >> DISP_HORIZ_SIZE;
            sm >> DISP_VERT_SIZE;
            sm >> ASPECT_RATIO_FLAG;
            if (ASPECT_RATIO_FLAG)
            {
                sm >> ASPECT_RATIO;
                if (15 == ASPECT_RATIO)
                {
                    sm >> ASPECT_HORIZ_SIZE;
                    sm >> ASPECT_VERT_SIZE;
                }
            }
            sm >> FRAMERATE_FLAG;
            if (FRAMERATE_FLAG)
            {
                sm >> FRAMERATEIND;
                if (!FRAMERATEIND)
                {
                    sm >> FRAMERATENR;
                    sm >> FRAMERATEDR;
                }
                else
                {
                    sm >> FRAMERATEEXP;
                }
            }
            sm >> COLOR_FORMAT_FLAG;
            if (COLOR_FORMAT_FLAG)
            {
                sm >> COLOR_PRIM;
                sm >> TRANSFER_CHAR;
                sm >> MATRIX_COEF;
            }
        }
        sm >> HRD_PARAM_FLAG;
        if (HRD_PARAM_FLAG)
        {
            sm >> HRD_PARAM;
        }
    }

    NBits<2>  PROFILE;
    NBits<3>  LEVEL;
    NBits<2>  COLORDIFF_FORMAT;
    NBits<3>  FRMRTQ_POSTPROC;
    NBits<5>  BITRTQ_POSTPROC;
    NBits<1>  POSTPROCFLAG;
    NBits<12> MAX_CODED_WIDTH;
    NBits<12> MAX_CODED_HEIGHT;
    NBits<1>  PULLDOWN;
    NBits<1>  INTERLACE;
    NBits<1>  TFCNTRFLAG;
    NBits<1>  FINTERPFLAG;
    NBits<1>  PSF;
    NBits<1>  DISPLAY_EXT;
    NBits<14> DISP_HORIZ_SIZE;
    NBits<14> DISP_VERT_SIZE;
    NBits<1>  ASPECT_RATIO_FLAG;
    NBits<4>  ASPECT_RATIO;
    NBits<8>  ASPECT_HORIZ_SIZE;
    NBits<8>  ASPECT_VERT_SIZE;
    NBits<1>  FRAMERATE_FLAG;
    NBits<1>  FRAMERATEIND;
    NBits<8>  FRAMERATENR;
    NBits<4>  FRAMERATEDR;
    NBits<16> FRAMERATEEXP;
    NBits<1>  COLOR_FORMAT_FLAG;
    NBits<8>  COLOR_PRIM;
    NBits<8>  TRANSFER_CHAR;
    NBits<8>  MATRIX_COEF;
    NBits<1>  HRD_PARAM_FLAG;
    HrdParam  HRD_PARAM;

private:
    UINT32 m_maxCodedWidth;
    UINT32 m_maxCodedHeight;
};

template <class Derived>
class HrdFullness
{
    typedef SeqLayerParamSrc<Derived> SeqLayerParamsSrcImpl;

public:
    HrdFullness(const SeqLayerParamsSrcImpl *seqLayerParamsSrc)
      : m_seqLayerParamsSrc(seqLayerParamsSrc)
    {}

    HrdFullness(const HrdFullness &rhs)
    {
        m_seqLayerParamsSrc = rhs.m_seqLayerParamsSrc;
        copy(
            &rhs.HRD_FULL[0],
            &rhs.HRD_FULL[0] + GetHrdNumLeakyBuckets(),
            &HRD_FULL[0]
        );
    }

    template <class Stream>
    void Serialize(Stream& sm)
    {
        SplitSerialization(sm, *this);
    }

    template <class Stream>
    void Load(Stream& sm)
    {
        for (size_t n = 1; n <= GetHrdNumLeakyBuckets(); ++n)
        {
            sm >> HRD_FULL[n - 1];
        }
    }

    void SetSeqLayerParamsSrc(const SeqLayerParamsSrcImpl *v)
    {
        m_seqLayerParamsSrc = v;
    }

private:
    UINT32 GetHrdNumLeakyBuckets() const
    {
        return m_seqLayerParamsSrc->GetHrdNumLeakyBuckets();
    }

    const SeqLayerParamsSrcImpl *m_seqLayerParamsSrc;

    NBits<8> HRD_FULL[31];
};

template <class Derived>
class HrdFullnessCopyHelper
{
    typedef SeqLayerParamSrc<Derived> SeqLayerParamsSrcImpl;

public:
    explicit HrdFullnessCopyHelper(const SeqLayerParamsSrcImpl *seqLayerParamsSrc)
        : HRD_FULLNESS(seqLayerParamsSrc)
    {}

    HrdFullnessCopyHelper(const HrdFullnessCopyHelper& rhs)
      : HRD_FULLNESS(rhs.HRD_FULLNESS)
    {
        HRD_FULLNESS.SetSeqLayerParamsSrc(This());
    }

    HrdFullnessCopyHelper& operator=(const HrdFullnessCopyHelper &rhs)
    {
        if (this != &rhs)
        {
            HRD_FULLNESS = rhs.HRD_FULLNESS;
            HRD_FULLNESS.SetSeqLayerParamsSrc(This());
        }

        return *this;
    }

protected:
    HrdFullness<Derived> HRD_FULLNESS;

private:
    const Derived* This() const
    {
        return static_cast<const Derived*>(this);
    }
};

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4355) // 'this' : used in base member initializer list
#endif
class EntryPointLayer :
    public HrdFullnessCopyHelper<EntryPointLayer>
  , public SeqLayerParamSrc<EntryPointLayer>
  , public EntryPLayerParamSrc<EntryPointLayer>
{
    typedef SeqLayerParamSrc<SequenceLayer> SeqLayerParamsSrcImpl;

public:
    EntryPointLayer(const SeqLayerParamsSrcImpl *seqLayerParamsSrc)
        : HrdFullnessCopyHelper<EntryPointLayer>(this)
        , m_seqLayerParamsSrc(seqLayerParamsSrc)
    {}

    UINT32 GetMaxCodedWidth() const
    {
        return m_seqLayerParamsSrc->GetMaxCodedWidth();
    }

    UINT32 GetMaxCodedHeight() const
    {
        return m_seqLayerParamsSrc->GetMaxCodedHeight();
    }

    bool GetHrdParamFlag() const
    {
        return m_seqLayerParamsSrc->GetHrdParamFlag();
    }

    UINT32 GetHrdNumLeakyBuckets() const
    {
        return m_seqLayerParamsSrc->GetHrdNumLeakyBuckets();
    }

    bool GetPanScanPresentFlag() const
    {
        return PANSCAN_FLAG;
    }

    UINT08 GetMBQuantizationFlag() const
    {
        return static_cast<UINT08>(DQUANT);
    }

    bool GetOverlap() const
    {
        return OVERLAP;
    }

    UINT32 GetQuantizerSpecifier() const
    {
        return QUANTIZER;
    }

    UINT32 GetCodedWidth() const
    {
        return CodedWidth;
    }

    UINT32 GetCodedHeight() const
    {
        return CodedHeight;
    }

    void SetSeqLayerParamsSrc(const SeqLayerParamsSrcImpl * v)
    {
        m_seqLayerParamsSrc = v;
    }

    template <class Stream>
    void Serialize(Stream& sm)
    {
        SplitSerialization(sm, *this);
    }

    template <class Stream>
    void Load(Stream& sm)
    {
        sm >> BROKEN_LINK;
        sm >> CLOSED_ENTRY;
        sm >> PANSCAN_FLAG;
        sm >> REFDIST_FLAG;
        sm >> LOOPFILTER;
        sm >> FASTUVMC;
        sm >> EXTENDED_MV;
        sm >> DQUANT;
        sm >> VSTRANSFORM;
        sm >> OVERLAP;
        sm >> QUANTIZER;
        if (GetHrdParamFlag())
        {
            sm >> HRD_FULLNESS;
        }
        sm >> CODED_SIZE_FLAG;
        if (CODED_SIZE_FLAG)
        {
            sm >> CODED_WIDTH;
            sm >> CODED_HEIGHT;
            CodedWidth = (CODED_WIDTH + 1) * 2;
            CodedHeight = (CODED_HEIGHT + 1) *2;
        }
        else
        {
            CodedWidth = GetMaxCodedWidth();
            CodedHeight = GetMaxCodedHeight();
        }
        if (EXTENDED_MV)
        {
            sm >> EXTENDED_DMV;
        }
        sm >> RANGE_MAPY_FLAG;
        if (RANGE_MAPY_FLAG)
        {
            sm >> RANGE_MAPY;
        }
        sm >> RANGE_MAPUV_FLAG;
        if (RANGE_MAPUV_FLAG)
        {
            sm >> RANGE_MAPUV;
        }
    }

    NBits<1>  BROKEN_LINK;
    NBits<1>  CLOSED_ENTRY;
    NBits<1>  PANSCAN_FLAG;
    NBits<1>  REFDIST_FLAG;
    NBits<1>  LOOPFILTER;
    NBits<1>  FASTUVMC;
    NBits<1>  EXTENDED_MV;
    NBits<2>  DQUANT;
    NBits<1>  VSTRANSFORM;
    NBits<1>  OVERLAP;
    NBits<2>  QUANTIZER;
    NBits<1>  CODED_SIZE_FLAG;
    NBits<12> CODED_WIDTH;
    NBits<12> CODED_HEIGHT;
    NBits<1>  EXTENDED_DMV;
    NBits<1>  RANGE_MAPY_FLAG;
    NBits<3>  RANGE_MAPY;
    NBits<1>  RANGE_MAPUV_FLAG;
    NBits<3>  RANGE_MAPUV;

    UINT32 CodedWidth;
    UINT32 CodedHeight;

private:
    const SeqLayerParamsSrcImpl *m_seqLayerParamsSrc;
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

template <class EntryPLayerParam>
class VopDQuant
{
    typedef EntryPLayerParamSrc<EntryPLayerParam> EntryPLayerParamSrcImpl;

public:
    VopDQuant(const EntryPLayerParamSrcImpl *entryPLayerParamSrc)
      : m_entryPLayerParamSrc(entryPLayerParamSrc)
    {}

    UINT08 GetMBQuantizationFlag() const
    {
        return m_entryPLayerParamSrc->GetMBQuantizationFlag();
    }

    void SetEntryPLayerParamSrc(const EntryPLayerParamSrcImpl *v)
    {
        m_entryPLayerParamSrc = v;
    }

    template <class Stream>
    void Serialize(Stream& sm)
    {
        SplitSerialization(sm, *this);
    }

    template <class Stream>
    void Load(Stream& sm)
    {
        if (2 == GetMBQuantizationFlag())
        {
            DQUANT_InFrame = true;
            sm & PQDIFF;
            if (7 == PQDIFF)
            {
                sm & ABSPQ;
            }
        }
        else
        {
            sm & DQUANTFRM;
            if (DQUANTFRM)
            {
                DQUANT_InFrame = true;
                sm & DQPROFILE;
                if (DQPROFILE_SINGLE_EDGE == DQPROFILE)
                {
                    sm & DQSBEDGE;
                }
                if (DQPROFILE_DOUBLE_EDGE == DQPROFILE)
                {
                    sm & DQDBEDGE;
                }
                if (DQPROFILE_ALL_MACROBLOCKS == DQPROFILE)
                {
                    sm & DQBILEVEL;
                }
                if (!(DQPROFILE_ALL_MACROBLOCKS == DQPROFILE && DQBILEVEL))
                {
                    sm & PQDIFF;
                    if (7 == PQDIFF)
                    {
                        sm & ABSPQ;
                    }
                }
            }
            else
            {
                DQUANT_InFrame = false;
            }
        }
    }

    bool DQUANT_InFrame;
    NBits<3> PQDIFF;
    NBits<5> ABSPQ;
    NBits<1> DQUANTFRM;
    NBits<2> DQPROFILE;
    NBits<2> DQSBEDGE;
    NBits<2> DQDBEDGE;
    NBits<1> DQBILEVEL;

private:
  const EntryPLayerParamSrcImpl *m_entryPLayerParamSrc;
};

template <class EntryPLayerParam>
class VopDQuantCopyHelper
{
    typedef EntryPLayerParamSrc<EntryPLayerParam> EntryPLayerParamSrcImpl;

public:
    explicit VopDQuantCopyHelper(const EntryPLayerParamSrcImpl *entryPLayerParamSrc)
        : VOPDQUANT(entryPLayerParamSrc)
    {}

    VopDQuantCopyHelper(const VopDQuantCopyHelper& rhs)
      : VOPDQUANT(rhs.VOPDQUANT)
    {
        VOPDQUANT.SetEntryPLayerParamSrc(This());
    }

    VopDQuantCopyHelper& operator=(const VopDQuantCopyHelper &rhs)
    {
        if (this != &rhs)
        {
            VOPDQUANT = rhs.VOPDQUANT;
            VOPDQUANT.SetEntryPLayerParamSrc(This());
        }

        return *this;
    }

protected:
    VopDQuant<EntryPLayerParam> VOPDQUANT;

private:
    const EntryPLayerParam* This() const
    {
        return static_cast<const EntryPLayerParam*>(this);
    }
};

extern const UINT08 pQuantForImplicitQuantizer[32];

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4355) // 'this' : used in base member initializer list
#endif
class PictureLayer
  : public EntryPLayerParamSrc<PictureLayer>
  , public VopDQuantCopyHelper<PictureLayer>
{
    typedef SeqLayerParamSrc<SequenceLayer> SeqLayerParamsSrcImpl;
    typedef EntryPLayerParamSrc<EntryPointLayer> EntryPLayerParamSrcImpl;

    static const size_t panScanWindowsMaxSize = 4;

public:
    PictureLayer(
        const SeqLayerParamsSrcImpl   *seqLayerParamsSrc,
        const EntryPLayerParamSrcImpl *entryPLayerParamSrc
    )
      : VopDQuantCopyHelper<PictureLayer>(this)
      , m_seqLayerParamsSrc(seqLayerParamsSrc)
      , m_entryPLayerParamSrc(entryPLayerParamSrc)
    {}

    bool GetPostprocessingFlag() const
    {
        return m_seqLayerParamsSrc->GetPostprocessingFlag();
    }

    bool GetPullDownFlag() const
    {
        return m_seqLayerParamsSrc->GetPullDownFlag();
    }

    bool GetInterlace() const
    {
        return m_seqLayerParamsSrc->GetInterlace();
    }

    bool GetFrameCounterFlag() const
    {
        return m_seqLayerParamsSrc->GetFrameCounterFlag();
    }

    bool GetFrameInterpolationFlag() const
    {
        return m_seqLayerParamsSrc->GetFrameInterpolationFlag();
    }

    bool GetProgressiveSegmentedFrame() const
    {
        return m_seqLayerParamsSrc->GetProgressiveSegmentedFrame();
    }

    bool GetPanScanPresentFlag() const
    {
        return m_entryPLayerParamSrc->GetPanScanPresentFlag();
    }

    UINT08 GetMBQuantizationFlag() const
    {
        return m_entryPLayerParamSrc->GetMBQuantizationFlag();
    }

    bool GetOverlap() const
    {
        return m_entryPLayerParamSrc->GetOverlap();
    }

    UINT32 GetQuantizerSpecifier() const
    {
        return m_entryPLayerParamSrc->GetQuantizerSpecifier();
    }

    UINT32 GetCodedWidth() const
    {
        return m_entryPLayerParamSrc->GetCodedWidth();
    }

    UINT32 GetCodedHeight() const
    {
        return m_entryPLayerParamSrc->GetCodedHeight();
    }

    template <class Stream>
    void Serialize(Stream& sm)
    {
        SplitSerialization(sm, *this);
    }

    template <class Stream>
    void Load(Stream& sm)
    {
        m_skipped = false;
        if (GetInterlace())
        {
            sm >> FCM;
        }
        UINT32 codedWidth = GetCodedWidth();
        UINT32 codedHeight  = GetCodedHeight();
        if (GetInterlace())
        {
            codedHeight /= 2;
        }
        m_widthInMB = (codedWidth + 15) / 16;
        m_heightInMB = (codedHeight + 15) / 16;
        m_frameSizeInMB = m_widthInMB * m_heightInMB;

        if (8 >= sm.BitsLeft())
        {
            m_skipped = true;
            return;
        }

        sm >> PTYPE;
        if (GetFrameCounterFlag())
        {
            sm >> TFCNTR;
        }
        if (GetPullDownFlag())
        {
            if (!GetInterlace() || GetProgressiveSegmentedFrame())
            {
                sm >> RPTFRM;
            }
            else
            {
                sm >> TFF;
                sm >> RFF;
            }
        }
        if (GetPanScanPresentFlag())
        {
            sm >> PS_PRESENT;
            if (PS_PRESENT)
            {
                if (GetInterlace() && !GetProgressiveSegmentedFrame())
                {
                    if (GetPullDownFlag())
                        NumberOfPanScanWindows = 2 + (RFF ? 1 : 0);
                    else
                        NumberOfPanScanWindows = 2;
                }
                else
                {
                    if (GetPullDownFlag())
                        NumberOfPanScanWindows = 1 + RPTFRM;
                    else
                        NumberOfPanScanWindows = 1;
                }
                for (size_t i = 0; NumberOfPanScanWindows > i; ++i)
                {
                    sm >> PS_HOFFSET[i];
                    sm >> PS_VOFFSET[i];
                    sm >> PS_WIDTH[i];
                    sm >> PS_HEIGHT[i];
                }
            }
        }
        sm >> RNDCTRL;
        if (GetInterlace())
        {
            sm >> UVSAMP;
        }
        if (GetFrameInterpolationFlag())
        {
            sm >> INTERPFRM;
        }
        sm >> PQINDEX;
        if (QUANT_FRAME_IMPLICIT == GetQuantizerSpecifier())
        {
            PQUANT = pQuantForImplicitQuantizer[PQINDEX];
        }
        else
        {
            PQUANT = PQINDEX;
        }

        if (8 >= PQINDEX)
        {
            sm >> HALFQP;
        }
        if (QUANT_FRAME_EXPLICIT == GetQuantizerSpecifier())
        {
            sm >> PQUANTIZER;
        }
        if (GetPostprocessingFlag())
        {
            sm >> POSTPROC;
        }
        ACPRED.SetDim(m_widthInMB, m_heightInMB);
        sm >> ACPRED;
        CONDOVER = OVERLAP_SMOOTHING_NONE;
        if (GetOverlap() && 8 >= PQUANT)
        {
            sm >> CONDOVER;
            if (OVERLAP_SMOOTHING_SELECT == CONDOVER)
            {
                OVERFLAGS.SetDim(m_widthInMB, m_heightInMB);
                sm >> OVERFLAGS;
            }
        }
        sm >> TRANSACFRM;
        sm >> TRANSACFRM2;
        sm >> TRANSDCTAB;
        if (0 != GetMBQuantizationFlag())
        {
            sm >> VOPDQUANT;
        }
    }

    NBits<1>      INTERPFRM;
    NBits<2>      FRMCNT;
    NBits<1>      RANGEREDFRM;
    Unary<2>      FCM;
    Unary<4>      PTYPE;
    NBits<8>      TFCNTR;
    NBits<2>      RPTFRM;
    NBits<1>      TFF;
    NBits<1>      RFF;
    BFractiolwLC  BFRACTION;
    NBits<7>      BF;
    NBits<1>      PS_PRESENT;
    NBits<18>     PS_HOFFSET[panScanWindowsMaxSize];
    NBits<18>     PS_VOFFSET[panScanWindowsMaxSize];
    NBits<14>     PS_WIDTH[panScanWindowsMaxSize];
    NBits<14>     PS_HEIGHT[panScanWindowsMaxSize];
    NBits<1>      RNDCTRL;
    NBits<1>      UVSAMP;
    NBits<5>      PQINDEX;
    UINT08        PQUANT;
    NBits<1>      HALFQP;
    NBits<1>      PQUANTIZER;
    NBits<2>      POSTPROC;
    BitPlane      ACPRED;
    CondoverVLC   CONDOVER;
    BitPlane      OVERFLAGS;
    Unary<3>      MVRANGE;
    NBits<2>      RESPIC;
    TransacfrmVlc TRANSACFRM;
    TransacfrmVlc TRANSACFRM2;
    NBits<1>      TRANSDCTAB;

    size_t        NumberOfPanScanWindows;

private:
    const SeqLayerParamsSrcImpl   *m_seqLayerParamsSrc;
    const EntryPLayerParamSrcImpl *m_entryPLayerParamSrc;

    UINT32 m_widthInMB;
    UINT32 m_heightInMB;
    UINT32 m_frameSizeInMB;
    bool   m_skipped;
    //bool   m_bitPlaneCodingUsed;
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace VC1

#endif // VC1SYNTAX_H
