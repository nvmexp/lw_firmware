/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gm20xfb.h"
#include "gmlit2adr.h"
#include "maxwell/gm204/dev_fuse.h"
#include "maxwell/gm204/dev_top.h"
#include "maxwell/gm204/dev_fbpa.h"
#include "maxwell/gm204/hwproject.h"
#include "maxwell/gm204/dev_ltc.h"

#ifndef MATS_STANDALONE
#include "core/include/platform.h"
#include "gpu/include/floorsweepimpl.h"
#endif

/* virtual */ GM20XFrameBuffer::~GM20XFrameBuffer()
{
}

GM20XFrameBuffer::GM20XFrameBuffer
(
    GpuSubdevice* pGpuSubdevice,
    LwRm*         pLwRm
) :
    GF100FrameBuffer("GM20X FrameBuffer", pGpuSubdevice, pLwRm)
{
}

GM20XFrameBuffer::GM20XFrameBuffer
(
    const char*   derivedClassName,
    GpuSubdevice* pGpuSubdevice,
    LwRm*         pLwRm
) :
    GF100FrameBuffer(derivedClassName, pGpuSubdevice, pLwRm)
{
}

//------------------------------------------------------------------------------
/* virtual */ RC GM20XFrameBuffer::Initialize()
{
    RC rc;

    CHECK_RC(InitCommon());
    m_Decoder.reset(new GMLIT2AddressDecode(GpuSub()));
    m_Encoder.reset(new GMLIT2AddressEncode(GpuSub()));
    return OK;
}

//------------------------------------------------------------------------------
void GM20XFrameBuffer::EnableDecodeDetails(bool enable)
{
    MASSERT(m_Decoder.get() != 0);
    if (m_Decoder.get() != 0)
    {
        m_Decoder->EnableDecodeDetails(enable);
    }
}

//------------------------------------------------------------------------------

/* virtual */ RC GM20XFrameBuffer::DecodeAddress
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

//------------------------------------------------------------------------------
/* virtual */ RC GM20XFrameBuffer::EncodeAddress
(
    const FbDecode &decode,
    UINT32 pteKind,
    UINT32 pageSize,
    UINT64 *pEccOnAddr,
    UINT64 *pEccOffAddr
) const
{
    MASSERT(m_Encoder.get() != nullptr);
    if (m_Encoder.get() == nullptr)
        return RC::UNSUPPORTED_FUNCTION;
    *pEccOnAddr  = m_Encoder.get()->EncodeAddress(decode, pteKind,
                                                  pageSize, true);
    *pEccOffAddr = m_Encoder.get()->EncodeAddress(decode, pteKind,
                                                  pageSize, false);
    return OK;
}

//------------------------------------------------------------------------------
FrameBuffer* CreateGM20XFrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm)
{
    return new GM20XFrameBuffer(pGpuSubdevice, pLwRm);
}
