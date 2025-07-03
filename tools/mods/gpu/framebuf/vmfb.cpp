/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "vmfb.h"
#include "core/include/platform.h"
#include "core/include/rc.h"
#include "gpu/include/gpusbdev.h"
#include "ctrl/ctrl2080.h"

//--------------------------------------------------------------------
//! \brief Initialize the FrameBuffer for virtual machines
//!
RC VmFrameBuffer::Initialize()
{
    RC rc;

    GpuSubdevice* pSubdev = GpuSub();

    m_FbpMask         = 0x1;
    m_MaxFbpCount     = 1;
    m_FbiosPerFbp     = 1;
    m_FbioMask        = 0x1;
    m_MaxFbioCount    = 1;
    m_MaxLtcsPerFbp   = 1;
    m_LtcMask         = 0x1;
    m_MaxLtcCount     = 1;
    m_MaxSlicesPerLtc = 1;
    m_L2SliceSize     = 0;

    // Framebuffer size
    //
    LW2080_CTRL_FB_INFO            fbInfo = {};
    LW2080_CTRL_FB_GET_INFO_PARAMS params = {};
    fbInfo.index          = LW2080_CTRL_FB_INFO_INDEX_RAM_SIZE;
    params.fbInfoListSize = 1;
    params.fbInfoList     = LW_PTR_TO_LwP64(&fbInfo);
    CHECK_RC(GetRmClient()->ControlBySubdevice(pSubdev,
                                               LW2080_CTRL_CMD_FB_GET_INFO,
                                               &params, sizeof(params)));
    m_FbSize = static_cast<UINT64>(fbInfo.data) * 1024;

    // Break the framebuffer into rows, so that the address bits are
    // split roughly evenly between the row number and row offset.
    //
    if (m_FbSize == 0)
    {
        m_RowBits = 0;
        m_RowSize = 0;
    }
    else
    {
        UINT32 fbBits = Utility::BitScanReverse64(m_FbSize);
        if (m_FbSize > (1ULL << fbBits))
        {
            ++fbBits;
        }
        MASSERT(m_FbSize <= (1ULL << fbBits));

        m_RowBits = (fbBits + 1) / 2;
        m_RowSize = 1 << (fbBits - m_RowBits);

        MASSERT((static_cast<UINT64>(m_RowSize) << m_RowBits) >= m_FbSize);
        MASSERT((static_cast<UINT64>(m_RowSize) << m_RowBits) < 2 * m_FbSize);
    }

    // Is ECC supported?
    //
    bool   isEccSupported = false;
    UINT32 eccMask        = 0;
    m_IsEccOn = (pSubdev->GetEccEnabled(&isEccSupported, &eccMask) == RC::OK &&
                 isEccSupported && Ecc::IsUnitEnabled(Ecc::Unit::DRAM, eccMask));

    // Is row remapping on?
    //
    if (pSubdev->HasFeature(Device::Feature::GPUSUB_SUPPORTS_ROW_REMAPPING))
    {
        MASSERT(Platform::IsVirtFunMode());

        // Check if the feature is available
        bool isRowRemappingFeatureEnabled = false;
        CHECK_RC(pSubdev->Test32PfRegFromVf(MODS_FUSE_FEATURE_READOUT_2_ROW_REMAPPER_ENABLED,
                                            &m_IsRowRemappingOn));

        // Check if the feature is enabled
        m_IsRowRemappingOn = false;
        if (isRowRemappingFeatureEnabled)
        {
            CHECK_RC(pSubdev->Test32PfRegFromVf(MODS_PFB_FBPA_ROW_REMAP_ENABLE_TRUE,
                                                &m_IsRowRemappingOn));
        }
    }
    else
    {
        m_IsRowRemappingOn = false;
    }

    return rc;
}

RC VmFrameBuffer::GetFbRanges(FbRanges* pRanges) const
{
    FbRange fbRange;
    fbRange.start = 0;
    fbRange.size  = m_FbSize;
    pRanges->clear();
    pRanges->push_back(fbRange);
    return OK;
}

bool VmFrameBuffer::IsSOC() const
{
    return GpuSub()->IsSOC();
}

RC VmFrameBuffer::EncodeAddress
(
    const FbDecode& decode,
    UINT32  pteKind,
    UINT32  pageSizeKB,
    UINT64* pEccOnAddr,
    UINT64* pEccOffAddr
) const
{
    MASSERT(pEccOnAddr);
    MASSERT(pEccOffAddr);
    *pEccOnAddr  = (static_cast<UINT64>(decode.row) * m_RowSize +
                    decode.burst * 4 +
                    decode.beatOffset);
    *pEccOffAddr = *pEccOnAddr;
    return OK;
}

RC VmFrameBuffer::DecodeAddress
(
    FbDecode* pDecode,
    UINT64 fbOffset,
    UINT32 pteKind,
    UINT32 pageSizeKB
) const
{
    MASSERT(pDecode);
    memset(pDecode, 0, sizeof(*pDecode));
    pDecode->row        = static_cast<UINT32>(fbOffset / m_RowSize);
    pDecode->burst      = static_cast<UINT32>(fbOffset % m_RowSize) / 4;
    pDecode->beatOffset = fbOffset % 4;
    pDecode->extColumn  = pDecode->burst;
    return OK;
}

void VmFrameBuffer::GetRBCAddress
(
    EccAddrParams* pRbcAddr,
    UINT64 fbOffset,
    UINT32 pteKind,
    UINT32 pageSize,
    UINT32 errCount,
    UINT32 errPos
)
{
    MASSERT(pRbcAddr);
    FbDecode decode;
    DecodeAddress(&decode, fbOffset, pteKind, pageSize);
    memset(pRbcAddr, 0, sizeof(*pRbcAddr));
    pRbcAddr->eccAddr    = decode.burst;
    pRbcAddr->eccAddrExt = decode.row;
}

void VmFrameBuffer::GetEccInjectRegVal
(
    EccErrInjectParams* pInjectParams,
    UINT64 fbOffset,
    UINT32 pteKind,
    UINT32 pageSize,
    UINT32 errCount,
    UINT32 errPos
)
{
    Printf(Tee::PriWarn,
           "ECC Error injection not supported in virtual machines\n");
    memset(pInjectParams, 0, sizeof(*pInjectParams));
}
