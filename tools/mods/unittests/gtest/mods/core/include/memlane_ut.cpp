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

#include "core/include/memlane.h"
#include "gtest/gtest.h"

using namespace Memory;

namespace
{
    template<Lane::Type LaneT>
    void TestSingleLaneEquality(UINT32 laneBit)
    {
        Lane l1(laneBit, LaneT);
        Lane l2(laneBit, LaneT);
        ASSERT_EQ(l1, l2);
    }

    template<Lane::Type LaneT>
    void TestSingleFbpaLaneEquality(HwFbpa hwFbpa, UINT32 laneBit)
    {
        FbpaLane l1(hwFbpa, laneBit, LaneT);
        FbpaLane l2(hwFbpa, laneBit, LaneT);
        ASSERT_EQ(l1, l2);
    }

    template<Lane::Type LaneT>
    void TestSingleFbioLaneEquality(HwFbio hwFbio, UINT32 laneBit)
    {
        FbioLane l1(hwFbio, laneBit, LaneT);
        FbioLane l2(hwFbio, laneBit, LaneT);
        ASSERT_EQ(l1, l2);
    }

}

TEST(MemLaneTest, GetLaneTypeString)
{
    // Positive
    //
    EXPECT_EQ(Lane::GetLaneTypeString(Lane::Type::DATA), "DATA");
    EXPECT_EQ(Lane::GetLaneTypeString(Lane::Type::DBI), "DBI");
    EXPECT_EQ(Lane::GetLaneTypeString(Lane::Type::DM), "DM");
    EXPECT_EQ(Lane::GetLaneTypeString(Lane::Type::ADDR), "ADDR");
    EXPECT_EQ(Lane::GetLaneTypeString(Lane::Type::UNKNOWN), "UNKNOWN");

    // Negative
    //
    EXPECT_EQ(Lane::GetLaneTypeString(static_cast<Lane::Type>(10000)), "UNKNOWN");
    EXPECT_EQ(Lane::GetLaneTypeString(static_cast<Lane::Type>(197334)), "UNKNOWN");
    EXPECT_EQ(Lane::GetLaneTypeString(static_cast<Lane::Type>(_UINT32_MAX - 1)), "UNKNOWN");
}

TEST(MemLaneTest, GetLaneTypeFromString)
{
    // Positive
    //
    EXPECT_EQ(Lane::GetLaneTypeFromString("DATA"), Lane::Type::DATA);
    EXPECT_EQ(Lane::GetLaneTypeFromString("DBI"), Lane::Type::DBI);
    EXPECT_EQ(Lane::GetLaneTypeFromString("DM"), Lane::Type::DM);
    EXPECT_EQ(Lane::GetLaneTypeFromString("ECC"), Lane::Type::DM);    // same lane as DM
    EXPECT_EQ(Lane::GetLaneTypeFromString("ADDR"), Lane::Type::ADDR);
    EXPECT_EQ(Lane::GetLaneTypeFromString("CMD"), Lane::Type::ADDR);  // same lane as ADDR
    EXPECT_EQ(Lane::GetLaneTypeFromString("UNKNOWN"), Lane::Type::UNKNOWN);
    EXPECT_EQ(Lane::GetLaneTypeFromString("data"), Lane::Type::DATA);
    EXPECT_EQ(Lane::GetLaneTypeFromString("DaTA"), Lane::Type::DATA);

    // Negative
    //
    EXPECT_EQ(Lane::GetLaneTypeFromString(""), Lane::Type::UNKNOWN);
    EXPECT_EQ(Lane::GetLaneTypeFromString("asda"), Lane::Type::UNKNOWN);
    EXPECT_EQ(Lane::GetLaneTypeFromString("1563214"), Lane::Type::UNKNOWN);
    EXPECT_EQ(Lane::GetLaneTypeFromString("     "), Lane::Type::UNKNOWN);

    EXPECT_EQ(Lane::GetLaneTypeFromString("DATA     "), Lane::Type::UNKNOWN);
    EXPECT_EQ(Lane::GetLaneTypeFromString("   DATA"), Lane::Type::UNKNOWN);
    EXPECT_EQ(Lane::GetLaneTypeFromString("DA  TA"), Lane::Type::UNKNOWN);
}

//!
//! \brief Lanes that have unknown type should never be equal to any other lane.
//!
TEST(MemLaneTest, EqualityOfUnknownLane)
{
    EXPECT_NE(Lane(0, Lane::Type::UNKNOWN), Lane(0, Lane::Type::UNKNOWN));
    EXPECT_NE(Lane(0, Lane::Type::UNKNOWN), Lane(255, Lane::Type::UNKNOWN));
    EXPECT_NE(Lane(0, Lane::Type::UNKNOWN), Lane(0, Lane::Type::UNKNOWN));
    EXPECT_NE(Lane(), Lane());
}

TEST(MemLaneTest, Equality)
{
    constexpr UINT32 numTests = 256; // Using value of the number of lanes on a
    // GV100 subpartition.

    // Positive
    //
    for (UINT32 n = 0; n < numTests; ++n)
    {
        ASSERT_NO_FATAL_FAILURE(TestSingleLaneEquality<Lane::Type::DATA>(n));
        ASSERT_NO_FATAL_FAILURE(TestSingleLaneEquality<Lane::Type::DBI>(n));
        ASSERT_NO_FATAL_FAILURE(TestSingleLaneEquality<Lane::Type::DM>(n));
        ASSERT_NO_FATAL_FAILURE(TestSingleLaneEquality<Lane::Type::ADDR>(n));
    }

    // Negative
    //
    // Lane type
    EXPECT_NE(Lane(0, Lane::Type::DATA), Lane(0, Lane::Type::DBI));
    EXPECT_NE(Lane(0, Lane::Type::DATA), Lane(0, Lane::Type::DM));
    EXPECT_NE(Lane(0, Lane::Type::DATA), Lane(0, Lane::Type::ADDR));

    EXPECT_NE(Lane(0, Lane::Type::DBI),  Lane(0, Lane::Type::DM));
    EXPECT_NE(Lane(0, Lane::Type::ADDR), Lane(0, Lane::Type::DBI));
    EXPECT_NE(Lane(0, Lane::Type::DBI),  Lane(0, Lane::Type::ADDR));

    // Lane bit
    EXPECT_NE(Lane(0, Lane::Type::DATA), Lane(1, Lane::Type::DATA));
    EXPECT_NE(Lane(1, Lane::Type::DATA), Lane(0, Lane::Type::DATA));
    EXPECT_NE(Lane(0, Lane::Type::DATA), Lane(1000, Lane::Type::DATA));

    // Both
    EXPECT_NE(Lane(1, Lane::Type::DATA),   Lane(0, Lane::Type::DBI));
    EXPECT_NE(Lane(100, Lane::Type::DATA), Lane(7, Lane::Type::DM));
    EXPECT_NE(Lane(252, Lane::Type::DATA), Lane(251, Lane::Type::ADDR));
}

TEST(FbpaLaneTest, EqualityOfUnknownLanes)
{
    EXPECT_NE(FbpaLane(HwFbpa(0), 0, Lane::Type::UNKNOWN), FbpaLane(HwFbpa(0), 0, Lane::Type::UNKNOWN));
    EXPECT_NE(FbpaLane(HwFbpa(0), 0, Lane::Type::UNKNOWN), FbpaLane(HwFbpa(0), 255, Lane::Type::UNKNOWN));
    EXPECT_NE(FbpaLane(HwFbpa(0), 0, Lane::Type::UNKNOWN), FbpaLane(HwFbpa(0), 0, Lane::Type::DATA));
    EXPECT_NE(FbpaLane(), FbpaLane());
}

TEST(FbpaLaneTest, Equality)
{
    constexpr UINT32 numTests = 256; // Using value of the number of lanes on a
    // GV100 subpartition.

    // Positive
    //
    for (UINT32 n = 0; n < numTests; ++n)
    {
        ASSERT_NO_FATAL_FAILURE(TestSingleFbpaLaneEquality<Lane::Type::DATA>(HwFbpa(numTests - n), n));
        ASSERT_NO_FATAL_FAILURE(TestSingleFbpaLaneEquality<Lane::Type::DBI>(HwFbpa(numTests - n), n));
        ASSERT_NO_FATAL_FAILURE(TestSingleFbpaLaneEquality<Lane::Type::DM>(HwFbpa(numTests - n), n));
        ASSERT_NO_FATAL_FAILURE(TestSingleFbpaLaneEquality<Lane::Type::ADDR>(HwFbpa(numTests - n), n));
    }

    // Negative
    //
    // FBPA
    EXPECT_NE(FbpaLane(HwFbpa(0), 0, Lane::Type::DATA),   FbpaLane(HwFbpa(1),  0, Lane::Type::DATA));
    EXPECT_NE(FbpaLane(HwFbpa(1), 0, Lane::Type::DATA),   FbpaLane(HwFbpa(0),  0, Lane::Type::DATA));
    EXPECT_NE(FbpaLane(HwFbpa(100), 0, Lane::Type::DATA), FbpaLane(HwFbpa(34), 0, Lane::Type::DATA));

    // Lane type
    EXPECT_NE(FbpaLane(HwFbpa(0), 0, Lane::Type::DATA), FbpaLane(HwFbpa(0), 0, Lane::Type::DBI));
    EXPECT_NE(FbpaLane(HwFbpa(0), 0, Lane::Type::DATA), FbpaLane(HwFbpa(0), 0, Lane::Type::DM));
    EXPECT_NE(FbpaLane(HwFbpa(0), 0, Lane::Type::DATA), FbpaLane(HwFbpa(0), 0, Lane::Type::ADDR));
    EXPECT_NE(FbpaLane(HwFbpa(0), 0, Lane::Type::DBI),  FbpaLane(HwFbpa(0), 0, Lane::Type::DM));
    EXPECT_NE(FbpaLane(HwFbpa(0), 0, Lane::Type::ADDR), FbpaLane(HwFbpa(0), 0, Lane::Type::DBI));
    EXPECT_NE(FbpaLane(HwFbpa(0), 0, Lane::Type::DBI),  FbpaLane(HwFbpa(0), 0, Lane::Type::ADDR));

    // Lane bit
    EXPECT_NE(FbpaLane(HwFbpa(0), 0, Lane::Type::DATA), FbpaLane(HwFbpa(0), 1, Lane::Type::DATA));
    EXPECT_NE(FbpaLane(HwFbpa(1), 0, Lane::Type::DATA), FbpaLane(HwFbpa(0), 0, Lane::Type::DATA));
    EXPECT_NE(FbpaLane(HwFbpa(0), 0, Lane::Type::DATA), FbpaLane(HwFbpa(1), 1000, Lane::Type::DATA));

    // Both
    EXPECT_NE(FbpaLane(HwFbpa(23), 1, Lane::Type::DATA),  FbpaLane(HwFbpa(50), 0, Lane::Type::DBI));
    EXPECT_NE(FbpaLane(HwFbpa(3), 100, Lane::Type::DATA), FbpaLane(HwFbpa(34), 7, Lane::Type::DM));
    EXPECT_NE(FbpaLane(HwFbpa(0), 252, Lane::Type::DATA), FbpaLane(HwFbpa(101), 251, Lane::Type::ADDR));
}

TEST(FbioLaneTest, EqualityOfUnknownLanes)
{
    EXPECT_NE(FbioLane(HwFbio(0), 0, Lane::Type::UNKNOWN), FbioLane(HwFbio(0), 0, Lane::Type::UNKNOWN));
    EXPECT_NE(FbioLane(HwFbio(0), 0, Lane::Type::UNKNOWN), FbioLane(HwFbio(0), 255, Lane::Type::UNKNOWN));
    EXPECT_NE(FbioLane(HwFbio(0), 0, Lane::Type::UNKNOWN), FbioLane(HwFbio(0), 0, Lane::Type::DATA));
    EXPECT_NE(FbioLane(), FbioLane());
}

TEST(FbioLaneTest, Equality)
{
    constexpr UINT32 numTests = 256; // Using value of the number of lanes on a
    // GV100 subpartition.

    // Positive
    //
    for (UINT32 n = 0; n < numTests; ++n)
    {
        ASSERT_NO_FATAL_FAILURE(TestSingleFbioLaneEquality<Lane::Type::DATA>(HwFbio(numTests - n), n));
        ASSERT_NO_FATAL_FAILURE(TestSingleFbioLaneEquality<Lane::Type::DBI>(HwFbio(numTests - n), n));
        ASSERT_NO_FATAL_FAILURE(TestSingleFbioLaneEquality<Lane::Type::DM>(HwFbio(numTests - n), n));
        ASSERT_NO_FATAL_FAILURE(TestSingleFbioLaneEquality<Lane::Type::ADDR>(HwFbio(numTests - n), n));
    }

    // Negative
    //
    // FBPA
    EXPECT_NE(FbioLane(HwFbio(0), 0, Lane::Type::DATA),   FbioLane(HwFbio(1),  0, Lane::Type::DATA));
    EXPECT_NE(FbioLane(HwFbio(1), 0, Lane::Type::DATA),   FbioLane(HwFbio(0),  0, Lane::Type::DATA));
    EXPECT_NE(FbioLane(HwFbio(100), 0, Lane::Type::DATA), FbioLane(HwFbio(34), 0, Lane::Type::DATA));

    // Lane type
    EXPECT_NE(FbioLane(HwFbio(0), 0, Lane::Type::DATA), FbioLane(HwFbio(0), 0, Lane::Type::DBI));
    EXPECT_NE(FbioLane(HwFbio(0), 0, Lane::Type::DATA), FbioLane(HwFbio(0), 0, Lane::Type::DM));
    EXPECT_NE(FbioLane(HwFbio(0), 0, Lane::Type::DATA), FbioLane(HwFbio(0), 0, Lane::Type::ADDR));
    EXPECT_NE(FbioLane(HwFbio(0), 0, Lane::Type::DBI),  FbioLane(HwFbio(0), 0, Lane::Type::DM));
    EXPECT_NE(FbioLane(HwFbio(0), 0, Lane::Type::ADDR), FbioLane(HwFbio(0), 0, Lane::Type::DBI));
    EXPECT_NE(FbioLane(HwFbio(0), 0, Lane::Type::DBI),  FbioLane(HwFbio(0), 0, Lane::Type::ADDR));

    // Lane bit
    EXPECT_NE(FbioLane(HwFbio(0), 0, Lane::Type::DATA), FbioLane(HwFbio(0), 1, Lane::Type::DATA));
    EXPECT_NE(FbioLane(HwFbio(1), 0, Lane::Type::DATA), FbioLane(HwFbio(0), 0, Lane::Type::DATA));
    EXPECT_NE(FbioLane(HwFbio(0), 0, Lane::Type::DATA), FbioLane(HwFbio(1), 1000, Lane::Type::DATA));

    // Both
    EXPECT_NE(FbioLane(HwFbio(23), 1, Lane::Type::DATA),  FbioLane(HwFbio(50), 0, Lane::Type::DBI));
    EXPECT_NE(FbioLane(HwFbio(3), 100, Lane::Type::DATA), FbioLane(HwFbio(34), 7, Lane::Type::DM));
    EXPECT_NE(FbioLane(HwFbio(0), 252, Lane::Type::DATA), FbioLane(HwFbio(101), 251, Lane::Type::ADDR));
}
