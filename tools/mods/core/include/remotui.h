/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_REMOTEUI_H
#define INCLUDED_REMOTEUI_H

#ifndef INCLUDED_SOCKMODS_H
#include "sockmods.h"
#endif

#include "types.h"
#include <string>

class RemoteUserInterface
{
   public:

      RemoteUserInterface(SocketMods *, bool useMessageInterface);
      ~RemoteUserInterface();

      void Run();

   private:

      SocketMods * m_Socket;
      bool     m_UseMessageInterface;

      // Do not allow copying.
      RemoteUserInterface(const RemoteUserInterface &);
      RemoteUserInterface & operator=(const RemoteUserInterface &);
};

#endif // ! REMOTEUI_H
