/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2017,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#ifndef INCLUDED_GF100FB_H
#define INCLUDED_GF100FB_H

#include "gpufb.h"
// These can't be pre-declared: GF100AddressDecode GF100AddressEncode
// as the unique_ptr woudn't be able to call the destructor
// [per msvc warning C4150]
#include "gf100adr.h"
#include <memory>

/**
 * @class( GF100FrameBuffer )
 *
 * GF100 frame buffer device, based on GT200FrameBuffer.
 */

class GF100FrameBuffer : public GpuFrameBuffer
{
public:
    explicit       GF100FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm);
    virtual RC     Initialize();
    virtual UINT32 HwFbioToHwFbpa(UINT32 hwFbio)                     const;
    virtual UINT32 HwFbpaToHwFbio(UINT32 hwFbpa)                     const;
    virtual UINT32 VirtualFbioToVirtualFbpa(UINT32 virtualFbio)      const;
    virtual UINT32 VirtualFbpaToVirtualFbio(UINT32 virtualFbpa)      const;
    virtual UINT32 HwLtcToHwFbio(UINT32 hwLtc)                       const;
    virtual UINT32 HwFbioToHwLtc(UINT32 hwFbio, UINT32 subpartition) const;
    virtual RC DecodeAddress
    (
        FbDecode *pDecode,
        UINT64 physicalFbOffset,
        UINT32 pteKind,
        UINT32 pageSizeKB
    ) const;
    virtual void GetRBCAddress
    (
        EccAddrParams *pRbcAddr,
        UINT64 fbOffset,
        UINT32 pteKind,
        UINT32 pageSize,
        UINT32 errCount,
        UINT32 errPos
    );

    virtual RC EncodeAddress
    (
        const FbDecode &decode,
        UINT32 pteKind,
        UINT32 pageSizeKB,
        UINT64 *eccOnAddr,
        UINT64 *eccOffAddr
    ) const;

    virtual UINT32 GetNumReversedBursts() const;

    virtual void EnableDecodeDetails(bool enable);
    virtual void GetEccInjectRegVal
    (
         EccErrInjectParams *pInjectParams,
         UINT64 fbOffset,
         UINT32 pteKind,
         UINT32 pageSize,
         UINT32 errCount,
         UINT32 errPos
    );

protected:
    virtual RC InitCommon();

    //! Constructor for use by derived classes (overrides Name).
    GF100FrameBuffer
    (
        const char*   derivedClassName,
        GpuSubdevice* pGpuSubdevice,
        LwRm*         pLwRm
    );

    UINT32 m_FbpaMask;
    UINT32 m_FbioShift;

private:
    unique_ptr<GF100AddressDecode> m_Decoder;
    unique_ptr<GF100AddressEncode> m_Encoder;
};

#endif // ! INCLUDED_GF100FB_H
