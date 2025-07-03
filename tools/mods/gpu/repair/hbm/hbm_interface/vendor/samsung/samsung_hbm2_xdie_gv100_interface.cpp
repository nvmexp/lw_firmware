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

#include "gpu/repair/hbm/hbm_interface/vendor/samsung/samsung_hbm2_xdie_gv100_interface.h"

#include "core/include/utility.h"
#include "gpu/repair/repair_util.h"

using namespace Memory;
using namespace Memory::Hbm;
using namespace Repair;

using WirF = Wir::Flags;
using WirT = Wir::Type;
using RegT = Wir::RegType;

RC SamsungHbm2XDieGV100::RowRepair
(
    const Settings& settings,
    const SiteRowErrorMap& rowErrors,
    RepairType repairType
) const
{
    RC rc;

    switch (repairType)
    {
        case RepairType::HARD:
            return HardRowRepair(settings, rowErrors);

        case RepairType::SOFT:
            return SoftRowRepair(settings, rowErrors);

        default:
            Printf(Tee::PriError, "Row repair: unsupported repair type");
            MASSERT(!"Row repair: unsupported repair type");
            return RC::SOFTWARE_ERROR;
    }
}

RC SamsungHbm2XDieGV100::PrintRepairedRows() const
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

RC SamsungHbm2XDieGV100::DoEnableFuseScanWar(const Site& hbmSite) const
{
    RC rc;

    GpuHbmInterface* pGpuIntf = GpuInterface();
    MASSERT(pGpuIntf);

    const Wir* pWirTmrs = nullptr;
    CHECK_RC(LookupWir(Wir::TEST_MODE_REGISTER_SET, &pWirTmrs));
    MASSERT(pWirTmrs);

    const Wir* pWirMdrs = nullptr;
    CHECK_RC(LookupWir(Wir::MODE_REGISTER_DUMP_SET, &pWirMdrs));
    MASSERT(pWirMdrs);

    // Grab the IEEE1500 mutex
    std::unique_ptr<PmgrMutexHolder> pPmgrMutex;
    CHECK_RC(pGpuIntf->AcquirePmgrMutex(&pPmgrMutex));

    // 3 TMRS Shifts
    //
    CHECK_RC(pGpuIntf->WirWriteRaw(*pWirTmrs, hbmSite, Wir::CHANNEL_SELECT_ALL));
    CHECK_RC(pGpuIntf->ModeWriteRaw(*pWirTmrs, hbmSite));
    pGpuIntf->SleepMs(1000);
    CHECK_RC(pGpuIntf->WdrWriteRaw(hbmSite, Wir::CHANNEL_SELECT_ALL, {0x02002001}));
    pGpuIntf->SleepMs(1000);
    CHECK_RC(pGpuIntf->WdrWriteRaw(hbmSite, Wir::CHANNEL_SELECT_ALL, {0x00000012}));
    pGpuIntf->SleepMs(1000);

    CHECK_RC(pGpuIntf->WirWriteRaw(*pWirTmrs, hbmSite, Wir::CHANNEL_SELECT_ALL));
    CHECK_RC(pGpuIntf->ModeWriteRaw(*pWirTmrs, hbmSite));
    pGpuIntf->SleepMs(1000);
    CHECK_RC(pGpuIntf->WdrWriteRaw(hbmSite, Wir::CHANNEL_SELECT_ALL, {0x08080010}));
    pGpuIntf->SleepMs(1000);
    CHECK_RC(pGpuIntf->WdrWriteRaw(hbmSite, Wir::CHANNEL_SELECT_ALL, {0x00000010}));
    pGpuIntf->SleepMs(1000);

    CHECK_RC(pGpuIntf->WirWriteRaw(*pWirTmrs, hbmSite, Wir::CHANNEL_SELECT_ALL));
    CHECK_RC(pGpuIntf->ModeWriteRaw(*pWirTmrs, hbmSite));
    pGpuIntf->SleepMs(1000);
    CHECK_RC(pGpuIntf->WdrWriteRaw(hbmSite, Wir::CHANNEL_SELECT_ALL, {0x08020001}));
    pGpuIntf->SleepMs(1000);
    CHECK_RC(pGpuIntf->WdrWriteRaw(hbmSite, Wir::CHANNEL_SELECT_ALL, {0x00000010}));
    pGpuIntf->SleepMs(1000);

    // Set MDRS to all 0s
    //
    CHECK_RC(pGpuIntf->WirWriteRaw(*pWirMdrs, hbmSite, Wir::CHANNEL_SELECT_ALL));
    CHECK_RC(pGpuIntf->ModeWriteRaw(*pWirMdrs, hbmSite));
    pGpuIntf->SleepMs(1000);
    CHECK_RC(pGpuIntf->WdrWriteRaw(hbmSite, Wir::CHANNEL_SELECT_ALL, {0}));
    pGpuIntf->SleepMs(1000);
    CHECK_RC(pGpuIntf->WdrWriteRaw(hbmSite, Wir::CHANNEL_SELECT_ALL, {0}));
    pGpuIntf->SleepMs(1000);
    CHECK_RC(pGpuIntf->WdrWriteRaw(hbmSite, Wir::CHANNEL_SELECT_ALL, {0}));
    pGpuIntf->SleepMs(1000);
    CHECK_RC(pGpuIntf->WdrWriteRaw(hbmSite, Wir::CHANNEL_SELECT_ALL, {0}));
    pGpuIntf->SleepMs(1000);

    return rc;
}

RC Hbm::SamsungHbm2XDieGV100::DoEnableFuseScan
(
    const Site& hbmSite,
    const HbmChannel& hbmChannel,
    const Stack& stack,
    const PseudoChannel& pseudoChannel,
    const Bank& bank,
    UINT32 pseudoChannelSpareRow,
    EnableFuseScanData* pResult
) const
{
    MASSERT(pResult);
    RC rc;
    MASSERT(pseudoChannelSpareRow < s_NumSpareRowsPerPseudoChannel);

    GpuHbmInterface* pGpuIntf = GpuInterface();
    MASSERT(pGpuIntf);

    const Wir* pWirEnableFuseScan = nullptr;
    CHECK_RC(LookupWir(Wir::ENABLE_FUSE_SCAN, &pWirEnableFuseScan));
    MASSERT(pWirEnableFuseScan);

    EnableFuseScanData enableFuseScanData =
        EnableFuseScanLookupTable::GetWdrData(stack, hbmChannel,
                                              pseudoChannel, bank,
                                              pseudoChannelSpareRow);

    constexpr UINT32 delayMs = 1;

    // Send ENABLE_FUSE_SCAN
    CHECK_RC(pGpuIntf->WirWriteRaw(*pWirEnableFuseScan, hbmSite, hbmChannel));
    CHECK_RC(pGpuIntf->ModeWriteRaw(*pWirEnableFuseScan, hbmSite));
    pGpuIntf->SleepMs(delayMs); // Wait to write data
    CHECK_RC(pGpuIntf->WdrWriteRaw(hbmSite, hbmChannel, enableFuseScanData));
    pGpuIntf->SleepMs(delayMs);

    // Check result
    CHECK_RC(pGpuIntf->WirWriteRaw(*pWirEnableFuseScan, hbmSite, hbmChannel));
    CHECK_RC(pGpuIntf->ModeWriteRaw(*pWirEnableFuseScan, hbmSite));
    pGpuIntf->SleepMs(delayMs); // Wait to read data
    CHECK_RC(pGpuIntf->WdrReadRaw(*pWirEnableFuseScan, hbmSite, hbmChannel, pResult));

    return rc;
}

RC SamsungHbm2XDieGV100::HardRowRepair
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
                    if (!settings.modifiers.ignoreHbmFuseRepairCheckResult)
                    {
                        return RC::HBM_SPARE_ROWS_NOT_AVAILABLE;
                    }
                    else
                    {
                        Printf(Tee::PriWarn, "Failure ignored\n");
                    }
                }
            }

            // Repair both pseudo channels
            for (PseudoChannel pseudoChannel(0);
                 pseudoChannel.Number() < s_NumPseudoChannels;
                 pseudoChannel++)
            {
                RowRepairWriteData repairData(repairType, Stack(row.rank), pseudoChannel,
                                              row.bank, row.row);

                if (!settings.modifiers.pseudoHardRepair)
                {
                    // Grab the IEEE1500 mutex
                    std::unique_ptr<PmgrMutexHolder> pPmgrMutex;
                    ROW_REPAIR_FIRST_RC_SKIP(pGpuIntf->AcquirePmgrMutex(&pPmgrMutex));

                    ROW_REPAIR_FIRST_RC_SKIP(pGpuIntf->WirWrite(*pWirHardRepair, hbmSite,
                                                                hbmChannel, repairData));

                    pGpuIntf->SleepMs(1000);

                    {
                        RegHal &regs = pGpuIntf->GetGpuSubdevice()->Regs();
                        HwFbpa masterFbpa;
                        CHECK_RC(pGpuIntf->GetHbmSiteMasterFbpa(hbmSite, &masterFbpa));

                        // Put HBM into reset mode
                        //
                        UINT32 regVal = Repair::Read32(settings, regs,
                                                       MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO,
                                                       masterFbpa.Number());

                        regs.SetField(&regVal, MODS_PFB_FBPA_MC_2_FBIO_HBM_TEST_MACRO_DRAM_WRST_RESET_ENABLED);

                        Repair::Write32(settings, regs,
                                        MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO,
                                        masterFbpa.Number(), regVal);

                        pGpuIntf->SleepMs(100);

                        // Pull HBM out of reset mode
                        //
                        regVal = Repair::Read32(settings, regs,
                                                MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO,
                                                masterFbpa.Number());

                        regs.SetField(&regVal, MODS_PFB_FBPA_MC_2_FBIO_HBM_TEST_MACRO_DRAM_WRST_RESET_DISABLED);

                        Repair::Write32(settings, regs, MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO,
                                        masterFbpa.Number(), regVal);

                        pGpuIntf->SleepUs(200);
                    }
                }
            }

            CHECK_RC(pGpuIntf->ResetSite(hbmSite));

            if (!settings.modifiers.skipHbmFuseRepairCheck)
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

                    if (!settings.modifiers.ignoreHbmFuseRepairCheckResult)
                    {
                        return RC::HBM_SPARE_ROWS_NOT_AVAILABLE;
                    }
                    else
                    {
                        Printf(Tee::PriWarn, "Failure ignored\n");
                    }
                }
            }

            firstRepairRc = PostSingleRowRepair(settings, repairType, rowErr,
                                                hbmSite, hbmChannel, RC::OK);
        }
    }

    return firstRepairRc;
}

RC SamsungHbm2XDieGV100::SoftRowRepair
(
    const Settings& settings,
    const SiteRowErrorMap& siteRowErrorMap
) const
{
    MASSERT(IsSetup());
    RC rc;

    // For ROW_REPAIR_FIRST_RC_SKIP
    constexpr RepairType repairType = Memory::RepairType::SOFT;

    GpuHbmInterface* pGpuIntf = GpuInterface();
    MASSERT(pGpuIntf);

    const Wir* pWirSoftRepair = nullptr;
    CHECK_RC(LookupWir(Wir::SOFT_REPAIR, &pWirSoftRepair));
    MASSERT(pWirSoftRepair);

    StickyRC firstRepairRc;
    for (const auto& siteRowErrors : siteRowErrorMap)
    {
        const Site hbmSite = siteRowErrors.first;

        CHECK_RC(DoEnableFuseScanWar(hbmSite));

        for (const RowErrorInfo& rowErrInfo : siteRowErrors.second)
        {
            const RowError& rowErr      = rowErrInfo.error;
            const HbmChannel hbmChannel = rowErrInfo.hbmChan;
            const Row& row              = rowErr.row;

            ROW_REPAIR_FIRST_RC_SKIP(PreSingleRowRepair(hbmSite, hbmChannel, rowErr));

            Printf(Tee::PriNormal,
                   "[repair]   Repair type: soft row repair\n"
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
                    if (!settings.modifiers.ignoreHbmFuseRepairCheckResult)
                    {
                        return RC::HBM_SPARE_ROWS_NOT_AVAILABLE;
                    }
                    else
                    {
                        Printf(Tee::PriWarn, "Failure ignored\n");
                    }
                }
            }

            // Grab the IEEE1500 mutex
            std::unique_ptr<PmgrMutexHolder> pPmgrMutex;
            CHECK_RC(pGpuIntf->AcquirePmgrMutex(&pPmgrMutex));

            // Dummy repair
            CHECK_RC(pGpuIntf->WirWriteRaw(*pWirSoftRepair, hbmSite, hbmChannel));
            CHECK_RC(pGpuIntf->ModeWriteRaw(*pWirSoftRepair, hbmSite));
            pGpuIntf->SleepMs(1000);
            CHECK_RC(pGpuIntf->WdrWriteRaw(hbmSite, hbmChannel, {0}));
            pGpuIntf->SleepMs(1000);

            // Repair both pseudo channels
            for (PseudoChannel pseudoChannel(0);
                 pseudoChannel.Number() < s_NumPseudoChannels;
                 pseudoChannel++)
            {
                RowRepairWriteData repairData(repairType, Stack(row.rank),
                                              pseudoChannel, row.bank, row.row);
                CHECK_RC(pGpuIntf->WirWriteRaw(*pWirSoftRepair, hbmSite, hbmChannel));
                CHECK_RC(pGpuIntf->ModeWriteRaw(*pWirSoftRepair, hbmSite));
                pGpuIntf->SleepMs(1000);
                CHECK_RC(pGpuIntf->WdrWriteRaw(hbmSite, hbmChannel, repairData));
                pGpuIntf->SleepMs(1000);
            }

            firstRepairRc = PostSingleRowRepair(settings, repairType, rowErr,
                                                hbmSite, hbmChannel, RC::OK);
        }
    }

    return firstRepairRc;
}

RC SamsungHbm2XDieGV100::GetNumSpareRowsAvailable
(
    const Site& hbmSite,
    const HbmChannel& hbmChannel,
    const Stack& stack,
    const Bank& bank,
    UINT32* pNumRowsAvailable
) const
{
    MASSERT(pNumRowsAvailable);
    *pNumRowsAvailable = 0;

    RC rc;

    GpuHbmInterface* pGpuIntf = GpuInterface();
    MASSERT(pGpuIntf);

    const Wir* pWirBypass = nullptr;
    CHECK_RC(LookupWir(Wir::BYPASS, &pWirBypass));
    MASSERT(pWirBypass);

    for (UINT32 spareRow = 0; spareRow < s_NumSpareRowsPerBank; ++spareRow)
    {
        for (PseudoChannel pseudoChannel(0);
             pseudoChannel.Number() < s_NumPseudoChannels;
             pseudoChannel++)
        {
            // Grab the IEEE1500 mutex
            std::unique_ptr<PmgrMutexHolder> pPmgrMutex;
            CHECK_RC(pGpuIntf->AcquirePmgrMutex(&pPmgrMutex));

            EnableFuseScanData fuseScanRead;
            CHECK_RC(DoEnableFuseScan(hbmSite, hbmChannel, stack, pseudoChannel,
                                      bank, spareRow, &fuseScanRead));

            if (fuseScanRead.IsRowAvailable())
            {
                // If both pseudo channels are available, then there is a full
                // spare row.
                if (pseudoChannel.Number() > 0)
                {
                    (*pNumRowsAvailable)++;
                }
            }
            else
            {
                // Don't bother checking another pseudo-channel if one of them
                // is fused since we fuse both pseudo channels at once.
                break;
            }

            // BYPASS needed to reset the interface for running additional
            // instructions.
            CHECK_RC(pGpuIntf->WirWrite(*pWirBypass, hbmSite, hbmChannel));
            pGpuIntf->SleepMs(1);
        }
    }

    return rc;
}
