/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Channel interface.

#include "lwtypes.h"
#include "core/include/channel.h"
#include "gpu/include/dispchan.h"
#include "core/include/massert.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "core/include/tasker.h"
#include "core/include/utility.h"
#include "fermi/gf100/dev_disp.h"
#include "lwos.h"
#include "gpu/include/gpudev.h"
#include "core/include/version.h"

#include "class/cl507a.h" // LW50_LWRSOR_CHANNEL_PIO
#include "class/cl507c.h" // LW50_BASE_CHANNEL_DMA
#include "class/cl507d.h" // LW50_CORE_CHANNEL_DMA
#include "class/cl507e.h" // LW50_OVERLAY_CHANNEL_DMA

//------------------------------------------------------------------------------
// Common error notifier checking routine, used for both PIO and DMA channels.
//
static inline UINT32 CheckErrNotifier(LwNotification * pChErrNotifier)
{
    if (0xffff != MEM_RD16(&pChErrNotifier->status))
        return 0;
    return MEM_RD32(&pChErrNotifier->info32);
}

DisplayChannel::DisplayChannel
(
    void *       ControlRegs[LW2080_MAX_SUBDEVICES],
    GpuDevice *  pGpuDev
)
: m_pErrorNotifierMemory(nullptr)
, m_hChannel(0)
, m_PushbufMemSpace(Memory::Coherent)
, m_TimeoutMs(1000.0f)
, m_pGpuDevice(pGpuDev)
, m_EnableLogging(false)
{
    m_NumSubdevices = pGpuDev->GetNumSubdevices();

    for (UINT32 SubdeviceIdx = 0; SubdeviceIdx < LW2080_MAX_SUBDEVICES; SubdeviceIdx++)
    {
        if (ControlRegs)
        {
            m_ControlRegs[SubdeviceIdx] = ControlRegs[SubdeviceIdx];
            MASSERT((NULL != ControlRegs[SubdeviceIdx]) ||
                    (SubdeviceIdx >= m_NumSubdevices));
        }
        else
        {
            m_ControlRegs[SubdeviceIdx] = nullptr;
        }
    }
}

DisplayChannel::DispChannelType DisplayChannel::ChannelNameToType(string name)
{
    DispChannelType type = DispChannelType::NUM_TYPES;
    
    if (name == "core")
        type = DispChannelType::CORE;
    else if (name == "base")
        type = DispChannelType::BASE;
    else if (name == "overlay")
        type = DispChannelType::OVERLAY;
    else if (name == "window")
        type = DispChannelType::WINDOW;
    else if (name == "windowImm")
        type = DispChannelType::WINDOW_IMM;
    else
        MASSERT(!"Channel unknown");

    return type;
}

RC DisplayChannel::WaitIdle
(
    FLOAT64 TimeoutMs
)
{
    LwRmPtr pLwRm;
    RC rc;

    // First do a WaitForDmaPush so that we spend less time inside the RM and
    // therefore yield more
    CHECK_RC(WaitForDmaPush(TimeoutMs));

    rc = pLwRm->IdleChannels(
        m_hChannel,
        1,          // numChannels
        nullptr, // phClients
        nullptr, // phDevices
        nullptr, // phChannels
        DRF_DEF(OS30, _FLAGS, _BEHAVIOR, _SLEEP) |
        DRF_DEF(OS30, _FLAGS, _IDLE, _CACHE1) |
        DRF_DEF(OS30, _FLAGS, _IDLE, _ALL_ENGINES) |
        DRF_DEF(OS30, _FLAGS, _CHANNEL, _SINGLE),
        static_cast<UINT32>(TimeoutMs * 1000),
        m_pGpuDevice->GetDeviceInst()); // Colwert to us for RM

    if (OK == rc)
    {
        rc = CheckError();
    }
    else
    {
        Printf(Tee::PriHigh, "%#010x channel could not be idled.\n", m_hChannel);
        JavaScriptPtr pJs;
        pJs->CallMethod(pJs->GetGlobalObject(), "ChannelWaitIdleErrorCallback");
        Tee::FlushToDisk();
        MASSERT(false);
    }

    return rc;
}

void DisplayChannel::SetDefaultTimeoutMs(FLOAT64 TimeoutMs)
{
    m_TimeoutMs = TimeoutMs;
}

FLOAT64 DisplayChannel::GetDefaultTimeoutMs() const
{
    return m_TimeoutMs;
}

RC DisplayChannel::CheckError() const
{
    UINT32 tmp = CheckErrNotifier(static_cast<LwNotification*>(m_pErrorNotifierMemory));

    return Channel::RobustChannelsCodeToModsRC(tmp);
}

void DisplayChannel::SetPushbufMemSpace(Memory::Location PushbufMemSpace)
{
    m_PushbufMemSpace = PushbufMemSpace;
}

void DisplayChannel::EnableLogging(bool b)
{
    m_EnableLogging = b;
}

void DisplayChannel::LogData(UINT32 n, UINT32 * pData) const
{
    const UINT32 nCols = 4;
    UINT32 i;
    UINT32 devInst = m_pGpuDevice->GetDeviceInst();
    for (i = 0; i < n; i++)
    {
        if (0 == i % nCols)
        {
            if (i)
                Printf(Tee::PriNormal, "\n");
            Printf(Tee::PriNormal, "DEV_%x_h_%x", devInst, m_hChannel);
        }
        Printf(Tee::PriNormal, " %08x", MEM_RD32(pData + i));
    }
    Printf(Tee::PriNormal, "\n");
}

//------------------------------------------------------------------------------
// PioDisplayChannel
//
struct PioDisplayChannel_PollArgs
{
    UINT32           NumSubdevices;
    volatile LwV32 * pFreeCount[LW2080_MAX_SUBDEVICES];
    UINT32 *         pCachedFreeCount[LW2080_MAX_SUBDEVICES];
    UINT32           Words;
    LwNotification * pChErrNotifier;
};

static bool PioDisplayChannel_PollFreeCount
(
   void * pArgs
)
{
    PioDisplayChannel_PollArgs Args = *static_cast<PioDisplayChannel_PollArgs *>(pArgs);

    for (UINT32 SubdeviceIdx = 0; SubdeviceIdx < Args.NumSubdevices; SubdeviceIdx++)
    {
        *Args.pCachedFreeCount[SubdeviceIdx] =
            MEM_RD32(Args.pFreeCount[SubdeviceIdx]) >> 2;

        if (CheckErrNotifier(Args.pChErrNotifier))
            return true;

        if (*Args.pCachedFreeCount[SubdeviceIdx] < Args.Words)
        {
            return false;
        }
    }

    return true;
}

/* virtual */ RC PioDisplayChannel::WaitFreeCount
(
    UINT32 Words,
    FLOAT64 TimeoutMs
)
{
    PioDisplayChannel_PollArgs Args;

    Args.NumSubdevices = m_NumSubdevices;
    Args.Words = Words;
    Args.pChErrNotifier = static_cast<LwNotification*>(m_pErrorNotifierMemory);

    for (UINT32 SubdeviceIdx = 0; SubdeviceIdx < Args.NumSubdevices; SubdeviceIdx++)
    {
        Lw50DispLwrsorControlPio *pCh =
            static_cast<Lw50DispLwrsorControlPio *>(m_ControlRegs[SubdeviceIdx]);
        Args.pFreeCount[SubdeviceIdx] = &pCh->Free;
        Args.pCachedFreeCount[SubdeviceIdx] = &m_CachedFreeCount[SubdeviceIdx];
    }

    RC rc = POLLWRAP_HW(PioDisplayChannel_PollFreeCount, &Args, TimeoutMs);
    if (OK == rc)
        rc = CheckError();

    return rc;
}

PioDisplayChannel::PioDisplayChannel
(
    void *       ControlRegs[LW2080_MAX_SUBDEVICES],
    void *       pErrorNotifierMemory,
    LwRm::Handle hChannel,
    GpuDevice *  pGpuDev
) :
    DisplayChannel(ControlRegs, pGpuDev)
{
    MASSERT(ControlRegs != NULL);
    MASSERT(pErrorNotifierMemory != NULL);

    m_pGpuDevice = pGpuDev;
    m_pErrorNotifierMemory = pErrorNotifierMemory;
    m_hChannel = hChannel;

    // Start the cached free count at zero, which is safe; the first write to the
    // channel will get us an accurate cached free count.
    for (UINT32 SubdeviceIdx = 0;
         SubdeviceIdx < sizeof(m_CachedFreeCount)/sizeof(m_CachedFreeCount[0]);
         SubdeviceIdx++)
    {
        m_CachedFreeCount[SubdeviceIdx] = 0;
    }

    m_SkipFreeCount = false;

    MEM_WR16(&(static_cast<LwNotification*>(m_pErrorNotifierMemory))->status, 0);
}

PioDisplayChannel::~PioDisplayChannel()
{
}

/* virtual */ RC PioDisplayChannel::SetAutoFlush
(
    bool AutoFlushEnable,
    UINT32 AutoFlushThreshold
)
{
    // Writes to a PIO channel don't need to be flushed and will take effect
    // whether you want them to or not.  So, setting AutoFlushEnable to false
    // is illegal, and AutoFlushThreshold is ignored.
    return AutoFlushEnable ? OK : RC::UNSUPPORTED_FUNCTION;
}

RC PioDisplayChannel::GetAutoFlush
(
    bool *pAutoFlushEnable,
    UINT32 *pAutoFlushThreshold
) const
{
    *pAutoFlushEnable = true;
    *pAutoFlushThreshold = 0;

    return OK;
}

/* virtual */ RC PioDisplayChannel::Write
(
    UINT32 Method,
    UINT32 Data
)
{
    RC rc;

    if (!m_SkipFreeCount)
    {
        // Must have space to write the method
        for (UINT32 SubdeviceIdx = 0; SubdeviceIdx < m_NumSubdevices; SubdeviceIdx++)
        {
            if (m_CachedFreeCount[SubdeviceIdx] < 1)
            {
                CHECK_RC(WaitFreeCount(1, m_TimeoutMs));
                break;
            }
        }
    }

    // Always flush WC before writing PIO methods
    CHECK_RC(Platform::FlushCpuWriteCombineBuffer());

    for (UINT32 SubdeviceIdx = 0; SubdeviceIdx < m_NumSubdevices; SubdeviceIdx++)
    {
        // Write the method to the channel.
        volatile UINT32 * Offset = reinterpret_cast<UINT32 *>(
            static_cast<UINT08 *>(m_ControlRegs[SubdeviceIdx]) + Method);
        MEM_WR32(Offset, Data);
        // Keep cached free count up to date
        m_CachedFreeCount[SubdeviceIdx]--;
    }

    return OK;
}

/* virtual */ RC PioDisplayChannel::Write
(
    UINT32         Method,
    UINT32         Count,
    const UINT32 * DataPtr
)
{
    RC rc;

    MASSERT(Count != 0);
    MASSERT(DataPtr != nullptr);

    // This could definitely be optimized, but we can't just wait for Count
    // words to become available because a method can have more data words than
    // there are entries in the FIFO.
    for (UINT32 i = 0; i < Count; i++)
    {
        CHECK_RC(Write(Method + 4*i, DataPtr[i]));
    }

    return OK;
}

/* virtual */ RC PioDisplayChannel::WriteNonInc
(
    UINT32         Method,
    UINT32         Count,
    const UINT32 * DataPtr
)
{
    RC rc;

    MASSERT(Count != 0);
    MASSERT(DataPtr != nullptr);

    // This could definitely be optimized, but we can't just wait for Count
    // words to become available because a method can have more data words than
    // there are entries in the FIFO.
    for (UINT32 i = 0; i < Count; i++)
    {
        CHECK_RC(Write(Method, DataPtr[i]));
    }

    return OK;
}

/* virtual */ RC PioDisplayChannel::WriteNop()
{
    // PIO channels don't have a concept of a channel nop.
    return OK;
}

/* virtual */ RC PioDisplayChannel::WriteJump(UINT32 Offset)
{
    // This isn't legal in a PIO channel
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC PioDisplayChannel::WriteSetSubdevice(UINT32 Subdevice)
{
    // This isn't legal in a PIO channel
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC PioDisplayChannel::WriteOpcode(UINT32          Opcode,
                                                UINT32          Method,
                                                UINT32          Count,
                                                const UINT32 *  DataPtr)
{
    // This isn't legal in a PIO channel
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC PioDisplayChannel::Flush()
{
    // PIO channels don't need to be flushed.
    return OK;
}

/* virtual */ RC PioDisplayChannel::WaitForDmaPush
(
    FLOAT64 TimeoutMs
)
{
    // PIO channels don't have a DMA buffer
    return OK;
}

/* virtual */ RC PioDisplayChannel::SetSkipFreeCount(bool skip)
{
    m_SkipFreeCount = skip;

    return OK;
}

//------------------------------------------------------------------------------
// DmaDisplayChannel
//

bool DmaDisplayChannel::PollGetEqPut
(
    void * pVoidPollArgs
)
{
    PollArgs *pPollArgs = static_cast<PollArgs *>(pVoidPollArgs);
    DmaDisplayChannel *pThis = pPollArgs->pThis;

    if (CheckErrNotifier(static_cast<LwNotification*>(pPollArgs->pErrNotifier)))
        return true;

    pThis->UpdateCachedGets();
    for (UINT32 SubdeviceIdx = 0; SubdeviceIdx < pPollArgs->NumSubdevices; SubdeviceIdx++)
    {
        if (pPollArgs->pCachedGets[SubdeviceIdx] != pPollArgs->Put)
            return false;
    }

    return true;
}

bool DmaDisplayChannel::PollGetNotEqPut
(
    void * pVoidPollArgs
)
{
    PollArgs *pPollArgs = static_cast<PollArgs *>(pVoidPollArgs);
    DmaDisplayChannel *pThis = pPollArgs->pThis;

    if (CheckErrNotifier(static_cast<LwNotification*>(pPollArgs->pErrNotifier)))
        return true;

    pThis->UpdateCachedGets();
    for (UINT32 SubdeviceIdx = 0; SubdeviceIdx < pPollArgs->NumSubdevices; SubdeviceIdx++)
    {
        if (pPollArgs->pCachedGets[SubdeviceIdx] == pPollArgs->Put)
            return false;
    }

    return true;
}

bool DmaDisplayChannel::PollWrap
(
    void * pVoidPollArgs
)
{
    PollArgs *pPollArgs = static_cast<PollArgs *>(pVoidPollArgs);
    DmaDisplayChannel *pThis = pPollArgs->pThis;

    if (CheckErrNotifier(static_cast<LwNotification*>(pPollArgs->pErrNotifier)))
        return true;

    pThis->UpdateCachedGets();
    for (UINT32 SubdeviceIdx = 0; SubdeviceIdx < pPollArgs->NumSubdevices; SubdeviceIdx++)
    {
        if ((pPollArgs->pCachedGets[SubdeviceIdx] > pPollArgs->Put) ||
            (pPollArgs->pCachedGets[SubdeviceIdx] <= pPollArgs->Count))
            return false;
    }

    return true;
}

bool DmaDisplayChannel::PollNoWrap
(
    void * pVoidPollArgs
)
{
    PollArgs *pPollArgs = static_cast<PollArgs *>(pVoidPollArgs);
    DmaDisplayChannel *pThis = pPollArgs->pThis;

    if (CheckErrNotifier(static_cast<LwNotification*>(pPollArgs->pErrNotifier)))
        return true;

    pThis->UpdateCachedGets();
    for (UINT32 SubdeviceIdx = 0; SubdeviceIdx < pPollArgs->NumSubdevices; SubdeviceIdx++)
    {
        if ((pPollArgs->pCachedGets[SubdeviceIdx] > pPollArgs->Put) &&
            (pPollArgs->pCachedGets[SubdeviceIdx] <= (pPollArgs->Put + pPollArgs->Count)))
            return false;
    }

    return true;
}

void DmaDisplayChannel::UpdateCachedGets()
{
    for (UINT32 SubdeviceIdx = 0; SubdeviceIdx < m_PollArgs.NumSubdevices; SubdeviceIdx++)
    {
        m_CachedGet[SubdeviceIdx] = MEM_RD32(m_PollArgs.pGet[SubdeviceIdx]);
    }
}

DmaDisplayChannel::DmaDisplayChannel
(
    void *       ControlRegs[LW2080_MAX_SUBDEVICES],
    void *       pPushBufferBase,
    UINT32       PushBufferSize,
    void *       pErrorNotifierMemory,
    LwRm::Handle hChannel,
    UINT32       Offset,
    GpuDevice *  pGpuDev
) :
    DisplayChannel(ControlRegs, pGpuDev),
    m_LastFlushAt(0),
    m_HighWaterMark(0)
{
    MASSERT(ControlRegs != NULL);
    MASSERT(pErrorNotifierMemory != NULL);

    m_pGpuDevice = pGpuDev;
    m_pErrorNotifierMemory = pErrorNotifierMemory;
    m_hChannel = hChannel;

    // Default is to flush automatically
    m_AutoFlushEnable = true;
    m_AutoFlushThreshold = 256;

    m_pPushBufferBase = static_cast<UINT08 *>(pPushBufferBase);
    m_PushBufferSize = PushBufferSize;

    // Bug 232440 WAR.
    // Force MakeRoom to wrap 12*4 bytes early
    // From Tyvis Cheung:
    // "This bug got instanced to gt200 and got wnf-fina.
    // In the gf100 version of this bug, it was wnf-fix-never."
    MASSERT(m_PushBufferSize > 12*4);
    m_PushBufferSize -= 12*4;

    // Start writing at the base of the pushbuffer plus the specified offset
    m_pLwrrent = reinterpret_cast<UINT32 *>(m_pPushBufferBase + Offset);
    m_CachedPut = Offset;

    m_pNextAutoFlush = reinterpret_cast<UINT32 *>(
        reinterpret_cast<UINT08 *>(m_pLwrrent) + m_AutoFlushThreshold);

    m_PollArgs.pThis = this;
    memset(&m_PollArgs.pGet[0], 0, sizeof(m_PollArgs.pGet));
    m_PollArgs.pCachedGets = m_CachedGet;
    m_PollArgs.pErrNotifier = m_pErrorNotifierMemory;
    m_PollArgs.NumSubdevices = m_NumSubdevices;
    m_PollArgs.Put = 0;
    m_PollArgs.Count = 0;

    for (UINT32 SubdeviceIdx = 0; SubdeviceIdx < m_PollArgs.NumSubdevices; SubdeviceIdx++)
    {
        //
        // Even for LwDisplay PUT starts from 0th offset followed by GET,
        // So its valid to typecast it to LW50DispBaseControlDma
        //
        m_PollArgs.pGet[SubdeviceIdx] = static_cast<volatile UINT32 *>(
            &static_cast<Lw50DispBaseControlDma *>(m_ControlRegs[SubdeviceIdx])->Get);
        m_CachedGet[SubdeviceIdx] = Offset;
    }

    MEM_WR16(&(static_cast<LwNotification*>(m_pErrorNotifierMemory))->status, 0);

    // HW bug 561630 requires the Put pointer to be 4DW aligned.
    if (m_pGpuDevice->GetSubdevice(0)->HasBug(561630))
        m_RqdPutAlignment = 4 * sizeof(UINT32);
    else
        m_RqdPutAlignment = sizeof(UINT32);
}

DmaDisplayChannel::~DmaDisplayChannel()
{
}

/* virtual */ RC DmaDisplayChannel::SetAutoFlush
(
    bool AutoFlushEnable,
    UINT32 AutoFlushThreshold
)
{
    m_AutoFlushEnable    = AutoFlushEnable;
    m_AutoFlushThreshold = AutoFlushThreshold;
    return OK;
}

RC DmaDisplayChannel::GetAutoFlush
(
    bool *pAutoFlushEnable,
    UINT32 *pAutoFlushThreshold
) const
{
    *pAutoFlushEnable = m_AutoFlushEnable;
    *pAutoFlushThreshold = m_AutoFlushThreshold;

    return OK;
}

/* virtual */ RC DmaDisplayChannel::Write
(
    UINT32 Method,
    UINT32 Data
)
{
    RC rc;

    CHECK_RC(MakeRoom(2));

    // Write the data to the push buffer.
    UINT32 *pLwrrent = m_pLwrrent;
    MEM_WR32
    (
        pLwrrent++,
        ComposeMethod(LW_UDISP_DMA_OPCODE_METHOD, 1, Method)
    );
    MEM_WR32(pLwrrent++, Data);
    m_pLwrrent = pLwrrent;

    if (m_AutoFlushEnable && (m_pLwrrent >= m_pNextAutoFlush))
    {
        CHECK_RC(Flush());
    }

    return OK;
}

/* virtual */ UINT32 DmaDisplayChannel::ComposeMethod
(
    UINT32 OpCode,
    UINT32 Count,
    UINT32 Offest
 )
{
    return DRF_NUM(_UDISP, _DMA, _OPCODE, OpCode) |
           DRF_NUM(_UDISP, _DMA, _METHOD_COUNT,  Count) |
           DRF_NUM(_UDISP, _DMA, _METHOD_OFFSET,  Offest >> 2);

}

/* virtual */ RC DmaDisplayChannel::Write
(
    UINT32         Method,
    UINT32         Count,
    const UINT32 * DataPtr
)
{
    RC rc;

    MASSERT(Count != 0);
    MASSERT(DataPtr != nullptr);

    if (Count > 2047)
    {
        return RC::METHOD_COUNT_TOO_LARGE;
    }

    CHECK_RC(MakeRoom(1 + Count));

    // Write the data to the push buffer.
    UINT32 *pLwrrent = m_pLwrrent;
    MEM_WR32
    (
        pLwrrent++,
        ComposeMethod(LW_UDISP_DMA_OPCODE_METHOD, Count, Method)
    );
    for (UINT32 i = 0; i < Count; i++)
    {
        MEM_WR32(&pLwrrent[i], DataPtr[i]);
    }
    m_pLwrrent = pLwrrent + Count;

    if (m_AutoFlushEnable && (m_pLwrrent >= m_pNextAutoFlush))
    {
        CHECK_RC(Flush());
    }

    return OK;
}

/* virtual */ RC DmaDisplayChannel::WriteNonInc
(
    UINT32         Method,
    UINT32         Count,
    const UINT32 * DataPtr
)
{
    RC rc;

    MASSERT(Count != 0);
    MASSERT(DataPtr != nullptr);

    if (Count > 2047)
    {
        return RC::METHOD_COUNT_TOO_LARGE;
    }

    CHECK_RC(MakeRoom(1 + Count));

    // Write the data to the push buffer.
    UINT32 *pLwrrent = m_pLwrrent;
    MEM_WR32
    (
        pLwrrent++,
        ComposeMethod(LW_UDISP_DMA_OPCODE_NONINC_METHOD, Count, Method)
    );
    for (UINT32 i = 0; i < Count; i++)
    {
        MEM_WR32(&pLwrrent[i], DataPtr[i]);
    }
    m_pLwrrent = pLwrrent + Count;

    if (m_AutoFlushEnable && (m_pLwrrent >= m_pNextAutoFlush))
    {
        CHECK_RC(Flush());
    }

    return OK;
}

/* virtual */ RC DmaDisplayChannel::WriteNop()
{
    RC rc;

    CHECK_RC(MakeRoom(1));
    MEM_WR32(m_pLwrrent++, 0);

    if (m_AutoFlushEnable && (m_pLwrrent >= m_pNextAutoFlush))
    {
        CHECK_RC(Flush());
    }

    return OK;
}

/* virtual */ RC DmaDisplayChannel::WriteJump(UINT32 Offset)
{
    MEM_WR32
    (
        m_pLwrrent++,
        DRF_DEF(_UDISP, _DMA, _OPCODE,      _JUMP) |
        DRF_NUM(_UDISP, _DMA, _JUMP_OFFSET, Offset / 4)
    );

    return OK;
}

/* virtual */ RC DmaDisplayChannel::WriteSetSubdevice(UINT32 Subdevice)
{
    RC rc;

    CHECK_RC(MakeRoom(1));

    MEM_WR32
    (
        m_pLwrrent++,
        DRF_DEF(_UDISP, _DMA, _OPCODE,                   _SET_SUBDEVICE_MASK) |
        DRF_NUM(_UDISP, _DMA, _SET_SUBDEVICE_MASK_VALUE, Subdevice)
    );

    if (m_AutoFlushEnable && (m_pLwrrent >= m_pNextAutoFlush))
    {
        CHECK_RC(Flush());
    }

    return OK;
}

/* virtual */ RC DmaDisplayChannel::WriteOpcode
(
    UINT32         Opcode,
    UINT32         Method,
    UINT32         Count,
    const UINT32 * DataPtr
)
{
    RC rc;

    MASSERT(Count != 0);
    MASSERT(DataPtr != nullptr);

    if (Count > 2047)
    {
        return RC::METHOD_COUNT_TOO_LARGE;
    }

    CHECK_RC(MakeRoom(1 + Count));

    // Write the data to the push buffer.
    UINT32 *pLwrrent = m_pLwrrent;
    MEM_WR32
    (
        pLwrrent++,
        ComposeMethod(Opcode, Count, Method)
    );
    for (UINT32 i = 0; i < Count; i++)
    {
        MEM_WR32(&pLwrrent[i], DataPtr[i]);
    }
    m_pLwrrent = pLwrrent + Count;

    if (m_AutoFlushEnable && (m_pLwrrent >= m_pNextAutoFlush))
    {
        CHECK_RC(Flush());
    }

    return OK;
}

/* virtual */ RC DmaDisplayChannel::WaitForDmaPush
(
    FLOAT64 TimeoutMs
)
{
    RC rc;

    // Wait for Get to equal Put
    m_PollArgs.Put = m_CachedPut;
    rc = POLLWRAP_HW(&PollGetEqPut, &m_PollArgs, TimeoutMs);
    if (OK == rc)
        rc = CheckError();

    if (m_EnableLogging &&
        (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK))
    {
        Printf(Tee::PriNormal,
                "DEV_%x_h_%x WaitForDmaPush returns %d\n",
                m_pGpuDevice->GetDeviceInst(),
                m_hChannel,
                UINT32(rc));
    }
    return rc;
}

UINT32 DmaDisplayChannel::GetRoomPercent()
{
    UpdateCachedGets();

    // Ignore the other subdevices and callwlate only for the main one:
    constexpr UINT32 subdeviceIdx = 0;

    UINT32 spaceTaken;

    if (m_CachedPut >= m_CachedGet[subdeviceIdx])
    {
        // The Put pointer has not wrapped yet and has higher value
        // than the Get pointer.
        spaceTaken = m_CachedPut - m_CachedGet[subdeviceIdx];
    }
    else
    {
        // The Put pointer has wrapped while the Get pointer has not.
        // The oclwpied space is between beginning of the buffer up to
        // the Put pointer plus from the Get pointer up to the buffer size.
        spaceTaken = m_CachedPut + m_PushBufferSize - m_CachedGet[subdeviceIdx];
    }

    if (spaceTaken >= m_PushBufferSize)
        return 0;

    return 100 * (m_PushBufferSize - spaceTaken) / m_PushBufferSize;
}

/* virtual */ RC DmaDisplayChannel::Flush()
{
    UINT32 NewPut = static_cast<UINT32>(
        reinterpret_cast<UINT08 *>(m_pLwrrent) - m_pPushBufferBase);

    // If we haven't written anything since the last flush, don't do anything
    if (m_CachedPut == NewPut)
    {
        return OK;
    }

    while (NewPut & (m_RqdPutAlignment-1)) // Not aligned
    {
        // Assume end of pushbuffer is m_RqdPutAlignment aligned
        MEM_WR32(m_pLwrrent++, 0); // NOP
        NewPut += sizeof(UINT32);
    }

    UINT08 * From = m_pPushBufferBase + m_LastFlushAt;
    UINT08 * To   = reinterpret_cast<UINT08 *>(m_pLwrrent);

    // Check if the put pointer wrapped.
    if (From > To)
    {
        UINT08 * HighWaterMark = m_pPushBufferBase + m_HighWaterMark;
        Platform::FlushCpuWriteCombineBuffer();
        if (m_EnableLogging &&
            (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK))
        {
            LogData(UINT32((HighWaterMark - From)/sizeof(UINT32)),
                reinterpret_cast<UINT32*>(From));
        }
        From = m_pPushBufferBase;
    }

    // We want to flush only to the last data written,
    // which is directly before the put pointer
    Platform::FlushCpuWriteCombineBuffer();

    if (m_EnableLogging &&
        (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK))
    {
        LogData(UINT32((To - From)/sizeof(UINT32)), reinterpret_cast<UINT32*>(From));
    }

    // Host does not provide synchronization between CPU writes through BAR1 to
    // the FB and display NISO reads of the pushbuffer from FB.  This is not an
    // issue for host channels, because FB provides ordering guarantees between
    // host's CPU writes and its pushbuffer fetches.  But for display, the
    // pushbuffer fetches could obtain stale data.  We can obtain the ordering
    // guarantees necessary by doing a read of any word in the FB; see bug 170904
    // for details.  System memory pushbuffers do not require this read, and it
    // could hurt performance, so we want to skip it there.
    // see also WAR for 388771
    if (m_PushbufMemSpace == Memory::Fb)
    {
        GpuDevice* pGpuDev = m_pGpuDevice;
        for (UINT32 i=0; i<pGpuDev->GetNumSubdevices(); i++)
        {
            GpuSubdevice *pSubdev = pGpuDev->GetSubdevice(i);
            pSubdev->FbFlush(m_TimeoutMs);
        }
    }

    // Flush the put pointer.
    for (UINT32 SubdeviceIdx = 0; SubdeviceIdx < m_NumSubdevices; SubdeviceIdx++)
    {
        if (m_EnableLogging &&
            (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK))
        {
            Printf(Tee::PriNormal,
                    "DEV_%x_h_%x Flush subdev %d: put was 0x%x now 0x%x\n",
                    m_pGpuDevice->GetDeviceInst(),
                    m_hChannel,
                    SubdeviceIdx,
                    m_CachedPut,
                    NewPut);
        }

        //
        // Even for LwDisplay PUT starts from 0th offset followed by GET,
        // So its valid to typecast it to LW50DispBaseControlDma
        //
        MEM_WR32(&(static_cast<Lw50DispBaseControlDma*>(m_ControlRegs[SubdeviceIdx]))->Put, NewPut);
    }
    m_CachedPut = NewPut;

    m_pNextAutoFlush = reinterpret_cast<UINT32 *>(
        reinterpret_cast<UINT08 *>(m_pLwrrent) + m_AutoFlushThreshold);
    m_LastFlushAt = m_CachedPut;

    return OK;
}

// private
RC DmaDisplayChannel::MakeRoom
(
    UINT32 Count
)
{
    RC rc;
    bool HaveSpace;

    // Add enough additional space for any Put pointer alignment needs
    while (Count % (m_RqdPutAlignment / sizeof(UINT32)))
        Count++;

    Count <<= 2;

    m_PollArgs.Put   = static_cast<UINT32>(
        reinterpret_cast<UINT08 *>(m_pLwrrent) - m_pPushBufferBase);
    m_PollArgs.Count = Count;

    // Check if there is enough room in the push buffer to write the data without
    // needing to wrap.  Leave room for the jump opcode on the next call.
    if (m_PollArgs.Put + Count + sizeof(UINT32) > m_PushBufferSize)
    {
        //
        // Not enough space -- we need to wrap.
        //

        if (m_AutoFlushEnable)
        {
            // Flush, to guarantee that space will become available eventually.
            CHECK_RC(Flush());

            // Due to WAR for bug 561630 Flush could have added some NOPs,
            //  increasing m_pLwrrent value, so recallwlate m_PollArgs.Put
            //  to avoid false overflow detection (bug 609437)
            m_PollArgs.Put   = static_cast<UINT32>(
                reinterpret_cast<UINT08 *>(m_pLwrrent) - m_pPushBufferBase);
        }

        // Check whether we will have space if we wait long enough
        if ((m_CachedPut > m_PollArgs.Put) ||
            (m_CachedPut <= m_PollArgs.Count))
        {
            Printf(Tee::PriError, "Pushbuffer for channel 0x%x is too small.\n",
                m_hChannel);
            return RC::PUSHBUFFER_TOO_SMALL;
        }

        // Wait for enough room at the bottom to free up.
        RC waitRC = POLLWRAP_HW(&PollWrap, &m_PollArgs, m_TimeoutMs);
        if (waitRC != OK)
        {
            Printf(Tee::PriError,
                "Error waiting for room to jump in pushbuffer for channel 0x%x.\n",
                 m_hChannel);
            return waitRC;
        }
        CHECK_RC(CheckError());

        // Write a jump back to offset zero.
        WriteJump(0);

        // Update the high water mark --- highest point the put pointer reached.
        m_HighWaterMark = static_cast<UINT32>(
            reinterpret_cast<UINT08 *>(m_pLwrrent) - m_pPushBufferBase);

        // Move put pointer back to the beginning of the push buffer.
        m_pLwrrent = reinterpret_cast<UINT32 *>(m_pPushBufferBase);

        // Put next automatic flush point back at start of pushbuffer
        m_pNextAutoFlush = reinterpret_cast<UINT32 *>(
            reinterpret_cast<UINT08 *>(m_pLwrrent) + m_AutoFlushThreshold);
    }
    else
    {
        //
        // Enough space -- we don't need to wrap.
        //

        // Do we know that we have enough space based on the last known position
        // of Get?
        HaveSpace = true;
        for (UINT32 SubdeviceIdx = 0; SubdeviceIdx < m_PollArgs.NumSubdevices; SubdeviceIdx++)
        {
            if ((m_CachedGet[SubdeviceIdx] > m_PollArgs.Put) &&
                (m_CachedGet[SubdeviceIdx] <= (m_PollArgs.Put + Count)))
            {
                HaveSpace = false;
                break;
            }
        }

        // If we know that we have enough space, return without reading Get.
        if (HaveSpace)
        {
            return OK;
        }

        if (m_AutoFlushEnable)
        {
            // Flush, to guarantee that space will become available eventually.
            CHECK_RC(Flush());

            // Due to WAR for bug 561630 Flush could have added some NOPs,
            //  increasing m_pLwrrent value, so recallwlate m_PollArgs.Put
            //  to avoid false overflow detection (bug 609437)
            m_PollArgs.Put   = static_cast<UINT32>(
                reinterpret_cast<UINT08 *>(m_pLwrrent) - m_pPushBufferBase);
        }

        // Check whether we will have space if we wait long enough
        if ((m_CachedPut > m_PollArgs.Put) &&
            (m_CachedPut <= m_PollArgs.Put + m_PollArgs.Count))
        {
            Printf(Tee::PriError, "Pushbuffer for channel 0x%x is too small.\n",
                m_hChannel);
            return RC::PUSHBUFFER_TOO_SMALL;
        }

        // Wait for enough room for put to not overrun get.
        RC waitRC = POLLWRAP_HW(&PollNoWrap, &m_PollArgs, m_TimeoutMs);
        if (waitRC != OK)
        {
            Printf(Tee::PriError,
                "Error waiting for room in pushbuffer for channel 0x%x.\n",
                 m_hChannel);
            return waitRC;
        }
        CHECK_RC(CheckError());
    }

    return OK;
}

UINT32 DmaDisplayChannel::GetCachedPut() const
{
    return m_CachedPut;
}

/*virtual*/ UINT32 DmaDisplayChannel::ScanlineRead(UINT32 head)
{
    return DRF_VAL(507C,_GET_SCANLINE,_LINE,
        MEM_RD32(&(static_cast<Lw50DispBaseControlDma *>(m_ControlRegs[0]))->GetScanline));
}

/* virtual */void DmaDisplayChannel::SetPut(DispChannelType ch, UINT32 newPut)
{
    for (UINT32 SubdeviceIdx = 0; SubdeviceIdx < m_NumSubdevices; SubdeviceIdx++)
    {
        switch (ch)
        {
            case DispChannelType::CORE:
                MEM_WR32(&(static_cast<Lw50DispCoreControlDma *>(
                    m_ControlRegs[SubdeviceIdx]))->Put, newPut);
                break;
            case DispChannelType::BASE:
                MEM_WR32(&(static_cast<Lw50DispBaseControlDma *>(
                    m_ControlRegs[SubdeviceIdx]))->Put, newPut);
                break;
            case DispChannelType::OVERLAY:
                MEM_WR32(&(static_cast<Lw50DispOverlayControlDma *>(
                    m_ControlRegs[SubdeviceIdx]))->Put, newPut);
                break;
            default:
                MASSERT(!"Channel unknown");
                break;
        }
    }
}

/* virtual */UINT32 DmaDisplayChannel::GetPut(DispChannelType ch)
{
    UINT32 put;
    switch (ch)
    {
        case DispChannelType::CORE:
            put = MEM_RD32(&(static_cast<Lw50DispCoreControlDma *>(m_ControlRegs[0]))->Put);
            break;
        case DispChannelType::BASE:
            put = MEM_RD32(&(static_cast<Lw50DispBaseControlDma *>(m_ControlRegs[0]))->Put);
            break;
        case DispChannelType::OVERLAY:
            put = MEM_RD32(&(static_cast<Lw50DispOverlayControlDma *>(m_ControlRegs[0]))->Put);
            break;
        default:
            MASSERT(!"ChannelName unknown");
            put = 0;
            break;
    }
    return put;
}

/* virtual */void DmaDisplayChannel::SetGet(DispChannelType ch, UINT32 newGet)
{
    for (UINT32 SubdeviceIdx = 0; SubdeviceIdx < m_NumSubdevices; SubdeviceIdx++)
    {
        switch (ch)
        {
            case DispChannelType::CORE:
                MEM_WR32(&(static_cast<Lw50DispCoreControlDma *>(
                    m_ControlRegs[SubdeviceIdx]))->Get, newGet);
                break;
            case DispChannelType::BASE:
                MEM_WR32(&(static_cast<Lw50DispBaseControlDma *>(
                    m_ControlRegs[SubdeviceIdx]))->Get, newGet);
                break;
            case DispChannelType::OVERLAY:
                MEM_WR32(&(static_cast<Lw50DispOverlayControlDma *>(
                    m_ControlRegs[SubdeviceIdx]))->Get, newGet);
                break;
            default:
                MASSERT(!"Channel unknown");
                break;
        }
    }
}

/* virtual */UINT32 DmaDisplayChannel::GetGet(DispChannelType ch)
{
    UINT32 get;
    switch (ch)
    {
        case DispChannelType::CORE:
            get = MEM_RD32(&(static_cast<Lw50DispCoreControlDma *>(m_ControlRegs[0]))->Get);
            break;
        case DispChannelType::BASE:
            get = MEM_RD32(&(static_cast<Lw50DispBaseControlDma *>(m_ControlRegs[0]))->Get);
            break;
        case DispChannelType::OVERLAY:
            get = MEM_RD32(&(static_cast<Lw50DispOverlayControlDma *>(m_ControlRegs[0]))->Get);
            break;
        default:
            MASSERT(!"Channel unknown");
            get = 0;
            break;
    }
    return get;
}

/* virtual */void DmaDisplayChannel::SetPut(string ChannelName, UINT32 newPut)
{
    SetPut(ChannelNameToType(ChannelName), newPut);
}

/* virtual */UINT32 DmaDisplayChannel::GetPut(string ChannelName)
{
    return GetPut(ChannelNameToType(ChannelName));
}

/* virtual */void DmaDisplayChannel::SetGet(string ChannelName, UINT32 newGet)
{
    SetGet(ChannelNameToType(ChannelName), newGet);
}

/* virtual */UINT32 DmaDisplayChannel::GetGet(string ChannelName)
{
    return GetGet(ChannelNameToType(ChannelName));
}
