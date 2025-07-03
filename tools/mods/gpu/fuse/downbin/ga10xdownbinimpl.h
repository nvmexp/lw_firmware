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

#include "downbinimpl.h"

//!
//! \brief  GA10x downbinning class
//!
class GA10xDownbinImpl : public DownbinImpl
{
    using FsUnit = Downbin::FsUnit;

public:
    explicit GA10xDownbinImpl(TestDevice *pDev);
    virtual ~GA10xDownbinImpl() = default;

protected:
    //!
    //! \brief  Add chips-specific floorsweeping rules to FS sets
    //!
    RC AddChipFsRules() override;

    //!
    //! \brief  Apply chip / SKU rules post downbinning
    //!
    //! \param skuName   Name of SKU being downbinned to
    //!
    RC ApplyPostDownbinRules(const string &skuName) override;

    //!
    //! \brief Add Element disable pickers corresponding to
    //!        Pes
    virtual RC AddPesEDPickers();
    //!
    //! \brief Add group disable pickers for GPC set
    //! \param skuName        Name of SKU being downbinned to
    //!
    RC AddGpcSetGroupDisablePickers(const string &skuName) override;
    //!
    //! \brief Add group reduce pickers for GPC set
    //! \param skuName        Name of SKU being downbinned to
    //!
    RC AddGpcSetGroupReducePickers(const string &skuName) override;
    //!
    //! \brief Add group disable pickers for GPC set
    //! \param skuName        Name of SKU being downbinned to
    //!
    RC AddGpcSetElementDisablePickers(const string &skuName) override;

    //!
    //! \brief Add group disable pickers for FBP set
    //! \param skuName        Name of SKU being downbinned to
    //!
    RC AddFbpSetGroupDisablePickers(const string &skuName) override;
    //!
    //! \brief Add group reduce pickers for FBP set
    //! \param skuName        Name of SKU being downbinned to
    //!
    RC AddFbpSetGroupReducePickers(const string &skuName) override;
    //!
    //! \brief Add element disable pickers for FBP set
    //! \param skuName        Name of SKU being downbinned to
    //!
    RC AddFbpSetElementDisablePickers(const string &skuName) override;

    //!
    //! \brief Add Misc DownbinPickers
    //!
    RC AddMiscDownbinPickers() override;

    //!
    //! \brief Try and apply a 3-slice pattern for L2 slices
    //!
    //! \param[in/out] pFbpSet    FBP FS set
    //! \param maxFbps            Number of FBPs
    //! \param maxLtcsPerFbp      Number of LTCs per FBP
    //! \param maxSlicesPerLtc    Number of L2 slices per LTC
    //! \param[out] pMasksApplied Was 3 slice mode applied or not
    //!
    RC TryApply3SliceMode
    (
        FsSet *pFbpSet,
        UINT32 maxFbps,
        UINT32 maxLtcsPerFbp,
        UINT32 maxSlicesPerLtc,
        bool *pMasksApplied
    );

    //!
    //! \brief Try and apply a 2-slice pattern for L2 slices
    //!
    //! \param[in/out] pFbpSet    FBP FS set
    //! \param maxFbps            Number of FBPs
    //! \param maxLtcsPerFbp      Number of LTCs per FBP
    //! \param maxSlicesPerLtc    Number of L2 slices per LTC
    //! \param[out] pMasksApplied Was 3 slice mode applied or not
    //!
    RC TryApply2SliceMode
    (
        FsSet *pFbpSet,
        UINT32 maxFbps,
        UINT32 maxLtcsPerFbp,
        UINT32 maxSlicesPerLtc,
        bool *pMasksApplied
    );

private:
    //!
    //! \brief  Get the array of L2 slice disable masks for the given fbp set
    //!
    //! \param fbpSet           FBP FS set
    //! \param maxFbps          Number of FBPs
    //! \param maxLtcsPerFbp    Number of LTCs per FBP
    //! \param maxSlicesPerLtc  Number of L2 slices per LTC
    //! \param[out] pL2SliceMasks The per-LTC L2 slice masks for the given set indexed by FBP
    //!                           (expected to be sized correctly)
    //!
    void GetL2SliceMasks
    (
        const FsSet& fbpSet,
        UINT32 maxFbps,
        UINT32 maxLtcsPerFbp,
        UINT32 maxSlicesPerLtc,
        vector<vector<UINT32>> *pL2SliceMasks
    );

    //!
    //! \brief Get the minimum number of slices on an enabled LTC
    //!
    //! \param fbpSet           FBP FS set
    //! \param maxFbps          Number of FBPs
    //! \param maxLtcsPerFbp    Number of LTCs per FBP
    //! \param maxSlicesPerLtc  Number of L2 slices per LTC
    //!
    UINT32 GetMinEnabledSliceCount
    (
        const FsSet& fbpSet,
        UINT32 maxFbps,
        UINT32 maxLtcsPerFbp,
        UINT32 maxSlicesPerLtc
    );

    //!
    //! \brief Check if the given L2 slice masks can be applied, and apply it
    //!        to the given fbp set is so
    //!
    //! \param[in/out] pFbpSet    FBP FS set
    //! \param maxSlicesPerLtc    Number of L2 slices per LTC
    //! \param oldL2SliceMasks    Old per-LTC L2 slice masks ordered by FBP
    //! \param newL2SliceMasks    New per-LTC L2 slice masks ordered by FBP
    //! \param[out] pMasksApplied Were the L2 slice masks applied or not
    //!
    RC TryApplyL2SliceMasks
    (
        FsSet *pFbpSet,
        UINT32 maxSlicesPerLtc,
        const vector<vector<UINT32>> &oldL2SliceMasks,
        const vector<vector<UINT32>> &newL2SliceMasks,
        bool *pMasksApplied
    );

    //!
    //! \brief Get the L2 slice around which the 2/3 slice modes need to be applied
    //!
    //! \param l2SliceMasks       Per-LTC L2 slice masks ordered by FBP
    //! \param maxSlicesPerLtc    Number of L2 slices per LTC
    //! \param[out] pPivotFbp     FBP idx for the pivot l2 slice
    //! \param[out] pPivotLtc     LTC idx for the pivot l2 slice
    //! \param[out] pPivotSlice   L2 slice idx for the pivot l2 slice
    //!
    RC GetPivotL2Slice
    (
        const vector<vector<UINT32>> &l2SliceMasks,
        UINT32 maxSlicesPerLtc,
        UINT32 *pPivotFbp,
        UINT32 *pPivotLtc,
        UINT32 *pPivotSlice
    ) const;

private:
    static constexpr UINT32 s_IlwalidIndex = UINT32_MAX;
};
