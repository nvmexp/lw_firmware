/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2012,2014-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#ifndef INCLUDED_GKLIT1FB_H
#define INCLUDED_GKLIT1FB_H

#include "gf100fb.h"
#include <memory>
class GKLIT1AddressDecode;

class GKLIT1FrameBuffer : public GF100FrameBuffer
{
public:
    explicit GKLIT1FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm);
    virtual ~GKLIT1FrameBuffer();
    virtual RC Initialize();

    virtual RC DecodeAddress
    (
        FbDecode *pDecode,
        UINT64 physicalFbOffset,
        UINT32 pteKind,
        UINT32 pageSizeKB
    ) const;
    void EnableDecodeDetails(bool enable);
protected:
    GKLIT1FrameBuffer
    (
        const char*   derivedClassName,
        GpuSubdevice* pGpuSubdevice,
        LwRm*         pLwRm
    );
private:
    unique_ptr<GKLIT1AddressDecode> m_Decoder;
};

#endif // ! INCLUDED_GKLIT1FB_H
