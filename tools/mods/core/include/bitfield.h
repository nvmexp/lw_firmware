/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#ifndef INCLUDED_BITFIELD_H
#define INCLUDED_BITFIELD_H

#include "core/include/massert.h"
#include "core/include/types.h"
#include "core/include/utility.h"

#include <boost/integer/static_log2.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>

//!
//! \brief Representation of an aribitrary length bitfield.
//!
//! Use 'ElementT' to select the element type of the underlying storage, and
//! 'BitCount' to set the length of the bitfield. Note that the bitfield only
//! works correctly when using unsigned storage types.
//!
template <
    class ElementT
    ,UINT32 BitCount
    ,class = enable_if_t<is_integral<ElementT>::value && is_unsigned<ElementT>::value>
    >
class Bitfield;

template <class ElementT, UINT32 BitCount>
class Bitfield<ElementT, BitCount>
{
    static_assert(BitCount > 0, "Bitfield size must be greater than 0");

    static constexpr UINT32 s_BitCount = BitCount;
    static constexpr UINT32 s_ElementBitCount = sizeof(ElementT) * CHAR_BIT;
    static constexpr UINT32 s_ElementCount =
        Utility::AlignUp<s_ElementBitCount>(s_BitCount) / s_ElementBitCount;
    //! Number of bits required to represent the number of bits in an element.
    static constexpr UINT32 s_NumBitsToRepElCount = boost::static_log2<s_ElementBitCount>::value;

public:
    using iterType         = typename array<ElementT, s_ElementCount>::iterator;
    using constIterType    = typename array<ElementT, s_ElementCount>::const_iterator;
    using revIterType      = typename array<ElementT, s_ElementCount>::reverse_iterator;
    using constRevIterType = typename array<ElementT, s_ElementCount>::const_reverse_iterator;

    Bitfield() { m_Bitfield.fill(0); }

    virtual ~Bitfield() = default; // virtual Reset requires a virtual destructor
    virtual void Reset();

    void SetBits(UINT32 startBit, UINT32 size, UINT64 value);
    void SetBit(UINT32 bit, UINT32 value);
    UINT64 GetBits(UINT32 startBit, UINT32 size) const;
    bool GetBit(UINT32 bit) const;

    //!
    //! \brief Return the total number of bits in the bitfield.
    //!
    //! \return Number of bits in bitfield.
    //!
    static constexpr UINT32 GetBitCount() { return s_BitCount; }

    //!
    //! \brief Return the number of array elements that make up the bitfield.
    //!
    //! \return Number of elements in bitfield.
    //!
    static constexpr UINT32 GetElementCount() { return s_ElementCount; }

    //!
    //! \brief Return a pointer to the bitfield array.
    //!
    ElementT* GetData() { return m_Bitfield.data(); }

    //!
    //! \brief Return a pointer to the constant bitfield array.
    //!
    const ElementT* GetData() const { return m_Bitfield.data(); }

    //!
    //! \brief Return an iterator to the beginning of the array.
    //!
    iterType begin() { return m_Bitfield.begin(); }
    constIterType begin() const { return m_Bitfield.begin(); }
    constIterType cbegin() const { return m_Bitfield.cbegin(); }

    //!
    //! \brief Return an iterator to the end of the array.
    //!
    iterType end() { return m_Bitfield.end(); }
    constIterType end() const { return m_Bitfield.end(); }
    constIterType cend() const { return m_Bitfield.cend(); }

    //!
    //! \brief Return a reverse iterator to the beginning.
    //!
    revIterType rbegin() { return m_Bitfield.rbegin(); }
    constRevIterType rbegin() const { return m_Bitfield.rbegin(); }
    constRevIterType crbegin() const { return m_Bitfield.crbegin(); }

    //!
    //! \brief Return a reverse iterator to the end.
    //!
    revIterType rend() { return m_Bitfield.rend(); }
    constRevIterType rend() const { return m_Bitfield.rend(); }
    constRevIterType crend() const { return m_Bitfield.crend(); }

    string ToString(bool showElementBoundary) const;
    string ArrayToString(bool showElementBoundary) const;

protected:
    array<ElementT, s_ElementCount> m_Bitfield;

    constexpr UINT32 Index(UINT32 bit) const;

    void SetElementBits(UINT32 index, ElementT offset,
                        ElementT size, ElementT value);
};

// Declaration of static members
template <class ElementT, UINT32 BitCount>
constexpr UINT32 Bitfield<ElementT, BitCount>::s_BitCount;
template <class ElementT, UINT32 BitCount>
constexpr UINT32 Bitfield<ElementT, BitCount>::s_ElementBitCount;
template <class ElementT, UINT32 BitCount>
constexpr UINT32 Bitfield<ElementT, BitCount>::s_ElementCount;
template <class ElementT, UINT32 BitCount>
constexpr UINT32 Bitfield<ElementT, BitCount>::s_NumBitsToRepElCount;

//------------------------------------------------------------------------------
namespace
{
    //!
    //! \brief Right bit shift the given value by the given amount.
    //!
    //! Assumes that the shift used to callwlate IsOverShift is the same as the
    //! given shift.
    //!
    //! Performs the shift when the shift amount would not 'overshift' the
    //! value's type, otherwise returns 0.
    //!
    //! \tparam IsOverShift True if the shift would be greater than or equal to
    //! the given value's type width. Use callwlation "sizeof(T) * CHAR_BIT >=
    //! shift".
    //! \tparam T Type of the value to shift.
    //! \param val Value to shift.
    //! \param shift Right shift amount.
    //!
    // NOTE: Uses underscore to avoid conflicting definitions. Must be declared
    // outside Bitfield class, otherwise error "explicit specialization in
    // non-namespace scope" is thrown.
    template <bool IsOverShift, class T>
    struct _BitShift
    {
        static constexpr T Right(T val, UINT32 shift)
        {
            // NOTE: Can't implicitly determine the shift here from the value T,
            // otherwise it would cause a compiler error on overshift.
            return val >> shift;
        }
    };

    template <class T>
    struct _BitShift<true, T>
    {
        static constexpr T Right(T val, UINT32 shift)
        {
            return 0;
        }
    };
};

//!
//! \brief Set all bits to 0.
//!
template <class ElementT, UINT32 BitCount>
/* virtual */ void Bitfield<ElementT, BitCount>::Reset()
{
    m_Bitfield.fill(0);
}

//!
//! \brief Set block of bits to the specified value.
//!
//! The given values is masked off to the lower 'size' bits; passing a value
//! that uses more than 'size' bits will have the upper bits truncated.
//!
//! \param startBit Starting bit.
//! \param size Number of bits to set. This can be less than the bits
//! provided as a value.
//! \param value Bits are set to this value.
//!
template <class ElementT, UINT32 BitCount>
void Bitfield<ElementT, BitCount>::SetBits
(
    UINT32 startBit,
    UINT32 size,
    UINT64 value
)
{
    // Don't write past the end of the bitfield
    MASSERT(startBit + size <= s_BitCount);

    UINT32 i = Index(startBit);
    MASSERT(i >= 0 && i < s_ElementCount); // Out of bounds check

    const UINT32 offset = startBit & Utility::Bitmask<ElementT>(s_NumBitsToRepElCount);

    if (size + offset > s_ElementBitCount)
    {
        // Value spans multiple array elements
        const UINT32 valPartSize = s_ElementBitCount - offset;
        SetElementBits(i, offset, valPartSize, static_cast<ElementT>(value));

        UINT32 remainingBits = size - valPartSize;
        UINT64 remainingVal = value >> (size - remainingBits);
        i--;

        while (remainingBits > 0 && (i < s_ElementCount))
        {
            if (remainingBits / s_ElementBitCount < 1)
            {
                // Last affected element
                SetElementBits(i, 0, remainingBits,
                               static_cast<ElementT>(remainingVal));
                remainingBits = 0;
            }
            else
            {
                SetElementBits(i, 0, s_ElementBitCount,
                               static_cast<ElementT>(remainingVal));
                remainingBits -= s_ElementBitCount;

                // NOTE: Trying to say:
                //     remainingVal = remainingVal >> s_ElementBitCount;
                //
                // However, when ElementT is the same as remainingVal's type, in
                // this case UINT64, the compiler warns:
                //     right shift count >= width of type.
                // The solution is to create a template specialization based on
                // whether there is an 'overshift':
                // - Return the regular shift if the shift is less than the type
                //   width.
                // - Return zero if it is an overshift.
                //
                // This hides the shift statement from the compiler under the
                // conditions where it would warn.
                constexpr bool isOverShift
                    = s_ElementBitCount >= sizeof(remainingVal) * CHAR_BIT;
                remainingVal
                    = _BitShift<isOverShift,
                                decltype(remainingVal)>::Right(remainingVal,
                                                               s_ElementBitCount);
            }

            i--;
        }
    }
    else
    {
        // Value fits in single array element
        SetElementBits(i, offset, size, static_cast<ElementT>(value));
    }
}

//!
//! \brief Set bit to given value.
//!
//! \param bit Bit to set.
//! \param value Bit value. Either 0 or 1.
//!
template <class ElementT, UINT32 BitCount>
void Bitfield<ElementT, BitCount>::SetBit(UINT32 bit, UINT32 value)
{
    // Don't write past end of bitfield
    MASSERT(bit < s_BitCount);
    // Value should be 0 or 1
    MASSERT(!value || (value & 1) == value);

    const UINT32 i = Index(bit);
    MASSERT(i >= 0 && i < s_ElementCount); // Out of bounds check

    SetElementBits(i, bit & Utility::Bitmask<ElementT>(s_NumBitsToRepElCount),
                   1, (value ? 1 : 0));
}

//!
//! \brief Return the value of 'size' number of bits starting from
//! 'startBit'.
//!
//! \param startBit Starting bit.
//! \param size Number of bits to get.
//! \return Bit value. Limited to returning UINT32 bits worth of data.
//!
template <class ElementT, UINT32 BitCount>
UINT64 Bitfield<ElementT, BitCount>::GetBits(UINT32 startBit, UINT32 size) const
{
    // Don't read past the end of the bitfield
    MASSERT(size + startBit <= s_BitCount);
    // Requested number of bits must be able to fit in the UINT64 return value
    MASSERT(size <= sizeof(UINT64) * CHAR_BIT);

    UINT64 bits = 0;

    UINT32 offset = startBit & Utility::Bitmask<ElementT>(s_NumBitsToRepElCount);
    UINT32 i = Index(startBit);
    MASSERT(i >= 0 && i < s_ElementCount); // Out of bounds check

    if (size + offset > s_ElementBitCount)
    {
        // Value spans multiple array elements
        const UINT32 valPartSize = s_ElementBitCount - offset;

        bits |= (m_Bitfield[i] & (Utility::Bitmask<ElementT>(valPartSize) << offset)) >> offset;

        UINT32 bitsOffset = valPartSize;
        UINT32 remainingBits = size - valPartSize;
        i--;

        while (remainingBits > 0 && (i < s_ElementCount))
        {
            if (remainingBits / s_ElementBitCount < 1)
            {
                // Last affected element
                const ElementT valPart = m_Bitfield[i] & Utility::Bitmask<ElementT>(remainingBits);
                bits |= static_cast<UINT64>(valPart) << bitsOffset;
                remainingBits = 0;
            }
            else
            {
                bits |= static_cast<UINT64>(m_Bitfield[i]) << bitsOffset;
                bitsOffset += s_ElementBitCount;
                remainingBits -= s_ElementBitCount;
            }

            i--;
        }
    }
    else
    {
        // Value fits in single array element
        bits = (m_Bitfield[i] & (Utility::Bitmask<ElementT>(size) << offset)) >> offset;
    }

    return bits;
}

//!
//! \brief Return the value of the given bit.
//!
//! \prarm bit Bit to get.
//! \return Value of the bit. Either 0 or 1.
//!
template <class ElementT, UINT32 BitCount>
bool Bitfield<ElementT, BitCount>::GetBit(UINT32 bit) const
{
    // Don't read past end of bitfield
    MASSERT(bit < s_BitCount);

    const UINT32 i = Index(bit);
    MASSERT(i >= 0 && i < s_ElementCount); // Out of bounds check

    return (m_Bitfield[i] & (1ULL << (bit % s_ElementBitCount))) != 0;
}

//!
//! \brief Return the string representation of the bitfield.
//!
//! \param showElementBounday Display the array element boundary using a
//! pipe symbol.
//! \return String representation of bitfield.
//!
template <class ElementT, UINT32 BitCount>
string Bitfield<ElementT, BitCount>::ToString(bool showElementBoundary) const
{
    string s;

    for (UINT32 n = 0; n < s_BitCount; n++)
    {
        const UINT32 bit = s_BitCount - 1 - n;
        const UINT32 offset = bit & Utility::Bitmask<ElementT>(s_NumBitsToRepElCount);
        const UINT32 i = Index(bit);
        MASSERT(i >= 0 && i < s_ElementCount); // Out of bounds check

        s += (m_Bitfield[i] & (1ULL << offset)) ? '1' : '0';

        // Display the array element boundary in the representation
        if (showElementBoundary && bit % s_ElementBitCount == 0)
        {
            s += '|';
        }
    }

    return s;
}

//!
//! \brief Return the string representation of the underlying array
//! format of the bitfield.
//!
//! This may differ from the bitfield representation depending on the underlying
//! storage method (ex. The endianess could be changed) and the bitfield's size
//! (ie. Will show the full array element, regardless of bitfield boundary).
//!
//! \param showElementBounday Display the array element boundary using a
//! pipe symbol.
//! \return String representation of bitfield.
//!
template <class ElementT, UINT32 BitCount>
string Bitfield<ElementT, BitCount>::ArrayToString(bool showElementBoundary) const
{
    string s;

    for (UINT32 i = 0; i < s_ElementCount; i++)
    {
        for (UINT32 n = 0; n < s_ElementBitCount; n++)
        {
            s += (m_Bitfield[i]
                  & (static_cast<UINT32>(1) << (s_ElementBitCount - 1 - n)) ? '1' : '0');
        }

        // Display the array element boundary in the representation
        if (showElementBoundary)
        {
            s += '|';
        }
    }

    return s;
}

//!
//! \brief Return the index of the bitfield array element that contains the
//! given bit.
//!
//! \param bit Bit in bitfield.
//! \return Array element index that bit oclwrs in.
//!
template <class ElementT, UINT32 BitCount>
constexpr
UINT32 Bitfield<ElementT, BitCount>::Index(UINT32 bit) const
{
    // NOTE: Use this form of indexing and change i-- to i++ to change
    // the endianess:
    // return bit / s_ElementBitCount;

    return s_ElementCount - 1 - (bit / s_ElementBitCount);
}

//!
//! \brief Set bits within single element of the bitfield array.
//!
//! The given value is masked off to the lower 'size' bits; passing a value
//! that uses more than 'size' bits will have the upper bits truncated.
//!
//! \param index Array index.
//! \param offset Bit offset into array element.
//! \param size Number of bits to set.
//! \param value Value to set bits to.
//!
template <class ElementT, UINT32 BitCount>
void Bitfield<ElementT, BitCount>::SetElementBits
(
    UINT32 index,
    ElementT offset,
    ElementT size,
    ElementT value
)
{
    MASSERT(index >= 0 && index < s_ElementCount); // Out of bounds check
    MASSERT(size + offset <= s_ElementBitCount); // Bit range must fit in ElementT

    const ElementT bitmask = Utility::Bitmask<ElementT>(size);
    ElementT e = m_Bitfield[index];

    // Clear bits where 'value' will be placed
    e = e & (~(bitmask << offset));

    // Set value
    m_Bitfield[index] = e | ((value & bitmask) << offset);
}

#endif
