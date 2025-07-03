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

#pragma once

#include <map>

#include "downbinrules.h"
#include "fselement.h"

class FsSet;

//!
//! \brief Group class to hold a single group bit (Eg. GPC) and the element sets
//!        (Eg. TPC, PESs for GPCs) and other dependents associated with that group.
//!
class FsGroup
{
    using FsUnit = Downbin::FsUnit;

public:
    FsGroup(FsUnit groupUnit, UINT32 index);
    virtual ~FsGroup() = default;

    // Non-copyable
    FsGroup(const FsGroup&)            = delete;
    FsGroup& operator=(const FsGroup&) = delete;

    // Move constructor
    FsGroup(FsGroup&& fsGroup);
    FsGroup& operator=(FsGroup&& fsGroup);

    //!
    //! \brief Set the FsSet object the elements belong to
    //!
    //! \param pParentSet Pointer to the parent FsSet object
    //!
    void SetParent(FsSet *pParentSet);
    //!
    //! Return a pointer to the parent FS set
    //!
    FsSet* GetParentSet()      { return m_pParentSet; }

    GET_PROP(Index,         UINT32);
    GET_PROP(GroupFsUnit,   FsUnit);

    //!
    //! \brief Add a set of elements to this group
    //!
    //! \param fsUnit        The floorsweeping unit the element set represents
    //! \param numElements   Number of elements per group
    //! \param bIsDerivative Is a derivative element or not
    //!
    RC AddElementSet
    (
        FsUnit fsUnit,
        UINT32 numElements,
        bool bIsDerivative
    );
    //!
    //! \brief Returns a pointer to the const element set for the given unit type
    //!
    //! \param fsUnit       The floorsweeping unit the element set represents
    //!
    const FsElementSet* GetElementSet(FsUnit fsUnit) const;
    //!
    //! \brief Returns a pointer to the element set for the given unit type
    //!
    //! \param fsUnit       The floorsweeping unit the element set represents
    //!
    FsElementSet* GetElementSet(FsUnit fsUnit);
    //!
    //! \brief Return the vector of all floorsweeping units belonging to this set
    //!
    //! \param[out] pFsUnits Vector of units to populate
    //!
    void GetElementFsUnits(vector<FsUnit> *pFsUnits) const;

    //!
    //! \brief Disable the given element
    //!
    //! \param fsUnit      The floorsweeping unit the element belongs to
    //! \param elementNum  Index of the element to be disabled
    //!
    RC DisableElement(FsUnit fsUnit, UINT32 elementNum);
    //!
    //! \brief Disable the elements corresponding to the given bit mask
    //!
    //! \param fsUnit      The floorsweeping unit the elements belong to
    //! \param elementNum  Bit mask for the elements to disable
    //!
    RC DisableElements(FsUnit fsUnit, UINT32 elementMask);
    //!
    //! \brief Disable all elements for the given fs unit
    //!
    //! \param fsUnit      The floorsweeping unit the elements belong to
    //!
    RC DisableAllElements(FsUnit fsUnit);
    //!
    //! \brief Mark the group as disabled
    //!        Also handles all the functions linked to the group's disablement
    //!
    RC DisableGroup();
    //!
    //! \brief Mark the group as must-be-enabled
    //!
    RC MarkProtected();

    //!
    //! \brief Returns if the group is marked as disabled
    //!
    bool IsDisabled() const   { return m_bIsDisabled;  }
    //!
    //! \brief Returns if the group is marked as protected
    //!
    bool IsProtected() const  { return m_bIsProtected; }

    //!
    //! \brief Add a downbin rule to be ilwoked when the group is newly disabled
    //!
    //! \param pRule Downbin rule
    //!
    void AddOnGroupDisabledListener(shared_ptr<DownbinRule> pRule);

private:
    // Pointer back to the parent set object so it can access group details / functions
    FsSet*  m_pParentSet   = nullptr;

    // The floorsweeping element the group is representing
    FsUnit  m_GroupFsUnit   = FsUnit::NONE;
    // Index of the group in the floorsweeping set
    UINT32  m_Index         = 0;

    // Is the group disabled
    bool   m_bIsDisabled    = false;
    // Is the group marked as always-enable
    bool   m_bIsProtected   = false;

    // Map of floorsweeping element sets belonging to this group
    map<FsUnit, FsElementSet> m_ElementSets;

    // List of all functions that must be called when an element is newly
    // disabled
    vector<shared_ptr<DownbinRule>> m_OnGroupDisabledListeners;
};
