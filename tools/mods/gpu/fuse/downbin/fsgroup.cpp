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

#include "core/include/massert.h"

#include "downbinrules.h"
#include "fsgroup.h"
#include "fsset.h"

using namespace Downbin;

//------------------------------------------------------------------------------
FsGroup::FsGroup(FsUnit groupUnit, UINT32 index)
    : m_GroupFsUnit(groupUnit)
    , m_Index(index)
{
}

FsGroup::FsGroup(FsGroup&& o)
    : m_GroupFsUnit(std::move(o.m_GroupFsUnit))
    , m_Index(std::move(o.m_Index))
    , m_bIsDisabled(std::move(o.m_bIsDisabled))
    , m_bIsProtected(std::move(o.m_bIsProtected))
    , m_ElementSets(std::move(o.m_ElementSets))
    , m_OnGroupDisabledListeners(std::move(o.m_OnGroupDisabledListeners))
{
    SetParent(o.m_pParentSet);
    o.m_pParentSet = nullptr;
}

FsGroup& FsGroup::operator=(FsGroup&& o)
{
    if (this != &o)
    {
        m_GroupFsUnit  = std::move(o.m_GroupFsUnit);
        m_Index        = std::move(o.m_Index);
        m_bIsDisabled  = std::move(o.m_bIsDisabled);
        m_bIsProtected = std::move(o.m_bIsProtected);
        m_ElementSets  = std::move(o.m_ElementSets);
        m_OnGroupDisabledListeners = std::move(o.m_OnGroupDisabledListeners);

        m_pParentSet   = o.m_pParentSet;
        SetParent(o.m_pParentSet);
        o.m_pParentSet = nullptr;
    }
    return *this;
}

//------------------------------------------------------------------------------
void FsGroup::SetParent(FsSet *pParentSet)
{
    MASSERT(pParentSet);
    m_pParentSet = pParentSet;

    for (auto& elementSet : m_ElementSets)
    {
        elementSet.second.SetParent(m_pParentSet, this);
    }
}

//------------------------------------------------------------------------------
RC FsGroup::AddElementSet
(
    FsUnit fsUnit,
    UINT32 numElements,
    bool bIsDerivative
)
{
    // Form element set for given FS unit
    FsElementSet fsElement(fsUnit, numElements, bIsDerivative);
    fsElement.SetParent(m_pParentSet, this);

    // Add to element set map
    m_ElementSets.emplace(fsUnit, std::move(fsElement));

    return RC::OK;
}

//------------------------------------------------------------------------------
const FsElementSet* FsGroup::GetElementSet(FsUnit fsUnit) const
{
    if (m_ElementSets.find(fsUnit) == m_ElementSets.end())
    {
        MASSERT("Element set not found in group\n");
        return nullptr;
    }
    return static_cast<const FsElementSet*>(&m_ElementSets.at(fsUnit));
}

//------------------------------------------------------------------------------
FsElementSet* FsGroup::GetElementSet(FsUnit fsUnit)
{
    if (m_ElementSets.find(fsUnit) == m_ElementSets.end())
    {
        MASSERT("Element set not found in group\n");
        return nullptr;
    }
    return &m_ElementSets.at(fsUnit);
}

//------------------------------------------------------------------------------
void FsGroup::GetElementFsUnits(vector<FsUnit> *pFsUnits) const
{
    MASSERT(pFsUnits);

    for (const auto& elementSet : m_ElementSets)
    {
        pFsUnits->push_back(elementSet.first);
    }
}

//------------------------------------------------------------------------------
RC FsGroup::DisableGroup()
{
    RC rc;

    if (m_bIsDisabled)
    {
        // Do nothing
        return RC::OK;
    }

    m_bIsDisabled = true;
    // Mark group element as disabled
    CHECK_RC(m_pParentSet->GetGroupElementSet()->DisableElement(m_Index));
    // Call on group disabled listeners
    for (auto &pListener : m_OnGroupDisabledListeners)
    {
        CHECK_RC(pListener->Execute());
    }

    return rc;
}

//------------------------------------------------------------------------------
RC FsGroup::DisableElement(FsUnit fsUnit, UINT32 elementNum)
{
    return m_ElementSets.at(fsUnit).DisableElement(elementNum);
}

//------------------------------------------------------------------------------
RC FsGroup::DisableElements(FsUnit fsUnit, UINT32 elementMask)
{
    return m_ElementSets.at(fsUnit).DisableElements(elementMask);
}

//------------------------------------------------------------------------------
RC FsGroup::DisableAllElements(FsUnit fsUnit)
{
    return m_ElementSets.at(fsUnit).DisableAllElements();
}

//------------------------------------------------------------------------------
RC FsGroup::MarkProtected()
{
    m_bIsProtected = true;
    return RC::OK;
}

//------------------------------------------------------------------------------
void FsGroup::AddOnGroupDisabledListener(shared_ptr<DownbinRule> pRule)
{
    m_OnGroupDisabledListeners.push_back(pRule);
}
