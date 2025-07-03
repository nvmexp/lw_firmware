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
//! \brief Choose sub-elements corresponding to the elements with the most number
//!        of enabled sub-elements
//!        Helps in balancing sub-elements across active elements
//!        Eg. Choose TPCs corresponding to PESs with least number of disabled TPCs
//!
class MostEnabledSubElementPicker : public DownbinElementPicker
{
    using FsUnit = Downbin::FsUnit;

public:
    MostEnabledSubElementPicker
    (
        FsSet *pFsSet,
        FsUnit elementFsUnit,
        FsUnit subElementFsUnit
    );

    string GetPickerName() const override;

    RC Pick
    (
        UINT32 grpIdx,
        const FsMask& InElementMask,
        FsMask *pOutElementMask
    ) const override;

private:
    FsSet *m_pFsSet           = nullptr;
    FsUnit m_ElementFsUnit    = FsUnit::NONE;
    FsUnit m_SubElementFsUnit = FsUnit::NONE;

    vector<UINT32> m_SubElementDistribution;
};

//!
//! \brief Choose the non-protected elements from a group 
//!
class AvailableElementPicker : public DownbinElementPicker
{
    using FsUnit = Downbin::FsUnit;

public:
    AvailableElementPicker
    (
        FsSet *pFsSet,
        FsUnit elementFsUnit
    );

    string GetPickerName() const override;

    RC Pick
    (
        UINT32 grpIdx,
        const FsMask& InElementMask,
        FsMask *pOutElementMask
    ) const override;

private:
    FsSet *m_pFsSet           = nullptr;
    FsUnit m_ElementFsUnit    = FsUnit::NONE;
};

//! \brief Choose the element which corresponds to most number of disabled
//!        dependent elements and without protected dependent elements
//!        Useful in picking LTC with most disabled L2 Slices and without any
//!        protected l2 slices
class MostDisabledDependentElementPicker : public DownbinElementPicker
{
    using FsUnit = Downbin::FsUnit;

public:
    MostDisabledDependentElementPicker
    (
        FsSet *pFsSet,
        FsUnit elementFsUnit,
        FsUnit depElementFsUnit
    );

    string GetPickerName() const override;

    RC Pick
    (
        UINT32 grpIdx,
        const FsMask& InElementMask,
        FsMask *pOutElementMask
    ) const override;

private:
    FsSet *m_pFsSet           = nullptr;
    FsUnit m_ElementFsUnit    = FsUnit::NONE;
    FsUnit m_DepElementFsUnit = FsUnit::NONE;
};

//!
//! \brief Choose the preferred elements from a group 
//!
class PreferredElementPicker : public DownbinElementPicker
{
    using FsUnit = Downbin::FsUnit;

public:
    PreferredElementPicker
    (
        FsSet *pFsSet,
        FsUnit elementFsUnit,
        vector<FsMask>& preferredElementMasks
    );

    string GetPickerName() const override;

    RC Pick
    (
        UINT32 grpIdx,
        const FsMask& InElementMask,
        FsMask *pOutElementMask
    ) const override;

private:
    FsSet *m_pFsSet           = nullptr;
    FsUnit m_ElementFsUnit    = FsUnit::NONE;
    vector<FsMask> m_PreferredElementMasks;
};