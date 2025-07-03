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

#include "core/include/xp.h"
#include "core/include/ethrsink.h"
#include "core/include/massert.h"
#include "core/include/utility.h"
#include "core/include/tee.h"
#include <string.h>

EthernetSink::EthernetSink()
{
    m_pSocket = NULL;
    m_Connected = false;
}

EthernetSink::~EthernetSink()
{
}

RC EthernetSink::Open()
{
    RC rc = OK;

    SocketMods *pSocket; // temp socket

    pSocket = Xp::GetSocket();
    if(pSocket == NULL)
    {
        return RC::NETWORK_CANNOT_CREATE_SOCKET;
    }

    // RAII cleanup
    ObjectHolder<SocketMods*> CloseSocket(pSocket, &Xp::FreeSocket);

    UINT32 ServerIP = 0;
    UINT32 Port = 8888;
    string ServerPortStr;
    string ServerIPStr = Xp::GetElw("MODS_LOG_SERVER_IP");
    if(ServerIPStr == "")
    {
        return RC::NETWORK_CANNOT_CREATE_SOCKET;
    }
    ServerIP = Socket::ParseIp(ServerIPStr);

    ServerPortStr = Xp::GetElw("MODS_LOG_SERVER_PORT");
    if(ServerPortStr != "")
    {
        Port = (UINT32)atoi(ServerPortStr.c_str());
    }
    else
    {
        Port = 8888;
    }

    if(ServerIP == 0)
    {
        // default - no ethernet logging
        return RC::NETWORK_CANNOT_CREATE_SOCKET;
    }

    CHECK_RC(pSocket->Connect(ServerIP, Port));

    m_Connected = true;
    Printf(Tee::ScreenOnly, "logging to %s at port %d\n", ServerIPStr.c_str(), Port );

    // all good, cancel cleanup
    CloseSocket.Cancel();
    m_pSocket = pSocket;
    return rc;
}

RC EthernetSink::Close(void)
{
    if(m_Connected == false)
        return OK;

    MASSERT(m_pSocket);
    RC rc;
    if((rc = m_pSocket->Close()) != OK)
        return rc;

    Xp::FreeSocket(m_pSocket);
    m_pSocket = NULL;
    m_Connected = false;
    return rc;
}

void EthernetSink::DoPrint
(
    const char*           str,
    size_t                strLen,
    Tee::ScreenPrintState state
)
{
    if (!m_Connected)
        return;

    MASSERT(m_pSocket);
    MASSERT(str);

    // for now assume a very simple send
    m_pSocket->Write(str, static_cast<UINT32>(strLen));

    // todo catch error?
}
