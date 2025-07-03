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

#include "core/utility/strong_typedef_helper.h"
#include "gpu/repair/hbm/hbm_interface/hbm2_interface.h"

namespace Memory
{
namespace Hbm
{
    //!
    //! \brief SK Hynix HBM2 model.
    //!
    //! Works for the following HBM2 models:
    //! - Mask3
    //! - Mask4
    //!
    //! Implements the sequence for SK Hynix Mask 3/4 described here:
    //! https://confluence.lwpu.com/x/r6SsB
    //!
    class SkHynixHbm2 : public Hbm2Interface
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
        SkHynixHbm2
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

        //!
        //! NOTE: HBM2 SK Hynix Mask3/4 does not show soft row repairs.
        //!
        RC PrintRepairedRows() const override;

        //!
        //! \brief A fuse number of a row repair fuse used to populate the
        //! 'Fuse set' field of the row repair WDR data.
        //!
        //! \see SoftRepairWriteData Contains 'fuse set' field.
        //! \see HardRepairData Contains 'fuse set' field.
        //!
        struct RowFuse : public NumericIdentifier
        {
            using NumericIdentifier::NumericIdentifier;
        };

        //!
        //! \brief Burned row fuse mask from the HBM.
        //!
        //! Represents the row fuses that have been blown on a single channel to
        //! perform repairs.
        //!
        struct RowBurnedFuseMask
            : type_safe::strong_typedef<RowBurnedFuseMask, UINT08>,
              type_safe::strong_typedef_op::bitmask<RowBurnedFuseMask>
        {
            using strong_typedef::strong_typedef;

            //!
            //! \brief Get the number of the first available fuse.
            //!
            //! \param[out] pRowFuse Available fuse number.
            //!
            //! \return Non-OK when no fuse is available.
            //!
            RC GetFirstAvailableFuse(RowFuse* pRowFuse);

            //!
            //! \brief Get the burned fuse mask.
            //!
            //! A set bit represents a burned fuse. Bit position is the fuse
            //! number.
            //!
            UINT08 Mask() const { return static_cast<UINT08>(*this); }

            //!
            //! \brief Return true if the given fuse is burned, false otherwise.
            //!
            bool IsFuseBurned(RowFuse rowFuse) const;
        };

    protected:
        //!
        //! \brief Perform a soft row repair.
        //!
        //! \param settings Repair settings.
        //! \param rowErrors Rows to repair.
        //!
        virtual RC SoftRowRepair
        (
            const Settings& settings,
            const SiteRowErrorMap& rowErrors
        ) const;

        //!
        //! \brief Perform a hard row repair.
        //!
        //! \param setting Repair settings.
        //! \param rowErrors Rows to repair.
        //!
        virtual RC HardRowRepair
        (
            const Settings& settings,
            const SiteRowErrorMap& rowErrors
        ) const;

        //!
        //! \brief Prints repair row information on the given channel
        //!
        //! \param site HBM site the channel belongs to
        //! \param stack Stack ID of the channel
        //! \param channel Channel information
        //! \param[out] pNumBurnedFusesOnChannel Number of fuses burnt on given channel
        //!
        virtual RC PrintChannelRepairedRows
        (
            Site site,
            Stack stack,
            Channel channel,
            UINT32 *pNumBurnedFusesOnChannel
        ) const;

    private:
        //! Number of spare row per bank pair
        static constexpr UINT32 s_NumSpareRowsPerBankPair = 4;
        //! Maximum number of repairs per channel
        static constexpr UINT32 s_MaxRowRepairsPerChannel = 16;
        //! Number of bank pairs
        static constexpr UINT32 s_NumBankPairs            = 8;

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
        //! \brief A pair of connected banks.
        //!
        //! See the HBM2 specification document regarding "bank pairs".
        //!
        struct BankPair
        {
            Bank bank0; //!< First bank
            Bank bank1; //!< Second bank

            string ToString() const;

            static UINT32 ToBankPairIndex(Bank bank);
            static BankPair FromBankPairIndex(UINT32 bankPairIndex);
            static constexpr UINT32 NumBankPairs() { return s_NumBanksPerChannel / 2; }
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
            //! \brief Return the bank pair index of the given bank.
            //!
            //! \param bank Bank.
            //!
            static UINT32 PprInfoBankPairIndex(Bank bank);

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
        //! \brief Remove any row from the given sorted list that is a neighbour
        //! of another row in the list.
        //!
        //! NOTE: SK Hynix mask 3/4 repairs neighbouring rows when a single row
        //! needs to be repaired. It would waste a fuse to repair both neighbours.
        //!
        //! \param[in,out] pRowErrors Collection of sorted rows.
        //!
        void FilterOutNeighbouringRowsFromSorted(vector<RowErrorInfo>* pRowErrors) const;

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
