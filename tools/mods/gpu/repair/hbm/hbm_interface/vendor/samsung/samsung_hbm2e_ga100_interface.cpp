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

#include "gpu/repair/hbm/hbm_interface/vendor/samsung/samsung_hbm2e_ga100_interface.h"

#include "core/include/utility.h"
#include "gpu/repair/repair_util.h"

using namespace Memory;
using namespace Memory::Hbm;
using namespace Repair;

using WirF = Wir::Flags;
using WirT = Wir::Type;
using RegT = Wir::RegType;

RC SamsungHbm2eGA100::RowRepair
(
    const Settings& settings,
    const SiteRowErrorMap& rowErrors,
    RepairType repairType
) const
{
    RC rc;

    if (!settings.modifiers.skipHbmFuseRepairCheck)
    {
        // ENABLE_FUSE_SCAN WIR is used to check the HBM fuses for
        // repairs. Don't do the pin setup if ENABLE_FUSE_SCAN is not going to
        // run.
        CHECK_RC(DoEnableFuseScanIoPinSetup());
    }

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

RC SamsungHbm2eGA100::PrintRepairedRows() const
{
    MASSERT(IsSetup());
    RC rc;

    CHECK_RC(DoEnableFuseScanIoPinSetup());

    Printf(Tee::PriNormal, "== HBM Repaired Rows Summary\n");

    UINT32 totalRepairedRows = 0;
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
            Printf(Tee::PriDebug, "Stack %u row repairs:\n", stack.Number());

            for (HbmChannel channel(0); channel.Number() < s_MaxNumChannels; ++channel)
            {
                Printf(Tee::PriDebug, "Channel %u row repairs:\n", channel.Number());
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
                for (Bank bank(0); bank.Number() < s_NumBanksPerPseudoChannel; ++bank)
                {
                    Printf(Tee::PriDebug, "Bank %u row repairs:\n", bank.Number());
                    UINT32 numSpareRows = 0;
                    CHECK_RC(GetNumSpareRowsAvailable(site, channel, stack, bank, &numSpareRows));

                    const UINT32 numUsedRows = s_NumSpareRowsPerBank - numSpareRows;
                    if (numUsedRows > 0)
                    {
                        channelDetails
                            += Utility::StrPrintf("    Bank %u has %u/%u repaired rows\n",
                                                  bank.Number(),
                                                  numUsedRows,
                                                  s_NumSpareRowsPerBank);
                        channelUsedRowCount += numUsedRows;
                        siteHasRepairs = true;
                    }
                }
                totalRepairedRows += channelUsedRowCount;

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
    Printf(Tee::PriNormal, "Total repaired rows: %u\n", totalRepairedRows);

    return rc;
}

RC SamsungHbm2eGA100::ForEachFbpaSetRegister(ModsGpuRegAddress address, UINT32 value) const
{
    RC rc;

    GpuHbmInterface* pGpuIntf = GpuInterface();
    MASSERT(pGpuIntf);
    GpuSubdevice* pSubdev = pGpuIntf->GetGpuSubdevice();
    MASSERT(pSubdev);
    FrameBuffer* pFb = pSubdev->GetFB();
    MASSERT(pFb);
    FloorsweepImpl* pFs = pSubdev->GetFsImpl();
    MASSERT(pFs);
    RegHal& regs = pSubdev->Regs();

    const UINT32 fbpFsMask = pFs->FbpMask();
    const UINT32 fbpasPerFbp = pFb->GetFbiosPerFbp();
    const UINT32 numHbmSites = pSubdev->GetNumHbmSites();

    for (UINT32 hbmSiteId = 0; hbmSiteId < numHbmSites; ++hbmSiteId)
    {
        bool hbmSiteActive = false;
        CHECK_RC(pFb->IsHbmSiteAvailable(hbmSiteId, &hbmSiteActive));
        if (!hbmSiteActive) { continue; }

        UINT32 fbpNum0 = 0;
        UINT32 fbpNum1 = 0;
        CHECK_RC(pSubdev->GetHBMSiteFbps(hbmSiteId, &fbpNum0, &fbpNum1));

        // For each FBP on HBM site
        for (UINT32 fbp : { fbpNum0, fbpNum1 })
        {
            const bool fbpActive = (fbpFsMask & (1U << fbp)) != 0;
            if (!fbpActive) { continue; }

            // For each FBPA on FBP
            // NOTE: If the FBP is active, all its FBPAs must be active.
            for (UINT32 n = 0; n < fbpasPerFbp; ++n)
            {
                const UINT32 fbpa = fbp * fbpasPerFbp + n;
                regs.Write32(address, fbpa, value);
            }
        }
    }

    return rc;
}

RC SamsungHbm2eGA100::DoEnableFuseScanIoPinSetup() const
{
    RC rc;

    GpuHbmInterface* pGpuIntf = GpuInterface();
    MASSERT(pGpuIntf);

    RegHal& regs = pGpuIntf->GetGpuSubdevice()->Regs();

    if (!regs.Test32(MODS_FBIO_BIST_PRIV_LEVEL_MASK_WRITE_PROTECTION_ALL_LEVELS_ENABLED))
    {
        Printf(Tee::PriError, "Missing write permission on register: LW_FBIO_BIST_*\n");
        return RC::PRIV_LEVEL_VIOLATION;
    }

    // Global setup though broadcast registers (0x009a****)
    //
    regs.Write32(MODS_FBIO_BIST_CONFIG, 0x0000001C);

    regs.Write32(MODS_FBIO_BIST_CONFIGREG, 0x20234101);

    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 0, 0x00000000);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 1, 0x00000000);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 2, 0x00000000);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 3, 0x00000000);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 4, 0x00000000);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 5, 0x00000000);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 6, 0x00000000);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 7, 0x00000000);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 8, 0x00000000);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 9, 0x00000000);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 10, 0x55555555);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 11, 0x55555555);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 12, 0x55555555);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 13, 0x55555555);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 14, 0x55555555);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 15, 0x55555555);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 16, 0x00000000);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 17, 0x00000000);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 18, 0xFFFFFFFC);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 19, 0xFFFFFFFF);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 20, 0x00000000);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 21, 0x00000000);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 22, 0x00000000);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 23, 0x00000000);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 24, 0x00000000);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 25, 0x00000000);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 26, 0x00000000);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 27, 0x00000000);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 28, 0x55555555);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 29, 0x55555555);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 30, 0x55555555);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 31, 0x55555555);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 32, 0x55555555);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 33, 0x55555555);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 34, 0x00000000);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 35, 0x00000000);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 36, 0x00000000);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 37, 0x00000000);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 38, 0x00000000);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 39, 0x00000000);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 40, 0x00000000);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 41, 0x00000000);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 42, 0x00000000);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 43, 0x00000000);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 44, 0x00000000);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 45, 0x00000000);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 46, 0x22222222);
    regs.Write32(MODS_FBIO_BIST_PBUF1_x, 47, 0x22222222);

    regs.Write32(MODS_FBIO_BIST_LANE_x_CONFIG, 0, 0x00000008);
    regs.Write32(MODS_FBIO_BIST_LANE_x_CONFIG, 1, 0x00000008);
    regs.Write32(MODS_FBIO_BIST_LANE_x_CONFIG, 2, 0x00000008);
    regs.Write32(MODS_FBIO_BIST_LANE_x_CONFIG, 3, 0x00000008);
    regs.Write32(MODS_FBIO_BIST_LANE_x_CONFIG, 4, 0x00000008);
    regs.Write32(MODS_FBIO_BIST_LANE_x_CONFIG, 5, 0x00000008);
    regs.Write32(MODS_FBIO_BIST_LANE_x_CONFIG, 6, 0x00000008);
    regs.Write32(MODS_FBIO_BIST_LANE_x_CONFIG, 7, 0x00000008);
    regs.Write32(MODS_FBIO_BIST_LANE_x_CONFIG, 8, 0x00000008);
    regs.Write32(MODS_FBIO_BIST_LANE_x_CONFIG, 9, 0x00000008);
    regs.Write32(MODS_FBIO_BIST_LANE_x_CONFIG, 10, 0x00000008);
    regs.Write32(MODS_FBIO_BIST_LANE_x_CONFIG, 11, 0x00000008);
    regs.Write32(MODS_FBIO_BIST_LANE_x_CONFIG, 12, 0x00000001);
    regs.Write32(MODS_FBIO_BIST_LANE_x_CONFIG, 13, 0x00000001);
    regs.Write32(MODS_FBIO_BIST_LANE_x_CONFIG, 14, 0x00000001);
    regs.Write32(MODS_FBIO_BIST_LANE_x_CONFIG, 15, 0x00000001);

    regs.Write32(MODS_FBIO_BIST_LANE_x_CONFIG, 0x00000000);

    regs.Write32(MODS_FBIO_BIST_PRBS_HBM_0, 0x3456789A);
    regs.Write32(MODS_FBIO_BIST_PRBS_HBM_1, 0x00000000);

    regs.Write32(MODS_FBIO_BIST_PROTOCOLGEN_0, 0x142850C0);
    regs.Write32(MODS_FBIO_BIST_PROTOCOLGEN_1, 0xA142850A);
    regs.Write32(MODS_FBIO_BIST_PROTOCOLGEN_2, 0x0A142850);
    regs.Write32(MODS_FBIO_BIST_PROTOCOLGEN_3, 0x00014285);

    regs.Write32(MODS_FBIO_BIST_FBCONFIG, 0x00094200);

    regs.Write32(MODS_FBIO_BIST_FBCONFIG2, 0x000009FC);

    regs.Write32(MODS_FBIO_BIST_FBREFCLKCNT, 0xFFFFFFFE);

    regs.Write32(MODS_FBIO_BIST_FBRDQSPATBUF, 0x0000AAAA);

    regs.Write32(MODS_FBIO_BIST_CONFIG, 0x0000001C);

    regs.Write32(MODS_FBIO_BIST_FBCONFIG, 0x02094200);

    regs.Write32(MODS_FBIO_BIST_PROTOCOLGEN_3, 0x00014285);

    regs.Write32(MODS_FBIO_BIST_FBCONFIG2, 0x000009FC);

    // Per FBPA setup
    //
    CHECK_RC(ForEachFbpaSetRegister(MODS_FBIO_BIST_FBCONFIG2_FBPA_x, 0x000009FD));
    CHECK_RC(ForEachFbpaSetRegister(MODS_FBIO_BIST_FBCONFIG_FBPA_x, 0x02094201));
    CHECK_RC(ForEachFbpaSetRegister(MODS_FBIO_BIST_PROTOCOLGEN_3_FBPA_x, 0x02014285));

    return rc;
}

RC SamsungHbm2eGA100::DoEnableFuseScan
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

    constexpr UINT32 fuseScanWaitTimeMs = 1;

    // Send ENABLE_FUSE_SCAN
    CHECK_RC(pGpuIntf->WirWriteRaw(*pWirEnableFuseScan, hbmSite, hbmChannel));
    CHECK_RC(pGpuIntf->ModeWriteRaw(*pWirEnableFuseScan, hbmSite));
    pGpuIntf->SleepMs(fuseScanWaitTimeMs); // Wait to write data
    CHECK_RC(pGpuIntf->WdrWriteRaw(hbmSite, hbmChannel, enableFuseScanData));

    pGpuIntf->SleepMs(fuseScanWaitTimeMs);

    // Check result
    CHECK_RC(pGpuIntf->WirWriteRaw(*pWirEnableFuseScan, hbmSite, hbmChannel));
    CHECK_RC(pGpuIntf->ModeWriteRaw(*pWirEnableFuseScan, hbmSite));
    pGpuIntf->SleepMs(fuseScanWaitTimeMs); // Wait to read data
    CHECK_RC(pGpuIntf->WdrReadRaw(*pWirEnableFuseScan, hbmSite, hbmChannel, pResult));

    return rc;
}

RC SamsungHbm2eGA100::HardRowRepair
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
                    if (!settings.modifiers.ignoreHbmFuseRepairCheckResult)
                    {
                        return RC::HBM_SPARE_ROWS_NOT_AVAILABLE;
                    }
                    else
                    {
                        Printf(Tee::PriWarn, "No spare row found. Ignoring!!!\n");
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

                    CHECK_RC(pGpuIntf->WirWrite(*pWirBypass, hbmSite, hbmChannel, WdrData({ 0 })));
                    pGpuIntf->SleepMs(1);

                    CHECK_RC(pGpuIntf->ToggleWrstN(hbmSite));
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
                        Printf(Tee::PriWarn, "Spare rows are same before "
                                "and after repair. Ignoring!!!\n");
                    }
                }
            }

            firstRepairRc = PostSingleRowRepair(settings, repairType, rowErr,
                                                hbmSite, hbmChannel, RC::OK);
        }
    }

    return firstRepairRc;
}

/* SoftRowRepair is not actually tested for now with GA100 since there were
   no GA100 boards available at the time of implementation which has
   consistency in row failure */
RC SamsungHbm2eGA100::SoftRowRepair
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
                        Printf(Tee::PriWarn, "No spare row found. Ignoring!!!\n");
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
            CHECK_RC(pGpuIntf->WdrWriteRaw(hbmSite, hbmChannel, { 0 }));
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

RC SamsungHbm2eGA100::GetNumSpareRowsAvailable
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
            // Reset the GPU before beginning the fuse scan
            CHECK_RC(pGpuIntf->ResetSite(hbmSite));

            // Grab the IEEE1500 mutex
            std::unique_ptr<PmgrMutexHolder> pPmgrMutex;
            CHECK_RC(pGpuIntf->AcquirePmgrMutex(&pPmgrMutex));

            EnableFuseScanData fuseScanRead;
            CHECK_RC(DoEnableFuseScan(hbmSite, hbmChannel, stack, pseudoChannel,
                                      bank, spareRow, &fuseScanRead));

            // BYPASS needed to reset the interface for running additional
            // instructions.
            CHECK_RC(pGpuIntf->WirWrite(*pWirBypass, hbmSite,
                                        Wir::CHANNEL_SELECT_ALL, WdrData({ 0 })));
            pGpuIntf->SleepMs(1);

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
        }
    }

    return rc;
}
