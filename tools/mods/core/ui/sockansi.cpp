/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2011,2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
// ASCII socket with ANSI support implementation
//------------------------------------------------------------------------------

#include "core/include/massert.h"
#include "core/include/script.h"
#include "core/include/sockansi.h"
#include "core/include/cmdhstry.h"
#include <algorithm>
#include <cstring>
#include <cctype>

// SocketAnsi static constants
const char SocketAnsi::RIGHT[] = { 27, '[', 'C', 0 };
const char SocketAnsi::LEFT[]  = { 27, '[', 'D', 0 };

// local defines
#define WINDOW_WIDTH                      80

// TODO: Create a platform-specific network subsystem initialization
// object with reference counting.  If ref=0, the OS gets inited,
// and when ref decrements back to 0, the OS support is shutdown.
class SocketAnsiInit; //etc

SocketAnsi::SocketAnsi(SocketMods& baseSocket) :
   m_pBaseSocket(&baseSocket),
   m_lastC('\0')
{
   SetMode(Plain);
   m_CommandHistory = new CommandHistory();
}

SocketAnsi::~SocketAnsi()
{
   delete m_CommandHistory;
}

RC SocketAnsi::Connect(UINT32 serverip, UINT16 serverport)
{
   return m_pBaseSocket->Connect(serverip, serverport);
}

UINT32 SocketAnsi::DoDnsLookup(const char *ServerAddress)
{
   return 0;
}

RC SocketAnsi::ListenOn(INT32 port)
{
   // pass through to socket
   return m_pBaseSocket->ListenOn(port);
}

RC SocketAnsi::Accept()
{
   // pass through to socket
   return m_pBaseSocket->Accept();
}

RC SocketAnsi::Write(const char *buf, UINT32 len)
{
   const int SEND_BUF_SIZE = 256;
   RC rc;

   char sendBuf[SEND_BUF_SIZE];
   int sendLen = 0;

   // for all characters in buf
   for(unsigned int i=0; i<len; i++)
   {
      // change \r or \n to \r\n
      // for now, assume caller sends either \r or \n, but not both
      if ((buf[i] == '\r') || (buf[i] == '\n'))
      {
         sendBuf[sendLen++] = '\r';
         sendBuf[sendLen++] = '\n';
      }
      // only send printable characters to avoid display problems
      else if (isprint((unsigned char)buf[i]))
         sendBuf[sendLen++] = buf[i];

      // send if send buffer cannot accept more input
      if (sendLen >= SEND_BUF_SIZE - 1)
      {
         CHECK_RC(m_pBaseSocket->Write(sendBuf, sendLen));
         sendLen = 0;
      }
   }

   // send all remaining data in the send buffer
   if (sendLen > 0)
      CHECK_RC(m_pBaseSocket->Write(sendBuf, sendLen));

   return OK;
}

// read an entire command
RC SocketAnsi::Read (string& line)
{
   RC rc;
   enum _state_t { Reg, Esc, Esc2 };
   string buf;

   // start in regular state
   _state_t state = Reg;

   // length and position
   INT32 len = 0;
   int pos = 0;

   INT32 commandsBack = 0;

   // print prompt
   CHECK_RC(m_pBaseSocket->Write(m_prompt, (UINT32) strlen(m_prompt)));

   bool done = false;
   do
   {
      MASSERT(commandsBack >= 0);

      // remember old length and position values
      int lenOld = len;
      int posOld = pos;

      char c;
      UINT32 readLen;

      // read a character from the socket
      CHECK_RC(m_pBaseSocket->Read(&c, 1, &readLen));

      // terminate loop on EOF
      if (readLen == 0)
         break;

      // Handle telnet protocol commands
      // See RFC 854
      enum TelnetCommands
      {
          TELNET_FIRST = 240,
          // commands which we don't care about not listed
          WILL = 251,
          WONT,
          DO,
          DONT,
          TELNET_LAST
      };
      if ((UINT08)c >= TELNET_FIRST)
      {
          if (((UINT08)c >= WILL) && ((UINT08)c <= DONT))
          {
              char option;
              CHECK_RC(m_pBaseSocket->Read(&option, 1, &readLen));
          }
          continue;
      }

      // regular (non-escaped) state
      if (state == Reg)
      {
         // printable character
         if(isprint((unsigned char)c))
         {
            // appending
            if (pos == len)
            {
               buf.append(1,c);
               pos++;
               len++;
               CHECK_RC(m_pBaseSocket->Write(&c, 1));
            }
            // inserting
            else
            {
               buf.insert(pos++,1,c);
               len++;
               CHECK_RC(RedrawCommand(buf.c_str(), lenOld, len, posOld, pos));
            }
         }
         else switch(c)
         {
         case '\r':
            done = true;
            CHECK_RC(Write(&c, 1));
            break;

         case '\n':
            // make sure \r\n does not result in two commands
            if (m_lastC != '\r')
            {
               done = true;
               CHECK_RC(Write("\r", 1));
            }
            break;

         case 27:                    // ESC
            state = Esc;
            break;

         case 8:                     // Backspace
            if (pos > 0)
            {
               buf.erase(pos - 1, 1);
               pos--;
               len--;
               CHECK_RC(RedrawCommand(buf.c_str(), lenOld, len, posOld, pos));
            }

            break;

         case 9:                     // Tab - switch mode
            if (GetMode() != Plain)
            {
               SetMode((GetMode() == Macro) ? Script : Macro);
               CHECK_RC(RedrawPrompt(pos));
            }
            break;

         case 127:                   // Delete
            if ((len > 0) && (pos < len))
            {
               buf.erase(pos, 1);
               len--;
               CHECK_RC(RedrawCommand(buf.c_str(), lenOld, len, posOld, pos));
            }
            break;

         default:
            break;
         }
      }
      else if (state == Esc)
      {
         switch(c)
         {
         case '[':
            state = Esc2;            // ESC [
            break;

         default:
            state = Reg;
            break;
         }
      }
      else if (state == Esc2)
      {
         switch(c)
         {
         case 'A':                   // up
            if (m_cmdBufEn)
            {
               string oldCmd = m_CommandHistory->GetCommand(++commandsBack);

               if(oldCmd != "")
               {
                  // copy command
                  buf = oldCmd;

                  // update position and length, redraw
                  pos = len = (INT32) buf.length();
                  CHECK_RC(RedrawCommand(buf.c_str(), lenOld, len, posOld, pos));
               }
               else
               {
                  --commandsBack;
               }
            }
            state = Reg;
            break;

         case 'B':                   // down
            if (m_cmdBufEn)
            {
               //if we go from 1 to 0, we get set to empty string as desired
               if(commandsBack > 0)
               {
                  buf = m_CommandHistory->GetCommand(--commandsBack);

                  // update position and length, redraw
                  pos = len = (INT32) buf.length();
                  CHECK_RC(RedrawCommand(buf.c_str(), lenOld, len, posOld, pos));
               }
            }
            state = Reg;
            break;

         case 'C':                   // right
            if (pos < len)
            {
               CHECK_RC(m_pBaseSocket->Write(RIGHT, (INT32) strlen(RIGHT)));
               ++pos;
            }
            state = Reg;
            break;

         case 'D':                   // left
            if (pos > 0)
            {
               CHECK_RC(m_pBaseSocket->Write(LEFT, (INT32) strlen(LEFT)));
               --pos;
            }
            state = Reg;
            break;

         default:
            state = Reg;
            break;
         }
      }

      // remember last character
      m_lastC = c;
   } while (!done);

   // if any characters input, keep command in command history
   if (buf.length())
   {
      m_CommandHistory->AddCommand(buf);
   }

   line = m_prefix;
   line.append(buf);

   return OK;
}

// read an entire command
RC SocketAnsi::Read(char *buf, UINT32 bufLen, UINT32 *pBytesRead)
{
    RC rc;
    string line;
    CHECK_RC(Read(line));

    size_t len = min( (size_t) bufLen, line.length());
    memcpy(buf,line.c_str(),len);
    *pBytesRead = static_cast<UINT32>(len);

    return OK;
}

RC SocketAnsi::Flush(void)
{
   // pass through to socket
   return m_pBaseSocket->Flush();
}

RC SocketAnsi::Close(void)
{
   // pass through to socket
   return m_pBaseSocket->Close();
}

RC SocketAnsi::GetHostIPs(vector<UINT32>* pIps)
{
   // pass through to socket
   return m_pBaseSocket->GetHostIPs(pIps);
}

SocketAnsi::Mode SocketAnsi::GetMode()
{
   return m_mode;
}

void SocketAnsi::SetMode(Mode m)
{
   // set mode
   m_mode = m;

   // set prompt, prefix, and command buffer enable
   switch(m)
   {
   case Plain:
      m_prompt = "";
      m_prefix = "";
      m_cmdBufEn = false;
      break;

   case Script:
      m_prompt = ">";
      m_prefix = "S: ";
      m_cmdBufEn = true;
      break;

   case Macro:
      m_prompt = ":";
      m_prefix = "M: ";
      m_cmdBufEn = true;
      break;
   }
}

void SocketAnsi::ResetCommandBuffer()
{
   delete m_CommandHistory;
   m_CommandHistory = new CommandHistory();
}

RC SocketAnsi::RedrawCommand(const char *buf, int oldLen, int len, int oldPos, int pos)
{
   RC rc;
   string s;

   // Not the common case, hence the hack.  If the user is editing a string
   // near the size of the window, redraw the darn thing every time
   if(strlen(buf) >= WINDOW_WIDTH - 2)
   {
      for(int i = 0; i < WINDOW_WIDTH; ++i)
         s += string(LEFT);

      s += string("\n\nLwrrent command is: \n\n");
   }

   // go to leftmost position and print buffer
   for(int i=0; i<oldPos; i++)
      s += string(LEFT);
   CHECK_RC(m_pBaseSocket->Write(s.c_str(), (UINT32) s.length()));

   CHECK_RC(m_pBaseSocket->Write(buf, len));

   s = string();
   // if length is shorter than previously
   if (len < oldLen)
   {
      // overwrite with spaces
      for(int i = len; i < oldLen; i++)
         s += " ";
      // set cursor to new position
      for(int i = pos; i < oldLen; i++)
         s += string(LEFT);
   }
   // if length is longer than previously
   else
      // set cursor to new position
      for(int i = pos; i < len; i++)
         s += string(LEFT);

   CHECK_RC(m_pBaseSocket->Write(s.c_str(), (UINT32) s.length()));

   return OK;
}

RC SocketAnsi::RedrawPrompt(int pos)
{
   RC rc;
   string s;
   int promptLen = (int) strlen(m_prompt);
   int i;

   // go to prompt position
   for(i = 0; i < (pos + promptLen); i++)
      s += string(LEFT);

   // write prompt
   s += m_prompt;

   // go back to cursor position
   for(i = 0; i < pos; i++)
      s += string(RIGHT);

   CHECK_RC(m_pBaseSocket->Write(s.c_str(), (UINT32) s.length()));

   return OK;
}
