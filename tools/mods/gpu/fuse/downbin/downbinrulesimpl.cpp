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

#include "core/include/utility.h"

#include "downbinrulesimpl.h"
#include "fsset.h"

using namespace Downbin;

//------------------------------------------------------------------------------
// BasicGroupDisableRule

BasicGroupDisableRule::BasicGroupDisableRule
(
    FsGroup *pFsGroup
)
    : m_pFsGroup(pFsGroup)
{
    MASSERT(pFsGroup);

    vector<FsUnit> elements;
    pFsGroup->GetElementFsUnits(&elements);
    for (auto fsUnit : elements)
    {
        if (!pFsGroup->GetElementSet(fsUnit)->IsDerivative())
        {
            m_FsUnitsToDisable.push_back(fsUnit);
        }
    }
}

string BasicGroupDisableRule::GetName() const
{
    string name = Utility::StrPrintf("BasicGroupDisableRule-%s%u",
                                      Downbin::ToString(m_pFsGroup->GetGroupFsUnit()).c_str(),
                                      m_pFsGroup->GetIndex());
    return name;
}

RC BasicGroupDisableRule::Execute()
{
    RC rc;
    for (auto &fsUnit : m_FsUnitsToDisable)
    {
        CHECK_RC(m_pFsGroup->GetElementSet(fsUnit)->DisableAllElements());
    }
    return rc;
}

//------------------------------------------------------------------------------
// MinGroupElementCountRule

MinGroupElementCountRule::MinGroupElementCountRule
(
    FsElementSet *pElementSet,
    UINT32 minCount
)
    : m_pFsElementSet(pElementSet)
    , m_MinCount(minCount)
{
    MASSERT(pElementSet);
    MASSERT(pElementSet->GetParentGroup());
}

string MinGroupElementCountRule::GetName() const
{
    string name = Utility::StrPrintf("MinGroupElementCountRule-%u%ss/Group",
                                      m_MinCount,
                                      Downbin::ToString(m_pFsElementSet->GetFsUnit()).c_str());
    return name;
}

RC MinGroupElementCountRule::Execute()
{
    RC rc;

    if (!m_pFsElementSet->GetParentGroup()->IsDisabled())
    {
        UINT32 numEnabledElements = m_pFsElementSet->GetTotalDisableMask()->GetNumUnsetBits();
        if (numEnabledElements < m_MinCount)
        {
            FsGroup *pFsGroup = m_pFsElementSet->GetParentGroup();
            string elementStr = Downbin::ToString(m_pFsElementSet->GetFsUnit());
            string fsGroupStr = Downbin::ToString(pFsGroup->GetGroupFsUnit());
            UINT32 fsGroupIdx = pFsGroup->GetIndex();

            Printf(Tee::PriLow, "%s count for %s%u < Min (%u). Disabling %s%u\n",
                                 elementStr.c_str(), fsGroupStr.c_str(), fsGroupIdx,
                                 m_MinCount, fsGroupStr.c_str(), fsGroupIdx);
            CHECK_RC(pFsGroup->DisableGroup());
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
// MinSubElementCountRule

MinSubElementCountRule::MinSubElementCountRule
(
    FsGroup *pFsGroup,
    FsUnit elementFsUnit,
    FsUnit subElementFsUnit,
    UINT32 minCountPerElement
)
    : m_MinCountPerElement(minCountPerElement)
{
    MASSERT(pFsGroup);

    m_pElementSet    = pFsGroup->GetElementSet(elementFsUnit);
    m_pSubElementSet = pFsGroup->GetElementSet(subElementFsUnit);
    MASSERT(m_pElementSet);
    MASSERT(m_pSubElementSet);

    UINT32 numElements      = m_pElementSet->GetNumElements();
    UINT32 numSubElements   = m_pSubElementSet->GetNumElements();
    Utility::GetUniformDistribution(numElements, numSubElements, &m_SubElementDistribution);
}

string MinSubElementCountRule::GetName() const
{
    string name = Utility::StrPrintf("MinSubElementCountRule- %u%s per %s",
                                      m_MinCountPerElement,
                                      Downbin::ToString(m_pSubElementSet->GetFsUnit()).c_str(),
                                      Downbin::ToString(m_pElementSet->GetFsUnit()).c_str());
    return name;
}

RC MinSubElementCountRule::Execute()
{
    RC rc;

    UINT32 numElements = m_pElementSet->GetNumElements();
    UINT32 subElementStartIdx = 0;
    UINT32 lwrSubElementMask = m_pSubElementSet->GetTotalDisableMask()->GetMask();
    for (UINT32 i = 0; i < numElements; i++)
    {
        if (!m_pElementSet->GetTotalDisableMask()->IsSet(i))
        {
            UINT32 fullSubElementMask = ((1 << m_SubElementDistribution[i]) - 1) <<
                                         subElementStartIdx;

            FsMask subElementMask(m_SubElementDistribution[i]);
            subElementMask.SetMask(fullSubElementMask & lwrSubElementMask);
            UINT32 subElementCount = subElementMask.GetNumUnsetBits();
            if (subElementCount < m_MinCountPerElement)
            {
                FsGroup *pFsGroup = m_pElementSet->GetParentGroup();
                UINT32 fsGroupIdx = pFsGroup->GetIndex();
                string fsGroupStr = Downbin::ToString(pFsGroup->GetGroupFsUnit());
                string elementStr = Downbin::ToString(m_pElementSet->GetFsUnit());
                string subElementStr = Downbin::ToString(m_pSubElementSet->GetFsUnit());
                Printf(Tee::PriLow, "%s count in %s%u-%s%u < Min (%u). Disabling %s%u\n",
                                     subElementStr.c_str(),
                                     fsGroupStr.c_str(), fsGroupIdx,
                                     elementStr.c_str(), i,
                                     m_MinCountPerElement,
                                     fsGroupStr.c_str(), fsGroupIdx);
                CHECK_RC(m_pElementSet->DisableElement(i));
            }
        }
        subElementStartIdx += m_SubElementDistribution[i];
    }
    return rc;
}

//------------------------------------------------------------------------------
// MinElementCountRule

MinElementCountRule::MinElementCountRule
(
    FsSet *pFsSet,
    FsUnit fsUnit,
    UINT32 minCount
)
    : m_pFsSet(pFsSet)
    , m_FsUnit(fsUnit)
    , m_MinCount(minCount)
{
    MASSERT(pFsSet);
    MASSERT(pFsSet->HasFsUnit(fsUnit));
}

MinElementCountRule::MinElementCountRule
(
    FsSet *pFsSet,
    FsUnit fsUnit,
    UINT32 minCount,
    vector<UINT32> elementMasks
)
    : m_pFsSet(pFsSet)
    , m_FsUnit(fsUnit)
    , m_MinCount(minCount)
    , m_ElementMasks(elementMasks)
{
    MASSERT(pFsSet);
    MASSERT(pFsSet->HasFsUnit(fsUnit));
}

string MinElementCountRule::GetName() const
{
    string name = Utility::StrPrintf("MinElementCountRule-%u%ss",
                                      m_MinCount,
                                      Downbin::ToString(m_FsUnit).c_str());
    return name;
}

RC MinElementCountRule::Execute()
{
    RC rc;

    UINT32 numEnabled = 0;
    if (m_ElementMasks.empty())
    {
        CHECK_RC(m_pFsSet->GetTotalEnabledElements(m_FsUnit, &numEnabled));
    }
    else
    {
        UINT32 numGroups = m_pFsSet->GetNumGroups();
        UINT32 numElPerGroup = m_pFsSet->GetNumElementsPerGroup(m_FsUnit);
        for (UINT32 groupIdx = 0; groupIdx < numGroups; groupIdx++)
        {
            const FsElementSet *pElementSet = m_pFsSet->GetElementSet(groupIdx, m_FsUnit);
            FsMask enabledMask(numElPerGroup);
            enabledMask.SetMask(~pElementSet->GetTotalDisableMask()->GetMask() &
                                m_ElementMasks[groupIdx]);
            numEnabled += enabledMask.GetNumSetBits();
        }
    }

    if (numEnabled < m_MinCount)
    {
        Printf(Tee::PriError, "%s count < Min (%u)\n",
                               Downbin::ToString(m_FsUnit).c_str(),
                               m_MinCount);
        return RC::CANNOT_MEET_FS_REQUIREMENTS;
    }

    return rc;
}

//------------------------------------------------------------------------------
// AlwaysEnableRule
AlwaysEnableRule::AlwaysEnableRule
(
    FsElementSet *pElementSet,
    UINT32 bit
)
    : m_pFsElementSet(pElementSet)
    , m_AlwaysEnableBit(bit)
{
    MASSERT(pElementSet);
}

string AlwaysEnableRule::GetName() const
{
    string name = Utility::StrPrintf("AlwaysEnableRule-%s%u",
                                      Downbin::ToString(m_pFsElementSet->GetFsUnit()).c_str(),
                                      m_AlwaysEnableBit);
    return name;
}

RC AlwaysEnableRule::Execute()
{
    if (m_pFsElementSet->GetTotalDisableMask()->IsSet(m_AlwaysEnableBit))
    {
        Printf(Tee::PriError, "%s%u disabled, but should always be enabled\n",
                               Downbin::ToString(m_pFsElementSet->GetFsUnit()).c_str(),
                               m_AlwaysEnableBit);
        return RC::CANNOT_MEET_FS_REQUIREMENTS;
    }
    return RC::OK;
}

//-----------------------------------------------------------------------------
// SubElementDependencyRule
SubElementDependencyRule::SubElementDependencyRule
(
    FsGroup *pFsGroup,
    FsUnit fsUnit,
    FsUnit subFsUnit
)
{
    MASSERT(pFsGroup);

    m_pElementSet    = pFsGroup->GetElementSet(fsUnit);
    m_pSubElementSet = pFsGroup->GetElementSet(subFsUnit);
    MASSERT(m_pElementSet);
    MASSERT(m_pSubElementSet);

    UINT32 numElements      = m_pElementSet->GetNumElements();
    UINT32 numSubElements   = m_pSubElementSet->GetNumElements();
    Utility::GetUniformDistribution(numElements, numSubElements, &m_SubElementDistribution);
}

string SubElementDependencyRule::GetName() const
{
    string name = Utility::StrPrintf("%s-%s-DependencyRule",
                                      Downbin::ToString(m_pElementSet->GetFsUnit()).c_str(),
                                      Downbin::ToString(m_pSubElementSet->GetFsUnit()).c_str());
    return name;
}

RC SubElementDependencyRule::Execute()
{
    RC rc;

    UINT32 numElements = m_pElementSet->GetNumElements();
    UINT32 subElementStartIdx = 0;
    UINT32 lwrSubElementMask = m_pSubElementSet->GetTotalDisableMask()->GetMask();
    for (UINT32 i = 0; i < numElements; i++)
    {
        UINT32 fullSubElementMask = ((1 << m_SubElementDistribution[i]) - 1) <<
                                     subElementStartIdx;
        // If element is disabled, disable all subelements
        if (m_pElementSet->GetTotalDisableMask()->IsSet(i))
        {
            CHECK_RC(m_pSubElementSet->DisableElements(fullSubElementMask));
        }
        // If all subelements corresponding to a element are disabled, disable the element
        else if ((lwrSubElementMask & fullSubElementMask) == fullSubElementMask)
        {
            CHECK_RC(m_pElementSet->DisableElement(i));
        }
        subElementStartIdx += m_SubElementDistribution[i];
    }

    return rc;
}

//-----------------------------------------------------------------------------
// CrossElementDependencyRule
CrossElementDependencyRule::CrossElementDependencyRule
(
    FsSet *pFsSet,
    FsUnit fsUnit,
    UINT32 groupIdx1,
    UINT32 elementGroupIdx1,
    UINT32 groupIdx2,
    UINT32 elementGroupIdx2
)
    : m_FsUnit(fsUnit)
    , m_GroupIdx1(groupIdx1)
    , m_ElementIdx1(elementGroupIdx1)
    , m_GroupIdx2(groupIdx1)
    , m_ElementIdx2(elementGroupIdx2)
{
    MASSERT(pFsSet);
    MASSERT(pFsSet->HasFsUnit(fsUnit));

    m_GroupFsUnit = pFsSet->GetGroupFsUnit();
    if (fsUnit == m_GroupFsUnit)
    {
        m_pFsElementSet1 = pFsSet->GetGroupElementSet();
        m_pFsElementSet2 = pFsSet->GetGroupElementSet();
    }
    else
    {
        MASSERT(pFsSet->HasElementFsUnit(fsUnit));
        m_pFsElementSet1 = pFsSet->GetElementSet(groupIdx1, fsUnit);
        m_pFsElementSet2 = pFsSet->GetElementSet(groupIdx2, fsUnit);
    }
}

string CrossElementDependencyRule::GetName() const
{
    string name;
    if (m_FsUnit == m_GroupFsUnit)
    {
        name = Utility::StrPrintf("%s%u,%s%u CrossDependencyRule",
                                   Downbin::ToString(m_FsUnit).c_str(),
                                   m_ElementIdx1,
                                   Downbin::ToString(m_FsUnit).c_str(),
                                   m_ElementIdx2);
    }
    else
    {
        name = Utility::StrPrintf("%s%u-%s%u,%s%u-%s%u CrossDependencyRule",
                                   Downbin::ToString(m_GroupFsUnit).c_str(),
                                   m_GroupIdx1,
                                   Downbin::ToString(m_FsUnit).c_str(),
                                   m_ElementIdx1,
                                   Downbin::ToString(m_GroupFsUnit).c_str(),
                                   m_GroupIdx2,
                                   Downbin::ToString(m_FsUnit).c_str(),
                                   m_ElementIdx2);
    }
    return name;
}

RC CrossElementDependencyRule::Execute()
{
    RC rc;

    if (m_pFsElementSet1->GetTotalDisableMask()->IsSet(m_ElementIdx1))
    {
        CHECK_RC(m_pFsElementSet2->DisableElement(m_ElementIdx2));
    }

    return rc;
}

//-----------------------------------------------------------------------------
// IlwalidElementComboRule
IlwalidElementComboRule::IlwalidElementComboRule
(
    FsElementSet *pElementSet,
    UINT32 mask
)
    : m_pFsElementSet(pElementSet)
    , m_IlwalidComboMask(mask)
{
    MASSERT(pElementSet);
}

string IlwalidElementComboRule::GetName() const
{
    FsUnit fsUnit = m_pFsElementSet->GetFsUnit();
    string name = Utility::StrPrintf("Invalid%sComboRule - 0x%x",
                                      Downbin::ToString(fsUnit).c_str(),
                                      m_IlwalidComboMask);
    return name;
}

RC IlwalidElementComboRule::Execute()
{
    RC rc;

    UINT32 disableMask = m_pFsElementSet->GetTotalDisableMask()->GetMask();
    if ((disableMask & m_IlwalidComboMask) == m_IlwalidComboMask)
    {
        FsGroup *pFsGroup = m_pFsElementSet->GetParentGroup();
        string fsUnitStr = Downbin::ToString(m_pFsElementSet->GetFsUnit());
        if (pFsGroup)
        {
            // Disable all elements in the set as well as parent group
            CHECK_RC(m_pFsElementSet->DisableAllElements());

            UINT32 fsGroupIdx = pFsGroup->GetIndex();
            Printf(Tee::PriLow, "Invalid %s mask 0x%x. Disabling %s%u\n",
                                 fsUnitStr.c_str(), disableMask,
                                 Downbin::ToString(pFsGroup->GetGroupFsUnit()).c_str(), fsGroupIdx);
            CHECK_RC(pFsGroup->DisableGroup());
        }
        else
        {
            // No parent group to disable
            Printf(Tee::PriError, "Invalid %s mask 0x%x\n",
                                   fsUnitStr.c_str(), disableMask);
            return RC::CANNOT_MEET_FS_REQUIREMENTS;
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
// GH100 L2 slice rule
GH100L2SliceRule::GH100L2SliceRule
(
    FsSet *pFsSet,
    UINT32 maxSlicesFs,
    UINT32 maxSlicesFsPerFbp
)
    : m_pFbpSet(pFsSet)
    , m_MaxSlicesFs(maxSlicesFs)
    , m_MaxSlicesFsPerFbp(maxSlicesFsPerFbp)
{
    MASSERT(pFsSet);
    MASSERT(pFsSet->GetGroupFsUnit() == FsUnit::FBP);
    MASSERT(pFsSet->HasElementFsUnit(FsUnit::LTC));
    MASSERT(pFsSet->HasElementFsUnit(FsUnit::L2_SLICE));

    m_NumFbps = pFsSet->GetNumGroups();
    m_NumLtcsPerFbp = pFsSet->GetNumElementsPerGroup(FsUnit::LTC);
    m_NumSlicesPerFbp = pFsSet->GetNumElementsPerGroup(FsUnit::L2_SLICE);
}

string GH100L2SliceRule::GetName() const
{
    string name = "GH100L2SliceRules";
    return name;
}

RC GH100L2SliceRule::Execute()
{
    RC rc;

    const UINT32 numSlicesPerLtc = m_NumSlicesPerFbp / m_NumLtcsPerFbp;
    const UINT32 ltcSliceFullMask = (1 << numSlicesPerLtc) - 1;

    FsElementSet *pFbElementSet = m_pFbpSet->GetGroupElementSet();

    // Check if we have any slice FSing, and disable the LTC if so
    bool bAllowSliceFsing = IsSliceFsAllowed();
    if (!bAllowSliceFsing)
    {
        for (UINT32 fbp = 0; fbp < m_NumFbps; fbp++)
        {
            if (!pFbElementSet->GetTotalDisableMask()->IsSet(fbp))
            {
                FsElementSet *pLtcSet   = m_pFbpSet->GetElementSet(fbp, FsUnit::LTC);
                FsElementSet *pSliceSet = m_pFbpSet->GetElementSet(fbp, FsUnit::L2_SLICE);

                const FsMask *pFbpSliceMask = pSliceSet->GetTotalDisableMask();
                for (UINT32 ltc = 0; ltc < m_NumLtcsPerFbp; ltc++)
                {
                    if (!pLtcSet->GetTotalDisableMask()->IsSet(ltc))
                    {
                        UINT32 ltcSliceMask = (pFbpSliceMask->GetMask() >> (ltc * numSlicesPerLtc)) &
                                              ltcSliceFullMask;
                        UINT32 numLtcSlicesFs = Utility::CountBits(ltcSliceMask);
                        if (numLtcSlicesFs != 0)
                        {
                            CHECK_RC(m_pFbpSet->DisableElement(FsUnit::LTC, fbp, ltc));
                            // The LTC rule should take care of this too if set up corrected,
                            // but adding it in anyway for completeness
                            CHECK_RC(m_pFbpSet->DisableElements(FsUnit::L2_SLICE, fbp,
                                                                ltcSliceFullMask << (ltc * numSlicesPerLtc)));
                        }
                    }
                }
            }
        }
    }

    return rc;
}

bool GH100L2SliceRule::IsSliceFsAllowed()
{
    // Slice floorsweeping requirements -
    //
    // 1. L2 slice floorsweeping can't be combined with LTC/FBP floorsweeping
    // 2. Max 'x' slice pairs are allowed to be floorswept on the chip
    //    (post this we do down to LTC or FBP FSing)
    // 3. Only 'y' slices can be floorswept per FBP

    bool bAllowSliceFsing = true;
    UINT32 totalSlicesFs = 0;
    FsElementSet *pFbElementSet = m_pFbpSet->GetGroupElementSet();
    for (UINT32 fbp = 0; fbp < m_NumFbps; fbp++)
    {
        if (pFbElementSet->GetTotalDisableMask()->IsSet(fbp))
        {
            // Slice FSing can't be combined with FB FSing
            bAllowSliceFsing = false;
            break;
        }
        else
        {
            FsElementSet *pLtcSet   = m_pFbpSet->GetElementSet(fbp, FsUnit::LTC);
            for (UINT32 ltc = 0; ltc < m_NumLtcsPerFbp; ltc++)
            {
                if (pLtcSet->GetTotalDisableMask()->IsSet(ltc))
                {
                    // Slice FSing can't be combined with LTC FSing
                    bAllowSliceFsing = false;
                    break;
                }
            }

            FsElementSet *pSliceSet = m_pFbpSet->GetElementSet(fbp, FsUnit::L2_SLICE);
            UINT32 numFbSlicesFs = pSliceSet->GetTotalDisableMask()->GetNumSetBits();
            if (numFbSlicesFs > m_MaxSlicesFsPerFbp)
            {
                // Restrictions on max number of slice floorswept per FBP
                bAllowSliceFsing = false;
                break;
            }
            totalSlicesFs += numFbSlicesFs;
        }
    }
    if (totalSlicesFs > m_MaxSlicesFs)
    {
        // Restrictions on total slices floorswept on the chip
        bAllowSliceFsing = false;
    }

    return bAllowSliceFsing;
}

VGpcSkylineRule::VGpcSkylineRule
(
    FsSet *pFsSet,
    const vector<UINT32> &vGpcSkylineList
)
    : m_pGpcSet(pFsSet)
    , m_vGpcSkylineList(vGpcSkylineList)
{
    MASSERT(pFsSet);
    MASSERT(pFsSet->GetGroupFsUnit() == FsUnit::GPC);
    MASSERT(pFsSet->HasElementFsUnit(FsUnit::TPC));
}

string VGpcSkylineRule::GetName() const
{
    string name = "VGpcSkylineRule";
    return name;
}

RC VGpcSkylineRule::Execute()
{
    RC rc;
    vector<UINT32> numsEnabledTpcPerGpc;
    CHECK_RC(m_pGpcSet->GetNumsEnabledElementsPerGroup(FsUnit::TPC, &numsEnabledTpcPerGpc));

    if (numsEnabledTpcPerGpc.size() < m_vGpcSkylineList.size())
    {
        Printf(Tee::PriError, "Length of vGpc Skyline List: %lu should not exceed GPC count: %lu\n",
                m_vGpcSkylineList.size(), numsEnabledTpcPerGpc.size());
        return RC::BAD_PARAMETER;
    }

    string numsEnabledTpcPerGpcStr = "[";
    for (auto num : numsEnabledTpcPerGpc)
    {
        numsEnabledTpcPerGpcStr += Utility::StrPrintf("%d ", num);
    }
    numsEnabledTpcPerGpcStr += "]";

    string vGpcSkylineStr = "[";
    for (auto num : m_vGpcSkylineList)
    {
        vGpcSkylineStr += Utility::StrPrintf("%d ", num);
    }
    vGpcSkylineStr += "]";
    
    // Sort the numsEnabledTpcPerGpc vector and compare with m_VGpcSkylineList
    // m_VGpcSkylineList is defined as a sorted array of TPC counts
    std::sort(numsEnabledTpcPerGpc.begin(), numsEnabledTpcPerGpc.end());

    vector<UINT32> padVGpcSkyLineList = m_vGpcSkylineList;
    // Pad the VGpcSkylineList with 0, if the length of it is shorter than GPC count
    padVGpcSkyLineList.insert(padVGpcSkyLineList.begin(), 
                numsEnabledTpcPerGpc.size() - padVGpcSkyLineList.size(), 0);
    
    for (UINT32 i = 0; i < numsEnabledTpcPerGpc.size(); i++)
    {
        if (numsEnabledTpcPerGpc[i] < padVGpcSkyLineList[i])
        {
            Printf(Tee::PriError, "Not satisfy VGpcSkylineRule. vGpc Sky Line is %s."
                    "Actual numbers of enabled TPC is %s\n", vGpcSkylineStr.c_str(),
                    numsEnabledTpcPerGpcStr.c_str());
            return RC::CANNOT_MEET_FS_REQUIREMENTS;
        }
    }
    return RC::OK;
}

