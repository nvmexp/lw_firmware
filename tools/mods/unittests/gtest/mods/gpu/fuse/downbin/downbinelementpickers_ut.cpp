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

#include "gpu/fuse/downbin/downbinelementpickers.h"
#include "gpu/fuse/downbin/fsset.h"
#include "gpu/fuse/downbin/fsmask.h"

#include "mockfsset.h"

#include "gtest/gtest.h"

using namespace Downbin;

//------------------------------------------------------------------------------
TEST(DownbinElementPickerTest, MostEnabledSubElementPicker)
{
    // 2-2
    MockFsSet gpcSet1(FsUnit::GPC, 1);
    gpcSet1.AddElement(FsUnit::TPC, 4, true, false);
    gpcSet1.AddElement(FsUnit::PES, 2, false, false);
    MostEnabledSubElementPicker picker1(&gpcSet1, FsUnit::PES, FsUnit::TPC);
    FsMask inputMask1, outputMask;
    // Input Mask = 0x1F
    inputMask1 = 0xFU;
    // No TPCs disabled
    EXPECT_EQ(picker1.Pick(0, inputMask1, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0xFU);
    // Disable 1 TPC corresponding to PES 0
    gpcSet1.DisableElements(FsUnit::TPC, 0, 0x1U);
    EXPECT_EQ(picker1.Pick(0, inputMask1, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0xLW);
    // Disable 1 TPC corresponding to PES 0 + 1 TPC from PES 1
    gpcSet1.DisableElements(FsUnit::TPC, 0, 0x8U);
    EXPECT_EQ(picker1.Pick(0, inputMask1, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x6U);
    // Pass invalid group number
    EXPECT_EQ(picker1.Pick(1, inputMask1, &outputMask), RC::BAD_PARAMETER);

    // 3-3-2
    MockFsSet gpcSet2(FsUnit::GPC, 1);
    gpcSet2.AddElement(FsUnit::TPC, 8, true, false);
    gpcSet2.AddElement(FsUnit::PES, 3, false, false);
    MostEnabledSubElementPicker picker2(&gpcSet2, FsUnit::PES, FsUnit::TPC);
    // Input Mask = 0xFF
    FsMask inputMask2;
    inputMask2 = 0xFFU;
    // No TPCs disabled
    EXPECT_EQ(picker2.Pick(0, inputMask2, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x3FU);
    // Disable 1 TPC corresponding to PES 0
    gpcSet2.DisableElements(FsUnit::TPC, 0, 0x1U);
    EXPECT_EQ(picker2.Pick(0, inputMask1, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x38U);
    // Disable 1 TPC corresponding to PES 0 + 1 TPC from PES 1
    gpcSet2.DisableElements(FsUnit::TPC, 0, 0x8U);
    EXPECT_EQ(picker2.Pick(0, inputMask2, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0xF6U);
    // Disable 1 TPC corresponding to PES0, PES1 and PES2
    gpcSet2.DisableElements(FsUnit::TPC, 0, 0x40U);
    EXPECT_EQ(picker2.Pick(0, inputMask2, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x36U);
    // Input Mask excludes TPCs from PES0
    inputMask2 = 0xF8U;
    EXPECT_EQ(picker2.Pick(0, inputMask2, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x30U);
}

//------------------------------------------------------------------------------
TEST(DownbinElementPickerTest, AvailableElementPicker)
{ 
    MockFsSet gpcSet1(FsUnit::GPC, 3);
    gpcSet1.AddElement(FsUnit::TPC, 4, true, false);
    gpcSet1.AddElement(FsUnit::PES, 2, false, false);
    // Using TPC as element FsUnit
    AvailableElementPicker picker1(&gpcSet1, FsUnit::TPC);
    FsMask inputMask1, outputMask;
    // Input Mask = 0xF
    inputMask1 = 0xFU;
    // No TPCs protected
    EXPECT_EQ(picker1.Pick(0, inputMask1, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0xFU);
    // 1 TPC from GPC0 protected
    gpcSet1.GetGroup(0)->GetElementSet(FsUnit::TPC)->MarkProtected(1);
    EXPECT_EQ(picker1.Pick(0, inputMask1, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0xDU);
    // 2 TPC from GPC0 protected
    gpcSet1.GetGroup(0)->GetElementSet(FsUnit::TPC)->MarkProtected(2);
    EXPECT_EQ(picker1.Pick(0, inputMask1, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x9U);
    //Disable 1 TPC from GPC1, 1 protected
    gpcSet1.DisableElements(FsUnit::TPC, 1, 0x1U);
    gpcSet1.GetGroup(1)->GetElementSet(FsUnit::TPC)->MarkProtected(3);
    inputMask1 = 0xDU;
    EXPECT_EQ(picker1.Pick(1, inputMask1, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x5U);
    // Pass invalid group number
    EXPECT_EQ(picker1.Pick(4, inputMask1, &outputMask), RC::BAD_PARAMETER);

    // Using PES as element FsUnit
    AvailableElementPicker picker2(&gpcSet1, FsUnit::PES);
    inputMask1 = 0x3U;
    // No PES protected
    EXPECT_EQ(picker2.Pick(0, inputMask1, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x3U);
    // PES0 from GPC0 protected
    gpcSet1.GetGroup(0)->GetElementSet(FsUnit::PES)->MarkProtected(0);
    EXPECT_EQ(picker2.Pick(0, inputMask1, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0x2U);
}

//------------------------------------------------------------------------------
TEST(DownbinElementPickerTest, PreferredElementPicker)
{ 
    MockFsSet gpcSet1(FsUnit::GPC, 8);
    gpcSet1.AddElement(FsUnit::TPC, 9, true, false);
    // TPC0, TPC1, TPC2 of GPC0 are graphics TPCs
    vector<FsMask> preferredElementMasks(8);
    preferredElementMasks[0] = 0x1f8U;
    preferredElementMasks[1] = 0x0U;
    preferredElementMasks[2] = 0x1ffU;
    preferredElementMasks[3] = 0x1ffU;
    preferredElementMasks[4] = 0x1ffU;
    preferredElementMasks[5] = 0x1ffU;
    preferredElementMasks[6] = 0x1ffU;
    preferredElementMasks[7] = 0x1ffU;

    PreferredElementPicker picker1(&gpcSet1, FsUnit::TPC, preferredElementMasks);
    FsMask inputMask1, outputMask;
    // Input Mask = 0x1ff
    inputMask1 = 0xffU;

    // preferredElementMask = 0x1f8U, choose the preferredElement
    EXPECT_EQ(picker1.Pick(0, inputMask1, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0xf8U);

    // preferredElementMask = 0x0U, outputMask is the same as inputMask
    EXPECT_EQ(picker1.Pick(1, inputMask1, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0xffU);

    // preferredElementMask = fullMask, outputMask is the same as inputMask
    EXPECT_EQ(picker1.Pick(2, inputMask1, &outputMask), RC::OK);
    EXPECT_EQ(outputMask, 0xffU);
}
