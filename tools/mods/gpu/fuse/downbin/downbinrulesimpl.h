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

#include "downbinfs.h"
#include "downbinrules.h"

class FsElementSet;
class FsGroup;
class FsSet;

//-----------------------------------------------------------------------------

//!
//! \brief If a group is disabled, all underlying elements should be disabled
//!
class BasicGroupDisableRule : public DownbinRule
{
    using FsUnit = Downbin::FsUnit;
public:
    BasicGroupDisableRule(FsGroup *pFsGroup);

    string GetName() const override;

    RC Execute() override;

private:
    FsGroup *m_pFsGroup = nullptr;
    vector<FsUnit> m_FsUnitsToDisable;
};

//!
//! \brief Minimum number of elements required in the group, if active
//!        If condition is not met, disable the group.
//!
class MinGroupElementCountRule : public DownbinRule
{
public:
    MinGroupElementCountRule(FsElementSet *pElementSet, UINT32 minCount);

    string GetName() const override;

    RC Execute() override;

private:
    FsElementSet *m_pFsElementSet = nullptr;
    UINT32        m_MinCount      = 0;
};

//!
//! \brief Minimum number of subelements per active element required in the group
//!        If condition is not met, disable the element.
//!        Eg. Min L2 slices per LTC, Min TPCs per PES
//!
class MinSubElementCountRule : public DownbinRule
{
    using FsUnit = Downbin::FsUnit;
public:
    MinSubElementCountRule
    (
        FsGroup *pFsGroup,
        FsUnit elementFsUnit,
        FsUnit subElementFsUnit,
        UINT32 minCountPerElement
    );

    string GetName() const override;

    RC Execute() override;

private:
    UINT32 m_MinCountPerElement = 0;

    FsElementSet *m_pElementSet    = nullptr;
    FsElementSet *m_pSubElementSet = nullptr;

    vector<UINT32> m_SubElementDistribution;
};

//!
//! \brief Minimum number of elements required in the set
//!
class MinElementCountRule : public DownbinRule
{
    using FsUnit = Downbin::FsUnit;
public:
    MinElementCountRule(FsSet *pSet, FsUnit fsUnit, UINT32 minCount);
    MinElementCountRule
    (
        FsSet *pSet,
        FsUnit fsUnit,
        UINT32 minCount,
        vector<UINT32> masks
    );

    string GetName() const override;

    RC Execute() override;

private:
    FsSet *m_pFsSet   = nullptr;
    FsUnit m_FsUnit   = FsUnit::NONE;
    UINT32 m_MinCount = 0;
    vector<UINT32> m_ElementMasks;
};

//!
//! \brief The given elements must always be enabled
//!
class AlwaysEnableRule : public DownbinRule
{
public:
    AlwaysEnableRule(FsElementSet *pElementSet, UINT32 index);

    string GetName() const override;

    RC Execute() override;

private:
    FsElementSet *m_pFsElementSet = nullptr;

    UINT32 m_AlwaysEnableBit = UINT32_MAX;
};

//!
//! \brief FS elements are dependent on each other and must be disabled in accordance with
//!        eachother
//!        If 1 unit of element1 is disabled, all (equally distributed) corresponding units
//!        of element2 must be disabled. And vice versa.
//!
class SubElementDependencyRule : public DownbinRule
{
    using FsUnit = Downbin::FsUnit;
public:
    SubElementDependencyRule
    (
        FsGroup *pFsGroup,
        FsUnit fsUnit,
        FsUnit subFsUnit
    );

    string GetName() const override;

    RC Execute() override;

private:
    FsElementSet *m_pElementSet    = nullptr;
    FsElementSet *m_pSubElementSet = nullptr;

    vector<UINT32> m_SubElementDistribution;
};

//!
//! \brief FS elements are dependent on each other and must be disabled in accordance with
//!        eachother
//!
class CrossElementDependencyRule : public DownbinRule
{
    using FsUnit = Downbin::FsUnit;
public:
    CrossElementDependencyRule
    (
        FsSet *pFsSet,
        FsUnit fsUnit,
        UINT32 groupIdx1,
        UINT32 elementGroupIdx1,
        UINT32 groupIdx2,
        UINT32 elementGroupIdx2
    );

    string GetName() const override;

    RC Execute() override;

private:
    FsElementSet *m_pFsElementSet1 = nullptr;
    FsElementSet *m_pFsElementSet2 = nullptr;

    FsUnit m_FsUnit      = FsUnit::NONE;    // Fs unit of the element set
    FsUnit m_GroupFsUnit = FsUnit::NONE;    // Fs unit of the parent group (if any)
    UINT32 m_GroupIdx1   = UINT32_MAX;      // Parent group index of element to be checked for, if any
    UINT32 m_ElementIdx1 = UINT32_MAX;      // Element index of the element in above group
    UINT32 m_GroupIdx2   = UINT32_MAX;      // Parent group index of dependent element, if any
    UINT32 m_ElementIdx2 = UINT32_MAX;      // Element index of the element in above group
};

//!
//! \brief Handle invalid element mask combinations
//!        If parent group is present and we hit an invalid combination, disable the whole group itself
//!
class IlwalidElementComboRule : public DownbinRule
{
    using FsUnit = Downbin::FsUnit;
public:
    IlwalidElementComboRule
    (
        FsElementSet *pElementSet,
        UINT32 mask
    );

    string GetName() const override;

    RC Execute() override;

private:
    FsElementSet *m_pFsElementSet = nullptr;

    UINT32 m_IlwalidComboMask = 0;
};

//-----------------------------------------------------------------------------
// GH100 specific downbinning rules
// (We are looking to form more generic rules whereever possible.
//  However, attempting to generalise some very chip-specific rules can sometimes make the code
//  much more complicated)

//!
//! \brief GH100 allows L2 slice floorsweeping, but with some restrictions on the
//!        number of slices that can be floorswept per LTC / FBP ; and also other related
//!        constraints
//!        Checks if we conditions for slice floorsweeping are met, otherwise disable the
//!        corresponding LTC
//!
//!        NOTE - This rule assumes that some other FBP - LTC - L2 slice dependencies are set up -
//!               - If FBP is disabled, all underlying elements (LTCs, slices) will be disabled
//!               - If LTC is disabled, all underlying slices will be disbaled
//!
class GH100L2SliceRule : public DownbinRule
{
    using FsUnit = Downbin::FsUnit;
public:
    GH100L2SliceRule(FsSet *pFsSet, UINT32 maxSlicesFs, UINT32 maxSlicesFsPerFbp);

    string GetName() const override;

    RC Execute() override;

private:
    bool IsSliceFsAllowed();

    FsSet *m_pFbpSet = nullptr;

    UINT32 m_MaxSlicesFs = 0;
    UINT32 m_MaxSlicesFsPerFbp = 0;

    UINT32 m_NumFbps = 0;
    UINT32 m_NumLtcsPerFbp = 0;
    UINT32 m_NumSlicesPerFbp = 0;
};

//!
//! \brief The GH100 SKUs define a "vgpc skyline". This is a sorted list of TPC counts per GPC.
//! Refer: http://lwbugs/3359659
//! For example it could be {5,8,8,8,9,9,9,9}
//! you need 65 TPCs total
//! you need 1 GPCs with at least 5 TPCs
//! you need 3 GPCs with at least 8 TPCs
//! you need 4 GPCs with at least 9 TPCs
class VGpcSkylineRule : public DownbinRule
{
    using FsUnit = Downbin::FsUnit;

public:
    VGpcSkylineRule(FsSet *pFsSet, const vector<UINT32> &vGpcSkylineList);

    string GetName() const override;

    RC Execute() override;

private:

    FsSet *m_pGpcSet = nullptr;
    vector<UINT32> m_vGpcSkylineList;
};

//-----------------------------------------------------------------------------

