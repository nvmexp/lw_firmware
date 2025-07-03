/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gklit1fb.h"
#include "gklit1adr.h"
#include "kepler/gk104/dev_top.h"
#include <bitset>

#define MAX_SUPPORTED_NUM_FBP 4

/* virtual */ GKLIT1FrameBuffer::~GKLIT1FrameBuffer()
{
}

GKLIT1FrameBuffer::GKLIT1FrameBuffer
(
    GpuSubdevice* pGpuSubdevice,
    LwRm*         pLwRm
)
: GF100FrameBuffer("GKLIT1 FrameBuffer", pGpuSubdevice, pLwRm)
{
}

GKLIT1FrameBuffer::GKLIT1FrameBuffer
(
    const char*   derivedClassName,
    GpuSubdevice* pGpuSubdevice,
    LwRm*         pLwRm
)
: GF100FrameBuffer(derivedClassName, pGpuSubdevice, pLwRm)
{
}

/* virtual */ RC GKLIT1FrameBuffer::Initialize()
{
    RC rc;
    CHECK_RC(GF100FrameBuffer::Initialize());
    m_Decoder.reset(new GKLIT1AddressDecode(GetFbioCount(), GpuSub()));
    return OK;
}

void GKLIT1FrameBuffer::EnableDecodeDetails(bool enable)
{
    m_Decoder->EnableDecodeDetails(enable);
}

/* virtual */ RC GKLIT1FrameBuffer::DecodeAddress
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

FrameBuffer* CreateGKLIT1FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm)
{
    return new GKLIT1FrameBuffer(pGpuSubdevice, pLwRm);
}
