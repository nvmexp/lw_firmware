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

#include "downbinfs.h"
#include "downbinpickers.h"
#include "fsgroup.h"

class DownbinImpl;

//!
//! \brief Class to represent a complete hierarchical set of floorsweepable elements
//!        belonging to a sub-system
//!        Eg. All GPCs + underlying element sets (Eg. TPCs, PESs, etc)
//!
class FsSet
{
    using FsUnit   = Downbin::FsUnit;
    using Settings = Downbin::Settings;

public:
    FsSet(FsUnit groupUnit, UINT32 numGroups);
    virtual ~FsSet() = default;

    // Non-copyable
    FsSet(const FsSet&)            = delete;
    FsSet& operator=(const FsSet&) = delete;

    // Move constructor
    FsSet(FsSet&& fsSet);
    FsSet& operator=(FsSet&& fsSet);

    //!
    //! \brief Set the downbin object the belong to
    //!
    //! \param pParentSet Pointer to the parent DownbinImpl object
    //!
    void SetParentDownbin(DownbinImpl *pDownbin);

    //!
    GET_PROP(NumGroups,     UINT32);
    GET_PROP(GroupFsUnit,   FsUnit);
    GET_PROP(PrimaryFsUnit, FsUnit);

    //!
    //! \brief Add a set of elements to this group
    //!
    //! \param fsUnit        The floorsweeping unit the element represents
    //! \param numElements   Number of elements per group
    //! \param bIsPrimary    If floorsweeping element is the primary hierarchial set
    //! \param bIsDerivative If floorsweeping element is a derivative
    //!
    RC AddElement
    (
        FsUnit fsUnit,
        UINT32 numElements,
        bool   bIsPrimary,
        bool   bIsDerivative
    );
    //!
    //! \brief Return the vector of all element floorsweeping units belonging to this set
    //!
    //! \param[out] pFsUnits Vector of units to populate
    //!
    void GetElementFsUnits(vector<FsUnit> *pFsUnits) const;
    //!
    //! \brief Return the vector of all floorsweeping units belonging to this set
    //!        (includes the group FS unit as well)
    //!
    //! \param[out] pFsUnits Vector of units to populate
    //!
    void GetFsUnits(vector<FsUnit> *pFsUnits) const;
    //!
    //! \brief Return if the given fs unit corresponds to the group represented in the set
    //!
    //! \param fsUnit       The floorsweeping unit to check
    //!
    bool IsGroupFsUnit(FsUnit fsUnit) const;

    //!
    //! \brief Return number of elements per group
    //!
    //! \param fsUnit       The floorsweeping unit the element represents
    //!
    UINT32 GetNumElementsPerGroup(FsUnit fsUnit) const;

    //!
    //! \brief Returns a pointer to the const group object at the given index
    //!
    //! \param groupIdx     Group Index
    //!
    const FsGroup* GetGroup(UINT32 groupIdx) const;
    //!
    //! \brief Returns a pointer to the const element set for the group floorsweeping unit
    //!
    const FsElementSet* GetGroupElementSet() const;
    //!
    //! \brief Returns a pointer to the const element set for the given unit type
    //!
    //! \param groupIdx     Group Index
    //! \param fsUnit       The floorsweeping unit the element set represents
    //!
    const FsElementSet* GetElementSet(UINT32 groupIdx, FsUnit fsUnit) const;
    //!
    //! \brief Returns a pointer to the group object at the given index
    //!
    //! \param groupIdx     Group Index
    //!
    FsGroup* GetGroup(UINT32 groupIdx);
    //!
    //! \brief Returns a pointer to the element set for the group floorsweeping unit
    //!
    FsElementSet* GetGroupElementSet();
    //!
    //! \brief Returns a pointer to the element set for the given unit type
    //!
    //! \param groupIdx     Group Index
    //! \param fsUnit       The floorsweeping unit the element set represents
    //!
    FsElementSet* GetElementSet(UINT32 groupIdx, FsUnit fsUnit);

    // Represents the fuse / FS information for a given FS unit
    struct FsUnitInfo
    {
        string    FuseName;             // floorsweeping element string represented in the fuse.
                                        // Eg. "sys_pipe"
        bool      bIsDisableFuse;       // Is the fuse a disable fuse
        bool      bIsPerGroupFuse;      // Are the fuse masks split across groups
        bool      bHasReconfig;         // Does the element have reconfig fuses

        FsUnitInfo()
        : FuseName("")
        , bIsDisableFuse(true)
        , bIsPerGroupFuse(false)
        , bHasReconfig(false)
        {}
    };
    string GetFuseName(FsUnit fsUnit) const;
    bool   IsDisableFuse(FsUnit fsUnit) const;
    bool   IsPerGroupFuse(FsUnit fsUnit) const;
    bool   HasReconfig(FsUnit fsUnit) const;
    void   SetFuseName(FsUnit fsUnit, string fuseName);
    void   SetIsDisableFuse(FsUnit fsUnit, bool bIsDisableFuse);
    void   SetIsPerGroupFuse(FsUnit fsUnit, bool bIsPerGroupFuse);
    void   SetHasReconfig(FsUnit fsUnit, bool bHasReconfig);

    //!
    //! \brief Pick and disable one group that is enabled lwrrently
    //!
    RC DisableOneGroup();
    //!
    //! \brief Pick and disable one element that is enabled lwrrently
    //!
    //! \param fsUnit       The floorsweeping unit the element belongs to
    //!
    RC DisableOneElement(FsUnit unit);
    //!
    //! \brief Disable the group at the given index
    //!
    //! \param bit  The index of the group to be disabled
    //!
    RC DisableGroup(UINT32 groupNum);
    //!
    //! \brief Disable the groups corresponding to the given bitmask
    //!
    //! \param bitMask Mask corresponding to the groups to be disabled
    //!
    RC DisableGroups(UINT32 groupMask);
    //!
    //! \brief Disable the given element
    //!
    //! \param fsUnit      The floorsweeping unit the element belongs to
    //! \param elementNum  Absolute index of the element to be disabled (across groups)
    //!
    RC DisableElement(FsUnit unit, UINT32 absElementNum);
    //!
    //! \brief Disable the given element
    //!
    //! \param fsUnit      The floorsweeping unit the element belongs to
    //! \param groupNum    Index of the group the element belongs to
    //! \param elementNum  Index of the element to be disabled in the given groups
    //!
    RC DisableElement(FsUnit unit, UINT32 groupNum, UINT32 elementNum);
    //!
    //! \brief Disable the given elements
    //!
    //! \param fsUnit      The floorsweeping unit the element belongs to
    //! \param groupNum    Index of the group the element belongs to
    //! \param elementMask Bit mask for the elements to disable in the given group
    //!
    RC DisableElements(FsUnit unit, UINT32 groupNum, UINT32 elementMask);
    RC DisableElements(FsUnit unit, const vector<UINT32> &elementMasks);

    //!
    //! \brief Get number of enabled units for the given element FS unit in the given group
    //!
    //! \param fsUnit           The floorsweeping unit the element belongs to
    //! \param groupNum         Index of the group the element belongs to       
    //! \param[out] pNumEnabled Number of enabled units of the above element
    //!
    RC GetGroupEnabledElements(FsUnit fsUnit, UINT32 groupNum, UINT32 *pNumEnabled) const;

    //!
    //! \brief Get total number of enabled units for the given FS unit
    //!
    //! \param fsUnit           The floorsweeping unit the element belongs to
    //! \param[out] pNumEnabled Number of enabled units of the above element
    //!
    RC GetTotalEnabledElements(FsUnit fsUnit, UINT32 *pNumEnabled) const;

    //!
    //! \brief Get total number of enabled units for the given FS unit
    //!
    //! \param fsUnit           The floorsweeping unit the element belongs to
    //! \param[out] pNumsEnabledPerGroup Numbers of enabled units per Group 
    //!
    RC GetNumsEnabledElementsPerGroup(FsUnit fsUnit, vector<UINT32> *pNumsEnabledPerGroup) const;

    //!
    //! \brief Get total number of disabled units for the given FS unit
    //!
    //! \param fsUnit           The floorsweeping unit the element belongs to
    //! \param[out] pNumDisabled Number of disabled units of the above element
    //!
    RC GetTotalDisabledElements(FsUnit fsUnit, UINT32 *pNumDisabled) const;

    //!
    //! \brief Get Reconfig element number for the given FS unit
    //!
    //! \param fsUnit           The floorsweeping unit the element belongs to
    //! \param[out] pNumReconfig Number of reconfig units of the above element
    //!
    RC GetTotalReconfigElements(FsUnit fsUnit, UINT32 *pNumReconfig) const;

    //!
    //! \brief Print details of the floorsweeping set
    //!
    //! \param pri Priority of the prints
    //!
    void PrintSet(Tee::Priority pri) const;

    //!
    //! \brief Return pointer to main downbin object
    //!
    DownbinImpl* GetDownbinImpl();

    //!
    //! \brief Returns if the floorsweeping unit is a part (group or element) of this set
    //!
    bool HasFsUnit(FsUnit fsUnit) const;
    //!
    //! \brief Returns if the floorsweeping unit is an element of this set
    //!
    bool HasElementFsUnit(FsUnit fsUnit) const;

    //!
    //! \brief Get downbin settings
    //!
    virtual const Settings* GetDownbinSettings() const;

    //!
    //! \brief Add a downbin picker to help choose a group to be disabled
    //!
    //! \param pPicker  Downbin group picker
    //!
    void AddGroupDisablePicker(unique_ptr<DownbinGroupPicker> pPicker);
    //!
    //! \brief Add a downbin picker to help choose a group from which
    //!        an element can be disabled
    //!
    //! \param fsUnit   The floorsweeping unit the element belongs to
    //! \param pPicker  Downbin group picker
    //!
    void AddGroupReducePicker(FsUnit fsUnit, unique_ptr<DownbinGroupPicker> pPicker);
    //!
    //! \brief Add a downbin picker to help choose an element to be disabled
    //!
    //! \param fsUnit   The floorsweeping unit the element belongs to
    //! \param pPicker  Downbin element picker
    //!
    void AddElementDisablePicker(FsUnit fsUnit, unique_ptr<DownbinElementPicker> pPicker);
   
    //!
    //! \brief Get the count of enabled units as per refGroupMask.
    //!       if fsUnit is specified as group fs unit, it simply returns the number of
    //!       set bits in refGroupMask
    //!       Otherwise, it gives the enable count of fsUnits for the groups 
    //!       in the refGroupMask
    //!
    //! \param refGroupMask Mask of the enabled fsGroups.
    //!       fsUnit       The floorsweeping unit whose enable count is desired
    //!       pEnableCount Stores the enable count
    //!
    RC GetPartialEnabledElements
    (
        const FsMask& refGroupMask,
        FsUnit fsUnit,
        UINT32 *pEnableCount
    );

    //!
    //! \brief Populate the element enable counts for all the UGPUs
    //!
    //! \param m_pDev  TestDevice object to access gpusubDevice functions
    //! \param refGroupMask  Mask of the enabled fsUnit
    //! \param fsUnit  The fs unit whose UGPU distribution is desired
    //! \param pUgpuUnitCounts  Vector to store enable counts for all UGPUs
    //!
    RC GetUgpuEnabledDistribution
    (
        TestDevice *pDev,
        FsMask refGroupMask,
        FsUnit fsUnit,
        vector<UINT32> *pUgpuUnitCounts
    );
private:
    //!
    //! \brief Adds element sets for the given floorsweeping unit
    //!
    //! \param fsUnit       The floorsweeping unit the element represents
    //! \param numElements  Number of elements per group
    //! \param bIsDerivative If floorsweeping element is a derivative
    //!
    RC AddElementSets(FsUnit fsUnit, UINT32 numElements, bool bIsDerivative);

    // Pointer back to main downbin object to get access common functions
    // and settings
    DownbinImpl *m_pDownbinImpl = nullptr;

    // Floorsweeping unit the group represents
    FsUnit m_GroupFsUnit   = FsUnit::NONE;
    // Number of instances of the group
    UINT32 m_NumGroups     = 0;

    // Floorsweeping unit representing the primary element
    FsUnit m_PrimaryFsUnit = FsUnit::NONE;

    // Element set representing the main group elements
    FsElementSet     m_GroupElementSet;
    // Vector of pointers to groups belonging to the set
    vector<FsGroup>  m_Groups;
    // Chip fuse details for the floorsweeping elements in the set
    map<FsUnit, FsUnitInfo>      m_FsUnitInfo;

    // Downbin pickers to choose a group to disable
    vector<unique_ptr<DownbinGroupPicker>> m_GroupDisablePickers;
    // Downbin pickers to choose a group to disable an element from
    map<FsUnit, vector<unique_ptr<DownbinGroupPicker>>> m_GroupReducePickers;
    // Downbin pickers to choose an element to disable
    map<FsUnit, vector<unique_ptr<DownbinElementPicker>>> m_ElementDisablePickers;
};
