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

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef INCLUDED_TEGRA_THI_CHANNEL_H
#define INCLUDED_TEGRA_THI_CHANNEL_H

#ifndef INCLUDED_CHANNEL_H
#include "core/include/channel.h"
#endif

//--------------------------------------------------------------------
//! \brief Class that wraps engine methods with THI methods on CheetAh.
//!
//! Some engines from dGPU have been adopted on CheetAh. This includes
//! VIC, LWDEC, LWENC, TSEC and probably more in the future.
//!
//! There is a physical layer between host1x and these engines called
//! THI, which stands for CheetAh Host Interface.
//!
//! Probably because of the deficiencies of host1x, you cannot write
//! the engine methods directly in the pushbuffer, but instead you have
//! to wrap them with THI methods.
//!
//! This class provides the THI method wrapping, so individual tests
//! targeting specific engines can just write the engine methods. This
//! allows these tests to work unmodified on both dGPU and CheetAh.
//!
class TegraTHIChannel : public Channel
{
public:
    explicit TegraTHIChannel(unique_ptr<Channel> pChannel);
    virtual ~TegraTHIChannel();

    virtual RC SetAutoFlush
    (
        bool AutoFlushEnable,
        UINT32 AutoFlushThreshold
    );
    virtual bool GetAutoFlushEnable() const;

    virtual RC SetAutoGpEntry
    (
        bool AutoGpEntryEnable,
        UINT32 AutoGpEntryThreshold
    );

    virtual RC WriteExternal
    (
        UINT32**      ppExtBuf,
        UINT32        subchannel,
        UINT32        method,
        UINT32        count,
        const UINT32* pData
    );
    virtual RC WriteExternalNonInc
    (
        UINT32**      ppExtBuf,
        UINT32        subchannel,
        UINT32        method,
        UINT32        count,
        const UINT32* pData
    );
    virtual RC WriteExternalIncOnce
    (
        UINT32**      ppExtBuf,
        UINT32        subchannel,
        UINT32        method,
        UINT32        count,
        const UINT32* pData
    );
    virtual RC WriteExternalImmediate
    (
        UINT32** ppExtBuf,
        UINT32   subchannel,
        UINT32   method,
        UINT32   data
    );

    virtual RC Write
    (
        UINT32 Subchannel,
        UINT32 Method,
        UINT32 Data
    );

    virtual RC Write
    (
        UINT32 Subchannel,
        UINT32 Method,
        UINT32 Count,
        UINT32 Data,
        ...
    );

    virtual RC Write
    (
        UINT32          Subchannel,
        UINT32          Method,
        UINT32          Count,
        const UINT32*   pData
    );

    virtual RC WriteNonInc
    (
        UINT32          Subchannel,
        UINT32          Method,
        UINT32          Count,
        const UINT32*   pData
    );

    virtual RC WriteHeader
    (
        UINT32 Subchannel,
        UINT32 Method,
        UINT32 Count
    );

    virtual RC WriteNonIncHeader
    (
        UINT32 Subchannel,
        UINT32 Method,
        UINT32 Count
    );

    virtual RC WriteIncOnce
    (
        UINT32          Subchannel,
        UINT32          Method,
        UINT32          Count,
        const UINT32*   pData
    );

    virtual RC WriteImmediate
    (
        UINT32          Subchannel,
        UINT32          Method,
        UINT32          Data
    );

    virtual RC WriteWithSurface
    (
        UINT32           subchannel,
        UINT32           method,
        const Surface2D& surface,
        UINT32           offset,
        UINT32           offsetShift,
        MappingType      mappingType
    );

    virtual RC WriteWithSurfaceHandle
    (
        UINT32      subchannel,
        UINT32      method,
        UINT32      hMem,
        UINT32      offset,
        UINT32      offsetShift,
        MappingType mappingType
    );

    virtual RC WriteNop();
    virtual RC WriteSetSubdevice(UINT32 mask);
    virtual RC WriteCrcChkMethod();
    virtual RC WriteGpCrcChkGpEntry();

    virtual RC WriteAcquireMutex(UINT32, UINT32);
    virtual RC WriteReleaseMutex(UINT32, UINT32);
    virtual RC SetClass(UINT32, UINT32);
    virtual RC SetClassAndWriteMask(UINT32, UINT32, UINT32, UINT32, const UINT32*);
    virtual RC WriteMask(UINT32, UINT32, UINT32, const UINT32*);
    virtual RC WriteDone();

    virtual RC CallSubroutine(UINT64 Offset, UINT32 Size);
    virtual RC InsertSubroutine(const UINT32 *pBuffer, UINT32 count);

    virtual RC SetHwCrcCheckMode(UINT32 Mode);

    virtual RC SetPrivEnable(bool Priv);

    virtual RC SetSyncEnable(bool Sync);

    virtual RC ScheduleChannel(bool Enable);
    virtual RC SetObject(UINT32 Subchannel, LwRm::Handle Handle);
    virtual RC UnsetObject(LwRm::Handle Handle);
    virtual RC SetReference(UINT32 Value);

    virtual RC SetContextDmaSemaphore(LwRm::Handle hCtxDma);
    virtual RC SetSemaphoreOffset(UINT64 Offset);
    virtual void SetSemaphoreReleaseFlags(UINT32 flags);
    virtual UINT32 GetSemaphoreReleaseFlags();
    virtual RC SetSemaphorePayloadSize(SemaphorePayloadSize size);
    virtual SemaphorePayloadSize GetSemaphorePayloadSize();
    virtual RC SemaphoreAcquire(UINT64 Data);
    virtual RC SemaphoreAcquireGE(UINT64 Data);
    virtual RC SemaphoreRelease(UINT64 Data);
    virtual RC SemaphoreReduction(Reduction Op, ReductionType redType, UINT64 Data);
    virtual RC NonStallInterrupt();
    virtual RC Yield(UINT32 Subchannel);

    virtual RC BeginInsertedMethods();
    virtual RC EndInsertedMethods();
    virtual void CancelInsertedMethods();

    virtual RC GetSyncPoint(UINT32*);
    virtual UINT32 ReadSyncPoint(UINT32);
    virtual void IncrementSyncPoint(UINT32);
    virtual RC WriteSyncPointIncrement(UINT32);
    virtual RC WriteSyncPointWait(UINT32, UINT32);

    virtual RC Flush();

    virtual RC WaitForDmaPush(FLOAT64 TimeoutMs);

    virtual RC WaitIdle(FLOAT64 TimeoutMs);
    virtual RC SetLegacyWaitIdleWithRiskyIntermittentDeadlocking(bool Enable);
    virtual bool GetLegacyWaitIdleWithRiskyIntermittentDeadlocking() const;

    virtual void SetDefaultTimeoutMs(FLOAT64 TimeoutMs);
    virtual FLOAT64 GetDefaultTimeoutMs();

    virtual UINT32 GetClass();

    virtual LwRm::Handle GetHandle();

    virtual RC CheckError();

    virtual void SetError(RC Error);
    virtual void ClearError();

    virtual bool DetectNewRobustChannelError() const;

    virtual bool DetectError() const;

    virtual void ClearPushbuffer();

    virtual RC GetCachedPut(UINT32* PutPtr);
    virtual RC SetCachedPut(UINT32 PutPtr);
    virtual RC GetPut(UINT32* PutPtr);
    virtual RC SetPut(UINT32 PutPtr);
    virtual RC GetGet(UINT32* GetPtr);
    virtual RC GetPushbufferSpace(UINT32* PbSpacePtr);

    virtual RC GetCachedGpPut(UINT32* GpPutPtr);
    virtual RC GetGpPut(UINT32* GpPutPtr);
    virtual RC GetGpGet(UINT32* GpGetPtr);
    virtual RC GetGpEntries(UINT32* GpEntriesPtr);
    virtual RC FinishOpenGpFifoEntry(UINT64* pEnd);
    virtual RC GetReference(UINT32* pRefCnt);

    virtual RC SetExternalGPPutAdvanceMode(bool enable);

    virtual UINT32 GetChannelId() const;

    virtual void EnableLogging(bool b);
    virtual bool GetEnableLogging() const;

    virtual void SetIdleChannelsRetries(UINT32 retries);
    virtual UINT32 GetIdleChannelsRetries();

    virtual GpuDevice* GetGpuDevice() const;

    virtual void SetWrapper(Channel* pWrapper);
    virtual Channel* GetWrapper() const;
    virtual AtomChannelWrapper* GetAtomChannelWrapper();
    virtual PmChannelWrapper* GetPmChannelWrapper();
    virtual RunlistChannelWrapper* GetRunlistChannelWrapper();
    virtual SemaphoreChannelWrapper* GetSemaphoreChannelWrapper();

    virtual RC RobustChannelCallback(UINT32 errorLevel,
                                     UINT32 errorType,
                                     void* pData,
                                     void* pRecoveryCallback);

    virtual RC RecoverFromRobustChannelError();
    virtual LwRm* GetRmClient();

    virtual RC RmcGetClassEngineId(LwRm::Handle hObject,
                                   UINT32* pClassEngineId,
                                   UINT32* pClassId, UINT32* pEngineId);
    virtual RC RmcResetChannel(UINT32 EngineId);
    virtual RC RmcGpfifoSchedule(bool Enable);

    virtual RC SetRandomAutoFlushThreshold(const MinMaxSeed &minmaxseed);
    virtual RC SetRandomAutoFlushThresholdGpFifoEntries(const MinMaxSeed &minmaxseed);
    virtual RC SetRandomAutoGpEntryThreshold(const MinMaxSeed &minmaxseed);
    virtual RC SetRandomPauseBeforeGPPutWrite(const MinMaxSeed &minmaxseed);
    virtual RC SetRandomPauseAfterGPPutWrite(const MinMaxSeed &minmaxseed);
    virtual RC SetAutoSchedule(bool);
    virtual bool Has64bitSemaphores() { return false; }
    virtual RC Initialize();

    virtual LwRm::Handle GetVASpace() const;
    virtual UINT32 GetEngineId() const;
    virtual bool GetDoorbellRingEnable() const;
    virtual RC SetDoorbellRingEnable(bool enable);

    virtual bool GetUseBar1Doorbell() const;

private:
    unique_ptr<Channel> m_pChannel;
};

#endif // INCLUDED_TEGRA_THI_CHANNEL_H
