/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2017, 2020-2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Channel wrapper interface.

#include "core/include/chanwrap.h"
#include "core/include/utility.h"
#include <vector>

ChannelWrapper::ChannelWrapper(Channel *pChannel)
:   m_pWrappedChannel(pChannel)
{
    // Set this wrapper to be the outermost wrapper
    SetWrapper(this);
}

ChannelWrapper::~ChannelWrapper()
{
    delete m_pWrappedChannel;
}

/* virtual */ RC ChannelWrapper::SetAutoFlush
(
    bool AutoFlushEnable,
    UINT32 AutoFlushThreshold
)
{
    return m_pWrappedChannel->SetAutoFlush(AutoFlushEnable,
                                           AutoFlushThreshold);
}
/* virtual */ bool ChannelWrapper::GetAutoFlushEnable() const
{
    return m_pWrappedChannel->GetAutoFlushEnable();
}
/* virtual */ RC ChannelWrapper::SetAutoGpEntry
(
    bool AutoGpEntryEnable,
    UINT32 AutoGpEntryThreshold
)
{
    return m_pWrappedChannel->SetAutoGpEntry(AutoGpEntryEnable,
                                             AutoGpEntryThreshold);
}

/* virtual */ RC ChannelWrapper::WriteExternal
(
    UINT32 **ppExtBuf,
    UINT32 subchannel,
    UINT32 method,
    UINT32 count,
    const UINT32 *pData
)
{
    return m_pWrappedChannel->WriteExternal(ppExtBuf, subchannel,
                                            method, count, pData);
}

/* virtual */ RC ChannelWrapper::WriteExternalNonInc
(
    UINT32 **ppExtBuf,
    UINT32 subchannel,
    UINT32 method,
    UINT32 count,
    const UINT32 *pData
)
{
    return m_pWrappedChannel->WriteExternalNonInc(ppExtBuf, subchannel,
                                                  method, count, pData);
}

/* virtual */ RC ChannelWrapper::WriteExternalIncOnce
(
    UINT32 **ppExtBuf,
    UINT32 subchannel,
    UINT32 method,
    UINT32 count,
    const UINT32 *pData
)
{
    return m_pWrappedChannel->WriteExternalIncOnce(ppExtBuf, subchannel,
                                                   method, count, pData);
}

/* virtual */ RC ChannelWrapper::WriteExternalImmediate
(
    UINT32 **ppExtBuf,
    UINT32 subchannel,
    UINT32 method,
    UINT32 data
)
{
    return m_pWrappedChannel->WriteExternalImmediate(ppExtBuf, subchannel,
                                                     method, data);
}

/* virtual */ RC ChannelWrapper::Write
(
    UINT32 Subchannel,
    UINT32 Method,
    UINT32 Data
)
{
    return m_pWrappedChannel->Write(Subchannel, Method, Data);
}
/* virtual */ RC ChannelWrapper::Write
(
    UINT32 Subchannel,
    UINT32 Method,
    UINT32 Count,
    UINT32 Data,   // Add dummy Data parameter to resolve ambiguity between
                   //    Write(UINT32, UINT32, UINT32).
    ... // Rest of Data
)
{

    RC rc;
    UINT32 i;

    MASSERT(Count != 0);

    // Write the data to the push buffer.
    va_list DataVa;
    va_start(DataVa, Data);

    vector<UINT32> DataVec(Count);
    DataVec[0] = Data;

    for (i = 1; i < Count; ++i)
    {
        DataVec[i] = va_arg(DataVa, UINT32);
    }
    va_end(DataVa);

    CHECK_RC(Write(Subchannel,
                   Method,
                   Count,
                   &DataVec[0]));

    return rc;
}
/* virtual */ RC ChannelWrapper::Write
(
    UINT32          Subchannel,
    UINT32          Method,
    UINT32          Count,
    const UINT32 *  pData
)
{
    return m_pWrappedChannel->Write(Subchannel, Method, Count, pData);
}
/* virtual */ RC ChannelWrapper::WriteNonInc
(
    UINT32          Subchannel,
    UINT32          Method,
    UINT32          Count,
    const UINT32 *  pData
)
{
    return m_pWrappedChannel->WriteNonInc(Subchannel, Method, Count, pData);
}
/* virtual */ RC ChannelWrapper::WriteHeader
(
    UINT32 Subchannel,
    UINT32 Method,
    UINT32 Count
)
{
    return m_pWrappedChannel->WriteHeader(Subchannel, Method, Count);
}
/* virtual */ RC ChannelWrapper::WriteNonIncHeader
(
    UINT32 Subchannel,
    UINT32 Method,
    UINT32 Count
)
{
    return m_pWrappedChannel->WriteNonIncHeader(Subchannel, Method, Count);
}

/* virtual */ RC ChannelWrapper::WriteIncOnce
(
    UINT32          Subchannel,
    UINT32          Method,
    UINT32          Count,
    const UINT32 *  pData
)
{
    return m_pWrappedChannel->WriteIncOnce(Subchannel, Method, Count, pData);
}
/* virtual */ RC ChannelWrapper::WriteImmediate
(
    UINT32          Subchannel,
    UINT32          Method,
    UINT32          Data
)
{
    return m_pWrappedChannel->WriteImmediate(Subchannel, Method, Data);
}
/* virtual */ RC ChannelWrapper::WriteWithSurface
(
    UINT32           subchannel,
    UINT32           method,
    const Surface2D& surface,
    UINT32           offset,
    UINT32           offsetShift,
    MappingType      mappingType
)
{
    return m_pWrappedChannel->WriteWithSurface(subchannel,
                                               method,
                                               surface,
                                               offset,
                                               offsetShift,
                                               mappingType);
}
/* virtual */ RC ChannelWrapper::WriteWithSurfaceHandle
(
    UINT32      subchannel,
    UINT32      method,
    UINT32      hSurface,
    UINT32      offset,
    UINT32      offsetShift,
    MappingType mappingType
)
{
    return m_pWrappedChannel->WriteWithSurfaceHandle(subchannel,
                                                     method,
                                                     hSurface,
                                                     offset,
                                                     offsetShift,
                                                     mappingType);
}
/* virtual */ RC ChannelWrapper::WriteNop()
{
    return m_pWrappedChannel->WriteNop();
}
/* virtual */ RC ChannelWrapper::WriteSetSubdevice(UINT32 mask)
{
    return m_pWrappedChannel->WriteSetSubdevice(mask);
}
/* virtual */ RC ChannelWrapper::WriteCrcChkMethod()
{
    return m_pWrappedChannel->WriteCrcChkMethod();
}
/* virtual */ RC ChannelWrapper::WriteGpCrcChkGpEntry()
{
    return m_pWrappedChannel->WriteGpCrcChkGpEntry();
}
/* virtual */ RC ChannelWrapper::WriteAcquireMutex
(
    UINT32 subchannel,
    UINT32 mutexIdx
)
{
    return m_pWrappedChannel->WriteAcquireMutex(subchannel, mutexIdx);
}
/* virtual */ RC ChannelWrapper::WriteReleaseMutex
(
    UINT32 subchannel,
    UINT32 mutexIdx
)
{
    return m_pWrappedChannel->WriteReleaseMutex(subchannel, mutexIdx);
}
/* virtual */ RC ChannelWrapper::SetClass
(
    UINT32 subchannel,
    UINT32 classId
)
{
    return m_pWrappedChannel->SetClass(subchannel, classId);
}
/* virtual */ RC ChannelWrapper::SetClassAndWriteMask
(
    UINT32 subchannel,
    UINT32 classId,
    UINT32 method,
    UINT32 mask,
    const UINT32* pData
)
{
    return m_pWrappedChannel->SetClassAndWriteMask(subchannel,
                                                   classId,
                                                   method,
                                                   mask,
                                                   pData);
}
/* virtual */ RC ChannelWrapper::WriteMask
(
    UINT32 subchannel,
    UINT32 method,
    UINT32 mask,
    const UINT32* pData
)
{
    return m_pWrappedChannel->WriteMask(subchannel, method, mask, pData);
}
/* virtual */ RC ChannelWrapper::WriteDone()
{
    return m_pWrappedChannel->WriteDone();
}
/* virtual */ RC ChannelWrapper::CallSubroutine(UINT64 Offset, UINT32 Size)
{
    return m_pWrappedChannel->CallSubroutine(Offset, Size);
}
/* virtual */ RC ChannelWrapper::InsertSubroutine
(
    const UINT32 *pBuffer,
    UINT32 count
)
{
    return m_pWrappedChannel->InsertSubroutine(pBuffer, count);
}
/* virtual */ RC ChannelWrapper::SetHwCrcCheckMode(UINT32 Mode)
{
    return m_pWrappedChannel->SetHwCrcCheckMode(Mode);
}
/* virtual */ RC ChannelWrapper::SetPrivEnable(bool Priv)
{
    return m_pWrappedChannel->SetPrivEnable(Priv);
}
/* virtual */ RC ChannelWrapper::SetSyncEnable(bool Sync)
{
    return m_pWrappedChannel->SetSyncEnable(Sync);
}
/* virtual */ RC ChannelWrapper::ScheduleChannel(bool Enable)
{
    return m_pWrappedChannel->ScheduleChannel(Enable);
}
/* virtual */ RC ChannelWrapper::SetObject
(
    UINT32 Subchannel,
    LwRm::Handle Handle
)
{
    return m_pWrappedChannel->SetObject(Subchannel, Handle);
}
/* virtual */ RC ChannelWrapper::UnsetObject(LwRm::Handle Handle)
{
    return m_pWrappedChannel->UnsetObject(Handle);
}
/* virtual */ RC ChannelWrapper::SetReference(UINT32 Value)
{
    return m_pWrappedChannel->SetReference(Value);
}
/* virtual */ RC ChannelWrapper::SetContextDmaSemaphore(LwRm::Handle hCtxDma)
{
    return m_pWrappedChannel->SetContextDmaSemaphore(hCtxDma);
}
/* virtual */ RC ChannelWrapper::SetSemaphoreOffset(UINT64 Offset)
{
    return m_pWrappedChannel->SetSemaphoreOffset(Offset);
}
/* virtual */ void ChannelWrapper::SetSemaphoreReleaseFlags(UINT32 flags)
{
    m_pWrappedChannel->SetSemaphoreReleaseFlags(flags);
}
/* virtual */ UINT32 ChannelWrapper::GetSemaphoreReleaseFlags()
{
    return m_pWrappedChannel->GetSemaphoreReleaseFlags();
}
/* virtual */ RC ChannelWrapper::SetSemaphorePayloadSize(SemaphorePayloadSize size)
{
    return m_pWrappedChannel->SetSemaphorePayloadSize(size);
}
/* virtual */ Channel::SemaphorePayloadSize ChannelWrapper::GetSemaphorePayloadSize()
{
    return m_pWrappedChannel->GetSemaphorePayloadSize();
}
/* virtual */ RC ChannelWrapper::SemaphoreAcquire(UINT64 Data)
{
    return m_pWrappedChannel->SemaphoreAcquire(Data);
}
/* virtual */ RC ChannelWrapper::SemaphoreAcquireGE(UINT64 Data)
{
    return m_pWrappedChannel->SemaphoreAcquireGE(Data);
}
/* virtual */ RC ChannelWrapper::SemaphoreRelease(UINT64 Data)
{
    return m_pWrappedChannel->SemaphoreRelease(Data);
}
/* virtual */ RC ChannelWrapper::SemaphoreReduction(Reduction Op, ReductionType redType, UINT64 Data)
{
    return m_pWrappedChannel->SemaphoreReduction(Op, redType, Data);
}
/* virtual */ RC ChannelWrapper::NonStallInterrupt()
{
    return m_pWrappedChannel->NonStallInterrupt();
}
/* virtual */ RC ChannelWrapper::Yield(UINT32 Subchannel)
{
    return m_pWrappedChannel->Yield(Subchannel);
}

//--------------------------------------------------------------------
//! \brief The caller is about to insert methods into the pushbuffer.
//!
//! This method tells the ChannelWrapper that the caller is about to
//! insert "extra" methods that weren't a part of the original test.
//!
//! Such methods are inserted by Runlist (for the end-of-entry backend
//! semaphore) and PolicyManager.  Inserted methods can corrupt the
//! test if they occur at the wrong moment, so some ChannelWrappers
//! are designed to prevent that.  For example, AtomChannelWrapper
//! prevents methods from getting inserted into the middle of "atomic"
//! method sequences, and SemaphoreChannelWrapper monitors the state
//! of semaphore methods in order to restore the original state when
//! the inserted methods end.
//!
//! Normally, every BeginInsertedMethods() should have a matching
//! EndInsertedMethods().  Begin/End calls can be nested.  If an error
//! oclwrs between Begin and End, the caller should call
//! CancelInsertedMethods() to tell the channel to clean up one nested
//! level of Begin/End, without the overhead of doing a full End.
//!
//! CancelInsertedMethods() should only be called if an error oclwrs
//! *between* BeginInsertedMethods() and EndInsertedMethods(),
//! non-inclusive.  If an error oclwrs in Begin or End, they should do
//! an automatic Cancel.  So, for example, the following code should
//! work:
//!
//! \verbatim
//!     CHECK_RC(pChannel->BeginInsertedMethods());
//!     CHECK_RC_CLEANUP(pChannel->Write(...));
//!     CHECK_RC_CLEANUP(pChannel->Write(...));
//! Cleanup:
//!     if (rc == OK)
//!         CHECK_RC(pChannel->EndInsertedMethods());
//!     else
//!         pChannel->CancelInsertedMethods();
//! \endverbatim
//!
//! \sa EndInsertedMethods()
//! \sa CancelInsertedMethods()
//!
/* virtual */ RC ChannelWrapper::BeginInsertedMethods()
{
    return m_pWrappedChannel->BeginInsertedMethods();
}

//--------------------------------------------------------------------
//! \brief End a series of inserted methods begun by BeginInsertedMethods().
//!
//! \sa BeginInsertedMethods()
//!
/* virtual */ RC ChannelWrapper::EndInsertedMethods()
{
    return m_pWrappedChannel->EndInsertedMethods();
}

//--------------------------------------------------------------------
//! \brief Cancel a BeginInsertedMethods()/EndInsertedMethods() due to error.
//!
//! \sa BeginInsertedMethods()
//!
/* virtual */ void ChannelWrapper::CancelInsertedMethods()
{
    return m_pWrappedChannel->CancelInsertedMethods();
}
/* virtual */ RC ChannelWrapper::GetSyncPoint(UINT32* pSyncPointId)
{
    return m_pWrappedChannel->GetSyncPoint(pSyncPointId);
}
/* virtual */ UINT32 ChannelWrapper::ReadSyncPoint(UINT32 syncPointId)
{
    return m_pWrappedChannel->ReadSyncPoint(syncPointId);
}
/* virtual */ void ChannelWrapper::IncrementSyncPoint(UINT32 syncPointId)
{
    m_pWrappedChannel->IncrementSyncPoint(syncPointId);
}
/* virtual */ RC ChannelWrapper::WriteSyncPointIncrement(UINT32 syncPointId)
{
    return m_pWrappedChannel->WriteSyncPointIncrement(syncPointId);
}
/* virtual */ RC ChannelWrapper::WriteSyncPointWait(UINT32 syncPointId, UINT32 value)
{
    return m_pWrappedChannel->WriteSyncPointWait(syncPointId, value);
}
/* virtual */ RC ChannelWrapper::Flush()
{
    return m_pWrappedChannel->Flush();
}
/* virtual */ RC ChannelWrapper::WaitForDmaPush(FLOAT64 TimeoutMs)
{
    return m_pWrappedChannel->WaitForDmaPush(TimeoutMs);
}
/* virtual */ RC ChannelWrapper::WaitIdle(FLOAT64 TimeoutMs)
{
    return m_pWrappedChannel->WaitIdle(TimeoutMs);
}
/* virtual */
RC ChannelWrapper::SetLegacyWaitIdleWithRiskyIntermittentDeadlocking
(
    bool Enable
)
{
    return m_pWrappedChannel->
        SetLegacyWaitIdleWithRiskyIntermittentDeadlocking(Enable);
}
/* virtual */
bool ChannelWrapper::GetLegacyWaitIdleWithRiskyIntermittentDeadlocking() const
{
    return m_pWrappedChannel->
        GetLegacyWaitIdleWithRiskyIntermittentDeadlocking();
}
/* virtual */ void ChannelWrapper::SetDefaultTimeoutMs(FLOAT64 TimeoutMs)
{
    m_pWrappedChannel->SetDefaultTimeoutMs(TimeoutMs);
}
/* virtual */ FLOAT64 ChannelWrapper::GetDefaultTimeoutMs()
{
    return m_pWrappedChannel->GetDefaultTimeoutMs();
}
/* virtual */ UINT32 ChannelWrapper::GetClass()
{
    return m_pWrappedChannel->GetClass();
}
/* virtual */ LwRm::Handle ChannelWrapper::GetHandle()
{
    return m_pWrappedChannel->GetHandle();
}
/* virtual */ void ChannelWrapper::ClearPushbuffer()
{
    m_pWrappedChannel->ClearPushbuffer();
}
/* virtual */ RC ChannelWrapper::CheckError()
{
    return m_pWrappedChannel->CheckError();
}
/* virtual */ void ChannelWrapper::SetError(RC Error)
{
    m_pWrappedChannel->SetError(Error);
}
/* virtual */ void ChannelWrapper::ClearError()
{
    m_pWrappedChannel->ClearError();
}
/* virtual */
bool ChannelWrapper::DetectNewRobustChannelError() const
{
    return m_pWrappedChannel->DetectNewRobustChannelError();
}
bool ChannelWrapper::DetectError() const
{
    return m_pWrappedChannel->DetectError();
}
/* virtual */ RC ChannelWrapper::GetCachedPut(UINT32 *PutPtr)
{
    return m_pWrappedChannel->GetCachedPut(PutPtr);
}
/* virtual */ RC ChannelWrapper::SetCachedPut(UINT32 PutPtr)
{
    return m_pWrappedChannel->SetCachedPut(PutPtr);
}
/* virtual */ RC ChannelWrapper::GetPut(UINT32 *PutPtr)
{
    return m_pWrappedChannel->GetPut(PutPtr);
}
/* virtual */ RC ChannelWrapper::SetPut(UINT32 PutPtr)
{
    return m_pWrappedChannel->SetPut(PutPtr);
}
/* virtual */ RC ChannelWrapper::GetGet(UINT32 *GetPtr)
{
    return m_pWrappedChannel->GetGet(GetPtr);
}
/* virtual */ RC ChannelWrapper::GetPushbufferSpace(UINT32 *PbSpacePtr)
{
    return m_pWrappedChannel->GetPushbufferSpace(PbSpacePtr);
}
/* virtual */ RC ChannelWrapper::GetCachedGpPut(UINT32 *GpPutPtr)
{
    return m_pWrappedChannel->GetCachedGpPut(GpPutPtr);
}
/* virtual */ RC ChannelWrapper::GetGpPut(UINT32 *GpPutPtr)
{
    return m_pWrappedChannel->GetGpPut(GpPutPtr);
}
/* virtual */ RC ChannelWrapper::GetGpGet(UINT32 *GpGetPtr)
{
    return m_pWrappedChannel->GetGpGet(GpGetPtr);
}
/* virtual */ RC ChannelWrapper::GetGpEntries(UINT32 *GpEntriesPtr)
{
    return m_pWrappedChannel->GetGpEntries(GpEntriesPtr);
}
/* virtual */ RC ChannelWrapper::FinishOpenGpFifoEntry(UINT64* pEnd)
{
    return m_pWrappedChannel->FinishOpenGpFifoEntry(pEnd);
}
/* virtual */ RC ChannelWrapper::GetReference(UINT32 *pRefCnt)
{
    return m_pWrappedChannel->GetReference(pRefCnt);
}
/* virtual */ RC ChannelWrapper::SetExternalGPPutAdvanceMode(bool enable)
{
    return m_pWrappedChannel->SetExternalGPPutAdvanceMode(enable);
}
/* virtual */ UINT32 ChannelWrapper::GetChannelId() const
{
    return m_pWrappedChannel->GetChannelId();
}
/* virtual */ void ChannelWrapper::EnableLogging(bool b)
{
    m_pWrappedChannel->EnableLogging(b);
}
/* virtual */ bool ChannelWrapper::GetEnableLogging() const
{
    return m_pWrappedChannel->GetEnableLogging();
}
/* virtual */ void ChannelWrapper::SetIdleChannelsRetries(UINT32 retries)
{
    m_pWrappedChannel->SetIdleChannelsRetries(retries);
}
/* virtual */ UINT32 ChannelWrapper::GetIdleChannelsRetries()
{
    return m_pWrappedChannel->GetIdleChannelsRetries();
}
/* virtual */ GpuDevice *ChannelWrapper::GetGpuDevice() const
{
    return m_pWrappedChannel->GetGpuDevice();
}
/* virtual */ void ChannelWrapper::SetWrapper(Channel *pWrapper)
{
    m_pOutermostWrapper = pWrapper;

    // The wrapper is always the outermost parent.  So propagate the wrapper
    // pointer down the line to all other potential wrappers
    m_pWrappedChannel->SetWrapper(pWrapper);
}
/* virtual */ AtomChannelWrapper * ChannelWrapper::GetAtomChannelWrapper()
{
    return m_pWrappedChannel->GetAtomChannelWrapper();
}
/* virtual */ PmChannelWrapper * ChannelWrapper::GetPmChannelWrapper()
{
    return m_pWrappedChannel->GetPmChannelWrapper();
}
/* virtual */ RunlistChannelWrapper *ChannelWrapper::GetRunlistChannelWrapper()
{
    return m_pWrappedChannel->GetRunlistChannelWrapper();
}
/* virtual */ SemaphoreChannelWrapper *ChannelWrapper::GetSemaphoreChannelWrapper()
{
    return m_pWrappedChannel->GetSemaphoreChannelWrapper();
}
/* virtual */ UtlChannelWrapper * ChannelWrapper::GetUtlChannelWrapper()
{
    return m_pWrappedChannel->GetUtlChannelWrapper();
}
/* virtual */ RC ChannelWrapper::RobustChannelCallback
(
    UINT32 errorLevel,
    UINT32 errorType,
    void *pData,
    void *pRecoveryCallback
)
{
    return m_pWrappedChannel->RobustChannelCallback(errorLevel, errorType,
                                                    pData, pRecoveryCallback);
}
/* virtual */ LwRm * ChannelWrapper::GetRmClient()
{
    return m_pWrappedChannel->GetRmClient();
}
/* virtual */ RC ChannelWrapper::RmcGetClassEngineId
(
    LwRm::Handle hObject,
    UINT32 *pClassEngineId,
    UINT32 *pClassId,
    UINT32 *pEngineId
)
{
    return m_pWrappedChannel->RmcGetClassEngineId(hObject, pClassEngineId,
                                                  pClassId, pEngineId);
}
/* virtual */ RC ChannelWrapper::RmcResetChannel(UINT32 EngineId)
{
    return m_pWrappedChannel->RmcResetChannel(EngineId);
}
/* virtual */ RC ChannelWrapper::RmcGpfifoSchedule(bool Enable)
{
    return m_pWrappedChannel->RmcGpfifoSchedule(Enable);
}
/* virtual */ RC ChannelWrapper::SetRandomAutoFlushThreshold
(
    const MinMaxSeed &minmaxseed
)
{
    return m_pWrappedChannel->SetRandomAutoFlushThreshold(minmaxseed);
}
/* virtual */ RC ChannelWrapper::SetRandomAutoFlushThresholdGpFifoEntries
(
    const MinMaxSeed &minmaxseed
)
{
    return m_pWrappedChannel->SetRandomAutoFlushThresholdGpFifoEntries(minmaxseed);
}
/* virtual */ RC ChannelWrapper::SetRandomAutoGpEntryThreshold
(
    const MinMaxSeed &minmaxseed
)
{
    return m_pWrappedChannel->SetRandomAutoGpEntryThreshold(minmaxseed);
}
/* virtual */ RC ChannelWrapper::SetRandomPauseBeforeGPPutWrite
(
    const MinMaxSeed &minmaxseed
)
{
    return m_pWrappedChannel->SetRandomPauseBeforeGPPutWrite(minmaxseed);
}
/* virtual */ RC ChannelWrapper::SetRandomPauseAfterGPPutWrite
(
    const MinMaxSeed &minmaxseed
)
{
    return m_pWrappedChannel->SetRandomPauseAfterGPPutWrite(minmaxseed);
}
/* virtual */ RC ChannelWrapper::RecoverFromRobustChannelError()
{
    return m_pWrappedChannel->RecoverFromRobustChannelError();
}

/* virtual */ RC ChannelWrapper::SetAutoSchedule(bool bAutoSched)
{
    return m_pWrappedChannel->SetAutoSchedule(bAutoSched);
}

/* virtual */ bool ChannelWrapper::Has64bitSemaphores()
{
    return m_pWrappedChannel->Has64bitSemaphores();
}

/* virtual */ RC ChannelWrapper::Initialize()
{
    return m_pWrappedChannel->Initialize();
}

/* virtual */ bool ChannelWrapper::GetDoorbellRingEnable() const
{
    return m_pWrappedChannel->GetDoorbellRingEnable();
}

/* virtual */ RC ChannelWrapper::SetDoorbellRingEnable(bool enable)
{
    return m_pWrappedChannel->SetDoorbellRingEnable(enable);
}

/* virtual */ bool ChannelWrapper::GetUseBar1Doorbell() const
{
    return m_pWrappedChannel->GetUseBar1Doorbell();
}
