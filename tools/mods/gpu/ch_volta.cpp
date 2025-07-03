/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Kepler Channel

#include "core/include/channel.h"
#include "gpu/utility/tsg.h"
#include "class/clc36f.h" // VOLTA_CHANNEL_GPFIFO_A
#include "class/clc46f.h" // TURING_CHANNEL_GPFIFO_A
#include "class/clc56f.h" // AMPERE_CHANNEL_GPFIFO_A
#include "class/clc86f.h" // HOPPER_CHANNEL_GPFIFO_A
#include "class/clc361.h" // LWC361
#include "ctrl/ctrlc36f.h"
#include "gpu/include/gralloc.h"
#include "core/include/lwrm.h"
#include "gpu/include/gpudev.h"
#include "core/include/cpu.h"
#include "core/include/platform.h"
#include "core/include/version.h"
#include "gpu/utility/surf2d.h"
#include "gpu/utility/chanwmgr.h"
#include "gpu/include/userdalloc.h"

VoltaGpFifoChannel::VoltaGpFifoChannel
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
:   KeplerGpFifoChannel(pUserdAlloc,
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
    // USERD access not available on CheetAh
    if (!Platform::UsesLwgpuDriver())
    {
        m_DoorbellControl.resize(Gpu::MaxNumSubdevices);
        m_pBar1FlushAddr.resize(Gpu::MaxNumSubdevices, nullptr);
    }
}

VoltaGpFifoChannel::~VoltaGpFifoChannel()
{
    m_DoorbellControl.clear();
    m_pBar1FlushAddr.clear();
}

/* virtual */ RC VoltaGpFifoChannel::Initialize()
{
    RC rc;
    for (UINT32 i = 0; i < GetGpuDevice()->GetNumSubdevices(); i++)
    {
        GpuSubdevice *pSubdev = GetGpuDevice()->GetSubdevice(i);
        if (!Platform::UsesLwgpuDriver())
        {
            m_DoorbellControl[i] = make_unique<DoorbellControl>();
            CHECK_RC(m_DoorbellControl[i]->Alloc(pSubdev,
                                                 GetRmClient(),
                                                 GetHandle(),
                                                 GetUseBar1Doorbell()));
        }
    }
    CHECK_RC(KeplerGpFifoChannel::Initialize());

    return rc;
}

/* virtual */ RC VoltaGpFifoChannel::RmcGetClassEngineId
(
    LwRm::Handle hObject,
    UINT32 *pClassEngineId,
    UINT32 *pClassId,
    UINT32 *pEngineId
)
{
    LwRm *pLwRm = GetRmClient();
    LwRm::Handle hCh = GetHandle();
    RC rc;

    switch (GetClass())
    {
        case VOLTA_CHANNEL_GPFIFO_A:
        case TURING_CHANNEL_GPFIFO_A:
        case AMPERE_CHANNEL_GPFIFO_A:
        case HOPPER_CHANNEL_GPFIFO_A:
        {
            LWC36F_CTRL_GET_CLASS_ENGINEID_PARAMS Params = {0};
            Params.hObject = hObject;
            CHECK_RC(pLwRm->Control(hCh, LWC36F_CTRL_GET_CLASS_ENGINEID,
                                    &Params, sizeof(Params)));
            if (pClassEngineId)
                *pClassEngineId = Params.classEngineID;
            if (pClassId)
                *pClassId = Params.classID;
            if (pEngineId)
                *pEngineId = Params.engineID;
            break;
        }
        default:
            return RC::UNSUPPORTED_FUNCTION;
    }

    return rc;
}

/* virtual */ RC VoltaGpFifoChannel::RmcResetChannel(UINT32 EngineId)
{
    LwRm *pLwRm = GetRmClient();
    LwRm::Handle hCh = GetHandle();

    switch (GetClass())
    {
        case VOLTA_CHANNEL_GPFIFO_A:
        case TURING_CHANNEL_GPFIFO_A:
        case AMPERE_CHANNEL_GPFIFO_A:
        case HOPPER_CHANNEL_GPFIFO_A:
        {
            LWC36F_CTRL_CMD_RESET_CHANNEL_PARAMS Params = {0};
            Params.engineID = EngineId;
            return pLwRm->Control(hCh, LWC36F_CTRL_CMD_RESET_CHANNEL,
                                  &Params, sizeof(Params));
        }
        default:
            break;
    }

    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC VoltaGpFifoChannel::RmcGpfifoSchedule(bool Enable)
{
    LwRm *pLwRm = GetRmClient();
    LwRm::Handle hCh = GetHandle();

    switch (GetClass())
    {
        case VOLTA_CHANNEL_GPFIFO_A:
        case TURING_CHANNEL_GPFIFO_A:
        case AMPERE_CHANNEL_GPFIFO_A:
        case HOPPER_CHANNEL_GPFIFO_A:
        {
            LWC36F_CTRL_GPFIFO_SCHEDULE_PARAMS Params = {0};
            Params.bEnable = Enable ? LW_TRUE : LW_FALSE;
            return pLwRm->Control(hCh, LWC36F_CTRL_CMD_GPFIFO_SCHEDULE,
                                  &Params, sizeof(Params));
        }
        default:
            break;
    }

    return RC::UNSUPPORTED_FUNCTION;
}

// This function does a dummy read to a BAR1 surface to flush out all the 
// previous BAR1 writes
RC VoltaGpFifoChannel::Bar1FlushSurfaceRead
(
    UINT32 subdevIdx, 
    GpuSubdevice *pSubdev
)
{
    RC rc;
    if (m_pBar1FlushSurface.get() == nullptr)
    {
        m_pBar1FlushSurface.reset(new Surface2D);
        if (m_pBar1FlushSurface.get() == nullptr)
        {
            return RC::CANNOT_ALLOCATE_MEMORY;
        }

        m_pBar1FlushSurface->SetWidth(4);
        m_pBar1FlushSurface->SetHeight(1);
        m_pBar1FlushSurface->SetColorFormat(ColorUtils::Y8);
        m_pBar1FlushSurface->SetLocation(Memory::Fb);
        if (Platform::GetSimulationMode() == Platform::Amodel)
        {
            // Amodel by default uses segmentation mode for buffer allocation
            // Since there is no way for fermi to setup contextdma, we have
            // to force paging mode in this case
            m_pBar1FlushSurface->SetAddressModel(Memory::Paging);
        }
        m_pBar1FlushSurface->SetGpuVASpace(GetVASpace());
        CHECK_RC(m_pBar1FlushSurface->Alloc(GetGpuDevice(), GetRmClient()));
    }

    if (m_pBar1FlushAddr[subdevIdx] == nullptr)
    {
        CHECK_RC(GetRmClient()->MapMemory(
            m_pBar1FlushSurface->GetMemHandle(GetRmClient()),
            0,
            4,
            &m_pBar1FlushAddr[subdevIdx],
            0,
            pSubdev));
    }

    volatile UINT32 dummyRead = MEM_RD32(m_pBar1FlushAddr[subdevIdx]);
    dummyRead = dummyRead;

    return rc;
}

// private
RC VoltaGpFifoChannel::WriteGpPut()
{
    StickyRC rc;

    // Note all new volta flush requirements are spelled out in the
    // Volta_snoop_free_Host.docx document. Sections 3.2.3.1 and 3.2.3.2
    if (m_LwrrentGpPut != m_CachedGpPut && !ExternalGPPutAdvance())
    {
        if (Platform::IsPPC())
        {
            // PPC requires a hwsync before writing GP_PUT.  Certain memory
            // configurations only require a lwsync, but those configurations
            // depend on USERD/GPFIFO/PB location combinations.  Since the PB
            // and GPFIFO memory can be allocated outside the channel class (and
            // in fact different submissions can have different GPFIFO locations -
            // see CallSubroutine), always use a hwsync
            Cpu::FlushCache();
        }
        else
        {
            // All other platforms require flushing the WC buffer
            rc = Platform::FlushCpuWriteCombineBuffer();
        }

        // Flush the GpPut pointer.
        for (UINT32 ii = 0; ii < GetGpuDevice()->GetNumSubdevices(); ii++)
        {
            if (m_EnableLogging &&
                (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK))
            {
                Printf(Tee::PriNormal,
                    "DEV_%x_CH_%x Flush subdev %d: GpPut was 0x%x now 0x%x\n",
                    m_DevInst, m_ChID, ii, m_CachedGpPut,
                    m_LwrrentGpPut);
            }

            // Check for RC errors before updating GP_PUT (sending a GP
            // entry down after an RC error will likely cause hangs since
            // the channel cannot continue to process data after an RC
            // error)
            CHECK_RC(CheckError());

            GpuSubdevice *pSubdev = GetGpuDevice()->GetSubdevice(ii);
            if (Platform::IsPPC())
            {
                // On PPC, when the USERD region is in coherent memory (either
                // vidmem or sysmem) and the GPFIFO or PB memory is in MMIO
                // (i.e. non-coherent) BAR1, then a read from BAR1 needs to
                // be done in order to push all priovious BAR1 writes to the
                // GPU.  Since the PB and GPFIFO memory can be allocated
                // outside the channel class (and in fact different submissions
                // can have different GPFIFO locations - see CallSubroutine),
                // perform the BAR1 read if USERD is in any form of coherent
                // memory
                //
                // TODO : Also USERD is in coherent vidmem (i.e. vidmem mapped
                // through lwlink in a coherent manner)
                if (GetUserdAlloc()->GetLocation() == Memory::Coherent)
                {
                    CHECK_RC(Bar1FlushSurfaceRead(ii, pSubdev));
                }
            }
            else if (GetUserdAlloc()->GetLocation() != Memory::Fb &&
                     !Platform::UsesLwgpuDriver())
            {
                // If the USERD is in sysmem and PB/GPFIFO is in FB then
                // the GPU needs to be flushed.  Since the PB and GPFIFO
                // memory can be allocated outside the channel class (and in
                // fact different submissions can have different GPFIFO
                // locations - see CallSubroutine), always perform the FB
                // flush if USERD is in sysmem
                CHECK_RC(pSubdev->FbFlush(m_TimeoutMs));
            }
            //If MODS detects fault before, channel will be reset. Then MODS
            //should not update the PUT to channel. Add an error check here
            //to prevent this kind of 'error update'.
            CHECK_RC(CheckError());
            CHECK_RC(CommitGpPut(GetUserdAlloc()->GetAddress(ii), m_pGpFifoBase,
                        m_CachedGpPut, m_LwrrentGpPut, &m_LwrrentGpPut, ii));

            // Check if there is a sync operation needed between GpPut update
            // (BAR1- if UserD is in vidmem) and doorbell ring (BAR0) and then 
            // insert a dummy BAR1 read
            if (IsBar1DummyReadNeeded())
            {
                if (m_EnableLogging &&
                    (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK))
                {
                    Printf(Tee::PriNormal, 
                        "Inserting Bar1 dummy Read between GpPut and Doorbell ring\n");
                }
                CHECK_RC(Bar1FlushSurfaceRead(ii, pSubdev));
            }
        }

        if (Platform::IsPPC())
        {
            // On PPC, need to hwsync after writing GP_PUT.  Certain
            // combinations of USERD/PB/GPFIFO only require a eieio. Since
            // the PB and GPFIFO memory can be allocated outside the channel
            // class (and in fact different submissions can have different
            // GPFIFO locations - see CallSubroutine), simply always perform
            // a hwsync
            Cpu::FlushCache();
        }
        else
        {
            // On non-PPC platforms, the MODS USERD mapping is WC therefore
            // it is necessary to flush the WC buffer after writing GP_PUT
            // before ringing the doorbell
            CHECK_RC(Platform::FlushCpuWriteCombineBuffer());
        }

        if (!GetDoorbellRingEnable())
        {
            Printf(Tee::PriWarn, 
                "Doorbell ring is disabled, skip ringing it for this GpPut update\n");
        }
        else if (!Platform::UsesLwgpuDriver())
        {
            for (UINT32 ii = 0; ii < GetGpuDevice()->GetNumSubdevices(); ii++)
            {
                if (!m_DoorbellControl[ii] ||
                    !m_DoorbellControl[ii]->IsInitialized())
                {
                    Printf(Tee::PriError,
                           "Volta Doorbell control not initialized!\n");
                    return RC::SOFTWARE_ERROR;
                }

                // Apply the pause before/after GPPut writes to the doorbell
                // ring in order to maintain the same timing as in previous GPUs.
                // For previous GPUs the GP_PUT write is snooped and actually
                // kicks off channel work.  On Volta+ it requires an explicit
                // write to a doorbell region to start channel work.
                PauseBeforeGpPutUpdate();

                CHECK_RC(m_DoorbellControl[ii]->Ring());

                PauseAfterGpPutUpdate();
            }
        }

        m_CachedGpPut = m_LwrrentGpPut;
    }
    return rc;
}

/* virtual */ RC VoltaGpFifoChannel::SemaphoreAcquire(UINT64 Data)
{
    RC rc;
    SemaphorePayloadSize payloadSize;
    CHECK_RC(GetActualSemaphorePayloadSize(Data, &payloadSize));

    UINT32 execute = DRF_DEF(C36F, _SEM_EXELWTE, _OPERATION, _ACQUIRE);
    if (ChannelWrapperMgr::Instance()->UsesRunlistWrapper())
    {
        execute |= DRF_DEF(C36F, _SEM_EXELWTE, _ACQUIRE_SWITCH_TSG, _DIS);
    }
    else
    {
        execute |= DRF_DEF(C36F, _SEM_EXELWTE, _ACQUIRE_SWITCH_TSG, _EN);
    }

    vector<UINT32> semData;
    semData.push_back(static_cast<UINT32>(GetHostSemaOffset()));
    semData.push_back(static_cast<UINT32>(GetHostSemaOffset()>>32));
    semData.push_back(static_cast<UINT32>(Data));
    if ((GetSemaphorePayloadSize() == SEM_PAYLOAD_SIZE_64BIT) ||
        ((GetSemaphorePayloadSize() == SEM_PAYLOAD_SIZE_AUTO) && (Data >> 32)))
    {
        semData.push_back(static_cast<UINT32>(Data>>32));
        execute |= DRF_DEF(C36F, _SEM_EXELWTE, _PAYLOAD_SIZE, _64BIT);
    }
    else
    {
        semData.push_back(0);
        execute |= DRF_DEF(C36F, _SEM_EXELWTE, _PAYLOAD_SIZE, _32BIT);
    }
    semData.push_back(execute);
    CHECK_RC(GetWrapper()->Write(0,
                                 LWC36F_SEM_ADDR_LO,
                                 static_cast<UINT32>(semData.size()),
                                 &semData[0]));
    return rc;
}

/* virtual */ RC VoltaGpFifoChannel::SemaphoreAcquireGE(UINT64 Data)
{
    RC rc;
    SemaphorePayloadSize payloadSize;
    CHECK_RC(GetActualSemaphorePayloadSize(Data, &payloadSize));

    UINT32 execute = DRF_DEF(C36F, _SEM_EXELWTE, _OPERATION, _ACQ_STRICT_GEQ);
    if (ChannelWrapperMgr::Instance()->UsesRunlistWrapper())
    {
        execute |= DRF_DEF(C36F, _SEM_EXELWTE, _ACQUIRE_SWITCH_TSG, _DIS);
    }
    else
    {
        execute |= DRF_DEF(C36F, _SEM_EXELWTE, _ACQUIRE_SWITCH_TSG, _EN);
    }

    vector<UINT32> semData;
    semData.push_back(static_cast<UINT32>(GetHostSemaOffset()));
    semData.push_back(static_cast<UINT32>(GetHostSemaOffset()>>32));
    semData.push_back(static_cast<UINT32>(Data));
    if ((GetSemaphorePayloadSize() == SEM_PAYLOAD_SIZE_64BIT) ||
        ((GetSemaphorePayloadSize() == SEM_PAYLOAD_SIZE_AUTO) && (Data >> 32)))
    {
        semData.push_back(static_cast<UINT32>(Data>>32));
        execute |= DRF_DEF(C36F, _SEM_EXELWTE, _PAYLOAD_SIZE, _64BIT);
    }
    else
    {
        semData.push_back(0);
        execute |= DRF_DEF(C36F, _SEM_EXELWTE, _PAYLOAD_SIZE, _32BIT);
    }
    semData.push_back(execute);
    CHECK_RC(GetWrapper()->Write(0,
                                 LWC36F_SEM_ADDR_LO,
                                 static_cast<UINT32>(semData.size()),
                                 &semData[0]));
    return rc;
}

/* virtual */ RC VoltaGpFifoChannel::SemaphoreRelease(UINT64 Data)
{
    RC rc;
    SemaphorePayloadSize payloadSize;
    CHECK_RC(GetActualSemaphorePayloadSize(Data, &payloadSize));

    UINT32 execute = DRF_DEF(C36F, _SEM_EXELWTE, _OPERATION, _RELEASE) |
                     (GetSemaphoreReleaseFlags() & FlagSemRelWithWFI?
                         DRF_DEF(C36F, _SEM_EXELWTE, _RELEASE_WFI, _EN):
                         DRF_DEF(C36F, _SEM_EXELWTE, _RELEASE_WFI, _DIS)) |
                     (GetSemaphoreReleaseFlags() & FlagSemRelWithTime?
                         DRF_DEF(C36F, _SEM_EXELWTE, _RELEASE_TIMESTAMP, _EN) :
                         DRF_DEF(C36F, _SEM_EXELWTE, _RELEASE_TIMESTAMP, _DIS));

    vector<UINT32> semData;
    semData.push_back(static_cast<UINT32>(GetHostSemaOffset()));
    semData.push_back(static_cast<UINT32>(GetHostSemaOffset()>>32));
    semData.push_back(static_cast<UINT32>(Data));
    if ((GetSemaphorePayloadSize() == SEM_PAYLOAD_SIZE_64BIT) ||
        ((GetSemaphorePayloadSize() == SEM_PAYLOAD_SIZE_AUTO) && (Data >> 32)))
    {
        semData.push_back(static_cast<UINT32>(Data>>32));
        execute |= DRF_DEF(C36F, _SEM_EXELWTE, _PAYLOAD_SIZE, _64BIT);
    }
    else
    {
        semData.push_back(0);
        execute |= DRF_DEF(C36F, _SEM_EXELWTE, _PAYLOAD_SIZE, _32BIT);
    }
    semData.push_back(execute);
    CHECK_RC(GetWrapper()->Write(0,
                                 LWC36F_SEM_ADDR_LO,
                                 static_cast<UINT32>(semData.size()),
                                 &semData[0]));
    return rc;
}

RC VoltaGpFifoChannel::SemaphoreReduction(Reduction Op, ReductionType redType, UINT64 Data)
{
    RC rc;
    SemaphorePayloadSize payloadSize;
    CHECK_RC(GetActualSemaphorePayloadSize(Data, &payloadSize));

    UINT32 execute = 0;
    execute |= DRF_DEF(C36F, _SEM_EXELWTE, _OPERATION, _REDUCTION);
    switch (redType)
    {
        case REDUCTION_UNSIGNED:
            execute |= DRF_DEF(C36F, _SEM_EXELWTE, _REDUCTION_FORMAT, _UNSIGNED);
            break;
        case REDUCTION_SIGNED:
            execute |= DRF_DEF(C36F, _SEM_EXELWTE, _REDUCTION_FORMAT, _SIGNED);
            break;
        default:
            MASSERT(!"Bad reduction type in SemaphoreReduction");
            return RC::SOFTWARE_ERROR;
    }
    execute |= (GetSemaphoreReleaseFlags() & FlagSemRelWithWFI?
                DRF_DEF(C36F, _SEM_EXELWTE, _RELEASE_WFI, _EN):
                DRF_DEF(C36F, _SEM_EXELWTE, _RELEASE_WFI, _DIS));
    execute |= (GetSemaphoreReleaseFlags() & FlagSemRelWithTime?
                DRF_DEF(C36F, _SEM_EXELWTE, _RELEASE_TIMESTAMP, _EN) :
                DRF_DEF(C36F, _SEM_EXELWTE, _RELEASE_TIMESTAMP, _DIS));
    switch (Op)
    {
        case REDUCTION_MIN:
            execute |= DRF_DEF(C36F, _SEM_EXELWTE, _REDUCTION, _IMIN);
            break;
        case REDUCTION_MAX:
            execute |= DRF_DEF(C36F, _SEM_EXELWTE, _REDUCTION, _IMAX);
            break;
        case REDUCTION_XOR:
            execute |= DRF_DEF(C36F, _SEM_EXELWTE, _REDUCTION, _IXOR);
            break;
        case REDUCTION_AND:
            execute |= DRF_DEF(C36F, _SEM_EXELWTE, _REDUCTION, _IAND);
            break;
        case REDUCTION_OR:
            execute |= DRF_DEF(C36F, _SEM_EXELWTE, _REDUCTION, _IOR);
            break;
        case REDUCTION_ADD:
            execute |= DRF_DEF(C36F, _SEM_EXELWTE, _REDUCTION, _IADD);
            break;
        case REDUCTION_INC:
            execute |= DRF_DEF(C36F, _SEM_EXELWTE, _REDUCTION, _INC);
            break;
        case REDUCTION_DEC:
            execute |= DRF_DEF(C36F, _SEM_EXELWTE, _REDUCTION, _DEC);
            break;
        default:
            MASSERT(!"Bad reduction in SemaphoreReduction");
            return RC::SOFTWARE_ERROR;
    }

    vector<UINT32> semData;
    semData.push_back(static_cast<UINT32>(GetHostSemaOffset()));
    semData.push_back(static_cast<UINT32>(GetHostSemaOffset()>>32));
    semData.push_back(static_cast<UINT32>(Data));
    if ((GetSemaphorePayloadSize() == SEM_PAYLOAD_SIZE_64BIT) ||
        ((GetSemaphorePayloadSize() == SEM_PAYLOAD_SIZE_AUTO) && (Data >> 32)))
    {
        semData.push_back(static_cast<UINT32>(Data>>32));
        execute |= DRF_DEF(C36F, _SEM_EXELWTE, _PAYLOAD_SIZE, _64BIT);
    }
    else
    {
        semData.push_back(0);
        execute |= DRF_DEF(C36F, _SEM_EXELWTE, _PAYLOAD_SIZE, _32BIT);
    }
    semData.push_back(execute);
    CHECK_RC(GetWrapper()->Write(0,
                                 LWC36F_SEM_ADDR_LO,
                                 static_cast<UINT32>(semData.size()),
                                 &semData[0]));
    return rc;
}

/* virtual */ UINT32 VoltaGpFifoChannel::GetHostSemaphoreSize() const
{
    return 6*sizeof(UINT32);
}

UINT64 VoltaGpFifoChannel::ReadGet(UINT32 i) const
{
    if (UseGetSemaphore())
    {
        const UINT32* ptr = static_cast<UINT32*>(GetSemaphoreAddress()) + s_SemaphoreDwords * i;
        const UINT32 get = MEM_RD32(ptr);
        const UINT32 getHi = MEM_RD32(ptr + 1);
        return (get | (UINT64(getHi) << 32));
    }
    return KeplerGpFifoChannel::ReadGet(i);
}

/* virtual */ RC VoltaGpFifoChannel::WriteGpGetSemaphore
(
    UINT64 semaOffset,
    UINT32 gpFifoEntries
)
{
    // We assume m_GpFifoEntries is a power of two, as required by HW.
    UINT32 semaValue = (m_LwrrentGpPut + 1) & (gpFifoEntries - 1);

    // 64b semaphores are not lwrrently implemented, when they are push
    // down the actual full offset so that no math is necessary on the read.
    // Until then, use the same method that previous chips did
    //
    UINT32 execute = DRF_DEF(C36F, _SEM_EXELWTE, _OPERATION, _RELEASE) |
                     DRF_DEF(C36F, _SEM_EXELWTE, _PAYLOAD_SIZE, _32BIT) |
                     DRF_DEF(C36F, _SEM_EXELWTE, _RELEASE_WFI, _DIS);

    const UINT32 data[] = {
        static_cast<UINT32>(semaOffset),
        static_cast<UINT32>(semaOffset >> 32),
        static_cast<UINT32>(semaValue),
        static_cast<UINT32>(0),
        execute
    };
    return WriteExternal(&m_pLwrrent, 0, LWC36F_SEM_ADDR_LO, 5, data);
}

/* virtual */ RC VoltaGpFifoChannel::WriteGetSemaphoreMethods
(
    UINT64 semaOffset,
    const UINT08* pPushBufBase
)
{
    // 64b semaphores are not lwrrently implemented, when they are push
    // down the actual full offset so that no math is necessary on the read.
    // Until then, use the same method that previous chips did
    //
    UINT32 execute = DRF_DEF(C36F, _SEM_EXELWTE, _OPERATION, _RELEASE) |
                     DRF_DEF(C36F, _SEM_EXELWTE, _PAYLOAD_SIZE, _64BIT) |
                     DRF_DEF(C36F, _SEM_EXELWTE, _RELEASE_WFI, _DIS);
    UINT64 semaValue = static_cast<UINT64>(reinterpret_cast<UINT08*>(m_pLwrrent) -
                                        pPushBufBase +
                                        PushBufferOffset() +
                                        GetHostSemaphoreSize());

    const UINT32 data[] = {
        static_cast<UINT32>(semaOffset),
        static_cast<UINT32>(semaOffset >> 32),
        static_cast<UINT32>(semaValue),
        static_cast<UINT32>(semaValue >> 32),
        execute
    };
    return WriteExternal(&m_pLwrrent, 0, LWC36F_SEM_ADDR_LO, 5, data);
}

RC VoltaGpFifoChannel::GetActualSemaphorePayloadSize(UINT64 data, SemaphorePayloadSize *pSize)
{
    *pSize = GetSemaphorePayloadSize();

    if (*pSize == SEM_PAYLOAD_SIZE_AUTO)
    {
        *pSize = (data >> 32) ? SEM_PAYLOAD_SIZE_64BIT : SEM_PAYLOAD_SIZE_32BIT;
    }
    else if (*pSize == SEM_PAYLOAD_SIZE_DEFAULT)
    {
        *pSize = SEM_PAYLOAD_SIZE_64BIT;
    }

    if ((*pSize == SEM_PAYLOAD_SIZE_32BIT) && (data >> 32))
    {
        Printf(Tee::PriError, "32 bit semaphore with 64 bit data!\n");
        return RC::BAD_PARAMETER;
    }

    return OK;
}

bool VoltaGpFifoChannel::GetDoorbellRingEnable() const
{
    return m_IsDoorbellEnabled;
}

RC VoltaGpFifoChannel::SetDoorbellRingEnable(bool enable)
{
    if (GetAutoFlushEnable() && !enable)
    {
        Printf(Tee::PriError, 
            "Disabling doorbell ring with autoflush enabled will interfere with MODS work submission process, hence it is not supported!\n");
        return RC::UNSUPPORTED_FUNCTION; 
    }

    m_IsDoorbellEnabled = enable;
    return RC::OK;
}

VoltaGpFifoChannel::DoorbellControl::DoorbellControl()
   : m_pLwRm(nullptr)
    ,m_pSubdev(nullptr)
    ,m_hCh(0)
    ,m_pUsermode(nullptr)
    ,m_pUserModeControl(nullptr)
    ,m_pDoorbell(nullptr)
    ,m_RingToken(ILWALID_RING_TOKEN)
{
}

RC VoltaGpFifoChannel::DoorbellControl::Alloc
(
    GpuSubdevice *pSubDev,
    LwRm * pLwRm,
    LwRm::Handle hCh,
    bool useBar1Alloc
)
{
    if (m_pLwRm)
    {
        Printf(Tee::PriHigh,
               "DoorbellControl already allocated, call Free before reallocation!\n");
        return RC::SOFTWARE_ERROR;
    }

    m_pLwRm = pLwRm;
    m_pSubdev = pSubDev;
    m_hCh = hCh;
    m_pUsermode = new UsermodeAlloc;

    RC rc;
    CHECK_RC(m_pUsermode->Alloc(m_pSubdev, m_pLwRm, useBar1Alloc));

    CHECK_RC(m_pLwRm->MapMemory(m_pUsermode->GetHandle(),
                                0,
                                DRF_SIZE(LWC361),
                                &m_pUserModeControl,
                                0,
                                m_pSubdev));

    m_pDoorbell =
        reinterpret_cast<volatile UINT32 *>(static_cast<UINT08 *>(m_pUserModeControl) +
                                            LWC361_NOTIFY_CHANNEL_PENDING);
    return rc;
}

void VoltaGpFifoChannel::DoorbellControl::Free()
{
    if (m_pUserModeControl)
    {
        m_pLwRm->UnmapMemory(m_pUsermode->GetHandle(),
                             m_pUserModeControl,
                             0,
                             m_pSubdev);
        m_pUserModeControl = nullptr;
        m_pDoorbell = nullptr;
    }
    if (m_pUsermode)
    {
        m_pUsermode->Free();
        delete m_pUsermode;
        m_pUsermode = nullptr;
    }
    m_pLwRm = nullptr;
    m_pSubdev = nullptr;
}

RC VoltaGpFifoChannel::DoorbellControl::Ring()
{
    MASSERT(IsInitialized());
    RC rc;

    if (m_RingToken == ILWALID_RING_TOKEN)
    {
        // Get the ring token the first time we call Ring(), since it
        // is not necessarily available at channel-creation time.
        LWC36F_CTRL_CMD_GPFIFO_GET_WORK_SUBMIT_TOKEN_PARAMS params = {};
        CHECK_RC(m_pLwRm->Control(m_hCh,
                                  LWC36F_CTRL_CMD_GPFIFO_GET_WORK_SUBMIT_TOKEN,
                                  &params,
                                  sizeof(params)));
        m_RingToken = params.workSubmitToken;
    }

    MEM_WR32(m_pDoorbell, m_RingToken);
    return OK;
}
