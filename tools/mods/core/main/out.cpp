/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <algorithm>

#include "jsstr.h"

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

// Out JavaScript object.

#include "core/include/script.h"
#include "core/include/tee.h"
#include "core/include/filesink.h"
#include "core/include/debuglogsink.h"
#include "core/include/sersink.h"
#include "core/include/stdoutsink.h"
#include "core/include/jscript.h"
#include "core/include/massert.h"
#include "core/include/globals.h"
#include "core/include/pool.h"
#include "core/include/utility.h"
#include "core/include/cmdline.h"

//------------------------------------------------------------------------------
// Out object, properties, and methods.
//------------------------------------------------------------------------------

// Object

JS_CLASS(Out);

static SObject Out_Object
(
   "Out",
   OutClass,
   0,
   0,
   "Output tee stream."
);

// Properties

static SProperty Out_PriNone
(
   Out_Object,
   "PriNone",
   0,
   Tee::PriNone,
   0,
   0,
   JSPROP_READONLY,
   "Never priority."
);

static SProperty Out_PriDebug
(
   Out_Object,
   "PriDebug",
   0,
   Tee::PriSecret,
   0,
   0,
   JSPROP_READONLY,
   "Debug priority."
);

static SProperty Out_PriLow
(
   Out_Object,
   "PriLow",
   0,
   Tee::PriLow,
   0,
   0,
   JSPROP_READONLY,
   "Low priority."
);

static SProperty Out_PriNormal
(
   Out_Object,
   "PriNormal",
   0,
   Tee::PriNormal,
   0,
   0,
   JSPROP_READONLY,
   "Normal priority."
);

static SProperty Out_PriHigh
(
   Out_Object,
   "PriHigh",
   0,
   Tee::PriHigh,
   0,
   0,
   JSPROP_READONLY,
   "High priority."
);

static SProperty Out_PriError
(
   Out_Object,
   "PriError",
   0,
   Tee::PriError,
   0,
   0,
   JSPROP_READONLY,
   "Error priority."
);

static SProperty Out_PriWarn
(
   Out_Object,
   "PriWarn",
   0,
   Tee::PriWarn,
   0,
   0,
   JSPROP_READONLY,
   "Warning priority."
);

static SProperty Out_PriAlways
(
   Out_Object,
   "PriAlways",
   0,
   Tee::PriAlways,
   0,
   0,
   JSPROP_READONLY,
   "Always priority."
);

static SProperty Out_ScreenOnly
(
   Out_Object,
   "ScreenOnly",
   0,
   Tee::ScreenOnly,
   0,
   0,
   JSPROP_READONLY,
   "Print to screen only."
);

static SProperty Out_FileOnly
(
   Out_Object,
   "FileOnly",
   0,
   Tee::FileOnly,
   0,
   0,
   JSPROP_READONLY,
   "Print to file only."
);

static SProperty Out_SerialOnly
(
   Out_Object,
   "SerialOnly",
   0,
   Tee::SerialOnly,
   0,
   0,
   JSPROP_READONLY,
   "Print to serial only."
);

static SProperty Out_CirlwlarOnly
(
   Out_Object,
   "CirlwlarOnly",
   0,
   Tee::CirlwlarOnly,
   0,
   0,
   JSPROP_READONLY,
   "Print to cirlwlar only."
);

static SProperty Out_DebuggerOnly
(
   Out_Object,
   "DebuggerOnly",
   0,
   Tee::DebuggerOnly,
   0,
   0,
   JSPROP_READONLY,
   "Print to debugger only."
);

static SProperty Out_EthernetOnly
(
   Out_Object,
   "EthernetOnly",
   0,
   Tee::EthernetOnly,
   0,
   0,
   JSPROP_READONLY,
   "Print to ethernet only."
);

static SProperty Out_LevAlways
(
   Out_Object,
   "LevAlways",
   0,
   Tee::LevAlways,
   0,
   0,
   JSPROP_READONLY,
   "Always level."
);

static SProperty Out_LevDebug
(
   Out_Object,
   "LevDebug",
   0,
   Tee::LevDebug,
   0,
   0,
   JSPROP_READONLY,
   "Debug level."
);

static SProperty Out_LevLow
(
   Out_Object,
   "LevLow",
   0,
   Tee::LevLow,
   0,
   0,
   JSPROP_READONLY,
   "Low level."
);

static SProperty Out_LevNormal
(
   Out_Object,
   "LevNormal",
   0,
   Tee::LevNormal,
   0,
   0,
   JSPROP_READONLY,
   "Normal level."
);

static SProperty Out_LevHigh
(
   Out_Object,
   "LevHigh",
   0,
   Tee::LevHigh,
   0,
   0,
   JSPROP_READONLY,
   "High level."
);

static SProperty Out_LevNone
(
   Out_Object,
   "LevNone",
   0,
   Tee::LevNone,
   0,
   0,
   JSPROP_READONLY,
   "None level."
);

static SProperty Out_Truncate
(
   Out_Object,
   "Truncate",
   0,
   FileSink::Truncate,
   0,
   0,
   JSPROP_READONLY,
   "Truncate open mode."
);

static SProperty Out_Append
(
   Out_Object,
   "Append",
   0,
   FileSink::Append,
   0,
   0,
   JSPROP_READONLY,
   "Append open mode."
);

P_(Out_Get_IsUIRemote);
static SProperty Out_IsUIRemote
(
   Out_Object,
   "IsUIRemote",
   0,
   false,
   Out_Get_IsUIRemote,
   0,
   JSPROP_READONLY,
   "Indicates whether user interface is remote."
);

// This name is bollocks -- on purpose.
// This property tells us whether the log is unencrypted.
// We don't necessarily want to name this property IsUnencrypted
// to avoid drawing attention to it.
P_(Out_Get_IsAccessible);
static SProperty Out_IsAccessible
(
   Out_Object,
   "IsAccessible",
   0,
   false,
   Out_Get_IsAccessible,
   0,
   JSPROP_READONLY,
   "Indicates whether user interface is accessible."
);

// Get / Set

P_(Out_Get_ScreenLevel);
P_(Out_Set_ScreenLevel);
static SProperty Out_ScreenLevel
(
   Out_Object,
   "ScreenLevel",
   0,
   Tee::LevNormal,
   Out_Get_ScreenLevel,
   Out_Set_ScreenLevel,
   0,
   "Output screen level."
);

P_(Out_Get_FileLevel);
P_(Out_Set_FileLevel);
static SProperty Out_FileLevel
(
   Out_Object,
   "FileLevel",
   0,
   Tee::LevNormal,
   Out_Get_FileLevel,
   Out_Set_FileLevel,
   0,
   "Output file level."
);

P_(Out_Get_MleLevel);
P_(Out_Set_MleLevel);
static SProperty Out_MleLevel
(
   Out_Object,
   "MleLevel",
   0,
   Tee::LevNormal,
   Out_Get_MleLevel,
   Out_Set_MleLevel,
   0,
   "Output mle level."
);

P_(Out_Get_SerialLevel);
P_(Out_Set_SerialLevel);
static SProperty Out_SerialLevel
(
   Out_Object,
   "SerialLevel",
   0,
   Tee::LevNormal,
   Out_Get_SerialLevel,
   Out_Set_SerialLevel,
   0,
   "Output serial level."
);

P_(Out_Get_CirlwlarLevel);
P_(Out_Set_CirlwlarLevel);
static SProperty Out_CirlwlarLevel
(
   Out_Object,
   "CirlwlarLevel",
   0,
   Tee::LevNormal,
   Out_Get_CirlwlarLevel,
   Out_Set_CirlwlarLevel,
   0,
   "Output cirlwlar level."
);

P_(Out_Get_DebuggerLevel);
P_(Out_Set_DebuggerLevel);
static SProperty Out_DebuggerLevel
(
   Out_Object,
   "DebuggerLevel",
   0,
   Tee::LevNormal,
   Out_Get_DebuggerLevel,
   Out_Set_DebuggerLevel,
   0,
   "Output debugger level."
);

P_(Out_Get_AssertInfoLevel);
P_(Out_Set_AssertInfoLevel);
static SProperty Out_AssertInfoLevel
(
   Out_Object,
   "AssertInfoLevel",
   0,
   Tee::LevLow,
   Out_Get_AssertInfoLevel,
   Out_Set_AssertInfoLevel,
   0,
   "Output assert information level."
);

P_(Out_Get_AlwaysFlush);
P_(Out_Set_AlwaysFlush);
static SProperty Out_AlwaysFlush
(
   Out_Object,
   "AlwaysFlush",
   0,
   false,
   Out_Get_AlwaysFlush,
   Out_Set_AlwaysFlush,
   0,
   "If true, always flush logfile to disk immediately after writing."
);

P_(Out_Get_PrependThreadId);
P_(Out_Set_PrependThreadId);
static SProperty Out_PrependThreadId
(
   Out_Object,
   "PrependThreadId",
   0,
   false,
   Out_Get_PrependThreadId,
   Out_Set_PrependThreadId,
   0,
   "Output prepend thread ID state."
);

CLASS_PROP_READWRITE_LWSTOM_FULL
(
    Out,
    PrependGpuId,
    "Prepend the GPU ID to each line of output",
    0,
    false
);

P_(Out_Get_EnablePrefix);
P_(Out_Set_EnablePrefix);
static SProperty Out_EnablePrefix
(
   Out_Object,
   "EnablePrefix",
   0,
   true,
   Out_Get_EnablePrefix,
   Out_Set_EnablePrefix,
   0,
   "Enable adding line prefixes to output."
);

P_(Out_Get_FileName);
static SProperty Out_FileName
(
   Out_Object,
   "FileName",
   0,
   "",
   Out_Get_FileName,
   0,
   0,
   "Output file name."
);

P_(Out_Get_FileNameOnly);
static SProperty Out_FileNameOnly
(
   Out_Object,
   "FileNameOnly",
   0,
   "",
   Out_Get_FileNameOnly,
   0,
   0,
   "Output file name without preceding directories."
);

P_(Out_Get_RenameOnClose);
P_(Out_Set_RenameOnClose);
static SProperty Out_RenameOnClose
(
   Out_Object,
   "RenameOnClose",
   0,
   "",
   Out_Get_RenameOnClose,
   Out_Set_RenameOnClose,
   0,
   "File name to rename to after the log file is closed."
);

P_(Out_Set_FileLimitMb);
static SProperty Out_FileLimitMb
(
   Out_Object,
   "FileLimitMb",
   0,
   2048,
   0,
   Out_Set_FileLimitMb,
   0,
   "Output screen level."
);
// Methods and Tests

C_(Out_Print);
static STest Out_Print
(
   Out_Object,
   "Print",
   C_Out_Print,
   2,
   "Print strings to the screen and file."
);

C_(Out_DumpTextFile);
static STest Out_DumpTextFile
(
   Out_Object,
   "DumpTextFile",
   C_Out_DumpTextFile,
   2,
   "Dump a text file."
);

C_(Out_EnableSerial);
static STest Out_EnableSerial
(
   Out_Object,
   "EnableSerial",
   C_Out_EnableSerial,
   0,
   "Enable serial output"
);

C_(Out_DisableSerial);
static STest Out_DisableSerial
(
   Out_Object,
   "DisableSerial",
   C_Out_DisableSerial,
   0,
   "Disable serial output"
);

C_(Out_EnableStdout);
static STest Out_EnableStdout
(
   Out_Object,
   "EnableStdout",
   C_Out_EnableStdout,
   0,
   "Enable stdout output"
);

C_(Out_DisableStdout);
static STest Out_DisableStdout
(
   Out_Object,
   "DisableStdout",
   C_Out_DisableStdout,
   0,
   "Disable stdout output"
);

C_(Out_Printf);
static SMethod Out_Printf
(
   Out_Object,
   "Printf",
   C_Out_Printf,
   2,
   "Print formatted string to the output devices, "
      "returns number of characters written."
);

C_(Out_Sprintf);
static SMethod Out_Sprintf
(
   Out_Object,
   "Sprintf",
   C_Out_Sprintf,
   1,
   "Return formatted string."
);

C_(Out_ToHex);
static SMethod Out_ToHex
(
   Out_Object,
   "ToHex",
   C_Out_ToHex,
   1,
   "Colwert number to a hexadecimal string."
);

C_(Out_Open);
static STest Out_Open
(
   Out_Object,
   "Open",
   C_Out_Open,
   2,
   "Open a log file."
);

C_(Out_Close);
static STest Out_Close
(
   Out_Object,
   "Close",
   C_Out_Close,
   0,
   "Close a log file."
);

C_(Out_Flush);
static STest Out_Flush
(
   Out_Object,
   "Flush",
   C_Out_Flush,
   0,
   "Flush the log file to disk."
);

C_(Out_EnableMessages);
static SMethod Out_EnableMessages
(
   Out_Object,
   "EnableMessages",
   C_Out_EnableMessages,
   1,
   "Enable debug messages for a module."
);

C_(Out_DisableMessages);
static SMethod Out_DisableMessages
(
   Out_Object,
   "DisableMessages",
   C_Out_DisableMessages,
   1,
   "Disable debug messages for a module."
);

C_(Out_SetEnableQueuedPrintf);
static SMethod Out_SetEnableQueuedPrintf
(
   Out_Object,
   "SetEnableQueuedPrintf",
   C_Out_SetEnableQueuedPrintf,
   1,
   "Enable/Disable queued printf for better multi threaded compatibility."
);

C_(Out_StartRecordingModsHeader);
static SMethod Out_StartRecordingModsHeader
(
    Out_Object,
    "StartRecordingModsHeader",
    C_Out_StartRecordingModsHeader,
    0,
    "Start recording of log header content."
);

C_(Out_StopRecordingModsHeader);
static SMethod Out_StopRecordingModsHeader
(
    Out_Object,
    "StopRecordingModsHeader",
    C_Out_StopRecordingModsHeader,
    0,
    "Stop recording of log header content."
);

C_(Out_SwitchLogFilename);
static SMethod Out_SwitchLogFilename
(
    Out_Object,
    "SwitchLogFilename",
    C_Out_SwitchLogFilename,
    1,
    "Close current .log/.mle files and resume logging into new ones with provided filename."
);

//------------------------------------------------------------------------------
// Implementation
//------------------------------------------------------------------------------

// Get / Set

P_(Out_Get_IsUIRemote)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   const bool isRemote = CommandLine::UserConsole() == CommandLine::UC_Remote;
   if (OK != JavaScriptPtr()->ToJsval(isRemote, pValue))
   {
       *pValue = JSVAL_NULL;
       return JS_FALSE;
   }
   return JS_TRUE;
}

P_(Out_Get_IsAccessible)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   const bool isEncrypted = Tee::GetFileSink()->IsEncrypted();
   if (RC::OK != JavaScriptPtr()->ToJsval(!isEncrypted, pValue))
   {
       *pValue = JSVAL_NULL;
       return JS_FALSE;
   }
   return JS_TRUE;
}

P_(Out_Get_ScreenLevel)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   if (OK !=JavaScriptPtr()->ToJsval(Tee::GetScreenLevel(), pValue))
   {
      JS_ReportError(pContext, "Failed to get Out.ScreenLevel.");
      *pValue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

P_(Out_Set_ScreenLevel)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   // Colwert argument.
   INT32 Level = 0;
   if (OK != JavaScriptPtr()->FromJsval(*pValue, &Level))
   {
      JS_ReportError(pContext, "Failed to set Out.ScreenLevel.");
      return JS_FALSE;
   }

   // Check the level is in range.
   if ((Level < Tee::LevAlways) || (Tee::LevNone < Level))
   {
      JS_ReportError(pContext, "Failed to set Out.ScreenLevel.");
      return JS_FALSE;
   }

   // Set the level.
   Tee::SetScreenLevel(static_cast<Tee::Level>(Level));

   return JS_TRUE;
}

P_(Out_Set_FileLimitMb)
{
   MASSERT(pContext && pValue);
   // Colwert argument.
   UINT32 LimitMb = 0;
   if (OK != JavaScriptPtr()->FromJsval(*pValue, &LimitMb))
   {
      return JS_FALSE;
   }
   Tee::GetMleSink()->SetFileLimitMb(LimitMb);
   Tee::GetFileSink()->SetFileLimitMb(LimitMb);
   Tee::GetDebugLogSink()->SetFileLimitMb(LimitMb);
   return JS_TRUE;
}

P_(Out_Get_FileLevel)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   if (OK != JavaScriptPtr()->ToJsval(Tee::GetFileLevel(), pValue))
   {
      JS_ReportError(pContext, "Failed to get Out.FileLevel.");
      *pValue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

P_(Out_Set_FileLevel)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   // Colwert argument.
   UINT32 level = 0;
   if (OK != JavaScriptPtr()->FromJsval(*pValue, &level))
   {
      JS_ReportError(pContext, "Failed to set Out.FileLevel.");
      return JS_FALSE;
   }

   // Check the level is in range.
   if ((level < Tee::LevAlways) || (Tee::LevNone < level))
   {
      JS_ReportError(pContext, "Failed to set Out.FileLevel.");
      return JS_FALSE;
   }

   // Set the level.
   Tee::SetFileLevel(static_cast<Tee::Level>(level));

   return JS_TRUE;
}

P_(Out_Get_MleLevel)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   if (OK != JavaScriptPtr()->ToJsval(Tee::GetMleLevel(), pValue))
   {
      JS_ReportError(pContext, "Failed to get Out.MleLevel.");
      *pValue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

P_(Out_Set_MleLevel)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   // Colwert argument.
   UINT32 level = 0;
   if (OK != JavaScriptPtr()->FromJsval(*pValue, &level))
   {
      JS_ReportError(pContext, "Failed to set Out.MleLevel.");
      return JS_FALSE;
   }

   // Check the level is in range.
   if ((level < Tee::LevAlways) || (Tee::LevNone < level))
   {
      JS_ReportError(pContext, "Failed to set Out.MleLevel.");
      return JS_FALSE;
   }

   // Set the level.
   Tee::SetMleLevel(static_cast<Tee::Level>(level));

   return JS_TRUE;
}

P_(Out_Get_FileName)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);
   FileSink *pFileSink = Tee::GetFileSink();

   if
   (
         (pFileSink == 0)
      || (OK != JavaScriptPtr()->ToJsval(pFileSink->GetFileName(), pValue))
   )
   {
      JS_ReportError(pContext, "Failed to get Out.FileName.");
      *pValue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

P_(Out_Get_FileNameOnly)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);
   FileSink *pFileSink = Tee::GetFileSink();

   if
   (
         (pFileSink == 0)
      || (OK != JavaScriptPtr()->ToJsval(pFileSink->GetFileNameOnly(), pValue))
   )
   {
      JS_ReportError(pContext, "Failed to get Out.FileNameOnly.");
      *pValue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

P_(Out_Get_RenameOnClose)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);
    FileSink *fileSink = Tee::GetFileSink();

    if
    (
        (0 == fileSink) ||
        (OK != JavaScriptPtr()->ToJsval(fileSink->GetRenameOnClose(), pValue))
    )
    {
        JS_ReportError(pContext, "Failed to get Out.RenameOnClose.");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

P_(Out_Set_RenameOnClose)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    FileSink *fileSink = Tee::GetFileSink();
    if (0 != fileSink)
    {
        string fileName;
        if (OK != JavaScriptPtr()->FromJsval(*pValue, &fileName))
        {
            JS_ReportError(pContext, "Failed to set Out.RenameOnClose.");
            return JS_FALSE;
        }
        fileSink->SetRenameOnClose(fileName);
    }

    return JS_TRUE;
}

P_(Out_Get_AlwaysFlush)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);
   FileSink *pFileSink = Tee::GetFileSink();

   if
   (
         (pFileSink == 0)
      || (OK != JavaScriptPtr()->ToJsval(pFileSink->GetAlwaysFlush(), pValue))
   )
   {
      JS_ReportError(pContext, "Failed to get Out.AlwaysFlush.");
      *pValue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

P_(Out_Set_AlwaysFlush)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   // Colwert argument.
   bool af = false;
   if (OK != JavaScriptPtr()->FromJsval(*pValue, &af))
   {
      JS_ReportError(pContext, "Failed to set Out.AlwaysFlush.");
      return JS_FALSE;
   }

   FileSink * pFileSink = Tee::GetFileSink();
   if (pFileSink)  pFileSink->SetAlwaysFlush(af);

   return JS_TRUE;
}

P_(Out_Get_SerialLevel)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   if (OK != JavaScriptPtr()->ToJsval(Tee::GetSerialLevel(), pValue))
   {
      JS_ReportError(pContext, "Failed to get Out.SerialLevel.");
      *pValue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

P_(Out_Set_SerialLevel)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   // Colwert argument.
   INT32 Level = 0;
   if (OK != JavaScriptPtr()->FromJsval(*pValue, &Level))
   {
      JS_ReportError(pContext, "Failed to set Out.SerialLevel.");
      return JS_FALSE;
   }

   // Check the level is in range.
   if ((Level < Tee::LevAlways) || (Tee::LevNone < Level))
   {
      JS_ReportError(pContext, "Failed to set Out.SerialLevel.");
      return JS_FALSE;
   }

   // Set the level.
   Tee::SetSerialLevel( static_cast<Tee::Level>(Level));

   return JS_TRUE;
}

P_(Out_Get_CirlwlarLevel)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   if (OK != JavaScriptPtr()->ToJsval(
             Tee::GetCirlwlarLevel(), pValue))
   {
      JS_ReportError(pContext, "Failed to get Out.CirlwlarLevel.");
      *pValue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

P_(Out_Set_CirlwlarLevel)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   // Colwert argument.
   INT32 Level = 0;
   if (OK != JavaScriptPtr()->FromJsval(*pValue, &Level))
   {
      JS_ReportError(pContext, "Failed to set Out.CirlwlarLevel.");
      return JS_FALSE;
   }

   // Check the level is in range.
   if ((Level < Tee::LevAlways) || (Tee::LevNone < Level))
   {
      JS_ReportError(pContext, "Failed to set Out.CirlwlarLevel.");
      return JS_FALSE;
   }

   // Set the level.
   Tee::SetCirlwlarLevel( static_cast<Tee::Level>(Level));

   return JS_TRUE;
}

P_(Out_Get_PrependThreadId)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   if (OK != JavaScriptPtr()->ToJsval(
             Tee::IsPrependThreadId(), pValue))
   {
      JS_ReportError(pContext, "Failed to get Out.PrependThreadId.");
      *pValue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

P_(Out_Set_PrependThreadId)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   // Colwert argument.
   bool state = false;
   if (OK != JavaScriptPtr()->FromJsval(*pValue, &state))
   {
      JS_ReportError(pContext, "Failed to set Out.PrependThreadId.");
      return JS_FALSE;
   }

   // Set the state.
   Tee::SetPrependThreadId(state);

   return JS_TRUE;
}

P_(Out_Get_PrependGpuId)
{
    MASSERT(pContext != nullptr);
    MASSERT(pValue   != nullptr);

    if (JavaScriptPtr()->ToJsval(Tee::IsPrependGpuId(), pValue) != RC::OK)
    {
        JS_ReportError(pContext, "Failed to get Out.PrependGpuId.");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

P_(Out_Set_PrependGpuId)
{
    MASSERT(pContext != nullptr);
    MASSERT(pValue   != nullptr);

    bool state = false;
    if (JavaScriptPtr()->FromJsval(*pValue, &state) != RC::OK)
    {
        JS_ReportError(pContext, "Failed to set Out.PrependGpuId.");
        return JS_FALSE;
    }

    Tee::SetPrependGpuId(state);
    return JS_TRUE;
}

P_(Out_Get_EnablePrefix)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   if (OK != JavaScriptPtr()->ToJsval(
             Tee::GetEnablePrefix(), pValue))
   {
      JS_ReportError(pContext, "Failed to get Out.EnablePrefix.");
      *pValue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

P_(Out_Set_EnablePrefix)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   // Colwert argument.
   bool state = false;
   if (OK != JavaScriptPtr()->FromJsval(*pValue, &state))
   {
      JS_ReportError(pContext, "Failed to set Out.EnablePrefix.");
      return JS_FALSE;
   }

   // Set the state.
   Tee::SetEnablePrefix(state);

   return JS_TRUE;
}

P_(Out_Get_DebuggerLevel)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   if (OK != JavaScriptPtr()->ToJsval(
             Tee::GetDebuggerLevel(), pValue))
   {
      JS_ReportError(pContext, "Failed to get Out.DebuggerLevel.");
      *pValue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

P_(Out_Set_DebuggerLevel)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   // Colwert argument.
   INT32 Level = 0;
   if (OK != JavaScriptPtr()->FromJsval(*pValue, &Level))
   {
      JS_ReportError(pContext, "Failed to set Out.DebuggerLevel.");
      return JS_FALSE;
   }

   // Check the level is in range.
   if ((Level < Tee::LevAlways) || (Tee::LevNone < Level))
   {
      JS_ReportError(pContext, "Failed to set Out.DebuggerLevel.");
      return JS_FALSE;
   }

   // Set the level.
   Tee::SetDebuggerLevel( static_cast<Tee::Level>(Level));

   return JS_TRUE;
}

P_(Out_Get_AssertInfoLevel)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   if (OK != JavaScriptPtr()->ToJsval(
             Tee::GetAssertInfoLevel(), pValue))
   {
      JS_ReportError(pContext, "Failed to get Out.AssertInfo.");
      *pValue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

P_(Out_Set_AssertInfoLevel)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   // Colwert argument.
   INT32 Level = 0;
   if (OK != JavaScriptPtr()->FromJsval(*pValue, &Level))
   {
      JS_ReportError(pContext, "Failed to set Out.AssertInfo.");
      return JS_FALSE;
   }

   // Check the level is in range.
   if ((Level < Tee::LevAlways) || (Tee::LevNone < Level))
   {
      JS_ReportError(pContext, "Failed to set Out.AssertInfo.");
      return JS_FALSE;
   }

   // Set the level.
   Tee::SetAssertInfoLevel( static_cast<Tee::Level>(Level));

   return JS_TRUE;
}

// Methods and Tests

// STest
C_(Out_Print)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;

   // The function should have at least two arguments;
   INT32 Priority = 0;
   if
   (
         (NumArguments < 2)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Priority))
   )
   {
      JS_ReportError(pContext, "Usage: Out.Print(priority, strings ...)");
      return JS_FALSE;
   }

   if
   (
         (Priority != Tee::PriDebug)
      && (Priority != Tee::PriLow)
      && (Priority != Tee::PriNormal)
      && (Priority != Tee::PriHigh)
      && (Priority != Tee::ScreenOnly)
      && (Priority != Tee::FileOnly)
      && (Priority != Tee::SerialOnly)
      && (Priority != Tee::CirlwlarOnly)
      && (Priority != Tee::DebuggerOnly)
      && (Priority != Tee::EthernetOnly)
      && (Priority != Tee::PriError)
      && (Priority != Tee::PriWarn)
   )
   {
      JS_ReportError(pContext, "invalid output priority");
      *pReturlwalue = JSVAL_NULL;
      return JS_FALSE;
   }

   RC     rc = OK;
   string String;

   for (uintN Arg = 1; Arg < NumArguments; ++Arg)
   {
      if (OK != (rc = pJavaScript->FromJsval(pArguments[Arg], &String)))
         RETURN_RC(rc);

      Printf(Priority, "%s", String.c_str());
   }

   RETURN_RC(OK);
}

// STest
C_(Out_DumpTextFile)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;

   // The function should have two arguments;
   INT32 Priority = 0;
   string String;
   if
   (
         (NumArguments != 2)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Priority))
      || (OK != pJavaScript->FromJsval(pArguments[1], &String))
   )
   {
      JS_ReportError(pContext, "Usage: Out.DumpTextFile(priority, fileName)");
      return JS_FALSE;
   }

   if
   (
         (Priority != Tee::PriDebug)
      && (Priority != Tee::PriLow)
      && (Priority != Tee::PriNormal)
      && (Priority != Tee::PriHigh)
      && (Priority != Tee::PriAlways)
      && (Priority != Tee::ScreenOnly)
      && (Priority != Tee::FileOnly)
      && (Priority != Tee::SerialOnly)
      && (Priority != Tee::CirlwlarOnly)
      && (Priority != Tee::DebuggerOnly)
      && (Priority != Tee::EthernetOnly)
      && (Priority != Tee::PriError)
      && (Priority != Tee::PriWarn)
   )
   {
      JS_ReportError(pContext, "invalid output priority");
      *pReturlwalue = JSVAL_NULL;
      return JS_FALSE;
   }

   RETURN_RC(Utility::DumpTextFile(Priority, String.c_str()));
}

struct FormatData
{
    unsigned int width;
    bool         isLong;
    bool         leftAlign;
    char         type;
};

template <typename CharType, typename OutputIterator>
static RC JsSprintf
(
   JSContext                    *pContext,
   uintN                         numArguments,
   jsval                        *pArguments,
   const basic_string<CharType> &format,
   OutputIterator                outIt
)
{
    typedef basic_string<CharType>              StringType;
    typedef typename StringType::const_iterator const_iterator;
    typedef typename StringType::size_type      size_type;

    JavaScriptPtr   pJavaScript;
    UINT32          fieldNum = 0;
    size_type       startPos = 0;
    size_type       endPos = 0;
    bool            badFormat = false;
    size_type       formatLength = format.size();

    UINT32          maxChars = 4095;
    const UINT32    maxCharsIncr = 1024;
    vector<wchar_t> buff(maxChars + 1, 0);

    FormatData      formatData;

    while (endPos < formatLength)
    {
        // Find the beginning of the next format field.
        endPos = format.find('%', startPos);

        // Output any remaining part of the string if there are no more format fields.
        if (StringType::npos == endPos)
        {
            copy(format.begin() + startPos, format.end(), outIt);
            break; // while (endPos < formatLength)
        }

        // Output the string up to the format field.
        copy(format.begin() + startPos, format.begin() + endPos, outIt);

        // Point to the start of the format field.
        startPos = endPos;

        // Check if this is a literal '%'.
        if ('%' == format[startPos + 1])
        {
            *outIt++ = '%';

            // Skip %% and continue.
            endPos += 2;
            startPos = endPos;
            continue;
        }

        // An argument must be passed along for every format field.
        if (++fieldNum > numArguments)
        {
            badFormat = true;
            break;
        }

        //
        // Parse the format field.
        // %[flags][width][.precision][long]type
        //    flags    : [-+0 #]
        //    width    : \d+             (note: we do not support the width specifier, '*', syntax.)
        //    precision: \.\d+
        //    long     : ll              (indicates a 64-bit type, supports only ouxX)
        //    type     : [cdiouxXeEfgGs]
        //

        // Find the end of the format field.
        static const CharType formatChars[] = { '-', '+', '0', ' ', '#', '.', '1', '2', '3', '4',
                                                '5', '6', '7', '8', '9', '\0' };
        if (StringType::npos == (endPos = format.find_first_not_of(&formatChars[0], startPos + 1)))
        {
            badFormat = true;
            break;
        }

        // Skip over the flags.
        static const CharType flagChars[] = { '-', '+', '0', ' ', '#', '\0' };
        size_type pos1 = format.find_first_not_of(&flagChars[0], startPos + 1);

        const_iterator flagsStart = format.begin() + startPos + 1;
        const_iterator flagsFinish = format.begin() + pos1;
        formatData.leftAlign = (flagsFinish != find(flagsStart, flagsFinish, '-'));

        // We do not support width specifiers.
        if ('*' == format[pos1])
        {
            JS_ReportWarning(pContext, "Width specifier, '*', is not supported.");
            badFormat = true;
            break;
        }

        // Store the width.
        formatData.width = 0;
        static const CharType digitChars[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '\0' };
        size_type pos2 = format.find_first_not_of(&digitChars[0], pos1);
        if (pos2 > pos1)
        {
            wstring s(&format[pos1], &format[pos2]);
            formatData.width = wcstol(s.c_str(), nullptr, 10);
        }

        // Check if the user specified a width greater than the maximum allowed limit.
        if (formatData.width > maxChars)
        {
            maxChars = ALIGN_UP(formatData.width, maxCharsIncr) - 1;
            buff.resize(maxChars + 1);
        }

        // Check if the user correctly specified the width.
        pos1 = pos2;
        if ('.' == format[pos1])
        {
            pos2 = format.find_first_not_of(&digitChars[0], ++pos1);
            if ((pos1 == pos2) || (pos2 != endPos))
            {
                badFormat = true;
                break;
            }
        }

        formatData.isLong = ((format[endPos] == 'l') && (format[endPos + 1] == 'l'));

        if (formatData.isLong)
        {
            formatData.type = static_cast<char>(format[endPos + 2]);
            switch (formatData.type)
            {
                case 'o':
                case 'u':
                case 'x':
                case 'X':
                    break;
                default:
                    JS_ReportError(pContext, "ll%c is an invalid type.\n", formatData.type);
                    badFormat = true;
                    break;
            }
            endPos += 2;
        }
        else
        {
            formatData.type = static_cast<char>(format[endPos]);
        }
        if (badFormat) break;

        // Output the field
        wstring wformat(&format[startPos], &format[endPos + 1]);
        switch (formatData.type)
        {
            // character
            case 'c':
            {
                INT32 arg = 0;
                if (OK != pJavaScript->FromJsval(pArguments[fieldNum - 1], &arg))
                {
                    badFormat = true;
                    break;
                }
                swprintf(&buff[0], buff.size(), wformat.c_str(), arg);
                copy(&buff[0], &buff[0] + wcslen(&buff[0]), outIt);
                break;
            }

            // signed integer
            case 'd':
            case 'i':
            {
                INT32 arg = 0;
                if (OK != pJavaScript->FromJsval(pArguments[fieldNum - 1], &arg))
                {
                    badFormat = true;
                    break;
                }
                swprintf(&buff[0], buff.size(), wformat.c_str(), arg);
                copy(&buff[0], &buff[0] + wcslen(&buff[0]), outIt);
                break;
            }

            // unsigned integer
            case 'o':
            case 'u':
            case 'x':
            case 'X':
            {
                if (formatData.isLong)
                {
                    UINT64 arg = 0;
                    if (OK != pJavaScript->FromJsval(pArguments[fieldNum - 1], &arg))
                    {
                        badFormat = true;
                        break;
                    }
#if defined(_WIN32) && (_MSC_VER < 1500)
                    wformat.replace(wformat.size() - 3, 2, L"I64");
#endif
                    swprintf(&buff[0], buff.size(), wformat.c_str(), arg);
                    copy(&buff[0], &buff[0] + wcslen(&buff[0]), outIt);
                }
                else
                {
                    UINT32 arg = 0U;
                    if (OK != pJavaScript->FromJsval(pArguments[fieldNum - 1], &arg))
                    {
                        badFormat = true;
                        break;
                    }
                    swprintf(&buff[0], buff.size(), wformat.c_str(), arg);
                    copy(&buff[0], &buff[0] + wcslen(&buff[0]), outIt);
                }
                break;
            }

            // double
            case 'e':
            case 'E':
            case 'f':
            case 'g':
            case 'G':
            {
                FLOAT64 arg = 0.0;
                if (OK != pJavaScript->FromJsval(pArguments[fieldNum - 1], &arg))
                {
                    badFormat = true;
                    break;
                }
                swprintf(&buff[0], buff.size(), wformat.c_str(), arg);
                copy(&buff[0], &buff[0] + wcslen(&buff[0]), outIt);
                break;
            }

            // C-string
            case 's':
            {
                jsval v = pArguments[fieldNum - 1];
                JSString *jsStr = JS_ValueToString(pContext, v);
                if (nullptr == jsStr)
                {
                    badFormat = true;
                    break;
                }

                jschar *chars = JSSTRING_CHARS(jsStr);
                size_t length = JSSTRING_LENGTH(jsStr);

                bool needPadding = formatData.width > length;
                if (needPadding)
                {
                    size_t paddingSize = formatData.width - length;
                    if (formatData.leftAlign)
                    {
                        copy(chars, chars + length, outIt);
                        while (paddingSize--) *outIt++ = ' ';
                    }
                    else
                    {
                        while (paddingSize--) *outIt++ = ' ';
                        copy(chars, chars + length, outIt);
                    }
                }
                else
                {
                    copy(chars, chars + length, outIt);
                }

                break;
            }

            default:
            {
                JS_ReportWarning(pContext, "%c is an invalid type.\n", formatData.type);
                badFormat = true;
                break;
            }
        }

        if (badFormat) break;

        ++endPos;

        startPos = endPos;
    }

    if (badFormat)
    {
        // Replace all '\n' with "\\n" so they get printed correctly.
        basic_string<jschar> tmpStr(format.size(), 0);
        const jschar sn[] = { '\\', 'n', 0 };
        copy(format.begin(), format.end(), tmpStr.begin());
        size_type pos = 0;
        while (basic_string<jschar>::npos != (pos = tmpStr.find('\n', pos)))
        {
            tmpStr.replace(pos, 1, sn);
        }
        JS_ReportError(pContext, "Bad format specification: \"%hs\".", tmpStr.c_str());
        return RC::BAD_FORMAT_SPECIFICAION;
    }

    return OK;
}

// SMethod
C_(Out_Printf)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;
    const char *usage1 = "Usage: Out.Printf(priority, format, ...)";
    const char *usage2 = "Usage: Out.Printf(priority, screenstate, format, ...)";

    // The function should have at least two arguments;
    INT32     priority = 0;
    string    format;
    UINT32    restOfArgsIdx = 2;
    UINT32    sps = Tee::SPS_END_LIST;

    if ((NumArguments < 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &priority)))
    {
        JS_ReportWarning(pContext, usage1);
        JS_ReportWarning(pContext, usage2);
        return JS_FALSE;
    }

    if (JSVAL_IS_DOUBLE(pArguments[1]) || JSVAL_IS_INT(pArguments[1]))
    {
        if ((NumArguments < 3) ||
            (OK != pJavaScript->FromJsval(pArguments[1], &sps)) ||
            (OK != pJavaScript->FromJsval(pArguments[2], &format)))
        {
            JS_ReportWarning(pContext, usage2);
            return JS_FALSE;
        }
        restOfArgsIdx = 3;
    }
    else if (JSVAL_IS_STRING(pArguments[1]))
    {
        if (OK != pJavaScript->FromJsval(pArguments[1], &format))
        {
            JS_ReportWarning(pContext, usage1);
            return JS_FALSE;
        }
    }
    else
    {
        JS_ReportWarning(pContext, usage1);
        JS_ReportWarning(pContext, usage2);
        return JS_FALSE;
    }

    if
    (
           (priority != Tee::PriNone)
        && (priority != Tee::PriDebug)
        && (priority != Tee::PriLow)
        && (priority != Tee::PriNormal)
        && (priority != Tee::PriHigh)
        && (priority != Tee::PriAlways)
        && (priority != Tee::ScreenOnly)
        && (priority != Tee::FileOnly)
        && (priority != Tee::SerialOnly)
        && (priority != Tee::CirlwlarOnly)
        && (priority != Tee::DebuggerOnly)
        && (priority != Tee::EthernetOnly)
        && (priority != Tee::PriError)
        && (priority != Tee::PriWarn)
    )
    {
        JS_ReportError(pContext, "First arg must be an integer output-priority such as Out.PriNormal");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    if ((restOfArgsIdx == 3) && (sps >= Tee::SPS_END_LIST))
    {
        JS_ReportError(pContext, "Second arg must be an integer output-priority such as SpsFail");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    vector<wchar_t> formattedOutput;
    formattedOutput.reserve(1024);
    if
    (
        OK != JsSprintf(pContext, NumArguments - restOfArgsIdx, pArguments + restOfArgsIdx,
                        format, back_inserter(formattedOutput))
    )
    {
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    string::size_type length;
    string::size_type startPos = 0;
    do
    {
        length = min(formattedOutput.size() - startPos, static_cast<size_t>(Tee::MaxPrintSize));
        wstring substr(&formattedOutput[startPos], length);
        Printf(priority, Tee::GetTeeModuleCoreCode(), sps, "%ls", substr.c_str());
        startPos += length;
    } while (startPos < formattedOutput.size());

    if (OK != pJavaScript->ToJsval(UINT32(formattedOutput.size()), pReturlwalue))
    {
       JS_ReportError(pContext, "Error oclwrred in Out.Printf()");
       *pReturlwalue = JSVAL_NULL;
       return JS_FALSE;
    }

    return JS_TRUE;
}

// SMethod
C_(Out_Sprintf)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;

   // The function should have at least one arguments;
   JSString *format = nullptr;
   if (NumArguments < 1 || !JSVAL_IS_STRING(pArguments[0]))
   {
       JS_ReportError(pContext, "Usage: Out.Sprintf(format, ...)");
       return JS_FALSE;
   }
   format = JS_ValueToString(pContext, pArguments[0]);

   basic_string<jschar> formatStr(JSSTRING_CHARS(format), JSSTRING_LENGTH(format));
   vector<jschar> formattedOutput;
   formattedOutput.reserve(1024);
   if
   (
       OK != JsSprintf(pContext, NumArguments - 1, pArguments + 1, formatStr,
                       back_inserter(formattedOutput))
   )
   {
       *pReturlwalue = JSVAL_NULL;
       return JS_FALSE;
   }

   JSString *formattedString =
       JS_NewUCStringCopyN(pContext, &formattedOutput[0], formattedOutput.size());
   if (nullptr == formattedString)
   {
       JS_ReportError(pContext, "Error oclwrred in Out.Sprintf()");
       *pReturlwalue = JSVAL_NULL;
       return JS_FALSE;
   }
   else
   {
       *pReturlwalue = STRING_TO_JSVAL(formattedString);
   }

   return JS_TRUE;
}

// SMethod
C_(Out_ToHex)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;

   // Colwert argument.
   UINT32 Number = 0;
   if
   (
         (NumArguments != 1)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Number))
   )
   {
      JS_ReportError(pContext, "Usage: Out.ToHex(unsigned integer)");
      return JS_FALSE;
   }

   char Str[33];
   sprintf(Str, "%x",Number);

   if (pJavaScript->ToJsval(Str, pReturlwalue) != RC::OK)
   {
      JS_ReportError(pContext, "Error oclwrred in Out.ToHex()");
      *pReturlwalue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

// STest
C_(Out_Open)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;
   const char *usage = "Usage: Out.Open(file name, mode, [encryption])";

   // Colwert the arguments.
   string FileName;
   INT32  OpenMode;
   bool   Encryption = CommandLine::GetEncryptFiles();
   if
   (
         ((NumArguments != 2) && (NumArguments != 3))
      || (OK != pJavaScript->FromJsval(pArguments[0], &FileName))
      || (OK != pJavaScript->FromJsval(pArguments[1], &OpenMode))
   )
   {
      JS_ReportError(pContext, usage);
      return JS_FALSE;
   }

   if ((NumArguments == 3) &&
       (OK != pJavaScript->FromJsval(pArguments[2], &Encryption)))
   {
      JS_ReportError(pContext, usage);
      return JS_FALSE;
   }

   FileSink * pFileSink = Tee::GetFileSink();

   RETURN_RC(pFileSink->Open(FileName, static_cast<FileSink::Mode>(OpenMode), Encryption));
}

// STest
C_(Out_Close)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   // Check this is a void method.
   if (NumArguments != 0)
   {
      JS_ReportError(pContext, "Usage: Out.Close()");
      return JS_FALSE;
   }

   Tee::FlushToDisk();

   FileSink * pFileSink = Tee::GetFileSink();

   RETURN_RC(pFileSink->Close());
}

// STest
C_(Out_Flush)
{
   // Check this is a void method.
   if (NumArguments != 0)
   {
      JS_ReportError(pContext, "Usage: Out.Flush()");
      return JS_FALSE;
   }

   Tee::FlushToDisk();

   RETURN_RC(OK);
}

// STest
C_(Out_EnableSerial)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   // Colwert argument.
   if (NumArguments != 0)
   {
      JS_ReportError(pContext, "Usage: Out.EnableSerial()");
      return JS_FALSE;
   }

   SerialSink * pSerialSink = Tee::GetSerialSink();

   RETURN_RC(pSerialSink->Initialize());
}

// STest
C_(Out_DisableSerial)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   if (NumArguments != 0)
   {
      JS_ReportError(pContext, "Usage: Out.DisableSerial()");
      return JS_FALSE;
   }

   SerialSink * pSerialSink = Tee::GetSerialSink();

   RETURN_RC(pSerialSink->Uninitialize());
}

// STest
C_(Out_EnableStdout)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   // Colwert argument.
   if (NumArguments != 0)
   {
      JS_ReportError(pContext, "Usage: Out.EnableStdout()");
      return JS_FALSE;
   }

   StdoutSink * pStdoutSink = Tee::GetStdoutSink();

   RETURN_RC(pStdoutSink->Enable());
}

// STest
C_(Out_DisableStdout)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   if (NumArguments != 0)
   {
      JS_ReportError(pContext, "Usage: Out.DisableStdout()");
      return JS_FALSE;
   }

   StdoutSink * pStdoutSink = Tee::GetStdoutSink();

   RETURN_RC(pStdoutSink->Disable());
}

static void EnDisMessages
(
    const string & ModuleNames,
    bool enable
)
{
    string::size_type start = 0;
    string::size_type end = ModuleNames.find_first_of(":,; |+.", start);
    while (start != end)
    {
        if (end == string::npos)
        {
            string ModuleName = ModuleNames.substr(start);
            Tee::EnDisModule(ModuleName.c_str(), enable);
            start = end;
        }
        else
        {
            string ModuleName = ModuleNames.substr(start, end - start);
            Tee::EnDisModule(ModuleName.c_str(), enable);
            start = ModuleNames.find_first_not_of(":,; |+.", end);
            end = ModuleNames.find_first_of(":,; |+.", start);
        }
    }
}

C_(Out_EnableMessages)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    string ModuleNames;
    // Colwert the argument.
    if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &ModuleNames))
    {
        JS_ReportError(pContext, "Failed to enable messages.");
        return JS_FALSE;
    }

    EnDisMessages(ModuleNames, true);

    return JS_TRUE;
}

C_(Out_DisableMessages)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    string ModuleNames;
    // Colwert the argument.
    if (OK != JavaScriptPtr()->FromJsval(pArguments[0], &ModuleNames))
    {
        JS_ReportError(pContext, "Failed to disable messages.");
        return JS_FALSE;
    }

    EnDisMessages(ModuleNames, false);

    return JS_TRUE;
}

C_(Out_SetEnableQueuedPrintf)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    bool bEnable;
    if ((NumArguments != 1) ||
        (OK != JavaScriptPtr()->FromJsval(pArguments[0], &bEnable)))
    {
       JS_ReportError(pContext, "Usage: Out.SetEnableQueuedPrintf(enable)");
       return JS_FALSE;
    }

    RETURN_RC(Tee::TeeEnableQueuedPrints(bEnable));
}

// STest
C_(Out_StartRecordingModsHeader)
{
    // Check this is a void method.
    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: Out.StartRecordingModsHeader()");
        return JS_FALSE;
    }

    Tee::GetFileSink()->StartRecordingModsHeader();
    Tee::GetMleSink()->StartRecordingModsHeader();

    RETURN_RC(RC::OK);
}

// STest
C_(Out_StopRecordingModsHeader)
{
    // Check this is a void method.
    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: Out.StopRecordingModsHeader()");
        return JS_FALSE;
    }

    Tee::GetFileSink()->StopRecordingModsHeader();
    Tee::GetMleSink()->StopRecordingModsHeader();

    RETURN_RC(RC::OK);
}

C_(Out_SwitchLogFilename)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    string filename;
    if ((NumArguments != 1) ||
        (RC::OK != JavaScriptPtr()->FromJsval(pArguments[0], &filename)))
    {
        JS_ReportError(pContext, "Usage: Out.SwitchLogFilename(filename)");
        return JS_FALSE;
    }

    RC rc;
    FileSink* logSink = Tee::GetFileSink();
    FileSink* mleSink = Tee::GetMleSink();
    constexpr FLOAT64 teeQueueFlushTimeoutMs = 1;

    // Want to have a report that represents all the tests since the start of
    // this log. That means reporting all errors for the current log and then
    // clearing out all the failing tests to start the next log fresh.
    //
    // NOTE: To avoid complications, ErrorMap is left untouched.

    // Print file footer
    //
    // Test summary and final RC
    JavaScriptPtr pJs;
    C_CHECK_RC(pJs->CallMethod(pJs->GetGlobalObject(), "PrintErrorWrapperPostTeardown"));
    // Clear all the failing tests so they aren't reported on the next log
    C_CHECK_RC(pJs->CallMethod(pJs->GetGlobalObject(), "ClearFailedTests"));

    // Ends logs with MODS END
    OneTimeInit::Timestamp(Mods::Stage::End);
    OneTimeInit::PrintStageTimestamp(Mods::Stage::End);
    CHECK_RC(Tee::TeeFlushQueuedPrints(teeQueueFlushTimeoutMs));

    // Switch to new log
    //
    // NOTE: This is not meant to be done when there are active prints
    // happening. The intended use case is to provide a way to switch files
    // through JS in a user script. Because of this, cases where many prints are
    // queued prior to the MODS start print or after the MODS end print are not
    // handled.
    C_CHECK_RC(logSink->SwitchFilename(filename + ".log"));
    C_CHECK_RC(mleSink->SwitchFilename(filename + ".mle"));

    // Print file header (MODS start and MODS header)
    OneTimeInit::PrintStageTimestamp(Mods::Stage::Start, true/*skipMleHeader*/);
    CHECK_RC(Tee::TeeFlushQueuedPrints(teeQueueFlushTimeoutMs));

    logSink->PrintModsHeader();
    mleSink->PrintModsHeader();

    RETURN_RC(rc);
}
