/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/utility.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "gmock/gmock-matchers.h"
#include "gtest/gtest.h"

#include <limits>

using namespace Utility;

TEST(UtilityTest, IsWithinBitWidth)
{
    // Positive
    //
    EXPECT_TRUE(IsWithinBitWidth(0, 32));
    EXPECT_TRUE(IsWithinBitWidth(std::numeric_limits<UINT32>::max(), 32));
    EXPECT_TRUE(IsWithinBitWidth(0, 0));

    EXPECT_TRUE(IsWithinBitWidth(0xf, 4));
    EXPECT_TRUE(IsWithinBitWidth(0xf, 5));
    EXPECT_TRUE(IsWithinBitWidth(0xf, 8));
    EXPECT_TRUE(IsWithinBitWidth(0xf, 32));

    for (UINT32 n = 0; n < 32; ++n)
    {
        EXPECT_TRUE(IsWithinBitWidth(1U << n, n + 1));
    }

    // Negative
    //
    EXPECT_FALSE(IsWithinBitWidth(0xf, 0));
    EXPECT_FALSE(IsWithinBitWidth(0xf, 1));
    EXPECT_FALSE(IsWithinBitWidth(0xf, 2));
    EXPECT_FALSE(IsWithinBitWidth(0xf, 3));

    EXPECT_FALSE(IsWithinBitWidth(std::numeric_limits<UINT32>::max(), 0));
    EXPECT_FALSE(IsWithinBitWidth(std::numeric_limits<UINT32>::max(), 31));
}

TEST(UtilityTest, GetUniformDistribution)
{
    // 2-2
    vector<UINT32> dist1;
    Utility::GetUniformDistribution(2, 4, &dist1);
    EXPECT_THAT(dist1, ::testing::ElementsAre(2, 2));

    // 3-2-2
    vector<UINT32> dist2;
    Utility::GetUniformDistribution(3, 7, &dist2);
    EXPECT_THAT(dist2, ::testing::ElementsAre(3, 2, 2));

    // 3-3-2
    vector<UINT32> dist3;
    Utility::GetUniformDistribution(3, 8, &dist3);
    EXPECT_THAT(dist3, ::testing::ElementsAre(3, 3, 2));

    // 3
    vector<UINT32> dist4;
    Utility::GetUniformDistribution(1, 3, &dist4);
    EXPECT_THAT(dist4, ::testing::ElementsAre(3));
}
