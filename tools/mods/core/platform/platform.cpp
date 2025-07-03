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

// Platform object, properties, and methods.
// Common (hw and sim) platform functionality

#include "core/include/platform.h"
#include "pltfmpvt.h"
#include "core/include/device.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/massert.h"
#include "core/include/xp.h"
#include "core/include/tasker.h"
#include "core/include/simpleui.h"
#include "core/include/cmdline.h"
#include "core/include/assertinfosink.h"
#include "core/include/log.h"
#include "core/include/cpu.h"
#include "core/include/js_uint64.h"
#ifdef INCLUDE_GPU
#include "core/include/mgrmgr.h"
#include "gpu/include/testdevicemgr.h"
#endif

#include <atomic>
#include <algorithm>
#include <set>
#include <vector>

//------------------------------------------------------------------------------
// Common (hw and sim) functionality
//------------------------------------------------------------------------------

// Vector of cleanup callback functions and their data
static vector< pair<void (*)(void *), void *> > s_CleanupCallbackFnsAndCtx;

static bool     s_InteractOnBreakPoint = false;
static bool     s_PrintSystemTime = false;
static bool     s_DumpCallStack = true;
static UINT32   s_TlsIdx = 0;
static bool     s_Use4ByteMemCopy = false;
static bool     s_SkipDisableUserInterface = false;
static bool     s_ForceVgaPrint = false;
static bool     s_DumpPciAccess = false;
static UINT32   s_MaxAllocPagesAddressBits = 64;
static bool     s_ForcedTermination = false;
static volatile INT32 s_BreakPointsDisabled = 0;
static bool     s_IsCCMode = false;

// List of breakpoints to ignore, set by -ignore_assert
struct BreakPointToIgnore
{
    string file;   // Filename (does not include directory)
    int line;      // Line number, or IGNORE_ALL_LINES
    bool operator<(const BreakPointToIgnore &rhs) const
        { return (line == rhs.line) ? (file < rhs.file) : (line < rhs.line); }
};
static set<BreakPointToIgnore> s_BreakPointsToIgnore;

//------------------------------------------------------------------------------
bool Platform::IsCCMode()
{
    return s_IsCCMode;
}

//------------------------------------------------------------------------------
void Platform::SetForcedTermination()
{
    s_ForcedTermination = true;
}

//------------------------------------------------------------------------------
bool Platform::IsForcedTermination()
{
    return s_ForcedTermination;
}

//------------------------------------------------------------------------------
void Platform::AddCleanupCallback(void (*pCleanupFn)(void *), void *context)
{
    s_CleanupCallbackFnsAndCtx.push_back(make_pair(pCleanupFn, context));
}

//------------------------------------------------------------------------------
void Platform::CallAndClearCleanupFunctions()
{
    for(INT32 i = (INT32)s_CleanupCallbackFnsAndCtx.size() - 1; i >= 0; --i)
    {
        pair<void (*)(void *), void *> *p = &s_CleanupCallbackFnsAndCtx[i];
        p->first(p->second);
    }

    s_CleanupCallbackFnsAndCtx.clear();
}

static int s_NumDisables = 0;

//----------------------------------------------------------------------------
RC Platform::DisableUserInterface()
{
    RC rc;

    if (s_SkipDisableUserInterface)
    {
        return OK;
    }

    ++s_NumDisables;

    if (s_NumDisables == 1)
    {
        if (Platform::IsInitialized() && (Hardware == GetSimulationMode()))
            rc = Xp::DisableUserInterface();
    }
    else
    {
        rc = OK;
    }
    return rc;
}

//----------------------------------------------------------------------------
bool Platform::IsInHypervisor()
{
    return Xp::IsInHypervisor();
}

//----------------------------------------------------------------------------
RC Platform::EnableUserInterface(GpuDevice * /*unused*/)
{
    // Obsolete version, delete me eventually!
    return EnableUserInterface();
}

//----------------------------------------------------------------------------
RC Platform::EnableUserInterface()
{
    RC rc;

    if (s_SkipDisableUserInterface)
    {
        return OK;
    }

    --s_NumDisables;

    if (s_NumDisables == 0)
    {
        if (Platform::IsInitialized() && (Hardware == GetSimulationMode()))
            rc = Xp::EnableUserInterface();
    }
    else if (s_NumDisables < 0)
    {
        MASSERT(!"Too many calls to Platform::EnableUserInterface!");
        s_NumDisables = 0;
        rc = RC::SOFTWARE_ERROR;
    }
    else
    {
        rc = OK;
    }
    return rc;
}

bool Platform::IsPPC()
{
#if defined(PPC64LE)
    return true;
#else
    return false;
#endif
}

//------------------------------------------------------------------------------
struct BpHook
{
    Platform::BreakPointHandler pFunction;
    void * pData;
};

// anonymous namespace functions.
namespace
{

    // Don't access these functions directly instead use the BreakPointHooker
    // class to properly manage nested hookers.

    //! Add a per-thread handler for BreakPoint calls. Returns existing hook
    //! if one exists, NULL otherwise.
    void * HookBreakPoint
    (
        Platform::BreakPointHandler pFunction,
        void * pData
    )
    {
        BpHook * pOrigHook = NULL;

        if (0 == s_TlsIdx)
            s_TlsIdx = Tasker::TlsAlloc();
        else
            pOrigHook = (BpHook*)Tasker::TlsGetValue(s_TlsIdx);

        BpHook * pHook = new BpHook;
        pHook->pFunction = pFunction;
        pHook->pData = pData;

        Tasker::TlsSetValue(s_TlsIdx, pHook);

        return pOrigHook;
    }

    //! Remove current per-thread handler for BreakPoint calls and restore the
    //! original hook if supplied.
    void UnHookBreakPoint(void * pOrigHook)
    {
        if (0 != s_TlsIdx)
        {
            BpHook * pHook = (BpHook *)Tasker::TlsGetValue(s_TlsIdx);
            delete pHook;
            Tasker::TlsSetValue(s_TlsIdx, pOrigHook);
        }
    }

}

//------------------------------------------------------------------------------
Platform::BreakPointHooker::BreakPointHooker()
    : m_Hooked(false), m_pOrigHook(NULL)
{
}

//------------------------------------------------------------------------------
Platform::BreakPointHooker::BreakPointHooker
(
    BreakPointHandler pFunction,
    void* pdata
)
{
    Hook(pFunction, pdata);
}

//------------------------------------------------------------------------------
void Platform::BreakPointHooker::Hook
(
    BreakPointHandler pFunction,
    void* pdata
)
{
    m_pOrigHook = HookBreakPoint(pFunction, pdata);
    m_Hooked = true;
}

//------------------------------------------------------------------------------
Platform::BreakPointHooker::~BreakPointHooker()
{
    if (m_Hooked)
        UnHookBreakPoint(m_pOrigHook);
}

//------------------------------------------------------------------------------
Platform::DisableBreakPoints::DisableBreakPoints()
{
    Cpu::AtomicAdd(&s_BreakPointsDisabled, 1);
}

//------------------------------------------------------------------------------
Platform::DisableBreakPoints::~DisableBreakPoints()
{
    Cpu::AtomicAdd(&s_BreakPointsDisabled, -1);
}

//------------------------------------------------------------------------------
#ifndef SIM_BUILD
namespace
{
    // Return true if we're ignoring breakpoints at the indicated file
    // and line-number
    //
    bool IsBreakPointIgnored(const char *file, int line)
    {
        // Strip the directory from the filename
        //
        const char *baseName = file;
        const char *firstSlash = strrchr(baseName, '/');
        if (firstSlash != nullptr)
        {
            baseName = firstSlash + 1;
        }
        firstSlash = strrchr(baseName, '\\');
        if (firstSlash != nullptr)
        {
            baseName = firstSlash + 1;
        }

        // Look up the file/line in s_BreakPointsToIgnore
        //
        BreakPointToIgnore breakPoint = {baseName, line};
        if (s_BreakPointsToIgnore.count(breakPoint))
        {
            return true;
        }
        breakPoint.line = Platform::IGNORE_ALL_LINES; // Not found; try wildcard
        if (s_BreakPointsToIgnore.count(breakPoint))
        {
            return true;
        }
        return false;
    }
}
#endif

namespace Platform
{
    atomic<bool> g_ModsExitOnBreakPoint{false};
}

//------------------------------------------------------------------------------
void Platform::BreakPoint(RC reasonRc, const char *file, int line)
{
    // If a break point is encountered during shutdown oclwrring due to
    // another break point, just abort
    if (g_ModsExitOnBreakPoint)
    {
        Printf(Tee::PriError, "Detected a relwrsive breakpoint while handling -exit_on_breakpoint.  Aborting!!\n");
        abort();
    }

    // BreakPoint can be called relwrsively, since we get fancy and try to
    // use mods services such as Tee and Tasker that themselves can BreakPoint.
    // So we NOOP when relwrsion is detected.

    static atomic<bool> lwrsed(false);

    bool expected = false;
    if (!lwrsed.compare_exchange_strong(expected, true))
    {
        return;  // No re-lwrsing.
    }

    DEFER { lwrsed = false; };

#ifdef SIM_BUILD
    const bool ignoreBreakPoint = false;
#else
    const bool ignoreBreakPoint = (!CommandLine::FailOnAssert() ||
                                   IsBreakPointIgnored(file, line));
#endif

    if (0 != s_TlsIdx)
    {
        BpHook * pHook = (BpHook *)Tasker::TlsGetValue(s_TlsIdx);
        if (pHook)
        {
            // Call breakpoint handler function for this thread.
            (*pHook->pFunction)(pHook->pData);
        }
    }

// The Windows DX team prefers that the process always ends immediately  on
// breakpoint (for automated runs):
#if defined(SERVER_MODE) || (defined(DEBUG) && !defined(SANITY_BUILD))
    if (!ignoreBreakPoint)
    {
        if (Cpu::AtomicAdd(&s_BreakPointsDisabled, 0) <= 0)
        {
            // Bring up debugger (if present), or crash the program (if not).
            Xp::BreakPoint();
        }
    }
#else

    string devName;
#ifdef INCLUDE_GPU
    const INT32 lwrDevInst = ErrorMap::DevInst();
    if (lwrDevInst >= 0)
    {
        Device* pDevice = nullptr;
        if (static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr)->GetDevice(
                    static_cast<UINT32>(lwrDevInst), &pDevice) == RC::OK)
        {
            MASSERT(pDevice);
            devName = " on ";
            devName += pDevice->GetName();
        }
    }
#endif

    printf("\n** ModsDrvBreakPoint%s **\n", devName.c_str());
    fflush(stdout);

    if (ignoreBreakPoint)
    {
        Printf(Tee::PriWarn, "The following breakpoint is being ignored!\n");
    }
    Printf(Tee::PriNormal, "\n");
    Printf(Tee::PriError, "** ModsDrvBreakPoint%s **\n", devName.c_str());

    if (Tee::GetAssertInfoSink())
    {
        // Only flush the assertinfo level log if the spew does not already go there
        // Prevents duplication
        if (Tee::GetSerialLevel() > Tee::GetAssertInfoLevel() &&
            Tee::GetSerialLevel() != Tee::LevNone)
        {
            Tee::GetAssertInfoSink()->Dump(Tee::SerialOnly);
        }
        if (Tee::GetFileLevel() > Tee::GetAssertInfoLevel() &&
            Tee::GetFileLevel() != Tee::LevNone)
        {
            Tee::GetAssertInfoSink()->Dump(Tee::FileOnly);
        }
        if (Tee::GetScreenLevel() > Tee::GetAssertInfoLevel() &&
            Tee::GetScreenLevel() != Tee::LevNone)
        {
            Tee::GetAssertInfoSink()->Dump(Tee::ScreenOnly);
        }
        if (Tee::GetMleLevel() > Tee::GetAssertInfoLevel() &&
            Tee::GetMleLevel() != Tee::LevNone)
        {
            Tee::GetAssertInfoSink()->Dump(Tee::MleOnly);
        }
    }

    if (!ignoreBreakPoint)
    {
        Log::AddAssertFailure(reasonRc);
    }

#endif

    // OK, now let's get really optimistic...
    if (s_InteractOnBreakPoint)
    {
        Printf(Tee::ScreenOnly,
            "InteractOnBreakPoint: type Exit() to continue\n");
        Printf(Tee::ScreenOnly,
            "  (in case of hang, try -poll_interrupts next time.)\n");

        SimpleUserInterface * pInterface = new SimpleUserInterface(true);
        pInterface->Run(false/*AllowYield*/);
        delete pInterface;
    }
}

//----------------------------------------------------------------------------
//! Ignore breakpoints at the indicated file & line
//!
//! \param file The file name, not including the directory
//! \param line Line number of the breakpoint, or IGNORE_ALL_LINES
//!
RC Platform::IgnoreBreakPoint(const string &file, int line)
{
    BreakPointToIgnore breakPoint = {file, line};
    s_BreakPointsToIgnore.insert(breakPoint);
    return OK;
}

//----------------------------------------------------------------------------
bool Platform::IsUserInterfaceDisabled()
{
    return (s_NumDisables > 0);
}

//----------------------------------------------------------------------------
bool Platform::GetUse4ByteMemCopy ()
{
    return s_Use4ByteMemCopy;
}
RC Platform::SetUse4ByteMemCopy (bool b)
{
    s_Use4ByteMemCopy = b;
    return OK;
}
bool Platform::HasWideMemCopy()
{
    if (GetUse4ByteMemCopy())
        return false;
    return Cpu::HasWideMemCopy();
}

//----------------------------------------------------------------------------
// Some GPUs can hang if a larger-than-4-byte read of BAR1 is in progress when
// a config-read oclwrs.  The system memcpy uses 16-byte SSE load instructions.
//
// See http://lwbugs/520341.
//
// This is a replacement memcpy that avoids using SSE instructions, but is
// slightly better than a simple byte-copy loop.
//
void Platform::Hw4ByteMemCopy
(
    volatile void *         Dst,
    const volatile void *   Src,
    size_t                  Count
)
{
    volatile UINT08 *       d8 = static_cast<volatile UINT08*>(Dst);
    const volatile UINT08 * s8 = static_cast<const volatile UINT08*>(Src);

    const size_t misalign = (d8 - (UINT08*)0) % sizeof(UINT32);
    if (misalign)
    {
        // Copy byte-by-byte until dst pointer is 4-byte aligned.
        const size_t byteCopyCount = min (Count, sizeof(UINT32) - misalign);

        for (size_t i = 0; i < byteCopyCount; i++)
        {
            *d8 = *s8;
            ++d8;
            ++s8;
        }
        Count -= byteCopyCount;
        Dst = d8;
        Src = s8;
    }

    if (Count >= sizeof(UINT32))
    {
        // Copy dword-by-dword until Count is < 4.

        volatile UINT32 *       d32 = static_cast<volatile UINT32*>(Dst);
        const volatile UINT32 * s32 = static_cast<const volatile UINT32*>(Src);

        // Loop unwind -- maybe the compiler is smart enough to do this anyway?
        while (Count >= 4*sizeof(UINT32))
        {
            d32[0] = s32[0];
            d32[1] = s32[1];
            d32[2] = s32[2];
            d32[3] = s32[3];
            d32 += 4;
            s32 += 4;
            Count -= 4*sizeof(UINT32);
        }
        while (Count >= sizeof(UINT32))
        {
            *d32 = *s32;
            ++d32;
            ++s32;
            Count -= sizeof(UINT32);
        }
        Dst = d32;
        Src = s32;
    }

    // Copy last 0 to 3 bytes.

    d8 = static_cast<volatile UINT08*>(Dst);
    s8 = static_cast<const volatile UINT08*>(Src);
    while (Count)
    {
        *d8 = *s8;
        ++d8;
        ++s8;
        Count -= 1;
    }
}

//----------------------------------------------------------------------------
bool Platform::GetSkipDisableUserInterface()
{
    return s_SkipDisableUserInterface;
}
RC Platform::SetSkipDisableUserInterface(bool b)
{
    s_SkipDisableUserInterface = b;
    return OK;
}
//----------------------------------------------------------------------------
bool Platform::GetForceVgaPrint()
{
    return s_ForceVgaPrint;
}
RC Platform::SetForceVgaPrint(bool ToForce)
{
    if (!s_ForceVgaPrint && ToForce)
    {
        Printf(Tee::PriWarn,
               "forcing VGA prints can lead to FB corruption!\n");
    }
    s_ForceVgaPrint = ToForce;
    return OK;
}
//----------------------------------------------------------------------------
bool Platform::GetDumpPciAccess()
{
    return s_DumpPciAccess;
}
RC Platform::SetDumpPciAccess(bool b)
{
    s_DumpPciAccess = b;
    return OK;
}
//----------------------------------------------------------------------------
RC Platform::SetMaxAllocPagesAddressBits(UINT32 bits)
{
    if (bits > 64)
        return RC::UNSUPPORTED_FUNCTION;

    s_MaxAllocPagesAddressBits = bits;
    return OK;
}

UINT32 Platform::GetAllowedAddressBits(UINT32 AddressBits)
{
    if (AddressBits > s_MaxAllocPagesAddressBits)
    {
        return s_MaxAllocPagesAddressBits;
    }
    return AddressBits;
}

//----------------------------------------------------------------------------
bool Platform::DumpCallStack()
{
    return s_DumpCallStack;
}

//----------------------------------------------------------------------------
string Platform::SimulationModeToString(Platform::SimulationMode mode)
{
    string modeStr;
    switch(mode)
    {
        case Platform::Hardware:
        {
            modeStr = "Hardware";
            break;
        }
        case Platform::RTL:
        {
            modeStr = "RTL";
            break;
        }
        case Platform::Fmodel:
        {
            modeStr = "Fmodel";
            break;
        }
        case Platform::Amodel:
        {
            modeStr = "Amodel";
            break;
        }
        default:
        {
            MASSERT(!"Invalid input mode");
            modeStr = "ILWALID_MODE";
            break;
        }
    }
    return modeStr;
}

//------------------------------------------------------------------------------
// Javascript linkage
//------------------------------------------------------------------------------

JS_CLASS(Platform);

SObject Platform_Object
(
    "Platform",
    PlatformClass,
    0,
    0,
    "Platform interface."
);

SProperty Platform_SwapEndian
(
    Platform_Object,
    "SwapEndian",
    0,
    false,
    0,
    0,
    0,
    "Run the hardware in big endian mode on a little endian computer."
);

P_(C_Platform_Get_IsChiplibLoaded);
SProperty Platform_IsChiplibLoaded
(
    Platform_Object,
    "IsChiplibLoaded",
    0,
    0,
    C_Platform_Get_IsChiplibLoaded,
    0,
    JSPROP_READONLY,
    "Have we loaded a chiplib"
);

P_(C_Platform_Get_HasClientSideResman);
SProperty Platform_HasClientSideResman
(
    Platform_Object,
    "HasClientSideResman",
    0,
    0,
    C_Platform_Get_HasClientSideResman,
    0,
    JSPROP_READONLY,
    "Has a client side Resman"
);

P_(C_Platform_Get_HasClientSideLwLink);
SProperty Platform_HasClientSideLwLink
(
    Platform_Object,
    "HasClientSideLwLink",
    0,
    0,
    C_Platform_Get_HasClientSideLwLink,
    0,
    JSPROP_READONLY,
    "Has a client side LwLink driver"
);

P_(C_Platform_Get_HasClientSideLwSwitch);
SProperty Platform_HasClientSideLwSwitch
(
    Platform_Object,
    "HasClientSideLwSwitch",
    0,
    0,
    C_Platform_Get_HasClientSideLwSwitch,
    0,
    JSPROP_READONLY,
    "Has a client side LwSwitch driver"
);

P_(C_Platform_Get_SimulationMode);
SProperty Platform_SimulationMode
(
    Platform_Object,
    "SimulationMode",
    0,
    0,
    C_Platform_Get_SimulationMode,
    0,
    JSPROP_READONLY,
    "The type of simulator, if any, that is loaded"
);

static SProperty Platform_Hardware
(
    Platform_Object,
    "Hardware",
    0,
    Platform::Hardware,
    0, 0, JSPROP_READONLY,
    "Hardware"
);
static SProperty Platform_RTL
(
    Platform_Object,
    "RTL",
    0,
    Platform::RTL,
    0, 0, JSPROP_READONLY,
    "RTL"
);
static SProperty Platform_Fmodel
(
    Platform_Object,
    "Fmodel",
    0,
    Platform::Fmodel,
    0, 0, JSPROP_READONLY,
    "Fmodel"
);
static SProperty Platform_Amodel
(
    Platform_Object,
    "Amodel",
    0,
    Platform::Amodel,
    0, 0, JSPROP_READONLY,
    "Amodel"
);

P_(C_Platform_Get_IsPhysFunMode);
static SProperty Platform_IsPhysFunMode
(
    Platform_Object,
    "IsPhysFunMode",
    0,
    0,
    C_Platform_Get_IsPhysFunMode,
    0,
    JSPROP_READONLY,
    "Get Platform::IsPhysFunMode value"
);

P_(C_Platform_Get_IsVirtFunMode);
static SProperty Platform_IsVirtFunMode
(
    Platform_Object,
    "IsVirtFunMode",
    0,
    0,
    C_Platform_Get_IsVirtFunMode,
    0,
    JSPROP_READONLY,
    "Get Platform::IsVirtFunMode value"
);

P_(C_Platform_IsInHypervisor);
static SProperty Platform_IsInHypervisor
(
    Platform_Object,
    "IsInHypervisor",
    0,
    0,
    C_Platform_IsInHypervisor,
    0,
    JSPROP_READONLY,
    "Check if running in Hypervisor."
);

//
// Methods and Tests
//

//------------------------------------------------------------------------------
C_(Platform_Initialize);
static STest Platform_Initialize
(
    Platform_Object,
    "Initialize",
    C_Platform_Initialize,
    0,
    "Set up the hardware or simulation platform."
);

C_(Platform_MapDeviceMemory);
static SMethod Platform_MapDeviceMemory
(
    Platform_Object,
    "MapDeviceMemory",
    C_Platform_MapDeviceMemory,
    0,
    "Map a given range of physical addresses."
);

C_(Platform_UnMapDeviceMemory);
static SMethod Platform_UnMapDeviceMemory
(
    Platform_Object,
    "UnMapDeviceMemory",
    C_Platform_UnMapDeviceMemory,
    0,
    "Unmap a given range of physical addresses."
);

C_(Platform_EscapeRead);
static SMethod Platform_EscapeRead
(
    Platform_Object,
    "EscapeRead",
    C_Platform_EscapeRead,
    0,
    "Do an escape read to a simulator."
);

C_(Platform_EscapeWrite);
static STest Platform_EscapeWrite
(
    Platform_Object,
    "EscapeWrite",
    C_Platform_EscapeWrite,
    0,
    "Do an escape write to a simulator."
);

C_(Platform_Delay);
static STest Platform_Delay
(
    Platform_Object,
    "Delay",
    C_Platform_Delay,
    0,
    "Delay a given number of microseconds."
);

C_(Platform_ClockSimulator);
static STest Platform_ClockSimulator
(
    Platform_Object,
    "ClockSimulator",
    C_Platform_ClockSimulator,
    0,
    "Clock the simulator a given number of clock cycles."
);

C_(Platform_SimulationModeToString);
static SMethod Platform_SimulationModeToString
(
    Platform_Object,
    "SimulationModeToString",
    C_Platform_SimulationModeToString,
    0,
    "Colwert a Platform::SimulationMode value to a string."
);

// SProperty
P_(C_Platform_Get_IsPhysFunMode)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    bool loaded = Platform::IsPhysFunMode();
    if (OK != JavaScriptPtr()->ToJsval(loaded, pValue))
    {
        JS_ReportError(pContext, "Failed to colwert value in Platform.IsPhysFunMode");
        return JS_FALSE;
    }
    return JS_TRUE;
}

P_(C_Platform_Get_IsVirtFunMode)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    bool loaded = Platform::IsVirtFunMode();
    if (OK != JavaScriptPtr()->ToJsval(loaded, pValue))
    {
        JS_ReportError(pContext, "Failed to colwert value in Platform.IsVirtFunMode");
        return JS_FALSE;
    }
    return JS_TRUE;
}

P_(C_Platform_IsInHypervisor)
{
    MASSERT(pContext != 0);
    MASSERT(pValue != 0);

    bool loaded = Platform::IsInHypervisor();
    if (OK != JavaScriptPtr()->ToJsval(loaded, pValue))
    {
        JS_ReportError(pContext, "Error oclwrred in IsInHypervisor().");
        return JS_FALSE;
    }

    return JS_TRUE;
}

P_(C_Platform_Get_IsChiplibLoaded)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    bool loaded = Platform::IsChiplibLoaded();
    if (OK != JavaScriptPtr()->ToJsval(loaded, pValue))
    {
        JS_ReportError(pContext, "Failed to colwert value in Platform.IsChiplibLoaded");
        return JS_FALSE;
    }
    return JS_TRUE;
}

// SMethod
P_(C_Platform_Get_HasClientSideResman)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    bool clientSide = Platform::HasClientSideResman();
    if (OK != JavaScriptPtr()->ToJsval(clientSide, pValue))
    {
        JS_ReportError(pContext, "Failed to colwert value in Platform.HasClientSideResman");
        return JS_FALSE;
    }
    return JS_TRUE;
}

// SMethod
P_(C_Platform_Get_HasClientSideLwLink)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    bool clientSide = Platform::HasClientSideLwLink();
    if (OK != JavaScriptPtr()->ToJsval(clientSide, pValue))
    {
        JS_ReportError(pContext, "Failed to colwert value in Platform.HasClientSideLwLink");
        return JS_FALSE;
    }
    return JS_TRUE;
}

// SMethod
P_(C_Platform_Get_HasClientSideLwSwitch)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    bool clientSide = Platform::HasClientSideLwSwitch();
    if (OK != JavaScriptPtr()->ToJsval(clientSide, pValue))
    {
        JS_ReportError(pContext, "Failed to colwert value in Platform.HasClientSideLwSwitch");
        return JS_FALSE;
    }
    return JS_TRUE;
}

// SProperty
P_(C_Platform_Get_SimulationMode)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    Platform::SimulationMode Mode = Platform::GetSimulationMode();
    if (OK != JavaScriptPtr()->ToJsval(Mode, pValue))
    {
        JS_ReportError(pContext, "Failed to colwert value in Platform.SimulationMode");
        return JS_FALSE;
    }
    return JS_TRUE;
}

// STest
C_(Platform_Initialize)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    // Check this is a void method.
    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: Platform.Initialize()");
        return JS_FALSE;
    }

    RETURN_RC(Platform::Initialize());
}

// STest
C_(Platform_MapDeviceMemory)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    // Check the arguments.
    JavaScriptPtr pJavaScript;
    PHYSADDR PhysAddr;
    UINT32 NumBytes;
    UINT32 TmpAttrib;
    UINT32 TmpProtect;
    if ((NumArguments != 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &PhysAddr)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &NumBytes)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &TmpAttrib)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &TmpProtect)))
    {
        JS_ReportError(pContext, "Usage: Platform.MapDeviceMemory(PhysAddr, NumBytes, Attrib, Protect)");
        return JS_FALSE;
    }

    Memory::Attrib Attrib = (Memory::Attrib)TmpAttrib;
    Memory::Protect Protect = (Memory::Protect)TmpProtect;

    // We don't bother remembering this address
    void *VirtualAddr;
    RETURN_RC(Platform::MapDeviceMemory(&VirtualAddr, PhysAddr,
                                        NumBytes, Attrib, Protect));
}

// STest
C_(Platform_UnMapDeviceMemory)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    // Check the arguments.
    JavaScriptPtr pJavaScript;
    PHYSADDR PhysAddr;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &PhysAddr)))
    {
        JS_ReportError(pContext, "Usage: Platform.UnMapDeviceMemory(PhysAddr)");
        return JS_FALSE;
    }

    // Get the address by doing a physical->virtual
    void *VirtualAddr = Platform::PhysicalToVirtual(PhysAddr);
    Platform::UnMapDeviceMemory(VirtualAddr);
    return JS_TRUE;
}

// SMethod
C_(Platform_EscapeRead)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    string Path;
    UINT32 Index;
    UINT32 Size;
    UINT32 Value;
    if ((NumArguments != 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Path)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Index)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Size)))
    {
        JS_ReportError(pContext, "Usage: Platform.EscapeRead(Path, Index, Size)");
        return JS_FALSE;
    }

    Platform::EscapeRead(Path.c_str(), Index, Size, &Value);

    if (OK != pJavaScript->ToJsval(Value, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in Platform.EscapeRead");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

// STest
C_(Platform_EscapeWrite)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    string Path;
    UINT32 Index;
    UINT32 Size;
    UINT32 Value;
    if ((NumArguments != 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Path)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Index)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Size)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &Value)))
    {
        JS_ReportError(pContext, "Usage: Platform.EscapeWrite(Path, Index, Size, Value)");
        return JS_FALSE;
    }

    Platform::EscapeWrite(Path.c_str(), Index, Size, Value);
    RETURN_RC(OK);
}

// STest
C_(Platform_Delay)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    UINT32 Microseconds;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Microseconds)))
    {
        JS_ReportError(pContext, "Usage: Platform.Delay(Microseconds)");
        return JS_FALSE;
    }

    Platform::Delay(Microseconds);
    RETURN_RC(OK);
}

// STest
C_(Platform_ClockSimulator)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    INT32 Cycles;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Cycles)))
    {
        JS_ReportError(pContext, "Usage: Platform.ClockSimulator(Cycles)");
        return JS_FALSE;
    }

    Platform::ClockSimulator(Cycles);
    RETURN_RC(OK);
}

JS_STEST_LWSTOM(Platform, IgnoreBreakPoint, 1,
                "Ignore the breakpoint at the indicated file and line number")
{
    STEST_HEADER(1, 1, "Usage: Platform.IgnoreBreakpoint(\"file[:line]\")");
    STEST_ARG(0, string, filename);
    RC rc;

    size_t colonPos = filename.find_last_of(':');
    if (colonPos == string::npos)
    {
        C_CHECK_RC(Platform::IgnoreBreakPoint(filename,
                                              Platform::IGNORE_ALL_LINES));
    }
    else
    {
        int lineNumber = atoi(filename.substr(colonPos + 1).c_str());
        C_CHECK_RC(Platform::IgnoreBreakPoint(filename.substr(0, colonPos),
                                              lineNumber));
    }
    RETURN_RC(OK);
}

JS_SMETHOD_LWSTOM(Platform, VirtualFindFreeVaInRange, 4,
                  "Find free VA space in the given range")
{
    STEST_HEADER(4, 4, "Usage: Platform.VirtualFindFreeVaInRange(sizeBytes, milwA, maxVA, alignBytes)");
    STEST_ARG(0, JSObject*, sizeBytesObj);
    STEST_ARG(1, JSObject*, milwAObj);
    STEST_ARG(2, JSObject*, maxVAObj);
    STEST_ARG(3, JSObject*, alignBytesObj);
    RC rc;

    // Cast from generic JS object to the UINT64 JS object
    JsUINT64* pJsSizeBytes = static_cast<JsUINT64*>(GetPrivateAndComplainIfNull(
                                       pContext,
                                       sizeBytesObj,
                                       "JsUINT64",
                                       "UINT64"));
    JsUINT64* pJsMilwA = static_cast<JsUINT64*>(GetPrivateAndComplainIfNull(
                                       pContext,
                                       milwAObj,
                                       "JsUINT64",
                                       "UINT64"));
    JsUINT64* pJsMaxVA = static_cast<JsUINT64*>(GetPrivateAndComplainIfNull(
                                       pContext,
                                       maxVAObj,
                                       "JsUINT64",
                                       "UINT64"));
    JsUINT64* pJsAlignBytes = static_cast<JsUINT64*>(GetPrivateAndComplainIfNull(
                                      pContext,
                                      alignBytesObj,
                                      "JsUINT64",
                                      "UINT64"));
    if (!pJsSizeBytes || !pJsMilwA || !pJsMaxVA || !pJsAlignBytes)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    // Fetch UINT64 values from JS objects
    size_t sizeBytes = static_cast<size_t>(pJsSizeBytes->GetValue());
    void* pBase  = reinterpret_cast<void*>(pJsMilwA->GetValue());
    void* pLimit = reinterpret_cast<void*>(pJsMaxVA->GetValue());
    size_t alignBytes = static_cast<size_t>(pJsAlignBytes->GetValue());

    // Find available VA range
    UINT64 retVal = reinterpret_cast<UINT64>(
        Platform::VirtualFindFreeVaInRange(sizeBytes, pBase, pLimit, alignBytes));

    // Return UINT64 JS object
    JsUINT64* pJsResult = new JsUINT64(retVal);
    MASSERT(pJsResult);
    if ((rc = pJsResult->CreateJSObject(pContext)) != OK ||
        (rc = pJavaScript->ToJsval(pJsResult->GetJSObject(), pReturlwalue)) != OK)
    {
        pJavaScript->Throw(pContext,
                           rc,
                           "Error oclwrred in VirtualFindFreeVaInRange: Unable to create return value");
        return JS_FALSE;
    }
    return JS_TRUE;
}

// SMethod
C_(Platform_SimulationModeToString)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    UINT32 mode;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &mode)))
    {
        JS_ReportError(pContext, "Usage: Platform.SimulationModeToString(mode)");
        return JS_FALSE;
    }
    string modeStr = Platform::SimulationModeToString((Platform::SimulationMode)mode);

    if (OK != pJavaScript->ToJsval(modeStr, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in Platform.SimulationModeToString");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

P_(Platform_Get_InteractOnBreakPoint);
P_(Platform_Set_InteractOnBreakPoint);
static SProperty Platform_InteractOnBreakPoint
(
    Platform_Object,
    "InteractOnBreakPoint",
    0,
    0,
    Platform_Get_InteractOnBreakPoint,
    Platform_Set_InteractOnBreakPoint,
    0,
    "Go to interactive prompt on driver BreakPoint (i.e. resman error recovery)."
);

P_(Platform_Get_InteractOnBreakPoint)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    if (OK != JavaScriptPtr()->ToJsval(s_InteractOnBreakPoint, pValue))
    {
        JS_ReportError(pContext, "Failed to get InteractOnBreakPoint.");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

P_(Platform_Set_InteractOnBreakPoint)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    // Colwert the argument.
    if (OK != JavaScriptPtr()->FromJsval(*pValue, &s_InteractOnBreakPoint))
    {
        JS_ReportError(pContext, "Failed to set ScriptInteractOnBreakPoint.");
        return JS_FALSE;
    }

    return JS_TRUE;
}

P_(Platform_Get_PrintSystemTime);
P_(Platform_Set_PrintSystemTime);
static SProperty Platform_PrintSystemTime
(
    Platform_Object,
    "PrintSystemTime",
    0,
    0,
    Platform_Get_PrintSystemTime,
    Platform_Set_PrintSystemTime,
    0,
    "Print [hh:mm:ss] prefix on all messages."
);

P_(Platform_Get_PrintSystemTime)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    if (OK != JavaScriptPtr()->ToJsval(s_PrintSystemTime, pValue))
    {
        JS_ReportError(pContext, "Failed to get PrintSystemTime.");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

P_(Platform_Set_PrintSystemTime)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    // Colwert the argument.
    if (OK != JavaScriptPtr()->FromJsval(*pValue, &s_PrintSystemTime))
    {
        JS_ReportError(pContext, "Failed to set ScriptPrintSystemTime.");
        return JS_FALSE;
    }

    Tee::SetUseSystemTime(s_PrintSystemTime);

    return JS_TRUE;
}

CLASS_PROP_READWRITE_LWSTOM(Platform, DumpCallStack,
                            "Should we dump call stack on breakpoint?");
P_(Platform_Get_DumpCallStack)
{
    MASSERT(pContext != nullptr);
    MASSERT(pValue   != nullptr);
    if (OK != JavaScriptPtr()->ToJsval(s_DumpCallStack, pValue))
    {
        JS_ReportError(pContext, "Failed to get DumpCallStack.");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}
P_(Platform_Set_DumpCallStack)
{
    MASSERT(pContext != nullptr);
    MASSERT(pValue   != nullptr);
    if (OK != JavaScriptPtr()->FromJsval(*pValue, &s_DumpCallStack))
    {
        JS_ReportError(pContext, "Failed to set DumpCallStack.");
        return JS_FALSE;
    }
    return JS_TRUE;
}

PROP_READWRITE_NAMESPACE(Platform, Use4ByteMemCopy, bool,
    "Force all MemCopy to use 32-bit reads or less.");

P_(C_Platform_Get_PerformanceCounter);
SProperty Platform_PerformanceCounter
(
    Platform_Object,
    "PerformanceCounter",
    0,
    0,
    C_Platform_Get_PerformanceCounter,
    0,
    JSPROP_READONLY,
    "ticks since start"
);
P_(C_Platform_Get_PerformanceCounter)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    UINT64 Ticks = Xp::QueryPerformanceCounter();
    if (OK != JavaScriptPtr()->ToJsval(Ticks, pValue))
    {
        JS_ReportError(pContext, "Failed to colwert value in Platform.PerformanceCounter");
        return JS_FALSE;
    }
    return JS_TRUE;
}

P_(C_Platform_Get_PerformanceFrequency);
SProperty Platform_PerformanceFrequency
(
    Platform_Object,
    "PerformanceFrequency",
    0,
    0,
    C_Platform_Get_PerformanceFrequency,
    0,
    JSPROP_READONLY,
    "speed of CPU"
);
P_(C_Platform_Get_PerformanceFrequency)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    UINT64 Ticks = Xp::QueryPerformanceFrequency();
    if (OK != JavaScriptPtr()->ToJsval(Ticks, pValue))
    {
        JS_ReportError(pContext, "Failed to colwert value in Platform.PerformanceFrequency");
        return JS_FALSE;
    }
    return JS_TRUE;
}

P_(C_Platform_Get_IsSOC);
SProperty Platform_IsSOC
(
    Platform_Object,
    "IsSOC",
    0,
    0,
    C_Platform_Get_IsSOC,
    0,
    JSPROP_READONLY,
    "system on a chip"
);
P_(C_Platform_Get_IsSOC)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    if (OK != JavaScriptPtr()->ToJsval(Platform::IsTegra(), pValue))
    {
        JS_ReportError(pContext, "Failed to colwert value in Platform.IsSOC");
        return JS_FALSE;
    }
    return JS_TRUE;
}

P_(C_Platform_Get_IsPPC);
SProperty Platform_IsPPC
(
    Platform_Object,
    "IsPPC",
    0,
    0,
    C_Platform_Get_IsPPC,
    0,
    JSPROP_READONLY,
    "Power PC"
);
P_(C_Platform_Get_IsPPC)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    if (OK != JavaScriptPtr()->ToJsval(Platform::IsPPC(), pValue))
    {
        JS_ReportError(pContext, "Failed to colwert value in Platform.IsPPC");
        return JS_FALSE;
    }
    return JS_TRUE;
}

PROP_READWRITE_NAMESPACE(Platform, SkipDisableUserInterface, bool,
    "Skip all DisableUserInterface calls (and therfore all Enable calls)");

PROP_READWRITE_NAMESPACE(Platform, ForceVgaPrint, bool,
    "User forces prints to be from Vga");

PROP_READWRITE_NAMESPACE(Platform, DumpPciAccess, bool,
    "Print out the registers and values when reading from and writing to the PCI.");

P_(C_Platform_Get_IsInitialized);
SProperty Platform_IsInitialized
(
    Platform_Object,
    "IsInitialized",
    0,
    0,
    C_Platform_Get_IsInitialized,
    0,
    JSPROP_READONLY,
    "Return true if platform is initialized"
);
P_(C_Platform_Get_IsInitialized)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    if (OK != JavaScriptPtr()->ToJsval(Platform::IsInitialized(), pValue))
    {
        JS_ReportError(pContext,
                       "Failed to colwert value in Platform.IsInitialized");
        return JS_FALSE;
    }
    return JS_TRUE;
}

P_(C_Platform_Get_PageSize);
SProperty Platform_PageSize
(
    Platform_Object,
    "PageSize",
    0,
    0,
    C_Platform_Get_PageSize,
    0,
    JSPROP_READONLY,
    "Page Size"
);
P_(C_Platform_Get_PageSize)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    if (OK != JavaScriptPtr()->ToJsval(static_cast<UINT32>(Platform::GetPageSize()), pValue))
    {
        JS_ReportError(pContext, "Failed to colwert value in Platform.PageSize");
        return JS_FALSE;
    }
    return JS_TRUE;
}
P_(C_Platform_Get_IsSelfHosted);
SProperty Platform_IsSelfHosted
(
    Platform_Object,
    "IsSelfHosted",
    0,
    0,
    C_Platform_Get_IsSelfHosted,
    0,
    JSPROP_READONLY,
    "self hosted hopper"
);
P_(C_Platform_Get_IsSelfHosted)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    if (OK != JavaScriptPtr()->ToJsval(Platform::IsSelfHosted(), pValue))
    {
        JS_ReportError(pContext, "Failed to colwert value in Platform.IsSelfHosted");
        return JS_FALSE;
    }
    return JS_TRUE;
}

CLASS_PROP_READWRITE_LWSTOM(Platform, IsCCMode,
                            "Confidential Compute Mode");
P_(Platform_Get_IsCCMode)
{
    MASSERT(pContext != nullptr);
    MASSERT(pValue   != nullptr);
    RC rc;
    if ((rc = JavaScriptPtr()->ToJsval(s_IsCCMode, pValue)) != RC::OK)
    {
        *pValue = JSVAL_NULL;
        JavaScriptPtr()->Throw(pContext,
                           rc,
                           "Failed to get IsCCMode.");
        return JS_FALSE;
    }
    return JS_TRUE;
}
P_(Platform_Set_IsCCMode)
{
    MASSERT(pContext != nullptr);
    MASSERT(pValue   != nullptr);
    RC rc;
    if ((rc = JavaScriptPtr()->FromJsval(*pValue, &s_IsCCMode)) != RC::OK)
    {
        JavaScriptPtr()->Throw(pContext,
                           rc,
                           "Failed to set IsCCMode.");
        
        return JS_FALSE;
    }
    return JS_TRUE;
}

