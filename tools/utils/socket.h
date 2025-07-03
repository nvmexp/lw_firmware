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
// Basic TCP socket abstration include file
//------------------------------------------------------------------------------

#ifndef INCLUDED_SOCKET_H
#define INCLUDED_SOCKET_H

#ifndef INCLUDED_LWDIAGUTILS_H
   #include "lwdiagutils.h"
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

class Socket
{
    public:
        virtual ~Socket() { /* empty */ };

        // Connect to a remote server
        virtual LwDiagUtils::EC Connect(UINT32 serverip, UINT16 serverport) = 0;

        // Do a DNS lookup
        virtual UINT32 DoDnsLookup(const char *ServerName) = 0;

        // Create socket for others to connect to
        virtual LwDiagUtils::EC ListenOn(INT32 port) = 0;
        virtual LwDiagUtils::EC Accept() = 0;

        enum SelectOn
        {
            ON_READ             = 1,
            ON_WRITE            = 2,
            ON_READ_AND_WRITE   = 3
        };

        // Wait for an event to occur on the socket
        virtual LwDiagUtils::EC Select(SelectOn selectOn, FLOAT64 timeoutMs) = 0;

        // R/W both return -1 on error, else # bytes transferred.
        // For write, len is the # to send to remote connection,
        // and for read, len is the max # to accept from remote connection.
        virtual LwDiagUtils::EC Write(const char *buf, UINT32 len) = 0;
        virtual LwDiagUtils::EC Read (char *buf, UINT32 len, UINT32 *pBytesRead) = 0;

        // Cleanup:
        virtual LwDiagUtils::EC Flush(void) = 0;
        virtual LwDiagUtils::EC Close(void) = 0;

        // Get local IP addresses
        virtual LwDiagUtils::EC GetHostIPs(vector<UINT32>* pIps) = 0;

        // Set timeout for read
        virtual LwDiagUtils::EC SetReadTimeout(FLOAT64 TimeoutMs) = 0;

        // static functions
        static string IpToString(UINT32 ip);
        static UINT32 ParseIp(const string& ipstr);
};

#endif
