/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gk110fb.h"
#include "gklit2adr.h"
#include "kepler/gk110/dev_ltc.h"
#include "kepler/gk110/dev_fbpa.h"
#include "kepler/gk110/hwproject.h"

#ifndef MATS_STANDALONE
#include "gpu/include/floorsweepimpl.h"
#endif

/* virtual */ GK110FrameBuffer::~GK110FrameBuffer()
{
}

GK110FrameBuffer::GK110FrameBuffer
(
    GpuSubdevice* pGpuSubdevice,
    LwRm*         pLwRm
)
: GF100FrameBuffer("GK110 FrameBuffer", pGpuSubdevice, pLwRm)
{
}

GK110FrameBuffer::GK110FrameBuffer
(
    const char*   derivedClassName,
    GpuSubdevice* pGpuSubdevice,
    LwRm*         pLwRm
)
: GF100FrameBuffer(derivedClassName, pGpuSubdevice, pLwRm)
{
}

/* virtual */ RC GK110FrameBuffer::Initialize()
{
    RC rc;
    CHECK_RC(GF100FrameBuffer::Initialize());
    m_Decoder.reset(new GKLIT2AddressDecode(GetFbioCount(), GpuSub()));
    m_Encoder.reset(new GKLIT2AddressEncode(GetFbioCount(), GpuSub()));
    return OK;
}

/* virtual */ RC GK110FrameBuffer::DecodeAddress
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

RC GK110FrameBuffer::EncodeAddress
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

FrameBuffer* CreateGK110FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm)
{
    return new GK110FrameBuffer(pGpuSubdevice, pLwRm);
}
