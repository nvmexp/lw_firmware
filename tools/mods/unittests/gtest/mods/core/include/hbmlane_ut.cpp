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

#include "core/include/hbmlane.h"
#include "gtest/gtest.h"

using namespace Memory;
using LaneT = Lane::Type;
using HbmLane = Hbm::HbmLane;
using HbmSite = Hbm::Site;
using HbmChannel = Hbm::Channel;

namespace
{
    template<LaneT LaneType>
    void TestSingleHbmLaneEquality(HbmSite site, HbmChannel channel, UINT32 laneBit)
    {
        HbmLane l1(site, channel, laneBit, LaneType);
        HbmLane l2(site, channel, laneBit, LaneType);
        ASSERT_EQ(l1, l2);
    }
}

TEST(HbmLaneTest, EqualityOfUnknownLanes)
{
    EXPECT_NE(HbmLane(HbmSite(0), HbmChannel(0), 0, LaneT::UNKNOWN),
              HbmLane(HbmSite(0), HbmChannel(0), 0, LaneT::UNKNOWN));
    EXPECT_NE(HbmLane(HbmSite(0), HbmChannel(0), 0, LaneT::UNKNOWN),
              HbmLane(HbmSite(0), HbmChannel(0), 255, LaneT::UNKNOWN));
    EXPECT_NE(HbmLane(HbmSite(0), HbmChannel(0), 0, LaneT::UNKNOWN),
              HbmLane(HbmSite(0), HbmChannel(0), 0, LaneT::DATA));
    EXPECT_NE(HbmLane(), HbmLane());
}

TEST(HbmLaneTest, Equality)
{
    constexpr UINT32 numTests = 256;   // Using value of the number of lanes on a GV100 HBM channel
    constexpr UINT32 sitesToCycle = 6; // Using value of the number of HBM sites on GA100
    // GV100 subpartition.

    // Positive
    //
    for (UINT32 n = 0; n < numTests; ++n)
    {
        const HbmSite site(n % sitesToCycle);
        const HbmChannel channel(numTests - n);
        const UINT32 laneBit = n;
        ASSERT_NO_FATAL_FAILURE(TestSingleHbmLaneEquality<LaneT::DATA>(site, channel, laneBit));
        ASSERT_NO_FATAL_FAILURE(TestSingleHbmLaneEquality<LaneT::DBI>(site, channel, laneBit));
        ASSERT_NO_FATAL_FAILURE(TestSingleHbmLaneEquality<LaneT::DM>(site, channel, laneBit));
        ASSERT_NO_FATAL_FAILURE(TestSingleHbmLaneEquality<LaneT::ADDR>(site, channel, laneBit));
    }

    // Negative
    //
    // HBM site
    EXPECT_NE(HbmLane(HbmSite(0), HbmChannel(0), 0, LaneT::DATA),
              HbmLane(HbmSite(1), HbmChannel(0), 0, LaneT::DATA));
    EXPECT_NE(HbmLane(HbmSite(1), HbmChannel(0), 0, LaneT::DATA),
              HbmLane(HbmSite(0), HbmChannel(0), 0, LaneT::DATA));
    EXPECT_NE(HbmLane(HbmSite(100), HbmChannel(0), 0, LaneT::DATA),
              HbmLane(HbmSite(34), HbmChannel(0), 0, LaneT::DATA));

    // HBM channel
    EXPECT_NE(HbmLane(HbmSite(0), HbmChannel(0), 0, LaneT::DATA),
              HbmLane(HbmSite(0), HbmChannel(1),  0, LaneT::DATA));
    EXPECT_NE(HbmLane(HbmSite(0), HbmChannel(1), 0, LaneT::DATA),
              HbmLane(HbmSite(0), HbmChannel(0),  0, LaneT::DATA));
    EXPECT_NE(HbmLane(HbmSite(0), HbmChannel(100), 0, LaneT::DATA),
              HbmLane(HbmSite(0), HbmChannel(34), 0, LaneT::DATA));

    // Lane type
    EXPECT_NE(HbmLane(HbmSite(0), HbmChannel(0), 0, LaneT::DATA),
              HbmLane(HbmSite(0), HbmChannel(0), 0, LaneT::DBI));
    EXPECT_NE(HbmLane(HbmSite(0), HbmChannel(0), 0, LaneT::DATA),
              HbmLane(HbmSite(0), HbmChannel(0), 0, LaneT::DM));
    EXPECT_NE(HbmLane(HbmSite(0), HbmChannel(0), 0, LaneT::DATA),
              HbmLane(HbmSite(0), HbmChannel(0), 0, LaneT::ADDR));
    EXPECT_NE(HbmLane(HbmSite(0), HbmChannel(0), 0, LaneT::DBI),
              HbmLane(HbmSite(0), HbmChannel(0), 0, LaneT::DM));
    EXPECT_NE(HbmLane(HbmSite(0), HbmChannel(0), 0, LaneT::ADDR),
              HbmLane(HbmSite(0), HbmChannel(0), 0, LaneT::DBI));
    EXPECT_NE(HbmLane(HbmSite(0), HbmChannel(0), 0, LaneT::DBI),
              HbmLane(HbmSite(0), HbmChannel(0), 0, LaneT::ADDR));

    // Lane bit
    EXPECT_NE(HbmLane(HbmSite(0), HbmChannel(0), 0, LaneT::DATA),
              HbmLane(HbmSite(0), HbmChannel(0), 1, LaneT::DATA));
    EXPECT_NE(HbmLane(HbmSite(0), HbmChannel(1), 0, LaneT::DATA),
              HbmLane(HbmSite(0), HbmChannel(0), 0, LaneT::DATA));
    EXPECT_NE(HbmLane(HbmSite(0), HbmChannel(0), 0, LaneT::DATA),
              HbmLane(HbmSite(0), HbmChannel(1), 1000, LaneT::DATA));

    // All
    EXPECT_NE(HbmLane(HbmSite(0), HbmChannel(23), 1, LaneT::DATA),
              HbmLane(HbmSite(1), HbmChannel(50), 0, LaneT::DBI));
    EXPECT_NE(HbmLane(HbmSite(8967), HbmChannel(3), 100, LaneT::DATA),
              HbmLane(HbmSite(8966), HbmChannel(34), 7, LaneT::DM));
    EXPECT_NE(HbmLane(HbmSite(4), HbmChannel(0), 252, LaneT::DATA),
              HbmLane(HbmSite(0), HbmChannel(101), 251, LaneT::ADDR));
}
