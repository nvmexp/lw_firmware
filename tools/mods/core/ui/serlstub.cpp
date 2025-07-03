/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007,2014-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Stub serial JavaScript object for platforms that don't support serial.

#define INSIDE_SERIAL_CPP

#include "core/include/serial.h"
#include "core/include/massert.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/tee.h"
#include "core/include/types.h"
#include <ctype.h>

///////////////////////////////////////////////////////////////////////////////
// COM1
///////////////////////////////////////////////////////////////////////////////

DECLARE_SERIAL(1, 1)

////////////////////////////////////////////////////////////////////////
// Actual working routines--independent of Javascript
////////////////////////////////////////////////////////////////////////

void Serial::ProgramUart()
{
    m_WriteHead = m_WriteTail = m_WriteBuffer;
    m_ReadHead  = m_ReadTail  = m_ReadBuffer;
    m_ReadOverflow  = false;
    m_WriteOverflow = false;
}

///////////////////////////////////////
// Implementation of Serial:: API
/////////////////////////////////////////

UINT32 Serial::GetHighestComPort()
{
    return 2;
}

Serial::Serial(UINT32 Port)
{
}

Serial::~Serial()
{
}

RC Serial::Initialize(UINT32 ClientId, bool IsSynchronous)
{
    return RC::UNSUPPORTED_HARDWARE_FEATURE;
}

RC Serial::Uninitialize(UINT32 ClientId, bool IsDestructor)
{
    return OK;
}

RC Serial::SetBaud(UINT32 ClientId, UINT32 Value)
{
    return RC::UNSUPPORTED_HARDWARE_FEATURE;
}

RC Serial::SetParity(UINT32 ClientId, char par)
{
    return RC::UNSUPPORTED_HARDWARE_FEATURE;
}

RC Serial::SetDataBits(UINT32 ClientId, UINT32 NumBits)
{
    return RC::UNSUPPORTED_HARDWARE_FEATURE;
}

RC Serial::SetStopBits(UINT32 ClientId, UINT32 Value)
{
    return RC::UNSUPPORTED_HARDWARE_FEATURE;
}

RC Serial::SetAddCarriageReturn(bool Value)
{
    return RC::UNSUPPORTED_HARDWARE_FEATURE;
}

bool Serial::GetReadOverflow() const
{
    return m_ReadOverflow;
}

bool Serial::GetWriteOverflow() const
{
    return m_WriteOverflow;
}

bool Serial::SetReadOverflow(bool x)
{
    return true;
}

bool Serial::SetWriteOverflow(bool x)
{
    return true;
}

UINT32 Serial::GetBaud() const
{
    return m_Baud;
}

char   Serial::GetParity() const
{
    return m_Parity;
}

UINT32 Serial::GetDataBits() const
{
    return m_DataBits;
}

UINT32 Serial::GetStopBits() const
{
    return m_StopBits;
}

bool Serial::GetAddCarriageReturn() const
{
    return m_AddCr;
}

UINT32 Serial::ReadBufCount()
{
    return 0;
}

UINT32 Serial::WriteBufCount()
{
    return 0;
}

RC Serial::ClearBuffers(UINT32 ClientId)
{
    return OK;
}

RC Serial::Get(UINT32 ClientId, UINT32 *Value)
{
    return OK;
}

RC Serial::GetString(UINT32 ClientId, string *pStr)
{
    return RC::UNSUPPORTED_HARDWARE_FEATURE;
}

RC Serial::Put(UINT32 ClientId, UINT32 outchar)
{
    return RC::UNSUPPORTED_HARDWARE_FEATURE;
}

RC Serial::PutString(UINT32 ClientId, string *pStr)
{
    return RC::UNSUPPORTED_HARDWARE_FEATURE;
}

RC Serial::PutString(UINT32 ClientId, const char *pStr, UINT32 Length)
{
    return RC::UNSUPPORTED_HARDWARE_FEATURE;
}

RC Serial::ValidateClient(UINT32 ClientId)
{
    return RC::UNSUPPORTED_HARDWARE_FEATURE;
}

RC Serial::SetReadCallbackFunction( void (*m_ReadCallbackFunc)(char) )
{
    return RC::UNSUPPORTED_HARDWARE_FEATURE;
}

//------------------------------------------------------------------------------
// GetSerialObj namespace
//------------------------------------------------------------------------------

Serial *GetSerialObj::GetCom(UINT32 Port)
{
    switch(Port)
    {
        case 1: return &HwCom1;
        default:
                Printf(Tee::PriLow, "Caller to GetCom() requested an invalid serial port\n");
                MASSERT(1);
                // return a valid pointer anyway so the caller doesn't try to dereference
                // a null;
                return &HwCom1;
    }
}

Serial *GetSerialObj::GetCom(const char *portName)
{
    if (0 != strcmp(portName, "COM1"))
    {
        Printf(Tee::PriLow, "Caller to GetCom() requested an invalid serial port\n");
        MASSERT(1);
    }
    return &HwCom1;
}
