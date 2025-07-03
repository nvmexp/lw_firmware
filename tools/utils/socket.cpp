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
//------------------------------------------------------------------------------
// Network utility implementation
//------------------------------------------------------------------------------
// 45678901234567890123456789012345678901234567890123456789012345678901234567890

#include "socket.h"
#include <cstdio>
#include <stdlib.h>

// colwert IP address to string
string Socket::IpToString(UINT32 ip)
{
   char s[16];
   sprintf(s, "%d.%d.%d.%d",
           (ip >> 24) & 0xFF,
           (ip >> 16) & 0xFF,
           (ip >> 8) & 0xFF,
           ip & 0xFF);
   return string(s);
}

// colwert string to IP address
UINT32 Socket::ParseIp(const string& ipstr)
{
   string s = ipstr + ".";
   UINT32 ip = 0;

   for(int i=0; i<4; i++)
   {
      size_t pos = s.find_first_of('.');

      // make sure there are enough dots
      if (pos == string::npos)
         return 0;

      int byte = atoi(s.substr(0, pos).c_str());

      // check for invalid byte
      if (byte != (byte & 0xFF))
         return 0;

      ip = (ip << 8) | byte;

      s = s.substr(pos + 1);
   }

   return ip;
}

