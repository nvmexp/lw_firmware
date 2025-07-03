/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/massert.h"
#include "core/include/lwrm.h"
#include "core/include/platform.h"
#include "core/include/channel.h"
#include "core/include/utility.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpudev.h"
#include "mdiag/tests/test.h"
#include "lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "lwos.h"
#include "Lwcm.h"
#include "gpu/include/gpudev.h"
#include "mdiag/utils/buffinfo.h"
#include "mdiag/utils/utils.h"
#include "mdiag/advschd/policymn.h"
#include "gpu/utility/atomwrap.h"
#include "gpu/utility/chanwmgr.h"
#include "mdiag/tests/gpu/lwgpu_tsg.h"
#include "mdiag/utils/lwgpu_classes.h"
#include "fermi/gf100/dev_ram.h"
#include "kepler/gk107/dev_ctxsw_prog.h"
#include "ctrl/ctrl0080.h"
#include "ctrl/ctrl9067.h" // LW9067_CTRL_CMD_SET_TPC_PARTITION_TABLE
#include "ctrl/ctrl0080/ctrl0080gr.h"

#include "class/cl906f.h" // GF100_CHANNEL_GPFIFO
#include "class/cl902d.h" // FERMI_TWOD_A
#include "class/cl90b8.h" // GF106_DMA_DECOMPRESS
#include "class/cla06f.h" // KEPLER_CHANNEL_GPFIFO_A
#include "class/cla16f.h" // KEPLER_CHANNEL_GPFIFO_B
#include "class/cla26f.h" // KEPLER_CHANNEL_GPFIFO_C
#include "class/clb06f.h" // MAXWELL_CHANNEL_GPFIFO_A
#include "class/clc06f.h" // PASCAL_CHANNEL_GPFIFO_A
#include "class/clc36f.h" // VOLTA_CHANNEL_GPFIFO_A
#include "class/clc46f.h" // TURING_CHANNEL_GPFIFO_A
#include "class/clc56f.h" // AMPERE_CHANNEL_GPFIFO_A
#include "class/clc86f.h" // HOPPER_CHANNEL_GPFIFO_A
#include "class/cla140.h" // KEPLER_INLINE_TO_MEMORY_B
#include "class/cla06fsubch.h" // Fixed subchannel number
#include "class/cl85b5sw.h" // LW85B5_ALLOCATION_PARAMETERS
#include "class/cla0b5sw.h" // LWA0B5_ALLOCATION_PARAMETERS

#include "mdiag/tests/gpu/trace_3d/pe_configurefile.h"
#include "mdiag/tests/gpu/trace_3d/tracetsg.h"
#include "mdiag/smc/smcengine.h"
#include "mdiag/smc/gpupartition.h"
#include "mdiag/utl/utl.h"

LWGpuChannel::LWGpuChannel(LWGpuResource *lwgpu, LwRm* pLwRm, SmcEngine *pSmcEngine)
{
    m_pLwRm = pLwRm;
    m_pSmcEngine = pSmcEngine;

    m_pCh = NULL;
    m_hCh = 0;

    m_UpdateType = AUTOMATIC;
    m_KickoffThreshold = 256;

    m_AutoGpEntryEnable = false;
    m_AutoGpEntryThreshold = 64;

    m_RandomAutoFlushThreshold.Max = 0;
    m_RandomAutoFlushThresholdGpFifoEntries.Max = 0;
    m_RandomAutoGpEntryThreshold.Max = 0;
    m_RandomPauseBeforeGPPutWrite.Max = 0;
    m_RandomPauseAfterGPPutWrite.Max = 0;

    m_UseBar1Doorbell = false;

    this->lwgpu = lwgpu;

    methodCount = 0;
    methodWriteCount = 0;

    initCtxSw();

    m_IdleMethodCount = 0;
    m_IdleMethodLwrrent = 0;

    m_InstMemLocOverride = false;
    m_RAMFCMemLocOverride = false;
    m_HashTableSizeOverride = false;
    m_TimesliceOverride = false;
    m_TimescaleOverride = false;
    m_EngTimesliceOverride = false;
    m_PbTimesliceOverride = false;
    m_InstMemLoc = _DMA_TARGET_VIDEO; // don't care
    m_RAMFCMemLoc = _DMA_TARGET_VIDEO; // don't care
    m_HashTableSize = 0; // don't care
    m_Timeslice = 0; // don't care
    m_Timescale = 0; // don't care
    m_EngTimeslice = 0;
    m_PbTimeslice = 0;
    m_Timeout = true;

    // Set default parameters for pushbuffer and GPFIFO
    m_Pushbuf.SetArrayPitch(1024*1024);
    m_Pushbuf.SetColorFormat(ColorUtils::Y8);
    m_Pushbuf.SetForceSizeAlloc(true);
    m_Pushbuf.SetLocation(Memory::NonCoherent);
    m_Pushbuf.SetProtect(Memory::Readable);
    m_GpFifo.SetLocation(Memory::NonCoherent);
    m_GpFifo.SetProtect(Memory::Readable);
    m_Err.SetLocation(Memory::Coherent);
    m_GpFifoEntries = 512;

    m_TimeoutMs = 10000;
    m_CtxSwEnabled = true;

    semAcquireTimeout = 0;
    m_NcohSnoop = false;
    m_Enabled = true;
    m_pNullChannel =  NULL;
    m_InsertNestingLevel = 0;

    m_SleepBetweenMethods = 0;
    m_UseLegacyWaitIdleWithRiskyIntermittentDeadlocking = false;

    m_HwCrcCheckMode = Channel::HWCRCCHKMODE_NONE;
    m_Preemptive = false;
    m_VprMode = false;
    m_IsGpuManaged = false;
    m_SkedRefBuf = 0;
    m_HostRefBuf = 0;

    m_pLWGpuTsg = 0;

    m_GpFifoEntriesFromTrace = false;
    m_PushbufferSizeFromTrace = false;

    m_HwChannelClass = GF100_CHANNEL_GPFIFO;
    m_SubchNumBase = 7;

    m_AllocChIdMode = ALLOC_ID_MODE_DEFAULT;

    m_Scheduled = false;
    m_MethodWritten = false;

    m_SCGType = GRAPHICS_COMPUTE0;
    m_CEPrefetchEnable = false;

    m_Pbdma = 0;
    m_hVASpace = 0;
    m_Mutex = Tasker::AllocMutex("LWGpuChannel::m_Mutex", Tasker::mtxUnchecked);
    m_WaitIdleMutex = Tasker::AllocMutex("LWGpuChannel::m_WaitIdleMutex", Tasker::mtxUnchecked);

    m_ChannelInitMethodCount = 0;

    m_SetCtxswPmModeDone = false;
    m_SetCtxswSmpcModeDone = false;

    m_PayloadForNonblockingPoll = 0;

    m_EngineId = LW2080_ENGINE_TYPE_NULL;
}

LWGpuChannel::~LWGpuChannel()
{
    CheckScheduleStatus();

    if (Utl::HasInstance())
    {
        Utl::Instance()->RemoveChannel(this);
    }

    if (lwgpu && m_pCh)
    {
        // recycle channel id
        lwgpu->AddChIdToPool(make_pair(GetLwRmPtr(), GetEngineId()), ChannelNum());
    }

    if (m_pCh)
        DestroyPushBuffer();
    delete m_pNullChannel;

    if (m_pLWGpuTsg)
    {
        m_pLWGpuTsg->RemoveLWGpuChannel(this);
        m_pLWGpuTsg = 0;
    }
}

UINT32 LWGpuChannel::ChannelHandle() const
{
    return m_hCh;
}

UINT32 LWGpuChannel::ChannelNum() const
{
    MASSERT(m_pCh);
    return m_pCh->GetChannelId();
}

UINT32 LWGpuChannel::GetPushBufferSize()
{
    return (UINT32)m_Pushbuf.GetAllocSize();
}

void LWGpuChannel::SetLWGpuTsg(LWGpuTsg* pLWGpuTsg)
{
    MASSERT(0 != pLWGpuTsg);
    m_pLWGpuTsg = pLWGpuTsg;
    m_pLWGpuTsg->AddLWGpuChannel(this);
}

int LWGpuChannel::MethodWrite(int subchannel, UINT32 method, UINT32 data)
{
    RC rc = MethodWriteRC(subchannel, method, data);
    if (rc != OK)
    {
        ErrPrintf("LWGpuChannel::MethodWrite error: %s\n",
            rc.Message());
        return 0;
    }
    return 1;
}

int LWGpuChannel::MethodWriteN(int subchannel,
                               UINT32 method_start,
                               unsigned count, const UINT32 *datap,
                               MethodType mode)
{
    RC rc = MethodWriteN_RC(subchannel, method_start, count, datap, mode);
    if (rc != OK)
    {
        ErrPrintf("LWGpuChannel::MethodWriteN error: %s\n",
            rc.Message());
        return 0;
    }
    return 1;
}

RC LWGpuChannel::MethodWriteRC(int subchannel, UINT32 method, UINT32 data)
{
    return MethodWriteN_RC(subchannel, method, 1, &data);
}

RC LWGpuChannel::MethodWriteN_RC(int subchannel,
                                 UINT32 method_start,
                                 unsigned count, const UINT32 *datap,
                                 MethodType mode)
{
    unsigned int lwrStart, lwrCount, lwrMethod;
    bool ctxSwitchAfterWrite;
    RC rc;

    // Work around strange mdiag method defn's
    method_start &= 0x3FFF;

    for (lwrStart = 0; lwrStart < count; lwrStart += lwrCount)
    {
        lwrMethod = GetMethodAddr(method_start, lwrStart, mode);
        lwrCount = GetWriteCount(subchannel, lwrMethod, count - lwrStart,
                                 &datap[lwrStart], mode, &ctxSwitchAfterWrite);
        if (lwrCount > 0)
        {
            switch (mode)
            {
                case NON_INCREMENTING:
                    CHECK_RC(m_pCh->WriteNonInc(
                        subchannel, lwrMethod,
                        lwrCount, &datap[lwrStart]));
                    break;
                case INCREMENTING:
                    CHECK_RC(m_pCh->Write(
                        subchannel, lwrMethod,
                        lwrCount, &datap[lwrStart]));
                    break;
                case INCREMENTING_ONCE:
                    CHECK_RC(m_pCh->WriteIncOnce(
                            subchannel, lwrMethod,
                            lwrCount, &datap[lwrStart]));
                    method_start += 4;
                    mode = NON_INCREMENTING;
                    break;
                case INCREMENTING_IMM:
                    CHECK_RC(m_pCh->WriteImmediate(
                                subchannel, lwrMethod,
                                *datap));
                    break;
                default:
                    MASSERT(0);
            }
        }

        if (ctxSwitchAfterWrite)
        {
            if (ctxswResetOnly)
            {
                CHECK_RC(MethodFlushRC());
                MASSERT(!"LWGpuChannel::ResetOnlyEvent not implemented");
            }
            else if (ctxswStopPull)
            {
                // XXX Not yet implemented
                MASSERT(0);
            }
            else
            {
                rc = WaitForIdleRC();
                if (rc != OK)
                {
                    WarnPrintf("Chip is not idle prior to CtxSwitch Yield: %s\n",
                        rc.Message());
                    return rc;
                }
                if (ctxswSelf)
                {
                    // Starting from Hopper, do not use backdoor ctxswself
                    // (Save/RestoreChannelState). It bypasses HOST, which 
                    // causes credits overflow. Use Preempt/Resubmit Runlist,
                    // so that HOST is aware of ctxsw. More detais here:
                    // https://confluence.lwpu.com/display/ArchMods/CtxSwSelf+changes-+Hopper+187
                    if (m_HwChannelClass >= HOPPER_CHANNEL_GPFIFO_A)
                    {
                        CHECK_RC(lwgpu->GetGpuDevice()->StopEngineRunlist(m_EngineId, m_pLwRm));
                        CHECK_RC(lwgpu->GetGpuDevice()->StartEngineRunlist(m_EngineId, m_pLwRm));
                    }
                    else
                    {
                        CHECK_RC(SaveChannelState());
                        CHECK_RC(RestoreChannelState());
                    }
                }
                if (!ctxswTimeSlice && !ctxswWFIOnly)
                {
                    CHECK_RC(lwgpu->GetContextScheduler()->YieldToNextCtxThread());
                }
            }
        }
    }

    if (m_CtxSwEnabled)
    {
        for (list<PushBufferListener*>::const_iterator
                 iter = m_push_buffer_listeners.begin();
             iter != m_push_buffer_listeners.end();
             ++iter)
        {
            (*iter)->notify_MethodWrite(this, subchannel, method_start, mode);
        }
    }

    // Support wait for idle every N groups of methods
    m_IdleMethodLwrrent++;
    if ((m_IdleMethodCount > 0) && (m_IdleMethodLwrrent >= m_IdleMethodCount))
    {
        bool idleAllowed = true;
        const AtomFsm* atomFsm = m_pCh->GetAtomChannelWrapper()->GetPeekableFsm();
        if (atomFsm && atomFsm->InAtom())
        {
            idleAllowed = false;
        }

        if (idleAllowed)
        {
            DebugPrintf(
                "wait for idle for channel %u as requested by -idle_method_count, "
                "current method count is %u\n", 
                m_pCh->GetChannelId(), 
                m_IdleMethodLwrrent);
            CHECK_RC(MethodFlushRC());
            CHECK_RC(WaitForIdleRC());
            m_IdleMethodLwrrent = 0;  // reset
        }
    }

    if (m_SleepBetweenMethods)
        Tasker::Sleep(m_SleepBetweenMethods);

    m_MethodWritten = true;

    return OK;
}

RC LWGpuChannel::WriteSetSubdevice(UINT32 data)
{
    RC rc = m_pCh->WriteSetSubdevice(data);
    if (rc != OK)
    {
        ErrPrintf("LWGpuChannel::WriteSetSubdevice error: %s\n",
            rc.Message());
    }
    return rc;
}

//------------------------------------------------------------------------------
Channel * LWGpuChannel::GetModsChannel()
{
    return m_pCh;
}

RC LWGpuChannel::ScheduleChannel(bool enable)
{
    m_Scheduled = true;
    return m_pCh->ScheduleChannel(enable);
}

int LWGpuChannel::SetObject(int subch, UINT32 handle)
{
    RC rc = SetObjectRC(subch, handle);
    if (rc != OK)
    {
        ErrPrintf("LWGpuChannel::SetObject error: %s\n",
               rc.Message());
        return 0;
    }
    return 1;
}

RC LWGpuChannel::SetObjectRC(int subch, UINT32 handle)
{
    MASSERT(m_pCh);
    m_Scheduled = true;
    return m_pCh->SetObject(subch, handle);
}

RC LWGpuChannel::SetSemaphoreOffsetRC(UINT64 Offset)
{
    MASSERT(m_pCh);
    return m_pCh->SetSemaphoreOffset(Offset);
}

//------------------------------------------------------------------------------
/* virtual */ void LWGpuChannel::SetSemaphoreReleaseFlags(UINT32 flags)
{
    MASSERT(m_pCh);
    m_pCh->SetSemaphoreReleaseFlags(flags);
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 LWGpuChannel::GetSemaphoreReleaseFlags()
{
    MASSERT(m_pCh);
    return m_pCh->GetSemaphoreReleaseFlags();
}

RC LWGpuChannel::SetSemaphorePayloadSize(Channel::SemaphorePayloadSize size)
{
    MASSERT(m_pCh);
    return m_pCh->SetSemaphorePayloadSize(size);
}

Channel::SemaphorePayloadSize LWGpuChannel::GetSemaphorePayloadSize()
{
    MASSERT(m_pCh);
    return m_pCh->GetSemaphorePayloadSize();
}

bool LWGpuChannel::Has64bitSemaphores()
{
    MASSERT(m_pCh);
    return m_pCh->Has64bitSemaphores();
}

RC LWGpuChannel::SemaphoreAcquireRC(UINT64 data)
{
    MASSERT(m_pCh);
    return m_pCh->SemaphoreAcquire(data);
}

RC LWGpuChannel::SemaphoreAcquireGERC(UINT64 data)
{
    MASSERT(m_pCh);
    return m_pCh->SemaphoreAcquireGE(data);
}

RC LWGpuChannel::SemaphoreReleaseRC(UINT64 data)
{
    MASSERT(m_pCh);
    return m_pCh->SemaphoreRelease(data);
}

RC LWGpuChannel::SemaphoreReductionRC
(
    Channel::Reduction reduction,
    Channel::ReductionType type,
    UINT64 data
)
{
    MASSERT(m_pCh);
    return m_pCh->SemaphoreReduction(reduction, type, data);
}

RC LWGpuChannel::NonStallInterruptRC()
{
    MASSERT(m_pCh);
    return MethodWriteRC(0, LW906F_NON_STALL_INTERRUPT, 0);
}

RC LWGpuChannel::BeginInsertedMethodsRC()
{
    MASSERT(m_pCh);
    RC rc;

    CHECK_RC(m_pCh->BeginInsertedMethods());
    ++m_InsertNestingLevel;
    return rc;
}

RC LWGpuChannel::EndInsertedMethodsRC()
{
    MASSERT(m_pCh);
    MASSERT(m_InsertNestingLevel > 0);
    --m_InsertNestingLevel;
    return m_pCh->EndInsertedMethods();
}

void LWGpuChannel::CancelInsertedMethods()
{
    MASSERT(m_pCh);
    MASSERT(m_InsertNestingLevel > 0);
    --m_InsertNestingLevel;
    m_pCh->CancelInsertedMethods();
}

int LWGpuChannel::MethodFlush()
{
    RC rc = MethodFlushRC();
    if (rc != OK)
    {
        ErrPrintf("LWGpuChannel::MethodFlush error: %s\n",
            rc.Message());
        return 0;
    }

    return 1;
}

RC LWGpuChannel::MethodFlushRC()
{
    return m_pCh->Flush();
}

RC LWGpuChannel::AllocSurfaceRC(Memory::Protect Protect, UINT64 Bytes,
                                   _DMA_TARGET Target, _PAGE_LAYOUT Layout,
                                   UINT32 *Handle, uintptr_t *Buf, UINT32 Attr,
                                   UINT64* GpuOffset, UINT32  subdev)
{
    RC rc;

    // First allocate the memory and context DMA
    CHECK_RC(lwgpu->AllocSurfaceRC(Protect, Bytes, Target,
            Layout, m_hVASpace, Handle, Buf, m_pLwRm, Attr, GpuOffset, subdev));

    // Then bind it into this channel
    return m_pLwRm->BindContextDma(m_hCh, *Handle);
}

RC LWGpuChannel::BindContextDma(UINT32 Handle)
{
    return m_pLwRm->BindContextDma(m_hCh, Handle);
}

UINT32 LWGpuChannel::CreateObject(UINT32 Class, void *Params, UINT32 *realClassName)
{
    LwRm::Handle Handle;
    RC rc;

    MASSERT(m_pCh);

    // Call the RM to create the object.
    rc = m_pLwRm->Alloc(m_hCh, &Handle, Class, Params);
    if (realClassName)
        *realClassName = Class;
    if (rc != OK)
    {
        ErrPrintf("LWGpuChannel::CreateObject error: %s\n",
               rc.Message());
        return 0;
    }

    // alloc subch number and build the map of handle and subchnum
    UINT32 subchNum = 0;
    rc = AllocSubChannelNum(Class, Handle, &subchNum);
    if (rc != OK)
    {
        ErrPrintf("LWGpuChannel::CreateObject error: "
            "no available subch number\n");
        return 0;
    }
    m_ObjHandle2SubchNumClassMap[Handle] = make_pair(subchNum, Class);
    InfoPrintf("Created channel object: Handle 0x%x, Class 0x%x, subchNum %d, channel #%d.\n",
        Handle, Class, subchNum, ChannelNum());

    return Handle;
}

void LWGpuChannel::DestroyObject(UINT32 handle)
{
    // Preemptive context buffer is binded to a compute objec, so whenever
    // compute object is being freed, we have to free corresponding context
    // buffers and SKED reflected buffers
    if (GetPreemptive())
    {
        m_PreemptCtxBuf.Free();
    }

    if (m_SkedRefBuf)
    {
        m_SkedRefBuf->Free();
    }

    if (m_HostRefBuf)
    {
        m_HostRefBuf->Free();
    }

    m_pLwRm->Free(handle);

    UINT32 subchannelNum;
    if (GetSubChannelNumClass(handle, &subchannelNum, 0) == OK)
    {
        m_AvailableSubchNum.push_back(subchannelNum);
    }

    m_ObjHandle2SubchNumClassMap.erase(handle);
}

int LWGpuChannel::WaitForDmaPush()
{
    RC rc = WaitForDmaPushRC();
    if (rc != OK)
    {
        ErrPrintf("LWGpuChannel::WaitForDmaPush error: %s\n",
            rc.Message());
        return 0;
    }
    return 1;
}

RC LWGpuChannel::WaitForDmaPushRC()
{
    RC rc;

    CheckScheduleStatus();

    // WaitForDmaPush implies a flush
    CHECK_RC(m_pCh->Flush());

    for (list<PushBufferListener*>::const_iterator
            iter = m_push_buffer_listeners.begin();
            iter != m_push_buffer_listeners.end();
            ++iter)
    {
        (*iter)->notify_Flush(this);
    }

    CHECK_RC(m_pCh->WaitForDmaPush(m_TimeoutMs));

    return OK;
}

int LWGpuChannel::WaitGrIdle()
{
    RC rc = WaitGrIdleRC();
    if (rc != OK)
    {
        ErrPrintf("LWGpuChannel::WaitGrIdle error: %s\n",
            rc.Message());

        if (rc == RC::TIMEOUT_ERROR)
        {
            ErrPrintf("MODS timed out after %f ms of waiting.  Set -timeout_ms to a higher value.\n", m_TimeoutMs);

            MASSERT(!"MODS timed out - triggering assertion to exit quickly");
        }

        return 0;
    }
    return 1;
}

RC LWGpuChannel::WaitGrIdleRC()
{
    RC rc;
    CheckScheduleStatus();

    // I believe it should be adequate to implement this as a WaitForIdle, minus
    // the flush.  This may not match mdiag behavior exactly in a few cases; the
    // most obvious one would be that this isn't waiting for other channels, just
    // this one.  This isn't called by all that many tests and in some case (e.g.
    // the image10/image30 tests) it even appears to be completely extraneous.
    rc = m_pCh->WaitIdle(m_TimeoutMs);

    InfoPrintf("%s channel id %u, engine id %u%s, WFI %s\n",
        __FUNCTION__, m_pCh->GetChannelId(), m_pCh->GetEngineId(),
        m_pSmcEngine ?
            Utility::StrPrintf(", swizid %u", m_pSmcEngine->GetGpuPartition()->GetSwizzId()).c_str() : "",
        rc == OK ? "succeed" : "failed");

    return rc;
}

int LWGpuChannel::WaitForIdle()
{
    RC rc = WaitForIdleRC();
    if (rc != OK)
    {
        ErrPrintf("LWGpuChannel::WaitForIdle error: %s\n",
            rc.Message());

        if (rc == RC::TIMEOUT_ERROR)
        {
            ErrPrintf("MODS timed out after %f ms of waiting.  Set -timeout_ms to a higher value.\n", m_TimeoutMs);

            MASSERT(!"MODS timed out - triggering assertion to exit quickly");
        }

        return 0;
    }
    return 1;
}

RC LWGpuChannel::WaitForIdleRC()
{
    RC rc;

    CheckScheduleStatus();

    // Safely ignore WaitForIdle if no channel yet.
    if (!m_pCh)
        return OK;

    if (m_HwCrcCheckMode & Channel::HWCRCCHKMODE_MTHD)
    {
        CHECK_RC(m_pCh->WriteCrcChkMethod());
    }

    if (m_HwCrcCheckMode & Channel::HWCRCCHKMODE_GP)
    {
        CHECK_RC(m_pCh->WriteGpCrcChkGpEntry());
    }

    // The Flush and WaitIdle calls below are not thread-safe.  For example,
    // the wait-for-idle semaphore allocation can yield to another thread
    // before the allocation is completed.  The Flush might yield to another
    // thread before the GP Fifo is fully updated.  If multiple threads
    // are using the same channel and happen to call WaitForIdleRC at the same
    // time, this can causes crashes or incorrect GP Fifo data.  This mutex
    // will guarantee that both operations finish for one thread before
    // the next thread starts its wait-for-idle.
    Tasker::MutexHolder mh(m_WaitIdleMutex);

    // WaitForIdle implies a flush for DMA channels
    CHECK_RC(m_pCh->Flush());

    rc = m_pCh->WaitIdle(m_TimeoutMs);
    if (!m_Enabled)
        rc.Clear();

    InfoPrintf("%s channel id %u, engine id %u%s, WFI %s\n",
        __FUNCTION__, m_pCh->GetChannelId(), m_pCh->GetEngineId(),
        m_pSmcEngine ?
            Utility::StrPrintf(", swizid %u", m_pSmcEngine->GetGpuPartition()->GetSwizzId()).c_str() : "",
        rc == OK ? "succeed" : "failed");

    return rc;
}

int LWGpuChannel::CheckForErrors()
{
    RC rc = CheckForErrorsRC();
    if (rc != OK)
    {
        ErrPrintf("LWGpuChannel::CheckForErrors error: %s\n",
               rc.Message());
        return 0;
    }
    return 1;
}

RC LWGpuChannel::CheckForErrorsRC()
{
    // Check for errors on the real channel, even if m_Enabled is
    // false, so that any error-cleanup code can run.  Note that the
    // cleanup can potentially change m_Enabled.
    RC rc = m_pLwRm->GetChannel(m_hCh)->CheckError();

    // Return the error (if any) if the channel is enabled, or OK if disabled.
    if (!m_Enabled)
        rc.Clear();
    return rc;
}

RC LWGpuChannel::SaveChannelState()
{
    RC rc;

#ifdef LW_VERIF_FEATURES
    LW2080_CTRL_GR_SET_CTXSW_TEST_STATE_PARAMS Params = {0};

    MASSERT(m_pCh);

    Params.hChannel = m_hCh;
    Params.command = LW2080_CTRL_GR_SET_CTXSW_TEST_STATE_COMMAND_SAVE;

    for (UINT32 subdev = 0;
         subdev < lwgpu->GetGpuDevice()->GetNumSubdevices(); subdev++)
    {
        rc = m_pLwRm->ControlBySubdevice(lwgpu->GetGpuSubdevice(subdev),
                        LW2080_CTRL_CMD_GR_SET_CTXSW_TEST_STATE,
                        (void*)&Params, sizeof(Params));
        if (rc != OK)
        {
            ErrPrintf("LWGpuChannel::SaveChannelState error: %s\n",
                   rc.Message());
            return rc;
        }
    }
#endif

    return rc;
}

RC LWGpuChannel::RestoreChannelState()
{
    RC rc;

#ifdef LW_VERIF_FEATURES
    LW2080_CTRL_GR_SET_CTXSW_TEST_STATE_PARAMS Params = {0};

    MASSERT(m_pCh);

    Params.hChannel = m_hCh;
    Params.command = LW2080_CTRL_GR_SET_CTXSW_TEST_STATE_COMMAND_RESTORE;

    for (UINT32 subdev = 0;
         subdev < lwgpu->GetGpuDevice()->GetNumSubdevices(); subdev++)
    {
        rc = m_pLwRm->ControlBySubdevice(lwgpu->GetGpuSubdevice(subdev),
                        LW2080_CTRL_CMD_GR_SET_CTXSW_TEST_STATE,
                        (void*)&Params, sizeof(Params));
        if (rc != OK)
        {
            ErrPrintf("LWGpuChannel::RestoreChannelState error: %s\n",
                   rc.Message());
            return rc;
        }
    }
#endif

    return rc;
}

RC LWGpuChannel::CtxSwResetMask()
{
    RC rc;

#ifdef LW_VERIF_FEATURES
    LW2080_CTRL_GR_SET_CTXSW_TEST_STATE_PARAMS Params = {0};

    MASSERT(m_pCh);

    Params.hChannel = m_hCh;
    Params.command = LW2080_CTRL_GR_SET_CTXSW_TEST_STATE_COMMAND_RESETMASK;
    Params.arg = ctxswResetMask[0];

    for (UINT32 subdev = 0;
         subdev < lwgpu->GetGpuDevice()->GetNumSubdevices(); subdev++)
    {
        rc = m_pLwRm->ControlBySubdevice(lwgpu->GetGpuSubdevice(subdev),
                        LW2080_CTRL_CMD_GR_SET_CTXSW_TEST_STATE,
                        (void*)&Params, sizeof(Params));
        if (rc != OK)
        {
            ErrPrintf("LWGpuChannel::CtxSwResetMask error: %s\n",
                   rc.Message());
            return rc;
        }
    }
#endif

    return rc;
}

RC LWGpuChannel::CtxSwBash()
{
    RC rc;

#ifdef LW_VERIF_FEATURES
    LW2080_CTRL_GR_SET_CTXSW_TEST_STATE_PARAMS Params = {0};

    Params.hChannel = m_hCh;
    Params.command = LW2080_CTRL_GR_SET_CTXSW_TEST_STATE_COMMAND_BASH;
    Params.arg = 1;

    for (UINT32 subdev = 0;
         subdev < lwgpu->GetGpuDevice()->GetNumSubdevices(); subdev++)
    {
        rc = m_pLwRm->ControlBySubdevice(lwgpu->GetGpuSubdevice(subdev),
                        LW2080_CTRL_CMD_GR_SET_CTXSW_TEST_STATE,
                        (void*)&Params, sizeof(Params));
        if (rc != OK)
        {
            ErrPrintf("CtxSwBash failed: %s\n",
                   rc.Message());
            return rc;
        }
    }
#endif

    return rc;
}

RC LWGpuChannel::CtxSwLog()
{
    RC rc;

#ifdef LW_VERIF_FEATURES
    LW2080_CTRL_GR_SET_CTXSW_TEST_STATE_PARAMS Params = {0};

    Params.hChannel = m_hCh;
    Params.command = LW2080_CTRL_GR_SET_CTXSW_TEST_STATE_COMMAND_DEBUG_CONTROL;
    Params.arg = LW2080_CTRL_GR_SET_CTXSW_TEST_STATE_ARG_DEBUG_CONTROL_LOG;

    for (UINT32 subdev = 0;
         subdev < lwgpu->GetGpuDevice()->GetNumSubdevices(); subdev++)
    {
        rc = m_pLwRm->ControlBySubdevice(lwgpu->GetGpuSubdevice(subdev),
                        LW2080_CTRL_CMD_GR_SET_CTXSW_TEST_STATE,
                        (void*)&Params, sizeof(Params));
        if (rc != OK)
        {
            ErrPrintf("CtxSwLog failed: %s\n",
                   rc.Message());
            return rc;
        }
    }
#endif

    return rc;
}

RC LWGpuChannel::CtxSwSingleStep()
{
    RC rc;

#ifdef LW_VERIF_FEATURES
    LW2080_CTRL_GR_SET_CTXSW_TEST_STATE_PARAMS Params = {0};

    Params.hChannel = m_hCh;
    Params.command = LW2080_CTRL_GR_SET_CTXSW_TEST_STATE_COMMAND_DEBUG_CONTROL;
    Params.arg = LW2080_CTRL_GR_SET_CTXSW_TEST_STATE_ARG_DEBUG_CONTROL_SINGLE_STEP;

    for (UINT32 subdev = 0;
         subdev < lwgpu->GetGpuDevice()->GetNumSubdevices(); subdev++)
    {
        rc = m_pLwRm->ControlBySubdevice(lwgpu->GetGpuSubdevice(subdev),
                        LW2080_CTRL_CMD_GR_SET_CTXSW_TEST_STATE,
                        (void*)&Params, sizeof(Params));
        if (rc != OK)
        {
            ErrPrintf("CtxSwSingleStep failed: %s\n",
                   rc.Message());
            return rc;
        }
    }
#endif

    return rc;
}

RC LWGpuChannel::CtxSwDebug(int num)
{
    RC rc;

#ifdef LW_VERIF_FEATURES
    LW2080_CTRL_GR_SET_CTXSW_TEST_STATE_PARAMS Params = {0};

    Params.hChannel = m_hCh;
    Params.command = LW2080_CTRL_GR_SET_CTXSW_TEST_STATE_COMMAND_DEBUG_CONTROL;
    Params.arg = LW2080_CTRL_GR_SET_CTXSW_TEST_STATE_ARG_DEBUG_CONTROL_DEBUG;

    for (UINT32 subdev = 0;
         subdev < lwgpu->GetGpuDevice()->GetNumSubdevices(); subdev++)
    {
        rc = m_pLwRm->ControlBySubdevice(lwgpu->GetGpuSubdevice(subdev),
                        LW2080_CTRL_CMD_GR_SET_CTXSW_TEST_STATE,
                        (void*)&Params, sizeof(Params));
        if (rc != OK)
        {
            ErrPrintf("CtxSwDebug failed: %s\n",
                   rc.Message());
            return rc;
        }
    }
#endif

    return rc;
}

RC LWGpuChannel::CtxSwSerial()
{
    RC rc;

#ifdef LW_VERIF_FEATURES
    LW2080_CTRL_GR_SET_CTXSW_TEST_STATE_PARAMS Params = {0};

    Params.hChannel = m_hCh;
    Params.command = LW2080_CTRL_GR_SET_CTXSW_TEST_STATE_COMMAND_DEBUG_CONTROL;
    Params.arg = LW2080_CTRL_GR_SET_CTXSW_TEST_STATE_ARG_DEBUG_CONTROL_SERIAL;

    for (UINT32 subdev = 0;
         subdev < lwgpu->GetGpuDevice()->GetNumSubdevices(); subdev++)
    {
        rc = m_pLwRm->ControlBySubdevice(lwgpu->GetGpuSubdevice(subdev),
                        LW2080_CTRL_CMD_GR_SET_CTXSW_TEST_STATE,
                        (void*)&Params, sizeof(Params));
        if (rc != OK)
        {
            ErrPrintf("CtxSwSerial failed: %s\n",
                   rc.Message());
            return rc;
        }
    }
#endif

    return rc;
}

RC LWGpuChannel::CtxSwSelf()
{
    RC rc;

#ifdef LW_VERIF_FEATURES
    LW2080_CTRL_GR_SET_CTXSW_TEST_STATE_PARAMS Params = {0};

    Params.hChannel = m_hCh;
    Params.command = LW2080_CTRL_GR_SET_CTXSW_TEST_STATE_COMMAND_DEBUG_CONTROL;
    Params.arg = LW2080_CTRL_GR_SET_CTXSW_TEST_STATE_ARG_DEBUG_CONTROL_SELF;

    for (UINT32 subdev = 0;
         subdev < lwgpu->GetGpuDevice()->GetNumSubdevices(); subdev++)
    {
        rc = m_pLwRm->ControlBySubdevice(lwgpu->GetGpuSubdevice(subdev),
                        LW2080_CTRL_CMD_GR_SET_CTXSW_TEST_STATE,
                        (void*)&Params, sizeof(Params));
        if (rc != OK)
        {
            ErrPrintf("CtxSwSelf failed: %s\n",
                   rc.Message());
            return rc;
        }
    }
#endif

    return rc;
}

RC LWGpuChannel::CtxSwZlwllDefault()
{
    RC rc;

    if (m_pLWGpuTsg && m_pLWGpuTsg->GetZlwllModeSet())
    {
        return rc;
    }

#ifdef LW_VERIF_FEATURES
    LW2080_CTRL_GR_CTXSW_ZLWLL_MODE_PARAMS Params = {0};

    // since amodel doesn't support any of these, don't send it to save from getting an error print
    if (Platform::GetSimulationMode() == Platform::Amodel)
        return OK;

    Params.hChannel = m_hCh;
    Params.hShareClient = m_pLwRm->GetClientHandle();
    Params.hShareChannel = m_hCh;
    Params.zlwllMode = LW2080_CTRL_CTXSW_ZLWLL_MODE_GLOBAL;

    for (UINT32 subdev = 0;
         subdev < lwgpu->GetGpuDevice()->GetNumSubdevices(); subdev++)
    {
        rc = m_pLwRm->ControlBySubdevice(lwgpu->GetGpuSubdevice(subdev),
                        LW2080_CTRL_CMD_GR_CTXSW_ZLWLL_MODE,
                        (void*)&Params, sizeof(Params));
        if (rc != OK)
        {
            ErrPrintf("CtxSwZlwllDefault failed: %s\n",
                   rc.Message());
            return rc;
        }
    }

    if (m_pLWGpuTsg)
    {
        m_pLWGpuTsg->SetZlwllModeSet();
    }
#endif

    return rc;
}

RC LWGpuChannel::CtxSwVprMode()
{
    RC rc;

#ifdef LW_VERIF_FEATURES
    LW2080_CTRL_GR_SET_CHANNEL_VPR_PARAMS params;
    params.hChannel = m_hCh;
    params.hClient = m_pLwRm->GetClientHandle();

    for (UINT32 subdev = 0;
         subdev < lwgpu->GetGpuDevice()->GetNumSubdevices(); subdev++)
    {
        rc = m_pLwRm->ControlBySubdevice(lwgpu->GetGpuSubdevice(subdev),
            LW2080_CTRL_CMD_GR_SET_CHANNEL_VPR,
            &params,
            sizeof(params));

        if (OK != rc)
        {
            ErrPrintf("CtxSwVprMode failed: %s\n", rc.Message());
            return rc;
        }
    }
#endif

    return rc;
}

bool LWGpuChannel::ParseFile
(
    int* pNum,
    vector<LwS32>* pOutLimiters,
    vector<string>* pOutNames,
    vector<char*>* pOutNamePointers,
    _limiter_table* pLimiterTable
)
const
{
#ifdef LW_VERIF_FEATURES
    int linenum;
    FILE * pFile;
    int i;
    LwU32 nStop;
    LwU32 nStart;

    char buf[256];
    char * pStr;

    InfoPrintf("Limiter File in Use [%s]\n", LimiterFileName.c_str());
    if (OK != Utility::OpenFile(LimiterFileName.c_str(), &pFile, "rt"))
    {
        return false;
    }

    vector<LwS32> Limiters;

    linenum = 0;
    while (!feof(pFile))
    {
        pStr = fgets(buf, sizeof(buf), pFile);

        if (NULL == pStr)
            continue;

        // blank line
        if (('\n' == *pStr) || ('\r' == *pStr))
            continue;

        // Find Start of String
        nStart = 0;
        while (isspace(buf[nStart]))
        {
            if (nStart < sizeof(buf))
                nStart++;
            else
            {
                ErrPrintf("Limiter File scan line error at line %d\n", linenum);
                return false;
            }
        }

        // look for a letter
        while (!isalpha(buf[nStart]))
        {
           if (nStart < sizeof(buf))
               nStart++;
           else
           {
               ErrPrintf("Limiter File scan line error at line %d\n", linenum);
               return false;
           }
        }

        // While not white space
        nStop = nStart;
        while (!isspace(buf[nStop]))
        {
            if (nStop < sizeof(buf))
                nStop++;
            else
            {
                ErrPrintf("Limiter File scan line error at line %d\n", linenum);
                return false;
            }
        }

        if (nStop == nStart)
            continue;

        LwS32 mean;
        if (1 != sscanf(&buf[nStop],"%d", &mean))
        {
            ErrPrintf("Limiter File scan line error at line %d\n", linenum);
            return false;
        }

        pOutNames->push_back(string(&buf[nStart], nStop-nStart));
        Limiters.push_back(mean);
    }

    fclose(pFile);

    if (Limiters.empty())
    {
        return false;
    }

    if (ctxswNumReplay < 1)
        *pNum = 1;
    else
        *pNum = ctxswNumReplay;

    const int nCount = (int)Limiters.size();
    *pNum = (*pNum) * nCount;
    vector<LwS32> pTable(*pNum);

    // Get Random Numbers
    for (i=0; i<nCount; i++)
    {
        if (0 != Limiters[i])
        {
            vector<LwS32> randoms;
            for (int j=0; j<ctxswNumReplay; j++)
                randoms.push_back(pRandomStream->RandomUnsigned(0, Limiters[i]));

            // sort
            sort(randoms.begin(), randoms.end());

            // set
            for (int j=0; j<ctxswNumReplay; j++)
            {
                pTable[i + j*nCount] = randoms[j];
            }
        }
        else
        {
            for (int j=0; j<ctxswNumReplay; j++)
                pTable[i + j*nCount] = 0;
        }
    }

    // Print them out
    for (i=0; i<nCount; i++)
    {
        InfoPrintf("Limiter Switch Index %d\n", i);
        for (int j=0; j<ctxswNumReplay; j++)
        {
            InfoPrintf("Limiter Switch %d %d\n", i+j*nCount, pTable[i+j*nCount]);
        }
    }

    for (i=0; i<nCount; i++)
    {
        pOutNamePointers->push_back(const_cast<char*>((*pOutNames)[i].c_str()));
    }

    pOutLimiters->swap(pTable);

    pLimiterTable->count = nCount;
    pLimiterTable->pLimiters = &(*pOutLimiters)[0];
    pLimiterTable->ppStr = &(*pOutNamePointers)[0];

    return true;
#else
    return false;
#endif
}

RC LWGpuChannel::CtxSwType(int Type)
{
    RC rc;

#ifdef LW_VERIF_FEATURES
    LW2080_CTRL_GR_SET_CTXSW_TEST_STATE_PARAMS Params = {0};
    LW_CFGEX_CTXSW_LIMITERS_PARAMS CtxSwLimiters = {0};
    int totalCount;
    UINT32 subdev;

    Params.hChannel = m_hCh;
    Params.command = LW2080_CTRL_GR_SET_CTXSW_TEST_STATE_COMMAND_SWITCH_TYPE;
    Params.arg = Type;

    if (Type == LW2080_CTRL_GR_SET_CTXSW_TEST_STATE_ARG_SWITCH_TYPE_SPILLREPLAYONLY)
    {
        // Pass in Real Table
        CtxSwLimiters.hChannel = m_hCh;
        vector<LwS32> Limiters;
        vector<string> Names;
        vector<char*> NamePointers;
        LIMITER_TABLE LimiterTable;
        if (ParseFile(&totalCount, &Limiters, &Names,
                    &NamePointers, &LimiterTable))
        {
            CtxSwLimiters.pLimiterTable = &LimiterTable;
            CtxSwLimiters.totalCount = totalCount;
            for (subdev = 0;
                 subdev < lwgpu->GetGpuDevice()->GetNumSubdevices(); subdev++)
            {
                rc = m_pLwRm->ConfigSetEx(LW_CFGEX_SET_CTXSW_LIMITERS,
                                        &CtxSwLimiters, sizeof(CtxSwLimiters),
                                        lwgpu->GetGpuSubdevice(subdev));
                if (rc != OK)
                {
                    ErrPrintf("CtxSwLimiters failed: %s\n",
                           rc.Message());
                    return rc;
                }
            }

            // Set Spill Replay Only
            for (subdev = 0;
                 subdev < lwgpu->GetGpuDevice()->GetNumSubdevices(); subdev++)
            {
                rc = m_pLwRm->ControlBySubdevice(lwgpu->GetGpuSubdevice(subdev),
                        LW2080_CTRL_CMD_GR_SET_CTXSW_TEST_STATE,
                        (void*)&Params, sizeof(Params));
                if (rc != OK)
                {
                    ErrPrintf("CtxSwType failed: %s\n",
                           rc.Message());
                    return rc;
                }
            }
        }
    }
    else
    {
        for (subdev = 0;
                subdev < lwgpu->GetGpuDevice()->GetNumSubdevices(); subdev++)
        {
            rc = m_pLwRm->ControlBySubdevice(lwgpu->GetGpuSubdevice(subdev),
                    LW2080_CTRL_CMD_GR_SET_CTXSW_TEST_STATE,
                    (void*)&Params, sizeof(Params));
            if (rc != OK)
            {
                ErrPrintf("CtxSwType failed: %s\n",
                        rc.Message());
                return rc;
            }
        }
    }
#endif

    return rc;
}

int LWGpuChannel::BeginChannelSwitch()
{
    // After enhancing ctxsw behavior, nothing to do here
    // To do: remove it if it's useless
    return 1;
}

int LWGpuChannel::EndChannelSwitch()
{
    // After enhancing ctxsw behavior, nothing to do here
    // To do: remove it if it's useless
    return 1;
}

unsigned int LWGpuChannel::GetSemaphoreAcquireTimeout() const
{
    return semAcquireTimeout;
}

void LWGpuChannel::SetSemaphoreAcquireTimeout(unsigned int acq_to)
{
    semAcquireTimeout = acq_to;
}

void LWGpuChannel::add_listener(PushBufferListener* listener)
{
    m_push_buffer_listeners.push_back(listener);
}

void LWGpuChannel::remove_listener(PushBufferListener* listener)
{
    m_push_buffer_listeners.remove(listener);
}

bool LWGpuChannel::HasSetCtxswPmDone() const
{
    // For a channel in tsg, context is shared.
    // If any channel in the tsg has set the PmCtxsw mode,
    // it means it's done for all channels sharing the context
    if (m_pLWGpuTsg)
    {
        TsgGpuChannels lwgpuChannels = m_pLWGpuTsg->GetTsgGpuChannels();
        for (TsgGpuChannels::iterator it = lwgpuChannels.begin();
             it != lwgpuChannels.end(); ++it)
        {
            if ((*it)->m_SetCtxswPmModeDone)
                return true;
        }

        return false;
    }

    // Legacy channel without explicit tsg
    return m_SetCtxswPmModeDone;
}

bool LWGpuChannel::HasSetCtxswSmpcDone() const
{
    // For a channel in tsg, context is shared.
    // If any channel in the tsg has set the SmpcCtxsw mode,
    // it means it's done for all channels sharing the context
    if (m_pLWGpuTsg)
    {
        TsgGpuChannels lwgpuChannels = m_pLWGpuTsg->GetTsgGpuChannels();
        for (TsgGpuChannels::iterator it = lwgpuChannels.begin();
             it != lwgpuChannels.end(); ++it)
        {
            if ((*it)->m_SetCtxswSmpcModeDone)
                return true;
        }

        return false;
    }

    // Legacy channel without explicit tsg
    return m_SetCtxswSmpcModeDone;
}

void LWGpuChannel::ParseChannelInstLocArgs(const ArgReader *params)
{
    m_InstMemLocOverride = params->ParamPresent("-loc_inst") > 0 ||
                           params->ParamPresent("-loc") > 0;
    m_RAMFCMemLocOverride = params->ParamPresent("-loc_ramfc") > 0 ||
                            params->ParamPresent("-loc") > 0;
    if (m_InstMemLocOverride)
    {
        m_InstMemLoc = (_DMA_TARGET)params->ParamUnsigned("-loc", m_InstMemLoc);
        m_InstMemLoc = (_DMA_TARGET)params->ParamUnsigned("-loc_inst", m_InstMemLoc);
    }

    if (m_RAMFCMemLocOverride)
    {
        m_RAMFCMemLoc = (_DMA_TARGET)params->ParamUnsigned("-loc", m_RAMFCMemLoc);
        m_RAMFCMemLoc = (_DMA_TARGET)params->ParamUnsigned("-loc_ramfc", m_RAMFCMemLoc);
    }

    if (m_InstMemLocOverride || m_RAMFCMemLocOverride)
    {
        // if location is specified, randomized location will be ignored
        if (0 != lwgpu->GetLocationRandomizer())
        {
            WarnPrintf("%s: randomizing inst/ramfc memory location"
                "(-loc_inst_random) is ignored because of option "
                "-loc/-loc_inst/-loc_ramfc. \n", __FUNCTION__);
        }
    }
    else
    {
        RandomStream* rand = lwgpu->GetLocationRandomizer();
        if (rand)
        {
            const _DMA_TARGET location[] =
            {
                _DMA_TARGET_VIDEO,
                _DMA_TARGET_COHERENT,
                _DMA_TARGET_NONCOHERENT
            };

            UINT32 index = rand->Random(0, sizeof(location)/sizeof(location[0]) - 1);
            m_InstMemLoc = location[index];
            m_RAMFCMemLoc = m_InstMemLoc;
            m_InstMemLocOverride = true;
        }
    }

    if (params->ParamPresent("-inst_in_sys") > 0)
    {
        GpuDevice *pGpuDevice = lwgpu->GetGpuDevice();
        if (pGpuDevice->GetSubdevice(0)->HasBug(632241))
        {
            m_InstMemLoc = _DMA_TARGET_NONCOHERENT;
        }
        else
        {
            m_InstMemLoc = _DMA_TARGET_COHERENT;
        }
        m_RAMFCMemLoc = m_InstMemLoc;
        m_InstMemLocOverride = true;
        m_RAMFCMemLocOverride = true;
    }

    if (lwgpu->GetGpuSubdevice()->IsFbBroken() &&
        (m_InstMemLoc == _DMA_TARGET_VIDEO || m_RAMFCMemLoc == _DMA_TARGET_VIDEO))
    {
        WarnPrintf("Overriding channel's Instance Memory and RAMFC location from Vidmem to Coherent Sysmem since FB is broken\n");
        m_InstMemLoc = _DMA_TARGET_COHERENT;
        m_RAMFCMemLoc = _DMA_TARGET_COHERENT;
        m_InstMemLocOverride = true;
        m_RAMFCMemLocOverride = true;
    }
}

void LWGpuChannel::ParseChannelArgs(const ArgReader *params)
{
    if (params->ParamPresent("-pushbufsize"))
    {
        UINT64 old_val = m_Pushbuf.GetArrayPitch();
        UINT64 new_val = params->ParamUnsigned64("-pushbufsize", old_val);
        m_Pushbuf.SetArrayPitch(new_val);
        if (m_PushbufferSizeFromTrace)
        {
            InfoPrintf("Pushbuffer size was overwritten to: %llu by trace_3d command line option -pushbufsize"
                " which was set to: %llu before by header command CHANNEL option PUSHBUFFER_SIZE.\n",
                new_val, old_val);
        }
    }
    ParseDmaBufferArgs(m_Pushbuf, params, "pb", &m_PushbufPteKindName, 0);
    ParseDmaBufferArgs(m_GpFifo, params, "gpfifo", &m_GpFifoPteKindName, 0);
    ParseDmaBufferArgs(m_Err, params, "err", 0, 0);
#ifdef DEBUG
    ChannelType Type;
    Type = (LWGpuChannel::ChannelType)params->ParamUnsigned("-channel_type", GPFIFO_CHANNEL);
    MASSERT(Type == GPFIFO_CHANNEL);
#endif
    if (params->ParamPresent("-gpfifo_entries"))
    {
        UINT32 old_val = m_GpFifoEntries;
        UINT32 new_val = params->ParamUnsigned("-gpfifo_entries", old_val);
        if (new_val == 0 || (new_val & (new_val - 1)) != 0)
        {
            MASSERT(!"gpfifo_entries should be the power of 2.\n");
        }
        m_GpFifoEntries = new_val;
        if (m_GpFifoEntriesFromTrace)
        {
            InfoPrintf("GpFifo entries was overwritten to: %u by trace_3d command line option -gpfifo_entries"
                " which was set to: %u before by header command CHANNEL option GPFIFO_ENTRIES.\n",
                new_val, old_val);
        }
    }
    m_KickoffThreshold = params->ParamUnsigned("-kickoff_thresh", m_KickoffThreshold);
    m_UpdateType = params->ParamPresent("-single_kick") > 0 ? MANUAL : AUTOMATIC;
    m_AutoGpEntryEnable = (params->ParamPresent("-auto_gp_entry_thresh") > 0);
    m_AutoGpEntryThreshold =
        params->ParamUnsigned("-auto_gp_entry_thresh", m_AutoGpEntryThreshold);

    m_UseBar1Doorbell = params->ParamPresent("-use_bar1_doorbell") > 0; 
    ParseChannelInstLocArgs(params);

    m_HashTableSizeOverride = params->ParamPresent("-hash_table_size") > 0;
    m_TimesliceOverride = params->ParamPresent("-timeslice") > 0;
    m_TimescaleOverride = params->ParamPresent("-timescale") > 0;
    m_EngTimesliceOverride = params->ParamPresent("-eng_timeslice") > 0;
    m_PbTimesliceOverride = params->ParamPresent("-pb_timeslice") > 0;
    m_HashTableSize = params->ParamUnsigned("-hash_table_size");
    m_Timeslice = params->ParamUnsigned("-timeslice");
    m_Timescale = params->ParamUnsigned("-timescale");
    m_EngTimeslice = params->ParamUnsigned("-eng_timeslice", 0);
    m_PbTimeslice = params->ParamUnsigned("-pb_timeslice", 0);
    m_Timeout = !params->ParamPresent("-no_timeout");

    m_IdleMethodCount = params->ParamUnsigned("-idle_method_count", 0);

    m_TimeoutMs = params->ParamFloat("-timeout_ms", m_TimeoutMs);
    m_NcohSnoop = params->ParamPresent("-pm_ncoh_no_snoop") > 0;

    m_SleepBetweenMethods = params->ParamUnsigned("-sleep_between_methods", m_SleepBetweenMethods);

    m_UseLegacyWaitIdleWithRiskyIntermittentDeadlocking =
        (params->ParamPresent("-use_legacy_wait_idle_with_risky_intermittent_deadlocking") > 0);

    if (params->ParamPresent("-hwcrccheck_gp") > 0)
    {
        m_HwCrcCheckMode |= Channel::HWCRCCHKMODE_GP;
    }

    if (params->ParamPresent("-hwcrccheck_pb") > 0)
    {
        m_HwCrcCheckMode |= Channel::HWCRCCHKMODE_PB;
    }

    if (params->ParamPresent("-hwcrccheck_method") > 0)
    {
        m_HwCrcCheckMode |= Channel::HWCRCCHKMODE_MTHD;
    }

    if (params->ParamPresent("-random_af_threshold") > 0)
    {
        m_RandomAutoFlushThreshold.Min  = params->ParamNUnsigned("-random_af_threshold", 0);
        m_RandomAutoFlushThreshold.Max  = params->ParamNUnsigned("-random_af_threshold", 1);
        m_RandomAutoFlushThreshold.Seed = params->ParamNUnsigned("-random_af_threshold", 2);
    }
    if (params->ParamPresent("-random_af_threshold_gp_ent") > 0)
    {
        m_RandomAutoFlushThresholdGpFifoEntries.Min  = params->ParamNUnsigned("-random_af_threshold_gp_ent", 0);
        m_RandomAutoFlushThresholdGpFifoEntries.Max  = params->ParamNUnsigned("-random_af_threshold_gp_ent", 1);
        m_RandomAutoFlushThresholdGpFifoEntries.Seed = params->ParamNUnsigned("-random_af_threshold_gp_ent", 2);
    }
    if (params->ParamPresent("-random_auto_gpentry_threshold") > 0)
    {
        m_AutoGpEntryEnable = true;
        m_RandomAutoGpEntryThreshold.Min  = params->ParamNUnsigned("-random_auto_gpentry_threshold", 0);
        m_RandomAutoGpEntryThreshold.Max  = params->ParamNUnsigned("-random_auto_gpentry_threshold", 1);
        m_RandomAutoGpEntryThreshold.Seed = params->ParamNUnsigned("-random_auto_gpentry_threshold", 2);
    }
    if (params->ParamPresent("-pause_before_gpwrite") > 0)
    {
        m_RandomPauseBeforeGPPutWrite.Min  = params->ParamNUnsigned("-pause_before_gpwrite", 0);
        m_RandomPauseBeforeGPPutWrite.Max  = params->ParamNUnsigned("-pause_before_gpwrite", 1);
        m_RandomPauseBeforeGPPutWrite.Seed = params->ParamNUnsigned("-pause_before_gpwrite", 2);
    }
    if (params->ParamPresent("-pause_after_gpwrite") > 0)
    {
        m_RandomPauseAfterGPPutWrite.Min  = params->ParamNUnsigned("-pause_after_gpwrite", 0);
        m_RandomPauseAfterGPPutWrite.Max  = params->ParamNUnsigned("-pause_after_gpwrite", 1);
        m_RandomPauseAfterGPPutWrite.Seed = params->ParamNUnsigned("-pause_after_gpwrite", 2);
    }

    if (params->ParamPresent("-subchannel_base"))
    {
        m_SubchNumBase = params->ParamUnsigned("-subchannel_base");
    }

    SetAllocChIdMode(params);
}

RC LWGpuChannel::AllocChannel(UINT32 engineId, UINT32 HwClass)
{
    RC rc;
    GpuDevice *pGpuDevice = lwgpu->GetGpuDevice();

    MASSERT(!m_pCh);

    // Create error notifier
    if (!m_pLWGpuTsg)
    {
        CHECK_RC(AllocErrorNotifierRC(lwgpu, NULL, m_Err.GetSurf2D(), m_pLwRm));
    }

    // Allocate the GPFIFO memory as a separate memory allocation
    m_GpFifo.SetArrayPitch(m_GpFifoEntries*8);
    m_GpFifo.SetColorFormat(ColorUtils::Y8);
    m_GpFifo.SetForceSizeAlloc(true);
    m_GpFifo.SetGpuVASpace(m_hVASpace);
    if (m_IsGpuManaged)
        m_GpFifo.SetProtect(Memory::ReadWrite);
    if (Platform::GetSimulationMode() == Platform::Amodel)
        m_GpFifo.SetLocation(Memory::Fb);
    else if ( (Platform::GetSimulationMode() == Platform::Hardware) &&
        m_NcohSnoop && (m_GpFifo.GetLocation() == Memory::NonCoherent) )
    {
        ErrPrintf("GPFIFO cannot be put in noncoherent memory when option "
            "'-pm_ncoh_no_snoop' used, use '-coh_gpfifo' to fix this problem\n");
        MASSERT(0);
    }
    CHECK_RC(SetPteKind(m_GpFifo, m_GpFifoPteKindName, lwgpu->GetGpuDevice()));
    // The GPFIFO buffer should not be cached if it's in sysmem.
    if (m_GpFifo.GetLocation() != Memory::Fb)
    {
        if (m_GpFifo.GetGpuCacheMode() == Surface2D::GpuCacheOn)
        {
            ErrPrintf("GPFIFO buffer in system memory must not have GPU caching turned on.  Use -gpu_cache_off_gpfifo.\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        else if (m_GpFifo.GetGpuCacheMode() == Surface2D::GpuCacheDefault)
        {
            m_GpFifo.SetGpuCacheMode(Surface2D::GpuCacheOff);
        }
    }

    CHECK_RC(m_GpFifo.Alloc(lwgpu->GetGpuDevice(), m_pLwRm));
    PrintDmaBufferParams(m_GpFifo);

    CHECK_RC(m_GpFifo.Map(0));

    // Allocate the pushbuffer memory
    if (Platform::GetSimulationMode() == Platform::Amodel)
        m_Pushbuf.SetLocation(Memory::Fb);
    else if ( (Platform::GetSimulationMode() == Platform::Hardware) &&
        m_NcohSnoop && (m_Pushbuf.GetLocation() == Memory::NonCoherent) )
    {
        ErrPrintf("Pushbuffer cannot be put in noncoherent memory when option "
            "'-pm_ncoh_no_snoop' used, use '-coh_pb' to fix this problem\n");
        MASSERT(0);
    }
    CHECK_RC(SetPteKind(m_Pushbuf, m_PushbufPteKindName, lwgpu->GetGpuDevice()));
    // The push buffer should not be cached if it's in sysmem.
    if (m_Pushbuf.GetLocation() != Memory::Fb)
    {
        if (m_Pushbuf.GetGpuCacheMode() == Surface2D::GpuCacheOn)
        {
            ErrPrintf("Pushbuffer in system memory must not have GPU caching turned on.  Use -gpu_cache_off_pb.\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        else if (m_Pushbuf.GetGpuCacheMode() == Surface2D::GpuCacheDefault)
        {
            m_Pushbuf.SetGpuCacheMode(Surface2D::GpuCacheOff);
        }
    }

    m_Pushbuf.SetGpuVASpace(m_hVASpace);
    CHECK_RC(m_Pushbuf.Alloc(lwgpu->GetGpuDevice(), m_pLwRm));
    PrintDmaBufferParams(m_Pushbuf);
    
    CHECK_RC(m_Pushbuf.Map(0));

    // Select flags for various channel overrides
    UINT32 verifFlags = 0;
#ifdef LW_VERIF_FEATURES
    if (m_InstMemLocOverride)
    {
        switch (m_InstMemLoc)
        {
            case _DMA_TARGET_VIDEO:
                verifFlags |= DRF_DEF(OS04, _VERIF_FLAGS, _INST_MEM_LOC, _VID);
                break;
            case _DMA_TARGET_COHERENT:
                verifFlags |= DRF_DEF(OS04, _VERIF_FLAGS, _INST_MEM_LOC, _COH);
                break;
            case _DMA_TARGET_NONCOHERENT:
                verifFlags |= DRF_DEF(OS04, _VERIF_FLAGS, _INST_MEM_LOC, _NCOH);
                break;
            default:
                MASSERT(0);
        }
    }
    if (m_RAMFCMemLocOverride)
    {
        switch (m_RAMFCMemLoc)
        {
            case _DMA_TARGET_VIDEO:
                verifFlags |= DRF_DEF(OS04, _VERIF_FLAGS, _RAMFC_MEM_LOC, _VID);
                break;
            case _DMA_TARGET_COHERENT:
                verifFlags |= DRF_DEF(OS04, _VERIF_FLAGS, _RAMFC_MEM_LOC, _COH);
                break;
            case _DMA_TARGET_NONCOHERENT:
                verifFlags |= DRF_DEF(OS04, _VERIF_FLAGS, _RAMFC_MEM_LOC, _NCOH);
                break;
            default:
                MASSERT(0);
        }
    }
    if (m_HashTableSizeOverride)
    {
        switch (m_HashTableSize)
        {
            case 4:
                verifFlags |= DRF_DEF(OS04, _VERIF_FLAGS, _HASH_TABLE_SIZE, _4KB);
                break;
            case 8:
                verifFlags |= DRF_DEF(OS04, _VERIF_FLAGS, _HASH_TABLE_SIZE, _8KB);
                break;
            case 16:
                verifFlags |= DRF_DEF(OS04, _VERIF_FLAGS, _HASH_TABLE_SIZE, _16KB);
                break;
            case 32:
                verifFlags |= DRF_DEF(OS04, _VERIF_FLAGS, _HASH_TABLE_SIZE, _32KB);
                break;
            default:
                MASSERT(0);
        }
    }
    if (m_TimesliceOverride)
    {
        verifFlags |= DRF_DEF(OS04, _VERIF_FLAGS, _TIMESLICE_SELECT_OVERRIDE, _ENABLED);
        verifFlags |= DRF_NUM(OS04, _VERIF_FLAGS, _TIMESLICE_SELECT, m_Timeslice-1);
    }
    if (m_TimescaleOverride)
    {
        verifFlags |= DRF_DEF(OS04, _VERIF_FLAGS, _TIMESLICE_TIMESCALE_OVERRIDE, _ENABLED);
        verifFlags |= DRF_NUM(OS04, _VERIF_FLAGS, _TIMESLICE_TIMESCALE, m_Timescale);
    }
    if (m_Timeout)
        verifFlags |= DRF_DEF(OS04, _VERIF_FLAGS, _TIMESLICE_TIMEOUT, _ENABLED);
    else
        verifFlags |= DRF_DEF(OS04, _VERIF_FLAGS, _TIMESLICE_TIMEOUT, _DISABLED);
#endif

    // Create the channel
    UINT32 ChClass = 0;
    if (Platform::GetSimulationMode() == Platform::Amodel)
    {
        if (EngineClasses::IsGpuFamilyClassOrLater(
            HwClass, LWGpuClasses::GPU_CLASS_KEPLER))
            ChClass = KEPLER_CHANNEL_GPFIFO_A;
        else
            ChClass = GF100_CHANNEL_GPFIFO;
    }
    else
    {
        static const UINT32 GpFifoClasses[] =
        {
            HOPPER_CHANNEL_GPFIFO_A,
            AMPERE_CHANNEL_GPFIFO_A,
            TURING_CHANNEL_GPFIFO_A,
            VOLTA_CHANNEL_GPFIFO_A,
            PASCAL_CHANNEL_GPFIFO_A,
            MAXWELL_CHANNEL_GPFIFO_A,
            KEPLER_CHANNEL_GPFIFO_C,
            KEPLER_CHANNEL_GPFIFO_B,
            KEPLER_CHANNEL_GPFIFO_A,
            GF100_CHANNEL_GPFIFO,
        };
        CHECK_RC(m_pLwRm->GetFirstSupportedClass(static_cast<UINT32>(NUMELEMS(GpFifoClasses)),
                                      GpFifoClasses, &ChClass, pGpuDevice));
    }

    const char *FamilyName = "Tesla";
    switch (ChClass)
    {
        case GF100_CHANNEL_GPFIFO:
            FamilyName = "Fermi";
            break;
        case KEPLER_CHANNEL_GPFIFO_A:
        case KEPLER_CHANNEL_GPFIFO_B:
        case KEPLER_CHANNEL_GPFIFO_C:
            FamilyName = "Kepler";
            break;
        case MAXWELL_CHANNEL_GPFIFO_A:
            FamilyName = "Maxwell";
            break;
        case PASCAL_CHANNEL_GPFIFO_A:
            FamilyName = "Pascal";
            break;
        case VOLTA_CHANNEL_GPFIFO_A:
            FamilyName = "Volta";
            break;
        case TURING_CHANNEL_GPFIFO_A:
            FamilyName = "Turing";
            break;
        case AMPERE_CHANNEL_GPFIFO_A:
            FamilyName = "Ampere";
            break;
        case HOPPER_CHANNEL_GPFIFO_A:
            FamilyName = "Hopper";
            break;
    }
    InfoPrintf("Creating %s GPFIFO channel...\n", FamilyName);
    MASSERT(ChClass);
    m_HwChannelClass = ChClass;

    LwRm::Handle contextObject = 0;
    UINT32 flags = 0;
    if (m_pLWGpuTsg)
    {
        UINT32 hParent = m_pLWGpuTsg->GetHandle();

        shared_ptr<SubCtx> pSubCtx = GetSubCtx();
        if (pSubCtx.get())
        {
            if (!pSubCtx->IsVeidValid())
            {
                UINT32 veid = 0;
                rc = m_pLWGpuTsg->AllocVeid(&veid, pSubCtx);
                if (OK != rc)
                {
                    ErrPrintf("%s: AllocVeid() failed with rc =  %s\n",
                        __FUNCTION__, rc.Message());
                    return rc;
                }

                pSubCtx->SetVeid(veid);
            }

            contextObject = pSubCtx->GetHandle();

            bool bSetModeTohw;
            rc = m_pLWGpuTsg->SetPartitionMode(pSubCtx->GetTraceTsg()->GetPartitionMode(), &bSetModeTohw);

            if(rc != OK)
            {
                ErrPrintf("%s: Invalid partition mode. rc = %s.\n", __FUNCTION__, rc.Message());
                return rc;
            }

            if(bSetModeTohw)
            {
                // 1. set partition table mode
                LW2080_CTRL_GR_SET_TPC_PARTITION_MODE_PARAMS modeParams = { };
                modeParams.hChannelGroup = hParent;
                modeParams.mode = (pSubCtx->GetTraceTsg()->GetPartitionMode() == PEConfigureFile::STATIC) ?
                        LW0080_CTRL_GR_TPC_PARTITION_MODE_STATIC :
                        LW0080_CTRL_GR_TPC_PARTITION_MODE_DYNAMIC;

                rc = m_pLwRm->Control(m_pLwRm->GetSubdeviceHandle(lwgpu->GetGpuSubdevice()),
                                      LW2080_CTRL_CMD_GR_SET_TPC_PARTITION_MODE,
                                      (void *)&modeParams,
                                      sizeof(modeParams));

                DebugPrintf("SUBCTX: Set partition table mode:\n");
                DebugPrintf("Partition mode: %s\n", (pSubCtx->GetTraceTsg()->GetPartitionMode() ==
                     PEConfigureFile::STATIC)?"STATIC":"DYNAMIC");

                if(rc != OK)
                {
                    ErrPrintf("%s: set partition table mode error.\n", __FUNCTION__);
                    return rc;
                }

            }

            MASSERT(m_pLWGpuTsg->GetPartitionMode() != PEConfigureFile::NOT_SET);
            if(m_pLWGpuTsg->GetPartitionMode() != PEConfigureFile::NONE)
            {
               // 2. configure partition table
                LW9067_CTRL_TPC_PARTITION_TABLE_TPC_INFO tpcList[LW9067_CTRL_TPC_PARTITION_TABLE_TPC_COUNT_MAX] = {{0, 0}};
                UINT32 TpcCount = 0;
                for(ValidPLs::const_iterator it = pSubCtx->Begin();
                        it != pSubCtx->End(); ++it )
                {
                    LW9067_CTRL_TPC_PARTITION_TABLE_TPC_INFO params = {0};
                    params.globalTpcIndex = (*it).first;
                    params.lmemBlockIndex = (*it).second;
                    tpcList[TpcCount++] = params;
                }

                LW9067_CTRL_TPC_PARTITION_TABLE_PARAMS params = {0};
                params.numUsedTpc = TpcCount;
                memcpy(params.tpcList, tpcList, LW9067_CTRL_TPC_PARTITION_TABLE_TPC_COUNT_MAX * sizeof(LW9067_CTRL_TPC_PARTITION_TABLE_TPC_INFO));

                rc = m_pLwRm->Control(contextObject,
                        LW9067_CTRL_CMD_SET_TPC_PARTITION_TABLE,
                        (void *)&params,
                        sizeof(params));

                DebugPrintf("-----------Setting Partition Table value-----------\n");
                DebugPrintf("SubCtx handle 0x%08x\n", pSubCtx->GetHandle());
                DebugPrintf("SubCtx name %s\n", pSubCtx->GetName().c_str());
                for(UINT32 i = 0; i < TpcCount; ++i)
                {
                    DebugPrintf("Global Logical Tpc index %02d = LMEM index (%2d)\n", tpcList[i].globalTpcIndex, tpcList[i].lmemBlockIndex);
                }

                if(rc != OK)
                {
                    ErrPrintf("%s: configure partition table error.\n", __FUNCTION__);
                    return rc;
                }
            }

            // set watermark value
            // Only set watermark if it met the below condition
            // 1. the watermark value is set by the test instead of default value
            // 2. this subctx has never set the watermark value
            if((pSubCtx->GetWatermark() != UNINITIALIZED_WATERMARK_VALUE) &&
                pSubCtx->HasSetWatermarkFlag() == false)
            {
                LW9067_CTRL_CWD_WATERMARK_PARAMS waterMarkParams = {0};
                waterMarkParams.watermarkValue = static_cast<UINT16>(pSubCtx->GetWatermark());

                rc = m_pLwRm->Control(contextObject,
                        LW9067_CTRL_CMD_SET_CWD_WATERMARK,
                        (void *)&waterMarkParams,
                        sizeof(waterMarkParams));

                DebugPrintf("-----------Setting waterMark value-----------\n");
                DebugPrintf("SubCtx Name = %s.\n", pSubCtx->GetName().c_str());
                DebugPrintf("Watermark Value = 0x%x.\n", pSubCtx->GetWatermark());

                if(rc != OK)
                {
                    ErrPrintf("%s: watermark setting error.\n", __FUNCTION__);
                    return rc;
                }
                pSubCtx->SetWatermarkFlag(true);
            }

        }
    }

    if (m_SCGType == COMPUTE1)
    {
        if (!m_pLWGpuTsg || !m_pLWGpuTsg->GetHandle())
        {
            ErrPrintf("%s: only tsg channel supports "
                    "SCG type COMPUTE1\n", __FUNCTION__);
            return RC::SOFTWARE_ERROR;
        }

        flags |= DRF_DEF(OS04, _FLAGS, _GROUP_CHANNEL_THREAD, _ONE);
    }

    if (m_CEPrefetchEnable)
    {
        // Set flag to enable prefetch for CE channel.
        flags |= DRF_DEF(OS04, _FLAGS, _SET_EVICT_LAST_CE_PREFETCH_CHANNEL, _TRUE);
    }

    if (m_Pbdma != 0)
    {
        if (!m_pLWGpuTsg || !m_pLWGpuTsg->GetHandle())
        {
            ErrPrintf("%s: only tsg channel supports "
                "Non-default values (0) for pbdma index\n", __FUNCTION__);
            return RC::SOFTWARE_ERROR;
        }
        flags |= DRF_DEF(OS04, _FLAGS, _GROUP_CHANNEL_RUNQUEUE, _ONE);
    }

    MASSERT(m_Pushbuf.GetCtxDmaHandle() == m_GpFifo.GetCtxDmaHandle());
    CHECK_RC(m_pLwRm->AllocChannelGpFifo(&m_hCh, m_HwChannelClass,
                                    m_Err.GetMemHandle(),
                                    m_Err.GetAddress(),
                                    m_Pushbuf.GetMemHandle(),
                                    m_Pushbuf.GetAddress(),
                                    m_Pushbuf.GetCtxDmaOffsetGpu()+m_Pushbuf.GetExtraAllocSize(),
                                    (UINT32)m_Pushbuf.GetSize(),
                                    m_GpFifo.GetAddress(),
                                    m_GpFifo.GetCtxDmaOffsetGpu()+m_GpFifo.GetExtraAllocSize(),
                                    m_GpFifoEntries, contextObject, verifFlags, 
                                    GetVerifFlags2(engineId, m_HwChannelClass), 
                                    flags,
                                    pGpuDevice,
                                    m_pLWGpuTsg,
                                    m_hVASpace, 
                                    engineId,
                                    m_UseBar1Doorbell));

    m_EngineId = engineId;

    //b200340035:
    //Since gv11b has no video memory, update inst and RAMFC location according to where rm really allocated.
    if (Platform::GetSimulationMode() != Platform::Amodel)
    {
        LW2080_CTRL_FIFO_MEM_INFO instProperties = MDiagUtils::GetInstProperties(pGpuDevice->GetSubdevice(0), m_hCh, m_pLwRm);
        switch(instProperties.aperture)
        {
            case LW2080_CTRL_CMD_FIFO_GET_CHANNEL_MEM_APERTURE_VIDMEM:
                m_InstMemLoc = _DMA_TARGET_VIDEO;
                break;
            case LW2080_CTRL_CMD_FIFO_GET_CHANNEL_MEM_APERTURE_SYSMEM_COH:
                m_InstMemLoc = _DMA_TARGET_COHERENT;
                break;
            case LW2080_CTRL_CMD_FIFO_GET_CHANNEL_MEM_APERTURE_SYSMEM_NCOH:
                m_InstMemLoc = _DMA_TARGET_NONCOHERENT;
                break;
            default:
                MASSERT(0);
                break;
        }
        m_RAMFCMemLoc = m_InstMemLoc;
    }

#ifdef LW_VERIF_FEATURES
    // bug 836173:
    // ramfc is not seperate from the inst block on fermi+.
    // RM will ignore the flag LWOS04_VERIF_FLAGS_RAMFC_MEM_LOC.
    // Fail the test to force user to check the argument -loc_ramfc.
    bool isSameLocation = m_InstMemLocOverride &&
                          m_RAMFCMemLocOverride &&
                          (m_InstMemLoc == m_RAMFCMemLoc);
    if (m_RAMFCMemLocOverride && !isSameLocation)
    {
        ErrPrintf("%s: ramfc is not seperate from the inst block on fermi+. "
            "It's invalid to specify a different mem type by argument "
            "-loc_ramfc/-vid_ramfc/-coh_ramfc/-ncoh_ramfc.\n", __FUNCTION__);
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
#endif

    // Program eng/pb timeslice for Fermi
    if (m_EngTimesliceOverride || m_PbTimesliceOverride)
    {
        LW0080_CTRL_FIFO_SET_CHANNEL_PROPERTIES_PARAMS params = {0};
        params.hChannel = m_hCh;

        if (m_EngTimesliceOverride)
        {
            if (m_EngTimeslice == 0)
            {
                // Disable engine timeslice
                params.property = LW0080_CTRL_FIFO_SET_CHANNEL_PROPERTIES_ENGINETIMESLICEDISABLE;
                params.value    = 0;
            }
            else
            {
                // Program engine timeslice
                params.property = LW0080_CTRL_FIFO_SET_CHANNEL_PROPERTIES_ENGINETIMESLICEINMICROSECONDS;
                params.value    = m_EngTimeslice;
            }
            rc = m_pLwRm->Control(m_pLwRm->GetDeviceHandle(pGpuDevice),
                                    LW0080_CTRL_CMD_FIFO_SET_CHANNEL_PROPERTIES,
                                    &params,
                                    sizeof(params));
            if (rc != OK)
            {
                ErrPrintf("%s is failed to set engine time slice: %s\n",
                    __FUNCTION__, rc.Message());
                return rc;
            }
        }
        if (m_PbTimesliceOverride)
        {
            if (m_PbTimeslice == 0)
            {
                // Disable pbdma timeslice
                params.property = LW0080_CTRL_FIFO_SET_CHANNEL_PROPERTIES_PBDMATIMESLICEDISABLE;
                params.value    = 0;
            }
            else
            {
                // Program pbdma timeslice
                params.property = LW0080_CTRL_FIFO_SET_CHANNEL_PROPERTIES_PBDMATIMESLICEINMICROSECONDS;
                params.value    = m_PbTimeslice;
            }
            rc = m_pLwRm->Control(m_pLwRm->GetDeviceHandle(pGpuDevice),
                                    LW0080_CTRL_CMD_FIFO_SET_CHANNEL_PROPERTIES,
                                    &params,
                                    sizeof(params));
            if (rc != OK)
            {
                ErrPrintf("%s is failed to set pushbuffer time slice: %s\n",
                    __FUNCTION__, rc.Message());
                return rc;
            }
        }
    }

    // If they exist, we want to bind some context DMAs
    UINT32 hCtxDma = pGpuDevice->GetGartCtxDma(m_pLwRm);
    CHECK_RC(m_pLwRm->BindContextDma(m_hCh, hCtxDma));

    // Amodel needs its own error notifier and pushbuffer context DMAs bound

    LwRm::Handle errCtxDmaHandle;
    if (m_pLWGpuTsg)
    {
        // Channels in a TSG do not have individual error notifier buffers.
        // Instead the TSG will have one error notifier that will be used
        // by all the channels.
        errCtxDmaHandle = m_pLWGpuTsg->GetErrCtxDmaHandle();
    }
    else
    {
        errCtxDmaHandle = m_Err.GetCtxDmaHandle();
    }

    if (errCtxDmaHandle != 0)
    {
        CHECK_RC(m_pLwRm->BindContextDma(m_hCh, errCtxDmaHandle));
    }

    m_pCh = m_pLwRm->GetChannel(m_hCh);

    // Create preemption context buffer
    if (GetPreemptive())
    {
        // If it's preemptive, get the preemption context buffer size
        // and allocate memory for it, see bug 642242
        UINT64 engCtxAlignment = 0;
        UINT64 engCtxSize = 0;
        if (m_pSmcEngine)
        {
            LW2080_CTRL_GR_GET_ENGINE_CONTEXT_PROPERTIES_PARAMS params = { };
            params.engineId = LW0080_CTRL_FIFO_GET_ENGINE_CONTEXT_PROPERTIES_ENGINE_ID_COMPUTE_PREEMPT;

            rc = m_pLwRm->Control(m_pLwRm->GetSubdeviceHandle(lwgpu->GetGpuSubdevice()),
                                  LW2080_CTRL_CMD_GR_GET_ENGINE_CONTEXT_PROPERTIES,
                                  &params,
                                  sizeof(params));
            if (rc != OK)
            {
                ErrPrintf("%s is failed to get engine context properties: %s\n",
                            __FUNCTION__, rc.Message());
                return rc;
            }
            engCtxAlignment = params.alignment;
            engCtxSize = params.size;
        }
        else
        {
            LW0080_CTRL_FIFO_GET_ENGINE_CONTEXT_PROPERTIES_PARAMS params = {0};
            params.engineId = LW0080_CTRL_FIFO_GET_ENGINE_CONTEXT_PROPERTIES_ENGINE_ID_COMPUTE_PREEMPT;
            rc = m_pLwRm->Control(m_pLwRm->GetDeviceHandle(pGpuDevice),
                                  LW0080_CTRL_CMD_FIFO_GET_ENGINE_CONTEXT_PROPERTIES,
                                  &params,
                                  sizeof(params));
            if (rc != OK)
            {
                ErrPrintf("%s is failed to get engine context properties: %s\n",
                            __FUNCTION__, rc.Message());
                return rc;
            }
            engCtxAlignment = params.alignment;
            engCtxSize = params.size;
        }

        m_PreemptCtxBuf.SetAlignment(engCtxAlignment);
        m_PreemptCtxBuf.SetColorFormat(ColorUtils::Y8);
        m_PreemptCtxBuf.SetArrayPitch(engCtxSize);
        m_PreemptCtxBuf.SetLocation(Memory::Fb);
        rc = m_PreemptCtxBuf.Alloc(lwgpu->GetGpuDevice(), m_pLwRm);
        if (rc != OK)
        {
            ErrPrintf("%s is failed to allocate preempt ctx buffer: %s\n",
                __FUNCTION__, rc.Message());
            return rc;
        }

        if (m_PreemptCtxBuf.GetCtxDmaOffsetGpu() & 0xFF)
        {
            ErrPrintf("Preempt context buffer address needs to align to 256bytes\n");
            m_PreemptCtxBuf.Free();
            return RC::SOFTWARE_ERROR;
        }

        rc = m_PreemptCtxBuf.Map(0);
        if (rc != OK)
        {
            ErrPrintf("%s is failed to map preempt ctx buffer: %s\n",
                __FUNCTION__, rc.Message());
            return rc;
        }
        // Bug 646106: need to clear only the first 37KB.
        Platform::MemSet(m_PreemptCtxBuf.GetAddress(), 0, 37*1024);
        PrintDmaBufferParams(m_PreemptCtxBuf);
    }

    // Tell the Channel object whether to log the pushbuffer on flush
    EnableLogging(lwgpu->GetChannelLogging());

    // Tell the Channel object whether to write Put automatically
    SetUpdateType(m_UpdateType);

    bool RecallwlateThresholds = false;
    if (m_RandomAutoFlushThreshold.Max > 0)
    {
        m_pCh->SetRandomAutoFlushThreshold(m_RandomAutoFlushThreshold);
        RecallwlateThresholds = true;
    }
    if (m_RandomAutoFlushThresholdGpFifoEntries.Max > 0)
    {
        m_pCh->SetRandomAutoFlushThresholdGpFifoEntries(m_RandomAutoFlushThresholdGpFifoEntries);
        RecallwlateThresholds = true;
    }
    if (m_RandomAutoGpEntryThreshold.Max > 0)
    {
        m_pCh->SetRandomAutoGpEntryThreshold(m_RandomAutoGpEntryThreshold);
        RecallwlateThresholds = true;
    }
    if (RecallwlateThresholds)
        m_pCh->ClearPushbuffer();

    if (m_RandomPauseBeforeGPPutWrite.Max > 0)
        m_pCh->SetRandomPauseBeforeGPPutWrite(m_RandomPauseBeforeGPPutWrite);
    if (m_RandomPauseAfterGPPutWrite.Max > 0)
        m_pCh->SetRandomPauseAfterGPPutWrite(m_RandomPauseAfterGPPutWrite);

    if (ctxswReset)
    {
        CHECK_RC(CtxSwResetMask());
    }
    if (ctxswLog)
    {
        CHECK_RC(CtxSwLog());
    }
    if (ctxswBash)
    {
        CHECK_RC(CtxSwBash());
    }
    if (ctxswSingleStep)
    {
        CHECK_RC(CtxSwSingleStep());
    }
    if (ctxswDebug>=0)
    {
        CHECK_RC(CtxSwDebug(ctxswDebug));
    }
    if (ctxswSerial)
    {
        CHECK_RC(CtxSwSerial());
    }
    if (ctxswType)
    {
        CHECK_RC(CtxSwType(ctxswType));
    }
    if (ctxswSelf)
    {
        CHECK_RC(CtxSwSelf());
    }

    //
    // This call triggers RmConfigSet/Get which are not supported in vGPU.
    // Temporarily disabling for VF (Bug 1974352)
    // Zlwll state not needed in SMC mode
    if (!Platform::IsVirtFunMode() && !m_pSmcEngine)
    {
        CHECK_RC(CtxSwZlwllDefault());
    }

    if (GetVprMode())
    {
        CHECK_RC(CtxSwVprMode());
    }

    m_pCh->SetDefaultTimeoutMs(m_TimeoutMs);
    m_pCh->SetLegacyWaitIdleWithRiskyIntermittentDeadlocking(m_UseLegacyWaitIdleWithRiskyIntermittentDeadlocking);
    if (m_HwCrcCheckMode != Channel::HWCRCCHKMODE_NONE)
    {
        rc = m_pCh->SetHwCrcCheckMode(m_HwCrcCheckMode);
        if (rc != OK)
        {
            ErrPrintf("%s is failed to set hw crc check mode: %s\n",
                __FUNCTION__, rc.Message());
            return rc;
        }
    }

    // initiate available subchannel number
    m_AvailableSubchNum.clear();
    for (UINT32 i=0; i < Channel::NumSubchannels; i++)
    {
        UINT32 subchNum = (i + m_SubchNumBase + 1) % Channel::NumSubchannels;
        m_AvailableSubchNum.push_back(subchNum);
    }

    // Remove allocated channel Id from pool
    lwgpu->RemoveChIdFromPool(make_pair(GetLwRmPtr(), engineId), ChannelNum());
    return rc;
}

/* static */ RC LWGpuChannel::AllocErrorNotifierRC
(
    LWGpuResource *lwgpu,
    const ArgReader *params,
    Surface2D *pErrorNotifier,
    LwRm* pLwRm
)
{
    MASSERT(lwgpu);
    MASSERT(pErrorNotifier);
    RC rc;

    if (params)
    {
        pErrorNotifier->SetLocation(Memory::Coherent);
        ParseDmaBufferArgs(*pErrorNotifier, params, "err", 0, 0);
    }

    pErrorNotifier->SetArrayPitch(16);
    pErrorNotifier->SetColorFormat(ColorUtils::Y8);
    pErrorNotifier->SetForceSizeAlloc(true);
    if (Platform::GetSimulationMode() == Platform::Amodel)
    {
        pErrorNotifier->SetLocation( Memory::Fb );
    }
    pErrorNotifier->SetAddressModel(Memory::Segmentation);
    pErrorNotifier->SetKernelMapping(true);

    if (pErrorNotifier->GetLocation() == Memory::Coherent)
    {
        pErrorNotifier->SetGpuCacheMode(Surface2D::GpuCacheOff);
        pErrorNotifier->SetP2PGpuCacheMode(Surface2D::GpuCacheOff);
    }

    // The error notifier should not be GPU cached by default.
    else if (pErrorNotifier->GetGpuCacheMode() == Surface2D::GpuCacheDefault)
    {
        pErrorNotifier->SetGpuCacheMode(Surface2D::GpuCacheOff);
    }

    CHECK_RC(pErrorNotifier->Alloc(lwgpu->GetGpuDevice(), pLwRm));
    PrintDmaBufferParams(*pErrorNotifier);
    CHECK_RC(pErrorNotifier->Map(0));

    return rc;
}

int LWGpuChannel::DestroyPushBuffer()
{
    CheckScheduleStatus();

    if (lwgpu && m_pCh)
    {
        // recycle channel id
        lwgpu->AddChIdToPool(make_pair(GetLwRmPtr(), GetEngineId()), ChannelNum());
    }

    if (m_hCh)
    {
        m_pLwRm->Free(m_hCh);
        m_pCh = NULL;
        m_hCh = 0;
    }
    else
    {
        if (m_pCh)
        {
            MASSERT(!"Channel handle m_hCh is NULL but its peer "
                "Channel* m_pCh isn't. Please check the case.\n");
        }
    }

    m_Pushbuf.Free();
    m_GpFifo.Free();
    m_Err.Free();

    return 1;
}

void LWGpuChannel::SetUpdateType(UpdateType t)
{
    m_UpdateType = t;
    if (m_pCh)
    {
        m_pCh->SetAutoFlush(m_UpdateType == AUTOMATIC, m_KickoffThreshold);
        m_pCh->SetAutoGpEntry(m_AutoGpEntryEnable, m_AutoGpEntryThreshold);
    }
}

int LWGpuChannel::GetLwrrentDput()
{
    UINT32 PutPtr;
    if (!m_pCh)
    {
        // Hack -- required by HIC
        return 0;
    }
    RC rc = m_pCh->GetPut(&PutPtr);
    if (rc != OK)
    {
        ErrPrintf("LWGpuChannel::GetLwrrentDput error: %s\n",
               rc.Message());
        return 0;
    }
    return PutPtr;
}

void LWGpuChannel::SetLwrrentDput(int new_lwrrent_dput)
{
    RC rc = m_pCh->SetPut(new_lwrrent_dput);
    if (rc != OK)
    {
        ErrPrintf("LWGpuChannel::GetLwrrentDput error: %s\n",
               rc.Message());
    }
}

int LWGpuChannel::SetDput(int new_dput)
{
    // This appears to be the equivalent of a SetLwrrentDput followed by a
    // MethodFlush.  Of course, many of the tests that call this do something
    // like "ch->SetDput(ch->GetLwrrentDput())", which is a bit silly.  I'll
    // implement it and hopefully we can clean this up later.
    SetLwrrentDput(new_dput);
    return MethodFlush();
}

UINT32 LWGpuChannel::GetMinDGet(int ch_num, int subch)
{
    // Warning: this is a dangerous API for multichip.  There is no canonical
    // ordering of Get pointers that is safe for all usage cases.
    UINT32 GetPtr;
    RC rc = m_pCh->GetGet(&GetPtr);
    if (rc != OK)
    {
        ErrPrintf("LWGpuChannel::GetMinDGet error: %s\n",
               rc.Message());
        return 0;
    }
    return GetPtr;
}

static TeeModule s_moduleTest("Mods_CtxSwInfo");
static UINT32 GetTeeModuleTestCode() { return s_moduleTest.GetCode(); }

void LWGpuChannel::initCtxSw()
{
    ctxswGran = -1;
    ctxswNum = 0;
    ctxswNumReplay = 0;
    ctxswIndex = 0;
    ctxswCount = 0;
    ctxswCountPrev = 0;
    ctxswPending = 0;
    ctxswSelf = 0;
    ctxswSerial = 0;
    ctxswReset = 0;
    ctxswResetOnly = 0;
    ctxswBash = false;
    ctxswWFIOnly = false;
    ctxswStopPull = false;

    ctxswLog = 0;
    ctxswSingleStep = 0;
    ctxswDebug = -1;
    ctxswType = 0;

    // XXXmjc ACK!!!  mpegbase seems to rely on this being uninitialized to a
    // nonzero value.  Otherwise, the first MethodWrite asserts when the context
    // switch mutex is released without ever having been acquired.  Don't initialize
    // to zero; make sure this is set to a nonzero value.
    ctxswTimeSlice = 1;

    // initialize the random stream to something in case the test never sets the seeds
    pRandomStream.reset(new RandomStream(0x1111, 0x2222, 0x3333));

    ctxswSuspended = false;
    ctxswPointsBase = 0;
}

void LWGpuChannel::SetCtxSwPoints(const vector<int>* pPoints)
{
    if (NULL != pPoints)
    {
        vector<int>::const_iterator it;
        for (it = pPoints->begin(); it !=  pPoints->end(); it++)
        {
            ctxswNum++;
            ctxswPoints.push_back(*it);

            InfoPrintf(GetTeeModuleTestCode(),
                "Context Switch Point added (specified by user) %d: %d\n",
                ctxswNum-1, ctxswPoints.back());
        }
    }
}

void LWGpuChannel::SetCtxSwPercents(const vector<UINT32>* pPercents)
{
    if (NULL != pPercents)
    {
        vector<UINT32>::const_iterator it;
        for (it = pPercents->begin(); it != pPercents->end(); it++)
        {
            ctxswNum++;
            ctxswPercents.push_back(*it);

            InfoPrintf(GetTeeModuleTestCode(),
                "Context Switch Percent added (specified by user) %d: %d%%\n",
                ctxswNum-1, ctxswPercents.back());
        }
    }
}

void LWGpuChannel::SetCtxSwGran(int gran)
{
    ctxswGran = gran;
}

void LWGpuChannel::SetCtxSwNum(int num)
{
    ctxswNum = num;
}

void LWGpuChannel::SetCtxSwNumReplay(int num)
{
    ctxswNumReplay = num;
}

void LWGpuChannel::SetLimiterFileName(const char * pName)
{
    LimiterFileName = pName;
}

void LWGpuChannel::SetCtxSwSeeds(int seed0, int seed1, int seed2)
{
    InfoPrintf(GetTeeModuleTestCode(),
        "Setting Context Switch Random Stream Seeds to 0x%x, 0x%x, 0x%x\n",
        seed0, seed1, seed2);
    pRandomStream.reset(new RandomStream(seed0, seed1, seed2));
}

void LWGpuChannel::SetMethodCount(int n)
{
    InfoPrintf(GetTeeModuleTestCode(),
        "Test w/ %d methods estimated in total\n", n);
    methodCount = n;

    MASSERT((int)ctxswPointsBase < methodCount);

    if (!ctxswPercents.empty())
    {
        vector<UINT32>::iterator it;
        for (it = ctxswPercents.begin(); it !=  ctxswPercents.end(); it++)
        {
            // colwert the percentage to method count
            float percent = (*it) / 100.0f;
            int point = (int)((methodCount - ctxswPointsBase) * percent + 0.5);
            ctxswPoints.push_back(point);
        }

        ctxswPercents.clear(); // release memory
    }

    if (!ctxswPoints.empty())
    {
        // sort the list of context switch points
        sort(ctxswPoints.begin(), ctxswPoints.end());
        vector<int>::iterator it = ctxswPoints.begin();
        for (; it !=  ctxswPoints.end(); it++)
        {
            *it += ctxswPointsBase;

            if (*it < 0)
            {
                MASSERT(!"Negtive ctxsw point! Please check -ctxswPoints!");
                *it = 0;
            }
        }

        // set the last context switch point greater than
        // the number of methods so we never hit it
        ctxswPoints.push_back(ctxswPoints.back() + 99999999);
        return;
    }

    if (!ctxswMethods.empty())
    {
        return;
    }

    // set up context switch points if user has requested a specific number of context switches
    if (ctxswNum > 0)
    {
        ctxswPoints.clear();

        for (int i=0; i < ctxswNum; i++)
        {
            ctxswPoints.push_back(
                pRandomStream->RandomUnsigned(ctxswPointsBase, methodCount-1));
        }

        // sort the list of context switch points
        sort(ctxswPoints.begin(), ctxswPoints.end());

        // set the last context switch point greater then the number of methods so we never hit it
        ctxswPoints.push_back(methodCount + 99999999);
        ctxswIndex = 0;
        ctxswCount = 0;
        ctxswCountPrev = 0;
        ctxswPending = 0;
    }
}

void LWGpuChannel::SetMethodWriteCount(int n)
{
    InfoPrintf(GetTeeModuleTestCode(),
        "Test w/ %d method writes estimated in total\n", n);
    methodWriteCount = n;
}

void LWGpuChannel::SetCtxSwMethods(vector<string>* methodList)
{
    if (methodList == NULL || methodList->empty())
        return;

    IRegisterMap* regmap = lwgpu->GetRegisterMap();
    if (!regmap)
    {
        WarnPrintf("SetCtxSwMethods: gpu has no RegisterMap\n");
        return;
    }

    vector<string>::iterator lwrMethod = methodList->begin();
    while (lwrMethod != methodList->end())
    {
        UINT32 methodBase;
        const char* newStr = lwrMethod->c_str();

        unique_ptr<IRegisterClass> reg = regmap->FindRegister(newStr);
        if (!reg)
        {
            InfoPrintf(GetTeeModuleTestCode(),
                "register by name %s not found\n", newStr);
            lwrMethod++;
            continue;
        }
        else
        {
            InfoPrintf(GetTeeModuleTestCode(),
                "adding ctxsw event on %s\n", newStr);
        }

        methodBase = reg->GetAddress();
        methodBase = REF_VAL(LW_FIFO_DMA_METHOD_ADDRESS_OLD, methodBase) << 2;
        UINT32 limit1, limit2, size1, size2;
        reg->GetFormula2(limit1, limit2, size1, size2);
        UINT32 methodEnd = methodBase + limit1 * size1 + limit2 * size2;

        ctxswMethods.push_back(AddrRange(lwrMethod->c_str(), methodBase, methodEnd));

        lwrMethod++;
    }
}

bool LWGpuChannel::isCtxSwEnabled()
{
    return (!ctxswSuspended &&
            m_InsertNestingLevel == 0 &&
            (ctxswGran != -1 || ctxswNum > 0));
}

void LWGpuChannel::setCtxSwDisabled()
{
    ctxswGran = -1;
    ctxswNum = 0;
}

bool LWGpuChannel::GetEnabled() const
{
    return m_Enabled;
}

void LWGpuChannel::StopSendingMethod()
{
    if (m_pCh)
    {
        if (m_pNullChannel == nullptr)
        {
            m_pNullChannel = new NullChannel;
        }
        m_pCh = m_pNullChannel;
    }
}

void LWGpuChannel::SetEnabled(bool enabled)
{
    m_Enabled = enabled;

    // Point m_pCh at the real channel if enabled, or a null channel
    // if disabled
    //
    if (m_pCh)
    {
        if (m_Enabled)
        {
            m_pCh = m_pLwRm->GetChannel(m_hCh);
        }
        else
        {
            StopSendingMethod();
        }
    }
}

// Return the method address for an index in the middle of a write.
//
/* static */ UINT32 LWGpuChannel::GetMethodAddr
(
    UINT32 methodStart,
    UINT32 index,
    MethodType mode
)
{
    switch (mode)
    {
        case INCREMENTING:
            return methodStart + 4 * index;

        case INCREMENTING_ONCE:
            return (index ? methodStart + 4 : methodStart);

        case NON_INCREMENTING:
        case INCREMENTING_IMM:
            return methodStart;

        default:
            MASSERT(!"Illegal case in GetMethodAddr");
            return methodStart;
    }
}

// Return the method data for an index in the middle of a write.
//
/* static */ const UINT32* LWGpuChannel::GetMethodData
(
    const UINT32 *pData,
    UINT32 index,
    MethodType mode,
    UINT32 count,
    UINT32* pDataSize
)
{
    if (index >= count)
    {
        *pDataSize = 0;
        return 0;
    }

    switch (mode)
    {
        case INCREMENTING:
            *pDataSize = 1;
            return pData + index;

        case INCREMENTING_ONCE:
            if (index)
            {
                *pDataSize = count - 1;
                return pData + 1;
            }
            else
            {
                *pDataSize = 1;
                return pData;
            }

        case NON_INCREMENTING:
        case INCREMENTING_IMM:
            *pDataSize = count;
            return pData;

        default:
            MASSERT(!"Illegal case in GetMethodAddr");
            *pDataSize = 1;
            return pData;
    }
}

// Called from MethodWriteN_RC() to decide how many methods to write.
//
// If we'll context-switch during the write, then the return value
// will equal the number of methods (possibly zero) to write before
// the ctxSwitch, and *pCtxSwitchAfterWrite will be set true.
// Otherwise, the return value will be the same as the "count" arg,
// and *pCtxSwitchAfterWrite will be false.
//
UINT32 LWGpuChannel::GetWriteCount
(
    int subchannel,
    UINT32 methodStart,
    UINT32 count,
    const UINT32 *pData,
    MethodType mode,
    bool *pCtxSwitchAfterWrite
)
{
    MASSERT(pCtxSwitchAfterWrite != NULL);
    *pCtxSwitchAfterWrite = false;

    if (!isCtxSwEnabled())
    {
        // No context switching:  Do the full write
        //
        return count;
    }
    else if (m_pCh->GetAtomChannelWrapper()->GetPeekableFsm())
    {
        AtomFsm atomFsm = *m_pCh->GetAtomChannelWrapper()->GetPeekableFsm();
        for (UINT32 index = 0; index < count; ++index)
        {
            UINT32 methodAddr = GetMethodAddr(methodStart, index, mode);
            if (ctxswPending > 0 && ctxswCount > ctxswCountPrev)
            {
                atomFsm.PeekMethod(subchannel, methodAddr, pData[index]);
                if (!atomFsm.InAtom())
                {
                    --ctxswPending;
                    InfoPrintf(GetTeeModuleTestCode(),
                        "%s Replay pending ctxsw Event. Pending ctxsw count = %d "
                        "Method to be written = 0x%x\n", __FUNCTION__,
                        ctxswPending, methodAddr);

                    ctxswCountPrev = ctxswCount;
                    *pCtxSwitchAfterWrite = true;
                    return index;
                }
            }
            atomFsm.EatMethod(subchannel, methodAddr, pData[index]);

            if (hasCtxSwEvent(subchannel, methodStart, index, count, pData, mode))
            {
                if (atomFsm.InAtom())
                {
                    ++ctxswPending;
                    InfoPrintf(GetTeeModuleTestCode(),
                        "%s Ctxsw event is being triggered. Delay the ctxsw Event "
                        " due to atomic method sequence. Pending ctxsw count = %d "
                        "Method to be written = 0x%x\n",
                        __FUNCTION__, ctxswPending, methodAddr);
                }
                else
                {
                    // because the switch happens after write,
                    // the trigger method should be counted as well
                    ++ctxswCount;

                    ctxswCountPrev = ctxswCount;
                    *pCtxSwitchAfterWrite = true;
                    return index + 1;
                }
            }
            else
            {
                ++ctxswCount;
            }
        }
    }
    else if (m_pCh->GetRunlistChannelWrapper() == NULL)
    {
        // Context switching, round-robin mode: Call hasCtxSwEvent() on
        // each method to find the next ctxSwitch
        //
        for (UINT32 index = 0; index < count; ++index)
        {
            if (hasCtxSwEvent(subchannel, methodStart, index, count, pData, mode))
            {
                // because the switch happens after write,
                // the trigger method should be counted as well
                ++ctxswCount;

                ctxswCountPrev = ctxswCount;
                *pCtxSwitchAfterWrite = true;

                WarnPrintf("AtomicChannelWrapper is not enabled! ctxswEvent may break"
                    "the atomic sequence to lead to FE exception\n");
                return index + 1;
            }
            ++ctxswCount;
        }
        return count;
    }
    else
    {
        // Context switching, runlist mode, the atom state is too
        // clogged for us to do any ctxSwitching on this write: Call
        // hasCtxSwEvent() on each method, but only to count how many
        // switches we skipped
        //
        for (UINT32 index = 0; index < count; ++index)
        {
            if (hasCtxSwEvent(subchannel, methodStart, index, count, pData, mode))
                ++ctxswPending;
            else
                ++ctxswCount;
        }
        return count;
    }

    return count;
}

bool LWGpuChannel::hasCtxSwEvent
(
    int subchannel,
    UINT32 methodStart,
    UINT32 index,
    UINT32 count,
    const UINT32 *pData,
    MethodType mode
)
{
    if (ctxswTimeSlice)
        return false;

    UINT32 dataSize = 0;
    UINT32 methodAddr = GetMethodAddr(methodStart, index, mode);
    const UINT32* pMethodData = GetMethodData(pData, index, mode, count, &dataSize);

    if (!ctxswMethods.empty())
    {
        bool ret = false;
        UINT32 i;
        // return of true on i=0 means we're ctxsw'ing after the method
        //                on i=1 means we're ctxsw'ing before the method
        for (i = 0; i < 2; i++)
        {
            vector<AddrRange>::iterator lwrRange = ctxswMethods.begin();
            while (lwrRange != ctxswMethods.end())
            {
                if (lwrRange->InRange(methodAddr))
                {
                    if (ctxswNum && (lwrRange->GetCount() < ctxswNum))
                    {
                        char tmpstr[80];
                        tmpstr[0] = 0;
                        if (!ret && ctxswNum)
                        {
                                lwrRange->IncrCount();
                                sprintf(tmpstr, "%d/%d ", lwrRange->GetCount(), ctxswNum);
                        }
                        InfoPrintf(GetTeeModuleTestCode(),
                                "Context Switch Forced %s method write #%d, %sin range of %s away from channel #%d\n",
                                i ? "before" : "after", ctxswCount+i, tmpstr, lwrRange->GetName().c_str(),ChannelNum());
                        // like this we mention & account for both the before and after method
                        // if we get a hit on the after method;

                        // print the data followed
                        if (pMethodData)
                        {
                            const UINT32 MAX_DATACN_TO_PRINT = 8;
                            InfoPrintf(GetTeeModuleTestCode(), "Method Data:");
                            for (UINT32 idx = 0; idx < dataSize; idx++)
                            {
                                if (idx >= MAX_DATACN_TO_PRINT)
                                    break;

                                RawPrintf(GetTeeModuleTestCode(), "0x%08x  ", pMethodData[idx]);
                            }
                            RawPrintf(GetTeeModuleTestCode(), "%s\n",
                                dataSize > MAX_DATACN_TO_PRINT?"...":"");
                        }

                        ret = true;
                        break;
                    }
                }
                lwrRange++;
            }
            index++;
            // check if we're going out of range
            if (index >= count)
                break;
        }
        return ret;
    }

    // the user has specified a specific number of context switches
    if (ctxswNum > 0)
    {
        if (methodCount == 0)
        {
            ErrPrintf("ERROR! The command line has specified the number of context switches, but this test\n");
            ErrPrintf("has not reported the total number of methods.  You must add this line:\n");
            ErrPrintf("ch->SetMethodCount(nummethods);\n");
            ErrPrintf("to this test if you want to run it while specifing the number of context switches\n");
            return false;
        }

        // if we have reached the next context switch point, bump the index
        if (ctxswPoints[ctxswIndex] <= ctxswCount)
        {
            ctxswIndex++;
            InfoPrintf(GetTeeModuleTestCode(),
                       "Context Switch Forced after method 0x%04x on subch %d, "
                       "write #%d (%d/%d) away from channel #%d\n",
                       methodAddr, subchannel, ctxswCount,
                       ctxswIndex, ctxswNum,ChannelNum());

            // print the data followed
            if (pMethodData)
            {
                const UINT32 MAX_DATACN_TO_PRINT = 8;
                InfoPrintf(GetTeeModuleTestCode(), "Method Data:");
                for (UINT32 idx = 0; idx < dataSize; idx++)
                {
                    if (idx >= MAX_DATACN_TO_PRINT)
                        break;

                    RawPrintf(GetTeeModuleTestCode(), "0x%08x  ", pMethodData[idx]);
                }
                RawPrintf(GetTeeModuleTestCode(), "%s\n",
                    dataSize > MAX_DATACN_TO_PRINT?"...":"");
            }

            return true;
        }
        else
        {
            return false;
        }
    }

    if (ctxswGran == 0 || ctxswGran == 1)
        return true;

    // user has selected a context switch every n methods (on average)
    return (pRandomStream->RandomUnsigned(1, ctxswGran) == 1);
}

void LWGpuChannel::SetCtxSwSerial()
{
    ctxswSerial = 1;
}

void LWGpuChannel::SetCtxSwSelf()
{
    ctxswSelf = 1;
}

void LWGpuChannel::SetCtxSwBash(bool val)
{
    ctxswBash = val;
}

void LWGpuChannel::SetCtxSwReset(int val)
{
    ctxswReset = val;
}

void LWGpuChannel::SetCtxSwLog(int val)
{
    ctxswLog = val;
}

void LWGpuChannel::SetCtxSwSingleStep()
{
    ctxswSingleStep = 1;
}

void LWGpuChannel::SetCtxSwDebug(int num)
{
    ctxswDebug = num;
}

void LWGpuChannel::SetCtxSwType(int val)
{
    ctxswType = val;
}

void LWGpuChannel::SetCtxSwResetMask(int val)
{
    ctxswResetMask[0] = val;
}

void LWGpuChannel::SetCtxSwResetOnly(int val)
{
    MASSERT(!val); // ctxswResetOnly was never implemented in MODS
    ctxswResetOnly = val;
}

void LWGpuChannel::SetCtxSwWFIOnly(bool val)
{
    ctxswWFIOnly = val;
}

void LWGpuChannel::SetCtxSwStopPull(bool val)
{
    MASSERT(!val); // ctxswStopPull was never implemented in MODS
    ctxswStopPull = val;
}

void LWGpuChannel::SetCtxSwTimeSlice(int val)
{
    ctxswTimeSlice = val;
}

int LWGpuChannel::GetNumMethodsSent()
{
    return ctxswCount;
}

bool LWGpuChannel::SetSuspendCtxSw(bool val)
{
    bool bReturn = ctxswSuspended;
    ctxswSuspended = val;
    return bReturn;
}

void LWGpuChannel::SetGpFifoEntries(UINT32 val)
{
    m_GpFifoEntries = val;
    m_GpFifoEntriesFromTrace = true;
}

void LWGpuChannel::SetPushbufferSize(UINT64 val)
{
    m_Pushbuf.SetArrayPitch(val);
    m_PushbufferSizeFromTrace = true;
}

void LWGpuChannel::EnableLogging(bool value)
{
    m_pCh->EnableLogging(value);
}

RC LWGpuChannel::GetGpPut(UINT32 *GpPutPtr)
{
    return m_pCh->GetGpPut(GpPutPtr);
}

RC LWGpuChannel::GetGpGet(UINT32 *GpGetPtr)
{
    return m_pCh->GetGpGet(GpGetPtr);
}

RC LWGpuChannel::FinishOpenGpFifoEntry(UINT64* pEnd)
{
    return m_pCh->FinishOpenGpFifoEntry(pEnd);
}

RC LWGpuChannel::SetExternalGPPutAdvanceMode(bool enable)
{
    return m_pCh->SetExternalGPPutAdvanceMode(enable);
}

FLOAT64 LWGpuChannel::GetDefaultTimeoutMs()
{
    return m_pCh->GetDefaultTimeoutMs();
}

LWGpuChannel::UpdateType LWGpuChannel::GetUpdateType()
{
    return m_UpdateType;
}

void LWGpuChannel::GetBuffInfo(BuffInfo* inf, const string& name)
{
    inf->AddEntry("InstMem(" + name + ')');
    inf->SetTarget(m_InstMemLoc);

    inf->AddEntry("RAMFC(" + name + ')');
    inf->SetTarget(m_RAMFCMemLoc);

    inf->SetDmaBuff("PushBuff(" + name + ')', m_Pushbuf, 0, 0);
    inf->SetDmaBuff("GpFifo(" + name + ')', m_GpFifo, 0, 0);
    if (!m_pLWGpuTsg)
    {
        // Error notifier is not allocated in TSG mode.
        inf->SetDmaBuff("Err(" + name + ')', m_Err, 0, 0);
    }

    if (GetPreemptive())
    {
        inf->SetDmaBuff("PreempCtxBuf(" + name + ')', m_PreemptCtxBuf, 0, 0);
    }
}

void LWGpuChannel::CheckScheduleStatus()
{
    if (m_MethodWritten && !m_Scheduled) // see bug 861192
    {
        WarnPrintf("LWGpuChannel has never been scheduled and might "
                   "still be active. Please call SetObject or "
                   "ScheduleChannel first.\n");
    }
}

bool LWGpuChannel::UseFixedSubChannelNum(LwRm::Handle objHandle) const
{
    //
    // KEPLER_CHANNEL_GPFIFO_A and later
    // Kepler channel need fixed subch number. Details in AllocSubChannelNum()
    //
    if (EngineClasses::IsGpuFamilyClassOrLater(
        m_HwChannelClass,LWGpuClasses::GPU_CLASS_KEPLER))
    {
        return true;
    }

    return false;
}

RC LWGpuChannel::AllocSubChannelNum
(
    UINT32 classId,
    LwRm::Handle objHandle,
    UINT32* pRtnSubchNum
)
{
    // Check whether using the subchannel number specified in trace
    bool bUseTraceSubchNum = false;
    UINT32 trSubchNum = 0;
    map<UINT32, UINT32>::iterator it_trSubchNum = m_TraceSpecifiedSubchNum.find(classId);
    if (it_trSubchNum != m_TraceSpecifiedSubchNum.end())
    {
        trSubchNum = it_trSubchNum->second;
        bUseTraceSubchNum = true;
    }

    // allocate subch number
    UINT32 subchNum = Channel::NumSubchannels - 1;
    if (UseFixedSubChannelNum(objHandle))
    {
        // Graphics Runlist:
        // LWA06F_SUBCHANNEL_3D          = 3d
        // LWA06F_SUBCHANNEL_COMPUTE     = compute
        // LWA06F_SUBCHANNEL_I2M         = i2m
        // LWA06F_SUBCHANNEL_2D          = 2d
        // LWA06F_SUBCHANNEL_COPY_ENGINE = graphic copy engine
        // 5-7                           = SW engines
        //
        // All other runlists:
        // 0-4:                          = HW engines
        // 5-7:                          = SW engines

        UINT32 engineType;
        if (OK != GetEngineTypeByHandle(objHandle, &engineType))
        {
            ErrPrintf("%s: Can't find enginetype\n", __FUNCTION__);
            return RC::SOFTWARE_ERROR;
        }

        UINT32 rangeMin = 0; // subch number range [Min, Max]
        UINT32 rangeMax = 0;
        if (engineType == LW2080_ENGINE_TYPE_SW)
        {
            // 5 - 7 for SW engines
            rangeMin = 5;
            rangeMax = 7;
        }
        else
        {
            if (EngineClasses::IsClassType("Ce", classId) ||
                classId == GF106_DMA_DECOMPRESS)
            {
                UINT32 grCopyEngineType;
                if (lwgpu->GetGrCopyEngineType(&grCopyEngineType, GetPbdma(), m_pLwRm) == OK &&
                    engineType == grCopyEngineType)
                {
                    // graphic copy engine
                    rangeMin = rangeMax = LWA06F_SUBCHANNEL_COPY_ENGINE;
                }
                else
                {
                    rangeMin = 0;
                    rangeMax = 4;
                } 
            }
            else if (EngineClasses::IsClassType("Compute", classId))
            {
                rangeMin = rangeMax = LWA06F_SUBCHANNEL_COMPUTE;
            }
            else if (EngineClasses::IsClassType("Gr", classId))
            {
                rangeMin = rangeMax = LWA06F_SUBCHANNEL_3D;
            }
            else
            {
                switch(classId)
                {
                    case KEPLER_INLINE_TO_MEMORY_B:
                        rangeMin = rangeMax = LWA06F_SUBCHANNEL_I2M;
                        break;
                    case FERMI_TWOD_A:
                        rangeMin = rangeMax = LWA06F_SUBCHANNEL_2D;
                        break;
                    default:
                        // Other HW Engines
                        rangeMin = 0;
                        rangeMax = 4;
                }
            }
        }

        if (bUseTraceSubchNum)
        {
            if (trSubchNum < rangeMin || trSubchNum > rangeMax)
            {
                WarnPrintf("%s: subchNum(%d) specified in trace "
                    "is not valid for class 0x%x.\n",
                    __FUNCTION__, trSubchNum, classId);
            }

            // Bug 686479: trace want to use subch number 0 for compute
            // quick fix to unblock as2: ignore kepler subch number mapping rule
            rangeMin = rangeMax = trSubchNum;
        }

        vector<UINT32>::iterator it = m_AvailableSubchNum.begin();
        for (; it != m_AvailableSubchNum.end(); it++)
        {
            if (*it >= rangeMin && *it <= rangeMax)
            {
                subchNum = *it;
                break;
            }
        }

        if (it == m_AvailableSubchNum.end())
        {
            ErrPrintf("error: %s: subchNum(%d) to be allocated "
                    "is not available.\n", __FUNCTION__, subchNum);
            return RC::SOFTWARE_ERROR;
        }
        else
        {
            m_AvailableSubchNum.erase(it);
        }
    }
    else
    {
        // choose one from available subchannel number
        if (m_AvailableSubchNum.empty())
        {
            ErrPrintf("error: %s: No availabe subchNum. Max num of subch "
                    "is %d.\n", __FUNCTION__, Channel::NumSubchannels);
            return RC::SOFTWARE_ERROR;
        }

        if (bUseTraceSubchNum)
        {
            vector<UINT32>::iterator it = find(m_AvailableSubchNum.begin(),
                                               m_AvailableSubchNum.end(),
                                               trSubchNum);
            if (it == m_AvailableSubchNum.end())
            {
                ErrPrintf("error: %s: subchNum(%d) specified in tace "
                    "is not available.\n", __FUNCTION__, trSubchNum);
                return RC::SOFTWARE_ERROR;
            }

            subchNum = *it;
            m_AvailableSubchNum.erase(it);
        }
        else
        {
            subchNum = m_AvailableSubchNum.back();
            m_AvailableSubchNum.pop_back();
        }
    }

    *pRtnSubchNum = subchNum;
    return OK;
}

RC LWGpuChannel::GetSubChannelNumClass(UINT32 Handle,
                                       UINT32* pRtnSubchNum,
                                       UINT32* pRtnSubchClass) const
{
    map<UINT32, pair<UINT32, UINT32> >::const_iterator cit;
    cit = m_ObjHandle2SubchNumClassMap.find(Handle);
    if (cit == m_ObjHandle2SubchNumClassMap.end())
    {
        return RC::SOFTWARE_ERROR;
    }

    if (pRtnSubchNum != 0)
        *pRtnSubchNum = cit->second.first;

    if (pRtnSubchClass != 0)
        *pRtnSubchClass = cit->second.second;
    return OK;
}

RC LWGpuChannel::GetSubChannelHandle(UINT32 Class, UINT32* pRtnHandle) const
{
    map<UINT32, pair<UINT32, UINT32> >::const_iterator cit;
    cit = m_ObjHandle2SubchNumClassMap.begin();
    for (; cit != m_ObjHandle2SubchNumClassMap.end(); cit++)
    {
        UINT32 classNum = cit->second.second;
        if (classNum == Class)
        {
            *pRtnHandle = cit->first;
            return OK;
        }
    }

    return RC::SOFTWARE_ERROR;
}

void LWGpuChannel::SetTraceSubchNum(UINT32 classId, UINT32 traceSubchNum)
{
    // use the subchannel number specified in trace
    // instead of allocating one from subchnum pool
    m_TraceSpecifiedSubchNum[classId] = traceSubchNum;
}

RC LWGpuChannel::GetSubChannelClass(UINT32 subChannelNum, UINT32* pClassNum)
{
    for (auto const & subChannelData : m_ObjHandle2SubchNumClassMap)
    {
        if (subChannelData.second.first == subChannelNum)
        {
            *pClassNum = subChannelData.second.second;
            return OK;
        }
    }

    return RC::SOFTWARE_ERROR;
}

bool LWGpuChannel::HasGrEngineObjectCreated(bool inTsgScope) const
{
    // search graphic object in tsg scope
    if (inTsgScope && m_pLWGpuTsg)
    {
        return m_pLWGpuTsg->HasGrEngineObjectCreated();
    }

    // scan all handles in channel to return whether graphic engine ilwolved
    //
    map<UINT32, pair<UINT32, UINT32> >::const_iterator cit;
    cit = m_ObjHandle2SubchNumClassMap.begin();
    for (; cit != m_ObjHandle2SubchNumClassMap.end(); cit++)
    {
        UINT32 engineType = 0;
        if (GetEngineTypeByHandle(cit->first, &engineType) == OK &&
            MDiagUtils::IsGraphicsEngine(engineType))
        {
            return true;
        }
    }

    return false;
}

bool LWGpuChannel::HasVideoEngineObjectCreated(bool inTsgScope) const
{
    // search video object in tsg scope
    if (inTsgScope && m_pLWGpuTsg)
    {
        return m_pLWGpuTsg->HasVideoEngineObjectCreated();
    }

    // scan all handles in channel to return whether copy engine ilwolved
    //
    map<UINT32, pair<UINT32, UINT32> >::const_iterator cit;
    cit = m_ObjHandle2SubchNumClassMap.begin();
    for (; cit != m_ObjHandle2SubchNumClassMap.end(); cit++)
    {
        UINT32 engineType = 0;
        if (GetEngineTypeByHandle(cit->first, &engineType) == OK &&
            (engineType == LW2080_ENGINE_TYPE_VP     ||
             engineType == LW2080_ENGINE_TYPE_PPP    ||
             MDiagUtils::IsLwDecEngine(engineType)   ||
             MDiagUtils::IsLwEncEngine(engineType)   ||
             engineType == LW2080_ENGINE_TYPE_MPEG   ||
             engineType == LW2080_ENGINE_TYPE_CIPHER ||
             engineType == LW2080_ENGINE_TYPE_VIC))
        {
            return true;
        }
    }

    return false;
}

bool LWGpuChannel::HasCopyEngineObjectCreated(bool inTsgScope) const
{
    // search ce object in tsg scope
    if (inTsgScope && m_pLWGpuTsg)
    {
        return m_pLWGpuTsg->HasCopyEngineObjectCreated();
    }

    // scan all handles in channel to return whether copy engine ilwolved
    //
    map<UINT32, pair<UINT32, UINT32> >::const_iterator cit;
    cit = m_ObjHandle2SubchNumClassMap.begin();
    for (; cit != m_ObjHandle2SubchNumClassMap.end(); cit++)
    {
        UINT32 engineType = 0;
        if (GetEngineTypeByHandle(cit->first, &engineType) == OK &&
            MDiagUtils::IsCopyEngine(engineType))
        {
            return true;
        }
    }

    return false;
}

// SCG type may comes from TraceChannel
RC LWGpuChannel::SetSCGType(SCGType type)
{
    m_SCGType = type;
    return OK;
}

RC LWGpuChannel::SetCEPrefetch(bool enable)
{
    m_CEPrefetchEnable = enable;
    return OK;
}

void LWGpuChannel::SetPbdma(UINT32 pbdma)
{
    m_Pbdma = pbdma;
}

void LWGpuChannel::SetVASpace(const shared_ptr<VaSpace> &pVaSpace)
{
    m_pVaSpace = pVaSpace;
    m_hVASpace = m_pVaSpace->GetHandle();
}

RC LWGpuChannel::SetupCtxswPm()
{
    RC rc = OK;

    if (HasSetCtxswPmDone())
    {
        // nothing to do because it has been done
        return rc;
    }

    if (!HasGrEngineObjectCreated(true))
    {
        InfoPrintf("%s: Skip setting LW2080_CTRL_CTXSW_PM_MODE_CTXSW "
            "for channel(chid=%d) because it's not a channel on GR engine\n",
            __FUNCTION__, m_pCh->GetChannelId());
        return rc;
    }

    UINT32 pmHandle = 0;
    if (m_pLWGpuTsg)
    {
        pmHandle = m_pLWGpuTsg->GetHandle();
    }
    else
    {
        pmHandle = m_pCh->GetHandle();
    }

    LW2080_CTRL_GR_CTXSW_PM_MODE_PARAMS grCtxswPmModeParams = {0};
    grCtxswPmModeParams.hChannel = pmHandle;
    grCtxswPmModeParams.pmMode = LW2080_CTRL_CTXSW_PM_MODE_CTXSW;


    rc = m_pLwRm->Control(m_pLwRm->GetSubdeviceHandle(lwgpu->GetGpuSubdevice()),
                        LW2080_CTRL_CMD_GR_CTXSW_PM_MODE,
                        (void*)&grCtxswPmModeParams,
                        sizeof(grCtxswPmModeParams));

    if (rc != OK)
    {
        ErrPrintf("%s: Error setting pm ctxsw mode in RM: %s\n",
                  __FUNCTION__, rc.Message());
        return rc;
    }

    // Set the flag to tag the channel
    m_SetCtxswPmModeDone = true;

    DebugPrintf("%s: Setting LW2080_CTRL_CTXSW_PM_MODE_CTXSW "
                "for channel(chid=%d) is done correctly.\n",
                __FUNCTION__, m_pCh->GetChannelId());

    return rc;
}

RC LWGpuChannel::SetupCtxswSmpc(SmpcCtxswMode smpcCtxswMode)
{
    RC rc = OK;

    if (HasSetCtxswSmpcDone())
    {
        // nothing to do because it has been done
        return rc;
    }

    if (!HasGrEngineObjectCreated(true))
    {
        InfoPrintf("%s: Skip setting LW2080_CTRL_CTXSW_SMPC_MODE_CTXSW "
            "for channel(chid=%d) because it's not a channel on GR engine\n",
            __FUNCTION__, m_pCh->GetChannelId());
        return rc;
    }

    UINT32 smpcHandle = 0;
    if (m_pLWGpuTsg)
    {
        smpcHandle = m_pLWGpuTsg->GetHandle();
    }
    else
    {
        smpcHandle = m_pCh->GetHandle();
    }

    LW2080_CTRL_GR_CTXSW_SMPC_MODE_PARAMS grCtxswSmpcModeParams = {0};
    grCtxswSmpcModeParams.hChannel = smpcHandle;
    if (smpcCtxswMode == SMPC_CTXSW_MODE_NO_CTXSW)
    {
        grCtxswSmpcModeParams.smpcMode = LW2080_CTRL_CTXSW_SMPC_MODE_NO_CTXSW;
    }
    else if (smpcCtxswMode == SMPC_CTXSW_MODE_CTXSW)
    {
        grCtxswSmpcModeParams.smpcMode = LW2080_CTRL_CTXSW_SMPC_MODE_CTXSW;
    }
    else
    {
        ErrPrintf("%s: Error setting SMPC ctxsw mode: "
            "Bad -smpc_ctxsw_mode argument %u\n", __FUNCTION__, smpcCtxswMode);
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }


    rc = m_pLwRm->Control(m_pLwRm->GetSubdeviceHandle(lwgpu->GetGpuSubdevice()),
                        LW2080_CTRL_CMD_GR_CTXSW_SMPC_MODE,
                        (void*)&grCtxswSmpcModeParams,
                        sizeof(grCtxswSmpcModeParams));

    if (rc != OK)
    {
        ErrPrintf("%s: Error setting SMPC ctxsw mode in RM: %s\n",
                  __FUNCTION__, rc.Message());
        return rc;
    }

    // Set the flag to tag the channel
    m_SetCtxswSmpcModeDone = true;

    DebugPrintf("%s: Successfully called LW2080_CTRL_CMD_GR_CTXSW_SMPC_MODE "
        "with mode=%u for channel(chid=%d).\n",
        __FUNCTION__, grCtxswSmpcModeParams.smpcMode, m_pCh->GetChannelId());

    return rc;
}

//
// Call RM api LWxxx_CTRL_GET_CLASS_ENGINEID to get enginetype by object handle.
//
// LWxxx_CTRL_GET_CLASS_ENGINEID is not supported on amodel,
// always return graphic engine
//
RC LWGpuChannel::GetEngineTypeByHandle
(
    LwRm::Handle objHandle,
    UINT32* pRtnEngineType
) const
{
    RC rc;

    // all classes are marked as graphics engine objects on amodel
    if (Platform::GetSimulationMode() == Platform::Amodel)
    {
        *pRtnEngineType = LW2080_ENGINE_TYPE_GRAPHICS;
        return OK;
    }

    // query RM to get the engine type
    CHECK_RC(m_pCh->RmcGetClassEngineId(objHandle, NULL, NULL,
                                        pRtnEngineType));
    return OK;
}

RC LWGpuChannel::InsertSemMethodsForNonblockingPoll()
{
    RC rc;

    //Initialize semaphore
    if (m_pSemaphoreForNonblockingPoll.get() == nullptr)
    {
        m_pSemaphoreForNonblockingPoll.reset(new MdiagSurf);

        m_pSemaphoreForNonblockingPoll->SetWidth(lwgpu->GetGpuDevice()->GetNumSubdevices() *
                                    NON_BLOCKING_SEM_SIZE);
        m_pSemaphoreForNonblockingPoll->SetHeight(1);
        m_pSemaphoreForNonblockingPoll->SetPageSize(4);
        m_pSemaphoreForNonblockingPoll->SetColorFormat(ColorUtils::Y8);
        m_pSemaphoreForNonblockingPoll->SetLocation(Memory::Coherent);
        m_pSemaphoreForNonblockingPoll->SetGpuVASpace(GetVASpaceHandle());
        m_pSemaphoreForNonblockingPoll->SetVASpace(Surface2D::GPUVASpace);
        m_pSemaphoreForNonblockingPoll->SetVaReverse(((GpuDevMgr*) DevMgrMgr::d_GraphDevMgr)->GetVasReverse());
        CHECK_RC(m_pSemaphoreForNonblockingPoll->Alloc(lwgpu->GetGpuDevice(), GetLwRmPtr()));
        CHECK_RC(m_pSemaphoreForNonblockingPoll->BindGpuChannel(ChannelHandle(), GetLwRmPtr()));
        CHECK_RC(m_pSemaphoreForNonblockingPoll->Map());
        CHECK_RC(m_pSemaphoreForNonblockingPoll->Fill(0));
    }

    //Insert nonblocking sem
    {
        CHECK_RC(BeginInsertedMethodsRC());
        Utility::CleanupOnReturn<LWGpuChannel>
            CleanupInsertedMethods(this, &LWGpuChannel::CancelInsertedMethods);
        UINT64 Offset = m_pSemaphoreForNonblockingPoll->GetCtxDmaOffsetGpu(GetLwRmPtr());
        m_PayloadForNonblockingPoll++;
        SetSemaphoreReleaseFlags(Channel::FlagSemRelDefault);
        for (UINT32 subdev =  0;
             subdev < lwgpu->GetGpuDevice()->GetNumSubdevices(); ++subdev)
        {
            if (lwgpu->GetGpuDevice()->GetNumSubdevices() > 1)
                CHECK_RC(WriteSetSubdevice(1 << subdev));
            CHECK_RC(SetSemaphoreOffsetRC(
                     Offset + subdev * NON_BLOCKING_SEM_SIZE));
            CHECK_RC(SemaphoreReleaseRC(m_PayloadForNonblockingPoll));
        }
        CleanupInsertedMethods.Cancel();
        CHECK_RC(EndInsertedMethodsRC());

        CHECK_RC(MethodFlushRC());
    }

    if (Xp::GetOperatingSystem() == Xp::OS_LINUXSIM ||
        Xp::GetOperatingSystem() == Xp::OS_WINSIM)
    {
        PHYSADDR semPhysAddr = 0;
        CHECK_RC(m_pSemaphoreForNonblockingPoll->GetPhysAddress(0, &semPhysAddr, GetLwRmPtr()));
        DebugPrintf("%s: Polling channel id %u semaphore address 0x%llx (virtual)"
                   " - 0x%llx (physical) for value %u\n",
                   __FUNCTION__, m_pCh->GetChannelId() , m_pSemaphoreForNonblockingPoll->GetCtxDmaOffsetGpu(),
                   semPhysAddr, m_PayloadForNonblockingPoll);
    }

    return rc;
}

bool LWGpuChannel::CheckSemaphorePayload(UINT32 payloadValue)
{
    GpuDevice *pDevice = lwgpu->GetGpuDevice();
    UINT08 *pSem = (UINT08*)m_pSemaphoreForNonblockingPoll->GetAddress();

    if (CheckForErrorsRC() != OK)
    {
        return true;
    }

    for (UINT32 subdev = 0; subdev < pDevice->GetNumSubdevices(); ++subdev)
    {
        // For non-blocking wfi, multi wfi traceops may run synchronously. 
        // When a wfi traceop succeeds in polling payload it required, 
        // the value in memory may have been updated to larger value.
        if (MEM_RD32((uintptr_t)pSem + subdev * NON_BLOCKING_SEM_SIZE) <
            payloadValue)
        {
            return false;
        }
    }

    return true;
}

// This function should be called just before test methods are written
// to the channel.  This is used to distinguish between test methods and
// MODS methods.
//
void LWGpuChannel::BeginTestMethods()
{
    ++m_SendingTestMethods;
}

// This function should be called in conjunction with BeginTestMethods,
// after all test methods have finished being written.
//
void LWGpuChannel::EndTestMethods()
{
    MASSERT(m_SendingTestMethods > 0);
    --m_SendingTestMethods;
}

void LWGpuChannel::SetAllocChIdMode(const ArgReader *params)
{
    m_AllocChIdMode = (AllocChIdMode)
        params->ParamUnsigned("-alloc_chid_mode", ALLOC_ID_MODE_DEFAULT);
}

UINT32 LWGpuChannel::GetVerifFlags2(UINT32 engineId, UINT32 channelClass)
{
    UINT32 verifFlags2 = 0;
#ifdef LW_VERIF_FEATURES
    switch(m_AllocChIdMode)
    {
        case ALLOC_ID_MODE_DEFAULT:
            // Non-SRIOV mode: use random channelIds
            // SRIOV mode: Let RM choose the ChannelId since vgpu plugin 
            // reserves some channelIds
            if (Platform::IsDefaultMode())
            {
                verifFlags2 |= DRF_DEF(OS04, _VERIF_FLAGS2, _ALLOC_ID_MODE, _PROVIDED);
                verifFlags2 |= DRF_NUM(OS04, _VERIF_FLAGS2, _ALLOC_ID, lwgpu->GetRandChIdFromPool(make_pair(GetLwRmPtr(), engineId)));
            }
            else
            {
                verifFlags2 |= DRF_DEF(OS04, _VERIF_FLAGS2, _ALLOC_ID_MODE, _DEFAULT);
            }
            break;
        case ALLOC_ID_MODE_GROWUP:
            verifFlags2 |= DRF_DEF(OS04, _VERIF_FLAGS2, _ALLOC_ID_MODE, _GROWUP);
            break;
        case ALLOC_ID_MODE_GROWDOWN:
            verifFlags2 |= DRF_DEF(OS04, _VERIF_FLAGS2, _ALLOC_ID_MODE, _GROWDOWN);
            break;
        case ALLOC_ID_MODE_RANDOM:
            verifFlags2 |= DRF_DEF(OS04, _VERIF_FLAGS2, _ALLOC_ID_MODE, _PROVIDED);
            verifFlags2 |= DRF_NUM(OS04, _VERIF_FLAGS2, _ALLOC_ID, lwgpu->GetRandChIdFromPool(make_pair(GetLwRmPtr(), engineId)));
            break;
        case ALLOC_ID_MODE_RM:
            verifFlags2 |= DRF_DEF(OS04, _VERIF_FLAGS2, _ALLOC_ID_MODE, _DEFAULT);
            break;
        default:
            MASSERT(!"Unkown alloc chid mode!\n");
    }
#endif
    return verifFlags2;
}

//
// class to represent creation parameter of ce object
//
CEObjCreateParam::CEObjCreateParam
(
    UINT32 type,
    UINT32 classId
):
    m_CeType(type),
    m_ClassId(classId)
{
    if (CE_INSTANCE_MAX - LAST_CE > 1)
    {
        WarnPrintf("%s: %d kinds of CE are supported!\n",
            __FUNCTION__, CE_INSTANCE_MAX - LAST_CE);
    }
}

CEObjCreateParam::~CEObjCreateParam()
{
}

// return parameter buffer RM needs according ce type
RC CEObjCreateParam::GetObjCreateParam
(
    LWGpuChannel* pLwGpuCh,
    void** ppRtnObjCreateParams,
    UINT32* pCeInstance
)
{
    if (!m_ClassParam.empty())
    {
        *ppRtnObjCreateParams = &m_ClassParam[0];
        return OK;
    }

    RC rc;
    UINT32 ceType = m_CeType;

    // select grcopy automatically for Kepler grchannel
    //
    if (EngineClasses::IsClassType("Ce", m_ClassId))
    {
        string className = EngineClasses::GetClassName(m_ClassId);
        const char* ceName = className.c_str();

        if (pLwGpuCh->HasGrEngineObjectCreated(true))
        {
            // In SMC mode, there is no GrCE
            if (pLwGpuCh->GetGpuResource()->IsSMCEnabled())
            {
                ErrPrintf("%s: %s object cannot be created on this channel (with Gr objects)"
                          " since there is no GrCE in SMC mode.\n", __FUNCTION__, ceName);
                return RC::ILWALID_CHANNEL;
            }
            if (ceType == UNSPECIFIED_CE)
            {
                ceType = GRAPHIC_CE;
            }
            else if (ceType != GRAPHIC_CE)
            {
                WarnPrintf("%s: %s object must be created"
                    " on graphic engine for this channel.\n", __FUNCTION__, ceName);
            }
        }
        else if (pLwGpuCh->HasVideoEngineObjectCreated(true))
        {
            WarnPrintf("%s: %s object cannot be created"
                " on this channel.\n", __FUNCTION__, ceName);
            return RC::ILWALID_CHANNEL;
        }
    }

    // create parameter buffer
    //
    Utility::CleanupOnReturn<vector<UINT08>, void> errorCleanup(
        &m_ClassParam, &vector<UINT08>::clear);
    if (ceType == GRAPHIC_CE)
    {
        m_ClassParam.resize(sizeof(LWA0B5_ALLOCATION_PARAMETERS));
        LWA0B5_ALLOCATION_PARAMETERS* pClassParam =
            reinterpret_cast<LWA0B5_ALLOCATION_PARAMETERS*>(&m_ClassParam[0]);

        pClassParam->version = LWA0B5_ALLOCATION_PARAMETERS_VERSION_1;
        CHECK_RC(pLwGpuCh->GetGpuResource()->GetGrCopyEngineType(
                &pClassParam->engineType, pLwGpuCh->GetPbdma(), pLwGpuCh->GetLwRmPtr()));
        if (pCeInstance)
        {
            *pCeInstance = MDiagUtils::GetCopyEngineOffset(pClassParam->engineType);
        }
    }
    else if (ceType == UNSPECIFIED_CE)
    {
        MASSERT(m_ClassParam.empty());
    }
    else
    {
        m_ClassParam.resize(sizeof(LW85B5_ALLOCATION_PARAMETERS));
        LW85B5_ALLOCATION_PARAMETERS* pClassParam =
            reinterpret_cast<LW85B5_ALLOCATION_PARAMETERS*>(&m_ClassParam[0]);

        pClassParam->engineInstance = ceType;
        if (pCeInstance)
        {
            *pCeInstance = ceType;
        }
    }

    errorCleanup.Cancel();
    if (m_ClassParam.empty())
        *ppRtnObjCreateParams = nullptr;
    else
        *ppRtnObjCreateParams = &m_ClassParam[0];
    return OK;
}

// ce type -> name
/*static*/const char* CEObjCreateParam::GetCeName(UINT32 ceType)
{
    switch (ceType)
    {
    case CE0:
        return "CE0";
    case CE1:
        return "CE1";
    case CE2:
        return "CE2";
    case CE3:
        return "CE3";
    case CE4:
        return "CE4";
    case CE5:
        return "CE5";
    case CE6:
        return "CE6";
    case CE7:
        return "CE7";
    case CE8:
        return "CE8";
    case CE9:
        return "CE9";
    case GRAPHIC_CE:
        return "GRAPHIC_CE";
    case UNSPECIFIED_CE:
        return "UNSPECIFIED_CE";
    default:
        return "UNKOWN";
    }
}

// ce engine type -> ce type(instance)
/*static*/UINT32 CEObjCreateParam::GetCeTypeByEngine(UINT32 ceEngineType)
{
    if (MDiagUtils::IsCopyEngine(ceEngineType))
    {
        return CE0 + MDiagUtils::GetCopyEngineOffset(ceEngineType);
    }
    else
    {
        MASSERT(!"Invalid CE Engine!\n");
        return UNSPECIFIED_CE;
    }
}

/*static*/ UINT32 CEObjCreateParam::GetEngineTypeByCeType(UINT32 ceType)
{
    if ((ceType >= CE0) && (CE0 <= LAST_CE))
    {
        return MDiagUtils::GetCopyEngineId(ceType - CE0);
    }
    else
    {
        MASSERT(!"Invalid CE Engine!\n");
        return LW2080_ENGINE_TYPE_NULL;
    }
}

/* static */void *LWGpuChannel::GetLwEncObjCreateParam
(
    UINT32 engineOffset,
    LW_MSENC_ALLOCATION_PARAMETERS *objParam
)
{
    if (objParam == nullptr ||
        engineOffset > LW2080_ENGINE_TYPE_LWENC_SIZE)
    {
        return nullptr;
    }
    
    objParam->size = sizeof(LW_MSENC_ALLOCATION_PARAMETERS);
    objParam->prohibitMultipleInstances = false;
    objParam->engineInstance = engineOffset;   

    return objParam;
}

void *LWGpuChannel::GetLwDecObjCreateParam
(
    UINT32 engineOffset,
    LW_BSP_ALLOCATION_PARAMETERS *objParam
)
{
    if (objParam == nullptr ||
        engineOffset > LW2080_ENGINE_TYPE_LWDEC_SIZE)
    {
        return nullptr;
    }

    objParam->size = sizeof(LW_BSP_ALLOCATION_PARAMETERS);
    objParam->prohibitMultipleInstances = false;
    objParam->engineInstance = engineOffset;

    return objParam;
}

void *LWGpuChannel::GetLwJpgObjCreateParam
(
    UINT32 engineOffset,
    LW_LWJPG_ALLOCATION_PARAMETERS *objParam
)
{
    if (objParam == nullptr || engineOffset >= LW2080_ENGINE_TYPE_LWJPEG_SIZE)
    {
        return nullptr;
    }

    objParam->size = sizeof(LW_LWJPG_ALLOCATION_PARAMETERS);
    objParam->prohibitMultipleInstances = false;
    objParam->engineInstance = engineOffset;

    return objParam;
}
