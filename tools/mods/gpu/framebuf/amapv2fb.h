/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#pragma once
#ifndef INCLUDED_AMAPV2FB_H
#define INCLUDED_AMAPV2FB_H

#include "gpu/framebuf/amapv1fb.h"

//! \brief Base class for all FrameBuffer objects that use the AMAP V2 library

class AmapV2FrameBuffer : public AmapV1FrameBuffer
{
public:
    AmapV2FrameBuffer(const char* name, GpuSubdevice* pGpuSubdevice,
                      LwRm* pLwRm, AmapLitter litter);
    void GetRBCAddress(EccAddrParams* pRbcAddr, UINT64 fbOffset,
                       UINT32 pteKind, UINT32 pageSizeKB,
                       UINT32 errCount, UINT32 errPos) override;
    RC   EccAddrToPhysAddr(const EccAddrParams& eccAddr, UINT32 pteKind,
                           UINT32 pageSizeKB, PHYSADDR* pPhysAddr) override;
    void GetEccInjectRegVal(EccErrInjectParams* pInjectParams, UINT64 fbOffset,
                            UINT32 pteKind, UINT32 pageSizeKB, UINT32 errCount,
                            UINT32 errPos) override;
    RC   DisableFbEccCheckbitsAtAddress(UINT64 physAddr, UINT32 pteKind,
                                        UINT32 pageSizeKB) override;
    RC   ApplyFbEccWriteKillPtr(UINT64 physAddr, UINT32 pteKind,
                                UINT32 pageSizeKB,
                                INT32 kptr0, INT32 kptr1) override;
};

#endif // INCLUDED_AMAPV2FB_H
