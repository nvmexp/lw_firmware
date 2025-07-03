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

#include "core/include/massert.h"
#include "core/include/utility.h"
#include "gpu/fuse/downbin/downbinimpl.h"

#include "fsset.h"

using namespace Downbin;

//------------------------------------------------------------------------------
FsSet::FsSet(FsUnit groupUnit, UINT32 numGroups)
    : m_GroupFsUnit(groupUnit)
    , m_NumGroups(numGroups)
    , m_GroupElementSet(groupUnit, numGroups, false)
{
    m_GroupElementSet.SetParent(this);
    for (UINT32 groupIdx = 0; groupIdx < numGroups; groupIdx++)
    {
        m_Groups.emplace_back(groupUnit, groupIdx);
        m_Groups[groupIdx].SetParent(this);
    }
    m_FsUnitInfo[groupUnit] = FsUnitInfo();
}

FsSet::FsSet(FsSet&& o)
    : m_GroupFsUnit(std::move(o.m_GroupFsUnit))
    , m_NumGroups(std::move(o.m_NumGroups))
    , m_PrimaryFsUnit(std::move(o.m_PrimaryFsUnit))
    , m_GroupElementSet(std::move(o.m_GroupElementSet))
    , m_Groups(std::move(o.m_Groups))
    , m_FsUnitInfo(std::move(o.m_FsUnitInfo))
    , m_GroupDisablePickers(std::move(o.m_GroupDisablePickers))
    , m_GroupReducePickers(std::move(o.m_GroupReducePickers))
    , m_ElementDisablePickers(std::move(o.m_ElementDisablePickers))
{
    m_pDownbinImpl = o.m_pDownbinImpl;
    m_GroupElementSet.SetParent(this);
    for (UINT32 groupIdx = 0; groupIdx < m_NumGroups; groupIdx++)
    {
        m_Groups[groupIdx].SetParent(this);
    }

    o.m_pDownbinImpl = nullptr;
}

FsSet& FsSet::operator=(FsSet&& o)
{
    if (this != &o)
    {
        m_GroupFsUnit     = std::move(o.m_GroupFsUnit);
        m_NumGroups       = std::move(o.m_NumGroups);
        m_PrimaryFsUnit   = std::move(o.m_PrimaryFsUnit);
        m_GroupElementSet = std::move(o.m_GroupElementSet);
        m_Groups          = std::move(o.m_Groups);
        m_FsUnitInfo      = std::move(o.m_FsUnitInfo);
        m_GroupDisablePickers   = std::move(o.m_GroupDisablePickers);
        m_GroupReducePickers    = std::move(o.m_GroupReducePickers);
        m_ElementDisablePickers = std::move(o.m_ElementDisablePickers);

        m_pDownbinImpl = o.m_pDownbinImpl;
        m_GroupElementSet.SetParent(this);
        for (UINT32 groupIdx = 0; groupIdx < m_NumGroups; groupIdx++)
        {
            m_Groups[groupIdx].SetParent(this);
        }

        o.m_pDownbinImpl = nullptr;
    }
    return *this;
}

//------------------------------------------------------------------------------
void FsSet::SetParentDownbin(DownbinImpl *pDownbin)
{
    MASSERT(pDownbin);
    m_pDownbinImpl = pDownbin;
}

//------------------------------------------------------------------------------
const Downbin::Settings* FsSet::GetDownbinSettings() const
{
    return m_pDownbinImpl->GetSettings();
}

//------------------------------------------------------------------------------
RC FsSet::AddElementSets
(
    FsUnit fsUnit,
    UINT32 numElements,
    bool   bIsDerivative
)
{
    RC rc;
    for (UINT32 groupIdx = 0; groupIdx < m_NumGroups; groupIdx++)
    {
        CHECK_RC(m_Groups[groupIdx].AddElementSet(fsUnit, numElements, bIsDerivative));
    }
    m_FsUnitInfo[fsUnit] = FsUnitInfo();
    return rc;
}

//------------------------------------------------------------------------------
RC FsSet::AddElement
(
    FsUnit fsUnit,
    UINT32 numElements,
    bool   bIsPrimary,
    bool   bIsDerivative
)
{
    RC rc;
    if (bIsPrimary)
    {
        if ((m_PrimaryFsUnit != FsUnit::NONE) && (m_PrimaryFsUnit != fsUnit))
        {
            Printf(Tee::PriError, "Can't set primary element %s. Already set as %s\n",
                                   Downbin::ToString(fsUnit).c_str(),
                                   Downbin::ToString(m_PrimaryFsUnit).c_str());
            return RC::BAD_PARAMETER;
        }
        m_PrimaryFsUnit = fsUnit;
    }
    if (bIsPrimary && bIsDerivative)
    {
        Printf(Tee::PriError, "Element %s can't be both primary and a derivative\n",
                               Downbin::ToString(fsUnit).c_str());
    }

    CHECK_RC(AddElementSets(fsUnit, numElements, bIsDerivative));
    return rc;
}

//------------------------------------------------------------------------------
void FsSet::GetElementFsUnits(vector<FsUnit> *pFsUnits) const
{
    MASSERT(pFsUnits);
    MASSERT(m_NumGroups > 0);

    return m_Groups[0].GetElementFsUnits(pFsUnits);
}

//------------------------------------------------------------------------------
void FsSet::GetFsUnits(vector<FsUnit> *pFsUnits) const
{
    MASSERT(pFsUnits);
    GetElementFsUnits(pFsUnits);
    pFsUnits->push_back(m_GroupFsUnit);
}

//------------------------------------------------------------------------------
bool FsSet::IsGroupFsUnit(FsUnit fsUnit) const
{
    return (GetGroupFsUnit() == fsUnit);
}

//------------------------------------------------------------------------------
UINT32 FsSet::GetNumElementsPerGroup(FsUnit fsUnit) const
{
    MASSERT(m_NumGroups > 0);
    if (HasElementFsUnit(fsUnit))
    {
        return m_Groups[0].GetElementSet(fsUnit)->GetNumElements();
    }

    MASSERT("Did not find given element in this FS group\n");
    return 0;
}

//------------------------------------------------------------------------------
const FsGroup* FsSet::GetGroup(UINT32 groupIdx) const
{
    if (groupIdx >= m_NumGroups)
    {
        MASSERT("Group index requested greater than max\n");
        return nullptr;
    }
    return static_cast<const FsGroup*>(&m_Groups[groupIdx]);
}

//------------------------------------------------------------------------------
const FsElementSet* FsSet::GetGroupElementSet() const
{
    return static_cast<const FsElementSet*>(&m_GroupElementSet);
}

//------------------------------------------------------------------------------
const FsElementSet* FsSet::GetElementSet(UINT32 groupIdx, FsUnit fsUnit) const
{
    if (groupIdx >= m_NumGroups)
    {
        MASSERT("Group index requested greater than max\n");
        return nullptr;
    }
    return m_Groups[groupIdx].GetElementSet(fsUnit);
}

//------------------------------------------------------------------------------
FsGroup* FsSet::GetGroup(UINT32 groupIdx)
{
    if (groupIdx >= m_NumGroups)
    {
        MASSERT("Group index requested greater than max\n");
        return nullptr;
    }
    return &m_Groups[groupIdx];
}

//------------------------------------------------------------------------------
FsElementSet* FsSet::GetGroupElementSet()
{
    return &m_GroupElementSet;
}

//------------------------------------------------------------------------------
FsElementSet* FsSet::GetElementSet(UINT32 groupIdx, FsUnit fsUnit)
{
    if (groupIdx >= m_NumGroups)
    {
        MASSERT("Group index requested greater than max\n");
        return nullptr;
    }
    return m_Groups[groupIdx].GetElementSet(fsUnit);
}

//------------------------------------------------------------------------------
bool FsSet::HasFsUnit(FsUnit fsUnit) const
{
    if (m_FsUnitInfo.find(fsUnit) != m_FsUnitInfo.end())
    {
        return true;
    }
    return false;
}

//------------------------------------------------------------------------------
bool FsSet::HasElementFsUnit(FsUnit fsUnit) const
{
    if (!IsGroupFsUnit(fsUnit) && HasFsUnit(fsUnit))
    {
        return true;
    }
    return false;
}

//------------------------------------------------------------------------------
string FsSet::GetFuseName(FsUnit fsUnit) const
{
    MASSERT(HasFsUnit(fsUnit));

    if (!m_FsUnitInfo.at(fsUnit).FuseName.empty())
    {
        return m_FsUnitInfo.at(fsUnit).FuseName;
    }
    return Downbin::ToString(fsUnit);
}

//------------------------------------------------------------------------------
bool FsSet::IsDisableFuse(FsUnit fsUnit) const
{
    MASSERT(HasFsUnit(fsUnit));
    return m_FsUnitInfo.at(fsUnit).bIsDisableFuse;
}

//------------------------------------------------------------------------------
bool FsSet::IsPerGroupFuse(FsUnit fsUnit) const
{
    MASSERT(HasFsUnit(fsUnit));
    return m_FsUnitInfo.at(fsUnit).bIsPerGroupFuse;
}

//------------------------------------------------------------------------------
bool FsSet::HasReconfig(FsUnit fsUnit) const
{
    MASSERT(HasFsUnit(fsUnit));
    return m_FsUnitInfo.at(fsUnit).bHasReconfig;
}

//------------------------------------------------------------------------------
void FsSet::SetFuseName(FsUnit fsUnit, string fuseName)
{
    MASSERT(HasFsUnit(fsUnit));
    m_FsUnitInfo.at(fsUnit).FuseName = fuseName;
}

//------------------------------------------------------------------------------
void FsSet::SetIsDisableFuse(FsUnit fsUnit, bool bIsDisableFuse)
{
    MASSERT(HasFsUnit(fsUnit));
    m_FsUnitInfo.at(fsUnit).bIsDisableFuse = bIsDisableFuse;
}

//------------------------------------------------------------------------------
void FsSet::SetIsPerGroupFuse(FsUnit fsUnit, bool bIsPerGroupFuse)
{
    MASSERT(HasFsUnit(fsUnit));
    m_FsUnitInfo.at(fsUnit).bIsPerGroupFuse = bIsPerGroupFuse;
}

//------------------------------------------------------------------------------
void FsSet::SetHasReconfig(FsUnit fsUnit, bool bHasReconfig)
{
    MASSERT(HasFsUnit(fsUnit));
    m_FsUnitInfo.at(fsUnit).bHasReconfig = bHasReconfig;
}

//------------------------------------------------------------------------------
RC FsSet::DisableOneGroup()
{
    RC rc;

    // 1. Pick a group to disable
    FsMask inputMask;
    inputMask.SetIlwertedMask(*(m_GroupElementSet.GetTotalDisableMask()));
    if (inputMask.IsEmpty())
    {
        Printf(Tee::PriError, "No groups left to disable\n");
        return RC::CANNOT_MEET_FS_REQUIREMENTS;
    }
    FsMask outputMask = inputMask;
    for (auto &pPicker : m_GroupDisablePickers)
    {
        CHECK_RC(pPicker->Pick(inputMask, &outputMask));
        if (outputMask.IsEmpty())
        {
            Printf(Tee::PriError, "No more valid groups found to disable\n");
            return RC::CANNOT_MEET_FS_REQUIREMENTS;
        }
        inputMask = outputMask;
    }
    // At this point, all selected groups are equally good to be disabled
    // Just choose the one with the lowest index out of the given choices
    INT32 disableIndex = Utility::BitScanForward(outputMask.GetMask());
    MASSERT(disableIndex >= 0);

    // 2. Disable it
    Printf(Tee::PriDebug, "Disabling %s%d\n",
                           ToString(GetGroupFsUnit()).c_str(), disableIndex);
    CHECK_RC(DisableGroup(static_cast<UINT32>(disableIndex)));

    return rc;
}

//------------------------------------------------------------------------------
RC FsSet::DisableOneElement(FsUnit fsUnit)
{
    RC rc;
    if (!HasElementFsUnit(fsUnit))
    {
        return RC::BAD_PARAMETER;
    }

    // 1. Pick a group to disable an element from
    FsMask inputMask;
    inputMask.SetIlwertedMask(*(m_GroupElementSet.GetTotalDisableMask()));
    if (inputMask.IsEmpty())
    {
        Printf(Tee::PriError, "No groups left to disable from\n");
        return RC::CANNOT_MEET_FS_REQUIREMENTS;
    }
    FsMask outputMask = inputMask;
    if (m_GroupReducePickers.find(fsUnit) != m_GroupReducePickers.end())
    {
        for (auto &pPicker : m_GroupReducePickers.at(fsUnit))
        {
            CHECK_RC(pPicker->Pick(inputMask, &outputMask));
            if (outputMask.IsEmpty())
            {
                Printf(Tee::PriError, "No more valid groups found to disable from\n");
                return RC::CANNOT_MEET_FS_REQUIREMENTS;
            }
            inputMask = outputMask;
        }
    }
    // At this point, all selected groups are equally good to be disable an element from
    // Just choose the one with the lowest index out of the given choices
    INT32 groupIdx = Utility::BitScanForward(outputMask.GetMask());
    MASSERT(groupIdx >= 0);

    // 2. Pick an element from that group
    const FsMask *pElDisMask = GetElementSet(static_cast<UINT32>(groupIdx), fsUnit)->GetTotalDisableMask();
    inputMask.SetIlwertedMask(*pElDisMask);
    if (inputMask.IsEmpty())
    {
        Printf(Tee::PriError, "No elements left in group %d to disable\n", groupIdx);
        return RC::CANNOT_MEET_FS_REQUIREMENTS;
    }
    outputMask = inputMask;
    if (m_ElementDisablePickers.find(fsUnit) != m_ElementDisablePickers.end())
    {
        for (auto &pPicker : m_ElementDisablePickers.at(fsUnit))
        {
            CHECK_RC(pPicker->Pick(groupIdx, inputMask, &outputMask));
            if (outputMask.IsEmpty())
            {
                Printf(Tee::PriError, "No more valid elements found to disable\n");
                return RC::CANNOT_MEET_FS_REQUIREMENTS;
            }
            inputMask = outputMask;
        }
    }
    // At this point, all selected elements are equally good to disable
    // Just choose the one with the lowest index out of the given choices
    INT32 elIdx = Utility::BitScanForward(outputMask.GetMask());
    MASSERT(elIdx >= 0);

    // 3. Disable the selected element
    Printf(Tee::PriDebug, "Disabling %s%d-%s%d\n",
                           ToString(GetGroupFsUnit()).c_str(), groupIdx,
                           ToString(fsUnit).c_str(), elIdx);
    CHECK_RC(DisableElement(fsUnit, static_cast<UINT32>(groupIdx), static_cast<UINT32>(elIdx)));
    return rc;
}

//------------------------------------------------------------------------------
RC FsSet::DisableGroup(UINT32 groupNum)
{
    RC rc;
    if (groupNum >= m_NumGroups)
    {
        Printf(Tee::PriError, "Group num %u greater than number of groups %u\n",
                               groupNum, m_NumGroups);
        return RC::BAD_PARAMETER;
    }

    CHECK_RC(m_GroupElementSet.DisableElement(groupNum));
    CHECK_RC(m_Groups[groupNum].DisableGroup());
    return rc;
}

//------------------------------------------------------------------------------
RC FsSet::DisableGroups(UINT32 groupMask)
{
    RC rc;
    while (groupMask != 0)
    {
        UINT32 groupIdx = static_cast<UINT32>(Utility::BitScanForward(groupMask));
        groupMask ^= (1U << groupIdx);
        CHECK_RC(DisableGroup(groupIdx));
    }
    return rc;
}

//------------------------------------------------------------------------------
RC FsSet::DisableElement(FsUnit fsUnit, UINT32 absElementNum)
{
    if (!HasElementFsUnit(fsUnit))
    {
        return RC::BAD_PARAMETER;
    }

    UINT32 numPerGroup = GetNumElementsPerGroup(fsUnit);
    MASSERT(numPerGroup);
    UINT32 groupNum    = absElementNum / numPerGroup;
    UINT32 elementNum  = absElementNum % numPerGroup;
    return DisableElement(fsUnit, groupNum, elementNum);
}

//------------------------------------------------------------------------------
RC FsSet::DisableElement(FsUnit fsUnit, UINT32 groupNum, UINT32 elementNum)
{
    if (!HasElementFsUnit(fsUnit))
    {
        return RC::BAD_PARAMETER;
    }

    return m_Groups[groupNum].DisableElement(fsUnit, elementNum);
}

//------------------------------------------------------------------------------
RC FsSet::DisableElements(FsUnit fsUnit, UINT32 groupNum, UINT32 elementMask)
{
    if (!HasElementFsUnit(fsUnit))
    {
        return RC::BAD_PARAMETER;
    }

    return m_Groups[groupNum].DisableElements(fsUnit, elementMask);
}

//------------------------------------------------------------------------------
RC FsSet::DisableElements(FsUnit fsUnit, const vector<UINT32> &elementMasks)
{
    RC rc;

    if (!HasElementFsUnit(fsUnit))
    {
        return RC::BAD_PARAMETER;
    }

    if (elementMasks.size() != m_NumGroups)
    {
        return RC::BAD_PARAMETER;
    }

    for (UINT32 groupIdx = 0; groupIdx < m_NumGroups; groupIdx++)
    {
        CHECK_RC(m_Groups[groupIdx].DisableElements(fsUnit, elementMasks[groupIdx]));
    }
    return rc;
}

//------------------------------------------------------------------------------
RC FsSet::GetGroupEnabledElements
(
    FsUnit fsUnit,
    UINT32 groupNum,
    UINT32 *pNumEnabled
) const
{
    RC rc;
    MASSERT(pNumEnabled);

    if (!HasElementFsUnit(fsUnit))
    {
        return RC::BAD_PARAMETER;
    }

    if (groupNum >= GetNumGroups())
    {
        return RC::BAD_PARAMETER;
    }

    const FsElementSet *pElementSet = GetElementSet(groupNum, fsUnit);
    *pNumEnabled = pElementSet->GetTotalDisableMask()->GetNumUnsetBits();
    return RC::OK;
}

//------------------------------------------------------------------------------
RC FsSet::GetTotalEnabledElements(FsUnit fsUnit, UINT32 *pNumEnabled) const
{
    MASSERT(pNumEnabled);

    if (!HasFsUnit(fsUnit))
    {
        return RC::BAD_PARAMETER;
    }

    if (IsGroupFsUnit(fsUnit))
    {
        *pNumEnabled = GetGroupElementSet()->GetTotalDisableMask()->GetNumUnsetBits();
    }
    else
    {
        UINT32 numGroups = GetNumGroups();
        *pNumEnabled = 0;
        for (UINT32 groupIdx = 0; groupIdx < numGroups; groupIdx++)
        {
            const FsElementSet *pElementSet = GetElementSet(groupIdx, fsUnit);
            *pNumEnabled += pElementSet->GetTotalDisableMask()->GetNumUnsetBits();
        }
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
RC FsSet::GetNumsEnabledElementsPerGroup(FsUnit fsUnit, vector<UINT32> *pNumsEnabledPerGroup) const
{
    MASSERT(pNumsEnabledPerGroup);
    if (!HasElementFsUnit(fsUnit))
    {
        Printf(Tee::PriError, "%s is not an element Fs Unit of %s FsSet\n",
            ToString(fsUnit).c_str(), ToString(GetGroupFsUnit()).c_str());
        return RC::BAD_PARAMETER;
    }
    UINT32 numGroups = GetNumGroups();
    for (UINT32 groupIdx = 0; groupIdx < numGroups; groupIdx++)
    {
        const FsElementSet *pElementSet = GetElementSet(groupIdx, fsUnit);
        pNumsEnabledPerGroup->push_back(pElementSet->GetTotalDisableMask()->GetNumUnsetBits());
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
RC FsSet::GetTotalDisabledElements(FsUnit fsUnit, UINT32 *pNumDisabled) const
{
    MASSERT(pNumDisabled);

    if (!HasFsUnit(fsUnit))
    {
        return RC::BAD_PARAMETER;
    }

    if (IsGroupFsUnit(fsUnit))
    {
        *pNumDisabled = GetGroupElementSet()->GetTotalDisableMask()->GetNumSetBits();
    }
    else
    {
        UINT32 numGroups = GetNumGroups();
        *pNumDisabled = 0;
        for (UINT32 groupIdx = 0; groupIdx < numGroups; groupIdx++)
        {
            const FsElementSet *pElementSet = GetElementSet(groupIdx, fsUnit);
            *pNumDisabled += pElementSet->GetTotalDisableMask()->GetNumSetBits();
        }
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
RC FsSet::GetTotalReconfigElements(FsUnit fsUnit, UINT32 *pNumReconfig) const
{
    MASSERT(pNumReconfig);
    RC rc;
    if (!HasFsUnit(fsUnit))
    {
        return RC::BAD_PARAMETER;
    }

    if (IsGroupFsUnit(fsUnit))
    {
        *pNumReconfig = GetGroupElementSet()->GetReconfigMask()->GetNumSetBits();
    }
    else
    {
        UINT32 numGroups = GetNumGroups();
        *pNumReconfig = 0;
        for (UINT32 groupIdx = 0; groupIdx < numGroups; groupIdx++)
        {
            const FsElementSet *pElementSet = GetElementSet(groupIdx, fsUnit);
            *pNumReconfig += pElementSet->GetReconfigMask()->GetNumSetBits();
        }
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
void FsSet::PrintSet(Tee::Priority pri) const
{
    string headerStr  = "--------------------------------------------------------------------- \n";
    headerStr += Utility::StrPrintf("     | %10s ",
                 Utility::ToUpperCase(Downbin::ToString(m_GroupFsUnit)).c_str());
    for (const auto& unitInfo : m_FsUnitInfo)
    {
        FsUnit fsUnit = unitInfo.first;
        if (!IsGroupFsUnit(fsUnit))
        {
            headerStr += Utility::StrPrintf("| %10s ",
                         Utility::ToUpperCase(Downbin::ToString(fsUnit)).c_str());
        }
    }
    headerStr += "\n--------------------------------------------------------------------- \n";

    string disableStr;
    string defectiveStr;
    string reconfigStr;
    bool bHasAnyReconfig = false;
    // Has no elements in the group
    if (m_FsUnitInfo.size() == 1)
    {
        FsUnit fsUnit = m_FsUnitInfo.begin()->first;
        MASSERT(IsGroupFsUnit(fsUnit));
        disableStr   += Utility::StrPrintf("  -  | 0x%08x\n",
                        m_GroupElementSet.GetFsMask(FuseMaskType::DISABLE)->GetMask());
        defectiveStr += Utility::StrPrintf("  -  | 0x%08x\n",
                        m_GroupElementSet.GetFsMask(FuseMaskType::DEFECTIVE)->GetMask());
        if (m_FsUnitInfo.at(fsUnit).bHasReconfig)
        {
            reconfigStr += Utility::StrPrintf("  -  | 0x%08x\n",
                           m_GroupElementSet.GetFsMask(FuseMaskType::RECONFIG)->GetMask());
            bHasAnyReconfig = true;
        }
    }
    else
    {
        for (UINT32 groupIdx = 0; groupIdx < m_NumGroups; groupIdx++)
        {
            disableStr   += Utility::StrPrintf(" %3d | 0x%08x ",
                            groupIdx,
                            m_GroupElementSet.GetFsMask(FuseMaskType::DISABLE)->IsSet(groupIdx) ?
                                                          0x1 : 0x0);
            defectiveStr += Utility::StrPrintf(" %3d | 0x%08x ",
                            groupIdx,
                            m_GroupElementSet.GetFsMask(FuseMaskType::DEFECTIVE)->IsSet(groupIdx) ?
                                                          0x1 : 0x0);
            reconfigStr  += Utility::StrPrintf(" %3d | %10s ", groupIdx, " ");
            for (const auto& unitInfo : m_FsUnitInfo)
            {
                FsUnit fsUnit = unitInfo.first;
                if (IsGroupFsUnit(fsUnit))
                {
                    continue;
                }

                const FsElementSet *pSet = m_Groups[groupIdx].GetElementSet(fsUnit);

                disableStr   += Utility::StrPrintf("| 0x%08x ",
                                                    pSet->GetFsMask(FuseMaskType::DISABLE)->GetMask());
                defectiveStr += Utility::StrPrintf("| 0x%08x ",
                                                    pSet->GetFsMask(FuseMaskType::DEFECTIVE)->GetMask());
                if (m_FsUnitInfo.at(fsUnit).bHasReconfig)
                {
                    reconfigStr += Utility::StrPrintf("| 0x%08x ",
                                                       pSet->GetFsMask(FuseMaskType::RECONFIG)->GetMask());
                    bHasAnyReconfig = true;
                }
                else
                {
                    reconfigStr  += Utility::StrPrintf("| %10s ", " ");
                }
            }
            disableStr += "\n";
            defectiveStr += "\n";
            reconfigStr += "\n";
        }
    }

    Printf(pri, "%s", headerStr.c_str());
    Printf(pri, "----------------------------  DEFECTIVE  ---------------------------- \n%s",
                defectiveStr.c_str());
    Printf(pri, "----------------------------  DISABLE  ------------------------------ \n%s",
                disableStr.c_str());
    if (bHasAnyReconfig)
    {
        Printf(pri, "----------------------------  RECONFIG  ----------------------------- \n%s",
                    reconfigStr.c_str());
    }
}

//------------------------------------------------------------------------------
DownbinImpl* FsSet::GetDownbinImpl()
{
    return m_pDownbinImpl;
}

//------------------------------------------------------------------------------
void FsSet::AddGroupDisablePicker(unique_ptr<DownbinGroupPicker> pPicker)
{
    m_GroupDisablePickers.push_back(std::move(pPicker));
}

//------------------------------------------------------------------------------
void FsSet::AddGroupReducePicker(FsUnit fsUnit, unique_ptr<DownbinGroupPicker> pPicker)
{
    MASSERT(HasElementFsUnit(fsUnit));
    m_GroupReducePickers[fsUnit].push_back(std::move(pPicker));
}

//------------------------------------------------------------------------------
void FsSet::AddElementDisablePicker(FsUnit fsUnit, unique_ptr<DownbinElementPicker> pPicker)
{
    MASSERT(HasElementFsUnit(fsUnit));
    m_ElementDisablePickers[fsUnit].push_back(std::move(pPicker));
}

//------------------------------------------------------------------------------
RC FsSet::GetPartialEnabledElements
(
    const FsMask& refGroupMask,
    FsUnit fsUnit,
    UINT32 *pEnableCount
)
{
    MASSERT(pEnableCount);
    MASSERT(fsUnit != FsUnit::NONE);

    *pEnableCount = 0;

    // TODO Move the common code from GetTotalEnabledElements and rename this func appropriately
    if (IsGroupFsUnit(fsUnit))
    {
        *pEnableCount = refGroupMask.GetNumSetBits();
    }
    else
    {
        if (!HasElementFsUnit(fsUnit))
        {
            Printf(Tee::PriError, "Invalid element fs unit!\n");
            return RC::SOFTWARE_ERROR;
        }

        for (INT32 grpIdx = Utility::BitScanForward(refGroupMask.GetMask());
         grpIdx >= 0;
         grpIdx = Utility::BitScanForward(refGroupMask.GetMask(), grpIdx + 1))
        {
            *pEnableCount += GetElementSet(grpIdx, fsUnit)->
                GetTotalDisableMask()->GetNumUnsetBits();
        }
    }

    return RC::OK;
}

RC FsSet::GetUgpuEnabledDistribution
(
    TestDevice *pDev,
    FsMask refGroupMask,
    FsUnit fsUnit,
    vector<UINT32> *pUgpuUnitCounts
)
{
    MASSERT(pDev);
    MASSERT(pUgpuUnitCounts);
    RC rc;

    GpuSubdevice *pSubdev = static_cast<GpuSubdevice*>(pDev);

    vector<FsMask> uGpuGroupEnMasks;
    CHECK_RC(Downbin::GetUGpuGroupEnMasks(*pSubdev,
                                          GetGroupFsUnit(),
                                          refGroupMask,
                                          &uGpuGroupEnMasks));

    for (UINT32 ugpu = 0; ugpu <  pSubdev->GetMaxUGpuCount(); ugpu++)
    {
        UINT32 ugpuUnitCount = 0;
        CHECK_RC(GetPartialEnabledElements(uGpuGroupEnMasks[ugpu],
                    fsUnit, &ugpuUnitCount));
        pUgpuUnitCounts->push_back(ugpuUnitCount);
    }

    return rc;
}
