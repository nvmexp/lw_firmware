/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "gpu/repair/hbm/hbm_interface/vendor/samsung/samsung_hbm2_xdie_interface.h"

#include <algorithm>

namespace Memory
{
namespace Hbm
{
    //!
    //! \brief Samsung HBM2 X-die interface for GV100.
    //!
    //! Should only be used on GV100.
    //!
    //! In theory there should only be one sequence needed for an HBM model,
    //! regardless of the GPU it is mounted to. In reality, GPU settings impact
    //! the HBM and, in some cases, require different sequences to accomplish
    //! the same thing. X-die on Volta versus Ampere is one of those cases.
    //!
    class SamsungHbm2XDieGV100 : public SamsungHbm2XDie
    {
        using HbmChannel      = Hbm::Channel;
        using RowError        = Repair::RowError;
        using Settings        = Repair::Settings;
        using SiteRowErrorMap = map<Site, vector<Hbm::RowErrorInfo>>;

    public:
        SamsungHbm2XDieGV100
        (
            const Model& hbmModel,
            std::unique_ptr<GpuHbmInterface> pGpuHbmInterface
        ) : SamsungHbm2XDie(hbmModel, std::move(pGpuHbmInterface)) {}

        RC RowRepair
        (
            const Settings& settings,
            const SiteRowErrorMap& rowErrors,
            RepairType repairType
        ) const override;

        RC PrintRepairedRows() const override;

    private:
        //!
        //! \brief WAR to x-die implementation hardware bug that affect the
        //! ENABLE_FUSE_SCAN WIR.
        //!
        //! NOTE: Only needs to be done once after each site reset.
        //!
        //! \param hbmSite HBM site.
        //!
        RC DoEnableFuseScanWar(const Site& hbmSite) const;

        //!
        //! \brief Perform an ENABLE_FUSE_SCAN on the given spare and gather the result.
        //!
        //! \param hbmSite HBM site.
        //! \param hbmChannel HBM channel.
        //! \param stack Stack ID.
        //! \param pseudoChannel Pseudo channel.
        //! \param bank Bank number.
        //! \param pseudoChannelSpareRow Spare row on the pseudo channel.
        //! \param[out] pResult Scan result.
        //!
        RC DoEnableFuseScan
        (
            const Site& hbmSite,
            const HbmChannel& hbmChannel,
            const Stack& stack,
            const PseudoChannel& pseudoChannel,
            const Bank& bank,
            UINT32 pseudoChannelSpareRow,
            EnableFuseScanData* pResult
        ) const;

        //!
        //! \brief Perform a hard row repair.
        //!
        //! \param setting Repair settings.
        //! \param rows Rows to repair.
        //!
        RC HardRowRepair
        (
            const Settings& settings,
            const SiteRowErrorMap& rowErrors
        ) const;

        //!
        //! \brief Perform a soft row repair.
        //!
        //! \param setting Repair settings.
        //! \param rows Rows to repair.
        //!
        RC SoftRowRepair
        (
            const Settings& settings,
            const SiteRowErrorMap& rowErrors
        ) const;

        //!
        //! \brief Get the number of spare rows in a given bank.
        //!
        RC GetNumSpareRowsAvailable
        (
            const Site& hbmSite,
            const HbmChannel& hbmChannel,
            const Stack& stack,
            const Bank& bank,
            UINT32* pNumRowsAvailable
        ) const;
    };

} // namespace Hbm
} // namespace Memory
