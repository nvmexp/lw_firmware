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

#include "gpu/fuse/downbin/ga10xdownbinimpl.h"
#include "gpu/fuse/downbin/fselement.h"
#include "gpu/fuse/downbin/fsgroup.h"
#include "gpu/fuse/downbin/fsset.h"

#include "mockfsset.h"
#include "mocksubdevice.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace Downbin;

class GA10xDownbinImplTest : public GA10xDownbinImpl
{
public:
    GA10xDownbinImplTest(TestDevice *pDev)
        : GA10xDownbinImpl(pDev)
    {}

    RC TryApply3SliceMode
    (
        FsSet *pFbpSet,
        UINT32 maxFbps,
        UINT32 maxLtcsPerFbp,
        UINT32 maxSlicesPerLtc,
        bool *pMasksApplied
    )
    {
        return GA10xDownbinImpl::TryApply3SliceMode(pFbpSet,
                maxFbps, maxLtcsPerFbp, maxSlicesPerLtc, pMasksApplied);
    }

    RC TryApply2SliceMode
    (
        FsSet *pFbpSet,
        UINT32 maxFbps,
        UINT32 maxLtcsPerFbp,
        UINT32 maxSlicesPerLtc,
        bool *pMasksApplied
    )
    {
        return GA10xDownbinImpl::TryApply2SliceMode(pFbpSet,
                maxFbps, maxLtcsPerFbp, maxSlicesPerLtc, pMasksApplied);
    }
};

//------------------------------------------------------------------------------
/*

// The next couple of functions are private, so can't be unit tested directly
// However, keeping these sections of code for now since they were useful
// in debugging the code during development

TEST(GA10xDownbinImpl, GetPivotL2Slice)
{
    MockGpuSubdevice mockDev;
    GA10xDownbinImplTest downbinImpl(mockDev.GetDevice());

    UINT32 pivotFbp, pivotLtc, pivotSlice;

    vector<vector<UINT32>> slices1 = { {0x0, 0x0}, {0x0, 0x0}, {0x0, 0x0} };
    UINT32 slicesPerLtc1 = 4;
    downbinImpl.GetPivotL2Slice(slices1, slicesPerLtc1, &pivotFbp, &pivotLtc, &pivotSlice);
    EXPECT_EQ(pivotFbp, 0x0U);
    EXPECT_EQ(pivotLtc, 0x0U);
    EXPECT_EQ(pivotSlice, 0x0U);

    vector<vector<UINT32>> slices2 = { {0x0, 0x1}, {0x0, 0x0}, {0x0, 0x0} };
    UINT32 slicesPerLtc2 = 4;
    downbinImpl.GetPivotL2Slice(slices2, slicesPerLtc2, &pivotFbp, &pivotLtc, &pivotSlice);
    EXPECT_EQ(pivotFbp, 0x0U);
    EXPECT_EQ(pivotLtc, 0x1U);
    EXPECT_EQ(pivotSlice, 0x0U);

    vector<vector<UINT32>> slices3 = { {0xF, 0x1}, {0x0, 0x0}, {0x0, 0x0} };
    UINT32 slicesPerLtc3 = 4;
    downbinImpl.GetPivotL2Slice(slices3, slicesPerLtc3, &pivotFbp, &pivotLtc, &pivotSlice);
    EXPECT_EQ(pivotFbp, 0x0U);
    EXPECT_EQ(pivotLtc, 0x1U);
    EXPECT_EQ(pivotSlice, 0x0U);

    vector<vector<UINT32>> slices4 = { {0xF, 0x1}, {0x0, 0x0}, {0x0, 0x0} };
    UINT32 slicesPerLtc4 = 5;
    downbinImpl.GetPivotL2Slice(slices4, slicesPerLtc4, &pivotFbp, &pivotLtc, &pivotSlice);
    EXPECT_EQ(pivotFbp, 0x0U);
    EXPECT_EQ(pivotLtc, 0x0U);
    EXPECT_EQ(pivotSlice, 0x0U);

    vector<vector<UINT32>> slices5 = { {0xF, 0xF}, {0x0, 0x0}, {0x4, 0x0} };
    UINT32 slicesPerLtc5 = 4;
    downbinImpl.GetPivotL2Slice(slices5, slicesPerLtc5, &pivotFbp, &pivotLtc, &pivotSlice);
    EXPECT_EQ(pivotFbp, 0x2U);
    EXPECT_EQ(pivotLtc, 0x0U);
    EXPECT_EQ(pivotSlice, 0x2U);
}

//------------------------------------------------------------------------------
TEST(GA10xDownbinImpl, GetMinEnabledSliceCount)
{
    MockGpuSubdevice mockDev;
    GA10xDownbinImplTest downbinImpl(mockDev.GetDevice());

    UINT32 minSliceCount;

    UINT32 maxFbps1 = 3;
    UINT32 maxLtcsPerFbp1 = 2;
    UINT32 maxSlicesPerLtc1 = 4;

    // 1 LTC with 1 slice disabled
    MockFsSet fbpSet1(FsUnit::FBP, maxFbps1);
    fbpSet1.AddElement(FsUnit::LTC, maxLtcsPerFbp1, true, false);
    fbpSet1.AddElement(FsUnit::L2_SLICE, maxSlicesPerLtc1 * maxLtcsPerFbp1, false, false);
    fbpSet1.DisableElements(FsUnit::L2_SLICE, 1, 0x1);
    minSliceCount = downbinImpl.GetMinEnabledSliceCount(fbpSet1,
                                maxFbps1, maxLtcsPerFbp1, maxSlicesPerLtc1);
    EXPECT_EQ(minSliceCount, 0x3U);

    // 1 LTC with 2 slices disabled
    MockFsSet fbpSet2(FsUnit::FBP, maxFbps1);
    fbpSet2.AddElement(FsUnit::LTC, maxLtcsPerFbp1, true, false);
    fbpSet2.AddElement(FsUnit::L2_SLICE, maxSlicesPerLtc1 * maxLtcsPerFbp1, false, false);
    fbpSet2.DisableElements(FsUnit::L2_SLICE, 1, 0x5);
    minSliceCount = downbinImpl.GetMinEnabledSliceCount(fbpSet2,
                                maxFbps1, maxLtcsPerFbp1, maxSlicesPerLtc1);
    EXPECT_EQ(minSliceCount, 0x2U);

    // 2 LTCs with 1 slice disabled (from same FBP)
    MockFsSet fbpSet3(FsUnit::FBP, maxFbps1);
    fbpSet3.AddElement(FsUnit::LTC, maxLtcsPerFbp1, true, false);
    fbpSet3.AddElement(FsUnit::L2_SLICE, maxSlicesPerLtc1 * maxLtcsPerFbp1, false, false);
    fbpSet3.DisableElements(FsUnit::L2_SLICE, 2, 0x41);
    minSliceCount = downbinImpl.GetMinEnabledSliceCount(fbpSet3,
                                maxFbps1, maxLtcsPerFbp1, maxSlicesPerLtc1);
    EXPECT_EQ(minSliceCount, 0x3U);

    // 2 LTCs with 1 slice disabled (from different FBP)
    MockFsSet fbpSet4(FsUnit::FBP, maxFbps1);
    fbpSet4.AddElement(FsUnit::LTC, maxLtcsPerFbp1, true, false);
    fbpSet4.AddElement(FsUnit::L2_SLICE, maxSlicesPerLtc1 * maxLtcsPerFbp1, false, false);
    fbpSet4.DisableElements(FsUnit::L2_SLICE, 0, 0x40);
    fbpSet4.DisableElements(FsUnit::L2_SLICE, 2, 0x1);
    minSliceCount = downbinImpl.GetMinEnabledSliceCount(fbpSet4,
                                maxFbps1, maxLtcsPerFbp1, maxSlicesPerLtc1);
    EXPECT_EQ(minSliceCount, 0x3U);

    // 1 LTC with 1 slice disabled, 1 with 2 slices disabled
    MockFsSet fbpSet5(FsUnit::FBP, maxFbps1);
    fbpSet5.AddElement(FsUnit::LTC, maxLtcsPerFbp1, true, false);
    fbpSet5.AddElement(FsUnit::L2_SLICE, maxSlicesPerLtc1 * maxLtcsPerFbp1, false, false);
    fbpSet5.DisableElements(FsUnit::L2_SLICE, 1, 0x60);
    fbpSet5.DisableElements(FsUnit::L2_SLICE, 2, 0x8);
    minSliceCount = downbinImpl.GetMinEnabledSliceCount(fbpSet5,
                                maxFbps1, maxLtcsPerFbp1, maxSlicesPerLtc1);
    EXPECT_EQ(minSliceCount, 0x2U);

    // 1 LTC with 1 slice disabled, 1 entire LTC disabled (from same FBP)
    MockFsSet fbpSet6(FsUnit::FBP, maxFbps1);
    fbpSet6.AddElement(FsUnit::LTC, maxLtcsPerFbp1, true, false);
    fbpSet6.AddElement(FsUnit::L2_SLICE, maxSlicesPerLtc1 * maxLtcsPerFbp1, false, false);
    fbpSet6.DisableElements(FsUnit::L2_SLICE, 1, 0x2F);
    minSliceCount = downbinImpl.GetMinEnabledSliceCount(fbpSet6,
                                maxFbps1, maxLtcsPerFbp1, maxSlicesPerLtc1);
    EXPECT_EQ(minSliceCount, 0x3U);

    // 1 LTC with 1 slice disabled, 1 entire LTC disabled (from different FBPs)
    MockFsSet fbpSet7(FsUnit::FBP, maxFbps1);
    fbpSet7.AddElement(FsUnit::LTC, maxLtcsPerFbp1, true, false);
    fbpSet7.AddElement(FsUnit::L2_SLICE, maxSlicesPerLtc1 * maxLtcsPerFbp1, false, false);
    fbpSet7.DisableElements(FsUnit::L2_SLICE, 1, 0xF);
    fbpSet7.DisableElements(FsUnit::L2_SLICE, 2, 0x20);
    minSliceCount = downbinImpl.GetMinEnabledSliceCount(fbpSet7,
                                maxFbps1, maxLtcsPerFbp1, maxSlicesPerLtc1);
    EXPECT_EQ(minSliceCount, 0x3U);

    // All slices disabled in a FBP
    MockFsSet fbpSet8(FsUnit::FBP, maxFbps1);
    fbpSet8.AddElement(FsUnit::LTC, maxLtcsPerFbp1, true, false);
    fbpSet8.AddElement(FsUnit::L2_SLICE, maxSlicesPerLtc1 * maxLtcsPerFbp1, false, false);
    fbpSet8.DisableElements(FsUnit::L2_SLICE, 1, 0xFF);
    minSliceCount = downbinImpl.GetMinEnabledSliceCount(fbpSet8,
                                maxFbps1, maxLtcsPerFbp1, maxSlicesPerLtc1);
    EXPECT_EQ(minSliceCount, 0x4U);
}
*/

//------------------------------------------------------------------------------
TEST(GA10xDownbinImpl, TryApply3SliceMode)
{
    MockGpuSubdevice mockDev;
    GA10xDownbinImplTest downbinImpl(mockDev.GetDevice());

    bool bMasksApplied;

    UINT32 maxFbps1 = 4;
    UINT32 maxLtcsPerFbp1 = 2;
    UINT32 maxSlicesPerLtc1 = 4;

    // No slice defective
    MockFsSet fbpSet1(FsUnit::FBP, maxFbps1);
    fbpSet1.AddElement(FsUnit::LTC, maxLtcsPerFbp1, true, false);
    fbpSet1.AddElement(FsUnit::L2_SLICE, maxSlicesPerLtc1 * maxLtcsPerFbp1, false, false);
    EXPECT_EQ(downbinImpl.TryApply3SliceMode(&fbpSet1, maxFbps1, maxLtcsPerFbp1, maxSlicesPerLtc1, &bMasksApplied), RC::OK);
    EXPECT_EQ(fbpSet1.GetElementSet(0, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x81U);
    EXPECT_EQ(fbpSet1.GetElementSet(1, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x42U);
    EXPECT_EQ(fbpSet1.GetElementSet(2, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x24U);
    EXPECT_EQ(fbpSet1.GetElementSet(3, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x18U);
    EXPECT_EQ(bMasksApplied, true);

    // FBP0-LTC0-slice1 defective
    MockFsSet fbpSet2(FsUnit::FBP, maxFbps1);
    fbpSet2.AddElement(FsUnit::LTC, maxLtcsPerFbp1, true, false);
    fbpSet2.AddElement(FsUnit::L2_SLICE, maxSlicesPerLtc1 * maxLtcsPerFbp1, false, false);
    fbpSet2.DisableElements(FsUnit::L2_SLICE, 0, 0x2);
    EXPECT_EQ(downbinImpl.TryApply3SliceMode(&fbpSet2, maxFbps1, maxLtcsPerFbp1, maxSlicesPerLtc1, &bMasksApplied), RC::OK);
    EXPECT_EQ(fbpSet2.GetElementSet(0, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x42U);
    EXPECT_EQ(fbpSet2.GetElementSet(1, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x24U);
    EXPECT_EQ(fbpSet2.GetElementSet(2, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x18U);
    EXPECT_EQ(fbpSet2.GetElementSet(3, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x81U);
    EXPECT_EQ(bMasksApplied, true);

    // FBP1-LTC0-slice3 defective
    MockFsSet fbpSet3(FsUnit::FBP, maxFbps1);
    fbpSet3.AddElement(FsUnit::LTC, maxLtcsPerFbp1, true, false);
    fbpSet3.AddElement(FsUnit::L2_SLICE, maxSlicesPerLtc1 * maxLtcsPerFbp1, false, false);
    fbpSet3.DisableElements(FsUnit::L2_SLICE, 1, 0x8);
    EXPECT_EQ(downbinImpl.TryApply3SliceMode(&fbpSet3, maxFbps1, maxLtcsPerFbp1, maxSlicesPerLtc1, &bMasksApplied), RC::OK);
    EXPECT_EQ(fbpSet3.GetElementSet(0, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x24U);
    EXPECT_EQ(fbpSet3.GetElementSet(1, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x18U);
    EXPECT_EQ(fbpSet3.GetElementSet(2, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x81U);
    EXPECT_EQ(fbpSet3.GetElementSet(3, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x42U);
    EXPECT_EQ(bMasksApplied, true);

    // FBP0-LTC1-slice3 + FBP2-LTC1-slice1 defective -> still fits 3 slice mode
    MockFsSet fbpSet4(FsUnit::FBP, maxFbps1);
    fbpSet4.AddElement(FsUnit::LTC, maxLtcsPerFbp1, true, false);
    fbpSet4.AddElement(FsUnit::L2_SLICE, maxSlicesPerLtc1 * maxLtcsPerFbp1, false, false);
    fbpSet4.DisableElements(FsUnit::L2_SLICE, 0, 0x80);
    fbpSet4.DisableElements(FsUnit::L2_SLICE, 2, 0x20);
    EXPECT_EQ(downbinImpl.TryApply3SliceMode(&fbpSet4, maxFbps1, maxLtcsPerFbp1, maxSlicesPerLtc1, &bMasksApplied), RC::OK);
    EXPECT_EQ(fbpSet4.GetElementSet(0, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x81U);
    EXPECT_EQ(fbpSet4.GetElementSet(1, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x42U);
    EXPECT_EQ(fbpSet4.GetElementSet(2, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x24U);
    EXPECT_EQ(fbpSet4.GetElementSet(3, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x18U);
    EXPECT_EQ(bMasksApplied, true);

    // FBP0-LTC1-slice3 + FBP2-LTC1-slice2 defective -> can't fit 3 slice mode
    MockFsSet fbpSet5(FsUnit::FBP, maxFbps1);
    fbpSet5.AddElement(FsUnit::LTC, maxLtcsPerFbp1, true, false);
    fbpSet5.AddElement(FsUnit::L2_SLICE, maxSlicesPerLtc1 * maxLtcsPerFbp1, false, false);
    fbpSet5.DisableElements(FsUnit::L2_SLICE, 0, 0x80);
    fbpSet5.DisableElements(FsUnit::L2_SLICE, 2, 0x40);
    EXPECT_EQ(downbinImpl.TryApply3SliceMode(&fbpSet5, maxFbps1, maxLtcsPerFbp1, maxSlicesPerLtc1, &bMasksApplied), RC::OK);
    EXPECT_EQ(fbpSet5.GetElementSet(0, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x80U);
    EXPECT_EQ(fbpSet5.GetElementSet(1, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet5.GetElementSet(2, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x40U);
    EXPECT_EQ(fbpSet5.GetElementSet(3, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(bMasksApplied, false);

    // FBP1 floorswept + FBP2-LTC0-slice0 defective
    MockFsSet fbpSet6(FsUnit::FBP, maxFbps1);
    fbpSet6.AddElement(FsUnit::LTC, maxLtcsPerFbp1, true, false);
    fbpSet6.AddElement(FsUnit::L2_SLICE, maxSlicesPerLtc1 * maxLtcsPerFbp1, false, false);
    fbpSet6.DisableGroup(1);
    fbpSet6.DisableElements(FsUnit::LTC, 1, 0x3);
    fbpSet6.DisableElements(FsUnit::L2_SLICE, 1, 0xFF);
    fbpSet6.DisableElements(FsUnit::L2_SLICE, 2, 0x1);
    EXPECT_EQ(downbinImpl.TryApply3SliceMode(&fbpSet6, maxFbps1, maxLtcsPerFbp1, maxSlicesPerLtc1, &bMasksApplied), RC::OK);
    EXPECT_EQ(fbpSet6.GetElementSet(0, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x18U);
    EXPECT_EQ(fbpSet6.GetElementSet(1, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xFFU);
    EXPECT_EQ(fbpSet6.GetElementSet(2, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x81U);
    EXPECT_EQ(fbpSet6.GetElementSet(3, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x42U);
    EXPECT_EQ(bMasksApplied, true);

    // FBP1-LTC0 floorswept + FBP2-LTC0-slice0 defective
    MockFsSet fbpSet7(FsUnit::FBP, maxFbps1);
    fbpSet7.AddElement(FsUnit::LTC, maxLtcsPerFbp1, true, false);
    fbpSet7.AddElement(FsUnit::L2_SLICE, maxSlicesPerLtc1 * maxLtcsPerFbp1, false, false);
    fbpSet7.DisableElements(FsUnit::LTC, 1, 0x1);
    fbpSet7.DisableElements(FsUnit::L2_SLICE, 1, 0xF);
    fbpSet7.DisableElements(FsUnit::L2_SLICE, 2, 0x1);
    EXPECT_EQ(downbinImpl.TryApply3SliceMode(&fbpSet7, maxFbps1, maxLtcsPerFbp1, maxSlicesPerLtc1, &bMasksApplied), RC::OK);
    EXPECT_EQ(fbpSet7.GetElementSet(0, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x24U);
    EXPECT_EQ(fbpSet7.GetElementSet(1, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x1FU);
    EXPECT_EQ(fbpSet7.GetElementSet(2, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x81U);
    EXPECT_EQ(fbpSet7.GetElementSet(3, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x42U);
    EXPECT_EQ(bMasksApplied, true);
}

//------------------------------------------------------------------------------
TEST(GA10xDownbinImpl, TryApply2SliceMode)
{
    MockGpuSubdevice mockDev;
    GA10xDownbinImplTest downbinImpl(mockDev.GetDevice());

    bool bMasksApplied;

    UINT32 maxFbps1 = 4;
    UINT32 maxLtcsPerFbp1 = 2;
    UINT32 maxSlicesPerLtc1 = 4;

    // No slice defective
    MockFsSet fbpSet1(FsUnit::FBP, maxFbps1);
    fbpSet1.AddElement(FsUnit::LTC, maxLtcsPerFbp1, true, false);
    fbpSet1.AddElement(FsUnit::L2_SLICE, maxSlicesPerLtc1 * maxLtcsPerFbp1, false, false);
    EXPECT_EQ(downbinImpl.TryApply2SliceMode(&fbpSet1, maxFbps1, maxLtcsPerFbp1, maxSlicesPerLtc1, &bMasksApplied), RC::OK);
    EXPECT_EQ(fbpSet1.GetElementSet(0, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xC3U);
    EXPECT_EQ(fbpSet1.GetElementSet(1, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xC3U);
    EXPECT_EQ(fbpSet1.GetElementSet(2, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xC3U);
    EXPECT_EQ(fbpSet1.GetElementSet(3, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xC3U);
    EXPECT_EQ(bMasksApplied, true);

    // FBP0-LTC0-slice1 defective
    MockFsSet fbpSet2(FsUnit::FBP, maxFbps1);
    fbpSet2.AddElement(FsUnit::LTC, maxLtcsPerFbp1, true, false);
    fbpSet2.AddElement(FsUnit::L2_SLICE, maxSlicesPerLtc1 * maxLtcsPerFbp1, false, false);
    fbpSet2.DisableElements(FsUnit::L2_SLICE, 0, 0x2);
    EXPECT_EQ(downbinImpl.TryApply2SliceMode(&fbpSet2, maxFbps1, maxLtcsPerFbp1, maxSlicesPerLtc1, &bMasksApplied), RC::OK);
    EXPECT_EQ(fbpSet2.GetElementSet(0, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xC3U);
    EXPECT_EQ(fbpSet2.GetElementSet(1, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xC3U);
    EXPECT_EQ(fbpSet2.GetElementSet(2, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xC3U);
    EXPECT_EQ(fbpSet2.GetElementSet(3, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xC3U);
    EXPECT_EQ(bMasksApplied, true);

    // FBP1-LTC0-slice3 defective
    MockFsSet fbpSet3(FsUnit::FBP, maxFbps1);
    fbpSet3.AddElement(FsUnit::LTC, maxLtcsPerFbp1, true, false);
    fbpSet3.AddElement(FsUnit::L2_SLICE, maxSlicesPerLtc1 * maxLtcsPerFbp1, false, false);
    fbpSet3.DisableElements(FsUnit::L2_SLICE, 1, 0x8);
    EXPECT_EQ(downbinImpl.TryApply2SliceMode(&fbpSet3, maxFbps1, maxLtcsPerFbp1, maxSlicesPerLtc1, &bMasksApplied), RC::OK);
    EXPECT_EQ(fbpSet3.GetElementSet(0, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x3LW);
    EXPECT_EQ(fbpSet3.GetElementSet(1, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x3LW);
    EXPECT_EQ(fbpSet3.GetElementSet(2, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x3LW);
    EXPECT_EQ(fbpSet3.GetElementSet(3, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x3LW);
    EXPECT_EQ(bMasksApplied, true);

    // FBP0-LTC1-slice3 + FBP2-LTC0-slice1 defective -> still fits 2 slice mode
    MockFsSet fbpSet4(FsUnit::FBP, maxFbps1);
    fbpSet4.AddElement(FsUnit::LTC, maxLtcsPerFbp1, true, false);
    fbpSet4.AddElement(FsUnit::L2_SLICE, maxSlicesPerLtc1 * maxLtcsPerFbp1, false, false);
    fbpSet4.DisableElements(FsUnit::L2_SLICE, 0, 0x80);
    fbpSet4.DisableElements(FsUnit::L2_SLICE, 2, 0x2);
    EXPECT_EQ(downbinImpl.TryApply2SliceMode(&fbpSet4, maxFbps1, maxLtcsPerFbp1, maxSlicesPerLtc1, &bMasksApplied), RC::OK);
    EXPECT_EQ(fbpSet4.GetElementSet(0, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xC3U);
    EXPECT_EQ(fbpSet4.GetElementSet(1, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xC3U);
    EXPECT_EQ(fbpSet4.GetElementSet(2, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xC3U);
    EXPECT_EQ(fbpSet4.GetElementSet(3, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xC3U);
    EXPECT_EQ(bMasksApplied, true);

    // FBP0-LTC1-slice3 + FBP2-LTC1-slice1 defective -> can't fit 2 slice mode
    MockFsSet fbpSet5(FsUnit::FBP, maxFbps1);
    fbpSet5.AddElement(FsUnit::LTC, maxLtcsPerFbp1, true, false);
    fbpSet5.AddElement(FsUnit::L2_SLICE, maxSlicesPerLtc1 * maxLtcsPerFbp1, false, false);
    fbpSet5.DisableElements(FsUnit::L2_SLICE, 0, 0x80);
    fbpSet5.DisableElements(FsUnit::L2_SLICE, 2, 0x20);
    EXPECT_EQ(downbinImpl.TryApply2SliceMode(&fbpSet5, maxFbps1, maxLtcsPerFbp1, maxSlicesPerLtc1, &bMasksApplied), RC::OK);
    EXPECT_EQ(fbpSet5.GetElementSet(0, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x80U);
    EXPECT_EQ(fbpSet5.GetElementSet(1, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(fbpSet5.GetElementSet(2, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x20U);
    EXPECT_EQ(fbpSet5.GetElementSet(3, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0x0U);
    EXPECT_EQ(bMasksApplied, false);

    // FBP1 floorswept + FBP2-LTC0-slice0 defective
    MockFsSet fbpSet6(FsUnit::FBP, maxFbps1);
    fbpSet6.AddElement(FsUnit::LTC, maxLtcsPerFbp1, true, false);
    fbpSet6.AddElement(FsUnit::L2_SLICE, maxSlicesPerLtc1 * maxLtcsPerFbp1, false, false);
    fbpSet6.DisableGroup(1);
    fbpSet6.DisableElements(FsUnit::LTC, 1, 0x3);
    fbpSet6.DisableElements(FsUnit::L2_SLICE, 1, 0xFF);
    fbpSet6.DisableElements(FsUnit::L2_SLICE, 2, 0x1);
    EXPECT_EQ(downbinImpl.TryApply2SliceMode(&fbpSet6, maxFbps1, maxLtcsPerFbp1, maxSlicesPerLtc1, &bMasksApplied), RC::OK);
    EXPECT_EQ(fbpSet6.GetElementSet(0, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xC3U);
    EXPECT_EQ(fbpSet6.GetElementSet(1, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xFFU);
    EXPECT_EQ(fbpSet6.GetElementSet(2, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xC3U);
    EXPECT_EQ(fbpSet6.GetElementSet(3, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xC3U);
    EXPECT_EQ(bMasksApplied, true);

    // FBP1-LTC0 floorswept + FBP2-LTC0-slice0 defective
    MockFsSet fbpSet7(FsUnit::FBP, maxFbps1);
    fbpSet7.AddElement(FsUnit::LTC, maxLtcsPerFbp1, true, false);
    fbpSet7.AddElement(FsUnit::L2_SLICE, maxSlicesPerLtc1 * maxLtcsPerFbp1, false, false);
    fbpSet7.DisableElements(FsUnit::LTC, 1, 0x1);
    fbpSet7.DisableElements(FsUnit::L2_SLICE, 1, 0xF);
    fbpSet7.DisableElements(FsUnit::L2_SLICE, 2, 0x1);
    EXPECT_EQ(downbinImpl.TryApply2SliceMode(&fbpSet7, maxFbps1, maxLtcsPerFbp1, maxSlicesPerLtc1, &bMasksApplied), RC::OK);
    EXPECT_EQ(fbpSet7.GetElementSet(0, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xC3U);
    EXPECT_EQ(fbpSet7.GetElementSet(1, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xCFU);
    EXPECT_EQ(fbpSet7.GetElementSet(2, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xC3U);
    EXPECT_EQ(fbpSet7.GetElementSet(3, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask(), 0xC3U);
    EXPECT_EQ(bMasksApplied, true);
}
