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

#pragma once
#ifndef INCLUDED_GF114FB_H
#define INCLUDED_GF114FB_H

#include "gf100fb.h"
#include <memory>
class GF100AddressDecode;

/**
 * @class( GF114FrameBuffer )
 *
 * GF114/GF116 frame buffer device, based on GF100FrameBuffer.
 */

class GF114FrameBuffer : public GF100FrameBuffer
{
public:
    explicit       GF114FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm);
    virtual        ~GF114FrameBuffer();
    virtual RC     Initialize();
    virtual RC     DecodeAddress
    (
        FbDecode *pDecode,
        UINT64 physicalFbOffset,
        UINT32 pteKind,
        UINT32 pageSizeKB
    ) const;
    void EnableDecodeDetails(bool enable);

protected:
    //! Constructor for use by derived classes (overrides Name).
    GF114FrameBuffer
    (
        const char*   derivedClassName,
        GpuSubdevice* pGpuSubdevice,
        LwRm*         pLwRm
    );

private:
    unique_ptr<GF100AddressDecode> m_SlowPartitionDecoder;
};

#endif // INCLUDED_GF114FB_H
