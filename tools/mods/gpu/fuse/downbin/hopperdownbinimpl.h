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

#include "downbinimpl.h"

//!
//! \brief  GH100 downbinning class
//!
class GH100DownbinImpl : public DownbinImpl
{
    using FsUnit = Downbin::FsUnit;

public:
    explicit GH100DownbinImpl(TestDevice *pDev);
    virtual ~GH100DownbinImpl() = default;

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
    //! \brief Add pickers to specify UGPU level constraints to Fs set
    //!
    //! \param pFsSet  The fs set to which ugpu group disable pickers would be added
    //! \param fsUnit  Can be group or element fs unit in the pFsSet
    //! \param unitConfig  DownbinConfig for the fsUnit
    //!
    RC AddUgpuGDPickers
    (
        FsSet *pFsSet,
        FsUnit fsUnit,
        const FuseUtil::DownbinConfig &unitConfig
    );
    //!
    //! \brief Add pickers to specify UGPU level constraints to Fs set
    //!
    //! \param pFsSet  The fs set to which ugpu pickers group reduce would be added
    //! \param fsUnit  Can be group or element fs unit in the pFsSet
    //! \param unitConfig  DownbinConfig for the fsUnit
    //!
    RC AddUgpuGRPickers
    (
        FsSet *pFsSet,
        FsUnit fsUnit,
        const FuseUtil::DownbinConfig &unitConfig
    );
    //!
    //! \brief Add group disable pickers for GPC set
    //!
    //! \param fuseConfig  map of downbin config for various fsUnits as per current SKU
    //!
    RC AddGpcSetGroupDisablePickers(const string &skuName) override;
    //! \brief Add group reduce pickers for GPC set
    //!
    //! \param skuName        Name of SKU being downbinned to
    //!
    RC AddGpcSetGroupReducePickers(const string &skuName) override;
    //! \brief Add element disable pickers for GPC set
    //!
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
    //! \brief downbin Syspipe Set
    //! \param syspipeConfig DownbinConfig for Syspipe Set
    //!
    RC DownbinSyspipeSet(const FuseUtil::DownbinConfig &syspipeConfig) override;
private:
    //!
    //! \brief Add GH100 specific L2 NOC constraints for downbinning
    //!
    RC AddL2NocRules();

    //!
    //! \brief Add GH100 specific L2 slice floorsweeping rules
    //!
    RC AddL2SliceFsRules();

    //!
    //! \brief Add GH100 specific GPC set rules for downbinning
    //!
    RC AddGpcSetRules();

    //!
    //! \brief Add GH100 specific Syspipe set rules for downbinning
    //!
    RC AddSyspipeRules();

    //!
    //! \brief Add picker to return a preferred list of FBP
    //!
    RC AddPreferredFbpGDPicker(const FuseUtil::DownbinConfig &fbpConfig);

    //!
    //! \brief Add picker to pick non-graphics TPCs first
    //!
    RC AddGraphicsTpcEDPicker();

    //!
    //! \brief Add picker to choose TPCs corresponding to CPCs with least number of
    //!        disabled TPCs
    //!
    RC AddCpcTpcEDPicker();

    //!
    //! \brief Add picker to pick the GPC in order not to break
    //! vgpc Skyline restriction
    //!
    RC AddVGpcSkylineGRPicker(FsSet *pFsSet, const FuseUtil::DownbinConfig &tpcConfig);

    //!
    //! \brief Add GH100 specific LWDEC and LWJPG pickers
    //!
    RC AddLwdecLwjpgDownbinPickers();

    //!
    //! \brief Add GH100 specific Syspipe pickers
    //!
    RC AddSyspipeDownbinPickers();

    // Pointer to the test device object
    TestDevice *m_pDev = nullptr;
};
