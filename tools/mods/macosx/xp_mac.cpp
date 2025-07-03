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

/**
 * @file  xp.cpp
 * @brief Mac OSX implementation of XP functions.
 *
 * @todo Major cleanup/bugfixes.
 */

#include "core/include/cnslmgr.h"
#include "core/include/console.h"
#include "core/include/cpu.h"
#include "core/include/evntthrd.h"
#include "core/include/filesink.h"
#include "core/include/globals.h"
#include "core/include/massert.h"
#include "core/include/memerror.h"
#include "ndrv.h"
#include "lwdiagutils.h"
#include "core/include/platform.h"
#include "core/include/pool.h"
#include "core/include/tasker.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include "core/include/pci.h"
#include "core/include/dllhelper.h"
#include "core/include/cmdline.h"
#include <algorithm>
#include <cstring> // memcpy
#include <deque>
#include <dlfcn.h>
#include <mach/mach_time.h> // mach_absolute_time
#include <nlwrses.h>
// lwrses.h defines OK and now that MODS OK is a constexpr instead of a define
// it will be used over MODS OK and also prevent inclusion of any header file
// that references RC::OK because it causes a compilation failure 
#ifdef OK
    #undef OK
#endif
#include <pwd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifdef INCLUDE_OGL
#include "opengl/modsgl.h"
#endif

//Javascript
#include "core/include/jscript.h"
#include "core/include/script.h"

//Socket
#include "core/include/sockmods.h"
#include "core/include/sockansi.h"
#include "core/include/remotesink.h"
#include "core/include/telnutil.h"

#define CFNETWORK_NEW_API
extern "C" {
#include <ApplicationServices/ApplicationServices.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <CoreServices/CoreServices.h>
#include <time.h>
}

#ifdef INCLUDE_GPU
#include "core/include/display.h"
#include "core/include/lwrm.h"
#include "lwos.h"
#include "Lwcm.h"
#include "core/include/gpu.h"
#include "gpu/include/gpumgr.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/perf/perfsub.h"
#if !defined(MACOSX_MFG)
#include <Carbon/Carbon.h>
#endif
#include "lwRmApi.h"
#endif

#include "core/include/memcheck.h"

#if defined(MACOSX_MFG)
#include "libmods.h"
#endif

namespace
{
#if defined(INCLUDE_GPU)
    // Boolean indicating whether we are in Windowed mode or
    //  console mode
    bool s_ConsoleMode = false;
#endif

    // Output stream for console-based I/O, plus other overhead
    bool             s_bRemoteConsole = false;
    RemoteDataSink * s_pRemoteSink = 0;
    SocketAnsi     * s_pRemoteSocket = 0;
    SocketMods     * s_pBaseSocket = 0;

    // Generic Socket support
    vector<SocketMods *>  * s_pOsSockets = 0;

    // Make sure we always un-stepaside
    bool   s_DoStepAside               = true;

#ifdef INCLUDE_GPU
    const UINT32 MAX_DISPLAYS = 32U;

#if !defined(MACOSX_MFG)
    bool   s_AppleSteppedAside     = false;
    bool   s_NdrvAttached          = false;
    bool   s_CoreChannelReleased   = false;
    UINT32 s_oldDisplayMask[LW0080_MAX_DEVICES] = {0};
#endif

    IOPMAssertionID s_DisplayAssertionID; // Used to keep display awake
    IOPMAssertionID s_SystemAssertionID;  // Used to keep system awake

    struct AppleDisplay
    {
        CGDirectDisplayID displayId;
        CGDisplayModeRef    originalMode;
    };
    typedef vector<AppleDisplay> AppleDisplays;
    AppleDisplays s_appleDisplays;

#endif // INCLUDE_GPU
} // namespace

#ifdef INCLUDE_GPU
RC DisableMalwserInterface();
RC EnableMalwserInterface();
#endif

// This is just like CHECK_RC, but for CG commands (Apple Quartz Core
// Graphics).  returlwalueForCGFailure must be a local variable.
//
#define CHECK_CG(cgCmd)                                          \
    do {                                                         \
        CGError cgErr = cgCmd;                                   \
        if (cgErr != kCGErrorSuccess) {                          \
            Printf(Tee::PriHigh, "ERROR: %s failed, err = %d\n", \
                   #cgCmd, (int)cgErr);                          \
            return returlwalueForCGFailure;                      \
        }                                                        \
    } while (0)

// Use these workarounds if needed. All WARs are disabled lwrrently.
UINT32 s_AppleWAR               = 0;

#if defined(MACOSX_MFG) && defined(INCLUDE_GPU)
extern RC InitializeKernelCalls();
extern RC FinalizeKernelCalls();
#endif

//----------------------------------------------------------
//----------------------------------------------------------
//
// OSX Specific Java variables
//

P_(Global_Get_StepAside);
P_(Global_Set_StepAside);
static SProperty Global_StepAside
(
    "StepAside",
    s_DoStepAside,
    Global_Get_StepAside,
    Global_Set_StepAside,
    0,
    "Enable or disable Apple stepaside code."
);

P_(Global_Get_StepAside)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    if (OK != JavaScriptPtr()->ToJsval(s_DoStepAside, pValue))
    {
        JS_ReportError(pContext, "Failed to get StepAside.");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

P_(Global_Set_StepAside)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    // Colwert the argument.
    UINT32 StepAside;
    if (OK != JavaScriptPtr()->FromJsval(*pValue, &StepAside))
    {
        JS_ReportError(pContext, "Failed to set StepAside.");
        return JS_FALSE;
    }

    s_DoStepAside = StepAside;

    return JS_TRUE;
}

// Bits of workarounds to use
P_(Global_Get_AppleWAR);
static SProperty Global_AppleWAR
(
    "AppleWAR",
    s_AppleWAR,
    Global_Get_AppleWAR,
    0,
    0,
    "Apple workaround bitmask."
);

P_(Global_Get_AppleWAR)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    if (OK != JavaScriptPtr()->ToJsval(s_AppleWAR, pValue))
    {
        JS_ReportError(pContext, "Failed to get AppleWAR.");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

//----------------------------------------------------------
//----------------------------------------------------------

#if !defined(MACOSX_MFG) || !defined(INCLUDE_GPU)

bool Xp::IRQTypeSupported(UINT32 irqType)
{
    switch (irqType)
    {
        case MODS_XP_IRQ_TYPE_INT:
            return true;
        case MODS_XP_IRQ_TYPE_MSI:
            return true;
        case MODS_XP_IRQ_TYPE_CPU:
            return true;
        case MODS_XP_IRQ_TYPE_MSIX:
            return false;
        default:
            Printf(Tee::PriWarn, "Unknown interrupt type %d\n", irqType);
            break;
    }

    return false;
}

void Xp::ProcessPendingInterrupts()
{
}

RC Xp::MapIRQ(UINT32 * logicalIrq, UINT32 physIrq, string dtName, string fullName)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::HookIRQ(UINT32 Irq, ISR func, void* cookie)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::UnhookIRQ(UINT32 Irq, ISR func, void* cookie)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::HookInt(const IrqParams& irqInfo, ISR func, void* cookie)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::UnhookInt(const IrqParams& irqInfo, ISR func, void* cookie)
{
    return RC::UNSUPPORTED_FUNCTION;
}

#endif

RC Xp::GetNTPTime(UINT32 &secs, UINT32 &msecs, const char *hostname)
{
    return RC::UNSUPPORTED_FUNCTION;
}

#if !defined(MACOSX_MFG) || !defined(INCLUDE_GPU)
void Xp::EnableInterrupts()
{
    MASSERT(0);
}

void Xp::DisableInterrupts()
{
    MASSERT(0);
}

Platform::Irql Xp::GetLwrrentIrql()
{
    // Since we can't actually raise the IRQL...
    return Platform::NormalIrql;
}

Platform::Irql Xp::RaiseIrql(Platform::Irql NewIrql)
{
    // Since we can't actually raise the IRQL...
    return Platform::NormalIrql;
}

void Xp::LowerIrql(Platform::Irql NewIrql)
{
    // nothing to do
}
#endif

//------------------------------------------------------------------------------
// Returns the path delimiter for extracting paths from elw variables
//
char Xp::GetElwPathDelimiter()
{
    return LwDiagXp::GetElwPathDelimiter();
}

Xp::OperatingSystem Xp::GetOperatingSystem()
{
#if defined(MACOSX_MFG)
    return OS_MAC;
#else
    return OS_MACRM;
#endif
}

#if defined(INCLUDE_GPU)
//----------------------------------------------------------
//----------------------------------------------------------
//
// OSX Specific entry and exit functions
//
//! \brief Colwert a CFStringRef pixelEncoding to an INT32 bitsPerPixel
//!
struct CFStringToBppLookup { const char *name; INT32 bpp; };
static INT32 PixelEncodeToBpp(CFStringRef pixelEncoding)
{
    static const CFStringToBppLookup pixelTable[] =
    {
        { kIO64BitDirectPixels, 64 },
        { IO32BitDirectPixels,  32 },
        { kIO32BitFloatPixels,  32 },
        { kIO30BitDirectPixels, 30 },
        { IO16BitDirectPixels,  16 },
        { kIO16BitFloatPixels,  16 },
        { IO8BitIndexedPixels,   8 }
    };

    for (UINT32 ii = 0; ii < NUMELEMS(pixelTable); ++ii)
    {
        CFComparisonResult result;
        CFStringRef lwrrPixelEncoding = CFStringCreateWithCString(
                kCFAllocatorDefault,
                pixelTable[ii].name,
                kCFStringEncodingASCII);

        result = CFStringCompare(pixelEncoding,
                                 lwrrPixelEncoding,
                                 kCFCompareCaseInsensitive);
        CFRelease(lwrrPixelEncoding);

        if (kCFCompareEqualTo == result)
        {
            return pixelTable[ii].bpp;
        }
    }

    // Should never get here
    MASSERT(!"PixelEncodeToBpp(): unsupported pixel format.");
    return 0;
}

//--------------------------------------------------------------------
//! \brief Find a CG mode supported by a display, other than the original mode
//!
//! This routine is used by Xp::EnableUserInterface, to find a mode
//! other than the original one.  (The original mode should be the
//! mode selected on the System Preferences panel).  The calling code
//! in Xp::EnableUserInterface explains why we need this.
//!
//! This routine lwrrently tries to find the "best" mode, where "best"
//! means (a) the same bits per pixel as the original mode, and (b) a
//! resolution as close as possible to the original one, without
//! actually being the same.  Requirement (a) was added after
//! selecting 8-bit-per-pixel modes was found to cause problems.
//! Requirement (b) was added after the M63 listed 1x1 as an available
//! resolution, even though it was not supported.
//!
static CGDisplayModeRef FindOtherMode
(
    CGDirectDisplayID displayId,
    CGDisplayModeRef origMode
)
{
    INT32 origWidth        = CGDisplayModeGetWidth(origMode);
    INT32 origHeight       = CGDisplayModeGetHeight(origMode);
    CFStringRef origPixelEncoding = CGDisplayModeCopyPixelEncoding(origMode);
    INT32 origBitsPerPixel = PixelEncodeToBpp(origPixelEncoding);

    CGDisplayModeRef otherMode  = origMode;
    double          otherScore = 0;

    CFArrayRef allModes = CGDisplayCopyAllDisplayModes(displayId, NULL);
    CFIndex numModes = CFArrayGetCount(allModes);

    for (CFIndex modeIdx = 0; modeIdx < numModes; modeIdx++)
    {
        CGDisplayModeRef mode =
            (CGDisplayModeRef)const_cast<void*>(CFArrayGetValueAtIndex(allModes, modeIdx));
        INT32 width           = CGDisplayModeGetWidth(mode);
        INT32 height          = CGDisplayModeGetHeight(mode);
        CFStringRef pixelEncoding = CGDisplayModeCopyPixelEncoding(mode);
        INT32 bitsPerPixel    = PixelEncodeToBpp(pixelEncoding);

        double score = 0;

        if (width != origWidth || height != origHeight)
        {
            double w1 = min(width, origWidth);
            double w2 = max(max(width, origWidth), (INT32)1);
            double h1 = min(height, origHeight);
            double h2 = max(max(height, origHeight), (INT32)1);
            score += min(w1/w2, h1/h2);
        }
        if (bitsPerPixel == origBitsPerPixel)
        {
            score += 1.0;
        }
        if (mode == origMode)
        {
            score = 0;
        }
        if (score > otherScore)
        {
            otherMode  = mode;
            otherScore = score;
        }

        // Release object reference pixelEncoding
        CFRelease(pixelEncoding);
    }

    // Release any local object references
    CFRelease(origPixelEncoding);
    CFRelease(allModes);

    return otherMode;
}
#endif

RC Xp::OnEntry(vector<string> *pArgs)
{
    MASSERT((NULL != pArgs) && (0 != pArgs->size()));

    RC rc;
    if (pArgs->size() == 1)
    {
        // Get the command line arguments from mods.arg.  Don't check the result
        // code of Utility::OpenFile(); if it's not present that just means that
        // there's no argument file and that's OK.
        FILE *ArgsFile = 0;
        if (DoesFileExist("mods.arg"))
            Utility::OpenFile("mods.arg", &ArgsFile, "r");

        if (ArgsFile)
        {
            // Read and parse the command line.
            char buf[256];
            fgets(buf, sizeof(buf), ArgsFile);
            fclose(ArgsFile);

            // Build new command line arguments from loaded arguments
            Utility::CreateArgv(buf, pArgs);
        }
    }

#if !defined(MACOSX_MFG)
    s_ConsoleMode = CommandLine::SingleUser();
#endif

    // Get CPU properties
    CHECK_RC(Cpu::Initialize());

    // Start our cooperative multithreader.
    // In macos, this is a wrapper around pthreads.
    rc = Tasker::Initialize();
    if (OK != rc)
    {
        Printf(Tee::PriAlways, "Tasker::Initialize failed!\n");
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

#if defined(INCLUDE_GPU)

#if defined(MACOSX_MFG)
    if (mods_open() < 0)
    {
        Printf(Tee::PriHigh,
               "Error: Unable to open driver. Another instance of MODS may be running.\n");
        return RC::SOFTWARE_ERROR;
    }

    // Detect whether we are in single user mode
    char *term = getelw("TERM");
    s_ConsoleMode = !!(term && strcmp(term, "vt100") == 0);

    // Detect displaylessness. This fixes bug 1317379 in which the CG calls on
    // Macs with the displayless GM108 were returning error codes and causing
    // MODS to fail. In console mode, no CG calls will be made.
    if (!s_ConsoleMode)
    {
        CGError cg;
        uint32_t numDisplays;
        RC returlwalueForCGFailure = RC::SOFTWARE_ERROR;

        cg = CGGetActiveDisplayList(0, NULL, &numDisplays);
        if (cg == kCGErrorNoneAvailable && numDisplays == 0)
        {
            Printf(Tee::PriNormal, "No displays found; using console mode\n");
            s_ConsoleMode = true;
        }
        else
        {
            CHECK_CG(cg);
        }
    }
#else

    // Detect whether we are running under the OSX Window Manager, or
    // text-only console mode. We only allow console mods if "-1" is
    // passed in
    // WAR: There is probably a better way to do this
    if (!s_ConsoleMode)
    {
        CGError err;

        // Try to hide the cursor.
        err = CGDisplayHideLwrsor( kCGDirectMainDisplay );
        Printf(Tee::PriDebug, "CGHideLwrsor returned 0x%x, ", err);

        // If we cannot modify the cursor, assume there is no lwsor, and
        // we are in text mode.
        if (err)
        {
            // only allow if "-1" (console mode allowed) is passed in
            if (!s_ConsoleMode)
            {
                Printf(Tee::PriAlways,
                       "Failed to connect to window Manager!!\n");
                Printf(Tee::PriAlways, "(Do you own the desktop?)\n");
                return RC::CORE_GRAPHICS_CALL_HUNG;
            }
        }
        else
        {
            s_ConsoleMode = false;
            CGDisplayShowLwrsor( kCGDirectMainDisplay );
        }

        Printf(Tee::PriLow, "Running %s window manager\n",
               s_ConsoleMode ? "without" : "with");
    }
#endif

    // Set the cursor level to 0.
    if (!s_ConsoleMode)
    {
        CFStringRef reasonForActivity = CFSTR("MODS Running");

        // Prevent display from sleeping
        IOReturn status = IOPMAssertionCreateWithName(kIOPMAssertPreventUserIdleDisplaySleep,
                                                      kIOPMAssertionLevelOn,
                                                      reasonForActivity,
                                                      &s_DisplayAssertionID);
        if (status != kIOReturnSuccess)
        {
            Printf(Tee::PriError, "Cannot prevent display from idling\n");
            return RC::SOFTWARE_ERROR;
        }

        // Prevent system from sleeping
        status = IOPMAssertionCreateWithName(kIOPMAssertPreventUserIdleSystemSleep,
                                             kIOPMAssertionLevelOn,
                                             reasonForActivity,
                                             &s_SystemAssertionID);

        if (status != kIOReturnSuccess)
        {
            Printf(Tee::PriError, "Cannot prevent system from idling\n");
            return RC::SOFTWARE_ERROR;
        }
    }
#endif // INCLUDE_GPU

    // Initialize Telnet MODS
    if (!CommandLine::TelnetCmdline().empty())
    {
        SocketMods * socket = GetSocket();

        SocketAnsi *psa = new SocketAnsi(*socket);

        // initialize remote console
        s_bRemoteConsole = true;
        s_pRemoteSocket = psa;
        s_pBaseSocket = socket;

        string RemoteArgs;
        if (CommandLine::TelnetServer())
        {
            // server mode
            // NONONO we can't use port 23 on a real OS.
            CHECK_RC(TelnetUtil::StartTelnetServer(*s_pRemoteSocket,
                                                   RemoteArgs, 2023));
        }
        else
        {
            // client mode
            const UINT32 Ip = Socket::ParseIp(CommandLine::TelnetClientIp());
            const UINT16 Port = static_cast<UINT16>(CommandLine::TelnetClientPort());
            CHECK_RC(TelnetUtil::StartTelnetClient(*s_pRemoteSocket,
                                                   RemoteArgs, Ip, Port));
        }
        RemoteArgs = pArgs->at(0) + " " + CommandLine::TelnetCmdline() + " " + RemoteArgs;

        Printf(Tee::PriAlways, "Remote command line: %s\n",
               RemoteArgs.c_str());

        s_pRemoteSink = new RemoteDataSink (s_pRemoteSocket);
        Tee::SetScreenSink(s_pRemoteSink);

        // Build new command line arguments from remote arguments
        Utility::CreateArgv(RemoteArgs, pArgs);
    }

#if defined(MACOSX_MFG) && defined(INCLUDE_GPU)
    CHECK_RC(InitializeKernelCalls());

    CHECK_RC(DisableMalwserInterface());
#endif

    return OK;
}

RC Xp::PreOnExit()
{
    StickyRC firstRc;

#ifdef INCLUDE_GPU

#if defined(MACOSX_MFG)
    firstRc = FinalizeKernelCalls();
#endif

#if !defined(MACOSX_MFG)
    // EnableUserInterface should have restored the core channel and
    // removed the Apple stepaside, but let's make sure as OSX will
    // not run otherwise
    //
    if (s_CoreChannelReleased)
    {
        NdrvReleaseCoreChannel(false);
        s_CoreChannelReleased = false;
        Printf(Tee::PriHigh,
               "Warning!! Exiting with Apple core channel still released!!\n");
    }
    if (s_AppleSteppedAside)
    {
        UINT32 stepaside = 0;
        firstRc = LwRmPtr()->OsConfigSet(LW_OSCFG_APPLE_STEP_ASIDE,
                                         0, &stepaside);
        s_AppleSteppedAside = false;
        Printf(Tee::PriHigh,
               "Warning!! Exiting with Apple stepaside still enabled!!\n");
    }
#endif

    // Has Gpu::ShutDown completed ?
    if (GpuPtr()->IsInitialized())
    {
        firstRc = GpuPtr()->ShutDown(Gpu::ShutDownMode::Normal);
    }

#if defined(MACOSX_MFG)
    firstRc = EnableMalwserInterface();
#endif

    // Release the apple resources
    //
    for( AppleDisplays::iterator pAppleDisp = s_appleDisplays.begin();
         pAppleDisp != s_appleDisplays.end(); ++pAppleDisp)
    {
        CFRelease(pAppleDisp->originalMode);
    }
    s_appleDisplays.clear();

#endif // INCLUDE_GPU

    return firstRc;
}

RC Xp::OnExit(RC exitRc)
{
    StickyRC firstRc = exitRc;

#if defined(MACOSX_MFG) && defined(INCLUDE_GPU)
    if (mods_close() < 0)
    {
        Printf(Tee::PriHigh, "Error: Unable to close kext\n");
        firstRc = RC::SOFTWARE_ERROR;
    }
#endif

    // Tasker must be cleaned up first, global destructors will delete
    // static data before tasker is done with it.
    // Console manager should be cleaned up after all event threads
    // complete but before Tasker is cleaned up since console manager in an
    // initialized state uses Tasker
    firstRc = DllHelper::Shutdown();
    EventThread::Cleanup();
    ConsoleManager::Instance()->Cleanup();
    MemError::Shutdown();
    Tasker::Cleanup();

    // Shutdown output console stream system:
    if (s_bRemoteConsole)
    {
        s_pRemoteSocket->Close();
        delete s_pRemoteSocket;
        FreeSocket(s_pBaseSocket);
        delete s_pRemoteSink;

        // Turn off console output!
        // @@@ Ideally, we would restore a "safe" sink.
        Tee::SetScreenSink(0);
        Tee::SetScreenLevel(Tee::LevNone);
    }

    // all sockets should be freed at this point, but if not, still free them
    MASSERT(NULL == s_pOsSockets);
    if (s_pOsSockets)
    {
        while(s_pOsSockets)
        {
            // FreeSocket removes it from the list
            FreeSocket(s_pOsSockets->back());
        }
    }

    // Allow the system and display to idle again
#if defined(INCLUDE_GPU)
    if (!s_ConsoleMode)
    {
        IOReturn status = IOPMAssertionRelease(s_DisplayAssertionID);
        if (status != kIOReturnSuccess)
        {
            Printf(Tee::PriError, "Could not restore display's ability to idle\n");
            firstRc = RC::SOFTWARE_ERROR;
        }

        status = IOPMAssertionRelease(s_SystemAssertionID);
        if (status != kIOReturnSuccess)
        {
            Printf(Tee::PriError, "Could not restore system's ability to idle\n");
            firstRc = RC::SOFTWARE_ERROR;
        }
    }
#endif

    return firstRc;
}

RC Xp::OnQuickExit(RC rc)
{
    // Accelerated exit is not implemented on this platform
    return Xp::OnExit(rc);
}

RC Xp::PauseKernelLogger(bool pause)
{
    return RC::OK;
}

void Xp::ExitFromGlobalObjectDestructor(INT32 exitValue)
{
    exit(exitValue);
}

//------------------------------------------------------------------------------
RC Xp::LoadDLL(const string &fileName, void **pModuleHandle, UINT32 loadDLLFlags)
{
    Tasker::MutexHolder dllMutexHolder(DllHelper::GetMutex());
    string fileNameWithSuffix = DllHelper::AppendMissingSuffix(fileName);
    RC rc;

    // Load the DLL
    //
    LwDiagUtils::EC ec =
        LwDiagXp::LoadDynamicLibrary(fileNameWithSuffix,
                                     pModuleHandle,
                                     RTLD_NOW | ((loadDLLFlags & GlobalDLL) ? RTLD_GLOBAL : 0));

    if (OK != Utility::InterpretModsUtilErrorCode(ec))
    {
        string dlErrorString = dlerror();
        Printf(Tee::PriHigh, "Error opening %s: %s\n",
               fileNameWithSuffix.c_str(), dlErrorString.c_str());
        return RC::DLL_LOAD_FAILED;
    }

    CHECK_RC(DllHelper::RegisterModuleHandle(*pModuleHandle, !!(loadDLLFlags & UnloadDLLOnExit)));
    return OK;
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
        Printf(Tee::PriHigh, "Error closing shared library: %s\n",
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

//---------------------------------------------
// disable/Enable UI stuff starts here...
//---------------------------------------------

#if defined(INCLUDE_GPU)

RC DisableMalwserInterface()
{
    RC returlwalueForCGFailure = RC::CANNOT_DISABLE_USER_INTERFACE;
    RC rc;

#if defined(MACOSX_MFG)
    if (s_ConsoleMode && mods_set_console_info(MODS_CONSOLE_INFO_DISABLE) < 0)
    {
        Printf(Tee::PriHigh, "Unable to set console info\n");
        return RC::CANNOT_DISABLE_USER_INTERFACE;
    }
#endif // MACOSX_MFG

    // Record the original resolution of the apple displays (using
    // apple data structures) so that we can restore them in
    // OnExit.
    //
    if (s_appleDisplays.empty())
    {
#if defined(MACOSX_MFG)
        if (!s_ConsoleMode)
        {
#endif // MACOSX_MFG
        CGDirectDisplayID displayArray[MAX_DISPLAYS];
        uint32_t numDisplays;

        CHECK_CG(CGGetActiveDisplayList(MAX_DISPLAYS, displayArray,
                                        &numDisplays));
        s_appleDisplays.resize(numDisplays);
        for (uint32_t ii = 0; ii < numDisplays; ii++)
        {
            CGDirectDisplayID displayId = displayArray[ii] ;
            s_appleDisplays[ii].displayId = displayId;
            s_appleDisplays[ii].originalMode =
                CGDisplayCopyDisplayMode(displayId);
        }
#if defined(MACOSX_MFG)
        }
#endif // MACOSX_MFG
    }

    // Disable the GUI if we are running under the OSX Window Manager
    //
    if (!s_ConsoleMode)
    {
        // Use CG (Quartz Core Graphics) interface to disable the GUI
        // and hide the cursor.
        //
        CHECK_CG(CGCaptureAllDisplays());
        Tasker::Sleep(200);

        CHECK_CG(CGDisplayHideLwrsor(kCGDirectMainDisplay));
        Tasker::Sleep(200);
    }

    return rc;
}

#endif // INCLUDE_GPU

//--------------------------------------------------------------------
//! \brief Disable the Apple GUI.
//!
//! Note: Historically, there was a pause after each CG command for
//! bug 84159.  This was because the Core Graphics calls return before
//! they are complete, but Panther was supposed to introduce a
//! "Display Disable" call to prevent the WM from interfering.  I'm
//! not sure if that's the same as the "Capture Display" call we're
//! using now, and MODS still works if we remove the sleeps, but we
//! can't see the start of the MODS tests if we remove the sleeps.
//!
RC Xp::DisableUserInterface()
{
#ifdef INCLUDE_GPU

    RC      rc;

#if !defined(MACOSX_MFG)

    LwRmPtr pLwRm;

    CHECK_RC(DisableMalwserInterface());

    // Make apple think the display driver is innaccesible.
    //
    if (s_DoStepAside)
    {
        UINT32 stepaside = 0;
        CHECK_RC(pLwRm->OsConfigSet(LW_OSCFG_APPLE_STEP_ASIDE,
                                    1 << 31, &stepaside));
        s_AppleSteppedAside = true;
    }

    // Initialize NDRV interface, and save display settings for later.
    //
    if (!s_NdrvAttached)
    {
        CHECK_RC(NdrvInit());
        s_NdrvAttached = true;
    }
    CHECK_RC(NdrvGamma(true));

    GpuDevMgr * pGpuDevMgr = (GpuDevMgr *)(DevMgrMgr::d_GraphDevMgr);
    MASSERT(pGpuDevMgr);

    for (GpuDevice *pGpuDev = pGpuDevMgr->GetFirstGpuDevice();
         pGpuDev != NULL; pGpuDev = pGpuDevMgr->GetNextGpuDevice(pGpuDev))
    {
        Display * pDisplay = pGpuDev->GetDisplay();

        if (Display::NULLDISP != pDisplay->GetDisplayClassFamily())
        {
            if (!s_CoreChannelReleased)
            {
                // This actually affects all devices, not just one,
                // don't repeat it per-device.
                CHECK_RC(NdrvReleaseCoreChannel(true));
                s_CoreChannelReleased = true;
            }

            s_oldDisplayMask[pGpuDev->GetDeviceInst()] = pDisplay->Selected();

            if (pDisplay->Selected())
                CHECK_RC(pDisplay->DisableDithering(pDisplay->Selected()));
        }
    }

#endif // ! MACOSX_MFG

    return rc;

#else  // INCLUDE_GPU
    Printf(Tee::PriHigh,
           "DisableUserInterface not supported in non-GPU builds yet!\n");
    return RC::CANNOT_DISABLE_USER_INTERFACE;
#endif // INCLUDE_GPU
}

//! \brief Undo the effects of Xp::DisbleUserInterface
//!
//! For every call to Xp::DisableUserInterface(), there should be a
//! corresponding call to this routine.  This routine undoes the
//! effects of Xp::DisableUserInterface().
//!
RC Xp::EnableUserInterface()
{
#ifdef INCLUDE_GPU

    RC rc;

    GpuDevMgr * pGpuDevMgr = (GpuDevMgr *)(DevMgrMgr::d_GraphDevMgr);
    MASSERT(pGpuDevMgr);

#if !defined(MACOSX_MFG)
    // Allow RM to change the PState to handle the upcoming mode set.
    for (GpuSubdevice *pSubdev = pGpuDevMgr->GetFirstGpu();
         pSubdev != NULL; pSubdev = pGpuDevMgr->GetNextGpu(pSubdev))
    {
        Perf *pPerf = pSubdev->GetPerf();
        bool bUnlockBeforePstateSwitch = true;

        if (pPerf)
        {
            // When shmooing with we expect MODS to not cause any backdoor
            // switching by relieving pstate control to RM
            // This feature is a default disable, but can enabled using
            // -clear_perf_levels_for_ui
            bUnlockBeforePstateSwitch = pPerf->GetClearPerfLevelsForUi();

            if (!bUnlockBeforePstateSwitch)
                continue;

            pPerf->ClearForcedPState();
        }
    }
#endif

    for (GpuDevice *pGpuDev = pGpuDevMgr->GetFirstGpuDevice();
         pGpuDev != NULL; pGpuDev = pGpuDevMgr->GetNextGpuDevice(pGpuDev))
    {
        Display* const pDisplay = pGpuDev->GetDisplay();
        if (pDisplay)
        {
            // Free the EVO core channel on G8x and above
            //
            pDisplay->FreeGpuResources(pGpuDev->GetDeviceInst());

#if !defined(MACOSX_MFG)
            // Make sure to free RM resources before returning contorl
            // to NDRV.  This prevents MODS from unhooking an OS event
            // that NDRV reattaches.
            //
            pDisplay->FreeRmResources(pGpuDev->GetDeviceInst());

            // Restore the display settings
            //
            pDisplay->Select(s_oldDisplayMask[pGpuDev->GetDeviceInst()]);
#endif
        }
    }

#if !defined(MACOSX_MFG)
    // Undo NDRV requests
    //
    if (s_CoreChannelReleased)
    {
        NdrvReleaseCoreChannel(false);
        s_CoreChannelReleased = false;
    }
    NdrvGamma(false);
    NdrvClose();

    CHECK_RC(EnableMalwserInterface());
#endif

    return rc;

#else  // INCLUDE_GPU
    Printf(Tee::PriHigh,
           "EnableUserInterface not supported in non-GPU builds yet!\n");
    return RC::SOFTWARE_ERROR;
#endif // INCLUDE_GPU
}

#if defined(INCLUDE_GPU)

RC EnableMalwserInterface()
{
    RC      rc;
    LwRmPtr pLwRm;
    RC      returlwalueForCGFailure = RC::CANNOT_ENABLE_USER_INTERFACE;

#if defined(MACOSX_MFG)
    if (s_ConsoleMode && mods_set_console_info(MODS_CONSOLE_INFO_ENABLE) < 0)
    {
        Printf(Tee::PriHigh, "Unable to set console info\n");
        return RC::CANNOT_ENABLE_USER_INTERFACE;
    }
#endif

#if !defined(MACOSX_MFG)
    // Remove Apple stepaside.  Must be done before we call any CG
    // stuff, or else the CG calls will wait forever.
    //
    if (s_AppleSteppedAside)
    {
        UINT32 stepaside = 0;
        CHECK_RC(pLwRm->OsConfigSet(LW_OSCFG_APPLE_STEP_ASIDE,
                                    0, &stepaside));
        s_AppleSteppedAside = false;
    }
#endif

    // Before releasing the CG displays, CG to change each display
    // to any resolution other than the one they were in when MODS
    // started.  Then we do a second modeset, to change the
    // displays to their original resolutions.
    //
    // The double-modeset forces the Apple driver to do the second
    // modeset.  (Apple skips the modeset if it thinks the display
    // is already in the requested mode.)  This ensures that the
    // Apple driver is controlling the display again; we sometimes
    // get a blank screen otherwise.
    //
    if (!s_appleDisplays.empty())
    {
#if defined(MACOSX_MFG)
        if (!s_ConsoleMode)
        {
#endif
        RC returlwalueForCGFailure = RC::CORE_GRAPHICS_CALL_HUNG;
        CGDisplayConfigRef config;
        AppleDisplays::iterator pAppleDisp;

        // Do a modeset to force each display into some mode other
        // that the original mode.
        //
        CHECK_CG(CGBeginDisplayConfiguration(&config));

        for (pAppleDisp = s_appleDisplays.begin();
             pAppleDisp != s_appleDisplays.end(); ++pAppleDisp)
        {
            CGDisplayModeRef mode = FindOtherMode(pAppleDisp->displayId,
                                                  pAppleDisp->originalMode);
            CHECK_CG(CGConfigureDisplayWithDisplayMode(config,
                                                       pAppleDisp->displayId,
                                                       mode, NULL));
        }

        CHECK_CG(CGConfigureDisplayFadeEffect(config, 0, 0,
                                              0.5, 0.5, 0.5));
        CHECK_CG(CGCompleteDisplayConfiguration(config,
                                                kCGConfigureForAppOnly));
        Tasker::Sleep(200);

        // Do a second modeset to force each display to its
        // original mode.
        //
        CHECK_CG(CGBeginDisplayConfiguration(&config));
        for (pAppleDisp = s_appleDisplays.begin();
             pAppleDisp != s_appleDisplays.end(); ++pAppleDisp)
        {
            CHECK_CG(CGConfigureDisplayWithDisplayMode(config,
                                                       pAppleDisp->displayId,
                                                       pAppleDisp->originalMode,
                                                       NULL));
        }
        CHECK_CG(CGConfigureDisplayFadeEffect(config, 0, 0,
                                              0.5, 0.5, 0.5));
        CHECK_CG(CGCompleteDisplayConfiguration(config,
                                                kCGConfigureForAppOnly));
        Tasker::Sleep(200);
#if defined(MACOSX_MFG)
        }
#endif

        // If running under the MacOSX GUI, then re-enable the GUI by
        // reenabling the cursor and releasing all displays.
        //
        if (!s_ConsoleMode)
        {
            CHECK_CG(CGDisplayShowLwrsor(kCGDirectMainDisplay));
            Tasker::Sleep(200);
            CHECK_CG(CGReleaseAllDisplays());
            Tasker::Sleep(200);
        }
    }

    return rc;
}

#endif // INCLUDE_GPU

//-----------------------------------
// End of Disable/Enable UI stuff.
//-----------------------------------

/**
 * Do we have a remote console?
 */
bool Xp::IsRemoteConsole()
{
    return s_bRemoteConsole;
}

/**
 * Get remote console Socket
 */
SocketMods * Xp::GetRemoteConsole()
{
    return s_pRemoteSocket;
}

// Allocate a socket.
SocketMods * Xp::GetSocket()
{
    SocketMods *psd = new SocketMods(LwDiagXp::CreateSocket());
    if (!psd)
        return 0;

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

// Allocate an OS event
void *Xp::AllocOsEvent(UINT32 hClient, UINT32 hDevice)
{
#ifdef INCLUDE_GPU

#if defined(MACOSX_MFG)
    return NULL;
#else //MACOSX_MFG
    void *pOsEvent;
    int status = LwRmAllocOsEvent(hClient, &pOsEvent);
    MASSERT(status == LW_OK);
    return (status == LW_OK) ? pOsEvent : NULL;
#endif // MACOSX_MFG

#else // INCLUDE_GPU
    MASSERT(!"Xp::AllocOsEvent() is not supported without GPU");
    return NULL;
#endif // INCLUDE_GPU
}

// Free an OS event
void Xp::FreeOsEvent(void *pOsEvent, UINT32 hClient, UINT32 hDevice)
{
#if !defined(MACOSX_MFG)
#ifdef INCLUDE_GPU
    LwRmFreeOsEvent(hClient, pOsEvent);
#else // INCLUDE_GPU
    MASSERT(!"Xp::FreeOsEvent() is not supported without GPU");
#endif // INCLUDE_GPU
#endif // ! MACOSX_MFG
}

/**
 * Unsupported under OSX.
 */
RC Xp::SetVesaMode
(
    UINT32 Mode,
    bool   Windowed,     // = true
    bool   ClearMemory   // = true
)
{
    return RC::SET_MODE_FAILED;
}

void Xp::BreakPoint()
{
    Cpu::BreakPoint();
}

void Xp::DebugPrint(const char* str, size_t strLen)
{
}

//------------------------------------------------------------------------------
//!< Return the page size in bytes for this platform.
size_t Xp::GetPageSize()
{
    return 4096;
}

//------------------------------------------------------------------------------
//! Implementation of Xp::Process for Mac
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
UINT32 Xp::GetPid()
{
    return getpid();
}

//------------------------------------------------------------------------------
bool Xp::IsPidRunning(UINT32 pid)
{
    return kill(pid, 0) == 0;
}

//------------------------------------------------------------------------------
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

#if !defined(MACOSX_MFG) || !defined(INCLUDE_GPU)
RC Xp::AllocPages
(
    size_t         numBytes,
    void **        pDescriptor,
    bool           contiguous,
    UINT32         addressBits,
    Memory::Attrib attrib,
    UINT32         domain,
    UINT32         bus,
    UINT32         device,
    UINT32         function
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
//! \brief Alloc system memory.
//!
//! \param NumBytes    : Number of bytes to allocate (actual allocation will be
//!                      a whole number of pages (NumBytes will be rounded up).
//! \param pDescriptor : Returned descriptor for the allocated memory
//! \param Contiguous  : If true, physical pages will be adjacent and in-order.
//! \param AddressBits : All pages will be locked and will be below physical
//!                      address 2**AddressBits.
//! \param Attrib      : Memory attributes to use when allocating
//! \param GpuInst     : IGNORED (For other platforms, if GpuInst is valid, then
//!                      on a NUMA system the system memory will be on the node
//!                      local to the GPU)
//!
//! \return OK if memory was successfully allocated and mapped, not OK otherwise
RC Xp::AllocPages
(
    size_t         NumBytes,
    void **        pDescriptor,
    bool           Contiguous,
    UINT32         AddressBits,
    Memory::Attrib Attrib,
    UINT32         GpuInst
)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
//! \brief Alloc contiguous system memory with pages aligned as requested
//!
//! \param NumBytes    : Number of bytes to allocate (actual allocation will be
//!                      a whole number of pages (NumBytes will be rounded up).
//! \param pDescriptor : Returned descriptor for the allocated memory
//! \param Contiguous  : If true, physical pages will be adjacent and in-order.
//! \param AddressBits : All pages will be locked and will be below physical
//!                      address 2**AddressBits.
//! \param Attrib      : Memory attributes to use when allocating
//! \param GpuInst     : IGNORED (For other platforms, if GpuInst is valid, then
//!                      on a NUMA system the system memory will be on the node
//!                      local to the GPU)
//!
//! \return OK if memory was successfully allocated and mapped, not OK otherwise
RC Xp::AllocPagesAligned
(
    size_t         NumBytes,
    void **        pDescriptor,
    size_t         PhysicalAlignment,
    UINT32         AddressBits,
    Memory::Attrib Attrib,
    UINT32         GpuInst
)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
//!< Free pages allocated by AllocPages or AllocPagesAligned.
void Xp::FreePages(void * Descriptor)
{
    MASSERT(0);
}

RC Xp::MergePages(void** pOutDescriptor, void** pInDescBegin, void** pInDescEnd)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
RC Xp::MapPages
(
    void **pReturnedVirtualAddress,
    void * Descriptor,
    size_t Offset,
    size_t Size,
    Memory::Protect Protect
)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
void Xp::UnMapPages(void * VirtualAddress)
{
    MASSERT(0);
}

//------------------------------------------------------------------------------
PHYSADDR Xp::GetPhysicalAddress(void *Descriptor, size_t Offset)
{
    MASSERT(0);
    return 0;
}

//------------------------------------------------------------------------------
bool Xp::CanEnableIova
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func
)
{
    return false;
}

//------------------------------------------------------------------------------
PHYSADDR Xp::GetMappedPhysicalAddress
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func,
    void  *descriptor,
    size_t offset
)
{
    return GetPhysicalAddress(descriptor, offset);
}

//------------------------------------------------------------------------------
RC Xp::DmaMapMemory
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func,
    void  *descriptor
)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
RC Xp::DmaUnmapMemory
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func,
    void  *descriptor
)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
RC Xp::SetDmaMask(UINT32 domain, UINT32 bus, UINT32 device, UINT32 func, UINT32 numBits)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
RC Xp::SetupDmaBase
(
    UINT16           domain,
    UINT08           bus,
    UINT08           dev,
    UINT08           func,
    PpcTceBypassMode bypassMode,
    UINT64           devDmaMask,
    PHYSADDR *       pDmaBaseAddr
)
{
    MASSERT(pDmaBaseAddr);
    Printf(Tee::PriLow,
           "Setup dma base not supported on current platform skipping\n");
    *pDmaBaseAddr = static_cast<PHYSADDR>(0);
    return OK;
}

//-----------------------------------------------------------------------------
RC Xp::SetLwLinkSysmemTrained
(
    UINT32 domain,
    UINT32 bus,
    UINT32 dev,
    UINT32 func,
    bool   trained
)
{
    Printf(Tee::PriLow, "SetLwLinkSysmemTrained not supported on current platform, skipping\n");
    return OK;
}
#endif

//------------------------------------------------------------------------------
RC Xp::GetCarveout(PHYSADDR* pPhysAddr, UINT64* pSize)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
RC Xp::GetFbConsoleInfo(PHYSADDR *pBaseAddress, UINT64 *pSize)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
void Xp::SuspendConsole(GpuSubdevice *pGpuSubdevice)
{
}

//------------------------------------------------------------------------------
void Xp::ResumeConsole(GpuSubdevice *pGpuSubdevice)
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

#if !defined(MACOSX_MFG) || !defined(INCLUDE_GPU)
//------------------------------------------------------------------------------
//!< Map device memory into the page table, get virtual address to use.
//!< Multiple or overlapping mappings will fail.
RC Xp::MapDeviceMemory
(
    void **     pReturnedVirtualAddress,
    PHYSADDR    PhysicalAddress,
    size_t      NumBytes,
    Memory::Attrib Attrib,
    Memory::Protect Protect
)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
//!< Undo the mapping created by MapDeviceMemory.
void Xp::UnMapDeviceMemory(void * VirtualAddress)
{
    MASSERT(0);
}
#endif

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
#if defined(MACOSX_MFG)
    return OK;
#else
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

//------------------------------------------------------------------------------
RC Xp::UnSetMemRange
(
    PHYSADDR    PhysicalAddress,
    size_t      NumBytes
)
{
#if defined(MACOSX_MFG)
    return OK;
#else
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
#endif
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
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::FreeAddressSpace
(
    void *                    pVirtualAddress,
    size_t                    numBytes,
    Memory::VirtualFreeMethod vfMethod
)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

void *Xp::VirtualFindFreeVaInRange(size_t size, void *pBase, void *pLimit, size_t align)
{
    MASSERT(!"*Xp::VirtualFindFreeVaInRange not supported on current platform");
    return NULL;
}

RC Xp::VirtualProtect(void *pAddr, size_t size, UINT32 protect)
{
    MASSERT(!"*Xp::VirtualProtect not supported on current platform");
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::VirtualMadvise(void *pAddr, size_t size, Memory::VirtualAdvice advice)
{
    MASSERT(!"*Xp::VirtualMadvise not supported on current platform");
    return RC::UNSUPPORTED_FUNCTION;
}

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
void * Xp::ExecMalloc(size_t NumBytes)
{
    void *Address = mmap(NULL, NumBytes,
                         PROT_EXEC | PROT_READ | PROT_WRITE,
                         MAP_ANON | MAP_PRIVATE, -1, 0);
    if (Address == MAP_FAILED)
        return NULL;
    Pool::AddNonPoolAlloc(Address, NumBytes);
    return Address;
}

void Xp::ExecFree(void *Address, size_t NumBytes)
{
    Pool::RemoveNonPoolAlloc(Address, NumBytes);
    munmap(Address, NumBytes);
}

#if !defined(MACOSX_MFG) || !defined(INCLUDE_GPU)
//------------------------------------------------------------------------------
// Colwert physical address to virtual address.
//
void * Xp::PhysicalToVirtual(PHYSADDR PhysicalAddress, Tee::Priority pri)
{
    return (void *)(uintptr_t)PhysicalAddress;
}

//------------------------------------------------------------------------------
// Colwert virtual address to physical address.
//
PHYSADDR Xp::VirtualToPhysical(volatile void * VirtualAddress, Tee::Priority pri)
{
    return (uintptr_t)VirtualAddress;
}

RC Xp::CallACPIGeneric(UINT32 GpuInst,
                       UINT32 Function,
                       void *Param1,
                       void *Param2,
                       void *Param3)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::LWIFMethod
(
    UINT32 Function,
    UINT32 SubFunction,
    void *InParams,
    UINT16 InParamSize,
    UINT32 *OutStatus,
    void *OutData,
    UINT16 *OutDataSize
)
{
    return RC::UNSUPPORTED_FUNCTION;
}
#endif

#if !defined(MACOSX_MFG) || !defined(INCLUDE_GPU)
RC Xp::FindPciDevice
(
    INT32   DeviceId,
    INT32   VendorId,
    INT32   Index,
    UINT32* pDomainNumber   /* = 0 */,
    UINT32* pBusNumber      /* = 0 */,
    UINT32* pDeviceNumber   /* = 0 */,
    UINT32* pFunctionNumber /* = 0 */
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::FindPciClassCode
(
    INT32   ClassCode,
    INT32   Index,
    UINT32* pDomainNumber   /* = 0 */,
    UINT32* pBusNumber      /* = 0 */,
    UINT32* pDeviceNumber   /* = 0 */,
    UINT32* pFunctionNumber /* = 0 */
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
// Force a scan of PCI bus.
RC Xp::RescanPciBus
(
    INT32 domain,
    INT32 bus
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
RC Xp::PciResetFunction
(
    INT32 DomainNumber,
    INT32 BusNumber,
    INT32 DeviceNumber,
    INT32 FunctionNumber
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
// Return Base Address and Size for specified GPU BAR.
RC Xp::PciGetBarInfo
(
    INT32 DomainNumber,
    INT32 BusNumber,
    INT32 DeviceNumber,
    INT32 FunctionNumber,
    INT32 BarIndex,
    UINT64* pBaseAddress,
    UINT64* pBarSize
)
{
    return Pci::GetBarInfo(DomainNumber, BusNumber,
                           DeviceNumber, FunctionNumber,
                           BarIndex, pBaseAddress, pBarSize);
}

//------------------------------------------------------------------------------
// Return the IRQ number for the specified GPU.
RC Xp::PciGetIRQ
(
    INT32 DomainNumber,
    INT32 BusNumber,
    INT32 DeviceNumber,
    INT32 FunctionNumber,
    UINT32* pIrq
)
{
    return Pci::GetIRQ(DomainNumber, BusNumber,
                       DeviceNumber, FunctionNumber, pIrq);
}

RC Xp::PciRead08
(
    INT32    DomainNumber,
    INT32    BusNumber,
    INT32    DeviceNumber,
    INT32    FunctionNumber,
    INT32    Address,
    UINT08 * pData
)
{
    MASSERT(pData != 0);

    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::PciRead16
(
    INT32    DomainNumber,
    INT32    BusNumber,
    INT32    DeviceNumber,
    INT32    FunctionNumber,
    INT32    Address,
    UINT16 * pData
)
{
    MASSERT(pData != 0);

    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::PciRead32
(
    INT32    DomainNumber,
    INT32    BusNumber,
    INT32    DeviceNumber,
    INT32    FunctionNumber,
    INT32    Address,
    UINT32 * pData
)
{
    MASSERT(pData != 0);

    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::PciWrite08
(
    INT32  DomainNumber,
    INT32  BusNumber,
    INT32  DeviceNumber,
    INT32  FunctionNumber,
    INT32  Address,
    UINT08 Data
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::PciWrite16
(
    INT32  DomainNumber,
    INT32  BusNumber,
    INT32  DeviceNumber,
    INT32  FunctionNumber,
    INT32  Address,
    UINT16 Data
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::PciWrite32
(
    INT32  DomainNumber,
    INT32  BusNumber,
    INT32  DeviceNumber,
    INT32  FunctionNumber,
    INT32  Address,
    UINT32 Data
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::PciRemove
(
    INT32    domainNumber,
    INT32    busNumber,
    INT32    deviceNumber,
    INT32    functionNumber
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
// Port read and write functions.
//
RC Xp::PioRead08(UINT16 Port, UINT08 * pData)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::PioRead16(UINT16 Port, UINT16 * pData)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::PioRead32(UINT16 Port, UINT32 * pData)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::PioWrite08(UINT16 Port, UINT08 Data)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::PioWrite16(UINT16 Port, UINT16 Data)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::PioWrite32(UINT16 Port, UINT32 Data)
{
    return RC::UNSUPPORTED_FUNCTION;
}
#endif

//------------------------------------------------------------------------------
// Enable or disable SR-IOV.  Lwrrently, mac doesn't support SR-IOV.
RC Xp::PciEnableSriov
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    UINT32 vfCount
)
{
    return (vfCount > 0) ? RC::UNSUPPORTED_FUNCTION : OK;
}

void Xp::MemCopy(void * dst, void * src, UINT32 length, UINT32 type)
{
    memcpy(dst, src, length);
}

bool Xp::DoesFileExist(string strFilename)
{
    return LwDiagXp::DoesFileExist(strFilename);
}

//------------------------------------------------------------------------------
// Delete file
//
RC Xp::EraseFile(const string& FileName)
{
    int ret = unlink(FileName.c_str());

    if (ret != 0)
    {
        Printf(Tee::PriNormal, "Failed to delete file %s\n", FileName.c_str() );

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

#if !defined(MACOSX_MFG)
// Implemented below MacReadRegistry...
static RC GetRegistryCF(CFTypeRef *returned, const char *name, const char *key);

/**
 * We're reading from the I/O Registry and trying to match Node with the value
 * stored in the "name" field of a registry entry.  If we find such an entry,
 * we look at the field specified by Key and copy that data to *pData.
 */
static RC MacReadRegistry(const char * Node, const char * Key, string * pData)
{
    MASSERT(pData != 0);

    CFTypeRef typeRef = NULL;

    RC result = GetRegistryCF(&typeRef, Node, Key);
    if (result != OK)
    {
        return result;
    }

    // If typeRef is a CFData object...
    if (CFGetTypeID(typeRef) == CFDataGetTypeID())
    {
        CFDataRef dataRef = (CFDataRef) typeRef;
        char *buffer = new char[CFDataGetLength(dataRef)];
        CFDataGetBytes(dataRef,
                       CFRangeMake(0, CFDataGetLength(dataRef)),
                       reinterpret_cast<UInt8*>(buffer));
        *pData = buffer; // =overloading makes a copy
        delete[] buffer;
    }

    // If typeRef is a CFNumber object...
    if (CFGetTypeID(typeRef) == CFNumberGetTypeID())
    {
        CFNumberRef numRef = (CFNumberRef) typeRef;
        INT32 number;
        char buffer[256];
        CFNumberGetValue(numRef, kCFNumberSInt32Type, &number);
        sprintf(buffer, "0x%08x", number);
        *pData = buffer; // =overloading makes a copy
    }

    // If typeRef is a CFString object...
    else if (CFGetTypeID(typeRef) == CFStringGetTypeID())
    {
        CFStringRef strRef = (CFStringRef) typeRef;
        CFIndex bufSize = CFStringGetLength(strRef);
        char *buffer = new char[bufSize];
        if (CFStringGetCString(strRef, buffer, bufSize,
                               CFStringGetSystemEncoding()))
        {
            *pData = buffer; // =overloading makes a copy
            delete[] buffer;
        }
        else
        {
            CFRelease(typeRef);
            delete[] buffer;
            return RC::REGISTRY_ERROR;
        }
    }
    // If typeRef is anything other than a CFData or CFString...
    else
    {
        CFRelease(typeRef);
        Printf(Tee::PriError,
               "ReadRegistry(char*, char*, string *) accessed a registry value that was not an OSData or OSString.\n");
        return RC::REGISTRY_KEY_NOT_FOUND;
    }
    CFRelease(typeRef);

    return OK;
}

/**
 * Iterates over the registry until it finds a registry entry whose
 * "name" field contains a value that matches name (case-sensitive).
 * Then, looks at the key field of that entry, gets a CF object
 * from that field, and sets *returned equal to that CF object.
 *
 * @return Possible values:
 *           - OK ==> success
 *           - RC::REGISTRY_KEY_NOT_FOUND ==> entry or key not found
 *           - RC::REGISTRY_ERROR ==> other error
 */
static RC GetRegistryCF(CFTypeRef *returned, const char *name, const char *key)
{
    IOReturn       kr         = 0;
    mach_port_t    masterPort = MACH_PORT_NULL;
    io_iterator_t  iterator   = IO_OBJECT_NULL;
    io_object_t    obj        = IO_OBJECT_NULL;
    CFTypeRef      typeRef    = NULL;
    char          *dynStr     = NULL;

    kr = IOMasterPort(bootstrap_port, &masterPort);
    if (kr)
        return RC::REGISTRY_ERROR;

    kr = IORegistryCreateIterator(masterPort, kIOServicePlane,
                                  kIORegistryIterateRelwrsively, &iterator);
    if (kr || !(IOIteratorIsValid(iterator)))
        return RC::REGISTRY_ERROR;

    // @@@ POSSIBLE BUG:
    // Assumes that "name" will hold a CFData (this is almost
    // always the case).  Will skip anything whose "name"
    // holds something else.
    while (0 != (obj = IOIteratorNext(iterator)))
    {
        // Get the "name" data, ensure it exists and is a CFData
        typeRef = IORegistryEntryCreateCFProperty(obj, CFSTR("name"),
                                                  NULL, 0);
        if (!typeRef || CFGetTypeID(typeRef) != CFDataGetTypeID())
        {
            IOObjectRelease(obj);
            if (typeRef)
                CFRelease(typeRef);
            continue;
        }
        CFDataRef dataRef = (CFDataRef) typeRef;

        // Compare the name to the Node string.
        int len = CFDataGetLength(dataRef);
        dynStr = new char[len];
        CFDataGetBytes(dataRef, CFRangeMake(0, len), (UInt8 *)dynStr);

        if (0 == strcmp(name, dynStr))
        {
            delete[] dynStr;
            CFRelease(typeRef);
            break;
        }

        delete[] dynStr;
        CFRelease(typeRef);
        IOObjectRelease(obj);
    }
    IOObjectRelease(iterator);

    // If obj != 0, then obj==the reg. entry we care about.
    if (obj == 0)
    {
        return RC::REGISTRY_KEY_NOT_FOUND;
    }
    else
    {
        CFStringRef tempStrRef = CFStringCreateWithCString(
                NULL, key, CFStringGetSystemEncoding());
        typeRef = IORegistryEntryCreateCFProperty(obj, tempStrRef, NULL, 0);

        IOObjectRelease(obj);
        if (!typeRef)
        {
            return RC::REGISTRY_KEY_NOT_FOUND;
        }

        *returned = typeRef;
        return OK;
    }
}

C_(Global_MacReadRegistryString);
static STest Global_MacReadRegistryString
(
    "MacReadRegistryString",
    C_Global_MacReadRegistryString,
    3,
    "Read a string from the registry."
);

// STest
C_(Global_MacReadRegistryString)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the argument.
    string     Node;
    string     Key;
    JSObject * pReturlwals;
    if
    (
         (NumArguments != 3)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Node))
      || (OK != pJavaScript->FromJsval(pArguments[1], &Key))
      || (OK != pJavaScript->FromJsval(pArguments[2], &pReturlwals))
    )
    {
        JS_ReportError(pContext,
                       "Usage: MacReadRegistryString(node, key, [data])");
        return JS_FALSE;
    }

    RC     rc = OK;
    string Data;

    C_CHECK_RC(MacReadRegistry(Node.c_str(), Key.c_str(), &Data));
    C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 0, Data));

    RETURN_RC(OK);
}
#endif

UINT64 Xp::QueryPerformanceCounter()
{
    return mach_absolute_time();
}

UINT64 Xp::QueryPerformanceFrequency()
{
    static UINT64 freq = 0;
    if (freq == 0)
    {
        mach_timebase_info_data_t info;
        mach_timebase_info(&info);
        freq = static_cast<UINT64>(1e9 * info.denom / info.numer);
    }
    return freq;
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

UINT64 Xp::GetWallTimeNS()
{
    static UINT64 s_StartTime            = 0;
    static bool   s_StartTimeInitialized = false;

    if (!s_StartTimeInitialized)
    {
        s_StartTime            = mach_absolute_time();
        s_StartTimeInitialized = true;
        return 0;
    }

    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
    return (mach_absolute_time() - s_StartTime) * info.numer / info.denom;
}

UINT64 Xp::GetWallTimeUS()
{
    return GetWallTimeNS() / 1000U;
}

UINT64 Xp::GetWallTimeMS()
{
    return GetWallTimeNS() / 1000000U;
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
    string variable,
    string value
)
{
    const int ret = setelw(variable.c_str(), value.c_str(), 1);
    if (ret)
        return RC::UNSUPPORTED_FUNCTION;
    else
        return OK;
}

/**
 * @note Does not flush the OS cache to disk.  Sorry.
 */
void Xp::FlushFstream(FILE *pFile)
{

    // Flush the output buffer to the operating system.
    fflush(pFile);
}

/**
 *------------------------------------------------------------------------------
 * @function(Xp::ValidateGpuInterrupt)
 *
 * Confirm that the GPU's interrupt mechanism is working.
 *------------------------------------------------------------------------------
 */

#if !defined(MACOSX_MFG) || !defined(INCLUDE_GPU)
RC Xp::ValidateGpuInterrupt( GpuSubdevice  *pGpuSubdevice,
                             UINT32 whichInterrupts)
{
    // Not yet implemented on this OS.
    return OK;
}
#endif

//! \brief Checks write permission for the defined file
//!
//! Check write permission on the file, returning OK if writeable
RC Xp::CheckWritePerm(const char *FileName)
{
    string path = Utility::StripFilename(FileName);
    if(path.length() == 0) path=".";
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
RC Xp::StartBlinkLightsThread()
{
    return RC::UNSUPPORTED_FUNCTION;
}

#if !defined(MACOSX_MFG) || !defined(INCLUDE_GPU)
//------------------------------------------------------------------------------
// Get the OS version
// returns UNSUPPORTED_FUNCTION
// -----------------------------------------------------------------------------
RC Xp::GetOSVersion(UINT64* pVer)
{
    return RC::UNSUPPORTED_FUNCTION;
}
#endif

/**
 *-----------------------------------------------------------------------------
 * @function(SwitchSystemPowerState)
 *
 * Lwrrently stubbed out, returns error
 *-----------------------------------------------------------------------------
 */
RC Xp::SwitchSystemPowerState(bool bPowerState, INT32 iSleepTimeInSec)
{
    MASSERT(!"Break in Xp::SwitchSystemPowerState, Unsupported yet on MAC");
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
        !"Break in Xp::GetOsDisplaySettingsBufferSize, Unsupported yet on MAC"
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
    MASSERT(!"Break in Xp::GetDisplaySettings, Unsupported yet on MAC");
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
    MASSERT(!"Break in Xp::SetDisplaySettings, Unsupported yet on MAC");
    return RC::SOFTWARE_ERROR;
}

//------------------------------------------------------------------------------
void * Xp::GetVbiosFromLws(UINT32 GpuInst)
{
    return NULL;
}
//------------------------------------------------------------------------------
Xp::OptimusMgr* Xp::GetOptimusMgr
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function
)
{
    return 0;
}
//------------------------------------------------------------------------------
Xp::JTMgr* Xp::GetJTMgr(UINT32 GpuInstance)
{
    return 0;
}
void Xp::CleanupJTMgr()
{
    return;
}

//------------------------------------------------------------------------------
RC Xp::ReadChipId(UINT32 * fuseVal240, UINT32 * fuseVal244)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//-----------------------------------------------------------------------------
Xp::ClockManager* Xp::GetClockManager()
{
    Printf(Tee::PriLow,
           "System-level clock setting is not supported on this platform\n");
    return 0;
}

/**
 *-----------------------------------------------------------------------------
 * @function(GetVirtualMemoryUsed)
 *-----------------------------------------------------------------------------
 */
size_t Xp::GetVirtualMemoryUsed()
{
    MASSERT(!"Break in Xp::GetVirtualMemoryUsed, Unsupported yet on MAC");
    return 0;
}

// Stub functions for watchdog functionality
RC Xp::InitWatchdogs(UINT32 TimeoutSecs)
{
    Printf(Tee::PriLow, "Skipping watchdog initialization since "
                        "it is not supported\n");
    return OK;
}

RC Xp::ResetWatchdogs()
{
    Printf(Tee::PriLow, "Skipping watchdog resetting since it is"
                        " not supported\n");
    return OK;
}

RC Xp::GetBoardInfo(BoardInfo* pBoardInfo)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
// Determine if running in Hypervisor.
//
bool Xp::IsInHypervisor()
{
    return false;
}

//------------------------------------------------------------------------------
RC Xp::ReadSystemUUID(string *SystemUUID)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
RC Xp::ReadHDiskSerialnum(string *HdiskSerno)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
//! \brief Get the CPU mask of CPUs that are local to the specified PCI device
//!        for NUMA enabled systems
//!
//! \param BusNumber      : PCI bus number of the device.
//! \param DeviceNumber   : PCI device number of the device
//! \param FunctionNumber : PCI function number of the device
//! \param pLocalCpuMasks : (NOT SUPPORTED) Empty vector returned
//!
//! \return OK
RC Xp::GetDeviceLocalCpus
(
    INT32 DomainNumber,
    INT32 BusNumber,
    INT32 DeviceNumber,
    INT32 FunctionNumber,
    vector<UINT32> *pLocalCpuMasks
)
{
    MASSERT(pLocalCpuMasks);
    pLocalCpuMasks->clear();
    return OK;
}

INT32 Xp::GetDeviceNumaNode
(
    INT32 domainNumber,
    INT32 busNumber,
    INT32 deviceNumber,
    INT32 functionNumber
)
{
    return -1;
}

//------------------------------------------------------------------------------
bool Xp::HasFeature(Feature feat)
{
    switch (feat)
    {
        case HAS_KERNEL_LEVEL_ACCESS:
            return Platform::HasClientSideResman();
        case HAS_UVA_SUPPORT:
            return false;
        default:
            MASSERT(!"Not implemented");
            return false;
    }
}

//------------------------------------------------------------------------------
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

//-----------------------------------------------------------------------------
RC Xp::SendMessageToFTB
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    const vector<tuple<UINT32, UINT32>>& messages
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//-----------------------------------------------------------------------------
RC Xp::ConfirmUsbAltModeConfig
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    UINT08 desiredAltMode
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::EnableUsbAutoSuspend
(
    UINT32 hcPciDomain,
    UINT32 hcPciBus,
    UINT32 hcPciDevice,
    UINT32 hcPciFunction,
    UINT32 autoSuspendDelayMs
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//-----------------------------------------------------------------------------
RC Xp::DisablePciDeviceDriver
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    string* pDriverName
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::EnablePciDeviceDriver
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    const string& driverName
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::GetLwrrUsbAltMode
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    UINT08 *pLwrrAltMode
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::GetUsbPowerConsumption
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    FLOAT64 *pVoltageVolt,
    FLOAT64 *pLwrrentAmp
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::GetPpcFwVersion
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    string *pPrimaryVersion,
    string *pSecondaryVersion
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::GetPpcDriverVersion
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    string *pDriverVersion
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::GetFtbFwVersion
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    string *pFtbFwVersion
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
RC Xp::RdMsrOnCpu(UINT32 cpu, UINT32 reg, UINT32 *pHigh, UINT32 *pLow)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::WrMsrOnCpu(UINT32 cpu, UINT32 reg, UINT32 high, UINT32 low)
{
    return RC::UNSUPPORTED_FUNCTION;
}

FLOAT64 Xp::GetCpuUsageSec()
{
    // TODO
    return 0;
}

UINT32 Xp::GetNumCpuCores()
{
    return static_cast<UINT32>(sysconf(_SC_NPROCESSORS_ONLN));
}
