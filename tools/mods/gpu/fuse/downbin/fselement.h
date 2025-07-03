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

#include "core/include/setget.h"

#include "downbinfs.h"
#include "downbinrules.h"
#include "fsmask.h"

class FsGroup;
class FsSet;

//!
//! \brief Set of floorsweepable elements.
//!        Eg. All the TPCs belonging to GPC0
//!
class FsElementSet
{
    using FsUnit       = Downbin::FsUnit;
    using FuseMaskType = Downbin::FuseMaskType;
    using Settings     = Downbin::Settings;

public:
    FsElementSet(FsUnit fsUnit, UINT32 numElements, bool bIsDerivative);
    virtual ~FsElementSet() = default;

    // Non-copyable
    FsElementSet(const FsElementSet&)             = delete;
    FsElementSet& operator=(const FsElementSet&)  = delete;

    // Move constructor
    FsElementSet(FsElementSet&& fsSet)            = default;
    FsElementSet& operator=(FsElementSet&& fsSet) = default;

    //!
    //! \brief Set the FS set objects the elements belong to
    //!
    //! \param pParentSet   Pointer to the parent FsSet object
    //!
    void SetParent(FsSet *pParentSet);
    //!
    //! \brief Set the FS class objects the elements belong to
    //!
    //! \param pParentSet   Pointer to the parent FsSet object
    //! \param pParentGroup Pointer to the parent FsGroup object
    //!
    void SetParent(FsSet *pParentSet, FsGroup *pParentGroup);

    //!
    //! Return a pointer to the parent FS group
    //!
    FsGroup* GetParentGroup()    { return m_pParentGroup; }
    //!
    //! Return a pointer to the parent FS set
    //!
    FsSet* GetParentSet()        { return m_pParentSet; }

    GET_PROP(FsUnit     , FsUnit);
    GET_PROP(NumElements, UINT32);
    bool IsDerivative()                       { return m_bIsDerivative; }
    void SetIsDerivative(bool bIsDerivative)  { m_bIsDerivative = bIsDerivative; }

    //!
    //! \brief Pick and disable one element that is enabled lwrrently
    //!        Also handles all the functions linked to the element's disablement
    //!
    RC DisableOneElement();
    //!
    //! \brief Disable the element at the given index
    //!        Also handles all the functions linked to the element's disablement
    //!
    //! \param bit  The index of the element to be disabled
    //!
    RC DisableElement(UINT32 bit);
    //!
    //! \brief Disable the elements corresponding to the given bitmask
    //!        Also handles all the functions linked to the elements' disablement
    //!
    //! \param bitMask Mask corresponding to the elements to be disabled
    //!
    RC DisableElements(UINT32 bitMask);
    //!
    //! \brief Disable all the elements in this set
    //!        Also handles all the functions linked to the elements' disablement
    //!
    RC DisableAllElements();
    //!
    //! \brief Disable the elements corresponding to the given bitmask
    //!        Also handles all the functions linked to the elements' disablement
    //!
    //! \param bitMask   Mask corresponding to the elements to be disabled
    //! \param settings  Downbin related settings
    //!
    RC DisableElements(UINT32 bitMask, const Settings& downbinSettings);

    //!
    //! \brief Mark this element to be enabled at all times
    //!
    //! \param bit  The index of the element to be marked as protect
    //!
    RC MarkProtected(UINT32 bit);

    //!
    //! \brief Returns if the element at the given bit is marked as defective
    //!
    //! \param bit  The index of the element to check
    //!
    bool IsDefective(UINT32 bit) const;
    //!
    //! \brief Returns if the element at the given bit is marked as disabled
    //!
    //! \param bit  The index of the element to check
    //!
    bool IsDisabled(UINT32 bit) const;
    //!
    //! \brief Returns if the element at the given bit is marked as protected
    //!
    //! \param bit  The index of the element to check
    //!
    bool IsProtected(UINT32 bit) const;
    //!
    //! \brief Returns if the element at the given bit is marked as reconfig
    //!
    //! \param bit  The index of the element to check
    //!
    bool IsReconfig(UINT32 bit) const;

    //!
    //! \brief Returns a pointer to the mask of the given type
    //!
    //! \param type  Fuse mask type
    //!
    const FsMask *GetFsMask(FuseMaskType type) const;

    const FsMask *GetDefectiveMask() const
        { return static_cast<const FsMask*>(&m_DefectiveMask); }
    const FsMask *GetDisableMask() const
        { return static_cast<const FsMask*>(&m_DisableMask); }
    const FsMask *GetReconfigMask() const
        { return static_cast<const FsMask*>(&m_ReconfigMask); }
    const FsMask *GetProtectedMask() const
        { return static_cast<const FsMask*>(&m_ProtectedMask); }
    const FsMask *GetTotalDisableMask() const
        { return static_cast<const FsMask*>(&m_TotalDisableMask); }

    //!
    //! \brief Add a downbin rule to be ilwoked every time an element
    //!        is newly disabled
    //!
    //! \param pRule Downbin rule
    //!
    void AddOnElementDisabledListener(shared_ptr<DownbinRule> pRule);

private:
    // Pointer back to the parent group object so it can access group details / functions
    FsGroup* m_pParentGroup = nullptr;

    // Pointer back to the parent set object so it can access group details / functions
    FsSet*   m_pParentSet   = nullptr;

    // Floorsweeping unit the element set represents
    FsUnit   m_FsUnit        = FsUnit::NONE;
    // Number of elements in the set
    UINT32   m_NumElements   = 0;
    // If the element is not a direct FS element in the set
    bool     m_bIsDerivative = false;

    // Mask corresponding to the defective fuse mask
    FsMask m_DefectiveMask;
    // Mask corresponding to the disable fuse mask
    FsMask m_DisableMask;
    // Mask corresponding to the reconfig fuse mask
    FsMask m_ReconfigMask;

    // Mask corresponding to the must-always-enable elements in the set
    FsMask m_ProtectedMask;

    // Aggregate mask corresponding to all disabled elements in the set
    // (includes both disable and reconfig masks)
    FsMask m_TotalDisableMask;

    // List of all functions that must be called when an element is newly
    // disabled
    vector<shared_ptr<DownbinRule>> m_OnElementDisabledListeners;
};
