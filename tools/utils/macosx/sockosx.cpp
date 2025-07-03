/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
// TCP socket for Osx implementation
//------------------------------------------------------------------------------

#include "lwdiagutils.h"
#include "sockosx.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/select.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <signal.h>

#define ILWALID_SOCKET 0xffff

// initialize SocketOsx static variables
int SocketOsx::s_InstanceCount = 0;
bool SocketOsx::s_SystemSocketInited = false;

SocketOsx::SocketOsx() :
   m_ServerSock(ILWALID_SOCKET),
   m_ClientSock(ILWALID_SOCKET),
   m_IsValid(false)
{
   AddInstance();
}

SocketOsx::SocketOsx(const SocketOsx& o) :
   m_ServerSock(o.m_ServerSock),
   m_ClientSock(o.m_ClientSock),
   m_IsValid(o.m_IsValid),
   m_ServerSIN(o.m_ServerSIN),
   m_ClientSIN(o.m_ClientSIN)
{
   AddInstance();
}

SocketOsx::~SocketOsx()
{
   // Ensure socket is closed
   if (m_IsValid)
   {
      Close();
      m_IsValid = false;
   }

   RemoveInstance();
}

LwDiagUtils::EC SocketOsx::Connect(UINT32 serverip, UINT16 serverport)
{
   // make sure the system socket is initialized and
   // the socket has not already been connected
   if (!s_SystemSocketInited)
      return LwDiagUtils::NETWORK_NOT_INITIALIZED;

   if (m_IsValid)
      return LwDiagUtils::NETWORK_ALREADY_CONNECTED;

   // create socket
   m_ClientSock = socket(AF_INET, SOCK_STREAM, 0);

   if (m_ClientSock == ILWALID_SOCKET)
      return LwDiagUtils::NETWORK_CANNOT_CREATE_SOCKET;

   // connect to server
   sockaddr_in peerSin;
   peerSin.sin_family = AF_INET;
   peerSin.sin_port = htons(serverport);
   peerSin.sin_addr.s_addr = htonl(serverip);
   int sock_err = connect(m_ClientSock,
                          (struct sockaddr *) &peerSin,
                          sizeof(peerSin));

   if (sock_err)
   {
      shutdown(m_ClientSock, 0);
      return LwDiagUtils::NETWORK_CANNOT_CONNECT;
   }

   m_IsValid = true;

   return LwDiagUtils::OK;
}

UINT32 SocketOsx::DoDnsLookup(const char *ServerName)
{
   struct hostent *HostInfo;
   HostInfo = gethostbyname(const_cast<char *>(ServerName));

   if(!HostInfo)
      return 0;

   return ParseIp(inet_ntoa( *(struct in_addr*)HostInfo->h_addr));
}

LwDiagUtils::EC SocketOsx::ListenOn(INT32 port)
{
   // make sure the system socket is initialized and
   // the client socket has not already been connected
   if (!s_SystemSocketInited)
      return LwDiagUtils::NETWORK_NOT_INITIALIZED;

   if (m_IsValid)
      return LwDiagUtils::NETWORK_ALREADY_CONNECTED;

   // create socket
   m_ServerSock = socket(AF_INET, SOCK_STREAM, 0);

   if (m_ServerSock == ILWALID_SOCKET)
      return LwDiagUtils::NETWORK_CANNOT_CREATE_SOCKET;

   // define local address and port to listen on
   m_ServerSIN.sin_family = AF_INET;
   m_ServerSIN.sin_port = htons(port);
   m_ServerSIN.sin_addr.s_addr = INADDR_ANY;

   // bind socket
   int sock_err = ::bind(m_ServerSock,
                       (struct sockaddr *) &m_ServerSIN,
                       sizeof (m_ServerSIN));

   if (sock_err)
   {
      shutdown(m_ServerSock, 0);
      return LwDiagUtils::NETWORK_CANNOT_BIND;
   }

   // find out local IP address of the interface to which the socket is bound
   int addrsize = sizeof(m_ServerSIN);
   sock_err = getsockname(m_ServerSock, (struct sockaddr *) &m_ServerSIN,
                          (socklen_t *)&addrsize);

   // if error, set to zero
   if (sock_err)
   {
      m_ServerSIN.sin_addr.s_addr = 0;
   }

   // listen for one incoming connection
   sock_err = listen (m_ServerSock, 1);

   if (sock_err)
   {
      shutdown (m_ServerSock, 0);
      return LwDiagUtils::NETWORK_ERROR;
   }

   return LwDiagUtils::OK;
}

LwDiagUtils::EC SocketOsx::Accept()
{
   // make sure the system socket is initialized and
   // the client socket has not already been connected
   if (!s_SystemSocketInited)
      return LwDiagUtils::NETWORK_NOT_INITIALIZED;

   if (m_IsValid)
      return LwDiagUtils::NETWORK_ALREADY_CONNECTED;

   int ClientSINLen = sizeof(m_ClientSIN);

   m_ClientSock = accept (m_ServerSock,     // listening socket
                          (struct sockaddr *) &m_ClientSIN,
                          (socklen_t *) &ClientSINLen);

   // stop the server socket from listening for more connection attempts
   shutdown (m_ServerSock, 0);
   m_ServerSock = ILWALID_SOCKET;

   m_IsValid = true;

   return (m_ClientSock == ILWALID_SOCKET) ? LwDiagUtils::NETWORK_ERROR : LwDiagUtils::OK;
}

LwDiagUtils::EC SocketOsx::Select(SelectOn selectOn, FLOAT64 timeoutMs)
{
    // Use either server socket or client socket
    int sock = ILWALID_SOCKET;
    if (m_ServerSock != ILWALID_SOCKET)
    {
        sock = m_ServerSock;
    }
    else if (m_ClientSock != ILWALID_SOCKET)
    {
        sock = m_ClientSock;
    }
    else
    {
        return LwDiagUtils::NETWORK_NOT_INITIALIZED;
    }

    // Prepare fd sets for select()
    fd_set rfds;
    fd_set wfds;
    fd_set efds;

    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_ZERO(&efds);

    if (selectOn != ON_WRITE)
    {
        FD_SET(sock, &rfds);
    }
    if (selectOn != ON_READ)
    {
        FD_SET(sock, &wfds);
    }
    FD_SET(sock, &efds);

    // Callwalate timeout
    timeval tv;
    tv.tv_sec = static_cast<unsigned>(timeoutMs / 1000);
    tv.tv_usec = static_cast<unsigned>((timeoutMs - tv.tv_sec*1000)*1000);

    // Ilwoke select
    // The first argument is the highest-numbered file descriptor in any
    // of the three fd sets, plus 1.
    const int ret = select(sock+1, &rfds, &wfds, &efds, &tv);

    // Handle return value
    if (ret > 0)
    {
        if (FD_ISSET(sock, &efds))
        {
            return LwDiagUtils::NETWORK_ERROR;
        }
        else
        {
            return LwDiagUtils::OK;
        }
    }
    else if (ret == 0)
    {
        return LwDiagUtils::TIMEOUT_ERROR;
    }
    else
    {
        return LwDiagUtils::NETWORK_ERROR;
    }
}

LwDiagUtils::EC SocketOsx::Write(const char *buf, UINT32 len)
{
   // make sure the client socket has been connected
   if (!m_IsValid)
      return LwDiagUtils::NETWORK_NOT_CONNECTED;

   // return immediately if length is zero
   if (len == 0)
      return LwDiagUtils::OK;

   // This loop is a workaround for the MS Lanman blocking sockets bug.
   // When there is no buffer space available, the blocking socket is supposed
   // to block until there is buffer space, but actually it returns with
   // MSS_ERR_NOBUFS.
   ssize_t ret_len;
   do
   {
      ret_len = send(m_ClientSock,
                     const_cast<char*>(buf),
                     len,
                     0);
   } while ((ret_len == -1) && (errno == ENOBUFS));

   if (ret_len == -1)
      return LwDiagUtils::NETWORK_WRITE_ERROR;

   return LwDiagUtils::OK;
}

LwDiagUtils::EC SocketOsx::Read(char *buf, UINT32 len, UINT32 *pBytesRead)
{
   // make sure the client socket has been connected
   if (!m_IsValid)
      return LwDiagUtils::NETWORK_NOT_CONNECTED;

   ssize_t ret_len = recv(m_ClientSock, buf, len, 0);

   if (ret_len < 0)
      return LwDiagUtils::NETWORK_READ_ERROR;

   *pBytesRead = static_cast<UINT32>(ret_len);

   return LwDiagUtils::OK;
}

LwDiagUtils::EC SocketOsx::Flush(void)
{
   if (!m_IsValid)
      return LwDiagUtils::NETWORK_NOT_CONNECTED;

   // TODO

   return LwDiagUtils::OK;
}

LwDiagUtils::EC SocketOsx::Close(void)
{
   // shutdown library call not yet implemented in mssocket
   // shutdown (m_ClientSock, SD_BOTH);     // turn off send & receive
   if (m_ClientSock != ILWALID_SOCKET)
      shutdown (m_ClientSock, 0);

   if (m_ServerSock != ILWALID_SOCKET)
      shutdown (m_ServerSock, 0);           // no shutdown() req'd

   m_ClientSock = m_ServerSock = ILWALID_SOCKET;

   m_IsValid = false;

   return LwDiagUtils::OK;
}

LwDiagUtils::EC SocketOsx::GetHostIPs(vector<UINT32>* pIps)
{
    pIps->clear();

    // only for server socket
    if (m_ServerSock == ILWALID_SOCKET)
        return LwDiagUtils::NETWORK_CANNOT_DETERMINE_ADDRESS;

    // Obtain local machine addresses to which the socket is bound
    if (m_ServerSIN.sin_addr.s_addr)
    {
        pIps->push_back(ntohl(m_ServerSIN.sin_addr.s_addr));
    }
    else
    {
        struct ifaddrs* addrs = 0;
        if (0 == getifaddrs(&addrs))
        {
            struct ifaddrs* addr = addrs;
            for ( ; addr; addr = addr->ifa_next)
            {
                // MODS socket API supports only IPv4
                if (addr->ifa_addr->sa_family == AF_INET)
                {
                    // Skip localhost (interface name starting with "lo")
                    if ((addr->ifa_name == 0) || (addr->ifa_name[0] == 0) ||
                        ((addr->ifa_name[0] != 'l') && (addr->ifa_name[1] != 'o')))
                    {
                        struct sockaddr_in* sin =
                            reinterpret_cast<struct sockaddr_in*>(addr->ifa_addr);
                        pIps->push_back(ntohl(sin->sin_addr.s_addr));
                    }
                }
            }
            freeifaddrs(addrs);
        }
    }
    return pIps->empty() ? LwDiagUtils::NETWORK_CANNOT_DETERMINE_ADDRESS : LwDiagUtils::OK;
}

LwDiagUtils::EC SocketOsx::SetReadTimeout(FLOAT64 TimeoutMs)
{
   if (!m_IsValid)  return LwDiagUtils::NETWORK_NOT_CONNECTED;

   struct timeval waitTime;
   waitTime.tv_sec = (TimeoutMs / 1000);
   waitTime.tv_usec = (TimeoutMs - (FLOAT64)(waitTime.tv_sec * 1000)) * 1000;
   int ret = setsockopt(m_ClientSock, SOL_SOCKET, SO_RCVTIMEO,
                        (const void *)&waitTime, sizeof(struct timeval));
   return (ret != 0) ? LwDiagUtils::NETWORK_ERROR : LwDiagUtils::OK;
}

SocketOsx& SocketOsx::operator = (const SocketOsx& o)
{
   // check if doing a self-referential assignment
   if (this != &o)
   {
      // not assigning to self, do the assignment

      // do not need to destruct current instance
      // since destructor just decreases reference count,
      // which we would need to increase again anyway

      // now copy the passed smart pointer
      m_ServerSock = o.m_ServerSock;
      m_ClientSock = o.m_ClientSock;
      m_IsValid = o.m_IsValid;
      m_ServerSIN = o.m_ServerSIN;
      m_ClientSIN = o.m_ClientSIN;
   }

   return *this;
}

void SocketOsx::AddInstance()
{
   s_InstanceCount++;

   signal(SIGPIPE, SIG_IGN);

   s_SystemSocketInited = true;
}

void SocketOsx::RemoveInstance()
{
   s_InstanceCount--;

   if (s_InstanceCount == 0)
   {
      s_SystemSocketInited = false;
   }
}

