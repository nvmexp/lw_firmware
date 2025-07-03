/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011,2014-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gk107fb.h"
#include "gk107adr.h"

/* virtual */ GK107FrameBuffer::~GK107FrameBuffer()
{
}

GK107FrameBuffer::GK107FrameBuffer
(
    GpuSubdevice* pGpuSubdevice,
    LwRm*         pLwRm
)
: GF100FrameBuffer("GK107 FrameBuffer", pGpuSubdevice, pLwRm)
{
}

GK107FrameBuffer::GK107FrameBuffer
(
    const char*   derivedClassName,
    GpuSubdevice* pGpuSubdevice,
    LwRm*         pLwRm
)
: GF100FrameBuffer(derivedClassName, pGpuSubdevice, pLwRm)
{
}

/* virtual */ RC GK107FrameBuffer::Initialize()
{
    RC rc;
    CHECK_RC(GF100FrameBuffer::Initialize());
    m_Decoder.reset(new GK107AddressDecode(GetFbioCount(), GpuSub()));
    return OK;
}

void GK107FrameBuffer::EnableDecodeDetails(bool enable)
{
    m_Decoder->EnableDecodeDetails(enable);
}

/* virtual */ RC GK107FrameBuffer::DecodeAddress
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

FrameBuffer* CreateGK107FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm)
{
    return new GK107FrameBuffer(pGpuSubdevice, pLwRm);
}
