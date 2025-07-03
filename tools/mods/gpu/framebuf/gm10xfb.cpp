/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gm10xfb.h"
#include "gmlit1adr.h"
#include "maxwell/gm107/dev_fuse.h"
#include "maxwell/gm107/dev_top.h"
#include "maxwell/gm107/dev_fbpa.h"
#include "maxwell/gm107/hwproject.h"
#include "maxwell/gm107/dev_ltc.h"

#ifndef MATS_STANDALONE
#include "gpu/include/floorsweepimpl.h"
#endif

/* virtual */ GM10XFrameBuffer::~GM10XFrameBuffer()
{
}

GM10XFrameBuffer::GM10XFrameBuffer
(
    GpuSubdevice* pGpuSubdevice,
    LwRm*         pLwRm
) :
    GF100FrameBuffer("GM10X FrameBuffer", pGpuSubdevice, pLwRm)
{
}

GM10XFrameBuffer::GM10XFrameBuffer
(
    const char*   derivedClassName,
    GpuSubdevice* pGpuSubdevice,
    LwRm*         pLwRm
) :
    GF100FrameBuffer(derivedClassName, pGpuSubdevice, pLwRm)
{
}

//------------------------------------------------------------------------------
/* virtual */ RC GM10XFrameBuffer::Initialize()
{
    RC rc;

    CHECK_RC(InitCommon());
    m_Decoder.reset(new GMLIT1AddressDecode(GpuSub()));
    m_Encoder.reset(new GMLIT1AddressEncode(GpuSub()));

    return OK;
}

//------------------------------------------------------------------------------
void GM10XFrameBuffer::EnableDecodeDetails(bool enable)
{
    MASSERT(m_Decoder.get() != 0);
    if (m_Decoder.get() != 0)
    {
        m_Decoder->EnableDecodeDetails(enable);
    }
}

//------------------------------------------------------------------------------

/* virtual */ RC GM10XFrameBuffer::DecodeAddress
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

/* virtual */ RC GM10XFrameBuffer::EncodeAddress
(
    const FbDecode &decode,
    UINT32 pteKind,
    UINT32 pageSize,
    UINT64 *pEccOnAddr,
    UINT64 *pEccOffAddr
) const
{
    MASSERT(m_Encoder.get() != nullptr);

    *pEccOnAddr  = m_Encoder.get()->EncodeAddress(decode, pteKind,
                                                  pageSize, true);
    *pEccOffAddr = m_Encoder.get()->EncodeAddress(decode, pteKind,
                                                  pageSize, false);
    // Check if someone implemented m_Encoder yet
    MASSERT(0 == *pEccOnAddr);
    MASSERT(0 == *pEccOffAddr);

    return OK;
}

//------------------------------------------------------------------------------
FrameBuffer* CreateGM10XFrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm)
{
    return new GM10XFrameBuffer(pGpuSubdevice, pLwRm);
}
