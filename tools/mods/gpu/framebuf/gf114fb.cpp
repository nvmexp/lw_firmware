/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2012,2014-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// GF114 frame buffer interface.
#include "gf114fb.h"
#include "gf100adr.h"

#include "fermi/gf104b/dev_fbpa.h"
#include "fermi/gf104b/dev_top.h"
#include "fermi/gf104b/hwproject.h"

/* virtual */ GF114FrameBuffer::~GF114FrameBuffer()
{
}

GF114FrameBuffer::GF114FrameBuffer
(
    GpuSubdevice* pGpuSubdevice,
    LwRm*         pLwRm
)
: GF100FrameBuffer("GF114 FrameBuffer", pGpuSubdevice, pLwRm)
{
}

GF114FrameBuffer::GF114FrameBuffer
(
    const char*   derivedClassName,
    GpuSubdevice* pGpuSubdevice,
    LwRm*         pLwRm
)
: GF100FrameBuffer(derivedClassName, pGpuSubdevice, pLwRm)
{
}

/* virtual */ RC GF114FrameBuffer::Initialize()
{
    RC rc;
    CHECK_RC(GF100FrameBuffer::Initialize());

    // Initialize special address decoder for the "slow" partition
    m_SlowPartitionDecoder.reset(new GF100AddressDecode(1, GpuSub()));

    return OK;
}

void GF114FrameBuffer::EnableDecodeDetails(bool enable)
{
    GF100FrameBuffer::EnableDecodeDetails(enable);
    m_SlowPartitionDecoder->EnableDecodeDetails(enable);
}

/* virtual */ RC GF114FrameBuffer::DecodeAddress
(
    FbDecode* pDecode,
    UINT64 physicalFbOffset,
    UINT32 pteKind,
    UINT32 pageSize
) const
{
    // If the physical address is 8GB or higher, we hit the slow partition
    // so use the special 1-partition address decode...
    const UINT64 eightGB = 0x200000000ULL;
    RC rc;
    if (physicalFbOffset >= eightGB)
    {
        MASSERT(m_SlowPartitionDecoder.get() != 0);
        if (m_SlowPartitionDecoder.get() != 0)
        {
            CHECK_RC(m_SlowPartitionDecoder->DecodeAddress(
                            pDecode, physicalFbOffset - eightGB,
                            pteKind, pageSize));
        }
    }
    // ...otherwise use regular address decode.
    else
    {
        CHECK_RC(GF100FrameBuffer::DecodeAddress(pDecode, physicalFbOffset,
                                                 pteKind, pageSize));
    }
    return OK;
}

FrameBuffer* CreateGF114FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm)
{
    return new GF114FrameBuffer(pGpuSubdevice, pLwRm);
}

