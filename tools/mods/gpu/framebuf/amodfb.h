/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------

#pragma once
#ifndef INCLUDED_AMODFB_H
#define INCLUDED_AMODFB_H

#include "core/include/framebuf.h"
class GpuSubdevice;

/**
 * @class( AmodFrameBuffer )
 *
 * AMODEL frame buffer device.
 */

//
// AModel does not model the FrameBuffer details, therefore the
// implementation here is pretty much a stub.
//

class AmodFrameBuffer : public FrameBuffer
{
public:
    explicit AmodFrameBuffer(GpuSubdevice * pGpuSubdevice, LwRm * pLwRm);

    RC       GetFbRanges(FbRanges *pFbRanges) const;
    UINT64   GetGraphicsRamAmount()        const;
    bool     IsSOC()                       const;
    UINT32   GetSubpartitions()            const { return 0; }
    UINT32   GetBusWidth()                 const { return 0; }
    UINT32   GetFbpMask()                            const { return 0; }
    UINT32   GetFbpCount()                           const { return 0; }
    UINT32   GetMaxFbpCount()                        const { return 0; }
    UINT32   VirtualFbpToHwFbp(UINT32 virtualFbp)    const { return 0; }
    UINT32   HwFbpToVirtualFbp(UINT32 hwFbp)         const { return 0; }
    UINT32   GetFbioMask()                           const { return 0; }
    UINT32   GetFbioCount()                          const { return 0; }
    UINT32   GetMaxFbioCount()                       const { return 0; }
    UINT32   GetFbiosPerFbp()                        const { return 0; }
    UINT32   VirtualFbioToHwFbio(UINT32 virtualFbio) const { return 0; }
    UINT32   HwFbioToVirtualFbio(UINT32 hwFbio)      const { return 0; }
    UINT32   GetLtcMask()                            const { return 0; }
    UINT32   GetLtcCount()                           const { return 0; }
    UINT32   GetMaxLtcCount()                        const { return 0; }
    UINT32   GetMaxLtcsPerFbp()                      const { return 0; }
    UINT32   VirtualLtcToHwLtc(UINT32 virtualLtc)    const { return 0; }
    UINT32   HwLtcToVirtualLtc(UINT32 hwLtc)         const { return 0; }
    UINT32   HwLtcToHwFbio(UINT32 hwLtc)             const { return 0; }
    UINT32   HwFbioToHwLtc(UINT32 fbio, UINT32 subp) const { return 0; }
    UINT32   VirtualL2SliceToHwL2Slice(UINT32 virtualLtc, UINT32 virtualL2Slice) const { return 0; }
    UINT32   GetSlicesPerLtc()                       const { return 0; }
    UINT32   GetPseudoChannelsPerChannel()           const { return 0; }
    bool     IsEccOn()                               const { return false; }
    bool     IsRowRemappingOn()                      const { return false; }
    UINT32   GetRankCount()                const { return 0; }
    UINT32   GetBankCount()                const { return 0; }
    UINT32   GetRowBits()                  const { return 0; }
    UINT32   GetRowSize()                  const { return 0; }
    UINT32   GetBurstSize()                const { return 0; }
    UINT32   GetDataRate()                 const { return 0; }
    RC       Initialize()                        { return OK; }
    RC       DecodeAddress(FbDecode *pDec, UINT64, UINT32, UINT32) const
                                { memset(pDec, 0, sizeof(*pDec)); return OK; }
    virtual void GetRBCAddress
    (
         EccAddrParams *pRbcAddr,
         UINT64 fbOffset,
         UINT32 pteKind,
         UINT32 pageSize,
         UINT32 errCount,
         UINT32 errPos
    );
    virtual void GetEccInjectRegVal
    (
         EccErrInjectParams *pInjectParams,
         UINT64 fbOffset,
         UINT32 pteKind,
         UINT32 pageSize,
         UINT32 errCount,
         UINT32 errPos
    );
};

#endif // ! INCLUDED_AMODFB_H
