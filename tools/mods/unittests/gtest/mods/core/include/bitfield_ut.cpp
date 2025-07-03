/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/bitfield.h"
#include "core/include/types.h"
#include "gtest/gtest.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <random>

class BitfieldTest : public testing::Test
{
};

TEST_F(BitfieldTest, IsZeroed)
{
    // Single element
    Bitfield<UINT32, 32> bitfieldSingle;
    EXPECT_EQ(*bitfieldSingle.GetData(), 0U);

    // Multiple elements
    Bitfield<UINT32, 96> bitfieldMulti;
    for (auto& e : bitfieldMulti)
    {
        EXPECT_EQ(e, 0U);
    }

    // Bitfield oclwpies part of an element
    Bitfield<UINT32, 8> bitfieldPartialEl;
    EXPECT_EQ(*bitfieldPartialEl.GetData(), 0U);

    // Bitfield oclwpies part of an element in multi element array
    Bitfield<UINT32, 49> bitfieldPartialElMulti;
    for (auto& e : bitfieldPartialElMulti)
    {
        EXPECT_EQ(e, 0U);
    }
}

//!
//! \brief Test element count at various array types and bit sizes.
//!
TEST_F(BitfieldTest, ElCountAtVariousT)
{
#define TEST_EL_COUNT(T, bitCount)                                      \
    do                                                                  \
    {                                                                   \
        Bitfield<T, (bitCount)> bitfield;                               \
        EXPECT_EQ(bitfield.GetBitCount(), (bitCount));                  \
        const UINT32 expectedElCount                                    \
            = static_cast<UINT32>(std::ceil(static_cast<float>(bitCount) \
                                            / (sizeof(T) * CHAR_BIT))); \
        EXPECT_EQ(bitfield.GetElementCount(), expectedElCount);         \
    }                                                                   \
    while (0)                                                           \

    // Single element
    TEST_EL_COUNT(UINT08, 5U);
    TEST_EL_COUNT(UINT16, 5U);
    TEST_EL_COUNT(UINT32, 5U);
    TEST_EL_COUNT(UINT64, 5U);

    // Multiple elements
    TEST_EL_COUNT(UINT08, 67U);
    TEST_EL_COUNT(UINT16, 129U);
    TEST_EL_COUNT(UINT32, 237U);
    TEST_EL_COUNT(UINT64, 543U);

    // Bit count oclwpies all of last element (multiple of the bits in ElementT)
    TEST_EL_COUNT(UINT08, 64U);
    TEST_EL_COUNT(UINT16, 128U);
    TEST_EL_COUNT(UINT32, 256U);
    TEST_EL_COUNT(UINT64, 512U);

#undef TEST_EL_COUNT
}

//!
//! \brief When setting a value that has set bits beyond the specified size
//! range, the upper bits of the value should be truncated after the specified
//! size.
//!
TEST_F(BitfieldTest, TruncateOversizedValue)
{
    // Check truncation oclwrs at correct length
    //
    Bitfield<UINT32, 64> b;

    // Single element
    b.SetBits(4, 4, 0xffff);
    EXPECT_EQ(b.GetBits(4, 4), 0x0fU);
    EXPECT_EQ(b.GetBits(0, 32), 0x0fU << 4);
    b.Reset();

    // Crossing an element boundary
    b.SetBits(24, 16, 0xffffffff);
    EXPECT_EQ(b.GetBits(24, 16), 0xffffU);
    EXPECT_EQ(b.GetBits(0, 32), 0xffU << 24); // Lower el
    EXPECT_EQ(b.GetBits(32, 32), 0xffU);      // Upper el
    b.Reset();

    // Check correct portion was truncated
    //
    // Single element
    b.SetBits(4, 8, 0x1A5f);
    EXPECT_EQ(b.GetBits(4, 8), 0x5fU);
    EXPECT_EQ(b.GetBits(0, 32), 0x5fU << 4);
    b.Reset();

    b.SetBits(4, 8, 0xf5A1);
    EXPECT_EQ(b.GetBits(4, 8), 0xA1U);
    EXPECT_EQ(b.GetBits(0, 32), 0xA1U << 4);
    b.Reset();

    // Crossing an element boundary
    b.SetBits(24, 16, 0x1A5fA51A5f);
    EXPECT_EQ(b.GetBits(24, 16), 0x1A5fU);
    EXPECT_EQ(b.GetBits(0, 32), 0x5fU << 24); // Lower el
    EXPECT_EQ(b.GetBits(32, 32), 0x1AU);      // Upper el
    b.Reset();
}

//!
//! \brief Test setting bits when bit count is equal to the bitwidth of the
//! element type.
//!
TEST_F(BitfieldTest, MaxVarSizeBitCount)
{
    { // UINT64
        Bitfield<UINT64, 64> b;

        constexpr UINT64 val0 = 0x955;
        b.SetBits(48, 16, val0);
        EXPECT_EQ(b.GetBits(48, 16), val0);

        constexpr UINT64 val1 = 0x2;
        b.SetBits(32, 5, val1);
        EXPECT_EQ(b.GetBits(32, 5), val1);
        EXPECT_EQ(b.GetBits(48, 16), val0);

        // Overwrite previous value
        constexpr UINT64 val2 = 0xf000f;
        b.SetBits(44, 20, val2);
        EXPECT_EQ(b.GetBits(32, 5), val1);
        EXPECT_NE(b.GetBits(48, 16), val0); // overridden by last set
        EXPECT_EQ(b.GetBits(44, 20), val2);
    }

    { // UINT32
        Bitfield<UINT32, 32> b;

        constexpr UINT64 val0 = 0x955;
        b.SetBits(16, 16, val0);
        EXPECT_EQ(b.GetBits(16, 16), val0);

        constexpr UINT64 val1 = 0x2;
        b.SetBits(0, 5, val1);
        EXPECT_EQ(b.GetBits(0, 5), val1);
        EXPECT_EQ(b.GetBits(16, 16), val0);

        // Overwrite previous value
        constexpr UINT64 val2 = 0xf000f;
        b.SetBits(4, 20, val2);
        EXPECT_EQ(b.GetBits(4, 20), val2);
        EXPECT_EQ(b.GetBits(0, 4), val1 & 0xf);
        EXPECT_NE(b.GetBits(16, 16), val0); // overridden by last set
        EXPECT_EQ(b.GetBits(24, 8), val0 >> 8); // existing value only overriden where expected
    }

    { // UINT16
        Bitfield<UINT16, 16> b;

        constexpr UINT64 val0 = 0x955;
        b.SetBits(4, 12, val0);
        EXPECT_EQ(b.GetBits(4, 12), val0);

        constexpr UINT64 val1 = 0x2;
        b.SetBits(0, 4, val1);
        EXPECT_EQ(b.GetBits(0, 4), val1);
        EXPECT_EQ(b.GetBits(4, 12), val0);

        // Overwrite previous value
        constexpr UINT64 val2 = 0xf0f;
        b.SetBits(0, 12, val2);
        EXPECT_NE(b.GetBits(0, 4), val1);  // overridden by last set
        EXPECT_NE(b.GetBits(4, 12), val0); // overridden by last set
        EXPECT_EQ(b.GetBits(0, 12), val2);
        EXPECT_EQ(b.GetBits(12, 4), val0 >> 8); // Check partial value
    }

    { // UINT08
        Bitfield<UINT08, 8> b;

        constexpr UINT64 val0 = 0b101;
        b.SetBits(4, 3, val0);
        EXPECT_EQ(b.GetBits(4, 3), val0);

        constexpr UINT64 val1 = 1;
        b.SetBits(5, 1, val1);
        EXPECT_EQ(b.GetBits(5, 1), val1);
        EXPECT_EQ(b.GetBits(4, 3), val0 | (val1 << 1));

        // Overwrite previous value
        constexpr UINT64 val2 = 0b01001111;
        b.SetBits(1, 7, val2);
        EXPECT_EQ(b.GetBits(1, 7), val2);
        EXPECT_EQ(b.GetBits(0, 8), val2 << 1);
    }
}

namespace
{
    template <typename ElementT>
    void SetFullElementTTest()
    {
        constexpr UINT32 numElements = 3;
        constexpr UINT32 bitsPerElement = sizeof(ElementT) * CHAR_BIT;
        constexpr ElementT value = ~static_cast<ElementT>(0);
        Bitfield<ElementT, bitsPerElement * numElements> b;

        for (UINT32 offset = 0; offset < b.GetBitCount(); offset += bitsPerElement)
        {
            b.SetBits(offset, bitsPerElement, value);
            EXPECT_EQ(b.GetBits(offset, bitsPerElement), value); // check after set
        }

        // Check final state after modification
        for (UINT32 offset = 0; offset < b.GetBitCount(); offset += bitsPerElement)
        {
            EXPECT_EQ(b.GetBits(offset, bitsPerElement), value);
        }
    }
};

//!
//! \brief Test setting the full element at various T.
//!
//! Tests an issue revealed from the usage in LwdaTTUStress.
//!
TEST_F(BitfieldTest, SetFullElT)
{
    SetFullElementTTest<UINT08>();
    SetFullElementTTest<UINT16>();
    SetFullElementTTest<UINT32>();
    SetFullElementTTest<UINT64>();
}

//!
//! \brief Test various hand crafted setting and getting of bits.
//!
//! This is a hand tuned set of manipulations that are expected to work.
//!
TEST_F(BitfieldTest, VariousKnownManipulation)
{
    Bitfield<UINT32, 93> a;

    a.SetBit(0, true);
    EXPECT_EQ(a.GetBit(0), true);

    a.SetBits(4, 2, 0b11);
    EXPECT_EQ(a.GetBits(4, 2), 0b11U);
    EXPECT_EQ(a.GetBits(3, 4), 0b0110U);

    a.Reset();
    for (auto& e : a)
    {
        EXPECT_EQ(e, 0U);
    }

    a.Reset();
    a.SetBits(2, 64, static_cast<UINT64>(0)-1);
    EXPECT_EQ(a.GetBits(0, 2), 0U);
    EXPECT_EQ(a.GetBits(2, 64), (static_cast<UINT64>(0)-1));
    EXPECT_EQ(a.GetBits(64, 29), 3U);
    a.SetBits(13, 8, 0);
    EXPECT_EQ(a.GetBits(13, 7), 0U);
    EXPECT_NE(a.GetBits(2, 64), static_cast<UINT64>(0)-1);
    EXPECT_EQ(a.GetBits(64, 29), 3U);
    a.SetBits(45, 4, 0);
    EXPECT_EQ(a.GetBits(45, 4), 0U);
    EXPECT_NE(a.GetBits(2, 64), static_cast<UINT64>(0)-1);
    EXPECT_EQ(a.GetBits(64, 29), 3U);
}

//!
//! \brief Tests consistency of different backed types with random manipulation.
//!
TEST_F(BitfieldTest, ConsistentBehavior)
{
    constexpr UINT32 numMods = 100;
    constexpr UINT32 bitfieldSize = 64;

    Bitfield<UINT64, bitfieldSize> b64;
    Bitfield<UINT32, bitfieldSize> b32;
    Bitfield<UINT16, bitfieldSize> b16;
    Bitfield<UINT08, bitfieldSize> b08;
    std::array<UINT64, 4> vals;

    // Seed random number gen
    constexpr UINT32 randSeed = 0; // Fixed seed for reproducibility
    std::mt19937_64 randGen(randSeed);
    std::uniform_int_distribution<UINT64> dist64(0, std::numeric_limits<UINT64>::max());
    std::uniform_int_distribution<UINT32> dist32(0, std::numeric_limits<UINT32>::max());

    for (UINT32 n = 0; n < numMods; ++n)
    {
        // Randomize an bit, length, and number
        const UINT32 setBit = dist32(randGen) % bitfieldSize;
        const UINT32 setLen = (dist32(randGen) % (bitfieldSize - setBit)) + 1;
        const UINT64 setVal = dist64(randGen);

        // Set the number
        b64.SetBits(setBit, setLen, setVal);
        b32.SetBits(setBit, setLen, setVal);
        b16.SetBits(setBit, setLen, setVal);
        b08.SetBits(setBit, setLen, setVal);

        // Check that all bitfields are the same (UINT64)
        vals = {
            b64.GetBits(0, bitfieldSize),
            b32.GetBits(0, bitfieldSize),
            b16.GetBits(0, bitfieldSize),
            b08.GetBits(0, bitfieldSize)
        };

        // Test SetBits/GetBits: All elements should be equal
        ASSERT_TRUE(std::adjacent_find(vals.begin(), vals.end(), std::not_equal_to<UINT64>()) == vals.end())
            << std::hex << "[loop " << n << "] GetBits disagrees at bit " << setBit << ", length " << bitfieldSize
            << ": {u64(0x" << vals[0] << "), u32(0x" << vals[1] << "), u16(0x" << vals[2] << "), u08(0x" << vals[3] << ")}";


        // Test GetBits: Check that bits are the same in a sliding window
        // Window is set smaller than element of UINT08 to trigger in-element
        // bit retrieval for all backing types
        constexpr UINT32 windowSize = 4;
        for (UINT32 bit = 0; bit < bitfieldSize - windowSize; ++bit)
        {
            vals = {
                b64.GetBits(bit, windowSize),
                b32.GetBits(bit, windowSize),
                b16.GetBits(bit, windowSize),
                b08.GetBits(bit, windowSize)
            };

            ASSERT_TRUE(std::adjacent_find(vals.begin(), vals.end(), std::not_equal_to<UINT64>()) == vals.end())
                << std::hex << "[loop " << n << "] GetBits disagrees at bit " << bit << ", length " << windowSize
                << ": {u64(0x" << vals[0] << "), u32(0x" << vals[1] << "), u16(0x" << vals[2] << "), u08(0x" << vals[3] << ")}";
        }
    }
}
