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

#include "fselement.h"
#include "downbinimpl.h"
#include "downbinrules.h"

using namespace Downbin;

//------------------------------------------------------------------------------
FsElementSet::FsElementSet(FsUnit fsUnit, UINT32 numElements, bool bIsDerivative)
    : m_FsUnit(fsUnit)
    , m_NumElements(numElements)
    , m_bIsDerivative(bIsDerivative)
    , m_DefectiveMask(numElements)
    , m_DisableMask(numElements)
    , m_ReconfigMask(numElements)
    , m_ProtectedMask(numElements)
    , m_TotalDisableMask(numElements)
{
}

//------------------------------------------------------------------------------
void FsElementSet::SetParent(FsSet *pParentSet)
{
    MASSERT(pParentSet);
    SetParent(pParentSet, nullptr);
}

//------------------------------------------------------------------------------
void FsElementSet::SetParent(FsSet *pParentSet, FsGroup *pParentGroup)
{
    MASSERT(pParentSet);
    // Parent group may be null, since FsElementSet can also represent a main group's elements

    m_pParentSet   = pParentSet;
    m_pParentGroup = pParentGroup;
}

//------------------------------------------------------------------------------
RC FsElementSet::DisableOneElement()
{
    // TODO - Pick an element
    // Call DisableElement(bit)
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
RC FsElementSet::DisableElement(UINT32 bit)
{
    return DisableElements(1U << bit);
}

//------------------------------------------------------------------------------
RC FsElementSet::DisableAllElements()
{
    return DisableElements(m_DisableMask.GetFullMask());
}

//------------------------------------------------------------------------------
RC FsElementSet::DisableElements(UINT32 bitMaskToDisable)
{
    const Settings* pSettings = m_pParentSet->GetDownbinSettings();
    return DisableElements(bitMaskToDisable, *pSettings);
}

//------------------------------------------------------------------------------
RC FsElementSet::DisableElements(UINT32 bitMaskToDisable, const Settings &settings)
{
    RC rc;
    // TODO: use FsMask in all places instead of UINT32 masks.
    FsMask bitMask, refMask;
    refMask.SetMask(bitMaskToDisable);
    bitMask.SetIlwertedMask(m_TotalDisableMask, refMask);
    if (bitMask.IsEmpty())
    {
        // Already disabled, nothing to do
        return RC::OK;
    }

    // Disable elements
    CHECK_RC(m_TotalDisableMask.SetBits(bitMask.GetMask()));
    if (settings.bModifyDefective)
    {
        CHECK_RC(m_DefectiveMask.SetBits(bitMask.GetMask()));
    }
    if (settings.bModifyReconfig)
    {
        CHECK_RC(m_ReconfigMask.SetBits(bitMask.GetMask()));
    }
    else
    {
        CHECK_RC(m_DisableMask.SetBits(bitMask.GetMask()));
    }

    // Call on element disabled listeners
    for (auto &pListener : m_OnElementDisabledListeners)
    {
        CHECK_RC(pListener->Execute());
    }

    return rc;
}

//------------------------------------------------------------------------------
RC FsElementSet::MarkProtected(UINT32 bit)
{
    return m_ProtectedMask.SetBit(bit);
}

//------------------------------------------------------------------------------
bool FsElementSet::IsDefective(UINT32 bit) const
{
    MASSERT(bit < m_NumElements);
    return m_DefectiveMask.IsSet(bit);
}

//------------------------------------------------------------------------------
bool FsElementSet::IsDisabled(UINT32 bit) const
{
    MASSERT(bit < m_NumElements);
    return m_DisableMask.IsSet(bit);
}

//------------------------------------------------------------------------------
bool FsElementSet::IsReconfig(UINT32 bit) const
{
    MASSERT(bit < m_NumElements);
    return m_ReconfigMask.IsSet(bit);
}

//------------------------------------------------------------------------------
bool FsElementSet::IsProtected(UINT32 bit) const
{
    MASSERT(bit < m_NumElements);
    return m_ProtectedMask.IsSet(bit);
}

//------------------------------------------------------------------------------
const FsMask* FsElementSet::GetFsMask(FuseMaskType type) const
{
    switch (type)
    {
        case FuseMaskType::DISABLE  :   return GetDisableMask();
        case FuseMaskType::DEFECTIVE:   return GetDefectiveMask();
        case FuseMaskType::RECONFIG :   return GetReconfigMask();
        default                     :   return nullptr;
    }

    MASSERT("Invalid FS mask type\n");
    return nullptr;
}

//------------------------------------------------------------------------------
void FsElementSet::AddOnElementDisabledListener(shared_ptr<DownbinRule> pRule)
{
    m_OnElementDisabledListeners.push_back(pRule);
}
