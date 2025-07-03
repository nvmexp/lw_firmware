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

#include "skhynix_hbm2e_interface.h"

#include "gpu/repair/repair_util.h"

using namespace Memory;
using namespace Memory::Hbm;
using namespace Repair;

using WirF = Wir::Flags;
using WirT = Wir::Type;
using RegT = Wir::RegType;

namespace
{
    // TODO(aperiwal): Change die / revisions once known

    //! SK Hynix HBM 2e WIRs.
    vector<Wir> s_SkHynixHbm2eWirs =
    {
        { "SOFT_REPAIR", WirT::SOFT_REPAIR, 0x07, RegT::Write, 23,
          SupportedHbmModels(
                {SpecVersion::V2e},
                {Vendor::SkHynix},
                {Die::Unknown},
                {StackHeight::All},
                {Revision::Unknown}),
          WirF::SingleChannelOnly
        },

        { "HARD_REPAIR", WirT::HARD_REPAIR, 0x08,
          // NOTE: Register read capability not formally listed in docs, but
          // need to read repair status.
          RegT::Write | RegT::Read, 25,
          SupportedHbmModels(
                {SpecVersion::V2e},
                {Vendor::SkHynix},
                {Die::Unknown},
                {StackHeight::All},
                {Revision::Unknown}),
        },

        { "PPR_INFO", WirT::PPR_INFO, 0xF1, RegT::Read, 128,
          SupportedHbmModels(
                {SpecVersion::V2e},
                {Vendor::SkHynix},
                {Die::Unknown},
                {StackHeight::All},
                {Revision::Unknown}),
        }
    };
}

RC Hbm::SkHynixHbm2e::Setup()
{
    RC rc;

    CHECK_RC(Hbm2Interface::Setup()); // Adds vendor inpedendent WIRs

    // Support checks
    //
    if (m_HbmModel.vendor != Vendor::SkHynix)
    {
        Printf(Tee::PriError, "Attempt to setup SK Hynix HBM2e interface with a "
               "non-SK Hynix HBM model:\n\t%s\n",
               m_HbmModel.ToString().c_str());
        MASSERT(!"Attempt to setup SK Hynix HBM2e interface with a non-SK Hynix "
                "HBM model");
        return RC::SOFTWARE_ERROR;
    }

    // TODO(aperiwal): Add die / revision checks once known

    CHECK_RC(AddWirs(s_SkHynixHbm2eWirs));

    return rc;
}

RC Hbm::SkHynixHbm2e::PrintChannelRepairedRows
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

    for (UINT32 bank = 0; bank < s_NumBanksPerChannel; bank++)
    {
        RowBurnedFuseMask rowBurnedFuseMask
            = pprInfoData.GetRowBurnedFuseMask(stack, Bank(bank));
        if (rowBurnedFuseMask.Mask() == 0)
        {
            continue;
        }

        UINT32 numBurnedFusesOnBank = 0;
        string fusesStr;
        for (RowFuse rowFuse(0);
             rowFuse.Number() < s_NumSpareRowsPerBank;
             ++rowFuse)
        {
            if (rowBurnedFuseMask.IsFuseBurned(rowFuse))
            {
                numBurnedFusesOnBank++;
                fusesStr += Utility::StrPrintf("%u, ", rowFuse.Number());
            }
        }
        MASSERT(!fusesStr.empty());
        fusesStr.pop_back(); // Remove separator
        fusesStr.pop_back(); // ^

        channelDetailsStr += Utility::StrPrintf(
            "\t\tBank %u has %u/%u repairs using fuse(s) %s\n",
            bank, numBurnedFusesOnBank,
            s_NumSpareRowsPerBank, fusesStr.c_str());

        *pNumBurnedFusesOnChannel += numBurnedFusesOnBank;
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

RC Hbm::SkHynixHbm2e::GetSpareRowWithRepairCheck
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

    // Check if bank hit max repairs
    if (getFuseRc != RC::OK)
    {
        Printf(Tee::PriWarn, "No spare rows available in [site %u, stack "
               "%u, channel %u, bank %u]\n", hbmSite.Number(),
               row.rank.Number(), hbmChannel.Number(), row.bank.Number());
        return RC::HBM_SPARE_ROWS_NOT_AVAILABLE;
    }

    return rc;
}

RC Hbm::SkHynixHbm2e::SoftRowRepair
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
                Printf(Tee::PriNormal,
                       "[repair] \tRepair type: soft repair\n"
                       "[repair] \tGPU location: HW FBPA %u, subpartition %u\n"
                       "[repair] \tHBM location: site %u, stack %u, channel %u, bank %u, row %u\n"
                       "[repair] \tUsing fuse: %u\n"
                       "[repair] \tInterface: IEEE1500\n"
                       "[repair] \tskip_hbm_fuse_repair_check: %s (ignored)\n",
                       row.hwFbio.Number(), row.subpartition.Number(),
                       hbmSite.Number(), row.rank.Number(), hbmChannel.Number(),
                       row.bank.Number(), row.row,
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

RC Hbm::SkHynixHbm2e::HardRowRepair
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

            RowFuse spareRowFuse;
            ROW_REPAIR_FIRST_RC_SKIP(GetSpareRowWithRepairCheck(hbmSite, hbmChannel,
                                                                row, &spareRowFuse));

            // NOTE: SK Hynix repairs both pseudo channels at once
            HardRepairData hardRepairData(spareRowFuse, Stack(row.rank.Number()),
                                          row.bank, row.row,
                                          HardRepairData::ENABLE_REPAIR);

            {
                Printf(Tee::PriNormal,
                       "[repair] \tRepair type: hard repair\n"
                       "[repair] \tGPU location: HW FBPA %u, subpartition %u\n"
                       "[repair] \tHBM location: site %u, stack %u, channel %u, bank %u, row %u\n"
                       "[repair] \tusing fuse: %u\n"
                       "[repair] \tInterface: IEEE1500\n"
                       "[repair] \tskip_hbm_fuse_repair_check: %s (ignored)\n"
                       "[repair] \tpseudo_hard_repair: %s\n",
                       row.hwFbio.Number(), row.subpartition.Number(),
                       hbmSite.Number(), row.rank.Number(), hbmChannel.Number(),
                       row.bank.Number(), row.row,
                       spareRowFuse.Number(),
                       Utility::ToString(settings.modifiers.skipHbmFuseRepairCheck).c_str(),
                       Utility::ToString(settings.modifiers.pseudoHardRepair).c_str());
            }

            // NOTE: SK Hynix supports writing all of the rows to be repaired in
            // one go by doing:
            // 1) WIR INSTR and MODE set.
            // 2) Write WDR for each repair with HARD_REPAIR WDR start bit set to 1.
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

                // End the row repair input
                hardRepairData.UnsetStartBit();
                ROW_REPAIR_FIRST_RC_SKIP(pGpuIntf->WdrWriteRaw(hbmSite, hbmChannel,
                                                               hardRepairData));

                pGpuIntf->SleepMs(500); // sleep after final repair in sequence

                // TODO(aperiwal):
                // From the HBM2 repair sequence.
                // Need a site reset. Also says we need an "HBM
                // init (already there in VBIOS)" before it takes affect. Does
                // that mean a GPU init as well, or is a site reset enough?
                ROW_REPAIR_FIRST_RC_SKIP(pGpuIntf->ResetSite(hbmSite));

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
// SoftRepairWriteData

Hbm::SkHynixHbm2e::SoftRepairWriteData::SoftRepairWriteData
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

    data |= 1 << 22;                // bit[   22]: signals start of soft repair

    AssertWithinBitWidth(rowFuse.Number(), 2, "fuse set");
    data |= rowFuse.Number() << 20; // bit[21:20]: fuse set

    AssertWithinBitWidth(stack.Number(), 1, "stack ID");
    data |= stack.Number() << 19;   // bit[   19]: stack ID

    AssertWithinBitWidth(bank.Number(), 4, "bank address");
    data |= bank.Number() << 15;    // bit[18:14]: row bank address

    AssertWithinBitWidth(row, 14, "row address");
    // NOTE: The last bit of the 15-bit row address is ignored. Only
    // write the upper 14-bits.
    data |= (row >> 1) << 1;        // bit[14: 1]: row address [14:1]

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

void Hbm::SkHynixHbm2e::SoftRepairWriteData::SetStartBit()
{
    m_Data.back() |= 1 << 22;    // bit[22]: signals start of soft repair
}

void Hbm::SkHynixHbm2e::SoftRepairWriteData::UnsetStartBit()
{
    m_Data.back() &= ~(1 << 22); // bit[22]: signals start of soft repair
}

/******************************************************************************/
// HardRepairData

Hbm::SkHynixHbm2e::HardRepairData::HardRepairData
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

    data |= 1 << 24;                // bit[   24]: signals start of hard repair

    AssertWithinBitWidth(rowFuse.Number(), 2, "fuse set");
    data |= rowFuse.Number() << 21; // bit[22:21]: fuse set

    AssertWithinBitWidth(stack.Number(), 1, "stack ID");
    data |= stack.Number() << 20;   // bit[   20]: stack ID

    AssertWithinBitWidth(bank.Number(), 4, "bank address");
    data |= bank.Number() << 16;    // bit[19:16]: row bank address

    AssertWithinBitWidth(row, 15, "row address");
    // NOTE: 15-bit addr is used, but rowAddr[0] is not used as they are
    // repair circuit structural characteristics
    data |= row << 1;               // bit[15: 1]: row address [14:0]

    // Bit 22 is reserved and must be set to 0
    MASSERT((data & (1 << 22)) == 0);
    // Bit 0 is reserved for the repair complete signal
    MASSERT((data & (1 << 0)) == 0);
}

void Hbm::SkHynixHbm2e::HardRepairData::SetStartBit()
{
    m_Data.back() |= 1 << 24; // bit[24]: signals start of hard repair
}

void Hbm::SkHynixHbm2e::HardRepairData::UnsetStartBit()
{
    m_Data.back() &= ~(1 << 24); // bit[24]: signals start of hard repair
}

bool Hbm::SkHynixHbm2e::HardRepairData::RepairSucceeded() const
{
    // bit[0]: signal that repair is successful if set to 1, o/w repair failed
    return m_Data.back() & 0x1;
}

/******************************************************************************/
// PprInfoReadData

RC Hbm::SkHynixHbm2e::PprInfoReadData::GetFirstAvailableFuse
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

Hbm::SkHynixHbm2e::RowBurnedFuseMask
Hbm::SkHynixHbm2e::PprInfoReadData::GetRowBurnedFuseMask
(
    Stack stack,
    Bank bank
) const
{
    // bits[127:64]: stack 1
    // bits[63:  0]: stack 0
    //
    // 4 bits per bank, i.e
    //   bits[3:0]: stack0, bank0,
    //   bits[7:4]: stack0, bank1,
    //   ...
    //   bits[67:64]: stack1, bank0,
    //   ...
    static constexpr UINT32 bankBitWidth  = 4;
    static constexpr UINT32 stackBitWidth = 64;
    static constexpr UINT32 bankMask      = (1 << bankBitWidth) - 1;

    MASSERT(m_Data.size() == 4); // PPR WDR width is 128
    MASSERT(stack.Number() < 2); // only 2 stacks supported

    const UINT32 wdrBitOffset  = (bank.Number() * bankBitWidth) +
                                 (stack.Number() * stackBitWidth);
    const UINT32 dataIndex     = static_cast<UINT32>(wdrBitOffset / 32);
    const UINT32 dataBitOffset = static_cast<UINT32>(wdrBitOffset % 32);
    const UINT32 fuseMaskData  = (m_Data[dataIndex] >> dataBitOffset) & bankMask;

    return RowBurnedFuseMask(static_cast<UINT08>(fuseMaskData));
}

bool Hbm::SkHynixHbm2e::PprInfoReadData::IsFuseBurned
(
    Stack stack,
    Bank bank,
    RowFuse rowFuse
) const
{
    return GetRowBurnedFuseMask(stack, bank).IsFuseBurned(rowFuse);
}

UINT32 Hbm::SkHynixHbm2e::PprInfoReadData::TotalBurnedFuses() const
{
    UINT32 totalBurnedFuses = 0;

    for (const UINT32 data : m_Data)
    {
        totalBurnedFuses += Utility::CountBits(data);
    }

    return totalBurnedFuses;
}
