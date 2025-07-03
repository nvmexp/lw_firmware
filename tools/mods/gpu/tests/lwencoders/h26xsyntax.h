/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2015, 2018 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include <cmath>
#include <cstdio>
#include <iterator>
#include <vector>
#include <type_traits>

#include "core/include/massert.h"
#include "core/include/types.h"

#include "nbits.h"
#include "inarchive.h"
#include "outarchive.h"
#include "inbitstream.h"
#include "outbitstream.h"

namespace H26X
{
using Vid::NBits;

template <class InputIterator>
class InBitStream : public BitsIO::InBitStream<InputIterator>
{
public:
    InBitStream(InputIterator start, InputIterator finish)
      : BitsIO::InBitStream<InputIterator>(start, finish)
    {}

    bool MoreRbspData() const
    {
        static const UINT08 trailingBits[9] =
        { 0, 0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80 };
        UINT64 bits = this->BitsLeft();
        if (bits == 0)
            return false;
        if (bits <= 8)
        {
            UINT08 trailCheck = this->GetLwrrentWord();
            if (trailCheck == trailingBits[bits])
                return false;
        }

        return true;
    }
};

//! Represents a unsigned integer that is serialized to or from a bit stream
//! using Exponential-Golomb coding. Corresponds to ue(v) of ITU-T H.264. See
//! section 7.2 of ITU-T H.264.
class Ue
{
public:
    typedef UINT32 ValueType;

private:
    ValueType m_v;

public:
    Ue() : m_v(0)
    {}

    Ue(ValueType v) : m_v(v)
    {}

    operator ValueType() const
    {
        return m_v;
    }

    Ue& operator =(ValueType rhs)
    {
        m_v = rhs;

        return *this;
    }

    Ue& operator++()
    {
        ++m_v;
        return *this;
    }

    Ue& operator--()
    {
        --m_v;
        return *this;
    }

    ValueType operator++(int)
    {
        ValueType save = m_v;
        ++m_v;
        return save;
    }

    ValueType operator--(int)
    {
        ValueType save = m_v;
        --m_v;
        return save;
    }

    Ue& operator >>=(ValueType rhs)
    {
        m_v >>= rhs;

        return *this;
    }
};

//! Represents a signed integer that serialized to or from a bit stream using
//! Exponential-Golomb coding. Corresponds to se(v) of ITU-T H.264. See section
//! 7.2 of ITU-T H.264.
class Se
{
public:
    typedef INT32 ValueType;

private:
    ValueType m_v;

public:
    Se() : m_v(0)
    {}

    Se(ValueType v) : m_v(v)
    {}

    operator ValueType() const
    {
        return m_v;
    }

    Se& operator =(ValueType rhs)
    {
        m_v = rhs;

        return *this;
    }
};

template <class NumType, class Enable = void>
class Num
{};

template <class NumType>
class Num<
    NumType
  , enable_if_t<is_integral<NumType>::value>
  >
{
public:
    Num()
      : m_v(0)
      , m_numBits(0)
    {}

    Num(NumType v)
      : m_v(v)
      , m_numBits(0)
    {}

    operator NumType() const
    {
        return m_v;
    }

    Num& operator=(NumType rhs)
    {
        m_v = rhs;
        return *this;
    }

    Num& operator()(unsigned int numBits)
    {
        m_numBits = numBits;
        return *this;
    }

    const Num& operator()(unsigned int numBits) const
    {
        m_numBits = numBits;
        return *this;
    }

    void SetNumBits(unsigned int v)
    {
        m_numBits = v;
    }

    unsigned int GetNumBits() const
    {
        return m_numBits;
    }

private:
    NumType      m_v;
    unsigned int m_numBits;
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

    bool MoreRbspData() const
    {
        return m_inBitStream.MoreRbspData();
    }

    UINT64 GetLwrrentOffset() const
    {
        return m_inBitStream.GetLwrrentOffset();
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

    void Load(Ue &ue)
    {
        UINT32 zeroBits;

        for (zeroBits = 0; 0 == m_inBitStream.Read(1) && 0 < m_inBitStream.BitsLeft(); ++zeroBits);
        if (0 == zeroBits)
        {
            ue = 0;
        }
        else
        {
            ue = (1 << zeroBits) + static_cast<UINT32>(m_inBitStream.Read(zeroBits)) - 1;
        }
    }

    void Load(Se &se)
    {
        Ue ret;
        Load(ret);

        if (0 == (ret & 0x1))
        {
            ret >>= 1;
            se = 0 - ret;
        }
        else
        {
            se = (ret + 1) >> 1;
        }
    }

    template <class NumType>
    void Load(Num<NumType> &n)
    {
        n = static_cast<NumType>(m_inBitStream.Read(n.GetNumBits()));
    }

private:
    InBitStream<InputIterator> m_inBitStream;
};
} // namespace H26X

namespace BitsIO
{
template <class NumType>
struct SerializationTraits<H26X::Num<NumType> >
{
    static constexpr bool isPrimitive = true;
};

template <>
struct SerializationTraits<H26X::Ue>
{
    static constexpr bool isPrimitive = true;
};

template <>
struct SerializationTraits<H26X::Se>
{
    static constexpr bool isPrimitive = true;
};
} // namespace BitsIO

namespace H26X
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

    bool MoreRbspData() const
    {
        return PrimitiveTypesOnIBitStream<InputIterator>::MoreRbspData();
    }

    UINT64 GetLwrrentOffset() const
    {
        return PrimitiveTypesOnIBitStream<InputIterator>::GetLwrrentOffset();
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

template <class OutputIterator>
class PrimitiveTypesOnOBitStream
{
#if defined(__GNUC__) && __GNUC__ < 3
public:
#else
    friend class BitsIO::SaveAccess;
protected:
#endif
    PrimitiveTypesOnOBitStream(OutputIterator it)
      : m_outBitStream(it)
    {}

    void FillByteAlign()
    {
        m_outBitStream.FillByteAlign();
    }

    template <unsigned int N>
    void Save(const NBits<N> &u)
    {
        m_outBitStream.Write(u, N);
    }

    void Save(const NBits<1> &u)
    {
        u ? m_outBitStream.Write(1, 1) : m_outBitStream.Write(0, 1);
    }

    void Save(const Ue &ue)
    {
        static const UINT08 mapToNumBits[32] =
        {
            22, 28, 13, 26, 17, 19, 9, 21, 12, 16, 8, 11, 7, 6, 5, 32,
            1, 23, 2, 29, 24, 14, 3, 30, 27, 25, 18, 20, 15, 10, 4, 31
        };

        UINT32 v = ue + 1;

        // callwlate the number of significant bits in ue + 1:

        // first round up to the next 'power of 2 - 1'
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;

        // Use a multiplicative hash function to map the pattern to 0..31, then
        // use this number to find the number of bits by means of a lookup
        // table. The multiplicative constant was found empirically. This number
        // is a de Bruijn sequence for k = 2 and n = 5. Among all suitable de
        // Bruijn sequences the one that groups small indices closer together
        // was chosen.
        UINT32 numBits = mapToNumBits[static_cast<UINT32>(v * 0x87dcd629U) >> 27];

        // by the definition of the Exponential-Golomb coding
        m_outBitStream.Write(0, numBits - 1);
        m_outBitStream.Write(ue + 1, numBits);
    }

    void Save(const Se &se)
    {
        Ue v = 2 * abs(se);
        if (se < 0) --v;
        Save(v);
    }

    template <class NumType>
    void Save(const Num<NumType> &n)
    {
        m_outBitStream.Write(n, n.GetNumBits());
    }

private:
    BitsIO::OutBitStream<OutputIterator> m_outBitStream;
};

template <class Archive, class OutputIterator>
class BitOArchiveImpl :
    public PrimitiveTypesOnOBitStream<OutputIterator>
  , public BitsIO::CommonOArchive<Archive>
{
public:
    BitOArchiveImpl(OutputIterator it)
      : PrimitiveTypesOnOBitStream<OutputIterator>(it)
    {}

    void FillByteAlign()
    {
        PrimitiveTypesOnOBitStream<OutputIterator>::FillByteAlign();
    }
};

template <class OutputIterator>
class BitOArchive :
    public BitOArchiveImpl<BitOArchive<OutputIterator>, OutputIterator>
{
public:
    BitOArchive(OutputIterator it)
      : BitOArchiveImpl<BitOArchive<OutputIterator>, OutputIterator>(it)
    {}
};

template <class InputIterator>
class IteratorWithCounter : public std::input_iterator_tag
{
public:
    typedef IteratorWithCounter<InputIterator> ThisClass;

    typedef typename iterator_traits<InputIterator>::value_type value_type;
    typedef typename iterator_traits<InputIterator>::reference reference;

    IteratorWithCounter(InputIterator it)
      : m_it(it)
      , m_count(0)
    {}

    ThisClass& operator++()
    {
        ++m_count;
    }

    ThisClass operator++(int)
    {
        ThisClass tmp = *this;
        ++m_it;
        ++m_count;

        return tmp;
    }

    reference operator *()
    {
        return *m_it;
    }

    bool operator ==(const InputIterator& rhs) const
    {
        return m_it == rhs;
    }

    bool operator !=(const InputIterator& rhs) const
    {
        return m_it != rhs;
    }

    size_t GetCount() const
    {
        return m_count;
    }

private:
    InputIterator m_it;
    size_t        m_count;
};

template <class InputIterator>
class PrimitiveTypesOnINALStream
{
#if defined(__GNUC__) && __GNUC__ < 3
public:
#else
    friend class BitsIO::LoadAccess;
protected:
#endif
    PrimitiveTypesOnINALStream(InputIterator start, InputIterator finish)
      : m_it(start)
      , m_end(finish)
    {}

    template <class NALU>
    void Load(NALU &nalu)
    {
        UINT32 startCode = 0;
        vector<UINT08> ebspBuf;

        if (0 == m_it.GetCount())
        {
            // According to the Annex B of ITU-T H.264 and H.265 the very first
            // NALU can have leading zeros before the start code. Therefore we
            // cannot just skip a start code, we have to search for it.

            startCode = 0xFFFFFFFF;
            while (m_it != m_end)
            {
                UINT08 lwrByte = *m_it++;

                startCode <<= 8;
                startCode |= lwrByte;
                if (1 == (0x00FFFFFF & startCode))
                {
                    break;
                }
                else
                {
                    MASSERT(0 == lwrByte);
                }
            }
        }

        size_t naluOffset = m_it.GetCount();

        // read until the next start code
        if (m_it != m_end)
        {
            startCode = 0xFFFFFFFF;
            while (m_it != m_end && (0x00FFFFFF & startCode) != 1)
            {
                UINT08 lwrByte = *m_it++;
                startCode <<= 8;
                startCode |= lwrByte;
                ebspBuf.push_back(lwrByte);
            }
        }
        vector<UINT08>::iterator finish = ebspBuf.end();
        if (m_it != m_end)
        {
            // we met the next start code
            if (0 != (0xFF000000 & startCode))
            {
                // three bytes start code
                finish -= 3;
            }
            else
            {
                // four bytes start code
                finish -= 4;
            }
        }
        nalu.InitFromEBSP(ebspBuf.begin(), finish, naluOffset);
    }

    bool IsEOS() const
    {
        return m_it == m_end;
    }

private:
    IteratorWithCounter<InputIterator> m_it;
    InputIterator                      m_end;
};

template <class Archive, class InputIterator>
class NALIArchiveImpl :
    public PrimitiveTypesOnINALStream<InputIterator>
  , public BitsIO::CommonIArchive<Archive>
{
public:
    NALIArchiveImpl(InputIterator start, InputIterator finish)
      : PrimitiveTypesOnINALStream<InputIterator>(start, finish)
    {}

    bool IsEOS() const
    {
        return PrimitiveTypesOnINALStream<InputIterator>::IsEOS();
    }
};

template <class InputIterator>
class NALIArchive :
    public NALIArchiveImpl<NALIArchive<InputIterator>, InputIterator>
{
public:
    NALIArchive(InputIterator start, InputIterator finish)
      : NALIArchiveImpl<NALIArchive<InputIterator>, InputIterator>(start, finish)
    {}
};

#if defined(SUPPORT_TEXT_OUTPUT)
class PrimitiveTypesOnOTextFile
{
public:
    FILE* GetOFile()
    {
        return m_out;
    }

#if defined(__GNUC__) && __GNUC__ < 3
public:
#else
    friend class BitsIO::SaveAccess;
protected:
#endif
    PrimitiveTypesOnOTextFile(FILE *outFile)
      : m_out(outFile)
    {}

    template <unsigned int N>
    void Save(const NBits<N> &u)
    {
        if (sizeof(unsigned int) * 8u >= N)
        {
            fprintf(m_out, "%u", static_cast<unsigned int>(u));
        }
        else
        {
            fprintf(m_out, "%llu", static_cast<unsigned long long int>(u));
        }
    }

    void Save(const NBits<1> &u)
    {
        fprintf(m_out, "%u", u ? 1 : 0);
    }

    void Save(const Ue &ue)
    {
        fprintf(m_out, "%u", static_cast<unsigned int>(ue));
    }

    void Save(const Se &se)
    {
        fprintf(m_out, "%d", static_cast<unsigned int>(se));
    }

    template <class NumType>
    void Save(const Num<NumType> &n)
    {
        if (sizeof(unsigned int) >= sizeof(NumType))
        {
            fprintf(m_out, "%u", static_cast<unsigned int>(n));
        }
        else
        {
            fprintf(m_out, "%llu", static_cast<unsigned long long int>(n));
        }
    }

    void Save(UINT08 n)
    {
        fprintf(m_out, "%u", static_cast<unsigned int>(n));
    }

    void Save(INT32 n)
    {
        fprintf(m_out, "%d", n);
    }

    void Save(UINT32 n)
    {
        fprintf(m_out, "%u", n);
    }

    void Save(UINT64 n)
    {
        fprintf(m_out, "%llu", static_cast<unsigned long long int>(n));
    }

    void Save(bool b)
    {
        fprintf(m_out, "%s", b ? "true" : "false");
    }

private:
    FILE *m_out;
};

template <class Archive>
class BasicTextOArchive :
    public BitsIO::CommonOArchive<Archive>
{
    friend class BitsIO::IOutArchive<Archive>;
    typedef BitsIO::CommonOArchive<Archive> Base;

public:
    size_t GetIndent() const
    {
        return indentInc * m_indentLevel;
    }

protected:
    BasicTextOArchive(size_t initIndentLevel = 0)
      : m_indentLevel(initIndentLevel)
    {}

    template <class T>
    void SaveOverride(const BitsIO::Lwp<T> &t)
    {
        using Utility::remove_cv;
        bool isPrimitive = BitsIO::SerializationTraits<typename remove_cv<T>::type>::isPrimitive;
        string indent(indentInc * m_indentLevel, ' ');
        FILE *of = this->This()->GetOFile();

        fprintf(of, "%s%s", indent.c_str(), t.Name());
        if (t.HasIdx1())
        {
            fprintf(of, "[%d]", t.GetIdx1());
        }
        if (t.HasIdx2())
        {
            fprintf(of, "[%d]", t.GetIdx2());
        }
        fprintf(of, " = ");
        if (!isPrimitive)
        {
            fprintf(of, "{\n");
            ++m_indentLevel;
        }
        this->Base::SaveOverride(t.ConstValue());
        if (isPrimitive)
        {
            fprintf(of, "\n");
        }
        else
        {
            fprintf(of, "%s}\n", indent.c_str());
            --m_indentLevel;
        }
    }

private:
    size_t              m_indentLevel;
    static const size_t indentInc = 2;
};

template <class Archive>
class TextOArchiveImpl :
    public PrimitiveTypesOnOTextFile
  , public BasicTextOArchive<Archive>
{
public:
    TextOArchiveImpl(FILE* outFile, size_t initIndentLevel = 0)
      : PrimitiveTypesOnOTextFile(outFile)
      , BasicTextOArchive<Archive>(initIndentLevel)
    {}
};

class TextOArchive :
    public TextOArchiveImpl<TextOArchive>
{
public:
    TextOArchive(FILE* outFile, size_t initIndentLevel = 0)
      : TextOArchiveImpl<TextOArchive>(outFile, initIndentLevel)
    {}
};
#endif

// Assignment abstraction for RBSP2EBSPColwerter
struct Assigner
{
    template <class T, class U>
    static void Assign(T &t, U u)
    {
        *t++ = u;
    }
};

struct AssignerStub
{
    template <class T, class U>
    static void Assign(T &t, U u)
    {
    }
};

template <class T>
struct RBSP2EBSPColwerter
{
    // Add emulation_prevention_three_byte, i.e. make the following colwersions
    // for all oclwrrences:
    // 0x000000  -> 0x00000300
    // 0x000001  -> 0x00000301
    // 0x000002  -> 0x00000302
    // 0x000003  -> 0x00000303
    // We need an assignment abstraction to be able to reuse the same code to
    // callwlate the size of the resulting EBSP.
    template <class InputIterator, class OutputIterator>
    static size_t Colwert(
        InputIterator rbspStart,
        InputIterator rbspFinish,
        OutputIterator ebspIt
    )
    {
        size_t zeroes = 0;
        size_t ebspIdx = 0;
        for (InputIterator it = rbspStart; rbspFinish != it; ++it)
        {
            if (2 == zeroes && 0 == (*it & 0xFC))
            {
                T::Assign(ebspIt, 3);
                ++ebspIdx;
                zeroes = 0;
            }
            T::Assign(ebspIt, *it);
            ++ebspIdx;
            if (0 == *it)
                ++zeroes;
            else
                zeroes = 0;
        }

        return ebspIdx;
    }
};

template <class InputIterator>
size_t CalcEBSPSize(InputIterator rbspStart, InputIterator rbspFinish)
{
    return RBSP2EBSPColwerter<AssignerStub>::Colwert(rbspStart, rbspFinish, 0);
}

template <class InputIterator, class OutputIterator>
void ColwertRBSP2EBSP(
    InputIterator rbspStart,
    InputIterator rbspFinish,
    OutputIterator ebspOut)
{
    RBSP2EBSPColwerter<Assigner>::Colwert(rbspStart, rbspFinish, ebspOut);
}

} // namespace H26X
