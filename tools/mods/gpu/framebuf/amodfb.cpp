/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009, 2016-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "amodfb.h"
#include "gpu/include/gpusbdev.h"

AmodFrameBuffer::AmodFrameBuffer
(
    GpuSubdevice * pGpuSubdevice,
    LwRm         * pLwRm
)
:   FrameBuffer("AModel FrameBuffer", pGpuSubdevice, pLwRm)
{
}

RC AmodFrameBuffer::GetFbRanges(FbRanges *pFbRanges) const
{
    pFbRanges->resize(1);
    (*pFbRanges)[0].start = 0;
    (*pFbRanges)[0].size  = GetGraphicsRamAmount();
    return OK;
}

UINT64   AmodFrameBuffer::GetGraphicsRamAmount() const
{
    // amodel will always use the FBAperture size as the FB size
    return GpuSub()->FramebufferBarSize();
}

bool AmodFrameBuffer::IsSOC() const
{
    return GpuSub()->IsSOC();
}

void AmodFrameBuffer::GetRBCAddress
(
   EccAddrParams *pRbcAddr,
   UINT64 fbOffset,
   UINT32 pteKind,
   UINT32 pageSize,
   UINT32 errCount,
   UINT32 errPos
)
{
    Printf(Tee::PriHigh, "Running Ecc test is not supported for current chip\n");
    memset((void *)pRbcAddr, 0, sizeof(EccAddrParams));
    return;
}

void AmodFrameBuffer::GetEccInjectRegVal
(
   EccErrInjectParams *pInjectParams,
   UINT64 fbOffset,
   UINT32 pteKind,
   UINT32 pageSize,
   UINT32 errCount,
   UINT32 errPos
)
{
    Printf(Tee::PriHigh, "Running Ecc test is not supported for current chip\n");
    memset((void *)pInjectParams, 0, sizeof(EccAddrParams));
    return;
}

FrameBuffer *CreateAmodFrameBuffer(GpuSubdevice * pGpuSubdevice, LwRm * pLwRm)
{
    return new AmodFrameBuffer(pGpuSubdevice, pLwRm);
}

