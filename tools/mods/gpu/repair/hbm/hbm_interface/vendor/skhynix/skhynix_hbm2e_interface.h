/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "gpu/repair/hbm/hbm_interface/vendor/skhynix/skhynix_hbm_interface.h"

namespace Memory
{
namespace Hbm
{
    //!
    //! \brief SK Hynix HBM2e model.
    //!
    //! Implements the sequence for SK Hynix HBM2e described here:
    //! https://confluence.lwpu.com/x/r6SsB
    //!
    class SkHynixHbm2e : public SkHynixHbm2
    {
        using HbmChannel      = Hbm::Channel;
        using Settings        = Repair::Settings;
        using RowError        = Repair::RowError;
        using SiteRowErrorMap = map<Site, vector<Hbm::RowErrorInfo>>;

    public:
        //!
        //! \brief Constructor.
        //!
        //! \param hbmModel The HBM model this interface represents.
        //! \param pGpuHbmInterface The GPU-to-HBM interface that is used to
        //! communicate with the HBM.
        //!
        SkHynixHbm2e
        (
            const Model& hbmModel,
            std::unique_ptr<GpuHbmInterface> pGpuHbmInterface
        ) : SkHynixHbm2(hbmModel, std::move(pGpuHbmInterface)) {}

        RC Setup() override;

    protected:
        //!
        //! \brief Perform a soft row repair.
        //!
        //! \param settings Repair settings.
        //! \param rowErrors Rows to repair.
        //!
        RC SoftRowRepair
        (
            const Settings& settings,
            const SiteRowErrorMap& rowErrors
        ) const override;

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
        ) const override;

        //!
        //! \brief Prints repaired rows information on the given channel
        //!
        //! \param site HBM site the channel belongs to
        //! \param stack Stack ID of the channel
        //! \param channel Channel information
        //! \param[out] pNumBurnedFusesOnChannel Number of fuses burnt on given channel
        //!
        RC PrintChannelRepairedRows
        (
            Site site,
            Stack stack,
            Channel channel,
            UINT32 *pNumBurnedFusesOnChannel
        ) const override;

    private:
        //! Number of spare rows per bank
        static constexpr UINT32 s_NumSpareRowsPerBank     = 4;

        //! Maximum number of repairs per channel
        //! NOTE -
        //!   This is assuming 4 per channel x 16 channels
        //!   Manuals don't specify a hard maximum per channel
        static constexpr UINT32 s_MaxRowRepairsPerChannel = 64;

        //!
        //! \brief Soft repair WDR data. Can be written.
        //!
        class SoftRepairWriteData : public WdrData
        {
        public:
            //!
            //! \brief Values for the 'Enable' field.
            //!
            //! NOTE: Repair must be enabled for a soft repair to work!
            //!
            enum EnableRepair : UINT08
            {
                ENABLE_REPAIR, //!< Repair is enabled
                DISABLE_REPAIR //!< Repair is disabled
            };

            SoftRepairWriteData() {}

            //!
            //! \brief Constructor.
            //!
            //! \param rowFuse
            //! \param stack Stack ID to repair.
            //! \param bank Bank to repair.
            //! \param row Row to repair.
            //! \param enableRepair State of the enable bit.
            //!
            SoftRepairWriteData
            (
                RowFuse rowFuse,
                Stack stack,
                Bank bank,
                UINT32 row,
                EnableRepair enableRepair
            );

            //!
            //! \brief Set the 'Start bit' field.
            //!
            //! Setting indicates the start of a row repair command.
            //!
            void SetStartBit();

            //!
            //! \brief Unset the 'Start bit' field.
            //!
            //! Unsetting indicates the end of a row repair command.
            //!
            void UnsetStartBit();
        };

        //!
        //! \brief Hard repair WDR data. Can be read and written.
        //!
        class HardRepairData : public WdrData
        {
        public:
            //!
            //! \brief Values for the 'Enable' field.
            //!
            //! NOTE: Repair must be enabled for a soft repair to work!
            //!
            enum EnableRepair : UINT08
            {
                ENABLE_REPAIR, //!< Repair is enabled
                DISABLE_REPAIR //!< Repair is disabled
            };

            HardRepairData() {}

            //!
            //! \brief Constructor.
            //!
            //! \param rowFuse
            //! \param stack Stack ID to repair.
            //! \param bank Bank to repair.
            //! \param row Row to repair.
            //! \param enableRepair State of the enable bit.
            //!
            HardRepairData
            (
                RowFuse rowFuse,
                Stack stack,
                Bank bank,
                UINT32 row,
                bool enableRepair
            );

            //!
            //! \brief Set the 'Start bit' field.
            //!
            //! Setting indicates the start of a row repair command.
            //!
            void SetStartBit();

            //!
            //! \brief Unset the 'Start bit' field.
            //!
            //! Unsetting indicates the end of a row repair command.
            //!
            void UnsetStartBit();

            //!
            //! \brief Return true if the repair succeeded, false otherwise.
            //!
            //! Checks the success bit. The result of this call is meaningless
            //! unless the WDR data is the read result of the repair.
            //!
            bool RepairSucceeded() const;
        };

        //!
        //! \brief Data corresponding the PPR_INFO WIR.
        //!
        //! Contains the HBM row fuse data for a single HBM channel.
        //!
        //! NOTE: PPR_INFO only retrieves hard repairs. Soft repairs will not be
        //! present.
        //!
        class PprInfoReadData : public WdrData
        {
        public:
            //!
            //! \brief Get the number of the first available fuse.
            //!
            //! \param stack Stack ID.
            //! \param bank Bank.
            //! \param[out] pRowFuse Available fuse number.
            //!
            RC GetFirstAvailableFuse
            (
                Stack stack,
                Bank bank,
                RowFuse* pRowFuse
            ) const;

            //!
            //! \brief Get the row burned fuse mask for the given stack and bank.
            //!
            //! \param stack Stack ID.
            //! \param bank Bank.
            //!
            RowBurnedFuseMask GetRowBurnedFuseMask(Stack stack, Bank bank) const;

            //!
            //! \brief Return true if the given fuse is burned, false otherwise.
            //!
            //! \param stack Stack ID of the fuse.
            //! \param bank Bank of the fuse.
            //! \param rowFuse Row fuse number.
            //!
            bool IsFuseBurned(Stack stack, Bank bank, RowFuse rowFuse) const;

            //!
            //! \brief Return the total number of burned fuses on the channel
            //! this PPR_INFO came from.
            //!
            UINT32 TotalBurnedFuses() const;
        };

        //!
        //! \brief Get a spare row fuse for the given row.
        //!
        //! \param hbmSite HBM Site.
        //! \param hbmChannel HBM channel.
        //! \param row Row that needs replacing.
        //! \param[out] pSpareRowFuse Spare row fuse number for the given
        //! \return Non-OK if unable to get a spare row.
        //!
        RC GetSpareRowWithRepairCheck
        (
            Site hbmSite,
            Channel hbmChannel,
            const Row& row,
            RowFuse* pSpareRowFuse
        ) const;
    };
} // namespace Hbm
} // namespace Memory
