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

#include "core/include/hbmtypes.h"
#include "gtest/gtest.h"

using namespace Memory::Hbm;
using Rank = Memory::Rank;

namespace
{
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
}

TEST(HbmTypesTest, Site)
{
    ASSERT_NO_FATAL_FAILURE(TestNumericIdent<Site>());
}

TEST(HbmTypesTest, Stack)
{
    ASSERT_NO_FATAL_FAILURE(TestNumericIdent<Stack>());
}

TEST(HbmTypesTest, Stack_RankColwersion)
{
    constexpr UINT32 testCases = 100;
    for (UINT32 n = 0; n < testCases; ++n)
    {
        const Stack s(n);
        const Rank r(n);
        ASSERT_EQ(s.Number(), r.Number());
        ASSERT_EQ(s, r);
    }
}

TEST(HbmTypesTest, Channel)
{
    ASSERT_NO_FATAL_FAILURE(TestNumericIdent<Channel>());
}
