/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2009,2014 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/massert.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/tee.h"
#include "core/include/tasker.h"
#include "core/include/serial.h"
#include "core/include/hp3632.h"
#include "core/include/rc.h"
#include <stdio.h>

namespace Hp3632Private
{
   // private functions and data
   RC     WriteString(const char *data);
   RC     ReadString(string *data);
   bool   IsInitialized=false;
   Serial *pCom = 0;
}

JS_CLASS(Hp3632);

static SObject Hp3632_Object
(
   "Hp3632",
   Hp3632Class,
   0,
   0,
   "HP E3632A Power Supply"
);

C_(Hp3632_Identify);
STest Hp3632_Identify
(
   Hp3632_Object,
   "Identify",
   C_Hp3632_Identify,
   1,
   "Get the supply's identity string"
);

C_(Hp3632_SetVoltage);
STest Hp3632_SetVoltage
(
   Hp3632_Object,
   "SetVoltage",
   C_Hp3632_SetVoltage,
   1,
   "Set voltage"
);

C_(Hp3632_SetLwrrent);
STest Hp3632_SetLwrrent
(
   Hp3632_Object,
   "SetLwrrent",
   C_Hp3632_SetLwrrent,
   1,
   "Set current"
);

C_(Hp3632_GetVoltage);
STest Hp3632_GetVoltage
(
   Hp3632_Object,
   "GetVoltage",
   C_Hp3632_GetVoltage,
   1,
   "Get voltage"
);

C_(Hp3632_GetLwrrent);
STest Hp3632_GetLwrrent
(
   Hp3632_Object,
   "GetLwrrent",
   C_Hp3632_GetLwrrent,
   1,
   "Get current"
);

C_(Hp3632_EnableOutput);
STest Hp3632_EnableOutput
(
   Hp3632_Object,
   "EnableOutput",
   C_Hp3632_EnableOutput,
   1,
   "Enable output (true or false)"
);

C_(Hp3632_Range);
STest Hp3632_Range
(
   Hp3632_Object,
   "Range",
   C_Hp3632_Range,
   1,
   "Set output range (15 or 30)"
);

C_(Hp3632_Message);
STest Hp3632_Message
(
   Hp3632_Object,
   "Message",
   C_Hp3632_Message,
   0,
   "Print message"
);

C_(Hp3632_Initialize);
STest Hp3632_Initialize
(
   Hp3632_Object,
   "Initialize",
   C_Hp3632_Initialize,
   1,
   "Initialize Hp3632 Interface"
);

C_(Hp3632_Uninitialize);
STest Hp3632_Uninitialize
(
   Hp3632_Object,
   "Uninitialize",
   C_Hp3632_Uninitialize,
   0,
   "Uninitialize Hp3632 Interface"
);

C_(Hp3632_VoltageProt);
STest Hp3632_VoltageProt
(
   Hp3632_Object,
   "VoltageProt",
   C_Hp3632_VoltageProt,
   0,
   "Set voltage protection"
);

C_(Hp3632_LwrrentProt);
STest Hp3632_LwrrentProt
(
   Hp3632_Object,
   "LwrrentProt",
   C_Hp3632_LwrrentProt,
   0,
   "Set current protection"
);

//////////////////////////////
//Implimentation of methods
//////////////////////////////
C_(Hp3632_Identify)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   if(NumArguments > 1)
   {
      JS_ReportWarning(pContext,
                       "Usage: Hp3632.Identify() or Hp3632.Identify(array)");
      return JS_TRUE;
   }

   string IdnString;
   RC rc = OK;
   C_CHECK_RC( Hp3632::Identify(IdnString) );

   JavaScriptPtr pJavaScript;
   JSObject * pReturlwals;

   if (NumArguments == 1)
   {
      if(OK != pJavaScript->FromJsval(pArguments[0], &pReturlwals))
      {
         RETURN_RC(pJavaScript->SetElement(pReturlwals, 0, IdnString));
      }
      else
      {
         JS_ReportError(pContext, "Error in Hp3632.Identify()");
         return JS_FALSE;
      }
   }
   // NumArguments == 0 if we got here
   Printf(Tee::PriNormal, "%s", IdnString.c_str());

   RETURN_RC(OK);
}

C_(Hp3632_SetVoltage)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   if(NumArguments!=1)
   {
      JS_ReportWarning(pContext, "Usage: Hp3632.SetVoltage(Voltage)");
      return JS_TRUE;
   }

   JavaScriptPtr pJavaScript;
   FLOAT64 Voltage;
   *pReturlwalue = JSVAL_NULL;

   if(OK != pJavaScript->FromJsval(pArguments[0], &Voltage))
   {
      JS_ReportError(pContext, "Error in Hp3632.SetVoltage()");
      return JS_FALSE;
   }
   RETURN_RC(Hp3632::SetVoltage(Voltage));
}

C_(Hp3632_VoltageProt)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   if(NumArguments!=1)
   {
      JS_ReportWarning(pContext,
                       "Usage: Hp3632.VoltageProt(Voltage or 0 for disable)");
      return JS_TRUE;
   }

   JavaScriptPtr pJavaScript;
   FLOAT64 Voltage;
   *pReturlwalue = JSVAL_NULL;

   if ( OK != pJavaScript->FromJsval(pArguments[0], &Voltage))
   {
      JS_ReportError(pContext, "Error in Hp3632.VoltageProt()");
      return JS_FALSE;
   }

   RETURN_RC(Hp3632::SetVoltageProtection(Voltage) );
}

C_(Hp3632_LwrrentProt)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   if(NumArguments!=1)
   {
      JS_ReportWarning(pContext,
                       "Usage: Hp3632.LwrrentProt(Current or 0 for disable)");
      return JS_TRUE;
   }

   JavaScriptPtr pJavaScript;
   FLOAT64 Current;
   *pReturlwalue = JSVAL_NULL;

   if ( OK != pJavaScript->FromJsval(pArguments[0], &Current))
   {
      JS_ReportError(pContext, "Error in Hp3632.LwrrentProt()");
      return JS_FALSE;
   }

   RETURN_RC(Hp3632::SetLwrrentProtection(Current) );
}

C_(Hp3632_SetLwrrent)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   if(NumArguments!=1)
   {
      JS_ReportWarning(pContext, "Usage: Hp3632.SetLwrrent(Current)");
      return JS_TRUE;
   }

   JavaScriptPtr pJavaScript;
   FLOAT64 Current;
   *pReturlwalue = JSVAL_NULL;

   if ( OK != pJavaScript->FromJsval(pArguments[0], &Current))
   {
      JS_ReportError(pContext, "Error in Hp3632.SetLwrrent()");
      return JS_FALSE;
   }

   RETURN_RC(Hp3632::SetLwrrent(Current) );
}

C_(Hp3632_GetVoltage)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   if(NumArguments > 1)
   {
      JS_ReportWarning(pContext,
                       "Usage: Hp3632.GetVoltage(Array) or .GetVoltage()");
      return JS_TRUE;
   }

   JavaScriptPtr pJavaScript;
   JSObject * pReturlwals;
   *pReturlwalue = JSVAL_NULL;
   FLOAT64 Voltage;

   RC rc = OK;
   C_CHECK_RC(Hp3632::GetVoltage(&Voltage));

   if (NumArguments == 1)
   {
      if(OK != pJavaScript->FromJsval(pArguments[0], &pReturlwals))
      {
         JS_ReportError(pContext, "Error in Hp3632.GetVoltage()");
         return JS_FALSE;
      }
      else
      {
         RETURN_RC(pJavaScript->SetElement(pReturlwals, 0, Voltage));
      }
   }

   // NumArguments == 0 if we got here

   Printf(Tee::PriNormal, "Voltage is %f\n", Voltage);

   RETURN_RC(OK);
}

C_(Hp3632_GetLwrrent)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   if(NumArguments > 1)
   {
      JS_ReportWarning(pContext,
                       "Usage: Hp3632.GetLwrrent(Array) or .GetLwrrent()");
      return JS_TRUE;
   }

   JavaScriptPtr pJavaScript;
   JSObject * pReturlwals;
   *pReturlwalue = JSVAL_NULL;
   FLOAT64 Current;

   RC rc = OK;
   C_CHECK_RC(Hp3632::GetLwrrent(&Current));

   if (NumArguments == 1)
   {
      if(OK != pJavaScript->FromJsval(pArguments[0], &pReturlwals))
      {
         JS_ReportError(pContext, "Error in Hp3632.GetLwrrent()");
         return JS_FALSE;
      }
      else
      {
         RETURN_RC(pJavaScript->SetElement(pReturlwals, 0, Current));
      }
   }

   // NumArguments == 0 if we got here
   Printf(Tee::PriNormal, "%f\n", Current);

   RETURN_RC(OK);
}

C_(Hp3632_EnableOutput)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   if(NumArguments != 1)
   {
      JS_ReportWarning(pContext, "Usage: Hp3632.EnableOutput( 0 or 1 )");
      return JS_TRUE;
   }

   JavaScriptPtr pJavaScript;
   bool Enable;
   *pReturlwalue = JSVAL_NULL;

   if ( OK != pJavaScript->FromJsval(pArguments[0], &Enable) )
   {
      JS_ReportError(pContext, "Error in Hp3632.EnableOutput()");
      return JS_FALSE;
   }

   RETURN_RC(Hp3632::EnableOutput(Enable));
}

C_(Hp3632_Range)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   if(NumArguments != 1)
   {
      JS_ReportWarning(pContext, "Usage: Hp3632.Range(15 or 30)");
      return JS_TRUE;
   }

   JavaScriptPtr pJavaScript;
   UINT32 Range;
   *pReturlwalue = JSVAL_NULL;

   if ( OK != pJavaScript->FromJsval(pArguments[0], &Range))
   {
      JS_ReportError(pContext, "Error in Hp3632.Range()");
      return JS_FALSE;
   }

   switch(Range)
   {
   case 15:
      RETURN_RC(Hp3632::Range15());
   case 30:
      RETURN_RC(Hp3632::Range30());
   }

   // we should only get here if the range isn't 15 or 30
   JS_ReportError(pContext, "Parameter must be 15 or 30");
   return JS_FALSE;
}

C_(Hp3632_Message)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   if(NumArguments != 0)
   {
      JS_ReportWarning(pContext, "Usage: Hp3632.Message()");
      return JS_TRUE;
   }

   string MyMessage("VLADS DA MAN");
   RETURN_RC(Hp3632::DisplayText(MyMessage));
}

C_(Hp3632_Initialize)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   if(NumArguments != 0)
   {
      JS_ReportError(pContext,"Usage: Hp3632.Initialize()");
      return JS_FALSE;
   }
   RETURN_RC( Hp3632::Initialize() );
}

C_(Hp3632_Uninitialize)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);
   RETURN_RC( Hp3632::Uninitialize() );
}

///////////////////////////////////////////////////////////////////////////
// Low-level functions
///////////////////////////////////////////////////////////////////////////

RC Hp3632::Initialize()
{
   using namespace Hp3632Private;
   if(IsInitialized) Hp3632::Uninitialize();
   IsInitialized=true;
   string Value;

   UINT32 Port;
   for(Port = 1; Port <= Serial::GetHighestComPort(); Port++)
   {
      pCom = GetSerialObj::GetCom(Port);

      pCom->Initialize(SerialConst::CLIENT_HP3632, false);
      pCom->SetBaud(SerialConst::CLIENT_HP3632, 9600);
      pCom->SetStopBits(SerialConst::CLIENT_HP3632, 2); // Must use 2 stop bits

      // if we can successfully get the *IDN? string, then everything is OK
      if(OK != WriteString("SYST:REM\n")) break;
      if(OK != Identify(Value))           break;

      // make sure it returned something that seems reasonable
      if( Value.length() > 10 )
      {
         Printf(Tee::PriLow, "Found supply on COM%d\n", Port);
         return OK;
      }

      pCom->Uninitialize(SerialConst::CLIENT_HP3632);
   }

   IsInitialized=false;
   return RC::SERIAL_COMMUNICATION_ERROR;
}

RC Hp3632::Uninitialize()
{
   using namespace Hp3632Private;
   IsInitialized=false;

   if(IsInitialized)
   {
      // enable the front panel interface on the power supply
      WriteString("SYST:LOC\n");
      pCom->Uninitialize(SerialConst::CLIENT_HP3632);
   }
   return OK;
}

RC Hp3632::Identify(string &Returlwal)
{
   RC rc = OK;
   CHECK_RC(Hp3632Private::WriteString("*IDN?\n"));
   Tasker::Sleep(200);
   CHECK_RC(Hp3632Private::ReadString(&Returlwal));
   return OK;
}

RC Hp3632::SetLwrrentProtection(FLOAT64 Current)
{
   RC rc = OK;
   if(Current == 0.0)
      return Hp3632Private::WriteString("LWRR:PROT:STAT OFF\n");

   CHECK_RC(Hp3632Private::WriteString("LWRR:PROT:STAT ON\n"));
   Tasker::Sleep(200);
   char TempStr[25];
   sprintf(TempStr, "LWRR:PROT %2.1f\n", Current);
   return Hp3632Private::WriteString(TempStr);
}

RC Hp3632::SetVoltageProtection(FLOAT64 Voltage)
{
   RC rc = OK;
   if(Voltage == 0.0)
      return Hp3632Private::WriteString("VOLT:PROT:STAT OFF\n");

   CHECK_RC(Hp3632Private::WriteString("VOLT:PROT:STAT ON\n"));
   Tasker::Sleep(200);

   char TempStr[25];
   sprintf(TempStr, "VOLT:PROT %2.1f\n", Voltage);
   return Hp3632Private::WriteString(TempStr);
}

RC Hp3632::SetVoltage(FLOAT64 Voltage)
{
   char TempStr[25];
   sprintf(TempStr, "VOLT %2.3f\n", Voltage);
   return Hp3632Private::WriteString(TempStr);
}

RC Hp3632::SetLwrrent(FLOAT64 Current)
{
   char TempStr[25];
   sprintf(TempStr, "LWRR %1.3f\n", Current);
   return Hp3632Private::WriteString(TempStr);
}

RC Hp3632::EnableOutput(bool Enable)
{
   if(Enable) return Hp3632Private::WriteString("OUTP ON\n");
   else       return Hp3632Private::WriteString("OUTP OFF\n");
}

RC Hp3632::Range15()
{
   return Hp3632Private::WriteString("VOLT:RANG P15V\n");
}

RC Hp3632::Range30()
{
   return Hp3632Private::WriteString("VOLT:RANG P30V\n");
}

RC Hp3632::GetVoltage(FLOAT64 *Voltage)
{
   RC rc = OK;
   CHECK_RC(Hp3632Private::WriteString("SYST:REM\n"));
   Tasker::Sleep(200);
   CHECK_RC(Hp3632Private::WriteString("MEAS:VOLT?\n"));
   Tasker::Sleep(200);

   string TempStr;
   CHECK_RC(Hp3632Private::ReadString(&TempStr));
   *Voltage = atof(TempStr.c_str());
   return OK;
}

RC Hp3632::GetLwrrent(FLOAT64 *Current)
{
   RC rc = OK;
   CHECK_RC(Hp3632Private::WriteString("SYST:REM\n"));
   Tasker::Sleep(200);
   CHECK_RC(Hp3632Private::WriteString("MEAS:LWRR?\n"));
   Tasker::Sleep(200);

   string TempStr;
   CHECK_RC(Hp3632Private::ReadString(&TempStr));
   *Current = atof(TempStr.c_str());
   return OK;
}

RC Hp3632::DisplayText(string &Text)
{
   RC rc = OK;
   CHECK_RC(Hp3632Private::WriteString("SYST:REM\n"));
   Tasker::Sleep(200);
   CHECK_RC(Hp3632Private::WriteString("DISP:TEXT 'VLADS DA MAN'\n"));
   return OK;
}

RC Hp3632Private::ReadString(string *pString)
{
   if(!IsInitialized) return RC::WAS_NOT_INITIALIZED;

   pCom->GetString(SerialConst::CLIENT_HP3632, pString);
   Printf(Tee::PriDebug,"Read:  %s",pString->c_str());
   Tasker::Sleep(200);
   return OK;
}

RC Hp3632Private::WriteString(const char *pChar)
{
   if(!IsInitialized) return RC::WAS_NOT_INITIALIZED;

   pCom->ClearBuffers(SerialConst::CLIENT_HP3632);
   pCom->Put(SerialConst::CLIENT_HP3632, 3);  // control C; put the power supply in a known state
   Tasker::Sleep(50);

   string WrSt(pChar);

   pCom->PutString(SerialConst::CLIENT_HP3632, &WrSt);
   Printf(Tee::PriDebug,"Wrote: %s",WrSt.c_str());
   Tasker::Sleep(200);
   return OK;
}

