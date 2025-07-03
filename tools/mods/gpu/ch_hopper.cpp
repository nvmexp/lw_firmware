/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Hopper Channel

#include "core/include/channel.h"
#include "class/clc86f.h"            // HOPPER_CHANNEL_GPFIFO
#include "gpu/include/userdalloc.h"

class GpuDevice;
class LwRm;
class Tsg;

HopperGpFifoChannel::HopperGpFifoChannel
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
:   AmpereGpFifoChannel
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
        engineId
    ),
    m_UseBar1Doorbell(useBar1Doorbell)
{
}

/* virtual */ RC HopperGpFifoChannel::ConstructGpEntryData
(
    UINT64 Get,
    UINT32 Length,
    bool Subroutine,
    UINT32* pGpEntry0,
    UINT32* pGpEntry1
)
{
    RC rc;

    CHECK_RC(WriteExtendedBase(Get));

    return FermiGpFifoChannel::ConstructGpEntryData(Get, Length, Subroutine,
        pGpEntry0, pGpEntry1);
}

// Hopper can support 57-bit virtual addresses for pushbuffers, but the gpentry
// data struct only has room for 40 bits.  In order to specify the upper 17
// bits, a separate gpentry with the SET_PB_SEGMENT_EXTENDED_BASE opcode needs
// to be sent first to tell host what the upper bits of the next pushbuffer's
// address will be.
//
RC HopperGpFifoChannel::WriteExtendedBase
(
    UINT64 Get
)
{
    RC rc;
    UINT32 extendedBase = LwU64_HI32(Get) >> 8;

    // The extended base opcode only needs to be sent when the value changes
    // from the previous one.
    if (extendedBase != m_LwrrentExtendedBase)
    {
        UINT32 opCode = DRF_DEF(C86F, _GP_ENTRY1, _OPCODE,
            _SET_PB_SEGMENT_EXTENDED_BASE);
        UINT32 GpEntry0 = DRF_NUM(C86F, _GP_ENTRY0, _PB_EXTENDED_BASE_OPERAND,
            extendedBase);
        UINT32 GpEntry1 = DRF_NUM(C86F, _GP_ENTRY1, _LENGTH, 0) | opCode;

        CHECK_RC(WriteGpEntryData(GpEntry0, GpEntry1));

        m_LwrrentExtendedBase = extendedBase;
    }

    return rc;
}

bool HopperGpFifoChannel::IsBar1DummyReadNeeded()
{
    return (GetUserdAlloc()->GetLocation() == Memory::Fb &&
            !GetUseBar1Doorbell());
}

bool HopperGpFifoChannel::GetUseBar1Doorbell() const
{
    return m_UseBar1Doorbell;
}
