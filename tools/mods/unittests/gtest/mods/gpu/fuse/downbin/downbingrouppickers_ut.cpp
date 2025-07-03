/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/fuse/downbin/downbingrouppickers.h"
#include "gpu/fuse/downbin/fsset.h"

#include "gpu/include/gpusbdev.h"
#include "gpu/ga100gpu.h"

#include "gtest/gtest.h"

#include "mockfsset.h"
#include "mocksubdevice.h"

using namespace Downbin;

//------------------------------------------------------------------------------
TEST(DownbinGroupPickerTest, MostDisabledGroupPicker)
{
    FsMask inputMask1, outputMask;

    // 3 GPCs, Input Mask = 0x7
    MockFsSet gpcSet1(FsUnit::GPC, 3);
    gpcSet1.AddElement(FsUnit::TPC, 3, true, false);
    MostDisabledGroupPicker picker1(&gpcSet1, FsUnit::TPC);
    inputMask1 = 0x7U;
    // No TPCs disabled
    EXPECT_EQ(picker1.Pick(inputMask1, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x7U);
    // Disable 1 TPC from GPC 0
    gpcSet1.DisableElements(FsUnit::TPC, 0, 0x1U);
    EXPECT_EQ(picker1.Pick(inputMask1, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x1U);
    // Disable 1 TPC from GPC 0 + GPC 1
    gpcSet1.DisableElements(FsUnit::TPC, 1, 0x1U);
    EXPECT_EQ(picker1.Pick(inputMask1, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x3U);
    // Disable 1 TPC from GPC 0 + GPC 1 + GPC 2
    gpcSet1.DisableElements(FsUnit::TPC, 2, 0x1U);
    EXPECT_EQ(picker1.Pick(inputMask1, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x7U);
    // Disable 1 TPC from GPC 0, 2 + 2 TPCs from GPC1
    gpcSet1.DisableElements(FsUnit::TPC, 1, 0x3U);
    EXPECT_EQ(picker1.Pick(inputMask1, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x2U);

    // 3 GPCs, Input Mask = 0x5
    MockFsSet gpcSet2(FsUnit::GPC, 3);
    gpcSet2.AddElement(FsUnit::TPC, 3, true, false);
    MostDisabledGroupPicker picker2(&gpcSet2, FsUnit::TPC);
    FsMask inputMask2;
    inputMask2 = 0x5U; // allowed due to overloaded = operator
    // No TPCs disabled
    EXPECT_EQ(picker2.Pick(inputMask2, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x5U);
    // Disable 1 TPC from GPC 0
    gpcSet2.DisableElements(FsUnit::TPC, 0, 0x1U);
    EXPECT_EQ(picker2.Pick(inputMask2, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x1U);
    // Disable 1 TPC from GPC 0 + GPC 1
    gpcSet2.DisableElements(FsUnit::TPC, 1, 0x1U);
    EXPECT_EQ(picker2.Pick(inputMask2, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x1U);
    // Disable 1 TPC from GPC 0 + GPC 1 + GPC 2
    gpcSet2.DisableElements(FsUnit::TPC, 2, 0x1U);
    EXPECT_EQ(picker2.Pick(inputMask2, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x5U);
    // Disable 1 TPC from GPC 0, 2 + 2 TPCs from GPC1
    gpcSet2.DisableElements(FsUnit::TPC, 1, 0x3U);
    EXPECT_EQ(picker2.Pick(inputMask2, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x5U);

    // 3 GPCs, Input Mask = 0xF
    MockFsSet gpcSet3(FsUnit::GPC, 3);
    gpcSet3.AddElement(FsUnit::TPC, 3, true, false);
    MostDisabledGroupPicker picker3(&gpcSet3, FsUnit::TPC);
    FsMask inputMask3;
    inputMask3 = 0xFU;
    // No TPCs disabled
    EXPECT_EQ(picker3.Pick(inputMask3, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x7U);
    // Disable 1 TPC from GPC 0
    gpcSet3.DisableElements(FsUnit::TPC, 0, 0x1U);
    EXPECT_EQ(picker3.Pick(inputMask3, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x1U);
}

//------------------------------------------------------------------------------
TEST(DownbinGroupPickerTest, MostEnabledUgpuPickerByGPCCount)
{
    MockFsSet gpcSet1(FsUnit::GPC, 8);
    gpcSet1.AddElement(FsUnit::TPC, 8, true, false);
    MockGpuSubdevice mockDev;
    // Setting ugpu level expectations
    const vector<UINT32> ugpuGpcMasks = { 0xC3, 0x3C }; 
    mockDev.SetMaxUGpuCount(2);
    mockDev.SetUGpuEnableMask(0x3);
    mockDev.SetUGpuUnitMasks(ugpuGpcMasks);
    MostEnabledUgpuPicker picker1(mockDev.GetDevice(), &gpcSet1, FsUnit::GPC);
    FsMask inputMask, outputMask;

    // No GPC disabled
    inputMask = 0xFF;
    EXPECT_EQ(picker1.Pick(inputMask, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0xFF);

    // Disable 1 GPC from UGPU0
    EXPECT_EQ(gpcSet1.DisableGroup(0), RC::OK); // GPC0 from UGPU0
    inputMask = 0xFE;
    EXPECT_EQ(picker1.Pick(inputMask, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x3C);

    // Disable 1 GPC from UGPU0 + UGPU1
    EXPECT_EQ(gpcSet1.DisableGroup(5), RC::OK); // GPC5 From UGPU1
    inputMask = 0xDE;
    EXPECT_EQ(picker1.Pick(inputMask, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0xDE);

    // Disable 2 GPCS from UGPU0, 1 GPC from UGPU1
    EXPECT_EQ(gpcSet1.DisableGroup(7), RC::OK);
    inputMask = 0x5E;
    EXPECT_EQ(picker1.Pick(inputMask, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x1C);
 
    // Disable 2 GPCS from UGPU0, 2 GPC from UGPU1
    EXPECT_EQ(gpcSet1.DisableGroup(2), RC::OK);
    inputMask = 0x5A;
    EXPECT_EQ(picker1.Pick(inputMask, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x5A);

    // Disable 3 GPCS from UGPU0, 2 GPC from UGPU1
    EXPECT_EQ(gpcSet1.DisableGroup(3), RC::OK);
    inputMask = 0x54;
    EXPECT_EQ(picker1.Pick(inputMask, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x14);

    MockFsSet gpcSet2(FsUnit::GPC, 8);
    gpcSet2.AddElement(FsUnit::TPC, 8, true, false);
    // Disable UGPU0 fully, 1 GPC frin UGPU1
    EXPECT_EQ(gpcSet2.DisableGroup(0), RC::OK);
    EXPECT_EQ(gpcSet2.DisableGroup(1), RC::OK);
    EXPECT_EQ(gpcSet2.DisableGroup(6), RC::OK);
    EXPECT_EQ(gpcSet2.DisableGroup(7), RC::OK);
    EXPECT_EQ(gpcSet2.DisableGroup(5), RC::OK);
    MostEnabledUgpuPicker picker2(mockDev.GetDevice(), &gpcSet2, FsUnit::GPC);
    inputMask = 0x1C;
    EXPECT_EQ(picker2.Pick(inputMask, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x1C);

    // Only UGPU0 enabled
    mockDev.SetUGpuEnableMask(0x1);
    MockFsSet gpcSet3(FsUnit::GPC, 8);
    gpcSet3.AddElement(FsUnit::TPC, 8, true, false);
    MostEnabledUgpuPicker picker3(mockDev.GetDevice(), &gpcSet3, FsUnit::GPC);
    inputMask = 0xC2;
    EXPECT_EQ(picker3.Pick(inputMask, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0xC2);
    
    // Only UGPU1 enabled
    mockDev.SetUGpuEnableMask(0x2);
    MockFsSet gpcSet4(FsUnit::GPC, 8);
    gpcSet4.AddElement(FsUnit::TPC, 8, true, false);
    MostEnabledUgpuPicker picker4(mockDev.GetDevice(), &gpcSet4, FsUnit::GPC);
    inputMask = 0x2C;
    EXPECT_EQ(picker4.Pick(inputMask, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x2C);
}

//----------------------------------------------------------------
TEST(DownbinGroupPickerTest, MostEnabledUgpuPickerByTPCCount)
{
    MockFsSet gpcSet1(FsUnit::GPC, 8);
    gpcSet1.AddElement(FsUnit::TPC, 8, true, false);
    MockGpuSubdevice mockDev;

    // Setting ugpu level expectations
    const vector<UINT32> ugpuGpcMasks = { 0xC3, 0x3C }; 
    mockDev.SetMaxUGpuCount(2);
    mockDev.SetUGpuEnableMask(0x3);
    mockDev.SetUGpuUnitMasks(ugpuGpcMasks);
    MostEnabledUgpuPicker picker1(mockDev.GetDevice(), &gpcSet1, FsUnit::TPC);
    FsMask inputMask, outputMask;

    // No GPC or TPC disabled
    inputMask = 0xFF;
    EXPECT_EQ(picker1.Pick(inputMask, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0xFF);

    // Disable 1 TPC from GPC0 of UGPU0
    EXPECT_EQ(gpcSet1.DisableElements(FsUnit::TPC, 0, 0x1U), RC::OK);
    EXPECT_EQ(picker1.Pick(inputMask, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x3C);
   
    // Disable 1 TPC from GPC 0 + GPC 1 of UGPU0
    EXPECT_EQ(gpcSet1.DisableElements(FsUnit::TPC, 1, 0x1U), RC::OK);
    EXPECT_EQ(picker1.Pick(inputMask, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x3C);
 
    // Disable 1 TPC from GPC 0 + GPC 1 of UGPU0, 2 TPC from UGPU1
    EXPECT_EQ(gpcSet1.DisableElements(FsUnit::TPC, 2, 0x3U), RC::OK);
    EXPECT_EQ(picker1.Pick(inputMask, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0xFF); 

    // Disable 2 TPC from GPC 0 + GPC 1 of UGPU0, 2 TPC from UGPU1
    EXPECT_EQ(gpcSet1.DisableElements(FsUnit::TPC, 0, 0x2U), RC::OK);
    EXPECT_EQ(picker1.Pick(inputMask, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x3C);

    // Disable 3 TPC UGPU0, 2 TPC + 1 GPC from UGPU1
    EXPECT_EQ(gpcSet1.DisableGroup(2), RC::OK);
    inputMask = 0xFB;
    EXPECT_EQ(picker1.Pick(inputMask, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0xC3); 
    
    // Disable 3TPC + 5 TPC from UGPU0, 1GPC from UGPU1
    EXPECT_EQ(gpcSet1.DisableElements(FsUnit::TPC, 7, 0x1FU), RC::OK);
    EXPECT_EQ(picker1.Pick(inputMask, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0xFB);

    // Disable 3+8 TPC from UGPU0, 1 GPC from UGPU1
    EXPECT_EQ(gpcSet1.DisableElements(FsUnit::TPC, 7, 0xE0U), RC::OK);
    inputMask = 0x7B; // adjusted for GPC7 with all TPCs disabled
    EXPECT_EQ(picker1.Pick(inputMask, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x38);

    // Disable 11 TPCs in UGPU0, 2 GPCs in UGPU1
    EXPECT_EQ(gpcSet1.DisableGroup(3), RC::OK);
    inputMask = 0x73;
    EXPECT_EQ(picker1.Pick(inputMask, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x43);
    
    // Disable 16 TPCs in UGPU0, 2 GPCs in UGPU1
    EXPECT_EQ(gpcSet1.DisableElements(FsUnit::TPC, 1, 0x3EU), RC::OK); // TPC0 disabled already
    EXPECT_EQ(picker1.Pick(inputMask, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x73);

    // Disable 17 TPCs in UGPU0, 2 GPCs in UGPU1
    EXPECT_EQ(gpcSet1.DisableElements(FsUnit::TPC, 0, 0x4U), RC::OK);// TPC0, 1 disabled already
    EXPECT_EQ(picker1.Pick(inputMask, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x30);

    // Disable 17 TPC from UGPU0, UGPU1 fully disabled
    EXPECT_EQ(gpcSet1.DisableGroup(4), RC::OK);
    EXPECT_EQ(gpcSet1.DisableGroup(5), RC::OK);
    inputMask = 0x43;
    EXPECT_EQ(picker1.Pick(inputMask, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x43);

    // Only UGPU0 enabled
    mockDev.SetUGpuEnableMask(0x1);
    MockFsSet gpcSet2(FsUnit::GPC, 8);
    gpcSet2.AddElement(FsUnit::TPC, 8, true, false);
    MostEnabledUgpuPicker picker2(mockDev.GetDevice(), &gpcSet2, FsUnit::TPC);
    inputMask = 0xC3;
    EXPECT_EQ(picker2.Pick(inputMask, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0xC3);
    
    // Only UGPU1 enabled
    mockDev.SetUGpuEnableMask(0x2);
    MockFsSet gpcSet3(FsUnit::GPC, 8);
    gpcSet3.AddElement(FsUnit::TPC, 8, true, false);
    MostEnabledUgpuPicker picker3(mockDev.GetDevice(), &gpcSet3, FsUnit::TPC);
    inputMask = 0x3C;
    EXPECT_EQ(picker3.Pick(inputMask, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x3C);
}

//------------------------------------------------------------------------------
TEST(DownbinGroupPickerTest, MostAvailableGroupPickerCheckDisabled)
{
    FsMask outputMask;

    // 3 GPCs, Input Mask = 0x7
    MockFsSet gpcSet1(FsUnit::GPC, 3);
    gpcSet1.AddElement(FsUnit::TPC, 3, true, false);
    MostAvailableGroupPicker picker0(&gpcSet1, FsUnit::TPC, false, false);
    FsMask inputMask1;
    inputMask1 = 0x7U;

    // Neither checking disabled nor protected elements - error out
    EXPECT_EQ(picker0.Pick(inputMask1, &outputMask), RC::BAD_PARAMETER);

    MostAvailableGroupPicker picker1(&gpcSet1, FsUnit::TPC, true, false);
    // No TPCs disabled
    EXPECT_EQ(picker1.Pick(inputMask1, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x7U);
    // Disable 1 TPC from GPC 0
    gpcSet1.DisableElements(FsUnit::TPC, 0, 0x1U);
    EXPECT_EQ(picker1.Pick(inputMask1, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x6U);
    // Disable 1 TPC from GPC 0 + GPC 1
    gpcSet1.DisableElements(FsUnit::TPC, 1, 0x1U);
    EXPECT_EQ(picker1.Pick(inputMask1, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x4U);
    // Disable 1 TPC from GPC 0 + GPC 1 + GPC 2
    gpcSet1.DisableElements(FsUnit::TPC, 2, 0x1U);
    EXPECT_EQ(picker1.Pick(inputMask1, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x7U);
    // Disable 1 TPC from GPC 0, 2 + 2 TPCs from GPC1
    gpcSet1.DisableElements(FsUnit::TPC, 1, 0x3U);
    EXPECT_EQ(picker1.Pick(inputMask1, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x5U);

    // 3 GPCs, Input Mask = 0x5
    MockFsSet gpcSet2(FsUnit::GPC, 3);
    gpcSet2.AddElement(FsUnit::TPC, 3, true, false);
    MostAvailableGroupPicker picker2(&gpcSet2, FsUnit::TPC, true, false);
    FsMask inputMask2;
    inputMask2 = 0x5U;
    // No TPCs disabled
    EXPECT_EQ(picker2.Pick(inputMask2, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x5U);
    // Disable 1 TPC from GPC 0
    gpcSet2.DisableElements(FsUnit::TPC, 0, 0x1U);
    EXPECT_EQ(picker2.Pick(inputMask2, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x4U);
    // Disable 1 TPC from GPC 0 + GPC 1
    gpcSet2.DisableElements(FsUnit::TPC, 1, 0x1U);
    EXPECT_EQ(picker2.Pick(inputMask2, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x4U);
    // Disable 1 TPC from GPC 0 + GPC 1 + GPC 2
    gpcSet2.DisableElements(FsUnit::TPC, 2, 0x1U);
    EXPECT_EQ(picker2.Pick(inputMask2, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x5U);
    // Disable 1 TPC from GPC 0, 2 + 2 TPCs from GPC1
    gpcSet2.DisableElements(FsUnit::TPC, 1, 0x3U);
    EXPECT_EQ(picker2.Pick(inputMask2, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x5U);

    // 3 GPCs, Input Mask = 0xF
    MockFsSet gpcSet3(FsUnit::GPC, 3);
    gpcSet3.AddElement(FsUnit::TPC, 3, true, false);
    MostAvailableGroupPicker picker3(&gpcSet3, FsUnit::TPC, true, false);
    FsMask inputMask3;
    inputMask3 = 0xFU;
    // No TPCs disabled
    EXPECT_EQ(picker3.Pick(inputMask3, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x7U);
    // Disable 1 TPC from GPC 0
    gpcSet3.DisableElements(FsUnit::TPC, 0, 0x1U);
    EXPECT_EQ(picker3.Pick(inputMask3, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x6U);
}

//------------------------------------------------------------------------------
TEST(DownbinGroupPickerTest, MostAvailableGroupPickerCheckProtected)
{
    FsMask outputMask;

    // CASE1: no element fs unit specified
    // 3 GPCs, Input Mask = 0x7
    MockFsSet gpcSet1(FsUnit::GPC, 3);
    gpcSet1.AddElement(FsUnit::TPC, 3, true, false);

    MostAvailableGroupPicker picker1(&gpcSet1, FsUnit::NONE, false, false);
    FsMask inputMask1;
    inputMask1 = 0x7U;
    // No GPC/TPCs marked protected
    EXPECT_EQ(picker1.Pick(inputMask1, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x7U);
    // 1 TPC from GPC0 protected
    gpcSet1.GetGroup(0)->GetElementSet(FsUnit::TPC)->MarkProtected(1);
    EXPECT_EQ(picker1.Pick(inputMask1, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x6U);
    // 1 TPC from GPC0, GPC 1 protected
    gpcSet1.GetGroup(1)->MarkProtected();
    EXPECT_EQ(picker1.Pick(inputMask1, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x4U);
    // 1 TPC from GPC0/1 TPC from GPC 2, GPC 1 protected
    gpcSet1.GetGroup(2)->GetElementSet(FsUnit::TPC)->MarkProtected(2);
    EXPECT_EQ(picker1.Pick(inputMask1, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x7U);

    // 4 GPCs, Input Mask = 0xF
    MockFsSet gpcSet1_1(FsUnit::GPC, 4);
    gpcSet1_1.AddElement(FsUnit::TPC, 4, true, false);
    gpcSet1_1.AddElement(FsUnit::PES, 2, false, false);
    MostAvailableGroupPicker picker1_1(&gpcSet1_1, FsUnit::NONE, false, false);
    FsMask inputMask1_1;
    inputMask1_1 = 0xF;
    // 1 TPC from GPC0/1, PES from GPC 2 protected
    gpcSet1_1.GetGroup(0)->GetElementSet(FsUnit::TPC)->MarkProtected(1);
    gpcSet1_1.GetGroup(2)->GetElementSet(FsUnit::PES)->MarkProtected(1);
    EXPECT_EQ(picker1_1.Pick(inputMask1_1, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0xAU);
    // 1 TPC from GPC0/1, PES from GPC 2 protected, GPC1 protected
    gpcSet1_1.GetGroup(1)->MarkProtected();
    EXPECT_EQ(picker1_1.Pick(inputMask1_1, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x8U);

    // CASE 2: TPC specified as FS unit
    // 4 GPCs, Input Mask = 0xB
    MockFsSet gpcSet2(FsUnit::GPC, 4);
    gpcSet2.AddElement(FsUnit::TPC, 4, true, false);

    MostAvailableGroupPicker picker2(&gpcSet2, FsUnit::TPC, false, true);
    FsMask inputMask2;
    inputMask2 = 0xB;
    // No TPCs disabled
    EXPECT_EQ(picker2.Pick(inputMask2, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0xB);
    // 1 TPC from GPC 0 protected
    gpcSet2.GetGroup(0)->GetElementSet(FsUnit::TPC)->MarkProtected(3);
    EXPECT_EQ(picker2.Pick(inputMask2, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0xAU);
    // 1 TPC from GPC 0, 1 TPC from GPC 1 protected
    gpcSet2.GetGroup(1)->GetElementSet(FsUnit::TPC)->MarkProtected(0);
    EXPECT_EQ(picker2.Pick(inputMask2, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x8U);
    // 1 TPC each from GPC 0, GPC 1, GPC 2 protected
    gpcSet2.GetGroup(2)->GetElementSet(FsUnit::TPC)->MarkProtected(1);
    EXPECT_EQ(picker2.Pick(inputMask2, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x8U);
    // 1 TPC each from GPC 0/1/2/3 protected
    gpcSet2.GetGroup(3)->GetElementSet(FsUnit::TPC)->MarkProtected(1);
    EXPECT_EQ(picker2.Pick(inputMask2, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0xBU);
    // 2 TPC from GPC1, 1 TPC from GPC0/2/3 protected
    gpcSet2.GetGroup(1)->GetElementSet(FsUnit::TPC)->MarkProtected(1);
    EXPECT_EQ(picker2.Pick(inputMask2, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x9U);

    // CASE 3: CASE 2 + m_CheckDisabled = true
    // 4 GPCs, Input Mask = 0xF
    MockFsSet gpcSet3(FsUnit::GPC, 4);
    gpcSet3.AddElement(FsUnit::TPC, 3, true, false);
    MostAvailableGroupPicker picker3(&gpcSet3, FsUnit::TPC, true, true);
    FsMask inputMask3;
    inputMask3 = 0xFU;
    // Disable 1 TPC from GPC 0, protect 1 TPC from GPC2
    gpcSet3.DisableElements(FsUnit::TPC, 0, 0x1U);
    gpcSet3.GetGroup(2)->GetElementSet(FsUnit::TPC)->MarkProtected(1);
    EXPECT_EQ(picker3.Pick(inputMask3, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0xAU);
    // Disable 1 TPC from GPC 0, protect 1 TPC from GPC2/3
    gpcSet3.GetGroup(3)->GetElementSet(FsUnit::TPC)->MarkProtected(1);
    EXPECT_EQ(picker3.Pick(inputMask3, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x2U); 
    // Disable 1 TPC from GPC 0/1, protect 1 TPC from GPC2/3
    gpcSet3.DisableElements(FsUnit::TPC, 1, 0x1U); 
    EXPECT_EQ(picker3.Pick(inputMask3, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0xFU);
    // Disable 1 TPC from GPC 0, 2 TPC from GPC1, protect 1 TPC from GPC2/3
    gpcSet3.DisableElements(FsUnit::TPC, 1, 0x2U);
    EXPECT_EQ(picker3.Pick(inputMask3, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0xDU);
    // Disable 1 TPC from GPC 0/2, 2 TPC from GPC1, protect 1 TPC from GPC2/3
    gpcSet3.DisableElements(FsUnit::TPC, 2, 0x1U);
    EXPECT_EQ(picker3.Pick(inputMask3, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x9U);
}

//------------------------------------------------------------------------------
TEST(DownbinGroupPickerTest, VGpcSkylineGroupPicker)
{
    FsMask inputMask1, outputMask;

    // Initial NumTpcs per GPC is: {9, 9, 9, 9, 9, 9, 9, 9}
    MockFsSet gpcSet1(FsUnit::GPC, 8);
    gpcSet1.AddElement(FsUnit::TPC, 9, true, false);

    FuseUtil::DownbinConfig mockTpcConfig;
    mockTpcConfig.vGpcSkylineList = {8,8,8,9,9,9,9};
    mockTpcConfig.minEnablePerGroup = 4;

    VGpcSkylineGroupPicker picker1(&gpcSet1, mockTpcConfig);
    //  input Mask is 1 1 1 1 1 1 1 0
    inputMask1 = 0xFEU;

    // NumTpcs per GPC is: {9, 9, 9, 9, 9, 9, 9, 9}
    EXPECT_EQ(picker1.Pick(inputMask1, &outputMask), RC::OK);
    // Any GPC can be chosen, except GPC0 which is not set in the inputMask
    // outputMask should be 1 1 1 1 1 1 1 0=> 0xFEU
    EXPECT_EQ(outputMask, 0xFEU);
    outputMask.ClearAll();

    // Disable GPC1-TPC0,GPC1-TPC1, GPC1-TPC2, GPC1-TPC3, GPC1-TPC4, NumTpcs per GPC is: {9, 4, 9, 9, 9, 9, 9, 9}
    gpcSet1.DisableElements(FsUnit::TPC, 1, 0x1FU);
    EXPECT_EQ(picker1.Pick(inputMask1, &outputMask), RC::OK);
    // outputMask should be 1 1 1 1 1 1 0 0 => 0xFLW
    // GPC0 can not be chosen, because it's not set in the inputMask
    // GPC1 can not be chosen, because it will be less than the minTpcPerGpc, which is 4
    EXPECT_EQ(outputMask, 0xFLW);
    outputMask.ClearAll();

    // Continue to disable GPC2-TPC0, GPC3-TPC0, GPC4-TPC0 NumTpcs per GPC is: {9, 4, 8, 8, 8, 9, 9, 9}
    gpcSet1.DisableElements(FsUnit::TPC, 2, 0x1U);
    gpcSet1.DisableElements(FsUnit::TPC, 3, 0x1U);
    gpcSet1.DisableElements(FsUnit::TPC, 4, 0x1U);
    EXPECT_EQ(picker1.Pick(inputMask1, &outputMask), RC::OK);
    // outputMask should be 0 0 0 0 0 0 0 0 => 0x0U
    EXPECT_EQ(outputMask, 0x0U);
}

//------------------------------------------------------------------------------
TEST(DownbinGroupPickerTest, CrossFsSetDependencyGroupPicker)
{
    FsMask inputMask1, outputMask;

    MockFsSet lwdecSet1(FsUnit::LWDEC, 8);
    MockFsSet lwjpgSet1(FsUnit::LWJPG, 8);

    CrossFsSetDependencyGroupPicker lwdecPicker(&lwdecSet1, &lwjpgSet1);
    CrossFsSetDependencyGroupPicker lwjpgPicker(&lwjpgSet1, &lwdecSet1);

    // lwdec enabled mask is {1, 1, 1, 1, 1, 1, 0, 1}
    lwdecSet1.DisableGroup(1);
    // lwdec enabled mask is {1, 1, 1, 1, 1, 0, 1, 1}
    lwjpgSet1.DisableGroup(2);

    //  input Mask is 1 1 1 1 1 1 1 0
    inputMask1 = 0xFEU;

    EXPECT_EQ(lwdecPicker.Pick(inputMask1, &outputMask), RC::OK);
    // expect output Mask is {1, 1, 1, 1, 1, 0, 0, 0} => 0xF8U
    // lwdec 0 can not be chosen, because it's not set in the input mask
    // lwdec 1 can not be chosen, because it has been disabled above
    // lwdec 2 can not be chosen, because lwjpg 2 has been disabled above
    EXPECT_EQ(outputMask, 0xF8U);
    outputMask.ClearAll();

    EXPECT_EQ(lwjpgPicker.Pick(inputMask1, &outputMask), RC::OK);
    // expect output Mask is {1, 1, 1, 1, 1, 0, 0, 0} => 0xF8U
    // lwjpg 0 can not be chosen, because it's not set in the input mask
    // lwjpg 1 can not be chosen, because lwdec 1 has been disabled above,
    // lwjpg 2 can not be chosen, because it has been disabled above
    EXPECT_EQ(outputMask, 0xF8U);
    outputMask.ClearAll();
}
