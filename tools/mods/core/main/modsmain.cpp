/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Main for Modular Diagnostic Software (MODS).
//          --      -          -

#include <vector>
#include <string>
#include <ctime>
#include <memory>

#include "core/include/onetime.h"
#include "core/include/jscript.h"
#include "core/include/cmdline.h"
#include "core/include/rawui.h"
#include "core/include/simpleui.h"
#include "core/include/remotui.h"
#include "core/include/tee.h"
#include "core/include/filesink.h"
#include "core/include/sersink.h"
#include "core/include/circsink.h"
#include "core/include/debuglogsink.h"
#include "core/include/version.h"
#include "core/include/xp.h"
#include "core/include/utility.h"
#include "core/include/massert.h"
#include "core/include/pool.h"
#include "core/include/log.h"
#include "core/include/platform.h"
#include "core/include/mgrmgr.h"
#include "core/include/cnslmgr.h"
#include "core/include/errcount.h"
#include "core/include/jsonlog.h"
#include "core/include/abort.h"
#include "core/include/errormap.h"
#include "core/include/restarter.h"
#include "core/include/fieldiag.h"
#include "core/include/heartbeatmonitor.h"

#include "core/include/mods_gdm_client.h"


// So we can free external test JS-related memory while we're shutting
#include "core/include/memcheck.h"
#include "lwdiagutils.h"

#if defined(BOOST_ENABLE_ASSERT_HANDLER)
namespace boost
{
    void assertion_failed_msg(
        char const *expr,
        char const *msg,
        char const *function,
        char const *file,
        long line
    )
    {
        string hackMsg = msg;
        hackMsg += '(';
        hackMsg += function;
        hackMsg += ')';
        ModsAssertFail(file, line, hackMsg.c_str(), expr);
    }

    void assertion_failed(
        char const * expr,
        char const * function,
        char const * file,
        long line
    )
    {
        ModsAssertFail(file, line, function, expr);
    }
} // namespace boost
#endif

#if defined(BOOST_NO_EXCEPTIONS)
namespace boost
{
    void throw_exception(std::exception const&)
    {
        Platform::BreakPoint(RC::ASSERT_FAILURE_DETECTED, __FILE__, __LINE__);
    }
}
#endif

// Class to enable and disable the watchdogs
class EnableWatchdog
{
public:
    // Constructor to initialize watchdogs with specified number of seconds
    EnableWatchdog(UINT32 timeoutSecs)
    {
        m_bEnabled = false;
        if (timeoutSecs > 0)
        {
            if (RC::OK != Xp::InitWatchdogs(timeoutSecs))
            {
                Printf(Tee::PriWarn, "Watchdog enable failed!!\n");
            }
            else
            {
                m_bEnabled = true;
            }
        }
    }

    // Destructor to reset the watchdogs
    ~EnableWatchdog()
    {
        if (m_bEnabled)
        {
            if (RC::OK != Xp::ResetWatchdogs())
            {
                Printf(Tee::PriWarn, "Watchdog reset failed!!\n");
            }
        }
    }

private:
    bool m_bEnabled;
};

// Runs JavaScript engine
RC RunJavaScript(JavaScript * pJavaScript, JSObject * pGlobObj)
{
    PROFILE_ZONE(GENERIC)

    RC rc;

    // Initialize the watchdogs
    EnableWatchdog watchdogs(CommandLine::WatchdogTimeout());

#ifdef BOUND_JS
    CHECK_RC(pJavaScript->RunBoundScript());
#else
    // Import the main script file.
    // If the user did not specify a script file (Argv[0]),
    // default to mods.js. Search for the script file in the same
    // directory as the program or in 'js' subdirectories.
    if (0 == CommandLine::Argv().size())
    {
        vector<string> paths;
        char   pathSeparator[2];
        string programPath = CommandLine::ProgramPath();
        pathSeparator[0] = '/';
        pathSeparator[1] = '\0';
        paths.push_back(programPath);
        paths.push_back(programPath + "js" + pathSeparator);
        paths.push_back(string("js") + pathSeparator);

        string scriptFile = "mods.js";
        string path = Utility::FindFile(scriptFile, paths);
        CHECK_RC(pJavaScript->Import(path + scriptFile));
    }
    else
    {
        CHECK_RC(pJavaScript->Import(CommandLine::Argv()[0]));
    }
#endif // BOUND_JS

    // Import the optional script files if necessary.
    vector<string>::const_iterator it;
    for
    (
        it  = CommandLine::ImportFileNames().begin();
        it != CommandLine::ImportFileNames().end();
        ++it
    )
    {
        CHECK_RC(pJavaScript->Import(*it));
    }

    // Execute the begin() method. begin() is the first called method.
    pJavaScript->CallMethod(pGlobObj, "begin");

    // Run the instant scripts, or pre-scripts before main() and post-scripts
    // after main(), if necessary.

    // Enable mods that can always enter interactive mode when given "-s" option
    // regardless of errors in main script or instant scripts.
    RC preInterfaceRc;

    if (CommandLine::InstantScripts().size() == 0)
    {
        // Run the pre-script if necessary.
        for
        (
            it  = CommandLine::PreScripts().begin();
            it != CommandLine::PreScripts().end();
            ++it
        )
        {
            pJavaScript->RunScript(*it);
        }

        // Exlwte the main() function if necessary.
        if (CommandLine::RunMain())
        {
            jsval retVal = JSVAL_NULL;
            CHECK_RC(pJavaScript->CallMethod(pGlobObj, "main", JsArray(), &retVal));
            MASSERT(retVal != JSVAL_NULL);
            INT32 mainRcInt = OK;
            CHECK_RC(pJavaScript->FromJsval(retVal, &mainRcInt));
            RC mainRc(mainRcInt);

            // We are done if main() failed.
            if (mainRc != RC::OK)
            {
                preInterfaceRc = mainRc;
            }
        }

        if (RC::OK == preInterfaceRc)
        {
            // Run the post-script if necessary.
            for
            (
                it  = CommandLine::PostScripts().begin();
                it != CommandLine::PostScripts().end();
                ++it
            )
            {
                pJavaScript->RunScript(*it);
            }
        }
    }
    else
    {
        for
        (
            it  = CommandLine::InstantScripts().begin();
            it != CommandLine::InstantScripts().end();
            ++it
        )
        {
            jsval scriptReturlwal;
            INT32 scriptReturnedRc;
            RC scriptParseRc = pJavaScript->RunScript(*it, &scriptReturlwal);
            RC extractValRc = pJavaScript->FromJsval(scriptReturlwal, &scriptReturnedRc);

            if (RC::OK != scriptParseRc)    // syntax error
            {
                preInterfaceRc = scriptParseRc;
                break;
            }
            if (RC::OK != extractValRc)     // can't extract return value
            {
                preInterfaceRc = extractValRc;
                break;
            }
            if (RC::OK != scriptReturnedRc) // javascript returned !OK
            {
                preInterfaceRc = scriptReturnedRc;
                break;
            }
        }
    }

    // Start the simple or raw user interface.
    if (CommandLine::UserConsole() == CommandLine::UC_Local)
    {
        if (CommandLine::UserInterface() == CommandLine::UI_Raw)
        {
            CHECK_RC(ConsoleManager::Instance()->SetConsole("raw", true));
        }

        if ((CommandLine::UserInterface() == CommandLine::UI_Script) ||
            (CommandLine::UserInterface() == CommandLine::UI_Macro) ||
            (CommandLine::UserInterface() == CommandLine::UI_Raw))
        {
            SimpleUserInterface * pInterface = new SimpleUserInterface(
                    CommandLine::UserInterface() == CommandLine::UI_Script);
            pInterface->Run();
            delete pInterface;
        }
    }
    else if ((CommandLine::UserConsole() == CommandLine::UC_Remote)
              && Xp::IsRemoteConsole())
    {
        if (CommandLine::UserInterface() == CommandLine::UI_Message)
        {
            // Newer CDM remote UI
            RemoteUserInterface * pInterface = new RemoteUserInterface(Xp::GetRemoteConsole(), true);
            pInterface->Run();
            delete pInterface;
        }
        else if (CommandLine::UserInterface() != CommandLine::UI_None)
        {
            // Old-style remote UI
            RemoteUserInterface * pInterface = new RemoteUserInterface(Xp::GetRemoteConsole(), false);
            pInterface->Run();
            delete pInterface;
        }
    }

    return preInterfaceRc;
}

class AccessTokenCommands : public CommandLine::CmdExtender
{
public:
    AccessTokenCommands() {}
    virtual ~AccessTokenCommands() { }

private:
    bool DoProcessOption
    (
        const vector<string> &args,
        size_t *lwrOption,
        vector<string> *pArgv
    ) override
    {
        if (nullptr == lwrOption)
        {
            return false;
        }

        size_t start;
        if (acquireAccessTokenOption.extenderOption == args[*lwrOption])
        {
            pArgv->push_back(acquireAccessTokenOption.modsOption);
            return true;
        }
        else if (releaseAccessTokenOption.extenderOption == args[*lwrOption])
        {
            pArgv->push_back(releaseAccessTokenOption.modsOption);
            return true;
        }
        else if (string::npos != (start = args[*lwrOption].find(accessTokenOption.extenderOption)))
        {
            string accessTokStr;
            accessTokStr.assign(
                args[*lwrOption],
                start + accessTokenOption.extenderOption.size(),
                string::npos
            );
            pArgv->push_back(accessTokenOption.modsOption);
            pArgv->push_back(accessTokStr);
            return true;
        }

        return false;
    }

    static const ExtendedOptionMapping accessTokenOption;
    static const ExtendedOptionMapping acquireAccessTokenOption;
    static const ExtendedOptionMapping releaseAccessTokenOption;
};
const CommandLine::CmdExtender::ExtendedOptionMapping AccessTokenCommands::accessTokenOption = { "access_token=", "-access_token" };                       //$
const CommandLine::CmdExtender::ExtendedOptionMapping AccessTokenCommands::acquireAccessTokenOption = { "acquire_access_token", "-acquire_access_token" }; //$
const CommandLine::CmdExtender::ExtendedOptionMapping AccessTokenCommands::releaseAccessTokenOption = { "release_access_token", "-release_access_token" }; //$

class StealthModeCommands : public CommandLine::CmdExtender
{
    bool   logFileNameWasSet;

public:
    StealthModeCommands() : logFileNameWasSet(false) { }

    RC AddJSProperties()
    {
        RC rc;
        JavaScriptPtr js;
        JSObject *globObj = js->GetGlobalObject();

        CHECK_RC(js->SetProperty(globObj, "HasLogFileName", HasLogFileName()));

        return rc;
    }

private:
    bool HasLogFileName() const
    {
        return logFileNameWasSet;
    }

    bool DoProcessOption
    (
        const vector<string> &args,
        size_t *lwrOption,
        vector<string> *pArgv
    )
    {
        if (0 == lwrOption)
        {
            return false;
        }

        size_t start;
        if (string::npos != (start = args[*lwrOption].find(logFileNameOption.extenderOption)))
        {
            string logFileName;
            logFileName.assign(
                args[*lwrOption],
                start + logFileNameOption.extenderOption.size(),
                string::npos
            );
            pArgv->push_back(logFileNameOption.modsOption);
            pArgv->push_back(logFileName);
            logFileNameWasSet = true;
            return true;
        }
        else if (supressLogFileOption.extenderOption == args[*lwrOption])
        {
            pArgv->push_back(supressLogFileOption.modsOption);
            return true;
        }
        else if ((g_FieldDiagMode == FieldDiagMode::FDM_617) &&
                 logOnErrorOption.extenderOption == args[*lwrOption])
        {
            pArgv->push_back(logOnErrorOption.modsOption);
            return true;
        }

        return false;
    }

    static const ExtendedOptionMapping logFileNameOption;
    static const ExtendedOptionMapping supressLogFileOption;
    static const ExtendedOptionMapping logOnErrorOption;
};

const CommandLine::CmdExtender::ExtendedOptionMapping StealthModeCommands::logFileNameOption = { "logfilename=", "-l" };        //$
const CommandLine::CmdExtender::ExtendedOptionMapping StealthModeCommands::supressLogFileOption = { "nolog", "-suppress_log" }; //$
const CommandLine::CmdExtender::ExtendedOptionMapping StealthModeCommands::logOnErrorOption = { "logonfail", "-L" };            //$

class DgxCommands : public CommandLine::CmdExtender
{
public:
    DgxCommands() {}

private:
    bool DoProcessOption(const vector<string> &args, size_t *lwrOption, vector<string> *pArgv)
    {
        if (nullptr == lwrOption)
        {
            return false;
        }

        if (appendLogOption.extenderOption == args[*lwrOption])
        {
            pArgv->push_back(appendLogOption.modsOption);
            return true;
        }
        else if (mleLimitedOption.extenderOption == args[*lwrOption])
        {
            pArgv->push_back(mleLimitedOption.modsOption);
            return true;
        }

        return false;
    }

    static const ExtendedOptionMapping appendLogOption;
    static const ExtendedOptionMapping mleLimitedOption;
};
const CommandLine::CmdExtender::ExtendedOptionMapping DgxCommands::appendLogOption = { "append_log", "-a" }; //$
const CommandLine::CmdExtender::ExtendedOptionMapping DgxCommands::mleLimitedOption = {"mle_limited", "-mle_limited"};

//------------------------------------------------------------------------------
//  main entry point
//------------------------------------------------------------------------------

namespace
{
    shared_ptr<ProcCtrl::Restarter> g_Restarter;
    RC PostResults(RC mainRc, RC finalRc)
    {
        RC rc;
        // If MODS is controlled by a controller process, post our exelwtion results.
        if (g_Restarter)
        {
            RC postRc = (finalRc != RC::OK) ? finalRc : mainRc;

            ProcCtrl::Performer *performer = nullptr;
            CHECK_RC(g_Restarter->GetPerformer(&performer));
            MASSERT(performer);
            if (performer->HasResultPoster())
            {
                CHECK_RC(performer->IlwokeResultPoster(postRc));
            }
            else
            {
                if (RC::OK == postRc)
                {
                    CHECK_RC(performer->PostSuccess(postRc));
                }
                else
                {
                    CHECK_RC(performer->PostFailure(postRc));
                }
            }
        }
        return rc;
    }
}

RC ModsMain
(
    int    argc,
    char * argv[]
)
{
    RC rc;
    StickyRC finalRc;
    vector<string> args(argv, argv + argc);

    bool appendLog = false;

    Xp::SetElw("MODS_EC", "");

    vector<CommandLine::CmdExtender*> cmdExtenders;

    // Add additional options to the stealth mode MODS. Stealth mode MODS has
    // a very limited set of options, all of them are in the form
    // option[=argument]. Most of them are treated in FIELDDIAG_end_user.js. We
    // cannot process log file name option there, because the log file must be
    // opened before the start of JavaScript engine. StealthModeCommands class
    // injects specific code inside CommandLine::Parse.
    StealthModeCommands stealthModeCommands;
    DgxCommands         dgxCommands;
    AccessTokenCommands accessTokCommands;
    if (g_FieldDiagMode != FieldDiagMode::FDM_NONE)
    {
        cmdExtenders.push_back(&accessTokCommands);
        if (g_FieldDiagMode == FieldDiagMode::FDM_DGXFIELDDIAG)
            cmdExtenders.push_back(&dgxCommands);
        else
            cmdExtenders.push_back(&stealthModeCommands);
    }

    MASSERT(argc > 0);
    MASSERT(argv != 0);

    // For fieldiag we need to always post results otherwise the exelwtable can
    // return failure when it really succeeds
    DEFERNAME(postResults)
    {
        PostResults(rc, finalRc);
    };

    // Defer reporting any errors from command line parsing until after Xp:OnEntry.
    // MODS shutdown procedures are not tolerant of smooth shutdown if Xp::OnEntry
    // has never been called
    rc = CommandLine::Parse(args, cmdExtenders);

    // Platform specific initialization code.  This code can potentially reset the command
    // line arguments if the system is running in remote mode (either client or server).
    // Capture the command line arguments and if they have changed after OnEntry then
    // reparse them
    vector<string> oldArgs = args;
    RC xpRc = Xp::OnEntry(&args);

    if (rc == RC::OK)
        rc = xpRc;
    CHECK_RC(rc);

    if (oldArgs != args)
    {
        CommandLine::Reset();
        CHECK_RC(CommandLine::Parse(args, cmdExtenders));
    }

    if (CommandLine::AccessTokenAcquire() || CommandLine::AccessTokenRelease())
        return RC::OK;

    // Initialize Ctrl-C handler with 10s delay
    Abort AbortObject(10000);

    DEFER { LwDiagUtils::NetShutdown(); };
    CHECK_RC(CommandLine::Handle());

    appendLog = CommandLine::GetAppendToLogFile();
    if (g_Restarter && !appendLog)
    {
        // We are controlled by another process that can restart the testing
        // transparently to the user. So we truncate the log file only once,
        // letting all restarted instances to append the log.
        ProcCtrl::Performer *performer = nullptr;
        CHECK_RC(g_Restarter->GetPerformer(&performer));
        MASSERT(performer);
        size_t restartCount;
        CHECK_RC(performer->GetStartsCount(&restartCount));
        if (0 < restartCount)
        {
            appendLog = true;
        }
    }

    // Open the mle log file if mle logging has not been disabled
    if (CommandLine::MleLogEnabled())
    {
        FileSink * pMleSink = Tee::GetMleSink();
        if (!CommandLine::OnlyWriteFileOnFail() && pMleSink != 0)
        {
            string mleLogFileName = CommandLine::MleLogFileName();
            if (mleLogFileName.empty())
            {
                mleLogFileName = "mods.mle";
            }

            UINT32 modeMask = FileSink::MM_MLE;
            if (appendLog)
            {
                modeMask |= FileSink::MM_APPEND;
            }

            if (CommandLine::GetEncryptFiles())
            {
                modeMask |= FileSink::MM_ENCRYPT;
            }

            rc = pMleSink->Open(modeMask, mleLogFileName);
            if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
            {
                // Ignore FileSink::Open()'s return code because it will fail
                // if we are trying to open a file on a read-only drive.
                rc.Clear();
            }
            CHECK_RC(rc);
        }
    }

    string gdmAddress = CommandLine::GdmAddress();
    if (!gdmAddress.empty())
    {
        CHECK_RC(ModsGdmClient::Instance().InitModsGdmClient(gdmAddress));
    }
    DEFER
    {
        ModsGdmClient::Instance().StopModsGdmClient();
    };

    // Start heartbeat monitor thread if enabled
    if (CommandLine::HeartBeatMonitorEnabled())
    {
        CHECK_RC(HeartBeatMonitor::InitMonitor());
    }
    DEFER
    {
        HeartBeatMonitor::StopHeartBeat();
    };
    // When debugging Javascript, the console must be the raw console (since
    // the JS debugger uses stdin/stdout directly).  This conflicts directly
    // with the nlwrses console, and will likely cause the debugger output to
    // not be seen if the MODS console is in use
    if (CommandLine::DebugJavascript())
    {
        CHECK_RC(ConsoleManager::Instance()->SetConsole("raw", true));
        ConsoleManager::Instance()->SetConsoleLocked(true);
        Printf(Tee::PriNormal,
                "********************************************************\n"
                "*  Console set to \"raw\" and locked for JS debugging!!  *\n"
                "********************************************************\n");
    }

    if ((g_FieldDiagMode == FieldDiagMode::FDM_629) && g_Restarter)
    {
        ProcCtrl::Performer *performer = nullptr;
        CHECK_RC(g_Restarter->GetPerformer(&performer));
        MASSERT(performer);
        CHECK_RC(performer->ConnectResultPoster(FieldiagRestartCheck));
    }

    // Open the log file if is not "null" and not only write-file-on-fail.
    FileSink * pFileSink = 0;
    if (!CommandLine::SuppressLogFile() && !CommandLine::OnlyWriteFileOnFail())
    {
        // Some Tee subclasses may not return a valid FileSink*.
        pFileSink = Tee::GetFileSink();
        if (pFileSink != 0)
        {
            pFileSink->SetFileSplitMb(CommandLine::LogFileSplitSize());
            rc = pFileSink->Open(CommandLine::LogFileName(),
                                 appendLog ? FileSink::Append : FileSink::Truncate);
            if ((Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK) &&
                (g_FieldDiagMode != FieldDiagMode::FDM_617))
            {
                // Ignore FileSink::Open()'s return code because it will fail if
                // we are trying to open a file on a read-only drive.
                rc.Clear();
            }
            CHECK_RC(rc);
        }
    }

    // Set the output levels.
    Tee::SetScreenLevel(CommandLine::ScreenLevel());
    Tee::SetFileLevel(CommandLine::FileLevel());
    Tee::SetMleLevel(CommandLine::MleLevel(), CommandLine::MleStdOut());
    Tee::SetSerialLevel(CommandLine::SerialLevel());
    Tee::SetCirlwlarLevel(CommandLine::CirlwlarLevel());
    Tee::SetDebuggerLevel(CommandLine::DebuggerLevel());
    Tee::SetEthernetLevel(CommandLine::EthernetLevel());

    Tee::SetMleLimited(CommandLine::IsMleLimited());

    // Start capturing kernel log after mods.log has been opened, otherwise
    // the beginning of the kernel log will go only to stdout and not to mods.log.
    CHECK_RC(Xp::StartKernelLogger());

    // calling Printf() before this point doesn't get to the mods.log.
    OneTimeInit::PrintStageTimestamp(Mods::Stage::Start);
    // Create the JavaScript engine.
    constexpr size_t runtimeMaxBytes  = 8 * 1024 * 1024;
    constexpr size_t contextStackSize = 8 * 1024;
    JavaScript * pJavaScript = new JavaScript(runtimeMaxBytes, contextStackSize);
    if (0 == pJavaScript)
    {
        return RC::CANNOT_ALLOCATE_MEMORY;
    }
    CHECK_RC(pJavaScript->GetInitRC());
    JavaScriptPtr::Install(pJavaScript);
    JSObject * pGlobObj = pJavaScript->GetGlobalObject();

    if (g_FieldDiagMode != FieldDiagMode::FDM_NONE)
    {
        CHECK_RC(stealthModeCommands.AddJSProperties());
    }

    // Make JS functions detect and log any MASSERTS that happen within them.
    MassertErrCounter massertErrCounterInJs;
    CHECK_RC(massertErrCounterInJs.Initialize("Massert", 1, 0, nullptr,
                                              ErrCounter::JS_FUNCTION,
                                              ErrCounter::MASSERT_PRI));

    // Makes mods tests return an error if a MASSERT happens during exelwtion.
    MassertErrCounter massertErrCounterInModsTest;
    CHECK_RC(massertErrCounterInModsTest.Initialize("Massert", 1, 0, nullptr,
                                                    ErrCounter::MODS_TEST,
                                                    ErrCounter::MASSERT_PRI));

    // Makes mods tests return an error if an unexpected HW interrupt
    // happens during exelwtion.
    // Controlled from JS properties:
    //g_UnexpectedHwIntVerbose
    //g_UnexpectedHwIntAllowed (per test limit)
    //g_UnexpectedHwIntAllowedTotal
    //g_UnexpectedHwIntDisabled
    //
    UnexpectedInterruptErrCounter unexpectedInterruptErrCounterInModsTest;
    CHECK_RC(unexpectedInterruptErrCounterInModsTest.Initialize(
                "UnexpectedHwInt", 1, 0, nullptr,
                ErrCounter::MODS_TEST,
                ErrCounter::UNEXPECTED_INTERRUPT_PRI));

    CHECK_RC(ErrorLogger::Initialize());

    if (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK)
    {
        Printf(Tee::PriAlways,
            "******************************************************************\n"
            "*  WARNING                                              WARNING  *\n"
            "*                                                                *\n"
            "*                   INTERNAL ONLY MODS UNLOCKED                  *\n"
            "*                    Not for use outside LWPU                  *\n"
            "*                                                                *\n");
        if (Utility::OnLwidiaIntranet())
        {
            if (CommandLine::InternalBypassPassed())
            {
                // Print when bypass.INTERNAL.bin is provided along with being on
                // lwpu network. In that case, authentication happens either because
                // of being on lwpu network or because of bypass file.
                // Authentication could happen due to bypass file because of possibly
                // authentication servers are not available from your subnet or system
                // does not have the required or system doesn't have OpenSSL 1.1.0f or later
                Printf(Tee::PriAlways,
                    "*      Authenticated on LWPU network and with bypass file      *\n");
            }
            else
            {
                Printf(Tee::PriAlways,
                    "*                   Authenticated on LWPU network              *\n");
            }
        }
        else
        {
           // Not on lwpu internet but bypass.INTERNAL.bin file is passed
            Printf(Tee::PriAlways,
                "*                   Authenticated with bypass file               *\n");
        }
        Printf(Tee::PriAlways,
            "*  WARNING                                              WARNING  *\n"
            "******************************************************************\n");
        if (g_Changelist != 0)
        {
            Printf(Tee::PriLow, "MODS built from P4 changelist %d\n", g_Changelist);
        }
        else
        {
            Printf(Tee::PriLow, "No embedded P4 changelist number\n");
        }
    }
    // print MODS_COMMENT
    string ModsComment = Xp::GetElw("MODS_COMMENT");
    if (!ModsComment.empty())
    {
        Printf(Tee::PriAlways, "Environment Log: %s\n", ModsComment.c_str());
    }

    // Run JavaScript engine
    finalRc = RunJavaScript(pJavaScript, pGlobObj);

    postResults.Cancel();
    finalRc = PostResults(rc, finalRc);

    // Execute the end() method. end() is the last called method.
    {
        PROFILE_NAMED_ZONE(GENERIC, "end")
        finalRc = pJavaScript->CallMethod(pGlobObj, "end");
    }

    JavaScriptPtr pJs;
    bool enableDceWar = false;
    pJs->GetProperty(pJs->GetGlobalObject(), "g_EnableDceRmDelayWAR", &enableDceWar);
    if (enableDceWar)
    {
        Platform::Delay(100000);
    }

    if ((finalRc != RC::OK) && CommandLine::OnlyWriteFileOnFail())
    {
        FileSink::Mode OpenMode = appendLog ? FileSink::Append : FileSink::Truncate;

        // Ignore FileSink::Open()'s return code because it will fail if we are
        // trying to open a file on a read-only drive.
        // Some Tee subclasses may not return a valid FileSink*.
        pFileSink = Tee::GetFileSink();
        if (pFileSink != 0)
            pFileSink->Open(CommandLine::LogFileName(), OpenMode);
        Tee::GetCirlwlarSink()->Dump(Tee::FileOnly);
    }

    INT32 lev = CommandLine::CirlwlarDumpOnExitLevel();
    if (lev != Tee::LevNone)
    {
        Tee::GetCirlwlarSink()->Dump(lev);
    }

    // Flush debug log sink
    Tee::GetDebugLogSink()->Flush();

    ConsoleManager *pConsoleManager = ConsoleManager::Instance();
    finalRc = pConsoleManager->SetConsole(pConsoleManager->GetDefaultConsole(),
                                        false);

    // Make sure the user interface is enabled
    //
    while (Platform::IsUserInterfaceDisabled())
    {
        Printf(Tee::PriWarn,
               "Exiting with user interface still disabled!!\n");
        Platform::EnableUserInterface();
    }

    finalRc = Xp::PreOnExit();

    finalRc = ErrorLogger::Shutdown();

    ErrorMap error(finalRc);
    Log::SetFirstError(error);

    ProcCtrl::ExitStatusEnum exitStatus = ProcCtrl::ExitStatus::unknown;
    if (g_Restarter)
    {
        ProcCtrl::Performer *performer = nullptr;
        CHECK_RC(g_Restarter->GetPerformer(&performer));
        MASSERT(performer);
        CHECK_RC(performer->GetLastPostedResult(&exitStatus));
    }

    if (ProcCtrl::ExitStatus::needRestart != exitStatus)
    {
        pJavaScript->CallMethod(pGlobObj, "PrintErrorCode");
    }

    // Enable leak threshold if running in PVS
    bool enable = 0;
    UINT32 maxleaks = (UINT32)-1;

    pJs->GetProperty(pJs->GetGlobalObject(), "g_EnableLeakThreshold", &enable);
    pJs->GetProperty(pJs->GetGlobalObject(), "g_MaximumLeakCount", &maxleaks);

    if (enable)
        Pool::EnforceThreshold((INT64)maxleaks);

    // Free the JavaScript engine.
    finalRc = massertErrCounterInJs.ShutDown(true);

    finalRc = Log::FirstError();

    JavaScriptPtr::Free();

    // make conscessions for PVS which uses the ethernet.
    if (Xp::IsRemoteConsole())
    {
        OneTimeInit::Timestamp(Mods::Stage::End);
        OneTimeInit::PrintStageTimestamp(Mods::Stage::End);
    }
    return finalRc;
}

//------------------------------------------------------------------------------
//  Exelwtes ModsMain() and handles error codes.
//------------------------------------------------------------------------------

int main
(
    int    argc,
    char * argv[]
)
{
    PROFILE_ZONE(GENERIC)

    // Initialize leak checker.  Perform this initialization here after all
    // globals are initialized.  We cannot init leak checking in OneTimeInit
    // reliably, because the order of initialization and destruction of globals
    // is undefined.
    Pool::LeakCheckInit();

    RC rc;

#ifdef BOUND_JS
    bool isController = false;

    if (g_FieldDiagMode == FieldDiagMode::FDM_629)
    {
        g_Restarter.reset(CreateProcessRestarter());
        CHECK_RC(g_Restarter->IsController(&isController));
    }

    if (g_Restarter && isController)
    {
        ProcCtrl::ExitStatusEnum exitStatus;
        int exitCode = 0;
        bool exitedNormally = false;
        RC childStartRes;
        RC childRC;
        ProcCtrl::Controller *ctrl(nullptr);
        g_Restarter->GetController(&ctrl);
        MASSERT(ctrl);
        do
        {
            // exitedNormally means that the child process exited by calling
            // _exit (in Linux) or ExitProcess (in Windows), it didn't hang,
            // wasn't terminated by a signal, etc. See 3 from
            // http://lwbugs/1703848/32.
            childStartRes = ctrl->StartNewAndWait(argc, argv, &exitedNormally, &exitStatus,
                                                  &exitCode, &childRC);
        } while (exitedNormally && RC::OK == childStartRes &&
                 ProcCtrl::ExitStatus::needRestart == exitStatus);

        return ProcCtrl::ExitStatus::success == exitStatus ? 0 : Utility::GetShellFailStatus();
    }
    else
#endif
    {
        rc = ModsMain(argc, argv);
    }

    // Report an error if there are any unused arguments
    // (which could also be misspelled arguments), no prints may occur
    // after Xp::OnExit.  Keep this check here
    if (CommandLine::ArgDb()->CheckForUnusedArgs(true) > 0)
    {
        Printf(Tee::PriAlways,
            "Error : Unused command line arguments, check your "
            "command line for typos.\n");
        if (rc == RC::OK)
            rc = RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    return Utility::ExitMods(rc, Utility::ExitNormally);
}
