/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Kepler Amodel Channel

#include "core/include/channel.h"
#include "core/include/lwrm.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/userdalloc.h"
#include "class/cl906f.h"            // GF100ControlGPFifo
#include "class/clc86f.h"            // HOPPER_CHANNEL_GPFIFO

AmodelGpFifoChannel::AmodelGpFifoChannel
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
:   KeplerGpFifoChannel( pUserdAlloc,
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
                        pTsg)
    ,m_EngineId(engineId)
{
    if (!GetGpuDevice()->GetSubdevice(0)->HasFeature(Device::GPUSUB_HOST_REQUIRES_ENGINE_ID))
    {
        // Go old flow for amodel test before Ampere.
        for (UINT32 i = 0; i <= m_PollArgs.LastGetIdx; i++)            
        {            
            void *pvUserd = GetUserdAlloc()->GetAddress(i);            
            m_pGet[i] = (volatile UINT32 *)            
                &((GF100ControlGPFifo *)(pvUserd))->Get;            
            m_pGetHi[i] = (volatile UINT32 *)            
                &((GF100ControlGPFifo *)(pvUserd))->GetHi;            
            m_pGpGet[i] = (volatile UINT32 *)            
                &((GF100ControlGPFifo *)(pvUserd))->GPGet;
        }    
    }
}

AmodelGpFifoChannel::~AmodelGpFifoChannel()
{
}

/* virtual */ RC AmodelGpFifoChannel::Initialize()
{
    return BaseChannel::Initialize();
}

/* virtual */ UINT64 AmodelGpFifoChannel::ReadGet(UINT32 i) const
{
    GpuDevice* gpuDev = GetGpuDevice();
    GpuSubdevice* gpuSubdev = gpuDev->GetSubdevice(i);

    if (gpuSubdev->HasFeature(Device::GPUSUB_HOST_REQUIRES_ENGINE_ID))
    {
        UINT32 getLow;
        UINT32 getHi;
        if (gpuSubdev->EscapeReadBuffer("GET_LOW", GetChannelId(), sizeof(UINT32), &getLow))
        {
            MASSERT(!"GET_LOW EscapeRead failed\n");
        }
        if (gpuSubdev->EscapeReadBuffer("GET_HI", GetChannelId(), sizeof(UINT32), &getHi))
        {
            MASSERT(!"GET_HI EscapeRead failed\n");
        }

        const UINT64 get = ((UINT64(getHi) << 32) | getLow);
        return get;
    }
    else
    {
        return KeplerGpFifoChannel::ReadGet(i);
    }
}

/* virtual */ void AmodelGpFifoChannel::UpdateCachedGpGets() 
{
    GpuDevice* gpuDev = GetGpuDevice();

    if (gpuDev->GetSubdevice(0)->HasFeature(Device::GPUSUB_HOST_REQUIRES_ENGINE_ID)) 
    {
        for (UINT32 i = 0; i < gpuDev->GetNumSubdevices(); i++)
        {
            UINT32 gpGet;
            GpuSubdevice* gpuSubdev = gpuDev->GetSubdevice(i);
            if (gpuSubdev->EscapeReadBuffer("GP_GET", GetChannelId(), sizeof(UINT32), &gpGet))
            {
                MASSERT(!"GP_GET EscapeRead failed\n");
            }
            m_CachedGpGet[i] = gpGet;
        }
    }
    else
    {
        KeplerGpFifoChannel::UpdateCachedGpGets();
    }
}

/* virtual */ RC AmodelGpFifoChannel::CommitGpPut
(
        void* controlRegs, 
        UINT08* pGpFifoBase,
        UINT32 startGpPut, 
        UINT32 endGpPut, 
        UINT32* pOutGpPut,
        UINT32 subdev
)
{
    GpuDevice* gpuDev = GetGpuDevice();
    GpuSubdevice* gpuSubdev = gpuDev->GetSubdevice(subdev);
    RC rc;

    if (gpuSubdev->HasFeature(Device::GPUSUB_HOST_REQUIRES_ENGINE_ID))
    {
        const size_t bufferSize = 
                            sizeof(UINT32) +    // ChannelId
                            sizeof(UINT32) + 1; // GpPut
        vector<UINT08> escwriteBuffer(bufferSize);
        UINT08* buf = &escwriteBuffer[0];

        *(UINT32*)buf = GetChannelId();
        buf += sizeof(UINT32);

        *(UINT32*)buf = endGpPut;
        buf += sizeof(UINT32);

        *buf = '\0';
        ++buf;
        
        if (gpuSubdev->EscapeWriteBuffer("GP_PUT", 0, bufferSize, &escwriteBuffer[0]))
        {
            MASSERT(!"GP_PUT EscapeWrite failed\n");
            return RC::SOFTWARE_ERROR;
        }
    }
    else
    {
        CHECK_RC(KeplerGpFifoChannel::CommitGpPut(controlRegs, pGpFifoBase,
                startGpPut, endGpPut, pOutGpPut, subdev));
    }

    return rc;
}

/* virtual */ RC AmodelGpFifoChannel::ConstructGpEntryData
(
    UINT64 get,
    UINT32 length,
    bool subroutine,
    UINT32* pGpEntry0,
    UINT32* pGpEntry1
)
{
    RC rc;

    CHECK_RC(WriteExtendedBase(get));

    return FermiGpFifoChannel::ConstructGpEntryData(
        get,
        length,
        subroutine,
        pGpEntry0,
        pGpEntry1);
}

// Hopper can support 57-bit virtual addresses for pushbuffers, but the gpentry
// data struct only has room for 40 bits.  In order to specify the upper 17
// bits, a separate gpentry with the SET_PB_SEGMENT_EXTENDED_BASE opcode needs
// to be sent first to tell host what the upper bits of the next pushbuffer's
// address will be.
//
// This function will also be called for pre-Hopper Amodels, but the Get
// pointer will never be above 40 bits, so the opcode will never be sent.
//
RC AmodelGpFifoChannel::WriteExtendedBase
(
    UINT64 get
)
{
    RC rc;
    UINT32 extendedBase = LwU64_HI32(get) >> 8;

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
