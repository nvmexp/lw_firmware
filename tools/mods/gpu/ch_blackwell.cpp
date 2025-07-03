/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Blackwell Channel

#include "core/include/channel.h"
//TODO/FIXME: replace class definition with new class for Blackwell
#include "class/clc86f.h"            // HOPPER_CHANNEL_GPFIFO
#include "gpu/include/userdalloc.h"

class GpuDevice;
class LwRm;
class Tsg;

BlackwellGpFifoChannel::BlackwellGpFifoChannel
(
    UserdAllocPtr pUserdAlloc,
    void *        pPushBufferBase,
    UINT32        PushBufferSize,
    void *        pErrorNotifierMemory,
    LwRm::Handle  hChannel,
    UINT32        ChID,
    UINT32        Class,
    UINT64        PushBufferOffset,
    void *        pGpFifoBase,
    UINT32        GpFifoEntries,
    LwRm::Handle  hVASpace,
    GpuDevice *   pGrDev,
    LwRm *        pLwRm,
    Tsg *         pTsg,
    UINT32        engineId,
    bool          useBar1Doorbell
)
:   HopperGpFifoChannel
    (
        pUserdAlloc,
        pPushBufferBase,
        PushBufferSize,
        pErrorNotifierMemory,
        hChannel,
        ChID,
        Class,
        PushBufferOffset,
        pGpFifoBase,
        GpFifoEntries,
        hVASpace,
        pGrDev,
        pLwRm,
        pTsg,
        engineId,
        useBar1Doorbell
    )
{
}

/* virtual */ RC BlackwellGpFifoChannel::GetReference(UINT32 *pRefCnt)
{
    return RC::UNSUPPORTED_FUNCTION;
}
