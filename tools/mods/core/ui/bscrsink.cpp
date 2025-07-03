/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

// Batch screen sink.

#include "core/include/bscrsink.h"
#include "core/include/tee.h"
#include "core/include/massert.h"
#include "core/include/cnslmgr.h"
#include "core/include/console.h"

BatchScreenSink::BatchScreenSink() :
   m_LinesWritten(0)
{
}
/* virtual */ void BatchScreenSink::DoPrint
(
   const char*           str,
   size_t                strLen,
   Tee::ScreenPrintState state
)
{
   MASSERT(str != 0);

   ConsoleManager::ConsoleContext consoleCtx;
   Console *pConsole = consoleCtx.AcquireConsole(false);

   bool DoPause = Tee::IsScreenPause();
   bool DoBlink = Tee::IsBlinkScroll();

   if (Tee::IsResetScreenPause())
   {
      if (DoBlink)
      {
         pConsole->ClearTextScreen();
         pConsole->SetLwrsorPosition(0,0);
      }
      m_LinesWritten = 0;
   }

   if (DoPause || DoBlink)
   {
      // We are performing special handling of scrolling.

      INT32 nRows = pConsole->GetScreenRows();

      static const char pressAKey[] = "< PRESS A KEY TO CONTINUE >";

      if (DoBlink)
      {
         // Some platforms (DOS on a mono card) scroll very slowly.
         // It can be as much as 10x faster to clear the screen and start
         // drawing again from the top instead of scrolling.

         // Check to see if this write might cause a screen scroll.
         INT32 lwrRow;
         pConsole->GetLwrsorPosition(&lwrRow, 0);
         if (lwrRow == (nRows - 1))
         {
            // Scrolling is about to occur.
            if (DoPause)
            {
               pConsole->PrintStringToScreen(pressAKey, sizeof(pressAKey) - 1, Tee::SPS_NORMAL);
               pConsole = consoleCtx.AcquireConsole(true);
               pConsole->GetKey();
               pConsole = consoleCtx.AcquireConsole(false);
               pConsole->PrintStringToScreen("\n", 1, Tee::SPS_NORMAL);
            }
            pConsole->ClearTextScreen();
            pConsole->SetLwrsorPosition(0,0);
         }
      }
      else if (DoPause)
      {
         if (m_LinesWritten == (nRows-1))
         {
            pConsole->PrintStringToScreen(pressAKey, sizeof(pressAKey) - 1, Tee::SPS_NORMAL);
            pConsole = consoleCtx.AcquireConsole(true);
            pConsole->GetKey();
            pConsole = consoleCtx.AcquireConsole(false);
            pConsole->PrintStringToScreen("\n", 1, Tee::SPS_NORMAL);
            m_LinesWritten = 0;
         }

         // Count lines:
         const char* pstr = str;
         const char* end  = str + strLen;
         const char* pnl  = str;
         while (pstr < end)
         {
            if ('\n' == *pstr)
            {
               ++m_LinesWritten;

               if (80 <= (pstr - pnl))
                  ++m_LinesWritten;

               pnl = pstr;
            }
            ++pstr;
         }
         if (80 <= (pstr - pnl))
            ++m_LinesWritten;
      }
   }

   pConsole->PrintStringToScreen(str, strLen, state);
}

/* virtual */ bool BatchScreenSink::DoPrintBinary
(
    const UINT08* data,
    size_t        size
)
{
    MASSERT(size);

    const size_t wrote = fwrite(data, 1, size, stdout);

    return wrote == size;
}

