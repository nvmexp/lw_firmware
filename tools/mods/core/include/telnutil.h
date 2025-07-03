/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
// Telnet utility include file
//------------------------------------------------------------------------------
// 45678901234567890123456789012345678901234567890123456789012345678901234567890

#ifndef INCLUDED_TELNUTIL_H
#define INCLUDED_TELNUTIL_H

#ifndef INCLUDED_SOCKANSI_H
   #include "sockansi.h"
#endif
#ifndef INCLUDED_STL_STRING
   #include <string>
   #define INCLUDED_STL_STRING
#endif

namespace TelnetUtil
{
   // start the MODS telnet server
   RC StartTelnetServer(SocketAnsi& socket, string& args, UINT16 port = 23);

   // start the MODS telnet client
   RC StartTelnetClient(SocketAnsi& socket, string& args, UINT32 ip, UINT16 port);
}

#endif

