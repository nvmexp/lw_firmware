/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2018, 2020-2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/include/tegra_thi_channel.h"

// We assume that THI is the same for all engines.
// So far there is no THI-specific header.
#include "t19x/t194/dev_vic_pri.h" // LW_PVIC_THI_METHOD0/1

TegraTHIChannel::TegraTHIChannel(unique_ptr<Channel> pChannel)
: m_pChannel(move(pChannel))
{
}

/* virtual */ TegraTHIChannel::~TegraTHIChannel()
{
}

/* virtual */ RC TegraTHIChannel::Write
(
    UINT32 subchannel,
    UINT32 method,
    UINT32 data
)
{
    RC rc;
    CHECK_RC(m_pChannel->Write(subchannel,
                LW_PVIC_THI_METHOD0 >> 2, method >> 2));
    CHECK_RC(m_pChannel->Write(subchannel,
                LW_PVIC_THI_METHOD1 >> 2, data));
    return rc;
}

/* virtual */ RC TegraTHIChannel::WriteWithSurface
(
    UINT32           subchannel,
    UINT32           method,
    const Surface2D& surface,
    UINT32           offset,
    UINT32           offsetShift,
    MappingType      mappingType
)
{
    RC rc;
    CHECK_RC(m_pChannel->Write(subchannel,
                LW_PVIC_THI_METHOD0 >> 2, method >> 2));
    CHECK_RC(m_pChannel->WriteWithSurface(subchannel,
                LW_PVIC_THI_METHOD1 >> 2, surface, offset, offsetShift, mappingType));
    return rc;
}

/* virtual */ RC TegraTHIChannel::WriteWithSurfaceHandle
(
    UINT32      subchannel,
    UINT32      method,
    UINT32      hSurface,
    UINT32      offset,
    UINT32      offsetShift,
    MappingType mappingType
)
{
    RC rc;
    CHECK_RC(m_pChannel->Write(subchannel,
                LW_PVIC_THI_METHOD0 >> 2, method >> 2));
    CHECK_RC(m_pChannel->WriteWithSurfaceHandle(subchannel,
                LW_PVIC_THI_METHOD1 >> 2, hSurface, offset, offsetShift, mappingType));
    return rc;
}

/* virtual */ RC TegraTHIChannel::SetObject(UINT32 Subchannel, LwRm::Handle Handle)
{
    // Not supported by host1x. Use SetClass() instead.
    // Just ignore this for compatibility with dGPU.
    return OK;
}

/* virtual */ RC TegraTHIChannel::WriteExternal
(
    UINT32**      ppExtBuf,
    UINT32        subchannel,
    UINT32        method,
    UINT32        count,
    const UINT32* pData
)
{
    MASSERT(ppExtBuf);
    MASSERT(*ppExtBuf);
    RC rc;

    if (pData == nullptr)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    UINT32 shiftedMethod = method >> 2;
    for (UINT32 ii = 0; ii < count; ++ii)
    {
        CHECK_RC(m_pChannel->WriteExternal(ppExtBuf, subchannel,
                                           LW_PVIC_THI_METHOD0 >> 2, 1,
                                           &shiftedMethod));
        CHECK_RC(m_pChannel->WriteExternal(ppExtBuf, subchannel,
                                           LW_PVIC_THI_METHOD1 >> 2, 1,
                                           &pData[ii]));
        ++shiftedMethod;
    }
    return rc;
}

/* virtual */ RC TegraTHIChannel::WriteExternalNonInc
(
    UINT32**      ppExtBuf,
    UINT32        subchannel,
    UINT32        method,
    UINT32        count,
    const UINT32* pData
)
{
    MASSERT(ppExtBuf);
    MASSERT(*ppExtBuf);
    RC rc;

    if (pData == nullptr)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    UINT32 shiftedMethod = method >> 2;
    for (UINT32 ii = 0; ii < count; ++ii)
    {
        CHECK_RC(m_pChannel->WriteExternal(ppExtBuf, subchannel,
                                           LW_PVIC_THI_METHOD0 >> 2, 1,
                                           &shiftedMethod));
        CHECK_RC(m_pChannel->WriteExternal(ppExtBuf, subchannel,
                                           LW_PVIC_THI_METHOD1 >> 2, 1,
                                           &pData[ii]));
    }
    return rc;
}

/* virtual */ RC TegraTHIChannel::WriteExternalIncOnce
(
    UINT32**      ppExtBuf,
    UINT32        subchannel,
    UINT32        method,
    UINT32        count,
    const UINT32* pData
)
{
    MASSERT(ppExtBuf);
    MASSERT(*ppExtBuf);
    RC rc;

    if (pData == nullptr)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    UINT32 shiftedMethod = method >> 2;
    for (UINT32 ii = 0; ii < count; ++ii)
    {
        CHECK_RC(m_pChannel->WriteExternal(ppExtBuf, subchannel,
                                           LW_PVIC_THI_METHOD0 >> 2, 1,
                                           &shiftedMethod));
        CHECK_RC(m_pChannel->WriteExternal(ppExtBuf, subchannel,
                                           LW_PVIC_THI_METHOD1 >> 2, 1,
                                           &pData[ii]));
        if (ii == 0)
        {
            ++shiftedMethod;
        }
    }
    return rc;
}

/* virtual */ RC TegraTHIChannel::WriteExternalImmediate
(
    UINT32** ppExtBuf,
    UINT32   subchannel,
    UINT32   method,
    UINT32   data
)
{
    MASSERT(ppExtBuf);
    MASSERT(*ppExtBuf);
    RC rc;

    CHECK_RC(m_pChannel->WriteExternalImmediate(ppExtBuf, subchannel,
                                                LW_PVIC_THI_METHOD0 >> 2,
                                                method >> 2));
    CHECK_RC(m_pChannel->WriteExternalImmediate(ppExtBuf, subchannel,
                                                LW_PVIC_THI_METHOD1 >> 2,
                                                data));
    return rc;
}

// ============================================================================
// Functions below simply ilwoke the inner channel object
// ============================================================================

/* virtual */ RC TegraTHIChannel::SetClass
(
    UINT32 subchannel,
    UINT32 classId
)
{
    return m_pChannel->SetClass(subchannel, classId);
}

/* virtual */ RC TegraTHIChannel::InsertSubroutine
(
    const UINT32 *pBuffer,
    UINT32 count
)
{
    return m_pChannel->InsertSubroutine(pBuffer, count);
}

/* virtual */ RC TegraTHIChannel::GetSyncPoint(UINT32* pSyncPointId)
{
    return m_pChannel->GetSyncPoint(pSyncPointId);
}

/* virtual */ UINT32 TegraTHIChannel::ReadSyncPoint(UINT32 syncPointId)
{
    return m_pChannel->ReadSyncPoint(syncPointId);
}

/* virtual */ void TegraTHIChannel::IncrementSyncPoint(UINT32 syncPointId)
{
    m_pChannel->IncrementSyncPoint(syncPointId);
}

/* virtual */ RC TegraTHIChannel::WriteSyncPointIncrement(UINT32 syncPointId)
{
    return m_pChannel->WriteSyncPointIncrement(syncPointId);
}

/* virtual */ RC TegraTHIChannel::WriteSyncPointWait(UINT32 syncPointId, UINT32 value)
{
    return m_pChannel->WriteSyncPointWait(syncPointId, value);
}

/* virtual */ RC TegraTHIChannel::Flush()
{
    return m_pChannel->Flush();
}

/* virtual */ void TegraTHIChannel::SetDefaultTimeoutMs(FLOAT64 timeoutMs)
{
    m_pChannel->SetDefaultTimeoutMs(timeoutMs);
}

/* virtual */ FLOAT64 TegraTHIChannel::GetDefaultTimeoutMs()
{
    return m_pChannel->GetDefaultTimeoutMs();
}

/* virtual */ UINT32 TegraTHIChannel::GetClass()
{
    return m_pChannel->GetClass();
}

/* virtual */ LwRm::Handle TegraTHIChannel::GetHandle()
{
    return m_pChannel->GetHandle();
}

/* virtual */ UINT32 TegraTHIChannel::GetChannelId() const
{
    return m_pChannel->GetChannelId();
}

/* virtual */ void TegraTHIChannel::EnableLogging(bool b)
{
    m_pChannel->EnableLogging(b);
}

/* virtual */ bool TegraTHIChannel::GetEnableLogging() const
{
    return m_pChannel->GetEnableLogging();
}

/* virtual */ GpuDevice* TegraTHIChannel::GetGpuDevice() const
{
    return m_pChannel->GetGpuDevice();
}

/* virtual */ LwRm* TegraTHIChannel::GetRmClient()
{
    return m_pChannel->GetRmClient();
}

/* virtual */ RC TegraTHIChannel::CheckError()
{
    return m_pChannel->CheckError();
}

/* virtual */ void TegraTHIChannel::SetError(RC error)
{
    m_pChannel->SetError(error);
}

/* virtual */ void TegraTHIChannel::ClearError()
{
    m_pChannel->ClearError();
}

/* virtual */ RC TegraTHIChannel::WaitIdle(FLOAT64 timeoutMs)
{
    return m_pChannel->WaitIdle(timeoutMs);
}

/* virtual */ LwRm::Handle TegraTHIChannel::GetVASpace() const
{
    return m_pChannel->GetVASpace();
}

/* virtual */ UINT32 TegraTHIChannel::GetEngineId() const
{
    return m_pChannel->GetEngineId();
}

/* virtual */ bool TegraTHIChannel::GetDoorbellRingEnable() const
{
    return m_pChannel->GetDoorbellRingEnable();
}

/* virtual */ RC TegraTHIChannel::SetDoorbellRingEnable(bool enable)
{
    return m_pChannel->SetDoorbellRingEnable(enable);
}

/* virtual */ bool TegraTHIChannel::GetUseBar1Doorbell() const
{
    return m_pChannel->GetUseBar1Doorbell();
}

// ============================================================================
// The remaining functions below are unsupported.
// ============================================================================

/* virtual */ RC TegraTHIChannel::SetAutoFlush
(
    bool AutoFlushEnable,
    UINT32 AutoFlushThreshold
)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ bool TegraTHIChannel::GetAutoFlushEnable() const
{
    return false;
}

/* virtual */ RC TegraTHIChannel::SetAutoGpEntry
(
    bool AutoGpEntryEnable,
    UINT32 AutoGpEntryThreshold
)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::Write
(
    UINT32 subchannel,
    UINT32 method,
    UINT32 count,
    UINT32 data,
    ...
)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::Write
(
    UINT32        subchannel,
    UINT32        method,
    UINT32        count,
    const UINT32* pData
)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::WriteNonInc
(
    UINT32          subchannel,
    UINT32          method,
    UINT32          count,
    const UINT32*   pData
)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::WriteHeader
(
    UINT32 subchannel,
    UINT32 method,
    UINT32 count
)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::WriteNonIncHeader
(
    UINT32 subchannel,
    UINT32 method,
    UINT32 count
)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::WriteIncOnce
(
    UINT32        subchannel,
    UINT32        method,
    UINT32        count,
    const UINT32* pData
)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::WriteImmediate
(
    UINT32 subchannel,
    UINT32 method,
    UINT32 data
)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::WriteNop()
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::WriteSetSubdevice(UINT32 mask)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::WriteCrcChkMethod()
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::WriteGpCrcChkGpEntry()
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::WriteAcquireMutex(UINT32, UINT32)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::WriteReleaseMutex(UINT32, UINT32)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::SetClassAndWriteMask(UINT32, UINT32, UINT32, UINT32, const UINT32*)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::WriteMask(UINT32, UINT32, UINT32, const UINT32*)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::WriteDone()
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::CallSubroutine(UINT64 Offset, UINT32 Size)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::SetHwCrcCheckMode(UINT32 Mode)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::SetPrivEnable(bool Priv)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::SetSyncEnable(bool Sync)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::ScheduleChannel(bool Enable)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::UnsetObject(LwRm::Handle Handle)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::SetReference(UINT32 Value)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::SetContextDmaSemaphore(LwRm::Handle hCtxDma)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::SetSemaphoreOffset(UINT64 Offset)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ void TegraTHIChannel::SetSemaphoreReleaseFlags(UINT32 flags)
{
}

/* virtual */ UINT32 TegraTHIChannel::GetSemaphoreReleaseFlags()
{
    return 0U;
}

/* virtual */ RC TegraTHIChannel::SetSemaphorePayloadSize(SemaphorePayloadSize size)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ Channel::SemaphorePayloadSize TegraTHIChannel::GetSemaphorePayloadSize()
{
    return SEM_PAYLOAD_SIZE_DEFAULT;
}

/* virtual */ RC TegraTHIChannel::SemaphoreAcquire(UINT64 Data)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::SemaphoreAcquireGE(UINT64 Data)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::SemaphoreRelease(UINT64 Data)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::SemaphoreReduction(Reduction Op, ReductionType redType, UINT64 Data)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::NonStallInterrupt()
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::Yield(UINT32 Subchannel)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::BeginInsertedMethods()
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::EndInsertedMethods()
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ void TegraTHIChannel::CancelInsertedMethods()
{
}

/* virtual */ RC TegraTHIChannel::WaitForDmaPush(FLOAT64 timeoutMs)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::SetLegacyWaitIdleWithRiskyIntermittentDeadlocking(bool Enable)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ bool TegraTHIChannel::GetLegacyWaitIdleWithRiskyIntermittentDeadlocking() const
{
    return false;
}

/* virtual */ bool TegraTHIChannel::DetectNewRobustChannelError() const
{
    return false;
}

/* virtual */ bool TegraTHIChannel::DetectError() const
{
    return false;
}

/* virtual */ void TegraTHIChannel::ClearPushbuffer()
{
}

/* virtual */ RC TegraTHIChannel::GetCachedPut(UINT32* PutPtr)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::SetCachedPut(UINT32 PutPtr)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::GetPut(UINT32* PutPtr)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::SetPut(UINT32 PutPtr)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::GetGet(UINT32* GetPtr)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::GetPushbufferSpace(UINT32* PbSpacePtr)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::GetCachedGpPut(UINT32* GpPutPtr)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::GetGpPut(UINT32* GpPutPtr)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::GetGpGet(UINT32* GpGetPtr)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::GetGpEntries(UINT32* GpEntriesPtr)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::FinishOpenGpFifoEntry(UINT64* pEnd)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::GetReference(UINT32* pRefCnt)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::SetExternalGPPutAdvanceMode(bool enable)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ void TegraTHIChannel::SetIdleChannelsRetries(UINT32 retries)
{
}

/* virtual */ UINT32 TegraTHIChannel::GetIdleChannelsRetries()
{
    return 0;
}

/* virtual */ void TegraTHIChannel::SetWrapper(Channel* pWrapper)
{
}

/* virtual */ Channel* TegraTHIChannel::GetWrapper() const
{
    return nullptr;
}

/* virtual */ AtomChannelWrapper* TegraTHIChannel::GetAtomChannelWrapper()
{
    return nullptr;
}

/* virtual */ PmChannelWrapper* TegraTHIChannel::GetPmChannelWrapper()
{
    return nullptr;
}

/* virtual */ RunlistChannelWrapper* TegraTHIChannel::GetRunlistChannelWrapper()
{
    return nullptr;
}

/* virtual */ SemaphoreChannelWrapper* TegraTHIChannel::GetSemaphoreChannelWrapper()
{
    return nullptr;
}

/* virtual */ RC TegraTHIChannel::RobustChannelCallback(UINT32 errorLevel,
                                 UINT32 errorType,
                                 void* pData,
                                 void* pRecoveryCallback)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::RecoverFromRobustChannelError()
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::RmcGetClassEngineId
(
    LwRm::Handle hObject,
    UINT32* pClassEngineId,
    UINT32* pClassId,
    UINT32* pEngineId
)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::RmcResetChannel(UINT32 EngineId)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::RmcGpfifoSchedule(bool Enable)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::SetRandomAutoFlushThreshold(const MinMaxSeed &minmaxseed)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::SetRandomAutoFlushThresholdGpFifoEntries(const MinMaxSeed &minmaxseed)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::SetRandomAutoGpEntryThreshold(const MinMaxSeed &minmaxseed)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::SetRandomPauseBeforeGPPutWrite(const MinMaxSeed &minmaxseed)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::SetRandomPauseAfterGPPutWrite(const MinMaxSeed &minmaxseed)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::SetAutoSchedule(bool)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC TegraTHIChannel::Initialize()
{
    return OK;
}
