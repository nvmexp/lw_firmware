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

#include "hbm2_interface.h"

#include "core/include/utility.h"
#include "gpu/repair/repair_util.h"

using namespace Memory;
using namespace Hbm;

using WirT = Wir::Type;
using RegT = Wir::RegType;

namespace
{
    //!
    //! \brief Vendor independent HBM2 WIRs from the specification document.
    //!
    const vector<Wir> s_Hbm2SpecWirs =
    {
        { "BYPASS", WirT::BYPASS, 0x00, RegT::Read | RegT::Write, 1,
          Repair::VendorIndepSupportedHbmModels({SpecVersion::V2, SpecVersion::V2e})
        },

        { "HBM_RESET", WirT::HBM_RESET, 0x05, RegT::Write, 1,
          Repair::VendorIndepSupportedHbmModels({SpecVersion::V2, SpecVersion::V2e})
        },

        { "SOFT_LANE_REPAIR", WirT::SOFT_LANE_REPAIR, 0x12, RegT::Read | RegT::Write, 72,
          Repair::VendorIndepSupportedHbmModels({SpecVersion::V2, SpecVersion::V2e})
        },

        { "HARD_LANE_REPAIR", WirT::HARD_LANE_REPAIR, 0x13, RegT::Read | RegT::Write, 72,
          Repair::VendorIndepSupportedHbmModels({SpecVersion::V2, SpecVersion::V2e})
        },

        { "MODE_REGISTER_DUMP_SET", WirT::MODE_REGISTER_DUMP_SET, 0x10, RegT::Read | RegT::Write, 128,
          Repair::VendorIndepSupportedHbmModels({SpecVersion::V2, SpecVersion::V2e})
        },

        { "DEVICE_ID", WirT::DEVICE_ID, 0x0e, RegT::Read, 82,
          Repair::VendorIndepSupportedHbmModels({SpecVersion::V2, SpecVersion::V2e})
        }
    };
}

/******************************************************************************/
// HbmDwordRemap

RC Hbm::Hbm2Interface::HbmDwordRemap::GetRemappedLanes
(
    vector<DwordLane>* pLanes
) const
{
    MASSERT(pLanes);
    RC rc;

    pLanes->clear();

    // NOTE: HBM operates in repair mode 2 where there is one repair possible
    // per byte pair.
    constexpr UINT32 numPossibleRepairs
        = HbmDwordRemap::dwordEncodingBitWidth / LaneRemap::bytePairEncodingBitWidth;

    for (UINT32 i = 0; i < numPossibleRepairs; ++i)
    {
        const UINT32 shift = i * LaneRemap::bytePairEncodingBitWidth;
        const UINT32 bytePairMask = Utility::Bitmask<UINT32>(LaneRemap::bytePairEncodingBitWidth);
        const UINT08 bytePair = static_cast<UINT08>((m_Value >> shift) & bytePairMask);

        constexpr UINT32 numDqLanesPerBytePair = 16;
        UINT32 dwordLaneOffset = i * numDqLanesPerBytePair;
        UINT08 repairTypeEncoding;

        constexpr UINT32 byteMask = Utility::Bitmask<UINT32>(LaneRemap::byteEncodingBitWidth);

        // Find repaired lane
        //
        if (bytePair == LaneRemap::BYTE_PAIR_NO_REMAP)
        {
            // No repair, continue
            continue;
        }
        else if ((bytePair & byteMask) == LaneRemap::BYTE_REMAP_IN_OTHER)
        {
            // Repair in 2nd byte in pair
            repairTypeEncoding = (bytePair >> LaneRemap::byteEncodingBitWidth) & byteMask;
            dwordLaneOffset += 8;
        }
        else
        {
            // Repair in 1st byte in pair
            repairTypeEncoding = bytePair & byteMask;
        }

        // Determine repair type
        //
        LaneError::RepairType repairType;

        // DM/ECC lane
        if (repairTypeEncoding == 0)
        {
            repairType = LaneError::RepairType::DM;

            // NOTE: By convention we represent the DM lane as the 0th bit of
            // the byte, so no dword lane modifications needed
        }
        // DQ (DATA) lane
        else if (repairTypeEncoding >= 1 && repairTypeEncoding <= 8)
        {
            repairType = LaneError::RepairType::DATA;
            // Specify the repaired DQ lane
            dwordLaneOffset += repairTypeEncoding - 1;
        }
        // DBI lane
        else if (repairTypeEncoding == 9)
        {
            repairType = LaneError::RepairType::DBI;

            // NOTE: By convention we represent the DBI lane as the 0th bit of
            // the byte, so no dword lane modifications needed
        }
        else
        {
            Printf(Tee::PriError, "Unsupported DWORD remap encoding value\n");
            return RC::SOFTWARE_ERROR;
        }

        // Record the repair information
        //
        pLanes->emplace_back(dwordLaneOffset, repairType);
    }

    // If no remapping, no repairs should be found, o/w there should be repairs
    MASSERT((m_Value == HbmDwordRemap::DWORD_NO_REMAP && pLanes->size() == 0)
            || (m_Value != HbmDwordRemap::DWORD_NO_REMAP && pLanes->size() > 0));

    return rc;
}

bool Hbm2Interface::HbmDwordRemap::IsNoRemap() const
{
    return m_Value == HbmDwordRemap::DWORD_NO_REMAP;
}

bool Hbm2Interface::HbmDwordRemap::IsRemappable
(
    const LaneRemap& desiredRemap
) const
{
    MASSERT(Utility::CountBits(desiredRemap.mask)
            == LaneRemap::bytePairEncodingBitWidth);

    return (m_Value & desiredRemap.mask)
        == (HbmDwordRemap::DWORD_NO_REMAP & desiredRemap.mask);
}

/******************************************************************************/
// HbmLaneRemapData

Hbm2Interface::HbmDwordRemap
Hbm::Hbm2Interface::HbmLaneRemapData::ExtractDwordRemapping(UINT32 dword) const
{
    // TODO(stewarts): duplicate implementation from: MBistImpl::GetLaneRepairData.
    // Delete old code in future CL.

    UINT16 laneData = 0;
    UINT32 bitShift = 0;
    switch (dword)
    {
        case 0:
            // DWORD0 is contained in bits 15:0
            bitShift = 0;
            laneData = m_Data[0] >> bitShift;
            break;

        case 1:
            // DWORD1 is contained in bits 31:16
            bitShift = 16;
            laneData = m_Data[0] >> bitShift;
            break;

        case 2:
            // DWORD2 is contained in bits 55:40
            bitShift = 40 - 32;
            laneData = m_Data[1] >> bitShift;
            break;

        case 3:
            // DWORD3 is contained in bits 71:56
            bitShift = 72 - 64;
            laneData = m_Data[2] << bitShift;

            bitShift = 56 - 32;
            laneData |= (m_Data[1] >> bitShift) & 0xFF;
            break;

        default:
            Printf(Tee::PriError, "Unable to extract lane remap: DWORD %u is "
                   "not valid!\n", dword);
            MASSERT(!"Unable to extract lane remap");
            return HbmDwordRemap(0);
    }

    return HbmDwordRemap(laneData);
}

void Hbm::Hbm2Interface::HbmLaneRemapData::ApplyRemap
(
    UINT32 dword,
    const LaneRemap& remap
)
{
    const UINT16& mask = remap.mask;
    const UINT16& remapVal = remap.value;

    UINT32 bitShift = 0;
    switch (dword)
    {
        case 0:
            // DWORD0 is contained in bits 15:0
            bitShift = 0;
            m_Data[0] = (m_Data[0] & ~(mask << bitShift)) | (remapVal << bitShift);
            break;

        case 1:
            // DWORD1 is contained in bits 31:16
            bitShift = 16;
            m_Data[0] = (m_Data[0] & ~(mask << bitShift)) | (remapVal << bitShift);
            break;

        case 2:
            // DWORD2 is contained in bits 55:40
            bitShift = 40 - 32;
            m_Data[1] = (m_Data[1] & ~(mask << bitShift)) | (remapVal << bitShift);
            break;

        case 3:
            // DWORD3 is contained in bits 71:56
            bitShift = 64 - 56;
            m_Data[2] = (m_Data[2] & ~(mask >> bitShift)) | (remapVal >> bitShift);

            bitShift = 56 - 32;
            m_Data[1] = (m_Data[1] & ~(mask << bitShift)) | (remapVal << bitShift);
            break;

        default:
            Printf(Tee::PriError, "Invalid lane repair: DWORD %u is not valid!\n", dword);
            break;
    }
}

/******************************************************************************/
// HardLaneRepairWriteData

void Hbm::Hbm2Interface::HardLaneRepairWriteData::ApplyRemap
(
    UINT32 dword,
    const LaneRemap& remap,
    UINT16 nibbleMask
)
{
    HbmLaneRemapData::ApplyRemap(dword,
                                 LaneRemap(static_cast<UINT16>(remap.value & nibbleMask),
                                           static_cast<UINT16>(remap.mask & nibbleMask)));
}

/******************************************************************************/
// Hbm2Interface

Hbm::Hbm2Interface::Hbm2Interface
(
    const Model& hbmModel,
    std::unique_ptr<GpuHbmInterface> pGpuHbmInterface
) : HbmInterface(hbmModel, std::move(pGpuHbmInterface)) {}

RC Hbm::Hbm2Interface::Setup()
{
    RC rc;

    CHECK_RC(HbmInterface::Setup());

    // Support checks
    //
    if (m_HbmModel.spec != Hbm::SpecVersion::V2 &&
        m_HbmModel.spec != Hbm::SpecVersion::V2e)
    {
        Printf(Tee::PriError, "Attempt to setup HBM2 interface with a non-HBM2 "
               "model:\n\t%s\n", m_HbmModel.ToString().c_str());
        MASSERT(!"Attempt to setup HBM2 interface with a non-HBM2 model");
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(AddWirs(s_Hbm2SpecWirs));

    return rc;
}

RC Hbm::Hbm2Interface::LaneRepair
(
    const Settings& settings,
    const vector<FbpaLane>& lanes,
    RepairType repairType
) const
{
    switch (repairType)
    {
        case RepairType::HARD:
            return HardLaneRepair(settings, lanes);

        case RepairType::SOFT:
            return SoftLaneRepair(settings, lanes);

        default:
            Printf(Tee::PriError, "Lane repair: unsupported repair type '%s'\n",
                   ToString(repairType).c_str());
            MASSERT(!"Lane repair: unsupported repair type");
            return RC::SOFTWARE_ERROR;
    }
}

RC Hbm::Hbm2Interface::CalcLaneRemapValue
(
    const HbmLane& lane,
    LaneRemap* pRemap
) const
{
    MASSERT(pRemap);
    RC rc;

    UINT32 laneRemapVal = 0;
    switch (lane.laneType)
    {
        case LaneError::RepairType::DM:
            laneRemapVal = 0;
            break;

        case LaneError::RepairType::DATA:
            laneRemapVal = (lane.laneBit % 8) + 1;
            break;

        case LaneError::RepairType::DBI:
            laneRemapVal = 9;
            break;

        default:
            Printf(Tee::PriError, "Lane repair is not supported for lane type: %s\n",
                   LaneError::GetLaneTypeString(lane.laneType).c_str());
            CHECK_RC(RC::UNSUPPORTED_FUNCTION);
    }

    constexpr UINT32 dwordBitWidth = 32;
    constexpr UINT32 byteBitWidth = 8;
    const UINT32 byte = (lane.laneBit % dwordBitWidth) / byteBitWidth;
    const UINT16 shift = 4 * (byte % 2);

    UINT16 mask = 0xFF;
    constexpr UINT16 repairInOtherSentinel =
        (static_cast<UINT16>(LaneRemap::BYTE_REMAP_IN_OTHER) << 4);
    UINT16 remapVal = (repairInOtherSentinel >> shift) | (laneRemapVal << shift);
    if (byte >= 2)
    {
        mask <<= 8;
        remapVal <<= 8;
    }

    *pRemap = LaneRemap(remapVal, mask);

    return rc;
}

RC Hbm::Hbm2Interface::GetHbmSiteDeviceInfo(Site hbmSite, HbmDeviceIdData* pDevIdData) const
{
    MASSERT(pDevIdData);
    RC rc;

    GpuHbmInterface* pGpuIntf = GpuInterface();
    MASSERT(pGpuIntf);

    const Wir* pWirDeviceId = nullptr;
    CHECK_RC(LookupWir(Wir::DEVICE_ID, &pWirDeviceId));
    MASSERT(pWirDeviceId);

    HwFbpa masterFbpa;
    CHECK_RC(pGpuIntf->GetHbmSiteMasterFbpa(hbmSite, &masterFbpa));

    // Grab the IEEE1500 mutex
    std::unique_ptr<PmgrMutexHolder> pPmgrMutex;
    CHECK_RC(pGpuIntf->AcquirePmgrMutex(&pPmgrMutex));

    CHECK_RC(pGpuIntf->WirWriteRaw(*pWirDeviceId, hbmSite, Wir::CHANNEL_SELECT_ALL));
    pGpuIntf->SleepMs(10);

    CHECK_RC(pGpuIntf->ModeWriteRaw(*pWirDeviceId, hbmSite));
    pGpuIntf->SleepMs(10);

    // Poll for completion
    CHECK_RC(pGpuIntf->Ieee1500WaitForIdle(hbmSite, WaitMethod::POLL));

    WdrData devIdData;
    CHECK_RC(pGpuIntf->WdrReadRaw(*pWirDeviceId, hbmSite, Wir::CHANNEL_SELECT_ALL, &devIdData));

    pDevIdData->Initialize(devIdData.GetRaw());

    return rc;
}

RC Hbm::Hbm2Interface::GetRepairedLanesFromGpu(vector<FbpaLane>* pLanes) const
{
    MASSERT(pLanes);
    RC rc;

    GpuHbmInterface* pGpuIntf = GpuInterface();
    MASSERT(pGpuIntf);

    GpuSubdevice*         pSubdev = pGpuIntf->GetGpuSubdevice();
    const FloorsweepImpl* pFs     = pSubdev->GetFsImpl();
    const FrameBuffer*    pFb     = pSubdev->GetFB();

    const UINT32     hwFbioMask    = pFs->FbioMask();
    const UINT32     maxFbioCount  = pFb->GetMaxFbioCount();
    const UINT32     numSubp       = pFb->GetSubpartitions();
    constexpr UINT32 dwordBitWidth = 32;
    const UINT32     dwordsPerSubp = pFb->GetSubpartitionBusWidth() / dwordBitWidth;

    for (HwFbio fbio(0); fbio.Number() < maxFbioCount; ++fbio)
    {
        if (!(hwFbioMask & (1 << fbio.Number()))) { continue; }

        for (FbioSubp subp(0); subp.Number() < numSubp; ++subp)
        {
            for (UINT32 fbpaDword = 0; fbpaDword < dwordsPerSubp; ++fbpaDword)
            {
                const HwFbpa fbpa(pFb->HwFbioToHwFbpa(fbio.Number()));
                CHECK_RC(ExtractLaneRepairFromLinkRepairReg(fbpa,
                                                            pGpuIntf->FbioSubpToFbpaSubp(subp),
                                                            fbpaDword, pLanes));
            }
        }
    }

    return rc;
}

RC Hbm::Hbm2Interface::GetRepairedLanesFromHbm(vector<FbpaLane>* pLanes) const
{
    MASSERT(pLanes);
    RC rc;

    const vector<HBMSite>& sites = GetHbmDevice()->GetHbmSites();
    for (Site site = Site(0); site.Number() < sites.size(); ++site)
    {
        const HBMSite& siteObj = sites[site.Number()];
        if (!siteObj.IsSiteActive()) { continue; }

        const string availableChannels = siteObj.GetSiteInfo().GetChannelsAvail();
        for (HbmChannel hbmChan(0); hbmChan.Number() < s_MaxNumChannels; ++hbmChan)
        {
            const bool isChannelAvailable
                = (availableChannels.find('a' + hbmChan.Number()) != string::npos);
            if (!isChannelAvailable) { continue; }

            CHECK_RC(GetRepairedLanesFromHbm(site, hbmChan, pLanes));
        }
    }

    return rc;
}

RC Hbm::Hbm2Interface::GetRepairedLanesFromHbm
(
    const Site& hbmSite,
    const HbmChannel& hbmChannel,
    vector<FbpaLane>* pLanes
) const
{
    MASSERT(IsSetup());
    MASSERT(pLanes);
    RC rc;

    GpuHbmInterface* pGpuIntf = GpuInterface();
    MASSERT(pGpuIntf);

    const Wir* pWirSoftLaneRepair = nullptr;
    CHECK_RC(LookupWir(Wir::SOFT_LANE_REPAIR, &pWirSoftLaneRepair));
    MASSERT(pWirSoftLaneRepair);

    HbmLaneRemapData remapData;
    {
        // Grab the IEEE1500 mutex
        std::unique_ptr<PmgrMutexHolder> pPmgrMutex;
        CHECK_RC(pGpuIntf->AcquirePmgrMutex(&pPmgrMutex));

        CHECK_RC(pGpuIntf->WirRead(*pWirSoftLaneRepair, hbmSite, hbmChannel, &remapData));
    }

    for (UINT32 hbmDword = 0; hbmDword < s_NumDwordsPerChannel; ++hbmDword)
    {
        HbmDwordRemap dwordRemap = remapData.ExtractDwordRemapping(hbmDword);

        if (dwordRemap.IsNoRemap()) { continue; }

        // Populate the lane error information from the repairs
        vector<FbpaLane> lanes;
        CHECK_RC(ExtractRepairedLanesFromHbmDwordRemap(hbmSite, hbmChannel,
                                                       hbmDword, dwordRemap,
                                                       &lanes));

        pLanes->reserve(pLanes->size() + lanes.size());
        pLanes->insert(pLanes->end(), lanes.begin(), lanes.end());
    }

    return rc;
}

RC Hbm::Hbm2Interface::SoftLaneRepair
(
    const Settings& settings,
    const vector<FbpaLane>& lanes
) const
{
    MASSERT(IsSetup());
    RC rc;

    // For LANE_REPAIR_FIRST_RC_SKIP
    constexpr Memory::RepairType repairType = Memory::RepairType::SOFT;

    GpuHbmInterface* pGpuIntf = GpuInterface();
    MASSERT(pGpuIntf);

    const Wir* pWirSoftLaneRepair = nullptr;
    CHECK_RC(LookupWir(Wir::SOFT_LANE_REPAIR, &pWirSoftLaneRepair));
    MASSERT(pWirSoftLaneRepair);
    const Wir* pWirBypass = nullptr;
    CHECK_RC(LookupWir(Wir::BYPASS, &pWirBypass));
    MASSERT(pWirBypass);

    StickyRC firstRepairRc;
    for (const FbpaLane& lane : lanes)
    {
        // Declare all variables used in LANE_REPAIR_FIRST_RC_SKIP as early as possible
        HbmLane hbmLane;
        LaneRemap remap;
        HbmLaneRemapData originalHbmLaneRemapData; // Original lane remap value of the hbm fuses
        HbmLaneRemapData hbmLaneRemapData;         // Lane remap value written to the hbm fuses

        LANE_REPAIR_FIRST_RC_SKIP(pGpuIntf->GetHbmLane(lane, &hbmLane));

        LANE_REPAIR_FIRST_RC_SKIP(PreSingleLaneRepair(hbmLane.site, hbmLane.channel, lane));

        // Need to hold the IEEE1500 mutex while we perform lane repairs on FB
        // priv path. Blocks external components from touching the interface
        // (assuming they play nice and are trying to grab the mutex!). NOTE: We
        // shouldn't need to grab this mutex since RM shouldn't be
        // initialized. It is nice to have for completeness.
        std::unique_ptr<PmgrMutexHolder> pPmgrMutex;
        LANE_REPAIR_FIRST_RC_SKIP(pGpuIntf->AcquirePmgrMutex(&pPmgrMutex));

        LANE_REPAIR_FIRST_RC_SKIP(pGpuIntf->WirRead(*pWirSoftLaneRepair,
                                                    hbmLane.site, hbmLane.channel,
                                                    &originalHbmLaneRemapData));
        hbmLaneRemapData = originalHbmLaneRemapData;

        // Get information on where the failing bit is positioned within the 128-bit bus
        UINT32 hbmDword = 0;
        UINT32 hbmByte = 0;
        LANE_REPAIR_FIRST_RC_SKIP(pGpuIntf->GetHbmDwordAndByte(hbmLane, &hbmDword, &hbmByte));

        // Callwlate mask and remap values for programming HBM (see Table
        // "LANE_REPAIR Wrapper Data Register" in Micron HBM Spec)
        LANE_REPAIR_FIRST_RC_SKIP(CalcLaneRemapValue(hbmLane, &remap));

        RC validationRc; // Postpone validation check until after status print
        if (!settings.modifiers.skipHbmFuseRepairCheck)
        {
            // Check that the lane is not already fused
            HbmDwordRemap lwrLaneData = hbmLaneRemapData.ExtractDwordRemapping(hbmDword);
            if (!lwrLaneData.IsRemappable(remap))
            {
                Printf(Tee::PriError,
                       "Cannot fuse already repaired lane: remapVal(0x%04x)\n",
                       lwrLaneData.Get());
                validationRc = RC::HBM_SPARE_LANES_NOT_AVAILABLE;
            }
        }

        // Modifier existing IEEE1500 lane repair data with new remap value
        hbmLaneRemapData.ApplyRemap(hbmDword, remap);

        Printf(Tee::PriNormal,
               "[repair]    Repair type: soft repair\n"
               "[repair]    GPU location: HW FBPA %u, subpartition %u, FBPA lane %u\n"
               "[repair]    HBM location: site %u, channel %u, DWORD %u, byte %u\n"
               "[repair]    Byte pair remap: value 0x%04x, mask 0x%04x\n"
               "[repair]    Interface: IEEE1500\n"
               "[repair]    skip_hbm_fuse_repair_check: %s\n"
               "[repair]    pseudo_hard_repair: %s\n"
               "[repair]    Pre-repair  IEEE data           : 0x%08x 0x%08x 0x%08x\n"
               "[repair]    Post-repair IEEE data (expected): 0x%08x 0x%08x 0x%08x\n",
               lane.hwFbpa.Number(), GetHbmDevice()->GetSubpartition(lane), lane.laneBit,
               hbmLane.site.Number(), hbmLane.channel.Number(), hbmDword, hbmByte,
               remap.value, remap.mask,
               Utility::ToString(settings.modifiers.skipHbmFuseRepairCheck).c_str(),
               Utility::ToString(settings.modifiers.pseudoHardRepair).c_str(),
               originalHbmLaneRemapData[0], originalHbmLaneRemapData[1], originalHbmLaneRemapData[2],
               hbmLaneRemapData[0], hbmLaneRemapData[1], hbmLaneRemapData[2]);

        // Check validation status
        LANE_REPAIR_FIRST_RC_SKIP(validationRc);

        // Perform the repair
        //
        LANE_REPAIR_FIRST_RC_SKIP(pGpuIntf->WdrWriteRaw(hbmLane.site,
                                                        hbmLane.channel,
                                                        hbmLaneRemapData));
        pGpuIntf->SleepUs(1); // Wait minimum 500ns

        // Write a Bypass instruction needed to reset the interface and allow
        // for running additional instructions
        LANE_REPAIR_FIRST_RC_SKIP(pGpuIntf->WirWrite(*pWirBypass,
                                                     hbmLane.site,
                                                     hbmLane.channel));

        // After a soft-repair, we need to reprogram the GPU lanes so they match
        // the mem chip
        LANE_REPAIR_FIRST_RC_SKIP(pGpuIntf->RemapGpuLanes(lane));

        // Final step, record repair metadata
        //
        firstRepairRc = PostSingleLaneRepair(settings, repairType, lane,
                                             hbmLane, remap,
                                             originalHbmLaneRemapData,
                                             hbmLaneRemapData, RC::OK);
    }

    return firstRepairRc;
}

// Avoid unused function warnings when compiling without asserts.
#ifdef DEBUG
namespace
{
    //!
    //! \brief Return true if the given mask corresponds to the REPAIR_IN_OTHER
    //! nibble sentinel value, false otherwise.
    //!
    bool IsMaskForRepairInOtherNibble(const LaneRemap& laneRemap, UINT16 nibbleMask)
    {
        // Nibble mask must only have 4 bits set
        MASSERT(Utility::CountBits(nibbleMask) == 4);

        constexpr UINT16 sentinelVal = LaneRemap::BYTE_REMAP_IN_OTHER;

        for (UINT32 shift = 0; shift <= 12; shift += 4)
        {
            const bool isSentinelNibble =
                ((laneRemap.value >> shift) & 0x0f) == sentinelVal;
            const bool isSameMask = nibbleMask == (0x0f << shift);

            if (isSentinelNibble && isSameMask)
            {
                return true;
            }
        }

        return false;
    }
};
#endif

void Hbm::Hbm2Interface::CreateLaneRepairFuseNibbleMasks
(
    const LaneRemap& laneRemap,
    vector<UINT16>* pNibbleMasks
) const
{
    MASSERT(pNibbleMasks);

    constexpr UINT32 numNibbles
        = LaneRemap::bytePairEncodingBitWidth / LaneRemap::byteEncodingBitWidth;
    static_assert(numNibbles == 2, "CreateLaneRepairFuseNibbleMasks algorithm "
                  "only supports remap values of 2 nibbles");

    pNibbleMasks->clear();
    pNibbleMasks->resize(numNibbles, 0);

    // Search for the repaired byte
    //
    constexpr UINT16 nibbleMask = 0x0f;
    constexpr UINT16 nibbleBitWidth = 4;
    constexpr UINT16 nibblesPerRemap = (sizeof(laneRemap.value) * CHAR_BIT) / nibbleBitWidth;
    static_assert(nibblesPerRemap == 4, "CreateLaneRepairFuseNibbleMasks "
                  "algorithm only supports 16bit remap values");

    for (UINT16 nibble = 0; nibble < nibblesPerRemap; ++nibble)
    {
        UINT16 shift = nibbleBitWidth * nibble;
        if (((laneRemap.value >> shift) & nibbleMask)
            == static_cast<UINT16>(LaneRemap::BYTE_REMAP_IN_OTHER))
        {
            pNibbleMasks->at(0) = nibbleMask << shift;
            pNibbleMasks->at(1) = (nibble % 2) ? (nibbleMask << (shift - nibbleBitWidth))
                : (nibbleMask << (shift + nibbleBitWidth));
            break;
        }
    }

    // Masks must have a value
    MASSERT(pNibbleMasks->at(0) && pNibbleMasks->at(1));

    // First mask must correspond to REMAP_IN_OTHER nibble
    MASSERT(IsMaskForRepairInOtherNibble(laneRemap, pNibbleMasks->at(0)));
}

RC Hbm::Hbm2Interface::HardLaneRepair
(
    const Settings& settings,
    const vector<FbpaLane>& lanes
) const
{
    MASSERT(IsSetup());
    RC rc;

    // For LANE_REPAIR_FIRST_RC_SKIP
    constexpr Memory::RepairType repairType = Memory::RepairType::HARD;

    GpuHbmInterface* pGpuIntf = GpuInterface();
    MASSERT(pGpuIntf);

    const Wir* pWirHardLaneRepair;
    CHECK_RC(LookupWir(Wir::HARD_LANE_REPAIR, &pWirHardLaneRepair));
    MASSERT(pWirHardLaneRepair);
    const Wir* pWirBypass;
    CHECK_RC(LookupWir(Wir::BYPASS, &pWirBypass));
    MASSERT(pWirBypass);

    StickyRC firstRepairRc;
    for (const FbpaLane& lane : lanes)
    {
        // Declare all variables used in LANE_REPAIR_FIRST_RC_SKIP as early as possible
        HbmLane hbmLane;
        LaneRemap remap = {};
        HbmLaneRemapData originalHbmLaneRemapData; // Original IEEE value of the hbm fuses
        HbmLaneRemapData hbmLaneRemapData;         // IEEE value written to the hbm fuses

        LANE_REPAIR_FIRST_RC_SKIP(pGpuIntf->GetHbmLane(lane, &hbmLane));

        LANE_REPAIR_FIRST_RC_SKIP(PreSingleLaneRepair(hbmLane.site, hbmLane.channel, lane));

        // Need to hold the IEEE1500 mutex while we perform lane repairs on FB
        // priv path. Blocks external components from touching the interface
        // (assuming they play nice and are trying to grab the mutex!). NOTE: We
        // shouldn't need to grab this mutex since RM shouldn't be
        // initialized. It is nice to have for completeness.
        std::unique_ptr<PmgrMutexHolder> pPmgrMutex;
        LANE_REPAIR_FIRST_RC_SKIP(pGpuIntf->AcquirePmgrMutex(&pPmgrMutex));

        LANE_REPAIR_FIRST_RC_SKIP(pGpuIntf->ResetSite(hbmLane.site));

        LANE_REPAIR_FIRST_RC_SKIP(pGpuIntf->WirRead(*pWirHardLaneRepair,
                                                    hbmLane.site, hbmLane.channel,
                                                    &originalHbmLaneRemapData));
        hbmLaneRemapData = originalHbmLaneRemapData;

        // Get location of the failing bit within the HBM channel
        UINT32 hbmDword = 0;
        UINT32 hbmByte = 0;
        LANE_REPAIR_FIRST_RC_SKIP(pGpuIntf->GetHbmDwordAndByte(hbmLane, &hbmDword, &hbmByte));

        // Callwlate mask and remap values for programming HBM (see Table
        // "LANE_REPAIR Wrapper Data Register" in Micron HBM Spec)
        LANE_REPAIR_FIRST_RC_SKIP(CalcLaneRemapValue(hbmLane, &remap));

        {
            // Modify existing lane repair data with new remap value for
            // display only
            HbmLaneRemapData expectedHbmLaneRemapData = hbmLaneRemapData;
            expectedHbmLaneRemapData.ApplyRemap(hbmDword, remap);

            Printf(Tee::PriNormal,
                   "[repair]    Repair type: hard repair\n"
                   "[repair]    GPU location: HW FBPA %u, subpartition %u, FBPA lane %u\n"
                   "[repair]    HBM location: site %u, channel %u, DWORD %u, byte %u\n"
                   "[repair]    Byte pair remap: value 0x%04x, mask 0x%04x\n"
                   "[repair]    Interface: IEEE1500\n"
                   "[repair]    skip_hbm_fuse_repair_check: %s\n"
                   "[repair]    pseudo_hard_repair: %s\n"
                   "[repair]    Pre-repair  IEEE data           : 0x%08x 0x%08x 0x%08x\n"
                   "[repair]    Post-repair IEEE data (expected): 0x%08x 0x%08x 0x%08x\n",
                   lane.hwFbpa.Number(), GetHbmDevice()->GetSubpartition(lane), lane.laneBit,
                   hbmLane.site.Number(), hbmLane.channel.Number(), hbmDword, hbmByte,
                   remap.value, remap.mask,
                   Utility::ToString(settings.modifiers.skipHbmFuseRepairCheck).c_str(),
                   Utility::ToString(settings.modifiers.pseudoHardRepair).c_str(),
                   originalHbmLaneRemapData[0], originalHbmLaneRemapData[1], originalHbmLaneRemapData[2],
                   expectedHbmLaneRemapData[0], expectedHbmLaneRemapData[1], expectedHbmLaneRemapData[2]);
        }

        if (!settings.modifiers.skipHbmFuseRepairCheck)
        {
            // Check that the lane is not already fused
            HbmDwordRemap lwrLaneData = hbmLaneRemapData.ExtractDwordRemapping(hbmDword);
            if ((lwrLaneData.Get() & remap.mask) != remap.mask)
            {
                Printf(Tee::PriError, "Cannot fuse already repaired lane: remapVal(0x%04x)\n",
                       lwrLaneData.Get());
                LANE_REPAIR_FIRST_RC_SKIP(RC::HBM_SPARE_LANES_NOT_AVAILABLE);
            }
        }

        // Lane repairs must be performed on a full byte pair, but only one
        // nibble at a time. Repairs must start with the "repair in other byte"
        // remap value.
        //
        vector<UINT16> nibbleMasks;
        CreateLaneRepairFuseNibbleMasks(remap, &nibbleMasks);
        for (UINT16 nibbleMask : nibbleMasks)
        {
            HardLaneRepairWriteData hardLaneRepairData;
            hardLaneRepairData.ApplyRemap(hbmDword, remap, nibbleMask);

            if (!settings.modifiers.pseudoHardRepair)
            {
                LANE_REPAIR_FIRST_RC_SKIP(pGpuIntf->WirWrite(*pWirHardLaneRepair,
                                                             hbmLane.site,
                                                             hbmLane.channel,
                                                             hardLaneRepairData));
                pGpuIntf->SleepMs(500);
            }
        }

        // Record the updated HBM lane remapping
        hbmLaneRemapData.ApplyRemap(hbmDword, remap);

        if (!settings.modifiers.pseudoHardRepair)
        {
            LANE_REPAIR_FIRST_RC_SKIP(pGpuIntf->WirWrite(*pWirBypass,
                                                         hbmLane.site,
                                                         hbmLane.channel));
            Tasker::Sleep(100);
        }

        // Reset is required for fuse burn to happen. On a subsequent reset of
        // the GPU, DevInit will read the fused bits and reprogram the GPU
        // accordingly
        LANE_REPAIR_FIRST_RC_SKIP(pGpuIntf->ResetSite(hbmLane.site));

        // Final step, record repair metadata
        //
        firstRepairRc = PostSingleLaneRepair(settings, repairType, lane, hbmLane,
                                             remap, originalHbmLaneRemapData,
                                             hbmLaneRemapData, RC::OK);
    }

    return firstRepairRc;
}

RC Hbm::Hbm2Interface::ExtractRepairedLanesFromHbmDwordRemap
(
    const Site& hbmSite,
    const HbmChannel& hbmChannel,
    UINT32 hbmDword,
    const HbmDwordRemap& dwordRemap,
    vector<FbpaLane>* pLanes
) const
{
    MASSERT(IsSetup());
    MASSERT(pLanes);
    RC rc;

    GpuHbmInterface* pGpuIntf = GpuInterface();
    MASSERT(pGpuIntf);

    constexpr UINT32 dwordBitWidth = 32;
    const FrameBuffer* const pFb = pGpuIntf->GetFb();

    // Return if no repair
    if (dwordRemap.IsNoRemap()) { return rc; }

    // Get associated hardware FBPA
    HwFbpa hwFbpa;
    FbpaSubp fbpaSubp;
    CHECK_RC(pGpuIntf->HbmSiteChannelToHwFbpaSubp(hbmSite, hbmChannel,
                                                  &hwFbpa, &fbpaSubp));

    // Get corresponding FBPA location
    UINT32 fbpaDword = 0;
    CHECK_RC(pGpuIntf->GetFbpaDwordAndByte(HbmLane(hbmSite, hbmChannel, hbmDword),
                                           &fbpaDword, nullptr));

    vector<DwordLane> remappedLanes;
    dwordRemap.GetRemappedLanes(&remappedLanes);

    for (const DwordLane& dwordLane : remappedLanes)
    {
        const UINT32 fbpaLane
            = pFb->GetSubpartitionBusWidth() * fbpaSubp
            + dwordBitWidth * fbpaDword
            + dwordLane.bit;

        pLanes->emplace_back(hwFbpa, fbpaLane, dwordLane.type);
    }

    return rc;
}

RC Hbm::Hbm2Interface::ExtractLaneRepairFromLinkRepairReg
(
    HwFbpa hwFbpa,
    FbpaSubp fbpaSubp,
    UINT32 fbpaDword,
    vector<FbpaLane>* pLanes
) const
{
    MASSERT(IsSetup());
    MASSERT(pLanes);
    RC rc;

    GpuHbmInterface* pGpuIntf = GpuInterface();
    MASSERT(pGpuIntf);

    const FrameBuffer* pFb           = pGpuIntf->GetFb();
    const UINT32       subpBitWidth  = pFb->GetSubpartitionBusWidth();
    constexpr UINT32   dwordBitWidth = 32;

    UINT16 rawRemapVal = 0;
    bool isLinkRegBypassSet = false;
    CHECK_RC(pGpuIntf->GetLinkRepairRegRemapValue(hwFbpa, fbpaSubp, fbpaDword,
                                                  &rawRemapVal, &isLinkRegBypassSet));
    HbmDwordRemap dwordRemap(rawRemapVal);

    // Return if no repair
    if (dwordRemap.IsNoRemap()) { return rc; }

    if (!isLinkRegBypassSet)
    {
        Printf(Tee::PriWarn, "Link repair register HW FBPA %u subp %u DWORD %u "
               "has remap but bypass bit is not set. Remap has no effect.\n",
               hwFbpa.Number(), fbpaSubp.Number(), fbpaDword);
    }

    // Extract remap data
    vector<DwordLane> remappedLanes;
    dwordRemap.GetRemappedLanes(&remappedLanes);

    for (const DwordLane& dwordLane : remappedLanes)
    {
        const UINT32 fbpaLane
            = subpBitWidth * fbpaSubp.Number()
            + dwordBitWidth * fbpaDword
            + dwordLane.bit;

        pLanes->emplace_back(hwFbpa, fbpaLane, dwordLane.type);
    }

    return rc;
}
