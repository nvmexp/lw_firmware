/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2015 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef OUTBITSTREAM_H
#define OUTBITSTREAM_H

#include <cstddef>

#include "core/include/types.h"

namespace BitsIO
{

//! A stream of bits that can be written in any amount up to 64 regardless of
//! byte boundaries. Most significant bits in a byte go first.
template <class OutputIterator>
class OutBitStream
{
public:
    explicit OutBitStream(OutputIterator it)
      : m_it(it)
      , m_numBits(0)
    {}

    //! Writes `numBits` rightmost bits from `what` to the stream.
    void Write(UINT64 what, size_t numBits)
    {
        if (0 == numBits) return;

        what <<= (64 - numBits);
        for (;;)
        {
            UINT08 lwrBit = m_numBits % 8; // the first free bit in a byte starting
                                           // from the most significant
            UINT08 bitsLeftInByte = 8 - lwrBit;

            if (0 != m_numBits && 0 == lwrBit)
            {
                // the last byte is fully oclwpied
                ++m_it;
            }
            int bitsWillLeftInByte = static_cast<int>(bitsLeftInByte - numBits);
            static const UINT08 mask[9] =
            {
                0, 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80, 0
            };
            *m_it &= mask[bitsLeftInByte];
            *m_it |= what >> (56 + lwrBit);
            if (0 == (bitsWillLeftInByte - 7) / 8) // negated how many byte boundaries we are crossing
            {
                m_numBits += numBits;
                return;
            }
            else
            {
                m_numBits += bitsLeftInByte;
                numBits -= bitsLeftInByte;
                what <<= bitsLeftInByte;
            }
        }
    }

    //! Write zero bits until the end of the current byte
    void FillByteAlign()
    {
        UINT08 lwrBit = m_numBits % 8;
        if (0 == lwrBit)
        {
            return;
        }

        UINT08 bitsLeftInByte = 8 - lwrBit;
        Write(0, bitsLeftInByte);
    }

    size_t GetNumBits() const
    {
        return m_numBits;
    }

private:
    OutputIterator m_it;
    size_t         m_numBits;
};

} // namespace BitsIO
#endif
