/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2015, 2017-2018 by LWPU Corporation. All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include <cstddef>

#include <type_traits>

#include "core/include/types.h"

namespace BitsIO
{

//! Little-endian policy class that defines how to glue and carve bits while
//! reading words from a stream.
struct LittleEndian
{
    // Appends a new read word to a previous value. In little-endian it goes in
    // front.
    template <typename Res, typename Word>
    static
    std::enable_if_t<std::is_unsigned<Res>::value, Res>
    Append(int where, Res prev, Word v)
    {
        return prev | (static_cast<Res>(v) << where * sizeof(Word) * 8);
    }

    template <typename Res>
    static
    std::enable_if_t<std::is_unsigned<Res>::value, Res>
    Append(int where, Res prev, UINT64 v)
    {
        return prev;
    }

    // Lwts out the result we need from several read words.
    template <int BitsInWord, typename Res, typename StartType, typename FinishType>
    static
    std::enable_if_t<std::is_unsigned<Res>::value, Res>
    Carve(Res v, StartType start, FinishType finish, int wordBoundaries)
    {
        const Res mask = Res(-1) >> (sizeof(v) * 8 - finish);
        return (v & mask) >> start;
    }

    // Concatenates two values.
    template <typename Res, typename BitsInFirstType, typename BitsInSecType>
    static
    std::enable_if_t<std::is_unsigned<Res>::value, Res>
    Cat(Res first, Res second, BitsInFirstType bitsInFirst, BitsInSecType bitsInSecond)
    {
        return (static_cast<Res>(second) << bitsInFirst) | first;
    }
};

//! Big-endian policy class that defines how to glue and carve bits while
//! reading words from a stream.
struct BigEndian
{
    // Appends a new read word to a previous value. In big-endian it goes to
    // back.
    template <typename Res, typename Word>
    static
    std::enable_if_t<std::is_unsigned<Res>::value, Res>
    Append(int where, Res prev, Word v)
    {
        return (prev << sizeof(Word) * 8) + v;
    }

    template <typename Res>
    static
    std::enable_if_t<std::is_unsigned<Res>::value, Res>
    Append(int where, Res prev, UINT64 v)
    {
        return v;
    }

    // Lwts out the result we need from several read words.
    template <int BitsInWord, typename Res, typename StartType, typename FinishType>
    static
    std::enable_if_t<std::is_unsigned<Res>::value, Res>
    Carve(Res v, StartType start, FinishType finish, int wordBoundaries)
    {
        // In big-endian `start` points to a bit in the first read word that is
        // `start` bits down from the MSB. For example, if the word size is 1,
        // the start bit is 2 and we had to read 10 bits, then the real location
        // of this bit inside `v` is 13.
        // ...|15|14|13|12|11|10|09|08||07|06|05|04|03|02|01|00|
        //           ^- start                    ^- finish
        // `realStart` below is the distance of this bit from the MSB,
        // i.e. 63 - 13 = 50, if `Res` type is 64 bits.
        const auto realStart = sizeof(Res) * 8 - (wordBoundaries + 1) * BitsInWord + start;
        const Res mask = Res(-1) >> realStart;
        return (v & mask) >> (sizeof(Res) * 8 - realStart - finish + start);
    }

    // Concatenates two values.
    template <typename Res, typename BitsInFirstType, typename BitsInSecType>
    static
    std::enable_if_t<std::is_unsigned<Res>::value, Res>
    Cat(Res first, Res second, BitsInFirstType bitsInFirst, BitsInSecType bitsInSecond)
    {
        return (static_cast<Res>(first) << bitsInSecond) | second;
    }
};

//! A stream of bits that can be retrieved in any amount regardless of words
//! boundaries.
template <typename InputIterator, typename EndianPolicy>
class GenericInBitStream
{
    static constexpr int BytesInWord =
        sizeof(typename std::iterator_traits<InputIterator>::value_type);
    static constexpr int BitsInWord = 8 * BytesInWord;

    template <int MaxBits>
    struct BigEnoughType
    {
        typedef std::conditional_t<
            MaxBits <= 8
          , UINT08
          , std::conditional_t<
                MaxBits <= 16
              , UINT16
              , std::conditional_t<
                    MaxBits <= 32
                  , UINT32
                  , UINT64
                  >
              >
          > Type;
    };
    template <int MaxBits>
    using BigEnoughTypeT = typename BigEnoughType<(MaxBits > BitsInWord ? MaxBits : BitsInWord)>::Type;

public:
    GenericInBitStream(InputIterator start, InputIterator finish)
      : m_it(start)
      , m_end(finish)
    {
        // Order is important, don't put it into the initializer list. You can
        // make it right with the proper declaration order, but it will work
        // only until Vasya Pupkin messes it up.
        m_bitsLeft = (m_end - m_it) * BitsInWord;
        m_startBitsAmount = m_bitsLeft;
    }

    //! Reads `n` bits from the stream. `MaxBits` is a small optimization: it
    //! chooses an optimal size for the result and performs all bit operations
    //! within that type.
    template <unsigned int MaxBits>
    BigEnoughTypeT<MaxBits> ReadWithLimit(size_t n)
    {
        if (0 == n || 0 == m_bitsLeft)
        {
            return 0;
        }

        if (n > m_bitsLeft) n = static_cast<size_t>(m_bitsLeft);

        BigEnoughTypeT<MaxBits> res;

        // it's either the number of bits that will remain in the current word
        // after reading, or, if negative, how many bits we overshoot past the
        // current word
        int bitsWillLeftInWord = static_cast<int>(BitsInWord - m_lwrBit - n);
        
        // the number of word boundaries the value we are going to read spans across
        int wordBoundaries = -(bitsWillLeftInWord - (BitsInWord - 1)) / BitsInWord;
        
        // check that the raw number of words that contain our value fits into the result
        if (wordBoundaries < static_cast<int>(sizeof(res) / BytesInWord))
        {
            // if it does, just read `wordBoundaries + 1` words and then cut out what we need
            res = *m_it;
            for (int i = 1; wordBoundaries >= i; ++i)
            {
                res = EndianPolicy::Append(i, res, *++m_it);
            }
            res = EndianPolicy::template Carve<BitsInWord>(res, m_lwrBit, m_lwrBit + n, wordBoundaries);
        }
        else
        {
            // if it doesn't, perform two reads and concatenate the results
            auto bitsInFirst = BitsInWord - m_lwrBit;
            auto bitsInSecond = n - bitsInFirst;
            auto firstRead = ReadWithLimit<MaxBits>(bitsInFirst);
            return EndianPolicy::Cat
            (
                firstRead,
                ReadWithLimit<MaxBits>(bitsInSecond),
                bitsInFirst,
                bitsInSecond
            );
        }

        m_lwrBit = (m_lwrBit + n) % BitsInWord;
        if (0 == m_lwrBit)
        {
            ++m_it;
        }
        m_bitsLeft -= n;

        return res;
    }

    template <unsigned int N>
    BigEnoughTypeT<N> ReadStatic()
    {
        return ReadWithLimit<N>(N);
    }

    UINT64 Read(size_t n)
    {
        return ReadWithLimit<64>(n);
    }

    INT64 ReadSigned(size_t n)
    {
        INT64 res = static_cast<INT64>(Read(n));
        auto const mask = 1ULL << (n - 1);
        return (res ^ mask) - mask;
    }

    //! Consumes `n` bits from the stream.
    void Skip(size_t n)
    {
        if (0 == n || 0 == m_bitsLeft)
        {
            return;
        }

        n = static_cast<size_t>(n > m_bitsLeft ? m_bitsLeft : n);

        // `BitsInWord - m_lwrBit - n` is a negated amount of bits our `n` bits
        // overshoot the current word. Increment our word iterator until it is
        // positive.
        for (int bwlib = static_cast<int>(BitsInWord - m_lwrBit - n); 0 > bwlib; ++m_it, bwlib += BitsInWord);
        
        // Now the current bit can be adjusted within one word.
        m_lwrBit = (m_lwrBit + n) % BitsInWord;
        if (0 == m_lwrBit)
        {
            ++m_it;
        }

        m_bitsLeft -= n;
    }

    //! Skip bits from the stream until the current position corresponds to `n`
    //! word boundary.
    void AlignTo(size_t n)
    {
        size_t bitsAlignment = n * BitsInWord;
        size_t needMoveTo =
            ((GetLwrrentOffset() + bitsAlignment - 1) / bitsAlignment) * bitsAlignment;
        Skip(needMoveTo - GetLwrrentOffset());
    }

    UINT64 BitsLeft() const
    {
        return m_bitsLeft;
    }

    UINT64 GetLwrrentOffset() const
    {
        return m_startBitsAmount - m_bitsLeft;
    }

protected:
    typename std::iterator_traits<InputIterator>::value_type
    GetLwrrentWord() const
    {
        return *m_it;
    }

private:
    InputIterator m_it;
    InputIterator m_end;
    UINT08        m_lwrBit = 0;
    UINT64        m_startBitsAmount;
    UINT64        m_bitsLeft;
};

template <typename InputIterator>
using InBitStream = GenericInBitStream<InputIterator, BigEndian>;

} // namespace BitsIO
