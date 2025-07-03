/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "mbist.h"

#include "core/include/mle_protobuf.h"
#include "gpu/include/pmgrmutex.h"
#include "gpu/hbm/hbm_mle_colw.h"
#include "gpu/hbm/hbm_spec_defines.h"

//-------------------------------------------------------------------------------------------------
// Pull lane data out of IEEE data registers according to the "LANE_REPAIR Wrapper Data Register"
// Table from the Micron HBM Spec
UINT16 MBistImpl::GetLaneRepairData(const vector<UINT32>& ieeeData, UINT32 dword) const
{
    UINT16 laneData = 0;
    UINT32 bitShift = 0;
    switch (dword)
    {
        // DWORD0 is contained in Bits 15:0 of the IEEE data
        case 0:
            bitShift = 0;
            laneData = ieeeData[0] >> bitShift;
            break;

        // DWORD1 is contained in Bits 31:16 of the IEEE data
        case 1:
            bitShift = 16;
            laneData = ieeeData[0] >> bitShift;
            break;

        // DWORD2 is contained in Bits 55:40 of the IEEE data
        case 2:
            bitShift = 40 - 32;
            laneData = ieeeData[1] >> bitShift;
            break;

        // DWORD3 is contained in Bits 71:56 of the IEEE data
        case 3:
            bitShift = 72 - 64;
            laneData = ieeeData[2] << bitShift;

            bitShift = 56 - 32;
            laneData |= (ieeeData[1] >> bitShift) & 0xFF;
            break;

        default:
            Printf(Tee::PriError, "Invalid lane repair: DWORD %u is not valid!\n", dword);
            return 0;
    }

    return laneData;
}

//-------------------------------------------------------------------------------------------------
// Put remap data into the IEEE data registers according to the "LANE_REPAIR Wrapper Data Register"
// Table from the Micron HBM Spec, and according to the mask we provide in case we want to leave
// other nibbles untouched
UINT16 MBistImpl::ModifyLaneRepairData
(
    vector<UINT32>* const pIeeeData
    ,UINT32 dword
    ,UINT16 remapVal
    ,UINT16 mask) const
{
    MASSERT(pIeeeData);

    UINT32 bitShift = 0;
    switch (dword)
    {
        // DWORD0 is contained in Bits 15:0 of the IEEE data
        case 0:
            bitShift = 0;
            (*pIeeeData)[0] = ((*pIeeeData)[0] & ~(mask << bitShift)) | (remapVal << bitShift);
            break;

        // DWORD1 is contained in Bits 31:16 of the IEEE data
        case 1:
            bitShift = 16;
            (*pIeeeData)[0] = ((*pIeeeData)[0] & ~(mask << bitShift)) | (remapVal << bitShift);
            break;

        // DWORD2 is contained in Bits 55:40 of the IEEE data
        case 2:
            bitShift = 40 - 32;
            (*pIeeeData)[1] = ((*pIeeeData)[1] & ~(mask << bitShift)) | (remapVal << bitShift);
            break;

        // DWORD3 is contained in Bits 71:56 of the IEEE data
        case 3:
            bitShift = 64 - 56;
            (*pIeeeData)[2] = ((*pIeeeData)[2] & ~(mask >> bitShift)) | (remapVal >> bitShift);

            bitShift = 56-32;
            (*pIeeeData)[1] = ((*pIeeeData)[1] & ~(mask << bitShift)) | (remapVal << bitShift);
            break;

        default:
            Printf(Tee::PriError, "Invalid lane repair: DWORD %u is not valid!\n", dword);
            return 0;
    }

    return GetLaneRepairData(*pIeeeData, dword);
}

//-----------------------------------------------------------------------------
//!
//! \brief Collect lane repair data for the given site and channel.
//!
//! \param siteID: HBM site.
//! \param hbmChannel: Channel in the given site.
//! \param pOutLaneRepairs: Populate given vector with the HBM fuse repair data.
//!
RC MBistImpl::GetRepairedLanes
(
    const UINT32 siteID
    ,const UINT32 hbmChannel
    ,vector<LaneError>* const pOutLaneRepairs
) const
{
    RC rc;

    UINT32 instructionID = (hbmChannel << 8);
    instructionID |= HBM_WIR_INSTR_SOFT_LANE_REPAIR;
    vector<UINT32> ieeeData;
    CHECK_RC(GetHBMImpl()->ReadHBMLaneRepairReg(siteID, instructionID, hbmChannel,
                                                &ieeeData, HBM_WDR_LENGTH_LANE_REPAIR,
                                                IsHostToJtagPath()));

    for (UINT32 hbmDword = 0; hbmDword < 4; hbmDword++)
    {
        UINT16 laneData = GetLaneRepairData(ieeeData, hbmDword);

        if (laneData == HBM_REMAP_DWORD_NO_REPAIR) { continue; }

        // Populate the lane error information from the repairs
        vector<LaneError> laneErrors;
        CHECK_RC(GetHBMImpl()->GetLaneRepairsFromHbmFuse(siteID, hbmChannel,
                                                         hbmDword, laneData,
                                                         &laneErrors));

        pOutLaneRepairs->reserve(pOutLaneRepairs->size() + laneErrors.size());
        pOutLaneRepairs->insert(pOutLaneRepairs->end(), laneErrors.begin(),
                                laneErrors.end());
    }

    return rc;
}

//-----------------------------------------------------------------------------
//!
//! \brief Perform a soft lane repair.
//!
//! \param pIeeeData IEE1500 data read from the fuses associated with the lane.
//! On a successful return, it will be modified to represent the data written to
//! the HBM fuses during the repair.
//! \param siteID HBM site number.
//! \param hbmChannel HBM channel number.
//! \param hbmDword HBM DWORD.
//! \param remapVal Remap value associated with the repair.
//! \param remapValMask Mask representing the portion of the given remap value
//! that is relevent to this repair.
//!
RC MBistImpl::SoftLaneRepair
(
    vector<UINT32>* const pIeeeData
    ,const UINT32 siteID
    ,const UINT32 hbmChannel
    ,const UINT32 hbmDword
    ,const UINT16 remapVal
    ,const UINT16 remapValMask
) const
{
    RC rc;

    // Populate new Lane Repair data, and readback remapVal from HBM registers
    // in case HBM has already been reprogrammed before; we'll pass that value
    // to GPU reprogramming.
    ModifyLaneRepairData(pIeeeData, hbmDword, remapVal, remapValMask);

    UINT32 instructionID = (HBM_WIR_INSTR_SOFT_LANE_REPAIR | (hbmChannel << 8));
    // Write back Lane Repair data to the IEEE1500 interface
    CHECK_RC(GetHBMImpl()->WriteHBMDataReg(siteID, instructionID, *pIeeeData,
                                           HBM_WDR_LENGTH_LANE_REPAIR, IsHostToJtagPath()));

    Tasker::Sleep(1); // Need to wait for 500ns

    // Write a Bypass instruction needed to reset the interface and allow for running additional
    // instructions
    CHECK_RC(GetHBMImpl()->WriteBypassInstruction(siteID, hbmChannel));

    return rc;
}

//-----------------------------------------------------------------------------
//!
//! \brief Perform a hard lane repair.
//!
//! \param pIeeeData IEE1500 data read from the fuses associated with the lane.
//! On a successful return, it will be modified to represent the data written to
//! the HBM fuses during the repair.
//! \param isPseudoRepair Do not execute code that would perform a permanent repair.
//! \param siteID HBM site number.
//! \param hbmChannel HBM channel number.
//! \param hbmDword HBM DWORD.
//! \param remapVal Remap value associated with the repair.
//! \param remapValMask Mask representing the portion of the given remap value
//! that is relevent to this repair.
//!
RC MBistImpl::HardLaneRepair
(
    vector<UINT32>* const pIeeeData
    ,const bool isPseudoRepair
    ,const UINT32 siteID
    ,const UINT32 hbmChannel
    ,const UINT32 hbmDword
    ,const UINT16 remapVal
    ,const UINT16 remapValMask
) const
{
    RC rc;

    // Create a vector of nibble masks that will select the individual nibbles
    // to fuse
    const UINT32 numNibbles = 2;
    vector< UINT16 > nibbleMask(numNibbles, 0);
    for (UINT16 i = 0; i < 4; i++)
    {
        UINT16 shift = 4 * i;
        if (((remapVal >> shift) & 0xF) == 0xE)
        {
            nibbleMask[0] = 0xF << shift;
            nibbleMask[1] = (i % 2) ? (0xF << (shift - 4))
                                    : (0xF << (shift + 4));
            break;
        }
    }
    MASSERT(nibbleMask[0] && nibbleMask[1]);

    // We need to perform the fusing one nibble at a time, starting with the 0xE
    // instruction
    for (UINT32 i = 0; i < numNibbles; i++)
    {
        vector<UINT32> wrIeeeData(pIeeeData->size(), ~0U);

        ModifyLaneRepairData(&wrIeeeData, hbmDword, remapVal & nibbleMask[i],
                             remapValMask & nibbleMask[i]);

        UINT32 instructionID = (HBM_WIR_INSTR_HARD_LANE_REPAIR | (hbmChannel << 8));
        if (!isPseudoRepair)
        {
            // Write back Lane Repair data to the IEEE1500 interface
            CHECK_RC(GetHBMImpl()->WriteHBMDataReg(siteID, instructionID, wrIeeeData,
                            HBM_WDR_LENGTH_LANE_REPAIR, IsHostToJtagPath()));

            Tasker::Sleep(500); // Need to wait for 500ms
        }
    }

    // Record the new IEEE Data
    ModifyLaneRepairData(pIeeeData, hbmDword, remapVal, remapValMask);

    if (!isPseudoRepair)
    {
        // Write a Bypass instruction needed to reset the interface and allow for
        // running additional instructions
        CHECK_RC(GetHBMImpl()->WriteBypassInstruction(siteID, hbmChannel));

        Tasker::Sleep(100);
    }

    // During a hard-repair, a reset is required for fuse burn to happen. On a
    // subsequent reset of the GPU, DevInit will read the fused bits and
    // reprogram the GPU accordingly
    CHECK_RC(GetHBMImpl()->ResetSite(siteID));

    return rc;
}

//-----------------------------------------------------------------------------
//!
//! \brief Perform a lane repair.
//!
//! \param isSoft True if repair should be a soft (temporary) repair, otherwise
//! performs a hard (permanent) repair.
//! \param skipVerif Skip hbm fuse check for previous repair. If false, will
//! fail if location is already repaired.
//! \param isPesudoRepair Do not execute code that would perform a permanent
//! repair.
//! \param hbmSite HBM site number.
//! \param hbmChannel HBM channel number.
//! \param laneError Lane to repair.
//!
RC MBistImpl::LaneRepair
(
    const bool isSoft
    ,const bool skipVerif
    ,const bool isPseudoRepair
    ,const UINT32 hbmSite
    ,const UINT32 hbmChannel
    ,const LaneError& laneError
) const
{
    RC rc;
    GpuSubdevice* pSubdev = GetGpuSubdevice();

    unique_ptr<PmgrMutexHolder> pPmgrMutex;
    if (IsHostToJtagPath())
    {
        // TODO(stewarts): This is the proposed HtoJ fix for clearing the SLR data
        // from the HtoJ interface so a following fuse read will not pick it up. It
        // does not appear to be working. SLR remap data is still present in the
        // fuses. As a temporary fix for hard repair, a hot reset is used to clear
        // the HtoJ interface.
        //
        // // Before starting HW repair, we should reset the HBM fields in case we did
        // // a SW repair earlier
        // if (!isSoft && IsHostToJtagPath())
        // {
        //     // Write a Bypass instruction needed to reset the interface and allow
        //     // for running additional instructions
        //     CHECK_RC(GetHBMImpl()->WriteBypassInstruction(hbmSite, hbmChannel));
        //
        //     Tasker::Sleep(100);
        //
        //     CHECK_RC(GetHBMImpl()->ResetSite(hbmSite));
        // }
    }
    else
    {
        // If we're using fbpriv path, we need to hold the IEEE1500 mutex while
        // we perform lane repairs
        pPmgrMutex = make_unique<PmgrMutexHolder>(pSubdev,
                                                  PmgrMutexHolder::MI_HBM_IEEE1500);
        CHECK_RC(pPmgrMutex->Acquire(Tasker::NO_TIMEOUT));

        // Before starting HW repair, we should reset the HBM fields in case we
        // did a SW repair earlier
        if (!isSoft)
        {
            CHECK_RC(GetHBMImpl()->ResetSite(hbmSite));
        }
    }

    UINT32 instructionID = isSoft ? HBM_WIR_INSTR_SOFT_LANE_REPAIR
                                  : HBM_WIR_INSTR_HARD_LANE_REPAIR;
    instructionID |= (hbmChannel << 8);
    vector<UINT32> originalIeeeData; // Original IEEE value of the hbm fuses
    vector<UINT32> ieeeData;         // IEEE value written to the hbm fuses
    CHECK_RC(GetHBMImpl()->ReadHBMLaneRepairReg(hbmSite, instructionID, hbmChannel,
                                                &originalIeeeData, HBM_WDR_LENGTH_LANE_REPAIR,
                                                IsHostToJtagPath()));
    ieeeData = originalIeeeData;

    // Get information on where the failing bit is positioned within the 128-bit bus
    UINT32 hbmDword = 0;
    UINT32 hbmByte = 0;
    CHECK_RC(GetHBMImpl()->GetDwordAndByteOverFbpaHbmBoundary(hbmSite, laneError.laneBit,
                                                              &hbmDword, &hbmByte));

    // Callwlate mask and remap values for programming HBM (see Table
    // "LANE_REPAIR Wrapper Data Register" in Micron HBM Spec)
    UINT16 remapValMask = 0;
    UINT16 remapVal = 0;
    CHECK_RC(HBMDevice::CalcLaneMaskAndRemapValue(laneError.laneType,
                                                  laneError.laneBit,
                                                  &remapValMask, &remapVal));

    Printf(Tee::PriNormal,
           "[repair] %s lane repair on %s\n"
           "[repair] \tGPU location: HW FBPA %u, subpartition %u, FBPA lane %u\n"
           "[repair] \tHBM location: site %u, channel %u, dword %u, byte %u\n"
           "[repair] \tByte pair remap value: 0x%04x, mask 0x%04x\n"
           "[repair] \tInterface: %s\n"
           "[repair] \tskip_hbm_fuse_repair_check: %s\n",
           (isSoft ? "Soft" : "Hard"), laneError.ToString().c_str(),
           laneError.hwFbpa.Number(), GetHbmDevice()->GetSubpartition(laneError), laneError.laneBit,
           hbmSite, hbmChannel, hbmDword, hbmByte,
           remapVal, remapValMask,
           (IsHostToJtagPath() ? "HOST2JTAG" : "IEEE1500"),
           Utility::ToString(skipVerif).c_str());

    RC repairRc;
    if (!skipVerif)
    {
        UINT16 lwrLaneData = GetLaneRepairData(ieeeData, hbmDword);
        if ((lwrLaneData & remapValMask) != remapValMask)
        {
            Printf(Tee::PriError, "Cannot fuse already repaired lane: 0x%04x!\n", lwrLaneData);
            repairRc = RC::HBM_SPARE_LANES_NOT_AVAILABLE;
        }
    }

    if (repairRc == RC::OK)
    {
        // Perform the repair
        if (isSoft)
        {
            repairRc = SoftLaneRepair(&ieeeData, hbmSite, hbmChannel, hbmDword, remapVal,
                                      remapValMask);

            if (repairRc == RC::OK)
            {
                // After a soft-repair, we need to reprogram the GPU lanes so they match
                // the mem chip
                repairRc = GetGpuSubdevice()->ReconfigGpuHbmLanes(laneError);
            }
        }
        else
        {
            Printf(Tee::PriNormal,
                   "[repair] \tpseudo_hard_repair: %s\n",
                   Utility::ToString(isPseudoRepair).c_str());

            repairRc = HardLaneRepair(&ieeeData, isPseudoRepair, hbmSite, hbmChannel,
                                      hbmDword, remapVal, remapValMask);
        }

        Printf(Tee::PriNormal,
               "[repair] \tPre-repair  IEEE data:\t0x%08x 0x%08x 0x%08x\n"
               "[repair] \tPost-repair IEEE data:\t0x%08x 0x%08x 0x%08x\n",
               originalIeeeData[0], originalIeeeData[1], originalIeeeData[2],
               ieeeData[0], ieeeData[1], ieeeData[2]);
    }

    if (repairRc != RC::OK)
    {
        Printf(Tee::PriError, "[repair] \tFAILED with RC %d: %s\n",
               repairRc.Get(), repairRc.Message());
    }

    std::string ecid;
    CHECK_RC(pSubdev->GetEcid(&ecid));

    Mle::Print(Mle::Entry::hbm_lane_repair_attempt)
        .status(repairRc.Get())
        .ecid(ecid)
        .name(laneError.ToString())
        .type(Mle::ToLane(laneError.laneType))
        .hw_fbpa(laneError.hwFbpa.Number())
        .subp(GetHbmDevice()->GetSubpartition(laneError))
        .fbpa_lane(laneError.laneBit)
        .site(hbmSite)
        .channel(hbmChannel)
        .dword(hbmDword)
        .byte(hbmByte)
        .remap_val(remapVal)
        .remap_mask(remapValMask)
        .pre_repair_ieee_data(originalIeeeData)
        .post_repair_ieee_data(ieeeData)
        .interface(m_UseHostToJtag
                   ? Mle::HbmRepair::host2jtag
                   : Mle::HbmRepair::ieee1500)
        .hard_repair(!isSoft)
        .skip_verif(skipVerif)
        .pseudo_repair(isPseudoRepair);

    return repairRc;
}
