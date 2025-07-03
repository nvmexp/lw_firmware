/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "hbm_spec_defines.h"
#include "gpu/include/hbmimpl.h"
#include "core/include/platform.h"

/* virtual */
FLOAT64 HBMImpl::GetPollTimeoutMs() const
{
    return Tasker::GetDefaultTimeoutMs();
}

//------------------------------------------------------------------------------
/* virtual */
RC HBMImpl::WaitForTestI1500(UINT32 site, bool busyWait, FLOAT64 timeoutMs) const
{
    RC rc;

    const RegHal &regs = m_pGpuSub->Regs();
    UINT32 masterFbpaNum;
    CHECK_RC(m_pGpuSub->GetHBMSiteMasterFbpaNumber(site, &masterFbpaNum));

    if (timeoutMs < 0)
    {
        timeoutMs = GetPollTimeoutMs();
    }

    // Poll for MODS_PFB_FBPA_<pa>_FBIO_HBM_TEST_I1500_STATUS_BRIDGE_STATE_IDLE
    const UINT32 pollValue = regs.LookupValue(
            MODS_PFB_FBPA_MC_2_FBIO_HBM_TEST_I1500_STATUS_BRIDGE_STATE_IDLE);

    if (busyWait)
    {
        // Don't allow the calling thread to yield while waiting for IEEE1500 to go idle
        UINT64 timeoutUs = (timeoutMs < 0 ? _UINT64_MAX : static_cast<UINT64>(timeoutMs * 1000.0f));
        UINT64 startTimeUs = Xp::GetWallTimeUS();
        do
        {
            if (regs.Test32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_STATUS_BRIDGE_STATE, masterFbpaNum,
                            pollValue))
            {
                return rc;
            }

            Platform::Pause();
        } while (Xp::GetWallTimeUS() - startTimeUs < timeoutUs);

        rc = RC::TIMEOUT_ERROR;
    }
    else
    {
        const UINT32 pollAddress = regs.LookupAddress(MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_STATUS,
            masterFbpaNum);
        const UINT32 pollMask = regs.LookupMask(
            MODS_PFB_FBPA_MC_2_FBIO_HBM_TEST_I1500_STATUS_BRIDGE_STATE);

        CHECK_RC(m_pGpuSub->PollRegValue(pollAddress, pollValue,
                                         pollMask, timeoutMs));
    }

    return rc;
}

//------------------------------------------------------------------------------
RC HBMImpl::InitSiteInfo
(
    const UINT32 siteID
    ,const bool useHostToJtag
    ,HbmDeviceIdData* pDevIdData
) const
{
    RC rc;
    MASSERT(pDevIdData);

    // Get site info in regValues
    vector<UINT32> regValues(HBM_WIR_DEVICE_ID_NUM_DWORDS);
    if (useHostToJtag)
    {
        CHECK_RC(GetHBMDevIdDataHtoJ(siteID, &regValues));

    }
    else
    {
        CHECK_RC(GetHBMDevIdDataFbPriv(siteID, &regValues));
    }

    MASSERT(pDevIdData);
    pDevIdData->Initialize(regValues);

    return rc;
}

// -----------------------------------------------------------------------------
//!
//! \brief Return the DWORD and byte associated with the given lane over the
//! FBPA <-> HBM boundary.
//!
//! Colwerts: FBPA lane -> HBM DWORD, HBM lane -> FBPA DWORD
//! Accounts for swizzling along the data path.
//!
//! \param siteID HBM site number.
//! \param fbpaLaneBit FBPA lane used to determine the DWORD and byte.
//! \param[out] pDword HBM DWORD. If laneBit is an FBPA lane, this returns the
//! associated HBM DWORD. If laneBit is an HBM lane, this returns the associated
//! FBPA DWORD. Can be nullptr.
//! \param[out] pByte Byte in DWORD containing the given lane. Can be nullptr.
//!
// TODO(stewarts): Break this up into the following:
// - GetHbmDwordAndByte(FbpaLane)
// - GetFbpaDwordAndByte(HbmLane)
/* virtual */ RC HBMImpl::GetDwordAndByteOverFbpaHbmBoundary
(
    const UINT32 siteID
    ,const UINT32 laneBit
    ,UINT32 *pDword
    ,UINT32 *pByte
) const
{
    RC rc;
    MASSERT(pDword || pByte); // Need at least one out parameter

    const UINT32 subpSize = m_pGpuSub->GetFB()->GetSubpartitionBusWidth();
    const UINT32 dwordSize = 32;
    const UINT32 byteSize = 8;

    if (pDword)
    {
        *pDword = ((laneBit % subpSize) / dwordSize);
    }

    if (pByte)
    {
        *pByte = (laneBit % dwordSize) / byteSize;
    }

    return rc;
}

//-----------------------------------------------------------------------------
//!
//! \brief Get lane repairs corresponding to the link repair register described
//! by the given parameters.
//!
//! NOTE: The given DWORD value does not need to be swizzled. When mapping HBM
//! repairs to the link repair registers, there is a need to swizzle the DWORD
//! to map the HBM site and channel repair to the perspective of the FBPA. This
//! means the DWORD that describes the link repair register is already in terms
//! of the FBPA.
//!
//! \param hwFbpa Hardware FBPA that describes the link repair register.
//! \param subp Subpartition associated with the repair in the link repair
//! register.
//! \param fbpaDword DWORD that describes the link repair register.
//! \param[out] pOutLaneRepairs Lanes that correspond to the link repair register
//! will be appended to this array.
//!
RC HBMImpl::GetLaneRepairsFromLinkRepairReg
(
    UINT32 hwFbpa
    ,UINT32 subp
    ,UINT32 fbpaDword
    ,vector<LaneError>* const pOutLaneRepairs
) const
{
    RC rc;
    MASSERT(pOutLaneRepairs);

    RegHal& regs = m_pGpuSub->Regs();
    const FrameBuffer* const pFb = m_pGpuSub->GetFB();
    const UINT32 subpSize = pFb->GetSubpartitionBusWidth();
    const UINT32 dwordSize = 32;

    // Get link repair register data
    LaneError lane(Memory::HwFbpa(hwFbpa), Memory::FbpaSubp(subp), subpSize, fbpaDword); // Any lane in given dword
    UINT32 regAddr = 0;
    CHECK_RC(m_pGpuSub->GetHbmLinkRepairRegister(lane, &regAddr));
    const UINT32 linkRepairRegVal = m_pGpuSub->RegRd32(regAddr);
    const UINT16 remapVal = static_cast<UINT16>(linkRepairRegVal);

    // Return if no repair
    if (remapVal == HBM_REMAP_DWORD_NO_REPAIR) { return rc; }

    if (!regs.GetField(linkRepairRegVal,
                       MODS_PFB_FBPA_0_FBIO_HBM_LINK_REPAIR_CHANNELx_DWORDy_BYPASS))
    {
        Printf(Tee::PriWarn, "Found link repair register 0x%08x with remap value "
               "0x%04x but the bypass bit is not set. Remap value has no "
               "effect.\n", regAddr, remapVal);
    }

    // Extract remap data
    vector<UINT32> dwordLaneOffsets;
    vector<LaneError::RepairType> repairTypes;
    CHECK_RC(ExtractRemapValueData(remapVal, &dwordLaneOffsets, &repairTypes));
    MASSERT(dwordLaneOffsets.size() == repairTypes.size()); // Indices should correspond
    MASSERT(dwordLaneOffsets.size() > 0); // Should be some repair

    // For each repair
    const UINT32 numRepairs = static_cast<UINT32>(dwordLaneOffsets.size());
    MASSERT(numRepairs == dwordLaneOffsets.size()); // Truncation check

    for (UINT32 i = 0; i < numRepairs; ++i)
    {
        const UINT32& dwordLaneOffset = dwordLaneOffsets[i];
        const LaneError::RepairType& repairType = repairTypes[i];

        const UINT32 fbpaLane
            = subpSize * subp
            + dwordSize * fbpaDword
            + dwordLaneOffset;

        // Record lane repair
        //
        pOutLaneRepairs->emplace_back(Memory::HwFbpa(hwFbpa), fbpaLane, repairType);
    }

    return rc;
}

//-----------------------------------------------------------------------------
//!
//! \brief Create one or more lane repair objects based given remap value.
//!
//! Since there can be multiple repairs per HBM fuse remap value, the
//! corresponding number of object will be created; one object per repair.
//!
RC HBMImpl::GetLaneRepairsFromHbmFuse
(
    UINT32 hbmSite
    ,UINT32 hbmChannel
    ,UINT32 hbmDword
    ,UINT32 remapValue
    ,vector<LaneError>* const pOutLaneRepairs
) const
{
    RC rc;
    MASSERT(pOutLaneRepairs);

    // Return if no repair
    if (remapValue == HBM_REMAP_DWORD_NO_REPAIR) { return rc; }

    const FrameBuffer* const pFb = m_pGpuSub->GetFB();
    const UINT32 dwordSize = 32;

    // Get associated hardware FBPA
    UINT32 hwFbio;
    UINT32 subp;
    CHECK_RC(pFb->HbmSiteChannelToFbioSubp(hbmSite, hbmChannel, &hwFbio, &subp));
    const UINT32 hwFbpa = pFb->HwFbioToHwFbpa(hwFbio);

    // Swizzle DWORD (if applicable)
    UINT32 fbpaDword = 0;
    const UINT32 busLaneInHbmDword = hbmDword * dwordSize;
    CHECK_RC(GetDwordAndByteOverFbpaHbmBoundary(hbmSite, busLaneInHbmDword,
                                                &fbpaDword, nullptr));

    // Extract remap data
    vector<UINT32> dwordLaneOffsets;
    vector<LaneError::RepairType> repairTypes;
    CHECK_RC(ExtractRemapValueData(remapValue, &dwordLaneOffsets, &repairTypes));
    MASSERT(dwordLaneOffsets.size() == repairTypes.size()); // Indices should correspond
    MASSERT(dwordLaneOffsets.size() > 0); // Should be some repair

    // For each repair
    for (UINT32 i = 0; i < dwordLaneOffsets.size(); ++i)
    {
        const UINT32& dwordLaneOffset = dwordLaneOffsets[i];
        const LaneError::RepairType& repairType = repairTypes[i];

        const UINT32 fbpaLane
            = pFb->GetSubpartitionBusWidth() * subp
            + dwordSize * fbpaDword
            + dwordLaneOffset;

        // Record lane repair
        //
        pOutLaneRepairs->emplace_back(Memory::HwFbpa(hwFbpa), fbpaLane, repairType);
    }

    return rc;
}

//-----------------------------------------------------------------------------
//!
//! \brief Extract the lane information present in the given remap value.
//!
//! Return the lane offset within the DWORD and the repair type for each repair
//! present in the given DWORD remap value.
//!
//! Index 'i' of the appended values each of the given arrays correspond to the
//! same repair found in the given remap value. No garantee is made for values
//! that were added to the arrays outside of this function.
//!
//! \param remapValue Remap value to decode.
//! \param[out] pOutDwordLaneOffset Array of lane offsets within a remapped
//! DWORD (value 0 to 31) to append to. DWORD lane offsets correspond to the
//! repairs in the given remap value, one element per repair. Each index
//! corresponds to a repair type at the same index in pOutRepairType.
//! \param[out] pOutRepairType Array of repair types to append to. Repair types
//! correspond to the repairs in the given remapValue, one element per repair.
//! Each index corresponds to a DWORD lane at the same index in pOutDwordLane.
//!
RC HBMImpl::ExtractRemapValueData
(
    UINT32 remapValue
    ,vector<UINT32>* const pOutDwordLaneOffsets
    ,vector<LaneError::RepairType>* const pOutRepairTypes
) const
{
    RC rc;
    MASSERT(pOutDwordLaneOffsets);
    MASSERT(pOutRepairTypes);
    MASSERT(pOutDwordLaneOffsets->size() == pOutRepairTypes->size());

    const UINT32 numBytePairs = 2;

    for (UINT32 i = 0; i < numBytePairs; ++i)
    {
        const UINT32 shift = i * HBM_REMAP_DWORD_BYTE_PAIR_ENCODING_SIZE;
        const UINT32 mask = 0xff;
        const UINT08 bytePair = static_cast<UINT08>((remapValue >> shift) & mask);

        // Get the fbpa lane
        //
        const UINT32 bitsPerBytePair = 16;
        UINT32 dwordLaneOffset = bitsPerBytePair * i;
        UINT08 repairTypeEncoding;
        if (bytePair == HBM_REMAP_DWORD_BYTE_PAIR_NO_REPAIR)
        {
            // No repair, continue
            continue;
        }
        else if ((bytePair & 0xf) == HBM_REMAP_DWORD_BYTE_REPAIR_IN_OTHER_BYTE)
        {
            // Repair in 2nd byte in pair
            repairTypeEncoding = (bytePair >> HBM_REMAP_DWORD_BYTE_ENCODING_SIZE) & 0xf;
            dwordLaneOffset += 8;
        }
        else
        {
            // Repair in 1st byte in pair
            repairTypeEncoding = bytePair & 0xf;
        }

        // Check the repair type
        LaneError::RepairType repairType;
        if (repairTypeEncoding == 0) // DM/ECC lane repair
        {
            repairType = LaneError::RepairType::DM;

            // NOTE: By convention we represent the DM lane as the 0th bit of
            // the byte, so no dword lane modifications needed
        }
        else if (repairTypeEncoding >= 1 && repairTypeEncoding <= 8) // DQ (DATA) lane repair
        {
            repairType = LaneError::RepairType::DATA;
            // Specify the repaired DQ lane
            dwordLaneOffset += repairTypeEncoding - 1;
        }
        else if (repairTypeEncoding == 9) // DBI lane repair
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
        pOutDwordLaneOffsets->push_back(dwordLaneOffset);
        pOutRepairTypes->push_back(repairType);
    }

    // 1:1 mapping of dwordLane:repairType
    MASSERT(pOutDwordLaneOffsets->size() == pOutRepairTypes->size());
    // If no remapping no repairs should be found, o/w there should be repairs
    MASSERT((remapValue == HBM_REMAP_DWORD_NO_REPAIR && pOutDwordLaneOffsets->size() == 0)
            || (remapValue != HBM_REMAP_DWORD_NO_REPAIR && pOutDwordLaneOffsets->size() > 0));

    return rc;
}
