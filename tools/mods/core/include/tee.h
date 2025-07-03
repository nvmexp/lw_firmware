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

// Tee output stream.

#pragma once

#include "types.h"
#include "modsdrv_defines.h"

#include <vector>
#include <string>

//------------------------------------------------------------------------------
// Tee stream functions.
//------------------------------------------------------------------------------

// Print formatted output to the TeeStream given a 'Priority'.
// Returns number of characters written, or negative value if error oclwred.
GNU_FORMAT_PRINTF(2, 3)
INT32 Printf(INT32 Priority, const char * Format, ... /* Arguments */);

// Same as above, except including extra masking per mods module.
// Parameter Module is compared to Tee::GetModuleMask().
GNU_FORMAT_PRINTF(3, 4)
INT32 Printf(INT32 Priority, UINT32 Module, const char * Format, ... );

// Same as above, except including extra parameter for the screen
// print state to use when printing to the screen
GNU_FORMAT_PRINTF(4, 5)
INT32 Printf(INT32 Priority, UINT32 Module, UINT32 Sps, const char * Format, ... /* Arguments */);

namespace Tee
{
    // PriDebug print level is 1, but in code which does prints, we use an object
    // of this dummy type in order to be able to easily compile-out all debug printfs
    // globally in certain MODS builds.
    //
    // This works by overriding printf functions to take PriDebugStub as an argument,
    // so when someone calls Printf(Tee::PriDebug, ...) it ends up being a separate
    // function, which is then easy to manipulate at compile time.
    //
    // The added benefit of doing this with the stub type is that it is not colwertible
    // to int, so it's not possible to inadvertently print debug messages in MODS builds
    // where they have been disabled.
    //
    // PriDebug prints are enabled with "-d/-D/-C"
    struct PriDebugStub
    {
    };

    static constexpr PriDebugStub PriDebug = PriDebugStub{};
}

#ifdef MODS_INCLUDE_DEBUG_PRINTS

GNU_FORMAT_PRINTF(2, 3)
INT32 Printf(Tee::PriDebugStub, const char* format, ...);

// Same as above, except including extra masking per mods module.
// Parameter Module is compared to Tee::GetModuleMask().
GNU_FORMAT_PRINTF(3, 4)
INT32 Printf(Tee::PriDebugStub, UINT32 module, const char* format, ...);

// Same as above, except including extra parameter for the screen
// print state to use when printing to the screen
GNU_FORMAT_PRINTF(4, 5)
INT32 Printf(Tee::PriDebugStub, UINT32 module, UINT32 sps, const char* format, ...);

#else

GNU_FORMAT_PRINTF(2, 3)
static constexpr INT32 Printf(Tee::PriDebugStub, const char*, ...)
{
    return 0;
}

// Same as above, except including extra masking per mods module.
// Parameter Module is compared to Tee::GetModuleMask().
GNU_FORMAT_PRINTF(3, 4)
static constexpr INT32 Printf(Tee::PriDebugStub, UINT32, const char*, ...)
{
    return 0;
}

// Same as above, except including extra parameter for the screen
// print state to use when printing to the screen
GNU_FORMAT_PRINTF(4, 5)
static constexpr INT32 Printf(Tee::PriDebugStub, UINT32, UINT32, const char*, ...)
{
    return 0;
}

#endif

extern "C" {

#include <stdarg.h>

// For those times when C code needs to print into the mods Tee system.
//
INT32 ModsExterlwAPrintf
(
   INT32        Priority,
   UINT32       ModuleCode,
   UINT32       Sps,
   const char * Format,
   va_list      RestOfArgs
);

}

class DataSink;
class FileSink;
class SerialSink;
class CirlwlarSink;
class DebugLogSink;
class EthernetSink;
class AssertInfoSink;
class StdoutSink;
class RC;
class TeeModule;
class ByteStream;

namespace Mle
{
    constexpr UINT64 NoUid = ~0ULL;
    constexpr INT32  NoId  = -1;

    struct Context
    {
        UINT64 uid       = NoUid;
        UINT64 timestamp = 0;
        INT32  threadId  = NoId;
        INT32  devInst   = NoId;
        INT32  testId    = NoId;
    };
}

/**
 * @namespace(Tee)
 *
 * This sends output to the screen and file based on
 * the current priority and level.
 * Priority is set at compile time, and level is set at runtime.
 */

namespace Tee
{
    // Please note that the printing levels are also defined in modsdrv.h
    // and they must match.
    enum Priority : UINT08
    {
        PriNone      = 0,
        PriSecret    = PRI_DEBUG,  // NOTE: Use PriDebug instead!!!
        PriLow       = PRI_LOW,    // Prints that can be enabled with "-verbose"

        PriNormal    = PRI_NORMAL, // Prints that aren't Warn/Error/Always, but seen in regular MODS runs
        PriWarn      = PRI_WARN,   // Warnings that warrant attention, but don't necessarily fail/crash the run
                                   //  PriWarn prints will be prepended with "WARNING: " on some sinks (ex. stdout)
        PriHigh      = PRI_HIGH,   // OBSOLETE: Don't use!
        PriError     = PRI_ERR,    // Prints related to an assert, crash, or RC error
                                   //  PriError prints will be prepended with "ERROR: " on some sinks (ex. stdout)
        PriAlways    = PRI_ALWAYS, // MODS header/footer and test entry/exit information

        ScreenOnly   = 8,
        FileOnly     = 9,
        SerialOnly   = 10,
        CirlwlarOnly = 11,
        DebuggerOnly = 12,
        EthernetOnly = 13,
        MleOnly      = 14,
    };

    inline bool operator!=(INT32 pri, PriDebugStub)
    {
        return pri != PriSecret;
    }

    enum Level
    {
        LevAlways  = 0,
        LevDebug   = 1,
        LevLow     = 2,
        LevNormal  = 3, // Warnings should be printed at LevNormal but hidden at LevHigh
        LevWarn    = 4, // LevWarn is defined here to prevent a missing entry.
                        // It is not used or exposed lwrrently.
                        // It can be used later on if they need to be treated differently.
        LevHigh    = 5,
        LevError   = 6,
        LevNone    = 7, // Only print PriAlways
        LevNever   = 8, // Don't print anything, not even PriAlways
    };
    static_assert(LevNever == PriAlways + 1, "LevNever == PriAlways + 1");

    //! Enumeration representing the different types of printing that may be
    //! available via Tee::SetScreenPrintState.  Not all screens
    //! support changing the screen print state (and screens that do may
    //! not support all of the available states).  Screens also may not
    //! implement the screen print state the same way.  For instance,
    //! SPS_HIGHLIGHT may be white on blue for windows platforms and black
    //! on cyan for DOS platforms
    enum ScreenPrintState
    {
        SPS_NORMAL
        ,SPS_FAIL
        ,SPS_PASS
        ,SPS_WARNING
        ,SPS_HIGHLIGHT
        ,SPS_HIGH
        ,SPS_BOTH
        ,SPS_LOW
        ,SPS_END_LIST
    };

    struct SinkPriorities;

    enum QueueDrainMethod
    {
        qdmATTACHED
        ,qdmDETACHED
        ,qdmBOTH
    };

    // METHODS
    int Write(const SinkPriorities& sinkPriorities,
              const char*           text,
              size_t                textLen,
              const char*           prefix       = 0,
              ScreenPrintState      state        = SPS_END_LIST,
              QueueDrainMethod      qDrainMethod = qdmBOTH,
              const Mle::Context*   mleContext   = nullptr);
    void PrintPrefix(INT32 Pri, char *pPrefixStr, bool bAddThreadId);

    bool WillPrintToMle(INT32 pri);
    void SetMleLimited(bool enable);
    bool IsMleLimited();

    void SetScreenLevel(Level Lev);
    void SetFileLevel(Level Lev);
    void SetMleLevel(Level Lev);
    void SetMleLevel(Level Lev, bool printMleToStdOut);
    void SetSerialLevel(Level Lev);
    void SetCirlwlarLevel(Level Lev);
    void SetDebuggerLevel(Level Lev);
    void SetEthernetLevel(Level Lev);
    void SetAssertInfoLevel(Level Lev);
    void SetAllLevels(Level Lev);

    void SetScreenPause(bool State);
    void SetScreenPrintState(ScreenPrintState state);
    Tee::ScreenPrintState GetScreenPrintState();
    void ResetScreenPause();
    void SetBlinkScroll(bool State);
    void SetPrependThreadId(bool State);
    void SetPrependGpuId(bool State);
    void SetScreenSink(DataSink *pDataSink);
    void FlushToDisk();
    void SetUseSystemTime(bool State);
    bool GetUseSystemTime();
    void SetEnablePrefix(bool Enable);
    bool GetEnablePrefix();

    UINT64 GetTimeFromStart();
    UINT64 GetWallTimeFromStart();

    //! Called once to init MODS start time in ns, used for timestamping log entries.
    void InitTimestamp(UINT64 startTimeNs);

    void TeeInit();
    void TeeQueueInit();
    void TeeDestroy();
    void TeeQueueDestroy();
    void FreeTeeModules();

    RC TeeInitAfterTasker();
    RC TeeShutdownBeforeTasker();
    RC TeeEnableQueuedPrints(bool bEnable);
    bool QueuedPrintsEnabled();
    RC TeeFlushQueuedPrints(FLOAT64 TimeoutMs);

    bool IsInitialized();

    // ACCESSORS
    DataSink       * GetScreenSink();
    FileSink       * GetFileSink();
    FileSink       * GetMleSink();
    SerialSink     * GetSerialSink();
    CirlwlarSink   * GetCirlwlarSink();
    DebugLogSink   * GetDebugLogSink();
    DataSink       * GetDebuggerSink();
    EthernetSink   * GetEthernetSink();
    AssertInfoSink * GetAssertInfoSink();
    StdoutSink     * GetStdoutSink();

    Level      GetScreenLevel();
    Level      GetFileLevel();
    Level      GetMleLevel();
    Level      GetSerialLevel();
    Level      GetCirlwlarLevel();
    Level      GetDebuggerLevel();
    Level      GetEthernetLevel();
    Level      GetAssertInfoLevel();

    bool       WillPrint(Priority pri);
    bool       WillPrint(const SinkPriorities &SinkPriorities);
    bool       WillPrint(const SinkPriorities &SinkPri,
                         QueueDrainMethod qDrainMethod);

#ifdef MODS_INCLUDE_DEBUG_PRINTS
    inline bool WillPrint(PriDebugStub)
    {
        return WillPrint(Tee::PriSecret);
    }
#else
    constexpr bool WillPrint(PriDebugStub)
    {
        return false;
    }
#endif

    bool IsGroup(UINT32 group);

    void       EnDisModule(const char* ModuleName, bool enable);
    void       EnDisAllModules(bool enable);
    bool       IsModuleEnabled(UINT32 ModuleCode);
    UINT32     GetModuleCode(const char* ModuleName);
    UINT32     GetTeeModuleCoreCode();
    const TeeModule& GetModule(UINT32 ModuleCode);

    bool       IsScreenPause();
    bool       IsResetScreenPause();
    bool       IsBlinkScroll();
    bool       IsPrependThreadId();
    bool       IsPrependGpuId();

    enum
    {
        MaxPrintSize         = 131071
        ,MaxPrintPrefixSize  = 64
        ,ModuleNone         = ~0
        ,DefPrintfQFlushMs  = 1000
    };

    //! Used to set separate priorities for each sink.
    struct SinkPriorities
    {
        SinkPriorities(Priority Pri) { SetAllPriority(Pri); }
        void SetAllPriority(Priority Pri);

        Priority ScreenPri;
        Priority FilePri;
        Priority MlePri;
        Priority SerialPri;
        Priority CirlwlarPri;
        Priority DebuggerPri;
        Priority EthernetPri;
        Priority AssertInfoPri;
    };

    class SaveLevels
    {
        public:
            SaveLevels();
            ~SaveLevels();

        private:
            Level m_ScreenLevel;
            Level m_FileLevel;
            Level m_MleLevel;
            Level m_SerialLevel;
            Level m_CirlwlarLevel;
            Level m_DebuggerLevel;
            Level m_EthernetLevel;
    };

    class SetLowAssertLevel
    {
        public:
            SetLowAssertLevel();
            ~SetLowAssertLevel();
        private:
            static Level s_OrigAssertLevel;
            static volatile INT32 s_RefCount;
    };

    void PrintMle(ByteStream* pBytes);
    void PrintMle(ByteStream* pBytes, Priority pri, const Mle::Context& ctx);
    void SetContext(Mle::Context* pMleContext);
    void SetContextUidAndTimestamp(Mle::Context* pMleContext);
    inline bool HasContext(const Mle::Context& ctx)
    {
        return ctx.uid != Mle::NoUid;
    }
}

//! TeeModule is used for debug message enabling/disabling.
//! For a specific mods module, a TeeModule can be specified like so:
//!
//! static TeeModule s_moduleTest("ModsTest");
//!
//! Then for debug prints in that module, this can be done:
//!
//! ModsExterlwAPrintf(Priority, s_moduleTest.GetCode(), Format, Arguments);
//!
//! Then if the mdiag option -message_disable ModsTest is used, the
//! message will not be printed if the priority is debug or lower.
//! The option -message_enable of course enables printing for the
//! specified modules.
//!
//! Messages for all modules are enabled by default. Pass false to "enabled"
//! in the constructor to disable the current messages if the users do not
//! explicitly enable them on the CLI.
//!
//! Be careful not to use a name (e.g., "ModsCore") that is already in use.

class TeeModule
{
public:
    TeeModule(const char* name, bool enabled=true, const char* moddir="::");
    ~TeeModule();

    UINT32 GetCode() const         { return m_code; }
    const char* GetName() const    { return m_name.c_str(); }
    const char* GetModDir() const  { return m_moddir.c_str(); }
private:
    UINT32       m_code;
    std::string       m_name;
    std::string       m_moddir;
};

// Helper macros for declaring/defining TeeModules
#define DECLARE_MSG_GROUP(Dir, Mod, Group) \
    extern TeeModule TEE##Dir##Mod##Group;

#define DEFINE_MSG_GROUP(Dir, Mod, Group, Enable) \
    TeeModule TEE##Dir##Mod##Group(#Mod "_" #Group, Enable, #Dir);

#define __MSGID__(Dir, Mod, Group) TEE##Dir##Mod##Group.GetCode()

#define DECLARE_MSG_DIR(Dir) \
    extern TeeModule TEE##Dir;

#define DEFINE_MSG_DIR(Dir, Enable) \
    TeeModule TEE##Dir(#Dir, Enable);

#define __MSGID_DIR__(Dir) TEE##Dir.GetCode()
