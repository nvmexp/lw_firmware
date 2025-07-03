/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h"
// first.
#include "mdiag/tests/stdtest.h"
#include "t3plugin.h"

#include "api_receiver.h"
#include "class/cla26f.h"
#include "class/clb06f.h"
#include "class/clc06f.h"
#include "class/clc36f.h"
#include "class/clc46f.h"
#include "class/clc56f.h"       // AMPERE_CHANNEL_GPFIFO_A
#include "class/clc86f.h"       // HOPPER_CHANNEL_GPFIFO_A

#include "core/include/evntthrd.h"
#include "core/include/isrdata.h"
#include "core/include/registry.h"
#include "core/utility/errloggr.h"
#include "device/interface/lwlink/lwlpowerstate.h"
#include "device/interface/pcie.h"
#include "gpu/perf/pmusub.h"
#include "gpu/utility/surfrdwr.h"
#include "gpu/utility/syncpoint.h"

#if defined(INCLUDE_LWLINK)
// TrainIntraNodeConnsParallel
#include "lwlink_lib_ctrl.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_lwlink_libif_user.h" 
#include "gpu/utility/lwlink/lwlinkdevif/lwl_devif.h"
#endif

#include "gputrace.h"
#include "mdiag/advschd/pmsurf.h"
#include "mdiag/advschd/policymn.h"
#include "mdiag/resource/lwgpu/dmasurfr.h"
#include "mdiag/resource/lwgpu/verif/GpuVerif.h"
#include "mdiag/smc/smcengine.h"
#include "mdiag/utils/types.h"
#include "mdiag/utils/utils.h"
#include "mdiag/utl/utl.h"

#include "lwos.h"
#include "lwRmReg.h"
#include "teegroups.h"
#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"
#include "trace_3d.h"
#include "tracemod.h"
#include "traceop.h"

#include <list>
#include <string>
#include <map>
#include <cerrno>
#include <limits>

#define MSGID() T3D_MSGID(PluginAPI)

namespace T3dPlugin
{
// This file contains the concrete classes that implement the trace_3d side of
// the trace_3d plugin interface, and the implementation of the member functions
// that provide services to the plugin
//

extern ApiReceiver *apiReceivers[];
extern const char *shimFuncNames[];

class Host_impl : public HostMods
{
public:
    friend HostMods * HostMods::Create( Trace3DTest * pTest,
                                        pFnShutdown_t pFnShutdown );
    explicit Host_impl()
        : m_pEventManager( 0 ),
          m_pGpuManager( 0 ),
          m_pBufferManager( 0 ),
          m_pTest( 0 ),
          m_pFnShutdown( 0 ),
          m_pMem( 0 ),
          m_pTraceManager( 0 ),
          m_pUtilityManager( 0 ),
          m_pJavaScriptManager( 0 ),
          m_pDisplayManager( 0 ),
          m_pSOC( 0 )
        {
        }

    virtual ~Host_impl()
        {
            delete m_pEventManager;
            delete m_pGpuManager;
            delete m_pBufferManager;
            delete m_pMem;
            delete m_pTraceManager;
            delete m_pUtilityManager;
            delete m_pJavaScriptManager;
            delete m_pDisplayManager;
            delete m_pSOC;
        }

    // functions to get pointers to  manager objects, which contain member
    // functions to perform actions on,
    // or query information from, the trace or the gpu
    //
    EventManagerMods * getModsEventManager()
        {
            return m_pEventManager;
        }
    GpuManager * getGpuManager()
        {
            return m_pGpuManager;
        }

    BufferManager * getBufferManager()
        {
            return m_pBufferManager;
        }

    EventManager * getEventManager()
        {
            return m_pEventManager;
        }

    TraceManager * getTraceManager()
        {
            return m_pTraceManager;
        }

    JavaScriptManager * getJavaScriptManager()
        {
            return m_pJavaScriptManager;
        }

    DisplayManager * getDisplayManager()
        {
            return m_pDisplayManager;
        }

    void Shutdown( void )
        {
            // shutdown all plugins, need to change to list of plugins, there
            // may be more than one
            //
            if ( m_pFnShutdown )
                m_pFnShutdown( this );
        }

    Trace3DTest * getT3dTest( void ) const
        {
            return m_pTest;
        }

    Mem * getMem( void )
        {
            return m_pMem;
        }

    UtilityManager * getUtilityManager( void )
        {
            return m_pUtilityManager;
        }

    void sendMessage( const char * msg, UINT32 value )
        {
            m_messages[ msg ] = value;
        }

    bool getMessage( const char * msg, UINT32 * pValue )
        {
            map<string,UINT32>::iterator it = m_messages.find( msg );
            if ( it == m_messages.end() )
                return false;

            *pValue = it->second;
            m_messages.erase( it );
            return true;
        }

    UINT32 getMajorVersion() const
    {
        return T3dPlugin::majorVersion;
    }

    UINT32 getMinorVersion() const
    {
        return T3dPlugin::minorVersion;
    }

    SOC * getSOC()
    {
        return m_pSOC;
    }

private:

    EventManagerMods  * m_pEventManager;
    GpuManagerMods    * m_pGpuManager;
    BufferManagerMods * m_pBufferManager;
    Trace3DTest       * m_pTest;
    pFnShutdown_t       m_pFnShutdown;
    Mem               * m_pMem;
    TraceManager      * m_pTraceManager;
    UtilityManager    * m_pUtilityManager;
    JavaScriptManager * m_pJavaScriptManager;
    DisplayManager    * m_pDisplayManager;
    SOC               * m_pSOC;

    map<string,UINT32> m_messages;
};

// HostMods:Create
// called from Trace3DTest::LoadPlugin to create the object that represents an
// instance of a plugin in this Trace3DTest.   a HostMods structure is the
// "top-level" object, from which all plugin functionality is accessed via
// various "managers".  A manager is an object that allows a plugin
// access to query or change trace_3d state, GPU state, or memory, or to
// register trace event callbacks.
//
// returns: pointer to HostMods object (or null on failure)
//

HostMods * HostMods::Create( Trace3DTest * pTest,
                             pFnShutdown_t pFnShutdown )
{
    Host_impl * pHost = new Host_impl;
    if ( ! pHost )
        return 0;

    pHost->m_pTest = pTest;
    pHost->m_pFnShutdown = pFnShutdown;

    pHost->m_pEventManager = EventManagerMods::Create( pHost );
    if ( ! pHost->m_pEventManager )
    {
        delete pHost;
        return 0;
    }

    pHost->m_pGpuManager   = GpuManagerMods::Create( pHost );
    if ( ! pHost->m_pGpuManager )
    {
        delete pHost;
        return 0;
    }

    pHost->m_pBufferManager = BufferManagerMods::Create( pHost );
    if ( ! pHost->m_pBufferManager )
    {
        delete pHost;
        return 0;
    }

    pHost->m_pMem = MemMods::Create( pHost );
    if ( ! pHost->m_pMem )
    {
        delete pHost;
        return 0;
    }

    pHost->m_pTraceManager = TraceManagerMods::Create( pHost );
    if ( ! pHost->m_pTraceManager )
    {
        delete pHost;
        return 0;
    }

    pHost->m_pUtilityManager = UtilityManagerMods::Create( pHost );
    if ( ! pHost->m_pUtilityManager )
    {
        delete pHost;
        return 0;
    }

    pHost->m_pJavaScriptManager = JavaScriptManagerMods::Create( pHost );
    if ( ! pHost->m_pJavaScriptManager )
    {
        delete pHost;
        return 0;
    }

    pHost->m_pDisplayManager = DisplayManagerMods::Create( pHost );
    if ( ! pHost->m_pDisplayManager )
    {
        delete pHost;
        return 0;
    }

    pHost->m_pSOC = SOCMods::Create( pHost );
    if ( ! pHost->m_pSOC )
    {
        delete pHost;
        return 0;
    }

    return pHost;
}

// inherit from the mods-enhanced EventManager class, not directly from the
// shared interface, because we need additional member functions dealing with
// managing and processing the list of registered events
//
class EventManager_impl : public EventManagerMods
{
public:

    friend EventManagerMods * EventManagerMods::Create( HostMods * pHost );

    explicit EventManager_impl( HostMods * pHost ) : m_pHost( pHost ) {}
    virtual ~EventManager_impl()
        {
            list< Event * >::iterator it = m_allEventList.begin();
            list< Event * >::iterator it_end = m_allEventList.end();

            for ( ; it != it_end; ++it )
            {
                delete *it;
            }
        }

    // factory function for events.   Events are the key object in the trace_3d
    // plugin.  Plugins create and register events with trace_3d, and trace_3d
    // transfers control to the plugin when the event oclwrs, allowing the
    // plugin to take action: create more events, access the status of the
    // trace, read / write GPU registers, etc.
    //
    Event * getEvent( const char * eventName )
        {
            EventMods * pEvent = EventMods::Create( m_pHost, eventName );
            m_allEventList.push_back( pEvent );
            return pEvent;
        }

    // iterators to loop through the list of available event names
    // TBD
    const char * beginEventNames( void ) { return 0; }
    const char * endEventNames( void ) { return 0; }

    // from the EventManagerMods interface
    //
    bool RegisterEvent( EventMods * pEvent )
        {
            m_registeredEventList.push_back( pEvent );
            bool rv = true;
            return rv;
        }

    bool EventOclwrred( const EventMods * pEventOclwrred )
        {
            // try to match the event which just oclwrred against the list of
            // registered events, ilwoke the callback on all matching
            // registered events
            //
            list< EventMods * >::iterator it = m_registeredEventList.begin();
            list< EventMods * >::iterator end_it = m_registeredEventList.end();
            for ( ; it != end_it; ++it )
            {
                EventMods * pEvent = *it;
                if ( pEvent->matches( pEventOclwrred ) )
                {
                    // set the pEventOclwrred generated params into pEvent
                    // (so the plugin can read them) and ilwoke pEvent's
                    // callback
                    //
                    pEvent->assignGeneratedParams( pEventOclwrred );
                    if(pEvent->IlwokeCallback() != 0)
                        return false;
                }
            }

            // check for event deletions that may have been requested by the
            // plugin during IlwokeCallback()
            //
            list< Event * >::iterator it2 = m_deleteEventsList.begin();
            list< Event * >::iterator it2_end = m_deleteEventsList.end();
            for ( ; it2 != it2_end; ++it2 )
            {
                reallyDeleteEvent( *it2 );
            }

            // we've processed all event deletions, clear the list now
            //
            m_deleteEventsList.clear();
            return true;
        }

    // This function marks the input event for deletion by adding it to a list
    // of events to delete.   We can't delete the object directly from the
    // plugin because the object's IlwokeCallback() may be active.  Once the
    // IlwokeCallback() function returns, in EventOclwrred(), we'll check to
    // see if any events are marked for deletion, and it so, we'll call
    // reallyDeleteEvent() to truly delete them.   This "delayed delete" allows
    // a plugin event handler callback to delete the calling event at the end of
    // the handler if the event is no longer needed.
    //
    void deleteEvent( Event * pEvent )
        {
            list< Event * >::iterator it = std::find( m_allEventList.begin(),
                                                      m_allEventList.end(),
                                                      pEvent );
            if ( it != m_allEventList.end() )
                m_deleteEventsList.push_back( pEvent );
            else
                ErrPrintf( "trace_3d: deleteEvent: event %p doesn't exist!\n",
                           pEvent );
        }

private:

    HostMods * m_pHost;

    // holds those events which have registered a callback for a particular
    // trace Event
    //
    list< EventMods * > m_registeredEventList;

    // records all events lwrrently in existance
    //
    list< Event * > m_allEventList;

    // keep track of events we need to delete after we're done processing an
    // event's IlwokeCallback()
    //
    list< Event * > m_deleteEventsList;

    // reallyDeleteEvent is the companion member funciton to deleteEvent().
    // It's called from EventOclwrred() to truly delete an event which have
    // been marked for deletion in deleteEvent().
    //
    void reallyDeleteEvent( Event * pEvent )
        {
            list< Event * >::iterator it = std::find( m_allEventList.begin(),
                                                      m_allEventList.end(),
                                                      pEvent );
            if ( it != m_allEventList.end() )
                m_allEventList.erase( it );
            else
            {
                ErrPrintf( "trace_3d: reallyDeleteEvent: internal error: "
                           "event %p doesn't exist!\n", pEvent );
                return;
            }

            EventMods * pEventMods = static_cast< EventMods * >( pEvent );
            list< EventMods * >::iterator itmods =
                std::find( m_registeredEventList.begin(),
                           m_registeredEventList.end(), pEventMods );
            if ( itmods != m_registeredEventList.end() )
                m_registeredEventList.erase( itmods );

            delete pEvent;
        }
};

EventManagerMods * EventManagerMods::Create( HostMods * pHost )
{
    EventManager_impl * pEventManager = new EventManager_impl( pHost );
    return pEventManager;
}

class Event_impl : public EventMods
{
public:

    friend EventMods * EventMods::Create( HostMods * pHost, const char * eventName);

    explicit Event_impl( HostMods * pHost, const char * eventName )
        : m_pHost( pHost ),
          m_eventName( eventName ) {}

    virtual ~Event_impl() {}

    // functions to set the value of this event's parameters
    //
    int setTemplateUint32Arg( const char * argName, UINT32 value )
        {
            m_TemplateParamMapUint32[ argName ] = value;
            int rv = 0;
            return rv;
        }
    int setTemplateStringArg( const char * argName, const char * value )
        {
            m_TemplateParamMapString[ argName ] = value;
            int rv = 0;
            return rv;
        }

    bool getTemplateUint32Arg( const char * argName, UINT32 * pValue ) const
        {
            map< const string, UINT32 >::const_iterator it =
                m_TemplateParamMapUint32.find( argName );
            if ( it == m_TemplateParamMapUint32.end() )
            {
                return false;
            }
            *pValue = it->second;
            return true;
        }

    bool getTemplateStringArg( const char * argName, const char * * pValue ) const
        {
            map< const string, string >::const_iterator it =
                m_TemplateParamMapString.find( argName );
            if ( it == m_TemplateParamMapString.end() )
            {
                return false;
            }

            *pValue = (it->second).c_str();
            return true;
        }

    int setGeneratedUint32Arg( const char * argName, UINT32 value )
        {
            m_GeneratedParamMapUint32[ argName ] = value;
            int rv = 0;
            return rv;
        }
    int setGeneratedStringArg( const char * argName, const char * value )
        {
            m_GeneratedParamMapString[ argName ] = value;
            int rv = 0;
            return rv;
        }

    bool getGeneratedUint32Arg( const char * argName, UINT32 * pValue ) const
        {
            map< const string, UINT32 >::const_iterator it =
                m_GeneratedParamMapUint32.find( argName );
            if ( it == m_GeneratedParamMapUint32.end() )
            {
                return false;
            }

            *pValue = it->second;
            return true;
        }

    bool getGeneratedStringArg( const char * argName, const char * * pValue ) const
        {
            map< const string, string >::const_iterator it =
                m_GeneratedParamMapString.find( argName );
            if ( it == m_GeneratedParamMapString.end() )
            {
                return false;
            }

            *pValue = (it->second).c_str();
            return true;
        }

    // register this event with trace_3d: When a matching trace event oclwrs,
    // the given callback is called, passing the given payload as a parameter
    //
    bool _register( pFnCallback_t pFnCallback, void * payload )
        {
            m_pFnCallback = pFnCallback;
            m_payload = payload;

            // add this event to a trace_3d side structure which remembers all
            // events
            //
            bool rv = m_pHost->getModsEventManager()->RegisterEvent( this );

            return rv;
        }

    // unregister the event with trace_3d: the plugin will no longer be called
    // when this event oclwrs
    //
    bool unregister( void )
        {
            bool rv = true;

            // XXX TBD
            // just call event manager Unregister event
            //

            return rv;
        }

    // return 0 if successful, otherwise it means something
    // wrong happened during the IlwokeCallback
    int IlwokeCallback()
        {
            Trace3DTest *trace = m_pHost->getT3dTest();
            if (trace->IsTopPlugin())
            {
                MailBox *pluginMailBox = trace->GetPluginMailBox();
                MailBox *traceMailBox = trace->GetTraceMailBox();

                pluginMailBox->Acquire();

                pluginMailBox->ResetDataOffset();
                pluginMailBox->Write(m_pFnCallback);
                pluginMailBox->Write(m_payload);
                pluginMailBox->WriteCommand(MailBox::CMD_T3DEVENT_REQ);

                pluginMailBox->Send();

                while (true)
                {
                    do {
                        MailBox::IRQStatus status = traceMailBox->ReadIRQStatus();
                        if (status != MailBox::IRQ_OK)
                        {
                            // usually status equals to MailBox::IRQ_OK
                            // but when FailTest is called in interrupt
                            // handler at plugin side, the process on the
                            // related CPU will exit, before exit, it changes
                            // the IRQ status to info mods that the process
                            // on the related CPU is exited, so mods should
                            // not interact with plugin side any more.
                            if (trace->HandleClientIRQStatus(status))
                                return 0;
                            else
                                return 1;
                        }
                        else
                        {
                            Tasker::Yield();
                        }
                    } while (!traceMailBox->HaveMail());

                    traceMailBox->ResetDataOffset();
                    MailBox::Command cmd = traceMailBox->ReadCommand();

                    if (MailBox::CMD_T3DAPI_REQ == cmd)
                    {
                        DebugPrintf("mods get a request from the mailbox\n");
                        UINT32 apiId = traceMailBox->Read<UINT32>();
                        if (apiReceivers[apiId] != 0)
                        {
                            DebugPrintf("%s: Calling API %s through mailbox protocol.\n", __FUNCTION__, T3dPlugin::shimFuncNames[apiId - 1]);
                            (*T3dPlugin::apiReceivers[apiId])(pluginMailBox, traceMailBox);

                            if(apiId == T3dPlugin::ENUM_UtilityManager_FailTest)
                            {
                                if (trace->GetClientStatus() == Test::TEST_SUCCEEDED)
                                    return 0;
                                else
                                    return 1;
                            }
                        }
                        else
                        {
                            ErrPrintf("%s: API %s does not support mailbox protocol yet.\n", __FUNCTION__, shimFuncNames[apiId - 1]);
                            return RC::SOFTWARE_ERROR;
                        }
                    }
                    else if (MailBox::CMD_T3DEVENT_ACK == cmd)
                    {
                        DebugPrintf("mods get an ack from the mailbox\n");
                        // Let thread yield when an event handler was done, though not necessary.
                        Tasker::Yield();
                        break;
                    }
                    else
                    {
                        WarnPrintf("mods get an unexpeted command from mailbox. cmd = %d\n", cmd);
                    }

                    // in case of top rtl, the simulator need be clocked
                    if (trace->GetParam()->ParamUnsigned("-cpu_model") == 2 && Platform::GetSimulationMode() != Platform::Hardware)
                        Platform::ClockSimulator(1);
                }

                traceMailBox->ResetDataOffset();

                traceMailBox->Release();
                return 0;
            }
            else
            {
                int result = m_pFnCallback( m_payload );
                DebugPrintf("%s: Plugin event handler returns %d.\n", __FUNCTION__, result);
                //W/O the option, treat plugin event handler process successfully
                if (trace->GetParam()->ParamPresent("-check_event_handler_retval") == 0)
                {
                    result = 0;
                }
                return result;
            }
        }

    const char * getName( void ) const
        {
            return m_eventName.c_str();
        }

    // EventMods functionality
    // returns true if the parameter generated event matches this event
    //     else returns false
    //
    bool matches( const EventMods * pEventGenerated )
        {
            // the event names and template/generated parameters must match for
            // two events to match
            //
            // does the generated event's generated parameters match
            // this event's template parameters?
            //
            // A template event matches a generated even if and only if
            // each template parameter exists and its value matches
            // the generated parameter.
            //
            if ( m_eventName != pEventGenerated->getName() )
            {
                return false;
            }

            // check that UINT32 parameters match
            //
            map< const string, UINT32 >::iterator itTemplUint;
            for ( itTemplUint = m_TemplateParamMapUint32.begin();
                  itTemplUint != m_TemplateParamMapUint32.end();
                  ++itTemplUint )
            {
                // given a template parameter, look it up in the generated event
                //
                const char * templParamName = (itTemplUint->first).c_str();
                UINT32 gelwalue = 0;
                bool paramExists = pEventGenerated->getGeneratedUint32Arg(
                    templParamName, &gelwalue );

                // does the template parameter exist in the generated event?
                //
                if ( ! paramExists )
                {
                    // no, events don't match
                    //
                    return false;
                }

                // yes the named parameter exists, do the values match?
                //
                if ( itTemplUint->second != gelwalue )
                {
                    // no, events don't match
                    //
                    return false;
                }

                // yes, this parameter matches, continue checking all
                // remaining parameters
                //
            }

            // check that string parameters match
            //
            map< const string, string >::iterator itTemplString;
            for ( itTemplString = m_TemplateParamMapString.begin();
                  itTemplString != m_TemplateParamMapString.end();
                  ++itTemplString )
            {
                // given a template parameter, look it up in the generated event
                //
                const char * templParamName = (itTemplString->first).c_str();
                const char * gelwalue = 0;
                bool paramExists = pEventGenerated->getGeneratedStringArg(
                    templParamName, &gelwalue );

                // does the template parameter exist in the generated event?
                //
                if ( ! paramExists )
                {
                    // no, events don't match
                    //
                    return false;
                }

                string templValStr( itTemplString->second );
                string gelwalueStr( gelwalue );

                // yes the named parameter exists, do the values match?
                //
                if ( templValStr != gelwalueStr )
                {
                    // no, events don't match
                    //
                    return false;
                }

                // yes, this parameter matches, continue checking all
                // remaining parameters
                //
            }

            // event names match, all parameters match, events match
            //
            return true;
        }

    // replace this event's generated params with the generated event's
    // generated params
    //
    void assignGeneratedParams( const EventMods * pEventGenerated )
        {
            const Event_impl * pEvntGen = static_cast<const Event_impl *>( pEventGenerated );

            m_GeneratedParamMapString = pEvntGen->m_GeneratedParamMapString;
            m_GeneratedParamMapUint32 = pEvntGen->m_GeneratedParamMapUint32;
        }

    Host * getHost( void )
        {
            return m_pHost;
        }

private:

    HostMods * m_pHost;
    string m_eventName;

    // template parameters:
    // set by plugin
    //
    map< const string, UINT32 > m_TemplateParamMapUint32;
    map< const string, string > m_TemplateParamMapString;

    // generated parameters:
    // set by trace_3d
    //
    map< const string, UINT32 > m_GeneratedParamMapUint32;
    map< const string, string > m_GeneratedParamMapString;

    pFnCallback_t m_pFnCallback;
    void * m_payload;

};

EventMods * EventMods::Create( HostMods * pHost, const char * eventName )
{
    Event_impl * pEvent = new Event_impl( pHost, eventName );
    return pEvent;
}

class GpuManagerMods_impl : public GpuManagerMods
{
public:

    friend GpuManagerMods * GpuManagerMods::Create( HostMods * pHost );

    explicit GpuManagerMods_impl( HostMods * pHost )
        : m_pHost( pHost ), m_numGpus( 0 )
        {
        }
    virtual ~GpuManagerMods_impl()
        {
            list< Gpu * >::iterator it = m_gpuList.begin();
            list< Gpu * >::iterator end_it = m_gpuList.end();

            for ( ; it != end_it; ++it )
            {
                delete *it;
            }
        }

    // returns the # of GPUs on the system (which are accessible to the plugin)
    //
    UINT32 getNumGpus( void )
        {
            return m_numGpus;
        }

    // factory function for Gpu objects.  Gpu objects contain member functions
    // which allow the plugin to read and modify the GPU state (registers,
    // memory)
    //
    Gpu * getGpu( UINT32 gpuNumber )
        {
            GpuMods * pGpu = 0;

            if ( gpuNumber >= m_numGpus )
            {
                WarnPrintf( "trace_3d: getGpu(), asking for a gpu that doesn't"
                            "exist, maxGpuNum = %d\n", m_numGpus );
            }
            else
            {
                pGpu = GpuMods::Create( m_pHost, gpuNumber );
                m_gpuList.push_back( pGpu );
            }

            return pGpu;
        }

    void deleteGpu( Gpu * pGpu )
        {
            list< Gpu * >::iterator it = std::find( m_gpuList.begin(),
                                                    m_gpuList.end(), pGpu );
            if ( it != m_gpuList.end() )
            {
                m_gpuList.erase( it );
                delete pGpu;
            }
            else
            {
                ErrPrintf( "trace_3d: deleteGpu: pGpu %p doesn't exist!\n",
                           pGpu );
            }
        }

    const char *GetDeviceIdString()
        {
            // For backwards compatibility
            static set<string> devStrings;
            string s = ::Gpu::DeviceIdToString(m_pHost->getT3dTest()->GetBoundGpuDevice()->DeviceId());
            auto val = devStrings.insert(s);
            return val.first->c_str();
        }

private:

    HostMods * m_pHost;
    UINT32 m_numGpus;
    list< Gpu * > m_gpuList;

};

GpuManagerMods * GpuManagerMods::Create( HostMods * pHost )
{
    GpuManagerMods_impl * pGpuMngr = new GpuManagerMods_impl( pHost );

    // cache the # of gpus on the system
    //
    if ( pHost->getT3dTest()->GetBoundGpuDevice() )
    {
        pGpuMngr->m_numGpus =
            pHost->getT3dTest()->GetBoundGpuDevice()->GetNumSubdevices();
    }
    else
    {
        WarnPrintf( "trace_3d:     %s: GetBoundGpuDevice is null!\n",
                    __FUNCTION__ );
    }

    return pGpuMngr;
}

class Gpu_impl : public GpuMods
{
public:

    friend GpuMods * GpuMods::Create( HostMods * pHost, UINT32 gpuNum );

    explicit Gpu_impl( HostMods * pHost, UINT32 gpuNum )
        : m_pHost( pHost ),
          m_gpuNum( gpuNum )
        {
            m_pTest = pHost->getT3dTest();
            m_pSubdev = m_pTest->GetBoundGpuDevice()->GetSubdevice(gpuNum);
            m_pGpuRes = m_pTest->GetGpuResource();

            m_pGpuPcie = m_pSubdev->GetInterface<Pcie>();
            m_physFbBase = m_pSubdev->GetPhysFbBase();

        }

    virtual ~Gpu_impl() {}

    LwRm* GetLwRmPtr() { return m_pTest->GetLwRmPtr(); }
    // workhorse functions of GPU access
    //

    // priv register access
    //
    UINT08 RegRd08( UINT32 offset ) const
    {
        return RegRd<UINT08>(offset, &LWGpuResource::RegRd08, &MEM_RD08);
    }

    UINT16 RegRd16( UINT32 offset ) const
    {
        return RegRd<UINT16>(offset, &LWGpuResource::RegRd16, &MEM_RD16);
    }

    UINT32 RegRd32( UINT32 offset ) const
    {
        return RegRd<UINT32>(offset, &LWGpuResource::RegRd32, &MEM_RD32);
    }

    void RegWr08( UINT32 offset, UINT08 data )
    {
        RegWr<UINT08>(offset, data, &LWGpuResource::RegWr08, &MEM_WR08);
    }

    void RegWr16( UINT32 offset, UINT16 data )
    {
        RegWr<UINT16>(offset, data, &LWGpuResource::RegWr16, &MEM_WR16);
    }

    void RegWr32( UINT32 offset, UINT32 data )
    {
        RegWr<UINT32>(offset, data, &LWGpuResource::RegWr32, &MEM_WR32);
    }

    // Frame buffer access
    //
    UINT08 FbRd08( UINT32 Offset ) const
        { return Platform::PhysRd08( m_physFbBase + Offset ); }

    UINT16 FbRd16( UINT32 Offset ) const
        { return Platform::PhysRd16( m_physFbBase + Offset ); }

    UINT32 FbRd32( UINT32 Offset ) const
        { return Platform::PhysRd32( m_physFbBase + Offset ); }

    UINT64 FbRd64( UINT32 Offset ) const
        { return Platform::PhysRd64( m_physFbBase + Offset ); }

    void   FbWr08( UINT32 Offset, UINT08 Data )
        { Platform::PhysWr08( m_physFbBase + Offset, Data ); }

    void   FbWr16( UINT32 Offset, UINT16 Data )
        { Platform::PhysWr16( m_physFbBase + Offset, Data ); }

    void   FbWr32( UINT32 Offset, UINT32 Data )
        { Platform::PhysWr32( m_physFbBase + Offset, Data ); }

    void   FbWr64( UINT32 Offset, UINT64 Data )
        { Platform::PhysWr64( m_physFbBase + Offset, Data ); }

    bool regName2Offset( const char * name, int dimensionality, UINT32 * offset,
                         int i, int j, const char *regSpace = nullptr)
    {
        // plugin-side interface-only function, implemented in the shim layer
        //
        return false;
    }

    UINT32 GetSMVersion() const
    {
        LwRm *pLwRm = m_pTest->GetLwRmPtr();

        LW2080_CTRL_GR_INFO info;
        info.index = LW2080_CTRL_GR_INFO_INDEX_SM_VERSION;
        LW2080_CTRL_GR_GET_INFO_PARAMS params = {0};
        params.grInfoListSize = 1;
        params.grInfoList = (LwP64)&info;

        RC rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(m_pSubdev),
            LW2080_CTRL_CMD_GR_GET_INFO,
            &params,
            sizeof(params));

        if (OK != rc)
        {
            ErrPrintf("Gpu::GetSMVersion: couldn't get SM version\n");
            return 0;
        }

        return info.data;
    }

    GpuSubdevice * getSubdev() const
    {
        return m_pSubdev;
    }

    INT32 GetRegNum(const char *regName, UINT32 *value, const char *regSpace = nullptr)
    {
        LwRm *pLwRm = m_pTest->GetLwRmPtr();
        string space;

        bool isPerRunlistReg = strstr(regName, "LW_RUNLIST_") || strstr(regName, "LW_CHRAM_");
        if  (!regSpace && isPerRunlistReg)
        {
            regSpace = "LW_ENGINE_GR0";
        }

        return m_pGpuRes->GetRegNum(regName,
                value, regSpace, m_gpuNum, pLwRm);
    }

    INT32 SetRegFldDef(const char *regName, const char *subfieldName,
        const char *subdefName, UINT32 *myValue, const char *regSpace = nullptr)
    {
        SmcEngine *pSmcEngine = m_pTest->GetSmcEngine();

        return m_pGpuRes->SetRegFldDef(regName,
            subfieldName, subdefName, myValue, regSpace, m_gpuNum, pSmcEngine);
    }

    INT32 SetRegFldNum(const char *regName, const char *subfieldName,
        UINT32 setValue, UINT32 *myValue, const char *regSpace = nullptr)
    {
        SmcEngine *pSmcEngine = m_pTest->GetSmcEngine();

        return m_pGpuRes->SetRegFldNum(regName,
            subfieldName, setValue, myValue, regSpace, m_gpuNum, pSmcEngine);
    }

    INT32 GetRegFldNum(const char *regName, const char *subfieldName,
        UINT32 *theValue, UINT32 *myValue, const char *regSpace = nullptr)
    {
        SmcEngine *pSmcEngine = m_pTest->GetSmcEngine();
        return m_pGpuRes->GetRegFldNum(regName,
            subfieldName, theValue, myValue, regSpace, m_gpuNum, pSmcEngine);
    }

    INT32 GetRegFldDef(const char *regName, const char *subfieldName,
        const char *subdefName, UINT32 *myValue)
    {
        //regSpace and SMC engine info is not needed since no register access.
        return m_pGpuRes->GetRegFldDef(regName,
            subfieldName, subdefName, myValue, m_gpuNum);
    }

    bool TestRegFldDef(const char *regName, const char *subfieldName,
        const char *subdefName, UINT32 *myValue, const char *regSpace = nullptr)
    {
        SmcEngine *pSmcEngine = m_pTest->GetSmcEngine();

        return m_pGpuRes->TestRegFldDef(regName,
            subfieldName, subdefName, myValue, regSpace, m_gpuNum, pSmcEngine);
    }

    bool TestRegFldNum(const char *regName, const char *subfieldName,
        UINT32 testValue, UINT32 *myValue, const char *regSpace = nullptr)
    {
        SmcEngine *pSmcEngine = m_pTest->GetSmcEngine();

        return m_pGpuRes->TestRegFldNum(regName,
            subfieldName, testValue, myValue, regSpace, m_gpuNum, pSmcEngine);
    }

    void SwReset()
    {
        Platform::ProcessPendingInterrupts();
        if (m_pGpuRes->GetGpuDevice()->Reset(LW0080_CTRL_BIF_RESET_FLAGS_TYPE_SW_RESET) != OK)
        {
            ErrPrintf("%s: failed to perform sw reset\n", __FUNCTION__);
        }
    }

    UINT08 PciRd08(UINT32 offset)
    {
        uintptr_t address = (m_pGpuPcie->DomainNumber() << 28) +
            (m_pGpuPcie->BusNumber() << 16) +
            (m_pGpuPcie->DeviceNumber() << 11) +
            (m_pGpuPcie->FunctionNumber() << 8) +
            offset;
        return Sys::CfgRd08(address);
    }

    UINT16 PciRd16(UINT32 offset)
    {
        uintptr_t address = (m_pGpuPcie->DomainNumber() << 28) +
            (m_pGpuPcie->BusNumber() << 16) +
            (m_pGpuPcie->DeviceNumber() << 11) +
            (m_pGpuPcie->FunctionNumber() << 8) +
            offset;
        return Sys::CfgRd16(address);
    }

    UINT32 PciRd32(UINT32 offset)
    {
        uintptr_t address = (m_pGpuPcie->DomainNumber() << 28) +
            (m_pGpuPcie->BusNumber() << 16) +
            (m_pGpuPcie->DeviceNumber() << 11) +
            (m_pGpuPcie->FunctionNumber() << 8) +
            offset;
        return Sys::CfgRd32(address);
    }

    void PciWr08(UINT32 offset, UINT08 data)
    {
        uintptr_t address = (m_pGpuPcie->DomainNumber() << 28) +
            (m_pGpuPcie->BusNumber() << 16) +
            (m_pGpuPcie->DeviceNumber() << 11) +
            (m_pGpuPcie->FunctionNumber() << 8) +
            offset;
        Sys::CfgWr08(address, data);
    }

    void PciWr16(UINT32 offset, UINT16 data)
    {
        uintptr_t address = (m_pGpuPcie->DomainNumber() << 28) +
            (m_pGpuPcie->BusNumber() << 16) +
            (m_pGpuPcie->DeviceNumber() << 11) +
            (m_pGpuPcie->FunctionNumber() << 8) +
            offset;
        Sys::CfgWr16(address, data);
    }

    void PciWr32(UINT32 offset, UINT32 data)
    {
        uintptr_t address = (m_pGpuPcie->DomainNumber() << 28) +
            (m_pGpuPcie->BusNumber() << 16) +
            (m_pGpuPcie->DeviceNumber() << 11) +
            (m_pGpuPcie->FunctionNumber() << 8) +
            offset;
        Sys::CfgWr32(address, data);
    }

    UINT32 GetIrq()
    {
        return m_pSubdev->GetIrq();
    }

    UINT32 GetNumIrqs()
    {
        return m_pSubdev->GetNumIrqs();
    }

    UINT32 GetIrq(UINT32 irqNum)
    {
        return m_pSubdev->GetIrq(irqNum);
    }

    HookedIntrType GetHookedIntrType()
    {
        return (HookedIntrType)m_pSubdev->GetHookedIntrType();
    }

    UINT32 UnlockHost2Jtag
    (
        UINT32 *secCfgMaskArr,
        UINT32 secCfgMaskSize,
        UINT32 *selwreKeysArr,
        UINT32 selwreKeysSize
    )
    {
        vector<UINT32> secCfgMask(secCfgMaskArr, secCfgMaskArr + secCfgMaskSize);
        vector<UINT32> selwreKeys(selwreKeysArr, selwreKeysArr + selwreKeysSize);
        return m_pSubdev->UnlockHost2Jtag(secCfgMask, selwreKeys);
    }

    UINT32 ReadHost2Jtag
    (
        UINT32 *chipletId,
        UINT32 *intstId,
        UINT32 jtagClustersSize,
        UINT32 chainLen,
        UINT32 *pResult,
        UINT32 *pResultSize,
        UINT32 capDis,
        UINT32 updtDis
    )
    {
        vector<GpuSubdevice::JtagCluster> jtagClusters;
        for (UINT32 i = 0; i < jtagClustersSize; ++i)
        {
            GpuSubdevice::JtagCluster temp(*(chipletId + i), *(intstId + i));
            jtagClusters.push_back(temp);
        }

        vector<UINT32> temp;
        UINT32 result = m_pSubdev->ReadHost2Jtag(jtagClusters, chainLen, &temp, capDis, updtDis);

        if(!result)
        {
            if(*pResultSize == 0 || *pResultSize < temp.size())
            {
                return ~0x0;
            }

            *pResultSize = temp.size();
            for(UINT32 i = 0; i < *pResultSize; ++i)
            {
                *(pResult + i) = temp[i];
            }
        }

        return result;
    }

    UINT32 WriteHost2Jtag
    (
        UINT32 *chipletId,
        UINT32 *intstId,
        UINT32 jtagClustersSize,
        UINT32 chainLen,
        UINT32 *inputDataArr,
        UINT32 inputDataSize,
        UINT32 capDis,
        UINT32 updtDis
    )
    {
        vector<GpuSubdevice::JtagCluster> jtagClusters;
        for(UINT32 i = 0; i < jtagClustersSize; ++i)
        {
            GpuSubdevice::JtagCluster temp(*(chipletId + i), *(intstId + i));
            jtagClusters.push_back(temp);
        }
        vector<UINT32> inputData(inputDataSize);
        for(UINT32 i = 0; i < inputDataSize; ++i)
        {
            inputData[i] = *(inputDataArr + i);
        }

        return m_pSubdev->WriteHost2Jtag(jtagClusters, chainLen, inputData, capDis, updtDis);
    }

    INT32 LoadPMUFirmware(const string &firmwareFile, bool resetOnLoad, bool waitInit)
    {
        MASSERT(!"Gpu_LoadPMUFirmware is deprecated! Please don't call this IF.\n");
        return 0;
    }

    UINT32 GetDeviceId()
    {
        return m_pSubdev->DeviceId();
    }

    const char *GetDeviceIdString()
    {
        // For backwards compatibility
        static set<string> devStrings;
        string s = ::Gpu::DeviceIdToString(m_pSubdev->DeviceId());
        auto val = devStrings.insert(s);
        return val.first->c_str();
    }

    INT32 SetPcieLinkSpeed(UINT32 newSpeed)
    {
        return m_pGpuPcie->SetLinkSpeed((Pci::PcieLinkSpeed)newSpeed);
    }

    INT32 GetPowerFeaturesSupported(UINT32 *powerFeature)
    {
        LwRmPtr pLwRm;
        LW2080_CTRL_POWER_FEATURES_SUPPORTED_PARAMS params = {0};

        *powerFeature = 0;

        RC rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(m_pSubdev),
            LW2080_CTRL_CMD_POWER_FEATURES_SUPPORTED,
            &params,
            sizeof(params));

        if (OK != rc)
        {
            ErrPrintf("%s: couldn't get power feature\n", __FUNCTION__);
            return -1;
        }

        *powerFeature = params.powerFeaturesSupported;
        return 0;
    }

    bool SetDiGpuReady()
    {
        Trace3DTest *pT3dTest = m_pHost->getT3dTest();
        RC rc = pT3dTest->SetDiGpuReady(m_pSubdev);

        if (OK != rc)
        {
            ErrPrintf("%s: set di gpu ready failed\n", __FUNCTION__);
            return false;
        }

        return true;
    }

    bool SetEnableCEPrefetch(bool bEnable)
    {
        Trace3DTest *pT3dTest = m_pHost->getT3dTest();
        RC rc = pT3dTest->SetEnableCEPrefetch(m_pSubdev, bEnable);

        if (OK != rc)
        {
            ErrPrintf("Enable/Disable CE prefetch failed\n");
            return false;
        }

        return true;
    }

    INT32 SetGpuPowerOnOff
    (
        UINT32 action,
        bool bUseHwTimer,
        UINT32 timeToWakeUs,
        bool bIsRTD3Transition,
        bool bIsD3Hot
    )
    {
        Trace3DTest *pT3dTest = m_pHost->getT3dTest();
        RC rc = pT3dTest->SetGpuPowerOnOff(m_pSubdev, action,
                                           bUseHwTimer, timeToWakeUs,
                                           bIsRTD3Transition, bIsD3Hot);

        if (OK != rc)
        {
            ErrPrintf("%s: gpu power action failed\n", __FUNCTION__);
            return -1;
        }

        return 0;
    }

    INT32 SetGpuPowerOnOff
    (
        UINT32 action,
        bool bUseHwTimer,
        UINT32 timeToWakeUs,
        bool bIsRTD3Transition
    )
    {
        return SetGpuPowerOnOff(action, bUseHwTimer, timeToWakeUs, bIsRTD3Transition, false);
    }

    INT32 SetGpuPowerOnOff
    (
        UINT32 action,
        bool bUseHwTimer,
        UINT32 timeToWakeUs
    )
    {
        return SetGpuPowerOnOff(action, bUseHwTimer, timeToWakeUs, false, false);        
    }

    INT32 GetGCxWakeupReason(UINT32 selectPowerState,
                             GCxWakeupReasonParams *pWakeupReasonParams)
    {
        // Parameter check
        if (!pWakeupReasonParams ||
            pWakeupReasonParams->TypeID >= GCxWakeupReasonParams::ParamTypeLast)
        {
            ErrPrintf("%s: Error - (pParams==NULL)  or (Unsupported ParamType)\n",
                __FUNCTION__);
            return -1;
        }

        // Get the wakeup reason from RM
        LwRmPtr pLwRm;
        LW2080_CTRL_GCX_GET_WAKEUP_REASON_PARAMS params = {0};
        params.selectPowerState = selectPowerState;
        RC rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(m_pSubdev),
            LW2080_CTRL_CMD_GCX_GET_WAKEUP_REASON,
            &params,
            sizeof(params));

        if (OK != rc)
        {
            ErrPrintf("%s: couldn't get wakeup reasons.\n", __FUNCTION__);
            return -1;
        }

        // Re-organize the parameters into plugin layout in agreement
        if (pWakeupReasonParams->TypeID >= GCxWakeupReasonParams::ParamType0)
        {
            pWakeupReasonParams->Data0.selectPowerState = params.selectPowerState;
            pWakeupReasonParams->Data0.statId = params.statId;
            pWakeupReasonParams->Data0.gc5ExitType = params.gc5ExitType;
            pWakeupReasonParams->Data0.deepL1Type = params.deepL1Type;
            pWakeupReasonParams->Data0.gc5AbortCode = params.gc5AbortCode;
            pWakeupReasonParams->Data0.sciIntr0 = params.sciIntr0;
            pWakeupReasonParams->Data0.sciIntr1 = params.sciIntr1;
            pWakeupReasonParams->Data0.pmcIntr0 = params.pmcIntr0;
            pWakeupReasonParams->Data0.pmcIntr1 = params.pmcIntr1;
        }

        // Example about how to append a new type
        // if (pWakeupReasonParams->TypeID >= GCxWakeupReasonParams::ParamType1)
        // {
        //     pWakeupReasonParams->Data1.NewMember = xxx;
        // }

        return 0;
    }

    INT32 GetGC6PlusIsIslandLoaded(GC6PlusIsIslandLoadedParams *pIsIslandLoadedParams)
    {
        // Get the wakeup reason from RM
        LwRmPtr pLwRm;
        LW2080_CTRL_GC6PLUS_IS_ISLAND_LOADED_PARAMS params = {0};
        RC rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(m_pSubdev),
            LW2080_CTRL_CMD_GC6PLUS_IS_ISLAND_LOADED,
            &params,
            sizeof(params));

        if (OK != rc)
        {
            ErrPrintf("%s: couldn't get isislandloaded.\n", __FUNCTION__);
            return -1;
        }

        // Re-organize the parameters into plugin layout in agreement
        pIsIslandLoadedParams->bIsIslandLoaded = params.bIsIslandLoaded;

        return 0;
    }

    UINT32 GobWidth()
    {
        GpuDevice *dev = m_pSubdev->GetParentDevice();
        return dev->GobWidth();
    }

    UINT32 GobHeight()
    {
        GpuDevice *dev = m_pSubdev->GetParentDevice();
        return dev->GobHeight();
    }

    UINT32 GobDepth()
    {
        GpuDevice *dev = m_pSubdev->GetParentDevice();
        return dev->GobDepth();
    }

    UINT32 GetGpcCount()
    {
        SmcEngine *pSmcEng = m_pTest->GetSmcEngine();
        if (pSmcEng)
        {
            LwRm *pLwRm = m_pTest->GetLwRmPtr();
            LW2080_CTRL_GR_GET_GPC_MASK_PARAMS params = { };

            RC rc = pLwRm->ControlBySubdevice(m_pSubdev, LW2080_CTRL_CMD_GR_GET_GPC_MASK, &params, sizeof(params));
            if (rc != OK)
            {
                Printf(Tee::PriError,"RM Ctrl call LW2080_CTRL_CMD_GR_GET_GPC_MASK error: %s\n", rc.Message());
                MASSERT(0);
            }
            return Utility::CountBits(params.gpcMask);
        }
        return m_pSubdev->GetGpcCount();
    }

    UINT32 GetTpcCountOnGpc(UINT32 virtualGpcNum)
    {
        SmcEngine *pSmcEng = m_pTest->GetSmcEngine();
        if (pSmcEng)
        {
            LwRm *pLwRm = m_pTest->GetLwRmPtr();
            LW2080_CTRL_GR_GET_TPC_MASK_PARAMS params = { };
            params.gpcId  = virtualGpcNum;

            RC rc = pLwRm->ControlBySubdevice(m_pSubdev, LW2080_CTRL_CMD_GR_GET_TPC_MASK, &params, sizeof(params));
            if (rc != OK)
            {
                Printf(Tee::PriError,"RM Ctrl call LW2080_CTRL_CMD_GR_GET_TPC_MASK error: %s\n", rc.Message());
                MASSERT(0);
            }
            return Utility::CountBits(params.tpcMask);
        }

        return m_pSubdev->GetTpcCountOnGpc(virtualGpcNum);
    }

    bool EnablePrivProtectedRegisters(bool bEnable)
    {
        return m_pSubdev->EnablePrivProtectedRegisters(bEnable);
    }

    bool EnablePowerGatingTestDuringRegAccess(bool bEnable)
    {
        return m_pSubdev->EnablePowerGatingTestDuringRegAccess(bEnable);
    }

    UINT32 GetLwlinkPowerState(UINT32 linkId) override
    {
        LwRmPtr pLwRm;

        LW2080_CTRL_LWLINK_GET_POWER_STATE_PARAMS params = {0};
        params.linkId = linkId;

        RC rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(m_pSubdev),
            LW2080_CTRL_CMD_LWLINK_GET_POWER_STATE,
            &params,
            sizeof(params));

        if (OK != rc)
        {
            ErrPrintf("Gpu::GetLwlinkPowerState: "
                "Couldn't get Lwlink power state!\n");
            return ~0;
        }

        return params.powerState;
    }

    UINT32 SetLwlinkPowerState(UINT32 linkMask, UINT32 powerState) override
    {
        LwRmPtr pLwRm;

        LW2080_CTRL_LWLINK_SET_POWER_STATE_PARAMS params = {0};
        params.linkMask = linkMask;
        params.powerState = powerState;

        RC rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(m_pSubdev),
            LW2080_CTRL_CMD_LWLINK_SET_POWER_STATE,
            &params,
            sizeof(params));

        if (OK != rc)
        {
            ErrPrintf("Gpu::SetLwlinkPowerState: "
                "Couldn't set Lwlink power state!\n");
            return ~0;
        }

        return 0;
    }

    UINT32 SetPrePostPowerOnOff(UINT32 action) override
    {
        LwRmPtr pLwRm;

        LW2080_CTRL_GPU_POWER_PRE_POST_POWER_ON_OFF_PARAMS params = {0};
        params.action = action;

        RC rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(m_pSubdev),
            LW2080_CTRL_CMD_GPU_POWER_PRE_POST_POWER_ON_OFF,
            &params,
            sizeof(params));

        if (OK != rc)
        {
            ErrPrintf("Gpu::SetPrePostPowerOnOff: "
                "Couldn't set pre power off or post power on!\n");
            return ~0;
        }

        return 0;
    }

    bool GetLwlinkConnected(UINT32 linkId)
    {
        LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS_PARAMS statusParams = {};
        RC rc = GetLwLinkStatus(&statusParams, "Gpu::GetLwlinkConnected");
        if(OK != rc)
        {
            return false;
        }
        return statusParams.linkInfo[linkId].connected;
    }

    UINT32 GetLwlinkLoopProp(UINT32 linkId, UINT08* loopProp)
    {
        MASSERT(loopProp);
        LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS_PARAMS statusParams = {};
        RC rc = GetLwLinkStatus(&statusParams, "Gpu::GetLwlinkLoopProp");
        if(OK == rc)
        {
            *loopProp = statusParams.linkInfo[linkId].loopProperty;
        }
        return rc;
    }

    UINT32 GetLwlinkRemoteLinkNum(UINT32 linkId, UINT08* linkNum)
    {
        MASSERT(linkNum);
        LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS_PARAMS statusParams = {};
        RC rc = GetLwLinkStatus(&statusParams, "Gpu::GetLwlinkRemoteLinkNum");
        if(OK == rc)
        {
            *linkNum = statusParams.linkInfo[linkId].remoteDeviceLinkNumber;
        }
        return rc;
    }

    UINT32 GetLwlinkLocalLinkNum(UINT32 linkId, UINT08* linkNum)
    {
        MASSERT(linkNum);
        LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS_PARAMS statusParams = {};
        RC rc = GetLwLinkStatus(&statusParams, "Gpu::GetLwlinkLocalLinkNum");
        if(OK == rc)
        {
            *linkNum = statusParams.linkInfo[linkId].localDeviceLinkNumber;
        }
        return rc;
    }

    UINT32 GetLwlinkDiscoveredMask(UINT32* mask)
    {
        MASSERT(mask);
        LW2080_CTRL_CMD_LWLINK_GET_LWLINK_CAPS_PARAMS capsParams = {};

        LwRmPtr pLwRm;
        RC rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(m_pSubdev),
                               LW2080_CTRL_CMD_LWLINK_GET_LWLINK_CAPS,
                               &capsParams,
                               sizeof(capsParams));
        if(OK == rc)
        {
            *mask = capsParams.discoveredLinkMask;
        }
        else
        {
            ErrPrintf("Gpu::GetLwlinkDiscoveredMask:"
                "Couldn't get connection status!\n");
        }
        return rc;
    }

    UINT32 GetLwlinkEnabledMask(UINT32* mask)
    {
        MASSERT(mask);
        LW2080_CTRL_CMD_LWLINK_GET_LWLINK_CAPS_PARAMS capsParams = {};

        LwRmPtr pLwRm;
        RC rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(m_pSubdev),
                               LW2080_CTRL_CMD_LWLINK_GET_LWLINK_CAPS,
                               &capsParams,
                               sizeof(capsParams));
        if(OK == rc)
        {
            *mask = capsParams.enabledLinkMask;
        }
        else
        {
            ErrPrintf("Gpu::GetLwlinkEnabledMask:"
                "Couldn't get connection status!\n");
        }
        return rc;
    }

    // On return pLinkMask=mask of available links on this subdevice
    // the return value is 0 on success
    UINT32 GetLwlinkConnectedMask(UINT32 *pLinkMask)
    {
        MASSERT(pLinkMask);
        LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS_PARAMS statusParams = {};
        RC rc = GetLwLinkStatus(&statusParams, "Gpu::GetLwlinkConnectedMask");
        if(OK != rc)
        {
            return ~0;
        }
        *pLinkMask = statusParams.enabledLinkMask;
        return rc;
    }

    UINT32 GetLwlinkFomVals(UINT32 linkId, UINT16** fom, UINT08* numLanes)
    {
        MASSERT(fom);
        MASSERT(numLanes);
        LW2080_CTRL_CMD_LWLINK_GET_LINK_FOM_VALUES_PARAMS fomParams = {};
        fomParams.linkId = linkId;
        LwRmPtr pLwRm;
        RC rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(m_pSubdev),
                               LW2080_CTRL_CMD_LWLINK_GET_LINK_FOM_VALUES,
                               &fomParams,
                               sizeof(fomParams));
        if(OK == rc)
        {
            if (!m_pFomVals)
            {
                m_pFomVals = make_unique<UINT16[]>(LW2080_CTRL_LWLINK_MAX_LANES);
            }
            memcpy(m_pFomVals.get(), fomParams.figureOfMeritValues,
                   sizeof(UINT16)*LW2080_CTRL_LWLINK_MAX_LANES);
            *numLanes = fomParams.numLanes;
            *fom = m_pFomVals.get();
        }
        else
        {
            ErrPrintf("Gpu::GetLwlinkFomVals:"
                "Couldn't get connection status!\n");
        }
        return rc;
    }

    UINT32 GetLwlinkLocalSidVals(UINT32 linkId, UINT64* sid)
    {
        MASSERT(sid);
        LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS_PARAMS statusParams = {};
        RC rc = GetLwLinkStatus(&statusParams, "Gpu::GetLwlinkLocalSidVals");
        if(OK == rc)
        {
            *sid = statusParams.linkInfo[linkId].localLinkSid;
        }
        return rc;
    }

    UINT32 GetLwlinkRemoteSidVals(UINT32 linkId, UINT64* sid)
    {
        MASSERT(sid);
        LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS_PARAMS statusParams = {};
        RC rc = GetLwLinkStatus(&statusParams, "Gpu::GetLwlinkRemoteSidVals");
        if(OK == rc)
        {
            *sid = statusParams.linkInfo[linkId].remoteLinkSid;
        }
        return rc;
    }

    UINT32 GetLwlinkLineRateMbps(UINT32 linkId, UINT32* rate)
    {
        MASSERT(rate);
        LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS_PARAMS statusParams = {};
        RC rc = GetLwLinkStatus(&statusParams, "Gpu::GetLwlinkLineRateMbps");
        if(OK == rc)
        {
            *rate = statusParams.linkInfo[linkId].lwlinkLineRateMbps;
        }
        return rc;
    }

    UINT32 GetLwlinkLinkClockMHz(UINT32 linkId, UINT32* clock)
    {
        MASSERT(clock);
        LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS_PARAMS statusParams = {};
        RC rc = GetLwLinkStatus(&statusParams, "Gpu::GetLwlinkLinkClockMHz");
        if(OK == rc)
        {
            *clock = statusParams.linkInfo[linkId].lwlinkLinkClockMhz;
        }
        return rc;
    }

    UINT32 GetLwlinkLinkClockType(UINT32 linkId, UINT32* clktype)
    {
        MASSERT(clktype);
        LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS_PARAMS statusParams = {};
        RC rc = GetLwLinkStatus(&statusParams, "Gpu::GetLwlinkLinkClockType");
        if(OK == rc)
        {
            *clktype = statusParams.linkInfo[linkId].lwlinkRefClkType;
        }
        return rc;
    }

    UINT32 GetLwlinkDataRateKiBps(UINT32 linkId, UINT32* rate)
    {
        MASSERT(rate);
        LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS_PARAMS statusParams = {};
        RC rc = GetLwLinkStatus(&statusParams, "Gpu::GetLwlinkDataRateKiBps");
        if(OK == rc)
        {
            *rate = statusParams.linkInfo[linkId].lwlinkLinkDataRateKiBps;
        }
        return rc;
    }

    pair<UINT32, bool> GetEngineResetId(const char* engine, UINT32 hwInstance) const
    {
        auto hwType = m_pSubdev->StringToHwDevType(engine);

        for (const auto& hwDevInfo : m_pSubdev->GetHwDevInfo())
        {
            if ((hwDevInfo.hwType == hwType) && (hwDevInfo.hwInstance == hwInstance))
            {
                return make_pair(hwDevInfo.resetId, hwDevInfo.bResetIdValid);
            }
        }

        return make_pair(numeric_limits<UINT32>::max(), false);
    }

    UINT32 GetLwlinkErrInfo(LWLINK_ERR_INFO** info, UINT08* numInfo)
    {
        MASSERT(info);
        LW2080_CTRL_LWLINK_GET_ERR_INFO_PARAMS errParams = {};
        LwRm* pLwRm = GetLwRmPtr();
        RC rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(m_pSubdev),
                               LW2080_CTRL_CMD_LWLINK_GET_ERR_INFO,
                               &errParams,
                               sizeof(errParams));
        if (OK == rc)
        {
            if (!m_pErrInfo)
            {
                m_pErrInfo = make_unique<LWLINK_ERR_INFO[]>(LW2080_CTRL_LWLINK_MAX_LINKS);
            }
            for (UINT32 i = 0; i < LW2080_CTRL_LWLINK_MAX_LINKS; ++i)
            {
                m_pErrInfo.get()[i].TLErrlog = errParams.linkErrInfo[i].TLErrlog;
                m_pErrInfo.get()[i].TLIntrEn = errParams.linkErrInfo[i].TLIntrEn;
                m_pErrInfo.get()[i].TLCTxErrStatus0 = errParams.linkErrInfo[i].TLCTxErrStatus0;
                m_pErrInfo.get()[i].TLCTxErrStatus1 = errParams.linkErrInfo[i].TLCTxErrStatus1;
                m_pErrInfo.get()[i].TLCTxSysErrStatus0 = errParams.linkErrInfo[i].TLCTxSysErrStatus0;
                m_pErrInfo.get()[i].TLCRxErrStatus0 = errParams.linkErrInfo[i].TLCRxErrStatus0;
                m_pErrInfo.get()[i].TLCRxErrStatus1 = errParams.linkErrInfo[i].TLCRxErrStatus1;
                m_pErrInfo.get()[i].TLCRxSysErrStatus0 = errParams.linkErrInfo[i].TLCRxSysErrStatus0;
                m_pErrInfo.get()[i].TLCTxErrLogEn0 = errParams.linkErrInfo[i].TLCTxErrLogEn0;
                m_pErrInfo.get()[i].TLCTxErrLogEn1 = errParams.linkErrInfo[i].TLCTxErrLogEn1;
                m_pErrInfo.get()[i].TLCTxSysErrLogEn0 = errParams.linkErrInfo[i].TLCTxSysErrLogEn0;
                m_pErrInfo.get()[i].TLCRxErrLogEn0 = errParams.linkErrInfo[i].TLCRxErrLogEn0;
                m_pErrInfo.get()[i].TLCRxErrLogEn1 = errParams.linkErrInfo[i].TLCRxErrLogEn1;
                m_pErrInfo.get()[i].TLCRxSysErrLogEn0 = errParams.linkErrInfo[i].TLCRxSysErrLogEn0;
                m_pErrInfo.get()[i].MIFTxErrStatus0 = errParams.linkErrInfo[i].MIFTxErrStatus0;
                m_pErrInfo.get()[i].MIFRxErrStatus0 = errParams.linkErrInfo[i].MIFRxErrStatus0;
                m_pErrInfo.get()[i].LWLIPTLnkErrStatus0 = errParams.linkErrInfo[i].LWLIPTLnkErrStatus0;
                m_pErrInfo.get()[i].LWLIPTLnkErrLogEn0 = errParams.linkErrInfo[i].LWLIPTLnkErrLogEn0;
                m_pErrInfo.get()[i].DLSpeedStatusTx = errParams.linkErrInfo[i].DLSpeedStatusTx;
                m_pErrInfo.get()[i].DLSpeedStatusRx = errParams.linkErrInfo[i].DLSpeedStatusRx;
                m_pErrInfo.get()[i].bExcessErrorDL = errParams.linkErrInfo[i].bExcessErrorDL;
            }
            *numInfo = LW2080_CTRL_LWLINK_MAX_LINKS;
            *info = m_pErrInfo.get();
        }
        else
        {
            ErrPrintf("Gpu::GetLwlinkErrInfo:"
                "Couldn't get connection status!\n");
        }
        return rc;
    }

    UINT32 EnterSlm(UINT32 linkId, bool tx)
    {
        auto powerState = m_pSubdev->GetInterface<LwLinkPowerState>();
        return powerState->RequestPowerState(linkId,
                                              LwLinkPowerState::SLS_PWRM_LP,
                                              false);
    }

    UINT32 ExitSlm(UINT32 linkId, bool tx)
    {
        auto powerState = m_pSubdev->GetInterface<LwLinkPowerState>();
        return powerState->RequestPowerState(linkId,
                                              LwLinkPowerState::SLS_PWRM_FB,
                                              false);
    }


private:
    // This RAII class will take care of Moving and Restoring the PGRAPH window
    // for SMC mode. MODS should avoid to move the PGRAPH window by itself since
    // it will lead to a mismatch in RM and MODS PGRAPH window state and can lead
    // to hard to debug bugs. In the particular case that it is being used for,
    // MODS is in ISR (hence will not yield) and will restore the window before
    // control is passed back to RM
    class MovePgraphWindow
    {
        public:
            MovePgraphWindow(const Gpu_impl* gpu, UINT32 sysPipe) :
            m_gpu(gpu)
            {
                GpuSubdevice * pGpusubdevice = m_gpu->m_pGpuRes->GetGpuSubdevice(m_gpu->m_gpuNum);
                RegHal& regHal = pGpusubdevice->Regs();
                m_oldPgraphWindow = regHal.Read32(MODS_PPRIV_SYS_BAR0_TO_PRI_WINDOW);
                UINT32 newPgraphWindow = m_oldPgraphWindow;

                regHal.SetField(&newPgraphWindow, MODS_PPRIV_SYS_BAR0_TO_PRI_WINDOW_INDEX, sysPipe);
                regHal.SetField(&newPgraphWindow, MODS_PPRIV_SYS_BAR0_TO_PRI_WINDOW_ENABLE_ENABLE);
                DebugPrintf("Move Pgraph Window: OldPgraphWinow:0x%08x NewPgraphWindow:0x%08x SysPipe:%d\n",
                            m_oldPgraphWindow, newPgraphWindow, sysPipe);
                regHal.Write32(MODS_PPRIV_SYS_BAR0_TO_PRI_WINDOW, newPgraphWindow);
            }
            ~MovePgraphWindow()
            {
                DebugPrintf("Restore PgraphWindow value to 0x%08x\n", m_oldPgraphWindow);
                GpuSubdevice * pGpusubdevice = m_gpu->m_pGpuRes->GetGpuSubdevice(m_gpu->m_gpuNum);
                RegHal& regHal = pGpusubdevice->Regs();
                regHal.Write32(MODS_PPRIV_SYS_BAR0_TO_PRI_WINDOW, m_oldPgraphWindow);
            }
        private:
            const Gpu_impl* m_gpu;
            UINT32 m_oldPgraphWindow;
    };
    template<typename T>
    struct RegOpFunc
    {
        using RegRdFunc = T (LWGpuResource::*)(UINT32, UINT32, SmcEngine*) const;
        using RegWrFunc = void (LWGpuResource::*)(UINT32, T, UINT32, SmcEngine*);
        using MemRdFunc = T (*)(uintptr_t);
        using MemWrFunc = void (*)(uintptr_t, T);
    };

    template<typename T>
    T RegRd(UINT32 offset, typename RegOpFunc<T>::RegRdFunc rdReg, typename RegOpFunc<T>::MemRdFunc rdMem) const
    {
        T value = ~0;

        if (Tasker::CanThreadYield())
        {
            value = (m_pGpuRes->*rdReg)(offset, m_gpuNum , m_pTest->GetSmcEngine());
        }
        else
        {
            //Disable power gating checking when rd/wr pg registers at ISR.
            DebugPrintf(MSGID(), "Possibly get incorrect value for reg 0x%x if it is power gated!\n", offset);
            bool bCheckPowerGating = m_pGpuRes->GetGpuSubdevice(m_gpuNum)->EnablePowerGatingTestDuringRegAccess(false);
            if (m_pGpuRes->IsPgraphReg(offset) && m_pGpuRes->IsSMCEnabled())
            {
                MASSERT(m_pTest->GetSmcEngine());
                UINT32 sysPipe = m_pTest->GetSmcEngine()->GetSysPipe();
                MovePgraphWindow mvPgraphWindow(this, sysPipe);
                value = (*rdMem)((uintptr_t)((char*)m_pGpuRes->GetGpuSubdevice(m_gpuNum)->GetLwBase() + offset));
            }
            else
            {
                value = (m_pGpuRes->*rdReg)(offset, m_gpuNum , m_pTest->GetSmcEngine());
            }
            m_pGpuRes->GetGpuSubdevice(m_gpuNum)->EnablePowerGatingTestDuringRegAccess(bCheckPowerGating);
        }

        return value;
    }

    template<typename T>
    void RegWr(UINT32 offset, T data, typename RegOpFunc<T>::RegWrFunc wrReg, typename RegOpFunc<T>::MemWrFunc wrMem)
    {
        if (Tasker::CanThreadYield())
        {
            (m_pGpuRes->*wrReg)(offset, data, m_gpuNum , m_pTest->GetSmcEngine());
        }
        else
        {
            //Disable power gating checking when rd/wr pg registers at ISR.
            DebugPrintf(MSGID(), "Possibly cannot write reg 0x%x successfully if it is power gated!\n", offset);
            bool bCheckPowerGating =m_pGpuRes->GetGpuSubdevice(m_gpuNum)->EnablePowerGatingTestDuringRegAccess(false);
            if (m_pGpuRes->IsPgraphReg(offset) && m_pGpuRes->IsSMCEnabled())
            {
                MASSERT(m_pTest->GetSmcEngine());
                UINT32 sysPipe = m_pTest->GetSmcEngine()->GetSysPipe();
                MovePgraphWindow mvPgraphWindow(this, sysPipe);
                (*wrMem)((uintptr_t)((char*)m_pGpuRes->GetGpuSubdevice(m_gpuNum)->GetLwBase() + offset), data);
            }
            else
            {
                (m_pGpuRes->*wrReg)(offset, data, m_gpuNum , m_pTest->GetSmcEngine());
            }
            m_pGpuRes->GetGpuSubdevice(m_gpuNum)->EnablePowerGatingTestDuringRegAccess(bCheckPowerGating);
        }
    }

    RC GetLwLinkStatus(LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS_PARAMS* statusParams,
        std::string funcName = "GetLwLinkStatus")
    {
        LwRmPtr pLwRm;
        RC rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(m_pSubdev),
                               LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS,
                               statusParams,
                               sizeof(*statusParams));
        if (OK != rc)
        {
            funcName += ": Couldn't get connection status!\n";
            ErrPrintf(funcName.c_str());
        }
        return rc;
    }

    HostMods * m_pHost;
    Trace3DTest * m_pTest;
    LWGpuResource * m_pGpuRes;
    GpuSubdevice * m_pSubdev;
    UINT32 m_gpuNum;
    Pcie *m_pGpuPcie;
    UINT64 m_physFbBase;
    unique_ptr<LWLINK_ERR_INFO[]> m_pErrInfo;
    unique_ptr<UINT16[]> m_pFomVals;

};

GpuMods * GpuMods::Create( HostMods * pHost, UINT32 gpuNum )
{
    Gpu_impl * pGpu = new Gpu_impl( pHost, gpuNum );
    return pGpu;
}

extern bool IsSurface(const MdiagSurf * pS2d); // see plgnsprt.cpp

class BufferManagerMods_impl : public BufferManagerMods
{
public:

    friend BufferManagerMods * BufferManagerMods::Create( HostMods * pHost );

    explicit BufferManagerMods_impl( HostMods * pHost )
        : m_pHost( pHost )
        {
            m_fileTypeMap[ GpuTrace::FT_VERTEX_BUFFER ] = "FT_VERTEX_BUFFER";
            m_fileTypeMap[ GpuTrace::FT_INDEX_BUFFER ] = "FT_INDEX_BUFFER";
            m_fileTypeMap[ GpuTrace::FT_TEXTURE ] = "FT_TEXTURE";
            m_fileTypeMap[ GpuTrace::FT_PUSHBUFFER ] = "FT_PUSHBUFFER";
            m_fileTypeMap[ GpuTrace::FT_SHADER_PROGRAM ] = "FT_SHADER_PROGRAM";
            m_fileTypeMap[ GpuTrace::FT_CONSTANT_BUFFER ] =
                "FT_CONSTANT_BUFFER";
            m_fileTypeMap[ GpuTrace::FT_TEXTURE_HEADER ] = "FT_TEXTURE_HEADER";
            m_fileTypeMap[ GpuTrace::FT_TEXTURE_SAMPLER] = "FT_TEXTURE_SAMPLER";
            m_fileTypeMap[ GpuTrace::FT_SHADER_THREAD_MEMORY ] =
                "FT_SHADER_THREAD_MEMORY";
            m_fileTypeMap[ GpuTrace::FT_SHADER_THREAD_STACK ] =
                "FT_SHADER_THREAD_STACK";
            m_fileTypeMap[ GpuTrace::FT_SEMAPHORE ] = "FT_SEMAPHORE";
            m_fileTypeMap[ GpuTrace::FT_SEMAPHORE_16 ] = "FT_SEMAPHORE_16";
            m_fileTypeMap[ GpuTrace::FT_NOTIFIER ] = "FT_NOTIFIER";
            m_fileTypeMap[ GpuTrace::FT_STREAM_OUTPUT ] = "FT_STREAM_OUTPUT";
            m_fileTypeMap[ GpuTrace::FT_SELFGILD ] = "FT_SELFGILD";
            m_fileTypeMap[ GpuTrace::FT_VP2_0 ] = "FT_VP2_0";
            m_fileTypeMap[ GpuTrace::FT_VP2_1 ] = "FT_VP2_1";
            m_fileTypeMap[ GpuTrace::FT_VP2_2 ] = "FT_VP2_2";
            m_fileTypeMap[ GpuTrace::FT_VP2_3 ] = "FT_VP2_3";
            m_fileTypeMap[ GpuTrace::FT_VP2_4 ] = "FT_VP2_4";
            m_fileTypeMap[ GpuTrace::FT_VP2_5 ] = "FT_VP2_5";
            m_fileTypeMap[ GpuTrace::FT_VP2_6 ] = "FT_VP2_6";
            m_fileTypeMap[ GpuTrace::FT_VP2_7 ] = "FT_VP2_7";
            m_fileTypeMap[ GpuTrace::FT_VP2_8 ] = "FT_VP2_8";
            m_fileTypeMap[ GpuTrace::FT_VP2_9 ] = "FT_VP2_9";
            m_fileTypeMap[ GpuTrace::FT_VP2_10 ] = "FT_VP2_10";
            m_fileTypeMap[ GpuTrace::FT_VP2_14 ] = "FT_VP2_14";
            m_fileTypeMap[ GpuTrace::FT_CIPHER_A ] = "FT_CIPHER_A";
            m_fileTypeMap[ GpuTrace::FT_CIPHER_B ] = "FT_CIPHER_B";
            m_fileTypeMap[ GpuTrace::FT_CIPHER_C ] = "FT_CIPHER_C";
            m_fileTypeMap[ GpuTrace::FT_GMEM_A ] = "FT_GMEM_A";
            m_fileTypeMap[ GpuTrace::FT_GMEM_B ] = "FT_GMEM_B";
            m_fileTypeMap[ GpuTrace::FT_GMEM_C ] = "FT_GMEM_C";
            m_fileTypeMap[ GpuTrace::FT_GMEM_D ] = "FT_GMEM_D";
            m_fileTypeMap[ GpuTrace::FT_GMEM_E ] = "FT_GMEM_E";
            m_fileTypeMap[ GpuTrace::FT_GMEM_F ] = "FT_GMEM_F";
            m_fileTypeMap[ GpuTrace::FT_GMEM_G ] = "FT_GMEM_G";
            m_fileTypeMap[ GpuTrace::FT_GMEM_H ] = "FT_GMEM_H";
            m_fileTypeMap[ GpuTrace::FT_GMEM_I ] = "FT_GMEM_I";
            m_fileTypeMap[ GpuTrace::FT_GMEM_J ] = "FT_GMEM_J";
            m_fileTypeMap[ GpuTrace::FT_GMEM_K ] = "FT_GMEM_K";
            m_fileTypeMap[ GpuTrace::FT_GMEM_L ] = "FT_GMEM_L";
            m_fileTypeMap[ GpuTrace::FT_GMEM_M ] = "FT_GMEM_M";
            m_fileTypeMap[ GpuTrace::FT_GMEM_N ] = "FT_GMEM_N";
            m_fileTypeMap[ GpuTrace::FT_GMEM_O ] = "FT_GMEM_O";
            m_fileTypeMap[ GpuTrace::FT_GMEM_P ] = "FT_GMEM_P";
            m_fileTypeMap[ GpuTrace::FT_LOD_STAT ] = "FT_LOD_STAT";
            m_fileTypeMap[ GpuTrace::FT_ZLWLL_RAM ] = "FT_ZLWLL_RAM";
            m_fileTypeMap[ GpuTrace::FT_PMU_0 ] = "FT_PMU_0";
            m_fileTypeMap[ GpuTrace::FT_PMU_1 ] = "FT_PMU_1";
            m_fileTypeMap[ GpuTrace::FT_PMU_2 ] = "FT_PMU_2";
            m_fileTypeMap[ GpuTrace::FT_PMU_3 ] = "FT_PMU_3";
            m_fileTypeMap[ GpuTrace::FT_PMU_4 ] = "FT_PMU_4";
            m_fileTypeMap[ GpuTrace::FT_PMU_5 ] = "FT_PMU_5";
            m_fileTypeMap[ GpuTrace::FT_PMU_6 ] = "FT_PMU_6";
            m_fileTypeMap[ GpuTrace::FT_PMU_7 ] = "FT_PMU_7";
            m_fileTypeMap[ GpuTrace::FT_ALLOC_SURFACE ] = "FT_ALLOC_SURFACE";
        }
    virtual ~BufferManagerMods_impl()
        {
            deleteTraceBufferList();

            list< Buffer * >::iterator it = m_createdBufferList.begin();
            list< Buffer * >::iterator it_end = m_createdBufferList.end();
            for ( ; it != it_end; ++it )
            {
                DebugPrintf(MSGID(), "trace_3d: ~BufferManagerMods_impl: deleting pBuff"
                             "er %p\n", *it );
                delete *it;
            }
            for ( list< MdiagSurf* >::iterator surfIt = m_createdSurfaceList.begin();
                  surfIt != m_createdSurfaceList.end(); ++surfIt )
            {
                DebugPrintf(MSGID(), "trace_3d: ~BufferManagerMods_impl: deleting pSurface"
                             " %p\n", *surfIt );
                delete *surfIt;
                *surfIt = NULL;
            }

        }

    // creates a new  TraceModule object, adds it to the GpuTrace's list of
    // TraceModules marks the TraceModule object as having plugin-specified
    // initial data, so that we can copy the plugin initial data into the
    // TraceModule at the right time in the trace_3d flow.  This "mark" is
    // done by storing a pointer to the Buffer object as an opaque handle in
    // the TraceModule object.   A non-null value of this pointer indicates
    // the TraceModule has plugin-specified initial data
    //
    Buffer * createTraceBuffer( const char * name, const char * type,
                                UINT32 sizeInBytes, const UINT32 * pData )
        {
            // first check that the type is valid (that it exists in our string
            // to enum map), if not then fail
            //
            map< GpuTrace::TraceFileType, string >::const_iterator it =
                m_fileTypeMap.begin();
            map< GpuTrace::TraceFileType, string >::const_iterator it_end =
                m_fileTypeMap.end();
            string fileType( type );

            // we use a linear search here because we're doing a reverse
            // lookup ( string -> enum )
            //
            for ( ; it != it_end; ++it )
            {
                if ( it->second == fileType )
                    break;
            }
            if ( it == it_end )
            {
                ErrPrintf( "createTraceBuffer: unknown type '%s'\n", type );
                return 0;
            }

            GpuTrace::TraceFileType ftype = it->first;
            Trace3DTest * pT3dTest = m_pHost->getT3dTest();
            GpuTrace * pGpuTrace = pT3dTest->GetTrace();

            // Check if the buffer already exists
            if (SurfaceExists(name))
            {
                ErrPrintf( "createTraceBuffer: buffer name %s already exist"
                            "s\n", name );
                return 0;
            }

            // create the TraceModule and link it in to Trace3d's list of
            // TraceModules
            //
            GenericTraceModule * newMod = new GenericTraceModule( pT3dTest,
                                                           name, ftype,
                                                           sizeInBytes );
            newMod->SetProtect(Memory::ReadWrite);

            // create the plugin Buffer structure that represents the
            // TraceModule this object holds the data from the plugin until the
            // time that it's copied out into the traceModule
            //
            BufferMods * pBuffer = BufferMods::Create( m_pHost, name, type,
                                                       sizeInBytes, pData,
                                                       newMod );
            m_createdBufferList.push_back( pBuffer );

            // link the TraceModule to this plugin Buffer object so at the
            // right time we can copy the plugin-supplied data into the
            // TraceModule (simulates reading the TraceModule's data
            // from the trace file.  We do this unobtrusively to the
            // TraceModule object by storing the pBuffer as an opaque void *
            // in the TraceModule class
            //
            newMod->setPluginDataHandle( (void *) pBuffer );

            // add our new TraceModule to the trace so that trace_3d manages it
            // from now on, just as if it had been a FILE in the trace
            //
            pGpuTrace->AddTraceModule( newMod );

            return pBuffer;
        }

    Surface* GetSurfaceByName(const char* name)
    {
        Trace3DTest* pT3dTest = m_pHost->getT3dTest();
        return reinterpret_cast<Surface*>(pT3dTest->GetSurfaceByName(name));
    }

    Surface* CreateSurfaceView(const char * name,
                               const char * parent,
                               const char * options)
    {
        GpuTrace* pTrace = getT3dTest()->GetTrace();

        // Check if parent is indeed a Surface
        TraceModule* parentMod = pTrace->ModFind(parent);
        if (!parentMod || !parentMod->GetParameterSurface())
        {
            ErrPrintf("trace_3d: CreateSurfaceView: Parent surface \"%s\" "
                      "doesn't exist!\n", parent);
            return NULL;
        }

        // Check if the surface view already exists
        if (SurfaceExists(name))
        {
            // MODS does not allow modifying an existing
            // surface (view), we do the same here.
            ErrPrintf("trace_3d: CreateSurfaceView: Cannot create duplicate "
                      "surface view \"%s\"!\n", name);
            return NULL;
        }

        // Prepare the T3d command
        int buflen = strlen(name) +
                     strlen(parent) +
                     strlen(options) +
                     100; // Command & whitespace

        char* cmdbuf = new char[buflen];
        if (snprintf(cmdbuf, buflen,
                     "SURFACE_VIEW %s PARENT_SURFACE %s %s",
                     name, parent, options) <= 0)
            return NULL;

        // Ilwoke GpuTrace to create the surface view
        bool success = (OK == pTrace->ExelwteT3dCommands(cmdbuf));

        // Free the buffer before exit
        delete [] cmdbuf;

        // Allocate the view upon success
        MdiagSurf * pLWSurface = NULL;
        if (success)
        {
            TraceModule* module = pTrace->ModFind(name);
            MASSERT (module != NULL);

            if (OK != module->Allocate())
                return NULL;

            pLWSurface = module->GetParameterSurface();
            MASSERT(pLWSurface != NULL);
        }

        return reinterpret_cast<Surface*>(pLWSurface);
    }

    virtual Surface * AllocSurface(const char * name,
                                   UINT64 sizeInBytes,
                                   const std::map<std::string, std::string>& properties)
    {
        Trace3DTest* pTest = getT3dTest();
        GpuTrace* pTrace = pTest->GetTrace();

        // Check if the surface already exists
        if (SurfaceExists(name))
        {
            ErrPrintf("trace_3d: AllocSurface: Cannot create duplicate "
                      "surface \"%s\"!\n", name);
            return nullptr;
        }

        // Hmm... Tricky! But we know that actually MdiagSurf is created in CreateDynamicSurface
        MdiagSurf* pSurf = reinterpret_cast<MdiagSurf*>(CreateDynamicSurface(name));

        std::map<std::string, std::string>::const_iterator iter = properties.begin();
        for (; iter != properties.end(); ++iter)
        {
            if (iter->first == "VIRT_ADDRESS")
            {
                char * p = nullptr;
                errno = 0;
                const UINT64 virtualAddress =
                        Utility::Strtoull(iter->second.c_str(), &p, 0);
                if ((p == iter->second.c_str()) ||
                    (p && *p != 0) ||
                    (ERANGE == errno) || // Catching out of range
                    (virtualAddress == 0))
                {
                    ErrPrintf("trace_3d: AllocSurface: VIRT_ADDRESS is not valid: "
                               "%s!\n", iter->second.c_str());
                    return nullptr;
                }
                pSurf->SetFixedVirtAddr(virtualAddress);
            }
            else if (iter->first == "VIRT_ADDRESS_RANGE")
            {
                char * p = nullptr;
                errno = 0;
                std::vector<std::string> tokens;
                Utility::Tokenizer(iter->second, ":", &tokens);
                if (tokens.size() != 2)
                {
                    ErrPrintf("address range for VIRT_ADDRESS_RANGE is incorrect."
                            " Correct format is properties[\"VIRT_ADDRESS_RANGE\"] = \"<virtAddressMin>:<virtAddressMax>\"\n");
                }
                const UINT64 virtAddressMin =
                        Utility::Strtoull(tokens[0].c_str(), &p, 0);
                if ((p == iter->second.c_str()) ||
                    (p && *p != 0) ||
                    (ERANGE == errno) || // Catching out of range
                    (virtAddressMin == 0))
                {
                    ErrPrintf("trace_3d: AllocSurface: VIRT_ADDRESS_RANGE:Min is not valid: "
                               "%s!\n", tokens[0].c_str());
                    return nullptr;
                }
                const UINT64 virtAddressMax =
                        Utility::Strtoull(tokens[1].c_str(), &p, 0);
                if ((p == iter->second.c_str()) ||
                    (p && *p != 0) ||
                    (ERANGE == errno) || // Catching out of range
                    (virtAddressMax == 0))
                {
                    ErrPrintf("trace_3d: AllocSurface: VIRT_ADDRESS_RANGE:Max is not valid: "
                               "%s!\n", tokens[1].c_str());
                    return nullptr;
                }
                if (virtAddressMin > virtAddressMax)
                {
                     ErrPrintf("min address > max address in VIRT_ADDRESS_RANGE.\n");
                }
                pSurf->SetVirtAddrRange(virtAddressMin, virtAddressMax);
            }
            else if (iter->first == "PHYS_ADDRESS")
            {
                char * p = nullptr;
                errno = 0;
                const UINT64 physAddress =
                        Utility::Strtoull(iter->second.c_str(), &p, 0);
                if ((p == iter->second.c_str()) ||
                    (p && *p != 0) ||
                    (ERANGE == errno)) // Catching out of range
                {
                    ErrPrintf("trace_3d: AllocSurface: PHYS_ADDRESS is not valid: "
                               "%s!\n", iter->second.c_str());
                    return nullptr;
                }
                pSurf->SetFixedPhysAddr(physAddress);
            }
            else
            {
                ErrPrintf("trace_3d: AllocSurface: %s is not supported yet!: ", iter->first.c_str());
                return nullptr;
            }
        }

        pSurf->SetArrayPitch(sizeInBytes);
        SurfaceType surfaceType = pTrace->FindSurface(name);

        // add module
        SurfaceTraceModule * newMod = new SurfaceTraceModule( pTest,
            name, "", *pSurf, static_cast<size_t>(sizeInBytes), surfaceType);
        newMod->SetProtect(Memory::ReadWrite);
        newMod->SaveTrace3DSurface(pSurf); // GetSurfaceProperties needs
        pTrace->AddTraceModule( newMod );

        if (Utl::HasInstance())
        {
            Utl::Instance()->AddTestSurface(pTest, pSurf);
            Utl::Instance()->TriggerSurfaceAllocationEvent(pSurf, pTest);
        }

        // alloc
        if (pSurf->Alloc(pTest->GetBoundGpuDevice(), pTest->GetLwRmPtr()) != OK)
        {
            ErrPrintf("trace_3d: AllocSurface: Cannot create "
                                  "surface \"%s\"!\n", name);
                        return NULL;
        }
        newMod->UpdateOffset();

        m_createdSurfaceList.push_back(pSurf);
        Surface* pT3dSurf = reinterpret_cast<Surface*>(newMod->GetParameterSurface());

        return pT3dSurf;
    }

    void deleteBuffer( Buffer * pBuffer )
        {
            list< Buffer * >::iterator it =
                std::find( m_createdBufferList.begin(),
                           m_createdBufferList.end(), pBuffer );

            if ( it != m_createdBufferList.end() )
            {
                DebugPrintf(MSGID(), "trace_3d: deleteBuffer: deleting pBuffer %p\n",
                             *it );
                m_createdBufferList.erase( it );
                delete pBuffer;
            }
            else
            {
                ErrPrintf( "trace_3d: deletebuffer: pBuffer %p doesn't exist!"
                           "\n", pBuffer );
            }
        }

    // walk the list of trace modules and create a wrapper host-side Buffer
    // structure for each one
    //
    void refreshBufferList( void )
        {
            // get rid of the current plugin-side trace buffer list and recreate
            // a new one.  caches the common traits (name, surface type, size)
            // in the buffer object
            //
            deleteTraceBufferList();

            Trace3DTest * pT3dTest = m_pHost->getT3dTest();
            GpuTrace * pGpuTrace = pT3dTest->GetTrace();

            ModuleIter modIt = pGpuTrace->ModBegin();
            ModuleIter modIt_end = pGpuTrace->ModEnd();
            for ( ; modIt != modIt_end; ++modIt )
            {
                TraceModule * pTraceModule( *modIt );
                if ( pTraceModule->IsColorOrZ() )
                {
                    // this buffer is actually managed by the surface manager,
                    // so it will be discovered and placed in to the surface
                    // list in refreshSurfaces()
                    //
                    continue;
                }

                const MdiagSurf * pSurf( pTraceModule->GetDmaBuffer() );
                if ( pSurf && IsSurface(pSurf) )
                {
                    // this is actually a surface object, even though it is not
                    // managed by the surface manager, so don't enumerate it in
                    // the buffer list
                    //
                    continue;
                }

                string name( (*modIt)->GetName() );
                UINT32 gpuSubdevIdx = 0;
                CachedSurface* pCachedSurface = pTraceModule->GetCachedSurface();
                UINT32 size = (pCachedSurface != nullptr) ? pCachedSurface->GetTraceSurfSize(gpuSubdevIdx) : (*modIt)->GetSize();
                GpuTrace::TraceFileType ftype( (*modIt)->GetFileType() );
                string ftypeStr;

                // we're not setting initial data on these surfaces, we're
                // just making them enumerable and their properties queryable
                // so the plugin initial data field is unused in this case
                //
                UINT32 *pData = 0;

                // store the file type enum as a string
                //
                map< GpuTrace::TraceFileType, string >::const_iterator it;
                map< GpuTrace::TraceFileType, string >::const_iterator it_end =
                    m_fileTypeMap.end();
                it = m_fileTypeMap.find( ftype );
                if ( it == it_end )
                {
                    ftypeStr = "unknown";
                }
                else
                {
                    ftypeStr = it->second;
                }

                BufferMods * pBuffer = BufferMods::Create( m_pHost, name.c_str(),
                                                           ftypeStr.c_str(),
                                                           size, pData,
                                                           *modIt );
                m_traceBufferList.push_back( pBuffer );
            }
        }

    BufferIterator begin( void )
        {
            return m_traceBufferList.begin();
        }

    BufferIterator end( void )
        {
            return m_traceBufferList.end();
        }

    Trace3DTest * getT3dTest( void ) const
        {
            return m_pHost->getT3dTest();
        }

    void refreshSurfaces( void )
        {
            // this is a plugin-only function
            //
        }

    const vector<Surface *> & getSurfaces( void )
        {
            // dummy function, this is a plugin-side only function
            //
            return m_surfaces;
        }

    Surface* CreateDynamicSurface(const char* name)
    {
        MdiagSurf* surface = new MdiagSurf();
        surface->SetName(string(name));

        surface->SetColorFormat(ColorUtils::Y8);
        surface->SetForceSizeAlloc(true);

        surface->SetLayout(Surface2D::Pitch);
        surface->SetLocation(Memory::Optimal);

        surface->BindGpuChannel(0);

        // return an blank Surface for client to fill in
        // properties and allocate thereafter. Client is
        // responsible for freeing these Surfaces???
        return reinterpret_cast<Surface*>(surface);
    }

    void SetSkipBufferDownloads(bool skip)
    {
        Trace3DTest* test = m_pHost->getT3dTest();
        GpuTrace* trace = test->GetTrace();

        trace->SetSkipBufferDownloads(skip);
    }

    void RedoRelocations()
    {
        Trace3DTest* test = m_pHost->getT3dTest();
        GpuTrace* trace = test->GetTrace();

        ModuleIter iter = trace->ModBegin();
        ModuleIter iterEnd = trace->ModEnd();

        for (; iter != iterEnd; ++iter)
        {
            TraceModule* module(*iter);
            module->RedoRelocations();
        }
    }

    void DeclareTegraSharedSurface(const char *name)
    {
        Trace3DTest *test = m_pHost->getT3dTest();
        test->DeclareTegraSharedSurface(name);
    }

    void DeclareGlobalSharedSurface(const char *name, const char *global_name)
    {
        m_pHost->getT3dTest()->DeclareGlobalSharedSurface(name, global_name);
    }

    UINT64 GetActualPhysAddr(const char *surfName, UINT64 offset)
    {
        // fix me.
        MASSERT(PolicyManager::HasInstance());

        string surfaceName(surfName);
        PolicyManager *pPolicyManager = PolicyManager::Instance();
        PmSubsurfaces subsurfaces = pPolicyManager->GetActiveSubsurfaces();

        for(PmSubsurfaces::iterator ppSubsurface = subsurfaces.begin();
                ppSubsurface != subsurfaces.end();++ppSubsurface)
        {
            if((*ppSubsurface)->GetMdiagSurf()->GetName() == surfaceName
                    && ((*ppSubsurface)->GetOffset()<= offset &&
                    (*ppSubsurface)->GetOffset()+(*ppSubsurface)->GetSize()>=offset))
            {
                if((*ppSubsurface)->GetSurface()->GetHiddenTegraMappingsHelper())
                {
                    PmMemMappings Mappings = (*ppSubsurface)->GetSurface()->GetHiddenTegraMappingsHelper()->GetMemMappings(*ppSubsurface);
                    for(PmMemMappings::iterator ppMemMapping = Mappings.begin();ppMemMapping != Mappings.end();++ppMemMapping)
                    {
                        if(offset >= (*ppMemMapping)->GetOffset() && offset<=(*ppMemMapping)->GetOffset() + (*ppMemMapping)->GetSize())
                            return offset - (*ppMemMapping)->GetOffset() + (*ppMemMapping)->GetPhysAddr();
                    }
                }
                else
                {
                    PmMemMappings Mappings = (*ppSubsurface)->GetSurface()->GetMemMappingsHelper()->GetMemMappings(*ppSubsurface);
                    for(PmMemMappings::iterator ppMemMapping = Mappings.begin();ppMemMapping != Mappings.end();++ppMemMapping)
                    {
                        if(offset >= (*ppMemMapping)->GetOffset() && offset<=(*ppMemMapping)->GetOffset() + (*ppMemMapping)->GetSize())
                        {
                            if((*ppMemMapping)->GetMemMappingsHelper())
                            {
                                PmMemMappings SubMappings = (*ppMemMapping)->GetMemMappingsHelper()->GetMemMappings(*ppSubsurface);
                                for(PmMemMappings::iterator ppSubMemMapping = SubMappings.begin(); ppSubMemMapping != SubMappings.end(); ++ppSubMemMapping)
                                {
                                    if(offset >= (*ppSubsurface)->GetOffset() && offset <= (*ppSubsurface)->GetOffset() + (*ppSubsurface)->GetSize() )
                                        return offset -(*ppMemMapping)->GetOffset() + (*ppMemMapping)->GetPhysAddr();
                                }
                            }
                            else
                            {
                                return offset - (*ppMemMapping)->GetOffset() + (*ppMemMapping)->GetPhysAddr();
                            }
                        }
                    }
                }
            }
        }
        return 0;
    }

private:

    HostMods * m_pHost;
    map< GpuTrace::TraceFileType, string > m_fileTypeMap;

    // this is the list of buffers created through the createBuffer()
    // member function.   We remember them so we can delete their
    // storage (not the surfaces themselves, just the Buffer object) if
    // the user doesn't call deletebuffer() on them
    //
    list< Buffer * > m_createdBufferList;

    list< MdiagSurf * > m_createdSurfaceList;

    // this is the list of all buffers (trace modules) in the trace
    // the plugin can iterate over this list and look for surfaces by
    // name, type, size, etc.
    //
    list< Buffer * > m_traceBufferList;

    bool SurfaceExists(const string& name)
    {
        Trace3DTest* pTest = getT3dTest();
        GpuTrace* pTrace = pTest->GetTrace();

        TraceModule* module = pTrace->ModFind(name.c_str());
        return module != NULL;
    }

    // frees all the buffer objects in m_traceBufferList
    //
    void deleteTraceBufferList( void )
        {
            BufferIterator it = m_traceBufferList.begin();
            BufferIterator it_end = m_traceBufferList.end();
            for ( ; it != it_end; ++it )
                delete *it;

            m_traceBufferList.clear();
        }

    UINT32 GetTraversalDirection()
    {
        ErrPrintf("CDE engine is Deprecated. Not support BufferManager_GetTraversalDirection any longer.\n");
        MASSERT(0);
        return 0;
    }

    // dummy, only needed to instantiate class
    // (needed for getSurfaces function)
    //
    vector<Surface *> m_surfaces;
};

BufferManagerMods * BufferManagerMods::Create( HostMods * pHost )
{
    BufferManagerMods_impl * pBufferMngr = new BufferManagerMods_impl( pHost );
    return pBufferMngr;
}

class Buffer_impl : public BufferMods
{
public:

    friend BufferMods * BufferMods::Create( HostMods * pHost,
                                            const char * name,
                                            const char * type,
                                            UINT32 sizeInBytes,
                                            const UINT32 * pData,
                                            TraceModule * pTraceModule );

    explicit Buffer_impl( HostMods * pHost,
                          const char * name,
                          const char * type,
                          UINT32 sizeInBytes,
                          const UINT32 * pData,
                          TraceModule * pTraceModule )
        : m_pHost( pHost ),
          m_name( name ),
          m_type( type ),
          m_sizeInWords( (sizeInBytes + sizeof(UINT32) - 1) / sizeof(UINT32) ),
          m_pData( 0 ),
          m_pTraceModule( pTraceModule ),
          m_sizeInBytes( sizeInBytes )
        {
            if ( m_sizeInWords == 0 )
                m_sizeInWords = 1;

            m_hasPluginInitData = (pData != NULL);

            m_pData = new UINT32 [ m_sizeInWords  ];

            // pData is null when we're just creating a wrapper for an
            // existing trace module
            //
            if ( pData != 0 )
            {
                for ( UINT32 i = 0; i < m_sizeInWords; ++i )
                {
                    m_pData[ i ] = pData[ i ];
                }
            }
        }

    virtual ~Buffer_impl()
        {
            delete[] m_pData;
        }

    UINT64 getGpuVirtAddr( void ) const
        {
            if ( ! m_pTraceModule )
            {
                // need to recode this with return codes
                //
                return 0xFFFFFFFF;
            }

            UINT64 va = m_pTraceModule->GetOffset();
            return va;
        }

    UINT64 getGpuPhysAddr( UINT32 subdev ) const
        {
            if ( ! m_pTraceModule )
            {
                // need to recode this with return codes
                //
                return 0xFFFFFFFF;
            }

            UINT64 va = m_pTraceModule->GetDmaBuffer()->GetVidMemOffset();
            return va;
        }

    UINT32 getPteKind( void ) const
        {
            if( ! m_pTraceModule )
                return 0xFFFFFFFF;

            const MdiagSurf * dma = m_pTraceModule->GetDmaBuffer();
            if( ! dma )
                return 0xFFFFFFFF;

            return dma->GetPteKind();
        }

    UINT32 getOffsetWithinRegion( void ) const
        {
            if ( ! m_pTraceModule )
                return 0xFFFFFFFF;

            UINT32 offset = (UINT32)m_pTraceModule->GetOffsetWithinDmaBuf();
            return offset;
        }

    const char * getName( void ) const
        {
            return m_name.c_str();
        }

    const char * getType( void ) const
        {
            return m_type.c_str();
        }

    UINT32 getSize( void ) const
        {
            return m_sizeInBytes;
        }

    // copy nWords of the Buffer's initial plugin-set data into the given
    // destination
    //
    void copyData ( UINT32 * pDest, UINT32 nWords ) const
        {
            if ( nWords > m_sizeInWords )
                nWords = m_sizeInWords;

            for ( UINT32 i = 0; i < nWords; ++i )
            {
                pDest[ i ] = m_pData[ i ];
            }
        }

    // typically, the data is of no use once it's copied out, so allow the user
    // to specify when the data is no longer needed by calling this function,
    // which frees the user-specified initial data
    //
    void freeData( void )
        {
            delete[] m_pData;
            m_pData = 0;
            m_sizeInWords = 0;
        }

    bool hasPluginInitData() const
    {
        return m_hasPluginInitData;
    }

    // map (a portion of) the surface's memory so that the CPU can
    // play with it
    //
    void mapMemory( UINT32 offset,
                    UINT32 bytesToMap,
                    UINT64 * virtAddr,
                    UINT32 gpuSubdevIdx )
        {
            MdiagSurf * pDmaBuffer = m_pTraceModule->GetDmaBufferNonConst();

            UINT64 offsetWithinDmaBuf = offset +
                m_pTraceModule->GetOffsetWithinDmaBuf();
            RC rc = pDmaBuffer->MapRegion( offsetWithinDmaBuf, bytesToMap,
                                           gpuSubdevIdx );
            // need RC check
            //
            if ( rc != OK )
            {
                ErrPrintf( "trace_3d: %s: MapRegion( offsetWithinDmaBuf="
                           "0x%08llx, bytesToMap=0x%08x subdevidx=%d) failed\n",
                           __FUNCTION__, offsetWithinDmaBuf, bytesToMap,
                           gpuSubdevIdx );

                // need real RC code
                //
                *virtAddr = 0;
                return;
            }

            *virtAddr = ( UINT64 ) pDmaBuffer->GetAddress();
        }

    // release a prior CPU mapping of this surface's memory
    //
    void unmapMemory( UINT64 virtAddr,
                      UINT32 gpuSubdevIdx )
        {
            MdiagSurf * pDmaBuffer = m_pTraceModule->GetDmaBufferNonConst();
            pDmaBuffer->Unmap();
        }

    UINT32 * Get032Addr( UINT32 subdev, UINT32 offset )
        {
            // this is a plugin-side only function, completely implemented in
            // the plugin's shim layer
            //
            return 0;
        }

    TraceModule * getTraceModule() const
        {
            return m_pTraceModule;
        }

    void setProperty( const char* pPropName, const char* pPropValue )
    {
        // plugin side only function
        //
    }

    bool ReadUsingDma( UINT32 offset, UINT32 size, UINT08 * pData,
                       UINT32 gpuSubdevIdx )
    {
        MdiagSurf * pSurf = m_pTraceModule->GetDmaBufferNonConst();
        Trace3DTest * pT3dTest( m_pHost->getT3dTest() );
        TraceChannel * pTraceChannel( pT3dTest->GetDefaultChannelByVAS(pSurf->GetGpuVASpace()) );
        GpuVerif * gpuverif( pTraceChannel->GetGpuVerif() );

        UINT64 realOffset( offset + m_pTraceModule->GetOffsetWithinDmaBuf() );

        if ( ! gpuverif->GetDmaUtils()->UseDmaTransfer() )
        {
            UINT64 virtAddr = 0;
            this->mapMemory( offset, size, &virtAddr, gpuSubdevIdx );
            Platform::VirtualRd( (const volatile void *)virtAddr, pData, size );
            this->unmapMemory( virtAddr, 0 );
            return true;
        }

        if ( ! gpuverif->GetDmaUtils()->GetDmaReader() && ! gpuverif->GetDmaUtils()->SetupDmaBuffer() )
        {
            MASSERT(!"Failed to setup DMA buffer for readback\n");
        }

        auto pDmaSurfReader = SurfaceUtils::CreateDmaSurfaceReader(*pSurf,
            gpuverif->GetDmaUtils()->GetDmaReader());
        SurfaceUtils::ISurfaceReader* pSurfReader = pDmaSurfReader.get();

        if (OK != pSurfReader->ReadBytes(realOffset, pData, size))
        {
            ErrPrintf( "Failed to read data for buffer %s\n", pSurf->GetName().c_str() );
            return false;
        }

        return true;
    }

    UINT32 SendGpEntry(const char* chName, UINT64 offset, UINT32 size, bool sync)
    {
        RC rc = OK;

        TraceChannel* tChannel = m_pHost->getT3dTest()->GetChannel(chName, GpuTrace::CPU_MANAGED);
        UINT64 get = getGpuVirtAddr() + offset;

        if (sync)
        {
            rc = tChannel->GetCh()->GetModsChannel()->SetSyncEnable(sync);
            if (rc != OK)
            {
                ErrPrintf("Buffer::SendGpEntry: sync enable failed\n");
                return 1;
            }
        }
        // CallSubroutine() really does not care which subchannel, choose first one
        rc = tChannel->GetSubChannel("")->GetSubCh()->CallSubroutine(get, size);
        if (rc != OK)
        {
            ErrPrintf("Buffer::SendGpEntry: send failed\n");
            return 1;
        }
        if (sync)
        {
            rc = tChannel->GetCh()->GetModsChannel()->SetSyncEnable(false);
            if (rc != OK)
            {
                ErrPrintf("Buffer::SendGpEntry: sync disable failed\n");
                return 1;
            }
        }
        return 0;
    }

    UINT32 WriteBuffer( UINT64 offset, const void* buf, size_t size,
                      UINT32 gpuSubdevIdx )
    {
        MdiagSurf * pSurf( m_pTraceModule->GetDmaBufferNonConst() );
        UINT64 realOffset( offset + m_pTraceModule->GetOffsetWithinDmaBuf() );
        RC rc( SurfaceUtils::WriteSurface( *(pSurf->GetSurf2D()), realOffset, buf, size,
                                            gpuSubdevIdx ) );
        if (rc != OK)
        {
            ErrPrintf("Buffer::WriteBuffer: write surface failed\n");
            return 1;
        }
        return 0;
    }

    UINT32 getGpuPartitionIndex(UINT64 physAddr)
    {
        MASSERT(m_pTraceModule);
        const MdiagSurf *pSurf(m_pTraceModule->GetDmaBuffer());
        FbDecode decode;
        GetFbDecode(decode, pSurf, physAddr);
        return decode.virtualFbio;
    }

    UINT32 getGpuLtcIndex(UINT64 physAddr)
    {
        MASSERT(m_pTraceModule);
        const MdiagSurf *pSurf(m_pTraceModule->GetDmaBuffer());
        FbDecode decode;
        GetFbDecode(decode, pSurf, physAddr);
        return pSurf->GetGpuSubdev(0)->GetFB()->VirtualFbioToVirtualLtc(
                decode.virtualFbio, decode.subpartition);
    }

    UINT32 getGpuL2SliceIndex(UINT64 physAddr)
    {
        MASSERT(m_pTraceModule);
        const MdiagSurf *pSurf(m_pTraceModule->GetDmaBuffer());
        FbDecode decode;
        GetFbDecode(decode, pSurf, physAddr);
        return decode.slice;
    }

    UINT32 getGpuXbarRawAddr(UINT64 physAddr)
    {
        MASSERT(m_pTraceModule);
        const MdiagSurf *pSurf(m_pTraceModule->GetDmaBuffer());
        FbDecode decode;
        GetFbDecode(decode, pSurf, physAddr);
        return decode.xbarRawAddr;
    }

    UINT32 getGpuSubpartition(UINT64 physAddr)
    {
        MASSERT(m_pTraceModule);
        const MdiagSurf *pSurf(m_pTraceModule->GetDmaBuffer());
        FbDecode decode;
        GetFbDecode(decode, pSurf, physAddr);
        return decode.subpartition;
    }

    UINT32 getGpuPCIndex(UINT64 physAddr)
    {
        MASSERT(m_pTraceModule);
        const MdiagSurf *pSurf(m_pTraceModule->GetDmaBuffer());
        FbDecode decode;
        GetFbDecode(decode, pSurf, physAddr);
        return decode.pseudoChannel;
    }

    UINT32 getGpuRBCAddr(UINT64 physAddr, UINT32 errCount, UINT32 errPos)
    {
        MASSERT(m_pTraceModule);
        const MdiagSurf *pSurf(m_pTraceModule->GetDmaBuffer());
        EccAddrParams eccDecode;
        GetRBCAddress(eccDecode, pSurf, physAddr, errCount, errPos);
        return eccDecode.eccAddr;
    }

    UINT32 getGpuRBCAddrExt(UINT64 physAddr, UINT32 errCount, UINT32 errPos)
    {
        MASSERT(m_pTraceModule);
        const MdiagSurf *pSurf(m_pTraceModule->GetDmaBuffer());
        EccAddrParams eccDecode;
        GetRBCAddress(eccDecode, pSurf, physAddr, errCount, errPos);
        return eccDecode.eccAddrExt;
    }

    UINT32 getGpuRBCAddrExt2(UINT64 physAddr, UINT32 errCount, UINT32 errPos)
    {
        MASSERT(m_pTraceModule);
        const MdiagSurf *pSurf(m_pTraceModule->GetDmaBuffer());
        EccAddrParams eccDecode;
        GetRBCAddress(eccDecode, pSurf, physAddr, errCount, errPos);
        return eccDecode.eccAddrExt2;
    }

    UINT32 getEccInjectRegVal(UINT64 physAddr, UINT32 errCount, UINT32 errPos)
    {
        MASSERT(m_pTraceModule);
        const MdiagSurf *pSurf(m_pTraceModule->GetDmaBuffer());
        EccErrInjectParams eccErrInjParams;
        GetEccInjectRegVal(eccErrInjParams, pSurf, physAddr, errCount, errPos);
        return eccErrInjParams.eccErrInjectAddr;
    }

    UINT32 getEccInjectExtRegVal(UINT64 physAddr, UINT32 errCount, UINT32 errPos)
    {
        MASSERT(m_pTraceModule);
        const MdiagSurf *pSurf(m_pTraceModule->GetDmaBuffer());
        EccErrInjectParams eccErrInjParams;
        GetEccInjectRegVal(eccErrInjParams, pSurf, physAddr, errCount, errPos);
        return eccErrInjParams.eccErrInjectAddrExt;
    }

    void mapEccAddrToPhysAddr(const EccAddrParams &eccAddr, UINT64 *pPhysAddr)
    {
        MASSERT(m_pTraceModule);
        const MdiagSurf *pSurf(m_pTraceModule->GetDmaBuffer());
        UINT32 pteKind = UINT32(pSurf->GetPteKind());
        UINT32 pageSize = pSurf->GetPageSize();
        if (0 == pageSize)
        {
            pageSize = pSurf->GetActualPageSizeKB();
        }
        // We cannot decode address under 0 page size.
        MASSERT(pageSize != 0 && pageSize != ~0U);

        pSurf->GetGpuSubdev(0)->GetFB()->EccAddrToPhysAddr(eccAddr, pteKind, pageSize, pPhysAddr);
    }

    bool IsAllocated() const
    {
        MASSERT(m_pTraceModule);
        const MdiagSurf *pSurf(m_pTraceModule->GetDmaBuffer());

        if ((pSurf != 0) && pSurf->IsAllocated())
        {
            return true;
        }

        return false;
    }

    bool IsGpuMapped() const
    {
        MASSERT(m_pTraceModule);
        const MdiagSurf *pSurf(m_pTraceModule->GetDmaBuffer());

        if ((pSurf != 0) && pSurf->IsGpuMapped())
        {
            return true;
        }

        return false;
    }

    UINT32 GetTagCount() const
    {
        MASSERT(m_pTraceModule);

        return m_pTraceModule->GetTagList().size();
    }

    const char* GetTag(UINT32 tagIndex) const
    {
        MASSERT(m_pTraceModule);
        MASSERT(tagIndex < GetTagCount());

        return m_pTraceModule->GetTagList()[tagIndex].c_str();
    }

    const vector<string>& GetTagList() const
    {
        // Dummy function - This is for the HW side only.
        return m_TagList;
    }

    SegmentList* CreateSegmentList(string which, UINT64 offset)
    {
        MASSERT(m_pTraceModule);

        MdiagSurf * dma = m_pTraceModule->GetDmaBufferNonConst();

        MASSERT(dma);

        UINT64 offsetWithinDmaBuf = offset +
            m_pTraceModule->GetOffsetWithinDmaBuf();

        MdiagSegList* pSegList;

        RC rc(dma->CreateSegmentList(which, offsetWithinDmaBuf, &pSegList));

        return (OK==rc) ? reinterpret_cast<SegmentList*>(pSegList) : nullptr;
    }

#undef TEMP_PACK

private:

    HostMods * m_pHost;
    string m_name;
    string m_type;
    UINT32 m_sizeInWords;
    UINT32 * m_pData;
    TraceModule * m_pTraceModule;
    UINT32 m_sizeInBytes;
    bool m_hasPluginInitData;
    vector<string> m_TagList;
};

BufferMods * BufferMods::Create( HostMods * pHost,
                                 const char * name,
                                 const char * type,
                                 UINT32 size,
                                 const UINT32 * pData,
                                 TraceModule * pTraceModule )
{
    Buffer_impl * pBuffer = new Buffer_impl( pHost, name, type, size, pData,
                                             pTraceModule );
    return pBuffer;
}

class MemMods_impl : public MemMods
{
public:

    friend MemMods * MemMods::Create( HostMods * pHost );

    explicit MemMods_impl( HostMods * pHost )
        {
            m_pGpuDevice = pHost->getT3dTest()->GetBoundGpuDevice();
            m_pLwRm = pHost->getT3dTest()->GetLwRmPtr();
        }

    virtual ~MemMods_impl() {}

    UINT08 VirtRd08( UINT64 va ) const
        {
            return MEM_RD08( (uintptr_t) va );
        }

    UINT16 VirtRd16( UINT64 va ) const
        {
            return MEM_RD16( (uintptr_t) va );
        }

    UINT32 VirtRd32( UINT64 va ) const
        {
            return MEM_RD32( (uintptr_t) va );
        }

    UINT64 VirtRd64( UINT64 va ) const
        {
            return MEM_RD64( (uintptr_t) va );
        }

    void VirtRd( UINT64 va, void * data, UINT32 count ) const
        {
            Platform::VirtualRd( (const volatile void *)(uintptr_t) va, data, count);
        }

    void VirtWr08( UINT64 va, UINT08 data )
        {
            MEM_WR08( (uintptr_t) va, data );
        }

    void VirtWr16( UINT64 va, UINT16 data )
        {
            MEM_WR16( (uintptr_t) va, data );
        }

    void VirtWr32( UINT64 va, UINT32 data )
        {
            MEM_WR32( (uintptr_t) va, data );
        }

    void VirtWr64( UINT64 va, UINT64 data )
        {
            MEM_WR64( (uintptr_t) va, data );
        }

    void VirtWr( UINT64 va, const void * data, UINT32 count)
        {
            Platform::VirtualWr( (volatile void *)(uintptr_t) va, data, count);
        }

    UINT08 PhysRd08( UINT64 pa ) const
    {
        return Platform::PhysRd08(pa);
    }

    UINT16 PhysRd16( UINT64 pa ) const
    {
        return Platform::PhysRd16(pa);
    }

    UINT32 PhysRd32( UINT64 pa ) const
    {
        return Platform::PhysRd32(pa);
    }

    UINT64 PhysRd64( UINT64 pa ) const
    {
        return Platform::PhysRd64(pa);
    }

    void PhysRd( UINT64 pa, void * data, UINT32 count ) const
    {
        Platform::PhysRd(pa, data, count);
    }

    void PhysWr08( UINT64 pa, UINT08 data )
    {
        Platform::PhysWr08(pa, data);
    }

    void PhysWr16( UINT64 pa, UINT16 data )
    {
        Platform::PhysWr16(pa, data);
    }

    void PhysWr32( UINT64 pa, UINT32 data )
    {
        Platform::PhysWr32(pa, data);
    }

    void PhysWr64( UINT64 pa, UINT64 data )
    {
        Platform::PhysWr64(pa, data);
    }

    void PhysWr( UINT64 pa, const void * data, UINT32 count)
    {
        Platform::PhysWr(pa, data, count);
    }

    bool LwRmMapMemory( UINT32 MemoryHandle,
                     UINT64 Offset,
                     UINT64 Length,
                     void  ** pAddress )
        {
            // plugin side only
            //
            return false;
        }

    void LwRmUnmapMemory( UINT32 MemoryHandle,
                        volatile void * Address )
        {
            // plugin side only
            //
        }

    bool LwRmMapMemoryDma( UINT32 hDma,
                         UINT32 hMemory,
                         UINT64 offset,
                         UINT64 length,
                         UINT32 flags,
                         UINT64 * pDmaOffset )
        {
            // plugin side only
            //
            return false;
        }

    void LwRmUnmapMemoryDma( UINT32 hDma,
                           UINT32 hMemory,
                           UINT32 flags,
                           UINT64 dmaOffset )
        {
            // plugin side only
            //
        }

    bool LwRmAllocOnDevice( UINT32 * pRtnObjectHandle,
                          UINT32 Class,
                          void * Parameters )
        {
            // plugin side only
            //
            return false;
        }

    void LwRmFree( UINT32 handle )
        {
            // plugin side only
            //
        }

    void FlushCpuWriteCombineBuffer()
        {
            // plugin side only
            //
        }

    bool MapDeviceMemory
    (
        void**        pReturnedVirtualAddress,
        UINT64        PhysicalAddress,
        size_t        NumBytes,
        MemoryAttrib  Attrib,
        MemoryProtect Protect
    )
    {
        // handled by plgnsprt.cpp
        return true;
    }

    void UnMapDeviceMemory ( void* VirtualAddress )
    {
        // handled by plgnsprt.cpp
    }

    bool AllocIRam( UINT32 Size, UINT64* VirtAddr, UINT64* PhysAddr )
    {
        void *va;
        if(OK == CheetAh::SocPtr()->IramAlloc(Size, &va))
        {
            *VirtAddr = (UINT64)va;
            *PhysAddr = Platform::VirtualToPhysical(va);
            InfoPrintf("Alloc IRAM: VA: 0x%llx, PA:0x%llx\n", *VirtAddr, *PhysAddr);
            return true;
        }
        return false;
    }

    void FreeIRam( LwU64 VirtAddr )
    {
        CheetAh::SocPtr()->IramFree(reinterpret_cast<void*>(VirtAddr));
        InfoPrintf("Free IRAM: VA: 0x%llx\n", VirtAddr);
    }

    GpuDevice* GetGpuDevice() { return m_pGpuDevice; }
    LwRm * GetLwRmPtr() { return m_pLwRm; }
private:
    GpuDevice* m_pGpuDevice;
    LwRm * m_pLwRm;
};

MemMods * MemMods::Create( HostMods * pHost )
{
    MemMods_impl * pMem = new MemMods_impl( pHost );
    return pMem;
}

// tracemanager

class TraceManagerMods_impl : public TraceManagerMods
{
public:

    friend TraceManagerMods * TraceManagerMods::Create( HostMods * pHost );

    explicit TraceManagerMods_impl( HostMods * pHost )
        : m_pHost( pHost )
        {
        }

    virtual ~TraceManagerMods_impl() {}

    // makes sure all sent methods have made it into the pushbuffer
    // and bumps the gpu's put pointer for all channels
    //
    int FlushMethodsAllChannels( void )
        {
            RC rc = m_pHost->getT3dTest()->FlushMethodsAllChannels();
            if ( rc != OK )
                return 1;
            return 0;
        }

    // wait until the gpu's pushbuffer "get" pointers have all
    // reached the "put" pointers in all channels
    //
    int WaitForDMAPushAllChannels( void ) const
        {
            RC rc = m_pHost->getT3dTest()->WaitForDMAPushAllChannels();
            if ( rc != OK )
                return 1;
            return 0;
        }

    // retrieves the version number of the trace_3d trace
    // (i.e., the "test.hdr" protocol number)
    //
    UINT32 getTraceVersion( void ) const
        {
            GpuTrace *pGpuTrace = m_pHost->getT3dTest()->GetTrace();
            if ( pGpuTrace == 0 )
                return 0xffffffff;
            return pGpuTrace->GetVersion();
        }

    UINT32 GetMethodCountOneChannel( UINT32 chNum ) const
        {
            return m_pHost->getT3dTest()->GetMethodCountOneChannel(chNum);
        }

    UINT32 GetMethodWriteCountOneChannel( UINT32 chNum ) const
        {
            return m_pHost->getT3dTest()->GetMethodWriteCountOneChannel(chNum);
        }

    int WaitForIdleOneChannel( UINT32 chNum ) const
        {
            RC rc = m_pHost->getT3dTest()->WaitForIdleRmIdleOneChannel(chNum);
            if ( rc != OK )
                return 1;
            return 0;
        }

    int WaitForIdleAllChannels( void ) const
        {
            RC rc = m_pHost->getT3dTest()->WaitForIdleHostSemaphoreIdleAllChannels();
            if ( rc != OK )
                return 1;
            return 0;
        }

    Trace3DTest * getT3dTest( void ) const
        {
            return m_pHost->getT3dTest();
        }

    void refreshTraceOps( void )
        {
            // this is a plugin-side only function
            //
        }

    const vector<TraceOp *> & getTraceOps( void ) const
        {
            // this is a plugin-side only function
            //
            return m_dummyTraceOpVector;
        }

    void refreshChannels( void )
        {
            // this is a plugin-side only operation
            //
        }

    const vector<Resource> & getChannels( void ) const
        {
            return m_dummyChannelVector;
        }

    void * getTraceChannelPointer( const char * chName ) const
    {
        // this is a plugin side only operation
        //
        return 0;
    }

    int MethodWrite( void * pTraceChannel, int subchannel,
                     UINT32 method, UINT32 data )
    {
        // this is a plugin side only operation
        //
        return 0;
    }

    int SetObject( void * pTraceChannel, int subch, UINT32 handle)
    {
        // this is a plugin side only operation
        //
        return 0;
    }

    UINT32 CreateObject( void * pTraceChannel, UINT32 Class,
                         void *Params = NULL, UINT32 *retClass = NULL)
    {
        // this is a plugin side only operation
        //
        return 0;
    }

    void DestroyObject( void * pTraceChannel, UINT32 handle)
    {
        // this is a plugin side only operation
        //
    }

    bool GetChannelRefCount( void * pTraceChannel, UINT32 * pValue ) const
    {
        // this is a plugin side only operation
        //
        return false;
    }

    // build a resource (property list) of trace subchannel to replay subchannel mappings
    // for a particular channel
    //
    Resource getSubchannels( void * pTraceChannel )
    {
        // this is a plugin side only operation
        //
        return Resource( "dummy resource", 0, 0, 0 );
    }

    const char* getTraceHeaderFile()
    {
        m_fullTraceHeaderFile = getT3dTest()->GetTraceHeaderFile();
        m_fullTraceHeaderFile = Utility::DefaultFindFile(m_fullTraceHeaderFile, true);
        return m_fullTraceHeaderFile.c_str();
    }

    const char* getTestName()
    {
        return getT3dTest()->GetTestName();
    }

    int RunTraceOps()
    {
        return getT3dTest()->GetTrace()->RunTraceOps();
    }

    int StartPerfMon(void* pTraceChannel, const char* name)
    {
        TraceChannel* pTraceChan( (TraceChannel*) pTraceChannel );
        TraceOpEventPMStart traceOp(getT3dTest(), pTraceChan, name);
        RC rc = traceOp.Run();
        if ( rc != OK )
            return 1;
        return 0;
    }

    int StopPerfMon(void* pTraceChannel, const char* name)
    {
        TraceChannel* pTraceChan( (TraceChannel*) pTraceChannel );
        TraceOpEventPMStop traceOp(getT3dTest(), pTraceChan, name);
        RC rc = traceOp.Run();
        if ( rc != OK )
            return 1;
        return 0;
    }

    void StopVpmCapture()
    {
        auto pGpuDevice = getT3dTest()->GetBoundGpuDevice();
        for (UINT32 i = 0; i < pGpuDevice->GetNumSubdevices(); i++)
        {
            pGpuDevice->GetSubdevice(i)->StopVpmCapture();
        }
    }

    const char* GetSyspipe()
    {
        static string sysPipeStr;
        Trace3DTest *pTest = getT3dTest();
        SmcEngine *pSmcEngine = pTest->GetSmcEngine();
        GpuDevice* pGpuDev(pTest->GetBoundGpuDevice());
        if (pGpuDev->GetFamily() < GpuDevice::Ampere)
            return "";

        if (pSmcEngine)
        {
            sysPipeStr = SmcResourceController::ColwertSysPipe2Str(pSmcEngine->GetSysPipe());
            return sysPipeStr.c_str();
        }
        return "sys0";
    }

    void AbortTest()
    {
        return getT3dTest()->AbortTest();
    }

private:

    HostMods * m_pHost;

    // unused on trace_3d side, but we need to have it to implement the
    // empty "getTraceOps" function
    //
    vector<TraceOp *> m_dummyTraceOpVector;

    // unused on trace_3d side, but we need to have it to implement the
    // empty "getChannels" function
    //
    vector<Resource> m_dummyChannelVector;

    string m_fullTraceHeaderFile;
};

TraceManagerMods * TraceManagerMods::Create( HostMods * pHost )
{
    TraceManagerMods_impl * pMem = new TraceManagerMods_impl( pHost );
    return pMem;
}

// utility manager

class UtilityManagerMods_impl : public UtilityManagerMods
{
public:

    friend UtilityManagerMods * UtilityManagerMods::Create( HostMods * pHost );

    explicit UtilityManagerMods_impl( HostMods * pHost )
        : m_pHost( pHost )
        {
        }

    virtual ~UtilityManagerMods_impl()
    {
        // unhook any outstanding hooked IRQs that we've been tracking from the plugin
        std::set< std::pair<UINT32, IsrData> >::iterator it = s_hookedIRQs.begin();
        while ( it != s_hookedIRQs.end() )
        {
            std::set< std::pair<UINT32, IsrData> >::iterator prev = it++;

            if( !UnhookIRQ( prev->first, prev->second.GetIsr(), prev->second.GetCookie() ) )
            {
                ErrPrintf("~UtilityManagerMods_impl: error unhooking IRQ.\n");
            }
        }
    }

    // get the time in NS
    //
    UINT64 GetTimeNS( void )
        {
            return Platform::GetTimeNS();
        }

    // gets the number of clock ticks
    //
    UINT64 GetSimulatorTime( void )
        {
            return Platform::GetSimulatorTime();
        }

    // gets the hang timeout
    //
    FLOAT64 GetTimeoutMS( void )
        {
            return m_pHost->getT3dTest()->GetParam()->ParamFloat("-timeout_ms", 10000);
        }

    // yield to other mods threads of exelwtion (e.g., simulators)
    //
    void _Yield( void )
        {
            Tasker::Yield();
        }

    UINT32 getNumErrorLoggerStrings( void ) const
        {
            return ErrorLogger::GetErrorCount();
        }

    const char * getErrorLoggerString( UINT32 strNum ) const
        {
            // need static storage that won't dissappear due to thread
            // switching: avoid race conditions between the time we fetch
            // the data from the error logger and when the plugin can
            // read/save it aside
            //
            static string errorString;
            errorString = ErrorLogger::GetErrorString(strNum);
            return errorString.c_str();
        }

    int EscapeWrite( const char * Path, UINT32 Index, UINT32 Size,
                     UINT32 Value )
        {
            // this is a plugin-only call implemented in the shim layer directly
            //
            return -1;
        }

    int EscapeRead(  const char * Path, UINT32 Index, UINT32 Size,
                     UINT32 * Value )
        {
            // this is a plugin-only call implemented in the shim layer directly
            //
            return -1;
        }

    int GpuEscapeWriteBuffer( UINT32 GpuId, const char * Path, UINT32 Index, size_t Size,
                     const void *Buf )
        {
            // this is a plugin-only call implemented in the shim layer directly
            //
            return -1;
        }

    int GpuEscapeReadBuffer( UINT32 GpuId, const char * Path, UINT32 Index, size_t Size,
                     void *BUf )
        {
            // this is a plugin-only call implemented in the shim layer directly
            //
            return -1;
        }

    int ExitMods()
    {
        RC rc;
        WarnPrintf("UtilityManager_ExitMods: Exiting MODS!\n");
        Platform::SetForcedTermination();
        Utility::ExitMods(rc, Utility::ExitQuickAndDirty);

        // should never get here
        return rc;
    }

    T3dPlugin::SimulationMode getSimulationMode() const
    {
        // this is a plugin-only call implemented in the shim layer directly
        //
        return T3dPlugin::Hardware;
    }

    bool isEmulation() const
    {
        Trace3DTest* pT3dTest = m_pHost->getT3dTest();
        if(NULL == pT3dTest)
            ErrPrintf("UtilityManager_IsEmulation: Trace3DTest* is not referenced properly" );
        GpuDevice* pGpuDev(pT3dTest->GetBoundGpuDevice() );
        if(NULL == pGpuDev)
            ErrPrintf("UtilityManager_IsEmulation: GpuDevice* is not referenced properly" );
        GpuSubdevice* pGpuSub(pGpuDev->GetSubdevice(0) );
        if(NULL == pGpuSub)
            ErrPrintf("UtilityManager_IsEmulation: GpuSubdevice* is not referenced properly" );

        return pGpuSub->IsEmulation();
    }

    UINT32 AllocEvent(UINT32* hEvent)
    {
        ModsEvent *pEvent = Tasker::AllocEvent();
        if (!pEvent)
        {
            ErrPrintf("UtilityManager: tasker event creation failed\n");
            return 1;
        }

        *hEvent = m_modsEvents.size();
        m_modsEvents.push_back(pEvent);
        return 0;
    }

    UINT32 FreeEvent(UINT32 hEvent)
    {
        if (hEvent >= m_modsEvents.size() || !m_modsEvents[hEvent])
        {
            ErrPrintf("UtilityManager: tasker event free failed\n");
            return 1;
        }
        Tasker::FreeEvent(m_modsEvents[hEvent]);
        m_modsEvents[hEvent] = 0;
        return 0;
    }

    UINT32 ResetEvent(UINT32 hEvent)
    {
        if (hEvent >= m_modsEvents.size() || !m_modsEvents[hEvent])
        {
            ErrPrintf("UtilityManager: tasker event reset failed\n");
            return 1;
        }
        Tasker::ResetEvent(m_modsEvents[hEvent]);
        return 0;
    }

    UINT32 SetEvent(UINT32 hEvent)
    {
        if (hEvent >= m_modsEvents.size() || !m_modsEvents[hEvent])
        {
            ErrPrintf("UtilityManager: tasker event set failed\n");
            return 1;
        }
        Tasker::SetEvent(m_modsEvents[hEvent]);
        return 0;
    }

    UINT32 IsEventSet(UINT32 hEvent, bool* isSet)
    {
        if (hEvent >= m_modsEvents.size() || !m_modsEvents[hEvent])
        {
            ErrPrintf("UtilityManager: tasker event isSet failed\n");
            return 1;
        }
        *isSet = Tasker::IsEventSet(m_modsEvents[hEvent]);
        return 0;
    }

    // The below logic is a blend of GpuSubdevice::HookResmanEvent()
    // and PMU::Initialize().
    UINT32 HookPmuEvent(UINT32 hEvent, UINT32 index)
    {
        // Lwrrently, allow only one hook per event type - see
        // GpuSubdevice::HookResmanEvent()
        if (m_eventHooks.count(index) == 0)
        {
            RC rc;
            GpuSubdevice *pSubDev = m_pHost->getT3dTest()->GetBoundGpuDevice()->GetSubdevice(0);

            PMU* pPmu;
            rc = pSubDev->GetPmu(&pPmu);
            if (rc != OK)
            {
                ErrPrintf("UtilityManager::HookPmuEvent: GetPmu failed\n");
                return 1;
            }
            // alloc event thread and give it a handler - see
            // PMU::Initialize()
            EventThread  *pEventThread = new EventThread();
            m_eventHooks[index].pEventThread = pEventThread;
            // send in the mods event as user data, so the plugin
            // can be notified.
            rc = m_eventHooks[index].pEventThread->SetHandler(HandleResmanEvent, m_modsEvents[hEvent]);
            if (rc != OK)
            {
                ErrPrintf("UtilityManager::HookPmuEvent: SetHandler failed\n");
                return 1;
            }
            // Lwrrently, listen for all PMU events.
            rc = pPmu->AddEvent(pEventThread->GetEvent(),
                                  PMU::ALL_UNIT_ID,
                                  true,
                                  RM_PMU_UNIT_INIT);
            if (rc != OK)
            {
                ErrPrintf("UtilityManager::HookPmuEvent: AddEvent failed\n");
                return 1;
            }
        }
        return 0;
    }

    UINT32 UnhookPmuEvent(UINT32 index)
    {
        if (m_eventHooks.count(index) != 0)
        {
            RC rc;
            GpuSubdevice *pSubDev = m_pHost->getT3dTest()->GetBoundGpuDevice()->GetSubdevice(0);

            PMU* pPmu;
            rc = pSubDev->GetPmu(&pPmu);
            if (rc != OK)
            {
                ErrPrintf("UtilityManager::UnhookPmuEvent: GetPmu failed\n");
                return 1;
            }
            pPmu->DeleteEvent(m_eventHooks[index].pEventThread->GetEvent());
            rc = m_eventHooks[index].pEventThread->SetHandler(NULL);
            if (rc != OK)
            {
                ErrPrintf("UtilityManager::UnhookPmuEvent: SetHandler failed\n");
                return 1;
            }
            delete m_eventHooks[index].pEventThread;
            m_eventHooks.erase(index);
        }
        return 0;
    }

    void FailTest(UtilityManager::TestStatus status = UtilityManager::TEST_FAILED)
    {
        m_pHost->getT3dTest()->FailTest( (Test::TestStatus) status);
    }

    static void HandleResmanEvent(void *ppThis)
    {
        Tasker::SetEvent((ModsEvent*)ppThis);
    }

    Semaphore* AllocSemaphore(UINT64 physAddr, SemaphorePayloadSize size, bool isSyncptShimSema=false)
    {
        return SemaphoreMods::Create(m_pHost, physAddr, size, isSyncptShimSema);
    }

    void FreeSemaphore(Semaphore* pSema)
    {
        if(pSema != nullptr)
        {
            delete pSema;
        }
    }

    UINT32 PciRead32(INT32 DomainNumber, INT32 BusNumber, INT32 DeviceNumber,
        INT32 FunctionNumber, INT32 Address, UINT32* Data)
    {
        RC rc;
        rc = Platform::PciRead32(DomainNumber, BusNumber, DeviceNumber, FunctionNumber,
                 Address, Data);
        if (rc != OK)
        {
            ErrPrintf("UtilityManager::PciRead32: read failed\n");
            return 1;
        }
        return 0;
    }

    UINT32 PciWrite32(INT32 DomainNumber, INT32 BusNumber, INT32 DeviceNumber,
        INT32 FunctionNumber, INT32 Address, UINT32 Data)
    {
        RC rc;
        rc = Platform::PciWrite32(DomainNumber, BusNumber, DeviceNumber, FunctionNumber,
                 Address, Data);
        if (rc != OK)
        {
            ErrPrintf("UtilityManager::PciWrite32: write failed\n");
            return 1;
        }
        return 0;
    }

    UINT64 AllocRawSysMem(UINT64 nbytes)
    {
        RC rc;
        void* ptr = NULL;
        rc = Platform::AllocPages(nbytes, &ptr, MODS_FALSE, 64, Memory::CA, 0);
        if (rc != OK)
        {
            ErrPrintf("UtilityManager::AllocRawSysMem: allocation failed\n");
            return (UINT64)~0;
        }

        return Platform::GetPhysicalAddress(ptr, 0);

    }

    void TriggerUtlUserEvent(const map<string, string> &userData)
    {
        if (Utl::HasInstance())
        {
            map<string, string> userDataCopy(userData); // Copy from const lvalue so it can be cast to rvalue
            Utl::Instance()->TriggerUserEvent(move(userDataCopy));
        }
        else
        {
            DebugPrintf("UtlUserEvent triggered by plugin but no UTL instance is running\n");
        }
    }

    UINT32 FindPciDevice
    (
        INT32   DeviceId,
        INT32   VendorId,
        INT32   Index,
        UINT32* pDomainNumber,
        UINT32* pBusNumber,
        UINT32* pDeviceNumber,
        UINT32* pFunctionNumber
    )
    {
        RC rc;
        rc = Platform::FindPciDevice(DeviceId, VendorId, Index, pDomainNumber,
                                     pBusNumber, pDeviceNumber, pFunctionNumber);
        if (rc != OK)
        {
            ErrPrintf("UtilityManager::FindPciDevice: read domain, bus, device, function fail, current domain = %d, bus = %d, device = %d, function = %d\n",
                            *pDomainNumber, *pBusNumber, *pDeviceNumber, *pFunctionNumber);
            return 1;
        }
        return 0;
    }

    UINT32 FindPciClassCode
    (
        INT32   ClassCode,
        INT32   Index,
        UINT32* pDomainNumber,
        UINT32* pBusNumber,
        UINT32* pDeviceNumber,
        UINT32* pFunctionNumber
    )
    {
        RC rc;
        rc = Platform::FindPciClassCode(ClassCode, Index, pDomainNumber,
                                        pBusNumber, pDeviceNumber, pFunctionNumber);
        if (rc != OK)
        {
            ErrPrintf("UtilityManager::FindPciClassCode: read domain, bus, device, function fail, current domain = %d, bus = %d, device = %d, function = %d\n",
                            *pDomainNumber, *pBusNumber, *pDeviceNumber, *pFunctionNumber);
            return 1;
        }
        return 0;
    }

    size_t GetVirtualMemoryUsed()
    {
        // handled in plgnsprt.cpp
        return 0;
    }

    INT32 Printf(const char *Format, ...)
    {
        // handled in plgnsprt.cpp
        return 0;
    }

    const char *GetOSString()
    {
        // handled in plgnsprt.cpp
        return 0;
    }

    bool HookIRQ(UINT32 irq, ISR handler, void* cookie)
    {
        if (GpuPtr()->GetPollInterrupts())
        {
            ErrPrintf("UtilityManager::HookIRQ: HookIRQ via plugin is "
                "unsupported when PollInterrupts is set!\n");
            MASSERT(0);
            return false;
        }

        // insert irq, handler, and cookie into recording container
        s_hookedIRQs.insert(std::make_pair(irq, IsrData(handler, cookie)));

        UINT08 irqType = MDiagUtils::GetIRQType();
        if (irqType == MODS_XP_IRQ_TYPE_INT)
        {
            return OK == Platform::HookIRQ(irq, handler, cookie);
        }
        else
        {
            IrqParams irqInfo;
            GpuSubdevice* pSubDev = m_pHost->getT3dTest()->GetBoundGpuDevice()->GetSubdevice(0);
            pSubDev->LoadIrqInfo(&irqInfo, irqType, 0, 0);
            irqInfo.irqNumber = irq;

            return OK == Platform::HookInt(irqInfo, handler, cookie);
        }
    }

    bool UnhookIRQ(UINT32 irq, ISR handler, void* cookie)
    {
        // find and remove irq, handler, and cookie from recording container
        s_hookedIRQs.erase(std::make_pair(irq, IsrData(handler, cookie)));

        UINT08 irqType = MDiagUtils::GetIRQType();
        if (irqType == MODS_XP_IRQ_TYPE_INT)
        {
            return OK == Platform::UnhookIRQ(irq, handler, cookie);
        }
        else
        {
            IrqParams irqInfo;
            GpuSubdevice* pSubDev = m_pHost->getT3dTest()->GetBoundGpuDevice()->GetSubdevice(0);
            pSubDev->LoadIrqInfo(&irqInfo, irqType, 0, 0);
            irqInfo.irqNumber = irq;

            return OK == Platform::UnhookInt(irqInfo, handler, cookie);
        }
    }

    bool DisableIRQ(UINT32 irq)
    {
        return OK == Platform::DisableIRQ(irq);
    }

    bool EnableIRQ(UINT32 irq)
    {
        return OK == Platform::EnableIRQ(irq);
    }

    bool IsIRQHooked(UINT32 irq)
    {
        return Platform::IsIRQHooked(irq);
    }

    void ModsAssertFail(const char *file, int line,
        const char *function, const char *cond)
    {
        // handled in plgnsprt.cpp
    }

    bool Poll(PollFunc Function, volatile void* pArgs, FLOAT64 TimeoutMs)
    {
        bool success = true;
        MASSERT(Function);
        UINT64 Start_uSec = Platform::GetTimeUS();

        while (!Function(const_cast<void*>(pArgs)))
        {
            UINT64 Lwr_uSec = Platform::GetTimeUS();
            if ((Lwr_uSec > Start_uSec) && (Lwr_uSec - Start_uSec > TimeoutMs*1000))
            {
                success = false;
                break;
            }

            if (Tasker::GetMaxHwPollHz() > 0.0)
            {
                Tasker::PollHwSleep();
            }
            else
            {
                Tasker::Yield();
            }
        }

        // "Tie" goes to the runner ...
        if (!success && Function(const_cast<void*>(pArgs)))
        {
            success = true;
        }

        return success;
    }

    void DelayNS(UINT32 nanoseconds) const
    {
        // handled in plgnsprt.cpp
    }

    INT32 VAPrintf(const char *Format, va_list Args)
    {
        // handled in plgnsprt.cpp
        return 0;
    }

    UINT32 GetThreadId()
    {
        // handled in plgnsprt.cpp
        return 0;
    }

    void DisableInterrupts()
    {
        Platform::DisableInterrupts();
    }

    void EnableInterrupts()
    {
        Platform::EnableInterrupts();
    }

    bool RegisterUnmapEvent()
    {
        return false; // already handled
    }

    bool RegisterMapEvent()
    {
        return false; //already handled
    }

    const vector<const char*>& GetChipArgs()
    {
        return Platform::GetChipLibArgV();
    }
    
    // This API is for LWLINK shutdown test, check Bug 3313439 for details.
    UINT32 TrainIntraNodeConnsParallel(UINT32 trainTo)
    {
    #if defined(INCLUDE_LWLINK)
        RC rc;
        lwlink_get_devices_info getInfo = {};
        lwlink_device_get_intranode_conns getIntraNode = {};
        lwlink_train_intranode_conns_parallel trainConnsParallel = {};
        if (!LwLinkDevIf::GetLwLinkLibIf())
        {
            ErrPrintf("Lwlink object uninitialized!\n");
            return 1;
        }
        
        rc = LwLinkDevIf::GetLwLinkLibIf()->Control(CTRL_LWLINK_GET_DEVICES_INFO,
                                                        &getInfo,
                                                        sizeof(getInfo));
        if (rc != OK)
        {
            ErrPrintf("Get Device Info failed!\n");
            return 1;
        }
        const UINT16 defaultLwLinkDeviceGetIntraNodeConnsNodeId = 0x0;
        getIntraNode.devInfo.pciInfo = getInfo.devInfo[0].pciInfo;
        getIntraNode.devInfo.nodeId = defaultLwLinkDeviceGetIntraNodeConnsNodeId;
        rc = LwLinkDevIf::GetLwLinkLibIf()->Control(CTRL_LWLINK_DEVICE_GET_INTRANODE_CONNS,
                                                        &getIntraNode,
                                                        sizeof(getIntraNode));
        if (rc != OK)
        {
            ErrPrintf("Get Intranode connection failed!\n");
            return 1;
        }
        DebugPrintf("Got %d for GET_INTRANODE_CONNS numConnections\n", getIntraNode.numConnections);
        for (UINT32 i = 0; i < getIntraNode.numConnections; i++)
        {
            trainConnsParallel.endPointPairs[i].src = getIntraNode.conn[i].srcEndPoint;
            trainConnsParallel.endPointPairs[i].dst = getIntraNode.conn[i].dstEndPoint;
        }
        trainConnsParallel.endPointPairsCount = getIntraNode.numConnections;
        trainConnsParallel.trainTo = trainTo;
        rc = LwLinkDevIf::GetLwLinkLibIf()->Control(CTRL_LWLINK_TRAIN_INTRANODE_CONNS_PARALLEL,
                                                        &trainConnsParallel,
                                                        sizeof(trainConnsParallel));
        if (rc != OK)
        {
            ErrPrintf("Device Link Init Status failed!\n");
            return 1;
        }
        return 0;
    #endif
        ErrPrintf("TrainIntraNodeConnsParallel called while MODS is not built with INCLUDE_LWLINK flag.\n");
        MASSERT(0);
        return 1;
    }

private:

    HostMods * m_pHost;
    vector<ModsEvent*> m_modsEvents;
    // the EventHook struct is copied from GpuSubdevice::HookResmanEvent()
    // the hResmanEvent member will be used when we add general RM event
    // handling capability
    struct EventHook
    {
        UINT32 hResmanEvent;
        EventThread *pEventThread;
    };
    map<UINT32, EventHook> m_eventHooks;

    static std::set< std::pair<UINT32, IsrData> > s_hookedIRQs;
};

std::set< std::pair<UINT32, IsrData> > UtilityManagerMods_impl::s_hookedIRQs;

UtilityManagerMods * UtilityManagerMods::Create( HostMods * pHost )
{
    UtilityManagerMods_impl * pMem = new UtilityManagerMods_impl( pHost );
    return pMem;
}

const map<string,string> & Resource::getProperties( void ) const
{
    // plugin side function only
    //
    static map<string,string> dummyMap;
    return dummyMap;
}

bool Resource::getProperty( const string & propName, string & propData ) const
{
    // plugin side function only
    //

    return false;
}

bool Resource::getProperty( const string & propName, UINT32 & propData ) const
{
    // plugin side function only
    //

    return false;
}

const string & Resource::getName( void ) const
{
    // plugin side function only
    //
    static string dummy;
    return dummy;
}

void Resource::printResource( void * pStream ) const
{
    // plugin side function only
    //

}

// Utility function to get FBDecode object.
// Used by both Buffer and Surface classes.
void GetFbDecode(FbDecode &decode, const MdiagSurf *pSurf, UINT64 physAddr)
{
    UINT32 pteKind = UINT32(pSurf->GetPteKind());
    // First use the light way to get page size.
    UINT32 pageSize = pSurf->GetPageSize();
    // Then the heavy way.
    if (0 == pageSize)
    {
        pageSize = pSurf->GetActualPageSizeKB();
    }
    // We cannot decode address under 0 page size.
    MASSERT(pageSize != 0 && pageSize != ~0U);

    pSurf->GetGpuSubdev(0)->GetFB()->DecodeAddress(&decode,
                                                   physAddr,
                                                   pteKind,
                                                   pageSize);
}

void GetRBCAddress(EccAddrParams &decode, const MdiagSurf *pSurf, UINT64 physAddr,UINT32 errCount, UINT32 errPos)
{
    UINT32 pteKind = UINT32(pSurf->GetPteKind());
    UINT32 pageSize = pSurf->GetPageSize();
    if (0 == pageSize)
    {
        pageSize = pSurf->GetActualPageSizeKB();
    }
    // We cannot decode address under 0 page size.
    MASSERT(pageSize != 0 && pageSize != ~0U);

    pSurf->GetGpuSubdev(0)->GetFB()->GetRBCAddress(&decode,
                                                   physAddr,
                                                   pteKind,
                                                   pageSize,
                                                   errCount,
                                                   errPos);
}

void GetEccInjectRegVal(EccErrInjectParams &injParams, const MdiagSurf *pSurf, UINT64 physAddr,UINT32 errCount, UINT32 errPos)
{
    UINT32 pteKind = UINT32(pSurf->GetPteKind());
    UINT32 pageSize = pSurf->GetPageSize();
    if (0 == pageSize)
    {
        pageSize = pSurf->GetActualPageSizeKB();
    }
    // We cannot decode address under 0 page size.
    MASSERT(pageSize != 0 && pageSize != ~0U);

    pSurf->GetGpuSubdev(0)->GetFB()->GetEccInjectRegVal(&injParams,
                                                        physAddr,
                                                        pteKind,
                                                        pageSize,
                                                        errCount,
                                                        errPos);
}

LWGpuChannel* GetLWGpuChannel(HostMods* m_pHost, void* pTraceChannel)
{
    LWGpuChannel* pLwgpuChannel = NULL;

    if (pTraceChannel == NULL)
    {
        // If no channel is given, we grab the first one
        // from the channel list held by Trace3DTest.
        Trace3DTest * pT3dTest = m_pHost->getT3dTest();
        GpuTrace::TraceChannelObjects* pTraceChannels;
        pTraceChannels = pT3dTest->GetTrace()->GetChannels();

        if (pTraceChannels->size() == 0)
        {
            MASSERT(!"SOC methods need a channel.");
        }

        pLwgpuChannel = pTraceChannels->begin()->GetCh();
    }
    else
    {
        pLwgpuChannel = ((TraceChannel*)pTraceChannel)->GetCh();
    }
    MASSERT(pLwgpuChannel != NULL);

    return pLwgpuChannel;
}

class SyncPointMods_impl : public SyncPointMods
{
public:

    friend SyncPointMods * SyncPointMods::Create(HostMods* pHost,
                                                 ModsSyncPoint* pHostSyncPoint);

    explicit SyncPointMods_impl(HostMods* pHost, ModsSyncPoint* pHostSyncPoint)
        : m_pHost(pHost), m_pHostSyncPoint(pHostSyncPoint)
    {}

    virtual ~SyncPointMods_impl()
    {
        if (m_pHostSyncPoint != NULL)
        {
            m_pHostSyncPoint->Free();
            delete m_pHostSyncPoint;
        }
    }

    UINT32 GetIndex() const
    {
       return  m_pHostSyncPoint->GetIndex();
    }

    void Wait(UINT32 threshold, bool base, UINT32 baseIndex, bool _switch,
              void * pTraceChannel, int subchannel)
    {
        MASSERT(m_pHostSyncPoint != NULL);

        // Write SYNCPOINTB
        UINT32 payload = 0x0;

        // SyncPoint Index
        UINT32 syncPointIndex = GetIndex();
        LWGpuChannel* pLwgpuChannel = GetLWGpuChannel(m_pHost, pTraceChannel);

        switch (pLwgpuChannel->GetChannelClass())
        {
            case KEPLER_CHANNEL_GPFIFO_C:
            case MAXWELL_CHANNEL_GPFIFO_A:
                pLwgpuChannel->MethodWrite(subchannel, LWA26F_SYNCPOINTA, threshold);

                // Operation: Wait
                payload = DRF_DEF(A26F, _SYNCPOINTB, _OPERATION, _WAIT);

                if (_switch)
                    payload |= DRF_DEF(A26F, _SYNCPOINTB, _WAIT_SWITCH, _EN);

                if (base)
                {
                    payload |= DRF_DEF(A26F, _SYNCPOINTB, _BASE, _EN);
                    payload |= (baseIndex << DRF_SHIFT(LWA26F_SYNCPOINTB_BASE_INDEX));
                }

                payload |= (syncPointIndex << DRF_SHIFT(LWA26F_SYNCPOINTB_SYNCPT_INDEX));

                pLwgpuChannel->MethodWrite(subchannel, LWA26F_SYNCPOINTB, payload);
                break;

            case PASCAL_CHANNEL_GPFIFO_A:
            case VOLTA_CHANNEL_GPFIFO_A:
            case TURING_CHANNEL_GPFIFO_A:
            case AMPERE_CHANNEL_GPFIFO_A:
            case HOPPER_CHANNEL_GPFIFO_A:
                pLwgpuChannel->MethodWrite(subchannel, LWC06F_SYNCPOINTA, threshold);

                // Operation: Wait
                payload = DRF_DEF(C06F, _SYNCPOINTB, _OPERATION, _WAIT);

                if (_switch)
                    payload |= DRF_DEF(C06F, _SYNCPOINTB, _WAIT_SWITCH, _EN);

                if (base)
                {
                    WarnPrintf("PASCAL doesn't support syncpoint base !\n");
                }

                payload |= (syncPointIndex << DRF_SHIFT(LWC06F_SYNCPOINTB_SYNCPT_INDEX));

                pLwgpuChannel->MethodWrite(subchannel, LWC06F_SYNCPOINTB, payload);
                break;
            default:
                MASSERT(!"unsupported channel class");
        }
    }

    void HostIncrement(bool wfi, bool flush, void * pTraceChannel, int subchannel)
    {
        // Write SYNCPOINTB
        UINT32 payload = 0x0;
        // SyncPoint Index
        UINT32 syncPointIndex = GetIndex();

        LWGpuChannel* pLwgpuChannel = GetLWGpuChannel(m_pHost, pTraceChannel);

        switch (pLwgpuChannel->GetChannelClass())
        {
            case KEPLER_CHANNEL_GPFIFO_C:
            case MAXWELL_CHANNEL_GPFIFO_A:
                if (wfi)
                {
                    pLwgpuChannel->MethodWrite(subchannel, LWA26F_WFI, 0);
                }

                if (flush)
                {
                    pLwgpuChannel->MethodWrite(subchannel, LWA26F_FB_FLUSH, 0);
                    UINT32 flushData = DRF_DEF(A26F, _MEM_OP_B, _OPERATION, _L2_FLUSH_DIRTY);
                    pLwgpuChannel->MethodWrite(subchannel, LWA26F_MEM_OP_B, flushData);
                }

                // Operation: Increment
                payload = DRF_DEF(A26F, _SYNCPOINTB, _OPERATION, _INCR);

                payload |= (syncPointIndex << DRF_SHIFT(LWA26F_SYNCPOINTB_SYNCPT_INDEX));

                pLwgpuChannel->MethodWrite(subchannel, LWA26F_SYNCPOINTB, payload);
                break;

            case PASCAL_CHANNEL_GPFIFO_A:
            case VOLTA_CHANNEL_GPFIFO_A:
            case TURING_CHANNEL_GPFIFO_A:
            case AMPERE_CHANNEL_GPFIFO_A:
            case HOPPER_CHANNEL_GPFIFO_A:
                if (wfi)
                {
                    pLwgpuChannel->MethodWrite(subchannel, LWC06F_WFI, 0);
                }

                if (flush)
                {
                    pLwgpuChannel->MethodWrite(subchannel, LWC06F_FB_FLUSH, 0);
                    UINT32 flushData = DRF_DEF(C06F, _MEM_OP_D, _OPERATION, _L2_FLUSH_DIRTY);
                    pLwgpuChannel->MethodWrite(subchannel, LWC06F_MEM_OP_D, flushData);
                }
                // Operation: Increment
                payload = DRF_DEF(C06F, _SYNCPOINTB, _OPERATION, _INCR);

                payload |= (syncPointIndex << DRF_SHIFT(LWC06F_SYNCPOINTB_SYNCPT_INDEX));

                pLwgpuChannel->MethodWrite(subchannel, LWC06F_SYNCPOINTB, payload);
                break;
            default:
                MASSERT(!"unsupported channel class");
        }
    }

    void GraphicsIncrement(bool wfi, bool flush, void * pTraceChannel, int subchannel)
    {
        // TBD
    }

    HostMods*       m_pHost;
    ModsSyncPoint*  m_pHostSyncPoint;
};

SyncPointMods * SyncPointMods::Create(HostMods* pHost, ModsSyncPoint* pHostSyncPoint)
{
    MASSERT (pHostSyncPoint != NULL);
    return new SyncPointMods_impl(pHost, pHostSyncPoint);
}

class SyncPointBaseMods_impl : public SyncPointBaseMods
{
public:

    friend SyncPointBaseMods * SyncPointBaseMods::Create(HostMods* pHost,
                                                 ModsSyncPointBase* pHostSyncPointBase);

    explicit SyncPointBaseMods_impl(HostMods* pHost, ModsSyncPointBase* pHostSyncPointBase)
        : m_pHost(pHost), m_pHostSyncPointBase(pHostSyncPointBase)
    {}

    virtual ~SyncPointBaseMods_impl()
    {
        if (m_pHostSyncPointBase != NULL)
        {
            m_pHostSyncPointBase->Free();
            delete m_pHostSyncPointBase;
        }
    }

    UINT32 GetIndex() const
    {
       return  m_pHostSyncPointBase->GetIndex();
    }

    UINT32 GetValue() const
    {
       MASSERT(m_pHostSyncPointBase!=NULL);
       UINT32 r;
       m_pHostSyncPointBase->GetValue(&r);
       return r;
    }

    void AddValue( UINT32 value,
                   bool wfi = true,
                   bool flush = true,
                   void * pTraceChannel = 0,
                   int subchannel = 0)
    {
        LWGpuChannel* pLwgpuChannel = GetLWGpuChannel(m_pHost, pTraceChannel);

        UINT32 baseIndex=GetIndex();

        pLwgpuChannel->MethodWrite(subchannel, LWA26F_SYNCPOINTA, value);

        if (flush)
        {
            pLwgpuChannel->MethodWrite(subchannel, LWA26F_FB_FLUSH, 0);
            UINT32 flushData = DRF_DEF(A26F, _MEM_OP_B, _OPERATION, _L2_FLUSH_DIRTY);
            pLwgpuChannel->MethodWrite(subchannel, LWA26F_MEM_OP_B, flushData);
        }

        if (wfi)
        {
            pLwgpuChannel->MethodWrite(subchannel, LWA26F_WFI, 0);
        }

        // Write SYNCPOINTB
        UINT32 payload = 0x0;

        // Operation: Base_Add
        payload = DRF_DEF(A26F, _SYNCPOINTB, _OPERATION, _BASE_ADD);

        // SyncPoint Index
        payload |= (baseIndex << DRF_SHIFT(LWA26F_SYNCPOINTB_BASE_INDEX));

        pLwgpuChannel->MethodWrite(subchannel, LWA26F_SYNCPOINTB, payload);
    }

    void SetValue( UINT32 value,
                  bool wfi = true,
                  bool flush = true,
                  void * pTraceChannel = 0,
                  int subchannel = 0)
    {
        LWGpuChannel* pLwgpuChannel = GetLWGpuChannel(m_pHost, pTraceChannel);

        pLwgpuChannel->MethodWrite(subchannel, LWA26F_SYNCPOINTA, value);

        UINT32 baseIndex=GetIndex();

        if (flush)
        {
            pLwgpuChannel->MethodWrite(subchannel, LWA26F_FB_FLUSH, 0);
            UINT32 flushData = DRF_DEF(A26F, _MEM_OP_B, _OPERATION, _L2_FLUSH_DIRTY);
            pLwgpuChannel->MethodWrite(subchannel, LWA26F_MEM_OP_B, flushData);
        }

        if (wfi)
        {
            pLwgpuChannel->MethodWrite(subchannel, LWA26F_WFI, 0);
        }

        // Write SYNCPOINTB
        UINT32 payload = 0x0;

        // Operation: Base_Write
        payload = DRF_DEF(A26F, _SYNCPOINTB, _OPERATION, _BASE_WRITE);

        // SyncPoint Index
        payload |= (baseIndex << DRF_SHIFT(LWA26F_SYNCPOINTB_BASE_INDEX));

        pLwgpuChannel->MethodWrite(subchannel, LWA26F_SYNCPOINTB, payload);
    }

    void CpuAddValue( UINT32 value)
    {
        CpuSetValue(value+GetValue());
    }

    void CpuSetValue( UINT32 value)
    {
       MASSERT(m_pHostSyncPointBase!=NULL);
       m_pHostSyncPointBase->SetValue(value);
    }

    HostMods*       m_pHost;
    ModsSyncPointBase*  m_pHostSyncPointBase;
};

SyncPointBaseMods * SyncPointBaseMods::Create(HostMods* pHost, ModsSyncPointBase* pHostSyncPointBase)
{
    MASSERT (pHostSyncPointBase != NULL);
    return new SyncPointBaseMods_impl(pHost, pHostSyncPointBase);
}

class SOCMods_impl : public SOCMods
{
public:

    friend SOCMods * SOCMods::Create( HostMods * pHost );

    explicit SOCMods_impl( HostMods * pHost ) : m_pHost( pHost )
    {
        m_pMem = m_pHost->getMem();
    }

    virtual ~SOCMods_impl()
    {
        deleteSyncPoints();
        deleteSyncPointBases();
    }

// FIXME:
// Temporary: RM support for reading/writing host1x registers is not
// ready yet. Therefore, we take this hacky solution by read/writing
// the corresponding physical memory.

    UINT08 RegRd08(UINT32 offset)
    {
        return m_pMem->PhysRd08(offset); // HACK: RegRd is same as PhysRd
    }

    void RegWr08(UINT32 offset, UINT08 data)
    {
        m_pMem->PhysWr08(offset, data);  // HACK: RegWr is same as PhysWr
    }

    UINT16 RegRd16(UINT32 offset)
    {
        return m_pMem->PhysRd16(offset); // HACK: RegRd is same as PhysRd
    }

    void RegWr16(UINT32 offset, UINT16 data)
    {
        m_pMem->PhysWr16(offset, data);  // HACK: RegWr is same as PhysWr
    }

    UINT32 RegRd32(UINT32 offset)
    {
        return m_pMem->PhysRd32(offset); // HACK: RegRd is same as PhysRd
    }

    void RegWr32(UINT32 offset, UINT32 data)
    {
        m_pMem->PhysWr32(offset, data);  // HACK: RegWr is same as PhysWr
    }

    SOC::SyncPoint* AllocSyncPoint( UINT32 sub = 0)
    {
        GpuDevice* pDev = m_pHost->getT3dTest()->GetBoundGpuDevice();

        // Allocate a MODS SyncPoint
        ModsSyncPoint* pHostSyncPoint = new ModsSyncPoint();

        if (OK != pHostSyncPoint->Alloc(pDev))
        {
            delete pHostSyncPoint;
            return NULL;
        }

        // Store the MODS SyncPoint in our T3d SyncPoint
        SyncPointMods* pSyncPoint = SyncPointMods::Create(m_pHost, pHostSyncPoint);

        // Keep a list of SyncPoints for cleanup
        m_syncPoints.push_back(pSyncPoint);

        return pSyncPoint;
    }

    SOC::SyncPointBase* AllocSyncPointBase
    (
        UINT32 initialValue = 0,
        bool forceIndex = false,
        UINT32 index = 0
    )
    {
        GpuDevice* pDev = m_pHost->getT3dTest()->GetBoundGpuDevice();
        MASSERT((pDev->GetFamily() == GpuDevice::Kepler || pDev->GetFamily() == GpuDevice::Maxwell)
                && "SyncPointBase is unsupported on this GPU");

        // Allocate a MODS SyncPointBase
        ModsSyncPointBase* pHostSyncPointBase = new ModsSyncPointBase();

        //Set initial value
        pHostSyncPointBase->SetValue(initialValue);

        //Set index if forceIndex
        if(forceIndex)
        {
            pHostSyncPointBase->ForceIndex(index);
        }

        if (OK != pHostSyncPointBase->Alloc(pDev))
        {
            delete pHostSyncPointBase;
            return NULL;
        }

        // Store the MODS SyncPointBase in our T3d SyncPointBase
        SyncPointBaseMods* pSyncPointBase = SyncPointBaseMods::Create(m_pHost, pHostSyncPointBase);

        // Keep a list of SyncPointBases for cleanup
        m_syncPointBases.push_back(pSyncPointBase);

        return pSyncPointBase;
    }

    // move the implementation to SOC_IsSmmuEnabled in plgnsprt.cpp
    bool IsSmmuEnabled()
    {
        return true;
    }

private:
    void deleteSyncPoints()
    {
        vector<SyncPointMods*>::iterator it  = m_syncPoints.begin();
        vector<SyncPointMods*>::iterator end = m_syncPoints.end();
        for (; it != end; ++it)
        {
            delete (*it);
        }
        m_syncPoints.clear();
    }

    void deleteSyncPointBases()
    {
        vector<SyncPointBaseMods*>::iterator it  = m_syncPointBases.begin();
        vector<SyncPointBaseMods*>::iterator end = m_syncPointBases.end();
        for (; it != end; ++it)
        {
            delete (*it);
        }
        m_syncPointBases.clear();
    }

private:
    HostMods * m_pHost;
    Mem*       m_pMem;
    vector<SyncPointMods*> m_syncPoints;
    vector<SyncPointBaseMods*> m_syncPointBases;
};

SOCMods * SOCMods::Create( HostMods * pHost )
{
    return new SOCMods_impl(pHost);
}

class SemaphoreMods_impl : public SemaphoreMods
{
public:

    friend SemaphoreMods * SemaphoreMods::Create(HostMods* pHost, UINT64 physAddr, SemaphorePayloadSize size, bool isSyncptShimSema);

    explicit SemaphoreMods_impl(HostMods* pHost, UINT64 physAddr, SemaphorePayloadSize size, bool isSyncptShimSema = false)
        : m_pHost(pHost), m_CpuPhysAddr(physAddr), m_IsSyncptShimSema(isSyncptShimSema), m_SemaphorePayloadSize(size)
    {
        /*
         * surface allocation is delayed to acquire/release.
         *
         * The reason is we doesn't know the right va space name at this point. It's a bug when using -address_space_name <va_name>
         * And different channels may have different va space names but use the same semaphore,
         * so the key element is channel. Because we can get channel info in acquire/release.
         * which means we can also get accurate va space name, we decided to delay surface allocation
         * to acquire/release.
         */

         m_ReleaseFlags = Channel::FlagSemRelDefault;
         if (m_IsSyncptShimSema)
         {
             m_ReleaseFlags &= ~(Channel::FlagSemRelWithTime);
         }
    }

    virtual ~SemaphoreMods_impl()
    {
        for(auto pSurf : m_pSurfs)
        {
            delete pSurf;
        }
    }

    void  Acquire(void * pTraceChannel, UINT64 data)
    {
        LWGpuChannel* pLwgpuChannel = GetLWGpuChannel(m_pHost, pTraceChannel);
        UINT64 gpuVirtAddr = GetVirtSurfByChannel(pLwgpuChannel)->GetCtxDmaOffsetGpu();
        RC rc;
        rc = pLwgpuChannel->SetSemaphoreOffsetRC(gpuVirtAddr);
        if (OK != rc)
        {
            ErrPrintf("SetSemaphoreOffsetRC failed: %s\n", rc.Message());
            MASSERT(0);
        }
        rc = pLwgpuChannel->SemaphoreAcquireRC(data);
        if (OK != rc)
        {
            ErrPrintf("SemaphoreAcquireRC failed: %s\n", rc.Message());
            MASSERT(0);
        }
    }

    void  AcquireGE(void * pTraceChannel, UINT64 data)
    {
        LWGpuChannel* pLwgpuChannel = GetLWGpuChannel(m_pHost, pTraceChannel);
        UINT64 gpuVirtAddr = GetVirtSurfByChannel(pLwgpuChannel)->GetCtxDmaOffsetGpu();
        RC rc;
        rc = pLwgpuChannel->SetSemaphoreOffsetRC(gpuVirtAddr);
        if (OK != rc)
        {
            ErrPrintf("SetSemaphoreOffsetRC failed: %s\n", rc.Message());
            MASSERT(0);
        }
        rc = pLwgpuChannel->SemaphoreAcquireGERC(data);
        if (OK != rc)
        {
            ErrPrintf("SemaphoreAcquireGERC failed: %s\n", rc.Message());
            MASSERT(0);
        }
    }

    void  Release(void * pTraceChannel, UINT64 data, bool flush = false)
    {
        LWGpuChannel* pLwgpuChannel = GetLWGpuChannel(m_pHost, pTraceChannel);
        UINT64 gpuVirtAddr = GetVirtSurfByChannel(pLwgpuChannel)->GetCtxDmaOffsetGpu();

        StickyRC src;
        src = pLwgpuChannel->SetSemaphoreOffsetRC(gpuVirtAddr);
        pLwgpuChannel->SetSemaphoreReleaseFlags(m_ReleaseFlags);
        InfoPrintf("%s: Semaphore CPA:0x%llx, release flags: %u\n", __FUNCTION__, m_CpuPhysAddr, m_ReleaseFlags);

        UINT64 mthdNum = 1;
        if (m_IsSyncptShimSema)
        {
            mthdNum = data;
            InfoPrintf("%s: Mods inserts %u release methods.\n", __FUNCTION__, mthdNum);
        }
        //syncpt value is increased by 1 each time, so need below loop.
        for(UINT64 i = 0; i < mthdNum; ++i)
        {
            src = pLwgpuChannel->SemaphoreReleaseRC(data);
        }

        //Insert "l2 flush" method if specified
        if (flush)
        {
            switch (pLwgpuChannel->GetChannelClass())
            {
                case PASCAL_CHANNEL_GPFIFO_A:
                case VOLTA_CHANNEL_GPFIFO_A:
                case TURING_CHANNEL_GPFIFO_A:
                case AMPERE_CHANNEL_GPFIFO_A:
                case HOPPER_CHANNEL_GPFIFO_A:
                {
                    int subchannel = 0;
                    if (OK != pLwgpuChannel->MethodWriteRC(subchannel, LWC06F_FB_FLUSH, 0))
                    {
                        ErrPrintf("SemaphoreMods::Release : Mods fail to send method LWC06F_FB_FLUSH.\n");
                        MASSERT(0);
                    }
                    UINT32 flushData = DRF_DEF(C06F, _MEM_OP_D, _OPERATION, _L2_FLUSH_DIRTY);
                    if (OK != pLwgpuChannel->MethodWriteRC(subchannel, LWC06F_MEM_OP_D, flushData))
                    {
                        ErrPrintf("SemaphoreMods::Release : Mods fail to send method LWC06F_MEM_OP_D.\n");
                        MASSERT(0);
                    }
                    break;
                }
                default:
                    MASSERT(!"unsupported channel class");
            }
        }

        if (OK != src)
        {
            ErrPrintf("SemaphoreMods::Release failed: %s\n", src.Message());
            MASSERT(0);
        }
    }

    UINT64 GetPhysAddress()
    {
        return m_CpuPhysAddr;
    }

    void SetReleaseFlags(void * pTraceChannel, UINT32 flags)
    {
        if (m_IsSyncptShimSema)
        {
            if ((flags & Channel::FlagSemRelWithTime) == Channel::FlagSemRelWithTime)
            {
                m_pHost->getT3dTest()->FailTest(Test::TEST_FAILED);
                MASSERT(!"Timestamp flag is not valid for syncptshim semaphore"
                          "because it increase syncpt value with 2*data!\n");
            }
        }

        m_ReleaseFlags = flags;
        InfoPrintf("%s: Semaphore release flags: %u\n", __FUNCTION__, flags);
    }

private:
    MdiagSurf* GetVirtSurfByChannel(LWGpuChannel* pLwgpuChannel)
    {
        UINT32 vasHandle = pLwgpuChannel->GetVASpaceHandle();

        MdiagSurf* pSurf = NULL;
        auto result = std::find_if(m_pSurfs.begin(), m_pSurfs.end(),
                        [vasHandle](const MdiagSurf* surf){return surf->GetGpuVASpace() == vasHandle;});

        if(result == m_pSurfs.end())
        {
            // alloc surface and update m_pSurfs
            pSurf = AllocSemaphoreSurface(pLwgpuChannel);
            InfoPrintf("%s: Semaphore CPU PA: 0x%llx, GPU VA: 0x%llx\n", __FILE__, m_CpuPhysAddr, pSurf->GetCtxDmaOffsetGpu());
        }
        else
        {
            pSurf = *(result);
        }

        return pSurf;
    }
    MdiagSurf* AllocSemaphoreSurface(LWGpuChannel* pLwgpuChannel)
    {
        GpuDevice* pDev = m_pHost->getT3dTest()->GetBoundGpuDevice();
        LwRm* pLwRm = m_pHost->getT3dTest()->GetLwRmPtr();

        UINT32 vasHandle = pLwgpuChannel->GetVASpaceHandle();
        DebugPrintf("%s@%d, allocating surface for semaphore by va space handle 0x%x\n", __FUNCTION__, __LINE__, vasHandle);

        MdiagSurf* pSurf = new MdiagSurf();

        if(m_CpuPhysAddr > 0)
        {
            pSurf->SetFixedPhysAddr(m_CpuPhysAddr);
        }

        pSurf->SetLocation(Memory::Coherent);
        pSurf->SetArrayPitch(m_SemaphorePayloadSize);
        pSurf->SetColorFormat(ColorUtils::Y8);
        pSurf->SetLayout(Surface2D::Pitch);
        pSurf->SetGpuVASpace(vasHandle);

        if (m_IsSyncptShimSema && m_SemaphorePayloadSize == SEM_PAYLOAD_SIZE_4KBYTE)
        {
            DebugPrintf("Set both surf alignment and page size as 4KB for syncptshim semaphore.\n");
            pSurf->SetAlignment(4_KB);
            pSurf->SetPageSize(4);
        }

        RC retVal = pSurf->Alloc(pDev, pLwRm);
        if(retVal != OK)
            ErrPrintf( "Mods failed to allocate surface at addr 0x%llx in function %s!,rc:%d\n", m_CpuPhysAddr, __FUNCTION__,retVal.Get());

        if(!m_CpuPhysAddr)
            pSurf->GetPhysAddress(0, &m_CpuPhysAddr, 0);

        m_pSurfs.push_back(pSurf);
        return m_pSurfs.back();
    }

    HostMods*               m_pHost;
    vector<MdiagSurf*>      m_pSurfs;
    UINT64                  m_CpuPhysAddr;
    bool                    m_IsSyncptShimSema;
    SemaphorePayloadSize    m_SemaphorePayloadSize;
    UINT32                  m_ReleaseFlags;
};

SemaphoreMods * SemaphoreMods::Create(HostMods* pHost, UINT64 physAddr, SemaphorePayloadSize size, bool isSyncptShimSema)
{
    return new SemaphoreMods_impl(pHost, physAddr, size, isSyncptShimSema);
}

} // namespace T3dPlugin
