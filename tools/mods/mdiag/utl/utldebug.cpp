/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/utility.h"
#include "core/include/platform.h"
#include "mdiag/sysspec.h"
#include "utldebug.h"
#include "utlevent.h"
#include "utlutil.h"

#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>

namespace UtlHangDebug
{
    using sighandler =  void (*)(int);
    static sighandler s_signals[32];

    const static UINT32 s_ilwalidSig = -1;
    static UINT32 s_trappedSig = s_ilwalidSig;

    static bool s_IsInitialized = false;

    void UtlHangErrorDiscover(int sig)
    {
        InfoPrintf("UTL: UtlHangErrorDiscover traps signal = %d\n", sig);
        DEFER
        {
            // if signal is not cleared, re-send the signal
            if (s_trappedSig != s_ilwalidSig)
            {
                signal(s_trappedSig, s_signals[s_trappedSig]);
                kill(getpid(), s_trappedSig);
            }
        };
        s_trappedSig = sig;
        UtlHangEvent event;
        event.Trigger();
    }

    void Init()
    {
        if (!s_IsInitialized)
        {
            s_signals[SIGXCPU] = signal(SIGXCPU, UtlHangErrorDiscover);
            s_signals[SIGXFSZ] = signal(SIGXFSZ, UtlHangErrorDiscover);
            s_signals[SIGUSR1] = signal(SIGUSR1, UtlHangErrorDiscover);
            s_signals[SIGUSR2] = signal(SIGUSR2, UtlHangErrorDiscover);
            s_IsInitialized = true;
        }
    }
}

void UtlDebug::Register(py::module module)
{
    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
    kwargs->RegisterKwarg("Utl.Terminate", "printCallStack", true, "a boolean value representing if call stack will be printed");
    kwargs->RegisterKwarg("Utl.Terminate", "flushTestResult", false, "a boolean value representing if test result will be dump into trep.txt");
    kwargs->RegisterKwarg("Utl.Terminate", "forceExitNormally", false, "a boolean value representing if test exits normally(with 0 return value)");

    module.def("Terminate", &Terminate,
        UTL_DOCSTRING("Utl.Terminate", "This function will terminate process."));
}

void UtlDebug::Init()
{
    UtlHangDebug::Init();
}

void UtlDebug::Terminate(py::kwargs kwargs)
{
    using namespace UtlHangDebug;

    bool printCallStack = UtlUtility::GetRequiredKwarg<bool>(kwargs, "printCallStack", "Utl.Terminate");
    bool flushTestResult = false;
    bool forceExitNormally = false;
    UtlUtility::GetOptionalKwarg<bool>(&flushTestResult, kwargs, "flushTestResult", "Utl.Terminate");
    UtlUtility::GetOptionalKwarg<bool>(&forceExitNormally, kwargs, "forceExitNormally", "Utl.Terminate");

    if (flushTestResult)
    {
        Utl::Instance()->ReportTestResults();
    }

    DEFER
    {
        // since the process will be ternimated, we don't need to resend the original signal
        s_trappedSig = s_ilwalidSig;
    };

    // disable core dump
    struct rlimit rlim;
    rlim.rlim_lwr = rlim.rlim_max = 0;
    if (setrlimit(RLIMIT_CORE, &rlim) < 0)
    {
        WarnPrintf("UtlDebug::Terminate: Failed to disable core dump: %s\n", strerror(errno));
    }

    InfoPrintf("Utl.Terminate is called, ending now.\n");
    if (forceExitNormally)
    {
        WarnPrintf("Forcing to exit normally without any callstack.\n");
        _exit(0);
    }
    else if (printCallStack)
    {
        // Send SIGABRT and let MODS exit w/ callstack dumping
        kill(getpid(), SIGABRT);
    }
    else
    {
        kill(getpid(), SIGQUIT);
    }
}

