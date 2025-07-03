/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2018, 2020-2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Channel interface.

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef INCLUDED_CHANWRAP_H
#define INCLUDED_CHANWRAP_H

#ifndef INCLUDED_CHANNEL_H
#include "channel.h"
#endif

//------------------------------------------------------------------------------
// ChannelWrapper interface.
//
// This class implements a generic pass-through wrapper for MODS channels.  I.e.
// All functions within this class simply call the appropriate API of the
// wrapped channel.  In general, all channel wrappers should be derived from
// this class.
//
// The purpose of this class is to provide a means for altering the functionality
// of a MODS channel by "wrapping" the actual MODS channel with a class derived
// from this one.  Terminology and Guidelines for usage:
//
//   1.  Multiple wrappers may be applied to a MODS channel, however only one
//       wrapper class may contain a pointer to the actual MODS channel.  This
//       wrapper is called the "innermost" wrapper.
//   2.  Wrappers after the first should wrap the last wrapper that was
//       applied.  In the following example a channel is wrapped with Wrapper1
//       and then wrapped again with Wrapper2 :
//            pWrappedChannel = new Wrapper1(pChannel);
//            pWrappedChannel = new Wrapper2(pWrappedChannel);
//   3.  *** CRITICAL *** A wrapper whose outermost wrapper pointer is NULL is the
//       "outermost" wrapper.  All function calls which ultimately will affect the
//       actual MODS channel MUST call through the outermost wrapper.  The
//       GetWrapper() API will always return a pointer to the outermost wrapper.
//
class ChannelWrapper : public Channel
{
public:
    ChannelWrapper(Channel *pChannel);
    virtual ~ChannelWrapper();

    //! Set options for automatic flushing of the channel
    virtual RC SetAutoFlush(bool AutoFlushEnable, UINT32 AutoFlushThreshold);
    virtual bool GetAutoFlushEnable() const;
    virtual RC SetAutoGpEntry(bool AutoFlushEnable,
                              UINT32 AutoGpEntryThreshold);

    //! WriteExternal -- format a method into an external buffer.
    //! If pData is nullptr, then just write the header.
    //! On exit, update *ppExtBuf to point at the new end of buffer.
    //! Caller must ensure there is enough space in the buffer.
    virtual RC WriteExternal
    (
        UINT32 **ppExtBuf,
        UINT32 subchannel,
        UINT32 method,
        UINT32 count,
        const UINT32 *pData
    );
    virtual RC WriteExternalNonInc
    (
        UINT32 **ppExtBuf,
        UINT32 subchannel,
        UINT32 method,
        UINT32 count,
        const UINT32 *pData
    );
    virtual RC WriteExternalIncOnce
    (
        UINT32 **ppExtBuf,
        UINT32 subchannel,
        UINT32 method,
        UINT32 count,
        const UINT32 *pData
    );
    virtual RC WriteExternalImmediate
    (
        UINT32 **ppExtBuf,
        UINT32 subchannel,
        UINT32 method,
        UINT32 data
    );

    //! Write a single method into the channel.
    virtual RC Write
    (
        UINT32 Subchannel,
        UINT32 Method,
        UINT32 Data
    );

    //! Write multiple methods to the channel.
    virtual RC Write
    (
        UINT32 Subchannel,
        UINT32 Method,
        UINT32 Count,
        UINT32 Data,   // Add dummy Data parameter to resolve ambiguity between
                       //    Write(UINT32, UINT32, UINT32).
        ... // Rest of Data
    );

    //! Write multiple methods to the channel but pass the data as an array
    virtual RC Write
    (
        UINT32          Subchannel,
        UINT32          Method,
        UINT32          Count,
        const UINT32 *  pData
    );

    // Write multiple non-incrementing methods to the channel with data as array
    virtual RC WriteNonInc
    (
        UINT32          Subchannel,
        UINT32          Method,
        UINT32          Count,
        const UINT32 *  pData
    );

    //! Write a method header, only
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

    //! Write increment-once method
    virtual RC WriteIncOnce
    (
        UINT32          Subchannel,
        UINT32          Method,
        UINT32          Count,
        const UINT32 *  pData
    );

    //! Write immediate-data method
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

    //! Write various control methods into the channel.
    virtual RC WriteNop();
    virtual RC WriteSetSubdevice(UINT32 mask);
    virtual RC WriteCrcChkMethod();
    virtual RC WriteGpCrcChkGpEntry();

    // Methods for CheetAh channel
    virtual RC WriteAcquireMutex(UINT32, UINT32);
    virtual RC WriteReleaseMutex(UINT32, UINT32);
    virtual RC SetClass(UINT32, UINT32);
    virtual RC SetClassAndWriteMask(UINT32, UINT32, UINT32, UINT32, const UINT32*);
    virtual RC WriteMask(UINT32, UINT32, UINT32, const UINT32*);
    virtual RC WriteDone();

    //! Execute an arbitrary block of commands as a subroutine
    virtual RC CallSubroutine(UINT64 Offset, UINT32 Size);
    virtual RC InsertSubroutine(const UINT32 *pBuffer, UINT32 count);

    //! Set hw crc check mode for the channel
    virtual RC SetHwCrcCheckMode(UINT32 Mode);

    //! Set the channel into priv or user mode (default is user)
    virtual RC SetPrivEnable(bool Priv);

    //! Set the channel into proceed or wait mode (default is proceed)
    virtual RC SetSyncEnable(bool Sync);

    //! Helper functions for various host methods
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

    // Functions for CheetAh engine synchronization using sync points
    virtual RC GetSyncPoint(UINT32*);
    virtual UINT32 ReadSyncPoint(UINT32);
    virtual void IncrementSyncPoint(UINT32);
    virtual RC WriteSyncPointIncrement(UINT32);
    virtual RC WriteSyncPointWait(UINT32, UINT32);

    //! Flush the method(s) and data.
    virtual RC Flush();

    //! Wait for the hardware to finish pulling all flushed methods.
    virtual RC WaitForDmaPush(FLOAT64 TimeoutMs);

    //! Wait for the hardware to finish exelwting all flushed methods.
    virtual RC WaitIdle(FLOAT64 TimeoutMs);
    virtual RC SetLegacyWaitIdleWithRiskyIntermittentDeadlocking(bool Enable);
    virtual bool GetLegacyWaitIdleWithRiskyIntermittentDeadlocking() const;

    //! Get and set the default timeout used for functions where no timeout is
    //! explicitly passed in
    virtual void SetDefaultTimeoutMs(FLOAT64 TimeoutMs);
    virtual FLOAT64 GetDefaultTimeoutMs();

    //! Return channel class.
    virtual UINT32 GetClass();

    //! Return channel handle.
    virtual LwRm::Handle GetHandle();

    //! Return a MODS RC if a channel error has oclwrred.
    virtual RC CheckError();

    //! Sets the error code returned by CheckError().
    virtual void SetError(RC Error);
    virtual void ClearError();

    //! Return true if the RM has sent a new robust-channel error,
    //! which hasn't been processed by CheckError() yet.
    virtual bool DetectNewRobustChannelError() const;

    //! Return true if CheckError() would return non-OK.  Don't do any
    //! cleanup/recovery.  Used for polling loops.
    virtual bool DetectError() const;

    virtual void ClearPushbuffer();

    //! Raw channel access to things like Get and Put.
    virtual RC GetCachedPut(UINT32 *PutPtr);
    virtual RC SetCachedPut(UINT32 PutPtr);
    virtual RC GetPut(UINT32 *PutPtr);
    virtual RC SetPut(UINT32 PutPtr);
    virtual RC GetGet(UINT32 *GetPtr);
    virtual RC GetPushbufferSpace(UINT32 *PbSpacePtr);

    virtual RC GetCachedGpPut(UINT32 *GpPutPtr);
    virtual RC GetGpPut(UINT32 *GpPutPtr);
    virtual RC GetGpGet(UINT32 *GpGetPtr);
    virtual RC GetGpEntries(UINT32 *GpEntriesPtr);
    virtual RC FinishOpenGpFifoEntry(UINT64* pEnd);
    virtual RC GetReference(UINT32 *pRefCnt);

    // ExternalGPPutAdvanceMode meant for High Priority channel support:
    // Class assumes that Lw506fControl::GPPut pointer will be updated
    // by an external entity.
    // WaitIdle will be checking GPPut==GPGet condition as if
    // Lw506fControl::GPPut was written by the GpFifoChannel class itself.
    virtual RC SetExternalGPPutAdvanceMode(bool enable);

    //! Get this channel's channel ID.
    virtual UINT32 GetChannelId() const;

    //! Enable very verbose channel debug spew (all methods and data, all flushes).
    virtual void EnableLogging(bool b);
    virtual bool GetEnableLogging() const;

    //! Set the number of retries on calls to LwRm::IdleChannels when
    //! LWRM_MORE_PROCESSING_REQUIRED is returned.
    virtual void SetIdleChannelsRetries(UINT32 retries);
    virtual UINT32 GetIdleChannelsRetries();

    virtual GpuDevice *GetGpuDevice() const;

    //! Set/Get the wrapper for the channel (the only wrapper that is ever
    //! visible is the outermost wrapper)
    virtual void SetWrapper(Channel *pWrapper);
    virtual Channel * GetWrapper() const { return  m_pOutermostWrapper; }
    virtual AtomChannelWrapper * GetAtomChannelWrapper();
    virtual PmChannelWrapper * GetPmChannelWrapper();
    virtual RunlistChannelWrapper * GetRunlistChannelWrapper();
    virtual SemaphoreChannelWrapper * GetSemaphoreChannelWrapper();
    virtual UtlChannelWrapper * GetUtlChannelWrapper();

    virtual RC RobustChannelCallback(UINT32 errorLevel, UINT32 errorType,
                                     void *pData, void *pRecoveryCallback);
    virtual LwRm * GetRmClient();
    virtual RC RmcGetClassEngineId(LwRm::Handle hObject,
                                   UINT32 *pClassEngineId,
                                   UINT32 *pClassId, UINT32 *pEngineId);
    virtual RC RmcResetChannel(UINT32 EngineId);
    virtual RC RmcGpfifoSchedule(bool Enable);

    virtual RC SetRandomAutoFlushThreshold(const MinMaxSeed &minmaxseed);
    virtual RC SetRandomAutoFlushThresholdGpFifoEntries(const MinMaxSeed &minmaxseed);
    virtual RC SetRandomAutoGpEntryThreshold(const MinMaxSeed &minmaxseed);
    virtual RC SetRandomPauseBeforeGPPutWrite(const MinMaxSeed &minmaxseed);
    virtual RC SetRandomPauseAfterGPPutWrite(const MinMaxSeed &minmaxseed);
    virtual RC SetAutoSchedule(bool);
    virtual bool Has64bitSemaphores();
    virtual RC Initialize();

    virtual LwRm::Handle GetVASpace() const { return m_pWrappedChannel->GetVASpace(); }
    virtual UINT32 GetEngineId() const { return m_pWrappedChannel->GetEngineId(); }
    virtual bool GetDoorbellRingEnable() const;
    virtual RC SetDoorbellRingEnable(bool enable);

    virtual bool GetUseBar1Doorbell() const;

protected:
    virtual RC RecoverFromRobustChannelError();

    Channel *m_pWrappedChannel;
    Channel *m_pOutermostWrapper;
};

#endif // INCLUDED_CHANWRAP_H
