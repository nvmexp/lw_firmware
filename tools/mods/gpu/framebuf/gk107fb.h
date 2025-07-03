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

#pragma once
#ifndef INCLUDED_GK107FB_H
#define INCLUDED_GK107FB_H

#include "gf100fb.h"
#include <memory>
class GK107AddressDecode;

class GK107FrameBuffer : public GF100FrameBuffer
{
public:
    explicit GK107FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm);
    virtual ~GK107FrameBuffer();
    virtual RC Initialize();

public:
    virtual RC DecodeAddress
    (
        FbDecode *pDecode,
        UINT64 physicalFbOffset,
        UINT32 pteKind,
        UINT32 pageSizeKB
    ) const;
    void EnableDecodeDetails(bool enable);
protected:
    GK107FrameBuffer
    (
        const char*   derivedClassName,
        GpuSubdevice* pGpuSubdevice,
        LwRm*         pLwRm
    );
private:
    unique_ptr<GK107AddressDecode> m_Decoder;
};

#endif // ! INCLUDED_GK107FB_H
