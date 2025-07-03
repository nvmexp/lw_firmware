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

#pragma once

#include "downbinfs.h"
#include "downbinpickers.h"

class FsSet;

//!
//! \brief Pick the groups that have most number of disabled elements/!
//!
class MostDisabledGroupPicker : public DownbinGroupPicker
{
    using FsUnit = Downbin::FsUnit;

public:
    MostDisabledGroupPicker(FsSet *pFsSet, FsUnit fsUnit);

    string GetPickerName() const override;

    RC Pick
    (
        const FsMask& InGroupMask,
        FsMask *pOutGroupMask
    ) const override;

private:
    FsSet *m_pFsSet        = nullptr;
    FsUnit m_ElementFsUnit = FsUnit::NONE;
};

//!
//!\brief Pick the ugpu having more number of enabled elements so that
//!       the difference in number of groups/elements accross UGPUs is less than
//!       max allowed imbalance
//!       Balancing is done on group count if fsUnit is same as the group FsUnit 
//!       of FsSet and on element count if fsUnit is an element FsUnit in FsSet
//!
class MostEnabledUgpuPicker : public DownbinGroupPicker
{
    using FsUnit = Downbin::FsUnit;

public:
    MostEnabledUgpuPicker
    (
        TestDevice *pDev,
        FsSet *pFsSet,
        FsUnit fsUnit
    );

    string GetPickerName() const override;

    RC Pick
    (
        const FsMask& bInGroupMask,
        FsMask *pOutGroupMask
    ) const override;

private:
    TestDevice *m_pDev = nullptr;
    FsSet *m_pFsSet    = nullptr;
    FsUnit m_FsUnit = FsUnit::NONE;
};

//!
//!\brief  Pick the groups with more number of available elements. Fewer is the
//!        number of protected or disabled elements more available is the group.
//!        If fsUnit = FsUnit::NONE, 
//!           pick those groups which are neither protected themselves nor contain 
//!           any elements which are protected.
//!        If bCheckDisabled = true, 
//!            check for disabled elements.This field is nop if fsUnit = FsUnit::NONE
//!        If bCheckProtected = true,
//!            check for protected elements.This field is nop if fsUnit = FsUnit::NONE
//!
class MostAvailableGroupPicker : public DownbinGroupPicker
{
    using FsUnit = Downbin::FsUnit;

public:
    MostAvailableGroupPicker
    (
        FsSet *pFsSet,
        FsUnit fsUnit,
        bool bCheckDisabled,
        bool bCheckProtected
    );

    string GetPickerName() const override;

    RC Pick
    (
        const FsMask& InGroupMask,
        FsMask *pOutGroupMask
    ) const override;

private:
    FsSet *m_pFsSet        = nullptr;
    FsUnit m_ElementFsUnit = FsUnit::NONE;
    bool m_CheckDisabled   = false;   
    bool m_CheckProtected  = false;
    RC PickUnprotectedGroups
    (
        const FsMask& InGroupMask,
        FsMask *pOutGroupMask,
        bool checkElement
    ) const;

    RC PickMostAvailableGroups
    (
        const FsMask& InGroupMask,
        FsMask *pOutGroupMask
    ) const;

    RC CountAvailableElements
    (
        UINT32 grpIdx,
        UINT32 *pAvailableCount
    ) const;
};

//!
//! \brief Pick the FBP based on some HBM connectivity relations and prefered order for
//!        disablement of various sites.
//!
class GH100FBPGroupPicker: public DownbinGroupPicker
{
    using FsUnit = Downbin::FsUnit;

public:
    GH100FBPGroupPicker
    (
        TestDevice *pDev,
        FsSet *pFbpSet,
        const FuseUtil::DownbinConfig &fbpConfig
    );

    string GetPickerName() const override;

    RC Pick
    (
        const FsMask& InGroupMask,
        FsMask *pOutGroupMask
    ) const override;

private:
    TestDevice *m_pDev = nullptr;
    FsSet *m_pFbpSet = nullptr;
    FuseUtil::DownbinConfig m_FbpConfig;
    INT32 m_TotalFbpDisableCount = -1;
    
    static constexpr UINT32 s_NumFbpPerHbmSite = 2;
    RC PickHBMPairFbp(const FsMask& InGroupMask, FsMask *pOutGroupMask) const;
    RC GeneratePreferredOrder(vector<UINT32> *pDesiredIndices, UINT32 numFbpDisabled) const;
};

//!
//! \brief When disabling a TPC, need to choose a GPC to disbale its TPC and not break
//! the vGpc skyline restriction.
//!
class VGpcSkylineGroupPicker: public DownbinGroupPicker
{
    using FsUnit = Downbin::FsUnit;

public:
    VGpcSkylineGroupPicker
    (
        FsSet *pGpcSet,
        const FuseUtil::DownbinConfig &tpcConfig
    );

    string GetPickerName() const override;

    RC Pick
    (
        const FsMask& InGroupMask,
        FsMask *pOutGroupMask
    ) const override;

private:
    bool IsWithilwgpcSkyline
    (
        vector<UINT32> &numsEnabledTpcPerGpc,
        const vector<UINT32> &padVGpcSkylineList
    ) const;

    FsSet *m_pGpcSet = nullptr;
    vector<UINT32> m_vGpcSkylineList;
    UINT32 m_MinEnableTpcPerGpc;
};

//!
//! \brief When disable one group in a FsSet, look into another FsSet and don't pick the group
//! with the same GroupId.
//! e.g.: We have LWDEC FsSet and LWJPG FsSet. If lwjpg0 is defective, 
//! and you need to disable one lwdec, don’t pick lwdec0
//!
class CrossFsSetDependencyGroupPicker: public DownbinGroupPicker
{
    using FsUnit = Downbin::FsUnit;

public:
    CrossFsSetDependencyGroupPicker
    (
        FsSet *pLwrFsSet,
        FsSet *pRefFsSet
    );

    string GetPickerName() const override;

    RC Pick
    (
        const FsMask& InGroupMask,
        FsMask *pOutGroupMask
    ) const override;

private:
    FsSet *m_pLwrFsSet;
    FsSet *m_pRefFsSet;
};
