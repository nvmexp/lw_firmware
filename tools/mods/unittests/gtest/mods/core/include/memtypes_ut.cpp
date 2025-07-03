/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/memtypes.h"
#include "core/utility/char_utility.h"
#include "gtest/gtest.h"

using namespace Memory;

namespace
{
    template<class T>
    void TestFbComponentFromNumber(UINT32 n)
    {
        T t(n);
        EXPECT_EQ(t.Number(), n);
        EXPECT_EQ(t.Letter(), static_cast<char>('A' + n));
    }

    template<class T>
    void TestFbComponentFromLetter(char c)
    {
        T t = T::FromLetter(c);
        const char upperC = CharUtility::ToUpperCase(c);
        EXPECT_EQ(t.Number(), static_cast<UINT32>(upperC - 'A'));
        EXPECT_EQ(t.Letter(), upperC);
    }

    template<class T>
    void TestFbComponent()
    {
        // Positive cases
        //
        constexpr UINT32 alphabetSize = 26;
        for (UINT32 n = 0; n < alphabetSize; ++n)
        {
            ASSERT_NO_FATAL_FAILURE(TestFbComponentFromNumber<T>(n));
            ASSERT_NO_FATAL_FAILURE(TestFbComponentFromLetter<T>(static_cast<char>(n + 'A')));
        }

        // Negative cases
        //
        EXPECT_NE(T(0), T(1));
        EXPECT_NE(T(1), T(0));
        EXPECT_NE(T(2), T(10));
        EXPECT_NE(T(3), T(100));
        EXPECT_NE(T(4), T(10234));
        EXPECT_NE(T(5), T(12));
        EXPECT_NE(T(25), T(0));

        EXPECT_NE(T(0), T::FromLetter('B'));
        EXPECT_NE(T(1), T::FromLetter('F'));
        EXPECT_NE(T(2), T::FromLetter('G'));
        EXPECT_NE(T(3), T::FromLetter('Z'));
        EXPECT_NE(T(4), T::FromLetter('Z'));
        EXPECT_NE(T(5), T::FromLetter('Z'));
        EXPECT_NE(T(25), T::FromLetter('A'));
    }

    template<class T>
    void TestNumericIdent()
    {
        // Positive cases
        //
        constexpr UINT32 testCases = 100;
        for (UINT32 n = 0; n < testCases; ++n)
        {
            ASSERT_EQ(T(n).Number(), n);
        }

        // Negative cases
        //
        EXPECT_NE(T(0), T(1));
        EXPECT_NE(T(1), T(0));
        EXPECT_NE(T(2), T(10));
        EXPECT_NE(T(3), T(100));
        EXPECT_NE(T(4), T(10234));
        EXPECT_NE(T(5), T(12));
        EXPECT_NE(T(25), T(0));
    }

    template<class T>
    void TestFbChannelComponent()
    {
        ASSERT_NO_FATAL_FAILURE(TestFbComponent<T>());

        // Positive cases
        //
        EXPECT_EQ(T(0) * 0, 0U);
        EXPECT_EQ(0 * T(0), 0U);
        EXPECT_EQ(T(0) * 2, 0U);
        EXPECT_EQ(2 * T(0), 0U);
        EXPECT_EQ(T(10) * 10, 100U);
        EXPECT_EQ(10 * T(10), 100U);
        EXPECT_EQ(T(11) * 10, 110U);
        EXPECT_EQ(10 * T(11), 110U);

        // Negative cases
        //
        EXPECT_NE(T(0) * 1, 1U);
    }
}

TEST(MemTypesTest, HwFbp)
{
    ASSERT_NO_FATAL_FAILURE(TestFbComponent<HwFbp>());
}

TEST(MemTypesTest, VirtFbp)
{
    ASSERT_NO_FATAL_FAILURE(TestFbComponent<VirtFbp>());
}

TEST(MemTypesTest, HwFbpa)
{
    ASSERT_NO_FATAL_FAILURE(TestFbComponent<HwFbpa>());
}

TEST(MemTypesTest, VirtFbpa)
{
    ASSERT_NO_FATAL_FAILURE(TestFbComponent<VirtFbpa>());
}

TEST(MemTypesTest, HwFbio)
{
    ASSERT_NO_FATAL_FAILURE(TestFbComponent<HwFbio>());
}

TEST(MemTypesTest, VirtFbio)
{
    ASSERT_NO_FATAL_FAILURE(TestFbComponent<VirtFbio>());
}

TEST(MemTypesTest, FbpaSubp)
{
    ASSERT_NO_FATAL_FAILURE(TestFbChannelComponent<FbpaSubp>());
}

TEST(MemTypesTest, FbioSubp)
{
    ASSERT_NO_FATAL_FAILURE(TestFbChannelComponent<FbioSubp>());
}

TEST(MemTypesTest, HwLtc)
{
    ASSERT_NO_FATAL_FAILURE(TestFbComponent<HwLtc>());
}

TEST(MemTypesTest, VirtLtc)
{
    ASSERT_NO_FATAL_FAILURE(TestFbComponent<VirtLtc>());
}

TEST(MemTypesTest, Rank)
{
    ASSERT_NO_FATAL_FAILURE(TestNumericIdent<Rank>());
}

TEST(MemTypesTest, Bank)
{
    ASSERT_NO_FATAL_FAILURE(TestNumericIdent<Bank>());
}
