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

#include "downbinelementpickers.h"
#include "ga10xdownbinimpl.h"

using namespace Downbin;

//------------------------------------------------------------------------------
GA10xDownbinImpl::GA10xDownbinImpl(TestDevice *pDev)
    : DownbinImpl(pDev)
{
}

//------------------------------------------------------------------------------
RC GA10xDownbinImpl::AddChipFsRules()
{
    // TODO
    return RC::OK;
}

//-------------------------------------------------------------------------------
RC GA10xDownbinImpl::AddPesEDPickers()
{
    // Add element disable picker to choose TPCs corresponding to PESs with least number of 
    // disabled TPCs
    FsSet &gpcSet  = m_FsSets.at(FsUnit::GPC);
    unique_ptr<DownbinElementPicker> pElementPicker =
        make_unique<MostEnabledSubElementPicker>(&gpcSet, FsUnit::PES, FsUnit::TPC);
    gpcSet.AddElementDisablePicker(FsUnit::TPC, move(pElementPicker));
    return RC::OK;
}

RC GA10xDownbinImpl::AddGpcSetGroupDisablePickers(const string &skuName)
{
    RC rc;
    FsSet &gpcSet  = m_FsSets.at(FsUnit::GPC);
    // LEVEL0 
    // Protected bits pickers already added in e AddDownbinPickers(parent function)

    // LEVEL1
    // group disable picker for primary element in GPC set
    // Pick the GPCs that have the min number of primary elements enabled
    CHECK_RC(AddMostDisabledGDPickers(&gpcSet, gpcSet.GetPrimaryFsUnit()));

    // LEVEL2

    // LEVEL3
    // Add basic group disable pickers for non-primary elements
    // Pick the GPCs that have the min number of non-primary elements enabled
    vector<FsUnit> elementFsUnits;
    gpcSet.GetElementFsUnits(&elementFsUnits);
    for (FsUnit element : elementFsUnits)
    {
        if (element != gpcSet.GetPrimaryFsUnit())
        {
            CHECK_RC(AddMostDisabledGDPickers(&gpcSet, element));
        }
    }

    return rc;
}

RC GA10xDownbinImpl::AddGpcSetGroupReducePickers(const string &skuName)
{
    RC rc;
    FsSet &gpcSet  = m_FsSets.at(FsUnit::GPC);

    // LEVEL0

    // LEVEL1
    // Add group reduce picker to pick the GPC with the least number of disabled TPCs
    CHECK_RC(AddMostAvailableGRPickers(&gpcSet, gpcSet.GetPrimaryFsUnit()));
    // Add group reduce pickers for protected bits
    // Picks the GPC with least no of protected TPC
    CHECK_RC(AddProtectedBitsGRPickers(&gpcSet, skuName));

    // LEVEL2
    // Add group reduce pickers for all the non-primary elements of gpcSet
    // Pick the elements that GPC which has most number of elements
    // available for downbinning
    vector<FsUnit> elementFsUnits;
    gpcSet.GetElementFsUnits(&elementFsUnits);
    for (FsUnit element : elementFsUnits)
    {
        if (element != gpcSet.GetPrimaryFsUnit())
        {

            CHECK_RC(AddMostAvailableGRPickers(&gpcSet, element));
        }
    }

    // LEVEL3

    return rc;
}

RC GA10xDownbinImpl::AddGpcSetElementDisablePickers(const string &skuName)
{
    RC rc;
    FsSet &gpcSet  = m_FsSets.at(FsUnit::GPC);
    // LEVEL0
    // Add pickers for selecting non-protected elements in GPC set
    CHECK_RC(AddProtectedBitsEDPickers(&gpcSet, skuName));

    // LEVEL1
    // Add pickers for selecting TPCs corresponding to PESs with least number of disabled TPCs
    CHECK_RC(AddPesEDPickers());

    // LEVEL2

    // LEVEL3
    return rc;
}

RC GA10xDownbinImpl::AddFbpSetGroupDisablePickers(const string &skuName)
{
    RC rc;
    FsSet &fbpSet  = m_FsSets.at(FsUnit::FBP);
    // LEVEL0
    // Protected bits picker already added in the AddDownbinPickers(parent function)
    //

    // LEVEL1
    // group disable picker for primary element in fbp set
    // Pick the fbps that have the min number of primary elements enabled
    CHECK_RC(AddMostDisabledGDPickers(&fbpSet, fbpSet.GetPrimaryFsUnit()));

    // LEVEL2

    // LEVEL3
    // Add basic group disable pickers for non-primary elements
    // Pick the fbps that have the min number of non-primary elements enabled
    vector<FsUnit> elementFsUnits;
    fbpSet.GetElementFsUnits(&elementFsUnits);
    for (FsUnit element : elementFsUnits)
    {
        if (element != fbpSet.GetPrimaryFsUnit())
        {
            CHECK_RC(AddMostDisabledGDPickers(&fbpSet, element));
        }
    }
    return rc;
}
RC GA10xDownbinImpl::AddFbpSetGroupReducePickers(const string &skuName)
{
    RC rc;
    FsSet &fbpSet  = m_FsSets.at(FsUnit::FBP);

    // LEVEL0

    // LEVEL1
    // Add group reduce picker to pick the FBP with the least number of disabled LTCs
    CHECK_RC(AddMostAvailableGRPickers(&fbpSet, fbpSet.GetPrimaryFsUnit()));
    // Add group reduce pickers for protected bits
    // Picks the FBP with least no of protected LTCs/L2SLICES
    CHECK_RC(AddProtectedBitsGRPickers(&fbpSet, skuName));

    // LEVEL2
    // Add group reduce pickers for all the non-primary elements of fbpSet
    // Pick the elements from  that FBP which has most number of elements
    // available for downbinning
    vector<FsUnit> elementFsUnits;
    fbpSet.GetElementFsUnits(&elementFsUnits);
    for (FsUnit element : elementFsUnits)
    {
        if (element != fbpSet.GetPrimaryFsUnit())
        {
            CHECK_RC(AddMostAvailableGRPickers(&fbpSet, element));
        }
    }

    // LEVEL3
    // Add group reduce pickers for selecting FBP based on L2SLICES for diabling
    // LTC
    CHECK_RC(AddLtcL2SliceDependencyGRPicker());
    return rc;
}
RC GA10xDownbinImpl::AddFbpSetElementDisablePickers(const string &skuName)
{
    RC rc;
    FsSet &fbpSet  = m_FsSets.at(FsUnit::FBP);
    // LEVEL0
    // Add pickers for selecting non-protected elements in GPC set
    CHECK_RC(AddProtectedBitsEDPickers(&fbpSet, skuName));


    // LEVEL1
    // Pick the LTC with most disabled L2 slices (and without any protected L2 slices))
    CHECK_RC(AddLtcL2SliceDependencyEDPicker());

    // LEVEL2

    // LEVEL3
    return rc;
}
//------------------------------------------------------------------------------
RC GA10xDownbinImpl::ApplyPostDownbinRules(const string& skuName)
{
    RC rc;

    // Adjust L2 slice masks

    FuseUtil::DownbinConfig sliceConfig;
    CHECK_RC(GetUnitDownbinConfig(skuName, FsUnit::L2_SLICE, &sliceConfig));

    if (m_FsSets.find(FsUnit::FBP) == m_FsSets.end())
    {
        Printf(Tee::PriError, "FBP set not found. Can't adjust L2 slice masks\n");
        return RC::SOFTWARE_ERROR;
    }

    FsSet &fbpSet = m_FsSets.at(FsUnit::FBP);
    UINT32 maxFbps         = fbpSet.GetNumGroups();
    UINT32 maxLtcsPerFbp   = fbpSet.GetNumElementsPerGroup(FsUnit::LTC);
    UINT32 maxSlicesPerFbp = fbpSet.GetNumElementsPerGroup(FsUnit::L2_SLICE);
    UINT32 maxSlicesPerLtc = maxSlicesPerFbp / maxLtcsPerFbp;
    UINT32 skuSliceEnCount = sliceConfig.enableCountPerGroup;
    if (skuSliceEnCount != FuseUtil::UNKNOWN_ENABLE_COUNT)
    {
        if (skuSliceEnCount > maxSlicesPerLtc)
        {
            Printf(Tee::PriError, "L2 slices requested per LTC (%u) greater than max possible (%u)\n",
                                   skuSliceEnCount, maxSlicesPerLtc);
            return RC::SOFTWARE_ERROR;
        }
    }

    bool bIsConsistent = false;
    while (!bIsConsistent)
    {
        UINT32 minEnabledCount = GetMinEnabledSliceCount(fbpSet, maxFbps, maxLtcsPerFbp, maxSlicesPerLtc);

        if (minEnabledCount > 4)
        {
            Printf(Tee::PriError, "Can't handle min slice count of %u\n", minEnabledCount);
            return RC::SOFTWARE_ERROR;
        }

        if (minEnabledCount == 4)
        {
            // All l2 slices enabled per active LTC, which is functionally valid
            bIsConsistent = true;
        }
        if (skuSliceEnCount == 3 ||
            (!bIsConsistent && (skuSliceEnCount == FuseUtil::UNKNOWN_ENABLE_COUNT) && minEnabledCount >= 3))
        {
            // Try to fit the 3 slice pattern
            CHECK_RC(TryApply3SliceMode(&fbpSet,
                                        maxFbps, maxLtcsPerFbp, maxSlicesPerLtc,
                                        &bIsConsistent));
        }
        if (skuSliceEnCount == 2 ||
            (!bIsConsistent && (skuSliceEnCount == FuseUtil::UNKNOWN_ENABLE_COUNT) && minEnabledCount >= 2))
        {
            // Try to fit the 2 slice pattern
            CHECK_RC(TryApply2SliceMode(&fbpSet,
                                        maxFbps, maxLtcsPerFbp, maxSlicesPerLtc,
                                        &bIsConsistent));
        }

        if (!bIsConsistent)
        {
            // Disable one LTC and try again
            //  - This should disable the LTC with most disabled slices
            //    (based on downbinning rules)
            fbpSet.DisableOneElement(FsUnit::LTC);
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
void GA10xDownbinImpl::GetL2SliceMasks
(
    const FsSet& fbpSet,
    UINT32 maxFbps,
    UINT32 maxLtcsPerFbp,
    UINT32 maxSlicesPerLtc,
    vector<vector<UINT32>> *pL2SliceMasks
)
{
    RC rc;
    MASSERT(pL2SliceMasks);
    MASSERT(pL2SliceMasks->size() == maxFbps);

    UINT32 fullLtcSliceMask = (1 << maxSlicesPerLtc) - 1;
    for (UINT32 fbpIdx = 0; fbpIdx < maxFbps; fbpIdx++)
    {
        MASSERT((*pL2SliceMasks)[fbpIdx].size() == maxLtcsPerFbp);
        UINT32 fbpSliceMask = fbpSet.GetElementSet(fbpIdx, FsUnit::L2_SLICE)->GetTotalDisableMask()->GetMask();
        for (UINT32 ltcIdx = 0; ltcIdx < maxLtcsPerFbp; ltcIdx++)
        {
            (*pL2SliceMasks)[fbpIdx][ltcIdx] = (fbpSliceMask >> (ltcIdx * maxSlicesPerLtc)) &
                                                fullLtcSliceMask;
        }
    }
}

//------------------------------------------------------------------------------
UINT32 GA10xDownbinImpl::GetMinEnabledSliceCount
(
    const FsSet& fbpSet,
    UINT32 maxFbps,
    UINT32 maxLtcsPerFbp,
    UINT32 maxSlicesPerLtc
)
{
    RC rc;
    vector<vector<UINT32>> l2SliceMasks(maxFbps, vector<UINT32>(maxLtcsPerFbp));

    UINT32 minEnabledSlices = UINT32_MAX;
    GetL2SliceMasks(fbpSet, maxFbps, maxLtcsPerFbp, maxSlicesPerLtc, &l2SliceMasks);
    for (const auto& fbpSliceMasks : l2SliceMasks)
    {
        for (const auto& ltcSliceMask : fbpSliceMasks)
        {
            UINT32 numDisabledSlices = static_cast<UINT32>(Utility::CountBits(ltcSliceMask));
            UINT32 numEnabledSlices = maxSlicesPerLtc - numDisabledSlices;
            if (numEnabledSlices != 0 && (numEnabledSlices < minEnabledSlices))
            {
                minEnabledSlices = numEnabledSlices;
            }
        }
    }
    return minEnabledSlices;
}

//------------------------------------------------------------------------------
RC GA10xDownbinImpl::TryApplyL2SliceMasks
(
    FsSet *pFbpSet,
    UINT32 maxSlicesPerLtc,
    const vector<vector<UINT32>> &oldL2SliceMasks,
    const vector<vector<UINT32>> &newL2SliceMasks,
    bool *pMasksApplied
)
{
    RC rc;
    MASSERT(pFbpSet);

    size_t maxFbps = oldL2SliceMasks.size();
    size_t maxLtcsPerFbp = 0;
    MASSERT(oldL2SliceMasks.size() == newL2SliceMasks.size());

    *pMasksApplied = false;
    for (UINT32 fbpIdx = 0; fbpIdx < maxFbps; fbpIdx++)
    {
        if (fbpIdx == 0)
        {
            maxLtcsPerFbp = oldL2SliceMasks[0].size();
        }
        MASSERT(oldL2SliceMasks[fbpIdx].size() == maxLtcsPerFbp);
        MASSERT(newL2SliceMasks[fbpIdx].size() == maxLtcsPerFbp);
        for (UINT32 ltcIdx = 0; ltcIdx < maxLtcsPerFbp; ltcIdx++)
        {
            // If the old masks have something disabled that isn't disabled in the
            // new mask, this pattern does not include all defective slices
            // i.e, 1 -> 0
            if (oldL2SliceMasks[fbpIdx][ltcIdx] & ~newL2SliceMasks[fbpIdx][ltcIdx])
            {
                return RC::OK;
            }
        }
    }

    // If we reached here, the new pattern can be applied
    for (UINT32 fbpIdx = 0; fbpIdx < maxFbps; fbpIdx++)
    {
        UINT32 newFbpSliceMask = 0;
        for (UINT32 ltcIdx = 0; ltcIdx < maxLtcsPerFbp; ltcIdx++)
        {
            newFbpSliceMask |= newL2SliceMasks[fbpIdx][ltcIdx] << (ltcIdx * maxSlicesPerLtc);
        }
        CHECK_RC(pFbpSet->DisableElements(FsUnit::L2_SLICE, fbpIdx, newFbpSliceMask));
    }
    *pMasksApplied = true;
    return RC::OK;
}

//------------------------------------------------------------------------------
RC GA10xDownbinImpl::GetPivotL2Slice
(
    const vector<vector<UINT32>> &l2SliceMasks,
    UINT32 maxSlicesPerLtc,
    UINT32 *pPivotFbp,
    UINT32 *pPivotLtc,
    UINT32 *pPivotSlice
) const
{
    MASSERT(pPivotFbp);
    MASSERT(pPivotLtc);
    MASSERT(pPivotSlice);

    *pPivotFbp = s_IlwalidIndex;
    *pPivotLtc = s_IlwalidIndex;
    *pPivotSlice = s_IlwalidIndex;

    UINT32 fullSliceMask = (1 << maxSlicesPerLtc) - 1;
    UINT32 fbpIdx = 0;
    for (const auto& fbpSliceMasks : l2SliceMasks)
    {
        UINT32 ltcIdx = 0;
        for (const auto& ltcSliceMask : fbpSliceMasks)
        {
            // If LTC is not fully disabled
            if (ltcSliceMask != fullSliceMask)
            {
                // If LTC has a disabled slice
                if (ltcSliceMask != 0)
                {
                    // Found enabled LTC with defective L2 slice
                    *pPivotFbp = fbpIdx;
                    *pPivotLtc = ltcIdx;
                    *pPivotSlice = static_cast<UINT32>(Utility::BitScanForward(ltcSliceMask));
                    return RC::OK;
                }
                else if (*pPivotFbp == s_IlwalidIndex)
                {
                    // If we don't find any enabled LTC with a defective slice,
                    // we can return the first slice in an enabled LTC as the pivot
                    *pPivotFbp = fbpIdx;
                    *pPivotLtc = ltcIdx;
                    *pPivotSlice = 0;
                }
            }
            ltcIdx++;
        }
        fbpIdx++;
    }

    if (*pPivotFbp == s_IlwalidIndex)
    {
        Printf(Tee::PriError, "No pivot L2 slice found\n");
        return RC::SOFTWARE_ERROR;
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
RC GA10xDownbinImpl::TryApply3SliceMode
(
    FsSet *pFbpSet,
    UINT32 maxFbps,
    UINT32 maxLtcsPerFbp,
    UINT32 maxSlicesPerLtc,
    bool *pMasksApplied
)
{
    RC rc;
    MASSERT(pFbpSet);
    MASSERT(maxLtcsPerFbp == 2);

    Printf(Tee::PriLow, "Trying 3 slice mode\n");

    // See //hw/doc/gpu/ampere/ampere/design/Functional_Descriptions/Floorsweeping/GA10x_FS_POR.pptx
    //
    // The 3 slice mode has a mirror pattern per FBP sliding as we move across
    // i.e, FBP0 - 0b0001 , 0b1000
    //      FBP1 - 0b0010 , 0b0100
    //      FBP2 - 0b0100 , 0b0010
    //      and so on ..
    // So we essentially have 4 patterns across the FBPs with logical FBP0 starting as
    // 0001, 0010, 0100 or 1000 and the above pattern continuing across the FBPs
    //
    //  - In case a FBP is floorswept,
    //    The pattern shifts over the disabled FBP
    //  - In case a LTC is floorswept,
    //    The pattern is unchanged, and the disabled LTC is just masked off

    vector<vector<UINT32>> lwrrL2SliceMasks(maxFbps, vector<UINT32>(maxLtcsPerFbp));
    vector<vector<UINT32>> newL2SliceMasks(maxFbps, vector<UINT32>(maxLtcsPerFbp));

    const FsMask* pFbpMask = pFbpSet->GetGroupElementSet()->GetTotalDisableMask();
    GetL2SliceMasks(*pFbpSet, maxFbps, maxLtcsPerFbp, maxSlicesPerLtc, &lwrrL2SliceMasks);

    // Find pivot slice
    UINT32 pivotFbp, pivotLtc, pivotSlice;
    CHECK_RC(GetPivotL2Slice(lwrrL2SliceMasks, maxSlicesPerLtc,
                             &pivotFbp, &pivotLtc, &pivotSlice));
    UINT32 maxIdx       = maxSlicesPerLtc - 1;
    UINT32 pivotLtc0Idx = (pivotLtc == 0) ? pivotSlice : maxIdx - pivotSlice;
    UINT32 pivotLtc1Idx = maxIdx - pivotLtc0Idx;

    // Populate l2 slice masks for starting FBP and FBPs to the left of it (<-)
    UINT32 ltc0Idx = pivotLtc0Idx;
    UINT32 ltc1Idx = pivotLtc1Idx;
    UINT32 fullSliceMask = (1 << maxSlicesPerLtc) - 1;
    for (UINT32 fbp = pivotFbp; fbp != ~0U; fbp--)
    {
        const FsMask *pLtcMask = pFbpSet->GetElementSet(fbp, FsUnit::LTC)->GetTotalDisableMask();
        newL2SliceMasks[fbp][0] = !pLtcMask->IsSet(0) ? (1 << ltc0Idx) : fullSliceMask;
        newL2SliceMasks[fbp][1] = !pLtcMask->IsSet(1) ? (1 << ltc1Idx) : fullSliceMask;

        // Roll over disabled FBPs
        if (!pFbpMask->IsSet(fbp))
        {
            ltc0Idx = (ltc0Idx == 0) ? maxIdx : (ltc0Idx - 1);
            ltc1Idx = maxIdx - ltc0Idx;
        }
    }

    // Populate l2 slice masks for FBPs to the right of the starting FBP (->)
    ltc0Idx = pivotLtc0Idx;
    ltc1Idx = pivotLtc1Idx;
    for (UINT32 fbp = pivotFbp + 1; fbp < maxFbps; fbp++)
    {
        // Roll over disabled FBPs
        if (!pFbpMask->IsSet(fbp))
        {
            ltc0Idx = (ltc0Idx == maxIdx) ? 0 : (ltc0Idx + 1);
            ltc1Idx = maxIdx - ltc0Idx;
        }

        const FsMask *pLtcMask = pFbpSet->GetElementSet(fbp, FsUnit::LTC)->GetTotalDisableMask();
        newL2SliceMasks[fbp][0] = !pLtcMask->IsSet(0) ? (1 << ltc0Idx) : fullSliceMask;
        newL2SliceMasks[fbp][1] = !pLtcMask->IsSet(1) ? (1 << ltc1Idx) : fullSliceMask;
    }

    CHECK_RC(TryApplyL2SliceMasks(pFbpSet, maxSlicesPerLtc,
                                  lwrrL2SliceMasks, newL2SliceMasks, pMasksApplied));
    return rc;
}

//------------------------------------------------------------------------------
RC GA10xDownbinImpl::TryApply2SliceMode
(
    FsSet *pFbpSet,
    UINT32 maxFbps,
    UINT32 maxLtcsPerFbp,
    UINT32 maxSlicesPerLtc,
    bool *pMasksApplied
)
{
    RC rc;
    MASSERT(pFbpSet);
    MASSERT(pMasksApplied);
    MASSERT(maxLtcsPerFbp == 2);

    Printf(Tee::PriLow, "Trying 2 slice mode\n");

    // See //hw/doc/gpu/ampere/ampere/design/Functional_Descriptions/Floorsweeping/GA10x_FS_POR.pptx
    //
    // The 2 slice mode has only 2 specific patterns allowed
    //   - LTC0 = 0b1100 (i.e. 0xC), LTC1 = 0b0011 (i.e, 0x3) in all FBPs, OR
    //   - LTC0 = 0b0011 (i.e, 0x3), LTC1 = 0b1100 (i.e, 0xC) in all FBPs
    //
    // - In case a FBP is floorswept,
    //   The pattern should be unaffected
    // - In case a LTC is floorswept,
    //   Assuming the disabled LTC can just be masked off as is lwrrently

    vector<vector<UINT32>> lwrrL2SliceMasks(maxFbps, vector<UINT32>(maxLtcsPerFbp));
    vector<vector<UINT32>> newL2SliceMasks(maxFbps, vector<UINT32>(maxLtcsPerFbp));

    GetL2SliceMasks(*pFbpSet, maxFbps, maxLtcsPerFbp, maxSlicesPerLtc, &lwrrL2SliceMasks);

    // Find pivot slice
    UINT32 pivotFbp, pivotLtc, pivotSlice;
    CHECK_RC(GetPivotL2Slice(lwrrL2SliceMasks, maxSlicesPerLtc,
                             &pivotFbp, &pivotLtc, &pivotSlice));

    UINT32 ltc0Mask = 0xC;
    UINT32 ltc1Mask = 0x3;
    UINT32 fullSliceMask = (1 << maxSlicesPerLtc) - 1;
    if (((pivotLtc == 0) && (pivotSlice <= 1)) ||
        ((pivotLtc == 1) && (pivotSlice > 1)))
    {
        ltc0Mask = 0x3;
        ltc1Mask = 0xC;
    }
    for (UINT32 fbp = 0; fbp < maxFbps; fbp++)
    {
        const FsMask *pLtcMask = pFbpSet->GetElementSet(fbp, FsUnit::LTC)->GetTotalDisableMask();
        newL2SliceMasks[fbp][0] = !pLtcMask->IsSet(0) ? ltc0Mask : fullSliceMask;
        newL2SliceMasks[fbp][1] = !pLtcMask->IsSet(1) ? ltc1Mask : fullSliceMask;
    }

    CHECK_RC(TryApplyL2SliceMasks(pFbpSet, maxSlicesPerLtc,
                                  lwrrL2SliceMasks, newL2SliceMasks, pMasksApplied));
    return rc;
}

RC GA10xDownbinImpl::AddMiscDownbinPickers()
{
    return RC::OK;
}
