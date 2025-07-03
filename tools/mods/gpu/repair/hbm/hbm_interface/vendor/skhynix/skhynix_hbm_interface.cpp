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

#include "skhynix_hbm_interface.h"

#include "gpu/repair/repair_util.h"

using namespace Memory;
using namespace Memory::Hbm;
using namespace Repair;

using WirF = Wir::Flags;
using WirT = Wir::Type;
using RegT = Wir::RegType;

namespace
{
    //! SK Hynix WIRs.
    vector<Wir> s_SkHynixHbm2Wirs =
    {
        { "SOFT_REPAIR", WirT::SOFT_REPAIR, 0x07, RegT::Write, 22,
          SupportedHbmModels(
                {SpecVersion::V2},
                {Vendor::SkHynix},
                {Die::Mask},
                {StackHeight::All},
                {Revision::V3, Revision::V4}),
          WirF::SingleChannelOnly
        },

        { "HARD_REPAIR", WirT::HARD_REPAIR, 0x08,
          // NOTE: Register read capability not formally listed in docs, but
          // need to read repair status.
          RegT::Write | RegT::Read, 24,
          SupportedHbmModels(
                {SpecVersion::V2},
                {Vendor::SkHynix},
                {Die::Mask},
                {StackHeight::All},
                {Revision::V3, Revision::V4})
        },

        { "PPR_INFO", WirT::PPR_INFO, 0xF1, RegT::Read, 64,
          SupportedHbmModels(
                {SpecVersion::V2},
                {Vendor::SkHynix},
                {Die::Mask},
                {StackHeight::All},
                {Revision::V3, Revision::V4})
        }
    };
}

RC Hbm::SkHynixHbm2::Setup()
{
    RC rc;

    CHECK_RC(Hbm2Interface::Setup()); // Adds vendor inpedendent WIRs

    // Support checks
    //
    if (m_HbmModel.vendor != Vendor::SkHynix)
    {
        Printf(Tee::PriError, "Attempt to setup SK Hynix HBM interface with a "
               "non-SK Hynix HBM model:\n\t%s\n",
               m_HbmModel.ToString().c_str());
        MASSERT(!"Attempt to setup SK Hynix HBM interface with a non-SK Hynix "
                "HBM model");
        return RC::SOFTWARE_ERROR;
    }

    // Only supports:
    // - Mask3
    // - Mask4
    if (m_HbmModel.die != Die::Mask || !(m_HbmModel.revision == Revision::V3
                                         || m_HbmModel.revision == Revision::V4))
    {
        Printf(Tee::PriError, "Unsupported SK Hynix HBM model:\n\t%s\n",
               m_HbmModel.ToString().c_str());
        return RC::UNSUPPORTED_FUNCTION;
    }

    CHECK_RC(AddWirs(s_SkHynixHbm2Wirs));

    return rc;
}

RC Hbm::SkHynixHbm2::RowRepair
(
    const Settings& settings,
    const SiteRowErrorMap& siteRowErrorMap,
    RepairType repairType
) const
{
    if (settings.modifiers.skipHbmFuseRepairCheck)
    {
        Printf(Tee::PriWarn, "SK Hynix row repair does not support skipping "
               "the HBM fuse repair check. Continuing repair with check.\n");
    }

    // Two neighbouring rows are always repaired simultaneously due to repair
    // structure. Filter out any row that would result in duplicate repairs.
    // TODO(stewarts): Report rows as repaired to the JS layer if their
    // neighbour was repaired.
    SiteRowErrorMap filteredRowsMap;
    for (const auto& siteRowErrors : siteRowErrorMap)
    {
        filteredRowsMap[siteRowErrors.first] = siteRowErrors.second;

        vector<RowErrorInfo> &filteredRowErrors = filteredRowsMap[siteRowErrors.first];
        std::sort(filteredRowErrors.begin(), filteredRowErrors.end(),
                  CmpRowErrorInfoByRow);
        FilterOutNeighbouringRowsFromSorted(&filteredRowErrors);
    }

    switch (repairType)
    {
        case RepairType::HARD:
            return HardRowRepair(settings, filteredRowsMap);

        case RepairType::SOFT:
            return SoftRowRepair(settings, filteredRowsMap);

        default:
            Printf(Tee::PriError, "Row repair: unsupported repair type");
            MASSERT(!"Row repair: unsupported repair type");
            return RC::SOFTWARE_ERROR;
    }
}

RC Hbm::SkHynixHbm2::PrintRepairedRows() const
{
    MASSERT(IsSetup());
    RC rc;

    const vector<HBMSite>& hbmSites = GpuInterface()->GetHbmDevice()->GetHbmSites();

    Printf(Tee::PriNormal, "== HBM Repaired Rows Summary:\n");
    for (Site site(0); site.Number() < hbmSites.size(); ++site)
    {
        const HBMSite& siteObj = hbmSites[site.Number()];

        Printf(Tee::PriNormal, "Site %u Row Repairs:\n", site.Number());

        if (!siteObj.IsSiteActive())
        {
            Printf(Tee::PriNormal, "\t[site inactive]\n");
            continue;
        }

        bool siteHasRepairs = false;

        HbmDeviceIdData siteInfo = siteObj.GetSiteInfo();
        const string availableChannels = siteInfo.GetChannelsAvail();
        const UINT32 numStacks = ((siteInfo.GetStackHeight() == 8) ? 2 : 1);

        for (Stack stack(0); stack.Number() < numStacks; ++stack)
        {
            for (Channel channel(0); channel.Number() < s_MaxNumChannels; ++channel)
            {
                const bool isChannelAvailable
                    = (availableChannels.find('a' + channel.Number()) != string::npos);
                if (!isChannelAvailable)
                {
                    Printf(Tee::PriLow, "Stack %u Channel %u was unavailable\n",
                           stack.Number(), channel.Number());
                    continue;
                }

                UINT32 numChannelRepairs;
                CHECK_RC(PrintChannelRepairedRows(site, stack, channel, &numChannelRepairs));
                if (numChannelRepairs > 0)
                {
                    siteHasRepairs = true;
                }
            }
        }

        if (!siteHasRepairs)
        {
            Printf(Tee::PriNormal, "\t[none]\n");
        }
    }

    return rc;
}

RC Hbm::SkHynixHbm2::PrintChannelRepairedRows
(
    Site site,
    Stack stack,
    Channel channel,
    UINT32 *pNumBurnedFusesOnChannel
) const
{
    RC rc;
    MASSERT(pNumBurnedFusesOnChannel);

    const Wir* pWirPprInfo = nullptr;
    CHECK_RC(LookupWir(Wir::PPR_INFO, &pWirPprInfo));
    MASSERT(pWirPprInfo);

    *pNumBurnedFusesOnChannel = 0;
    PprInfoReadData pprInfoData;
    {
        // Grab the IEEE1500 mutex
        std::unique_ptr<PmgrMutexHolder> pPmgrMutex;
        CHECK_RC(GpuInterface()->AcquirePmgrMutex(&pPmgrMutex));

        CHECK_RC(GpuInterface()->WirRead(*pWirPprInfo, site, channel, &pprInfoData));
    }

    string channelDetailsStr;

    const UINT32 numBankPairs = BankPair::NumBankPairs();
    for (UINT32 bankPairIndex = 0; bankPairIndex < numBankPairs; ++bankPairIndex)
    {
        BankPair bankPair = BankPair::FromBankPairIndex(bankPairIndex);

        // NOTE: Bank pairs share the same row fuses.
        RowBurnedFuseMask rowBurnedFuseMask
            = pprInfoData.GetRowBurnedFuseMask(stack, bankPair.bank0);
        if (rowBurnedFuseMask.Mask() == 0)
        {
            continue;
        }

        UINT32 numBurnedFusesOnBankPair = 0;
        string fusesStr;
        for (RowFuse rowFuse(0);
             rowFuse.Number() < s_NumSpareRowsPerBankPair;
             ++rowFuse)
        {
            if (rowBurnedFuseMask.IsFuseBurned(rowFuse))
            {
                numBurnedFusesOnBankPair++;
                fusesStr += Utility::StrPrintf("%u, ", rowFuse.Number());
            }
        }
        MASSERT(!fusesStr.empty());
        fusesStr.pop_back(); // Remove separator
        fusesStr.pop_back(); // ^

        channelDetailsStr += Utility::StrPrintf(
            "\t\tBank pair %s has %u/%u repairs using fuse(s) %s\n",
            bankPair.ToString().c_str(), numBurnedFusesOnBankPair,
            s_NumSpareRowsPerBankPair, fusesStr.c_str());

        *pNumBurnedFusesOnChannel += numBurnedFusesOnBankPair;
    }

    if (*pNumBurnedFusesOnChannel > 0)
    {
        Printf(Tee::PriNormal, "\tStack %u Channel %u has %u/%u "
               "repaired rows:\n%s", stack.Number(), channel.Number(),
               *pNumBurnedFusesOnChannel, s_MaxRowRepairsPerChannel,
               channelDetailsStr.c_str());
    }

    return rc;
}

void Hbm::SkHynixHbm2::FilterOutNeighbouringRowsFromSorted
(
    vector<RowErrorInfo>* pRowErrors
) const
{
    MASSERT(pRowErrors);

    vector<RowErrorInfo>::iterator lastRowIt;
    for (auto it = pRowErrors->begin(); it != pRowErrors->end();)
    {
        const bool isFirstRow = (it == pRowErrors->begin());

        const Row& lwrRow = it->error.row;
        const Row& lastRow = lastRowIt->error.row;

        // Considered neighbouring row if bits [13:1] match in the 14 bit row
        // address.
        const bool inSameBank = !isFirstRow
            && lwrRow.hwFbio       == lastRow.hwFbio
            && lwrRow.subpartition == lastRow.subpartition
            && lwrRow.rank         == lastRow.rank
            && lwrRow.bank         == lastRow.bank;

        const bool foundNeighbour = inSameBank
            && ((lwrRow.row & ~0x1U) == (lastRow.row & ~0x1U));

        if (foundNeighbour)
        {
            Printf(Tee::PriNormal, "Repairing row %s will implicitly repair "
                   "row %s. Removing %s from the repair list.\n",
                   lastRow.rowName.c_str(), lwrRow.rowName.c_str(),
                   lwrRow.rowName.c_str());
            it = pRowErrors->erase(it);
        }
        else
        {
            lastRowIt = it; // Erasing after this iterator, will remain valid
            ++it;
        }
    }
}

RC Hbm::SkHynixHbm2::GetSpareRowWithRepairCheck
(
    Site hbmSite,
    Channel hbmChannel,
    const Row& row,
    RowFuse* pSpareRowFuse
) const
{
    MASSERT(pSpareRowFuse);
    MASSERT(IsSetup());
    RC rc;

    const Wir* pWirPprInfo = nullptr;
    CHECK_RC(LookupWir(Wir::PPR_INFO, &pWirPprInfo));
    MASSERT(pWirPprInfo);

    // Grab the IEEE1500 mutex
    std::unique_ptr<PmgrMutexHolder> pPmgrMutex;
    CHECK_RC(GpuInterface()->AcquirePmgrMutex(&pPmgrMutex));

    PprInfoReadData pprInfoData;
    CHECK_RC(GpuInterface()->WirRead(*pWirPprInfo, hbmSite, hbmChannel,
                                     &pprInfoData));

    // Check if channel hit max repairs
    if (pprInfoData.TotalBurnedFuses() >= s_MaxRowRepairsPerChannel)
    {
        Printf(Tee::PriWarn, "Maximum number of repairs (%u) have been "
               "performed on channel [site %u, stackID %u, channel %u]\n",
               s_MaxRowRepairsPerChannel, hbmSite.Number(), row.rank.Number(),
               hbmChannel.Number());
        return RC::HBM_SPARE_ROWS_NOT_AVAILABLE;
    }

    RC getFuseRc = pprInfoData.GetFirstAvailableFuse(Stack(row.rank.Number()),
                                                     row.bank, pSpareRowFuse);

    // Check if bank pair hit max repairs
    if ((getFuseRc != RC::OK) || (pSpareRowFuse->Number() >= s_NumSpareRowsPerBankPair))
    {
        Printf(Tee::PriWarn, "No spare rows available in [site %u, stack "
               "%u, channel %u, bank %u]\n", hbmSite.Number(),
               row.rank.Number(), hbmChannel.Number(), row.bank.Number());
        return RC::HBM_SPARE_ROWS_NOT_AVAILABLE;
    }

    return rc;
}

RC Hbm::SkHynixHbm2::SoftRowRepair
(
    const Settings& settings,
    const SiteRowErrorMap& siteRowErrorMap
) const
{
    MASSERT(IsSetup());
    RC rc;

    // For ROW_REPAIR_FIRST_RC_SKIP
    constexpr RepairType repairType = RepairType::SOFT;

    GpuHbmInterface* pGpuIntf = GpuInterface();
    MASSERT(pGpuIntf);

    const Wir* pWirSoftRepair = nullptr;
    CHECK_RC(LookupWir(Wir::SOFT_REPAIR, &pWirSoftRepair));
    MASSERT(pWirSoftRepair);
    const Wir* pWirBypass = nullptr;
    CHECK_RC(LookupWir(Wir::BYPASS, &pWirBypass));
    MASSERT(pWirBypass);

    StickyRC firstRepairRc;
    for (const auto& siteRowErrors : siteRowErrorMap)
    {
        const Site hbmSite = siteRowErrors.first;
        for (const RowErrorInfo& rowErrInfo : siteRowErrors.second)
        {
            const RowError& rowErr      = rowErrInfo.error;
            const HbmChannel hbmChannel = rowErrInfo.hbmChan;
            const Row& row              = rowErr.row;

            ROW_REPAIR_FIRST_RC_SKIP(PreSingleRowRepair(hbmSite, hbmChannel, rowErr));

            RowFuse spareRowFuse;
            ROW_REPAIR_FIRST_RC_SKIP(GetSpareRowWithRepairCheck(hbmSite, hbmChannel,
                                                                row, &spareRowFuse));

            // NOTE: SK Hynix repairs both pseudo channels at once
            SoftRepairWriteData softRepairData(spareRowFuse,
                                               Stack(row.rank.Number()),
                                               row.bank, row.row,
                                               SoftRepairWriteData::ENABLE_REPAIR);

            {
                BankPair bankPair
                    = BankPair::FromBankPairIndex(BankPair::ToBankPairIndex(row.bank));
                Printf(Tee::PriNormal,
                       "[repair] \tRepair type: soft repair\n"
                       "[repair] \tGPU location: HW FBPA %u, subpartition %u\n"
                       "[repair] \tHBM location: site %u, stack %u, channel %u, bank %u, row %u\n"
                       "[repair] \tBank pair: %s\n"
                       "[repair] \tUsing bank pair fuse: %u\n"
                       "[repair] \tInterface: IEEE1500\n"
                       "[repair] \tskip_hbm_fuse_repair_check: %s (ignored)\n",
                       row.hwFbio.Number(), row.subpartition.Number(),
                       hbmSite.Number(), row.rank.Number(), hbmChannel.Number(),
                       row.bank.Number(), row.row,
                       bankPair.ToString().c_str(),
                       spareRowFuse.Number(),
                       Utility::ToString(settings.modifiers.skipHbmFuseRepairCheck).c_str());
            }

            // NOTE: SK Hynix supports writing all of the rows to be repaired in one
            // instruction by doing:
            // 1) WIR INSTR and MODE set
            // 2) Write WDR for each repair with SOFT_REPAIR WDR start bit set to 1
            // 3) Repeat (1) until all rows are entered
            //    - may be able to skip (1) and start with (2)
            // 4) Dummy write with SOFT_REPAIR WDR start bit set to 0
            //
            // To guarantee each row is being repaired, only one row is repaired at
            // a time.

            // Grab the IEEE1500 mutex
            std::unique_ptr<PmgrMutexHolder> pPmgrMutex;
            ROW_REPAIR_FIRST_RC_SKIP(pGpuIntf->AcquirePmgrMutex(&pPmgrMutex));

            ROW_REPAIR_FIRST_RC_SKIP(pGpuIntf->WirWrite(*pWirSoftRepair,
                                                        hbmSite,
                                                        hbmChannel,
                                                        softRepairData));

            // End the row repair input with dummy data write
            softRepairData.UnsetStartBit();
            ROW_REPAIR_FIRST_RC_SKIP(pGpuIntf->WdrWriteRaw(hbmSite,
                                                           hbmChannel,
                                                           softRepairData));

            pGpuIntf->SleepUs(5);

            // Write bypass instruction to reset the interface. Allows
            // running additional instructions.
            ROW_REPAIR_FIRST_RC_SKIP(pGpuIntf->WirWrite(*pWirBypass, hbmSite,
                                                        Wir::CHANNEL_SELECT_ALL));

            // NOTE: No way to verify the soft repairs oclwred since PPR_INFO only
            // reports hard repairs.

            // Final step, record repair metadata
            //
            firstRepairRc = PostSingleRowRepair(settings, repairType, rowErr, hbmSite,
                                                hbmChannel, RC::OK);
        }
    }

    return firstRepairRc;
}

RC Hbm::SkHynixHbm2::HardRowRepair
(
    const Settings& settings,
    const SiteRowErrorMap& siteRowErrorMap
) const
{
    MASSERT(IsSetup());
    RC rc;

    // For ROW_REPAIR_FIRST_RC_SKIP
    constexpr RepairType repairType = RepairType::HARD;

    GpuHbmInterface* pGpuIntf = GpuInterface();
    MASSERT(pGpuIntf);

    const Wir* pWirHardRepair = nullptr;
    CHECK_RC(LookupWir(Wir::HARD_REPAIR, &pWirHardRepair));
    MASSERT(pWirHardRepair);
    const Wir* pWirPprInfo = nullptr;
    CHECK_RC(LookupWir(Wir::PPR_INFO, &pWirPprInfo));
    MASSERT(pWirPprInfo);

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

            // Reset the HBM site before starting HW repair in case we did a SW
            // repair earlier
            ROW_REPAIR_FIRST_RC_SKIP(pGpuIntf->ResetSite(hbmSite));

            RowFuse spareRowFuse;
            ROW_REPAIR_FIRST_RC_SKIP(GetSpareRowWithRepairCheck(hbmSite, hbmChannel,
                                                                row, &spareRowFuse));

            // NOTE: SK Hynix repairs both pseudo channels at once
            HardRepairData hardRepairData(spareRowFuse, Stack(row.rank.Number()),
                                          row.bank, row.row,
                                          HardRepairData::ENABLE_REPAIR);

            {
                BankPair bankPair
                    = BankPair::FromBankPairIndex(BankPair::ToBankPairIndex(row.bank));
                Printf(Tee::PriNormal,
                       "[repair] \tRepair type: hard repair\n"
                       "[repair] \tGPU location: HW FBPA %u, subpartition %u\n"
                       "[repair] \tHBM location: site %u, stack %u, channel %u, bank %u, row %u\n"
                       "[repair] \tBank pair: %s, using fuse: %u\n"
                       "[repair] \tInterface: IEEE1500\n"
                       "[repair] \tskip_hbm_fuse_repair_check: %s (ignored)\n"
                       "[repair] \tpseudo_hard_repair: %s\n",
                       row.hwFbio.Number(), row.subpartition.Number(),
                       hbmSite.Number(), row.rank.Number(), hbmChannel.Number(),
                       row.bank.Number(), row.row,
                       bankPair.ToString().c_str(), spareRowFuse.Number(),
                       Utility::ToString(settings.modifiers.skipHbmFuseRepairCheck).c_str(),
                       Utility::ToString(settings.modifiers.pseudoHardRepair).c_str());
            }

            // NOTE: SK Hynix supports writing all of the rows to be repaired in
            // one go by doing:
            // 1) WIR INSTR and MODE set.
            // 2) Write WDR for each repair with SOFT_REPAIR WDR start bit set to 1.
            // 3) Repeat (2) until all rows are entered.
            //    - may be able to skip (1) and start with (2)
            // 4) Dummy write with HARD_REPAIR WDR start bit set to 0.
            //
            // To guarantee each row is being repaired, only one row is repaired at
            // a time.

            if (!settings.modifiers.pseudoHardRepair)
            {
                // Grab the IEEE1500 mutex
                std::unique_ptr<PmgrMutexHolder> pPmgrMutex;
                ROW_REPAIR_FIRST_RC_SKIP(pGpuIntf->AcquirePmgrMutex(&pPmgrMutex));

                ROW_REPAIR_FIRST_RC_SKIP(pGpuIntf->WirWrite(*pWirHardRepair, hbmSite,
                                                            hbmChannel, hardRepairData));

                pGpuIntf->SleepMs(150); // sleep after each HARD_REPAIR WDR write

                HardRepairData hardRepairResultData;
                ROW_REPAIR_FIRST_RC_SKIP(pGpuIntf->WdrReadRaw(*pWirHardRepair,
                                                              hbmSite, hbmChannel,
                                                              &hardRepairResultData));
                const bool rowSucceeded = hardRepairResultData.RepairSucceeded();
                if (!rowSucceeded)
                {
                    // NOTE: If the row did not succeed, we still need to send the
                    // dummy WDR to signal the end of the row data and wait.
                    //
                    // Make sure that the failure is recorded incase the postamble
                    // of the hard repair fails.
                    Printf(Tee::PriError, "HBM reported failure to repair row %s\n",
                           row.rowName.c_str());
                    firstRepairRc = RC::BAD_HBM_FUSE_DATA;
                }

                pGpuIntf->SleepMs(500); // sleep after final repair in sequence

                // TODO(stewarts): Need a site reset. Also says we need an "HBM
                // init (already there in VBIOS)" before it takes affect. Does
                // that mean a GPU init as well, or is a site reset enough?
                ROW_REPAIR_FIRST_RC_SKIP(pGpuIntf->ToggleWrstN(hbmSite));

                // NOTE: No bypass needed due to HBM site reset.

                if (!rowSucceeded)
                {
                    ROW_REPAIR_FIRST_RC_SKIP(RC::BAD_HBM_FUSE_DATA);
                }
            }

            // Confirm the repair happened
            //
            PprInfoReadData postRepairPprInfoData;
            ROW_REPAIR_FIRST_RC_SKIP(pGpuIntf->WirRead(*pWirPprInfo, hbmSite,
                                                       hbmChannel,
                                                       &postRepairPprInfoData));
            if (!postRepairPprInfoData.IsFuseBurned(Stack(row.rank.Number()),
                                                    row.bank, spareRowFuse))
            {
                Printf(Tee::PriError, "Fuse used to repair row %s is reporting "
                       "unused after repair!\n", row.rowName.c_str());
                ROW_REPAIR_FIRST_RC_SKIP(RC::BAD_HBM_FUSE_DATA);
            }

            // Final step, record repair metadata
            //
            firstRepairRc = PostSingleRowRepair(settings, repairType, rowErr, hbmSite,
                                                hbmChannel, RC::OK);
        }
    }

    return firstRepairRc;
}

/******************************************************************************/
// RowBurnedFuseMask

RC Hbm::SkHynixHbm2::RowBurnedFuseMask::GetFirstAvailableFuse(RowFuse* pRowFuse)
{
    MASSERT(pRowFuse);

    constexpr INT32 noBitSet = -1;

    // The burned fuse mask shows the fuses that have been used. The
    // complement shows which fuses are available.
    const UINT08 availableFuseMask = ~static_cast<UINT08>(*this);
    const INT32 bitPos = Utility::BitScanForward(availableFuseMask);
    if (bitPos == noBitSet)
    {
        return RC::HBM_SPARE_ROWS_NOT_AVAILABLE;
    }

    MASSERT(bitPos >= 0);
    *pRowFuse = RowFuse(static_cast<UINT08>(bitPos));

    return RC::OK;
}

bool Hbm::SkHynixHbm2::RowBurnedFuseMask::IsFuseBurned(RowFuse rowFuse) const
{
    constexpr UINT08 burnedFuseFlag = 0x1; // (0-unused, 1-used)
    // bit[3]: fuse 3 information
    // bit[2]: fuse 2 information
    // bit[1]: fuse 1 information
    // bit[0]: fuse 0 information
    return ((Mask() >> rowFuse.Number()) & burnedFuseFlag) == burnedFuseFlag;
}

/******************************************************************************/
// SoftRepairWriteData

Hbm::SkHynixHbm2::SoftRepairWriteData::SoftRepairWriteData
(
    RowFuse rowFuse,
    Stack stack,
    Bank bank,
    UINT32 row,
    EnableRepair enableRepair
)
{
    using namespace Utility;

    MASSERT(m_Data.empty());
    m_Data.push_back(0);
    UINT32& data = m_Data.back();

    data |= 1 << 21;                // bit[   21]: signals start of soft repair

    AssertWithinBitWidth(rowFuse.Number(), 2, "fuse set");
    data |= rowFuse.Number() << 19; // bit[20:19]: fuse set

    AssertWithinBitWidth(stack.Number(), 1, "stack ID");
    data |= stack.Number() << 18;   // bit[   18]: stack ID

    AssertWithinBitWidth(bank.Number(), 4, "bank address");
    data |= bank.Number() << 14;    // bit[17:14]: row bank address

    AssertWithinBitWidth(row, 14, "row address");
    // NOTE: The last bit of the 14-bit row address is ignored. Only
    // write the upper 13-bits.
    data |= (row >> 1) << 1;        // bit[13: 1]: row address [14:1]

    if (enableRepair == ENABLE_REPAIR)
    {
        data |= 1;                  // bit[    0]: enable repair
    }
    else
    {
        MASSERT(enableRepair == DISABLE_REPAIR);
        MASSERT((data & 1) == 0); // Must be set to 0 to signal repair disabled
    }
}

void Hbm::SkHynixHbm2::SoftRepairWriteData::SetStartBit()
{
    m_Data.back() |= 1 << 21; // bit[21]: signals start of soft repair
}

void Hbm::SkHynixHbm2::SoftRepairWriteData::UnsetStartBit()
{
    m_Data.back() &= ~(1 << 21); // bit[21]: signals start of soft repair
}

/******************************************************************************/
// HardRepairData

Hbm::SkHynixHbm2::HardRepairData::HardRepairData
(
    RowFuse rowFuse,
    Stack stack,
    Bank bank,
    UINT32 row,
    bool enableRepair
)
{
    using namespace Utility;

    MASSERT(m_Data.empty());
    m_Data.push_back(0);
    UINT32& data = m_Data.back();

    data |= 1 << 23;                // bit[   23]: signals start of hard repair

    AssertWithinBitWidth(rowFuse.Number(), 2, "fuse set");
    data |= rowFuse.Number() << 20; // bit[21:20]: fuse set

    AssertWithinBitWidth(stack.Number(), 1, "stack ID");
    data |= stack.Number() << 19;   // bit[   19]: stack ID

    AssertWithinBitWidth(bank.Number(), 4, "bank address");
    data |= bank.Number() << 15;    // bit[18:15]: row bank address

    AssertWithinBitWidth(row, 14, "row address");
    // NOTE: The last bit of the 14-bit row address is ignored. Only
    // write the upper 13-bits.
    data |= row << 1;        // bit[14: 1]: row address [14:1]

    // Bit 22 is reserved and must be set to 0
    MASSERT((data & (1 << 22)) == 0);
    // Bit 0 is reserved for the repair complete signal
    MASSERT((data & (1 << 0)) == 0);
}

void Hbm::SkHynixHbm2::HardRepairData::SetStartBit()
{
    m_Data.back() |= 1 << 23; // bit[23]: signals start of hard repair
}

void Hbm::SkHynixHbm2::HardRepairData::UnsetStartBit()
{
    m_Data.back() &= ~(1 << 23); // bit[23]: signals start of hard repair
}

bool Hbm::SkHynixHbm2::HardRepairData::RepairSucceeded() const
{
    // bit[0]: signal that repair is successful if set to 1, o/w repair failed
    return m_Data.back() & 0x1;
}

/******************************************************************************/
// BankPair

string Hbm::SkHynixHbm2::BankPair::ToString() const
{
    return Utility::StrPrintf("(%u,%u)", bank0.Number(), bank1.Number());
}

UINT32 Hbm::SkHynixHbm2::BankPair::ToBankPairIndex(Bank bank)
{
    switch (bank.Number())
    {
        case 0:
        case 2:
            return 0;
        case 1:
        case 3:
            return 1;
        case 4:
        case 6:
            return 2;
        case 5:
        case 7:
            return 3;
        case 8:
        case 10:
            return 4;
        case 9:
        case 11:
            return 5;
        case 12:
        case 14:
            return 6;
        case 13:
        case 15:
            return 7;
        default:
            MASSERT(!"Invalid bank number");
            return -1;
    }
}

Hbm::SkHynixHbm2::BankPair
Hbm::SkHynixHbm2::BankPair::FromBankPairIndex(UINT32 bankPairIndex)
{
    switch (bankPairIndex)
    {
        case 0: return {Bank(0),  Bank(2)};
        case 1: return {Bank(1),  Bank(3)};
        case 2: return {Bank(4),  Bank(6)};
        case 3: return {Bank(5),  Bank(7)};
        case 4: return {Bank(8),  Bank(10)};
        case 5: return {Bank(9),  Bank(11)};
        case 6: return {Bank(12), Bank(14)};
        case 7: return {Bank(13), Bank(15)};

        default:
            MASSERT(!"Invalid bank pair index");
            return {Bank(-1), Bank(-1)};
    }
}

/******************************************************************************/
// PprInfoReadData

RC Hbm::SkHynixHbm2::PprInfoReadData::GetFirstAvailableFuse
(
    Stack stack,
    Bank bank,
    RowFuse* pRowFuse
) const
{
    MASSERT(pRowFuse);
    RC rc;

    RowBurnedFuseMask burnedFuseMask = GetRowBurnedFuseMask(stack, bank);
    RowFuse rowFuse;
    CHECK_RC(burnedFuseMask.GetFirstAvailableFuse(&rowFuse));
    *pRowFuse = rowFuse;

    return rc;
}

Hbm::SkHynixHbm2::RowBurnedFuseMask
Hbm::SkHynixHbm2::PprInfoReadData::GetRowBurnedFuseMask
(
    Stack stack,
    Bank bank
) const
{
    MASSERT(m_Data.size() == 2); // PPR_INFO is 64bits
    MASSERT(stack.Number() < 2); // only 2 stacks supported

    // bits[63:32]: stack 1
    // bits[31: 0]: stack 0
    const UINT32& stackData = m_Data[stack.Number()];

    constexpr UINT32 bankPairBitWidth = 4;
    constexpr UINT32 bankPairMask = 0xf;

    const UINT32 offset = bankPairBitWidth * BankPair::ToBankPairIndex(bank);
    const UINT32 fuseMaskData = (stackData >> offset) & bankPairMask;

    return RowBurnedFuseMask(static_cast<UINT08>(fuseMaskData));
}

bool Hbm::SkHynixHbm2::PprInfoReadData::IsFuseBurned
(
    Stack stack,
    Bank bank,
    RowFuse rowFuse
) const
{
    return GetRowBurnedFuseMask(stack, bank).IsFuseBurned(rowFuse);
}

UINT32 Hbm::SkHynixHbm2::PprInfoReadData::TotalBurnedFuses() const
{
    UINT32 totalBurnedFuses = 0;

    for (const UINT32 data : m_Data)
    {
        totalBurnedFuses += Utility::CountBits(data);
    }

    return totalBurnedFuses;
}
