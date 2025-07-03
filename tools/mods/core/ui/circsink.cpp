/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2001-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/circsink.h"
#include "core/include/tee.h"
#include "core/include/massert.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include <string.h>

CirlwlarSink::~CirlwlarSink()
{
    Uninitialize();
}

RC CirlwlarSink::Uninitialize()
{
    m_IsEnabled = false;
    m_Filter.clear();
    m_CircBuffer.Uninitialize();
    return RC::OK;
}

RC CirlwlarSink::Erase()
{
    // no need to deallocate and reallocate!
    m_CircBuffer.Erase();
    return RC::OK;
}

/* virtual */ void CirlwlarSink::DoPrint
(
    const char*           str,
    size_t                strLen,
    Tee::ScreenPrintState state
)
{
    MASSERT(str != nullptr);

    if (!IsInitialized() || !m_CircBuffer.IsInitialized())
        return;

    // if the Cirlwlar.Filter property is set to something, make sure
    // that we filter out any ::Print() calls that don't contain the right string
    if (!m_Filter.empty())
    {
        if (0 == memmem(str, strLen, m_Filter.data(), m_Filter.size()))
            return;
    }

    m_CircBuffer.Print(str, strLen);
}

RC CirlwlarSink::Initialize()
{
    RC rc;

    CHECK_RC(m_CircBuffer.Initialize(m_CircBufferSize));

    m_IsEnabled = true;

    if(m_ExtraMessages)
    {
        Printf(Tee::PriNormal,"Cirlwlar output enabled\n");
    }
    return rc;
}

bool CirlwlarSink::IsInitialized()
{
    return m_IsEnabled;
}

RC CirlwlarSink::Dump(INT32 DumpPri)
{
    if (!m_IsEnabled)
    {
        Printf(Tee::PriNormal,"Cirlwlar buffer disabled\n");
        return RC::SOFTWARE_ERROR;
    }
    RC rc;

    CHECK_RC(Tee::TeeFlushQueuedPrints(Tee::DefPrintfQFlushMs));

    Tee::SinkPriorities sinkPri(static_cast<Tee::Priority>(DumpPri));

    m_IsEnabled = false;

    if (m_ExtraMessages)
    {
        static const char info[] =
            "------------------------- BEGIN CIRLWLAR DUMP -------------------------\n";
        Tee::Write(sinkPri, info, sizeof(info) - 1);
    }

    m_CircBuffer.Dump(DumpPri);

    if (m_ExtraMessages)
    {
        static const char info[] =
            "-------------------------- END CIRLWLAR DUMP --------------------------\n";
        Tee::Write(sinkPri, info, sizeof(info) - 1);
    }

    // re-enable cirlwlar buffer capture.  See comment above.
    m_IsEnabled = true;
    return rc;
}

UINT32 CirlwlarSink::GetCharsInBuffer()
{
    return m_CircBuffer.GetCharsInBuffer();
}

void CirlwlarSink::SetExtraMessages(bool value)
{
    m_ExtraMessages = value;
}

void CirlwlarSink::SetCircBufferSize(UINT32 size)
{
    // Assume the size coming in is in megabytes
    m_CircBufferSize = size * 1024 * 1024;
}

//------------------------------------------------------------------------------
// Cirlwlar object, properties and methods.
//------------------------------------------------------------------------------

JS_CLASS(Cirlwlar);
static SObject Cirlwlar_Object
(
   "Cirlwlar",
   CirlwlarClass,
   0,
   0,
   "Cirlwlar Buffer Class"
);

C_(Cirlwlar_Enable);
static STest Cirlwlar_Enable
(
   Cirlwlar_Object,
   "Enable",
   C_Cirlwlar_Enable,
   0,
   "Enable the cirlwlar buffer."
);

C_(Cirlwlar_Disable);
static STest Cirlwlar_Disable
(
   Cirlwlar_Object,
   "Disable",
   C_Cirlwlar_Disable,
   0,
   "Disable the cirlwlar buffer."
);

C_(Cirlwlar_Dump);
static STest Cirlwlar_Dump
(
   Cirlwlar_Object,
   "Dump",
   C_Cirlwlar_Dump,
   0,
   "Dump the cirlwlar buffer."
);

C_(Cirlwlar_Erase);
static STest Cirlwlar_Erase
(
   Cirlwlar_Object,
   "Erase",
   C_Cirlwlar_Erase,
   0,
   "Erase the cirlwlar buffer."
);

P_(Cirlwlar_Get_CharsInBuffer);
static SProperty Cirlwlar_CharsInBuffer
(
   Cirlwlar_Object,
   "CharsInBuffer",
   0,
   0,
   Cirlwlar_Get_CharsInBuffer,
   0,
   JSPROP_READONLY,
   "Number of characters in the cirlwlar buffer."
);

P_(Cirlwlar_Set_FilterString);
P_(Cirlwlar_Get_FilterString);
static SProperty Cirlwlar_FilterString
(
   Cirlwlar_Object,
   "Filter",
   0,
   0,
   Cirlwlar_Get_FilterString,
   Cirlwlar_Set_FilterString,
   0,
   "Output that includes this string will be put in the buffer"
);

// STest
C_(Cirlwlar_Enable)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   // Colwert argument.
   if (NumArguments != 0)
   {
      JS_ReportWarning(pContext, "Usage: Cirlwlar.Enable()");
      RETURN_RC(RC::SOFTWARE_ERROR);
   }

   RETURN_RC(Tee::GetCirlwlarSink()->Initialize());
}

// STest
C_(Cirlwlar_Erase)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   // Colwert argument.
   if (NumArguments != 0)
   {
      JS_ReportWarning(pContext, "Usage: Cirlwlar.Erase()");
      RETURN_RC(RC::SOFTWARE_ERROR);
   }

   RETURN_RC(Tee::GetCirlwlarSink()->Erase());
}

// STest
C_(Cirlwlar_Disable)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   if (NumArguments != 0)
   {
      JS_ReportWarning(pContext, "Usage: Cirlwlar.Disable()");
      RETURN_RC(RC::SOFTWARE_ERROR);
   }

   RETURN_RC(Tee::GetCirlwlarSink()->Uninitialize());
}

C_(Cirlwlar_Dump)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   if (NumArguments != 1)
   {
      JS_ReportWarning(pContext, "Usage: Cirlwlar.Dump(Priority)");
      RETURN_RC(RC::SOFTWARE_ERROR);
   }

   INT32 Priority;
   if(OK != JavaScriptPtr()->FromJsval(pArguments[0], &Priority))
   {
      JS_ReportWarning(pContext, "Usage: Cirlwlar.Dump(Priority)");
      RETURN_RC(RC::SOFTWARE_ERROR);
   }

   RETURN_RC(Tee::GetCirlwlarSink()->Dump(Priority));
}

P_(Cirlwlar_Get_CharsInBuffer)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   CirlwlarSink * pCirc = Tee::GetCirlwlarSink();

   if (OK != JavaScriptPtr()->ToJsval( pCirc->GetCharsInBuffer(), pValue))
   {
      JS_ReportError(pContext, "Failed to get Cirlwlar.CharsInBuffer");
      *pValue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

P_(Cirlwlar_Set_FilterString)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   // Colwert argument.
   string Fstring;

   if (OK != JavaScriptPtr()->FromJsval(*pValue, &Fstring))
   {
      JS_ReportError(pContext, "Failed to set Cirlwlar.FilterString.");
      return JS_FALSE;
   }

   Tee::GetCirlwlarSink()->SetFilterString(Fstring.c_str());

   return JS_TRUE;
}

P_(Cirlwlar_Get_FilterString)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   CirlwlarSink * pCirc = Tee::GetCirlwlarSink();

   if (JavaScriptPtr()->ToJsval(pCirc->GetFilterString(), pValue) != RC::OK)
   {
      JS_ReportError(pContext, "Failed to get Cirlwlar.FilterString.");
      *pValue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

