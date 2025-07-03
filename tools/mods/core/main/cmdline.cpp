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

#include "core/include/cmdline.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "core/include/filesink.h"
#include "core/include/sersink.h"
#include "core/include/circsink.h"
#include "core/include/debuglogsink.h"
#include "core/include/version.h"
#include "core/include/tee.h"
#include "core/include/xp.h"
#include "core/include/massert.h"
#include "core/include/utility.h"
#include "core/include/js_param.h"
#include "core/include/argpproc.h"
#include "core/include/gpu.h"
#include "core/include/errormap.h"
#include <cstring>
#include <set>
#include <memory>
#include "lwdiagutils.h"
#include "core/include/assertinfosink.h"
#include "core/include/fileholder.h"
#include "core/include/platform.h"

#include "jsstr.h"

#include "core/include/memcheck.h"

namespace
{
    #define APPEND_LOG_OPT            "-a"
    #define ASSERT_DBG_OPT            "-ald"
    #define ASSERT_FAIL_DIS_OPT       "-A"
    #define BYPASS_OPT                "-b"
    #define SERIAL_BAUD_OPT           "-B"
    #define CHIP_OPT                  "-chip"
    #define CIRLWLAR_OPT              "-C"
    #define DEBUG_OPT                 "-d"
    #define DEBUG_FILE_OPT            "-D"
    #define EXELWTE_SCRIPT_OPT        "-e"
    #define FILTER_SINK_OPT           "-F"
    #define NOLOG_RETCODE_OPT         "-g"
    #define HELP_H_OPT                "-h"
    #define HELP_Q_OPT                "-?"
    #define IMPORT_FILE_OPT           "-i"
    #define LOGFILE_NAME_OPT          "-l"
    #define LOG_ONLY_ERR_OPT          "-L"
    #define LOG_SPLIT_SIZE            "-log_split_size"
    #define MLE_OPT                   "-mle"
    #define MLE_DIS_OPT               "-mle_disabled"
    #define MLE_FORCE_TP_OPT          "-mle_force_tp"
    #define MLE_FORCE_OPT             "-mle_forced"
    #define MLE_NORM_VERB_OPT         "-mle_lw"
    #define MLE_VERB_OPT              "-mle_v"
    #define MLE_LIMITED_OPT           "-mle_limited"
    #define MLE_STDOUT_OPT            "-mle_stdout"
    #define PRE_MAIN_SCRIPT_OPT       "-m"
    #define POST_MAIN_SCRIPT_OPT      "-n"
    #define NO_MAIN_OPT               "-o"
    #define CIRC_DBG_OPT              "-P"
    #define QUIET_LOG_OPT             "-q"
    #define RECORD_INPUT_OPT          "-r"
    #define REDIRECT_STDOUT_OPT       "-redir"
    #define REMOTE_UI_OPT             "-R"
    #define SUPPRESS_LOG_OPT          "-suppress_log"
    #define SERIAL_SINK_LEV_OPT       "-S"
    #define SERIAL_SINK_PORT_OPT      "-SPort"
    #define TELNET_SERVER_OPT         "-T"
    #define TELNET_CLIENT_OPT         "-U"
    #define VERSION_OPT               "-v"
    #define VERBOSE_LOG_OPT           "-verbose_log"
    #define WATCHDOG_OPT              "-W"
    #define CIRC_LOG_SIZE_OPT         "-X"
    #define ENCRYPT_OPT               "-y"
    #define MAC_SU_OPT                "-1"
    #define TSTSTREAM_OPT             "-tstStream"
    #define BIOS_OPT                  "-bios"
    #define GPUBIOS_OPT               "-gpubios"
    #define NETBIN_OPT                "-netbin"
    #define SOC_FW_OPT                "-soc_firmware"
    #define JSD_OPT                   "-jsd"
    #define RUNTAG_OPT                "-runtag"
    #define SIGN_OPT                  "-sign"
    #define UI_TYPE_OPT               "-ui_type"
    #define ACQUIRE_ACCESS_TOK_OPT    "-acquire_access_token"
    #define RELEASE_ACCESS_TOK_OPT    "-release_access_token"
    #define ACCESS_TOK_OPT            "-access_token"
    #define NO_NET_OPT                "-no_net"
    #define NLWRSES_DISABLE_OPT       "-nc"
    #define PIPE_OPT                  "-p"
    #define HEARTBEAT_ENABLE_OPT      "-enable_heartbeat"
    #define GDM_IP_PORT               "-gdm"
}

static const ParamDecl s_ModsParams[] =
{
    SIMPLE_PARAM(APPEND_LOG_OPT,                                 "append to log file")                                                                                   //$
   ,SIMPLE_PARAM(ASSERT_DBG_OPT,                                 "set assert info level to debug")                                                                       //$
   ,SIMPLE_PARAM(ASSERT_FAIL_DIS_OPT,                            "disable FAIL on assert(default enabled in release builds)")                                            //$
   ,{ BYPASS_OPT, "t", ParamDecl::PARAM_MULTI_OK, 0, 0, "bypass file name" }                                                                                             //$
   ,UNSIGNED_PARAM(SERIAL_BAUD_OPT,                              "set the serial sink baud rate")                                                                        //$
#ifdef SIM_BUILD
   ,STRING_PARAM(CHIP_OPT,                                       "load simulator chip library from <file>")                                                              //$
#endif
   ,SIMPLE_PARAM(CIRLWLAR_OPT,                                   "enable cirlwlar buffer, set to 'debug' level & dump on exit")                                          //$
   ,SIMPLE_PARAM(DEBUG_OPT,                                      "set 'debug' level output")                                                                             //$
   ,SIMPLE_PARAM(DEBUG_FILE_OPT,                                 "enable debug logging to seperate log file")                                                            //$
   ,SIMPLE_PARAM(VERBOSE_LOG_OPT,                                "set 'verbose' level output for log file")                                                              //$
   ,{ EXELWTE_SCRIPT_OPT, "t", ParamDecl::PARAM_MULTI_OK, 0, 0,  "execute the specified script" }                                                                        //$
   ,STRING_PARAM(FILTER_SINK_OPT,                                "set a filter for serial and cirlwlar sinks")                                                           //$
   ,SIMPLE_PARAM(NOLOG_RETCODE_OPT,                              "do not log return codes")                                                                              //$
   ,SIMPLE_PARAM(HELP_H_OPT,                                     "print help")                                                                                           //$
   ,SIMPLE_PARAM(HELP_Q_OPT,                                     "print help")                                                                                           //$
   ,{ IMPORT_FILE_OPT, "t", ParamDecl::PARAM_MULTI_OK, 0, 0,     "import file" }                                                                                         //$
   ,STRING_PARAM(LOGFILE_NAME_OPT,                               "log file name; also applies to mle, aside extension")                                                  //$
   ,SIMPLE_PARAM(LOG_ONLY_ERR_OPT,                               "only write the log file if there is an error")                                                         //$
   ,UNSIGNED_PARAM(LOG_SPLIT_SIZE,                               "split log file into multiple up to specified size in MB")                                              //$
   ,STRING_PARAM(MLE_OPT,                                        "mle log filename")                                                                                     //$
   ,SIMPLE_PARAM(MLE_DIS_OPT,                                    "disable logging to mle")                                                                               //$
   ,SIMPLE_PARAM(HEARTBEAT_ENABLE_OPT,                           "enable heart beat monitor")                                                                               //$
   ,SIMPLE_PARAM(MLE_FORCE_TP_OPT,                               "force test progress to be enabled (normally disabled during interactive mods)")                        //$
   ,SIMPLE_PARAM(MLE_FORCE_OPT,                                  "force logging to mle in builds where mle is not enabled by default")                                   //$
   ,SIMPLE_PARAM(MLE_NORM_VERB_OPT,                              "have mle log with normal verbosity without affecting .log file verbosity")                             //$
   ,SIMPLE_PARAM(MLE_VERB_OPT,                                   "have mle log with '-verbose' without affecting .log file verbosity")                                   //$
   ,SIMPLE_PARAM(MLE_STDOUT_OPT,                                 "have mle dumped to stdout")                                                                            //$
   ,SIMPLE_PARAM(MLE_LIMITED_OPT,                                "log only non-sensitive data to the mle log")                                                           //$
   ,{ PRE_MAIN_SCRIPT_OPT, "t", ParamDecl::PARAM_MULTI_OK, 0, 0, "execute script before main()" }                                                                        //$
   ,{ POST_MAIN_SCRIPT_OPT, "t", ParamDecl::PARAM_MULTI_OK, 0, 0,"execute script after main()" }                                                                         //$
   ,SIMPLE_PARAM(NO_MAIN_OPT,                                    "do not run main()")                                                                                    //$
   ,SIMPLE_PARAM(CIRC_DBG_OPT,                                   "enable cirlwlar buffer, set to 'debug' level & dump at normal priority on exit")                       //$
   ,SIMPLE_PARAM(QUIET_LOG_OPT,                                  "set 'none' level output (disable logging)")                                                            //$
   ,SIMPLE_PARAM(RECORD_INPUT_OPT,                               "record user input")                                                                                    //$
   ,STRING_PARAM(REDIRECT_STDOUT_OPT,                            "redirect standard output to the specified file")                                                       //$
   ,SIMPLE_PARAM(REMOTE_UI_OPT,                                  "remote user interface (run over network)")                                                             //$
   ,SIMPLE_PARAM(SUPPRESS_LOG_OPT,                               "suppress the log file output entirely")                                                                //$
   ,{ SERIAL_SINK_LEV_OPT, "u", 0, Tee::LevNone, Tee::LevAlways, "set the serial sink output level" }                                                                    //$
   ,UNSIGNED_PARAM(SERIAL_SINK_PORT_OPT,                         "set serial sink port")                                                                                 //$
   ,SIMPLE_PARAM(TELNET_SERVER_OPT,                              "remote terminal user interface (telnet)")                                                              //$
   ,{ TELNET_CLIENT_OPT, "tu", 0, 0, 0,                          "remote terminal user interface (client mode), specify \"ip port\"" }                                   //$
   ,SIMPLE_PARAM(VERSION_OPT,                                    "print MODS version and exit")                                                                          //$
   ,UNSIGNED_PARAM(WATCHDOG_OPT,                                 "enable timeout after specified seconds if test hangs")                                                 //$
   ,UNSIGNED_PARAM(CIRC_LOG_SIZE_OPT,                            "enable cirlwlar buffer logging to file only with specified size in MB")                                //$
   ,SIMPLE_PARAM(ENCRYPT_OPT,                                    "encrypt files generated by MODS")                                                                      //$
   ,SIMPLE_PARAM(MAC_SU_OPT,                                     "enable single user mode for MacOSX")                                                                   //$
   ,{ TSTSTREAM_OPT, "t", 0, 0, 0,                               "" }                                                                                                    //$
   ,STRING_PARAM(BIOS_OPT,                                       "")                                                                                                     //$
   ,{ GPUBIOS_OPT, "ut", 0, 0, 0,                                "vbios filename to use for specified gpu device" }                                                      //$
   ,STRING_PARAM(NETBIN_OPT,                                     "")                                                                                                     //$
   ,STRING_PARAM(SOC_FW_OPT,                                     "soc firmware filename")                                                                                //$
   ,SIMPLE_PARAM(JSD_OPT,                                        "")                                                                                                     //$
   ,STRING_PARAM(RUNTAG_OPT,                                     "")                                                                                                     //$
   ,SIMPLE_PARAM(SIGN_OPT,                                       "")                                                                                                     //$
                                                                                                                                                                         //$
   ,SIMPLE_PARAM(ACQUIRE_ACCESS_TOK_OPT,                         "acquire an access token lock on MODS, print the token and exit")                                       //$
   ,SIMPLE_PARAM(RELEASE_ACCESS_TOK_OPT,                         "release the access token lock on MODS and exit")                                                       //$
   ,UNSIGNED_PARAM(ACCESS_TOK_OPT,                               "Use the specified access token for running MODS")                                                      //$
   ,SIMPLE_PARAM(NO_NET_OPT,                                     "Treat system as not being on the internal Lwpu network")                                             //$
   ,SIMPLE_PARAM(NLWRSES_DISABLE_OPT,                            "Disable use of nlwrses console (lwrrently MAC only)")                                                  //$
   ,STRING_PARAM(PIPE_OPT,                                       "Set piped user interface with the specified pipe name (lwrrently Windows only")                        //$
   ,STRING_PARAM(GDM_IP_PORT,                                    "IP address and Port for GDM seperated by colon <IP>:<PORT>")                                           //$

   ,{ UI_TYPE_OPT,  "u",  (ParamDecl::PARAM_ENFORCE_RANGE | ParamDecl::GROUP_START), CommandLine::UI_None, CommandLine::UI_Message, "set user interface type:" }         //$
   ,{ "-s",        reinterpret_cast<const char *>(CommandLine::UI_Script),  ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED,  0, 0, "script user interface" } //$
   ,{ "-t",        reinterpret_cast<const char *>(CommandLine::UI_Macro),   ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED,  0, 0, "macro user interface" }  //$
   ,{ "-w",        reinterpret_cast<const char *>(CommandLine::UI_Raw),     ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED,  0, 0, "raw user interface" }    //$
   ,LAST_PARAM
};
static const ParamConstraints s_ModsParamConstraints[] =
{
    // Verbosity flags are incompatible
    MUTUAL_EXCLUSIVE_PARAM(DEBUG_OPT, QUIET_LOG_OPT)
   ,MUTUAL_EXCLUSIVE_PARAM(DEBUG_OPT, MLE_NORM_VERB_OPT)
   ,MUTUAL_EXCLUSIVE_PARAM(DEBUG_OPT, MLE_VERB_OPT)
   ,MUTUAL_EXCLUSIVE_PARAM(DEBUG_OPT, VERBOSE_LOG_OPT)
   ,MUTUAL_EXCLUSIVE_PARAM(VERBOSE_LOG_OPT, QUIET_LOG_OPT)
   ,MUTUAL_EXCLUSIVE_PARAM(VERBOSE_LOG_OPT, MLE_NORM_VERB_OPT)
   ,MUTUAL_EXCLUSIVE_PARAM(VERBOSE_LOG_OPT, MLE_VERB_OPT)
   ,MUTUAL_EXCLUSIVE_PARAM(QUIET_LOG_OPT, MLE_NORM_VERB_OPT)
   ,MUTUAL_EXCLUSIVE_PARAM(QUIET_LOG_OPT, MLE_VERB_OPT)
   ,MUTUAL_EXCLUSIVE_PARAM(MLE_NORM_VERB_OPT, MLE_VERB_OPT)

    // Cirlwlar flags are incompatible
   ,MUTUAL_EXCLUSIVE_PARAM(CIRLWLAR_OPT, CIRC_DBG_OPT)
   ,MUTUAL_EXCLUSIVE_PARAM(CIRLWLAR_OPT, CIRC_LOG_SIZE_OPT)
   ,MUTUAL_EXCLUSIVE_PARAM(CIRC_DBG_OPT, CIRC_LOG_SIZE_OPT)

    // Client and server mode must be mutually exclusive
   ,MUTUAL_EXCLUSIVE_PARAM(TELNET_SERVER_OPT, TELNET_CLIENT_OPT)

   // Enabling/disabling mle at the same time is incompatible
   ,MUTUAL_EXCLUSIVE_PARAM(MLE_DIS_OPT, MLE_FORCE_OPT)

    // Acquire and release cannot both be specified
   ,MUTUAL_EXCLUSIVE_PARAM(ACQUIRE_ACCESS_TOK_OPT, RELEASE_ACCESS_TOK_OPT)

    // Acquire and supplying an access token cannot both be specified
   ,MUTUAL_EXCLUSIVE_PARAM(ACQUIRE_ACCESS_TOK_OPT, ACCESS_TOK_OPT)

   // MLE stdout and quiet logging cannot be both used
   ,MUTUAL_EXCLUSIVE_PARAM(MLE_STDOUT_OPT, QUIET_LOG_OPT)

   ,LAST_CONSTRAINT
};

// The database and reader must persist through shutdown and needs to be
// available very early (in OneTimeInit) even if it hasnt been parsed.  Because
// of this it must be a direct pointer type rather than a unique_ptr.
// OneTimeInit is responsible for creating/destroying
static ArgDatabase *s_ModsArgDatabase;
static ArgReader   *s_ModsArgReader;

//------------------------------------------------------------------------------
// Private CommandLine context.
//
static string                 s_ProgramPath("");
static string                 s_ScriptFilePath("");
static vector<string>         s_Argv;
static ArgDatabase            s_ArgDatabase;
static unique_ptr<ArgReader>  s_pJsArgReader;
static JsParamList            s_JsParams;
static vector<string>         s_ImportFileNames;

// Don't have Mle enabled by default when using the old tasker since MODS runtime
// is affected (there is no batched-printing happening in backbround threads).
// Ex. SIM builds don't use the new tasker
//
// Don't enable MLE by default on CheetAh, large MLE files lead to issues such as
// running out of space.

static vector<string>       s_InstantScripts;
static vector<string>       s_PreScripts;
static vector<string>       s_PostScripts;
static map<UINT32, string>  s_GpuBiosFileNames;

static bool s_InternalBypassPassed = false;

// MODS command line options that can be overridden once JS starts
enum class JsFailOnAssert
{
    DEFAULT
   ,FAIL
   ,NO_FAIL
};
static JsFailOnAssert       s_JsFailOnAssert(JsFailOnAssert::DEFAULT);
static CommandLine::UI      s_JsUserInterface(CommandLine::UI_Count);

namespace
{
    Utility::SelwrityUnlockLevel GetBypassUnlockLevel(const string &fileName)
    {
        string fullFilePath = fileName;
        if (!Xp::DoesFileExist(fullFilePath))
        {
            // Search for the file.
            vector<string> paths;
            paths.push_back(CommandLine::ProgramPath());
            string path = Utility::FindFile(fullFilePath, paths);
            if ("" == path)
                return Utility::SUL_NONE;
            fullFilePath = path + fullFilePath;
        }
        FileHolder bypassFile;
        if (OK != bypassFile.Open(fullFilePath, "rb"))
            return Utility::SUL_NONE;

        long fileSize = 0;
        if (OK != Utility::FileSize(bypassFile.GetFile(), &fileSize))
            return Utility::SUL_NONE;

        const long BYPASS_FILE_SIZE = 16;
        if (fileSize != BYPASS_FILE_SIZE)
            return Utility::SUL_NONE;

        UINT32 bypassHash;
        UINT32 internalBypassHash;
        UINT64 bypassExpiration;

        if (!Utility::FRead(&bypassHash, 4, 1, bypassFile.GetFile()) ||
            !Utility::FRead(&internalBypassHash, 4, 1, bypassFile.GetFile()) ||
            !Utility::FRead(&bypassExpiration, 8, 1, bypassFile.GetFile()))
        {
            return Utility::SUL_NONE;
        }
        bypassHash           = ~bypassHash;
        internalBypassHash   = ~internalBypassHash;
        bypassExpiration     = ~bypassExpiration;

        if (bypassHash == g_BypassHash)
        {
            if (internalBypassHash == g_InternalBypassHash)
            {
                s_InternalBypassPassed = true;
                return Utility::SUL_LWIDIA_NETWORK;
            }
            else if (Platform::GetTimeMS() < bypassExpiration)
                return Utility::SUL_BYPASS_UNEXPIRED;
            return Utility::SUL_BYPASS_EXPIRED;
        }
        return Utility::SUL_NONE;
    }

    Utility::SelwrityUnlockLevel GetBypassUnlockLevel(const vector<string> *pBypassFiles)
    {
        Utility::SelwrityUnlockLevel bypassFileLevel = Utility::SUL_NONE;
        for (auto lwrFile : *pBypassFiles)
        {
            Utility::SelwrityUnlockLevel lwrLev = GetBypassUnlockLevel(lwrFile);

            // If bypass.INTERNAL.bin wasnt passed to force the lwpu network then simply
            // use the max the level as the file level
            if ((bypassFileLevel < Utility::SUL_LWIDIA_NETWORK) &&
                (lwrLev < Utility::SUL_LWIDIA_NETWORK))
            {
                bypassFileLevel = max(bypassFileLevel, lwrLev);
            }
            else
            {
                // Otherwise, bypass.INTERNAL.bin was previously passed in order to force internal
                // features.  If bypass.bin was also passed this will activate the same
                // functionalities that it activates externally
                Utility::SelwrityUnlockLevel minLev = min(bypassFileLevel, lwrLev);
                if (minLev == Utility::SUL_BYPASS_UNEXPIRED)
                {
                    // This is the highest possible security level, simply return it
                    return Utility::SUL_LWIDIA_NETWORK_BYPASS;
                }
                else
                {
                    // Either the current or previously detected bypass level must be
                    // SUL_LWIDIA_NETWORK, since it didnt get bumped up by the check
                    // above, simply set the current bypass file level to lwpu network
                    bypassFileLevel = Utility::SUL_LWIDIA_NETWORK;
                }
            }
        }
        return bypassFileLevel;
    }
}

bool CommandLine::InternalBypassPassed()
{
    return s_InternalBypassPassed;
}

//------------------------------------------------------------------------------
void CommandLine::Initialize()
{
    s_ModsArgReader = new ArgReader(s_ModsParams, s_ModsParamConstraints);
    s_ModsArgDatabase = new ArgDatabase;
    MASSERT(s_ModsArgReader && s_ModsArgDatabase);
}

//------------------------------------------------------------------------------
void CommandLine::Shutdown()
{
    if (s_ModsArgReader)
        delete s_ModsArgReader;
    if (s_ModsArgDatabase)
        delete s_ModsArgDatabase;
}

//------------------------------------------------------------------------------
// CommandLine
//
RC CommandLine::Parse(const vector<string> &args, const vector<CmdExtender*> &extenders)
{
    RC rc;

    size_t argc = args.size();

    string::size_type pos;

    // Set the program path.  Fix up any backslashes.
    // Include the last path separator.
    if (argc > 0)
    {
        string Program(args[0]);
        Printf(Tee::PriDebug, "args[0] is %s\n", args[0].c_str());
        for (size_t i = 0; i < Program.length(); i++)
        {
            if (Program[i] == '\\')
            {
                Program[i] = '/';
            }
        }
        if (string::npos != (pos = Program.rfind('/')))
        {
            s_ProgramPath = Program.substr(0, pos + 1);
        }
        else
        {
            s_ProgramPath = "./";
        }
        Printf(Tee::PriLow, "program path: %s\n", s_ProgramPath.c_str());
        LwDiagUtils::AddExtendedSearchPath(s_ProgramPath);
    }

#ifndef BOUND_JS
    // If the user didn't specify any arguments at all, they probably need the help
    // message.
    if (argc < 2)
    {
        ArgReader::DescribeParams(s_ModsParams, false);
        return RC::EXIT_OK;
    }
#endif

    // Expand any arguments of the form "@argfile"
    vector<string> argArray = args;
#if !defined(BOUND_JS)
    static const int MAX_EXPANDED_FILES = 1000;
    int numExpandedFiles = 0;
    for (auto pArg = argArray.begin(); pArg != argArray.end();)
    {
        if (pArg->size() > 0 && pArg->front() == '@')
        {
            if (++numExpandedFiles > MAX_EXPANDED_FILES)
            {
                Printf(Tee::PriError,
                       "Expanded %d files in command line; possible relwsion in %s\n",
                       numExpandedFiles, pArg->c_str());
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
            const string &filename = pArg->substr(1);
            vector<char> fileData;
            bool fileEncrypted;
            CHECK_RC(Utility::ReadPossiblyEncryptedFile(filename, &fileData,
                                                        &fileEncrypted));
            if (fileEncrypted && argArray.size() != 2)
            {
                Printf(Tee::PriAlways,
                       "Response file '@%s' must be the only argument\n",
                       filename.c_str());
                return RC::FILE_READ_ERROR;
            }

            string fileString(&fileData[0], fileData.size());
            string delimiters = " \t\n\r";
            vector<string> tokenArgs;
            rc = Utility::Tokenizer(fileString, delimiters, &tokenArgs);
            if (rc != OK)
            {
                Printf(Tee::PriAlways, "Error processing response file '%s'\n",
                       filename.c_str());
                return RC::FILE_READ_ERROR;
            }
            pArg = argArray.erase(pArg);
            pArg = argArray.insert(pArg, tokenArgs.begin(), tokenArgs.end());
        }
        else
        {
            ++pArg;
        }
    }
#endif

    // Handle special case arguments.
    vector<string>  modsArgs;

    for (size_t arg = 1; arg < argArray.size(); arg++)
    {
        bool optionWasProcessedByExtender = false;
        size_t oldArg = arg;
        for (size_t ex = 0; extenders.size() > ex; ++ex)
        {
            if (0 != extenders[ex])
            {
                if (extenders[ex]->ProcessOption(argArray, &arg, &modsArgs))
                {
                    optionWasProcessedByExtender = true;
                    break; // do not allow to process one option by several
                           // extenders
                }
            }
        }
        if (optionWasProcessedByExtender)
        {
            // erase what was processed by an extender
            argArray.erase(argArray.begin() + oldArg, argArray.begin() + arg + 1);
            // we have a new element at the current position now, process it again
            --arg;
        }
    }

    size_t lwrArg = 0;
#if defined(BOUND_JS)
    // Store the arguments in s_Argv for use by the bound.js script's main().
    s_Argv = argArray;
    argArray = modsArgs;
#else
    // Check for non-bound stealth mode (useful during stealth mode development)
    // and handle arguments the same way as if it was stealth mode (i.e. all
    // arguments must be passed after the JS file and the arguments must be
    // appropriate for the stealth mode JS file)
    MASSERT(!argArray.empty());
    if (g_FieldDiagMode != FieldDiagMode::FDM_NONE)
    {
        s_Argv.assign(argArray.begin() + 1, argArray.end());
        argArray = modsArgs;
    }
    else
    {
        argArray.insert(argArray.begin() + 1, modsArgs.begin(), modsArgs.end());

        // argArray has the exelwtable at the start so start with the first arg
        lwrArg = 1;
    }
#endif

    // The command line arguments are single letters prefixed by a "-".
    // Arguments may have optional input.
    for (; lwrArg < argArray.size(); lwrArg++)
    {
        CHECK_RC(s_ModsArgDatabase->AddSingleArg(argArray[lwrArg].c_str()));
    }

    s_ModsArgReader->SetStopOnUnknownArg(true);
    if (!s_ModsArgReader->ParseArgs(s_ModsArgDatabase))
    {
        Printf(Tee::PriAlways, "Failed to process mods arguments\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    if (g_FieldDiagMode == FieldDiagMode::FDM_NONE)
    {
        Printf(Tee::PriNormal, "MODS arguments : ");
        s_ModsArgReader->PrintParsedArgs(Tee::PriNormal);
        Printf(Tee::PriNormal, "\n");
    }

    string firstUnknownArg = s_ModsArgReader->GetFirstUnknownArg();
    if (!firstUnknownArg.empty())
    {
        if (firstUnknownArg[0] == '-')
        {
            Printf(Tee::PriAlways,
                "Bad argument \"%s\".\n"
                "If your argument is a JS argument, please place it after "
                "the JS file. \n Example: 'mods -s mods.js -pstate 12'\n", firstUnknownArg.c_str());
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        s_ScriptFilePath = LwDiagUtils::StripFilename(firstUnknownArg.c_str());
        if (s_ScriptFilePath.empty())
        {
            s_ScriptFilePath = "./";
        }

        Printf(Tee::PriLow, "script path: %s\n",
                s_ScriptFilePath.c_str());

        LwDiagUtils::AddExtendedSearchPath(s_ScriptFilePath);

        auto pArgvScript = find(argArray.begin(), argArray.end(), firstUnknownArg);

        // Store the script and all its arguments in s_Argv.
        s_Argv.assign(pArgvScript, argArray.end());
    }

    // Redact argument strings for JS script args only
    //
    // Redaction can only happen after we parse all the args
    // (since the MODS arg -no_net affects redaction)
    for (size_t i = 0; i < s_Argv.size(); i++)
    {
        s_Argv[i] = Utility::RedactString(s_Argv[i]);
    }

    return rc;
}

//------------------------------------------------------------------------------
// CommandLine
//
RC CommandLine::Handle()
{
    // Handle command line options in the order that makes the most sense

    // Redirecting stdout to a file should happen as soon as possible
    string stdoutFile = s_ModsArgReader->ParamStr(REDIRECT_STDOUT_OPT, "");
    if (!stdoutFile.empty() && (0 == freopen(stdoutFile.c_str(), "wt", stdout)))
    {
        Printf(Tee::PriWarn, "Unable to redirect standard output to %s!\n", stdoutFile.c_str());
    }

    // If just requesing help, we will be exiting immediately so go ahead and handle that
    // before handling any other command line options
    if (s_ModsArgReader->ParamPresent(HELP_H_OPT) > 0 ||
        s_ModsArgReader->ParamPresent(HELP_Q_OPT) > 0)
    {
        ArgReader::DescribeParams(s_ModsParams, false);
        return RC::EXIT_OK;
    }

    // Next Ensure that the security level is set early since some other arguments
    // depend up it
    Utility::SelwrityUnlockLevel bypassUnlockLevel = Utility::SUL_NONE;
    if (g_FieldDiagMode != FieldDiagMode::FDM_NONE &&
        g_FieldDiagMode != FieldDiagMode::FDM_629)
    {
        bypassUnlockLevel = Utility::SUL_BYPASS_UNEXPIRED;
    }

    const vector<string>* pBypassFiles = s_ModsArgReader->ParamStrList(BYPASS_OPT);
    if (pBypassFiles != nullptr)
    {
        Utility::SelwrityUnlockLevel readBypassLevel = GetBypassUnlockLevel(pBypassFiles);
        if (readBypassLevel > bypassUnlockLevel)
            bypassUnlockLevel = readBypassLevel;
    }

    // Set the screen sink level early so that the "-d" argument can show informational
    // messages that occur during command line processing.  The file sink has not been
    // setup at this point so no need to set that here
    Tee::SetScreenLevel(CommandLine::ScreenLevel());

    Utility::InitSelwrityUnlockLevel(bypassUnlockLevel);

    // ---------------------------------------------------------------------------------
    // HANDLE ARGUMENT ERRORS
    if ((Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK) &&
        (s_ModsArgReader->ParamPresent(JSD_OPT) > 0))
    {
        Printf(Tee::PriWarn, "Argument -jsd not supported\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    // Single user mode for MacOSX (non-Mfg). This flag should
    // also be caught and acted on in the appropriate
    // Xp::OnEntry() function.
    if (s_ModsArgReader->ParamPresent(MAC_SU_OPT) > 0 &&
        (Xp::GetOperatingSystem() != Xp::OS_MACRM))
    {
        Printf(Tee::PriAlways,  "Bad argument \"-1\".\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    // HP CDM (Common Diagnostics Module) slave mode.
    // Winmods only for now (linux someday).
    // This is the RAW user interface with input and output
    // directed to a win32 named pipe instead of a socket.
    // win32/xp.cpp also parses this command-line option, and
    // will grab the named pipe filename there.
    if (!UIPipeName().empty() && (Xp::GetOperatingSystem() != Xp::OS_WINDOWS))
    {
        Printf(Tee::PriAlways, "Bad argument \"-p\".\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    if (CommandLine::AccessTokenRelease() && (s_ModsArgReader->ParamPresent(ACCESS_TOK_OPT) == 0))
    {
        Printf(Tee::PriError, RELEASE_ACCESS_TOK_OPT " must also specify an access token\n");
        return RC::BAD_PARAMETER;
    }
    // ---------------------------------------------------------------------------------

    // ---------------------------------------------------------------------------------
    // LOGGING "SINK" ARGUMENTS
    //
    if (GetEncryptFiles())
    {
        FileSink* const pFileSink = Tee::GetFileSink();
        if (0 != pFileSink)
        {
            pFileSink->SetEncrypt(true);
        }
    }

    // Serial sink
    UINT32 Baud = s_ModsArgReader->ParamUnsigned(SERIAL_BAUD_OPT, 115200);
    if (s_ModsArgReader->ParamPresent(SERIAL_SINK_LEV_OPT) > 0)
    {
        if (s_ModsArgReader->ParamPresent(SERIAL_SINK_PORT_OPT) > 0)
            Tee::GetSerialSink()->SetPort(s_ModsArgReader->ParamUnsigned(SERIAL_SINK_PORT_OPT, 0));
        Tee::GetSerialSink()->Initialize();
        Tee::GetSerialSink()->SetBaud(Baud);
    }

    // Debug enablement
    if (s_ModsArgReader->ParamPresent(DEBUG_OPT) > 0)
    {
        Tee::SetAssertInfoLevel(Tee::LevNone);
        Tee::GetAssertInfoSink()->Uninitialize();
        Tee::EnDisAllModules(true);
    }

    // Cirlwlar sink
    // Mutual exclusion between C, X, and P is enforced at the parse level
    if ((s_ModsArgReader->ParamPresent(CIRLWLAR_OPT) > 0) ||
        (s_ModsArgReader->ParamPresent(CIRC_LOG_SIZE_OPT) > 0) ||
        (s_ModsArgReader->ParamPresent(CIRC_DBG_OPT) > 0) ||
        (s_ModsArgReader->ParamPresent(LOG_ONLY_ERR_OPT) > 0))
    {

        const UINT32 size = s_ModsArgReader->ParamUnsigned(CIRC_LOG_SIZE_OPT, 0);

        if (size != 0)
            Tee::GetCirlwlarSink()->SetCircBufferSize(size);
        const bool logOnlyOnError = s_ModsArgReader->ParamPresent(LOG_ONLY_ERR_OPT) > 0;
        Tee::GetCirlwlarSink()->SetExtraMessages(!logOnlyOnError);
        Tee::GetCirlwlarSink()->Initialize();
    }

    // Debug sink
    if (s_ModsArgReader->ParamPresent(DEBUG_FILE_OPT) > 0)
    {
        Tee::GetDebugLogSink()->SetFileName("debug.log");
        Tee::GetDebugLogSink()->Initialize();
    }

    // Set filter on all sinks
    if (s_ModsArgReader->ParamPresent(FILTER_SINK_OPT) > 0)
    {
        const char *pFilterStr = s_ModsArgReader->ParamStr(FILTER_SINK_OPT, "");
        Tee::GetCirlwlarSink()->SetFilterString(pFilterStr);
        Tee::GetDebugLogSink()->SetFilterString(pFilterStr);
        Tee::GetSerialSink()->SetFilterString(pFilterStr);
    }

    if (s_ModsArgReader->ParamPresent(LOG_ONLY_ERR_OPT) > 0 || (UserInterface() == UI_Message))
    {
        Tee::SetFileLevel(Tee::LevNone);
    }

    if (s_ModsArgReader->ParamPresent(SIGN_OPT))
        Tee::GetFileSink()->StartSigning();

    if (s_ModsArgReader->ParamPresent(ASSERT_DBG_OPT))
        Tee::SetAssertInfoLevel(Tee::LevDebug);

    // ---------------------------------------------------------------------------------
    // SCRIPT ARGUMENTS
    //
    UINT32 sCount = s_ModsArgReader->ParamPresent(EXELWTE_SCRIPT_OPT);
    for (UINT32 si = 0; si < sCount; si++)
    {
        s_InstantScripts.push_back(s_ModsArgReader->ParamNStr(EXELWTE_SCRIPT_OPT, si, 0));
    }

    sCount = s_ModsArgReader->ParamPresent(PRE_MAIN_SCRIPT_OPT);
    for (UINT32 si = 0; si < sCount; si++)
    {
        s_PreScripts.push_back(s_ModsArgReader->ParamNStr(PRE_MAIN_SCRIPT_OPT, si, 0));
    }

    sCount = s_ModsArgReader->ParamPresent(POST_MAIN_SCRIPT_OPT);
    for (UINT32 si = 0; si < sCount; si++)
    {
        s_PostScripts.push_back(s_ModsArgReader->ParamNStr(POST_MAIN_SCRIPT_OPT, si, 0));
    }

    const UINT32 ifCount = s_ModsArgReader->ParamPresent(IMPORT_FILE_OPT);
    for (UINT32 fi = 0; fi < ifCount; fi++)
    {
        s_ImportFileNames.push_back(s_ModsArgReader->ParamNStr(IMPORT_FILE_OPT, fi, 0));
    }

    // ---------------------------------------------------------------------------------
    // CHIP ARGUMENTS
    //
    if (s_ModsArgReader->ParamPresent(BIOS_OPT) > 0)
    {
        s_GpuBiosFileNames[0] = s_ModsArgReader->ParamStr(BIOS_OPT, "");
        Printf(Tee::PriAlways, "Warning:  The -bios option is deprecated."
                               "  Please use -gpubios instead.\n");
        Printf(Tee::PriAlways, "The -bios option will eventually be removed from mods.\n");
    }

    const UINT32 biosCount = s_ModsArgReader->ParamPresent(GPUBIOS_OPT);
    for (UINT32 bi = 0; bi < biosCount; bi++)
    {
        UINT32 deviceNum = s_ModsArgReader->ParamNUnsigned(GPUBIOS_OPT, bi, 0);
        s_GpuBiosFileNames[deviceNum] = s_ModsArgReader->ParamNStr(GPUBIOS_OPT, bi, 1);
    }

    const string socFw = s_ModsArgReader->ParamStr(SOC_FW_OPT, "");
    if (!socFw.empty())
    {
        if (s_GpuBiosFileNames.count(SOC_FIRMWARE))
        {
            Printf(Tee::PriNormal, "Firmware already provided, ignoring %s\n", socFw.c_str());
        }
        else
        {
            s_GpuBiosFileNames[SOC_FIRMWARE] = socFw;
        }
    }

    // ---------------------------------------------------------------------------------
    // MISCELLANEOUS ARGUMENTS
    //
    const string runtag = s_ModsArgReader->ParamStr(RUNTAG_OPT, "");
    Printf(Tee::PriAlways, "%s\n", runtag.c_str());

#ifndef BOUND_JS
    if (s_ModsArgReader->ParamPresent(VERSION_OPT) > 0)
    {
        Printf(Tee::PriAlways, "Version:      %s\n", g_Version);
        Printf(Tee::PriAlways, "Build:        %s\n", Utility::GetBuildType().c_str());
        if (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK)
        {
            string CLNumber = "unspecified";
            if (g_Changelist != 0)
            {
                CLNumber = Utility::StrPrintf("%d", g_Changelist);
            }
            Printf(Tee::PriAlways, "Changelist:   %s\n", CLNumber.c_str());
            Printf(Tee::PriAlways, "Build date:   %s\n", g_BuildDate);
            Printf(Tee::PriAlways, "Modules:      %s\n", Utility::GetBuiltinModules().c_str());
        }
        Xp::SetElw("MODS_VERSION", g_Version);
        return RC::EXIT_OK;
    }
#endif // BOUND_JS

    if (!MleLogEnabled())
    {
        ErrorMap::EnableTestProgress(false);
    }
    else
    {
        // If we're in interactive mods and have not set the 'force test progress'
        // arg, then disable the tracking of test progress in MLE
        if (!s_ModsArgReader->ParamPresent(MLE_FORCE_TP_OPT))
        {
            // Disable Test Progress in Interactive MODS, since it forces users to reset Progress
            // counters manually, which is a bad UX design. Also, it's unlikely that Interactive
            // MODS users need to keep track of Test Progress
            if (UserInterface() == UI_Script)
            {
                ErrorMap::EnableTestProgress(false);
            }
        }
    }

    return OK;
}

void CommandLine::Reset()
{
    s_ModsArgReader->Reset();
    s_ModsArgDatabase->ClearArgs();
}

RC CommandLine::Parse(const vector<string> &args, CmdExtender *extender)
{
    vector<CmdExtender*> v;
    if (0 != extender)
    {
        v.push_back(extender);
    }

    return Parse(args, v);
}

RC CommandLine::Parse(const vector<string> &args)
{
    return Parse(args, 0);
}

bool CommandLine::GetAppendToLogFile()
{
    return s_ModsArgReader->ParamPresent(APPEND_LOG_OPT) > 0;
}

bool CommandLine::RecordUserInput()
{
    return s_ModsArgReader->ParamPresent(RECORD_INPUT_OPT) > 0;
}

bool CommandLine::RunMain()
{
    return !s_ModsArgReader->ParamPresent(NO_MAIN_OPT);
}

bool CommandLine::LogReturnCodes()
{
    return !s_ModsArgReader->ParamPresent(NOLOG_RETCODE_OPT);
}

string CommandLine::ProgramPath()
{
    return s_ProgramPath;
}

string CommandLine::ScriptFilePath()
{
    return s_ScriptFilePath;
}

const vector<string> & CommandLine::Argv()
{
    return s_Argv;
}

void CommandLine::SetScriptArgv(const vector<string> &newArgv)
{
    Printf(Tee::PriLow, "Original script arguments overridden\n");

    s_Argv.clear();

    for (UINT32 i = 0; i < newArgv.size(); ++i)
    {
        s_Argv.push_back(newArgv[i]);
        Printf(Tee::PriDebug, "%s is now Argv[%d]\n", newArgv[i].c_str(), i);
    }
}

ArgDatabase * CommandLine::ArgDb()
{
    return &s_ArgDatabase;
}

ArgReader * CommandLine::GetJsArgReader()
{
    return s_pJsArgReader.get();
}

JsParamList * CommandLine::ParamList()
{
    return &s_JsParams;
}

Tee::Level CommandLine::ScreenLevel()
{
    if ((g_FieldDiagMode != FieldDiagMode::FDM_NONE) ||
        (s_ModsArgReader->ParamPresent(QUIET_LOG_OPT) > 0))
    {
        return Tee::LevNone;
    }
    return s_ModsArgReader->ParamPresent(DEBUG_OPT) > 0 ? Tee::LevDebug : Tee::LevNormal;
}

Tee::Level CommandLine::FileLevel()
{
    if (s_ModsArgReader->ParamPresent(QUIET_LOG_OPT) > 0 && !GetEncryptFiles())
        return Tee::LevNone;
    return s_ModsArgReader->ParamPresent(DEBUG_OPT) > 0       ? Tee::LevDebug :
           s_ModsArgReader->ParamPresent(VERBOSE_LOG_OPT) > 0 ? Tee::LevLow
                                                              : Tee::LevNormal;
}

Tee::Level CommandLine::MleLevel()
{
    if (!MleLogEnabled() ||
        (s_ModsArgReader->ParamPresent(QUIET_LOG_OPT) > 0 && !GetEncryptFiles()))
        return Tee::LevNone;
    else if ((s_ModsArgReader->ParamPresent(DEBUG_OPT) > 0) ||
             (s_ModsArgReader->ParamPresent(DEBUG_FILE_OPT) > 0))
        return Tee::LevDebug;
    else if (s_ModsArgReader->ParamPresent(MLE_NORM_VERB_OPT) > 0)
        return Tee::LevNormal;
    else if (s_ModsArgReader->ParamPresent(MLE_VERB_OPT) > 0 ||
             s_ModsArgReader->ParamPresent(VERBOSE_LOG_OPT) > 0)
        return Tee::LevLow;
#ifdef DEBUG
    return Tee::LevLow;
#else
    return Tee::LevNormal;
#endif
}

bool CommandLine::IsMleLimited()
{
    return s_ModsArgReader->ParamPresent(MLE_LIMITED_OPT) > 0;
}

Tee::Level CommandLine::SerialLevel()
{
    if (g_FieldDiagMode != FieldDiagMode::FDM_NONE)
        return Tee::LevNone;
    return static_cast<Tee::Level>(s_ModsArgReader->ParamUnsigned(SERIAL_SINK_LEV_OPT, Tee::LevNormal));
}

Tee::Level CommandLine::CirlwlarLevel()
{
    return (s_ModsArgReader->ParamPresent(CIRLWLAR_OPT) ||
            s_ModsArgReader->ParamPresent(DEBUG_FILE_OPT) ||
            s_ModsArgReader->ParamPresent(CIRC_LOG_SIZE_OPT) ||
            s_ModsArgReader->ParamPresent(CIRC_DBG_OPT)) ? Tee::LevDebug : Tee::LevNormal;
}

Tee::Level CommandLine::DebuggerLevel()
{
    if ((g_FieldDiagMode != FieldDiagMode::FDM_NONE) ||
        s_ModsArgReader->ParamPresent(QUIET_LOG_OPT) > 0)
    {
        return Tee::LevNone;
    }
    return s_ModsArgReader->ParamPresent(DEBUG_OPT) > 0 ? Tee::LevDebug : Tee::LevNormal;
}

Tee::Level CommandLine::EthernetLevel()
{
    if (g_FieldDiagMode != FieldDiagMode::FDM_NONE)
        return Tee::LevNone;
    return s_ModsArgReader->ParamPresent(DEBUG_OPT) > 0 ? Tee::LevDebug : Tee::LevNormal;
}

bool CommandLine::FailOnAssert()
{
    if (s_JsFailOnAssert == JsFailOnAssert::DEFAULT)
        return !s_ModsArgReader->ParamPresent(ASSERT_FAIL_DIS_OPT);
    return (s_JsFailOnAssert == JsFailOnAssert::FAIL) ? true : false;
}

void CommandLine::SetJsFailOnAssert(bool State)
{
    s_JsFailOnAssert = State ? JsFailOnAssert::FAIL : JsFailOnAssert::NO_FAIL;
}

bool CommandLine::OnlyWriteFileOnFail()
{
    return s_ModsArgReader->ParamPresent(LOG_ONLY_ERR_OPT) > 0;
}

INT32 CommandLine::CirlwlarDumpOnExitLevel()
{
    if (s_ModsArgReader->ParamPresent(CIRC_DBG_OPT) > 0)
    {
        return Tee::LevNormal;
    }
    else if (s_ModsArgReader->ParamPresent(CIRLWLAR_OPT) > 0 ||
             s_ModsArgReader->ParamPresent(DEBUG_FILE_OPT) > 0 ||
             s_ModsArgReader->ParamPresent(CIRC_LOG_SIZE_OPT) > 0)
    {
        return Tee::FileOnly;
    }

    return Tee::LevNone;
}

const vector<string> & CommandLine::ImportFileNames()
{
    return s_ImportFileNames;
}

string CommandLine::LogFileName()
{
    string logFileName = s_ModsArgReader->ParamStr(LOGFILE_NAME_OPT, "");
    if (logFileName.empty())
    {
        if (g_FieldDiagMode != FieldDiagMode::FDM_NONE)
        {
            if (g_FieldDiagMode == FieldDiagMode::FDM_GRID)
                logFileName = "griddiag.log";
            else if (g_FieldDiagMode == FieldDiagMode::FDM_617)
                logFileName = "lwpu-diagnostic.log";
            else
                logFileName = "fieldiag.log";
        }
        else
        {
            logFileName = "mods.log";
        }
    }
    return logFileName;
}

UINT32 CommandLine::LogFileSplitSize()
{
    return s_ModsArgReader->ParamUnsigned(LOG_SPLIT_SIZE, 0);
}

string CommandLine::MleLogFileName()
{
    string mleLog = s_ModsArgReader->ParamStr(MLE_OPT, "");
    if (mleLog.empty() && !CommandLine::LogFileName().empty())
    {
        string logFile = CommandLine::LogFileName();
        mleLog = logFile.substr(0, logFile.find_last_of('.'));
        mleLog.append(".mle");
    }
    return mleLog;
}

bool CommandLine::MleLogEnabled()
{
#if defined(USE_NEW_TASKER) && !defined(TEGRA_MODS) && !defined(SIM_BUILD)
    bool bMleEnabled = true;
#else
    bool bMleEnabled = false;
#endif

    if (s_ModsArgReader->ParamPresent(MLE_DIS_OPT) > 0)
        bMleEnabled = false;

    if (s_ModsArgReader->ParamPresent(MLE_FORCE_OPT) > 0)
        bMleEnabled = true;

    const bool mleLimitedPresent = s_ModsArgReader->ParamPresent(MLE_LIMITED_OPT) > 0;
    if (mleLimitedPresent)
        bMleEnabled = true;

    // NOTE: Want MLE to be disabled in the field diag. The only exception is if it's only printing
    // non-sensitive information with mle_limited.
    const bool isFieldDiag = (g_FieldDiagMode != FieldDiagMode::FDM_NONE);
    if (isFieldDiag && !mleLimitedPresent)
    {
        bMleEnabled = false;
    }
    return bMleEnabled;
}

bool CommandLine::HeartBeatMonitorEnabled()
{
    return s_ModsArgReader->ParamPresent(HEARTBEAT_ENABLE_OPT) > 0;
}

bool CommandLine::MleStdOut()
{
    return s_ModsArgReader->ParamPresent(MLE_STDOUT_OPT) > 0;
}

const vector<string> & CommandLine::InstantScripts()
{
    return s_InstantScripts;
}

const vector<string> & CommandLine::PreScripts()
{
    return s_PreScripts;
}

const vector<string> & CommandLine::PostScripts()
{
    return s_PostScripts;
}

CommandLine::UI CommandLine::UserInterface()
{
    if (s_JsUserInterface != CommandLine::UI_Count)
        return s_JsUserInterface;
    if (s_ModsArgReader->ParamPresent(PIPE_OPT) > 0)
        return UI_Message;
    return static_cast<CommandLine::UI>(s_ModsArgReader->ParamUnsigned(UI_TYPE_OPT,
                                                                       CommandLine::UI_None));
}

bool CommandLine::RemoteUserInterface()
{
    return s_ModsArgReader->ParamPresent(REMOTE_UI_OPT) > 0;
}

CommandLine::UC CommandLine::UserConsole()
{
    return (s_ModsArgReader->ParamPresent(REMOTE_UI_OPT) > 0 ||
            s_ModsArgReader->ParamPresent(TELNET_SERVER_OPT) > 0 ||
            s_ModsArgReader->ParamPresent(TELNET_CLIENT_OPT) > 0 ||
            (UserInterface() == UI_Message)) ? UC_Remote : UC_Local;
}

void CommandLine::SetJsUserInterface(UI State)
{
    s_JsUserInterface = State;
}

const map<UINT32, string> & CommandLine::GpuBiosFileNames()
{
    return s_GpuBiosFileNames;
}

string CommandLine::GpuNetlistName()
{
    return s_ModsArgReader->ParamStr(NETBIN_OPT, "");
}

string CommandLine::GdmAddress()
{
    return s_ModsArgReader->ParamStr(GDM_IP_PORT, "");
}

string CommandLine::GpuChipLib()
{
    return s_ModsArgReader->ParamStr(CHIP_OPT, "");
}

string CommandLine::GpuRTLArgs()
{
    if (!s_ModsArgReader->ParamPresent(TSTSTREAM_OPT))
        return "";

    string gpuRtlArgs = TSTSTREAM_OPT;
    gpuRtlArgs += " ";
    gpuRtlArgs += s_ModsArgReader->ParamStr(TSTSTREAM_OPT, "");
    return gpuRtlArgs;
}

bool CommandLine::GetEncryptFiles()
{
    return ((g_FieldDiagMode != FieldDiagMode::FDM_NONE) ||
            s_ModsArgReader->ParamPresent(ENCRYPT_OPT) > 0);
}

bool CommandLine::DebugJavascript()
{
    return (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK) ?
        false : s_ModsArgReader->ParamPresent(JSD_OPT) > 0;
}

UINT32 CommandLine::WatchdogTimeout()
{
    return s_ModsArgReader->ParamUnsigned(WATCHDOG_OPT, 0) > 0;
}

bool CommandLine::SuppressLogFile()
{
    return s_ModsArgReader->ParamPresent(SUPPRESS_LOG_OPT) > 0;
}

string CommandLine::TelnetClientIp()
{
    if (!s_ModsArgReader->ParamPresent(TELNET_CLIENT_OPT))
        return "";
    return s_ModsArgReader->ParamNStr(TELNET_CLIENT_OPT, 0, 0);
}

UINT32 CommandLine::TelnetClientPort()
{
    if (!s_ModsArgReader->ParamPresent(TELNET_CLIENT_OPT))
        return 0;
    return s_ModsArgReader->ParamNUnsigned(TELNET_CLIENT_OPT, 0, 1);
}

bool CommandLine::TelnetServer()
{
    return s_ModsArgReader->ParamPresent(TELNET_SERVER_OPT) > 0;
}

string CommandLine::TelnetCmdline()
{
    string telnetCmdLine;
    // Mutual exclusion is enforced at parse time
    if (s_ModsArgReader->ParamPresent(TELNET_SERVER_OPT) > 0)
        telnetCmdLine = TELNET_SERVER_OPT;
    if (s_ModsArgReader->ParamPresent(TELNET_CLIENT_OPT) > 0)
    {
        telnetCmdLine = TELNET_CLIENT_OPT;
        telnetCmdLine += Utility::StrPrintf(" %s %u", TelnetClientIp().c_str(), TelnetClientPort());
    }
    return telnetCmdLine;
}

bool CommandLine::AccessTokenAcquire()
{
    return s_ModsArgReader->ParamPresent(ACQUIRE_ACCESS_TOK_OPT) > 0;
}

bool CommandLine::AccessTokenRelease()
{
    return s_ModsArgReader->ParamPresent(RELEASE_ACCESS_TOK_OPT) > 0;
}

UINT32 CommandLine::AccessToken()
{
    return s_ModsArgReader->ParamUnsigned(ACCESS_TOK_OPT, ~0U);
}

bool CommandLine::NoNet()
{
#if defined(BOUND_JS) || defined(TEGRA_MODS)
    // Never check network in end-user builds
    return true;
#else
    return s_ModsArgReader->ParamPresent(NO_NET_OPT) > 0;
#endif
}

bool CommandLine::SingleUser()
{
    return s_ModsArgReader->ParamPresent(MAC_SU_OPT) > 0;
}

bool CommandLine::NlwrsesDisabled()
{
    return s_ModsArgReader->ParamPresent(NLWRSES_DISABLE_OPT) > 0;
}

string CommandLine::UIPipeName()
{
    return s_ModsArgReader->ParamStr(PIPE_OPT, "");
}

//------------------------------------------------------------------------------
// Argv
//------------------------------------------------------------------------------

P_(Global_Get_Argv);
P_(Global_Set_Argv);
static SProperty Global_Argv
(
    "Argv",
    0,
    "",
    Global_Get_Argv,
    Global_Set_Argv,
    0,
    "Command line argument array."
);

P_(Global_Get_ArgDb);
static SProperty Global_ArgDb
(
    "ArgDb",
    0,
    "",
    Global_Get_ArgDb,
    0,
    0,
    "Command line argument database."
);

P_(Global_Get_ParamList);
static SProperty Global_ParamList
(
    "ParamList",
    0,
    "",
    Global_Get_ParamList,
    0,
    0,
    "Parameter list."
);

P_(Global_Get_UserInterface);
P_(Global_Set_UserInterface);
static SProperty Global_UserInterface
(
    "UserInterface",
    0,
    0,
    Global_Get_UserInterface,
    Global_Set_UserInterface,
    0,
    "User interface: none, script, or macro."
);

static SProperty Global_Version
(
    "Version",
    0,
    g_Version,
    0,
    0,
    JSPROP_READONLY,
    "MODS version."
);

static SProperty Global_UI_Script
(
    "UI_Script",
    0,
    CommandLine::UI_Script,
    0,
    0,
    JSPROP_READONLY,
    "Script user interface."
);

static SProperty Global_UI_Macro
(
    "UI_Macro",
    0,
    CommandLine::UI_Macro,
    0,
    0,
    JSPROP_READONLY,
    "Macro user interface."
);

P_(Global_Get_FailOnAssert);
P_(Global_Set_FailOnAssert);
static SProperty Global_FailOnAssert
(
    "FailOnAssert",
    0,
    0,
    Global_Get_FailOnAssert,
    Global_Set_FailOnAssert,
    0,
    "FailOnAssert: true or false."
);

C_(Global_ParseArgv);
static SMethod Global_ParseArgv
(
    "ParseArgv",
    C_Global_ParseArgv,
    0,
    "Parse the command line parameters"
);

P_(Global_Get_DebugJavascript);
static SProperty Global_DebugJavascript
(
    "DebugJavascript",
    0,
    "",
    Global_Get_DebugJavascript,
    0,
    0,
    "Debug Javascript."
);

C_(Global_SetDeviceVbios);
static STest Global_SetDeviceVbios
(
    "SetDeviceVbios",
    C_Global_SetDeviceVbios,
    2,
    "Set the vbios file for the specified device"
);

//
// Implementation
//
P_(Global_Get_Argv)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    *pValue = JSVAL_NULL;

    JavaScriptPtr pJavaScript;

    if (!JS_EnterLocalRootScope(pContext))
    {
        JS_ReportError(pContext, "Failed to get Argv.");
        return JS_FALSE;
    }

    DEFER
    {
        JS_LeaveLocalRootScope(pContext);
    };

    // Colwert the vector of string to a JS array.
    const vector<string> & Argv = CommandLine::Argv();
    JsArray                Vals(Argv.size());

    for (JsArray::size_type i = 0; i < Argv.size(); ++i)
    {
        if (OK != pJavaScript->ToJsval(Argv[i], &Vals[i]))
        {
            JS_ReportError(pContext, "Failed to get Argv.");
            return JS_FALSE;
        }
    }

    if (OK != pJavaScript->ToJsval(&Vals, pValue))
    {
        JS_ReportError(pContext, "Failed to get Argv.");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

P_(Global_Set_Argv)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JavaScriptPtr pJs;
    JsArray jsArgv;
    vector<string> cppArgv;
    JSString *tmpStr = nullptr;

    if (OK != pJs->FromJsval(*pValue, &jsArgv))
    {
        JS_ReportError(pContext, "Failed to get new Argv.");
        return JS_FALSE;
    }

    for (JsArray::size_type i = 0; i < jsArgv.size(); ++i)
    {
        if (OK != pJs->FromJsval(jsArgv[i], &tmpStr))
        {
            JS_ReportError(pContext, "Failed while colwerting from JS Argv");
            return JS_FALSE;
        }
        else
        {
            JSContext *cx = nullptr;
            pJs->GetContext(&cx);
            string argStr = DeflateJSString(cx, tmpStr);
            cppArgv.push_back(argStr);
        }
    }

    CommandLine::SetScriptArgv(cppArgv);

    return JS_TRUE;
}

P_(Global_Get_ArgDb)
{
    MASSERT(pObject  != 0);
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JavaScriptPtr pJs;

    if (CommandLine::ArgDb()->GetJSObject() == NULL)
    {
        if (OK != CommandLine::ArgDb()->CreateJSObject(pContext, pObject))
        {
            JS_ReportError(pContext,
                           "Unable to create JSObject for the ArgDatabase.");
            return JS_FALSE;
        }
    }

    if (OK != pJs->ToJsval(CommandLine::ArgDb()->GetJSObject(), pValue))
    {
        JS_ReportError(pContext, "Failed to get new ArgDb.");
        return JS_FALSE;
    }

    return JS_TRUE;
}

P_(Global_Get_ParamList)
{
    MASSERT(pObject  != 0);
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JavaScriptPtr pJs;

    if (CommandLine::ParamList()->GetJSObject() == NULL)
    {
        if (OK != CommandLine::ParamList()->CreateJSObject(pContext, pObject))
        {
            JS_ReportError(pContext,
                           "Unable to create JSObject for the Parameter List.");
            return JS_FALSE;
        }
    }

    if (OK != pJs->ToJsval(CommandLine::ParamList()->GetJSObject(), pValue))
    {
        JS_ReportError(pContext, "Failed to get new ParamList.");
        return JS_FALSE;
    }

    return JS_TRUE;
}

P_(Global_Get_UserInterface)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    if (OK != JavaScriptPtr()->ToJsval(CommandLine::UserInterface(), pValue))
    {
        JS_ReportError(pContext, "Failed to get UserInterface.");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

P_(Global_Set_UserInterface)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    // Colwert the argument.
    UINT32 State = false;
    if (OK != JavaScriptPtr()->FromJsval(*pValue, &State))
    {
        JS_ReportError(pContext, "Failed to set ScriptUserInterface.");
        return JS_FALSE;
    }

    CommandLine::SetJsUserInterface(static_cast<CommandLine::UI>(State));

    return JS_TRUE;
}

P_(Global_Get_FailOnAssert)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    if (OK != JavaScriptPtr()->ToJsval(CommandLine::FailOnAssert(), pValue))
    {
        JS_ReportError(pContext, "Failed to get FailOnAssert.");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

P_(Global_Set_FailOnAssert)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    // Colwert the argument.
    bool State = false;
    if (OK != JavaScriptPtr()->FromJsval(*pValue, &State))
    {
        JS_ReportError(pContext, "Failed to set FailOnAssert.");
        return JS_FALSE;
    }

    CommandLine::SetJsFailOnAssert(static_cast<bool>(State));

    return JS_TRUE;
}

//-----------------------------------------------------------------------------
//! \brief Call the DoneParsingArgs JS function when ParseArgv is complete
//!
//! \param pContext     : JS Context for the ParseArgv() call
//! \param pReturlwalue : Return value for ParseArgv()
//!
//! \return JS_TRUE if successful, JS_FALSE otherwise
static JSBool DoneParsingArgs(JSContext *    pContext,
                              jsval     *    pReturlwalue)
{
    JavaScriptPtr pJs;
    JsArray       jsArgs;
    UINT32        retValJs;
    jsval         jsRetVal;
    RC            rc;

    jsArgs.clear();

    C_CHECK_RC_MSG(pJs->CallMethod(pJs->GetGlobalObject(), "DoneParsingArgs",
                                   jsArgs, &jsRetVal),
                   "ParseArgv() : Unable to call DoneParsingArgs()");

    if (jsRetVal == JSVAL_NULL)
    {
        *pReturlwalue = JSVAL_NULL;
        return JS_TRUE;
    }

    C_CHECK_RC( pJs->FromJsval(jsRetVal, &retValJs));
    RETURN_RC(retValJs);
}

//-----------------------------------------------------------------------------
//! \brief Create the global ArgDatabase (using vector<string> within
//!        C_ParseArgv results in warnings)
//!
//! \param argvStart  : Starting index of actual arguments within s_Argv
//! \param delimIndex : Index of the first delimiting parameter in s_Argv
//!                     (beginning at argvStart)
//!
//! \return OK if database creation succeeded, not OK otherwise
static RC CreateArgDb(ArgPreProcessor *pPreProc,
                      UINT32           argvStart,
                      INT32            delimIndex)
{
    // Clear the database prior to starting command line processing
    s_ArgDatabase.ClearArgs();

    // If there are no arguments to add, simply return OK
    if (s_Argv.size() <= argvStart)
        return OK;

    RC rc;
    UINT32 i;
    vector<string>::iterator argDbStart = s_Argv.begin() + argvStart;
    vector<string>::iterator argDbEnd = s_Argv.end();

    // If a delimiter was present, get iterators to the first and last
    // elements that must be processed in this function
    if (delimIndex != -1)
    {
        argDbEnd = argDbStart;
        for (i = argvStart; i < (UINT32)delimIndex; i++)
            argDbEnd++;
    }

    // If there are no arguments to process we can end processing here
    if (argDbEnd == argDbStart)
        return OK;

    vector<string>  argDbArgv(argDbStart, argDbEnd);

    CHECK_RC(pPreProc->PreprocessArgs(&argDbArgv));

    CHECK_RC(pPreProc->AddArgsToDatabase(&s_ArgDatabase));

    return rc;
}

C_(Global_ParseArgv)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments > 0)
    {
        JS_ReportError(pContext, "Usage: ParseArgv()\n");
        return JS_FALSE;
    }

    JsParamDecl          *pLwrParam = NULL;
    ParamDecl *           pParamList = s_JsParams.GetParamDeclArray();
    ParamConstraints *    pParamConstraints;
    INT32                 delimIndex = -1;
    JsParamDecl          *pDelimParam = NULL;
    vector<JsParamDecl *> delimParams;
    const JsParamDecl    *pDevParam = NULL;
    UINT32                argvStart = 1;
    UINT32                i;
    UINT32                paramIndex;
    RC                    rc;

    // Keep track of the first delimiting parameter that is encountered
    // on the command line.  All parsing of command line arguments will
    // stop at this parameter and any arguments after the delimiting
    // parameter will be passed to a custom arg handling function
    s_JsParams.GetParameterType(JsParamDecl::DELIM, &delimParams);
    if (!delimParams.empty())
    {
        for (i = argvStart; i < s_Argv.size() && (delimIndex == -1); i++)
        {
            for (paramIndex = 0; paramIndex < delimParams.size(); paramIndex++)
            {
                if ((s_Argv[i] == delimParams[paramIndex]->GetName()) &&
                    (delimIndex == -1))
                {
                    delimIndex = i;
                    pDelimParam = delimParams[paramIndex];
                }
            }
        }
    }

    UINT32          lastDevScope = Gpu::MaxNumDevices;
    JsArray         jsArgs;
    jsval           jsArg;
    JavaScriptPtr   pJs;
    ArgPreProcessor argPreProc;

    C_CHECK_RC_MSG(CreateArgDb(&argPreProc, argvStart, delimIndex),
                   "ParseArgv() : Unable to create ArgDatabase");

    // Remember the last device scope that was specified with "-dev" on the
    // command line so that "-dev" can be handled since it can be both a scope
    // specifier and regular parameter (with restrictions)
    if (ArgPreProcessor::GetDevScopeShortlwt())
    {
        lastDevScope = argPreProc.LastDevScope();
        pDevParam = s_JsParams.GetParameter("-dev");
    }

    if (s_ArgDatabase.IsEmpty())
    {
        if (delimIndex != -1)
        {
            C_CHECK_RC_MSG(pDelimParam->HandleDelim(delimIndex),
                           "ParseArgv() : Unable to handle delimiting parameter");
        }

        // If the "-dev" argument is specified to be both a scoping argument
        // and a real argument, then it is never added to the database (since
        // its primary purpose is for scoping other arguments), however the
        // last oclwrence of "-dev" still needs to be handled as a normal
        // argument since the last oclwrence of the "-dev" argument sets the
        // "current" device (which is also the device under test in some cases)
        //
        // If "-dev" is the only argument present then the arg database will be
        // empty, but the "-dev" argument still needs to be handled here
        if ((lastDevScope < Gpu::MaxNumDevices) && pDevParam)
        {
            C_CHECK_RC_MSG(pDevParam->HandleFunc1Arg(
                                  Utility::StrPrintf("%d", lastDevScope)),
                           "ParseArgv() : Unable to handle last device parameter");
        }
        return DoneParsingArgs(pContext, pReturlwalue);
    }

    // Print preprocessed args (prints at debug level)
    argPreProc.PrintArgs();

    pParamConstraints = s_JsParams.GetParamConstraintsArray();

    s_pJsArgReader = make_unique<ArgReader>(pParamList, pParamConstraints);

    if (!s_pJsArgReader->ParseArgs(&s_ArgDatabase))
    {
        C_CHECK_RC_MSG(RC::BAD_COMMAND_LINE_ARGUMENT,
                       "ParseArgv() : Unable to parse global arguments");
    }

    if ((s_pJsArgReader->ParamDefined(HELP_Q_OPT) && s_pJsArgReader->ParamPresent(HELP_Q_OPT)) ||
        (s_pJsArgReader->ParamDefined(HELP_H_OPT) && s_pJsArgReader->ParamPresent(HELP_H_OPT)) ||
        (s_pJsArgReader->ParamDefined("-help")    && s_pJsArgReader->ParamPresent("-help")) ||
        (s_pJsArgReader->ParamDefined("--args")   && s_pJsArgReader->ParamPresent("--args")))
    {
        const char *helpType = s_pJsArgReader->ParamStr("--args", "JS");

        if (!strcmp(helpType, "JS") ||
            !strcmp(helpType, "mdiag") ||
            !strcmp(helpType, "all"))
        {
            s_JsParams.DescribeParams();
        }

        // If not just displaying JS arguments, call the overridable
        // PrintArgHelp function to print any additional arguments
        if (strcmp(helpType, "JS"))
        {
            C_CHECK_RC_MSG(pJs->ToJsval(helpType, &jsArg),
                     "ParseArgv() : Unable to colwert help string to jsval");
            jsArgs.push_back(jsArg);
            C_CHECK_RC_MSG(pJs->CallMethod(pJs->GetGlobalObject(),
                                           "PrintArgHelp", jsArgs),
                           "ParseArgv() : Unable to call PrintArgHelp()");
        }

        // returning NULL signals to caller to abort script
        *pReturlwalue = JSVAL_NULL;
        return JS_TRUE;
    }

    // Handle all global arguments
    pLwrParam = s_JsParams.GetFirstParameter();
    while (pLwrParam != NULL)
    {
        rc = pLwrParam->HandleArg(s_pJsArgReader.get(), Gpu::MaxNumDevices);
        if (rc != OK)
        {
            JS_ReportWarning(pContext,
                   "ParseArgv() : Unable to handle parameter \"%s\"\n",
                   pLwrParam->GetName().c_str());
            C_CHECK_RC(rc);
        }
        pLwrParam = s_JsParams.GetNextParameter(pLwrParam);
    }

    // Handle all arguments in the device scope
    for (UINT32 devInst = argPreProc.GetFirstDevScope();
         devInst != Gpu::MaxNumDevices;
         devInst = argPreProc.GetNextDevScope(devInst))
    {
        // Reset the arg reader so it can be parsed on a different device scope
        s_pJsArgReader->Reset();

        string devScope = Utility::StrPrintf("dev%d", devInst);

        if (!s_pJsArgReader->ParseArgs(&s_ArgDatabase, devScope.c_str(), false))
        {
            JS_ReportWarning(pContext,
                   "ParseArgv() : Unable to parse arguments for scope \"%s\"\n",
                   devScope.c_str());
            C_CHECK_RC(RC::BAD_COMMAND_LINE_ARGUMENT);
        }

        pLwrParam = s_JsParams.GetFirstParameter();
        while (pLwrParam != NULL)
        {
            rc = pLwrParam->HandleArg(s_pJsArgReader.get(), devInst);
            if (rc != OK)
            {
                JS_ReportWarning(pContext,
                       "ParseArgv() : Unable to handle parameter \"%s\" on scope \"%s\"\n",
                       pLwrParam->GetName().c_str(),
                       devScope.c_str());
                C_CHECK_RC(rc);
            }
            pLwrParam = s_JsParams.GetNextParameter(pLwrParam);
        }
    }

    if (delimIndex != -1)
    {
        C_CHECK_RC_MSG(pDelimParam->HandleDelim(delimIndex),
                       "ParseArgv() : Unable to handle delimiting parameter");
    }

    if ((lastDevScope < Gpu::MaxNumDevices) && pDevParam)
    {
        C_CHECK_RC_MSG(pDevParam->HandleFunc1Arg(
                              Utility::StrPrintf("%d", lastDevScope)),
                       "ParseArgv() : Unable to handle last device parameter");
    }

    s_pJsArgReader->Reset();
    return DoneParsingArgs(pContext, pReturlwalue);
}

P_(Global_Get_DebugJavascript)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    if (OK != JavaScriptPtr()->ToJsval(CommandLine::DebugJavascript(), pValue))
    {
        JS_ReportError(pContext, "Failed to get DebugJavascript.");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

C_(Global_SetDeviceVbios)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    UINT32 device;
    string vbiosFile;
    JavaScriptPtr pJavaScript;

    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &device) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &vbiosFile))))
    {
        JS_ReportError(pContext, "Usage: SetDeviceVbios(devInst, vbiosFile)\n");
        return JS_FALSE;
    }

    s_GpuBiosFileNames[device] = vbiosFile;

    RETURN_RC(OK);
}
