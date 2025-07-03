/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2000-2015 by LWPU Corporation.  All rights reserved.  All
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
#include "math.h"
#include "time.h"
#include "core/include/oven.h"
#include "core/include/rc.h"

namespace
{

   // constants
   const FLOAT64 SOAK_DIFFERENCE      =   0.5;
   const FLOAT64 MAX_SAFE_TEMP        = 150.0;
   const FLOAT64 MIN_SAFE_TEMP        = -30.0;
   const FLOAT64 REPLY_TIMEOUT        =   0.7; // In Seconds
   const UINT16  SERIAL_DELAY         =  50;   // milliseconds
   const UINT16  SOAK_RAMP_TIME       =  30;   // In Minutes
   const UINT16  SOAK_SLEEP_TIME      =   2;   // In Seconds
   const UINT16  SOAK_RAMP_SLEEP_TIME =   2;   // In Seconds
   const UINT32  MAX_OVENS            =   4;

   // Pass this constant to Oven->Initialize() to search all com ports
   const UINT32  ANY_PORT             = 0xd00dcafe;

   // Note: element zero of this array is unused.  I wanted to enumerate the
   // ovens starting at one instead of zero, and wasting an element seemed
   // cleaner than doing all kinds of +1 / -1 index arithmetic all over.
   class Oven *s_pOvens[MAX_OVENS+1] = {0};

   UINT32 s_ActiveOven = 1;
}

// In the early days of controlling ovens with mods, I had a javascript
// that allowed a user to access registers directly.  One of the summer interns
// promptly used it to program many of the non-volatile registers in the
// TestEquity ovens that would cause the compressors to fail.
//
// I want to leave a way in to read/write register for debugging, but general
// mods builds shouldn't do this... lest we spend another $10K fixing ovens
// #define ALLOW_DIRECT

//------------------------------------------------------------------------------
// Javascript interface
//------------------------------------------------------------------------------
JS_CLASS(Oven);

static SObject Oven_Object
(
   "Oven",
   OvenClass,
   0,
   0,
   "TestEquity Oven"
);

P_( Oven_Get_Temperature );
static SProperty Temperature
(
   Oven_Object,
   "Temperature",
   0,
   0,
   Oven_Get_Temperature,
   0,
   JSPROP_READONLY,
   "Present Oven Temperature"
);

P_( Oven_Set_SetPoint );
P_( Oven_Get_SetPoint );
static SProperty SetPoint
(
   Oven_Object,
   "SetPoint",
   0,
   0,
   Oven_Get_SetPoint,
   Oven_Set_SetPoint,
   0,
   "Oven Temperature SetPoint"
);

P_( Oven_Set_Fan );
P_( Oven_Get_Fan );
static SProperty Fan
(
   Oven_Object,
   "Fan",
   0,
   0,
   Oven_Get_Fan,
   Oven_Set_Fan,
   0,
   "Oven Fan Enable"
);

P_( Oven_Set_OvenNumber );
P_( Oven_Get_OvenNumber );
static SProperty OvenNumber
(
   Oven_Object,
   "OvenNumber",
   0,
   0,
   Oven_Get_OvenNumber,
   Oven_Set_OvenNumber,
   0,
   "Oven Number"
);

C_(Oven_Soak);
STest Oven_Soak
(
   Oven_Object,
   "Soak",
   C_Oven_Soak,
   2,
   "Temperature Soak for a given amount of time"
);

C_(Oven_Initialize);
STest Oven_Initialize
(
   Oven_Object,
   "Initialize",
   C_Oven_Initialize,
   1,
   "Initialize Oven Interface"
);

C_(Oven_Uninitialize);
STest Oven_Uninitialize
(
   Oven_Object,
   "Uninitialize",
   C_Oven_Uninitialize,
   0,
   "Uninitialize Oven Interface"
);

C_(Oven_Status);
STest Oven_Status
(
   Oven_Object,
   "Status",
   C_Oven_Status,
   0,
   "Print the status of the ovens"
);

#ifdef ALLOW_DIRECT

C_(Oven_ReadRegister);
STest Oven_ReadRegister
(
   Oven_Object,
   "ReadRegister",
   C_Oven_ReadRegister,
   0,
   "Read a register from the oven"
);

C_(Oven_WriteRegister);
STest Oven_WriteRegister
(
   Oven_Object,
   "WriteRegister",
   C_Oven_WriteRegister,
   0,
   "Write a register from the oven"
);

#endif // ALLOW_DIRECT

//------------------------------------------------------------------------------
// Javascript properties
//------------------------------------------------------------------------------

P_( Oven_Set_SetPoint )
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   // Colwert the argument.
   FLOAT64 Value;
   if (OK != JavaScriptPtr()->FromJsval(*pValue, &Value))
   {
      JS_ReportError(pContext, "Failed to colwert Value");
      return JS_FALSE;
   }

   if ( 0 == s_pOvens[s_ActiveOven] ) return JS_TRUE;

   if ( OK != s_pOvens[s_ActiveOven]->SetSetPoint(Value)) {
      JS_ReportError(pContext, "Error Setting SetPoint");
      *pValue = JSVAL_NULL;
      return JS_FALSE;

   }
   return JS_TRUE;
}

P_( Oven_Get_Temperature )
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   FLOAT64 Returlwal = -10000.0;
   if ( s_pOvens[s_ActiveOven] )
   {
      if( OK != s_pOvens[s_ActiveOven]->GetTemperature( &Returlwal ) )
      {
         JS_ReportWarning(pContext, "Error Getting Temperature");
         *pValue = JSVAL_NULL;
      }
   }

   if (OK != JavaScriptPtr()->ToJsval(Returlwal, pValue))
   {
      JS_ReportError(pContext, "Failed to colwert output temperature");
      *pValue = JSVAL_NULL;
      return JS_FALSE;
   }
   return JS_TRUE;
}

P_( Oven_Get_SetPoint )
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   FLOAT64 Returlwal = -10000.0;

   if ( s_pOvens[s_ActiveOven] )
   {
      if( OK != s_pOvens[s_ActiveOven]->GetSetPoint( &Returlwal ) )
      {
         JS_ReportWarning(pContext, "Error Getting SetPoint");
         *pValue = JSVAL_NULL;
      }
   }

   if (OK != JavaScriptPtr()->ToJsval(Returlwal, pValue))
   {
      JS_ReportError(pContext, "Failed to colwert output setpoint");
      *pValue = JSVAL_NULL;
      return JS_FALSE;
   }
   return JS_TRUE;
}

P_( Oven_Set_Fan )
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   // Colwert the argument.
   bool Value = 0;
   if (OK != JavaScriptPtr()->FromJsval(*pValue, &Value))
   {
      JS_ReportError(pContext, "Failed to colwert input value");
      return JS_FALSE;
   }

   if ( 0 == s_pOvens[s_ActiveOven] ) return JS_TRUE;

   if( OK != s_pOvens[s_ActiveOven]->SetFan(Value) )
   {
      JS_ReportWarning(pContext, "Error Setting Fan");
      *pValue = JSVAL_NULL;
      return JS_TRUE;
   }
   return JS_TRUE;
}

P_( Oven_Get_Fan )
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   bool Returlwal = false;

   if ( s_pOvens[s_ActiveOven] )
   {
      if( OK != s_pOvens[s_ActiveOven]->GetFan( &Returlwal ) )
      {
         JS_ReportWarning(pContext, "Error Getting Fan");
         *pValue = JSVAL_NULL;
      }
   }

   if (OK != JavaScriptPtr()->ToJsval(Returlwal, pValue))
   {
     JS_ReportError(pContext, "Failed to colwert output fan value");
     *pValue = JSVAL_NULL;
     return JS_FALSE;
   }
   return JS_TRUE;
}

P_( Oven_Set_OvenNumber )
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   // Colwert the argument.
   UINT32 Value;
   if (OK != JavaScriptPtr()->FromJsval(*pValue, &Value))
   {
      JS_ReportError(pContext, "Failed to colwert input value");
      return JS_FALSE;
   }

   if((Value == 0) || (Value > MAX_OVENS))
   {
      Printf(Tee::PriNormal, "Invalid oven number: must be from 1 to %d.\n",
             MAX_OVENS);
      *pValue = JSVAL_NULL;
      return JS_TRUE;
   }

   s_ActiveOven = Value;

   return JS_TRUE;
}

P_( Oven_Get_OvenNumber )
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   if (OK != JavaScriptPtr()->ToJsval(s_ActiveOven, pValue))
   {
     JS_ReportError(pContext, "Failed to colwert active oven number");
     *pValue = JSVAL_NULL;
     return JS_FALSE;
   }
   return JS_TRUE;
}

//------------------------------------------------------------------------------
// Javascript methods
//------------------------------------------------------------------------------

C_(Oven_Soak)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   if(NumArguments!=2)
   {
      JS_ReportWarning(pContext, "Usage: Oven.Soak(temperature, time in min)");
      return JS_TRUE;
   }

   if( 0 == s_pOvens[s_ActiveOven] )
   {
      JS_ReportError(pContext, "Oven not initialized");
      return JS_FALSE;
   }

   JavaScriptPtr pJavaScript;
   UINT32 Time;
   FLOAT64 Temperature;
   *pReturlwalue = JSVAL_NULL;

   if (  (OK != pJavaScript->FromJsval(pArguments[0], &Temperature)) ||
         (OK != pJavaScript->FromJsval(pArguments[1], &Time       )) )
   {
      JS_ReportError(pContext, "Error in Oven.Soak()");
      return JS_FALSE;
   }

   RETURN_RC(s_pOvens[s_ActiveOven]->Soak(Temperature,Time));
}

C_(Oven_Initialize)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   if(NumArguments > 1)
   {
      JS_ReportError(pContext,
                     "Usage: Oven.Initialize() or Oven.Initialize(Port)");
      return JS_FALSE;
   }

   RC rc = OK;

   if(s_pOvens[s_ActiveOven])
   {
      Printf(Tee::PriNormal, "Oven %d already initialized\n", s_ActiveOven);
   }

   UINT32 PortNum = ANY_PORT;
   JavaScriptPtr pJs;

   if(NumArguments)
   {
      if(OK != pJs->FromJsval(pArguments[0], &PortNum ))
      {
         JS_ReportError(pContext, "Error in Oven.Initialize()");
         return JS_FALSE;
      }
   }

   // See if we can find an oven.

   //--------------------------------------------
   // Look for TestEquity oven
   //--------------------------------------------
   s_pOvens[s_ActiveOven] = new TestEquity;
   rc = s_pOvens[s_ActiveOven]->Initialize(PortNum);
   if(OK == rc) RETURN_RC(OK);

   //--------------------------------------------
   // Look for Lwpu 5C7-based oven
   //--------------------------------------------
   delete s_pOvens[s_ActiveOven];

   s_pOvens[s_ActiveOven] = new Lw5C7;
   rc = s_pOvens[s_ActiveOven]->Initialize(PortNum);
   if(OK == rc) RETURN_RC(OK);

   //--------------------------------------------
   // Look for Giant Force 9700 oven
   //--------------------------------------------
   delete s_pOvens[s_ActiveOven];

   s_pOvens[s_ActiveOven] = new GF9700;
   rc = s_pOvens[s_ActiveOven]->Initialize(PortNum);
   if(OK == rc) RETURN_RC(OK);

   //--------------------------------------------
   // No ovens found.  Bail.
   //--------------------------------------------
   delete s_pOvens[s_ActiveOven];
   s_pOvens[s_ActiveOven] = 0;

   RETURN_RC(rc);
}

C_(Oven_Uninitialize)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   if(0 == s_pOvens[s_ActiveOven]) RETURN_RC(OK);

   RC rc = s_pOvens[s_ActiveOven]->Uninitialize();

   delete s_pOvens[s_ActiveOven];
   s_pOvens[s_ActiveOven] = 0;

   RETURN_RC(rc);
}

C_(Oven_Status)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   if(NumArguments)
   {
      JS_ReportError(pContext, "Usage: Oven.Status()");
      return JS_FALSE;
   }

   Printf(Tee::PriNormal, "OVEN  SETPOINT  PRESENT TEMP\n");
   Printf(Tee::PriNormal, "----  --------  ------------\n");

   UINT32 OriginalOven = s_ActiveOven;

   for(s_ActiveOven = 1 ; s_ActiveOven <= MAX_OVENS ; s_ActiveOven++)
   {
      Printf(Tee::PriNormal, "%3d   ", s_ActiveOven);

      if(s_pOvens[s_ActiveOven])
      {
         FLOAT64 SetpointValue = 0.0;
         FLOAT64 Temperature   = 0.0;

         if( ( OK == s_pOvens[s_ActiveOven]->GetSetPoint( &SetpointValue ) )    &&
             ( OK == s_pOvens[s_ActiveOven]->GetTemperature( &Temperature ) ) )
         {
            Printf(Tee::PriNormal, "%#6.1f    %#10.1f\n", SetpointValue, Temperature);
         }
         else
         {
            Printf(Tee::PriNormal, "  ----        ----\n");
         }
      }
      else
      {
         Printf(Tee::PriNormal, "  ----        ----\n");
      }
   }

   s_ActiveOven = OriginalOven;

   RETURN_RC(OK);
}

#ifdef ALLOW_DIRECT

C_(Oven_ReadRegister)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   if(NumArguments != 2)
   {
      JS_ReportWarning(pContext, "Usage: Oven.ReadRegister(register, Array for return)");
      return JS_TRUE;
   }

   if( 0 == s_pOvens[s_ActiveOven] )
   {
      JS_ReportError(pContext, "Oven not initialized");
      return JS_FALSE;
   }

   JSObject * pReturlwals;
   JavaScriptPtr pJavaScript;
   UINT32 Register;
   UINT32 Rval;

   if ( (OK != pJavaScript->FromJsval(pArguments[0], &Register)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &pReturlwals))  )
   {
      JS_ReportError(pContext, "Error in Oven.ReadRegister()");
      return JS_FALSE;
   }

   RC rc = OK;
   C_CHECK_RC( s_pOvens[s_ActiveOven]->ReadReg( Register, &Rval ));
   RETURN_RC(pJavaScript->SetElement(pReturlwals, 0, Rval));
}

C_(Oven_WriteRegister)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   if(NumArguments != 2)
   {
      JS_ReportWarning(pContext, "Usage: Oven.WriteRegister(register, value)");
      return JS_TRUE;
   }

   if( 0 == s_pOvens[s_ActiveOven] )
   {
      JS_ReportError(pContext, "Oven not initialized");
      return JS_FALSE;
   }

   UINT32 Register;
   UINT32 Value;
   JavaScriptPtr pJavaScript;

   if ( (OK != pJavaScript->FromJsval(pArguments[0], &Register)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Value))    )
   {
      JS_ReportError(pContext, "Error in Oven.WriteRegister()");
      return JS_FALSE;
   }

   RETURN_RC( s_pOvens[s_ActiveOven]->WriteReg( Register, Value ));
}

#endif

//------------------------------------------------------------------------------
// Generic Oven Interface
//------------------------------------------------------------------------------

Oven::Oven() :
   IsOvenInitialized(false),
   pCom(0)
{
}

RC Oven::Soak(FLOAT64 Temperature, UINT32 Minutes)
{
   RC rc = OK;
   FLOAT64 TargetTemperature;
   FLOAT64 TemperatureRightNow;
   FLOAT64 Difference = 10.0;

   clock_t t1=0,t2=0,t3=0;

   if(!IsOvenInitialized) return RC::DEV_OVEN_NOT_INITIALIZED;

   CHECK_RC( SetSetPoint(Temperature) );
   CHECK_RC( GetSetPoint( &TargetTemperature ) );

   Printf(Tee::PriNormal,"Waiting for temperature to reach %3.1f.\n"
          ,TargetTemperature);

   // Store Initial time
   t1 = clock();

   // Ramp to desired temperature, allowed timeout of SOAK_RAMP_TIME
   while (Difference > SOAK_DIFFERENCE)
   {
      CHECK_RC( GetTemperature( &TemperatureRightNow ) );
      Difference=fabs(TemperatureRightNow - TargetTemperature);

      Printf(Tee::ScreenOnly,"Present temp=%3.1f   Target temp=%3.1f   Difference=%3.1f.     \r",
             TemperatureRightNow,TargetTemperature,Difference);

      Tasker::Sleep(SOAK_RAMP_SLEEP_TIME * 1000);

      // Check if timeout has oclwred
      t2 = clock();
      if( t2 > ( t1 + CLOCKS_PER_SEC * SOAK_RAMP_TIME * 60) )
      {
         Printf(Tee::PriNormal,"\nCould not reach target temperature...aborting.\n");
         return RC::ERROR_SETTING_TEMPERATURE;
      }
   }

   Printf(Tee::PriNormal,
          "\nReached target temperature after %i minutes and %i seconds.\n",
          int((t2-t1)/CLOCKS_PER_SEC/60),int((t2-t1)/CLOCKS_PER_SEC%60));

   // set t1 to new clock time
   t1 = clock();  // Main Clock
   t2 = clock();  // t2 and t3 are differential clocks for status output
   t3 = clock();

   if(Minutes != 0)
   {
      Printf(Tee::PriNormal,"Soaking for %i Minutes.\n",Minutes);
      do
      {
         Tasker::Sleep(SOAK_SLEEP_TIME*1000);
         t3 = clock();

         Printf(Tee::ScreenOnly,"%i Minutes, %i Seconds remaining.       \r",
                int((Minutes*60 - (t3-t1)/CLOCKS_PER_SEC)/60),
                int((Minutes*60 - (t3-t1)/CLOCKS_PER_SEC)%60) );

         CHECK_RC(OK);
         t2 = clock(); // Reset Status Start Point
      }
      while( clock() < (t1 + CLOCKS_PER_SEC*(INT32)Minutes*60) );

      Printf(Tee::PriNormal,"\nSoak Complete.\n");
   }
   return OK;
}

//------------------------------------------------------------------------------
// TestEquity Oven Interface
//------------------------------------------------------------------------------

namespace TestEquityConst
{
    enum ModbusCodes
    {
        ModbusWriteCommand              = 0x06,
        ModbusReadCommand               = 0x03,
        ModbusSetPointRegister          = 300,
        ModbusPresentTempRegister       = 100,
        ModBusAlarmLowDeviationRegister = 302,
        ModBusAlarmHiDeviationRegister  = 303,
        ModBusAlarmSourceRegister       = 716,
        ModBusAlarmHysterisisRegister   = 703,
        ModBusAlarmLatchingRegister     = 721,  // self-clearing
        ModBusSilencingRegister         = 705,
        ModBusAlarmSidesRegister        = 706,
        ModBusAlarmLogicRegister        = 707,
        ModBusAlarmMessagesRegister     = 708,
        ModbusAlarmOutput1Register      = 702,
        ModbusAddress                   = 1
    };
}

RC TestEquity::WriteReg(UINT32 Register, UINT32 Value)
{
   RC rc = OK;
   pCom->ClearBuffers(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1);

   UINT08 Packet[MAX_PACKET_SIZE];
   UINT32 PacketLength=0;

   Packet[PacketLength++] = TestEquityConst::ModbusAddress;
   Packet[PacketLength++] = TestEquityConst::ModbusWriteCommand;
   Packet[PacketLength++] = (Register >> 8) & 0xFF;
   Packet[PacketLength++] =  Register & 0xFF;
   Packet[PacketLength++] = (Value >> 8) & 0xFF;
   Packet[PacketLength++] =  Value & 0xFF;

   AppendCrc(Packet, &PacketLength);

   UINT32 i;
   const UINT32 ExpectedReceiveCount = 8;

   for (i=0;i<PacketLength;i++) pCom->Put(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1, Packet[i]);

   PrintPacket(Packet, PacketLength);

   clock_t t1,t2;
   FLOAT64 time = 0;

   // Store Initial time
   t1 = clock();

   // Cycle, reading input buffer, allowed to wait up to 2 seconds
   while( (pCom->ReadBufCount() < ExpectedReceiveCount) && (time < REPLY_TIMEOUT) )
   {
      t2 = clock();
      time = ((FLOAT64)(t2-t1))/CLOCKS_PER_SEC;  // get time elapsed in seconds
   }

   // Give a little extra time for any extraneous serial characters to arrive
   Tasker::Sleep(25);

   PacketLength=0;

   // if oven didn't respond
   if (time > REPLY_TIMEOUT) return RC::DEV_OVEN_COMMUNICATION_ERROR;

   // Wait 50 milliseconds ( necessary timeout condition for controller to reset )
   Tasker::Sleep(SERIAL_DELAY);

   // Make sure that reading in the packet won't overflow our input buffer
   if( pCom->ReadBufCount() > MAX_PACKET_SIZE )
       return RC::DEV_OVEN_COMMUNICATION_ERROR;

   while (pCom->ReadBufCount())
   {
      UINT32 Value = 0;
      CHECK_RC( pCom->Get(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1, &Value) );
      Packet[PacketLength++]= (UINT08)Value;
   }

   if(PacketLength < ExpectedReceiveCount)
   {
      return RC::DEV_OVEN_COMMUNICATION_ERROR;
   }

   PrintPacket(Packet, PacketLength);

   UINT08 *PacketStart;

   for(i = 0; i <= PacketLength - ExpectedReceiveCount; i ++)
   {
      PacketStart = Packet + i;

      if(OK == CheckCrc(PacketStart, ExpectedReceiveCount))
      {
          return OK;
      }
   }

   return RC::DEV_OVEN_MODBUS_CRC_ERROR;
}

RC TestEquity::ReadReg(UINT32 Register, UINT32 *Value)
{
   RC rc = OK;
   pCom->ClearBuffers(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1);

   UINT08 Packet[MAX_PACKET_SIZE];

   UINT32 PacketLength=0;

   Packet[PacketLength++] = TestEquityConst::ModbusAddress;
   Packet[PacketLength++] = TestEquityConst::ModbusReadCommand;
   Packet[PacketLength++] = (Register >> 8) & 0xFF;
   Packet[PacketLength++] = Register & 0xFF;
   Packet[PacketLength++] = 0x00;  // number of registers to return high byte;
   Packet[PacketLength++] = 0x01;  // number of registers to return low byte;

   AppendCrc(Packet, &PacketLength);

   const UINT32 ExpectedReceiveCount = 7;

   pCom->PutString(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1, (char*)Packet, PacketLength);

   PrintPacket(Packet, PacketLength);
   Tasker::Sleep(100);

   clock_t t1,t2;
   FLOAT64 time = 0;

   t1 = clock(); // Store Initial time

   // Cycle, reading input buffer, allowed to wait up to 2 seconds
   while( (pCom->ReadBufCount() < ExpectedReceiveCount) && (time < REPLY_TIMEOUT) )
   {
      t2 = clock();
      time = ((FLOAT64)(t2-t1))/CLOCKS_PER_SEC;  // get time elapsed in seconds
   }

   // Give a little time for any extraneous serial characters to arrive
   Tasker::Sleep(25);

   PacketLength=0;
   *Value = 0; // default value if an error oclwrs

   // if oven didn't respond
   if (time > REPLY_TIMEOUT) return RC::DEV_OVEN_COMMUNICATION_ERROR;

   // Wait 50 milliseconds ( necessary timeout condition for controller to reset )
   Tasker::Sleep(SERIAL_DELAY);

   // Make sure that reading in the packet won't overflow our input buffer
   if( pCom->ReadBufCount() > MAX_PACKET_SIZE )
       return RC::DEV_OVEN_COMMUNICATION_ERROR;

   while (pCom->ReadBufCount())
   {
      UINT32 Value = 0;
      CHECK_RC( pCom->Get(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1, &Value) );
      Packet[PacketLength++]= Value;
   }

   if(PacketLength < ExpectedReceiveCount)
   {
      return RC::DEV_OVEN_COMMUNICATION_ERROR;
   }

   PrintPacket(Packet, PacketLength);

   UINT08 *PacketStart;

   for(UINT32 i = 0 ; i <= PacketLength - ExpectedReceiveCount; i ++)
   {
       PacketStart = Packet + i;

       if(OK == CheckCrc(PacketStart, ExpectedReceiveCount))
       {
          *Value = (PacketStart[3] << 8) | PacketStart[4];
          return OK;
       }
   }

   return RC::DEV_OVEN_MODBUS_CRC_ERROR;
}

RC TestEquity::CheckCrc(unsigned char *Packet, UINT32 Length)
{
   Printf(Tee::PriDebug,"Checking a packet CRC at %p of length %d\n",
          Packet, Length);

   // packet must be at least 3 bytes
   if(Length<3) return RC::DEV_OVEN_COMMUNICATION_ERROR;

   UINT08 ReceivedCrcMsb=Packet[Length - 1];
   UINT08 ReceivedCrcLsb=Packet[Length - 2];

   UINT32 ReceivedCrc=(ReceivedCrcMsb << 8) | ReceivedCrcLsb;

   UINT32 ExpectedCrc=CallwlateCrc(Packet, Length - 2);

   if (ExpectedCrc!=ReceivedCrc)
   {
      Printf(Tee::PriLow, "Received bad CRC: Expected %4x Received %4x\n",
            ExpectedCrc, ReceivedCrc );
     return RC::DEV_OVEN_MODBUS_CRC_ERROR;
   }
   if(Packet[1] & 0x80) // an exception
   {
      Printf(Tee::PriLow, "Received exception from oven.\n");
      return RC::DEV_OVEN_MODBUS_CRC_ERROR;
   }
   return OK;
}

void TestEquity::AppendCrc(unsigned char *Packet, UINT32 *PacketLength)
{
   MASSERT(*PacketLength < (MAX_PACKET_SIZE -2));

   UINT32 crc=CallwlateCrc(Packet, *PacketLength);
   Packet[ (*PacketLength)   ] = crc & 0xFF;
   Packet[ (*PacketLength)+1 ] = (crc >> 8) & 0xFF;

   *PacketLength +=2;
}

UINT32 TestEquity::CallwlateCrc(unsigned char *PacketStart, UINT32 Length)
{
   UINT32 crc=0xffff;
   UINT32 CharCount;
   UINT32 BitCount;

   for ( CharCount=0 ; CharCount<Length ; CharCount++)
   {
     crc ^= UINT32(PacketStart[CharCount]);
     for (BitCount=0;BitCount<8;BitCount++)
     {
       if (crc & 0x0001)
       {
         crc >>=1;
         crc ^=0xA001;
       } else crc >>=1;
     }
   }
   return crc;
}

RC TestEquity::SetSetPoint(FLOAT64 SetPoint)
{
   if(!IsOvenInitialized) return RC::DEV_OVEN_NOT_INITIALIZED;

   long IntSetPoint=(long)(10*SetPoint);
   if ( (IntSetPoint > 10*MAX_SAFE_TEMP ) || ( IntSetPoint < 10*MIN_SAFE_TEMP ) )
   {
      Printf(Tee::PriNormal, "%ld.%ld C is outside the safe range.\n",
             IntSetPoint / 10 , IntSetPoint % 10);
      Printf(Tee::PriNormal, "Must be between %ldC and %ldC\n" ,
             (long)MIN_SAFE_TEMP, (long)MAX_SAFE_TEMP );
     return RC::DEV_OVEN_ILWALID_TEMPERATURE;
   }
   return WriteReg(TestEquityConst::ModbusSetPointRegister,IntSetPoint);
}

RC TestEquity::GetTemperature(FLOAT64 *Temperature)
{
   RC rc = OK;

   if(!IsOvenInitialized) return RC::DEV_OVEN_NOT_INITIALIZED;

   UINT32 Value;
   CHECK_RC( ReadReg(TestEquityConst::ModbusPresentTempRegister, &Value) );

   // consider two's compliment
   if (Value <= 0x7FFF) *Temperature = ((FLOAT64)Value)/10;
   else *Temperature = -((FLOAT64)((0x10000 - Value)))/10;
   return OK;
}

RC TestEquity::GetSetPoint(FLOAT64 *Returlwalue)
{
   RC rc = OK;

   if(!IsOvenInitialized) return RC::DEV_OVEN_NOT_INITIALIZED;

   UINT32 ModbusVal;
   CHECK_RC( ReadReg(TestEquityConst::ModbusSetPointRegister, &ModbusVal) );

   // consider two's compliment
   if(ModbusVal <= 0x7FFF) *Returlwalue = ((FLOAT64)ModbusVal)/10;
   else                    *Returlwalue = -((FLOAT64)((0x10000 - ModbusVal)))/10;
   return OK;
}

RC TestEquity::Initialize(UINT32 Port)
{
   if(IsOvenInitialized) TestEquity::Uninitialize();
   IsOvenInitialized=true;
   UINT32 DummyValue;

   UINT32 StartPort = (ANY_PORT != Port) ? Port : 1;
   UINT32 EndPort   = (ANY_PORT != Port) ? Port : Serial::GetHighestComPort();

   UINT32 PortIdx;
   for(PortIdx = StartPort; PortIdx <= EndPort; PortIdx++)
   {
      pCom = GetSerialObj::GetCom(PortIdx);

      pCom->Initialize(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1, false);
      pCom->SetBaud(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1, 19200);

      // if we can successfully read a register, then everything is OK
      if( OK == ReadReg(TestEquityConst::ModbusPresentTempRegister, &DummyValue) )
      {
         FLOAT64 Temperature;
         UINT32 Value = DummyValue;

         if (Value <= 0x7FFF)
            Temperature = ((FLOAT64)Value)/10;
         else
            Temperature = -((FLOAT64)((0x10000 - Value)))/10;

         Printf(Tee::PriNormal,
                "Found TestEquity Oven %d on COM%d (Temp=%f)\n",
                s_ActiveOven,
                PortIdx,
                Temperature
                );
         return OK;
      }

      pCom->Uninitialize(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1);
   }

   // both com ports failed, so the game is over
   IsOvenInitialized=false;
   return RC::DEV_OVEN_COMMUNICATION_ERROR;
}

RC TestEquity::Uninitialize()
{
   if(IsOvenInitialized) pCom->Uninitialize(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1);
   IsOvenInitialized=false;
   return OK;
}

void TestEquity::PrintPacket(UINT08 *Packet, UINT32 PacketLength)
{
   UINT32 i;
   Printf(Tee::PriDebug, "Modbus Packet: ");
   for (i=0;i<PacketLength;i++)
      Printf(Tee::PriLow, "%3x", Packet[i]);

   Printf(Tee::PriDebug, "\n");
}

RC TestEquity::SetFan(bool FanControl)
{
   //This entire function refers to ALARM1 which will be physically
   //connected via a surge suppressor to terminals 5 and 6
   //of the oven.  These terminals control the ON/OFF state of the
   //chamber, and hence the oven.
   //Oven Registers should be HW set in this config
   //REGISTER  VALUE  COMMENT
   //302  -70  Alarm  Low Deviation MODE
   //303   70  Alarm  Hi Deviation MODE
   //716    0  Alarm  Source
   //703  50   Alarm  Hysterisis (5.0C)
   //721   0   Alarm  Latching(Self Clearing)
   //705   0          Silencing(NO)
   //706   0   Alarm  Sides(BOTH)
   //707   1   Alarm  Logic(CLOSE ON ALARM)
   //708   0   Alarm  Messages(YES)
   // for FAN

   if(!IsOvenInitialized) return RC::DEV_OVEN_NOT_INITIALIZED;
   if(FanControl)
   {
      //Normal Operation
      return WriteReg(TestEquityConst::ModbusAlarmOutput1Register,0);
      //Turn ALARM OFF workaround by setting to crazy value
      //WriteReg(302,-500);  // Set Alarm Lose Set Point -274C
      //WriteReg(303,-499);  // Set Alarm Hi Set Point -275C
   }
   else
   {
      return WriteReg(TestEquityConst::ModbusAlarmOutput1Register,2);
      //Turn Alarm ON & to Deviation Mode
      //WriteReg(302,-70); // Set Alarm Low Deviation to -7C
      //WriteReg(303,70); // Set Alarm Hi Deviation to 7C
   }
}

RC TestEquity::GetFan(bool *Value)
{
   RC rc = OK;
   if(!IsOvenInitialized) return RC::DEV_OVEN_NOT_INITIALIZED;
   UINT32 Falwalue;
   CHECK_RC( ReadReg(TestEquityConst::ModbusAlarmOutput1Register, &Falwalue) );

   if(Falwalue & 0x02) *Value = false;
   *Value = true;
   return OK;
}

//------------------------------------------------------------------------------
// Lw5C7 Oven Interface
//------------------------------------------------------------------------------

namespace Lw5C7Const
{
   static const  UINT32 WRITE_INDEX         = 0;
   static const  UINT32 READ_INDEX          = 1;

   static const  UINT32 PACKET_SEND         = 0;
   static const  UINT32 PACKET_RECEIVE      = 1;

   static const  UINT32 PACKET_SEND_SIZE    = 16;
   static const  UINT32 PACKET_RECEIVE_SIZE = 12;

   static const  UINT32 START_CHARACTER     = 0x2A;
   static const  UINT32 END_CHARACTER       = 0x0D;
   static const  UINT32 ACK_CHARACTER       = 0x5E;

   // read-only registers                                      WRITE  READ
   static const  UINT08 INPUT1[2]                           = { 0xFF, 0x01 };
   static const  UINT08 DESIRED_CONTROL_VALUE[2]            = { 0xFF, 0x03 };
   /*
   static const  UINT08 POWER_OUTPUT[2]                     = { 0xFF, 0x04 };
   static const  UINT08 ALARM_STATUS[2]                     = { 0xFF, 0x05 };
   static const  UINT08 INPUT2[2]                           = { 0xFF, 0x06 };
   */

   // read-write registers
   /*
   static const  UINT08 ALARM_TYPE[2]                       = { 0x28, 0x41 };
   static const  UINT08 INPUT2_DEFINE[2]                    = { 0x29, 0x42 };
   static const  UINT08 SENSOR_TYPE[2]                      = { 0x2A, 0x43 };
   static const  UINT08 CONTROL_TYPE[2]                     = { 0x2B, 0x44 };
   static const  UINT08 OUTPUT_POLARITY[2]                  = { 0x2C, 0x45 };
   static const  UINT08 POWER_ON_OFF[2]                     = { 0x2D, 0x46 };
   static const  UINT08 OUTPUT_SHUTDOWN_IF_ALARM[2]         = { 0x2E, 0x47 };
   */
   static const  UINT08 FIXED_DESIRED_CONTROL_SETTING[2]    = { 0x1C, 0x50 };
   /*
   static const  UINT08 PROPORTIONAL_BANDWIDTH[2]           = { 0x1D, 0x51 };
   static const  UINT08 INTEGRAL_GAIN[2]                    = { 0x1E, 0x52 };
   static const  UINT08 DERIVATIVE_GAIN[2]                  = { 0x1F, 0x53 };
   static const  UINT08 LOW_EXTERNAL_SET_RANGE[2]           = { 0x20, 0x54 };
   static const  UINT08 HIGH_EXTERNAL_SET_RANGE[2]          = { 0x21, 0x55 };
   static const  UINT08 ALARM_DEADBAND[2]                   = { 0x22, 0x56 };
   static const  UINT08 HIGH_ALARM_DEADBAND[2]              = { 0x23, 0x57 };
   static const  UINT08 LOW_ALARM_SETTING[2]                = { 0x24, 0x58 };
   static const  UINT08 CONTROL_DEADBAND_SETTING[2]         = { 0x25, 0x59 };
   static const  UINT08 INPUT1_OFFSET[2]                    = { 0x26, 0x5A };
   static const  UINT08 INPUT2_OFFSET[2]                    = { 0x27, 0x5B };
   static const  UINT08 ALARM_LATCH_ENABLE[2]               = { 0x2F, 0x48 };
   static const  UINT08 CONTROL_TIMEBASE[2]                 = { 0x30, 0x49 };

   static const  UINT08 ALARM_LATCH_RESET[2]                = { 0x33, 0x00 };
   static const  UINT08 HEAT_MULTIPLIER[2]                  = { 0x0C, 0x5C };
   static const  UINT08 CHOOSE_SENSOR_FOR_ALARM_FUNCTION[2] = { 0x31, 0x4A };
   static const  UINT08 CHOOSE_C_OR_F_TEMP_WORKING_UNITS[2] = { 0x32, 0x4B };

   static const  UINT08 PASSWORD_VALUE[4][2]              = { { 0x60, 0x80 },
                                                              { 0x62, 0x82 },
                                                              { 0x64, 0x84 },
                                                              { 0x66, 0x86 } };

   static const  UINT08 TAG_NAME[4][2]                    = { { 0x68, 0x88 },
                                                              { 0x6A, 0x8A },
                                                              { 0x6C, 0x8C },
                                                              { 0x6E, 0x8E } };

   static const  UINT08 MAXIMUM_CONTROL_TEMP_INPUT[2]       = { 0x70, 0x90 };
   static const  UINT08 MINIMUM_CONTROL_TEMP_INPUT[2]       = { 0x72, 0x92 };
   static const  UINT08 MAXIMUM_ALARM_SET_VALUE[2]          = { 0x74, 0x94 };
   static const  UINT08 MINIMUM_ALARM_SET_VALUE[2]          = { 0x76, 0x96 };
   static const  UINT08 PASSWORD_ENABLE[2]                  = { 0x78, 0x98 };
   */
}

RC Lw5C7::WriteReg(UINT32 Register, UINT32 Value)
{
   UINT08 RegArray[2];

   RegArray[ Lw5C7Const::WRITE_INDEX ] = Register;
   RegArray[  Lw5C7Const::READ_INDEX ] = Register;

   return WriteReg(RegArray, Value);
}

RC Lw5C7::ReadReg(UINT32 Register, UINT32 *Value)
{
   UINT08 RegArray[2];

   RegArray[ Lw5C7Const::WRITE_INDEX ] = Register;
   RegArray[  Lw5C7Const::READ_INDEX ] = Register;

   return ReadReg(RegArray, Value);
}

RC Lw5C7::WriteReg(const UINT08 *Register, UINT32 Value)
{
   using namespace Lw5C7Const;

   RC  rc = OK;
   UINT32 i;
   pCom->ClearBuffers(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1);

   for( i=0 ; i < PACKET_RECEIVE_SIZE ; i++ ) ReceivePacket[i] = 0;
   for( i=0 ; i < PACKET_SEND_SIZE    ; i++ ) SendPacket[i]    = 0;

   SendPacket[  0 ] = START_CHARACTER;
   SendPacket[  1 ] = UINT32('0');
   SendPacket[  2 ] = UINT32('0');
   SendPacket[  3 ] = ColwertIntToChar( (Register[WRITE_INDEX] >> 4) & 0xF );
   SendPacket[  4 ] = ColwertIntToChar( (Register[WRITE_INDEX] ) & 0xF );
   SendPacket[  5 ] = ColwertIntToChar( ( Value >> 28 ) & 0xF );
   SendPacket[  6 ] = ColwertIntToChar( ( Value >> 24 ) & 0xF );
   SendPacket[  7 ] = ColwertIntToChar( ( Value >> 20 ) & 0xF );
   SendPacket[  8 ] = ColwertIntToChar( ( Value >> 16 ) & 0xF );
   SendPacket[  9 ] = ColwertIntToChar( ( Value >> 12 ) & 0xF );
   SendPacket[ 10 ] = ColwertIntToChar( ( Value >>  8 ) & 0xF );
   SendPacket[ 11 ] = ColwertIntToChar( ( Value >>  4 ) & 0xF );
   SendPacket[ 12 ] = ColwertIntToChar( ( Value >>  0 ) & 0xF );

   AppendChecksum(); // locations 13 and 14

   SendPacket[ 15 ] = END_CHARACTER;

   for ( i=0 ; i< PACKET_SEND_SIZE ; i++ )
      pCom->Put(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1, SendPacket[i]);

   PrintPacket(PACKET_SEND);

   clock_t t1,t2;
   FLOAT64 time = 0;

   // Store Initial time
   t1 = clock();

   // Cycle, reading input buffer, allowed to wait up to 2 seconds
   while( (pCom->ReadBufCount() < PACKET_RECEIVE_SIZE) && (time < REPLY_TIMEOUT) )
   {
      t2 = clock();
      time = ((FLOAT64)(t2-t1))/CLOCKS_PER_SEC;  // get time elapsed in seconds
   }

   // Give a little extra time for any extraneous serial characters to arrive
   Tasker::Sleep(25);

   // if oven didn't respond
   if (time > REPLY_TIMEOUT) return RC::DEV_OVEN_COMMUNICATION_ERROR;

   // Wait 50 milliseconds ( necessary timeout condition for controller to reset )
   Tasker::Sleep(SERIAL_DELAY);

   UINT32 PacketIdx = 0;

   // Make sure that reading in the packet won't overflow our input buffer
   if( pCom->ReadBufCount() > MAX_PACKET_SIZE )
       return RC::DEV_OVEN_COMMUNICATION_ERROR;

   while (pCom->ReadBufCount())
   {
      UINT32 Value = 0;
      CHECK_RC( pCom->Get(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1, &Value) );
      ReceivePacket[PacketIdx++]= Value;
   }

   PrintPacket(PACKET_RECEIVE);

   CHECK_RC( CheckChecksum(true) );

   return OK;
}

RC Lw5C7::ReadReg(const UINT08 *Register, UINT32 *Value)
{
   using namespace Lw5C7Const;

   RC rc = OK;
   pCom->ClearBuffers(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1);

   UINT32 i;
   for( i=0 ; i < PACKET_RECEIVE_SIZE ; i++ ) ReceivePacket[i] = 0;
   for( i=0 ; i < PACKET_SEND_SIZE    ; i++ ) SendPacket[i]    = 0;

   SendPacket[  0 ] = START_CHARACTER;
   SendPacket[  1 ] = UINT32('0');
   SendPacket[  2 ] = UINT32('0');
   SendPacket[  3 ] = ColwertIntToChar( (Register[READ_INDEX] >> 4) & 0xF );
   SendPacket[  4 ] = ColwertIntToChar( (Register[READ_INDEX] ) & 0xF );
   SendPacket[  5 ] = UINT32('0');
   SendPacket[  6 ] = UINT32('0');
   SendPacket[  7 ] = UINT32('0');
   SendPacket[  8 ] = UINT32('0');
   SendPacket[  9 ] = UINT32('0');
   SendPacket[ 10 ] = UINT32('0');
   SendPacket[ 11 ] = UINT32('0');
   SendPacket[ 12 ] = UINT32('0');

   AppendChecksum(); // locations 13 and 14

   SendPacket[ 15 ] = END_CHARACTER;

   for (i=0; i<PACKET_SEND_SIZE ;i++) pCom->Put(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1, SendPacket[i]);

   PrintPacket(PACKET_SEND);

   clock_t t1,t2;
   FLOAT64 time = 0;

   // Store Initial time
   t1 = clock();

   // Cycle, reading input buffer, allowed to wait up to 2 seconds
   while( (pCom->ReadBufCount() < PACKET_RECEIVE_SIZE) && (time < REPLY_TIMEOUT) )
   {
      t2 = clock();
      time = ((FLOAT64)(t2-t1))/CLOCKS_PER_SEC;  // get time elapsed in seconds
   }

   *Value = 0; // default value if an error oclwrs

   // Give a little extra time for any extraneous serial characters to arrive
   Tasker::Sleep(25);

   // if oven didn't respond
   if (time > REPLY_TIMEOUT) return RC::DEV_OVEN_COMMUNICATION_ERROR;

   // Wait 50 milliseconds ( necessary timeout condition for controller to reset )
   Tasker::Sleep(SERIAL_DELAY);

   // Make sure that reading in the packet won't overflow our input buffer
   if( pCom->ReadBufCount() > MAX_PACKET_SIZE )
       return RC::DEV_OVEN_COMMUNICATION_ERROR;

   UINT32 PacketIdx = 0;
   while (pCom->ReadBufCount())
   {
      UINT32 Value = 0;
      CHECK_RC( pCom->Get(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1, &Value) );
      ReceivePacket[PacketIdx++]= Value;
   }

   PrintPacket(PACKET_RECEIVE);

   CHECK_RC( CheckChecksum(false) );

   *Value = 0;

   for( i=1 ; i < 9 ; i++ )
   {
      UINT32 Nybble = ColwertCharToInt( ReceivePacket[i] );
      *Value |= ( Nybble << (4*(8-i)) );
   }

   return OK;
}

RC Lw5C7::CheckChecksum(bool ComparePackets)
{
   using namespace Lw5C7Const;

   UINT32 i;
   if(ComparePackets)
   {
      for(i=0 ; i<8; i++)
      {
         if( SendPacket[i+5] != ReceivePacket[i+1] )
         {
            Printf(Tee::PriLow,"Send packet didn't match receive packet\n");
            return RC::DEV_OVEN_COMMUNICATION_ERROR;
         }
      }
   }

   UINT32 ReceivedChecksum = ( ColwertCharToInt( ReceivePacket[ 9 ]  ) << 4 |
                               ColwertCharToInt( ReceivePacket[ 10 ] )      ) & 0xFF;

   UINT32 ExpectedChecksum=CallwlateChecksum(PACKET_RECEIVE);

   if (ExpectedChecksum!=ReceivedChecksum)
   {
      Printf(Tee::PriLow, "Received Invalid checksum (Expected %4x Received %4x)\n",
            ExpectedChecksum, ReceivedChecksum );
     return RC::DEV_OVEN_MODBUS_CRC_ERROR;
   }
   if(ReceivePacket[11] != ACK_CHARACTER ) // an exception
   {
      Printf(Tee::PriLow, "Received exception from oven.\n");
      return RC::DEV_OVEN_MODBUS_CRC_ERROR;
   }
   return OK;
}

void Lw5C7::AppendChecksum()
{
   UINT32 crc=CallwlateChecksum( Lw5C7Const::PACKET_SEND );

   SendPacket[ 13 ] = ColwertIntToChar( (crc >> 4) & 0xF);
   SendPacket[ 14 ] = ColwertIntToChar( (crc     ) & 0xF);
}

UINT32 Lw5C7::CallwlateChecksum(UINT32 PacketType)
{
   UINT32 i;
   UINT32 sum = 0;

   if(Lw5C7Const::PACKET_SEND == PacketType)
   {
      for( i=1 ; i < (Lw5C7Const::PACKET_SEND_SIZE - 3); i++)
      {
         sum += SendPacket[i];
      }
   }
   else // PACKET_RECEIVE
   {
      for( i=1 ; i < (Lw5C7Const::PACKET_RECEIVE_SIZE - 3); i++)
      {
         sum += ReceivePacket[i];
      }
   }

   return (sum & 0xFF);
}

RC Lw5C7::SetSetPoint(FLOAT64 SetPoint)
{
   if(!IsOvenInitialized) return RC::DEV_OVEN_NOT_INITIALIZED;

   long IntSetPoint=(long)(10*SetPoint);

   if ( (IntSetPoint > 10*MAX_SAFE_TEMP ) || ( IntSetPoint < 10*MIN_SAFE_TEMP ) )
   {
      Printf(Tee::PriNormal, "%ld.%ld C is outside the safe range.\n",
             IntSetPoint / 10 , IntSetPoint % 10);
      Printf(Tee::PriNormal, "Must be between %ldC and %ldC\n" ,
             (long)MIN_SAFE_TEMP, (long)MAX_SAFE_TEMP );
     return RC::DEV_OVEN_ILWALID_TEMPERATURE;
   }

   return WriteReg(Lw5C7Const::FIXED_DESIRED_CONTROL_SETTING ,IntSetPoint);
}

RC Lw5C7::GetTemperature(FLOAT64 *Temperature)
{
   RC rc = OK;

   if(!IsOvenInitialized) return RC::DEV_OVEN_NOT_INITIALIZED;

   UINT32 Value;
   CHECK_RC( ReadReg(Lw5C7Const::INPUT1, &Value) );

   // consider two's compliment
   if (Value <= 0x7FFF) *Temperature = ((FLOAT64)Value)/10;
   else *Temperature = -((FLOAT64)((0x10000 - Value)))/10;
   return OK;
}

RC Lw5C7::GetSetPoint(FLOAT64 *Returlwalue)
{
   RC rc = OK;

   if(!IsOvenInitialized) return RC::DEV_OVEN_NOT_INITIALIZED;

   UINT32 Value;
   CHECK_RC( ReadReg(Lw5C7Const::DESIRED_CONTROL_VALUE, &Value) );

   // consider two's compliment
   if(Value <= 0x7FFF) *Returlwalue = ((FLOAT64)Value)/10;
   else                *Returlwalue = -((FLOAT64)((0x10000 - Value)))/10;
   return OK;
}

RC Lw5C7::Initialize(UINT32 Port)
{
   if(IsOvenInitialized) Lw5C7::Uninitialize();
   IsOvenInitialized=true;

   UINT32 StartPort = (ANY_PORT != Port) ? Port : 1;
   UINT32 EndPort   = (ANY_PORT != Port) ? Port : Serial::GetHighestComPort();

   UINT32 PortIdx;
   for(PortIdx = StartPort; PortIdx <= EndPort; PortIdx++)
   {
      pCom = GetSerialObj::GetCom(PortIdx);
      UINT32 DummyValue;

      pCom->Initialize(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1, false);
      pCom->SetBaud(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1, 9600);

      // if we can successfully read a register, then everything is OK
      if( OK == ReadReg(Lw5C7Const::INPUT1, &DummyValue) )
      {
         Printf(Tee::PriNormal,
                "Found Lw5C7 Oven %d on COM%d\n",
                s_ActiveOven,
                PortIdx);
         return OK;
      }

      pCom->Uninitialize(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1);
   }

   // both com ports failed, so the game is over
   IsOvenInitialized=false;
   return RC::DEV_OVEN_COMMUNICATION_ERROR;
}

RC Lw5C7::Uninitialize()
{
   if(IsOvenInitialized) pCom->Uninitialize(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1);
   IsOvenInitialized=false;
   return OK;
}

void Lw5C7::PrintPacket(UINT32 PacketType)
{
   UINT32 i;
   if(Lw5C7Const::PACKET_SEND == PacketType)
   {
      Printf(Tee::PriDebug, "Send Packet: ");
      for( i=0 ; i < Lw5C7Const::PACKET_SEND_SIZE ; i++)
      {
         char ch = char(SendPacket[i]);
         if(ch > 32) Printf(Tee::PriDebug, "%c ",ch);
         else Printf(Tee::PriDebug, "%02x ",ch);
      }
   }
   else // PACKET_RECEIVE
   {
      Printf(Tee::PriDebug, "Receive Packet: ");
      for( i=0 ; i < Lw5C7Const::PACKET_RECEIVE_SIZE ; i++)
      {
         char ch = char(ReceivePacket[i]);
         if(ch > 32) Printf(Tee::PriDebug, "%c ",ch);
         else Printf(Tee::PriDebug, "%02x ",ch);
      }
   }
   Printf(Tee::PriDebug, "\n");
}

RC Lw5C7::SetFan(bool FanControl)
{
   // not implemented in this oven
   return OK;
}

RC Lw5C7::GetFan(bool *Value)
{
   // not implemented in this oven
   *Value = false;
   return OK;
}

UINT32 Lw5C7::ColwertCharToInt(UINT32 in)
{
   char ch = char(in);

   if      (ch >= '0' && ch <= '9') return UINT32(ch - '0');
   else if (ch >= 'a' && ch <= 'f') return UINT32(ch - 'a' + 10);
   else if (ch >= 'A' && ch <= 'F') return UINT32(ch - 'A' + 10);
   else
   {
      Printf(Tee::PriLow, "Bad parameter %x in ColwertCharToInt()\n", ch);
      return 0;
   }
}

UINT32 Lw5C7::ColwertIntToChar(UINT32 in)
{
   MASSERT(in < 16);
   if(in >= 16) Printf(Tee::PriLow, "Bad parameter %x ColwertInToChar()\n", in);

   if(in <= 9) return UINT32('0') + in;
   else        return UINT32('a') + in - 10;
}

//------------------------------------------------------------------------------
// GF9700 Oven Interface
//------------------------------------------------------------------------------
// Code Has been test on Model: Giant Force ICT-800
namespace GF9700Const
{
    static const  UINT08 LWRRENT_TEMPERATURE            = 0;
    static const  UINT08 TEMPERATURE_SETTING            = 1;

    static const  UINT08 SetTemperatureDataPayLoad[]    = {0x40,0x30,0x31,0x31,
        0x35,0x30,0x32,0x32,0x36,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,
        0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30};
    static const  UINT32 SetTemperatureData_Length      = 32;

    static const  UINT32 OvenCodeByteIndex              = 1;
    static const  UINT32 SetTemperatureDataByteIndex    = 5;
    /*
    static const  UINT32 SetTempeSlopeByteIndex         = 13;   //the slope, now fix at 0.0C/min
    */

    static const  UINT08 GetSettingDataPayLoad[]        = {0x40,0x30,0x31,0x30,0x31};
    static const  UINT32 GetSettingData_Length          = 5;

    static const  UINT32 GetTemperatureDataByteIndex    = 5;
    static const  UINT32 GetTempeSettingDataByteIndex   = 13;

    static const  UINT08 GetOvenClockDataPayLoad[]      = {0x40,0x30,0x31,0x30,0x33};
    static const  UINT32 GetOvenClockData_Length        = 5;
    static const  UINT32 GetOvenClockByteIndex          = 5;
    static const  UINT32 GetOvenClockFormatLength       = 12;
    static const  UINT32 GetOvenClockExpectLength       = 28;

    static const  UINT08 PackageTailData[]              = {0x2a,0x0d,0x0a};
    static const  UINT32 PackageTail_Length             = 3;
}

RC GF9700::SetSetPoint(FLOAT64 SetPoint)
{
    if(!IsOvenInitialized) return RC::DEV_OVEN_NOT_INITIALIZED;

    long IntSetPoint = (long)( 10 * SetPoint );
    if ( ( IntSetPoint > 10*MAX_SAFE_TEMP ) || ( IntSetPoint < 10*MIN_SAFE_TEMP ) )
    {
        Printf(Tee::PriNormal, "%ld.%ld C is outside the safe range.\n",
            IntSetPoint / 10 , IntSetPoint % 10);
        Printf(Tee::PriNormal, "Must be between %ldC and %ldC\n" ,
            (long)MIN_SAFE_TEMP, (long)MAX_SAFE_TEMP );
        return RC::DEV_OVEN_ILWALID_TEMPERATURE;
    }

    using namespace GF9700Const;
    RC rc = OK;
    INT16 TempeSetting = (INT16)( SetPoint * 10 );

    for(UINT08 i = 0; i <  SetTemperatureData_Length; i++)
    {
        SendBuffer[i] = SetTemperatureDataPayLoad[i];       //copy payload into send buffer
    }

    SendBuffer[SetTemperatureDataByteIndex+0]   = Hex2ASCII( (TempeSetting & 0xf000) >> 12 );
    SendBuffer[SetTemperatureDataByteIndex+1]   = Hex2ASCII( (TempeSetting & 0x0f00) >> 8 );
    SendBuffer[SetTemperatureDataByteIndex+2]   = Hex2ASCII( (TempeSetting & 0x00f0) >> 4 );
    SendBuffer[SetTemperatureDataByteIndex+3]   = Hex2ASCII( (TempeSetting & 0x000f) >> 0 );

    SendPackage(SetTemperatureData_Length);

    UINT32 RcvDataLen = Rcv2Buffer();
    if ( 0 == RcvDataLen )
    {
        return RC::DEV_OVEN_COMMUNICATION_ERROR;
    }

    if ( CheckFCS(RcvDataLen) != OK )
    {
        return RC::DEV_OVEN_COMMUNICATION_ERROR;
    }

    //GF9700 need extra reading to make registers aware of setting changes
    //or a long piriod sleep is required
    FLOAT64 MyTemperature;
    long GotSetPoint;
    long MySetPoint = (long)( 10 * SetPoint );
    UINT32 Count = 0;
    do{
        Tasker::Sleep(100);
        GetSetPoint(& MyTemperature);
        GotSetPoint = (long)( 10 * MyTemperature );
        Count++;
    }while( MySetPoint != GotSetPoint && Count < 10 );
    if ( 10 == Count )
        Printf(Tee::PriNormal, "Warning: Oven didn't ack correctly. Temperature may went wrong!\n");

    //Printf(Tee::PriDebug, "Oven Ack took count %d .\n",Count);
    return rc;
}

RC GF9700::GetSetPoint(FLOAT64 *Temperature)
{
    return GetOvenTempeStatus(Temperature, GF9700Const::TEMPERATURE_SETTING);
}

RC GF9700::GetTemperature(FLOAT64 *Temperature)
{
    return GetOvenTempeStatus(Temperature, GF9700Const::LWRRENT_TEMPERATURE);
}

RC GF9700::GetOvenTempeStatus(FLOAT64 *Data, UINT08 DataType)
{
    using namespace GF9700Const;
    RC rc = OK;

    for(UINT08 i = 0; i <  GetSettingData_Length; i++)
    {
        SendBuffer[i] = GetSettingDataPayLoad[i];
    }

    SendPackage(GetSettingData_Length);

    UINT32 RcvDataLen = Rcv2Buffer();
    if ( 0 == RcvDataLen )
    {
        return RC::DEV_OVEN_COMMUNICATION_ERROR;
    }

    if ( CheckFCS(RcvDataLen) != OK )
    {
        return RC::DEV_OVEN_COMMUNICATION_ERROR;
    }

    UINT32 BitIndex = 0;
    //retrieve the data
    switch (DataType)
    {
        case LWRRENT_TEMPERATURE:
            BitIndex = GetTemperatureDataByteIndex;
            break;
        case TEMPERATURE_SETTING:
            BitIndex = GetTempeSettingDataByteIndex;
            break;
        default:
            return RC::DEV_OVEN_COMMUNICATION_ERROR;   //dummy return-code
    }

    INT16 Tempe = 0;   //temperature data are 4 bytes long.
    for(UINT32 i = BitIndex; i < BitIndex + 4; i++)
    {
        UINT16 temp = ( (UINT16) ASCII2Hex( ReceiveBuffer[i] ) ) << ( 4 * ( BitIndex + 3 - i ) );
        Tempe = Tempe | temp;
    }

    *Data = (FLOAT64)Tempe;
    *Data = *Data /100;

    return rc;

}

UINT32 GF9700::Rcv2Buffer()
{
    UINT32 ReadByteCount = pCom->ReadBufCount();
    UINT32 Count = 0;
    //wait REPLY_TIMEOUT sec for data to come
    while ( ReadByteCount == 0 && Count < (REPLY_TIMEOUT * 1000/25) )
    {
        Tasker::Sleep(25);
        Count++;
        ReadByteCount = pCom->ReadBufCount();
    }
    if ( ReadByteCount == 0 )
        return 0;

    do{
        ReadByteCount = pCom->ReadBufCount();
        Tasker::Sleep(25);      //about 10bit/ms @ 9600Buad rate
    }
    while (ReadByteCount != pCom->ReadBufCount());
    //if there is no data come in within 25ms, we think the tranfer has finished.

    if( ReadByteCount > MAX_PACKET_SIZE * 4 )
        return 0;   //buffer overflow

    UINT32 PacketLength = 0;
    while (pCom->ReadBufCount())
    {
       UINT32 Value = 0;
       if ( pCom->Get(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1, &Value) != OK )
           return 0;
       ReceiveBuffer[PacketLength++] = (UINT08)Value;
    }
    //Printf(Tee::PriDebug, "Oven ack for %d bytes.\r",PacketLength);
    return PacketLength;
}

UINT08 GF9700::CalcFCS(UINT32 PacketLength)
{
    UINT08 Fcs = 0;

    for(UINT32 i = 0; i < PacketLength; i++)
    {
        Fcs = Fcs ^ SendBuffer[i];
    }
    return Fcs;
}

RC GF9700::CheckFCS(UINT32 PacketLength)
{
    using namespace GF9700Const;

    UINT32 DataLength = PacketLength - 5;     //last 5 bytes are [FCS0][FCS1][0x2a][0x0d][0x0a]
    UINT08 Fcs = 0;

    for(UINT32 i = 0; i < DataLength; i++)
    {
        Fcs = Fcs ^ ReceiveBuffer[i];
    }

    if ( ReceiveBuffer[DataLength] != Hex2ASCII( (Fcs >> 4) & 0x0f ) )
        return RC::DEV_OVEN_COMMUNICATION_ERROR;

    if ( ReceiveBuffer[DataLength+1] != Hex2ASCII( Fcs & 0x0f) )
        return RC::DEV_OVEN_COMMUNICATION_ERROR;

    if ( ReceiveBuffer[DataLength+2] != PackageTailData[0]
        || ReceiveBuffer[DataLength+3] != PackageTailData[1]
        || ReceiveBuffer[DataLength+4] != PackageTailData[2] )
        return RC::DEV_OVEN_COMMUNICATION_ERROR;

    return OK;
}

RC GF9700::SendPackage(UINT32 DataLength)
{
    using namespace GF9700Const;
    RC rc = OK;

    pCom->ClearBuffers(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1);

    UINT08 OvenCode = 0x01;   //encode Oven code, in most of case it's 01.
    SendBuffer[OvenCodeByteIndex+0]   = Hex2ASCII( (OvenCode & 0xf0) >> 4 );
    SendBuffer[OvenCodeByteIndex+1]   = Hex2ASCII( (OvenCode & 0x0f) >> 0 );

    for(UINT32 i = 0; i < DataLength; i++)
    {
        pCom->Put(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1, SendBuffer[i]);
    }

    UINT08 fcs = CalcFCS(DataLength);

    UINT08 tmp_buf = Hex2ASCII( (fcs >> 4) & 0x0f );
    pCom->Put(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1, tmp_buf);
    tmp_buf = Hex2ASCII( fcs & 0x0f);
    pCom->Put(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1, tmp_buf);

    for (UINT32 i = 0; i < PackageTail_Length; i++)
    {
        pCom->Put(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1, PackageTailData[i]);
    }

    return rc;
}

RC GF9700::GetOvenClock(char* clk)
{
    using namespace GF9700Const;
    RC rc = OK;

    if ( clk[GetOvenClockFormatLength] != 0)
        return 0;   //a weak protection

    for(UINT32 i = 0; i <  GetOvenClockData_Length; i++)
    {
        SendBuffer[i] = GetOvenClockDataPayLoad[i];
    }

    SendPackage(GetOvenClockData_Length);

    UINT32 RcvDataLen = Rcv2Buffer();
    if ( 0 == RcvDataLen || GetOvenClockExpectLength != RcvDataLen )
        return RC::DEV_OVEN_COMMUNICATION_ERROR;

    for(UINT32 i = 0; i < GetOvenClockFormatLength; i++)
    {
        clk[i] = ReceiveBuffer[GetOvenClockByteIndex+i];
    }
    return rc;
}

RC GF9700::Initialize(UINT32 Port)
{
    if ( IsOvenInitialized )
        GF9700::Uninitialize();

    IsOvenInitialized = true;

    UINT32 StartPort = (ANY_PORT != Port) ? Port : 1;
    UINT32 EndPort   = (ANY_PORT != Port) ? Port : Serial::GetHighestComPort();

    UINT32 PortIdx;
    for(PortIdx = StartPort; PortIdx <= EndPort; PortIdx++)
    {
       pCom = GetSerialObj::GetCom(PortIdx);
       char DateTime[] = "060101075959";   //the dummy payload bytes

       pCom->Initialize(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1, false);
       pCom->SetBaud(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1, 9600);
       pCom->SetParity(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1, 'e');
       pCom->SetDataBits(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1, 7);
       pCom->SetStopBits(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1, 1);

       // if we can successfully read a register, then everything is OK
       if( OK == GetOvenClock(DateTime) )
       {
          Printf(Tee::PriNormal,
                 "Found GiantForce 9700 Oven %d on COM%d.\nIt's time is: %s\n",
                 s_ActiveOven,
                 PortIdx,
                 DateTime);
          return OK;
       }

       pCom->Uninitialize(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1);
    }

    // all com ports failed, so the game is over
    IsOvenInitialized = false;
    return RC::DEV_OVEN_COMMUNICATION_ERROR;
}

RC GF9700::Uninitialize()
{
   if ( IsOvenInitialized )
       pCom->Uninitialize(SerialConst::CLIENT_OVEN1 + s_ActiveOven - 1);
   IsOvenInitialized = false;
   return OK;
}

UINT08 GF9700::Hex2ASCII(UINT08 HexValue)
{
  if (HexValue > 0x9)
    return HexValue - 0x9 + 0x40;
  else
    return HexValue + 0x30;
}

UINT08 GF9700::ASCII2Hex(UINT08 AsciiValue)
{
  if (AsciiValue > 0x40)
    return  AsciiValue - 0x40 + 0x9;
  else
    return  AsciiValue - 0x30;
}

//no fan for GiantForce
RC GF9700::SetFan(bool FanControl)
{
    return OK;
}
//no fan for GiantForce
RC GF9700::GetFan(bool *Value)
{
    *Value = true;
    return OK;
}
//GF doesn't employee any register interface but using it's own native token-based comm protocol.
RC GF9700::WriteReg(UINT32 Register, UINT32 Value)
{
    return OK;
}
//GF doesn't employee any register interface but using it's own native token-based comm protocol.
RC GF9700::ReadReg(UINT32 Register, UINT32 *Value)
{
    *Value = 0xdeadbeef;
    Printf(Tee::PriNormal, "Warning:: ReadReg() not Implememented for Giant Force oven.\n");
    return OK;
}

//for debug purpose
void GF9700::PrintBuffer(UINT32 Type)
{
   UINT32 i;
   if( 1 == Type)       //send
   {
      Printf(Tee::PriDebug, "Send Packet: \n");
      for( i=0 ; i < 32 ; i++)
      {
         char ch = char(SendBuffer[i]);
         Printf(Tee::PriDebug, "%c ",ch);
      }
   }
   else // PACKET_RECEIVE
   {
      Printf(Tee::PriDebug, "Receive Packet: \n");
      for( i=0 ; i < 32 ; i++)
      {
         char ch = char(ReceiveBuffer[i]);
         Printf(Tee::PriDebug, "%c ",ch);
      }
      Printf(Tee::PriDebug, "\n");
      for( i=0 ; i < 32 ; i++)
      {
         char ch = char(ReceiveBuffer[i]);
         Printf(Tee::PriDebug, "%02x ",ch);
      }
   }
   Printf(Tee::PriDebug, "\n");
}
