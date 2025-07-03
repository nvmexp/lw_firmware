
/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Ampere Channel

#include "core/include/channel.h"
#include "core/include/lwrm.h"
#include "gpu/include/userdalloc.h"

class GpuDevice;
class Tsg;

AmpereGpFifoChannel::AmpereGpFifoChannel
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
        UINT32        engineId
)
:   VoltaGpFifoChannel(pUserdAlloc,
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
                       engineId)
{
}

RC AmpereGpFifoChannel::SetHwCrcCheckMode(UINT32 Mode)
{
    if (Mode & HWCRCCHKMODE_MTHD)
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    return VoltaGpFifoChannel::SetHwCrcCheckMode(Mode);
}
