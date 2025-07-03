/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#ifndef INCLUDED_GM20XFB_H
#define INCLUDED_GM20XFB_H

#include "gf100fb.h"
#include <memory>
class GMLIT2AddressDecode;
class GMLIT2AddressEncode;

class GM20XFrameBuffer : public GF100FrameBuffer
{
public:
    explicit GM20XFrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm);
    virtual ~GM20XFrameBuffer();
    virtual RC Initialize();
    void EnableDecodeDetails(bool enable);
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
    GM20XFrameBuffer
    (
        const char*   derivedClassName,
        GpuSubdevice* pGpuSubdevice,
        LwRm*         pLwRm
    );

private:
    unique_ptr<GMLIT2AddressDecode> m_Decoder;
    unique_ptr<GMLIT2AddressEncode> m_Encoder;
};

#endif // ! INCLUDED_GM20XFB_H
