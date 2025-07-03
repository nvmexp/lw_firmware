#ifndef __T3PLUGIN_H__
#define __T3PLUGIN_H__

#include "plugin.h"     // the interface available to the plugin, defines
                        // functionality and version of the interface supported
                        // by the trace_3d side

// This file contains the trace_3d-side abstract interface classes that extend
// the shared interface classes with functionality needed to implement the
// trace_3d/plugin interface.   They are derived from the shared interface
// classes only where needed.   All the derived trace_3d side interface classes
// are named <Foo>Mods.   E.g., there is a HostMods interface which is derived
// from the shared Host interface

// forward declaration
//
class Trace3DTest;
class TraceModule;
class GpuSubdevice;
class MdiagSurf;

#include "core/include/framebuf.h"
#include "core/include/jscript.h"

class SyncPoint; // gpu/utility/syncpoint.h
class SyncPointBase; // gpu/utility/syncpoint.h
typedef SyncPoint ModsSyncPoint;
typedef SyncPointBase ModsSyncPointBase;

namespace T3dPlugin
{

// Hack: enum of API name to index apiReceivers[].
// Will remove this once we create all receivers.
enum
{
    XXX_FIRST_SHIM_INDEX_XXX = 0,
#define USE_API_LIST
#define API_ENTRY(api) ENUM_##api,
#include "api_list.h"
#undef API_ENTRY
    // last entry must be zero
    XXX_LAST_SHIM_INDEX_XXX
};

// inherit the shared interface to EventManager, and add more functionality
// and specializations that we need on the mods side.   These are named
// (object)Mods (e.g., Host is the shared interface, HostMods has mods-side
// extensions and specializations )
//

// forward declarations
class EventMods;
class EventManagerMods;

class HostMods : public Host
{
public:

    static HostMods * Create( Trace3DTest * pTest,
                              pFnShutdown_t pFnShutdown );
    virtual ~HostMods() {}

    virtual EventManagerMods * getModsEventManager( void ) = 0;
    virtual void Shutdown( void ) = 0;
    virtual Trace3DTest * getT3dTest( void ) const = 0;

protected:
    explicit HostMods() {}

};

class EventManagerMods : public EventManager
{
public:

    static EventManagerMods * Create( HostMods * pHost );

    virtual ~EventManagerMods() {}

    // add an event to this event manager's list of registered events
    //
    virtual bool RegisterEvent( EventMods * pEvent ) = 0;

    // an event described by the input parameter has olwrred in trace_3d, ilwoke
    // the callbacks on all matching registered events
    //
    virtual bool EventOclwrred( const EventMods * pEvent ) = 0;

protected:

    // Protect the constructor so that the only way to create this object is
    // through calling the Create() factory function
    //
    explicit EventManagerMods() {}

};

// inherit the shared interface to Event, and add more functionality
// that we need on the mods side
//
class EventMods : public Event
{
public:

    static EventMods * Create( HostMods * pHost,
                               const char * eventName );

    virtual ~EventMods() {}

    // does the argument event match this event?
    //
    virtual bool matches( const EventMods * pEvent ) = 0;

    // copy (and replace) the generated event's generated params into this
    // event's generated params
    //
    virtual void assignGeneratedParams( const EventMods * pEventGenerated ) = 0;

protected:

    // Protect the constructor so that the only way to create this object is
    // through calling the Create() factory function
    //
    explicit EventMods() {}

};

class GpuManagerMods : public GpuManager
{
public:

    static GpuManagerMods * Create( HostMods * pHost );

};

class GpuMods : public Gpu
{
public:

    static GpuMods * Create( HostMods * pHost, UINT32 gpuNum );

    virtual GpuSubdevice * getSubdev() const = 0;

};

class BufferManagerMods : public BufferManager
{
public:

    static BufferManagerMods * Create( HostMods * pHost );

    virtual Trace3DTest * getT3dTest( void ) const = 0;

};

class BufferMods : public Buffer
{
public:

    static BufferMods * Create( HostMods * pHost,
                                const char * name,
                                const char * type,
                                UINT32 sizeInBytes,
                                const UINT32 * pData,
                                TraceModule * pTraceModule );

    virtual void copyData( UINT32 * pDest, UINT32 nWords ) const = 0;
    virtual void freeData() = 0;
    virtual bool hasPluginInitData() const = 0;
    virtual TraceModule * getTraceModule() const = 0;

};

class MemMods : public Mem
{
public:

    static MemMods * Create( HostMods * pHost );

};

class TraceManagerMods : public TraceManager
{
public:

    static TraceManagerMods * Create( HostMods * pHost );

    virtual Trace3DTest * getT3dTest( void ) const = 0;

    virtual const char* GetSyspipe() = 0;
};

class UtilityManagerMods : public UtilityManager
{
public:

    static UtilityManagerMods * Create( HostMods * pHost );

};

class JavaScriptManagerMods : public JavaScriptManager
{
public:

    static JavaScriptManagerMods * Create( HostMods * pHost );

protected:

    virtual RC InternalCallMethod(const char *methodName,
        const char **argValue,
        UINT32 argCount,
        jsval *retValue) = 0;
};

class DisplayManagerMods : public DisplayManager
{
public:

    static DisplayManagerMods * Create( HostMods * pHost );

};

// Helper function to get FbDecode object.
// Used by both Buffer and Surface classes.
void GetFbDecode(FbDecode &decode, const MdiagSurf *pSurf, UINT64 physAddr);
//Function to get RBC details for Ecc error tests
void GetRBCAddress(EccAddrParams &decode, const MdiagSurf *pSurf, UINT64 physAddr,UINT32 errCount, UINT32 errPos);
void GetEccInjectRegVal(EccErrInjectParams &injParams, const MdiagSurf *pSurf, UINT64 physAddr,UINT32 errCount, UINT32 errPos);

class SOCMods : public SOC
{
public:

    static SOCMods * Create( HostMods * pHost );

};

class SyncPointMods : public SOCSyncPoint
{
public:

    static SyncPointMods * Create(HostMods* m_pHost, ModsSyncPoint* pHostSyncPoint);

protected:
    explicit SyncPointMods() {}
};

class SyncPointBaseMods : public SOCSyncPointBase
{
public:

    static SyncPointBaseMods * Create(HostMods* m_pHost, ModsSyncPointBase* pHostSyncPoint);

protected:
    explicit SyncPointBaseMods() {}
};

class SemaphoreMods : public Semaphore
{
public:

    static SemaphoreMods * Create(HostMods* m_pHost, UINT64 physAddr, SemaphorePayloadSize size, bool isSyncptShimSema = false);

protected:
    explicit SemaphoreMods() {}
};

} // namespace T3dPlugin

#endif // #ifndef __T3DPLUGIN_H__
