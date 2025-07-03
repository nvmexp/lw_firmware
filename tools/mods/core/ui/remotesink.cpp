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

//------------------------------------------------------------------------------
// Remote data sink class.
//------------------------------------------------------------------------------

#include "core/include/remotesink.h"
#include <string>

RemoteDataSink::RemoteDataSink(SocketMods *pSocket) :
   m_pSocket(pSocket),
   m_BufferLen(0)
{}

RemoteDataSink::~RemoteDataSink() {}

void RemoteDataSink::DoPrint
(
    const char*           str,
    size_t                strLen,
    Tee::ScreenPrintState state
)
{
    const char* const end = str + strLen;

    while (str < end)
    {
        const char c = *(str++);
        if (c == '\0')
        {
            break;
        }
        else if (c == '\r')
        {
            // ignore CRs
        }
        else if (m_BufferLen == (BUFFERLENGTH-1) ||
                 c           == '\n')
        {
            // pass LFs or terminate line before buffer overflow
            m_Buffer[m_BufferLen++] = '\n';
            m_pSocket->Write(m_Buffer, m_BufferLen);
            m_BufferLen = 0;
        }
        else
        {
            m_Buffer[m_BufferLen++] = c;
        }
    }
}
