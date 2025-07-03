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

// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h"
// first.

#include "mdiag/tests/stdtest.h"

#include "trace_3d.h"
#include "traceop.h"
#include "t3plugin.h"            // trace_3d plugin interface
#include "core/include/xp.h"
#include "gpu/utility/surfrdwr.h"
#include "core/include/channel.h"

// for DMA surface reader
#include "mdiag/resource/lwgpu/dmasurfr.h"
#include "gpu/utility/surfrdwr.h"
#include "mdiag/resource/lwgpu/surfasm.h"
#include "mdiag/resource/lwgpu/verif/GpuVerif.h"

// for va_list stuff
#include <cstdarg>

#include "mdiag/utils/tex.h"

#include "api_receiver.h"
#include "mailbox.h"

#include "ArmVAPrintf.h"

#include "mdiag/advschd/pmevent.h"
#include "mdiag/advschd/policymn.h"
#include "mdiag/advschd/pmevntmn.h"

#include "core/include/registry.h"
#include "lwRmReg.h"
#include "mdiag/utl/utl.h"

// this file contains the definition of member functions from core trace_3d
// classes that need knowledge of the plugin interface in order to work.   By
// putting such functions here  in one place, the plugin interface
// remains separate and does not invade core trace_3d source code or headers.

// member function Trace3DTest::LoadPlugin
//
// inputs: char * to string with path to plugin and plugin parameters
//             e.g., "/path/to/myplugin.so -printSurfaces"
// returns: RC indicating success or failure
// side effects: initializes the plugin manager object (the HostMods object).
// loads the plugin .so into the
//          mods process, and calls its __Startup() and __Initialize() entry
// points, which in turn may call back into the trace_3d object to register
// events
//
// Description:
//           Trace_3d now supports external plugins.   These are external
// loadable libraries which conform to a certain protocol: they use the
// interface specified in the  file "plugin.h",
// and they implement 3 functions: __Startup(), __Initialize(), and
// __Shutdown().
//
// The Host object holds pointers to "Manager" objects which make internal
// trace_3d functionality available to the plugin.  E.g., the GpuManager
// lets the plugin acquire a Gpu object  (in reality a GpuSubdevice of
// the lwrrently bound GpuDevice in trace_3d), which provides member functions
// like read/write register, read/write memory.  A BufferManager allows the
// plugin to find/examine/create/ modify surfaces in the
// trace.  An EventManager allows the plugin to register interest in certain
// "events" in the trace (e.g., before trace loaded, after trace loaded,
// before CRC check,  before/after a certain method write,
//  and so on).  When the event oclwrs, trace_3d calls a callback function in
// the plugin (set up at the time the plugin registererd interest in the event).
//
// This function creates the trace_3d-side plugin objects, loads the plugin's
// shared library, and calls the plugin's __Startup() and __Initialize()
// functions
//
//

using T3dPlugin::pFnStartup_t;
using T3dPlugin::pFnInitialize_t;
using T3dPlugin::pFnShutdown_t;
using T3dPlugin::Event;
using T3dPlugin::EventMods;
using T3dPlugin::BufferMods;
using T3dPlugin::BufferManager;
using T3dPlugin::MailBox;

namespace T3dPlugin
{
    extern const char *shimFuncNames[];
    extern const pFlwoid shimFuncPtrs[];
    extern ApiReceiver *apiReceivers[];
    void InitApiReceivers();
}

const map<Trace3DTest::TraceEventType, string> Trace3DTest::s_EventTypeMap =
{
    #undef DEFINE_TRACE_EVENT
    #define DEFINE_TRACE_EVENT(type)  { Trace3DTest::TraceEventType::type , #type },
    #include "t3event.h"
};

Trace3DTest::PluginHosts Trace3DTest::s_Hosts;

RC Trace3DTest::LoadPlugin( const char *pluginInfo )
{
    InfoPrintf( "trace_3d: activating plugin: '%s'\n", pluginInfo );

    if ( ! pluginInfo )
    {
        ErrPrintf( "%s: null pluginInfo string\n", __FUNCTION__ );
        return RC::SOFTWARE_ERROR;
    }

    // save the input parameter string into a std::string for easier processing
    //
    string plgnInfo( pluginInfo );
    string pluginPath;
    string pluginArgs;

    // remove leading whitespace from the pluginInfo string
    //
    string::size_type posNonSpace = plgnInfo.find_first_not_of( " \t", 0 );
    if ( posNonSpace == string::npos )
    {
        ErrPrintf( "%s: pluginInfo string contains only whitespace\n",
                   __FUNCTION__ );
        return RC::SOFTWARE_ERROR;
    }

    // if there is leading whitespace, remove it
    //
    plgnInfo.erase( 0, posNonSpace );

    // extract the plugin library name
    // format is "<path to plugin>[<space>+<argument>]*"
    // start by finding the first space character in the string, which should
    // be the space between the path to the plugin and the start of the plugin
    // arguments
    //
    string::size_type posSpace = plgnInfo.find_first_of( " \t", 0 );
    if ( posSpace == string::npos )
    {
        // found no space in the string -- the plugin name takes up the entire
        // string and there are no plugin parameters
        //
        pluginPath = plgnInfo;
    }
    else
    {
        // we found a space character, extract the plugin name.  The plugin
        // parameters are everything left over after removing the plugin name
        //
        pluginPath = plgnInfo.substr( 0, posSpace );

        // erase the plugin name and the first space found from the input string
        // what's left is the argument list (may still have leading whitespace)
        //
        plgnInfo.erase( 0, posSpace + 1 );

        // plgnInfo <--> pluginArgs
        //
        pluginArgs.swap( plgnInfo );
    }

    // mods64 should load 64bit plugin lib(no socket)
    // according the agreement, the name of 64b lib ends with _64
    // mods64 needs to append "_64" to the end of name smartly
    // Need Not append the "_64" to the end of top plugin name
    if ( sizeof(void*) == 8 && (!m_IsTopPlugin) )
    {
        string fileSuffix;
        string::size_type pos = string::npos;

        pos = pluginPath.rfind( '.' );
        if ( pos != string::npos )
        {
            fileSuffix = pluginPath.substr( pos );
            pluginPath.erase( pos );
        }

        // append if no "_64" at the end
        string name64;
        if (pluginPath.size() > 3)
        {
            name64 = pluginPath.substr(pluginPath.size() - 3, 3);
        }

        if ( name64 != "_64" )
        {
            pluginPath.append("_64");
        }

        pluginPath += fileSuffix;
    }
    m_pluginName = pluginPath;

    if (m_IsTopPlugin)
    {
        RC rc = LoadTopPlugin(pluginArgs.c_str());
        if (rc != OK)
        {
            ErrPrintf( "%s: couldn't load top plugin library '%s'\n", __FUNCTION__,
                pluginPath.c_str() );
            return RC::SOFTWARE_ERROR;
        }
    }
    else
    {
        // load the plugin library
        //
        RC rc = Xp::LoadDLL( pluginPath.c_str(), & m_pluginLibraryHandle, Xp::UnloadDLLOnExit );
        if ( rc != OK )
        {
            ErrPrintf( "%s: couldn't load plugin library '%s'\n", __FUNCTION__,
                pluginPath.c_str() );
            return RC::SOFTWARE_ERROR;
        }

        // get all the required entrypoint function symbols
        //
        pFnStartup_t pFnStartup = (pFnStartup_t)
            Xp::GetDLLProc( m_pluginLibraryHandle, "__Startup" );

        if ( pFnStartup == 0 )
        {
            ErrPrintf( "%s: %s missing __Startup() symbol\n", __FUNCTION__,
                pluginPath.c_str() );
            return RC::SOFTWARE_ERROR;
        }

        pFnInitialize_t pFnInitialize = (pFnInitialize_t)
            Xp::GetDLLProc( m_pluginLibraryHandle, "__Initialize" );

        if ( pFnInitialize == 0 )
        {
            ErrPrintf( "%s: %s missing __Initialize() symbol\n", __FUNCTION__,
                pluginPath.c_str() );
            return RC::SOFTWARE_ERROR;
        }

        pFnShutdown_t pFnShutdown = (pFnShutdown_t)
            Xp::GetDLLProc( m_pluginLibraryHandle, "__Shutdown" );

        if ( pFnShutdown == 0 )
        {
            ErrPrintf( "%s: %s missing __Shutdown() symbol\n", __FUNCTION__,
                pluginPath.c_str() );
            return RC::SOFTWARE_ERROR;
        }

        // create the main plugin manager object, which calls the Startup and
        // Initialize functions in the plugin
        //
        m_pPluginHost = T3dPlugin::HostMods::Create( this, pFnShutdown );

        if ( ! m_pPluginHost )
        {
            ErrPrintf( "%s: couldn't create HostMods object\n", __FUNCTION__ );
            return RC::SOFTWARE_ERROR;
        }
        s_Hosts[m_pluginName].refCount++;
        s_Hosts[m_pluginName].hosts.push_back(m_pPluginHost);
        // start up the plugin: give it the Host object it'll use to communicate
        // with trace_3d
        //
        UINT32 pluginMajorVerion = 0;
        UINT32 pluginMinorVersion = 0;

        // need to implement these for shim layer
        //

        int pluginRc = pFnStartup( m_pPluginHost,
            T3dPlugin::majorVersion,
            T3dPlugin::minorVersion,
            &pluginMajorVerion,
            &pluginMinorVersion,
            T3dPlugin::shimFuncNames,
            T3dPlugin::shimFuncPtrs );

        if ( pluginMajorVerion != T3dPlugin::majorVersion )
        {
            ErrPrintf( "%s: major version mismatch (mods=%d, plugin=%d),"
                " mods/plugin are not compatible\n",
                __FUNCTION__, T3dPlugin::majorVersion, pluginMajorVerion );
            return RC::SOFTWARE_ERROR;
        }

        InfoPrintf( "trace_3d: plugin interface versions: trace_3d: major=%d "
            "minor=%d, plugin: major=%d minor=%d\n",
            T3dPlugin::majorVersion, T3dPlugin::minorVersion,
            pluginMajorVerion, pluginMinorVersion );

        if ( pluginRc != 0 )
        {
            ErrPrintf( "%s: plugun's __Startup() returned error\n", __FUNCTION__ );
            return RC::SOFTWARE_ERROR;
        }

        // initialize the plugin with its arguments
        //
        pluginRc = pFnInitialize( m_pPluginHost, pluginArgs.c_str() );
        if ( pluginRc != 0 )
        {
            ErrPrintf( "%s: plugin's __Initialize() returned error\n",
                __FUNCTION__ );
            return RC::SOFTWARE_ERROR;
        }
    }

    // the plugin is now loaded and ready to go!
    //

    return OK;
}

bool Trace3DTest::HandleClientIRQStatus(MailBox::IRQStatus irqStatus)
{
    m_clientExited = true;

    if (irqStatus != MailBox::IRQ_TESTSUCCEEDED)
    {
        ErrPrintf("Detect IRQStatus = IRQ_TESTFAILED in Core %d mailbox, "
                "all the plugins running on core %d have exited, so Mods will "
                "shutdown the interact with plugins on core %d\n",m_CoreId,m_CoreId,m_CoreId);
        m_clientStatus = Test::TEST_FAILED;

        if (status == Test::TEST_SUCCEEDED || status == Test::TEST_INCOMPLETE)
        {
            status = m_clientStatus;
        }
        return false;
    }
    else
    {
        InfoPrintf("Detect IRQStatus = IRQ_TESTSUCCEEDED in Core %d mailbox,"
                "all the plugins running on core %d have exited, so Mods will"
                "shutdown the interact with plugins on core %d\n",m_CoreId,m_CoreId,m_CoreId);
        m_clientStatus = Test::TEST_SUCCEEDED;
        return true;
    }
}

RC Trace3DTest::PollAckAndServeApiRequest(MailBox::Command ack)
{
    RC rc;

    while (true)
    {
        do {
            MailBox::IRQStatus status = m_TraceMailBox->ReadIRQStatus();
            if (status != MailBox::IRQ_OK)
            {
                // usually status equals to MailBox::IRQ_OK
                // but when FailTest is called in interrupt
                // handler at plugin side, the process on the
                // related CPU will exit, before exit, it changes
                // the IRQ status to inform mods that the process
                // on the related CPU is exited, so mods should
                // not interact with plugin side any more.
                if (HandleClientIRQStatus(status))
                    return rc;
                else
                    return RC::SOFTWARE_ERROR;
            }
            else
            {
                Tasker::Yield();
            }
        } while (!m_TraceMailBox->HaveMail());

        m_TraceMailBox->ResetDataOffset();
        MailBox::Command cmd = m_TraceMailBox->ReadCommand();

        if (MailBox::CMD_T3DAPI_REQ == cmd)
        {
            DebugPrintf("mods get a request from mailbox\n");
            UINT32 apiId = m_TraceMailBox->Read<UINT32>();
            if (T3dPlugin::apiReceivers[apiId] != 0)
            {
                DebugPrintf("%s: Calling API %s through mailbox protocol.\n", __FUNCTION__, T3dPlugin::shimFuncNames[apiId - 1]);
                (*T3dPlugin::apiReceivers[apiId])(m_PluginMailBox, m_TraceMailBox);

                if(apiId == T3dPlugin::ENUM_UtilityManager_FailTest)
                {
                    if (m_clientStatus == Test::TEST_SUCCEEDED)
                        return rc;
                    else
                        return RC::SOFTWARE_ERROR;
                }
            }
            else
            {
                ErrPrintf("%s: API %s does not support mailbox protocol yet.\n", __FUNCTION__, T3dPlugin::shimFuncNames[apiId - 1]);
                return RC::SOFTWARE_ERROR;
            }
        }
        else if (ack == cmd)
        {
            DebugPrintf("mods get an ack from mailbox\n");
            // Let thread yield when an event handler was done, though not necessary.
            Tasker::Yield();
            break;
        }
        else
        {
            WarnPrintf("mods get an unexpected command from mailbox. cmd = %d\n", cmd);
        }

        // in case of top rtl, the simulator need be clocked
        if (params->ParamUnsigned("-cpu_model") == 2 && Platform::GetSimulationMode() != Platform::Hardware)
            Platform::ClockSimulator(1);
    }

    return rc;
}

RC Trace3DTest::PluginStartupSender()
{
    RC rc;

    m_PluginMailBox->Acquire();

    m_PluginMailBox->ResetDataOffset();
    m_PluginMailBox->Write(m_pPluginHost);
    m_PluginMailBox->Write(T3dPlugin::majorVersion);
    m_PluginMailBox->Write(T3dPlugin::minorVersion);
    m_PluginMailBox->WriteCommand(MailBox::CMD_STARTUP_REQ);

    m_PluginMailBox->Send();

    InfoPrintf("%s: Calling plugin __Startup() through mailbox protocol.\n", __FUNCTION__);
    CHECK_RC(PollAckAndServeApiRequest(MailBox::CMD_STARTUP_ACK));
    InfoPrintf("%s: Plugin __Startup() finished.\n", __FUNCTION__);

    if (m_clientExited == true)
    {
        if (m_clientStatus == Test::TEST_FAILED)
            return RC::SOFTWARE_ERROR;
        else
            return rc;
    }

    m_TraceMailBox->ResetDataOffset();
    int pluginRc = m_TraceMailBox->Read<int>();
    UINT32 pluginMajorVerion = m_TraceMailBox->Read<UINT32>();
    UINT32 pluginMinorVersion = m_TraceMailBox->Read<UINT32>();

    m_TraceMailBox->Release();

    if ( pluginMajorVerion != T3dPlugin::majorVersion )
    {
        ErrPrintf( "%s: major version mismatch (mods=%d, plugin=%d),"
                   " mods/plugin are not compatible\n",
                   __FUNCTION__, T3dPlugin::majorVersion, pluginMajorVerion );
        return RC::SOFTWARE_ERROR;
    }
    InfoPrintf( "trace_3d: plugin interface versions: trace_3d: major=%d "
                "minor=%d, plugin: major=%d minor=%d\n",
                T3dPlugin::majorVersion, T3dPlugin::minorVersion,
                pluginMajorVerion, pluginMinorVersion );
    if ( pluginRc != 0 )
    {
        ErrPrintf( "%s: plugun's __Startup() returned error\n", __FUNCTION__ );
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

RC Trace3DTest::PluginInitializeSender(const char *pluginArgs)
{
    RC rc;

    m_PluginMailBox->Acquire();

    m_PluginMailBox->ResetDataOffset();
    m_PluginMailBox->Write(m_pPluginHost);
    m_PluginMailBox->Write(pluginArgs);
    m_PluginMailBox->WriteCommand(MailBox::CMD_INITIALIZE_REQ);

    m_PluginMailBox->Send();

    InfoPrintf("%s: Calling plugin __Initialize() through mailbox protocol.\n", __FUNCTION__);
    CHECK_RC(PollAckAndServeApiRequest(MailBox::CMD_INITIALIZE_ACK));
    InfoPrintf("%s: Plugin __Initialize() finished.\n", __FUNCTION__);

    if (m_clientExited == true)
    {
        if (m_clientStatus == Test::TEST_FAILED)
            return RC::SOFTWARE_ERROR;
        else
            return rc;
    }

    m_TraceMailBox->ResetDataOffset();
    int pluginRc = m_TraceMailBox->Read<int>();

    m_TraceMailBox->Release();

    if ( pluginRc != 0 )
    {
        ErrPrintf( "%s: plugin's __Initialize() returned error\n",
                   __FUNCTION__ );
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

RC Trace3DTest::PluginShutdownSender()
{
    RC rc;

    m_PluginMailBox->Acquire();

    m_PluginMailBox->ResetDataOffset();
    m_PluginMailBox->Write(m_pPluginHost);
    m_PluginMailBox->WriteCommand(MailBox::CMD_SHUTDOWN_REQ);

    m_PluginMailBox->Send();

    InfoPrintf("%s: Calling plugin __Shutdown() through mailbox protocol.\n", __FUNCTION__);
    CHECK_RC(PollAckAndServeApiRequest(MailBox::CMD_SHUTDOWN_ACK));
    InfoPrintf("%s: Plugin __Shutdown() finished.\n", __FUNCTION__);

    if (m_clientExited == true)
    {
        if (m_clientStatus == Test::TEST_FAILED)
            return RC::SOFTWARE_ERROR;
        else
            return rc;
    }

    m_TraceMailBox->ResetDataOffset();
    int pluginRc = m_TraceMailBox->Read<int>();

    m_TraceMailBox->Release();

    if ( pluginRc != 0 )
    {
        ErrPrintf( "%s: plugin's __Shutdown() returned error\n",
                   __FUNCTION__ );
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

RC Trace3DTest::LoadTopPlugin(const char *pluginArgs)
{
    RC rc;

    if (params->ParamPresent("-cpu_core"))
    {
        m_CoreId = params->ParamUnsigned("-cpu_core");
        if (m_CoreId > 3)
        {
            ErrPrintf("%s: Invalid CPU core ID %u. Only 0-3 are supported.\n",
                    __FUNCTION__, m_CoreId);
            return RC::SOFTWARE_ERROR;
        }
        InfoPrintf("%s: Top plugin running on core %u.\n", __FUNCTION__, m_CoreId);
    }

    m_PluginMailBox = MailBox::Create(MailBox::MailBoxScratchBase + MailBox::MailBoxSize * m_CoreId * 2, m_CoreId, m_ThreadId);
    m_TraceMailBox = MailBox::Create(MailBox::MailBoxScratchBase + MailBox::MailBoxSize * m_CoreId * 2 + MailBox::MailBoxSize, m_CoreId, m_ThreadId);

    if (s_Hosts[m_pluginName].refCount == 0)
    {
        if (!params->ParamPresent("-cpu_model"))
        {
            ErrPrintf("please specify the -cpu_model that test will run on.\n");
            return RC::SOFTWARE_ERROR;
        }

        T3dPlugin::InitApiReceivers();

        // In case of asim(0) and dsim(1), launch cosim, and no need to launch cosim for rtl(2).
        if (params->ParamUnsigned("-cpu_model") != 2)
        {
            // Get kernel elf and secondary elf names
            const char* elfName = m_pluginName.c_str();
            std::string secElfName;
            if (params->ParamPresent("-sec_elf_name") > 0)
                secElfName.assign(params->ParamStr("-sec_elf_name"));

            // Get the optional backdoor memory library path
            const char* backdoorPath = 0;
            if (params->ParamPresent("-backdoorlib_path"))
                backdoorPath = params->ParamStr("-backdoorlib_path");

            const char* cosim_args = "";
            if (params->ParamPresent("-cosim_args") > 0) {
                cosim_args = params->ParamStr("-cosim_args");
            }

            // Load elf.
            rc = RunCosim(elfName,
                          secElfName.empty() ? 0 : secElfName.c_str(),
                          backdoorPath, cosim_args);
            if ( rc != OK )
            {
                ErrPrintf("%s: RunCosim failed.\n", __FUNCTION__);
                return rc;
            }
        }
    }

    while (!m_PluginMailBox->IsReady())
    {
        Tasker::Yield();
    }

    InfoPrintf("%s: Top plugin is now ready.\n", __FUNCTION__);

    m_pPluginHost = T3dPlugin::HostMods::Create(this, NULL);

    if (!m_pPluginHost)
    {
        ErrPrintf( "%s: couldn't create HostMods object\n", __FUNCTION__ );
        return RC::SOFTWARE_ERROR;
    }
    s_Hosts[m_pluginName].refCount++;
    s_Hosts[m_pluginName].hosts.push_back(m_pPluginHost);

    CHECK_RC(PluginStartupSender());
    CHECK_RC(PluginInitializeSender(pluginArgs));

    return rc;
}

void Trace3DTest::ShutdownPlugins( void )
{
    if (m_IsTopPlugin)
    {
        RC rc = ShutdownTopPlugins();
        if (rc != OK)
        {
            ErrPrintf( "%s: couldn't shutdown top plugin library.\n", __FUNCTION__);
            MASSERT(0);
        }
    }
    else
    {
        // tell the plugin to free all memory and to prepare for being unloaded
        //
        m_pPluginHost->Shutdown();

        if ( m_pluginLibraryHandle )
        {
            Xp::UnloadDLL( m_pluginLibraryHandle );
            m_pluginLibraryHandle = 0;
        }
    }

    // free all trace_3d memory associated with the plugin
    //
    PluginHosts::iterator it = s_Hosts.find(m_pluginName);
    if(it == s_Hosts.end())
    {
        ErrPrintf( "cannot findout plugin:%s in records\n",
            m_pluginName.c_str() );
        MASSERT(0);
    }
    (it->second).refCount--;
    InfoPrintf("plugin %s unloaded, refCount = %d\n",
        m_pluginName.c_str(),  (it->second).refCount);
    if((it->second).refCount == 0)
    {
        InfoPrintf("delete %d Hosts for plugin %s in MODS side\n",
            (it->second).hosts.size(), m_pluginName.c_str());

        vector<T3dPlugin::HostMods *>::iterator pHost = (it->second).hosts.begin();
        vector<T3dPlugin::HostMods *>::iterator pEnd = (it->second).hosts.end();
        for(; pHost !=  pEnd; ++pHost)
        {
            delete *pHost;
        }
        s_Hosts.erase(it);
    }
    m_pPluginHost = 0;
}

RC Trace3DTest::ShutdownTopPlugins()
{
    RC rc;

    if (!m_clientExited)
    {
        CHECK_RC(PluginShutdownSender());
    }

    if (s_Hosts[m_pluginName].refCount == 1)
    {
        // Inform all cores and threads to exit the main function.
        MailBox::GlobalShutdown();

        // In case of asim(0) and dsim(1), shutdown.
        if (params->ParamUnsigned("-cpu_model") != 2)
        {
            // Unload ELF.
            ShutdownAsim();
        }
    }

    delete m_PluginMailBox;
    m_PluginMailBox = NULL;
    delete m_TraceMailBox;
    m_TraceMailBox = NULL;

    return rc;
}

void Trace3DTest::ShutdownAsim()
{
    // This should shutdown all ASIM cores.
    UINT32 ret = 0xFFFFFFFF;
    Platform::PhysWr(0x538F0FFC, &ret, sizeof(ret));
}

void Trace3DTest::BlockEvent( bool bBlock )
{
    if ( bBlock )
        m_NeedBlockEvent = true;

    while( m_IsProcessingEvent && m_NeedBlockEvent )
    {
        Tasker::Yield();
    }

    m_IsProcessingEvent = true;

    if ( bBlock )
        m_NeedBlockEvent = true;
}

void Trace3DTest::UnBlockEvent( bool bBlock )
{
    m_IsProcessingEvent = false;

    if(bBlock)
        m_NeedBlockEvent = false;
}

Trace3DTest::TraceEventType Trace3DTest::GetEventTypeByStr(const string& eventName)
{
    auto it = std::find_if(Trace3DTest::s_EventTypeMap.begin(), Trace3DTest::s_EventTypeMap.end(),
            [&eventName](const std::pair<TraceEventType, string>& t) -> bool
            {
                return t.second == eventName;
            });

    if ( it == Trace3DTest::s_EventTypeMap.end())
    {
        // Event is not supported
        return TraceEventType::Unknown;
    }

    return it->first;
}

// creates and sends a traceEvent with no parameters
//
RC Trace3DTest::traceEvent(TraceEventType eventType, bool bBlock )
{
    const char* eventName = s_EventTypeMap.at(eventType).c_str();

    // Policy Manager handle with trace event.
    PolicyManager *pPM = PolicyManager::Instance();

    if (pPM->IsInitialized())
    {
        PmEventManager *pEventManager = pPM->GetEventManager();

        PmEvent_OnT3dPluginEvent T3dPluginEvent(pPM->GetTestFromUniquePtr(this), eventName);
        RC rc;
        CHECK_RC_MSG(pEventManager->HandleEvent(&T3dPluginEvent),
                "Failed to handle trace3d plugin event in policy manager");
    }

    if (Utl::HasInstance())
    {
        if (Utl::Instance()->TriggerTrace3dEvent(this, eventName) != OK)
        {
            return RC::SOFTWARE_ERROR;
        }
    }

    // Plugin handle with trace event.
    if ( ! m_pPluginHost )
        return OK;

    // Stop sending event if trace3d test has been aborted
    if ( m_bAborted || m_clientExited )
        return OK;


    InfoPrintf( "trace_3d: traceEvent: %s\n", eventName);

    unique_ptr<EventMods> pEvent( newTraceEvent(eventType) );
    if ( !pEvent.get() )
        return RC::SOFTWARE_ERROR;

    return traceEvent( pEvent.get(), bBlock );
}

// creates and returns an empty traceEvent so the caller can fill it with
// parameters before sending

EventMods* Trace3DTest::newTraceEvent(TraceEventType eventType)
{
    if ( ! m_pPluginHost && !Utl::HasInstance())
        return nullptr;

    EventMods * pEvent = EventMods::Create( m_pPluginHost, s_EventTypeMap.at(eventType).c_str() );

    return pEvent;
}

// sends an existing traceEvent
//
RC Trace3DTest::traceEvent( EventMods * pEvent, bool bBlock )
{
    if ( ! m_pPluginHost )
        return OK;

    // Stop sending event if trace3d test has been aborted
    if ( m_bAborted || m_clientExited )
        return OK;

    DebugPrintf("trace_3d: traceEvent: %s\n", pEvent->getName());

    //If other thread is waiting/polling response for some trace event, then yield.
    Trace3DEventLock eventLock(this, bBlock);

    if (!m_pPluginHost->getModsEventManager()->EventOclwrred( pEvent ))
        return RC::SOFTWARE_ERROR;

    return OK;
}

// send a trace event with one UINT32 parameter
//
RC Trace3DTest::traceEvent( TraceEventType eventType,
                            const char * paramName,
                            UINT32 paramValue,
                            bool bBlock )
{
    if ( ! m_pPluginHost )
        return OK;

    unique_ptr<EventMods> pEvent( newTraceEvent(eventType) );
    if ( ! pEvent.get() )
        return RC::SOFTWARE_ERROR;

    pEvent->setGeneratedUint32Arg( paramName, paramValue );

    return traceEvent( pEvent.get(), bBlock );
}

// send a trace event with one string parameter
//
RC Trace3DTest::traceEvent( TraceEventType eventType,
                            const char * paramName,
                            const char * paramValue,
                            bool bBlock )
{
    if ( ! m_pPluginHost )
        return OK;

    unique_ptr<EventMods> pEvent( newTraceEvent(eventType) );
    if ( ! pEvent.get() )
        return RC::SOFTWARE_ERROR;

    pEvent->setGeneratedStringArg( paramName, paramValue );

    return traceEvent( pEvent.get(), bBlock );
}

RC Trace3DTest::lwstomTraceEvent(const char * eventName,
                            const char * paramName,
                            const char * paramValue,
                            bool bBlock )
{
    if ( ! m_pPluginHost )
        return OK;

    unique_ptr<EventMods> pEvent( EventMods::Create(m_pPluginHost, eventName) );
    if ( ! pEvent.get() )
        return RC::SOFTWARE_ERROR;

    pEvent->setGeneratedStringArg( paramName, paramValue );

    return  traceEvent( pEvent.get(), bBlock );
}

// creates and sends a BeforeMethodWrite event of type WriteNop.
//
RC Trace3DTest::BeforeMethodWriteNop_traceEvent( UINT32 origOffset,
                                                 UINT32 channelNumber,
                                                 UINT32 subchannelClass,
                                                 const char* channelName )
{
    if ( ! m_pPluginHost )
        return OK;

    unique_ptr<EventMods> pEvent( newTraceEvent(TraceEventType::BeforeMethodWrite) );
    if ( ! pEvent.get() )
        return RC::SOFTWARE_ERROR;

    pEvent->setGeneratedStringArg( "type", "WriteNop" );
    pEvent->setGeneratedUint32Arg( "offset", origOffset );
    pEvent->setGeneratedUint32Arg( "channelNumber", channelNumber );
    pEvent->setGeneratedUint32Arg( "subchannelClass", subchannelClass );
    pEvent->setGeneratedStringArg( "channelName", channelName );

    return traceEvent( pEvent.get() );
}

// creates and sends a BeforeMethodWrite event of type
// MethodWriteN.  The data is dumped into UINT32 method parameters
// named "data<N>".
//
RC Trace3DTest::BeforeMethodWriteN_traceEvent( UINT32 origOffset,
                                               UINT32 method_offset,
                                               UINT32 num_dwords,
                                               UINT32 * pData,
                                               const char * incrementType,
                                               UINT32 channelNumber,
                                               UINT32 subchannelClass,
                                               const char* channelName )
{
    if ( ! m_pPluginHost )
        return OK;

    unique_ptr<EventMods> pEvent( newTraceEvent(TraceEventType::BeforeMethodWrite) );
    if ( ! pEvent.get() )
        return RC::SOFTWARE_ERROR;

    pEvent->setGeneratedStringArg( "type", "MethodWriteN" );
    pEvent->setGeneratedUint32Arg( "pbOffset", origOffset );
    pEvent->setGeneratedUint32Arg( "method_offset", method_offset );
    pEvent->setGeneratedUint32Arg( "nDwords", num_dwords );
    pEvent->setGeneratedStringArg( "incType", incrementType );
    pEvent->setGeneratedUint32Arg( "channelNumber", channelNumber );
    pEvent->setGeneratedUint32Arg( "subchannelClass", subchannelClass );
    pEvent->setGeneratedStringArg( "channelName", channelName );

    // copy the data into UINT32 parameters named "data<N>"
    //
    char paramName[ 100 ];
    int rv = 0;

    for ( UINT32 i = 0; i < num_dwords; ++i )
    {
        rv = snprintf( paramName, sizeof(paramName), "data%d", i );
        if ( (rv < 0) || (rv >= (int)sizeof(paramName)) )
        {
            ErrPrintf( "%s: snprintf error, rv=%d", __FUNCTION__, rv );
            return RC::SOFTWARE_ERROR;
        }

        pEvent->setGeneratedUint32Arg( paramName, pData[ i ] );
    }

    return traceEvent( pEvent.get() );
}

// creates and sends a BeforeMethodWrite event of type
// MethodWrite.
// Only called when trace_3d is unrolling multi-method headers
//
// Note that when unrolling multiple-method headers, this function
// will receive multiple calls with identical pbOffsetHeader
//
RC Trace3DTest::BeforeMethodWrite_traceEvent( UINT32 pbOffsetHeader,
                                              UINT32 method_offset,
                                              UINT32 methodData,
                                              UINT32 channelNumber,
                                              UINT32 subchannelClass,
                                              const char* channelName )
{
    if ( ! m_pPluginHost )
        return OK;

    unique_ptr<EventMods> pEvent( newTraceEvent(TraceEventType::BeforeMethodWrite) );
    if ( ! pEvent.get() )
        return RC::SOFTWARE_ERROR;

    pEvent->setGeneratedStringArg( "type", "MethodWrite" );
    pEvent->setGeneratedUint32Arg( "pbOffset", pbOffsetHeader );
    pEvent->setGeneratedUint32Arg( "method_offset", method_offset );
    pEvent->setGeneratedUint32Arg( "method_data", methodData );
    pEvent->setGeneratedUint32Arg( "channelNumber", channelNumber );
    pEvent->setGeneratedUint32Arg( "subchannelClass", subchannelClass );
    pEvent->setGeneratedStringArg( "channelName", channelName );

    return traceEvent( pEvent.get() );
}

void GenericTraceModule::setPluginDataHandle ( void * handle )
{
    m_pluginDataHandle = handle;
}

bool GenericTraceModule::HasPluginInitData() const
{
    BufferMods* buffer = (BufferMods*) m_pluginDataHandle;

    return buffer->hasPluginInitData();
}

// called from RC TraceModule::LoadFromFile(TraceFileMgr * pTraceFileMgr)
// when the TraceModule is determined to have been created by a plugin (by
// checking the void * m_pluginDataHandle, if non-null then it's actually a
// pointer to a BufferMods object holding the initial data).
// This call replaces the             rc = pTraceFileMgr->Read(m_Data, h); code
// that reads the surface's initial data contents from the trace
//
//
RC GenericTraceModule::LoadDataFromPlugin( void * handle,
                                           UINT32 * pData,
                                           UINT32 nWords )
{
    InfoPrintf( "trace_3d: %s: handle=%p, pData=%p, nWords=0x%08x (%d)\n",
                __FUNCTION__, handle, pData, nWords, nWords );

    BufferMods * pBuffer = (BufferMods *) handle;

    // copies the plugin specified initial surface data into pData
    //
    pBuffer->copyData( pData, nWords );

    // there is no more use for the trace_3d copy of the plugin-provided
    // initial surface data, free it
    //
    pBuffer->freeData();

    // set the TraceModule's plugin data handle to null to indicate that the
    // plugin-supplied initial surface data is no longer available
    //
    m_pluginDataHandle = 0;

    return OK;
}

bool Trace3DTest::getPluginMessage( const char * msg, UINT32 * pValue )
{
    if ( ! m_pPluginHost )
        return false;

    bool rv =  m_pPluginHost->getMessage( msg, pValue );
    return rv;
}

bool Trace3DTest::PushbufferHeader_traceEvent( UINT32 pbOffsetHeader,
                                               const string & pbName,
                                               T3dPlugin::EventAction * pAction,
                                               UINT32 * pEventData )
{
    if ( !m_pPluginHost && !Utl::HasInstance())
        return false;

    EventMods * pEvent = newTraceEvent( TraceEventType::PushbufferHeader );
    if ( ! pEvent )
        return false;

    pEvent->setGeneratedUint32Arg( "pbOffset", pbOffsetHeader );

    RC rc = traceEvent( pEvent );
    delete pEvent;

    if ( rc != OK )
        return false;

    if (Utl::HasInstance())
    {
        Utl::Instance()->TriggerBeforeSendPushBufHeaderEvent(
                this,
                pbName,
                pbOffsetHeader
                );
        if (!m_pPluginHost)
            return false;
    }

    // check for any action messages from the plugin system
    //
    if ( m_pPluginHost->getMessage( "pushbufHeaderEventAction", (UINT32 *) pAction ) )
    {
        if ( *pAction == T3dPlugin::Jump )
        {
            if ( ! m_pPluginHost->getMessage( "pushbufHeaderEventData", pEventData ) )
            {
                // malformed action / data
                //
                ErrPrintf( "trace3d_plugin: received event action %d but no "
                           "event data\n", (UINT32) *pAction );
                return false;
            }
        }
        return true;
    }
    return false;
}

// returns true if there is a valid response message that needs attending to,
// false otherwise
//
bool Trace3DTest::TraceOp_traceEvent( TraceEventType eventType,
                                      UINT32 traceOpNum,
                                      T3dPlugin::EventAction * pAction,
                                      UINT32 * pEventData )
{
    if ( !m_pPluginHost )
        return false;

    EventMods * pEvent = newTraceEvent( eventType );
    if ( ! pEvent )
        return false;

    pEvent->setGeneratedUint32Arg( "opNum", traceOpNum );
    RC rc = traceEvent( pEvent );
    delete pEvent;

    if ( rc != OK )
        return false;

    // check for any action messages from the plugin system
    //
    if ( pAction != nullptr && 
         pEventData != nullptr &&
         m_pPluginHost->getMessage( "TraceOpEventAction", (UINT32 *) pAction ) )
    {
        if ( *pAction == T3dPlugin::Jump )
        {
            if ( ! m_pPluginHost->getMessage( "TraceOpEventData", pEventData ) )
            {
                // malformed action / data
                //
                ErrPrintf( "trace3d_plugin: received event action %d but no "
                    "event data\n", (UINT32) *pAction );
                return false;
            }
        }
        return true;
    }
    return false;
}

////////////////// trace_3d plugin shim layer ///////////////////
//
// trace_3d passes in two coressponding tables to the plugin side at
// initialization: one of strings, and one of function pointers.  The
// string at position "i" of the string table names the function pointer
// at position "i" of the function pointer table.  In this way the plugin
// side shim functions can find the trace_3d-side  shim functions.
//
// For example: in order for the plugin to call call function "foo" in
// trace_3d's plugin shim layer, (conceptually) the plugin finds function
// "foo" in the string table, notes the index "i", and then fetches the
// function pointer to the corresponding trace_3d shim function from the
// function pointer table, and then can call the function.  In practice
// the plugin side can organize the information in a much more optimal
// fashion to get constant-time lookups.
//
// Using this technique, the version of trace_3d and the version of
// the plugin only need to match in the functions used by the
// plugin, even the compiler can be different since only a C-interface
// is used across the shim layer
//
/////////////////////////////////////////////////////////////////

namespace T3dPlugin
{
    EventManager * Host_getEventManager( Host * pTrace3dHost )
    {
        return pTrace3dHost->getEventManager();
    }
    RTApiReceiver1<EventManager *, Host *> Host_getEventManager_Receiver(Host_getEventManager);

    GpuManager * Host_getGpuManager( Host * pTrace3dHost )
    {
        return pTrace3dHost->getGpuManager();
    }
    RTApiReceiver1<GpuManager *, Host *> Host_getGpuManager_Receiver(Host_getGpuManager);

    BufferManager * Host_getBufferManager( Host * pTrace3dHost )
    {
        return pTrace3dHost->getBufferManager();
    }
    RTApiReceiver1<BufferManager *, Host *> Host_getBufferManager_Receiver(Host_getBufferManager);

    Mem * Host_getMem( Host * pTrace3dHost )
    {
        return pTrace3dHost->getMem();
    }
    RTApiReceiver1<Mem *, Host *> Host_getMem_Receiver(Host_getMem);

    TraceManager * Host_getTraceManager( Host * pTrace3dHost )
    {
        return pTrace3dHost->getTraceManager();
    }
    RTApiReceiver1<TraceManager *, Host *> Host_getTraceManager_Receiver(Host_getTraceManager);

    UtilityManager * Host_getUtilityManager( Host * pTrace3dHost )
    {
        return pTrace3dHost->getUtilityManager();
    }
    RTApiReceiver1<UtilityManager *, Host *> Host_getUtilityManager_Receiver(Host_getUtilityManager);

    JavaScriptManager * Host_getJavaScriptManager( Host * pTrace3dHost )
    {
        return pTrace3dHost->getJavaScriptManager();
    }
    RTApiReceiver1<JavaScriptManager *, Host *> Host_getJavaScriptManager_Receiver(Host_getJavaScriptManager);

    DisplayManager * Host_getDisplayManager( Host * pTrace3dHost )
    {
        return pTrace3dHost->getDisplayManager();
    }
    RTApiReceiver1<DisplayManager *, Host *> Host_getDisplayManager_Receiver(Host_getDisplayManager);

    Event * EventManager_getEvent( EventManager * pTrace3dEventManager,
                                   const char * eventName )
    {
        return pTrace3dEventManager->getEvent( eventName );
    }
    RTApiReceiver2<Event *, EventManager *, const char *> EventManager_getEvent_Receiver(EventManager_getEvent);

    void EventManager_deleteEvent( EventManager * pTrace3dEventManager,
                                   Event * pEvent )
    {
        pTrace3dEventManager->deleteEvent( pEvent );
    }

    int Event_setTemplateUint32Arg( Event * pTrace3dEvent,
                                    const char * argName, UINT32 value )
    {
        return pTrace3dEvent->setTemplateUint32Arg( argName, value );
    }

    int Event_setTemplateStringArg( Event *pTrace3dEvent,
                                    const char * argName,
                                    const char * value )
    {
        return pTrace3dEvent->setTemplateStringArg( argName, value );
    }

    bool Event_getGeneratedUint32Arg( Event * pTrace3dEvent,
                                    const char * argName, UINT32 * pValue )
    {
        return pTrace3dEvent->getGeneratedUint32Arg( argName, pValue );
    }

    class Event_getGeneratedUint32Arg_Receiver_Class : public ApiReceiver
    {
    public:
        void operator()(MailBox* pluginMailBox, MailBox* traceMailBox)
        {
            Event* pEvent = traceMailBox->Read<Event*>();
            const char* argName = traceMailBox->Read<const char*>();

            traceMailBox->Release();

            UINT32 value;
            bool rv = Event_getGeneratedUint32Arg( pEvent, argName, &value );

            pluginMailBox->Acquire();

            pluginMailBox->ResetDataOffset();
            pluginMailBox->Write(value);
            pluginMailBox->Write(rv);

            pluginMailBox->WriteCommand(MailBox::CMD_T3DAPI_ACK);

            pluginMailBox->Send();
        }
    } Event_getGeneratedUint32Arg_Receiver;

    bool Event_getGeneratedStringArg( Event *pTrace3dEvent,
                                    const char * argName,
                                    const char * * pValue )
    {
        return pTrace3dEvent->getGeneratedStringArg( argName, pValue );
    }

    class Event_getGeneratedStringArg_Receiver_Class : public ApiReceiver
    {
    public:
        void operator()(MailBox* pluginMailBox, MailBox* traceMailBox)
        {
            Event* pEvent = traceMailBox->Read<Event*>();
            const char* argName = traceMailBox->Read<const char*>();

            traceMailBox->Release();

            const char* pValue = NULL;
            bool rv = Event_getGeneratedStringArg( pEvent, argName, &pValue );

            pluginMailBox->Acquire();

            pluginMailBox->ResetDataOffset();
            pluginMailBox->Write(pValue);
            pluginMailBox->Write(rv);

            pluginMailBox->WriteCommand(MailBox::CMD_T3DAPI_ACK);

            pluginMailBox->Send();
        }
    } Event_getGeneratedStringArg_Receiver;

    bool Event__register( Event * pTrace3dEvent,
                          pFnCallback_t pFnCallback, void * payload )
    {
        return pTrace3dEvent->_register( pFnCallback, payload );
    }
    RTApiReceiver3<bool, Event *, pFnCallback_t, void *> Event__register_Receiver(Event__register);

    bool Event_unregister( Event * pTrace3dEvent )
    {
        return pTrace3dEvent->unregister();
    }

    UINT32 GpuManager_getNumGpus( GpuManager * pTrace3dGpuManager )
    {
        return pTrace3dGpuManager->getNumGpus();
    }

    Gpu * GpuManager_getGpu( GpuManager * pTrace3dGpuManager, UINT32 gpuNumber )
    {
        return pTrace3dGpuManager->getGpu( gpuNumber );
    }

    void GpuManager_deleteGpu( GpuManager * pTrace3dGpuManager, Gpu * pGpu )
    {
        return pTrace3dGpuManager->deleteGpu( pGpu );
    }

    UINT08 Gpu_RegRd08( Gpu * pTrace3dGpu, UINT32 offset )
    {
        return pTrace3dGpu->RegRd08( offset );
    }

    UINT16 Gpu_RegRd16( Gpu * pTrace3dGpu, UINT32 offset )
    {
        return pTrace3dGpu->RegRd16( offset );
    }

    UINT32 Gpu_RegRd32( Gpu * pTrace3dGpu, UINT32 offset )
    {
        return pTrace3dGpu->RegRd32( offset );
    }

    void Gpu_RegWr08( Gpu * pTrace3dGpu, UINT32 offset, UINT08 data )
    {
        pTrace3dGpu->RegWr08( offset, data );
    }

    void Gpu_RegWr16( Gpu * pTrace3dGpu, UINT32 offset, UINT16 data )
    {
        pTrace3dGpu->RegWr16( offset, data );
    }

    void Gpu_RegWr32( Gpu * pTrace3dGpu, UINT32 offset, UINT32 data )
    {
        pTrace3dGpu->RegWr32( offset, data );
    }

    UINT08 Gpu_FbRd08( Gpu * pTrace3dGpu, UINT32 offset )
    {
        return pTrace3dGpu->FbRd08( offset );
    }

    UINT16 Gpu_FbRd16( Gpu * pTrace3dGpu, UINT32 offset )
    {
        return pTrace3dGpu->FbRd16( offset );
    }

    UINT32 Gpu_FbRd32( Gpu * pTrace3dGpu, UINT32 offset )
    {
        return pTrace3dGpu->FbRd32( offset );
    }

    UINT64 Gpu_FbRd64( Gpu * pTrace3dGpu, UINT32 offset )
    {
        return pTrace3dGpu->FbRd64( offset );
    }

    void Gpu_FbWr08( Gpu * pTrace3dGpu, UINT32 offset, UINT08 data )
    {
        pTrace3dGpu->FbWr08( offset, data );
    }

    void Gpu_FbWr16( Gpu * pTrace3dGpu, UINT32 offset, UINT16 data )
    {
        pTrace3dGpu->FbWr16( offset, data );
    }

    void Gpu_FbWr32( Gpu * pTrace3dGpu, UINT32 offset, UINT32 data )
    {
        pTrace3dGpu->FbWr32( offset, data );
    }

    void Gpu_FbWr64( Gpu * pTrace3dGpu, UINT32 offset, UINT64 data )
    {
        pTrace3dGpu->FbWr64( offset, data );
    }

    UINT32 Gpu_GetSMVersion( Gpu * pTrace3dGpu )
    {
        return pTrace3dGpu->GetSMVersion();
    }

    void Gpu_SwReset( Gpu * pTrace3dGpu )
    {
        pTrace3dGpu->SwReset();
    }

    Buffer * BufferManager_createTraceBuffer( BufferManager * pT3dBufMgr,
                                              const char * name,
                                              const char * type,
                                              UINT32 size,
                                              const UINT32 * pData )
    {
        return pT3dBufMgr->createTraceBuffer( name, type, size, pData );
    }

    void BufferManager_deleteBuffer( BufferManager * pT3dBufMgr,
                                     Buffer * pBuffer )
    {
        pT3dBufMgr->deleteBuffer( pBuffer );
    }

    void BufferManager_refreshBufferList( BufferManager * pT3dBufMgr )
    {
        pT3dBufMgr->refreshBufferList();
    }

    // BufferIterator shim functions are obsolete.   Worse than that, they're
    // broken -- the shim layer must not pass c++ objects back and forth, only
    // pointers to objects, and those pointers are never dereferenced on the
    // plugin side.
    // The BufferIterator shim functions are replaced with
    // BufferManager_GetNumBuffers and BufferManager_GetBufferProperties
    //
    BufferManager::BufferIterator BufferManager_begin( BufferManager * pT3dBufMgr )
    {
        return pT3dBufMgr->begin();
    }

    BufferManager::BufferIterator BufferManager_end( BufferManager * pT3dBufMgr )
    {
        return pT3dBufMgr->end();
    }

    UINT64 Buffer_getGpuVirtAddr( Buffer * pT3dBuffer )
    {
        return pT3dBuffer->getGpuVirtAddr();
    }

    UINT64 Buffer_getGpuPhysAddr( Buffer * pT3dBuffer, UINT32 subdev )
    {
        return pT3dBuffer->getGpuPhysAddr( subdev );
    }

    UINT32 Buffer_getPteKind( Buffer * pT3dBuffer )
    {
        return pT3dBuffer->getPteKind();
    }

    UINT32 Buffer_getOffsetWithinRegion( Buffer * pT3dBuffer )
    {
        return pT3dBuffer->getOffsetWithinRegion();
    }

    void Buffer_mapMemory( Buffer * pT3dBuffer, UINT32 offset,
                           UINT32 bytesToMap, UINT64 * virtAddr,
                           UINT32 gpuSubdevIdx )
    {
        pT3dBuffer->mapMemory( offset, bytesToMap, virtAddr, gpuSubdevIdx );
    }

    void Buffer_unmapMemory( Buffer * pT3dBuffer, UINT64 virtAddr,
                             UINT32 gpuSubdevIdx )
    {
        pT3dBuffer->unmapMemory( virtAddr, gpuSubdevIdx );
    }

    const char * Buffer_getName( Buffer * pT3dBuffer )
    {
        return pT3dBuffer->getName();
    }

    const char * Buffer_getType( Buffer * pT3dBuffer )
    {
        return pT3dBuffer->getType();
    }

    UINT32 Buffer_getSize( Buffer * pT3dBuffer )
    {
        return pT3dBuffer->getSize();
    }

    UINT08 Mem_VirtRd08( Mem * pT3dMem, UINT64 va )
    {
        return pT3dMem->VirtRd08( va );
    }

    UINT16 Mem_VirtRd16( Mem * pT3dMem, UINT64 va )
    {
        return pT3dMem->VirtRd16( va );
    }

    UINT32 Mem_VirtRd32( Mem * pT3dMem, UINT64 va )
    {
        return pT3dMem->VirtRd32( va );
    }

    UINT64 Mem_VirtRd64( Mem * pT3dMem, UINT64 va )
    {
        return pT3dMem->VirtRd64( va );
    }

    void Mem_VirtRd( Mem * pT3dMem, UINT64 va, void * data, UINT32 count )
    {
        pT3dMem->VirtRd(va, data, count);
    }

    void Mem_VirtWr08( Mem * pT3dMem, UINT64 va, UINT08 data )
    {
        pT3dMem->VirtWr08( va, data );
    }

    void Mem_VirtWr16( Mem * pT3dMem, UINT64 va, UINT16 data )
    {
        pT3dMem->VirtWr16( va, data );
    }

    void Mem_VirtWr32( Mem * pT3dMem, UINT64 va, UINT32 data )
    {
        pT3dMem->VirtWr32( va, data );
    }

    void Mem_VirtWr64( Mem * pT3dMem, UINT64 va, UINT64 data )
    {
        pT3dMem->VirtWr64( va, data );
    }

    void Mem_VirtWr( Mem * pT3dMem, UINT64 va, const void * data, UINT32 count)
    {
        pT3dMem->VirtWr(va, data, count);
    }

    bool Mem_AllocIRam( Mem * pT3dMem, UINT32 size, UINT64 * va, UINT64 * pa )
    {
        return pT3dMem->AllocIRam( size, va, pa );
    }

    class Mem_AllocIRam_Receiver_Class : public ApiReceiver
    {
    public:
        void operator ()(MailBox *pluginMailBox, MailBox *traceMailBox)
        {
            // No reset.
            Mem *pMem =  traceMailBox->Read<Mem *>();
            UINT32 size =  traceMailBox->Read<UINT32>();
            UINT64 virtAddr = 0;
            UINT64 physAddr = 0;

            traceMailBox->Release();

            bool rt = Mem_AllocIRam(pMem, size, &virtAddr, &physAddr);

            pluginMailBox->Acquire();

            pluginMailBox->ResetDataOffset();
            pluginMailBox->Write(rt);
            pluginMailBox->Write(virtAddr);
            pluginMailBox->Write(physAddr);
            pluginMailBox->WriteCommand(MailBox::CMD_T3DAPI_ACK);

            pluginMailBox->Send();
        }
    } Mem_AllocIRam_Receiver;

    void Mem_FreeIRam( Mem * pT3dMem, UINT64 va)
    {
        pT3dMem->FreeIRam(va);
    }

    VoidApiReceiver2<Mem *, UINT64> Mem_FreeIRam_Receiver(Mem_FreeIRam);

    int TraceManager_FlushMethodsAllChannels( TraceManager * pT3dTraceMgr )
    {
        return pT3dTraceMgr->FlushMethodsAllChannels();
    }
    RTApiReceiver1<int, TraceManager *> TraceManager_FlushMethodsAllChannels_Receiver(TraceManager_FlushMethodsAllChannels);

    int TraceManager_WaitForDMApushAllChannels( TraceManager * pT3dTraceMgr )
    {
        return pT3dTraceMgr->WaitForDMAPushAllChannels();
    }

    UINT32 TraceManager_getTraceVersion( TraceManager * pT3dTraceMgr )
    {
        return pT3dTraceMgr->getTraceVersion();
    }

    int TraceManager_WaitForIdleAllChannels( TraceManager * pT3dTraceMgr )
    {
        return pT3dTraceMgr->WaitForIdleAllChannels();
    }
    RTApiReceiver1<int, TraceManager *> TraceManager_WaitForIdleAllChannels_Receiver(TraceManager_WaitForIdleAllChannels);

    int TraceManager_WaitForIdleOneChannel( TraceManager * pT3dTraceMgr, UINT32 chNum )
    {
        return pT3dTraceMgr->WaitForIdleOneChannel(chNum);
    }

    UINT32 TraceManager_GetMethodCountOneChannel( TraceManager * pT3dTraceMgr, UINT32 chNum )
    {
        return pT3dTraceMgr->GetMethodCountOneChannel(chNum);
    }

    UINT32 TraceManager_GetMethodWriteCountOneChannel( TraceManager * pT3dTraceMgr, UINT32 chNum )
    {
        return pT3dTraceMgr->GetMethodWriteCountOneChannel(chNum);
    }

    UINT32 TraceManager_GetNumTraceOps( TraceManagerMods * pT3dTraceMgr )
    {
        Trace3DTest * pT3dTest = pT3dTraceMgr->getT3dTest();
        const TraceOps & traceOps = pT3dTest->GetTrace()->GetTraceOps();
        return traceOps.size();
    }
    RTApiReceiver1<UINT32, TraceManagerMods *> TraceManager_GetNumTraceOps_Receiver(TraceManager_GetNumTraceOps);

    void TraceManager_GetTraceOpProperties( TraceManagerMods * pT3dTraceMgr,
                                            UINT32 traceOpNum,
                                            const char * * pName,
                                            UINT32 * pNumProperties,
                                            const char * * * pPropNames,
                                            const char * * * pPropData )
    {
        Trace3DTest * pT3dTest = pT3dTraceMgr->getT3dTest();
        const TraceOps & traceOps = pT3dTest->GetTrace()->GetTraceOps();
        if ( traceOpNum >= traceOps.size() )
            return;

        // Optimization to store the previous TraceOp iterator so we do not need
        // to advance from the start each time
        static GpuTrace* s_pTrace = nullptr;
        static TraceOps::const_iterator s_It = traceOps.begin();
        static UINT32 s_PrevTraceOpNum = 0;

        // Re-init cached data in case this is a different trace
        if (pT3dTest->GetTrace() != s_pTrace)
        {
            s_pTrace = pT3dTest->GetTrace();
            s_It = traceOps.begin();
            s_PrevTraceOpNum = 0;
        }
        std::advance(s_It, traceOpNum - s_PrevTraceOpNum);
        s_PrevTraceOpNum = traceOpNum;

        static map<string,string> properties;
        properties.clear();

        (s_It->second)->GetTraceopProperties( pName, properties );

        // unpack the properties into arrays of string pointers
        //

        // this is really ugly, but the shim layer requires passing raw C data
        //
        const UINT32 maxProperties = 100;
        static const char * s_propNames[ maxProperties ];
        static const char * s_propData[ maxProperties ];

        UINT32 nProperties = properties.size();
        if ( nProperties > maxProperties )
        {
            ErrPrintf( "%s: # of properties exceeded, setting # to max=%d\n",
                       __FUNCTION__, maxProperties );
            nProperties = maxProperties;
        }
        *pNumProperties = nProperties;

        map<string,string>::const_iterator itProp = properties.begin();
        for ( UINT32 i = 0; i < nProperties; ++i, ++itProp )
        {
            s_propNames[i] = (itProp->first).c_str();
            s_propData[i] = (itProp->second).c_str();
        }

        *pPropNames = s_propNames;
        *pPropData = s_propData;
    }

    class TraceManager_GetTraceOpProperties_Receiver_Class : public ApiReceiver
    {
    public:
        void operator ()(MailBox *pluginMailBox, MailBox *traceMailBox)
        {
            TraceManagerMods *pT3dTraceMgr = traceMailBox->Read<TraceManagerMods *>();
            UINT32 traceOpNum = traceMailBox->Read<UINT32>();
            traceMailBox->Release();

            const char *name;
            UINT32 nProperties = 0;
            const char **pPropNames, **pPropData;
            TraceManager_GetTraceOpProperties(pT3dTraceMgr, traceOpNum, &name, &nProperties, &pPropNames, &pPropData);

            pluginMailBox->Acquire();
            pluginMailBox->ResetDataOffset();
            pluginMailBox->Write(name);
            pluginMailBox->Write(nProperties);

            for (UINT32 i = 0; i < nProperties; ++i)
            {
                pluginMailBox->Write(pPropNames[i]);
                pluginMailBox->Write(pPropData[i]);
            }

            pluginMailBox->WriteCommand(MailBox::CMD_T3DAPI_ACK);
            pluginMailBox->Send();
        }
    } TraceManager_GetTraceOpProperties_Receiver;

    const char* TraceManager_GetSyspipe(TraceManager* pTraceManager)
    {
        return pTraceManager->GetSyspipe();
    }

    UINT64 UtilityManager_GetTimeNS( UtilityManager * pT3dUtilityMgr )
    {
        return pT3dUtilityMgr->GetTimeNS();
    }

    UINT64 UtilityManager_GetSimulatorTime( UtilityManager * pT3dUtilityMgr )
    {
        return pT3dUtilityMgr->GetSimulatorTime();
    }

    void UtilityManager__Yield( UtilityManager * pT3dUtilityMgr )
    {
        pT3dUtilityMgr->_Yield();
    }

    class UtilityManager__Yield_Receiver_Class : public ApiReceiver
    {
    public:
        void operator ()(MailBox *pluginMailBox, MailBox *traceMailBox)
        {
            // Don't need to write back the ack to plugin side because in plugin
            // side, _Yield is implemented as an asynchronous call to Mods.
            UtilityManager *pT3dUtilityMgr = traceMailBox->Read<UtilityManager *>();

            traceMailBox->Release();

            UtilityManager__Yield(pT3dUtilityMgr);
        }
    } UtilityManager__Yield_Receiver;

    UINT32 UtilityManager_getNumErrLoggerStrings( UtilityManager * pT3dUtilityMgr )
    {
        return pT3dUtilityMgr->getNumErrorLoggerStrings();
    }

    const char * UtilityManager_getErrorLoggerString( UtilityManager * pT3dUtilityMgr,
                                                      UINT32 strNum )
    {
        return pT3dUtilityMgr->getErrorLoggerString( strNum );
    }

    UINT32 UtilityManager_GetThreadId()
    {
        return Tasker::LwrrentThread();
    }

    RTApiReceiver0<UINT32> UtilityManager_GetThreadId_Receiver(UtilityManager_GetThreadId);

    void UtilityManager_DisableInterrupts(UtilityManager * pT3dUtilityMgr)
    {
        return pT3dUtilityMgr->DisableInterrupts();
    }

    void UtilityManager_EnableInterrupts(UtilityManager * pT3dUtilityMgr)
    {
        return pT3dUtilityMgr->EnableInterrupts();
    }

    // broken.  Instead the plugin side will query the # of Buffers and get
    // the buffer properties one at a time and build up a plugin-side mirror
    // list (just like GetNumSurfaces and GetSurfaceProperties)
    //
    Buffer * BufferIterator_Dereference( BufferManager::BufferIterator it )
    {
        return *it;
    }

    BufferManager::BufferIterator BufferIterator_Advance( BufferManager::BufferIterator it )
    {
        return ++it;
    }

    bool BufferIterator_Equal( BufferManager::BufferIterator it1,
                               BufferManager::BufferIterator it2 )
    {
        return ( it1 == it2 );
    }

    bool IsSurface(const MdiagSurf * pS2d) // callable from t3plugin.cpp
    {
        bool isSurfaceView = pS2d->IsSurfaceView(); // bug 887585
        bool isDimensional = pS2d->GetWidth() && pS2d->GetHeight();
        return isSurfaceView || isDimensional;
    }

    // counts & returns the # of valid surface in the surface manager
    //
    UINT32 BufferManager_GetNumSurfaces( BufferManagerMods * pT3dBufMgr )
    {
        Trace3DTest * pT3dTest = pT3dBufMgr->getT3dTest();
        IGpuSurfaceMgr * gsm = pT3dTest->GetSurfaceMgr();

        UINT32 nSurfaces = 0;

        ::MdiagSurf* pSurf = gsm->GetSurfaceHead( 0 );
        while ( pSurf )
        {
            if (  gsm->GetValid( pSurf ) )
                ++ nSurfaces;

            pSurf = gsm->GetSurfaceNext( 0 );
        }

        // now, look for trace modules that are really surfaces and that are
        // not already managed by the surface manager.
        //

        GpuTrace * pGpuTrace = pT3dTest->GetTrace();
        ModuleIter modIt = pGpuTrace->ModBegin();
        ModuleIter modIt_end = pGpuTrace->ModEnd();
        for ( ; modIt != modIt_end; ++modIt )
        {
            TraceModule * pTraceModule( *modIt );
            if ( pTraceModule->IsColorOrZ() )
            {
                // this buffer is already managed by the surface manager,
                // so we've already counted it above
                //
                continue;
            }

            const MdiagSurf * pSurf( pTraceModule->GetDmaBuffer() );
            if ( pSurf && IsSurface(pSurf) )
            {
                // found one!  This is really a surface, even though it's in
                // the trace module list.  We'll include it in
                // the surface list
                //
                ++ nSurfaces;
            }
        }

        return nSurfaces;
    }

    // not a plugin shim function but a utility function that extracts the
    // surface properties into a C property list.  Shared both by
    // BufferManager_GetSurfaceProperties and by
    // BufferManager_GetbufferProperties for those buffers which are really
    // surfaces (e.g., ALLOC_SURFACE can result in this type of "surface" that is
    // not managed by the surface manager).
    //
    // params:
    //   property: the starting property index
    //   pSurf: the surface object whose properties we're fetching
    //   pMod: the TraceModule object containing pSurf
    //   propNames: names of the properties we're extracting
    //   propData: data values
    //
    //  returns: # of properties added
    //
    UINT32 GetSurfaceProperties( UINT32 property, const MdiagSurf * pSurf,
                                 const TraceModule * pMod,
                                 const char * * * ppPropNames,
                                 const char * * * ppPropData )
    {
        // keep this large enough for the properties below
        const UINT32 maxProperties = 20;
        static const char * s_propNames[ maxProperties ];
        static const char * s_propData[ maxProperties ];

        *ppPropNames = s_propNames;
        *ppPropData  = s_propData;

        UINT32 startProperty( property );

        // some texture surface properties are stored inside TraceModule.
        TextureHeader *th = NULL;
        s_propNames[ property ] = "isTexture";
        static string isTexture;
        if (pMod && pMod->GetFileType() == GpuTrace::FT_TEXTURE)
        {
            th = pMod->GetTxParams();
            isTexture = "1";
        }
        else if (pSurf && pSurf->GetType() == LWOS32_TYPE_TEXTURE)
        {
            isTexture = "1";
        }
        else
        {
            isTexture = "0";
        }
        s_propData[ property++ ] = isTexture.c_str();

        s_propNames[ property ] = "width";
        static string widthStr;
        widthStr = Utility::StrPrintf( "%d", th ? th->TexWidth() : pSurf->GetWidth() );
        s_propData[ property++ ] = widthStr.c_str();

        s_propNames[ property ] = "height";
        static string heightStr;
        heightStr = Utility::StrPrintf( "%d", th ? th->TexHeight() : pSurf->GetHeight() );
        s_propData[ property++ ] = heightStr.c_str();

        s_propNames[ property ] = "depth";
        static string depthStr;
        depthStr = Utility::StrPrintf( "%d", th ? th->TexDepth() : pSurf->GetDepth() );
        s_propData[ property++ ] = depthStr.c_str();

        s_propNames[ property ] = "arraySize";
        static string arraySizeStr;
        arraySizeStr = Utility::StrPrintf( "%d", pSurf->GetArraySize() );
        s_propData[ property++ ] = arraySizeStr.c_str();

        s_propNames[ property ] = "bytesPerPixel";
        static string bytesPerPixelStr;
        bytesPerPixelStr = Utility::StrPrintf( "%d", pSurf->GetBytesPerPixel() );
        s_propData[ property++ ] = bytesPerPixelStr.c_str();

        s_propNames[ property ] = "format";
        static string formatStr;
        formatStr = ColorUtils::FormatToString( pSurf->GetColorFormat() );
        s_propData[ property++ ] = formatStr.c_str();

        s_propNames[ property ] = "rawSize";
        static string rawSizeStr;
        rawSizeStr = Utility::StrPrintf( "%llu", pSurf->GetSize() );
        s_propData[ property++ ] = rawSizeStr.c_str();

        s_propNames[ property ] = "memHandle";
        static string memHandleStr;
        memHandleStr = Utility::StrPrintf( "%u", pSurf->GetMemHandle() );
        s_propData[ property++ ] = memHandleStr.c_str();

        s_propNames[ property ] = "pteKind";
        static string pteKind;
        pteKind = Utility::StrPrintf( "%u", pSurf->GetPteKind() );
        s_propData[ property++ ] = pteKind.c_str();

        s_propNames[ property ] = "virtualAddress";
        static string virtualAddress;
        virtualAddress = Utility::StrPrintf( "%u",
            (unsigned int) (pSurf->GetCtxDmaOffsetGpu() +
            pSurf->GetExtraAllocSize()) );
        s_propData[ property++ ] = virtualAddress.c_str();

        s_propNames[property] = "layout";
        s_propData[property++] = pSurf->GetLayoutStr(pSurf->GetLayout());

        s_propNames[property] = "tiled";
        static string tiled;
        tiled = Utility::StrPrintf("%u", pSurf->GetTiled());
        s_propData[property++] = tiled.c_str();

        s_propNames[property] = "blockWidthInGob";
        static string blockWidth;
        blockWidth = Utility::StrPrintf("%u", th ? th->BlockWidth() : 1 << pSurf->GetLogBlockWidth());
        s_propData[property++] = blockWidth.c_str();

        s_propNames[property] = "blockHeightInGob";
        static string blockHeight;
        blockHeight = Utility::StrPrintf("%u", th ? th->BlockHeight() : 1 << pSurf->GetLogBlockHeight());
        s_propData[property++] = blockHeight.c_str();

        s_propNames[property] = "blockDepthInGob";
        static string blockDepth;
        blockDepth = Utility::StrPrintf("%u", th ? th->BlockDepth() : 1 << pSurf->GetLogBlockDepth());
        s_propData[property++] = blockDepth.c_str();

        s_propNames[property] = "compressed";
        static string compressed;
        compressed = Utility::StrPrintf("%u", (UINT32)pSurf->GetCompressed());
        s_propData[property++] = compressed.c_str();

        return property - startProperty;
    }

    // if the given surface is not found, *pNumProperties will
    // be set to zero upon return
    //
    void BufferManager_GetSurfaceProperties( BufferManagerMods * pT3dBufMgr,
                                             UINT32 surfNum,
                                             const char * * ppName,
                                             UINT32 * pNumProperties,
                                             const char * * * ppPropNames,
                                             const char * * * ppPropData,
                                             void * * ppSurface )
    {
        Trace3DTest * pT3dTest = pT3dBufMgr->getT3dTest();
        IGpuSurfaceMgr * gsm = pT3dTest->GetSurfaceMgr();

        // first, find the "surfNum"-th valid surface
        // start looking in the surface manager
        //
        UINT32 lwrSurfNum = 0;
        MdiagSurf * pSurf = gsm->GetSurfaceHead( 0 );
        bool found = false;
        *pNumProperties = 0;
        while ( pSurf )
        {
            if (  gsm->GetValid( pSurf ) )
            {
                if ( lwrSurfNum == surfNum )
                {
                    found = true;
                    break;
                }
                ++ lwrSurfNum;
            }

            pSurf = gsm->GetSurfaceNext( 0 );
        }

        // now search in the trace module list for "stealth" surfaces
        //

        GpuTrace * pGpuTrace = pT3dTest->GetTrace();
        ModuleIter modIt = pGpuTrace->ModBegin();
        ModuleIter modIt_end = pGpuTrace->ModEnd();
        TraceModule * pTraceModule = 0;
        for ( ; !found && (modIt != modIt_end); ++modIt )
        {
            pTraceModule = *modIt;
            if ( pTraceModule->IsColorOrZ() )
            {
                // this buffer is already managed by the surface manager,
                // so we've already counted it above
                //
                continue;
            }

            MdiagSurf * pS2d( pTraceModule->GetDmaBufferNonConst() );
            if ( pS2d && IsSurface(pS2d) )
            {
                // found one!  This is really a surface, even though it's in
                // the trace module list and not in the surface manager
                //
                if ( lwrSurfNum == surfNum )
                {
                    found = true;
                    pSurf = pS2d;
                    break;
                }
                ++ lwrSurfNum;
            }
        }

        if ( ! found )
            return;

        // we found the desired surface, now fill in its properties
        //
        *ppSurface = pSurf;

        // some TraceModule surfaces do not forward their names into the MdiagSurf object,
        // so in those cases get the name from the trace module
        //
        static string name;
        if ( pTraceModule )
            name = pTraceModule->GetName();
        else
            name = pSurf->GetName();
        *ppName = name.c_str();

        UINT32 property = 0;
        *pNumProperties = GetSurfaceProperties( property,
                                                pSurf,
                                                pTraceModule,
                                                ppPropNames,
                                                ppPropData );
    }

    UINT32 BufferManager_GetNumBuffers( BufferManagerMods * pT3dBufMgr )
    {
        BufferManager::BufferIterator it    = pT3dBufMgr->begin();
        BufferManager::BufferIterator end   = pT3dBufMgr->end();
        UINT32 numBuffers = 0;

        // this is a bit clunky, but the original interface only exposed
        // begin() and end().  Should eventually replace this with
        // explicit "get" function on the interface to get a ref to the
        // entire container.
        //
        for ( ; it != end; ++it )
        {
            numBuffers += 1;
        }

        return numBuffers;
    }

    void BufferManager_GetBufferProperties( BufferManagerMods * pT3dBufMgr,
                                            UINT32 bufNum,
                                            const char * * ppName,
                                            const char * * ppType,
                                            UINT32 * pSize,
                                            UINT32 * pNumProperties,
                                            const char * * * ppPropNames,
                                            const char * * * ppPropData,
                                            Buffer * * ppBuffer )
    {
        BufferManager::BufferIterator it = pT3dBufMgr->begin();
        std::advance( it, bufNum );

        // we found it, now fill in its properties
        //
        BufferMods * pBuffer = (BufferMods *) *it;
        *ppBuffer = pBuffer;

        TraceModule * pTraceModule( pBuffer->getTraceModule() );
        const MdiagSurf * pS2d( pTraceModule->GetDmaBuffer() );

        *ppName = pBuffer->getName();
        *ppType = pBuffer->getType();
        *pSize  = pBuffer->getSize();

        // this is really ugly, but the shim layer requires passing raw C data
        //
        const UINT32 maxProperties = 100;
        static const char * s_propNames[ maxProperties ];
        static const char * s_propData[ maxProperties ];

        *ppPropNames = s_propNames;
        *ppPropData  = s_propData;

        UINT32 property = 0;

        s_propNames[ property ] = "name";
        s_propData[ property++ ] = pBuffer->getName();

        s_propNames[ property ] = "type";
        s_propData[ property++ ] = pBuffer->getType();

        s_propNames[ property ] = "size";
        static string sizeStr;
        sizeStr = Utility::StrPrintf( "%d", pBuffer->getSize() );
        s_propData[ property++ ] = sizeStr.c_str();

        s_propNames[ property ] = "memHandle";
        static string memHandleStr;
        if ( pS2d )
            memHandleStr = Utility::StrPrintf( "%u", pS2d->GetMemHandle() );
        else
            memHandleStr = "0";
        s_propData[ property++ ] = memHandleStr.c_str();

        *pNumProperties = property;
    }

    bool Surface_CreateSizedPitchImageUsingDma( BufferManagerMods * pT3dBufMgr,
                                           MdiagSurf * pSurf, UINT08 * pData,
                                           size_t bufSize, UINT32 subdev );

    // Note that "Surface" in this case is the mods surface --
    // not the T3dPlugin::Surface object.   They used to be called
    // "LWSurface" but were renamed  to "Surface", so there's a conflict
    // (T3dPlugin::Surface will have to be explicitly qualified)
    // Update: since all MODS "Surface" types are typedefs for MdiagSurf,
    // just explicitly name them MdiagSurf here so that there is no confusion
    // with T3dPlugin::Surface.
    //
    // Surface_WriteTga now produces the same tga files as the GpuVerif's
    // -dumpImages argument does, for easier comparison of plugin-dumped
    // TGA files versus plain mods/mdiag/trace_3d TGA files.  It uses
    // Buf::SaveBufTga instead of MdiagSurf::WriteTga.
    //
    bool Surface_WriteTga( BufferManagerMods * pHostBufferManager,
                           MdiagSurf * pSurf, const char * filename,
                           UINT32 subdev )
    {
        Trace3DTest * pT3dTest( pHostBufferManager->getT3dTest() );
        IGpuSurfaceMgr * gsm = pT3dTest->GetSurfaceMgr();

        Buf buf;
        buf.SetConfigBuffer( gsm->GetBufferConfig() );
        UINT32 depth = pSurf->GetBytesPerPixel();
        int width = pSurf->GetWidth();
        int height = pSurf->GetHeight() * pSurf->GetDepth() *
                                                 pSurf->GetArraySize();
        UINT32 pitch = pSurf->GetWidth() * pSurf->GetBytesPerPixel();
        ColorUtils::Format colorFormat = pSurf->GetColorFormat();
        RasterFormat rasformat = RASTER_LW50;
        LW50Raster* pRaster = gsm->GetRaster( colorFormat );

        if( ! pRaster)
        {
            ErrPrintf( "Surface_WriteTga: GetRaster failed: unsupported colorformat %s , filename %s\n",
                      ColorUtils::FormatToString(colorFormat).c_str(), filename);
            return false;
        }

        size_t bufSize( (size_t) pSurf->GetSize() );
        vector< UINT08 > data( bufSize );
        UINT08 * pData( &data[0] );
        bool rv( Surface_CreateSizedPitchImageUsingDma( pHostBufferManager, pSurf,
                                                  pData, bufSize, subdev ) );
        if ( ! rv )
        {
            ErrPrintf( "Surface_WriteTga: internal error: CreatePitchImage "
                        "failed\n" );
            return false;
        }

        const char * key = "nokey";
        int ret = buf.SaveBufTGA( filename, width, height,
                                  (uintptr_t) &data[0], pitch, depth,
                                  CTRaster(rasformat, pRaster), 0, key );
        if ( ret == 0 )
        {
            ErrPrintf( "Surface_WriteTga: buf.SaveBufTGA returned error\n" );
            return false;
        }
        return true;
    }

    bool Surface_WritePng( ::MdiagSurf* pSurf, const char * filename,
                           UINT32 subdev )
    {
        RC rc = pSurf->WritePng( filename, subdev );
        if ( rc == OK )
            return true;
        return false;
    }

    bool Surface_CreatePitchImage( MdiagSurf * pSurf,
                                   UINT08 * pData,
                                   UINT32 subdev )
    {
        MASSERT(!"Surface_CreatePitchImage is deprecated! Please sync TOT t3dplugin and rebuild\n");
        return false;
    }

    bool Surface_CreateSizedPitchImage( MdiagSurf * pSurf,
                                   UINT08 * pData,
                                   size_t bufSize,
                                   UINT32 subdev )
    {
        RC rc = pSurf->CreatePitchImage( pData, bufSize, subdev );
        if ( rc == OK )
            return true;
        return false;
    }

    bool Surface_Fill( ::MdiagSurf* pSurf, UINT32 value, UINT32 subdev )
    {
        RC rc = pSurf->Fill( value, subdev );
        if ( rc == OK )
            return true;
        return false;
    }

    bool Surface_FillRect( ::MdiagSurf* pSurf, UINT32 x, UINT32 y,
                           UINT32 width, UINT32 height, UINT32 value,
                           UINT32 subdev )
    {
        RC rc = pSurf->FillRect( x, y, width, height, value, subdev );
        if ( rc == OK )
            return true;
        return false;
    }

    bool Surface_FillRect64( ::MdiagSurf* pSurf, UINT32 x, UINT32 y,
                             UINT32 width, UINT32 height,
                             UINT32 r, UINT32 g, UINT32 b, UINT32 a,
                             UINT32 subdev )
    {
        RC rc = pSurf->FillRect64( x, y, width, height, r, g, b, a, subdev );
        if ( rc == OK )
            return true;
        return false;
    }

    UINT32 Surface_ReadPixel( ::MdiagSurf* pSurf, UINT32 x, UINT32 y )
    {
        if ( ! pSurf->IsMapped() )
            pSurf->Map( 0 );
        return pSurf->ReadPixel( x, y );
    }

    void Surface_WritePixel( ::MdiagSurf* pSurf, UINT32 x, UINT32 y, UINT32 value )
    {
        if ( ! pSurf->IsMapped() )
            pSurf->Map( 0 );
        pSurf->WritePixel( x, y, value );
    }

    // added in major version 1, minor version 97
    //
    void* Surface_CreateSegmentList(MdiagSurf* pSurf, char* which, UINT64 offset)
    {
        MdiagSegList* pSegList;

        RC rc(pSurf->CreateSegmentList(which, offset, &pSegList));

        return (OK==rc) ? pSegList : nullptr;
    }

    void Host_sendMessage( HostMods * pT3dHost, const char * msg, UINT32 value )
    {
        pT3dHost->sendMessage( msg, value );
    }
    VoidApiReceiver3<HostMods *, const char *, UINT32> Host_sendMessage_Receiver(Host_sendMessage);

    UINT32 TraceManager_GetNumChannels( TraceManagerMods * pT3dTraceMgr )
    {
        Trace3DTest * pT3dTest = pT3dTraceMgr->getT3dTest();
        const GpuTrace::TraceChannelObjects * pTraceChannels =
                                       pT3dTest->GetTrace()->GetChannels();
        return pTraceChannels->size();
    }
    RTApiReceiver1<UINT32, TraceManagerMods *> TraceManager_GetNumChannels_Receiver(TraceManager_GetNumChannels);

    void TraceManager_GetChannelProperties( TraceManagerMods * pT3dTraceMgr,
                                            UINT32 channelNum,
                                            const char * * pName,
                                            UINT32 * pNumSubChannels,
                                            UINT32 * * pSubChNums,
                                            UINT32 * * pSubChHwClasses,
                                            UINT32* pVEID)
    {
        Trace3DTest * pT3dTest = pT3dTraceMgr->getT3dTest();
        GpuTrace::TraceChannelObjects *
            pTraceChannels = pT3dTest->GetTrace()->GetChannels();

        if ( channelNum >= pTraceChannels->size() )
            return;
        GpuTrace::TraceChannelObjects::iterator it = pTraceChannels->begin();
        std::advance( it, channelNum );
        TraceChannel & channel = *it;

        static string channelName;
        channelName = channel.GetName();
        *pName = channelName.c_str();
        shared_ptr<SubCtx> pSubCtx = channel.GetCh()->GetSubCtx();
        if (pVEID && pSubCtx.get() && pSubCtx->IsVeidValid())
        {
            *pVEID = pSubCtx->GetVeid();
            DebugPrintf("%s: Channel %s, subctx veid %d.\n",
                __FUNCTION__,
                *pName, *pVEID);
        }

        // encode the subchannels of a channel as the property list of
        // that channel.   The "property name" is the subchannel number,
        // and the "property value" is the HW class in that subchannel
        //

        // this is really ugly, but the shim layer requires passing raw C data
        //
        const UINT32 maxSubChannels = 8;
        static UINT32 s_subchNums[ maxSubChannels ];
        static UINT32 s_subchHwClasses[ maxSubChannels ];

        UINT32 subchNum = 0;

        if ( channel.HasMultipleSubChs() )
        {
            // there are multiple subchannels in this channel.  The
            // in-trace subchannel # is significant, it is used in the
            // sendmethodsegment code to select the proper replay-time
            // subchannel to send methods to, via the TraceChannel's
            // m_SubChList list.
            // This loop reads out the contents of the m_SubChList list to
            // form the plugin's subchannel -> hwclass mapping for this
            // channel
            // addendum: while the in-trace subchannels is indeed used, it is
            // used to look-up the actual run-time subchannel structure.  Thus
            // the run-time subchannel number need not match the in-trace
            // subchannel number, and in fact usually doesn't!
            // The TraceManager_GetSubchannelProperties() function returns a
            // property list that maps trace subchannel to replay subchannel
            //
            for ( UINT32 sc = 0; sc < 8; ++sc )
            {
                UINT32 hwclass = channel.GetTraceSubChannelHwClass( sc );
                if ( hwclass != 0 )
                {
                    s_subchNums[ subchNum ] = sc;
                    s_subchHwClasses[ subchNum ] = hwclass;
                    ++subchNum;
                }
            }
        }
        else
        {
            // there is only one subchannel in the trace: and trace_3d
            // completely ignores trace-file pushbuffer subchannel number: while
            // replaying the pushbuffer data,
            // the trace methods could reference any subchannel number and
            // trace_3d would still write to its one subchannel, so in this
            // case, to mimic this "pushbuffer subchannel # doesn't matter"
            // behavior, we create a subchnumber -> hwclass map where all subchannel
            // numbers map to the channel's one and only hwclass number.
            //
            TraceSubChannel * ptracesubch = channel.GetSubChannel( "" );

            if (!ptracesubch)
            {
                *pNumSubChannels = 0;
                return;
            }
            UINT32 hwclass = ptracesubch->GetClass();
            for ( UINT32 sc = 0; sc < 8; ++sc )
            {
                s_subchNums[ subchNum ] = sc;
                s_subchHwClasses[ subchNum ] = hwclass;
                ++subchNum;
            }
        }

#if 0
        TraceSubChannelList::iterator itsc = channel.SubChBegin();
        TraceSubChannelList::iterator itsc_end = channel.SubChEnd();
        for ( ; itsc != itsc_end; ++itsc, ++subchNum )
        {
            if ( subchNum > 7 )
                return;

            TraceSubChannel * pSubch = *itsc;
            s_subchNums[ subchNum ] = pSubch->GetSubChNum();
            s_subchHwClasses[ subchNum ] = pSubch->GetClass();
        }
#endif

        *pNumSubChannels = subchNum;
        *pSubChNums = &s_subchNums[ 0 ];
        *pSubChHwClasses = &s_subchHwClasses[ 0 ];
    }

    class TraceManager_GetChannelProperties_Receiver_Class : public ApiReceiver
    {
    public:
        void operator()(MailBox* pluginMailBox, MailBox* traceMailBox)
        {
            TraceManagerMods* pT3dTraceMgr = traceMailBox->Read<TraceManagerMods*>();
            UINT32 ch = traceMailBox->Read<UINT32>();

            traceMailBox->Release();

            const char* name;
            UINT32 numSubch;
            UINT32* pSubChNums;
            UINT32* pSubChHwClasses;
            UINT32 veid = SubCtx::VEID_END;
            TraceManager_GetChannelProperties(pT3dTraceMgr, ch,
                &name, &numSubch, &pSubChNums, &pSubChHwClasses, &veid);

            pluginMailBox->Acquire();

            pluginMailBox->ResetDataOffset();
            pluginMailBox->Write(name);
            pluginMailBox->Write(numSubch);
            // Why are we writing these one by one rather than doing block copies of each array?
            for (UINT32 i = 0; i < numSubch; ++i)
            {
                pluginMailBox->Write(pSubChNums[i]);
                pluginMailBox->Write(pSubChHwClasses[i]);
            }
            // SubCtx related properties.
            // Only VEID for now.
            pluginMailBox->Write(veid);

            pluginMailBox->WriteCommand(MailBox::CMD_T3DAPI_ACK);

            pluginMailBox->Send();
        }
    }TraceManager_GetChannelProperties_Receiver;

    UINT32 *
    Buffer_Get032Addr( BufferMods * pBuffer, UINT32 subdev, UINT32 offset )
    {
        TraceModule * pTraceModule = pBuffer->getTraceModule();
        UINT32 * p = pTraceModule->Get032Addr( offset, subdev );
        return p;
    }

    int UtilityManager_EscapeWrite( UtilityManager * pUm,
                                    const char * Path, UINT32 Index,
                                    UINT32 Size, UINT32 Value )
    {
        return Platform::EscapeWrite( Path, Index, Size, Value );
    }

    int UtilityManager_EscapeRead( UtilityManager * pUm,
                                   const char * Path, UINT32 Index,
                                   UINT32 Size, UINT32 * Value )
    {
        return Platform::EscapeRead( Path, Index, Size, Value );
    }

    int UtilityManager_GpuEscapeWriteBuffer( UtilityManager * pUm,
                                    UINT32 GpuId, const char *Path, UINT32 Index,
                                    size_t Size, const void* Buf )
    {
        return Platform::GpuEscapeWriteBuffer( GpuId, Path, Index, Size, Buf );
    }

    int UtilityManager_GpuEscapeReadBuffer( UtilityManager * pUm,
                                    UINT32 GpuId, const char *Path, UINT32 Index,
                                    size_t Size, void *Buf )
    {
        return Platform::GpuEscapeReadBuffer( GpuId, Path, Index, Size, Buf);
    }

    int UtilityManager_ExitMods(UtilityManager * pT3dUtilityMgr)
    {
        return pT3dUtilityMgr->ExitMods();
    }

    // return the offset of an n-dimensional register given the name of the
    // register as a string, the expected dimensionality of the register,
    // and the (up to 2) indices into the register address space
    // returns false if the register name doesn't exist, or if the register
    // exists but the dimensionality != input dimensionality
    //
    bool Gpu_GetRefmanRegOffset( GpuMods * pTrace3dGpu, const char * regName,
                                 int dimensionality, UINT32 * offset,
                                 int i, int j, const char *regSpace)
    {
        GpuSubdevice * pSubdev( pTrace3dGpu->getSubdev() );
        RefManual * pRefMan( pSubdev->GetRefManual() );
        if ( ! pRefMan )
        {
            ErrPrintf( "Gpu_GetRefmanRegOffset: pRefMan is null\n" );
            return false;
        }
        const RefManualRegister * pReg( pRefMan->FindRegister( regName ) );
        if ( ! pReg )
        {
            ErrPrintf( "Gpu_GetRefmanRegOffset: couldn't find register '%s' "
                       "in refman\n", regName );
            return false;
        }
        if ( pReg->GetDimensionality() != dimensionality )
        {
            ErrPrintf( "input dimensionality (%d) != refman dimensionality "
                       " (%d)\n", dimensionality, pReg->GetDimensionality() );
            return false;
        }
        UINT32 refOffset = 0;
        switch( dimensionality )
        {
        case 0:
            refOffset = pReg->GetOffset();
            break;
        case 1:
            refOffset = pReg->GetOffset( i );
            break;
        case 2:
            refOffset = pReg->GetOffset( i, j );
            break;
        default:
            ErrPrintf( "Gpu_GetRefmanRegOffset: bad dimensionality (%d)\n",
                       dimensionality );
            return false;
        }

        LwRm* pLwRm = pTrace3dGpu->GetLwRmPtr();
        UINT32 priBase = MDiagUtils::GetDomainBase(regName, regSpace, pSubdev, pLwRm);
        *offset = priBase + refOffset;

        return true;
    }

    void Surface_MapMemory( MdiagSurf * pLWsurf, UINT32 offset,
                            UINT32 bytesToMap, UINT64 * virtAddr,
                            UINT32 gpuSubdevIdx )
    {
        RC rc( pLWsurf->MapRegion( offset, bytesToMap, gpuSubdevIdx ) );
        if ( rc != OK )
        {
            ErrPrintf( "t3dplugin: Surface_MapMemory: MapRegion failed\n" );
            *virtAddr = 0;
            return;
        }
        *virtAddr = (UINT64) pLWsurf->GetAddress();
    }

    class Surface_MapMemory_Receiver_Class : public ApiReceiver
    {
    public:
        void operator ()(MailBox *pluginMailBox, MailBox *traceMailBox)
        {
            // No reset.
            MdiagSurf *pLWsurf =  traceMailBox->Read<MdiagSurf *>();
            UINT32 offset =  traceMailBox->Read<UINT32>();
            UINT32 bytesToMap =  traceMailBox->Read<UINT32>();
            UINT32 gpuSubdevIdx =  traceMailBox->Read<UINT32>();
            UINT64 virtAddr = 0;

            traceMailBox->Release();

            Surface_MapMemory(pLWsurf, offset, bytesToMap, &virtAddr, gpuSubdevIdx);

            pluginMailBox->Acquire();

            pluginMailBox->ResetDataOffset();
            pluginMailBox->Write(virtAddr);
            pluginMailBox->WriteCommand(MailBox::CMD_T3DAPI_ACK);

            pluginMailBox->Send();
        }
    } Surface_MapMemory_Receiver;

    void Surface_UnmapMemory( MdiagSurf * pLWsurf, UINT64 virtAddr,
                             UINT32 gpuSubdevIdx )
    {
        pLWsurf->Unmap();
    }
    VoidApiReceiver3<MdiagSurf *, UINT64, UINT32> Surface_UnmapMemory_Receiver(Surface_UnmapMemory);

    UINT64 Surface_GetSizeInBytes( MdiagSurf * pLWsurf )
    {
        UINT64 rv( pLWsurf->GetSize() );
        return rv;
    }
    RTApiReceiver1<UINT64, MdiagSurf *> Surface_GetSizeInBytes_Receiver(Surface_GetSizeInBytes);

    bool Surface_ReadSurface( MdiagSurf * pLWsurf, UINT64 Offset, void * Buf,
                              size_t BufSize, UINT32 Subdev )
    {
        RC rc( SurfaceUtils::ReadSurface( *(pLWsurf->GetSurf2D()), Offset, Buf, BufSize,
                                           Subdev ) );
        bool rv( rc == OK );
        return rv;
    }

    bool Surface_WriteSurface( MdiagSurf * pLWsurf, UINT64 Offset,
                               const void * Buf, size_t BufSize,
                               UINT32 Subdev )
    {
        RC rc( SurfaceUtils::WriteSurface( *(pLWsurf->GetSurf2D()), Offset, Buf, BufSize,
                                            Subdev ) );
        bool rv( rc == OK );
        return rv;
    }

    void * TraceManager_GetTraceChannelPtr( TraceManagerMods * pT3dTraceMgr,
                                            const char * pChannelName )
    {
        Trace3DTest * pT3dTest = pT3dTraceMgr->getT3dTest();
        GpuTrace::TraceChannelObjects *
            pTraceChannels = pT3dTest->GetTrace()->GetChannels();
        GpuTrace::TraceChannelObjects::iterator it = pTraceChannels->begin();
        string channelName( pChannelName );

        for ( ; it != pTraceChannels->end(); ++it )
        {
            TraceChannel & channel( *it );
            if ( channelName == channel.GetName() )
            {
                void * rv( &channel );
                return rv;
            }
        }

        return 0;
    }

    RTApiReceiver2<void *, TraceManagerMods *, const char *> TraceManager_GetTraceChannelPtr_Receiver(TraceManager_GetTraceChannelPtr);

    int TraceManager_MethodWrite( void * pTraceChannel,
                                  int subchannel,
                                  UINT32 method,
                                  UINT32 data )
    {
        TraceChannel * pTc( (TraceChannel *) pTraceChannel );
        LWGpuChannel * pLwgpuChannel( pTc->GetCh() );
        int rv( pLwgpuChannel->MethodWrite( subchannel, method, data ) );
        return rv;
    }
    RTApiReceiver4<int, void *, int, UINT32, UINT32> TraceManager_MethodWrite_Receiver(TraceManager_MethodWrite);

    int TraceManager_SetObject( void * pTraceChannel, int subch, UINT32 handle)
    {
        TraceChannel * pTc( (TraceChannel *) pTraceChannel );
        LWGpuChannel * pLwgpuChannel( pTc->GetCh() );
        int rv( pLwgpuChannel->SetObject( subch, handle ) );
        return rv;

    }

    UINT32 TraceManager_CreateObject( void * pTraceChannel,
                                     UINT32 Class,
                                     void *Params,
                                     UINT32 *retClass )
    {
        TraceChannel * pTc( (TraceChannel *) pTraceChannel );
        LWGpuChannel * pLwgpuChannel( pTc->GetCh() );
        UINT32 rv( pLwgpuChannel->CreateObject( Class, Params, retClass ) );
        return rv;
    }

    void TraceManager_DestroyObject( void * pTraceChannel, UINT32 handle )
    {
        TraceChannel * pTc( (TraceChannel *) pTraceChannel );
        LWGpuChannel * pLwgpuChannel( pTc->GetCh() );
        pLwgpuChannel->DestroyObject( handle );
    }

    bool TraceManager_GetChannelRefCount( void * pTraceChannel, UINT32 * pValue )
    {
        TraceChannel * pTc( (TraceChannel *) pTraceChannel );
        LWGpuChannel * pLwgpuChannel( pTc->GetCh() );
        Channel * pModsChannel( pLwgpuChannel->GetModsChannel() );
        
        if (pModsChannel->GetGpuDevice()->GetFamily() >= GpuDevice::Blackwell)
        {
            ErrPrintf("GetChannelRefCount is not supported since Blackwell.\n");
            return false;
        }

        RC rc( pModsChannel->GetReference( pValue ) );
        if ( OK != rc )
            return false;
        return true;
    }

    bool Mem_LwRmMapMemory( Mem * pT3dMem,
                            UINT32 MemoryHandle,
                            UINT64 Offset,
                            UINT64 Length,
                            void  ** pAddress )
    {
        RC rc( pT3dMem->GetLwRmPtr()->MapMemory( MemoryHandle, Offset, Length, pAddress, 0, pT3dMem->GetGpuDevice()->GetSubdevice(0) ) );
        if ( rc != OK )
            return false;
        return true;
    }

    void Mem_LwRmUnmapMemory( Mem * pT3dMem,
                              UINT32 MemoryHandle,
                              volatile void * Address )
    {
        pT3dMem->GetLwRmPtr()->UnmapMemory( MemoryHandle, Address, 0, pT3dMem->GetGpuDevice()->GetSubdevice(0) );
    }

    bool Mem_LwRmMapMemoryDma( Mem * pT3dMem,
                               UINT32 hDma,
                               UINT32 hMemory,
                               UINT64 offset,
                               UINT64 length,
                               UINT32 flags,
                               UINT64 * pDmaOffset )
    {
        RC rc( pT3dMem->GetLwRmPtr()->MapMemoryDma( hDma, hMemory, offset, length, flags,
                                    pDmaOffset, pT3dMem->GetGpuDevice() ) );
        if ( rc != OK )
            return false;
        return true;
    }

    void Mem_LwRmUnmapMemoryDma( Mem * pT3dMem,
                                 UINT32 hDma,
                                 UINT32 hMemory,
                                 UINT32 flags,
                                 UINT64 dmaOffset )
    {
        pT3dMem->GetLwRmPtr()->UnmapMemoryDma( hDma, hMemory, flags, dmaOffset, pT3dMem->GetGpuDevice() );
    }

    bool Mem_LwRmAllocOnDevice( Mem * pT3dMem,
                                UINT32 * pRtnObjectHandle,
                                UINT32 Class,
                                void * Parameters )
    {
        LwRm* pLwRm = pT3dMem->GetLwRmPtr();
        RC rc( pLwRm->Alloc( pLwRm->GetDeviceHandle(pT3dMem->GetGpuDevice()), pRtnObjectHandle,
                             Class, Parameters ) );
        if ( rc != OK )
            return false;
        return true;
    }

    void Mem_LwRmFree( Mem * pT3dMem, UINT32 handle )
    {
        pT3dMem->GetLwRmPtr()->Free( handle );
    }

    // TraceManager_GetSubchannelProperties returns a property list that
    // maps in-trace subchannel numbers to replay time subchannel numbers
    // for a particular channel number.
    //
    void TraceManager_GetSubchannelProperties( TraceChannel * pTraceChannel,
                                               UINT32 * pNumProperties,
                                               const char * * * ppPropNames,
                                               const char * * * ppPropData )
    {
        // this is really ugly, but the shim layer requires passing raw C data
        //
        const UINT32 maxProperties = 100;
        static const char * s_propNames[ maxProperties ];
        static const char * s_propData[ maxProperties ];

        *ppPropNames = s_propNames;
        *ppPropData  = s_propData;
        UINT32 property( 0 );

        s_propNames[ property ] = "hasMultiSubch";
        static string hasMultiSubchStr;
        hasMultiSubchStr = Utility::StrPrintf( "%d", pTraceChannel->HasMultipleSubChs() );
        s_propData[ property ] = hasMultiSubchStr.c_str();

        TraceSubChannel * ptracesubch( 0 );
        LWGpuSubChannel * pSubch( 0 );
        UINT32 subchNum( 0 );

         if ( pTraceChannel->HasMultipleSubChs() )
         {
             // add a property for each existing subchannel that maps trace
             // subchannel to replay subchannel
             //
             const UINT32 numSubchannels( 8 );
             static string subChMapPropName[ numSubchannels ];
             static string subChMapPropData[ numSubchannels ];

             for ( UINT32 i = 0; i < numSubchannels; ++i )
             {
                 ptracesubch = pTraceChannel->GetSubChannel( i, false );
                 if ( ! ptracesubch )
                     continue;
                 pSubch = ptracesubch->GetSubCh();
                 MASSERT( pSubch );
                 subchNum = pSubch->subchannel_number();
                 subChMapPropName[ i ] = Utility::StrPrintf( "subch_%d", i );
                 subChMapPropData[ i ] = Utility::StrPrintf( "%d", subchNum );

                 ++property;
                 s_propNames[ property ] = subChMapPropName[ i ].c_str();
                 s_propData[ property ] = subChMapPropData[ i ].c_str();
             }
         }
         else
         {
             // this trace has only a single subchannel
             //
             ptracesubch = pTraceChannel->GetSubChannel( "" );
             if (!ptracesubch)
             {
                *pNumProperties = property + 1;
                return;
             }
             pSubch = ptracesubch->GetSubCh();
             MASSERT( pSubch );
             subchNum = pSubch->subchannel_number();

             ++property;
             s_propNames[ property ] = "subch";
             static string subChStr;
             subChStr = Utility::StrPrintf( "%d", subchNum );
             s_propData[ property ] = subChStr.c_str();
         }

         // we must have recorded at least one subchannel property
         //
         MASSERT( property > 0 );

         *pNumProperties = property + 1;
    }

    T3dPlugin::SimulationMode UtilityManager_GetSimulationMode()
    {
        T3dPlugin::SimulationMode simMode( T3dPlugin::Hardware );
        switch ( Platform::GetSimulationMode() )
        {
        case Platform::Hardware:
            simMode = T3dPlugin::Hardware;
            break;
        case Platform::RTL:
            simMode = T3dPlugin::RTL;
            break;
        case Platform::Fmodel:
            simMode = T3dPlugin::Cmodel;
            break;
        case Platform::Amodel:
            simMode = T3dPlugin::Amodel;
            break;
        default:
            ErrPrintf( "UtilityManager_GetSimulationMode: unknown simulation "
                "mode: %d\n", Platform::GetSimulationMode() );
            break;
        }
        return simMode;
    }

    bool UtilityManager_IsEmulation(UtilityManager* pT3dUtilityMgr)
    {
        return pT3dUtilityMgr->isEmulation();
    }

    UINT32 Surface_GetPixelOffset( MdiagSurf * pSurf,
                                   UINT32 x, UINT32 y, UINT32 z, UINT32 a )
    {
        UINT32 rv = UNSIGNED_CAST(UINT32, pSurf->GetPixelOffset( x, y, z, a ));
        return rv;
    }

    // added in major version 1, minor version 4
    //
    void Buffer_SetProperty( BufferMods * pBuf,
                             const char * pPropName,
                             const char * pPropValue )
    {
        GenericTraceModule * pMod( (GenericTraceModule *)
                                                  pBuf->getTraceModule() );
        if ( ! pMod )
        {
            ErrPrintf( "trying to set property on buffer with null TraceModule"
                " (probably need to call createTraceBuffer first)\n" );
            MASSERT( ! "set property on buffer with null tracemodule" );
        }
        string propertyName( pPropName );
        string propertyValue( pPropValue );
        if ( propertyName == "permissions" )
        {
            Memory::Protect prot( Memory::Readable );
            if ( propertyValue == "r" )
                prot = Memory::Readable;
            else if ( propertyValue == "w" )
                prot = Memory::Writeable;
            else if ( propertyValue == "rw" )
                prot = Memory::ReadWrite;
            else if ( propertyValue == "e" )
                prot = Memory::Exelwtable;
            else
            {
                ErrPrintf( "unrecognized permissions string: '%s'\n",
                                                   propertyValue.c_str() );
                MASSERT( ! "unrecognized buffer permissions string" );
            }

            pMod->SetProtect( prot );
        }
        else
        {
            ErrPrintf( "T3Plugin::Buffer_SetProperty: unrecognized property "
                       "'%s' ignored\n", propertyName.c_str() );
            MASSERT( ! "unrecognized buffer property" );
        }
    }

    // added in major version 1, minor version 97
    //
    void* Buffer_CreateSegmentList(Buffer* pT3dBuffer, char* which, UINT64 offset)
    {
        return pT3dBuffer->CreateSegmentList(which, offset);
    }

    // added in major version 1, minor version 6
    //
    void Mem_FlushCpuWriteCombineBuffer()
    {
        Platform::FlushCpuWriteCombineBuffer();
    }

    // added in major version 1, minor version 7
    //
    bool Surface_ReadSurfaceUsingDma( BufferManagerMods * pT3dBufMgr,
                                      MdiagSurf * pLWsurf, UINT64 Offset,
                                      void * Buf, size_t BufSize,
                                      UINT32 Subdev )
    {
        Trace3DTest * pT3dTest( pT3dBufMgr->getT3dTest() );
        TraceChannel * pTraceChannel( pT3dTest->GetDefaultChannelByVAS(pLWsurf->GetGpuVASpace()) );
        GpuVerif * gpuverif( pTraceChannel->GetGpuVerif() );

        if ( ! gpuverif->GetDmaUtils()->UseDmaTransfer() )
        {
            // user has not indicated interest in DMA for moving data, use direct
            // CPU reads instead
            //
            return Surface_ReadSurface( pLWsurf, Offset, Buf, BufSize, Subdev );
        }

        if ( ! gpuverif->GetDmaUtils()->GetDmaReader() && ! gpuverif->GetDmaUtils()->SetupDmaBuffer() )
        {
            MASSERT(!"Failed to setup DMA buffer for readback\n");
        }

        auto pDmaSurfReader = SurfaceUtils::CreateDmaSurfaceReader(*pLWsurf,
            gpuverif->GetDmaUtils()->GetDmaReader());
        SurfaceUtils::ISurfaceReader* pSurfReader = pDmaSurfReader.get();

        if (OK != pSurfReader->ReadBytes(Offset, Buf, BufSize))
        {
            ErrPrintf( "Failed to read data for buffer %s\n", pLWsurf->GetName().c_str() );
            return false;
        }

        return true;
    }

    // added in major version 1, minor version 118
    //
    bool Surface_WriteSurfaceUsingDma( BufferManagerMods * pT3dBufMgr,
                                      MdiagSurf * pLWsurf, UINT64 Offset,
                                      void * Buf, size_t BufSize,
                                      UINT32 Subdev )
    {
        Trace3DTest * pT3dTest( pT3dBufMgr->getT3dTest() );
        TraceChannel * pTraceChannel( pT3dTest->GetDefaultChannelByVAS(pLWsurf->GetGpuVASpace()) );
        GpuVerif * gpuverif( pTraceChannel->GetGpuVerif() );
        if ( ! gpuverif->GetDmaUtils()->UseDmaTransfer() )
        {
            // user has not indicated interest in DMA for moving data, use direct
            // CPU write instead
            //
            return Surface_WriteSurface( pLWsurf, Offset, Buf, BufSize, Subdev );
        }

        if ( ! gpuverif->GetDmaUtils()->GetDmaReader() && ! gpuverif->GetDmaUtils()->SetupDmaBuffer())
        {
            MASSERT(!"Failed to setup DMA buffer \n");
        }

        RC rc = pLWsurf->DownloadWithCE(pT3dTest->GetParam(), gpuverif, (UINT08*)Buf, BufSize, Offset, false, Subdev);
        if (rc != OK)
        {
            ErrPrintf( "Failed to write data for buffer %s\n", pLWsurf->GetName().c_str() );
            return false;
        }

        return true;
    }

    // if the user has requested dma for image transfer (-dmaCheck), then use DMA
    // for reading the surface's data and creating the image from it, otherwise, just
    // use the regular MdiagSurf::CreatePitchImage.   Assumes the caller has
    // allocated width x height x depth x arraySize x bytesPerPixel bytes in "pData".
    //
    bool Surface_CreateSizedPitchImageUsingDma( BufferManagerMods * pT3dBufMgr,
                                           MdiagSurf * pSurf,
                                           UINT08 * pData,
                                           size_t bufSize,
                                           UINT32 subdev )
    {
        Trace3DTest * pT3dTest( pT3dBufMgr->getT3dTest() );
        TraceChannel * pTraceChannel = pT3dTest->GetDefaultChannelByVAS(pSurf->GetGpuVASpace());
        GpuVerif * gpuverif = pTraceChannel->GetGpuVerif();

        if ( ! gpuverif->GetDmaUtils()->UseDmaTransfer() )
        {
            // user has not indicated interest in DMA for moving data, use direct
            // CPU reads instead
            //
            RC rc = pSurf->CreatePitchImage( pData, bufSize, subdev );
            if ( rc == OK )
                return true;
            return false;
        }

        if ( ! gpuverif->GetDmaUtils()->GetDmaReader() && ! gpuverif->GetDmaUtils()->SetupDmaBuffer() )
        {
            MASSERT(!"Failed to setup DMA buffer for readback\n");
        }

        auto pDmaSurfReader = SurfaceUtils::CreateDmaSurfaceReader(*pSurf,
            gpuverif->GetDmaUtils()->GetDmaReader());
        SurfaceUtils::ISurfaceReader* pSurfReader = pDmaSurfReader.get();

        vector<UINT08> data( static_cast<size_t>( pSurf->GetSize() ) );
        if (OK != pSurfReader->ReadBytes(0, &data[0], pSurf->GetSize()))
        {
            ErrPrintf( "Failed to fetch data buffer %s\n", pSurf->GetName().c_str() );
            return false;
        }

        UINT32 BytesPerPixel = pSurf->GetBytesPerPixel();
        // Colwert to pitch format and throw away the native-format image
        for (UINT32 a = 0; a < pSurf->GetArraySize(); a++)
        {
            for (UINT32 z = 0; z < pSurf->GetDepth(); z++)
            {
                for (UINT32 y = 0; y < pSurf->GetHeight(); y++)
                {
                    for (UINT32 x = 0; x < pSurf->GetWidth(); x++)
                    {
                        size_t Offset = UNSIGNED_CAST(size_t, pSurf->GetPixelOffset( x, y, z, a ));
                        const UINT08 *pSrc = &data[ Offset ];
                        memcpy( pData, pSrc, BytesPerPixel );
                        pData += BytesPerPixel;
                    }
                }
            }
        }

        return true;
    }

    bool Surface_CreatePitchImageUsingDma( BufferManagerMods * pT3dBufMgr,
                                           MdiagSurf * pSurf,
                                           UINT08 * pData,
                                           UINT32 subdev )
    {
        MASSERT(!"Surface_CreatePitchImage is deprecated! Please sync TOT t3dplugin and rebuild\n");
        return false;
    }

    UINT64 SegmentList_GetLength(MdiagSegList* pSeg)
    {
        return pSeg->GetLength();
    }

    Surface::Location SegmentList_GetLocation(MdiagSegList* pSeg, UINT32 n)
    {
        return Surface::Location(pSeg->GetLocation(n));
    }

    UINT64 SegmentList_GetSize(MdiagSegList* pSeg, UINT32 n)
    {
        return pSeg->GetSize(n);
    }

    UINT64 SegmentList_GetPhysAddress(MdiagSegList* pSeg, UINT32 n)
    {
        return pSeg->GetPhysAddress(n);
    }

    UINT64 SegmentList_Map(MdiagSegList* pSeg, UINT32 n)
    {
        return pSeg->Map(n);
    }

    bool SegmentList_IsMapped(MdiagSegList* pSeg, UINT32 n)
    {
        return pSeg->IsMapped(n);
    }

    UINT64 SegmentList_GetAddress(MdiagSegList* pSeg, UINT32 n)
    {
        return (UINT64)(pSeg->GetAddress(n));
    }

    void SegmentList_Unmap(MdiagSegList* pSeg, UINT32 n)
    {
        pSeg->Unmap(n);
    }

    bool Buffer_ReadUsingDma( Buffer * pT3dBuffer, UINT32 offset, UINT32 size,
                              UINT08 * pData, UINT32 gpuSubdevIdx )
    {
        return pT3dBuffer->ReadUsingDma( offset, size, pData, gpuSubdevIdx );
    }

    UINT32 UtilityManager_AllocEvent( UtilityManager * pT3dUtilityMgr,
                                      UINT32* hEvent )
    {
        return pT3dUtilityMgr->AllocEvent(hEvent);
    }

    UINT32 UtilityManager_FreeEvent( UtilityManager * pT3dUtilityMgr,
                                     UINT32 hEvent )
    {
        return pT3dUtilityMgr->FreeEvent(hEvent);
    }

    UINT32 UtilityManager_ResetEvent( UtilityManager * pT3dUtilityMgr,
                                     UINT32 hEvent )
    {
        return pT3dUtilityMgr->ResetEvent(hEvent);
    }

    UINT32 UtilityManager_SetEvent( UtilityManager * pT3dUtilityMgr,
                                     UINT32 hEvent )
    {
        return pT3dUtilityMgr->SetEvent(hEvent);
    }

    UINT32 UtilityManager_IsEventSet( UtilityManager * pT3dUtilityMgr,
                                     UINT32 hEvent, bool* isSet )
    {
        return pT3dUtilityMgr->IsEventSet(hEvent, isSet);
    }

    UINT32 UtilityManager_HookPmuEvent( UtilityManager * pT3dUtilityMgr,
                                     UINT32 hEvent, UINT32 index )
    {
        return pT3dUtilityMgr->HookPmuEvent(hEvent, index);
    }

    UINT32 UtilityManager_UnhookPmuEvent( UtilityManager * pT3dUtilityMgr,
                                     UINT32 index )
    {
        return pT3dUtilityMgr->UnhookPmuEvent(index);
    }

    void UtilityManager_FailTest( UtilityManager * pT3dUtilityMgr, UtilityManager::TestStatus status )
    {
        if (status != T3dPlugin::UtilityManager::TEST_SUCCEEDED)
            ErrPrintf("FailTest is called from plugin side.\n");
        else
            InfoPrintf("FailTest is called from plugin side with status TEST_SUCCEEDED.\n");
        pT3dUtilityMgr->FailTest(status);
    }
    VoidApiReceiver2<UtilityManager *, UtilityManager::TestStatus> UtilityManager_FailTest_Receiver(UtilityManager_FailTest);

    UINT64 UtilityManager_AllocRawSysMem(UtilityManager * pT3dUtilityMgr, UINT64 nbytes)
    {
        return pT3dUtilityMgr->AllocRawSysMem(nbytes);
    }

    void UtilityManager_TriggerUtlUserEvent(UtilityManager * pT3dUtilityMgr, 
                                            UINT32 numUserData,
                                            const char * * pUserDataKeys,
                                            const char * * pUserDataValues)
    {
        std::map<std::string, std::string> userData;
        for (UINT32 i = 0; i < numUserData; ++i)
        {
            userData.emplace(pUserDataKeys[i], pUserDataValues[i]);
        }
        return pT3dUtilityMgr->TriggerUtlUserEvent(userData);
    }

    UINT32 UtilityManager_PciRead32( UtilityManager * pT3dUtilityMgr,
                                     INT32 DomainNumber,
                                     INT32 BusNumber, INT32 DeviceNumber,
                                     INT32 FunctionNumber, INT32 Address,
                                     UINT32* Data)
    {
        return pT3dUtilityMgr->PciRead32(DomainNumber, BusNumber, DeviceNumber,
                                         FunctionNumber, Address, Data);
    }

    UINT32 UtilityManager_PciWrite32( UtilityManager * pT3dUtilityMgr,
                                     INT32 DomainNumber,
                                     INT32 BusNumber, INT32 DeviceNumber,
                                     INT32 FunctionNumber, INT32 Address,
                                     UINT32 Data)
    {
        return pT3dUtilityMgr->PciWrite32(DomainNumber, BusNumber, DeviceNumber,
                                         FunctionNumber, Address, Data);
    }

    UINT32 UtilityManager_FindPciDevice
    (
        UtilityManager * pT3dUtilityMgr,
        INT32   DeviceId,
        INT32   VendorId,
        INT32   Index,
        UINT32* pDomainNumber,
        UINT32* pBusNumber,
        UINT32* pDeviceNumber,
        UINT32* pFunctionNumber
    )
    {
        return pT3dUtilityMgr->FindPciDevice(DeviceId, VendorId, Index, pDomainNumber,
                                             pBusNumber, pDeviceNumber, pFunctionNumber);
    }

    UINT32 UtilityManager_FindPciClassCode
    (
        UtilityManager * pT3dUtilityMgr,
        INT32   ClassCode,
        INT32   Index,
        UINT32* pDomainNumber,
        UINT32* pBusNumber,
        UINT32* pDeviceNumber,
        UINT32* pFunctionNumber
    )
    {
        return pT3dUtilityMgr->FindPciClassCode(ClassCode, Index, pDomainNumber,
                                                pBusNumber, pDeviceNumber, pFunctionNumber);
    }

    const char * TraceManager_getTraceHeaderFile( TraceManager* pT3dTraceMgr )
    {
        return pT3dTraceMgr->getTraceHeaderFile();
    }

    const char * TraceManager_getTestName( TraceManager* pT3dTraceMgr )
    {
        return pT3dTraceMgr->getTestName();
    }
    RTApiReceiver1<const char *, TraceManager *> TraceManager_getTestName_Receiver(TraceManager_getTestName);

    UINT32 Buffer_SendGpEntry( Buffer * pT3dBuffer,
                                     const char* chName,
                                     UINT64 offset, UINT32 size, bool sync)
    {
        return pT3dBuffer->SendGpEntry(chName, offset, size, sync);
    }

    UINT32 Buffer_WriteBuffer( Buffer * pT3dBuffer, UINT64 offset,
                      const void * buf, size_t size, UINT32 gpuSubdevIdx )
    {
        return pT3dBuffer->WriteBuffer(offset, buf, size, gpuSubdevIdx);
    }

    bool Mem_MapDeviceMemory
    (
        void**      pReturnedVirtualAddress,
        UINT64      PhysicalAddress,
        size_t      NumBytes,
        MemoryAttrib Attrib,
        MemoryProtect Protect
    )
    {
        Memory::Attrib memAttrib = Memory::UC;
        switch (Attrib)
        {
            case T3dPlugin::UC: memAttrib = Memory::UC; break;
            case T3dPlugin::WC: memAttrib = Memory::WC; break;
            case T3dPlugin::WT: memAttrib = Memory::WT; break;
            case T3dPlugin::WP: memAttrib = Memory::WP; break;
            case T3dPlugin::WB: memAttrib = Memory::WB; break;
        }
        Memory::Protect memProtect = Memory::Readable;
        switch (Protect)
        {
            case T3dPlugin::Readable: memProtect = Memory::Readable; break;
            case T3dPlugin::Writeable: memProtect = Memory::Writeable; break;
            case T3dPlugin::Exelwtable: memProtect = Memory::Exelwtable; break;
            case T3dPlugin::ReadWrite: memProtect = Memory::ReadWrite; break;
        }
        RC rc( Platform::MapDeviceMemory(
            pReturnedVirtualAddress,
            PhysicalAddress,
            NumBytes,
            memAttrib,
            memProtect ) );
        return ( rc == OK );
    }

    void Mem_UnMapDeviceMemory ( void* VirtualAddress )
    {
        Platform::UnMapDeviceMemory( VirtualAddress );
    }

    size_t UtilityManager_GetVirtualMemoryUsed()
    {
        return Xp::GetVirtualMemoryUsed();
    }

    INT32 UtilityManager_Printf(const char *Format, void *Args)
    {
        return ModsExterlwAPrintf(Tee::PriNormal, Tee::ModuleNone,
                                  Tee::SPS_NORMAL, Format,
                                  *((va_list *)Args));
    }
    class UtilityManager_Printf_Receiver_Class : public ApiReceiver
    {
    public:
        void operator ()(MailBox *pluginMailBox, MailBox *traceMailBox)
        {
            char *buf = traceMailBox->Read<char *>();

            traceMailBox->Release();

            INT32 ret = Printf(Tee::PriNormal, buf);

            pluginMailBox->Acquire();

            pluginMailBox->ResetDataOffset();
            pluginMailBox->Write(ret);
            pluginMailBox->WriteCommand(MailBox::CMD_T3DAPI_ACK);

            pluginMailBox->Send();
        }
    } UtilityManager_Printf_Receiver;

    const char *GpuManager_GetDeviceIdString(GpuManager *pTrace3dGpuManager)
    {
        return pTrace3dGpuManager->GetDeviceIdString();
    }

    const char *UtilityManager_GetOSString()
    {
        Xp::OperatingSystem os = Xp::GetOperatingSystem();
        switch (os)
        {
        case Xp::OS_NONE:
            return "NONE";
        case Xp::OS_WINDOWS:
            return "WINDOWS";
        case Xp::OS_LINUX:
            return "LINUX";
        case Xp::OS_LINUXRM:
            return "LINUXRM";
        case Xp::OS_MAC:
        case Xp::OS_MACRM:
            return "MACOSX";
        case Xp::OS_WINSIM:
            return "WINSIM";
        case Xp::OS_LINUXSIM:
            return "LINUXSIM";
        case Xp::OS_WINMFG:
            return "WINMFG";
        case Xp::OS_ANDROID:
            return "ANDROID";
        default:
            MASSERT(!"Shouldn't reach here.\n");
            return 0;
        }
    }

    UINT32 Buffer_getGpuPartitionIndex(Buffer *pT3dBuffer, UINT64 physAddr)
    {
        return pT3dBuffer->getGpuPartitionIndex(physAddr);
    }

    UINT32 Buffer_getGpuLtcIndex(Buffer *pT3dBuffer, UINT64 physAddr)
    {
        return pT3dBuffer->getGpuLtcIndex(physAddr);
    }

    UINT32 Buffer_getGpuL2SliceIndex(Buffer *pT3dBuffer, UINT64 physAddr)
    {
        return pT3dBuffer->getGpuL2SliceIndex(physAddr);
    }

    UINT32 Buffer_getGpuXbarRawAddr(Buffer *pT3dBuffer, UINT64 physAddr)
    {
        return pT3dBuffer->getGpuXbarRawAddr(physAddr);
    }

    UINT32 Buffer_getGpuSubpartition(Buffer *pT3dBuffer, UINT64 physAddr)
    {
        return pT3dBuffer->getGpuSubpartition(physAddr);
    }

    UINT32 Buffer_getGpuPCIndex(Buffer *pT3dBuffer, UINT64 physAddr)
    {
        return pT3dBuffer->getGpuPCIndex(physAddr);
    }

    UINT32 Buffer_getGpuRBCAddr(Buffer *pT3dBuffer, UINT64 physAddr, UINT32 errCount, UINT32 errPos)
    {
        return pT3dBuffer->getGpuRBCAddr(physAddr, errCount, errPos);
    }

    UINT32 Buffer_getGpuRBCAddrExt(Buffer *pT3dBuffer, UINT64 physAddr, UINT32 errCount, UINT32 errPos)
    {
        return pT3dBuffer->getGpuRBCAddrExt(physAddr, errCount, errPos);
    }

    UINT32 Buffer_getGpuRBCAddrExt2(Buffer *pT3dBuffer, UINT64 physAddr, UINT32 errCount, UINT32 errPos)
    {
        return pT3dBuffer->getGpuRBCAddrExt2(physAddr, errCount, errPos);
    }

    UINT32 Buffer_getEccInjectRegVal(Buffer *pT3dBuffer, UINT64 physAddr, UINT32 errCount, UINT32 errPos)
    {
        return pT3dBuffer->getEccInjectRegVal(physAddr, errCount, errPos);
    }

    UINT32 Buffer_getEccInjectExtRegVal(Buffer *pT3dBuffer, UINT64 physAddr, UINT32 errCount, UINT32 errPos)
    {
        return pT3dBuffer->getEccInjectExtRegVal(physAddr, errCount, errPos);
    }

    void Buffer_mapEccAddrToPhysAddr(Buffer *pT3dBuffer, const EccAddrParams &eccAddr, UINT64 *pPhysAddr)
    {
        pT3dBuffer->mapEccAddrToPhysAddr(eccAddr, pPhysAddr);
    }

    UINT64 Surface_getGpuPhysAddr( ::MdiagSurf* pSurf, UINT32 subdev )
    {
        return pSurf->GetVidMemOffset();
    }
    RTApiReceiver2<UINT64, ::MdiagSurf*, UINT32> Surface_getGpuPhysAddr_Receiver(Surface_getGpuPhysAddr);

    UINT32 Surface_getGpuPartitionIndex( ::MdiagSurf* pSurf, UINT64 physAddr )
    {
        FbDecode decode;
        GetFbDecode(decode, pSurf, physAddr);
        return decode.virtualFbio;
    }

    UINT32 Surface_getGpuLtcIndex( ::MdiagSurf* pSurf, UINT64 physAddr )
    {
        FbDecode decode;
        GetFbDecode(decode, pSurf, physAddr);
        return pSurf->GetGpuSubdev(0)->GetFB()->VirtualFbioToVirtualLtc(
                decode.virtualFbio, decode.subpartition);
    }

    UINT32 Surface_getGpuL2SliceIndex( ::MdiagSurf* pSurf, UINT64 physAddr )
    {
        FbDecode decode;
        GetFbDecode(decode, pSurf, physAddr);
        return decode.slice;
    }

    UINT32 Surface_getGpuXbarRawAddr( ::MdiagSurf* pSurf, UINT64 physAddr )
    {
        FbDecode decode;
        GetFbDecode(decode, pSurf, physAddr);
        return decode.xbarRawAddr;
    }

    void * BufferManager_CreateSurfaceView(BufferManager * pBufferManager,
                                           const char * name,
                                           const char * parent,
                                           const char * options)
    {
        return pBufferManager->CreateSurfaceView(name, parent, options);
    }

    void * BufferManager_AllocSurface(BufferManager * pBufferManager,
                                     const char * name,
                                     UINT64 sizeInBytes,
                                     UINT32 numProperties,
                                     const char * * pPropNames,
                                     const char * * pPropData)
    {
        std::map<std::string, std::string> properties;
        for (UINT32 i = 0; i < numProperties; ++i)
        {
            properties.insert(std::make_pair(pPropNames[i], pPropData[i]));
        }
        return pBufferManager->AllocSurface(name, sizeInBytes, properties);
    }

    bool Surface_IsSurfaceView(MdiagSurf * pSurf)
    {
        return pSurf->IsSurfaceView();
    }

    Surface* BufferManager_GetSurfaceByName(BufferManagerMods * pT3dBufMgr,
                                            const char * name,
                                            UINT32 * pNumProperties,
                                            const char * * * ppPropNames,
                                            const char * * * ppPropData)
    {
        Surface *pSurface = pT3dBufMgr->GetSurfaceByName(name);
        if (pSurface != NULL)
        {
            MdiagSurf* pHostSurf = reinterpret_cast<MdiagSurf*>(pSurface);

            Trace3DTest *pT3dTest = pT3dBufMgr->getT3dTest();
            TraceModule *pTraceModule = pT3dTest->GetTrace()->ModFind(name);

            UINT32 property = 0;
            *pNumProperties = GetSurfaceProperties(property,
                                                   pHostSurf,
                                                   pTraceModule,
                                                   ppPropNames,
                                                   ppPropData);
        }

        return pSurface;
    }
    class BufferManager_GetSurfaceByName_Receiver_Class : public ApiReceiver
    {
    public:
        void operator ()(MailBox *pluginMailBox, MailBox *traceMailBox)
        {
            // No reset.
            BufferManagerMods *pT3dBufMgr =  traceMailBox->Read<BufferManagerMods *>();
            char *name =  traceMailBox->Read<char *>();

            traceMailBox->Release();

            UINT32 nProperties = 0;
            const char **pPropNames, **pPropData;
            Surface *pSurface = BufferManager_GetSurfaceByName(pT3dBufMgr, name, &nProperties, &pPropNames, &pPropData);

            pluginMailBox->Acquire();

            pluginMailBox->ResetDataOffset();
            pluginMailBox->Write(pSurface);
            pluginMailBox->Write(nProperties);
            for (UINT32 i = 0; i < nProperties; ++i)
            {
                pluginMailBox->Write(pPropNames[i]);
                pluginMailBox->Write(pPropData[i]);
            }

            pluginMailBox->WriteCommand(MailBox::CMD_T3DAPI_ACK);

            pluginMailBox->Send();
        }
    } BufferManager_GetSurfaceByName_Receiver;

    UINT32 TraceManager_getTraceOpLineNumber(TraceManagerMods * pT3dTraceMgr,
                                             UINT32 traceOpNum)
    {
        GpuTrace* pTrace = pT3dTraceMgr->getT3dTest()->GetTrace();
        const TraceOps & traceOps = pTrace->GetTraceOps();
        if (traceOpNum >= traceOps.size())
            return 0;

        // Optimization to store the previous TraceOp iterator so we do not need
        // to advance from the start each time
        static GpuTrace* s_pTrace = nullptr;
        static TraceOps::const_iterator s_It = traceOps.begin();
        static UINT32 s_PrevTraceOpNum = 0;

        // Re-init cached data in case this is a different trace
        if (s_pTrace != pTrace)
        {
            s_pTrace = pTrace;
            s_It = traceOps.begin();
            s_PrevTraceOpNum = 0;
        }
        std::advance(s_It, traceOpNum - s_PrevTraceOpNum);
        s_PrevTraceOpNum = traceOpNum;

        return pTrace->GetTraceOpLineNumber(s_It->second);
    }
    RTApiReceiver2<UINT32, TraceManagerMods *, UINT32> TraceManager_getTraceOpLineNumber_Receiver(TraceManager_getTraceOpLineNumber);

    INT32 JavaScriptManager_LoadJavaScript(JavaScriptManager * pJavaScriptManager,
        const char *fileName)
    {
        return pJavaScriptManager->LoadJavaScript(fileName);
    }

    INT32 JavaScriptManager_ParseArgs(JavaScriptManager * pJavaScriptManager,
        const char *cmdLine)
    {
        return pJavaScriptManager->ParseArgs(cmdLine);
    }

    INT32 JavaScriptManager_CallVoidMethod(JavaScriptManager * pJavaScriptManager,
        const char *methodName,
        const char **argValue,
        UINT32 argCount)
    {
        return pJavaScriptManager->CallVoidMethod(methodName,
            argValue,
            argCount);
    }

    INT32 JavaScriptManager_CallINT32Method(JavaScriptManager * pJavaScriptManager,
        const char *methodName,
        const char **argValue,
        UINT32 argCount,
        INT32 *retValue)
    {
        return pJavaScriptManager->CallINT32Method(methodName,
            argValue,
            argCount,
            retValue);
    }

    INT32 JavaScriptManager_CallUINT32Method(JavaScriptManager * pJavaScriptManager,
        const char *methodName,
        const char **argValue,
        UINT32 argCount,
        UINT32 *retValue)
    {
        return pJavaScriptManager->CallUINT32Method(methodName,
            argValue,
            argCount,
            retValue);
    }

    void *DisplayManager_GetChannelByHandle(DisplayManager *pDisplayManager,
        UINT32 handle)
    {
        return pDisplayManager->GetChannelByHandle(handle);
    }

    INT32 DisplayManager_MethodWrite(DisplayManager *pDisplayManager,
        void *channel,
        UINT32 method,
        UINT32 data)
    {
        return pDisplayManager->MethodWrite(channel, method, data);
    }

    INT32 DisplayManager_FlushChannel(DisplayManager *pDisplayManager,
        void *channel)
    {
        return pDisplayManager->FlushChannel(channel);
    }

    INT32 DisplayManager_CreateContextDma(DisplayManager *pDisplayManager,
        UINT32 chHandle,
        UINT32 flags,
        UINT32 size,
        const char *memoryLocation,
        UINT32 *dmaHandle)
    {
        return pDisplayManager->CreateContextDma(chHandle,
            flags,
            size,
            memoryLocation,
            dmaHandle);
    }

    INT32 DisplayManager_GetDmaCtxAddr(DisplayManager *pDisplayManager,
        UINT32 dmaHandle,
        void **addr)
    {
        return pDisplayManager->GetDmaCtxAddr(dmaHandle, addr);
    }

    INT32 DisplayManager_GetDmaCtxOffset(DisplayManager *pDisplayManager,
        UINT32 dmaHandle,
        UINT64 *offset)
    {
        return pDisplayManager->GetDmaCtxOffset(dmaHandle, offset);
    }

    INT32 TraceManager_RunTraceOps(TraceManager *pT3dTraceMgr)
    {
        return pT3dTraceMgr->RunTraceOps();
    }
    RTApiReceiver1<int, TraceManager *> TraceManager_RunTraceOps_Receiver(TraceManager_RunTraceOps);

    INT32 TraceManager_StartPerfMon(TraceManager* pTraceManager,
        void* pTraceChannel,
        const char* name)
    {
        return pTraceManager->StartPerfMon(pTraceChannel, name);
    }
    RTApiReceiver3<int, TraceManager *, void *, const char *> TraceManager_StartPerfMon_Receiver(TraceManager_StartPerfMon);

    INT32 TraceManager_StopPerfMon(TraceManager* pTraceManager,
        void* pTraceChannel,
        const char* name)
    {
        return pTraceManager->StopPerfMon(pTraceChannel, name);
    }
    RTApiReceiver3<int, TraceManager *, void *, const char *> TraceManager_StopPerfMon_Receiver(TraceManager_StopPerfMon);

    void TraceManager_StopVpmCapture(TraceManager* pTraceManager)
    {
        pTraceManager->StopVpmCapture();
    }
    VoidApiReceiver1<TraceManager *> TraceManager_StopVpmCapture_Receiver(TraceManager_StopVpmCapture);

    void TraceManager_AbortTest(TraceManager* pTraceManager)
    {
        pTraceManager->AbortTest();
    }
    VoidApiReceiver1<TraceManager *> TraceManager_AbortTest_Receiver(TraceManager_AbortTest);

    SOC * Host_getSOC( Host * pTrace3dHost )
    {
        return pTrace3dHost->getSOC();
    }
    RTApiReceiver1<SOC *, Host *> Host_getSOC_Receiver(Host_getSOC);

    Surface* BufferManager_CreateDynamicSurface(BufferManager * pT3dBufMgr,
                                                const char* name)
    {
        return pT3dBufMgr->CreateDynamicSurface(name);
    }
    RTApiReceiver2<Surface *, BufferManager *, const char *>
        BufferManager_CreateDynamicSurface_Receiver(BufferManager_CreateDynamicSurface);

    bool Surface_Alloc(BufferManager * pT3dBufMgr, MdiagSurf * pSurf)
    {
        if (pSurf->GetArrayPitch() > 0)
        {
            Trace3DTest *pTest = ((BufferManagerMods*)pT3dBufMgr)->getT3dTest();

            return OK == pSurf->Alloc(pTest->GetBoundGpuDevice(), pTest->GetLwRmPtr());
        }
        else
            return false;
    }
    RTApiReceiver2<bool, BufferManager *, MdiagSurf *> Surface_Alloc_Receiver(Surface_Alloc);

    void Surface_Free(MdiagSurf * pSurf)
    {
        pSurf->Free();
    }
    VoidApiReceiver1<MdiagSurf *> Surface_Free_Receiver(Surface_Free);

    void Surface_SetPitch(MdiagSurf * pSurf, UINT32 pitch)
    {
        pSurf->SetArrayPitch(pitch);
    }
    VoidApiReceiver2<MdiagSurf *, UINT32> Surface_SetPitch_Receiver(Surface_SetPitch);

    void Surface_SetPhysContig(MdiagSurf * pSurf, bool physContig)
    {
        pSurf->SetPhysContig(physContig);
    }

    UINT64 Surface_GetGpuVirtAddr(MdiagSurf * pSurf)
    {
        return pSurf->GetCtxDmaOffsetGpu();
    }

    UINT08 SOC_RegRd08(SOC* pT3dSOC, UINT32 offset)
    {
        return pT3dSOC->RegRd08(offset);
    }

    void SOC_RegWr08(SOC* pT3dSOC, UINT32 offset, UINT08 data)
    {
        pT3dSOC->RegWr08(offset, data);
    }

    UINT16 SOC_RegRd16(SOC* pT3dSOC, UINT32 offset)
    {
        return pT3dSOC->RegRd16(offset);
    }

    void SOC_RegWr16(SOC* pT3dSOC, UINT32 offset, UINT16 data)
    {
        pT3dSOC->RegWr16(offset, data);
    }

    UINT32 SOC_RegRd32(SOC* pT3dSOC, UINT32 offset)
    {
        return pT3dSOC->RegRd32(offset);
    }

    void SOC_RegWr32(SOC* pT3dSOC, UINT32 offset, UINT32 data)
    {
        pT3dSOC->RegWr32(offset, data);
    }

    SOCSyncPoint* SOC_AllocSyncPoint
    (
       SOC* pT3dSOC,
       UINT32 subdev
    )
    {
        return pT3dSOC->AllocSyncPoint(subdev);
    }
    RTApiReceiver2<SOCSyncPoint *, SOC *, UINT32> SOC_AllocSyncPoint_Receiver(SOC_AllocSyncPoint);

    SOCSyncPointBase* SOC_AllocSyncPointBase
    (
       SOC* pT3dSOC,
       UINT32 initialValue,
       bool forceIndex,
       UINT32 index
    )
    {
        return pT3dSOC->AllocSyncPointBase(initialValue, forceIndex, index);
    }
    RTApiReceiver4<SOCSyncPointBase *, SOC *, UINT32, bool, UINT32> SOC_AllocSyncPointBase_Receiver(SOC_AllocSyncPointBase);

    UINT08 Mem_PhysRd08( Mem* pT3dMem, UINT64 pa )
    {
        return pT3dMem->PhysRd08(pa);
    }

    UINT16 Mem_PhysRd16( Mem* pT3dMem, UINT64 pa )
    {
        return pT3dMem->PhysRd16(pa);
    }

    UINT32 Mem_PhysRd32( Mem* pT3dMem, UINT64 pa )
    {
        return pT3dMem->PhysRd32(pa);
    }

    UINT64 Mem_PhysRd64( Mem* pT3dMem, UINT64 pa )
    {
        return pT3dMem->PhysRd64(pa);
    }

    void Mem_PhysRd( Mem * pT3dMem, UINT64 pa, void * data, UINT32 count )
    {
        pT3dMem->PhysRd(pa, data, count);
    }

    void Mem_PhysWr08( Mem* pT3dMem, UINT64 pa, UINT08 data )
    {
        pT3dMem->PhysWr08(pa, data);
    }

    void Mem_PhysWr16( Mem* pT3dMem, UINT64 pa, UINT16 data )
    {
        pT3dMem->PhysWr16(pa, data);
    }

    void Mem_PhysWr32( Mem* pT3dMem, UINT64 pa, UINT32 data )
    {
        pT3dMem->PhysWr32(pa, data);
    }

    void Mem_PhysWr64( Mem* pT3dMem, UINT64 pa, UINT64 data )
    {
        pT3dMem->PhysWr64(pa, data);
    }

    void Mem_PhysWr( Mem * pT3dMem, UINT64 pa, const void * data, UINT32 count)
    {
        pT3dMem->PhysWr(pa, data, count);
    }

    UINT32 SOC_SyncPoint_GetIndex(SOCSyncPoint* pT3dSyncPoint)
    {
        return pT3dSyncPoint->GetIndex();
    }
    RTApiReceiver1<UINT32, SOCSyncPoint *> SOC_SyncPoint_GetIndex_Receiver(SOC_SyncPoint_GetIndex);

    void SOC_SyncPoint_Wait(SOCSyncPoint* pT3dSyncPoint,
                            UINT32 threshold, bool base,
                            UINT32 baseIndex, bool _switch,
                            void * pTraceChannel, int subchannel)
    {
        return pT3dSyncPoint->Wait(threshold, base, baseIndex,
                _switch, pTraceChannel, subchannel);
    }
    VoidApiReceiver7<SOCSyncPoint *, UINT32, bool, UINT32, bool, void *, int>
        SOC_SyncPoint_Wait_Receiver(SOC_SyncPoint_Wait);

    void SOC_SyncPoint_HostIncrement(SOCSyncPoint* pT3dSyncPoint,
            bool wfi, bool flush, void* pTraceChannel, int subchannel)
    {
        return pT3dSyncPoint->HostIncrement(wfi, flush, pTraceChannel, subchannel);
    }
    VoidApiReceiver5<SOCSyncPoint *, bool, bool, void *, int>
        SOC_SyncPoint_HostIncrement_Receiver(SOC_SyncPoint_HostIncrement);

    void SOC_SyncPoint_GraphicsIncrement(SOCSyncPoint* pT3dSyncPoint,
            bool wfi, bool flush, void* pTraceChannel, int subchannel)
    {
        return pT3dSyncPoint->GraphicsIncrement(wfi, flush, pTraceChannel, subchannel);
    }
    VoidApiReceiver5<SOCSyncPoint *, bool, bool, void *, int>
        SOC_SyncPoint_GraphicsIncrement_Receiver(SOC_SyncPoint_GraphicsIncrement);

    UINT32 SOC_SyncPointBase_GetIndex(SOCSyncPointBase* pT3dSyncPointBase)
    {
        return pT3dSyncPointBase->GetIndex();
    }
    RTApiReceiver1<UINT32, SOCSyncPointBase *> SOC_SyncPointBase_GetIndex_Receiver(SOC_SyncPointBase_GetIndex);

    UINT32 SOC_SyncPointBase_GetValue(SOCSyncPointBase* pT3dSyncPointBase)
    {
        return pT3dSyncPointBase->GetValue();
    }
    RTApiReceiver1<UINT32, SOCSyncPointBase *> SOC_SyncPointBase_GetValue_Receiver(SOC_SyncPointBase_GetValue);

    void SOC_SyncPointBase_SetValue
    (
      SOCSyncPointBase* pT3dSyncPointBase, UINT32 value,
      bool wfi, bool flush,
      void* pTraceChannel, int subchannel
    )
    {
        pT3dSyncPointBase->SetValue(value, wfi, flush, pTraceChannel, subchannel);
    }
    VoidApiReceiver6<SOCSyncPointBase *, UINT32, bool, bool, void *, int>
        SOC_SyncPointBase_SetValue_Receiver(SOC_SyncPointBase_SetValue);

    void SOC_SyncPointBase_AddValue
    (
      SOCSyncPointBase* pT3dSyncPointBase, UINT32 value,
      bool wfi, bool flush,
      void* pTraceChannel, int subchannel
    )
    {
        pT3dSyncPointBase->AddValue(value, wfi, flush, pTraceChannel, subchannel);
    }
    VoidApiReceiver6<SOCSyncPointBase *, UINT32, bool, bool, void *, int>
        SOC_SyncPointBase_AddValue_Receiver(SOC_SyncPointBase_AddValue);

    void SOC_SyncPointBase_CpuSetValue(SOCSyncPointBase* pT3dSyncPointBase, UINT32 value)
    {
        pT3dSyncPointBase->CpuSetValue(value);
    }
    VoidApiReceiver2<SOCSyncPointBase *, UINT32> SOC_SyncPointBase_CpuSetValue_Receiver(SOC_SyncPointBase_CpuSetValue);

    void SOC_SyncPointBase_CpuAddValue(SOCSyncPointBase* pT3dSyncPointBase, UINT32 value)
    {
        pT3dSyncPointBase->CpuAddValue(value);
    }
    VoidApiReceiver2<SOCSyncPointBase *, UINT32> SOC_SyncPointBase_CpuAddValue_Receiver(SOC_SyncPointBase_CpuAddValue);

    Semaphore* UtilityManager_AllocSemaphore
    (
       UtilityManager* pT3dUtilityManager,
       UINT64 physAddr,
       SemaphorePayloadSize size,
       bool isSyncptShimSema=false
    )
    {
        return pT3dUtilityManager->AllocSemaphore(physAddr, size, isSyncptShimSema);
    }
    RTApiReceiver4<Semaphore*, UtilityManager*, UINT64, SemaphorePayloadSize, bool> UtilityManager_AllocSemaphore_Receiver(UtilityManager_AllocSemaphore);

    void UtilityManager_FreeSemaphore
    (
       UtilityManager* pT3dUtilityManager,
       Semaphore* pSema
    )
    {
        return pT3dUtilityManager->FreeSemaphore(pSema);
    }
    VoidApiReceiver2<UtilityManager*, Semaphore*> UtilityManager_FreeSemaphore_Receiver(UtilityManager_FreeSemaphore);

    void Semaphore_Acquire(Semaphore* pT3dSemaphore, void* pTraceChannel, UINT64 data)
    {
        return pT3dSemaphore->Acquire(pTraceChannel, data);
    }
    VoidApiReceiver3<Semaphore *, void *, UINT64>
        Semaphore_Acquire_Receiver(Semaphore_Acquire);

    void Semaphore_AcquireGE(Semaphore* pT3dSemaphore, void* pTraceChannel, UINT64 data)
    {
        return pT3dSemaphore->AcquireGE(pTraceChannel, data);
    }
    VoidApiReceiver3<Semaphore *, void *, UINT64>
        Semaphore_AcquireGE_Receiver(Semaphore_AcquireGE);

    void Semaphore_Release(Semaphore* pT3dSemaphore, void* pTraceChannel, UINT64 data, bool flush)
    {
        return pT3dSemaphore->Release(pTraceChannel, data, flush);
    }
    VoidApiReceiver4<Semaphore *, void *, UINT64, bool>
        Semaphore_Release_Receiver(Semaphore_Release);

    UINT64 Semaphore_GetPhysAddress(Semaphore* pT3dSemaphore)
    {
        return pT3dSemaphore->GetPhysAddress();
    }
    RTApiReceiver1<UINT64, Semaphore *> Semaphore_GetPhysAddress_Receiver(Semaphore_GetPhysAddress);

    void Semaphore_SetReleaseFlags(Semaphore* pT3dSemaphore, void* pTraceChannel, UINT32 flags)
    {
        pT3dSemaphore->SetReleaseFlags(pTraceChannel, flags);
    }
    VoidApiReceiver3<Semaphore *, void *, UINT32> Semaphore_SetReleaseFlags_Receiver(Semaphore_SetReleaseFlags);

    bool UtilityManager_HookIRQ(UtilityManager* pT3dUtilityManager,
                                UINT32 irq, ISR handler, void* cookie)
    {
        return pT3dUtilityManager->HookIRQ(irq, handler, cookie);
    }

    bool UtilityManager_UnhookIRQ(UtilityManager* pT3dUtilityManager,
                                  UINT32 irq, ISR handler, void* cookie)
    {
        return pT3dUtilityManager->UnhookIRQ(irq, handler, cookie);
    }

    bool UtilityManager_DisableIRQ(UtilityManager* pT3dUtilityManager, UINT32 irq)
    {
        return pT3dUtilityManager->DisableIRQ(irq);
    }
    RTApiReceiver2<bool, UtilityManager *, UINT32> UtilityManager_DisableIRQ_Receiver(UtilityManager_DisableIRQ);

    bool UtilityManager_EnableIRQ(UtilityManager* pT3dUtilityManager, UINT32 irq)
    {
        return pT3dUtilityManager->EnableIRQ(irq);
    }
    RTApiReceiver2<bool, UtilityManager *, UINT32> UtilityManager_EnableIRQ_Receiver(UtilityManager_EnableIRQ);

    bool UtilityManager_IsIRQHooked(UtilityManager* pT3dUtilityManager, UINT32 irq)
    {
        return pT3dUtilityManager->IsIRQHooked(irq);
    }
    RTApiReceiver2<bool, UtilityManager *, UINT32> UtilityManager_IsIRQHooked_Receiver(UtilityManager_IsIRQHooked);

    void UtilityManager_ModsAssertFail(const char *file, int line,
        const char *function, const char *cond)
    {
        return ModsAssertFail(file, line, function, cond);
    }

    INT32 Gpu_GetRegNum(GpuMods * pTrace3dGpu, const char *regName,
        UINT32 *value, const char *regSpace)
    {
        return pTrace3dGpu->GetRegNum(regName, value, regSpace);
    }

    INT32 Gpu_SetRegFldDef(GpuMods * pTrace3dGpu, const char *regName,
        const char *subfieldName, const char *subdefName, UINT32 *myValue, const char *regSpace)
    {
        return pTrace3dGpu->SetRegFldDef(regName, subfieldName, subdefName,
            myValue, regSpace);
    }

    INT32 Gpu_SetRegFldNum(GpuMods * pTrace3dGpu, const char *regName,
        const char *subfieldName, UINT32 setValue, UINT32 *myValue, const char *regSpace)
    {
        return pTrace3dGpu->SetRegFldNum(regName, subfieldName, setValue,
            myValue, regSpace);
    }

    INT32 Gpu_GetRegFldNum(GpuMods * pTrace3dGpu, const char *regName,
        const char *subfieldName, UINT32 *theValue, UINT32 *myValue, const char *regSpace)
    {
        return pTrace3dGpu->GetRegFldNum(regName, subfieldName, theValue,
            myValue, regSpace);
    }

    INT32 Gpu_GetRegFldDef(GpuMods * pTrace3dGpu, const char *regName,
        const char *subfieldName, const char *subdefName, UINT32 *myValue)
    {
        return pTrace3dGpu->GetRegFldDef(regName, subfieldName, subdefName,
            myValue);
    }

    bool Gpu_TestRegFldDef(GpuMods * pTrace3dGpu, const char *regName,
        const char *subfieldName, const char *subdefName, UINT32 *myValue, const char *regSpace)
    {
        return pTrace3dGpu->TestRegFldDef(regName, subfieldName, subdefName,
            myValue, regSpace);
    }

    bool Gpu_TestRegFldNum(GpuMods * pTrace3dGpu, const char *regName,
        const char *subfieldName, UINT32 testValue, UINT32 *myValue, const char *regSpace)
    {
        return pTrace3dGpu->TestRegFldNum(regName, subfieldName, testValue,
            myValue, regSpace);
    }

    UINT08 Gpu_PciRd08(GpuMods * pTrace3dGpu, UINT32 offset)
    {
        return pTrace3dGpu->PciRd08(offset);
    }

    UINT16 Gpu_PciRd16(GpuMods * pTrace3dGpu, UINT32 offset)
    {
        return pTrace3dGpu->PciRd16(offset);
    }

    UINT32 Gpu_PciRd32(GpuMods * pTrace3dGpu, UINT32 offset)
    {
        return pTrace3dGpu->PciRd32(offset);
    }

    void Gpu_PciWr08(GpuMods * pTrace3dGpu, UINT32 offset, UINT08 data)
    {
        pTrace3dGpu->PciWr08(offset, data);
    }

    void Gpu_PciWr16(GpuMods * pTrace3dGpu, UINT32 offset, UINT16 data)
    {
        pTrace3dGpu->PciWr16(offset, data);
    }

    void Gpu_PciWr32(GpuMods * pTrace3dGpu, UINT32 offset, UINT32 data)
    {
        pTrace3dGpu->PciWr32(offset, data);
    }

    UINT32 Gpu_GetIrq(GpuMods * pTrace3dGpu, UINT32 irqNum)
    {
        return pTrace3dGpu->GetIrq(irqNum);
    }

    UINT32 Gpu_GetNumIrqs(GpuMods * pTrace3dGpu)
    {
        return pTrace3dGpu->GetNumIrqs();
    }

    UINT32 Gpu_GetHookedIntrType(GpuMods * pTrace3dGpu)
    {
        return pTrace3dGpu->GetHookedIntrType();
    }

    UINT32 Gpu_UnlockHost2Jtag
    (
        GpuMods * pTrace3dGpu,
        UINT32 *secCfgMaskArr,
        UINT32 secCfgMaskSize,
        UINT32 * selwreKeysArr,
        UINT32 selwreKeysSize
    )
    {
        return pTrace3dGpu->UnlockHost2Jtag(secCfgMaskArr, secCfgMaskSize, selwreKeysArr, selwreKeysSize);
    }

    class Gpu_UnlockHost2Jtag_Receiver_class : public ApiReceiver
    {
        public:
            void operator()(MailBox *pluginMailBox, MailBox * traceMailBox)
            {
                GpuMods * pTrace3dGpu = traceMailBox->Read<GpuMods *>();
                UINT32 secCfgMasksSize = traceMailBox->Read<UINT32>();
                vector<UINT32> secCfgMasks;
                for (UINT32 i=0; i < secCfgMasksSize; i++)
                {
                    secCfgMasks.push_back(traceMailBox->Read<UINT32>());
                }
                UINT32 selwreKeysSize = traceMailBox->Read<UINT32>();
                vector<UINT32> selwreKeys;
                for(UINT32 i = 0; i < selwreKeysSize; ++i)
                {
                    selwreKeys.push_back(traceMailBox->Read<UINT32>());
                }
                traceMailBox->Release();

                UINT32 result = pTrace3dGpu->UnlockHost2Jtag(
                    secCfgMasks.data(),
                    secCfgMasksSize,
                    selwreKeys.data(),
                    selwreKeysSize);

                pluginMailBox->Acquire();

                pluginMailBox->ResetDataOffset();
                pluginMailBox->Write(result);
                pluginMailBox->WriteCommand(MailBox::CMD_T3DAPI_ACK);

                pluginMailBox->Send();
            }
    }Gpu_UnlockHost2Jtag_Receiver;

    UINT32 Gpu_ReadHost2Jtag
    (
        GpuMods * pTrace3dGpu,
        UINT32 * chipletId,
        UINT32 * intstId,
        UINT32 jtagClustersSize,
        UINT32 chainLen,
        UINT32 * pResult,
        UINT32 * resultSize,
        UINT32 capDis,
        UINT32 updtDis
    )
    {
        return pTrace3dGpu->ReadHost2Jtag(chipletId, intstId, jtagClustersSize, chainLen, pResult, resultSize, capDis, updtDis);
    }

    class Gpu_ReadHost2Jtag_Receiver_class : public ApiReceiver
    {
        public:
            void operator()(MailBox *pluginMailBox, MailBox * traceMailBox)
            {
                GpuMods * pTrace3dGpu = traceMailBox->Read<GpuMods *>();
                UINT32 jtagClustersSize = traceMailBox->Read<UINT32>();
                UINT32 chainLen = traceMailBox->Read<UINT32>();
                UINT32 capDis = traceMailBox->Read<UINT32>();
                UINT32 updtDis = traceMailBox->Read<UINT32>();
                vector<UINT32> chipletId;
                vector<UINT32> intstId;

                for(UINT32 i = 0; i < jtagClustersSize; ++i)
                {
                    chipletId.push_back(traceMailBox->Read<UINT32>());
                    intstId.push_back(traceMailBox->Read<UINT32>());
                }
                traceMailBox->Release();

                unique_ptr<UINT32[]> temp(new UINT32[((chainLen + 31) / 32)]);
                UINT32 tempSize;
                UINT32 result = pTrace3dGpu->ReadHost2Jtag(chipletId.data(), intstId.data(), jtagClustersSize, chainLen, temp.get(), &tempSize, capDis, updtDis);

                pluginMailBox->Acquire();

                pluginMailBox->ResetDataOffset();
                pluginMailBox->Write(result);
                if(!result)
                {
                    UINT32 resultDataSize = tempSize;
                    pluginMailBox->Write(resultDataSize);
                    for(UINT32 i = 0; i < resultDataSize; ++i)
                    {
                        pluginMailBox->Write(temp[i]);
                    }
                }
                pluginMailBox->WriteCommand(MailBox::CMD_T3DAPI_ACK);

                pluginMailBox->Send();
            }
    }Gpu_ReadHost2Jtag_Receiver;

    UINT32 Gpu_WriteHost2Jtag
    (
        GpuMods * pTrace3dGpu,
        UINT32 * chipletId,
        UINT32 * intstId,
        UINT32 jtagClustersSize,
        UINT32 chainLen,
        UINT32 * inputDataArr,
        UINT32 inputDataSize,
        UINT32 capDis,
        UINT32 updtDis
    )
    {
        return  pTrace3dGpu->WriteHost2Jtag(chipletId, intstId, jtagClustersSize, chainLen, inputDataArr, inputDataSize, capDis, updtDis);
    }

    class Gpu_WriteHost2Jtag_Receiver_class : public ApiReceiver
    {
        public:
            void operator()(MailBox *pluginMailBox, MailBox * traceMailBox)
            {
                GpuMods * pTrace3dGpu = traceMailBox->Read<GpuMods *>();
                UINT32 jtagClustersSize = traceMailBox->Read<UINT32>();
                UINT32 chainLen = traceMailBox->Read<UINT32>();
                UINT32 capDis = traceMailBox->Read<UINT32>();
                UINT32 updtDis = traceMailBox->Read<UINT32>();

                vector<UINT32> chipletId;
                vector<UINT32> intstId;
                for(UINT32 i = 0; i < jtagClustersSize; ++i)
                {
                    chipletId.push_back(traceMailBox->Read<UINT32>());
                    intstId.push_back(traceMailBox->Read<UINT32>());
                }

                UINT32 inputDataSize = traceMailBox->Read<UINT32>();
                vector<UINT32> inputData(inputDataSize);
                for(UINT32 i = 0; i < inputDataSize; ++i)
                {
                    inputData[i] = traceMailBox->Read<UINT32>();
                }

                traceMailBox->Release();

                UINT32 result = pTrace3dGpu->WriteHost2Jtag(chipletId.data(), intstId.data(), jtagClustersSize, chainLen, inputData.data(), inputDataSize, capDis, updtDis);

                pluginMailBox->Acquire();

                pluginMailBox->ResetDataOffset();
                pluginMailBox->Write(result);
                pluginMailBox->WriteCommand(MailBox::CMD_T3DAPI_ACK);

                pluginMailBox->Send();
            }
    }Gpu_WriteHost2Jtag_Receiver;

    UINT32 Gpu_GetDeviceId(GpuMods * pTrace3dGpu)
    {
        return pTrace3dGpu->GetDeviceId();
    }

    const char *Gpu_GetDeviceIdString(GpuMods * pTrace3dGpu)
    {
        return pTrace3dGpu->GetDeviceIdString();
    }

    FLOAT64 UtilityManager_GetTimeoutMS( UtilityManager * pT3dUtilityMgr )
    {
        return pT3dUtilityMgr->GetTimeoutMS();
    }

    bool UtilityManager_Poll(UtilityManager* pT3dUtilityManager,
        PollFunc Function, volatile void* pArgs, FLOAT64 TimeoutMs)
    {
        return pT3dUtilityManager->Poll(Function, pArgs, TimeoutMs);
    }

    void UtilityManager_DelayNS(UINT32 nanoseconds)
    {
        return Platform::DelayNS(nanoseconds);
    }

    bool Buffer_IsAllocated(Buffer* pT3dBuffer)
    {
        return pT3dBuffer->IsAllocated();
    }

    bool Surface_IsAllocated(MdiagSurf* pSurf)
    {
        return pSurf->IsAllocated();
    }

    UINT32 Buffer_GetTagCount(Buffer* pT3dBuffer)
    {
        return pT3dBuffer->GetTagCount();
    }

    const char* Buffer_GetTag(Buffer* pT3dBuffer, UINT32 tagIndex)
    {
        return pT3dBuffer->GetTag(tagIndex);
    }

    INT32 Gpu_SetPcieLinkSpeed(GpuMods *pTrace3dGpu, UINT32 newSpeed)
    {
        return pTrace3dGpu->SetPcieLinkSpeed(newSpeed);
    }

    void BufferManager_SetSkipBufferDownloads(BufferManager * pT3dBufMgr,
        bool skip)
    {
        pT3dBufMgr->SetSkipBufferDownloads(skip);
    }

    UINT32 Surface_CallwlateCrc(BufferManagerMods* pBufMgr, MdiagSurf* pSurf,
            UINT32 subdev, const Surface::CrcCheckInfo* pInfo)
    {
        Trace3DTest* pT3dTest = pBufMgr->getT3dTest();
        GpuVerif* pGpuVerif = pT3dTest->GetDefaultChannelByVAS(
            pSurf->GetGpuVASpace())->GetGpuVerif();

        unique_ptr<SurfaceAssembler::Rectangle> pWindow;
        if (pInfo && pInfo->Windowing())
        {
            pWindow.reset(new SurfaceAssembler::Rectangle(
                    pInfo->WinX, pInfo->WinY,
                    pInfo->WinWidth, pInfo->WinHeight));
        }

        TraceModule *pTraceModule = pT3dTest->GetTrace()->ModFind(pSurf->GetName().c_str());
        if (pTraceModule)
        {
            RC rc = pTraceModule->PrepareToCheck(CRC_CHECK, pSurf->GetName().c_str(), 0);
            if (rc != OK)
            {
                return 0;
            }
        }

        UINT32 crcValue = pGpuVerif->CallwlateCrc(
                pSurf->GetName(), subdev, pWindow.get());

        return crcValue;
    }

    INT32 Gpu_LoadPMUFirmware(GpuMods *pTrace3dGpu,
                              const char *firmwareFile,
                              bool resetOnLoad,
                              bool waitInit)
    {
        MASSERT(!"Gpu_LoadPMUFirmware is deprecated! Please don't call this IF.\n");
        return 0;
    }

    void BufferManager_RedoRelocations(BufferManager* pT3dBufMgr)
    {
        pT3dBufMgr->RedoRelocations();
    }

    bool Buffer_IsGpuMapped(Buffer* pT3dBuffer)
    {
        return pT3dBuffer->IsGpuMapped();
    }

    bool Surface_IsGpuMapped(MdiagSurf* pSurf)
    {
        return pSurf->IsGpuMapped();
    }

    void Buffer_GetTagList()
    {
        // Fake, plugin side only.
        MASSERT(!"This is a plugin side only function. Shouldn't reach here.\n");
    }

    UINT64 Surface_GetTegraVirtAddr(MdiagSurf *pSurf)
    {
        MASSERT(!"This API is depreciated!\n");
        return 0;
    }
    RTApiReceiver1<UINT64, MdiagSurf *> Surface_GetTegraVirtAddr_Receiver(Surface_GetTegraVirtAddr);

    void Surface_SetVASpace(MdiagSurf *pSurf, Surface::VASpace vaSpace)
    {
        pSurf->SetVASpace(Surface2D::VASpace(vaSpace));
    }
    VoidApiReceiver2<MdiagSurf *, Surface::VASpace> Surface_SetVASpace_Receiver(Surface_SetVASpace);

    bool SOC_IsSmmuEnabled(SOC* pT3dSOC)
    {
        bool isEnabled = true;
        UINT32 smmuEnabled = 0U;
        if (OK == Registry::Read("ResourceManager", "RMSmmuConfig", &smmuEnabled))
        {
            // smmuEnabled == 0 = DRF_DEF(_REG_STR_RM, _SMMU_CONFIG, _ALL, _DEFAULT),  means RM default setting which is again TRUE
            // smmuEnabled != 0 means MODS override; so check the registry value
            if (smmuEnabled)
            {
                // "-disable_smmu" sets the registry value to DRF(_REG_STR_RM, _SMMU_CONFIG, _ALL, _DISABLE)
                //  checking for ALL enable may not be apt, because SMMU display can be set independently.
                isEnabled = !FLD_TEST_DRF(_REG_STR_RM, _SMMU_CONFIG, _ALL, _DISABLE, smmuEnabled);
            }
        }
        return isEnabled;
    }
    RTApiReceiver1<bool, SOC*> SOC_IsSmmuEnabled_Receiver(SOC_IsSmmuEnabled);

    void BufferManager_DeclareTegraSharedSurface(BufferManager *pT3dBufMgr,
            const char *name)
    {
        pT3dBufMgr->DeclareTegraSharedSurface(name);
    }
    VoidApiReceiver2<BufferManager *, const char *> BufferManager_DeclareTegraSharedSurface_Receiver(BufferManager_DeclareTegraSharedSurface);

    void BufferManager_DeclareGlobalSharedSurface(BufferManager *pT3dBufMgr,
            const char *name, const char *global_name)
    {
        pT3dBufMgr->DeclareGlobalSharedSurface(name, global_name);
    }

    INT32 Gpu_GetPowerFeaturesSupported(Gpu *pTrace3dGpu,
                                        UINT32 *powerFeature)
    {
        return pTrace3dGpu->GetPowerFeaturesSupported(powerFeature);
    }

    bool Gpu_SetDiGpuReady(Gpu *pTrace3dGpu)
    {
        return pTrace3dGpu->SetDiGpuReady();
    }

    INT32 Gpu_SetGpuPowerOnOff(Gpu *pTrace3dGpu,
                               UINT32 action,
                               bool bUseHwTimer,
                               UINT32 timeToWakeUs,
                               bool bIsRTD3Transition,
                               bool bIsD3Hot)
    {
        return pTrace3dGpu->SetGpuPowerOnOff(action, bUseHwTimer, timeToWakeUs, bIsRTD3Transition, bIsD3Hot);
    }

    INT32 Gpu_GetGCxWakeupReason(Gpu *pTrace3dGpu,
                                 UINT32 selectPowerState,
                                 Gpu::GCxWakeupReasonParams *pWakeupReasonParams)
    {
        return pTrace3dGpu->GetGCxWakeupReason(selectPowerState, pWakeupReasonParams);
    }

    INT32 Gpu_GetGC6PlusIsIslandLoaded(Gpu *pTrace3dGpu,
                                 Gpu::GC6PlusIsIslandLoadedParams *pIsIslandLoadedParams)
    {
        return pTrace3dGpu->GetGC6PlusIsIslandLoaded(pIsIslandLoadedParams);
    }

    class Gpu_GetGC6PlusIsIslandLoaded_Receiver_Class : public ApiReceiver
    {
    public:
        void operator()(MailBox* pluginMailBox, MailBox* traceMailBox)
        {
            Gpu *pTrace3dGpu = traceMailBox->Read<Gpu*>();

            traceMailBox->Release();

            Gpu::GC6PlusIsIslandLoadedParams value;
            INT32 rv = Gpu_GetGC6PlusIsIslandLoaded( pTrace3dGpu, &value );

            pluginMailBox->Acquire();

            pluginMailBox->ResetDataOffset();
            pluginMailBox->Write(static_cast<char>(value.bIsIslandLoaded));
            pluginMailBox->Write(rv);

            pluginMailBox->WriteCommand(MailBox::CMD_T3DAPI_ACK);

            pluginMailBox->Send();
        }
    } Gpu_GetGC6PlusIsIslandLoaded_Receiver;

    UINT32 Gpu_GobWidth(GpuMods * pTrace3dGpu)
    {
        return pTrace3dGpu->GobWidth();
    }
    RTApiReceiver1<UINT32, GpuMods *> Gpu_GobWidth_Receiver(Gpu_GobWidth);

    UINT32 Gpu_GobHeight(GpuMods * pTrace3dGpu)
    {
        return pTrace3dGpu->GobHeight();
    }
    RTApiReceiver1<UINT32, GpuMods *> Gpu_GobHeight_Receiver(Gpu_GobHeight);

    UINT32 Gpu_GobDepth(GpuMods * pTrace3dGpu)
    {
        return pTrace3dGpu->GobDepth();
    }
    RTApiReceiver1<UINT32, GpuMods *> Gpu_GobDepth_Receiver(Gpu_GobDepth);

    UINT32 Gpu_GetGpcCount(GpuMods * pTrace3dGpu)
    {
        return pTrace3dGpu->GetGpcCount();
    }
    RTApiReceiver1<UINT32, GpuMods *> Gpu_GetGpcCount_Receiver(Gpu_GetGpcCount);

    UINT32 Gpu_GetTpcCountOnGpc(GpuMods * pTrace3dGpu, UINT32 virtualGpcNum)
    {
        return pTrace3dGpu->GetTpcCountOnGpc(virtualGpcNum);
    }
    RTApiReceiver2<UINT32, GpuMods *, UINT32> Gpu_GetTpcCountOnGpc_Receiver(Gpu_GetTpcCountOnGpc);

    bool Gpu_EnablePrivProtectedRegisters(Gpu *pTrace3dGpu, bool bEnable)
    {
        return pTrace3dGpu->EnablePrivProtectedRegisters(bEnable);
    }

    bool Gpu_EnablePowerGatingTestDuringRegAccess(Gpu *pTrace3dGpu, bool bEnable)
    {
        return pTrace3dGpu->EnablePowerGatingTestDuringRegAccess(bEnable);
    }

    bool Gpu_SetEnableCEPrefetch(Gpu *pTrace3dGpu, bool bEnable)
    {
        return pTrace3dGpu->SetEnableCEPrefetch(bEnable);
    }

    UINT32 Gpu_GetLwlinkPowerState(Gpu *pTrace3dGpu, UINT32 linkId)
    {
        return pTrace3dGpu->GetLwlinkPowerState(linkId);
    }

    UINT32 Gpu_SetLwlinkPowerState(Gpu *pTrace3dGpu, UINT32 linkMask,
        UINT32 powerState)
    {
        return pTrace3dGpu->SetLwlinkPowerState(linkMask, powerState);
    }

    UINT32 Gpu_SetPrePostPowerOnOff(Gpu *pTrace3dGpu, UINT32 action)
    {
        return pTrace3dGpu->SetPrePostPowerOnOff(action);
    }

    bool Gpu_GetLwlinkConnected(Gpu *pTrace3dGpu, UINT32 linkId)
    {
        return pTrace3dGpu->GetLwlinkConnected(linkId);
    }

    UINT32 Gpu_GetLwlinkLoopProp(Gpu *pTrace3dGpu, UINT32 linkId, UINT08* loopProp)
    {
        return pTrace3dGpu->GetLwlinkLoopProp(linkId, loopProp);
    }

    UINT32 Gpu_GetLwlinkRemoteLinkNum(Gpu *pTrace3dGpu, UINT32 linkId, UINT08* linkNum)
    {
        return pTrace3dGpu->GetLwlinkRemoteLinkNum(linkId, linkNum);
    }

    UINT32 Gpu_GetLwlinkLocalLinkNum(Gpu *pTrace3dGpu, UINT32 linkId, UINT08* linkNum)
    {
        return pTrace3dGpu->GetLwlinkLocalLinkNum(linkId, linkNum);
    }

    UINT32 Gpu_GetLwlinkDiscoveredMask(Gpu *pTrace3dGpu, UINT32* mask)
    {
        return pTrace3dGpu->GetLwlinkDiscoveredMask(mask);
    }

    UINT32 Gpu_GetLwlinkEnabledMask(Gpu *pTrace3dGpu, UINT32* mask)
    {
        return pTrace3dGpu->GetLwlinkEnabledMask(mask);
    }

    UINT32 Gpu_GetLwlinkConnectedMask(Gpu *pTrace3dGpu, UINT32* pLinkMask)
    {
        return pTrace3dGpu->GetLwlinkConnectedMask(pLinkMask);
    }

    UINT32 Gpu_GetLwlinkFomVals(Gpu *pTrace3dGpu, UINT32 linkId, UINT16** fom, UINT08* numLanes)
    {
        return pTrace3dGpu->GetLwlinkFomVals(linkId, fom, numLanes);
    }

    UINT32 Gpu_GetLwlinkLocalSidVals(Gpu *pTrace3dGpu, UINT32 linkId, UINT64* sid)
    {
        return pTrace3dGpu->GetLwlinkLocalSidVals(linkId, sid);
    }

    UINT32 Gpu_GetLwlinkRemoteSidVals(Gpu *pTrace3dGpu, UINT32 linkId, UINT64* sid)
    {
        return pTrace3dGpu->GetLwlinkRemoteSidVals(linkId, sid);
    }

    UINT32 Gpu_GetLwlinkLineRateMbps(Gpu *pTrace3dGpu, UINT32 linkId, UINT32* rate)
    {
        return pTrace3dGpu->GetLwlinkLineRateMbps(linkId, rate);
    }

    UINT32 Gpu_GetLwlinkLinkClockMHz(Gpu *pTrace3dGpu, UINT32 linkId, UINT32* clock)
    {
        return pTrace3dGpu->GetLwlinkLinkClockMHz(linkId, clock);
    }

    UINT32 Gpu_GetLwlinkLinkClockType(Gpu *pTrace3dGpu, UINT32 linkId, UINT32* clktype)
    {
        return pTrace3dGpu->GetLwlinkLinkClockType(linkId, clktype);
    }

    UINT32 Gpu_GetLwlinkDataRateKiBps(Gpu *pTrace3dGpu, UINT32 linkId, UINT32* rate)
    {
        return pTrace3dGpu->GetLwlinkDataRateKiBps(linkId, rate);
    }

    void Gpu_GetEngineResetId
    (
        Gpu *pTrace3dGpu,
        const char* engine,
        UINT32 hwInstance,
        UINT32* resetId,
        bool* bResetIdValid
    )
    {
        tie(*resetId, *bResetIdValid) = pTrace3dGpu->GetEngineResetId(engine, hwInstance);
    }

    UINT32 Gpu_GetLwlinkErrInfo(Gpu *pTrace3dGpu, Gpu::LWLINK_ERR_INFO** info, UINT08* numInfo)
    {
        return pTrace3dGpu->GetLwlinkErrInfo(info, numInfo);
    }

    UINT32 Gpu_EnterSlm(Gpu *pTrace3dGpu, UINT32 linkId, bool tx)
    {
        return pTrace3dGpu->EnterSlm(linkId, tx);
    }

    UINT32 Gpu_ExitSlm(Gpu *pTrace3dGpu, UINT32 linkId, bool tx)
    {
        return pTrace3dGpu->ExitSlm(linkId, tx);
    }

    INT32 DisplayManager_RmControl5070(DisplayManager *pDisplayManager,
                                       UINT32 ctrlCmdNum,
                                       void* params,
                                       size_t paramsSize)
    {
        return pDisplayManager->RmControl5070(ctrlCmdNum, params, paramsSize);
    }

    INT32 DisplayManager_GetActiveDisplayIDs(DisplayManager *pDisplayManager,
                                             UINT32 *pDisplayIDs,
                                             size_t *pSizeInBytes)
    {
        return pDisplayManager->GetActiveDisplayIDs(pDisplayIDs, pSizeInBytes);
    }

    UINT32 DisplayManager_GetMasterSubdevice(DisplayManager *pDisplayManager)
    {
        return pDisplayManager->GetMasterSubdevice();
    }

    INT32 DisplayManager_GetConfigParams
    (
        DisplayManager *pDisplayManager,
        UINT32 displayID,
        DisplayManager::DisplayMngCfgParams *pParams
    )
    {
        return pDisplayManager->GetConfigParams(displayID,
                                                pParams);
    }

    UINT64 Surface_getPhysAddress( ::MdiagSurf* pSurf, UINT64 virtAddr )
    {
        PHYSADDR physAddr;
        if (OK != pSurf->GetPhysAddress(virtAddr, &physAddr))
        {
            return 0;
        }
        return physAddr;
    }
    RTApiReceiver2<UINT64, ::MdiagSurf*, UINT64> Surface_getPhysAddress_Receiver(Surface_getPhysAddress);

    UINT64 Surface_getBAR1PhysAddress( ::MdiagSurf* pSurf, UINT64 virtAddr, UINT32 subdev )
    {
        UINT64 GpuVirtAddr = 0;
        GpuSubdevice *pSubdev = pSurf->GetGpuSubdev(subdev);
        void* pGetBar1OffsetMutex = Tasker::AllocMutex(
            "GpuSubdevice::m_pGetBar1OffsetMutex", Tasker::mtxUnchecked);
        Tasker::MutexHolder mutexHolder(pGetBar1OffsetMutex);
        LwRmPtr pLwRm;
        LW2080_CTRL_FB_GET_BAR1_OFFSET_PARAMS params = {0};
        params.cpuVirtAddress = LW_PTR_TO_LwP64(virtAddr);

        if( pLwRm->Control(pLwRm->GetSubdeviceHandle(pSubdev),
            LW2080_CTRL_CMD_FB_GET_BAR1_OFFSET,
            &params, sizeof (params)) == OK )
        {
            GpuVirtAddr = params.gpuVirtAddress + pSubdev->GetPhysFbBase();
        }
        mutexHolder.Release();
        Tasker::FreeMutex(pGetBar1OffsetMutex);

        return GpuVirtAddr;
    }
    RTApiReceiver3<UINT64, ::MdiagSurf*, UINT64, UINT32> Surface_getBAR1PhysAddress_Receiver(Surface_getBAR1PhysAddress);

    Surface::MemoryMappingMode Surface_GetMemoryMappingMode(MdiagSurf *pSurf)
    {
        return Surface::MemoryMappingMode(pSurf->GetMemoryMappingMode());
    }
    RTApiReceiver1<Surface::MemoryMappingMode, ::MdiagSurf*> Surface_GetMemoryMappingMode_Receiver(Surface_GetMemoryMappingMode);

    void Surface_SetMemoryMappingMode(MdiagSurf *pSurf, Surface::MemoryMappingMode mode)
    {
        pSurf->SetMemoryMappingMode(Surface2D::MemoryMappingMode(mode));
    }
    VoidApiReceiver2<MdiagSurf*, Surface::MemoryMappingMode> Surface_SetMemoryMappingMode_Receiver(Surface_SetMemoryMappingMode);

    Surface::Location Surface_GetLocation(MdiagSurf *pSurf)
    {
        return Surface::Location(pSurf->GetLocation());
    }

    RTApiReceiver1<Surface::Location, ::MdiagSurf*> Surface_GetLocation_Receiver(Surface_GetLocation);

    void Surface_SetLocation(MdiagSurf *pSurf, Surface::Location location)
    {
        pSurf->SetLocation(Memory::Location(location));
    }

    VoidApiReceiver2<MdiagSurf*, Surface::Location> Surface_SetLocation_Receiver(Surface_SetLocation);

    Surface::GpuCacheMode Surface_GetGpuCacheMode(MdiagSurf *pSurf)
    {
        return Surface::GpuCacheMode(pSurf->GetGpuCacheMode());
    }
    RTApiReceiver1<Surface::GpuCacheMode, ::MdiagSurf*> Surface_GetGpuCacheMode_Receiver(Surface_GetGpuCacheMode);

    void Surface_SetGpuCacheMode(MdiagSurf *pSurf, Surface::GpuCacheMode mode)
    {
        pSurf->SetGpuCacheMode(Surface2D::GpuCacheMode(mode));
    }
    VoidApiReceiver2<MdiagSurf*, Surface::GpuCacheMode> Surface_SetGpuCacheMode_Receiver(Surface_SetGpuCacheMode);

    void Surface_SetVideoProtected(MdiagSurf *pSurf, bool bProtected)
    {
        pSurf->SetVideoProtected(bProtected);
    }
    VoidApiReceiver2<MdiagSurf*, bool> Surface_SetVideoProtected_Receiver(Surface_SetVideoProtected);

    INT32 UtilityManager_VAPrintf(const char *Format, va_list Args)
    {
        return ModsExterlwAPrintf(Tee::PriNormal, Tee::ModuleNone,
                                  Tee::SPS_NORMAL, Format,
                                  *((va_list *)&Args));
    }
    class UtilityManager_VAPrintf_Receiver_Class : public ApiReceiver
    {
    public:
        void operator ()(MailBox *pluginMailBox, MailBox *traceMailBox)
        {
            // No reset.
            char *Format = traceMailBox->Read<char *>();
            void *Args =  traceMailBox->Read<void *>();

            traceMailBox->Release();

            // Yes, so far mailbox is ARM dedicated.
            INT32 ret = ArmVAPrintf(Format, Args);

            pluginMailBox->Acquire();

            pluginMailBox->ResetDataOffset();
            pluginMailBox->Write(ret);
            pluginMailBox->WriteCommand(MailBox::CMD_T3DAPI_ACK);

            pluginMailBox->Send();
        }
    } UtilityManager_VAPrintf_Receiver;

    FILE * FileProxy_Fopen(const char * filename, const char * type)
    {
        return fopen(filename,type);
    }
    RTApiReceiver2<FILE *, const char *, const char *> FileProxy_Fopen_Receiver(FileProxy_Fopen);

    INT32 FileProxy_Fclose(FILE * fp)
    {
        return fclose(fp);
    }
    RTApiReceiver1<INT32, FILE *> FileProxy_Fclose_Receiver(FileProxy_Fclose);

    INT32  FileProxy_Feof(FILE * fp)
    {
        return feof(fp);
    }
    RTApiReceiver1<INT32, FILE *> FileProxy_Feof_Receiver(FileProxy_Feof);

    INT32 FileProxy_Ferror(FILE * fp)
    {
        return ferror(fp);
    }
    RTApiReceiver1<INT32, FILE*> FileProxy_Ferror_Receiver(FileProxy_Ferror);

    UINT32 FileProxy_Fwrite(const void * buf, UINT32 size, UINT32 count, FILE * fp)
    {
        return fwrite(buf, size, count, fp);
    }
    class FileProxy_Fwrite_Receiver_class : public ApiReceiver
    {
    public:
        void operator()(MailBox *pluginMailBox, MailBox * traceMailBox)
        {
            UINT32 size = traceMailBox->Read<UINT32>();
            UINT32 count = traceMailBox->Read<UINT32>();
            FILE *fp = traceMailBox->Read<FILE *>();

            UINT32 total = size * count;
            char * buf = new char[total];
            traceMailBox->Read(buf, total);

            traceMailBox->Release();

            UINT32 result =  FileProxy_Fwrite(buf, size, count, fp);

            pluginMailBox->Acquire();

            pluginMailBox->ResetDataOffset();
            pluginMailBox->Write(result);
            pluginMailBox->WriteCommand(MailBox::CMD_T3DAPI_ACK);

            pluginMailBox->Send();

            delete [] buf;
        }
    }FileProxy_Fwrite_Receiver;

    UINT32 FileProxy_Fread(void * buf, UINT32 size, UINT32 count, FILE * fp)
    {
        return fread(buf, size, count, fp);
    }
    class FileProxy_Fread_Receiver_class : public ApiReceiver
    {
    public:
        void operator()(MailBox *pluginMailBox, MailBox * traceMailBox)
        {
            UINT32 size = traceMailBox->Read<UINT32>();
            UINT32 count = traceMailBox->Read<UINT32>();
            FILE *fp = traceMailBox->Read<FILE *>();

            traceMailBox->Release();

            UINT32 total = size * count;
            char * buf = new char[total];
            UINT32 result =  FileProxy_Fread(buf, size, count, fp);

            pluginMailBox->Acquire();

            pluginMailBox->ResetDataOffset();
            pluginMailBox->Write(result);
            if(result)
            {
                pluginMailBox->Write(buf, result * size);
            }
            pluginMailBox->WriteCommand(MailBox::CMD_T3DAPI_ACK);

            pluginMailBox->Send();

            delete [] buf;
        }
    }FileProxy_Fread_Receiver;

    INT32 FileProxy_Fgetc(FILE * fp)
    {
        return fgetc(fp);
    }
    RTApiReceiver1<INT32, FILE*> FileProxy_Fgetc_Receiver(FileProxy_Fgetc);

    INT32 FileProxy_Fputc(INT32 c, FILE * fp)
    {
        return fputc(c, fp);
    }
    RTApiReceiver2<INT32, INT32, FILE*> FileProxy_Fputc_Receiver(FileProxy_Fputc);

    char * FileProxy_Fgets(char * buf, int n, FILE * fp)
    {
        return fgets(buf, n, fp);
    }
    class FileProxy_Fgets_Receiver_class : public ApiReceiver
    {
    public:
        void operator()(MailBox *pluginMailBox, MailBox * traceMailBox)
        {
            UINT32 n = traceMailBox->Read<UINT32>();
            FILE *fp = traceMailBox->Read<FILE *>();

            traceMailBox->Release();

            char * buf = new char[n];
            char * result =  FileProxy_Fgets(buf, n, fp);
            UINT32 length = 0;
            if(result)
            {
                length = strlen(buf) + 1;
            }

            pluginMailBox->Acquire();

            pluginMailBox->ResetDataOffset();
            pluginMailBox->Write(length);
            if(length)
            {
                pluginMailBox->Write(buf, length);
            }
            pluginMailBox->WriteCommand(MailBox::CMD_T3DAPI_ACK);

            pluginMailBox->Send();

            delete [] buf;
        }
    }FileProxy_Fgets_Receiver;

    INT32 FileProxy_Fputs(const char * str, FILE * fp)
    {
        return fputs(str, fp);
    }
    RTApiReceiver2<INT32, const char *, FILE*> FileProxy_Fputs_Receiver(FileProxy_Fputs);

    INT32 FileProxy_Ftell (FILE * fp)
    {
        return ftell(fp);
    }
    RTApiReceiver1<INT32, FILE*> FileProxy_Ftell_Receiver(FileProxy_Ftell);

    INT32 FileProxy_Fseek(FILE * fp, INT32 offset, INT32 where)
    {
        return fseek(fp, offset, where);
    }
    RTApiReceiver3<INT32, FILE*, INT32, INT32> FileProxy_Fseek_Receiver(FileProxy_Fseek);

    Surface* Surface_SwizzleCompressionTags(BufferManagerMods * pHostBufferManager, Surface *pSurf)
    {
        ErrPrintf("CDE engine is Deprecated. Not support Surface_SwizzleCompressionTags any longer.\n");
        MASSERT(0);
        return NULL;
    }

    UINT32 BufferManager_GetTraversalDirection(BufferManager *pBufferManager)
    {
        ErrPrintf("CDE engine is Deprecated. Not support BufferManager_GetTraversalDirection any longer.\n");
        MASSERT(0);
        return 0;
    }

    RTApiReceiver1<UINT32, BufferManager *> BufferManager_TraversalDirection_Receiver(BufferManager_GetTraversalDirection);

    UINT64 BufferManager_GetActualPhysAddr(BufferManagerMods * pT3dBufMgr, const char *surfName, UINT64 offset)
    {
        return pT3dBufMgr->GetActualPhysAddr(surfName, offset);
    }
    RTApiReceiver3<UINT64, BufferManagerMods*, const char *, UINT64> BufferManager_GetActualPhysAddr_Receiver(BufferManager_GetActualPhysAddr);

    bool UtilityManager_RegisterUnmapEvent()
    {
        PolicyManager *pPolicyManager = PolicyManager::Instance();
        PmEvent_Plugin event("UnmapSurface");
        if (OK == pPolicyManager->GetEventManager()->HandleEvent(&event))
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    RTApiReceiver0<bool> UtilityManager_RegisterUnmapEvent_Receiver(UtilityManager_RegisterUnmapEvent);

    bool UtilityManager_RegisterMapEvent()
    {
        PolicyManager *pPolicyManager = PolicyManager::Instance();
        PmEvent_Plugin event("MapSurface");
        if (OK == pPolicyManager->GetEventManager()->HandleEvent(&event))
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    RTApiReceiver0<bool> UtilityManager_RegisterMapEvent_Receiver(UtilityManager_RegisterMapEvent);

    UINT16 UtilityManager_GetChipArgs(UtilityManager* pT3dUtilityManager, const char* const** argv)
    {
        const vector<const char*>& args = pT3dUtilityManager->GetChipArgs();
        *argv = static_cast<const char* const*>(&args[0]);
        return args.size();
    }

    UINT32 UtilityManager_TrainIntraNodeConnsParallel(UtilityManager* pT3dUtilityManager, UINT32 trainTo)
    {
        return pT3dUtilityManager->TrainIntraNodeConnsParallel(trainTo);
    }
////////////////////

    const char * shimFuncNames[] =
    {
#define USE_API_LIST
#define API_ENTRY(api) #api,
#include "api_list.h"
#undef API_ENTRY
        // last entry must be null string
        //
        ""
    };

    const pFlwoid shimFuncPtrs[] =
    {
#define USE_API_LIST
#define API_ENTRY(api) (pFlwoid) api,
#include "api_list.h"
#undef API_ENTRY
        // last entry must be zero
        0
    };

    ApiReceiver *apiReceivers[sizeof(shimFuncNames) + 1] =
    {
        // Every receiver should be exactly named as <API_NAME>_Receiver,
        // in order to use macro to expand this array easily.
#define USE_API_LIST
#define API_ENTRY(api) (&(api##_Receiver)),
// #include "api_list.h"
#undef API_ENTRY
        // last entry must be zero
        0
    };

    // Hack: function to zero apiReceivers and set the receivers we have.
    void InitApiReceivers()
    {
        memset(apiReceivers, 0, sizeof(apiReceivers));
        apiReceivers[ENUM_Host_getEventManager] = &Host_getEventManager_Receiver;
        apiReceivers[ENUM_Host_getGpuManager] = &Host_getGpuManager_Receiver;
        apiReceivers[ENUM_Host_getBufferManager] = &Host_getBufferManager_Receiver;
        apiReceivers[ENUM_Host_getMem] = &Host_getMem_Receiver;
        apiReceivers[ENUM_Host_getTraceManager] = &Host_getTraceManager_Receiver;
        apiReceivers[ENUM_Host_getUtilityManager] = &Host_getUtilityManager_Receiver;
        apiReceivers[ENUM_Host_getJavaScriptManager] = &Host_getJavaScriptManager_Receiver;
        apiReceivers[ENUM_Host_getDisplayManager] = &Host_getDisplayManager_Receiver;
        apiReceivers[ENUM_Host_getSOC] = &Host_getSOC_Receiver;
        apiReceivers[ENUM_Host_sendMessage] = &Host_sendMessage_Receiver;
        apiReceivers[ENUM_UtilityManager_Printf] = &UtilityManager_Printf_Receiver;
        apiReceivers[ENUM_EventManager_getEvent] = &EventManager_getEvent_Receiver;
        apiReceivers[ENUM_Event__register] = &Event__register_Receiver;
        apiReceivers[ENUM_UtilityManager__Yield] = &UtilityManager__Yield_Receiver;
        apiReceivers[ENUM_BufferManager_GetSurfaceByName] = &BufferManager_GetSurfaceByName_Receiver;
        apiReceivers[ENUM_SOC_IsSmmuEnabled] = &SOC_IsSmmuEnabled_Receiver;
        apiReceivers[ENUM_Surface_MapMemory] = &Surface_MapMemory_Receiver;
        apiReceivers[ENUM_Surface_GetTegraVirtAddr] = &Surface_GetTegraVirtAddr_Receiver;
        apiReceivers[ENUM_Surface_getGpuPhysAddr] = &Surface_getGpuPhysAddr_Receiver;
        apiReceivers[ENUM_Gpu_GobWidth] = &Gpu_GobWidth_Receiver;
        apiReceivers[ENUM_Gpu_GobHeight] = &Gpu_GobHeight_Receiver;
        apiReceivers[ENUM_Gpu_GobDepth] = &Gpu_GobDepth_Receiver;
        apiReceivers[ENUM_Gpu_GetGpcCount] = &Gpu_GetGpcCount_Receiver;
        apiReceivers[ENUM_Gpu_GetTpcCountOnGpc] = &Gpu_GetTpcCountOnGpc_Receiver;
        apiReceivers[ENUM_Gpu_GetGC6PlusIsIslandLoaded] = &Gpu_GetGC6PlusIsIslandLoaded_Receiver;
        apiReceivers[ENUM_Surface_GetSizeInBytes] = &Surface_GetSizeInBytes_Receiver;
        apiReceivers[ENUM_Surface_UnmapMemory] = &Surface_UnmapMemory_Receiver;
        apiReceivers[ENUM_BufferManager_DeclareTegraSharedSurface] = &BufferManager_DeclareTegraSharedSurface_Receiver;
        apiReceivers[ENUM_Surface_getPhysAddress] = &Surface_getPhysAddress_Receiver;
        apiReceivers[ENUM_Surface_getBAR1PhysAddress] = &Surface_getBAR1PhysAddress_Receiver;
        apiReceivers[ENUM_Surface_GetMemoryMappingMode] = &Surface_GetMemoryMappingMode_Receiver;
        apiReceivers[ENUM_Surface_SetMemoryMappingMode] = &Surface_SetMemoryMappingMode_Receiver;
        apiReceivers[ENUM_Surface_GetLocation] = &Surface_GetLocation_Receiver;
        apiReceivers[ENUM_Surface_SetLocation] = &Surface_SetLocation_Receiver;
        apiReceivers[ENUM_Surface_GetGpuCacheMode] = &Surface_GetGpuCacheMode_Receiver;
        apiReceivers[ENUM_Surface_SetGpuCacheMode] = &Surface_SetGpuCacheMode_Receiver;
        apiReceivers[ENUM_BufferManager_CreateDynamicSurface] = &BufferManager_CreateDynamicSurface_Receiver;
        apiReceivers[ENUM_Surface_SetPitch] = &Surface_SetPitch_Receiver;
        apiReceivers[ENUM_Surface_SetVASpace] = &Surface_SetVASpace_Receiver;
        apiReceivers[ENUM_Surface_Alloc] = &Surface_Alloc_Receiver;
        apiReceivers[ENUM_Surface_Free] = &Surface_Free_Receiver;
        apiReceivers[ENUM_SOC_AllocSyncPoint] = &SOC_AllocSyncPoint_Receiver;
        apiReceivers[ENUM_SOC_AllocSyncPointBase] = &SOC_AllocSyncPointBase_Receiver;
        apiReceivers[ENUM_SOC_SyncPoint_GetIndex] = &SOC_SyncPoint_GetIndex_Receiver;
        apiReceivers[ENUM_SOC_SyncPoint_Wait] = &SOC_SyncPoint_Wait_Receiver;
        apiReceivers[ENUM_SOC_SyncPoint_HostIncrement] = &SOC_SyncPoint_HostIncrement_Receiver;
        apiReceivers[ENUM_SOC_SyncPoint_GraphicsIncrement] = &SOC_SyncPoint_GraphicsIncrement_Receiver;
        apiReceivers[ENUM_SOC_SyncPointBase_GetIndex] = &SOC_SyncPointBase_GetIndex_Receiver;
        apiReceivers[ENUM_SOC_SyncPointBase_GetValue] = &SOC_SyncPointBase_GetValue_Receiver;
        apiReceivers[ENUM_SOC_SyncPointBase_SetValue] = &SOC_SyncPointBase_SetValue_Receiver;
        apiReceivers[ENUM_SOC_SyncPointBase_AddValue] = &SOC_SyncPointBase_AddValue_Receiver;
        apiReceivers[ENUM_SOC_SyncPointBase_CpuSetValue] = &SOC_SyncPointBase_CpuSetValue_Receiver;
        apiReceivers[ENUM_SOC_SyncPointBase_CpuAddValue] = &SOC_SyncPointBase_CpuAddValue_Receiver;
        apiReceivers[ENUM_UtilityManager_AllocSemaphore] = &UtilityManager_AllocSemaphore_Receiver;
        apiReceivers[ENUM_UtilityManager_FreeSemaphore] = &UtilityManager_FreeSemaphore_Receiver;
        apiReceivers[ENUM_Semaphore_Acquire] = &Semaphore_Acquire_Receiver;
        apiReceivers[ENUM_Semaphore_AcquireGE] = &Semaphore_AcquireGE_Receiver;
        apiReceivers[ENUM_Semaphore_Release] = &Semaphore_Release_Receiver;
        apiReceivers[ENUM_Semaphore_GetPhysAddress] = &Semaphore_GetPhysAddress_Receiver;
        apiReceivers[ENUM_Semaphore_SetReleaseFlags] = &Semaphore_SetReleaseFlags_Receiver;
        apiReceivers[ENUM_TraceManager_FlushMethodsAllChannels] = &TraceManager_FlushMethodsAllChannels_Receiver;
        apiReceivers[ENUM_TraceManager_WaitForIdleAllChannels] = &TraceManager_WaitForIdleAllChannels_Receiver;
        apiReceivers[ENUM_Mem_AllocIRam] = &Mem_AllocIRam_Receiver;
        apiReceivers[ENUM_Mem_FreeIRam] = &Mem_FreeIRam_Receiver;
        apiReceivers[ENUM_Surface_SetVideoProtected] = &Surface_SetVideoProtected_Receiver;
        apiReceivers[ENUM_UtilityManager_VAPrintf] = &UtilityManager_VAPrintf_Receiver;
        apiReceivers[ENUM_TraceManager_GetChannelProperties] = &TraceManager_GetChannelProperties_Receiver;
        apiReceivers[ENUM_TraceManager_GetNumChannels] = &TraceManager_GetNumChannels_Receiver;
        apiReceivers[ENUM_TraceManager_GetTraceChannelPtr] = &TraceManager_GetTraceChannelPtr_Receiver;
        apiReceivers[ENUM_TraceManager_MethodWrite] = &TraceManager_MethodWrite_Receiver;
        apiReceivers[ENUM_TraceManager_getTestName] = &TraceManager_getTestName_Receiver;
        apiReceivers[ENUM_TraceManager_RunTraceOps] = &TraceManager_RunTraceOps_Receiver;
        apiReceivers[ENUM_TraceManager_StartPerfMon] = &TraceManager_StartPerfMon_Receiver;
        apiReceivers[ENUM_TraceManager_StopPerfMon] = &TraceManager_StopPerfMon_Receiver;
        apiReceivers[ENUM_TraceManager_GetNumTraceOps] = &TraceManager_GetNumTraceOps_Receiver;
        apiReceivers[ENUM_TraceManager_getTraceOpLineNumber] = &TraceManager_getTraceOpLineNumber_Receiver;
        apiReceivers[ENUM_TraceManager_GetTraceOpProperties] = &TraceManager_GetTraceOpProperties_Receiver;
        apiReceivers[ENUM_TraceManager_StopVpmCapture] = &TraceManager_StopVpmCapture_Receiver;
        apiReceivers[ENUM_TraceManager_AbortTest] = &TraceManager_AbortTest_Receiver;
        apiReceivers[ENUM_UtilityManager_GetThreadId] = &UtilityManager_GetThreadId_Receiver;
        apiReceivers[ENUM_UtilityManager_FailTest] = &UtilityManager_FailTest_Receiver;

        apiReceivers[ENUM_FileProxy_Fopen] = &FileProxy_Fopen_Receiver;
        apiReceivers[ENUM_FileProxy_Fclose] = &FileProxy_Fclose_Receiver;
        apiReceivers[ENUM_FileProxy_Feof] = &FileProxy_Feof_Receiver;
        apiReceivers[ENUM_FileProxy_Ferror] = &FileProxy_Ferror_Receiver;
        apiReceivers[ENUM_FileProxy_Fwrite] = &FileProxy_Fwrite_Receiver;
        apiReceivers[ENUM_FileProxy_Fread] = &FileProxy_Fread_Receiver;
        apiReceivers[ENUM_FileProxy_Fgetc] = &FileProxy_Fgetc_Receiver;
        apiReceivers[ENUM_FileProxy_Fputc] = &FileProxy_Fputc_Receiver;
        apiReceivers[ENUM_FileProxy_Fgets] = &FileProxy_Fgets_Receiver;
        apiReceivers[ENUM_FileProxy_Fputs] = &FileProxy_Fputs_Receiver;
        apiReceivers[ENUM_FileProxy_Ftell] = &FileProxy_Ftell_Receiver;
        apiReceivers[ENUM_FileProxy_Fseek] = &FileProxy_Fseek_Receiver;

        apiReceivers[ENUM_Event_getGeneratedStringArg] = &Event_getGeneratedStringArg_Receiver;
        apiReceivers[ENUM_Event_getGeneratedUint32Arg] = &Event_getGeneratedUint32Arg_Receiver;

        apiReceivers[ENUM_UtilityManager_DisableIRQ] = &UtilityManager_DisableIRQ_Receiver;
        apiReceivers[ENUM_UtilityManager_EnableIRQ] = &UtilityManager_EnableIRQ_Receiver;
        apiReceivers[ENUM_UtilityManager_IsIRQHooked] = &UtilityManager_IsIRQHooked_Receiver;

        apiReceivers[ENUM_BufferManager_GetActualPhysAddr] = &BufferManager_GetActualPhysAddr_Receiver;
        apiReceivers[ENUM_UtilityManager_RegisterUnmapEvent] = &UtilityManager_RegisterUnmapEvent_Receiver;
        apiReceivers[ENUM_UtilityManager_RegisterMapEvent] = &UtilityManager_RegisterMapEvent_Receiver;

        apiReceivers[ENUM_Gpu_UnlockHost2Jtag] = &Gpu_UnlockHost2Jtag_Receiver;
        apiReceivers[ENUM_Gpu_ReadHost2Jtag] = &Gpu_ReadHost2Jtag_Receiver;
        apiReceivers[ENUM_Gpu_WriteHost2Jtag] = &Gpu_WriteHost2Jtag_Receiver;
    }

}
