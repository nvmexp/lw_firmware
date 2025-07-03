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

#include "downbingrouppickers.h"
#include "downbinelementpickers.h"
#include "downbinrulesimpl.h"
#include "hopperdownbinimpl.h"

using namespace Downbin;

//------------------------------------------------------------------------------
GH100DownbinImpl::GH100DownbinImpl(TestDevice *pDev)
    : DownbinImpl(pDev)
    , m_pDev(pDev)
{
    MASSERT(pDev != nullptr);
    MASSERT(dynamic_cast<GpuSubdevice*>(pDev) != nullptr);
}

//------------------------------------------------------------------------------
RC GH100DownbinImpl::AddChipFsRules()
{
    RC rc;
    CHECK_RC(AddGpcSetRules());
    CHECK_RC(AddL2NocRules());
    CHECK_RC(AddL2SliceFsRules());
    CHECK_RC(AddSyspipeRules());

    return rc;
}

//------------------------------------------------------------------------------
RC GH100DownbinImpl::AddSyspipeRules()
{
    RC rc;
    MASSERT(m_FsSets.count(FsUnit::SYSPIPE));

    FsSet &syspipeSet = m_FsSets.at(FsUnit::SYSPIPE);
    // Sypipe0 must always be enabled
    FsElementSet *pSyspipeElSet = syspipeSet.GetGroupElementSet();
    shared_ptr<DownbinRule> pRule = make_shared<AlwaysEnableRule>(pSyspipeElSet, 0);
    pSyspipeElSet->AddOnElementDisabledListener(pRule);
    CHECK_RC(pSyspipeElSet->MarkProtected(0));
    CHECK_RC(syspipeSet.GetGroup(0)->MarkProtected());
    return rc;
}

//------------------------------------------------------------------------------
RC GH100DownbinImpl::AddSyspipeDownbinPickers()
{
    RC rc;

    FsSet &syspipeSet = m_FsSets.at(FsUnit::SYSPIPE);
    // Add picker to select only syspipes which are not protected
    unique_ptr<DownbinGroupPicker> pPicker =
        make_unique<MostAvailableGroupPicker>(&syspipeSet, FsUnit::NONE, false,
                false);
    syspipeSet.AddGroupDisablePicker(move(pPicker));
    return rc;
}

RC GH100DownbinImpl::AddMiscDownbinPickers()
{
    RC rc;
    // Add GH100 specific Syspipe pickers
    if (m_FsSets.count(FsUnit::SYSPIPE))
    {
        CHECK_RC(AddSyspipeDownbinPickers());
    }
    // Add GH100 specific LWDEC and LWJPG pickers
    if (m_FsSets.count(FsUnit::LWDEC) && m_FsSets.count(FsUnit::LWJPG))
    {
        CHECK_RC(AddLwdecLwjpgDownbinPickers());
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
RC GH100DownbinImpl::AddLwdecLwjpgDownbinPickers()
{
    // if there's not lwdec / lwjpg FsSet, do nothing
    if (!m_FsSets.count(FsUnit::LWDEC) || !m_FsSets.count(FsUnit::LWJPG))
    {
        return RC::OK;
    }

    FsSet &lwdecFsSet = m_FsSets.at(FsUnit::LWDEC);
    FsSet &lwjpgFsSet = m_FsSets.at(FsUnit::LWJPG);

    unique_ptr<DownbinGroupPicker> pLwdecPicker =
        make_unique<CrossFsSetDependencyGroupPicker>(&lwdecFsSet, &lwjpgFsSet);
    lwdecFsSet.AddGroupDisablePicker(move(pLwdecPicker));

    unique_ptr<DownbinGroupPicker> pLwjpgPicker =
        make_unique<CrossFsSetDependencyGroupPicker>(&lwjpgFsSet, &lwdecFsSet);
    lwjpgFsSet.AddGroupDisablePicker(move(pLwjpgPicker));

    return RC::OK;
}

//------------------------------------------------------------------------------
RC GH100DownbinImpl::DownbinSyspipeSet(const FuseUtil::DownbinConfig &downbinConfig)
{
    RC rc;

    FsSet &gpcSet = m_FsSets.at(FsUnit::GPC);
    FsSet &syspipeSet = m_FsSets.at(FsUnit::SYSPIPE);

    UINT32 maxSyspipe = 0;
    CHECK_RC(syspipeSet.GetTotalEnabledElements(FsUnit::SYSPIPE, &maxSyspipe));
    const UINT32 minSyspipe = downbinConfig.minEnablePerGroup;

    if (minSyspipe > maxSyspipe)
    {
        Printf(Tee::PriError, "Min syspipe enable requested %u is greater than max "
                "available %u\n", minSyspipe, maxSyspipe);
        return RC::CANNOT_MEET_FS_REQUIREMENTS;
    }

    UINT32 lwrrentGpcCount = 0;
    CHECK_RC(gpcSet.GetTotalEnabledElements(FsUnit::GPC, &lwrrentGpcCount));
    UINT32 syspipeDisableCount = 0;
    if (maxSyspipe > minSyspipe && maxSyspipe > lwrrentGpcCount)
    {
        syspipeDisableCount = min(maxSyspipe - minSyspipe, maxSyspipe - lwrrentGpcCount);
    }

    while (syspipeDisableCount > 0)
    {
        CHECK_RC(syspipeSet.DisableOneGroup());
        syspipeDisableCount--;
    }
    return rc;
}

//------------------------------------------------------------------------------
RC GH100DownbinImpl::AddGpcSetRules()
{
    RC rc;
    FsSet &gpcSet = m_FsSets.at(FsUnit::GPC);

    // GPC0 must be always enabled
    FsElementSet *pGpcElSet = gpcSet.GetGroupElementSet();
    shared_ptr<DownbinRule> pGpc0Rule = make_shared<AlwaysEnableRule>(pGpcElSet, 0);
    pGpcElSet->AddOnElementDisabledListener(pGpc0Rule);
    CHECK_RC(pGpcElSet->MarkProtected(0));

    // GPC0-CPC0 must always be enabled
    FsElementSet *pGpc0CpcSet = gpcSet.GetElementSet(0, FsUnit::CPC);
    shared_ptr<DownbinRule> pGpc0CpcRule = make_shared<AlwaysEnableRule>(pGpc0CpcSet, 0);
    pGpc0CpcSet->AddOnElementDisabledListener(pGpc0CpcRule);
    CHECK_RC(pGpc0CpcSet->MarkProtected(0));

    // CPC0-CPC1 can't be floorswept together in any GPC
    const UINT32 numGpcs = gpcSet.GetNumGroups();
    for (UINT32 gpcNum = 0; gpcNum < numGpcs; gpcNum++)
    {
        FsElementSet *pCpcSet = gpcSet.GetElementSet(gpcNum, FsUnit::CPC);
        shared_ptr<DownbinRule> pCpcRule = make_shared<IlwalidElementComboRule>(pCpcSet, 0x3);
        pCpcSet->AddOnElementDisabledListener(pCpcRule);
    }

    // GPC0-PES0 must always be enabled
    FsElementSet *pGpc0PesSet = gpcSet.GetElementSet(0, FsUnit::PES);
    shared_ptr<DownbinRule> pGpc0PesRule = make_shared<AlwaysEnableRule>(pGpc0PesSet, 0);
    pGpc0PesSet->AddOnElementDisabledListener(pGpc0PesRule);
    CHECK_RC(pGpc0PesSet->MarkProtected(0));

    // GPC0-ROP0 must always be enabled
    FsElementSet *pGpc0RopSet = gpcSet.GetElementSet(0, FsUnit::ROP);
    shared_ptr<DownbinRule> pGpc0RopRule = make_shared<AlwaysEnableRule>(pGpc0RopSet, 0);
    pGpc0RopSet->AddOnElementDisabledListener(pGpc0RopRule);
    CHECK_RC(pGpc0RopSet->MarkProtected(0));

    return rc;
}

//------------------------------------------------------------------------------
RC GH100DownbinImpl::AddL2NocRules()
{
    GpuSubdevice *pSubdev = static_cast<GpuSubdevice*>(m_pDev);

    // Add L2 NOC pair rules
    FsSet &fbpSet = m_FsSets.at(FsUnit::FBP);
    const UINT32 numFbps = fbpSet.GetNumGroups();
    for (UINT32 fbNum = 0; fbNum < numFbps; fbNum++)
    {
        FsGroup *pFsGroup = fbpSet.GetGroup(fbNum);

        // Paired FBPs
        INT32 pairedFbp = pSubdev->GetFbpNocPair(fbNum);
        shared_ptr<DownbinRule> pFbRule = make_shared<CrossElementDependencyRule>(&fbpSet,
                                                                                  FsUnit::FBP,
                                                                                  0, fbNum,
                                                                                  0, pairedFbp);
        pFsGroup->AddOnGroupDisabledListener(pFbRule);

        // Paired LTCs
        UINT32 numLtcsPerFbp = fbpSet.GetNumElementsPerGroup(FsUnit::LTC);
        for (UINT32 ltcNum = 0; ltcNum < numLtcsPerFbp; ltcNum++)
        {
            INT32 pairedLtc = pSubdev->GetFbpLtcNocPair(fbNum, ltcNum);
            shared_ptr<DownbinRule> pLtcRule = make_shared<CrossElementDependencyRule>(&fbpSet,
                                                                                       FsUnit::LTC,
                                                                                       fbNum, ltcNum,
                                                                                       pairedFbp, pairedLtc);
            FsElementSet *pElementSet = fbpSet.GetElementSet(fbNum, FsUnit::LTC);
            pElementSet->AddOnElementDisabledListener(pLtcRule);
        }

        // Paired L2 slices
        UINT32 numSlicesPerFbp = fbpSet.GetNumElementsPerGroup(FsUnit::L2_SLICE);
        for (UINT32 sliceNum = 0; sliceNum < numSlicesPerFbp; sliceNum++)
        {
            INT32 pairedSlice = pSubdev->GetFbpSliceNocPair(fbNum, sliceNum);
            shared_ptr<DownbinRule> pSliceRule = make_shared<CrossElementDependencyRule>(&fbpSet,
                                                                                         FsUnit::L2_SLICE,
                                                                                         fbNum, sliceNum,
                                                                                         pairedFbp, pairedSlice);
            FsElementSet *pElementSet = fbpSet.GetElementSet(fbNum, FsUnit::L2_SLICE);
            pElementSet->AddOnElementDisabledListener(pSliceRule);
        }
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
RC GH100DownbinImpl::AddL2SliceFsRules()
{
    static const UINT32 s_MaxSlicesFs = 4;
    static const UINT32 s_MaxSlicesFsPerFbp = 1;

    FsSet &fbpSet = m_FsSets.at(FsUnit::FBP);
    const UINT32 numFbps = fbpSet.GetNumGroups();
    for (UINT32 fbNum = 0; fbNum < numFbps; fbNum++)
    {
        FsElementSet *pL2SliceSet = fbpSet.GetElementSet(fbNum, FsUnit::L2_SLICE);
        shared_ptr<DownbinRule> pSliceFsRule = make_shared<GH100L2SliceRule>(&fbpSet, s_MaxSlicesFs, s_MaxSlicesFsPerFbp);
        pL2SliceSet->AddOnElementDisabledListener(pSliceFsRule);
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
RC GH100DownbinImpl::AddUgpuGDPickers
(
    FsSet *pFsSet,
    FsUnit fsUnit,
    const FuseUtil::DownbinConfig &unitConfig
)
{
    GpuSubdevice *pSubdev = static_cast<GpuSubdevice*>(m_pDev);
    if (pSubdev->GetMaxUGpuCount() > 0)
    {
        unique_ptr<DownbinGroupPicker> pPicker =
            make_unique<MostEnabledUgpuPicker>(m_pDev, pFsSet, fsUnit);
        if (pFsSet->IsGroupFsUnit(fsUnit))
        {
            pFsSet->AddGroupDisablePicker(move(pPicker));
        }
    }
    return RC::OK;
}

RC GH100DownbinImpl::AddUgpuGRPickers
(
    FsSet *pFsSet,
    FsUnit fsUnit,
    const FuseUtil::DownbinConfig &unitConfig
)
{
    GpuSubdevice *pSubdev = static_cast<GpuSubdevice*>(m_pDev);
    if (pSubdev->GetMaxUGpuCount() > 0)
    {
        unique_ptr<DownbinGroupPicker> pPicker =
            make_unique<MostEnabledUgpuPicker>(m_pDev, pFsSet, fsUnit);
        if (!pFsSet->IsGroupFsUnit(fsUnit))
        {
            pFsSet->AddGroupReducePicker(fsUnit, move(pPicker));
        }
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
RC GH100DownbinImpl::AddGpcSetGroupDisablePickers(const string &skuName)
{
    RC rc;
    FsSet &gpcSet  = m_FsSets.at(FsUnit::GPC);
    // LEVEL0
    // Protected bits picker already added in the AddDownbinPickers(parent function)
    //
    // Add group disable picker for balancing GPCs among ugpu for SKUs having a no UGPU-GPC
    // imbalance
    FuseUtil::DownbinConfig gpcConfig;
    CHECK_RC(GetUnitDownbinConfig(skuName, gpcSet.GetGroupFsUnit(), &gpcConfig));
    if (gpcConfig.maxUnitUgpuImbalance <= 1)
    {
        CHECK_RC(AddUgpuGDPickers(&gpcSet, FsUnit::GPC, gpcConfig));
    }

    // LEVEL1
    // group disable picker for primary element in GPC set
    // Pick the GPCs that have the min number of primary elements enabled
    CHECK_RC(AddMostDisabledGDPickers(&gpcSet, gpcSet.GetPrimaryFsUnit()));

    // LEVEL2
    if (gpcConfig.maxUnitUgpuImbalance > 1)
    {
       // Add as a general recommendation for SKUs where ugpu balancing is not a strict
       // requirement
       CHECK_RC(AddUgpuGDPickers(&gpcSet, FsUnit::GPC, gpcConfig));
    }
    // TODO: Add thermal pickers based on thermal studies later

    // LEVEL3
    // Add basic group disable pickers for non-primary elements
    // Pick the GPCs that have the min number of non-primary elements enabled
    // Only add for CPC as we only 1 ROP and 1 PES which are protected
    CHECK_RC(AddMostDisabledGDPickers(&gpcSet, FsUnit::CPC));

    return rc;
}

//------------------------------------------------------------------------------
RC GH100DownbinImpl::AddGpcSetGroupReducePickers(const string &skuName)
{
    RC rc;
    FsSet &gpcSet  = m_FsSets.at(FsUnit::GPC);

    FuseUtil::DownbinConfig tpcConfig;
    CHECK_RC(GetUnitDownbinConfig(skuName, FsUnit::TPC, &tpcConfig));
    // LEVEL0
    // Add group reduce picker to pick the GPC in order not to break vgpc Skyline restriction
    CHECK_RC(AddVGpcSkylineGRPicker(&gpcSet, tpcConfig));
    // Add group reduce picker to balance TPCs among active GPCs for a SKU with no UGPU-TPC
    // imbalance
    if (tpcConfig.maxUnitUgpuImbalance <= 1)
    {
        CHECK_RC(AddUgpuGRPickers(&gpcSet, FsUnit::TPC, tpcConfig));
    }

    // LEVEL1
    // Add group reduce picker to pick the GPC with the least number of disabled TPCs
    CHECK_RC(AddMostAvailableGRPickers(&gpcSet, gpcSet.GetPrimaryFsUnit()));
    // Add group reduce pickers for protected bits
    // Picks the GPC with least no of protected TPC
    CHECK_RC(AddProtectedBitsGRPickers(&gpcSet, skuName));

    // LEVEL2
    // Add group reduce pickers for the non-primary elements of gpcSet
    // Pick the GPC which has most number of elements
    // available for downbinning
    // Only add for CPC as we only 1 ROP and 1 PES which are protected
    CHECK_RC(AddMostAvailableGRPickers(&gpcSet, FsUnit::CPC));

    // Add group reduce picker to balance TPCs among active GPCs
    if (tpcConfig.maxUnitUgpuImbalance > 1)
    {
        CHECK_RC(AddUgpuGRPickers(&gpcSet, FsUnit::TPC, tpcConfig));
    }
    // LEVEL3

    return rc;
}

//------------------------------------------------------------------------------
RC GH100DownbinImpl::AddGpcSetElementDisablePickers(const string &skuName)
{
    RC rc;
    FsSet &gpcSet  = m_FsSets.at(FsUnit::GPC);
    // LEVEL0
    // Add pickers for selecting non-protected elements in GPC set
    CHECK_RC(AddProtectedBitsEDPickers(&gpcSet, skuName));

    // LEVEL1
    CHECK_RC(AddGraphicsTpcEDPicker());
    CHECK_RC(AddCpcTpcEDPicker());

    // LEVEL2

    // LEVEL3
    return rc;
}

//------------------------------------------------------------------------------
RC GH100DownbinImpl::AddPreferredFbpGDPicker(const FuseUtil::DownbinConfig &fbpConfig)
{
    FsSet &fbpSet  = m_FsSets.at(FsUnit::FBP);
    unique_ptr<DownbinGroupPicker> pPicker =
        make_unique<GH100FBPGroupPicker>(m_pDev, &fbpSet, fbpConfig);
    fbpSet.AddGroupDisablePicker(move(pPicker));
    return RC::OK;
}

//------------------------------------------------------------------------------
RC GH100DownbinImpl::AddCpcTpcEDPicker()
{
    // Add element disable picker to choose TPCs corresponding to CPCs with least number of
    // disabled TPCs
    FsSet &gpcSet  = m_FsSets.at(FsUnit::GPC);
    unique_ptr<DownbinElementPicker> pElementPicker =
        make_unique<MostEnabledSubElementPicker>(&gpcSet, FsUnit::CPC, FsUnit::TPC);
    gpcSet.AddElementDisablePicker(FsUnit::TPC, move(pElementPicker));
    return RC::OK;
}

//------------------------------------------------------------------------------
RC GH100DownbinImpl::AddGraphicsTpcEDPicker()
{
    GpuSubdevice *pSubdev = static_cast<GpuSubdevice*>(m_pDev);
    FsSet &gpcSet  = m_FsSets.at(FsUnit::GPC);
    MASSERT(gpcSet.HasElementFsUnit(FsUnit::TPC));
    UINT32 numGpcs = gpcSet.GetNumGroups();
    vector<FsMask> preferredElementMasks(numGpcs);
    const FsElementSet *pElementSet = gpcSet.GetElementSet(0, FsUnit::TPC);
    const UINT32 numTpcsPerGpc = pElementSet->GetNumElements();
    for (UINT32 gpcIdx = 0; gpcIdx < numGpcs; gpcIdx++)
    {
        INT32 graphicsTpcMask = pSubdev->GetFullGraphicsTpcMask(gpcIdx);
        preferredElementMasks[gpcIdx] = (~graphicsTpcMask) & ((1U << numTpcsPerGpc) - 1);
    }

    // Add picker to choose non Graphics elements in a specific group
    unique_ptr<DownbinElementPicker> pElementPicker =
        make_unique<PreferredElementPicker>(&gpcSet, FsUnit::TPC, preferredElementMasks);
    gpcSet.AddElementDisablePicker(FsUnit::TPC, move(pElementPicker));
    return RC::OK;
}

//------------------------------------------------------------------------------
RC GH100DownbinImpl::AddVGpcSkylineGRPicker(FsSet *pFsSet, const FuseUtil::DownbinConfig &tpcConfig)
{
    MASSERT(pFsSet);
    MASSERT(pFsSet->HasElementFsUnit(FsUnit::TPC));
    if (tpcConfig.vGpcSkylineList.size())
    {
        unique_ptr<DownbinGroupPicker> pPicker =
            make_unique<VGpcSkylineGroupPicker>(pFsSet, tpcConfig);
        pFsSet->AddGroupReducePicker(FsUnit::TPC, move(pPicker));
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
RC GH100DownbinImpl::AddFbpSetGroupDisablePickers(const string &skuName)
{
    RC rc;
    FsSet &fbpSet  = m_FsSets.at(FsUnit::FBP);
    // LEVEL0
    // Protected bits picker already added in the AddDownbinPickers(parent function)
    //

    // LEVEL1
    // group disable picker for primary element in fbp set
    // Pick the FBPs that have the min number of primary elements- LTC enabled
    CHECK_RC(AddMostDisabledGDPickers(&fbpSet, FsUnit::LTC));
    // Pick the FBPs that have the min number of L2 SLICES enabled
    CHECK_RC(AddMostDisabledGDPickers(&fbpSet, FsUnit::L2_SLICE));

    // LEVEL2
    // Add picker to return a preferred list of FBP to choose to disable based on
    // HBM connectivity and desired order of disablement of HBM site
    FuseUtil::DownbinConfig fbpConfig;
    CHECK_RC(GetUnitDownbinConfig(skuName, fbpSet.GetGroupFsUnit(), &fbpConfig));
    CHECK_RC(AddPreferredFbpGDPicker(fbpConfig));

    // LEVEL3

    return rc;
}

//------------------------------------------------------------------------------
RC GH100DownbinImpl::AddFbpSetGroupReducePickers(const string &skuName)
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

//------------------------------------------------------------------------------
RC GH100DownbinImpl::AddFbpSetElementDisablePickers(const string &skuName)
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
RC GH100DownbinImpl::ApplyPostDownbinRules(const string& skuName)
{
    RC rc;
    // Checks for max imbalance in the number of GPC/TPC among UGPUs
    FsSet &gpcSet = m_FsSets.at(FsUnit::GPC);
    vector<FsUnit> checkUnitImbalanceList;
    checkUnitImbalanceList.push_back(FsUnit::GPC);
    checkUnitImbalanceList.push_back(FsUnit::TPC);
    for (FsUnit fsUnit : checkUnitImbalanceList)
    {
        FuseUtil::DownbinConfig unitConfig;
        CHECK_RC(GetUnitDownbinConfig(skuName, fsUnit, &unitConfig));

        vector<UINT32> ugpuUnitCounts;
        FsMask groupEnableMask(gpcSet.GetNumGroups());
        groupEnableMask.SetIlwertedMask(*(gpcSet.GetGroupElementSet()->GetTotalDisableMask()));
        CHECK_RC(gpcSet.GetUgpuEnabledDistribution(m_pDev,
                                                   groupEnableMask,
                                                   fsUnit,
                                                   &ugpuUnitCounts));

        UINT32 lwrMaxImbalance = 0;
        // Callwlate the max imbalance of unit count across UPGUs
        auto  bounds = minmax_element(ugpuUnitCounts.begin(), ugpuUnitCounts.end());
        lwrMaxImbalance = *bounds.second - *bounds.first;

        if (lwrMaxImbalance > unitConfig.maxUnitUgpuImbalance)
        {
            Printf(Tee::PriError, "Difference in %s enabled count accross UGPUs(%u) > max allowed "
                    "difference(%u)\n", Downbin::ToString(fsUnit).c_str(),
                    lwrMaxImbalance, unitConfig.maxUnitUgpuImbalance);
            return RC::CANNOT_MEET_FS_REQUIREMENTS;
        }
    }
    return rc;
}

