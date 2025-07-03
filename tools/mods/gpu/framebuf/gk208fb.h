/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#ifndef INCLUDED_GK208FB_H
#define INCLUDED_GK208FB_H

#include "gf100fb.h"
#include <memory>
class GK208AddressDecode;

class GK208FrameBuffer : public GF100FrameBuffer
{
public:
    explicit GK208FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm);
    virtual ~GK208FrameBuffer();
    virtual RC Initialize();
    virtual UINT32 GetSubpartitions() const;

    virtual RC DecodeAddress
    (
        FbDecode *pDecode,
        UINT64 physicalFbOffset,
        UINT32 pteKind,
        UINT32 pageSizeKB
    ) const;
protected:
    //! Constructor for use by derived classes (overrides Name).
    GK208FrameBuffer
    (
        const char*   derivedClassName,
        GpuSubdevice* pGpuSubdevice,
        LwRm*         pLwRm
    );
private:
    unique_ptr<GK208AddressDecode> m_Decoder;
};

#endif // ! INCLUDED_GK208FB_H

