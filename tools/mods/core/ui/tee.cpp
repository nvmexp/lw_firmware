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

//------------------------------------------------------------------------------
// 45678901234567890123456789012345678901234567890123456789012345678901234567890

#include "core/include/mle_protobuf.h"
#include "core/include/tee.h"
#include "core/include/massert.h"
#include "core/include/datasink.h"
#include "core/include/filesink.h"
#include "core/include/sersink.h"
#include "core/include/bscrsink.h"
#include "core/include/dbgsink.h"
#include "core/include/circsink.h"
#include "core/include/debuglogsink.h"
#include "core/include/ethrsink.h"
#include "core/include/assertinfosink.h"
#include "core/include/stdoutsink.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "inc/bytestream.h"
#include "core/include/cpu.h"
#include "core/include/errormap.h"
#include "core/include/xp.h"
#ifdef INCLUDE_GPU
#include "gpu/include/vgpu.h"
#endif
#include "protobuf/pbwriter.h"

//#define DO_STACK_CHECKS 1
#include "core/include/tasker.h"
#include "core/include/cnslmgr.h"
#include "core/include/memcheck.h"

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <ctype.h>
#include <map>
#include <string>
#include <string.h>
#include <utility>

//------------------------------------------------------------------------------
// String stream buffer which contains the output stream.

namespace
{
    // The DataSink, FileSink, SerialSink and CirlwlarSink objects are
    // allocated and managed externally to Tee.
    DataSink       * d_pScreenSink       = 0;
    FileSink       * d_pFileSink         = 0;
    FileSink       * d_pMleSink          = 0;
    SerialSink     * d_pSerialSink       = 0;
    CirlwlarSink   * d_pCirlwlarSink     = 0;
    DebugLogSink   * d_pDebugLogSink     = 0;
    DataSink       * d_pDebuggerSink     = 0;
    EthernetSink   * d_pEthernetSink     = 0;
    AssertInfoSink * d_pAssertInfoSink   = 0;
    StdoutSink     * d_pStdoutSink       = 0;

    Tee::Level     d_ScreenLevel         = Tee::LevNormal;
    Tee::Level     d_FileLevel           = Tee::LevNormal;
    Tee::Level     d_MleLevel            = Tee::LevNone;
    Tee::Level     d_SerialLevel         = Tee::LevNormal;
    Tee::Level     d_CirlwlarLevel       = Tee::LevNormal;
    Tee::Level     d_DebuggerLevel       = Tee::LevNormal;
    Tee::Level     d_EthernetLevel       = Tee::LevNormal;
    Tee::Level     d_AssertInfoLevel     = Tee::LevNormal;

    bool           d_IsScreenPause      = false;
    volatile INT32 d_IsResetScreenPause = 0;
    bool           d_IsBlinkScroll      = false;
    bool           d_PrependThreadId    = false;
    bool           d_PrependGpuId       = false;
    bool           d_IsInitialized      = false;
    bool           d_IsShutDown         = false;
    bool           d_UseSystemTime      = false;
    bool           d_EnablePrefix       = true;
    bool           d_PrintMleToStdOut   = false;

    UINT64         d_StartTimeNS = 0;

    Tee::ScreenPrintState d_ScreenPrintState = Tee::SPS_NORMAL;

    vector<bool>*  d_pModuleEnabledMask = 0;
    vector<const TeeModule*>* d_pModuleList = 0;
    map<string, const TeeModule*>* d_pModuleNameMap = 0;
    map<string, vector<const TeeModule*>* >* d_pModDirMap = 0;
    typedef map<string, vector<const TeeModule*>* >::iterator ModDirMapIter;
}

//------------------------------------------------------------------------------
// Class for temporary saving of output levels.
//------------------------------------------------------------------------------

Tee::SaveLevels::SaveLevels()
: m_ScreenLevel(d_ScreenLevel),
  m_FileLevel(d_FileLevel),
  m_MleLevel(d_MleLevel),
  m_SerialLevel(d_SerialLevel),
  m_CirlwlarLevel(d_CirlwlarLevel),
  m_DebuggerLevel(d_DebuggerLevel),
  m_EthernetLevel(d_EthernetLevel)
{
}

Tee::SaveLevels::~SaveLevels()
{
    d_ScreenLevel = m_ScreenLevel;
    d_FileLevel = m_FileLevel;
    d_MleLevel = m_MleLevel;
    d_SerialLevel = m_SerialLevel;
    d_CirlwlarLevel = m_CirlwlarLevel;
    d_DebuggerLevel = m_DebuggerLevel;
    d_EthernetLevel = m_EthernetLevel;
}

INT32 Printf
(
   INT32        Priority,
   const char * Format,
   ... //       Arguments
)
{
   INT32 CharactersWritten;

   va_list Arguments;
   va_start(Arguments, Format);

   CharactersWritten = ModsExterlwAPrintf(Priority, Tee::GetTeeModuleCoreCode(),
                                          d_ScreenPrintState, Format,
                                          Arguments);

   va_end(Arguments);

   return CharactersWritten;
}

INT32 Printf
(
   INT32        Priority,
   UINT32       ModuleCode,
   const char * Format,
   ... //       Arguments
)
{
    INT32 CharactersWritten = 0;

    va_list Arguments;
    va_start(Arguments, Format);

    CharactersWritten = ModsExterlwAPrintf(Priority, ModuleCode,
                                           d_ScreenPrintState, Format,
                                           Arguments);

    va_end(Arguments);

    return CharactersWritten;
}

INT32 Printf
(
   INT32        Priority,
   UINT32       ModuleCode,
   UINT32       Sps,
   const char * Format,
   ... //       Arguments
)
{
    INT32 CharactersWritten = 0;

    va_list Arguments;
    va_start(Arguments, Format);

    CharactersWritten = ModsExterlwAPrintf(Priority, ModuleCode, Sps, Format,
                                           Arguments);

    va_end(Arguments);

    return CharactersWritten;
}

#ifdef MODS_INCLUDE_DEBUG_PRINTS

INT32 Printf
(
   Tee::PriDebugStub,
   const char * format,
   ... //       arguments
)
{
   INT32 charactersWritten;

   va_list arguments;
   va_start(arguments, format);

   charactersWritten = ModsExterlwAPrintf(Tee::PriSecret, Tee::GetTeeModuleCoreCode(),
                                          d_ScreenPrintState, format,
                                          arguments);

   va_end(arguments);

   return charactersWritten;
}

INT32 Printf
(
   Tee::PriDebugStub,
   UINT32       moduleCode,
   const char * format,
   ... //       arguments
)
{
    INT32 charactersWritten = 0;

    va_list arguments;
    va_start(arguments, format);

    charactersWritten = ModsExterlwAPrintf(Tee::PriSecret, moduleCode,
                                           d_ScreenPrintState, format,
                                           arguments);

    va_end(arguments);

    return charactersWritten;
}

INT32 Printf
(
   Tee::PriDebugStub,
   UINT32       moduleCode,
   UINT32       Sps,
   const char * format,
   ... //       arguments
)
{
    INT32 charactersWritten = 0;

    va_list arguments;
    va_start(arguments, format);

    charactersWritten = ModsExterlwAPrintf(Tee::PriSecret, moduleCode, Sps, format,
                                           arguments);

    va_end(arguments);

    return charactersWritten;
}

#endif

void Tee::PrintMle(ByteStream* pBytes)
{
    Mle::Context ctx;
    Tee::SetContext(&ctx);

    PrintMle(pBytes, Tee::MleOnly, ctx);
}

void Tee::PrintMle(ByteStream* pBytes, Priority pri, const Mle::Context& ctx)
{
    FileSink* const pMleSink    = Tee::GetMleSink();
    DataSink* const pScreenSink = d_PrintMleToStdOut ? Tee::GetScreenSink() : nullptr;

    if (pMleSink)
    {
        pMleSink->PrintMle(pBytes, pri, ctx);

        if (pScreenSink)
        {
            // The previous call PrintMle() has already wrapped the data
            // in all the necessary headers, so just re-use it and write
            // it directly into the screen sink.
            pScreenSink->PrintRaw(*pBytes);
        }
    }
    else if (pScreenSink)
    {
        pScreenSink->PrintMle(pBytes, pri, ctx);
    }
}

namespace
{
    atomic<UINT64> s_MleUID{0};
}

void Tee::SetContextUidAndTimestamp(Mle::Context* pMleContext)
{
    MASSERT(pMleContext);
    MASSERT(!HasContext(*pMleContext));

    pMleContext->uid       = s_MleUID++;
    pMleContext->timestamp = Tee::GetWallTimeFromStart();
}

void Tee::SetContext(Mle::Context* pMleContext)
{
    MASSERT(pMleContext);
    MASSERT(!HasContext(*pMleContext));

    SetContextUidAndTimestamp(pMleContext);

    if (Tasker::IsInitialized())
    {
        pMleContext->threadId = Tasker::LwrrentThread();
        pMleContext->testId   = ErrorMap::Test();
        pMleContext->devInst  = ErrorMap::DevInst();
    }
}

void Tee::TeeInit()
{
    if (d_IsInitialized || d_IsShutDown)
    {
        return; // Do nothing
    }

    d_IsInitialized = true;

    Tee::TeeQueueInit();

    d_pScreenSink     = new BatchScreenSink;
    d_pFileSink       = new FileSink;
    d_pMleSink        = new FileSink;
    d_pSerialSink     = new SerialSink;
    d_pCirlwlarSink   = new CirlwlarSink;
    d_pDebugLogSink   = new DebugLogSink;
    d_pDebuggerSink   = new DebuggerSink;
    d_pEthernetSink   = new EthernetSink;
    d_pAssertInfoSink = new AssertInfoSink;
    d_pStdoutSink     = new StdoutSink;

    MASSERT(d_pScreenSink     != 0);
    MASSERT(d_pFileSink       != 0);
    MASSERT(d_pMleSink        != 0);
    MASSERT(d_pSerialSink     != 0);
    MASSERT(d_pCirlwlarSink   != 0);
    MASSERT(d_pDebugLogSink   != 0);
    MASSERT(d_pDebuggerSink   != 0);
    MASSERT(d_pEthernetSink   != 0);
    MASSERT(d_pAssertInfoSink != 0);
    MASSERT(d_pStdoutSink     != 0);

    SetScreenLevel(Tee::LevNormal);
    SetFileLevel(Tee::LevNormal);
    SetMleLevel(Tee::LevNone, false);
    SetSerialLevel(Tee::LevNormal);
    SetCirlwlarLevel(Tee::LevNormal);
    SetDebuggerLevel(Tee::LevNormal);
    SetEthernetLevel(Tee::LevNormal);

    // Bug 512956: AssertInfoSink causes a lot of debug spew in
    //             Mdiag tests.
#ifdef SIM_BUILD
    SetAssertInfoLevel(Tee::LevNormal);
#else
    SetAssertInfoLevel(Tee::LevLow);
#endif
    if(d_pEthernetSink != NULL)
        d_pEthernetSink->Open();

    if(d_pAssertInfoSink != NULL)
        d_pAssertInfoSink->Initialize();
}

void Tee::TeeDestroy()
{
    if (d_IsShutDown)
    {
        return;  // Do nothing
    }

    FlushToDisk();

    d_IsInitialized = false;
    d_IsShutDown = true;

    if(d_pEthernetSink != NULL)
    {
        d_pEthernetSink->Close();
        delete d_pEthernetSink;
        d_pEthernetSink = 0;
    }

    delete d_pScreenSink;
    delete d_pFileSink;
    delete d_pMleSink;
    delete d_pSerialSink;
    delete d_pCirlwlarSink;
    delete d_pDebugLogSink;
    delete d_pDebuggerSink;
    delete d_pAssertInfoSink;
    delete d_pStdoutSink;

    d_pScreenSink = 0;
    d_pFileSink = 0;
    d_pMleSink = 0;
    d_pSerialSink = 0;
    d_pCirlwlarSink = 0;
    d_pDebugLogSink = 0;
    d_pDebuggerSink = 0;
    d_pAssertInfoSink = 0;
    d_pStdoutSink = 0;

    delete d_pModuleEnabledMask;
    delete d_pModuleList;
    delete d_pModuleNameMap;
    delete d_pModDirMap;
    d_pModuleEnabledMask = 0;
    d_pModuleList = 0;
    d_pModuleNameMap = 0;
    d_pModDirMap = 0;

    Tee::TeeQueueDestroy();
}

bool Tee::IsInitialized()
{
    return (true == d_IsInitialized);
}

// -----------------------------------------------------------------------------
// Set Priority
// -----------------------------------------------------------------------------

void Tee::SinkPriorities::SetAllPriority
(
   Priority Pri
)
{
   switch (Pri)
   {
      case Tee::PriNone   :
      case Tee::PriSecret :
      case Tee::PriLow    :
      case Tee::PriNormal :
      case Tee::PriHigh   :
      case Tee::PriAlways :
      case Tee::PriError  :
      case Tee::PriWarn   :
      {
         ScreenPri     = Pri;
         FilePri       = Pri;
         MlePri        = Pri;
         SerialPri     = Pri;
         CirlwlarPri   = Pri;
         DebuggerPri   = Pri;
         EthernetPri   = Pri;
         AssertInfoPri = Pri;
         break;
      }

      case Tee::ScreenOnly :
      {
         ScreenPri     = PriAlways;
         FilePri       = PriNone;
         MlePri        = PriNone;
         SerialPri     = PriNone;
         CirlwlarPri   = PriNone;
         DebuggerPri   = PriNone;
         EthernetPri   = PriNone;
         AssertInfoPri = PriNone;
         break;
      }

      case Tee::FileOnly :
      {
         ScreenPri     = PriNone;
         FilePri       = PriAlways;
         MlePri        = PriNone;
         SerialPri     = PriNone;
         CirlwlarPri   = PriNone;
         DebuggerPri   = PriNone;
         EthernetPri   = PriAlways;    // for log
         AssertInfoPri = PriNone;
         break;
      }

      case Tee::SerialOnly :
      {
         ScreenPri     = PriNone;
         FilePri       = PriNone;
         MlePri        = PriNone;
         SerialPri     = PriAlways;
         CirlwlarPri   = PriNone;
         DebuggerPri   = PriNone;
         EthernetPri   = PriNone;
         AssertInfoPri = PriNone;
         break;
      }

      case Tee::CirlwlarOnly :
      {
         ScreenPri     = PriNone;
         FilePri       = PriNone;
         MlePri        = PriNone;
         SerialPri     = PriNone;
         CirlwlarPri   = PriAlways;
         DebuggerPri   = PriNone;
         EthernetPri   = PriNone;
         AssertInfoPri = PriAlways;
         break;
      }

      case Tee::DebuggerOnly :
      {
         ScreenPri     = PriNone;
         FilePri       = PriNone;
         MlePri        = PriNone;
         SerialPri     = PriNone;
         CirlwlarPri   = PriNone;
         DebuggerPri   = PriAlways;
         EthernetPri   = PriNone;
         AssertInfoPri = PriNone;
         break;
      }

      case Tee::EthernetOnly :
      {
         ScreenPri     = PriNone;
         FilePri       = PriNone;
         MlePri        = PriNone;
         SerialPri     = PriNone;
         CirlwlarPri   = PriNone;
         DebuggerPri   = PriNone;
         EthernetPri   = PriAlways;
         AssertInfoPri = PriNone;
         break;
      }

      case Tee::MleOnly :
      {
         ScreenPri     = (d_PrintMleToStdOut ? PriAlways : PriNone);
         FilePri       = PriNone;
         MlePri        = PriAlways;
         SerialPri     = PriNone;
         CirlwlarPri   = PriNone;
         DebuggerPri   = PriNone;
         EthernetPri   = PriNone;
         AssertInfoPri = PriNone;
         break;
      }

      default :
      {
         // We should not reach this code in a working program.
         MASSERT(false);
      }
   }
}

// -----------------------------------------------------------------------------
// Set Level
// -----------------------------------------------------------------------------

void Tee::SetScreenLevel(Level Lev)
{
   d_ScreenLevel = Lev;
}

void Tee::SetFileLevel(Level Lev)
{
   d_FileLevel = Lev;
}

void Tee::SetMleLevel(Level Lev)
{
   d_MleLevel = Lev;
}

void Tee::SetMleLevel(Level Lev, bool printMleToStdOut)
{
   d_PrintMleToStdOut = printMleToStdOut;
   d_MleLevel = Lev;
}

void Tee::SetSerialLevel(Level Lev)
{
   d_SerialLevel = Lev;
}

void Tee::SetCirlwlarLevel(Level Lev)
{
   d_CirlwlarLevel = Lev;
}

void Tee::SetDebuggerLevel(Level Lev)
{
   d_DebuggerLevel = Lev;
}

void Tee::SetEthernetLevel(Level Lev)
{
   d_EthernetLevel = Lev;
}

void Tee::SetAssertInfoLevel(Level Lev)
{
   d_AssertInfoLevel = Lev;
}

void Tee::SetAllLevels(Level Lev)
{
    d_ScreenLevel   = Lev;
    d_FileLevel     = Lev;
    d_MleLevel      = Lev;
    d_SerialLevel   = Lev;
    d_CirlwlarLevel = Lev;
    d_DebuggerLevel = Lev;
    d_EthernetLevel = Lev;
}

// -----------------------------------------------------------------------------
// Misc screen functions
// -----------------------------------------------------------------------------
void Tee::SetScreenPause
(
   bool State
)
{
   d_IsScreenPause = State;
}

void Tee::SetScreenPrintState(Tee::ScreenPrintState state)
{
    d_ScreenPrintState = state;
}

Tee::ScreenPrintState Tee::GetScreenPrintState()
{
    return d_ScreenPrintState;
}

void Tee::ResetScreenPause()
{
    Cpu::AtomicWrite(&d_IsResetScreenPause, 1);
}

void Tee::SetBlinkScroll
(
   bool State
)
{
   d_IsBlinkScroll = State;
}

void Tee::SetPrependThreadId
(
   bool State
)
{
   d_PrependThreadId = State;
}

void Tee::SetPrependGpuId(bool state)
{
    d_PrependGpuId = state;
}

bool Tee::IsBlinkScroll()
{
   return d_IsBlinkScroll;
}

bool Tee::IsScreenPause()
{
   return d_IsScreenPause;
}

bool Tee::IsResetScreenPause()
{
    return Cpu::AtomicRead(&d_IsResetScreenPause) != 0;
}

bool Tee::IsPrependThreadId()
{
   return d_PrependThreadId;
}

bool Tee::IsPrependGpuId()
{
    return d_PrependGpuId;
}

void Tee::SetUseSystemTime
(
   bool State
)
{
   d_UseSystemTime = State;
}

bool Tee::GetUseSystemTime()
{
    return d_UseSystemTime;
}

void Tee::SetEnablePrefix(bool Enable)
{
   d_EnablePrefix = Enable;
}

bool Tee::GetEnablePrefix()
{
   return d_EnablePrefix;
}

// -----------------------------------------------------------------------------
// Misc disk functions
// -----------------------------------------------------------------------------
void Tee::FlushToDisk()
{
   if (Tee::TeeFlushQueuedPrints(Tee::DefPrintfQFlushMs) != OK)
   {
       Printf(Tee::PriHigh,
              "Tee::FlushToDisk : Timeout flushing queued prints\n");
   }
   d_pFileSink->Flush();
   d_pMleSink->Flush();
}

// -----------------------------------------------------------------------------
// Set Screen Sink
// -----------------------------------------------------------------------------
void Tee::SetScreenSink(DataSink *pDataSink)
{
   d_pScreenSink = pDataSink;
}

// -----------------------------------------------------------------------------
// Get Sink
// -----------------------------------------------------------------------------
DataSink * Tee::GetScreenSink()
{
   return d_pScreenSink;
}

FileSink * Tee::GetFileSink()
{
   return d_pFileSink;
}

FileSink * Tee::GetMleSink()
{
   return d_pMleSink;
}

SerialSink * Tee::GetSerialSink()
{
   return d_pSerialSink;
}

CirlwlarSink * Tee::GetCirlwlarSink()
{
   return d_pCirlwlarSink;
}

DebugLogSink * Tee::GetDebugLogSink()
{
   return d_pDebugLogSink;
}

DataSink * Tee::GetDebuggerSink()
{
   return d_pDebuggerSink;
}

EthernetSink * Tee::GetEthernetSink()
{
   return d_pEthernetSink;
}

AssertInfoSink * Tee::GetAssertInfoSink()
{
   return d_pAssertInfoSink;
}

StdoutSink * Tee::GetStdoutSink()
{
   return d_pStdoutSink;
}

// -----------------------------------------------------------------------------
// Get Level
// -----------------------------------------------------------------------------
Tee::Level Tee::GetScreenLevel()
{
   return d_ScreenLevel;
}

Tee::Level Tee::GetFileLevel()
{
   return d_FileLevel;
}

Tee::Level Tee::GetMleLevel()
{
   return d_MleLevel;
}

Tee::Level Tee::GetSerialLevel()
{
   return d_SerialLevel;
}

Tee::Level Tee::GetCirlwlarLevel()
{
   return d_CirlwlarLevel;
}

Tee::Level Tee::GetDebuggerLevel()
{
   return d_DebuggerLevel;
}

Tee::Level Tee::GetEthernetLevel()
{
   return d_EthernetLevel;
}

Tee::Level Tee::GetAssertInfoLevel()
{
   return d_AssertInfoLevel;
}

bool Tee::IsGroup(UINT32 group)
{
    return group != static_cast<UINT32>(ModuleNone);
}

//------------------------------------------------------------------------------
// Return true if any sink will print something.
//------------------------------------------------------------------------------
bool Tee::WillPrint
(
    const SinkPriorities &SinkPri,
    Tee::QueueDrainMethod qDrainMethod
)
{
    // Note: The per-sink queue drain methods in this function must match
    // those in Write()

    // The screen level is used for both the screen sink (attached drain)
    // and stdout sink (detached drain)
    if ((d_ScreenLevel     <= (int)SinkPri.ScreenPri) ||
        ((d_FileLevel       <= (int)SinkPri.FilePri) &&
         (qDrainMethod != Tee::qdmATTACHED)) ||
        ((d_MleLevel       <= (int)SinkPri.MlePri) &&
         (qDrainMethod != Tee::qdmATTACHED)) ||
        ((d_SerialLevel     <= (int)SinkPri.SerialPri) &&
         (qDrainMethod != Tee::qdmDETACHED)) ||
        ((d_CirlwlarLevel   <= (int)SinkPri.CirlwlarPri) &&
         (qDrainMethod != Tee::qdmATTACHED)) ||
        ((d_DebuggerLevel   <= (int)SinkPri.DebuggerPri) &&
         (qDrainMethod != Tee::qdmDETACHED)) ||
        ((d_EthernetLevel   <= (int)SinkPri.EthernetPri) &&
         (qDrainMethod != Tee::qdmDETACHED)) ||
        ((d_AssertInfoLevel <= (int)SinkPri.AssertInfoPri) &&
         (qDrainMethod != Tee::qdmATTACHED)))
      return true;

   return false;
}

//------------------------------------------------------------------------------
// Return true if any sink will print something.
//------------------------------------------------------------------------------
bool Tee::WillPrint(const SinkPriorities &SinkPri)
{
   return  WillPrint(SinkPri, Tee::qdmBOTH);
}

//------------------------------------------------------------------------------
// Return true if any sink would print something.
//------------------------------------------------------------------------------
bool Tee::WillPrint(Priority pri)
{
    SinkPriorities SinkPri(pri);
    return WillPrint(SinkPri);
}

static inline void ToLowerCase(string* str)
{
    std::transform(str->begin(), str->end(), str->begin(), ::tolower);
}

static void EnDisModulePattern(const string& pattern,
                               vector<const TeeModule*>* modules,
                               bool enable)
{
    if (pattern[0] != '%')
        return; // not partial matching

    string namePattern = pattern.substr(1);
    vector<const TeeModule*>::iterator modIt = modules->begin();
    if (*modIt == NULL) ++ modIt; // skip the 1st NULL module

    for (; modIt != modules->end(); ++modIt)
    {
        const TeeModule* mod = *modIt;
        string modName(mod->GetName());
        ToLowerCase(&modName);

        if (modName.find(namePattern) != string::npos)
        {
            (*d_pModuleEnabledMask)[mod->GetCode()] = enable;
        }
    }
}

void Tee::EnDisModule(const char* ModuleName, bool enable)
{
    if (!d_pModuleEnabledMask || !ModuleName || strlen(ModuleName) == 0)
    {
        // If called after FreeTeeModules or with empty name, NOP.
        return;
    }

    // Format:
    //   Full:     [^dir][#][%][mod]
    //   Dir-only: ^dir
    //   Mod-only: [%]mod
    if (ModuleName[0] == '^') // module directory matching
    {
        string moddir(ModuleName + 1);
        ToLowerCase(&moddir);

        ModDirMapIter it;
        string moduleStr;
        size_t separator = moddir.find("#");
        if (separator != string::npos) // dir-qualified groups
        {
            it = d_pModDirMap->find(moddir.substr(0, separator));
            moduleStr = moddir.substr(separator + 1);
            moddir = moddir.substr(0, separator);
        }
        else
        {
            it = d_pModDirMap->find(moddir);
        }

        if (it != d_pModDirMap->end())
        {
            vector<const TeeModule*>* modules = it->second;
            if (moduleStr.size() > 0) // dir-qualified groups
            {
                ToLowerCase(&moduleStr);

                if (moduleStr[0] == '%') // partial matching
                {
                    EnDisModulePattern(moduleStr, modules, enable);
                }
                else // exact matching
                {
                    UINT32 code = GetModuleCode(moduleStr.c_str());
                    if (code != 0)
                    {
                        (*d_pModuleEnabledMask)[code] = enable;
                    }
                }
            }
            else // directory-only
            {
                // enable/disable ALL modules in this directory
                vector<const TeeModule*>::iterator modIt = modules->begin();
                for (; modIt != modules->end(); ++modIt)
                {
                    (*d_pModuleEnabledMask)[(*modIt)->GetCode()] = enable;
                }

                // set the directory module itself
                UINT32 code = (*d_pModuleNameMap)[moddir]->GetCode();
                (*d_pModuleEnabledMask)[code] = enable;
            }
        }
    }
    else if (ModuleName[0] == '%') // partial name matching
    {
        string lowerName(ModuleName);
        ToLowerCase(&lowerName);
        EnDisModulePattern(lowerName, d_pModuleList, enable);
    }
    else // singleton matching
    {
        UINT32 ModuleCode = GetModuleCode(ModuleName);
        if (ModuleCode != 0)
        {
            (*d_pModuleEnabledMask)[ModuleCode] = enable;
        }
    }
}

void Tee::EnDisAllModules(bool enable)
{
    vector<bool>::iterator it = d_pModuleEnabledMask->begin();
    for (; it != d_pModuleEnabledMask->end(); ++it)
    {
        *it = enable;
    }
}

bool Tee::IsModuleEnabled(UINT32 ModuleCode)
{
    if (IsGroup(ModuleCode) && d_pModuleEnabledMask &&
        ModuleCode < static_cast<UINT32>(d_pModuleEnabledMask->size()))
    {
        return (*d_pModuleEnabledMask)[ModuleCode];
    }
    else
    {
        return true;
    }
}

namespace Tee
{
    UINT32 GetNextModuleCode(bool enabled);
}

UINT32 Tee::GetNextModuleCode(bool enabled)
{
    if (!d_pModuleEnabledMask)
    {
        d_pModuleEnabledMask = new vector<bool>;
        // extra push because GetModuleCode() returns 0 for
        // non-existent names
        d_pModuleEnabledMask->push_back(true);
    }
    UINT32 temp = static_cast<UINT32>(d_pModuleEnabledMask->size());
    d_pModuleEnabledMask->push_back(enabled);
    return temp;
}

TeeModule::TeeModule(const char* name, bool enabled, const char* moddir) :
    m_code(Tee::GetNextModuleCode(enabled)), m_name(name), m_moddir(moddir)
{
    MASSERT(name && moddir != NULL);
    if (!d_pModuleList)
    {
        d_pModuleList = new vector<const TeeModule*>;
        d_pModuleList->push_back(NULL); // one-based

        d_pModuleNameMap = new map<string, const TeeModule*>;
        d_pModDirMap = new map<string, vector<const TeeModule*>* >;
    }

    string lowerName(name);
    ToLowerCase(&lowerName); // always colwert to lower case internally
    (*d_pModuleNameMap)[lowerName] = this;
    d_pModuleList->push_back(this);

    // module directory
    string sModdir(moddir);
    ToLowerCase(&sModdir);
    vector<const TeeModule*>* modules = NULL;
    ModDirMapIter it = d_pModDirMap->find(sModdir);

    if (it == d_pModDirMap->end())
    {
        modules = new vector<const TeeModule*>;
        (*d_pModDirMap)[sModdir] = modules;
    }
    else
    {
        modules = it->second;
    }
    modules->push_back(this);
}

TeeModule::~TeeModule()
{
    string modName(m_name);
    ToLowerCase(&modName);
    map<string, const TeeModule *>::iterator moduleNameIt = d_pModuleNameMap->find(modName);
    if (moduleNameIt != d_pModuleNameMap->end())
    {
        d_pModuleNameMap->erase(moduleNameIt);
    }

    vector<const TeeModule *>::iterator moduleListIt = find(d_pModuleList->begin(),
            d_pModuleList->end(), this);
    if (moduleListIt != d_pModuleList->end())
    {
        d_pModuleList->erase(moduleListIt);
    }

    string modDir(m_moddir);
    ToLowerCase(&modDir);
    map<string, vector<const TeeModule*>* >::iterator it = d_pModDirMap->find(modDir);
    if (it != d_pModDirMap->end())
    {
        vector<const TeeModule *>* pTeeModules = it->second;

        vector<const TeeModule *>::iterator teeModuleIt = find(pTeeModules->begin(),
            pTeeModules->end(), this);
        if (teeModuleIt != pTeeModules->end())
        {
            pTeeModules->erase(teeModuleIt);
        }

        if (pTeeModules->empty())
        {
            delete it->second;
            d_pModDirMap->erase(it);
        }
    }

}

static TeeModule s_moduleCore("ModsCore");

UINT32 Tee::GetTeeModuleCoreCode() { return s_moduleCore.GetCode(); }

UINT32 Tee::GetModuleCode(const char* ModuleName)
{
    if (!d_pModuleEnabledMask)
    {
        // If called after FreeTeeModules, punt.
        return 0;
    }
    string lowerName(ModuleName);
    ToLowerCase(&lowerName); // always colwert to lower case internally
    map<string, const TeeModule*>::iterator iter = d_pModuleNameMap->find(lowerName);
    if (iter == d_pModuleNameMap->end())
    {
        return 0;
    }
    else
    {
        return iter->second->GetCode();
    }
}

const TeeModule& Tee::GetModule(UINT32 ModuleCode)
{
    MASSERT (d_pModuleList && d_pModuleList->size() > ModuleCode);
    return *((*d_pModuleList)[ModuleCode]);
}

//! Free persistent TeeModule memory during shutdown, before leak-checker runs.
void Tee::FreeTeeModules()
{
    delete d_pModuleEnabledMask;
    d_pModuleEnabledMask = 0;

    delete d_pModuleList;
    d_pModuleList = 0;

    delete d_pModuleNameMap;
    d_pModuleNameMap = 0;

    ModDirMapIter it = d_pModDirMap->begin();
    for (; it != d_pModDirMap->end(); ++it)
    {
        delete it->second;
    }
    delete d_pModDirMap;
    d_pModDirMap = 0;
}

//------------------------------------------------------------------------------
// Write a string out to the different sinks
//------------------------------------------------------------------------------

int Tee::Write
(
    const SinkPriorities& sinkPri,
    const char*           text,
    size_t                textLen,
    const char*           prefix,
    ScreenPrintState      sps,
    QueueDrainMethod      qDrainMethod,
    const Mle::Context*   pMleContext
)
{
    MASSERT(text);

    // Note: The per-sink queue drain methods in this function must match
    // those in WillPrint()

    if (d_pScreenSink && (qDrainMethod != qdmDETACHED) && !d_PrintMleToStdOut &&
        (static_cast<int>(d_ScreenLevel) <=
                          static_cast<int>(sinkPri.ScreenPri)))
    {
        if (sps == SPS_END_LIST)
            sps = d_ScreenPrintState;

        d_pScreenSink->Print(text, textLen, prefix, nullptr, sps);
    }

   // The StdoutSink uses the screen level (since it should be a duplicate
   // of the screen) to determine what to print
   if (d_pStdoutSink && (qDrainMethod != qdmATTACHED) && !d_PrintMleToStdOut &&
       (static_cast<int>(d_ScreenLevel) <=
                         static_cast<int>(sinkPri.ScreenPri)))
   {
       d_pStdoutSink->Print(text, textLen, prefix, nullptr);
   }

   if (d_pFileSink && (qDrainMethod != qdmATTACHED) &&
       (static_cast<int>(d_FileLevel) <=
                       static_cast<int>(sinkPri.FilePri)))
   {
       d_pFileSink->Print(text, textLen, prefix);
   }

   // NOTE: If limiting MLE, can't print arbitrary prints to MLE log. Take out
   // all printf -> MLE prints.
   if (!IsMleLimited() && d_pMleSink && d_pMleSink->IsFileOpen() &&
       (qDrainMethod != qdmATTACHED) && pMleContext && HasContext(*pMleContext) &&
       (static_cast<int>(d_MleLevel) <= static_cast<int>(sinkPri.MlePri)))
   {
       // TODO get bytestream from TLS
       ByteStream bs;
       const size_t len = textLen -
           ((textLen && (text[textLen - 1] == '\n')) ? 1 : 0);
       Mle::Entry::print(&bs).EmitValue(Mle::ProtobufString(text, len), Mle::Output::Force);
       PrintMle(&bs, sinkPri.MlePri, *pMleContext);
   }

   if (d_pSerialSink && (qDrainMethod != qdmDETACHED) &&
       (static_cast<int>(d_SerialLevel) <=
                         static_cast<int>(sinkPri.SerialPri)))
   {
       d_pSerialSink->Print(text, textLen, prefix);
   }

   if (d_pCirlwlarSink && (qDrainMethod != qdmATTACHED) &&
       (static_cast<int>(d_CirlwlarLevel) <=
                           static_cast<int>(sinkPri.CirlwlarPri)))
   {
       d_pCirlwlarSink->Print(text, textLen, prefix);
   }

   if (d_pDebugLogSink && (qDrainMethod != qdmATTACHED) &&
       (static_cast<int>(d_CirlwlarLevel) <=
                           static_cast<int>(sinkPri.CirlwlarPri)))
   {
       d_pDebugLogSink->Print(text, textLen, prefix);
   }

   if (d_pDebuggerSink && (qDrainMethod != qdmDETACHED) &&
       (static_cast<int>(d_DebuggerLevel) <=
                           static_cast<int>(sinkPri.DebuggerPri)))
   {
       d_pDebuggerSink->Print(text, textLen, prefix);
   }

   if (d_pEthernetSink && (qDrainMethod != qdmDETACHED) &&
       (static_cast<int>(d_EthernetLevel) <=
                           static_cast<int>(sinkPri.EthernetPri)))
   {
       d_pEthernetSink->Print(text, textLen, prefix);
   }

   if (d_pAssertInfoSink && (qDrainMethod != qdmATTACHED) &&
       (static_cast<int>(d_AssertInfoLevel) <=
                             static_cast<int>(sinkPri.AssertInfoPri)))
   {
       d_pAssertInfoSink->Print(text, textLen, pMleContext);
   }

   Cpu::AtomicWrite(&d_IsResetScreenPause, 0);

   return static_cast<int>(textLen);
}

void Tee::InitTimestamp(UINT64 startTimeNs)
{
    MASSERT(d_StartTimeNS == 0);
    d_StartTimeNS = startTimeNs;
}

UINT64 Tee::GetTimeFromStart()
{
#ifdef SIM_BUILD
    if (Platform::IsChiplibLoaded() && Platform::GetSimulationMode() != Platform::Hardware)
    {
        // On simulation, use simulator clocks to timestamp log entries
        return Platform::GetSimulatorTime() * 1000'000'000ULL;
    }
#endif

    // On hardware, use wall clock time to timestamp log entries
    return GetWallTimeFromStart();
}

UINT64 Tee::GetWallTimeFromStart()
{
    const UINT64 lwrTime = Xp::GetWallTimeNS();
    return lwrTime - d_StartTimeNS;
}

void Tee::PrintPrefix(INT32 Pri, char *pPrefixStr, bool bAddThreadId)
{
    if (!d_EnablePrefix)
        return;

    size_t prefixPos = 0;

    // Print the thread ID being used
    if (Tee::IsPrependThreadId() && bAddThreadId)
    {
        const Tasker::ThreadID threadId = (Tasker::IsInitialized() ?
                                           Tasker::LwrrentThread() : 0);
        prefixPos += snprintf(&pPrefixStr[prefixPos],
                              Tee::MaxPrintPrefixSize - prefixPos,
                              "%d: ", threadId);
    }

    // Print the GPU ID being used
    if (Tee::IsPrependGpuId())
    {
        const INT32 gpuId = ErrorMap::DevInst();
        if (gpuId != -1)
        {
            prefixPos += snprintf(&pPrefixStr[prefixPos],
                                  Tee::MaxPrintPrefixSize - prefixPos,
                                  "gpu%d: ", gpuId);
        }
    }

    const char errorPrefix[] = "ERROR: ";
    const char warningPrefix[] = "WARNING: ";
    switch (Pri)
    {
        case Tee::PriError:
            snprintf(&pPrefixStr[prefixPos],
                     Tee::MaxPrintPrefixSize - prefixPos,
                     "%s", errorPrefix);
            prefixPos += strlen(errorPrefix);
            break;

        case Tee::PriWarn:
            snprintf(&pPrefixStr[prefixPos],
                     Tee::MaxPrintPrefixSize - prefixPos,
                     "%s", warningPrefix);
            prefixPos += strlen(warningPrefix);
            break;

        default: // No prefixes added for other priorities at this time
            break;
    }

    // Print the number of simulator clocks elapsed at the start
    // of every line
    if (Platform::IsChiplibLoaded())
    {
        if (Tee::GetUseSystemTime())
        {
            UINT64 wallTimeMS = Xp::GetWallTimeMS();

            // Colwert to YYYY-MM-DD HH:MM:SS
            time_t timer = static_cast<time_t>(wallTimeMS / 1000);
            tm* ts = localtime(&timer);

            prefixPos += strftime(&pPrefixStr[prefixPos],
                Tee::MaxPrintPrefixSize - prefixPos,
                "[%Y-%m-%d %H:%M:%S.",
                ts);

            // Add milliseconds to the prefix
            string milliSecondsStr = Utility::StrPrintf("%03u] ", (UINT32)(wallTimeMS % 1000));
            if (prefixPos + milliSecondsStr.size() <= Tee::MaxPrintPrefixSize)
            {
                strcat(pPrefixStr, milliSecondsStr.c_str());
                prefixPos += milliSecondsStr.size();
            }
        }
        if (Platform::GetSimulationMode() != Platform::Hardware
#ifdef INCLUDE_GPU
                && !Vgpu::IsSupported()
#endif
            )
        {
            prefixPos += snprintf(&pPrefixStr[prefixPos],
                     Tee::MaxPrintPrefixSize - prefixPos, "[%d] ",
                     (UINT32)Platform::GetSimulatorTime());
        }
    }
    else if (Tee::GetUseSystemTime())
    {
        // Non sim builds or HW platforms:
        UINT64 timeFromStart = GetTimeFromStart();
        snprintf(&pPrefixStr[prefixPos],
            Tee::MaxPrintPrefixSize - prefixPos, "[%06u.%09u] ",
            UINT32(timeFromStart/1000000000),
            UINT32(timeFromStart%1000000000));
    }
}

bool Tee::WillPrintToMle(INT32 pri)
{
    return d_pMleSink && d_pMleSink->IsFileOpen() && pri >= d_MleLevel;
}

void Tee::SetMleLimited(bool enable)
{
    ProtobufWriter::ProtobufPusher::EnforceFieldVisibility(enable);
}

bool Tee::IsMleLimited()
{
    return ProtobufWriter::ProtobufPusher::IsFieldVisibilityEnforced();
}

//------------------------------------------------------------------------------
// Class for temporary setting the assert info level to LevLow (to reduce the
// number of prints in some very print heavy sections of code)
//------------------------------------------------------------------------------
Tee::Level Tee::SetLowAssertLevel::s_OrigAssertLevel = Tee::LevNormal;
volatile INT32 Tee::SetLowAssertLevel::s_RefCount = 0;
Tee::SetLowAssertLevel::SetLowAssertLevel()
{
    const INT32 prevRefCount = Cpu::AtomicAdd(&s_RefCount, 1);
    if (prevRefCount == 0)
    {
        s_OrigAssertLevel = Tee::GetAssertInfoLevel();
        Tee::SetAssertInfoLevel(Tee::LevLow);
    }
}
Tee::SetLowAssertLevel::~SetLowAssertLevel()
{
    const INT32 prevRefCount = Cpu::AtomicAdd(&s_RefCount, -1);
    MASSERT(prevRefCount > 0);
    if (prevRefCount == 1)
    {
        Tee::SetAssertInfoLevel(s_OrigAssertLevel);
    }
}
