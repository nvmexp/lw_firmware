/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2011 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
// TCP socket for OSX include file
//------------------------------------------------------------------------------

#ifndef INCLUDED_SOCKDOS_H
#define INCLUDED_SOCKDOS_H

#ifndef INCLUDED_SOCKET_H
   #include "socket.h"
#endif
#ifndef INCLUDED_IN_H
   #include <netinet/in.h>
#endif

class SocketOsx : public Socket
{
   public:

      SocketOsx();
      SocketOsx(const SocketOsx&);
      virtual ~SocketOsx();

      virtual LwDiagUtils::EC Connect(UINT32 serverip, UINT16 serverport);

      virtual UINT32 DoDnsLookup(const char *ServerName);

      virtual LwDiagUtils::EC ListenOn(INT32 port);
      virtual LwDiagUtils::EC Accept();

      virtual LwDiagUtils::EC Select(SelectOn selectOn, FLOAT64 timeoutMs);

      virtual LwDiagUtils::EC Write(const char *buf, UINT32 len);
      virtual LwDiagUtils::EC Read (char *buf, UINT32 len, UINT32 *pBytesRead);

      virtual LwDiagUtils::EC Flush(void);
      virtual LwDiagUtils::EC Close(void);

      virtual LwDiagUtils::EC GetHostIPs(vector<UINT32>* pIps);

      virtual LwDiagUtils::EC SetReadTimeout(FLOAT64 TimeoutMs);

      SocketOsx& operator = (const SocketOsx&);

   protected:

      // increase socket count
      static void AddInstance();

      // decrease socket count
      static void RemoveInstance();

      // socket count
      static int s_InstanceCount;

      // is the system socket initialized?
      static bool s_SystemSocketInited;

   protected:

      // server socket handle
      int m_ServerSock;

      // client socket handle
      int m_ClientSock;

      // is client socket connected?
      bool m_IsValid;

      // record socket addresses for internet sockets
      sockaddr_in m_ServerSIN;
      sockaddr_in m_ClientSIN;
};

#endif

