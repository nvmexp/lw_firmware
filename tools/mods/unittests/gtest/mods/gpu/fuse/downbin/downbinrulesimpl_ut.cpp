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

#include "gpu/fuse/downbin/downbinfs.h"
#include "gpu/fuse/downbin/downbinimpl.h"
#include "gpu/fuse/downbin/downbinrulesimpl.h"
#include "gpu/fuse/downbin/fselement.h"
#include "gpu/fuse/downbin/fsgroup.h"
#include "gpu/fuse/downbin/fsset.h"

#include "mockfsset.h"

#include "gtest/gtest.h"

using namespace Downbin;

//------------------------------------------------------------------------------
TEST(DownbinRuleTest, ExelwteBasicGroupDisableRule)
{
    MockFsSet gpcSet1(FsUnit::GPC, 1);
    gpcSet1.AddElement(FsUnit::TPC, 4, true, false);
    gpcSet1.AddElement(FsUnit::PES, 2, false, false);
    FsGroup *pGroup1 = gpcSet1.GetGroup(0);
    shared_ptr<DownbinRule> pRule1 = make_shared<BasicGroupDisableRule>(pGroup1);
    pGroup1->AddOnGroupDisabledListener(pRule1);
    // Disable GPC0. Underlying elements should get disabled
    EXPECT_EQ(gpcSet1.DisableGroup(0), RC::OK);
    EXPECT_EQ(gpcSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet1.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0xFU);
    EXPECT_EQ(gpcSet1.GetElementSet(0, FsUnit::PES)->GetTotalDisableMask()->GetMask(), 0x3U);
}

//------------------------------------------------------------------------------
TEST(DownbinRuleTest, ExelwteMinGroupElementCountRule)
{
    MockFsSet gpcSet1(FsUnit::GPC, 2);
    gpcSet1.AddElement(FsUnit::TPC, 4, true, false);
    // Min TPC count per GPC = 3
    for (UINT32 gpc = 0; gpc < 2; gpc++)
    {
        FsElementSet *pTpcs = gpcSet1.GetElementSet(gpc, FsUnit::TPC);
        shared_ptr<DownbinRule> pRule1 = make_shared<MinGroupElementCountRule>(pTpcs, 3);
        pTpcs->AddOnElementDisabledListener(pRule1);
    }
    // Disable GPC0-TPC0. Is within limits
    EXPECT_EQ(gpcSet1.DisableElement(FsUnit::TPC, 0, 0), RC::OK);
    EXPECT_EQ(gpcSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet1.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet1.GetElementSet(1, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x0U);
    // Disable GPC0-TPC2. 2 TPCs disabled in the GPC. GPC should get disabled
    EXPECT_EQ(gpcSet1.DisableElement(FsUnit::TPC, 0, 2), RC::OK);
    EXPECT_EQ(gpcSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet1.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x5U);
    EXPECT_EQ(gpcSet1.GetElementSet(1, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x0U);
    // Disable GPC0-TPC3. Group already disabled, rule shouldn't have any extra effect
    EXPECT_EQ(gpcSet1.DisableElement(FsUnit::TPC, 0, 3), RC::OK);
    EXPECT_EQ(gpcSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet1.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0xDU);
    EXPECT_EQ(gpcSet1.GetElementSet(1, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x0U);
}

//------------------------------------------------------------------------------
TEST(DownbinRuleTest, ExelwteDownbinMinSubElementCountRule)
{
    MockFsSet fbpSet1(FsUnit::FBP, 2);
    fbpSet1.AddElement(FsUnit::LTC, 2, true, false);
    fbpSet1.AddElement(FsUnit::L2_SLICE, 8, false, false);
    for (UINT32 fbp = 0; fbp < 2; fbp++)
    {
        FsGroup *pFsGroup = fbpSet1.GetGroup(fbp);
        shared_ptr<DownbinRule> pRule1 = make_shared<MinSubElementCountRule>
                                            (pFsGroup, FsUnit::LTC, FsUnit::L2_SLICE,
                                             3);
        FsElementSet *pL2SliceSet = fbpSet1.GetElementSet(fbp, FsUnit::L2_SLICE);
        pL2SliceSet->AddOnElementDisabledListener(pRule1);
    }
    // Disable FBP0-LTC0-Slice0. Is within limits
    EXPECT_EQ(fbpSet1.DisableElements(FsUnit::L2_SLICE, 0, 0x1), RC::OK);
    EXPECT_EQ(fbpSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet1.GetElementSet(0, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet1.GetElementSet(0, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(fbpSet1.GetElementSet(1, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet1.GetElementSet(1, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x0U);
    // Disable FBP0-LTC1-Slice2. Is within limits
    EXPECT_EQ(fbpSet1.DisableElements(FsUnit::L2_SLICE, 0, 0x40), RC::OK);
    EXPECT_EQ(fbpSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet1.GetElementSet(0, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet1.GetElementSet(0, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x41U);
    EXPECT_EQ(fbpSet1.GetElementSet(1, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet1.GetElementSet(1, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x0U);
    // Disable FBP0-LTC0-Slice2. Num slices in LTC0 < 3, LTC should get disabled
    EXPECT_EQ(fbpSet1.DisableElements(FsUnit::L2_SLICE, 0, 0x4), RC::OK);
    EXPECT_EQ(fbpSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet1.GetElementSet(0, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(fbpSet1.GetElementSet(0, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x45U);
    EXPECT_EQ(fbpSet1.GetElementSet(1, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet1.GetElementSet(1, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x0U);
    // Disable FBP1-LTC1. No special effect
    EXPECT_EQ(fbpSet1.DisableElements(FsUnit::LTC, 1, 0x2), RC::OK);
    EXPECT_EQ(fbpSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet1.GetElementSet(0, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(fbpSet1.GetElementSet(0, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x45U);
    EXPECT_EQ(fbpSet1.GetElementSet(1, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x2U);
    EXPECT_EQ(fbpSet1.GetElementSet(1, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x0U);
    // Disable FBP1-LTC1-Slice0,1. No special effect
    EXPECT_EQ(fbpSet1.DisableElements(FsUnit::L2_SLICE, 1, 0x30), RC::OK);
    EXPECT_EQ(fbpSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet1.GetElementSet(0, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(fbpSet1.GetElementSet(0, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x45U);
    EXPECT_EQ(fbpSet1.GetElementSet(1, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x2U);
    EXPECT_EQ(fbpSet1.GetElementSet(1, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x30U);
    // Disable FBP1-LTC0-Slice0,1. Disables FBP1-LTC0
    EXPECT_EQ(fbpSet1.DisableElements(FsUnit::L2_SLICE, 1, 0x3), RC::OK);
    EXPECT_EQ(fbpSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet1.GetElementSet(0, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(fbpSet1.GetElementSet(0, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x45U);
    EXPECT_EQ(fbpSet1.GetElementSet(1, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x3U);
    EXPECT_EQ(fbpSet1.GetElementSet(1, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x33U);
}

//------------------------------------------------------------------------------
TEST(DownbinRuleTest, ExelwteDownbinMinElementCountRule)
{
    MockFsSet gpcSet1(FsUnit::GPC, 2);
    gpcSet1.AddElement(FsUnit::TPC, 4, true, false);
    // Min TPC count = 6
    shared_ptr<DownbinRule> pRule1 = make_shared<MinElementCountRule>(&gpcSet1, FsUnit::TPC, 6);
    for (UINT32 gpc = 0; gpc < 2; gpc++)
    {
        FsElementSet *pTpcs = gpcSet1.GetElementSet(gpc, FsUnit::TPC);
        pTpcs->AddOnElementDisabledListener(pRule1);
    }
    // Disable GPC0-TPC0,2. Is within limits
    EXPECT_EQ(gpcSet1.DisableElements(FsUnit::TPC, 0, 0x5), RC::OK);
    EXPECT_EQ(gpcSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet1.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x5U);
    EXPECT_EQ(gpcSet1.GetElementSet(1, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x0U);
    // Disable GPC1-TPC0. Will result in 5 total TPCs in the set
    EXPECT_EQ(gpcSet1.DisableElements(FsUnit::TPC, 1, 0x1), RC::CANNOT_MEET_FS_REQUIREMENTS);
    EXPECT_EQ(gpcSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet1.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x5U);
    EXPECT_EQ(gpcSet1.GetElementSet(1, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
}

//------------------------------------------------------------------------------
TEST(DownbinRuleTest, ExelwteDownbinMinElementCountMaskRule)
{
    // Full TPC masks, min TPC count = 3
    MockFsSet gpcSet1(FsUnit::GPC, 2);
    gpcSet1.AddElement(FsUnit::TPC, 4, true, false);
    vector<UINT32> tpcMasks1 = {0xF, 0xF};
    shared_ptr<DownbinRule> pRule1 = make_shared<MinElementCountRule>
                                     (&gpcSet1, FsUnit::TPC, 3, tpcMasks1);
    for (UINT32 gpc = 0; gpc < 2; gpc++)
    {
        FsElementSet *pTpcs = gpcSet1.GetElementSet(gpc, FsUnit::TPC);
        pTpcs->AddOnElementDisabledListener(pRule1);
    }
    // Disable GPC0-TPC0,2. Is within limits (6 enabled)
    EXPECT_EQ(gpcSet1.DisableElements(FsUnit::TPC, 0, 0x5), RC::OK);
    EXPECT_EQ(gpcSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet1.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x5U);
    EXPECT_EQ(gpcSet1.GetElementSet(1, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x0U);
    // Disable GPC1-TPC1. Is still within limits (5 enabled)
    EXPECT_EQ(gpcSet1.DisableElements(FsUnit::TPC, 1, 0x2), RC::OK);
    EXPECT_EQ(gpcSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet1.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x5U);
    EXPECT_EQ(gpcSet1.GetElementSet(1, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x2U);
    // Disable GPC1-TPC2,3. Is still within limits (3 enabled)
    EXPECT_EQ(gpcSet1.DisableElements(FsUnit::TPC, 1, 0xC), RC::OK);
    EXPECT_EQ(gpcSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet1.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x5U);
    EXPECT_EQ(gpcSet1.GetElementSet(1, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0xEU);
    // Disable GPC0-TPC4. Will result in 2 total TPCs in the set
    EXPECT_EQ(gpcSet1.DisableElements(FsUnit::TPC, 0, 0x8), RC::CANNOT_MEET_FS_REQUIREMENTS);

    // Non full TPC masks, min masked TPC count = 3
    MockFsSet gpcSet2(FsUnit::GPC, 2);
    gpcSet2.AddElement(FsUnit::TPC, 4, true, false);
    vector<UINT32> tpcMasks2 = {0x5, 0x7};
    shared_ptr<DownbinRule> pRule2 = make_shared<MinElementCountRule>
                                     (&gpcSet2, FsUnit::TPC, 3, tpcMasks2);
    for (UINT32 gpc = 0; gpc < 2; gpc++)
    {
        FsElementSet *pTpcs = gpcSet2.GetElementSet(gpc, FsUnit::TPC);
        pTpcs->AddOnElementDisabledListener(pRule2);
    }
    // Disable GPC0-TPC0,2. Is within limits (6 total enabled, 3 masked enabled)
    EXPECT_EQ(gpcSet2.DisableElements(FsUnit::TPC, 0, 0x5), RC::OK);
    EXPECT_EQ(gpcSet2.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet2.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x5U);
    EXPECT_EQ(gpcSet2.GetElementSet(1, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x0U);
    // Disable GPC1-TPC1. Will result in 5 total TPCs, but only 2 TPCs from the masks
    EXPECT_EQ(gpcSet2.DisableElements(FsUnit::TPC, 1, 0x2), RC::CANNOT_MEET_FS_REQUIREMENTS);

    // Only some TPCs in GPC0 are relevant, min TPC count = 1
    MockFsSet gpcSet3(FsUnit::GPC, 2);
    gpcSet3.AddElement(FsUnit::TPC, 4, true, false);
    vector<UINT32> tpcMasks3 = {0x7, 0x0};
    shared_ptr<DownbinRule> pRule3 = make_shared<MinElementCountRule>
                                     (&gpcSet3, FsUnit::TPC, 1, tpcMasks3);
    for (UINT32 gpc = 0; gpc < 2; gpc++)
    {
        FsElementSet *pTpcs = gpcSet3.GetElementSet(gpc, FsUnit::TPC);
        pTpcs->AddOnElementDisabledListener(pRule3);
    }
    // Disable GPC0-TPC0,2. Is within limits (6 enabled, 1 masked enabled)
    EXPECT_EQ(gpcSet3.DisableElements(FsUnit::TPC, 0, 0x5), RC::OK);
    EXPECT_EQ(gpcSet3.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet3.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x5U);
    EXPECT_EQ(gpcSet3.GetElementSet(1, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x0U);
    // Disable all TPCs in GPC1. Is still within limits (2 enabed, 1 masked enabled)
    EXPECT_EQ(gpcSet3.DisableElements(FsUnit::TPC, 1, 0xF), RC::OK);
    EXPECT_EQ(gpcSet3.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet3.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x5U);
    EXPECT_EQ(gpcSet3.GetElementSet(1, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0xFU);
    // Disable GPC0-TPC1. Will result in 0 masked TPCs
    EXPECT_EQ(gpcSet3.DisableElements(FsUnit::TPC, 0, 0x2), RC::CANNOT_MEET_FS_REQUIREMENTS);
}

//------------------------------------------------------------------------------
TEST(DownbinRuleTest, ExelwteAlwaysEnableRule)
{
    MockFsSet gpcSet1(FsUnit::GPC, 2);
    gpcSet1.AddElement(FsUnit::TPC, 4, true, false);
    FsElementSet *pGpc0Tpcs = gpcSet1.GetElementSet(0, FsUnit::TPC);
    // Always enable GPC0-TPC1
    shared_ptr<DownbinRule> pRule1 = make_shared<AlwaysEnableRule>(pGpc0Tpcs, 1);
    pGpc0Tpcs->AddOnElementDisabledListener(pRule1);
    // Disable GPC0-TPC0
    EXPECT_EQ(gpcSet1.DisableElement(FsUnit::TPC, 0, 0), RC::OK);
    EXPECT_EQ(gpcSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet1.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet1.GetElementSet(1, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x0U);
    // Disable GPC1-TPC1
    EXPECT_EQ(gpcSet1.DisableElement(FsUnit::TPC, 1, 1), RC::OK);
    EXPECT_EQ(gpcSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet1.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet1.GetElementSet(1, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x2U);
    // Disable GPC0-TPC1
    EXPECT_EQ(gpcSet1.DisableElement(FsUnit::TPC, 0, 1), RC::CANNOT_MEET_FS_REQUIREMENTS);
    EXPECT_EQ(gpcSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet1.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x3U);
    EXPECT_EQ(gpcSet1.GetElementSet(1, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x2U);
}

//------------------------------------------------------------------------------
TEST(DownbinRuleTest, ExelwteSubElementDependencyRule)
{
    // [2, 2] distribution
    MockFsSet gpcSet1(FsUnit::GPC, 1);
    gpcSet1.AddElement(FsUnit::TPC, 4, true, false);
    gpcSet1.AddElement(FsUnit::PES, 2, false, false);
    FsGroup *pGroup1 = gpcSet1.GetGroup(0);
    shared_ptr<DownbinRule> pRule1 = make_shared<SubElementDependencyRule>(pGroup1,
                                                                           FsUnit::PES, FsUnit::TPC);
    // Add rule to PES
    gpcSet1.GetElementSet(0, FsUnit::PES)->AddOnElementDisabledListener(pRule1);
    // Disable PES0
    EXPECT_EQ(gpcSet1.DisableElement(FsUnit::PES, 0, 0), RC::OK);
    EXPECT_EQ(gpcSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet1.GetElementSet(0, FsUnit::PES)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet1.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x3U);
    // Disable PES1
    EXPECT_EQ(gpcSet1.DisableElement(FsUnit::PES, 0, 1), RC::OK);
    EXPECT_EQ(gpcSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet1.GetElementSet(0, FsUnit::PES)->GetTotalDisableMask()->GetMask(), 0x3U);
    EXPECT_EQ(gpcSet1.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0xFU);

    // [3, 3, 2] distribution
    MockFsSet gpcSet2(FsUnit::GPC, 1);
    gpcSet2.AddElement(FsUnit::TPC, 8, true, false);
    gpcSet2.AddElement(FsUnit::PES, 3, false, false);
    FsGroup *pGroup2 = gpcSet2.GetGroup(0);
    shared_ptr<DownbinRule> pRule2 = make_shared<SubElementDependencyRule>(pGroup2,
                                                                           FsUnit::PES, FsUnit::TPC);
    // Add rule to PES
    gpcSet2.GetElementSet(0, FsUnit::PES)->AddOnElementDisabledListener(pRule2);
    // Disable PES0
    EXPECT_EQ(gpcSet2.DisableElement(FsUnit::PES, 0, 0), RC::OK);
    EXPECT_EQ(gpcSet2.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet2.GetElementSet(0, FsUnit::PES)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet2.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x7U);
    // Disable PES2
    EXPECT_EQ(gpcSet2.DisableElement(FsUnit::PES, 0, 2), RC::OK);
    EXPECT_EQ(gpcSet2.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet2.GetElementSet(0, FsUnit::PES)->GetTotalDisableMask()->GetMask(), 0x5U);
    EXPECT_EQ(gpcSet2.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0xC7U);

    // [3, 3, 2] distribution
    MockFsSet gpcSet3(FsUnit::GPC, 1);
    gpcSet3.AddElement(FsUnit::TPC, 8, true, false);
    gpcSet3.AddElement(FsUnit::PES, 3, false, false);
    FsGroup *pGroup3 = gpcSet3.GetGroup(0);
    shared_ptr<DownbinRule> pRule3 = make_shared<SubElementDependencyRule>(pGroup3,
                                                                           FsUnit::PES, FsUnit::TPC);
    // Add rule to TPC
    gpcSet3.GetElementSet(0, FsUnit::TPC)->AddOnElementDisabledListener(pRule3);
    // Disable TPC0,1. Nothing happens
    EXPECT_EQ(gpcSet3.DisableElements(FsUnit::TPC, 0, 0x3), RC::OK);
    EXPECT_EQ(gpcSet3.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet3.GetElementSet(0, FsUnit::PES)->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet3.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x3U);
    // Disable TPC2. PES0 disabled
    EXPECT_EQ(gpcSet3.DisableElements(FsUnit::TPC, 0, 0x4), RC::OK);
    EXPECT_EQ(gpcSet3.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet3.GetElementSet(0, FsUnit::PES)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet3.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x7U);
    // Disable TPC6,7. PES2 disabled
    EXPECT_EQ(gpcSet3.DisableElements(FsUnit::TPC, 0, 0xC0), RC::OK);
    EXPECT_EQ(gpcSet3.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet3.GetElementSet(0, FsUnit::PES)->GetTotalDisableMask()->GetMask(), 0x5U);
    EXPECT_EQ(gpcSet3.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0xC7U);
}

//------------------------------------------------------------------------------
TEST(DownbinRuleTest, ExelwteCrossElementDependencyRule)
{
    // Add dependency b/w GPC0 and GPC2
    MockFsSet gpcSet1(FsUnit::GPC, 3);
    gpcSet1.AddElement(FsUnit::TPC, 4, true, false);
    shared_ptr<DownbinRule> pRule1 = make_shared<CrossElementDependencyRule>(&gpcSet1,
                                                                             FsUnit::GPC,
                                                                             0, 0, 0, 2);
    gpcSet1.GetGroup(0)->AddOnGroupDisabledListener(pRule1);
    EXPECT_EQ(gpcSet1.DisableGroup(1), RC::OK);
    EXPECT_EQ(gpcSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x02U);
    EXPECT_EQ(gpcSet1.DisableGroup(0), RC::OK);
    EXPECT_EQ(gpcSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x07U);

    MockFsSet gpcSet2(FsUnit::GPC, 3);
    gpcSet2.AddElement(FsUnit::TPC, 4, true, false);
    shared_ptr<DownbinRule> pRule2 = make_shared<CrossElementDependencyRule>(&gpcSet2,
                                                                             FsUnit::GPC,
                                                                             0, 2, 0, 0);
    gpcSet2.GetGroup(2)->AddOnGroupDisabledListener(pRule2);
    EXPECT_EQ(gpcSet2.DisableGroup(0), RC::OK);
    EXPECT_EQ(gpcSet2.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x01U);
    EXPECT_EQ(gpcSet2.DisableGroup(2), RC::OK);
    EXPECT_EQ(gpcSet2.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x05U);

    // Add dependency between GPC0-TPC1 and GPC0-TPC2
    MockFsSet gpcSet3(FsUnit::GPC, 2);
    gpcSet3.AddElement(FsUnit::TPC, 4, true, false);
    shared_ptr<DownbinRule> pRule3 = make_shared<CrossElementDependencyRule>(&gpcSet3,
                                                                             FsUnit::TPC,
                                                                             0, 1, 0, 2);
    gpcSet3.GetElementSet(0, FsUnit::TPC)->AddOnElementDisabledListener(pRule3);
    EXPECT_EQ(gpcSet3.DisableElement(FsUnit::TPC, 0, 1), RC::OK);
    EXPECT_EQ(gpcSet3.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x6U);
    EXPECT_EQ(gpcSet3.GetElementSet(1, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x0U);

    // Add dependency between GPC0-TPC1 and GPC1-TPC0
    MockFsSet gpcSet4(FsUnit::GPC, 2);
    gpcSet4.AddElement(FsUnit::TPC, 4, true, false);
    shared_ptr<DownbinRule> pRule4 = make_shared<CrossElementDependencyRule>(&gpcSet4,
                                                                             FsUnit::TPC,
                                                                             0, 1, 1, 0);
    gpcSet4.GetElementSet(0, FsUnit::TPC)->AddOnElementDisabledListener(pRule4);
    EXPECT_EQ(gpcSet4.DisableElements(FsUnit::TPC, 0, 0x3), RC::OK);
    EXPECT_EQ(gpcSet4.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x3U);
    EXPECT_EQ(gpcSet4.GetElementSet(1, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
}

//------------------------------------------------------------------------------
TEST(DownbinRuleTest, ExelwteIlwalidElementComboRule)
{
    // Invalid group element combo
    MockFsSet gpcSet1(FsUnit::GPC, 3);
    gpcSet1.AddElement(FsUnit::TPC, 4, true, false);
    shared_ptr<DownbinRule> pRule1 = make_shared<IlwalidElementComboRule>(gpcSet1.GetGroupElementSet(),
                                                                          0x5);
    gpcSet1.GetGroupElementSet()->AddOnElementDisabledListener(pRule1);
    EXPECT_EQ(gpcSet1.DisableGroup(0), RC::OK);
    EXPECT_EQ(gpcSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x01U);
    EXPECT_EQ(gpcSet1.DisableGroup(2), RC::CANNOT_MEET_FS_REQUIREMENTS);

    // Invalid element combo
    MockFsSet gpcSet2(FsUnit::GPC, 2);
    gpcSet2.AddElement(FsUnit::TPC, 4, true, false);
    shared_ptr<DownbinRule> pRule2 = make_shared<IlwalidElementComboRule>(gpcSet2.GetElementSet(0, FsUnit::TPC),
                                                                          0x5);
    gpcSet2.GetElementSet(0, FsUnit::TPC)->AddOnElementDisabledListener(pRule2);
    EXPECT_EQ(gpcSet2.DisableElement(FsUnit::TPC, 0, 2), RC::OK);
    EXPECT_EQ(gpcSet2.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet2.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x4U);
    EXPECT_EQ(gpcSet2.DisableElement(FsUnit::TPC, 0, 0), RC::OK);
    EXPECT_EQ(gpcSet2.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet2.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0xFU);

    // Invalid element combo
    MockFsSet gpcSet3(FsUnit::GPC, 2);
    gpcSet3.AddElement(FsUnit::TPC, 4, true, false);
    shared_ptr<DownbinRule> pRule3 = make_shared<IlwalidElementComboRule>(gpcSet3.GetElementSet(0, FsUnit::TPC),
                                                                          0x6);
    gpcSet3.GetElementSet(0, FsUnit::TPC)->AddOnElementDisabledListener(pRule3);
    EXPECT_EQ(gpcSet3.DisableElements(FsUnit::TPC, 0, 0x5), RC::OK);
    EXPECT_EQ(gpcSet3.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet3.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x5U);
    EXPECT_EQ(gpcSet3.DisableElement(FsUnit::TPC, 0, 1), RC::OK);
    EXPECT_EQ(gpcSet3.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet3.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0xFU);
}

//------------------------------------------------------------------------------
TEST(DownbinRuleTest, ExelwteDownbinGH100L2Slice)
{
    UINT32 maxSlicesFs = 4;
    UINT32 maxSlicesFsPerFbp = 1;

    UINT32 numFbps1 = 2;
    UINT32 numLtcsPerFbp = 2;
    UINT32 numSlicesPerFbp = 8;
    MockFsSet fbpSet1(FsUnit::FBP, numFbps1);
    fbpSet1.AddElement(FsUnit::LTC, numLtcsPerFbp, true, false);
    fbpSet1.AddElement(FsUnit::L2_SLICE, numSlicesPerFbp, false, false);
    shared_ptr<DownbinRule> pRule1 = make_shared<GH100L2SliceRule>(&fbpSet1, maxSlicesFs, maxSlicesFsPerFbp);
    for (UINT32 fbp = 0; fbp < numFbps1; fbp++)
    {
        FsElementSet *pSlices = fbpSet1.GetElementSet(fbp, FsUnit::L2_SLICE);
        pSlices->AddOnElementDisabledListener(pRule1);
    }
    // Disable 1 slice. Ok.
    EXPECT_EQ(fbpSet1.DisableElement(FsUnit::L2_SLICE, 0, 0), RC::OK);
    EXPECT_EQ(fbpSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet1.GetElementSet(0, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet1.GetElementSet(1, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet1.GetElementSet(0, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(fbpSet1.GetElementSet(1, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x0U);
    // Disable 2 slices (in different FBPs)
    EXPECT_EQ(fbpSet1.DisableElement(FsUnit::L2_SLICE, 1, 2), RC::OK);
    EXPECT_EQ(fbpSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet1.GetElementSet(0, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet1.GetElementSet(1, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet1.GetElementSet(0, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(fbpSet1.GetElementSet(1, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x4U);
    // Disable 2 slices (in the same FBP). Disables LTCs
    EXPECT_EQ(fbpSet1.DisableElement(FsUnit::L2_SLICE, 0, 2), RC::OK);
    EXPECT_EQ(fbpSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet1.GetElementSet(0, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(fbpSet1.GetElementSet(1, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(fbpSet1.GetElementSet(0, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xFU);
    EXPECT_EQ(fbpSet1.GetElementSet(1, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xFU);
    // Disable another slice. Now disables LTCs.
    EXPECT_EQ(fbpSet1.DisableElement(FsUnit::L2_SLICE, 0, 7), RC::OK);
    EXPECT_EQ(fbpSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet1.GetElementSet(0, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x3U);
    EXPECT_EQ(fbpSet1.GetElementSet(1, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(fbpSet1.GetElementSet(0, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xFFU);
    EXPECT_EQ(fbpSet1.GetElementSet(1, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xFU);

    UINT32 numFbps2 = 6;
    MockFsSet fbpSet2(FsUnit::FBP, numFbps2);
    fbpSet2.AddElement(FsUnit::LTC, numLtcsPerFbp, true, false);
    fbpSet2.AddElement(FsUnit::L2_SLICE, numSlicesPerFbp, false, false);
    shared_ptr<DownbinRule> pRule2 = make_shared<GH100L2SliceRule>(&fbpSet2, maxSlicesFs, maxSlicesFsPerFbp);
    for (UINT32 fbp = 0; fbp < numFbps2; fbp++)
    {
        FsElementSet *pSlices = fbpSet2.GetElementSet(fbp, FsUnit::L2_SLICE);
        pSlices->AddOnElementDisabledListener(pRule2);
    }
    // Disable 3 slices (in different FBPs). Ok.
    EXPECT_EQ(fbpSet2.DisableElement(FsUnit::L2_SLICE, 0, 0), RC::OK);
    EXPECT_EQ(fbpSet2.DisableElement(FsUnit::L2_SLICE, 1, 1), RC::OK);
    EXPECT_EQ(fbpSet2.DisableElement(FsUnit::L2_SLICE, 2, 3), RC::OK);
    EXPECT_EQ(fbpSet2.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet2.GetElementSet(0, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet2.GetElementSet(1, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet2.GetElementSet(2, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet2.GetElementSet(3, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet2.GetElementSet(4, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet2.GetElementSet(5, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet2.GetElementSet(0, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(fbpSet2.GetElementSet(1, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x2U);
    EXPECT_EQ(fbpSet2.GetElementSet(2, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x8U);
    EXPECT_EQ(fbpSet2.GetElementSet(3, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet2.GetElementSet(4, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet2.GetElementSet(5, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x0U);
    // Disable 5 slices - 1 per FBP. Disable LTCs.
    EXPECT_EQ(fbpSet2.DisableElement(FsUnit::L2_SLICE, 3, 2), RC::OK);
    EXPECT_EQ(fbpSet2.DisableElement(FsUnit::L2_SLICE, 4, 7), RC::OK);
    EXPECT_EQ(fbpSet2.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet2.GetElementSet(0, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(fbpSet2.GetElementSet(1, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(fbpSet2.GetElementSet(2, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(fbpSet2.GetElementSet(3, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(fbpSet2.GetElementSet(4, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x2U);
    EXPECT_EQ(fbpSet2.GetElementSet(5, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet2.GetElementSet(0, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xFU);
    EXPECT_EQ(fbpSet2.GetElementSet(1, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xFU);
    EXPECT_EQ(fbpSet2.GetElementSet(2, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xFU);
    EXPECT_EQ(fbpSet2.GetElementSet(3, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xFU);
    EXPECT_EQ(fbpSet2.GetElementSet(4, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xF0U);
    EXPECT_EQ(fbpSet2.GetElementSet(5, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x0U);
    // Disable another slice in another FBP. Now disables LTCs.
    EXPECT_EQ(fbpSet2.DisableElement(FsUnit::L2_SLICE, 5, 4), RC::OK);
    EXPECT_EQ(fbpSet2.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet2.GetElementSet(0, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(fbpSet2.GetElementSet(1, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(fbpSet2.GetElementSet(2, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(fbpSet2.GetElementSet(3, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(fbpSet2.GetElementSet(4, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x2U);
    EXPECT_EQ(fbpSet2.GetElementSet(5, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x2U);
    EXPECT_EQ(fbpSet2.GetElementSet(0, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xFU);
    EXPECT_EQ(fbpSet2.GetElementSet(1, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xFU);
    EXPECT_EQ(fbpSet2.GetElementSet(2, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xFU);
    EXPECT_EQ(fbpSet2.GetElementSet(3, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xFU);
    EXPECT_EQ(fbpSet2.GetElementSet(4, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xF0U);
    EXPECT_EQ(fbpSet2.GetElementSet(5, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xF0U);

    UINT32 numFbps3 = 2;
    UINT32 numSlicesPerFbp3 = 4;  // Only 2 slices per FBP
    MockFsSet fbpSet3(FsUnit::FBP, numFbps3);
    fbpSet3.AddElement(FsUnit::LTC, numLtcsPerFbp, true, false);
    fbpSet3.AddElement(FsUnit::L2_SLICE, numSlicesPerFbp3, false, false);
    shared_ptr<DownbinRule> pRule3 = make_shared<GH100L2SliceRule>(&fbpSet3, maxSlicesFs, maxSlicesFsPerFbp);
    for (UINT32 fbp = 0; fbp < numFbps3; fbp++)
    {
        FsElementSet *pSlices = fbpSet3.GetElementSet(fbp, FsUnit::L2_SLICE);
        pSlices->AddOnElementDisabledListener(pRule3);
    }
    // Disable a LTC.
    EXPECT_EQ(fbpSet3.DisableElement(FsUnit::LTC, 0, 0), RC::OK);
    EXPECT_EQ(fbpSet3.DisableElements(FsUnit::L2_SLICE, 0, 0x3), RC::OK);
    EXPECT_EQ(fbpSet3.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet3.GetElementSet(0, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(fbpSet3.GetElementSet(1, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet3.GetElementSet(0, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x3U);
    EXPECT_EQ(fbpSet3.GetElementSet(1, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x0U);
    // Disable 1 slice. Disables LTC.
    EXPECT_EQ(fbpSet3.DisableElement(FsUnit::L2_SLICE, 1, 0), RC::OK);
    EXPECT_EQ(fbpSet3.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet3.GetElementSet(0, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(fbpSet3.GetElementSet(1, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(fbpSet3.GetElementSet(0, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x3U);
    EXPECT_EQ(fbpSet3.GetElementSet(1, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x3U);

    UINT32 numFbps4 = 2;
    UINT32 numSlicesPerFbp4 = 8;
    MockFsSet fbpSet4(FsUnit::FBP, numFbps4);
    fbpSet4.AddElement(FsUnit::LTC, numLtcsPerFbp, true, false);
    fbpSet4.AddElement(FsUnit::L2_SLICE, numSlicesPerFbp4, false, false);
    shared_ptr<DownbinRule> pRule4 = make_shared<GH100L2SliceRule>(&fbpSet4, maxSlicesFs, maxSlicesFsPerFbp);
    for (UINT32 fbp = 0; fbp < numFbps4; fbp++)
    {
        FsElementSet *pSlices = fbpSet4.GetElementSet(fbp, FsUnit::L2_SLICE);
        pSlices->AddOnElementDisabledListener(pRule4);
    }
    // Disable a FBP.
    EXPECT_EQ(fbpSet4.DisableGroup(0), RC::OK);
    EXPECT_EQ(fbpSet4.DisableElements(FsUnit::LTC, 0, 0x3), RC::OK);
    EXPECT_EQ(fbpSet4.DisableElements(FsUnit::L2_SLICE, 0, 0xF), RC::OK);
    EXPECT_EQ(fbpSet4.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(fbpSet4.GetElementSet(0, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x3U);
    EXPECT_EQ(fbpSet4.GetElementSet(1, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet4.GetElementSet(0, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xFU);
    EXPECT_EQ(fbpSet4.GetElementSet(1, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x0U);
    // Disable 1 slice. Disables LTC.
    EXPECT_EQ(fbpSet4.DisableElement(FsUnit::L2_SLICE, 1, 1), RC::OK);
    EXPECT_EQ(fbpSet4.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(fbpSet4.GetElementSet(0, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x3U);
    EXPECT_EQ(fbpSet4.GetElementSet(1, FsUnit::LTC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(fbpSet4.GetElementSet(0, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xFU);
    EXPECT_EQ(fbpSet4.GetElementSet(1, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xFU);
}

//------------------------------------------------------------------------------
TEST(DownbinRuleTest, ExelwteVGpcSkylineRule)
{
    // Initial NumTpcs per GPC is: {9, 9, 9, 9, 9, 9, 9, 9}
    MockFsSet gpcSet1(FsUnit::GPC, 8);
    gpcSet1.AddElement(FsUnit::TPC, 9, true, false);
    vector<UINT32> mockVGpcSkylineList1 = {5,8,8,8,9,9,9,9};
    shared_ptr<DownbinRule> pRule1 = make_shared<VGpcSkylineRule>(&gpcSet1, mockVGpcSkylineList1);
    for (UINT32 gpcIdx = 0; gpcIdx < 8; gpcIdx++)
    {
        FsElementSet *pElementSet = gpcSet1.GetElementSet(gpcIdx, FsUnit::TPC);
        pElementSet->AddOnElementDisabledListener(pRule1);
    }
    // Disable GPC0-TPC0. Is within limits. NumTpcs per GPC is: {8, 9, 9, 9, 9, 9, 9, 9}
    EXPECT_EQ(gpcSet1.DisableElement(FsUnit::TPC, 0, 0), RC::OK);
    EXPECT_EQ(gpcSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet1.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet1.GetElementSet(1, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x0U);

    // Disable GPC1-TPC0. Is within limits. NumTpcs per GPC is: {8, 8, 9, 9, 9, 9, 9, 9}
    EXPECT_EQ(gpcSet1.DisableElement(FsUnit::TPC, 1, 0), RC::OK);
    EXPECT_EQ(gpcSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet1.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet1.GetElementSet(1, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet1.GetElementSet(2, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x0U);

    // Disable GPC2-TPC0. Is within limits. NumTpcs per GPC is: {8, 8, 8, 9, 9, 9, 9, 9}
    EXPECT_EQ(gpcSet1.DisableElement(FsUnit::TPC, 2, 0), RC::OK);
    EXPECT_EQ(gpcSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet1.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet1.GetElementSet(1, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet1.GetElementSet(2, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet1.GetElementSet(3, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x0U);

    // Disable GPC3-TPC0. Is within limits. NumTpcs per GPC is: {8, 8, 8, 8, 9, 9, 9, 9}
    EXPECT_EQ(gpcSet1.DisableElement(FsUnit::TPC, 3, 0), RC::OK);
    EXPECT_EQ(gpcSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet1.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet1.GetElementSet(1, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet1.GetElementSet(2, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet1.GetElementSet(3, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet1.GetElementSet(4, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x0U);

    // Disable GPC4-TPC0. Not within limits. NumTpcs per GPC is: {8, 8, 8, 8, 8, 9, 9, 9}
    // mockVGpcSkylineList = {5,8,8,8,9,9,9,9}. Which requires 4 GPCs that has at leatse 9 TPCs
    EXPECT_EQ(gpcSet1.DisableElement(FsUnit::TPC, 4, 0), RC::CANNOT_MEET_FS_REQUIREMENTS);
    EXPECT_EQ(gpcSet1.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet1.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet1.GetElementSet(1, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet1.GetElementSet(2, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet1.GetElementSet(3, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet1.GetElementSet(4, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet1.GetElementSet(5, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x0U);

    // Initial NumTpcs per GPC is: {9, 9, 9, 9, 9, 9, 9, 9}
    MockFsSet gpcSet2(FsUnit::GPC, 8);
    gpcSet2.AddElement(FsUnit::TPC, 9, true, false);
    // VGpcSkylineList will be padded as: {0, 5, 8, 8, 9, 9, 9, 9}
    vector<UINT32> mockVGpcSkylineList2 = {5,8,8,9,9,9,9};
    shared_ptr<DownbinRule> pRule2 = make_shared<VGpcSkylineRule>(&gpcSet2, mockVGpcSkylineList2);
    for (UINT32 gpcIdx = 0; gpcIdx < 8; gpcIdx++)
    {
        FsElementSet *pElementSet = gpcSet2.GetElementSet(gpcIdx, FsUnit::TPC);
        pElementSet->AddOnElementDisabledListener(pRule2);
    }
    // Disable GPC0-TPC0. Is within limits. NumTpcs per GPC is: {8, 9, 9, 9, 9, 9, 9, 9}
    EXPECT_EQ(gpcSet2.DisableElement(FsUnit::TPC, 0, 0), RC::OK);
    EXPECT_EQ(gpcSet2.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet2.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet2.GetElementSet(1, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x0U);

    // Disable GPC1-TPC0. Is within limits. NumTpcs per GPC is: {8, 8, 9, 9, 9, 9, 9, 9}
    EXPECT_EQ(gpcSet2.DisableElement(FsUnit::TPC, 1, 0), RC::OK);
    EXPECT_EQ(gpcSet2.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet2.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet2.GetElementSet(1, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet2.GetElementSet(2, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x0U);

    // Disable GPC2-TPC0. Is within limits. NumTpcs per GPC is: {8, 8, 8, 9, 9, 9, 9, 9}
    EXPECT_EQ(gpcSet2.DisableElement(FsUnit::TPC, 2, 0), RC::OK);
    EXPECT_EQ(gpcSet2.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet2.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet2.GetElementSet(1, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet2.GetElementSet(2, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet2.GetElementSet(3, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x0U);

    // Disable GPC3-TPC0. Is within limits. NumTpcs per GPC is: {8, 8, 8, 8, 9, 9, 9, 9}
    EXPECT_EQ(gpcSet2.DisableElement(FsUnit::TPC, 3, 0), RC::OK);
    EXPECT_EQ(gpcSet2.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet2.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet2.GetElementSet(1, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet2.GetElementSet(2, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet2.GetElementSet(3, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet2.GetElementSet(4, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x0U);

    // Disable GPC4-TPC0. Not within limits. NumTpcs per GPC is: {8, 8, 8, 8, 8, 9, 9, 9}
    // mockVGpcSkylineList = {0,8,8,8,9,9,9,9}. Which requires 4 GPCs that has at least 9 TPCs
    EXPECT_EQ(gpcSet2.DisableElement(FsUnit::TPC, 4, 0), RC::CANNOT_MEET_FS_REQUIREMENTS);
    EXPECT_EQ(gpcSet2.GetGroupElementSet()->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(gpcSet2.GetElementSet(0, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet2.GetElementSet(1, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet2.GetElementSet(2, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet2.GetElementSet(3, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet2.GetElementSet(4, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x1U);
    EXPECT_EQ(gpcSet2.GetElementSet(5, FsUnit::TPC)->GetTotalDisableMask()->GetMask(), 0x0U);
}
