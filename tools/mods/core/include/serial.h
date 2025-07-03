/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2014,2016-2020 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// 45678901234567890123456789012345678901234567890123456789012345678901234567890

#pragma once

#ifndef INCLUDED_SERIAL_H
#define INCLUDED_SERIAL_H

#include <string>

#include <boost/core/noncopyable.hpp>

#include "rc.h"
#include "types.h"

namespace SerialConst
{
   const UINT32 BUFFER_SIZE       = 1 << 12; // must be a power of 2

   // These constants are used to determine who owns the port.  All of the
   // CLIENT_OVENX objects must be sequential (the oven code assumes it)
   const UINT32 CLIENT_NOT_IN_USE          = 0;
   const UINT32 CLIENT_JAVASCRIPT          = 1;
   const UINT32 CLIENT_HP3632              = 2;
   const UINT32 CLIENT_SINK                = 3;
   const UINT32 CLIENT_OVEN1               = 4;
   const UINT32 CLIENT_OVEN2               = 5;
   const UINT32 CLIENT_OVEN3               = 6;
   const UINT32 CLIENT_OVEN4               = 7;
   const UINT32 CLIENT_SERDOOR             = 8;
   const UINT32 CLIENT_METER               = 9;
   const UINT32 CLIENT_IRTHERMOMETER       = 10;
   const UINT32 CLIENT_USB_FTB             = 11;
   const UINT32 CLIENT_USB_IOT_SUPERSWITCH = 12;
   const UINT32 CLIENT_HIGHESTNUM          = 12;

#ifdef INSIDE_SERIAL_CPP

   // must match CLIENT_ constants above
   // these must be declared only inside serial.cpp or you'll get a warning
   // about unused variables
   static const char *CLIENT_STRING[]=
   {
      "Not in use",
      "JavaScript",
      "Hp 3632",
      "Sink",
      "Oven #1",
      "Oven #2",
      "Oven #3",
      "Oven #4",
      "SerialDoor",
      "Meter",
      "Irthermometer",
      "USB FTB",
      "USB IOT superswitch"
   };
   ct_assert(sizeof(CLIENT_STRING) / sizeof(CLIENT_STRING[0]) ==
             CLIENT_HIGHESTNUM + 1);
#endif
}

class Serial;
namespace GetSerialObj
{
    class SerialImpl;
    Serial *GetCom(UINT32 port);
    Serial *GetCom(const char *portName);
}

class Serial : private boost::noncopyable
{
    friend class SerialBufferAccess;
    friend class GetSerialObj::SerialImpl;

    Serial(UINT32 port);
    Serial(const char *portName);

public:
    static UINT32 GetHighestComPort();

    ~Serial();

    RC Initialize(UINT32 clientId, bool isSynchronous);
    RC Uninitialize(UINT32 clientId, bool isDestructor = false);

    RC SetBaud(UINT32 clientId, UINT32 baud);
    RC SetParity(UINT32 clientId, char par);
    RC SetDataBits(UINT32 clientId, UINT32 numBits);
    RC SetStopBits(UINT32 clientId, UINT32 stopBits);
    RC SetAddCarriageReturn(bool val);

    UINT32 GetBaud() const;
    char   GetParity() const;
    UINT32 GetDataBits() const;
    UINT32 GetStopBits() const;
    bool   GetAddCarriageReturn() const;

    bool GetReadOverflow() const;
    bool GetWriteOverflow() const;
    bool SetReadOverflow(bool val);
    bool SetWriteOverflow(bool val);

    RC Put(UINT32 clientId, UINT32 outchar);
    RC PutString(UINT32 clientId, string     *pStr);
    RC PutString(UINT32 clientId, const char *pStr, UINT32 length = 0xDEADCAFE);
    RC GetString(UINT32 clientId, string     *pStr);
    RC Get(UINT32 clientId, UINT32 *value);
    RC ClearBuffers(UINT32 clientId);
    RC SetReadCallbackFunction(void (*readCallbackFunc)(char));

    UINT32 ReadBufCount();
    UINT32 WriteBufCount();

    UINT32 GetComPortId() const { return m_ComPortId; }
    const string &GetPortName() const { return m_ComPortName; }

private:
    void ProgramUart();

    void SetComPortId(UINT32 portId) { m_ComPortId = portId; }

    UINT32 m_Baud     = 19200;
    UINT32 m_DataBits = 8;
    char   m_Parity   = 'n';
    UINT32 m_StopBits = 1;
    bool   m_AddCr    = false;

    UINT32 m_ComPortId = ~0U;
    string m_ComPortName;

    volatile unsigned char *m_WriteHead = nullptr;
    volatile unsigned char *m_WriteTail = nullptr;
    volatile unsigned char *m_ReadHead  = nullptr;
    volatile unsigned char *m_ReadTail  = nullptr;

    unsigned char m_WriteBuffer[SerialConst::BUFFER_SIZE] = {};
    unsigned char m_ReadBuffer[SerialConst::BUFFER_SIZE]  = {};

    bool   m_ReadOverflow  = false;
    bool   m_WriteOverflow = false;
    UINT32 m_ComPortClient = SerialConst::CLIENT_NOT_IN_USE;

    RC ValidateClient(UINT32 clientId);
};

class SerialBufferAccess
{
public:
    static
    volatile unsigned char * GetReadHead(Serial *ser)
    {
        return ser->m_ReadHead;
    }

    static
    void SetReadHead(Serial *ser, volatile unsigned char *p)
    {
        ser->m_ReadHead = p;
    }

    static
    unsigned char * GetReadBuffer(Serial *ser)
    {
        return ser->m_ReadBuffer;
    }
};

#ifdef INSIDE_SERIAL_CPP
//------------------------------------------------------------------------------
// This enormous macro sets up the JavaScript interface for a single com port
//------------------------------------------------------------------------------
#define DECLARE_SERIAL( COM_PORT_NAME, COM_PORT_NUM )                                         \
JS_CLASS(Com ## COM_PORT_NAME  );                                                             \
Serial & HwCom ## COM_PORT_NAME = *GetSerialObj::GetCom(COM_PORT_NUM);                        \
                                                                                              \
static SObject Com ## COM_PORT_NAME ##_Object                                                 \
(                                                                                             \
   "Com" #COM_PORT_NAME,                                                                      \
   Com ## COM_PORT_NAME ## Class,                                                             \
   0,                                                                                         \
   0,                                                                                         \
   "Com " #COM_PORT_NAME "Port"                                                               \
);                                                                                            \
                                                                                              \
P_( Com ## COM_PORT_NAME ## _Get_ReadOverflow );                                              \
P_( Com ## COM_PORT_NAME ## _Set_ReadOverflow );                                              \
static SProperty Com ## COM_PORT_NAME ## _ReadOverflow                                        \
(                                                                                             \
   Com ## COM_PORT_NAME ## _Object,                                                           \
   "ReadOverflow",                                                                            \
   0,                                                                                         \
   0,                                                                                         \
   Com ## COM_PORT_NAME ## _Get_ReadOverflow,                                                 \
   Com ## COM_PORT_NAME ## _Set_ReadOverflow,                                                 \
   0,                                                                                         \
   "Gets set to true if a read overflow oclwrs"                                               \
  );                                                                                          \
                                                                                              \
P_( Com ## COM_PORT_NAME ## _Get_WriteOverflow );                                             \
P_( Com ## COM_PORT_NAME ## _Set_WriteOverflow );                                             \
static SProperty Com ## COM_PORT_NAME ## _WriteOverflow                                       \
(                                                                                             \
   Com ## COM_PORT_NAME ## _Object,                                                           \
   "WriteOverflow",                                                                           \
   0,                                                                                         \
   0,                                                                                         \
   Com ## COM_PORT_NAME ## _Get_WriteOverflow,                                                \
   Com ## COM_PORT_NAME ## _Set_WriteOverflow,                                                \
   0,                                                                                         \
   "Gets set to true if a write overflow oclwrs"                                              \
  );                                                                                          \
                                                                                              \
P_( Com ## COM_PORT_NAME ## _Get_Baud );                                                      \
P_( Com ## COM_PORT_NAME ## _Set_Baud );                                                      \
static SProperty Com ## COM_PORT_NAME ## _Baud                                                \
(                                                                                             \
   Com ## COM_PORT_NAME ## _Object,                                                           \
   "Baud",                                                                                    \
   0,                                                                                         \
   0,                                                                                         \
   Com ## COM_PORT_NAME ## _Get_Baud,                                                         \
   Com ## COM_PORT_NAME ## _Set_Baud,                                                         \
   0,                                                                                         \
   "Baud Rate"                                                                                \
  );                                                                                          \
                                                                                              \
P_( Com ## COM_PORT_NAME ## _Get_DataBits );                                                  \
P_( Com ## COM_PORT_NAME ## _Set_DataBits );                                                  \
static SProperty Com ## COM_PORT_NAME ## _DataBits                                            \
(                                                                                             \
   Com ## COM_PORT_NAME ## _Object,                                                           \
   "DataBits",                                                                                \
   0,                                                                                         \
   0,                                                                                         \
   Com ## COM_PORT_NAME ## _Get_DataBits,                                                     \
   Com ## COM_PORT_NAME ## _Set_DataBits,                                                     \
   0,                                                                                         \
   "Data Bits (5, 6, 7 or 8)"                                                                 \
  );                                                                                          \
                                                                                              \
P_( Com ## COM_PORT_NAME ## _Get_Parity );                                                    \
P_( Com ## COM_PORT_NAME ## _Set_Parity );                                                    \
static SProperty Com ## COM_PORT_NAME ## _Parity                                              \
(                                                                                             \
   Com ## COM_PORT_NAME ## _Object,                                                           \
   "Parity",                                                                                  \
   0,                                                                                         \
   0,                                                                                         \
   Com ## COM_PORT_NAME ## _Get_Parity,                                                       \
   Com ## COM_PORT_NAME ## _Set_Parity,                                                       \
   0,                                                                                         \
   "Parity (none, even, odd, mark, space)"                                                    \
   );                                                                                         \
                                                                                              \
P_( Com ## COM_PORT_NAME ## _Get_StopBits );                                                  \
P_( Com ## COM_PORT_NAME ## _Set_StopBits );                                                  \
static SProperty Com ## COM_PORT_NAME ## _StopBits                                            \
(                                                                                             \
   Com ## COM_PORT_NAME ## _Object,                                                           \
   "StopBits",                                                                                \
   0,                                                                                         \
   0,                                                                                         \
   Com ## COM_PORT_NAME ## _Get_StopBits,                                                     \
   Com ## COM_PORT_NAME ## _Set_StopBits,                                                     \
   0,                                                                                         \
   "Stop Bits (1 or 2)"                                                                       \
);                                                                                            \
                                                                                              \
C_(Com ## COM_PORT_NAME ## _Get);                                                             \
STest Com ## COM_PORT_NAME ## _Get                                                            \
(                                                                                             \
    Com ## COM_PORT_NAME ## _Object,                                                          \
   "Get",                                                                                     \
   C_Com ## COM_PORT_NAME ## _Get,                                                            \
   1,                                                                                         \
   "Get a byte from the serial port"                                                          \
);                                                                                            \
                                                                                              \
C_(Com ## COM_PORT_NAME ## _GetString);                                                       \
STest Com ## COM_PORT_NAME ## _GetString                                                      \
(                                                                                             \
    Com ## COM_PORT_NAME ## _Object,                                                          \
   "GetString",                                                                               \
   C_Com ## COM_PORT_NAME ## _GetString,                                                      \
   1,                                                                                         \
   "Get the entire read buffer as a string"                                                   \
);                                                                                            \
                                                                                              \
C_(Com ## COM_PORT_NAME ## _Initialize);                                                      \
STest Com ## COM_PORT_NAME ## _Initialize                                                     \
(                                                                                             \
    Com ## COM_PORT_NAME ## _Object,                                                          \
   "Initialize",                                                                              \
   C_Com ## COM_PORT_NAME ## _Initialize,                                                     \
   0,                                                                                         \
   "Initialize serial port"                                                                   \
);                                                                                            \
                                                                                              \
C_(Com ## COM_PORT_NAME ## _Uninitialize);                                                    \
STest Com ## COM_PORT_NAME ## _Uninitialize                                                   \
(                                                                                             \
    Com ## COM_PORT_NAME ## _Object,                                                          \
   "Uninitialize",                                                                            \
   C_Com ## COM_PORT_NAME ## _Uninitialize,                                                   \
   0,                                                                                         \
   "Uninitialize serial port"                                                                 \
);                                                                                            \
                                                                                              \
P_( Com ## COM_PORT_NAME ## _GetReadBufCount );                                               \
static SProperty Com ## COM_PORT_NAME ## _ReadBufCount (                                      \
   Com ## COM_PORT_NAME ## _Object,                                                           \
   "ReadBufCount",                                                                            \
   0,                                                                                         \
   0,                                                                                         \
   Com ## COM_PORT_NAME ## _GetReadBufCount,                                                  \
   0,                                                                                         \
   0,                                                                                         \
   "Count of chracters in the read buffer"                                                    \
);                                                                                            \
                                                                                              \
P_( Com ## COM_PORT_NAME ## _GetWriteBufCount );                                              \
static SProperty Com ## COM_PORT_NAME ## _WriteBufCount (                                     \
   Com ## COM_PORT_NAME ## _Object,                                                           \
   "WriteBufCount",                                                                           \
   0,                                                                                         \
   0,                                                                                         \
   Com ## COM_PORT_NAME ## _GetWriteBufCount,                                                 \
   0,                                                                                         \
   0,                                                                                         \
   "Count of chracters in the write buffer"                                                   \
);                                                                                            \
                                                                                              \
C_(Com ## COM_PORT_NAME ## _PutString);                                                       \
STest Com ## COM_PORT_NAME ## _PutString                                                      \
(                                                                                             \
    Com ## COM_PORT_NAME ## _Object,                                                          \
   "PutString",                                                                               \
   C_Com ## COM_PORT_NAME ## _PutString,                                                      \
   1,                                                                                         \
   "Send a string to the serial port"                                                         \
);                                                                                            \
                                                                                              \
C_(Com ## COM_PORT_NAME ## _Put);                                                             \
STest Com ## COM_PORT_NAME ## _Put                                                            \
(                                                                                             \
    Com ## COM_PORT_NAME ## _Object,                                                          \
   "Put",                                                                                     \
   C_Com ## COM_PORT_NAME ## _Put,                                                            \
   1,                                                                                         \
   "Send a single byte to the serial port"                                                    \
);                                                                                            \
                                                                                              \
C_(Com ## COM_PORT_NAME ## _ClearBuffers);                                                    \
STest Com ## COM_PORT_NAME ## _ClearBuffers                                                   \
(                                                                                             \
    Com ## COM_PORT_NAME ## _Object,                                                          \
   "ClearBuffers",                                                                            \
   C_Com ## COM_PORT_NAME ## _ClearBuffers,                                                   \
   0,                                                                                         \
   "Zero the read and write buffers"                                                          \
);                                                                                            \
                                                                                              \
P_( Com ## COM_PORT_NAME ## _Get_ReadOverflow )                                               \
{                                                                                             \
   MASSERT(pContext != 0);                                                                    \
   MASSERT(pValue   != 0);                                                                    \
                                                                                              \
   UINT32 Returlwal;                                                                          \
   Returlwal=HwCom ## COM_PORT_NAME .GetReadOverflow();                                       \
                                                                                              \
   if (OK != JavaScriptPtr()->ToJsval(Returlwal, pValue))                                     \
   {                                                                                          \
      JS_ReportError(pContext, "Failed to get Com ## COM_PORT_NAME ##.ReadOverflow.");        \
      *pValue = JSVAL_NULL;                                                                   \
      return JS_FALSE;                                                                        \
   }                                                                                          \
   return JS_TRUE;                                                                            \
}                                                                                             \
                                                                                              \
P_( Com ## COM_PORT_NAME ##_Set_ReadOverflow )                                                \
{                                                                                             \
   MASSERT(pContext != 0);                                                                    \
   MASSERT(pValue   != 0);                                                                    \
                                                                                              \
   bool Value;                                                                                \
   if (OK != JavaScriptPtr()->FromJsval(*pValue, &Value))                                     \
   {                                                                                          \
      JS_ReportError(pContext, "Failed to set Com" #COM_PORT_NAME ".ReadOverflow.");          \
      return JS_FALSE;                                                                        \
   }                                                                                          \
   HwCom ## COM_PORT_NAME .SetReadOverflow(Value);                                            \
   return JS_TRUE;                                                                            \
}                                                                                             \
                                                                                              \
P_( Com ## COM_PORT_NAME ##_Get_WriteOverflow )                                               \
{                                                                                             \
   MASSERT(pContext != 0);                                                                    \
   MASSERT(pValue   != 0);                                                                    \
                                                                                              \
   UINT32 Returlwal;                                                                          \
   Returlwal=HwCom ## COM_PORT_NAME .GetWriteOverflow();                                      \
                                                                                              \
   if (OK != JavaScriptPtr()->ToJsval(Returlwal, pValue))                                     \
   {                                                                                          \
      JS_ReportError(pContext, "Failed to get Com" #COM_PORT_NAME ".WriteOverflow.");         \
      *pValue = JSVAL_NULL;                                                                   \
      return JS_FALSE;                                                                        \
   }                                                                                          \
   return JS_TRUE;                                                                            \
}                                                                                             \
                                                                                              \
P_( Com ## COM_PORT_NAME ##_Set_WriteOverflow )                                               \
{                                                                                             \
   MASSERT(pContext != 0);                                                                    \
   MASSERT(pValue   != 0);                                                                    \
                                                                                              \
   bool Value;                                                                                \
   if (OK != JavaScriptPtr()->FromJsval(*pValue, &Value))                                     \
   {                                                                                          \
      JS_ReportError(pContext, "Failed to set Com" #COM_PORT_NAME ".WriteOverflow.");         \
      return JS_FALSE;                                                                        \
   }                                                                                          \
   HwCom ## COM_PORT_NAME .SetWriteOverflow(Value);                                           \
   return JS_TRUE;                                                                            \
}                                                                                             \
                                                                                              \
P_( Com ## COM_PORT_NAME ##_Get_Baud )                                                        \
{                                                                                             \
   MASSERT(pContext != 0);                                                                    \
   MASSERT(pValue   != 0);                                                                    \
                                                                                              \
   UINT32 Returlwal;                                                                          \
   Returlwal=HwCom ## COM_PORT_NAME .GetBaud();                                               \
                                                                                              \
   if (OK != JavaScriptPtr()->ToJsval(Returlwal, pValue))                                     \
   {                                                                                          \
      JS_ReportError(pContext, "Failed to get Com" #COM_PORT_NAME ".Baud.");                  \
      *pValue = JSVAL_NULL;                                                                   \
      return JS_FALSE;                                                                        \
   }                                                                                          \
   return JS_TRUE;                                                                            \
}                                                                                             \
                                                                                              \
P_( Com ## COM_PORT_NAME ##_Set_Baud )                                                        \
{                                                                                             \
   MASSERT(pContext != 0);                                                                    \
   MASSERT(pValue   != 0);                                                                    \
                                                                                              \
   UINT32 Value;                                                                              \
   if (OK != JavaScriptPtr()->FromJsval(*pValue, &Value))                                     \
   {                                                                                          \
      JS_ReportError(pContext, "Failed to set Com" #COM_PORT_NAME ".WriteOverflow.");         \
      return JS_FALSE;                                                                        \
   }                                                                                          \
   HwCom ## COM_PORT_NAME .SetBaud(SerialConst::CLIENT_JAVASCRIPT, Value);                    \
   return JS_TRUE;                                                                            \
}                                                                                             \
                                                                                              \
P_( Com ## COM_PORT_NAME ##_Get_DataBits )                                                    \
{                                                                                             \
   MASSERT(pContext != 0);                                                                    \
   MASSERT(pValue   != 0);                                                                    \
                                                                                              \
   UINT32 Returlwal;                                                                          \
   Returlwal=HwCom ## COM_PORT_NAME .GetDataBits();                                           \
   if (OK != JavaScriptPtr()->ToJsval(Returlwal, pValue))                                     \
   {                                                                                          \
      JS_ReportError(pContext, "Failed to get Com" #COM_PORT_NAME ".DataBits.");              \
      *pValue = JSVAL_NULL;                                                                   \
      return JS_FALSE;                                                                        \
   }                                                                                          \
   return JS_TRUE;                                                                            \
}                                                                                             \
                                                                                              \
P_( Com ## COM_PORT_NAME ##_Set_DataBits )                                                    \
{                                                                                             \
   MASSERT(pContext != 0);                                                                    \
   MASSERT(pValue   != 0);                                                                    \
                                                                                              \
   UINT32 Value;                                                                              \
   if (OK != JavaScriptPtr()->FromJsval(*pValue, &Value))                                     \
   {                                                                                          \
       JS_ReportError(pContext, "Failed to set Com" #COM_PORT_NAME ".DataBits.");             \
       return JS_FALSE;                                                                       \
   }                                                                                          \
   if(Value < 5 || Value > 8)                                                                 \
   {                                                                                          \
       JS_ReportError(pContext, "DataBits must be 5, 6, 7 or 8");                             \
       return JS_FALSE;                                                                       \
   }                                                                                          \
   HwCom ## COM_PORT_NAME .SetDataBits(SerialConst::CLIENT_JAVASCRIPT, Value);                \
   return JS_TRUE;                                                                            \
}                                                                                             \
                                                                                              \
P_( Com ## COM_PORT_NAME ##_Get_Parity )                                                      \
{                                                                                             \
   MASSERT(pContext != 0);                                                                    \
   MASSERT(pValue   != 0);                                                                    \
                                                                                              \
   string Returlwal;                                                                          \
   switch(HwCom ## COM_PORT_NAME .GetParity())                                                \
   {                                                                                          \
   case 'n': Returlwal="none";  break;                                                        \
   case 'o': Returlwal="odd";   break;                                                        \
   case 'e': Returlwal="even";  break;                                                        \
   case 'm': Returlwal="mark";  break;                                                        \
   case 's': Returlwal="space"; break;                                                        \
   }                                                                                          \
   if (OK != JavaScriptPtr()->ToJsval(Returlwal, pValue))                                     \
   {                                                                                          \
      JS_ReportError(pContext, "Failed to get Com" #COM_PORT_NAME ".Parity.");                \
      *pValue = JSVAL_NULL;                                                                   \
      return JS_FALSE;                                                                        \
   }                                                                                          \
   return JS_TRUE;                                                                            \
}                                                                                             \
                                                                                              \
P_( Com ## COM_PORT_NAME ##_Set_Parity )                                                      \
{                                                                                             \
   MASSERT(pContext != 0);                                                                    \
   MASSERT(pValue   != 0);                                                                    \
                                                                                              \
   string Value;                                                                              \
   if (OK != JavaScriptPtr()->FromJsval(*pValue, &Value))                                     \
   {                                                                                          \
       JS_ReportError(pContext, "Failed to set Com" #COM_PORT_NAME ".Parity.");               \
       return JS_FALSE;                                                                       \
   }                                                                                          \
   UINT32 i;                                                                                  \
   char TranslatedParity='x';                                                                 \
   for(i=0;i<Value.length();i++) Value[i]=tolower(Value[i]);                                  \
                                                                                              \
   if     (Value=="none")  TranslatedParity='n';                                              \
   else if(Value=="odd")   TranslatedParity='o';                                              \
   else if(Value=="even")  TranslatedParity='e';                                              \
   else if(Value=="mark")  TranslatedParity='m';                                              \
   else if(Value=="space") TranslatedParity='s';                                              \
   else                                                                                       \
   {                                                                                          \
      JS_ReportError(pContext,                                                                \
         "Parity must be none, odd, even, mark or space.");                                   \
      return JS_FALSE;                                                                        \
   }                                                                                          \
   HwCom ## COM_PORT_NAME .SetParity(SerialConst::CLIENT_JAVASCRIPT, TranslatedParity);       \
   return JS_TRUE;                                                                            \
}                                                                                             \
                                                                                              \
P_( Com ## COM_PORT_NAME ##_Get_StopBits )                                                    \
{                                                                                             \
   MASSERT(pContext != 0);                                                                    \
   MASSERT(pValue   != 0);                                                                    \
                                                                                              \
   UINT32 Returlwal;                                                                          \
   Returlwal=HwCom ## COM_PORT_NAME .GetStopBits();                                           \
                                                                                              \
   if (OK != JavaScriptPtr()->ToJsval(Returlwal, pValue))                                     \
   {                                                                                          \
      JS_ReportError(pContext, "Failed to get Com" #COM_PORT_NAME ".StopBits.");              \
      *pValue = JSVAL_NULL;                                                                   \
      return JS_FALSE;                                                                        \
   }                                                                                          \
   return JS_TRUE;                                                                            \
}                                                                                             \
                                                                                              \
P_( Com ## COM_PORT_NAME ##_Set_StopBits )                                                    \
{                                                                                             \
   MASSERT(pContext != 0);                                                                    \
   MASSERT(pValue   != 0);                                                                    \
                                                                                              \
   UINT32 Value;                                                                              \
   if (OK != JavaScriptPtr()->FromJsval(*pValue, &Value))                                     \
   {                                                                                          \
       JS_ReportError(pContext, "Failed to set Com" #COM_PORT_NAME ".StopBits.");             \
       return JS_FALSE;                                                                       \
   }                                                                                          \
   HwCom ## COM_PORT_NAME .SetStopBits(SerialConst::CLIENT_JAVASCRIPT, Value);                \
   return JS_TRUE;                                                                            \
}                                                                                             \
                                                                                              \
C_(Com ## COM_PORT_NAME ##_Put)                                                               \
{                                                                                             \
    MASSERT(pContext    !=0);                                                                 \
    MASSERT(pArguments  !=0);                                                                 \
    MASSERT(pReturlwalue!=0);                                                                 \
                                                                                              \
    JavaScriptPtr pJavaScript;                                                                \
                                                                                              \
    if(NumArguments!=1)                                                                       \
    {                                                                                         \
        JS_ReportError(pContext, "Usage: Put(data)");                                         \
        return JS_FALSE;                                                                      \
    }                                                                                         \
    UINT32 SendChar;                                                                          \
    if (OK != pJavaScript->FromJsval(pArguments[0], &SendChar))                               \
    {                                                                                         \
       JS_ReportError(pContext, "Failed to get argument.");                                   \
       return JS_FALSE;                                                                       \
    }                                                                                         \
                                                                                              \
    RC Rval=HwCom ## COM_PORT_NAME .Put(SerialConst::CLIENT_JAVASCRIPT, SendChar);            \
                                                                                              \
    if ( OK != pJavaScript->ToJsval( 0, pReturlwalue ))                                       \
    {                                                                                         \
       JS_ReportError( pContext, "Error oclwrred in Com" #COM_PORT_NAME ".Write()" );         \
       *pReturlwalue = JSVAL_NULL;                                                            \
       return JS_FALSE;                                                                       \
    }                                                                                         \
    RETURN_RC(Rval);                                                                          \
}                                                                                             \
                                                                                              \
C_(Com ## COM_PORT_NAME ##_PutString)                                                         \
{                                                                                             \
   MASSERT(pContext     !=0);                                                                 \
   MASSERT(pArguments   !=0);                                                                 \
   MASSERT(pReturlwalue !=0);                                                                 \
                                                                                              \
   JavaScriptPtr pJavaScript;                                                                 \
                                                                                              \
   string SendString;                                                                         \
   if                                                                                         \
   ( (NumArguments != 1)                                                                      \
      || (OK != pJavaScript->FromJsval(pArguments[0], &SendString)) )                         \
   {                                                                                          \
       JS_ReportError(pContext, "Usage: PutString(string)");                                  \
       return JS_FALSE;                                                                       \
   }                                                                                          \
                                                                                              \
   if (OK !=HwCom ## COM_PORT_NAME .PutString(SerialConst::CLIENT_JAVASCRIPT, &SendString))   \
      RETURN_RC(RC::SERIAL_COMMUNICATION_ERROR);                                              \
                                                                                              \
   RETURN_RC(OK);                                                                             \
}                                                                                             \
                                                                                              \
                                                                                              \
C_(Com ## COM_PORT_NAME ##_GetString)                                                         \
{                                                                                             \
   MASSERT(pContext     !=0);                                                                 \
   MASSERT(pArguments   !=0);                                                                 \
   MASSERT(pReturlwalue !=0);                                                                 \
                                                                                              \
   JavaScriptPtr pJavaScript;                                                                 \
                                                                                              \
   JSObject * pReturlwals;                                                                    \
   string LocalString;                                                                        \
                                                                                              \
   if (                                                                                       \
         (NumArguments != 0) &&                                                               \
                                                                                              \
        !( (NumArguments == 1) &&                                                             \
           (OK == pJavaScript->FromJsval(pArguments[0], &pReturlwals))  )                     \
         )                                                                                    \
   {                                                                                          \
      JS_ReportError(pContext, "Usage: GetString([string])");                                 \
      return JS_FALSE;                                                                        \
   }                                                                                          \
                                                                                              \
   if (OK != HwCom ## COM_PORT_NAME .GetString(SerialConst::CLIENT_JAVASCRIPT, &LocalString)) \
      RETURN_RC(RC::SERIAL_COMMUNICATION_ERROR);                                              \
                                                                                              \
   /* if 0 arguments, just print the string */                                                \
   if ( NumArguments == 0 )                                                                   \
   {                                                                                          \
      Printf(Tee::PriNormal, "%s",LocalString.c_str());                                       \
      RETURN_RC(OK);                                                                          \
   }                                                                                          \
                                                                                              \
   /* if 1 argument, put the string in the array reference passed in */                       \
   RETURN_RC(pJavaScript->SetElement(pReturlwals, 0, LocalString));                           \
}                                                                                             \
                                                                                              \
C_(Com ## COM_PORT_NAME ##_Get)                                                               \
{                                                                                             \
   MASSERT(pContext     !=0);                                                                 \
   MASSERT(pArguments   !=0);                                                                 \
   MASSERT(pReturlwalue !=0);                                                                 \
                                                                                              \
   JavaScriptPtr pJavaScript;                                                                 \
   JSObject * pReturlwals;                                                                    \
                                                                                              \
   if ( (NumArguments != 1)                                                                   \
      || (OK != pJavaScript->FromJsval(pArguments[0], &pReturlwals)) )                        \
   {                                                                                          \
      JS_ReportError(pContext, "Usage: Get([Array])");                                        \
      return JS_FALSE;                                                                        \
   }                                                                                          \
                                                                                              \
   RC rc = OK;                                                                                \
   UINT32 LocalReturlwal = 0;                                                                 \
   C_CHECK_RC(HwCom ## COM_PORT_NAME .Get(SerialConst::CLIENT_JAVASCRIPT, &LocalReturlwal));  \
                                                                                              \
   RETURN_RC(pJavaScript->SetElement(pReturlwals, 0, LocalReturlwal));                        \
}                                                                                             \
                                                                                              \
C_(Com ## COM_PORT_NAME ##_Initialize)                                                        \
{                                                                                             \
    MASSERT(pContext     !=0);                                                                \
    MASSERT(pArguments   !=0);                                                                \
    MASSERT(pReturlwalue !=0);                                                                \
                                                                                              \
    if(NumArguments != 0)                                                                     \
    {                                                                                         \
       JS_ReportError(pContext,"Usage: Com" #COM_PORT_NAME ".Initialize()");                  \
       return JS_FALSE;                                                                       \
    }                                                                                         \
    RC rc = HwCom ## COM_PORT_NAME .Initialize(SerialConst::CLIENT_JAVASCRIPT, false);        \
    RETURN_RC(rc);                                                                            \
}                                                                                             \
                                                                                              \
C_(Com ## COM_PORT_NAME ##_ClearBuffers)                                                      \
{                                                                                             \
    MASSERT(pContext    !=0);                                                                 \
    MASSERT(pArguments  !=0);                                                                 \
    MASSERT(pReturlwalue   !=0);                                                              \
                                                                                              \
    if(NumArguments != 0)                                                                     \
    {                                                                                         \
       JS_ReportError(pContext,"Usage: Com" #COM_PORT_NAME ".ClearBuffers()");                \
       return JS_FALSE;                                                                       \
    }                                                                                         \
    HwCom ## COM_PORT_NAME .ClearBuffers(SerialConst::CLIENT_JAVASCRIPT);                     \
    return JS_TRUE;                                                                           \
}                                                                                             \
                                                                                              \
                                                                                              \
P_( Com ## COM_PORT_NAME ##_GetReadBufCount )                                                 \
{                                                                                             \
   MASSERT(pContext != 0);                                                                    \
   MASSERT(pValue   != 0);                                                                    \
                                                                                              \
   UINT32 Returlwal = HwCom ## COM_PORT_NAME .ReadBufCount();                                 \
                                                                                              \
   if (OK != JavaScriptPtr()->ToJsval(Returlwal, pValue))                                     \
   {                                                                                          \
       JS_ReportError(pContext, "Failed to get Com" #COM_PORT_NAME ".ReadBufCount.");         \
      *pValue = JSVAL_NULL;                                                                   \
      return JS_FALSE;                                                                        \
   }                                                                                          \
   return JS_TRUE;                                                                            \
}                                                                                             \
                                                                                              \
                                                                                              \
P_( Com ## COM_PORT_NAME ##_GetWriteBufCount )                                                \
{                                                                                             \
   MASSERT(pContext != 0);                                                                    \
   MASSERT(pValue   != 0);                                                                    \
                                                                                              \
   UINT32 Returlwal = HwCom ## COM_PORT_NAME .WriteBufCount();                                \
                                                                                              \
   if (OK != JavaScriptPtr()->ToJsval(Returlwal, pValue))                                     \
   {                                                                                          \
      JS_ReportError(pContext, "Failed to get Com" #COM_PORT_NAME ".WriteBufCount.");         \
      *pValue = JSVAL_NULL;                                                                   \
      return JS_FALSE;                                                                        \
   }                                                                                          \
   return JS_TRUE;                                                                            \
}                                                                                             \
                                                                                              \
C_(Com ## COM_PORT_NAME ##_Uninitialize)                                                      \
{                                                                                             \
   RC rc = HwCom ## COM_PORT_NAME .Uninitialize(SerialConst::CLIENT_JAVASCRIPT);              \
   RETURN_RC(rc);                                                                             \
}

#endif // INSIDE_SERIAL_CPP

#endif // ! INCLUDED_SERIAL_H
