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

#include "core/include/sersink.h"
#include "core/include/tee.h"
#include "core/include/massert.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include <string.h>

RC SerialSink::Uninitialize()
{
   if (m_pCom) m_pCom->Uninitialize(SerialConst::CLIENT_SINK);
   m_pCom = 0;
   return OK;
}

/* virtual */ void SerialSink::DoPrint
(
   const char*           str,
   size_t                strLen,
   Tee::ScreenPrintState state
)
{
    MASSERT(str != nullptr);

    if (m_pCom == nullptr)
        return;

    // if the Serial.Filter property is set to something, make sure
    // that we filter out any ::Print() calls that don't contain the right string
    if (!m_Filter.empty())
    {
        if (nullptr == memmem(str, strLen, m_Filter.data(), m_Filter.size()))
            return;
    }

    // Send the new line of text to the serial port
    m_pCom->PutString(SerialConst::CLIENT_SINK, str, static_cast<UINT32>(strLen));
}

RC SerialSink::Initialize()
{
   RC rc = OK;
   m_pCom = GetSerialObj::GetCom(m_Port);

   m_pCom->SetAddCarriageReturn(true);

   if(OK != (rc = m_pCom->Initialize(SerialConst::CLIENT_SINK, true)))
   {
      m_pCom->Uninitialize(SerialConst::CLIENT_SINK);
      m_pCom = 0;
      return rc;
   }
   if(OK != (rc = m_pCom->SetBaud(SerialConst::CLIENT_SINK, 115200)))
   {
      m_pCom->Uninitialize(SerialConst::CLIENT_SINK);
      m_pCom = 0;
      return rc;
   }
   return OK;
}

bool SerialSink::IsInitialized()
{
    return (m_pCom != 0);
}

RC SerialSink::SetBaud(UINT32 Baud)
{
   RC rc = OK;
   if (m_pCom == 0)
       return OK;

   if(OK != (rc = m_pCom->SetBaud(SerialConst::CLIENT_SINK, Baud)))
   {
      m_pCom->Uninitialize(SerialConst::CLIENT_SINK);
      m_pCom = 0;
      return rc;
   }

   Printf(Tee::PriNormal,"Serial output set at %d baud, 8N1\n", Baud);
   return OK;
}

UINT32 SerialSink::GetBaud()
{
   if (m_pCom == 0)
       return 0;

   return m_pCom->GetBaud();
}

UINT32 SerialSink::GetPort()
{
    return m_Port;
}

RC SerialSink::SetPort(UINT32 Port)
{
    if ((Port < 1) || (Port > 4))
    {
        Printf(Tee::PriAlways,
               "You must specify a port between 1 and 4\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    m_Port = Port;

    // If serial sink is already initialized, uninit and re-init
    // to use the new serial port.

    if (IsInitialized())
    {   Uninitialize();
        Initialize();
    }

    return OK;
}

//------------------------------------------------------------------------------
// SerialSink object, properties and methods.
//------------------------------------------------------------------------------

JS_CLASS(SerialSink);
static SObject SerialSink_Object
(
   "SerialSink",
   SerialSinkClass,
   0,
   0,
   "SerialSink Buffer Class"
);

C_(SerialSink_Enable);
static STest SerialSink_Enable
(
   SerialSink_Object,
   "Enable",
   C_SerialSink_Enable,
   0,
   "Enable the cirlwlar buffer."
);

C_(SerialSink_Disable);
static STest SerialSink_Disable
(
   SerialSink_Object,
   "Disable",
   C_SerialSink_Disable,
   0,
   "Disable the cirlwlar buffer."
);

P_(SerialSink_Set_FilterString);
P_(SerialSink_Get_FilterString);
static SProperty SerialSink_FilterString
(
   SerialSink_Object,
   "Filter",
   0,
   0,
   SerialSink_Get_FilterString,
   SerialSink_Set_FilterString,
   0,
   "Output that includes this string will be put in the buffer"
);

// STest
C_(SerialSink_Enable)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   // Colwert argument.
   if (NumArguments != 0)
   {
      JS_ReportWarning(pContext, "Usage: SerialSink.Enable()");
      RETURN_RC(RC::SOFTWARE_ERROR);
   }

   RETURN_RC(Tee::GetSerialSink()->Initialize());
}

// STest
C_(SerialSink_Disable)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   if (NumArguments != 0)
   {
      JS_ReportWarning(pContext, "Usage: SerialSink.Disable()");
      RETURN_RC(RC::SOFTWARE_ERROR);
   }

   RETURN_RC(Tee::GetSerialSink()->Uninitialize());
}

P_(SerialSink_Set_FilterString)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   // Colwert argument.
   string Fstring;

   if (OK != JavaScriptPtr()->FromJsval(*pValue, &Fstring))
   {
      JS_ReportError(pContext, "Failed to set SerialSink.FilterString.");
      return JS_FALSE;
   }

   Tee::GetSerialSink()->SetFilterString(Fstring.c_str());

   return JS_TRUE;
}

P_(SerialSink_Get_FilterString)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   SerialSink * pSerialSink = Tee::GetSerialSink();

   if (JavaScriptPtr()->ToJsval(pSerialSink->GetFilterString(), pValue) != RC::OK)
   {
      JS_ReportError(pContext, "Failed to get SerialSink.FilterString.");
      *pValue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

