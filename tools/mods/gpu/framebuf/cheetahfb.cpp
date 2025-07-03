/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "tegrafb.h"
#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"
#include "core/include/platform.h"
#include "core/include/gpu.h"
#include "gpu/include/gpusbdev.h"

TegraFrameBuffer::TegraFrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm) :
    FrameBuffer("CheetAh FrameBuffer", pGpuSubdevice, pLwRm)
{
    if (Platform::IsTegra())
    {
        const UINT32 dramType = CheetAh::SocPtr()->GetRAMType();
        switch (dramType)
        {
            case CheetAh::SocDevice::RAM_DDR1:
                m_RamProtocol = RamDDR1;
                break;
            case CheetAh::SocDevice::RAM_DDR2:
                m_RamProtocol = RamDDR2;
                break;
            case CheetAh::SocDevice::RAM_LPDDR2:
                m_RamProtocol = RamLPDDR2;
                break;
            case CheetAh::SocDevice::RAM_DDR3:
                m_RamProtocol = RamDDR3;
                break;
            case CheetAh::SocDevice::RAM_LPDDR3:
                m_RamProtocol = RamLPDDR3;
                break;
            case CheetAh::SocDevice::RAM_LPDDR4:
                m_RamProtocol = RamLPDDR4;
                break;
            case CheetAh::SocDevice::RAM_LPDDR5:
                m_RamProtocol = RamLPDDR5;
                break;
            default:
                m_RamProtocol = RamUnknown;
                break;
        }
    }
    else
    {
        m_RamProtocol = RamUnknown;
    }
}

/* virtual */ TegraFrameBuffer::~TegraFrameBuffer()
{
}

/* virtual */ RC TegraFrameBuffer::GetFbRanges(FbRanges *pRanges) const
{
    const PHYSADDR tegraPhysBase = 0x80000000U; // 2GB

    pRanges->resize(1);
    (*pRanges)[0].start = tegraPhysBase;
    (*pRanges)[0].size  = tegraPhysBase + GetGraphicsRamAmount();
    return OK;
}

/* virtual */ UINT64 TegraFrameBuffer::GetGraphicsRamAmount() const
{
    if (Platform::IsTegra())
    {
        return CheetAh::SocPtr()->GetRAMAmountMB() * (1024ULL*1024ULL);
    }
    else
    {
        return 0;
    }
}

/* virtual */ bool TegraFrameBuffer::IsSOC() const
{
    return true;
}

/* virtual */ UINT32 TegraFrameBuffer::GetSubpartitions() const
{
    if (CheetAh::IsInitialized())
    {
        return CheetAh::SocPtr()->GetSubpartitions();
    }
    // mdiag doesn't initialize CheetAh, avoid null pointer dereference
    else
    {
        return 1;
    }
}

/* virtual */ UINT32 TegraFrameBuffer::GetBusWidth() const
{
    if (CheetAh::IsInitialized())
    {
        return CheetAh::SocPtr()->GetRAMWidth();
    }
    // mdiag doesn't initialize CheetAh, avoid null pointer dereference
    else
    {
        return 64;
    }
}

/* virtual */ UINT32 TegraFrameBuffer::VirtualFbpToHwFbp(UINT32 fbp) const
{
    return fbp;
}

/* virtual */ UINT32 TegraFrameBuffer::HwFbpToVirtualFbp(UINT32 fbp) const
{
    return fbp;
}

/* virtual */ UINT32 TegraFrameBuffer::VirtualFbioToHwFbio(UINT32 fbio) const
{
    return fbio;
}

/* virtual */ UINT32 TegraFrameBuffer::HwFbioToVirtualFbio(UINT32 fbio) const
{
    return fbio;
}

/* virtual */ UINT32 TegraFrameBuffer::VirtualLtcToHwLtc(UINT32 ltc) const
{
    return ltc;
}

/* virtual */ UINT32 TegraFrameBuffer::HwLtcToVirtualLtc(UINT32 ltc) const
{
    return ltc;
}

/* virtual */ UINT32 TegraFrameBuffer::HwLtcToHwFbio(UINT32 ltc) const
{
    return ltc;
}

/* virtual */
UINT32 TegraFrameBuffer::HwFbioToHwLtc(UINT32 fbio, UINT32 subpartition) const
{
    return 0;
}

/* virtual */ UINT32 TegraFrameBuffer::VirtualL2SliceToHwL2Slice(UINT32 ltc, UINT32 l2Slice) const
{
    return l2Slice;
}

/* virtual */ UINT32 TegraFrameBuffer::GetPseudoChannelsPerChannel() const
{
    return 1;
}

/* virtual */ bool   TegraFrameBuffer::IsEccOn() const
{
    return false;
}

/* virtual */ bool   TegraFrameBuffer::IsRowRemappingOn() const
{
    return false;
}

/* virtual */ UINT32 TegraFrameBuffer::GetRankCount() const
{
    return CheetAh::IsInitialized() ? CheetAh::SocPtr()->GetRanks() : 1;
}

/* virtual */ UINT32 TegraFrameBuffer::GetBankCount() const
{
    return 0;  // Not used yet for SOC
}

/* virtual */ UINT32 TegraFrameBuffer::GetRowBits() const
{
    return 0;  // Not used yet for SOC
}

/* virtual */ UINT32 TegraFrameBuffer::GetRowSize() const
{
    return 0;  // Not used yet for SOC
}

/* virtual */ UINT32 TegraFrameBuffer::GetBurstSize() const
{
    return CheetAh::SocPtr()->GetBurstSize();
}

/* virtual */ UINT32 TegraFrameBuffer::GetBeatSize() const
{
    return CheetAh::SocPtr()->GetBeatSize();
}

/* virtual */ UINT32 TegraFrameBuffer::GetDataRate() const
{
    return 2; // DDR
}

/* virtual */ RC TegraFrameBuffer::Initialize()
{
    return InitFbpInfo();
}

/* virtual */ RC TegraFrameBuffer::InitFbpInfo()
{
    // In cases when we are running MODS on CheetAh without a GPU, set the members
    // to some reasonable values
    //
    if (GpuSub()->IsFakeTegraGpu())
    {
        m_MaxFbpCount     = 1;
        m_FbpMask         = 1;
        m_MaxFbioCount    = 1;
        m_FbiosPerFbp     = 1;
        m_FbioMask        = 1;
        m_MaxLtcsPerFbp   = 1;
        m_MaxLtcCount     = 1;
        m_LtcMask         = 1;
        m_MaxSlicesPerLtc = 1;
        m_L2SliceSize  = 0x20000;
        return OK;
    }

    RC rc;
    const RegHal &regs = GpuSub()->Regs();

    // Get FBP config
    //
    m_MaxFbpCount = regs.LookupAddress(MODS_SCAL_LITTER_NUM_FBPS);
    m_FbpMask     = (1 << m_MaxFbpCount) - 1;
    MASSERT(m_MaxFbpCount > 0);

    // Set FBIO config
    // CheetAh doesn't have FBPAs/FBIOs, but we use the CheetAh channels in their place
    // since they perform a similar function.
    //
    m_MaxFbioCount = CheetAh::IsInitialized()
                         ? CheetAh::SocPtr()->GetChannels()
                         // mdiag doesn't initialize CheetAh, avoid null pointer dereference
                         :1;
    m_FbiosPerFbp  = m_MaxFbioCount / m_MaxFbpCount;
    m_FbioMask     = (1 << m_MaxFbioCount) - 1;

    // Get LTC config
    // Can't use PTOP registers in CheetAh, so use LITTER values from hwproject.h
    //
    m_MaxLtcsPerFbp =
        regs.IsSupported(MODS_SCAL_LITTER_NUM_LTC_PER_FBP)
            ? regs.LookupAddress(MODS_SCAL_LITTER_NUM_LTC_PER_FBP)
            // Kepler chips have 1 LTC per FBP
            : 1;
    m_MaxLtcCount = m_MaxLtcsPerFbp * m_MaxFbpCount;
    m_LtcMask     = (1 << m_MaxLtcCount) - 1;
    MASSERT(m_MaxLtcsPerFbp > 0);

    // Get L2 Slice config
    // Can't use PTOP registers in CheetAh, so use LITTER values from hwproject.h
    //
    m_MaxSlicesPerLtc =
        regs.IsSupported(MODS_SCAL_LITTER_NUM_SLICES_PER_LTC)
            ? regs.LookupAddress(MODS_SCAL_LITTER_NUM_SLICES_PER_LTC)
            // We have to use a different register for Kepler chips
            // since they only ever have 1 LTC / FBP
            : regs.LookupAddress(MODS_SCAL_LITTER_NUM_LTC_SLICES);

    m_L2SliceSize = regs.LookupAddress(MODS_CHIP_LTC_LTS_NUM_SETS)        *
                    regs.LookupAddress(MODS_SCAL_LITTER_NUM_LTC_LTS_WAYS) *
                    regs.LookupAddress(MODS_CHIP_LTC_LTS_BYTES_PER_LINE);
    MASSERT(m_MaxSlicesPerLtc > 0);
    MASSERT(m_L2SliceSize > 0);

    // If there is no L2Slice floorsweeping each LTC has the maximum number of slices
    // LtcMask is already inited so it is safe to get the count
    MASSERT(m_LTCToLTSMap.size() == 0);
    for (UINT32 ltcIndex = 0; ltcIndex < m_MaxLtcCount; ltcIndex++)
    {
        m_LTCToLTSMap.push_back(m_MaxSlicesPerLtc);
    }

    // Initialize L2 slice masks
    const UINT32 sliceMask = (1u << (m_MaxSlicesPerLtc * m_MaxLtcsPerFbp)) - 1;
    m_L2SliceMasks.resize(m_MaxFbpCount, sliceMask);

    return rc;
}

/* virtual */ RC TegraFrameBuffer::DecodeAddress
(
    FbDecode* pDecode,
    UINT64 fbOffset,
    UINT32 pteKind,
    UINT32 pageSizeKB
) const
{
    RC rc;
    CHECK_RC(CheetAh::SocPtr()->DecodeAddress(fbOffset, pDecode));
    return OK;
}

void TegraFrameBuffer::GetRBCAddress
(
   EccAddrParams *pRbcAddr,
   UINT64 fbOffset,
   UINT32 pteKind,
   UINT32 pageSize,
   UINT32 errCount,
   UINT32 errPos
)
{
    Printf(Tee::PriWarn,
           "Running Ecc test is not supported for current chip\n");
    memset((void *)pRbcAddr, 0, sizeof(EccAddrParams));
}

void TegraFrameBuffer::GetEccInjectRegVal
(
   EccErrInjectParams *pInjectParams,
   UINT64 fbOffset,
   UINT32 pteKind,
   UINT32 pageSize,
   UINT32 errCount,
   UINT32 errPos
)
{
    Printf(Tee::PriWarn,
           "Running Ecc test is not supported for current chip\n");
    memset((void *)pInjectParams, 0, sizeof(EccAddrParams));
}

FrameBuffer* CreateTegraFrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm)
{
    return new TegraFrameBuffer(pGpuSubdevice, pLwRm);
}
