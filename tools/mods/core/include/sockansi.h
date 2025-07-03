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
// ASCII socket with ANSI support include file
//------------------------------------------------------------------------------

#ifndef INCLUDED_SOCKANSI_H
#define INCLUDED_SOCKANSI_H

#ifndef INCLUDED_SOCKMODS_H
   #include "sockmods.h"
#endif

class CommandHistory;

class SocketAnsi : public SocketMods
{
   public:
      enum Mode { Plain, Script, Macro };

   public:
      SocketAnsi(SocketMods& baseSocket);
      virtual ~SocketAnsi();

      // Socket class methods
      virtual RC Connect(UINT32 serverip, UINT16 serverport);

      virtual UINT32 DoDnsLookup(const char *ServerName);

      virtual RC ListenOn(INT32 port);
      virtual RC Accept();

      virtual RC Write(const char *buf, UINT32 len);
      virtual RC Read (char *buf, UINT32 len, UINT32 *pBytesRead);

      virtual RC Flush(void);
      virtual RC Close(void);

      virtual RC GetHostIPs(vector<UINT32>* pIps);

      // Ansi socket methods

      Mode GetMode();
      void SetMode(Mode m);
      void ResetCommandBuffer();
      RC Read (string& line);

   private:
      // for now, do not allow copy constructor and assignment operator
      // since they can result in inconsistencies in the command buffer
      SocketAnsi(const SocketAnsi&);
      SocketAnsi& operator = (const SocketAnsi&);

   private:

      // private helper methods

      RC RedrawCommand(const char *buf, int oldlen, int len, int oldpos, int pos);
      RC RedrawPrompt(int pos);

      // private data members

      SocketMods* m_pBaseSocket;        // underlying socket

      CommandHistory *m_CommandHistory;

      Mode m_mode;                      // command mode select
      const char* m_prompt;             // command prompt
      const char* m_prefix;             // command prefix
      bool m_cmdBufEn;                  // command buffer enable

      char m_lastC;                     // last received character

      // private constants

      static const char RIGHT[];
      static const char LEFT[];
};

#endif

