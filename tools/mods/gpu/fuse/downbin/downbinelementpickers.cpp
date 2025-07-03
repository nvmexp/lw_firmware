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

#include "core/include/utility.h"

#include "downbinelementpickers.h"
#include "fsset.h"

using namespace Downbin;

//------------------------------------------------------------------------------
// MostEnabledSubElementPicker

MostEnabledSubElementPicker::MostEnabledSubElementPicker
(
    FsSet *pFsSet,
    FsUnit elementFsUnit,
    FsUnit subElementFsUnit
)
    : m_pFsSet(pFsSet)
    , m_ElementFsUnit(elementFsUnit)
    , m_SubElementFsUnit(subElementFsUnit)
{
    MASSERT(pFsSet);
    MASSERT(pFsSet->HasElementFsUnit(elementFsUnit));
    MASSERT(pFsSet->HasElementFsUnit(subElementFsUnit));

    UINT32 numElements    = m_pFsSet->GetElementSet(0, elementFsUnit)->GetNumElements();
    UINT32 numSubElements = m_pFsSet->GetElementSet(0, subElementFsUnit)->GetNumElements();
    Utility::GetUniformDistribution(numElements, numSubElements, &m_SubElementDistribution);
}

string MostEnabledSubElementPicker::GetPickerName() const
{
    string name = Utility::StrPrintf("MostEnabledSubElementPicker-%s-%s",
                                      Downbin::ToString(m_ElementFsUnit).c_str(),
                                      Downbin::ToString(m_SubElementFsUnit).c_str());
    return name;
}

RC MostEnabledSubElementPicker::Pick
(
    UINT32 grpIdx,
    const FsMask& inElementMask,
    FsMask *pOutElementMask
) const
{
    MASSERT(pOutElementMask);

    UINT32 numGroups = m_pFsSet->GetNumGroups();
    if (grpIdx >= numGroups)
    {
        Printf(Tee::PriError, "Input group index %u greater than max groups %u\n",
                               grpIdx, numGroups);
        return RC::BAD_PARAMETER;
    }

    const FsElementSet *pElementSet    = m_pFsSet->GetElementSet(grpIdx, m_ElementFsUnit);
    const FsElementSet *pSubElementSet = m_pFsSet->GetElementSet(grpIdx, m_SubElementFsUnit);
    UINT32 numElements = pElementSet->GetNumElements();
    UINT32 lwrSubElementMask = pSubElementSet->GetTotalDisableMask()->GetMask();
    UINT32 maxEnabledSubElements = 0;
    UINT32 subElementStartIdx = 0;
    pOutElementMask->ClearAll();
    for (UINT32 i = 0; i < numElements; i++)
    {
        UINT32 fullSubElementMask = ((1 << m_SubElementDistribution[i]) - 1) <<
                                     subElementStartIdx;
        // Check if there are any sub-elements available corresponding to this element
        if (fullSubElementMask & inElementMask.GetMask())
        {
            UINT32 subElementMask = fullSubElementMask & lwrSubElementMask;
            UINT32 numEnabledSubElements = m_SubElementDistribution[i] -
                                         Utility::CountBits(subElementMask);
            if (numEnabledSubElements == maxEnabledSubElements)
            {
                // Equally valid choice
                pOutElementMask->SetBits(~subElementMask & fullSubElementMask);
            }
            else if (numEnabledSubElements > maxEnabledSubElements)
            {
                // Lesser disable count => discard previously chosen elements
                pOutElementMask->SetMask(~subElementMask & fullSubElementMask);
                maxEnabledSubElements = numEnabledSubElements;
            }
        }
        subElementStartIdx += m_SubElementDistribution[i];
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
// AvailableElementPicker

AvailableElementPicker::AvailableElementPicker
(
    FsSet *pFsSet,
    FsUnit elementFsUnit
)
    : m_pFsSet(pFsSet)
    , m_ElementFsUnit(elementFsUnit)
{
    MASSERT(pFsSet);
    MASSERT(pFsSet->HasElementFsUnit(elementFsUnit));
}

string AvailableElementPicker::GetPickerName() const
{
    string name = Utility::StrPrintf("AvailableElementPicker-%s",
                                      Downbin::ToString(m_ElementFsUnit).c_str());
    return name;
}

RC AvailableElementPicker::Pick
(
    UINT32 grpIdx,
    const FsMask& inElementMask,
    FsMask *pOutElementMask
) const
{
    MASSERT(pOutElementMask);
    pOutElementMask->ClearAll();
    UINT32 numGroups = m_pFsSet->GetNumGroups();
    if (grpIdx >= numGroups)
    {
        Printf(Tee::PriError, "Input group index %u greater than max groups %u\n",
                               grpIdx, numGroups);
        return RC::BAD_PARAMETER;
    }
    const FsElementSet *pElementSet = m_pFsSet->GetElementSet(grpIdx, m_ElementFsUnit);
    const FsMask *pProtectedMask = pElementSet->GetProtectedMask();
    pOutElementMask->SetMask(inElementMask & ~(*pProtectedMask));
    return RC::OK;
}

//------------------------------------------------------------------------------
// MostDisabledDependentElementPicker

MostDisabledDependentElementPicker::MostDisabledDependentElementPicker
(
    FsSet *pFsSet,
    FsUnit elementFsUnit,
    FsUnit depElementFsUnit
)
    : m_pFsSet(pFsSet)
    , m_ElementFsUnit(elementFsUnit)
    , m_DepElementFsUnit(depElementFsUnit)
{
    MASSERT(pFsSet);
    MASSERT(pFsSet->HasElementFsUnit(elementFsUnit));
    MASSERT(pFsSet->HasElementFsUnit(depElementFsUnit));
}

string MostDisabledDependentElementPicker::GetPickerName() const
{
    string name = Utility::StrPrintf("MostDisabledDependentElementPicker-%s-%s",
                                      Downbin::ToString(m_ElementFsUnit).c_str(),
                                      Downbin::ToString(m_DepElementFsUnit).c_str());
    return name;
}

RC MostDisabledDependentElementPicker::Pick
(
    UINT32 grpIdx,
    const FsMask& inElementMask,
    FsMask *pOutElementMask
) const
{
    MASSERT(pOutElementMask);

    UINT32 numGroups = m_pFsSet->GetNumGroups();
    if (grpIdx >= numGroups)
    {
        Printf(Tee::PriError, "Input group index %u greater than max groups %u\n",
                               grpIdx, numGroups);
        return RC::BAD_PARAMETER;
    }

    const FsElementSet *pElementSet    = m_pFsSet->GetElementSet(grpIdx, m_ElementFsUnit);
    const FsElementSet *pDepElementSet = m_pFsSet->GetElementSet(grpIdx, m_DepElementFsUnit);
    UINT32 numElements = pElementSet->GetNumElements();
    UINT32 numDepElements = pDepElementSet->GetNumElements();

    MASSERT(numElements);
    UINT32 numDepPerElement = numDepElements / numElements;
    FsMask depElementProtectedMask(numDepElements); 
    depElementProtectedMask.SetMask(pDepElementSet->GetProtectedMask()->GetMask());

    FsMask tempInElementMask(numElements);
    tempInElementMask.SetMask(inElementMask.GetMask());
    // Remove elements with protected dependents
    for (UINT32 el = 0; el < numElements; el++)
    {
        FsMask depStrideMask(numDepElements);
        depStrideMask.SetMask(depElementProtectedMask.GetMask() >> (numDepPerElement * el));
        if (depStrideMask.GetMask() & ((1 << numDepPerElement) - 1))
        {
            tempInElementMask.ClearBit(el);
        }
    }
    pOutElementMask->ClearAll();
    FsMask depElementDisMask(numDepElements);
    depElementDisMask.SetMask(pDepElementSet->GetTotalDisableMask()->GetMask());
    UINT32 maxDisabledDepElements = 0;
    for (UINT32 ele = 0; ele < numElements; ele++)
    {
        // Proceed if current element is available
        if (tempInElementMask.IsSet(ele))
        {
            FsMask depStrideMask(numDepElements);
            depStrideMask.SetMask(depElementDisMask.GetMask() >> (numDepPerElement * ele));
            
            if (depStrideMask.GetNumSetBits() == maxDisabledDepElements)
            {
                // Equally valid element
                pOutElementMask->SetBits(1 << ele);
            }
            else if (depStrideMask.GetNumSetBits() > maxDisabledDepElements)
            {
                // has more disabled dependents, so discard previously chosen elements
                maxDisabledDepElements = depStrideMask.GetNumSetBits();
                pOutElementMask->SetMask(1 << ele);
            }
        }        
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
// PreferredElementPicker

PreferredElementPicker::PreferredElementPicker
(
    FsSet *pFsSet,
    FsUnit elementFsUnit,
    vector<FsMask>& preferredElementMasks
)
    : m_pFsSet(pFsSet)
    , m_ElementFsUnit(elementFsUnit)
    , m_PreferredElementMasks(preferredElementMasks)
{
    MASSERT(pFsSet);
    MASSERT(pFsSet->HasElementFsUnit(elementFsUnit));
    MASSERT(preferredElementMasks.size() == pFsSet->GetNumGroups());
}

string PreferredElementPicker::GetPickerName() const
{
    string name = Utility::StrPrintf("PreferredElementPicker-%s",
                                      Downbin::ToString(m_ElementFsUnit).c_str());
    return name;
}

RC PreferredElementPicker::Pick
(
    UINT32 grpIdx,
    const FsMask& inElementMask,
    FsMask *pOutElementMask
) const
{
    MASSERT(pOutElementMask);
    pOutElementMask->ClearAll();
    UINT32 numGroups = m_pFsSet->GetNumGroups();
    if (grpIdx >= numGroups)
    {
        Printf(Tee::PriError, "Input group index %u greater than max groups %u\n",
                               grpIdx, numGroups);
        return RC::BAD_PARAMETER;
    }
    const FsElementSet *pElementSet = m_pFsSet->GetElementSet(grpIdx, m_ElementFsUnit);
    const UINT32 numElements = pElementSet->GetNumElements();
    FsMask fullMask;
    fullMask.SetNumBits(numElements);
    if (m_PreferredElementMasks[grpIdx].GetMask() > fullMask.GetFullMask())
    {
        Printf(Tee::PriError, "PreferredElementMasks[%u]: 0x%x should not exceed full mask 0x%x",
                grpIdx, m_PreferredElementMasks[grpIdx].GetMask(), fullMask.GetFullMask());
        return RC::BAD_PARAMETER;
    }
    // if preferredElementMask == fullMask, directly return input Mask
    if (m_PreferredElementMasks[grpIdx].GetMask() == fullMask.GetFullMask())
    {
        pOutElementMask->SetMask(inElementMask);
        return RC::OK;
    }
    for (INT32 elementIdx = Utility::BitScanForward(inElementMask.GetMask());
         elementIdx >= 0;
         elementIdx = Utility::BitScanForward(inElementMask.GetMask(), elementIdx + 1))
    {
        if (m_PreferredElementMasks[grpIdx].IsSet(elementIdx))
        {
            pOutElementMask->SetBit(elementIdx);
        }
    }

    // No valid choice, return the input mask
    if (pOutElementMask->IsEmpty())
    {
        pOutElementMask->SetMask(inElementMask);
    }
    return RC::OK;
}
