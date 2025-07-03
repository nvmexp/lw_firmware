/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2010 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
// MODS socket abstration implemenation file (thin wrapper around utils/socket.h)
//------------------------------------------------------------------------------

#include "core/include/sockmods.h"
#include "socket.h"
#include "core/include/massert.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include "core/include/tasker.h"

SocketMods::SocketMods(Socket * pSocket) :
    m_pBaseSocket(pSocket)
{
    MASSERT(m_pBaseSocket);
}

/* virtual */ SocketMods::~SocketMods()
{
    if (NULL != m_pBaseSocket)
        delete m_pBaseSocket;
    m_pBaseSocket = NULL;
}

// Connect to a remote server
/* virtual */ RC SocketMods::Connect(UINT32 serverip, UINT16 serverport)
{
    MASSERT(m_pBaseSocket);
    return Utility::InterpretModsUtilErrorCode(m_pBaseSocket->Connect(serverip, serverport));
}

// Do a DNS lookup
/* virtual */ UINT32 SocketMods::DoDnsLookup(const char *ServerName)
{
    MASSERT(m_pBaseSocket);
    return m_pBaseSocket->DoDnsLookup(ServerName);
}

// Create socket for others to connect to
/* virtual */ RC SocketMods::ListenOn(INT32 port)
{
    MASSERT(m_pBaseSocket);
    return Utility::InterpretModsUtilErrorCode(m_pBaseSocket->ListenOn(port));
}

/* virtual */ RC SocketMods::Accept()
{
    MASSERT(m_pBaseSocket);
    RC rc;
    CHECK_RC(Wait(Socket::ON_READ));
    return Utility::InterpretModsUtilErrorCode(m_pBaseSocket->Accept());
}

// R/W both return -1 on error, else # bytes transferred.
// For write, len is the # to send to remote connection,
// and for read, len is the max # to accept from remote connection.
/* virtual */ RC SocketMods::Write(const char *buf, UINT32 len)
{
    MASSERT(m_pBaseSocket);
    RC rc;
    CHECK_RC(Wait(Socket::ON_WRITE));
    return Utility::InterpretModsUtilErrorCode(m_pBaseSocket->Write(buf, len));
}

/* virtual */ RC SocketMods::Read (char *buf, UINT32 len, UINT32 *pBytesRead)
{
    MASSERT(m_pBaseSocket);
    RC rc;
    CHECK_RC(Wait(Socket::ON_READ));
    return Utility::InterpretModsUtilErrorCode(m_pBaseSocket->Read(buf, len, pBytesRead));
}

/* virtual */ RC SocketMods::Flush(void)
{
    MASSERT(m_pBaseSocket);
    return Utility::InterpretModsUtilErrorCode(m_pBaseSocket->Flush());
}

/* virtual */ RC SocketMods::Close(void)
{
    MASSERT(m_pBaseSocket);
    return Utility::InterpretModsUtilErrorCode(m_pBaseSocket->Close());
}

// Get local IP addresses
/* virtual */ RC SocketMods::GetHostIPs(vector<UINT32>* pIps)
{
    MASSERT(m_pBaseSocket);
    return Utility::InterpretModsUtilErrorCode(m_pBaseSocket->GetHostIPs(pIps));
}

RC SocketMods::Wait(Socket::SelectOn onWhat)
{
    RC rc;
    for (;;)
    {
        rc.Clear();
        rc = Utility::InterpretModsUtilErrorCode(
                m_pBaseSocket->Select(onWhat, 0));
        if (rc != RC::TIMEOUT_ERROR)
        {
            break;
        }
        Tasker::Yield();
    }
    return rc;
}
