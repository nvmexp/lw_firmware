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

#include "gpu/repair/hbm/hbm_interface/hbm2_interface.h"

#include <algorithm>

namespace Memory
{
namespace Hbm
{
    //!
    //! \brief Samsung HBM2e base implementation.
    //!
    //! Contains Samsung HBM2e functionality that is consistent across all GPUs.
    //!
    //!
    class SamsungHbm2e : public Hbm2Interface
    {
        using HbmChannel      = Hbm::Channel;
        using RowError        = Repair::RowError;
        using Settings        = Repair::Settings;
        using SiteRowErrorMap = map<Site, vector<Hbm::RowErrorInfo>>;

    public:
        //!
        //! \brief ENABLE_FUSE_SCAN WDR data for reading and writing.
        //!
        class EnableFuseScanData : public WdrData
        {
        public:
            EnableFuseScanData() {}

            //!
            //! \brief Fuse scan mask value.
            //!
            //! Meant to be propulated from the WDR result of ENABLE_FUSE_SCAN.
            //!
            EnableFuseScanData(UINT32 fuseScanMaskValue);

            //!
            //! \brief True if there is a spare row available. Only meaningful
            //! if the ENABLE_FUSE_SCAN data was read from the WDR.
            //!
            bool IsRowAvailable() const;

            //!
            //! \brief Print object to stream.
            //!
            //! NOTE: This is the GTest override that will be used for printing
            //! this type in GTest runs.
            //!
            friend void PrintTo(const EnableFuseScanData& data, ostream* os)
            {
                MASSERT(data.size() == 1);
                string s;

                for (UINT32 i = 0; i < sizeof(data[0]) * CHAR_BIT; ++i)
                {
                    s += ((data[0] & (1U << i)) ? '1' : '0');
                }

                reverse(s.begin(), s.end());
                s = "0b" + s;

                *os << s;
            }

            friend bool operator==(const EnableFuseScanData& lhs, const EnableFuseScanData& rhs)
            {
                if (lhs.size() != rhs.size()) { return false; }

                for (UINT32 i = 0; i < lhs.size(); ++i)
                {
                    if (lhs[i] != rhs[i]) { return false; }
                }

                return true;
            }
        };

        //!
        //! \brief Spare row fuse scan mask lookup table for ENABLE_FUSE_SCAN.
        //!
        class EnableFuseScanLookupTable
        {
        public:
            //!
            //! \brief Get the ENABE_FUSE_SCAN WDR write data that corresponds
            //! to the given spare pseudo channel row.
            //!
            //! \param stack Stack ID.
            //! \param HBM channel.
            //! \param hbmPseudoChan HBM pseudo channel.
            //! \param bank Bank.
            //! \param pseudoChannelSpareRow Pseudo channel spare row number.
            //!
            static EnableFuseScanData GetWdrData
            (
                const Stack& stack,
                const Channel& hbmChannel,
                const PseudoChannel& hbmPseudoChan,
                const Bank& bank,
                UINT32 pseudoChannelSpareRow
            );

        private:
            //! Number of entries in the fuse scan table.
            static constexpr UINT32 s_NumLookupTableRows = 256;
            //! Fuse scan table.
            static const array<UINT32, s_NumLookupTableRows> s_FuseMaskLookupTable;
        };

        class RowRepairWriteData : public WdrData
        {
        public:
            RowRepairWriteData() {}

            //!
            //! \brief Constructor.
            //!
            //! \param repairType Type of repair.
            //! \param stack Stack ID.
            //! \param hbmPseudoChan HBM pseudo channel.
            //! \param bank Bank number.
            //! \param row Row number.
            //!
            RowRepairWriteData
            (
                RepairType repairType,
                Stack stack,
                PseudoChannel hbmPseudoChan,
                Bank bank,
                UINT32 row
            );
        };

        SamsungHbm2e
        (
            const Model& hbmModel,
            std::unique_ptr<GpuHbmInterface> pGpuHbmInterface
        ) : Hbm2Interface(hbmModel, std::move(pGpuHbmInterface)) {}

        RC Setup() override;

    protected:
        static constexpr UINT32 s_NumBanksPerPseudoChannel = 16;
        static constexpr UINT32 s_NumPseudoChannels = 2;
        static constexpr UINT32 s_NumBanksPerChannel = (s_NumPseudoChannels *
                                                     s_NumBanksPerPseudoChannel);
        static constexpr UINT32 s_NumSpareRowsPerBank = 4;
        static constexpr UINT32 s_NumSpareRowsPerPseudoChannel = (s_NumSpareRowsPerBank *
                                                         s_NumBanksPerPseudoChannel);
        static constexpr UINT32 s_MaxRowRepairsPerChannel = (s_NumSpareRowsPerPseudoChannel *
                                                            s_NumPseudoChannels);

    };
} // namespace Hbm
} // namespace Memory
