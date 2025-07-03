/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2012,2016-2019, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Command line parsing and state information.

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef INCLUDED_CMDLINE_H
#define INCLUDED_CMDLINE_H

#ifndef INCLUDED_RC_H
#include "rc.h"
#endif
#ifndef INCLUDED_TEE_H
#include "tee.h"
#endif
#ifndef INCLUDED_STL_VECTOR
#define INCLUDED_STL_VECTOR
#include <vector>
#endif
#ifndef INCLUDED_STL_MAP
#define INCLUDED_STL_MAP
#include <map>
#endif
#ifndef INCLUDED_STL_STRING
#define INCLUDED_STL_STRING
#include <string>
#endif

#ifndef INCLUDED_ARGDB_H
#include "argdb.h"
#endif
#ifndef INCLUDED_ARGREAD_H
#include "argread.h"
#endif

class JsParamDecl;
class JsParamList;

namespace CommandLine
{
    enum UI
    {
        UI_None,
        UI_Script,
        UI_Macro,
        UI_Raw,
        UI_Message,
        UI_Count
    };

    enum UC
    {
        UC_Local,
        UC_Remote
    };

    class CmdExtender
    {
    public:
        virtual ~CmdExtender()
        {}

        bool ProcessOption(const vector<string> &args, size_t *lwrArgument, vector<string> *pArgv)
        {
            return DoProcessOption(args, lwrArgument, pArgv);
        }
    protected:
        struct ExtendedOptionMapping
        {
            string extenderOption;  // option that the extender looks for
            string modsOption;      // corresponding MODS option (if one exists)
        };
    private:
        // Descendant's implementation of DoProcessOption can parse command line
        // options. If an option was processed, the descendant class returns
        // true. In this case the option will be removed from the further
        // processing by CommandLine::Parse. A descendant also can modify
        // lwrOption if an option required arguments. lwrOption has to be
        // shifted to the number of argumnets for a particular option.
        virtual bool DoProcessOption
        (
            const vector<string> &args
           ,size_t               *lwrOption
           ,vector<string>       *pArgv
        ) = 0;
    };

    void Initialize();
    void Shutdown();

    RC Parse(const vector<string> &args, const vector<CmdExtender*> &extenders);
        // Parse the command line arguments.

    RC Parse(const vector<string> &args, CmdExtender *extender);
        // Parse the command line arguments.

    RC Parse(const vector<string> &args);
        // Parse the command line arguments.

    RC Handle();
        // Handles the command line arguments.

    void Reset();
        // Resets the command line parsing state.

    bool GetAppendToLogFile();
        // Should we append to the log file?

    bool RecordUserInput();
        // Should we record user input to the log file?

    bool RunMain();
        // Should we run main() script method?

    bool LogReturnCodes();
        // Should we log STest return codes?

    INT32 CirlwlarDumpOnExitLevel();
        // Level at which we dump the cirlwlar buffer Tee::LevNone -> disabled

    bool OnlyWriteFileOnFail();
        // Should we dump the cirlwlar buffer to Tee::FileOnly on exit?

    void SetOnlyWriteFileOnFail();
        // Sets dumping the cirlwlar buffer to Tee::FileOnly on exit.

    string ProgramPath();
        // Complete program path including last path separator.

    string ScriptFilePath();
        // Complete path including last path separator of the script file parsed
        // and exelwted by MODS.

    const vector<string> & Argv();
        // List of arguments passed to the script file. Argv[0] is the name of
        // of the script file.

    void SetScriptArgv(const vector<string> &newArgv);

    ArgDatabase * ArgDb();
        // Argument database containing the list of arguments passed to the
        // script file.  Initially created empty, arguments are added to it
        // from the JS file

    JsParamList * ParamList();

    ArgReader * GetJsArgReader();

    Tee::Level ScreenLevel();
    Tee::Level FileLevel();
    Tee::Level MleLevel();
    Tee::Level SerialLevel();
    Tee::Level CirlwlarLevel();
    Tee::Level DebuggerLevel();
    Tee::Level EthernetLevel();
        // Screen and file output priorities.

    bool IsMleLimited();

    bool FailOnAssert();
    void SetJsFailOnAssert(bool State);
        // Should mods exit with failure code on assert(only in release mods)

    const vector<string> & ImportFileNames();
        // List of files to import.

    string LogFileName();
        // Name of log file.

    UINT32 LogFileSplitSize();
        // Size in MBs to divide log file into

    string MleLogFileName();
        // Name of mle log file.

    bool MleLogEnabled();
        // Is Mle logging enabled (true by default).

    bool HeartBeatMonitorEnabled();
        // Is Heart beat monitor enabled (false by default).

    bool MleStdOut();
        // Is Mle to be printed to stdout.

    const vector<string> & InstantScripts();
    const vector<string> & PreScripts();
    const vector<string> & PostScripts();
        // List of scripts to immediately execute, before main() and after main().

    UI   UserInterface();
    void SetJsUserInterface(UI State);
        // Type of user interface to run.

    bool RemoteUserInterface();
        // Whether the user interface is remote.

    UC   UserConsole();
        // Location of user console.

    enum BIOSTYPE
    {
        SOC_FIRMWARE = 128
    };

    const map<UINT32, string> & GpuBiosFileNames();
        // VBIOS file names used in GPU simulation runs, indexed by device

    string GpuNetlistName();
        // Netlist file name used in GPU emulation/simulation runs

    string GdmAddress();
        // IP Address and Port Num Gdm is listening on for clients

    string GpuRTLArgs();
        // Arguments used to initialize the GPU simulator.

    string GpuChipLib();
        // GPU chip library.

    bool GetEncryptFiles();
        // Whether files generated by MODS should be encrypted.

    bool DebugJavascript();
        // Return true if Javascript debugging is enabled.

    UINT32 WatchdogTimeout();
        // Number of seconds to wait before timeout (0 if disabled).

    bool SuppressLogFile();
        // suppress log file output entirely.

    string TelnetClientIp();
        // When operating in telnet client mode, get the IP string.

    UINT32 TelnetClientPort();
        // When operating in telnet client mode, get the port number.

    bool TelnetServer();
        // Whether the client is operating in telnet server mode

    string TelnetCmdline();
        // Get the telnet command line if present

    bool AccessTokenAcquire();
        // Acquire a MODS global access token

    bool AccessTokenRelease();
        // Release a MODS global access token

    UINT32 AccessToken();
        // Get the MODS global access token

    bool NoNet();
        // Whether to skip network authentication

    bool SingleUser();
       // (MAC only) Single user mode of operation

    bool NlwrsesDisabled();
       // (MAC only) Allow Nlwrses MODS console on MAC

    string UIPipeName();
       // (Windows only) The pipe name of the user interface pipe if enabled

    bool InternalBypassPassed();
      // Returns true if bypass.INTERNAL.bin was passed
};

#endif // !INCLUDED_CMDLINE_H
