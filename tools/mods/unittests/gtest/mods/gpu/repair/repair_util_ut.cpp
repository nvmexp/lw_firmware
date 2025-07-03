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

#include "gpu/repair/repair_util.h"
#include "gtest/gtest.h"

using namespace Memory;

TEST(RepairUtilTest, DISABLED_GetLanesFromFileFormat)
{
    // TODO(sdsmith):
}

TEST(RepairUtilTest, GetLaneFromString)
{
    FbpaLane lane;

    // Positive cases
    //
    ASSERT_TRUE(Repair::GetLaneFromString("A000_DATA", &lane));
    EXPECT_EQ(lane, FbpaLane(HwFbpa::FromLetter('A'), 0, FbpaLane::Type::DATA));

    ASSERT_TRUE(Repair::GetLaneFromString("Z000_DATA", &lane));
    EXPECT_EQ(lane, FbpaLane(HwFbpa::FromLetter('Z'), 0, FbpaLane::Type::DATA));

    ASSERT_TRUE(Repair::GetLaneFromString("A255_DATA", &lane));
    EXPECT_EQ(lane, FbpaLane(HwFbpa::FromLetter('A'), 255, FbpaLane::Type::DATA));

    ASSERT_TRUE(Repair::GetLaneFromString("B111_DATA", &lane));
    EXPECT_EQ(lane, FbpaLane(HwFbpa::FromLetter('B'), 111, FbpaLane::Type::DATA));

    ASSERT_TRUE(Repair::GetLaneFromString("C003_DATA", &lane));
    EXPECT_EQ(lane, FbpaLane(HwFbpa::FromLetter('C'), 3, FbpaLane::Type::DATA));

    ASSERT_TRUE(Repair::GetLaneFromString("D100_DBI", &lane));
    EXPECT_EQ(lane, FbpaLane(HwFbpa::FromLetter('D'), 100, FbpaLane::Type::DBI));

    ASSERT_TRUE(Repair::GetLaneFromString("E090_DM", &lane));
    EXPECT_EQ(lane, FbpaLane(HwFbpa::FromLetter('E'), 90, FbpaLane::Type::DM));

    ASSERT_TRUE(Repair::GetLaneFromString("F990_ECC", &lane));
    EXPECT_EQ(lane, FbpaLane(HwFbpa::FromLetter('F'), 990, FbpaLane::Type::DM)); // DM and ECC are the same physical lane

    ASSERT_TRUE(Repair::GetLaneFromString("G011_ADDR", &lane));
    EXPECT_EQ(lane, FbpaLane(HwFbpa::FromLetter('G'), 11, FbpaLane::Type::ADDR));

    ASSERT_TRUE(Repair::GetLaneFromString("H000_CMD", &lane));
    EXPECT_EQ(lane, FbpaLane(HwFbpa::FromLetter('H'), 0, FbpaLane::Type::ADDR)); // ADDR and CMD are interchangable terms

    ASSERT_TRUE(Repair::GetLaneFromString("I1_DATA", &lane));
    EXPECT_EQ(lane, FbpaLane(HwFbpa::FromLetter('I'), 1, FbpaLane::Type::DATA));

    ASSERT_TRUE(Repair::GetLaneFromString("J000000034214_DATA", &lane));
    EXPECT_EQ(lane, FbpaLane(HwFbpa::FromLetter('J'), 34214, FbpaLane::Type::DATA));

    ASSERT_TRUE(Repair::GetLaneFromString("K100000_DATA", &lane));
    EXPECT_EQ(lane, FbpaLane(HwFbpa::FromLetter('K'), 100000, FbpaLane::Type::DATA));

    // Case changes
    ASSERT_TRUE(Repair::GetLaneFromString("A000_Data", &lane));
    EXPECT_EQ(lane, FbpaLane(HwFbpa::FromLetter('A'), 0, FbpaLane::Type::DATA));

    ASSERT_TRUE(Repair::GetLaneFromString("a255_data", &lane));
    EXPECT_EQ(lane, FbpaLane(HwFbpa::FromLetter('A'), 255, FbpaLane::Type::DATA));

    ASSERT_TRUE(Repair::GetLaneFromString("z000_DAtA", &lane));
    EXPECT_EQ(lane, FbpaLane(HwFbpa::FromLetter('Z'), 0, FbpaLane::Type::DATA));

    // Negative cases
    //
    EXPECT_FALSE(Repair::GetLaneFromString("skldjflsakdjfrkelw", &lane));
    EXPECT_FALSE(Repair::GetLaneFromString("AA000_DATA", &lane));
    EXPECT_FALSE(Repair::GetLaneFromString("AA000_DATE", &lane));
    EXPECT_FALSE(Repair::GetLaneFromString("A000_DATe", &lane));
    EXPECT_FALSE(Repair::GetLaneFromString("A000DATA", &lane));
    EXPECT_FALSE(Repair::GetLaneFromString("A000 DATA", &lane));
    EXPECT_FALSE(Repair::GetLaneFromString("A000 DATA", &lane));
    EXPECT_FALSE(Repair::GetLaneFromString("A000_DATAdflsd", &lane));
    EXPECT_FALSE(Repair::GetLaneFromString("A000_DATA dkfjsld", &lane));
    EXPECT_FALSE(Repair::GetLaneFromString("A000_DATA ", &lane));
    EXPECT_FALSE(Repair::GetLaneFromString(" A000_DATA", &lane));
    EXPECT_FALSE(Repair::GetLaneFromString("sjdfhsdf A000_DATA", &lane));
    EXPECT_FALSE(Repair::GetLaneFromString("askdlj_kasjda", &lane));
}

//!
//! \brief Test GetLanes when the string represents a lane description.
//!
TEST(RepairUtilTest, GetLanes_SingleLaneDesc)
{
    vector<FbpaLane> lanes;
    lanes.reserve(1);

    // Positive cases
    EXPECT_EQ(Repair::GetLanes("A000_DATA", &lanes), RC::OK);
    EXPECT_EQ(lanes.size(), 1ULL);
    EXPECT_EQ(lanes.back(), FbpaLane(HwFbpa::FromLetter('A'), 0, FbpaLane::Type::DATA));
    lanes.clear();

    // Negative cases
    EXPECT_NE(Repair::GetLanes("Aksdj_kdjfs", &lanes), RC::OK);
    EXPECT_TRUE(lanes.empty());
}

//!
//! \brief Test GetLanes when the string represents a path to a file containing
//! a series of lane descriptions.
//!
TEST(RepairUtilTest, DISABLED_GetLanes_FromFile)
{
    // TODO(sdsmith):
}

TEST(RepairUtilTest, DISABLED_GetRowsFromFileFormat)
{
    // TODO(sdsmith):
}

TEST(RepairUtilTest, DISABLED_GetRows)
{
    // TODO(sdsmith):
}
