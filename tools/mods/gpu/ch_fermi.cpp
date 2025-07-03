/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2022 by LWPU Corporation. All rights
 * reserved. All information contained herein is proprietary and confidential
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Fermi Channel

#include <algorithm>

#include "lwtypes.h"
#include "core/include/channel.h"
#include "core/include/gpu.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpumgr.h"
#include "core/include/massert.h"
#include "core/include/platform.h"
#include "class/cl906f.h"   // GF100_CHANNEL_GPFIFO
#include "ctrl/ctrl906f.h"  // LW906F_CTRL_GET_CLASS_ENGINEID_PARAMS
#include "core/include/crc.h"
#include "gpu/utility/surf2d.h"
#include "core/include/utility.h"
#include "gpu/utility/chanwmgr.h"
#include "gpu/utility/atomwrap.h"
#include "core/include/jscript.h"

#include "fermi/gf100/dev_ram.h"

FermiGpFifoChannel::FermiGpFifoChannel
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
        LwRm *        pLwRm
)
:   GpFifoChannel(pUserdAlloc,
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
                  pLwRm),
    m_SemRelFlags(FlagSemRelDefault),
    m_SemaphorePayloadSize(SEM_PAYLOAD_SIZE_32BIT),
    m_WaitIdleValue(0),
    m_UseLegacyWaitIdleWithRiskyIntermittentDeadlocking(false),
    m_HostSemaOffset(~0ULL)
{
    m_HwCrcCheckMode = HWCRCCHKMODE_NONE,
    m_GpCrc = 0;
    m_PbCrc = 0;
    m_MethodCrc = 0;
    ChannelWrapperMgr::Instance()->UseSemaphoreWrapper();
}

FermiGpFifoChannel::~FermiGpFifoChannel()
{
}

/* virtual */ RC FermiGpFifoChannel::WriteExternal
(
    UINT32      ** ppExtBuf,
    UINT32         subchannel,
    UINT32         method,
    UINT32         count,
    const UINT32 * pData
)
{
    MASSERT(subchannel < NumSubchannels);
    MASSERT(count > 0);
    MASSERT(count <= DRF_MASK(LW_FIFO_DMA_INCR_COUNT));

    const UINT32 header =
        REF_DEF(LW_FIFO_DMA_INCR_OPCODE,      _VALUE) |
        REF_NUM(LW_FIFO_DMA_INCR_COUNT,       count) |
        REF_NUM(LW_FIFO_DMA_INCR_SUBCHANNEL,  subchannel) |
        REF_NUM(LW_FIFO_DMA_INCR_ADDRESS,     method >> 2);
    return WriteExternalImpl(ppExtBuf, header, count, pData);
}

/* virtual */ RC FermiGpFifoChannel::WriteExternalNonInc
(
    UINT32      ** ppExtBuf,
    UINT32         subchannel,
    UINT32         method,
    UINT32         count,
    const UINT32 * pData
)
{
    MASSERT(subchannel < NumSubchannels);
    MASSERT(count > 0);
    MASSERT(count <= DRF_MASK(LW_FIFO_DMA_NONINCR_COUNT));

    const UINT32 header =
        REF_DEF(LW_FIFO_DMA_NONINCR_OPCODE,      _VALUE) |
        REF_NUM(LW_FIFO_DMA_NONINCR_COUNT,       count) |
        REF_NUM(LW_FIFO_DMA_NONINCR_SUBCHANNEL,  subchannel) |
        REF_NUM(LW_FIFO_DMA_NONINCR_ADDRESS,     method >> 2);
    return WriteExternalImpl(ppExtBuf, header, count, pData);
}

/* virtual */ RC FermiGpFifoChannel::WriteExternalIncOnce
(
    UINT32      ** ppExtBuf,
    UINT32         subchannel,
    UINT32         method,
    UINT32         count,
    const UINT32 * pData
)
{
    MASSERT(subchannel < NumSubchannels);
    MASSERT(count > 0);
    MASSERT(count <= DRF_MASK(LW_FIFO_DMA_ONEINCR_COUNT));

    const UINT32 header =
        REF_DEF(LW_FIFO_DMA_ONEINCR_OPCODE,      _VALUE) |
        REF_NUM(LW_FIFO_DMA_ONEINCR_COUNT,       count) |
        REF_NUM(LW_FIFO_DMA_ONEINCR_SUBCHANNEL,  subchannel) |
        REF_NUM(LW_FIFO_DMA_ONEINCR_ADDRESS,     method >> 2);
    return WriteExternalImpl(ppExtBuf, header, count, pData);
}

/* virtual */ RC FermiGpFifoChannel::WriteExternalImmediate
(
    UINT32 ** ppExtBuf,
    UINT32    subchannel,
    UINT32    method,
    UINT32    data
)
{
    MASSERT(subchannel < NumSubchannels);
    MASSERT(data <= DRF_MASK(LW_FIFO_DMA_IMMD_DATA));

    const UINT32 header =
        REF_DEF(LW_FIFO_DMA_IMMD_OPCODE,      _VALUE) |
        REF_NUM(LW_FIFO_DMA_IMMD_DATA,        data) |
        REF_NUM(LW_FIFO_DMA_IMMD_SUBCHANNEL,  subchannel) |
        REF_NUM(LW_FIFO_DMA_IMMD_ADDRESS,     method >> 2);
    return WriteExternalImpl(ppExtBuf, header, 0, nullptr);
}

//! \brief Used to implement the other WriteExternalXXX() methods
//!
//! \param ppExtBuf The external buffer we're writing to.  The caller
//!     must ensure it has enough room.  On exit, *ppExtBuf points at
//!     the next available word in the buffer.
//! \param header The header word at the start of the method
//! \param count Number of data words in pData.  Ignored if pData is nullptr
//! \param pData Data words that follow the header, or nullptr for header only.
//!
RC FermiGpFifoChannel::WriteExternalImpl
(
    UINT32      ** ppExtBuf,
    UINT32         header,
    UINT32         count,
    const UINT32 * pData
)
{
    MASSERT(ppExtBuf);
    MASSERT(*ppExtBuf);

    MEM_WR32((*ppExtBuf)++, header);
    if (pData)
    {
        Platform::VirtualWr(*ppExtBuf, pData, count * sizeof(UINT32));
        *ppExtBuf += count;
    }
    return OK;
}

/* virtual */ RC FermiGpFifoChannel::Write
(
    UINT32 Subchannel,
    UINT32 Method,
    UINT32 Data
)
{
    RC rc;

    CHECK_RC(MakeRoom(2));
    UINT32 *const pBase = m_pLwrrent;
    CHECK_RC(WriteExternal(&m_pLwrrent, Subchannel, Method, 1, &Data));
    CHECK_RC(CallwlatePbMthdCrc(pBase, UNSIGNED_CAST(UINT32, m_pLwrrent - pBase)));
    CHECK_RC(AutoFlushIfNeeded());
    return rc;
}

/* virtual */ RC FermiGpFifoChannel::Write
(
    UINT32 subchannel,
    UINT32 method,
    UINT32 count,
    UINT32 data,
    ... // Rest of Data
)
{
    RC rc;

    MASSERT(count != 0);

    if (count > DRF_MASK(LW_FIFO_DMA_INCR_COUNT))
        return RC::METHOD_COUNT_TOO_LARGE;

    // Copy the data from the va_list
    vector<UINT32> dataVector(count);
    va_list dataVa;
    va_start(dataVa, data);
    dataVector[0] = data;
    for (UINT32 ii = 1; ii < count; ++ii)
        dataVector[ii] = va_arg(dataVa, UINT32);
    va_end(dataVa);

    // Write the data to the push buffer.
    CHECK_RC(MakeRoom(1 + count));
    UINT32 *const pBase = m_pLwrrent;
    CHECK_RC(WriteExternal(&m_pLwrrent, subchannel,
                           method, count, &dataVector[0]));
    CHECK_RC(CallwlatePbMthdCrc(pBase, UNSIGNED_CAST(UINT32, m_pLwrrent - pBase)));
    CHECK_RC(AutoFlushIfNeeded());
    return rc;
}

/* virtual */ RC FermiGpFifoChannel::Write
(
    UINT32         Subchannel,
    UINT32         Method,
    UINT32         Count,
    const UINT32 * pData
)
{
    RC rc;

    MASSERT(Count != 0);
    MASSERT(pData != 0);

    if (Count > DRF_MASK(LW_FIFO_DMA_INCR_COUNT))
        return RC::METHOD_COUNT_TOO_LARGE;

    CHECK_RC(MakeRoom(1 + Count));
    UINT32 *const pBase = m_pLwrrent;
    CHECK_RC(WriteExternal(&m_pLwrrent, Subchannel, Method, Count, pData));
    CHECK_RC(CallwlatePbMthdCrc(pBase, UNSIGNED_CAST(UINT32, m_pLwrrent - pBase)));
    CHECK_RC(AutoFlushIfNeeded());
    return rc;
}

/* virtual */ RC FermiGpFifoChannel::WriteNonInc
(
    UINT32         Subchannel,
    UINT32         Method,
    UINT32         Count,
    const UINT32 * pData
)
{
    RC rc;

    MASSERT(Subchannel < NumSubchannels);
    MASSERT(Count != 0);
    MASSERT(pData != 0);

    if (Count > DRF_MASK(LW_FIFO_DMA_NONINCR_COUNT))
        return RC::METHOD_COUNT_TOO_LARGE;

    CHECK_RC(MakeRoom(1 + Count));
    UINT32 *const pBase = m_pLwrrent;
    CHECK_RC(WriteExternalNonInc(&m_pLwrrent, Subchannel,
                                 Method, Count, pData));
    CHECK_RC(CallwlatePbMthdCrc(pBase, UNSIGNED_CAST(UINT32, m_pLwrrent - pBase)));
    CHECK_RC(AutoFlushIfNeeded());
    return rc;
}

/* virtual */ RC FermiGpFifoChannel::WriteIncOnce
(
    UINT32          Subchannel,
    UINT32          Method,
    UINT32          Count,
    const UINT32 *  pData
)
{
    RC rc;

    MASSERT(Subchannel < NumSubchannels);
    MASSERT(Count != 0);
    MASSERT(pData != 0);

    if (Count > DRF_MASK(LW_FIFO_DMA_ONEINCR_COUNT))
        return RC::METHOD_COUNT_TOO_LARGE;

    CHECK_RC(MakeRoom(1 + Count));
    UINT32 *const pBase = m_pLwrrent;
    CHECK_RC(WriteExternalIncOnce(&m_pLwrrent, Subchannel,
                                  Method, Count, pData));
    CHECK_RC(CallwlatePbMthdCrc(pBase, UNSIGNED_CAST(UINT32, m_pLwrrent - pBase)));
    CHECK_RC(AutoFlushIfNeeded());
    return rc;
}

/* virtual */ RC FermiGpFifoChannel::WriteImmediate
(
    UINT32         Subchannel,
    UINT32         Method,
    UINT32         Data
)
{
    RC rc;

    MASSERT(Subchannel < NumSubchannels);

    if (Data > DRF_MASK(LW_FIFO_DMA_IMMD_DATA))
        return RC::BAD_PARAMETER;

    CHECK_RC(MakeRoom(1));
    UINT32 *const pBase = m_pLwrrent;
    CHECK_RC(WriteExternalImmediate(&m_pLwrrent, Subchannel, Method, Data));
    CHECK_RC(CallwlatePbMthdCrc(pBase, UNSIGNED_CAST(UINT32, m_pLwrrent - pBase)));
    CHECK_RC(AutoFlushIfNeeded());
    return rc;
}

/* virtual */ RC FermiGpFifoChannel::WriteHeader
(
    UINT32 Subchannel,
    UINT32 Method,
    UINT32 Count
)
{
    RC rc;

    MASSERT(Subchannel < NumSubchannels);
    MASSERT(Count != 0);

    if (Count > DRF_MASK(LW_FIFO_DMA_INCR_COUNT))
        return RC::METHOD_COUNT_TOO_LARGE;

    CHECK_RC(MakeRoom(1));
    UINT32 *const pBase = m_pLwrrent;
    CHECK_RC(WriteExternal(&m_pLwrrent, Subchannel, Method, Count, nullptr));
    CHECK_RC(CallwlatePbMthdCrc(pBase, UNSIGNED_CAST(UINT32, m_pLwrrent - pBase)));
    CHECK_RC(AutoFlushIfNeeded());
    return rc;
}

/* virtual */ RC FermiGpFifoChannel::WriteNonIncHeader
(
    UINT32 Subchannel,
    UINT32 Method,
    UINT32 Count
)
{
    RC rc;

    MASSERT(Subchannel < NumSubchannels);
    MASSERT(Count != 0);

    if (Count > DRF_MASK(LW_FIFO_DMA_NONINCR_COUNT))
        return RC::METHOD_COUNT_TOO_LARGE;

    CHECK_RC(MakeRoom(1));
    UINT32 *const pBase = m_pLwrrent;
    CHECK_RC(WriteExternalNonInc(&m_pLwrrent, Subchannel,
                                 Method, Count, nullptr));
    CHECK_RC(CallwlatePbMthdCrc(pBase, UNSIGNED_CAST(UINT32, m_pLwrrent - pBase)));
    CHECK_RC(AutoFlushIfNeeded());
    return rc;
}

/* virtual */ RC FermiGpFifoChannel::WriteNop()
{
    RC rc;
    const UINT32 method = LW_FIFO_DMA_NOP;

    CHECK_RC(MakeRoom(1));
    MEM_WR32(m_pLwrrent++, method);
    CHECK_RC(CallwlatePbMthdCrc(&method, 1));
    CHECK_RC(AutoFlushIfNeeded());
    return rc;
}

/* virtual */ RC FermiGpFifoChannel::WriteSetSubdevice(UINT32 Subdevice)
{
    RC rc;
    const UINT32 method =
        REF_DEF(LW_FIFO_DMA_SET_SUBDEVICE_MASK_OPCODE, _VALUE) |
        REF_NUM(LW_FIFO_DMA_SET_SUBDEVICE_MASK_VALUE,  Subdevice);

    if (Subdevice > DRF_MASK(LW_FIFO_DMA_SET_SUBDEVICE_MASK_VALUE))
        return RC::BAD_PARAMETER;

    CHECK_RC(MakeRoom(1));
    MEM_WR32(m_pLwrrent++, method);
    CHECK_RC(CallwlatePbMthdCrc(&method, 1));
    CHECK_RC(AutoFlushIfNeeded());
    return rc;
}

/* virtual */ RC FermiGpFifoChannel::InsertSubroutine
(
    const UINT32 *pBuffer,
    UINT32 count
)
{
    const UINT32 maxCount = GetGpEntry1LengthMask();
    RC rc;

    while (count > 0)
    {
        const UINT32 countThisLoop = min(count, maxCount);
        CHECK_RC(MakeRoom(countThisLoop));
        Platform::VirtualWr(m_pLwrrent, pBuffer,
                            countThisLoop * sizeof(UINT32));
        m_pLwrrent += countThisLoop;
        CHECK_RC(CallwlatePbMthdCrc(pBuffer, countThisLoop));
        CHECK_RC(AutoFlushIfNeeded());
        pBuffer += countThisLoop;
        count -= countThisLoop;
    }
    return rc;
}

/* virtual */ RC FermiGpFifoChannel::WriteCrcChkMethod()
{
    RC rc;

    if ((m_MethodCrc != 0) &&
        (m_HwCrcCheckMode & HWCRCCHKMODE_MTHD))
    {
        CHECK_RC(Write(0, LW906F_CRC_CHECK, m_MethodCrc));
        m_MethodCrc = 0; // Clear for next round
    }
    return rc;
}

/* virtual */ RC FermiGpFifoChannel::WriteGpCrcChkGpEntry()
{
    RC rc;

    if ((m_GpCrc != 0) &&
        (m_HwCrcCheckMode & HWCRCCHKMODE_GP))
    {
        UINT32 opCode = DRF_DEF(906F, _GP_ENTRY1, _OPCODE, _GP_CRC);
        UINT32 GpEntry0 = DRF_NUM(906F, _GP_ENTRY0, _OPERAND, m_GpCrc);
        UINT32 GpEntry1 = DRF_NUM(906F, _GP_ENTRY1, _LENGTH, 0) | opCode;
        m_GpCrc = 0; // clear for next round

        CHECK_RC(WriteGpEntryData(GpEntry0, GpEntry1));
    }

    return rc;
}

//! Fermi implementation of SetObject, which translates the Handle
//! parameter into the (engine,class) format recognized by fermi.
//!
/* virtual */ RC FermiGpFifoChannel::SetObject
(
    UINT32 Subchannel,
    LwRm::Handle Handle
)
{
    UINT32 ClassEngineId;
    RC rc;

    CHECK_RC(RmcGetClassEngineId(Handle, &ClassEngineId, NULL, NULL));
    CHECK_RC(Write(Subchannel, LW906F_SET_OBJECT, ClassEngineId));
    return rc;
}

/* virtual */ RC FermiGpFifoChannel::SetContextDmaSemaphore
(
    LwRm::Handle hCtxDma
)
{
    // Fermi eliminated need for context dmas, but RM still supports them
    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ void FermiGpFifoChannel::SetSemaphoreReleaseFlags(UINT32 flags)
{
    m_SemRelFlags = flags;
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 FermiGpFifoChannel::GetSemaphoreReleaseFlags()
{
    return m_SemRelFlags;
}

//------------------------------------------------------------------------------
/* virtual */ RC FermiGpFifoChannel::SetSemaphorePayloadSize(SemaphorePayloadSize size)
{
    if (!Has64bitSemaphores() && (size == SEM_PAYLOAD_SIZE_64BIT))
    {
        Printf(Tee::PriError, "64bit semaphores are not supported!\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    m_SemaphorePayloadSize = size;
    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ Channel::SemaphorePayloadSize FermiGpFifoChannel::GetSemaphorePayloadSize()
{
    return m_SemaphorePayloadSize;
}

/* virtual */ RC FermiGpFifoChannel::SetSemaphoreOffset(UINT64 offset)
{
    m_HostSemaOffset = offset;
    return OK;
}

/* virtual */ RC FermiGpFifoChannel::SemaphoreAcquire(UINT64 Data)
{
    if (Data >> 32)
    {
        Printf(Tee::PriError, "64bit semaphores are not supported!\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    UINT32 semaphoreD = DRF_DEF(906F, _SEMAPHORED, _OPERATION, _ACQUIRE);

    if (ChannelWrapperMgr::Instance()->UsesRunlistWrapper())
    {
        semaphoreD |= DRF_DEF(906F, _SEMAPHORED, _ACQUIRE_SWITCH, _DISABLED);
    }
    else
    {
        semaphoreD |= DRF_DEF(906F, _SEMAPHORED, _ACQUIRE_SWITCH, _ENABLED);
    }

    return GetWrapper()->Write(
                0, LW906F_SEMAPHOREA, 4,
                static_cast<UINT32>(m_HostSemaOffset>>32),
                static_cast<UINT32>(m_HostSemaOffset),
                static_cast<UINT32>(Data),
                semaphoreD);
}

/* virtual */ RC FermiGpFifoChannel::SemaphoreAcquireGE(UINT64 Data)
{
    if (Data >> 32)
    {
        Printf(Tee::PriError, "64bit semaphores are not supported!\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    UINT32 semaphoreD = DRF_DEF(906F, _SEMAPHORED, _OPERATION, _ACQ_GEQ);

    if (ChannelWrapperMgr::Instance()->UsesRunlistWrapper())
    {
        semaphoreD |= DRF_DEF(906F, _SEMAPHORED, _ACQUIRE_SWITCH, _DISABLED);
    }
    else
    {
        semaphoreD |= DRF_DEF(906F, _SEMAPHORED, _ACQUIRE_SWITCH, _ENABLED);
    }

    return GetWrapper()->Write(
                0, LW906F_SEMAPHOREA, 4,
                static_cast<UINT32>(m_HostSemaOffset>>32),
                static_cast<UINT32>(m_HostSemaOffset),
                static_cast<UINT32>(Data),
                semaphoreD);
}

/* virtual */ RC FermiGpFifoChannel::SemaphoreRelease(UINT64 Data)
{
    if (Data >> 32)
    {
        Printf(Tee::PriError, "64bit semaphores are not supported!\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    return GetWrapper()->Write(
                0, LW906F_SEMAPHOREA, 4,
                static_cast<UINT32>(m_HostSemaOffset>>32),
                static_cast<UINT32>(m_HostSemaOffset),
                static_cast<UINT32>(Data),
                DRF_DEF(906F, _SEMAPHORED, _OPERATION, _RELEASE) |
                (m_SemRelFlags & FlagSemRelWithWFI?
                    DRF_DEF(906F, _SEMAPHORED, _RELEASE_WFI, _EN):
                    DRF_DEF(906F, _SEMAPHORED, _RELEASE_WFI, _DIS)) |
                (m_SemRelFlags & FlagSemRelWithTime?
                    DRF_DEF(906F, _SEMAPHORED, _RELEASE_SIZE, _16BYTE) :
                    DRF_DEF(906F, _SEMAPHORED, _RELEASE_SIZE, _4BYTE)));
}

/* virtual */ UINT32 FermiGpFifoChannel::GetHostSemaphoreSize() const
{
    return 5*sizeof(UINT32);
}

/* virtual */ RC FermiGpFifoChannel::WriteGetSemaphore
(
    UINT64 semaOffset,
    const UINT08* pPushBufBase
)
{
    RC rc;
    UINT32 *const pBase = m_pLwrrent;
    CHECK_RC(WriteGetSemaphoreMethods(semaOffset, pPushBufBase));
    CHECK_RC(CallwlatePbMthdCrc(pBase, UNSIGNED_CAST(UINT32, m_pLwrrent - pBase)));
    return OK;
}

/* virtual */ RC FermiGpFifoChannel::WriteGetSemaphoreMethods
(
    UINT64 semaOffset,
    const UINT08* pPushBufBase
)
{
    const UINT32 semaValue = static_cast<UINT32>(
        reinterpret_cast<UINT08*>(m_pLwrrent) - pPushBufBase + GetHostSemaphoreSize());

    const UINT32 data[] = {
        static_cast<UINT32>(semaOffset >> 32),
        static_cast<UINT32>(semaOffset),
        semaValue,
        DRF_DEF(906F, _SEMAPHORED, _OPERATION, _RELEASE) |
            DRF_DEF(906F, _SEMAPHORED, _RELEASE_WFI, _DIS) |
            DRF_DEF(906F, _SEMAPHORED, _RELEASE_SIZE, _4BYTE)
    };
    return WriteExternal(&m_pLwrrent, 0, LW906F_SEMAPHOREA, 4, data);
}

/* virtual */ RC FermiGpFifoChannel::WriteGpGetSemaphore
(
    UINT64 semaOffset,
    UINT32 gpFifoEntries
)
{
    // We assume m_GpFifoEntries is a power of two, as required by HW.
    UINT32 nextGpPut = (m_LwrrentGpPut + 1) & (gpFifoEntries - 1);

    const UINT32 data[] = {
        static_cast<UINT32>(semaOffset >> 32),
        static_cast<UINT32>(semaOffset),
        nextGpPut,
        DRF_DEF(906F, _SEMAPHORED, _OPERATION, _RELEASE) |
            DRF_DEF(906F, _SEMAPHORED, _RELEASE_WFI, _DIS) |
            DRF_DEF(906F, _SEMAPHORED, _RELEASE_SIZE, _4BYTE)
    };
    return WriteExternal(&m_pLwrrent, 0, LW906F_SEMAPHOREA, 4, data);
}

/* virtual */ RC FermiGpFifoChannel::SetSyncEnable(bool Sync)
{
    m_SyncWait = Sync;
    return OK;
}
RC FermiGpFifoChannel::WaitIdle(FLOAT64 TimeoutMs)
{
    // In legacy mode, use the old WaitIdle which calls RmIdleChannels()
    //
    if (m_UseLegacyWaitIdleWithRiskyIntermittentDeadlocking)
    {
        Printf(Tee::PriHigh,
               "WARNING : USING STATUS REGISTER POLLING FOR CHANNEL IDLE!!\n");
        return BaseChannel::WaitIdle(TimeoutMs);
    }

    // If we get this far, we're using the preferred way to wait for
    // the channel to idle: push a semaphore with the WFI bit, and
    // wait for the semaphore to be released.
    //
    RC rc;
    GpuDevice *pDevice = GetGpuDevice();
    Channel *pWrapper = GetWrapper();

    CHECK_RC(CheckError());   // Abort if pending error, before we add to PB

    if (m_pWaitIdleSem.get() == NULL)
    {
        m_pWaitIdleSem.reset(new Surface2D);
        if (m_pWaitIdleSem.get() == NULL)
            return RC::CANNOT_ALLOCATE_MEMORY;

        m_pWaitIdleSem->SetWidth(pDevice->GetNumSubdevices() *
                                 WAIT_IDLE_SEM_SIZE);
        m_pWaitIdleSem->SetHeight(1);
        m_pWaitIdleSem->SetPageSize(4);
        m_pWaitIdleSem->SetColorFormat(ColorUtils::Y8);
        if (Platform::IsSelfHosted() || Platform::IsCCMode())
        {
            m_pWaitIdleSem->SetLocation(Memory::Fb);
        }
        else
        {
            m_pWaitIdleSem->SetLocation(Memory::Coherent);
        }
        if (Platform::GetSimulationMode() == Platform::Amodel)
        {
            // Amodel by default uses segmentation mode for buffer allocation
            // Since there is no way for fermi to setup contextdma, we have
            // to force paging mode in this case
            m_pWaitIdleSem->SetAddressModel(Memory::Paging);
        }
        m_pWaitIdleSem->SetGpuVASpace(GetVASpace());
        m_pWaitIdleSem->SetVASpace(Surface2D::GPUVASpace);
        m_pWaitIdleSem->SetVaReverse(((GpuDevMgr*) DevMgrMgr::d_GraphDevMgr)->GetVasReverse());
        CHECK_RC(m_pWaitIdleSem->Alloc(pDevice, GetRmClient()));
        CHECK_RC(m_pWaitIdleSem->BindGpuChannel(GetHandle(), GetRmClient()));
        CHECK_RC(m_pWaitIdleSem->Map());
        CHECK_RC(m_pWaitIdleSem->Fill(0));
    }

    // Insert the semaphore
    //
    {
        CHECK_RC(pWrapper->BeginInsertedMethods());
        Utility::CleanupOnReturn<Channel>
            CleanupInsertedMethods(pWrapper, &Channel::CancelInsertedMethods);
        UINT64 Offset = m_pWaitIdleSem->GetCtxDmaOffsetGpu(GetRmClient());
        m_WaitIdleValue++;
        pWrapper->SetSemaphoreReleaseFlags(FlagSemRelDefault);
        for (UINT32 subdev =  0;
             subdev < pDevice->GetNumSubdevices(); ++subdev)
        {
            if (pDevice->GetNumSubdevices() > 1)
                CHECK_RC(pWrapper->WriteSetSubdevice(1 << subdev));
            CHECK_RC(pWrapper->SetSemaphoreOffset(
                         Offset + subdev * WAIT_IDLE_SEM_SIZE));
            CHECK_RC(pWrapper->SemaphoreRelease(m_WaitIdleValue));
        }
        CleanupInsertedMethods.Cancel();
        CHECK_RC(pWrapper->EndInsertedMethods());
    }

    CHECK_RC(pWrapper->Flush());

    if (Xp::GetOperatingSystem() == Xp::OS_LINUXSIM ||
        Xp::GetOperatingSystem() == Xp::OS_WINSIM)
    {
        PHYSADDR semPhysAddr = 0;
        CHECK_RC(m_pWaitIdleSem->GetPhysAddress(0, &semPhysAddr, GetRmClient()));
        Printf(Tee::PriDebug, "%s: Polling channel id %u semaphore address 0x%llx (virtual)"
               " - 0x%llx (physical) for value %u\n",
               __FUNCTION__, GetChannelId() , m_pWaitIdleSem->GetCtxDmaOffsetGpu(),
               semPhysAddr, m_WaitIdleValue);
    }

    // Wait for the semaphore.
    //
    UINT32 retries = 0;
    do
    {
        rc = POLLWRAP(FermiGpFifoChannel::PollIdleSemaphore, this, TimeoutMs);
    } while ((++retries < m_IdleChannelsRetries) &&
             (rc == RC::TIMEOUT_ERROR));

    if (OK == rc)
    {
        rc = CheckError();
    }
    // USER_ABORTED_SCRIPT is returned only if there was no other error
    else if (RC::USER_ABORTED_SCRIPT != rc)
    {
        Printf(Tee::PriHigh, "%#010x channel could not be idled.\n", m_hChannel);
        if (JavaScriptPtr::IsInstalled()) // This code is also called from OnExit
                                          //  when JS no longer exists
        {
            JavaScriptPtr pJs;
            pJs->CallMethod(pJs->GetGlobalObject(), "ChannelWaitIdleErrorCallback");
        }
        Tee::FlushToDisk();
    }

    // Informational escape write
    if (Platform::GetSimulationMode() != Platform::Hardware &&
        !Platform::UsesLwgpuDriver())
    {
        const UINT32 value = 0;
        m_pGrDev->EscapeWriteBuffer("WaitForIdle", 0, 4, &value);
    }

    return rc;
}

/* virtual */
RC FermiGpFifoChannel::SetLegacyWaitIdleWithRiskyIntermittentDeadlocking
(
    bool Enable
)
{
    if (Enable)
    {
        Printf(Tee::PriHigh,
               "WARNING : USING STATUS REGISTER POLLING FOR CHANNEL IDLE!!\n");
    }
    m_UseLegacyWaitIdleWithRiskyIntermittentDeadlocking = Enable;
    return OK;
}

/* virtual */
bool FermiGpFifoChannel::GetLegacyWaitIdleWithRiskyIntermittentDeadlocking
(
) const
{
    return m_UseLegacyWaitIdleWithRiskyIntermittentDeadlocking;
}

/* virtual */ RC FermiGpFifoChannel::SetHwCrcCheckMode(UINT32 Mode)
{
    m_HwCrcCheckMode = Mode;
    return OK;
}

/* virtual */ RC FermiGpFifoChannel::RmcGetClassEngineId
(
    LwRm::Handle hObject,
    UINT32 *pClassEngineId,
    UINT32 *pClassId,
    UINT32 *pEngineId
)
{
    LwRm *pLwRm = GetRmClient();
    RC rc;

    LW906F_CTRL_GET_CLASS_ENGINEID_PARAMS Params = {0};
    Params.hObject = hObject;
    CHECK_RC(pLwRm->Control(GetHandle(), LW906F_CTRL_GET_CLASS_ENGINEID,
                            &Params, sizeof(Params)));
    if (pClassEngineId)
        *pClassEngineId = Params.classEngineID;
    if (pClassId)
        *pClassId = Params.classID;
    if (pEngineId)
        *pEngineId = Params.engineID;

    return rc;
}

/* virtual */ RC FermiGpFifoChannel::RmcResetChannel(UINT32 EngineId)
{
    LW906F_CTRL_CMD_RESET_CHANNEL_PARAMS Params = {0};
    Params.engineID    = EngineId;
    Params.resetReason = LW906F_CTRL_CMD_RESET_CHANNEL_REASON_DEFAULT;
    return GetRmClient()->Control(GetHandle(), LW906F_CTRL_CMD_RESET_CHANNEL,
                                  &Params, sizeof(Params));
}

/* virtual */ RC FermiGpFifoChannel::WriteGpEntryData
(
    UINT32 GpEntry0,
    UINT32 GpEntry1
)
{
    RC rc;

    CHECK_RC(GpFifoChannel::WriteGpEntryData(GpEntry0, GpEntry1));

    if (m_HwCrcCheckMode & HWCRCCHKMODE_GP)
    {
        if (DRF_DEF(906F, _GP_ENTRY1, _OPCODE, _GP_CRC) !=
            DRF_NUM(906F, _GP_ENTRY1, _OPCODE, GpEntry1))
        {
            UINT64 dataGpEntry = ((UINT64)GpEntry1 << 32) | GpEntry0;
            m_GpCrc = ~Crc::Append(&dataGpEntry, sizeof(dataGpEntry), ~m_GpCrc);
        }
    }

    return rc;
}

/* virtual */ RC FermiGpFifoChannel::WritePbCrcCheckGpEntry
(
    const void *pGpEntryData,
    UINT32 Length
)
{
    RC rc;

    if ((m_PbCrc != 0) &&
        (m_HwCrcCheckMode & HWCRCCHKMODE_PB))
    {
        // If GP_ENTRY0_FETCH field is FETCH_CONDITIONAL,
        // then the PB segment is only fetched
        // if the LW_PPBDMA_SUBDEVICE_STATUS field is STATUS_ACTIVE.
        // PB_CRC check has not implemented for this case.
        if (DRF_NUM(906F, _GP_ENTRY0, _FETCH, *(const UINT32*)pGpEntryData)
            == DRF_DEF(906F, _GP_ENTRY0, _FETCH, _CONDITIONAL))
        {
            return RC::UNSUPPORTED_FUNCTION;
        }

        UINT32 opCode = DRF_DEF(906F, _GP_ENTRY1, _OPCODE, _PB_CRC);

        UINT32 GpEntry0 = DRF_NUM(906F, _GP_ENTRY0, _OPERAND, m_PbCrc);
        UINT32 GpEntry1 = DRF_NUM(906F, _GP_ENTRY1, _LENGTH, 0) | opCode;
        m_PbCrc = 0; // clear for next round

        CHECK_RC(WriteGpEntryData(GpEntry0, GpEntry1));
    }

    return rc;
}

/* virtual */ RC FermiGpFifoChannel::ConstructGpEntryData
(
    UINT64 Get,
    UINT32 Length,
    bool Subroutine,
    UINT32* pGpEntry0,
    UINT32* pGpEntry1
)
{
    UINT32 GpEntry0 =
        DRF_DEF(906F, _GP_ENTRY0, _FETCH, _UNCONDITIONAL) |
        DRF_DEF(906F, _GP_ENTRY0, _NO_CONTEXT_SWITCH, _FALSE) |
        DRF_NUM(906F, _GP_ENTRY0, _GET, LwU64_LO32(Get) >> 2);
    UINT32 GpEntry1 =
        DRF_NUM(906F, _GP_ENTRY1, _GET_HI, LwU64_HI32(Get)) |
        DRF_NUM(906F, _GP_ENTRY1, _LENGTH, Length >> 2);

    if (GetPrivEnable())
        GpEntry1 |= DRF_DEF(906F, _GP_ENTRY1, _PRIV, _KERNEL);
    else
        GpEntry1 |= DRF_DEF(906F, _GP_ENTRY1, _PRIV, _USER);

    if (Subroutine)
        GpEntry1 |= DRF_DEF(906F, _GP_ENTRY1, _LEVEL, _SUBROUTINE);
    else
        GpEntry1 |= DRF_DEF(906F, _GP_ENTRY1, _LEVEL, _MAIN);

    if (m_SyncWait)
        GpEntry1 |= DRF_DEF(906F, _GP_ENTRY1, _SYNC, _WAIT);
    else
        GpEntry1 |= DRF_DEF(906F, _GP_ENTRY1, _SYNC, _PROCEED);

    *pGpEntry0 = GpEntry0;
    *pGpEntry1 = GpEntry1;

    return OK;
}

/* virtual */ UINT32 FermiGpFifoChannel::GetGpEntry1LengthMask() const
{
    return DRF_MASK(LW906F_GP_ENTRY1_LENGTH);
}

RC FermiGpFifoChannel::CallwlatePbMthdCrc(const UINT32 *pData, UINT32 count)
{
    MASSERT(pData != nullptr);

    // Update m_PbCrc
    //
    if (m_HwCrcCheckMode & HWCRCCHKMODE_PB)
    {
        m_PbCrc = ~Crc::Append(pData, count * 4, ~m_PbCrc);
    }

    // Update m_MethodCrc by looping through pData and parsing the
    // method/data info.
    //
    // pData must start on a method header.  pData is allowed to
    // truncate the data after the last method header, in which case
    // the remaining data words must be inside of a CallSubroutine
    // gpEntry.  We don't lwrrently support passing the remaining data
    // words in a separate call to CallwlatePbMthdCrc().
    //
    // Method CRC algorithm is based on Pbdma::updateMethodCrc(uint, uint)
    // in pbdma.cpp of hw tree.
    //
    if (m_HwCrcCheckMode & HWCRCCHKMODE_MTHD)
    {
        UINT32 index = 0;
        while (index < count)
        {
            UINT32 header = pData[index];
            UINT32 opcode = REF_VAL(LW_FIFO_DMA_SEC_OP,            header);
            UINT32 subch  = REF_VAL(LW_FIFO_DMA_METHOD_SUBCHANNEL, header);
            UINT32 addr   = REF_VAL(LW_FIFO_DMA_METHOD_ADDRESS,    header);

            // The LW_PPBDMA_METHOD_CRC register contains a cyclic
            // redundancy check value callwlated from the methods sent
            // to Host's Crossbar.  This therefore excludes software
            // methods and host-only methods.
            //
            const bool isValidCrcMethod = (addr << 2) >= 0x100 || addr == 0;

            switch (opcode)
            {
                case LW_FIFO_DMA_SEC_OP_INC_METHOD:
                case LW_FIFO_DMA_SEC_OP_NON_INC_METHOD:
                case LW_FIFO_DMA_SEC_OP_ONE_INC:
                {
                    UINT32 dataCount = min(
                            REF_VAL(LW_FIFO_DMA_METHOD_COUNT, header),
                            count - index - 1);
                    UINT32 crcCount = isValidCrcMethod ? dataCount : 0;
                    for (UINT32 ii = 0; ii < crcCount; ++ii)
                    {
                        UINT32 data4B = pData[index + 1 + ii];
                        UINT32 meth2B =
                            REF_NUM(LW_FIFO_DMA_METHOD_SUBCHANNEL, subch) |
                            REF_NUM(LW_FIFO_DMA_METHOD_ADDRESS, addr);
                        m_MethodCrc = ~Crc::Append(&data4B, 4, ~m_MethodCrc);
                        m_MethodCrc = ~Crc::Append(&meth2B, 2, ~m_MethodCrc);
                        if (opcode == LW_FIFO_DMA_SEC_OP_INC_METHOD ||
                            (opcode == LW_FIFO_DMA_SEC_OP_ONE_INC && ii == 0))
                        {
                            ++addr;
                        }
                    }
                    index += dataCount + 1;
                    break;
                }
                case LW_FIFO_DMA_SEC_OP_IMMD_DATA_METHOD:
                {
                    if (isValidCrcMethod)
                    {
                        UINT32 data4B = REF_VAL(LW_FIFO_DMA_IMMD_DATA, header);
                        UINT32 meth2B =
                            REF_NUM(LW_FIFO_DMA_METHOD_SUBCHANNEL, subch) |
                            REF_NUM(LW_FIFO_DMA_METHOD_ADDRESS, addr);
                        m_MethodCrc = ~Crc::Append(&data4B, 4, ~m_MethodCrc);
                        m_MethodCrc = ~Crc::Append(&meth2B, 2, ~m_MethodCrc);
                    }
                    index += 1;
                    break;
                }
                case LW_FIFO_DMA_SEC_OP_GRP0_USE_TERT:
                {
                    if (header != LW_FIFO_DMA_NOP &&
                        !FLD_TEST_REF(LW_FIFO_DMA_SET_SUBDEVICE_MASK_OPCODE,
                                      _VALUE, header))
                    {
                        MASSERT(!"Method mode invalid or not supported.");
                        return RC::SOFTWARE_ERROR;
                    }
                    // Ignore NOP and SET_SUBDEVICE_MASK
                    index += 1;
                    break;
                }
                default:
                {
                    MASSERT(!"Method mode invalid or not supported.");
                    return RC::SOFTWARE_ERROR;
                }
            }
        }
    }
    return OK;
}

bool FermiGpFifoChannel::PollIdleSemaphore(void *pvArgs)
{
    FermiGpFifoChannel *pThis = static_cast<FermiGpFifoChannel *>(pvArgs);
    GpuDevice *pDevice = pThis->GetGpuDevice();
    UINT08 *pSem = (UINT08*)pThis->m_pWaitIdleSem->GetAddress();

    if (pThis->CheckError() != OK)
    {
        return true;
    }

    for (UINT32 subdev = 0; subdev < pDevice->GetNumSubdevices(); ++subdev)
    {
        if (MEM_RD32(pSem + subdev * WAIT_IDLE_SEM_SIZE) !=
            pThis->m_WaitIdleValue)
        {
            return false;
        }
    }

    return true;
}
