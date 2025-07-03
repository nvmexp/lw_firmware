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

#include "core/utility/strong_typedef_helper.h"
#include "gtest/gtest.h"

namespace
{
    template<class T>
    void TestIdentifier()
    {
        // Postive cases
        //
        EXPECT_EQ(T(0), T(0));
        EXPECT_EQ(T(1), T(1));
        EXPECT_EQ(T(128379), T(128379));
        EXPECT_EQ(T(std::numeric_limits<UINT32>::min()), T(std::numeric_limits<UINT32>::min()));
        EXPECT_EQ(T(std::numeric_limits<UINT32>::max()), T(std::numeric_limits<UINT32>::max()));

        // Negative cases
        //
        EXPECT_NE(T(0), T(1));
        EXPECT_NE(T(1), T(0));
        EXPECT_NE(T(2), T(10));
        EXPECT_NE(T(3), T(100));
        EXPECT_NE(T(4), T(10234));
        EXPECT_NE(T(5), T(12));
        EXPECT_NE(T(25), T(0));
        EXPECT_NE(T(112093), T(1239071));
        EXPECT_NE(T(std::numeric_limits<UINT32>::min()), T(std::numeric_limits<UINT32>::max()));
    }
}

TEST(StrongTypedefHelperTest, Identifier)
{
    ASSERT_NO_FATAL_FAILURE(TestIdentifier<Identifier>());
}

TEST(StrongTypedefHelperTest, NumericIdentifier)
{
    ASSERT_NO_FATAL_FAILURE(TestIdentifier<NumericIdentifier>());

    // Positive cases
    //
    constexpr UINT32 testCases = 100;
    for (UINT32 n = 0; n < testCases; ++n)
    {
        ASSERT_EQ(NumericIdentifier(n).Number(), n);
    }

    EXPECT_EQ(NumericIdentifier(0).Number(), 0U);
    EXPECT_EQ(NumericIdentifier(1).Number(), 1U);
    EXPECT_EQ(NumericIdentifier(128379).Number(), 128379U);
    EXPECT_EQ(NumericIdentifier(std::numeric_limits<UINT32>::min()).Number(),
                                std::numeric_limits<UINT32>::min());
    EXPECT_EQ(NumericIdentifier(std::numeric_limits<UINT32>::max()).Number(),
                                std::numeric_limits<UINT32>::max());

    // Negative cases
    //
    EXPECT_NE(NumericIdentifier(0).Number(), 1U);
    EXPECT_NE(NumericIdentifier(1).Number(), 0U);
    EXPECT_NE(NumericIdentifier(2).Number(), 10U);
    EXPECT_NE(NumericIdentifier(3).Number(), 100U);
    EXPECT_NE(NumericIdentifier(4).Number(), 10234U);
    EXPECT_NE(NumericIdentifier(5).Number(), 12U);
    EXPECT_NE(NumericIdentifier(25).Number(), 0U);
    EXPECT_NE(NumericIdentifier(112093).Number(), 1239071U);
    EXPECT_NE(NumericIdentifier(std::numeric_limits<UINT32>::min()).Number(),
                                std::numeric_limits<UINT32>::max());

}
