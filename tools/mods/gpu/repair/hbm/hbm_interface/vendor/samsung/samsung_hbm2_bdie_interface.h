/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "gpu/repair/hbm/hbm_interface/hbm2_interface.h"

namespace Memory
{
namespace Hbm
{
    class SamsungHbm2BDie : public Hbm2Interface
    {
        using HbmChannel      = Hbm::Channel;
        using RowError        = Repair::RowError;
        using Settings        = Repair::Settings;
        using SiteRowErrorMap = map<Site, vector<Hbm::RowErrorInfo>>;

    public:
        SamsungHbm2BDie
        (
            const Model& hbmModel,
            std::unique_ptr<GpuHbmInterface> pGpuHbmInterface
        ) : Hbm2Interface(hbmModel, std::move(pGpuHbmInterface)) {}

        RC Setup() override;

        RC RowRepair
        (
            const Settings& settings,
            const SiteRowErrorMap& rowErrors,
            RepairType repairType
        ) const override;

        RC PrintRepairedRows() const override;

    private:
        static constexpr UINT32 s_NumBanksPerChannel       = 16;
        static constexpr UINT32 s_NumBanksPerPseudoChannel = 8;
        static constexpr UINT32 s_NumSpareRowsPerBank      = 2;
        static constexpr UINT32 s_MaxRowRepairsPerChannel  = 16;
        static constexpr UINT32 s_NumPseudoChannels        = 2;

        //!
        //! \brief ENABLE_FUSE_SCAN WDR data for reading and writing.
        //!
        class EnableFuseScanData : public WdrData
        {
        public:
            EnableFuseScanData() {}
            EnableFuseScanData(const Stack& stack, UINT32 fuseScanMask);

            //!
            //! \brief True if there is a spare row available. Only meaningful
            //! if the ENABLE_FUSE_SCAN data was read from the WDR.
            //!
            bool IsRowAvailable() const;
        };

        //!
        //! \brief Spare row fuse scan mask lookup table for ENABLE_FUSE_SCAN.
        //!
        class EnableFuseScanLookupTable
        {
        public:
            //!
            //! \brief Get ENABLE_FUSE_SCAN WDR associated with the given row.
            //!
            //! \param stack Stack ID.
            //! \param hbmPseudoChan HBM pseudo channel.
            //! \param bank Bank.
            //! \param row Row.
            //! \return ENABLE_FUSE_SCAN WDR data.
            //!
            static EnableFuseScanData GetWdrData
            (
                const Stack& stack,
                const PseudoChannel& hbmPseudoChan,
                const Bank& bank,
                UINT32 row
            );

        private:
            //! Number of columns in the fuse scan table \a s_EnableFuseScanRegMask.
            static constexpr UINT32 s_NumFuseScanTableCols = 23;

            //! Lookup table for the spare fuse address of a bank. Used in
            //! ENABLE_FUSE_SCAN.
            static const bool s_EnableFuseScanRegMask[64][s_NumFuseScanTableCols];
        };

        class RowRepairWriteData : public WdrData
        {
        public:
            //!
            //! \brief Constructor.
            //!
            //! \param stack Stack ID.
            //! \param hbmPseudoChan HBM pseudo channel.
            //! \param bank Bank number.
            //! \param row Row number.
            //!
            RowRepairWriteData
            (
                Stack stack,
                PseudoChannel hbmPseudoChan,
                Bank bank,
                UINT32 row
            );
        };

        //!
        //! \brief Get the number of spare rows in a given bank.
        //!
        //! \param hbmSite HBM site.
        //! \param hbmChannel HBM channel.
        //! \param stack Stack ID.
        //! \param bank Bank.
        //! \param[out] pNumRowsAvailable Number of available spare rows.
        //! \param printResults Print detailed search results.
        //!
        RC GetNumSpareRowsAvailable
        (
            const Site& hbmSite,
            const HbmChannel& hbmChannel,
            const Stack& stack,
            const Bank& bank,
            UINT32* pNumRowsAvailable,
            bool printResults = false
        ) const;

        //!
        //! \brief Perform a hard row repair.
        //!
        //! \param setting Repair settings.
        //! \param rowErrors Rows to repair.
        //!
        RC HardRowRepair
        (
            const Settings& settings,
            const SiteRowErrorMap& rowErrors
        ) const;
    };
} // namespace Hbm
} // namespace Memory
