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

// Cross platform (XP) interface.

#include "core/include/xp.h"

#include "core/include/assertinfosink.h"
#include "core/include/cmdline.h"
#include "core/include/cnslmgr.h"
#include "core/include/console.h"
#include "core/include/cpu.h"
#include "core/include/dllhelper.h"
#include "core/include/errormap.h"
#include "core/include/evntthrd.h"
#include "core/include/fileholder.h"
#include "core/include/filesink.h"
#include "core/include/globals.h"
#include "core/include/jscript.h"
#include "core/include/massert.h"
#include "core/include/memerror.h"
#include "core/include/mle_protobuf.h"
#include "core/include/platform.h"
#include "core/include/pool.h"
#include "core/include/regexp.h"
#include "core/include/remotesink.h"
#include "core/include/script.h"
#include "core/include/sockansi.h"
#include "core/include/tasker.h"
#include "core/include/tee.h"
#include "core/include/telnutil.h"
#include "core/include/utility.h"
#include "core/utility/sharedsysmem.h"
#include "device/include/memtrk.h"
#include "lwdiagutils.h"
#include "rm.h"

#ifdef INCLUDE_GPU
#include "acpidsmguids.h"
#include "core/include/display.h"
#include "core/include/gpu.h"
#include "core/include/mgrmgr.h"
#include "core/include/lwrm.h"
#include "device/interface/pcie.h"
#include "gpu/gc6/ec_info.h"
#include "gpu/include/gpumgr.h"
#include "gpu/utility/commonjtmgr.h"
#include "jt.h"
#include "Lwcm.h"
#include "lwhybridacpi.h"
#include "lwos.h"
#include "lwRmApi.h"
#endif

#define WE_NEED_LINUX_XP
#include "xp_linux_internal.h"

#ifdef TEGRA_MODS
#include "cheetah/include/tegrasocdeviceptr.h"
#include "cheetah/include/tegrasocdevice.h"
#endif

#include <cstring> // memcpy
#include <ctype.h> // isdigit
#include <map>
#include <memory>
#include <poll.h>
#include <pwd.h>
#include <regex>
#include <regex.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(USE_PTHREADS)
#include <time.h>
#else
#include <sys/time.h>
#endif

#ifndef QNX
#include <sys/sysinfo.h>
#include <sys/klog.h>
#endif

extern "C"
{
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/mman.h>
#ifndef QNX
#include <asm/mman.h>
#endif
}

#ifdef LINUX_MFG
#define SUPPORT_BLINK_LEDS
#include <sys/ioctl.h>
#endif

#define SUPPORT_SIGNALS 1

#include "core/include/memcheck.h"

#include <atomic>

// Arrrg!! Why do they define this?!
#undef OS_LINUX

// Output stream for console-based I/O, plus other overhead
static bool                       s_bRemoteConsole = false;
static bool                       s_bHypervisor = false;
static SocketMods*                s_pBaseSocket = 0;
static unique_ptr<SocketMods>     s_pSocket;
static unique_ptr<RemoteDataSink> s_pRemoteSink;

static vector<SocketMods *>  * s_pOsSockets = 0;

static int s_ZeroFd = -1;
static int s_ModsElwVarsFd = -1;

static atomic<INT32> s_KernelLogPriority{Tee::PriLow};

static bool s_IsWatchdogOn =
#if defined(LINUX_MFG) && defined(LWCPU_FAMILY_ARM)
    true;
#else
    false;
#endif

#ifdef SUPPORT_BLINK_LEDS
static bool s_BlinkLightsThreadLive = false;
static int s_Keyboard = -1;
static Tasker::ThreadID s_BlinkLightsThreadId = Tasker::NULL_THREAD;
#endif

#define LINUX_PAGE_MASK (GetPageSize() - 1)

#ifdef SUPPORT_SIGNALS
static void OnModsCrash(int sig);
static void PrintStack();
typedef void (*sighandler_t)(int);
static sighandler_t s_signals[32];
static char s_ExeName[256];
#endif

//------------------------------------------------------------------------------
// Returns the path delimiter for extracting paths from elw variables
//
char Xp::GetElwPathDelimiter()
{
    return LwDiagXp::GetElwPathDelimiter();
}

namespace Xp
{
    RC InitZeroFd()
    {
        if (s_ZeroFd != -1)
        {
            return RC::OK;
        }
        s_ZeroFd = open("/dev/zero", O_RDWR);
        return (s_ZeroFd != -1) ? RC::OK : RC::ON_ENTRY_FAILED;
    }
}

namespace
{
    Tasker::ThreadID s_KlogThread          = Tasker::NULL_THREAD;
    int              s_Klog                = -1;
    int              s_KlogEventPipeReadEnd = -1;
    int              s_KlogEventPipe       = -1;
    regex_t          s_KlogRe;
    atomic<bool>     s_LogKernelMessages(true);

    RC ProcessKernelLog(int fd, void* cookie, void (*logFunc)(const char* str, int len, void* cookie))
    {
        static char buf[1024];
        static size_t bufSize = 0;

        for (;;)
        {
            // Leave room for trailing NUL
            const size_t freeLeft = sizeof(buf) - 1 - bufSize;

            const ssize_t numRead = read(fd, &buf[bufSize], freeLeft);
            if (numRead >= 0)
            {
                bufSize += numRead;
                MASSERT(bufSize < sizeof(buf) - 1);
                buf[bufSize] = 0;
            }

            if (!bufSize)
            {
                break;
            }

            regmatch_t match[3];
            while ((regexec(&s_KlogRe, buf, NUMELEMS(match), match, 0) == 0) &&
                   (match[0].rm_eo > 0) &&
                   (match[0].rm_eo <= static_cast<int>(bufSize)))
            {
                MASSERT(match[0].rm_so < match[0].rm_eo);
                MASSERT(match[1].rm_so == match[0].rm_so);
                MASSERT(match[1].rm_so < match[1].rm_eo);
                MASSERT(match[1].rm_eo < match[2].rm_so);
                MASSERT(match[2].rm_so <= match[2].rm_eo);
                MASSERT(match[2].rm_eo == match[0].rm_eo);
                MASSERT((match[0].rm_eo == static_cast<int>(bufSize)) || (buf[match[0].rm_eo] == '\n'));

                buf[match[1].rm_eo] = 0;
                const long kernelPri = atol(&buf[match[1].rm_so]);
                const int  len       = match[2].rm_eo - match[2].rm_so;

                // kernelPri is:
                // * bits 0..2 are priority, with 7 being debug
                // * bits 3+ are facility, with 0 being kernel
                // Print only non-debug kernel messages
                if ((kernelPri < 7) && len)
                {
                    logFunc(&buf[match[2].rm_so], len, cookie);
                }

                if (match[0].rm_eo >= static_cast<int>(bufSize) - 1)
                {
                    bufSize = 0;
                    break;
                }
                else
                {
                    const size_t skip = match[0].rm_eo + 1;
                    memmove(buf, &buf[skip], bufSize - skip + 1);
                    bufSize -= skip;
                }
            }

            if (numRead < 0)
            {
                // Ignore transient errors
                const int err = errno;
                if ((err == EINTR) || (err == EAGAIN))
                {
                    break;
                }

                Printf(Tee::PriWarn, "Failed to read from /dev/kmsg: %d %s\n", err, strerror(err));
                return RC::FILE_READ_ERROR;
            }
        }

        return RC::OK;
    }

    RC ReadKernelLog(string* pMessages)
    {
        return ProcessKernelLog(s_Klog, pMessages, [](const char* str, int len, void* cookie)
        {
            string* pMessages = static_cast<string*>(cookie);
            pMessages->append(str, len);
            pMessages->append(1, '\n');
        });
    }

    bool MakeNonBlocking(int fd)
    {
        const int flags = fcntl(fd, F_GETFL);
        return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
    }

    RC PrepareKernelLogger()
    {
#ifdef SIM_BUILD
        // In sim MODS we rarely run on Silicon on the system being tested.
        // In theory we could enable this when Platform::GetSimulationMode() == Platform::Hardware,
        // which is often with hwchip2.so on real Silicon, but not always.  However, there is relatively
        // little benefit from this, especially that chiplibs log stuff with printf() anyway, so they
        // don't log into mods.log.
        return RC::OK;
#endif

        // Kernels older than 3.5 don't support /dev/kmsg, so just print a message
        // and pretend everything is OK.
        const int fd = open("/dev/kmsg", O_RDONLY);
        if (fd == -1)
        {
            // Note: This is before mods.log is open, so this message will only make
            // it to stdout but it won't make it to mods.log.
            Printf(Tee::PriWarn, "Failed to open /dev/kmsg\n");
            return RC::OK;
        }

        bool closeOnReturn = true;
        DEFER
        {
            if (closeOnReturn)
            {
                close(fd);
            }
        };

        if (!MakeNonBlocking(fd))
        {
            Printf(Tee::PriWarn, "Failed to open /dev/kmsg in non-blocking mode\n");
            return RC::OK;
        }

        int pipes[2];
        if (pipe(pipes) != 0)
        {
            Printf(Tee::PriWarn, "Failed to create pipes\n");
            return RC::OK;
        }
        DEFER
        {
            if (closeOnReturn)
            {
                close(pipes[0]);
                close(pipes[1]);
            }
        };

        if (!MakeNonBlocking(pipes[0]) || !MakeNonBlocking(pipes[1]))
        {
            Printf(Tee::PriWarn, "Failed to make pipes non-blocking\n");
            return RC::OK;
        }

        // The kernel log read from /dev/kmsg looks like so:
        //     L,U,T,F;message\n
        // Where:
        // * L - unsigned integer: (level << 3) | facility
        //       level is print level, from 0 to 7, with 7 being debug ("lowest")
        //       facility is 0 for kernel and 1 for anything else, we only want kernel
        // * U - unsigned integer: unique identifier of message
        // * T - unsigned integer: timestamp, which seems like kernel uptime, although
        //       it cannot be compared to uptime from sysinfo(), uptime from sysinfo()
        //       seems to be uptime from system start, kernel starts later
        // * F - flags, usually some characters
        // * message - the text of the message starts right after the semicolon and ends
        //             with EOL, so each log entry is a single line of ASCII text
        if (regcomp(&s_KlogRe, "^([0-9]+)[^;]*;(.*)$", REG_EXTENDED | REG_NEWLINE) != 0)
        {
            Printf(Tee::PriWarn, "Failed to compile regex\n");
            return RC::OK;
        }
        DEFER
        {
            if (closeOnReturn)
            {
                regfree(&s_KlogRe);
            }
        };

        // Ignore/skip the beginning of the kernel log up till now.
        // We only care about logging stuff printed in the kernel log while MODS is running.
        if (ProcessKernelLog(fd, nullptr, [](const char*, int, void*) { }) != RC::OK)
        {
            // Ignore error, just assume /dev/kmsg is not functioning correctly
            return RC::OK;
        }

        closeOnReturn = false;
        s_Klog = fd;
        s_KlogEventPipeReadEnd = pipes[0];
        s_KlogEventPipe = pipes[1];
        return RC::OK;
    }

    void StopKernelLogger()
    {
        if (s_KlogThread != Tasker::NULL_THREAD)
        {
            MASSERT(s_KlogEventPipe != -1);

            char msg = 'X';
            if (write(s_KlogEventPipe, &msg, 1) == 1)
            {
                Tasker::Join(s_KlogThread);
            }
            else
            {
                Printf(Tee::PriError, "Failed to stop kernel logger\n");
            }
            s_KlogThread = Tasker::NULL_THREAD;
        }
        else if (s_Klog != -1)
        {
            string messages;
            ReadKernelLog(&messages);
            if (!messages.empty())
            {
                Printf(s_KernelLogPriority.load(), "%s", messages.c_str());
            }
        }
        if (s_Klog != -1)
        {
            close(s_Klog);
            s_Klog = -1;
        }
        if (s_KlogEventPipeReadEnd != -1)
        {
            close(s_KlogEventPipeReadEnd);
            s_KlogEventPipeReadEnd = -1;
        }
        if (s_KlogEventPipe != -1)
        {
            close(s_KlogEventPipe);
            s_KlogEventPipe = -1;

            regfree(&s_KlogRe);
        }
    }
}

RC Xp::StartKernelLogger()
{
    if (s_Klog == -1)
    {
        return RC::OK;
    }

    s_KlogThread = Tasker::CreateThread("Kernel logger", []
    {
        Tasker::DetachThread detach;
        string messages;
        messages.reserve(128);

        for (;;)
        {
            pollfd fds[2];
            // s_KlogEventPipeReadEnd should always be at index 0
            fds[0].fd      = s_KlogEventPipeReadEnd;
            fds[0].events  = POLLIN;
            fds[0].revents = 0;
            fds[1].fd      = s_Klog;
            fds[1].events  = POLLIN;
            fds[1].revents = 0;

#ifdef SIM_BUILD
            constexpr int timeout = 0;
#else
            constexpr int timeout = -1;
#endif
            const UINT32 numFDsToCheck = s_LogKernelMessages ? 2 : 1;
            const int ret = poll(fds, numFDsToCheck, timeout);

            if (ret < 0)
            {
                Printf(Tee::PriError, "Pipe error in kernel logger\n");
                break;
            }

            if (fds[1].revents)
            {
                const RC rc = ReadKernelLog(&messages);

                if (!messages.empty())
                {
                    Printf(s_KernelLogPriority.load(), "%s", messages.c_str());
                    messages.clear();
                }

                if (rc != RC::OK)
                {
                    return;
                }
            }

            if (fds[0].revents)
            {
                char buf;
                bool exitLogger = false;
                while (read(s_KlogEventPipeReadEnd, &buf, 1) == 1)
                {
                    // Ignoring 'W' on purpose since we don't have to do anything,
                    // except for waking up the thread.
                    if (buf == 'X')
                    {
                        exitLogger = true;
                    }
                }
                if (exitLogger)
                {
                    break;
                }
            }

#ifdef SIM_BUILD
            Tasker::Yield();
#endif
        }
    });

    if (s_KlogThread == Tasker::NULL_THREAD)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
// Platform specific initialization code.
//
RC Xp::OnEntry(vector<string> *pArgs)
{
    MASSERT((NULL != pArgs) && (0 != pArgs->size()));

#if !defined(CLIENT_SIDE_RESMAN) && !defined(TEGRA_MODS) && !defined(LIN_DA)
    if (geteuid() != 0)
    {
        Printf(Tee::PriError, "This program requires root privileges\n");
        return RC::LWRM_INSUFFICIENT_PERMISSIONS;
    }
#endif

    RC rc;
    const vector<string> &args = *pArgs;

#ifdef SUPPORT_SIGNALS
#ifdef SEQ_FAIL
    if (!Utility::IsSeqFailActive())
#endif
    {
        strncpy(s_ExeName, args[0].c_str(), sizeof(s_ExeName));
        s_ExeName[sizeof(s_ExeName)-1] = 0;

        // We'll cleanup and possibly print call stack if we're killed
        s_signals[SIGHUP]  = signal(SIGHUP,  OnModsCrash);
        s_signals[SIGINT]  = signal(SIGINT,  OnModsCrash);
        s_signals[SIGQUIT] = signal(SIGQUIT, OnModsCrash);
        s_signals[SIGTRAP] = signal(SIGTRAP, OnModsCrash);
        s_signals[SIGABRT] = signal(SIGABRT, OnModsCrash);
        s_signals[SIGBUS]  = signal(SIGBUS,  OnModsCrash);
        s_signals[SIGFPE]  = signal(SIGFPE,  OnModsCrash);
        s_signals[SIGSEGV] = signal(SIGSEGV, OnModsCrash);
        s_signals[SIGTERM] = signal(SIGTERM, OnModsCrash);
    }
#endif

    s_IsWatchdogOn = CommandLine::WatchdogTimeout() > 0;

    // Get CPU properties
    CHECK_RC(Cpu::Initialize());

    // Start our cooperative multithreader.
    rc = Tasker::Initialize();
    if (OK != rc)
    {
        Printf(Tee::PriAlways, "Tasker::Initialize failed!\n");
        return rc;
    }

    rc = Tee::TeeInitAfterTasker();
    if (OK != rc)
    {
        Printf(Tee::PriAlways, "Tee::TeeInitAfterTasker failed!\n");
        return rc;
    }

    CHECK_RC(DllHelper::Initialize());
    CHECK_RC(MemError::Initialize());

    rc = ConsoleManager::Instance()->Initialize();
    if (OK != rc)
    {
       Printf(Tee::PriAlways, "ConsoleManager::Initialize failed!\n");
       return rc;
    }
    if ((CommandLine::UserInterface() == CommandLine::UI_Script) &&
        !CommandLine::NlwrsesDisabled() &&
        ConsoleManager::Instance()->IsConsoleRegistered("nlwrses"))
    {
        CHECK_RC(ConsoleManager::Instance()->SetConsole("nlwrses", false));
    }

    CHECK_RC(PrepareKernelLogger());

    // Log that the watchdogs will be turned on
    if (s_IsWatchdogOn)
    {
        FileHolder watchdogFile;
        if (LwDiagUtils::OK == watchdogFile.Open("watchdog.log", "w"))
        {
            fprintf(watchdogFile.GetFile(), "%s\n", CommandLine::LogFileName().c_str());
            fprintf(watchdogFile.GetFile(), "Watchdog timers on\n");
        }
    }

    CHECK_RC(Xp::InitZeroFd());
    CHECK_RC(Xp::LinuxInternal::OpenDriver(args));

    // Find out if remote mode is specified
    if (!CommandLine::TelnetCmdline().empty())
    {
        SocketMods* const socket = GetSocket();

        SocketAnsi* const psa = new SocketAnsi(*socket);

        // Initialize remote console
        s_bRemoteConsole = true;
        s_pBaseSocket = socket;
        s_pSocket.reset(psa);

        string RemoteArgs;
        if (CommandLine::TelnetServer())
        {
            // Server mode
            CHECK_RC(TelnetUtil::StartTelnetServer(*psa, RemoteArgs));
        }
        else
        {
            // Client mode
            const UINT32 Ip = Socket::ParseIp(CommandLine::TelnetClientIp());
            const UINT16 Port = static_cast<UINT16>(CommandLine::TelnetClientPort());
            CHECK_RC(TelnetUtil::StartTelnetClient(*psa, RemoteArgs,
                                                   Ip, Port));
        }

        Printf(Tee::PriAlways, "Remote command line: %s\n",
                RemoteArgs.c_str());

        // Build new command line arguments from path, and the telnet command line
        RemoteArgs = args[0] + " " + CommandLine::TelnetCmdline() + " " + RemoteArgs;

        s_pRemoteSink.reset(new RemoteDataSink(s_pSocket.get()));
        Tee::SetScreenSink(s_pRemoteSink.get());

        // Build new argument list from remote arguments
        Utility::CreateArgv(RemoteArgs, pArgs);

        // Create file which will store elw vars set by MODS for PVS
        s_ModsElwVarsFd = open("modselw", O_CREAT | O_TRUNC | O_RDWR, 0644);
    }

    if (Xp::DoesFileExist("/sys/hvc"))
    {
        s_bHypervisor = true;
    }

    return OK;
}

//------------------------------------------------------------------------------
// Platform specific clean up code.
//
RC Xp::PreOnExit()
{
    StickyRC firstRc;

    // Log that the watchdogs have been turned off
    if (s_IsWatchdogOn)
    {
        FileHolder watchdogFile;
        if (LwDiagUtils::OK == watchdogFile.Open("watchdog.log", "a"))
        {
            fprintf(watchdogFile.GetFile(), "Watchdog timers off\n");
        }
    }

#ifdef INCLUDE_GPU
    // Shut down the GPU resman and OpenGL (if applicable) connection(s).
    if (GpuPtr()->IsInitialized())
    {
        firstRc = GpuPtr()->ShutDown(Gpu::ShutDownMode::Normal);
    }
#endif
    return firstRc;
}

RC Xp::OnExit(RC exitRc)
{
    StickyRC firstRc = exitRc;

#ifndef QNX
    MemoryTracker::CheckAndFree();
#endif

#ifdef SUPPORT_BLINK_LEDS
    if (s_BlinkLightsThreadLive)
    {
        s_BlinkLightsThreadLive = false;
        Tasker::Join(s_BlinkLightsThreadId);
    }
#endif
    CleanupJTMgr();

    // Clean up hardware or simulation platform
    Platform::Cleanup(Platform::CheckForLeaks);

    firstRc = Xp::LinuxInternal::CloseModsDriver();

    if (s_ZeroFd >= 0)
    {
        close(s_ZeroFd);
        s_ZeroFd = -1;
    }

    StopKernelLogger();

    // Lastly free the Tasker and console manager resources.  Console
    // manager should be cleaned up after all event threads complete
    // but before Tasker is cleaned up since console manager in an
    // initialized state uses Tasker
    firstRc = DllHelper::Shutdown();
    firstRc = Tee::TeeShutdownBeforeTasker();
    EventThread::Cleanup();
    ConsoleManager::Instance()->Cleanup();
    MemError::Shutdown();
    Tasker::Cleanup();

    // Shutdown output console stream system
    if (s_bRemoteConsole)
    {
        s_pSocket->Close();
        s_pSocket.reset(0);
        FreeSocket(s_pBaseSocket);
        s_pRemoteSink.reset(0);

        // Turn off console output!
        // @@@ Ideally, we would restore a "safe" sink.
        Tee::SetScreenSink(0);
        Tee::SetScreenLevel(Tee::LevNone);
    }

    // all sockets should be freed at this point, but if not, still free them
    MASSERT(NULL == s_pOsSockets);
    if (s_pOsSockets)
    {
        while (s_pOsSockets)
        {
            // FreeSocket removes it from the list
            FreeSocket(s_pOsSockets->back());
        }
    }

    return firstRc;
}

RC Xp::OnQuickExit(RC rc)
{
#ifdef INCLUDE_GPU
    GpuPtr()->ShutDown(Gpu::ShutDownMode::QuickAndDirty);
#endif

#if defined(TEGRA_MODS) && !defined(QNX)
    CheetAh::QuickAndDirtyShutDownSOC();
#endif

    // Clean up hardware or simulation platform
    Platform::Cleanup(Platform::DontCheckForLeaks);

    return rc;
}

RC Xp::PauseKernelLogger(bool pause)
{
    s_LogKernelMessages = !pause;
    if (s_KlogThread != Tasker::NULL_THREAD)
    {
        MASSERT(s_KlogEventPipe != -1);

        char msg = 'W';
        if (write(s_KlogEventPipe, &msg, 1) != 1)
        {
            Printf(Tee::PriError, "Failed to pause kernel logger\n");
            return RC::CANNOT_SET_STATE;
        }
    }
    return RC::OK;
}

void Xp::ExitFromGlobalObjectDestructor(INT32 exitValue)
{
    exit(exitValue);
}

static string FindFileInLdLibraryPath(const string *FileName)
{
    const char * path = getelw("LD_LIBRARY_PATH");
    if (!path)
    {
        return "";
    }
    char * s = new char [strlen(path) + 1];
    strcpy(s, path);
    char * ps = s;
    string fullPath;

    for (ps = strtok(s, ":"); ps; ps = strtok(NULL, ":"))
    {
        fullPath = ps;
        fullPath += "/";
        fullPath += *FileName;

        struct stat statbuf;
        if (stat(fullPath.c_str(), &statbuf) >= 0)
        {
            break;
        }
        fullPath = "";
    }
    delete [] s;

    return fullPath;
}

namespace // Anonymous namespace
{

// Parse out the library path from the error returned by dlerror
string GetAbsoluteLibPathFromDlError(const string & dlErrorStr)
{
    return dlErrorStr.substr(0, dlErrorStr.find(':'));
}

// Returns the real library path in "realPath" and true if the file appears to
// be a linker script, false otherwise
RC ParseRealLibPathFromLinkerScript(const string & scriptContents,
                                    string * realPath)
{
    RC rc;
    MASSERT(realPath);

    // Linker scripts generally have this at the top
    if (scriptContents.find("GNU ld script") == string::npos)
    {
        return RC::DLL_LOAD_FAILED;
    }

    RegExp realPathRegex;
    // Matches strings like "GROUP ( /lib64/libusb-0.1.so.4 )"
    // and captures "/lib64/libusb-0.1.so.4" out of such strings
    CHECK_RC(realPathRegex.Set("GROUP *\\(\\s*(\\S*?)\\s*\\)", RegExp::SUB));

    if (!realPathRegex.Matches(scriptContents))
    {
        return RC::DLL_LOAD_FAILED;
    }

    *realPath = realPathRegex.GetSubstring(1);

    return rc;
}

RC LoadLinkerScript(const string & fileName, void ** ModuleHandle)
{
    RC rc;

    string linkerScriptStr;
    if (OK != Xp::InteractiveFileRead(fileName.c_str(),
                                      &linkerScriptStr))
    {
        return RC::DLL_LOAD_FAILED;
    }

    string realLibPath;
    CHECK_RC(ParseRealLibPathFromLinkerScript(linkerScriptStr, &realLibPath));

    *ModuleHandle = dlopen(realLibPath.c_str(), RTLD_NOW);

    // Even if dlopen fails here, still return OK
    // The failed dlopen will be caught by the *ModuleHandle check in this
    // function's caller

    return rc;
}
} // anonymous namespace

//------------------------------------------------------------------------------
RC Xp::LoadDLL(const string &fileName, void **pModuleHandle, UINT32 loadDLLFlags)
{
    Tasker::MutexHolder dllMutexHolder(DllHelper::GetMutex());
    string fileNameWithSuffix = DllHelper::AppendMissingSuffix(fileName);

    // Try to guess where dlopen will find the library.
    // This is failure prone, but many man-hours will be saved if we can
    // tell people why their sim-runs mysteriously fail in the farm.
    //
    Printf(Tee::PriLow,
           "Check LD_LIBRARY_PATH to try to guess what dlopen() will use...\n");

    string fullPath = FindFileInLdLibraryPath(&fileNameWithSuffix);
    if (fullPath.size() == 0)
    {
        Printf(Tee::PriLow, " ... not found.  Try dlopen anyway...\n");
    }
    else
    {
        Printf(Tee::PriLow, " ...%s\n", fullPath.c_str());
    }

    UINT32 loadFlags = (loadDLLFlags & LoadLazy) ? RTLD_LAZY : RTLD_NOW;
    loadFlags |= ((loadDLLFlags & GlobalDLL) ? RTLD_GLOBAL : 0);
#if !defined(QNX) && !defined(BIONIC)
    loadFlags |= ((loadDLLFlags & LoadNoDelete) ? RTLD_NODELETE : 0);
    loadFlags |= ((loadDLLFlags & DeepBindDLL) ? RTLD_DEEPBIND : 0);
#endif
    LwDiagUtils::EC ec =
        LwDiagXp::LoadDynamicLibrary(fileNameWithSuffix, pModuleHandle, loadFlags);

    if (OK != Utility::InterpretModsUtilErrorCode(ec))
    {
        string dlErrorStr = dlerror();
        // If we didn't find ELF magic number, maybe it's a linker script
        if (string::npos != dlErrorStr.find("invalid ELF header"))
        {
            string absPath = GetAbsoluteLibPathFromDlError(dlErrorStr);
            LoadLinkerScript(absPath, pModuleHandle);
        }

        if (*pModuleHandle == nullptr)
        {
            Printf(Tee::PriError, "Failed to open shared library %s: %s\n",
                   fileNameWithSuffix.c_str(), dlErrorStr.c_str());
            return RC::DLL_LOAD_FAILED;
        }
    }

    return DllHelper::RegisterModuleHandle(*pModuleHandle, !!(loadDLLFlags & UnloadDLLOnExit));
}

//------------------------------------------------------------------------------
RC Xp::UnloadDLL(void * moduleHandle)
{
    Tasker::MutexHolder dllMutexHolder(DllHelper::GetMutex());
    RC rc;

    CHECK_RC(DllHelper::UnregisterModuleHandle(moduleHandle));

    LwDiagUtils::EC ec = LwDiagXp::UnloadDynamicLibrary(moduleHandle);
    if (OK != Utility::InterpretModsUtilErrorCode(ec))
    {
        string dlErrorString = dlerror();
        Printf(Tee::PriError, "Error closing shared library: %s\n",
               dlErrorString.c_str());
        return RC::BAD_PARAMETER;
    }
    return OK;
}

//------------------------------------------------------------------------------
void * Xp::GetDLLProc(void * moduleHandle, const char * funcName)
{
    return LwDiagXp::GetDynamicLibraryProc(moduleHandle, funcName);
}

//------------------------------------------------------------------------------
string Xp::GetDLLSuffix()
{
    return LwDiagXp::GetDynamicLibrarySuffix();
}

//------------------------------------------------------------------------------
Xp::ExelwtableType Xp::GetExelwtableType(const char * FileName)
{
    Xp::ExelwtableType ret = EXE_TYPE_UNKNOWN;

    FileHolder FileExelwtable;

    if (Xp::DoesFileExist(FileName))
    {
        if (FileExelwtable.Open(FileName, "rb") != OK)
        {
            return EXE_TYPE_UNKNOWN;
        }
    }
    else
    {
        string StringFileName(FileName);
        string LdLibPath = FindFileInLdLibraryPath(&StringFileName);
        if ( (LdLibPath.size() == 0) ||
             (FileExelwtable.Open(LdLibPath, "rb") != OK) )
        {
            return EXE_TYPE_UNKNOWN;
        }
    }

    UINT32 ELFHeader[2];
    if (fread(&ELFHeader, sizeof(ELFHeader), 1, FileExelwtable.GetFile()) != 1)
        return EXE_TYPE_UNKNOWN;

    if (ELFHeader[0] != 0x464C457F) // "\0x7fELF"
        return EXE_TYPE_UNKNOWN;

    UINT08 EIClass = ELFHeader[1]&0xFF;

    switch (EIClass)
    {
        case 0x01:
            ret = EXE_TYPE_32;
            break;
        case 0x02:
            ret = EXE_TYPE_64;
            break;
        default:
            Printf(Tee::PriError, "Unknown exelwtable type %d of file %s\n",
                   EIClass, FileName);
            MASSERT(!"Unknown exelwtable type");
            break;
    }

    return ret;
}

//------------------------------------------------------------------------------
// Determine if running with remote console support.
//
bool Xp::IsRemoteConsole()
{
    return s_bRemoteConsole;
}

//------------------------------------------------------------------------------
// Get Socket for reading remote connection.
//
SocketMods * Xp::GetRemoteConsole()
{
    return s_pSocket.get();
}

//------------------------------------------------------------------------------
// Allocate a socket.
SocketMods * Xp::GetSocket()
{
    SocketMods *psd = new SocketMods(LwDiagXp::CreateSocket());
    if (!psd)
        return nullptr;

    // Store for deletion later
    if (!s_pOsSockets)
    {
        s_pOsSockets = new vector<SocketMods *>;
        MASSERT(s_pOsSockets);
    }

    s_pOsSockets->push_back(psd);

    return psd;
}

// Delete a socket.
void Xp::FreeSocket(SocketMods * sock)
{
    MASSERT(s_pOsSockets);
    MASSERT(sock);

    static vector<SocketMods *>::iterator sock_iter;
    for (sock_iter = s_pOsSockets->begin();
         sock_iter != s_pOsSockets->end();
         sock_iter++)
    {
        if ((*sock_iter) == sock)
        {
            delete sock;
            s_pOsSockets->erase(sock_iter);
            break;
        }
    }

    if (s_pOsSockets->empty())
    {
        delete s_pOsSockets;
        s_pOsSockets = NULL;
    }
}

//------------------------------------------------------------------------------
// Issue a break point.
//
void Xp::BreakPoint()
{
    Cpu::BreakPoint();
}

// Send a string to the attached debugger
void Xp::DebugPrint(const char* str, size_t strLen)
{
}

//------------------------------------------------------------------------------
//!< Return the page size in bytes for this platform.
size_t Xp::GetPageSize()
{
    static bool   s_Initialized = false;
    static size_t s_PageSize    = 0;

    if (!s_Initialized)
    {
        const long pageSize = sysconf(_SC_PAGESIZE);
        if (pageSize < 0)
        {
            Printf(Tee::PriError, "Failed to obtain page size from OS, defaulting to 4KB\n");
            s_PageSize = 4096;
        }
        else
        {
            s_PageSize = static_cast<size_t>(pageSize);
        }
        s_Initialized = true;

        const bool pageSizeIsPowerOf2 = s_PageSize && !(s_PageSize & (s_PageSize - 1U));

        MASSERT(pageSizeIsPowerOf2);

        if (!pageSizeIsPowerOf2)
        {
            Printf(Tee::PriError, "Page size is %u B which is not power of 2, defaulting to 4KB\n",
                   static_cast<unsigned>(s_PageSize));
            s_PageSize = 4096U;
        }

        Printf(Tee::PriLow, "Sysmem page size is %u KB\n",
               static_cast<unsigned>(s_PageSize / 1024U));
    }

    return s_PageSize;
}

//------------------------------------------------------------------------------
//! Implementation of Xp::Process for Linux
namespace
{
    class ProcessImpl : public Xp::Process
    {
    public:
        ~ProcessImpl()                { Kill(); }
        RC     Run(const string& cmd) override;
        bool   IsRunning()            override;
        RC     Wait(INT32* pStatus)   override;
        RC     Kill()                 override;
    private:
        pid_t m_Pid        = 0;
        int   m_ExitStatus = 0;
    };

    RC ProcessImpl::Run(const string& cmd)
    {
        MASSERT(m_Pid == 0);
        m_Pid = fork();
        if (m_Pid == 0)
        {
            for (const auto& elw: m_Elw)
            {
                Xp::SetElw(elw.first, elw.second);
            }
            setpgid(0, 0);
            int status = execl("/bin/sh", "bin/sh", "-c", cmd.c_str(), nullptr);
            exit(status);  // Should never get here
        }
        else if (m_Pid < 0)
        {
            m_Pid = 0;
            return RC::SCRIPT_FAILED_TO_EXELWTE;
        }
        return OK;
    }

    bool ProcessImpl::IsRunning()
    {
        if (m_Pid > 0 && waitpid(m_Pid, &m_ExitStatus, WNOHANG) == m_Pid)
        {
            m_Pid = 0;
        }
        return (m_Pid > 0);
    }

    RC ProcessImpl::Wait(INT32* pStatus)
    {
        if (m_Pid > 0)
        {
            if (waitpid(m_Pid, &m_ExitStatus, 0) == m_Pid)
            {
                m_Pid = 0;
            }
            else
            {
                return RC::FILE_CHILD;
            }
        }
        if (pStatus)
        {
            *pStatus = WEXITSTATUS(m_ExitStatus);
        }
        return OK;
    }

    RC ProcessImpl::Kill()
    {
        if (m_Pid > 0 && kill(m_Pid, SIGKILL) != 0)
        {
            return RC::FILE_CHILD;
        }
        m_Pid = 0;
        return OK;
    }
}

unique_ptr<Xp::Process> Xp::AllocProcess()
{
    return make_unique<ProcessImpl>();
}

//------------------------------------------------------------------------------
//!< Return the process id
UINT32 Xp::GetPid()
{
    return getpid();
}

//------------------------------------------------------------------------------
//!< Test whether the indicated PID is running
bool Xp::IsPidRunning(UINT32 pid)
{
    return kill(pid, 0) == 0;
}

//------------------------------------------------------------------------------
//!< Return the effective username
string Xp::GetUsername()
{
    uid_t euid = geteuid();
    const passwd *pPasswd = getpwuid(euid);
    return pPasswd ? pPasswd->pw_name : Utility::StrPrintf("user%u", euid);
}

//------------------------------------------------------------------------------
//!< Dump the lwrrently mapped pages.
void Xp::DumpPageTable()
{
}

//------------------------------------------------------------------------------
//!< Reserve resources for subsequent allocations of the same Attrib
//!< by AllocPages or AllocPagesAligned.
//!< Under DOS, this will pre-allocate a contiguous heap and MTRR reg.
//!< Platforms that use real paging memory managers may ignore this.
void Xp::ReservePages
(
    size_t NumBytes,
    Memory::Attrib Attrib
)
{
    // nop
}

//------------------------------------------------------------------------------
//!< Program a Memory Type and Range Register to control cache mode
//!< for a range of physical addresses.
//!< Called by the resman to make the (unmapped) AGP aperture WC.
RC Xp::SetMemRange
(
    PHYSADDR    PhysicalAddress,
    size_t      NumBytes,
    Memory::Attrib Attrib
)
{
    // This issue require more thinking
    // At the moment we assume that implementation is unnecessary
    return OK;
}

//------------------------------------------------------------------------------
RC Xp::UnSetMemRange
(
    PHYSADDR    PhysicalAddress,
    size_t      NumBytes
)
{
    // This issue require more thinking
    // At the moment we assume that implementation is unnecessary
    return OK;
}

// Pure virtual memory allocation APIs mostly for use in support of LWCA UVA.
// Implementations are taken from :
//
// drivers/gpgpu/shared/lwos/lwos_common_posix.cpp
// drivers/gpgpu/shared/lwos/Linux/lwosLinux.cpp
namespace
{
    int ModsProtectFlagsToMmapFlags(UINT32 protect)
    {
        int prot = PROT_NONE;
        if (protect & Memory::Readable)
            prot |= PROT_READ;
        if (protect & Memory::Writeable)
            prot |= PROT_WRITE;
        if (protect & Memory::Exelwtable)
            prot |= PROT_EXEC;
        return prot;
    }

    void IntersectAndAlignRanges
    (
        size_t  range1Start,
        size_t  range1End,
        size_t  range2Start,
        size_t  range2End,
        size_t  align,
        size_t *rangeIntersectionStart,
        size_t *rangeIntersectionEnd
    )
    {
        *rangeIntersectionStart = max(range1Start, range2Start);
        *rangeIntersectionEnd = min(range1End, range2End);

        *rangeIntersectionStart = ALIGN_UP(*rangeIntersectionStart, align);

        if (*rangeIntersectionStart > *rangeIntersectionEnd)
        {
            *rangeIntersectionEnd = *rangeIntersectionStart;
        }
    }
}

//------------------------------------------------------------------------------
RC Xp::AllocAddressSpace
(
    size_t  numBytes,
    void ** ppReturnedVirtualAddress,
    void *  pBase,
    UINT32  protect,
    UINT32  vaFlags
)
{
    if ((vaFlags == Memory::VirualAllocDefault) &&
        (protect == Memory::ProtectDefault))
    {
        if (s_ZeroFd < 0)
        {
            Printf(Tee::PriError, "/dev/zero file descriptor not available\n");
            return RC::UNSUPPORTED_FUNCTION;
        }

        // Always throw in a dummy page for good measure, to catch overruns
        numBytes += GetPageSize();

        *ppReturnedVirtualAddress = mmap(pBase, numBytes, PROT_NONE,
                                        MAP_PRIVATE, s_ZeroFd, 0);
        if (*ppReturnedVirtualAddress == MAP_FAILED)
            return RC::CANNOT_ALLOCATE_MEMORY;

        Pool::AddNonPoolAlloc(*ppReturnedVirtualAddress, numBytes);
        return OK;
    }

    int prot = ModsProtectFlagsToMmapFlags(protect);

    int flags = 0;
    if (vaFlags & Memory::Shared)
        flags |= MAP_SHARED;
    if (vaFlags & Memory::Anonymous)
        flags |= MAP_ANONYMOUS;
    if (vaFlags & Memory::Fixed)
        flags |= MAP_FIXED;
    if (vaFlags & Memory::Private)
        flags |= MAP_PRIVATE;

    *ppReturnedVirtualAddress = mmap(pBase, numBytes, prot, flags, -1, 0);
    if (*ppReturnedVirtualAddress == MAP_FAILED)
        return RC::CANNOT_ALLOCATE_MEMORY;
    return OK;
}

RC Xp::FreeAddressSpace
(
    void *                    pVirtualAddress,
    size_t                    numBytes,
    Memory::VirtualFreeMethod vfMethod
)
{
    switch (vfMethod)
    {
        case Memory::Decommit:
            if (mmap(pVirtualAddress, numBytes,
                     PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0) != pVirtualAddress)
            {
                Printf(Tee::PriError, "Decommiting virtual memory failed : %d", errno);
                return RC::SOFTWARE_ERROR;
            }
            break;
        case Memory::Release:
            if (munmap(pVirtualAddress, numBytes) != 0)
            {
                Printf(Tee::PriError, "Releasing virtual memory failed : %d", errno);
                return RC::SOFTWARE_ERROR;
            }
            break;
        case Memory::VirtualFreeDefault:
            // Make sure to unmap the dummy page, too
            Pool::RemoveNonPoolAlloc(pVirtualAddress, numBytes);
            munmap(pVirtualAddress, numBytes + GetPageSize());
            break;
        default:
            Printf(Tee::PriError, "%s : Unknown free method %d\n", __FUNCTION__, vfMethod);
            return RC::BAD_PARAMETER;
    }
    return OK;
}

void *Xp::VirtualFindFreeVaInRange(size_t size, void *pBase, void *pLimit, size_t align)
{
    FileHolder fh;
    if (OK != fh.Open("/proc/self/maps", "r"))
        return NULL;

    char line[256];
    size_t freeVaStart = 0;
    size_t freeVaEnd   = 0;
    size_t prevEnd     = 0;
    while (fgets(line, sizeof(line), fh.GetFile()) != NULL)
    {
        if (strchr(line, '\n') == NULL)
        {
            Printf(Tee::PriError, "%s : Line too long, skipping the rest of the file\n",
                   __FUNCTION__);
            return NULL;
        }

        size_t start = 0;
        size_t end   = 0;
        if (sscanf(line, "%zx-%zx", &start, &end) != 2)
        {
            // Skip lines that don't have a range; notably the first line doesn't
            continue;
        }

        // There is a gap between the end of the previous VA and the begining
        // of the current one.
        IntersectAndAlignRanges((size_t)pBase,
                                (size_t)pLimit,
                                prevEnd,
                                start,
                                align,
                                &freeVaStart,
                                &freeVaEnd);
        if (freeVaEnd - freeVaStart >= size)
            return (void *)freeVaStart;

        // If the end of current VA goes beyond the limit, we can give up.
        if (end >= (size_t)pLimit)
            return NULL;

        prevEnd = end;
    }

    IntersectAndAlignRanges((size_t)pBase,
                            (size_t)pLimit,
                            prevEnd,
                            (size_t)pLimit,
                            align,
                            &freeVaStart,
                            &freeVaEnd);
    if (freeVaEnd - freeVaStart >= size)
        return (void *)freeVaStart;

    return NULL;
}

RC Xp::VirtualProtect(void *pAddr, size_t size, UINT32 protect)
{
    int prot = ModsProtectFlagsToMmapFlags(protect);
    if (mprotect(pAddr, size, prot) != 0)
    {
        Printf(Tee::PriError, "Protecting virtual memory failed : %d", errno);
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

#ifndef QNX
RC Xp::VirtualMadvise(void *pAddr, size_t size, Memory::VirtualAdvice advice)
{
    int madviseAdvice;
    switch (advice)
    {
        case Memory::Normal:
            madviseAdvice = MADV_NORMAL;
            break;
        case Memory::Fork:
            madviseAdvice = MADV_DOFORK;
            break;
        case Memory::DontFork:
            madviseAdvice = MADV_DONTFORK;
            break;
        default:
            Printf(Tee::PriError, "%s : Unknown advice %d\n", __FUNCTION__, advice);
            return RC::BAD_PARAMETER;
    }

    if (madvise(pAddr, size, madviseAdvice) != 0)
    {
        Printf(Tee::PriError, "Advising virtual memory failed : %d", errno);
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}
#endif

//------------------------------------------------------------------------------
RC Xp::RemapPages
(
    void *          VirtualAddress,
    PHYSADDR        PhysicalAddress,
    size_t          NumBytes,
    Memory::Attrib  Attrib,
    Memory::Protect Protect
)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
void* Xp::ExecMalloc(size_t numBytes)
{
    void*        ptr         = nullptr;
    const size_t pageSize    = GetPageSize();
    const size_t alignedSize = Utility::AlignUp(numBytes, pageSize);

    if (posix_memalign(&ptr, pageSize, alignedSize))
        return nullptr;

    if (mprotect(ptr, alignedSize, PROT_READ | PROT_WRITE | PROT_EXEC))
    {
        Printf(Tee::PriWarn, "Failed to enable exec on ptr %p size 0x%zx\n", ptr, alignedSize);
        free(ptr);
        return nullptr;
    }

    Pool::AddNonPoolAlloc(ptr, alignedSize);

    return ptr;
}

void Xp::ExecFree(void* ptr, size_t numBytes)
{
    if (ptr)
    {
        const size_t alignedSize = Utility::AlignUp(numBytes, GetPageSize());
        Pool::RemoveNonPoolAlloc(ptr, alignedSize);
        mprotect(ptr, alignedSize, PROT_READ | PROT_WRITE);
        free(ptr);
    }
}

void Xp::MemCopy(void * dst, void * src, UINT32 length, UINT32 type)
{
    memcpy(dst, src, length);
}

//------------------------------------------------------------------------------
// Return true if file exists.
//
bool Xp::DoesFileExist
(
    string strFilename
)
{
    // Check if the file exists.
    if (!LwDiagXp::DoesFileExist(strFilename))
    {
        //fail if absolute path entered and file not found
        if (strFilename[0] == '/')
            return false;
        // Search for the file in other locations that Import() checks
        vector<string> paths;
        paths.push_back(CommandLine::ProgramPath());
        paths.push_back(CommandLine::ScriptFilePath());
        string path = Utility::FindFile(strFilename, paths);
        return !path.empty(); //return whether or not file found at either other path
    }
    return true; //found it immediately
}

//------------------------------------------------------------------------------
// Delete file
//
RC Xp::EraseFile(const string& FileName)
{
    int ret = unlink(FileName.c_str());

    if (ret != 0)
    {
        Printf(Tee::PriError, "Failed to delete file %s\n", FileName.c_str());

        return Utility::InterpretFileError();
    }

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Open file.  Normally called by Utility::OpenFile() wrapper.
//!
FILE *Xp::Fopen(const char *FileName, const char *Mode)
{
    return LwDiagXp::Fopen(FileName, Mode);
}

//-----------------------------------------------------------------------------
FILE* Xp::Popen(const char* command, const char* type)
{
    return popen(command, type);
}

//-----------------------------------------------------------------------------
INT32 Xp::Pclose(FILE* stream)
{
    return pclose(stream);
}

UINT64 Xp::QueryPerformanceCounter()
{
#if defined(USE_PTHREADS)
    timespec tv;
    if (0 != clock_gettime(CLOCK_REALTIME, &tv))
    {
        return 0;
    }
    const UINT64 sec = static_cast<UINT64>(tv.tv_sec);
    const UINT64 nsec = static_cast<UINT64>(tv.tv_nsec);
    return 1000000000ULL * sec + nsec;
#else
    struct timeval Now;
    gettimeofday(&Now, NULL);

    UINT64 Seconds      = UINT64(Now.tv_sec);
    UINT64 Microseconds = UINT64(Now.tv_usec);

    return 1000000 * Seconds + Microseconds;
#endif
}

UINT64 Xp::QueryPerformanceFrequency()
{
#if defined(USE_PTHREADS)
    // QueryPerformanceCounter() uses clock_gettime which returns ns
    return 1000000000ULL;
#else
    // This is supposed to return the number of "ticks" in a second.  Since
    // the gettimeofday() call we are using in QueryPerformanceCounter() returns
    // the number of microseconds... we return 1,000,000 since there are
    // 1,000,000 us in a sec.
    return 1000000;
#endif
}

/**
 * Xp::QueryIntervalTimer
 *   @return unix system timer value
 */
UINT32 Xp::QueryIntervalTimer()
{
    return (UINT32) clock();
}

/**
 * Xp::QueryIntervalFrequency
 *   @return unix system timer freq
 */
UINT32 Xp::QueryIntervalFrequency()
{
    return CLOCKS_PER_SEC;
}

// nanoseconds
UINT64 Xp::GetWallTimeNS()
{
#if defined(USE_PTHREADS)
    // QueryPerformanceCounter() uses clock_gettime which returns ns
    return QueryPerformanceCounter();
#else
    // gettimeofday() is the basis for time; its granularity is microseconds
    UINT64 us = Xp::GetWallTimeUS();
    UINT64 ns = us * 1000;
    return ns;
#endif
}

UINT64 Xp::GetWallTimeUS()
{
#if defined(USE_PTHREADS)
    // QueryPerformanceCounter() uses clock_gettime which returns ns
    return QueryPerformanceCounter() / 1000ULL;
#else
    return Xp::QueryPerformanceCounter();
#endif
}

UINT64 Xp::GetWallTimeMS()
{
#if defined(USE_PTHREADS)
    // QueryPerformanceCounter() uses clock_gettime which returns ns
    return QueryPerformanceCounter() / 1000000ULL;
#else
    UINT64 us = Xp::GetWallTimeUS();
    UINT64 ms = us / 1000;
    return ms;
#endif
}

//------------------------------------------------------------------------------
// Get and set an environment variable.
//
string Xp::GetElw
(
    string Variable
)
{
    return LwDiagXp::GetElw(Variable);
}

RC Xp::SetElw
(
    string Variable,
    string Value
)
{
    if (s_ModsElwVarsFd != -1)
    {
        const string arg = Variable + "=\"" + Value + "\"\n";
        if (static_cast<int>(arg.size()) !=
            write(s_ModsElwVarsFd, arg.c_str(), arg.size()))
        {
            return RC::FILE_WRITE_ERROR;
        }
    }

    int ret = setelw(Variable.c_str(), Value.c_str(), 1);
    if (ret)
        return RC::SOFTWARE_ERROR;
    else
        return OK;
}

/**
 *------------------------------------------------------------------------------
 * @function(Xp::FlushFstream)
 *
 * Force recent changes to an open file to be written to disk immediately.
 *------------------------------------------------------------------------------
 */
void Xp::FlushFstream(FILE *pFile)
{
    // Flush the output buffer to the operating system.
    fflush(pFile);
}

//! \brief Checks write permission for the defined file
//!
//! Check write permission on the file, returning OK if writeable
RC Xp::CheckWritePerm(const char *FileName)
{
    string path = Utility::StripFilename(FileName);
    if (path.length() == 0)
    {
        path=".";
    }
    if (!access(FileName, W_OK) ||
        (access(FileName, F_OK) && !access(path.c_str(), W_OK)))
    {
        Printf(Tee::PriLow, "Confirmed write permission for %s\n", FileName);
        return OK;
    }

    Printf(Tee::PriLow, "Write permission denied for %s!\n", FileName);
    return Utility::InterpretFileError();
}

//------------------------------------------------------------------------------
#ifdef SUPPORT_BLINK_LEDS
static void BlinkLightsThread(void*)
{
    // We do not detach from the Tasker here because this thread
    // will let us know if the Tasker is still alive by blinking
    // the keyboard LEDs every second.

    // These are from linux/kd.h
    // We don't include linux/kd.h to avoid compiler warnings.
    const int KDGETLED = 0x4B31;
    const int KDSETLED = 0x4B32;
    const unsigned char LED_SCR = 0x01;
    const unsigned char LED_NUM = 0x02;
    const unsigned char LED_CAP = 0x04;

    unsigned char savedLeds = 0;
    if (0 == ioctl(s_Keyboard, KDGETLED, &savedLeds))
    {
        for (int blinkState=0; s_BlinkLightsThreadLive; blinkState++)
        {
            const unsigned char leds[] = { LED_NUM, LED_CAP, LED_SCR, LED_CAP };
            if (0 != ioctl(s_Keyboard, KDSETLED, leds[blinkState&3]))
            {
                Printf(Tee::PriWarn, "Unable to alter state of keyboards lights!\n");
                break;
            }
            Tasker::Sleep(1000);
        }

        if (0 != ioctl(s_Keyboard, KDSETLED, savedLeds))
        {
            Printf(Tee::PriWarn, "Unable to restore state of keyboards lights!\n");
        }
    }
    else
    {
        Printf(Tee::PriWarn, "Unable to get state of keyboards lights!\n");
    }
    close(s_Keyboard);
}
#endif

//------------------------------------------------------------------------------
RC Xp::StartBlinkLightsThread()
{
#ifdef SUPPORT_BLINK_LEDS
    const char* const KeybDev = "/dev/console";

    // Check if console device exists
    struct stat buf;
    if (0 != stat(KeybDev, &buf))
    {
        Printf(Tee::PriWarn, "%s does not exist, LEDs cannot blink.\n", KeybDev);
        return OK;
    }

    // Check access permissions
    if (0 != access(KeybDev, R_OK | W_OK))
    {
        Printf(Tee::PriWarn, "Insufficient access rights for %s, LEDs cannot blink.\n",
                KeybDev);
        return OK;
    }

    // Open console device
    s_Keyboard = open(KeybDev, O_RDWR);
    if (s_Keyboard == -1)
    {
        Printf(Tee::PriWarn, "Unable to open %s for LED blinking.\n",
                KeybDev);
        return OK;
    }

    if (s_BlinkLightsThreadId == Tasker::NULL_THREAD)
    {
        s_BlinkLightsThreadLive = true;
        s_BlinkLightsThreadId = Tasker::CreateThread(BlinkLightsThread, NULL,
                                                     Tasker::MIN_STACK_SIZE,
                                                     "BlinkLights");
    }
    if (s_BlinkLightsThreadId == Tasker::NULL_THREAD)
        return RC::SOFTWARE_ERROR;
    else
        return OK;
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

#ifdef SUPPORT_SIGNALS
namespace
{
    bool RunCmd(const char* command, const char* errorStr)
    {
        Printf(Tee::PriNormal, "Running: %s\n", command);
        if (system(command) != 0)
        {
            Printf(Tee::PriWarn, "%s\n", errorStr);
            return false;
        }
        return true;
    };

    class BufMgr
    {
        public:
            char   buf[1024];
            size_t size = 0;

            template<typename T, size_t len>
            bool Insert(size_t pos, T (&str)[len])
            {
                return Insert(pos, &str[0], len - 1);
            }

            bool Insert(size_t pos, const char* str, size_t len)
            {
                // Note: leave room for trailing zero
                if (size + len >= sizeof(buf))
                {
                    Printf(Tee::PriWarn, "File name too long\n");
                    return false;
                }

                if (pos > size)
                {
                    pos = size;
                }
                else if (pos < size)
                {
                    memmove(&buf[pos + len], &buf[pos], size - pos);
                }

                memcpy(&buf[pos], str, len);
                size += len;
                return true;
            }

            bool Append(const char* str, size_t len)
            {
                return Insert(size, str, len);
            }

            void Erase(size_t begin, size_t end)
            {
                MASSERT(begin <= size);
                MASSERT(end <= size);
                MASSERT(begin <= end);
                if (begin == end)
                {
                    return;
                }

                if (end < size)
                {
                    memmove(&buf[begin], &buf[end], size - end);
                }

                size -= end - begin;
            }

            bool ReadLine(FileHolder& fh)
            {
                if (size + 1 >= sizeof(buf))
                {
                    return false;
                }

                const char* ret = fgets(&buf[size], sizeof(buf) - size, fh.GetFile());
                if (!ret)
                {
                    return false;
                }

                size += strlen(ret);

                // Remove trailing EOL
                if (size && (buf[size - 1] == '\n'))
                {
                    buf[--size] = 0;
                }
                return true;
            }

            bool ClearAndReadLine(FileHolder& fh)
            {
                size = 0;
                return ReadLine(fh);
            }

            void Clear()
            {
                size = 0;
            }

            bool Run(const char* errorStr)
            {
                return RunCmd(buf, errorStr);
            }
    };

    void DownloadSymbols()
    {
        static const char symbolsLinkFile[] = "gdb_symbols.txt";
        if (!Xp::DoesFileExist(symbolsLinkFile))
        {
            Printf(Tee::PriDebug, "File %s not found\n", symbolsLinkFile);
            return;
        }

        // Read URL to symbols for this MODS package
        FileHolder file;
        if (RC::OK != file.Open(symbolsLinkFile, "r"))
        {
            Printf(Tee::PriWarn, "Failed to load URL from %s\n", symbolsLinkFile);
            return;
        }

        BufMgr buf;
        if (!buf.ReadLine(file))
        {
            Printf(Tee::PriWarn, "Failed to load URL to symbols\n");
            return;
        }
        file.Close();

        // Retrieve location of the symbols package from DVS
#define WGET_CMD "wget -lw -O "
#define SYMBOLS_TXT "symbols.txt"
        static const char wgetSymbolsLoc[] = WGET_CMD SYMBOLS_TXT " \"";
        if (!buf.Insert(0, wgetSymbolsLoc) ||
            !buf.Append("\"", 2))
        {
            return;
        }
        if (!buf.Run("Failed to query symbols location from DVS"))
        {
            return;
        }

        if (RC::OK != file.Open(SYMBOLS_TXT, "r"))
        {
            Printf(Tee::PriWarn, "Failed to load URL from " SYMBOLS_TXT "\n");
            return;
        }
        if (!buf.ClearAndReadLine(file))
        {
            Printf(Tee::PriWarn, "Failed to load status from " SYMBOLS_TXT "\n");
            return;
        }
        Printf(Tee::PriNormal, "%s\n", buf.buf);
        static const char SUCCESS[] = "SUCCESS,";
        if (memcmp(buf.buf, SUCCESS, sizeof(SUCCESS) - 1) != 0)
        {
            Printf(Tee::PriWarn, "Failed to query symbols location from DVS\n");
            return;
        }

        if (!buf.ClearAndReadLine(file))
        {
            Printf(Tee::PriWarn, "Failed to load path from " SYMBOLS_TXT "\n");
            return;
        }
        file.Close();
        Printf(Tee::PriNormal, "%s\n", buf.buf);

        const char* path = static_cast<const char*>(memchr(buf.buf, ',', buf.size));
        if (!path)
        {
            Printf(Tee::PriWarn, "Failed to find path in " SYMBOLS_TXT "\n");
            return;
        }

        const size_t urlPos = path + 1 - buf.buf;
        buf.Erase(0, urlPos);

        // Download symbols package
#define SYMBOLS_TGZ "symbols.tgz"
        static const char wgetSymbolsTgz[] = WGET_CMD SYMBOLS_TGZ " \"";
        if (!buf.Insert(0, wgetSymbolsTgz) ||
            !buf.Append("\"", 2))
        {
            return;
        }
        if (!buf.Run("Failed to download symbols"))
        {
            return;
        }

        // Unpack symbols archive
        RunCmd("tar xzf " SYMBOLS_TGZ, "Failed to unpack symbols");

        // Get exelwtable path, this is where symbol file must be placed
        static char exePath[sizeof(s_ExeName)];
        strncpy(exePath, s_ExeName, sizeof(exePath));
        exePath[sizeof(exePath) - 1] = 0;
        char* slashPos = strrchr(exePath, '/');
        if (slashPos)
        {
            *slashPos = '\0';
        }
        else
        {
            exePath[0] = 0;
        }

        const auto UnXzSymbols = [](const char* filename, const char* exePath)
        {
            if (access(filename, R_OK) == 0)
            {
                static const char unxzCmd[] = "unxz ";
                BufMgr buf;
                buf.Insert(0, unxzCmd);
                const size_t filenameLen = strlen(filename);
                buf.Append(filename, filenameLen + 1);

                if (buf.Run("Failed to unpack symbols") && exePath[0])
                {
                    buf.Clear();
                    static const char mvCmd[] = "mv ";
                    buf.Insert(0, mvCmd);
                    buf.Append(filename, filenameLen - strlen(".xz"));
                    buf.Append(" ", 1);
                    buf.Append(exePath, strlen(exePath) + 1);
                    buf.Run("Failed to move symbols to MODS runspace");
                }
            }
        };

        // Unpack debug symbols if they've been compressed with xz
        UnXzSymbols("mods.debug.xz", exePath);
#ifdef INCLUDE_OGL
        UnXzSymbols("librtcore.so.debug.xz", exePath);
#endif
    }
}

//------------------------------------------------------------------------------
// This should get called when mods crashes, to do emergency cleanup
//
static void OnModsCrash(int sig)
{
    DEFER
    {
        // Restore the original signal handler
        signal(sig, s_signals[sig]);

        // Re-send the signal
        kill(getpid(), sig);
    };

    bool printStack = false;
    const char* sigName = "Unknown signal";
    switch (sig)
    {
        case SIGHUP:  sigName = "SIGHUP";  break;
        case SIGINT:  sigName = "SIGINT";  break;
        case SIGQUIT: sigName = "SIGQUIT"; break;
        case SIGTRAP: sigName = "SIGTRAP"; printStack = true; break;
        case SIGABRT: sigName = "SIGABRT"; printStack = true; break;
        case SIGBUS:  sigName = "SIGBUS";  printStack = true; break;
        case SIGFPE:  sigName = "SIGFPE";  printStack = true; break;
        case SIGSEGV: sigName = "SIGSEGV"; printStack = true; break;
        case SIGTERM: sigName = "SIGTERM"; break;
        default:
            break;
    }

    Printf(Tee::PriNormal, "%s\n", sigName);

    if (printStack && Tee::GetAssertInfoSink())
    {
        Tee::GetAssertInfoSink()->Dump(Tee::PriNormal);
    }

    // Ensure all VF's are shutdown
    Platform::Cleanup(Platform::MininumCleanup);

    // Cleanup shared memory
    SharedSysmem::CleanupOnCrash();

    // Print stack on some signals
    if (printStack)
    {
        PrintStack();
    }

    const INT32 finalRc = Log::FirstError();
    Mle::Print(Mle::Entry::mods_end).rc(finalRc ? finalRc : RC::ASSERT_FAILURE_DETECTED);

    // Flush output
    Tee::FlushToDisk();
}

//------------------------------------------------------------------------------
// Print call stack
//
void PrintStack()
{
    char buf[4096];
    const char stack_file[] = "gdb.txt";
    const char script_file[] = "cmd.txt";

    // Create gdb script
    if (!Platform::DumpCallStack())
    {
        Printf(Tee::PriNormal, "***Call stack is disabled***\n");
        return;
    }

    DownloadSymbols();

    FileHolder file;
    if (OK != file.Open(script_file, "w"))
    {
        Printf(Tee::PriWarn,
               "Can not create command file; stack is not available\n");
        return;
    }
#ifdef SIM_BUILD
    fprintf(file.GetFile(), "set auto-solib-add %s\n",
            Platform::LoadSimSymbols() ? "on" : "off");
#endif
    fprintf(file.GetFile(), "file %s\n", s_ExeName);
    fprintf(file.GetFile(), "attach %d\n", getpid());
#ifdef SIM_BUILD
    fprintf(file.GetFile(), "sharedlibrary librm\n");
#endif
    fprintf(file.GetFile(), "sharedlibrary libc\n");
#ifdef USE_PTHREADS
    fprintf(file.GetFile(), "thread apply all bt\n");
#else
    fprintf(file.GetFile(), "bt\n");
#endif
    fprintf(file.GetFile(), "quit\n");
    file.Close();

    printf("Dumping call stack(s) with GDB...\n");

    snprintf(buf, sizeof(buf), "gdb -q -x %s > %s 2>&1", script_file, stack_file);

    // Attach gdb to the current process and execute script_file
    if (0 != system(buf))
    {
        Printf(Tee::PriWarn, "Unable to execute gdb\n");
        return;
    }

    // Dump call stack into the log
    if (OK != file.Open(stack_file, "r"))
    {
        Printf(Tee::PriWarn, "Stack is not available\n");
        return;
    }

    Printf(Tee::PriNormal, "----------call stack----------\n");
    for (const char* temp = fgets(buf, sizeof(buf), file.GetFile());
         temp;
         temp = fgets(buf, sizeof(buf), file.GetFile()))
    {
        // ignore line unless it starts w/ a hash mark or a space,
        // actual gdb stack output or it tells the thread for which
        // the trace is dumped
        if ((buf[0] != '#') &&
            (buf[0] != ' ') &&
            (strncmp(buf, "Thread", 6) != 0) &&
            (buf[0] != '\n'))
        {
             continue;
        }
        Printf(Tee::PriNormal, "%s", buf);
    }
    Printf(Tee::PriNormal, "----------call stack----------\n");
    file.Close();

    Xp::EraseFile(stack_file);
    Xp::EraseFile(script_file);
}
#endif // SUPPORT_SIGNALS

/**
 *-----------------------------------------------------------------------------
 * @function(SwitchSystemPowerState)
 *
 * Lwrrently stubbed out, returns error
 *-----------------------------------------------------------------------------
 */
RC Xp::SwitchSystemPowerState(bool bPowerState, INT32 iSleepTimeInSec)
{
    MASSERT(!"Break in Xp::SwitchSystemPowerState, Unsupported yet on LINUX");
    return RC::SOFTWARE_ERROR;
}

/**
 *-----------------------------------------------------------------------------
 * @function(GetOsDisplaySettingsBufferSize)
 *
 * Lwrrently stubbed out, returns error
 *-----------------------------------------------------------------------------
 */
UINT32 Xp::GetOsDisplaySettingsBufferSize()
{
    MASSERT(
        !"Break in Xp::GetOsDisplaySettingsBufferSize, Unsupported yet on LINUX"
           );
    return 0;
}
/**
 *-----------------------------------------------------------------------------
 * @function(GetDisplaySettings)
 *
 * Lwrrently stubbed out, returns error
 *-----------------------------------------------------------------------------
 */
RC Xp::SaveOsDisplaySettings(void *pOsDisplaySettingsBuffer)
{
    MASSERT(!"Break in Xp::GetDisplaySettings, Unsupported yet on LINUX");
    return RC::SOFTWARE_ERROR;
}

/**
 *-----------------------------------------------------------------------------
 * @function(SetDisplaySettings)
 *
 * Lwrrently stubbed out, returns error
 *-----------------------------------------------------------------------------
 */
RC Xp::RestoreOsDisplaySettings(void *pOsDisplaySettingsBuffer)
{
    MASSERT(!"Break in Xp::SetDisplaySettings, Unsupported yet on LINUX");
    return RC::SOFTWARE_ERROR;
}

//------------------------------------------------------------------------------
void * Xp::GetVbiosFromLws(UINT32 GpuInst)
{
    return NULL;
}

RC Xp::ReadChipId(UINT32 * fuseVal240, UINT32 * fuseVal244)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
// Determine if running in Hypervisor.
//
bool Xp::IsInHypervisor()
{
    return s_bHypervisor;
}

//------------------------------------------------------------------------------
P_(Global_Get_LinuxArch);
static SProperty Global_LinuxArch
(
    "LinuxArch",
    0,
    Global_Get_LinuxArch,
    0,
    0,
    "Linux architecture."
);

P_(Global_Get_LinuxArch)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

#if defined(LWCPU_ARM)
    const string Arch = "arm";
#elif defined(PPC64LE)
    const string Arch = "ppc64le";
#elif defined(LWCPU_AARCH64)
    const string Arch = "aarch64";
#else
    string Arch = "x86";
    if (sizeof(void*) == 8)
    {
        Arch = "x86_64";
    }
#endif
    if (OK != JavaScriptPtr()->ToJsval(Arch, pValue))
    {
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

//------------------------------------------------------------------------------
P_(Global_Get_LinuxKernelVer);
static SProperty Global_LinuxKernelVer
(
    "LinuxKernelVer",
    0,
    Global_Get_LinuxKernelVer,
    0,
    0,
    "Linux kernel version."
);

P_(Global_Get_LinuxKernelVer)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    string release = "N/A";

    utsname name;
    if (0 == uname(&name))
    {
        release = name.release;
    }

    if (OK != JavaScriptPtr()->ToJsval(release, pValue))
    {
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

//------------------------------------------------------------------------------
P_(Global_Get_LinuxHostName);
static SProperty Global_LinuxHostName
(
    "LinuxHostName",
    0,
    Global_Get_LinuxHostName,
    0,
    0,
    "Host name."
);

P_(Global_Get_LinuxHostName)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    char buf[128];
    if ((0 != gethostname(buf, sizeof(buf))) ||
        (JavaScriptPtr()->ToJsval(buf, pValue) != RC::OK))
    {
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

//------------------------------------------------------------------------------
P_(Global_Get_SystemRAMStats);
static SProperty Global_SystemRAMStats
(
    "SystemRAMStats",
    0,
    Global_Get_SystemRAMStats,
    0,
    0,
    "System RAM statistics."
);

P_(Global_Get_SystemRAMStats)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

#ifdef QNX
    // This API is not available on QNX, fake it and return zeroes
    struct
    {
        unsigned long freeram;
        unsigned long totalram;
    } info = { };
#else
    struct sysinfo info = { };
    sysinfo(&info);
#endif

    JSObject* pObj = JS_NewObject(pContext, nullptr, nullptr, nullptr);
    jsval freeVal;
    jsval sizeVal;

    if (pObj &&
        JS_NewNumberValue(pContext, info.freeram, &freeVal) &&
        JS_DefineProperty(pContext, pObj, "free", freeVal, nullptr, nullptr, JSPROP_ENUMERATE) &&
        JS_NewNumberValue(pContext, info.totalram, &sizeVal) &&
        JS_DefineProperty(pContext, pObj, "size", sizeVal, nullptr, nullptr, JSPROP_ENUMERATE))
    {
        *pValue = OBJECT_TO_JSVAL(pObj);
        return JS_TRUE;
    }

    *pValue = JSVAL_NULL;
    return JS_FALSE;
}

//------------------------------------------------------------------------------
P_(Global_Get_KernelLogPriority);
P_(Global_Set_KernelLogPriority);
static SProperty Global_KernelLogPriority
(
    "KernelLogPriority",
    0,
    Global_Get_KernelLogPriority,
    Global_Set_KernelLogPriority,
    0,
    "Priority at which kernel log is printed on exit."
);

P_(Global_Get_KernelLogPriority)
{
    MASSERT(pContext != nullptr);
    MASSERT(pValue   != nullptr);

    if (JavaScriptPtr()->ToJsval(s_KernelLogPriority.load(), pValue) != RC::OK)
    {
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

P_(Global_Set_KernelLogPriority)
{
    MASSERT(pContext != nullptr);
    MASSERT(pValue   != nullptr);

    INT32 pri = Tee::PriNone;
    JavaScriptPtr pJs;
    const RC rc = pJs->FromJsval(*pValue, &pri);
    if (rc != RC::OK)
    {
        ErrorMap em(rc);
        pJs->Throw(pContext, rc, em.Message());
        return JS_FALSE;
    }

    s_KernelLogPriority.store(pri);

    return JS_TRUE;
}

/**
 *-----------------------------------------------------------------------------
 * @function(GetVirtualMemoryUsed)
 *-----------------------------------------------------------------------------
 */
size_t Xp::GetVirtualMemoryUsed()
{
    const char *name = "/proc/self/status";
    FileHolder file;
    RC rc = file.Open(name, "rb");
    if (OK != rc)
    {
        ErrorMap em(rc);
        Printf(Tee::PriError, "%s can't open %s: %d %s.\n",
               __FUNCTION__, name, rc.Get(), em.Message());
        MASSERT(!"Getting virtual memory used failed.");
        return 0;
    }

    size_t vm = 0;
    char line[128];

    while (fgets(line, 128, file.GetFile()) != NULL)
    {
        if (strncmp(line, "VmSize:", 7) == 0)
        {
            char *p = line;
            while (!isdigit(*p))
            {
                p++;
            }
            char *temp;
            vm = strtoul(p, &temp, 0);
            if (temp == p)
            {
                Printf(Tee::PriError, "%s can't parse token %s.\n",
                       __FUNCTION__, p);
                MASSERT(!"Getting virtual memory used failed.");
            }
            break;
        }
    }
    return vm * 1024;
}

//------------------------------------------------------------------------------
RC Xp::GetCarveout(PHYSADDR* pPhysAddr, UINT64* pSize)
{
    return RC::UNSUPPORTED_FUNCTION;
}

#if defined(INCLUDE_GPU) && !defined(PPC64LE)
//------------------------------------------------------------------------------
// Wrapper class for Jefferson Technology protocol for GC6
class LinuxJTMgr : public CommonJTMgr
{
public:
    LinuxJTMgr(UINT32 GpuInstance) : CommonJTMgr(GpuInstance) { }
    RC RTD3PowerCycle(Xp::PowerState state) override;
protected:
    virtual RC DiscoverACPI();
    virtual RC DiscoverNativeACPI();
    virtual RC DSM_Eval_ACPI
    (
        UINT16 domain,
        UINT16 bus,
        UINT08 dev,
        UINT08 func,
        UINT32 acpiFunc,
        UINT32 acpiSubFunc,
        UINT32 *pInOut,
        UINT16 *pSize
    );
private:
    UINT32          m_RPDomain   = 0;
    UINT32          m_RPBus      = 0;
    UINT32          m_RPDevice   = 0;
    UINT32          m_RPFunction = 0;
};

//--------------------------------------------------------------------------
// Wrapper class to manage one JTMgr instance per GpuInstance.
class LinuxJTMgrs : public CommonJTMgrs
{
public:
    LinuxJTMgrs() { }

    Xp::JTMgr* Get(UINT32 GpuInstance)
    {
    #if defined(INCLUDE_GPU) && !defined(PPC64LE)
        if (m_JTMgrs.count(GpuInstance) == 0)
        {
            m_JTMgrs[GpuInstance] = new LinuxJTMgr(GpuInstance);
            m_JTMgrs[GpuInstance]->Initialize();
        }
        return m_JTMgrs[GpuInstance];
    #else
        return nullptr;
    #endif
    }
};
LinuxJTMgrs JTMgrs;

RC LinuxJTMgr::RTD3PowerCycle(Xp::PowerState state)
{
    StickyRC rc;
    if (m_SMBusSupport)
    {
        switch (state)
        {
            case Xp::PowerState::PowerOff:
                return m_SmbEc.EnterRTD3Power();
            case Xp::PowerState::PowerOn:
                return m_SmbEc.ExitRTD3Power();
            default:
                return RC::SOFTWARE_ERROR;
        }
    }
    else if (m_NativeACPISupport)
    {
        Xp::LinuxInternal::AcpiPowerMethod method;
        switch (state)
        {
            case Xp::PowerState::PowerOff:
                method = Xp::LinuxInternal::AcpiPowerMethod::AcpiOff;
                break;
            case Xp::PowerState::PowerOn:
                method = Xp::LinuxInternal::AcpiPowerMethod::AcpiOn;
                break;
            default:
                return RC::SOFTWARE_ERROR;
        }
        const auto startTime = Xp::GetWallTimeMS();
        rc = Xp::LinuxInternal::AcpiRootPortEval(
            m_RPDomain, m_RPBus, m_RPDevice, m_RPFunction, method, nullptr);
        const auto totalTime = Xp::GetWallTimeMS() - startTime;

        if (state == Xp::PowerState::PowerOff)
        {
            m_ACPIStats.entryTimeMs = totalTime;
            m_ACPIStats.entryStatus = rc;
            m_ACPIStats.entryPwrStatus = 0;
        }
        else if (state == Xp::PowerState::PowerOn)
        {
            m_ACPIStats.exitTimeMs = totalTime;
            m_ACPIStats.exitStatus = rc;
            m_ACPIStats.exitPwrStatus = 0;
        }
        return rc;
    }
    return RC::SOFTWARE_ERROR;
}

//------------------------------------------------------------------------------
RC LinuxJTMgr::DSM_Eval_ACPI
(
    UINT16 domain,
    UINT16 bus,
    UINT08 dev,
    UINT08 func,
    UINT32 acpiFunc,
    UINT32 acpiSubFunc,
    UINT32 *pInOut,
    UINT16 *pSize
)
{
    return Xp::LinuxInternal::AcpiDsmEval(
                domain, bus, dev, func,
                acpiFunc,
                acpiSubFunc,
                pInOut,
                pSize);
}

//------------------------------------------------------------------------------
// Determine if ACPI is supported on this Gpu.
RC LinuxJTMgr::DiscoverACPI()
{
    RC rc;
    UINT32 rtlwalue = 0;
    m_JTACPISupport = false;
    UINT16 size = sizeof(rtlwalue);

    if ((m_Domain > 0xFFFFU) ||
        (m_Bus > 0xFFU) ||
        (m_Device > 0xFFU) ||
        (m_Function > 0xFFU))
    {
        Printf(Tee::PriLow,
               "Invalid PCI device passed to LinuxJTMgr::DiscoverACPI - %04x:%x:%02x.%x\n",
                m_Domain, m_Bus, m_Device, m_Function);
        return RC::UNSUPPORTED_FUNCTION;
    }

    CHECK_RC(Xp::LinuxInternal::AcpiDsmEval(
                static_cast<UINT16>(m_Domain),
                static_cast<UINT16>(m_Bus),
                static_cast<UINT08>(m_Device),
                static_cast<UINT08>(m_Function),
                MODS_DRV_CALL_ACPI_GENERIC_DSM_JT,
                JT_FUNC_CAPS,
                &rtlwalue,
                &size));

    // We need all 4 bytes to determine the full JT capabilities. If
    // the implementor is returning less than 4 they can't possibly be
    // supporting JT_FUNC.
    if (size == sizeof(UINT32))
    {
        if ((rtlwalue >= LWHG_ERROR_UNSPECIFIED) &&
            (rtlwalue <= LWHG_ERROR_PARM_ILWALID))
        {
            return RC::UNSUPPORTED_FUNCTION;
        }
        // data is valid for inquiry.
        if (LW_JT_FUNC_CAPS_JT_ENABLED_TRUE !=
            DRF_VAL(_JT, _FUNC_CAPS, _JT_ENABLED, rtlwalue))
        {
            return RC::UNSUPPORTED_FUNCTION;
        }
        else
        {
            m_Caps = rtlwalue;
            m_JTACPISupport = true;
            Printf(Tee::PriLow, "ACPI JT capabilities: 0x%x\n", m_Caps);
        }
    }
    else
        return RC::SOFTWARE_ERROR;

    return rc;
}

RC LinuxJTMgr::DiscoverNativeACPI()
{
    RC rc;
    PexDevice *pPexDev = nullptr;
    UINT32 port;

    auto *pcie = m_pSubdev->GetInterface<Pcie>();
    if (pcie)
    {
        pcie->GetUpStreamInfo(&pPexDev, &port);
    }
    if (pPexDev == nullptr)
    {
        return rc;
    }

    m_RPDomain = pPexDev->GetDownStreamPort(port)->Domain;
    m_RPBus = pPexDev->GetDownStreamPort(port)->Bus;
    m_RPDevice = pPexDev->GetDownStreamPort(port)->Dev;
    m_RPFunction = pPexDev->GetDownStreamPort(port)->Func;
    UINT32 caps = 0;

    CHECK_RC(Xp::LinuxInternal::AcpiRootPortEval(
        m_RPDomain, m_RPBus, m_RPDevice, m_RPFunction,
        Xp::LinuxInternal::AcpiPowerMethod::AcpiStatus, &caps));
    m_NativeACPISupport = (caps == 1);

    return rc;
}
#endif

//------------------------------------------------------------------------------
Xp::JTMgr* Xp::GetJTMgr(UINT32 GpuInstance)
{
#if defined(INCLUDE_GPU) && !defined(PPC64LE)
    return JTMgrs.Get(GpuInstance);
#else
    return nullptr;
#endif
}

void Xp::CleanupJTMgr()
{
#if defined(INCLUDE_GPU) && !defined(PPC64LE)
    return JTMgrs.Cleanup();
#else
    return;
#endif
}

//-----------------------------------------------------------------------------
namespace
{
    string ReadAndCloseFile(int fd)
    {
        string buf;
        ssize_t numRead = 0;
        do
        {
            const size_t resize = 1024;
            const size_t pos = buf.size();
            buf.resize(pos+resize);
            numRead = read(fd, &buf[pos], resize);
            buf.resize(pos+((numRead > 0) ? numRead : 0));
        }
        while (numRead > 0);
        close(fd);
        return buf;
    }

    RC ReadCpuInfo(const string& lineHeader, string* pValue)
    {
        MASSERT(pValue);
        *pValue = "";

        const int fd = open("/proc/cpuinfo", O_RDONLY);
        if (fd < 0)
        {
            return RC::CPUID_NOT_SUPPORTED;
        }

        const string cpuinfo = ReadAndCloseFile(fd);

        size_t pos = 0;
        while (pos < cpuinfo.size())
        {
            // Read serial number
            if (cpuinfo.substr(pos, lineHeader.size()) == lineHeader)
            {
                Printf(Tee::PriDebug, "Found line %s in /proc/cpuinfo\n",
                        lineHeader.c_str());

                // Skip past colon
                while (cpuinfo[pos] != ':')
                {
                    pos++;
                    if (pos == cpuinfo.size())
                    {
                        Printf(Tee::PriDebug, "Colon not found!\n");
                        return RC::CPUID_NOT_SUPPORTED;
                    }
                }
                pos++;

                // Skip spaces
                while (cpuinfo[pos] == ' ' && pos < cpuinfo.size())
                {
                    pos++;
                }

                // Find end of line
                size_t endPos = pos;
                while (endPos < cpuinfo.size() &&
                       cpuinfo[endPos] != '\n')
                {
                    endPos++;
                }

                // Remove trailing spaces
                while (pos+1 < endPos && cpuinfo[endPos-1] == ' ')
                {
                    endPos--;
                }

                if (endPos <= pos)
                {
                    Printf(Tee::PriDebug, "No value - only spaces!\n");
                    return RC::CPUID_NOT_SUPPORTED;
                }

                *pValue = cpuinfo.substr(pos, endPos-pos);
                Printf(Tee::PriDebug, "Read line %s from /proc/cpuinfo: %s\n",
                        lineHeader.c_str(), pValue->c_str());
                return OK;
            }

            // Go to next line
            while (pos < cpuinfo.size() && cpuinfo[pos] != '\n')
            {
                pos++;
            }
            if (pos < cpuinfo.size())
            {
                pos++;
            }
        }

        Printf(Tee::PriDebug, "Line %s not found in /proc/cpuinfo\n",
                lineHeader.c_str());
        return RC::CPUID_NOT_SUPPORTED;
    }

    void RemoveNULs(string* pStr)
    {
        auto pos = pStr->find('\0');
        while (pos != string::npos)
        {
            pStr->erase(pos, 1);
            pos = pStr->find('\0', pos);
        }
    }
}

//-----------------------------------------------------------------------------
RC Xp::GetBoardInfo(BoardInfo* pBoardInfo)
{
    MASSERT(pBoardInfo);

    if (!Platform::IsTegra())
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    RC rc;
    if (OK != ReadCpuInfo("Hardware", &pBoardInfo->boardName))
    {
        const int fd = open("/proc/device-tree/model", O_RDONLY);
        if (fd < 0)
        {
            Printf(Tee::PriError, "Failed to read /proc/device-tree/model\n");
            return RC::CPUID_NOT_SUPPORTED;
        }

        pBoardInfo->boardName = ReadAndCloseFile(fd);

        RemoveNULs(&pBoardInfo->boardName);
    }

    if (OK == ReadCpuInfo("Serial", &pBoardInfo->boardSerial))
    {
        if (pBoardInfo->boardSerial.size() < 16)
        {
            Printf(Tee::PriError,
                   "Failed to parse Serial from /proc/cpuinfo, serial too short: '%s'\n",
                   pBoardInfo->boardSerial.c_str());
            return RC::CPUID_NOT_SUPPORTED;
        }
        if (1 != sscanf(pBoardInfo->boardSerial.substr(0, 4).c_str(), "%x",
                    &pBoardInfo->boardId))
        {
            Printf(Tee::PriError,
                   "Failed to parse Serial from /proc/cpuinfo: '%s'\n",
                   pBoardInfo->boardSerial.c_str());
            return RC::CPUID_NOT_SUPPORTED;
        }
    }
    else
    {
        if (Xp::DoesFileExist("/proc/device-tree/lwpu,proc-boardid"))
        {
            const int fd = open("/proc/device-tree/lwpu,proc-boardid", O_RDONLY);
            if (fd < 0)
            {
                Printf(Tee::PriError, "Failed to read /proc/device-tree/lwpu,proc-boardid\n");
                return RC::CPUID_NOT_SUPPORTED;
            }

            string serial = ReadAndCloseFile(fd);
            //p2379:0000:C01 Remove the first character if it is not number.
            if (!(serial[0] >= '0' && serial[0] <= '9' ))
            {
                serial.erase(0, 1);
            }

            if (serial.size() < 9 || serial[4] != ':')
            {
                Printf(Tee::PriError, "Failed to parse /proc/device-tree/lwpu,proc-boardid\n");
                return RC::CPUID_NOT_SUPPORTED;
            }

            if (1 != sscanf(serial.c_str(), "%u", &pBoardInfo->boardId))
            {
                Printf(Tee::PriError, "Failed to parse /proc/device-tree/lwpu,proc-boardid\n");
                return RC::CPUID_NOT_SUPPORTED;
            }

            pBoardInfo->boardSerial = serial;

        }
        else if (Xp::DoesFileExist("/proc/device-tree/chosen/plugin-manager/cvm"))
        {
            const int fd = open("/proc/device-tree/chosen/plugin-manager/cvm", O_RDONLY);
            if (fd < 0)
            {
                Printf(Tee::PriError, "Failed to read /proc/device-tree/chosen/plugin-manager/cvm\n");
                return RC::CPUID_NOT_SUPPORTED;
            }

            string serial = ReadAndCloseFile(fd);
            //p2379:0000:C01 Remove the first character if it is not number.
            if (!(serial[0] >= '0' && serial[0] <= '9' ))
            {
                serial.erase(0, 1);
            }

            if (serial.size() < 9 || serial[4] != '-')
            {
                Printf(Tee::PriError, "Failed to parse /proc/device-tree/chosen/plugin-manager/cvm\n");
                return RC::CPUID_NOT_SUPPORTED;
            }

            pBoardInfo->boardId = Utility::Strtoull(serial.c_str(), nullptr, 10);
            pBoardInfo->boardSerial = serial;
        }
        else if (Xp::DoesFileExist("/proc/device-tree/chosen/proc-board/id"))
        {
            const int fd = open("/proc/device-tree/chosen/proc-board/id", O_RDONLY);
            if (fd < 0)
            {
                Printf(Tee::PriError, "Failed to read /proc/device-tree/chosen/proc-board/id\n");
                return RC::CPUID_NOT_SUPPORTED;
            }

            string boardid = ReadAndCloseFile(fd);
            if (boardid.length() < 4)
            {
                Printf(Tee::PriError, "Failed to parse /proc/device-tree/chosen/proc-board/id\n");
                return RC::CPUID_NOT_SUPPORTED;
            }

            // the first two chars will always be 0
            pBoardInfo->boardId = static_cast<UINT32>(boardid[2] << 8) |
                                  static_cast<UINT32>(boardid[3]);
        }
        else
        {
            string boardInfo;
            string filesRead;

            static const char* const filesWithBoardName[] =
            {
                "/proc/device-tree/compatible",
                "/proc/device-tree/model"
            };

            for (const char* filename : filesWithBoardName)
            {
                if (Xp::DoesFileExist(filename))
                {
                    const int fd = open(filename, O_RDONLY);
                    if (fd < 0)
                    {
                        Printf(Tee::PriWarn, "Failed to open %s\n", filename);
                    }
                    else
                    {
                        boardInfo += ReadAndCloseFile(fd);

                        if (filesRead.size())
                        {
                            filesRead += ", ";
                        }
                        filesRead += filename;
                    }
                }
            }

            if (boardInfo.size() == 0)
            {
                Printf(Tee::PriError, "Found no way to identify the board\n");
                return RC::CPUID_NOT_SUPPORTED;
            }

            size_t pos;
            do
            {
                pos = boardInfo.find('\0');
                if (pos != string::npos)
                {
                    boardInfo[pos] = ' ';
                }
            } while (pos != string::npos);

            Printf(Tee::PriDebug, "%s: '%s'\n", filesRead.c_str(), boardInfo.c_str());

            // Example:
            // lwpu,tegra234\0lwidia,e2421-1099+e2425-1099\0lwidia,e2421-1099

            std::regex  reBoardId("\\b[ep](\\d{4})\\b", std::regex::ECMAScript);
            std::smatch match;
            if (std::regex_search(boardInfo, match, reBoardId) && (match.size() == 2))
            {
                const string boardIdStr = match.str(1);
                Printf(Tee::PriDebug, "Found '%s', at position %u\n",
                       boardIdStr.c_str(), (unsigned)match.position(1));
                pBoardInfo->boardId = static_cast<UINT32>(
                        Utility::Strtoull(boardIdStr.c_str(), nullptr, 10));
            }
            else
            {
                Printf(Tee::PriError, "Failed to parse %s: '%s'\n", filesRead.c_str(), boardInfo.c_str());
                return RC::CPUID_NOT_SUPPORTED;
            }
        }
    }

    RemoveNULs(&pBoardInfo->boardSerial);

    CHECK_RC(ReadCpuInfo("CPU architecture", &pBoardInfo->cpuArchitecture));
    pBoardInfo->cpuId =
        static_cast<UINT32>(strtoul(pBoardInfo->cpuArchitecture.c_str(), 0, 0));
    return OK;
}

//------------------------------------------------------------------------------
#ifndef LINUX_MFG
RC Xp::GetAtsTargetAddressRange
(
    UINT32  gpuInst,
    UINT32* pAddr,
    UINT32* pGuestAddr,
    UINT32* pAddrWidth,
    UINT32* pMask,
    UINT32* pMaskWidth,
    UINT32* pGranularity,
    bool    bIsPeer,
    UINT32  peerIndex
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
RC Xp::GetLwLinkLineRate
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func,
    UINT32 *speed
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::GetDriverStats(DriverStats *pDriverStats)
{
    return RC::UNSUPPORTED_FUNCTION;
}

#endif

static FLOAT64 GetTimeSec(const struct timeval& tv)
{
    return static_cast<FLOAT64>(tv.tv_sec) +
           static_cast<FLOAT64>(tv.tv_usec) / 1'000'000.0;
}

//------------------------------------------------------------------------------
FLOAT64 Xp::GetCpuUsageSec()
{
    static FLOAT64 lastValue = 0;
    struct rusage rusage;
    if (getrusage(RUSAGE_SELF, &rusage))
    {
        return lastValue;
    }

    const FLOAT64 userTimeSec   = GetTimeSec(rusage.ru_utime);
    const FLOAT64 kernelTimeSec = GetTimeSec(rusage.ru_stime);

    Printf(Tee::PriDebug,
           "CPU time elapsed: user %.3fs, kernel %.3fs\n",
           userTimeSec,
           kernelTimeSec);
    Printf(Tee::PriDebug, "Max RSS: %ld KB\n", rusage.ru_maxrss);
    Printf(Tee::PriDebug,
           "Page faults: IO %ld, non-IO %ld\n", rusage.ru_majflt, rusage.ru_minflt);
    Printf(Tee::PriDebug,
           "Context switches: voluntary %ld, priority %ld\n",
           rusage.ru_lwcsw, rusage.ru_nivcsw);

    const FLOAT64 totalTimeSec = userTimeSec + kernelTimeSec;

    lastValue = totalTimeSec;

    return totalTimeSec;
}

UINT32 Xp::GetNumCpuCores()
{
    return static_cast<UINT32>(sysconf(_SC_NPROCESSORS_ONLN));
}
