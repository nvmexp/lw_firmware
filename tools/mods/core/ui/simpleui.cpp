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

#include "core/include/simpleui.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "core/include/cmdhstry.h"
#include "core/include/cmdline.h"
#include "core/include/tee.h"
#include "core/include/xp.h"
#include "core/include/massert.h"
#include "runmacro.h"
#include "core/include/sersink.h"
#include "core/include/cnslmgr.h"
#include "core/include/console.h"
#include "core/include/stdoutsink.h"
#include "core/include/utility.h"
#include "core/include/tabcompl.h"
#include <ctype.h>
#include <map>
#include <memory>

// TO DO: These should not be global!
map<string, string> gUIMacros;
bool gExit = false;

static bool IsScreenPause = false;
static bool IsBlinkScroll = false;
static bool s_ToggleUI = false;

// Use '>' as the prompt character for script UI and ':' for macro UI.
const char ScriptPrompt[] = ">";
const char MacroPrompt[]  = ":";

// RAII class for disabling the standard out sink
class DisableStdoutSink
{
public:
    DisableStdoutSink();
    ~DisableStdoutSink();
private:
    bool m_WasEnabled;
};

DisableStdoutSink::DisableStdoutSink()
: m_WasEnabled(false)
{
    StdoutSink *pStdoutSink = Tee::GetStdoutSink();
    if (pStdoutSink && pStdoutSink->IsEnabled())
    {
        m_WasEnabled = true;
        pStdoutSink->Disable();
    }
}
DisableStdoutSink::~DisableStdoutSink()
{
    StdoutSink *pStdoutSink = Tee::GetStdoutSink();
    if (pStdoutSink && m_WasEnabled)
    {
        pStdoutSink->Enable();
    }
}

/**
 *------------------------------------------------------------------------------
 * @function(SimpleUserInterface::SimpleUserInterface)
 *
 * ctor
 *------------------------------------------------------------------------------
 */

SimpleUserInterface::SimpleUserInterface
(
    bool StartInScriptMode
) :
   m_IsInsertMode(true),
   m_RecordUserInput(CommandLine::RecordUserInput()),
   m_IsScriptUserInterface(StartInScriptMode),
   m_pHistory(0),
   m_CommandsBack(0)
{
    ConsoleManager::ConsoleContext consoleCtx;

    consoleCtx.AcquireRealConsole(false)->SetLwrsor(Console::LWRSOR_NORMAL);

    m_pHistory = new CommandHistory;
    MASSERT(m_pHistory != 0);
}

/**
 *------------------------------------------------------------------------------
 * @function(SimpleUserInterface::~SimpleUserInterface)
 *
 * dtor
 *------------------------------------------------------------------------------
 */

SimpleUserInterface::~SimpleUserInterface()
{
    ConsoleManager::ConsoleContext consoleCtx;
    consoleCtx.AcquireRealConsole(false)->SetLwrsor(Console::LWRSOR_NORMAL);

    delete m_pHistory;
}

/**
 *------------------------------------------------------------------------------
 * @function(SimpleUserInterface::Run)
 *
 * Run the user interface.
 *------------------------------------------------------------------------------
 */

void SimpleUserInterface::Run(bool AllowYield /*= true*/)
{
   JavaScriptPtr pJavaScript;
   string        Input;
   ConsoleManager::ConsoleContext consoleCtx;
   Console *pConsole;
   bool     bPrintedStdoutMessage = false;
   unique_ptr<DisableStdoutSink> stdoutDisable;

   // Cursor position is zero based, i.e. upper-left equals (0,0).
   INT32 Column;

   while (!gExit)
   {
       if(s_ToggleUI)
       {
           m_IsScriptUserInterface = !m_IsScriptUserInterface;
           s_ToggleUI = false;
       }

       if (AllowYield)
       {
           pConsole = consoleCtx.AcquireRealConsole(true);
       }
       else
       {
           pConsole = consoleCtx.AcquireConsole(true);
       }

       if ((pConsole == ConsoleManager::Instance()->GetNullConsole()) ||
           (!pConsole->IsEnabled() && (!AllowYield || (OK != pConsole->PopUp()))))
       {
           Printf(Tee::FileOnly,
                  "\n***************************************************\n"
                  "* Current console is not suitable for interaction *\n"
                  "* Exiting SimpleUserInterface!!!!                 *\n"
                  "***************************************************\n");
           return;
       }
       pConsole->AllowYield(AllowYield);

       // The StdoutSink is used to duplicate the output (so that the output
       // goes both to the MODS console and to stdout), however because stdout
       // doesn't have the same formatting capabilites that the MODS console
       // does, attempting to duplicate interactive input lines to the
       // StdoutSink (which prints at screen level) results in garbage.
       //
       // Print a message indicating what must be done to work around this
       // limitation and disable the StdoutSink.
       //
       // TODO : Enhance StdoutSink in some way so that the input can be
       // duplicated.
       if (!bPrintedStdoutMessage && Tee::GetStdoutSink() &&
           (ConsoleManager::Instance()->GetModsConsole() == pConsole))
       {
           static const char header[] = "\n"
               "**************************************************************************\n"
               "* User input is displayed on the monitor connected to the GPU            *\n"
               "* Add -disable_mods_console to the command line if the monitor is remote *\n"
               "**************************************************************************\n\n";
           Tee::GetStdoutSink()->Print(header, sizeof(header) - 1);
           bPrintedStdoutMessage = true;
       }
       stdoutDisable.reset(new DisableStdoutSink);

       Tee::TeeFlushQueuedPrints(Tee::DefPrintfQFlushMs);

       // Always place the prompt at the begining of a new line.
       pConsole->GetLwrsorPosition(0, &Column);
       if (0 < Column)
           pConsole->PrintStringToScreen("\n", 1, Tee::SPS_NORMAL);

       if (m_IsScriptUserInterface)
       {
           pConsole->PrintStringToScreen(ScriptPrompt, sizeof(ScriptPrompt) - 1, Tee::SPS_NORMAL);
       }
       else
       {
           pConsole->PrintStringToScreen(MacroPrompt, sizeof(MacroPrompt) - 1, Tee::SPS_NORMAL);
       }

       Input = GetInput(pConsole, AllowYield, this);

       stdoutDisable.reset();

       consoleCtx.ReleaseConsole();

       // Enable or disable screen pause.
       Tee::SetScreenPause(IsScreenPause);

       Tee::ResetScreenPause();

       Tee::SetBlinkScroll(IsBlinkScroll);

       if (m_IsScriptUserInterface)
       {
           RC rc = pJavaScript->RunScript(Input);

           // Javascript will only report uncaught exceptions at the top level
           // of the stack frame (which incidentally calls CallHook).  When
           // running under "AllowYield" this will not be the case so in order
           // to correctly report errors in the UI under ignore yield, process
           // them here
           if (!AllowYield && (rc == RC::SCRIPT_FAILED_TO_EXELWTE))
           {
               JSContext *pContext;
               if (OK == pJavaScript->GetContext(&pContext))
               {
                   RC exnRc = pJavaScript->GetRcException(pContext);

                   // Colwert any exception to an RC, OK will be returned if
                   // either there was no exception or it couldnt colwert the
                   // exception to an RC.  In that case report and clear the
                   // exception through the JS mechanism, otherwise report the
                   // failure directly here
                   rc.Clear();
                   if (exnRc != OK)
                       rc = exnRc;
                   else
                       pJavaScript->ReportAndClearException(pContext);
               }

               if (rc != OK)
               {
                   const string errStr = Utility::StrPrintf("%s : %s (%d)\n",
                                                            Input.c_str(),
                                                            rc.Message(),
                                                            static_cast<INT32>(rc));
                   pConsole->PrintStringToScreen(errStr.data(), errStr.size(), Tee::SPS_NORMAL);
               }
           }
       }
       else
           RunMacro(Input);
   }
   gExit = false;

   if (AllowYield)
   {
       pConsole = consoleCtx.AcquireRealConsole(true);
   }
   else
   {
       pConsole = consoleCtx.AcquireConsole(true);
   }
   if ((pConsole != ConsoleManager::Instance()->GetNullConsole()) &&
       pConsole->IsPoppedUp())
   {
       pConsole->PopDown();
   }
}

//-----------------------------------------------------------------------------
//! \brief Get a line of user input.
//!
//! \return string that the user typed
/* static */ string SimpleUserInterface::GetLine()
{
    ConsoleManager::ConsoleContext consoleCtx;
    Console *pConsole;

    pConsole = consoleCtx.AcquireConsole(true);

    string Input = GetInput(pConsole, true, NULL);

    consoleCtx.ReleaseConsole();

    return Input;
}

//-----------------------------------------------------------------------------
//! \brief Get input from the user.
//!
//! \param pConsole   : Console to use to get the input
//! \param AllowYield : Whether or not to yield when getting the input
//! \param pUI        : User interface associated with the input.  If null then
//!                     UI related features (command history) and shortlwts
//!                     (exit MODS, console scrolling, etc.) are disabled.
//!
//! \return string that the user typed
/* private */
/* static */ string SimpleUserInterface::GetInput
(
    Console *            pConsole,
    bool                 AllowYield,
    SimpleUserInterface *pUI
)
{
   // Cursor position is zero based, i.e. upper-left equals (0,0).
   INT32 Column;
   INT32 Row;
   INT32 StartColumn;
   bool  bIsInsertMode = pUI ? pUI->m_IsInsertMode : true;
   INT32 TabsPending = 0;

   string            Input;
   string::size_type Start = 0;
   string::size_type Put   = 0;

   INT32 Columns = pConsole->GetScreenColumns();

   // Get the prompt position
   pConsole->GetLwrsorPosition(&Row, &StartColumn);

   // Set the initial column
   Column = StartColumn;

   // Resetup the cursor since the UI maintains once cursor state and non-UI
   // calls have a transient state
   if (bIsInsertMode)
      pConsole->SetLwrsor(Console::LWRSOR_NORMAL);
   else
      pConsole->SetLwrsor(Console::LWRSOR_BLOCK);

   // Non-formatting console input does not need to be called in the while loop
   // as it only supports getting a generic string
   if (!pConsole->Formatting())
   {
       if (pConsole->GetString(&Input) != OK)
       {
           Input = pUI ? "" : "Exit()";
       }
       return Input;
   }

   // Loop ends when user presses the return key or Alt-F4 (exit program).
   while (1)
   {
      if (!AllowYield)
      {
         // CPU busy loop, never yields to another thread.
         // Used only by the LwRm.InteractOnBreakPoint mode, called from inside
         // an ISR.
         while (!pConsole->KeyboardHit())
            /*empty*/;
      }

      Console::VirtualKey Key = pConsole->GetKey();

      if (pUI && (Console::VKC_TAB == Key)) // Key handling
      {
         ++TabsPending;
         vector<string> Options;
         string Completion = TabCompletion::Complete(
                                            Input.substr(0, Put), &Options);
         Input.insert(Put, Completion);
         Put += Completion.size();
         Column += static_cast<INT32>(Completion.size());

         if (Column >= Columns)
         {
            Start += Column - (Columns - 1);
            Column = Columns - 1;
         }

         pConsole->ScrollScreen(0);

         if ((TabsPending > 1) && (Options.size() > 1))
         {
            vector<string> HelpMsg;
            TabCompletion::Format(Options, Columns - 1, &HelpMsg);

            pConsole->SetLwrsorPosition(Row, StartColumn);
            pConsole->PrintStringToScreen(&Input[Start], Columns - StartColumn - 1, Tee::SPS_NORMAL);
            pConsole->PrintStringToScreen("\n", 1, Tee::SPS_NORMAL);

            if ((HelpMsg.size() > pConsole->GetScreenRows() - 2)
                    && (TabsPending == 2))
            {
               const string tooMany = Utility::StrPrintf(
                             "Too many options (%u), <Tab> to force printing\n",
                             (UINT32) Options.size());
               pConsole->PrintStringToScreen(tooMany.data(), tooMany.size(), Tee::SPS_NORMAL);
            }
            else
            {
               for (auto ipLine : HelpMsg)
               {
                   pConsole->PrintStringToScreen(ipLine.data(), ipLine.size(), Tee::SPS_NORMAL);
                   pConsole->PrintStringToScreen("\n", 1, Tee::SPS_NORMAL);
               }
               TabsPending = 0;
            }

            pConsole->PrintStringToScreen(
               pUI->m_IsScriptUserInterface ? ScriptPrompt : MacroPrompt,
               pUI->m_IsScriptUserInterface ? sizeof(ScriptPrompt) - 1 : sizeof(MacroPrompt) - 1,
               Tee::SPS_NORMAL
            );

            pConsole->GetLwrsorPosition(&Row, NULL);

         }
      }

      else if ((Key < 128) && isprint(Key)) // Check if the key is printable.
      {
         TabsPending = 0;
         pConsole->ScrollScreen(0);
         if (bIsInsertMode)
            Input.insert(Put++, 1, Key);
         else
            Input.replace(Put++, 1, 1, Key);

         if (Column < (Columns - 1))
            ++Column;
         else
            ++Start;
      }
      else
      {
         TabsPending = 0;
         switch (Key)
         {
             case Console::VKC_CONTROL_D:
                 if (pUI)
                 {
                    return "Exit()";
                 }
                 break;

            // Return input string.
            case Console::VKC_LINEFEED:
            case Console::VKC_RETURN:
            {
               pConsole->ScrollScreen(0);
               if (pUI && pUI->m_RecordUserInput)
               {
                  Printf(Tee::FileOnly, "%s\n",Input.c_str());
               }
               Printf(Tee::EthernetOnly, "%s\n",Input.c_str());

               if (pUI)
               {
                   pUI->m_CommandsBack = 0;
                   pUI->m_pHistory->AddCommand(Input);
               }

               if (pConsole->Echo())
               {
                   pConsole->SetLwrsorPosition(Row, StartColumn);
                   pConsole->PrintStringToScreen(Input.data(), Input.size(), Tee::SPS_NORMAL);
                   static const char nr[] = "\n\r";
                   pConsole->PrintStringToScreen(nr, sizeof(nr) - 1, Tee::SPS_NORMAL);
               }

               return Input;
            }

            // Retrieve previous command in history.
            case Console::VKC_UP:
            case Console::VKC_EUP:
               if (pUI)
               {
                  pConsole->ScrollScreen(0);
                  Input = pUI->m_pHistory->GetCommand(++pUI->m_CommandsBack);

                  // Move to the end.
                  Put = Input.length();
                  if (static_cast<UINT32>(Columns - StartColumn - 1) < Put)
                  {
                     Start  = Put - Columns + StartColumn + 1;
                     Column = Columns - 1;
                  }
                  else
                  {
                     Start  = 0;
                     Column = (INT32) Put + StartColumn;
                  }
               }
               break;

            case Console::VKC_CONTROL_DOWN:
            case Console::VKC_CONTROL_EDOWN:
                if (pUI)
                    pConsole->ScrollScreen(1);
                break;
            case Console::VKC_PAGEDOWN:
            case Console::VKC_EPAGEDOWN:
                if (pUI)
                    pConsole->ScrollScreen(pConsole->GetScreenRows());
                break;
            case Console::VKC_CONTROL_UP:
            case Console::VKC_CONTROL_EUP:
                if (pUI)
                    pConsole->ScrollScreen(-1);
                break;
            case Console::VKC_PAGEUP:
            case Console::VKC_EPAGEUP:
                if (pUI)
                    pConsole->ScrollScreen(-(INT32)pConsole->GetScreenRows());
                break;

            // Retrieve next command in history.
            case Console::VKC_DOWN:
            case Console::VKC_EDOWN:
                if (pUI)
                {
                   pConsole->ScrollScreen(0);
                   if (0 < pUI->m_CommandsBack)
                   {
                      Input = pUI->m_pHistory->GetCommand(--pUI->m_CommandsBack);

                      // Move to the end.
                      Put = Input.length();
                      if (static_cast<UINT32>(Columns - StartColumn -1) < Put)
                      {
                         Start  = Put - Columns + StartColumn + 1;
                         Column = Columns - 1;
                      }
                      else
                      {
                         Start  = 0;
                         Column = (INT32) Put + StartColumn;
                      }
                   }
                }
                break;

            // Move cursor left.
            case Console::VKC_LEFT:
            case Console::VKC_ELEFT:
            {
               pConsole->ScrollScreen(0);
               if (0 < Put)
               {
                  if (StartColumn < Column)
                     --Column;
                  else
                     --Start;

                  --Put;
               }
               else
               {
                  Start  = 0;
                  Put    = 0;
                  Column = StartColumn;
               }
               break;
            }

            // Move cursor right.
            case Console::VKC_RIGHT:
            case Console::VKC_ERIGHT:
            {
               pConsole->ScrollScreen(0);
               if (Put < Input.length())
               {
                  if (Column < (Columns - 1))
                     ++Column;
                  else
                     ++Start;

                  ++Put;
               }
               break;
            }

            // Delete previous character.
            case Console::VKC_BACKSPACE:
            {
               pConsole->ScrollScreen(0);
               if (0 < Put)
               {
                  Input.erase(--Put, 1);
                  if (StartColumn < Column)
                     --Column;
                  else
                     --Start;
               }
               else
               {
                  Start  = 0;
                  Put    = 0;
                  Column = StartColumn;
               }
               break;
            }

            // Toggle insert/overwrite mode.
            case Console::VKC_INSERT:
            case Console::VKC_EINSERT:
            {
                if (pUI)
                {
                    pUI->m_IsInsertMode = pUI->m_IsInsertMode ? false : true;
                    bIsInsertMode = pUI->m_IsInsertMode;
                }
               break;
            }

            // Move to beginning of input line.
            case Console::VKC_HOME:
            case Console::VKC_EHOME:
            {
               pConsole->ScrollScreen(0);
               Start  = 0;
               Put    = 0;
               Column = StartColumn;
               break;
            }

            // Delete current character.
            case Console::VKC_DELETE:
            case Console::VKC_EDELETE:
            {
               pConsole->ScrollScreen(0);
               if (Put < Input.length())
                  Input.erase(Put, 1);
               break;
            }

            // Delete all characters from Put to end.
            case Console::VKC_CONTROL_DELETE:
            case Console::VKC_CONTROL_K:
            {
               pConsole->ScrollScreen(0);
               Input.erase(Put);
               break;
            }

            // Delete all characters up to Put.
            case Console::VKC_CONTROL_U:
            {
               pConsole->ScrollScreen(0);
               Input.erase(0, Put);
               Put = 0;
               Column = StartColumn;
               Start = 0;
               break;
            }

            // Move to end of input line.
            case Console::VKC_END:
            case Console::VKC_EEND:
            {
               pConsole->ScrollScreen(0);
               Put = Input.length();
               if (static_cast<UINT32>(Columns - StartColumn - 1) < Put)
               {
                  Start  = Put - Columns + StartColumn + 1;
                  Column = Columns - 1;
               }
               else
               {
                  Start  = 0;
                  Column = (INT32) Put + StartColumn;
               }
               break;
            }

            // Delete all characters of input line.
            case Console::VKC_ESCAPE:
            {
               pConsole->ScrollScreen(0);
               Input.erase();
               Start  = 0;
               Put    = 0;
               Column = StartColumn;
               break;
            }

            // Switch user interface mode.
            case Console::VKC_F5:
                if (pUI)
                {
                   INT32 LwrColumn;
                   pConsole->GetLwrsorPosition(&Row, &LwrColumn);
                   pConsole->SetLwrsorPosition(Row, 0);
                   if (pUI->m_IsScriptUserInterface)
                   {
                      pUI->m_IsScriptUserInterface = false;
                      pConsole->PrintStringToScreen(MacroPrompt, sizeof(MacroPrompt) - 1,
                                                    Tee::SPS_NORMAL);
                   }
                   else
                   {
                      pUI->m_IsScriptUserInterface = true;
                      pConsole->PrintStringToScreen(ScriptPrompt, sizeof(ScriptPrompt) - 1,
                                                    Tee::SPS_NORMAL);
                   }
                   pConsole->SetLwrsorPosition(Row, LwrColumn);
                }
                break;

            // Switch screen pause mode.
            case Console::VKC_F6:
                if (pUI)
                {
                   static const char screenPauseOff[] = "\nScreen Pause OFF\n";
                   static const char screenPauseOn[]  = "\nScreen Pause ON\n";
                   pConsole->PrintStringToScreen(
                           IsScreenPause ? screenPauseOff : screenPauseOn,
                           IsScreenPause ? sizeof(screenPauseOff) - 1 : sizeof(screenPauseOn) - 1,
                           Tee::SPS_NORMAL);
                   IsScreenPause = !IsScreenPause;
                }
                break;

            // Switch blink scroll mode.
            case Console::VKC_F7:
                if (pUI)
                {
                   static const char blinkScrollOff[] = "\nBlink Scroll OFF\n";
                   static const char blinkScrollOn[]  = "\nBlink Scroll ON\n";
                   pConsole->PrintStringToScreen(
                           IsBlinkScroll ? blinkScrollOff : blinkScrollOn,
                           IsBlinkScroll ? sizeof(blinkScrollOff) - 1 : sizeof(blinkScrollOn) - 1,
                           Tee::SPS_NORMAL);
                   IsBlinkScroll = !IsBlinkScroll;
                }
                break;

            case Console::VKC_F8:
                if (pUI)
                {
                   SerialSink * pSerialSink = Tee::GetSerialSink();
                   switch(pSerialSink->GetBaud())
                   {
                      case 115200:
                         pSerialSink->Initialize();
                         pSerialSink->SetBaud(9600);
                         break;

                      case 9600:
                         pSerialSink->Uninitialize();
                         {
                             static const char serialOutputOff[] = "Serial output disabled\n";
                             pConsole->PrintStringToScreen(serialOutputOff,
                                                           sizeof(serialOutputOff) - 1,
                                                           Tee::SPS_NORMAL);
                         }
                         break;

                      case 0:
                      default:
                         pSerialSink->Initialize();
                         pSerialSink->SetBaud(115200);
                         break;

                   }
                }
                break;

            // Switch to/from monochrome monitor.
            case Console::VKC_F9:
                if (pUI)
                {
                   Xp::SetVesaMode(3);

                   Columns = pConsole->GetScreenColumns();
                }
                break;

            // Exit the program.
            case Console::VKC_ALT_F4:
                if (pUI)
                {
                   pConsole->PrintStringToScreen("\n", 1, Tee::SPS_NORMAL);
                   gExit = true;
                   return "";
                }
                break;

#ifdef DEBUG
            // Issue a breakpoint (break in to the debugger).
            case Console::VKC_ALT_F10:
            case Console::VKC_ALT_F1:
                if (pUI)
                {
                   Xp::BreakPoint();
                }
                break;
#endif
            // Unknown character --- ring the bell.
            default:
            {
               pConsole->PrintStringToScreen("\a", 1, Tee::SPS_NORMAL);
               break;
            }

         } // switch on the key

      } // end of key handling

      if (pConsole->Echo())
      {
          string screenStr;
          // Output the characters to the screen.
          pConsole->SetLwrsor(Console::LWRSOR_NONE);

          Columns = pConsole->GetScreenColumns();

          pConsole->SetLwrsorPosition(Row, StartColumn);

          pConsole->PrintStringToScreen(&Input[Start], Columns - StartColumn - 1, Tee::SPS_NORMAL);

          // When clearing the line, avoid writing the last character on
          // the line because certain consoles have an implicit line feed
          // when the last character is written
          const INT32 numSpaces = (INT32)(Start + Columns - Input.length() - StartColumn - 1);
          if (numSpaces > 0)
          {
              screenStr = Utility::StrPrintf("%*c", numSpaces, ' ');
              pConsole->PrintStringToScreen(screenStr.data(), screenStr.size(), Tee::SPS_NORMAL);
          }
          pConsole->SetLwrsorPosition(Row, StartColumn);
          pConsole->PrintStringToScreen(&Input[Start], Column - StartColumn, Tee::SPS_NORMAL);

          if (bIsInsertMode)
             pConsole->SetLwrsor(Console::LWRSOR_NORMAL);
          else
             pConsole->SetLwrsor(Console::LWRSOR_BLOCK);
      }
   } // input loop

   return "";   // This statement keeps the compiler happy, never gets exelwted

} // GetInput()

//------------------------------------------------------------------------------
// JavaScript properties and methods.
//------------------------------------------------------------------------------

P_(Global_Set_Macros);
static SProperty Global_Macros
(
   "Macros",
   0,
   0,
   0,
   Global_Set_Macros,
   0,
   "Macro user interface commands."
);

P_(Global_Get_ScreenPause);
P_(Global_Set_ScreenPause);
static SProperty Global_ScreenPause
(
   "ScreenPause",
   0,
   false,
   Global_Get_ScreenPause,
   Global_Set_ScreenPause,
   0,
   "Pause screen after each page."
);

P_(Global_Get_BlinkScroll);
P_(Global_Set_BlinkScroll);
static SProperty Global_BlinkScroll
(
   "BlinkScroll",
   0,
   false,
   Global_Get_BlinkScroll,
   Global_Set_BlinkScroll,
   0,
   "Fast, ugly text scrolling (i.e. for slow MGA displays)."
);

C_(Global_Exit);
static STest Global_Exit
(
   "Exit",
   C_Global_Exit,
   0,
   "Exit MODS."
);

C_(Global_ToggleMacroMode);
static SMethod Global_ToggleMacroMode
(
    "ToggleMacroMode",
    C_Global_ToggleMacroMode,
    0,
    "Toggle between macro mode and script mode"
);

P_(Global_Set_Macros)
{
   //---------------------------------------------------------------------------
   // Macros =
   // [
   //    [ "rr",   "x"  ],
   //    [ "rw",   "xx" ],
   //    [ "fr",   "x"  ],
   //    [ "fw",   "xx" ],
   //    [ "clks", ""   ],
   //    [ "gclk", "f"  ],
   //    [ "mclk", "f"  ],
   //    [ "pclk", "f"  ],
   // ];
   //---------------------------------------------------------------------------

   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   JavaScriptPtr pJavaScript;

   // Get the pValue's elements (I = 0..M).
   // pValue is an M x N(=2) array of arrays.
   JsArray I;
   if (OK != pJavaScript->FromJsval(*pValue, &I))
   {
      JS_ReportError(pContext, "Failed to set Macros.");
      return JS_FALSE;
   }

   // Clear out the current Macros.
   gUIMacros.clear();

   // Store the array elements in gUIMacros.
   UINT32 M = (UINT32) I.size();
   for (UINT32 i = 0; i < M; ++i)
   {
      // Each element in the I array is another array (J),
      // whose elements represent the Macro components.
      JsArray J;
      string  Command;
      string  Format;
      if
      (
            (OK != pJavaScript->FromJsval(I[i], &J))
         || (J.size() != 2)
         || (OK != pJavaScript->FromJsval(J[0], &Command))
         || (OK != pJavaScript->FromJsval(J[1], &Format))
      )
      {
         JS_ReportError(pContext, "Failed to set Macros.");
         return JS_FALSE;
      }

      // Print a warning message if the Command is already defined.
      if (gUIMacros.find(Command) != gUIMacros.end())
      {
         Printf(Tee::PriAlways, "Warning: %s already defined.\n",
                Command.c_str() );
      }

      gUIMacros[Command] = Format;
   }

   return JS_TRUE;
}

P_(Global_Get_ScreenPause)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   if (OK != JavaScriptPtr()->ToJsval(IsScreenPause, pValue))
   {
      JS_ReportError(pContext, "Failed to get ScreenPause.");
      *pValue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

P_(Global_Set_ScreenPause)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   // Colwert the argument.
   UINT32 State = false;
   if (OK != JavaScriptPtr()->FromJsval(*pValue, &State))
   {
      JS_ReportError(pContext, "Failed to set ScreenPause.");
      return JS_FALSE;
   }

   IsScreenPause = (State != 0);

   return JS_TRUE;
}

P_(Global_Get_BlinkScroll)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   if (OK != JavaScriptPtr()->ToJsval(IsBlinkScroll, pValue))
   {
      JS_ReportError(pContext, "Failed to get BlinkScroll.");
      *pValue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

P_(Global_Set_BlinkScroll)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   // Colwert the argument.
   UINT32 State = false;
   if (OK != JavaScriptPtr()->FromJsval(*pValue, &State))
   {
      JS_ReportError(pContext, "Failed to set BlinkScroll.");
      return JS_FALSE;
   }

   IsBlinkScroll = (State != 0);

   return JS_TRUE;
}

C_(Global_Exit)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   // Check this is a void method.
   if (NumArguments != 0)
   {
      JS_ReportError(pContext, "Usage: Exit()");
      return JS_FALSE;
   }

   gExit = true;

   RETURN_RC(OK);
}

C_(Global_ToggleMacroMode)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    s_ToggleUI = true;

    RETURN_RC(OK);
}

C_(Global_Readln);
static SMethod Global_Readln
(
   "Readln",
   C_Global_Readln,
   0,
   "Read a line from the input"
);

C_(Global_Readln)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   // Check this is a void method.
   if (NumArguments != 0)
   {
      JS_ReportError(pContext, "Usage: Readln()");
      return JS_FALSE;
   }

   if (CommandLine::UserConsole() != CommandLine::UC_Local)
   {
       JS_ReportError(pContext, "Readln() only supported on local consoles");
       return JS_FALSE;
   }

   // Yield before reading a line so that any pending input is flushed
   Tasker::Yield();

   string Input = SimpleUserInterface::GetLine();

   if (OK != JavaScriptPtr()->ToJsval(Input, pReturlwalue))
   {
      JS_ReportError(pContext, "Failed to readline");
      *pReturlwalue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}
