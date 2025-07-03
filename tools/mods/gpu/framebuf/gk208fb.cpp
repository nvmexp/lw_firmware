/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2012,2014,2016-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "gk208fb.h"
#include "gk208adr.h"
#include "kepler/gk208s/dev_fbpa.h"

/* virtual */ GK208FrameBuffer::~GK208FrameBuffer()
{
}

GK208FrameBuffer::GK208FrameBuffer
(
    GpuSubdevice* pGpuSubdevice,
    LwRm*         pLwRm
)
: GF100FrameBuffer("GK208 FrameBuffer", pGpuSubdevice, pLwRm)
{
}

GK208FrameBuffer::GK208FrameBuffer
(
    const char*   derivedClassName,
    GpuSubdevice* pGpuSubdevice,
    LwRm*         pLwRm
)
: GF100FrameBuffer(derivedClassName, pGpuSubdevice, pLwRm)
{
}

/* virtual */ RC GK208FrameBuffer::Initialize()
{
    RC rc;
    CHECK_RC(GF100FrameBuffer::Initialize());
    m_Decoder.reset(new GK208AddressDecode(GetFbioCount(), GpuSub()));
    return OK;
}

/* virtual */ UINT32 GK208FrameBuffer::GetSubpartitions() const
{
    static const UINT32 channelEnable =
        DRF_VAL(_PFB, _FBPA_MC_1_FBIO_CHANNELCFG, _CHANNEL_ENABLE,
            GpuSub()->RegRd32(LW_PFB_FBPA_MC_1_FBIO_CHANNELCFG));
    return channelEnable == LW_PFB_FBPA_MC_1_FBIO_CHANNELCFG_CHANNEL_ENABLE_64_BIT ?
            2 : 1;
}

/* virtual */ RC GK208FrameBuffer::DecodeAddress
(
    FbDecode* pDecode,
    UINT64 physicalFbOffset,
    UINT32 pteKind,
    UINT32 pageSize
) const
{
    MASSERT(m_Decoder.get() != nullptr);
    RC rc;
    if (m_Decoder.get() == nullptr)
        return RC::UNSUPPORTED_FUNCTION;
    CHECK_RC(m_Decoder->DecodeAddress(pDecode, physicalFbOffset,
                                      pteKind, pageSize));
    return OK;
}

FrameBuffer* CreateGK208FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm)
{
    return new GK208FrameBuffer(pGpuSubdevice, pLwRm);
}
