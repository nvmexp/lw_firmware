/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Channel interface.

#pragma once

#ifndef INCLUDED_CHANNEL_H
#define INCLUDED_CHANNEL_H

#include "lwrm.h"
#include "gpu.h"
#include "gpu/utility/surf2d.h"
#include <memory>
#include <vector>

// Old 16-bit windows had a Yield function, modern windows does not,
// and the windows header files have an empty Yield macro to help
// nasty old windows code compile.  Of course that breaks mods...
#if defined(Yield)
 #undef Yield
#endif

class GpuDevice;
class GpuSubdevice;
class AtomChannelWrapper;
class PmChannelWrapper;
class UtlChannelWrapper;
class RcHelper;
class RunlistChannelWrapper;
class SemaphoreChannelWrapper;
class Surface2D;
class Random;
class UsermodeAlloc;
class UserdAlloc;
typedef shared_ptr<UserdAlloc> UserdAllocPtr;

//------------------------------------------------------------------------------
// Channel interface.
//
// This is a pure-virtual interface for MODS channel classes.  All channel types
// must be derived from this class, or more appropriately, the BaseChannel class.
// This pure-virtual interface is also used to create channel wrappers.  See
// chanwrap.h
//
class Channel
{
public:
    enum MappingType
    {
        MapDefault,
        MapPitch,
        MapBlockLinear
    };

    static const UINT32 NumSubchannels = 8;

    //! Gets the engine Id to use for host allocations given a gpu object engine ID
    //! Host object allocations may need to be allocated on an engine ID that is
    //! different from the GPU object.  Lwrrently that is limited to copy engines
    //! If a copy engine happens to be a GRCE then the host engine ID that needs to
    //! be used is the GRAPHICS engine rather than the specific copy engine
    static RC GetHostEngineId(GpuDevice * pGpuDev, LwRm *pLwRm, UINT32 engineId, UINT32 *pHostEng);
    static RC RobustChannelsCodeToModsRC(UINT32 rcrc);

    // LW_FIFO_DMA_SET_SUBDEVICE_MASK_VALUE is 12 bits.
    static const UINT32 AllSubdevicesMask = 0xfff;

    // Channel class is multi client compliant and should never use the
    // LwRmPtr constructor without specifying which client
    DISALLOW_LWRMPTR_IN_CLASS(Channel);

    static const vector<UINT32> FifoClasses;

public:
    virtual ~Channel() { }

    //! Set options for automatic flushing of the channel
    virtual RC SetAutoFlush
    (
        bool AutoFlushEnable,
        UINT32 AutoFlushThreshold
    ) = 0;
    virtual bool GetAutoFlushEnable() const = 0;

    virtual RC SetAutoGpEntry
    (
        bool AutoGpEntryEnable,
        UINT32 AutoGpEntryThreshold
    ) = 0;

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
    ) = 0;
    virtual RC WriteExternalNonInc
    (
        UINT32 **ppExtBuf,
        UINT32 subchannel,
        UINT32 method,
        UINT32 count,
        const UINT32 *pData
    ) = 0;
    virtual RC WriteExternalIncOnce
    (
        UINT32 **ppExtBuf,
        UINT32 subchannel,
        UINT32 method,
        UINT32 count,
        const UINT32 *pData
    ) = 0;
    virtual RC WriteExternalImmediate
    (
        UINT32 **ppExtBuf,
        UINT32 subchannel,
        UINT32 method,
        UINT32 data
    ) = 0;

    //! Write a single method into the channel.
    virtual RC Write
    (
        UINT32 Subchannel,
        UINT32 Method,
        UINT32 Data
    ) = 0;

    //! Write multiple methods to the channel.
    virtual RC Write
    (
        UINT32 Subchannel,
        UINT32 Method,
        UINT32 Count,
        UINT32 Data,   // Add dummy Data parameter to resolve ambiguity between
                       //    Write(UINT32, UINT32, UINT32).
        ... // Rest of Data
    ) = 0;

    //! Write multiple methods to the channel but pass the data as an array
    virtual RC Write
    (
        UINT32          Subchannel,
        UINT32          Method,
        UINT32          Count,
        const UINT32 *  pData
    ) = 0;

    // Write multiple non-incrementing methods to the channel with data as array
    virtual RC WriteNonInc
    (
        UINT32          Subchannel,
        UINT32          Method,
        UINT32          Count,
        const UINT32 *  pData
    ) = 0;

    //! Write a method header, only
    virtual RC WriteHeader
    (
        UINT32 Subchannel,
        UINT32 Method,
        UINT32 Count
    ) = 0;

    virtual RC WriteNonIncHeader
    (
        UINT32 Subchannel,
        UINT32 Method,
        UINT32 Count
    ) = 0;

    //! Write increment-once method
    virtual RC WriteIncOnce
    (
        UINT32          Subchannel,
        UINT32          Method,
        UINT32          Count,
        const UINT32 *  pData
    ) = 0;

    //! Write immediate-data method
    virtual RC WriteImmediate
    (
        UINT32          Subchannel,
        UINT32          Method,
        UINT32          Data
    ) = 0;

    //! Write method with a surface offset as data.
    virtual RC WriteWithSurface
    (
        UINT32           subchannel,
        UINT32           method,
        const Surface2D& surface,
        UINT32           offset,          //!< Offset from the beginning of the surface.
        UINT32           offsetShift = 0, //!< How much the surface address is shifted right after
                                          //!< adding offset to the surface base address.
        MappingType      mappingType = MapDefault //!< How the surface is mapped in lwmap
    ) = 0;

    //! Write method with a surface offset as data.
    //!
    //! This function is strictly for use with CheetAh host1x-based engines.
    //! Having this function enables us to use VIC in memory tests, which don't
    //! use Surface2D.
    virtual RC WriteWithSurfaceHandle
    (
        UINT32      subchannel,
        UINT32      method,
        UINT32      hMem,            //!< Surface physical memory handle
        UINT32      offset,          //!< Offset from the beginning of the surface.
        UINT32      offsetShift = 0, //!< How much the surface address is shifted right after
                                     //!< adding offset to the surface base address.
        MappingType mappingType = MapDefault //!< How the surface is mapped in lwmap
    ) = 0;

    enum SemRelFlags
    {
        FlagSemRelNone     = 0x0,
        FlagSemRelWithTime = 0x1,
        FlagSemRelWithWFI  = 0x2,
        FlagSemRelDefault  = FlagSemRelWithTime |
                             FlagSemRelWithWFI
    };

    enum SemaphorePayloadSize
    {
        SEM_PAYLOAD_SIZE_32BIT
       ,SEM_PAYLOAD_SIZE_64BIT
       ,SEM_PAYLOAD_SIZE_AUTO     // Non-zero upper 32 bits will automatically
                                  // use 64b semaphore operation
       ,SEM_PAYLOAD_SIZE_DEFAULT  // If the channel supports 64 bit semaphores
                                  // they will be used
       ,SEM_PAYLOAD_SIZE_ILWALID
    };

    enum Reduction
    {
        REDUCTION_MIN,
        REDUCTION_MAX,
        REDUCTION_XOR,
        REDUCTION_AND,
        REDUCTION_OR,
        REDUCTION_ADD,
        REDUCTION_INC,
        REDUCTION_DEC
    };

    enum ReductionType
    {
        REDUCTION_SIGNED,
        REDUCTION_UNSIGNED,
    };

    // Write various control methods into the channel.
    virtual RC WriteNop()                                         = 0;
    virtual RC WriteSetSubdevice(UINT32 mask)                     = 0;
    virtual RC WriteCrcChkMethod()                                = 0;
    virtual RC WriteGpCrcChkGpEntry()                             = 0;

    //! Write chip-specific command to flush dirty L2 cache lines.
    //! Also performs sysmembar as a side effect.
    RC WriteL2FlushDirty();

    // Methods for CheetAh channel
    virtual RC WriteAcquireMutex(UINT32, UINT32)                  = 0;
    virtual RC WriteReleaseMutex(UINT32, UINT32)                  = 0;
    virtual RC SetClass(UINT32, UINT32)                           = 0;
    virtual RC SetClassAndWriteMask(UINT32, UINT32, UINT32, UINT32, const UINT32*) = 0;
    virtual RC WriteMask(UINT32, UINT32, UINT32, const UINT32*)   = 0;
    virtual RC WriteDone()                                        = 0;

    //! Execute an arbitrary block of commands as a subroutine.
    //! The subroutine oclwpies a separate gpentry.  Size is in bytes.
    virtual RC CallSubroutine(UINT64 Offset, UINT32 Size)            = 0;

    //! Execute an arbitrary block of commands, without crossing
    //! gpentry bounds or implicitly flushing.  Count is in words.
    virtual RC InsertSubroutine(const UINT32 *pBuffer, UINT32 count) = 0;

    enum HWCrcChkMode
    {
        HWCRCCHKMODE_NONE   = 0x0,
        HWCRCCHKMODE_GP     = 0x1,
        HWCRCCHKMODE_PB     = 0x2,
        HWCRCCHKMODE_MTHD   = 0x4,
    };
    //! Set hw crc check mode for the channel
    virtual RC SetHwCrcCheckMode(UINT32 Mode)                     = 0;

    //! Set the channel into priv or user mode (default is user)
    virtual RC SetPrivEnable(bool Priv)                           = 0;

    //! Set the channel into proceed or wait mode (default is proceed)
    virtual RC SetSyncEnable(bool Sync)                           = 0;

    //! Helper functions for various host methods
    virtual RC ScheduleChannel(bool Enable)                       = 0;
    virtual RC SetObject(UINT32 Subchannel, LwRm::Handle Handle)  = 0;
    virtual RC UnsetObject(LwRm::Handle Handle)                   = 0;
    virtual RC SetReference(UINT32 Value)                         = 0;

    virtual RC SetContextDmaSemaphore(LwRm::Handle hCtxDma)       = 0;
    virtual RC SetSemaphoreOffset(UINT64 Offset)                  = 0;
    virtual void SetSemaphoreReleaseFlags(UINT32 flags)           = 0;
    virtual UINT32 GetSemaphoreReleaseFlags()                     = 0;
    virtual RC SetSemaphorePayloadSize(SemaphorePayloadSize size) = 0;
    virtual SemaphorePayloadSize GetSemaphorePayloadSize()        = 0;
    virtual RC SemaphoreAcquire(UINT64 Data)                      = 0;
    virtual RC SemaphoreAcquireGE(UINT64 Data)                    = 0;
    virtual RC SemaphoreRelease(UINT64 Data)                      = 0;
    virtual RC SemaphoreReduction(Reduction Op, ReductionType redType, UINT64 Data) = 0;
    virtual RC NonStallInterrupt()                                = 0;
    virtual RC Yield(UINT32 Subchannel)                           = 0;

    //! See ChannelWrapper::BeginInsertedMethods()
    virtual RC BeginInsertedMethods()                             = 0;
    //! See ChannelWrapper::EndInsertedMethods()
    virtual RC EndInsertedMethods()                               = 0;
    //! See ChannelWrapper::CancelInsertedMethods()
    virtual void CancelInsertedMethods()                          = 0;

    // Functions for CheetAh engine synchronization using sync points
    virtual RC GetSyncPoint(UINT32*)                              = 0;
    virtual UINT32 ReadSyncPoint(UINT32)                          = 0;
    virtual void IncrementSyncPoint(UINT32)                       = 0;
    virtual RC WriteSyncPointIncrement(UINT32)                    = 0;
    virtual RC WriteSyncPointWait(UINT32, UINT32)                 = 0;

    //! Flush the method(s) and data.
    virtual RC Flush()                                            = 0;

    //! Wait for the hardware to finish pulling all flushed methods.
    virtual RC WaitForDmaPush(FLOAT64 TimeoutMs)                  = 0;

    //! Wait for the hardware to finish exelwting all flushed methods.
    virtual RC WaitIdle(FLOAT64 TimeoutMs)                        = 0;
    virtual RC SetLegacyWaitIdleWithRiskyIntermittentDeadlocking(bool Enable)
                                                                  = 0;
    virtual bool GetLegacyWaitIdleWithRiskyIntermittentDeadlocking() const
                                                                  = 0;

    //! Get and set the default timeout used for functions where no timeout is
    //! explicitly passed in
    virtual void SetDefaultTimeoutMs(FLOAT64 TimeoutMs)           = 0;
    virtual FLOAT64 GetDefaultTimeoutMs()                         = 0;

    //! Return channel class.
    virtual UINT32 GetClass()                                     = 0;

    //! Return channel handle.
    virtual LwRm::Handle GetHandle()                              = 0;

    //! Return a MODS RC if a channel error has oclwrred,
    //! and do cleanup/recovery if needed.
    virtual RC CheckError()                                       = 0;

    //! Sets the error code returned by CheckError().
    virtual void SetError(RC Error)                               = 0;
    virtual void ClearError()                                     = 0;

    //! Return true if the RM has sent a new robust-channel error,
    //! which hasn't been processed by CheckError() yet.
    virtual bool DetectNewRobustChannelError() const              = 0;

    //! Return true if CheckError() would return non-OK.  Don't do any
    //! cleanup/recovery.  Used for polling loops.
    virtual bool DetectError() const                              = 0;

    //! Clear the pushbuffer data, resetting the cached put/get/gpfifo
    //! to their initial values.  Used in RC recovery when the RM
    //! clears the channel data in the GPU to the initial state, and
    //! MODS needs to have matching data in the Channel.
    virtual void ClearPushbuffer()                                = 0;

    //! Raw channel access to things like Get and Put.
    virtual RC GetCachedPut(UINT32 *PutPtr) = 0;
    virtual RC SetCachedPut(UINT32 PutPtr)  = 0;
    virtual RC GetPut(UINT32 *PutPtr)       = 0;
    virtual RC SetPut(UINT32 PutPtr)        = 0;
    virtual RC GetGet(UINT32 *GetPtr)       = 0;
    virtual RC GetPushbufferSpace(UINT32 *PbSpacePtr) = 0;

    virtual RC GetCachedGpPut(UINT32 *GpPutPtr)    = 0;
    virtual RC GetGpPut(UINT32 *GpPutPtr)          = 0;
    virtual RC GetGpGet(UINT32 *GpGetPtr)          = 0;
    virtual RC GetGpEntries(UINT32 *GpEntriesPtr)  = 0;
    virtual RC FinishOpenGpFifoEntry(UINT64* pEnd) = 0;
    virtual RC GetReference(UINT32 *pRefCnt)       = 0;

    // ExternalGPPutAdvanceMode meant for High Priority channel support:
    // Class assumes that Lw506fControl::GPPut pointer will be updated
    // by an external entity.
    // WaitIdle will be checking GPPut==GPGet condition as if
    // Lw506fControl::GPPut was written by the GpFifoChannel class itself.
    virtual RC SetExternalGPPutAdvanceMode(bool enable) = 0;

    //! Get this channel's channel ID.
    virtual UINT32 GetChannelId() const                 = 0;

    //! Enable very verbose channel debug spew (all methods and data, all flushes).
    virtual void EnableLogging(bool b)                  = 0;
    virtual bool GetEnableLogging() const               = 0;

    //! Set the number of retries on calls to LwRm::IdleChannels when
    //! LWRM_MORE_PROCESSING_REQUIRED is returned.
    virtual void SetIdleChannelsRetries(UINT32 retries) = 0;
    virtual UINT32 GetIdleChannelsRetries()             = 0;

    //! Get GpuDevice
    virtual GpuDevice *GetGpuDevice() const             = 0;

    //! Set/Get the wrapper for the channel (the only wrapper that is ever
    //! visible is the outermost wrapper)
    virtual void SetWrapper(Channel *pWrapper)       = 0;
    virtual Channel * GetWrapper() const             = 0;
    virtual AtomChannelWrapper      * GetAtomChannelWrapper() = 0;
    virtual PmChannelWrapper        * GetPmChannelWrapper() = 0;
    virtual RunlistChannelWrapper   * GetRunlistChannelWrapper() = 0;
    virtual SemaphoreChannelWrapper * GetSemaphoreChannelWrapper() = 0;
    virtual UtlChannelWrapper       * GetUtlChannelWrapper() { return nullptr; }

    //! Called from RM ISR if GpuDevice::GetUseRobustChannelCallback
    //! is enabled.  Records the error, and tells the channel to call
    //! RecoverFromRobustChannelError later.
    virtual RC RobustChannelCallback(UINT32 errorLevel,
                                     UINT32 errorType,
                                     void *pData,
                                     void *pRecoveryCallback) = 0;

    //! Puts the channel in a quiescent state after a robust-channel
    //! error.  Called from CheckError().
    virtual RC RecoverFromRobustChannelError() = 0;
    virtual LwRm * GetRmClient() = 0;

    // Wrappers around popular RmControl calls that get redefined in
    // each new channel class
    //
    virtual RC RmcGetClassEngineId(LwRm::Handle hObject,
                                   UINT32 *pClassEngineId,
                                   UINT32 *pClassId, UINT32 *pEngineId) = 0;
    virtual RC RmcResetChannel(UINT32 EngineId) = 0;
    virtual RC RmcGpfifoSchedule(bool Enable) = 0;

    struct MinMaxSeed
    {
        UINT32 Min;
        UINT32 Max;
        UINT32 Seed;
    };
    //! Units: bytes of pushbuffer
    virtual RC SetRandomAutoFlushThreshold(const MinMaxSeed &minmaxseed) = 0;
    //! Units: number of GPFifoEntries
    virtual RC SetRandomAutoFlushThresholdGpFifoEntries(const MinMaxSeed &minmaxseed) = 0;
    //! Units: bytes of pushbuffer
    virtual RC SetRandomAutoGpEntryThreshold(const MinMaxSeed &minmaxseed) = 0;
    //! Units: numer of Platform::Pause calls
    virtual RC SetRandomPauseBeforeGPPutWrite(const MinMaxSeed &minmaxseed) = 0;
    //! Units: numer of Platform::Pause calls
    virtual RC SetRandomPauseAfterGPPutWrite(const MinMaxSeed &minmaxseed) = 0;
    //! true - automatically schedule channel upon next SetObject call.
    //! false - assume the channel is scheduled and do not schedule it.
    virtual RC SetAutoSchedule(bool) = 0;

    virtual bool Has64bitSemaphores() = 0;

    virtual RC Initialize() = 0;

    //! Returns VA space handle associated with the channel
    virtual LwRm::Handle GetVASpace() const = 0;

    //! Newer channels (Ampere +) now require allocation on a specific engine and only
    //! GPU objects that are compatible with that engine may be used on a channel.  Track
    //! the engine that the channel was allocated on so mainly for error checking
    //! purposes
    virtual UINT32 GetEngineId() const = 0;

    virtual bool GetDoorbellRingEnable() const = 0;
    virtual RC SetDoorbellRingEnable(bool enable) = 0;

    virtual bool GetUseBar1Doorbell() const = 0;

protected:
    Channel() { }

private:
    // Non-copyable
    Channel(const Channel&);
    Channel& operator=(const Channel&);
};

class NullChannel : public Channel
{
public:
    virtual RC SetAutoFlush(bool AutoFlushEnable, UINT32 AutoFlushThreshold);
    virtual bool GetAutoFlushEnable() const;
    virtual RC SetAutoGpEntry(bool AutoGpEntryEnable,
                              UINT32 AutoGpEntryThreshold);
    virtual RC WriteExternal(UINT32 **pExtBuf, UINT32 subchannel,
                             UINT32 method, UINT32 count,
                             const UINT32 *pData);
    virtual RC WriteExternalNonInc(UINT32 **ppExtBuf, UINT32 subchannel,
                                   UINT32 method, UINT32 count,
                                   const UINT32 *pData);
    virtual RC WriteExternalIncOnce(UINT32 **ppExtBuf, UINT32 subchannel,
                                    UINT32 method, UINT32 count,
                                    const UINT32 *pData);
    virtual RC WriteExternalImmediate(UINT32 **ppExtBuf, UINT32 subchannel,
                                      UINT32 method, UINT32 data);
    virtual RC Write(UINT32 Subchannel, UINT32 Method, UINT32 Data);
    virtual RC Write(UINT32 Subchannel, UINT32 Method, UINT32 Count,
                     UINT32 Data, ...);
    virtual RC Write(UINT32 Subchannel, UINT32 Method, UINT32 Count,
                     const UINT32 *pData);
    virtual RC WriteNonInc(UINT32 Subchannel, UINT32 Method, UINT32 Count,
                           const UINT32 *pData);
    virtual RC WriteHeader(UINT32 Subchannel, UINT32 Method, UINT32 Count);
    virtual RC WriteNonIncHeader(UINT32 Subchannel, UINT32 Method,
                                 UINT32 Count);
    virtual RC WriteIncOnce(UINT32 Subchannel, UINT32 Method, UINT32 Count,
                            const UINT32 *pData);
    virtual RC WriteImmediate(UINT32 Subchannel, UINT32 Method, UINT32 Data);
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
    virtual RC SetExternalGPPutAdvanceMode(bool enable);
    virtual UINT32 GetChannelId() const;
    virtual void EnableLogging(bool b);
    virtual bool GetEnableLogging() const;
    virtual void SetIdleChannelsRetries(UINT32 retries);
    virtual UINT32 GetIdleChannelsRetries();
    virtual GpuDevice *GetGpuDevice() const;
    virtual void SetWrapper(Channel *pWrapper);
    virtual Channel *GetWrapper() const;
    virtual AtomChannelWrapper *GetAtomChannelWrapper();
    virtual PmChannelWrapper *GetPmChannelWrapper();
    virtual RunlistChannelWrapper *GetRunlistChannelWrapper();
    virtual SemaphoreChannelWrapper *GetSemaphoreChannelWrapper();
    virtual RC RobustChannelCallback(UINT32 errorLevel, UINT32 errorType,
                                     void *pData, void *pRecoveryCallback);
    virtual RC RecoverFromRobustChannelError();
    virtual LwRm * GetRmClient();
    virtual RC RmcGetClassEngineId(LwRm::Handle, UINT32*, UINT32*, UINT32*);
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
    virtual UINT32 GetEngineId() const { return 0U; }
    virtual bool GetDoorbellRingEnable() const;
    virtual RC SetDoorbellRingEnable(bool enable);
    virtual bool GetUseBar1Doorbell() const;
};

//------------------------------------------------------------------------------
// BaseChannel interface.
//
class BaseChannel : public Channel
{
public:
    virtual ~BaseChannel();

    //! Set options for automatic flushing of the channel
    virtual RC SetAutoFlush
    (
        bool AutoFlushEnable,
        UINT32 AutoFlushThreshold
    ) = 0;
    virtual bool GetAutoFlushEnable() const = 0;

    virtual RC SetAutoGpEntry
    (
        bool AutoGpEntryEnable,
        UINT32 AutoGpEntryThreshold
    ) = 0;

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
    ) = 0;
    virtual RC WriteExternalNonInc
    (
        UINT32 **ppExtBuf,
        UINT32 subchannel,
        UINT32 method,
        UINT32 count,
        const UINT32 *pData
    ) = 0;
    virtual RC WriteExternalIncOnce
    (
        UINT32 **ppExtBuf,
        UINT32 subchannel,
        UINT32 method,
        UINT32 count,
        const UINT32 *pData
    ) = 0;
    virtual RC WriteExternalImmediate
    (
        UINT32 **ppExtBuf,
        UINT32 subchannel,
        UINT32 method,
        UINT32 data
    ) = 0;

    //! Write a single method into the channel.
    virtual RC Write
    (
        UINT32 Subchannel,
        UINT32 Method,
        UINT32 Data
    ) = 0;

    //! Write multiple methods to the channel.
    virtual RC Write
    (
        UINT32 Subchannel,
        UINT32 Method,
        UINT32 Count,
        UINT32 Data,   // Add dummy Data parameter to resolve ambiguity between
                       //    Write(UINT32, UINT32, UINT32).
        ... // Rest of Data
    ) = 0;

    //! Write multiple methods to the channel but pass the data as an array
    virtual RC Write
    (
        UINT32          Subchannel,
        UINT32          Method,
        UINT32          Count,
        const UINT32 *  pData
    ) = 0;

    // Write multiple non-incrementing methods to the channel with data as array
    virtual RC WriteNonInc
    (
        UINT32          Subchannel,
        UINT32          Method,
        UINT32          Count,
        const UINT32 *  pData
    ) = 0;

    //! Write a method header, only
    virtual RC WriteHeader
    (
        UINT32 Subchannel,
        UINT32 Method,
        UINT32 Count
    ) = 0;
    virtual RC WriteNonIncHeader
    (
        UINT32 Subchannel,
        UINT32 Method,
        UINT32 Count
    ) = 0;

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
    ) = 0;
    virtual RC WriteWithSurfaceHandle
    (
        UINT32      subchannel,
        UINT32      method,
        UINT32      hMem,
        UINT32      offset,
        UINT32      offsetShift,
        MappingType mappingType
    ) = 0;

    //! Write various control methods into the channel.
    virtual RC WriteNop()                          = 0;
    virtual RC WriteSetSubdevice(UINT32 mask)      = 0;
    virtual RC WriteCrcChkMethod()                 = 0;
    virtual RC WriteGpCrcChkGpEntry()              = 0;

    // Methods for CheetAh channel
    virtual RC WriteAcquireMutex(UINT32, UINT32);
    virtual RC WriteReleaseMutex(UINT32, UINT32);
    virtual RC SetClass(UINT32, UINT32);
    virtual RC SetClassAndWriteMask(UINT32, UINT32, UINT32, UINT32, const UINT32*);
    virtual RC WriteMask(UINT32, UINT32, UINT32, const UINT32*);
    virtual RC WriteDone();

    //! Execute an arbitrary block of commands as a subroutine
    virtual RC CallSubroutine(UINT64 Offset, UINT32 Size) = 0;
    virtual RC InsertSubroutine(const UINT32 *pBuffer, UINT32 count) = 0;

    //! Set hw crc check mode for the channel
    virtual RC SetHwCrcCheckMode(UINT32 Mode) = 0;

    //! Set the channel into priv or user mode (default is user)
    virtual RC SetPrivEnable(bool Priv) = 0;

    //! Set the channel into proceed or wait mode (default is proceed)
    virtual RC SetSyncEnable(bool Sync) = 0;

    //! Helper functions for various host methods
    virtual RC ScheduleChannel(bool Enable);
    virtual RC SetObject
    (
        UINT32 Subchannel,
        LwRm::Handle Handle
    );
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
    virtual RC Flush() = 0;

    //! Wait for the hardware to finish pulling all flushed methods.
    virtual RC WaitForDmaPush
    (
        FLOAT64 TimeoutMs
    ) = 0;

    //! Wait for the hardware to finish exelwting all flushed methods.
    virtual RC WaitIdle
    (
        FLOAT64 TimeoutMs
    );
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

    //! Return a MODS RC if a channel error has oclwrred,
    //! and do cleanup/recovery if needed.
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

    //! Clears the pushbuffer data in the Channel object
    virtual void ClearPushbuffer() = 0;

    //! Raw channel access to things like Get and Put.
    virtual RC GetCachedPut(UINT32 *PutPtr) = 0;
    virtual RC SetCachedPut(UINT32 PutPtr)  = 0;
    virtual RC GetPut(UINT32 *PutPtr)       = 0;
    virtual RC SetPut(UINT32 PutPtr)        = 0;
    virtual RC GetGet(UINT32 *GetPtr)       = 0;
    virtual RC GetPushbufferSpace(UINT32 *PbSpacePtr) = 0;

    virtual RC GetCachedGpPut(UINT32 *GpPutPtr)    = 0;
    virtual RC GetGpPut(UINT32 *GpPutPtr)          = 0;
    virtual RC GetGpGet(UINT32 *GpGetPtr)          = 0;
    virtual RC GetGpEntries(UINT32 *GpEntriesPtr)  = 0;
    virtual RC FinishOpenGpFifoEntry(UINT64* pEnd) = 0;
    virtual RC GetReference(UINT32 *pRefCnt)       = 0;

    // ExternalGPPutAdvanceMode meant for High Priority channel support:
    // Class assumes that Lw506fControl::GPPut pointer will be updated
    // by an external entity.
    // WaitIdle will be checking GPPut==GPGet condition as if
    // Lw506fControl::GPPut was written by the GpFifoChannel class itself.
    virtual RC SetExternalGPPutAdvanceMode(bool enable) = 0;

    //! Get this channel's channel ID.
    virtual UINT32 GetChannelId() const;

    //! Enable very verbose channel debug spew (all methods and data, all flushes).
    virtual void EnableLogging(bool b);
    virtual bool GetEnableLogging() const;

    //! Set the number of retries on calls to LwRm::IdleChannels when
    //! LWRM_MORE_PROCESSING_REQUIRED is returned.
    virtual void SetIdleChannelsRetries(UINT32 retries);
    virtual UINT32 GetIdleChannelsRetries();

    //! Get GpuDevice
    virtual GpuDevice *GetGpuDevice() const { return m_pGrDev; }

    //! Set/Get the wrapper for the channel (the only wrapper that is ever
    //! visible is the outermost wrapper)
    virtual void SetWrapper(Channel *pWrapper) { m_pOutermostWrapper = pWrapper; }
    virtual Channel * GetWrapper() const { return m_pOutermostWrapper; }
    virtual AtomChannelWrapper    * GetAtomChannelWrapper()    { return NULL; }
    virtual PmChannelWrapper      * GetPmChannelWrapper()      { return NULL; }
    virtual RunlistChannelWrapper * GetRunlistChannelWrapper() { return NULL; }
    virtual SemaphoreChannelWrapper * GetSemaphoreChannelWrapper() { return NULL; }

    virtual RC RobustChannelCallback(UINT32 errorLevel, UINT32 errorType,
                                     void *pData, void *pRecoveryCallback);
    virtual RC RecoverFromRobustChannelError();

    virtual LwRm * GetRmClient();
    virtual RC RmcGetClassEngineId(LwRm::Handle, UINT32*, UINT32*, UINT32*);
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
    virtual UINT32 GetEngineId() const { return 0U; }
    virtual bool GetDoorbellRingEnable() const;
    virtual RC SetDoorbellRingEnable(bool enable);
    virtual bool GetUseBar1Doorbell() const;
protected:
    // base-class Constructor
    BaseChannel
    (
        void *        pErrorNotifierMemory,
        LwRm::Handle  hChannel,
        UINT32        ChID,
        UINT32        Class,
        GpuDevice *   pGrDev,
        LwRm *        pLwRm
    );
    virtual RcHelper *GetRcHelper() const;

    LwRm::Handle m_hChannel;
    UINT32       m_ChID;
    UINT32       m_DevInst;
    UINT32       m_Class;
    FLOAT64      m_TimeoutMs;
    unique_ptr<RcHelper> m_pRcHelper;

    //! Enable very verbose channel info dumping.
    bool         m_EnableLogging;

    UINT32       m_IdleChannelsRetries;

    void LogData(UINT32 n, UINT32 * pData);

    GpuDevice * m_pGrDev;

    Channel *m_pOutermostWrapper;

    bool m_PushBufferInFb;

   //! Most recent channel error.  Return value of CheckError.
    RC   m_Error;

    static const UINT32 s_SemaphoreDwords;

private:
    // RM Client where the channel is allocated
    LwRm *m_pLwRm;
};

//------------------------------------------------------------------------------
// Get/Put FIFO channel implementation.
//
// Supports multi-GPU "SLI" devices.
// We assume the pushbuffer and gpfifo buffers are in host memory.
//
class GpFifoChannel : public BaseChannel
{
public:
    GpFifoChannel
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
        LwRm *        pLwRm  = 0
    );
    ~GpFifoChannel();

    virtual RC Initialize();

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

    virtual RC CallSubroutine(UINT64 Offset, UINT32 Size);
    virtual RC SetPrivEnable(bool Priv);
    virtual RC NonStallInterrupt();
    virtual RC Flush();
    virtual RC WaitForDmaPush(FLOAT64 TimeoutMs);

    virtual void ClearPushbuffer();

    virtual RC GetCachedPut(UINT32 *PutPtr);
    virtual RC SetCachedPut(UINT32 PutPtr);
    virtual RC GetPut(UINT32 *PutPtr);
    virtual RC SetPut(UINT32 PutPtr);
    virtual RC GetGet(UINT32 *GetPtr);
    virtual RC GetPushbufferSpace(UINT32 *PbSpacePtr);
    virtual RC GetReference(UINT32 *pRefCnt);

    virtual RC GetCachedGpPut(UINT32 *GpPutPtr);
    virtual RC GetGpPut(UINT32 *GpPutPtr);
    virtual RC GetGpGet(UINT32 *GpGetPtr);
    virtual RC GetGpEntries(UINT32 *GpEntriesPtr);
    virtual RC FinishOpenGpFifoEntry(UINT64* pEnd);
    virtual RC SetExternalGPPutAdvanceMode(bool enable);

    virtual RC RecoverFromRobustChannelError();

    virtual RC SetRandomAutoFlushThreshold(const MinMaxSeed &minmaxseed);
    virtual RC SetRandomAutoFlushThresholdGpFifoEntries(const MinMaxSeed &minmaxseed);
    virtual RC SetRandomAutoGpEntryThreshold(const MinMaxSeed &minmaxseed);
    virtual RC SetRandomPauseBeforeGPPutWrite(const MinMaxSeed &minmaxseed);
    virtual RC SetRandomPauseAfterGPPutWrite(const MinMaxSeed &minmaxseed);

    virtual RC RmcResetChannel(UINT32 EngineId);

    RC SetupGetSemaphore();

    virtual LwRm::Handle GetVASpace() const { return m_hVASpace; }

protected:
    RC MakeRoom(UINT32 Count);
    RC Wrap(UINT32 writeSize);
    UINT32 GetFlushSize() const;
    RC AutoFlushIfNeeded();
    bool GetPrivEnable() const;
    virtual RC WriteGpEntryData(UINT32 GpEntry0, UINT32 GpEntry1);
    virtual RC WritePbCrcCheckGpEntry(const void *pGpEntryData, UINT32 Length);
    virtual RC ConstructGpEntryData(UINT64 Get, UINT32 Length, bool Subroutine,
        UINT32* pGpEntry0, UINT32* pGpEntry1);
    virtual UINT32 GetGpEntry1LengthMask() const;
    virtual RC CommitGpPut(void* controlRegs, UINT08* pGpFifoBase,
            UINT32 startGpPut, UINT32 endGpPut, UINT32* pOutGpPut,
            UINT32 subdev);
    virtual bool IsBar1DummyReadNeeded() { return false; }

    void PauseBeforeGpPutUpdate();
    void PauseAfterGpPutUpdate();
    bool ExternalGPPutAdvance() { return m_ExternalGPPutAdvanceMode; }
    virtual UINT64 ReadGet(UINT32 i) const;
    virtual bool CanUseGetGpGetSemaphore() const;
    bool UseGetSemaphore() const { return m_bUseGetSemaphore; }
    bool UseGpGetSemaphore() const { return m_bUseGpGetSemaphore; }
    void * GetSemaphoreAddress() const { return m_GetSemaphore.GetAddress(); }
    void * GpGetSemaphoreAddress() const { return m_GpGetSemaphore.GetAddress(); }
    UINT64 PushBufferOffset() const { return m_PushBufferOffset; }
    UserdAllocPtr GetUserdAlloc() { return m_pUserdAlloc; }
    virtual void UpdateCachedGpGets();

    // Pointer to where we are lwrrently writing in the pushbuffer
    UINT32 * m_pLwrrent;

    // do we need to wait before fetching this pb?
    bool     m_SyncWait;

    // Pointer to the base of the GPFIFO
    UINT08 * m_pGpFifoBase;

    // Current value of GpPut; keeps track both of whether we have wrapped the
    // GPFIFO and where we are in it
    UINT32   m_LwrrentGpPut;

    // Cached GpPut pointer: this is always the last value we wrote into the
    // channel's GpPut register.
    UINT32   m_CachedGpPut;

    // Cached GpGet pointer: this is the last value we read from the
    // channel's GpGet register (for each subdevice).
    UINT32   m_CachedGpGet[Gpu::MaxNumSubdevices];

    // Pre-callwlated pointers to the get registers, per-subdevice.
    volatile UINT32 *m_pGet[Gpu::MaxNumSubdevices];
    volatile UINT32 *m_pGetHi[Gpu::MaxNumSubdevices];
    volatile UINT32 *m_pGpGet[Gpu::MaxNumSubdevices];

    struct PollArgs
    {
        GpFifoChannel *pThis;
        UINT64 *pCachedGet;
        UINT32 *pCachedGpGet;
        UINT32 LastGetIdx;
        UINT64 Put;
        UINT32 GpPut;
        UINT32 Count;
    };

    PollArgs m_PollArgs;

private:
    virtual UINT32 GetHostSemaphoreSize() const;
    virtual RC WriteGetSemaphore(UINT64 semaOffset,
                                 const UINT08* pPushBufBase) = 0;
    virtual RC SetupGpGetSemaphore();
    virtual RC WriteGpGetSemaphore(UINT64 semaOffset,
                                   UINT32 gpFifoEntries) = 0;

    static bool PollGetEqPut(void * pVoidPollArgs);
    static bool PollGpGetEqGpPut(void * pVoidPollArgs);
    RC WaitForRoom(UINT64 beginPut, UINT64 endPut);
    static bool PollGpFifoFull(void * pVoidPollArgs);

    //! VA space associated with this channel
    LwRm::Handle m_hVASpace;

    // Are we in priv mode right now?
    bool     m_PrivEnable;

    // Pointer to the base of the pushbuffer, and offset of pushbuffer
    // from the start of its context DMA
    UINT08 * m_pPushBufferBase;
    UINT64   m_PushBufferOffset;

    // Size of the pushbuffer and GPFIFO
    UINT32   m_PushBufferSize;
    UINT32   m_GpFifoEntries;

    // Pushbuffer offset where the current GPFIFO entry being assembled starts.
    // May differ from m_CachedPut when we wrap, or add GPFIFO entries
    // without flushing.
    UINT64   m_GpEntryPut;

    // Cached Put pointer: This is the Put pointer immediately after
    // the last flush, before wrapping.
    UINT64   m_CachedPut;

    // Cached Get pointers: these are the last value we read from the
    // channel's Get register (for each subdevice).
    UINT64   m_CachedGet[Gpu::MaxNumSubdevices];

    // Options for automatic flushing
    bool     m_AutoFlushEnable;
    Random * m_pAutoFlushThreshold;
    UINT32   m_AutoFlushThresholdMin;
    UINT32   m_AutoFlushThresholdMax;
    UINT32   GetAutoFlushThreshold();
    Random * m_pAutoFlushThresholdGpFifoEntries;
    UINT32   m_AutoFlushThresholdGpFifoEntriesMin;
    UINT32   m_AutoFlushThresholdGpFifoEntriesMax;
    UINT32   GetAutoFlushThresholdGpFifoEntries();

    bool     m_AutoGpEntryEnable;
    Random * m_pAutoGpEntryThreshold;
    UINT32   m_AutoGpEntryThresholdMin;
    UINT32   m_AutoGpEntryThresholdMax;
    UINT32   GetAutoGpEntryThreshold();

    Random * m_pPauseBeforeGPPutWrite;
    UINT32   m_PauseBeforeGPPutWriteMin;
    UINT32   m_PauseBeforeGPPutWriteMax;

    Random * m_pPauseAfterGPPutWrite;
    UINT32   m_PauseAfterGPPutWriteMin;
    UINT32   m_PauseAfterGPPutWriteMax;

    // Pointer to where the next automatic flush should occur.
    UINT32 * m_pNextAutoFlush;

    // Pointer to where the next automatic GpEntry should occur.
    UINT32 * m_pNextAutoGpEntry;

    RC WriteGpEntry(UINT64 Get, UINT32 Length, bool Subroutine);
    virtual RC WriteGpPut();

    GpFifoChannel(const GpFifoChannel &);                 // not implemented
    GpFifoChannel & operator=(const GpFifoChannel &);     // not implemented

    void UpdateCachedGets();
    UINT32 CalcAutoFlushThresholdGpFifoEntries();

    bool     m_ExternalGPPutAdvanceMode;

    UINT64 m_PbBaseInLastWrittenGpEntry; // pb base in last written GpEntry
    UINT32 m_PbSizeInLastWrittenGpEntry; // pb size in last written GpEntry

    // This allows us to use a semaphore to track the get pointer instead
    // of reading the get pointer from USERD.
    bool          m_bUseGetSemaphore;
    bool          m_bUseGpGetSemaphore;
    Surface2D     m_GetSemaphore;
    Surface2D     m_GpGetSemaphore;
    UserdAllocPtr m_pUserdAlloc;

    // Saved pointer for avoiding writing get/gpget semaphores back to back
    UINT32*        m_pSavedLwrrent;
};

//------------------------------------------------------------------------------
// Fermi GPFIFO implementation.
//
class FermiGpFifoChannel : public GpFifoChannel
{
public:
    FermiGpFifoChannel
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
        GpuDevice    *pGrDev,
        LwRm         *pLwRm    = 0
    );
    virtual ~FermiGpFifoChannel();

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
        ... // Rest of Data
    );

    virtual RC Write
    (
        UINT32         Subchannel,
        UINT32         Method,
        UINT32         Count,
        const UINT32 * Data
    );

    virtual RC WriteNonInc
    (
        UINT32          Subchannel,
        UINT32          Method,
        UINT32          Count,
        const UINT32 *  pData
    );

    virtual RC WriteIncOnce
    (
        UINT32          Subchannel,
        UINT32          Method,
        UINT32          Count,
        const UINT32 *  pData
    );
    virtual RC WriteImmediate
    (
        UINT32          Subchannel,
        UINT32          Method,
        UINT32          Data
    );

    virtual RC WriteHeader
    (
        UINT32          Subchannel,
        UINT32          Method,
        UINT32          Count
    );
    virtual RC WriteNonIncHeader
    (
        UINT32          Subchannel,
        UINT32          Method,
        UINT32          Count
    );

    virtual RC WriteNop();
    virtual RC WriteSetSubdevice(UINT32 Subdevice);
    virtual RC InsertSubroutine(const UINT32 *pBuffer, UINT32 count);
    virtual RC WriteCrcChkMethod();
    virtual RC WriteGpCrcChkGpEntry();
    virtual RC SetObject(UINT32 Subchannel, LwRm::Handle Handle);
    virtual RC SetContextDmaSemaphore(LwRm::Handle hCtxDma);
    virtual void SetSemaphoreReleaseFlags(UINT32 flags);
    virtual UINT32 GetSemaphoreReleaseFlags();
    virtual RC SetSemaphorePayloadSize(SemaphorePayloadSize size);
    virtual SemaphorePayloadSize GetSemaphorePayloadSize();
    virtual RC SetSemaphoreOffset(UINT64 Offset);
    virtual RC SemaphoreAcquire(UINT64 Data);
    virtual RC SemaphoreAcquireGE(UINT64 Data);
    virtual RC SemaphoreRelease(UINT64 Data);

    virtual RC SetSyncEnable(bool Sync);

    virtual RC WaitIdle(FLOAT64 TimeoutMs);
    virtual RC SetLegacyWaitIdleWithRiskyIntermittentDeadlocking(bool Enable);
    virtual bool GetLegacyWaitIdleWithRiskyIntermittentDeadlocking() const;

    virtual RC SetHwCrcCheckMode(UINT32 Mode);

    virtual RC RmcGetClassEngineId(LwRm::Handle hObject,
                                   UINT32 *pClassEngineId,
                                   UINT32 *pClassId, UINT32 *pEngineId);
    virtual RC RmcResetChannel(UINT32 EngineId);

protected:
    virtual RC WriteGpEntryData(UINT32 GpEntry0, UINT32 GpEntry1);
    virtual RC WritePbCrcCheckGpEntry(const void *pGpEntryData, UINT32 Length);
    RC ConstructGpEntryData(UINT64 Get, UINT32 Length, bool Subroutine,
        UINT32* pGpEntry0, UINT32* pGpEntry1) override;
    virtual UINT32 GetGpEntry1LengthMask() const;
    RC CallwlatePbMthdCrc(const UINT32 *pData, UINT32 Count);
    UINT64 GetHostSemaOffset() const { return m_HostSemaOffset; }

private:
    FermiGpFifoChannel(const FermiGpFifoChannel &);                 // not implemented
    FermiGpFifoChannel & operator=(const FermiGpFifoChannel &);     // not implemented

    virtual UINT32 GetHostSemaphoreSize() const;
    virtual RC WriteGetSemaphore(UINT64 semaOffset, const UINT08* pPushBufBase);
    virtual RC WriteGpGetSemaphore(UINT64 semaOffset, UINT32 gpFifoEntries);
    virtual RC WriteGetSemaphoreMethods(UINT64 semaOffset, const UINT08* pPushBufBase);
    RC WriteExternalImpl(UINT32 **ppExtBuf, UINT32 header,
                         UINT32 count, const UINT32 *pData);

    static bool PollIdleSemaphore(void *pvArgs);
    UINT32               m_SemRelFlags;
    SemaphorePayloadSize m_SemaphorePayloadSize;
    unique_ptr<Surface2D> m_pWaitIdleSem;
    enum { WAIT_IDLE_SEM_SIZE = 16 };
    UINT32         m_WaitIdleValue;
    bool           m_UseLegacyWaitIdleWithRiskyIntermittentDeadlocking;

    UINT32         m_HwCrcCheckMode;
    UINT32         m_GpCrc;   //! Crc value of Gp buffer
    UINT32         m_PbCrc;   //! Crc value of Pb buffer
    UINT32         m_MethodCrc; //! Crc value of method to crossbar
    UINT64         m_HostSemaOffset; // Host semaphore offset
};

//------------------------------------------------------------------------------
// Kepler GPFIFO implementation.
//
class KeplerGpFifoChannel : public FermiGpFifoChannel
{
public:
    KeplerGpFifoChannel
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
        GpuDevice    *pGrDev,
        LwRm         *pLwRm    = 0,
        Tsg          *pTsg     = 0
    );
    virtual ~KeplerGpFifoChannel();

    virtual RC ScheduleChannel(bool Enable);
    virtual RC SetObject(UINT32 Subchannel, LwRm::Handle Handle);
    virtual RC SemaphoreReduction(Reduction Op, ReductionType redType, UINT64 Data);
    virtual RC WaitIdle(FLOAT64 TimeoutMs);
    virtual RC SetAutoSchedule(bool bAutoSched);
    virtual RC CheckError();
    virtual RC RmcGetClassEngineId(LwRm::Handle hObject,
                                   UINT32 *pClassEngineId,
                                   UINT32 *pClassId, UINT32 *pEngineId);
    virtual RC RmcResetChannel(UINT32 EngineId);
    virtual RC RmcGpfifoSchedule(bool Enable);

protected:
    virtual RcHelper *GetRcHelper() const;

private:
    KeplerGpFifoChannel(const KeplerGpFifoChannel &);                 // not implemented
    KeplerGpFifoChannel & operator=(const KeplerGpFifoChannel &);     // not implemented

    bool m_NeedsSchedule;

    Tsg *m_pTsg;
};

//------------------------------------------------------------------------------
// Volta GPFIFO implementation.
//
class VoltaGpFifoChannel : public KeplerGpFifoChannel
{
public:
    VoltaGpFifoChannel
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
        LwRm *        pLwRm    = 0,
        Tsg *         pTsg     = 0,
        UINT32        engineId = 0
    );
    virtual ~VoltaGpFifoChannel();
    virtual RC Initialize();

    virtual RC SemaphoreAcquire(UINT64 Data);
    virtual RC SemaphoreAcquireGE(UINT64 Data);
    virtual RC SemaphoreRelease(UINT64 Data);
    virtual RC SemaphoreReduction(Reduction Op, ReductionType redType, UINT64 Data);

    virtual RC RmcGetClassEngineId(LwRm::Handle hObject,
                                   UINT32 *pClassEngineId,
                                   UINT32 *pClassId, UINT32 *pEngineId);
    virtual RC RmcResetChannel(UINT32 EngineId);
    virtual RC RmcGpfifoSchedule(bool Enable);
    virtual bool Has64bitSemaphores() { return true; }
    virtual UINT32 GetEngineId() const { return m_EngineId; }
    virtual bool GetDoorbellRingEnable() const;
    virtual RC SetDoorbellRingEnable(bool enable);
protected:
    //--------------------------------------------------------------------------
    // Class used to ring the doorbell that notifies host that GP_PUT has been
    // updated
    //
    class DoorbellControl
    {
        public:
            DoorbellControl();
            ~DoorbellControl() { Free(); }
            RC Alloc(GpuSubdevice *pSubDev, LwRm * pLwRm, LwRm::Handle hCh, bool useBar1Alloc);
            void Free();
            RC   Ring();
            bool IsInitialized() { return m_pDoorbell != nullptr; }
        private:
            DoorbellControl(const DoorbellControl &) = delete;
            DoorbellControl & operator=(const DoorbellControl &) = delete;

            static const UINT32 ILWALID_RING_TOKEN = _UINT32_MAX;
            LwRm *            m_pLwRm;
            GpuSubdevice *    m_pSubdev;
            LwRm::Handle      m_hCh;
            UsermodeAlloc *   m_pUsermode;
            void *            m_pUserModeControl;
            volatile UINT32 * m_pDoorbell;
            UINT32            m_RingToken;
    };
    typedef unique_ptr<DoorbellControl> DoorbellControlPtr;

    virtual UINT64 ReadGet(UINT32 i) const;
    RC Bar1FlushSurfaceRead(UINT32 subdevIdx, GpuSubdevice *pSubdev);
private:
    VoltaGpFifoChannel(const VoltaGpFifoChannel &);                 // not implemented
    VoltaGpFifoChannel & operator=(const VoltaGpFifoChannel &);     // not implemented

    virtual RC WriteGpPut();
    virtual UINT32 GetHostSemaphoreSize() const;
    virtual RC WriteGpGetSemaphore(UINT64 semaOffset, UINT32 gpFifoEntries);
    virtual RC WriteGetSemaphoreMethods(UINT64 semaOffset, const UINT08* pPushBufBase);

    RC GetActualSemaphorePayloadSize(UINT64 data, SemaphorePayloadSize *pSize);

    vector<DoorbellControlPtr> m_DoorbellControl;
    vector<Memory::Location>   m_ControlRegLocs;
    unique_ptr<Surface2D>      m_pBar1FlushSurface;
    vector<void *>             m_pBar1FlushAddr;
    UINT32                     m_EngineId;
    bool                       m_IsDoorbellEnabled = true;
};

//------------------------------------------------------------------------------
// Ampere GPFIFO implementation.
//
class AmpereGpFifoChannel : public VoltaGpFifoChannel
{
public:
    AmpereGpFifoChannel
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
        LwRm *        pLwRm    = 0,
        Tsg *         pTsg     = 0,
        UINT32        engineId = 0
    );
    virtual ~AmpereGpFifoChannel() { }
    RC Yield(UINT32 Subchannel) override { return RC::UNSUPPORTED_HARDWARE_FEATURE; }
    RC SetHwCrcCheckMode(UINT32 Mode) override;
};

//------------------------------------------------------------------------------
// Hopper GPFIFO implementation.
//
class HopperGpFifoChannel : public AmpereGpFifoChannel
{
public:
    HopperGpFifoChannel
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
        LwRm *        pLwRm    = 0,
        Tsg *         pTsg     = 0,
        UINT32        engineId = 0,
        bool          useBar1Doorbell = false
    );
    virtual ~HopperGpFifoChannel() { }

    virtual bool GetUseBar1Doorbell() const;

private:
    RC ConstructGpEntryData(UINT64 Get, UINT32 Length, bool Subroutine,
        UINT32* pGpEntry0, UINT32* pGpEntry1) override;
    RC WriteExtendedBase(UINT64 Get);
    bool IsBar1DummyReadNeeded() override;

    UINT32 m_LwrrentExtendedBase = 0;
    bool m_UseBar1Doorbell = false;
};

//------------------------------------------------------------------------------
// Blackwell GPFIFO implementation.
//
class BlackwellGpFifoChannel : public HopperGpFifoChannel
{
public:
    BlackwellGpFifoChannel
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
        LwRm *        pLwRm    = 0,
        Tsg *         pTsg     = 0,
        UINT32        engineId = 0,
        bool          useBar1Doorbell = false
    );
    virtual ~BlackwellGpFifoChannel() { }
    virtual RC GetReference(UINT32 *pRefCnt);

protected: 
    virtual bool CanUseGetGpGetSemaphore() const { return true; }
};

//------------------------------------------------------------------------------
// Kepler GPFIFO AMODEL implementation.
//
class AmodelGpFifoChannel : public KeplerGpFifoChannel
{
public:
    AmodelGpFifoChannel
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
        LwRm *        pLwRm    = 0,
        Tsg *         pTsg     = 0,
        UINT32        engineId = 0
    );
    virtual ~AmodelGpFifoChannel();
    virtual RC Initialize();
    virtual UINT32 GetEngineId() const { return m_EngineId; }

protected:
    virtual UINT64 ReadGet(UINT32 i) const;
    virtual void UpdateCachedGpGets();
    virtual RC CommitGpPut(void* controlRegs, UINT08* pGpFifoBase,
            UINT32 startGpPut, UINT32 endGpPut, UINT32* pOutGpPut,
            UINT32 subdev);
    virtual bool CanUseGetGpGetSemaphore() const { return true; }
private:
    AmodelGpFifoChannel(const AmodelGpFifoChannel &);                 // not implemented
    AmodelGpFifoChannel & operator=(const AmodelGpFifoChannel &);     // not implemented

    RC ConstructGpEntryData
    (
        UINT64 get,
        UINT32 length,
        bool subroutine,
        UINT32* pGpEntry0,
        UINT32* pGpEntry1
    ) override;

    RC WriteExtendedBase(UINT64 get);

    UINT32 m_EngineId;
    UINT32 m_LwrrentExtendedBase = 0;
};

#endif // ! INCLUDED_CHANNEL_H
