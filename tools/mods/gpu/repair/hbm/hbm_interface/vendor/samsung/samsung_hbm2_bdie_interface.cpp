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

#include "gpu/repair/hbm/hbm_interface/vendor/samsung/samsung_hbm2_bdie_interface.h"

#include "core/include/utility.h"
#include "gpu/repair/repair_util.h"

using namespace Memory;
using namespace Memory::Hbm;
using namespace Repair;

using HbmChannel = Hbm::Channel;

using WirF = Wir::Flags;
using WirT = Wir::Type;
using RegT = Wir::RegType;

namespace
{
    //! HBM2 Samsung B-Die WIRs
    vector<Wir> s_SamsungHbm2BDieWirs =
    {
        { "HARD_REPAIR", WirT::HARD_REPAIR, 0x08, RegT::Write, 21,
          SupportedHbmModels(
                {SpecVersion::V2},
                {Vendor::Samsung},
                {Die::B},
                {StackHeight::All},
                {Revision::D, Revision::F}),
          WirF::SingleChannelOnly
        },

        { "ENABLE_FUSE_SCAN", WirT::ENABLE_FUSE_SCAN, 0xc0, RegT::Read | RegT::Write, 26,
          SupportedHbmModels(
                {SpecVersion::V2},
                {Vendor::Samsung},
                {Die::B},
                {StackHeight::All},
                {Revision::D, Revision::F})
        }
    };
}

/******************************************************************************/
//SamsungHbm2BDie

RC SamsungHbm2BDie::Setup()
{
    RC rc;

    CHECK_RC(Hbm2Interface::Setup()); // Adds vendor inpedendent WIRs

    // Support checks
    //
    if (m_HbmModel.vendor != Vendor::Samsung)
    {
        Printf(Tee::PriError, "Attempt to setup Samsung HBM interface with a "
               "non-Samsung model:\n  %s\n",
               m_HbmModel.ToString().c_str());
        MASSERT(!"Attempt to setup Samsung interface with a non-Samsung "
                "HBM model");
        return RC::SOFTWARE_ERROR;
    }

    if (m_HbmModel.die != Die::B || !(m_HbmModel.revision == Revision::D
                                      || m_HbmModel.revision == Revision::F))
    {
        Printf(Tee::PriError, "Unsupported Samsung HBM model:\n  %s\n",
               m_HbmModel.ToString().c_str());
        return RC::UNSUPPORTED_FUNCTION;
    }

    CHECK_RC(AddWirs(s_SamsungHbm2BDieWirs));

    return rc;
}

RC SamsungHbm2BDie::RowRepair
(
    const Settings& settings,
    const SiteRowErrorMap& rowErrors,
    RepairType repairType
) const
{
    switch (repairType)
    {
        case RepairType::HARD:
            return HardRowRepair(settings, rowErrors);

        default:
            Printf(Tee::PriError, "Row repair: unsupported repair type");
            MASSERT(!"Row repair: unsupported repair type");
            return RC::SOFTWARE_ERROR;
    }
}

RC SamsungHbm2BDie::PrintRepairedRows() const
{
    MASSERT(IsSetup());
    RC rc;

    Printf(Tee::PriNormal, "== HBM Repaired Rows Summary\n");

    const vector<HBMSite>& sites = GpuInterface()->GetHbmDevice()->GetHbmSites();
    for (Site site = Site(0); site.Number() < sites.size(); ++site)
    {
        const HBMSite& siteObj = sites[site.Number()];

        Printf(Tee::PriNormal, "Site %u row repairs:\n", site.Number());
        if (!siteObj.IsSiteActive())
        {
            Printf(Tee::PriNormal, "  [site inactive]\n");
            continue;
        }

        bool siteHasRepairs = false;
        HbmDeviceIdData siteInfo = siteObj.GetSiteInfo();
        const string availableChannels = siteInfo.GetChannelsAvail();
        const UINT32 numStacks = ((siteInfo.GetStackHeight() == 8) ? 2 : 1);

        for (Stack stack(0); stack.Number() < numStacks; ++stack)
        {
            for (HbmChannel channel(0); channel.Number() < s_MaxNumChannels; ++channel)
            {
                const bool isChannelAvailable
                    = (availableChannels.find('a' + channel.Number()) != string::npos);
                if (!isChannelAvailable)
                {
                    Printf(Tee::PriLow, "  Stack %u Channel %u was unavailable\n",
                           stack.Number(), channel.Number());
                    continue;
                }

                UINT32 channelUsedRowCount = 0;
                string channelDetails;
                for (Bank bank(0); bank.Number() < s_NumBanksPerChannel; ++bank)
                {
                    UINT32 numSpareRows = 0;
                    CHECK_RC(GetNumSpareRowsAvailable(site, channel, stack, bank, &numSpareRows));

                    const UINT32 numUsedRows = s_NumSpareRowsPerBank - numSpareRows;
                    if (numUsedRows > 0)
                    {
                        channelDetails
                            += Utility::StrPrintf("    Bank %u has %u/%u repaired rows\n",
                                                  bank.Number(), numUsedRows, s_NumSpareRowsPerBank);
                        channelUsedRowCount += numUsedRows;
                        siteHasRepairs = true;
                    }
                }

                if (channelUsedRowCount > 0)
                {
                    MASSERT(!channelDetails.empty());
                    Printf(Tee::PriNormal,
                           "  Stack %u Channel %u has %u/%u repaired rows:\n%s",
                           stack.Number(), channel.Number(), channelUsedRowCount,
                           s_MaxRowRepairsPerChannel, channelDetails.c_str());
                }
            }
        }

        if (!siteHasRepairs)
        {
            Printf(Tee::PriNormal, "  [no repairs]\n");
        }
    }

    return rc;
}

RC SamsungHbm2BDie::HardRowRepair
(
    const Settings& settings,
    const SiteRowErrorMap& siteRowErrorMap
) const
{
    MASSERT(IsSetup());
    RC rc;

    // For ROW_REPAIR_FIRST_RC_SKIP
    constexpr RepairType repairType = Memory::RepairType::HARD;

    GpuHbmInterface* pGpuIntf = GpuInterface();
    MASSERT(pGpuIntf);

    const Wir* pWirHardRepair = nullptr;
    CHECK_RC(LookupWir(Wir::HARD_REPAIR, &pWirHardRepair));
    MASSERT(pWirHardRepair);

    const Wir* pWirBypass = nullptr;
    CHECK_RC(LookupWir(Wir::BYPASS, &pWirBypass));
    MASSERT(pWirBypass);

    StickyRC firstRepairRc;
    for (const auto& siteRowErrors : siteRowErrorMap)
    {
        const Site hbmSite = siteRowErrors.first;

        // Reset the HBM site before starting HW repair in case we did a SW
        // repair earlier
        CHECK_RC(pGpuIntf->ResetSite(hbmSite));

        for (const RowErrorInfo& rowErrInfo : siteRowErrors.second)
        {
            const RowError& rowErr      = rowErrInfo.error;
            const HbmChannel hbmChannel = rowErrInfo.hbmChan;
            const Row& row              = rowErr.row;

            ROW_REPAIR_FIRST_RC_SKIP(PreSingleRowRepair(hbmSite, hbmChannel, rowErr));

            Printf(Tee::PriNormal,
                   "[repair]   Repair type: hard row repair\n"
                   "[repair]   GPU location: HW FBPA %u, subpartition %u\n"
                   "[repair]   HBM location: site %u, stack %u, channel %u, bank %u, row %u\n"
                   "[repair]   Interface: IEEE1500\n"
                   "[repair]   skip_hbm_fuse_repair_check: %s\n"
                   "[repair]   pseudo_hard_repair: %s\n",
                   row.hwFbio.Number(), row.subpartition.Number(),
                   hbmSite.Number(), row.rank.Number(), hbmChannel.Number(),
                   row.bank.Number(), row.row,
                   Utility::ToString(settings.modifiers.skipHbmFuseRepairCheck).c_str(),
                   Utility::ToString(settings.modifiers.pseudoHardRepair).c_str());

            // Check spare rows after repair info print
            UINT32 numRowsAvailable = 0;
            if (!settings.modifiers.skipHbmFuseRepairCheck)
            {
                ROW_REPAIR_FIRST_RC_SKIP(GetNumSpareRowsAvailable(hbmSite, hbmChannel,
                                                                  Stack(row.rank), row.bank,
                                                                  &numRowsAvailable));

                if (numRowsAvailable < 1)
                {
                    Printf(Tee::PriError, "Insufficient spare rows available\n");
                    return RC::HBM_SPARE_ROWS_NOT_AVAILABLE;
                }
            }

            // Repair both pseudo channels
            for (PseudoChannel pseudoChannel(0);
                 pseudoChannel.Number() < s_NumPseudoChannels;
                 pseudoChannel++)
            {
                RowRepairWriteData repairData(Stack(row.rank), pseudoChannel, row.bank, row.row);

                if (!settings.modifiers.pseudoHardRepair)
                {
                    // Grab the IEEE1500 mutex
                    std::unique_ptr<PmgrMutexHolder> pPmgrMutex;
                    ROW_REPAIR_FIRST_RC_SKIP(pGpuIntf->AcquirePmgrMutex(&pPmgrMutex));

                    ROW_REPAIR_FIRST_RC_SKIP(pGpuIntf->WirWrite(*pWirHardRepair, hbmSite,
                                                                hbmChannel, repairData));
                    pGpuIntf->SleepMs(500);

                    // Write a Bypass instruction needed to reset the interface and
                    // allow for running additional instructions
                    ROW_REPAIR_FIRST_RC_SKIP(pGpuIntf->WirWrite(*pWirBypass, hbmSite,
                                                                Wir::CHANNEL_SELECT_ALL));
                }
            }

            // Reset is required for fuse burn to happen
            ROW_REPAIR_FIRST_RC_SKIP(pGpuIntf->ResetSite(hbmSite));

            if (!settings.modifiers.skipHbmFuseRepairCheck
                && !settings.modifiers.pseudoHardRepair)
            {
                // If the number of available rows didn't decrease, the repair wasn't successful
                const UINT32 prevNumRowsAvailable = numRowsAvailable;
                ROW_REPAIR_FIRST_RC_SKIP(GetNumSpareRowsAvailable(hbmSite, hbmChannel,
                                                                  Stack(row.rank), row.bank,
                                                                  &numRowsAvailable));
                if (numRowsAvailable != prevNumRowsAvailable - 1)
                {
                    Printf(Tee::PriError,
                           "Row repair failure in [site %u, stackID %u, channel %u, bank %u]\n",
                           hbmSite.Number(), row.rank.Number(), hbmChannel.Number(),
                           row.bank.Number());
                    return RC::HBM_SPARE_ROWS_NOT_AVAILABLE;
                }
            }

            firstRepairRc = PostSingleRowRepair(settings, repairType, rowErr,
                                                hbmSite, hbmChannel, RC::OK);
        }
    }

    return firstRepairRc;
}

RC SamsungHbm2BDie::GetNumSpareRowsAvailable
(
    const Site& hbmSite,
    const HbmChannel& hbmChannel,
    const Stack& stack,
    const Bank& bank,
    UINT32* pNumRowsAvailable,
    bool printResults
) const
{
    MASSERT(pNumRowsAvailable);
    *pNumRowsAvailable = 0;

    RC rc;

    GpuHbmInterface* pGpuIntf = GpuInterface();
    MASSERT(pGpuIntf);

    const Wir* pWirEnableFuseScan = nullptr;
    CHECK_RC(LookupWir(Wir::ENABLE_FUSE_SCAN, &pWirEnableFuseScan));
    MASSERT(pWirEnableFuseScan);

    const Wir* pWirBypass = nullptr;
    CHECK_RC(LookupWir(Wir::BYPASS, &pWirBypass));
    MASSERT(pWirBypass);

    if (printResults)
    {
        Printf(Tee::PriNormal,
               "Checking for spare rows in site %d, stackID %d, channel %d, bank %d.\n",
               hbmSite.Number(), stack.Number(), hbmChannel.Number(), bank.Number());
    }

    for (UINT32 row = 0; row < s_NumSpareRowsPerBank; row++)
    {
        for (PseudoChannel pseudoChannel(0);
             pseudoChannel.Number() < s_NumPseudoChannels;
             pseudoChannel++)
        {
            EnableFuseScanData fuseScanWrite = EnableFuseScanLookupTable::GetWdrData(stack,
                                                                                     pseudoChannel,
                                                                                     bank, row);

            // Grab the IEEE1500 mutex
            std::unique_ptr<PmgrMutexHolder> pPmgrMutex;
            CHECK_RC(pGpuIntf->AcquirePmgrMutex(&pPmgrMutex));

            CHECK_RC(pGpuIntf->WirWrite(*pWirEnableFuseScan, hbmSite, hbmChannel, fuseScanWrite));

            pGpuIntf->SleepMs(1); // Wait 2000ns

            EnableFuseScanData fuseScanRead;
            CHECK_RC(pGpuIntf->WirRead(*pWirEnableFuseScan, hbmSite, hbmChannel, &fuseScanRead));
            if (fuseScanRead.IsRowAvailable())
            {
                if (printResults)
                {
                    Printf(Tee::PriNormal, "Available row found: write: 0x%x, read: 0x%x\n",
                           fuseScanWrite.back(), fuseScanRead.back());
                }

                if (pseudoChannel.Number() > 0)
                {
                    (*pNumRowsAvailable)++;
                }
            }
            else
            {
                if (printResults)
                {
                    Printf(Tee::PriNormal, "Repaired row found: write: 0x%x, read: 0x%x\n",
                           fuseScanWrite.back(), fuseScanRead.back());
                }

                // Don't bother checking another pseudo-channel if one of them
                // is fused since we fuse both pseudo channels.
                break;
            }

            // Write a Bypass instruction needed to reset the interface and
            // allow for running additional instructions
            CHECK_RC(pGpuIntf->WirWrite(*pWirBypass, hbmSite, hbmChannel));
            pGpuIntf->SleepMs(100);
        }
    }

    return rc;
}

/******************************************************************************/
// RowRepairWriteData

SamsungHbm2BDie::RowRepairWriteData::RowRepairWriteData
(
    Stack stack,
    PseudoChannel hbmPseudoChan,
    Bank bank,
    UINT32 row
)
{
    using namespace Utility;

    MASSERT(m_Data.empty());
    m_Data.push_back(0);
    UINT32& data = m_Data.back();

    data |= 1 << 20;

    AssertWithinBitWidth(stack.Number(), 1, "stack ID");
    data |= stack.Number() << 19; // bit [19]: stack ID

    AssertWithinBitWidth(hbmPseudoChan.Number(), 1, "pseudo channel");
    m_Data.back() |= hbmPseudoChan.Number() << 18;

    AssertWithinBitWidth(stack.Number(), 4, "bank");
    data |= bank.Number() << 14;

    AssertWithinBitWidth(row, 14, "row address");
    data |= row;
}

/******************************************************************************/
// EnableFuseScanData

SamsungHbm2BDie::EnableFuseScanData::EnableFuseScanData
(
    const Stack& stack,
    UINT32 fuseScanMask
)
{
    using namespace Utility;

    MASSERT(m_Data.empty());
    m_Data.push_back(0);
    UINT32& data = m_Data.back();

    AssertWithinBitWidth(stack.Number(), 1, "stack ID");
    data |= stack.Number() ? (1 << 24) : (1 << 23);

    AssertWithinBitWidth(fuseScanMask, 23, "fuse scan mask");
    data |= fuseScanMask;
}

bool SamsungHbm2BDie::EnableFuseScanData::IsRowAvailable() const
{
    MASSERT(size() == 1);
    return (m_Data.back() & 0x1) != 0x1;
}

/******************************************************************************/
// EnableFuseScanLookupTable

SamsungHbm2BDie::EnableFuseScanData
SamsungHbm2BDie::EnableFuseScanLookupTable::GetWdrData
(
    const Stack& stack,
    const PseudoChannel& hbmPseudoChan,
    const Bank& bank,
    UINT32 row
)
{
    const UINT32 bankOffset = bank.Number() < 8 ? 0 : 16;
    const UINT32 pseudoChanOffset = hbmPseudoChan.Number() ? 16 : 0;
    const UINT32 tableRowIdx = pseudoChanOffset + bankOffset + bank.Number() * 2 + row;

    UINT32 fuseScanMask = 0;
    for (UINT32 i = 0; i < s_NumFuseScanTableCols; i++)
    {
        const UINT32 col = s_NumFuseScanTableCols - 1 - i;
        fuseScanMask += ((1 << i)
                         * static_cast<UINT32>(s_EnableFuseScanRegMask[tableRowIdx][col]));
    }

    return EnableFuseScanData(stack, fuseScanMask);
}

const bool SamsungHbm2BDie::EnableFuseScanLookupTable::
s_EnableFuseScanRegMask[64][SamsungHbm2BDie::EnableFuseScanLookupTable::s_NumFuseScanTableCols] =
{
    {0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0}
};
