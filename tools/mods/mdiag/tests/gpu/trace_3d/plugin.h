/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef __PLUGIN_H__
#define __PLUGIN_H__

// This file contains interface classes that constitute the functionality
// available to trace_3d plugins.   It is shared between trace_3d and the plugin
//  build environment, and is checked in both to mods/trace_3d and to the HW
// p4 depot plugin directory:
//      //hw/tools/mods/trace_3d/plugin
// which contains the plugin-side framework and an example plugin
// (pluginexample.cpp)
//

#include <list>
#include <string>
#include <vector>
#include <map>
#include <string.h>
#include <stdarg.h>
#include <utility>

#include "gpu/include/gpusbdev.h"

namespace T3dPlugin
{

// the major and minor version numbers cover the interfaces of both the
// entry point functions
// __Startup, __Initialize, __Shutdown, __EventHandler, and the underlying
// message passing interface as implemented in the "Message" object (see
// Message.h)
//

//
// Description of trace_3d plugin versioning strategy:
//
// The goal is that all combinations of (newer/older) mods and (newer/older)
// plugin be supported when possible.   This can only work when there is a
// strict contract of available functionality and protocol/handshake
// compatibility between mods and the plugin, as describe by the major and
// minor version numbers.
//
// 1) Major version number changes indicate a change in behavior in existing
//    functionality (data format changes in existing API calls), or an
//    incompatibility of the low level mods/plugin handshake (e.g., shim
//    function vtable format change).   The mods and plugin major version
//    numbers must match exactly or the two cannot run together.
//
// 2) Minor version number changes within a given major version number
//    indicate new functionality available to the plugin, which is a strict
//    superset of the previous minor version number.   That is, minor version
//    number N+1 contains exactly the same features as minor version number N,
//    and in addition in contains new features.   A plugin compiled against
//    minor version number 5 will work with all mods of minor version number
//    >= 5.    A plugin compiled against minor version 10 might work with a
//    mods of minor version 7 as long as it only makes use of functionality
//    available in minor versions <= 7 (this may not always hold 100% true),
//    but the plugin would have to be minor version aware.
//
//  This means that any change that formats data (e.g., property tables) a
//  different way must bump the major revision number.   The addition of
//  new functionality that doesn't affect old functionality can just bump the
//  minor version number
//
//  It is strongly recommended therefore not to change existing API functions
//  or the format of the data they return, which requires a major version
//  number change.   Instead, add new API functions, which only requires a
//  bump of the minor version number.

// version history
//
// Major Version 1:
//    Minor version 1:
//       all changes up to and including perforce SW CL 3517908
//           dated 2009/02/17
//
//    Minor version 2:
//       adds TraceManager::getSubChannels()
//            Host::getHostMinorVersion()
//            UtilityManager::getSimulationMode()
//
//    Minor version 3:
//       adds Surface::GetPixelOffset()
//
//    Minor version 4:
//       adds Buffer::setProperty()
//
//    Minor version 5:
//        Buffers which are really images and which are not managed by the
//        surface manager are enumerated in the surface list, not the buffer
//        list (no shim API additions)
//
//   Minor version 6:
//       adds Mem::FlushCpuWriteCombineBuffer()
//
//   Minor version 7:
//       adds Surface::ReadSurfaceUsingDma()
//            Surface::CreatePitchImageUsingDma()
//            Buffer::ReadUsingDma()
//
//   Minor version 8:
//       adds Buffer::getPteKind()
//
//   Minor version 9:
//       adds UtilityManager::AllocEvent()
//            UtilityManager::FreeEvent()
//            UtilityManager::ResetEvent()
//            UtilityManager::SetEvent()
//            UtilityManager::IsEventSet()
//            UtilityManager::HookPmuEvent()
//            UtilityManager::UnhookPmuEvent()
//            UtilityManager::FailTest()
//
//   Minor version 10:
//       adds UtilityManager::PciRead32()
//            UtilityManager::PciWrite32()
//
//   Minor version 11:
//       adds TraceManager::WaitForIdleOneChannel()
//       adds TraceManager::GetMethodCountOneChannel()
//
//   Minor version 12:
//       adds TraceManager::getTraceHeaderFile()
//            Buffer::SendGpEntry()
//
//   Minor version 13:
//       adds Buffer::getGpuPhysAddr()
//
//   Minor version 14:
//       adds Mem::MapDeviceMemory()
//            Mem::UnMapDeviceMemory()
//
//   Minor version 15:
//       adds Buffer::WriteBuffer()
//
//   Minor version 16:
//       adds UtilityManager::GetSimulatorTime()
//
//   Minor version 17:
//       adds TraceManager::GetMethodWriteCountOneChannel()
//
//   Minor version 18:
//       adds UtilityManager::GetVirtualMemoryUsed()
//            UtilityManager::Printf()
//
//   Minor version 19:
//       adds GpuManager::GetDeviceIdString()
//
//   Minor version 20:
//       adds UtilityManager::GetOSString()
//
//   Minor version 21:
//       adds Buffer::getGpuPartitionIndex()
//            Buffer::getGpuL2SliceIndex()
//            Buffer::getGpuXbarRawAddr()
//
//   Minor version 22:
//       adds Gpu::GetSMVersion()
//
//   Minor version 23:
//       adds Buffer::getGpuSubpartition()
//            Buffer::getGpuRBCAddr()
//            Buffer::getGpuRBCAddrExt()
//
//   Minor version 24:
//       adds Surface::getGpuPhysAddr()
//            Surface::getGpuPartitionIndex()
//            Surface::getGpuL2SliceIndex()
//            Surface::getGpuXbarRawAddr()
//
//   Minor version 25:
//       adds BufferManager::CreateSurfaceView()
//            BufferManager::GetSurfaceByName()
//            Surface::IsSurfaceView()
//
//   Minor version 26:
//       adds TraceOp::GetLineNumber()
//
//   Minor version 27:
//       adds Host::getJavaScriptManager()
//            Host::getDisplayManager()
//            JavaScriptManager::LoadJavaScript()
//            JavaScriptManager::ParseArgs()
//            JavaScriptManager::CallVoidMethod()
//            JavaScriptManager::CallINT32Method()
//            JavaScriptManager::CallUINT32Method()
//            DisplayManager::GetChannelByHandle()
//            DisplayManager::MethodWrite()
//            DisplayManager::FlushChannel()
//            DisplayManager::CreateContextDma()
//            DisplayManager::GetDmaCtxAddr()
//            DisplayManager::GetDmaCtxOffset()
//            TraceManager::RunTraceOps()
//
//   Minor version 28:
//            TraceManager::StartPerfMon()
//            TraceManager::StopPerfMon()
//
//   Minor version 29: (RTAPI)
//       adds Host::GetSOC()
//            BufferManager::CreateDynamicSurface()
//            Surface::Alloc()
//            Surface::Free()
//            Surface::SetPitch()
//            SOC::RegRd32()
//            SOC::RegWr32()
//            SOC::AllocSyncPoint()
//            Mem::PhysRd08()
//            Mem::PhysRd16()
//            Mem::PhysRd32()
//            Mem::PhysRd64()
//            Mem::PhysWr08()
//            Mem::PhysWr16()
//            Mem::PhysWr32()
//            Mem::PhysWr64()
//            SOC::SyncPoint::Wait()
//            SOC::SyncPoint::HostIncrement()
//            SOC::SyncPoint::GraphicsIncrement()
//            SOC::SyncPoint::GetIndex()
//            UtilityManager::HookIRQ()
//            UtilityManager::UnhookIRQ()
//
//   Minor version 30:
//       adds UtilityManager::ModsAssertFail()
//            Gpu::GetRegNum()
//            Gpu::SetRegFldDef()
//            Gpu::SetRegFldNum()
//            Gpu::GetRegFldDef()
//            Gpu::GetRegFldNum()
//            Gpu::PciRd08()
//            Gpu::PciRd16()
//            Gpu::PciRd32()
//            Gpu::PciWr08()
//            Gpu::PciWr16()
//            Gpu::PciWr32()
//
//   Minor version 31:
//       adds Gpu::GetDeviceId()
//            Gpu::GetDeviceIdString()
//
//   Minor version 32:
//       adds UtilityManager::GetTimeoutMS()
//            UtilityManager::Poll()
//
//   Minor version 33:
//       adds UtilityManager::DelayNS()
//
//   Minor version 34:
//       adds Buffer::IsAllocated()
//            Surface::IsAllocated()
//            Buffer::GetTagCount()
//            Buffer::GetTag()
//            Buffer::GetTagList()
//
//   Minor version 35:
//       adds Gpu::SetPcieLinkSpeed()
//
//   Minor version 36:
//       adds BufferManager::SetSkipBufferDownloads()
//
//   Minor version 37:
//       adds Surface::CallwlateCrc()
//
//   Minor version 38:
//       adds Gpu::LoadPMUFirmware()
//
//   Minor version 39:
//       adds BufferManager::RedoRelocations()
//
//   Minor version 40:
//       adds Buffer::IsGpuMapped()
//            Surface::IsGpuMapped()
//
//   Minor version 41:
//       adds Surface::SetVASpace()
//            Surface::GetTegraVirtAddr()
//            SOC::IsSmmuEnabled()
//
//   Minor version 42:
//       adds BufferManager::DeclareTegraSharedSurface()
//
//   Minor version 43:
//       modifies UtilityManager::HookIRQ()
//                UtilityManager::UnhookIRQ()
//
//   Minor version 44:
//       modifies SOC::AllocSyncPoint()
//       adds SOC::SyncPointBase
//
//   Minor version 45:
//       adds Gpu::GetPowerFeaturesSupported()
//            Gpu::SetGpuPowerOnOff()
//
//   Minor version 46:
//       adds Gpu::GobWidth()
//            Gpu::GobHeight()
//            Gpu::GobDepth()
//       modifies BufferManager::CreateDynamicSurface()
//
//   Minor version 47:
//       adds Gpu::EnablePrivProtectedRegisters()
//            Gpu::EnablePowerGatingTestDuringRegAccess()
//
//   Minor version 48:
//       modifies UtilityManager::FailTest()
//
//   Minor version 49:
//       adds BufferManager::DeclareGlobalSharedSurface()
//
//   Minor version 50:
//       modify the name of enum in BufferManager::MemTarget
//
//   Minor version 51:
//       adds Gpu::RmControl5070()
//
//   Minor version 52:
//       moves RmControl5070() from Gpu to DisplayManager
//       adds DisplayManager::GetActiveDisplayIDs()
//
//   Minor version 53:
//       adds DisplayManager::GetMasterSubdevice()
//
//   Minor version 54:
//       adds DisplayManager::GetConfigParams()
//
//   Minor version 55:
//       adds Surface::getPhysAddress()
//            Surface::getBAR1PhysAddress()
//            Surface::GetMemoryMappingMode()
//            Surface::SetMemoryMappingMode()
//            Surface::GetLocation()
//            Surface::SetLocation()
//            Surface::GetGpuCacheMode()
//            Surface::SetGpuCacheMode()
//
//   Minor version 56
//       adds Mem::AllocIRam()
//            Mem::FreeIRam()
//            Surface::SetVideoProtected()
//
//   Minor version 57:
//       adds UtilityManager::VAPrintf()
//
//   Minor version 58:
//       adds Mem::VirtRd()
//            Mem::VirtWr()
//
//   Minor version 59:
//       adds UtilityManager::GetThreadId()
//
//   Minor version 61:
//      adds UtilityManager::isEmulation()
//
//   Minor version 62:
//       adds UtilityManager::DisableIRQ()
//            UtilityManager::EnableIRQ()
//            UtilityManager::IsIRQHooked()
//
//   Minor version 63:
//      new field returned in DisplayManager::GetConfigParams()
//      DisplayMngCfgParams::TypeID is set to ParamType1 by default now
//
//   Minor version 64:
//       adds Gpu::GetGCxWakeupReason
//
//   Minor version 65:
//       adds TraceManager::getTestName()
//
//   Minor version 66:
//       adds Gpu::GetGC6PlusIsIslandLoaded
//
//   Minor version 67:
//       adds SOC::RegRd08()
//            SOC::RegWr08()
//            SOC::RegRd16()
//            SOC::RegWr16()
//
//   Minor version 68:
//       adds UtilityManager::DisableInterrupts()
//            UtilityManager::EnableInterrupts()
//
//   Minor version 69:
//       adds Surface::SwizzleCompressionTags()
//            BufferManager::GetTraversalDirection()
//
//   Minor version 70:
//            BufferManager::GetActualPhysAddr()
//            UtilityManager::RegisterUnmapEvent()
//            UtilityManager::RegisterMapEvent()
//
//   Minor version 71:
//            Buffer::getGpuLtcIndex()
//            Surface::getGpuLtcIndex()
//
//   Minor version 72:
//       modifies Surface::CreatePitchImageUsingDma
//                Surface::CreatePitchImage
//
//   Minor version 73:
//            Gpu::SetDiGpuReady()
//
//   Minor version 74:
//            Gpu::GetIrq()
//
//   Minor version 75:
//            Gpu::UnlockHost2Jtag()
//            Gpu::ReadHost2Jtag()
//            Gpu::WriteHost2Jtag()
//
//   Minor version 76:
//            BufferManager::AllocSurface
//
//   Minor version 77:
//       modifies Gpu::ReadHost2Jtag()
//                Gpu::WriteHost2Jtag()
//
//   Minor version 78:
//       adds UtilityManager::FindPciDevice()
//            UtilityManager::FindPciClassCode()
//
//   Minor version 79:
//            Gpu::GetGpcCount()
//            Gpu::GetTpcCountOnGpc()
//
//   Minor version 80:
//       adds Buffer::getEccInjectRegVal()
//
//   Minor version 81:
//            SOC::AllocSemaphore()
//            SOC::Semaphore::Acquire()
//            SOC::Semaphore::Release()
//            SOC::Semaphore::GetPhysAddress()
//
//   Minor version 82:
//       Move semaphore from T3dPlugin::SOC to T3dPlugin
//            Semaphore::Acquire()
//            Semaphore::Release()
//            Semaphore::GetPhysAddress()
//       Add Semaphore IFs in UtilityManager
//            UtilityManager::AllocSemaphore()
//            UtilityManager::FreeSemaphore()
//
//   Minor version 83:
//            Semaphore::AcquireGE()
//
//   Minor version 84:
//       adds Mem::PhysRd()
//            Mem::PhysWr()
//
//   Minor version 85:
//       adds UtilityManager::AllocRawSysMem()
//
//   Minor version 86:
//       adds Gpu::GetLwlinkPowerState()
//            Gpu::SetLwlinkPowerState()
//
//   Minor version 87:
//       Extend Gpu::SetGpuPowerOnOff() interface with RTD3 flag(default=false)
//
//   Minor version 88:
//      adds Gpu::GetNumIrqs()
//           Gpu::GetHookedIntrType()
//           Gpu::GetIrq() (for specific irqNum)
//
//   Minor version 89:
//       adds Gpu::SetPrePostPowerOnOff()
//
//   Minor version 90:
//       modifies Surface::Alloc()
//       Mem::LwRmMapMemory() Mem::LwRmUnmapMemory()
//       Mem::LwRmMapMemoryDma() Mem::LwRmUnmapMemoryDma()
//       Mem::LwRmAllocOnDevice() Mem::LwRmFree()
//
//   Minor version 91:
//            SOC::Semaphore::SetReleaseFlags()
//
//   Minor version 92:
//       adds TraceManager::GetSyspipe()
//
//   Minor version 93:
//      modifies Gpu::regName2Offset
//      Gpu::GetRegNum
//      Gpu::SetRegFldDef
//      Gpu::SetRegFldNum
//      Gpu::GetRegFldNum
//
//   Minor version 94:
//       adds Gpu::GetLwlinkConnectedMask()
//
//   Minor version 95:
//       adds Buffer::mapEccAddrToPhysAddr()
//
//   Minor version 96:
//       adds GpuManager::GetLwlinkConnected()
//       adds GpuManager::GetLwlinkLoopProp()
//       adds GpuManager::GetLwlinkClock()
//       adds GpuManager::GetLwlinkCommonClock()
//       adds GpuManager::GetLwlinkRemoteLinkNum()
//       adds GpuManager::GetLwlinkLocalLinkNum()
//       adds GpuManager::GetLwlinkDiscoveredMask()
//
//   Minor version 97:
//       adds Buffer::CreateSegmentList()
//       adds Surface::CreateSegmentList()
//       adds Segment::GetLength
//       adds Segment::GetLocation
//       adds Segment::GetSize
//       adds Segment::GetPhysAddress
//       adds Segment::Map
//       adds Segment::IsMapped
//       adds Segment::GetAddress
//       adds Segment::Unmap
//
//   Minor version 98:
//       adds GpuManager::Gpu_GetLwlinkFomVals()
//       adds GpuManager::Gpu_GetLwlinkLocalSidVals()
//       adds GpuManager::Gpu_GetLwlinkRemoteSidVals()
//       adds GpuManager::Gpu_GetLwlinkLineRateMbps()
//       adds GpuManager::Gpu_GetLwlinkLinkClockMHz()
//       adds GpuManager::Gpu_GetLwlinkLinkClockType()
//       adds GpuManager::Gpu_GetLwlinkDataRateKiBps()
//       deprecates GpuManager::Gpu_GetLwlinkClock()
//            - renamed to Gpu_GetLwlinkLineRateMbps
//       deprecates GpuManager::Gpu_GetLwlinkCommonClock()
//            - renamed to Gpu_GetLwlinkLinkClockMHz
//
//   Minor version 99:
//       Modify EventManager::getEvent
//            - adds comments about supported eventName.
//
//   Minor version 100:
//       adds Surface::SetPhysContig()
//
//   Minor version 101:
//       add Gpu::GetEngineResetId()
//
//   Minor version 102:
//       adds GpuManager::Gpu_GetLwlinkErrInfo()
//
//   Minor version 103:
//       add UtilityManager::GetChipArgs()
//
//   Minor version 104:
//       add UtilityManager::GpuEscapeWriteBuffer()
//           UtilityManager::GpuEscapeReadBuffer()
//
//   Minor version 105:
//       adds GpuManager::EnterSlm()
//       adds GpuManager::ExitSlm()
//
//   Minor version 106:
//       adds Gpu::TestRegFldDef()
//       adds Gpu::TestRegFldNum()
//
//   Minor version 107:
//       adds UtilityManager::ExitMods()
//
//   Minor version 108:
//       adds Surface::GetGpuVirtAddr()
//
//   Minor version 109:
//       adds UtilityManager::TriggerUtlUserEvent()
//
//   Minor version 110:
//       adds TraceManager::StopVpmCapture()
//
//   Minor version 111:
//       adds Buffer::getGpuRBCAddrExt2()
//       adds Buffer::getGpuPCIndex(UINT64 physAddr)
//       adds Buffer::getEccInjectExtRegVal()
//
//   Minor version 112:
//       adds Gpu::SwReset()
//
//   Minor version 113:
//      adds TraceManager::AbortTest()
//
//   Minor version 114:
//      adds UtilityManager::TrainIntraNodeConnsParrell()
//
//   Minor version 115:
//      modifies Gpu::ReadHost2Jtag()
//      modifies Gpu::UnlockHost2Jtag()
//      modifies Gpu::WriteHost2Jtag()
//      adds Gpu::LoadPMUFirmware()
//      adds Gpu::SetGpuPowerOnOff()
//
//   Minor version 116:
//      modifies UtilityManager::FindPciDevice()
//      modified UtilityManager::FindPciClassCode()
//
//   Minor version 117:
//      adds Gpu::SetEnableCEPrefetch()
//
//   Minor version 118:
//      adds Surface::WriteSurfaceUsingDma

const UINT32 majorVersion = 1;
const UINT32 minorVersion = 118;

// forward declaration
class Host;

typedef void (*pFlwoid)( void );
typedef int (*pFnStartup_t)( Host * pHost, UINT32 majorVersion,
                             UINT32 minorVersion,
                             UINT32 * pPluginMajorVersion,
                             UINT32 * pPluginMinorVersion,
                             const char * hostShimFuncNames[],
                             const pFlwoid hostShimFuncPtrs[] );

typedef int (*pFnInitialize_t)( Host * pHost, const char *argString );
typedef int (*pFnShutdown_t)( Host * pHost );
typedef int (*pFnCallback_t)( void * );

enum SemaphorePayloadSize
{
    SEM_PAYLOAD_SIZE_32BIT=4,
    SEM_PAYLOAD_SIZE_64BIT=8,
    //4K size is for syncpt shim
    SEM_PAYLOAD_SIZE_4KBYTE=4096
};

//! ISR return code (copied from MODS xp.h) //Refer to Bug 1821005
enum IsrResult
{
   // Plugin has not handled the exception & need further handling by others (e.g. RM).
   ISR_RESULT_NOT_SERVICED = 0,
   // Plugin has handled the exception & does not need further handling by others (e.g. RM).
   ISR_RESULT_SERVICED     = 1,
   ISR_RESULT_TIMEOUT      = 2
};

//! Form of a function used as an ISR (copied from MODS types.h)
typedef long (*ISR)(void*);

//! Form of a function used as a PollFunc (copied from MODS tasker.h)
typedef bool (*PollFunc)(void*);

//! Copied from MODS subctx.h
static const UINT32 VEID_END = 64;

// forward declarations
//
class EventManager;
class GpuManager;
class BufferManager;
class Mem;
class TraceManager;
class UtilityManager;
class JavaScriptManager;
class DisplayManager;
class SOC;

enum EventAction
{
    NoAction,
    Jump,       // jump to a different location in the current operation
    Skip,       // skip the current operation in the traceop
    Exit,       // exit the current operation
};

enum HookedIntrType
    {
        HOOKED_NONE = 0,
        HOOKED_INTA = 1,
        HOOKED_MSI  = 2,
        HOOKED_MSIX = 4
    };

// class Host
//
// This class is the top-level class of the plugin architecture, and is the
// starting place for the plugin to obtain access to all the functionality
// available to it from trace_3d, via the get<Manager>()"
// member functions.  A Host object is created for each instantiation of the
// plugin, and so a pointer to a Host class object is in a way the "handle"
// for that instantiation of the  plugin.  There may be multiple instantiations
// of a plugin if mods is running multiple, conlwrrent versions of the trace_3d
// test.
// A typical access pattern from the plugin would be:
//
//       Foo * pFoo = pHost->getFooManager()->getFoo( <parameters> );
//       pFoo->doStuff1( <parameters> );
//       pFoo->doMoreStuff( <parameters> );
//       pHost->getFooManager()->deleteFoo( pFoo );
//
// Example managers are GpuManager (do stuff with the GPU), EventManager
// (do stuff with plugin events, which are
// "interesting things / times / phases / boundaries that occur in the
// processing of a trace by trace_3d"),
// BufferManager (do stuff with "buffers/surfaces/objects in memory"), and so
// on.
//

class Host
{
public:
    virtual ~Host() {}

    // factory for this object
    // pHostIn is a pointer to the trace3d-side sister Host object
    //
    static Host * Create( Host * pHostIn,
                          UINT32 hostMajorVersion,
                          UINT32 hostMinorVersion );

    // returns the host's major version number of the plugin interface
    //
    virtual UINT32 getMajorVersion() const = 0;

    // returns the host's minor version number of the plugin interface
    //
    virtual UINT32 getMinorVersion() const = 0;

    // functions to get pointers to "manager" objects, which contain member
    // functions to perform actions on,
    // or query information from, the trace or the gpu
    //
    virtual EventManager   * getEventManager( void ) = 0;
    virtual GpuManager     * getGpuManager( void ) = 0;
    virtual BufferManager  * getBufferManager( void ) = 0;
    virtual Mem            * getMem( void ) = 0;
    virtual TraceManager   * getTraceManager( void ) = 0;
    virtual UtilityManager * getUtilityManager( void ) = 0;
    virtual JavaScriptManager * getJavaScriptManager( void ) = 0;
    virtual DisplayManager * getDisplayManager( void ) = 0;
    virtual SOC              * getSOC( void ) = 0;

    virtual void sendMessage( const char * msg, UINT32 value ) = 0;
    virtual bool getMessage( const char * msg, UINT32 * pValue ) = 0;

protected:

    // Protect the constructor so that the only way to create this object is
    // through calling the Create() factory function
    //
    explicit Host() {}
};

// forward declaration
//
class Event;

// class EventManager
//
// This class manages plugin events: mostly just creation via getEvent().
// Derived implementation classes on both the plugin and trace_3d side do more
/// under the hood, e.g., maintaining lists of existing events and lists of
// registered events
//

class EventManager
{
public:
    virtual ~EventManager() {}

    // factory function for this object
    //
    static EventManager * Create( Host * pHost,
                                  EventManager * pHostEventManager );

    // factory function for events.   Events are the key object in the
    // trace_3d plugin.  Plugins
    // create and register events with trace_3d, and trace_3d transfers control
    // to the plugin when the event oclwrs, allowing the plugin to take action:
    //  create more events, access the status of the trace, read / write GPU
    // registers, etc.
    //
    // Please refer to //sw/dev/gpu_drv/chips_a/diag/mods/mdiag/tests/gpu/trace_3d/t3event.h
    // for supported eventName

    virtual Event * getEvent( const char * eventName ) = 0;

    // deletes a previously created event (both from trace_3d side and plugin
    // wrapper)
    //
    virtual void deleteEvent( Event * pEvent ) = 0;

    // iterators to loop through the list of available event names
    //
    virtual const char * beginEventNames( void ) = 0;
    virtual const char * endEventNames( void ) = 0;

protected:

    // Protect the constructor so that the only way to create this object is
    // through calling the Create() factory function
    //
    explicit EventManager() {}
};

// class Event
//
// This class represents interesting times/actions/phases in the creation and
// the running of a trace, and allows the plugin writer to associate handler
// functions for these events
// that are called from trace_3d when the matching event oclwrs during the
// playback of a trace.  If a plugin wanted to perform some actions on the GPU
// after a particular interrupt oclwrs, it would likely create an "interrupt"
// event, set some parameters to indicate the interrupt of interest, and then
// register the event with an event handler function along with a generic
// payload that could be used to identify the event.    Events are the only way
// a plugin can get control during the playback of a trace_3d trace, other than
// the Startup(), Initialize(), and Shutdown() functions.
//

class Event
{
public:
    virtual ~Event() {}

    // factory function for this object -- not for end-user consumption, this
    // is called from the EventManager in response to its getEvent()
    //
    static Event * Create( Host * pHost, Event * pHostEvent,
                                  const char * eventName );

    // functions to set and to get the value of this event's template
    // and generated parameters.  A template parameter is one used as
    // a pattern to match against trace_3d events.  The plugin typically
    // only sets template parameters.   A generated parameter is one that
    // is created when a trace_3d event oclwrs.   When a trace_3d-generated
    // event's generated paramers match a registered event's template parameters
    // then the event's callback is called.
    // A plugin typically only sets template parameters and gets generated
    // parameters.
    // trace_3d typically only sets generated parameters
    //

    virtual int setTemplateUint32Arg( const char * argName, UINT32 value ) = 0;
    virtual int setTemplateStringArg( const char * argName,
                                      const char * value ) = 0;

    virtual int setGeneratedUint32Arg( const char * argName, UINT32 value ) = 0;
    virtual int setGeneratedStringArg( const char * argName,
                                       const char * value ) = 0;

    virtual bool getTemplateUint32Arg( const char * argName,
                                       UINT32 * pValue ) const = 0;
    virtual bool getTemplateStringArg( const char * argName,
                                       const char * * pValue ) const = 0;

    virtual bool getGeneratedUint32Arg( const char * argName,
                                        UINT32 * pValue ) const = 0;
    virtual bool getGeneratedStringArg( const char * argName,
                                        const char * * pValue ) const = 0;

    // register this event with trace_3d: when this event happens, the plugin
    // will get control by trace_3d calling the call_back function and will
    // pass in the payload payload originally registered with the event.
    //
    virtual bool _register( pFnCallback_t pFnCallback, void * payload ) = 0;

    // unregister the event with trace_3d: the plugin will no longer be called
    // when this event oclwrs
    //
    virtual bool unregister( void ) = 0;

    virtual int IlwokeCallback( void ) = 0;

    virtual const char * getName( void ) const = 0;

    virtual Host * getHost( void ) = 0;

protected:

    // Protect the constructor so that the only way to create this object is
    // through calling the Create() factory function
    //
    explicit Event() {}
};

// forward declaration
//
class Gpu;

class GpuManager
{
public:
    virtual ~GpuManager() {}

    // factory function for this object
    //
    static GpuManager * Create( GpuManager * pHostGpuManager );

    // returns the # of GPUs on the system (which are accessible to the plugin)
    //
    virtual UINT32 getNumGpus( void ) = 0;

    // factory function for Gpu objects.  Gpu objects contain member functions
    // which allow the plugin to read and modify the GPU state (registers,
    // memory)
    //
    virtual Gpu * getGpu( UINT32 gpuNumber ) = 0;

    // a plugin may explicitly free a gpu object that it no longer needs
    //
    virtual void deleteGpu( Gpu * ) = 0;

    // get device id as string
    //
    virtual const char *GetDeviceIdString() = 0;

protected:

    // Protect the constructor so that the only way to create this object is
    // through calling the Create() factory function
    //
    explicit GpuManager() {}
};

class Gpu
{
public:

    virtual ~Gpu() {}

    // factory function for this object
    //
    static Gpu * Create( Gpu * pHostGpu, UINT32 gpuNum );

    // workhorse functions of GPU access
    //

    // priv register access
    //
    virtual UINT08 RegRd08( UINT32 Offset ) const = 0;
    virtual UINT16 RegRd16( UINT32 Offset ) const = 0;
    virtual UINT32 RegRd32( UINT32 Offset ) const = 0;
    virtual void RegWr08( UINT32 Offset, UINT08 Data ) = 0;
    virtual void RegWr16( UINT32 Offset, UINT16 Data ) = 0;
    virtual void RegWr32( UINT32 Offset, UINT32 Data ) = 0;

    //! Read and write frame buffer memory (BAR1).
    //
    virtual UINT08 FbRd08( UINT32 Offset ) const = 0;
    virtual UINT16 FbRd16( UINT32 Offset ) const = 0;
    virtual UINT32 FbRd32( UINT32 Offset ) const = 0;
    virtual UINT64 FbRd64( UINT32 Offset ) const = 0;
    virtual void   FbWr08( UINT32 Offset, UINT08 Data ) = 0;
    virtual void   FbWr16( UINT32 Offset, UINT16 Data ) = 0;
    virtual void   FbWr32( UINT32 Offset, UINT32 Data ) = 0;
    virtual void   FbWr64( UINT32 Offset, UINT64 Data ) = 0;

    virtual UINT32 GetSMVersion() const = 0;

    virtual bool regName2Offset( const char * name, int dimensionality,
                                 UINT32 * offset /*out*/, int i, int j, const char *regSpace = nullptr) = 0;


    // Read/write register/field by name
    virtual INT32 GetRegNum(const char *regName, UINT32 *value, const char *regSpace = nullptr) = 0;
    virtual INT32 SetRegFldDef(const char *regName, const char *subfieldName,
        const char *subdefName, UINT32 *myValue = 0, const char *regSpace = nullptr) = 0;
    virtual INT32 SetRegFldNum(const char *regName, const char *subfieldName,
        UINT32 setValue, UINT32 *myValue = 0, const char *regSpace = nullptr) = 0;
    virtual INT32 GetRegFldNum(const char *regName, const char *subfieldName,
        UINT32 *theValue, UINT32 *myValue = 0, const char *regSpace = nullptr) = 0;
    virtual INT32 GetRegFldDef(const char *regName, const char *subfieldName,
        const char *subdefName, UINT32 *myValue) = 0;
    virtual bool TestRegFldDef(const char *regName, const char *subfieldName,
        const char *subdefName, UINT32 *myValue = 0, const char *regSpace = nullptr) = 0;
    virtual bool TestRegFldNum(const char *regName, const char *subfieldName,
        UINT32 testValue, UINT32 *myValue = 0, const char *regSpace = nullptr) = 0;
    virtual void SwReset() = 0;

    // PCI read/write
    virtual UINT08 PciRd08(UINT32 offset) = 0;
    virtual UINT16 PciRd16(UINT32 offset) = 0;
    virtual UINT32 PciRd32(UINT32 offset) = 0;
    virtual void PciWr08(UINT32 offset, UINT08 data) = 0;
    virtual void PciWr16(UINT32 offset, UINT16 data) = 0;
    virtual void PciWr32(UINT32 offset, UINT32 data) = 0;

    virtual UINT32 GetIrq() = 0;
    virtual UINT32 GetNumIrqs() = 0;
    virtual UINT32 GetIrq(UINT32 irqNum) = 0;
    virtual HookedIntrType GetHookedIntrType() = 0;

    virtual UINT32 UnlockHost2Jtag(UINT32 *secCfgMaskArr,
                                   UINT32 secCfgMaskSize,
                                   UINT32 *selwreKeysArr,
                                   UINT32 selwreKeysSize) = 0;
    virtual UINT32 ReadHost2Jtag(UINT32 *chipletId,
                                 UINT32 *intstId,
                                 UINT32 jtagClustersSize,
                                 UINT32 chainLen,
                                 UINT32 *pResult,
                                 UINT32 *pResultSize,
                                 UINT32 capDis = 0,
                                 UINT32 updtDis = 0) = 0;
    virtual UINT32 WriteHost2Jtag(UINT32 *chipletId,
                                  UINT32 *intstId,
                                  UINT32 jtagClustersSize,
                                  UINT32 chainLen,
                                  UINT32 *inputData,
                                  UINT32 inputDataSize,
                                  UINT32 capDis = 0,
                                  UINT32 updtDis = 0) = 0;

    virtual UINT32 GetDeviceId() = 0;
    virtual const char *GetDeviceIdString() = 0;

    virtual INT32 SetPcieLinkSpeed(UINT32 newSpeed) = 0;

    virtual INT32 GetPowerFeaturesSupported(UINT32 *powerFeature) = 0;
    virtual INT32 SetGpuPowerOnOff(UINT32 action,
                                   bool bUseHwTimer,
                                   UINT32 timeToWakeUs,
                                   bool bIsRTD3Transition,
                                   bool bIsD3Hot) = 0;
    virtual INT32 SetGpuPowerOnOff(UINT32 action,
                                   bool bUseHwTimer,
                                   UINT32 timeToWakeUs,
                                   bool bIsRTD3Transition) = 0;
    virtual INT32 SetGpuPowerOnOff(UINT32 action,
                                   bool bUseHwTimer,
                                   UINT32 timeToWakeUs) = 0;
    virtual bool SetDiGpuReady() = 0;
    virtual INT32 LoadPMUFirmware(const string &firmwareFile,
                                  bool resetOnLoad = false,
                                  bool waitInit = true) = 0;

    virtual bool SetEnableCEPrefetch(bool bEnable) = 0;

    typedef struct
    {
        UINT32   TLErrlog;
        UINT32   TLIntrEn;
        UINT32   TLCTxErrStatus0;
        UINT32   TLCTxErrStatus1;
        UINT32   TLCTxSysErrStatus0;
        UINT32   TLCRxErrStatus0;
        UINT32   TLCRxErrStatus1;
        UINT32   TLCRxSysErrStatus0;
        UINT32   TLCTxErrLogEn0;
        UINT32   TLCTxErrLogEn1;
        UINT32   TLCTxSysErrLogEn0;
        UINT32   TLCRxErrLogEn0;
        UINT32   TLCRxErrLogEn1;
        UINT32   TLCRxSysErrLogEn0;
        UINT32   MIFTxErrStatus0;
        UINT32   MIFRxErrStatus0;
        UINT32   LWLIPTLnkErrStatus0;
        UINT32   LWLIPTLnkErrLogEn0;
        UINT32   DLSpeedStatusTx;
        UINT32   DLSpeedStatusRx;
        bool  bExcessErrorDL;
    } LWLINK_ERR_INFO;
    virtual UINT32 GetLwlinkErrInfo(LWLINK_ERR_INFO** info, UINT08* numInfo) = 0;

    virtual UINT32 GetLwlinkPowerState(UINT32 linkId) = 0;
    virtual UINT32 SetLwlinkPowerState(UINT32 linkMask, UINT32 powerState) = 0;
    virtual UINT32 SetPrePostPowerOnOff(UINT32 action) = 0;

    virtual bool GetLwlinkConnected(UINT32 linkId) = 0;
    // The following functions return 0 on success
    // Desired return value is populated in pointer param
    virtual UINT32 GetLwlinkLoopProp(UINT32 linkId, UINT08* loopProp) = 0;
    virtual UINT32 GetLwlinkRemoteLinkNum(UINT32 linkId, UINT08* linkNum) = 0;
    virtual UINT32 GetLwlinkLocalLinkNum(UINT32 linkId, UINT08* linkNum) = 0;
    virtual UINT32 GetLwlinkDiscoveredMask(UINT32* mask) = 0;
    virtual UINT32 GetLwlinkEnabledMask(UINT32* mask) = 0;
    virtual UINT32 GetLwlinkConnectedMask(UINT32* pLinkMask) = 0;
    virtual UINT32 GetLwlinkFomVals(UINT32 linkId, UINT16** fom, UINT08* numLanes) = 0;
    virtual UINT32 GetLwlinkLocalSidVals(UINT32 linkId, UINT64* sid) = 0;
    virtual UINT32 GetLwlinkRemoteSidVals(UINT32 linkId, UINT64* sid) = 0;
    virtual UINT32 GetLwlinkLineRateMbps(UINT32 linkId, UINT32* rate) = 0;
    virtual UINT32 GetLwlinkLinkClockMHz(UINT32 linkId, UINT32* clock) = 0;
    virtual UINT32 GetLwlinkLinkClockType(UINT32 linkId, UINT32* clktype) = 0;
    virtual UINT32 GetLwlinkDataRateKiBps(UINT32 linkId, UINT32* rate) = 0;

    virtual UINT32 EnterSlm(UINT32 linkId, bool tx) = 0;
    virtual UINT32 ExitSlm(UINT32 linkId, bool tx) = 0;

    //
    // The parameter definition may be changed or appended
    // in the future. Different definitions are identified by
    // ParamTypeID.
    struct GCxWakeupReasonParams
    {
        // Parameter type definition
        //
        struct ParamsType0
        {
            UINT32   selectPowerState;
            UINT32   statId;
            UINT32   gc5ExitType;
            UINT32   deepL1Type;
            UINT32   gc5AbortCode;
            UINT32   sciIntr0;
            UINT32   sciIntr1;
            UINT32   pmcIntr0;
            UINT32   pmcIntr1;
        };

        // Examples to extend a new variable in the future
        //
        //struct CfgParamsType1 : public CfgParamsType0
        //{
        //  UINT64 NewMember;
        //};

        // Please append ParamType enum once a new type defined
        //
        enum ParamTypeID
        {
            ParamType0 = 0
            //,ParamType1
            ,ParamTypeLast
        };

        // Pamameter data
        //
        ParamTypeID TypeID;
        union
        {
            ParamsType0 Data0;
          //ParamsType1 Data1;
        };

        // Init the structure to use Type0 by default
        GCxWakeupReasonParams(): TypeID(ParamType0)
        {
        }
    };

    struct GC6PlusIsIslandLoadedParams
    {
        bool bIsIslandLoaded;
    };

    virtual INT32 GetGCxWakeupReason(
                UINT32 selectPowerState,
                GCxWakeupReasonParams *pWakeupReasonParams) = 0;

    virtual INT32 GetGC6PlusIsIslandLoaded(
                GC6PlusIsIslandLoadedParams *pIsIslandLoadedParams) = 0;

    virtual UINT32 GobWidth() = 0;
    virtual UINT32 GobHeight() = 0;
    virtual UINT32 GobDepth() = 0;

    virtual UINT32 GetGpcCount() = 0;
    virtual UINT32 GetTpcCountOnGpc(UINT32 virtualGpcNum) = 0;

    virtual bool EnablePrivProtectedRegisters(bool bEnable) = 0;
    virtual bool EnablePowerGatingTestDuringRegAccess(bool bEnable) = 0;
    virtual LwRm* GetLwRmPtr() = 0;

    // The returned value will have a engine reset id and an corresponding flag to indicate if the id is valid or not
    virtual pair<UINT32, bool> GetEngineResetId(const char* engine, UINT32 hwInstance) const = 0;

protected:

    // Protect the constructor so that the only way to create this object is
    // through calling the Create() factory function
    //
    explicit Gpu() {}
};

// forward declarations
//
class Buffer;
class Surface;
class SegmentList;

class BufferManager
{
public:

    virtual ~BufferManager() {}

    // factory function for this object
    //
    static BufferManager * Create( BufferManager * pHostBufferManager );

    // function for querying a Surface by name
    virtual Surface* GetSurfaceByName(const char* name) = 0;

    // function for creating a dynamic Surface for RTAPI
    // TZ_MEM is not yet supported
    //
    virtual Surface* CreateDynamicSurface( const char* name ) = 0;

    // create a trace3d-managed surface.  When created at the proper time
    // ( usually upon receipt of the "afterReadTraceHeader" event), such a
    // buffer will be allocated and downloaded by trace_3d, just as if the
    // buffer had been listed as a FILE in the trace header originally
    // (without all the options available to a FILE statement)
    //
    virtual Buffer * createTraceBuffer( const char * name,
                                        const char * type,
                                        UINT32 size,
                                        const UINT32 * data ) = 0;

    virtual Surface * CreateSurfaceView(const char * name,
                                        const char * parent,
                                        const char * options) = 0;

    virtual Surface * AllocSurface(const char * name,
                                   UINT64 sizeInBytes,
                                   const std::map<std::string, std::string>& properties) = 0;

    // frees the Buffer Object storage representing a trace surface.  Does
    // NOT remove the surface from the trace
    //
    virtual void deleteBuffer( Buffer * pBuffer ) = 0;

    // frees the old buffer list and builds a new version based on the current
    // list of trace (explicit, meaning in the trace header) surfaces
    //
    virtual void refreshBufferList( void ) = 0;

    // for iterating over the list of surfaces (as of the last
    // refreshBufferList() call )
    //
    typedef list< Buffer * >::iterator BufferIterator;
    virtual BufferIterator begin( void ) = 0;
    virtual BufferIterator end( void ) = 0;

    // frees the old surface list and builds a new version based on the
    // current list of implicit trace surfaces (e.g., color, zeta, clipID)
    //
    virtual void refreshSurfaces( void ) = 0;

    // for iterating over the list of surfaces (as of the last
    // refreshSurfaceList() call )
    //
    virtual const vector<Surface *> & getSurfaces( void ) = 0;

    // Used to tell trace3d not to download file contents
    // to the trace buffers.
    virtual void SetSkipBufferDownloads(bool skip) = 0;

    // Used to re-run previous trace header relocations.
    // This is primarily for TDBG checkpointing.  (See bug 960228.)
    virtual void RedoRelocations() = 0;

    // Used to declare a trace surface to be shared by CheetAh engines.
    // The shared surface will be allocated with GPUVASpace.
    virtual void DeclareTegraSharedSurface(const char *name) = 0;

    // Used to declare a trace surface to be shared globally with other traces.
    // The trace will allocate the global surface if it is not allocated before.
    // The shared surface won't be shared with CheetAh by default.
    // If shared with CheetAh, need call DeclareTegraSharedSurface in the trace who allocates it.
    virtual void DeclareGlobalSharedSurface(const char *name, const char *global_name) = 0;

    virtual UINT32 GetTraversalDirection() = 0;

    virtual UINT64 GetActualPhysAddr(const char *surfname, UINT64 offset) = 0;

protected:

    // Protect the constructor so that the only way to create this object
    // is through calling the Create() factory function
    //
    explicit BufferManager() {}
};


class Buffer
{
public:

    virtual ~Buffer() {}

    // factory function for this object
    //
    static Buffer * Create( Buffer * pHostBuffer, const char * name,
                            const char * type, UINT32 size  );

    virtual UINT64 getGpuVirtAddr( void ) const = 0;
    virtual UINT64 getGpuPhysAddr( UINT32 subdev ) const = 0;

    // trace_3d bundles surfaces of similar type together into single "regions"
    // The region is allocated with a single allocation and multiple surfaces
    // within the region are located at offsets within these regions.
    // getOffsetWithinRegion returns this offset for a particular trace
    // buffer within the multi-surface region where it lives.
    //
    virtual UINT32 getOffsetWithinRegion( void ) const = 0;

    virtual void mapMemory( UINT32 offset,
                            UINT32 bytesToMap,
                            UINT64 * virtAddr,
                            UINT32 gpuSubdevIdx ) = 0;

    virtual void unmapMemory( UINT64 virtAddr,
                              UINT32 gpuSubdevIdx ) = 0;

    virtual const char * getName( void ) const = 0;
    virtual const char * getType( void ) const = 0;
    virtual UINT32 getSize( void ) const = 0;
    virtual UINT32 getPteKind( void ) const = 0;

    // get a pointer to this buffer's cached (in trace_3d memory) data.  Can return null if
    // there is no cache, then the user must call mapMemory() and read the data out that
    // way.
    //
    virtual UINT32 * Get032Addr( UINT32 subdev, UINT32 offset ) = 0;

    // set one of a buffer's properties (e.g.,  "permissions")
    //
    virtual void setProperty(const char* pPropName, const char* pPropValue) = 0;

    // readback data from the buffer using DMA if the user specified to use dma
    // transfer (via -dmaCheck), otherwise use CPU readback.  Assumes pData is
    // pointer to allocated memory of at least <size> bytes.
    //
    virtual bool ReadUsingDma( UINT32 offset, UINT32 size, UINT08 * pData,
                               UINT32 gpuSubdevIdx ) = 0;

    virtual UINT32 SendGpEntry(const char* chName, UINT64 offset,
                               UINT32 size, bool sync) = 0;

    virtual UINT32 WriteBuffer( UINT64 offset, const void* buf,
                                size_t size, UINT32 gpuSubdevIdx ) = 0;

    // physical address related functions
    virtual UINT32 getGpuPartitionIndex( UINT64 physAddr ) = 0;

    virtual UINT32 getGpuLtcIndex( UINT64 physAddr ) = 0;

    virtual UINT32 getGpuL2SliceIndex( UINT64 physAddr ) = 0;

    virtual UINT32 getGpuXbarRawAddr( UINT64 physAddr ) = 0;

    virtual UINT32 getGpuSubpartition( UINT64 physAddr ) = 0;

    virtual UINT32 getGpuPCIndex( UINT64 physAddr ) = 0;

    virtual UINT32 getGpuRBCAddr( UINT64 physAddr, UINT32 errCount, UINT32 errPos ) = 0;

    virtual UINT32 getGpuRBCAddrExt( UINT64 physAddr, UINT32 errCount, UINT32 errPos ) = 0;

    virtual UINT32 getGpuRBCAddrExt2( UINT64 physAddr, UINT32 errCount, UINT32 errPos ) = 0;

    virtual UINT32 getEccInjectRegVal( UINT64 physAddr, UINT32 errCount, UINT32 errPos ) = 0;

    virtual UINT32 getEccInjectExtRegVal( UINT64 physAddr, UINT32 errCount, UINT32 errPos ) = 0;

    virtual void   mapEccAddrToPhysAddr(const EccAddrParams &eccAddr, PHYSADDR *pPhysAddr) = 0;

    virtual bool IsAllocated() const = 0;
    virtual bool IsGpuMapped() const = 0;

    virtual UINT32 GetTagCount() const = 0;
    virtual const char* GetTag(UINT32 tagIndex) const = 0;

    virtual const vector<string>& GetTagList() const = 0;

    virtual SegmentList* CreateSegmentList(string which, UINT64 offset) = 0;

protected:

    // Protect the constructor so that the only way to create this object is
    // through calling the Create() factory function
    //
    explicit Buffer() {}
};

// Resource is a non-abstract class which implements functionality common to many
// resource types (e.g., property and name management)
//

class Resource
{
public:

    virtual ~Resource()  {}

    virtual const map<string,string> & getProperties( void ) const;
    virtual bool getProperty( const string & propName, string & propData ) const;
    virtual bool getProperty( const string & propName, UINT32 & propData ) const;
    virtual const string & getName( void ) const;
    virtual void printResource( void * pStream ) const;

public:

    explicit Resource( const char * name,
                       UINT32 nProps,
                       const char * * propNames,
                       const char * * propData )
        : m_name( name )
        {
            for ( UINT32 i = 0; i < nProps; ++i )
            {
                string key( propNames[i] );
                string data( propData[i] );

                m_properties[ key ] = data;
            }
        }

    explicit Resource( const char * name,
                       map< string, string > const& properties )
        : m_name( name ), m_properties( properties )
        {
        }

    void SetExtraProperty(const char* pPropName, const char* pPropData)
    {
        m_ExtraProps[pPropName] = pPropData;
    }

    // Get extra property data by name.
    //
    // @return NULL for property not found.
    const char* GetExtraPropertyData(const char* pPropName) const
    {
        map<string, string>::const_iterator it;
        for (it = m_ExtraProps.begin(); it != m_ExtraProps.end(); it++)
        {
            if (it->first == pPropName)
            {
                return it->second.c_str();
            }
        }
        return NULL;
    }

private:

    string m_name;
    map<string, string> m_properties;
    map<string, string> m_ExtraProps;

};

// the surface represents an implicit trace_3d surface like color buffers, zeta
// buffers, clip ID surfaces, zlwll surfaces, etc.
//

class Surface : public Resource
{
public:
    virtual ~Surface() {}

    static Surface * Create( BufferManager * pBufferManager,
                             void * pHostLWSurface,
                             const char * name,
                             UINT32 nProps,
                             const char * * propNames,
                             const char * * propData );

    virtual bool Alloc() = 0;
    virtual void Free() = 0;
    virtual void SetPitch(UINT32 pitch) = 0;
    virtual void SetPhysContig(bool physContig) = 0;
    virtual UINT64 GetGpuVirtAddr() = 0;

    virtual bool WriteTga( const char * fileName, UINT32 subdev ) const = 0;
    virtual bool WritePng( const char * fileName, UINT32 subdev ) const = 0;
    virtual bool Fill( UINT32 value, UINT32 subdev ) = 0;
    virtual bool FillRect( UINT32 x, UINT32 y, UINT32 width, UINT32 height,
                           UINT32 value, UINT32 subdev ) = 0;
    virtual bool FillRect64( UINT32 x, UINT32 y, UINT32 width, UINT32 height,
                             UINT16 r, UINT16 g, UINT16 b, UINT16 a,
                             UINT32 subdev ) = 0;
    virtual UINT32 ReadPixel( UINT32 x, UINT32 y ) const = 0;
    virtual void WritePixel( UINT32 x, UINT32 y, UINT32 value ) = 0;

    // ordering of data in image is from outermost to innermost iteration
    // order: arraySize, depth, height, width
    // ==> GetPixelOffset( x, y, z, a ) =
    //  a * width * height * depth * bytesPerPixel +
    //  z * width * height * bytesPerPixel +
    //  y * width * bytesPerPixel +
    //  x * bytesPerPixel
    //
    // total size is arraySize * depth * height * width * bytesPerPixel
    //
    // NOTE: caller must call "delete" on the UINT08* when done
    //
    virtual bool CreatePitchImage( UINT08 * * pPtrBuf, UINT32 subdev ) const = 0;

    // these are analogues of the Buffer classes functions.  Underneath it
    // all, they both map to MdiagSurf, but in trace_3d they're arranged
    // differently, "buffers" are packaged inside the "TraceModule" class,
    // and dynamic surfaces are stored as "Surface"s, so for now we have
    // some duplicate functions in T3dPlugin::Buffer and T3dPlugin::Surface
    //
    virtual void mapMemory( UINT32 offset, UINT32 bytesToMap,
                            UINT64 * virtAddr, UINT32 gpuSubdevIdx ) = 0;

    virtual void unmapMemory( UINT64 virtAddr, UINT32 gpuSubdevIdx ) = 0;

    // get the raw, start to finish, size in bytes of the Surface.  This can
    // be used to read out and write back the entire surface exactly as its
    // represented in memory.
    //
    virtual UINT64 getSizeInBytes() const = 0;

    // read a portion of the entire surface efficiently:
    // parameter types chosen to match SurfaceUtils::ReadSurface
    //
    virtual bool ReadSurface( UINT64 Offset, void * Buf, size_t BufSize,
                      UINT32 Subdev ) = 0;

    // write a portion of the entire surface efficiently:
    // parameter types chosen to match SurfaceUtils::WriteSurface
    //
    virtual bool WriteSurface( UINT64 Offset, const void * Buf, size_t BufSize,
                       UINT32 Subdev ) = 0;

    // get the offset of a pixel address
    //
    virtual UINT32 GetPixelOffset( UINT32 x, UINT32 y, UINT32 z = 0,
                                   UINT32 a = 0 ) = 0;

    // read a portion of the entire surface efficiently:
    // parameter types chosen to match SurfaceUtils::ReadSurface
    // will optionally use DMA transfer if the mods user has specified a
    // preference (via the -dmaCheck trace_3d parameter)
    //
    virtual bool ReadSurfaceUsingDma( UINT64 Offset, void * Buf,
                                      size_t BufSize, UINT32 Subdev ) = 0;

    // write a portion of the entire surface efficiently:
    // parameter types chosen to match SurfaceUtils::WriteSurface
    // will optionally use DMA transfer if the mods user has specified a
    // preference (via the -dmaCheck trace_3d parameter)
    //
    virtual bool WriteSurfaceUsingDma( UINT64 Offset, const void * Buf,
                                       size_t BufSize, UINT32 Subdev ) = 0;

    // same as CreatePitchImage but will use DMA for readback if the user has
    // specified to use dma (-dmaCheck).
    // NOTE: caller must call "delete[]" on the UINT08* when done
    //
    virtual bool CreatePitchImageUsingDma( UINT08 * * pPtrBuf, UINT32 subdev ) const = 0;

    // physical address related functions
    virtual UINT64 getGpuPhysAddr( UINT32 subdev ) const = 0;
    virtual UINT64 getPhysAddress( UINT64 virtAddr ) const = 0;
    virtual UINT64 getBAR1PhysAddress( UINT64 virtAddr, UINT32 subdev ) const = 0;

    virtual UINT32 getGpuPartitionIndex( UINT64 physAddr ) = 0;

    virtual UINT32 getGpuLtcIndex( UINT64 physAddr ) = 0;

    virtual UINT32 getGpuL2SliceIndex( UINT64 physAddr ) = 0;

    virtual UINT32 getGpuXbarRawAddr( UINT64 physAddr ) = 0;

    virtual bool IsSurfaceView() const = 0;

    virtual bool IsAllocated() const = 0;
    virtual bool IsGpuMapped() const = 0;

    struct CrcCheckInfo
    {
        UINT32 WinX, WinY, WinWidth, WinHeight;
        // more member can be added here if necessary
        CrcCheckInfo() { memset(this, 0, sizeof(CrcCheckInfo)); }
        bool Windowing() const { return WinWidth || WinHeight; }
    };

    virtual UINT32 CallwlateCrc(UINT32 subdev, const CrcCheckInfo* pInfo) = 0;

    // copied from surf2d.h
    enum VASpace
    {
        //! Default VA space is:
        //! - GPUVASpace on big GPU and CheetAh with big GPU (e.g. T124)
        DefaultVASpace          = 0
        ,GPUVASpace             = 1
    };

    virtual void SetVASpace(VASpace vaSpace) = 0;

    virtual UINT64 GetTegraVirtAddr() const = 0;

    // copied from surf2d.h
    enum MemoryMappingMode
    {
        MapDefault,
        MapDirect,
        MapReflected
    };

    virtual MemoryMappingMode GetMemoryMappingMode() const = 0;
    virtual void SetMemoryMappingMode(MemoryMappingMode mode) = 0;

    // copied from memtypes.h
    enum Location
    {
        Fb,          // GPU framebuffer
        Coherent,    // Sysmem, CPU-cached with cache snooping
        NonCoherent, // Sysmem, CPU-cached, no cache snooping, requires explicit flushing
        Optimal,     // On GPU - FB, on CheetAh - NonCoherent
    };

    virtual Location GetLocation() const = 0;
    virtual void SetLocation(Location location) = 0;

    // copied from surf2d.h
    enum GpuCacheMode
    {
        GpuCacheDefault,
        GpuCacheOff,
        GpuCacheOn
    };
    virtual GpuCacheMode GetGpuCacheMode() const = 0;
    virtual void SetGpuCacheMode(GpuCacheMode mode) = 0;
    virtual void SetVideoProtected(bool flag) = 0;
    virtual Surface* SwizzleCompressionTags() = 0;
    virtual SegmentList* CreateSegmentList(string which, UINT64 offset) = 0;

protected:

    // Protect the constructor so that the only way to create this object is
    // through calling the Create() factory function
    //
    explicit Surface( const char * name,
                      UINT32 nProps,
                      const char * * propNames,
                      const char * * propData )
        : Resource( name, nProps, propNames, propData )
        {}

};

class SegmentList
{
public:
    virtual ~SegmentList() {};

    // factory function for this object
    //
    static SegmentList * Create(SegmentList * pHostLWSegment);

    virtual UINT64 GetLength() const = 0;
    virtual Surface::Location GetLocation(UINT32 n) const = 0;
    virtual UINT64 GetSize(UINT32 n) const = 0;
    virtual UINT64 GetPhysAddress(UINT32 n) const = 0;
    virtual void Map(UINT32 n) = 0;
    virtual bool IsMapped(UINT32 n) const = 0;
    virtual UINT64 GetAddress(UINT32 n) const = 0;
    virtual void Unmap(UINT32 n) = 0;
protected:

    // Protect the constructor so that the only way to create this object is
    // through calling the Create() factory functions
    //
    explicit SegmentList() {}
};

// class Mem provides an interface for accessing virtual memory -- memory
// that has been mapped, e.g., with Buffer.mapMemory().  There's no
// reason to have a "MemManager" since individual instances of Mem
// are all identical -- there is no instance data, just member functions
//

enum MemoryAttrib
{
    UC = 1,  // UnCached
    WC = 2,  // Write-Combined
    WT = 3,  // Write-Through
    WP = 4,  // Write-Protect
    WB = 5,  // Write-Back
};

enum MemoryProtect
{
    Readable   = 0x1,
    Writeable  = 0x2,
    Exelwtable = 0x4,
    ReadWrite  = Readable | Writeable, // shorthand for common case
};

class Mem
{
public:

    virtual ~Mem() {}

    static Mem * Create( Mem * pHostMem );

    virtual UINT08 VirtRd08( UINT64 va ) const = 0;
    virtual UINT16 VirtRd16( UINT64 va ) const = 0;
    virtual UINT32 VirtRd32( UINT64 va ) const = 0;
    virtual UINT64 VirtRd64( UINT64 va ) const = 0;
    virtual void VirtRd( UINT64 va, void * data, UINT32 count) const = 0;

    virtual void VirtWr08( UINT64 va, UINT08 data ) = 0;
    virtual void VirtWr16( UINT64 va, UINT16 data ) = 0;
    virtual void VirtWr32( UINT64 va, UINT32 data ) = 0;
    virtual void VirtWr64( UINT64 va, UINT64 data ) = 0;
    virtual void VirtWr( UINT64 va, const void * data, UINT32 count) = 0;

    virtual UINT08 PhysRd08( UINT64 pa ) const = 0;
    virtual UINT16 PhysRd16( UINT64 pa ) const = 0;
    virtual UINT32 PhysRd32( UINT64 pa ) const = 0;
    virtual UINT64 PhysRd64( UINT64 pa ) const = 0;
    virtual void PhysRd( UINT64 pa, void * data, UINT32 count) const = 0;

    virtual void PhysWr08( UINT64 pa, UINT08 data ) = 0;
    virtual void PhysWr16( UINT64 pa, UINT16 data ) = 0;
    virtual void PhysWr32( UINT64 pa, UINT32 data ) = 0;
    virtual void PhysWr64( UINT64 pa, UINT64 data ) = 0;
    virtual void PhysWr( UINT64 pa, const void * data, UINT32 count) = 0;

    virtual bool LwRmMapMemory( UINT32 MemoryHandle,
                                UINT64 Offset,
                                UINT64 Length,
                                void  ** pAddress ) = 0;

    virtual void LwRmUnmapMemory( UINT32 MemoryHandle,
                                  volatile void * Address ) = 0;

    virtual bool LwRmMapMemoryDma( UINT32 hDma,
                                   UINT32 hMemory,
                                   UINT64 offset,
                                   UINT64 length,
                                   UINT32 flags,
                                   UINT64 * pDmaOffset ) = 0;

    virtual void LwRmUnmapMemoryDma( UINT32 hDma,
                                     UINT32 hMemory,
                                     UINT32 flags,
                                     UINT64 dmaOffset ) = 0;

    // calls pRm->Alloc( pLwRm->GetDeviceHandle(), <args>... )
    //
    virtual bool LwRmAllocOnDevice( UINT32 * pRtnObjectHandle,
                                    UINT32 Class,
                                    void * Parameters ) = 0;

    virtual void LwRmFree( UINT32 handle ) = 0;

    virtual void FlushCpuWriteCombineBuffer() = 0;

    virtual bool MapDeviceMemory
    (
        void**        pReturnedVirtualAddress,
        UINT64        PhysicalAddress,
        size_t        NumBytes,
        MemoryAttrib  Attrib,
        MemoryProtect Protect
    ) = 0;

    virtual void UnMapDeviceMemory( void* VirtualAddress ) = 0;

    virtual bool AllocIRam(UINT32 size, UINT64* VirtAddr, UINT64* PhysAddr) = 0;

    virtual void FreeIRam(UINT64 VirtAddr) = 0;
    virtual GpuDevice* GetGpuDevice() = 0;
    virtual LwRm * GetLwRmPtr() = 0;

protected:

    // Protect the constructor so that the only way to create this object is
    // through calling the Create() factory function
    //
    explicit Mem() {}

};

// class TraceManager provides an interface for accessing trace-
// specific functionality: enumerating channels, asking for the trace
// version number, "all channels" actions, etc.
//

// forward declaration
//
class TraceOp;

class TraceManager
{
public:

    virtual ~TraceManager() {}

    static TraceManager * Create( TraceManager * pHostTraceManager );

    // makes sure all sent methods have made it into the pushbuffer
    // and bumps the gpu's put pointer for all channels
    //
    virtual int FlushMethodsAllChannels( void ) = 0;

    // wait until the gpu's pushbuffer "get" pointers have all
    // reached the "put" pointers in all channels
    //
    virtual int WaitForDMAPushAllChannels( void ) const = 0;

    // retrieves the version number of the trace_3d trace
    // (i.e., the "test.hdr" protocol number)
    //
    virtual UINT32 getTraceVersion( void ) const = 0;

    // idles all channels (via polling) in the trace via an
    // RM IdleChannels call
    //
    virtual int WaitForIdleAllChannels( void ) const = 0;
    virtual int WaitForIdleOneChannel( UINT32 chNum ) const = 0;

    virtual void refreshTraceOps( void ) = 0;

    virtual const vector<TraceOp *> & getTraceOps( void ) const = 0;

    // (re)build the list of channels
    //
    virtual void refreshChannels() = 0;

    // fetch the list of channels.
    // A "channel" in this API is a resource who's property list maps
    // subchannel number to HW class number
    //
    virtual const vector<Resource> & getChannels( void ) const = 0;

    // get the trace_3d-side trace channel pointer for a named channel
    // This pointer is a required parameter for the below channel utility
    // functions
    //
    virtual void * getTraceChannelPointer( const char * chName ) const = 0;

    virtual int MethodWrite( void * pTraceChannel, int subchannel,
                             UINT32 method, UINT32 data) = 0;

    virtual int SetObject( void * pTraceChannel, int subch, UINT32 handle) = 0;

    virtual UINT32 CreateObject( void * pTraceChannel, UINT32 Class,
                                 void *Params = NULL,
                                 UINT32 *retClass = NULL) = 0;

    virtual void DestroyObject( void * pTraceChannel, UINT32 handle) = 0;

    virtual bool GetChannelRefCount( void * pTraceChannel, UINT32 * pValue ) const = 0;

    // build a resource (property list) of trace subchannel to replay subchannel mappings
    // for a particular channel
    //
    virtual Resource getSubchannels( void * pTraceChannel ) = 0;

    virtual UINT32 GetMethodCountOneChannel(UINT32 chNum) const = 0;
    virtual UINT32 GetMethodWriteCountOneChannel(UINT32 chNum) const = 0;

    virtual const char* getTraceHeaderFile() = 0;

    virtual const char* getTestName() = 0;

    virtual int RunTraceOps() = 0;

    virtual int StartPerfMon(void* pTraceChannel, const char* name) = 0;
    virtual int StopPerfMon(void* pTraceChannel, const char* name) = 0;

    // Get the syspipe od the SMC engine on which trace is running
    // For GA100, If SMC is not active, just return sys0
    // Prior to GA100, it will return empty string.
    virtual const char* GetSyspipe() = 0;

    virtual void StopVpmCapture() = 0;

    virtual void AbortTest() = 0;
protected:

    // Protect the constructor so that the only way to create this object is
    // through calling the Create() factory function
    //
    explicit TraceManager() {}

};

// the traceOp is an entirely plugin-side structure that caches the properties
// of a trace_3d - side trace op
//

class TraceOp : public Resource
{
public:
    virtual ~TraceOp() {}

    static TraceOp * Create( const char * name,
                             UINT32 nProps,
                             const char * * propNames,
                             const char * * propData );

    virtual UINT32 GetLineNumber() const = 0;

protected:

    // Protect the constructor so that the only way to create this object is
    // through calling the Create() factory function
    //
    explicit TraceOp( const char * name,
                      UINT32 nProps,
                      const char * * propNames,
                      const char * * propData )
        : Resource( name, nProps, propNames, propData )
        { }
};

// class UtilityManager provides an interface for accessing various
// useful mods utility functions: timers, thread yielding, etc.
//

enum SimulationMode
{
    Hardware,
    RTL,
    Cmodel,
    Amodel,
};

class Semaphore;

class UtilityManager
{
public:

    virtual ~UtilityManager() {}

    static UtilityManager * Create( UtilityManager * pHostUtilityManager );

    // gets the current time in NS
    //
    virtual UINT64 GetTimeNS( void ) = 0;

    // gets the number of clock ticks
    //
    virtual UINT64 GetSimulatorTime( void ) = 0;

    // gets the test hang timeout
    //
    virtual FLOAT64 GetTimeoutMS( void ) = 0;

    // yields to other threads of exelwtion (e.g., simulators)
    //
    virtual void _Yield( void ) = 0;

    // find out how many error strings there are
    //
    virtual UINT32 getNumErrorLoggerStrings( void ) const = 0;

    // get a particular error logger string
    //
    virtual const char * getErrorLoggerString( UINT32 strNum ) const = 0;

    // interact with a model via side-band escape calls
    //
    virtual int EscapeWrite( const char * Path, UINT32 Index, UINT32 Size,
                             UINT32 Value ) = 0;
    virtual int EscapeRead(  const char * Path, UINT32 Index, UINT32 Size,
                             UINT32 * Value ) = 0;

    // force MODS to exit without freeing underlying simulation resources
    //
    virtual int ExitMods() = 0;

    virtual SimulationMode getSimulationMode() const = 0;
    virtual bool isEmulation() const = 0;

    virtual UINT32 AllocEvent(UINT32* hEvent) = 0;
    virtual UINT32 FreeEvent(UINT32 hEvent) = 0;
    virtual UINT32 ResetEvent(UINT32 hEvent) = 0;
    virtual UINT32 SetEvent(UINT32 hEvent) = 0;
    virtual UINT32 IsEventSet(UINT32 hEvent, bool* isSet) = 0;
    virtual UINT32 HookPmuEvent(UINT32 hEvent, UINT32 index) = 0;
    virtual UINT32 UnhookPmuEvent(UINT32 index) = 0;

    virtual Semaphore* AllocSemaphore(UINT64 physAddr, SemaphorePayloadSize size, bool isSyncptShimSema = false) = 0;
    virtual void FreeSemaphore(Semaphore* pSema) = 0;

  //TestStatus must match Test::TestStatus
    enum TestStatus
    {
        TEST_NOT_STARTED        = 0,
        TEST_INCOMPLETE         = 1,
        TEST_SUCCEEDED          = 2,
        TEST_FAILED             = 3,
        TEST_FAILED_TIMEOUT     = 4,
        TEST_FAILED_CRC         = 5,
        TEST_NO_RESOURCES       = 6,
        TEST_CRC_NON_EXISTANT   = 7,
        TEST_FAILED_PERFORMANCE = 8,
        TEST_FAILED_ECOV        = 9,
        TEST_ECOV_NON_EXISTANT  = 10
    };
    virtual void FailTest(TestStatus status = TEST_FAILED) = 0;

    virtual UINT64 AllocRawSysMem(UINT64 nbytes) = 0;
    virtual void TriggerUtlUserEvent(const map<string, string> &userData) = 0;

    virtual UINT32 PciRead32(INT32 DomainNumber, INT32 BusNumber, INT32 DeviceNumber,
        INT32 FunctionNumber, INT32 Address, UINT32* Data) = 0;
    virtual UINT32 PciWrite32(INT32 DomainNumber, INT32 BusNumber, INT32 DeviceNumber,
        INT32 FunctionNumber, INT32 Address, UINT32 Data) = 0;
    virtual UINT32 FindPciDevice(INT32 DeviceId, INT32 VendorId, INT32 Index,
        UINT32* pDomainNumber, UINT32* pBusNumber, UINT32* pDeviceNumber, UINT32* pFunctionNumber) = 0;
    virtual UINT32 FindPciClassCode(INT32 ClassCode, INT32 Index,
        UINT32* pDomainNumber, UINT32* pBusNumber, UINT32* pDeviceNumber, UINT32* pFunctionNumber) = 0;

    virtual size_t GetVirtualMemoryUsed() = 0;

    virtual INT32 Printf(const char *Format, ...) = 0;

    virtual const char *GetOSString() = 0;

    virtual bool HookIRQ(UINT32 irq, ISR handler, void* cookie) = 0;
    virtual bool UnhookIRQ(UINT32 irq, ISR handler, void* cookie) = 0;

    virtual bool DisableIRQ(UINT32 irq) = 0;
    virtual bool EnableIRQ(UINT32 irq) = 0;

    virtual bool IsIRQHooked(UINT32 irq) = 0;

    virtual void ModsAssertFail(const char *file, int line,
        const char *function, const char *cond) = 0;

    virtual bool Poll(PollFunc Function, volatile void* pArgs,
        FLOAT64 TimeoutMs) = 0;

    virtual void DelayNS(UINT32 nanoseconds) const = 0;

    // The mailbox version of this API is for ARM use only.
    virtual INT32 VAPrintf(const char *Format, va_list Args) = 0;
    virtual UINT32 GetThreadId() = 0;
    virtual void DisableInterrupts() = 0;
    virtual void EnableInterrupts() = 0;

    virtual bool RegisterUnmapEvent() = 0;
    virtual bool RegisterMapEvent() = 0;

    virtual const vector<const char*>& GetChipArgs() = 0;

    virtual int GpuEscapeWriteBuffer(UINT32 GpuId, const char *Path, UINT32 Index, size_t Size, const void* Buf) = 0;
    virtual int GpuEscapeReadBuffer(UINT32 GpuId, const char *Path, UINT32 Index, size_t Size, void *Buf) = 0;

    virtual UINT32 TrainIntraNodeConnsParallel(UINT32 trainTo) = 0;

protected:

    // Protect the constructor so that the only way to create this object is
    // through calling the Create() factory function
    //
    explicit UtilityManager() {}

};

class JavaScriptManager
{
public:

    virtual ~JavaScriptManager() {}

    static JavaScriptManager * Create( JavaScriptManager * pHostJavaScriptManager );

    virtual INT32 LoadJavaScript(const char *fileName) = 0;

    virtual INT32 ParseArgs(const char *cmdLine) = 0;

    virtual INT32 CallVoidMethod(const char *methodName,
        const char **argValue,
        UINT32 argCount) = 0;

    virtual INT32 CallINT32Method(const char *methodName,
        const char **argValue,
        UINT32 argCount,
        INT32 *retValue) = 0;

    virtual INT32 CallUINT32Method(const char *methodName,
        const char **argValue,
        UINT32 argCount,
        UINT32 *retValue) = 0;

protected:

    // Protect the constructor so that the only way to create this object is
    // through calling the Create() factory function
    //
    explicit JavaScriptManager() {}
};

class DisplayManager
{
public:

    virtual ~DisplayManager() {}

    static DisplayManager * Create( DisplayManager * pHostDisplayManager );

    virtual void *GetChannelByHandle(UINT32 handle) const = 0;

    virtual INT32 MethodWrite(void *channel, UINT32 method, UINT32 data) = 0;

    virtual INT32 FlushChannel(void *channel) = 0;

    virtual INT32 CreateContextDma(UINT32 chHandle,
        UINT32 flags,
        UINT32 size,
        const char *memoryLocation,
        UINT32 *dmaHandle) = 0;

    virtual INT32 GetDmaCtxAddr(UINT32 dmaHandle, void **addr) = 0;

    virtual INT32 GetDmaCtxOffset(UINT32 dmaHandle, UINT64 *offset) = 0;

    virtual INT32 RmControl5070(UINT32 ctrlCmdNum,
                                void* params,
                                size_t paramsSize) = 0;

    virtual INT32 GetActiveDisplayIDs(UINT32 *pDisplayIDs,
                                      size_t *pSizeInBytes) = 0;

    virtual UINT32 GetMasterSubdevice() = 0;

    //
    // Data type defintion for display configuration parameters.
    // The parameter definition may be changed or appended
    // in the future, but interface GetConfigParams()  keep
    // unchanged. Different definitions are identified by
    // ParamTypeID.
    struct DisplayMngCfgParams
    {
        // Parameter type definition
        //
        struct CfgParamsType0
        {
            UINT32 ActiveHeight;
            UINT32 ActiveWidth;
            UINT32 SyncHeight;
            UINT32 SyncWidth;
            UINT32 BackPorchHeight;
            UINT32 BackPorchWidth;
            UINT32 FrontPorchHeight;
            UINT32 FrontPorchWidth;
            UINT32 Depth;
            UINT32 PixelClockHz;
            UINT32 BurstPixelClockHz;
        };

        struct CfgParamsType1 : public CfgParamsType0
        {
            UINT32 OutputResourcePixelDepth;
        };

        // Please append ParamType enum once a new type defined
        //
        enum ParamTypeID
        {
            ParamType0 = 0
            ,ParamType1
            ,ParamTypeLast
        };

        // Pamameter data
        //
        ParamTypeID TypeID;
        union
        {
            CfgParamsType0 Data0;
            CfgParamsType1 Data1;
        };

        // Init the structure to use Type0 by default
        DisplayMngCfgParams(): TypeID(ParamType1)
        {
        }
    };

    virtual INT32 GetConfigParams(UINT32 displayID,
                                  DisplayMngCfgParams *pParams) = 0;

protected:

    // Protect the constructor so that the only way to create this object is
    // through calling the Create() factory function
    //
    explicit DisplayManager() {}
};

class SOC
{
public:
    // Keep this inside SOC to be more clear it's an SOC SyncPoint
    class SyncPoint
    {
    public:
        virtual ~SyncPoint() {}

        static SyncPoint * Create( SyncPoint * pHostSyncPoint );

        virtual UINT32 GetIndex() const = 0;

        virtual void Wait(UINT32 threshold, bool base,
                          UINT32 baseIndex, bool _switch,
                          void * pTraceChannel = 0,
                          int subchannel = 0) = 0;

        virtual void HostIncrement(
                bool wfi = true, bool flush = true,
                void* pTraceChannel = 0, int subchannel = 0) = 0;

        virtual void GraphicsIncrement(
                bool wfi = true, bool flush = true,
                void* pTraceChannel = 0, int subchannel = 0) = 0;

    protected:
        explicit SyncPoint() {}
    };

    class SyncPointBase
    {
    public:
        virtual ~SyncPointBase() {}

        static SyncPointBase * Create( SyncPointBase * pHostSyncPointBase );

        virtual UINT32 GetIndex() const = 0;

        virtual UINT32 GetValue() const = 0;

        //In-band method
        virtual void AddValue( UINT32 value, bool wfi = true,
                               bool flush = true, void * pTraceChannel = 0,
                               int subchannel = 0) = 0;

        //In-band method
        virtual void SetValue( UINT32 value, bool wfi = true,
                               bool flush = true, void * pTraceChannel = 0,
                               int subchannel = 0) = 0;

        //Out-band method
        virtual void CpuAddValue( UINT32 value) = 0;

        //Out-band method
        virtual void CpuSetValue( UINT32 value) = 0;
    protected:
        explicit SyncPointBase() {}
    };

public:
    virtual ~SOC() {}
    static SOC * Create( SOC * pHostSOC );

    virtual UINT08 RegRd08(UINT32 offset) = 0;
    virtual void   RegWr08(UINT32 offset, UINT08 data) = 0;
    virtual UINT16 RegRd16(UINT32 offset) = 0;
    virtual void   RegWr16(UINT32 offset, UINT16 data) = 0;
    virtual UINT32 RegRd32(UINT32 offset) = 0;
    virtual void   RegWr32(UINT32 offset, UINT32 data) = 0;

    //The subdev is not used. It is kept here to be compatible with the previous implementation.
    virtual SOC::SyncPoint* AllocSyncPoint( UINT32 subdev = 0) = 0;

    virtual SOC::SyncPointBase* AllocSyncPointBase( UINT32 initialValue = 0,
                                                    bool forceIndex = false,
                                                    UINT32 index = 0 ) = 0;

    virtual bool IsSmmuEnabled() = 0;

protected:
    explicit SOC() {}
};

typedef SOC::SyncPoint SOCSyncPoint;
typedef SOC::SyncPointBase SOCSyncPointBase;

class Semaphore
{
    public:
        virtual ~Semaphore() {}
        static Semaphore* Create( Semaphore* pHostSemaphore );

        virtual void Release(void* pTraceChannel, UINT64 data, bool flush = false) = 0;
        virtual void Acquire(void* pTraceChannel, UINT64 data) = 0;
        virtual void AcquireGE(void* pTraceChannel, UINT64 data) = 0;
        virtual UINT64 GetPhysAddress() = 0;
        virtual void SetReleaseFlags(void* pTraceChannel, UINT32 flags) = 0;

    protected:
        explicit Semaphore() {}
};

} // namespace T3dPlugin

#endif // __PLUGIN_H__
