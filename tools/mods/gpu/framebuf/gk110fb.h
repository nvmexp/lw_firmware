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
#ifndef INCLUDED_GK110FB_H
#define INCLUDED_GK110FB_H

#include "gf100fb.h"
#include <memory>
class GKLIT2AddressDecode;
class GKLIT2AddressEncode;

class GK110FrameBuffer : public GF100FrameBuffer
{
public:
    explicit GK110FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm);
    virtual ~GK110FrameBuffer();
    virtual RC Initialize();

    virtual RC DecodeAddress
    (
        FbDecode *pDecode,
        UINT64 physicalFbOffset,
        UINT32 pteKind,
        UINT32 pageSizeKB
    ) const;

    virtual RC EncodeAddress
    (
        const FbDecode &decode,
        UINT32 pteKind,
        UINT32 pageSizeKB,
        UINT64 *pEccOnAddr,
        UINT64 *pEccOffAddr
    ) const;

protected:
    //! Constructor for use by derived classes (overrides Name).
    GK110FrameBuffer
    (
        const char*   derivedClassName,
        GpuSubdevice* pGpuSubdevice,
        LwRm*         pLwRm
    );
private:
    unique_ptr<GKLIT2AddressDecode> m_Decoder;
    unique_ptr<GKLIT2AddressEncode> m_Encoder;
};

#endif // ! INCLUDED_GK110FB_H

