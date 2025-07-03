/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/fuse/downbin/downbinelementpickers.h"
#include "gpu/fuse/downbin/downbingrouppickers.h"
#include "gpu/fuse/downbin/fsset.h"

#include "mockfsset.h"

#include "gtest/gtest.h"

using namespace Downbin;

//------------------------------------------------------------------------------
TEST(DownbinFsSet, DisableOneGroup)
{
    // No TPCs disabled
    MockFsSet gpcSet1(FsUnit::GPC, 2);
    gpcSet1.AddElement(FsUnit::TPC, 4, true, false);
    unique_ptr<DownbinGroupPicker> pPicker1 = make_unique<MostDisabledGroupPicker>(&gpcSet1, FsUnit::TPC);
    gpcSet1.AddGroupDisablePicker(std::move(pPicker1));
    EXPECT_EQ(gpcSet1.DisableOneGroup(), RC::OK);
    EXPECT_EQ(gpcSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x1U);

    // 1 TPC in GPC1 disabled
    MockFsSet gpcSet2(FsUnit::GPC, 2);
    gpcSet2.AddElement(FsUnit::TPC, 4, true, false);
    unique_ptr<DownbinGroupPicker> pPicker2 = make_unique<MostDisabledGroupPicker>(&gpcSet2, FsUnit::TPC);
    gpcSet2.AddGroupDisablePicker(std::move(pPicker2));
    gpcSet2.DisableElements(FsUnit::TPC, 1, 0x1);
    EXPECT_EQ(gpcSet2.DisableOneGroup(), RC::OK);
    EXPECT_EQ(gpcSet2.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x2U);

    // 1 TPC in GPC0 and 2 TPCs in GPC1 disabled
    MockFsSet gpcSet3(FsUnit::GPC, 2);
    gpcSet3.AddElement(FsUnit::TPC, 4, true, false);
    unique_ptr<DownbinGroupPicker> pPicker3 = make_unique<MostDisabledGroupPicker>(&gpcSet3, FsUnit::TPC);
    gpcSet3.AddGroupDisablePicker(std::move(pPicker3));
    gpcSet3.DisableElements(FsUnit::TPC, 0, 0x1);
    gpcSet3.DisableElements(FsUnit::TPC, 1, 0x3);
    EXPECT_EQ(gpcSet3.DisableOneGroup(), RC::OK);
    EXPECT_EQ(gpcSet3.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x2U);

    // GPC0 disabled
    MockFsSet gpcSet4(FsUnit::GPC, 2);
    gpcSet4.AddElement(FsUnit::TPC, 4, true, false);
    unique_ptr<DownbinGroupPicker> pPicker4 = make_unique<MostDisabledGroupPicker>(&gpcSet4, FsUnit::TPC);
    gpcSet4.AddGroupDisablePicker(std::move(pPicker4));
    gpcSet4.DisableGroups(0x1);
    EXPECT_EQ(gpcSet4.DisableOneGroup(), RC::OK);
    EXPECT_EQ(gpcSet4.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x3U);

    // GPC0 + GPC1 disabled
    MockFsSet gpcSet5(FsUnit::GPC, 2);
    gpcSet5.AddElement(FsUnit::TPC, 4, true, false);
    unique_ptr<DownbinGroupPicker> pPicker5 = make_unique<MostDisabledGroupPicker>(&gpcSet5, FsUnit::TPC);
    gpcSet5.AddGroupDisablePicker(std::move(pPicker5));
    gpcSet5.DisableGroups(0x3);
    EXPECT_EQ(gpcSet5.DisableOneGroup(), RC::CANNOT_MEET_FS_REQUIREMENTS);

    // No group disable pickers
    MockFsSet gpcSet6(FsUnit::GPC, 2);
    gpcSet6.AddElement(FsUnit::TPC, 4, true, false);
    EXPECT_EQ(gpcSet6.DisableOneGroup(), RC::OK);
    EXPECT_EQ(gpcSet6.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x1U);

    // 2 group disable pickers
    // TODO
}

//------------------------------------------------------------------------------
TEST(DownbinFsSet, DisableOneElement)
{
    // No TPCs disabled
    MockFsSet gpcSet1(FsUnit::GPC, 2);
    gpcSet1.AddElement(FsUnit::TPC, 4, true, false);
    gpcSet1.AddElement(FsUnit::PES, 2, false, false);
    unique_ptr<DownbinGroupPicker> pGrpPicker1 =
        make_unique<MostAvailableGroupPicker>(&gpcSet1, FsUnit::TPC, true, false);
    unique_ptr<DownbinElementPicker> pElPicker1 = make_unique<MostEnabledSubElementPicker>(&gpcSet1,
                                                                            FsUnit::PES, FsUnit::TPC);
    gpcSet1.AddGroupReducePicker(FsUnit::TPC, std::move(pGrpPicker1));
    gpcSet1.AddElementDisablePicker(FsUnit::TPC, std::move(pElPicker1));
    EXPECT_EQ(gpcSet1.DisableOneElement(FsUnit::TPC), RC::OK);
    EXPECT_EQ(gpcSet1.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet1.GetElementSet(1, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x0U);

    // 1 TPC in GPC0 disabled
    MockFsSet gpcSet2(FsUnit::GPC, 2);
    gpcSet2.AddElement(FsUnit::TPC, 4, true, false);
    gpcSet2.AddElement(FsUnit::PES, 2, false, false);
    unique_ptr<DownbinGroupPicker> pGrpPicker2 =
        make_unique<MostAvailableGroupPicker>(&gpcSet2, FsUnit::TPC, true, false);
    unique_ptr<DownbinElementPicker> pElPicker2 = make_unique<MostEnabledSubElementPicker>(&gpcSet2,
                                                                            FsUnit::PES, FsUnit::TPC);
    gpcSet2.AddGroupReducePicker(FsUnit::TPC, std::move(pGrpPicker2));
    gpcSet2.AddElementDisablePicker(FsUnit::TPC, std::move(pElPicker2));
    gpcSet2.DisableElements(FsUnit::TPC, 0, 0x8);
    EXPECT_EQ(gpcSet2.DisableOneElement(FsUnit::TPC), RC::OK);
    EXPECT_EQ(gpcSet2.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x8U);
    EXPECT_EQ(gpcSet2.GetElementSet(1, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);

    // 1 TPC in GPC0 and 2 TPCs in GPC1 disabled
    MockFsSet gpcSet3(FsUnit::GPC, 2);
    gpcSet3.AddElement(FsUnit::TPC, 4, true, false);
    gpcSet3.AddElement(FsUnit::PES, 2, false, false);
    unique_ptr<DownbinGroupPicker> pGrpPicker3 =
        make_unique<MostAvailableGroupPicker>(&gpcSet3, FsUnit::TPC, true, false);
    unique_ptr<DownbinElementPicker> pElPicker3 = make_unique<MostEnabledSubElementPicker>(&gpcSet3,
                                                                            FsUnit::PES, FsUnit::TPC);
    gpcSet3.AddGroupReducePicker(FsUnit::TPC, std::move(pGrpPicker3));
    gpcSet3.AddElementDisablePicker(FsUnit::TPC, std::move(pElPicker3));
    gpcSet3.DisableElements(FsUnit::TPC, 0, 0x1);
    gpcSet3.DisableElements(FsUnit::TPC, 1, 0x3);
    EXPECT_EQ(gpcSet3.DisableOneElement(FsUnit::TPC), RC::OK);
    EXPECT_EQ(gpcSet3.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x5U);
    EXPECT_EQ(gpcSet3.GetElementSet(1, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x3U);

    // 1 GPC disabled
    MockFsSet gpcSet4(FsUnit::GPC, 2);
    gpcSet4.AddElement(FsUnit::TPC, 4, true, false);
    gpcSet4.AddElement(FsUnit::PES, 2, false, false);
    unique_ptr<DownbinGroupPicker> pGrpPicker4 =
        make_unique<MostAvailableGroupPicker>(&gpcSet4, FsUnit::TPC, true, false);
    unique_ptr<DownbinElementPicker> pElPicker4 = make_unique<MostEnabledSubElementPicker>(&gpcSet4,
                                                                            FsUnit::PES, FsUnit::TPC);
    gpcSet4.AddGroupReducePicker(FsUnit::TPC, std::move(pGrpPicker4));
    gpcSet4.AddElementDisablePicker(FsUnit::TPC, std::move(pElPicker4));
    gpcSet4.DisableGroups(0x1);
    EXPECT_EQ(gpcSet4.DisableOneElement(FsUnit::TPC), RC::OK);
    EXPECT_EQ(gpcSet4.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet4.GetElementSet(1, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);

    // No group reduce pickers, element disable pickers
    MockFsSet gpcSet5(FsUnit::GPC, 2);
    gpcSet5.AddElement(FsUnit::TPC, 4, true, false);
    EXPECT_EQ(gpcSet5.DisableOneElement(FsUnit::TPC), RC::OK);
    EXPECT_EQ(gpcSet5.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet5.GetElementSet(1, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x0U);

    // 2 group reduce pickers
    // TODO

    // 2 element disable pickers
    // TODO
}
