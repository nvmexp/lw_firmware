
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
// MODS socket abstration include file (thin wrapper around utils/socket.h)
//------------------------------------------------------------------------------

#ifndef INCLUDED_SOCKMODS_H
#define INCLUDED_SOCKMODS_H

#ifndef INCLUDED_RC_H
   #include "rc.h"
#endif
#ifndef INCLUDED_SOCKET_H
   #include "socket.h"
#endif
#ifndef INCLUDED_STL_STRING
   #include <string>
   #define INCLUDED_STL_STRING
#endif
#ifndef INCLUDED_STL_VECTOR
   #include <vector>
   #define INCLUDED_STL_VECTOR
#endif

#define MODS_NET_PORT 1337
#define TELNET_NET_PORT 23

// Abstract class to describe/manipulate a socket network connection.

class SocketMods
{
    public:
        SocketMods() : m_pBaseSocket(NULL) {}
        SocketMods(Socket * pSocket);
        virtual ~SocketMods();

        // Connect to a remote server
        virtual RC Connect(UINT32 serverip, UINT16 serverport);

        // Do a DNS lookup
        virtual UINT32 DoDnsLookup(const char *ServerName);

        // Create socket for others to connect to
        virtual RC ListenOn(INT32 port);
        virtual RC Accept();

        // R/W both return -1 on error, else # bytes transferred.
        // For write, len is the # to send to remote connection,
        // and for read, len is the max # to accept from remote connection.
        virtual RC Write(const char *buf, UINT32 len);
        virtual RC Read (char *buf, UINT32 len, UINT32 *pBytesRead);

        // Cleanup:
        virtual RC Flush(void);
        virtual RC Close(void);

        // Get local IP addresses
        virtual RC GetHostIPs(vector<UINT32>* pIps);

        Socket *GetSocket() { return m_pBaseSocket; }

        // static functions
        static string IpToString(UINT32 ip);
        static UINT32 ParseIp(const string& ipstr);

    private:
        RC Wait(Socket::SelectOn onWhat);

        Socket* m_pBaseSocket;        // underlying socket
};

#endif
