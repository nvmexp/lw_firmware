/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/tee.h"
#include "core/include/rc.h"
#include "core/include/massert.h"
#include "core/include/tasker.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "core/include/debuglogsink.h"
#include "core/include/chiplibtracecapture.h"
#include "inc/bytestream.h"
#include <cstdio>
#include <ctime>

//! True if a print is being put at the start of a line
static char s_Buff[Tee::MaxPrintSize + 1];
static char s_PrintPrefixBuff[Tee::MaxPrintPrefixSize + 1];

extern "C" {

//-----------------------------------------------------------------------------
//! \brief Queue a printf for processing by the printf thread
//!
//! \param Priority   : Priority for the print (see Tee::Priority enum).
//! \param ModuleCode : Module for the print (Tee::ModuleNone to ignore the
//!                     module
//! \param Sps        : Screen print state (see Tee::ScreenPrintState)
//! \param Format     : Format string for the print
//! \param RestOfArgs : Variable argument list for the format string
//!
//! \return Number of characters queued
//!
INT32 ModsExterlwAPrintf
(
   INT32                     Priority,
   UINT32                    ModuleCode,
   UINT32                    Sps,
   const char              * Format,
   va_list                   RestOfArgs
)
{
    MASSERT(Format != 0);

#ifndef MODS_INCLUDE_DEBUG_PRINTS
    if (Priority <= Tee::PriSecret)
    {
        return 0;
    }
#endif

    // Call TeeInit() the first time we print something.  If TeeInit()
    // fails to initialize, presumably because TeeDestroy() was
    // already called, then fall back on vprintf().
    //
    if (!Tee::IsInitialized())
    {
        Tee::TeeInit();
        if (!Tee::IsInitialized())
        {
            vprintf(Format, RestOfArgs);
            return 0;
        }
    }

    //
    // Printf is not reentrant.
    // Make sure we don't step on s_Buff if we are called from an ISR.
    //
    static volatile int  PrintfMutex = 0;
    if (PrintfMutex)
    {
        return 0;
    }
    PrintfMutex = 1;

    INT32 CharactersWritten = 0;

    Tee::SinkPriorities SinkPri(static_cast<Tee::Priority>(Priority));
    Tee::ScreenPrintState screenState =
        static_cast<Tee::ScreenPrintState>(Sps);

    // If no sinks have a high enough verbosity level to pass this output,
    // save CPU time by skipping the vsprintf.

    if (Tee::WillPrint(SinkPri) &&
        ((static_cast<Tee::Priority>(Priority) >= Tee::PriNormal) ||
        Tee::IsModuleEnabled(ModuleCode)))
    {
        // Write out the formatted string.
        CharactersWritten = vsnprintf(s_Buff, sizeof(s_Buff) - 1, Format, RestOfArgs);
        MASSERT(CharactersWritten < (INT32) sizeof(s_Buff));
        s_PrintPrefixBuff[0] = '\0';

        // Print the thread ID being used
        Tee::PrintPrefix(Priority, s_PrintPrefixBuff, (s_Buff[0] != '\n'));

        Mle::Context printContext;
        if (Tee::WillPrintToMle(Priority))
        {
            Tee::SetContext(&printContext);
        }

        Tee::Write(SinkPri, s_Buff, CharactersWritten, s_PrintPrefixBuff,
                   screenState, Tee::qdmBOTH, &printContext);

        if (ChiplibTraceDumper::GetPtr()->DumpChiplibTrace())
        {
            ChiplibOpBlock *lwrBlock = ChiplibTraceDumper::GetPtr()->GetLwrrentChiplibOpBlock();
            lwrBlock->AddAnnotOp(s_PrintPrefixBuff, s_Buff);
        }
    }

    STACK_CHECK();
    PrintfMutex = 0;
    return CharactersWritten;
}
} // extern "C"

//-----------------------------------------------------------------------------
//! \brief Tee initialization to be done after Tasker is initialized (stub)
//!
//! \return OK
RC Tee::TeeInitAfterTasker()
{
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Tee shutdown to be called immediately prior to shutting down
//!        tasker (stub)
//!
//! \return OK
RC Tee::TeeShutdownBeforeTasker()
{
    GetDebugLogSink()->KillThread();
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Stub function for enabling/disabling queued prints (always fails
//!        since this file does not support queued prints)
//!
//! \param bEnable : true to enable queued prints, false to disable (unused)
//!
//! \return UNSUPPORTED_FUNCTION
RC Tee::TeeEnableQueuedPrints(bool bEnable)
{
    if (bEnable)
        return RC::UNSUPPORTED_FUNCTION;
    else
        return OK; // if we don't support queue prints this is a NO-OP
}

//-----------------------------------------------------------------------------
//! \brief Return whether queued printing is enabled
//!
//! \return false (queued printing not supported)
bool Tee::QueuedPrintsEnabled()
{
    return false;
}

//-----------------------------------------------------------------------------
//! \brief Flush the queued prints to all the sinks (stub)
//!
//! \param TimeoutMs : Timeout in milliseconds for flusing the print
//!                    queue (unused)
//!
//! \return OK
RC Tee::TeeFlushQueuedPrints(FLOAT64 TimeoutMs)
{
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Private initialization function for queue data structures (stub)
void Tee::TeeQueueInit()
{
}

//-----------------------------------------------------------------------------
//! \brief Private initialization function for queue data structures (stub)
void Tee::TeeQueueDestroy()
{
}

