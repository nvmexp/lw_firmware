/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2010,2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
// Telnet utility implementation
//------------------------------------------------------------------------------

#include "core/include/script.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "core/include/telnutil.h"
#include <string.h>

// get command line arguments from remote user
static RC GetArguments(SocketAnsi& socket, string& args)
{
   RC rc;

   // send message
   const char* msg = "Welcome to MODS. Please enter the command line arguments:\n";
   CHECK_RC(socket.Write(msg, (UINT32) strlen(msg)));

   // read command line arguments
   CHECK_RC(socket.Read(args));

   return OK;
}

// start the MODS telnet server
RC TelnetUtil::StartTelnetServer(SocketAnsi& socket, string& args, UINT16 Port)
{
   RC rc;

   // RAII cleanup
   Utility::CleanupOnReturn<SocketAnsi,RC> CloseSocket(&socket, &SocketAnsi::Close);
   Utility::CleanupOnReturnArg<void,void,const char*>
       ErrMsg(&Utility::PrintMsgAlways, "Network error - remote access disabled.\n");

   // listen on port
   rc = socket.ListenOn(Port);
   if (RC::NETWORK_CANNOT_BIND == rc)
   {
      Printf(Tee::PriAlways, "Unable to bind port %u\n", Port);
#if defined(linux)
      if (Port < 1024)
      {
          Printf(Tee::PriAlways, "Must be root to bind low port\n");
      }
#endif
   }
   CHECK_RC(rc);

   // output message
   Printf(Tee::PriAlways, "MODS waiting for remote client.");

   // output IP address if possible
   vector<UINT32> ips;
   if (OK == socket.GetHostIPs(&ips))
   {
      Printf(Tee::PriAlways, " Connect to IP");
      for (vector<UINT32>::const_iterator it=ips.begin();
           it != ips.end(); ++it)
      {
          if (it != ips.begin())
          {
              Printf(Tee::PriAlways, " or");
          }
          Printf(Tee::PriAlways, " %s",
             Socket::IpToString(*it).c_str());
      }
   }

   Printf(Tee::PriAlways, "\n");

   // wait for client login
   CHECK_RC(socket.Accept());

   Printf(Tee::PriAlways, "Remote client logged in.\n");

   // get command line arguments
   CHECK_RC(GetArguments(socket, args));

   // set user interface mode to script
   socket.SetMode(SocketAnsi::Script);
   socket.ResetCommandBuffer();

   // all good, cancel cleanup
   CloseSocket.Cancel();
   ErrMsg.Cancel();
   return rc;
}

RC TelnetUtil::StartTelnetClient(SocketAnsi& socket, string& args, UINT32 ip, UINT16 port)
{
   RC rc;

   // RAII cleanup
   Utility::CleanupOnReturn<SocketAnsi,RC> CloseSocket(&socket, &SocketAnsi::Close);
   Utility::CleanupOnReturnArg<void,void,const char*>
       ErrMsg(&Utility::PrintMsgAlways,"Could not connect to server - remote access disabled.\n");

   Printf(Tee::PriAlways, "Connecting to server %s port %d.\n",
          Socket::IpToString(ip).c_str(), port);

   // connect to server
   CHECK_RC(socket.Connect(ip, port));

   Printf(Tee::PriAlways, "Connected.\n");

   // get command line arguments
   CHECK_RC(GetArguments(socket, args));

   // set user interface mode to script
   socket.SetMode(SocketAnsi::Script);

   // all good, cancel cleanup
   CloseSocket.Cancel();
   ErrMsg.Cancel();
   return rc;
}

