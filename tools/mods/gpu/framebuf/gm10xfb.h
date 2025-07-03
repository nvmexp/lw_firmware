/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#ifndef INCLUDED_GM10XFB_H
#define INCLUDED_GM10XFB_H

#include "gf100fb.h"
#include <memory>
class GMLIT1AddressDecode;
class GMLIT1AddressEncode;

class GM10XFrameBuffer : public GF100FrameBuffer
{
public:
    explicit GM10XFrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm);
    virtual ~GM10XFrameBuffer();
    virtual RC Initialize();
    virtual void EnableDecodeDetails(bool enable);
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
    GM10XFrameBuffer
    (
        const char*   derivedClassName,
        GpuSubdevice* pGpuSubdevice,
        LwRm*         pLwRm
    );

private:
    unique_ptr<GMLIT1AddressDecode> m_Decoder;
    unique_ptr<GMLIT1AddressEncode> m_Encoder;
};

#endif // ! INCLUDED_GM10XFB_H
