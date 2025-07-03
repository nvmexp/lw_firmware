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

// LW resource manager architecture interface.

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef INCLUDE_GPU
#include "stub/rm_stub.h"
#include "stub/gpu_stub.h"
#else

#ifndef INCLUDED_LWRM_H
#define INCLUDED_LWRM_H

#include "core/include/memtypes.h"
#include "core/include/lwrm_handles.h"
#include "core/include/rc.h"
#include <map>
#include <set>
#include <vector>
#include <memory>

#include "ctrl/ctrl0000/ctrl0000gpu.h"
#include "ctrl/ctrl0080.h"
#include "class/cl0080.h"
#include "lwos.h"
class Channel;
class DisplayChannel;
class GpuDevice;
class GpuSubdevice;
class Tsg;
class TegraRm;
class UserdAlloc;
typedef shared_ptr<UserdAlloc> UserdAllocPtr;

/**
 * @class(LwRm)
 *
 * Manages resource manager handles for the rest of mods.
 *
 * Provides simpler interface to RM functions (supplies handle arguments, etc).
 * Maps RM error codes to RC.
 * Generates guaranteed-not-to-collide object handle IDs.
 * Frees RM resources on mods shutdown.
 */

class LwRm
{
public:
    // TYPES
    typedef UINT32                    Handle;
    typedef set<Handle>               Handles;
    typedef map<Handle, Channel *>    Channels;
    typedef map<Handle, DisplayChannel *>    DisplayChannels;
    enum class Lib : UINT32
    {
        RMAPI,
        DISP_RMAPI,
        ILWALID_LIB
    };
    //------------------------------------------------------------------------
    // ACCESSORS
    //------------------------------------------------------------------------

    // Based on class Gpu's current device, subdevice.
    Handle    GetClientHandle() const;
    Handle    GetDeviceHandle(const GpuDevice *gpudev) const;
    Handle    GetDeviceHandle(UINT32 inst) const;
    Handle    GetSubdeviceHandle(const GpuSubdevice *gpusubdev) const;

    Channel * GetChannel
    (
        Handle ChannelHandle
    );

    DisplayChannel * GetDisplayChannel
    (
        Handle ChannelHandle
    );

    void *GetMutex() { return m_Mutex; }

    //------------------------------------------------------------------------
    // Architecture interface.
    // These are only called by class Gpu.
    //------------------------------------------------------------------------

    // Alloc the client handle.
    RC AllocRoot ();

    RC AllocDevice
    (
        UINT32 DeviceReference
    );

    RC AllocDevice
    (
        void  *pvAllocParams
    );

    RC AllocSubDevice
    (
        UINT32 DeviceReference,
        UINT32 SubDeviceReference
    );

    bool IsFreed() const;

#if defined(TEGRA_MODS)
    RC DispRmInit(GpuDevice *pGpudev);
    Handle GetDispClientHandle() const { return m_DisplayClient; }
    RC GetTegraMemFd(Handle memoryHandle, INT32 *pMemFd);
#endif

    RC FreeClient();

    //------------------------------------------------------------------------
    // These are called from all over mods.
    //
    // The client and device handle arguments are supplied internally
    // based on class Gpu's current device/subdevice.
    //------------------------------------------------------------------------
    RC Alloc
    (
        Handle   ParentHandle,
        Handle * pRtnObjectHandle,
        UINT32   Class,
        void *   Parameters
    );

    RC AllocContextDma
    (
        Handle * pRtnDmaHandle,
        UINT32   Flags,
        UINT32   Memory,
        UINT64   Offset,
        UINT64   Limit
    );

#if defined(TEGRA_MODS)
    RC AllocDisplayContextDma
    (
        Handle * pRtnDmaHandle,
        UINT32   Flags,
        UINT32   Memory,
        UINT64   Offset,
        UINT64   Limit
    );

    RC AllocDisp
    (
        Handle   ParentHandle,
        Handle * pRtnObjectHandle,
        UINT32   Class,
        void *   Parameters
    );
#endif

    RC AllocChannelGpFifo
    (
        Handle * pRtnChannelHandle,
        UINT32   Class,
        Handle   ErrorMemHandle,
        void *   pErrorNotifierMemory,
        Handle   DataMemHandle,
        void *   pPushBufferBase,
        UINT64   PushBufferOffset,
        UINT32   PushBufferSize,
        void *   pGpFifoBase,
        UINT64   GpFifoOffset,
        UINT32   GpFifoEntries,
        Handle   ContextSwitchObjectHandle,
        UINT32   VerifFlags,
        UINT32   VerifFlags2,
        UINT32   Flags,
        GpuDevice  *gpudev,
        Tsg        *pTsg = nullptr,
        UINT32      hVASpace = 0,
        UINT32      engineId = 0,
        bool        useBar1Doorbell = false,
        UserdAllocPtr pUserdAlloc = UserdAllocPtr()
    );

    RC AllocDisplayChannelPio
    (
        Handle   ParentHandle,
        Handle * pRtnChannelHandle,
        UINT32   Class,
        UINT32   channelInstance,
        Handle   ErrorContextHandle,
        void *   pErrorNotifierMemory,
        UINT32   Flags,
        GpuDevice *gpudev
    );
    RC AllocDisplayChannelDma
    (
        Handle   ParentHandle,
        Handle * pRtnChannelHandle,
        UINT32   Class,
        UINT32   channelInstance,
        Handle   ErrorContextHandle,
        void *   pErrorNotifierMemory,
        Handle   DataContextHandle,
        void *   pPushBufferBase,
        UINT32   PushBufferSize,
        UINT32   Offset,
        UINT32   Flags,
        GpuDevice *gpudev
    );
    RC SwitchToDisplayChannel
    (
        Handle ParentHandle,
        Handle DisplayChannelHandle,
        UINT32 Class,
        UINT32 Head,
        Handle ErrorContextHandle,
        Handle DataContextHandle,
        UINT32 Offset = 0,
        UINT32 Flags = 0
    );

    RC AllocMemory
    (
        Handle *  pRtnMemoryHandle,
        UINT32    Class,
        UINT32    Flags,
        void   ** pRtnAddress,
        UINT64 *  pLimit,
        GpuDevice *gpudev
    );

    RC AllocSystemMemory
    (
        Handle *  pRtnMemoryHandle,
        UINT32    Flags,
        UINT64    Size,
        GpuDevice *gpudev
    );

#if defined(TEGRA_MODS)
    RC AllocDisplayMemory
    (
        Handle                      *  pRtnMemoryHandle,
        UINT32                         Class,
        LW_MEMORY_ALLOCATION_PARAMS *  pAllocMemParams,
        GpuDevice                      *gpudev
    );

    RC AllocDisplaySystemMemory
    (
        Handle                      *  pRtnMemoryHandle,
        LW_MEMORY_ALLOCATION_PARAMS *  pAllocMemParams,
        GpuDevice                      *gpudev
    );
#endif

    RC AllocEvent
    (
        Handle    ParentHandle,
        Handle *  pRtnEventHandle,
        UINT32    Class,
        UINT32    Index,
        void *    hEvent
    );

#if defined(TEGRA_MODS)
    RC AllocDispEvent
    (
        Handle    ParentHandle,
        Handle *  pRtnEventHandle,
        UINT32    Class,
        UINT32    Index,
        void *    hEvent
    );
#endif

    //! Equivalent to LwRmMapMemory and LwRmUnmapMemory
    RC MapMemory
    (
        Handle   MemoryHandle,
        UINT64   Offset,
        UINT64   Length,
        void  ** pAddress,
        UINT32   Flags,
        GpuSubdevice *gpusubdev
    );

#if defined(TEGRA_MODS)
    RC MapDispMemory
    (
        Handle   MemoryHandle,
        UINT64   Offset,
        UINT64   Length,
        void  ** pAddress,
        UINT32   Flags,
        GpuSubdevice *gpusubdev
    );
#endif

    void UnmapMemory
    (
        Handle          MemoryHandle,
        volatile void * Address,
        UINT32          Flags,
        GpuSubdevice  * gpusubdev
    );

#if defined(TEGRA_MODS)
    void UnmapDispMemory
    (
        Handle          MemoryHandle,
        volatile void * Address,
        UINT32          Flags,
        GpuSubdevice  * gpusubdev
    );
#endif

    //! Returns internal handle usable with kernel drivers (on Android).
    RC GetKernelHandle(Handle memoryHandle, void** pKernelHandle);

    //! Returns pointer to CheetAh RM
    static TegraRm* CheetAh();

    //! Wrapper for mapping/unmapping regions of video memory
    RC MapFbMemory
    (
        UINT64   Offset,
        UINT64   Length,
        void  ** pAddress,
        UINT32   Flags,
        GpuSubdevice *Subdev
    );
    void UnmapFbMemory
    (
        volatile void * Address,
        UINT32          Flags,
        GpuSubdevice *Subdev
    );

    RC FlushUserCache
    (
        GpuDevice* pGpuDev,
        Handle     hMem,
        UINT64     offset,
        UINT64     size
    );

    RC AllocObject
    (
        Handle   ChannelHandle,
        Handle * pRtnObjectHandle,
        UINT32   Class
    );

    void FreeSubDevice(UINT32 DeviceReference, UINT32 SubDeviceReference);
    void FreeDevice(UINT32 DeviceReference);
    void FreeSyscon(UINT32 SysconReference);

    void Free
    (
        Handle ObjectHandle
    );

    RC ReadRegistryDword
    (
        const char      *devNode,
        const char      *parmStr,
        UINT32          *data
    );

    RC WriteRegistryDword
    (
        const char      *devNode,
        const char      *parmStr,
        UINT32           data
    );

    RC VidHeapAllocSize
    (
        UINT32   Type,
        UINT32   Attr,
        UINT32   Attr2,
        UINT64   Size,
        UINT32 * pHandle,
        UINT64 * pOffset,
        UINT64 * pLimit,
        UINT64 * pRtnFree,
        UINT64 * pRtnTotal,
        GpuDevice *gpudev,
        UINT32   hVASpace = 0
    );

    RC VidHeapAllocSizeEx
    (
        UINT32   Type,
        UINT32   Flags,
        UINT32 * pAttr,
        UINT32 * pAttr2,
        UINT64   Size,
        UINT64   Alignment,
        INT32  * pFormat,
        UINT32 * pCoverage,
        UINT32 * pPartitionStride,
        UINT32 * pHandle,
        UINT64 * pOffset,
        UINT64 * pLimit,
        UINT64 * pRtnFree,
        UINT64 * pRtnTotal,
        UINT32   width,
        UINT32   height,
        GpuDevice *gpudev,
        UINT32   hVASpace = 0
    );

    // Add CtagOffset support
    RC VidHeapAllocSizeEx
    (
        UINT32   Type,
        UINT32   Flags,
        UINT32 * pAttr,
        UINT32 * pAttr2,
        UINT64   Size,
        UINT64   Alignment,
        INT32  * pFormat,
        UINT32 * pCoverage,
        UINT32 * pPartitionStride,
        UINT32   CtagOffset,
        UINT32 * pHandle,
        UINT64 * pOffset,
        UINT64 * pLimit,
        UINT64 * pRtnFree,
        UINT64 * pRtnTotal,
        UINT32   width,
        UINT32   height,
        GpuDevice *gpudev,
        UINT64   rangeBegin = 0,
        UINT64   rangeEnd = 0,
        UINT32   hVASpace = 0
    );

    // Returns actual size and alignment
    RC VidHeapAllocSizeEx
    (
        UINT32   Type,
        UINT32   Flags,
        UINT32 * pAttr,
        UINT32 * pAttr2,
        UINT64 * pSize,
        UINT64 * pAlignment,
        UINT32 * pHandle,
        UINT64 * pOffset,
        UINT64 * pLimit,
        GpuDevice *gpudev,
        UINT64   rangeBegin,
        UINT64   rangeEnd,
        UINT32   hVASpace
    );

    RC VidHeapAllocPitchHeight
    (
        UINT32   Type,
        UINT32   Attr,
        UINT32   Attr2,
        UINT32   Height,
        UINT32 * pPitch,
        UINT32 * pHandle,
        UINT64 * pOffset,
        UINT64 * pLimit,
        UINT64 * pRtnFree,
        UINT64 * pRtnTotal,
        GpuDevice *gpudev,
        LwRm::Handle hVASpace = 0
    );

    RC VidHeapAllocPitchHeightEx
    (
        UINT32   Type,
        UINT32   Flags,
        UINT32 * pAttr,
        UINT32 * pAttr2,
        UINT32   Height,
        UINT32 * pPitch,
        UINT64   Alignment,
        INT32  * pFormat,
        UINT32 * pCoverage,
        UINT32 * pPartitionStride,
        UINT32 * pHandle,
        UINT64 * pOffset,
        UINT64 * pLimit,
        UINT64 * pRtnFree,
        UINT64 * pRtnTotal,
        GpuDevice *gpudev,
        LwRm::Handle hVASpace = 0
    );

    // Add CtagOffset support
    RC VidHeapAllocPitchHeightEx
    (
        UINT32   Type,
        UINT32   Flags,
        UINT32 * pAttr,
        UINT32 * pAttr2,
        UINT32   Height,
        UINT32 * pPitch,
        UINT64   Alignment,
        INT32  * pFormat,
        UINT32 * pCoverage,
        UINT32 * pPartitionStride,
        UINT32   CtagOffset,
        UINT32 * pHandle,
        UINT64 * pOffset,
        UINT64 * pLimit,
        UINT64 * pRtnFree,
        UINT64 * pRtnTotal,
        GpuDevice *gpudev,
        UINT64   rangeBegin = 0,
        UINT64   rangeEnd = 0,
        LwRm::Handle hVASpace = 0
    );

    RC VidHeapReleaseCompression
    (
        UINT32     hMemory,
        GpuDevice *gpudev
    );

    RC VidHeapReacquireCompression
    (
        UINT32     hMemory,
        GpuDevice *gpudev
    );

    RC VidHeapHwAlloc
    (
        UINT32 AllocType,
        UINT32 AllocAttr,
        UINT32 AllocAttr2,
        UINT32 AllocWidth,
        UINT32 AllocPitch,
        UINT32 AllocHeight,
        UINT32 AllocSize,
        UINT32 *pAllochMemory,
        GpuDevice *pGpuDev
    );

    RC VidHeapHwFree
    (
        UINT32 hResourceHandle,
        UINT32 flags,
        GpuDevice *pGpuDev
    );

    RC VidHeapInfo
    (
        UINT64 * pFree,
        UINT64 * pTotal,
        UINT64 * pOffsetLargest,
        UINT64 * pSizeLargest,
        GpuDevice *GpuDev
    );

    RC VidHeapOsDescriptor
    (
        UINT32 type,
        UINT32 flags,
        UINT32 attr,
        UINT32 attr2,
        UINT64 *physAddr,
        UINT64 size,
        GpuDevice *gpuDevice,
        LwRm::Handle *physMem
    );

    RC FreeOsDescriptor
    (
        GpuDevice* pDevice,
        LwRm::Handle physMem
    );

    RC ConfigGet
    (
        UINT32   Index,
        UINT32 * pValue,
        void   * subdev
    );

    RC ConfigSet
    (
        UINT32   Index,
        UINT32   NewValue,
        UINT32 * pOldValue,
        GpuSubdevice *subdev
    );

    RC ConfigSetEx
    (
        UINT32 Index,
        void * pParamStruct,
        UINT32 ParamSize,
        GpuSubdevice *subdev
    );

    RC ConfigGetEx
    (
        UINT32 Index,
        void * pParamStruct,
        UINT32 ParamSize,
        const GpuSubdevice *subdev
    );

// Mac Only Config Calls.
// Also #ifdef'd in RM
#ifdef MACOSX
    RC OsConfigGet
    (
        UINT32   Index,
        UINT32 * pValue
    );

    RC OsConfigSet
    (
        UINT32   Index,
        UINT32   NewValue,
        UINT32 * pOldValue = 0
    );
#endif

    RC I2CAccess
    (
        void * pCtrlStruct,
        void * subdev
    );

    //! Equivalent to LwRmIdleChannels
    RC IdleChannels
    (
        Handle hChannel,
        UINT32 numChannels,
        UINT32 *phClients,
        UINT32 *phDevices,
        UINT32 *phChannels,
        UINT32 flags,
        UINT32 timeoutUs,
        UINT32 DeviceInst
    );

    //! Equivalent to LwRmBindContextDma
    RC BindContextDma
    (
        Handle hChannel,
        Handle hCtxDma
    );

#if defined(TEGRA_MODS)
    RC BindDispContextDma
    (
        Handle hChannel,
        Handle hCtxDma
    );
#endif

    //! Equivalent to LwRmMapMemoryDma and LwRmUnmapMemoryDma
    RC MapMemoryDma
    (
        Handle hDma,
        Handle hMemory,
        UINT64 offset,
        UINT64 length,
        UINT32 flags,
        UINT64 *pDmaOffset,
        GpuDevice *gpudev
    );
    void UnmapMemoryDma
    (
        Handle hDma,
        Handle hMemory,
        UINT32 flags,
        UINT64 dmaOffset,
        GpuDevice *gpudev
    );

    RC DuplicateMemoryHandle
    (
        Handle hSrc,
        Handle hSrcClient,
        Handle *hDst,
        GpuDevice *pGpuDevice
    );
    RC DuplicateMemoryHandle(Handle hSrc, Handle *hDst, GpuDevice *pGpuDevice);

    //! Get the valid SLI configurations
    RC GetValidConfigurations(vector <LW0000_CTRL_GPU_SLI_CONFIG> *SliConfigList);

    //! Get the invalid SLI configurations
    RC GetIlwalidConfigurations(vector <LW0000_CTRL_GPU_SLI_CONFIG> *SliConfigList,
                                vector <UINT32> *SliIlwalidStatus);

    //! Check a SLI configuration for validity
    RC ValidateConfiguration(UINT32 NumGpus, UINT32 *GpuIds,
                             UINT32 *DisplayInstance,
                             UINT32 *SliStatus);

    //! Link multiple GpuIds into an SLI device (if legal)
    RC LinkGpus(UINT32 NumGpus, UINT32 *GpuIds, UINT32 DisplayInst,
                UINT32 *LinkedDeviceInst);

    //! Unlink a device (no-op if not in SLI)
    RC UnlinkDevice(UINT32 DeviceInst);

    //! Wrapper for LwRmControl
    RC ControlByDevice
    (
        const GpuDevice *pGpuDevice,
        UINT32 cmd,
        void   *pParams,
        UINT32 paramsSize
    );

    //! Wrapper for LwRmControl
    RC ControlBySubdevice
    (
        const GpuSubdevice *pGpuSubdevice,
        UINT32 cmd,
        void   *pParams,
        UINT32 paramsSize
    );

    //! Wrapper for LwRmControl
    RC ControlSubdeviceChild
    (
        const GpuSubdevice *pGpuSubdevice,
        UINT32 ClassId,
        UINT32 Cmd,
        void   *pParams,
        UINT32 ParamsSize
    );

    RC CreateFbSegment
    (
        GpuDevice *pGpudev,
        void *pParams
    );

    RC DestroyFbSegment
    (
        GpuDevice *pGpudev,
        void *pParams
    );

    //! Wrapper for LwRmControl
    RC Control
    (
        Handle hObject,
        UINT32 cmd,
        void   *pParams,
        UINT32 paramsSize
    );

    void HandleResmanEvent
    (
        void  *pOsEvent,
        UINT32 GpuInstance
    );

    //! Since we keep track of a tree of handles, we might as well provide a way
    //! to print it out, to visualize who owns what objects.
    void PrintHandleTree(UINT32 Parent = 0, UINT32 Indent = 0);

    //! Check whether a particular class is supported
    bool IsClassSupported(UINT32 ClassID, GpuDevice *gpudev);

    //! Pick a supported class from a list of classes in order of priority
    //! Returns OK if a supported class is found or
    //!     RC::UNSUPPORTED_FUNCTION if no supported class is found
    //! The first supported class ID is returned in the returnID parameter
    RC GetFirstSupportedClass(UINT32 nClasses, const UINT32 *ClassIDs,
                                UINT32 *returnID, GpuDevice *gpudev);

    //! Fill classList vector with a list of supported classes
    RC GetSupportedClasses
    (
        vector<UINT32>* classList,
        GpuDevice* pGpuDevice
    );

    //! Fill classList vector with a list of supported classes via EscapeReads
    //! for AMODEL
    RC GetSupportedClassesFromAmodel
    (
        vector<UINT32>* classList,
        GpuDevice* pGpuDevice
    );

    //! Fill classList vector with a list of supported classes via RM ctrl call
    RC GetSupportedClassesFromRm
    (
        vector<UINT32>* classList,
        GpuDevice* pGpuDevice
    );


    void EnableControlTrace(bool bEnable) { m_bEnableControlTrace = bEnable; }
    bool GetEnableControlTrace() {return m_bEnableControlTrace; }

    class DisableRcWatchdog
    {
    public:
        explicit DisableRcWatchdog(GpuSubdevice* pGpuSubdev);
        ~DisableRcWatchdog();

    private:
        // Non-copyable
        DisableRcWatchdog(const DisableRcWatchdog&);
        DisableRcWatchdog& operator=(const DisableRcWatchdog&);

        static map<UINT32,int> s_DisableCount;
        GpuSubdevice* m_pSubdev;
        bool m_Disabled;
    };

    class DisableRcRecovery
    {
    public:
        explicit DisableRcRecovery(GpuSubdevice* pGpuSubdev);
        ~DisableRcRecovery();

    private:
        // Non-copyable
        DisableRcRecovery(const DisableRcRecovery&);
        DisableRcRecovery& operator=(const DisableRcRecovery&);

        static map<UINT32,int> s_DisableCount;
        GpuSubdevice* m_pSubdev;
        bool m_Disabled;
    };

    //! RAII class for acquiring an exclusive lock on the RM.  This class
    //! locks the mutex MODS uses when accessing the RM to prevent any other
    //! threads from performing any RM operations
    class ExclusiveLockRm
    {
    public:
        explicit ExclusiveLockRm(LwRm* pLwRm)
            : m_pLwRm(pLwRm), m_LockCount (0) { }
        ~ExclusiveLockRm();

        // Non-copyable
        ExclusiveLockRm(const ExclusiveLockRm&) = delete;
        ExclusiveLockRm& operator=(const ExclusiveLockRm&) = delete;

        RC AcquireLock();
        RC TryAcquireLock();
        void ReleaseLock();
    private:
        LwRm* m_pLwRm;
        volatile INT32 m_LockCount;
    };

    //! RAII class for disabling control tracing, useful in blocks of code that
    //! generate excessive numbers of control calls resulting in heavy amounts
    //! of debug code
    class DisableControlTrace
    {
    public:
        explicit DisableControlTrace();
        ~DisableControlTrace();

    private:
        // Non-copyable
        DisableControlTrace(const DisableControlTrace&);
        DisableControlTrace& operator=(const DisableControlTrace&);

        static volatile INT32 s_DisableCount;
        static volatile bool s_bEnableState;
    };

private:
    //! Equivalent to LwRmVidHeapControl -- should not be used directly
    RC VidHeapControl
    (
        void *pParams,
        GpuDevice *gpudev = 0
    );

    UINT32               m_ClientId;
    Handle               m_Client;
    Channels             m_Channels;
    DisplayChannels      m_DisplayChannels;
    void                *m_Mutex;
    bool                 m_bEnableControlTrace;
    bool                 m_UseInternalCall;
#if defined(TEGRA_MODS)
    // Don't expose display client handle outside of LwRm class
    Handle               m_DisplayClient = 0;
    // Handles which are replicated in rmapi_tegra and disp rmapi. Need
    // to free the handles in both libraries
    vector<LwRm::Handle> m_ReplicatedHandles;
#endif

    // In order to keep track of the RM objects outstanding, we maintain our own
    // list.  When a parent object is deleted we can then delete all of the child
    // objects.
    Utility::HandleStorage m_Handles;

    static constexpr UINT32 s_DevBaseHandle = 0xD0000000;

    Handle GenUniqueHandle(UINT32 Parent, Lib lib);
    Handle GenUniqueHandle(UINT32 Parent)
        { return GenUniqueHandle(Parent, Lib::RMAPI); }
    // Generate handles for devices
    Handle DevHandle(UINT32 devRef) const
        { return s_DevBaseHandle | (m_ClientId << 16) | (devRef<<8); }
    // Generate handles for subdevices
    // subdevRef should be 1-indexed.  0 is the broadcast subdevice.
    // Note that for Dynamic SLI purposes, subdevRef 1 is actually subdevice 0.
    Handle SubdevHandle(UINT32 devRef, UINT32 subdevRef) const
        { return (DevHandle(devRef) & 0xFFFFFF00) | subdevRef; }
    Handle GetHandleForCfgGetSet(UINT32 Index, void * pParamStruct,
                                 bool isSet, const GpuSubdevice *gpusubdev) const;
    INT32 CountChildren(LwRm::Handle Handle);
    void DeleteHandleWithChildren(LwRm::Handle Handle);
    void DeleteHandle(LwRm::Handle handle);

    UINT32 ClientIdFromHandle(LwRm::Handle Handle);
    LwRm::Handle ClientHandleFromObjectHandle(LwRm::Handle Handle);
    bool ValidateClient(LwRm::Handle Handle);
    void SetChannel(Handle handle, Channel *pChan);
    void SetDisplayChannel(Handle handle, DisplayChannel *pDispChan);
    RC ChannelIdFromHandle(GpuDevice *pGpuDev, Handle hChannel, UINT32 *pChID);

    void AllocGpFifoChannelCleanup(void *pvCleanupArgs);
    RC PrepareAllocParams
    (
        Handle clientHandle,
        UINT32 deviceReference,
        LW0080_ALLOC_PARAMETERS *pAllocParams
    ) const;

    static bool ValidateHandleTagAndLib(Handle handle);
    static Lib GetLibFromHandle(Handle handle);
#if defined(TEGRA_MODS)
    RC AllocDisplayClient();
    bool   IsDisplayClientAllocated() const { return m_DisplayClient != 0; }
    Handle MapToDispRmHandle(Handle handle) const
        { return (handle == m_Client) ? m_DisplayClient: handle; }
    RC GetDispRmSupportedClasses(Handle hDev, vector<UINT32> *pClassList);
    void FreeDispRmHandle(Handle ObjectHandle);
    bool IsDisplayClass(UINT32 ctrlClass) const;
    bool IsRmControlRoutedToDispRmapi(UINT32 cmd) const;
    UINT32 GetClassID(Handle ObjectHandle);
#endif
    RC MapMemoryInternal
    (
        Handle   MemoryHandle,
        UINT64   Offset,
        UINT64   Length,
        void  ** ppAddress,
        UINT32   Flags,
        GpuSubdevice *gpusubdev,
        UINT32   Class
    );
    // Do not allow copying.
    LwRm(const LwRm &);
    LwRm & operator=(const LwRm &);

    // Only LwRmPtr should ever be able to create an object of type LwRm
    LwRm();
    ~LwRm();

    friend class LwRmPtr;
};

/**
 * @class(LwRmPtr)  A helper class, providing simplified RM
 *       Client access.
 */
class LwRmPtr
{
private:
   LwRm *m_pLwRmClient;

   // Maximum number of RM clients that may be created
   static constexpr UINT32 s_MaxClients = 256U;
   static LwRm s_LwRmClients[s_MaxClients];
   static UINT32 s_ValidClientCount;
   static UINT32 s_FreeClientCount;

public:
   explicit LwRmPtr();
   explicit LwRmPtr(UINT32 Client);
   LwRm * Get()        const { return m_pLwRmClient; }
   LwRm * operator->() const { return m_pLwRmClient; }
   LwRm & operator*()  const { return *m_pLwRmClient; }

   static RC     SetValidClientCount(UINT32 ClientCount);
   static UINT32 GetValidClientCount() { return s_ValidClientCount; }
   static RC     SetFreeClientCount(UINT32 ClientCount);
   static UINT32 GetFreeClientCount() { return s_FreeClientCount; }
   static LwRm * GetFreeClient();
   static bool IsInstalled() { return true; }
};

RC RmApiStatusToModsRC(UINT32 Status);

#define DISALLOW_LWRMPTR_IN_CLASS(classname)           \
    class LwRmPtr : public ::LwRmPtr                   \
    {                                                  \
    public:                                            \
        LwRmPtr(UINT32 Client) : ::LwRmPtr(Client) { } \
    private:                                           \
        LwRmPtr() { }                                  \
    };

#endif // !INCLUDED_LWRM_H
#endif // INCLUDE_GPU
