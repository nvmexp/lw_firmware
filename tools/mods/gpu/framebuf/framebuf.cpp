/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------

#include "core/include/framebuf.h"

#ifdef MATS_STANDALONE
#include "gpu/reghal/reghal.h"
#else
#include "core/include/mgrmgr.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#endif

//------------------------------------------------------------------------------
// FrameBuffer object properties and methods.
//------------------------------------------------------------------------------

/* virtual */ RC FrameBuffer::InitFbpInfo()
{
    RC rc;

    const RegHal &regs = GpuSub()->Regs();
    const FloorsweepImpl *pFsImpl = GpuSub()->GetFsImpl();

    // Get FBP config
    //
    m_FbpMask = pFsImpl->FbpMask();
    m_MaxFbpCount = regs.Read32(MODS_PTOP_SCAL_NUM_FBPS);
    MASSERT(m_MaxFbpCount <= 8 * sizeof(m_FbpMask));
    MASSERT(m_FbpMask >> m_MaxFbpCount == 0);
    MASSERT(m_FbpMask != 0);

    // Get FBIO config
    //
    m_FbioMask = pFsImpl->FbioMask();
    m_MaxFbioCount = regs.IsSupported(MODS_PTOP_SCAL_NUM_FBPAS)
                         ? regs.Read32(MODS_PTOP_SCAL_NUM_FBPAS)
                         : regs.Read32(MODS_PTOP_SCAL_NUM_FBPS);
    m_FbiosPerFbp = m_MaxFbioCount / m_MaxFbpCount;
    MASSERT(m_MaxFbioCount % m_MaxFbpCount == 0);
    MASSERT(m_MaxFbioCount <= 8 * sizeof(m_FbioMask));
    MASSERT(m_FbioMask >> m_MaxFbioCount == 0);

    // Get LTC config
    //
    m_MaxLtcsPerFbp = regs.IsSupported(MODS_PTOP_SCAL_NUM_LTC_PER_FBP)
                          ? regs.Read32(MODS_PTOP_SCAL_NUM_LTC_PER_FBP)
                          // Fermi and Kepler chips have 1 LTC per FBP
                          : 1;
    m_LtcMask = 0;
    for (INT32 fbp = Utility::BitScanForward(m_FbpMask);
         fbp >= 0;
         fbp = Utility::BitScanForward(m_FbpMask, fbp + 1))
    {
        const UINT32 ltcsOnFbp = pFsImpl->L2Mask(fbp);
        MASSERT(ltcsOnFbp >> m_MaxLtcsPerFbp == 0);
        m_LtcMask |= ltcsOnFbp << (fbp * m_MaxLtcsPerFbp);
    }
    m_MaxLtcCount = m_MaxLtcsPerFbp * m_MaxFbpCount;
    MASSERT(m_MaxLtcsPerFbp * m_MaxFbpCount <= 8 * sizeof(m_LtcMask));
    MASSERT(m_LtcMask >> (m_MaxLtcsPerFbp * m_MaxFbpCount) == 0);
    MASSERT(m_LtcMask > 0);

    // Get the floorswept LTC Slices. Amaplib "slsfs" needs to be configured
    // with this value. Supported on Hopper+.
    if (regs.IsSupported(MODS_PFB_PRI_MMU_NUM_ACTIVE_LTCS_SLICE_MODE))
    {
        m_L2FSSlices = regs.Read32(MODS_PFB_PRI_MMU_NUM_ACTIVE_LTCS_SLICE_MODE);
        MASSERT(m_L2FSSlices == 0 /* MODS_PFB_PRI_MMU_NUM_ACTIVE_LTCS_SLICE_MODE_ZERO */ ||
                m_L2FSSlices == 2 /* MODS_PFB_PRI_MMU_NUM_ACTIVE_LTCS_SLICE_MODE_TWO  */ ||
                m_L2FSSlices == 4 /* MODS_PFB_PRI_MMU_NUM_ACTIVE_LTCS_SLICE_MODE_FOUR */);
    }

    // Get maximum number of L2 slices
    //
    // When the LTS are floorswept, MODS_PLTCG_LTC_LTSS_CBC_PARAM_SLICES_PER_LTC does not
    // always give the max number of slices among all LTCs but LW_PTOP does, because LW_PTOP
    // does not account for floorsweeping.
    if (m_L2FSSlices > 0)
    {
        m_MaxSlicesPerLtc = regs.Read32(MODS_PTOP_SCAL_NUM_SLICES_PER_LTC);
    }
    // Pre-Hopper we change the "max slices" depending on if half-ltc mode is enabled
    else if (regs.IsSupported(MODS_PLTCG_LTCS_LTSS_CBC_PARAM_SLICES_PER_LTC))
    {
        m_MaxSlicesPerLtc = regs.Read32(MODS_PLTCG_LTCS_LTSS_CBC_PARAM_SLICES_PER_LTC);
    }
    // LW_PTOP always reports the _actual_ max slices per LTC based on the LITTER
    else if (regs.IsSupported(MODS_PTOP_SCAL_NUM_SLICES_PER_LTC))
    {
        m_MaxSlicesPerLtc = regs.Read32(MODS_PTOP_SCAL_NUM_SLICES_PER_LTC);
    }
    // We have to use a different register for Kepler chips
    // since they only ever have 1 LTC / FBP
    else if (regs.IsSupported(MODS_SCAL_LITTER_NUM_LTC_SLICES))
    {
        m_MaxSlicesPerLtc = regs.LookupAddress(MODS_SCAL_LITTER_NUM_LTC_SLICES);
    }
    // Fermi chips do not have the number of slices in the
    // reference manuals. GF119 has 1 slice, the rest 2.
    else
    {
        m_MaxSlicesPerLtc = 2;
    }

    // Get L2 Slice config
    //
    // Because LTC slices are floor-sweepable, we need to build a map of [LTC$ LTS Count]
    // that GetSlicesPerLtc() and friends can consult for Hopper. Don't bother unless
    // LTS is floorswept, because then all the LTS related routines would fall back to
    // old way of figuring out LTS per LTC, etc which will be more efficient.
    if (m_L2FSSlices > 0)
    {
        MASSERT(regs.IsSupported(MODS_PPRIV_MASTER_RING_ENUMERATE_RESULTS_L2_COUNT));
        UINT32 numActiveLTCs = regs.Read32(MODS_PPRIV_MASTER_RING_ENUMERATE_RESULTS_L2_COUNT);
        MASSERT(numActiveLTCs != 0);
        MASSERT(numActiveLTCs == GetLtcCount());

        UINT32 totalNumLTS = 0;
        for (UINT32 ltcIndex = 0; ltcIndex < numActiveLTCs; ++ltcIndex)
        {
            MASSERT(regs.IsSupported(MODS_PLTCG_LTCx_LTS0_CBC_PARAM_SLICES_PER_LTC, ltcIndex));
            UINT32 numLTS = regs.Read32(MODS_PLTCG_LTCx_LTS0_CBC_PARAM_SLICES_PER_LTC, ltcIndex);
            m_LTCToLTSMap.push_back(numLTS);
            totalNumLTS += numLTS;
        }
        MASSERT((m_MaxLtcCount * m_MaxSlicesPerLtc) >= totalNumLTS);
    }
    // If there is no L2Slice floorsweeping each LTC has the maximum number of slices
    else
    {
        // LtcMask is already inited so it is safe to get the count
        for (UINT32 ltcIndex = 0; ltcIndex < GetLtcCount(); ltcIndex++)
        {
            m_LTCToLTSMap.push_back(m_MaxSlicesPerLtc);
        }
    }

    // Since Fermi is the only family with none of the slice values in reghal,
    // hardcode slice size for Fermi
    if (!regs.IsSupported(MODS_CHIP_LTC_LTS_NUM_SETS)        &&
        !regs.IsSupported(MODS_SCAL_LITTER_NUM_LTC_LTS_WAYS) &&
        !regs.IsSupported(MODS_CHIP_LTC_LTS_BYTES_PER_LINE))
    {
        m_L2SliceSize = 0x10000;
    }
    // Otherwise callwlate from manuals
    else
    {
        m_L2SliceSize = regs.LookupAddress(MODS_CHIP_LTC_LTS_NUM_SETS)        *
                        regs.LookupAddress(MODS_SCAL_LITTER_NUM_LTC_LTS_WAYS) *
                        regs.LookupAddress(MODS_CHIP_LTC_LTS_BYTES_PER_LINE);
    }
    MASSERT(m_MaxSlicesPerLtc > 0);
    MASSERT(m_L2SliceSize > 0);

    // Initialize L2 slice masks
    m_L2SliceMasks.resize(m_MaxFbpCount, 0);
    for (INT32 hwFbp = Utility::BitScanForward(m_FbpMask);
         hwFbp >= 0;
         hwFbp = Utility::BitScanForward(m_FbpMask, hwFbp + 1))
    {
        // Directly fetch the L2Slice masks if supported
        if (pFsImpl->SupportsL2SliceMask(0))
        {
            m_L2SliceMasks[hwFbp] = pFsImpl->L2SliceMask(hwFbp);
        }
        // Otherwise callwlate them from the L2Mask
        //
        // Even though HW doesn't doesn't use these masks it simplifies
        // the Framebuffer class to create them
        else
        {
            const UINT32 ltcMaskOnFbp = pFsImpl->L2Mask(hwFbp);
            const UINT32 enableMask = (1u << m_MaxSlicesPerLtc) - 1;
            for (INT32 ltc = Utility::BitScanForward(ltcMaskOnFbp);
                 ltc >= 0;
                 ltc = Utility::BitScanForward(ltcMaskOnFbp, ltc + 1))
            {
                m_L2SliceMasks[hwFbp] |= enableMask << (ltc * m_MaxSlicesPerLtc);
            }
        }
    }

    return rc;
}

UINT32 FrameBuffer::GetSubpartitionBusWidth() const
{
    return
        GetPseudoChannelsPerSubpartition() * // PseudoChannels per subpartition
        GetBeatSize() *                      // Bytes per pseudoChannel
        8;                                   // Bits per byte
}

/* virtual */ UINT32 FrameBuffer::GetFbpCount() const
{
    return Utility::CountBits(GetFbpMask());
}

/* virtual */ UINT32 FrameBuffer::GetFbioCount() const
{
    return Utility::CountBits(GetFbioMask());
}

/* virtual */ UINT32 FrameBuffer::GetLtcCount() const
{
    return Utility::CountBits(GetLtcMask());
}

/* virtual */ UINT32 FrameBuffer::GetSlicesPerLtc(UINT32 ltcIndex) const
{
    MASSERT(ltcIndex >= 0 && ltcIndex < GetLtcCount());
    MASSERT(m_LTCToLTSMap[ltcIndex] > 0);
    return m_LTCToLTSMap[ltcIndex];
}

/* virtual */ RC FrameBuffer::GlobalToLocalLtcSliceId
(
    UINT32 globalSliceIndex,
    UINT32 *ltcId,
    UINT32 *localSliceId
) const
{
    *ltcId = ~0;
    *localSliceId = ~0;

    if (m_L2FSSlices == 0)
    {
        // LTS is not floorswept
        const UINT32 slicesPerLtc = GetMaxSlicesPerLtc();
        MASSERT(slicesPerLtc > 0);
        *ltcId = globalSliceIndex / slicesPerLtc;
        *localSliceId = globalSliceIndex % slicesPerLtc;
    }
    else
    {
        UINT32 lastIdx = 0;
        bool bFoundIt = false;

        for (UINT32 ltcIndex = 0; ltcIndex < m_LTCToLTSMap.size(); ltcIndex++)
        {
            UINT32 slices = m_LTCToLTSMap[ltcIndex];

            if (globalSliceIndex >= lastIdx && globalSliceIndex < (lastIdx + slices))
            {
                *ltcId = ltcIndex;
                *localSliceId = globalSliceIndex - lastIdx;
                MASSERT(*localSliceId < slices);
                bFoundIt = true;
                break;
            }
            lastIdx += slices;
        }

        // We should've found it!!
        if (!bFoundIt)
        {
            MASSERT(!"No valid LTC<->LTS mapping was found.\n");
            return RC::SOFTWARE_ERROR;
        }
    }

    return RC::OK;
}

/* virtual */ UINT32 FrameBuffer::GetFirstGlobalSliceIdForLtc(UINT32 ltcIndex) const
{
    UINT32 globalSliceId = ~0;

    if (m_L2FSSlices == 0)
    {
        // LTS is not floorswept
        globalSliceId = ltcIndex * GetMaxSlicesPerLtc();
    }
    else
    {
        bool bFoundIt = false;

        globalSliceId = 0;
        for (UINT32 ltcIdx = 0; ltcIdx < m_LTCToLTSMap.size(); ltcIdx++)
        {
            if (ltcIndex == ltcIdx)
            {
                bFoundIt = true;
                break;
            }
            globalSliceId += m_LTCToLTSMap[ltcIdx];
        }

        // We should've found it!!
        if (!bFoundIt)
        {
            MASSERT(!"No valid LTC<->LTS mapping was found.\n");
        }
    }

    return globalSliceId;
}

/* virtual */ UINT32 FrameBuffer::GetL2SliceCount() const
{
    UINT32 numActiveL2Slices = 0;
    for (auto const& slices : m_LTCToLTSMap)
    {
        numActiveL2Slices += slices;
    }
    return numActiveL2Slices;
}

/* virtual */ UINT32 FrameBuffer::VirtualFbpToHwFbp(UINT32 virtualFbp) const
{
    INT32 hwFbp = Utility::FindNthSetBit(GetFbpMask(), virtualFbp);
    MASSERT(hwFbp >= 0);
    return hwFbp;
}

/* virtual */ UINT32 FrameBuffer::HwFbpToVirtualFbp(UINT32 hwFbp) const
{
    MASSERT(((GetFbpMask() >> hwFbp) & 1) == 1);
    return Utility::CountBits(GetFbpMask() & ((1 << hwFbp) - 1));
}

/* virtual */ UINT32 FrameBuffer::VirtualFbioToHwFbio(UINT32 virtualFbio) const
{
    INT32 hwFbio = Utility::FindNthSetBit(GetFbioMask(), virtualFbio);
    MASSERT(hwFbio >= 0);
    return hwFbio;
}

/* virtual */ UINT32 FrameBuffer::HwFbioToVirtualFbio(UINT32 hwFbio) const
{
    MASSERT(((GetFbioMask() >> hwFbio) & 1) == 1);
    return Utility::CountBits(GetFbioMask() & ((1 << hwFbio) - 1));
}

/* virtual */ RC FrameBuffer::FbioSubpToHbmSiteChannel
(
    UINT32  hwFbio,
    UINT32  subpartition,
    UINT32 *pHbmSite,
    UINT32 *pHbmChannel
) const
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC FrameBuffer::FbioSubpToHbmSiteChannel
(
    UINT32  hwFbio,
    UINT32  subpartition,
    UINT32  pseudoChannel,
    UINT32 *pHbmSite,
    UINT32 *pHbmChannel
) const
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC FrameBuffer::HbmSiteChannelToFbioSubp
(
    UINT32  hbmSite,
    UINT32  hbmChannel,
    UINT32 *pHwFbio,
    UINT32 *pSubpartition
) const
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC FrameBuffer::HwFbpToHbmSite(UINT32 hwFbp, UINT32* pHbmSite) const
{
    MASSERT(pHbmSite);
    MASSERT(IsHbm());

    // A single FBP is only associated with one HBM site. Grab an arbitrary
    // FBIO and subp for the colwersion.
    const UINT32 hwFbio = hwFbp * GetFbiosPerFbp();
    UINT32 hbmChannel = 0;
    return FbioSubpToHbmSiteChannel(hwFbio, 0, pHbmSite, &hbmChannel);
}

UINT32 FrameBuffer::GetFbpsPerHbmSite() const
{
    MASSERT(IsHbm());

    const UINT32 fbiosPerSite = GetChannelsPerSite() / GetChannelsPerFbio();
    return fbiosPerSite / GetFbiosPerFbp();
}

RC FrameBuffer::IsHbmSiteAvailable(UINT32 hbmSite, bool* pIsAvailable) const
{
    MASSERT(pIsAvailable);
    MASSERT(IsHbm());
    RC rc;

    const UINT32 fbpMask = GetFbpMask();

    UINT32 fbp0;
    UINT32 fbp1;
    CHECK_RC(m_pGpuSubdevice->GetHBMSiteFbps(hbmSite, &fbp0, &fbp1));

    auto isFbpAvailable = [fbpMask](UINT32 fbp) -> bool
        { return (fbpMask & (1U << fbp)) != 0; };

    // If any FBP on the site is active, consider the site available
    *pIsAvailable = isFbpAvailable(fbp0) || isFbpAvailable(fbp1);
    return rc;
}

/* virtual */ UINT32 FrameBuffer::VirtualLtcToHwLtc(UINT32 virtualLtc) const
{
    INT32 hwLtc = Utility::FindNthSetBit(GetLtcMask(), virtualLtc);
    MASSERT(hwLtc >= 0);
    return hwLtc;
}

/* virtual */ UINT32 FrameBuffer::HwLtcToVirtualLtc(UINT32 hwLtc) const
{
    const UINT32 ltcMask = GetLtcMask();
    MASSERT(((ltcMask >> hwLtc) & 1) == 1);
    return Utility::CountBits(ltcMask & ((1 << hwLtc) - 1));
}

/* virtual */
UINT32 FrameBuffer::VirtualL2SliceToHwL2Slice(UINT32 virtualLtc, UINT32 virtualL2Slice) const
{
    if (m_L2FSSlices == 0)
    {
        // LTS is not floorswept
        return virtualL2Slice;
    }
    else
    {
        const UINT32 hwLtc  = VirtualLtcToHwLtc(virtualLtc);
        const UINT32 hwFbio = HwLtcToHwFbio(hwLtc);
        const UINT32 hwFbp  = hwFbio / GetFbiosPerFbp();
        MASSERT(hwLtc >= 0 && hwLtc < m_MaxLtcCount);
        MASSERT(hwFbp >= 0 && hwFbp < m_MaxFbpCount);
        MASSERT(m_L2SliceMasks[hwFbp] != 0x0);

        const UINT32 hwLtcInFbp = hwLtc % GetMaxLtcsPerFbp();
        const UINT32 ltcSliceMask = (m_L2SliceMasks[hwFbp]) >> (hwLtcInFbp * GetMaxSlicesPerLtc());
        INT32 hwL2Slice = Utility::FindNthSetBit(ltcSliceMask, virtualL2Slice);
        MASSERT(ltcSliceMask != 0x0);
        MASSERT(hwL2Slice >= 0);
        return hwL2Slice;
    }
}

/* virtual */ UINT32 FrameBuffer::HwLtcToHwFbio(UINT32 hwLtc) const
{
    MASSERT(GetMaxLtcsPerFbp() % GetFbiosPerFbp() == 0);
    const UINT32 ltcsPerFbio = GetMaxLtcsPerFbp() / GetFbiosPerFbp();
    return hwLtc / ltcsPerFbio;
}

/* virtual */
UINT32 FrameBuffer::HwFbioToHwLtc(UINT32 hwFbio, UINT32 subpartition) const
{
    MASSERT(GetMaxLtcsPerFbp() % GetFbiosPerFbp() == 0);
    const UINT32 ltcsPerFbio = GetMaxLtcsPerFbp() / GetFbiosPerFbp();
    MASSERT(ltcsPerFbio == 1 || ltcsPerFbio == GetSubpartitions());
    if (ltcsPerFbio == 1)
    {
        return hwFbio;
    }
    else
    {
        // With 2 LTCs/FBIO, LTC maps to subpartition unless one is floorswept
        //
        const UINT32 fbioLtcMask = (GetLtcMask() >> (hwFbio * 2)) & 3;
        switch (fbioLtcMask)
        {
            case 1:
                return hwFbio * ltcsPerFbio;
            case 2:
                return hwFbio * ltcsPerFbio + 1;
            case 3:
                return hwFbio * ltcsPerFbio + subpartition;
            default:
                MASSERT(!"Both LTC bits floorswept in FBIO");
                return 0;
        }
    }
}

UINT32 FrameBuffer::VirtualFbioToVirtualLtc
(
    UINT32 virtualFbio,
    UINT32 subpartition
) const
{
    // Colwenience method for an operation that requires 3 colwersions otherwise
    const UINT32 hwFbio     = VirtualFbioToHwFbio(virtualFbio);
    const UINT32 hwLtc      = HwFbioToHwLtc(hwFbio, subpartition);
    const UINT32 virtualLtc = HwLtcToVirtualLtc(hwLtc);
    return virtualLtc;
}

//--------------------------------------------------------------------
//! \brief Helper function for printing an FbDecode struct
//!
//! Return a human-readable dump of the fields in an FbDecode struct,
//! with the format "FBIO=A Subpart=0 Bank=5..." Fields that don't
//! apply to the current board are omitted, such as rank on a
//! single-rank board or HBM fields on a non-HBM board.
//!
string FrameBuffer::GetDecodeString(const FbDecode &decode) const
{
    string str;

    str = Utility::StrPrintf("FBIO=%c Subpart=%x",
                             VirtualFbioToLetter(decode.virtualFbio),
                             decode.subpartition);
    if (IsHbm())
    {
        str += Utility::StrPrintf(" HbmSite=%x HbmChan=%c",
                                  decode.hbmSite, 'a' + decode.hbmChannel);
    }
    if (GetPseudoChannelsPerSubpartition() > 1)
    {
        str += Utility::StrPrintf(" PseudoCh=%x", decode.pseudoChannel);
    }
    if (GetRankCount() > 1)
    {
        str += Utility::StrPrintf(" %s=%x", IsHbm() ? "StackID" : "Rank",
                                  decode.rank);
    }
    str += Utility::StrPrintf(" Bank=%x Row=%x Col=%x Beat=%x BtOffset=%x",
                              decode.bank, decode.row, decode.extColumn,
                              decode.beat, decode.beatOffset);
    return str;
}

//--------------------------------------------------------------------
//! \brief Get the offset of a bit on the FBIO bus
//!
//! If we consider all the subpartitions and pseudochannels on an FBIO
//! to be one big N-bit bus, this method callwlates the offset of a
//! failing bit within that bus.
//!
//! The subpartition, pseudoChannel, and beatOffset parameters are
//! generally read from FbDecode, and represent the position of the
//! decoded address within a beat.  bitOffset represents the offset of
//! a bit within the decoded address.  It's permissible to pass the
//! decoded address of a 32-bit or 64-bit word even if the beat is
//! shorter (eg GDDR5x has 16-bit beats) as long as the word fits
//! within a burst (usually 128-256 bits); this method handles the
//! modulo operation.
//!
UINT32 FrameBuffer::GetFbioBitOffset
(
    UINT32 subpartition,
    UINT32 pseudoChannel,
    UINT32 beatOffset,
    UINT32 bitOffset
) const
{
    const UINT32 bitsPerPseudoCh = GetBeatSize() * 8;
    const UINT32 pseudoChPerSubp = GetPseudoChannelsPerSubpartition();
    return ((subpartition * pseudoChPerSubp + pseudoChannel) * bitsPerPseudoCh +
            (beatOffset * 8 + bitOffset) % bitsPerPseudoCh);
}

string FrameBuffer::GetGpuName(bool multiGpuOnly) const
{
    if (multiGpuOnly && DevMgrMgr::d_GraphDevMgr->NumGpus() == 1)
    {
        return "";
    }
    return m_pGpuSubdevice->GetName();
}

//------------------------------------------------------------------------------
//! \brief Return whether clamshell mode is enabled
//!
/* virtual */ bool FrameBuffer::IsClamshell() const
{
    // GDDR5X and GDDR6(X) technically support x16 and x8 (clamshell) modes
    // However, the register value here ignores the pseudochannels so it is
    // always called either x32 or x16 (clamshell) mode.
    const RegHal& regs = m_pGpuSubdevice->Regs();
    return regs.Test32(MODS_PFB_FBPA_CFG0_CLAMSHELL_X16);
}

//------------------------------------------------------------------------------
//! \brief Return whether half-fbpa is enabled on the specified virtual fbio
//!
/* virtual */ bool FrameBuffer::IsHalfFbpaEn(const UINT32 virtFbio) const
{
    const UINT32 halfFbpaStride = GetHalfFbpaStride();
    if (halfFbpaStride == 0)
    {
        return false;
    }
    // LW_PFB_FBPA_<IDX> registers are indexed by the HW FBPA index
    const UINT32 hwFbpa = HwFbioToHwFbpa(VirtualFbioToHwFbio(virtFbio));
    const bool halfFbpaEn = static_cast<bool>((GetHalfFbpaMask() >> (halfFbpaStride * hwFbpa)) &
                                              ((1u << halfFbpaStride) - 1));
    return halfFbpaEn;
}

/* virtual */ string FrameBuffer::GetRamProtocolString() const
{
    switch (GetRamProtocol())
    {
        case FrameBuffer::RamSdram:   return "SDRAM";
        case FrameBuffer::RamDDR1:    return "DDR1";
        case FrameBuffer::RamDDR2:    return "DDR2";
        case FrameBuffer::RamGDDR2:   return "GDDR2";
        case FrameBuffer::RamGDDR3:   return "GDDR3";
        case FrameBuffer::RamGDDR4:   return "GDDR4";
        case FrameBuffer::RamDDR3:    return "DDR3";
        case FrameBuffer::RamSDDR4:   return "SDDR4";
        case FrameBuffer::RamGDDR5:   return "GDDR5";
        case FrameBuffer::RamGDDR5X:  return "GDDR5X";
        case FrameBuffer::RamLPDDR2:  return "LPDDR2";
        case FrameBuffer::RamLPDDR3:  return "LPDDR3";
        case FrameBuffer::RamLPDDR4:  return "LPDDR4";
        case FrameBuffer::RamLPDDR5:  return "LPDDR5";
        case FrameBuffer::RamHBM1:    return "HBM1";
        case FrameBuffer::RamHBM2:    return "HBM2";
        case FrameBuffer::RamHBM3:    return "HBM3";
        case FrameBuffer::RamGDDR6:   return "GDDR6";
        case FrameBuffer::RamGDDR6X:  return "GDDR6X";
        case FrameBuffer::RamUnknown: return "Unknown type";
        default:                      return "Unrecognized type";
    }
}

//------------------------------------------------------------------------------
//! \brief Return true if the GPU uses HBM memory
//!
//! This method should work during early initialization, before the
//! FrameBuffer object has been instantiated.
//!
/* static */ bool FrameBuffer::IsHbm(GpuSubdevice* pGpuSubdevice)
{
    if (pGpuSubdevice->GetFB() != nullptr)
    {
        // If the FrameBuffer object has already been created, use it
        //
        return pGpuSubdevice->GetFB()->IsHbm();
    }
    else
    {
        // Otherwise, read registers to determine whether this is HBM
        //
        const RegHal& regs = pGpuSubdevice->Regs();
        const UINT32  ddr  = regs.Read32(MODS_PFB_FBPA_FBIO_BROADCAST);
        return regs.TestField(ddr, MODS_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_HBM1)
            || regs.TestField(ddr, MODS_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_HBM2)
            || regs.TestField(ddr, MODS_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_HBM3);
    }
}

UINT32 FrameBuffer::GetL2SlicesPerFbp(UINT32 virtualFbp) const
{
    MASSERT(virtualFbp >= 0 && virtualFbp < GetFbpCount());
    const UINT32 hwFbp = VirtualFbpToHwFbp(virtualFbp);
    MASSERT(hwFbp >= 0 && hwFbp < GetMaxFbpCount());
    MASSERT(m_L2SliceMasks[hwFbp] != 0x0);
    return Utility::CountBits(m_L2SliceMasks[hwFbp]);
}

//------------------------------------------------------------------------------
// The number of slices per LTC vary from GPU to GPU. For example
// On Turing there are 4 slices per LTC and 2 LTCs per FBP.
// On Volta there are 2 slices per LTC and 2 LTCs per FBP.
// We can floorsweep one of the LTCs which would ilwalidate 2 or 4 of the 4 or 8 slices that would
// be available depending on the GPU. To determine if a particular slice is valid we need to look
// at the L2Mask to see what has been floorswept.
// For example on Turing the following masks would indicate that FB3 slice 0-3 & FB4 slice
// 4-7 are not valid, and on Volta it wold indicate that FB3 slice 0-1 & FB4 slice 2-3 are not
// valid.
// FB   Mask      : 0x3f (6 FB Partitions)
// ROP/L2 Mask    : [3 1 2 3 3 3] (10 ROP/L2s)
bool FrameBuffer::IsL2SliceValid(UINT32 hwSlice, UINT32 virtualFbp)
{
    if (virtualFbp < GetFbpCount())
    {
        // Colwert virtual FBP to physical FBP
        UINT32 hwFbp = VirtualFbpToHwFbp(virtualFbp);
        MASSERT(m_L2SliceMasks[hwFbp] != 0x0);
        // Check if HW L2 slice is present in mask
        return m_L2SliceMasks[hwFbp] & BIT(hwSlice);
    }
    return false;
}
