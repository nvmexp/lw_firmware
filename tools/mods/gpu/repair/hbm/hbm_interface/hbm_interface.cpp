/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "hbm_interface.h"

#include "core/include/mle_protobuf.h"
#include "gpu/hbm/hbm_mle_colw.h"

using namespace Memory;
using namespace Repair;

/******************************************************************************/
// LaneRemap

string Hbm::LaneRemap::ToString() const
{
    return Utility::StrPrintf("value 0x%04x, mask 0x%04x", value, mask);
}

/******************************************************************************/
// HbmInterface

Hbm::HbmInterface::HbmInterface
(
    const Model& hbmModel,
    std::unique_ptr<GpuHbmInterface> pGpuHbmInterface
)
    : m_HbmModel(hbmModel),
      m_pGpuHbmInterface(std::move(pGpuHbmInterface))
{}

Hbm::HbmInterface::HbmInterface(HbmInterface&& o)
    : m_HbmModel(std::move(o.m_HbmModel)),
      m_WirMap(std::move(o.m_WirMap)),
      m_IsSetup(std::move(o.m_IsSetup)),
      m_pGpuHbmInterface(std::move(o.m_pGpuHbmInterface))
{}

Hbm::HbmInterface& Hbm::HbmInterface::operator=(HbmInterface&& o)
{
    if (this != &o)
    {
        m_HbmModel = std::move(o.m_HbmModel);
        m_WirMap = std::move(o.m_WirMap);
        m_IsSetup = std::move(o.m_IsSetup);
        m_pGpuHbmInterface = std::move(o.m_pGpuHbmInterface);
    }

    return *this;
}

RC Hbm::HbmInterface::Setup()
{
    m_IsSetup = true;
    return RC::OK;
}

RC Hbm::HbmInterface::PrintHbmDeviceInfo() const
{
    RC rc;

    string ecid;
    CHECK_RC(GpuInterface()->GetGpuSubdevice()->GetEcid(&ecid));

    const vector<HBMSite>& sites = GpuInterface()->GetHbmDevice()->GetHbmSites();
    for (Site site(0); site.Number() < sites.size(); site++)
    {
        const HBMSite& siteObj = sites[site.Number()];
        if (siteObj.IsSiteActive())
        {
            HbmDeviceIdData devIdData;
            CHECK_RC(GetHbmSiteDeviceInfo(site, &devIdData));

            Printf(Tee::PriNormal, "HBM Site %u:\n", site.Number());
            devIdData.DumpDevIdData();

            Mle::Print(Mle::Entry::hbm_site)
                .ecid(ecid)
                .site_id(site.Number())
                .gen2(devIdData.GetGen2Supported())
                .ecc(devIdData.GetEccSupported())
                .density_per_chan(devIdData.GetDensityPerChanGb())
                .mfg_id(static_cast<UINT08>(devIdData.GetVendorId()))
                .mfg_location(devIdData.GetManufacturingLoc())
                .mfg_year(devIdData.GetManufacturingYear())
                .mfg_week(devIdData.GetManufacturingWeek())
                .serial_num(devIdData.GetSerialNumber())
                .addr_mode(static_cast<Mle::HbmSite::AddressMode>(devIdData.GetAddressModeVal()))
                .channels_avail(devIdData.GetChannelsAvailVal())
                .stack_height(devIdData.GetStackHeight())
                .model_part_num(devIdData.GetModelPartNumber());
        }
    }

    return rc;
}

RC Hbm::HbmInterface::PrintRepairedLanes(bool readHbmLanes) const
{
    RC rc;
    vector<FbpaLane> lanes;

    if (readHbmLanes)
    {
        CHECK_RC(GetRepairedLanesFromHbm(&lanes));
    }
    else
    {
        CHECK_RC(GetRepairedLanesFromGpu(&lanes));
    }

    string s = Utility::StrPrintf("== HBM Repaired Lanes Summary - sourced from %s:\n",
                                  (readHbmLanes ? "HBM fuses" : "GPU"));

    string ecid;
    CHECK_RC(m_pGpuHbmInterface->GetGpuSubdevice()->GetEcid(&ecid));

    for (const FbpaLane& lane : lanes)
    {
        s += lane.ToString();
        s += '\n';

        Mle::Print(Mle::Entry::hbm_repaired_lane)
            .ecid(ecid)
            .name(lane.ToString())
            .hw_fbpa(lane.hwFbpa.Number())
            .fbpa_lane(lane.laneBit)
            .type(Mle::ToLane(lane.laneType))
            .repair_source(readHbmLanes ? Mle::HbmRepair::hbm_fuses : Mle::HbmRepair::gpu);
    }

    Printf(Tee::PriNormal, "%sTotal lane repair: %zu\n\n", s.c_str(), lanes.size());

    return rc;
}

RC Hbm::HbmInterface::AddWirs(const vector<Wir>& wirs)
{
    for (const Wir& wir : wirs)
    {
        if (wir.IsSupported(m_HbmModel))
        {
            const auto iter = m_WirMap.find(wir.GetType());
            if (iter != m_WirMap.end())
            {
                // WIR has already been added, implying confliciting WIR definition
                Printf(Tee::PriError, "Conflicting WIR definition for HBM model '%s':"
                       "\t%s\n\t%s\n", m_HbmModel.ToString().c_str(),
                       iter->second->ToString().c_str(), wir.FullToString().c_str());
                return RC::SOFTWARE_ERROR;
            }

            m_WirMap[wir.GetType()] = &wir;
        }
    }

    return RC::OK;
}

RC Hbm::HbmInterface::LookupWir(Hbm::Wir::Type wirType, const Wir** ppWir) const
{
    MASSERT(ppWir);

    const auto wirIter = m_WirMap.find(wirType);
    if (wirIter == m_WirMap.end())
    {
        Printf(Tee::PriError, "Unknown WIR: %u", static_cast<UINT32>(wirType));
        MASSERT(!"Unable to find WIR in WIR map");
        return RC::SOFTWARE_ERROR;
    }
    *ppWir = wirIter->second;

    return RC::OK;
}

RC Hbm::HbmInterface::PreSingleLaneRepair
(
    Hbm::Site hbmSite,
    Hbm::Channel hbmChannel,
    const FbpaLane& lane
) const
{
    const vector<HBMSite>& hbmSites = GpuInterface()->GetHbmDevice()->GetHbmSites();

    if (!hbmSites[hbmSite.Number()].IsSiteActive())
    {
        Printf(Tee::PriError, "Cannot repair lane %s because HBM site %u is inactive\n",
               lane.ToString().c_str(), hbmSite.Number());
        return RC::HBM_REPAIR_IMPOSSIBLE;
    }

    Printf(Tee::PriNormal, "\n[repair] Repairing lane %s\n",
           lane.ToString().c_str());

    // TODO(stewarts): check if lane values make sense

    return RC::OK;
}

RC Hbm::HbmInterface::PostSingleLaneRepair
(
    const Settings& settings,
    RepairType repairType,
    const FbpaLane& lane,
    const HbmLane& hbmLane,
    const LaneRemap& remap,
    const WdrData& originalHbmLaneRemapData,
    const WdrData& hbmLaneRemapData,
    RC repairStatus
) const
{
    RC rc;

    if (repairStatus != RC::OK)
    {
        Printf(Tee::PriError, "[repair] FAILED with RC %d: %s\n",
               repairStatus.Get(), repairStatus.Message());
    }
    else
    {
        Printf(Tee::PriNormal, "[repair] SUCCESS: lane %s\n",
               lane.ToString().c_str());
    }

    UINT32 hbmDword = 0;
    UINT32 hbmByte = 0;
    CHECK_RC(GpuInterface()->GetHbmDwordAndByte(hbmLane, &hbmDword, &hbmByte));

    string ecid;
    CHECK_RC(m_pGpuHbmInterface->GetGpuSubdevice()->GetEcid(&ecid));
    Mle::Print(Mle::Entry::hbm_lane_repair_attempt)
        .status(repairStatus)
        .ecid(ecid)
        .name(lane.ToString())
        .type(Mle::ToLane(lane.laneType))
        .hw_fbpa(lane.hwFbpa.Number())
        .subp(GpuInterface()->GetSubpartition(lane).Number())
        .fbpa_lane(lane.laneBit)
        .site(hbmLane.site.Number())
        .channel(hbmLane.channel.Number())
        .dword(hbmDword)
        .byte(hbmByte)
        .remap_val(remap.value)
        .remap_mask(remap.mask)
        .pre_repair_ieee_data(originalHbmLaneRemapData.GetRaw())
        .post_repair_ieee_data(hbmLaneRemapData.GetRaw())
        .interface(Mle::HbmRepair::ieee1500)
        .hard_repair(repairType == RepairType::HARD)
        .skip_verif(settings.modifiers.skipHbmFuseRepairCheck)
        .pseudo_repair(settings.modifiers.pseudoHardRepair);

    return RC::OK;
}

RC Hbm::HbmInterface::PreSingleRowRepair
(
    Hbm::Site hbmSite,
    Hbm::Channel hbmChannel,
    const RowError& rowError
) const
{
    const vector<HBMSite>& hbmSites = GpuInterface()->GetHbmDevice()->GetHbmSites();
    const Row& row = rowError.row;

    // Set the origin test for recording purposes
    //
    // NOTE: If there was to be a failure, the test number that would be
    // associated with the RC would be the test ID set here.
    ErrorMap::SetTest(rowError.originTestId);

    // Check site in range
    //
    if (hbmSites.size() < hbmSite.Number())
    {
        Printf(Tee::PriError, "Invalid site number: num(%u), total_num_sites(%u)\n",
               hbmSite.Number(), static_cast<UINT32>(hbmSites.size()));
        MASSERT(!"Invalid site number");
        return RC::SOFTWARE_ERROR;
    }

    // Check site is active
    //
    const HBMSite& site = hbmSites[hbmSite.Number()];
    if (!site.IsSiteActive())
    {
        Printf(Tee::PriError, "Cannot repair row %s because site %u is inactive\n",
               row.rowName.c_str(), hbmSite.Number());
        return RC::HBM_REPAIR_IMPOSSIBLE;
    }

    // Check stack/rank in range
    //
    const HbmDeviceIdData siteInfo = site.GetSiteInfo();
    const UINT32 stackHeight = siteInfo.GetStackHeight();
    if (row.rank.Number() >= stackHeight)
    {
        Printf(Tee::PriError, "Invalid StackId (%u) for site %u's StackHeight (%u)\n",
               row.rank.Number(), hbmSite.Number(), stackHeight);
        return RC::HBM_REPAIR_IMPOSSIBLE;
    }

    // Check channel is active
    //
    const string channelStr = siteInfo.GetChannelsAvail();

    bool repairChannelActive = false;
    for (const char channelLetter : Utility::ToLowerCase(channelStr))
    {
        const Hbm::Channel activeChan(channelLetter - 'a');
        if (activeChan == hbmChannel)
        {
            repairChannelActive = true;
            break;
        }
    }

    if (!repairChannelActive)
    {
        Printf(Tee::PriError, "Cannot repair row %s because channel %u on site %u is inactive\n",
               row.rowName.c_str(), hbmChannel.Number(), hbmSite.Number());
        return RC::HBM_REPAIR_IMPOSSIBLE;
    }

    // Print repair header
    //
    Printf(Tee::PriNormal, "\n[repair] Reparing row %s\n",
           row.rowName.c_str());

    return RC::OK;
}

RC Hbm::HbmInterface::PostSingleRowRepair
(
    const Settings& settings,
    RepairType repairType,
    const RowError& rowError,
    Hbm::Site hbmSite,
    Hbm::Channel hbmChannel,
    RC repairStatus
) const
{
    RC rc;

    const Row& row = rowError.row;

    if (repairStatus != RC::OK)
    {
        Printf(Tee::PriError, "[repair] FAILED with RC %d: %s\n",
               repairStatus.Get(), repairStatus.Message());
    }
    else
    {
        Printf(Tee::PriNormal, "[repair] SUCCESS: row %s\n",
               row.rowName.c_str());

        // Report successfully repaired rows to JS layer
        JavaScriptPtr pJs;
        JsArray Args(1);
        CHECK_RC(pJs->ToJsval(row.rowName, &Args[0]));
        CHECK_RC(pJs->CallMethod(pJs->GetGlobalObject(), "AddRepairedRow", Args, 0));
    }

    // Set metadata
    //
    // NOTE: Because the current test is set to the origin test, this repair
    // will be associated with the origin test.
    string ecid;
    CHECK_RC(m_pGpuHbmInterface->GetGpuSubdevice()->GetEcid(&ecid));
    Mle::Print(Mle::Entry::hbm_row_repair_attempt)
        .status(repairStatus)
        .ecid(ecid)
        .name(row.rowName)
        .stack(row.rank.Number())
        .bank(row.bank.Number())
        .row(row.row)
        .hw_fbpa(row.hwFbio.Number())
        .subp(row.subpartition.Number())
        .site(hbmSite.Number())
        .channel(hbmChannel.Number())
        .interface(Mle::HbmRepair::ieee1500)
        .hard_repair(repairType == RepairType::HARD)
        .skip_verif(settings.modifiers.skipHbmFuseRepairCheck)
        .pseudo_repair(settings.modifiers.pseudoHardRepair);

    // Unset the row's origin test ID
    ErrorMap::SetTest(s_UnknownTestId);

    return rc;
}

RC Hbm::HbmInterface::RowRepair
(
    const Settings& settings,
    const vector<RowError>& rowErrors,
    RepairType repairType
) const
{
    RC rc;

    // Sort the row errors by site
    SiteRowErrorMap siteRowErrorMap;
    for (const RowError& rowErr : rowErrors)
    {
        Site hbmSite;
        HbmChannel hbmChannel;
        CHECK_RC(GpuInterface()->HwFbpaSubpToHbmSiteChannel(rowErr.row.hwFbio,
                                                            rowErr.row.subpartition,
                                                            &hbmSite,
                                                            &hbmChannel));
        siteRowErrorMap[hbmSite].push_back({rowErr, hbmChannel});
    }

    CHECK_RC(RowRepair(settings, siteRowErrorMap, repairType));
    return rc;
}

RC Hbm::HbmInterface::RunMBist() const
{
    return RC::UNSUPPORTED_FUNCTION;
}

bool Hbm::CmpRowErrorInfoByRow(const RowErrorInfo& a, const RowErrorInfo& b)
{
    return Repair::CmpRowErrorByRow(a.error, b.error);
}
